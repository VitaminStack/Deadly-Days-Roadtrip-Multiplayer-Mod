# opt-powerups-client: Dosenessen und Staubsauger wirken beim Client nicht

Behoben mit Mod-Version 22, auf einem PC geprüft am 27.11.2026; Bestätigung auf zwei PCs offen. Gefunden im Live-Test am 26.11.2026
(zwei PCs, Build `26786272`, Mod-Version 21).

## Reproduktion

Ein PC, zwei Instanzen: `Start-Session.ps1 -Mod -Tunnel Lobby -Map /Game/Maps/DDDynamic_Map -Godmode -Brief` mit
`DDCOOP_TEST_MAP_PROFILE` auf `MGP_Forest`. Die Client-Figur mit `!shop to <x> <y>` beim Client weit weg von der Host-Figur stellen,
sonst greift der Attraktor der falschen Figur zu. Beim Host `!shop powerup <Klasse> <Spielerindex> [Abstand]` legt das Power-up an die
Figur, `!shop powerup try <Spielerindex>` löst ihren Slot aus wie die Taste. `py tools/spike/powerups.py <host-pid> <client-pid>` zeigt
je Figur Leben, Ausdauer, Loot-Radius und Slots, dazu die Power-ups in der Welt. Schaden: `!god off`, `DD_DamagePlayerAtIndex 40 1`,
`!god on`; vor jeder Lebensmessung `DD_KillAllEnemies 0`.

## Belege

- [Belegt, Messung] Dosenessen an der Host-Figur: Leben 60 → 80, `GA_PowerUp_Heal_C.K2_ActivateAbility` im Host-Log. An der
  Client-Figur: Leben bleibt 60 auf beiden Seiten, keine Aktivierung auf keiner Seite.
- [Belegt, Code] `APowerUpLoot::Loot_Implementation` (`0x59B2250`) holt das `ActivePowerupInventory` am PlayerState der Figur. Mit
  `bTryImmediateActivation` und `CanBeImmediatelyActivated` aktiviert es sofort über einen temporären Slot (`0x5A297F0`), sonst legt es
  das Power-up mit `TryAddPowerup` in einen freien Slot. Beides endet in `UPowerupSlot::TryActivateSlot` (`0x5A288B0`): Effekt über
  `ApplyGameplayEffectSpecToSelf`, Fähigkeit über `GiveAbilityAndActivateOnce`. Keine Rollenprüfung, kein RPC.
- [Belegt, Messung] Definitionen (`powerups.py --defs`): Dosenessen `GA_PowerUp_Heal_C`, Level-Up `GA_PowerUp_LevelUp_C`, beide
  `LocalPredicted` und sofort; Staubsauger `GE_PowerupAttractionRadius_Speed_C`, Energy Drink `GE_IncreaseStaminaRegen_C` und Dynamit
  `GE_ExplosivePowerupTime_C` sind Effekte ohne Sofortwirkung, sie gehen in den Slot.
- [Belegt, Messung] Lücke 1, Fähigkeiten: `GiveAbilityAndActivateOnce` bricht beim Host für die nicht lokal gesteuerte Client-Figur
  bei `LocalPredicted` ab. Mit der Richtlinie `ServerOnly` am Klassen-Defaultobjekt (Experiment per Speicherschreiben in der eigenen
  Host-Instanz) heilt das Dosenessen die Client-Figur 60 → 80 auf beiden Seiten, Level-Up hebt die Stufe beim Host 3 → 4, der Client
  zeigt `Host-Level 4`.
- [Belegt, Messung] Lücke 2, Slots: Ein Staubsauger an der Client-Figur landet beim Host im Slot der Client-Figur und beim Client
  zusätzlich in dessen lokalem Slot. Die Taste löst nur den lokalen Slot des Clients aus, dort fehlt die Autorität. Löst der Host den Slot
  der Client-Figur aus (`TryActivateSlot`), steigt der Loot-Radius 450 → 450000 beim Host und repliziert zum Client.
- [Belegt, Messung] Folgefehler: Der Slot der Client-Figur beim Host bleibt voll. `CanBeLooted` verlangt Sofortwirkung oder einen
  freien Slot, darum bleibt jedes weitere Slot-Power-up (gemessen: Energy Drink nach Dynamit) an der Client-Figur liegen.
- [Belegt, Messung] `ActivePowerupInventory.ActivateSlot` per `ProcessEvent` löst nichts aus; `TryActivateSlot(false)` am Slot schon.

## Hypothesen

