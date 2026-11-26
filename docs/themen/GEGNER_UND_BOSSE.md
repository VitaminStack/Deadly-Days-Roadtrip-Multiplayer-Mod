# Gegner und Bosse

Gilt für Build `26786272`. Datierte Befunde und Messungen stehen im [Archiv](../archive/GAME_ANALYSIS.md), Abschnitte unten unter „Historie“.

## Überblick

- Gegner, Spawning und Pooling laufen beim Host; der Client sieht sie über die native Replikation.
- Was beim Host nur lokal entsteht, zeigt der Client nach: Warnanzeigen der Angriffe (Kreise und Sprint-Striche), Schadenszahlen, Gegner-Sounds, Bedrohungsstufe.
- Boss-Leisten: Der Client zeigt wie der Host nur die HP der laufenden Phase und erst ab Kampfbeginn, umgesetzt für alle Bosse. Zu zweit bestätigt ist bisher nur der Lazy-Boss.
- Hält der Host die Weltzeit an (etwa nach dem Bosstod), zeigt der Client nach 1 s den Hinweis „Der Host hat das Spiel angehalten“.
- Den Sensenmann erzeugt nur der Host. Kommt er beim Client an, holt dieser Ansage, Ton und Stoppuhr lokal nach, ohne Nachricht; die Ausweitung der Spawn-Biome bleibt beim Host ([Fallakte](../faelle/opt-sensenmann-ansage-client.md)).

## Code

| Datei | Aufgabe | Test |
|---|---|---|
| `src/Game/EnemyTelegraphPatches.cpp`, `src/Game/TelegraphAdapter.h` | Warnanzeigen der Gegnerangriffe | `tests/NetTests/TelegraphTests.cpp` |
| `src/Game/DamageNumberPatches.cpp`, `src/Game/DamageNumberAdapter.h` | Schadenszahlen beim Client | `tests/NetTests/DamageNumberTests.cpp` |
| `src/Game/EnemySoundPatches.cpp`, `src/Game/EnemySoundPolicy.h` | Gegner-Sounds | `tests/GameTests/EnemySoundPolicyTests.cpp` |
| `src/Game/ThreatAdapter.cpp`, `src/Game/ThreatPolicy.h` | Bedrohungsstufe | `tests/NetTests/ThreatTests.cpp` (Nachricht `ThreatState`) |
| `src/Game/BossBarAdapter.cpp`, `src/Game/BossBarPolicy.h` | Boss-Leiste | `tests/GameTests/BossBarPolicyTests.cpp` |
| `src/Game/ProjectilePatches.cpp` | Projektile aus dem Pool beim Client sichtbar | – |
| `src/Game/ReaperAnnouncementAdapter.cpp`, `src/Game/ReaperAnnouncementPolicy.h` | Ansage des Sensenmanns beim Client | `tests/GameTests/ReaperAnnouncementPolicyTests.cpp` |

Mehr: [src/Sync/Enemies/README.md](../../src/Sync/Enemies/README.md).

## Offene Fehler

[opt-sensenmann-ansage-client](../faelle/opt-sensenmann-ansage-client.md). Bestätigungstests stehen bei `opt-boss-phasenleiste`, `opt-boss-tafel-client` und `opt-endboss-leiste` in [TODO.md](../../TODO.md).

## Historie

Wortgleiche Befunde, nur zum Nachschlagen, nicht als Ist-Zustand lesen:

- [Boss-Dump (`boss-dump`)](../archive/GAME_ANALYSIS.md#boss-dump-boss-dump)
- [Warnanzeigen der Gegner beim Client (`opt-gegner-indikatoren`)](../archive/GAME_ANALYSIS.md#warnanzeigen-der-gegner-beim-client-opt-gegner-indikatoren)
- [Schadenszahlen beim Client (`opt-schadenszahlen`)](../archive/GAME_ANALYSIS.md#schadenszahlen-beim-client-opt-schadenszahlen)
- [Verlorene Schadenszahlen (`opt-schadenszahlen-verlust`)](../archive/GAME_ANALYSIS.md#verlorene-schadenszahlen-opt-schadenszahlen-verlust)
- [Gegner und Pausen](../archive/GAME_ANALYSIS.md#gegner-und-pausen)
- [Gegner-Spawning und Pooling](../archive/GAME_ANALYSIS.md#gegner-spawning-und-pooling)
- [Ein Gegnertyp synchron (`gegnertyp`)](../archive/GAME_ANALYSIS.md#ein-gegnertyp-synchron-gegnertyp)
- [Bedrohungsstufe beim Client (`opt-bedrohung-sound`)](../archive/GAME_ANALYSIS.md#bedrohungsstufe-beim-client-opt-bedrohung-sound)
- [Endboss erscheint nicht (`endboss-spawn`)](../archive/GAME_ANALYSIS.md#endboss-erscheint-nicht-endboss-spawn)
- [Stillstand des Clients nach dem Bosstod (`boss-client-einfrieren`)](../archive/GAME_ANALYSIS.md#stillstand-des-clients-nach-dem-bosstod-boss-client-einfrieren)
- [Phasen und Kampfstart der Boss-Leiste beim Client (`opt-boss-phasenleiste`)](../archive/GAME_ANALYSIS.md#phasen-und-kampfstart-der-boss-leiste-beim-client-opt-boss-phasenleiste)
- [Boss-Leiste aller Bosse beim Client (`opt-endboss-leiste`)](../archive/GAME_ANALYSIS.md#boss-leiste-aller-bosse-beim-client-opt-endboss-leiste)
- [Absturz beim Betreten der Boss-Karte (`boss-karte-absturz`)](../archive/GAME_ANALYSIS.md#absturz-beim-betreten-der-boss-karte-boss-karte-absturz)
- [Hinweis beim Client, wenn der Host die Welt anhält (`opt-boss-tafel-client`)](../archive/GAME_ANALYSIS.md#hinweis-beim-client-wenn-der-host-die-welt-anhält-opt-boss-tafel-client)
