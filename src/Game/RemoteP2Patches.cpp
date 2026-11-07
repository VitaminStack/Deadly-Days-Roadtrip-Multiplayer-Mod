// Host: den Remote-Spieler als P2 in die native Coop-Logik eintragen, ohne zweiten LocalPlayer. Aus der Spike-Probe
// research/spikes/a-connect/RemoteP2.cpp, Ablauf und Gegenproben in research/spikes/a-connect/RemoteP2.md.
//
// Ablauf je Frame beim Listen-Host mit genau einem lokalen und einem Remote-Spieler:
// 1. ActivePlayers[0/1] auf die aktuellen Controller setzen (natives TMap-Add, schwache Verweise).
// 2. Nur wenn AllowRemoteCoop() gerufen wurde: SetCoopEnabled(true) über ProcessEvent und einmal je Welt und Controller
//    die native Beitrittsmeldung OnPlayerJoined(1, Controller). Standard ist aus: Mit Koop-Modus hängen Lager und
//    Airdrop-Auswahl, bis airdrop-auswahl und inventar-ui sie übers Netz lösen.
// Fehlt der Remote-Spieler, entfernt die Mod nur ihre eigene P2-Bindung. Der Patch im Reconnect-Update nimmt die lokale
// P2-Gerätepflicht aus, solange diese Bindung vollständig ist.
#include <string>

#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"
#include "Game/RemoteP2Policy.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t MapAdd = 0x58D1940;
constexpr uintptr_t MapSparseRemove = 0x1344470;
constexpr uintptr_t GetController = 0x58EE0D0;
constexpr uintptr_t DelegateBroadcast = 0x24E9410;
constexpr uintptr_t DeviceCheckSite = 0x59072AA;
constexpr uintptr_t SoloLossTest = 0x59072AF;
constexpr uintptr_t NeedReconnect = 0x59072B4;

// ULocalCoopSubsystem
constexpr uintptr_t CoopOnPlayerJoined = 0x38;
constexpr uintptr_t CoopActivePlayers = 0x100;
constexpr uintptr_t CoopEnabled = 0x180;
constexpr uintptr_t CoopMaxLocalPlayers = 0x184;
constexpr uintptr_t CoopReconnectRequired = 0x189;
constexpr uintptr_t CoopRecreateP2 = 0x18A;
// UGameInstance, APlayerState, APlayerController
constexpr uintptr_t GameInstanceLocalPlayers = 0x38;
constexpr uintptr_t PlayerStateOwner = 0x158;
constexpr uintptr_t ControllerPlayer = 0x348;
constexpr uintptr_t ControllerNetConnection = 0x520;
constexpr uint32_t GarbageOrUnreachableFlags = 0x30;

using GetControllerFn = uintptr_t(__fastcall*)(uintptr_t, int32_t);
using MapAddFn = WeakController*(__fastcall*)(uintptr_t, const int32_t*, const WeakController*);
using BroadcastFn = void(__fastcall*)(uintptr_t, void*);

struct Roster
{
    uintptr_t World = 0, Coop = 0, Host = 0, Remote = 0;
};

struct Binding
{
    uintptr_t World = 0, Remote = 0;
    WeakController Slot{};
    uintptr_t JoinedWorld = 0;
    WeakController Joined{};
    uintptr_t CachedInstance = 0, CachedCoop = 0;
    bool Failed = false;
    bool CoopAllowed = false;
} Bound;

bool Alive(uintptr_t Object)
{
    return Object && !(Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachableFlags) & GarbageOrUnreachableFlags);
}

uintptr_t Pointer(uintptr_t Address)
{
    return Memory::ReadPointer(Address).value_or(0);
}

// Das Subsystem gehört zu dieser GameInstance. Die Suche im Objekt-Array läuft nur, wenn sich die GameInstance ändert.
uintptr_t CoopSubsystem(uintptr_t Instance)
{
    if (Instance != Bound.CachedInstance || !Alive(Bound.CachedCoop) || Pointer(Bound.CachedCoop + Offsets::UObject::Outer) != Instance)
    {
        Bound.CachedInstance = Instance;
        const uintptr_t Found = Objects::FindInstance("LocalCoopSubsystem");
        Bound.CachedCoop = Found && Pointer(Found + Offsets::UObject::Outer) == Instance ? Found : 0;
    }
    return Bound.CachedCoop;
}

