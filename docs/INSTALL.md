# Installation für Spieler

Anleitung für das Release-Paket `DDCoop-<Version>.zip`. Wer an der Mod mitarbeitet, nimmt stattdessen
[tools/Deploy-Mod.ps1](../tools/README.md#deploy-modps1), siehe [Einrichtung](SETUP.md).

Die Mod ist ein inoffizielles Fan-Projekt ohne Verbindung zu Pixelsplit oder dem Publisher. Jeder Spieler braucht eine eigene Kopie
des Spiels auf Steam, und beide Spieler brauchen dieselbe Mod-Version.

## Voraussetzungen

- Windows 10 oder 11, Deadly Days: Roadtrip über Steam installiert.
- **Spiel-Build `26187885` oder `26495302`.** Offsets und Patches der Mod gelten nur für diese Builds. Auf einem anderen Build lehnt der Installer ab
  und nennt den gefundenen Build. Eine trotzdem vorhandene Mod schaltet sich beim Start selbst ab, das Spiel läuft dann ohne Mod.
- Das Spiel ist beendet.
- Keine andere Mod, die ebenfalls eine `version.dll` neben die Spiel-Exe legt.

## Installieren

1. Die ZIP-Datei vollständig in einen beliebigen Ordner entpacken, nicht direkt aus dem ZIP heraus starten.
2. `Install.cmd` doppelklicken. Der Installer sucht das Spiel in allen Steam-Bibliotheken, auch auf anderen Laufwerken.
3. Das Spiel wie gewohnt aus Steam starten.

Findet der Installer das Spiel nicht, den Spielordner angeben. Gemeint ist der Ordner, in dem `DDSurvivors` liegt:

```bat
Install.cmd -GameDir "D:\SteamLibrary\steamapps\common\Deadly Days Roadtrip"
```

Eine neue Mod-Version wird genauso installiert, sie ersetzt die alte. Einstellungen in `ddcoop.ini` bleiben erhalten.

## Entfernen

`Uninstall.cmd` doppelklicken. Danach startet das Spiel wieder ohne Mod. Logs und `ddcoop.ini` bleiben im Ordner `DDCoop`,
mit `Uninstall.cmd -Purge` wird auch dieser Ordner gelöscht.

Abschalten geht auch ohne Entfernen: In `DDSurvivors\Binaries\Win64\DDCoop\ddcoop.ini` die Zeile `enabled=false` eintragen.

## Was der Installer ändert

Nur zwei Dateien in `DDSurvivors\Binaries\Win64` im Spielordner:

| Datei | Aufgabe |
|---|---|
| `version.dll` | Loader: Das Spiel lädt ihn beim Start, er startet die Mod und reicht alles andere an die `version.dll` von Windows weiter |
| `DDCoop\DDCoop.dll` | Die Mod selbst |

Zur Laufzeit schreibt die Mod Logs nach `DDCoop\logs`. Spielstände, Steam-Einstellungen und alle anderen Spieldateien bleiben unberührt.
Eine `version.dll`, die nicht zu dieser Mod gehört, wird nie überschrieben oder gelöscht.

„Integrität der Spieldateien überprüfen“ in Steam entfernt die Mod nicht, weil die beiden Dateien nicht zum Spiel gehören.
Dafür `Uninstall.cmd` nehmen.

`SHA256SUMS.txt` im Paket enthält die Prüfsummen aller Dateien. Sie müssen mit denen der Veröffentlichung übereinstimmen.

## Fehlermeldungen

| Meldung | Bedeutung und Abhilfe |
|---|---|
| `Spiel-Build … statt 26187885 oder 26495302` | Steam hat das Spiel auf einen Build aktualisiert, den die Mod noch nicht kennt. Auf eine Mod-Version für diesen Build warten, bis dahin ohne Mod spielen. |
| `Steam nicht gefunden` oder `in keiner Steam-Bibliothek gefunden` | Den Spielordner mit `-GameDir` angeben, siehe oben. |
| `Steam-Manifest nicht gefunden` | Der angegebene Ordner liegt nicht in einer Steam-Bibliothek (`…\steamapps\common\…`). Nur Steam-Installationen werden unterstützt. |
| `Spiel-Exe nicht gefunden` | Mit `-GameDir` den Ordner angeben, in dem `DDSurvivors` liegt, nicht einen Unterordner. |
| `Das Spiel laeuft` | Spiel beenden, auch im Task-Manager prüfen, dann erneut ausführen. |
| `Fremde version.dll gefunden` | Eine andere Mod nutzt denselben Dateinamen. Erst diese entfernen, beide gleichzeitig gehen nicht. |
| `… fehlt. Das Paket vollstaendig entpacken.` | Das ZIP wurde nicht vollständig entpackt oder ein Virenscanner hat eine Datei entfernt. |

## Virenscanner und Proton

Die Mod lädt sich über eine nachgebaute `version.dll` und ändert Funktionen im laufenden Spiel. Manche Virenscanner halten das
für verdächtig.

**Geprüft:** Windows Defender (Signaturen `1.463.207.0`, 09.11.2026) meldet im entpackten Paket nichts, auch nicht in
`version.dll` und `DDCoop\DDCoop.dll`. Andere Scanner sind nicht geprüft.

**Meldet ein Virenscanner die Mod:**

1. Nur dem Paket von der offiziellen Release-Seite vertrauen. Die Prüfsummen stehen in `SHA256SUMS.txt` im Paket.
2. Prüfen, ob die Datei aus Quarantäne oder Ordner fehlt. Dann meldet der Installer `… fehlt. Das Paket vollständig entpacken.`
3. Die Meldung mit Scannername und Erkennungsname als Issue im Repo melden. Das Team meldet den Fehlalarm beim Hersteller,
   bei Microsoft über <https://www.microsoft.com/wdsi/filesubmission>.
4. Den Ordner nur ausnehmen, wenn die Prüfsummen stimmen. Nie den Virenscanner ganz abschalten.

**Steam Deck und Proton:** nicht unterstützt und nicht getestet. Die Mod ist nur unter Windows mit Steam geprüft. Wer es
trotzdem versucht, setzt in den Startoptionen `WINEDLLOVERRIDES="version=n,b" %command%`, damit Proton die `version.dll` der
Mod lädt. Fehler dabei bitte nicht als Fehler der Mod melden.

## Paket bauen

Für das Team: Das Paket entsteht im Repo mit

```bash
powershell -ExecutionPolicy Bypass -File tools/New-ReleasePackage.ps1 -Version <version>
```

Details unter [Tools](../tools/README.md#new-releasepackageps1).
