# dev-flugschreiber: Flugschreiber für Live-Sitzungen, nur im Dev-Build

## Reproduktion

Anlass: Fehler wie [opt-kamera-ruck](opt-kamera-ruck.md) zeigen sich nur in echten Sitzungen auf zwei PCs und lassen sich auf
einem PC nicht nachstellen. Logs zeigen heute nur Zustandswechsel, `Takt:`-Warnungen, `Desync:` und F9-Marken
(`MARKE n` aus `Sync/SessionLog.cpp`), nicht den Verlauf davor. Wunsch des Teams vom 28.11.2026: Live-Analyse deutlich ausbauen,
im Release-Build darf nichts davon enthalten sein.

## Belege

- [Belegt] Der Takt der Mod ist ein Timer an der Fensterprozedur (`Game/GameThread.h`, 16 ms), kein Frame. Zwischen zwei Frames
  gelesen, also ohne die Zwischenwerte, die eine externe Probe mitten in der Kamera-Berechnung sieht.
- [Belegt] Release-Pakete baut `New-ReleasePackage.ps1` aus `Release`; Spieltests (`Start-Session.ps1`, `Deploy-Mod.ps1`) nutzen
  bisher ebenfalls `Release`. `Debug` läuft ohne Optimierung.

## Hypothesen

- [Vermutung] Ein Abtasten im Takt (rund 60 Hz) verpasst Sprünge von 13 bis 50 ms. Prüfidee: Aufrufe von `CameraPlayersHook` je
  Takt zählen; liegt er je Frame, dort in einen Ringpuffer abtasten.
- [Vermutung] Die replizierte Serverzeit taugt als gemeinsame Zeitachse beider PCs. Prüfidee: Feld per Reflection lesen, auf einem
  PC mit `-NetProfile Schlecht` gegen die Systemuhr messen.
- [Vermutung] Der zweite PC braucht für Live-Tests ein Dev-Paket. Prüfidee: Weg des Partners zur Mod und Versionsprüfung lesen.

## Nächster Schritt

Offene Fragen klären (Weg des zweiten PCs, Abtastrate, Serverzeit), dann in dieser Reihenfolge bauen: Schalter `Dev` mit
`DDCOOP_DIAGNOSTICS` und Prüfung im Release-Paket, Flugschreiber mit Kamera, Figuren und Marken, `Collect-Logs.ps1` und
Auswertungsskript.

## Abnahme

Wie in der TODO-Zeile: Dev-Build schreibt in einer Sitzung mit zwei Instanzen Aufzeichnungen beider Seiten, `Collect-Logs.ps1`
sammelt sie, ein Skript gleicht beide Seiten ab und schreibt eine Kurzfassung mit Auffälligkeiten und Markierungen, eigene
Messzeit des Flugschreibers unter 0,2 ms je Takt, Release-Paket nachweislich ohne Flugschreiber.
