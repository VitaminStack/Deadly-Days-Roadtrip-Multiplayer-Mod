---
name: patch-anlegen
description: Use when adding a new hook, byte patch, adapter or control message to src/Game or src/Sync of the coop mod.
---

# Patch anlegen

1. `src/Game/CLAUDE.md` und die Themen-Datei laut `docs/KONTEXT.md` lesen.
2. Die Prüfliste in `docs/ARCHITECTURE.md` (Abschnitt „Verbindliches Architekturprinzip: lokal zuerst, minimal synchronisieren“) beantworten: Was bleibt lokal, was wird geteilt, wer entscheidet, welcher Weg wird wiederverwendet, wie werden Ablehnung und Neuaufbau getestet.
3. Logik als freie Funktion in `*Policy.h`, zuerst der fehlschlagende Test in `tests/GameTests` (doctest).
4. Hook in `*Patches.cpp`: RVA als Konstante für Build `26786272`, Stellen mit Abstand als `At(Rva, Abstand)`. Prüfbytes mit `py tools/patch_signatures.py` erzeugen und prüfen, dann `py tools/check_patch_registry.py`.
5. Feldzugriffe über `Reflection::Property`. Ein fester Offset nur in `Offsets.h`, belegt in `docs/OFFSETS.md`, geprüft in `SdkChecks.cpp`.
6. Neue Kontrollnachricht: zuerst in `docs/CONTROL_PROTOCOL.md` (Belegte Nachrichtentypen) eintragen, `ModVersion` erhöhen, State-Test in `tests/NetTests`.
7. Bauen und testen: `tools/Build-Mod.ps1 -Test`. Spieltest mit `tools/spike/Start-Session.ps1 -Brief`, eine Sitzung pro PC.
