# Spike A und B: Verbindung und Spieler

Stand: 02.11.2026, Build `26187885`. Wegwerfcode, die Offsets gelten nur für diesen Build.

## Aufbau

- Zwei Instanzen auf einem PC unter demselben Steam-Konto, Verbindung über `127.0.0.1:7777`.
- Die Exe wird direkt gestartet, mit den Umgebungsvariablen `SteamAppId` und `SteamGameId` sowie je einem eigenen `-UserDir=`.
  So bleibt der echte Spielstand unberührt.
- `spike.cpp` wird als DLL in beide Instanzen geladen. Sie führt Konsolenbefehle aus einer Datei auf dem Game Thread aus.
  Der Game Thread wird über eine Nachricht an das Spielfenster erreicht.

Gebaut und gestartet wird über die Skripte in `tools/spike/`, die auch den Crash-Reporter absichern, siehe [Testen](../../../docs/TESTING.md).

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
| Missionsende, Routenkarte, Tag 2 | Host klappt. Client stürzt beim Ankommen an Tag 2 ab, Ursache im Inventar, siehe [Run-Ablauf](../../../docs/GAME_ANALYSIS.md#run-ablauf) |
| Pausemenü ins Hauptmenü | Host klappt, Client stürzt beim Trennen bei `0x5975A6A` ab |

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
| Client | `0x5963604`, `0x5A7E060`, `0x5965660` | Schleifen über `MyItems` und `Slots` lesen `null`-Einträge | Eintrag überspringen. Reicht nicht, siehe unten |

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

## Bekannte Abweichungen

Diese Punkte gehören zu Thema 6, sie stoppen die Technik nicht:

- `MovementSpeed` und Beschleunigung weichen ab: Host 819 und 2866, Client 600 und 2048. Upgrades kommen beim Client also nicht an.
- Gegner-Zähler weichen ab, zuletzt 30 beim Host und 48 beim Client. Vermutlich bleiben beim Client tote Gegner als Objekte stehen, geprüft ist das nicht.
- Blut- und Treffereffekte sowie die XP-Leiste erscheinen nur beim Host.
- Tutorial-Hinweise laufen auf beiden Seiten getrennt.
- Die direkt geladene Mission startet keine Gegner von selbst. Über das Spielmenü gestartet tut sie das.
- Der Client stürzte einmal im Missions-Tick bei `0x5A17ACC` ab (Lesen von `0x2C8`). Das trat nur einmal auf, die Ursache ist offen.
- An Tag 2 zeigt der Host ein anderes Figurenporträt mit violetter Leiste. Die Ursache ist offen.
- Ob die Figuren automatisch feuern, ist unklar. Die Host-Figur schoss, ohne dass ihr Fenster Eingaben bekam.

## Offener Client-Absturz

Der Client überlebt den zweiten Kartenwechsel nicht. Nach jedem Schutz in den Inventar-Schleifen stürzte er an der nächsten Stelle derselben Klasse ab: `0x596362D`, `0x5A7E063`, `0x5965663`, zuletzt `0x59661C3`.
Weitere Einzelprüfungen lohnen sich nicht. Das Inventar braucht beim Client eine echte Lösung, siehe [TODO](../../../TODO.md). Die Abstürze dort erzeugten teils keinen Bericht im `UserDir`, die Adresse steht dann nur im Windows-Ereignisprotokoll.

## Crash-Reporter

Das Spiel enthält den `CrashReportClient` mit einer Upload-Adresse von Pixelsplit. **Mit `-unattended` lädt er Berichte ohne Rückfrage hoch**, inklusive
Minidump und einer Kopie des aktiven Spielstands. So gingen bei diesem Spike zwei Berichte raus.

- `-unattended` niemals verwenden.
- Während der Tests läuft `tools/spike/CrashGuard.ps1`, das `CrashReportClient.exe` sofort beendet. Das hat die weiteren Abstürze abgefangen.
- Ein Override von `DataRouterUrl` in der `Engine.ini` des `UserDir` wirkt nicht. Die Adresse stand trotzdem im Bericht.
