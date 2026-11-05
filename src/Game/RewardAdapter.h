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
// Sitzungsbild für den Client. Choices sind lesbare Namen in der Reihenfolge der nativen Choices.
struct Offer
{
    uint64_t Session = 0;  // Zähler der Mod je Sitzung, 0 = keine Sitzung
    uint64_t Revision = 0; // Prüfsumme der Choice-Zeiger, ändert sich bei Reroll oder Claim
    int32_t Level = 0;
    std::vector<std::string> Choices;
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
}
