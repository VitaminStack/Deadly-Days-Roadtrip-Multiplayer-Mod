# Mod

Die Mod-DLL: bindet alle Module zusammen und startet sie in der richtigen Reihenfolge. Darf einbinden: `Core`, `Net`, `Game`, `Sync`, `UI`.

## Wo was liegt

- `Mod.cpp`: Start und Reihenfolge der Module
- `Lobby.cpp`: Treffpunkt beider Spieler, wartet auf Steam
- `Control.cpp`: Testkanal für Skripte, antwortet mit Fehler statt zu hängen, wenn der Game Thread nicht läuft

## Mehr

[README.md](README.md), Spieltests in [docs/TESTING.md](../../docs/TESTING.md).
