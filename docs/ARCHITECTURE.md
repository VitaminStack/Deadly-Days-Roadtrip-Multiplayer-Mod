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

Das Laden erfolgt über eine Proxy-DLL neben der Exe. Welche DLL sich eignet, klärt Phase 2.

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
