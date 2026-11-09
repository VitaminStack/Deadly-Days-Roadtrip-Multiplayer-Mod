// XP-Baelle sind lokale Niagara-Partikel des AExpEmissionHandler. Nur der Host erzeugt sie.
// Der Host zeichnet jede native Spawn-Anfrage auf, der Client spielt sie nur zur Anzeige nach.
// Beim Client zaehlt keine Aufnahme: ReceiveExp ist dort gesperrt, der XP-Stand kommt als Host-Snapshot.
#include "Game/PatchRuntime.h"
#include "Core/Dispatcher.h"
#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/ExpAdapter.h"
#include "Game/ExpBounds.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <deque>
#include <mutex>

namespace Game
{
namespace
{
// Gemeinsamer Kern von RequestExpSpawning (0x58C1D40) und den nativen Gegner-/Effektpfaden (0x58C7075, 0x58C741E,
// 0x58C7770, 0x59B5A54): rcx = UExpSubsystem, rdx = Spawn-Anfrage.
constexpr uintptr_t SpawnRequest = 0x58C1B10;
constexpr uintptr_t ReceiveExpImpl = 0x58BF210; // UExpSubsystem::ReceiveExp, hinter dem exec-Thunk
constexpr uintptr_t UpdateBounds = 0x58BF120;
constexpr uintptr_t LocalViewportCorners = 0x5B08070;
constexpr size_t QueueLimit = 512;
struct alignas(8) Request
{
    int32_t Amount;
    uint32_t Pad;
    double Start[3], End[3];
    float Color[4];
    double Extra;
    float Tail;
    uint32_t Pad2;
};
static_assert(offsetof(Request, Start) == 0x08 && offsetof(Request, End) == 0x20 && offsetof(Request, Color) == 0x38 &&
              offsetof(Request, Extra) == 0x48 && offsetof(Request, Tail) == 0x50 && sizeof(Request) == 0x58);
using SpawnFn = void(__fastcall*)(uintptr_t, const Request*);
using ReceiveFn = void(__fastcall*)(uintptr_t, int32_t, uintptr_t);
SpawnFn OriginalSpawn = nullptr;
ReceiveFn OriginalReceive = nullptr;
using BoundsFn = void(__fastcall*)(uintptr_t);
using CornersFn = void*(__fastcall*)(void*, uintptr_t, uintptr_t, void*);
BoundsFn OriginalBounds = nullptr;
uintptr_t ClientHandler = 0;
std::mutex Lock;
std::deque<ExpSpawn> Pending;
std::atomic<unsigned> DroppedClientSpawns = 0, DroppedClientReceives = 0;

uintptr_t Pointer(uintptr_t Address)
{
    return Memory::ReadPointer(Address).value_or(0);
}

void RepairClientBounds(uintptr_t Handler)
{
    if (Patches::CurrentRole() != NetRole::Client)
        return;
    // Der native Mehrspieler-Helper liest GameMode.SpawnedPlayers. Auf einem
    // Netzwerkclient fehlt der GameMode: Er liefert nur degenerierte Ersatzecken.
    // Derselbe Einzelspieler-Helper funktioniert mit dem lokalen Controller und
    // der tatsächlich angezeigten Kamera ohne GameMode.
    const uintptr_t World = Patches::CurrentWorld();
    const uintptr_t Instance = World ? Pointer(World + Offsets::UWorld::OwningGameInstance) : 0;
    const uintptr_t Players = Instance ? Pointer(Instance + 0x38) : 0;
    const uintptr_t Local = Players ? Pointer(Players) : 0;
    const uintptr_t Controller = Local ? Pointer(Local + 0x30) : 0;
    const uintptr_t CameraSubsystem = Pointer(Handler + 0x3E8);
    const uintptr_t CameraActor = CameraSubsystem ? Pointer(CameraSubsystem + 0x38) : 0;
    const uintptr_t Camera = CameraActor ? Pointer(CameraActor + 0x2A8) : 0;
    const uintptr_t Emitter = Pointer(Handler + 0x2F8);
    static uintptr_t SetVector = 0;
    if (!Controller || !Camera || !Emitter || !Objects::IsA(Camera, "CameraComponent") ||
        !Objects::IsA(Emitter, "NiagaraComponent"))
        return;
    if (!SetVector)
        SetVector = Objects::FindObject("SetVariableVec3", "NiagaraComponent");
    if (!SetVector)
        return;
    std::array<ExpVisuals::Vector, 4> Corners{};
    ExpVisuals::Vector Player{};
    reinterpret_cast<CornersFn>(Patches::At(LocalViewportCorners))(&Corners, Controller, Camera, &Player);
    std::array<double, 2> Buffer{};
    if (!Memory::ReadBytes(Handler + 0x2D0, Buffer.data(), sizeof(Buffer)))
        return;
    const auto Bounds = ExpVisuals::ViewBounds(Corners, Player, Buffer);
    if (!Bounds)
        return;
    struct
    {
        uint64_t Name;
        ExpVisuals::Vector Value;
    } Params{Memory::Read<uint64_t>(Handler + 0x2E0).value_or(0), *Bounds};
    static_assert(sizeof(Params) == 0x20);
    Memory::WriteBytes(Handler + 0x320, Bounds->data(), sizeof(*Bounds));
    Objects::ProcessEvent(Emitter, SetVector, &Params);
}

void __fastcall BoundsHook(uintptr_t Handler)
{
    OriginalBounds(Handler);
    if (Patches::CurrentRole() == NetRole::Client)
        ClientHandler = Handler;
    RepairClientBounds(Handler);
}

ExpSpawn FromRequest(const Request& In)
{
    ExpSpawn Out{In.Amount};
    std::memcpy(Out.Start, In.Start, sizeof(Out.Start));
    std::memcpy(Out.End, In.End, sizeof(Out.End));
    std::memcpy(Out.Color, In.Color, sizeof(Out.Color));
    Out.Extra = In.Extra;
    Out.Tail = In.Tail;
    return Out;
}

void __fastcall SpawnHook(uintptr_t Subsystem, const Request* In)
{
    const auto Role = Patches::CurrentRole();
    if (Role == NetRole::Client)
    {
        // Eigene Ausstoesse des Clients zeigten doppelte Baelle; die Host-Bilder ersetzen sie.
        if (DroppedClientSpawns.fetch_add(1) < 3)
            Core::Log::Info("XP-Baelle: lokaler Client-Ausstoss ausgelassen");
        return;
    }
    OriginalSpawn(Subsystem, In);
    if (Role != NetRole::Host || !In)
        return;
    const auto Value = FromRequest(*In);
    if (!ValidExpSpawn(Value))
        return;
    std::lock_guard Guard(Lock);
    if (Pending.size() >= QueueLimit)
        Pending.pop_front();
    Pending.push_back(Value);
}

void __fastcall ReceiveHook(uintptr_t Subsystem, int32_t Amount, uintptr_t Recipient)
{
    if (Patches::CurrentRole() != NetRole::Client)
        return OriginalReceive(Subsystem, Amount, Recipient);
    if (DroppedClientReceives.fetch_add(1) < 3)
        Core::Log::Info("XP-Baelle: Aufnahme beim Client nur angezeigt, nicht gezaehlt");
}

bool CheckSpawn()
{
    return Code::Expect(Patches::At(SpawnRequest), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10}, "ExpSubsystem.SpawnRequest");
}
bool CheckReceive()
{
    return Code::Expect(Patches::At(ReceiveExpImpl), {0x48, 0x89, 0x5C, 0x24, 0x18, 0x55, 0x56, 0x57, 0x48, 0x83, 0xEC, 0x30}, "ExpSubsystem.ReceiveExp");
}
bool ApplySpawn(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(Patches::At(SpawnRequest)), reinterpret_cast<void*>(&SpawnHook),
                                reinterpret_cast<void**>(&OriginalSpawn));
}
bool ApplyReceive(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(Patches::At(ReceiveExpImpl)), reinterpret_cast<void*>(&ReceiveHook),
                                reinterpret_cast<void**>(&OriginalReceive));
}
bool CheckBounds()
{
    return Code::Expect(Patches::At(UpdateBounds), {0x40, 0x53, 0x48, 0x83, 0xEC, 0x70, 0x48, 0x8B, 0xD9}, "ExpEmissionHandler.UpdateBounds") &&
           Code::Expect(Patches::At(LocalViewportCorners), {0x4C, 0x8B, 0xDC, 0x55, 0x53, 0x56, 0x57, 0x41, 0x56}, "Viewport.LocalCorners");
}
bool ApplyBounds(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(Patches::At(UpdateBounds)), reinterpret_cast<void*>(&BoundsHook),
                                reinterpret_cast<void**>(&OriginalBounds));
}
// Je Patch die benutzten Adressen der Signaturtabelle, erzeugt von tools/patch_signatures.py sites.
constexpr uintptr_t SpawnSites[] = {SpawnRequest};
constexpr uintptr_t ReceiveSites[] = {ReceiveExpImpl};
constexpr uintptr_t BoundsSites[] = {UpdateBounds, LocalViewportCorners};
const Patches::Patch List[] = {
    {"XP-Ausstoss beim Host aufzeichnen", CheckSpawn, ApplySpawn, SpawnSites},
    {"XP-Aufnahme beim Client nicht zaehlen", CheckReceive, ApplyReceive, ReceiveSites},
    {"XP-Sichtbereich aus lokaler Client-Kamera", CheckBounds, ApplyBounds, BoundsSites},
};
}

