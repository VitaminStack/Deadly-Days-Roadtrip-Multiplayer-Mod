// Host: Airdrop-Auswahl für beide Spieler über die native Koop-Reward-Sitzung, siehe src/Game/README.md#airdrop-auswahl.
//
// 1. Beim Öffnen einer Kiste entscheidet UCoopInventoryCoordinatorComponent (0x596EB30) über bCoopEnabled des
//    Coop-Subsystems: gesetzt, startet eine Sitzung mit einem Claim je Spieler und Pause bis zum letzten Claim, sonst
//    die Solo-Auswahl. Die Mod setzt das Byte nur für diese Sitzung, ohne SetCoopEnabled, damit das Lager am Tagesende
//    wie ohne Koop-Modus läuft.
// 2. Bestätigt der Host zuerst, übergäbe das Panel die UI-Autorität an P2 und wartete auf dessen lokale Eingabe. Die Mod
//    hält das Bestätigen deshalb zurück, bis der Client gewählt hat, und holt es dann nach. Der letzte Claim läuft so
//    immer über das native Panel, das danach schließt.
// 3. Die Wahl des Clients landet nativ im Inventar des Hosts; die Mod legt sie nach der Sitzung in das Inventar des
//    Clients.
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <string>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"
#include "Game/RewardAdapter.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t BeginCoopReward = 0x596EB30;
constexpr uintptr_t PanelConfirm = 0x5A6CB20;
constexpr uintptr_t MemoryFree = 0x12F8400;

// ULocalCoopSubsystem
constexpr uintptr_t CoopEnabled = 0x180;
// UCoopInventoryCoordinatorComponent, RewardSession ab 0x170
constexpr uintptr_t Session = 0x170;
constexpr uintptr_t SessionChoices = Session + 0x08;
constexpr uintptr_t SessionEligible = Session + 0x68;
constexpr uintptr_t SessionClaimed = Session + 0xC8;
constexpr uintptr_t SessionLevel = Session + 0xE0;
constexpr uintptr_t SessionState = Session + 0xF0;
constexpr uintptr_t QueuedRewards = 0x268;
constexpr uint8_t AwaitingClaims = 2, Completed = 3;
// UInventoryItemInstance, UInventoryItemData, URarity, UInventoryComponent, APlayerState
constexpr uintptr_t ItemData = 0xE0;
constexpr uintptr_t ItemBroken = 0x110;
constexpr uintptr_t ItemRarity = 0x118;
constexpr uintptr_t DataName = 0x40;
constexpr uintptr_t RarityName = 0x28;
constexpr uintptr_t InventoryPlayerState = 0x228;
constexpr uintptr_t PlayerStateOwner = 0x158;
constexpr uint32_t GarbageOrUnreachableFlags = 0x30;

using BeginFn = bool(__fastcall*)(uintptr_t, uintptr_t, uintptr_t);
using ConfirmFn = void(__fastcall*)(uintptr_t);
using FreeFn = void(__fastcall*)(void*);
BeginFn OriginalBegin = nullptr;
ConfirmFn OriginalConfirm = nullptr;

std::atomic<bool> RemoteReady = false;

struct
{
    uintptr_t Coordinator = 0, Coop = 0, Chest = 0;
    bool ForcedCoop = false;
    uint64_t Session = 0;
    uintptr_t HeldPanel = 0;
    uint64_t HeldSession = 0;
    uintptr_t ClientChoice = 0;
    uint64_t ClientSession = 0;
    ULONGLONG CompletedSince = 0;
    bool Acknowledged = false;
    std::vector<std::pair<uintptr_t, std::string>> Names;
} Reward;

bool Alive(uintptr_t Object)
{
    return Object && !(Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachableFlags) & GarbageOrUnreachableFlags);
}

uintptr_t Pointer(uintptr_t Address)
{
    return Memory::ReadPointer(Address).value_or(0);
}

std::vector<int32_t> Ints(uintptr_t Array)
{
    std::vector<int32_t> Result;
    const uintptr_t Data = Pointer(Array);
    const int32_t Count = Memory::Read<int32_t>(Array + 8).value_or(0);
    for (int32_t i = 0; Data && i < std::min(Count, 8); ++i)
        Result.push_back(Memory::Read<int32_t>(Data + 4 * i).value_or(-1));
    return Result;
}

