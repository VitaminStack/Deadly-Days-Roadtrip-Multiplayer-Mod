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
    // Der Client hat für diese Revision Bestätigen gedrückt (bestaetigen-alle).
    bool ClientReady = false;
};

// Bereit-Stand des Clients für eine angezeigte Auswahl (bestaetigen-alle).
struct ReadyState
{
    uint64_t Session = 0, Revision = 0;
    bool Ready = false;
};

// Host: Bereit des Clients übernehmen. Ohne Wirkung, wenn die Sitzung nicht offen ist.
void SetClientReady(uint64_t Session, uint64_t Revision, bool Ready);

// Client: Bereit-Stand der angezeigten Auswahl, nullopt ohne Auswahl oder nach dem Bestätigen.
std::optional<ReadyState> ClientReadyState();

// Client: Haben alle bestätigt (Host wartet und hat das Bereit des Clients), löst die Mod das native Bestätigen aus. Der Edit
// geht danach wie bisher an den Host. Fehlt die Wahl, nimmt sie das Bereit zurück.
void FinishClientIfAllReady(bool AllReady);

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

// Lager am Rundenende (bestaetigen-rundenende). Bestätigen im Lager markiert wie beim Airdrop nur bereit (Knopf gelb); erst wenn
// Host und Client bereit sind, laufen beide nativen Bestätigen und der Host kommt zur Routenkarte. Ids sind
// Game::Map::RoundEnd::Id, 0 heißt kein Rundenende.

// Beide, je Takt aus den Kartenpatches: Host sein eigenes Rundenende in dieser Welt, Client das selbst geöffnete.
void SetRoundEnd(uint64_t Id);

// Host: Stand des Clients aus 0x306.
void SetRoundEndClient(uint64_t Id, bool Ready);

// Stand des Hosts für den Client (0x307).
struct RoundEndState
{
    uint64_t Id = 0;
    bool HostReady = false, ClientReady = false;
};
RoundEndState HostRoundEnd();

// Client: eigenes Rundenende und Bereit für 0x306, nullopt ohne Rundenende.
struct RoundEndOwn
{
    uint64_t Id = 0;
    bool Ready = false;
};
std::optional<RoundEndOwn> ClientRoundEnd();

// Client: neuester Stand des Hosts, nullopt ohne frischen Stand. Löst ein zurückgehaltenes Bestätigen aus, wenn beide
// bereit sind oder der Stand nicht mehr passt.
void SetHostRoundEndStatus(std::optional<RoundEndState> Status);

// werkbank-client: UInventoryItemInstance.ItemRarity (+0x118) repliziert nicht, beim Client steht dort für jedes replizierte
// Item null. Das native Panel prüft beim Ablegen auf ein gleiches Item die Seltenheit und wertet ohne sie nie auf. Der Host
// schickt die Seltenheiten des Clientinventars, der Client setzt nur dieses Feld.
struct RarityEntry
{
    uint16_t Index = 0;    // Platz in MyItems, replizierte Reihenfolge
    uint32_t DataHash = 0; // Game::InventoryEdit::NameHash des ItemData-Objektnamens
    std::string Rarity;    // Objektname der URarity, leer = keine
    bool operator==(const RarityEntry&) const = default;
};

// Host: Seltenheiten der Items im Inventar des Clients, leer ohne gebundenen Client.
std::vector<RarityEntry> ClientRarities();

// Client: Seltenheiten des Hosts auf die replizierten Items des eigenen Inventars legen. Nur Items, deren Daten an diesem
// Platz übereinstimmen; während einer lokalen Bearbeitung nicht.
void ApplyRarities(const std::vector<RarityEntry>& Entries);

// lager-handel-client: Der Schrott (ScrapWalletComponent am GameState der Mission, Kontostand CurrencyAmount) repliziert nicht.
// Nur der Host bucht ihn; der Client zeigt den Stand des Hosts, damit sein natives Panel Reparaturen richtig freigibt.
// Host: eigener Kontostand, nullopt ohne Mission oder Konto.
std::optional<int32_t> HostScrap();
// Client: Kontostand des Hosts übernehmen (natives AddAmount mit dem Unterschied, damit die Anzeige folgt). Nicht während einer
// lokalen Bearbeitung oder solange ein Edit unbeantwortet ist; dann gilt die lokale Vorschau des Panels.
void ApplyScrap(int32_t Amount);
}
