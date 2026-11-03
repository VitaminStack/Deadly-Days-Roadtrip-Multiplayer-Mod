# Blueprint-Logik: Vererbung, Bytecode-Audit und Ablauftraces

Aufgabe `blueprint-logik`, Build `26187885`. Eigene Beschreibungen, keine
decompilierten Funktionskoerper oder SDK-Dateien. **Stichprobe, kein vollstaendiger
Blueprint-Audit.**

## Untersuchung

1. Mission-SDK nach Klassen, Vererbung und eigenen Blueprint-Funktionen durchsucht.
   Ein generierter SDK-Wrapper ruft die Engine auf und ist **nicht** die
   Implementierung des Blueprint-Graphen.
2. Zwei Instanzen ueber `Start-Session.ps1` direkt in `DDDynamic_Map` gestartet.
   Nur lesend Default-Objekte, FunctionFlags und `UStruct::Script` untersucht.
   Die eigene Sitzung danach mit `Stop-Session.ps1` beendet.
3. `blueprint_refs.py` liest die Script-TArrays und sucht bekannte UFunction-Pointer
   hinter drei Call-Token-Kandidaten. Es meldet nur eine sortierte Referenzmenge,
   keine Bytes, Parameter oder Graphrekonstruktion. Gegenprobe beim Client fuer
   PowerUpCrate, Airdrop und den ImpactEffect-Parent: Klassen und eigene Funktionen
   dort ebenfalls geladen. Das ist **kein** Nachweis einer RPC-Zustellung.

