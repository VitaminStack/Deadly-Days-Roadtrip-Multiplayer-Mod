# Architektur

Grundlage sind die Fakten aus [GAME_ANALYSIS.md](GAME_ANALYSIS.md). Dieses Dokument beschreibt den Plan, nicht den Ist-Stand:
Es existiert noch kein Mod-Code.

## Zielbild

- **2 Spieler** online. Mehr ist aufwendig, weil die Coop-Logik fest auf P1 und P2 ausgelegt ist.
- **Host-autoritativ:** Der Host berechnet das Spiel, der Client fragt Aktionen an und zeigt den Zustand an.
- **Eigene Kopie:** Jeder Spieler braucht das Spiel und dieselbe Mod-Version.
- **Verbindung über Steam:** Einladung über Freunde, keine Portfreigabe.

## Mod-Form

| Option | Einschätzung |
|---|---|
| **Native C++-DLL mit Dumper-7-SDK und Hooks** | **Gewählt.** Volle Kontrolle, passt zum vorhandenen `UE Live Object Explorer` |
| UE4SS | Keine Bestätigung für UE 5.8 gefunden. Später als Hilfswerkzeug prüfen |
| Pak-Mod mit eigenem UE-5.8-Projekt | Nur als Ergänzung für neue Assets, reicht nicht für Netzwerklogik |

Das Laden erfolgt im erprobten Beispiel über eine `version.dll`-Proxy-DLL neben
der Exe. Wichtig: vollständige Exportoberfläche, nicht nur die drei direkten
Imports. Der kleine Proxy-/ProcessEvent-End-to-End-Test auf Host und Client
ist erfolgt; die produktive Loader-/Core-Implementierung ist weiterhin separat.

### Technische Andockpunkte (Build 26187885)

Stand 02.11.2026, Aufgabe `andockpunkte`. Reproduzierbarer Versuch:
[Proxy- und Signatur-Spike](../research/spikes/andockpunkte/README.md).

**Laden:** Die Exe importiert `VERSION.dll` regulär (nicht nur verzögert):
`GetFileVersionInfoW`, `GetFileVersionInfoSizeW`, `VerQueryValueW`. Ein kleiner
Proxy mit genau diesen Exporten wurde in einem separaten Testprogramm implizit
geladen; alle drei Weiterleitungen lieferten gültige Versionsdaten. Das Original
muss über den absoluten, mit `GetSystemDirectoryW` bestimmten Systempfad geladen
werden. Nur der Basisname plus System32-Suchflag führte im Versuch zur Rekursion
in den Proxy. `DllMain` enthält keine Engine-Aufrufe und kein `LoadLibrary`.

`dwmapi.dll` und `dxgi.dll` sind ebenfalls reguläre Imports. In der untersuchten
Installation existiert bereits eine lokale `dwmapi.dll`: nicht überschreiben,
Ursprung unbekannt. `XINPUT1_4.dll` ist nur ein Delay-Import, deshalb kein
verlässlicher früher Bootstrap. Die Wahl von `version.dll` ist vorläufig:
Spiel-Laden, zusätzliche dynamisch abgefragte Exporte und andere Mods sind noch
nicht geprüft. Der Beispielproxy lädt absichtlich noch keine Mod.

**Hooks:** Für reflektierte Aufrufe ist `ProcessEvent(UObject*, UFunction*, void*)`
der vorhandene Weg (Windows-x64-ABI, RVA `0x160C040`, VTable-Slot `0x4A`).
`Widgets.cpp` zeigt den globalen Inline-Hook, `Trace.cpp` den Character-VTable-Hook.
Letzterer verändert die gemeinsam verwendete Klassentabelle, nicht nur eine
Instanz. Beide Wege sind im früheren Verbindungs-Spike erprobt, hier nicht erneut.
Direkte native C++-Aufrufe können `ProcessEvent` umgehen. Für Blueprint-Aufrufe
nativer Funktionen liefert `UFunction::Func` bei `+0xD8` einen separaten Exec-Thunk
mit `(UObject*, FFrame*, void*)`; das ist **nicht** die Signatur der nativen Methode.

Für die spätere Mod: eng nach Klasse/Funktion filtern, Original genau einmal
weiterleiten, Reentranz behandeln und Game-Thread-Zugriffe absichern. Parameter
nicht pauschal kopieren: Größe, Out-Parameter und Referenzen hängen von der
UFunction ab. Verzögerte UObject-Zeiger brauchen eine Lebensdauerprüfung. Der
derzeitige 8-Byte-Widget-Puffer ist kein allgemeiner Dispatcher. Unterbrechungsfreie
Hook-Installation, Trampolin-Relokation und Rückbau sind noch zu lösen.

