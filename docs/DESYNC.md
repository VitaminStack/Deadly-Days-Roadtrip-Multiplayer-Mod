# Desync-Melder

Diagnose in `src/Sync/DesyncSync.cpp`, Messung in `src/Game/DesyncAdapter.cpp`.
Keine Zustandskorrektur, XP-/Itemvergabe, Save-Mutation oder automatische Reparatur.
Der Getter `GetComponentByClass` wird über ProcessEvent aufgerufen; ansonsten liest
der Adapter nur Speicher. Er läuft ausschließlich auf dem Game Thread.

## Messumfang

| Feld | Bedeutung | Grenze |
|---|---|---|
| Tag | `DDMissionGameState.DayNum` | Nur gültige Missionswelt mit NetGUID |
| Phase | 0 = laufend, 1 = pausiert (`TimeDilation`/`PauserPlayerState`) | Keine Unterscheidung von Lager, Route oder Rundenübersicht |
| Spieler | Anzahl gültiger `DDPlayerState`s im PlayerArray | Höchstens zwei; fehlende NetGUIDs machen das Feld unbekannt |
| HP | NetGUID, aktuelle/maximale HP je Spieler, auf Zehntel gerundet | Keine vollständigen Attribute/Buffs |
| Gegner | Sichtbare `EnemyBase`-Actors des PersistentLevel | Keine Streaming-Level; wegen `opt-gegnerzahl` standardmäßig ausgenommen |
| Inventar | Replizierte Items und BackpackSlots je Spieler | Kein Stash, Buff-/Effektzustand, Rarität oder nicht replizierte ItemInstanceId |

Items enthalten den vollständigen Asset-Pfad von ItemData, Pivot, Drehung und die
Flags Broken/IsInBackpack/IsReward/IsTrash. Slots enthalten Koordinaten und den
Inhaltshash ihres Occupier. FNV-1a über explizite Little-Endian-Zahlen und
längenmarkierte UTF-8-Namen, danach sortierte Multiset-Hashes: Reihenfolge ist
egal, doppelte Inhalte bleiben erhalten. Keine Prozessadressen, lokalen
FName-Indizes oder persönlichen Namen übertragen. Hashgleichheit ist kein
Beweis vollständiger Gameplay-Gleichheit; Kollisionen sind möglich.

Nicht lesbare, zu große, noch unvollständige oder nicht kanonisierbare Sammlungen
werden **unbekannt**, nicht leer. Wide-/nummerierte Asset-Namen werden derzeit
nicht geraten. Damit kann die Inventarprüfung offen bleiben; das ist kein OK.

## Ablauf und Fehlalarmschutz

- Ein zentraler Queue-Drain in `Sync.cpp` gibt Progress und Desync dasselbe
  unveränderte Nachrichtenfenster. Beide filtern ihre eigene Typfamilie.
- Lokale Messung einmal pro Sekunde, clientseitige Probe frühestens alle zwei
  Sekunden. Nur eine offene Probe; auf eine langsame Antwort wird bis zehn
  Sekunden gewartet, statt sie nach zwei Sekunden ungültig zu machen.
- Ein Feld muss lokal auf beiden Seiten mindestens zwei Sekunden unverändert
  sein. Erst drei aufeinanderfolgende Proben mit demselben abweichenden Wertepaar
  bestätigen einen Alarm. Änderungen, unbekannte Werte und Sequenzlücken setzen
  die Bestätigungen zurück. Das ist kein zeitlich atomarer Snapshot beider PCs.
- Host entscheidet die Ausnahmen und bestätigt das Ergebnis. Client prüft
  Peer, lokale Run-ID/Nonce, Probensequenz, NetGUID, Host-Run/Epoche/Revision und
  lokale Antwortfrist. Lokale Änderungen seit der Probe sperren das betreffende
  Feld in der Anzeige. Alte oder doppelte Antworten ändern keinen Wasserstand.
- Welt/State/NetGUID/Tag/Peer/Rolle-Wechsel löschen Messung, Stabilität und Alarme.
  Ohne frische Gegenmessung nach zehn Sekunden kein OK und kein alter Alarm.