std::vector<uintptr_t> Choices(uintptr_t Coordinator)
{
    std::vector<uintptr_t> Result;
    const uintptr_t Data = Pointer(Coordinator + SessionChoices);
    const int32_t Count = Memory::Read<int32_t>(Coordinator + SessionChoices + 8).value_or(0);
    for (int32_t i = 0; Data && i < std::min(Count, 8); ++i)
        Result.push_back(Pointer(Data + 8 * i));
    return Result;
}

bool Contains(const std::vector<int32_t>& Values, int32_t Value)
{
    return std::find(Values.begin(), Values.end(), Value) != Values.end();
}

std::string Join(const std::vector<int32_t>& Values)
{
    std::string Text;
    for (const int32_t Value : Values)
        Text += (Text.empty() ? "" : ",") + std::to_string(Value);
    return "[" + Text + "]";
}

uint8_t QueueState(uintptr_t Coordinator)
{
    return Memory::Read<uint8_t>(Coordinator + SessionState).value_or(0);
}

bool Current(uintptr_t Coordinator)
{
    return Alive(Coordinator) && IsA(Coordinator, "CoopInventoryCoordinatorComponent");
}

uint64_t Revision(const std::vector<uintptr_t>& Items)
{
    uint64_t Hash = 1469598103934665603ull;
    for (const uintptr_t Item : Items)
        Hash = (Hash ^ Item) * 1099511628211ull;
    return Hash ? Hash : 1;
}

uintptr_t Function(uintptr_t& Cache, const char* Name, const char* Outer)
{
    if (!Cache)
        Cache = Objects::FindObject(Name, Outer);
    return Cache;
}

// FText über KismetTextLibrary.Conv_TextToString. Das FString-Ergebnis gibt FMemory::Free frei.
std::string TextAt(uintptr_t Address)
{
    static uintptr_t Library = 0, Convert = 0;
    if (!Library)
        Library = Objects::FindNamed("Default__KismetTextLibrary");
    if (!Library || !Function(Convert, "Conv_TextToString", "KismetTextLibrary"))
        return {};
    struct
    {
        uint8_t Text[0x10];
        wchar_t* Data;
        int32_t Count, Max;
    } Params{};
    static_assert(sizeof(Params) == 0x20);
    if (!Memory::ReadBytes(Address, Params.Text, sizeof(Params.Text)))
        return {};
    Objects::ProcessEvent(Library, Convert, &Params);
    std::string Result;
    if (Params.Data && Params.Count > 1 && Params.Count < 256)
    {
        const int Size = WideCharToMultiByte(CP_UTF8, 0, Params.Data, Params.Count - 1, nullptr, 0, nullptr, nullptr);
        Result.resize(static_cast<size_t>(std::max(Size, 0)));
        WideCharToMultiByte(CP_UTF8, 0, Params.Data, Params.Count - 1, Result.data(), Size, nullptr, nullptr);
    }
    if (Params.Data)
        reinterpret_cast<FreeFn>(At(MemoryFree))(Params.Data);
    return Result;
}

std::string ItemName(uintptr_t Item)
{
    for (const auto& [Known, Name] : Reward.Names)
        if (Known == Item)
            return Name;
    const uintptr_t Data = Pointer(Item + ItemData);
    std::string Name = Data ? TextAt(Data + DataName) : std::string{};
    if (Name.empty())
        Name = Data ? Memory::NameOf(Data) : "?";
    if (const uintptr_t Rarity = Pointer(Item + ItemRarity))
        if (const std::string Text = TextAt(Rarity + RarityName); !Text.empty())
            Name += " (" + Text + ")";
    Reward.Names.emplace_back(Item, Name);
    return Name;
}

// Nur für die aktuelle Sitzung der Mod, mit Koop-Modus von ihr gesetzt.
bool OwnSession()
{
    return Reward.ForcedCoop && Current(Reward.Coordinator);
}

// Bestätigen des Hosts zurückhalten, solange er zuerst wäre: beide Spieler haben noch nicht geclaimt.
bool HoldConfirm()
{
    if (!OwnSession() || QueueState(Reward.Coordinator) != AwaitingClaims || !RemoteReady.load())
        return false;
    const auto Eligible = Ints(Reward.Coordinator + SessionEligible);
    return Contains(Eligible, 0) && Contains(Eligible, 1);
}