**Signaturen:** Der Read-only-Audit findet `ProcessEvent`, `SetClientTravel` und
`SeamlessTravelToLevel` je einmal an den bekannten RVAs; RIP-relative Daten im
ProcessEvent-Muster sind maskiert. Nur ausführbare PE-Sektionen werden durchsucht.
Das ist ein Byte-Signatur-Test, keine neue Bestätigung ihrer nativen C++-Prototypen.
Unbekannten Build, keinen oder mehrere Treffer immer ablehnen; keinen beliebigen
ersten Treffer patchen. In-Memory-Prüfung bleibt nötig, weil andere Hooks die Bytes
ändern können. Die sechs geprüften Anfangsbytes im vorhandenen Widget-Hook sind
nicht mit einem eindeutigen Suchmuster gleichzusetzen.

**Nachgeholter End-to-End-Test:** Der Drei-Export-Proxy reichte im Spiel nicht;
beide Prozesse beendeten sich vor dem Fenster mit Code 0. Die Vollvariante mit
17 Weiterleitungen nach Name und Ordinal erreicht dagegen mit Host und Client
die Mission. Ein vom Attach unabhängig von API-Aufrufen gestarteter Worker
prüft die drei Live-Signaturen vor weiteren Spike-Patches. Über eine eigene
Fensternachricht läuft der kleine Character-ProcessEvent-Test: genau eine
Weiterleitung von `K2_GetActorLocation`, unveränderter Rückgabewert, Slot
wiederhergestellt. Die drei Signaturen sind im geladenen Code je eindeutig.

