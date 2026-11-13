// Testhilfen: CheatManager fuer die DD_-Befehle des Spiels und Laden einzelner Assets, etwa fuer einen SDK-Dump mit Bossen.
#include "Spike.h"

#include <algorithm>
#include <vector>
#include "TestGodmodePolicy.h"

namespace Spike
{
namespace
{
constexpr uintptr_t kControllerCheatManager = 0x410;
bool TestGodmodeEnabled = false; // Explicit opt-in; Spike only, never a production mod setting.
struct ProtectedPawn
{
    uintptr_t Pawn;
    uintptr_t AbilitySystem;
    int32_t Name;
    int32_t Index;
};
std::vector<ProtectedPawn> GodmodeOwned;
}

// Uses the game's loose-tag API, not healing or fabricated replicated health. A private +1 contribution can be
// removed without clearing invincibility granted by gameplay effects. Never dereference a cached old-world pawn.
void TickTestGodmode()
{
    if (!TestGodmodeEnabled && GodmodeOwned.empty())
        return;
    const uint64_t Tag = Read<uint64_t>(Base + Rva(0xAC680D8));
    if (!Tag || NameFromIndex(static_cast<int32_t>(Tag)) != "State.Invincible")
        return;
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t State = World ? Read<uintptr_t>(World + kWorldGameState) : 0;
    const uintptr_t Players = State ? Read<uintptr_t>(State + kGameStatePlayerArray) : 0;
    const int32_t Count = State ? Read<int32_t>(State + kGameStatePlayerArray + 8) : 0;
    if (!Players || Count <= 0 || Count > 16)
        return;
    std::vector<ProtectedPawn> Roster;
    // Do not forget ownership while the roster is partially replicated; the same pawn may return next tick.
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Player = Read<uintptr_t>(Players + i * 8);
        const uintptr_t Pawn = Player ? Read<uintptr_t>(Player + 0x320) : 0;
        if (!Pawn || (Read<uint32_t>(Pawn + 8) & 0x10))
            return;
        bool IsCharacter = false;
        for (uintptr_t Class = Read<uintptr_t>(Pawn + 0x10); Class; Class = Read<uintptr_t>(Class + 0x40))
            if (NameOf(Class) == "DDCharacter") { IsCharacter = true; break; }
        if (!IsCharacter)
            return;
        const uintptr_t Asc = Read<uintptr_t>(Pawn + 0x6E0);
        if (!Asc)
            return;
        bool Duplicate = false;
        for (const auto& Existing : Roster)
            Duplicate |= Existing.AbilitySystem == Asc;
        if (!Duplicate)
            Roster.push_back({Pawn, Asc, Read<int32_t>(Pawn + 0x18), Read<int32_t>(Pawn + 0x0C)});
    }
    std::vector<ProtectedPawn> Current;
    for (const auto& Entry : Roster)
    {
        const uintptr_t Pawn = Entry.Pawn;
        const uintptr_t Asc = Entry.AbilitySystem;
        bool Owned = false;
        for (const auto& Old : GodmodeOwned)
            Owned |= Old.Pawn == Pawn && Old.AbilitySystem == Asc && Old.Name == Entry.Name && Old.Index == Entry.Index;
        using HasTagFn = bool(__fastcall*)(uintptr_t, uint64_t);
        const uintptr_t Interface = Asc + 0x148;
        const bool HasTag = Read<HasTagFn*>(Interface)[3](Interface, Tag);
        using UpdateTagFn = void(__fastcall*)(uintptr_t, const uint64_t*, int32_t, uint8_t);
        const auto Update = reinterpret_cast<UpdateTagFn>(Base + Rva(0x4B7D9D0));
        const int Delta = TestGodmodePolicy::Delta(TestGodmodeEnabled, Owned, HasTag);
        if (Delta > 0)
        {
            Update(Asc, &Tag, 1, 0);
            Owned = true;
            Log("Test-Godmode: State.Invincible gesetzt fuer " + NameOf(Pawn));
        }
        else if (!TestGodmodeEnabled && Owned)
        {
            if (Delta < 0)
                Update(Asc, &Tag, -1, 0);
            Owned = false;
            Log("Test-Godmode: eigener Tag-Beitrag entfernt fuer " + NameOf(Pawn));
        }
        if (Owned)
            Current.push_back(Entry);
    }
    GodmodeOwned = std::move(Current);
}

void SetTestGodmode(bool Enabled)
{
    TestGodmodeEnabled = Enabled;
    Log(Enabled ? "Test-Godmode: EIN, bleibt ueber Reisen aktiv" : "Test-Godmode: AUS");
    TickTestGodmode();
}

// Im Shipping-Build bleibt APlayerController::CheatManager leer, die Konsole verwirft dann alle DD_-Befehle.
// GameplayStatics.SpawnObject legt den Manager mit dem Controller als Outer an, wie es die Engine selbst taete.
void EnableCheats()
{
    const uintptr_t Statics = FindObject("Default__GameplayStatics", nullptr);
    const uintptr_t SpawnObject = FindObject("SpawnObject", "GameplayStatics");
    const uintptr_t CheatClass = FindObject("DDCheatManager", nullptr);
    if (!Statics || !SpawnObject || !CheatClass)
    {
        Log("Cheats: Klassen nicht gefunden");
        return;
    }
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    int Enabled = 0;
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x10) != 0)
            continue;
        bool IsController = false;
        for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class && !IsController; Class = Read<uintptr_t>(Class + 0x40))
            IsController = NameOf(Class) == "PlayerController";
        // Das Feld erst lesen, wenn das Objekt sicher ein Controller ist, kleinere Objekte enden davor.
        if (!IsController || Read<uintptr_t>(Object + kControllerCheatManager))
            continue;
        struct { uintptr_t ObjectClass; uintptr_t Outer; uintptr_t ReturnValue; } Params{CheatClass, Object, 0};
        Read<ProcessEventFn*>(Statics)[kProcessEventIndex](Statics, SpawnObject, &Params);
        if (!Params.ReturnValue)
            continue;
        *reinterpret_cast<uintptr_t*>(Object + kControllerCheatManager) = Params.ReturnValue;
        ++Enabled;
    }
    Log("Cheats: CheatManager fuer " + std::to_string(Enabled) + " Controller angelegt");
}

