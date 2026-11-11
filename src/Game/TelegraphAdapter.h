#pragma once
#include <cstdint>
#include <string>
#include <vector>

// opt-gegner-indikatoren: Warnanzeigen der Gegnerangriffe (Telegraphs) beim Client. Die Angriffsfähigkeiten der Gegner
// laufen nur beim Host, ihre Niagara-Anzeigen entstehen dort lokal und replizieren nicht. Der Host meldet je Anzeige nur den
// Auslöser, der Client zeigt dasselbe Niagara-System selbst an. Schaden, Ziel und Zeitpunkt entscheidet weiter der Host.
// Befund: docs/GAME_ANALYSIS.md, Abschnitt „Warnanzeigen der Gegner beim Client“. Nur Game Thread.
namespace Game
{
enum class TelegraphKind : uint8_t
{
    Circle = 1,     // UEnemyAttackAbility::SpawnCircleTelegraph: Kreis am Boden (Spucken, Sprünge, Bosse)
    LineStart = 2,  // UTackleAbilityExecution::ActivateLineTelegraph: Strich vor dem Sprint
    LineUpdate = 3, // UTackleAbilityExecution::CalculateLineTelegraphEndPoint: Ziel und Anteil des Strichs
    LineEnd = 4,    // UTackleAbilityExecution::ExecuteTackle oder EndTackle: Strich aus
};

struct EnemyTelegraph
{
    TelegraphKind Kind = TelegraphKind::Circle;
    // NetGUID des Gegners, nur für den Strich. Der Kreis kommt an seinem Weltort, ohne Gegner.
    uint64_t Enemy = 0;
    // Kreis: Mitte. LineUpdate: Ziel des Strichs.
    double X = 0, Y = 0, Z = 0;
    // Kreis: Parameter des Niagara-Systems. LineUpdate: Percentage = Anteil des Strichs.
    float Delay = 0, Lifetime = 0, Size = 0, Percentage = 0;
    // Kreis: Pfad des Niagara-Systems („/Game/…/NS_X.NS_X“) und Namen der Parameter aus der Fähigkeit des Hosts.
    std::string System, SizeName, LifetimeName, DelayName;
    bool operator==(const EnemyTelegraph&) const = default;
};

// Host: seit dem letzten Aufruf entstandene Anzeigen, höchstens 64 je Abruf, ältere verfallen.
std::vector<EnemyTelegraph> TakeEnemyTelegraphs();
// Client: Anzeige des Hosts lokal zeigen. Unbekannte Gegner oder Systeme werden übersprungen.
void ReceiveEnemyTelegraph(const EnemyTelegraph& Value);
}
