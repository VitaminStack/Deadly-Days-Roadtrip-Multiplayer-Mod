// Airdrop-Auswahl für Host und Client ohne Koop-Modus, siehe src/Game/README.md#airdrop-auswahl.
//
// Host: Die Kiste läuft wie im Solo-Spiel, öffnet sie der Remote-Spieler, bekommt die Figur des Hosts die Auswahl.
// UInventoryComponent::SetRewardItems (0x5979B30) zeigt die Choices im nativen Panel; die Mod merkt sie sich als
// Sitzung und schickt sie über den Kontrollkanal an den Client. Das Bestätigen des Hosts (InventoryPanel.
// HandleConfirmButton, 0x5A6CB20) hält die Mod zurück, bis der Client gewählt hat; so lange bleibt das Spiel pausiert.
// Die Wahl des Clients legt der Host neu in dessen repliziertes Inventar.
//
// Client: Die Mod legt lokale Instanzen derselben Items an und zeigt sie über SetRewardItems im nativen Panel. Das Panel
// bearbeitet das Inventar nur lokal (InventoryEdit.cpp). Nach dem Bestätigen ist die gewählte Instanz keine Belohnung
// mehr; Wahl und Endstand gehen als ein Edit an den Host (inventar-lokal).
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <optional>
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
#include "Game/RewardAdapter.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t SetRewardItems = 0x5979B30;
constexpr uintptr_t PanelConfirm = 0x5A6CB20;
constexpr uintptr_t PanelSetupInventory = 0x5A8BAB0;
// UInventoryComponent::CreateItemInstance, nativ. Die UFunction ist BlueprintAuthorityOnly, ProcessEvent überspringt
// sie beim Client.
constexpr uintptr_t CreateItemInstance = 0x5959700;
constexpr uintptr_t MemoryFree = 0x12F8400;

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
SetRewardsFn OriginalSetRewards = nullptr;
ConfirmFn OriginalConfirm = nullptr;
ConfirmFn OriginalSetupInventory = nullptr;

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
    bool Ready = false;  // Bestätigen gedrückt, wartet auf alle (bestaetigen-alle)
} Shown;
// Lager am Rundenende (bestaetigen-rundenende), beide Rollen. Id kommt je Takt aus den Kartenpatches.
struct
{
    uint64_t Id = 0;
    uint64_t Done = 0;        // Bestätigen für dieses Rundenende ist nativ gelaufen
    uintptr_t HeldPanel = 0;  // zurückgehaltenes Bestätigen
    uint64_t ClientIn = 0, ClientReady = 0; // Host: Stand des Clients aus 0x306
    uint64_t HostStatusId = 0; // Client: Rundenende im frischen Stand des Hosts, 0 ohne
} End;
// Client: gesendeter Edit bis zur Antwort des Hosts.
std::optional<Game::InventoryEdit::Edit> ClientPending;
constexpr uintptr_t ItemPivotTile = 0x28;
constexpr uint32_t FuncNet = 0x40;
constexpr uintptr_t FunctionFlags = 0xB0;
constexpr uintptr_t PanelIsDragging = 0x790;
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

uintptr_t Tinted = 0; // gelb gefärbter Knopf

// Knopf gelb (bereit) oder wieder normal, über UUserWidget.SetColorAndOpacity. Die Farbe ist rein lokal.
void SetReadyTint(uintptr_t Panel, bool Ready)
{
    static uintptr_t SetColor = 0;
    const uintptr_t Button = Ready ? ConfirmButtonOf(Panel) : Tinted;
    if (!Alive(Button) || !Function(SetColor, "SetColorAndOpacity", "UserWidget"))
    {
        if (!Ready)
            Tinted = 0;
        return;
    }
    struct
    {
        float R, G, B, A;
    } Color = Ready ? decltype(Color){1.0f, 0.82f, 0.1f, 1.0f} : decltype(Color){1.0f, 1.0f, 1.0f, 1.0f};
    Objects::ProcessEvent(Button, SetColor, &Color);
    Tinted = Ready ? Button : 0;
}

