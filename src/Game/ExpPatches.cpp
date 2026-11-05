// XP-Baelle sind lokale Niagara-Partikel des AExpEmissionHandler. Nur der Host erzeugt sie.
// Der Host zeichnet jede native Spawn-Anfrage auf, der Client spielt sie nur zur Anzeige nach.
// Beim Client zaehlt keine Aufnahme: ReceiveExp ist dort gesperrt, der XP-Stand kommt als Host-Snapshot.
#include "Game/PatchRuntime.h"
#include "Core/Dispatcher.h"
#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/ExpAdapter.h"
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
// Gemeinsamer Kern von RequestExpSpawning (0x58C1260) und den nativen Gegner-/Effektpfaden (0x58C6E35, 0x58C71DE,
// 0x58C7530, 0x59B1894): rcx = UExpSubsystem, rdx = Spawn-Anfrage.
constexpr uintptr_t SpawnRequest = 0x58C1030;
constexpr uintptr_t ReceiveExpImpl = 0x58BE8B0; // UExpSubsystem::ReceiveExp, hinter dem exec-Thunk 0x56EA930
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
std::mutex Lock;
std::deque<ExpSpawn> Pending;
std::atomic<unsigned> DroppedClientSpawns = 0, DroppedClientReceives = 0;

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
const Patches::Patch List[] = {
    {"XP-Ausstoss beim Host aufzeichnen", CheckSpawn, ApplySpawn},
    {"XP-Aufnahme beim Client nicht zaehlen", CheckReceive, ApplyReceive},
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
