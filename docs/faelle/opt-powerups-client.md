# opt-powerups-client: Dosenessen und Staubsauger wirken beim Client nicht

Offen. Gefunden im Live-Test am 26.11.2026 (zwei PCs, Build `26786272`, Mod-Version 21).

## Reproduktion

Zwei PCs, gemeinsamer Run. Der Client nimmt das Power-up Dosenessen (Leben zurück) oder den Staubsauger (sammelt Loot ein). Beim
Client passiert nichts; nimmt der Host dieselben Power-ups, wirken sie bei ihm. Auf einem PC noch nicht nachgestellt.

## Belege

- [Gemeldet, Tester] Dosenessen und Staubsauger wirken beim Client nicht, beim Host schon. Das Power-up Dynamit wirkt beim Client.
- [Belegt, Client-Log] Je Minute kamen 4 bis 8 Loot-Objekte, die der Client lokal aufgenommen hatte, nach 6000 ms wieder zum
  Vorschein (`Loot beim Client: … nach 6000 ms wieder gezeigt`). Ob die Power-ups darunter waren, zeigt das Log nicht.
- [Belegt, Code] Power-ups laufen über dieselbe Aufnahme wie Schrott, Benzin und Itemsäcke (`src/Game/InventoryPatches.cpp`,
  `src/Game/PatchPolicy.h`); die lokale Kopie der Power-up-Kiste beim Client unterdrückt `src/Game/ClientPatches.cpp`.
- Verwandt: Bestätigungstest von `opt-loot-aufnahme-client` (Energy Drink und Level-Up wirkten am 24.11. laut Spieler nicht).

## Hypothesen

- [Vermutung] Wirkungen auf die eigene Figur (Leben, Einsammeln im Umkreis) bucht nur der Host. Die Aufnahme des Clients kommt bei
  ihm nicht als Aufnahme dieser Figur an, oder er wendet die Wirkung auf seine eigene Figur an. Dynamit wirkt in der Welt und fällt
  darum nicht auf.

## Nächster Schritt

Auf einem PC mit zwei Instanzen ein Power-up neben den Client legen lassen und vergleichen: Leben und Loot des Clients beim Host
und beim Client, Logzeilen der Aufnahme auf beiden Seiten.

## Abnahme

Dosenessen, Staubsauger, Energy Drink und Level-Up wirken auf die Figur des Clients wie beim Host, Leben und eingesammelter Loot sind
bei beiden gleich. Zwei PCs.
