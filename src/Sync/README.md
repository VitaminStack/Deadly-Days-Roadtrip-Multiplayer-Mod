# Sync

Legt fest, was wann zwischen Host und Client abgeglichen wird. Greift über `Game` auf das Spiel und über `Net` auf die Verbindung zu.

**Lokal zuerst:** Vorhandene Client-Logik für UI, Vorschau und Darstellung nutzen, nur notwendige
Multiplayer-Aktionen und bestätigte gemeinsame Daten übertragen. Keine parallelen Zustandsbesitzer
oder zusätzlichen Nachrichten für bereits nativ replizierte/ableitbare Werte. Vollbilder für
Join/Travel/Reconnect bleiben nötig. [Architekturregel](../../docs/ARCHITECTURE.md#projektweite-nutzervorgabe-vom-05112026)
und [konkrete Fundstellen](../../docs/CLIENT_LOCAL_AUDIT.md).

Geplante Unterordner, jeweils mit eigener Zuständigkeit:

| Ordner | Inhalt |
|---|---|
| `Players/` | Beitritt, Bewegung, Status der Spieler |
| `Combat/` | Waffen, Projektile, Treffer, Fähigkeiten |
| `Enemies/` | Spawn, Position und Tod der Gegner, Bosse. Ein Gegnertyp läuft nativ, ohne eigenen Code, siehe [Enemies](Enemies/README.md) |
| `World/` | Karte, Tageszyklus, Missionen, Loot |
| `Inventory/` | Airdrop-Auswahl und Edit des Clients (Rucksack, Rewards) |

Gate G1 legt native UE-Replikation mit gezielten Kontrollkanal-Ergänzungen fest.

Einstieg ist `Sync::Start` in `Sync/Sync.h`. Darf `Core`, `Net` und `Game` nutzen, aber kein SDK einbinden.
`Sync.cpp` leert den Kontrollkanal einmal pro Tick und verteilt die Nachrichten
an Progress, Desync, Reward und die Karte (`World/MapSync.cpp`, gleiche Karte bei beiden, `missionsstart`). Diagnoseanzeige wird als Callback aus Mod angebunden,
keine direkte Abhängigkeit zum UI. [Desync-Melder, Konfiguration und offene Spielabnahme](../../docs/DESYNC.md).
