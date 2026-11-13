// opt-schadenszahlen: Schadenszahlen beim Client. Beim Host ruft der Schadensweg je Treffer den nativen Handler des
// DDPlayerController (0x5A22DB0) auf. Er prüft lokale Steuerung, die Einstellung EnableDamageNumbers und das Ziel, merkt den Ort
// und holt nach kurzer Verzögerung einen BP_FloatingText aus dem Pool, der ShowDamageText zeigt. Beim Client läuft der
// Schadensweg nie, Gegner-HP repliziert dort nicht (0/0). Der Host merkt sich darum je Handler-Aufruf Ziel, Betrag und Flags
// (Hook, nur Host), Sync schickt sie als 0xA00, und der Client ruft denselben Handler an seinem eigenen Controller auf. Damit
// entscheiden beim Client seine eigene Einstellung, die nativen Filter, Ort, Timer und Pool. Schaden und HP bleiben beim Host.
// Befund: docs/GAME_ANALYSIS.md#schadenszahlen-beim-client-opt-schadenszahlen
#include <windows.h>

#include <array>
#include <atomic>
#include <cstring>
#include <unordered_map>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/CodeCave.h"
#include "Game/DamageNumberAdapter.h"
#include "Game/Memory.h"
#include "Game/NetworkObjects.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"

