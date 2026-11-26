# opt-kopierladen: Kopie des Clients im Kopierladen geht verloren

## Reproduktion

Ein PC, zwei Instanzen: `DDCOOP_TEST_MAP_PROFILE=/Game/Map_Generation/Blueprints/MapEvents/MGP_CopyShopEvent.MGP_CopyShopEvent` in PowerShell
setzen, dann `Start-Session.ps1 -Build -Mod -Tunnel Lobby -Map /Game/Maps/DDDynamic_Map -Brief`.
Beim Host `!scrap 200` und `!give ItemData_SMG`, beim Client zum Laden, Dialog weiter und `!shop copy ItemData_SMG` (Ablauf in
[TESTING.md](../TESTING.md#gezielt-in-eine-karte)).
Ohne Fix kommt die Kopie nicht ins Inventar, der Schrott springt auf den Stand des Hosts zurück.

## Belege

- [Belegt, Spiel] Live-Test 24.11.2026 (zwei PCs, Build `26786272`): Client-Log „Inventar lokal: AddToInventory des Clients außerhalb einer
  Bearbeitung geht nativ an den Host“, 4 s später im Host-Log „AddToInventory ohne Item vom Client verworfen“, danach „Schrott: Stand des Hosts
  übernommen, 452 statt 441“. Keine Bearbeitung, kein Edit.
- [Belegt, Spiel] Die Original-SMG blieb im Live-Test erhalten: Das Client-Log zeigt sie danach bis zum Ende des Runs im Rucksack
  (`Rucksack-Anzeige: ItemData_SMG … Pivot (-2, 0)`). Verschwunden ist nur die Kopie, die das Panel kurz gezeigt hatte.
- [Belegt, Spiel] 26.11.2026 auf einem PC nachgestellt, wie oben: dieselben drei Logzeilen, `inventory.py` danach vollständig und gleich, beide
  Seiten ohne Kopie; Original unverändert.
- [Belegt, Disassembly] Ablauf im Panel `UCopyShopPanel` (`WI_CopyShopPanel_C`): Die Auswahl eines Items im Inventar-Panel setzt `ItemToCopy`
  (`+0x388`), `ItemInventory` (`+0x390`) und über `SetPriceForItem` den Preis (`+0x398`, nicht reflektiert). „Item kopieren“ ruft
  `HandleCopyItem` (Knöpfe sperren, Scan-Animation), danach legt `FinalizeCopyItem` (`0x58B4250`) mit `CreateItemInstanceCopy` eine Kopie an,
  ruft `AddToInventory` und zieht den Preis lokal ab (`AddAmount`). Beim Client ist `AddToInventory` ein Server-RPC mit einer Instanz, die der
  Host nicht kennt.
- [Belegt, Spiel] Die native Kopie beim Host übernimmt Daten, Seltenheit und den Zustand „kaputt“: Eine kaputte seltene Pistole ergab eine
  kaputte seltene Kopie im Lager, Preis 26 (SMG ungewöhnlich: 11). Eine Kopie ist darum kein Kauf eines neuen, heilen Items.
- [Belegt, Spiel] Der POI des Kopierladens verschwindet nach einer Kopie (`DestroyCopyShopPoi`), beim Client auch dann, wenn der Host die Kopie
  ablehnt. Der Host hat seinen eigenen POI.

## Prüfliste ([ARCHITECTURE.md](../ARCHITECTURE.md#verbindliches-architekturprinzip-lokal-zuerst-minimal-synchronisieren))

- **Was bleibt lokal?** Panel, Auswahl, Preis, Scan-Animation und die lokale Kopie als Vorschau, wie beim Kauf. Der lokale Abzug ist nur Vorschau.
- **Was wird geteilt?** Der Edit `0x303` bekommt hinter den Käufen eine Liste von Kopien: Quelle des Originals, Preis, Endstand der Kopie.
  Neues Feld, darum `ModVersion` 18 ([Kontrollkanal](../CONTROL_PROTOCOL.md#kopierladen-mod-kompatibilität-18)).
- **Wer entscheidet?** Der Host. Er sucht das Original im Inventar des Clients, prüft Preis und Schrott, kopiert seine eigene Instanz mit
  `CreateItemInstanceCopy`, legt sie mit `AddToInventory` ab und bucht einmal.
- **Welcher Weg wird wiederverwendet?** Die Stations-Bearbeitung (`BeginPanelEdit`, `FinishStation`), Edit, `ApplyClientEdit`, `TradeBalance`,
  `Apply` und Zurückrollen wie beim Kauf beim Händler.
- **Ablehnung:** Der Client rollt zurück, die lokale Kopie verschwindet, das Original bleibt, den Schrott gleicht der nächste Stand des Hosts an.
- **Neuaufbau:** Die Kopie des Hosts repliziert wie jede Vergabe; nach der Übernahme wartet eine neue Bearbeitung, bis sie die lokale Instanz
  ersetzt hat (`ClientSettling`). Wiederbeitritt: Die Kopie liegt dann im Inventar des Hosts und kommt wie jedes andere Item zurück.

- [Belegt, Spiel] Mit Fix am 26.11.2026 auf einem PC, zwei Instanzen, Mod-Version 18, je Lauf neue Karte:
  - Übernahme: Client kopiert eine ungewöhnliche SMG aus dem Lager für 11. Client-Log „Station: Kopie beim Client“, „Kopie 1 lokal“,
    Edit mit 1 Kopie; Host-Log „Handel: Kopie 1 geprüft“, „1 von 1 kopiert, Schrott 200 zu 189“, übernommen, vollständig. `inventory.py`
    vollständig und gleich (zwei SMG im Lager), Schrott bei beiden 189 (aus dem Speicher gelesen).
  - Kaputtes Original: seltene, kaputte SMG aus dem Rucksack für 17. Kopie bei beiden im Lager, selten und kaputt, Original im Rucksack,
    Schrott bei beiden 183. Nach Wiederbeitritt (`Test-Rejoin.ps1`) und Rundenende im Lager beider Seiten dasselbe, Bild des Client-Lagers
    zeigt die Kopie. Das Ablegen der SMG vorher per `!shop place` lief ohne offenes Panel nativ (bekannte Grenze von `opt-station-zug`, Pivot
    beim Host 0,0); der Wiederbeitritt glich das an.
  - Ablehnung: 1,5 s Ping (`-NetProfile 1500,0,0`), direkt nach dem Kopieren `!scrap -195` beim Host. Host „Schrott 5 reicht nicht“,
    abgelehnt; Client „Ausgangsstand wiederhergestellt, vollständig=true“, `inventory.py` gleich ohne Kopie, Schrott bei beiden 5.
  - Nicht im Spiel gesehen: der Zweig „Kopie verworfen“ (Edit noch unbeantwortet), nur als Policy getestet; Aufwerten von Original und Kopie im
    selben Edit, nur als Plan getestet; zwei PCs.

## Hypothesen

- [Widerlegt] Die Original-SMG geht beim Kopieren verloren (siehe Belege).

## Nächster Schritt

Im nächsten Live-Test auf zwei PCs eine Kopie des Clients bestätigen (Logzeilen siehe Belege).

## Abnahme

Eine Kopie des Clients wird beim Host gebucht, das Item liegt bei beiden im Lager, der Schrott ist bei beiden gleich.
