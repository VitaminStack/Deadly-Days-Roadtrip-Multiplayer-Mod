#pragma once

#include <cstdint>

// opt-zonenziel-anzeige: Regeln für das Zonenziel beim Client (etwa „Töte die Titanen“), ohne Spielzugriff und damit testbar.
// Das Ziel ist ein lokales Objekt der GameInstance des Hosts (DDObjectiveSubsystem), es repliziert nicht. Der Host meldet nur den
// Stand, der Client zeigt ihn mit einem eigenen Objekt derselben Klasse in der nativen Leiste. Befund: docs/faelle/opt-zonenziel-anzeige.md
namespace Game
{
inline constexpr int64_t ZoneGoalMax = 1000000;
// Der Host wiederholt den unveränderten Stand in diesem Abstand: gegen Paketverlust, für späte Beitritte und für einen Client,
// der die Welt erst nach dem Host betritt.
inline constexpr uint64_t ZoneGoalRepeatMs = 2000;
// So oft wiederholt der Host „kein Ziel“, danach schweigt er, bis sich etwas ändert oder ein Mitspieler dazukommt.
inline constexpr int ZoneGoalIdleRepeats = 3;
// Ticks nach dem Anlegen, bevor der Client den ersten geänderten Wert setzt (wie bei der Boss-Leiste).
inline constexpr int ZoneGoalSettleTicks = 3;

// Stand des Ziels beim Host. Ohne Ziel sind beide Werte 0.
struct ZoneGoalView
{
    bool Active = false;
    int64_t Current = 0, Target = 0;
    constexpr bool operator==(const ZoneGoalView&) const = default;
};

constexpr bool ZoneGoalValid(const ZoneGoalView& View)
{
    if (!View.Active)
        return View.Current == 0 && View.Target == 0;
    return View.Target >= 1 && View.Target <= ZoneGoalMax && View.Current >= 0 && View.Current <= ZoneGoalMax;
}

// Host: Stand aus dem laufenden Ziel des Subsystems. Boss-Ziele zählen nicht, ihre Leiste baut der Client aus den replizierten
// Boss-HP (BossBarPolicy.h). Ein Ziel ohne brauchbares Soll gilt als keines; Werte außerhalb des Bereichs werden begrenzt.
constexpr ZoneGoalView ZoneGoalFromHost(bool HasObjective, bool IsBossObjective, int64_t Current, int64_t Target)
{
    if (!HasObjective || IsBossObjective || Target < 1 || Target > ZoneGoalMax)
        return {};
    return {true, Current < 0 ? 0 : Current > ZoneGoalMax ? ZoneGoalMax : Current, Target};
}

// Host: was zuletzt angenommen gesendet wurde.
struct ZoneGoalSent
{
    bool Sent = false;
    ZoneGoalView Last;
    uint64_t LastMs = 0;
    int IdleRepeats = 0; // Wiederholungen von „kein Ziel“ seit der letzten Änderung
};

constexpr bool ZoneGoalSendDue(const ZoneGoalSent& State, const ZoneGoalView& Now, uint64_t NowMs)
{
    if (!State.Sent || !(State.Last == Now) || NowMs < State.LastMs)
        return true;
    if (NowMs - State.LastMs < ZoneGoalRepeatMs)
        return false;
    return Now.Active || State.IdleRepeats < ZoneGoalIdleRepeats;
}

// Nach einem Sendeversuch. Ohne angenommenen Versand (kein Mitspieler, Warteschlange voll) gilt der Stand als nicht gesendet.
constexpr ZoneGoalSent ZoneGoalAfterSend(const ZoneGoalSent& State, const ZoneGoalView& Now, uint64_t NowMs, bool Accepted)
{
    if (!Accepted)
        return {};
    const bool Repeat = State.Sent && State.Last == Now;
    return {true, Now, NowMs, Repeat && !Now.Active ? State.IdleRepeats + 1 : 0};
}

enum class ZoneGoalStep
{
    None,
    Create, // Ziel lokal anlegen und der Anzeige melden
    Settle, // angelegt, noch kurz warten
    Update, // neuen Wert des Hosts setzen
    Remove  // Leiste beenden
};

struct ZoneGoalClient
{
    bool Client = false;      // nur beim Client, der Host hat sein eigenes Ziel
    bool HostKnown = false;   // ein Stand des Hosts für die aktuelle Welt liegt vor
    ZoneGoalView Host;
    bool Shown = false;       // unser Ziel steht in der Anzeige
    int64_t ShownCurrent = 0; // zuletzt gesetzte Werte
    int64_t ShownTarget = 0;
    int SettleTicks = 0;      // Ticks seit dem Anlegen
    bool HasClass = false;    // die Zone der eigenen Karte nennt die Klasse des Ziels
    bool WidgetReady = false; // eine Anzeige mit Subsystem ist da
    bool WidgetBusy = false;  // die Anzeige zeigt gerade ein anderes Ziel, etwa die Boss-Leiste
};

// Der Client zeigt nur einen laufenden Stand unter dem Soll. Das Soll selbst setzt er nie: Damit schlösse sein Objekt das Ziel ab
// (HandleObjectiveCompleted), und Abschluss und Belohnung bucht nur der Host. Der Host meldet danach „kein Ziel“, die Leiste
// verschwindet wie bei ihm.
constexpr bool ZoneGoalWanted(const ZoneGoalClient& State)
{
    return State.HostKnown && ZoneGoalValid(State.Host) && State.Host.Active && State.Host.Current < State.Host.Target;
}

constexpr ZoneGoalStep DecideZoneGoal(const ZoneGoalClient& State)
{
    if (!State.Client)
        return ZoneGoalStep::None;
    const bool Wanted = ZoneGoalWanted(State);
    if (!State.Shown)
        return Wanted && State.HasClass && State.WidgetReady && !State.WidgetBusy ? ZoneGoalStep::Create : ZoneGoalStep::None;
    // Anderes Soll oder ein gesunkener Wert: ein neues Ziel des Hosts. Die Leiste wird entfernt und im nächsten Tick neu angelegt,
    // ein Wert wird nie zurückgesetzt.
    if (!Wanted || State.Host.Target != State.ShownTarget || State.Host.Current < State.ShownCurrent)
        return ZoneGoalStep::Remove;
    if (State.Host.Current == State.ShownCurrent)
        return ZoneGoalStep::None;
    return State.SettleTicks < ZoneGoalSettleTicks ? ZoneGoalStep::Settle : ZoneGoalStep::Update;
}
}
