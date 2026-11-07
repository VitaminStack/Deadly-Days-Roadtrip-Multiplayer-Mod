#include "Game/ProgressAdapter.h"

#include <cmath>
#include <chrono>
#include <vector>
#include "Core/Dispatcher.h"
#include "Game/Memory.h"
#include "Game/NetworkObjects.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"

#ifdef DDCOOP_WITH_SDK
#pragma warning(push, 0)
#include <SDK/DDSurvivors_classes.hpp>
#include <SDK/WP_PlayerOverlay_classes.hpp>
#pragma warning(pop)
static_assert(offsetof(SDK::ADDMissionGameState, DayNum) == 0x390);
static_assert(offsetof(SDK::ADDMissionGameState, LevelSystem) == 0x3B8);
static_assert(offsetof(SDK::UDDWidgetController, PlayerState) == 0x30);
static_assert(offsetof(SDK::UWP_PlayerOverlay_C, OverlayWidgetController) == 0x418);
static_assert(offsetof(SDK::UWP_PlayerOverlay_C, ExperienceProgressBar) == 0x3D0);
static_assert(offsetof(SDK::UWP_PlayerOverlay_C, ExperienceProgress) == 0x3E8);
static_assert(offsetof(SDK::UWP_PlayerOverlay_C, TargetExpProgress) == 0x3F0);
#endif