// Testhilfe (werkbank-client): Host legt jedem Spielerinventar ein Item an, "!give ItemData_Pistol [Rarity] [broken]". Natives
// CreateAndAddItem, das Item repliziert wie jede Vergabe. Nur beim Host sinnvoll, beim Client entstuende eine lokale Instanz.
void GiveItemCommand(const std::wstring& Args)
{
    const std::string Text(Args.begin(), Args.end());
    // lager-handel-client: "broken" an beliebiger Stelle nach den Daten legt das Item kaputt an.
    std::vector<std::string> Words;
    for (size_t At = 0; At < Text.size();)
    {
        const size_t Space = Text.find(' ', At);
        const std::string Word = Text.substr(At, Space == std::string::npos ? std::string::npos : Space - At);
        if (!Word.empty())
            Words.push_back(Word);
        At = Space == std::string::npos ? Text.size() : Space + 1;
    }
    const bool Broken = std::find(Words.begin(), Words.end(), "broken") != Words.end();
    Words.erase(std::remove(Words.begin(), Words.end(), "broken"), Words.end());
    const std::string DataName = Words.empty() ? std::string{} : Words[0], RarityName = Words.size() > 1 ? Words[1] : std::string{};
    const uintptr_t Data = FindObject(DataName.c_str(), nullptr);
    const uintptr_t Rarity = RarityName.empty() ? 0 : FindObject(RarityName.c_str(), nullptr);
    const uintptr_t Create = FindObject("CreateAndAddItem", "InventoryComponent");
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    if (!Data || !Create || !World || !Read<uintptr_t>(World + kWorldGameMode) || (!RarityName.empty() && !Rarity))
    {
        Log("Give: " + DataName + " nicht moeglich (Daten, Seltenheit oder kein Host)");
        return;
    }
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    int Given = 0;
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x30) != 0 || NameOf(Read<uintptr_t>(Object + 0x10)) != "InventoryComponent")
            continue;
        const uintptr_t Owner = Read<uintptr_t>(Object + 0x20);
        if (!Owner || NameOf(Read<uintptr_t>(Owner + 0x10)).find("PlayerState") == std::string::npos)
            continue;
        struct
        {
            uintptr_t Data, Rarity;
            bool Broken, FlagAsNew, SystemAdd;
            uint8_t Pad;
            int32_t Position;
            bool AllowP2Reserve;
            uint8_t Pad2[7];
            uintptr_t Result;
        } Params{Data, Rarity, Broken, true, true, 0, -1, false, {}, 0};
        static_assert(sizeof(Params) == 0x28);
        Read<ProcessEventFn*>(Object)[kProcessEventIndex](Object, Create, &Params);
        Given += Params.Result != 0;
    }
    Log("Give: " + DataName + (RarityName.empty() ? "" : " " + RarityName) + (Broken ? " kaputt" : "") + " an " + std::to_string(Given) + " Inventare");
}

// Testhilfe (opt-haendler-client): Host aendert den Schrott der Mission, "!scrap 200" oder "!scrap -50". Natives
// AbstractCurrency.AddAmount am ScrapWallet des Missions-GameState (+0x3C0), die Mod schickt den Stand an den Client.
void ScrapCommand(const std::wstring& Args)
{
    const int32_t Delta = _wtoi(Args.c_str());
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t State = World ? Read<uintptr_t>(World + kWorldGameState) : 0;
    const uintptr_t Add = FindObject("AddAmount", "AbstractCurrency");
    if (!State || !Add || !Read<uintptr_t>(World + kWorldGameMode))
    {
        Log("Scrap: nicht moeglich (kein Host oder kein GameState)");
        return;
    }
    const uintptr_t Wallet = Read<uintptr_t>(State + 0x3C0);
    if (!Wallet || NameOf(Read<uintptr_t>(Wallet + 0x10)) != "ScrapWalletComponent")
    {
        Log("Scrap: Konto nicht gefunden");
        return;
    }
    int32_t Params = Delta;
    Read<ProcessEventFn*>(Wallet)[kProcessEventIndex](Wallet, Add, &Params);
    Log("Scrap: " + std::to_string(Delta) + ", Stand " + std::to_string(Read<int32_t>(Wallet + 0x100)));
}

// Testhilfe (opt-haendler-client, opt-werkbank-station): Haendler und Station ohne Maus und Tastatur bedienen, in der Instanz, die
// den Befehl bekommt (meist der Client). Alles ueber ProcessEvent, also dieselben Funktionen, die Knopf und Panel rufen.
//   !shop poi          POIs der Karte (BP_MetaBase_POI_C und Unterklassen) mit Ort
//   !shop go <n>       eigene Figur auf POI n stellen (K2_TeleportTo), das Betreten oeffnet das Panel
//   !shop away         eigene Figur 3000 Einheiten wegstellen, das Verlassen schliesst das Panel
//   !shop list         Angebote aller Haendler-Panels mit Preis, dazu das eigene Inventar
//   !shop buy <n>      UShopPanelItem.TryBuyItem am Angebot n
//   !shop panels       Inventar-Panels zum eigenen Inventar mit Offen-Zustand
//   !shop confirm <n>  HandleConfirmButton an Panel n (Bestaetigen)
//   !shop click <Name>  OnButtonClicked an jedem sichtbaren Knopf mit diesem Namen, etwa ContinueButton
//   !shop call <Klasse> <Funktion>   parameterlose UFunction an jeder Instanz, etwa ein Knopf-Ereignis
//   !shop upgrade      zwei gleiche Items des eigenen Inventars ueber UInventoryComponent.UpgradeItems zur naechsten Seltenheit
namespace
{
bool IsClass(uintptr_t Object, const char* ClassName)
{
    uintptr_t Class = Object ? Read<uintptr_t>(Object + 0x10) : 0;
    for (int Depth = 0; Class && Depth < 40; ++Depth, Class = Read<uintptr_t>(Class + 0x40))
        if (NameOf(Class) == ClassName)
            return true;
    return false;
}

// Lebende Instanzen (kein Klassen-Defaultobjekt) einer Klasse oder Unterklasse.
std::vector<uintptr_t> InstancesOf(const char* ClassName)
{
    std::vector<uintptr_t> Result;
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (Object && (Read<uint32_t>(Object + 8) & 0x70) == 0 && IsClass(Object, ClassName))
            Result.push_back(Object);
    }
    return Result;
}

void Call(uintptr_t Object, uintptr_t Function, void* Params)
{
    Read<ProcessEventFn*>(Object)[kProcessEventIndex](Object, Function, Params);
}

// Eigener Controller: UWorld.OwningGameInstance (+0x228), LocalPlayers (+0x38), UPlayer.PlayerController (+0x30).
uintptr_t LocalController()
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Instance = World ? Read<uintptr_t>(World + 0x228) : 0;
    const uintptr_t Players = Instance ? Read<uintptr_t>(Instance + 0x38) : 0;
    const uintptr_t Local = Players ? Read<uintptr_t>(Players) : 0;
    return Local ? Read<uintptr_t>(Local + 0x30) : 0;
}

struct Vec
{
    double X, Y, Z;
};

Vec LocationOf(uintptr_t Actor)
{
    const uintptr_t Get = FindObject("K2_GetActorLocation", "Actor");
    Vec Result{};
    if (Actor && Get)
        Call(Actor, Get, &Result);
    return Result;
}

bool Teleport(uintptr_t Actor, Vec To)
{
    const uintptr_t Fn = FindObject("K2_TeleportTo", "Actor");
    if (!Actor || !Fn)
        return false;
    // FVector DestLocation, FRotator DestRotation, bool ReturnValue; grosszuegig und genullt.
    uint8_t Params[0x80]{};
    memcpy(Params, &To, sizeof(To));
    Call(Actor, Fn, Params);
    return true;
}

// Eigenes Inventar: UInventoryComponent, dessen Outer der PlayerState des eigenen Controllers (+0x2B0) ist.
uintptr_t LocalInventory()
{
    const uintptr_t Controller = LocalController();
    const uintptr_t State = Controller ? Read<uintptr_t>(Controller + 0x2B0) : 0;
    if (!State)
        return 0;
    for (const uintptr_t Object : InstancesOf("InventoryComponent"))
        if (Read<uintptr_t>(Object + 0x20) == State)
            return Object;
    return 0;
}

