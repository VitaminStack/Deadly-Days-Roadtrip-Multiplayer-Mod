# Itemsack-Absturz

Aufgabe `itemsack-absturz`, Build `26187885`, 05.11.2026.

## Ursache und Schutz

Zwei erhaltene Client-Crashberichte aus `client-visuell` zeigen denselben Stack:
`0x5959809` → `0x59AB0B3` → `0x571465A` → Blueprint-Aufruf →
`0x572F25E` → `0x59B8263`. Zugriff auf Adresse `0xF0`.

**Statisch beobachtet (lokale MSVC-Disassembly und SDK-Deklarationen):**
Der Exec-Thunk bei `0x57145E0` reicht `Actor+0x2A8` als
`ILootableInterface` an dessen virtuellen Loot-Eintrag weiter.
Die ItemLoot-Implementierung bei `0x59AB040` führt zunächst den Basis-Lootpfad
aus und ruft dann `CreateItemInstance` (`0x5959700`) mit
`ItemLoot.ItemData` (`Actor+0x2E0`) auf. Diese Referenz ist im SDK nicht als
repliziert deklariert. Bei `0x5959809` wird sie ohne Nullprüfung dereferenziert.
Das ist keine vollständige Rekonstruktion der Blueprint-Logik.

**Fix in `src/Game/InventoryPatches.cpp`:** Hook am Beginn von `ItemLoot.Loot`,
vor Basis-Loot, Item-Erzeugung, Inventaränderung und Statistik.
Nur ein Authority-Actor außerhalb einer Netzwerk-Client-Welt darf das Original
ausführen. Damit bleibt auch eine lokal erzeugte Authority-Kopie beim Client
ohne eigene Item-Vergabe. Der originale Host-Pfad erhält denselben Looter;
keine Ersetzung durch Spieler 0, kein zusätzlicher RPC und kein lokales Ersatzitem.
Kraftstoff, Airdrops und andere Loot-Klassen bleiben unverändert.

Eine Nullprüfung nur in `CreateItemInstance` wäre nicht ausreichend: Der
Aufrufer benutzt auch dessen Rückgabe ungeprüft. Fehlende ItemData durch
irgendein lokales Item zu ersetzen würde falsche oder doppelte Gewinne erzeugen.

REA konnte die Exe identifizieren (SHA-256 wie in `docs/OFFSETS.md`), aber
nicht decompilieren: Der konfigurierte JDK-Pfad fehlt. Keine Toolkonfiguration
geändert. Die obigen statischen Aussagen stammen aus MSVC-Disassembly und SDK,
nicht aus erfolgreicher REA-Decompilierung.

## Tests

`tools/Build-Mod.ps1 -Test`: 109 C++-Testfälle und 124 Python-Tests bestanden.
Der neue Regeltest prüft Standalone und Host sowie alle Actor-Rollen beim
Client, insbesondere die lokale Authority-Kopie. Das beweist noch keine
Kollision, Replikation oder korrekte Spielerzuordnung im laufenden Spiel.

### Spielabnahme

Eigene geschützte Sitzung mit `Start-Session.ps1 -Mod -Tunnel Lobby -Godmode
-Brief`, anschließend echte Route mit Seed `20795` bis Tag 1. Zwei Instanzen
auf einem PC, getrennte Test-Spielstände, kein Zwei-PC-/Internet-Test.
Beide Logs: `Patches: 30 gesetzt`. Beide Routenprüfungen sauber,
Inventare vor dem Test vollständig und gleich.

- **Client-Negativtest:** Aktueller Sack beim Client hat `Role=1`,
  `ItemData=null`. Direkter `ItemLoot.Loot`-Aufruf kehrt zurück; Schutzlog
  erscheint. Host- und Client-Inventare bleiben gegenüber vorher unverändert.
  Auch anschließend natürlich ausgelöste Client-Aufrufe bleiben ohne Absturz.
- **Isolierte Host-Vergabe:** Direkter Loot-Aufruf mit dem Host-LootAttractor
  erhöht ausschließlich dessen Inventar von 1 auf 2 Items; Remote-Spieler
  bleibt bei 1. Danach beide Inventarbilder vollständig und gleich.
