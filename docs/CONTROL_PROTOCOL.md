# Kontrollkanal, Version 1

`Net::Transport` trennt jetzt UE-Pakete und Mod-Kontrollnachrichten in einem
gemeinsamen Steam-Socket. Der Inhalt eines UE-Pakets bleibt unverändert; nur auf
dem Steam-Weg wird er eingerahmt. Alte Mod-Versionen ohne Rahmen sind inkompatibel
und werden nicht still als Spielpakete akzeptiert.

## Handshake

Beide Seiten senden nach Steam-Verbindungsaufbau zuerst zuverlässig ihre
Spiel-Build-ID, Mod-Kompatibilitätsversion und Rolle. Erst bei gleicher Version
und entgegengesetzter Rolle wird die Verbindung freigegeben. Vorher werden keine
UE- oder Kontrollpakete ins Spiel geliefert. Ein Hello hat fünf Sekunden Zeit.
Unzuverlässige UE-Pakete dürfen das Hello überholen und werden dann verworfen;
Steam hält Hello und nachfolgende zuverlässige Kontrollnachrichten geordnet.

Die Mod-Kompatibilitätsversion steht in `Net/Protocol.h`. Änderungen an Format
oder Bedeutung müssen sie erhöhen. Der Core-Buildschutz prüft separat, ob das
lokale Spiel tatsächlich unterstützt ist. Lobby-Metadaten ersetzen diesen
Verbindungs-Handshake nicht.

## Format

Alle Ganzzahlen explizit Little Endian, keine nativen Structs:

| Rahmenfeld | Bytes |
|---|---:|
| Magic `DDCP` | 4 |
| Protokollversion | 2 |
| Art: Hello=1, UE=2, Control=3 | 1 |
| Reserviert, zwingend 0 | 1 |
| Exakte Nutzlastlänge | 4 |

Maximal 65536 Nutzlastbytes. Hello: Build (4), Mod-Version (4), Host-Flag (1).
Control: Run-ID (8), Welt-Epoche (8), Revision (8), Nachrichtentyp (2), Nutzlast.
Alle vier Control-Kennungen müssen ungleich 0 sein. Keine Prozessadressen oder
lokalen UObject-/FName-IDs übertragen. Die jeweilige Sync-Familie besitzt und
validiert die Nutzlast, insbesondere Request-ID und erwartete Revision bei
Client-Anfragen. Der Transport erfindet keine Gameplay-Semantik.

## Benutzung und Lebensdauer

- `CurrentTransport()->ControlPeers()` liefert ausschließlich bestätigte lokale
  Verbindungstoken. Der Host lässt genau einen Remote-Spieler zu. Ein Client kann
  keine Spielerkennung im Envelope behaupten; die Spielzuordnung muss der Host
  aus diesem Token ableiten. Steam authentifiziert P2P-Verbindungen; Loopback ist
  nur der lokale Zwei-Instanzen-Test und kein Identitätsnachweis zweier Menschen.
- `SendControl(peer, value)` kopiert in eine begrenzte Queue. `false` bedeutet
  **nicht angenommen** (etwa Disconnect oder volle Queue). Der Aufrufer muss den
  Zustand behalten und erneut als Snapshot anbieten, statt Erfolg anzunehmen.
- `TakeControlMessages()` auf dem Game Thread leeren. Nachrichten enthalten das
  lokale Token der tatsächlichen Verbindung. Run/Epoche/Revision und Host-Rolle
  vor jeder Anwendung in Sync prüfen. Zuverlässiger Transport ersetzt weder
  Idempotenz noch fachliche Berechtigung.
- Nach Join/Travel/Reconnect vollständiges Zustandsbild senden; reine Events
  sind für diese Fälle unzureichend. Das gehört zur jeweiligen Sync-Familie.
- Nach Disconnect werden ausstehende Nachrichten dieses Tokens entfernt. Neue
  Verbindungen bekommen auch nach Transport-Neustart neue Token. Bereits vom
  Aufrufer entnommene Nachrichten muss er gegen die aktuellen Peers prüfen.
- Beide Queues sind auf je 128 Nachrichten begrenzt (je maximal etwa 8 MiB).
  Empfangsüberlauf oder nicht angenommener zuverlässiger Steam-Send beendet die
  Verbindung explizit, statt einen angeblich zugestellten Zustand zu verlieren.
- Der Netzwerkthread greift niemals auf SDK-/Spielobjekte zu. Keine Callback-
  Ausführung in Sync oder Game unter einer Transport-Sperre.

Direktverbindung ohne Mod-Tunnel (`open 127.0.0.1:7777`) hat keinen Kontrollkanal.
Für Sync-Tests deshalb den Mod-Tunnel verwenden, nicht den unveränderten
Legacy-Tunnel des Wegwerf-Spikes.
