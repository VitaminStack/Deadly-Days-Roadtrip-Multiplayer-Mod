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
  (PreviousRoute = CurrentRoute, CurrentRoute = Knoten), aufgerufen vom Klick auf einen Knoten.

## Fix (PR `task/opt-routenkarte`)

- **Lokal bleibt** die ganze Karte beim Client: Missionsstruktur, Biome, Knoten, Ereignisse, Linien und Bus erzeugt er selbst aus dem
  Seed. Auch das Weiterführen der Route am Tagesende macht sein eigenes `FinishMission`.
- **Geteilt wird** nur, woraus das Spiel die Karte baut: Seed, Gefahrenzuschlag, Kennung, Index, gemerkter Index und Ende der aktuellen
  Route, Kennung der vorigen Route und die Wahl. Neue Nachricht `0x403`, `ModVersion` 20
  ([Kontrollkanal](../CONTROL_PROTOCOL.md#route-mod-kompatibilität-20)). Jeder Client bekommt sie einzeln (Sync je Peer).
- **Es entscheidet** der Host: Er wählt die Route. Der Client übernimmt sie direkt vor dem Nachziehen des Tagesendes
  (`FollowRoundEnd`), baut die Struktur mit `GetMissionStructure` und setzt `CurrentRoute` und `PreviousRoute` mit `FRoute::operator=`
  aus seinen eigenen Einträgen. OpenPanel sieht beim Client für diesen einen Aufruf die Rolle Authority. Ein Klick auf einen Knoten
  setzt beim Client keine Route (`0x59056C0`), die Reise sperrt weiter `ClientMayStartTravel`.
- **Eigene Run-Info:** Vor der ersten Übernahme sichert der Client Seed, Zuschlag, Wahl und beide Routen. Sobald die Welt wechselt
  (Reise, Hauptmenü, Trennung), stellt er sie zurück. Der Run-Stand des Clients bleibt ohnehin gesperrt (`RunSaveChangeAllowed`).
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

## Abnahme

Offen.
