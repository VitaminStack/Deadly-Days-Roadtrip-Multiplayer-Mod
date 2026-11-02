# Sync-Bedarf: XP, Waehrungen, Rewards und UI

Aufgabe `sync-bedarf`, Build `26187885`. **Architekturentwurf, keine Implementierung.**
Grundlage: Mission-SDK aus der eigenen Spielkopie und die bereits dokumentierte
[Replikationsmessung](GAME_ANALYSIS.md#replikation-zur-laufzeit). SDK-Header zeigen
Schnittstellen und reflektierte Felder, nicht die gesamte native Implementierung.

## Befunde und festgelegter Weg pro Komponente

| Komponente | Beleg im SDK | Vorgeschlagener Sync-Weg | Trigger und Client-Verhalten |
|---|---|---|---|
| `LevelSystemComponent` | `AddExperience`, `RequestLevelUp`, `OnExperienceChanged`, `OnLevelChanged`, `GetCurrentLevel`, `GetExperiencePercentage`; Run-/Travel-Interfaces | Host-Snapshot fuer gemeinsame Run-XP/Level plus getrennten Reward-Zustand. Keine XP-Simulation beim Client | Delegates nach autoritativer Aenderung beobachten; Vollbild nach Join/Travel. Client aktualisiert nur ein Mod-/HUD-Anzeigemodell, kein `AddExperience`-Replay |
| `ScrapWalletComponent` | Erbt `AbstractRunPersistedCurrency`; GameState-Getter, `AbstractCurrency::GetAmount` und `OnCurrencyChanged` | Absoluter, revisionierter Run-Kontostand vom Host | Nach Einzahlung oder genehmigtem Kauf uebermitteln; Client fragt Kauf an, setzt den Betrag nicht selbst |
| `RerollCurrency` | Run-persistierte Currency plus `RerollCountsPerContext`, `PerformReroll`, `GetRerollCountForContext`, `OnRerolled` | Betrag **und** Kontextzaehler zusammen mit neuer Reward-Revision als Transaktion | Client fragt Reroll fuer aktuelle Sitzung an; Host prueft Kosten/Lock/Quote und erzeugt die Auswahl. Nie clientseitig neu wuerfeln |
| `MetaCurrencyWallet` | Erbt `AbstractCurrency`; `CurrencyAmount` und `PaidReceipts` sind SaveGame-Felder, SaveManager-/Save-Interface vorhanden | Host-Run-Anzeige als fluechtiger Spiegel, normale Client-Profilwaehrung nicht veraendern | Absolute Anzeige nach Host-Aenderung. Persistente Mitspieler-Belohnung wird nicht automatisch gutgeschrieben |
| `RareMetaCurrencyWallet` | Gleiche Currency-/Save-Basis wie MetaCurrency | Wie MetaCurrency, getrennte Waehrungsart und Revision | Keine Vermischung der beiden Konten, keine lokalen `AddAmount`-Aufrufe zur Anzeige |
| `CoopInventoryCoordinatorComponent` | `TransferItem`, `ClaimReward`, `RerollRewardSession`, `AcknowledgeRewardUIClosed`, `OnRewardSessionChanged`; `RewardSession`, `QueuedRewards`, `HeldItems`, `WorkbenchItems` | Verlaessliche Client-Anfragen, Host-Validierung und absolutes Sitzungs-/Inventar-Anzeigebild; native Item-Replikation separat erhalten | Nach Commit/Fehler Ergebnis mit Anfrage-ID senden. Aufloesung replizierter Item-Unterobjekte abwarten; unbekannte Items sperren statt dereferenzieren |

Die PlayerState-Getter fuer XP/Schrott/Reroll beweisen keinen getrennten Kontostand
je Spieler. Die entsprechenden GameState-Felder und Shared-Kontexthelfer sprechen
fuer gemeinsame Run-Daten. **Hypothese:** gemeinsame Run-Konten; vor Implementierung
mit zwei echten Spielern die Objektidentitaet und Wirkung der Getter bestaetigen.
Keine zwei unabhaengigen XP-/Waehrungssimulationen einfuehren.

## Was Thema 6 zusaetzlich braucht

- **XP:** Wert, Level und Fortschrittsanzeige nachliefern. GAS-Attribute mit
  `OnRep_Exp`/`OnRep_Level` sind nicht automatisch die nicht replizierte
  LevelSystem-/Reward-Steuerung. Die genaue Einheit von `OnExperienceChanged`
  (aktueller Balken oder kumuliert) ist noch nicht dynamisch bestaetigt; bis dahin
  keine erfundene rohe XP-Property aus SDK-Padding lesen. Prozentwert explizit als
  Anzeigewert kennzeichnen, nicht zur Berechnung von Upgrades benutzen.
- **Waehrungen:** absolute Betraege statt ungesicherter Add-/Subtract-Ereignisse;
  keine Speicherroutinen oder Erfolge durch das Anwenden von Anzeigedaten ausloesen.
- **Rewards:** `FCoopRewardSessionState` enthaelt Auswahl, berechtigte Spieler,
  Quoten, bereits beanspruchte Spieler, erforderliche Claims, Level, Reroll-Kontext,
  Lock und Queue-Zustand. Nicht nur ein boolesches „Reward offen“ senden.
- **Pausen:** Weltpause und UI-Sitzung getrennt betrachten. Engine-WorldSettings
  koennen bereits replizieren; zusaetzlicher Kontroll-Sync liefert Anlass,
  Revision, Bedienberechtigung und Input-Sperre. Keine zweite freie Client-Pause.
  Die Untersuchung in PR #4 ist ein separates, noch nicht gemergtes Teilergebnis.
- **UI:** Client zeigt denselben Host-Zustand mit erlaubten Aktionen. `AuthorityPlayerIndex`
  ist UI-Bedienrecht, nicht Netzwerkautoritaet. Eingabe geht als Anfrage an den Host,
  nie als selbst behaupteter Spielerindex oder fertiges Reward-Ergebnis.

## Kontrollkanal und Datenmodell

Vorgeschlagener, zu implementierender Vertrag, unabhaengig vom konkreten
Netzwerk-Backend:

1. Verlaesslicher Kontrollkanal mit authentifizierter Peer-/Spielerzuordnung. Die
   Transport-Aufgabe entscheidet ueber dessen konkrete Umsetzung; kein eigener
   UDP-Nebenkanal ohne Verlaesslichkeit als stillschweigende Annahme.
2. Jede Nachricht traegt Protokollversion, Run-ID, Welt-Epoche und Revision.
   Client-Anfragen zusaetzlich eindeutige Anfrage-ID und erwartete Zustandsrevision.
3. Keine Prozessadressen, lokale FName-Indizes, WeakObjectPtr-Indizes, TMap-/TSet-
   Rohdaten oder Save-Receipts uebertragen. Host vergebene Item-/Session-IDs
   verwenden; die Zuordnung zu replizierten Items ist vor Implementierung zu klaeren.
4. Der Host ordnet den Spieler aus der Verbindung zu und validiert Sitzung,
   berechtigten Spieler, Quote, Item-Auswahl, Besitzer, Zielplatz, Kosten und
   Reroll-Lock. Unbekannte/veraltete IDs ablehnen, nicht „best effort“ anwenden.
5. Ein Request wird auf dem Host-Game-Thread hoechstens einmal angewendet;
   Wiederholungen bekommen dasselbe gespeicherte Ergebnis. Reroll, Kontobelastung
   und neue Choice-Revision sind eine logische Transaktion.
6. Client-Spiegel sind revisionierte Anzeigedaten. Aeltere/doppelte Bilder verwerfen.
   Vollstaendigen Snapshot nach Join, Travel oder Reconnect verlangen; Events
   allein koennen verlorene oder zwischenzeitlich entstandene Zustaende nicht ersetzen.
7. Beim World-Travel alte UObjects und UI-Handles verwerfen. Epochengrenze verhindert,
   dass ein spaetes Claim aus Tag 1 ein Item an Tag 2 veraendert. Geschlossene oder
   abgebrochene Sitzungen brauchen einen expliziten Zustand, nicht nur das Ausbleiben
   weiterer Nachrichten.
8. Keine Hook-/Netzwerkarbeit auf SDK-Objekten ausserhalb des Game Threads.
   `Game` liest/ruft das SDK, `Sync` entscheidet und besitzt das Anzeigemodell,
   `Net` transportiert, `UI` zeigt an. Keine SDK-Includes in `Net` oder `Sync`.

## Persistenz und Konfliktgrenzen

Vorgeschlagener sicherer Anfang: Der Host besitzt Run, Auswahl und gemeinsame
Run-Konten. Der Client behaelt seine normalen Saves unveraendert; Mod-Spielstaende
sind getrennt. Ob ein Mitspieler spaeter Meta-Fortschritt erhaelt, ist eine eigene
Produktentscheidung aus Phase 5 und wird hier nicht als geloest abgehakt.

`AbstractCurrency::AddAmount` ist keine reine UI-Setter-Funktion. Save-/Delegate-
Seiteneffekte sind moeglich und bislang nicht voll untersucht. Deshalb native
Client-Wallets nicht mit einem Differenzbetrag „angleichen“. Bei Bedarf das
HUD-Getter-/ViewModel anbinden, nicht die Savedaten nachbauen.

Die Reparatur von `Slots`/`MyItems` gehoert zur reservierten Aufgabe `inventar`.
Der Sync-Entwurf heilt keine fehlenden Item-Unterobjekte. Auch nicht replizierte
RewardChest-Actors duerfen nicht als garantiert clientseitig vorhanden gelten.

## Beleg- und Testgrenze

- Gegen das lokal erzeugte SDK nachgeschlagen: alle sechs Komponenten,
  Currency-Vererbung, Delegates, Getter und Reward-Sitzungsfelder.
- Fruehere Replikationsbefunde wurden nicht als neuer Laufzeittest ausgegeben.
- Kein Transport, keine neuen RPCs, kein Save-Sync und kein funktionsfaehiger
  Transaktionsweg implementiert. Die genannten Hooks/Delegates sind Kandidaten;
  direkte native Aufrufe koennen ProcessEvent umgehen.
- Offen: XP-Einheit, Shared-/Player-Zuordnung in echten Runs, Item-ID-Aufloesung,
  native Kosten-/Quota-Semantik, Meta-Auszahlung und konkrete Transportbindung.

Abnahmetests der spaeteren Implementierung: gemeinsamer XP-Gewinn, beide HUDs,
kontrollierter Level-up, Kauf bei knappen Mitteln, gleichzeitige Claims,
doppelter/verspaeteter Request, Reroll gegen alte Revision, Join/Travel/Disconnect,
fehlendes Item-Unterobjekt und unveraenderte normale Saves. Das sind geplante,
noch nicht bestandene Tests.
