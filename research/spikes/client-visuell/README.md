# Client-Darstellung: Projektile, XP-Orbs, Airdrops und Partikeleffekte

Aufgabe `client-visuell`, auf Nutzerwunsch vorgezogen, Build `26187885`.
**Abgeschlossen am 05.11.2026 auf Nutzerentscheidung.** XP-/Levelanzeige, XP-Bälle, Projektilbewegung und Airdrops sind in der Mod angebunden. Größe, Treffer-/Partikeleffekte und Randfälle stehen in `opt-client-visuell`.

## Umsetzung vom 05.11.2026: XP-Bälle und Projektile beim Client

**XP-Bälle.** `Game/ExpPatches.cpp` hängt am gemeinsamen Kern aller Ausstöße von
`UExpSubsystem` (Spawn-Anfrage, RVA in [OFFSETS](../../../docs/OFFSETS.md)). Der Host ruft das
Original auf und zeichnet die Anfrage auf: Menge, Start, Ziel, Farbe und zwei Zusatzwerte.
`ProgressSync` sendet sie als Kontrollnachricht `Orbs` (0x102), höchstens 256 je Tick, gebunden an
Client-Lebensdauer, Tag und GameState-GUID. Der Client verwirft eigene native Ausstöße und spielt die
Host-Anfragen über das Original nur zur Anzeige nach. `ReceiveExp` ist beim Client gesperrt, sein
XP-/Levelstand kommt weiter nur als Host-Snapshot.

- Fehlversuch: Der erste Hook auf `RequestExpSpawning` (0x58C1260) erfasste nur den
  Blueprint-Pfad. Natürliche Gegnertode laufen nativ direkt in den inneren Kern 0x58C1030.
- **[Beobachtet, Lobby, Tag 1]** Nach natürlichen Kills zeigt der Client die Bälle an derselben
  Stelle wie der Host („XP-Baelle: 1 Host-Ausstoss(e) angezeigt“). Die Client-HUD folgt dem
  Host-Level (1 → 2), kein eigener Gewinn. Die Bilder entstanden nacheinander, nicht im selben Frame.
- Nicht geprüft: Verschwinden beim Einsammeln durch den Client im Einzelfall, Kartenwechsel
  mitten im Flug, Verlust einer Nachricht. Überschneidet sich mit der offenen Aufgabe `xp-baelle`.

**Projektile.** Gepoolte `AProjectile` replizieren als Actor, aber ohne Bewegung. Der Client sah
seine Proxies versteckt, mit eigener Startgeschwindigkeit weit weggeflogen. `Game/ProjectilePatches.cpp`
schaltet beim Host einmal je Projektil-Actor im PersistentLevel `SetReplicateMovement(true)` und
`ForceNetUpdate` ein, Abgleich je Sekunde. Der Pool behält den Actor und damit das Flag.

- Fehlversuch: Ein Beobachter auf `OnTakeFromPool_BP` nach ProcessEvent sah den Aufruf beim Host
  nie. Das Flag blieb aus, das Ereignis läuft für diese Klassen offenbar nicht über ProcessEvent.
- **[Beobachtet, Lobby, erste Runde]** Das Flag kommt beim Client an. Der Nutzer bestätigte im
  Live-Vergleich dieselben Projektile in beiden Fenstern.
- Grenzen: Das erste Geschoss eines neuen Pool-Actors kann bis zu 1 s ohne Bewegung erscheinen.
  `BulletSize` wird weiterhin nicht übertragen. Treffer- und Todeseffekte sind nicht angebunden.

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

## Airdrop-Auswahl für beide Spieler: Befund vom 05.11.2026

Nutzerwunsch: Egal, wer einen Airdrop einsammelt, bei **beiden** öffnet sich die
Item-Auswahl. Beide bestätigen, erst dann läuft das Spiel weiter, und jeder erhält
seine eigene Belohnung. Noch nicht umgesetzt; dies ist der gemessene Ausgangsstand.

