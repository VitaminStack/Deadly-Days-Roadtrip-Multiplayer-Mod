// opt-laufanimation-client, Host: Laufanimation (Wackeln) der Client-Figur. Das Wackeln (UWobbleAnimationComponent) nimmt sein Tempo
// vom UPlayerAnimationVelocityProvider. Der liefert ohne Schalter das CMC-Tempo der Figur, und das bleibt beim Host für die
// Client-Figur fast 0, weil der Host ihre Position nur übernimmt (bewegung). Lokal zuerst: Der Host schaltet den nativen Modus
// „manuell“ des Providers ein, der das Tempo selbst aus der Ortsänderung misst. Keine Nachricht, kein Hook, Ort und CMC bleiben
// unberührt. Befund: docs/GAME_ANALYSIS.md#laufanimation-beim-client-opt-laufanimation-client
#include <windows.h>

#include <cstddef>

#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchPolicy.h"
#include "Game/PatchRuntime.h"

#ifdef DDCOOP_WITH_SDK
#pragma warning(push, 0)
#include <SDK/BP_DD_Character_classes.hpp>
#pragma warning(pop)
static_assert(offsetof(SDK::ABP_DD_Character_C, PlayerAnimationVelocityProvider) == 0x910);
static_assert(sizeof(SDK::UPlayerAnimationVelocityProvider) == 0xF8);
#endif

namespace Game::Patches
{
namespace
{
constexpr uintptr_t CharacterVelocityProvider = 0x910; // BP_DD_Character_C.PlayerAnimationVelocityProvider
constexpr uintptr_t ProviderManualTracking = 0xC0;     // bool, nativ gesetzt von SetManualTrackingActive, gelesen von GetVelocity
constexpr uintptr_t PawnController = 0x2D8;
constexpr uintptr_t ActorRemoteRole = 0x60;
constexpr uint8_t RoleAutonomousProxy = 2;
constexpr uint32_t GarbageOrUnreachable = 0x30;
constexpr uint64_t LogLimit = 8;

uint64_t Switched = 0;

bool Alive(uintptr_t Object)
{
    return Object && !(Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachable) & GarbageOrUnreachable);
}

// Die Figur gehört einem Client: ihr Controller ist beim Host vorhanden und auf der Gegenseite autonom.
bool RemoteControlled(uintptr_t Character)
{
    const uintptr_t Controller = Memory::ReadPointer(Character + PawnController).value_or(0);
    return Alive(Controller) && Memory::Read<uint8_t>(Controller + ActorRemoteRole).value_or(0) == RoleAutonomousProxy;
}

void SetManualTracking(uintptr_t Provider)
{
    // Native UFunction der C++-Klasse, gilt für den ganzen Prozess.
    static const uintptr_t Function = Objects::FindObject("SetManualTrackingActive", "PlayerAnimationVelocityProvider");
    if (!Function)
        return;
    struct
    {
        bool Active;
    } Params{true};
    Objects::ProcessEvent(Provider, Function, &Params);
    if (++Switched <= LogLimit)
        Core::Log::Info("Laufanimation: Tempo der Client-Figur misst der Host aus ihrem Ort ({}. Mal)", Switched);
}
}

void TickWalkAnimation(NetRole Role, bool Second)
{
    // Jede Sekunde neu aus der aktuellen Welt: keine gemerkten Zeiger, Kartenwechsel und freigegebene Figuren brauchen nichts.
    if (Role != NetRole::Host || !Second)
        return;
    const uintptr_t World = CurrentWorld();
    const uintptr_t Level = World ? Memory::ReadPointer(World + Offsets::UWorld::PersistentLevel).value_or(0) : 0;
    if (!Level)
        return;
    Objects::ForEachLevelActor(Level, [](uintptr_t Character)
    {
        if (!Alive(Character) || Memory::NameOf(Memory::ReadPointer(Character + Offsets::UObject::Class).value_or(0)) != "BP_DD_Character_C")
            return;
        const uintptr_t Provider = Memory::ReadPointer(Character + CharacterVelocityProvider).value_or(0);
        if (!Alive(Provider) || !Objects::IsA(Provider, "PlayerAnimationVelocityProvider"))
            return;
        const bool Manual = Memory::Read<uint8_t>(Provider + ProviderManualTracking).value_or(1) != 0;
        if (WalkAnimationNeedsManualTracking(true, RemoteControlled(Character), Manual))
            SetManualTracking(Provider);
    });
}
}
