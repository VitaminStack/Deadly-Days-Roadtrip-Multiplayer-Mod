#pragma once

// Boss-Leiste beim Client (gegner-alle). Der Host legt das Boss-Objective über den Spawnpunkt an, beim Client gibt es keins. Dieser
// Adapter baut es lokal aus dem nativ replizierten Boss und seinen Lebenspunkten, ohne eigene Netzwerknachricht. Die Entscheidung
// steht in Game/BossBarPolicy.h.
namespace Game
{
// Game-Thread-Tick, einmal je Sekunde, nur beim Client aktiv.
void TickBossBar();
}