Eigene Sitzung `Start-Session.ps1 -Mod -Tunnel Lobby -Godmode`, zwei Instanzen auf
einem PC, Tag 1 über `Run-Route.ps1 -Days 1 -StayLast` (Seed 20795, Inventare
`complete=true`, `equal=true`). Level-up-Airdrops über die normale XP-Quelle
(Probe `3`), eingesammelt durch Hineinlaufen des Nutzers, einmal mit der Host- und
einmal mit der Client-Figur. Lesend gemessen mit Probe `7`/`9`, `pause_state.py`
und `inventory.py`, dazu Bilder beider Fenster.

| | Host sammelt ein | Client sammelt ein |
|---|---|---|
| Auswahl beim Host | Inventar-Panel der Host-Figur, „Nimm 1 von 3 Items“ | ebenfalls Panel der **Host-Figur**, 3 Belohnungen |
| Auswahl beim Client | keine | keine |
| Reward-Queue beim Host | – | `CurrentRewardObject` an der **Client-Figur** |
| Pause | Host: Requester `WP_Inventory_Panel`, blockierende UI-Sitzung. Beide Welten `TimeDilation≈0.0001` mit Pauser | gleich |
| Belohnung nach Bestätigen | beide neuen Items im selben Inventar (PlayerId 258) | |

- Die native Koop-Reward-Sitzung `UCoopInventoryCoordinatorComponent::RewardSession`
  blieb in beiden Fällen leer (`state=0`, keine Choices, `RequiredClaims=2` nur als
  Default). Die Auswahl läuft über die `RewardQueueComponent` der Figur und das
  normale `InventoryPanel` (`TriggerRewardDisplay`/`SetupRewardChoices`), nicht über
  `ClaimReward`.
- Die Weltpause erreicht den Client schon heute über die replizierten
  WorldSettings. Es fehlen eine eigene Auswahl beim Client und ein Host-Grund,
  der die Pause bis zur Bestätigung des Clients hält.
- **Fehler:** Egal wer einsammelt, wählt nur der Host-Spieler, und beide neuen Items
  lagen danach im selben replizierten Inventar (Host- und Clientbild gleich). Die
  Panel-Figur war Spieler 0. Das Inventar trug PlayerId 258; in der Folgesitzung
  gehörte 258 zum Remote-Client (PlayerArray-Index 1). Ob die IDs in beiden
  Sitzungen gleich vergeben waren, wurde nicht geprüft, die Zuordnung ist daher
  **nicht eindeutig**.
- Nach einer vom Client eingesammelten Kiste blieb `CurrentRewardObject` der
  Client-Reward-Queue auf dieser Kiste stehen. Der nächste Airdrop wurde bei
  Berührung durch den Client als `opened=1` markiert, ohne Auswahl, und ließ sich
  danach auch vom Host nicht mehr öffnen. Hängende Client-Queue ist naheliegend,
  aber nicht bewiesen.
- Der Client-Rucksack ist host-autoritativ repliziert. Ein lokales Hinzufügen beim
  Client wäre kein gültiger Weg: Die gewählte Belohnung und ihre Rasterposition
  müssen beim Host im Inventar der Client-Figur entstehen. Das berührt die
  Reward-Queue aus `inventar-sync`.

### Native Koop-Auswahl mit Remote-P2 (wie beim lokalen Koop/Remote Play Together)

Neue eigene Sitzung, gleicher Aufbau. Vor dem ersten Airdrop beim Host der Spike-
Befehl `!p2 on` und `!p2 joined` ([Remote-P2](../a-connect/RemoteP2.md)): Remote-
Controller als P2 in `ActivePlayers`, `SetCoopEnabled(true)`, aktive Spieler 2.

1. **Host sammelt ein:** Die native Koop-Reward-Sitzung startet:
   `state=2` (`AwaitingClaims`), 3 Choices, `eligible=[0,1]`, `RequiredClaims=2`,
   `opener=0`. Das Host-Panel zeigt beide Rucksäcke nebeneinander, die Pause wirkt
   wie oben auf beide Welten.
2. **Host legt ein Item in den P2-Rucksack und bestätigt:** Das Item steht im
   Inventar von PlayerArray-Index 1 (Remote-Client). Die Sitzung zählt den Claim
   aber als **Spieler 0** (`claimed=[0]`, `eligible=[1]`, 2 Choices übrig) und
   bleibt `AwaitingClaims`, die Pause bleibt. Der Host wählt damit für den Client;
   pro Spieler und Kiste gibt es genau einen Claim.