namespace Patches
{
std::span<const Patch> ExpPatches() { return List; }
}

bool ValidExpSpawn(const ExpSpawn& Value)
{
    if (Value.Amount <= 0 || Value.Amount > 1000000 || !std::isfinite(Value.Extra) || !std::isfinite(Value.Tail))
        return false;
    for (int i = 0; i < 3; ++i)
        if (!std::isfinite(Value.Start[i]) || !std::isfinite(Value.End[i]) || std::fabs(Value.Start[i]) > 1e7 || std::fabs(Value.End[i]) > 1e7)
            return false;
    for (const float Channel : Value.Color)
        if (!std::isfinite(Channel))
            return false;
    return true;
}

std::vector<ExpSpawn> TakeExpSpawns(size_t Limit)
{
    std::lock_guard Guard(Lock);
    const size_t Count = std::min(Limit, Pending.size());
    std::vector<ExpSpawn> Values(Pending.begin(), Pending.begin() + Count);
    Pending.erase(Pending.begin(), Pending.begin() + Count);
    return Values;
}

unsigned ReplayExpSpawns(std::span<const ExpSpawn> Values)
{
    if (Values.empty() || !OriginalSpawn || !Core::GameThread().IsDrainThread() ||
        Patches::CurrentState() != Patches::State::Active || Patches::CurrentRole() != NetRole::Client)
        return 0;
    static uintptr_t Subsystem = 0;
    if (!Subsystem || !Objects::IsA(Subsystem, "ExpSubsystem") ||
        (Memory::Read<uint32_t>(Subsystem + Offsets::UObject::Flags).value_or(0x30) & 0x30))
        Subsystem = Objects::FindInstance("ExpSubsystem");
    if (!Subsystem)
        return 0;
    // Beim ersten Bounds-Aufruf kann die Kamera noch fehlen. Bei neuen Bildern
    // erneut versuchen; alte Welt-/Template-Actors niemals benutzen.
    const uintptr_t Level = Pointer(Patches::CurrentWorld() + Offsets::UWorld::PersistentLevel);
    if (ClientHandler && Level && Pointer(ClientHandler + Offsets::UObject::Outer) == Level &&
        !(Memory::Read<uint32_t>(ClientHandler + Offsets::UObject::Flags).value_or(0x30) & 0x30) &&
        Objects::IsA(ClientHandler, "ExpEmissionHandler"))
        RepairClientBounds(ClientHandler);
    unsigned Replayed = 0;
    for (const auto& Value : Values)
    {
        if (!ValidExpSpawn(Value))
            continue;
        Request Out{Value.Amount, 0};
        std::memcpy(Out.Start, Value.Start, sizeof(Out.Start));
        std::memcpy(Out.End, Value.End, sizeof(Out.End));
        std::memcpy(Out.Color, Value.Color, sizeof(Out.Color));
        Out.Extra = Value.Extra;
        Out.Tail = Value.Tail;
        Out.Pad2 = 0;
        OriginalSpawn(Subsystem, &Out); // Direkt das Original: keine Sperre, keine erneute Aufzeichnung.
        ++Replayed;
    }
    return Replayed;
}
}