bool WeakFor(uintptr_t Object, WeakController& Weak)
{
    Weak.Index = Memory::Read<int32_t>(Object + 0xC).value_or(0);
    if (Weak.Index <= 0 || Weak.Index >= Objects::Count() || Objects::At(Weak.Index) != Object)
        return false;
    const uintptr_t Chunks = Pointer(At(Offsets::GObjects));
    const uintptr_t Chunk = Chunks ? Pointer(Chunks + 8 * (Weak.Index / Offsets::ObjectArray::ElementsPerChunk)) : 0;
    if (!Chunk)
        return false;
    const uintptr_t Item = Chunk + (Weak.Index % Offsets::ObjectArray::ElementsPerChunk) * Offsets::ObjectArray::ItemSize;
    Weak.Serial = Memory::Read<int32_t>(Item + Offsets::ObjectArray::ItemSerialNumber).value_or(0);
    return Weak.Serial > 0;
}

// Nur der Listen-Host mit genau einem LocalPlayer und höchstens zwei aktuellen Controllern in dieser Welt. Während Reise
// und erster Replikation bleibt das Ergebnis leer, dann ändert die Mod nichts.
Roster CurrentRoster(bool WithCoop = true)
{
    Roster Result;
    Result.World = CurrentWorld();
    if (!Result.World || CurrentRole() != NetRole::Host)
        return {};
    const uintptr_t Instance = Pointer(Result.World + Offsets::UWorld::OwningGameInstance);
    if (!Instance || Memory::Read<int32_t>(Instance + GameInstanceLocalPlayers + 8).value_or(0) != 1)
        return {};
    const uintptr_t GameState = Pointer(Result.World + Offsets::UWorld::GameState);
    const uintptr_t States = GameState ? Pointer(GameState + Offsets::AGameStateBase::PlayerArray) : 0;
    const int32_t Count = GameState ? Memory::Read<int32_t>(GameState + Offsets::AGameStateBase::PlayerArray + 8).value_or(0) : 0;
    if (!States || Count < 1 || Count > 2)
        return {};
    const uintptr_t Level = Pointer(Result.World + Offsets::UWorld::PersistentLevel);
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t PlayerState = Pointer(States + 8 * i);
        const uintptr_t Controller = PlayerState ? Pointer(PlayerState + PlayerStateOwner) : 0;
        if (!Alive(Controller) || !IsA(Controller, "DDPlayerController") || Pointer(Controller + Offsets::UObject::Outer) != Level)
            return {};
        if (Pointer(Controller + ControllerNetConnection))
            Result.Remote = Controller;
        else if (IsA(Pointer(Controller + ControllerPlayer), "LocalPlayer"))
            Result.Host = Controller;
    }
    if (WithCoop)
        Result.Coop = CoopSubsystem(Instance);
    return Result;
}

// Aus dem Reconnect-Update des Spiels, also auf dem Game Thread.
bool __fastcall HasRemoteP2(uintptr_t Coop)
{
    if (Bound.Failed)
        return false;
    const Roster R = CurrentRoster();
    WeakController Remote{};
    const auto& Map = *reinterpret_cast<const ActivePlayersMap*>(Coop + CoopActivePlayers);
    const int32_t Slot = R.Coop == Coop && Map.Valid() ? Map.Find(1) : -1;
    return Slot >= 0 && R.Host && R.Remote && R.Host != R.Remote && WeakFor(R.Remote, Remote) &&
           SameRemoteNotification(0, Map.Data[Slot].Controller, 0, Remote);
}

int32_t ActivePlayerCount(uintptr_t Coop)
{
    int32_t Count = -1;
    if (const uintptr_t Function = Objects::FindObject("GetActivePlayerCount", "LocalCoopSubsystem"))
        Objects::ProcessEvent(Coop, Function, &Count);
    return Count;
}

