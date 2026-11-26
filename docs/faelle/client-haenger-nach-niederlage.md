# client-haenger-nach-niederlage: Client hängt nach der Niederlage in der MetaBase, beide behalten ihr Run-Inventar

Offen. Gefunden im Live-Test am 26.11.2026 (zwei PCs über Steam-Relay, Build `26786272`, Mod-Version 21, Client über Steam gestartet).

## Reproduktion

Zwei PCs, gemeinsamer Run bis zur Niederlage (hier an Tag 5, 4 s vorher schaltete der Host per Dev-Aktion 3 die Unverwundbarkeit
um). Nach dem Ergebnisbildschirm reisen beide in die MetaBase. Der Client reagiert danach nicht mehr und muss beendet werden. Laut
Tester fliegt er nach dem Run-Ende raus und landet nicht in der MetaBase. Host und Client behielten laut Tester ihr Inventar aus dem
Run. Der Host startete gleich den nächsten Run. Auf einem PC mit zwei Instanzen noch nicht nachgestellt.

## Belege

- [Belegt, Client-Log] Ergebnisbildschirm beim Client wie beim Host (`Run-Ende: Client zeigt den Ergebnisbildschirm (Niederlage)`),
  vorher `Run-Stand: Client lässt seinen eigenen Run-Stand unberührt (Rundenwechsel, Grund 1)`. Danach `Zustand (Weltwechsel)` erst
  nach `Untitled`, dann nach `MetaBase` mit `Spieler=2`, `Schrott: Stand des Hosts übernommen, 113 statt 0`, obwohl der Host in der
  MetaBase 0 meldet.
- [Belegt, Client-Log] 0,4 s später `Welt: MetaBase, Client, 1 Spieler` (19:43:05,3 Client-Uhr). Das ist die letzte Zeile des
  Spielthreads; die Taktzeile alle 60 s fehlt danach.
- [Belegt, Host-Log] `Rundenende: Host beendet die Runde`, `Wiedereintritt: gemerkte Inventare verworfen (Run-Ende)`,
  `Rundenwerte: 44 Werte an den Client gesendet`, `Reise umgeleitet: /Game/Maps/MetaBase`. Bei der Ankunft in der MetaBase
  (19:43:06,9 Host-Uhr, etwa 1,5 s vor der Client-Uhr) `Pause: Weltzeit angehalten, Faktor 1 Pauser true`.
- [Belegt, Host-Log] Um 19:43:14 reiste der Host mit 2 Spielern in den neuen Run (`MGP_City_LittleIndia`, Seed 31692). Der Client
  folgte in 15 s nicht, sonst folgt er innerhalb einer Sekunde. Im neuen Run meldet der Host `Wiedereintritt: Inventar des Clients
  gefunden (26 Items), Konto neu`; die Liste beim späteren Trennen enthält Items aus dem alten Run, etwa `RobotArm/5_Mythic` und die
  beim Händler gekaufte `Battery/2_Rare`.
- [Belegt, Client-Log] Der Netzthread läuft weiter: 6 s nach der letzten Zeile eine Transportzeile (Ping 47 ms, 0 verworfen, 34
  Kontrollnachrichten zum Abholen), 24 s danach (19:43:29) Trennung mit Grund 1003 und `Protokoll: Ungueltige oder unerwartete
  Kontrollnachricht vom Mitspieler`. `Transport::ReceiveFrame` (`src/Net/Transport.cpp`) liefert für einen Kontrollrahmen `false`,
  wenn `DecodeControl` scheitert oder die Eingangsschlange voll ist (`MaxControlQueue` = 128), und `Protocol::Explain` meldet beides
  mit diesem Text. Welcher Fall es war, zeigt das Log nicht; eine volle Schlange passt dazu, dass der Spielthread nicht mehr abholt.
- [Belegt, Windows] Der Prozess reagierte nicht und belegte rund drei Viertel eines Kerns. Kein Absturzbericht, kein Ereignis 1000
  oder 1002, kein `EXCEPTION` im Log; der Prozess endete gut eine Minute nach dem Hänger.

## Hypothesen

- [Vermutung] Der Spielthread läuft in einer Schleife (CPU-Last, keine Sperre). Kandidaten: Abbau des Run-Stands beim Weltwechsel
  in die MetaBase, oder die Spielerliste, die von 2 auf 1 fällt, während der Host noch Nachrichten der Mission schickt.
- [Vermutung] Die abgelehnte Kontrollnachricht ist Folge, nicht Ursache: Der Spielthread stand da schon, die Schlange lief voll.
- [Vermutung] Das Inventar bleibt, weil der Run-Stand nach einer Niederlage in den Mod-Ständen (`SaveGames_DDCoop`) nicht wie solo
  zurückgesetzt wird. Beim Host deutet das auf die Run-Persistenz, beim Client auf die Sperre seines Run-Stands
  (`client-runstand-abbruch`). Prüfidee: solo mit Mod sterben und in der MetaBase das Inventar prüfen; danach zu zweit die
  Spielstandzeilen nach `Run-Ende` vergleichen.
- [Vermutung] Hänger und Inventar können getrennte Ursachen haben. Dann bekommt der Rest eine eigene Kennung.

## Nächster Schritt

Auf einem PC mit zwei Instanzen eine Niederlage herbeiführen und prüfen, ob der Client beim Ankommen in der MetaBase hängt. Hängt er,
vor dem Beenden einen Speicherabzug der Instanz nehmen (nur lokal) und den Stapel des Spielthreads ansehen. Solo mit Mod gegenprüfen,
ob das Inventar nach dem Tod bleibt. Die Meldung bei voller Kontrollschlange im selben PR ehrlich benennen.

## Abnahme

Nach Niederlage und Sieg kommt der Client in der MetaBase an und bleibt bedienbar und verbunden; beide behalten nur das Inventar, das
solo nach dem Ende bleibt. Ein neuer Run in derselben Sitzung startet bei beiden mit Startausrüstung (Bestätigungstest von
`run-ende`). Zwei PCs.
