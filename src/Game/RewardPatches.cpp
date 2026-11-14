// Airdrop-Auswahl für Host und Client ohne Koop-Modus, siehe src/Game/README.md#airdrop-auswahl.
//
// Host: Die Kiste läuft wie im Solo-Spiel, öffnet sie der Remote-Spieler, bekommt die Figur des Hosts die Auswahl.
// UInventoryComponent::SetRewardItems (0x597DC10) zeigt die Choices im nativen Panel; die Mod merkt sie sich als
// Sitzung und schickt sie über den Kontrollkanal an den Client. Das Bestätigen des Hosts (InventoryPanel.
// HandleConfirmButton, 0x5A71900) hält die Mod zurück, bis der Client gewählt hat; so lange bleibt das Spiel pausiert.
// Die Wahl des Clients legt der Host neu in dessen repliziertes Inventar.
//
// Client: Die Mod legt lokale Instanzen derselben Items an und zeigt sie über SetRewardItems im nativen Panel. Das Panel
// bearbeitet das Inventar nur lokal (InventoryEdit.cpp). Nach dem Bestätigen ist die gewählte Instanz keine Belohnung
// mehr; Wahl und Endstand gehen als ein Edit an den Host (inventar-lokal).
#include <windows.h>

#include <intrin.h>

#include <algorithm>
#include <atomic>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/InventoryEditRuntime.h"
#include "Game/Offsets.h"
#include "Game/PatchPolicy.h"
#include "Game/PatchRuntime.h"
#include "Game/Reflection.h"
#include "Game/RewardAdapter.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t SetRewardItems = 0x597DC10;
constexpr uintptr_t PanelConfirm = 0x5A71900;
constexpr uintptr_t PanelSetupInventory = 0x5A90940;
// UInventoryComponent::CreateItemInstance, nativ. Die UFunction ist BlueprintAuthorityOnly, ProcessEvent überspringt
// sie beim Client.
constexpr uintptr_t CreateItemInstance = 0x595DA20;
constexpr uintptr_t MemoryFree = 0x12F85E0;
// UInventoryComponent::UpgradeItems, Exec-Thunk 0x571E5A0. Rufen InventoryPanel.UpgradeDroppedItem (0x5A9D370) und
// UpgradeItemToNextLevelAndReEquip (0x5984AF0) direkt (werkbank-client).
constexpr uintptr_t UpgradeItems = 0x5984CD0;
// UInventoryComponent::HandleItemSold, hinter dem Exec-Thunk. Ruft ItemRepresentation.ConfirmSell (0x5AA7E50) nach der lokalen
// Gutschrift (AbstractCurrency::AddAmount) direkt; nimmt das Item über die Inventar-RPCs heraus (lager-handel-client).
constexpr uintptr_t HandleItemSold = 0x596BA70;
// UShopPanelItem::TryBuyItem, hinter dem Exec-Thunk 0x579ADE0 (opt-haendler-client). Prüft den Preis über einen Funktor des
// Panels, zieht ihn lokal ab und legt die lokale Instanz des Shops mit AddToInventory ins Inventar des Käufers.
constexpr uintptr_t TryBuyItem = 0x58CE280;
// UShopPanelItem, aus dem Code von TryBuyItem dieses Builds: ManuallyCallClearAndAdd, CostDisplay, ItemInstance;
// UCostDisplay: der verlangte Preis (GetActualCost).
constexpr uintptr_t ShopItemManualAdd = 0x848, ShopItemCostDisplay = 0x850, ShopItemInstance = 0x868;
constexpr uintptr_t CostDisplayActual = 0x384;
// ADDMissionGameState.ScrapWallet, UAbstractCurrency.CurrencyAmount
constexpr uintptr_t GameStateScrapWallet = 0x3C0;
constexpr uintptr_t CurrencyAmount = 0x100;
// URarity.NextUpgradableRarity
constexpr uintptr_t RarityNext = 0x58;

// UInventoryItemInstance, UInventoryItemData, URarity, UInventoryComponent
constexpr uintptr_t ItemData = 0xE0;
constexpr uintptr_t ItemBroken = 0x110;
constexpr uintptr_t ItemRarity = 0x118;
constexpr uintptr_t ItemInBackpack = 0x180;
constexpr uintptr_t ItemIsReward = 0x182;
constexpr uintptr_t DataName = 0x40;
constexpr uintptr_t RarityName = 0x28;
constexpr uintptr_t InventoryPlayerState = 0x228;
constexpr uintptr_t InventoryItems = 0x120;
// UGameInstance.LocalPlayers, UPlayer.PlayerController, AController.PlayerState, AController.Pawn
constexpr uintptr_t GameInstanceLocalPlayers = 0x38;
constexpr uintptr_t PlayerController = 0x30;
constexpr uintptr_t ControllerPlayerState = 0x2B0;
constexpr uintptr_t ControllerPawn = 0x2E8;
// BP_DD_Character_C.RewardQueue, URewardQueueComponent: Byte +0x108 = Kiste offen (OpenNextReward setzt/löscht)
constexpr uintptr_t CharacterRewardQueue = 0x980;
constexpr uintptr_t QueueBusy = 0x108;
constexpr uintptr_t QueueCurrent = 0x10C; // CurrentRewardObject, FWeakObjectPtr
constexpr uint32_t GarbageOrUnreachableFlags = 0x30;

struct ItemArray
{
    uintptr_t* Data;
    int32_t Count, Max;
};
using SetRewardsFn = void(__fastcall*)(uintptr_t, ItemArray*, int32_t);
using ConfirmFn = void(__fastcall*)(uintptr_t);
using CreateFn = uintptr_t(__fastcall*)(uintptr_t, uintptr_t, bool, uintptr_t);
using FreeFn = void(__fastcall*)(void*);
// UInventoryComponent::UpgradeItems(this, const TArray& Inputs, Data, Rarity, FlagAsNew, Position), nativ
using UpgradeFn = uintptr_t(__fastcall*)(uintptr_t, ItemArray*, uintptr_t, uintptr_t, bool, int32_t);
// UInventoryComponent::HandleItemSold(this, Item), nativ
using SoldFn = void(__fastcall*)(uintptr_t, uintptr_t);
SetRewardsFn OriginalSetRewards = nullptr;
ConfirmFn OriginalConfirm = nullptr;
ConfirmFn OriginalSetupInventory = nullptr;
UpgradeFn OriginalUpgrade = nullptr;
SoldFn OriginalSold = nullptr;
ConfirmFn OriginalTryBuy = nullptr; // UShopPanelItem::TryBuyItem(this)

std::atomic<bool> RemoteReady = false;

// Host: aktuelle Auswahl.
struct
{
    uint64_t Session = 0; // Zähler, 0 vor der ersten Auswahl
    bool Active = false;
    uintptr_t Inventory = 0;
    std::vector<uintptr_t> Items;
    std::vector<Game::Rewards::ChoiceItem> Choices;
    std::vector<std::pair<uintptr_t, uintptr_t>> Data; // ItemData, Rarity je Choice
    int32_t Level = 0;
    std::optional<uint32_t> ClientPick;
    uintptr_t HeldPanel = 0;
    Game::Objects::Ref HeldRef; // HeldPanel im Objekt-Array, dazu die Welt beim Zurückhalten
    uintptr_t HeldWorld = 0;
    bool ClientReady = false; // Client hat Bestätigen gedrückt (bestaetigen-alle), für ReadyRevision
    uint64_t ReadyRevision = 0;
    int ReplayCount = 0;        // Versuche, das zurückgehaltene Bestätigen nachzuholen
    ULONGLONG NextReplay = 0;
} Host;
// Client: Anzeige der Host-Choices im nativen Panel, nur lokale Instanzen. Ihre Inventar-RPCs laufen während der
// Bearbeitung lokal (InventoryEdit::RouteClientRpc); beim Host kämen sie als Null-Zeiger an (Absturz in AddToBackpack).
struct
{
    uint64_t Session = 0, Revision = 0;
    uintptr_t Inventory = 0;
    std::vector<uintptr_t> Items;
    uintptr_t Panel = 0; // Panel, in dem der Client Bestätigen gedrückt hat
    Game::Objects::Ref PanelRef;
    uintptr_t PanelWorld = 0;
    bool Ready = false;  // Bestätigen gedrückt, wartet auf alle (bestaetigen-alle)
} Shown;
// Lager am Rundenende (bestaetigen-rundenende), beide Rollen. Id kommt je Takt aus den Kartenpatches.
struct
{
    uint64_t Id = 0;
    uint64_t Done = 0;        // Bestätigen für dieses Rundenende ist nativ gelaufen
    uintptr_t HeldPanel = 0;  // zurückgehaltenes Bestätigen
    Game::Objects::Ref HeldRef; // dasselbe Panel im Objekt-Array, dazu die Welt beim Zurückhalten
    uintptr_t HeldWorld = 0;
    uint64_t ClientIn = 0, ClientReady = 0; // Host: Stand des Clients aus 0x306
    uint64_t HostStatusId = 0; // Client: Rundenende im frischen Stand des Hosts, 0 ohne
} End;
// Client: gesendeter Edit bis zur Antwort des Hosts.
std::optional<Game::InventoryEdit::Edit> ClientPending;
// Client, werkbank-client: Die erste Aufwertung im Lager hat eine lokale Bearbeitung begonnen, sie endet mit dem Bestätigen.
// lager-handel-client: ebenso der erste Verkauf oder die erste Reparatur.
uint64_t LagerEdit = 0; // Id des Rundenendes, 0 ohne
// Client, opt-werkbank-station: Bearbeitung an einer Station außerhalb von Lager und Airdrop-Auswahl, etwa das Inventar-Panel
// im Händler-Event. Sie beginnt mit der ersten Aufwertung, dem ersten Verkauf oder der ersten Reparatur und endet, sobald
// kein Panel der Station mehr offen ist; dann geht der Edit an den Host. Panels sind die Inventar-Panels zum eigenen Inventar.
struct
{
    bool Active = false;
    uintptr_t World = 0, Inventory = 0;
    std::vector<Game::Objects::Ref> Panels;
    ULONGLONG NextPoll = 0, NextScan = 0;
    bool Closed = false; // ein Inventar-Panel zum eigenen Inventar hat sein Schließen gemeldet
} Station;
constexpr uintptr_t ItemPivotTile = 0x28;
constexpr uint32_t FuncNet = 0x40;
constexpr uintptr_t FunctionFlags = 0xB0;
// UInventoryPanel.IsDragging (SDK und HandleConfirmButton).
constexpr uintptr_t PanelIsDragging = 0x788;
// UDefaultFocusableButton.ParentPanel; der Bestätigen-Knopf des Inventar-Panels heißt im Widget-Baum MainButton.
constexpr uintptr_t ButtonParentPanel = 0x3A8;

bool Alive(uintptr_t Object)
{
    return Object && !(Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachableFlags) & GarbageOrUnreachableFlags);
}

uintptr_t Pointer(uintptr_t Address)
{
    return Memory::ReadPointer(Address).value_or(0);
}

uint64_t Revision(const std::vector<uintptr_t>& Items)
{
    uint64_t Hash = 1469598103934665603ull;
    for (const uintptr_t Item : Items)
        Hash = (Hash ^ Item) * 1099511628211ull;
    return Hash ? Hash : 1;
}

