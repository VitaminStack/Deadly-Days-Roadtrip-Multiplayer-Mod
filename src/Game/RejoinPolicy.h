#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

// wiedereintritt-inventar: Der Host hebt das Inventar eines Clients auf, der die Sitzung verlässt, und gibt es ihm beim
// Wiederbeitritt oder Hot Join zurück. Reine Logik ohne Spielzugriff; die Spielseite steht in RewardPatches.cpp (TickRejoin).
//
// Was lokal bleibt: alles beim Client, er bekommt das Inventar über die native Replikation wie jede Vergabe des Hosts.
// Geteilt wird nichts Neues, keine Nachricht. Entscheiden tut allein der Host. Schlüssel ist die Steam-Identität des Peers am
// Kontrollkanal, nicht der Zeiger auf PlayerState oder Inventar: Der Host legt bei jedem Beitritt einen neuen PlayerState an.
namespace Game::Rejoin
{
using Tile = std::pair<int32_t, int32_t>;

// Ein Item, wie der Host es zuletzt im Inventar des Clients gesehen hat. Data und Rarity sind Objektnamen (UInventoryItemData, URarity).
struct SavedItem
{
    std::string Data, Rarity;
    bool Broken = false, InBackpack = false, Trash = false;
    int32_t Turns = 0;
    Tile Pivot{};
    std::vector<Tile> Tiles; // belegte Rucksackfelder, sortiert, nur im Rucksack
    bool operator==(const SavedItem&) const = default;
};

struct Snapshot
{
    std::vector<SavedItem> Items; // MyItems in replizierter Reihenfolge, ohne Belohnungen einer offenen Auswahl
    std::vector<Tile> Slots;      // Rucksackfelder, sortiert
    bool operator==(const Snapshot&) const = default;
};

// Ein neuer PlayerState legt sein Startinventar nicht auf einmal an (Charakterwahl, Startausrüstung). Erst wenn das Inventar so
// lange da ist und sich so lange nicht geändert hat, ersetzt der Host es.
inline constexpr uint64_t SettleMs = 10000, StableMs = 3000;

struct Restore
{
    uint64_t Inventory = 0; // Kennung des neuen Inventars (Platz und Serial im Objekt-Array)
    Snapshot Saved;
};

// Je Steam-Identität ein Eintrag. Ablauf je Takt beim Host:
//   verbunden, gleiches Inventar   -> Bild nachführen
//   Peer weg                        -> Eintrag als verlassen merken, Bild eingefroren
//   wieder da, neues Inventar       -> nach SettleMs/StableMs einmal Restore, danach Bild wieder nachführen
//   wieder da, altes Inventar       -> kurzer Ausfall des Kontrollkanals, kein Restore
//   neues Inventar ohne Trennung    -> Reise oder neuer Tag: das Spiel trägt das Inventar selbst hinüber, kein Restore
// So bucht nichts doppelt: Jedes neue Inventar bekommt höchstens einen Restore, und nur nach einer beobachteten Trennung.
// Mehrere Clients haben je ein Konto und damit je einen eigenen Eintrag.
class Ledger
{
public:
    // Je Takt vor Tick: Steam-Identitäten aller verbundenen Clients. Wer fehlt, gilt als getrennt.
    void Connected(std::span<const uint64_t> Keys)
    {
        for (auto& [Key, R] : Records)
            if (std::find(Keys.begin(), Keys.end(), Key) == Keys.end())
                R.Left = true;
    }

