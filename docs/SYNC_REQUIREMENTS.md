# Sync-Bedarf: XP, Waehrungen, Rewards und UI

Aufgabe `sync-bedarf`, Build `26187885`. **Architekturentwurf, keine Implementierung.**
Grundlage: Mission-SDK aus der eigenen Spielkopie und die bereits dokumentierte
[Replikationsmessung](GAME_ANALYSIS.md#replikation-zur-laufzeit). SDK-Header zeigen
Schnittstellen und reflektierte Felder, nicht die gesamte native Implementierung.

## Befunde und festgelegter Weg pro Komponente

**Projektweite Präzisierung vom 05.11.2026:** [Lokal zuerst, minimal synchronisieren](ARCHITECTURE.md#projektweite-nutzervorgabe-vom-05112026).
„Client-Spiegel“ bedeutet bestätigte gemeinsame Daten, nicht ausschließliche Fernsteuerung seiner UI.
Native lokale UI-/Inventar-Vorprüfung, Layout, Vorschau und Kosmetik sollen weiterlaufen;
Vergabe, Verbrauch, Kosten und Effekte müssen genau einmal bestätigt werden. Abgeleitete lokale
Caches nach Übernahme neu aufbauen, nicht als Host-Speicher kopieren. Die Matrix unten bleibt
ein Vertrag für gemeinsame Wirkung, kein Verbot lokaler Berechnung. [Audit und Folgearbeit](CLIENT_LOCAL_AUDIT.md).

| Komponente | Beleg im SDK | Vorgeschlagener Sync-Weg | Trigger und Client-Verhalten |
|---|---|---|---|
| `LevelSystemComponent` | `AddExperience`, `RequestLevelUp`, `OnExperienceChanged`, `OnLevelChanged`, `GetCurrentLevel`, `GetExperiencePercentage`; Run-/Travel-Interfaces | Host-Snapshot fuer gemeinsame Run-XP/Level plus getrennten Reward-Zustand. Keine XP-Simulation beim Client | Delegates nach autoritativer Aenderung beobachten; Vollbild nach Join/Travel. Client aktualisiert nur ein Mod-/HUD-Anzeigemodell, kein `AddExperience`-Replay |
| `ScrapWalletComponent` | Erbt `AbstractRunPersistedCurrency`; GameState-Getter, `AbstractCurrency::GetAmount` und `OnCurrencyChanged` | Absoluter, revisionierter Run-Kontostand vom Host | Nach Einzahlung oder genehmigtem Kauf uebermitteln; Client fragt Kauf an, setzt den Betrag nicht selbst. **Stand 07.11.2026 (`lager-handel-client`):** Host schickt den Kontostand (`0x309`), der Client gleicht ihn außerhalb einer Bearbeitung an; Verkauf und Reparatur bucht nur der Host im Edit, siehe [Verkaufen und Reparieren](../src/Game/README.md#verkaufen-und-reparieren-beim-client) |
| `RerollCurrency` | Run-persistierte Currency plus `RerollCountsPerContext`, `PerformReroll`, `GetRerollCountForContext`, `OnRerolled` | Betrag **und** Kontextzaehler zusammen mit neuer Reward-Revision als Transaktion | Client fragt Reroll fuer aktuelle Sitzung an; Host prueft Kosten/Lock/Quote und erzeugt die Auswahl. Nie clientseitig neu wuerfeln |
| `MetaCurrencyWallet` | Erbt `AbstractCurrency`; `CurrencyAmount` und `PaidReceipts` sind SaveGame-Felder, SaveManager-/Save-Interface vorhanden | Host-Run-Anzeige als fluechtiger Spiegel, normale Client-Profilwaehrung nicht veraendern | Absolute Anzeige nach Host-Aenderung. Persistente Mitspieler-Belohnung wird nicht automatisch gutgeschrieben |
| `RareMetaCurrencyWallet` | Gleiche Currency-/Save-Basis wie MetaCurrency | Wie MetaCurrency, getrennte Waehrungsart und Revision | Keine Vermischung der beiden Konten, keine lokalen `AddAmount`-Aufrufe zur Anzeige |
| `CoopInventoryCoordinatorComponent` | `TransferItem`, `ClaimReward`, `RerollRewardSession`, `AcknowledgeRewardUIClosed`, `OnRewardSessionChanged`; `RewardSession`, `QueuedRewards`, `HeldItems`, `WorkbenchItems` | Verlaessliche Client-Anfragen, Host-Validierung und absolutes Sitzungs-/Inventar-Anzeigebild; native Item-Replikation separat erhalten | Nach Commit/Fehler Ergebnis mit Anfrage-ID senden. Aufloesung replizierter Item-Unterobjekte abwarten; unbekannte Items sperren statt dereferenzieren. **Stand 06.11.2026:** Weg verworfen, der Client bearbeitet lokal und schickt einen Edit, siehe [Inventar lokal bearbeiten](../src/Game/README.md#inventar-lokal-bearbeiten) |

Die PlayerState-Getter fuer XP/Schrott/Reroll beweisen keinen getrennten Kontostand
je Spieler. Die entsprechenden GameState-Felder und Shared-Kontexthelfer sprechen
fuer gemeinsame Run-Daten. **Hypothese:** gemeinsame Run-Konten; vor Implementierung
mit zwei echten Spielern die Objektidentitaet und Wirkung der Getter bestaetigen.
Keine zwei unabhaengigen XP-/Waehrungssimulationen einfuehren.

### Nutzervorgabe: gemeinsame Run-XP (04.11.2026)

Unabhaengig davon, welcher Spieler einen XP-Orb einsammelt, zaehlt der Gewinn
genau einmal zum gemeinsamen Run-XP-Fortschritt. Beide Spieler erhalten denselben
bestaetigten Fortschritt und sehen denselben XP-/Levelstand. Keine doppelte
Gutschrift bei gleichzeitiger Aufnahme, wiederholten Nachrichten oder erneut
abgespielten visuellen Effekten. Die Vergabe entscheidet der Host; der Client
uebernimmt den bestaetigten Zustand, ohne `AddExperience`/`ReceiveExp` lokal
noch einmal auszufuehren. Die technische Zuordnung der nativen Objekte bleibt zu
pruefen; die gewuenschte Mod-Regel ist damit entschieden. Daraus folgt keine
Entscheidung ueber identische persoenliche Reward-Auswahlen. Fuer Meta-Fortschritt
gilt die folgende Nutzervorgabe.

Abnahme fuer `fortschritt`: Aufnahme nur durch Host, nur durch Client sowie
gleichzeitig durch beide testen. Beispiel: ein Orb mit Wert 10 bedeutet einen
gemeinsamen Gewinn von 10, nicht 20; beide HUDs zeigen den neuen gemeinsamen
Stand. Doppelte/verspaetete Zustellung, Level-up, Reise und erneuter Beitritt
duerfen weder Gewinn wiederholen noch alte Anzeigen zurueckschreiben.

### Nutzervorgabe: Meta-Fortschritt des Hosts (06.11.2026)

Entscheidung des Nutzers fuer Phase 5 (`meta-fortschritt`): **Im gemeinsamen
Run gilt der Meta-Fortschritt des Hosts.** Seine Freischaltungen und dauerhaften
Meta-Verbesserungen bilden die Grundlage fuer beide Spieler; der persoenliche
Meta-Stand des Clients wird weder eingemischt noch zur Voraussetzung fuer den
Beitritt. Das ersetzt nicht die persoenliche Itemwahl und den gemeinsamen
Run-XP-Fortschritt.

- Der Host entscheidet ueber Meta-Kosten, Freischaltungen und Meta-Auszahlungen.
  Dauerhafter Fortschritt aus dem Coop-Run gehoert zum Mod-Spielstand des Hosts.
- Der Client nutzt den fuer die Sitzung bestaetigten Host-Stand. Er bekommt
  keine automatische Meta-Gutschrift und keine Uebernahme der Host-Freischaltungen
  in sein persoenliches Profil. Nach dem Verlassen gilt wieder sein eigener Stand.
- UI, Darstellung und aus bestaetigten Daten ableitbare Wirkungen laufen lokal.
  Zuerst native Replikation verwenden; nur nachgewiesene Luecken synchronisieren.
  Keine kompletten Save-Dateien kopieren und keine lokalen Wallet-Buchungen als
  Anzeigeabgleich ausfuehren.
- Normale Spielstaende bleiben getrennt (`mod-saves`). Die technische Umsetzung
  und die Pruefung von Join/Reconnect, Run-Ende, Niederlage, Reise und genau
  einmaliger Auszahlung liegen bei `fortschritt`, `run-ende` und `mod-saves`.

Damit ist die Produktentscheidung abgeschlossen, **nicht** ihre Implementierung
oder Spielabnahme. Besonders das bereits beobachtete lokale Speichern durch
`FinishMission` beim Client muss `mod-saves` absichern.

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
  Die Untersuchung steht im [Pausen-Spike](../research/spikes/pausen/README.md).
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

Verbindlicher Weg: Der Host besitzt Run, Auswahl und gemeinsame Run-Konten.
Es gilt sein Meta-Fortschritt, ohne dauerhafte Meta-Gutschrift beim Client, siehe
[Nutzervorgabe](#nutzervorgabe-meta-fortschritt-des-hosts-06112026). Der Client
behaelt seine normalen Saves unveraendert; Mod-Spielstaende sind getrennt.
Die Umsetzung dieser Speichergrenze ist weiterhin `mod-saves`, kein belegter Ist-Zustand.

`AbstractCurrency::AddAmount` ist keine reine UI-Setter-Funktion. Save-/Delegate-
Seiteneffekte sind moeglich und bislang nicht voll untersucht. Deshalb native
Client-Wallets nicht mit einem Differenzbetrag „angleichen“. Bei Bedarf das
HUD-Getter-/ViewModel anbinden, nicht die Savedaten nachbauen.

Die Reparatur von `Slots`/`MyItems` gehoert zur Aufgabe `tag3`, siehe [Teilfix](../research/spikes/a-connect/README.md#inventar-ursache-teilfix-und-verbleibende-grenze).
Der Sync-Entwurf heilt keine fehlenden Item-Unterobjekte. Auch nicht replizierte
RewardChest-Actors duerfen nicht als garantiert clientseitig vorhanden gelten.

## Beleg- und Testgrenze

**Umsetzungsnachtrag 05.11.2026:** `client-visuell` bindet inzwischen die
gemeinsame XP-/Levelanzeige über den [Kontrollkanal](CONTROL_PROTOCOL.md) an.
Host-Getter liefern absolute Anzeigen, der Client aktualisiert nur seine HUD.
Native Probe: Beide Host-PlayerStates teilen das GameState-Levelsystem; die
abweichende lokale Client-Instanz erklärt die bisher falsche Anzeige. Gegen
Spät-/Doppelzustellung schützen Peer, Client-Anforderungskennung, Run/Epoche,
Revision und GameState-NetGUID. Keine lokale `AddExperience`-/Reward-Ausführung.
Das implementiert **nicht** die komplette Level-up-/Reward-/Währungssteuerung
von `fortschritt`. Die folgenden Punkte beschreiben weiterhin den ursprünglichen
Analyseabschluss von `sync-bedarf`, nicht den aktuellen Umfang des Transports.

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