// Nur einmal suchen, auch wenn nicht gefunden (1 = fehlt): läuft in jedem ProcessEvent.
uintptr_t Function_(uintptr_t& Cache, const char* Name)
{
    if (!Cache)
        if (!(Cache = Objects::FindObject(Name, "InventoryComponent")))
            Cache = 1;
    return Cache;
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

std::string ItemName(uintptr_t Data, uintptr_t Rarity)
{
    std::string Name = Data ? TextAt(Data + DataName) : std::string{};
    if (Name.empty())
        Name = Data ? Memory::NameOf(Data) : "?";
    if (Rarity)
        if (const std::string Text = TextAt(Rarity + RarityName); !Text.empty())
            Name += " (" + Text + ")";
    return Name;
}

uintptr_t InventoryOf(uintptr_t PlayerState)
{
    if (!PlayerState)
        return 0;
    const int32_t Total = Objects::Count();
    for (int32_t i = 0; i < Total; ++i)
    {
        const uintptr_t Object = Objects::At(i);
        if (Object && (Pointer(Object + InventoryPlayerState) == PlayerState || Pointer(Object + Offsets::UObject::Outer) == PlayerState) &&
            Alive(Object) && Objects::IsA(Object, "InventoryComponent"))
            return Object;
    }
    return 0;
}

uintptr_t InventoryOfController(uintptr_t Controller)
{
    return InventoryOf(Controller ? Pointer(Controller + ControllerPlayerState) : 0);
}

uintptr_t LocalController()
{
    const uintptr_t World = CurrentWorld();
    const uintptr_t Instance = World ? Pointer(World + Offsets::UWorld::OwningGameInstance) : 0;
    const uintptr_t Players = Instance ? Pointer(Instance + GameInstanceLocalPlayers) : 0;
    const uintptr_t Local = Players ? Pointer(Players) : 0;
    return Local ? Pointer(Local + PlayerController) : 0;
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

bool IsReward(uintptr_t Item)
{
    return Alive(Item) && Memory::Read<uint8_t>(Item + ItemIsReward).value_or(0);
}

// Datenobjekte der Choices über ihre Objektnamen, ein Durchlauf über das Objekt-Array.
std::vector<std::pair<uintptr_t, uintptr_t>> ResolveChoices(const std::vector<Game::Rewards::ChoiceItem>& Choices)
{
    std::vector<std::pair<uintptr_t, uintptr_t>> Result(Choices.size());
    size_t Missing = 0;
    for (const auto& Choice : Choices)
        Missing += 1 + !Choice.Rarity.empty();
    const int32_t Total = Objects::Count();
    for (int32_t i = 0; i < Total && Missing; ++i)
    {
        const uintptr_t Object = Objects::At(i);
        if (!Object)
            continue;
        const std::string Name = Memory::NameOf(Object);
        for (size_t c = 0; c < Choices.size(); ++c)
        {
            if (!Result[c].first && Name == Choices[c].Data && Objects::IsA(Object, "InventoryItemData"))
            {
                Result[c].first = Object;
                --Missing;
            }
            if (!Result[c].second && !Choices[c].Rarity.empty() && Name == Choices[c].Rarity && Objects::IsA(Object, "Rarity"))
            {
                Result[c].second = Object;
                --Missing;
            }
        }
    }
    return Result;
}

// Bestätigen-Knopf eines Inventar-Panels: DefaultFocusableButton mit diesem ParentPanel, Name MainButton. Nur beim Klick
// gesucht, ein Durchlauf über das Objekt-Array. Fehlt er, stehen die Knöpfe des Panels einmal im Log.
uintptr_t ConfirmButtonOf(uintptr_t Panel)
{
    static uintptr_t CachedPanel = 0, CachedButton = 0;
    if (Panel == CachedPanel && Alive(CachedButton) && Pointer(CachedButton + ButtonParentPanel) == Panel)
        return CachedButton;
    CachedPanel = Panel;
    CachedButton = 0;
    std::string Others;
    const int32_t Total = Objects::Count();
    for (int32_t i = 0; i < Total; ++i)
    {
        const uintptr_t Object = Objects::At(i);
        if (!Object || Pointer(Object + ButtonParentPanel) != Panel || !Alive(Object) || !Objects::IsA(Object, "DefaultFocusableButton"))
            continue;
        const std::string Name = Memory::NameOf(Object);
        // Im Spieltest 06.11.2026 hatte das Panel genau einen MainButton; ConfirmButton gehört zum Rezept-Panel.
        if (Name == "MainButton")
        {
            Core::Log::Info("Bestätigen: Knopf {} im Panel gefunden ({})", Name, Memory::NameOf(Pointer(Object + Offsets::UObject::Outer)));
            return CachedButton = Object;
        }
        Others += Others.empty() ? Name : ", " + Name;
    }
    static bool Logged = false;
    if (!std::exchange(Logged, true))
        Core::Log::Warn("Bestätigen: Knopf MainButton im Panel nicht gefunden, Knöpfe: {}", Others.empty() ? "keine" : Others);
    return 0;
}

Objects::Ref Tinted; // gelb gefärbter Knopf

// lager-bestaetigen-absturz: Ein zurückgehaltenes Panel ist nur nutzbar, solange es im Objekt-Array lebt und die Welt dieselbe
// ist. Alive allein liest Flags am alten Speicher; nach der Reise des Hosts bestand ein freigegebenes Panel diese Prüfung,
// HandleConfirmButton folgte dann seinen Feldern bis zu einem leeren Funktionszeiger.
bool HeldPanelUsable(const Objects::Ref& Held, uintptr_t HeldWorld)
{
    return MayReplayHeldConfirm(Objects::IsLive(Held), HeldWorld, CurrentWorld()) && Objects::IsA(Held.Object, "InventoryPanel");
}

// Knopf gelb (bereit) oder wieder normal, über UUserWidget.SetColorAndOpacity. Die Farbe ist rein lokal.
void SetReadyTint(uintptr_t Panel, bool Ready)
{
    static uintptr_t SetColor = 0;
    const uintptr_t Button = Ready ? ConfirmButtonOf(Panel) : Tinted.Object;
    if (!(Ready ? Alive(Button) : Objects::IsLive(Tinted)) || !Function(SetColor, "SetColorAndOpacity", "UserWidget"))
    {
        if (!Ready)
            Tinted = {};
        return;
    }
    struct
    {
        float R, G, B, A;
    } Color = Ready ? decltype(Color){1.0f, 0.82f, 0.1f, 1.0f} : decltype(Color){1.0f, 1.0f, 1.0f, 1.0f};
    Objects::ProcessEvent(Button, SetColor, &Color);
    Tinted = Ready ? Objects::RefTo(Button) : Objects::Ref{};
}

void ClearReadyTint()
{
    if (Tinted.Object)
        SetReadyTint(0, false);
}

void EndHostSession(const char* Why)
{
    if (!Host.Active)
        return;
    ClearReadyTint();
    if (!Host.ClientPick && RemoteReady.load())
        Core::Log::Warn("Airdrop-Auswahl: Sitzung {} endet ohne Wahl des Clients ({})", Host.Session, Why);
    else
        Core::Log::Info("Airdrop-Auswahl: Sitzung {} beendet ({})", Host.Session, Why);
    const uint64_t Last = Host.Session;
    Host = {};
    Host.Session = Last;
}

void __fastcall SetRewardsHook(uintptr_t Inventory, ItemArray* Items, int32_t Level)
{
    if (CurrentRole() == NetRole::Host && Items && Items->Data && Items->Count > 0 && Items->Count <= 8)
    {
        EndHostSession("neue Auswahl");
        Host.Active = true;
        ++Host.Session;
        Host.Inventory = Inventory;
        Host.Level = Level;
        for (int32_t i = 0; i < Items->Count; ++i)
        {
            const uintptr_t Item = Items->Data[i];
            const uintptr_t Data = Item ? Pointer(Item + ItemData) : 0, Rarity = Item ? Pointer(Item + ItemRarity) : 0;
            Host.Items.push_back(Item);
            Host.Data.emplace_back(Data, Rarity);
            Host.Choices.push_back({ItemName(Data, Rarity), Data ? Memory::NameOf(Data) : std::string{}, Rarity ? Memory::NameOf(Rarity) : std::string{},
                                    Item && Memory::Read<uint8_t>(Item + ItemBroken).value_or(0) != 0});
        }
        Core::Log::Info("Airdrop-Auswahl: Sitzung {} mit {} Choices, Stufe {}", Host.Session, Host.Items.size(), Level);
    }
    OriginalSetRewards(Inventory, Items, Level);
}

bool ClientConfirm(uintptr_t Panel)
{
    if (Shown.Items.empty() || !Game::InventoryEdit::ClientEditing())
        return false;
    // HandleConfirmButton selbst ignoriert Bestätigen während eines Drags. Dann auch keinen Edit abschließen.
    if (Memory::Read<uint8_t>(Panel + PanelIsDragging).value_or(1))
        return true;
    OriginalConfirm(Panel); // entfernt die übrigen Choices, lokal
    // Gewählt ist die Instanz, die keine Belohnung mehr ist, bevorzugt die im Inventar (ein Rucksack kann schon verbraucht sein).
    std::optional<uint32_t> Pick;
    for (int Pass = 0; Pass < 2 && !Pick; ++Pass)
        for (uint32_t i = 0; i < Shown.Items.size() && !Pick; ++i)
            if (Alive(Shown.Items[i]) && !IsReward(Shown.Items[i]) && (Pass == 1 || InInventory(Shown.Inventory, Shown.Items[i])))
                Pick = i;
    const bool Open = std::any_of(Shown.Items.begin(), Shown.Items.end(),
                                  [](uintptr_t Item) { return IsReward(Item) && InInventory(Shown.Inventory, Item); });
    if (!Pick && Open)
    {
        Core::Log::Info("Airdrop-Auswahl: Bestätigen ohne Wahl, Panel bleibt offen");
        return true;
    }
    Core::Log::Info("Airdrop-Auswahl: Client bestätigt im Panel, Wahl={}", Pick ? std::to_string(*Pick) : std::string("keine"));
    ClientPending = Game::InventoryEdit::FinishClient(Shown.Session, Shown.Revision,
                                                      Pick ? std::optional<uint8_t>(static_cast<uint8_t>(*Pick)) : std::nullopt,
                                                      Pick ? Shown.Items[*Pick] : 0);
    Shown.Items.clear();
    return true;
}

// bestaetigen-rundenende: Bestätigen im Lager am Rundenende zurückhalten (Knopf gelb), bis beide bereit sind. Falsch, wenn das
// native Bestätigen laufen soll; dann gilt das Rundenende als bestätigt. Im Drag bleibt es nativ, dort ist es ohne Wirkung.
bool HoldRoundEndConfirm(uintptr_t Panel)
{
    const NetRole Role = CurrentRole();
    if (!End.Id || End.Done == End.Id || Role == NetRole::Standalone || Memory::Read<uint8_t>(Panel + PanelIsDragging).value_or(1))
        return false;
    if (End.HeldPanel == Panel)
        return true; // schon bereit, weitere Klicks ändern nichts
    // Ein offener Edit des Clients (Aufwertung im Lager) hält sein Bestätigen bis zur Antwort des Hosts zurück.
    const bool Hold = Role == NetRole::Host ? HostHoldsRoundEndConfirm(End.Id, End.Done, RemoteReady.load(), End.ClientIn, End.ClientReady)
                                            : ClientHoldsRoundEndConfirm(End.Id, End.Done, End.HostStatusId) || ClientPending.has_value();
    if (!Hold)
    {
        End.Done = End.Id;
        // Beim Host heißt nicht zurückgehalten auch: Client nicht verbunden oder nicht im Lager. Dann nicht „alle bereit“.
        const bool Alone = Role == NetRole::Host && (!RemoteReady.load() || End.ClientIn != End.Id);
        Core::Log::Info("Lager: {} bestätigt, {}", Role == NetRole::Host ? "Host" : "Client", Alone ? "ohne Client im Lager" : "alle bereit");
        return false;
    }
    End.HeldPanel = Panel;
    End.HeldRef = Objects::RefTo(Panel);
    End.HeldWorld = CurrentWorld();
    SetReadyTint(Panel, true);
    Core::Log::Info("Lager: {} bereit, wartet auf {}", Role == NetRole::Host ? "Host" : "Client", Role == NetRole::Host ? "den Client" : "den Host");
    return true;
}

// Zurückgehaltenes Bestätigen im Lager nativ nachholen. Einmal: Bleibt es ohne Wirkung, bestätigt der Spieler erneut, dann
// läuft es direkt nativ. Ist das Panel weg oder die Welt gewechselt (Host schon weitergereist), nur die Farbe vergessen.
void ReleaseRoundEndConfirm(const char* Why)
{
    const uintptr_t Panel = End.HeldPanel;
    if (!Panel)
        return;
    if (End.HeldRef.Object != Panel || !HeldPanelUsable(End.HeldRef, End.HeldWorld))
    {
        const bool WorldChanged = End.HeldWorld != CurrentWorld();
        End.HeldPanel = 0;
        End.HeldRef = {};
        Tinted = {};
        Core::Log::Warn("Lager: {}, zurückgehaltenes Bestätigen verworfen ({})", WorldChanged ? "Welt gewechselt" : "Panel weg", Why);
        return;
    }
    if (Memory::Read<uint8_t>(Panel + PanelIsDragging).value_or(1))
        return; // nach dem Drag erneut
    End.HeldPanel = 0;
    End.HeldRef = {};
    End.Done = End.Id;
    SetReadyTint(Panel, false);
    Core::Log::Info("Lager: Bestätigen nachgeholt ({})", Why);
    OriginalConfirm(Panel);
}

// bestaetigen-alle: Bestätigen schließt das Panel nicht sofort, sondern markiert den Spieler als bereit (Knopf gelb). Weitere
// Klicks ändern nichts; ein Zurücknehmen hob im Spieltest das Bereit wieder auf, wenn der andere gerade bestätigte. Erst wenn
// Host und Client bereit sind, löst der Client sein natives Bestätigen aus und schickt den Edit; danach holt der Host sein
// Bestätigen nach (Tick).
void __fastcall ConfirmHook(uintptr_t Panel)
{
    if (CurrentRole() == NetRole::Client && !Shown.Items.empty() && !ClientPending && Game::InventoryEdit::ClientEditing())
    {
        if (Shown.Ready || Memory::Read<uint8_t>(Panel + PanelIsDragging).value_or(1))
            return; // schon bereit; im Drag nativ auch ohne Wirkung
        Shown.Ready = true;
        Shown.Panel = Panel;
        Shown.PanelRef = Objects::RefTo(Panel);
        Shown.PanelWorld = CurrentWorld();
        SetReadyTint(Panel, true);
        Core::Log::Info("Bestätigen: Client bereit, wartet auf alle (Sitzung {})", Shown.Session);
        return;
    }
    if (CurrentRole() == NetRole::Client && ClientConfirm(Panel))
        return;
    if (CurrentRole() == NetRole::Host && Host.Active && !Host.ClientPick && RemoteReady.load())
    {
        if (Host.HeldPanel == Panel || Memory::Read<uint8_t>(Panel + PanelIsDragging).value_or(1))
            return;
        Core::Log::Info("Bestätigen: Host bereit, wartet auf den Client (bereit={})", Host.ClientReady);
        Host.HeldPanel = Panel;
        Host.HeldRef = Objects::RefTo(Panel);
        Host.HeldWorld = CurrentWorld();
        SetReadyTint(Panel, true);
        return;
    }
    // werkbank-client, lager-handel-client: Hat der Client im Lager aufgewertet, verkauft oder repariert, geht die lokale Bearbeitung
    // beim Bestätigen als Edit an den Host.
    // Bis zur Antwort bleibt das Bestätigen zurückgehalten (HoldRoundEndConfirm, ClientRoundEnd).
    if (CurrentRole() == NetRole::Client && LagerEdit && LagerEdit == End.Id && Game::InventoryEdit::ClientEditing() && !ClientPending)
    {
        if (Memory::Read<uint8_t>(Panel + PanelIsDragging).value_or(1))
            return;
        LagerEdit = 0;
        ClientPending = Game::InventoryEdit::FinishClient(0, 0, std::nullopt, 0);
        Core::Log::Info("Lager: Client bestätigt nach lokaler Bearbeitung, Edit {}", ClientPending ? std::to_string(ClientPending->Id) : std::string("ohne Änderung"));
    }
    if (HoldRoundEndConfirm(Panel))
        return;
    OriginalConfirm(Panel);
}

// opt-werkbank-station: Offen-Zustand der Panels beim Client aus UAbstractPanel.OnOpenStateChanged(bool) und
// OnOpenStateChangedAndFinished(bool), die über ProcessEvent kommen. Im Spiel geprüft (13.11.2026): Das Feld AnimationContainer der
// Basisklasse ist bei Inventar- und Händler-Panel leer. Das Inventar-Panel meldet sein Öffnen nur über ...AndFinished(true) und sein
// Schließen über OnOpenStateChanged(false). Das Händler-Panel meldet sein Öffnen, aber nie sein Schließen: Man verlässt den Händler
// über das Inventar-Panel, dessen Schließen (Bestätigen) darum auch die Stations-Bearbeitung beendet.
struct PanelState
{
    Objects::Ref Panel;
    bool Open = false;
};
std::unordered_map<uintptr_t, PanelState> PanelStates;

void NotePanelState(uintptr_t Object, uintptr_t Function, void* Params)
{
    static uintptr_t Changed = 0, Finished = 0;
    if (!Changed && !(Changed = Objects::FindObject("OnOpenStateChanged", "AbstractPanel")))
        Changed = 1;
    if (!Finished && !(Finished = Objects::FindObject("OnOpenStateChangedAndFinished", "AbstractPanel")))
        Finished = 1;
    if ((Function != Changed && Function != Finished) || !Params || !Object)
        return;
    if (PanelStates.size() > 64)
        std::erase_if(PanelStates, [](const auto& Entry) { return !Objects::IsLive(Entry.second.Panel); });
    const bool Open = Memory::Read<uint8_t>(reinterpret_cast<uintptr_t>(Params)).value_or(0) != 0;
    PanelStates[Object] = {Objects::RefTo(Object), Open};
    constexpr uintptr_t PanelInventory = 0x370;
    if (!Open && Station.Active && Objects::IsA(Object, "InventoryPanel") && Pointer(Object + PanelInventory) == Station.Inventory)
        Station.Closed = true;
}

// Ist das Panel offen? Zuerst das gemeldete Ereignis, sonst UAbstractPanel.AnimationContainer und UPanelAnimationContainer.IsOpen
// über die Reflexion. Ohne beides gilt das Panel als geschlossen, dann beginnt keine Stations-Bearbeitung.
bool PanelOpen(uintptr_t Panel)
{
    if (const auto It = PanelStates.find(Panel); It != PanelStates.end() && It->second.Panel.Object == Panel && Objects::IsLive(It->second.Panel))
        return It->second.Open;
    constexpr Reflection::FieldSpec ContainerSpec{"/Script/DDSurvivors", "AbstractPanel", "AnimationContainer", "ObjectProperty", 8};
    constexpr Reflection::FieldSpec OpenSpec{"/Script/DDSurvivors", "PanelAnimationContainer", "IsOpen", "BoolProperty", 1};
    const auto ContainerField = Reflection::Property(ContainerSpec);
    const auto OpenField = Reflection::Property(OpenSpec);
    if (!ContainerField || !OpenField || !Alive(Panel))
        return false;
    const uintptr_t Container = Pointer(Panel + ContainerField->Offset);
    return Alive(Container) && (Memory::Read<uint8_t>(Container + OpenField->Offset).value_or(0) & OpenField->BoolMask) != 0;
}

// Panels einer Station: jedes lebende Inventar-Panel zum eigenen Inventar. Ein Durchlauf über das Objekt-Array, darum nur beim
// Beginn und danach alle zwei Sekunden.
std::vector<Objects::Ref> StationPanels(uintptr_t Inventory)
{
    constexpr uintptr_t PanelInventory = 0x370;
    std::vector<Objects::Ref> Result;
    const int32_t Total = Objects::Count();
    for (int32_t i = 0; i < Total; ++i)
    {
        const uintptr_t Object = Objects::At(i);
        if (!Object || Pointer(Object + PanelInventory) != Inventory || !Alive(Object) || !Objects::IsA(Object, "InventoryPanel"))
            continue;
        Result.push_back(Objects::RefTo(Object));
    }
    return Result;
}

bool AnyStationPanelOpen(const std::vector<Objects::Ref>& Panels)
{
    return std::any_of(Panels.begin(), Panels.end(), [](const Objects::Ref& Panel) { return Objects::IsLive(Panel) && PanelOpen(Panel.Object); });
}

// Client: Im Lager beginnt die erste Aufwertung (werkbank-client), der erste Verkauf oder die erste Reparatur (lager-handel-client)
// eine lokale Bearbeitung, die bis zum Bestätigen läuft. Vorher bleibt das Lager bei den nativen RPCs (rundenende-client).
// opt-werkbank-station: Ohne Rundenende beginnt dasselbe eine Stations-Bearbeitung, wenn ein Inventar-Panel offen ist.
// opt-haendler-client: Ein Kauf (Shop) kommt aus dem offenen Händler-Panel und braucht kein offenes Inventar-Panel.
// Solange der Client nach einem übernommenen Edit noch lokale Instanzen hält, beginnt keine neue Bearbeitung (ClientSettling).
void BeginPanelEdit(uintptr_t Inventory, const char* What, bool Shop = false, bool Quiet = false)
{
    if (Game::InventoryEdit::ClientEditing() || ClientPending || !Shown.Items.empty() || !Inventory ||
        Inventory != InventoryOfController(LocalController()))
        return;
    if (Game::InventoryEdit::ClientSettling())
    {
        if (!Quiet)
            Core::Log::Info("Inventar lokal: {} wartet, der Stand des Hosts zum letzten Edit ist noch nicht repliziert", What);
        return;
    }
    if (End.Id)
    {
        if (End.Done == End.Id || End.HeldPanel)
            return;
        Game::InventoryEdit::BeginClient(Inventory, {}, LocalController());
        LagerEdit = End.Id;
        Core::Log::Info("Lager: {} beim Client, Lager wird lokal bearbeitet bis zum Bestätigen", What);
        return;
    }
    std::vector<Objects::Ref> Panels = StationPanels(Inventory);
    if (!Shop && !AnyStationPanelOpen(Panels))
    {
        if (!Quiet)
            Core::Log::Warn("Station: {} beim Client ohne offenes Panel ({} Panels zum Inventar), keine Bearbeitung", What, Panels.size());
        return;
    }
    Game::InventoryEdit::BeginClient(Inventory, {}, LocalController());
    Station = {};
    Station.Active = true;
    Station.World = CurrentWorld();
    Station.Inventory = Inventory;
    Station.Panels = std::move(Panels);
    Station.NextScan = GetTickCount64() + 2000;
    Core::Log::Info("Station: {} beim Client, Inventar wird lokal bearbeitet, bis das Panel schließt ({} Panels)", What, Station.Panels.size());
}

// Stations-Bearbeitung abschließen: Der Endstand geht als Edit an den Host.
void FinishStation(const char* Why)
{
    if (!Station.Active)
        return;
    Station = {};
    if (!Game::InventoryEdit::ClientEditing() || ClientPending)
        return;
    ClientPending = Game::InventoryEdit::FinishClient(0, 0, std::nullopt, 0);
    Core::Log::Info("Station: Bearbeitung beendet ({}), Edit {}", Why, ClientPending ? std::to_string(ClientPending->Id) : std::string("ohne Änderung"));
}

// Stations-Bearbeitung verwerfen, etwa nach einem Kartenwechsel. Lebt das Inventar nicht mehr, gibt es nichts wiederherzustellen.
void DropStation(const char* Why)
{
    if (!Station.Active)
        return;
    const bool InventoryAlive = Alive(Station.Inventory) && Objects::IsA(Station.Inventory, "InventoryComponent");
    Station = {};
    if (!Game::InventoryEdit::ClientEditing() || ClientPending)
        return;
    Core::Log::Warn("Station: Bearbeitung verworfen ({})", Why);
    if (InventoryAlive)
        Game::InventoryEdit::RevertClient(Why);
    else
        Game::InventoryEdit::ForgetClient();
}

// Client, je Takt: Die Stations-Bearbeitung endet, sobald kein Panel der Station mehr offen ist, nicht mitten in einem Drag.
void TickStation()
{
    if (!Station.Active)
        return;
    if (Station.World != CurrentWorld())
    {
        DropStation("Welt gewechselt");
        return;
    }
    if (!Game::InventoryEdit::ClientEditing())
    {
        Station = {}; // anderswo beendet (Ablehnung, Verbindung weg)
        return;
    }
    const ULONGLONG Now = GetTickCount64();
    if (Now < Station.NextPoll)
        return;
    Station.NextPoll = Now + 150;
    if (Now >= Station.NextScan)
    {
        Station.NextScan = Now + 2000;
        Station.Panels = StationPanels(Station.Inventory);
    }
    if (!Station.Closed && AnyStationPanelOpen(Station.Panels))
        return;
    for (const Objects::Ref& Panel : Station.Panels)
        if (Objects::IsLive(Panel) && Memory::Read<uint8_t>(Panel.Object + PanelIsDragging).value_or(0))
            return;
    FinishStation(Station.Closed ? "Inventar-Panel geschlossen" : "kein Panel mehr offen");
}

// lager-handel-client: HandleItemSold beim Client. ItemRepresentation.ConfirmSell schreibt den Preis lokal gut und ruft es
// direkt; es nimmt das Item über die Inventar-RPCs heraus (RemoveFromInventory mit SoldForScrap). Ohne Eingriff entfernte der
// Host das Item, ohne Schrott zu buchen. Jetzt läuft es nur in einer lokalen Bearbeitung: Der Edit meldet den Verkauf, der Host
// prüft und bucht. Die lokale Gutschrift ist nur Vorschau, danach gilt wieder der Schrott des Hosts (ApplyScrap).
void __fastcall SoldHook(uintptr_t Inventory, uintptr_t Item)
{
    if (CurrentRole() == NetRole::Client)
    {
        BeginPanelEdit(Inventory, "Verkauf");
        if (!Game::InventoryEdit::ClientEditingInventory(Inventory))
        {
            // Außerhalb einer Bearbeitung (etwa ohne Rundenende) verkauft der Client nicht: Der Host buchte keinen Schrott.
            Core::Log::Warn("Verkauf beim Client außerhalb einer Bearbeitung verworfen ({}, Aufrufer {:#x})", Memory::NameOf(Pointer(Item + ItemData)),
                            reinterpret_cast<uintptr_t>(_ReturnAddress()) - reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)));
            return;
        }
        Core::Log::Info("Verkauf beim Client lokal: {}", Memory::NameOf(Pointer(Item + ItemData)));
    }
    OriginalSold(Inventory, Item);
}