namespace Game
{
namespace
{
// Build 26495302.
constexpr uintptr_t DamageHandler = 0x5A22DB0;      // ADDPlayerController: Treffer -> Schadenszahl (rcx Controller, rdx FDamageInformation*)
constexpr uintptr_t DamageInfoConstruct = 0x56591B0; // FDamageInformation::FDamageInformation(), wie im Thunk von ShowDamageText
constexpr uintptr_t DamageInfoMapDestruct = 0x12BC260; // Elemente von TagMagnitudes freigeben, wie im Thunk
constexpr uintptr_t MemoryFree = 0x12F85E0;          // FMemory::Free, Hash von TagMagnitudes, wie im Thunk

// FDamageInformation (0xA0, SDK).
constexpr uintptr_t InfoDamage = 0x00;
constexpr uintptr_t InfoDamagedActor = 0x04;
constexpr uintptr_t InfoDamagedAvatar = 0x0C;
constexpr uintptr_t InfoFlags = 0x48;
constexpr uintptr_t InfoTagMap = 0x50;
constexpr uintptr_t InfoTagHash = 0x90;
constexpr uintptr_t InfoTagHashSize = 0x98;
constexpr size_t InfoSize = 0xA0;

constexpr uintptr_t ActorRootComponent = 0x1B8;
constexpr uintptr_t SceneRelativeLocation = 0x148;
constexpr uintptr_t ActorDestroyFlags = 0x5D; // Bit 1: bActorIsBeingDestroyed
constexpr uint32_t GarbageOrUnreachable = 0x30;
constexpr uintptr_t GameInstanceLocalPlayers = 0x38;
constexpr uintptr_t PlayerController = 0x30;

using HandlerFn = void(__fastcall*)(uintptr_t Controller, const void* Info);
using ConstructFn = void(__fastcall*)(void* Info);
using DestructFn = void(__fastcall*)(void* Map);
using FreeFn = void(__fastcall*)(void* Memory);
HandlerFn OriginalHandler = nullptr;
std::atomic<bool> Installed = false;

struct Pending
{
    uintptr_t Object = 0;
    int32_t Index = -1;
    int32_t Serial = 0;
    float Damage = 0;
    uint8_t Flags = 0;
    float X = 0, Y = 0, Z = 0;
};

// Nur Game Thread: Der Handler läuft im Schadensweg der Engine auf dem Game Thread, Take aus dem Sync-Takt ebenso.
constexpr size_t RingSize = 128;
std::array<Pending, RingSize> Ring{};
size_t RingCount = 0;
uintptr_t RecordWorld = 0;

// Zähler für den Spieltest, alle 10 Sekunden ins Log, wenn sich etwas geändert hat.
struct Counters
{
    uint64_t Calls = 0, Recorded = 0, Overflow = 0, NoGuid = 0, Taken = 0, Sent = 0, Remote = 0, SameTick = 0; // Host
    uint64_t SendFailed = 0, NoPeer = 0, EncodeFailed = 0, NoTarget = 0; // Host
    uint64_t Received = 0, Shown = 0, Unknown = 0, Dying = 0, NoController = 0; // Client
    uint64_t Bundled = 0, DecodeFailed = 0, RevisionGap = 0, WrongRole = 0; // Client
    bool operator==(const Counters&) const = default;
} Count, Logged;
ULONGLONG NextLog = 0;

void LogCounters(bool Host)
{
    const ULONGLONG Now = GetTickCount64();
    if (Now < NextLog || Count == Logged)
        return;
    NextLog = Now + 10000;
    Logged = Count;
    if (Host)
        Core::Log::Info("Schadenszahlen Host: Handler {} (fremder Controller {}, ohne Ziel {}), gemerkt {} (gleiches Ziel und Schaden "
                        "im Takt {}), übergeben {}, gesendet {}, Senden abgelehnt {}, ohne Mitspieler {}, Paket ungültig {}, "
                        "ohne NetGUID {}, Puffer voll {}", Count.Calls, Count.Remote, Count.NoTarget, Count.Recorded, Count.SameTick,
                        Count.Taken, Count.Sent, Count.SendFailed, Count.NoPeer, Count.EncodeFailed, Count.NoGuid, Count.Overflow);
    else
        Core::Log::Info("Schadenszahlen Client: empfangen {} (davon im Sammelabruf {}), an Handler {}, Ziel unbekannt {}, "
                        "Ziel stirbt {}, ohne Controller {}, falsche Rolle {}, Nachricht unlesbar {}, Nachrichten fehlen {}",
                        Count.Received, Count.Bundled, Count.Shown, Count.Unknown, Count.Dying, Count.NoController, Count.WrongRole,
                        Count.DecodeFailed, Count.RevisionGap);
}

uintptr_t ObjectItem(int32_t Index)
{
    if (Index <= 0 || Index >= Objects::Count())
        return 0;
    const auto Chunks = Memory::ReadPointer(Memory::CachedRva<Offsets::GObjects>());
    const auto Chunk = Chunks ? Memory::ReadPointer(*Chunks + 8 * (Index / Offsets::ObjectArray::ElementsPerChunk)) : std::nullopt;
    return Chunk ? *Chunk + (Index % Offsets::ObjectArray::ElementsPerChunk) * Offsets::ObjectArray::ItemSize : 0;
}

// Objekt zu einem schwachen Zeiger (Index, Seriennummer), 0 wenn er nicht mehr gilt.
uintptr_t ResolveWeak(int32_t Index, int32_t Serial)
{
    const uintptr_t Item = ObjectItem(Index);
    if (!Item || !Serial || Memory::Read<int32_t>(Item + Offsets::ObjectArray::ItemSerialNumber).value_or(0) != Serial)
        return 0;
    return Memory::ReadPointer(Item + Offsets::ObjectArray::ItemObject).value_or(0);
}

// Der Platz im Objekt-Array beweist, dass das Objekt noch lebt; freigegebener Speicher wird nie benutzt.
bool Usable(uintptr_t Object, int32_t& Index, int32_t& Serial)
{
    Index = Memory::Read<int32_t>(Object + Offsets::UObject::InternalIndex).value_or(-1);
    const uintptr_t Item = ObjectItem(Index);
    if (!Item || Memory::ReadPointer(Item + Offsets::ObjectArray::ItemObject).value_or(0) != Object)
        return false;
    Serial = Memory::Read<int32_t>(Item + Offsets::ObjectArray::ItemSerialNumber).value_or(0);
    if (!Serial || (Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachable) & GarbageOrUnreachable))
        return false;
    return !(Memory::Read<uint8_t>(Object + ActorDestroyFlags).value_or(2) & 2);
}

void Record(const uint8_t* Info)
{
    ++Count.Calls;
    const uintptr_t World = Patches::CurrentWorld();
    if (World != RecordWorld)
    {
        RingCount = 0;
        RecordWorld = World;
    }
    const auto Weak = Memory::Read<std::array<int32_t, 2>>(reinterpret_cast<uintptr_t>(Info) + InfoDamagedActor);
    const uintptr_t Object = Weak ? ResolveWeak((*Weak)[0], (*Weak)[1]) : 0;
    if (!Object)
    {
        ++Count.NoTarget;
        return;
    }
    if (RingCount == RingSize)
    {
        ++Count.Overflow;
        return;
    }
    Pending Entry{Object, (*Weak)[0], (*Weak)[1]};
    Entry.Damage = Memory::Read<float>(reinterpret_cast<uintptr_t>(Info) + InfoDamage).value_or(0.f);
    Entry.Flags = Memory::Read<uint8_t>(reinterpret_cast<uintptr_t>(Info) + InfoFlags).value_or(0);
    const uintptr_t Root = Memory::ReadPointer(Object + ActorRootComponent).value_or(0);
    if (const auto Location = Root ? Memory::Read<std::array<double, 3>>(Root + SceneRelativeLocation) : std::nullopt)
    {
        Entry.X = static_cast<float>((*Location)[0]);
        Entry.Y = static_cast<float>((*Location)[1]);
        Entry.Z = static_cast<float>((*Location)[2]);
    }
    // Diagnose: Läuft der Handler für einen Treffer zweimal (zwei Aufrufstellen im Schadenspfad), sähe der Client ihn doppelt.
    for (size_t i = 0; i < RingCount; ++i)
        if (Ring[i].Object == Entry.Object && Ring[i].Damage == Entry.Damage)
        {
            ++Count.SameTick;
            break;
        }
    Ring[RingCount++] = Entry;
    ++Count.Recorded;
}

uintptr_t LocalController();

// Original zuerst: Zahl und Ort des Hosts bleiben unverändert. Nur der Host merkt; der Aufruf des Clients selbst (ShowOne)
// kommt hier auch vorbei und wird darum nie gemerkt.
void __fastcall DamageHandlerHook(uintptr_t Controller, const uint8_t* Info)
{
    OriginalHandler(Controller, Info);
    if (!Info || Patches::CurrentRole() != NetRole::Host)
        return;
    if (Controller != LocalController())
        ++Count.Remote;
    Record(Info);
}

uintptr_t LocalController()
{
    const auto Pointer = [](uintptr_t Address) { return Address ? Memory::ReadPointer(Address).value_or(0) : 0; };
    const uintptr_t World = Patches::CurrentWorld();
    const uintptr_t Instance = Pointer(World ? World + Offsets::UWorld::OwningGameInstance : 0);
    const uintptr_t Players = Pointer(Instance ? Instance + GameInstanceLocalPlayers : 0);
    const uintptr_t Local = Pointer(Players);
    const uintptr_t Controller = Pointer(Local ? Local + PlayerController : 0);
    return Controller && Patches::IsA(Controller, "DDPlayerController") ? Controller : 0;
}

// Wie der native Thunk von ShowDamageText: Struct mit dem Konstruktor des Spiels anlegen, danach TagMagnitudes freigeben.
void ShowOne(uintptr_t Controller, int32_t Index, int32_t Serial, const DamageNumberHit& Hit)
{
    alignas(16) std::array<uint8_t, InfoSize + 0x10> Buffer{};
    uint8_t* Info = Buffer.data();
    reinterpret_cast<ConstructFn>(Patches::At(DamageInfoConstruct))(Info);
    const int32_t Weak[2] = {Index, Serial};
    std::memcpy(Info + InfoDamage, &Hit.Damage, sizeof(float));
    std::memcpy(Info + InfoDamagedActor, Weak, sizeof(Weak));
    std::memcpy(Info + InfoDamagedAvatar, Weak, sizeof(Weak));
    Info[InfoFlags] = Hit.Flags;
    OriginalHandler(Controller, Info);
    *reinterpret_cast<int32_t*>(Info + InfoTagHashSize) = 0;
    if (void* Hash = *reinterpret_cast<void**>(Info + InfoTagHash))
        reinterpret_cast<FreeFn>(Patches::At(MemoryFree))(Hash);
    reinterpret_cast<DestructFn>(Patches::At(DamageInfoMapDestruct))(Info + InfoTagMap);
    ++Count.Shown;
}
}

