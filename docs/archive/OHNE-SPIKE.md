> **Archiv.** Historischer Stand, nicht aktuell. Gilt nur, wenn eine aktuelle Doku darauf verweist.

# Mod ohne Spike (Stand 05.11.2026)

Abschnitt aus [src/Game/README.md](../../src/Game/README.md). Überholt: Die Mod braucht den Spike im Spiel nicht mehr, der Spike ist nur noch Testwerkzeug.

## Ohne Spike

Im Release gibt es keinen Spike. Stand `ohne-spike` (05.11.2026, Build `26187885`): Die Mod läuft allein, Lobby-Beitritt, MetaBase und
Mission an Tag 1 ohne Absturz, geprüft mit `Start-Session.ps1 -NoSpike` und `Run-Route.ps1 -Start -NoSpike -Days 1 -StayLast`,
siehe [Nur Mod, ohne Spike](../TESTING.md#nur-mod-ohne-spike). Der Absturz beim Beitritt ohne Spike (Host `0x5901CE8`,
Client `0x56B16D9`) stammte aus der Zeit vor `patches`, heute setzt die Mod beide Fixes beim Spielstart.

**Für den Betrieb nötig, aber nur im Spike.** Das läuft dort über Dateien in `%TEMP%\ddspike`, also nur auf einem PC. In die Mod
gehört es mit Übertragung über den [Kontrollkanal](../CONTROL_PROTOCOL.md), das ist Arbeit der genannten Aufgaben, nicht ein
Kopieren des Spike-Codes.

| Spike-Datei | Eingriff | Ohne ihn | Aufgabe |
|---|---|---|---|
| `Mission.cpp`, `RoundPhase*.h`, Beobachter in `Widgets.cpp` | Rundenübersicht, Lager und Routenkarte des Hosts beim Client mit Anzeige und Eingabesperre | Client bleibt in der Missionsansicht, während der Host im Lager oder auf der Routenkarte ist | `pausen-sync`, `tagesablauf` |

**Schon in der Mod**, der Spike lässt seine Fassung aus, sobald `DDCoopPatchState` 1 meldet: `Players.cpp`, `Client.cpp`, `Camera.cpp`,
`Inventory.cpp`, `Travel.cpp`, `Widgets.cpp` (HUD wartet auf den GameState), `MapObjects.cpp` (PowerUp-Kiste), Busplatz aus `TestEnv.cpp`,
`HealthBars.cpp`, `RunEnd.cpp`, `MapGen.cpp` und `Pcg.cpp` (Werte über den Kontrollkanal statt über Dateien). Dazu ohne Gegenstück im Spike-Schalter: Sperre des Crash-Reporters (`Core/CrashReports`) und
Tutorial-Hinweise (`TutorialAdapter`, im Spike `Cheats.cpp`). `Tunnel.cpp` ersetzt der Transport aus `src/Net`.

**Nur Testhilfe oder Messung**, kommt nicht in die Mod: Befehlsdatei und Konsolenbefehle (`Main.cpp`, etwa `open`, `servertravel`,
`DD_SetCurrentRunSeed`; Ersatz mit Antworten ist der Testkanal `src/Mod/Control.cpp`, Aufgabe `mod-steuerung`), `Cheats.cpp` (CheatManager, `!load`), Godmode, `!finish` und `!round`
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