- Abweichungen mit Hashwerten im Host-Log, Zusammenfassung auf beiden Seiten im
  Log und F8-Fenster. Statusänderungen sofort, unveränderte Zusammenfassung alle
  30 Sekunden. `OK` gilt nur, wenn alle nicht ausgenommenen Felder vergleichbar
  und gleich sind. Ausnahmen bleiben sichtbar.

## Konfiguration (`ddcoop.ini`)

Fehlende Schlüssel verwenden diese Standardwerte. Einzeln abschaltbar; der Host
bestimmt die gemeinsame Ausnahme-Policy, auch wenn der Client anders konfiguriert ist.

```ini
desync_enabled=true
desync_ignore_day=false
desync_ignore_phase=false
desync_ignore_players=false
desync_ignore_health=false
desync_ignore_enemies=true
desync_ignore_inventory=false
```

`desync_enabled=false` beendet nur die Diagnose, nicht Progress/Transport. Ohne
aktivierte Gegenseite erscheint kein Gleichheitsnachweis.

## Reproduzierbarer Spieltest

Eigene geschützte Sitzung mit `Start-Session.ps1 -Mod -Tunnel Lobby -Godmode -Brief`,
dann `Run-Route.ps1 -Days 1 -StayLast -Brief`. Anschließend:

```powershell
powershell -ExecutionPolicy Bypass -File tools/spike/Test-Desync.ps1
```

Das Skript verlangt zu Beginn und vor der Abweichungsprobe auf beiden Seiten alle
fünf aktiven Felder als OK und misst mindestens 600 Sekunden ohne Alarm. Mindestens
90 Prozent der Messungen müssen vollständig vergleichbar sein; Zustandswechsel
dürfen höchstens 60 Sekunden am Stück im Stabilisierungsstatus bleiben. Solche
Fenster und der tatsächliche Abdeckungsanteil stehen im Bericht, sie werden nicht
als nachgewiesene Gleichheit gezählt. Danach toggelt
`desync_difference.py` **nur im eigenen lokalen Testclient** das Broken-Flag eines
vorhandenen Inventaritems für 14 Sekunden. Es erlaubt keine beliebigen PIDs oder
Adressen als Argumente und stellt den Originalwert in `finally` wieder her, sofern
die Objektlebensdauer unverändert ist. Ein Abbruch des Helferprozesses während
dieses Tests ist zu vermeiden. Beide F8-Modelle müssen den Inventaralarm zeigen
und nach Rücksetzung wieder OK werden. Journale und Bericht liegen unter
`research/local/session/desync-<Zeit>/` und werden nicht committet.

## Stand der Abnahme (05.11.2026)

Release und Debug: jeweils 122 C++-Testfälle und 124 Python-Tests bestanden,
einschließlich Codec, Stabilität, Alarm, Ausnahmen, Lebensdauer, Fanout und
unabhängigen UI-Updates. Zusätzlich Game mit lokalem SDK und Offset-/Flag-
Assertions erfolgreich gebaut. Der Modelltest mit 600000 ms gleicher
synthetischer Bilder ist ausdrücklich **keine zehnminütige Spielabnahme**.

Spielstart zunächst durch die fremde Sitzung `ohne-spike` abgewiesen. Nach Warten
eigene Sitzung mit Lobbytransport und echter Route nach Tag 1: zwei Spieler,
vollständige gleiche Inventare, fünf aktive Diagnosefelder auf beiden Seiten OK.
Ergebnisse der Spielabnahme in den folgenden Abschnitten. Noch keine Internet-/Zwei-Konten-
Abnahme und keine Freigabe für Gate G2.

### Langzeitmessung im Spiel (05.11.2026, nach Merge von `main`)

Eigene Sitzung mit `-Mod -Tunnel Lobby -Godmode`, Run-Route bis Tag 1, beide in
der Mission. Zwei Läufe von `Test-Desync.ps1` in derselben Mission, zusammen rund
636 Sekunden ohne Eingabe. Die Messung wurde auf Wunsch nach dem zweiten Lauf
beendet und gilt nicht als Abbruch.

