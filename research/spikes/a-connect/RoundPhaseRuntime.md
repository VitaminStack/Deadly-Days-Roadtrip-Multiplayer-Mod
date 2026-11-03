# Rundenphasen: lokaler Laufzeitadapter, 03.11.2026

**Kein abgeschlossener P0 und kein Produktionsprotokoll.** Der Adapter in `Mission.cpp` ist gebaut, an die ausdrücklich freigegebene `Main.cpp` angeschlossen und ohne Spiel getestet. Kein Spiel gestartet, kein Fenster bedient, keine REA-Sitzung verändert. SDK-unabhängig, keine neuen Offsets oder nativen Detours. `Client.cpp` bleibt für die parallele Arbeit unangetastet.

## Was tatsächlich implementiert ist

- Explizite Diagnosebindung Host/Client an Session, Run und Weltgeneration. Rolle wird am vorhandenen NetDriver/ServerConnection geprüft. IDs werden nicht aus Paketen übernommen; Rollen-/Sessionwechsel benötigen `!phase stop`.
- Host-Publisher mit monotoner Sequenz, vollständigen 48-Byte-Snapshots und sekündlichen Heartbeats. Ohne bestätigte Hostphase wird **kein** initiales `Playing` gesendet.
- Reale lokale Dateiübertragung über `%TEMP%/ddspike/round-<session>-<run>-<world>.bin`. Erst vollständige temporäre Datei schreiben/flushen, dann atomar ersetzen. Empfänger akzeptiert nur genau 48 gültige Bytes. Das ist ein gleichbenutziger Ein-PC-Spike, **keine Authentisierung**, kein Netzwerkkanal und kein Ersatz für `protokoll`.
- Empfänger: zunächst gesperrt; nur frischere, gültige Snapshots erneuern die 3,5-Sekunden-Frist. Das wiederholte Lesen einer alten Datei hält keine Verbindung künstlich lebendig. Timeout sperrt und zeigt Warten; frischer Snapshot hebt Warten wieder auf. Nicht-Playing bleibt gesperrt.
- Rein anzeigende Win32-Textzeile als nicht aktivierendes Kindfenster im Client. Kein Spielpanel, kein Route-/Inventarobjekt und keine Persistenz wird erzeugt oder aufgerufen. Anzeige nennt die Hostphase und den Tag, nicht den Inhalt des Lagers oder einzelne Routenknoten.
- Filter für Tastatur-, Maus- und Raw-Input-Nachrichten an die Spiel-WndProc. Timer, Diagnosebefehle und `WM_CLOSE` werden weitergereicht; `WM_INPUT` erhält die erforderliche `DefWindowProc`-Bereinigung.
- Welt-, Driver- oder ServerConnection-Wechsel verwirft die alte Bindung und entfernt eigene Anzeige/Sperre. Danach ist eine **explizite neue Bindung nötig**, kein automatischer Travel-Handshake. Während dieser ungebundenen Lücke besteht kein Inputschutz.

## Hostquelle und Beleggrenze

`ObserveRoundPhaseEvent` wird **nach** dem ursprünglichen `ProcessEvent` aufgerufen. Es akzeptiert ausschließlich die per `FindObject` aufgelösten `DDMissionGameState.FinishMission`/`DDMissionGameInstance.TriggerEndRound` und das aktuelle GameState-/GameInstance-Objekt. Nicht-Game-Thread-Aufrufe werden vor Zugriff auf Adapterzustand verworfen.

`TriggerEndRound` liefert seinen Grund aus dem vorhandenen Ein-Byte-Parameter. Bei `FinishMission` wird `LastRoundChangeReason` der aktuellen GameInstance gelesen; ohne gültigen Nicht-Null-Grund entsteht keine Rundenende-Nachricht. Der Tag wird beim Binden aus dem GameState gelesen. Grundlage sind die bereits dokumentierten Felder in `roundstate.py`: World→GameInstance `0x228`, LastRoundChangeReason `0x299`, GameState.CurrentDay `0x390`. Es wurden keine neuen Offsets geraten.

Diese Quelle belegt nach Integration einen reflektierten Hostaufruf, **nicht** die sichtbare Fertigstellung der Rundenübersicht. Direkte native Aufrufe außerhalb `ProcessEvent` sind nicht abgedeckt. Der Grund kann erst später gesetzt werden; das muss der Lauf klären. Für Lager-/Routenöffnung sind im vorhandenen Quellbestand keine sicheren Ereignisse belegt. Daher sind `stage stash/route` explizite manuelle Diagnosemarkierungen, keine angeblich automatische Synchronisation. Bestehende Panelobjekte/AnimationContainer werden weiterhin nicht als Sichtbarkeitsquelle benutzt.

## Integration durch den Hauptagenten

Bereits angeschlossen; beim Übernehmen des Commits diese Änderungen in `Main.cpp` erhalten:

