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

`CurrentTransport()` liefert auch den von `Session` gestarteten Lobbytransport.
Die Registrierung ist schwach: Verlassen/Stop hält keine Sitzung wegen eines
Sync-Abonnenten am Leben. Die API ist somit nicht auf den Testschalter beschränkt.

## Diagnosefamilie (Mod-Kompatibilität 2)

Rahmenversion bleibt 1; Mod-Kompatibilitätsversion ist für die neue Familie 2.
Beide Seiten müssen dieselbe Mod-Version benutzen. Zentraler Queue-Drain und
Details der Messung: [Desync-Melder](DESYNC.md). Typen `0x200` (Client-Probe)
und `0x201` (Host-Ergebnis), jeweils exakt 84 Nutzlastbytes:

| Offset | Bytes | Inhalt |
|---:|---:|---|
| 0 | 8 | Client-Run-ID, im Ergebnis Echo |
| 8 | 8 | Client-Lebensdauer-Nonce, im Ergebnis Echo |
| 16 | 8 | Client-Probensequenz, im Ergebnis Echo |
| 24 | 8 | NetGUID des aktuellen MissionGameState |
| 32 | 1 | Known-Bitmaske |
| 33 | 1 | Stable-Bitmaske, Teilmenge von Known |
| 34 | 1 | Ignored-Bitmaske, nur vom Host |
| 35 | 1 | bestätigte Alerts, stabil und nicht ausgenommen, nur vom Host |
| 36 | 48 | Sechs uint64: Tag, Phase, Spieler, HP-Hash, Gegner, Inventar-Hash |

Nur die unteren sechs Maskenbits sind gültig. Unbekannte Feldwerte müssen null
sein. Tag höchstens 100000, Phase 0/1, Spieler höchstens 2, Gegner höchstens
20000. Alle vier Payload-Kennungen ungleich null; NetGUID höchstens 54 Bit.
Probe-Envelope: Run/Nonce/Sequence identisch zur Payload. Ergebnis-Envelope:
Host-Run, Host-Lebensdauer-Nonce, monoton steigende Antwortrevision. Payload-
Echo und Peer binden die Antwort zusätzlich an die aktuell offene Client-Probe.
Client-Proben enthalten weder Ausnahmen noch Alarme. Ausgenommene Felder sind
kein Gleichheitsnachweis. Noch keine Spielabnahme dieser Familie.

## Bisher geprüft (05.11.2026, Kontrollkanal vor Diagnoseerweiterung)

- Build und alle Tests bestanden, darunter 24 Net-Testfälle: binäre UE-Pakete in
  beide Richtungen, exakte Rahmengrenzen, falsche Build-/Mod-Version, fehlendes
  Hello/Timeout, getrennte Control-Nachrichten vor der ersten Spieladresse,
  Disconnect/Neustart ohne Token-Wiederverwendung und schwache Lobbyregistrierung.
- Zwei geschützte Spielinstanzen mit `-Mod -Tunnel Mod`: Handshake bestätigt,
  MetaBase, echter Routenweg nach Tag 1 und gleiche Inventare. Anschließend lief
  darüber die XP-Anzeigeanbindung aus `client-visuell`, ohne einen zweiten Kanal.
- Aufgabe `protokoll` damit abgeschlossen. Die Internet-/Zwei-Konten-Prüfung des
  Handshakes gehört zu `lobby-live` bzw. `integration-g2`.
- Noch keine Internet-/Zwei-Konten-Abnahme und keine zugesicherte Gameplay-
  Transaktionssemantik. Die Familien für Requests, Snapshots und Rewards müssen
  ihre jeweilige fachliche Validierung selbst liefern.