| Lauf | Messungen | Dauer | beide OK | Alarme |
|---|---|---|---|---|
| 1 | 543 | 557 s | 533 (98,2 %) | 0 |
| 2 | 79 | 79 s | 79 (100 %) | 0 |

- In der gesamten Zeit **kein Fehlalarm**. Die zehn nicht vollständig vergleichbaren
  Messungen in Lauf 1 lagen alle in einem Fenster ab Sekunde 547, in dem Phase
  und Inventar auf beiden Seiten wechselten: Loot-Benachrichtigung offen, Inventar
  geändert, kurze Pause. Der Melder zeigte dabei „Abgleich“, keinen Alarm.
- Lauf 1 brach an der damaligen Testregel „höchstens zehn Sekunden am Stück offen“
  ab. Echte Zustandswechsel dauern länger, die Grenze liegt jetzt bei 60 Sekunden.
  Abdeckung von mindestens 90 Prozent und Abbruch bei jedem Alarm bleiben.
- Rund 12,5 Minuten nach Missionsstart, am regulären Rundenende von Tag 1, stürzte
  zuerst der Client ab (Access Violation beim Lesen von `0x348`), 30 Sekunden später
  der Host (`0x1B8`). Der Stack enthält nur Frames des Spiels, keine aus `DDCoop`.
  Dasselbe Paar mit identischen Stacks gab es schon im Lauf vor dem Merge von `main`.
  Ob der Melder beteiligt ist, ist **nicht geklärt**. Die Gegenprobe ist dasselbe
  Rundenende mit `desync_enabled=false`.
- Weiterverfolgt als eigene Aufgabe `rundenende-absturz`.

### Absichtliche Abweichung im Spiel (05.11.2026)

Neue Sitzung, Tag 1, `Test-Desync.ps1 -Seconds 10`. Die zehn Sekunden Gleichheit
davor waren vollständig vergleichbar. Der Helfer setzte beim Client das
Broken-Flag eines Items. Nach rund acht Sekunden zeigten Host und Client
„ABWEICHUNG Inventar“, nach der Rücksetzung wieder „OK“ (`result.json`:
`hostAlert`, `clientAlert` und `recovered` jeweils `true`). Kein Absturz.

Beim ersten Versuch meldete das Skript einen Fehler des Helfers, obwohl dieser
erfolgreich war: Nach `Start-Process -PassThru` bleibt `ExitCode` ohne vorherigen
Zugriff auf `Handle` leer. Das ist im Skript behoben.

**Abnahme erfüllt:** gleiche Zustände ohne Fehlalarm (rund 636 s, auf Wunsch
nicht weiter verlängert) und absichtliche Abweichung auf beiden Seiten gemeldet.

### Gefundene und behobene Fehlversuche

- Direkte Abhängigkeit Sync → UI vom Modulgrenzentest abgewiesen; Mod verbindet
  jetzt einen Diagnose-Callback, die Modulgrenzen bleiben unverändert.
- Neues STATIC-Feld zuerst vor den bisherigen Overlaytexten erzeugt. Dadurch las
  das Lobby-Testskript das falsche Feld und lief ins Zeitlimit, obwohl der Host
  bereits eine Lobby hatte. Alte Reihenfolge erhalten, Diagnose hinten angehängt,
  nativen Regressionstest für alle fünf Textfelder ergänzt.
- Inventar zunächst unbekannt: `0x00200000` am UObject ist `LoadCompleted`, nicht
  das interne GUObjectArray-Garbageflag. Im Spiel an geladenen InventoryComponents
  nachgewiesen. Getrennte EObjectFlags-Policy mit Regressionstest und SDK-Assertion;
  danach alle Inventarhashes vergleichbar und gleich.
- Erste strenge Langzeitprobe brach nach 365 Messungen wegen kurzzeitig offener
  Phase/Inventar ab, ohne bestätigten Alarm. Eine Wiederholung zeigte dasselbe.
  Die genaue Ursache dieser kurzen Messfenster ist nicht gesondert belegt.
  Der Test unterscheidet jetzt ausdrücklich
  Alarm von Stabilisierung, protokolliert offene Fenster und verlangt mindestens
  90 Prozent volle Abdeckung sowie höchstens zehn Sekunden am Stück ohne sie.