// werkbank-client: UpgradeItems beim Client. Ablegen auf ein gleiches Item und die Werkbank rufen es direkt, es legt die
// Ausgabe lokal an (CreateItemInstance) und schickt das Entfernen der Eingaben sonst als Server-RPC: Der Host verlor die
// Eingaben, die Ausgabe kannte er nicht. Jetzt läuft es nur innerhalb einer lokalen Bearbeitung; der Edit meldet die
// Aufwertung, der Host führt sie selbst aus. Im Lager beginnt die erste Aufwertung eine Bearbeitung bis zum Bestätigen.
uintptr_t __fastcall UpgradeHook(uintptr_t Inventory, ItemArray* Inputs, uintptr_t Data, uintptr_t Rarity, bool FlagAsNew, int32_t Position)
{
    if (CurrentRole() != NetRole::Client)
        return OriginalUpgrade(Inventory, Inputs, Data, Rarity, FlagAsNew, Position);
    std::vector<uintptr_t> In;
    for (int32_t i = 0; Inputs && Inputs->Data && i < Inputs->Count && i < 16; ++i)
        In.push_back(Inputs->Data[i]);
    const uintptr_t Caller = reinterpret_cast<uintptr_t>(_ReturnAddress()) - reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    BeginPanelEdit(Inventory, "Aufwertung");
    if (!Game::InventoryEdit::ClientEditingInventory(Inventory))
    {
        Core::Log::Warn("Aufwertung beim Client außerhalb einer Bearbeitung ({} Eingaben, Aufrufer {:#x}), läuft nativ", In.size(), Caller);
        return OriginalUpgrade(Inventory, Inputs, Data, Rarity, FlagAsNew, Position);
    }
    const uintptr_t Output = OriginalUpgrade(Inventory, Inputs, Data, Rarity, FlagAsNew, Position);
    Core::Log::Info("Aufwertung beim Client lokal (Aufrufer {:#x}), Ausgabe {}", Caller, Output != 0);
    Game::InventoryEdit::RecordUpgrade(In, Output, Data, Rarity);
    return Output;
}

// opt-haendler-client: TryBuyItem beim Client. Nativ zahlt das Panel lokal und schickt AddToInventory mit der lokalen Instanz des
// Shops: Beim Host kam ein Nullzeiger an, kein Item, kein Schrott gebucht. Jetzt läuft der Kauf nur in einer lokalen Bearbeitung
// (Station oder Lager): Das Item liegt lokal im Inventar, der Edit meldet Daten, Seltenheit und Preis, der Host legt das Item an
// und bucht. Der lokale Abzug ist nur Vorschau. Ohne offenes Inventar-Panel geht der Kauf sofort als eigener Edit an den Host: Das
// Händler-Panel meldet sein Schließen nie, und eine lange offene Bearbeitung hielte die übrigen Server-RPCs des Clients zurück.
// Automaten (ManuallyCallClearAndAdd) legen das Item erst später ab und bleiben nativ.
void __fastcall TryBuyHook(uintptr_t ShopItem)
{
    if (CurrentRole() != NetRole::Client)
        return OriginalTryBuy(ShopItem);
    const uintptr_t Item = Pointer(ShopItem + ShopItemInstance);
    if (!Alive(Item))
        return OriginalTryBuy(ShopItem); // nativ ohne Wirkung
    const uintptr_t Parent = Pointer(ShopItem + ButtonParentPanel);
    // Nur der Händler (WI_MerchantPanel_C). Der dubiose Händler hat einen eigenen Ablauf und ist nicht geprüft, er bleibt nativ.
    const bool IsShop = Alive(Parent) && Objects::IsA(Parent, "WI_MerchantPanel_C");
    if (Memory::Read<uint8_t>(ShopItem + ShopItemManualAdd).value_or(1) || !IsShop)
    {
        static unsigned Logged = 0;
        if (++Logged <= 5)
            Core::Log::Warn("Kauf beim Client in {} nicht abgedeckt (Automat, dubioser Händler oder unbekanntes Panel), läuft nativ",
                            Alive(Parent) ? Memory::NameOf(Pointer(Parent + Offsets::UObject::Class)) : std::string("?"));
        return OriginalTryBuy(ShopItem);
    }
    const uintptr_t Inventory = InventoryOfController(LocalController());
    BeginPanelEdit(Inventory, "Kauf", true);
    if (!Game::InventoryEdit::ClientEditingInventory(Inventory))
    {
        // Etwa solange ein Edit unbeantwortet ist: Der Host buchte nichts, das Item gäbe es nur beim Client.
        Core::Log::Warn("Kauf beim Client außerhalb einer Bearbeitung verworfen ({})", Memory::NameOf(Pointer(Item + ItemData)));
        return;
    }
    const uintptr_t Display = Pointer(ShopItem + ShopItemCostDisplay);
    const int32_t Cost = Alive(Display) ? Memory::Read<int32_t>(Display + CostDisplayActual).value_or(-1) : 0;
    const uintptr_t Data = Pointer(Item + ItemData), Rarity = Pointer(Item + ItemRarity);
    const auto Has = [&]
    {
        const std::vector<uintptr_t> Items = Game::InventoryEdit::ItemsOf(Inventory);
        return std::find(Items.begin(), Items.end(), Item) != Items.end();
    };
    const bool Had = Has();
    OriginalTryBuy(ShopItem);
    if (!Had && Has())
        Game::InventoryEdit::RecordPurchase(Item, Data, Rarity, Cost);
    else
        Core::Log::Info("Kauf beim Client nicht zustande gekommen ({}, Preis {})", Memory::NameOf(Data), Cost);
    if (Station.Active && !AnyStationPanelOpen(Station.Panels))
        FinishStation("Kauf");
}

