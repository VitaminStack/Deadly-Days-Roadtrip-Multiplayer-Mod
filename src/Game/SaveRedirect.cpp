#include "Game/SaveRedirect.h"

#include <windows.h>

#include <atomic>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <mutex>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/SavePaths.h"

namespace Game::SaveRedirect
{
namespace
{
std::vector<SavePaths::Root> Roots;
std::atomic<long long> Calls = 0;
thread_local bool Inside = false;

std::mutex NoteLock;
std::set<std::wstring> Noted;
constexpr size_t MaxNoted = 64;

std::string Narrow(const std::wstring& Text)
{
    if (Text.empty())
        return {};
    const int Size = WideCharToMultiByte(CP_UTF8, 0, Text.data(), static_cast<int>(Text.size()), nullptr, 0, nullptr, nullptr);
    std::string Out(Size > 0 ? Size : 0, '\0');
    if (Size > 0)
        WideCharToMultiByte(CP_UTF8, 0, Text.data(), static_cast<int>(Text.size()), Out.data(), Size, nullptr, nullptr);
    return Out;
}

// Jede Art und Datei einmal loggen, nur Dateinamen.
void Note(const char* Operation, const std::wstring& Path, bool Foreign)
{
    const auto Key = std::wstring(Foreign ? L"!" : L"") + std::wstring(Operation, Operation + strlen(Operation)) + L":" + SavePaths::FileName(Path);
    {
        std::lock_guard Guard(NoteLock);
        if (Noted.size() >= MaxNoted || !Noted.insert(Key).second)
            return;
    }
    if (Foreign)
        Core::Log::Warn("Spielstand: {} auf {} außerhalb der bekannten Ordner, nicht umgeleitet", Operation, Narrow(SavePaths::FileName(Path)));
    else
        Core::Log::Info("Spielstand: {} {} im Mod-Ordner", Operation, Narrow(SavePaths::FileName(Path)));
}

// Ein Pfadargument. Get() liefert den umgeleiteten oder den unveränderten Zeiger. Blocked: Pfad betrifft Spielstände,
// ließ sich aber nicht auflösen; dann lieber den Aufruf ablehnen als den normalen Spielstand anfassen.
class PathArg
{
public:
    PathArg(LPCWSTR Path, const char* Operation) : Original(Path)
    {
        if (Inside || !SavePaths::MayConcernSaves(Path))
            return;
        Inside = true;
        const DWORD Error = GetLastError();
        std::wstring Full(MAX_PATH, L'\0');
        DWORD Length = GetFullPathNameW(Path, static_cast<DWORD>(Full.size()), Full.data(), nullptr);
        if (Length >= Full.size())
        {
            Full.resize(Length + 1);
            Length = GetFullPathNameW(Path, static_cast<DWORD>(Full.size()), Full.data(), nullptr);
        }
        if (Length == 0 || Length >= Full.size())
        {
            Blocked = true;
            Core::Log::Warn("Spielstand: {} mit unlesbarem Pfad abgelehnt", Operation);
        }
        else
        {
            Full.resize(Length);
            if (auto Mapped = SavePaths::Redirect(Full, Roots))
            {
                Storage = std::move(*Mapped);
                Redirected = true;
                Calls.fetch_add(1, std::memory_order_relaxed);
                Note(Operation, Storage, false);
            }
            else if (SavePaths::LooksLikeForeignSave(Full))
            {
                Note(Operation, Full, true);
            }
        }
        Inside = false;
        SetLastError(Error);
    }

