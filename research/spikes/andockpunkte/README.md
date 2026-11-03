# Spike: technische Andockpunkte

Build `26187885`, Untersuchung am 02.11.2026. Der End-to-End-Spieltest ist inzwischen
erfolgt, siehe Nachtrag unten. Das Minimalbeispiel allein ist **nicht** der
spielgeeignete Proxy. Ausgaben bleiben lokal; der temporaere Link in der
Spielinstallation wurde nach dem Test entfernt.

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

## Nachtrag: vollstaendiger Exportproxy und End-to-End-Test

Die zuvor genannten fehlenden **kleinen Andockpunkt-Tests** wurden nachgeholt.
Der Abschnitt „Noch erforderlich“ beschreibt die Grenze der ersten Untersuchung,
nicht mehr den Abschlussstand dieser Aufgabe.

### Entscheidender Fehlversuch

Der Proxy mit nur den drei direkt von der Exe importierten Funktionen bestand
den isolierten Test, reichte aber fuer den Spielstart nicht: Beide gestarteten
Instanzen beendeten sich mit Exit-Code 0 vor dem Spielfenster. Auch anderes
Arbeitsverzeichnis und die separate Spielkopie loesten das nicht. Kein Absturz
oder konkrete fehlende Funktion wurde daraus behauptet. Auch ein erst durch
API-Aufrufe gestarteter Bootstrap ist nicht zuverlaessig: Die Exporte muessen
nicht vor dem benoetigten Bootstrap-Zeitpunkt benutzt werden.

`FullVersionProxy.*` bietet stattdessen alle 17 Exporte der untersuchten
System-VERSION.dll, einschliesslich Namen/Ordinalen, an. MASM-Stubs erhalten die
vier Integer- und XMM-Argumentregister sowie Stackparameter und springen nach
Aufloesung in die Originalfunktion. Insbesondere wird fuer den undokumentierten
ByHandle-Export **keine** C++-Signatur geraten. Das Original wird ueber den
absoluten Systempfad geladen; der Resolver erhaelt den eingehenden LastError.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File research/spikes/andockpunkte/Test-FullProxy.ps1 -WithEntryProbe
```

Das Skript baut und prueft nur lokal. Es installiert nichts ins Spiel.
Ein gesonderter, ausdruecklich genehmigter temporaerer Hardlink neben der
installierten Spiel-Exe ermoeglichte den echten Ladeversuch. Der Dateiinhalte
blieben im lokalen Buildordner. Vorhandene Dateien/Mods wurden nicht ersetzt.

### Erfolgreicher Spieltest

- Start ausschliesslich ueber `Start-Session.ps1`, mit Crash-Schutz und getrennten
  UserDirs. Zwei Instanzen auf einem PC, Loopback, Mission `DDDynamic_Map`.
- Die Testvariante startet im Attach nur einen Worker und wartet dort **nicht**
  auf ihn. Engine-Zugriffe erfolgen erst spaeter ueber eine Fensternachricht.
- **Host und Client:** Proxy-Bootstrap aktiv, alle drei Signaturen in den
  ausfuehrbaren **geladenen** PE-Sektionen eindeutig und an den erwarteten RVAs.
- **Host und Client:** Die Probe fuehrt `Actor.K2_GetActorLocation` vor dem Hook
  und durch den temporaeren Character-ProcessEvent-Hook aus. Der Hook leitet
  genau einmal an das vorherige Original weiter. Rueckgabewert unveraendert,
  Slot danach wiederhergestellt. Protokoll je Seite:

```text
Live-Signatur ProcessEvent: hits=1 expected=yes
Live-Signatur SetClientTravel: hits=1 expected=yes
Live-Signatur SeamlessTravelToLevel: hits=1 expected=yes
Probe: Fensterthread bestaetigt
ProcessEvent probe: calls=1 same-result=yes restored=yes
```

Die Spielinstallation hatte zusaetzlich UE4SS mit einer vorhandenen Mod geladen.
Das wurde weder deaktiviert noch als reine Stock-Umgebung ausgegeben. Bestaetigt
ist die Kompatibilitaet **dieses kleinen Beispiels** mit dem vorgefundenen Aufbau,
nicht mit beliebigen Mods. Der globale Widget-Hook des Verbindungs-Spikes bleibt
in der Weiterleitungskette. Die kleine Probe ist kein generisches Hook-Framework:
keine Parameterverzoegerung, kein Hot-Unload und kein allgemeiner Thread-Dispatcher.

Damit sind Proxy-Kandidat, ProcessEvent-Weg und drei native Byte-Signaturen am
kleinen Beispiel erprobt. Die spaetere produktive Loader-/Core-Implementierung,
andere Windows-Exporttabellen, weitere Builds und ein voller Run bleiben eigene
Aufgaben. Bei Exporttabellenabweichungen oder fremd geaenderten Einsprungbytes
muss die Implementierung ablehnen statt die hier geprueften Werte zu uebernehmen.
