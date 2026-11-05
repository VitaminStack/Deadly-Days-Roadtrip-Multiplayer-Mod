# Client-Darstellung: Projektile, XP-Orbs, Airdrops und Partikeleffekte

Aufgabe `client-visuell`, auf Nutzerwunsch vorgezogen, Build `26187885`.
**In Arbeit. XP-/Levelanzeige ist jetzt in der Mod angebunden; die gesamte visuelle Abnahme ist noch offen.**

## Umsetzung vom 05.11.2026: gemeinsamer XP-Anzeigestand

- `src/Net/Protocol.*` und der erweiterte Transport liefern den reservierten
  Kontrollkanal aus `protokoll`, kein Dateisync und kein zweiter UDP-Kanal.
  Handshake, Build-/Mod-Version und Gegenrolle werden vor dem Spielverkehr geprüft.
  Die Lobby und der Testschalter stellen denselben Transport für Sync bereit.
- `Game/ProgressAdapter.*` liest beim Host ausschließlich die nativen Getter des
  gemeinsamen Levelsystems. Beim Client werden nur die aktuelle Spieler-HUD und
  ihr Fortschrittsbalken aktualisiert. **Kein `AddExperience`, `ReceiveExp`,
  `RequestLevelUp`, Reward- oder Save-Aufruf beim Client.**
- `Sync/ProgressState.h` / `ProgressSync.cpp` übertragen vollständige, absolute
  Bilder mit Host-Run, Epoche, monotoner Revision und einer clientseitigen
  Anforderungskennung pro lokaler Welt-/Sitzungslebensdauer. Der GameState wird
  zusätzlich über seine bestehende **UE-NetGUID** gebunden, nicht über ähnlich
  benannte Actors oder lokale FName-/Objektindizes. Alte und doppelte Bilder,
  fremde Verbindungstoken und Antworten für eine andere Welt werden verworfen.
- Bestätigter nativer Befund in der geschützten Tag-1-Sitzung: Beide PlayerStates
  des Hosts liefern exakt dasselbe Levelsystem wie der GameState. Der Client hat
  dagegen eine lokale Instanz mit altem Stand. Die Mod behebt die daraus falsche
  Anzeige, statt zwei Gewinne in das gemeinsame Host-System zu buchen.
- In der ersten Gegenprobe über `-Mod -Tunnel Mod` kamen laufende XP-/Level-
  Änderungen in der Client-HUD an. Nach kontrollierten XP-Quellen an den beiden
  serverseitigen Spielerpositionen zeigten beide HUDs Level 5. Die Quellen liefen
  nur beim Host; der native Client-Levelsystemstand wurde nicht manipuliert.
  Wegen gleichzeitigem Kampf und möglicherweise überlappenden Sammelbereichen ist
  dies **kein isolierter Nachweis, welcher Spieler welchen Orb einsammelte**.
- Vollständiger Build/Testlauf bestanden, einschließlich Tests für ungültige,
   doppelte, verspätete und weltfremde XP-Snapshots. NetGUID-Bindung und reguläre
   Lobby-Anbindung wurden anschließend ebenfalls im Spiel bestätigt.

Die native Client-Levelsystem-/Upgrade- und Reward-Steuerung bleibt Gegenstand von
`fortschritt`. Diese Anzeigeanbindung macht weder dessen komplette Abnahme noch
 die noch fehlenden Orb-, Projektil- und Partikeldarstellungen fertig.

## Einfache Airdrop-Replikation vom 05.11.2026

- `Game/AirdropAdapter.*` aktiviert beim Host die eingebaute Actor- und
  Bewegungsreplikation für vorhandene Airdrops. Keine zweite lokale Kiste.
  Kleine vollständige Snapshots ergänzen Öffnungs-, Bereitschafts-, Sichtbarkeits-
  und Animationszustand, zugeordnet über UE-NetGUID. Animation-Seeking löst keine
  Notifies aus. Client-Kopien erhalten keine eigene Kollisions-/Reward-Ausführung.
- Reguläre Steam-Lobby, zwei geschützte Instanzen auf einem PC: Ein normaler
  Level-up-Airdrop erschien sichtbar beim Client, an der Host-Position bis auf
  normale Replikationsquantisierung. Ein manuell erzeugter Testactor wurde nicht
  als Nachweis des normalen Lebenszyklus gewertet.
- Eine frühere Gegenprobe zeigte eine zurückgebliebene geschlossene Client-Kopie
  nach Host-Entfernung. Deshalb blendet die Mod zuvor verwaltete Kopien aus und
  deaktiviert ihre Kollision, sobald sie im vollständigen Host-Bild fehlen.
- Abschließende Gegenprobe: Auf beiden Seiten derselbe normale, bereite,
  sichtbare Airdrop. Native Host-Öffnung meldete `opened=1`; anschließend gab es
  auf beiden Seiten keinen lebenden Airdrop mehr. Kein sichtbarer Ghost in dieser
  Actor-Prüfung. Ob der ergänzende Hide-Fallback dabei zum Einsatz kam oder UE
  die Kopie direkt entfernte, ist nicht nachgewiesen.
- Build/Test: 100 C++-Testfälle (36 Core, 13 UI, 23 Game, 28 Net) und 118
  Python-Tests bestanden. Das ist keine vollständige visuelle Animationsabnahme,
  kein Zwei-PC-Test und kein Nachweis vollständiger Client-Reward-Auswahl.