Token-Bezeichnungen zum Nachschlagen:
[UAssetAPI EExprToken](https://github.com/atenfyr/UAssetAPI/blob/master/UAssetAPI/Kismet/Bytecode/EExprToken.cs).
Genutzt: `EX_FinalFunction` (0x1C), `EX_LocalFinalFunction` (0x46), `EX_CallMath` (0x68).
Keine Implementierung aus dieser Quelle kopiert.

## Verhalten, in eigenen Worten

### Gegner und Spawns

- **[Belegt, SDK und geladene Klassen]** `BP_EnemySpawner_C` erbt direkt von
  `EnemySpawner`, hat keine eigenen Blueprint-UFunctions und repliziert im
  Default-Objekt nicht. Es ergaenzt Komponenten fuer Biome und Nachtvarianten.
  Die untersuchte Klasse ist damit vor allem Konfiguration fuer native
  Spawner-Logik, kein hier nachgewiesener zusaetzlicher Blueprint-Spawngraph.
- **[Belegt, Default]** `BP_Enemy_C` und `BP_Projectile_C` replizieren, der Spawner
  nicht. **Sync-Folgerung:** Host erzeugt Gegner/Projektile; der Client zeigt die
  nativen replizierten Ergebnisse. Kein zweiter Client-Spawner.
- **[Belegt, Referenzmenge]** Beim Projektile-Graphen liegen Verweise auf Lesen und
  Setzen der Actor-Skalierung sowie Vektorrechnung. `SetBulletSize` fuehrt in den
  gemeinsamen Ubergraph. **[Hypothese]** Dort wird die Darstellung an die
  Projektilgroesse angepasst. Weder Formel noch exakter Zeitpunkt sind mit dem
  Referenzscan belegt; die Sicht-/Treffergeometrie braucht einen gesonderten Test.

### PowerUp-Kiste: Spielwirkung und Darstellung sind gemischt

- **[Belegt, SDK/Flags]** `BP_PowerUpCrate_C` hat Damage-, BeginPlay- und Hit-Einstiege.
  Der Actor repliziert im Default-Objekt. `ShowEffect` ist als verlaesslicher
  NetMulticast markiert (`0x0C0040C0`). Das beweist keinen erfolgreichen Aufruf.
- **[Belegt, Referenzmenge]** Im gemeinsamen Graphen finden sich Authority-Pruefung,
  Schaden, Loot-Erzeugung, verzoegertes Actor-Spawning, An-/Abhaengen sowie ein
  FMOD-Ortssound. **[Hypothese]** Die Kiste verbindet Treffer-/Loot-Logik mit dem
  sichtbaren Zerstoeren und Sound. Welcher Zweig auf welcher Seite ausgefuehrt wird,
  ist noch nicht rekonstruiert.
- **Sync-Folgerung:** Keine komplette Damage-/Loot-Kette beim Client nachspielen,
  nur weil ihm ein Effekt fehlt. Bestehenden ShowEffect-Pfad und Actor-Replikation
  zuerst pruefen; bei Bedarf ausschliesslich kosmetische Darstellung ergaenzen.

### Airdrop: deklarierter RPC reicht nicht

- **[Belegt, Default und Flags]** `BP_Airdrop_Lootable_C` erbt `RewardChest` und hat
  Replikation im Default aus. `StartFallDown` ist als nicht verlaesslicher
  NetMulticast markiert (`0x0C004040`). Ein RPC auf einem nicht replizierten Actor
  garantiert kein entsprechendes Objekt oder Event beim Client.
- **[Belegt, Referenzmenge]** Es gibt Referenzen auf Hoehen-/Deckenpruefung,
  Niagara-Deckenwirkung, Sound, Skelettanimation, Oeffnungszustand,
  GameplayEffects, Gegner-Zurueckschieben und Reward-Auswahl.
  **[Hypothese]** Der Ablauf setzt Ankunfts-/Oeffnungsdarstellung mit
  autoritativen Belohnungs- und Spielwirkungen zusammen. Die Reihenfolge und
  Bedingungen wurden nicht aus den Namen abgeleitet.
- **Sync-Folgerung:** Bereit-/offen-Zustand und Reward-Sitzung hostseitig abbilden,
  Ankunft und Oeffnungsdarstellung beim Client ergaenzen, falls der native Weg
  nicht traegt. Kein pauschales Aktivieren der Actor-Replikation ohne Tests.
  Spaet beitretende Clients brauchen den aktuellen Zustand, nicht nur ein altes
  Fall-/Soundereignis.

### Item-Effekte: den gemeinsamen Parent untersuchen

- **[Belegt, SDK/Default]** `BP_Effect_Add_Explosive_Damage_C` und
  `BP_Effect_Add_Poison_Cloud_C` haben keine eigenen Blueprint-Funktionen und erben
  `BP_Effect_Add_ImpactEffect_Template_C`. Ihre `ImpactActor`-Defaults verweisen
  auf `BP_Impact_Explosion_Damage_C` bzw. `BP_Impact_Poison_Cloud_C`.
- **[Belegt, Referenzmenge]** Der Parent hat Apply-/Remove-Einstiege fuer sich
  selbst und andere Items; im gemeinsamen Graphen liegen Referenzen auf
  `InventoryItemInstance.AddImpactActor` und `RemoveImpactActor`.
  **[Hypothese]** Der Parent traegt damit Impact-Klassen an Items ein bzw. aus,
  statt bei jedem Apply sofort einen sichtbaren Impact zu erzeugen. Die Auswahl
  des betroffenen Items und alle Guards sind noch nicht voll ausgewertet.
- **[Belegt, Default]** Der Explosions-Impact erbt `RadialDamageImpactActor` und
  repliziert nicht. Die Giftwolke erbt `AreaGameplayEffectImpactActor` und
  repliziert dagegen. Das sind keine zwei identisch zu synchronisierenden
  „Effekt-Blueprints“.
- **Sync-Folgerung:** Schaden/Status/Itemmodifikationen beim Host belassen.
  Bereits replizierte Giftwolken nicht lokal duplizieren. Fehlende Explosions-VFX
  gegebenenfalls als **rein kosmetisches** Ereignis darstellen, nicht durch
  erneutes Erzeugen eines schadensausloesenden Actors beim Client.

### HUD und Events

- **[Belegt, SDK/Referenzen]** `WP_UI_C` besitzt lokale Fuel-/Day-/Second-/Threat-
  Einstiegspunkte sowie Construct/PreConstruct. Die Referenzmenge enthaelt
  GameState-/Fuel-/NightMode-Abfragen, Sound und Tween-/Widget-Aktionen.
- **Sync-Folgerung:** UI auf dem Client aus vorhandenem GameState und eigenem
  Sync-Anzeigemodell aufbauen. Keine UI-UObject-Pointer oder Host-Widgetgraphen
  uebertragen. Die bereits dokumentierte GameState-Warteschleife in PreConstruct
  bleibt ein Initialisierungsproblem, kein Beweis fuer einen fehlenden RPC.

## Werkzeug und Beleggrenzen

```powershell
py -B -m unittest discover -s tests -p test_blueprint_refs.py
py -B tools/spike/blueprint_refs.py <host-pid>
py -B tools/spike/blueprint_refs.py <client-pid> --owners BP_PowerUpCrate_C BP_Airdrop_Lootable_C
```

Fuenf Tests mit ausschliesslich synthetischen Daten bestanden. Es wurde kein
Bytecode-Dump ins Repository geschrieben; lokale JSON-Inventare bleiben unter
`research/local/blueprint-logik`. Kein Asset-Extractor oder neuer Runtime-Hook.

**Grenzen:** Die Token-Suche validiert keine vollstaendigen Instruktionsgrenzen;
Treffer innerhalb von Daten bleiben theoretisch moeglich. Virtuelle Aufrufe,
Delegate-Bindungen, Parameter, Spruenge und native Implementierungen werden nicht
vollstaendig ausgewertet. Default-Flags koennen je Instanz geaendert werden.
Keine Kontrollfluss-, Zeit- oder Authority-Aussage allein aus einem Treffer machen.
Klasse geladen bedeutet nicht, dass ihr Ereignis im Test ausgeloest wurde.

Nach SDK-Nachschlagen, Parent-/Default-Pruefung und Referenzscan blieb zunaechst fuer einen
vollstaendigen Abschluss ein versionspassender Kismet-Parser bzw. gezielter
Game-Thread-Trace erforderlich. Nicht getestet: konkrete Treffer-/Airdrop-Ereignisse
auf beiden Seiten, alle Effektklassen, Bossklassen, alle geladenen Blueprints und
Spaetbeitritt. Das war der unvollstaendige Stand der ersten Untersuchung.

## Nachtrag 03.11.2026: Grenzpruefung und Ereignistests

### Vom Kandidatenscan zum begrenzten Kismet-Audit

`kismet_audit.py` liest die In-Memory-Ausdruecke rekursiv statt jedes Byte als
moegliches Call-Token zu betrachten. Geprueft werden Operandenlaengen, verschachtelte
Ausdruecke/Terminatoren, Script-Ende, bekannte Ziele finaler Aufrufe, absolute
statische Sprungziele auf Statement-Grenzen und konstante Einspruenge der
Event-Wrapper in ihre geladenen Ubergraphen. Unbekannte Tokens, abgerissene Daten
und mehrdeutige Namenslayouts werden abgelehnt, nicht als Erfolg weitergescannt.

Auf Host und Client: **42 von 42 ausgewaehlten Funktionen vollstaendig gelesen,
25 von 25 Eventgraph-Einspruengen gueltig, identische Audit-Ergebnisse.**
Klassen ohne eigene Funktionen bleiben getrennt als native Konfiguration erfasst.

Beispiele fuer die gemeinsamen Graphen:

| Graph | Statements | Absolute statische Ziele | Bedingte Spruenge | Berechneter Dispatcher |
|---|---:|---:|---:|---:|
| Projektil | 22 | 0 | 0 | 1 |
| PowerUp-Kiste | 46 | 2 | 3 | 1 |
| Airdrop | 86 | 7 | 7 | 1 |
| Impact-Itemeffekt-Parent | 14 | 8 | 2 | 1 |
| HUD | 115 | 13 | 12 | 1 |

Das ist ein **Syntax-/Grenz- und Einsprungnachweis**, keine automatische Semantik
aller Zweige. Virtuelle Aufrufziele, Kontext-Skip-Semantik, alle moeglichen
latenten Fortsetzungen und native Implementierungen werden nicht vollstaendig
aufgeloest. Deshalb wurden die entscheidenden Pfade zusaetzlich ausgefuehrt.
Es werden nur Zaehler und symbolische Aufrufmengen ausgegeben, keine Graphkoerper,
Rohbytes, Prozesszeiger oder SDK-Dateien.

### Gezielter Runtime-Trace

`EventProbe.cpp` beobachtet ausgewaehlte `UFunction::Func`-Einstiege und native
Folgeaufrufe. Installation und Testaufrufe laufen ueber eine Fensternachricht.
Jeder Wrapper leitet genau einmal ans vorherige Original weiter. Die Ausgabe
enthaelt Eintritt, Klasse, Verschachtelung und explizite Zaehler; die ersten
100 Eintritte je Funktion werden ausgeschrieben. In der letzten Ereignissitzung
waren auf beiden Seiten alle **49 ausgewaehlten Hooks** installiert.

**Wichtige Grenze:** Lokale Script-zu-Script-Aufrufe koennen `UFunction::Func`
umgehen. Ein Ubergraph-Zaehler 0 beweist daher nicht, dass der Graph nicht lief.
Der Grenzaudit und die beobachteten nativen Folgeaufrufe ergaenzen genau das.

Fuenf eigene geschuetzte Sitzungen, Direktmission `DDDynamic_Map`, ein PC/Loopback,
Build `26187885`, getrennte Testspielstaende. Im ersten Aufbau gab es keine
geeignete Kiste; das wurde als Fehlversuch gespeichert. Danach wurden fuer die
kontrollierten Graphentests Kiste, Airdrop und Projektil ausdruecklich als
**Testobjekte** ueber Deferred-Spawn erzeugt. Kein behaupteter natuerlicher
Loot-/Missions-Spawn. Tutorialdialoge fuer zeitabhaengige Tests geschlossen.
Vorgefundene UE4SS-/Mod-Installation unveraendert; kein Stock-Kompatibilitaetsbeweis.

### Konkrete Befunde

**PowerUp-Kiste:** Die Host-Testkiste erscheint als replizierter Actor beim
Client (Role 3 gegen Role 1). `ApplyDamage` beim Host erreicht
`ReceiveAnyDamage`, eine Authority-Abfrage mit Ergebnis true, genau einen
`LootSpot.SpawnLoot`-Aufruf und einen FMOD-Ortssound-Aufruf. Beim Client wird
`ShowEffect` tatsaechlich empfangen; dies ist jetzt ein Zustellungsbeleg, nicht
nur ein RPC-Flag. Dort kein Loot-Aufruf im Test. Der clientseitige ApplyDamage-
Versuch liefert 0 und erreicht das Schadensevent nicht. Ein separat direkt
aufgerufenes Client-Schadensevent erreicht seinen Einstieg, aber keinen
beobachteten Loot-Aufruf. Daraus wird **kein** gemessenes `HasAuthority=false`
behauptet: Ein solches Ergebnis wurde nicht geloggt. Sound-/VFX-Wahrnehmung und
alle Kollisionszweige sind mit dem Eintrittslog allein nicht abgenommen.

**Airdrop:** Der Host-Actor hat auch zur Laufzeit Replikation aus. Nach
`StartFallDown` folgt eine latente Graphfortsetzung mit FMOD-Aufruf. Der gezielt
ausgeloeste Oeffnungs-Callback erreicht Authority=true und genau einen nativen
`RewardChest.OpenAirdropChoices`-Aufruf. Der Callback-Einstieg erscheint zweimal
in der Verschachtelung, was **nicht** als zwei Reward-Erzeugungen gezaehlt wird.
Auf dem Client weder entsprechender Actor noch Fall-/Oeffnungsereignis im Test.
Damit braucht der Client einen bestaetigten Airdrop-Zustand und Darstellung;
Host-Reward-Entscheidung nicht lokal nachspielen. Keine Belohnungsauswahl/-annahme,
kein vollstaendiger Deckendurchbruch- oder Spaetbeitrittstest behauptet.

**Impact-Itemeffekt:** Mit dem Default `ApplyToSelf=false` laufen Apply/Remove
ohne beobachteten Add-/RemoveImpactActor-Aufruf. Nur am frisch erzeugten
Testeffekt auf true gestellt: Eigen-Apply erreicht `AddImpactActor`, Eigen-Remove
`RemoveImpactActor`. Wieder false: ApplyToOther/RemoveFromOther mit gueltigem
Itemkontext erreichen jeweils dieselben nativen Itemaenderungen. Das wurde beim
Host **und bei explizitem lokalem Aufruf beim Client** gemessen. Der Parent ist
also tatsaechlich Itemmodifikation mit Eigen-/Fremditem-Pfaden, nicht bloss VFX;
kein eingebauter Netzwerktransport oder verlaesslicher Host-Guard fuer diese
lokalen Methodenaufrufe. Explosion/Giftwolke bleiben unterschiedliche Impact-
Defaults mit unterschiedlicher Actor-Replikation; kein Damage-Actor-Replay beim
Client. Kein bewaffneter Explosions-/Giftwolken-Treffertest behauptet.

**Projektil:** Lokaler `SetBulletSize(2)`-Aufruf erreicht die Actor-Skalierung
und aendert `(1,1,1)` auf `(1.5,1.5,1)`; separat auf Host und Client ausgefuehrt.
In einem weiteren Test nur beim Host `SetBulletSize(3)`: Host `(2.25,2.25,1)`,
Client bleibt `(1,1,1)` und hat keinen SetBulletSize-Eintritt. Actor-Replikation
allein garantiert also nicht diese Groessenanpassung. Im SDK ist `BulletSize`
nicht Net. Fuer die Umsetzung einen bestaetigten Groessen-/Darstellungszustand
vorsehen und den echten Schuss-/Pool-Reuse-Pfad pruefen. Aus zwei Messpunkten wird
keine allgemeine Formel oder korrekte Client-Treffersimulation behauptet.

**HUD:** Nach Freigabe der Tutorialpause kommen echte `OnSecondChanged`-
Ereignisse auf beiden Seiten an. Zusaetzliche explizite lokale Callback-Tests
erreichen ebenfalls den Einstieg. Das Feld `intseconds` blieb dabei unveraendert;
es ist **kein** nachgewiesener allgemeiner HUD-Setter oder visueller Erfolg.
Zeit-/Fuel-/Day-Darstellung lokal aus bestaetigten Daten versorgen, keine Host-
Widgetgraphen replizieren. Alle Fuel-/Threat-/Tween-Zweige bleiben Integrationstests.

### Ergebnis fuer die Umsetzung

Die urspruengliche Frage zu typischen Spawn-, Effekt- und Eventpfaden ist damit
an repraesentativen Beispielen geklaert: native Spawn-/Kampfautoritaet auf dem
Host, vorhandenen Kisten-Multicast nutzen, nicht replizierten Airdrop-Zustand und
fehlende Projektilgroesse gezielt nachliefern, Itemwirkungen nicht beim Client
nachspielen, HUD lokal versorgen. Der Kontrollfluss wurde nicht mehr nur aus
Referenznamen geraten; ausgewaehlte Guards/Verzweigungen sind dynamisch belegt.
Das ist **kein vollstaendiger Audit jedes Blueprints, Bosses und Effektzweigs**
und keine implementierte Synchronisierung. Die genannten spezifischen
Integrationspruefungen bleiben erhalten, statt als erledigt zu gelten.

### Wiederholen und lokale Belege

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File research/spikes/blueprint-logik/Build-Probe.ps1
py -B tools/spike/blueprint_refs.py <pid>
py -B tools/spike/blueprint_trace_summary.py <lokales-host-log> <lokales-client-log>
py -B -m unittest discover -s tests -p 'test_*.py'
```

Probe nur in eigener Sitzung mit `inject.py` laden. Befehle ueber
`<TEMP>/ddblueprint/<pid>.cmd`: 0 Trace installieren, 1 ApplyDamage Kiste,
2 Projektilgroesse 2, 3 Airdrop-Fall, 4 Effekt-Guardmatrix, 5 Zaehler/Actor-Survey,
6/7/8 kontrollierter Testspawn Kiste/Airdrop/Projektil, 9 direktes Schadensevent,
10 Airdrop-Oeffnungs-Callback, 11 Projektilgroesse 3, 12 lokaler HUD-Callback.
Niemals gegen fremde Sitzung, nie Mapwechsel/Hot-Unload mit geladener Probe.
Die Testobjekte und Hooks enden mit der eigenen Sitzung; alle Sitzungen beendet.

Lokale Belege: `events-{host,client}-final.log`, `event-summary.jsonl`,
`{host,client}-boundary-audit.json`; Fehlversuche und fruehere Sitzungen separat.
**21 synthetische Python-Tests bestanden**, davon sieben fuer den Grenzaudit
und sechs fuer die Trennung von Hook-Installation und beobachteter Ausfuehrung.
Native Probe mit lokalem Mission-SDK gebaut. Keine dieser Spieldaten im PR.
