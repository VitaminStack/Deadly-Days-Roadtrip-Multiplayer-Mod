// Die eigentliche Mod-DLL. Bindet alle Module zusammen und startet sie in der richtigen Reihenfolge.
#include <windows.h>

#include <exception>
#include <filesystem>
#include <format>
#include <string>

#include "Core/Config.h"
#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Core/Protection.h"
#include "Core/Version.h"
#include "Game/Game.h"
#include "Mod/Lobby.h"
#include "Net/Net.h"
#include "Sync/Sync.h"
#include "UI/UI.h"

namespace
{
std::filesystem::path OwnDirectory()
{
    HMODULE Self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&OwnDirectory), &Self);
    std::wstring Path(MAX_PATH, L'\0');
    const DWORD Length = GetModuleFileNameW(Self, Path.data(), static_cast<DWORD>(Path.size()));
    Path.resize(Length < Path.size() ? Length : 0);
    return std::filesystem::path(Path).parent_path();
}

void Start()
{
    const auto ModDirectory = OwnDirectory();
    const auto Config = Core::Config::Load(ModDirectory / "ddcoop.ini");

    // Host und Client laufen im Test aus demselben Ordner, deshalb je Prozess eine Datei.
    Core::Log::Open(ModDirectory / "logs" / std::format("ddcoop-{}.log", GetCurrentProcessId()));
    if (const auto Level = Core::Log::ParseLevel(Config.GetString("log_level", "info")))
        Core::Log::SetLevel(*Level);

    if (!Config.GetBool("enabled", true))
    {
        Core::Log::Info("In ddcoop.ini abgeschaltet, Mod bleibt aus");
        return;
    }
    const auto Build = Core::DetectSupportedBuild();
    if (!Build)
        return;
    if (!Core::Hooks::Initialize())
        return;
    // Ohne alle Sperren gegen Berichte an den Hersteller bleibt die Mod aus, Game, Net, Sync und UI starten nicht.
    if (!Core::Protection::Install())
        return;

    // Die Lobby hängt einen Game-Thread-Tick an, das geht nur vor Game::Start.
    Mod::Lobby::Install();
    Game::Start();
    Net::Start();
    Sync::Start();
    UI::Start();
    Core::Log::Info("Mod gestartet, Spiel-Build {}", *Build);
}
}

// Ruft der Loader einmal auf seinem eigenen Thread auf. Nichts darf von hier ins Spiel durchschlagen.
extern "C" __declspec(dllexport) void ModStart()
{
    try
    {
        Start();
    }
    catch (const std::exception& Failure)
    {
        Core::Log::Error("Start fehlgeschlagen, Mod bleibt aus: {}", Failure.what());
    }
    catch (...)
    {
        Core::Log::Error("Start fehlgeschlagen, Mod bleibt aus");
    }
}
