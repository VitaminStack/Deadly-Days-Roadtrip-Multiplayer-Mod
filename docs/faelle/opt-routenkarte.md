# opt-routenkarte: Routenkarte des Clients bleibt leer

## Reproduktion

Seit `routenkarte-client` (07.11.2026, [Archiv](../archive/GAME_ANALYSIS.md#routenkarte-beim-client-routenkarte-client)): Nach dem
Rundenende zeigt die Routenkarte des Clients nur Wolken, Hintergrund und den Bus oben links mit „999“. Nachstellen auf einem PC: Tag 1
mit dem Rezept „Mehrere Tage ohne echte Eingaben“ aus [TESTING.md](../TESTING.md) beenden, die Karte beider Seiten mit
`Game-Input.ps1 <Rolle> shot` (nur Bild aus dem Fenster) vergleichen.

## Belege

- [Belegt, Disassembly 26786272] Die Rollenprüfung sitzt in `UBusPanel::OpenPanel` (`0x5ABA540`), nicht in `OpenMapIfRequired`
  (`0x5ABA2D0`), wie das Archiv vermutete. OpenPanel kopiert `CurrentRoute` und ruft nur mit `ROLE_Authority` des eigenen Controllers
  `SetChoices(GetMissionStructure(-1))`. Danach setzt es den Merker „Karte überspringen“ (`+0x358`), wenn die Route noch Missionen hat.
  `OpenMapIfRequired` öffnet die Karte nur ohne diesen Merker.
- [Belegt, Reflexion im Spiel] Layout von `FCurrentRunInfo`, `FRoute` und `FPossibleMissionGroup` in [OFFSETS.md](../OFFSETS.md#routenkarte-opt-routenkarte).
  Die Missionsstruktur hat 8 Gruppen (je Tag eine, Biom, `PossibleRoutes`). `CurrentRoute` des Hosts ist eine Kopie von
  `Struktur[Gruppe].PossibleRoutes[Route]` samt Index; die Kennung ist `(Gruppe, Route)`. `ChosenMissionIndexes` ist die gewählte
  Knotenposition je Tag (0, 2 oder 4).
- [Belegt, Speicher Tag 1] Host: Seed des Runs, Kennung `(0, 0)`, eine Mission, Index 0, gemerkter Index -1, gewählt `[0]`. Client:
  eigener Seed, Kennung `(0, 0)`, keine Mission, Index -1, gewählt `[]`. Die Run-Info repliziert nicht.
- [Belegt, Disassembly] `GetMissionStructure` (`0x58EFD10`) baut den Cache des Subsystems bei jedem Aufruf aus Seed und Run-Info neu.
  `SetChoices` (`0x5AC0820`) liest Seed, `CurrentRoute` und `ChosenMissionIndexes`. Gesetzt wird die Route nativ in `0x59056C0`
  (PreviousRoute = CurrentRoute, CurrentRoute = Knoten). Aufrufer sind `LoadMapByInformation` (`0x5B1B260`, Klick auf einen Knoten)
  und `LoadNonMissionMap` (`0x5B1B4F0`, etwa die Rückkehr ins Hauptmenü über das Pausemenü).
- [Belegt, Spiel] Die Zahl unter der Stoppuhr (Gefahrenstufe) setzt die Haupt-UI über `WP_UI_C.OnThreatLevelChanged_Event(Old, New)`,
  das wie das Ereignis der Stoppuhr am Delegate `ThreatLevel.OnThreatLevelChanged` hängt. `0xC00` (`opt-bedrohung-sound`) rief beim
  Client nur das Ereignis der Stoppuhr (Animation und Sound): Der Client zeigte an Tag 4 „0“, der Host „7“, an Tag 5 „0“ gegen „9“,
  obwohl das Log „Stoppuhr von 6 auf 7“ meldete. Die Archiv-Vermutung, die Zahl komme aus der Route (`GetDisplayThreatLevel`), stimmt
  nicht: Das ist die Gefahr je Knoten auf der Karte (`ThreatStatLevel + ThreatDisplayBonus`, 0 bis 4).

## Fix (PR `task/opt-routenkarte`)

- **Lokal bleibt** die ganze Karte beim Client: Missionsstruktur, Biome, Knoten, Ereignisse, Linien und Bus erzeugt er selbst aus dem
  Seed. Auch das Weiterführen der Route am Tagesende macht sein eigenes `FinishMission`.
- **Geteilt wird** nur, woraus das Spiel die Karte baut: Seed, Gefahrenzuschlag, Kennung, Index, gemerkter Index und Ende der aktuellen
  Route, Kennung der vorigen Route und die Wahl. Neue Nachricht `0x403`, `ModVersion` 20
  ([Kontrollkanal](../CONTROL_PROTOCOL.md#route-mod-kompatibilität-20)). Jeder Client bekommt sie einzeln (Sync je Peer).
- **Es entscheidet** der Host: Er wählt die Route. Der Client übernimmt sie direkt vor dem Nachziehen des Tagesendes
  (`FollowRoundEnd`), baut die Struktur mit `GetMissionStructure` und setzt `CurrentRoute` und `PreviousRoute` mit `FRoute::operator=`
  aus seinen eigenen Einträgen. OpenPanel sieht beim Client für diesen einen Aufruf die Rolle Authority. Ein Klick auf einen Knoten
  läuft beim Client nicht bis `LoadMapByInformation`: keine Route, keine Reise. Das Panel graut die Knoten danach aus wie beim Host
  nach seiner Wahl, und der Blueprint hängt vorher einen Eintrag an `ChosenMissionIndexes` an; den überschreibt die nächste Übernahme
  oder die Rückstellung.
- **Gefahrenstufe im HUD:** Der Client gibt die Stufe aus `0xC00` zusätzlich an `WP_UI_C.OnThreatLevelChanged_Event`
  (`src/Game/ThreatAdapter.cpp`). Das gilt für jeden Stand, auch einen unveränderten, sobald die Haupt-UI neu ist oder ihn noch nicht
  hat; als Haupt-UI zählt die Instanz im Viewport. Direkt nach der Reise lebt noch die UI der alten Welt; der erste Stand
  `(N, N)` landete dort, und der Client zeigte am Tagesstart 0 (gemessen an Tag 2: Host 3, Clients 0). Kein neues Format.
- **Eigene Run-Info:** Vor der ersten Übernahme sichert der Client Seed, Zuschlag, Wahl und beide Routen. Sobald die Welt wechselt
  (Reise, Hauptmenü, Trennung), stellt er sie zurück, über die gemerkte GameInstance (beim Weltwechsel ist die Welt kurz 0). Der Run-Stand des Clients bleibt ohnehin gesperrt (`RunSaveChangeAllowed`).
- **Run-Ende:** Am Sieg und an der Niederlage übernimmt der Client nichts. Mit der vollen Route würde sein lokales `FinishMission`
  den Sieg selbst buchen.
- **Wiederverwendet** werden der Weg des Rundenendes (`FollowRoundEnd` in `MapPatches.cpp`) und die Karten-Id aus `0x400`.
- **Regeln** in `src/Game/RouteMapPolicy.h`, getestet in `tests/GameTests/RouteMapTests.cpp`; Format in `tests/NetTests/RouteTests.cpp`.
  Die nativen Stellen sind der Patch „Routenkarte des Hosts beim Client“ der eigenen Liste `RouteMap`; fehlt eine, bleibt die Karte
  des Clients leer wie bisher.

## Prüfliste (ARCHITECTURE.md)

- Lokal: Struktur, Karte, Darstellung, Weiterführen der Route. Geteilt: die Run-Info-Werte oben, weil sie nicht replizieren und sich
  nicht ableiten lassen. Entscheidung: Host. Wiederverwendet: Rundenende `0x401` und Karten-Id `0x400`.
- Verlust und Wiederholung: Der Host wiederholt den Stand alle 5 s und schickt ihn einem neuen Client sofort. Ohne Route zur Karte
  bleibt die Karte leer, mit Warnung im Log. Neuaufbau: Die Route gilt nur für die Karte, auf der der Host sie gelesen hat.

## Abnahme (26.11.2026, ein PC, Host und Client über die Lobby der Mod, Godmode, nur Spike-Befehle und Fensterbilder)

- Ein Run von der MetaBase bis Tag 7 mit dem Rezept „Mehrere Tage ohne echte Eingaben“, Biome Siedlungen, Vororte (ab Tag 3) und
  Wälder (ab Tag 5). Nach jedem Tagesende (Tag 1, 2, 3, 4 und 6) zeigte die Karte des Clients dieselben Knoten, Biome, Ereignisse,
  gefahrenen Linien, denselben Bus und dieselbe Tagesspalte wie der Host (Fensterbilder; nach Tag 1 bis 3 auch gezählt: höchstens
  0,25 % abweichende Bildpunkte durch Animation, ohne die Fußzeile mit dem Knopf-Symbol). `route_probe` (Lesen der Run-Info) meldete beim Client nach dem
  lokalen `FinishMission` dieselbe aktuelle Route samt Index und Ende wie beim Host. Die vorige Route hat beim Client den Stand aus
  der Struktur (gemerkter Index -1), beim Host den gefahrenen; im Bild ohne Unterschied.
- Tag 5 lag auf einer Route mit zwei Missionen: Am Ende von Tag 5 übersprang der Host die Karte und reiste direkt weiter, der Client
  folgte ohne Karte und ohne Warnung.
- Ein Klick des Clients auf jeden Knoten (`callat 0` bis `20`) änderte seine Route nicht und löste keine Reise aus.
- Gefahrenstufe: über `DD_SetThreatLevel` 5 und 8 zeigten Host und Client dieselbe Zahl unter der Stoppuhr (vorher Client 0).
- In jeder Mission stand beim Client wieder seine eigene Run-Info (eigener Seed, leere Route), `check_round.py` sauber an allen Tagen.
  Verlassen über das Pausemenü bei offener Karte: Client im Hauptmenü mit eigenem Seed und leerer Route, Log „eigene Run-Info
  zurückgestellt“. `run_save.py`: Run-Stand des Clients leer. Keine Absturzberichte.
- Zweite Sitzung mit Host und zwei Clients (`-Clients 2`), Tag 1 bis 3: Beide Clients zeigen nach Tag 1 und 2 die Karte des Hosts
  (höchstens 0,45 % abweichende Bildpunkte, im Bild gleich) und dieselbe Run-Info. Die Gefahrenstufe am natürlichen Tagesstart, ohne
  Cheat: Tag 2 bei allen dreien 2, Tag 3 bei allen dreien 4 (Log „Haupt-UI zeigt Stufe“ rund 2 s nach der Reise).
- Nicht geprüft: zwei PCs, Wiederbeitritt während der Karte, Run-Ende (dort übernimmt der Client nichts, nur Unit-Test), Bossknoten
  am Ende der Route, Endlosmodus. Ebenso das erneute Öffnen der Karte nach einem Klick des Clients: „Zurück“ ließ sich über den
  Spike nicht auslösen. Der Client schreibt dafür die Wahl beim Öffnen aus dem übernommenen Stand neu.
