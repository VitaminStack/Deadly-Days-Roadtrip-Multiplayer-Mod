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

**Ablehnungsgrund.** Passt die erste Nachricht nicht, nennt `Protocol::Explain` in Log und Overlay den Grund (`versions-meldung`):
Mitspieler ohne Mod-Protokoll (Mod vor `protokoll`), andere Protokollversion, anderer Spiel-Build, andere Mod-Version jeweils mit
beiden Werten, beide Seiten Host oder beide Client. Erst wenn nichts davon zutrifft, heißt es „Ungueltige … Kontrollnachricht“, dann sind
die Daten wirklich kaputt. Die Gegenseite sieht weiter Grund 1003 („Der Host hat die Verbindung abgelehnt“).

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

## Airdrop-Auswahl (Mod-Kompatibilität 3)

Aufgabe `airdrop-auswahl`, Ablauf in [Airdrop-Auswahl](../src/Game/README.md#airdrop-auswahl). Mod-Version 3, weil der Host
ab dann beim Airdrop auf die Wahl des Clients wartet; ein Client mit Version 2 würde ihn blockieren und wird schon beim Hello
abgelehnt. Code in `src/Sync/Inventory`, Envelope-Run je Prozess, Epoche 1, Revision monoton.

`0x300` Sitzungsbild, Host an Client, bei Änderung und spätestens jede Sekunde:

| Offset | Bytes | Inhalt |
|---:|---:|---|
| 0 | 8 | Sitzung, Zähler der Mod beim Host, 0 = keine offene Auswahl |
| 8 | 8 | Revision, Prüfsumme der aktuellen Choices, ungleich 0 bei offener Sitzung |
| 16 | 4 | Airdrop-Stufe |
| 20 | 1 | Bits: 0 Client hat gewählt, 1 Host hat gewählt, 2 Host wartet auf den Client |
| 21 | 1 | Anzahl Choices, höchstens 4, ohne Sitzung 0 |
| 22 | … | je Choice: 1 Byte beschädigt (0 oder 1), dann lesbarer Name, Objektname der `UInventoryItemData` und der `URarity`, je 1 Byte Länge und höchstens 120 Byte UTF-8 |

Der Client sucht `ItemData` und `Rarity` über die Objektnamen und legt damit eigene Anzeige-Instanzen an.

`0x301` und `0x302` sind seit Mod-Version 4 ersetzt, siehe [Inventar lokal](#inventar-lokal-mod-kompatibilität-4). Bis dahin:

`0x301` Wahl, Client an Host, genau 17 Bytes: Sitzung (8), Revision (8), Index (1, kleiner 4). Der Host nimmt sie nur an,
wenn Sitzung und Revision zur offenen Sitzung passen und der Client noch nicht gewählt hat; sonst verwirft er sie und das nächste
Bild zeigt den Stand. Keine Objektzeiger auf dem Draht, die Revision ist nur eine Prüfsumme.

`0x302` Ablage im Rucksack, Client an Host, nach der Wahl: Sitzung (8), Drehung `Turns` (4), Pivot-Feld X und Y (je 4),
Anzahl Felder (1, 1 bis 32), je Feld X und Y (je 4). Der Host legt die zuletzt für diese Sitzung angelegte Wahl auf diese Felder,
sonst bleibt sie im Lager. Stand 05.11.2026: Der Client merkt sich die Ablage, im Test kam die Nachricht beim Host nicht an
(`airdrop-rucksack`).

## Inventar lokal (Mod-Kompatibilität 4)

Aufgabe `inventar-lokal`, Ablauf in [Inventar lokal bearbeiten](../src/Game/README.md#inventar-lokal-bearbeiten). Mod-Version 4,
weil Wahl und Ablage nicht mehr als `0x301`/`0x302` kommen; ein Client mit Version 3 könnte nicht mehr wählen. `0x300` bleibt.
Ganzzahlen little-endian, Felder als X und Y mit je 4 Byte (vorzeichenbehaftet).

`0x303` Edit, Client an Host, beim Bestätigen und bis zur Antwort jede Sekunde erneut:

| Offset | Bytes | Inhalt |
|---:|---:|---|
| 0 | 8 | Kennung des Edits, ungleich 0 |
| 8 | 8 | Sitzung der Auswahl, mit Wahl ungleich 0 |
| 16 | 8 | Revision der Auswahl |
| 24 | 1 | Index der Wahl, kleiner 8, `0xFF` = keine |
| 25 | 1 | Anzahl neuer Felder, höchstens 64 |
| 26 | 8 je | neue Felder |
| … | 1 | Anzahl Items, höchstens 96 |
| … | je Item | Index im Ausgangsbild (2, `0xFFFF` = die Wahl), Prüfsumme des `ItemData`-Objektnamens (4, FNV-1a), Bits (1: 0 vorhanden, 1 im Rucksack, 2 Papierkorb, 3 Rucksackeffekt auslösen), Kontext von `RemoveFromInventory` (1), `Turns` (4), Pivot (8), Anzahl Felder (1, höchstens 32), je Feld 8 |

Nur geänderte Items, jeweils der Endzustand. Der Host lehnt ab, wenn die Form nicht stimmt (doppelte Quelle oder Felder, Felder
ohne Rucksack, Belohnung ohne Wahl), die Wahl nicht zur offenen Sitzung passt, ein Item fehlt oder ein Zielfeld weder frei noch neu ist.

`0x304` Antwort, Host an Client, genau 9 Bytes: Kennung (8), übernommen (1, 0 oder 1). Kommt eine Kennung erneut, schickt der Host
nur die Antwort noch einmal.

## Alle bestätigen (Mod-Kompatibilität 5)

Aufgabe `bestaetigen-alle`. Mod-Version 5, weil der Host ab dann mit dem Schließen der Auswahl wartet, bis der Client bereit ist;
ein Client mit Version 4 meldet das nie und würde die Auswahl blockieren. Bestätigen markiert einen Spieler nur als bereit (Knopf
gelb, weitere Klicks ändern nichts). Erst wenn der Host bereit ist und das Bereit des Clients hat, löst der Client sein
natives Bestätigen aus und schickt den Edit (`0x303`), danach schließt der Host sein Panel.

`0x300` bekommt Bit 3 im Flag-Byte (Offset 20): der Host hat das Bereit des Clients für genau diese Revision. Bit 2 „Host wartet“
heißt jetzt zugleich „Host ist bereit“.

`0x305` Bereit, Client an Host, genau 17 Bytes: Sitzung (8, ungleich 0), Revision (8), bereit (1, 0 oder 1). Der Client schickt es bei
jeder Änderung und alle 500 ms erneut, bis das Sitzungsbild den Stand zeigt. Der Host nimmt es nur für die offene Sitzung an, solange
der Client noch nicht gewählt hat; eine neue Revision (Reroll) braucht ein neues Bereit.
## Lager am Rundenende (Mod-Kompatibilität 6)

Aufgabe `bestaetigen-rundenende`. Mod-Version 6, weil der Host im Lager am Rundenende ab dann auf das Bereit des Clients wartet; ein
Client mit Version 5 meldet es nie. Bestätigen im Lager markiert wie in [Alle bestätigen](#alle-bestätigen-mod-kompatibilität-5) nur
bereit. Die Id ist die des Rundenendes aus `0x401`.

`0x306` Client an Host, genau 9 Bytes: Id des Rundenendes (8), das der Client selbst geöffnet hat, und bereit (1, 0 oder 1).
Alle 500 ms, solange der Client in diesem Rundenende ist. Ohne diese Meldung wartet der Host nicht. Id 0 nur mit bereit 0: Der Client
hat das Rundenende verlassen, er schickt das danach 3 s lang alle 500 ms, und der Host hört sofort auf zu warten. Bleibt die Meldung
nur aus (Absturz), wartet der Host weiter, bis die Verbindung abläuft (`lager-absturz-client`).

`0x307` Host an Client, genau 9 Bytes: Id des Rundenendes (8, 0 = keins) und Flags (1): Bit 0 Host bereit, Bit 1 Host hat das Bereit des
Clients. Ohne Rundenende sind die Flags 0, andere Bits sind ungültig. Bei Änderung sofort, sonst jede Sekunde. Der Client wartet nur
mit einem höchstens 5 s alten Stand für dasselbe Rundenende und bestätigt, sobald beide Bits gesetzt sind oder der Stand fehlt.

## Aufwerten und Seltenheiten (Mod-Kompatibilität 7)

Aufgabe `werkbank-client`, Ablauf in [Werkbank und Aufwerten beim Client](../src/Game/README.md#werkbank-und-aufwerten-beim-client).
Mod-Version 7, weil `0x303` ein neues Feld am Ende hat und ein Host mit Version 6 jeden Edit ablehnen würde.

`0x303` Edit bekommt hinter den Items:

| Bytes | Inhalt |
|---:|---|
| 1 | Anzahl Aufwertungen, höchstens 8 |
| je Aufwertung | Anzahl Eingaben (1, 1 bis 4), je Eingabe die Quelle (2), Name der Ausgabedaten (1 Länge + bis 120 Byte UTF-8), Name der Seltenheit (ebenso, leer = keine), die Ausgabe als Item wie oben |

Quellen der Eingaben: Index im Ausgangsbild, `0xFFFF` die Wahl, `0xFF00 + k` die Ausgabe der Aufwertung `k`. Die Ausgabe der Aufwertung
`k` hat die Quelle `0xFF00 + k`, ihre Prüfsumme ist die des Datennamens. Jede Eingabe steht als nicht mehr vorhanden im Edit, eine
frühere Ausgabe als Eingabe ebenso, und wird nur einmal verbraucht. Der Host lehnt zusätzlich ab, wenn eine Aufwertung weder zum Ablegen
auf ein gleiches Item noch zu einem Rezept der Werkbank passt.

`0x308` Seltenheiten, Host an Client: Anzahl (1, höchstens 96), je Item Platz in `MyItems` des Clientinventars (2), Prüfsumme des
`ItemData`-Objektnamens (4), Name der Seltenheit (1 Länge + bis 120 Byte, leer = keine). Bei Änderung sofort, sonst alle 5 s. Der Client
setzt die Seltenheit nur, wenn die Daten an diesem Platz übereinstimmen.

## Verkaufen, Reparieren und Schrott (Mod-Kompatibilität 9)

Aufgabe `lager-handel-client`, Ablauf in [Verkaufen und Reparieren beim Client](../src/Game/README.md#verkaufen-und-reparieren-beim-client).
Mod-Version 9, weil `0x303` ein neues Flag trägt und ein Host mit Version 8 die Reparatur nicht kennt und `0x309` fehlt.

`0x303` Edit: Im Flag-Byte jedes Items steht zusätzlich Bit 4 = kaputt (`Broken`), Werte über 31 sind ungültig. Ein Item, das im
Ausgangsbild kaputt war und im Edit heil vorhanden ist, gilt als repariert. Verkauft heißt nicht vorhanden mit `RemoveContext` 2
(`SoldForScrap`); `RemoveContext` über 3 und ein vorhandenes Item mit `RemoveContext` ungleich 0 sind ungültig. Verkaufen dürfen nur
Ausgangs-Items, nicht die Wahl, keine Eingabe und keine Ausgabe einer Aufwertung. Heil zu kaputt lehnt der Host ab, ebenso einen anderen
Zustand der Wahl als im Angebot. Der Host rechnet die Preise selbst (`GetDynamicSellPrice`, `GetDynamicRepairPrice`), schreibt erst alle
Verkäufe gut und zieht dann jede Reparatur ab; reicht sein Schrott für eine Reparatur nicht, lehnt er den ganzen Edit ab. Gebucht wird
nach der Übernahme genau einmal und nur für tatsächlich entfernte bzw. reparierte Items.

`0x309` Schrott, Host an Client: genau 4 Byte, Kontostand von `ScrapWalletComponent` beim Host (int32, 0 bis 1 000 000 000). Der Host
prüft jede halbe Sekunde, sendet bei Änderung sofort, sonst alle 5 s, nur in einer Mission. Der Client gleicht sein eigenes Konto mit
dem nativen `AddAmount` (Unterschied) an, nicht während einer lokalen Bearbeitung und nicht, solange ein Edit unbeantwortet ist.

## Rundenende (`rundenende-client`)

`0x401` Host an Client, neben der Karte `0x400` aus `missionsstart` (`src/Sync/World`). Genau 17 Bytes: Karten-Id der Mission
(`HostMap::Id`, 8), Id des Rundenendes (8), beide zufällig und ungleich 0, und der Ausgang des Runs (`RunOutcome`, 1: 0 Tagesende,
1 Niederlage, 2 Sieg, seit `run-ende`). Alles 0 heißt kein Rundenende, andere Werte des Ausgangs und ein Ausgang ohne Ids werden
abgelehnt. Der Host schickt die Nachricht bei jeder Änderung und sonst jede Sekunde, bis er die Welt verlässt. Der Client beendet seine
Runde nur, wenn die Karten-Id zu der Karte passt, die er in dieser Welt vom Host übernommen hat, und jede Id nur einmal. Mod-Version 10,
weil eine Gegenseite mit der alten Länge von 16 Bytes die Nachricht verwerfen würde.

## Dodge-Richtung (Mod-Kompatibilität 8)

Aufgabe `dodge-client-live`, Befund in [Dodge im Zwei-PC-Test](GAME_ANALYSIS.md#dodge-im-zwei-pc-test-dodge-client-live), Code in
`src/Game/MovementPatches.cpp` und `src/Sync/MovementState.h`. Mod-Version 8, weil ein Host ohne diese Nachricht wieder die
geschätzte Richtung nähme.

`0x500` Client an Host, genau 10 Bytes, je eigener Aktivierung von `GA_Dash` beim Client: Vorhersageschlüssel der Aktivierung
(`FPredictionKey.Current`, int16, 2) und die Eingabe, die `GA_Dash` liest, normiert oder (0, 0) (X und Y als float, je 4). Längen über
1, NaN und Unendlich sind ungültig. Der Host liest denselben Schlüssel an seiner Instanz der Fähigkeit und trägt die Eingabe direkt vor
`K2_ActivateAbility` in den Controller des Clients ein. Kommt das Spielpaket mit der Aktivierung vor der Meldung an, hält der Host die
Aktivierung höchstens 250 ms an und startet sie dann mit der Meldung, sonst mit der geschätzten Richtung (Warnung im Log). Meldungen
ohne Aktivierung verfallen nach 2 s. Der Host holt `0x500` in jedem Takt mit `TakeControlMessages(Type)` ab, die übrigen Familien
weiter alle 250 ms. Kosten, Schub und Ende rechnet der Host nativ, die Ausdauer repliziert wie gewohnt.

## Pausemenü (Mod-Kompatibilität 9)

Aufgabe `pausen-sync`, Code in `src/Sync/Players` und `src/Game/PauseAdapter.cpp`, Befund im
[Pausen-Spike](../research/spikes/pausen/README.md#umsetzung-pausen-sync-08112026). Mod-Version 9, weil ein Host mit Version 8 das
Pausemenü des Clients nicht kennt und weiterliefe. Lokal bleibt das native Pausemenü samt Inventaransicht beim Client. Geteilt wird
nur, ob es offen ist. Der Host entscheidet über die Weltzeit: Er setzt eine eigene Zeitanfrage mit den Werten seines Pausemenüs,
die native Replikation von `WorldSettings` trägt die Pause zum Client.

`0x700` Client an Host, genau 9 Bytes: Sitzung (8, ungleich 0, neu je Öffnen) und offen (1, 0 oder 1). Beim Öffnen und Schließen sofort,
offen alle 500 ms, nach dem Schließen 3 s lang alle 500 ms. Der Host hält die Pause einer Sitzung, bis die Freigabe kommt, 3 s keine
Meldung kam, seine Welt wechselt oder die Verbindung weg ist. Eine so beendete Sitzung pausiert nicht noch einmal, erst ein neues Öffnen.
Pausen des Hosts brauchen keine Nachricht, sie replizieren nativ. Keine Antwort vom Host.

## Warnanzeigen der Gegner (Mod-Kompatibilität 11)

Aufgabe `opt-gegner-indikatoren`, Code in `src/Game/EnemyTelegraphPatches.cpp` und `src/Sync/Enemies/TelegraphState.h`, Befund in
[Warnanzeigen der Gegner](GAME_ANALYSIS.md#warnanzeigen-der-gegner-beim-client-opt-gegner-indikatoren). Mod-Version 11, weil ein
Client mit Version 10 die Anzeigen nicht kennt und ein Host mit Version 10 sie nicht schickt. Die Angriffsfähigkeiten laufen nur beim
Host, ihre Niagara-Anzeigen replizieren nicht. Lokal bleibt die ganze Darstellung beim Client: dasselbe Niagara-System, Farbwechsel,
Länge und Ende des Strichs. Geteilt wird nur der Auslöser je Anzeige. Der Host entscheidet über Angriff, Ziel, Zeitpunkt und Schaden.

`0x900` Host an Client, je Anzeige: Art (1: 1 Kreis, 2 Strich an, 3 Strich-Stand, 4 Strich aus), NetGUID des Gegners (8, nur Strich,
sonst 0), Ort (3 × double; Kreis: Mitte, Strich-Stand: Ziel), Delay, Lifetime, Size, Percentage (je float), dann vier Texte mit je einem
Längenbyte (höchstens 200 druckbare ASCII-Zeichen): Pfad des Niagara-Systems und die Namen der drei Parameter aus der Fähigkeit des
Hosts. Nur der Kreis trägt Texte, nur der Strich eine NetGUID. Den Strich-Stand schickt der Host höchstens alle 50 ms je Gegner
und beim Anteil 1. Der Client holt `0x900` in jedem Takt ab und nimmt sie auch aus dem gebündelten Abruf alle 250 ms. Ein fehlendes System oder ein unbekannter Gegner wird übersprungen,
Verlust kostet nur eine Anzeige. Ohne Nachricht blendet der Client einen Strich nach 3 s, einen Kreis nach Delay + Lifetime + 1 s aus,
der Weltwechsel vergisst alles. Keine Antwort, keine Wiederholung, kein Nachholen bei Join oder Travel.

## Schadenszahlen (Mod-Kompatibilität 12)

Aufgabe `opt-schadenszahlen`, Befund in [Schadenszahlen beim Client](GAME_ANALYSIS.md#schadenszahlen-beim-client-opt-schadenszahlen),
Code in `src/Game/DamageNumberPatches.cpp` und `src/Sync/Enemies/DamageNumberState.h`. Mod-Version 12 (11 und `0x900` belegt `opt-gegner-indikatoren`), weil ein Client mit älterer Version
die Nachricht nicht kennt. Lokal bleiben Einstellung, Ort, Timer, Pool und Anzeige der Zahl. Geteilt wird nur, welcher Actor wie viel
Schaden bekam, denn die HP normaler Gegner replizieren nicht. Der Host entscheidet, sein nativer Schadenspfad ruft den Handler auf.

`0xA00` Host an Client, je Takt mit mindestens einem Treffer: Anzahl (1, 1 bis 64), je Treffer NetGUID des getroffenen Actors (8,
ungleich 0, höchstens 54 Bit), Schaden (float, 4, 0 bis 1e9), `DamageFlags` (1) und Ort des Ziels beim Host (X, Y, Z als float, je 4,
Betrag höchstens 1e7), also genau 1 + 25 × Anzahl Bytes. NaN, Unendlich und andere Längen werden abgelehnt. Der Client holt `0xA00` in
jedem Takt ab und ruft für jedes noch lebende, auflösbare Ziel den nativen Handler seines eigenen Controllers. Ein verlorenes Paket
heißt nur fehlende Zahlen, keine Wiederholung, keine Antwort.

Seit `opt-schadenszahlen-verlust` (13.11.2026, Format und Mod-Version unverändert) wertet der Client `0xA00` auch im Sammelabruf alle
250 ms aus: Eine Nachricht, die nach dem eigenen Abruf im selben Takt ankam, nahm sonst der Sammelabruf mit, und dort verfiel sie
ungezählt. Das gilt für jeden Typ, der einzeln abgeholt wird (`0x900` macht es genauso). Die Revision zählt der Host je angenommener
Nachricht um 1 hoch; der Client zählt daran fehlende Nachrichten, er fordert nichts nach. Zähler im Mod-Log: beim Host „übergeben“,
„gesendet“ (nur von `SendControl` angenommene), „Senden abgelehnt“, „ohne Mitspieler“, „Paket ungültig“; beim Client „im
Sammelabruf“, „Nachricht unlesbar“, „Nachrichten fehlen“, „falsche Rolle“. Die Transport-Zeile nennt den höchsten Füllstand beider
Warteschlangen des Kontrollkanals (Grenze 128) und wegen voller Warteschlange abgelehnte Nachrichten. Befund in
[GAME_ANALYSIS.md](GAME_ANALYSIS.md#verlorene-schadenszahlen-opt-schadenszahlen-verlust).

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
