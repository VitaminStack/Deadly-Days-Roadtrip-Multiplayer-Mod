#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Airdrop-Auswahl für beide Spieler (airdrop-auswahl), nur beim Host. Die native Koop-Reward-Sitzung des
// UCoopInventoryCoordinatorComponent wartet auf einen Claim je Spieler und hält so lange die Pause. Die Mod setzt den
// Koop-Modus nur für die Dauer dieser Sitzung, der Client wählt über den Kontrollkanal, der Host ruft ClaimReward(1, …).
// Alles nur auf dem Game Thread.
namespace Game::Rewards
{
// Eine Choice: lesbarer Name, Objektnamen von UInventoryItemData und URarity, beschädigt.
struct ChoiceItem
{
    std::string Name, Data, Rarity;
    bool Broken = false;
};

// Sitzungsbild für den Client, Choices in der Reihenfolge der nativen Choices.
struct Offer
{
    uint64_t Session = 0;  // Zähler der Mod je Sitzung, 0 = keine Sitzung
    uint64_t Revision = 0; // Prüfsumme der Choice-Zeiger, ändert sich bei Reroll oder Claim
    int32_t Level = 0;
    std::vector<ChoiceItem> Choices;
    bool ClientClaimed = false;
    bool HostClaimed = false;
    // Der Host hat bestätigt, die Mod hält sein Bestätigen bis zur Wahl des Clients zurück.
    bool HostWaiting = false;
};

// Host: Ein Client mit der Mod ist über den Kontrollkanal verbunden. Nur dann setzt die Mod beim Öffnen einer Kiste
// den Koop-Modus. Sonst bleibt alles wie ohne Mod.
void SetRemoteReady(bool Ready);

// Host: Aktuelle Sitzung, nullopt ohne aktive Sitzung der Mod.
std::optional<Offer> ReadOffer();

// Host: Wahl des Clients eintragen. Falsch, wenn Sitzung, Revision oder Index nicht mehr passen.
bool ClaimForClient(uint64_t Session, uint64_t Revision, uint32_t Index);

// Host: Aufräumen je Takt: zurückgehaltenes Bestätigen nachholen, Koop-Modus nach der Sitzung zurücksetzen, die Wahl
// des Clients in sein Inventar legen.
void Tick();

// Client: Choices im nativen Reward-Panel zeigen (eigene, nur lokale Item-Instanzen über SetRewardItems). Ohne
// Wirkung, wenn dieselbe Sitzung und Revision schon angezeigt wird oder schon gewählt wurde.
bool ShowForClient(uint64_t Session, uint64_t Revision, int32_t Level, const std::vector<ChoiceItem>& Choices);

// Client: Anzeige schließen, lokale Instanzen aus dem Inventar nehmen.
void CloseForClient();

// Client: Index der im Panel bestätigten Choice, einmalig nach dem Bestätigen.
std::optional<uint32_t> TakeClientPick();

// Ablage der gewählten Choice im Rucksack: Drehung, Pivot-Feld und belegte Felder (UBackpackSlot.Coordinates).
struct Placement
{
    int32_t Turns = 0, PivotX = 0, PivotY = 0;
    std::vector<std::pair<int32_t, int32_t>> Slots;
};

// Client: Ablage der zuletzt gewählten Choice, einmalig, sobald das Panel sie in den Rucksack gelegt hat.
std::optional<Placement> TakeClientPlacement();

// Host: Wahl des Clients aus der Sitzung wie beim Client in seinen Rucksack legen. Ohne Ablage bleibt sie im Lager.
bool PlaceForClient(uint64_t Session, const Placement& Value);
}
