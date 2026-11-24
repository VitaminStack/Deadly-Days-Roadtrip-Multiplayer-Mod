# Tools

Werkzeuge für Aufgaben, Doku, Build, Spieltests und Offsets. Python nur mit Standardbibliothek, Tests unter `tests/test_*.py`.

## Wichtigste Befehle

- `py tools/tasks.py list --short`: offene Aufgaben kurz. `show`, `claim`, `done`, `tests` siehe [README.md](README.md#taskspy).
- `py tools/task_status.py check` und `py tools/doc_check.py check`: müssen vor jedem PR bestehen.
- `tools/Build-Mod.ps1 -Test`: bauen und alle Tests.
- `tools/spike/Start-Session.ps1 -Brief`: Spielsitzung, eine pro PC, siehe [docs/TESTING.md](../docs/TESTING.md).

## Nicht tun

- Nie `-unattended`, nie fremde Spielinstanzen beenden.
- Keine persönlichen Pfade oder IDs in Skripte schreiben.

## Mehr

[README.md](README.md) (Nachschlagewerk, nicht ganz lesen).