    LPCWSTR Get() const { return Redirected ? Storage.c_str() : Original; }
    bool IsBlocked() const { return Blocked; }

private:
    LPCWSTR Original;
    std::wstring Storage;
    bool Redirected = false;
    bool Blocked = false;
};

template <typename T> T Refuse(T Failure)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return Failure;
}

using CreateFileWFn = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using CreateFile2Fn = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, DWORD, LPCREATEFILE2_EXTENDED_PARAMETERS);
using PathBoolFn = BOOL(WINAPI*)(LPCWSTR);
using GetAttributesFn = DWORD(WINAPI*)(LPCWSTR);
using GetAttributesExFn = BOOL(WINAPI*)(LPCWSTR, GET_FILEEX_INFO_LEVELS, LPVOID);
using SetAttributesFn = BOOL(WINAPI*)(LPCWSTR, DWORD);
using FindFirstFn = HANDLE(WINAPI*)(LPCWSTR, LPWIN32_FIND_DATAW);
using FindFirstExFn = HANDLE(WINAPI*)(LPCWSTR, FINDEX_INFO_LEVELS, LPVOID, FINDEX_SEARCH_OPS, LPVOID, DWORD);
using CreateDirectoryFn = BOOL(WINAPI*)(LPCWSTR, LPSECURITY_ATTRIBUTES);
using MoveFileFn = BOOL(WINAPI*)(LPCWSTR, LPCWSTR);
using MoveFileExFn = BOOL(WINAPI*)(LPCWSTR, LPCWSTR, DWORD);
using MoveFileProgressFn = BOOL(WINAPI*)(LPCWSTR, LPCWSTR, LPPROGRESS_ROUTINE, LPVOID, DWORD);
using ReplaceFileFn = BOOL(WINAPI*)(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, LPVOID, LPVOID);
using CopyFileFn = BOOL(WINAPI*)(LPCWSTR, LPCWSTR, BOOL);
using CopyFileExFn = BOOL(WINAPI*)(LPCWSTR, LPCWSTR, LPPROGRESS_ROUTINE, LPVOID, LPBOOL, DWORD);
using CopyFile2Fn = HRESULT(WINAPI*)(PCWSTR, PCWSTR, COPYFILE2_EXTENDED_PARAMETERS*);

CreateFileWFn OriginalCreateFileW = nullptr;
CreateFile2Fn OriginalCreateFile2 = nullptr;
PathBoolFn OriginalDeleteFileW = nullptr;
PathBoolFn OriginalRemoveDirectoryW = nullptr;
GetAttributesFn OriginalGetFileAttributesW = nullptr;
GetAttributesExFn OriginalGetFileAttributesExW = nullptr;
SetAttributesFn OriginalSetFileAttributesW = nullptr;
FindFirstFn OriginalFindFirstFileW = nullptr;
FindFirstExFn OriginalFindFirstFileExW = nullptr;
CreateDirectoryFn OriginalCreateDirectoryW = nullptr;
MoveFileFn OriginalMoveFileW = nullptr;
MoveFileExFn OriginalMoveFileExW = nullptr;
MoveFileProgressFn OriginalMoveFileWithProgressW = nullptr;
ReplaceFileFn OriginalReplaceFileW = nullptr;
CopyFileFn OriginalCopyFileW = nullptr;
CopyFileExFn OriginalCopyFileExW = nullptr;
CopyFile2Fn OriginalCopyFile2 = nullptr;

HANDLE WINAPI DetourCreateFileW(LPCWSTR Name, DWORD Access, DWORD Share, LPSECURITY_ATTRIBUTES Security, DWORD Disposition, DWORD Flags, HANDLE Template)
{
    const PathArg Path(Name, (Access & (GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE)) ? "schreiben" : "lesen");
    if (Path.IsBlocked())
        return Refuse(INVALID_HANDLE_VALUE);
    return OriginalCreateFileW(Path.Get(), Access, Share, Security, Disposition, Flags, Template);
}

HANDLE WINAPI DetourCreateFile2(LPCWSTR Name, DWORD Access, DWORD Share, DWORD Disposition, LPCREATEFILE2_EXTENDED_PARAMETERS Parameters)
{
    const PathArg Path(Name, (Access & (GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE)) ? "schreiben" : "lesen");
    if (Path.IsBlocked())
        return Refuse(INVALID_HANDLE_VALUE);
    return OriginalCreateFile2(Path.Get(), Access, Share, Disposition, Parameters);
}

BOOL WINAPI DetourDeleteFileW(LPCWSTR Name)
{
    const PathArg Path(Name, "löschen");
    return Path.IsBlocked() ? Refuse(FALSE) : OriginalDeleteFileW(Path.Get());
}

BOOL WINAPI DetourRemoveDirectoryW(LPCWSTR Name)
{
    const PathArg Path(Name, "Ordner löschen");
    return Path.IsBlocked() ? Refuse(FALSE) : OriginalRemoveDirectoryW(Path.Get());
}

DWORD WINAPI DetourGetFileAttributesW(LPCWSTR Name)
{
    const PathArg Path(Name, "prüfen");
    return Path.IsBlocked() ? Refuse(INVALID_FILE_ATTRIBUTES) : OriginalGetFileAttributesW(Path.Get());
}

BOOL WINAPI DetourGetFileAttributesExW(LPCWSTR Name, GET_FILEEX_INFO_LEVELS Level, LPVOID Info)
{
    const PathArg Path(Name, "prüfen");
    return Path.IsBlocked() ? Refuse(FALSE) : OriginalGetFileAttributesExW(Path.Get(), Level, Info);
}

