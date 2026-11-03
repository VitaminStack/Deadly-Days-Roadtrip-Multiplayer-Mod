# Game

Einzige Schicht mit SDK-Zugriff. Enthält Adapter auf Spielklassen und die Hook-Definitionen, je Spielsystem eine Datei
(zum Beispiel `PlayerAdapter`, `EnemyAdapter`, `InventoryAdapter`). Bei einem Spiel-Update wird vor allem dieser Ordner angepasst.

Einstieg ist `Game::Start` in `Game/Game.h`, noch ohne Inhalt. SDK-Einbindung und Muster für die Adapter kommen mit `game-adapter`. Darf `Core` nutzen.