- [Widerlegt] Der Host bekommt die Aufnahme der Client-Figur nicht mit: Sie kommt an, der Slot der Client-Figur beim Host füllt sich.
- [Vermutung] Das automatische Benutzen der Slots richtet sich beim Host nach seiner eigenen Einstellung, auch für die Client-Figur; die
  beiden Inventare teilen dort dieselben Einstellungsobjekte. Nicht gemessen, im Test war es nie ausgelöst.
- [Vermutung] Dynamit wirkte im Live-Test beim Client, weil die Explosion in der Welt auch ohne den Effekt sichtbar ist. Gemessen
  wurde nur, dass Dynamit an der Client-Figur wie der Staubsauger in den Slot geht.
- [Vermutung] Power-ups aus Items (Schnellkochtopf, Kriegstrommel, Teleporter) haben beim Client dieselbe Slot-Lücke. Ihre Slots sind
  nicht verbrauchbar und bleiben vom Abgleich unberührt. Nicht gemessen.

## Umsetzung

Prüfliste aus `docs/ARCHITECTURE.md`:

- Lokal bleibt beim Client: Taste und automatisches Benutzen, das native `TryActivateSlot` mit Klang, Anzeige und Leeren des Slots, beim
  Aufsammeln Klang und Darstellung der Basis wie beim Schrott.
- Geteilt wird der Slot-Stand der Client-Figur (`0xE00`, Host an Client) und das Auslösen eines verbrauchbaren Slots (`0xE01`, Client an
  Host). Native Replikation gibt es dafür nicht: Inventar und Slots replizieren nicht, `TryActivateSlot` hat keinen RPC. Leben, Stufe,
  Attribute und Effekte replizieren nativ, dafür keine Nachricht.
- Es entscheidet der Host: Er füllt die Slots aller Figuren, löst sie aus und stellt Power-up-Fähigkeiten vorher auf `ServerOnly`
  (`src/Game/PowerUpPatches.cpp`, Regel in `src/Game/PowerUpPolicy.h`). Der Client bucht beim Aufsammeln nichts mehr
  (`PowerUpLootHook` in `src/Game/InventoryPatches.cpp`) und gleicht seine Slots mit `TryAddPowerup` und `TryRemovePowerup` an.
- Ablehnung und Neuaufbau: Hat der Host den gemeldeten Slot nicht oder läuft die Abklingzeit, bestätigt er nur; der nächste Stand legt
  einen noch vollen Slot zurück. Wiederholung alle 2 s, neue Welt verwirft offene Meldungen. Getestet in
  `tests/GameTests/PowerUpPolicyTests.cpp` und `tests/NetTests/PowerUpTests.cpp`.

## Prüfung auf einem PC (27.11.2026, Mod-Version 22)

Zwei Instanzen wie unter Reproduktion, Client-Figur 2000 Einheiten neben der Host-Figur, nur Befehlskanäle des Spikes.

| Power-up an der Client-Figur | Host | Client |
|---|---|---|
| Dosenessen (Leben 60) | 60 → 80, `GA_PowerUp_Heal_C` auf `ServerOnly` | 60 → 80 |
| Staubsauger, Slot beim Client ausgelöst | Loot-Radius 450 → 450000 für 5 s | gleichzeitig 450 → 450000 für 5 s |
| Energy Drink, Slot beim Client ausgelöst | Regeneration 1,5 → 155 | 1,5 → 155 |
| Level-Up | Stufe 5 → 6 | Anzeige `Host-Level 6` |
| Dynamit, Slot beim Client ausgelöst | Slot der Client-Figur ausgelöst und leer | Slot leer |

- Slots: Der Host-Slot der Client-Figur füllt sich beim Aufsammeln, der Client übernimmt ihn rund 50 ms später; löst der Host ihn selbst
  aus, nimmt der Client ihn wieder aus seinem Slot. Schrott danach auf beiden Seiten 14.
- Gegenprobe Host-Figur: Staubsauger aus ihrem Slot wirkt wie vorher (450000), der Client sieht es an der Mitspieler-Figur.
- Keine Warnung zu Power-ups, kein Absturz. Nicht geprüft: echte Taste, automatisches Benutzen, Power-ups aus Items (Schnellkochtopf,
  Kriegstrommel, Teleporter), mehrere Clients, die Explosion des Dynamits in der Welt.

## Nächster Schritt

Bestätigung auf zwei PCs mit echter Taste: Der Client nimmt Dosenessen verletzt, Staubsauger, Energy Drink, Level-Up und Dynamit und
löst die Slot-Power-ups selbst aus; dabei Leben, Schrott und Stufe auf beiden Seiten vergleichen.

## Abnahme

Dosenessen, Staubsauger, Energy Drink und Level-Up wirken auf die Figur des Clients wie beim Host, Leben und eingesammelter Loot sind
bei beiden gleich. Zwei PCs.
