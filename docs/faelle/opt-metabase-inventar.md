# opt-metabase-inventar

## Bestätigungstest (Stand)

Live-Test 17.11.2026, Build 26669225: Sieg, Ergebnisbildschirm beim Client, eigener Run-Stand geleert, Host danach Level 1. Offen: neuer Run mit Level 1 und Startinventar bei beiden, Rollentausch, MetaBase-Items, [Live-Test 17.11.](../GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-17112026-livetest-1711)

## Aufgabe und Abnahme

Gefunden im Live-Test am 13.11.2026 abends (zwei PCs, Build `26495302`): Nach dem Sieg haben beide Spieler ihre Run-Items in der MetaBase noch. Am 14.11. begann der nächste Run beim neuen Host mit Level 35 statt 1, das Spiel skaliert die Gegner danach. **P1, spielbarkeitsrelevant:** Wer im Coop Client war, startet seinen nächsten Run als Host oder allein mit dem alten Level und Inventar, der Run ist damit kaum spielbar. Belegt für den Level-35-Weg: Der Client speichert bei jedem Tagesende seinen eigenen Run-Stand (`EndOfDay`), das Run-Ende läuft bei ihm nie nativ (`RunLossAllowed`), der Stand bleibt offen; der Fix leert ihn nach Sieg und Niederlage. Zweiter Träger: Die umgeleitete Reise behält Items und Level bis in die MetaBase; ob der echte Run-Start sie zurücksetzt, ist offen (braucht Maus und Tastatur). **Fertig, wenn** ein neuer Run zu zweit nach Niederlage und nach Sieg mit demselben Inventar und Level 1 beginnt wie solo, auch nach Rollentausch. Ordner `src/Game`. **Einstieg:** [Run-Stand des Clients](../GAME_ANALYSIS.md#run-stand-des-clients-nach-dem-run-ende-opt-metabase-inventar)