1. `Main.cpp` bindet `#include "RoundPhaseAdapter.h"` ein. Am Anfang von `TickSession`, **vor** dem `if (!Driver) return`, `TickRoundPhase(GameWindow);` aufrufen, damit Disconnect-Cleanup auch ohne Driver läuft.
2. Am Anfang von `RunPendingCommand`: `if (RoundPhaseCommand(Pending)) return;`. In `WndProc` vor dem Aufruf von `CallWindowProcW`: `if (RoundPhaseBlocksMessage(Window, Message, WParam, LParam)) return 0;`. Nachrichten nur filtern, nicht den gesamten Thread anhalten.
3. **Noch separat durch den Hauptagenten:** `Widgets.cpp` bindet denselben Header ein und ruft direkt **nach** `OriginalNativeProcessEvent(Object, Function, Params);` den Beobachter `ObserveRoundPhaseEvent(Object, Function, Params);` auf. Keinen zweiten ProcessEvent-Detour installieren. Zurückgehaltene HUD-Konstruktoren brauchen keinen Phasenbeobachter. Bis dahin melden nur die Diagnosebefehle `!finish`/`!round` ihren tatsächlichen Hostaufruf ausdrücklich an den Beobachter; normale Menü-/Busaufrufe bleiben unverbunden.

`Build-Spike.ps1` nimmt `Mission.cpp` bereits mit. Der Adapter bleibt bis zur expliziten `!phase bind`-Aktivierung inaktiv. Keine Änderungen an `Spike.h` erforderlich.

### Kontrollierter Lauf (nur Hauptagent)

Über den normalen geschützten Startweg und bekannte Build-ID, niemals manuell oder unattended starten. Für jeden Test eine **neue zufällige positive dezimale Session-ID** wählen (z.B. PowerShell `Get-Random -Minimum 100000000 -Maximum 2147483647`); alte Dateien sind absichtlich keine automatische Sessionquelle.

In bereits verbundener Mission, gleiche IDs auf beiden Seiten, getrennte Befehle:

```text
Host:   !phase bind host <session> 1 1
Client: !phase bind client <session> 1 1
Host:   !phase stage playing
```

Der Client soll bis zum Host-Snapshot warten und danach seine Textzeile verlieren. Normales Host-Missionsende oder `!finish` auslösen und Log `reflektierter Host-Rundenendeaufruf beobachtet` suchen. Fehlt es, **nicht** als automatische Quelle abnehmen. Nur für den Transport-/Anzeigetest nach realem Hostpanelwechsel:

```text
Host: !phase stage stash
Host: !phase stage route
```

Diese Befehle öffnen selbst kein Hostpanel. Der Client darf weder Lager bestätigen noch eine Route auswählen. Vor/nach Reise `stage travel` bzw. frische Weltgeneration auf beiden Seiten explizit binden; der Adapter errät keine Generation aus Weltzeigern. Beim Neubinden zunächst warten, dann Hostphase bestätigen. Zum Entfernen auf beiden Seiten `!phase stop`; nur dieser eigene Anzeige-/Filterzustand wird entfernt, keine andere Pausen-/Spielsperre zurückgesetzt.

## Tests und offene Nachweise

Nacheinander ausführen, da `Common.ps1` eine gemeinsame lokale `vsenv.cmd` schreibt:

```powershell
powershell -NoProfile -File tests/Run-RoundPhase.ps1
powershell -NoProfile -File tests/Run-RoundPhaseRuntime.ps1
powershell -NoProfile -File tools/spike/Build-Spike.ps1
```

Kern- und Adaptertests mit C++20 `/W4 /WX`, kompletter Spike als MSVC-DLL. Der Runtime-Test startet ausschließlich seinen eigenen zweiten **Testprozess** mit `CREATE_NO_WINDOW`; der Adaptertest benutzt eigene Bytearrays statt eines Spielprozesses. Kein UI-Testfenster wird angelegt. Geprüft: echte lokale Byteübertragung, atomarer Austausch, kurze/lange/fehlende Dateien, Codec/Generation/Sequenz, Timeout einschließlich doppelter Altdatei, neue Welt, Disconnect, Host-Quellobjektfilter, manueller Phasenpfad, Client-Schreibverbot und Nachrichtenauswahl. Das beweist weder einen vollständigen Spielrun noch sichtbare UE-UI.

**Für P0 weiterhin zwingend offen:**

1. Sichere automatische Quellen für tatsächlich geöffnete Rundenübersicht, Lager und Route, einschließlich Initialroute und späterem Run/Tag; nativer direkter Aufrufpfad vs. ProcessEvent.
2. Sichtbarkeit/Position/Lesbarkeit des Textfensters unter dem echten Renderer und Aufräumen bei Travel/Disconnect, kein Fensterfokuswechsel. Lager-/Routeninhalt und Hostfortschritt werden noch nicht dargestellt.
3. Vollständiger UE-Inputblock. Der aktuelle Filter ist **nur Win32-Nachrichtenpolitik**: XInput/Gamepad und anderweitig gepollter Input, automatisch feuernde Fähigkeiten und bereits gehaltene Bewegungen können weiterlaufen. Key-up-Verlust darf beim Entsperren keine hängenden Tasten verursachen. Keine unbewiesenen UE-Inputfunktionen aufgerufen.
4. Automatische verifizierte Verbindung-/Run-/Weltbindung ohne ungeschützte Travel-Lücke; Late Join und Hostverlust im Spiel.
5. Vollständiger Lauf bis mindestens Tag 3 und danach; Internet-/Paketverlusttest gehört nicht zu diesem Dateitransport.

Ein erster paralleler Test-Build kollidierte an `vsenv.cmd`; anschließend alle Builds seriell wiederholt. Der echte Adaptertest fand zusätzlich den Windows-`max`-Makrokonflikt im Header; mit geklammertem `numeric_limits::max` behoben. Keine fremden Testprozesse oder Sitzungen verändert.