// CachedBackpackLocalPosition (+0x150) ist NICHT repliziert. Die echte Instanz
// aus dem Host-Inventar kommt daher mit (0, 0), obwohl ZeroPivotTile und Slots
// korrekt replizieren. SetupInventory zeichnet sonst Revolver und Pistole
// übereinander. Den nativen UI-Snap nutzen, nicht Inventar oder Pixelraster erfinden.
void RestoreBackpackVisuals(uintptr_t Panel)
{
    if (CurrentRole() != NetRole::Client || !Alive(Panel) || Memory::Read<uint8_t>(Panel + PanelIsDragging).value_or(1))
        return;
    constexpr uintptr_t PanelInventory = 0x370;
    const uintptr_t Inventory = Pointer(Panel + PanelInventory);
    const uintptr_t Controller = LocalController();
    if (!Alive(Inventory) || !Controller || Pointer(Inventory + InventoryPlayerState) != Pointer(Controller + ControllerPlayerState))
        return;
    static uintptr_t GetContainer = 0, GetRepresentation = 0, Snap = 0;
    if (!Function(GetContainer, "GetBackpackContainer", "InventoryPanel") ||
        !Function(GetRepresentation, "TryGetItemRepresentation", "InventoryPanel") ||
        !Function(Snap, "SnapRepresentationToTile", "InventoryPanel"))
        return;
    uintptr_t Container = 0;
    Objects::ProcessEvent(Panel, GetContainer, &Container);
    if (!Alive(Container))
        return;
    const uintptr_t Data = Pointer(Inventory + InventoryItems);
    const int32_t Count = Memory::Read<int32_t>(Inventory + InventoryItems + 8).value_or(-1);
    if (!Data || Count <= 0 || Count > 4096)
        return;
    // opt-rucksack-ueberlappung: Der Snap erwartet die obere linke Ecke der belegten Felder, nicht den Pivot (SnapTile).
    const std::vector<Game::InventoryEdit::SlotInfo> Slots = Game::InventoryEdit::SlotsOf(Inventory);
    unsigned Restored = 0, Moved = 0;
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Item = Pointer(Data + 8 * i);
        if (!Alive(Item) || IsReward(Item) || !Memory::Read<uint8_t>(Item + ItemInBackpack).value_or(0))
            continue;
        std::vector<Game::InventoryEdit::Tile> Tiles;
        for (const auto& Slot : Slots)
            if (Slot.Occupier == Item)
                Tiles.push_back(Slot.At);
        struct
        {
            uintptr_t Item, Representation;
            bool Found;
            uint8_t Pad[7];
        } Lookup{Item, 0, false, {}};
        static_assert(sizeof(Lookup) == 0x18);
        Objects::ProcessEvent(Panel, GetRepresentation, &Lookup);
        if (!Lookup.Found || !Alive(Lookup.Representation))
            continue;
        struct
        {
            uintptr_t Representation;
            int32_t X, Y;
            uintptr_t Container;
        } Params{Lookup.Representation, 0, 0, Container};
        static_assert(sizeof(Params) == 0x18);
        if (!Memory::ReadBytes(Item + ItemPivotTile, &Params.X, 8))
            continue;
        const Game::InventoryEdit::Tile Corner = Game::InventoryEdit::SnapTile({Params.X, Params.Y}, Tiles);
        Moved += Corner != Game::InventoryEdit::Tile{Params.X, Params.Y};
        Params.X = Corner.first;
        Params.Y = Corner.second;
        Objects::ProcessEvent(Panel, Snap, &Params);
        ++Restored;
    }
    if (Restored)
        Core::Log::Info("Airdrop-Auswahl: {} Rucksack-Anzeige(n) aus replizierten Feldern positioniert, {} davon mit Ecke abseits des Pivots", Restored,
                        Moved);
}

// airdrop-dodge-absturz: SetupInventory liest das Inventar des Panels (+0x370) ohne Prüfung, und keiner der acht nativen Aufrufer
// prüft es vorher, darunter Ereignis-Handler. Dodgte der Client in einen Airdrop, kam der Aufruf manchmal ohne Inventar und
// stürzte ab. Ohne Inventar gibt es nichts aufzubauen; der nächste Aufruf mit Inventar holt es nach. Der Aufrufer steht im Log.
std::atomic<unsigned> SetupWithoutInventory = 0;

void __fastcall SetupInventoryHook(uintptr_t Panel)
{
    constexpr uintptr_t PanelInventory = 0x370;
    if (!Panel || !Memory::Read<uintptr_t>(Panel + PanelInventory).value_or(0))
    {
        if (SetupWithoutInventory.fetch_add(1) < 10)
            Core::Log::Warn("Inventar: SetupInventory ohne Inventar ausgelassen, Panel {:#x}, Aufrufer {:#x}", Panel,
                            reinterpret_cast<uintptr_t>(_ReturnAddress()) - reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)));
        return;
    }
    OriginalSetupInventory(Panel);
    RestoreBackpackVisuals(Panel);
}

bool CheckSetRewards()
{
    return Code::Expect(At(SetRewardItems), {0x48, 0x89, 0x5C, 0x24, 0x18, 0x48, 0x89, 0x54, 0x24, 0x10}, "InventoryComponent.SetRewardItems");
}
bool CheckConfirm()
{
    if (!Code::Expect(At(CreateItemInstance), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x55, 0x56, 0x57, 0x48, 0x81, 0xEC}, "InventoryComponent::CreateItemInstance"))
        return false;
    // Dazu der native Drag-Test (cmp byte ptr [rdi+IsDragging],0), damit PanelIsDragging zum Build passt.
    static_assert(PanelIsDragging == 0x788, "Drag-Test unten anpassen");
    return Code::Expect(At(PanelConfirm), {0x48, 0x89, 0x5C, 0x24, 0x18, 0x55, 0x56, 0x57, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x8B, 0xF9},
                        "InventoryPanel.HandleConfirmButton") &&
           Code::Expect(At(PanelConfirm, 0x4A), {0x80, 0xBF, 0x88, 0x07, 0x00, 0x00, 0x00},
                        "InventoryPanel.HandleConfirmButton, IsDragging") &&
           Code::Expect(At(MemoryFree), {0x48, 0x85, 0xC9, 0x74, 0x2E}, "FMemory::Free");
}
bool ApplySetRewards(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(SetRewardItems)), reinterpret_cast<void*>(&SetRewardsHook),
                                reinterpret_cast<void**>(&OriginalSetRewards));
}
bool ApplyConfirm(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(PanelConfirm)), reinterpret_cast<void*>(&ConfirmHook),
                                reinterpret_cast<void**>(&OriginalConfirm));
}
bool CheckSetupInventory()
{
    return Code::Expect(At(PanelSetupInventory), {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18}, "InventoryPanel.SetupInventory");
}
bool ApplySetupInventory(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(PanelSetupInventory)), reinterpret_cast<void*>(&SetupInventoryHook),
                                reinterpret_cast<void**>(&OriginalSetupInventory));
}
bool CheckUpgrade()
{
    return Code::Expect(At(UpgradeItems), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x4C, 0x89, 0x4C, 0x24, 0x20, 0x55, 0x56, 0x57}, "InventoryComponent::UpgradeItems");
}
bool ApplyUpgrade(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(UpgradeItems)), reinterpret_cast<void*>(&UpgradeHook),
                                reinterpret_cast<void**>(&OriginalUpgrade));
}
bool CheckSold()
{
    // Beide Builds gleich: Prolog, dann xor r8d,r8d; mov rbx,rdx; mov rdi,rcx vor RemoveFromBackpack.
    return Code::Expect(At(HandleItemSold), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x45, 0x33, 0xC0, 0x48, 0x8B, 0xDA},
                        "InventoryComponent::HandleItemSold");
}
bool ApplySold(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(HandleItemSold)), reinterpret_cast<void*>(&SoldHook),
                                reinterpret_cast<void**>(&OriginalSold));
}
bool CheckTryBuy()
{
    // Prolog, dann cmp qword ptr [rcx+ItemInstance],0: belegt zugleich den Offset der Instanz.
    static_assert(ShopItemInstance == 0x868, "Prüfbytes unten anpassen");
    return Code::Expect(At(TryBuyItem), {0x40, 0x57, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x83, 0xB9, 0x68, 0x08, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xF9},
                        "ShopPanelItem::TryBuyItem");
}
bool ApplyTryBuy(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(TryBuyItem)), reinterpret_cast<void*>(&TryBuyHook),
                                reinterpret_cast<void**>(&OriginalTryBuy));
}
// Je Patch die benutzten Adressen der Signaturtabelle, erzeugt von tools/patch_signatures.py sites.
constexpr uintptr_t SetRewardsSites[] = {MemoryFree, SetRewardItems};
constexpr uintptr_t UpgradeSites[] = {UpgradeItems};
constexpr uintptr_t SoldSites[] = {HandleItemSold};
constexpr uintptr_t TryBuySites[] = {TryBuyItem};
constexpr uintptr_t ConfirmSites[] = {MemoryFree, CreateItemInstance, PanelConfirm};
constexpr uintptr_t SetupInventorySites[] = {PanelSetupInventory};
const Patch List[] = {
    {"Airdrop-Auswahl: Choices des Hosts mitlesen", CheckSetRewards, ApplySetRewards, SetRewardsSites},
    {"Aufwerten beim Client nur in lokaler Bearbeitung, Host übernimmt", CheckUpgrade, ApplyUpgrade, UpgradeSites},
    {"Verkaufen beim Client nur in lokaler Bearbeitung, Host bucht", CheckSold, ApplySold, SoldSites},
    {"Kaufen beim Händler nur in lokaler Bearbeitung, Host legt an und bucht", CheckTryBuy, ApplyTryBuy, TryBuySites},
    {"Airdrop-Auswahl: Bestätigen bis zur Wahl des Partners halten", CheckConfirm, ApplyConfirm, ConfirmSites},
    {"Airdrop-Auswahl: Rucksack-Anzeige aus replizierten Feldern aufbauen", CheckSetupInventory, ApplySetupInventory, SetupInventorySites},
};
}

std::span<const Patch> RewardPatches()
{
    return List;
}

// Aus RewardChest.OpenAirdropChoices beim Host: Öffnet die Figur des Remote-Spielers die Kiste, bekommt die Figur des
// Hosts die native Auswahl, der Client seine über den Kontrollkanal.
// Host: Inventar-RPCs eines Clients mit einer Instanz, die es beim Host nicht gibt, kommen mit Null an. Die nativen
// Implementierungen lesen sie ungeprüft (Absturz in AddToBackpack). Solche Aufrufe verwirft die Mod.
bool NullItemRpc(uintptr_t Function, void* Params)
{
    static uintptr_t Cache[6]{};
    static const char* const Names[6]{"AddToBackpack",       "AddToInventory",           "RemoveFromInventory",
                                      "OccupyBackpackSlots", "ForceEquipItemInBackpack", "ApplyAddToBackpackEffect"};
    for (size_t i = 0; i < 6; ++i)
        if (Function && Function == Function_(Cache[i], Names[i]))
        {
            if (Params && !Memory::Read<uintptr_t>(reinterpret_cast<uintptr_t>(Params)).value_or(1))
            {
                Core::Log::Warn("Airdrop-Auswahl: {} ohne Item vom Client verworfen", Names[i]);
                return true;
            }
            return false;
        }
    return false;
}

// lager-handel-client, Host: RepairItem am Inventar des Clients kommt nur noch aus dem Edit (HostApplyingEdit). Ein direkter
// Aufruf vom Client reparierte ohne Schrott, denn der Client hatte nur lokal abgezogen; er wird verworfen. Das eigene Inventar
// des Hosts bleibt unberührt.
bool HostApplyingEdit = false;
bool ClientTradeRpc(uintptr_t Object, uintptr_t Function)
{
    static uintptr_t Repair = 0;
    if (!Function || Function != Function_(Repair, "RepairItem") || HostApplyingEdit)
        return false;
    uintptr_t Coop = 0, HostController = 0, Remote = 0;
    if (!BoundRemoteP2(Coop, HostController, Remote) || Object != InventoryOfController(Remote))
        return false;
    Core::Log::Warn("Reparatur: RepairItem direkt vom Client verworfen, Reparaturen kommen im Edit");
    return true;
}

// opt-station-zug, Client: Mission ohne MetaBase. Die Station (Händler-Event) ist eine Missionskarte mit DDMissionGameState.
bool InMissionWorld()
{
    const uintptr_t World = CurrentWorld();
    const uintptr_t State = World ? Pointer(World + Offsets::UWorld::GameState) : 0;
    if (!Alive(State) || !Objects::IsA(State, "DDMissionGameState"))
        return false;
    // Name der Welt wie in der Logzeile "Welt: …": MetaBase, DDDynamic_Map, MainMenu.
    return Memory::NameOf(World).find("MetaBase") == std::string::npos;
}

// opt-station-zug, Client: Ein Inventar-RPC außerhalb jeder Bearbeitung. Trifft er das eigene Inventar, beginnt er im Lager oder an
// einer Station eine lokale Bearbeitung; RouteClientRpc führt ihn danach lokal aus. Das eigene Inventar und die Panels zu suchen
// läuft über das Objekt-Array: Das Inventar wird gemerkt, ein gescheiterter Versuch an einer Station erst nach 250 ms wiederholt.
void BeginMoveEdit(uintptr_t Object, uintptr_t Function)
{
    static Objects::Ref OwnInventory;
    static uintptr_t OwnController = 0;
    static ULONGLONG NextStationTry = 0;
    static std::vector<uintptr_t> Reported;
    const ClientMove Move = ClientMoveBegins(!Game::InventoryEdit::ClientIdle(), ClientPending.has_value(), !Shown.Items.empty(), End.Id, End.Done,
                                             End.HeldPanel != 0, End.Id || InMissionWorld());
    if (Move == ClientMove::Native)
        return;
    const uintptr_t Controller = LocalController();
    if (!Controller)
        return;
    if (Controller != OwnController || !Objects::IsLive(OwnInventory))
    {
        OwnController = Controller;
        OwnInventory = Objects::RefTo(InventoryOfController(Controller));
    }
    if (Objects::IsLive(OwnInventory) && Pointer(OwnInventory.Object + InventoryPlayerState) != Pointer(Controller + ControllerPlayerState))
        OwnInventory = Objects::RefTo(InventoryOfController(Controller));
    const uintptr_t Own = Objects::IsLive(OwnInventory) ? OwnInventory.Object : 0;
    if (!Own || !Game::InventoryEdit::ChangesInventory(Object, Function, Own))
        return;
    const ULONGLONG Now = GetTickCount64();
    if (Move == ClientMove::BeginLager)
        BeginPanelEdit(Own, "Zug im Rucksack");
    else if (Now >= NextStationTry)
    {
        BeginPanelEdit(Own, "Zug im Rucksack", false, true);
        if (!Game::InventoryEdit::ClientEditing())
            NextStationTry = Now + 250;
    }
    if (Game::InventoryEdit::ClientEditing() || std::find(Reported.begin(), Reported.end(), Function) != Reported.end())
        return;
    if (Reported.size() < 64)
        Reported.push_back(Function);
    Core::Log::Warn("Inventar lokal: {} des Clients außerhalb einer Bearbeitung geht nativ an den Host ({})", Memory::NameOf(Function),
                    Move == ClientMove::BeginLager ? "Lager" : "Mission ohne offenes Inventar-Panel oder Stand des letzten Edits noch nicht repliziert");
}