bool __fastcall BeginHook(uintptr_t Coordinator, uintptr_t Chest, uintptr_t Opener)
{
    uintptr_t Coop = 0, Host = 0, Remote = 0;
    bool Forced = false;
    if (CurrentRole() == NetRole::Host && RemoteReady.load() && BoundRemoteP2(Coop, Host, Remote) &&
        !Memory::Read<uint8_t>(Coop + CoopEnabled).value_or(1))
    {
        const uint8_t On = 1;
        Forced = Memory::WriteBytes(Coop + CoopEnabled, &On, 1);
    }
    const bool Result = OriginalBegin(Coordinator, Chest, Opener);
    if (!Forced)
        return Result;
    if (!Reward.ForcedCoop)
    {
        Reward.ForcedCoop = true;
        Reward.Coop = Coop;
    }
    Reward.Coordinator = Coordinator;
    Core::Log::Info("Airdrop-Auswahl: Kiste geöffnet, Koop-Sitzung {} state={} eligible={} warteschlange={}", Result ? "gestartet" : "nicht gestartet",
                    QueueState(Coordinator), Join(Ints(Coordinator + SessionEligible)), Memory::Read<int32_t>(Coordinator + QueuedRewards + 8).value_or(-1));
    return Result;
}

void __fastcall ConfirmHook(uintptr_t Panel)
{
    if (CurrentRole() == NetRole::Host && HoldConfirm())
    {
        if (Reward.HeldPanel != Panel || Reward.HeldSession != Reward.Session)
            Core::Log::Info("Airdrop-Auswahl: Host hat bestätigt, wartet auf die Wahl des Clients");
        Reward.HeldPanel = Panel;
        Reward.HeldSession = Reward.Session;
        return;
    }
    OriginalConfirm(Panel);
}

uintptr_t PlayerStateOf(uintptr_t Controller)
{
    const uintptr_t World = CurrentWorld();
    const uintptr_t GameState = World ? Pointer(World + Offsets::UWorld::GameState) : 0;
    const uintptr_t States = GameState ? Pointer(GameState + Offsets::AGameStateBase::PlayerArray) : 0;
    const int32_t Count = GameState ? Memory::Read<int32_t>(GameState + Offsets::AGameStateBase::PlayerArray + 8).value_or(0) : 0;
    for (int32_t i = 0; States && i < std::min(Count, 4); ++i)
        if (const uintptr_t PlayerState = Pointer(States + 8 * i); PlayerState && Pointer(PlayerState + PlayerStateOwner) == Controller)
            return PlayerState;
    return 0;
}

uintptr_t InventoryOf(uintptr_t PlayerState)
{
    if (!PlayerState)
        return 0;
    const int32_t Total = Objects::Count();
    for (int32_t i = 0; i < Total; ++i)
    {
        const uintptr_t Object = Objects::At(i);
        if (Object && Pointer(Object + InventoryPlayerState) == PlayerState && Alive(Object) && Objects::IsA(Object, "InventoryComponent"))
            return Object;
    }
    return 0;
}

bool InInventory(uintptr_t Inventory, uintptr_t Item)
{
    static uintptr_t Fn = 0;
    if (!Inventory || !Function(Fn, "IsInInventory", "InventoryComponent"))
        return false;
    struct
    {
        uintptr_t Item;
        bool Result;
    } Params{Item, false};
    Objects::ProcessEvent(Inventory, Fn, &Params);
    return Params.Result;
}

// Die Wahl des Clients liegt nach ClaimReward(1) beim Host. Neu im Inventar des Clients anlegen, beim Host entfernen.
void MoveClientChoice()
{
    const uintptr_t Item = Reward.ClientChoice;
    Reward.ClientChoice = 0;
    uintptr_t Coop = 0, Host = 0, Remote = 0;
    if (!Alive(Item) || !BoundRemoteP2(Coop, Host, Remote))
    {
        Core::Log::Warn("Airdrop-Auswahl: Wahl des Clients nicht mehr greifbar, nicht verschoben");
        return;
    }
    const uintptr_t HostInventory = InventoryOf(PlayerStateOf(Host)), ClientInventory = InventoryOf(PlayerStateOf(Remote));
    if (InInventory(ClientInventory, Item))
    {
        Core::Log::Info("Airdrop-Auswahl: Wahl liegt schon im Inventar des Clients");
        return;
    }
    if (!ClientInventory || !InInventory(HostInventory, Item))
    {
        Core::Log::Warn("Airdrop-Auswahl: Wahl des Clients weder bei Host noch Client gefunden (Inventare {} {})", HostInventory != 0,
                        ClientInventory != 0);
        return;
    }
    static uintptr_t Create = 0, Remove = 0;
    if (!Function(Create, "CreateAndAddItem", "InventoryComponent") || !Function(Remove, "RemoveFromInventory", "InventoryComponent"))
        return;
    struct
    {
        uintptr_t Data, Rarity;
        bool Broken, FlagAsNew, SystemAdd;
        uint8_t Pad;
        int32_t Position;
        bool AllowP2Reserve;
        uint8_t Pad2[7];
        uintptr_t Result;
    } Add{Pointer(Item + ItemData), Pointer(Item + ItemRarity), Memory::Read<uint8_t>(Item + ItemBroken).value_or(0) != 0, true, true, 0, -1,
          false, {}, 0};
    static_assert(sizeof(Add) == 0x28);
    if (!Add.Data)
        return;
    Objects::ProcessEvent(ClientInventory, Create, &Add);
    if (!Add.Result)
    {
        Core::Log::Warn("Airdrop-Auswahl: CreateAndAddItem beim Client fehlgeschlagen, Wahl bleibt beim Host");
        return;
    }
    struct
    {
        uintptr_t Item;
        uint8_t Context;
        bool Destroy;
        uint8_t Pad[6];
    } Drop{Item, 0, true, {}};
    static_assert(sizeof(Drop) == 0x10);
    Objects::ProcessEvent(HostInventory, Remove, &Drop);
    Core::Log::Info("Airdrop-Auswahl: {} ins Inventar des Clients gelegt, beim Host {}", ItemName(Item),
                    InInventory(HostInventory, Item) ? "noch vorhanden" : "entfernt");
}

