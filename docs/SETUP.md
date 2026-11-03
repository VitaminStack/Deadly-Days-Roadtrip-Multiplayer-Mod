# Entwicklungsumgebung einrichten

Danach lässt sich die Mod bauen, testen und im Spiel starten. Das dauert wenige Minuten, ohne SDK.

## Benötigt

| Werkzeug | Wofür |
|---|---|
| Deadly Days: Roadtrip (Steam) | Testen, jeder braucht eine eigene Kopie |
| Visual Studio 2022 oder neuer mit „Desktopentwicklung mit C++“ | Mod und Spike bauen, `dumpbin` für die Disassembly |
| Python 3 mit Launcher `py` | Tests und Testskripte, nur Standardbibliothek |
| Git | Versionsverwaltung |

**Nur bei Bedarf:**

| Werkzeug | Wofür |
|---|---|
| Dumper-7 | SDK zum Nachschlagen von Klassen, Pflicht erst für `src/Game` |
| Steamworks SDK 1.64 | Header für Steam-Funktionen (kostenloser Download für Steam-Partner-Konten, nicht im Repo) |
| FModel oder repak | Paks und Configs auslesen |
| Steam-Konto je Testspieler | Online-Tests. Für Tests auf einem PC reicht ein Konto |

## Einrichtung

