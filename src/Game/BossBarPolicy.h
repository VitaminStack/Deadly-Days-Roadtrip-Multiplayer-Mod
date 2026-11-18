#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Entscheidung der Boss-Leiste beim Client, ohne Spielzugriff und damit testbar. Der Client hat kein Boss-Objective, der Host legt es
// über den Spawnpunkt an. Die Leiste entsteht lokal aus dem nativ replizierten Boss, siehe src/Sync/Enemies/README.md.
namespace Game
{
enum class BossBarStep
{
    None,    // nichts tun
    Create,  // Objective anlegen und die Anzeige starten
    Settle,  // angelegt, noch kurz warten, bevor Werte gesetzt werden
    Sync,    // Werte aus den Boss-HP setzen
    Remove   // Boss weg oder tot: Anzeige beenden
};

// Ticks nach dem Anlegen, bevor die ersten Werte gesetzt werden. Früher gesetzte Werte haben im Spike den Client abstürzen lassen.
inline constexpr int BossBarSettleTicks = 3;

struct BossBarState
{
    bool Client = false;        // nur beim Client, der Host hat sein eigenes Objective
    bool LiveBoss = false;      // ein repliziertes, nicht zerstörtes BP_BossEnemy_C
    bool HasObjective = false;  // unser Objective existiert schon
    bool Shown = false;         // die Anzeige zeigt es gerade
    int SettleTicks = 0;        // Ticks seit dem Anlegen
    int64_t Health = 0;         // Boss-HP, 0 wenn nicht lesbar
    int64_t MaxHealth = 0;
};

// Health unter 1 setzt die Leiste nie, das würde das Objective abschließen (HandleObjectiveCompleted), dem Client-Subsystem fehlt dafür
// das aktuelle Objective, und das Spiel stürzt ab. Dort greift stattdessen Remove, sobald der Boss weg ist.
inline BossBarStep DecideBossBar(const BossBarState& State)
{
    if (!State.Client)
        return BossBarStep::None;
    if (!State.HasObjective)
        return State.LiveBoss ? BossBarStep::Create : BossBarStep::None;
    const bool Readable = State.LiveBoss && State.Health >= 1 && State.MaxHealth > 1;
    if (!Readable)
        return State.Shown ? BossBarStep::Remove : BossBarStep::None;
    return State.SettleTicks < BossBarSettleTicks ? BossBarStep::Settle : BossBarStep::Sync;
}

// Jeder SetCurrentProgressCounter sendet OnProgressChanged an die Anzeige, darum nur bei geänderten Werten.
inline bool BossBarNeedsUpdate(int64_t LastHealth, int64_t LastMax, int64_t Health, int64_t MaxHealth)
{
    return Health != LastHealth || MaxHealth != LastMax;
}

// Ein Boss, den der Client sieht, mit seinen replizierten Lebenspunkten (opt-endboss-leiste).
struct BossCandidate
{
    uintptr_t Id = 0;
    int64_t Health = 0;
    int64_t MaxHealth = 0;
};

inline constexpr size_t NoBoss = static_cast<size_t>(-1);

// Welcher Boss die Leiste bekommt. Ein schon verfolgter Boss bleibt es, solange er lebt; sonst NoBoss, damit die alte Leiste erst
// entfernt wird. Ohne Leiste zuerst ein lebender Boss mit Schaden (Kampf läuft), sonst der erste lebende. Auf einer HQ-Karte steht der Endboss schon vor
// dem Kampf da; ein zweiter Boss (etwa per !bossfight) bekäme sonst Werte in die Leiste des ersten.
inline size_t PickBoss(const std::vector<BossCandidate>& Bosses, uintptr_t Tracked)
{
    if (Tracked)
    {
        for (size_t i = 0; i < Bosses.size(); ++i)
            if (Bosses[i].Id == Tracked)
                return i;
        return NoBoss;
    }
    // Nur lebende Bosse mit lesbaren HP: Der LazyBoss bleibt bei 0 HP als Actor stehen, sonst entstünde jede Sekunde eine neue Leiste.
    size_t First = NoBoss;
    for (size_t i = 0; i < Bosses.size(); ++i)
    {
        if (Bosses[i].MaxHealth <= 1 || Bosses[i].Health < 1)
            continue;
        if (Bosses[i].Health < Bosses[i].MaxHealth)
            return i;
        if (First == NoBoss)
            First = i;
    }
    return First;
}

}
