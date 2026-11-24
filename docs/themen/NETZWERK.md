# Netzwerk, Lobby und Verbindung

Gilt für Build `26786272`. Datierte Befunde und Messungen stehen im [Archiv](../archive/GAME_ANALYSIS.md), Abschnitte unten unter „Historie“.

## Überblick

- Zwei eigene Spielinstanzen verbinden sich über eine Steam-Lobby. Der Host ist Listen-Server, das Spiel repliziert klassisch (nicht Iris).
- Neben der nativen Replikation gibt es einen eigenen Kontrollkanal über Steam Networking Sockets für nachgewiesene Lücken. Nachrichtentypen: [Kontrollprotokoll](../CONTROL_PROTOCOL.md).
- Der Host lässt nur Konten aus seiner Lobby zu und trennt ein Konto, das länger als `RevokeAfterMs` (10 s) in der Lobby fehlt (`lobby-auth`).
- Steam initialisiert das Spiel erst nach dem Laden der Mod; Transport und Lobby warten darauf.

## Code

| Datei | Aufgabe | Test |
|---|---|---|
| `src/Net/Transport.cpp`, `src/Net/SteamSockets.cpp` | Kontrollkanal über Steam Networking Sockets | `tests/NetTests/TransportTests.cpp`, `tests/NetTests/NetSimTests.cpp` |
| `src/Net/Session.cpp` | Sitzung, Lobby-Mitglieder, Zugriffskontrolle | `tests/NetTests/SessionTests.cpp`, `tests/NetTests/AuthorizationTests.cpp` |
| `src/Net/ControlProtocol.cpp` | Format der Kontrollnachrichten, `ModVersion` | `tests/NetTests/ControlProtocolTests.cpp`, `tests/NetTests/ProtocolTests.cpp` |
| `src/Net/SteamLobbyApi.cpp`, `src/Mod/Lobby.cpp` | Steam-Lobby erstellen und beitreten | `tests/NetTests/SessionTests.cpp` |
| `src/Sync/SessionLog.cpp` | Zustandszeile und Testermarken im Log | `tests/NetTests/SessionLogTests.cpp` |

Mehr zum Aufbau: [src/Net/README.md](../../src/Net/README.md).

## Offene Fehler

- [lobby-rauswurf](../faelle/lobby-rauswurf.md): Host trennt einen gesunden Client wegen „nicht mehr in der Lobby“.
- Wiederverbinden während eines Runs ist optional (`reconnect`, [Optionen](../OPTIONEN.md#reconnect)).

## Historie

Wortgleiche Befunde, nur zum Nachschlagen, nicht als Ist-Zustand lesen:

- [Netzwerk](../archive/GAME_ANALYSIS.md#netzwerk)
- [Technische Andockpunkte: neue statische Prüfung](../archive/GAME_ANALYSIS.md#technische-andockpunkte-neue-statische-prüfung)
- [Vorhandene Netzwerkbefunde](../archive/GAME_ANALYSIS.md#vorhandene-netzwerkbefunde)
- [Konfiguration](../archive/GAME_ANALYSIS.md#konfiguration)
- [Verbindungstest](../archive/GAME_ANALYSIS.md#verbindungstest)
- [Replikation zur Laufzeit](../archive/GAME_ANALYSIS.md#replikation-zur-laufzeit)
- [Ergaenzung: SDK-Schnittstellen fuer eigenen Sync](../archive/GAME_ANALYSIS.md#ergaenzung-sdk-schnittstellen-fuer-eigenen-sync)
- [Trennen ins Hauptmenü (`trennen`)](../archive/GAME_ANALYSIS.md#trennen-ins-hauptmenü-trennen)
- [Host-Absturz nach hartem Wegfall des Clients (`opt-host-absturz-trennung`)](../archive/GAME_ANALYSIS.md#host-absturz-nach-hartem-wegfall-des-clients-opt-host-absturz-trennung)
- [Steam-Lobby über das Internet (`lobby-live`)](../archive/GAME_ANALYSIS.md#steam-lobby-über-das-internet-lobby-live)
- [Sekündlicher Ruckler beim Host (`host-lagspike`)](../archive/GAME_ANALYSIS.md#sekündlicher-ruckler-beim-host-host-lagspike)
