# niederlage-metabase: Nach der Niederlage behalten beide ihr Inventar, und der Client hängt in der MetaBase

## Reproduktion

Live-Test am 26.11.2026, zwei PCs über Steam-Relay, Build `26786272`, Mod-Version 21. Der Run endete an Tag 5 mit dem Tod des Hosts,
kurz vorher schaltete der Host per Dev-Aktion 3 die Unverwundbarkeit um. Beide kehrten in die MetaBase zurück, der Host startete
gleich den nächsten Run. Noch nicht auf einem PC mit zwei Instanzen nachgestellt.

## Belege

- [Belegt] Tester: Nach dem Tod behielten Host und Client in der MetaBase ihr Inventar aus dem Run.
- [Belegt] Host-Log: `Rundenende: Host beendet die Runde`, `Wiedereintritt: gemerkte Inventare verworfen (Run-Ende)`,
  `Rundenwerte: 44 Werte an den Client gesendet`, danach `Reise umgeleitet: /Game/Maps/MetaBase`. Im neuen Run
  (`MGP_City_LittleIndia`, Seed 31692) meldet der Host `Wiedereintritt: Inventar des Clients gefunden (26 Items), Konto neu`. Die
  Liste beim späteren Trennen enthält Items aus dem alten Run, etwa `RobotArm/5_Mythic` und die beim Händler gekaufte `Battery/2_Rare`.
- [Belegt] Client-Log: `Run-Stand: Client lässt seinen eigenen Run-Stand unberührt (Rundenwechsel, Grund 1)`,
  `Run-Ende: Client zeigt den Ergebnisbildschirm (Niederlage)`, Reise in die MetaBase. Dann `Schrott: Stand des Hosts übernommen,
  113 statt 0`, obwohl der Host in der MetaBase 0 meldet, und `Welt: MetaBase, Client, 1 Spieler` um 19:43:05.329.
- [Belegt] Danach schreibt der Spiel-Thread des Clients nichts mehr, auch keinen `Zustand (Takt)`, der sonst alle 30 s kommt. Nur
  der Transport-Thread läuft weiter. Um 19:43:11 warten 34 Kontrollnachrichten aufs Abholen (vorher höchstens 4).
- [Belegt] Um 19:43:29 trennt der Client mit Grund 1003 und `Protokoll: Ungueltige oder unerwartete Kontrollnachricht vom
  Mitspieler`. In `Transport::ReceiveFrame` (`src/Net/Transport.cpp`) gibt eine volle Eingangsschlange (`MaxControlQueue` = 128) `false`
  zurück, und `Reject` meldet das mit diesem Text aus `Protocol::Explain`. Der Text ist also irreführend, die Schlange lief voll,
  weil der Spiel-Thread sie nicht abholte.
- [Belegt] Das Client-Log endet um 19:43:29 ohne `EXCEPTION`.

## Hypothesen

- [Vermutung] Der Spiel-Thread des Clients hängt in der MetaBase kurz nach dem Wegfall der Host-Figur („1 Spieler“). Prüfidee: auf
  einem PC mit zwei Instanzen eine Niederlage herbeiführen, beim Client `Zustand (Takt)` und die Kontrollschlange beobachten und im
  Hänger einen Dump ziehen.
- [Vermutung] Das Inventar bleibt, weil der Run-Stand nach einer Niederlage in den Mod-Ständen (`SaveGames_DDCoop`) nicht wie solo
  zurückgesetzt wird. Beim Host deutet das auf die Run-Persistenz, beim Client auf die Sperre seines Run-Stands
  (`client-runstand-abbruch`). Prüfidee: solo mit Mod sterben und in der MetaBase das Inventar prüfen; danach zu zweit die
  Spielstandzeilen nach `Run-Ende` vergleichen.
- [Vermutung] Beide Symptome können getrennte Ursachen haben. Dann bekommt der Rest eine eigene Kennung.

## Nächster Schritt

Auf einem PC mit zwei Instanzen nach einer Niederlage nachstellen, solo mit Mod gegenprüfen. Die Meldung bei voller Kontrollschlange
im selben PR ehrlich benennen.

## Abnahme

Nach einer Niederlage zu zweit kehren beide mit dem Inventar in die MetaBase zurück, das solo nach einem Tod bleibt. Der Client bleibt
dort bedienbar und verbunden. Der nächste Run startet bei beiden mit Startausrüstung.