    // Je verbundenem Client. Key: seine Steam-Identität, 0 ohne. Inventory: Kennung seines Inventars beim Host, 0 ohne. Now: sein
    // Bild, nullopt wenn nicht lesbar. Busy: gerade keine Ersetzung (offene Airdrop-Auswahl, Edit des Clients). Gibt höchstens einmal
    // je neuem Inventar einen Restore zurück; danach muss Done für denselben Key folgen.
    std::optional<Restore> Tick(uint64_t Key, uint64_t Inventory, const std::optional<Snapshot>& Now, bool Busy, uint64_t NowMs)
    {
        if (!Key || !Inventory || !Now)
            return std::nullopt;
        Record& R = Records[Key];
        if (R.Restoring)
            return std::nullopt;
        if (!R.Inventory)
        {
            Adopt(R, Inventory, *Now);
            return std::nullopt;
        }
        if (Inventory == R.Inventory)
        {
            // Gleiches Inventar: weiter nachführen. Ein leeres Bild (Inventar wird gerade abgebaut) überschreibt kein volles.
            R.Left = false;
            if (!R.Frozen && (!Now->Items.empty() || R.Saved.Items.empty()))
                R.Saved = *Now;
            return std::nullopt;
        }
        if (!R.Left)
        {
            // Neues Inventar ohne Trennung: Reise oder neuer Tag, das Spiel hat es selbst befüllt.
            Adopt(R, Inventory, *Now);
            return std::nullopt;
        }
        if (Inventory == R.Failed)
            return std::nullopt;
        if (Inventory != R.Candidate)
        {
            R.Candidate = Inventory;
            R.FirstSeen = R.LastChange = NowMs;
            R.Seen = *Now;
        }
        else if (*Now != R.Seen)
        {
            R.Seen = *Now;
            R.LastChange = NowMs;
        }
        if (Busy || NowMs - R.FirstSeen < SettleMs || NowMs - R.LastChange < StableMs)
            return std::nullopt;
        R.Restoring = true;
        return Restore{Inventory, R.Saved};
    }

    // Ergebnis des letzten Restore. Gelungen: das neue Inventar gilt, das Bild folgt ihm wieder. Fehlgeschlagen: das Bild bleibt
    // eingefroren, dieses Inventar bekommt keinen zweiten Versuch (nichts doppelt), ein späterer Wiederbeitritt versucht es erneut.
    void Done(uint64_t Key, bool Ok)
    {
        const auto It = Records.find(Key);
        if (It == Records.end() || !It->second.Restoring)
            return;
        Record& R = It->second;
        R.Restoring = false;
        if (Ok)
        {
            R.Inventory = R.Candidate;
            R.Left = R.Frozen = false;
            R.Failed = 0;
        }
        else
        {
            R.Failed = R.Candidate;
            R.Frozen = true;
        }
        R.Candidate = 0;
    }

    // Run vorbei oder keine Sitzung als Host: Nichts mehr zurückgeben, ein späterer Beitritt beginnt mit dem Startinventar.
    bool Forget()
    {
        const bool Had = !Records.empty();
        Records.clear();
        return Had;
    }

    // Für Log und Test: Gemerktes Bild zu Key, nullopt ohne.
    std::optional<Snapshot> Saved(uint64_t Key) const
    {
        const auto It = Records.find(Key);
        return It == Records.end() || !It->second.Inventory ? std::nullopt : std::optional<Snapshot>(It->second.Saved);
    }
    bool HasLeft(uint64_t Key) const
    {
        const auto It = Records.find(Key);
        return It != Records.end() && It->second.Left;
    }

private:
    struct Record
    {
        uint64_t Inventory = 0; // Inventar, dem das Bild folgt
        Snapshot Saved;
        bool Left = false;      // Trennung beobachtet, Bild eingefroren bis zum Restore
        bool Frozen = false;    // nach fehlgeschlagenem Restore
        bool Restoring = false;
        uint64_t Candidate = 0, Failed = 0;
        uint64_t FirstSeen = 0, LastChange = 0;
        Snapshot Seen; // letztes Bild des Kandidaten
    };

    static void Adopt(Record& R, uint64_t Inventory, const Snapshot& Now)
    {
        R.Inventory = Inventory;
        R.Saved = Now;
        R.Left = R.Frozen = false;
        R.Candidate = R.Failed = 0;
    }

    std::map<uint64_t, Record> Records;
};

// Rucksackfelder für den Restore: anzulegen sind die gemerkten, die fehlen; abzubauen die vorhandenen, die das Bild nicht kennt.
inline std::pair<std::vector<Tile>, std::vector<Tile>> SlotChanges(const std::vector<Tile>& Have, const std::vector<Tile>& Want)
{
    std::vector<Tile> Create, Destroy;
    for (const Tile& Next : Want)
        if (std::find(Have.begin(), Have.end(), Next) == Have.end())
            Create.push_back(Next);
    for (const Tile& Next : Have)
        if (std::find(Want.begin(), Want.end(), Next) == Want.end())
            Destroy.push_back(Next);
    return {Create, Destroy};
}
}