void LogStatus(const char* Event, const Roster& R)
{
    const auto Get = reinterpret_cast<GetControllerFn>(At(GetController));
    Core::Log::Info("RemoteP2: {} coop={} max={} reconnect={} recreate={} p1={} p2={} aktiv={}", Event,
                    Memory::Read<uint8_t>(R.Coop + CoopEnabled).value_or(0), Memory::Read<int32_t>(R.Coop + CoopMaxLocalPlayers).value_or(-1),
                    Memory::Read<uint8_t>(R.Coop + CoopReconnectRequired).value_or(0), Memory::Read<uint8_t>(R.Coop + CoopRecreateP2).value_or(0),
                    R.Host && Get(R.Coop, 0) == R.Host, R.Remote && Get(R.Coop, 1) == R.Remote, ActivePlayerCount(R.Coop));
}

ActivePlayersMap& ActivePlayers(uintptr_t Coop)
{
    return *reinterpret_cast<ActivePlayersMap*>(Coop + CoopActivePlayers);
}

// Remote-Spieler fehlt: nur die eigene P2-Bindung entfernen. Kein RemoveLocalPlayer, kein künstliches Leave während einer Reise.
void Unbind(const Roster& R)
{
    if (!Bound.Slot.Index)
        return;
    ActivePlayersMap& Map = ActivePlayers(R.Coop);
    const int32_t Index = Map.Find(1);
    if (Index >= 0 && SameRemoteNotification(0, Map.Data[Index].Controller, 0, Bound.Slot) &&
        Map.Remove(Index, reinterpret_cast<ActivePlayersMap::RemoveSparseFn>(At(MapSparseRemove))))
        LogStatus("P2-Bindung entfernt, Remote-Spieler fehlt", R);
    Bound.Slot = {};
    Bound.Remote = 0;
}

void Bind(const Roster& R)
{
    WeakController Host{}, Remote{};
    if (!WeakFor(R.Host, Host) || !WeakFor(R.Remote, Remote))
        return;
    // Über die Map selbst, nicht über GetPlayerController: Die Abfrage kann vom Koop-Modus abhängen.
    const ActivePlayersMap& Map = ActivePlayers(R.Coop);
    const int32_t P1Slot = Map.Find(0), P2Slot = Map.Find(1);
    const bool Changed = Bound.World != R.World || Bound.Remote != R.Remote || P1Slot < 0 || P2Slot < 0 ||
                         !SameRemoteNotification(0, Map.Data[P1Slot].Controller, 0, Host) ||
                         !SameRemoteNotification(0, Map.Data[P2Slot].Controller, 0, Remote);
    if (Changed)
    {
        const auto Add = reinterpret_cast<MapAddFn>(At(MapAdd));
        const int32_t P1 = 0, P2 = 1;
        Add(R.Coop + CoopActivePlayers, &P1, &Host);
        Add(R.Coop + CoopActivePlayers, &P2, &Remote);
        if (!ActivePlayers(R.Coop).Valid())
        {
            Bound.Failed = true;
            Core::Log::Error("RemoteP2: ActivePlayers nach dem Eintragen ungültig, P2-Anbindung abgeschaltet");
            return;
        }
        Bound.Slot = Remote;
    }
    if (Bound.CoopAllowed && !Memory::Read<uint8_t>(R.Coop + CoopEnabled).value_or(1))
        if (const uintptr_t Function = Objects::FindObject("SetCoopEnabled", "LocalCoopSubsystem"))
        {
            bool On = true;
            Objects::ProcessEvent(R.Coop, Function, &On);
        }
    if (Changed)
    {
        Bound.World = R.World;
        Bound.Remote = R.Remote;
        LogStatus("P2 gebunden", R);
    }
    // Dieselbe Meldung wie beim lokalen Spawn eines P2, ohne dessen Spawn- und Gerätepfad. Sie zeigt die P2-Markierung.
    if (Bound.CoopAllowed && Memory::Read<uint8_t>(R.Coop + CoopEnabled).value_or(0) && !SameRemoteNotification(R.World, Remote, Bound.JoinedWorld, Bound.Joined))
    {
        struct
        {
            int32_t Index;
            int32_t Padding;
            uintptr_t Controller;
        } Params{1, 0, R.Remote};
        static_assert(sizeof(Params) == 0x10);
        reinterpret_cast<BroadcastFn>(At(DelegateBroadcast))(R.Coop + CoopOnPlayerJoined, &Params);
        Bound.JoinedWorld = R.World;
        Bound.Joined = Remote;
        Core::Log::Info("RemoteP2: Beitritt von P2 gemeldet");
    }
}

