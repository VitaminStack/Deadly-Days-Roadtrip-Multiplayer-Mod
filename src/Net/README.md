# Net

Verbindung zwischen den Spielern: Transport-Tunnel, Steam-Lobby, Session, Handshake und Serialisierung.
Bindet kein SDK ein und ist deshalb ohne Spiel testbar. Greift nie direkt auf Unreal-Objekte zu.

Einstieg ist `Net::Start` in `Net/Net.h`. Darf `Core` nutzen. Daten für den Game Thread gehen über `Core::GameThread().Post`.

## Bausteine

| Datei | Aufgabe |
|---|---|
| `SteamSockets.h` | Schmale Sicht auf `ISteamNetworkingSockets`. `LoadSteamSockets` nutzt die Steam-Laufzeit des Spiels, Tests eine nachgebaute |
| `Transport.h` | Tunnel zwischen dem UDP des Spiels und Steam Networking, Host und Client |
| `Net.h` | `Start` und der laufende Transport |

## Transport (`transport`)

Das Spiel hat keinen Steam-NetDriver, aber den `IpNetDriver`. Der Tunnel lässt das Spiel weiter UDP auf 127.0.0.1 sprechen und
leitet jedes Paket unverändert über Steam Networking weiter. Keine Portfreigabe, Steam vermittelt und nutzt notfalls Relays.

```text
Client-Spiel ─ UDP 127.0.0.1:7778 ─ Transport ─ Steam P2P ─ Transport ─ UDP 127.0.0.1:7777 ─ Host-Spiel
```

- `StartHost` nimmt Verbindungen per P2P (virtueller Port 0) und über 127.0.0.1:7790 an. Je Client öffnet er einen eigenen
  UDP-Socket zum Spiel, das Spiel sieht also jeden Client unter einer eigenen Adresse.
- `StartClient(SteamID64)` verbindet per P2P zum Konto des Hosts, `StartLoopbackClient` über 127.0.0.1 zu einem Host auf demselben PC.
  Steam lehnt P2P zum eigenen Konto ab, darum braucht ein PC mit einem Konto den Weg über 127.0.0.1.
- Pakete gehen unzuverlässig ohne Nagle, die Zuverlässigkeit macht die Engine selbst. Pakete des Spiels vor dem Aufbau zählen als verworfen.
- `Stop` schließt Verbindungen und Listen-Sockets, gibt die UDP-Ports frei und darf mehrfach kommen. Danach ist ein neuer Start möglich.
- `Stats` liefert Zustand, Clients, Ping, Qualität, Pakete und Route (`Relay fra` oder `ohne Relay`) für Overlay und Fehlerbehandlung.
  Endet die Verbindung, meldet der Client `Failed` mit Grund, der Host wartet weiter auf Clients.
- Der Tunnel fasst keine Unreal-Objekte an. Listen-Server und `open 127.0.0.1:7778` veranlasst der Aufrufer.
- Logs ohne SteamIDs und IP-Adressen: Routen und Steam-Meldungen laufen durch `MaskAddresses`.

Die Steam-Funktionen kommen über die flache C-Schnittstelle aus der `steam_api64.dll` des Spiels (Steamworks 1.64,
`ISteamNetworkingSockets` v012), ein SDK ist nicht nötig. Herkunft und Messungen: [Steam-Tunnel](../../research/spikes/a-connect/README.md#spike-e-steam-tunnel-tunnel).

### Testschalter

Bis die Steam-Lobby den Transport startet, geht das über die Kommandozeile des Spiels:

| Schalter | Wirkung |
|---|---|
| `-ddcoop_transport=host` | Host-Tunnel |
| `-ddcoop_transport=loopback` | Client über 127.0.0.1, Host auf demselben PC |
| `-ddcoop_transport=<SteamID64>` | Client per P2P zum Konto des Hosts |

`Net::Start` wartet bis zu 120 s, bis das Spiel Steam initialisiert hat. Der Client versucht es 24-mal im Abstand von etwa 5 s,
weil beide Seiten im Test gleichzeitig starten. `Start-Session.ps1 -Mod -Tunnel Mod` setzt die Schalter und verbindet danach
das Client-Spiel mit `open 127.0.0.1:7778`, siehe [Testen](../../docs/TESTING.md).

## Tests

`tests/NetTests` prüft den Tunnel ohne Spiel und ohne Steam: Ein nachgebautes Steam Networking verbindet mehrere Konten im
Speicher, das UDP zum Spiel ist echt (Ports ab 47710). Geprüft sind Pakete in beide Richtungen, P2P und 127.0.0.1, abgelehnte
Verbindungen, belegter Port, Ende der Verbindung auf beiden Seiten, wiederholtes `Stop` mit Neustart, die Testschalter und das
Maskieren von Adressen. Läuft über `Build-Mod.ps1 -Test` und in der CI.
