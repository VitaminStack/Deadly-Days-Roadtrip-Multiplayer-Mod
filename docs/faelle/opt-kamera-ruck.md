# opt-kamera-ruck: Gemeinsame Kamera zieht kurz weg oder ruckt, sobald sich eine Figur bewegt

## Reproduktion

Meldung des Menschen vom 28.11.2026, Build `26786272`, Seite (Host oder Client) nicht genannt:

- Stehen beide Figuren still, ist die Kamera ruhig.
- Bewegt sich eine Figur, zieht die Kamera manchmal kurz in eine Richtung und kehrt dann in die richtige Lage zurück.
- Dasht der Client, wird die Kameralage manchmal ruckartig angepasst statt weich.
- Nachfrage 28.11.2026: nur in echter Sitzung auf zwei PCs übers Internet gesehen; auch beim reinen Laufen, aber seltener als
  beim Dash; Nähe zum Bildrand spielt keine Rolle; welche Seite, weiß der Mensch nicht sicher.

Nachstellen auf einem PC: `Start-Session.ps1 -Mod -Tunnel Lobby -Map /Game/Maps/DDDynamic_Map -Godmode` (mit und ohne
`-NetProfile Internet`), Figuren per `Game-Input.ps1` (Laufen, `Space` = Dodge), parallel
`py tools/spike/camera_jitter.py <host-pid> <client-pid> --seconds N --csv <datei>`. Die Probe liest den Blickort des lokalen
`PlayerCameraManager` und die Orte aller Figuren, rund 170 Bilder/s je Instanz, und meldet Rücksprünge und große Einzelschritte
je Figur. Welche Figur sich bewegt, an der Rolle prüfen (beim Client: 2 eigene, 1 fremde).
`POV.Location` steht mitten in der Kamera-Berechnung kurz auf Zwischenwerten (0, 0, 0 oder ein wiederkehrender fester Wert); eine
Umkehr des Blickorts aus einer einzelnen Probe ist darum kein Beleg.

## Belege

Messungen 28.11.2026, Build `26786272`, ein PC, zwei Instanzen. Alle Läufe in `DDDynamic_Map` lagen in der Aufzugkabine, in
der beide Figuren starten (`BP_Elevator_Tower_C`, Breite 523 Einheiten); nur der letzte Lauf war eine echte Runde.

- [Belegt, echte Runde, `-NetProfile Schlecht`, Godmode] Tag 1 der Route, Laufen und Dash beider Figuren nacheinander, 40 s:
  Blickort auf beiden Seiten ohne Umkehr (größter Schritt je Bild 42 beim Host, 28 beim Client), Kopie der Client-Figur beim Host
  folgt dem Client mit Verzug ohne Rücksprung, Kopie der Host-Figur beim Client einmal 52 Einheiten zurück. Der Mensch sah im
  Fenster nichts Auffälliges. Auf einem PC also nicht nachgestellt.

- [Belegt] Stillstand: Blickort ruhig. Während der Kamera-Berechnung steht `POV.Location` kurz auf (0, 0, 0), die Probe verwirft das.
  Kleine Wackler von etwa ±15 Einheiten bei stehenden Figuren kommen vermutlich vom Kamerawackeln beim Schießen (nicht geprüft).
- [Belegt] Laufen (beide Figuren, mit und ohne Netzprofil): Blickort folgt auf beiden Seiten gleichmäßig, keine Umkehr.
- [Belegt, `-NetProfile Internet`] **Dodge des Clients im Laufen, Kopie beim Host:** Der Host lässt die Client-Figur den Dodge selbst
  ausführen (Tempo −6500) und setzt sie danach auf die verspäteten Orte vom Client zurück. Zweimal von zwei Läufen: Rücksprung
  um 244 Einheiten (von 2376 auf 2620, danach 2536, 2461 … bis 1932) und um 234 Einheiten, dazu weitere Sprünge über 200 Einheiten
  in einer Dodge-Kette. Beim Client läuft derselbe Dodge glatt über rund 10 Bilder, ohne Rücksprung. Zeitlich passend loggt der
  Host `Client-Dodge (Schluessel 1) mit gemeldeter Richtung (-1.00,-0.00)`; in der Kette danach
  `Dodge-Werte der Client-Figur beim Host hingen … Reparatur 1`. Ein einzelner `Space` aus dem Stand ergab vier Dodges
  (Schlüssel 4 bis 7) mit Richtung (0, 0).
- [Vermutung, ohne Netzprofil] Dodge durch die stehende andere Figur: Die Kopie der stehenden Figur beim Host wird um 259 Einheiten
  weggeschoben und springt nach 15 ms zurück; Dodge aus dem Stand: Sprung um 493 Einheiten in einem Bild bei Tempo 0. Welche Figur
  gedasht hat, ist in diesem Lauf unklar (Logs zeigen Client-Dodges mit Schlüssel 1 bis 5, Rollen und Tasten passen nicht
  zusammen). Prüfidee: Lauf wiederholen, Rolle je Figur vor dem Tastendruck festhalten.
