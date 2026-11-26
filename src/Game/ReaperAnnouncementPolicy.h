#pragma once

#include <array>
#include <cstdint>
#include <string_view>

// opt-sensenmann-ansage-client: Regeln für die Ansage des Sensenmanns beim Client, ohne Spielzugriff und damit testbar.
// Den Sensenmann erzeugt nur der Host (ReaperMaySpawn in PatchPolicy.h). Seine Spawn-Funktion (AReaperSpawner, 0x58C9E20)
// meldet danach OnReaperSpawned, spielt den Ton, startet die Sprechblase und setzt WasReaperSpawned. Beim Client holt
// ReaperAnnouncementAdapter.cpp diesen Teil lokal nach, sobald der Sensenmann des Hosts repliziert ankommt.
namespace Game
{
// Client: Sensenmann des Hosts ist da, der eigene Spawner hat ihn noch nicht gemeldet. WasReaperSpawned dient als Sperre, die mit
// jeder neuen Karte (neuer Spawner) von selbst zurückfällt. Host und Einzelspieler melden ihn nativ selbst.
constexpr bool ReaperAnnouncementDue(bool IsNetClient, bool HasSpawner, bool WasReaperSpawned, bool ReaperPresent)
{
    return IsNetClient && HasSpawner && !WasReaperSpawned && ReaperPresent;
}

// Empfänger von OnReaperSpawned, die beim Client nicht laufen dürfen: Die Ausweitung der Spawn-Biome bestimmt, wo Gegner
// erscheinen, das bucht nur der Host. Anzeige wie die Stoppuhr (WBP_Stopwatch_C.OnReaperSpawned) läuft lokal.
inline constexpr std::array<std::string_view, 1> HostOnlyReaperReceivers = {"EnemyBiomeSpawnExpansionComponent"};

// ClassChain: Klasse des Empfängers und ihre Oberklassen, nächste zuerst.
template <typename Chain> constexpr bool ReaperReceiverRunsOnClient(const Chain& ClassChain)
{
    for (std::string_view Class : ClassChain)
        for (std::string_view HostOnly : HostOnlyReaperReceivers)
            if (Class == HostOnly)
                return false;
    return true;
}

// FAdditionalSpeechData wie in der Spawn-Funktion des Hosts (0x58CA39B bis 0x58CA3C0, Werte aus .rdata, Build 26786272): Blase an
// fester Bildschirmstelle, ohne Tween. Der Sprecher kommt beim Host von der ersten Spielerfigur, beim Client von der eigenen.
struct ReaperSpeechLayout
{
    bool UseAbsolutePosition = true;
    double ScreenLocation[2] = {-1.0, -1.0};
    double ScreenPercentages[2] = {0.5, 0.75};
    bool TweenBubbleToNewPosition = false;
    float TweenTime = 0.5f;
    uint8_t TweenType = 8;
    bool ShowBubbleDuringTween = false;
};
inline constexpr ReaperSpeechLayout ReaperSpeech{};
}
