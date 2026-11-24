# Inventar, Lager, Airdrop und Handel

Gilt für Build `26786272`. Datierte Befunde und Messungen stehen im [Archiv](../archive/GAME_ANALYSIS.md), Abschnitte unten unter „Historie“.

## Überblick

- Inventar-Unterobjekte laufen über den klassischen Replikationspfad.
- Der Client bearbeitet sein Inventar lokal mit den nativen Abläufen. Beim Bestätigen schickt er einen Edit, den der Host als einzige Autorität übernimmt oder ablehnt (Airdrop-Auswahl, Lager, Händler, Werkbank).
- Die Airdrop-Auswahl läuft ohne Koop-Modus für beide: Der Host schickt das Sitzungsbild, der Client wählt lokal.
- Item-Fähigkeiten der Client-Figur (Heil-Kit, Granate, Roboterarm) laufen beim Host, weil das Spiel sie nur für lokal gesteuerte Figuren ausführt.
- Ein Desync-Melder vergleicht offene Bearbeitungen mit dem Stand des Hosts.

## Code

| Datei | Aufgabe | Test |
|---|---|---|
| `src/Game/InventoryPatches.cpp` | Inventar-Unterobjekte, Lebensdauer-Fixes, Itemsack-Vergabe | `tests/GameTests/PatchTests.cpp` |
| `src/Game/InventoryEdit.cpp`, `src/Game/InventoryEditPlan.h`, `src/Game/InventoryEditRuntime.h` | Lokale Inventarbearbeitung des Clients | `tests/GameTests/InventoryEditTests.cpp` |
| `src/Game/RewardPatches.cpp`, `src/Game/RewardAdapter.h`, `src/Sync/Inventory/RewardSync.cpp` | Airdrop-Auswahl, Lager, Händler, Edits und Ablehnung | `tests/NetTests/RewardTests.cpp` |
| `src/Game/AirdropPatches.cpp`, `src/Game/AirdropAdapter.cpp`, `src/Game/AirdropPolicy.h` | Airdrop-Kisten | `tests/GameTests/AirdropTests.cpp` |
| `src/Game/ItemAbilityPatches.cpp`, `src/Game/ItemAbilityPolicy.h` | Item-Fähigkeiten der Client-Figur beim Host | `tests/GameTests/ItemAbilityPolicyTests.cpp` |
| `src/Game/HandPreview.h` | Vorschau der Hände | `tests/GameTests/HandPreviewTests.cpp` |
| `src/Game/DesyncAdapter.cpp`, `src/Sync/DesyncSync.cpp` | Desync-Melder | `tests/NetTests/DesyncTests.cpp` |

Mehr: [src/Game/README.md](../../src/Game/README.md).

## Offene Fehler

- [airdrop-auswahl-absturz](../faelle/airdrop-auswahl-absturz.md): Client stürzt bei Airdrop-Auswahl ohne Inventar ab.
- Was im Spiel noch zu prüfen ist: [Spicker](../ERLEDIGT.md#bestätigungstests-ausstehend), etwa `lager-edit-ablehnung` und `opt-metabase-inventar`.

## Historie

Wortgleiche Befunde, nur zum Nachschlagen, nicht als Ist-Zustand lesen:

- [Schrott beim Aufsammeln (`opt-schrott-anzeige`)](../archive/GAME_ANALYSIS.md#schrott-beim-aufsammeln-opt-schrott-anzeige)
- [Airdrop-Auswahl für beide Spieler (`airdrop-auswahl`)](../archive/GAME_ANALYSIS.md#airdrop-auswahl-für-beide-spieler-airdrop-auswahl)
- [Inventar-Unterobjekte: Untersuchung `inventar`](../archive/GAME_ANALYSIS.md#inventar-unterobjekte-untersuchung-inventar)
- [Rundenübersicht und Lager beim Client (`rundenende-client`)](../archive/GAME_ANALYSIS.md#rundenübersicht-und-lager-beim-client-rundenende-client)
- [Seltenheit und Aufwerten beim Client (`werkbank-client`)](../archive/GAME_ANALYSIS.md#seltenheit-und-aufwerten-beim-client-werkbank-client)
- [Heil- und Wurf-Items beim Client (`client-items-wirkung`)](../archive/GAME_ANALYSIS.md#heil--und-wurf-items-beim-client-client-items-wirkung)
- [Desync-Melder in offenen Bearbeitungen (`opt-desync-bearbeitung`)](../archive/GAME_ANALYSIS.md#desync-melder-in-offenen-bearbeitungen-opt-desync-bearbeitung)
- [Rasterposition außerhalb des Rucksacks im Desync-Melder (`opt-desync-pivot`)](../archive/GAME_ANALYSIS.md#rasterposition-außerhalb-des-rucksacks-im-desync-melder-opt-desync-pivot)
- [Roboterarm beim Client (`client-roboterarm`)](../archive/GAME_ANALYSIS.md#roboterarm-beim-client-client-roboterarm)
- [Run-Stand des Clients nach dem Run-Ende (`opt-metabase-inventar`)](../archive/GAME_ANALYSIS.md#run-stand-des-clients-nach-dem-run-ende-opt-metabase-inventar)
- [Run-Stand des Clients beim Verlassen (`client-runstand-abbruch`)](../archive/GAME_ANALYSIS.md#run-stand-des-clients-beim-verlassen-client-runstand-abbruch)
- [Rucksack-Überlappung beim Client (`opt-rucksack-ueberlappung`)](../archive/GAME_ANALYSIS.md#rucksack-überlappung-beim-client-opt-rucksack-ueberlappung)
- [Händler und Werkbank im Händler-Event (`opt-haendler-client`, `opt-werkbank-station`)](../archive/GAME_ANALYSIS.md#händler-und-werkbank-im-händler-event-opt-haendler-client-opt-werkbank-station)
- [Absturz beim Bestätigen im Lager (`lager-bestaetigen-absturz`)](../archive/GAME_ANALYSIS.md#absturz-beim-bestätigen-im-lager-lager-bestaetigen-absturz)
- [Lager-Edit des Clients mit Reparatur und Aufwertung (`inventar-client-abweichung`)](../archive/GAME_ANALYSIS.md#lager-edit-des-clients-mit-reparatur-und-aufwertung-inventar-client-abweichung)
- [Lager-Edit des Clients stabiler (`lager-edit-stabil`)](../archive/GAME_ANALYSIS.md#lager-edit-des-clients-stabiler-lager-edit-stabil)
