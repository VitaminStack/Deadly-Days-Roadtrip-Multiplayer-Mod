# Zusammenarbeit

Beiträge sind willkommen, auch ohne Einladung ins Team. Wer keinen Schreibzugriff hat, erstellt einen Fork und öffnet von dort einen Pull
Request. Größere Vorhaben bitte vorher in einem Issue abstimmen, damit keine Arbeit doppelt entsteht. Mit einem Beitrag stimmst du zu,
dass er unter der [MIT-Lizenz](LICENSE) des Projekts steht.

## Arbeitsbereiche

Die Bereiche zeigen, welche Ordner zusammengehören. Feste Personen je Bereich und `CODEOWNERS` gibt es nicht. Die Arbeit teilt sich
über die [Reservierung](#aufgaben-reservieren): Wer eine Aufgabe nimmt, reserviert sie und arbeitet dann in den Ordnern, die sie braucht.

| Bereich | Ordner |
|---|---|
| Fundament | `src/Loader`, `src/Mod`, `src/Core`, `tools` |
| Netzwerk | `src/Net`, `tests` |
| Spieler und Kampf | `src/Sync/Players`, `src/Sync/Combat`, `src/Game` |
| Gegner und Welt | `src/Sync/Enemies`, `src/Sync/World`, `src/Game` |
| Inventar und UI | `src/Sync/Inventory`, `src/UI`, `src/Game` |
| Reverse Engineering | `research`, `docs/GAME_ANALYSIS.md` |

`src/Game` nutzen mehrere Bereiche gemeinsam. Deshalb liegt dort pro Spielsystem eine eigene Datei, etwa `PlayerAdapter`, `EnemyAdapter` und `InventoryAdapter`.

## Ablauf

1. [Aufgabe reservieren](#aufgaben-reservieren), damit niemand doppelt arbeitet.
2. Branch von aktuellem `main` erstellen. Bei reservierten Aufgaben ist das `task/<kennung>`.
3. Klein arbeiten und oft pushen.
4. Pull Request öffnen und ein Review einholen.
5. Per Squash mergen und den Branch löschen.

`main` muss immer bauen. Direkte Pushes auf `main` sind nicht vorgesehen.

### Aufgaben reservieren

Jede Aufgabe hat in [TODO.md](TODO.md) eine Kennung, eine Zeile in der Tabelle ihrer Phase und unter „Braucht“ ihre Voraussetzungen. Nehmen darf man nur, was dort `frei` ist, siehe [Wo kann ich anfangen?](TODO.md#wo-kann-ich-anfangen). Reserviert ist, was einen Branch `task/<kennung>` auf GitHub hat.
Der erste Push gewinnt, ein zweiter wird abgelehnt. Das gilt für Menschen und KI-Agenten, die genauen Befehle stehen in [CLAUDE.md](CLAUDE.md#vor-dem-start).

`py tools/tasks.py list` zeigt jede offene Kennung mit Status: frei, blockiert, Team, reserviert, in Arbeit oder PR wartet auf Review. Blockierte und Team-Aufgaben lehnt `claim` ab.
`py tools/tasks.py claim <kennung> <name>` reserviert und öffnet gleich einen Draft-PR `[WIP] <kennung>`. Wer gerade woran arbeitet,
steht deshalb auf GitHub unter „Pull requests“. **Erledigt** ist, was in `TODO.md` auf `origin/main` unter [Erledigt](TODO.md#erledigt) steht, auch ohne GitHub CLI und nach dem Löschen eines Branches. Für ergänzende Review-/PR-Informationen braucht das Skript die angemeldete [GitHub CLI](https://cli.github.com/) (`gh auth login`); ohne sie bleiben diese Informationen unbekannt, der TODO-Abschluss bleibt erhalten. Ein gemergter PR bei noch offener Aufgabe verlangt Klärung statt automatischer Freigabe.
Wer über einen Fork arbeitet, kann so nicht reservieren und legt stattdessen ein Issue an.

Aufgaben mit der Bearbeitung **zu zweit** sind Tests auf zwei PCs. Dort dürfen zwei Personen je eine Rolle nehmen:
`py tools/tasks.py claim <kennung> <name> host` und `... client` legen `task/<kennung>/host` und `task/<kennung>/client` an.
Wer beide PCs selbst bedient, reserviert ohne Rolle. Ablauf und Abschluss stehen in [Test auf zwei PCs](docs/TESTING.md#test-auf-zwei-pcs).

### Aufgabe abschließen und Fortschritt pflegen

Die Schritte stehen in [Aufgabe abschließen](TODO.md#aufgabe-abschließen). Kurz:

1. Ergebnis und Testgrenzen in der Doku festhalten. Erledigte Analyse ist nicht automatisch fertige Umsetzung.
2. Nur bei erfüllter Abnahme die Zeile aus der Phasentabelle nach [Erledigt](TODO.md#erledigt) verschieben, alphabetisch, mit einem Satz Ergebnis. Neue Aufgaben bekommen eine Zeile in ihrer Phase mit „Braucht“.
3. `py tools/task_status.py update` erneuert „Wo kann ich anfangen?“ und die Fortschrittstabelle im README. Zahlen nicht von Hand schätzen.
4. `py tools/task_status.py check` und `py -B -m unittest discover -s tests` ausführen. Die CI erkennt doppelte oder unbekannte Kennungen, Kreise in „Braucht“, falsche Werte und veraltete erzeugte Abschnitte. Erst nach dem Merge zeigt `tasks.py list` den neuen Stand von `origin/main`.

### Branches

```text
task/<kennung>                          task/inventar
task/<kennung>/<rolle>                  task/tunnel-p2p/host (nur Aufgaben „zu zweit“)
feature/<bereich>-<kurzbeschreibung>    feature/net-steam-lobby
fix/<bereich>-<kurzbeschreibung>        fix/core-hook-crash
research/<thema>                        research/spike-listen-server
docs/<thema>                            docs/setup-steamworks
```

### Commits

Kurz, im Imperativ, mit Bereich vorne. Sprache im Repo: Deutsch für Dokumentation, Englisch für Code und Bezeichner.

```text
net: add lobby invite handling
core: fix dispatcher shutdown race
docs: update RPC list for build 26187885
```

### Pull Requests

- Eine Sache pro PR. Lieber drei kleine als einen großen.
- Sagen, wie getestet wurde: Spiel-Build, ein oder zwei PCs, was funktioniert hat.
- Änderungen am Netzwerkprotokoll erhöhen die Protokollversion.
- Neue Erkenntnisse über das Spiel gehören in `docs/GAME_ANALYSIS.md`, auch gescheiterte Versuche. Wohin was gehört, steht in [docs/TESTING.md](docs/TESTING.md#ergebnisse-festhalten).

## Regeln im Code

Die Modulgrenzen stehen in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#module). Am wichtigsten:

- SDK-Includes nur in `src/Game`.
- Unreal-Objekte nur auf dem Game Thread anfassen.
- C++20, Formatierung laut `.editorconfig`.
- Abhängigkeiten nur nach Absprache hinzufügen und in `third_party/README.md` eintragen.

## Was nie ins Repo darf

- Spieldateien: `.exe`, `.pak`, `.ucas`, `.utoc`, `.uasset`, `.uexp`
- Generierte SDKs und Dumps von Dumper-7
- Decompilierter Spielcode und entpackte Assets
- Steamworks-SDK-Dateien (Lizenz erlaubt keine freie Weitergabe)
- Persönliche Pfade, Spielstände, Tokens, Steam-IDs

Die `.gitignore` fängt das meiste ab, ersetzt aber nicht das Hinschauen vor dem Commit. Eigene Notizen wie Klassennamen, Hook-Listen oder
Beschreibungen des Verhaltens sind in Ordnung. Das Repo ist öffentlich: Alles, was hier landet, ist für alle sichtbar und bleibt in der
Git-Historie, auch wenn es später gelöscht wird.

## Spiel-Updates

Das Spiel wird noch aktiv gepatcht, und jedes Update kann Offsets und Klassen ändern. Deshalb gilt seit dem 03.11.2026:
**einfrieren und nur bei Bedarf nachziehen.** Alle arbeiten auf dem [unterstützten Build](docs/GAME_ANALYSIS.md#unterstützte-spielversion),
auch wenn Steam schon einen neueren anbietet.

### Auf dem unterstützten Build bleiben

1. **Entwicklung von Steam trennen:** Solange der unterstützte Build installiert ist, einmal
   `powershell -ExecutionPolicy Bypass -File tools/Freeze-GameBuild.ps1 -Activate` ausführen.
   Das erstellt eine echte Kopie unter `%LOCALAPPDATA%/DDCoop/builds/26187885`, prüft sie und setzt `PinnedGameDir` in `config/Local.props`.
   Das Ziel liegt außerhalb jeder `steamapps`-Struktur; Junctions, Symlinks und Hardlinks werden abgewiesen.
2. Steam bleibt für die Anmeldung online und darf **seine Installation** aktualisieren. Die getrennte Entwicklungskopie wird dadurch nicht ersetzt.
   Keine ACF-Schreibschutztricks, manipulierten Build-IDs, globalen Firewallregeln oder Steam-Offlinemodus nötig.
3. Nur über `tools/spike/Start-Session.ps1` testen. Bei gesetztem `PinnedGameDir` prüfen Start und Deploy alle eingefrorenen Dateien gegen den
   SHA-256-Beleg, unabhängig vom aktuellen Steam-Manifest. Fehlende/geänderte Dateien führen zum Abbruch, nie zum Rückfall auf die Steam-Kopie.
   `GameDir` bleibt die Steam-Quelle für eine spätere, bewusst vorbereitete Update-Unterstützung. Ohne `PinnedGameDir` bleibt der alte
   manifestbasierte Startweg aus Kompatibilitätsgründen erhalten; dieser ist **nicht eingefroren**.
4. Die Entwicklungskopie privat sichern, nicht nur die Exe. Wer den Stand noch nicht besitzt, muss zuerst eine eigene passende Spielkopie beschaffen.
   Ein Download über die Steam-Konsole kommt infrage (`steam://open/console`), mit den Manifest-IDs aus der
   [Spielversion](docs/GAME_ANALYSIS.md#unterstützte-spielversion):

   ```text
   download_depot 3026450 3026451 1832737755528485607
   download_depot 3026450 3026452 7724756528743620812
   ```

   Erwartetes Ziel ist `steamapps/content/app_3026450/depot_<Depot-ID>/`. Dieser Weg ist noch nicht ausprobiert; bekannte Manifest-IDs allein
   belegen weder Downloadberechtigung noch spätere Verfügbarkeit. Eine Wiederherstellung noch nicht durch blindes Zusammenkopieren versuchen:
   alte und neue Dateien sowie das Steam-Manifest können danach auseinanderliegen. Die vorhandene Installation und Spielstände bleiben beim
   ersten Test unangetastet, verglichen wird zunächst im separaten Downloadordner.

**Stand und Abnahme:** Die Trennung ist mit angemeldetem Steam und Host/Client samt Mod aus der eingefrorenen Kopie getestet.
Siehe [Einrichten und Grenzen](docs/SETUP.md#gleiche-build-id-im-team). Jedes Teammitglied erstellt seine eigene Kopie aus seinem lizenzierten Spiel;
keine Weitergabe von Spieldateien. Steam-Einstellungen und Downloads werden nicht automatisch geändert.

### Wenn ein Update erscheint

1. Wer es bemerkt, meldet es im Team mit der neuen Build-ID. Die Steam-Installation darf aktuell werden, die Entwicklungskopie bleibt unverändert.
   Spieltests laufen weiter auf dem eingefrorenen Build; kein automatisches Neu-Erstellen oder Umschalten auf den neuen Stand.
2. Jeder neue Build bekommt bei Bedarf eigene Update-Unterstützung, zum Beispiel wenn er einen blockierenden Absturz behebt.
   Das Team priorisiert die zentrale Aufgabe `spiel-update`; ein Steam-Update allein erzwingt keinen Entwicklungswechsel.
   Vor `integration-run` und `release` legt das Team die konkret unterstützten und getesteten Builds fest.
3. Das Nachziehen ist aufgeteilt: `update-basis` (zentral) unterstützt beide Builds und liefert Patch-Tabelle und Adresswerkzeug, danach portieren `update-client`, `update-inventar` und `update-welt` parallel, `spiel-update` schließt ab. Wer `update-basis` reserviert, aktualisiert sein Spiel, erzeugt ein neues SDK und passt Offsets,
   Signaturen und Patches auf ihrem Branch an.
4. Im selben PR Build-ID, Exe-Version und Manifest-IDs in [docs/GAME_ANALYSIS.md](docs/GAME_ANALYSIS.md#unterstützte-spielversion), die Werte in
   [docs/OFFSETS.md](docs/OFFSETS.md) und `src/Game/Offsets.h` (mit neuem SDK bauen, dann prüft `SdkChecks.cpp` jeden Wert), `$SupportedBuild` in `tools/spike/Common.ps1`, die Build-/Hash-Referenz in `tools/game_build.py` sowie
   `SupportedBuilds` in `src/Core/Version.cpp` aktualisieren. Der neue Build bekommt einen eigenen Kopieordner, der alte wird nicht überschrieben.
5. Erst nach dem Merge und einer ausdrücklichen Teamentscheidung wechseln alle ihre Entwicklungskopie, weil alle denselben Build brauchen.
   Die alte Kopie und der dazu passende Mod-Branch bleiben verfügbar, bis der neue Stand läuft.

## Fehler melden

Das Issue-Template „Bug“ verwenden und Logs beider Spieler anhängen. Mod-Fehler **nie** über die Bug-Report-Funktion im Spiel oder an
Pixelsplit melden. Tests mit dem Spike nur über `tools/spike/Start-Session.ps1` starten, weil es den Crash-Reporter abfängt. Falls sich nach einem Absturz mit Mod oder Dumper der Crash-Reporter meldet, nichts absenden. Vor Fehlermeldungen im Spiel-Forum die Mod deaktivieren und den Fehler ohne Mod nachstellen.
