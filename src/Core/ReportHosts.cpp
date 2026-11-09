#include "Core/ReportHosts.h"

// winsock2.h vor windows.h, sonst kollidiert es mit winsock.h. Gelinkt wird ws2_32 nicht, die Ziele kommen per GetProcAddress.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <cwctype>
#include <mutex>
#include <set>
#include <vector>

#include "Core/Hooks.h"
#include "Core/Log.h"

namespace Core::ReportHosts
{
namespace
{
// Fest im Spiel: Bug-Reports und Berichte über beschädigte Spielstände (Codecks), Spielstand-Analytics (Pixelsplit) und
// ein Dienst, den das Spiel beim Run-Ende anfragt (layerapps, Inhalt nicht untersucht, siehe docs/GAME_ANALYSIS.md) und die
// Weekly-Challenge (ddrweekly), die mit Mod gesperrt bleibt wie Erfolge und Bestenlisten (Entscheidung weeklymode).
const std::vector<std::wstring> DefaultHosts = {L"codecks.io", L"pixelsplit.games", L"deadlydays.layerapps.com",
                                          L"ddrweekly.deadlydays.com"};

std::mutex Lock;
std::vector<std::wstring> Hosts = DefaultHosts;
std::set<std::wstring> Seen;
constexpr size_t MaxSeen = 256;

std::wstring Lower(std::wstring_view Text)
{
    std::wstring Out(Text);
    for (wchar_t& Character : Out)
        Character = static_cast<wchar_t>(towlower(Character));
    while (!Out.empty() && Out.back() == L'.')
        Out.pop_back();
    return Out;
}

std::string Narrow(std::wstring_view Text)
{
    std::string Out;
    for (wchar_t Character : Text)
        Out.push_back(Character < 0x80 ? static_cast<char>(Character) : '?');
    return Out;
}

// IP-Adressen gehören nicht ins Log, siehe README. Namen enthalten mindestens einen Buchstaben, IPv6 einen Doppelpunkt.
bool IsAddress(std::wstring_view Host)
{
    if (Host.find(L':') != std::wstring_view::npos)
        return true;
    for (wchar_t Character : Host)
        if (iswalpha(Character))
            return false;
    return true;
}

// Jeden Namen einmal loggen, das zeigt im Test, welche Ziele das Spiel wirklich anfragt.
void Report(const std::wstring& Host, bool Blocked)
{
    if (IsAddress(Host))
        return;
    {
        std::lock_guard Guard(Lock);
        if (Seen.size() >= MaxSeen || !Seen.insert(Host).second)
            return;
    }
    if (Blocked)
        Log::Warn("Berichte: Namensauflösung für {} gesperrt", Narrow(Host));
    else
        Log::Info("Berichte: Namensauflösung für {} erlaubt", Narrow(Host));
}

// Wahr, wenn die Anfrage scheitern soll. Name null heißt lokale Adresse, die bleibt erlaubt.
bool Refuse(std::wstring_view Name)
{
    if (Name.empty())
        return false;
    const std::wstring Host = Lower(Name);
    const bool Blocked = IsBlocked(Host);
    Report(Host, Blocked);
    return Blocked;
}

using GetAddrInfoAFn = INT(WSAAPI*)(PCSTR, PCSTR, const ADDRINFOA*, PADDRINFOA*);
using GetAddrInfoWFn = INT(WSAAPI*)(PCWSTR, PCWSTR, const ADDRINFOW*, PADDRINFOW*);
using GetAddrInfoExWFn = INT(WSAAPI*)(PCWSTR, PCWSTR, DWORD, LPGUID, const ADDRINFOEXW*, PADDRINFOEXW*, timeval*, LPOVERLAPPED,
                                      LPLOOKUPSERVICE_COMPLETION_ROUTINE, LPHANDLE);
GetAddrInfoAFn OriginalA = nullptr;
GetAddrInfoWFn OriginalW = nullptr;
GetAddrInfoExWFn OriginalExW = nullptr;

INT WSAAPI DetourA(PCSTR Name, PCSTR Service, const ADDRINFOA* Hints, PADDRINFOA* Result)
{
    if (Name)
    {
        std::wstring Wide;
        for (const char* Character = Name; *Character; ++Character)
            Wide.push_back(static_cast<unsigned char>(*Character));
        if (Refuse(Wide))
        {
            if (Result)
                *Result = nullptr;
            SetLastError(WSAHOST_NOT_FOUND);
            return WSAHOST_NOT_FOUND;
        }
    }
    return OriginalA(Name, Service, Hints, Result);
}

INT WSAAPI DetourW(PCWSTR Name, PCWSTR Service, const ADDRINFOW* Hints, PADDRINFOW* Result)
{
    if (Name && Refuse(Name))
    {
        if (Result)
            *Result = nullptr;
        SetLastError(WSAHOST_NOT_FOUND);
        return WSAHOST_NOT_FOUND;
    }
    return OriginalW(Name, Service, Hints, Result);
}

INT WSAAPI DetourExW(PCWSTR Name, PCWSTR Service, DWORD NameSpace, LPGUID NspId, const ADDRINFOEXW* Hints, PADDRINFOEXW* Result,
                     timeval* Timeout, LPOVERLAPPED Overlapped, LPLOOKUPSERVICE_COMPLETION_ROUTINE Completion, LPHANDLE Handle)
{
    // Auch asynchron sofort ablehnen: Ein Fehlerwert ungleich WSA_IO_PENDING heißt, es kommt kein Rückruf.
    if (Name && Refuse(Name))
    {
        if (Result)
            *Result = nullptr;
        SetLastError(WSAHOST_NOT_FOUND);
        return WSAHOST_NOT_FOUND;
    }
    return OriginalExW(Name, Service, NameSpace, NspId, Hints, Result, Timeout, Overlapped, Completion, Handle);
}

template <typename T> bool HookExport(HMODULE Module, const char* Name, T Detour, T* Original)
{
    auto* Target = reinterpret_cast<T>(GetProcAddress(Module, Name));
    if (!Target || !Hooks::Install(Target, Detour, Original))
    {
        Log::Error("Berichte: Hook auf {} fehlgeschlagen", Name);
        return false;
    }
    return true;
}
}

bool IsBlocked(std::wstring_view Host)
{
    const std::wstring Name = Lower(Host);
    if (Name.empty())
        return false;
    std::lock_guard Guard(Lock);
    for (const std::wstring& Entry : Hosts)
    {
        if (Name == Entry)
            return true;
        if (Name.size() > Entry.size() && Name.ends_with(Entry) && Name[Name.size() - Entry.size() - 1] == L'.')
            return true;
    }
    return false;
}

void Add(std::wstring_view Host)
{
    const std::wstring Name = Lower(Host);
    if (Name.empty())
        return;
    if (IsBlocked(Name))
    {
        Log::Info("Berichte: {} ist bereits gesperrt", Narrow(Name));
        return;
    }
    {
        std::lock_guard Guard(Lock);
        Hosts.push_back(Name);
    }
    Log::Info("Berichte: {} zusätzlich gesperrt", Narrow(Name));
}

std::wstring HostFromUrl(std::wstring_view Url)
{
    const size_t Scheme = Url.find(L"://");
    std::wstring_view Rest = Scheme == std::wstring_view::npos ? Url : Url.substr(Scheme + 3);
    const size_t At = Rest.find_first_of(L"@/?#");
    if (At != std::wstring_view::npos && Rest[At] == L'@')
        Rest = Rest.substr(At + 1);
    const size_t End = Rest.find_first_of(L":/?#");
    return Lower(Rest.substr(0, End));
}

bool Block()
{
    HMODULE Sockets = GetModuleHandleW(L"ws2_32.dll");
    if (!Sockets)
        Sockets = LoadLibraryW(L"ws2_32.dll");
    if (!Sockets)
    {
        Log::Error("Berichte: ws2_32.dll nicht verfügbar, Sperre nicht aktiv");
        return false;
    }
    const bool Ok = HookExport(Sockets, "getaddrinfo", &DetourA, &OriginalA) & HookExport(Sockets, "GetAddrInfoW", &DetourW, &OriginalW) &
                    HookExport(Sockets, "GetAddrInfoExW", &DetourExW, &OriginalExW);
    if (!Ok)
        return false;
    // Selbsttest über den echten Export: Ein gesperrter Name muss ohne Netzanfrage scheitern.
    auto* Lookup = reinterpret_cast<GetAddrInfoAFn>(GetProcAddress(Sockets, "getaddrinfo"));
    PADDRINFOA Result = nullptr;
    const INT Status = Lookup ? Lookup("api.codecks.io", "443", nullptr, &Result) : -1;
    if (Status != WSAHOST_NOT_FOUND || Result)
    {
        Log::Error("Berichte: Selbsttest fehlgeschlagen, Ergebnis {}", Status);
        return false;
    }
    Log::Info("Berichte: Telemetrie- und Berichtsziele gesperrt, Selbsttest bestanden");
    return true;
}
}