std::vector<uintptr_t> ItemsOf(uintptr_t Inventory)
{
    std::vector<uintptr_t> Items;
    const uintptr_t Data = Inventory ? Read<uintptr_t>(Inventory + 0x120) : 0;
    const int32_t Count = Data ? Read<int32_t>(Inventory + 0x128) : 0;
    for (int32_t i = 0; i < Count && i < 512; ++i)
        if (const uintptr_t Item = Read<uintptr_t>(Data + 8 * i))
            Items.push_back(Item);
    return Items;
}

std::string ItemText(uintptr_t Item)
{
    const uintptr_t Data = Read<uintptr_t>(Item + 0xE0), Rarity = Read<uintptr_t>(Item + 0x118);
    return (Data ? NameOf(Data) : std::string("?")) + " " + (Rarity ? NameOf(Rarity) : std::string("-")) + (Read<uint8_t>(Item + 0x180) ? " Rucksack" : "");
}
}

void ShopCommand(const std::wstring& Args)
{
    const std::string Text(Args.begin(), Args.end());
    const size_t Space = Text.find(' ');
    const std::string Action = Text.substr(0, Space);
    const int Index = Space == std::string::npos ? -1 : atoi(Text.c_str() + Space + 1);
    const uintptr_t Controller = LocalController();
    const uintptr_t Pawn = Controller ? Read<uintptr_t>(Controller + 0x2E8) : 0;
    if (Action == "poi" || Action == "go")
    {
        const std::vector<uintptr_t> Pois = InstancesOf("MetaBasePoi");
        for (size_t i = 0; i < Pois.size(); ++i)
        {
            const Vec At = LocationOf(Pois[i]);
            if (Action == "poi")
                Log("Shop: POI " + std::to_string(i) + " " + NameOf(Read<uintptr_t>(Pois[i] + 0x10)) + " " + NameOf(Pois[i]) + " bei " +
                    std::to_string(static_cast<int>(At.X)) + " " + std::to_string(static_cast<int>(At.Y)) + " " + std::to_string(static_cast<int>(At.Z)));
        }
        if (Action == "poi")
        {
            const Vec Own = LocationOf(Pawn);
            Log("Shop: " + std::to_string(Pois.size()) + " POIs, eigene Figur bei " + std::to_string(static_cast<int>(Own.X)) + " " +
                std::to_string(static_cast<int>(Own.Y)) + " " + std::to_string(static_cast<int>(Own.Z)));
            return;
        }
        if (Index < 0 || Index >= static_cast<int>(Pois.size()) || !Pawn)
        {
            Log("Shop: POI oder eigene Figur fehlt");
            return;
        }
        Vec To = LocationOf(Pois[Index]);
        To.Z = LocationOf(Pawn).Z;
        Log(std::string("Shop: Figur auf POI ") + std::to_string(Index) + (Teleport(Pawn, To) ? " gestellt" : " nicht gestellt"));
    }
    else if (Action == "away")
    {
        Vec To = LocationOf(Pawn);
        To.X += 3000;
        Log(std::string("Shop: Figur ") + (Teleport(Pawn, To) ? "weggestellt" : "nicht weggestellt"));
    }
    else if (Action == "list" || Action == "buy")
    {
        // UShopPanelItem: CostDisplay +0x850 (Preis +0x384), ItemInstance +0x868, aus dem Code von TryBuyItem.
        std::vector<uintptr_t> Offers;
        for (const uintptr_t Offer : InstancesOf("ShopPanelItem"))
            if (Read<uintptr_t>(Offer + 0x868))
                Offers.push_back(Offer);
        if (Action == "list")
        {
            for (size_t i = 0; i < Offers.size(); ++i)
            {
                const uintptr_t Display = Read<uintptr_t>(Offers[i] + 0x850);
                Log("Shop: Angebot " + std::to_string(i) + " " + ItemText(Read<uintptr_t>(Offers[i] + 0x868)) + " fuer " +
                    std::to_string(Display ? Read<int32_t>(Display + 0x384) : -1) + " in " + NameOf(Read<uintptr_t>(Read<uintptr_t>(Offers[i] + 0x3A8) + 0x10)));
            }
            const std::vector<uintptr_t> Items = ItemsOf(LocalInventory());
            for (size_t i = 0; i < Items.size(); ++i)
                Log("Shop: Inventar " + std::to_string(i) + " " + ItemText(Items[i]));
            Log("Shop: " + std::to_string(Offers.size()) + " Angebote, " + std::to_string(Items.size()) + " Items im eigenen Inventar");
            return;
        }
        const uintptr_t Buy = FindObject("TryBuyItem", "ShopPanelItem");
        if (Index < 0 || Index >= static_cast<int>(Offers.size()) || !Buy)
        {
            Log("Shop: Angebot oder TryBuyItem fehlt");
            return;
        }
        const std::string Name = ItemText(Read<uintptr_t>(Offers[Index] + 0x868));
        Call(Offers[Index], Buy, nullptr);
        Log("Shop: TryBuyItem an Angebot " + std::to_string(Index) + " (" + Name + ") gerufen");
    }
    else if (Action == "upgrade")
    {
        const uintptr_t Inventory = LocalInventory();
        const uintptr_t Upgrade = FindObject("UpgradeItems", "InventoryComponent");
        const std::vector<uintptr_t> Items = ItemsOf(Inventory);
        for (size_t a = 0; Upgrade && a < Items.size(); ++a)
            for (size_t b = a + 1; b < Items.size(); ++b)
            {
                const uintptr_t Data = Read<uintptr_t>(Items[a] + 0xE0), Rarity = Read<uintptr_t>(Items[a] + 0x118);
                const uintptr_t Next = Rarity ? Read<uintptr_t>(Rarity + 0x58) : 0; // URarity.NextUpgradableRarity
                if (!Data || !Next || Data != Read<uintptr_t>(Items[b] + 0xE0) || Rarity != Read<uintptr_t>(Items[b] + 0x118))
                    continue;
                uintptr_t Inputs[2]{Items[a], Items[b]};
                struct
                {
                    uintptr_t* Data;
                    int32_t Count, Max;
                    uintptr_t Output, Rarity;
                    bool FlagAsNew;
                    uint8_t Pad[3];
                    int32_t Position;
                    uintptr_t Result;
                } Params{Inputs, 2, 2, Data, Next, true, {}, -1, 0};
                static_assert(sizeof(Params) == 0x30);
                Call(Inventory, Upgrade, &Params);
                Log("Shop: UpgradeItems mit zweimal " + NameOf(Data) + " zu " + NameOf(Next) + ", Ausgabe " + (Params.Result ? "ja" : "nein"));
                return;
            }
        Log("Shop: keine zwei gleichen aufwertbaren Items im eigenen Inventar");
    }
    else if (Action == "panels" || Action == "confirm")
    {
        // Inventar-Panels zum eigenen Inventar (UInventoryPanel.Inventory +0x370). "confirm <n>" ruft HandleConfirmButton nur an
        // diesem einen Panel: An einer unbenutzten Instanz stuerzt die Funktion ab (im Test gesehen).
        const uintptr_t Inventory = LocalInventory();
        std::vector<uintptr_t> Panels;
        for (const uintptr_t Panel : InstancesOf("InventoryPanel"))
            if (Read<uintptr_t>(Panel + 0x370) == Inventory)
                Panels.push_back(Panel);
        if (Action == "panels")
        {
            for (size_t i = 0; i < Panels.size(); ++i)
            {
                char Address[32];
                sprintf_s(Address, "%llx", static_cast<unsigned long long>(Panels[i]));
                const uintptr_t Container = Read<uintptr_t>(Panels[i] + 0x330);
                Log("Shop: Panel " + std::to_string(i) + " " + Address + " " + NameOf(Read<uintptr_t>(Panels[i] + 0x20)) + ", Container " +
                    (Container ? (Read<uint8_t>(Container + 0x328) ? "offen" : "zu") : "fehlt") + ", Drag " +
                    std::to_string(Read<uint8_t>(Panels[i] + 0x788)));
            }
            Log("Shop: " + std::to_string(Panels.size()) + " Inventar-Panels zum eigenen Inventar");
            // Haendler-Panels: sichtbar (UWidget.IsVisible), im Viewport, Rabattgrenzen (UShopPanel.DiscountChance +0x3D8,
            // MinDiscountIfDiscounted +0x3DC, MaxDiscountIfDiscounted +0x3E0, Offsets aus dem SDK von 26187885, darum nur Diagnose).
            const uintptr_t Visible = FindObject("IsVisible", "Widget"), InViewport = FindObject("IsInViewport", "UserWidget");
            for (const uintptr_t Shop : InstancesOf("ShopPanel"))
            {
                uint8_t Shown[8]{}, Viewport[8]{};
                if (Visible)
                    Call(Shop, Visible, Shown);
                if (InViewport)
                    Call(Shop, InViewport, Viewport);
                Log("Shop: " + NameOf(Read<uintptr_t>(Shop + 0x10)) + " sichtbar " + std::to_string(Shown[0]) + ", im Viewport " +
                    std::to_string(Viewport[0]) + ", Rabatt Chance " + std::to_string(Read<float>(Shop + 0x3D8)) + " von " +
                    std::to_string(Read<float>(Shop + 0x3DC)) + " bis " + std::to_string(Read<float>(Shop + 0x3E0)));
            }
            return;
        }
        const uintptr_t Confirm = FindObject("HandleConfirmButton", "InventoryPanel");
        if (Index < 0 || Index >= static_cast<int>(Panels.size()) || !Confirm)
        {
            Log("Shop: Panel oder HandleConfirmButton fehlt");
            return;
        }
        Call(Panels[Index], Confirm, nullptr);
        Log("Shop: HandleConfirmButton an Panel " + std::to_string(Index) + " gerufen");
    }
    else if (Action == "click")
    {
        // "!shop click <Name>": UDefaultFocusableButton.OnButtonClicked an jedem sichtbaren Knopf mit diesem Objektnamen, etwa
        // ContinueButton in der Rundenuebersicht. Sichtbar ueber UWidget.IsVisible.
        const std::string Name = Space == std::string::npos ? std::string{} : Text.substr(Space + 1);
        const uintptr_t Clicked = FindObject("OnButtonClicked", "DefaultFocusableButton"), Visible = FindObject("IsVisible", "Widget");
        int Pressed = 0;
        for (const uintptr_t Button : InstancesOf("DefaultFocusableButton"))
        {
            if (!Clicked || !Visible || NameOf(Button) != Name)
                continue;
            uint8_t Shown[8]{};
            Call(Button, Visible, Shown);
            if (!Shown[0])
                continue;
            uint8_t Params[0x40]{};
            Call(Button, Clicked, Params);
            ++Pressed;
        }
        Log("Shop: Knopf " + Name + " an " + std::to_string(Pressed) + " sichtbaren Instanzen gedrueckt");
    }
    else if (Action == "press")
    {
        // "!shop press <n>": das eigene Klick-Ereignis des Angebots n (WI_MerchantPanel_BuyableItem_C), also der Weg des Knopfs im
        // Spiel samt allem, was sein Blueprint davor und danach tut.
        std::vector<uintptr_t> Offers;
        for (const uintptr_t Offer : InstancesOf("ShopPanelItem"))
            if (Read<uintptr_t>(Offer + 0x868))
                Offers.push_back(Offer);
        if (Index < 0 || Index >= static_cast<int>(Offers.size()))
        {
            Log("Shop: Angebot fehlt");
            return;
        }
        static const char* const Events[] = {
            "BndEvt__WI_MerchantPanel_BuyableItem_ButtonComponent_K2Node_ComponentBoundEvent_0_OnButtonClickedEvent__DelegateSignature",
            "OnClickResponse_Event"};
        uintptr_t Function = 0;
        const char* Used = "";
        for (const char* Event : Events)
            for (uintptr_t Class = Read<uintptr_t>(Offers[Index] + 0x10); Class && !Function; Class = Read<uintptr_t>(Class + 0x40))
                if ((Function = FindObject(Event, NameOf(Class).c_str())) != 0)
                    Used = Event;
        if (!Function)
        {
            Log("Shop: Klick-Ereignis des Angebots nicht gefunden");
            return;
        }
        const std::string Name = ItemText(Read<uintptr_t>(Offers[Index] + 0x868));
        uint8_t Params[0x100]{};
        Call(Offers[Index], Function, Params);
        Log("Shop: " + std::string(Used).substr(0, 40) + " an Angebot " + std::to_string(Index) + " (" + Name + ") gerufen");
    }
    else if (Action == "call")
    {
        // "!shop call <Klasse> <Funktion>": parameterlose UFunction an jeder lebenden Instanz der Klasse, etwa der Zurueck-Knopf des
        // Haendler-Panels. Die Funktion wird in der Klasse der Instanz und ihren Oberklassen gesucht.
        const size_t Second = Space == std::string::npos ? std::string::npos : Text.find(' ', Space + 1);
        if (Second == std::string::npos)
        {
            Log("Shop: call braucht Klasse und Funktion");
            return;
        }
        const std::string ClassName = Text.substr(Space + 1, Second - Space - 1), FunctionName = Text.substr(Second + 1);
        int Called = 0;
        for (const uintptr_t Object : InstancesOf(ClassName.c_str()))
        {
            uintptr_t Function = 0;
            for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class && !Function; Class = Read<uintptr_t>(Class + 0x40))
                Function = FindObject(FunctionName.c_str(), NameOf(Class).c_str());
            if (!Function)
                continue;
            uint8_t Params[0x100]{};
            Call(Object, Function, Params);
            ++Called;
        }
        Log("Shop: " + ClassName + "." + FunctionName + " an " + std::to_string(Called) + " Instanzen gerufen");
    }
    else
        Log("Shop: unbekannt, siehe Kommentar in Cheats.cpp");
}

