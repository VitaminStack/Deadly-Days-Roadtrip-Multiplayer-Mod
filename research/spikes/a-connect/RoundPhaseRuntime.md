# Rundenphasen beim Client (`phasenadapter`)

Stand 04.11.2026, Build `26187885`. Der Client sieht bei jedem Rundenende dieselbe Phase wie der Host: Rundenübersicht, Lager und Routenkarte. Solange der Host dort ist, kann der Client nichts auslösen. Das läuft automatisch, ohne Befehle. Transport ist eine Datei auf demselben PC. Über das Netz trägt das erst `transport` und `protokoll`.

## Ablauf

**Host.** In jeder neuen Welt mit NetDriver bindet sich der Adapter selbst: Session ist die eigene Prozess-ID, die Weltgeneration zählt mit, der Tag kommt aus `DayNum` des `DDMissionGameState`. Startphase ist `Playing`. Jede Sekunde und bei jedem Phasenwechsel schreibt er den 48-Byte-Snapshot nach `%TEMP%\ddspike\round-<session>-<run>-<welt>.bin` und kündigt die aktuelle Welt in `round-host.txt` an.

Die Phase kommt aus `ProcessEvent`. `Widgets.cpp` ruft dafür nach jedem Aufruf `ObserveRoundPhaseEvent` auf, ein zweiter Detour ist nicht nötig:

| Ereignis beim Host | Phase |
|---|---|
| `DDMissionGameState.FinishMission` am aktuellen GameState, `DDMissionGameInstance.TriggerEndRound` an der aktuellen GameInstance | Rundenübersicht, Grund aus `LastRoundChangeReason` bzw. dem Parameter |
| `AbstractPanel.OnOpenStateChanged(true)` an `WP_Round_Stats_C` | Rundenübersicht (bestätigt den Aufruf oben, greift auch ohne ihn) |
| dasselbe an `WP_Inventory_Panel_C`, nur nach einem Rundenende | Lager |
| dasselbe an `WI_MissionChoicePanel_C` | Routenkarte, auch die erste Route in der MetaBase |
| `OnOpenStateChanged(false)` an `WI_MissionChoicePanel_C` | vor einem Rundenende „Zurück“, also `Playing`, danach `Reise` |

Das Inventar-Panel öffnet sich auch mitten in der Mission, etwa bei der Belohnungsauswahl. Dort zählt es nicht als Lager, der Client bleibt frei. Die Panel-Klassen wurden mit `!phase scan` ermittelt. Der Befehl protokolliert jedes neue Paar aus Klasse und Funktion, das durch `ProcessEvent` läuft.

**Client.** Jede Sekunde liest er `round-host.txt`. Ist sie frisch (höchstens 5 s alt), bindet er sich an die angekündigte Hostwelt und liest deren Snapshot. Weicht die Phase von `Playing` ab oder ist der Snapshot älter als 3,5 s, ist der Client gesperrt:

- **Sperre:** `AActor::DisableInput(PC)` auf dem eigenen PlayerController und seiner Figur nimmt beide aus dem Input-Stack der Engine. Damit erreicht kein Gerät mehr das Gameplay. Zusätzlich filtert die Fensterprozedur Tastatur-, Maus- und Raw-Input-Nachrichten. Loslassen-Nachrichten (`WM_KEYUP`, `WM_*BUTTONUP`) kommen durch, damit keine Taste hängen bleibt. Endet die Sperre, ruft der Adapter `EnableInput` auf.
- **Anzeige:** ein eigenes Popup-Fenster oben über dem Spielfenster, zum Beispiel „Coop: Host ist in Routenkarte (Tag 1), Eingabe gesperrt“. Ein Kindfenster würde von der Swapchain übermalt.
- **Hostverlust:** Bleibt die Ankündigung aus, entfernt der Client Banner und Sperre nach 5 s selbst, er bleibt also nie dauerhaft gesperrt.

Welt- oder Verbindungswechsel verwerfen die Bindung auf beiden Seiten. Danach bindet der Adapter im selben Tick neu. Phasen dürfen innerhalb einer Welt zurückgehen, etwa wenn die Routenkarte mit „Zurück“ geschlossen wird. Die Reihenfolge legt allein die Sequenznummer fest.