BOOL WINAPI DetourSetFileAttributesW(LPCWSTR Name, DWORD Attributes)
{
    const PathArg Path(Name, "Attribute setzen");
    return Path.IsBlocked() ? Refuse(FALSE) : OriginalSetFileAttributesW(Path.Get(), Attributes);
}

HANDLE WINAPI DetourFindFirstFileW(LPCWSTR Name, LPWIN32_FIND_DATAW Data)
{
    const PathArg Path(Name, "suchen");
    return Path.IsBlocked() ? Refuse(INVALID_HANDLE_VALUE) : OriginalFindFirstFileW(Path.Get(), Data);
}

HANDLE WINAPI DetourFindFirstFileExW(LPCWSTR Name, FINDEX_INFO_LEVELS Level, LPVOID Data, FINDEX_SEARCH_OPS Search, LPVOID Filter, DWORD Flags)
{
    const PathArg Path(Name, "suchen");
    return Path.IsBlocked() ? Refuse(INVALID_HANDLE_VALUE) : OriginalFindFirstFileExW(Path.Get(), Level, Data, Search, Filter, Flags);
}

BOOL WINAPI DetourCreateDirectoryW(LPCWSTR Name, LPSECURITY_ATTRIBUTES Security)
{
    const PathArg Path(Name, "Ordner anlegen");
    return Path.IsBlocked() ? Refuse(FALSE) : OriginalCreateDirectoryW(Path.Get(), Security);
}

BOOL WINAPI DetourMoveFileW(LPCWSTR From, LPCWSTR To)
{
    const PathArg Source(From, "verschieben"), Target(To, "verschieben");
    return Source.IsBlocked() || Target.IsBlocked() ? Refuse(FALSE) : OriginalMoveFileW(Source.Get(), Target.Get());
}

BOOL WINAPI DetourMoveFileExW(LPCWSTR From, LPCWSTR To, DWORD Flags)
{
    const PathArg Source(From, "verschieben"), Target(To, "verschieben");
    return Source.IsBlocked() || Target.IsBlocked() ? Refuse(FALSE) : OriginalMoveFileExW(Source.Get(), Target.Get(), Flags);
}

BOOL WINAPI DetourMoveFileWithProgressW(LPCWSTR From, LPCWSTR To, LPPROGRESS_ROUTINE Progress, LPVOID Data, DWORD Flags)
{
    const PathArg Source(From, "verschieben"), Target(To, "verschieben");
    return Source.IsBlocked() || Target.IsBlocked() ? Refuse(FALSE) : OriginalMoveFileWithProgressW(Source.Get(), Target.Get(), Progress, Data, Flags);
}

BOOL WINAPI DetourReplaceFileW(LPCWSTR Replaced, LPCWSTR Replacement, LPCWSTR Backup, DWORD Flags, LPVOID Exclude, LPVOID Reserved)
{
    const PathArg A(Replaced, "ersetzen"), B(Replacement, "ersetzen"), C(Backup, "ersetzen");
    if (A.IsBlocked() || B.IsBlocked() || C.IsBlocked())
        return Refuse(FALSE);
    return OriginalReplaceFileW(A.Get(), B.Get(), C.Get(), Flags, Exclude, Reserved);
}

BOOL WINAPI DetourCopyFileW(LPCWSTR From, LPCWSTR To, BOOL FailIfExists)
{
    const PathArg Source(From, "kopieren"), Target(To, "kopieren");
    return Source.IsBlocked() || Target.IsBlocked() ? Refuse(FALSE) : OriginalCopyFileW(Source.Get(), Target.Get(), FailIfExists);
}

BOOL WINAPI DetourCopyFileExW(LPCWSTR From, LPCWSTR To, LPPROGRESS_ROUTINE Progress, LPVOID Data, LPBOOL Cancel, DWORD Flags)
{
    const PathArg Source(From, "kopieren"), Target(To, "kopieren");
    return Source.IsBlocked() || Target.IsBlocked() ? Refuse(FALSE) : OriginalCopyFileExW(Source.Get(), Target.Get(), Progress, Data, Cancel, Flags);
}

HRESULT WINAPI DetourCopyFile2(PCWSTR From, PCWSTR To, COPYFILE2_EXTENDED_PARAMETERS* Parameters)
{
    const PathArg Source(From, "kopieren"), Target(To, "kopieren");
    if (Source.IsBlocked() || Target.IsBlocked())
        return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
    return OriginalCopyFile2(Source.Get(), Target.Get(), Parameters);
}

