#include "Game/PauseAdapter.h"

#include <cmath>
#include <string_view>

#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"
#include "Game/Reflection.h"

namespace Game::Pause
{
namespace
{
constexpr std::string_view Package = "/Script/DDSurvivors";
constexpr std::string_view TimescalePackage = "/Script/PixelsplitTimescaleSystem";
constexpr Reflection::FieldSpec ControllerHud{"/Script/Engine", "PlayerController", "MyHUD", "ObjectProperty", 8};
constexpr Reflection::FieldSpec HudPauseManager{Package, "DDHud", "PauseScreenManager", "ObjectProperty", 8};
constexpr Reflection::FieldSpec ManagerRequest{Package, "PauseScreenManager", "TimeRequest", "StructProperty", 12};
constexpr Reflection::FieldSpec ManagerTimescale{Package, "PauseScreenManager", "TimescaleManager", "ObjectProperty", 8};
constexpr Reflection::FieldSpec ManagerOpenRequests{Package, "PauseScreenManager", "PauseScreenRequests", "ArrayProperty", 16};
// UGameInstance.LocalPlayers, UPlayer.PlayerController wie in RewardPatches; ULevel.WorldSettings und die Felder der
// WorldSettings wie im Desync-Adapter (dort per SDK geprüft).
constexpr uintptr_t GameInstanceLocalPlayers = 0x38;
constexpr uintptr_t PlayerController = 0x30;
constexpr uintptr_t LevelWorldSettings = 0x2B8;
constexpr uintptr_t SettingsTimeDilation = 0x3E0;
constexpr uintptr_t SettingsPauser = 0x4A8;

// FTimeRequest: Zeitfaktor, Priorität, PauseRegardlessOfTimescale (12 Byte mit Füllung).
struct TimeRequest
{
    float Timescale;
    int32_t Priority;
    bool PauseRegardless;
    uint8_t Pad[3];
};
static_assert(sizeof(TimeRequest) == 12);

uintptr_t Pointer(uintptr_t Address)
{
    return Address ? Memory::ReadPointer(Address).value_or(0) : 0;
}

uintptr_t LocalPauseManager()
{
    const auto Hud = Reflection::Property(ControllerHud);
    const auto Manager = Reflection::Property(HudPauseManager);
    if (!Hud || !Manager)
        return 0;
    const uintptr_t World = Patches::CurrentWorld();
    const uintptr_t Instance = Pointer(World ? World + Offsets::UWorld::OwningGameInstance : 0);
    const uintptr_t Players = Pointer(Instance ? Instance + GameInstanceLocalPlayers : 0);
    const uintptr_t Local = Pointer(Players);
    const uintptr_t Controller = Pointer(Local ? Local + PlayerController : 0);
    const uintptr_t HudActor = Pointer(Controller ? Controller + Hud->Offset : 0);
    if (!HudActor || !Patches::IsA(HudActor, "DDHud"))
        return 0;
    const uintptr_t Result = Pointer(HudActor + Manager->Offset);
    return Result && Patches::IsA(Result, "PauseScreenManager") ? Result : 0;
}

uintptr_t WorldSettings()
{
    const uintptr_t World = Patches::CurrentWorld();
    const uintptr_t Level = Pointer(World ? World + Offsets::UWorld::PersistentLevel : 0);
    const uintptr_t Settings = Pointer(Level ? Level + LevelWorldSettings : 0);
    return Settings && Patches::IsA(Settings, "WorldSettings") ? Settings : 0;
}

struct
{
    uintptr_t Requester = 0, World = 0, Manager = 0;
    bool Logged = false;
} Held;
}

std::optional<bool> LocalMenuOpen()
{
    const uintptr_t Manager = LocalPauseManager();
    const auto Requests = Reflection::Property(ManagerOpenRequests);
    const uintptr_t IsPaused = Reflection::Function(Package, "PauseScreenManager", "GetIsPaused");
    if (!Manager || !Requests || !IsPaused)
        return std::nullopt;
    // Nur eine eigene Öffnungsanfrage zählt. Eine vom Host replizierte Weltpause setzt sie nicht, sonst hielte der Client
    // eine Pause des Hosts über dessen Schließen hinaus fest.
    const auto Count = Memory::Read<int32_t>(Manager + Requests->Offset + 8);
    if (!Count || *Count < 0 || *Count > 64)
        return std::nullopt;
    if (*Count == 0)
        return false;
    struct
    {
        bool Result;
    } Params{false};
    Objects::ProcessEvent(Manager, IsPaused, &Params);
    return Params.Result;
}

bool HoldForRemote(bool Hold)
{
    const uintptr_t Remove = Reflection::Function(TimescalePackage, "TimescaleManager", "RemoveRequest");
    const uintptr_t Request = Reflection::Function(TimescalePackage, "TimescaleManager", "RequestTimescale");
    if (!Remove || !Request)
        return false;
    if (!Hold)
    {
        if (!Held.Requester)
            return true;
        // Nach einem Weltwechsel ist die alte WorldSettings weg; die Reise hat alle Zeitanfragen schon abgeräumt.
        if (Patches::CurrentWorld() == Held.World && WorldSettings() == Held.Requester && Held.Manager)
        {
            struct
            {
                uintptr_t Requester;
            } Params{Held.Requester};
            Objects::ProcessEvent(Held.Manager, Remove, &Params);
        }
        Held = {};
        return true;
    }
    const uintptr_t Manager = LocalPauseManager();
    const auto RequestField = Reflection::Property(ManagerRequest);
    const auto TimescaleField = Reflection::Property(ManagerTimescale);
    const uintptr_t Settings = WorldSettings();
    if (!Manager || !RequestField || !TimescaleField || !Settings)
        return false;
    const uintptr_t Timescale = Pointer(Manager + TimescaleField->Offset);
    const auto Values = Memory::Read<TimeRequest>(Manager + RequestField->Offset);
    if (!Timescale || !Patches::IsA(Timescale, "TimescaleManager") || !Values || !std::isfinite(Values->Timescale) ||
        Values->Timescale < 0 || Values->Timescale > 1)
        return false;
    if (!Held.Logged)
    {
        Core::Log::Info("Pause: Zeitanfrage wie Pausemenü, Faktor {} Priorität {} Pause {}", Values->Timescale,
                        Values->Priority, Values->PauseRegardless);
        Held.Logged = true;
    }
    struct
    {
        uintptr_t Requester;
        TimeRequest Value;
        uint8_t Pad[4];
    } Params{Settings, *Values, {}};
    Objects::ProcessEvent(Timescale, Request, &Params);
    Held.Requester = Settings;
    Held.World = Patches::CurrentWorld();
    Held.Manager = Timescale;
    return true;
}

uint64_t WorldKey()
{
    return Patches::CurrentWorld();
}

std::optional<WorldTime> ReadWorldTime()
{
    const uintptr_t Settings = WorldSettings();
    const auto Dilation = Settings ? Memory::Read<float>(Settings + SettingsTimeDilation) : std::nullopt;
    const auto Pauser = Settings ? Memory::Read<uintptr_t>(Settings + SettingsPauser) : std::nullopt;
    if (!Dilation || !Pauser || !std::isfinite(*Dilation))
        return std::nullopt;
    return WorldTime{*Dilation, *Pauser != 0};
}
}