3. **Code-Claim für Spieler 1:** `ClaimReward(1, Choices[0])` beim Host liefert
   `true`; Sitzung `state=3` (`Completed`), `claimed=[0,1]`. Ohne Platzierung landete
   das Item (`FireShoes`) im Reservebereich („Lager“) von Inventar PlayerId 257
   (Index 0), nicht im P2-Rucksack.
4. **Panel hängt danach:** Das Host-Panel blieb offen mit ausgegrautem
   „Wähle eine Belohnung“, keine Eingabe möglich, Timescale-Request
   `WP_Inventory_Panel` und Pause blieben. Weder ein direkter Aufruf
   `InventoryPanel.HandleCoopRewardSessionChanged(Sitzung)` noch
   `AcknowledgeRewardUIClosed` (`false`) löste das. Sitzung danach verworfen.
5. **Fehlversuch `TransferItem`:** Quelle `RewardChoice`, Ziel `P2Backpack` mit dem
   P2-Inventar und freien P2-Slots (1×1, 1×2, 2×1) ergab für alle Choices
   `InvalidRequest`, vor und nach dem Host-Claim. Der Aufbau des Requests ist also
   noch falsch oder dieser Weg ist für Reward-Choices nicht vorgesehen.

**Folgerung:** Das lokale Koop-System übernimmt das Schwierige bereits nativ:
eine gemeinsame Sitzung pro Kiste, ein Claim je Spieler, Pause bis zum letzten
Claim, Platzierung in den richtigen Rucksack. Für den Online-Koop fehlen noch:

- dauerhafte Remote-P2-Bindung in der Mod statt nur im Spike (`remote-spieler`),
- Host-Panel auf die eigene Hälfte (Spieler 0) begrenzen,
- beim Client eine Auswahl aus dem Host-Sitzungsbild (Choices, Level, offene
  Claims) mit lokaler Platzierung,
- Client-Wahl und Rasterposition über den Kontrollkanal; der Host platziert im
  P2-Rucksack und ruft `ClaimReward(1, …)` auf,
- der native Weg, wie das Panel nach dem P2-Claim schließt, vermutlich die
  Übergabe der UI-Autorität an P2 (`RequestUIAuthorityTransfer`), die laut
  Remote-P2-Befund ohne lokales P2-Gerät noch blockiert.

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
`FinishSpawningActor` für Actor-/Bewegungsreplikation anmelden, `6`/`8` = einen
ungeöffneten Airdrop beim Host mit der Figur von Spieler 0/1 öffnen, `7` =
Koop-Reward-Sitzung lesen, `9` = Inventar-Panels und Reward-Queues lesen.
`10` = `ClaimReward(1, erste Wahl)`, `11` = Wahl per `TransferItem` in freie P2-Slots legen und beanspruchen, `12` = Sitzung an offene Inventar-Panels melden, `13` = `AcknowledgeRewardUIClosed`. `2`, `7` und `9` sind beim Client erlaubt; die übrigen Befehle brauchen den Listen-Host.
Eine zweite Kopie für eine bereits laufende Sitzung lässt sich mit anderem
`PROBE_DIR` bauen, damit nicht zwei Probes dieselbe Befehlsdatei lesen. `5` ruft keine
Reward-Öffnung auf und ist noch kein Produktionsfix. Danach die Sitzung beenden,
nicht die DLL heiß entladen.
Rohberichte, Bilder und Binärdateien bleiben unter `research/local` bzw. im
temporären Probe-Verzeichnis; keine Spieldaten oder SDK-Dateien im Repository.

Die REA-Decompilierung war hier nicht verfügbar: Der Server verweist auf einen
nicht vorhandenen JDK-Pfad. Keine Konfiguration geändert. SDK-Felddeklarationen,
lesende Prozessabfragen und begrenzte lokale Disassembly wurden genutzt; daraus
wird keine vollständige Rekonstruktion der Niagara-/Pool-Funktionen behauptet.
