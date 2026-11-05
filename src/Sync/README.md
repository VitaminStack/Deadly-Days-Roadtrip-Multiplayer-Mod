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

Einstieg ist `Sync::Start` in `Sync/Sync.h`. Darf `Core`, `Net` und `Game` nutzen, aber kein SDK einbinden.
`Sync.cpp` leert den Kontrollkanal einmal pro Tick und verteilt die Nachrichten
an Progress und Desync. Diagnoseanzeige wird als Callback aus Mod angebunden,
keine direkte Abhängigkeit zum UI. [Desync-Melder, Konfiguration und offene Spielabnahme](../../docs/DESYNC.md).