- **Drei natürliche Aufsammlungen des Remote-Spielers in Folge:** Die Probe
  erzeugt jeweils einen Sack mit vorhandenem `ItemData_Pistol` neben der
  Remote-Figur und ruft **kein Loot** auf. Der reguläre Anziehungs-/Overlap-Pfad
  sammelt ein. Remote-Inventar nacheinander 8, 9 und 10 Items, Host jedes Mal
  unverändert bei 4; nach jedem Fall vollständige, gleiche Inventare auf
  Host und Client. Zuordnung über die aktuelle PlayerArray und PlayerId,
  nicht über ähnlich benannte Actors. Jeder Schritt hat genau einen neuen
  Eintrag, keine doppelte Client-Vergabe. Die vorherigen Gewinne erklären den
  höheren Ausgangsstand dieses Teiltests.
- **Grenzen/Fehlversuche:** Die erste Probenversion übersah das
  Funktionsbibliothek-CDO durch ihren Default-Object-Filter und erzeugte nichts;
  danach eigene Sitzung beendet, Probe korrigiert und neu gestartet.
  Ein früher Automatiklauf wartete auf einen außerhalb des Sammelbereichs
  vermuteten Sack vergeblich. Weitere Standort-Gegenproben erwarteten ein Item
  beim einen Spieler, gemessen wurde es beim anderen. Der Spawn-Standort allein
  legt den tatsächlichen Sammler nicht fest; bei nahen Figuren/mehreren Säcken
  sind diese Fälle ohne zusätzliche Beobachtung nicht isoliert. Sie sind keine
  bestandenen Zuordnungsfälle und kein Beleg einer falschen nativen Vergabe.
  Der strikte Test meldet solche Abweichungen weiterhin als Fehler.

Am Ende beide Prozesse in Tag 1 mit zwei Spielern verbunden, keine neuen
Absturzberichte. Sitzung mit `Stop-Session.ps1` beendet und Test-Mod entfernt.
Rohberichte/Inventarbilder unter `research/local/itemsack-absturz`; insbesondere
`natural-pickups.json` enthält die drei erfolgreichen Remote-Fälle.
Offene Inventar-/Reward-Auswahl-Synchronisierung ist nicht Teil dieses Fixes.

## Kontrollierte Probe

`Build-Probe.ps1` baut die eigene `ItemLootProbe.dll` nach `research/local`.
Nur in die Instanzen der eigenen Sitzung aus `Start-Session.ps1 -Mod` laden,
mit absolutem DLL-Pfad über `tools/spike/inject.py`. Kein Hot-Unload;
anschließend `Stop-Session.ps1`.

Die Probe läuft über eine Fensternachricht auf dem Game Thread. Befehle über
`<TEMP>/dditemsack/<pid>.cmd`, Ergebnisse in der entsprechenden `.log`:

| Befehl | Wirkung |
|---|---|
| `0` | Aktuelle lebende ItemLoot-Actors mit Rolle und ItemData lesen |
| `1` / `2` | Host erzeugt genau einen Itemsack 300 Einheiten neben Spieler 0 / 1, aus dessen erstem vorhandenen Inventaritem |
| `3` / `4` | Host ruft den nativen Lootpfad eines vorhandenen Sacks mit LootAttractor von Spieler 0 / 1 auf |
| `5` | Client ruft Loot auf einem vorhandenen Sack auf; mit Fix muss die Vergabe blockiert werden |

`1` bis `4` sind beim Client gesperrt. Die Probe erfindet keine Itemdaten;
fehlt eine Voraussetzung, wird nichts erzeugt. Host- und Client-Inventare
vor/nach jedem Fall mit `tools/spike/inventory.py` vergleichen.
Direkte Loot-Aufrufe sind ein isolierter Vergabetest und ersetzen nicht die
Abnahme mehrerer echter Aufsammlungen durch den Client.

`py -B research/spikes/itemsack-absturz/test_pickups.py` prüft standardmäßig
drei natürliche Remote-Aufsammlungen nacheinander mit exakten Inventardeltas.
Mit `--players 0 0` sind Host-Gegenproben möglich; Figuren vorher trennen.
Die Probe erzeugt nur Säcke, der Test ruft nie Loot auf und bewegt keine Figur.
Kommt kein Item oder erhält es ein anderer Spieler, endet er nach höchstens
20 Sekunden je Fall mit Fehler statt die Zuordnung zu erraten. Einzelne Läufe
schreiben eigene JSON-Dateien und bewahren auch fehlgeschlagene Deltas auf.
