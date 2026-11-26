#pragma once

// opt-sensenmann-ansage-client: Ansage, Ton und Stoppuhr beim Client, wenn der Sensenmann des Hosts erscheint. Den Sensenmann erzeugt
// nur der Host, er kommt über die native Replikation. Sobald er beim Client steht, holt dieser Adapter lokal nach, was die
// Spawn-Funktion des Hosts danach tut (OnReaperSpawned an die Anzeige, Ton, Sprechblase, WasReaperSpawned). Keine Netzwerknachricht,
// die Regeln stehen in Game/ReaperAnnouncementPolicy.h.
namespace Game
{
// Game-Thread-Tick, einmal je Sekunde, nur beim Client aktiv.
void TickReaperAnnouncement();
}
