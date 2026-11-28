# Inventar, Lager, Airdrop und Handel

Gilt für Build `26786272`. Datierte Befunde und Messungen stehen im [Archiv](../archive/GAME_ANALYSIS.md), Abschnitte unten unter „Historie“.

## Überblick

- Koop-Modus (`coop-modus-wahl`): Der Host wählt im Overlay (F8) über „Lobby erstellen“ zwischen „Eigene Inventare“ (Standard) und
  „Geteiltes Inventar“. Der Modus steht in den Lobby-Daten (`ddcoop_mode`), jeder Client liest ihn beim Beitritt; Host und Client
  zeigen ihn im Overlay und schreiben ihn ins Log. Fest ist er ab „Lobby erstellen“ bis zum Ende der Sitzung und während eines Runs
  (außerhalb von Hauptmenü und MetaBase). **Ist-Zustand:** Ein geteiltes Inventar gibt es noch nicht, im Modus „Geteiltes Inventar“
  hat weiter jeder Spieler sein eigenes Inventar wie unten beschrieben. Der Modus steht nicht im Spielstand: Nach einem Neustart des
  Spiels steht er auf „Eigene Inventare“, im Hauptmenü ist er wieder wählbar. **Vermutung, nicht geprüft:** Setzt der Host einen
  gespeicherten offenen Run nach dem Hauptmenü fort, kann er ihn so mit einem anderen Modus weiterspielen
  ([Kontrollprotokoll](../CONTROL_PROTOCOL.md#koop-modus-mod-kompatibilität-23)).
- Inventar-Unterobjekte laufen über den klassischen Replikationspfad.
- Der Client bearbeitet sein Inventar lokal mit den nativen Abläufen. Beim Bestätigen schickt er einen Edit, den der Host als einzige Autorität übernimmt oder ablehnt (Airdrop-Auswahl, Lager, Händler, Werkbank, Kopierladen).
- Kopierladen (`opt-kopierladen`): Eine Kopie des Clients geht sofort als Edit an den Host. Der Host kopiert seine eigene Instanz des Originals
  (Daten, Seltenheit, Zustand), legt sie im Inventar des Clients ab und bucht den Preis des Panels; die Kopie repliziert wie jede Vergabe
  ([Kopierladen beim Client](../../src/Game/README.md#kopierladen-beim-client)).
- Die Airdrop-Auswahl läuft ohne Koop-Modus für beide: Der Host schickt das Sitzungsbild, der Client wählt lokal.
- Item-Fähigkeiten der Client-Figur (Heil-Kit, Granate, Roboterarm) laufen beim Host, weil das Spiel sie nur für lokal gesteuerte Figuren ausführt.
- Power-ups (`opt-powerups-client`): Die Slots aller Figuren füllt und löst der Host aus, Power-up-Fähigkeiten (Dosenessen, Level-Up) stellt
  er vorher auf `ServerOnly`. Der Client bucht beim Aufsammeln nichts, zeigt den Slot-Stand des Hosts (`0xE00`) und meldet das Auslösen
  eines eigenen Slots (`0xE01`); Taste, Klang und Anzeige bleiben lokal ([Kontrollprotokoll](../CONTROL_PROTOCOL.md#power-ups-mod-kompatibilität-22)).
- Ein Desync-Melder vergleicht offene Bearbeitungen mit dem Stand des Hosts.
- Wiederbeitritt und Hot Join (`wiedereintritt-inventar`): Der Host merkt sich das Inventar des Clients je Steam-Identität und ersetzt nach
  einer Trennung das Startinventar des neuen PlayerState damit, mit denselben nativen Funktionen wie bei einem Edit. Keine Nachricht. Der
  Schrott ist ein gemeinsames Konto der Mission und bleibt beim Host; die Rüstung des Astronauten kommt mit dem Anzug-Item zurück. Schlüssel
  ist das Steam-Konto hinter dem Peer (`Net::Transport::PeerAccount`). Powerups und Item-Modifikatoren sind nicht enthalten
  ([Fallakte](../faelle/wiedereintritt-inventar.md)).

## Code

| Datei | Aufgabe | Test |
|---|---|---|
| `src/Net/CoopMode.h`, `src/Net/Session.cpp`, `ModeBlocker` in `src/Mod/Lobby.cpp`, Auswahl in `src/UI/UI.cpp` | Koop-Modus wählen, in den Lobby-Daten übertragen, sperren | `tests/NetTests/SessionTests.cpp`, `tests/UITests/OverlayModelTests.cpp`, `tests/UITests/WindowTests.cpp` |
| `src/Game/InventoryPatches.cpp` | Inventar-Unterobjekte, Lebensdauer-Fixes, Itemsack-Vergabe | `tests/GameTests/PatchTests.cpp` |
| `src/Game/InventoryEdit.cpp`, `src/Game/InventoryEditPlan.h`, `src/Game/InventoryEditRuntime.h` | Lokale Inventarbearbeitung des Clients | `tests/GameTests/InventoryEditTests.cpp` |
| `src/Game/RewardPatches.cpp`, `src/Game/RewardAdapter.h`, `src/Sync/Inventory/RewardSync.cpp` | Airdrop-Auswahl, Lager, Händler, Kopierladen, Edits und Ablehnung | `tests/NetTests/RewardTests.cpp` |
| `src/Game/PatchPolicy.h` (`ClientCopyStep`) | Kopierladen: wann der Client lokal kopiert | `tests/GameTests/PatchTests.cpp` |
| `src/Game/AirdropPatches.cpp`, `src/Game/AirdropAdapter.cpp`, `src/Game/AirdropPolicy.h` | Airdrop-Kisten | `tests/GameTests/AirdropTests.cpp` |
| `src/Game/ItemAbilityPatches.cpp`, `src/Game/ItemAbilityPolicy.h` | Item-Fähigkeiten der Client-Figur beim Host | `tests/GameTests/ItemAbilityPolicyTests.cpp` |
| `src/Game/PowerUpPatches.cpp`, `src/Game/PowerUpPolicy.h`, `src/Sync/Players/PowerUpState.h` | Power-up-Slots der Client-Figur beim Host, Aufsammeln beim Client nur angezeigt | `tests/GameTests/PowerUpPolicyTests.cpp`, `tests/NetTests/PowerUpTests.cpp` |
| `src/Game/HandPreview.h` | Vorschau der Hände | `tests/GameTests/HandPreviewTests.cpp` |
| `src/Game/DesyncAdapter.cpp`, `src/Sync/DesyncSync.cpp` | Desync-Melder | `tests/NetTests/DesyncTests.cpp` |
| `src/Game/RejoinPolicy.h`, `TickRejoin` in `src/Game/RewardPatches.cpp` | Inventar des Clients über Trennung und Wiederbeitritt | `tests/GameTests/RejoinPolicyTests.cpp` |

Mehr: [src/Game/README.md](../../src/Game/README.md).

## Offene Fehler

- [airdrop-auswahl-absturz](../faelle/airdrop-auswahl-absturz.md): Client stürzte bei einer Airdrop-Auswahl ohne Inventar am Panel ab. Inventar-Panels bauen sich ohne Inventar nicht mehr auf (`SetupInventory` und `ForceUpdateFromComponent`); warum das Panel ohne Inventar kam, ist offen.
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