// Testhilfe (gegner-alle): Host spawnt einen Boss ueber EnemySpawner::SpawnSpecificEnemy, "!boss SpikeBoss [Abstand]".
// Namen der EnemyType-Assets: SpikeBoss, FrankensteinBoss, BlobBallBoss, BallerinaBoss, LazyBoss, HQBoss. Das Asset muss geladen sein,
// sonst vorher "!load <Pfad>". Spawn Abstand (Standard 600) vor der ersten Spielerfigur. Nur beim Host, die Replikation macht das Spiel.
void BossCommand(const std::wstring& Args)
{
    const std::string Text(Args.begin(), Args.end());
    const size_t Space = Text.find(' ');
    const std::string TypeName = Text.substr(0, Space);
    const double Distance = Space == std::string::npos ? 600.0 : atof(Text.c_str() + Space + 1);
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    uintptr_t Type = FindObject(TypeName.c_str(), TypeName.c_str());
    // opt-gegner-indikatoren: auch normale Gegnertypen nach "!load", der Outer eines Assets ist sein Paketpfad.
    if (!Type)
        Type = FindObject(TypeName.c_str(), ("/Game/DDSurvivors/Blueprints/Enemies/Definitions/" + TypeName).c_str());
    if (!Type)
    {
        // Nicht geladen: Pfad aus DA_BossSpawnLookup lesen (BossSpawnInfos +0x28, je 0x68, BossID +0x00, weicher Verweis auf den
        // EnemyType ab +0x20: Paketname, Assetname) und laden. Alle Eintraege landen im Log.
        const uintptr_t Lookup = FindObject("DA_BossSpawnLookup", nullptr);
        const uintptr_t Entries = Lookup ? Read<uintptr_t>(Lookup + 0x28) : 0;
        const int32_t EntryCount = Lookup ? Read<int32_t>(Lookup + 0x30) : 0;
        Log("Boss: Lookup " + std::string(Lookup ? "gefunden" : "fehlt") + ", Eintraege " + std::to_string(EntryCount));
        for (int32_t e = 0; Entries && e < EntryCount && e < 16 && !Type; ++e)
        {
            const uintptr_t Entry = Entries + e * 0x68;
            const std::string Id = NameFromIndex(Read<int32_t>(Entry)), Package = NameFromIndex(Read<int32_t>(Entry + 0x18)),
                              Asset = NameFromIndex(Read<int32_t>(Entry + 0x20));
            Log("Boss: Lookup " + Id + " -> " + Package + "." + Asset);
            if (Asset == TypeName || Id == TypeName)
                Type = LoadObjectByPath(Package + "." + Asset);
        }
    }
    const uintptr_t Spawn = FindObject("SpawnSpecificEnemy", "EnemySpawner");
    const uintptr_t GetLocation = FindObject("K2_GetActorLocation", "Actor");
    if (!World || !Read<uintptr_t>(World + kWorldGameMode) || !Type || !Spawn || !GetLocation)
    {
        Log("Boss: Host " + std::to_string(World && Read<uintptr_t>(World + kWorldGameMode) != 0) + ", Typ " + std::to_string(Type != 0) +
            ", SpawnSpecificEnemy " + std::to_string(Spawn != 0) + ", K2_GetActorLocation " + std::to_string(GetLocation != 0));
        Log("Boss: " + TypeName + " nicht moeglich (kein Host, Typ nicht geladen oder Funktion fehlt)");
        return;
    }
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    uintptr_t Spawner = 0, Character = 0;
    for (int32_t i = 0; i < Count && !(Spawner && Character); ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x30) != 0)
            continue;
        for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class; Class = Read<uintptr_t>(Class + 0x40))
        {
            const std::string Name = NameOf(Class);
            if (!Spawner && Name == "EnemySpawner")
                Spawner = Object;
            if (!Character && Name == "DDCharacter")
                Character = Object;
        }
    }
    if (!Spawner || !Character)
    {
        Log("Boss: kein EnemySpawner oder keine Spielerfigur");
        return;
    }
    struct { double X, Y, Z; } Location{};
    Read<ProcessEventFn*>(Character)[kProcessEventIndex](Character, GetLocation, &Location);
    struct
    {
        uintptr_t EnemyType, Instigator;
        double Rotation[4];      // FTransform ab 0x10: FQuat
        double Translation[3];   // 0x30, danach 8 Byte Fuellung
        double Pad0;
        double Scale[3];         // 0x50
        double Pad1;             // FTransform endet bei 0x70, Gesamt 0x60
        int32_t ThreatLevel, BaseThreatLevel;
        bool ApplySpawnerEffects, ClampToExtends;
        uint8_t Pad2[6];
        uintptr_t Bounds, Result;
    } Params{};
    static_assert(sizeof(Params) == 0x90);
    Params.EnemyType = Type;
    Params.Rotation[3] = 1.0;
    Params.Translation[0] = Location.X + Distance;
    Params.Translation[1] = Location.Y;
    Params.Translation[2] = Location.Z;
    Params.Scale[0] = Params.Scale[1] = Params.Scale[2] = 1.0;
    Params.ThreatLevel = Params.BaseThreatLevel = -1;
    Params.ApplySpawnerEffects = true;
    Params.ClampToExtends = false;
    Read<ProcessEventFn*>(Spawner)[kProcessEventIndex](Spawner, Spawn, &Params);
    Log("Boss: " + TypeName + (Params.Result ? " gespawnt (" + NameOf(Read<uintptr_t>(Params.Result + 0x10)) + ")" : " nicht entstanden"));
}

