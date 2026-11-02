// Das Testprogramm importiert dieselben drei Funktionen wie die Spiel-Exe.
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>

int wmain()
{
    wchar_t Executable[MAX_PATH]{}, Loaded[MAX_PATH]{}, System[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, Executable, MAX_PATH) ||
        !GetModuleFileNameW(GetModuleHandleW(L"version.dll"), Loaded, MAX_PATH) ||
        !GetSystemDirectoryW(System, MAX_PATH))
        return 1;
    const std::wstring OwnPath(Executable);
    const std::wstring Expected = OwnPath.substr(0, OwnPath.find_last_of(L"\\") + 1) + L"version.dll";
    if (_wcsicmp(Expected.c_str(), Loaded) != 0)
    {
        std::puts("FAIL: lokaler Proxy wurde nicht geladen");
        return 2;
    }
    const std::wstring Target = std::wstring(System) + L"\\kernel32.dll";
    DWORD Handle = 0;
    const DWORD Size = GetFileVersionInfoSizeW(Target.c_str(), &Handle);
    if (!Size)
        return 3;
    std::vector<unsigned char> Data(Size);
    if (!GetFileVersionInfoW(Target.c_str(), 0, Size, Data.data()))
        return 4;
    VS_FIXEDFILEINFO* Info = nullptr;
    UINT InfoSize = 0;
    if (!VerQueryValueW(Data.data(), L"\\", reinterpret_cast<void**>(&Info), &InfoSize) ||
        InfoSize < sizeof(*Info) || Info->dwSignature != 0xFEEF04BD)
        return 5;
    std::puts("PASS: impliziter lokaler Proxy und alle drei Weiterleitungen");
    return 0;
}
