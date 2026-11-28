# opt-kamera-ruck: Gemeinsame Kamera zieht kurz weg oder ruckt, sobald sich eine Figur bewegt

## Reproduktion

Meldung des Menschen vom 28.11.2026, Build `26786272`, Seite (Host oder Client) nicht genannt:

- Stehen beide Figuren still, ist die Kamera ruhig.
- Bewegt sich eine Figur, zieht die Kamera manchmal kurz in eine Richtung und kehrt dann in die richtige Lage zurück.
- Dasht der Client, wird die Kameralage manchmal ruckartig angepasst statt weich.

## Belege

Messung 28.11.2026, Build `26786272`, ein PC, zwei Instanzen, `-Mod -Tunnel Lobby -Map /Game/Maps/DDDynamic_Map -Godmode`, ohne
Netzprofil. Probe `tools/spike/camera_jitter.py <host-pid> <client-pid> --csv`, Blickort des lokalen `PlayerCameraManager` und Orte der
Figuren, rund 170 Bilder/s je Instanz. Figuren per `Game-Input.ps1` (Laufen, `Space` = Dodge).

- [Belegt] Stillstand: Blickort auf beiden Seiten ruhig. Während der Kamera-Berechnung steht `POV.Location` kurz auf (0, 0, 0); die
  Probe verwirft solche Proben.
- [Belegt] Laufen des Clients: Blickort folgt auf beiden Seiten gleichmäßig, je 100 ms 6 bis 8 Einheiten, Host und Client höchstens
  3 Einheiten auseinander.
- [Belegt] **Dodge des Clients ist ein Sprung in einem Bild**, auf beiden Seiten: Client-Figur beim Host von −731 auf −241 (490
  Einheiten) zwischen zwei Proben 30 ms auseinander, CMC-Tempo dabei 0; beim Client die eigene Figur 24 ms später ebenso, Tempo 0.
  Mod-Log Client: `eigener Dodge (Schluessel 3..5) … gemeldet`, dazu einmal `Dodge der eigenen Figur abgebrochen`; Host:
  `Client-Dodge (Schluessel 3) nach 62 ms … gestartet`. Die eigene Vorhersage des Clients bewegt die Figur also nicht, sie springt erst
  mit dem Ergebnis des Hosts.
- [Belegt] Zum Vergleich der **Dodge des Hosts**: Host-Figur über rund 5 Bilder (60 ms) mit Tempo −6500, auf beiden Seiten.
- [Belegt] Der Blickort glättet beide Dodges ohne Netzprofil: kein Sprung, gleichmäßige Annäherung (+4 bis +10 je 10 ms).
- [Belegt] Nebenbefund, nativ: Kommt eine Figur dem Bildrand nahe, zoomt die Kamera heraus (`UseZoomOutAtEdge`, Arm 8000 bis 8122) und
  danach wieder herein. Der Blickort wandert dabei rund 90 Einheiten aus und zurück, ohne dass sich eine Figur so bewegt.
- [Belegt] Beim Client schaltet `FollowReplicatedLocation` für die Host-Figur in der Testkarte oft um (`folge dem Host-Ort` und
  `Unterlage … wieder bekannt` im Abstand weniger Sekunden).

## Hypothesen

- [Vermutung] Die Kamera (`AAllPlayerFramingCamera`) rahmt die Orte der Actors, nicht die geglätteten Meshes. Die Kopie der jeweils
  anderen Figur springt mit jedem Netz-Update; die Glättung der Engine wirkt nur aufs Mesh. Hinweis: Das beim Host gemessene Tempo der
  Client-Figur springt mit den Netz-Updates, Spitzen bis 2407 ([Laufanimation](../archive/GAME_ANALYSIS.md#laufanimation-beim-client-opt-laufanimation-client)).
  Prüfidee: Kameraort und Ort beider Figuren je Frame auf beiden Seiten lesen, Sprünge des Kameraorts den Sprüngen eines Figurenorts zuordnen.
- [Vermutung] Beim Host: Der Dodge der Client-Figur läuft dort als eigener Schub (`GA_Dash`, Eingabe vom Client gemeldet), dazu kommen
  die Orte vom Client (`bServerAcceptClientAuthoritativePosition`). Beide schieben die Figur, der Ort springt vor und zurück.
  Prüfidee: Ort der Client-Figur beim Host während einer Dodge-Kette je Frame lesen, mit dem Ort beim Client vergleichen.
- [Vermutung] Beim Client: Die Kopie der Host-Figur wird mit dem replizierten Tempo vorausgerechnet (Engine oder `FollowReplicatedLocation`
  in `src/Game/MovementPatches.cpp`, höchstens 150 ms). Hält der Host an, schießt die Kopie über und springt zurück; über 6 m Abstand
  setzt die Mod sie hart um. Prüfidee: dieselbe Messung beim Client mit laufender und anhaltender Host-Figur.

## Nächster Schritt

Lesende Probe für Kameraort und Figurenorte je Frame bauen, eine Sitzung mit zwei Instanzen, Host- und Client-Figur nacheinander laufen
und dashen lassen, auch mit `-NetProfile Internet`.

## Abnahme

Die Kamera folgt auf beiden Seiten beim Laufen und bei Dodge-Ketten beider Figuren ohne Sprung, auch mit `-NetProfile Internet`,
auf einem PC mit zwei Instanzen gemessen.
