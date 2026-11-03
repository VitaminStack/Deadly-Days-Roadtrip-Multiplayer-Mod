# Blueprint-Logik: Vererbung, Default-Objekte und Skriptreferenzen

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

Nach SDK-Nachschlagen, Parent-/Default-Pruefung und Referenzscan bleibt fuer einen
vollstaendigen Abschluss ein versionspassender Kismet-Parser bzw. gezielter
Game-Thread-Trace erforderlich. Nicht getestet: konkrete Treffer-/Airdrop-Ereignisse
auf beiden Seiten, alle Effektklassen, Bossklassen, alle geladenen Blueprints und
Spaetbeitritt. Deshalb bleibt die Roadmap-Zeile offen.