bool FilterRewardEvent(uintptr_t Object, uintptr_t Function, void* Params)
{
    if (CurrentRole() == NetRole::Host)
        return RemoteReady.load(std::memory_order_relaxed) && (NullItemRpc(Function, Params) || ClientTradeRpc(Object, Function));
    NotePanelState(Object, Function, Params);
    // lager-handel-client: ItemRepresentation.ConfirmRepair zieht den Preis lokal ab und schickt dann RepairItem. Im Lager beginnt
    // die erste Reparatur eine lokale Bearbeitung, RouteClientRpc führt sie dann lokal aus und der Edit meldet sie.
    static uintptr_t Repair = 0;
    if (Function && Function == Function_(Repair, "RepairItem") && !Game::InventoryEdit::ClientEditing())
        BeginPanelEdit(Object, "Reparatur");
    // opt-rucksack-ueberlappung: Auch der erste Zug im Lager (Ablegen, Verschieben, Herausnehmen) beginnt die lokale Bearbeitung.
    // Als native RPCs kamen beim Host nur Feldbelegung und IsInBackpack an, Pivot und Drehung blieben (0, 0) und 0, und der Client
    // bekam im Lager nichts zurück: Sein eigener Stand zeigte die Items weiter im Lager.
    // Erst die Funktion einordnen: Das eigene Inventar zu suchen läuft über das ganze Objekt-Array.
    // opt-station-zug: Dasselbe ohne Rundenende, an einer Station mit offenem Inventar-Panel (ClientMoveBegins). Was danach noch
    // nativ zum Host geht (kein offenes Panel, Stand des letzten Edits noch nicht repliziert), steht je Funktion einmal im Log.
    // Nur bei ruhender Sitzung: Während des Zurückrollens laufen verschachtelte Inventar-RPCs hier durch, sie dürfen nichts beginnen.
    if (Game::InventoryEdit::ClientIdle() && !ClientPending && Shown.Items.empty() && Game::InventoryEdit::MayChangeInventory(Function))
        BeginMoveEdit(Object, Function);
    if (Function && Function == Repair && !Game::InventoryEdit::ClientEditingInventory(Object))
    {
        // Außerhalb einer Bearbeitung reparierte der Host ohne Schrott. Der Client behält das Item kaputt.
        Core::Log::Warn("Reparatur beim Client außerhalb einer Bearbeitung verworfen");
        return true;
    }
    return Game::InventoryEdit::RouteClientRpc(Object, Function, Params);
}

void TraceInventoryEvent(uintptr_t Object, uintptr_t Function, void* Params)
{
    static const bool Enabled = []
    {
        char Value[4]{};
        return GetEnvironmentVariableA("DDCOOP_TRACE_INVENTORY", Value, sizeof(Value)) == 1 && Value[0] == '1';
    }();
    if (!Enabled || !Function)
        return;
    // Je Funktion einmal entscheiden: Besitzerklasse der UFunction gehört zum Inventar.
    static std::unordered_map<uintptr_t, std::string> Names;
    auto It = Names.find(Function);
    if (It == Names.end())
    {
        const std::string Owner = Memory::NameOf(Pointer(Function + Offsets::UObject::Outer));
        const std::string Name = Memory::NameOf(Function);
        // waffe-client-auswahl: Effekte der Items und Ausrüsten der Waffen gehören dazu.
        const bool Wanted = Owner == "InventoryComponent" || Owner == "InventoryItemInstance" || Owner == "BackpackSlot" ||
                            Owner == "InventoryPanel" || Owner == "RewardQueueComponent" || Owner == "CoopInventoryCoordinatorComponent" ||
                            Owner == "CharacterEffectManager" || (Owner == "DDCharacter" && Name.find("EquippedWeapon") != std::string::npos) ||
                            Owner.starts_with("WP_Inventory") || Owner.starts_with("WP_Backpack") || Owner.starts_with("WP_Item") ||
                            // opt-werkbank-station, opt-haendler-client: Händler, Rezepte, Öffnen und Schließen der Panels, Währung.
                            Owner == "ShopPanel" || Owner == "ShopPanelItem" || Owner == "AbstractPanel" || Owner == "TransactionPanel" ||
                            Owner == "RecipePanel" || Owner == "RecipeButton" || Owner == "PanelAnimationContainer" || Owner == "AbstractCurrency" ||
                            Owner == "TimescaleManager" || Owner == "CostDisplay" || Owner.starts_with("WI_Merchant") ||
                            Owner.starts_with("WP_Recipe") || Owner.starts_with("BP_MetaBase_POI") || Owner == "MetaBasePoi";
        // Animation und Abfragen je Bild nicht, sie verdrängen die eigentlichen Vorgänge.
        const bool Noise = Name.starts_with("ApplyEasing") || Name.starts_with("OnYoyo") || Name.starts_with("OnLoop") ||
                           Name == "GetAnimationContainer" || Name.ends_with("Tick") || Name.starts_with("Get") || Name.starts_with("Is") ||
                           Name.starts_with("Can") || Name.starts_with("Has");
        It = Names.emplace(Function, Wanted && !Noise ? Owner + "." + Name : std::string{}).first;
    }
    if (It->second.empty())
        return;
    static unsigned Lines = 0;
    if (++Lines > 20000)
        return;
    const uint32_t Flags = Memory::Read<uint32_t>(Function + FunctionFlags).value_or(0);
    const uintptr_t First = Params ? Memory::Read<uintptr_t>(reinterpret_cast<uintptr_t>(Params)).value_or(0) : 0;
    const std::string FirstName = First > 0x10000 && Memory::Read<uint32_t>(First + Offsets::UObject::Flags) ? Memory::NameOf(First) : std::string{};
    // Zusatz: Style von RemoveFromBackpack (+0x8), Item und AllowRetainEffect aus FItemEffectContext (+0x8, +0x30).
    std::string Extra;
    const uintptr_t At = reinterpret_cast<uintptr_t>(Params);
    if (Params && It->second == "InventoryComponent.RemoveFromBackpack")
        Extra = ", Style " + std::to_string(Memory::Read<uint8_t>(At + 8).value_or(0xFF));
    else if (Params && It->second.starts_with("CharacterEffectManager."))
    {
        const uintptr_t Item = Memory::Read<uintptr_t>(At + 8).value_or(0);
        Extra = ", Item " + (Item ? Memory::NameOf(Pointer(Item + 0xE0)) : std::string("-"));
        if (It->second == "CharacterEffectManager.RemoveItemFromBackpack")
            Extra += ", AllowRetain " + std::to_string(Memory::Read<uint8_t>(At + 0x30).value_or(0xFF));
    }
    Core::Log::Info("InvTrace {} {}{} an {:#x} ({}), erster Parameter {:#x} {}{}", CurrentRole() == NetRole::Client ? "C" : "H", It->second,
                    Flags & FuncNet ? " [Net]" : "", Object, Memory::NameOf(Object), First, FirstName, Extra);
}

uintptr_t RewardCharacter(uintptr_t Character)
{
    uintptr_t Coop = 0, HostController = 0, Remote = 0;
    if (CurrentRole() != NetRole::Host || !RemoteReady.load() || !BoundRemoteP2(Coop, HostController, Remote))
        return Character;
    const uintptr_t RemotePawn = Pointer(Remote + ControllerPawn), HostPawn = Pointer(HostController + ControllerPawn);
    if (Character != RemotePawn || !HostPawn)
        return Character;
    Core::Log::Info("Airdrop-Auswahl: Kiste vom Remote-Spieler geöffnet, Auswahl über die Figur des Hosts");
    return HostPawn;
}

namespace
{
// lager-handel-client: Schrottkonto der Mission (ADDMissionGameState.ScrapWallet, dasselbe holt der native Getter am
// Inventar), 0 außerhalb einer Mission.
uintptr_t ScrapWallet()
{
    const uintptr_t World = CurrentWorld();
    const uintptr_t State = World ? Pointer(World + Offsets::UWorld::GameState) : 0;
    if (!Alive(State) || !Objects::IsA(State, "DDMissionGameState"))
        return 0;
    const uintptr_t Wallet = Pointer(State + GameStateScrapWallet);
    return Alive(Wallet) && Objects::IsA(Wallet, "ScrapWalletComponent") ? Wallet : 0;
}

std::optional<int32_t> ScrapAmount(uintptr_t Wallet)
{
    return Wallet ? Memory::Read<int32_t>(Wallet + CurrencyAmount) : std::nullopt;
}

// Natives AbstractCurrency.AddAmount: begrenzt auf 0 bis INT32_MAX und löst OnCurrencyChanged aus (Anzeige), sonst nichts.
void AddScrap(uintptr_t Wallet, int32_t Delta)
{
    static uintptr_t Fn = 0;
    if (!Delta || !Alive(Wallet) || !Function(Fn, "AddAmount", "AbstractCurrency"))
        return;
    int32_t Params = Delta;
    Objects::ProcessEvent(Wallet, Fn, &Params);
}

// Native Preise, beim Host mit seiner Seltenheit des Items. -1, wenn die Funktion fehlt.
int32_t SellPrice(uintptr_t Item)
{
    static uintptr_t Fn = 0;
    if (!Alive(Item) || !Function(Fn, "GetDynamicSellPrice", "InventoryItemInstance"))
        return -1;
    int32_t Params = -1;
    Objects::ProcessEvent(Item, Fn, &Params);
    return Params;
}

int32_t RepairPrice(uintptr_t Item)
{
    static uintptr_t Fn = 0;
    if (!Alive(Item) || !Function(Fn, "GetDynamicRepairPrice", "InventoryItemInstance"))
        return -1;
    struct
    {
        uintptr_t World;
        int32_t ReturnValue;
        uint8_t Pad[4];
    } Params{CurrentWorld(), -1, {}};
    static_assert(sizeof(Params) == 0x10);
    Objects::ProcessEvent(Item, Fn, &Params);
    return Params.ReturnValue;
}

// opt-haendler-client: Verkaufspreis eines Items dieser Art beim Host (UInventoryItemData.GetBaseSellPrice), -1 ohne Funktion.
// Daraus die Untergrenze für den Kaufpreis, den der Client meldet (PurchaseFloor in InventoryEditPlan.h).
int32_t BaseSellPrice(uintptr_t Data, uintptr_t Rarity)
{
    static uintptr_t Fn = 0;
    if (!Alive(Data) || !Function(Fn, "GetBaseSellPrice", "InventoryItemData"))
        return -1;
    struct
    {
        uintptr_t Rarity;
        int32_t ReturnValue;
        uint8_t Pad[4];
    } Params{Rarity, -1, {}};
    static_assert(sizeof(Params) == 0x10);
    Objects::ProcessEvent(Data, Fn, &Params);
    return Params.ReturnValue;
}

bool IsBroken(uintptr_t Item)
{
    return Memory::Read<uint8_t>(Item + ItemBroken).value_or(0) != 0;
}

// Ein Verkauf oder eine Reparatur aus einem Edit, mit dem Preis beim Prüfen (vor dem Entfernen).
struct Trade
{
    uintptr_t Item = 0;
    int32_t Price = 0;
    bool Consumed = false; // Eingabe einer Aufwertung desselben Edits, vor ihr repariert
    bool Done = false;     // Consumed: RepairItem lief vor der Aufwertung
};
}
}

