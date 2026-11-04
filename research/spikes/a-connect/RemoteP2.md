# Remote-Controller als P2 (`remote-p2`)

Stand 04.11.2026, eingefrorener Build `26187885`, nur Spike. **Analyse-Abnahme:** Der Remote-Controller kann beim Listen-Host P2 sein, ohne einen zweiten `ULocalPlayer` anzulegen. Welche Anpassungen nötig sind und wo native Coop-Logik nicht genügt, ist damit geklärt. Das ist kein Abschluss von `remote-spieler`, `eingaben`, `inventar-sync` oder `inventar-ui` und kein vollständiger Coop-Run mit eingeschaltetem nativen Coop.

## Belegter Weg und Gegenprobe

1. **Nur einschalten genügt nicht.** Ohne Probe war `bCoopEnabled=0`, `GetPlayerController(1)` lieferte beim Host nicht den Remote-Controller. `ActivePlayers` war leer oder enthielt alte schwache Verweise. Der normale Netzwerk-Spawn war trotzdem vorhanden.
2. **Aktuelle Controller eintragen.** Der Probe-Tick nimmt die Controller aus der aktuellen `PlayerArray`, unterscheidet lokalen Spieler und `NetConnection` und prüft die aktuelle Level-Zugehörigkeit. Genau ein lokaler Spieler und ein Remote-Spieler sind erlaubt. In `ActivePlayers` werden die Schlüssel 0 und 1 mit dem nativen Map-Add ersetzt; die Werte sind Index/Seriennummer des schwachen UObject-Verweises. Keine Änderung von Map-Schlüsseln, keine zusätzlichen starken Referenzen, kein `CreateLocalPlayer`.
3. **`SetCoopEnabled(true)` über `ProcessEvent`.** Danach stimmen P1/P2-Abfragen und `GetActivePlayerCount()==2`. Das Original wird nicht durch bloßes Setzen des Flags ersetzt, seine Benachrichtigungen bleiben erhalten. Das Flag `+0x18A` für die lokale Neuerzeugung von P2 wird nicht gesetzt.
4. **Die lokale Gerätepflicht ist eine echte Sperre.** Ohne zusätzlichen Guard wurde `IsReconnectRequired` wahr; das Hostfenster zeigte „KOOP-SPIELER HINZUFÜGEN / WEITEREN CONTROLLER VERBINDEN“. Im Host-Router lagen blockierende lokale UI-Sitzungen. Der Netzwerkcontroller allein erfüllt diese Geräteprüfung nicht.
5. **Eng begrenzter Guard.** Bei `0x59072AA` wird nur die Prüfung auf eine fehlende lokale P2-Gerätebindung ergänzt: Eine vollständig vorhandene Remote-P2-Bindung beim aktuellen Listen-Host genügt an dieser Stelle. Die davor liegende Prüfung auf ein fehlendes P1-Gerät und die folgende Solo-Controller-Verlustprüfung bleiben bestehen. Kein gefälschtes physisches Gerät und keine pauschale Abschaltung des Reconnect-Systems. Ohne gültigen Remote-Roster gilt die Originalentscheidung. Der Guard installiert sich erst mit `!p2 on`; alle eigenen Voraussetzungen und die überschriebene Instruktionsfolge sind bytegeprüft.
6. **Beitrittsmeldung getrennt prüfen.** `!p2 joined` sendet denselben nativen `OnPlayerJoined(1, RemoteController)`-Delegate wie der lokale Spawn, aber ohne dessen Spawn-/Gerätepfad. Auf dem Host erschien damit die grüne P2-Markierung über der Figur. Wiederholung für dieselbe Welt und denselben schwachen Controller-Verweis sendet kein zweites Ereignis; Controller-Tausch und neue Seriennummer erlauben eine neue Meldung. Die Probe sendet diese Meldung absichtlich nicht automatisch: Ihr Effekt ist dadurch getrennt messbar.
7. **Weltwechsel/Trennen.** Der Tick ersetzt nach dem Controller-Tausch beide schwachen Map-Werte. Fehlt der Remote-Spieler bei sonst gültigem Host-Roster, entfernt er nur seine eigene zuletzt eingetragene P2-Bindung: Hash-Link aushängen, dann den nativen SparseArray-Entferner für Elemente von `0x14` Byte aufrufen. Kein `RemoveLocalPlayer`, kein erneuter Login, kein künstliches Leave-Ereignis während eines möglicherweise noch laufenden Travel. Bei erneuter Verbindung wird der neue Remote-Controller eingetragen. Die Netz-/UI-Fehlerbehandlung bleibt eine andere Aufgabe.