bool CheckBegin()
{
    return Code::Expect(At(BeginCoopReward), {0x48, 0x89, 0x5C, 0x24, 0x18, 0x48, 0x89, 0x74, 0x24, 0x20}, "Koop-Reward-Sitzung beginnen");
}
bool CheckConfirm()
{
    return Code::Expect(At(PanelConfirm), {0x40, 0x57, 0x48, 0x83, 0xEC, 0x50, 0x48, 0x8B, 0xF9}, "InventoryPanel.HandleConfirmButton") &&
           Code::Expect(At(MemoryFree), {0x48, 0x85, 0xC9, 0x74, 0x2E}, "FMemory::Free");
}
bool ApplyBegin(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(BeginCoopReward)), reinterpret_cast<void*>(&BeginHook),
                                reinterpret_cast<void**>(&OriginalBegin));
}
bool ApplyConfirm(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(PanelConfirm)), reinterpret_cast<void*>(&ConfirmHook),
                                reinterpret_cast<void**>(&OriginalConfirm));
}
const Patch List[] = {
    {"Airdrop-Auswahl: Koop-Sitzung für den Remote-Spieler", CheckBegin, ApplyBegin},
    {"Airdrop-Auswahl: Host-Bestätigen bis zur Client-Wahl halten", CheckConfirm, ApplyConfirm},
};
}

std::span<const Patch> RewardPatches()
{
    return List;
}
}

