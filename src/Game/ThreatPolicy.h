#pragma once

#include <cstdint>

// opt-bedrohung-sound: Regeln für die Bedrohungsstufe beim Client, ohne Spielzugriff und damit testbar.
// Befund: docs/archive/GAME_ANALYSIS.md#bedrohungsstufe-beim-client-opt-bedrohung-sound
namespace Game
{
inline constexpr int32_t ThreatLevelMax = 100000;
// Der Host wiederholt den unveränderten Stand in diesem Abstand, damit ein verlorenes Paket, ein später Beitritt oder eine
// Stoppuhr, die beim ersten Mal noch fehlte, nachgeholt werden.
inline constexpr uint64_t ThreatRepeatMs = 2000;

constexpr bool ThreatLevelValid(int32_t Level)
{
    return Level >= 0 && Level <= ThreatLevelMax;
}

// Host: Stand senden, wenn er neu ist oder die Wiederholung fällig wird.
constexpr bool ThreatSendDue(bool SentBefore, int32_t LastLevel, int32_t Level, uint64_t LastSentMs, uint64_t NowMs)
{
    return !SentBefore || LastLevel != Level || NowMs < LastSentMs || NowMs - LastSentMs >= ThreatRepeatMs;
}

struct ThreatShown
{
    bool Known = false;
    int32_t Level = 0;
};

// Aufruf des nativen Ereignisses der Stoppuhr (Old, New), wie es beim Host die Komponente ThreatLevel auslöst.
struct ThreatEvent
{
    bool Call = false;
    int32_t Old = 0, New = 0;
    constexpr bool operator==(const ThreatEvent&) const = default;
};

// Client: Was die Stoppuhr für einen angekommenen Stand tun soll. Der erste Stand einer Welt setzt nur die Zahl (Old gleich
// New: kein Anstieg, also kein Sound nach Beitritt oder Reise), jede spätere Änderung läuft mit altem und neuem Wert durch
// das Ereignis, das bei einem Anstieg den Sound spielt. Ein unveränderter Stand tut nichts.
constexpr ThreatEvent ThreatLevelChange(const ThreatShown& Shown, int32_t Level)
{
    if (!ThreatLevelValid(Level))
        return {};
    if (!Shown.Known)
        return {true, Level, Level};
    if (Shown.Level == Level)
        return {};
    return {true, Shown.Level, Level};
}

// opt-routenkarte: Die Zahl unter der Stoppuhr setzt die Haupt-UI (WP_UI_C) über ihr Ereignis am selben Delegate. Sie bekommt jeden
// Stand, den sie noch nicht zeigt, auch einen unveränderten: Nach Reise oder Beitritt ist sie neu, der erste Stand ging womöglich
// an die UI der alten Welt. Current: die gemerkte UI lebt und ist im Viewport. Shown: dort gezeigte Stufe, -1 ohne. Search: eine
// neue Haupt-UI suchen, die dann (Level, Level) bekommt.
struct UiThreatUpdate
{
    bool Call = false;
    bool Search = false;
    int32_t Old = 0, New = 0;
    constexpr bool operator==(const UiThreatUpdate&) const = default;
};

constexpr UiThreatUpdate UiThreatLevelChange(bool Current, int32_t Shown, int32_t Level)
{
    if (!ThreatLevelValid(Level) || (Current && Shown == Level))
        return {};
    if (!Current || Shown < 0)
        return {true, !Current, Level, Level};
    return {true, false, Shown, Level};
}
}
