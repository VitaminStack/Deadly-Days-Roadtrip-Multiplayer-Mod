## Was ändert sich?

<!-- Ein bis drei Sätze. Issue verlinken: Closes #... -->

## Wie wurde getestet?

- Spiel-Build:
- Aufbau: <!-- nur gebaut / ein PC / zwei PCs / über Internet -->
- Ergebnis:

## Checkliste

- [ ] Solution baut in `Debug|x64` und `Release|x64`, CI ist grün
- [ ] Tests ergänzt, falls Code ohne SDK-Abhängigkeit geändert wurde
- [ ] SDK-Zugriffe nur in `src/Game`
- [ ] Protokollversion erhöht, falls das Netzwerkprotokoll geändert wurde
- [ ] Keine Spieldateien, SDK-Dateien oder persönlichen Pfade enthalten
- [ ] Ist-Zustand in `docs/themen/` angepasst (ersetzt, nicht angehängt), Befunde stehen in dieser Beschreibung oder der Fallakte
- [ ] Eigene Zeilen in `TODO.md` angepasst, bei fertiger Aufgabe `py tools/tasks.py done` (Fortschritt im README), `docs/ROADMAP.md` nur bei erledigter Phase