// Testhilfe (gegner-alle): Host fuegt dem Boss ueber GameplayStatics::ApplyDamage Schaden zu, "!bossdmg 30000". Der Weg ist der des
// Spiels (OnTakeAnyDamage -> DamageToGASTranslator), Phasenwechsel und Tod laufen wie im Kampf. Nur beim Host.
void BossDamageCommand(const std::wstring& Args)
{
    const float Amount = static_cast<float>(_wtof(Args.c_str()));
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Statics = FindObject("Default__GameplayStatics", nullptr);
    const uintptr_t Apply = FindObject("ApplyDamage", "GameplayStatics");
    if (!World || !Read<uintptr_t>(World + kWorldGameMode) || !Statics || !Apply || Amount <= 0)
    {
        Log("BossDmg: nicht moeglich (kein Host, Funktion fehlt oder Schaden <= 0)");
        return;
    }
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    uintptr_t Boss = 0, Controller = 0;
    for (int32_t i = 0; i < Count && !(Boss && Controller); ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x30) != 0)
            continue;
        for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class; Class = Read<uintptr_t>(Class + 0x40))
        {
            const std::string Name = NameOf(Class);
            if (!Boss && Name == "BP_BossEnemy_C")
                Boss = Object;
            if (!Controller && Name == "DDPlayerController")
                Controller = Object;
        }
    }
    if (!Boss)
    {
        Log("BossDmg: kein Boss");
        return;
    }
    struct { uintptr_t Actor; float Damage; uint32_t Pad; uintptr_t Instigator, Causer, DamageType; float Result; uint32_t Pad2; } Params{Boss, Amount, 0, Controller, Boss, 0, 0, 0};
    static_assert(sizeof(Params) == 0x30);
    Read<ProcessEventFn*>(Statics)[kProcessEventIndex](Statics, Apply, &Params);
    Log("BossDmg: " + std::to_string(static_cast<int>(Amount)) + " Schaden, angewendet " + std::to_string(static_cast<int>(Params.Result)));
}

