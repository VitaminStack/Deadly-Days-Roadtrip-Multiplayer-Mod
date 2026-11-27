#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Game/WorldAdapter.h"

// opt-powerups-client: Regeln für Power-ups (Dosenessen, Staubsauger, Energy Drink, Level-Up, Dynamit) im Coop, ohne Spielzugriff
// und damit testbar. Das Spiel kennt nur lokalen Coop: Das Power-up-Inventar (UActivePowerupInventory am PlayerState) und seine
// Slots replizieren nicht, und UPowerupSlot::TryActivateSlot wendet Effekt oder Fähigkeit ohne RPC an. Beim Client fehlt dafür die
// Autorität, beim Host kam für die Client-Figur nie ein Auslösen an. Lokal zuerst, der Host bucht: Er füllt und löst die Slots
// aller Figuren aus. Der Client bucht keine Power-ups, zeigt den Slot-Stand des Hosts (0xE00) und meldet das Auslösen eines Slots
// (0xE01). Befund: docs/faelle/opt-powerups-client.md
namespace Game::PowerUp
{
// EGameplayAbilityNetExecutionPolicy der Engine.
constexpr uint8_t LocalPredicted = 0, LocalOnly = 1, ServerInitiated = 2, ServerOnly = 3;

// Host: Die Fähigkeit eines Power-ups (GA_PowerUp_Heal_C, GA_PowerUp_LevelUp_C, alle LocalPredicted) läuft über
// GiveAbilityAndActivateOnce. Für die nicht lokal gesteuerte Figur eines Netz-Clients bricht die Engine dort bei LocalPredicted
// und LocalOnly ab, ohne Aktivierung und ohne RPC (gemessen). Der Host stellt solche Klassen darum vor dem Auslösen auf ServerOnly.
// ServerInitiated, ServerOnly und unbekannte Werte bleiben.
constexpr bool AbilityMovesToHost(uint8_t Policy)
{
    return Policy == LocalPredicted || Policy == LocalOnly;
}

// Ein Power-up aufsammeln (APowerUpLoot::Loot, Sofortwirkung oder Slot) bucht nur, wer die Slots führt: Host und Einzelspieler.
// Beim Client laufen nur Klang und Darstellung der Basis.
constexpr bool LootMayBook(NetRole Role)
{
    return Role != NetRole::Client;
}

inline constexpr size_t MaxSlots = 8;
inline constexpr size_t MaxNameBytes = 64;
inline constexpr int32_t MaxLevel = 0xFFFF;
// Der Host wiederholt den unveränderten Slot-Stand in diesem Abstand: gegen Verlust und für eine neue Welt des Clients.
inline constexpr uint64_t RepeatMs = 2000;
// Ein gemeldetes Auslösen ohne Bestätigung des Hosts verfällt nach dieser Zeit; dann gilt wieder allein der Stand des Hosts.
inline constexpr uint64_t RequestExpireMs = 5000;

// Name einer PowerupDefinition (etwa POD_Vacuum): 1 bis MaxNameBytes Zeichen aus Buchstaben, Ziffern und Unterstrich.
constexpr bool ValidName(std::string_view Name)
{
    if (Name.empty() || Name.size() > MaxNameBytes)
        return false;
    for (const char C : Name)
        if (!((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '_'))
            return false;
    return true;
}

// Ein verbrauchbarer Slot (Power-up aus dem Aufsammeln). Slots aus Items (bConsumable aus) gehören nicht dazu.
struct Slot
{
    std::string Definition;
    int32_t Level = 1;
    bool operator==(const Slot&) const = default;
};

inline bool ValidSlots(const std::vector<Slot>& Slots)
{
    if (Slots.size() > MaxSlots)
        return false;
    for (const Slot& Entry : Slots)
        if (!ValidName(Entry.Definition) || Entry.Level < 0 || Entry.Level > MaxLevel)
            return false;
    return true;
}

// Client: was an den eigenen verbrauchbaren Slots zu ändern ist, damit sie dem Stand des Hosts entsprechen.
struct Plan
{
    std::vector<std::string> Remove; // je Eintrag einen Slot mit dieser Definition leeren
    std::vector<Slot> Add;           // je Eintrag ein Power-up in einen freien Slot legen
    bool Empty() const { return Remove.empty() && Add.empty(); }
};

// Local: Definitionen der eigenen verbrauchbaren Slots beim Client. Host: Stand des Hosts für diese Figur. Pending: beim Client
// schon ausgelöst und gemeldet, vom Host noch nicht bestätigt; je Eintrag zählt ein passender Slot des Hosts als schon verbraucht,
// damit eine ältere Meldung des Hosts das eben ausgelöste Power-up nicht zurücklegt. Reihenfolge ist egal, gezählt wird je Name.
inline Plan Reconcile(const std::vector<std::string>& Local, const std::vector<Slot>& Host, const std::vector<std::string>& Pending)
{
    std::vector<Slot> Wanted = Host;
    for (const std::string& Name : Pending)
        for (size_t i = 0; i < Wanted.size(); ++i)
            if (Wanted[i].Definition == Name)
            {
                Wanted.erase(Wanted.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
    Plan Result;
    std::vector<std::string> Have = Local;
    for (const Slot& Entry : Wanted)
    {
        bool Found = false;
        for (size_t i = 0; i < Have.size() && !Found; ++i)
            if (Have[i] == Entry.Definition)
            {
                Have.erase(Have.begin() + static_cast<std::ptrdiff_t>(i));
                Found = true;
            }
        if (!Found)
            Result.Add.push_back(Entry);
    }
    Result.Remove = Have;
    return Result;
}

// Client: lokal ausgelöste Slots, gemeldet mit fortlaufender Nummer ab 1. Der Host bestätigt die höchste verarbeitete Nummer im
// Slot-Stand; alles bis dahin ist erledigt.
class Requests
{
public:
    static constexpr size_t Capacity = 32;

    // Nummer der neuen Meldung, 0 wenn kein Platz oder der Name ungültig ist.
    uint32_t Add(std::string_view Name, uint64_t NowMs)
    {
        if (!ValidName(Name) || Entries.size() >= Capacity)
            return 0;
        Entries.push_back({++Last, std::string(Name), NowMs});
        return Last;
    }
    void Acknowledge(uint32_t Acked)
    {
        std::erase_if(Entries, [Acked](const Entry& Value) { return Value.Sequence <= Acked; });
    }
    void Expire(uint64_t NowMs)
    {
        std::erase_if(Entries, [NowMs](const Entry& Value) { return NowMs - Value.SentMs >= RequestExpireMs; });
    }
    std::vector<std::string> Pending() const
    {
        std::vector<std::string> Names;
        for (const Entry& Value : Entries)
            Names.push_back(Value.Name);
        return Names;
    }
    // Neue Welt oder neue Verbindung: Offenes verwerfen. Die Nummer läuft weiter, damit eine späte Bestätigung nichts Neues löscht.
    void Clear() { Entries.clear(); }
    size_t Size() const { return Entries.size(); }

private:
    struct Entry
    {
        uint32_t Sequence;
        std::string Name;
        uint64_t SentMs;
    };
    std::vector<Entry> Entries;
    uint32_t Last = 0;
};

// Host, je Client: was zuletzt gesendet wurde.
struct Sent
{
    bool Any = false;
    std::vector<Slot> Slots;
    uint32_t Acked = 0;
    uint64_t AtMs = 0;
};

// Senden bei Änderung von Slots oder Bestätigung, sonst als Wiederholung alle RepeatMs.
inline bool SendDue(const Sent& Last, const std::vector<Slot>& Slots, uint32_t Acked, uint64_t NowMs)
{
    return !Last.Any || Last.Slots != Slots || Last.Acked != Acked || NowMs - Last.AtMs >= RepeatMs;
}
}
