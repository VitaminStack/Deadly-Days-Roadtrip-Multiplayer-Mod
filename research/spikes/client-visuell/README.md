# Client-Darstellung: Projektile, XP-Orbs und Partikeleffekte

Aufgabe `client-visuell`, auf Nutzerwunsch vorgezogen, Build `26187885`.
**In Arbeit, kein fertiger Produktionsfix und keine bestandene visuelle Abnahme.**

## Ziel und Grenze

Der Client soll dieselben Projektile, XP-Orbs und Kampfpartikel wie der Host sehen:
Spawn, Bewegung, Größe, Treffer, Einsammeln und Verschwinden. Schaden, Loot und
XP-Vergabe bleiben ausschließlich beim Host. Bereits replizierte Actors werden
nicht zusätzlich lokal erzeugt. `kampf`, `fortschritt` und `vfx-sound` bleiben offen.

## Beobachtungen vom 04.11.2026

Eigene Sitzung mit `Start-Session.ps1 -Mod -Godmode`, zwei Instanzen auf einem PC,
Loopback, Seed `20795`, echter Menüweg über die MetaBase und Routenkarte nach Tag 1.
Vor Beginn keine laufende Spielinstanz. Die Inventarprüfung nach dem Missionswechsel
meldete `complete=true`, `equal=true`. Getrennte Testspielstände und CrashGuard aktiv.

- **[Beobachtet, Bilder]** In der laufenden Mission zeigt der Host große
  Treffer-/Todeseffekte und Schadenszahlen; entsprechende große Effekte fehlen
  im Clientbild. Die Bilder entstanden nacheinander, nicht im gleichen Frame.
  Das bestätigt die bereits dokumentierte Lücke bei nicht replizierten Pool-Actors,
  ist aber kein neuer vollständiger Nachweis sämtlicher Effektarten.
- **[Beobachtet, CPU-Daten]** Auf beiden Seiten gibt es im aktuellen PersistentLevel
  einen `BP_ExpEmissionHandler_C` mit einem `NS_ExpOrbVFX`-Emitter. Im ersten
  Vergleich standen die Arrays für Start, Ende, Werte und Farben beim Host jeweils
  auf 1, beim Client jeweils auf 0. `ExpData` und `PendingExpData` waren auf beiden
  Seiten leer. Diese gespeicherten CPU-Arraygrößen sind **keine Zählung lebender
  Niagara-Partikel** und kein Nachweis, wann einzelne Orbs eingesammelt wurden.
