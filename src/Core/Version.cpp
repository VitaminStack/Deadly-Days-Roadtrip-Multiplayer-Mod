#include "Core/Version.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>

#include "Core/Log.h"

namespace Core
{
namespace
{
constexpr std::wstring_view GameExe = L"DDSurvivors-Win64-Shipping.exe";

struct SupportedBuild
{
    uint32_t BuildId;
    BuildFingerprint Fingerprint;
};

// Bei einem Spiel-Update hier ergänzen, siehe CONTRIBUTING.md, Abschnitt Spiel-Updates.
constexpr std::array SupportedBuilds = {
    SupportedBuild{26187885, {0x32C34474, 0x0B742000}},
};

bool IsGameProcess()
{
    std::wstring Path(MAX_PATH, L'\0');
    const DWORD Length = GetModuleFileNameW(nullptr, Path.data(), static_cast<DWORD>(Path.size()));
    if (!Length || Length >= Path.size())
        return false;
    Path.resize(Length);
    return _wcsicmp(std::filesystem::path(Path).filename().c_str(), GameExe.data()) == 0;
}
}

std::optional<BuildFingerprint> ReadFingerprint(const void* ImageBase)
{
    if (!ImageBase)
        return std::nullopt;
    const auto Base = static_cast<const uint8_t*>(ImageBase);
    const auto Dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(Base);
    if (Dos->e_magic != IMAGE_DOS_SIGNATURE || Dos->e_lfanew <= 0)
        return std::nullopt;
    const auto Nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(Base + Dos->e_lfanew);
    if (Nt->Signature != IMAGE_NT_SIGNATURE || Nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return std::nullopt;
    return BuildFingerprint{Nt->FileHeader.TimeDateStamp, Nt->OptionalHeader.SizeOfImage};
}

std::optional<uint32_t> FindSupportedBuild(const BuildFingerprint& Fingerprint)
{
    const auto Found = std::ranges::find(SupportedBuilds, Fingerprint, &SupportedBuild::Fingerprint);
    if (Found == SupportedBuilds.end())
        return std::nullopt;
    return Found->BuildId;
}

std::optional<uint32_t> DetectSupportedBuild()
{
    if (!IsGameProcess())
    {
        Log::Warn("Prozess ist nicht das Spiel, Mod bleibt aus");
        return std::nullopt;
    }
    const auto Fingerprint = ReadFingerprint(GetModuleHandleW(nullptr));
    if (!Fingerprint)
    {
        Log::Error("Exe-Header nicht lesbar, Mod bleibt aus");
        return std::nullopt;
    }
    const auto Build = FindSupportedBuild(*Fingerprint);
    if (!Build)
        Log::Error("Unbekannter Spiel-Build (TimeDateStamp {:#x}, SizeOfImage {:#x}), Mod bleibt aus", Fingerprint->TimeDateStamp, Fingerprint->SizeOfImage);
    return Build;
}
}
