# Spike A und B: Verbindung und Spieler

Stand: 02.11.2026, Build `26187885`. Wegwerfcode, die Offsets gelten nur für diesen Build.

## Aufbau

- Zwei Instanzen auf einem PC unter demselben Steam-Konto, Verbindung über `127.0.0.1:7777`.
- Die Exe wird direkt gestartet, mit den Umgebungsvariablen `SteamAppId` und `SteamGameId` sowie je einem eigenen `-UserDir=`.
  So bleibt der echte Spielstand unberührt.
- Der Spike wird als DLL in beide Instanzen geladen. Sie führt Konsolenbefehle aus einer Datei auf dem Game Thread aus.
  Der Game Thread wird über eine Nachricht an das Spielfenster erreicht.

Gebaut und gestartet wird über die Skripte in `tools/spike/`, die auch den Crash-Reporter absichern, siehe [Testen](../../../docs/TESTING.md).

## Builds

Der Quelltext nennt seit `spiel-update` (09.11.2026) Code- und Datenadressen von `26495302`, die Abschnitte unten mit älterem Stand die von `26187885`
([Umrechnung](../../../docs/OFFSETS.md#referenz-build-26495302-spiel-update)). `Rva()` in `Builds.cpp` liefert sie nur auf diesem Build, sonst `kMissingRva`.
Neue Adressen immer als `Rva(0x…)` schreiben, relative Sprünge mit `ExpectBranch` prüfen, Importeinträge mit `FindImportSlot` suchen.

## Dateien

Je Thema eine Datei, damit parallele Arbeit nicht in dieselbe Datei läuft. Neue Patches kommen in die passende Datei und werden in deren `Patch…`-Funktion eingehängt.

| Datei | Inhalt |
|---|---|
| `Spike.h` | Gemeinsame Offsets und Schnittstellen |
| `Core.cpp` | Log, Objektsuche, `CodeWriter` für Patches |
| `Main.cpp` | Laden der DLL, Befehls-Schnittstelle, Sekunden-Tick |
| `Players.cpp` | Spawn der Remote-Spieler, `SpawnedPlayers`, clientautoritative Position |
| `RemoteP2.cpp` | Ausdrücklich einschaltbare Phase-1-Probe: schwache `ActivePlayers`-Bindungen, native Coop-Aktivierung, gezielter Geräte-Guard und getrennte Beitrittsmeldung; [Ergebnis und Grenzen](RemoteP2.md) |
| `Client.cpp` | Client-Stellen ohne Host-Objekte: Refresh, Attribute, Figurenkomponente, `ThreatLevel`, Blockaden-Autoritätsablauf, Log für nicht angelegte Actors |
| `Camera.cpp` | Gemeinsame Kamera und Bodenfix |
| `Inventory.cpp` | Netzwerk-Support für Items/Slots und klassischer Unterobjekt-Replikationspfad je neuer Inventarkomponente |
| `Travel.cpp` | Umleitung der Reise in `servertravel`, vorher Zeitanfragen des `UTimescaleManager` abräumen |
| `Widgets.cpp` | Zurückhalten der HUD bis zum `GameState`, nach jedem `ProcessEvent` der Phasenbeobachter aus `Mission.cpp` |
| `MapObjects.cpp` | Client: lokale Kopien replizierter Kartenobjekte unterdrücken |
| `Mission.cpp` | Gegnerwelle, Missionsende, Rundenphasen vom Host zum Client mit Anzeige und Eingabesperre |
| `Trace.cpp` | Zähler für RPCs, Run-Funktionen, Sounds und Effekte (`!fx`) |
| `CrashReports.cpp` | Sperre des Crash-Reporters, zweite Schicht neben `CrashGuard.ps1` |
| `MapGen.cpp` | Client erzeugt die Karte mit den Werten des Hosts, lokale Kopien replizierter Child Actors unterdrückt. Mit der Mod aus, die das über ihren Kontrollkanal macht (`missionsstart`) |
| `Pcg.cpp` | Client erzeugt die PCG-Vegetation mit dem Graphen des Hosts, gleiche Seeds der Instanz-Komponenten. Mit der Mod aus wie `MapGen.cpp` |
| `Hits.cpp` | Host: Treffer auf Gegner mit Schütze, Schaden und Lebenspunkten |
| `HealthBars.cpp` | Client: Lebensleisten über den Figuren nach replizierten HP aktualisieren |
| `RunEnd.cpp` | Niederlage: Run-Ende-Handler ohne `LocalPlayer` des Remote-Controllers |
| `Tunnel.cpp` | Spike E: UDP-Verkehr des Spiels über Steam Networking tunneln (`!tunnel`) |
| `Cheats.cpp` | Testhilfen: CheatManager für die `DD_`-Befehle, Items vergeben, Laden einzelner Assets, Tutorial abschalten |
| `Load.cpp` | Lastmessung: UDP-Bytes, Pakete und Framerate je Sekunde im Log |
| `TestEnv.cpp` | Host: Busplatz schont alle Spielerfiguren, Diagnose-Log für `EndPlay` jeder Figur mit Stapel |

## Ergebnis

| Schritt | Ergebnis |
|---|---|
| Karten-URL in der Kommandozeile (`Map?listen`) | Wird ignoriert, das Spiel lädt immer das Hauptmenü |
| `open /Game/Maps/MainMenu?listen` beim Host | Klappt, `IpNetDriver` lauscht auf UDP 7777 |
| `open 127.0.0.1:7777` beim Client | Klappt, beide sehen 2 Spieler in derselben Welt |
| Host wechselt über das Menü nach `MetaBase` | Klappt, der Netzwerktreiber bleibt bestehen, der Client folgt aber nicht |
| `servertravel /Game/Maps/MetaBase?listen` | Der Client folgt und lädt die Karte |
| Spawn des Remote-Spielers beim Host | Stürzt ohne Patch ab, mit Patch entstehen 2 Controller, 2 PlayerStates, 2 Characters |
| Client empfängt die Welt | Klappt mit den Patches unten: eigene Figur `AutonomousProxy`, fremde `SimulatedProxy` |
| Bewegung Host → Client | Klappt, die Positionen stimmen überein |
| Bewegung Client → Host | Klappt nur mit Bodenfix und clientautoritativer Position, siehe unten. Dann Abweichung rund 10 Einheiten während des Laufens |
| `servertravel /Game/Maps/DDDynamic_Map?listen` | Klappt, beide sind in derselben Mission, 17 `MapTile` auf beiden Seiten, Kartenbild am Start gleich |
| Gegner | Mit `ForceSpawnWave` beim Host gestartet: Host `Authority`, Client `SimulatedProxy`, Bewegung sichtbar auf beiden Seiten |
| Schießen | Schüsse des Clients treffen beim Host, ziehen Leben ab und töten, gemessen mit `Hits.cpp`, siehe [Treffer des Clients](../../../docs/GAME_ANALYSIS.md#treffer-des-clients-treffer) |
| Mission über das Spielmenü | Klappt mit Umleitung der Reise, beide in DDDynamic_Map, Gegner laufen von selbst. Mit `MapGen.cpp` gleiche Karte auf beiden Seiten, siehe [Client erzeugt die Host-Karte](../../../docs/GAME_ANALYSIS.md#client-erzeugt-die-host-karte-karte-client). Mit `Pcg.cpp` auch gleiche Vegetation, siehe [PCG-Vegetation](../../../docs/GAME_ANALYSIS.md#pcg-vegetation-beim-client-pcg) |
| Missionsende, Routenkarte, Tag 2 | Mit Inventar-Teilfix zweimal erreicht; vollständige replizierte Inventare auf beiden Seiten. Weiterreise auf Tag 3 endet noch mit Host-Absturz, siehe unten |
| Pausemenü ins Hauptmenü | Mit Inventar-Teilfix klappt es für beide (ein Lauf). Ohne ihn stürzt der Client bei `0x5975A6A` ab, weil `UInventoryComponent::EndPlay` über Null-Items läuft, siehe [Trennen](../../../docs/GAME_ANALYSIS.md#trennen-ins-hauptmenü-trennen) |

Beim Host laufen in der `MetaBase` rund 150 bis 300 `ActorChannel`, und es gibt keine `EngineReplicationBridge`-Instanz. Aktiv ist also die klassische Replikation, nicht Iris.

## Bewegung vom Client

Der Client schickt `ServerMovePacked`, die Pakete kommen beim Host an (gleiche Zählerstände). Trotzdem bewegte sich die Figur dort nicht. Zwei Ursachen:

1. **Bodenkacheln.** `MetaBase_Tile_*` sind `Movable`, und jede Seite erzeugt sie selbst, sie sind also nicht repliziert. Auf einem beweglichen Boden sendet der Client seine
   Position relativ zum Boden. Der Host kann die Kachel nicht auflösen und liest die Werte als Weltposition: Der Client meldete `relLoc (739, -395)`, der Host stand danach exakt dort.
   Auch die Beschleunigung kam um die Drehung der Kachel verdreht an. Unter derselben Stelle lag zudem auf beiden Seiten eine andere Kachel (`S0_04` und `S1_05`).
   Der Spike setzt nicht replizierte Böden beim Client auf `Static`, damit Weltkoordinaten gesendet werden.
2. **Host-Simulation.** Mit Weltkoordinaten stimmte die Richtung, die Figur kam beim Host aber nur mit rund 45 statt 600 Einheiten pro Sekunde voran. Die Geschwindigkeit
   sinkt zwischen den Zügen immer wieder auf null. Die Ursache ist nicht geklärt. Der Spike nimmt die Position deshalb direkt vom Client an
   (`bIgnoreClientMovementErrorChecksAndCorrection`, `bServerAcceptClientAuthoritativePosition`).

## Patches

Der Host-Absturz war immer derselbe Callstack. Das Spiel fragt beim Spawnen `GetLocalPlayer()` ab und nutzt das Ergebnis ohne Prüfung:

| Seite | RVA | Problem | Spike-Patch |
|---|---|---|---|
| Host | `0x5901CE8` | Spawn-Logik: Remote-Controller hat keinen `LocalPlayer` | Sprung in den vorhandenen Pfad ohne Coop-Subsystem |
| Host | `0x5B0A2E5` | Helfer Controller zu Spielerindex: gleiche Annahme | Ohne `LocalPlayer` gilt Index 1, also P2 |
| Client | `0x59635E6` | Eine Funktion erwartet ein Objekt in `this+0x370`, das beim Client fehlt | Aufruf bei `0x5A7350B` auslassen |
| Client | `0x5A0DA90` | `AllPlayerFramingCamera` liest `GameMode->SpawnedPlayers`, der Client hat keinen `GameMode` | Ersatzfunktion liest die Figuren aus der `PlayerArray` |
| Client | `0x59B7E17` | Tick sucht das AbilitySystem, bevor der `PlayerState` da ist | Diesen Tick auslassen |
| Host | `0x5994BB0` | `MapLootSpawner` liest die Figur des ersten Spielers, die es beim Spawn noch nicht gibt | Vorhandener Pfad ohne `RootComponent` |
| Host | `0x58CEDC7` | Gegner-Spawner nimmt einen `null`-Eintrag aus `SpawnedPlayers` | Spawn für diesen Tick auslassen |
| Host | `0x5976AD0` | `SeamlessTravelToLevel` reist nur lokal | Umleitung in `servertravel`, nur wenn scharf und nicht ins Hauptmenü. Vorher `ClearAllRequests`, sonst bleibt die Pause aus `FinishMission` stehen |
| Host | `0x5991B70` | Freiräumen am Busplatz zerstört die Remote-Figur, die auf der Host-Figur steht | Schonliste um alle Figuren der `PlayerArray` ergänzen |
| Beide | `0x160C040` | `WP_UI` wartet in `PreConstruct` auf den `GameState` | `ProcessEvent`-Hook hält `PreConstruct` und `Construct` zurück, bis er da ist |
| Client | `0x593DAF3` | `ThreatLevel` nutzt beim Aufräumen `FreezeThreatLevelManager`, den nur der Host hat | Vorhandenes `je` springt ans Funktionsende |
| Client | `0x58393CC` | Figurenkomponente erwartet einen Controller, die fremde Figur hat keinen | Funktion verlassen |
| Beide | Vtables `0x939CE08`, `0x94B46E8`, jeweils `+0x200` | Slots/Items benutzen den UObject-Standard für Netzwerk-Support | Nur für diese Klassen dynamische Netzwerkobjekte erlauben |
| Beide | Vtable `0x94BA3F0`, `+0x3D8` | Inventarkomponente nutzt registrierte Liste statt ihres vorhandenen `ReplicateSubobjects` | In `ReadyForReplication` Bit 6 bei `+0x98` löschen, dann ursprüngliche Funktion aufrufen |
| Client | `0x2A678F0` | `BP_PowerUpCrate_C` spawnt in `BeginPlay` ohne Autoritätsprüfung seine `BP_ChaosDestructable_PowerUp_C`, beim Client kommt dieselbe zusätzlich repliziert an | `BeginDeferredActorSpawnFromClass` gibt beim Client für diese Klasse aus einer Kiste `null` zurück |
| Beide | `0x59EF6D0` | Der Client erzeugt die Karte ohne Missionsdaten und scheitert | Host schreibt Seed, Profil, Größe und Tags in eine Datei, der Client setzt sie vor `StartMapGeneration` und wartet notfalls |
| Client | `0x288B510` | Lokale Parzellen sind `Authority` und erzeugen replizierte Child Actors ein zweites Mal | Während der eigenen Erzeugung gilt der Besitzer für den Aufruf als `SimulatedProxy` |
| Host | `0x584CD82` | Run-Ende nach Niederlage: Handler je Controller ruft `GetLocalPlayer()->...()` ohne Prüfung | Ohne `LocalPlayer` gilt Index 1, also P2 |
| Beide | `0x59EE6A0` | Der Client hat keine Route und damit kein Biom, der PCG-Graph bleibt leer | Host schreibt den Graphen in eine Datei, der Client setzt ihn danach und wartet notfalls |
| Beide | Sekunden-Tick | Jede ISM-Komponente würfelt `InstancingRandomSeed`, die Baumarten weichen ab | Seed aus Mesh und Instanzen setzen, Render-Zustand per `SetStaticMesh` neu |

**Mit der Mod** (`patches`, 04.11.2026): Diese Fixes stehen auch geprüft in `src/Game/*Patches.cpp`, siehe [Patches der Mod](../../../src/Game/README.md#patches).
Ist `DDCoop.dll` geladen und meldet über `DDCoopPatchState`, dass sie ihre Patches gesetzt hat, setzt der Spike seine eigenen nicht.
Ausgelassen werden dann `PatchPlayers`, `PatchClient` (samt Diagnose `NewActor failed`), `PatchCamera`, `PatchInventory`, `PatchTravel`,
`PatchWidgets`, `PatchMapObjects`, der Busplatz aus `PatchTestEnv`, `PatchHealthBars` und `PatchRunEnd`, dazu im Sekunden-Tick Kamera,
Client-Positionen, `SpawnedPlayers` und die zurückgehaltene HUD. `!arm`, `!cam` und `!authmove` melden dann nur, dass die Mod das übernimmt.
Der Phasenbeobachter meldet sich über `DDCoopObserveProcessEvent` bei der Mod an. Seit `missionsstart` lässt der Spike dann auch Karte (`MapGen.cpp`) und PCG (`Pcg.cpp`) aus; die Mod bringt die Werte über ihren Kontrollkanal. Ohne Tunnel der Mod, also mit `-Mod` und `-Tunnel Ip`, `P2p` oder `None`, bekommt der Client dann keine Host-Karte. Testhilfen und Messung bleiben im Spike.
Ohne Mod ändert sich nichts.

Die Zuordnung der Funktionen stammt aus der Disassembly ohne Symbole. Die Kamera ist über Offsets im SDK belegt, die übrigen passen zu den Aufrufern.

Beim Client setzt der Spike die Kamera jede Sekunde, beim Host nimmt er jede Sekunde Client-Positionen an und gleicht `SpawnedPlayers` als echte Menge mit der aktuellen `GameState.PlayerArray` ab. Dazu kommen Befehle für die Datei-Schnittstelle:

| Befehl | Seite | Wirkung |
|---|---|---|
| `!cam` | Client | Setzt die gemeinsame Kamera als ViewTarget, sonst schaut der Client aus der eigenen Figur. Läuft auch automatisch jede Sekunde und setzt nur, wenn das ViewTarget abweicht, siehe [ViewTarget](../../../docs/GAME_ANALYSIS.md#viewtarget-beim-client-kamera-viewtarget). Ohne den Befehl sprang auch die eigene Figur zum Ursprung, vermutlich durch die Sichtbereich-Begrenzung |
| `!authmove` | Host | Nimmt die Position von Remote-Figuren vom Client an |
| `!p2 on`, `!p2 status`, `!p2 joined` | Host | Diagnoseprobe für Remote-P2; standardmäßig aus, **kein fertiger UI-/Inventar-Sync**. [Ablauf, Rücksetzen und Grenzen](RemoteP2.md) |
| `!trace` | Beide | Zählt Client- und Server-RPCs der Spielfigur |
| `!wave` | Host | Startet die Gegner-Spawner |
| `!flow` | Beide | Protokolliert die ersten Aufrufe der Run-Funktionen |
| `!fx`, `!fx dump`, `!fx stack <Text>`, `!fx fuel [n]`, `!fx pull <Klassenteil> [n]` | Beide | Zählt jeden gestarteten FMOD-Sound mit Ereignispfad (Delay-Import `EventInstance::start` der Exe) und Sound-, Niagara- und Pool-Aufrufe aus Blueprints je Aufrufer. `dump` schreibt die Zähler ins Spike-Log und setzt sie zurück, `stack` zählt Sounds mit `<Text>` im Pfad zusätzlich je Aufrufkette (RVAs). Vergleich Host gegen Client, siehe [Sound und Effekte beim Client](../../../docs/GAME_ANALYSIS.md#sound-und-effekte-beim-client-vfx-sound). Sounds, die C++ ohne `start` abspielt, und Effekte aus C++ fehlen. Testhilfen ohne Maus und Tastatur (`opt-waffen-sound`, `opt-item-sound`): `fuel` ruft beim Host `DropRequiredFuel` an der ersten Spielerfigur auf (benötigter Tank plus `n` Kanister), `pull` setzt bis zu `n` sichtbare Actors mit dem Klassenteil im Namen neben die erste Figur (nur in dieser Instanz) |
| `!arm` | Host | Schaltet die Umleitung der Reise scharf |
| `!finish` | Host | Ruft `FinishMission` auf |
| `!cheats` | Host | Legt einen `UDDCheatManager` an, danach wirken die `DD_`-Befehle des Spiels |
| `!give <ItemData> [Rarity] [broken]` | Host | Legt jedem Spielerinventar das Item über `CreateAndAddItem` an, mit `broken` kaputt; es repliziert wie jede Vergabe (Testhilfe `werkbank-client`, `lager-handel-client`) |
| `!load <Pfad>` | Beide | Lädt ein Asset, etwa eine Blueprint-Klasse für einen SDK-Dump |
| `!notutorial` | Beide | Meldet den Stand der Tutorial-Sperre. Der Spike schaltet die Hinweise ohnehin auf beiden Seiten selbst ab, sie halten sonst die Mission an |
| `!round <n>` | Host | Ruft `TriggerEndRound` auf, Wert wie `ERoundChangeReason` |
| `!phase status\|scan\|filter on\|filter off\|stop\|auto` | Beide | Diagnose des Phasenadapters, siehe [Rundenphasen](RoundPhaseRuntime.md#diagnosebefehle) |

`Mission.cpp` lehnt `!finish` und `!round` auf einem verbundenen Client ab. Das sind Diagnosebefehle für den Host, kein UI-Sync.

### Untersuchung `rundenende`: noch keine Client-Phasenanzeige

Build `26187885`, ein PC, zwei Loopback-Instanzen mit getrennten Test-Spielständen, ausschliesslich `Start-Session.ps1` und CrashGuard, nie `-unattended`. Branch basiert auf `origin/main`, nicht auf einem ungemergten anderen Task.

**Laufzeittest:** Über Menü/Ring/Routenkarte in die Mission. Host-`!finish`, Fortsetzen, Lager bestätigen: Host zeigt Rundenübersicht → Lager → Routenkarte, Client bleibt in Missionsansicht. Host-Route hat eine Mission und Index 0, Client keine und Index -1. `LastRoundChangeReason` wechselt nur beim Host auf `NewDay`; Sekunden stehen danach auf beiden Seiten bei 53. Keine Weiterreise mit dem bekannten Inventarfehler versucht.

**Untersuchte Ansätze:**

1. Bestehende Ereignisse/RPCs benutzen: `FinishMission`, `TriggerEndRound`, `SetFreezeTime` sind keine RPCs, die Endphasen-Delegates lokal. Es gibt darüber keine automatische Phasen-Nachricht.
2. Den Ablauf beim Client nachspielen: verworfen, weil dort Route/Run-Persistenz fehlen und laut Architektur nur Anzeige laufen darf. Als sichere Befehlsgrenze werden Client-`!finish` und Client-`!round` nun abgewiesen. Nach Neubau in separater geschützter Sitzung beide Befehle negativ getestet: passende Logs, Client-Grund bleibt `NoReason`, beide weiter in MetaBase. Der unveränderte Host-`!finish`-Pfad wurde zuvor bis zur Routenkarte getestet.
3. Den Zustand aus offenen UI-Objekten übernehmen: `AnimationContainer` war für die beobachteten Panels nicht lesbar (`null`/unbekannt), GameInstance-Widgets können Karten überleben. Präsenz eines Panelobjekts ist kein verlässliches Signal und wurde nicht als Sync implementiert.

Lesendes Diagnosewerkzeug:

```text
py tools/spike/roundstate.py <host-pid> <client-pid>
```

Es zeigt tatsächliche Route-Längen/Indizes, Grund, Tag, Sekunden und Panelbeobachtungen. `scope=game_instance` oder `unresolved` und `open=null` dürfen nicht als aktuelle sichtbare Phase interpretiert werden. Rohdaten bleiben unter `research/local/`.

**Ergebnis offen:** Ein eigener Host-geführter, versionierter und sequenzierter Phasenvertrag mit rein anzeigendem Client ist noch zu implementieren. Weder Client-Anzeige noch Eingabesperre oder deren Aufhebung beim Wechsel/Trennen sind getestet. `Mission.cpp` besitzt aktuell keinen gemeinsamen Patch-/Tick-Einstieg; andere Spike-Dateien wurden für diesen Task nicht geändert. MSVC-Build, Python-Syntaxprüfung und die beiden Client-Befehlsgrenzen sind geprüft, kein vollständiger Run oder Internet-Test. TODO bleibt offen.

## SpawnedPlayers: echte Mengeneinträge (`spawnedplayers`)

Stand 03.11.2026, Build `26187885`. `Players.cpp` ersetzt keine Schlüssel mehr direkt. Es entfernt veraltete, durch GC auf null gesetzte und doppelte belegte Einträge mit korrekt ausgehängter Hash-Verkettung und dem nativen SparseArray-Entferner. Neue aktuelle Controller kommen über denselben nativen TSet-Add-Helfer hinein, den die Registrierung des Spiels verwendet. Der vollständige Registrierungsweg wird **nicht** erneut aufgerufen: Er würde zusätzlich Spiel-Delegates auslösen.

Die Allokationsbits entscheiden über freie Plätze, nicht der Inhalt des ersten Zeigers. Entfernen nutzt den gespeicherten Bucket: Ein durch GC genullter Schlüssel hat nicht mehr seinen ursprünglichen Hash. Vor Änderungen werden Layout, Belegungszahl und alle Hash-Ketten geprüft; bei unvollständiger PlayerArray wird gewartet. Keine Controller- oder GameMode-Zeiger werden über Weltwechsel gespeichert.

**Unmittelbarer Anschluss (03.11., noch ohne Laufzeittest):** Der native Registrierungsweg `0x58FB020` wird mit einem bytegeprüften Trampolin umschlossen. Erst läuft das Original unverändert, danach der Mengenabgleich, sofern das übergebene GameMode zur aktuellen Welt gehört. Kein zusätzlicher Delegate, kein Registrierungs-Replay. Wichtig: Ein Abgleich *vor* dem Original könnte dessen Contains-Prüfung erfolgreich machen und damit den legitimen Add/Broadcast-Zweig unterdrücken. Der Sekundentick bleibt als Rückfallebene, besonders bei einer noch unvollständigen PlayerArray und beim Trennen.

Statische Grundlage: Vorliegende lokale Disassembly, Preferred Image Base `0x140000000`. `0x58FB020` sichert `rbx/rsi`, übernimmt `rdx` als Controller und `rcx` als GameMode, prüft die Controller-Klasse, sucht den Schlüssel im Set bei `GameMode+0x388`, ruft bei Fehlen `0x58F9C50`, dann `0x57BD5E0` (Callsite `0x58FB11D`) und schließlich den Broadcast-Helfer `0x24E9410` mit `GameMode+0x340` (Callsite `0x58FB133`). Vorhandene direkte Aufrufer: `0x58D9903` innerhalb der PlayerArray-Schleife ab `0x58D98CD` und Tailcall `0x58F9085` nach einem Basisaufruf bei `0x58F9070`. Deren Engine-Lifecycle-Namen und eine vollständige virtuelle Aufrufermenge sind mit dieser Disassembly **nicht** belegt. Deshalb kein erfundener HandleSeamlessTravelPlayer-/Travel-Ende-Hook und noch kein Vollständigkeitsnachweis für spätere Controller-Ersetzungen. Der Timer reicht dafür nicht als Beweis: Zwischen zwei WM_TIMER-Nachrichten können Spawner/Kamera bereits laufen; auch die maximale Zustellverzögerung ist nicht auf eine Sekunde begrenzt.

**Abnahme durch den Hauptagenten:** Neue DLL bauen, nur über den dokumentierten Sitzungsweg testen. In beiden Logs `Patch: aktiv` und keine Byte-Abweichung verlangen. Im Hostlog muss nach Beitritt und nach *jedem* Weltwechsel ein `SpawnedPlayers: source=registration ... roster=2 valid=1` stehen; ein späteres `source=timer ... added=1`/`removed=1` belegt dagegen eine noch offene Lifecycle-Lücke. Anschließend `players.py <host-pid>`: `hash_valid=true`, `roster_equal=true`, zwei aktuelle Controller, keine alten Controller. Menü → MetaBase → Mission und mindestens zwei weitere Missionswechsel, außerdem Mission → MetaBase, Trennen (Audit mit `--expected-players 1`) und erneuter Beitritt prüfen. Broadcast-Zähler/Breakpoint bei `0x58FB133` bei Bedarf extern erfassen: Der Hook darf gegenüber dem Originalaufruf keinen weiteren Broadcast erzeugen. Ein reiner Audit nach dem Sekundentick beweist die unmittelbare Abdeckung nicht. Für den strikten Nachweis einen separaten Testbuild ohne den Aufruf `RepairSpawnedPlayers(World)` in `Main.cpp` verwenden (nicht dauerhaft entfernen); gleiche Travel-Folge muss mit ausschließlich `source=registration` konsistent bleiben. Falls dabei die Menge zurückbleibt: P0 offen lassen und den tatsächlich ausgeführten Controller-Ersetzungsweg separat statisch/dynamisch erfassen. Vollständiger Run bleibt zusätzlich erforderlich.

Prüfen ohne Spiel/SDK: `powershell -File tests/Run-SpawnedPlayers.ps1`. Synthetische Fälle: freie Plätze mit Datenmüll, belegter Null-Schlüssel, Kollisionskette, Kopf-/Endentfernung, doppelte Schlüssel, ungültige Indizes, Zyklen und falsche Buckets. `py tools/spike/players.py <host-pid>` prüft zusätzlich tatsächliche Pointer-Hashes und genaue Übereinstimmung mit den aktuellen Controllern; Exitcode 0 nur bei konsistenter Menge und standardmäßig zwei Spielern (`--expected-players 1` für eine absichtliche Einzelspieler-/Trennungsmessung). Kein atomarer Laufzeit-Snapshot.

**Offline-Abnahme des neuen Hooks:** `tests/Run-SpawnedPlayers.ps1` (C++ mit `/W4 /WX`, Mengentests und sieben Python-Audit-Tests) sowie `tools/spike/Build-Spike.ps1` erfolgreich. Diese Tests prüfen keine Engine-Callback-Ausführung.
### Abnahme auf der echten Route (`spawnedplayers-lauf`)

Stand 03.11.2026, Build `26187885`, ein PC über Loopback, nur `Start-Session.ps1` mit CrashGuard. Route über das Spielmenü mit `Start-Run.ps1 -Seed 20795` und `Travel-Day.ps1 -Stay`, Tutorials mit `!notutorial` aus. Zwei Läufe: MetaBase → Tag 1 → Tag 2, dann mit Diagnose MetaBase → Tag 1 → Trennen → erneuter Beitritt. Die Log-Zeile nennt jetzt, welche Seite sich geändert hat: `added:local` oder `added:remote` (der Controller hat eine `NetConnection`), `removed:alive` oder `removed:null`.

| Schritt | Hostlog | `players.py` |
|---|---|---|
| Beitritt in der MetaBase | `source=registration ... roster=2 valid=1`, kein Timer | konsistent, 2 Controller |
| MetaBase → Tag 1 | `registration ... added=1 ... added:local`, danach `source=timer removed=1 added=1 ... removed:alive added:remote` | konsistent, erst nach dem Timer |
| Tag 1 → Tag 2 (erster Lauf, ohne Diagnose) | gleiches Muster wie Tag 1 | konsistent, erst nach dem Timer |
| Trennen (`disconnect` beim Client) | keine Zeile, das Spiel entfernt den Eintrag selbst | konsistent mit `--expected-players 1` |
| Erneuter Beitritt in der Mission | `source=registration ... roster=2 valid=1`, kein Timer | konsistent, 2 Controller |

Kein Absturz, die Inventare waren nach jeder Ankunft vollständig und gleich.

- **[Belegt]** Beitritt und erneuter Beitritt laufen vollständig über den Registrierungshook.
- **[Belegt]** Nach jedem Missionswechsel bleibt eine Lücke: Die Registrierung läuft, solange der Client noch seinen alten Controller hat. Danach ersetzt der Host ihn durch einen neuen, ohne `0x58FB020` zu rufen. Bis zum nächsten Sekunden-Tick steht der alte, noch lebende Controller in der Menge und der neue fehlt. Der Sekunden-Abgleich ist also weiter nötig.
- **[Vermutung]** Der Austausch ist `HandleSeamlessTravelPlayer` der Engine, wenn der Client die neue Welt geladen hat. Nicht geprüft.
- **Nicht gemacht:** Mission → MetaBase, dort stürzt der Host bekannt bei `0x57CC398` ab. Der strenge Testbuild ohne `RepairSpawnedPlayers` ist nicht nötig, das Ergebnis zeigt das Log schon: Der neue Client-Controller käme nie in die Menge. Kein Broadcast-Zähler bei `0x58FB133`.

Folgeaufgabe `controller-tausch`: den Austauschweg finden und dort abgleichen. Ob der alte Controller in dieser Sekunde etwas stört, etwa beim Gegner-Spawn oder bei der Kamera, ist nicht geprüft. Der Spawn-Schutz bei `0x58CEDC7` greift nur bei `null`-Einträgen.

### Controller-Tausch nach dem Missionswechsel (`controller-tausch`)

Stand 04.11.2026, Build `26187885`. Der Host ersetzt nach einem Missionswechsel die Controller, wenn ein Spieler die neue Welt geladen hat. Die Log-Texte der Engine dazu fehlen im Shipping-Build. Am Ende des Tauschs ruft die Engine aber das Blueprint-Ereignis `K2_OnSwapPlayerControllers(OldPC, NewPC)` am GameMode auf, und das läuft immer über `ProcessEvent`.

`Players.cpp` tauscht deshalb `ProcessEvent` in der vtable von `Default__DDMissionGameMode`, die Blueprint-Unterklassen teilen sie. Nach dem Original entfernt der Spike `OldPC` aus `SpawnedPlayers` und fügt `NewPC` mit dem nativen Add-Helfer ein, sofern er noch fehlt. Einen vollen Abgleich über die `PlayerArray` macht er hier nicht, sie kann in diesem Moment noch unvollständig sein. Log: `SpawnedPlayers: source=swap ...`.

Test: ein PC über Loopback, `Start-Session.ps1` mit CrashGuard, Route über das Spielmenü mit Seed `20795`, MetaBase → Tag 1 → Tag 2.

| Wechsel | Hostlog | `players.py` |
|---|---|---|
| MetaBase → Tag 1 | `swap ... added:local`, zweimal `registration ... roster=2`, `swap removed=1 added=1 ... added:remote` | konsistent |
| Tag 1 → Tag 2 | gleiches Muster | konsistent |

- **[Belegt]** Im ganzen Lauf gab es keine Zeile `source=timer`. Der Sekunden-Abgleich bleibt als Rückfallebene im Code, hatte aber nichts mehr zu tun. Kein Absturz, die Inventare waren nach jeder Ankunft vollständig und gleich.
- **[Belegt]** Auch der Host-Controller wird nach dem Missionswechsel getauscht, sein Tausch kommt vor der Registrierung.
- **[Vermutung]** Ein Delegate-Broadcast des Spiels geht dabei nicht verloren: Schon vorher kamen beide neuen Controller nicht über den nativen Registrierungsweg in die Menge, sondern über den Spike. Gezählt ist das nicht.
- Nicht geprüft: Mission → MetaBase, dort stürzt der Host bekannt bei `0x57CC398` ab. Trennen und erneuter Beitritt sind unverändert, siehe oben.

**Historische Laufzeit, vor dem neuen Registrierungshook:** Zwei Instanzen/Loopback, getrennte Test-Spielstände, CrashGuard. Mit diesem Branch nach MetaBase und zwei direkten Missionstravels jeweils zwei aktuelle Controller, konsistente Allokationsbits und Hash-Verkettung. Ein weiterer Wechsel in die MetaBase reproduzierte den bekannten Host-Callback-Absturz `0x57CC398`; die korrekte Menge allein behebt ihn nicht. Zusätzlicher Integrationstest mit dem Timer-Lebensdauerfix aus `tag3` in einer nur lokal zusammengesetzten DLL: Start in der Mission und zwei weitere Missionstravels verbunden, jeweils `hash_valid=true`, `roster_equal=true`, zwei belegte Plätze und identische vollständige Inventare beim letzten Vergleich. Beim nächsten Wechsel neuer Client-Absturz `0x178BA90`; Host bleibt aktiv. Nach der Trennung enthält die Menge korrekt nur noch den Host (ein belegter/ein freier Platz, gültiger Hash). Anschließend isolierten Branch erneut gebaut und Start in die Mission mit dem finalen GameMode-Klassenfilter geprüft; keine Menü-Layoutwarnungen mehr. Kein Nachweis für Remote-P2-/Coop-Subsystem-Integration, Internet oder vollständigen Run. Der Gegner-Spawn-Schutz während des Übergangs bleibt bestehen.

## Spike E: Steam-Tunnel (`tunnel`)

Stand 04.11.2026, Build `26187885`. `Tunnel.cpp` leitet den UDP-Verkehr des `IpNetDriver` unverändert über `ISteamNetworkingSockets` weiter. Das Spiel selbst merkt davon nichts:

```text
Client-Spiel ─ UDP 127.0.0.1:7778 ─ Tunnel ─ Steam Networking ─ Tunnel ─ UDP 127.0.0.1:7777 ─ Host-Spiel
```

- **[Belegt]** Das Spiel bringt die Steamworks-Laufzeit 1.64 mit (`Engine/Binaries/ThirdParty/Steamworks/Steamv164`) und initialisiert Steam selbst. Der Spike holt sich die Funktionen der flachen C-Schnittstelle (`SteamAPI_ISteamNetworkingSockets_*`, Schnittstelle `v012`) per `GetProcAddress` aus der geladenen `steam_api64.dll`. Ein Steamworks-SDK ist nicht nötig, die wenigen Strukturen sind im Spike nachgebildet und mit `static_assert` auf ihre Größe geprüft.
- Der Tunnel läuft in einem eigenen Thread mit `RunCallbacks`, Statusänderungen kommen über den Callback in der Verbindungsoption, Pakete gehen unzuverlässig ohne Nagle (`UnreliableNoNagle`), die Zuverlässigkeit macht die Engine selbst.
- Host: `!tunnel host` lauscht per P2P (virtueller Port 0) und per IP auf `127.0.0.1:7790` und nimmt jede eingehende Verbindung an. Je Verbindung öffnet er einen eigenen UDP-Socket zum Spiel, der Host sieht also jeden Client unter einer eigenen Adresse.
- Client: `!tunnel join <steamid>` (P2P), `!tunnel join self` (P2P zum eigenen Konto) oder `!tunnel join ip`, danach `open 127.0.0.1:7778`. `Start-Session.ps1 -Tunnel Ip` macht das automatisch.
- Alle 5 s schreibt der Tunnel `Tunnel: Verbindung ... Pakete/B, verworfen, Ping, Qualitaet` ins Log.

Test: ein PC, ein Steam-Konto, `Start-Session.ps1 -Tunnel ...` mit CrashGuard, Route über das Spielmenü mit Seed `20795`.

| Schritt | Ergebnis |
|---|---|
| `-Tunnel P2p`: P2P zur eigenen SteamID | **[Belegt]** Steam lehnt ab, `ConnectP2P` liefert Handle `0`. Auf einem PC mit einem Konto ist P2P nicht testbar |
| `-Tunnel Ip`: Verbindung über Steam Networking per IP | Aufgebaut und bei Steam angemeldet, beide Seiten nennen die SteamID der Gegenseite. Ping 0 ms, Qualität 1,0, nichts verworfen |
| MetaBase über den Tunnel | 2 Spieler auf beiden Seiten, Client `role: client` |
| MetaBase → Tag 1 → Tag 2 | Beide Kartenwechsel über dieselbe Steam-Verbindung, Inventare jeweils vollständig und gleich |
| Bewegung an Tag 2 | Client- und Host-Figur laufen, beide Seiten melden danach dieselben Positionen |
| Kampf und Schaden | Schüsse des Clients zählen beim Host (168 Treffer, 56 Kills), 30 Schaden am Client ergeben auf beiden Seiten 70 HP und 70 % in der Leiste |
| Verkehr gesamt | rund 22 000 Pakete je Richtung, Host → Client rund 4,1 MB, Client → Host rund 1,3 MB. Kein Absturz |

- Nicht geprüft: Abbruch und Neuaufbau des Tunnels, mehr als ein Client, Mission → MetaBase.

### P2P auf zwei PCs (`tunnel-p2p`)

Stand 04.11.2026, Build `26187885`, `main` auf `0607feb`. Zwei PCs, zwei Steam-Konten, je PC eine Instanz mit Spike und CrashGuard,
`Test-TwoPc.ps1 -Role Host` und `-Role Client -Peer <SteamID>`, siehe [Test auf zwei PCs](../../../docs/TESTING.md#test-auf-zwei-pcs).

| Schritt | Ergebnis |
|---|---|
| `!tunnel join <steamid>` zum anderen Konto | **[Belegt]** `ConnectP2P` baut die Verbindung auf, Steam vermittelt ohne Portfreigabe und ohne Freundschaft |
| MetaBase über den Tunnel | 2 Spieler, Host `clients: 1`, Client `role: client` |
| Run mit Seed `20795`, Mission an Tag 1 | Beide in `DDDynamic_Map` mit 2 Spielern, 120 s ohne Absturz auf beiden Seiten |
| Ping | Host 39 / 41 / 44 ms (min / Median / max, 75 Messungen), Client 39 / 41 / 43 ms (66 Messungen) |
| Verkehr | Host rund 24 000 Pakete gesendet und 22 000 empfangen, verworfen 0, Qualität 1,00 auf beiden Seiten |
| Route | **[Belegt]** Nicht direkt, sondern über ein Steam-Relay in Frankfurt (`fra`), Ersatz Amsterdam (`ams`). Der Client meldet 25 ms zum Relay |

- **[Belegt]** Der Tunnel verhält sich über P2P wie per IP, nur der Verbindungsaufbau unterscheidet sich. Die Vermutung aus dem Ein-PC-Test stimmt.
- Auf der Client-Seite war keine Änderung nötig. Auf dem Host-PC öffnete sich das Spielfenster mit 1215 x 683 statt 800 x 450, darum
  trafen die Klicks von `Start-Run.ps1` daneben. `Game-Input.ps1` setzt das Fenster seitdem vor jedem Klick, jeder Taste und jedem Bild auf 800 x 450.
- Nicht geprüft: Verbindung ohne Relay (direkt), Paketverlust unter Last, Abbruch und Neuaufbau über P2P, Tag 2 und weiter über zwei PCs.
- **[Belegt]** In der Mission fielen die Figuren durch den Boden. Der Client hatte keine Karte: `MapGen.cpp` liest die Host-Karte aus `mapgen.txt`, die es nur auf dem Host-PC gab. Das Log endete bei `MapGen: Client wartet auf die Host-Karte`. Der Bericht von `Test-TwoPc.ps1` prüft das nicht. Behoben mit `karte-tunnel`, siehe unten.

### Host-Karte über den Tunnel (`karte-tunnel`)

Stand 04.11.2026, Build `26187885`. Der Tunnel trägt neben dem Spielverkehr die Host-Karte, damit der Client sie auch auf einem anderen PC bekommt.

- Jede Steam-Nachricht beginnt mit einem Typ-Byte: `0` Spielpaket, `1` Host-Karte. **Beide PCs brauchen deshalb denselben Spike-Stand**, ein alter Spike auf der Gegenseite verwirft die Pakete (`verworfen` im Log steigt).
- Host-Tunnel: prüft alle 250 ms, ob `mapgen.txt` neu geschrieben wurde, und schickt den Inhalt zuverlässig (`Reliable`) an jeden verbundenen Client. Eine Datei, die schon vor `!tunnel host` da war, gilt als alt und geht nicht raus. Werte älter als 120 s gehen auch an neue Verbindungen nicht mehr.
- Client-Tunnel: legt den Inhalt als eigene `mapgen.txt` ab, `MapGen.cpp` liest sie wie bisher. Steht dieselbe Fassung schon frisch dort, weil Host und Client auf einem PC laufen, bleibt die Datei unberührt.
- Log: `Tunnel: Host-Karte an Verbindung ... geschickt` beim Host, `Tunnel: Host-Karte empfangen ...` beim Client.

| Test | Ergebnis |
|---|---|
| Ein PC, `-Tunnel Ip`, Seed `20795`, Tag 1 | **[Belegt]** Host schickt die Karte, Client empfängt sie (`schon aktuell`), übernimmt Seed und Profil, Inventare gleich, 0 verworfene Pakete |
| Zwei PCs, P2P | Nicht mehr einzeln geprüft, abgelöst (siehe unten) |

**Abschluss 07.11.2026.** Seit `missionsstart` überträgt die Mod die Host-Karte selbst über ihren Kontrollkanal (`0x400`, siehe
[Gleiche Karte in der Mod](../../../docs/GAME_ANALYSIS.md#gleiche-karte-in-der-mod-missionsstart)); mit der Mod lässt sich der Spike-Tunnel
nicht mehr nutzen. Beim [Live-Test auf zwei PCs am 06.11.2026](../../../docs/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-06112026-livetest-befunde)
über das Steam-Relay standen laut Nutzer beide Figuren normal auf dem Boden, beide sahen dieselbe Karte, Tag 1 und 2 wurden gespielt
(**[Gemeldet]**). Mod-Logs mit „Karte: Werte … an den Client gesendet“ aus diesem Lauf lagen bei der Auswertung nicht vor. Die
Abnahme über zwei PCs gilt damit als erfüllt; `integration-g2` prüft dasselbe über zehn Minuten erneut.

## Bekannte Abweichungen

### Rundenphasen: Codec-/Empfängerkern (`rundenende`)

`RoundPhase.h` ist eine noch **nicht an den Spielbetrieb angeschlossene** Teilumsetzung der rein anzeigenden Phasenübertragung. Er enthält weder Transport noch Host-Ereignishooks noch UI-/Eingabe-Engineaufrufe. `Presentation` hält Snapshot und Sperrentscheidung atomar auf Modellebene zusammen. Neue Weltbindungen warten zunächst gesperrt auf einen passenden Snapshot; alte Nachrichten und unbestätigte Generationswechsel werden abgelehnt. Disconnect löscht den eigenen Zustand.

`powershell -File tests/Run-RoundPhase.ps1` baut und prüft den Kern ohne Spiel/SDK mit C++20 `/W4 /WX`. Testumfang und offene Integration stehen in der [Untersuchung](../../../docs/GAME_ANALYSIS.md#rundenende-befund-und-offene-phasenübertragung-rundenende). Die Schema-Version 1 gehört **nur zum Spike**, sie ersetzt weder das zentrale Produktionsprotokoll noch dessen Handshake/Authentisierung. `Session` muss pro neuer Verbindung frisch vergeben werden; die Bindung muss aus verifiziertem Transport-/Travel-Zustand stammen, nicht aus beliebigen Paketdaten.

**Ergänzung 03.11.2026:** `Mission.cpp` enthält jetzt einen explizit aktivierbaren lokalen Testadapter: Publisher/Heartbeat, atomarer Dateitransport, Timeout, Textanzeige und Win32-Nachrichtenfilter. Tatsächlicher Adapter und Zwei-Testprozess-Transport ohne Spiel geprüft; `Main.cpp` verbindet Befehl, Tick und Filter, `!finish`/`!round` liefern ihre Hostaufrufe. Der globale Beobachter für normale Menü-/Busereignisse ist **noch nicht angeschlossen**; Lager/Route benötigen weiterhin manuelle Diagnosemarkierung, vollständiger UE-Inputblock und sichtbarer Laufzeitnachweis fehlen. Integration, Bedienung und Grenzen: [RoundPhaseRuntime.md](RoundPhaseRuntime.md). P0 bleibt offen.

**Abschluss `phasenadapter`, 04.11.2026:** Der Adapter läuft jetzt automatisch. Der Host erkennt Rundenübersicht, Lager und Routenkarte an `AbstractPanel.OnOpenStateChanged` der Panels und an `FinishMission`/`TriggerEndRound`. Der Client folgt, zeigt ein Banner und sperrt die Eingabe über `DisableInput` und den Nachrichtenfilter. Im Spiel an der ersten Route und an drei Rundenenden geprüft. Ablauf, Tests und Grenzen: [RoundPhaseRuntime.md](RoundPhaseRuntime.md).

### Weitere Abweichungen

Diese Punkte gehören zu Thema 6, sie stoppen die Technik nicht:

- `MovementSpeed` und Beschleunigung weichen ab: Host 819 und 2866, Client 600 und 2048. Upgrades kommen beim Client also nicht an.
- Gegner-Zähler weichen ab, zuletzt 30 beim Host und 48 beim Client. Vermutlich bleiben beim Client tote Gegner als Objekte stehen, geprüft ist das nicht.
- Blut- und Treffereffekte sowie die XP-Leiste erscheinen nur beim Host.
- Tutorial-Hinweise laufen auf beiden Seiten getrennt.
- Die direkt geladene Mission startet keine Gegner von selbst. Über das Spielmenü gestartet tut sie das.
- Der Client stürzte einmal bei `0x5A17ACC` ab (Lesen von `0x2C8`). Das war kein Tick: Der Bewegungs-Restrictor der Kamera hatte keinen `MissionGameState` und stürzt bei der ersten Bewegungseingabe ab. Mit dem Feld auf null nachgestellt, siehe [Restrictor](../../../docs/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz).
- An Tag 2 zeigt der Host ein anderes Figurenporträt mit violetter Leiste. Die Ursache ist offen.
- Ob die Figuren automatisch feuern, ist unklar. Die Host-Figur schoss, ohne dass ihr Fenster Eingaben bekam.

## Inventar: Ursache, Teilfix und verbleibende Grenze

Historisch wanderten die Client-Abstürze nach jedem Null-Schutz zur nächsten Schleife: `0x596362D`, `0x5A7E063`, `0x5965663`, `0x59661C3`. Alle drei Schutzsprünge sind jetzt entfernt; es gibt keine weiteren Einzelprüfungen in den Spielschleifen.

**Messung:** Schon an Tag 1 fehlten auf dem Client beide Inventare vollständig (je acht Null-Slots und ein Null-Item), während die Objekte auf dem Host existierten. Es war kein bloss kurzzeitig unaufgelöster Zustand. Die Spielklasse hat einen klassischen `ReplicateSubobjects`-Override, setzt aber die registrierte Unterobjektliste ein. Dazu fehlt den Item-/Slot-Klassen ausdrücklicher Netzwerk-Support.

**Versuche:**

1. Factory-Trace als erste Änderung; danach Menüweg erneut erfolgreich geprüft (Ring, bei frischem Stand Tutorial-Erklärung, Routenkarte, Mission mit automatisch startenden Gegnern). Kontrolllauf reproduzierte Client-Crash `0x59661C3` beim nächsten Tag.
2. Nur Netzwerk-Support in beiden Unterobjektklassen: weiterhin Null-Verweise.
3. Zusätzlich Umschaltung auf klassischen Pfad in den Factories: Tag 1 vollständig, Tag 2 wieder Crash. Beim Kopieren der Inventare laufen die Factories nicht erneut.
4. Endstand: Umschaltung für jede Komponente in `ReadyForReplication`, Netzwerk-Support nur für die zwei Unterobjektklassen. Keine Outer-Änderung; keine zusätzliche Synchronisierung oder leere Ersatzinventare.

**Getestet:** Build `26187885`, ein PC, Host/Client über Loopback mit getrennten Test-Spielständen, nur `Start-Session.ps1`/CrashGuard, ohne `-unattended`. Nach Build des Endstands zweimal unabhängig über das Menü bis Tag 2 gereist, Missionsende jeweils über Host-`!finish`, danach Fortsetzen, Lager bestätigen und den erreichbaren nächsten Routenknoten auswählen. Beide Seiten an Tag 2 verbunden, je Spieler acht gültige Slots und ein gültiges Item. Koordinaten, Belegungszuordnung, ItemData, Pivot, Turns und replizierte Zustandsbits stimmen überein. Vergleich in der zweiten Sitzung auch an Tag 1 und nach zusätzlicher Wartezeit an Tag 2.

```text
py tools/spike/inventory.py <host-pid> <client-pid>
```

Das Werkzeug liest nur Inventare der aktuellen `GameState.PlayerArray`, keine alten PlayerStates oder Blueprint-Vorlagen. Im ruhenden Zustand aufrufen; kein atomarer Snapshot. `complete` und `equal` müssen beide wahr sein. Ein leeres MetaBase-Inventar ist damit kein Beleg für ein funktionierendes Missionsinventar.

**Folgeuntersuchung `tag3`, 03.11.2026:** Der Callback ist dem `HitByEnemyTypeAchievementDetectorComponent` zugeordnet. Dessen BeginPlay speichert einen rohen Component-Zeiger in einem Next-Tick-Timer, EndPlay löscht diesen Timer ursprünglich nicht. Der Spike bewahrt nun den echten TimerHandle auf und räumt einen noch lebenden Timer vor dem ursprünglichen EndPlay ab. Keine zusätzlichen Null-Schutzsprünge; die bisherigen Inventar-Patches bleiben unverändert. Byte-/Vtable-Checks vor dem Einbau. Details, Gegenprobe und Grenzen stehen in der [Analyse](../../../docs/GAME_ANALYSIS.md#tag-3-callback-lebensdauer-des-achievement-detektors-tag3).

**Begrenzter neuer Test:** Start nur über `Start-Session.ps1`/CrashGuard; MetaBase → Mission → Mission → MetaBase verbunden, Inventare nach dem zweiten Missionstravel vollständig und gleich. Timer-Cleanup wiederholt im Log nachgewiesen. Beim anschließenden Missionstravel überlebte der Host; ein anderer Client-Absturz `0x178BA90` (Lesen `0x8C`) beendet den Test. Direkte Reisen, **kein** Menü-/Tagesablauf und kein vollständiger Run. Zusätzlicher lokaler Integrationstest mit `spawnedplayers`, nicht Teil dieses Branch-Builds: Start in Mission plus zwei weitere Missionstravels verbunden und Inventare vollständig/gleich; beim nächsten Wechsel erneut Client `0x178BA90`, Host bleibt aktiv. Damit ist auch der Folgefehler reproduzierbar.

**Echte Route, Übergabestand:** Der Host-Absturz bei der Reise auf Tag 3 kam von einem Kartengenerator-Lambda mit rohem Zeiger auf den alten Generator. `Inventory.cpp` lässt diesen Schritt aus, wenn der Generator nicht mehr zur aktuellen Welt gehört. `Client.cpp` wartet in der Exp-Aggregation auf fehlende AttributeSets (`0x178BA90`) und fängt zwei Client-Abstürze an Tag 4 ab. Zweimal Tag 3 über die echte Route erreicht, Inventare an Tag 1 bis 3 vollständig und gleich. Offen ist ein dritter Client-Absturz an Tag 4 (`0x4B68195` über `0x59A4B90`), Folgeaufgabe `tageswechsel`. Einzelheiten in der [Analyse](../../../docs/GAME_ANALYSIS.md#echte-route-bis-tag-4-tag3-übergabestand).

**Tageswechsel, Übergabestand:** `Client.cpp` prüft jetzt am Einstieg der AttributeSet-Suche `0x4B68180`, ob ein AbilitySystem da ist. Das ersetzt den Schutz nur nach `OnRep_DayNum`, der dritte Absturz an Tag 4 trat in drei Läufen nicht mehr auf. Die eigentliche Ursache liegt tiefer: Ab Tag 4 fehlen dem Client die PlayerStates, weil in seinem NetGUID-Cache die Einträge für Archetyp und Paket der PlayerState-Klasse fehlen. `SerializeNewActor` meldet das jetzt als `NewActor failed`. RootSet für Klassen, ein Patch am 90-s-Ablauf in `CleanReferences` und ein `servertravel`-Schnelltest mit GC in jedem Frame halfen nicht, Einzelheiten in der [Analyse](../../../docs/GAME_ANALYSIS.md#tageswechsel-client-verliert-die-playerstates-tageswechsel-übergabestand). Zuerst kommt `testumgebung`, danach `guidcache`.

**Noch offen:** Fehlende PlayerStates ab Tag 4, Client-Absturz an Tag 5 (`0x56B16D9`) und alles danach, historischer Client-Folgefehler `0x57D080C`. Nicht getestet: Client-Inventarbedienung und RPCs, grössere/wechselnde Inventare, Belohnungen/Item-Effekte, Internet und Paketverlust. `obj gc` wurde historisch gesendet, die tatsächliche GC-Ausführung aber nicht nachgewiesen. Rohmessungen und Absturzberichte bleiben unter `research/local/`.

### Blockaden-Gebietswechsel (`guidcache`, abgeschlossen)

**Test-Godmode:** `tools/spike/Start-Session.ps1 -Build -Godmode` aktiviert die Testhilfe explizit in beiden Instanzen. `!god on` / `!god off` schalten sie pro Instanz; fuer beide Rollen den Befehl an beide PIDs senden. Sie lebt nur im Spike-Prozess, nicht im Spielstand oder der ausgelieferten Mod. Der Game-Thread-Tick schuetzt die aktuellen Spielerfiguren aus dem GameState-Roster erneut nach Reisen, einschliesslich der autoritativen Client-Figur beim Host. Fehlende PlayerStates werden dadurch weder ersetzt noch als sauber gewertet.

Die Hilfe verwendet den spielinternen `State.Invincible`-Tag ueber dessen native Loose-Tag-API. Sie heilt nicht und setzt keine Lebenspunkte. Ein eigener additiver Beitrag wird nicht bei jedem Tick erneut hinzugefuegt und beim Abschalten nur fuer die noch aktuellen, selbst geschuetzten Figuren entfernt; fremde Gameplay-Beitraege werden nicht geloescht. Alte Weltobjekte werden nicht aus gecachten Zeigern aufgerufen. Erste Schadens-Gegenprobe an Tag 5: beide autoritativen Figuren 100 HP mit Schutz, je 95 HP nach demselben 5-Schaden-Befehl ohne Schutz. **Noch offen:** Vollprobe mit intaktem Client-Roster samt Replikation und Wiederholung nach weiteren Kartenwechseln. `tools/spike/Test-Godmode.ps1` prueft EIN/AUS/EIN, protokolliert die HP beider Rollen und laesst den Schutz eingeschaltet; nicht parallel zur Routenautomatisierung ausfuehren.

Auf einer frischen Route mit Seed `20795` Tag 1 bis 4 erneut sauber geprüft, beim Laden von Tag 5 den bekannten Client-Absturz reproduziert. REA/Frida identifizierte den Aufrufer als `HighwayBlockageEventComponent`: Der Client startet seinen eigenen Gegner-/Zielablauf, obwohl der dafür benötigte GameMode nur beim Host existiert. Der gemeldete Zugriff `0x3D0` entsteht aus null + `SpawnedPlayers` (`0x388`) + Feld `0x48` des Mengen-Kopierers, nicht aus einem belegten NetGUID-Verlust in diesem Lauf.

`Client.cpp` schützt jetzt den gemeinsamen `UBlockadeEventComponent::OnLoadingFinished`-Einstieg, bevor dessen verzögerter virtueller Start geplant wird. Auf einem Netzwerk-Client bleibt dieser Host-Ablauf aus; Host/Standalone führen das Original aus. `BlockadeAuthority.h` hat keine Tages-, Karten- oder GUID-Sonderfälle. Die Entscheidung ist ohne Spiel mit `tests/Run-BlockadeAuthority.ps1` prüfbar (auch CI); das ersetzt keine Route-/Endlos-Abnahme. Byteprüfungen sichern Einstieg, reflektierten Aufruf und Timeradapter für Build `26187885` ab.

Die nachfolgende Untersuchung dokumentiert den Weg bis zur akzeptierten Abnahme. `guidcache` ist nach Nutzerfreigabe vom 04.11.2026 abgeschlossen; der Lauf bis Endlos-Tag 13 und zwei Biomwechsel genügt vorerst. Weitere Detailtests sind nicht nötig, neue konkrete Probleme erhalten bei Auftreten ein neues TODO. [Abschluss, Nachweise und Testgrenzen](../../../docs/GAME_ANALYSIS.md#abschluss-und-akzeptierter-testumfang-guidcache).

**Zwischenstand vor der Endlos-Wiederholung:** Ein beschleunigter Run erreichte Tag 1–8 mit sauberen Einstiegskontrollen und gleichen Inventaren. Die Cache-Prüfung benutzt nun den Cache-eigenen Driver samt Rückverweis, weil `GWorld` während Travel-GC null ist. Godmode EIN/AUS/EIN wurde an Tag 5 und nach Reise an Tag 6 einschließlich der Client-HP-Replikation geprüft. Der erste Endlos-Einstieg an Tag 9 stürzte dagegen in `SneakApproachComponent` ab: Die gleiche Ziel-Figur wurde trotz noch fehlendem Mesh wiederverwendet. Daraufhin wurde der Readiness-Fix samt synthetischen Maschinencode-Tests ergänzt.

**Wiederholung mit Sneak-Readiness:** Endlos-Tag 9 überstand dieselbe per Frida beobachtete Null-Mesh-Situation. Anschließend Tag 10/11 und nach bestätigter Belohnung Tag 12 sauber, Tag 13 ebenfalls sauber und Inventare vollständig/gleich. Damit sind zwei weitere Biomwechsel im beschleunigten Test gefahren (Siedlungen → Felder → Wälder). Der spätere Tag-8-Abstandscheck war dagegen auffällig; dies bleibt im Protokoll stehen. Natürlich ausgespielter Bosskampf und neue automatisch gespeicherte Inventarnachweise für Tag 1–4 sind nicht belegt; der Nutzer akzeptiert den vorhandenen Umfang ausdrücklich zum Abschluss. Kein Dauerstabilitätsversprechen und vorerst keine weiteren Detailtests.

Abschließende lokale Prüfungen nach Integration der aktuellen TODO-Struktur: 86 Python-Tests, native Autoritäts-/Godmode-/NetGUID-/Sneak-Tests, Spike-Build, PowerShell-Syntax und TODO-/README-Konsistenz bestanden. Eigene Testsitzung beendet; alle 54 eingefrorenen Originaldateien unverändert.

## Crash-Reporter

Das Spiel enthält den `CrashReportClient` mit einer Upload-Adresse von Pixelsplit. **Mit `-unattended` lädt er Berichte ohne Rückfrage hoch**, inklusive
Minidump und einer Kopie des aktiven Spielstands. So gingen bei diesem Spike zwei Berichte raus.

- `-unattended` niemals verwenden.
- Während der Tests läuft `tools/spike/CrashGuard.ps1`, das `CrashReportClient.exe` sofort beendet. Das hat die weiteren Abstürze abgefangen.
- Ein Override von `DataRouterUrl` in der `Engine.ini` des `UserDir` wirkt nicht. Die Adresse stand trotzdem im Bericht.

### Sperre in Mod und Spike (`crash-reporter`)

Stand: Disassembly und Spieltests am 03.11.2026, Build `26187885`.

- **[Belegt] Einziger Startweg:** Nach einem Absturz schreibt das Spiel den Bericht nach `Saved/Crashes` und baut dann ab `0x145DD6E` die Kommandozeile für `CrashReportClient.exe`. Ist das Spiel unattended oder `bImplicitSend` gesetzt, hängt es ` -Unattended` an, dann lädt der Reporter ohne Rückfrage hoch. Gestartet wird über `FPlatformProcess::CreateProc` (`0x144D740`) und den Import `CreateProcessW`. Der zweite Aufrufer von `CreateProc` öffnet nur URLs.
- **[Belegt] Scheitert der Start,** zeigt das Spiel nur „has crashed and will close“ (ohne `-Unattended`) und beendet sich. Es wird nichts gesendet, der lokale Bericht bleibt.
- **Mod:** `Core::CrashReports::Block()` hängt sich per MinHook in `kernelbase!CreateProcessW` und lässt nur den Start von `CrashReportClient.exe` scheitern. Unabhängig von Spiel-Offsets, aktiv sobald die Mod läuft, also nicht bei `enabled=false` oder unbekanntem Build.
- **Spike:** `CrashReports.cpp` ersetzt den Eintrag `CreateProcessW` in der Importtabelle der Exe. Greift erst ab dem Laden des Spikes, deshalb bleibt `CrashGuard.ps1` die erste Schicht.
- **Gescheiterter erster Versuch im Spike:** Nur der Parameter `lpApplicationName` wurde geprüft. Unreal übergibt dort `null` und setzt das Programm vorne in die Kommandozeile, der Reporter startete. Jetzt zählt das erste Token der Kommandozeile.

Ausgelöst wurde ein echter Spielabsturz: `MissionGameState` des Restrictors auf null, dann eine Bewegungseingabe, siehe [Restrictor](../../../docs/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz). `debug crash` ist im Shipping-Build wirkungslos. Ein Prozess-Wächter prüfte alle 10 ms auf `CrashReportClient`.

| Lauf | Absturz | Reporter-Prozess | Log |
|---|---|---|---|
| Mod, Client | `0x5A17ACC` | keiner | „Start des Crash-Reporters verhindert“ |
| Mod, Host | `0x5A17ACC` | keiner | „Start des Crash-Reporters verhindert“ |
| Ohne Mod, Spike ohne Sperre (Gegenprobe) | `0x5A17ACC` | gestartet, von `CrashGuard` beendet | – |
| Spike mit Sperre, erster Versuch | `0x5A17ACC` | gestartet, von `CrashGuard` beendet | – |
| Spike mit Sperre, korrigiert | `0x5A17ACC` | keiner | „Start des Crash-Reporters verhindert“ |

Nicht abgedeckt: Abstürze vor dem Laden der Mod oder des Spikes, Telemetrie und Bug-Reports (Phase 6), andere Spiel-Builds für den Spike-Patch. Ob sich der Netzwerkverkehr zum Hersteller ganz vermeiden lässt, ist nicht gemessen; ohne Reporter-Prozess gibt es aber keinen Upload-Weg für den Bericht (**[Vermutung]**, das Spiel selbst lädt keine Berichte hoch).