## Diagnosebefehle

| Befehl | Wirkung |
|---|---|
| `!phase status` | Rolle, Welt, Phase und Sperre ins Log |
| `!phase scan` | Neue Klasse-Funktions-Paare aus `ProcessEvent` ins Log, erneut aufrufen zum Abschalten |
| `!phase filter off` / `on` | Fensternachrichtenfilter aus/an. Mit `off` sperrt nur `DisableInput`, so lässt sich das einzeln prüfen |
| `!phase stop` / `auto` | Adapter aus und eigene Sperre entfernen, bzw. wieder an |

## Tests

```powershell
powershell -NoProfile -File tests/Run-RoundPhase.ps1
powershell -NoProfile -File tests/Run-RoundPhaseRuntime.ps1
```

Ohne Spiel geprüft (C++20 `/W4 /WX`): Codec und Empfänger, Transport zwischen zwei eigenen Testprozessen und der echte Adapter aus `Mission.cpp` mit eigenen Speicherpuffern. Der Adaptertest deckt Hostquellen samt Objektfilter, Lager nur nach Rundenende, „Zurück“ auf der Route, Grund aus `TriggerEndRound`, Client-Bindung, `DisableInput`/`EnableInput` auf Controller und Figur, Nachrichtenfilter, Hostverlust und `!phase stop/auto` ab. Er schreibt `round-host.txt`, also nicht während einer Spielsitzung laufen lassen.

Im Spiel, nur über `Start-Session.ps1 -Build -Godmode`, Host und Client auf einem PC, Seed `20795`, echte Route (`Start-Run.ps1`, `Travel-Day.ps1`):

- **Erste Route in der MetaBase:** Der Client wird beim Öffnen der Karte gesperrt. Mit `!phase filter off`, also nur `DisableInput`, bewegt sich die Client-Figur bei 1,5 s gedrücktem W nicht. Nach „Zurück“ beim Host ist der Client frei und läuft mit derselben Taste rund 500 Einheiten.
- **Rundenende an Tag 1, 2 und 3** über `!finish`: Beim Client erscheinen Rundenübersicht, Lager und Routenkarte in derselben Reihenfolge wie beim Host, jeweils mit Sperre. In der nächsten Welt ist er wieder frei, an Tag 3 auch mit Bewegung geprüft. `WP_Round_Stats_C` bestätigt die Rundenübersicht im Spiel unabhängig von `FinishMission`.
- **Sperre und Gegenprobe:** Mit Sperre öffnet Esc beim Client nichts, ohne Sperre an Tag 2 öffnet Esc das Pausenmenü. Das Banner liegt laut `WindowFromPoint` vorn über dem Client-Fenster und zeigt den Text des Hosts.
- **Hostverlust:** `!phase stop` beim Host, nach 5 s entfernt der Client Banner und Sperre.
- **Belohnungsauswahl mitten in Tag 4:** Das Inventar-Panel öffnet beim Host, der Client bleibt frei, wie gewollt.
- Kein Absturz, keine Absturzberichte.

## Grenzen

- **Transport:** nur Datei auf einem PC, ohne Authentisierung. Den Snapshot über das Netz zu tragen ist Aufgabe von `transport` und `protokoll`.
- **Natürliches Tagesende:** Nicht beobachtet. An Tag 4 kamen nach dem Ablauf des Timers immer wieder Belohnungsauswahlen dazwischen. Die Rundenübersicht erkennt der Adapter aber am Panel, nicht am Auslöser.
- **Gamepad:** ohne echtes Gerät nicht getestet. `DisableInput` greift am Input-Stack, also unabhängig vom Gerät. Den Nachweis bringt erst ein Test mit Controller.
- **Kurze Lücken:** Wählt der Host in der MetaBase einen Knoten, schließt die Karte. Der Client ist dann bis zum Kartenwechsel, unter einer Sekunde, frei. Nach `!phase auto` mitten in einer Phase meldet der Host bis zum nächsten Panelwechsel `Playing`.
- **Pausen in der Mission**, etwa durch die Belohnungsauswahl, gehören zu `pausen-sync`, nicht hierher.