- [Vermutung] Beim Laufen hält der Host einzelne Bilder an: Lücke bis 56 ms statt rund 6 ms, Schritt 13,6 statt Median 4,8, dazu
  `Takt: Bewegung hielt den Frame 9.6 ms auf`. Ein solcher Hänger kann als kurzer Ruck wirken. Prüfidee: Lücken des Blickorts
  mit `Takt:`-Zeilen über mehrere Läufe abgleichen.
- [Belegt, Testkarte, mit und ohne Netzprofil] **Sprung der stehenden Client-Figur beim Host:** Steht die Client-Figur in der
  Aufzugkabine still (beim Client auf `BP_Elevator_Tower_C`, Ort fest), verliert ihre Kopie beim Host etwa alle 7,5 s die Unterlage (MovementBase leer), rutscht in
  rund 13 ms um 523 Einheiten in −x (Tempo −22005, auf x −1798,2, jedes Mal derselbe Ort) und steht ein bis drei Bilder später
  mit Unterlage wieder am Client-Ort. x −1798 ist die gegenüberliegende Kabinenwand, derselbe Ort auch im Lauf ohne Netzprofil.
  Viermal in 40 s, im Host-Log ±200 ms keine Zeile dazu. Testkarten-Artefakt, nicht der gemeldete Fehler: Der Mensch sieht im
  Stand keinen Ruck. Der Blickort des Hosts wackelt schon davor um ±20 bis 30, der des Clients nicht.
- [Belegt] Der Blickort glättet alle diese Sprünge stark: Auch beim Rücksprung um 244 Einheiten wird sein Schritt je Bild nur kurz
  kleiner (−22, −14, −18), er kehrt nicht um. Auf einem PC also kein sichtbarer Kamerasprung gemessen.
- [Belegt] Nativ: Kommt eine Figur dem Bildrand nahe, zoomt die Kamera heraus (`UseZoomOutAtEdge`, Arm 8000 bis 8205) und danach
  wieder herein. Der Blickort wandert dabei rund 90 Einheiten aus und zurück, ohne dass sich eine Figur so bewegt.
- [Belegt] Beim Client schaltet `FollowReplicatedLocation` für die Host-Figur in der Testkarte oft um (`folge dem Host-Ort` und
  `Unterlage … wieder bekannt` im Abstand weniger Sekunden). Kein Rücksprung der Host-Figur beim Client gemessen.

## Hypothesen

- [Vermutung] Was der Mensch als Kamerasprung sieht, ist beim Host der Rücksprung der Client-Figur nach ihrem Dodge (doppelter Schub:
  eigener `GA_Dash` beim Host plus übernommene Client-Orte, `bServerAcceptClientAuthoritativePosition`). Über zwei PCs mit echter
  Latenz läuft die Kopie weiter voraus, der Rücksprung wird größer. Prüfidee: Mensch fragen, auf welcher Seite; Lauf mit
  `-NetProfile Schlecht`; danach zu zweit.
- [Widerlegt, nur nach Aussage des Menschen] Der Zoom am Bildrand ist der „Zug in eine Richtung und zurück“ beim Laufen: Laut ihm tritt der Ruck unabhängig
  vom Bildrand auf und nur in echter Sitzung über zwei PCs, der Zoom wäre auch lokal zu sehen.
- [Vermutung] Ursache hängt an echter Latenz und Schwankung: Pakete kommen gebündelt oder in falscher Reihenfolge, die Kopie der
  fremden Figur springt beim Host (übernommene Client-Orte) oder beim Client (`FollowReplicatedLocation`, Vorausrechnen bis 150 ms,
  Teleport ab 6 m) vor und zurück, die Kamera rahmt beide Figuren und zieht mit. Beim Dash größer, weil Tempo −6500.
  Das Netzprofil der Mod (250 ms ±80, 5 % Verlust) reicht auf einem PC nicht, den Ruck zu zeigen. Was eine echte Sitzung über
  Steam zusätzlich hat (Paketbündel, schwankender Takt, zwei Rechner mit verschiedener Bildrate), ist offen.
  Prüfidee: `camera_jitter.py` auf beiden PCs während einer Live-Sitzung, CSV beider Seiten nach Zeit abgleichen.
- [Vermutung, nicht beobachtet] Vorausrechnen der Host-Figur beim Client (Engine oder `FollowReplicatedLocation`) überschießt: in
  einer Messung mit `-NetProfile Internet` monoton, kein Rücksprung über 30 Einheiten, `FollowReplicatedLocation` dabei die ganze
  Zeit aktiv. Prüfidee: mit `-NetProfile Schlecht` und mit Unterlage wiederholen.

## Nächster Schritt

Nächste Live-Sitzung auf zwei PCs: auf jedem PC `py tools/spike/camera_jitter.py <pid> --seconds 120 --csv <datei>` laufen
lassen, während beide laufen und dashen; der Mensch notiert die Uhrzeit, wenn er den Ruck sieht. Dann je Seite Rücksprünge der
fremden Figur und Umkehr des Blickorts an diesen Zeiten prüfen.

## Abnahme

Die Kamera folgt auf beiden Seiten beim Laufen und bei Dodge-Ketten beider Figuren ohne Sprung, in einer Live-Sitzung auf zwei PCs
gemessen und vom Menschen gesehen.