Der vollständig aufgelöste System-DLL-Pfad verhindert Selbstrekursion. Die
Vollvariante benutzt ABI-erhaltende x64-Tailcalls auch für undokumentierte Exporte,
statt deren Prototypen zu raten. Der Bootstrap wartet nie im Loader-Lock; die
Produktivlösung braucht weiterhin Lebensdauer-/Thread-/Hook-Verwaltung. Die Tests
liefen mit der vorgefundenen UE4SS-/Mod-Umgebung, nicht mit garantiert reinem
Stock-Spiel. Der genehmigte eigene Proxy-Link wurde nach dem Test entfernt.
Details, Fehlversuche und Belege stehen im [Nachtrag](../research/spikes/andockpunkte/README.md#nachtrag-vollstaendiger-exportproxy-und-end-to-end-test).

## Synchronisierung: zwei Wege

Die Entscheidung fällt an Gate G1 in [TODO.md](../TODO.md).

### Weg A: Native UE-Replikation (bevorzugt)

Der Host läuft als Listen-Server, der Client verbindet sich als normaler UE-Client. Die Mod schaltet Replikation frei und
schließt Lücken.

- **Dafür:** Die Exe enthält RPCs, `OnRep_*`-Funktionen, GAS und den `IpNetDriver`. Spieler, Gegner, Projektile, Waffen und Loot haben `bReplicates` gesetzt, auch in ihren Blueprints. Bewegung, Attribute und Fähigkeiten gäbe es fast geschenkt.
- **Dagegen:** Der Code ist vielleicht veraltet. Die Coop-Logik erwartet zwei lokale Spieler auf einem PC. Viele Gegner könnten das Netz überlasten. Die Blueprints enthalten fast keine eigenen RPCs. XP, Währungen und Reward-Ablauf replizieren nicht. Die Karte entsteht nicht repliziert, Objekte darauf teils schon.

### Weg B: Eigene Snapshot-Synchronisierung (Plan B)

Der Host simuliert ein echtes Local-Coop, in dem P2 die Eingaben des Clients erhält. Der Client simuliert nichts, sondern stellt
Spieler, Gegner und Projektile als Stellvertreter-Objekte dar. Der Zustand kommt als kompakte Snapshots über ein eigenes Protokoll.

- **Dafür:** Unabhängig vom Zustand des Netzwerkcodes im Spiel. Die vorhandene Coop-Logik bleibt beim Host unverändert gültig. Gegner lassen sich gebündelt übertragen.
- **Dagegen:** Deutlich mehr Eigenbau, jedes sichtbare System braucht eigenen Sync-Code. Die Inventar-UI beim Client ist besonders aufwendig.

### Hybrid

Beide Wege schließen sich nicht aus. Denkbar ist native Replikation als Grundlage, dazu eigener Sync-Code für das, was nicht repliziert:
XP, Währungen, Reward-Ablauf, Pausen und Effekte aus Blueprints. Gebündelte Snapshots kämen nur dazu, falls die Gegnermassen das Netz
überlasten. Nach der Laufzeitprüfung ist das der wahrscheinlichste Ausgang, mit Weg A als deutlich größerem Anteil. Die Modulgrenzen unten
lassen das zu.

## Blueprint-Spielwirkung und Darstellung trennen

Aufgabe `blueprint-logik`: Die [Stichprobe](../research/spikes/blueprint-logik/README.md)
unterscheidet native Spawner-Konfiguration, replizierte Actors, lokale HUD-Events
und gemischte Damage-/Reward-/Darstellungsgraphen. Ein Blueprint-Name oder RPC-Flag
ist kein Beweis fuer einen funktionierenden Netzwerkpfad.

Vorgeschlagene Regeln fuer die Umsetzung:

- Gegner, Schaden, Loot und Itemmodifikationen bleiben host-autoritativ. Client
  zeigt native replizierte Actors, startet keinen zweiten Spawner.
- Bestehende Multicasts zuerst pruefen, nicht durch doppelte eigene Events ueberlagern.
  Airdrop hat trotz RPC im Default keine Actor-Replikation; dessen persistenter
  Oeffnungs-/Reward-Zustand braucht einen bestaetigten eigenen oder nativen Weg.
- Explosion und Giftwolke nicht gleich behandeln: Explosion-Impact repliziert im
  Default nicht, Giftwolken-Impact dagegen schon. Fehlende Darstellung als reine
  VFX/Sound-Nachricht nachliefern, niemals einen Damage-Actor blind beim Client
  nachspawnen. Replizierte Wirkungs-Actors nicht duplizieren.
- Kurzlebige Kosmetik-Ereignisse von persistentem Offen-/Bereit-/Reward-Zustand
  trennen. Ereignisse deduplizieren; bei Join/Travel Zustand neu aufbauen.
- Client-HUD lokal aus replizierten und zusaetzlich synchronisierten Daten speisen,
  keine Host-Widgetgraphen oder Prozesszeiger serialisieren.

Diese Regeln sind ein Entwurf. Der Referenzscan rekonstruiert keine Guards oder
Reihenfolge und ersetzt keine Tests konkreter Events auf Host und Client.
## Pausen und blockierende UI (Untersuchung `pausen`)

SDK und begrenzte Laufzeitmessung zeigen zwei Ebenen: lokale Request-/UI-Sitzungen
und replizierte Welt-Properties. In einer Tutorialpause fehlte beim Client der
lokale Zeitrequest samt Coop-UI-Sitzung, die Welt war dort trotzdem pausiert.
Deshalb **nicht** pauschal einen zweiten Timescale-Sync über die native
WorldSettings-Replikation legen. Details und Fehlversuche:
[Pausen-Spike](../research/spikes/pausen/README.md).

**Vorgeschlagener Sync-Vertrag, noch nicht implementiert:** Der Host entscheidet
über globale Pausen und das Fortsetzen. Clients fragen UI-Aktionen an; der Host
liefert zusätzlich Sitzung-ID, Revision, Anlass, Bedienberechtigung und
Gameplay-Input-Sperre für die Anzeige und UI des Clients. Keine UObject-Zeiger,
Geräte-IDs oder lokalen SessionStack-Speicher kopieren. Eine lokale Menüanzeige
des Clients darf nicht eigenständig die globale Pause aufheben. Verschachtelte
Requests müssen bis zur Freigabe des letzten maßgeblichen Requesters erhalten
bleiben; Mapwechsel und Disconnect brauchen eine explizite Aufräumregel.

`bBlocksGameplay` und `PauseGameWhileOpen` getrennt behandeln. Level-up muss an den
autoritativen Reward-/Auswahlzustand gekoppelt werden, nicht nur an einen booleschen
Pausenwert. Ob die native Weltpause auch unter Last und bei allen UI-Übergängen
ausreicht, ist offen. Der direkt geladene, durch Tutorial blockierte Methodenversuch
belegt keinen vollständigen Inventar- oder Level-up-Ablauf.

## Transport

Die Exe enthält keinen Steam-NetDriver, aber den `IpNetDriver` und die Steamworks-Laufzeit. Geplant ist deshalb ein Tunnel:

```text
Client-Spiel                                         Host-Spiel
IpNetDriver ─ UDP ─ 127.0.0.1 ─ Tunnel ─ Steam P2P ─ Tunnel ─ 127.0.0.1 ─ UDP ─ IpNetDriver
```

Das Spiel glaubt, lokal per IP zu sprechen. Die Mod leitet die Pakete über die Steam-Networking-API weiter. Vorteil: kein eigener NetDriver,
keine Portfreigabe, und Steam-Relays verbergen die IP-Adressen. Das ist bisher nur ein Plan und wird in Spike E geprüft.

Für die frühen Spikes reicht eine direkte IP-Verbindung im LAN oder über Tailscale oder ZeroTier.

## Module

### Eigener Kontroll-Sync fuer nicht replizierte Spiellogik

Aufgabe `sync-bedarf`: [Sync-Anforderungsmatrix](SYNC_REQUIREMENTS.md) legt fuer
LevelSystem, Schrott, Reroll, beide Meta-Wallets und CoopInventoryCoordinator
einen Weg fest. Der Entwurf verwendet Host-Snapshots fuer Werte/Anzeige und
verlaessliche, validierte Anfragen fuer Kauf, Transfer, Claim und Reroll.
UI-Sitzung und Reward-Zustand werden zusaetzlich zur nativen Replikation abgebildet.
Das ist noch keine implementierte oder im Netz getestete Synchronisierung.

Client-Anzeige darf keine `AddExperience`-/`AddAmount`-Replays und keine normalen
Speicher-/Erfolgsroutinen ausloesen. Item-/Session-IDs, Run-Epoche, Revisionen und
Anfrage-Deduplizierung sind verbindliche Anforderungen an die Umsetzung.
Die Netzwerk-Transportbindung, XP-Einheit, gemeinsame Kontozuordnung und
persistente Mitspieler-Meta-Belohnung sind noch zu klaeren.

```text
Sync ──> Game ──> SDK (Dumper-7, nicht im Repo)
  │       │
  └──> Net│
  │       │
  └──> Core <── Loader
UI ──> Net, Core
```

| Modul | Aufgabe | Darf nicht |
|---|---|---|
| `Loader` | Proxy-DLL, startet die Mod | Spiellogik enthalten |
| `Core` | Logging, Config, Hooks, Game-Thread-Dispatcher, Versionsprüfung | Spielklassen kennen |
| `Net` | Transport, Lobby, Session, Handshake, Serialisierung | SDK einbinden |
| `Game` | Adapter auf Spielklassen und Hook-Definitionen | Netzwerklogik enthalten |
| `Sync` | Was wird wann synchronisiert | Direkt auf das SDK zugreifen |
| `UI` | Overlay für Lobby und Status | Spielzustand ändern, außer über `Sync` oder `Net` |

### Wichtigste Regeln

1. **Das SDK wird nur in `Game` eingebunden.** Bei einem Spiel-Update muss dann nur dieses Modul angepasst werden. Außerdem lassen sich `Net` und große Teile von `Sync` ohne Spiel testen und bauen.
2. **Unreal-Objekte nur auf dem Game Thread anfassen.** Netzwerkcode übergibt Daten über den Dispatcher aus `Core`.
3. **Unbekannter Spiel-Build bedeutet: Mod aus.** Lieber nicht aktiv als ein Absturz oder ein beschädigter Spielstand.
4. **Protokoll ist versioniert.** Unterschiedliche Versionen werden beim Handshake abgelehnt.

## Entscheidungen

Jede größere Technikentscheidung bekommt hier eine Zeile.

| Datum | Entscheidung | Begründung | Status |
|---|---|---|---|
| 02.11.2026 | Mod als native C++-DLL | Volle Kontrolle, UE4SS für 5.8 unbestätigt | Festgelegt |
| 02.11.2026 | SDK nur im Modul `Game` | Spiel-Updates bleiben beherrschbar | Festgelegt |
| 02.11.2026 | Weg A vor Weg B testen | RPCs sprechen dafür, ist deutlich weniger Arbeit | Vorläufig bis G1 |
| 02.11.2026 | Ziel sind 2 Spieler | Coop-Logik fest auf P1 und P2 | Festgelegt |
