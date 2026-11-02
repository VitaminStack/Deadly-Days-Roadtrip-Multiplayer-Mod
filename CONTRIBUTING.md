# Zusammenarbeit

Beiträge sind willkommen, auch ohne Einladung ins Team. Wer keinen Schreibzugriff hat, erstellt einen Fork und öffnet von dort einen Pull
Request. Größere Vorhaben bitte vorher in einem Issue abstimmen, damit keine Arbeit doppelt entsteht. Mit einem Beitrag stimmst du zu,
dass er unter der [MIT-Lizenz](LICENSE) des Projekts steht.

## Arbeitsbereiche

Damit alle parallel arbeiten können, hat jeder Bereich eine hauptverantwortliche Person. Änderungen in fremden Bereichen sind
erlaubt, aber diese Person macht dann das Review.

| Bereich | Ordner | Verantwortlich |
|---|---|---|
| Fundament | `src/Loader`, `src/Mod`, `src/Core`, `tools` | offen |
| Netzwerk | `src/Net`, `tests` | offen |
| Spieler und Kampf | `src/Sync/Players`, `src/Sync/Combat`, `src/Game` | offen |
| Gegner und Welt | `src/Sync/Enemies`, `src/Sync/World`, `src/Game` | offen |
| Inventar und UI | `src/Sync/Inventory`, `src/UI`, `src/Game` | offen |
| Reverse Engineering | `research`, `docs/GAME_ANALYSIS.md` | offen |

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

Das Spiel wird noch aktiv gepatcht, und jedes Update kann Offsets und Klassen ändern.

1. In Steam, falls möglich, unter Eigenschaften → Updates „nur beim Start aktualisieren“ wählen und das Spiel während der Entwicklung nicht unnötig starten.
2. Nach einem Update meldet die RE-Person das im Team, erzeugt ein neues SDK und testet die Hooks auf einem Branch.
3. Build-ID und Exe-Version in [docs/GAME_ANALYSIS.md](docs/GAME_ANALYSIS.md#unterstützte-spielversion), [docs/OFFSETS.md](docs/OFFSETS.md), `$SupportedBuild` in `tools/spike/Common.ps1` sowie `SupportedBuilds` in `src/Core/Version.cpp` aktualisieren.
4. Erst dann ziehen alle anderen nach, weil alle denselben Build brauchen.

## Fehler melden

Das Issue-Template „Bug“ verwenden und Logs beider Spieler anhängen. Mod-Fehler **nie** über die Bug-Report-Funktion im Spiel oder an
Pixelsplit melden. Tests mit dem Spike nur über `tools/spike/Start-Session.ps1` starten, weil es den Crash-Reporter abfängt. Falls sich nach einem Absturz mit Mod oder Dumper der Crash-Reporter meldet, nichts absenden. Vor Fehlermeldungen im Spiel-Forum die Mod deaktivieren und den Fehler ohne Mod nachstellen.
