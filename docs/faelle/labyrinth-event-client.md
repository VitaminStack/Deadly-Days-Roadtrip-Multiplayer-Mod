# labyrinth-event-client

## Bestätigungstest (Stand)

Live-Test 24.11.2026, zwei PCs: zweiter Lauf ohne Fallen (Karte nach 187 ms, beide Figuren im Gang), Weiterreise ohne Absturz. Offen: ein weiterer Lauf ohne Fallen, [Live-Test 24.11.](../archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-24112026-livetest-2411)

## Aufgabe und Abnahme

Gefunden im Live-Test am 17.11.2026 (zwei PCs, Build `26669225`): Im Event `MGP_LabyrinthEvent` fällt der Client durch die Karte, im Bild ist alles schwarz bis auf die eigene Figur. Der Client übernahm die Kartenwerte des Hosts, setzte aber Seeds für 0 ISM-Komponenten (normale Karten 7) und meldete, die fremde Figur stehe auf einer ihm unbekannten Unterlage. Erstes Auftreten des Events im Coop; ob das erst mit 1.0.2 kam, ist offen. Vermutung: Das Labyrinth baut seine Geometrie anders, und der Client erzeugt sie nicht. **Fertig, wenn** der Client im Labyrinth-Event Boden und Wände sieht und sich bewegen kann, normale Karten unverändert laufen, auf beiden Builds. Ordner `src/Game`. **Einstieg:** [Live-Test 17.11.](../archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-17112026-livetest-1711)
