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
| `Client.cpp` | Client-Stellen ohne Host-Objekte: Refresh, Attribute, Figurenkomponente, `ThreatLevel` |
| `Camera.cpp` | Gemeinsame Kamera und Bodenfix |
| `Inventory.cpp` | Netzwerk-Support für Items/Slots und klassischer Unterobjekt-Replikationspfad je neuer Inventarkomponente |
| `Travel.cpp` | Umleitung der Reise in `servertravel` |
| `Widgets.cpp` | Zurückhalten der HUD bis zum `GameState` |
| `MapObjects.cpp` | Client: lokale Kopien replizierter Kartenobjekte unterdrücken |
| `Mission.cpp` | Gegnerwelle, Missionsende, Rundenende |
| `Trace.cpp` | Zähler für RPCs und Run-Funktionen |

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
| Schießen | Schüsse des Clients sind beim Host sichtbar. Neben der Client-Figur sterben beim Host Gegner, die Host-Figur ist dort außer Reichweite |
| Mission über das Spielmenü | Klappt mit Umleitung der Reise, beide in DDDynamic_Map, Gegner laufen von selbst. Karten verschieden: 144 bis 167 MapTile beim Host, 17 beim Client |
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
| Host | `0x5976AD0` | `SeamlessTravelToLevel` reist nur lokal | Umleitung in `servertravel`, nur wenn scharf und nicht ins Hauptmenü |
| Beide | `0x160C040` | `WP_UI` wartet in `PreConstruct` auf den `GameState` | `ProcessEvent`-Hook hält `PreConstruct` und `Construct` zurück, bis er da ist |
| Client | `0x593DAF3` | `ThreatLevel` nutzt beim Aufräumen `FreezeThreatLevelManager`, den nur der Host hat | Vorhandenes `je` springt ans Funktionsende |
| Client | `0x58393CC` | Figurenkomponente erwartet einen Controller, die fremde Figur hat keinen | Funktion verlassen |
| Beide | Vtables `0x939CE08`, `0x94B46E8`, jeweils `+0x200` | Slots/Items benutzen den UObject-Standard für Netzwerk-Support | Nur für diese Klassen dynamische Netzwerkobjekte erlauben |
| Beide | Vtable `0x94BA3F0`, `+0x3D8` | Inventarkomponente nutzt registrierte Liste statt ihres vorhandenen `ReplicateSubobjects` | In `ReadyForReplication` Bit 6 bei `+0x98` löschen, dann ursprüngliche Funktion aufrufen |
| Client | `0x2A678F0` | `BP_PowerUpCrate_C` spawnt in `BeginPlay` ohne Autoritätsprüfung seine `BP_ChaosDestructable_PowerUp_C`, beim Client kommt dieselbe zusätzlich repliziert an | `BeginDeferredActorSpawnFromClass` gibt beim Client für diese Klasse aus einer Kiste `null` zurück |

Die Zuordnung der Funktionen stammt aus der Disassembly ohne Symbole. Die Kamera ist über Offsets im SDK belegt, die übrigen passen zu den Aufrufern.

Beim Client setzt der Spike die Kamera jede Sekunde, beim Host nimmt er jede Sekunde Client-Positionen an und ersetzt veraltete Einträge in `SpawnedPlayers`. Dazu kommen Befehle für die Datei-Schnittstelle:

| Befehl | Seite | Wirkung |
|---|---|---|
| `!cam` | Client | Setzt die gemeinsame Kamera als ViewTarget, sonst schaut der Client vom Weltursprung aus. Ohne den Befehl sprang auch die eigene Figur zum Ursprung, vermutlich durch die Sichtbereich-Begrenzung |
| `!authmove` | Host | Nimmt die Position von Remote-Figuren vom Client an |
| `!trace` | Beide | Zählt Client- und Server-RPCs der Spielfigur |
| `!wave` | Host | Startet die Gegner-Spawner |
| `!flow` | Beide | Protokolliert die ersten Aufrufe der Run-Funktionen |
| `!arm` | Host | Schaltet die Umleitung der Reise scharf |
| `!finish` | Host | Ruft `FinishMission` auf |
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

## Bekannte Abweichungen

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

**Noch offen:** Beide anschliessenden Reisen auf Tag 3 endeten beim Host im Callback `0x57CC390`, Fehler `0x57CC395` bzw. `0x57CC398`, nächster Stack-Eintrag `0x315A764`. Klasse des Callback-Objekts und Zusammenhang mit dem Teilfix nicht geklärt. Ein Folgeabsturz des Clients lag bei `0x57D080C`. Deshalb bleibt die TODO-Zeile offen; kein vollständiger Run belegt. Nicht getestet: Client-Inventarbedienung und RPCs, grössere/wechselnde Inventare, Belohnungen/Item-Effekte, Internet und Paketverlust. `obj gc` wurde gesendet, die tatsächliche GC-Ausführung aber nicht nachgewiesen. Rohmessungen und Absturzberichte bleiben unter `research/local/`.

## Crash-Reporter

Das Spiel enthält den `CrashReportClient` mit einer Upload-Adresse von Pixelsplit. **Mit `-unattended` lädt er Berichte ohne Rückfrage hoch**, inklusive
Minidump und einer Kopie des aktiven Spielstands. So gingen bei diesem Spike zwei Berichte raus.

- `-unattended` niemals verwenden.
- Während der Tests läuft `tools/spike/CrashGuard.ps1`, das `CrashReportClient.exe` sofort beendet. Das hat die weiteren Abstürze abgefangen.
- Ein Override von `DataRouterUrl` in der `Engine.ini` des `UserDir` wirkt nicht. Die Adresse stand trotzdem im Bericht.