std::vector<DamageNumberHit> TakeDamageNumberHits()
{
    std::vector<DamageNumberHit> Hits;
    if (!Installed.load(std::memory_order_relaxed) || Patches::CurrentRole() != NetRole::Host)
    {
        RingCount = 0;
        return Hits;
    }
    if (Patches::CurrentWorld() != RecordWorld)
        RingCount = 0;
    if (RingCount)
    {
        // Die NetGUID-Tabelle einmal je Takt lesen, nie im Hook.
        std::unordered_map<uintptr_t, uint64_t> Guids;
        if (const auto Entries = ReadNetworkObjects())
            for (const auto& Entry : *Entries)
                Guids.emplace(Entry.Object, Entry.Guid);
        for (size_t i = 0; i < RingCount; ++i)
        {
            const Pending& Entry = Ring[i];
            const auto Guid = ResolveWeak(Entry.Index, Entry.Serial) == Entry.Object ? Guids.find(Entry.Object) : Guids.end();
            if (Guid == Guids.end())
            {
                ++Count.NoGuid;
                continue;
            }
            if (Hits.size() == MaxDamageNumberHits)
            {
                ++Count.Overflow;
                continue;
            }
            Hits.push_back({Guid->second, Entry.Damage, Entry.Flags, Entry.X, Entry.Y, Entry.Z});
        }
        RingCount = 0;
        Count.Taken += Hits.size();
    }
    LogCounters(true);
    return Hits;
}

