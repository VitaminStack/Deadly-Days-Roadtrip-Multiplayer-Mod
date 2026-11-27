# client-haenger-nach-niederlage: Client hängt nach der Niederlage in der MetaBase, beide behalten ihr Run-Inventar

Offen, Ursache des Hängers unbekannt. Gefunden im Live-Test am 26.11.2026 (zwei PCs über Steam-Relay, Build `26786272`, Mod-Version 21, Client über Steam gestartet).
Seit diesem Stand schreibt die Mod bei einem stehenden Game Thread dessen Stapel ins Log, und die Trennung bei voller Kontrollschlange hat einen eigenen Grund.

## Reproduktion

Zwei PCs, gemeinsamer Run bis zur Niederlage (hier an Tag 5, 4 s vorher schaltete der Host per Dev-Aktion 3 die Unverwundbarkeit
um). Nach dem Ergebnisbildschirm reisen beide in die MetaBase. Der Client reagiert danach nicht mehr und muss beendet werden. Laut
Tester fliegt er nach dem Run-Ende raus und landet nicht in der MetaBase. Host und Client behielten laut Tester ihr Inventar aus dem
Run. Der Host startete gleich den nächsten Run.

Auf einem PC mit zwei Instanzen nicht nachgestellt (27.11.2026, fünf Läufe, siehe Belege). Niederlage ohne echte Eingaben:
beim Host `!cheats`, `DD_DamagePlayerAtIndex 100000 1` und `… 0`; nach rund 30 s „Zurück zur Basis“ mit
`!shop callat <n> WP_Round_Stats_C BndEvt__WP_Round_Stats_BackToBaseButton_K2Node_ComponentBoundEvent_1_OnClickResponse__DelegateSignature`.
`<n>` ist die Instanz, deren Outer-Kette an `GI_DDMissionGameInstance_C` hängt; die Vorlage im `WidgetTree` des Pakets zählt mit,
in einer frischen Sitzung war es 1; vorher Instanzen und Outer-Ketten aus dem Speicher lesen. Einmal stürzte der Host beim Sieg nach mehreren Runs
in derselben Sitzung mit `callat 1` ab (Lesen `0x110`, wie beim Aufruf an der Vorlage); [Vermutung] falsche Instanz, die Liste wurde
davor nicht gelesen. `!round 3` reicht nicht: Der Client zeigt den Ergebnisbildschirm, der Host bleibt in der Mission.

## Belege

- [Belegt, Client-Log] Ergebnisbildschirm beim Client wie beim Host (`Run-Ende: Client zeigt den Ergebnisbildschirm (Niederlage)`),
  vorher `Run-Stand: Client lässt seinen eigenen Run-Stand unberührt (Rundenwechsel, Grund 1)`. Danach `Zustand (Weltwechsel)` erst
  nach `Untitled`, dann nach `MetaBase` mit `Spieler=2`, `Schrott: Stand des Hosts übernommen, 113 statt 0`, obwohl der Host in der
  MetaBase 0 meldet.
- [Belegt, Client-Log] 0,4 s später `Welt: MetaBase, Client, 1 Spieler` (19:43:05,3 Client-Uhr). Das ist die letzte Zeile des
  Spielthreads; die Taktzeile alle 60 s fehlt danach. Die Spielerzahl fiel also in der MetaBase von 2 auf 1.
- [Belegt, Host-Log] `Rundenende: Host beendet die Runde`, `Wiedereintritt: gemerkte Inventare verworfen (Run-Ende)`,
  `Rundenwerte: 44 Werte an den Client gesendet`, `Reise umgeleitet: /Game/Maps/MetaBase`. Bei der Ankunft in der MetaBase
  (19:43:06,9 Host-Uhr, etwa 1,5 s vor der Client-Uhr) `Pause: Weltzeit angehalten, Faktor 1 Pauser true`.
- [Belegt, Host-Log] Um 19:43:14 reiste der Host mit 2 Spielern in den neuen Run (`MGP_City_LittleIndia`, Seed 31692). Der Client
  folgte in 15 s nicht, sonst folgt er innerhalb einer Sekunde. Im neuen Run meldet der Host `Wiedereintritt: Inventar des Clients
  gefunden (26 Items), Konto neu`; die Liste beim späteren Trennen enthält Items aus dem alten Run, etwa `RobotArm/5_Mythic` und die
  beim Händler gekaufte `Battery/2_Rare`.
- [Belegt, Client-Log] Der Netzthread läuft weiter: 6 s nach der letzten Zeile die Transportzeile mit höchstens 34 Kontrollnachrichten
  zum Abholen (im Lauf davor höchstens 29), 24 s nach der letzten Zeile (19:43:29) Trennung mit Grund 1003 und
  `Protokoll: Ungueltige oder unerwartete Kontrollnachricht vom Mitspieler`. Diesen Text gab es damals für eine unlesbare Nachricht
  und für eine volle Eingangsschlange (`MaxControlQueue` = 128).
- [Belegt, Windows] Der Prozess reagierte nicht und belegte rund drei Viertel eines Kerns. Kein Absturzbericht, kein Ereignis 1000
  oder 1002, kein `EXCEPTION` im Log; der Prozess endete gut eine Minute nach dem Hänger.
