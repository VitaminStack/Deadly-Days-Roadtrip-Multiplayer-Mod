# UI

Overlay der Mod (eigenes Win32-Toolfenster, F8): Lobby, Einladungen, Verbindungsstatus, Debug-Anzeige. Darf einbinden: `Core`, `Net`. Ändert den Spielzustand nur über `Net` oder `Sync`.

## Muster

- Kein Renderer-Hook, keine Engine-Aufrufe auf dem UI-Thread.
- Anzeige-Logik im Modell, getestet in `tests/UITests` (`OverlayModelTests.cpp`, `WindowTests.cpp`).

## Tests

`tests/UITests`, lokal `bin\Release\UITests.exe`.

## Mehr

[README.md](README.md).