void NoteDamageNumbers(DamageNumberNote Note, size_t Amount)
{
    switch (Note)
    {
    case DamageNumberNote::Sent: Count.Sent += Amount; break;
    case DamageNumberNote::SendFailed: Count.SendFailed += Amount; break;
    case DamageNumberNote::NoPeer: Count.NoPeer += Amount; break;
    case DamageNumberNote::EncodeFailed: Count.EncodeFailed += Amount; break;
    case DamageNumberNote::DecodeFailed: Count.DecodeFailed += Amount; break;
    case DamageNumberNote::RevisionGap: Count.RevisionGap += Amount; break;
    }
}

void ShowDamageNumbers(std::span<const DamageNumberHit> Hits, bool Bundled)
{
    if (!Installed.load(std::memory_order_relaxed))
        return;
    if (Patches::CurrentRole() != NetRole::Client)
    {
        Count.WrongRole += Hits.size();
        LogCounters(false);
        return;
    }
    Count.Received += Hits.size();
    if (Bundled)
        Count.Bundled += Hits.size();
    const uintptr_t Controller = Hits.empty() ? 0 : LocalController();
    if (!Hits.empty() && !Controller)
        Count.NoController += Hits.size();
    if (Controller)
    {
        std::unordered_map<uint64_t, uintptr_t> Objects;
        if (const auto Entries = ReadNetworkObjects())
            for (const auto& Entry : *Entries)
                Objects.emplace(Entry.Guid, Entry.Object);
        for (const auto& Hit : Hits)
        {
            const auto Found = Objects.find(Hit.Guid);
            if (Found == Objects.end())
            {
                ++Count.Unknown;
                continue;
            }
            int32_t Index = -1, Serial = 0;
            if (!Usable(Found->second, Index, Serial))
            {
                ++Count.Dying;
                continue;
            }
            ShowOne(Controller, Index, Serial, Hit);
        }
    }
    LogCounters(false);
}

namespace Patches
{
namespace
{
bool CheckDamageNumbers()
{
    return Code::Expect(At(DamageHandler), {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x18, 0x55, 0x56, 0x57}, "Schadenszahl-Handler") &&
           Code::Expect(At(DamageInfoConstruct), {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x4C, 0x24, 0x08},
                        "FDamageInformation-Konstruktor") &&
           Code::Expect(At(DamageInfoMapDestruct), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x30},
                        "TagMagnitudes freigeben") &&
           Code::Expect(At(MemoryFree), {0x48, 0x85, 0xC9, 0x74, 0x2E, 0x53, 0x48, 0x83, 0xEC, 0x20}, "FMemory::Free");
}

bool ApplyDamageNumbers(Code::Writer&)
{
    const bool Ok = Core::Hooks::Install(reinterpret_cast<void*>(At(DamageHandler)), reinterpret_cast<void*>(&DamageHandlerHook),
                                         reinterpret_cast<void**>(&OriginalHandler));
    Installed.store(Ok, std::memory_order_relaxed);
    return Ok;
}

// Je Patch die benutzten Adressen der Signaturtabelle, erzeugt von tools/patch_signatures.py sites.
constexpr uintptr_t DamageNumberSites[] = {DamageHandler, DamageInfoConstruct, DamageInfoMapDestruct, MemoryFree};
const Patch List[] = {
    {"Schadenszahlen beim Client", &CheckDamageNumbers, &ApplyDamageNumbers, DamageNumberSites},
};
}

std::span<const Patch> DamageNumberPatches()
{
    return List;
}
}
}
