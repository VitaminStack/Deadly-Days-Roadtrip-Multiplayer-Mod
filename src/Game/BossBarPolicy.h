#pragma once

#include <cmath>
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
    bool Started = true;        // der Kampf läuft, siehe BossFightStarted
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
        return State.LiveBoss && State.Started ? BossBarStep::Create : BossBarStep::None;
    const bool Readable = State.LiveBoss && State.Health >= 1 && State.MaxHealth > 1;
    if (!Readable)
        return State.Shown ? BossBarStep::Remove : BossBarStep::None;
    return State.SettleTicks < BossBarSettleTicks ? BossBarStep::Settle : BossBarStep::Sync;
}

// Ob der Kampf läuft (opt-boss-phasenleiste). Der Host legt sein Objective erst mit TriggerBossfight an, der Boss ist beim Client
// aber schon vorher repliziert. Vor dem Kampf trägt der Boss das replizierte Tag State.Pause; es fällt nach dem Intro weg, dann zeigt
// auch der Host die Leiste (während der Intro-Kamera hat er kein HUD). Paused 1: Tag da, 0: Tag fehlt, -1: Tags nicht lesbar. Nicht
// lesbar zählt wie gestartet, sonst gäbe es gar keine Leiste. Ein Boss mit Schaden kämpft schon (TriggerBossfightWhenBossDamaged,
// Beitritt mitten im Kampf während einer späteren Pause). Eine bestehende Leiste bleibt, das Tag sperrt nur das Anlegen.
inline bool BossFightStarted(int Paused, int64_t Health, int64_t MaxHealth)
{
    return Paused != 1 || (MaxHealth > 1 && Health >= 1 && Health < MaxHealth);
}

// Jeder SetCurrentProgressCounter sendet OnProgressChanged an die Anzeige, darum nur bei geänderten Werten.
inline bool BossBarNeedsUpdate(int64_t LastHealth, int64_t LastMax, int64_t Health, int64_t MaxHealth)
{
    return Health != LastHealth || MaxHealth != LastMax;
}

// Phasenbezogene Leiste (opt-boss-phasenleiste). Der Host zeigt bei Bossen, deren Spawnpunkt IsObjectivePhaseBased setzt, nur die
// HP der laufenden Phase: Wert = HP minus Untergrenze der Phase. Die Marken sind Anteile am Maximum (PhaseMarkers des Objectives,
// etwa 0,5 oder 0,8 und 0,4). Die Phase folgt allein aus den replizierten HP, damit auch ein Beitritt mitten im Kampf stimmt.
struct BossPhaseView
{
    int Phase = 1;      // 1 = erste Phase
    int64_t Value = 0;  // Wert für das Objective, HP über der Untergrenze der Phase
    int64_t Span = 0;   // HP-Umfang der Phase, der Nenner der Anzeige
};

// Steht der Boss genau auf einer Marke, läuft schon die nächste Phase: Der Host hält die HP beim Phasenwechsel an der Marke fest
// (gemessen: SpikeBoss bleibt nach 50.000 Schaden bei 40.000 von 80.000). Der Wert ist darum bei HP ab 1 nie unter 1; ein Wert
// unter 1 würde das Objective abschließen und den Client abstürzen lassen. Ungültige Marken (nicht fallend, außerhalb von 0 bis 1)
// werden übergangen, ohne Marken ist es eine einzige Phase über die vollen HP.
inline BossPhaseView PhaseOf(int64_t Health, int64_t MaxHealth, const std::vector<float>& Markers)
{
    BossPhaseView View;
    int64_t Upper = MaxHealth;
    for (const float Marker : Markers)
    {
        if (!(Marker > 0.0f && Marker < 1.0f))
            continue;
        const auto Lower = std::llround(static_cast<double>(MaxHealth) * static_cast<double>(Marker));
        if (Lower <= 0 || Lower >= Upper)
            continue;
        if (Health > Lower)
        {
            View.Value = Health - Lower;
            View.Span = Upper - Lower;
            return View;
        }
        Upper = Lower;
        ++View.Phase;
    }
    View.Value = Health;
    View.Span = Upper;
    return View;
}

// Ein Boss, den der Client sieht, mit seinen replizierten Lebenspunkten (opt-endboss-leiste).
struct BossCandidate
{
    uintptr_t Id = 0;
    int64_t Health = 0;
    int64_t MaxHealth = 0;
};

inline constexpr size_t NoBoss = static_cast<size_t>(-1);

// Was eine Anzeige (DDObjectiveWidget) gerade hat: Manager (das Subsystem) und die Unteranzeigen, die OnObjectiveStarted ohne Prüfung
// liest (DescriptionText, ProgressText, ProgressBar; auch der Fortschritt schreibt in ProgressBar). Live: weder unerreichbar noch Müll
// noch im Abbau (Objects::IsLive).
struct ObjectiveWidgetView
{
    bool Manager = false;
    bool Description = false;
    bool ProgressText = false;
    bool ProgressBar = false;
    bool Live = true;
};

// Nur eine lebende, fertig aufgebaute Anzeige bekommt OnObjectiveStarted (boss-leiste-wiederbeitritt). Ohne Manager stürzt die zweite
// Instanz ab. Beim Wiederbeitritt im selben Prozess stand noch eine Anzeige der vorigen Welt im Objekt-Array, die das Spiel schrittweise
// freigibt: einmal ohne Unteranzeigen (Lesen 0x0), einmal mit einem schon freigegebenen CurrentObjective, das OnObjectiveStarted zuerst
// abmeldet (Lesen freigegebenen Speichers). Beides stürzte den Client ab.
inline bool CanNotifyObjectiveWidget(const ObjectiveWidgetView& Widget)
{
    return Widget.Live && Widget.Manager && Widget.Description && Widget.ProgressText && Widget.ProgressBar;
}

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