1. Repo mit Submodulen klonen: `git clone --recurse-submodules <url>`. Wer schon ohne geklont hat: `git submodule update --init`.
2. `config/Local.props.example` nach `config/Local.props` kopieren und `GameDir` eintragen. Die Datei bleibt lokal. Worktrees unter `.worktrees/` nutzen die aus dem Hauptordner.
3. Spiel auf die [unterstützte Version](GAME_ANALYSIS.md#unterstützte-spielversion) bringen.
4. Einrichtung prüfen. Das Skript ändert nichts und nennt, was fehlt:

   ```bash
   powershell -ExecutionPolicy Bypass -File tools/Test-Setup.ps1
   ```

5. `DeadlyDaysCoop.sln` in Visual Studio öffnen und `Release|x64` bauen, oder in der „Developer PowerShell für VS“:

   ```bash
   msbuild DeadlyDaysCoop.sln /m /p:Configuration=Release /p:Platform=x64
   ```

6. Tests ausführen, wie in [tests/README.md](../tests/README.md) beschrieben. Dieselben Tests laufen bei jedem Pull Request in GitHub Actions.
7. Im Spiel testen mit [docs/TESTING.md](TESTING.md#mod-im-spiel-testen): Die Mod kommt mit `tools/Deploy-Mod.ps1` ins Spiel, die Sitzung startet `tools/spike/Start-Session.ps1`. Danach die Mod mit `-Remove` wieder entfernen.

Dann eine Aufgabe nehmen und reservieren, siehe [Wo kann ich anfangen?](../TODO.md#wo-kann-ich-anfangen) und [Aufgaben reservieren](../CONTRIBUTING.md#aufgaben-reservieren).

## Gleiche Build-ID im Team

**Ergebnis der lesenden Prüfung vom 03.11.2026:** Build `26187885` und die bekannten Depot-Manifeste sind weiterhin öffentlich gelistet
und lokal installiert. Für eine Zusage „alle können dauerhaft auf diesem Build bleiben“ fehlen Wiederherstellungs- und Team-Tests.
Der Phase-0-Punkt bleibt deshalb offen, die [Update-Regel](../CONTRIBUTING.md#spiel-updates) gilt unverändert.
Belege und Dateireferenz stehen in [Gemeinsame Build-ID](GAME_ANALYSIS.md#gemeinsame-build-id-lesende-prüfung-build-id).

### Jetzt auf jedem PC prüfen, ohne etwas zu ändern

Im Repo-Ordner in PowerShell ausführen. Das liest nur lokale Dateien, startet kein Spiel und verändert Steam nicht:

```powershell
$ErrorActionPreference = 'Stop'
. .\tools\spike\Common.ps1
Assert-SupportedBuild
$GameDir = Get-GameDir
$Manifest = Join-Path $GameDir '..\..\appmanifest_3026450.acf'
Select-String -LiteralPath $Manifest -Pattern '"buildid"', '"manifest"', '"AutoUpdateBehavior"'
$Files = @('DDSurvivors/Binaries/Win64/DDSurvivors-Win64-Shipping.exe')
$Files += Get-ChildItem -LiteralPath (Join-Path $GameDir 'DDSurvivors/Content/Paks') -File |
    Where-Object { $_.Extension -in '.pak', '.ucas', '.utoc' } |
    ForEach-Object { 'DDSurvivors/Content/Paks/' + $_.Name }
$Files | Sort-Object | ForEach-Object {
    $Hash = (Get-FileHash -LiteralPath (Join-Path $GameDir $_) -Algorithm SHA256).Hash.ToLowerInvariant()
    '{0}  {1}' -f $Hash, $_
}
```

Erwartet: Build-ID `26187885`, Depot-Manifeste wie in der Spielversion, genau die sechs Dateien aus der Dateireferenz und deren Hashes.
Weitere Container oder abweichende Hashes zuerst klären, nicht den Versionsschutz abschalten.
`Select-String` zeigt lokale Dateipfade an: Diese Ausgabe, das ganze Steam-Manifest und Kontodaten nicht öffentlich posten.
Für die Rückmeldung reichen Datum, Build-ID, „sechs Hashes gleich/abweichend“ und das Prüfergebnis; keine persönlichen Pfade oder Steam-IDs.
Die Einstellung „nur beim Starten aktualisieren“ muss die jeweilige Person zusätzlich in Steam bestätigen; der Lesetest stellt sie nicht ein.

### Was zum Abhaken noch fehlt

Jedes aktive Testmitglied bestätigt für die eigene lizenzierte Spielkopie:

1. Build-ID und sechs Vergleichsdateien stimmen; Update-Einstellung kontrolliert.
2. Eine eigene Sicherung außerhalb der von Steam verwalteten Installation ist vorhanden. Nicht nur die Exe sichern;
   Spielordner und zugehöriges Steam-Manifest privat aufheben. Nicht im Repo oder untereinander als Spielpaket verteilen.
3. Eine Wiederherstellung aus dieser Sicherung ist separat geprüft: keine Mischung alter und neuer Dateien, passende Inhalte,
   Start über den geschützten Testweg bei laufendem Steam. Das ist mit den aktuellen Skripten nicht automatisch für externe Kopien gelöst.
4. Falls die Sicherung fehlt: die beiden `download_depot`-Befehle aus der Update-Regel im eigenen Konto separat testen,
   erfolgreichen Abschluss und Dateiinhalte prüfen. Vorher nicht die bestehende Installation überschreiben. Auch ein erfolgreicher Download
   des heutigen `public`-Builds garantiert keine spätere Abrufbarkeit, wenn der Publisher ihn ersetzt oder den Zugriff ändert.

Ein erfolgreicher Test auf einem Konto ersetzt die Bestätigung der anderen nicht. Es gibt derzeit weder einen belegten Rückweg nach einem
echten Update noch einen Nachweis für zwei PCs mit verschiedenen Konten. Kein Spielstart oder Download wurde für diese Untersuchung ausgeführt.
Ein künftiger Online-Test darf sich nicht allein auf den Steam-Offlinemodus als Update-Sperre verlassen.

**Falls jemand den Stand nicht halten oder wiederherstellen kann:** keine gemischten Builds testen. Das Team entscheidet über das gemeinsame
Nachziehen mit der zentralen Aufgabe `spiel-update` oder fragt den Publisher nach einem stabilen Altversions-Zweig. Die Versionsprüfung bleibt an.

## SDK

Solution, Tests und Spike brauchen kein SDK, der Spike kommt mit den [Offsets](OFFSETS.md) aus. Das SDK hilft beim Nachschlagen von Klassen und wird für `src/Game` benötigt.

Es wird aus dem Spiel erzeugt und liegt außerhalb des Repos, zum Beispiel unter `C:\Dumper-7\<Version>\CppSDK`. Den Pfad trägt man als `Dumper7SdkRoot` in `config/Local.props` ein.
Alle im Team müssen ein SDK **desselben Spiel-Builds** verwenden.

> **Wichtig:** Dumper-7 braucht für UE 5.8 einen Fix und das SDK eine kleine Nacharbeit,
> siehe [SDK-Erzeugung unter UE 5.8](GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58).
> Der getestete Stand mit Fix liegt als Submodul unter `third_party/Dumper-7`. Gebaut wird er mit [`tools/dumper7/Build-Dumper7.ps1`](../tools/README.md#dumper7), kein eigener Patch nötig.

Das Repo ist öffentlich, darum wird das SDK nicht darüber verteilt. Alle erzeugen es selbst aus der eigenen Spielkopie.

## Logs und Spielstand

- Jede Spielinstanz schreibt `DDSurvivors/Binaries/Win64/DDCoop/logs/ddcoop-<pid>.log`. Bei Bug-Reports bitte die Logs beider Spieler anhängen.
- Der echte Spielstand liegt unter `%LOCALAPPDATA%\DDSurvivors\Saved\SaveGames`. Die Testskripte nutzen ein eigenes `-UserDir` und berühren ihn nicht.
