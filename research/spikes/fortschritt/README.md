# Fortschritt: Bestand und Live-Abnahme

Stand 09.11.2026. Loot, XP, Level-up-Airdrops und Aufwertungen sind durch erledigte Aufgaben belegt:
`itemsack-absturz`, `airdrop-rucksack-live`, `inventar-transaktion`, `client-visuell`, `xp-baelle-live`,
`bestaetigen-alle`, `airdrop-warten`, `werkbank-client`. Offen ist nur die Live-Abnahme der Status-Effekte.

## Live-Test zu zweit (etwa 30 Minuten)

Gebündelt mit den Bestätigungstests `update-welt` (XP-Bälle, Dash am Kamerarand) und
`lager-handel-client` (Verkauf, Reparatur im Lager), weil alle drei denselben Lauf bis Tag 2 brauchen.
Ablauf und Start wie in [Test auf zwei PCs](../../../docs/TESTING.md#test-auf-zwei-pcs).

Spielaktionen macht der Mensch, Messungen der Agent per Skript.

1. Tag 1: Gegner besiegen. Prüfen: XP-Bälle beim Client sichtbar, XP und Level auf beiden gleich (`update-welt`).
2. Client dasht an den Kamerarand. Prüfen: Figur bleibt in der gemeinsamen Kamera (`update-welt`).
3. Giftwolke: Ein Spieler nutzt ein Item mit `BP_Effect_Add_Poison_Cloud_C`, der Client steht darin.
   Prüfen: Wolke beim Client sichtbar, Client verliert HP, HP-Leiste auf beiden gleich, Wirkung endet auf beiden.
4. Item mit dauerhaftem Effekt (Werte-Bonus) im Rucksack des Clients ablegen und wieder entfernen.
   Prüfen: Attribut am Client-Pawn beim Host und beim Client gleich, nach Entfernen zurück.
   `MovementSpeed` ist als bekannte Lücke in `opt-tempo-upgrades` ausgenommen.
5. Rundenende, Lager: Client verkauft und repariert ein Item. Prüfen: Schrott und Items gleich,
   repariertes Item ausrüstbar (`lager-handel-client`).

## Abnahme

`fortschritt` ist fertig, wenn Schritt 3 und 4 ohne Abweichung und ohne Absturz laufen.
Abweichungen bekommen eine neue Kennung in `TODO.md`, kleine Darstellungsfehler unter Optionale Optimierungen.

## Ergebnisse

Noch kein Lauf.
