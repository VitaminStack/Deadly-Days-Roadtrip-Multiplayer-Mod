# Dauerlauf mit Mod und Latenz (`dauerlauf`)

Lauf am 09.11.2026 auf einem PC, Build `26495302`, Mod mit Spike und Lobby-Tunnel, Netzprofil `Internet`
(120 ms Ping, 30 ms Schwankung, 2 % Verlust), Godmode für beide Spieler. Tag 1 bis 5 mit
`Run-Route.ps1 -Start -NetProfile Internet`, ab Tag 6 Knoten aus der Routenkarte mit `Travel-Day.ps1`, Missionen mit `!finish`
abgekürzt. Endboss an Tag 8, danach Endlosmodus.

**Ergebnis:** 12 Tage ohne Absturz, kein `NewActor failed`, Inventare nach jedem Tag gleich. Tag 1 bis 10 per Skript gemessen,
Tag 11 und 12 vom Nutzer weitergespielt und ohne Probleme gemeldet. Die Aufgabe wurde vom Nutzer damit als abgeschlossen akzeptiert,
13 Tage wurden nicht erreicht.

## Wachstum je Tag

Speicher als Private Bytes in MB (`check_round.py`, Feld `private_mb`), NetGUID-Cache des Clients als Einträge (`guid_entries`)
und Höchststand der Slots (`guid_slots`).

| Stand | Host MB | Client MB | GUID-Einträge | GUID-Slots |
| --- | ---: | ---: | ---: | ---: |
| MetaBase | 1380 | 1541 | 792 | 792 |
| Tag 1 | 1803 | 1652 | 1795 | 1795 |
| Tag 2 | 1700 | 1692 | 2723 | 2723 |
| Tag 3 | 1882 | 1751 | 4145 | 4145 |
| Tag 4 | 1744 | 1746 | 4558 | 4558 |
| Tag 5 | 1622 | 1588 | 4127 | 4572 |
| Tag 6 | 1858 | 1732 | 6567 | 6567 |
| Tag 7 | 1808 | 1766 | 7024 | 7024 |
| Tag 8 (Endboss) | 1738 | 1698 | 5513 | 7080 |
| Tag 9 (Endlos, Siedlungen) | 1773 | 1761 | 5247 | 7080 |
| Tag 10 | 1888 | 1860 | 7500 | 7500 |

- Speicher schwankt zwischen 1,6 und 1,9 GB je Instanz ohne stetigen Anstieg über die Tage.
- Der NetGUID-Cache wächst bis Tag 7 und fällt danach zweimal. Die Slots bleiben unter 7600. Kein Hinweis auf unbegrenztes Wachstum.
- `uobjects` liest `NumElements` des `GUObjectArray`. Der Wert bleibt ab Tag 2 gleich, weil freie Slots wiederverwendet werden.
  Er taugt nur als Höchststand, nicht als Zahl lebender Objekte.

## Befunde

- **Tag 5:** Das erste `!finish` beendete die Mission nicht, ein zweiter Versuch schon. `Complete-RoundEnd` hat dafür keine
  Wiederholung. Nur Testwerkzeug, kein Spielfehler.
- **Tag 6:** Biomwechsel mit Belohnungswahl „Nimm 1 von 3 Items“, Welt dabei pausiert. Bis zur Bestätigung beider Spieler
  meldet die Prüfung Pause und ungleiche Inventare, danach sauber. Erwartetes Verhalten.
- **Tag 8:** Figuren nicht am Bus, wie schon in `guidcache` beim Endboss dokumentiert.
- **Endlos-Start:** Beim Client zeigt die Rundenübersicht danach „Tag 0/8“ und nur Nullwerte, der Host ist schon im Lager.
  Fortsetzen und Lager funktionieren. Gehört zu `routenkarte-client` und `run-ende` (Run-Info beim Client nicht repliziert).
- **Sieg-Bildschirm:** Der Host meldet den Erfolg „Erster Apfel“. Ob dabei ein Steam-Erfolg ausgelöst wurde, ist nicht geprüft.
- **Werkzeug:** `Test-RouteMap` erkennt die Routenkarte ab dem Endlosmodus nicht, die Reise per Klick klappt trotzdem.
