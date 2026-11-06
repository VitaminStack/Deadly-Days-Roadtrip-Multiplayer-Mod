#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Game/InventoryEditPlan.h"

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

// Host: Edit des Clients übernehmen (inventar-lokal): gewählte Belohnung anlegen, geänderte Items und neue Felder mit den
// nativen Funktionen setzen. Falsch, wenn er nicht passt; dann bleibt das Inventar des Clients unverändert.
bool ApplyClientEdit(const Game::InventoryEdit::Edit& Value);

// Host: Aufräumen je Takt: zurückgehaltenes Bestätigen nachholen, Koop-Modus nach der Sitzung zurücksetzen, die Wahl
// des Clients in sein Inventar legen.
void Tick();

// Client: Choices im nativen Reward-Panel zeigen (eigene, nur lokale Item-Instanzen über SetRewardItems). Ohne
// Wirkung, wenn dieselbe Sitzung und Revision schon angezeigt wird oder schon gewählt wurde.
bool ShowForClient(uint64_t Session, uint64_t Revision, int32_t Level, const std::vector<ChoiceItem>& Choices);

// Client: Anzeige schließen, lokale Instanzen aus dem Inventar nehmen.
void CloseForClient();

// Client: Edit nach dem Bestätigen, bis der Host antwortet (zum wiederholten Senden).
std::optional<Game::InventoryEdit::Edit> PendingClientEdit();

// Client: Antwort des Hosts. Bei Ablehnung stellt der Client seinen Ausgangsstand wieder her.
void ResolveClientEdit(uint64_t Id, bool Accepted);

// Client: Verbindung weg, offenen Edit verwerfen und Ausgangsstand wiederherstellen.
void AbortClientEdit();
}