void ClearReadyTint()
{
    if (Tinted)
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
    const bool Hold = Role == NetRole::Host ? HostHoldsRoundEndConfirm(End.Id, End.Done, RemoteReady.load(), End.ClientIn, End.ClientReady)
                                            : ClientHoldsRoundEndConfirm(End.Id, End.Done, End.HostStatusId);
    if (!Hold)
    {
        End.Done = End.Id;
        Core::Log::Info("Lager: {} bestätigt, alle bereit", Role == NetRole::Host ? "Host" : "Client");
        return false;
    }
    End.HeldPanel = Panel;
    SetReadyTint(Panel, true);
    Core::Log::Info("Lager: {} bereit, wartet auf {}", Role == NetRole::Host ? "Host" : "Client", Role == NetRole::Host ? "den Client" : "den Host");
    return true;
}

// Zurückgehaltenes Bestätigen im Lager nativ nachholen. Einmal: Bleibt es ohne Wirkung, bestätigt der Spieler erneut, dann
// läuft es direkt nativ. Ist das Panel weg, nur die Farbe vergessen.
void ReleaseRoundEndConfirm(const char* Why)
{
    const uintptr_t Panel = End.HeldPanel;
    if (!Panel)
        return;
    if (!Alive(Panel) || !Objects::IsA(Panel, "InventoryPanel"))
    {
        End.HeldPanel = 0;
        Tinted = 0;
        Core::Log::Warn("Lager: Panel weg, zurückgehaltenes Bestätigen verworfen ({})", Why);
        return;
    }
    if (Memory::Read<uint8_t>(Panel + PanelIsDragging).value_or(1))
        return; // nach dem Drag erneut
    End.HeldPanel = 0;
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
        SetReadyTint(Panel, true);
        return;
    }
    if (HoldRoundEndConfirm(Panel))
        return;
    OriginalConfirm(Panel);
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
    unsigned Restored = 0;
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Item = Pointer(Data + 8 * i);
        if (!Alive(Item) || IsReward(Item) || !Memory::Read<uint8_t>(Item + ItemInBackpack).value_or(0))
            continue;
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
        Objects::ProcessEvent(Panel, Snap, &Params);
        ++Restored;
    }
    if (Restored)
        Core::Log::Info("Airdrop-Auswahl: {} Rucksack-Anzeige(n) aus replizierten Pivot-Feldern positioniert", Restored);
}

