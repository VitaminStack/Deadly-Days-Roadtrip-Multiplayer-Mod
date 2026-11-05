# Game

Einzige Schicht mit Spielzugriff. Enthält Adapter auf Spielklassen und später die Hook-Definitionen, je Spielsystem eine Datei
(zum Beispiel `PlayerAdapter`, `EnemyAdapter`, `InventoryAdapter`). Bei einem Spiel-Update wird vor allem dieser Ordner angepasst. Darf `Core` nutzen.

## Bausteine

| Datei | Aufgabe |
|---|---|
| `Game.h` | `Game::Start` registriert die Adapter-Ticks und startet den Takt, aufgerufen von `Mod.cpp` |
| `GameThread.h` | Takt auf dem Game Thread, siehe unten |
| `Offsets.h` | Alle Offsets und RVAs als Konstanten, Herkunft in [docs/OFFSETS.md](../../docs/OFFSETS.md) |
| `Memory.h` | Geschützte Lesezugriffe (`Read<T>`, `ReadPointer`), Schreiben von Werten (`WriteBytes`) und Objektnamen (`NameOf`) |
| `SdkChecks.cpp` | Prüft `Offsets.h` beim Bauen gegen das Dumper-7-SDK, ohne SDK leer |
| `WorldAdapter.h` | Muster-Adapter: Karte, Rolle (allein, Host, Client) und Spielerzahl |
| `DesyncAdapter.h` / `DesyncSnapshot.h` | Lesende Diagnosebilder, kanonische HP-/Inventar-Hashes, explizit unbekannte Felder; [Messumfang und offene Abnahme](../../docs/DESYNC.md) |
| `Objects.h` | Suche im Objekt-Array (`FindInstance`, `FindObject`, `IsA`) und Aufruf über `ProcessEvent` |
| `TutorialAdapter.h` | Schaltet die Tutorial-Hinweise als Host und Client ab, Prüfung einmal je Sekunde |
| `TelemetryAdapter.h` | Schaltet im Mod-Betrieb Balance-Telemetrie und automatische Berichte über beschädigte Spielstände ab und sperrt die Telemetrie-Adresse über `Core::ReportHosts` |
| `Patches.h` | Die Spielfehler-Fixes für eine Mission zu zweit, siehe [Patches](#patches) |
| `CodeCave.h` | Werkzeuge für Patches mitten in Funktionen: Code-Höhle nahe der Exe, Bytes prüfen, Sprünge und vtable-Einträge schreiben |
| `RemoteP2Policy.h` | Reine Logik für den Remote-Spieler als P2: `ActivePlayers`-Map prüfen und aufräumen, Geräte-Wächter, ohne Spiel getestet |
| `PatchPolicy.h` | Reine Regeln der Patches (SpawnedPlayers-Menge, Wächter-Code, Reise, Busplatz), ohne Spiel getestet |

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

Schreibende Zugriffe und Hooks folgen demselben Muster, siehe [Patches](#patches).

## Patches

Die Fixes, ohne die keine Mission zu zweit läuft, kommen aus dem Spike (Aufgabe `patches`). Herkunft und Belege jeder Stelle stehen in
der [Patch-Tabelle des Spikes](../../research/spikes/a-connect/README.md#patches). Die RVAs stehen als Konstanten am Anfang der jeweiligen Datei.

| Datei | Seite | Inhalt |
|---|---|---|
| `PlayerPatches.cpp` | Host | Spawn und Spielerindex ohne `LocalPlayer`, Loot- und Gegner-Spawner ohne Spieler, `SpawnedPlayers` bei Registrierung und Controller-Tausch, Abgleich je Sekunde als Rückfall, Client-Position annehmen, Busplatz schont alle Spielerfiguren, Run-Ende ohne `LocalPlayer` |
| `ClientPatches.cpp` | Client | Refresh, Attribut-Tick, AttributeSet-Suche, Kartengenerator-Schritt, Figurenkomponente und ThreatLevel ohne Host-Objekte; Exp-Aggregation wartet auf AttributeSets; Blockaden nur beim Host; NetGUID-Pfade bleiben; SneakApproach wartet auf Mesh und PlayerState; keine lokale PowerUp-Kopie |
| `MovementPatches.cpp` | Client | Nach dem Dodge: eigene Figur bekommt Reibung und Schwerkraft zurück, fremde Figur folgt dem replizierten Ort, wenn ihre Unterlage beim Client fehlt oder sie ohne Modus hängt, siehe [Dodge beim Client](../../docs/GAME_ANALYSIS.md#dodge-beim-client-bewegung) |
| `CameraPatches.cpp` | Client | Kamera-Spieler aus der `PlayerArray` statt aus dem GameMode, Bodenfix, gemeinsame Kamera als ViewTarget je Sekunde; Bewegungs-Restrictor ohne GameState stürzt nicht ab und bekommt ihn nachgetragen, siehe [Restrictor](../../docs/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz) |
| `InventoryPatches.cpp` | beide | Inventar-Unterobjekte über den klassischen Replikationspfad, Timer des Achievement-Detektors beim `EndPlay` löschen, Kartengenerator-Schritt nur für die aktuelle Welt; [Itemsack-Vergabe](../../research/spikes/itemsack-absturz/README.md) nur auf Authority außerhalb einer Client-Welt |
| `TravelPatches.cpp` | Host | Reise ins Spiel wird `servertravel`, vorher Zeitanfragen abräumen; allein und ins Hauptmenü reist das Spiel wie gewohnt |
| `WidgetPatches.cpp` | Client | `WP_UI` wartet auf den GameState; Beobachter nach jedem `ProcessEvent` |
| `RemoteP2Patches.cpp` | Host | Remote-Spieler als P2 in `ActivePlayers` des Coop-Subsystems, ohne zweiten `LocalPlayer`; beim Verlassen nur die eigene Bindung entfernen; lokale P2-Gerätepflicht nur bei vollständiger Bindung ausnehmen. Koop-Modus erst nach `AllowRemoteCoop()`, siehe [Remote-Spieler](#remote-spieler-als-p2) |
| `HealthBarPatches.cpp` | Client | Lebensleisten nach repliziertem `OnRep_Health` und `OnRep_MaxHealth` |

**Alle oder keiner.** `Patches::Install` läuft in `Game::Start` vor dem ersten Takt. Es prüft zuerst jede Stelle (Bytes, vtable-Einträge)
und setzt erst dann alle Patches. Passt eine Stelle nicht, setzt es keinen und protokolliert die Stelle; das Spiel läuft dann wie ohne Mod.
Ganze Funktionen hängt `Core::Hooks` (MinHook) ein, Stellen mitten in einer Funktion springen in Code-Höhlen aus `CodeCave.h`.
Was Spielobjekte braucht (`Default__DDMissionGameMode`, Funktionsnamen), holt `Patches::Tick` auf dem Game Thread nach.

**Mod und Spike.** Die Mod exportiert `DDCoopPatchState` (0 offen, 1 alle gesetzt, 2 keiner, 3 ein Teil). Findet der Spike die Mod mit 1 oder 3,
setzt er seine eigenen Fassungen nicht und meldet seinen Phasenbeobachter über `DDCoopObserveProcessEvent` an. Karte und PCG vom Host,
Testhilfen und Messung bleiben im Spike, sie gehören zu späteren Aufgaben (`missionsstart`).

### Remote-Spieler als P2

Aus der Spike-Probe [`remote-p2`](../../research/spikes/a-connect/RemoteP2.md), Aufgabe `remote-spieler`. Der Host trägt in jedem Takt den
aktuellen lokalen und den Remote-Controller als Schlüssel 0 und 1 in `ULocalCoopSubsystem::ActivePlayers` ein, mit dem nativen Map-Add und
schwachen Verweisen. Danach liefert `GetPlayerController(1)` den Remote-Controller und `GetActivePlayerCount()` 2. Nur beim Listen-Host mit
genau einem `LocalPlayer` und höchstens zwei aktuellen Controllern, sonst ändert die Mod nichts. `Patches::RemoteP2Bound()` meldet den Zustand.

**Koop-Modus bleibt aus.** `SetCoopEnabled(true)` und die native Beitrittsmeldung `OnPlayerJoined(1, …)` laufen erst, wenn
`Patches::AllowRemoteCoop()` gerufen wurde. Mit Koop-Modus wartet das Lager am Tagesende auf „Bestätigen & Wechseln“ von P2 und hängt, und
die Airdrop-Auswahl wartet auf den Claim von P2. Das lösen `airdrop-auswahl` und `inventar-ui` über den Kontrollkanal, dann schalten sie
den Modus ein. Zurückschalten ist nicht vorgesehen: `SetCoopEnabled(false)` entfernt laut Analyse vorher einen vorhandenen P2.

**Geprüft** (05.11.2026, Build `26187885`, `Start-Session.ps1 -Build -Mod -Tunnel Lobby`, Seed `20795`, ein PC):

- Ohne Koop-Modus: In MetaBase und an Tag 1 bis 3 nach jedem Wechsel `p1=true p2=true aktiv=2`. Lager, Routenkarte und ein Level-up-Airdrop,
  vom Nutzer eingesammelt, liefen wie ohne Mod. Inventare gleich, kein Absturz.
- Mit Koop-Modus (Gegenprobe): Grüne P2-Markierung über der Client-Figur, kein Controller-Fenster. `Test-NetEnd.ps1 -Cases b,d`: Bindung
  beim Verlassen und beim harten Abbruch entfernt (`aktiv=1`, `reconnect=0`), nach dem Neubeitritt neu gesetzt. Das Lager am Ende von
  Tag 1 hing mit beiden Rucksäcken bei „Bestätigen & Wechseln“, deshalb der Standard aus.

**Geprüft** (04.11.2026, Build `26187885`, `Start-Session.ps1 -Build -Mod`, Seed `20795`): MetaBase, dann Tag 1 bis 5 über die echte Route,
danach Niederlage und „Zurück zur Basis“. Kein Absturz, die Inventare waren nach jedem Wechsel gleich. `SpawnedPlayers` kam bei jedem Wechsel
über Registrierung und Tausch in Ordnung, der Abgleich je Sekunde hatte nichts zu tun. Gegriffen haben im Log: Reise, Zeitanfragen, Busplatz,
Controller-Tausch, Client-Position, Kamera, PowerUp, Lebensleisten. Die übrigen Schutzstellen (HUD, Exp, Blockade, alter Kartengenerator)
wurden in diesem Lauf nicht gebraucht; ihre Logik ist mit `tests/GameTests/PatchTests.cpp` und aus den Spike-Läufen belegt.

## Ohne Spike

Im Release gibt es keinen Spike. Stand `ohne-spike` (05.11.2026, Build `26187885`): Die Mod läuft allein, Lobby-Beitritt, MetaBase und
Mission an Tag 1 ohne Absturz, geprüft mit `Start-Session.ps1 -NoSpike` und `Run-Route.ps1 -Start -NoSpike -Days 1 -StayLast`,
siehe [Nur Mod, ohne Spike](../../docs/TESTING.md#nur-mod-ohne-spike). Der Absturz beim Beitritt ohne Spike (Host `0x5901CE8`,
Client `0x56B16D9`) stammte aus der Zeit vor `patches`, heute setzt die Mod beide Fixes beim Spielstart.

**Für den Betrieb nötig, aber nur im Spike.** Beide laufen dort über Dateien in `%TEMP%\ddspike`, also nur auf einem PC. In die Mod
gehören sie mit Übertragung über den [Kontrollkanal](../../docs/CONTROL_PROTOCOL.md), das ist Arbeit der genannten Aufgaben, nicht ein
Kopieren des Spike-Codes.

| Spike-Datei | Eingriff | Ohne ihn | Aufgabe |
|---|---|---|---|
| `MapGen.cpp` | Client erzeugt die Karte mit Seed, Profil, Größe und Tags des Hosts (`0x59EF6D0`), lokale Kopien replizierter Child Actors unterdrückt (`0x288B510`) | Client erzeugt eine eigene Karte ohne Missionsdaten, sie scheitert (17 statt 42 bis 138 Kacheln). Seine Figur startet abseits vom Bus und läuft auf anderem Boden, im ersten Lauf sah er die Host-Figur nicht | `missionsstart` |
| `Pcg.cpp` | PCG-Graph des Hosts beim Client (`0x59EE6A0`), gleiche Seeds der ISM-Komponenten im Sekunden-Tick | Andere Vegetation beim Client | `missionsstart` |
| `Mission.cpp`, `RoundPhase*.h`, Beobachter in `Widgets.cpp` | Rundenübersicht, Lager und Routenkarte des Hosts beim Client mit Anzeige und Eingabesperre | Client bleibt in der Missionsansicht, während der Host im Lager oder auf der Routenkarte ist | `pausen-sync`, `tagesablauf` |

**Schon in der Mod**, der Spike lässt seine Fassung aus, sobald `DDCoopPatchState` 1 meldet: `Players.cpp`, `Client.cpp`, `Camera.cpp`,
`Inventory.cpp`, `Travel.cpp`, `Widgets.cpp` (HUD wartet auf den GameState), `MapObjects.cpp` (PowerUp-Kiste), Busplatz aus `TestEnv.cpp`,
`HealthBars.cpp`, `RunEnd.cpp`. Dazu ohne Gegenstück im Spike-Schalter: Sperre des Crash-Reporters (`Core/CrashReports`) und
Tutorial-Hinweise (`TutorialAdapter`, im Spike `Cheats.cpp`). `Tunnel.cpp` ersetzt der Transport aus `src/Net`.

**Nur Testhilfe oder Messung**, kommt nicht in die Mod: Befehlsdatei und Konsolenbefehle (`Main.cpp`, etwa `open`, `servertravel`,
`DD_SetCurrentRunSeed`; Ersatz mit Antworten ist `mod-steuerung`), `Cheats.cpp` (CheatManager, `!load`), Godmode, `!finish` und `!round`
aus `Mission.cpp`, `!wave`, `Trace.cpp`, `Load.cpp`, `Hits.cpp`, Diagnose-Log aus `TestEnv.cpp`, die `RemoteP2.cpp`-Probe und die
zweite Sperre des Crash-Reporters in `CrashReports.cpp`.

**Geprüft** (05.11.2026, ein PC, zwei Läufe): `spike.dll` war in keiner Instanz geladen, `DDCoop.dll` in beiden, je 30 Patches gesetzt.
Lobby-Beitritt in rund 4 s, beide in der MetaBase mit 2 Spielern. Im ersten Lauf ging der Host über `Game-Input.ps1` und den Startring in die Mission,
im zweiten über `Run-Route.ps1`. In beiden Läufen reisten beide Instanzen nach `DDDynamic_Map`. Im Mod-Log griffen Reise, Busplatz,
Controller-Tausch, Client-Position, Kamera und PowerUp. Bewegung des Clients kam beim Host an (270 Einheiten in 1,5 s, gleiche Position
auf beiden Seiten). Im zweiten Lauf starben beide Figuren ohne Eingabe nach gut 3 Minuten. Der Host zeigte „Du bist gestorben“, der
Client blieb in der Missionsansicht (fehlende Rundenphase, siehe oben). Nach „Zurück zur Basis“ waren beide wieder in der MetaBase. Kein
Absturz, keine Absturzberichte. Nicht geprüft: die Knotensuche auf der
Routenkarte, weil beide Test-Spielstände ohne Routenkarte in den ersten Run fuhren.
