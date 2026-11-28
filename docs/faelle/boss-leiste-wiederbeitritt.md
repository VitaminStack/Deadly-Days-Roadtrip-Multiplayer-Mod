# boss-leiste-wiederbeitritt: Client stürzt beim Anlegen der Boss-Leiste nach mehreren Beitritten im selben Prozess ab

## Reproduktion

Ein PC, zwei Instanzen, Build `26786272`, beobachtet am 27.11.2026 abends (gefunden in `coop-modus-wahl`, PR #318):
`Start-Session.ps1 -Build -Mod -Tunnel Lobby -Brief`, dann Testkanal `travel /Game/Maps/DDDynamic_Map` ohne Testschalter, die Karte wurde `MGP_HQ8_Boss` (HQ-Endboss, 5 000 000 HP).
Danach `Test-Rejoin.ps1 -Rounds 2`, dann verließen Host und Client die Lobby, der Host erstellte mitten in der Mission eine neue Lobby, der Client trat im selben Prozess ein viertes Mal bei.
Gegenprobe ohne Absturz: 4 Wiederbeitritte mit `DDCOOP_TEST_MAP_PROFILE` = `MGP_MerchantEvent` (keine Boss-Karte).

## Belege

- [Belegt] Absturz des Clients: `EXCEPTION_ACCESS_VIOLATION` beim Lesen von `0x0`, Stack `+5aa0693`, `+5a814c9`, `+56c48e5`, `+1527c28`, `+160c55b` (`crashes.py`).
- [Belegt] Die letzte Zeile im Client-Log war „Boss-Leiste: Boss erkannt (Maximum 5000000), lege Objective an“, „lokal angelegt“ fehlt; der Absturz liegt also in `Create` (`src/Game/BossBarAdapter.cpp`).
- [Belegt] Im selben Prozess davor: Eintritt 2 legte die Leiste 7 s nach der Karte an, Eintritt 3 schon 30 ms nach „Client übernimmt“ während der Kartenerzeugung, je mit „Anzeigen 1“, ohne Absturz. Eintritt 4 stürzte 0,5 s nach „Client übernimmt“ ab.
- [Belegt] Die Rahmen gehören zu `DDObjectiveWidget::OnObjectiveStarted(Objective)`, also zu `NotifyWidgets` in `Create` (Disassembly `26786272`):
  `+56c4870` ist der Exec-Thunk mit einem Objektparameter ohne Rückgabe und ruft `+5a81480`.
  `+5a81480` schreibt das Argument nach `this+0x380` (`UDDObjectiveWidget.CurrentObjective`, OFFSETS.md) und bindet `Argument+0x28` (`UDDObjective.OnProgressChanged`), `this` ist also die Anzeige.
  Danach ruft es `+5aa0670`, das `this+0x338` ohne Prüfung liest und dereferenziert (`+5aa0693`). Dieses Feld war 0.
- [Belegt] `+1527c28` und `+160c55b` sind die allgemeinen `ProcessEvent`-Rahmen, die auch `boss-karte-absturz` zeigte; die Klassensuche aus #264 ist hier nicht beteiligt (dritter Rahmen anders).

## Hypothesen

- [Vermutung] `NotifyWidgets` nimmt jede `DDObjectiveWidget` mit gesetztem `ObjectiveManager` (`+0x378`), auch eine, deren Unteranzeige bei `+0x338` noch nicht oder nicht mehr gesetzt ist.
  Möglich: eine noch nicht aufgebaute Anzeige der neuen Welt (die HUD `WP_UI_C` wartet beim Client in `WidgetPatches` auf den GameState) oder eine übrig gebliebene der vorigen Welt.
  Prüfidee: zwischen den Wiederbeitritten alle `DDObjectiveWidget` mit `+0x338`, Manager, `CurrentObjective`, Outer und Welt aus dem Speicher lesen.
- [Vermutung] Fix: nur Anzeigen mit gesetzter Unteranzeige benachrichtigen (Feld per Reflexion), sonst warten wie bei „keine Anzeige mit Subsystem“.

## Nächster Schritt

Nachstellen: Boss-Karte ohne Testschalter (`MGP_HQ8_Boss`), `Test-Rejoin.ps1 -Rounds 5`, je Runde Anzeigen aus dem Speicher lesen und Boss-Zeilen des Clients zählen.

## Abnahme

`Test-Rejoin.ps1 -Rounds 5` auf der Boss-Karte ohne Absturz, der Client meldet je Eintritt genau einmal „Boss-Leiste: lokal angelegt“, auf einem PC mit zwei Instanzen.