namespace Game
{
namespace
{
// Native UFunctions bleiben mit ihren Klassen geladen; keine Blueprint-Funktionszeiger ueber Reisen cachen.
uintptr_t NativeFunction(const char* Name, const char* Owner, uintptr_t& Cached)
{
    if (!Cached)
        Cached = Objects::FindObject(Name, Owner);
    return Cached;
}
uint64_t Lifetime(uintptr_t Object)
{
    const auto Index = Memory::Read<int32_t>(Object + 0xC);
    if (!Index || *Index < 0 || *Index >= Objects::Count() || Objects::At(*Index) != Object)
        return 0;
    const auto Chunks = Memory::ReadPointer(Memory::Rva(Offsets::GObjects));
    const auto Chunk = Chunks ? Memory::ReadPointer(*Chunks + 8 * (*Index >> 16)) : std::nullopt;
    const auto Serial = Chunk ? Memory::Read<uint32_t>(*Chunk + (*Index & 0xffff) * 0x18 + 0x10) : std::nullopt;
    return Serial ? (uint64_t(*Serial) << 32) | static_cast<uint32_t>(*Index) : 0;
}

uintptr_t MissionState()
{
    const auto World = Memory::ReadPointer(Memory::Rva(Offsets::GWorld));
    const uintptr_t State = World ? Memory::ReadPointer(*World + Offsets::UWorld::GameState).value_or(0) : 0;
    return Objects::IsA(State, "DDMissionGameState") ? State : 0;
}

bool Live(uintptr_t Object)
{
    const auto Flags = Memory::Read<uint32_t>(Object + Offsets::UObject::Flags);
    return Flags && !(*Flags & 0x30);
}
}

std::optional<ProgressContext> ReadProgressContext()
{
    if (!Core::GameThread().IsDrainThread())
        return std::nullopt;
    const uintptr_t World = Patches::CurrentWorld();
    const uintptr_t State = MissionState();
    if (!World || !State || !Live(State))
        return std::nullopt;
    const auto Day = Memory::Read<int32_t>(State + 0x390);
    const uint64_t WorldLife = Lifetime(World), StateLife = Lifetime(State);
    if (!WorldLife || !StateLife || !Day || *Day < 0 || *Day > 100000)
        return std::nullopt;
    const uint64_t Guid = NetworkGuidFor(State);
    if (!Guid)
        return std::nullopt; // Erst nach UE-Replikationsidentitaet an diese Welt binden.
    return ProgressContext{WorldLife, StateLife, Guid, Memory::NameOf(World), *Day, Patches::CurrentRole()};
}

std::optional<SharedProgress> ReadSharedProgress()
{
    if (!Core::GameThread().IsDrainThread() || Patches::CurrentRole() != NetRole::Host)
        return std::nullopt;
    const uintptr_t State = MissionState();
    const uintptr_t Level = State ? Memory::ReadPointer(State + 0x3B8).value_or(0) : 0;
    static uintptr_t LevelFunction = 0, FractionFunction = 0;
    const uintptr_t GetLevel = NativeFunction("GetCurrentLevel", "LevelSystemComponent", LevelFunction);
    const uintptr_t GetFraction = NativeFunction("GetExperiencePercentage", "LevelSystemComponent", FractionFunction);
    if (!Objects::IsA(Level, "LevelSystemComponent") || !GetLevel || !GetFraction)
        return std::nullopt;
    SharedProgress Value;
    Objects::ProcessEvent(Level, GetLevel, &Value.Level);
    Objects::ProcessEvent(Level, GetFraction, &Value.Fraction);
    if (Value.Level < 1 || Value.Level > 100000 || !std::isfinite(Value.Fraction) || Value.Fraction < 0 || Value.Fraction > 1)
        return std::nullopt;
    return Value;
}

unsigned ShowSharedProgress(const SharedProgress& Value)
{
    if (!Core::GameThread().IsDrainThread() || Patches::CurrentRole() != NetRole::Client ||
        Value.Level < 1 || Value.Level > 100000 || !std::isfinite(Value.Fraction) || Value.Fraction < 0 || Value.Fraction > 1)
        return 0;
    const uintptr_t State = MissionState();
    const uintptr_t Players = State ? Memory::ReadPointer(State + Offsets::AGameStateBase::PlayerArray).value_or(0) : 0;
    const int32_t Count = State ? Memory::Read<int32_t>(State + Offsets::AGameStateBase::PlayerArray + 8).value_or(-1) : -1;
    const uintptr_t SetPercent = Objects::FindObject("SetPercent", "ProgressBar");
    const uintptr_t UpdateLevel = Objects::FindObject("UpdateLevel", "WP_PlayerOverlay_C");
    if (!Players || Count < 1 || Count > 2 || !SetPercent || !UpdateLevel)
        return 0;
    // Widgets nur einmal pro Sekunde suchen; zwischendurch schwache Index/Serial-Identitaeten pruefen.
    struct WidgetRef { uintptr_t Object; uint64_t Life; int32_t Level = -1; };
    static std::vector<WidgetRef> Widgets;
    static uint64_t LastState = 0;
    static auto NextScan = std::chrono::steady_clock::now();
    const uint64_t CurrentState = Lifetime(State);
    if (LastState != CurrentState)
    {
        Widgets.clear();
        LastState = CurrentState;
        NextScan = std::chrono::steady_clock::now();
    }
    if (std::chrono::steady_clock::now() >= NextScan)
    {
        NextScan = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        std::erase_if(Widgets, [](const auto& Ref) { return Lifetime(Ref.Object) != Ref.Life || !Live(Ref.Object); });
        for (int32_t Index = 0, Total = Objects::Count(); Index < Total; ++Index)
        {
            const uintptr_t Object = Objects::At(Index);
            if (!Object || !Live(Object) || !Objects::IsA(Object, "WP_PlayerOverlay_C"))
                continue;
            bool Known = false;
            for (const auto& Ref : Widgets)
                Known |= Ref.Object == Object;
            if (!Known)
                Widgets.push_back({Object, Lifetime(Object)});
        }
    }
    unsigned Shown = 0;
    for (auto& Ref : Widgets)
    {
        const uintptr_t Widget = Ref.Object;
        if (Lifetime(Widget) != Ref.Life || !Live(Widget))
            continue;
        const uintptr_t Controller = Memory::ReadPointer(Widget + 0x418).value_or(0);
        const uintptr_t PlayerState = Controller ? Memory::ReadPointer(Controller + 0x30).value_or(0) : 0;
        bool Current = false;
        for (int32_t Player = 0; Player < Count; ++Player)
            Current |= PlayerState && Memory::ReadPointer(Players + 8 * Player).value_or(0) == PlayerState;
        const uintptr_t Bar = Memory::ReadPointer(Widget + 0x3D0).value_or(0);
        if (!Current || !Objects::IsA(Bar, "ProgressBar"))
            continue; // Keine HUD einer alten Welt anfassen.
        const double Fraction = Value.Fraction;
        Memory::WriteBytes(Widget + 0x3E8, &Fraction, sizeof(Fraction));
        Memory::WriteBytes(Widget + 0x3F0, &Fraction, sizeof(Fraction));
        float Percent = Value.Fraction;
        int32_t Level = Value.Level;
        Objects::ProcessEvent(Bar, SetPercent, &Percent);
        if (Ref.Level != Level)
        {
            Objects::ProcessEvent(Widget, UpdateLevel, &Level);
            Ref.Level = Level;
        }
        ++Shown;
    }
    return Shown;
}
}
