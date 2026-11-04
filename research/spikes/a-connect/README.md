# Spike A und B: Verbindung und Spieler

Stand: 02.11.2026, Build `26187885`. Wegwerfcode, die Offsets gelten nur für diesen Build.

## Aufbau

- Zwei Instanzen auf einem PC unter demselben Steam-Konto, Verbindung über `127.0.0.1:7777`.
- Die Exe wird direkt gestartet, mit den Umgebungsvariablen `SteamAppId` und `SteamGameId` sowie je einem eigenen `-UserDir=`.
  So bleibt der echte Spielstand unberührt.
- Der Spike wird als DLL in beide Instanzen geladen. Sie führt Konsolenbefehle aus einer Datei auf dem Game Thread aus.
  Der Game Thread wird über eine Nachricht an das Spielfenster erreicht.

Gebaut und gestartet wird über die Skripte in `tools/spike/`, die auch den Crash-Reporter absichern, siehe [Testen](../../../docs/TESTING.md).

## Dateien

Je Thema eine Datei, damit parallele Arbeit nicht in dieselbe Datei läuft. Neue Patches kommen in die passende Datei und werden in deren `Patch…`-Funktion eingehängt.

| Datei | Inhalt |
|---|---|
| `Spike.h` | Gemeinsame Offsets und Schnittstellen |
| `Core.cpp` | Log, Objektsuche, `CodeWriter` für Patches |
| `Main.cpp` | Laden der DLL, Befehls-Schnittstelle, Sekunden-Tick |
| `Players.cpp` | Spawn der Remote-Spieler, `SpawnedPlayers`, clientautoritative Position |
| `Client.cpp` | Client-Stellen ohne Host-Objekte: Refresh, Attribute, Figurenkomponente, `ThreatLevel`, Log für nicht angelegte Actors |
| `Camera.cpp` | Gemeinsame Kamera und Bodenfix |
| `Inventory.cpp` | Netzwerk-Support für Items/Slots und klassischer Unterobjekt-Replikationspfad je neuer Inventarkomponente |
| `Travel.cpp` | Umleitung der Reise in `servertravel`, vorher Zeitanfragen des `UTimescaleManager` abräumen |
| `Widgets.cpp` | Zurückhalten der HUD bis zum `GameState` |
| `MapObjects.cpp` | Client: lokale Kopien replizierter Kartenobjekte unterdrücken |
| `Mission.cpp` | Gegnerwelle, Missionsende, Rundenende |
| `Trace.cpp` | Zähler für RPCs und Run-Funktionen |
| `CrashReports.cpp` | Sperre des Crash-Reporters, zweite Schicht neben `CrashGuard.ps1` |
| `MapGen.cpp` | Client erzeugt die Karte mit den Werten des Hosts, lokale Kopien replizierter Child Actors unterdrückt |
| `Pcg.cpp` | Client erzeugt die PCG-Vegetation mit dem Graphen des Hosts, gleiche Seeds der Instanz-Komponenten |
| `Hits.cpp` | Host: Treffer auf Gegner mit Schütze, Schaden und Lebenspunkten |
| `Cheats.cpp` | Testhilfen: CheatManager für die `DD_`-Befehle, Laden einzelner Assets, Tutorial abschalten |
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
| Beide | `0x59EE6A0` | Der Client hat keine Route und damit kein Biom, der PCG-Graph bleibt leer | Host schreibt den Graphen in eine Datei, der Client setzt ihn danach und wartet notfalls |
| Beide | Sekunden-Tick | Jede ISM-Komponente würfelt `InstancingRandomSeed`, die Baumarten weichen ab | Seed aus Mesh und Instanzen setzen, Render-Zustand per `SetStaticMesh` neu |

Die Zuordnung der Funktionen stammt aus der Disassembly ohne Symbole. Die Kamera ist über Offsets im SDK belegt, die übrigen passen zu den Aufrufern.