## Auswirkung auf UI und Eingaben

- **Mission:** Host und Client bewegten ihre eigenen Figuren unabhängig. In der Messung blieb beim Client-Lauf die Host-Figur stehen; anschließend bewegte Host-Eingabe nur die Host-Figur. Positionen stimmten auf beiden Seiten überein. In der geprüften Mission waren keine Router-Sitzungen offen, kein Reconnect-/Gerätefenster und keine Weltpause aktiv.
- **Rundenübersicht:** Mit nativer Coop-Aktivierung zeigt der Host die blauen/grünen Statistiken für beide Spieler. Das ist ein lokaler UI-Effekt, keine Replikation dieser Oberfläche zum Client.
- **Lager:** Der Host zeigt die zwei Coop-Rucksäcke, verlangt aber eine lokale Kontrollübergabe. Beim Bestätigen erschien „Der andere Spieler konnte noch nicht die Kontrolle übernehmen“. Die lokalen Router-Sitzungen blieben bei Authority-Index 0. Der native lokale Ablauf darf deshalb **nicht** als bereits netzwerkfähig übernommen werden. Kein Klick-/Geräte-Fake eingebaut, um diesen Befund zu verstecken.
- **Client:** Sein eigener Controller bleibt engine-seitig lokaler Spieler 0. Host-`ActivePlayers`, `bCoopEnabled` und Router-Sitzungen kommen dort nicht automatisch an; `!p2 on` wird auf einem Client abgewiesen. Die Mod muss die Netzwerk-Identität P2 von der lokalen Geräte-/Viewport-Identität 0 trennen. Bloßes Umschalten des Client-Coop-Flags wäre keine Lösung.
- **Folgerung für die bestehenden Aufgaben:** `remote-spieler` übernimmt Roster/Lifecycle/Host-P2-Anbindung. `eingaben` hält lokale Geräte vom Remote-Slot getrennt. `protokoll`, `inventar-sync` und `inventar-ui` übertragen autorisierte UI-Anfragen und Ergebniszustände, statt die lokale Geräteübergabe nachzuspielen. Figuren-/Haustierwahl bleibt zusätzlich an den bekannten ungeprüften `GetLocalPlayer`-Stellen abzusichern; diese Probe testet sie nicht.

## Durchgeführte Prüfungen

Alle Spielstarts über `Start-Session.ps1` mit CrashGuard, getrennten Test-UserDirs, Godmode und unveränderter eingefrorener Spielkopie; nie `-unattended`. Ein PC, Loopback, Route über das Menü, Seed `20795`.

| Prüfung | Ergebnis |
|---|---|
| Ausgangszustand, dann Map-Eintrag + natives Einschalten ohne Geräte-Guard | P2 zugeordnet, aber lokales Gerätefenster und Eingabesperre; absichtlich als Gegenprobe gesichert |
| Mit Geräte-Guard, MetaBase → Mission Tag 1 | Durchgehend ein `LocalPlayer`, zwei aktuelle PlayerStates/Controller, richtige Host-P1/P2-Zuordnung, `recreate=0`, `reconnect=0`, Map-Hash gültig |
| Getrennte Bewegung, Kamera, Inventare | Beide Spieler steuerbar, keine neue Actor-Replikationsfehlermeldung; Inventare bei geprüften Ankünften vollständig/gleich |
| Explizite Beitrittsmeldung in MetaBase und Mission | P2-Markierung erscheint; auf dem finalen Build wird eine doppelte Meldung unterdrückt |
| Client-`!p2 on` | Abgewiesen, Client-Coop-Zustand unverändert |
| `!finish`, Rundenübersicht und Lager | Zwei Coop-Statistikspalten und Rucksäcke; lokale Kontrollübergabe blockiert wie oben beschrieben. Kein Weiterfahren als „vollständiger Run“ behauptet |
| Trennen vor Cleanup-Erweiterung | Alter P2-Map-Wert konnte noch auf einen lebenden alten Controller zeigen. Nicht als GC-sicher ignoriert |
| Finaler Cleanup-Build: Trennen/erneuter Beitritt in MetaBase | Eigene P2-Bindung entfernt, native aktive Spielerzahl 1, kein zusätzlicher lokaler Spieler; neuer Controller danach als P2, native aktive Spielerzahl 2, Hash gültig, Inventare vollständig/gleich |
| Frida-Beobachtung über den Menü-/Missionswechsel und anschließende UI-Proben | `CreateLocalPlayer` und `SpawnLocalPlayer` im überwachten Intervall jeweils 0 Aufrufe; keine gekürzte Nachrichtenhistorie. Die Instrumentierung wurde erst nach dem initialen Einschalten geladen, beweist also nicht dessen Aufrufzahl |

