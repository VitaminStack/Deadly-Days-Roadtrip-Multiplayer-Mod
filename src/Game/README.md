# Game

Einzige Schicht mit Spielzugriff. Enthält Adapter auf Spielklassen und später die Hook-Definitionen, je Spielsystem eine Datei
(zum Beispiel `PlayerAdapter`, `EnemyAdapter`, `InventoryAdapter`). Bei einem Spiel-Update wird vor allem dieser Ordner angepasst. Darf `Core` nutzen.

## Bausteine

| Datei | Aufgabe |
|---|---|
| `Game.h` | `Game::Start` registriert die Adapter-Ticks und startet den Takt, aufgerufen von `Mod.cpp` |
| `GameThread.h` | Takt auf dem Game Thread, siehe unten |
| `Offsets.h` | Alle Offsets und RVAs als Konstanten, Herkunft in [docs/OFFSETS.md](../../docs/OFFSETS.md) |
| `Memory.h` | Geschützte Lesezugriffe (`Read<T>`, `ReadPointer`) und Objektnamen (`NameOf`) |
| `SdkChecks.cpp` | Prüft `Offsets.h` beim Bauen gegen das Dumper-7-SDK, ohne SDK leer |
| `WorldAdapter.h` | Muster-Adapter: Karte, Rolle (allein, Host, Client) und Spielerzahl |

## Game Thread

`GameThread::Start` sucht im Hintergrund das `UnrealWindow` des eigenen Prozesses, hängt sich in dessen Fensterprozedur und setzt dort einen
Timer (16 ms). Unreal pumpt diese Nachrichten auf dem Game Thread zwischen zwei Frames. In jedem Takt laufen erst
`Core::GameThread().Drain()`, dann die mit `AddTick` registrierten Ticks. Ein Tick, der selbst Nachrichten pumpt, löst keinen zweiten aus.
Mit Host und Client im Spiel geprüft, auch über den Wechsel auf Tag 1. Der Spike hängt sich an dasselbe Fenster, beide laufen nebeneinander.

## SDK

Das Dumper-7-SDK liegt nie im Repo. Ist `Dumper7SdkRoot` in `config/Local.props` gesetzt und vorhanden, definiert `Game.vcxproj`
`DDCOOP_WITH_SDK` und bindet die SDK-Header als externe Includes ohne Warnungen ein. Genutzt werden nur Header, keine SDK-Quellen.
`SdkChecks.cpp` vergleicht dann jeden Wert aus `Offsets.h` per `static_assert` mit `offsetof` im SDK. Ein falscher Wert bricht den Build.
Die CI und Mitwirkende ohne SDK bauen dieselbe Mod, nur ohne diese Prüfung. Beim Start steht im Log, welcher Fall gilt.

Warum keine SDK-Typen im Laufzeitcode: Die Mod muss überall bauen, auch in der CI. Und gegen ein SDK eines anderen Builds würde sie still
falsche Layouts verwenden, statt laut zu scheitern.

## Muster für neue Adapter

1. Header ohne SDK- und Windows-Typen, mit einfachen Structs wie `WorldInfo`. So können `Sync` und Tests ihn nutzen.
2. Neue Offsets in `Offsets.h` eintragen, in `docs/OFFSETS.md` belegen und in `SdkChecks.cpp` gegen das SDK prüfen.
3. Spielspeicher nur über `Memory::Read`/`ReadPointer` lesen. Ein Lesefehler liefert `nullopt`, nie einen Absturz.
4. Nur auf dem Game Thread aufrufen. Lesefunktionen prüfen `Core::GameThread().IsDrainThread()`. Andere Threads reichen Arbeit mit
   `Core::GameThread().Post` herein.
5. Laufende Beobachtung als Tick über `GameThread::AddTick` in `Game::Start` registrieren. Logs nur bei Änderungen, nicht in jedem Takt.
6. Reine Logik (wie `RoleFrom`) als freie Funktion herausziehen und in `tests/GameTests` ohne Spiel testen.

Schreibende Zugriffe, Hooks und ProcessEvent-Aufrufe kommen mit `patches`. Sie folgen demselben Muster und nutzen `Core::Hooks`.