struct Hook
{
    const wchar_t* Module;
    const char* Export;
    void* Detour;
    void** Original;
};

const Hook Hooks[] = {
    {L"kernelbase.dll", "CreateFileW", reinterpret_cast<void*>(&DetourCreateFileW), reinterpret_cast<void**>(&OriginalCreateFileW)},
    {L"kernelbase.dll", "CreateFile2", reinterpret_cast<void*>(&DetourCreateFile2), reinterpret_cast<void**>(&OriginalCreateFile2)},
    {L"kernelbase.dll", "DeleteFileW", reinterpret_cast<void*>(&DetourDeleteFileW), reinterpret_cast<void**>(&OriginalDeleteFileW)},
    {L"kernelbase.dll", "RemoveDirectoryW", reinterpret_cast<void*>(&DetourRemoveDirectoryW), reinterpret_cast<void**>(&OriginalRemoveDirectoryW)},
    {L"kernelbase.dll", "GetFileAttributesW", reinterpret_cast<void*>(&DetourGetFileAttributesW), reinterpret_cast<void**>(&OriginalGetFileAttributesW)},
    {L"kernelbase.dll", "GetFileAttributesExW", reinterpret_cast<void*>(&DetourGetFileAttributesExW), reinterpret_cast<void**>(&OriginalGetFileAttributesExW)},
    {L"kernelbase.dll", "SetFileAttributesW", reinterpret_cast<void*>(&DetourSetFileAttributesW), reinterpret_cast<void**>(&OriginalSetFileAttributesW)},
    {L"kernelbase.dll", "FindFirstFileW", reinterpret_cast<void*>(&DetourFindFirstFileW), reinterpret_cast<void**>(&OriginalFindFirstFileW)},
    {L"kernelbase.dll", "FindFirstFileExW", reinterpret_cast<void*>(&DetourFindFirstFileExW), reinterpret_cast<void**>(&OriginalFindFirstFileExW)},
    {L"kernelbase.dll", "CreateDirectoryW", reinterpret_cast<void*>(&DetourCreateDirectoryW), reinterpret_cast<void**>(&OriginalCreateDirectoryW)},
    {L"kernel32.dll", "MoveFileW", reinterpret_cast<void*>(&DetourMoveFileW), reinterpret_cast<void**>(&OriginalMoveFileW)},
    {L"kernelbase.dll", "MoveFileExW", reinterpret_cast<void*>(&DetourMoveFileExW), reinterpret_cast<void**>(&OriginalMoveFileExW)},
    {L"kernelbase.dll", "MoveFileWithProgressW", reinterpret_cast<void*>(&DetourMoveFileWithProgressW), reinterpret_cast<void**>(&OriginalMoveFileWithProgressW)},
    {L"kernelbase.dll", "ReplaceFileW", reinterpret_cast<void*>(&DetourReplaceFileW), reinterpret_cast<void**>(&OriginalReplaceFileW)},
    {L"kernelbase.dll", "CopyFileW", reinterpret_cast<void*>(&DetourCopyFileW), reinterpret_cast<void**>(&OriginalCopyFileW)},
    {L"kernelbase.dll", "CopyFileExW", reinterpret_cast<void*>(&DetourCopyFileExW), reinterpret_cast<void**>(&OriginalCopyFileExW)},
    {L"kernelbase.dll", "CopyFile2", reinterpret_cast<void*>(&DetourCopyFile2), reinterpret_cast<void**>(&OriginalCopyFile2)},
};

std::wstring Environment(const wchar_t* Name)
{
    std::wstring Value(32768, L'\0');
    const DWORD Length = GetEnvironmentVariableW(Name, Value.data(), static_cast<DWORD>(Value.size()));
    Value.resize(Length < Value.size() ? Length : 0);
    return Value;
}

std::wstring FullPath(const std::wstring& Path)
{
    std::wstring Full(32768, L'\0');
    const DWORD Length = GetFullPathNameW(Path.c_str(), static_cast<DWORD>(Full.size()), Full.data(), nullptr);
    Full.resize(Length < Full.size() ? Length : 0);
    return Full;
}

std::wstring ExeDirectory()
{
    std::wstring Path(32768, L'\0');
    const DWORD Length = GetModuleFileNameW(nullptr, Path.data(), static_cast<DWORD>(Path.size()));
    Path.resize(Length < Path.size() ? Length : 0);
    return SavePaths::Parent(SavePaths::Normalize(Path));
}

