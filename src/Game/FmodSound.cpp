#include "Game/FmodSound.h"

#include <cstddef>
#include <cstring>

#include "Game/Memory.h"
#include "Game/Objects.h"

#ifdef DDCOOP_WITH_SDK
#pragma warning(push, 0)
#include <SDK/Engine_classes.hpp>
#include <SDK/FMODStudio_parameters.hpp>
#pragma warning(pop)
static_assert(offsetof(SDK::AActor, RootComponent) == 0x1B8);
static_assert(offsetof(SDK::Params::FMODBlueprintStatics_PlayEventAtLocation, Location) == 0x10);
static_assert(offsetof(SDK::Params::FMODBlueprintStatics_PlayEventAtLocation, bAutoPlay) == 0x70);
static_assert(sizeof(SDK::Params::FMODBlueprintStatics_PlayEventAtLocation) == 0x80);
#endif

namespace Game::FmodSound
{
namespace
{
// Build 26786272, wie in WeaponSoundPatches.cpp.
constexpr uintptr_t ActorRootComponent = 0x1B8; // AActor.RootComponent
constexpr uintptr_t ComponentToWorld = 0x1F0;   // USceneComponent.ComponentToWorld (FTransform, 0x60)
constexpr size_t TransformSize = 0x60;

// UFMODBlueprintStatics::PlayEventAtLocation(WorldContextObject, Event, const FTransform& Location, bAutoPlay)
struct alignas(16) PlayEventParams
{
    uintptr_t WorldContextObject = 0;
    uintptr_t Event = 0;
    uint8_t Location[TransformSize]{};
    bool AutoPlay = true;
    uint8_t Pad[7]{};
    uintptr_t ReturnValue = 0;
};
static_assert(sizeof(PlayEventParams) == 0x80);
static_assert(offsetof(PlayEventParams, Location) == 0x10);
static_assert(offsetof(PlayEventParams, AutoPlay) == 0x70);

// Funktion und Defaultobjekt gehören einer nativen Klasse und bleiben bis zum Spielende gültig.
uintptr_t PlayFunction = 0, PlayDefaults = 0;

bool Resolve()
{
    if (!PlayFunction)
        PlayFunction = Objects::FindObject("PlayEventAtLocation", "FMODBlueprintStatics");
    if (!PlayDefaults)
        PlayDefaults = Objects::FindNamed("Default__FMODBlueprintStatics");
    return PlayFunction && PlayDefaults;
}
}

bool PlayAtActor(uintptr_t Actor, uintptr_t Event)
{
    const uintptr_t Root = Actor ? Memory::ReadPointer(Actor + ActorRootComponent).value_or(0) : 0;
    PlayEventParams Params{Actor, Event};
    if (!Event || !Root || !Resolve() || !Memory::ReadBytes(Root + ComponentToWorld, Params.Location, TransformSize))
        return false;
    Objects::ProcessEvent(PlayDefaults, PlayFunction, &Params);
    return true;
}

bool PlayAtLocation(uintptr_t WorldContext, uintptr_t Event, double X, double Y, double Z)
{
    if (!Event || !WorldContext || !Resolve())
        return false;
    PlayEventParams Params{WorldContext, Event};
    // FTransform: Rotation (Quat, 4 double), Translation (3 double + Pad), Scale3D (3 double + Pad).
    const double Transform[12] = {0, 0, 0, 1, X, Y, Z, 0, 1, 1, 1, 0};
    static_assert(sizeof(Transform) == TransformSize);
    std::memcpy(Params.Location, Transform, TransformSize);
    Objects::ProcessEvent(PlayDefaults, PlayFunction, &Params);
    return true;
}
}