- **[Beobachtet, Projektile]** Die untersuchten gepoolten Host-Projektile hatten
  `bReplicateMovement=false`. Versteckte Poolobjekte lagen beim Host auf einer sehr
  weit entfernten Parkposition. Bei Client-Proxies wurden andere Positionen
  beobachtet. Deshalb genügt Actor-Replikation allein nicht als Beleg für einen
  korrekten Schuss-/Pool-Lebenszyklus. Die bekannte fehlende Übertragung von
  `BulletSize` bleibt davon getrennt; siehe [Blueprint-Probe](../blueprint-logik/README.md#konkrete-befunde).

## Kontrollierter Fehlversuch: nur Bewegungsreplikation einschalten

`VisualProbe.cpp` ruft auf dem Game Thread ausschließlich beim Listen-Host für
bereits existierende Projektile `Actor.SetReplicateMovement(true)` auf. Keine
Default-Objekte, keine neuen Actors, keine XP- oder Schadensaufrufe.

Im Versuch änderte sich das Flag bei zwei Host-Projektilen. Damit ist der Setter
ausgeführt, **nicht** die Darstellung repariert: Der unmittelbar folgende
Client-Snapshot enthielt noch alte Proxies ohne das Flag; ein späterer Snapshot
enthielt keine Projektile im ausgewählten PersistentLevel. Ohne framegenaue
Beobachtung von aktivem Schuss, Pool-Rückgabe und Wiederverwendung ist weder
Synchronität noch ein erfolgreicher Fix belegt. Die extremen Pool-Parkpositionen
sind ein weiterer Grund, den Schalter nicht pauschal in die Mod zu übernehmen.

**Konsequenz:** Dieser Versuch wird nicht automatisch installiert und verändert
keinen Produktionscode. Die eigene Sitzung wurde danach vollständig beendet und
die Test-Mod mit `Stop-Session.ps1` entfernt. Kein Hot-Unload der Probe.

Ein erster Sitzungsstart wurde nach 120 Sekunden vom ausführenden Werkzeug
abgebrochen; beide Prozesse waren danach beendet. Der erneute Start mit längerer
Werkzeugfrist erreichte die MetaBase und anschließend die Mission. Ein erster
Ladeversuch der Probe mit relativem DLL-Pfad scheiterte; mit absolutem Pfad
erfolgreich. Diese Fehlversuche sind keine Spielabstürze.

## Nächste Umsetzungsschritte

1. Projektile während **aktiver** Schüsse und Pool-Wiederverwendung vergleichen;
   Position, Richtung, Größe und Lebenszyklus getrennt beobachten. Kein pauschales
   Nachspielen von `OnReturnToPool`, das beim Host auch Impact-/Gameplaypfade erreicht.
2. XP-Orbs als Niagara-Darstellung behandeln, nicht voreilig als replizierte
   `BP_EXPOrb`-Actors. Hostseitige Erzeugung, Zielbewegung und Abschluss müssen als
   zusammenhängender Lebenszyklus nachgeliefert werden; der Client darf dabei
   `ReceiveExp` oder die Belohnungslogik nicht erneut ausführen.
3. Für tatsächlich fehlende Host-Daten den vorgesehenen Kontroll-/Sync-Kanal aus
   `protokoll` anbinden. Der vorhandene Transport tunnelt nur UE-Pakete unverändert;
   es gibt noch keine öffentliche Schnittstelle für eigene kosmetische Daten.
   Kein zweites Ad-hoc-Protokoll und keine gemeinsame Datei als vermeintliche
   Internet-Lösung einführen. Diese Integration ist noch offen.
4. Treffer-/Todeseffekte separat rein kosmetisch ausgeben, replizierte Flächen-/Gift-
   Actors nicht duplizieren. Danach kontrollierte Aufnahmen vor/nach dem Fix,
   Pool-Reuse, Kartenwechsel, Trennen und erneuter Verbindung prüfen.

## Werkzeuge

```powershell
py -B tools/spike/visual_state.py <host-pid> <client-pid>
py -B -m unittest discover -s tests -p test_visual_state.py
powershell -NoProfile -ExecutionPolicy Bypass -File research/spikes/client-visuell/Build-Probe.ps1
```

`visual_state.py` liest nur aktuelle PersistentLevel-Actors und kennzeichnet
unlesbare Arrayheader/Flags als unbekannt statt als leer. Versteckte und bereits
zerstörende Actors bleiben ausdrücklich unterscheidbar. Kein Vergleich absoluter
Prozessadressen und keine Behauptung framegenauer Gleichheit.

Die optionale Probe nur in eine **eigene geschützte Sitzung** mit absolutem
DLL-Pfad laden. Befehl `1` über `<TEMP>/ddvisual/<host-pid>.cmd`; beim Client wird
die Änderung abgelehnt. Danach die Sitzung beenden, nicht die DLL heiß entladen.
Rohberichte, Bilder und Binärdateien bleiben unter `research/local` bzw. im
temporären Probe-Verzeichnis; keine Spieldaten oder SDK-Dateien im Repository.

Die REA-Decompilierung war hier nicht verfügbar: Der Server verweist auf einen
nicht vorhandenen JDK-Pfad. Keine Konfiguration geändert. SDK-Felddeklarationen,
lesende Prozessabfragen und begrenzte lokale Disassembly wurden genutzt; daraus
wird keine vollständige Rekonstruktion der Niagara-/Pool-Funktionen behauptet.
