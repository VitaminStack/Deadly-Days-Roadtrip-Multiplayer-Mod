# Third Party

Liste der Abhängigkeiten mit Version und Lizenz. Noch ist nichts endgültig festgelegt.

| Bibliothek | Zweck | Version | Lizenz | Status |
|---|---|---|---|---|
| [MinHook](https://github.com/TsudaKageyu/minhook) | Funktionen abfangen | 1.3.4 (`c3fcafd`) | BSD-2-Clause, siehe `minhook/LICENSE.txt` | Unveränderte Quellkopie unter `minhook/`, kompiliert in `Core` |
| Dear ImGui | Overlay | offen | offen | Auswahl in Phase 2 |
| [doctest](https://github.com/doctest/doctest) | C++-Tests | 2.5.3 (`2d0a935`) | MIT, siehe `doctest/LICENSE.txt` | Unveränderter Header unter `doctest/`, nur für `tests/CoreTests` |
| Steamworks SDK 1.64 | Lobby und P2P | 1.64 | Steamworks-Lizenz | Jeder lädt es selbst herunter, nicht im Repo |
| [Dumper-7](https://github.com/Encryqed/Dumper-7) | SDK erzeugen (Werkzeug) | upstream `2c1eae3` plus Fix, Fork-Commit `2570b5b` | Keine Lizenz angegeben | Submodul `Dumper-7/` auf den eigenen Fork [VitaminStack/Dumper-7](https://github.com/VitaminStack/Dumper-7), Branch `main`. Der Fork ist dauerhaft die Quelle für die Mod, upstream wird nur bei Bedarf übernommen. Keine Kopie im Repo, weil Dumper-7 keine Lizenz hat |

Quellkopien bleiben unverändert, damit ein Update nur das Ersetzen der Dateien ist.

Lizenz vor dem Hinzufügen prüfen und hier eintragen.