// Testhilfe (gegner-alle): Host setzt einen BossfightSpawnPoint in die Welt und startet den Bosskampf wie das Spiel (Intro, Objective,
// Lebensleiste, Phasen), "!bossfight SpikeBoss [Abstand]". Klasse /Game/DDSurvivors/Blueprints/Enemies/Bosses/BP_<Name>SpawnPoint.
// Nur beim Host, die Replikation macht das Spiel.
void BossFightCommand(const std::wstring& Args)
{
    const std::string Text(Args.begin(), Args.end());
    const size_t Space = Text.find(' ');
    const std::string TypeName = Text.substr(0, Space);
    const double Distance = Space == std::string::npos ? 800.0 : atof(Text.c_str() + Space + 1);
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Statics = FindObject("Default__GameplayStatics", nullptr);
    const uintptr_t Begin = FindObject("BeginDeferredActorSpawnFromClass", "GameplayStatics");
    const uintptr_t Finish = FindObject("FinishSpawningActor", "GameplayStatics");
    const uintptr_t GetLocation = FindObject("K2_GetActorLocation", "Actor");
    const uintptr_t Trigger = FindObject("TriggerBossfight", "BossfightSpawnPoint");
    const std::string ClassName = "BP_" + TypeName + "SpawnPoint_C";
    uintptr_t Class = FindObject(ClassName.c_str(), nullptr);
    if (!Class)
        Class = LoadObjectByPath("/Game/DDSurvivors/Blueprints/Enemies/Bosses/BP_" + TypeName + "SpawnPoint.BP_" + TypeName + "SpawnPoint_C");
    const bool IsHost = World && Read<uintptr_t>(World + kWorldGameMode) != 0;
    if (!World || !Statics || !Begin || !Finish || !GetLocation || !Trigger || !Class)
    {
        Log("BossFight: " + TypeName + " nicht moeglich (Klasse " +
            std::to_string(Class != 0) + ", Begin " + std::to_string(Begin != 0) + ", Finish " + std::to_string(Finish != 0) + ", Trigger " +
            std::to_string(Trigger != 0) + ")");
        return;
    }
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    uintptr_t Character = 0;
    for (int32_t i = 0; i < Count && !Character; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x30) != 0)
            continue;
        for (uintptr_t C = Read<uintptr_t>(Object + 0x10); C; C = Read<uintptr_t>(C + 0x40))
            if (NameOf(C) == "DDCharacter")
            {
                Character = Object;
                break;
            }
    }
    if (!Character)
    {
        Log("BossFight: keine Spielerfigur");
        return;
    }
    struct { double X, Y, Z; } Location{};
    Read<ProcessEventFn*>(Character)[kProcessEventIndex](Character, GetLocation, &Location);
    struct Transform
    {
        double Rotation[4];
        double Translation[3];
        double Pad0;
        double Scale[3];
        double Pad1;
    };
    static_assert(sizeof(Transform) == 0x60);
    Transform Where{};
    Where.Rotation[3] = 1.0;
    Where.Translation[0] = Location.X + Distance;
    Where.Translation[1] = Location.Y;
    Where.Translation[2] = Location.Z;
    Where.Scale[0] = Where.Scale[1] = Where.Scale[2] = 1.0;
    struct
    {
        uintptr_t WorldContext, ActorClass;
        Transform Spawn;
        uint8_t Collision, Pad[7];
        uintptr_t Owner;
        uint8_t ScaleMethod, Pad2[7];
        uintptr_t Result;
    } BeginParams{World, Class, Where, 1, {}, 0, 0, {}, 0};
    static_assert(sizeof(BeginParams) == 0x90);
    Read<ProcessEventFn*>(Statics)[kProcessEventIndex](Statics, Begin, &BeginParams);
    if (!BeginParams.Result)
    {
        Log("BossFight: SpawnPoint nicht entstanden");
        return;
    }
    struct
    {
        uintptr_t Actor, Pad;
        Transform Spawn;
        uint8_t ScaleMethod, Pad2[7];
        uintptr_t Result;
    } FinishParams{BeginParams.Result, 0, Where, 0, {}, 0};
    static_assert(sizeof(FinishParams) == 0x80);
    Read<ProcessEventFn*>(Statics)[kProcessEventIndex](Statics, Finish, &FinishParams);
    // Beim Client nur der Spawnpunkt wie im Level, Boss und Kampfstart kommen vom Host.
    if (!IsHost)
    {
        Log("BossFight: " + TypeName + " Spawnpunkt beim Client gesetzt");
        return;
    }
    if (!Read<uintptr_t>(BeginParams.Result + 0x498))
        if (const uintptr_t SpawnBoss = FindObject("SpawnBoss", "BossfightSpawnPoint"))
            Read<ProcessEventFn*>(BeginParams.Result)[kProcessEventIndex](BeginParams.Result, SpawnBoss, nullptr);
    float Delay = 1.0f;
    Read<ProcessEventFn*>(BeginParams.Result)[kProcessEventIndex](BeginParams.Result, Trigger, &Delay);
    {
        const uintptr_t ObjectiveClass = Read<uintptr_t>(BeginParams.Result + 0x530);
        Log("BossFight: BossObjectiveClass " + (ObjectiveClass ? NameOf(ObjectiveClass) : std::string("keine")));
    }
    Log("BossFight: " + TypeName + " Spawnpunkt gesetzt, Kampf angestossen (SpawnedBoss " +
        (Read<uintptr_t>(BeginParams.Result + 0x498) ? NameOf(Read<uintptr_t>(Read<uintptr_t>(BeginParams.Result + 0x498) + 0x10)) : std::string("keiner")) + ")");
}

// Client: speist ein lokales DDBossObjective aus den replizierten Boss-HP (CommonAttributeSet des Bosses). Der Host fuettert es
// ueber BossComponent::OnDamageReceivedEvent, dieses Ereignis gibt es beim Client nicht. Aufruf einmal je Sekunde.
// Rueckgabe: true, wenn ein Wert gesetzt wurde.
bool SyncBossBar()
{
    static const uintptr_t SetProgress = FindObject("SetCurrentProgressCounter", "DDObjective");
    if (!SetProgress)
        return false;
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    uintptr_t Boss = 0, Objective = 0;
    for (int32_t i = 0; i < Count && !(Boss && Objective); ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x30) != 0)
            continue;
        for (uintptr_t C = Read<uintptr_t>(Object + 0x10); C; C = Read<uintptr_t>(C + 0x40))
        {
            const std::string Name = NameOf(C);
            if (!Boss && Name == "BP_BossEnemy_C" && (Read<uint8_t>(Object + 0x5D) & 0x02) == 0)
                Boss = Object;
            if (!Objective && Name == "DDBossObjective")
                Objective = Object;
        }
    }
    if (!Boss || !Objective)
        return false;
    // Die Gesundheit des Bosses: das CommonAttributeSet mit dem grossen Maximum (der Boss besitzt mehrere Saetze).
    float Health = -1, MaxHealth = 0;
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x30) != 0 || Read<uintptr_t>(Object + 0x20) != Boss ||
            NameOf(Read<uintptr_t>(Object + 0x10)) != "CommonAttributeSet")
            continue;
        const float Max = Read<float>(Object + 0x30 + 0xC);
        if (Max > MaxHealth)
        {
            MaxHealth = Max;
            Health = Read<float>(Object + 0xA0 + 0xC);
        }
    }
    if (Health < 0 || MaxHealth <= 1)
        return false;
    // Nur bei Aenderung setzen: Jeder Aufruf sendet OnProgressChanged an die Anzeige.
    static uintptr_t LastObjective = 0;
    static int64_t LastHealth = -1, LastMax = -1;
    // Nie auf 0 setzen: Das schliesst das Objective ab (HandleObjectiveCompleted), dem Client-Subsystem fehlt dafuer das aktuelle
    // Objective, und das Spiel stuerzt ab. Die Anzeige verschwindet, wenn der Boss weg ist (TickBossBar).
    if (Health < 1)
        return false;
    const int64_t NewHealth = static_cast<int64_t>(Health), NewMax = static_cast<int64_t>(MaxHealth);
    if (Objective == LastObjective && NewHealth == LastHealth && NewMax == LastMax)
        return true;
    constexpr uintptr_t kObjectiveMaxHealth = 0xD0;
    *reinterpret_cast<int64_t*>(Objective + kObjectiveMaxHealth) = NewMax;
    struct { int64_t Progress; } Params{NewHealth};
    Log("BossBar: setze " + std::to_string(NewHealth) + " / " + std::to_string(NewMax));
    Read<ProcessEventFn*>(Objective)[kProcessEventIndex](Objective, SetProgress, &Params);
    LastObjective = Objective;
    LastHealth = NewHealth;
    LastMax = NewMax;
    return true;
}

