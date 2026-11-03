# Sync

Legt fest, was wann zwischen Host und Client abgeglichen wird. Greift über `Game` auf das Spiel und über `Net` auf die Verbindung zu.

Geplante Unterordner, jeweils mit eigener Zuständigkeit:

| Ordner | Inhalt |
|---|---|
| `Players/` | Beitritt, Bewegung, Status der Spieler |
| `Combat/` | Waffen, Projektile, Treffer, Fähigkeiten |
| `Enemies/` | Spawn, Position und Tod der Gegner, Bosse |
| `World/` | Karte, Tageszyklus, Missionen, Loot |
| `Inventory/` | Rucksack, Stash, Rewards |

Welche Teile die Engine selbst übernimmt, hängt von Gate G1 ab.

Einstieg ist `Sync::Start` in `Sync/Sync.h`, noch ohne Inhalt. Darf `Core`, `Net` und `Game` nutzen, aber kein SDK einbinden.
