# Projektweiter Audit: lokal zuerst, minimal synchronisieren

Nutzervorgabe und Quellcodeprüfung vom **05.11.2026**, Aufgabe `client-lokal-audit`,
Basiscommit `066fdba`, unterstützter Spiel-Build `26187885`.

## Auftrag und Grenze

Möglichst viel vorhandene Logik beim Client nutzen und nur das für Multiplayer nötige Minimum
teilen. Das soll Fehler und Entwicklungsaufwand reduzieren. Die verbindliche Regel steht in
[CLAUDE.md](../CLAUDE.md#verbindliches-architekturprinzip-lokal-zuerst-minimal-synchronisieren)
und [Architektur](ARCHITECTURE.md#projektweite-nutzervorgabe-vom-05112026).

**Dies ist ein Quellcode-/Dokumentationsaudit, kein Laufzeittest und keine Umsetzung der Empfehlungen.**
Geprüft wurden die vier produktiven Kontroll-Sync-Familien (Progress, Reward, Map, Desync),
ihre Adapter, die Client-/Kamera-/Bewegungs-/Projektil-/Inventar-/Widget-Patches,
Remote-P2-Anbindung sowie bestehende Anforderungen und Aufgaben für noch fehlende Systeme.
Netzwerk-Sendewege wurden zusätzlich über `SendControl` im gesamten `src/` gesucht.
Nicht geprüft: sämtliche nativen Spiel-/Blueprintpfade, alle Gegner/GAS-Effekte oder Bandbreite
im Internet. Native Aussagen unten stammen aus vorhandener Dokumentation, nicht neuer Decompilierung.

Uncommittete Arbeit in `task/airdrop-rucksack` und `task/xp-baelle` ist **nicht** Bestandteil
dieses Basiscommits. Der aktuelle Nutzertest wird als gesonderter Befund berücksichtigt:
Revolver sichtbar und im Kampf funktionierend; neue Equipmentfelder weiterhin fehlerhaft.
Im offenen Airdrop wurden 17 Host- gegen 14 Clientfelder desselben Inventars gemessen;
eine dauerhafte Differenz nach dem Bestätigen ist damit noch nicht bewiesen.

Die folgenden Punkte sind Arbeitsumfang/Abnahmen bestehender Kennungen, **keine zweite Aufgabenliste**.
Status und Abhängigkeiten stehen ausschließlich in [TODO.md](../TODO.md).
P0/P1/P2 bezeichnen hier den Änderungsbedarf; daraus werden weder blockierte Aufgaben freigegeben
noch optionale Schönheitskorrekturen vor den spielbaren Run gezogen.

## Wichtigste Umbaugrenzen

### 1. Inventar, Rucksackfelder und Reward-Auswahl — zuerst, P0

**Quellen:** [RewardPatches.cpp](../src/Game/RewardPatches.cpp), `ShowForClient`,
`FilterRewardEvent`, `ClientConfirm`, `DropLocal`, `RememberPlacement`, `ClaimForClient`, `PlaceForClient`;
[RewardSync.cpp](../src/Sync/Inventory/RewardSync.cpp), `TickClient`, `TickHost`;
[InventoryPatches.cpp](../src/Game/InventoryPatches.cpp), `InventoryReadyForReplicationHook`.

**Ist:** Lokale Choice-Instanzen werden erzeugt, deren native Server-RPCs herausgefiltert und
Wahl/Ablage separat über die Mod geschickt. Bestätigte Items kommen als andere native
Unterobjekte zurück. `RememberPlacement` erfasst normale Slot-Belegung, keinen vollständigen
Bag-Vorgang. Im untersuchten Code gibt es keinen gleichwertigen gemeinsamen Commit für
Rucksackverbrauch, Feldanlage und resultierenden Inventarzustand. Das ist eine konkrete
Verantwortungslücke, nicht bloß „zu viel Host“ oder zu wenig Bandbreite.

**Ziel:** Native Drag-/Rotations-/Platzierungslogik lokal behalten, aber auf einem klaren
Vorschauzustand mit stabiler Zuordnung zur bestätigten Item-ID. Eine Aktion bzw. abgeschlossene
Änderungsmenge mit erwarteter Revision senden; Host prüft und übernimmt zusammengehörige
Wirkungen genau einmal. Rucksackfelder müssen Bestandteil dieses Vorgangs sein.
Bestätigung/Ablehnung liefert den neuen verbindlichen Zustand; Client baut Layout und Caches lokal
auf. Native Replikation und Mod-Bestätigung brauchen eine definierte Übergabe, nicht zwei unabhängige
Schreiber auf `MyItems`/Slots. Bestehende Items verschieben ist ebenso abzudecken wie neue Choices.

**Abnahme:** Rucksack platzieren, drehen, abbrechen/umwählen; auf neue Felder sofort ein Item
legen; zweites Panel, Umbau bestehender Items, Reise; doppelte/verspätete Aktion und volle Sendewarteschlange.
Vergleichen: Besitz, Verbrauch, Feldkoordinaten, Belegung und Itemeffekte; keine lokalen Geisterobjekte.
**Zuständig:** `airdrop-rucksack` und `inventar-lokal` (erledigt), Werkbank in `werkbank-client`.
Kein neuer paralleler Inventar-Sync neben diesen Aufgaben.

**Umgesetzt in `inventar-lokal` (06.11.2026, noch ohne Spieltest):** Server-RPCs des Inventars laufen bei offenem Panel lokal,
beim Bestätigen geht ein Edit mit dem Endzustand der geänderten Items, der Wahl und den neuen Feldern an den Host, der ihn ganz
übernimmt oder ablehnt; der Client rollt bei Ablehnung zurück. `DropLocal`, `RememberPlacement` und `PlaceForClient` sind entfernt.
Ablauf: [Inventar lokal bearbeiten](../src/Game/README.md#inventar-lokal-bearbeiten).

### 2. Pause, Rundenphasen und blockierende UI — P0/P1, fehlende Integration

**Quellen:** [Game-README](../src/Game/README.md), Tabelle der Spike-Restpfade;
[RemoteP2Patches.cpp](../src/Game/RemoteP2Patches.cpp), `Bind`/`AllowRemoteCoop`;
[Architektur: Pausen](ARCHITECTURE.md#pausen-und-blockierende-ui-untersuchung-pausen).

**Ist:** Das dokumentierte Spike-Modell spiegelt Host-Panels und sperrt den Client, statt ihm
eigene Navigation zu geben. Produktiver Sync dispatcht Progress/Desync/Reward/Map, keinen
vollständigen Phasen-/Pausevertrag. Der native Local-Coop-Modus bleibt aus, weil er andernfalls
auf lokale P2-Bedienung wartet. Das ist nicht als implementierter neuer Netzwerkfehler auszugeben.

**Ziel:** Global nur Phase, Sitzung, erlaubte Aktionen und erforderliche Bereitschaft teilen.
Menüs, Fokus und Navigation lokal ausführen. Host entscheidet über globale Pause/Fortsetzen
anhand aller relevanten Gründe; Client schließt nicht das Host-Menü und hebt nicht dessen Pause auf.
Keine zweite Timescale-Replikation neben funktionierender nativer WorldSettings-Replikation.

**Abnahme:** Host zuerst fertig, Client zuerst fertig, verschachteltes Menü, Disconnect und
Travel; beide lokal bedienbar, globale Runde erst beim gültigen gemeinsamen Übergang frei.
**Zuständig:** `airdrop-warten`, `pausen-sync`, `tagesablauf`, `bestaetigen-rundenende`.

### 3. Kamera und XP-Sichtbereich — P1, vorhandene lokale Logik vervollständigen

**Quellen:** [CameraPatches.cpp](../src/Game/CameraPatches.cpp), `CameraPlayersHook`,
`ViewThroughSharedCamera`, `RepairRestrictor`; [ExpPatches.cpp](../src/Game/ExpPatches.cpp),
`ReplayExpSpawns`; ergänzende uncommittete Bounds-Arbeit im XP-Task.

**Ist:** Kamera liest beim Client bereits die replizierte PlayerArray anstelle des fehlenden
GameMode. Beim XP-Test fiel derselbe Host-only-Abhängigkeitsfehler im Sichtbereich auf.
Das lässt sich nicht sinnvoll mit fortlaufend gesendeten Host-Kameragrenzen heilen.

**Ziel:** Gemeinsame Kameraregel lokal aus bestätigten Spielern berechnen; Viewportprojektion,
Partikel-Culling und UI lokal aus der tatsächlichen Kamera ableiten. Host-Spawn-/Sammelregeln
bleiben gemeinsame Spielwirkung. Die beschlossene gemeinsame Kamera wird nicht durch getrennte ersetzt.

**Abnahme:** verschiedene Fenstergrößen/Seitenverhältnisse, Bildschirmrand, Pause/Zoom und
Kartenwechsel; keine Doppel-XP und keine hostabhängige lokale Sichtberechnung.
**Zuständig:** `kamera-code`, `xp-baelle`; getrennte Kameras bleiben `opt-kameras`.

## Weitere konkrete Vereinfachungskandidaten

### 4. Airdrop-Animation: Zustände statt Abspielkopf — P2, messen vor Umbau

**Quellen:** [AirdropAdapter.cpp](../src/Game/AirdropAdapter.cpp), `ReadAirdropViews` liest
Position/Playing, `ShowAirdropViews` ruft `SetPosition` auf;
[ProgressSync.cpp](../src/Sync/ProgressSync.cpp), `TickProgress`.

**Ist:** Native Actor-/Bewegungsreplikation plus Anzeigezustand im Progress-Vollbild;
Animationsposition wird im Kontrolltakt übertragen und auf dem Client gesetzt.
**Zielkandidat:** Start/Phase und geeignete gemeinsame Zeitbasis teilen, Animation lokal laufen
lassen; nur bei Join/Drift korrigieren. Native Actor-Lebensdauer weiterverwenden, keine zweite
Kiste/Belohnung spawnen. Erst klären, ob Animation steuernde Gameplay-Notifies hat.
**Abnahme:** Ankunft/Öffnung/Pause, spätes Eintreffen, Verlust und keine doppelten Notifies/Rewards.
**Zuständig:** `opt-client-visuell` / `vfx-sound`, kein vorgezogener Schönheitsumbau.

### 5. Progress/HUD: wiederholte Vollbilder und lokale Schreibarbeit — P2

**Quellen:** [Sync.cpp](../src/Sync/Sync.cpp), 250-ms-Takt;
[ProgressSync.cpp](../src/Sync/ProgressSync.cpp), `TickProgress` sendet Subscribe und Snapshot
auch unverändert; [ProgressAdapter.cpp](../src/Game/ProgressAdapter.cpp), `ShowSharedProgress`
setzt Progressfelder und `SetPercent` bei jedem Aufruf, auch ohne neues Bild.

**Ziel:** Subscription nach Bestätigung nicht jedes Mal erneuern; Host sendet Änderungen plus
begründetes Wiederherstellungs-/Lebenszeichenintervall. Client aktualisiert ein lokales Modell
bei neuer Revision, HUD glättet/zeichnet selbst. Ein ungeänderter Snapshot darf weiterhin als
gezielte Reparatur möglich sein. Keine lokale Fortschritts-/Reward-Gutschrift für eine Anzeige.
**Abnahme:** gleicher HUD-Stand bei weniger Sendungen/Setteraufrufen, neuer HUD nach Reise,
Sendefehler und verspäteter Snapshot. Bis zur Messung keine behauptete Bandbreitenersparnis in Zahlen.
**Zuständig:** `fortschritt`, bei reinem Feinschliff `opt-client-visuell`.

### 6. Projektile, Treffer-VFX und Sound — P2 / bestehender Kampf-Umfang

**Quelle:** [ProjectilePatches.cpp](../src/Game/ProjectilePatches.cpp), `TickProjectiles` schaltet
native Bewegungsreplikation für Pool-Actors ein; dokumentierte Lücken: `BulletSize`, Effekte, erstes Pool-Update.

**Ziel:** Fehlende Größe/visuelle Konfiguration bei Spawn/Pool-Neuverwendung übermitteln und lokal
anwenden. Animation, Partikel und Sound lokal aus bestätigtem Ereignis/Actorzustand abspielen.
Keine zusätzlichen Kosmetikdaten pro Frame, keine lokale zweite Schadenssimulation.
Native Projektilreplikation **nicht** allein zur Byte-Ersparnis durch eine eigene Simulation ersetzen:
Bewegung/Pool/Kollision sind dafür noch kein nachgewiesen deterministischer Vertrag.
**Abnahme:** Wiederverwendung, Größe, erste Bewegung, einmalige Treffer-/Soundeffekte bei Latenz,
unveränderte Treffer/HP. **Zuständig:** `kampf`, `vfx-sound`, `opt-client-visuell`, `opt-blut`.

**Ergänzung 05.11.2026 (`kampf`):** Drei weitere lokale Wege, alle ohne neue Spielwirkung beim Client:

- **Schadenszahlen und Treffereffekte aus `OnRep_Health`.** Der Client sieht jede HP-Änderung schon über
  `HealthBarPatches.cpp`. Aus der Differenz kann er Schadenszahl und Treffer-VFX aus seinem eigenen Pool zeigen,
  ohne eine Nachricht. Nie einen Damage-Actor nachspawnen. **Zuständig:** `opt-blut`, `vfx-sound`.
- **Tote Gegner sofort ausblenden.** Beim Client bleiben sie rund 15 s als Objekte stehen. Bei HP 0 lokal
  verstecken und Kollision lösen, das Zerstören bleibt Sache der Replikation. **Zuständig:** `gegnertyp`, `opt-gegnerzahl`.
  **Ergebnis `gegnertyp` (06.11.2026): nicht nötig.** Die stehenden Objekte haben schon `bActorIsBeingDestroyed`, sind aus
  `Level->Actors` entfernt und unsichtbar; der Host behält seine genauso bis zur Garbage Collection. Gegner-HP repliziert beim
  Client gar nicht (dort 0/0), ein HP-0-Auslöser wäre also falsch. [Befund](GAME_ANALYSIS.md#ein-gegnertyp-synchron-gegnertyp)
- **Eigene Schüsse sofort anzeigen.** Eine rein kosmetische Vorhersage des eigenen Projektils beim Client, bis das
  replizierte eintrifft. Nur, wenn der Latenztest (`-NetProfile Internet`) die Verzögerung als störend zeigt;
  Treffer bleiben beim Host. **Zuständig:** `opt-client-visuell`.

### 7. Bewegung und Upgrade-Parameter — prüfen, nicht pauschal auslagern

**Quellen:** [MovementPatches.cpp](../src/Game/MovementPatches.cpp), `RestoreDashSettings`
liest Klassen-Defaults, `FollowReplicatedLocation` extrapoliert bereits lokal;
[TODO](../TODO.md#optionale-optimierungen), gemeldete fehlende Tempo-Upgrades.

**Ziel:** Spielwirksame bestätigte Bewegungsparameter müssen den lokalen nativen Bewegungspfad
erreichen. Defaults dürfen nicht unbemerkt bestätigte Modifikatoren ersetzen; ob der aktuelle
Reibungs-/Gravity-Fallback wirklich Upgrades überschreibt, ist **nicht belegt**.
Vorhersage und Interpolation lokal behalten; keine zusätzliche Eingabe- oder Positionsfamilie
neben der Engine einführen, ohne deren konkrete Lücke nachzuweisen.
**Abnahme:** Upgrade plus Dodge, Pause und verschiedene Latenzen, keine dauerhafte Drift oder
verlorenen Parameter. **Zuständig:** `eingaben`, `opt-tempo-upgrades`, `opt-host-bewegung`.

### 8. XP-Orbs: lokale Kosmetik ist richtig, Aufnahmevertrag bleibt wichtig

**Quellen:** [ExpPatches.cpp](../src/Game/ExpPatches.cpp), `SpawnHook`, `ReceiveHook`,
`ReplayExpSpawns`; [ProgressSync.cpp](../src/Sync/ProgressSync.cpp), Orb-Batches.

**Ist/Ziel:** Native Partikelsimulation lokal, nur Ausstoßereignisse übertragen; Vergabe einmal
beim Host. Bereits passend. Nicht jeden Ball pro Frame übertragen und nicht beide Niagara-
Simulationen zu unabhängigen XP-Kassen machen. Erst den nachgewiesenen Sichtbereichsfehler lösen;
komplette Orb-IDs-/Aufnahmeprotokolle nur bei belegtem Bedarf für die Spielregel entwickeln.
**Abnahme:** beide/einzelner Spieler sammelt ein, Verlust/Travel, gleicher Gewinn genau einmal.
**Zuständig:** `xp-baelle`, `fortschritt`; visuelle Randfälle `opt-client-visuell`.

### 9. Karte/PCG: Vorbild beibehalten, Wiederholung gezielt begrenzen — P2

**Quellen:** [MapPatches.cpp](../src/Game/MapPatches.cpp), `PublishHostMap` und Client-Erzeugung;
[MapSync.cpp](../src/Sync/World/MapSync.cpp), `TickHost` wiederholt das Bild spätestens jede Sekunde.

**Ist:** Seed/Profil/Tags/Graph statt aller Kacheln übermitteln, lokal erzeugen; replizierte
Spielobjekte nicht lokal duplizieren. Das erfüllt die Vorgabe bereits.
**Kandidat:** Nach belastbarer Bestätigung gleicher Erzeugungs-ID auf Änderungen/Neuabonnement
umschalten statt unverändertem Sekundenbild. Kein isoliertes Entfernen des Resends: Er deckt
Warten auf lokale Assets/Generatoren und Wiederanlauf ab.
**Abnahme:** Identität von Karte und Kollision, genau einmal erzeugen, Reise/später Generator,
abweichende lokale Inhalte ablehnen. **Zuständig:** `missionsstart` bleibt abgeschlossen;
nur neue konkrete Abweichungen in `opt-karte-details`/`reconnect` verfolgen.

### 10. Diagnose und bestätigter Zustand — P1 bei Inventarintegration

**Quellen:** [DesyncSync.cpp](../src/Sync/DesyncSync.cpp), `TickDesync`;
[DesyncAdapter.cpp](../src/Game/DesyncAdapter.cpp), Inventaraufnahme.

**Ist:** Lokale Berechnung kompakter Prüfsummen, Probe/Antwort etwa alle zwei Sekunden,
keine automatische Zustandskorrektur. Das ist sinnvoller Minimal-Sync, keine neue Autorität.
**Ziel:** Bei Einführung einer lokalen Inventarvorschau den bestätigten Stand/relevante Revision
vergleichen und ausstehende Vorschau getrennt kennzeichnen. Kein legitimes Preview als dauerhaft
falsches Inventar werten; echte Feldabweichungen nicht durch pauschales Ausschließen verdecken.
**Abnahme:** offene Vorschau ohne Fehlalarm, absichtlich falscher bestätigter Slot mit Alarm,
Ablehnung/Travel/Disconnect. **Zuständig:** `opt-desync-pivot` und `werkbank-client`, mit vorhandener Diagnose.

## Keine unnötigen Umbauten

- **Itemsack-/Loot-Vergabe:** `InventoryPatches.cpp::ItemLootHook` verhindert doppelte native
  Client-Vergabe; nicht zum bloßen Wiederverwenden lokaler Logik wieder freigeben.
- **Gegner, Treffer, Wirtschaft und Saves:** keine unabhängigen Client-Ergebnisse für gemeinsamen
  Zustand. Lokale Darstellung/Vorprüfung ja; Schaden, RNG-Vergabe und Kosten genau einmal bestätigen.
  `fortschritt`, `inventar-sync`, `mod-saves` und die Teamentscheidung `meta-fortschritt` behalten ihre Grenzen.
- **Native UE-Replikation/Steam-Tunnel:** Grundlage beibehalten; keine eigene Gesamtsimulation,
  kein neuer Transport. `Net` transportiert, besitzt keine Inventar- oder UI-Regeln.
- **Null-/Travel-/Lebensdauerwächter:** `ClientPatches.cpp`, `WidgetPatches.cpp` und NetGUID-Fixes
  nicht entfernen, nur weil lokale Logik bevorzugt wird. Sie sichern verspätete Objektverfügbarkeit ab.

## Empfohlene Reihenfolge und Nachweis

1. Aktuellen Inventar-/Bag-Vorgang einschließlich lokalem Neuaufbau kohärent machen.
2. Eigene Client-UI und globale Pause/Phase sauber trennen; fehlende Übergänge anbinden.
3. Kamera-/XP-Projektion aus lokalen Daten und bestätigte Bewegungsparameter absichern.
4. Danach nur gemessene unnötige Vollbilder/Animationskorrekturen reduzieren und Kosmetik ergänzen.

Jeder Umsetzungsschritt benennt den ersetzten bisherigen Weg; keine zusätzliche Schicht dauerhaft
neben der alten laufen lassen. Vergleichen: gleiche bestätigte Wirkung, einfache lokale Bedienung,
Anzahl Nachrichten/Bytes und Wartungsaufwand. Negative Tests: doppelte Nachricht, Ablehnung,
volle Queue, verspätetes UObject, Join, Travel, Disconnect/Reconnect. Weniger Code/Nachrichten
allein beweisen keine Fehlerfreiheit. Dieser Audit erzeugt keine neue Spielabnahme.
