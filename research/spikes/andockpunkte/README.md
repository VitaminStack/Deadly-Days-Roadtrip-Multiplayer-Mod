# Spike: technische Andockpunkte

Build `26187885`, Untersuchung am 02.11.2026. Kein Deployment und kein neuer
Spieltest. Beispiele und Ausgabe bleiben ausserhalb der Spielinstallation.

## Reproduzieren

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File research/spikes/andockpunkte/Test.ps1
py -B -m unittest discover -s research/spikes/andockpunkte -p test_audit.py
py -B research/spikes/andockpunkte/audit.py <Pfad-zur-Spiel-Exe>
```

`Test.ps1` benoetigt MSVC mit Windows SDK und schreibt nur nach
`research/local/andockpunkte`, einschliesslich seines temporaeren Build-Verzeichnisses.
Es startet **nicht** das Spiel. Der Audit liest die Exe nur und gibt keine lokalen
Pfade aus. Seine Signaturen sind keine Freigabe fuer andere Builds.

## Ergebnisse und Grenzen

- **[Belegt, statisch]** `VERSION.dll` wird regulaer mit genau drei benannten
  Funktionen importiert. Siehe den Abschnitt in `docs/ARCHITECTURE.md`.
- **[Belegt, isolierter Laufzeittest]** `ProxySmoke.exe` bindet gegen die
  Windows-Importbibliothek `version.lib`. Der Windows-Loader waehlt die lokale
  `version.dll`; der Test prueft deren Pfad und ruft alle drei importierten
  Funktionen auf. Eine gueltige `VS_FIXEDFILEINFO` kommt zurueck.
- **Fehlversuch:** `LoadLibraryExW(L"version.dll", ..., LOAD_LIBRARY_SEARCH_SYSTEM32)`
  lieferte beim Test den bereits geladenen Proxy. Die Weiterleitung lief rekursiv
  bis `0xC00000FD` (Stackueberlauf). Der korrigierte Versuch nutzt
  `GetSystemDirectoryW` und einen absoluten System-DLL-Pfad und besteht den Test.
  Keine feste Laufwerksannahme im Code.
- **[Belegt, statisch]** Alle drei Muster in `audit.py` ergeben genau einen Treffer
  in ausfuehrbaren PE-Sektionen, jeweils an der dokumentierten RVA.
- Sechs synthetische Scanner-Tests bestehen (exakt, ueberlappend, Wildcard,
  abgeschnitten, kein Treffer, ungueltige Eingabe).
- **Nicht getestet:** Laden des Proxys durch die Spiel-Exe, weitere dynamische
  Exporte, Mod-Bootstrap, Hook-Installation im Spiel, Hook-Rueckbau, Mapwechsel,
  gleichzeitige Hooks anderer Mods und Build-Wechsel.

Die bereits vorhandene lokale `dwmapi.dll` wurde weder ersetzt noch als Teil der
Spielauslieferung bewertet. Ihr Vorhandensein ist ein Konflikthinweis, kein Beweis
fuer den Ursprung der Datei.

## ProcessEvent: keine erneute Implementierung

Der bestehende Verbindungs-Spike liefert bereits zwei konkrete Wege:

1. `Widgets.cpp`: globaler Inline-Hook auf `0x160C040`, HUD-Konstruktion bei fehlendem
   GameState zurueckhalten und spaeter ueber das Original nachholen.
2. `Trace.cpp`: gemeinsamer VTable-Eintrag `0x4A` der Character-Klasse fuer RPC-Trace;
   ausserdem separate `UFunction::Func`-Hooks auf native Exec-Thunks (`+0xD8`).

Diese frueheren Spieltests sind in der Spike-A/B-README dokumentiert. Hier wurden
die Implementierungen gelesen und die Bytes auf Platte geprueft, nicht erneut im
Spiel erprobt. Die Muster identifizieren Einsprungstellen, beweisen aber weder
deren Semantik noch die C++-Signatur direkter nativer Methoden.

## Noch erforderlich

Fuer einen Abschluss der Roadmap-Zeile muss ein separater, geschuetzter Spieltest
ueber `tools/spike/Start-Session.ps1` Proxy-Laden und Hook-Weiterleitung belegen.
Aktuell wurde wegen der Vorgabe, nicht auf das Systemlaufwerk zu schreiben,
kein Spiel gestartet: `-UserDir` und umgeleitetes `TEMP` reichen nicht als Nachweis,
dass Betriebssystem-Crashberichte und Drittanbieter ebenfalls dort nichts ablegen.
Die kleine Test-Exe ist keine fertig deploybare Mod-Loader-Loesung.