- [Belegt, ein PC, 27.11.2026] Kein Hänger in vier Läufen: Niederlage an Tag 1 ohne Items über 127.0.0.1; dieselbe mit
  `-NetProfile Internet`; Tag 5, Bedrohungsstufe 6, Host-Level 8 und drei Minuten Kampf mit Internet-Profil; Sieg an Tag 8
  (`DD_SetCurrentDay 8`, `!finish`); Niederlage mit 113 Schrott beim Host (Testinstanz-Speicher gesetzt). Jedes Mal kam der Client mit 2 Spielern in der MetaBase an (Spielerzahl 0, 1, 2, nie 2 zu 1),
  der Takt lief weiter, beide blieben verbunden. Nach der Niederlage startete in derselben Sitzung ein neuer Run, beide nur mit der
  Pistole (`inventory.py`: vollständig, gleich), der Host verwarf die gemerkten Inventare (`Host außerhalb der Mission`). Vorher
  hatten beide auch nur die Pistole; dass Run-Items verworfen werden, ist damit nicht gezeigt.
- [Belegt, ein PC] Schrott in der MetaBase: Der Client behält den letzten Stand des Hosts aus der Mission und schreibt ihn, sobald er ein
  Konto sieht. Bei mir sah er es erst nach dem Stand 0 des Hosts (Weltwechsel mit `Schrott=-`), live sofort (`Schrott=0`) und schrieb
  113. Den Pfad gezielt ausgelöst: Host-Schrott in der MetaBase auf 113 gesetzt, Client `Schrott: Stand des Hosts übernommen, 113
  statt 0`, kein Hänger. Der Schrott-Pfad allein ist es also nicht; die veraltete Anzeige bis zum nächsten Stand bleibt ein Schönheitsfehler.
- [Belegt, ein PC] Gegenprobe des Wächters: Game Thread des Clients in der MetaBase 15 s von außen angehalten. Nach 8 s
  `Spielthread: seit 8.0 s kein Takt der Mod, Probe 1/3, …` mit vollständigem Stapel bis `KERNEL32.DLL`, nach 12 s Probe 2, danach
  `Takt der Mod läuft wieder`. Der Client blieb verbunden. Die Probe nennt den offenen Abschnitt der Mod (`im Abschnitt <Tick>`, im Test belegt)
  oder `außerhalb der Mod-Ticks` (im Spiel belegt), weil RVAs der DLL ohne passende Symbole nichts sagen. In allen Läufen keine Fehlmeldung des Wächters
  beim Start, Laden und Reisen.

## Hypothesen

- [Vermutung] Der Spielthread läuft in einer Schleife (CPU-Last, keine Sperre). Kandidaten: Abbau des Run-Stands beim Weltwechsel
  in die MetaBase, oder etwas, das auf die von 2 auf 1 fallende Spielerliste reagiert; das trat auf einem PC nie auf.
  Prüfidee: Die Zeilen `Spielthread: … Probe` des nächsten Hängers zeigen den Ort.
- [Widerlegt] Endlose Hash-Ketten in `ControllerSet::Remove` und `ActivePlayersMap::Remove`: Beide laufen nur nach `Valid()`, das jede
  Kette mit Schrittgrenze prüft, und nur beim Host. Auch sonst laufen alle Zeiger-Ketten der Mod (Outer, SuperStruct, Felder) mit Grenze.
- [Vermutung, passt zu den Zahlen] Die Trennung ist Folge, nicht Ursache: Mit rund 5 Nachrichten je Sekunde füllen sich die 128
  Plätze in gut 20 s, gemessen waren es 24 s. Seit diesem Stand heißt der Grund in diesem Fall `Kontrollnachrichten stauen sich`.
- [Vermutung] Das Inventar des Clients im neuen Run ist Folge des Hängers: Der Client reiste nie mit, der Host las das alte Inventar
  seines Controllers. Auf einem PC startete der neue Run nach der Niederlage bei beiden mit Startausrüstung, aber ohne Run-Items vorher.
- [Vermutung] Dass auch der Host sein Run-Inventar behielt, hängt an der Run-Persistenz oder an der Sperre des Client-Run-Stands
  (`client-runstand-abbruch`). Prüfidee: solo mit Mod sterben und das Inventar in der MetaBase ansehen, zu zweit dasselbe mit Items.

## Nächster Schritt

Live-Test auf zwei PCs mit diesem Stand, Niederlage mit Run-Items und Schrott an einem späteren Tag. Hängt der Client wieder, stehen die Proben
im Client-Log; die RVAs der Exe mit `Disassemble.ps1` und die UFunctions ansehen, dann gezielt beheben. Hängt er nicht, das Inventar
nach der Niederlage in der MetaBase und beim nächsten Run vergleichen. Items über Airdrop-Cheats auf einem PC scheiterten: Die Auswahl
bleibt nach `!shop choose` und `!shop confirm 0` offen und die Welt pausiert (`airdrop-auswahl-absturz`).

## Abnahme

Nach Niederlage und Sieg kommt der Client in der MetaBase an und bleibt bedienbar und verbunden; beide behalten nur das Inventar, das
solo nach dem Ende bleibt. Ein neuer Run in derselben Sitzung startet bei beiden mit Startausrüstung (Bestätigungstest von
`run-ende`). Zwei PCs.
