# Loader

Proxy-DLL, die das Spiel beim Start lädt und die Mod startet. Enthält nur Weiterleitungen und den Start der Mod, keine Spiellogik.

- Baut `version.dll`, die neben der Exe in `DDSurvivors/Binaries/Win64/` liegt.
- Leitet alle 17 Exporte der System-`version.dll` weiter, nicht nur die drei Importe der Exe. Fehlt anderen Modulen im Prozess ein Export, schlägt deren Laden fehl.
- Das Original kommt über den absoluten Systempfad, siehe [Andockpunkte](../../docs/ARCHITECTURE.md#technische-andockpunkte-build-26187885).
- `DllMain` startet nur einen Thread. Dieser lädt `DDCoop/DDCoop.dll` neben der Proxy-DLL und ruft `ModStart` auf. Fehlt der Ordner, läuft das Spiel unverändert.

Der Loader hängt von keinem Modul ab, damit er sich selten ändert.
