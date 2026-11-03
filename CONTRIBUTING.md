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

Jede Aufgabe hat in [TODO.md](TODO.md#wo-kann-ich-anfangen) eine Kennung, auch die [späteren Pakete](TODO.md#spätere-arbeitspakete) und die [zentrale Arbeit](TODO.md#zentrale-arbeit). Reserviert ist, was einen Branch `task/<kennung>` auf GitHub hat.
Der erste Push gewinnt, ein zweiter wird abgelehnt. Das gilt für Menschen und KI-Agenten, die genauen Befehle stehen in [CLAUDE.md](CLAUDE.md#vor-dem-start).

`py tools/tasks.py list` zeigt jede Kennung mit Status: frei, reserviert, in Arbeit, PR wartet auf Review oder erledigt.
`py tools/tasks.py claim <kennung> <name>` reserviert und öffnet gleich einen Draft-PR `[WIP] <kennung>`. Wer gerade woran arbeitet,
steht deshalb auf GitHub unter „Pull requests“. Für den Status aus den PRs braucht das Skript die angemeldete [GitHub CLI](https://cli.github.com/) (`gh auth login`), sonst sieht es nur die Branches.
Wer über einen Fork arbeitet, kann so nicht reservieren und legt stattdessen ein Issue an.

### Branches

```text
task/<kennung>                          task/inventar
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

1. In Steam unter Eigenschaften → Updates „Dieses Spiel nur beim Starten aktualisieren“ wählen.
2. Das Spiel während der Entwicklung nur über `tools/spike/Start-Session.ps1` starten, nie über Steam. Das Skript startet die Exe direkt statt über
   den Steam-Startknopf. Das ist keine dauerhafte Update-Sperre; ob eine alte Version nach einem Update weiter startet, ist noch nicht getestet.
   Es bricht ab, wenn das Steam-Manifest einen anderen Build meldet. Ein Dateiabgleich ist das nicht.
3. Eine Kopie des Spielordners außerhalb des Repos aufheben, etwa 1,8 GB. Kommt doch ein Update durch, ist das der schnellste Weg zurück.
4. Ohne Kopie kommt ein Download über die Steam-Konsole infrage (`steam://open/console`), mit den Manifest-IDs aus der
   [Spielversion](docs/GAME_ANALYSIS.md#unterstützte-spielversion):

   ```text
   download_depot 3026450 3026451 1832737755528485607
   download_depot 3026450 3026452 7724756528743620812
   ```

   Erwartetes Ziel ist `steamapps/content/app_3026450/depot_<Depot-ID>/`. Dieser Weg ist noch nicht ausprobiert; bekannte Manifest-IDs allein
   belegen weder Downloadberechtigung noch spätere Verfügbarkeit. Eine Wiederherstellung noch nicht durch blindes Zusammenkopieren versuchen:
   alte und neue Dateien sowie das Steam-Manifest können danach auseinanderliegen. Die vorhandene Installation und Spielstände bleiben beim
   ersten Test unangetastet, verglichen wird zunächst im separaten Downloadordner.

**Stand und Abnahme:** Der gemeinsame Build ist aktuell öffentlich gelistet, ein dauerhaftes Festhalten für alle aber noch nicht belegt.
Siehe [Build-Abgleich und offene Team-Prüfung](docs/SETUP.md#gleiche-build-id-im-team). Steam-Einstellungen und Downloads werden nicht automatisch geändert.

### Wenn ein Update erscheint

1. Wer es bemerkt, meldet es im Team mit der neuen Build-ID. Niemand aktualisiert, die Spieltests laufen auf dem alten Build weiter.
2. Nachgezogen wird nur bei Bedarf: wenn das Update etwas bringt, das wir brauchen, zum Beispiel einen Fix für einen blockierenden Absturz.
   Spätestens vor `integration-run` und `release`, weil Spieler immer den neuesten Build haben.
3. Das Nachziehen ist die zentrale Aufgabe `spiel-update`. Eine Person reserviert sie, aktualisiert ihr Spiel, erzeugt ein neues SDK und passt Offsets,
   Signaturen und Patches auf ihrem Branch an.
4. Im selben PR Build-ID, Exe-Version und Manifest-IDs in [docs/GAME_ANALYSIS.md](docs/GAME_ANALYSIS.md#unterstützte-spielversion), die Werte in
   [docs/OFFSETS.md](docs/OFFSETS.md), `$SupportedBuild` in `tools/spike/Common.ps1` sowie `SupportedBuilds` in `src/Core/Version.cpp` aktualisieren.
5. Erst nach dem Merge aktualisieren alle anderen ihr Spiel, weil alle denselben Build brauchen. Die Kopie des alten Builds bleibt, bis der neue läuft.

## Fehler melden

Das Issue-Template „Bug“ verwenden und Logs beider Spieler anhängen. Mod-Fehler **nie** über die Bug-Report-Funktion im Spiel oder an
Pixelsplit melden. Tests mit dem Spike nur über `tools/spike/Start-Session.ps1` starten, weil es den Crash-Reporter abfängt. Falls sich nach einem Absturz mit Mod oder Dumper der Crash-Reporter meldet, nichts absenden. Vor Fehlermeldungen im Spiel-Forum die Mod deaktivieren und den Fehler ohne Mod nachstellen.
