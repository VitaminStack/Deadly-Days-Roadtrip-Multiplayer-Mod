# Net

Verbindung zwischen Host und Client: Steam-Lobby, Kontrollkanal über Steam Networking Sockets, Sitzung und Zugriffskontrolle. Darf einbinden: `Core`.

## Muster

- Steam-Zugriffe nur über die flache C-Schnittstelle der `steam_api64.dll` des Spiels (`SteamSockets.cpp`, `SteamLobbyApi.cpp`), kein Steamworks-SDK im Repo.
- Tests laufen gegen `tests/NetTests/FakeSteam.*`, ohne Spiel und Steam.
- Neue Nachricht oder neues Format: zuerst in `docs/CONTROL_PROTOCOL.md` eintragen und `ModVersion` erhöhen.

## Wo was liegt

- `Transport.cpp`, `SteamSockets.cpp`: Kontrollkanal
- `Session.cpp`: Sitzung, Lobby-Mitglieder, Zugriffskontrolle (`RevokeAfterMs`), Neubeitritt des Clients
- `LobbyMembership.cpp`: wann eine Lobby-Liste gilt und wann der Host trennt
- `ControlProtocol.cpp`: Nachrichtenformat
- `SteamLobbyApi.cpp`: Lobby-Aufrufe

## Tests

`tests/NetTests`, lokal `bin\Release\NetTests.exe` oder `tools/Build-Mod.ps1 -Test`.

## Nicht tun

- Keine echten Steam-IDs oder persönlichen Daten in Tests, Logs im Repo oder Doku.
- Feste Wartezeiten in Tests vermeiden, siehe `tests/README.md`.

## Mehr

[README.md](README.md), Thema [Netzwerk](../../docs/themen/NETZWERK.md).
