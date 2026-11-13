# Installation for players

Guide for the release package `DDCoop-<version>.zip`. Contributors use
[tools/Deploy-Mod.ps1](../tools/README.md#deploy-modps1) instead, see [setup](SETUP.md).

The mod is an unofficial fan project, not affiliated with Pixelsplit or the publisher. Every player needs their own copy
of the game on Steam, and both players need the same mod version.

## Requirements

- Windows 10 or 11, Deadly Days: Roadtrip installed through Steam.
- **Game build `26495302`.** The mod's offsets and patches only apply to this build. On any other build the installer refuses
  and names the build it found. A mod that is present anyway disables itself at startup, and the game runs without it.
- The game is closed.
- No other mod that also places a `version.dll` next to the game exe.

## Install

1. Extract the ZIP file completely into any folder; do not start it from inside the ZIP.
2. Double-click `Install.cmd`. The installer searches all Steam libraries for the game, including other drives.
3. Start the game from Steam as usual.

If the installer does not find the game, specify the game folder, meaning the folder that contains `DDSurvivors`:

```bat
Install.cmd -GameDir "D:\SteamLibrary\steamapps\common\Deadly Days Roadtrip"
```

A new mod version is installed the same way and replaces the old one. Settings in `ddcoop.ini` are kept.

## Remove

Double-click `Uninstall.cmd`. The game then starts without the mod again. Logs and `ddcoop.ini` stay in the `DDCoop` folder;
`Uninstall.cmd -Purge` deletes that folder too.

You can also disable the mod without removing it: add the line `enabled=false` to `DDSurvivors\Binaries\Win64\DDCoop\ddcoop.ini`.

## What the installer changes

Only two files in `DDSurvivors\Binaries\Win64` in the game folder:

| File | Purpose |
|---|---|
| `version.dll` | Loader: the game loads it at startup, it starts the mod and forwards everything else to the Windows `version.dll` |
| `DDCoop\DDCoop.dll` | The mod itself |

At runtime the mod writes logs to `DDCoop\logs`. Savegames, Steam settings and all other game files stay untouched.
A `version.dll` that does not belong to this mod is never overwritten or deleted.

"Verify integrity of game files" in Steam does not remove the mod, because the two files are not part of the game.
Use `Uninstall.cmd` for that.

`SHA256SUMS.txt` in the package contains the checksums of all files. They must match those of the release.

## Error messages

| Message | Meaning and fix |
|---|---|
| `Game build … instead of 26495302` | Steam updated the game to a build the mod does not know yet. Wait for a mod version for that build, and play without the mod until then. |
| `Steam not found` or `not found in any Steam library` | Specify the game folder with `-GameDir`, see above. |
| `Steam manifest not found` | The given folder is not in a Steam library (`…\steamapps\common\…`). Only Steam installations are supported. |
| `Game exe not found` | Use `-GameDir` to specify the folder that contains `DDSurvivors`, not a subfolder. |
| `The game is running` | Close the game, also check the Task Manager, then run again. |
| `Foreign version.dll found` | Another mod uses the same file name. Remove it first; both at once do not work. |
| `… is missing. Extract the package completely.` | The ZIP was not fully extracted, or an antivirus removed a file. |

## Antivirus and Proton

The mod loads through a replacement `version.dll` and modifies functions in the running game. Some antivirus programs
consider that suspicious.

**Checked:** Windows Defender (signatures `1.463.207.0`, 9 Nov 2026) reports nothing in the extracted package, including
`version.dll` and `DDCoop\DDCoop.dll`. Other scanners were not checked.

**If an antivirus reports the mod:**

1. Only trust the package from the official release page. The checksums are in `SHA256SUMS.txt` in the package.
2. Check whether the file is missing from quarantine or folder. The installer then reports `… is missing. Extract the package completely.`
3. Report the message with the scanner name and detection name as an issue in the repo. The team reports the false positive
   to the vendor, for Microsoft at <https://www.microsoft.com/wdsi/filesubmission>.
4. Only add an exclusion for the folder if the checksums match. Never turn off the antivirus completely.

**Steam Deck and Proton:** not supported and not tested. The mod is only verified on Windows with Steam. If you try anyway,
set `WINEDLLOVERRIDES="version=n,b" %command%` in the launch options so Proton loads the mod's `version.dll`. Please do not
report failures there as mod bugs.

## Build the package

For the team: the package is built in the repo with

```bash
powershell -ExecutionPolicy Bypass -File tools/New-ReleasePackage.ps1 -Version <version>
```

Details under [Tools](../tools/README.md#new-releasepackageps1).
