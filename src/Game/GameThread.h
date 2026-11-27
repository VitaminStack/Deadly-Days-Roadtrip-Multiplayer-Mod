#pragma once

#include <chrono>
#include <functional>
#include <string>

#include "Game/StallPolicy.h"

// Takt auf dem Game Thread. Die Mod hängt sich an die Fensterprozedur des Spielfensters (`UnrealWindow`) und setzt
// dort einen Timer. Unreal pumpt die Fensternachrichten auf dem Game Thread zwischen zwei Frames, deshalb dürfen
// Ticks und die Aufgaben aus Core::GameThread() dort Spielobjekte anfassen.
namespace Game::GameThread
{
using Tick = std::function<void()>;

// Sucht im Hintergrund das Spielfenster des eigenen Prozesses und hängt den Takt an. Kehrt sofort zurück.
void Start(std::chrono::milliseconds Interval = std::chrono::milliseconds(16));

// Hängt sich vom Fenster ab und beendet die Suche. Danach läuft kein Tick mehr.
void Stop();

// Wahr, sobald der Takt am Fenster hängt.
bool IsAttached();

// client-haenger-nach-niederlage: Wächter. Läuft der Takt länger als Settings::AfterMs nicht, schreibt ein eigener Thread
// Stapelproben des Game Threads ins Log ("Spielthread: …"). Settings für Tests; StallSamples und LastStallReport zählen und
// zeigen die Proben seit dem Laden.
void SetStallSettings(const Stall::Settings& Value);
int StallSamples();
std::string LastStallReport();

// Registriert einen Tick, der nach Core::GameThread().Drain() in jedem Takt läuft. Vor Start aufrufen.
// Der Name erscheint im Log, wenn der Tick den Frame spürbar aufhält.
void AddTick(Tick Callback, const char* Name = "Tick");

// Misst einen Abschnitt auf dem Game Thread. Dauert er länger als ein paar Millisekunden, steht er mit Namen und Dauer
// im Log, je Name höchstens alle paar Sekunden. Name muss ein String-Literal sein. Der innerste offene Abschnitt steht auch in
// den Proben des Wächters ("im Abschnitt …"), so zeigt das Log den Ort in der Mod auch ohne Symbole.
class SlowScope
{
public:
    explicit SlowScope(const char* Name);
    ~SlowScope();
    SlowScope(const SlowScope&) = delete;
    SlowScope& operator=(const SlowScope&) = delete;

private:
    const char* Name;
    const char* Outer;
    long long Start;
};
}
