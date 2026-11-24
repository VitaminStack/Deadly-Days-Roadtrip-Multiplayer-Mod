---
name: fehler-untersuchen
description: Use when investigating a crash or wrong behaviour of a task in this coop mod (a Kennung from TODO.md), before proposing a fix.
---

# Fehler untersuchen

1. `py tools/tasks.py show <kennung>` lesen. Fallakte `docs/faelle/<kennung>.md` öffnen; fehlt sie, nach der Vorlage in `docs/PROZESS.md` (Abschnitt „Vorlagen“) anlegen.
2. Ist-Zustand nur aus der Themen-Datei laut `docs/KONTEXT.md` und dem Code nehmen. Das Archiv `docs/archive/` nur per `grep -n` für die Anker, die die Fallakte nennt.
3. Logs zuerst als Kurzfassung: `py tools/log_summary.py <log>` (oder die `*.summary.txt` aus `Collect-Logs.ps1`). Das volle Log nur abschnittsweise.
4. Jede Aussage in der Fallakte markieren: `[Belegt]` mit Logauszug oder Messung auf Build `26786272`, `[Vermutung]` mit Prüfidee, `[Widerlegt]` wenn geprüft und falsch.
5. Erst die Ursache belegen, dann ändern. Der Fix beginnt mit einem fehlschlagenden Test der zuständigen Policy in `tests/GameTests` oder des States in `tests/NetTests`.
   Ein behobener Absturz bekommt immer einen Regressionstest oder ein festes Szenario für `tools/spike/Run-Route.ps1`.
6. Nach jedem Schritt „Nächster Schritt“ in der Fallakte ersetzen, nicht anhängen.
7. Abnahme erfüllt: Themen-Datei auf den neuen Ist-Zustand bringen, Fallakte löschen, `py tools/tasks.py done <kennung> "<ein Satz>"`. Messungen in die PR-Beschreibung.
   Fehlt nur noch die Prüfung zu zweit: `py tools/tasks.py tests pending <kennung> --note "<Stand>"`. Das schließt die Aufgabe ab; der Test bleibt nur als Info.
