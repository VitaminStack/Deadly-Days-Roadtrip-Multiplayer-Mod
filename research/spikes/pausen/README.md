# Pausen: SDK-Befunde und begrenzter Methodenversuch

Build `26187885`. Kein fertiger Pause-Sync. Untersuchung mit selbst erzeugtem
Mission-SDK, zwei Instanzen auf einem PC und direkter Loopback-Verbindung.
Das Spiel wurde ausschliesslich mit `tools/spike/Start-Session.ps1` gestartet,
direkt in `DDDynamic_Map`, mit getrennten Testspielstaenden und Crash-Schutz.

## Was das SDK aussagt

- `UTimescaleManager` ist ein GameInstance-Subsystem. Es verwaltet Requests pro
  Requester, einen aktiven Requester und `OnGamePauseChanged`. `FTimeRequest`
  enthaelt Zeitfaktor, Prioritaet und `PauseRegardlessOfTimescale`.
- `UPauseScreenManager` verwaltet Oeffnungsanfragen und Blocker getrennt, hat
  einen eigenen `FTimeRequest` und einen Verweis auf den TimescaleManager.
- `UAbstractPanel` hat **getrennte** Schalter fuer Coop-UI-Sitzung,
  `bCoopBlocksGameplay` und `PauseGameWhileOpen`. Ein blockierendes Panel ist
  deshalb nicht schon nach dem Property-Namen als Timescale-Request nachgewiesen.
- `UCoopInputRouterSubsystem::SessionStack` speichert Sitzungen mit Session-/Parent-ID,
  UI-Autoritaet und `bBlocksGameplay`. UI-Autoritaet ist keine Netzwerkautoritaet.
- `ADDMissionGameState::SetFreezeTime` verwaltet noch eine weitere Request-Schicht.
  `UWorldSettings::TimeDilation`, `CinematicTimeDilation` und `PauserPlayerState`
  sind dagegen laut Engine-SDK Netzwerk-Properties. Die lokale Requestverwaltung
  darf nicht mit ausschliesslich lokaler Wirkung gleichgesetzt werden.
- Das native und Blueprint-Inventarpanel hatte im Default-Objekt
  `bUseCoopUISession=true`, `bCoopBlocksGameplay=true`, `PauseGameWhileOpen=false`.
  Das sind Default-Werte, kein Nachweis eines erfolgreichen Oeffnungspfads.

## Versuche und Ergebnis

1. **Read-only-Snapshot beider Prozesse:** Beim Host war bereits ein Tutorialdialog
   aktiv. Timescale-Requester: `DialogComponent`, eine belegte Request-Zelle, eine
   UI-Sitzung mit `bBlocksGameplay=true`. Client: kein Requester, keine belegte
   Request-Zelle, keine lokale UI-Sitzung. Auf **beiden** Seiten standen
   `TimeDilation` bei etwa `0.0001` und `PauserPlayerState` auf einem Spieler.
   Screenshot des Hosts bestaetigte den Tutorialdialog.
   Das belegt den Zustand, nicht den genauen verursachenden nativen Aufruf.
2. **Inventar-Methodentest:** Der separate Probe-Spike fand keinen geeigneten
   Inventar-Animationscontainer. `OpenPanel` wurde deshalb nicht ausgefuehrt.
   Kein daraus abgeleiteter Netzwerk- oder Pausenerfolg.
3. **Level-up-Methodentest:** `GameState->GetLevelSystem()->RequestLevelUp()` lief
   auf dem Fenster-/Game-Thread ohne Absturz durch. Die Snapshots blieben beim
   bestehenden Dialogrequest; kein neuer Sitzungseintrag und keine isolierte
   Level-up-Pause nachgewiesen. Der direkt geladene, bereits pausierte Testaufbau
   ist fuer den normalen XP-/Reward-Ablauf nicht ausreichend.

**Ergebnis:** Die Aussage „Timescale wirkt nur lokal“ ist nicht bestaetigt.
Die UI-Sitzungen kommen in diesem Test nicht beim Client an, waehrend der
Welt-Pausenzustand auf beiden Seiten vorhanden ist. Kein offener Inventar- oder
normaler Level-up-Ablauf wurde erfolgreich bis zum Schliessen durchgespielt.

## Werkzeuge und Wiederholung

`tools/spike/pause_state.py <host-pid> <client-pid>` liest nur Speicher. Die Ausgabe
ist nicht atomar; alte Objekte koennen noch im Objektarray liegen. Default-Werte
werden gesondert ausgewiesen, Archetypen werden aus den Instanzmessungen entfernt.
Rohdaten und Screenshots bleiben in `research/local/`.

`Build-Probe.ps1` baut `PauseProbe.cpp` gegen das **lokale** SDK nach
`research/local/pausen`. Nach dem Bauen eine eigene geschuetzte Testsitzung starten
und die Probe nur dort per `tools/spike/inject.py` laden. Die Probe liest
`<TEMP>/ddpause/<pid>.cmd`: `1` fordert ein Level-up an, `2` versucht das
Inventarpanel zu oeffnen, `3` es zu schliessen. Ueber eine eigene Fensternachricht
erfolgen die SDK-Aufrufe auf dem Fenster-/Game-Thread; die bestehende WndProc wird
weiter aufgerufen. Nur ein fester Kartenaufenthalt, keine Mapwechsel und kein
Hot-Unload waehrend des Tests. Zum Entfernen die **eigene** Sitzung mit
`Stop-Session.ps1` beenden. Kein Release-Loader und keine allgemeine Objektsuche.

Die Probe kann mangels Animation auf ein unpassendes altes Objekt stossen; dieser
Fall wird als Fehlversuch protokolliert. Sie umgeht natuerliche Eingaben und darf
nicht als Test der normalen Menue-/XP-Kette beschrieben werden.

## Weiteres Vorgehen

Mit einer normal ueber das Menue gestarteten Mission und abgeschlossenem Tutorial
je einen echten XP-Level-up und eine Inventaroeffnung/-schliessung messen. Dabei
Requests, SessionStack, Input-Sperren, WorldSettings und Host-/Client-Fortschritt
vorher/waehrend/nachher vergleichen. Auch verschachtelte Fenster, beide Spieler,
Disconnect und Mapwechsel pruefen. Die Roadmap-Zeile bleibt bis dahin offen.