Frida-Beobachtung: `ev_924511a52b39e3f31792e902638c62dfb0aeff2b13098a40c4d1e81bf196c006`. Das Tool konnte die Prozessbeobachtung nicht an einen Artefakt-Hash binden; Zielpfad/Build und die jeweiligen Sitzungssnapshots sind separat geprüft. Rohlogs, Bilder, vollständige Snapshots und Evidence-Bundle liegen nur privat unter `research/local/session/`. Die Sitzungen und die Instrumentierung sind beendet; alle 54 eingefrorenen Originaldateien anschließend unverändert.

Ohne Spiel: `tests/Run-RemoteP2.ps1` führt den tatsächlich verwendeten Guard-Maschinencode in 16 Kombinationen von Remote-P2, lokalem P2-Gerät, P1-Gerät und Solo-Verlust aus. Dazu Sparse-Map-/Hash-Prüfungen, Kollisions-/Entfernungsfälle und Benachrichtigungsidentität. Python-Tests prüfen den rein lesenden Snapshot, Seriennummern, Hashfehler, unvollständige Reads und fremde GameInstances. Abschließend bestehen 94 Python-Tests, Debug-/Release-Build mit Core-/UI-/Game-Tests, bestehende native Autoritäts-/Readiness-Tests, Spike-Build und TODO-/README-/SVG-Konsistenz. Diese Tests ersetzen keine echte Geräte-Abziehprobe.

## Reproduzieren und Übernahmegrenzen

```powershell
powershell -File tools/spike/Start-Session.ps1 -Build -Godmode
powershell -File tools/spike/Send-Command.ps1 Host '!p2 on'
powershell -File tools/spike/Send-Command.ps1 Host '!p2 joined'
py tools/spike/remote_p2.py <host-pid> <client-pid>
```

Nach einem Controller-/Weltwechsel kann `!p2 joined` erneut gesendet werden. Die Probe bleibt bis zum Ende ihrer eigenen Testsitzung eingeschaltet; zum Rücksetzen `Stop-Session.ps1` verwenden, nicht blind `SetCoopEnabled(false)` auf einem noch verbundenen Remote-Controller aufrufen. Das Spiel kann den Coop-Zustand in seine Run-Daten übernehmen: nur die isolierten Test-Spielstände verwenden. Normale Spike-Sitzungen ohne `!p2 on` bleiben unverändert.

Für die Produktionsübernahme sind unmittelbare Lifecycle-Hooks statt des Diagnose-Sekundenticks, explizite Netzwerkidentität/Abbruchzustände, ein wohlgeordnetes Join/Leave-Ereignismodell und der UI-Kontrollkanal nötig. Nicht geprüft: Internet, echte Gamepads/Abziehen des P1-Geräts, Figuren-/Haustierwahl, Save/Load eines Coop-Runs oder ein vollständiger Run mit eingeschaltetem nativen Coop. Der zuvor akzeptierte Endlos-Nachweis wird durch diese neue, standardmäßig ausgeschaltete Probe weder erweitert noch wieder geöffnet.
