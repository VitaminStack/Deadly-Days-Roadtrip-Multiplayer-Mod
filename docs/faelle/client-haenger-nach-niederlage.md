# client-haenger-nach-niederlage: Client hängt nach der Niederlage beim Ankommen in der MetaBase

Offen. Gefunden im Live-Test am 26.11.2026 (zwei PCs, Build `26786272`, Mod-Version 21, Client über Steam gestartet).

## Reproduktion

Zwei PCs, gemeinsamer Run bis zur Niederlage (hier an Tag 5). Nach dem Ergebnisbildschirm reisen beide in die MetaBase. Der Client
reagiert danach nicht mehr und muss beendet werden. Laut Tester fliegt er nach dem Run-Ende raus und landet nicht in der MetaBase.
Auf einem PC mit zwei Instanzen noch nicht nachgestellt.

## Belege

- [Belegt, Client-Log] Ergebnisbildschirm beim Client wie beim Host (`Run-Ende: Client zeigt den Ergebnisbildschirm (Niederlage)`),
  danach `Zustand (Weltwechsel)` erst nach `Untitled`, dann nach `MetaBase` mit `Spieler=2`, `Schrott: Stand des Hosts übernommen`.
- [Belegt, Client-Log] 0,4 s später `Welt: MetaBase, Client, 1 Spieler`. Das ist die letzte Zeile des Spielthreads; die Taktzeile
  alle 60 s fehlt danach.
- [Belegt, Client-Log] Der Netzthread läuft weiter: 6 s später eine Transportzeile (Ping 47 ms, 0 verworfen, 34 Kontrollnachrichten
  zum Abholen), 24 s später `Protokoll: Ungueltige oder unerwartete Kontrollnachricht vom Mitspieler`.
- [Belegt, Windows] Der Prozess reagierte nicht und belegte rund drei Viertel eines Kerns. Kein Absturzbericht, kein Ereignis 1000
  oder 1002; der Prozess endete gut eine Minute nach dem Hänger.
- [Offen] Host-Log desselben Zeitraums liegt noch nicht vor.

## Hypothesen

- [Vermutung] Der Spielthread läuft in einer Schleife (CPU-Last, keine Sperre). Kandidaten: Abbau des Run-Stands beim Weltwechsel
  in die MetaBase, oder die Spielerliste, die von 2 auf 1 fällt, während der Host noch Nachrichten der Mission schickt.
- [Vermutung] Die abgelehnte Kontrollnachricht ist eine Nachricht der Mission, die in der MetaBase unerwartet ist. Sie ist eher Folge
  als Ursache, weil der Spielthread da schon stand.

## Nächster Schritt

Host-Log auswerten. Auf einem PC mit zwei Instanzen eine Niederlage herbeiführen und prüfen, ob der Client beim Ankommen in der
MetaBase hängt. Hängt er, vor dem Beenden einen Speicherabzug der Instanz nehmen (nur lokal) und den Stapel des Spielthreads ansehen.

## Abnahme

Nach Niederlage und Sieg kommt der Client in der MetaBase an und bleibt bedienbar; ein neuer Run in derselben Sitzung startet
(Bestätigungstest von `run-ende`). Zwei PCs.