// Die Wurzel, die das Spiel tatsächlich nutzt, kommt zuerst: -UserDir, sonst %LOCALAPPDATA%\DDSurvivors.
// Die übrigen Kandidaten werden trotzdem umgeleitet, damit kein normaler Spielstand erreichbar bleibt.
bool BuildRoots(bool& UsesUserDir)
{
    Roots.clear();
    const auto UserDir = SavePaths::ParseUserDir(GetCommandLineW());
    UsesUserDir = UserDir.has_value();
    if (UserDir)
    {
        const auto Full = FullPath(*UserDir);
        if (Full.empty())
            return false;
        SavePaths::AddRoot(Roots, SavePaths::RootForSaved(SavePaths::Normalize(Full) + L"\\Saved"));
    }
    if (const auto Local = Environment(L"LOCALAPPDATA"); !Local.empty())
        SavePaths::AddRoot(Roots, SavePaths::RootForSaved(SavePaths::Normalize(FullPath(Local)) + L"\\DDSurvivors\\Saved"));
    // <Spiel>\DDSurvivors\Binaries\Win64\<exe>: Saved neben Binaries.
    if (const auto Exe = ExeDirectory(); !Exe.empty())
        if (const auto Project = SavePaths::Parent(SavePaths::Parent(Exe)); !Project.empty())
            SavePaths::AddRoot(Roots, SavePaths::RootForSaved(Project + L"\\Saved"));
    return !Roots.empty();
}

// Läuft vor den Hooks. Liest den normalen Ordner nur, schreibt nur in den Mod-Ordner und überschreibt dort nichts.
void SeedOnce(const SavePaths::Root& Active, bool CopyNormalSaves)
{
    namespace fs = std::filesystem;
    std::error_code Error;
    const fs::path Mod(Active.Mod), Normal(Active.Normal);
    if (fs::exists(Mod, Error))
    {
        Core::Log::Info("Spielstand: Mod-Ordner {} vorhanden, Mod nutzt ihre eigenen Stände", Narrow(std::wstring(SavePaths::ModFolder)));
        return;
    }
    if (!fs::create_directories(Mod, Error))
    {
        Core::Log::Warn("Spielstand: Mod-Ordner ließ sich nicht anlegen, das Spiel legt ihn selbst an");
        return;
    }
    if (!CopyNormalSaves || !fs::is_directory(Normal, Error))
    {
        Core::Log::Info("Spielstand: Mod-Ordner neu angelegt, leer");
        return;
    }
    int Copied = 0;
    for (const auto& Entry : fs::directory_iterator(Normal, Error))
    {
        if (!Entry.is_regular_file(Error) || !SavePaths::ShouldSeedFile(Entry.path().filename().wstring()))
            continue;
        const auto Target = Mod / Entry.path().filename();
        if (CopyFileW(Entry.path().c_str(), Target.c_str(), TRUE))
            ++Copied;
        else
            Core::Log::Warn("Spielstand: {} nicht in den Mod-Ordner kopiert ({})", Narrow(Entry.path().filename().wstring()), GetLastError());
    }
    Core::Log::Info("Spielstand: Mod-Ordner neu angelegt, {} normale Stände kopiert, die Originale bleiben unverändert", Copied);
}
}

bool Install(bool CopyNormalSaves)
{
    bool UsesUserDir = false;
    if (!BuildRoots(UsesUserDir))
    {
        Core::Log::Error("Spielstand: Ordner unbekannt, Mod bleibt aus, damit der normale Spielstand unberührt bleibt");
        return false;
    }
    SeedOnce(Roots.front(), CopyNormalSaves);

    std::vector<void*> Installed;
    for (const auto& H : Hooks)
    {
        const HMODULE Module = GetModuleHandleW(H.Module);
        void* Target = Module ? reinterpret_cast<void*>(GetProcAddress(Module, H.Export)) : nullptr;
        if (Target && Core::Hooks::Install(Target, H.Detour, H.Original))
        {
            Installed.push_back(Target);
            continue;
        }
        Core::Log::Error("Spielstand: Umleitung für {} fehlt, Mod bleibt aus, damit der normale Spielstand unberührt bleibt", H.Export);
        for (void* Done : Installed)
            Core::Hooks::Remove(Done);
        return false;
    }
    Core::Log::Info("Spielstand: Mod speichert in {} neben SaveGames ({} Ordner, {}), normaler Spielstand bleibt unberührt",
                    Narrow(std::wstring(SavePaths::ModFolder)), Roots.size(), UsesUserDir ? "UserDir" : "Standard");
    return true;
}

long long RedirectedCalls()
{
    return Calls.load(std::memory_order_relaxed);
}
}
