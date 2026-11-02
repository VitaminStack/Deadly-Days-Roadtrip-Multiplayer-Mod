// Client: Gemeinsame Kamera und Bodenfix fuer die Bewegung.
#include "Spike.h"

namespace Spike
{
namespace
{
constexpr uintptr_t kCameraPlayersFunction = 0x5A0DA90;
constexpr uintptr_t kArrayResizeGrow = 0x1272520;
constexpr uintptr_t kPlayerStatePawn = 0x320;
constexpr uintptr_t kCharacterMovementBase = 0x350;
constexpr uintptr_t kComponentMobility = 0x1AB;
constexpr uintptr_t kActorReplicatesBits = 0x5B;

struct FPointerArray
{
    uintptr_t* Data;
    int32_t Num;
    int32_t Max;
};

// Die Kamera holt ihre Spieler aus GameMode->SpawnedPlayers. Der Client hat keinen GameMode, also kommen sie hier
// aus der replizierten PlayerArray.
FPointerArray* __fastcall CameraPlayersWithoutGameMode(uintptr_t, FPointerArray* Out)
{
    Out->Data = nullptr;
    Out->Num = 0;
    Out->Max = 0;
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t GameState = World ? Read<uintptr_t>(World + kWorldGameState) : 0;
    if (!GameState)
        return Out;

    using ResizeGrowFn = int32_t(__fastcall*)(uint32_t, FPointerArray*, int32_t*);
    const auto ResizeGrow = reinterpret_cast<ResizeGrowFn>(Base + kArrayResizeGrow);
    const uintptr_t States = Read<uintptr_t>(GameState + kGameStatePlayerArray);
    const int32_t Count = Read<int32_t>(GameState + kGameStatePlayerArray + 8);
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t State = Read<uintptr_t>(States + 8 * i);
        const uintptr_t Pawn = State ? Read<uintptr_t>(State + kPlayerStatePawn) : 0;
        if (!Pawn)
            continue;

        // Bewegliche Boeden lassen den Client relativ zum Boden senden. Bodenkacheln sind nicht repliziert, der Host
        // kann sie nicht aufloesen und liest die Werte als Weltposition. Static erzwingt Weltkoordinaten.
        const uintptr_t Floor = Read<uintptr_t>(Pawn + kCharacterMovementBase);
        const uintptr_t FloorOwner = Floor ? Read<uintptr_t>(Floor + 0x20) : 0;
        if (FloorOwner && Read<uint8_t>(Floor + kComponentMobility) == 2 && !(Read<uint8_t>(FloorOwner + kActorReplicatesBits) & 8))
            *reinterpret_cast<uint8_t*>(Floor + kComponentMobility) = 0;

        const int32_t Index = Out->Num == Out->Max ? ResizeGrow(0x808, Out, &Out->Max) : Out->Num;
        Out->Num = Index + 1;
        Out->Data[Index] = Pawn;
    }
    return Out;
}
}

// Die Kamera in AllPlayerFramingCamera+318h (GameMode) ist beim Client leer, dann die Ersatzfunktion nutzen.
bool PatchCamera(CodeWriter& Code)
{
    const uintptr_t Site = Base + kCameraPlayersFunction;
    if (!Expect(Site, {0x48, 0x89, 0x5C, 0x24, 0x18}, "in der Kamera"))
        return false;
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x83, 0xB9, 0x18, 0x03, 0x00, 0x00, 0x00}); // cmp qword ptr [rcx+318h],0
    Code.Emit({0x75, 0x0C});                                     // jne original
    Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(&CameraPlayersWithoutGameMode));
    Code.Emit({0x48, 0x89, 0x5C, 0x24, 0x18});                   // mov [rsp+18h],rbx
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 5);
    WriteJump(Site, Cave);
    return true;
}

// Beim Client landet die gemeinsame Kamera nicht als ViewTarget, er schaut sonst vom Weltursprung aus.
void ViewThroughSharedCamera()
{
    const uintptr_t Camera = FindInstance("BP_PlayerCameraActor_C");
    const uintptr_t Controller = FindInstance("BP_DDPlayerController_C");
    const uintptr_t Function = FindObject("SetViewTargetWithBlend", "PlayerController");
    // Beim Spawn wird das ViewTarget noch ueberschrieben, deshalb eine Weile lang wiederholen.
    static uintptr_t LastCamera = 0;
    static int Repeats = 0;
    if (!Camera || !Controller || !Function)
        return;
    if (Camera != LastCamera)
        Repeats = 15;
    LastCamera = Camera;
    if (Repeats-- <= 0)
        return;
    struct
    {
        uintptr_t Target;
        float BlendTime;
        uint8_t BlendFunction;
        float BlendExponent;
        bool LockOutgoing;
    } Params{Camera, 0.f, 0, 0.f, false};
    Read<ProcessEventFn*>(Controller)[kProcessEventIndex](Controller, Function, &Params);
    Log("Kamera: gesetzt");
}
}
