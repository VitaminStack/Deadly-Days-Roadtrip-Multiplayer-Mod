# TODO und Roadmap

Sobald das GitHub-Repo steht, werden die Aufgaben als Issues angelegt, und GitHub Issues/Projects ist die führende Liste.
Diese Datei hält dann nur noch Phasen und Gates fest.

**Vorgehen:** Das größte Risiko ist die Frage, ob native UE-Replikation im Spiel überhaupt noch funktioniert. Deshalb
wird das zuerst mit kleinen Spikes geprüft (Phase 1), bevor echte Mod-Infrastruktur entsteht.

| Priorität | Bedeutung |
|---|---|
| P0 | Blockiert andere Aufgaben |
| P1 | Nötig für das nächste Gate |
| P2 | Wichtig, aber später |

## Phase 0 – Projekt-Setup

Ziel: Alle können auf demselben Spielstand und mit derselben Toolchain arbeiten.

- [x] P0 Öffentliches GitHub-Repo anlegen
- [ ] P0 Freunde als Mitwirkende einladen
- [x] P0 Branch-Schutz für `main` aktivieren
- [ ] P0 Team-Aufteilung auf die [Arbeitsbereiche](CONTRIBUTING.md#arbeitsbereiche) festlegen, danach `CODEOWNERS` anlegen
- [x] P0 SDK für UE 5.8 erstellen. Dumper-7 brauchte dafür einen Fix, siehe [Analyse](docs/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58)
- [x] P0 SDK zusätzlich während einer laufenden Mission dumpen, damit die Blueprint-Klassen enthalten sind
- [ ] P2 SDK auch in einem Bosskampf dumpen, weil diese Klassen im Mission-Dump weitgehend fehlen
- [ ] P1 Dumper-7-Fix dem Dumper-7-Projekt vorschlagen, damit alle Helfer ihn ohne eigenen Patch bekommen
- [ ] P0 Visual-Studio-Solution mit einem Projekt je Modul anlegen, `config/Local.props` einbinden
- [ ] P1 Festlegen, wie das Team mit Spiel-Updates umgeht ([Update-Regel](CONTRIBUTING.md#spiel-updates))
- [ ] P1 Klären, ob alle das Spiel auf derselben Build-ID halten können

## Phase 1 – Analyse und Spikes

Ziel: Fakten für Gate G1 sammeln. Spike-Code ist Wegwerfcode und landet nicht in `src/`, sondern unter `research/spikes/`.

**Analyse**

- [ ] P0 Config aus den Paks lesen (FModel oder repak): `NetDriverDefinitions`, `DefaultPlatformService`, Iris-Einstellungen, Standard-Maps
- [x] P0 RPC-Liste aus dem neuen Dump gegen den Juli-Dump vergleichen: 3 RPCs entfernt, keiner hinzugekommen
- [ ] P0 Replizierte Properties und `bReplicates` der Kernklassen erfassen (Character, Enemy, Projektil, Loot, GameState)
- [ ] P1 Coop-Ablauf nachvollziehen: `LocalCoopSubsystem`, wie P2 entsteht, was `CoopAuthority` tatsächlich regelt
- [ ] P1 Karten-Erzeugung verstehen: Ist die Map aus einem Seed reproduzierbar (`Map_Generation`)?
- [ ] P1 Enemy-Spawning und `PoolManager` verstehen: Wie viele Actors, wie gepoolt?
- [ ] P1 Pausen-Logik klären: Was passiert bei Level-up und Inventar? (Hinweis: `bCoopBlocksGameplay`)
- [ ] P0 `bReplicates` der Blueprint-Actors prüfen (`BP_Enemy`, `BP_Projectile`, Loot), zum Beispiel an ihren Default-Objekten zur Laufzeit
- [ ] P1 Blueprint-Logik auslesen: Was läuft dort bei Spawns, Effekten und Events, und was davon muss der Client sehen?
- [ ] P1 Pixelsplit über das öffentliche Projekt informieren: Ist offizieller Online-Coop geplant, und wie stehen sie zu der Mod?

**Spikes**

- [ ] P0 **Spike A – Verbindung:** Host über `?listen`, zweite Instanz per IP (LAN, Tailscale oder ZeroTier) verbinden. Kommen beide in dieselbe Welt?
- [ ] P0 **Spike B – Spieler:** Sieht jeder den anderen Character, funktionieren Bewegung und Schießen?
- [ ] P1 **Spike C – Mission:** Lädt eine prozedurale Mission beim Client identisch?
- [ ] P1 **Spike D – Last:** Bandbreite und Framerate bei voller Gegnerzahl messen
- [ ] P1 **Spike E – Tunnel:** Spielverkehr über Steam P2P tunneln (siehe [Architektur](docs/ARCHITECTURE.md#transport))
- [ ] P1 Testaufbau klären: Laufen zwei Instanzen auf einem PC, oder braucht jeder Test zwei PCs?

### Gate G1 – Technikentscheidung

Entscheiden und in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#entscheidungen) festhalten:

- **Weg A (native Replikation)**, wenn Spike A und B funktionieren und Spike D tragbar ist.
- **Weg B (eigene Snapshot-Synchronisierung)**, wenn native Replikation unbrauchbar ist.
- **Projekt stoppen oder verkleinern**, wenn keiner der Wege tragbar ist, etwa bei Logik, die fast nur in Blueprints liegt und nicht netzwerkfähig ist.

## Phase 2 – Fundament

Ziel: Stabile Basis, auf der alle parallel arbeiten können. Ab hier sind die Arbeitsbereiche weitgehend unabhängig.

- [ ] P0 `Loader`: Proxy-DLL lädt die Mod beim Spielstart
- [ ] P0 `Core`: Logging in Datei, Config, Hook-Framework
- [ ] P0 `Core`: Dispatcher für Aufrufe auf dem Game Thread
- [ ] P0 `Core`: Versionsprüfung – bei unbekannter Build-ID Mod sauber deaktivieren statt abstürzen
- [ ] P0 `Game`: Adapter-Schicht für die ersten Klassen
- [ ] P1 `UI`: Einfaches Overlay mit Status, Lobby-Knöpfen und Debug-Anzeige
- [ ] P1 `tools`: Deploy-Skript, das die Mod ins Spiel kopiert
- [ ] P1 `tests`: Testprojekt und CI für Code ohne SDK-Abhängigkeit

## Phase 3 – Session und Verbindung

Ziel: Zwei Spieler finden sich über Steam und verbinden sich ohne Portfreigabe.

- [ ] P0 Steam-Lobby erstellen, Freunde einladen, beitreten (`OnlineSubsystemSteam` oder direkt Steamworks)
- [ ] P0 Transport-Tunnel aus Spike E als richtiges Modul umsetzen
- [ ] P0 Handshake: Spiel-Build und Mod-Version müssen bei beiden gleich sein
- [ ] P1 Saubere Fehlerbehandlung: Timeout, Abbruch, Host verlässt das Spiel
- [ ] P2 Wiederverbinden während eines Runs (Hinweis: `OnCoopReconnectRequiredChanged`)

## Phase 4 – Vertical Slice

Ziel: Ein spielbarer Ausschnitt von vorne bis hinten. Zwei Spieler starten eine Mission, kämpfen gegen einen Gegnertyp und sehen dasselbe.

- [ ] P0 Remote-Spieler als P2 in die Coop-Logik einbinden, obwohl er kein lokaler Spieler ist
- [ ] P0 Gemeinsamer Missionsstart, gleiche Karte bei beiden
- [ ] P0 Bewegung, Dash und Blickrichtung
- [ ] P0 Waffen, Projektile, Treffer, Schaden, Tod
- [ ] P0 Ein Gegnertyp inklusive Spawn und Tod
- [ ] P1 Eingaben aus dem lokalen `CoopInputRouter` sauber vom Remote-Spieler trennen

### Gate G2 – Spielbarkeit

Eine zehnminütige Testrunde über echtes Internet zwischen zwei Haushalten: stabil, ohne Desync und spielbar.
Falls nicht, Ursachen beheben, bevor weitere Spielinhalte synchronisiert werden.

## Phase 5 – Vollständiger Run

Ziel: Ein kompletter Run ist zu zweit online spielbar.

- [ ] P0 Alle Gegner, Bosse und Boss-Phasen
- [ ] P0 Loot, XP, Level-ups, Fähigkeiten und Status-Effekte (GAS)
- [ ] P0 Inventar: Rucksack, Stash, Werkbank, Reward-Queue (`CoopInventoryCoordinator`)
- [ ] P0 Inventar-UI beim Remote-Spieler
- [ ] P0 Tageszyklus, Treibstoff, Missionsziele, Fahrt zwischen den Etappen
- [ ] P1 Run-Ende und Rückkehr zur Meta-Basis
- [ ] P1 Vorgehen für Meta-Fortschritt: Wessen Stand zählt, was bekommt der Client?
- [ ] P2 VFX und Sound beim Client (FMOD)
- [ ] P2 Wöchentliche Herausforderung (WeeklyMode) entweder unterstützen oder online sperren

## Phase 6 – Release

- [ ] P0 **Bestenlisten und Erfolge online sperren**, damit Mod-Runs keine Ranglisten verfälschen
- [ ] P0 **Telemetrie, Bug-Reports und Crash-Reports im Mod-Betrieb abschalten**, damit Pixelsplit keine Mod-Fehler als Spielfehler bekommt
- [ ] P0 Mod-Spielstände vom normalen Spielstand trennen, damit kein Save beschädigt wird
- [ ] P1 Installer oder einfache Installationsanleitung
- [ ] P1 Vor dem ersten fertigen Release die Rückmeldung von Pixelsplit berücksichtigen
- [ ] P2 Mehr als 2 Spieler prüfen (Coop-Logik ist fest auf P1 und P2 ausgelegt, daher aufwendig)

## Risiken

| Risiko | Auswirkung | Gegenmaßnahme |
|---|---|---|
| RPCs sind nur Altlast aus dem Vor-Coop-Stand (Coop-Klassen haben keine RPCs) | Weg A fällt aus, deutlich mehr Eigenbau | Spike A und B zuerst; Weg B als Plan B |
| Logik liegt in Blueprints ohne Netzwerkcode | Viele Stellen brauchen eigene Hooks | Blueprint-Anteil früh abschätzen |
| Coop setzt zwei lokale Spieler voraus | Remote-P2 passt nicht in die Coop-Logik | Coop-Ablauf in Phase 1 analysieren |
| Viele Gegner überlasten das Netzwerk | Laggy oder unspielbar | Spike D; Gegner gebündelt synchronisieren |
| Spiel-Updates (seit 1.0 am 29.10.2026 aktiv gepatcht) | Hooks und SDK veralten | Versionsprüfung, Update-Regel, Adapter-Schicht |
| Offizieller Online-Coop erscheint | Mod wird überflüssig | Bei Pixelsplit nachfragen, bevor viel Aufwand fließt |
