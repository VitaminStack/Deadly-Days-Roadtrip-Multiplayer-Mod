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

// Beim Client landet die gemeinsame Kamera nicht als ViewTarget, er schaut sonst aus der eigenen Figur.
// Nur der lokale Controller (mit LocalPlayer) und die Kamera der aktuellen Welt zaehlen: Nach einem Kartenwechsel
// leben die alten Objekte noch eine Weile. Das Spiel setzt das ViewTarget beim Possess wieder auf die Figur,
// deshalb jede Sekunde pruefen statt eine feste Zahl von Wiederholungen.
void ViewThroughSharedCamera()
{
    constexpr uintptr_t kControllerPlayer = 0x348;
    constexpr uintptr_t kControllerCameraManager = 0x360;
    constexpr uintptr_t kCameraManagerViewTarget = 0x340;
    constexpr uintptr_t kWorldPersistentLevel = 0x30;
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Level = World ? Read<uintptr_t>(World + kWorldPersistentLevel) : 0;
    if (!Level)
        return;
    uintptr_t Camera = 0, Controller = 0;
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    for (int32_t i = 0; i < Count && !(Camera && Controller); ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x10) || Read<uintptr_t>(Object + 0x20) != Level)
            continue;
        const std::string Class = NameOf(Read<uintptr_t>(Object + 0x10));
        if (!Camera && Class == "BP_PlayerCameraActor_C")
            Camera = Object;
        else if (!Controller && Class == "BP_DDPlayerController_C" && Read<uintptr_t>(Object + kControllerPlayer))
            Controller = Object;
    }
    const uintptr_t Manager = Controller ? Read<uintptr_t>(Controller + kControllerCameraManager) : 0;
    if (!Camera || !Manager)
        return;
    const uintptr_t Current = Read<uintptr_t>(Manager + kCameraManagerViewTarget);
    if (Current == Camera)
        return;
    static const uintptr_t Function = FindObject("SetViewTargetWithBlend", "PlayerController");
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
    Read<ProcessEventFn*>(Controller)[kProcessEventIndex](Controller, Function, &Params);
    Log("Kamera: gesetzt, vorher " + (Current ? NameOf(Current) : std::string("keins")));
}
}
