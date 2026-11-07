#pragma once

// Reine Pfadlogik der Spielstand-Umleitung (mod-saves), ohne Windows- und Spielaufrufe, damit sie testbar bleibt.
// Alles unter <Saved>\SaveGames geht mit der Mod nach <Saved>\SaveGames_DDCoop, siehe docs/GAME_ANALYSIS.md.
#include <cwctype>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Game::SavePaths
{
inline constexpr std::wstring_view NormalFolder = L"SaveGames";
inline constexpr std::wstring_view ModFolder = L"SaveGames_DDCoop";

struct Root
{
    std::wstring Normal; // absolut, Backslashes, ohne abschließenden Trenner
    std::wstring Mod;
};

inline wchar_t Lower(wchar_t C)
{
    return static_cast<wchar_t>(std::towlower(C));
}

inline bool EqualsNoCase(std::wstring_view A, std::wstring_view B)
{
    if (A.size() != B.size())
        return false;
    for (size_t I = 0; I < A.size(); ++I)
        if (Lower(A[I]) != Lower(B[I]))
            return false;
    return true;
}

inline bool ContainsNoCase(std::wstring_view Text, std::wstring_view Part)
{
    if (Part.empty() || Text.size() < Part.size())
        return Part.empty();
    for (size_t I = 0; I + Part.size() <= Text.size(); ++I)
        if (EqualsNoCase(Text.substr(I, Part.size()), Part))
            return true;
    return false;
}

// Schneller Vorfilter für jeden Dateiaufruf: Nur Pfade mit "SaveGames" können umgeleitet werden.
inline bool MayConcernSaves(const wchar_t* Path)
{
    if (!Path)
        return false;
    return ContainsNoCase(std::wstring_view(Path), NormalFolder);
}

// Schrägstriche zu Backslashes, doppelte Trenner zusammenfassen (außer am Anfang), abschließenden Trenner entfernen.
inline std::wstring Normalize(std::wstring_view Path)
{
    std::wstring Out;
    Out.reserve(Path.size());
    for (size_t I = 0; I < Path.size(); ++I)
    {
        const wchar_t C = Path[I] == L'/' ? L'\\' : Path[I];
        if (C == L'\\' && !Out.empty() && Out.back() == L'\\' && Out.size() > 1)
            continue;
        Out.push_back(C);
    }
    while (Out.size() > 3 && Out.back() == L'\\')
        Out.pop_back();
    return Out;
}

inline std::wstring Parent(std::wstring_view Path)
{
    const auto Position = Path.find_last_of(L'\\');
    return Position == std::wstring_view::npos ? std::wstring() : std::wstring(Path.substr(0, Position));
}

// Wurzel zu einem Saved-Ordner. Leer, wenn kein Saved-Ordner bekannt ist.
inline std::optional<Root> RootForSaved(std::wstring_view SavedDirectory)
{
    if (SavedDirectory.empty())
        return std::nullopt;
    const auto Saved = Normalize(SavedDirectory);
    return Root{Saved + L"\\" + std::wstring(NormalFolder), Saved + L"\\" + std::wstring(ModFolder)};
}

// Wert von -UserDir= wie FParse::Value: Groß/klein egal, in Anführungszeichen bis zum nächsten, sonst bis zum Leerraum.
inline std::optional<std::wstring> ParseUserDir(std::wstring_view CommandLine)
{
    constexpr std::wstring_view Key = L"-userdir=";
    for (size_t I = 0; I + Key.size() <= CommandLine.size(); ++I)
    {
        if (!EqualsNoCase(CommandLine.substr(I, Key.size()), Key))
            continue;
        if (I > 0 && !std::iswspace(CommandLine[I - 1]) && CommandLine[I - 1] != L'"')
            continue;
        auto Rest = CommandLine.substr(I + Key.size());
        std::wstring Value;
        if (!Rest.empty() && Rest.front() == L'"')
        {
            const auto End = Rest.find(L'"', 1);
            Value = std::wstring(Rest.substr(1, End == std::wstring_view::npos ? std::wstring_view::npos : End - 1));
        }
        else
        {
            size_t End = 0;
            while (End < Rest.size() && !std::iswspace(Rest[End]) && Rest[End] != L'"')
                ++End;
            Value = std::wstring(Rest.substr(0, End));
        }
        if (Value.empty())
            return std::nullopt;
        return Value;
    }
    return std::nullopt;
}

inline void AddRoot(std::vector<Root>& Roots, std::optional<Root> Candidate)
{
    if (!Candidate)
        return;
    for (const auto& Existing : Roots)
        if (EqualsNoCase(Existing.Normal, Candidate->Normal))
            return;
    Roots.push_back(std::move(*Candidate));
}

// Leitet einen absoluten Pfad um, der in einem normalen SaveGames-Ordner liegt oder ihn selbst meint.
// Ein \\?\-Präfix bleibt erhalten. Pfade im Mod-Ordner bleiben gleich, die Umleitung ist also idempotent.
inline std::optional<std::wstring> Redirect(std::wstring_view FullPath, const std::vector<Root>& Roots)
{
    constexpr std::wstring_view LongPrefix = L"\\\\?\\";
    std::wstring Prefix;
    if (FullPath.size() >= LongPrefix.size() && FullPath.substr(0, LongPrefix.size()) == LongPrefix)
    {
        Prefix = std::wstring(LongPrefix);
        FullPath.remove_prefix(LongPrefix.size());
        if (FullPath.size() >= 4 && EqualsNoCase(FullPath.substr(0, 4), L"UNC\\"))
            return std::nullopt;
    }
    const auto Path = Normalize(FullPath);
    for (const auto& R : Roots)
    {
        if (Path.size() < R.Normal.size() || !EqualsNoCase(std::wstring_view(Path).substr(0, R.Normal.size()), R.Normal))
            continue;
        if (Path.size() == R.Normal.size())
            return Prefix + R.Mod;
        if (Path[R.Normal.size()] == L'\\')
            return Prefix + R.Mod + Path.substr(R.Normal.size());
    }
    return std::nullopt;
}

// Pfad mit einem Ordner "SaveGames" als Bestandteil, der keine bekannte Wurzel trifft. Nur für die Warnung im Log.
inline bool LooksLikeForeignSave(std::wstring_view FullPath)
{
    const auto Path = Normalize(FullPath);
    return ContainsNoCase(Path, L"\\SaveGames\\") && Path.size() > 4 && EqualsNoCase(std::wstring_view(Path).substr(Path.size() - 4), L".sav");
}

// Nur der Dateiname, Logs dürfen keine vollständigen Pfade enthalten.
inline std::wstring FileName(std::wstring_view Path)
{
    const auto Position = Path.find_last_of(L"\\/");
    return std::wstring(Position == std::wstring_view::npos ? Path : Path.substr(Position + 1));
}

// Beim ersten Start mit Mod: normale .sav-Dateien der obersten Ebene in den Mod-Ordner kopieren, ohne Überschreiben.
inline bool ShouldSeedFile(std::wstring_view Name)
{
    return Name.size() > 4 && EqualsNoCase(Name.substr(Name.size() - 4), L".sav");
}
}
