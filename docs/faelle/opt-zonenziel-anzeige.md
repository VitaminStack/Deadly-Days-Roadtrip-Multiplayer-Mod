# opt-zonenziel-anzeige: Fortschritt des Zonenziels fehlt beim Client

Behoben mit Mod-Version 21: Der Host meldet den Stand des Zonenziels (`0xD00`), der Client zeigt ihn in der nativen Leiste.
Auf einem PC geprüft, der Test auf zwei PCs steht aus.

## Reproduktion

Ein PC, zwei Instanzen. In PowerShell
`DDCOOP_TEST_MAP_PROFILE=/Game/Map_Generation/Blueprints/MapEvents/TitanBlockage/MGP_TitanBlockage.MGP_TitanBlockage` setzen, dann
`Start-Session.ps1 -Build -Mod -Tunnel Lobby -Godmode -Map /Game/Maps/DDDynamic_Map -Brief`.
Der Host zeigt oben die Leiste „Töte alle Titanen!“ mit „0/10“, der Client ohne Fix nichts. Mit `-Godmode` sterben die zehn Titanen
ohne Eingaben in rund 80 s, danach verschwindet die Leiste des Hosts.

## Belege

- [Gemeldet] Live-Test 13.11.2026 abends (zwei PCs, Build `26495302`): Der Client sieht die Fortschrittsanzeige nicht, im Log keine Zeile zum Ziel.
- [Belegt, Asset-Index] Die Zone ist das Kartenprofil `MGP_TitanBlockage`; im selben Ordner liegen `BP_KillTitansObjective` und
  `BP_Titan_BlockEvent_Manager`. Das Schwesterprofil `MGP_HighwayBlockage` hat `BP_HighwayDestroyObjective`.
- [Belegt, Spiel, 26.11.2026, `26786272`, ein PC, vor dem Fix] Host: `DDObjectiveSubsystem.CurrentObjective` zeigt auf ein
  `BP_KillTitansObjective_C` (`DDKillEnemyTypeObjective`) mit Soll 10, die Anzeige `WI_ObjectiveDisplay_C` trägt es. Client: Das
  Subsystem hat kein Ziel und keine Instanz, die Anzeige ist leer. `day_state.py` meldet „Ziele 1“ gegen „Ziele 0“.
- [Belegt, Spiel] Der Actor `BP_Titan_BlockEvent_Manager_C` und seine `TitanBlockadeEventComponent` (erbt von
  `BlockadeEventComponent`) stehen auf beiden Seiten, beim Client lokal aus der eigenen Kartenerzeugung (Rolle Authority, nicht
  repliziert). Beim Client ist `ObjectiveClass` gesetzt (`BP_KillTitansObjective_C`), `ObjectiveInstance`, `CachedSpawner` und
  `TitanEnemies` sind leer: Die Komponente startet Ziel und Titanen nur beim Host.
- [Belegt, Spiel] Die zehn Titanen sind replizierte `BP_StandardEnemy_C`. Das Merkmal „Titan“ (`GA_TitanAbility_C`) gibt es nur beim
  Host, der Client kann Titanen darum nicht von anderen Gegnern unterscheiden und Tode nicht selbst zählen.
- [Belegt, Spiel] Abschluss beim Host: `CurrentObjective` wird leer, die Leiste verschwindet, das Objekt bleibt im Subsystem liegen
  und steht wieder auf 0 von 10.
- [Vermutung] Ziele außerhalb der Blockade-Zonen (HQ-Karten, etwa `DD_CollectKeycardObjective`) legt ebenfalls nur der Host an.
  Nicht gemessen; dem Client fehlt dort die Zonen-Komponente als Quelle der Klasse.

## Ursache

Das Zonenziel ist ein Objekt des `DDObjectiveSubsystem` der GameInstance. Die Zonen-Komponente legt es nur beim Host an, es
repliziert nicht, und der Zähler hängt an den Gegner-Spawnern des Hosts. Der Client hat darum nichts, was seine Anzeige zeigen könnte.

## Prüfliste ([ARCHITECTURE.md](../ARCHITECTURE.md#verbindliches-architekturprinzip-lokal-zuerst-minimal-synchronisieren))