namespace Game::Rewards
{
using namespace Game::Patches;

void SetRemoteReady(bool Ready)
{
    if (RemoteReady.exchange(Ready) != Ready)
        Core::Log::Info("Airdrop-Auswahl: Client mit Mod {}", Ready ? "verbunden, Koop-Sitzung bei Airdrops an" : "fehlt, Solo-Auswahl");
}

std::optional<Offer> ReadOffer()
{
    if (!OwnSession() || QueueState(Reward.Coordinator) != AwaitingClaims)
        return std::nullopt;
    const auto Items = Choices(Reward.Coordinator);
    const auto Eligible = Ints(Reward.Coordinator + SessionEligible);
    Offer Result;
    Result.Session = Reward.Session;
    Result.Revision = Revision(Items);
    Result.Level = Memory::Read<int32_t>(Reward.Coordinator + SessionLevel).value_or(0);
    for (const uintptr_t Item : Items)
        Result.Choices.push_back(Item ? ItemName(Item) : "?");
    Result.ClientClaimed = !Contains(Eligible, 1);
    Result.HostClaimed = !Contains(Eligible, 0);
    Result.HostWaiting = Reward.HeldPanel && Reward.HeldSession == Reward.Session;
    return Result;
}

bool ClaimForClient(uint64_t SessionId, uint64_t Expected, uint32_t Index)
{
    if (!OwnSession() || SessionId != Reward.Session || QueueState(Reward.Coordinator) != AwaitingClaims ||
        !Contains(Ints(Reward.Coordinator + SessionEligible), 1))
        return false;
    const auto Items = Choices(Reward.Coordinator);
    if (Revision(Items) != Expected || Index >= Items.size() || !Items[Index])
        return false;
    static uintptr_t Claim = 0;
    if (!Function(Claim, "ClaimReward", "CoopInventoryCoordinatorComponent"))
        return false;
    struct
    {
        int32_t Player;
        uint32_t Pad;
        uintptr_t Choice;
        bool Result;
        uint8_t Pad2[7];
    } Params{1, 0, Items[Index], false, {}};
    static_assert(sizeof(Params) == 0x18);
    const std::string Name = ItemName(Items[Index]);
    Objects::ProcessEvent(Reward.Coordinator, Claim, &Params);
    Core::Log::Info("Airdrop-Auswahl: Client wählt {} ({}), ClaimReward(1)={} state={} eligible={}", Index, Name, Params.Result,
                    QueueState(Reward.Coordinator), Join(Ints(Reward.Coordinator + SessionEligible)));
    if (Params.Result)
    {
        Reward.ClientChoice = Items[Index];
        Reward.ClientSession = SessionId;
    }
    return Params.Result;
}

void Tick()
{
    if (CurrentRole() != NetRole::Host || !Reward.ForcedCoop)
        return;
    if (!Current(Reward.Coordinator))
    {
        // Welt gewechselt: Koop-Byte am Subsystem (GameInstance) trotzdem zurücksetzen.
        Core::Log::Warn("Airdrop-Auswahl: Koordinator weg, Koop-Modus zurückgesetzt");
        const uint8_t Off = 0;
        Memory::WriteBytes(Reward.Coop + CoopEnabled, &Off, 1);
        Reward = {};
        return;
    }
    const uint8_t Now = QueueState(Reward.Coordinator);
    const uintptr_t Chest = Pointer(Reward.Coordinator + Session);
    if (Now == AwaitingClaims && Chest != Reward.Chest)
    {
        Reward.Chest = Chest;
        ++Reward.Session;
        Reward.Names.clear();
        Reward.CompletedSince = 0;
        Reward.Acknowledged = false;
        Core::Log::Info("Airdrop-Auswahl: Sitzung {} level={} choices={}", Reward.Session,
                        Memory::Read<int32_t>(Reward.Coordinator + SessionLevel).value_or(0), Choices(Reward.Coordinator).size());
    }
    // Zurückgehaltenes Bestätigen nachholen, sobald der Client geclaimt hat.
    if (Reward.HeldPanel && (Reward.HeldSession != Reward.Session || Now != AwaitingClaims || !HoldConfirm()))
    {
        const uintptr_t Panel = Reward.HeldPanel;
        Reward.HeldPanel = 0;
        if (Now == AwaitingClaims && Reward.HeldSession == Reward.Session && Alive(Panel) && IsA(Panel, "InventoryPanel"))
        {
            Core::Log::Info("Airdrop-Auswahl: Client hat gewählt, Bestätigen des Hosts nachgeholt");
            OriginalConfirm(Panel);
            Core::Log::Info("Airdrop-Auswahl: nach dem Host-Claim state={} claimed={}", QueueState(Reward.Coordinator),
                            Join(Ints(Reward.Coordinator + SessionClaimed)));
        }
    }
    // Rückfall, falls das Panel nach dem letzten Claim nicht selbst abschließt.
    if (Now == Completed)
    {
        const ULONGLONG Tick = GetTickCount64();
        if (!Reward.CompletedSince)
            Reward.CompletedSince = Tick;
        else if (!Reward.Acknowledged && Tick - Reward.CompletedSince > 3000)
        {
            Reward.Acknowledged = true;
            static uintptr_t Ack = 0;
            bool Result = false;
            if (Function(Ack, "AcknowledgeRewardUIClosed", "CoopInventoryCoordinatorComponent"))
                Objects::ProcessEvent(Reward.Coordinator, Ack, &Result);
            Core::Log::Warn("Airdrop-Auswahl: Sitzung blieb abgeschlossen offen, AcknowledgeRewardUIClosed={}", Result);
        }
    }
    else
        Reward.CompletedSince = 0;
    if (Now == 0 && Memory::Read<int32_t>(Reward.Coordinator + QueuedRewards + 8).value_or(0) == 0)
    {
        if (Reward.ClientChoice)
            MoveClientChoice();
        const uint8_t Off = 0;
        Memory::WriteBytes(Reward.Coop + CoopEnabled, &Off, 1);
        Core::Log::Info("Airdrop-Auswahl: Sitzung beendet, Koop-Modus wieder aus");
        const uint64_t Last = Reward.Session;
        Reward = {};
        Reward.Session = Last;
    }
}
}