**Vereinfachung auf Nutzerwunsch:** Eine zusätzliche lokale Client-Belohnung wäre
vorerst akzeptabel. Für die bisher geprüfte Sichtbarkeit und Host-Öffnung war sie
nicht nötig; die Schutz-Hooks bleiben deshalb aktiv. Selbständiges Client-Öffnen
und synchronisierte Belohnungsauswahl sind damit noch nicht implementiert.
Ohne etablierten Mod-Kontrollkanal oder aktive Schutzpatches bleibt die Anbindung
inaktiv. Die Gesamtaufgabe bleibt offen.

## Ziel und Grenze

Der Client soll dieselben Projektile, XP-Orbs, Airdrops und Kampfpartikel wie der Host sehen:
Spawn, Bewegung, Größe, Treffer, Einsammeln, Airdrop-Ankunft/-Öffnung und Verschwinden. Schaden, Loot und
XP-Vergabe bleiben ausschließlich beim Host (Airdrop-Ausnahme vorerst vom Nutzer
erlaubt, aber bislang nicht benötigt). Bereits replizierte Actors werden
nicht zusätzlich lokal erzeugt. `kampf`, `fortschritt` und `vfx-sound` bleiben offen.

**Nutzerergänzung vom 04.11.2026:** Airdrops sind im Client nicht sichtbar.
Dieser zusätzliche Fehler wurde inzwischen nachgestellt und wie oben beschrieben
angebunden. Die frühere [Airdrop-Probe](../blueprint-logik/README.md#konkrete-befunde)
hat bereits einen nicht replizierten Host-Airdrop ohne entsprechenden Client-Actor
belegt. Seine Öffnungslogik erzeugt Rewards; ein vollständiges Nachspielen beim
Client wäre daher kein sicherer Darstellungsfix.

**XP-Regel, vom Nutzer festgelegt:** Egal, wer einen Orb einsammelt, sein XP-Wert
zählt genau einmal zum gemeinsamen Run-Fortschritt. Beide Spieler sehen denselben
bestätigten XP-/Levelstand. Zwei Spieler am selben Orb oder eine wiederholte
Nachricht dürfen keinen zweiten Gewinn auslösen. Das ist eine gewünschte Mod-Regel,
kein neuer Nachweis des unveränderten Spiels; die Umsetzung gehört zu `fortschritt`.

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

**Konsequenz für den Schalterversuch vom 04.11.:** Dieser Versuch wird nicht automatisch installiert und verändert
keinen Produktionscode. Die damalige eigene Sitzung wurde danach vollständig beendet und
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
3. Den jetzt implementierten [Kontrollkanal](../../../docs/CONTROL_PROTOCOL.md)
   für die noch fehlenden kosmetischen Zustandsfamilien nutzen. Die vorhandene
   NetGUID-Auflösung erlaubt die eindeutige Zuordnung bereits replizierter
   Actors; sie erzeugt selbst keine neuen Actors und keine kosmetischen Events.
4. Treffer-/Todeseffekte separat rein kosmetisch ausgeben, replizierte Flächen-/Gift-
   Actors nicht duplizieren. Danach kontrollierte Aufnahmen vor/nach dem Fix,
   Pool-Reuse, Kartenwechsel, Trennen und erneuter Verbindung prüfen.
5. Airdrop-Darstellung für Ankunft, Landung, ungeöffnet/offen und Entfernen aus dem
   bestätigten Host-Zustand herstellen; auch bereits gelandete/geöffnete Airdrops
   bei spätem Beitritt richtig anzeigen. Belohnungserzeugung und Öffnungsfreigabe
   bleiben beim Host, keine zweite `OpenAirdropChoices`-Ausführung beim Client.
6. Zusammen mit `fortschritt` getrennte Aufnahmefälle prüfen: nur Host sammelt,
   nur Client sammelt, beide gleichzeitig am selben Orb, wiederholte Nachricht,
   verspätete Nachricht nach Reise. Erwartung jeweils ein gemeinsamer Gewinn und
   gleiche XP-/Levelanzeigen, kein mehrfacher Level-up durch Darstellungsereignisse.

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
DLL-Pfad laden. Befehle über `<TEMP>/ddvisual/<pid>.cmd`: `1` = verworfener
Bewegungsschalterversuch, `2` = native XP-/Shared-Identität lesen, `3`/`4` = je
200 XP am serverseitigen Spieler 0/1 anfordern, `5` = einen Test-Airdrop vor
`FinishSpawningActor` für Actor-/Bewegungsreplikation anmelden. Nur `2` ist beim
Client erlaubt; die übrigen Befehle brauchen den Listen-Host. `5` ruft keine
Reward-Öffnung auf und ist noch kein Produktionsfix. Danach die Sitzung beenden,
nicht die DLL heiß entladen.
Rohberichte, Bilder und Binärdateien bleiben unter `research/local` bzw. im
temporären Probe-Verzeichnis; keine Spieldaten oder SDK-Dateien im Repository.

Die REA-Decompilierung war hier nicht verfügbar: Der Server verweist auf einen
nicht vorhandenen JDK-Pfad. Keine Konfiguration geändert. SDK-Felddeklarationen,
lesende Prozessabfragen und begrenzte lokale Disassembly wurden genutzt; daraus
wird keine vollständige Rekonstruktion der Niagara-/Pool-Funktionen behauptet.