void __fastcall SetupInventoryHook(uintptr_t Panel)
{
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
    return Code::Expect(At(PanelConfirm), {0x40, 0x57, 0x48, 0x83, 0xEC, 0x50, 0x48, 0x8B, 0xF9}, "InventoryPanel.HandleConfirmButton") &&
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
const Patch List[] = {
    {"Airdrop-Auswahl: Choices des Hosts mitlesen", CheckSetRewards, ApplySetRewards},
    {"Airdrop-Auswahl: Bestätigen bis zur Wahl des Partners halten", CheckConfirm, ApplyConfirm},
    {"Airdrop-Auswahl: Rucksack-Anzeige aus replizierten Feldern aufbauen", CheckSetupInventory, ApplySetupInventory},
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

bool FilterRewardEvent(uintptr_t Object, uintptr_t Function, void* Params)
{
    if (CurrentRole() == NetRole::Host)
        return RemoteReady.load(std::memory_order_relaxed) && NullItemRpc(Function, Params);
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
        const bool Wanted = Owner == "InventoryComponent" || Owner == "InventoryItemInstance" || Owner == "BackpackSlot" ||
                            Owner == "InventoryPanel" || Owner == "RewardQueueComponent" || Owner == "CoopInventoryCoordinatorComponent" ||
                            Owner.starts_with("WP_Inventory") || Owner.starts_with("WP_Backpack") || Owner.starts_with("WP_Item");
        const std::string Name = Memory::NameOf(Function);
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
    Core::Log::Info("InvTrace {} {}{} an {:#x} ({}), erster Parameter {:#x} {}", CurrentRole() == NetRole::Client ? "C" : "H", It->second,
                    Flags & FuncNet ? " [Net]" : "", Object, Memory::NameOf(Object), First, FirstName);
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
    std::vector<Tile> Create;
    for (const Tile& At : Value.NewSlots)
        if (std::find(Existing.begin(), Existing.end(), At) == Existing.end())
            Create.push_back(At);
    const std::vector<SlotInfo> Now = SlotsOf(Inventory);
    std::vector<ItemState> Current;
    for (size_t i = 0; i < Resolved.size(); ++i)
        Current.push_back(ReadItem(Resolved[i], Now, Value.Items[i].Source));
    const bool Ok = Apply(Inventory, Resolved, Current, Value.Items, Create, {}, false);
    Core::Log::Info("Inventar lokal: Edit {} übernommen, {} Items, {} neue Felder, vollständig={}", Value.Id, Resolved.size(), Create.size(), Ok);
    return true;
}

// Kisten laufen über die RewardQueue der öffnenden Figur. Das Panel bestätigt nur in der Queue des Hosts
// (InventoryPanel.HandleConfirmButton ruft OpenNextReward der eigenen Queue). Die Queue von P2 bliebe nach einer
// vom Client geöffneten Kiste belegt (+0x108 gesetzt, CurrentRewardObject zerstört), und RegisterReward gibt keine
// weitere Kiste mehr frei. OpenNextReward ändert nichts, solange die aktuelle Kiste lebt und nicht fertig ist.
void AdvanceRemoteQueue()
{
    static ULONGLONG Next = 0;
    const ULONGLONG Now = GetTickCount64();
    if (Now < Next)
        return;
    Next = Now + 1000;
    uintptr_t Coop = 0, HostController = 0, Remote = 0;
    if (!BoundRemoteP2(Coop, HostController, Remote))
        return;
    const uintptr_t Pawn = Pointer(Remote + ControllerPawn);
    const uintptr_t Queue = Alive(Pawn) ? Pointer(Pawn + CharacterRewardQueue) : 0;
    static uintptr_t Open = 0;
    if (!Alive(Queue) || !Memory::Read<uint8_t>(Queue + QueueBusy).value_or(0) ||
        !Function(Open, "OpenNextReward", "RewardQueueComponent"))
        return;
    struct
    {
        bool Opened;
        uint8_t Pad[7];
    } Params{};
    Objects::ProcessEvent(Queue, Open, &Params);
    if (!Memory::Read<uint8_t>(Queue + QueueBusy).value_or(1) || Params.Opened)
        Core::Log::Info("Airdrop-Auswahl: RewardQueue des Remote-Spielers weitergeschaltet, nächste Kiste {}",
                        Params.Opened ? "freigegeben" : "keine");
}

void Tick()
{
    if (CurrentRole() != NetRole::Host)
        return;
    // Lager am Rundenende: nachholen, sobald der Client bereit oder nicht mehr verbunden ist.
    if (End.HeldPanel && (End.ClientReady == End.Id || !RemoteReady.load() || End.ClientIn != End.Id))
        ReleaseRoundEndConfirm(End.ClientReady == End.Id ? "Client bereit" : "Client nicht mehr im Lager");
    if (!Host.Active)
    {
        AdvanceRemoteQueue();
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
        if (!Alive(Panel) || !Objects::IsA(Panel, "InventoryPanel") || Host.ReplayCount >= 10)
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
    if (ClientPending)
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
    Game::InventoryEdit::BeginClient(Inventory, Instances);
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
    if (Game::InventoryEdit::ClientEditing())
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
    if (!Alive(Panel) || !Objects::IsA(Panel, "InventoryPanel"))
    {
        Core::Log::Warn("Bestätigen: Panel des Clients weg, Bereit zurückgenommen");
        Shown.Ready = false;
        Tinted = 0;
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
    if (End.HeldPanel && Alive(End.HeldPanel))
        SetReadyTint(End.HeldPanel, false);
    if (Id)
        Core::Log::Info("Lager: neues Rundenende, Bestätigen wartet auf alle");
    else
        Core::Log::Info("Lager: Rundenende vorbei");
    End = {};
    End.Id = Id;
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
    return RoundEndOwn{End.Id, End.HeldPanel != 0 || End.Done == End.Id};
}

void SetHostRoundEndStatus(std::optional<RoundEndState> Status)
{
    if (CurrentRole() != NetRole::Client)
        return;
    End.HostStatusId = Status ? Status->Id : 0;
    if (End.HeldPanel && ClientMayReleaseRoundEnd(End.Id, End.HostStatusId, Status && Status->HostReady, Status && Status->ClientReady))
        ReleaseRoundEndConfirm(End.HostStatusId == End.Id ? "alle bereit" : "kein passender Stand vom Host");
}
}
