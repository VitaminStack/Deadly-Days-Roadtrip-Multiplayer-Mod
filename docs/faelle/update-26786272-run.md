# update-26786272-run

## Bestätigungstest (Stand)

Live-Test 24.11.2026, zwei PCs: Run über 14 Karten bis zur HQ-Karte mit Händler, Labyrinth, Kopierladen und Boss, Händlerkäufe und Lager übernommen; Rezept an der Werkbank abgelehnt. Offen: Run-Route.ps1 selbst, Rezept-Panel, Item-Abschaltung, [Live-Test 24.11.](../GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-24112026-livetest-2411)

## Aufgabe und Abnahme

Rest aus `update-basis-26786272`: Auf Build `26786272` (Patch 1.0.3) ist die Mod bisher nur auf einem PC bis in die Mission und über ein Rundenende gemessen (57 Patches gesetzt). Offen: `Run-Route.ps1 -Days 2` bzw. Lauf über die Routenkarte, Händler, Werkbank und Rezept-Panel (1.0.3 ändert `URecipePanel`, `URecipeButton`, `UInventoryPanel` und `UItemRepresentation`), Aufwertung und Verkauf im Lager beim Client, ein Boss, Sitzung nur mit Mod (`-NoSpike`) über mehrere Tage. Dabei prüfen, ob die neue Item-Abschaltung (`IsItemSwitchingOffItem`, `DeactivatedIcon`; Zweck ist eine Vermutung) beim Client stimmt. Mitprüfen: die offenen Punkte der entfallenen Tests `update-1-0-2-lebensleisten`, `update-1-0-2-waffensound` und `update-1-0-2-spike` (Lebensleisten nach Schaden, Nachladesound beider Figuren hörbar, `Run-Route.ps1 -Days 2` mit Spike). Nur Fehler, die den Run verhindern, als eigene Aufgaben eintragen. **Fertig, wenn** ein Lauf über mindestens zwei Tage mit Händler und Werkbank auf `26786272` ohne Absturz und ohne Inventar-Abweichung belegt ist, erst auf einem PC, dann einmal auf zwei PCs (zweite Person hilft beim Test). Ordner `src/Game`. **Einstieg:** [Build 26786272](../archive/OFFSETS-ALTE-BUILDS.md#build-26786272-update-basis-26786272)