namespace Game::Rewards
{
using namespace Game::Patches;

void SetRemoteReady(bool Ready)
{
    if (RemoteReady.exchange(Ready) != Ready)
        Core::Log::Info("Airdrop-Auswahl: Client mit Mod {}", Ready ? "verbunden, Airdrop-Auswahl für beide" : "fehlt, Auswahl nur beim Host");
}

std::optional<Offer> ReadOffer()
{
    if (CurrentRole() != NetRole::Host || !Host.Active)
        return std::nullopt;
    Offer Result;
    Result.Session = Host.Session;
    Result.Revision = Revision(Host.Items);
    Result.Level = Host.Level;
    Result.Choices = Host.Choices;
    Result.ClientClaimed = Host.ClientPick.has_value();
    Result.HostClaimed = false;
    Result.HostWaiting = Host.HeldPanel != 0;
    Result.ClientReady = Host.ClientReady && Host.ReadyRevision == Result.Revision;
    return Result;
}

void SetClientReady(uint64_t Session, uint64_t Expected, bool Ready)
{
    if (CurrentRole() != NetRole::Host || !Host.Active || Session != Host.Session || Host.ClientPick)
        return;
    if (Host.ClientReady != Ready || Host.ReadyRevision != Expected)
        Core::Log::Info("Bestätigen: Client {} (Sitzung {}, Host bereit={})", Ready ? "bereit" : "nicht mehr bereit", Session, Host.HeldPanel != 0);
    Host.ClientReady = Ready;
    Host.ReadyRevision = Expected;
}

// Host: Wahl des Clients in sein Inventar legen (CreateAndAddItem), das Item repliziert. 0, wenn sie nicht passt.
uintptr_t ClaimForClient(uintptr_t Inventory, uint64_t SessionId, uint64_t Expected, uint32_t Index)
{
    if (CurrentRole() != NetRole::Host || !Host.Active || SessionId != Host.Session || Host.ClientPick || Revision(Host.Items) != Expected ||
        Index >= Host.Data.size() || !Host.Data[Index].first)
        return 0;
    static uintptr_t Create = 0;
    if (!Inventory || !Function(Create, "CreateAndAddItem", "InventoryComponent"))
        return 0;
    struct
    {
        uintptr_t Data, Rarity;
        bool Broken, FlagAsNew, SystemAdd;
        uint8_t Pad;
        int32_t Position;
        bool AllowP2Reserve;
        uint8_t Pad2[7];
        uintptr_t Result;
    } Add{Host.Data[Index].first, Host.Data[Index].second, Host.Choices[Index].Broken, true, true, 0, -1, false, {}, 0};
    static_assert(sizeof(Add) == 0x28);
    Objects::ProcessEvent(Inventory, Create, &Add);
    Core::Log::Info("Airdrop-Auswahl: Client wählt {} ({}), ins Inventar des Clients gelegt={}", Index, Host.Choices[Index].Name, Add.Result != 0);
    if (Add.Result)
        Host.ClientPick = Index;
    return Add.Result;
}

// opt-haendler-client, Host: gekauftes Item ins Inventar des Clients legen (CreateAndAddItem), es repliziert wie jede Vergabe.
uintptr_t CreateForClient(uintptr_t Inventory, uintptr_t Data, uintptr_t Rarity)
{
    static uintptr_t Create = 0;
    if (!Inventory || !Data || !Function(Create, "CreateAndAddItem", "InventoryComponent"))
        return 0;
    struct
    {
        uintptr_t Data, Rarity;
        bool Broken, FlagAsNew, SystemAdd;
        uint8_t Pad;
        int32_t Position;
        bool AllowP2Reserve;
        uint8_t Pad2[7];
        uintptr_t Result;
    } Add{Data, Rarity, false, true, true, 0, -1, false, {}, 0};
    static_assert(sizeof(Add) == 0x28);
    Objects::ProcessEvent(Inventory, Create, &Add);
    return Add.Result;
}

// werkbank-client, Host: Art eines Items aus seinen Datenobjekten.
Game::InventoryEdit::ItemKind KindOf(uintptr_t Data, uintptr_t Rarity)
{
    using Game::InventoryEdit::NameHash;
    return {Data ? NameHash(Memory::NameOf(Data)) : 0u, Rarity ? NameHash(Memory::NameOf(Rarity)) : 0u};
}

uint32_t NextRarityOf(uintptr_t Rarity)
{
    const uintptr_t Next = Alive(Rarity) ? Pointer(Rarity + RarityNext) : 0;
    return Next ? Game::InventoryEdit::NameHash(Memory::NameOf(Next)) : 0u;
}

// Rezepte der Werkbank aus jeder DataTable mit RowStruct RecipeTableRow (FRecipeTableRow: OutputItem +0x18,
// OutputRarityOverride +0x20, RequiredItems +0x28 als TArray von {Item, RequiredRarityOverride}). Nur lesend. RowMap ist
// eine TMap<FName, uint8*> bei +0x30: Elemente (0x18 Byte, Wert bei +8) ab +0x30, Anzahl +0x38, Belegungsbits inline ab
// +0x40 oder über den Zeiger bei +0x50, freie Plätze +0x64.
std::vector<Game::InventoryEdit::Recipe> RecipesOfWorkbench()
{
    using namespace Game::InventoryEdit;
    constexpr uintptr_t RowStruct = 0x28, Rows = 0x30, RowsCount = 0x38, Flags = 0x40, FlagsHeap = 0x50, FreeCount = 0x64;
    std::vector<Recipe> Result;
    const int32_t Total = Objects::Count();
    for (int32_t i = 0; i < Total; ++i)
    {
        const uintptr_t Table = Objects::At(i);
        if (!Table || Memory::NameOf(Pointer(Table + Offsets::UObject::Class)) != "DataTable" || !Alive(Table) ||
            Memory::NameOf(Pointer(Table + RowStruct)) != "RecipeTableRow")
            continue;
        const uintptr_t Data = Pointer(Table + Rows);
        const int32_t Count = Memory::Read<int32_t>(Table + RowsCount).value_or(0);
        const bool Holes = Memory::Read<int32_t>(Table + FreeCount).value_or(1) != 0;
        const uintptr_t Bits = Pointer(Table + FlagsHeap) ? Pointer(Table + FlagsHeap) : Table + Flags;
        for (int32_t r = 0; Data && r < Count && r < 1024; ++r)
        {
            if (Holes && !((Memory::Read<uint32_t>(Bits + 4 * (r / 32)).value_or(0) >> (r % 32)) & 1))
                continue;
            const uintptr_t Row = Pointer(Data + 0x18 * r + 8);
            if (!Row)
                continue;
            Recipe Next;
            Next.Output = KindOf(Pointer(Row + 0x18), Pointer(Row + 0x20));
            const uintptr_t Required = Pointer(Row + 0x28);
            const int32_t Needed = Memory::Read<int32_t>(Row + 0x30).value_or(0);
            for (int32_t n = 0; Required && n < Needed && n <= static_cast<int32_t>(MaxUpgradeInputs); ++n)
                Next.Required.push_back(KindOf(Pointer(Required + 0x10 * n), Pointer(Required + 0x10 * n + 8)));
            if (Next.Output.Data && !Next.Required.empty() && Next.Required.size() <= MaxUpgradeInputs)
                Result.push_back(std::move(Next));
        }
    }
    return Result;
}

// Natives InventoryEffectHelperLibrary.CanItemsBeUpgradedTogether, beim Host für zwei Instanzen im Clientinventar.
bool CanUpgradeTogether(uintptr_t First, uintptr_t Second, uintptr_t Context)
{
    static uintptr_t Library = 0, Fn = 0;
    if (!Library)
        Library = Objects::FindNamed("Default__InventoryEffectHelperLibrary");
    if (!Library || !Function(Fn, "CanItemsBeUpgradedTogether", "InventoryEffectHelperLibrary"))
        return false;
    struct
    {
        uintptr_t First, Second, Context;
        bool Result;
        uint8_t Pad[7];
    } Params{First, Second, Context, false, {}};
    static_assert(sizeof(Params) == 0x20);
    Objects::ProcessEvent(Library, Fn, &Params);
    return Params.Result;
}

// Host: Aufwertungen eines Edits prüfen, bevor sich etwas ändert. Live hält je Source die Instanz beim Host (0, solange sie
// noch nicht existiert: Wahl, frühere Ausgabe), Kinds deren Art. Objects bekommt je Aufwertung Daten und Seltenheit.
bool CheckUpgrades(const Game::InventoryEdit::Edit& Value, std::unordered_map<uint16_t, uintptr_t> Live,
                   std::unordered_map<uint16_t, std::pair<Game::InventoryEdit::ItemKind, uint32_t>> Kinds, uintptr_t Inventory,
                   std::vector<std::pair<uintptr_t, uintptr_t>>& Objects_)
{
    using namespace Game::InventoryEdit;
    std::optional<std::vector<Recipe>> Recipes;
    for (size_t K = 0; K < Value.Upgrades.size(); ++K)
    {
        const Upgrade& Next = Value.Upgrades[K];
        const auto Found = ResolveChoices({Game::Rewards::ChoiceItem{{}, Next.Data, Next.Rarity, false}});
        if (!Found[0].first || (!Next.Rarity.empty() && !Found[0].second))
        {
            Core::Log::Warn("Inventar lokal: Aufwertung {} nennt unbekannte Daten {} {}", K + 1, Next.Data, Next.Rarity);
            return false;
        }
        std::vector<ItemKind> In;
        std::vector<uint32_t> NextOf;
        std::vector<uintptr_t> Instances;
        for (const uint16_t Source : Next.Inputs)
        {
            const auto It = Kinds.find(Source);
            if (It == Kinds.end())
            {
                Core::Log::Warn("Inventar lokal: Eingabe {} der Aufwertung {} unbekannt", Source, K + 1);
                return false;
            }
            In.push_back(It->second.first);
            NextOf.push_back(It->second.second);
            Instances.push_back(Live.contains(Source) ? Live[Source] : 0);
        }
        const ItemKind Out = KindOf(Found[0].first, Found[0].second);
        bool Merge = IsMerge(In, NextOf, Out);
        if (Merge && Instances[0] && Instances[1] && !CanUpgradeTogether(Instances[0], Instances[1], Inventory))
        {
            Core::Log::Warn("Inventar lokal: Aufwertung {}: die Eingaben lassen sich nativ nicht zusammen aufwerten", K + 1);
            Merge = false;
        }
        if (!Merge)
        {
            if (!Recipes)
                Recipes = RecipesOfWorkbench();
            if (!IsRecipe(*Recipes, In, Out))
            {
                Core::Log::Warn("Inventar lokal: Aufwertung {} zu {} {} passt weder zum Ablegen noch zu einem der {} Rezepte", K + 1, Next.Data,
                                Next.Rarity, Recipes->size());
                return false;
            }
        }
        Core::Log::Info("Inventar lokal: Aufwertung {} geprüft ({}), {} Eingaben zu {} {}", K + 1, Merge ? "Ablegen" : "Rezept", In.size(), Next.Data,
                        Next.Rarity);
        Kinds[UpgradeSource(K)] = {Out, NextRarityOf(Found[0].second)};
        Objects_.push_back(Found[0]);
    }
    return true;
}

// Host: natives UInventoryComponent.UpgradeItems am Clientinventar. Die Ausgabe repliziert wie jede Vergabe.
uintptr_t UpgradeForClient(uintptr_t Inventory, std::vector<uintptr_t> Inputs, uintptr_t Data, uintptr_t Rarity)
{
    static uintptr_t Fn = 0;
    if (!Function(Fn, "UpgradeItems", "InventoryComponent"))
        return 0;
    struct
    {
        ItemArray Inputs;
        uintptr_t Data, Rarity;
        bool FlagAsNew;
        uint8_t Pad[3];
        int32_t Position;
        uintptr_t Result;
    } Params{{Inputs.data(), static_cast<int32_t>(Inputs.size()), static_cast<int32_t>(Inputs.size())}, Data, Rarity, true, {}, -1, 0};
    static_assert(sizeof(Params) == 0x30);
    Objects::ProcessEvent(Inventory, Fn, &Params);
    return Params.Result;
}

bool ApplyClientEdit(const Game::InventoryEdit::Edit& Value)
{
    using namespace Game::InventoryEdit;
    if (CurrentRole() != NetRole::Host)
        return false;
    if (!WellFormed(Value))
    {
        Core::Log::Warn("Inventar lokal: Edit {} ungültig, abgelehnt", Value.Id);
        return false;
    }
    uintptr_t Coop = 0, HostController = 0, Remote = 0;
    const uintptr_t Inventory = BoundRemoteP2(Coop, HostController, Remote) ? InventoryOfController(Remote) : 0;
    if (!Inventory)
    {
        Core::Log::Warn("Inventar lokal: Inventar des Clients nicht gefunden, Edit {} abgelehnt", Value.Id);
        return false;
    }
    if (Value.Choice && (!Host.Active || Value.Session != Host.Session || Host.ClientPick || Revision(Host.Items) != Value.Revision ||
                         *Value.Choice >= Host.Data.size()))
    {
        Core::Log::Warn("Inventar lokal: Edit {} mit Wahl {} passt nicht zur Sitzung {}, abgelehnt", Value.Id, *Value.Choice, Host.Session);
        return false;
    }
    // Items über Index und Datenname zuordnen; weicht der Index ab, das erste unbenutzte Item mit demselben Namen.
    const std::vector<uintptr_t> Items = ItemsOf(Inventory);
    std::vector<uintptr_t> Resolved(Value.Items.size());
    const auto Free = [&](uintptr_t Candidate) { return std::find(Resolved.begin(), Resolved.end(), Candidate) == Resolved.end(); };
    size_t ChoiceAt = Value.Items.size();
    for (size_t i = 0; i < Value.Items.size(); ++i)
    {
        const ItemState& Item = Value.Items[i];
        if (Item.Source == ChoiceSource)
        {
            ChoiceAt = i;
            continue;
        }
        if (Item.Source < Items.size() && DataHashOf(Items[Item.Source]) == Item.DataHash && Free(Items[Item.Source]))
            Resolved[i] = Items[Item.Source];
        else
            for (const uintptr_t Candidate : Items)
                if (!Resolved[i] && DataHashOf(Candidate) == Item.DataHash && Free(Candidate))
                    Resolved[i] = Candidate;
        if (!Resolved[i])
        {
            Core::Log::Warn("Inventar lokal: Item {} des Edits {} nicht im Inventar des Clients, abgelehnt", Item.Source, Value.Id);
            return false;
        }
    }
    const std::vector<SlotInfo> Slots = SlotsOf(Inventory);
    std::vector<Tile> Existing, Others;
    for (const SlotInfo& Slot : Slots)
    {
        Existing.push_back(Slot.At);
        if (Slot.Occupier && std::find(Resolved.begin(), Resolved.end(), Slot.Occupier) == Resolved.end())
            Others.push_back(Slot.At);
    }
    if (!Fits(Value, Existing, Others))
    {
        Core::Log::Warn("Inventar lokal: Edit {} passt nicht in den Rucksack des Clients, abgelehnt", Value.Id);
        return false;
    }
    // lager-handel-client: Verkäufe und Reparaturen prüfen, bevor sich etwas ändert. Preise rechnet der Host selbst.
    std::vector<Trade> Sells, Repairs;
    const std::set<uint16_t> Consumed = ConsumedSources(Value);
    for (size_t i = 0; i < Value.Items.size(); ++i)
    {
        const ItemState& Want = Value.Items[i];
        if (i == ChoiceAt)
        {
            if (Want.Present && Value.Choice && *Value.Choice < Host.Choices.size() && Want.Broken != Host.Choices[*Value.Choice].Broken)
            {
                Core::Log::Warn("Inventar lokal: Wahl des Edits {} mit anderem Zustand (kaputt {}), abgelehnt", Value.Id, Want.Broken);
                return false;
            }
            continue;
        }
        const bool Broken = IsBroken(Resolved[i]);
        if (Want.Present && Want.Broken && !Broken)
        {
            Core::Log::Warn("Inventar lokal: Item {} des Edits {} soll kaputt werden, abgelehnt", Want.Source, Value.Id);
            return false;
        }
        // inventar-client-abweichung: Auch eine im selben Edit reparierte und dann aufgewertete Eingabe ist eine Reparatur.
        const bool Input = Consumed.contains(Want.Source);
        if ((Want.Present || Input) && !Want.Broken && Broken)
            Repairs.push_back({Resolved[i], RepairPrice(Resolved[i]), Input});
        if (IsSold(Want))
        {
            if (IsReward(Resolved[i]))
            {
                Core::Log::Warn("Inventar lokal: Item {} des Edits {} ist eine Belohnung und nicht verkaufbar, abgelehnt", Want.Source, Value.Id);
                return false;
            }
            Sells.push_back({Resolved[i], SellPrice(Resolved[i])});
        }
    }
    // opt-haendler-client: Käufe prüfen, bevor sich etwas ändert. Daten und Seltenheit müssen beim Host existieren; den Preis nennt
    // das Panel des Clients, der Host verlangt mindestens die Untergrenze zum eigenen Verkaufspreis und genug Schrott.
    std::vector<std::pair<uintptr_t, uintptr_t>> Wares;
    if (!Value.Purchases.empty())
    {
        std::vector<Game::Rewards::ChoiceItem> Names;
        for (const Purchase& Next : Value.Purchases)
            Names.push_back({{}, Next.Data, Next.Rarity, false});
        Wares = ResolveChoices(Names);
        for (size_t K = 0; K < Value.Purchases.size(); ++K)
        {
            const Purchase& Next = Value.Purchases[K];
            if (!Wares[K].first || (!Next.Rarity.empty() && !Wares[K].second))
            {
                Core::Log::Warn("Inventar lokal: Kauf {} des Edits {} nennt unbekannte Daten {} {}, abgelehnt", K + 1, Value.Id, Next.Data, Next.Rarity);
                return false;
            }
            const int32_t Sell = BaseSellPrice(Wares[K].first, Wares[K].second);
            if (Sell < 0 || Next.Cost < PurchaseFloor(Sell))
            {
                Core::Log::Warn("Inventar lokal: Kauf {} des Edits {} ({} {}) für {} liegt unter der Untergrenze {} (Verkaufspreis {} beim Host), abgelehnt",
                                K + 1, Value.Id, Next.Data, Next.Rarity, Next.Cost, Sell < 0 ? -1 : PurchaseFloor(Sell), Sell);
                return false;
            }
            Core::Log::Info("Handel: Kauf {} geprüft, {} {} für {} (Verkaufspreis beim Host {}, Untergrenze {})", K + 1, Next.Data, Next.Rarity,
                            Next.Cost, Sell, PurchaseFloor(Sell));
        }
    }
    const bool Trades = !Sells.empty() || !Repairs.empty() || !Value.Purchases.empty();
    const uintptr_t Wallet = Trades ? ScrapWallet() : 0;
    if (Trades)
    {
        const std::optional<int32_t> Have = ScrapAmount(Wallet);
        std::vector<int32_t> SellPrices, RepairPrices, BuyPrices;
        for (const Purchase& Next : Value.Purchases)
            BuyPrices.push_back(Next.Cost);
        for (const Trade& Next : Sells)
            SellPrices.push_back(Next.Price);
        for (const Trade& Next : Repairs)
            RepairPrices.push_back(Next.Price);
        const auto After = Have ? TradeBalance(*Have, SellPrices, RepairPrices, BuyPrices) : std::nullopt;
        if (!After)
        {
            Core::Log::Warn("Inventar lokal: Edit {} mit {} Verkäufen, {} Reparaturen und {} Käufen abgelehnt, Schrott {} reicht nicht oder Preis ungültig",
                            Value.Id, Sells.size(), Repairs.size(), BuyPrices.size(), Have ? std::to_string(*Have) : std::string("ohne Konto"));
            return false;
        }
        Core::Log::Info("Handel: Edit {} geprüft, {} Verkäufe, {} Reparaturen, {} Käufe, Schrott {} wird {}", Value.Id, Sells.size(), Repairs.size(),
                        BuyPrices.size(), *Have, *After);
    }
    // werkbank-client: Aufwertungen vollständig prüfen, bevor sich etwas ändert.
    std::unordered_map<uint16_t, uintptr_t> Live;
    std::unordered_map<uint16_t, std::pair<ItemKind, uint32_t>> Kinds;
    for (size_t i = 0; i < Value.Items.size(); ++i)
        if (i != ChoiceAt)
        {
            const uintptr_t Rarity = Pointer(Resolved[i] + ItemRarity);
            Live[Value.Items[i].Source] = Resolved[i];
            Kinds[Value.Items[i].Source] = {KindOf(Pointer(Resolved[i] + ItemData), Rarity), NextRarityOf(Rarity)};
        }
    if (Value.Choice)
        Kinds[ChoiceSource] = {KindOf(Host.Data[*Value.Choice].first, Host.Data[*Value.Choice].second), NextRarityOf(Host.Data[*Value.Choice].second)};
    for (size_t K = 0; K < Wares.size(); ++K)
        Kinds[PurchaseSource(K)] = {KindOf(Wares[K].first, Wares[K].second), NextRarityOf(Wares[K].second)};
    std::vector<std::pair<uintptr_t, uintptr_t>> Outputs;
    // Die native Prüfung sieht reparierte Eingaben nur repariert: Kaputt-Flag für die Prüfung kurz löschen, danach zurück.
    const auto SetBroken = [&](uint8_t Flag)
    {
        for (const Trade& Next : Repairs)
            if (Next.Consumed)
                Memory::WriteBytes(Next.Item + ItemBroken, &Flag, sizeof(Flag));
    };
    SetBroken(0);
    const bool UpgradesOk = Value.Upgrades.empty() || CheckUpgrades(Value, Live, Kinds, Inventory, Outputs);
    SetBroken(1);
    if (!UpgradesOk)
    {
        Core::Log::Warn("Inventar lokal: Edit {} mit {} Aufwertungen abgelehnt", Value.Id, Value.Upgrades.size());
        return false;
    }
    if (ChoiceAt < Value.Items.size())
    {
        Resolved[ChoiceAt] = ClaimForClient(Inventory, Value.Session, Value.Revision, *Value.Choice);
        if (!Resolved[ChoiceAt])
        {
            Core::Log::Warn("Inventar lokal: Wahl {} des Edits {} nicht angelegt, abgelehnt", *Value.Choice, Value.Id);
            return false;
        }
        if (DataHashOf(Resolved[ChoiceAt]) != Value.Items[ChoiceAt].DataHash)
            Core::Log::Warn("Inventar lokal: Wahl {} hat beim Client ein anderes Item", *Value.Choice);
    }
    if (ChoiceAt < Value.Items.size())
        Live[ChoiceSource] = Resolved[ChoiceAt];
    bool Ok = true;
    // opt-haendler-client: gekaufte Items anlegen, vor den Aufwertungen, die sie verbrauchen können.
    std::vector<uintptr_t> Bought;
    for (size_t K = 0; K < Wares.size(); ++K)
    {
        Bought.push_back(CreateForClient(Inventory, Wares[K].first, Wares[K].second));
        Core::Log::Info("Handel: Kauf {} des Edits {} beim Host, {} angelegt={}", K + 1, Value.Id, Value.Purchases[K].Data, Bought.back() != 0);
        if (Bought.back())
            Live[PurchaseSource(K)] = Bought.back();
        else
            Ok = false;
    }
    // Aufwertungen in ihrer Reihenfolge nativ ausführen. UpgradeItems nimmt die Eingaben selbst aus dem Inventar.
    // Reparierte Eingaben vor den Aufwertungen nativ reparieren; gebucht wird wie jede Reparatur unten.
    HostApplyingEdit = true;
    for (Trade& Next : Repairs)
        if (Next.Consumed && Alive(Next.Item))
        {
            static uintptr_t Repair = 0;
            if (const uintptr_t Fn = Function_(Repair, "RepairItem"))
            {
                uintptr_t Params = Next.Item;
                Objects::ProcessEvent(Inventory, Fn, &Params);
            }
            Next.Done = !IsBroken(Next.Item);
        }
    HostApplyingEdit = false;
    // opt-schuss-animation: Eingaben, die beim Host noch im Rucksack liegen, vor der Aufwertung nativ herausnehmen. Im nativen
    // Panel hat das Ziehen das schon getan; der Client zieht aber nur lokal. UpgradeItems löscht die Instanz, ohne ihre Wirkung zu
    // beenden: Die Waffe blieb in WeaponLookup und an den Händen der Client-Figur, die Ausgabe bekam keine Darstellung.
    {
        std::vector<uintptr_t> Worn;
        std::vector<ItemState> Before, After;
        const std::vector<SlotInfo> Placed = SlotsOf(Inventory);
        for (const auto& Next : Value.Upgrades)
            for (const uint16_t Source : Next.Inputs)
            {
                const uintptr_t Item = Live.contains(Source) ? Live[Source] : 0;
                if (!Alive(Item) || !Memory::Read<uint8_t>(Item + ItemInBackpack).value_or(0) || std::find(Worn.begin(), Worn.end(), Item) != Worn.end())
                    continue;
                Worn.push_back(Item);
                Before.push_back(ReadItem(Item, Placed, Source));
                After.push_back(Before.back());
                After.back().InBackpack = false;
                After.back().Tiles.clear();
            }
        if (!Worn.empty())
        {
            HostApplyingEdit = true;
            const bool Taken = Apply(Inventory, Worn, Before, After, {}, {}, false);
            HostApplyingEdit = false;
            Core::Log::Info("Inventar lokal: {} Eingabe(n) der Aufwertungen des Edits {} vorher aus dem Rucksack genommen, vollständig={}", Worn.size(),
                            Value.Id, Taken);
        }
    }
    std::vector<uintptr_t> Made;
    for (size_t K = 0; K < Value.Upgrades.size() && Ok; ++K)
    {
        std::vector<uintptr_t> Inputs;
        for (const uint16_t Source : Value.Upgrades[K].Inputs)
            Inputs.push_back(Live.contains(Source) ? Live[Source] : 0);
        const bool Present = std::all_of(Inputs.begin(), Inputs.end(), [&](uintptr_t Item) { return Alive(Item) && InInventory(Inventory, Item); });
        const uintptr_t Output = Present ? UpgradeForClient(Inventory, Inputs, Outputs[K].first, Outputs[K].second) : 0;
        Core::Log::Info("Inventar lokal: Aufwertung {} des Edits {} beim Host, Ausgabe {}", K + 1, Value.Id, Output != 0);
        if (!Output)
        {
            Core::Log::Warn("Inventar lokal: Aufwertung {} nicht ausgeführt, Rest des Edits wird nur teilweise übernommen", K + 1);
            Ok = false;
            break;
        }
        Made.push_back(Output);
        Live[UpgradeSource(K)] = Output;
    }
    std::vector<Tile> Create;
    for (const Tile& At : Value.NewSlots)
        if (std::find(Existing.begin(), Existing.end(), At) == Existing.end())
            Create.push_back(At);
    // Verbrauchte Eingaben fehlen schon, die Ausgaben bekommen ihren Endstand wie jedes andere Item.
    std::vector<uintptr_t> Targets;
    std::vector<ItemState> Goal;
    for (size_t i = 0; i < Resolved.size(); ++i)
        if (!Consumed.contains(Value.Items[i].Source))
        {
            Targets.push_back(Resolved[i]);
            Goal.push_back(Value.Items[i]);
        }
    for (size_t K = 0; K < Made.size(); ++K)
        if (!Consumed.contains(UpgradeSource(K)))
        {
            Targets.push_back(Made[K]);
            Goal.push_back(Value.Upgrades[K].Output);
        }
    for (size_t K = 0; K < Bought.size(); ++K)
        if (Bought[K] && !Consumed.contains(PurchaseSource(K)))
        {
            Targets.push_back(Bought[K]);
            Goal.push_back(Value.Purchases[K].Output);
        }
    const std::vector<SlotInfo> Now = SlotsOf(Inventory);
    std::vector<ItemState> Current;
    for (size_t i = 0; i < Targets.size(); ++i)
        Current.push_back(ReadItem(Targets[i], Now, Goal[i].Source));
    HostApplyingEdit = true; // RepairItem aus dem Plan nicht als direkten Aufruf des Clients verwerfen
    Ok = Apply(Inventory, Targets, Current, Goal, Create, {}, false) && Ok;
    HostApplyingEdit = false;
    // lager-handel-client: Schrott genau einmal und nur für tatsächlich Erledigtes buchen.
    if (Trades)
    {
        const std::vector<uintptr_t> After = ItemsOf(Inventory);
        int64_t Delta = 0;
        unsigned Sold = 0, Repaired = 0, Purchased = 0;
        for (size_t K = 0; K < Bought.size(); ++K)
            if (Bought[K])
            {
                Delta -= Value.Purchases[K].Cost;
                ++Purchased;
            }
        for (const Trade& Next : Sells)
            if (std::find(After.begin(), After.end(), Next.Item) == After.end())
            {
                Delta += Next.Price;
                ++Sold;
            }
        for (const Trade& Next : Repairs)
            if (Next.Consumed ? Next.Done : Alive(Next.Item) && !IsBroken(Next.Item))
            {
                Delta -= Next.Price;
                ++Repaired;
            }
        const int32_t Before = ScrapAmount(Wallet).value_or(0);
        AddScrap(Wallet, static_cast<int32_t>(Delta));
        Core::Log::Info("Handel: Edit {} gebucht, {} von {} verkauft, {} von {} repariert, {} von {} gekauft, Schrott {} zu {}", Value.Id, Sold,
                        Sells.size(), Repaired, Repairs.size(), Purchased, Bought.size(), Before, ScrapAmount(Wallet).value_or(0));
        Ok = Ok && Sold == Sells.size() && Repaired == Repairs.size() && Purchased == Bought.size();
    }
    // waffe-client-auswahl: das native Ablegen frischt danach die Wirkungen beider Figuren auf. Ohne das bleibt eine
    // neue Waffe bis zum nächsten Levelaufstieg wirkungslos und eine entfernte wirkt weiter.
    RefreshEffects(Inventory);
    if (const uintptr_t HostInventory = InventoryOfController(HostController); HostInventory && HostInventory != Inventory)
        RefreshEffects(HostInventory);
    Core::Log::Info("Inventar lokal: Edit {} übernommen, {} Items, {} neue Felder, {} Aufwertungen, {} Käufe, vollständig={}", Value.Id, Targets.size(),
                    Create.size(), Made.size(), Bought.size(), Ok);
    return true;
}

// Kisten laufen über die RewardQueue der öffnenden Figur. Das Panel bestätigt nur in der Queue des Hosts
// (InventoryPanel.HandleConfirmButton ruft OpenNextReward der eigenen Queue). Die Queue von P2 bliebe nach einer
// vom Client geöffneten Kiste belegt (+0x108 gesetzt, CurrentRewardObject zerstört), und RegisterReward gibt keine
// weitere Kiste mehr frei. OpenNextReward ändert nichts, solange die aktuelle Kiste lebt und nicht fertig ist.
// opt-airdrop-uebertrag: Über einen Kartenwechsel hängt keine Queue, beide Figuren und ihre Queues entstehen neu (gemessen
// 13.11.2026). Jede Änderung des Zustands beider Queues steht einmal im Log (Figur, belegt, aktuelle Kiste), damit ein Live-Test
// zeigt, ob eine Queue innerhalb einer Karte hängen bleibt. Als Absicherung schaltet die Mod auch die Queue des Hosts weiter,
// aber nur, wenn sie belegt ist und ihre aktuelle Kiste nicht mehr lebt; dann kann sie niemand mehr bestätigen.
void AdvanceQueues()
{
    static ULONGLONG Next = 0;
    const ULONGLONG Now = GetTickCount64();
    if (Now < Next)
        return;
    Next = Now + 1000;
    uintptr_t Coop = 0, HostController = 0, Remote = 0;
    if (!BoundRemoteP2(Coop, HostController, Remote))
    {
        HostController = LocalController();
        Remote = 0;
    }
    struct Seen
    {
        uintptr_t World = 0, Pawn = 0, Queue = 0;
        int Busy = -1, Current = -1;
    };
    static Seen Last[2];
    static uintptr_t Open = 0;
    const uintptr_t World = CurrentWorld();
    const uintptr_t Controllers[2] = {HostController, Remote};
    for (int i = 0; i < 2; ++i)
    {
        const char* Who = i ? "Remote-Spieler" : "Host";
        const uintptr_t Pawn = Controllers[i] ? Pointer(Controllers[i] + ControllerPawn) : 0;
        const uintptr_t Queue = Alive(Pawn) && Objects::IsA(Pawn, "DDCharacter") ? Pointer(Pawn + CharacterRewardQueue) : 0;
        if (!Alive(Queue) || !Objects::IsA(Queue, "RewardQueueComponent"))
            continue;
        const int Busy = Memory::Read<uint8_t>(Queue + QueueBusy).value_or(0) != 0;
        // CurrentRewardObject, schwacher Zeiger: Platz und Serial im Objekt-Array. 0 = keine, 1 = lebt, 2 = zerstört.
        const int32_t Index = Memory::Read<int32_t>(Queue + QueueCurrent).value_or(-1);
        const uint32_t Serial = Memory::Read<uint32_t>(Queue + QueueCurrent + 4).value_or(0);
        const int Current = Index <= 0 && !Serial ? 0 : Objects::IsLive({Objects::At(Index), Index, Serial}) ? 1 : 2;
        Seen& Was = Last[i];
        if (Was.World != World || Was.Pawn != Pawn || Was.Queue != Queue || Was.Busy != Busy || Was.Current != Current)
        {
            static unsigned Lines = 0;
            if (++Lines <= 400)
                Core::Log::Info("Reward-Queue {}: {}{}belegt {}, aktuelle Kiste {}", Who, Was.World != World ? "neue Welt, " : "",
                                !Was.Pawn ? "" : Was.Pawn != Pawn ? "neue Figur, " : Was.World != World ? "dieselbe Figur, " : "", Busy != 0,
                                Current == 0 ? "keine" : Current == 1 ? "lebt" : "zerstört");
            Was = {World, Pawn, Queue, Busy, Current};
        }
        if (!Busy || (i == 0 && Current != 2) ||!Function(Open, "OpenNextReward", "RewardQueueComponent"))
            continue;
        struct
        {
            bool Opened;
            uint8_t Pad[7];
        } Params{};
        Objects::ProcessEvent(Queue, Open, &Params);
        const bool Freed = !Memory::Read<uint8_t>(Queue + QueueBusy).value_or(1);
        if (Freed || Params.Opened)
            Core::Log::Info("Airdrop-Auswahl: RewardQueue des {} weitergeschaltet, nächste Kiste {}", i ? "Remote-Spielers" : "Hosts",
                            Params.Opened ? "freigegeben" : "keine");
    }
}

void Tick()
{
    if (CurrentRole() == NetRole::Client)
        TickStation();
    else if (Station.Active)
        Station = {};
    if (CurrentRole() != NetRole::Host)
        return;
    // Lager am Rundenende: nachholen, sobald der Client bereit oder nicht mehr verbunden ist.
    if (End.HeldPanel && (End.ClientReady == End.Id || !RemoteReady.load() || End.ClientIn != End.Id))
        ReleaseRoundEndConfirm(End.ClientReady == End.Id ? "Client bereit" : "Client nicht mehr im Lager");
    if (!Host.Active)
    {
        AdvanceQueues();
        return;
    }
    if (!Alive(Host.Inventory))
    {
        EndHostSession("Inventar weg");
        return;
    }
    // Zurückgehaltenes Bestätigen nachholen, sobald der Client gewählt hat oder nicht mehr verbunden ist.
    // HandleConfirmButton ignoriert den Aufruf während eines Drags (+0x790) oder ohne Koop-Autorität. Dann wäre der
    // Klick verloren und der Host müsste erneut bestätigen. Deshalb nicht im Drag nachholen und, solange die Sitzung
    // offen bleibt, alle 500 ms erneut, höchstens zehnmal.
    const auto SessionOpen = [] { return std::any_of(Host.Items.begin(), Host.Items.end(), [](uintptr_t Item) { return IsReward(Item) && InInventory(Host.Inventory, Item); }); };
    if (Host.HeldPanel && (Host.ClientPick || !RemoteReady.load()))
    {
        const uintptr_t Panel = Host.HeldPanel;
        const ULONGLONG Now = GetTickCount64();
        if (Host.HeldRef.Object != Panel || !HeldPanelUsable(Host.HeldRef, Host.HeldWorld) || Host.ReplayCount >= 10)
        {
            if (Host.ReplayCount >= 10)
                Core::Log::Warn("Airdrop-Auswahl: Bestätigen des Hosts nach 10 Versuchen nicht übernommen, Host muss selbst bestätigen");
            Host.HeldPanel = 0;
        }
        else if (Now >= Host.NextReplay && !Memory::Read<uint8_t>(Panel + PanelIsDragging).value_or(1))
        {
            ++Host.ReplayCount;
            Host.NextReplay = Now + 500;
            Core::Log::Info("Airdrop-Auswahl: Bestätigen des Hosts nachgeholt (Versuch {})", Host.ReplayCount);
            OriginalConfirm(Panel);
            if (!SessionOpen())
                Host.HeldPanel = 0;
        }
    }
    // Ende: keine Choice mehr als Belohnung im Inventar des Hosts, das Panel hat bestätigt.
    if (!SessionOpen())
        EndHostSession("Host hat bestätigt");
}

bool ShowForClient(uint64_t SessionId, uint64_t Expected, int32_t Level, const std::vector<ChoiceItem>& Items)
{
    if (CurrentRole() != NetRole::Client)
        return false;
    if (Shown.Session == SessionId && Shown.Revision == Expected && !Shown.Items.empty())
        return true;
    // opt-werkbank-station: Eine offene Stations-Bearbeitung geht zuerst an den Host, die Auswahl folgt nach seiner Antwort.
    FinishStation("Auswahl des Hosts");
    // opt-haendler-client: auch nicht, solange nach einem übernommenen Edit noch lokale Instanzen im Inventar liegen.
    if (ClientPending || Game::InventoryEdit::ClientSettling())
        return false;
    CloseForClient();
    static uintptr_t SetRewards = 0;
    const uintptr_t Inventory = InventoryOfController(LocalController());
    if (!Inventory || Items.empty() || !Function(SetRewards, "SetRewardItems", "InventoryComponent"))
    {
        Core::Log::Warn("Airdrop-Auswahl: Panel beim Client nicht möglich (Inventar {})", Inventory != 0);
        return false;
    }
    const auto Resolved = ResolveChoices(Items);
    std::vector<uintptr_t> Instances;
    for (size_t i = 0; i < Items.size(); ++i)
    {
        const uintptr_t Instance =
            Resolved[i].first ? reinterpret_cast<CreateFn>(At(CreateItemInstance))(Inventory, Resolved[i].first, false, Resolved[i].second) : 0;
        if (!Instance)
        {
            Core::Log::Warn("Airdrop-Auswahl: Choice {} ({}) beim Client nicht anlegbar, Daten={}@{:#x} Seltenheit={}@{:#x}", i, Items[i].Name, Items[i].Data,
                            Resolved[i].first, Items[i].Rarity, Resolved[i].second);
            return false;
        }
        const uint8_t Broken = Items[i].Broken;
        Memory::WriteBytes(Instance + ItemBroken, &Broken, 1);
        Instances.push_back(Instance);
    }
    Game::InventoryEdit::BeginClient(Inventory, Instances, LocalController());
    ItemArray Array{Instances.data(), static_cast<int32_t>(Instances.size()), static_cast<int32_t>(Instances.size())};
    struct
    {
        ItemArray Items;
        int32_t Level;
        uint8_t Pad[4];
    } Params{Array, Level, {}};
    static_assert(sizeof(Params) == 0x18);
    Objects::ProcessEvent(Inventory, SetRewards, &Params);
    Shown = {SessionId, Expected, Inventory, Instances};
    Core::Log::Info("Airdrop-Auswahl: Panel beim Client mit {} Choices geöffnet (Sitzung {})", Instances.size(), SessionId);
    return true;
}

void CloseForClient()
{
    ClearReadyTint();
    if (!Shown.Items.empty())
        Core::Log::Info("Airdrop-Auswahl: Panel beim Client geschlossen, Sitzung {}", Shown.Session);
    // Eine Bearbeitung aus dem Lager (Aufwertung, werkbank-client) gehört nicht zur Auswahl und endet mit dem Lager.
    if (Game::InventoryEdit::ClientEditing() && !LagerEdit && !Station.Active)
        Game::InventoryEdit::RevertClient("Auswahl geschlossen");
    Shown = {};
}

std::optional<ReadyState> ClientReadyState()
{
    if (CurrentRole() != NetRole::Client || Shown.Items.empty() || ClientPending)
        return std::nullopt;
    return ReadyState{Shown.Session, Shown.Revision, Shown.Ready};
}

void FinishClientIfAllReady(bool AllReady)
{
    if (CurrentRole() != NetRole::Client || !AllReady || !Shown.Ready || ClientPending || Shown.Items.empty())
        return;
    const uintptr_t Panel = Shown.Panel;
    if (Shown.PanelRef.Object != Panel || !HeldPanelUsable(Shown.PanelRef, Shown.PanelWorld))
    {
        Core::Log::Warn("Bestätigen: Panel des Clients weg, Bereit zurückgenommen");
        Shown.Ready = false;
        Tinted = {};
        return;
    }
    if (Memory::Read<uint8_t>(Panel + PanelIsDragging).value_or(1))
        return; // nach dem Drag erneut
    Core::Log::Info("Bestätigen: alle bereit, Client bestätigt (Sitzung {})", Shown.Session);
    SetReadyTint(Panel, false);
    ClientConfirm(Panel);
    if (!ClientPending && !Shown.Items.empty())
    {
        Shown.Ready = false; // ohne Wahl bleibt das Panel offen
        Core::Log::Info("Bestätigen: Client ohne Wahl, Bereit zurückgenommen");
    }
}

std::optional<Game::InventoryEdit::Edit> PendingClientEdit()
{
    return ClientPending;
}

void ResolveClientEdit(uint64_t Id, bool Accepted)
{
    if (!ClientPending || ClientPending->Id != Id)
        return;
    ClientPending.reset();
    if (Accepted)
        Game::InventoryEdit::ForgetClient();
    else
        Game::InventoryEdit::RevertClient("vom Host abgelehnt");
}

void AbortClientEdit()
{
    if (!ClientPending)
        return;
    ClientPending.reset();
    Game::InventoryEdit::RevertClient("Verbindung zum Host weg");
}

void SetRoundEnd(uint64_t Id)
{
    if (Id == End.Id)
        return;
    // opt-werkbank-station: Beginnt ein Rundenende, geht eine offene Stations-Bearbeitung vorher an den Host.
    if (Id)
        FinishStation("Rundenende");
    if (End.HeldPanel)
        SetReadyTint(End.HeldPanel, false); // prüft den gemerkten Knopf selbst
    if (Id)
        Core::Log::Info("Lager: neues Rundenende, Bestätigen wartet auf alle");
    else
        Core::Log::Info("Lager: Rundenende vorbei");
    End = {};
    End.Id = Id;
    if (LagerEdit && LagerEdit != Id)
    {
        LagerEdit = 0;
        if (Game::InventoryEdit::ClientEditing() && !ClientPending)
            Game::InventoryEdit::RevertClient("Lager ohne Bestätigen verlassen");
    }
}

void SetRoundEndClient(uint64_t Id, bool Ready)
{
    if (CurrentRole() != NetRole::Host)
        return;
    if (!Id && End.ClientIn)
        Core::Log::Info("Lager: Client meldet sich nicht mehr aus dem Lager");
    else if (Id && (End.ClientIn != Id || (Ready && End.ClientReady != Id)))
        Core::Log::Info("Lager: Client {} (eigenes Rundenende {})", Ready ? "bereit" : "im Lager", Id == End.Id ? "passt" : "passt nicht");
    End.ClientIn = Id;
    if (Ready)
        End.ClientReady = Id;
}

RoundEndState HostRoundEnd()
{
    if (CurrentRole() != NetRole::Host || !End.Id)
        return {};
    return {End.Id, End.HeldPanel != 0 || End.Done == End.Id, End.ClientReady == End.Id};
}

std::optional<RoundEndOwn> ClientRoundEnd()
{
    if (CurrentRole() != NetRole::Client || !End.Id)
        return std::nullopt;
    // Bereit erst, wenn ein Edit aus dem Lager beantwortet ist; sonst reiste der Host vor der Übernahme weiter.
    return RoundEndOwn{End.Id, (End.HeldPanel != 0 || End.Done == End.Id) && !ClientPending};
}

void SetHostRoundEndStatus(std::optional<RoundEndState> Status)
{
    if (CurrentRole() != NetRole::Client)
        return;
    End.HostStatusId = Status ? Status->Id : 0;
    if (End.HeldPanel && !ClientPending && ClientMayReleaseRoundEnd(End.Id, End.HostStatusId, Status && Status->HostReady, Status && Status->ClientReady))
        ReleaseRoundEndConfirm(End.HostStatusId == End.Id ? "alle bereit" : "kein passender Stand vom Host");
}

namespace
{
// Inventar zu einem Controller, gemerkt, solange es lebt und zum PlayerState passt.
uintptr_t CachedInventory(uintptr_t& Cache, uintptr_t Controller)
{
    const uintptr_t State = Controller ? Pointer(Controller + ControllerPlayerState) : 0;
    if (!State)
        return 0;
    // Gleiche Bedingung wie InventoryOf, sonst sucht jeder Aufruf erneut im ganzen Objekt-Array (host-lagspike).
    if (!Alive(Cache) || (Pointer(Cache + InventoryPlayerState) != State && Pointer(Cache + Offsets::UObject::Outer) != State))
        Cache = InventoryOf(State);
    return Cache;
}
}

std::vector<RarityEntry> ClientRarities()
{
    static uintptr_t Cache = 0;
    uintptr_t Coop = 0, HostController = 0, Remote = 0;
    const uintptr_t Inventory = CurrentRole() == NetRole::Host && BoundRemoteP2(Coop, HostController, Remote) ? CachedInventory(Cache, Remote) : 0;
    std::vector<RarityEntry> Result;
    const std::vector<uintptr_t> Items = Inventory ? Game::InventoryEdit::ItemsOf(Inventory) : std::vector<uintptr_t>{};
    for (size_t i = 0; i < Items.size() && i < 96; ++i)
    {
        const uintptr_t Rarity = Pointer(Items[i] + ItemRarity);
        Result.push_back({static_cast<uint16_t>(i), Game::InventoryEdit::DataHashOf(Items[i]), Rarity ? Memory::NameOf(Rarity) : std::string{}});
    }
    return Result;
}

void ApplyRarities(const std::vector<RarityEntry>& Entries)
{
    static uintptr_t Cache = 0;
    static std::unordered_map<std::string, uintptr_t> Known;
    if (CurrentRole() != NetRole::Client || Entries.empty() || Game::InventoryEdit::ClientEditing() || ClientPending)
        return;
    const uintptr_t Inventory = CachedInventory(Cache, LocalController());
    const std::vector<uintptr_t> Items = Inventory ? Game::InventoryEdit::ItemsOf(Inventory) : std::vector<uintptr_t>{};
    // Unbekannte Namen in einem Durchlauf über das Objekt-Array auflösen; nur URarity-Objekte.
    std::vector<Game::Rewards::ChoiceItem> Missing;
    for (const RarityEntry& Entry : Entries)
        if (!Entry.Rarity.empty() && (!Known.contains(Entry.Rarity) || !Alive(Known[Entry.Rarity])) &&
            std::none_of(Missing.begin(), Missing.end(), [&](const auto& Item) { return Item.Rarity == Entry.Rarity; }))
            Missing.push_back({{}, {}, Entry.Rarity, false});
    if (!Missing.empty())
    {
        const auto Found = ResolveChoices(Missing);
        for (size_t i = 0; i < Missing.size(); ++i)
            Known[Missing[i].Rarity] = Found[i].second;
    }
    unsigned Set = 0;
    for (const RarityEntry& Entry : Entries)
    {
        if (Entry.Index >= Items.size() || !Alive(Items[Entry.Index]) || Game::InventoryEdit::DataHashOf(Items[Entry.Index]) != Entry.DataHash)
            continue;
        const uintptr_t Want = Entry.Rarity.empty() ? 0 : Known[Entry.Rarity];
        if ((!Want && !Entry.Rarity.empty()) || Pointer(Items[Entry.Index] + ItemRarity) == Want)
            continue;
        Set += Memory::WriteBytes(Items[Entry.Index] + ItemRarity, &Want, sizeof(Want));
    }
    if (Set)
        Core::Log::Info("Seltenheiten: {} Items des Clients vom Host übernommen", Set);
}
}

namespace Game::Rewards
{
std::optional<int32_t> HostScrap()
{
    using namespace Game::Patches;
    if (CurrentRole() != NetRole::Host)
        return std::nullopt;
    return ScrapAmount(ScrapWallet());
}

void ApplyScrap(int32_t Amount)
{
    using namespace Game::Patches;
    if (CurrentRole() != NetRole::Client || Amount < 0 || Game::InventoryEdit::ClientEditing() || ClientPending)
        return;
    const uintptr_t Wallet = ScrapWallet();
    const std::optional<int32_t> Have = ScrapAmount(Wallet);
    if (!Have || *Have == Amount)
        return;
    AddScrap(Wallet, Amount - *Have);
    Core::Log::Info("Schrott: Stand des Hosts übernommen, {} statt {}", ScrapAmount(Wallet).value_or(-1), *Have);
}
}
