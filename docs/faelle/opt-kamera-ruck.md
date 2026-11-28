# opt-kamera-ruck: Gemeinsame Kamera zieht kurz weg oder ruckt, sobald sich eine Figur bewegt

## Reproduktion

Meldung des Menschen vom 28.11.2026, Build `26786272`, Seite (Host oder Client) nicht genannt:

- Stehen beide Figuren still, ist die Kamera ruhig.
- Bewegt sich eine Figur, zieht die Kamera manchmal kurz in eine Richtung und kehrt dann in die richtige Lage zurück.
- Dasht der Client, wird die Kameralage manchmal ruckartig angepasst statt weich.

Nachstellen auf einem PC: `Start-Session.ps1 -Mod -Tunnel Lobby -Map /Game/Maps/DDDynamic_Map -Godmode` (mit und ohne
`-NetProfile Internet`), Figuren per `Game-Input.ps1` (Laufen, `Space` = Dodge), parallel
`py tools/spike/camera_jitter.py <host-pid> <client-pid> --seconds N --csv <datei>`. Die Probe liest den Blickort des lokalen
`PlayerCameraManager` und die Orte aller Figuren, rund 170 Bilder/s je Instanz, und meldet Rücksprünge und große Einzelschritte
je Figur. Welche Figur sich bewegt, an der Rolle prüfen (beim Client: 2 eigene, 1 fremde).
`POV.Location` steht mitten in der Kamera-Berechnung kurz auf Zwischenwerten (0, 0, 0 oder ein wiederkehrender fester Wert); eine
Umkehr des Blickorts aus einer einzelnen Probe ist darum kein Beleg.

## Belege

Messungen 28.11.2026, Build `26786272`, ein PC, zwei Instanzen.

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
- [Vermutung] Der Zoom am Bildrand ist der „Zug in eine Richtung und zurück“ beim Laufen. Prüfidee: dasselbe ohne Mod, lokales Coop.
- [Vermutung, nicht beobachtet] Vorausrechnen der Host-Figur beim Client (Engine oder `FollowReplicatedLocation`) überschießt: in
  einer Messung mit `-NetProfile Internet` monoton, kein Rücksprung über 30 Einheiten, `FollowReplicatedLocation` dabei die ganze
  Zeit aktiv. Prüfidee: mit `-NetProfile Schlecht` und mit Unterlage wiederholen.

## Nächster Schritt

Den Menschen fragen, auf welcher Seite er den Sprung sah und ob die Figur mitsprang. Dann `-NetProfile Schlecht` messen und prüfen,
ob der Host den Dodge der Client-Figur selbst ausführen muss oder nur Kosten buchen und den Schub den Client-Orten überlassen kann.

## Abnahme

Die Kamera folgt auf beiden Seiten beim Laufen und bei Dodge-Ketten beider Figuren ohne Sprung, auch mit `-NetProfile Internet`,
auf einem PC mit zwei Instanzen gemessen.
