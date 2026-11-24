# Darstellung beim Client

Gilt für Build `26786272`. Datierte Befunde und Messungen stehen im [Archiv](../archive/GAME_ANALYSIS.md), Abschnitte unten unter „Historie“.

## Überblick

Vieles zeigt das Spiel nur dort, wo die Logik läuft, also beim Host. Die Mod spielt diese Darstellung beim Client lokal nach, ohne zweite Autorität:

- Lebens-, Ausdauer-, Rüstungs-, Bus- und Lootbalken aus replizierten Werten.
- Blut, Fleisch und Körperteile beim Tod eines Gegners, XP-Bälle, Schuss- und Nachladesound, Aufsammelsound.
- Blickrichtung und Laufanimation der Figuren, HUD-Porträt der eigenen Figur.

## Code

| Datei | Aufgabe | Test |
|---|---|---|
| `src/Game/HealthBarPatches.cpp` | Lebensleisten nach `OnRep_Health` | – |
| `src/Game/BusBarPatches.cpp`, `src/Game/LootBarPatches.cpp` | Bus- und Lootbalken | `tests/GameTests/PatchTests.cpp` |
| `src/Game/BloodPatches.cpp`, `src/Game/BloodPolicy.h`, `src/Game/BodyPartPatches.cpp` | Blut und Körperteile | `tests/GameTests/BloodPolicyTests.cpp` |
| `src/Game/ExpPatches.cpp`, `src/Game/ExpBounds.h` | XP-Bälle | `tests/GameTests/ExpBoundsTests.cpp` |
| `src/Game/WeaponSoundPatches.cpp`, `src/Game/FmodSound.cpp` | Waffensounds | `tests/NetTests/WeaponSoundTests.cpp` |
| `src/Game/FacingPatches.cpp`, `src/Game/WalkAnimationPatches.cpp` | Blickrichtung und Laufanimation | `tests/GameTests/PatchTests.cpp` |

## Offene Fehler

Keine eigene Fallakte. Offene Punkte zur Darstellung stehen unter den Optionen, etwa `opt-client-visuell` und `opt-schuss-animation` in [docs/OPTIONEN.md](../OPTIONEN.md).

## Historie

Wortgleiche Befunde, nur zum Nachschlagen, nicht als Ist-Zustand lesen:

- [Lebensleisten beim Client (`hp-leisten`)](../archive/GAME_ANALYSIS.md#lebensleisten-beim-client-hp-leisten)
- [Ausdauerbalken beim Client (`opt-ausdauerbalken`)](../archive/GAME_ANALYSIS.md#ausdauerbalken-beim-client-opt-ausdauerbalken)
- [Rüstung beim Client (`opt-astronaut-ruestung`)](../archive/GAME_ANALYSIS.md#rüstung-beim-client-opt-astronaut-ruestung)
- [Lootbalken beim Client (`opt-lootbalken`)](../archive/GAME_ANALYSIS.md#lootbalken-beim-client-opt-lootbalken)
- [Blut beim Client (`opt-blut`)](../archive/GAME_ANALYSIS.md#blut-beim-client-opt-blut)
- [Blickrichtung beim Client (`opt-blickrichtung-client`)](../archive/GAME_ANALYSIS.md#blickrichtung-beim-client-opt-blickrichtung-client)
- [Laufanimation beim Client (`opt-laufanimation-client`)](../archive/GAME_ANALYSIS.md#laufanimation-beim-client-opt-laufanimation-client)
- [Schuss-Animation und Geisterwaffen (`opt-schuss-animation`)](../archive/GAME_ANALYSIS.md#schuss-animation-und-geisterwaffen-opt-schuss-animation)
- [XP-Bälle beim Client (`xp-baelle`)](../archive/GAME_ANALYSIS.md#xp-bälle-beim-client-xp-baelle)
- [HUD-Porträt (`opt-portraet`)](../archive/GAME_ANALYSIS.md#hud-porträt-opt-portraet)
- [Sound und Effekte beim Client (`vfx-sound`)](../archive/GAME_ANALYSIS.md#sound-und-effekte-beim-client-vfx-sound)
- [Körperteile beim Client (`opt-koerperteile`)](../archive/GAME_ANALYSIS.md#körperteile-beim-client-opt-koerperteile)
- [Busbalken beim Client (`opt-busbalken`)](../archive/GAME_ANALYSIS.md#busbalken-beim-client-opt-busbalken)
