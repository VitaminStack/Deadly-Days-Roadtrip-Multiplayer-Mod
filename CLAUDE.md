# Arbeitsregeln für KI-Agenten

Online-Coop-Mod für Deadly Days: Roadtrip. Unterstützt wird nur der neueste Spiel-Build, derzeit `26786272` ([E-07](docs/ENTSCHEIDUNGEN.md#e-07-nur-der-neueste-spiel-build)).
Doku auf Deutsch, Code und Bezeichner auf Englisch. Mehrere Agenten arbeiten parallel.

## Einstieg

1. `py tools/tasks.py list --short` zeigt die offenen Aufgaben, `py tools/tasks.py show <kennung>` eine davon.
2. [docs/KONTEXT.md](docs/KONTEXT.md) nennt je Thema die eine Datei mit dem Ist-Zustand, den Code und die Tests. Nur diese lesen.
3. Wer einen Fehler bearbeitet, arbeitet mit seiner Fallakte unter `docs/faelle/` und legt sie an, falls sie fehlt.

Große Dateien (über 60 KB), `docs/archive/` und `docs/ERLEDIGT.md` nie ganz lesen, nur per `grep -n` und Abschnitt.
Das Archiv ist Geschichte, nicht Ist-Zustand.

## Fokus

Alle Phasen sind abgeschlossen. `TODO.md` listet alle Probleme und Verbesserungen, ohne Abhängigkeiten ([E-13](docs/ENTSCHEIDUNGEN.md#e-13-phasen-abgeschlossen-flache-aufgabenliste)).
Probleme, die den Run zu zweit stören, vor Verbesserungen; einen Fund mit derselben Ursache im selben PR beheben.
Einzelheiten: [Fokus](docs/PROZESS.md#fokus).

## Architektur: lokal zuerst, minimal synchronisieren

Vorhandene Logik und Darstellung lokal beim Client ausführen, nur das für Multiplayer nötige Minimum teilen ([E-01](docs/ENTSCHEIDUNGEN.md#e-01-lokal-zuerst-minimal-synchronisieren)).
Pro gemeinsamem Zustand genau eine Autorität: Vergabe, Schaden, Gegner und Fortschritt bucht nur der Host.
Zuerst native Replikation nutzen, eigene Nachrichten nur für belegte Lücken.
Vor jedem neuen Sync- oder Hook-Pfad die Prüfliste in [ARCHITECTURE.md](docs/ARCHITECTURE.md#verbindliches-architekturprinzip-lokal-zuerst-minimal-synchronisieren) beantworten.

## Harte Regeln

- Spieldateien, SDK, decompilierter Code, persönliche Pfade und IDs nie ins Repo ([CONTRIBUTING.md](CONTRIBUTING.md#was-nie-ins-repo-darf)). Das Repo ist öffentlich.
- Eine Spielsitzung pro PC, `Start-Session.ps1 -Brief`; nie fremde Instanzen beenden, die Exe nie von Hand mit dem Spike starten, nie `-unattended`.
- Neue Kontrollnachricht oder anderes Format: zuerst in [CONTROL_PROTOCOL.md](docs/CONTROL_PROTOCOL.md#belegte-nachrichtentypen) eintragen und `ModVersion` erhöhen.
- Offsets gelten nur für den unterstützten Build, neue Werte in [OFFSETS.md](docs/OFFSETS.md).
- Doku beschreibt den Ist-Zustand: ersetzen statt anhängen. Befunde in PR-Beschreibung und Fallakte, nie ins Archiv.

## Ablauf

`claim` → Arbeit im eigenen Worktree unter `.worktrees/` → `py tools/task_status.py check` und `py tools/doc_check.py check` → `done` → PR fertigstellen.
Merge nur auf ausdrückliche Anweisung des Menschen. Vollständige Schritte: [Arbeitsablauf](docs/PROZESS.md#vor-dem-start), [Beim Arbeiten](docs/PROZESS.md#beim-arbeiten), [Abschluss](docs/PROZESS.md#abschluss).
