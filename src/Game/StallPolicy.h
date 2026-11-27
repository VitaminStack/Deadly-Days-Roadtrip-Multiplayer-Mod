#pragma once

#include <cstdint>

// client-haenger-nach-niederlage: Wächter für den Game Thread. Läuft der Takt der Mod (WM_TIMER am Spielfenster) eine Weile
// nicht, steht der Game Thread oder hängt in einer Schleife. Dann nimmt ein eigener Thread ein paar Stapelproben, damit das
// Log eines Live-Tests den Ort zeigt. Reine Logik, ohne Windows, getestet in tests/GameTests/StallPolicyTests.cpp.
namespace Game::Stall
{
struct Settings
{
    // Ab so langer Pause gilt der Takt als stehend. Ladebildschirme und Reisen liegen gemessen deutlich darunter.
    uint64_t AfterMs = 8000;
    // Abstand der Proben im selben Hänger: Bleibt der Ort gleich, ist es eine Schleife, wandert er, ein langer Abschnitt.
    uint64_t EveryMs = 4000;
    // Höchstens so viele Proben je Hänger, danach nur noch die Meldung beim Wiederanlaufen.
    int MaxSamples = 3;
};

struct State
{
    bool Stalled = false;
    int Samples = 0;
    uint64_t LastSampleMs = 0;
    // Letzter Takt beim Beginn des Hängers; ein neuerer heißt: läuft wieder.
    uint64_t StallTickMs = 0;
};

enum class Action
{
    None,
    Sample,
    Recovered
};

// NowMs und LastTickMs aus derselben Uhr (GetTickCount64). LastTickMs 0: noch nie getaktet, nichts zu bewachen.
inline Action Next(State& Value, uint64_t NowMs, uint64_t LastTickMs, const Settings& Limits = {})
{
    if (!LastTickMs)
    {
        Value = {};
        return Action::None;
    }
    if (Value.Stalled)
    {
        if (LastTickMs != Value.StallTickMs)
        {
            Value = {};
            return Action::Recovered;
        }
        if (Value.Samples >= Limits.MaxSamples || NowMs - Value.LastSampleMs < Limits.EveryMs)
            return Action::None;
        ++Value.Samples;
        Value.LastSampleMs = NowMs;
        return Action::Sample;
    }
    if (NowMs < LastTickMs || NowMs - LastTickMs < Limits.AfterMs)
        return Action::None;
    Value.Stalled = true;
    Value.StallTickMs = LastTickMs;
    Value.Samples = Limits.MaxSamples > 0 ? 1 : 0;
    Value.LastSampleMs = NowMs;
    return Limits.MaxSamples > 0 ? Action::Sample : Action::None;
}
}