// Client, einmal je Sekunde: legt die Boss-Leiste an, sobald ein lebender Boss repliziert ist, haelt sie aktuell und nimmt sie
// weg, wenn der Boss weg ist. Beim Host gibt es das Objective ueber den Spawnpunkt, hier nur Client-Anzeige aus replizierten Werten.
void TickBossBar()
{
    static bool Shown = false;
    static const uintptr_t Removed = FindObject("OnObjectiveRemoved", "DDObjectiveWidget");
    // FindInstance vergleicht nur den genauen Klassennamen, das Objective ist aber eine Blueprint-Unterklasse von DDBossObjective.
    uintptr_t Objective = 0;
    {
        const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
        const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
        for (int32_t i = 0; i < Count && !Objective; ++i)
        {
            const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
            const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
            if (!Object || (Read<uint32_t>(Object + 8) & 0x30) != 0)
                continue;
            for (uintptr_t C = Read<uintptr_t>(Object + 0x10); C; C = Read<uintptr_t>(C + 0x40))
                if (NameOf(C) == "DDBossObjective")
                {
                    Objective = Object;
                    break;
                }
        }
    }
    if (!Objective)
    {
        Shown = false;
        // Anlegen nur, wenn wirklich ein lebender Boss da ist (BossBarCommand prueft das und meldet sonst nur einmal im Log).
        static bool Reported = false;
        const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
        const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
        bool Live = false;
        for (int32_t i = 0; i < Count && !Live; ++i)
        {
            const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
            const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
            if (!Object || (Read<uint32_t>(Object + 8) & 0x30) != 0)
                continue;
            for (uintptr_t C = Read<uintptr_t>(Object + 0x10); C; C = Read<uintptr_t>(C + 0x40))
                if (NameOf(C) == "BP_BossEnemy_C")
                {
                    Live = (Read<uint8_t>(Object + 0x5D) & 0x02) == 0;
                    break;
                }
        }
        if (Live)
        {
            if (!Reported)
                Log("BossBar: Boss repliziert, lege lokale Leiste an");
            Reported = true;
            BossBarCommand(L"late");
        }
        else
            Reported = false;
        return;
    }
    // Das Objective braucht nach dem Anlegen einen Moment, bevor Werte gesetzt werden duerfen.
    static uintptr_t Settling = 0;
    static int SettleTicks = 0;
    if (Objective != Settling)
    {
        Settling = Objective;
        SettleTicks = 0;
    }
    if (SettleTicks < 3)
    {
        ++SettleTicks;
        return;
    }
    if (SyncBossBar())
    {
        Shown = true;
        return;
    }
    // Boss weg oder tot: Anzeige beenden und das Objective fallen lassen (es wird dabei vom Spiel selbst nicht entfernt).
    if (Shown && Removed)
    {
        const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
        const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
        for (int32_t i = 0; i < Count; ++i)
        {
            const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
            const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
            if (!Object || (Read<uint32_t>(Object + 8) & 0x30) != 0)
                continue;
            for (uintptr_t C = Read<uintptr_t>(Object + 0x10); C; C = Read<uintptr_t>(C + 0x40))
                if (NameOf(C) == "DDObjectiveWidget")
                {
                    // Nur die Anzeige mit Subsystem (die zweite Instanz hat keins und stuerzt bei OnObjectiveRemoved ab).
                    if (Read<uintptr_t>(Object + 0x378))
                    {
                        struct { uintptr_t Objective; } P{Objective};
                        Read<ProcessEventFn*>(Object)[kProcessEventIndex](Object, Removed, &P);
                    }
                    break;
                }
        }
        Log("BossBar: Boss weg, Leiste entfernt");
        Shown = false;
    }
}