- **Was bleibt lokal?** Die Klasse des Ziels (aus `BlockadeEventComponent.ObjectiveClass` der eigenen Karte), Beschreibung, Leiste und
  Anzeige (`WI_ObjectiveDisplay_C`). Der Client legt ein eigenes Objekt über `CreateAndStartObjective` ohne Autostart an und meldet es
  seiner Anzeige mit `OnObjectiveStarted`.
- **Was wird geteilt und warum?** Nur der Stand: Ziel läuft, Ist, Soll. Der Client kann ihn nicht ableiten, weil er Titanen nicht
  erkennt und ein Beitritt mitten in der Zone frühere Tode nicht kennt. Neue Nachricht `0xD00`, darum `ModVersion` 21
  ([Kontrollkanal](../CONTROL_PROTOCOL.md#zonenziel-mod-kompatibilität-21)).
- **Wer entscheidet?** Der Host. Das Objekt des Clients wird nie gestartet (`StartTracking` bleibt aus), zählt nicht selbst und
  bekommt nie das Soll gesetzt; Abschluss, Belohnung und Folgen der Zone laufen nur beim Host.
- **Welcher Weg wird wiederverwendet?** Das Muster der Bedrohungsstufe (`0xC00`): Stand statt Ereignis, bei Änderung und alle 2 s,
  an jeden Client. Die Leiste entsteht wie die Boss-Leiste, mit denselben Schutzregeln (nur Anzeigen mit Subsystem, Wartezeit vor
  dem ersten Wert).
- **Ablehnung, Wiederholung, Neuaufbau:** Ungültige Pakete verwirft der Client. Verlust, später Beitritt und Reise holt die
  Wiederholung nach; ein Weltwechsel verwirft den Stand und entfernt die alte Leiste. Getestet in `ZoneGoalPolicyTests.cpp` und
  `ZoneGoalTests.cpp`, im Spiel siehe unten.
- **Boss-Leiste:** Boss-Ziele meldet der Host nicht. Zeigt die Anzeige schon ein anderes Ziel, wartet das Zonenziel; entfernt wird
  nur an Anzeigen, die noch das eigene Ziel zeigen.

## Ergebnis im Spiel (26.11.2026, `26786272`, ein PC, zwei Instanzen, `-Mod -Tunnel Lobby -Godmode`)

- Titanen-Zone: Beide Fenster zeigen „Töte alle Titanen!“ mit demselben Wert (Bild bei „5/10“). Im Speicher folgte das Ziel des
  Clients dem Host von 0 bis 9; mit dem Abschluss beim Host verschwand die Leiste auf beiden Seiten. Client-Log: „lokal angelegt, 0
  von 10“, je Wert eine Zeile, „Leiste entfernt (Host meldet kein Ziel mehr)“.
- Zweite Reise in die Zone im selben Prozess: neue Leiste, kein Absturz.
- Neustart des Clients mitten in der Zone, mit `-NetProfile Internet` (rund 60 ms je Richtung, 2 % Verlust): Die Leiste entstand
  sofort mit dem Stand des Hosts (Bild „7/10“), lief bis 9 mit und verschwand mit dem Abschluss.
- `MGP_HighwayBlockage`: Der Client legt `BP_HighwayDestroyObjective_C` an und zeigt „0 von 10“ wie der Host. Fortschritt und
  Abschluss dort nicht gefahren (die Trümmer brauchen Eingaben).
- Das Anlegen sucht einmal im Objekt-Array und hielt den Frame dabei 9 bis 12 ms auf (einmal je Zone).
- Keine Absturzberichte.

## Offen

- Test auf zwei PCs, siehe [Bestätigungstests](../ERLEDIGT.md#bestätigungstests-ausstehend).
- Nicht geprüft: `MGP_LabyrinthEvent` (hat mit `LabyrinthBlockadeEventComponent` dieselbe Basis, [Vermutung] gleicher Weg), ein Boss
  in einer Blockade-Zone, drei Clients.
- Ziele der HQ-Karten: Der Host meldet auch deren Stand, dem Client fehlt die Klasse; sein Log sagt dann „Host meldet ein Ziel, die
  Karte des Clients nennt keine Klasse dafuer“. Eigene Aufgabe, falls das im Spiel auffällt.
