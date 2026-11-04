// Client: gemeinsame Kamera ohne GameMode, Bodenfix für die Bewegung und die Kamera als ViewTarget. Aus Spike Camera.cpp.
#include <string>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t CameraPlayersFunction = 0x5A0DA90;
constexpr uintptr_t ArrayResizeGrow = 0x1272520;
constexpr uintptr_t CameraGameMode = 0x318;
constexpr uintptr_t PlayerStatePawn = 0x320;
constexpr uintptr_t CharacterMovementBase = 0x350;
constexpr uintptr_t ComponentMobility = 0x1AB;
constexpr uint8_t MobilityStatic = 0;
constexpr uint8_t MobilityMovable = 2;
constexpr uintptr_t ActorReplicatesBits = 0x5B;
constexpr uint8_t ReplicatesBit = 0x08;
constexpr uintptr_t ControllerPlayer = 0x348;
constexpr uintptr_t ControllerCameraManager = 0x360;
constexpr uintptr_t CameraManagerViewTarget = 0x340;

struct PointerArray
{
    uintptr_t* Data;
    int32_t Num;
    int32_t Max;
};

using CameraPlayersFn = PointerArray*(__fastcall*)(uintptr_t, PointerArray*);
CameraPlayersFn OriginalCameraPlayers = nullptr;

// Bewegliche Böden lassen den Client relativ zum Boden senden. Die Bodenkacheln sind nicht repliziert, der Host liest
// die Werte als Weltposition. Static erzwingt Weltkoordinaten.
void MakeFloorStatic(uintptr_t Pawn)
{
    const auto Floor = Memory::ReadPointer(Pawn + CharacterMovementBase);
    const auto Owner = Floor ? Memory::ReadPointer(*Floor + Offsets::UObject::Outer) : std::nullopt;
    if (!Owner || Memory::Read<uint8_t>(*Floor + ComponentMobility) != MobilityMovable)
        return;
    if (Memory::Read<uint8_t>(*Owner + ActorReplicatesBits).value_or(ReplicatesBit) & ReplicatesBit)
        return;
    Memory::WriteBytes(*Floor + ComponentMobility, &MobilityStatic, 1);
}

// Die Kamera holt ihre Spieler aus GameMode->SpawnedPlayers. Der Client hat keinen GameMode, also kommen sie aus der
// replizierten PlayerArray.
PointerArray* __fastcall CameraPlayersHook(uintptr_t Camera, PointerArray* Out)
{
    if (Memory::ReadPointer(Camera + CameraGameMode))
        return OriginalCameraPlayers(Camera, Out);
    Out->Data = nullptr;
    Out->Num = 0;
    Out->Max = 0;
    const uintptr_t World = CurrentWorld();
    const uintptr_t GameState = World ? Memory::ReadPointer(World + Offsets::UWorld::GameState).value_or(0) : 0;
    if (!GameState)
        return Out;
    using ResizeGrowFn = int32_t(__fastcall*)(uint32_t, PointerArray*, int32_t*);
    const auto ResizeGrow = reinterpret_cast<ResizeGrowFn>(At(ArrayResizeGrow));
    const uintptr_t States = Memory::ReadPointer(GameState + Offsets::AGameStateBase::PlayerArray).value_or(0);
    const int32_t Count = Memory::Read<int32_t>(GameState + Offsets::AGameStateBase::PlayerArray + 8).value_or(0);
    for (int32_t i = 0; States && i < Count && i < 16; ++i)
    {
        const auto State = Memory::ReadPointer(States + 8 * i);
        const auto Pawn = State ? Memory::ReadPointer(*State + PlayerStatePawn) : std::nullopt;
        if (!Pawn)
            continue;
        MakeFloorStatic(*Pawn);
        const int32_t Index = Out->Num == Out->Max ? ResizeGrow(0x808, Out, &Out->Max) : Out->Num;
        Out->Num = Index + 1;
        Out->Data[Index] = *Pawn;
    }
    return Out;
}

bool CheckCamera()
{
    return Code::Expect(At(CameraPlayersFunction), {0x48, 0x89, 0x5C, 0x24, 0x18}, "in der Kamera");
}

bool ApplyCamera(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(CameraPlayersFunction)), reinterpret_cast<void*>(&CameraPlayersHook),
                                reinterpret_cast<void**>(&OriginalCameraPlayers));
}

// Beim Client landet die gemeinsame Kamera nicht als ViewTarget, er schaut sonst aus der eigenen Figur. Nur der lokale
// Controller (mit LocalPlayer) und die Kamera der aktuellen Welt zählen, alte Objekte leben nach der Reise noch.
void ViewThroughSharedCamera()
{
    const uintptr_t World = CurrentWorld();
    const uintptr_t Level = World ? Memory::ReadPointer(World + Offsets::UWorld::PersistentLevel).value_or(0) : 0;
    if (!Level)
        return;
    uintptr_t Camera = 0, Controller = 0;
    const int32_t Total = Objects::Count();
    for (int32_t i = 0; i < Total && !(Camera && Controller); ++i)
    {
        const uintptr_t Object = Objects::At(i);
        if (!Object || Memory::ReadPointer(Object + Offsets::UObject::Outer).value_or(0) != Level)
            continue;
        const std::string Class = Memory::NameOf(Memory::ReadPointer(Object + Offsets::UObject::Class).value_or(0));
        if (!Camera && Class == "BP_PlayerCameraActor_C")
            Camera = Object;
        else if (!Controller && Class == "BP_DDPlayerController_C" && Memory::ReadPointer(Object + ControllerPlayer))
            Controller = Object;
    }
    const uintptr_t Manager = Controller ? Memory::ReadPointer(Controller + ControllerCameraManager).value_or(0) : 0;
    if (!Camera || !Manager)
        return;
    const uintptr_t Current = Memory::ReadPointer(Manager + CameraManagerViewTarget).value_or(0);
    if (Current == Camera)
        return;
    static const uintptr_t Function = Objects::FindObject("SetViewTargetWithBlend", "PlayerController");
    if (!Function)
        return;
    struct
    {
        uintptr_t Target;
        float BlendTime;
        uint8_t BlendFunction;
        float BlendExponent;
        bool LockOutgoing;
    } Params{Camera, 0.f, 0, 0.f, false};
    Objects::ProcessEvent(Controller, Function, &Params);
    Core::Log::Info("Kamera: gemeinsame Kamera gesetzt, vorher {}", Current ? Memory::NameOf(Current) : std::string("keine"));
}

const Patch List[] = {
    {"Kamera ohne GameMode", &CheckCamera, &ApplyCamera},
};
}

std::span<const Patch> CameraPatches()
{
    return List;
}

void TickCamera(NetRole Role, bool Second)
{
    // Das Spiel setzt das ViewTarget beim Possess wieder auf die Figur, deshalb jede Sekunde.
    if (Role == NetRole::Client && Second)
        ViewThroughSharedCamera();
}
}
