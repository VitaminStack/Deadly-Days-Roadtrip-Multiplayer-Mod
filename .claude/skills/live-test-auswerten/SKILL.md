---
name: live-test-auswerten
description: Use when evaluating logs and crash reports after a play session of the coop mod, on one or two PCs.
---

# Live-Test auswerten

1. Auf jedem PC `tools/spike/Collect-Logs.ps1` ausführen. Das Zip unter `research/local/livetest/` kommt nie ins Repo (IDs, Pfade).
2. Zuerst `logs/*.summary.txt` lesen, dann `crashes/` und `crash-events.txt`. Volle Logs nur abschnittsweise.
3. Offene Prüfungen holen: `py tools/tasks.py tests list`. Jeden Befund einer Kennung zuordnen.
4. Bestanden: `py tools/tasks.py tests pass <kennung> --note "<Beleg>"`. Teilweise: `tests update`. Durchgefallen: `tests reopen` und die Fallakte ergänzen (siehe Skill `fehler-untersuchen`).
5. Neue Funde: eine neue Kennung nur, wenn sie den Run zu zweit verhindern (P0/P1, Entscheidung E-08 in `docs/ENTSCHEIDUNGEN.md`). Sonst nur in die PR-Beschreibung.
6. Ergebnisse: Ist-Zustand in `docs/themen/` ersetzen, nichts Datiertes anhängen, das Archiv nicht ändern. Ablauf: `docs/TESTING.md`, Abschnitt „Ergebnisse festhalten“.