bool CheckRemoteP2()
{
    return Code::Expect(At(MapAdd), {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x18}, "TMap-Add der Coop-Spieler") &&
           Code::Expect(At(GetController), {0x48, 0x89, 0x5C, 0x24, 0x08}, "GetPlayerController des Coop-Subsystems") &&
           Code::Expect(At(MapSparseRemove), {0x45, 0x85, 0xC0}, "TSparseArray-RemoveAt der Coop-Spieler") &&
           Code::Expect(At(DelegateBroadcast), {0x4C, 0x8B, 0xDC, 0x53, 0x55, 0x56, 0x57, 0x41, 0x56}, "Broadcast der Beitrittsmeldung") &&
           Code::Expect(At(DeviceCheckSite), {0x48, 0x85, 0xDB, 0x74, 0x05, 0x40, 0x84, 0xED, 0x74, 0x04}, "P2-Geräteprüfung im Reconnect-Update");
}

bool ApplyRemoteP2(Code::Writer& Code)
{
    uint8_t* Cave = Code.Here();
    EmitRemoteDeviceGuard(Code, reinterpret_cast<uintptr_t>(&HasRemoteP2), At(SoloLossTest), At(NeedReconnect));
    return !Code.Failed() && Code::WriteJump(At(DeviceCheckSite), Cave);
}

// Je Patch die benutzten Adressen der Signaturtabelle, erzeugt von tools/patch_signatures.py sites.
constexpr uintptr_t RemoteP2Sites[] = {MapSparseRemove, DelegateBroadcast, MapAdd, GetController, DeviceCheckSite, SoloLossTest, NeedReconnect};
const Patch List[] = {
    {"Remote-Spieler als P2 ohne lokales Gerät", &CheckRemoteP2, &ApplyRemoteP2, RemoteP2Sites},
};
}

std::span<const Patch> RemoteP2Patches()
{
    return List;
}

void AllowRemoteCoop()
{
    if (!Bound.CoopAllowed)
        Core::Log::Info("RemoteP2: Koop-Modus für den Remote-Spieler erlaubt");
    Bound.CoopAllowed = true;
}

bool BoundRemoteP2(uintptr_t& Coop, uintptr_t& Host, uintptr_t& Remote)
{
    const Roster R = CurrentRoster();
    if (!R.Coop || !R.Host || !R.Remote || !Bound.Slot.Index)
        return false;
    WeakController Weak{};
    const ActivePlayersMap& Map = ActivePlayers(R.Coop);
    const int32_t Slot = Map.Valid() ? Map.Find(1) : -1;
    if (!WeakFor(R.Remote, Weak) || Slot < 0 || !SameRemoteNotification(0, Map.Data[Slot].Controller, 0, Weak))
        return false;
    Coop = R.Coop;
    Host = R.Host;
    Remote = R.Remote;
    return true;
}

bool RemoteP2Bound()
{
    uintptr_t Coop = 0, Host = 0, Remote = 0;
    return BoundRemoteP2(Coop, Host, Remote);
}

void TickRemoteP2(NetRole Role)
{
    if (Role != NetRole::Host || Bound.Failed)
        return;
    const Roster R = CurrentRoster();
    if (!R.Coop || !R.Host || R.Host == R.Remote || !ActivePlayers(R.Coop).Valid())
        return;
    if (R.Remote)
        Bind(R);
    else
        Unbind(R);
}
}