Beim Client setzt der Spike die Kamera jede Sekunde, beim Host nimmt er jede Sekunde Client-Positionen an und gleicht `SpawnedPlayers` als echte Menge mit der aktuellen `GameState.PlayerArray` ab. Dazu kommen Befehle für die Datei-Schnittstelle:

| Befehl | Seite | Wirkung |
|---|---|---|
| `!cam` | Client | Setzt die gemeinsame Kamera als ViewTarget, sonst schaut der Client vom Weltursprung aus. Ohne den Befehl sprang auch die eigene Figur zum Ursprung, vermutlich durch die Sichtbereich-Begrenzung |
| `!authmove` | Host | Nimmt die Position von Remote-Figuren vom Client an |
| `!trace` | Beide | Zählt Client- und Server-RPCs der Spielfigur |
| `!wave` | Host | Startet die Gegner-Spawner |
| `!flow` | Beide | Protokolliert die ersten Aufrufe der Run-Funktionen |
| `!arm` | Host | Schaltet die Umleitung der Reise scharf |
| `!finish` | Host | Ruft `FinishMission` auf |
| `!cheats` | Host | Legt einen `UDDCheatManager` an, danach wirken die `DD_`-Befehle des Spiels |
| `!load <Pfad>` | Beide | Lädt ein Asset, etwa eine Blueprint-Klasse für einen SDK-Dump |
| `!notutorial` | Beide | Meldet den Stand der Tutorial-Sperre. Der Spike schaltet die Hinweise ohnehin auf beiden Seiten selbst ab, sie halten sonst die Mission an |
| `!round <n>` | Host | Ruft `TriggerEndRound` auf, Wert wie `ERoundChangeReason` |

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

## Bekannte Abweichungen

### Rundenphasen: Codec-/Empfängerkern (`rundenende`)

`RoundPhase.h` ist eine noch **nicht an den Spielbetrieb angeschlossene** Teilumsetzung der rein anzeigenden Phasenübertragung. Er enthält weder Transport noch Host-Ereignishooks noch UI-/Eingabe-Engineaufrufe. `Presentation` hält Snapshot und Sperrentscheidung atomar auf Modellebene zusammen. Neue Weltbindungen warten zunächst gesperrt auf einen passenden Snapshot; alte Nachrichten und unbestätigte Generationswechsel werden abgelehnt. Disconnect löscht den eigenen Zustand.

`powershell -File tests/Run-RoundPhase.ps1` baut und prüft den Kern ohne Spiel/SDK mit C++20 `/W4 /WX`. Testumfang und offene Integration stehen in der [Untersuchung](../../../docs/GAME_ANALYSIS.md#rundenende-befund-und-offene-phasenübertragung-rundenende). Die Schema-Version 1 gehört **nur zum Spike**, sie ersetzt weder das zentrale Produktionsprotokoll noch dessen Handshake/Authentisierung. `Session` muss pro neuer Verbindung frisch vergeben werden; die Bindung muss aus verifiziertem Transport-/Travel-Zustand stammen, nicht aus beliebigen Paketdaten.

**Ergänzung 03.11.2026:** `Mission.cpp` enthält jetzt einen explizit aktivierbaren lokalen Testadapter: Publisher/Heartbeat, atomarer Dateitransport, Timeout, Textanzeige und Win32-Nachrichtenfilter. Tatsächlicher Adapter und Zwei-Testprozess-Transport ohne Spiel geprüft; `Main.cpp` verbindet Befehl, Tick und Filter, `!finish`/`!round` liefern ihre Hostaufrufe. Der globale Beobachter für normale Menü-/Busereignisse ist **noch nicht angeschlossen**; Lager/Route benötigen weiterhin manuelle Diagnosemarkierung, vollständiger UE-Inputblock und sichtbarer Laufzeitnachweis fehlen. Integration, Bedienung und Grenzen: [RoundPhaseRuntime.md](RoundPhaseRuntime.md). P0 bleibt offen.

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
