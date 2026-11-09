// Opt-in Phase-1 probe. Runs only on the game thread; never creates a LocalPlayer.
#include "Spike.h"
#include "RemoteP2Policy.h"

namespace Spike
{
namespace
{
constexpr uintptr_t kActivePlayers = 0x100;
constexpr uintptr_t kCoopEnabled = 0x180;
constexpr uintptr_t kLocalPlayers = 0x38;
constexpr uintptr_t kGameInstance = 0x228;
const uintptr_t kMapAdd = Rva(0x58D1A80);
const uintptr_t kMapSparseRemove = Rva(0x1344650);
const uintptr_t kGetController = Rva(0x58EFFD0);
bool Enabled = false;
bool FunctionsReady = false;
uintptr_t LastWorld = 0, LastRemote = 0;
uintptr_t LastJoinedWorld = 0;
WeakController LastJoinedController{};
WeakController LastBoundController{};

bool IsA(uintptr_t Object, const char* Name)
{
    if (!Object || (Read<uint32_t>(Object + 8) & 0x30))
        return false;
    uintptr_t Class = Read<uintptr_t>(Object + 0x10);
    for (int Depth = 0; Class && Depth < 64; ++Depth, Class = Read<uintptr_t>(Class + 0x40))
        if (NameOf(Class) == Name)
            return true;
    return false;
}

bool WeakFor(uintptr_t Object, WeakController& Weak)
{
    Weak.Index = Read<int32_t>(Object + 0xC);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    if (Weak.Index <= 0 || Weak.Index >= Count)
        return false;
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const uintptr_t Chunk = Chunks ? Read<uintptr_t>(Chunks + 8 * (Weak.Index >> 16)) : 0;
    const uintptr_t Item = Chunk ? Chunk + (Weak.Index & 0xFFFF) * 0x18 : 0;
    if (!Item || Read<uintptr_t>(Item + 8) != Object)
        return false;
    Weak.Serial = Read<int32_t>(Item + 0x10);
    return Weak.Serial > 0;
}

struct Roster
{
    uintptr_t World = 0, Coop = 0, Host = 0, Remote = 0;
};

Roster CurrentRoster()
{
    Roster Result;
    Result.World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Driver = Result.World ? Read<uintptr_t>(Result.World + kWorldNetDriver) : 0;
    if (!Driver || Read<uintptr_t>(Driver + kDriverServerConnection))
        return Result;
    const uintptr_t Instance = Read<uintptr_t>(Result.World + kGameInstance);
    if (!Instance || Read<int32_t>(Instance + kLocalPlayers + 8) != 1)
        return Result;
    const uintptr_t State = Read<uintptr_t>(Result.World + kWorldGameState);
    const uintptr_t States = State ? Read<uintptr_t>(State + kGameStatePlayerArray) : 0;
    const int32_t StateCount = State ? Read<int32_t>(State + kGameStatePlayerArray + 8) : 0;
    if (!States || StateCount < 1 || StateCount > 2)
        return Result;
    for (int32_t i = 0; i < StateCount; ++i)
    {
        const uintptr_t PlayerState = Read<uintptr_t>(States + 8 * i);
        const uintptr_t Controller = PlayerState ? Read<uintptr_t>(PlayerState + 0x158) : 0;
        if (!IsA(Controller, "DDPlayerController") ||
            Read<uintptr_t>(Controller + 0x20) != Read<uintptr_t>(Result.World + 0x30))
            return {};
        if (Read<uintptr_t>(Controller + 0x520))
            Result.Remote = Controller;
        else if (IsA(Read<uintptr_t>(Controller + 0x348), "LocalPlayer"))
            Result.Host = Controller;
    }
    // The subsystem belongs to this GameInstance, not an old world or another test instance.
    const uintptr_t Coop = FindInstance("LocalCoopSubsystem");
    if (Coop && Read<uintptr_t>(Coop + 0x20) == Instance)
        Result.Coop = Coop;
    return Result;
}

bool __fastcall HasRemoteP2Device(uintptr_t Coop)
{
    if (!Enabled || !FunctionsReady)
        return false;
    const auto R = CurrentRoster();
    return R.Coop == Coop && R.Host && R.Remote && R.Host != R.Remote;
}

bool PatchRemoteDeviceCheck()
{
    static bool Installed = false;
    if (Installed)
        return true;
    const uintptr_t Site = Base + Rva(0x590B2AA);
    if (!Expect(Site, {0x48, 0x85, 0xDB, 0x74, 0x05, 0x40, 0x84, 0xED, 0x74, 0x04}, "RemoteP2 missing device branch"))
        return false;
    struct Writer : CodeWriter
    {
        void EmitPointer(uintptr_t Pointer)
        {
            for (size_t i = 0; i < sizeof(Pointer); ++i)
                Emit({static_cast<uint8_t>(Pointer >> (i * 8))});
        }
    } Code;
    if (!Code.Allocate())
        return false;
    uint8_t* Start = Code.Here();
    EmitRemoteDeviceGuard(Code, reinterpret_cast<uintptr_t>(&HasRemoteP2Device), Base + Rva(0x590B2AF), Base + Rva(0x590B2B4));
    WriteJump(Site, Start);
    Installed = true;
    return true;
}

void Status()
{
    const auto R = CurrentRoster();
    if (!R.Coop)
    {
        Log("RemoteP2: kein vollstaendiger Host-Roster (1 LocalPlayer, 2 PlayerStates erforderlich)");
        return;
    }
    using GetFn = uintptr_t(__fastcall*)(uintptr_t, int32_t);
    const auto Get = reinterpret_cast<GetFn>(Base + kGetController);
    int32_t ActiveCount = -1;
    const uintptr_t CountFunction = FindObject("GetActivePlayerCount", "LocalCoopSubsystem");
    if (CountFunction)
        Read<ProcessEventFn*>(R.Coop)[kProcessEventIndex](R.Coop, CountFunction, &ActiveCount);
    Log("RemoteP2: enabled=" + std::to_string(Enabled) + " coop=" + std::to_string(Read<uint8_t>(R.Coop + kCoopEnabled)) +
        " max=" + std::to_string(Read<int32_t>(R.Coop + 0x184)) +
        " reconnect=" + std::to_string(Read<uint8_t>(R.Coop + 0x189)) +
        " recreate=" + std::to_string(Read<uint8_t>(R.Coop + 0x18A)) +
        " p1=" + std::to_string(R.Host && Get(R.Coop, 0) == R.Host) + " p2=" + std::to_string(R.Remote && Get(R.Coop, 1) == R.Remote) +
        " active=" + std::to_string(ActiveCount));
}
}

void TickRemoteP2()
{
    if (ModOwnsPatches || !Enabled || !FunctionsReady)
        return;
    const auto R = CurrentRoster();
    if (!R.Coop || !R.Host || R.Host == R.Remote)
        return;
    const uintptr_t Map = R.Coop + kActivePlayers;
    // Bounded native map layout. Native Add replaces existing keys and owns allocation/hash bookkeeping.
    if (!reinterpret_cast<const ActivePlayersMap*>(Map)->Valid())
        return;
    if (!R.Remote)
    {
        auto& Active = *reinterpret_cast<ActivePlayersMap*>(Map);
        for (int32_t i = 0; i < Active.Num; ++i)
            if (Active.Allocated(i) && Active.Data[i].Key == 1 &&
                SameRemoteNotification(0, Active.Data[i].Controller, 0, LastBoundController))
            {
                if (Active.Remove(i, reinterpret_cast<ActivePlayersMap::RemoveSparseFn>(Base + kMapSparseRemove)))
                {
                    LastRemote = 0;
                    LastBoundController = {};
                    Log("RemoteP2: eigene veraltete P2-Bindung entfernt (Roster ohne Remote-Spieler)");
                }
                break;
            }
        return;
    }
    WeakController Host{}, Remote{};
    if (!WeakFor(R.Host, Host) || !WeakFor(R.Remote, Remote))
        return;
    using GetFn = uintptr_t(__fastcall*)(uintptr_t, int32_t);
    const auto Get = reinterpret_cast<GetFn>(Base + kGetController);
    const bool Changed = LastWorld != R.World || LastRemote != R.Remote ||
        Get(R.Coop, 0) != R.Host || Get(R.Coop, 1) != R.Remote;
    if (Changed)
    {
        using AddFn = WeakController*(__fastcall*)(uintptr_t, const int32_t*, const WeakController*);
        const auto Add = reinterpret_cast<AddFn>(Base + kMapAdd);
        const int32_t P1 = 0, P2 = 1;
        Add(Map, &P1, &Host);
        Add(Map, &P2, &Remote);
        if (!reinterpret_cast<const ActivePlayersMap*>(Map)->Valid())
        {
            Enabled = false;
            Log("RemoteP2: native ActivePlayers update failed validation; probe stopped");
            return;
        }
        LastBoundController = Remote;
    }
    if (!Read<uint8_t>(R.Coop + kCoopEnabled))
    {
        const uintptr_t Function = FindObject("SetCoopEnabled", "LocalCoopSubsystem");
        if (!Function)
            return;
        bool On = true;
        Read<ProcessEventFn*>(R.Coop)[kProcessEventIndex](R.Coop, Function, &On);
    }
    if (Changed)
    {
        LastWorld = R.World;
        LastRemote = R.Remote;
        Status();
    }
}

bool RemoteP2Command(const std::wstring& Command)
{
    if (Command != L"!p2 status" && Command != L"!p2 on" && Command != L"!p2 joined")
        return false;
    // Mit der Mod bindet sie selbst P2 und meldet den Beitritt (src/Game/RemoteP2Patches.cpp), die Probe bliebe ein zweiter Schreiber.
    if (ModOwnsPatches)
    {
        Log("RemoteP2: uebernimmt die Mod");
        return true;
    }
    if (Command == L"!p2 on")
    {
        const auto R = CurrentRoster();
        if (!R.Coop || !R.Host || !R.Remote)
        {
            Log("RemoteP2: on abgelehnt; nur vollstaendiger Listen-Host mit einem lokalen und einem Remote-Spieler");
            return true;
        }
        FunctionsReady = Expect(Base + kMapAdd, {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x18}, "RemoteP2 map add") &&
            Expect(Base + kGetController, {0x48, 0x89, 0x5C, 0x24, 0x08}, "RemoteP2 get controller") &&
            Expect(Base + kMapSparseRemove, {0x45, 0x85, 0xC0}, "RemoteP2 20-byte sparse remove") &&
            Expect(Base + Rva(0x24E95F0), {0x4C, 0x8B, 0xDC, 0x53, 0x55, 0x56, 0x57, 0x41, 0x56}, "RemoteP2 script delegate broadcast") &&
            PatchRemoteDeviceCheck();
        Enabled = FunctionsReady;
        TickRemoteP2();
    }
    else if (Command == L"!p2 joined" && Enabled && FunctionsReady)
    {
        const auto R = CurrentRoster();
        WeakController Weak{};
        if (R.Coop && R.Remote && R.Host && Read<uint8_t>(R.Coop + kCoopEnabled) && WeakFor(R.Remote, Weak) &&
            reinterpret_cast<const ActivePlayersMap*>(R.Coop + kActivePlayers)->Valid() &&
            !SameRemoteNotification(R.World, Weak, LastJoinedWorld, LastJoinedController))
        {
            // Same signature and delegate as SpawnLocalPlayer's native join broadcast, without its local spawn.
            struct { int32_t Index; int32_t Padding; uintptr_t Controller; } Params{1, 0, R.Remote};
            static_assert(sizeof(Params) == 0x10);
            using BroadcastFn = void(__fastcall*)(uintptr_t, void*);
            reinterpret_cast<BroadcastFn>(Base + Rva(0x24E95F0))(R.Coop + 0x38, &Params);
            LastJoinedWorld = R.World;
            LastJoinedController = Weak;
            Log("RemoteP2: explicit native OnPlayerJoined broadcast");
        }
    }
    Status();
    return true;
}
}
