# Aufgaben-Details

Langtexte zu offenen Aufgaben in [TODO.md](../TODO.md), deren Zeile nicht alles fasst. Aufgaben ohne Abschnitt sind in ihrer Zeile vollständig beschrieben.
Die Zeile ist die Kurzfassung: Nennt der Abschnitt mehr Bedingungen unter „Fertig, wenn“, gilt der Abschnitt.
Wer eine Aufgabe nimmt, liest hier nur ihren Abschnitt; mit dem Abschluss der Aufgabe wird er gelöscht.

## `opt-schuss-animation`

Gemeldet im Live-Test am 13.11.2026: Bei der Client-Figur liegen mehrere Waffen übereinander, und die Figur gleitet über den Boden.

- [Belegt, ein PC] Die übereinander liegenden Waffen waren Geisterwaffen aus dem Host-Zustand nach einer Aufwertung im Client-Edit; dagegen ist #224 gemergt, live nicht bestätigt (`tools/spike/hands.py` lief im Live-Test nicht), siehe [Schuss-Animation und Geisterwaffen](archive/GAME_ANALYSIS.md#schuss-animation-und-geisterwaffen-opt-schuss-animation).
- [Gemeldet, Tester, Live-Test 26.11.2026, zwei PCs] Die Client-Figur gleitet beim Host starr, die Laufanimation blitzt nur einzelne Frames auf.
- [Belegt, Host-Log] Je Welt steht dort „Tempo der Client-Figur misst der Host aus ihrem Ort“; die Messung aus `opt-laufanimation-client` greift also an.
- [Gemeldet, Live-Test 13.11.2026 abends] Die Client-Figur glitt auch beim Client ohne richtige Laufanimation.
- [Belegt] Auf einem PC lief das Wackeln (`WobbleAnimation.Progress`) auf beiden Seiten, auch mit `-NetProfile Internet`; `tools/spike/walk_animation.py` stellt den Fehler also nicht nach, siehe [Laufanimation beim Client](archive/GAME_ANALYSIS.md#laufanimation-beim-client-opt-laufanimation-client).
- [Vermutung] Der Host übernimmt den Ort der Client-Figur paketweise (`AcceptClientPositions` in `src/Game/PlayerPatches.cpp`), die Figur springt in Stufen und das gemessene Tempo ist zwischen zwei Paketen 0.
- [Vermutung] Dieselbe Ursache hat das zeitweise Rucken der gemeinsamen Kamera des Hosts, das nur auftritt, wenn sich Figuren bewegen.
- Prüfidee: Positionssprünge der Client-Figur im Host-Log protokollieren und mit dem gemessenen Tempo vergleichen.

**Fertig, wenn** die Ursache des Gleitens belegt ist (auf einem PC nachgestellt oder im Log eines Live-Tests gemessen) und die Client-Figur danach bei Host und Client durchgehend mit Laufanimation läuft und keine Waffen übereinander trägt.
Die Messung mit `walk_animation.py` auf einem PC allein genügt nicht, sie bestand schon vor dem Live-Test.
Ordner `src/Game`.

## `coop-inventar-konzept`

Enthält die frühere Aufgabe `coop-inventar-analyse`: untersuchen und das Konzept schreiben.
Keine andere Aufgabe wartet auf das Konzept. Was `coop-inventar-zwei`, `coop-modus-wahl` oder `coop-inventar-geteilt` schon gebaut oder festgelegt haben, übernimmt und prüft es.

Untersuchen, ohne Mod, Referenz ist lokaler Koop mit zwei Controllern auf einem PC:

- Aufbau des gemeinsamen Rucksacks mit Bereichen je Spieler (`ECoopInventoryLocationType::P1Backpack`/`P2Backpack`, `CoopP1Field`/`CoopP2Field`, `dd.Coop.SplitBackpackView`).
- Regeln, nach denen Unterstützungs-Items über die Bereichsgrenze auf Waffen wirken, und die Zuordnung von Waffe zu Figur.
- Ablauf von Lager, Airdrop-Wahl (`ClaimReward`, `AwaitingClaims`) und Rundenende mit `bCoopEnabled`.
- Neu bewerten, warum `inventar-sync` den Weg über `CoopInventoryCoordinatorComponent` verworfen hat und warum das Lager mit Koop-Modus hing.

Im Konzept klären, ausgelegt auf zwei bis vier Spieler:

- Was bleibt lokal (Anzeige, Drag, Hervorhebung, Wirkungsberechnung), was geht über die Leitung, wer bucht.
- Stabile Spieler-Slots statt fester P1/P2, Anordnung und Größe der Bereiche bei drei und vier Spielern, und ob ein Spieler nur im eigenen Bereich ablegt.
- Wie beide Modi nebeneinander bestehen (gemeinsame Teile, modusabhängige Pfade, keine doppelte Buchung) und wo der Modus gewählt und festgehalten wird.
- Beitritt, Ausstieg eines Spielers mitten im Run, Spielstand.
- Native Wege nur nutzen, wo die Untersuchung sie trägt.

**Fertig, wenn** ein Abschnitt in `docs/themen/INVENTAR_UND_HANDEL.md` Datenmodell, Wirkregeln über Bereichsgrenzen, native Andockpunkte und die Grenzen der fest auf zwei Spieler ausgelegten Typen mit Belegt/Vermutung und Bildvergleich nennt, das Konzept in `docs/` steht, die Fragen aus dem Architekturprinzip beantwortet und das Team es freigegeben hat.
**Einstieg:** [Lokale Coop-Schnittstellen](archive/GAME_ANALYSIS.md#lokale-coop-schnittstellen), [Bis zu vier Spieler](MEHR_SPIELER.md).

## `coop-inventar-zwei`

Enthält die frühere Aufgabe `coop-inventar-ablauf`.
Fehlende Festlegungen (Datenmodell, wer bucht, Umschalter zwischen den Modi) trifft die Aufgabe selbst und hält sie in `docs/themen/INVENTAR_UND_HANDEL.md` fest.
Vorhandenes, etwa ein Konzept oder eine Moduswahl, übernimmt und prüft sie.

- Ein gemeinsamer Rucksack mit einem Bereich je Spieler, beide sehen dasselbe Raster.
- Unterstützungs-Items wirken über die Bereichsgrenze auf Waffen des Mitspielers; die Hervorhebung beim Überfahren zeigt auch diese Wirkungen.
- Der Host bucht, der Client bearbeitet lokal und bekommt Bestätigung oder Rücknahme.
- Alle Inventarabläufe laufen im geteilten Inventar: Airdrop-Wahl, Itemsäcke, Werkbank, Lager mit Verkauf und Reparatur, Rundenende mit Bestätigen.
- Der Modus Eigene Inventare bleibt unverändert nutzbar; beide Modi teilen sich vorhandene Pfade, wo das Konzept es vorsieht.

**Fertig, wenn** auf zwei PCs ein Run von Tag 1 bis Tag 4 im Modus Geteiltes Inventar ohne Absturz und ohne Desync-Befund läuft, ein Unterstützungs-Item im Bereich von Spieler A die Waffe von Spieler B nachweislich verstärkt, die Waffenwerte bei beiden gleich sind und ein Run bis Tag 2 im Modus Eigene Inventare weiter wie in Release 1.0 läuft.
Ordner `src/Game`, `src/Sync`.

## `coop-modus-wahl`

Wo der Modus gewählt und festgehalten wird, legt die Aufgabe selbst fest, solange es dazu nichts gibt.
Gibt es das geteilte Inventar noch nicht, überträgt und zeigt sie nur den Modus; was er im Inventar bewirkt, gehört nicht zu dieser Aufgabe.
Vorhandenes, etwa einen Umschalter aus `coop-inventar-zwei`, übernimmt und prüft sie.

- Der Host wählt vor dem Hosten der Lobby zwischen den Modi Eigene Inventare und Geteiltes Inventar.
- Der Modus geht beim Beitritt an alle Clients, gilt für den ganzen Run und lässt sich währenddessen nicht wechseln; Standard bleibt Eigene Inventare.
- Clients ohne passende Mod-Version lehnen ab, statt im falschen Modus zu spielen.
- Der Modus wird so festgehalten, dass ein gespeicherter Run ihn behalten kann.

**Fertig, wenn** auf zwei PCs beide Modi nacheinander wählbar sind, Host und Client denselben Modus anzeigen und im Log führen, er sich im Run nicht wechseln lässt und ein Client mit unpassender Mod-Version ablehnt.

## `coop-inventar-geteilt`

Wunsch des Nutzers im Wortlaut: „Seperate new shared Inventory Logic like real coop“.
Gemeint ist eine neue Inventarlogik, getrennt von der bisherigen, die wie der lokale Koop des Spiels einen gemeinsamen Rucksack mit einem Bereich je Spieler zeigt, für zwei bis vier Spieler.
Die bisherige Logik (jeder Spieler hat sein eigenes Inventar, Release 1.0 und `vier-spieler`) bleibt unverändert und Standard; die neue bekommt eigene Pfade statt Sonderfällen in der alten.

Diese Zeile deckt drei und vier Spieler ab; zwei Spieler samt allen Abläufen beschreibt `coop-inventar-zwei`, die Wahl vor dem Hosten `coop-modus-wahl`.
Sie wartet auf keine davon: Gibt es das geteilte Inventar für zwei Spieler noch nicht, baut sie die nötige Grundlage selbst, Vorhandenes übernimmt und prüft sie.

- Die nativen Typen kennen nur P1 und P2 (`ECoopInventoryLocationType`), darum eine eigene Bereichszuordnung je Spieler-Slot.
- Unterstützungs-Items wirken über alle Bereichsgrenzen, jeder Spieler sieht alle Bereiche.
- Der Host bucht, Clients bearbeiten lokal mit Bestätigung oder Rücknahme.

**Fertig, wenn** mit drei und mit vier Spielern ein Run bis Tag 2 im geteilten Inventar ohne Absturz und ohne Desync-Befund läuft, Unterstützungs-Items über jede Bereichsgrenze wirken und ein Run mit der bisherigen Logik weiter wie vorher läuft.
**Einstieg:** [Bis zu vier Spieler](MEHR_SPIELER.md). Ordner `src/Game`, `src/Sync`.

## `update-startpruefung`

Mod-/Spiel-Kompatibilität vor dem Laden prüfen: gemeinsame Quelle für Mod-Version, Protokollversion, unterstützte Game-Builds, Kompatibilitätsprofil und Datei-Hashes; Starter prüft die tatsächlich gestartete Installation (auch eingefrorene Kopien, Exe und relevante Inhaltscontainer), nicht nur das Steam-Manifest. Bei direktem Steam-Start prüft der `version.dll`-Loader erneut vor dem Laden von `DDCoop.dll`; die interne Prüfung bleibt bestehen. **Fertig, wenn** unbekannte/veränderte Builds und widersprüchliche Paketangaben die Mod nicht laden, ein sichtbarer Hinweis Mod-Version, gefundenen/erwarteten Build und Grund nennt und Abbrechen oder ein nachweislich modfreier Start möglich sind; keine Meldung unter dem Loader-Lock. Eine ältere, weiterhin kompatible Mod bleibt erlaubt; Internet ist für die lokale Freigabe nicht nötig. Pflicht-Patch-/Adapterfehler verhindern die Coop-Freigabe, Teilinstallation darf nicht als erfolgreicher Start gelten. Positiv-, Negativ- und Direktstarttests dokumentieren. **Einstieg:** `src/Loader/Loader.cpp`, `src/Core/Version.cpp`, `src/Mod/Mod.cpp`, `src/Game/Patches.cpp`, `tools/game_build.py`

## `update-releasefeed`

Vertrauenswürdig prüfbare Release-/Kompatibilitätsinformationen anbieten: signierte Metadaten mit Mod-Version, unterstützten Build-/Profilidentitäten, Protokollversion, Paket-Hashes und ausdrücklichen Rücknahmen kritischer Releases. Zunächst nur nach einem passenden Update suchen und es anbieten, nichts automatisch installieren. **Fertig, wenn** nur ausdrücklich freigegebene Pakete für den tatsächlich geprüften Game-Build angeboten werden, Signatur/Schema sowie veraltete oder zurückgespielte Metadaten geprüft werden und ungültige Antworten keine Freigabe bewirken; Offlinebetrieb nutzt die lokale Kompatibilitätsprüfung und kennzeichnet den Update-Status als unbekannt. Bekannte gültige Rücknahmen werden nicht durch Offlinebetrieb vergessen; eine bloß ältere kompatible Mod wird nicht gesperrt. Kein automatischer Support durch gefundene Signaturen. Tests für passende/unpassende Releases, Rücknahmen, ungültige Signaturen, veraltete Antworten und Netzausfall bestehen

## `update-autoupdater`

Optionales automatisches Mod-Update, nur nach ausdrücklicher Aktivierung.
Fehlen die signierten Release-Metadaten noch, baut die Aufgabe den benötigten Teil selbst; Vorhandenes aus `update-releasefeed` übernimmt und prüft sie.
**Fertig, wenn** ein für den vorhandenen Game-Build freigegebenes Paket zunächst separat heruntergeladen, anhand signierter Metadaten und Hashes geprüft und ausschließlich bei beendetem Spiel transaktional installiert wird; Archive dürfen keine fremden Pfade überschreiben. Abbruch, beschädigter Download, Dateisperren und Installationsfehler lassen das alte vollständige Paket erhalten oder stellen es wieder her. Vorversion und Rückkehrmöglichkeit bleiben verfügbar, Konfiguration und Saves bleiben erhalten; Save-Kompatibilität vor Rückkehr prüfen. Keine Game-Updates, keine Änderung eingefrorener Spielkopien und kein Austausch geladener DLLs. Tests für Fehlerfälle, Wiederanlauf und Rückkehr dokumentieren