// Versuch (gegner-alle): Client legt die Boss-Leiste lokal an, "!bossbar [Klasse]". Das Objective ist ein lokales Objekt der
// GameInstance (DDObjectiveSubsystem), der Host legt es ueber den Spawnpunkt an, beim Client gibt es keins. Hier: CreateAndStartObjective
// mit der Objective-Klasse, dann AddBossEnemy mit dem replizierten Boss.
void BossBarCommand(const std::wstring& Args)
{
    // Standard: die Klasse, die der Host ueber den Spawnpunkt nutzt (BP mit Phasenmarken und Leistenwidget).
    // "!bossbar late": Objective ohne AutoStart anlegen, Boss anmelden, dann StartTracking (Reihenfolge ausprobieren).
    if (Args == L"sync")
    {
        Log(SyncBossBar() ? "BossBar: Werte gesetzt" : "BossBar: kein Boss oder keine Leiste");
        return;
    }
    const bool Late = Args == L"late";
    const std::string ClassName = Args.empty() || Late ? std::string("DD_BaseBossObjective_C") : std::string(Args.begin(), Args.end());
    const uintptr_t Subsystem = FindInstance("DDObjectiveSubsystem");
    const uintptr_t Create = FindObject("CreateAndStartObjective", "DDObjectiveSubsystem");
    const uintptr_t AddBoss = FindObject("AddBossEnemy", "DDBossObjective");
    uintptr_t ObjectiveClass = FindObject(ClassName.c_str(), nullptr);
    if (!ObjectiveClass && ClassName == "DD_BaseBossObjective_C")
        ObjectiveClass = LoadObjectByPath("/Game/DDSurvivors/Blueprints/Objectives/DD_BaseBossObjective.DD_BaseBossObjective_C");
    uintptr_t Boss = 0;
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    for (int32_t i = 0; i < Count && !Boss; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x30) != 0)
            continue;
        for (uintptr_t C = Read<uintptr_t>(Object + 0x10); C; C = Read<uintptr_t>(C + 0x40))
            // Das Flag liegt in AActor, erst lesen, wenn die Klasse sicher ein Boss ist.
            if (NameOf(C) == "BP_BossEnemy_C" && (Read<uint8_t>(Object + 0x5D) & 0x02) == 0)
            {
                Boss = Object;
                break;
            }
    }
    if (!Subsystem || !Create || !AddBoss || !ObjectiveClass || !Boss)
    {
        Log("BossBar: nicht moeglich (Subsystem " + std::to_string(Subsystem != 0) + ", Create " + std::to_string(Create != 0) + ", AddBoss " +
            std::to_string(AddBoss != 0) + ", Klasse " + std::to_string(ObjectiveClass != 0) + ", Boss " + std::to_string(Boss != 0) + ")");
        return;
    }
    struct { uintptr_t Class; bool AutoStart; uint8_t Pad[7]; uintptr_t Result; } Create_{ObjectiveClass, !Late, {}, 0};
    Read<ProcessEventFn*>(Subsystem)[kProcessEventIndex](Subsystem, Create, &Create_);
    if (!Create_.Result)
    {
        Log("BossBar: Objective nicht entstanden");
        return;
    }
    struct { uintptr_t Boss; bool Result; uint8_t Pad[7]; } Add{Boss, false, {}};
    Read<ProcessEventFn*>(Create_.Result)[kProcessEventIndex](Create_.Result, AddBoss, &Add);
    // Die Phasenmarken der Anzeige entstehen beim Start und rechnen mit MaxHealth. Mit 0 fehlen sie, beim Ueberschreiten stuerzt der Client ab.
    if (Late)
    {
        float Max = 0;
        const uintptr_t C2 = Read<uintptr_t>(Base + kGObjects);
        const int32_t N2 = Read<int32_t>(Base + kGObjects + 8);
        for (int32_t i = 0; i < N2; ++i)
        {
            const uintptr_t Chunk = Read<uintptr_t>(C2 + 8 * (i >> 16));
            const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
            if (!Object || (Read<uint32_t>(Object + 8) & 0x30) != 0 || Read<uintptr_t>(Object + 0x20) != Boss ||
                NameOf(Read<uintptr_t>(Object + 0x10)) != "CommonAttributeSet")
                continue;
            if (Read<float>(Object + 0x30 + 0xC) > Max)
                Max = Read<float>(Object + 0x30 + 0xC);
        }
        if (Max > 1)
            *reinterpret_cast<int64_t*>(Create_.Result + 0xD0) = static_cast<int64_t>(Max);
        Log("BossBar: MaxHealth vor dem Start " + std::to_string(static_cast<int>(Max)));
    }
    if (Late)
        if (const uintptr_t Start = FindObject("StartTracking", "DDObjective"))
        {
            struct { uintptr_t World; } StartParams{Read<uintptr_t>(Base + kGWorld)};
            Read<ProcessEventFn*>(Create_.Result)[kProcessEventIndex](Create_.Result, Start, &StartParams);
        }
    // Ohne AutoStart meldet das Subsystem den Start nicht. Die Anzeige (DDObjectiveWidget) kennt das Objective erst nach OnObjectiveStarted.
    if (Late)
    {
        const uintptr_t Started = FindObject("OnObjectiveStarted", "DDObjectiveWidget");
        int Notified = 0;
        for (int32_t i = 0; Started && i < Count; ++i)
        {
            const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
            const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
            if (!Object || (Read<uint32_t>(Object + 8) & 0x30) != 0)
                continue;
            for (uintptr_t C = Read<uintptr_t>(Object + 0x10); C; C = Read<uintptr_t>(C + 0x40))
                if (NameOf(C) == "DDObjectiveWidget")
                {
                    if (Read<uintptr_t>(Object + 0x378))
                    {
                        struct { uintptr_t Objective; } P{Create_.Result};
                        Read<ProcessEventFn*>(Object)[kProcessEventIndex](Object, Started, &P);
                        ++Notified;
                    }
                    break;
                }
        }
        Log("BossBar: " + std::to_string(Notified) + " Anzeige(n) benachrichtigt");
    }
    Log("BossBar: Objective " + NameOf(Create_.Result) + " angelegt, AddBossEnemy " + std::to_string(Add.Result) + ", TrackedBosses " +
        std::to_string(Read<int32_t>(Create_.Result + 0xA8)) + (Late ? " (late)" : ""));
}

// Laedt ein Asset ueber seinen Pfad, etwa "/Game/.../BP_X.BP_X_C". Dumper-7 erfasst danach auch diese Klasse.
void LoadAssetCommand(const std::wstring& Path)
{
    const std::string Narrow(Path.begin(), Path.end());
    const uintptr_t Object = LoadObjectByPath(Narrow);
    Log(std::string("Laden: ") + (Object ? NameOf(Object) + " (" + NameOf(Read<uintptr_t>(Object + 0x10)) + ")" : "fehlgeschlagen") + " " + Narrow);
}
// Tutorial-Hinweise pausieren die ganze Welt ueber den PauserPlayerState, auch beim Client. Ein Hinweis, der beim
// Kartenwechsel offen bleibt, haelt jede weitere Runde an: Niemand steigt am Bus aus, die Uhr steht bei 00:00.
// Das Spiel hat dafuer einen eigenen Schalter, der Tick setzt ihn in jeder Instanz, sobald das Subsystem existiert.
namespace
{
constexpr uintptr_t kTutorialSkipFlag = 0xB8; // UTutorialStateSubsystem::SkipTutorialFlag
uintptr_t TutorialSubsystem = 0;
int32_t TutorialSubsystemIndex = -1;

uintptr_t FindTutorialSubsystem()
{
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    // Das Subsystem lebt mit der GameInstance. Solange der Platz im Objekt-Array dasselbe Objekt haelt, gilt der Treffer.
    if (TutorialSubsystem && TutorialSubsystemIndex >= 0)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (TutorialSubsystemIndex >> 16));
        if (Chunk && Read<uintptr_t>(Chunk + (TutorialSubsystemIndex & 0xFFFF) * 0x18 + 8) == TutorialSubsystem)
            return TutorialSubsystem;
    }
    TutorialSubsystem = 0;
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x10) != 0)
            continue;
        for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class; Class = Read<uintptr_t>(Class + 0x40))
            if (NameOf(Class) == "TutorialStateSubsystem")
            {
                TutorialSubsystem = Object;
                TutorialSubsystemIndex = i;
                return Object;
            }
    }
    return 0;
}
}

void SkipTutorials(bool Report)
{
    const uintptr_t Tutorials = FindTutorialSubsystem();
    if (!Tutorials)
    {
        if (Report)
            Log("Tutorial: Subsystem nicht gefunden");
        return;
    }
    // Einmal festhalten, ob der Schalter einen Neustart ueberlebt, also im Spielstand liegt.
    static bool FirstCheck = true;
    if (FirstCheck)
    {
        FirstCheck = false;
        Log(Read<bool>(Tutorials + kTutorialSkipFlag) ? "Tutorial: Schalter beim Start schon gesetzt" : "Tutorial: Schalter beim Start aus");
    }
    if (Read<bool>(Tutorials + kTutorialSkipFlag))
    {
        if (Report)
            Log("Tutorial: Hinweise sind abgeschaltet");
        return;
    }
    static const uintptr_t Function = FindObject("SetForceSkipTutorials", "TutorialStateSubsystem");
    if (!Function)
    {
        Log("Tutorial: SetForceSkipTutorials nicht gefunden");
        return;
    }
    struct { bool bSkip; } Params{true};
    Read<ProcessEventFn*>(Tutorials)[kProcessEventIndex](Tutorials, Function, &Params);
    Log(Read<bool>(Tutorials + kTutorialSkipFlag) ? "Tutorial: Hinweise abgeschaltet" : "Tutorial: Schalter nicht gesetzt");
}
}
