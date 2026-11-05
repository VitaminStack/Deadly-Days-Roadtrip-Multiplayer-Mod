# Net

Verbindung zwischen den Spielern: Transport-Tunnel, Steam-Lobby, Session, Handshake und Serialisierung.
Bindet kein SDK ein und ist deshalb ohne Spiel testbar. Greift nie direkt auf Unreal-Objekte zu.

Einstieg ist `Net::Start` in `Net/Net.h`. Darf `Core` nutzen. Daten für den Game Thread gehen über `Core::GameThread().Post`.

## Bausteine

| Datei | Aufgabe |
|---|---|
| `SteamSockets.h` | Schmale Sicht auf `ISteamNetworkingSockets`. `LoadSteamSockets` nutzt die Steam-Laufzeit des Spiels, Tests eine nachgebaute |
| `Transport.h` | Tunnel zwischen dem UDP des Spiels und Steam Networking, Host und Client |
| `SteamLobbyApi.h` | Schmale Sicht auf Matchmaking, Friends und Utils für die Lobby. `LoadSteamLobbyApi` nutzt die Steam-Laufzeit des Spiels |
| `Session.h` | Ablauf einer Sitzung: Lobby erstellen, einladen, beitreten, Transport starten, Spiel verbinden |
| `Net.h` | `Start` und der laufende Transport |

## Transport (`transport`)

Das Spiel hat keinen Steam-NetDriver, aber den `IpNetDriver`. Der Tunnel lässt das Spiel weiter UDP auf 127.0.0.1 sprechen und
leitet jedes Paket unverändert über Steam Networking weiter. Keine Portfreigabe, Steam vermittelt und nutzt notfalls Relays.

```text
Client-Spiel ─ UDP 127.0.0.1:7778 ─ Transport ─ Steam P2P ─ Transport ─ UDP 127.0.0.1:7777 ─ Host-Spiel
```

- `StartHost` nimmt Verbindungen per P2P (virtueller Port 0) und über 127.0.0.1:7790 an. Je Client öffnet er einen eigenen
  UDP-Socket zum Spiel, das Spiel sieht also jeden Client unter einer eigenen Adresse.
- `StartClient(SteamID64)` verbindet per P2P zum Konto des Hosts, `StartLoopbackClient` über 127.0.0.1 zu einem Host auf demselben PC.
  Steam lehnt P2P zum eigenen Konto ab, darum braucht ein PC mit einem Konto den Weg über 127.0.0.1.
- Pakete gehen unzuverlässig ohne Nagle, die Zuverlässigkeit macht die Engine selbst. Pakete des Spiels vor dem Aufbau zählen als verworfen.
- `Stop` schließt Verbindungen und Listen-Sockets, gibt die UDP-Ports frei und darf mehrfach kommen. Danach ist ein neuer Start möglich.
- `Stats` liefert Zustand, Clients, Ping, Qualität, Pakete und Route (`Relay fra` oder `ohne Relay`) für Overlay und Fehlerbehandlung.
  Endet die Verbindung, meldet der Client `Failed` mit Grund, der Host wartet weiter auf Clients.
- Der Tunnel fasst keine Unreal-Objekte an. Listen-Server und `open 127.0.0.1:7778` veranlasst der Aufrufer.
- Logs ohne SteamIDs und IP-Adressen: Routen und Steam-Meldungen laufen durch `MaskAddresses`.

Die Steam-Funktionen kommen über die flache C-Schnittstelle aus der `steam_api64.dll` des Spiels (Steamworks 1.64,
`ISteamNetworkingSockets` v012), ein SDK ist nicht nötig. Herkunft und Messungen: [Steam-Tunnel](../../research/spikes/a-connect/README.md#spike-e-steam-tunnel-tunnel).

### Testschalter

Ohne Lobby lässt sich der Transport für Tests direkt über die Kommandozeile des Spiels starten:

| Schalter | Wirkung |
|---|---|
| `-ddcoop_transport=host` | Host-Tunnel |
| `-ddcoop_transport=loopback` | Client über 127.0.0.1, Host auf demselben PC |
| `-ddcoop_transport=<SteamID64>` | Client per P2P zum Konto des Hosts |

Der Testschalter ist ein Testmodus ohne Lobby: Der Host öffnet 127.0.0.1 und nimmt per P2P jedes von Steam bestätigte Konto an,
siehe [Zugriffskontrolle](#zugriffskontrolle-lobby-auth). Nie für öffentliche Sitzungen verwenden.

`Net::Start` wartet bis zu 120 s, bis das Spiel Steam initialisiert hat. Der Client versucht es 24-mal im Abstand von etwa 5 s,
weil beide Seiten im Test gleichzeitig starten. `Start-Session.ps1 -Mod -Tunnel Mod` setzt die Schalter und verbindet danach
das Client-Spiel mit `open 127.0.0.1:7778`, siehe [Testen](../../docs/TESTING.md).

### Netzsimulation (`latenz-test`)

Unreals `Net PktLag`/`PktLoss` fehlt im Shipping-Build: Die Exe enthält nur die Reflexionsnamen von `PacketSimulationSettings`,
nicht die Befehlstexte, die `DO_ENABLE_NET_TEST` mitbringt. Darum simuliert der Tunnel der Mod selbst ([NetSim.h](NetSim.h)).
Der Schalter `-ddcoop_netsim=<Ping>,<Schwankung>,<Verlust>` gilt für Lobby und Testschalter gleichermaßen:

- Jede Instanz verzögert ihre ausgehenden Spielpakete um den halben Ping ± die halbe Schwankung. Mit beiden Seiten ergibt sich der
  Ping des Profils. Spielpakete können sich dabei überholen und gehen je Richtung mit dem angegebenen Anteil verloren.
- Kontrollnachrichten werden nur verzögert und behalten ihre Reihenfolge. Das Hello bleibt unverzögert.
- Alle 30 s steht im Mod-Log `Netzsimulation: <n> Pakete verzögert, Mittel <ms> (<min> bis <max> ms), <k> verloren (<%>), Steam-Ping <ms>`.
  Der Steam-Ping enthält die Simulation nicht.

`Start-Session.ps1 -NetProfile Internet` setzt den Schalter, siehe [Latenz simulieren](../../docs/TESTING.md#latenz-simulieren).

## Steam-Lobby (`steam-lobby`)

Direkt über Steamworks, ohne `OnlineSubsystemSteam`: Das Spiel bringt keinen Steam-NetDriver mit, die Lobby braucht nur
Matchmaking und Friends aus derselben `steam_api64.dll` wie der Transport. Niemand tauscht SteamIDs von Hand aus.

```text
Host:   Erstellen -> Lobby nur für Freunde -> Transport-Host -> Spiel wird Listen-Server in der MetaBase -> Einladen
Client: Einladung annehmen oder Lobby-ID -> Lobby betreten -> Besitzer der Lobby -> Transport-Client -> open 127.0.0.1:7778
```

- `Session` kennt keine Unreal-Objekte. Was im Spiel passieren soll, kommt über `SessionHooks`; `src/Mod/Lobby.cpp` verdrahtet sie
  mit Konsolenbefehlen (`Game::ExecuteConsoleCommand`), dem Overlay (`UI::Overlay`) und dem Game-Thread-Takt.
- Die Lobby trägt `ddcoop=<Version>` in den Lobby-Daten. Ohne den Wert oder mit anderer Version bricht der Beitritt ab und verlässt die Lobby.
- Einladungen: „Einladen“ im Overlay öffnet die Freundesliste im Steam-Overlay. Nimmt ein Freund an, während das Spiel läuft,
  kommt `GameLobbyJoinRequested_t`; die laufende Sitzung endet, dann folgt der Beitritt. Bei geschlossenem Spiel startet Steam
  es mit `+connect_lobby <id>`, die Mod tritt bei, sobald die erste Welt geladen ist.
- Beitritt per Lobby-ID: Feld im Overlay. Der Host schreibt die ID ins Mod-Log, sie gilt nur, solange die Lobby lebt, und gehört
  zu keinem Konto. SteamIDs und Adressen stehen weiter nirgends im Log.
- Zwei Instanzen auf einem PC laufen unter einem Konto. Ist der Besitzer der Lobby das eigene Konto, nimmt der Client den Weg
  über 127.0.0.1, sonst P2P.
- Verlassen schließt Transport und Lobby, der Client trennt im Spiel mit `disconnect`. Endet der Tunnel des Clients, meldet die
  Sitzung `Failed` mit Grund und verlässt die Lobby. Danach sind Erstellen und Beitreten wieder möglich.
- Alle Aufrufe laufen auf dem Game Thread. Dort ruft das Spiel `SteamAPI_RunCallbacks` auf, dort kommen Callbacks und
  Ergebnisse von Steam an, und nur dort liefert die Steam-Laufzeit ihre Schnittstellen (siehe Transport).

| Schalter | Wirkung |
|---|---|
| `-ddcoop_lobby=create` | Lobby erstellen, sobald eine Welt geladen ist |
| `-ddcoop_lobby=<Lobby-ID>` | Der Lobby beitreten, sobald eine Welt geladen ist |
| `-ddcoop_lobby_delay=<s>` | Erstellen oder Beitreten frühestens so viele Sekunden nach dem Start, im Test bis der Spike geladen ist |

Mit `-ddcoop_transport=...` bleibt die Lobby aus, beide nutzen dieselben Ports. `Start-Session.ps1 -Mod -Tunnel Lobby` braucht die
Schalter nicht mehr: Es erstellt und betritt die Lobby nach dem Laden des Spikes über das Overlay, siehe [Testen](../../docs/TESTING.md).

### Lobby im Spiel (04.11.2026)

Build `26187885`, ein PC, ein Konto, `Start-Session.ps1 -Mod -Tunnel Lobby` mit Spike und CrashGuard, Release-Build der Mod.

| Schritt | Ergebnis |
|---|---|
| Lobby erstellen | Nach etwa 0,3 s erstellt, Host-Tunnel lauscht, `ExecuteConsoleCommand` macht den Host zum Listen-Server in der MetaBase |
| Beitritt per Lobby-ID, ohne Spike beim Beitritt | Lobby betreten, Besitzer ist das eigene Konto, Weg über 127.0.0.1, Tunnel nach 20 ms, Client lädt die MetaBase. Dann Absturz: erst Host `0x5901CE8`, im zweiten Lauf Client `0x56B16D9`. Beides sind bekannte Abstürze, die der Spike patcht, Übernahme in die Mod ist `patches` |
| Mit `-ddcoop_lobby_delay`, Spike vor dem Beitritt | Beide in der MetaBase mit 2 Spielern, ohne Spike-Befehl verbunden |
| `Start-Run.ps1 -Seed 20795`, `Travel-Day.ps1 62 213` | Beide in `DDDynamic_Map` mit 2 Spielern, Inventare vollständig und gleich, zurück auf der Routenkarte |
| Verkehr | rund 10 000 Pakete je Richtung, Ping 0 ms, Qualität 1,00, verworfen 0. Keine Fehler im Mod-Log |

Nicht geprüft, das übernimmt `lobby-live`: Einladung über das Steam-Overlay und `GameLobbyJoinRequested` (brauchen ein zweites Konto), Beitritt per P2P im
Spiel, Start über `+connect_lobby`, Verlassen und Abbruch im Spiel (nur in `SessionTests`). Ohne die Spike-Patches stürzte der
Beitritt im Spiel ab. Seit `patches` setzt die Mod diese Fixes selbst, ein Beitritt ohne Spike ist damit noch nicht geprüft.

### Lobby nur mit Server (`lobby-bereit`)

Der Host schließt seine Lobby selbst, sobald sein Spiel keinen Server mehr anbietet. Sonst laufen Beitritte ins Leere: Tunnel und
Handshake stehen, aber die Engine des Clients findet keinen Listen-Server.

- **Hook `GameHosting`:** Die Mod meldet `true` für einen Listen-Server, `false` nur im Hauptmenü, sonst unbekannt (Laden, Reise).
- **Hauptmenü:** War das Spiel schon Listen-Server und steht es 2 s im Hauptmenü (`HostGameLost`), endet die Sitzung mit „Lobby
  geschlossen: Das Spiel ist im Hauptmenü“. Der Tunnel schickt dabei Grund 1001, der Client meldet sofort „Der Host hat die Sitzung
  verlassen“.
- **Nie Listen-Server:** Wird das Spiel nach dem Erstellen binnen 60 s kein Listen-Server (`HostListen`), schließt die Lobby ebenso.
  Erstellen im Hauptmenü bleibt möglich, die Mod lädt dann die MetaBase als Listen-Server.
- **Geschlossene Lobby:** Ein Beitritt endet mit „Die Lobby gibt es nicht mehr, der Host hat sie geschlossen“ (Steam-Antwort 2).

Im Spiel (05.11.2026, ein PC, `Start-Session.ps1 -Build -Mod -Tunnel Lobby`): Host per Konsole `open /Game/Maps/MainMenu` aus der
MetaBase. Lobby nach 2,0 s geschlossen, Client 2 ms danach „Der Host hat die Sitzung verlassen“ statt nach 30 s „Verbindung verloren“.
Beitritt in die alte Lobby: sofort die neue Meldung in beiden Overlays. Neue Lobby im Hauptmenü erstellt: Host wird Listen-Server, die
Lobby bleibt offen, Client wieder in der MetaBase mit 2 Spielern. Kein Absturz.

### Zwei PCs, nur Mod (05.11.2026)

Build `26187885`, zwei PCs mit eigenen Steam-Konten, beide normal über Steam gestartet, nur `DDCoop.dll` aus `main` `64acf6f`
(kein Spike, kein CrashGuard). VitaminStack Host, Lobby über das Overlay, Beitritt per Lobby-ID. Gemessen auf der Client-Seite aus dem
Mod-Log; die Host-Seite wertet VitaminStack getrennt aus.

| Schritt | Ergebnis |
|---|---|
| Beitritt mit verschiedenen Mod-Ständen | Tunnel steht über das Relay `fra`, die erste Nachricht des Hosts wird sofort abgelehnt: „Ungueltige oder inkompatible Kontrollnachricht“, dreimal. **Vermutung:** Host noch mit Mod-Version 1, vor `desync-melder` (#98, Mod-Version 2). Nach dem Update beider auf denselben Stand bestätigt der Handshake. Seit `versions-meldung` nennt die Meldung beide Versionen |
| Beitritt, Host im Hauptmenü | Tunnel und Handshake stehen, die Engine meldet nie „mit dem Host verbunden“. Nach 18 s lädt der Client die MetaBase ohne Verbindung und fällt 2,6 s später ins Hauptmenü. Zweimal so. Seit `lobby-bereit` schließt der Host die Lobby im Hauptmenü, siehe [Lobby nur mit Server](#lobby-nur-mit-server-lobby-bereit) |
| Beitritt, Host in der MetaBase | Nach 0,4 bis 1,4 s verbunden, MetaBase mit 2 Spielern, gemeinsame Kamera, XP-Anzeige |
| Reise in die Mission | Ohne Absturz, XP-Bälle und XP-Anzeige kommen an. Ping 32 bis 36 ms, Qualität 1,00, verworfen 0. **Die Karten sind verschieden**, die Figuren stehen nicht zusammen: Ohne Spike überträgt noch niemand Karte und PCG des Hosts, das ist `missionsstart` (im Spike `karte-tunnel`). Seit `missionsstart` bringt die Mod Karte und PCG des Hosts über den Kontrollkanal, auf einem PC geprüft, siehe [Gleiche Karte in der Mod](../../docs/GAME_ANALYSIS.md#gleiche-karte-in-der-mod-missionsstart) |
| Host geht über das Spielmenü ins Hauptmenü | Client fällt sofort ins Hauptmenü, die Mod wartet 30 s und meldet dann „Verbindung zum Host verloren“ statt „Der Host hat die Sitzung verlassen“. Seit `lobby-bereit` nach rund 2 s mit Grund |
| Host „Verlassen“ im Overlay | Client meldet sofort „Der Host hat die Sitzung verlassen“ (Grund 1001) |

Kein Absturz auf beiden Seiten. Nicht geprüft: Einladung über das Steam-Overlay und `+connect_lobby` (bleibt `lobby-live`), eine gemeinsame
Mission mit gleicher Karte.

## Zugriffskontrolle (`lobby-auth`)

Der Versions-Handshake prüft nur, ob beide Seiten zusammenpassen (Build, Mod-Version, Rolle). Wer überhaupt verbinden darf,
entscheidet der Host vorher, noch vor `AcceptConnection`:

1. **Identität von Steam:** `SteamStatusChange::RemoteSteamId` kommt aus `SteamNetConnectionInfo::IdentityRemote`, nur bei Typ
   SteamID und einem einzelnen Konto im öffentlichen Universum (`IsIndividualSteamId`). Eine SteamID aus Paketen oder dem
   Handshake zählt nie. Ohne bestätigte Identität: abgelehnt.
2. **Lobby-Mitglied:** Die Sitzung liest jeden Tick die Mitglieder der eigenen Lobby (`SteamLobbyApi::Members`, über
   `GetLobbyMemberByIndex`) und gibt sie mit `Transport::SetLobbyMembers` an den Netzwerkthread. Ohne Lobby wird jede
   P2P-Verbindung abgelehnt, das eigene Konto ebenso.
3. **Kurz warten, nie freigeben:** Meldet Steam den Beitritt erst nach dem Verbindungsversuch, wartet die Verbindung bis zu
   `AuthorizeWaitMs` (5 s) unangenommen. Sie belegt keinen Platz und keinen UDP-Port. Höchstens vier warten gleichzeitig, ein
   Konto nur einmal; danach Ablehnung mit Grund 1005.
4. **Platz:** Erst ein berechtigtes Konto bekommt den einzigen Remote-Platz, ein zweites bekommt `Busy` (1004).
5. **Bindung an die Verbindung:** Die SteamID hängt am `Link`. Fehlt das Konto länger als `RevokeAfterMs` (10 s) in der
   Lobby, trennt der Host mit 1005. Jede neue Verbindung wird neu geprüft.

Der Client sieht bei 1005 „Der Host hat die Verbindung abgelehnt: nicht Mitglied seiner Lobby“. Logs nennen nur den Grund,
keine SteamID und keine Adresse.

**127.0.0.1 nur im Testmodus:** Dort prüft Steam keine Identität. Der Host lauscht darauf nur mit `-ddcoop_localtest`
(zwei Instanzen unter einem Konto, setzt `Start-Session.ps1 -Tunnel Lobby` ohne `-Role` selbst) oder mit dem Testschalter
`-ddcoop_transport`. Welcher Listen-Socket die Verbindung bekam, entscheidet, welcher Weg gilt.

| Schalter | Wirkung |
|---|---|
| `-ddcoop_localtest` | Host der Lobby nimmt zusätzlich 127.0.0.1 ohne Steam-Identität an, Warnung im Log |

Geprüft in `AuthorizationTests.cpp` und `SessionTests.cpp` mit nachgebautem Steam: Mitglied verbindet, Fremder, fehlende
Identität, keine Lobby, verspätete Mitgliedschaft, Fremder vor dem Freund, zweites Mitglied, Verlassen der Lobby, erneuter
Beitritt, falsche Mod-Version, 127.0.0.1 mit und ohne Testmodus, viele Fremdversuche. Im Spiel am 05.11.2026 mit
`Start-Session.ps1 -Mod -Tunnel Lobby -NoSpike`: Host meldet den Testmodus, nimmt den Client über 127.0.0.1 an, beide in der
MetaBase. Offen ist der Test mit getrennten Steam-Konten, siehe `lobby-auth-live`.

## Fehlerbehandlung (`net-fehler`)

Jedes Ende einer Sitzung landet als lesbare Meldung im Overlay (F8) und im Mod-Log, ohne IDs und Adressen. Danach sind
Erstellen und Beitreten wieder möglich, der Client trennt sein Spiel mit `disconnect`.

**Gründe beim Schließen.** Wer bewusst geht, schließt die Steam-Verbindung mit einem eigenen Grund aus `EndReason`
(Bereich `k_ESteamNetConnectionEnd_App_*`). Die Gegenseite sieht ihn als `ClosedByPeer` und kann unterscheiden:

| Ende | Client sieht | Host sieht |
|---|---|---|
| Host verlässt die Sitzung (`HostLeft`) | Der Host hat die Sitzung verlassen | – |
| Client verlässt die Sitzung (`ClientLeft`) | – | Der Mitspieler hat die Sitzung verlassen |
| Handshake abgelehnt (`Rejected`) | Der Host hat die Verbindung abgelehnt (andere Mod-Version oder anderer Spiel-Build) | Der Mitspieler hat die Verbindung abgelehnt |
| Host hat schon einen Mitspieler (`Busy`) | Der Host hat schon einen Mitspieler | – |
| Gegenseite schließt ohne eigenen Grund, etwa beim Beenden des Spiels | Der Host hat die Verbindung beendet | Der Mitspieler hat die Verbindung beendet |
| Steam gibt nach seinem Zeitlimit auf (`ProblemDetectedLocally`) | Verbindung zum Host verloren, vor dem Aufbau: Host nicht erreichbar | Verbindung zum Mitspieler verloren |

Der Host bleibt danach offen, das Overlay sagt bis zum nächsten Mitspieler, warum der letzte gegangen ist.

**Zeitlimits der Sitzung** (`SessionTimeouts`). Steam und die Engine haben eigene, diese greifen, wenn von dort nichts mehr kommt:

| Schritt | Zeitlimit | Meldung |
|---|---|---|
| Steam bestätigt Erstellen der Lobby nicht | 15 s | Zeitlimit: Steam hat die Lobby nicht erstellt |
| Steam bestätigt den Beitritt nicht | 15 s | Zeitlimit: Steam hat den Beitritt nicht bestätigt |
| Tunnel und Versions-Handshake zum Host | 20 s | Zeitlimit: keine Verbindung zum Host |
| Spiel lädt nach `open 127.0.0.1:7778` die Welt des Hosts | 90 s | Das Spiel konnte sich nicht mit dem Host verbinden |
| Spiel läuft ohne Host, obwohl der Tunnel steht | 30 s | Das Spiel hat die Verbindung zum Host verloren |

Die letzten beiden fragen über den Hook `GameConnectedToHost` die Rolle der Welt ab (`Game::ReadWorld`). Ohne lesbare Welt,
etwa beim Laden einer Karte, zählt das nicht als getrennt. 30 s sind großzügig, damit ein Kartenwechsel nicht auslöst.

### Im Spiel (05.11.2026)

Build `26187885`, ein PC, ein Konto, `Start-Session.ps1 -Build -Mod -Tunnel Lobby` mit Spike und CrashGuard, Release-Build der Mod.
„Verlassen“, „Lobby erstellen“ und „Beitreten“ im Overlay wurden per `WM_COMMAND` an das Overlay-Fenster ausgelöst, die Meldung
im Overlay alle 0,5 s ausgelesen. Zeiten ab der Aktion.

| Fall | Ort | Ergebnis |
|---|---|---|
| a) Host „Verlassen“ | MetaBase | Client: Grund 1001, nach 0,02 s „Der Host hat die Sitzung verlassen“, `disconnect`, nach 0,3 s im Hauptmenü |
| a) Host „Verlassen“ | `DDDynamic_Map`, Tag 1 | Gleich, nach 0,4 s im Hauptmenü, kein Absturz |
| b) Client „Verlassen“ | MetaBase | Host: Grund 1002, nach 0,3 s „Der Mitspieler hat die Sitzung verlassen. Lobby offen, Freunde über Einladen holen“. P2 nach 0,03 s aus der Welt (1 Spieler), die Engine entfernt ihn sofort, nicht erst nach ihrem Timeout |
| b) Client „Verlassen“ | `DDDynamic_Map` | Host wie oben. **Client stürzt einmal ab** beim `disconnect`: `LowLevelFatalError` in `ReferenceChainSearch.cpp` Zeile 2061, „Crashing due to uobject leaks (world leaks)“, Stack `15e8981 324c67f 325c726 324b595 32654ce 2a4a2b3 46859f4 …`. Im zweiten Versuch, ebenfalls nach angezeigten XP-Bällen, kein Absturz. Das Shipping-Spiel schreibt keine Referenzkette, siehe `opt-trennen-absturz` |
| c) Host-Prozess hart beendet | MetaBase | Client: Grund 5003 „Connection dropped“ nach 10,6 s, „Verbindung zum Host verloren“, `disconnect`, im Hauptmenü, Erstellen und Beitreten wieder frei |
| d) Client-Prozess hart beendet | MetaBase | Host: Grund 5003 nach 10,6 s, „Verbindung zum Mitspieler verloren. Lobby offen …“, P2 zugleich entfernt |
| e) Beitritt per Lobby-ID | MetaBase | Host „Lobby erstellen“ nach a), Client tritt per ID bei: nach 1,1 s beide „Im Spiel“, „Mitspieler verbunden“ |
| f) Kartenwechsel | MetaBase → `DDDynamic_Map` | `Start-Run.ps1 -Seed 20795`, `Travel-Day.ps1 62 213 -Stay`: keine falsche Meldung „Das Spiel hat die Verbindung zum Host verloren“, Inventare gleich |

Fehlschlag mit einem Konto: Nach b) lehnt Steam den erneuten Beitritt in dieselbe Lobby mit Antwort 2 ab (Lobby existiert
nicht), obwohl der Host noch „Lobby offen“ zeigt. Vermutlich verlässt mit dem Client das gemeinsame Konto die Lobby, und Steam
löst sie auf. Mit zwei Konten bleibt der Host Mitglied; das gehört zu `lobby-live`. Neuer Beitritt nach a) mit frisch erstellter
Lobby klappt.

Nicht geprüft: Zeitlimits Lobby, Connect und GameJoin im Spiel (nur in `SessionTests`), `Busy` mit einem zweiten Client,
Gegenprobe über das Internet (Live-Sitzung zu zweit).

## Tests

`tests/NetTests` prüft den Tunnel ohne Spiel und ohne Steam: Ein nachgebautes Steam Networking verbindet mehrere Konten im
Speicher, das UDP zum Spiel ist echt (Ports ab 47710). Geprüft sind Pakete in beide Richtungen, P2P und 127.0.0.1, abgelehnte
Verbindungen, belegter Port, Ende der Verbindung auf beiden Seiten, wiederholtes `Stop` mit Neustart, die Testschalter und das
Maskieren von Adressen. `SessionTests.cpp` prüft die Sitzung mit nachgebauten Lobbys und dem echten Transport: Erstellen,
Einladen, Beitritt per P2P und über 127.0.0.1, Verlassen von beiden Seiten, fremde Lobby, andere Version, Host antwortet nicht,
abgelehntes Erstellen, angenommene Einladung während einer Sitzung, `+connect_lobby`. Läuft über `Build-Mod.ps1 -Test` und in der CI.

### Im Spiel (04.11.2026)

Build `26187885`, ein PC, ein Konto, `Start-Session.ps1 -Mod -Tunnel Mod` mit Spike und CrashGuard, Release-Build der Mod.

| Schritt | Ergebnis |
|---|---|
| Erster Versuch | Kein Steam: `SteamAPI_SteamNetworkingSockets_SteamAPI_v012` lieferte vom eigenen Thread des Transports 120 s lang `nullptr` |
| Schnittstelle auf dem Game Thread geholt | Sofort da. Host lauscht per P2P und über 127.0.0.1, Client verbindet über 127.0.0.1, Route `ohne Relay` |
| MetaBase über den Tunnel der Mod | Host `clients: 1`, beide 2 Spieler |
| `Start-Run.ps1 -Seed 20795`, `Travel-Day.ps1 62 213` | Beide in `DDDynamic_Map` mit 2 Spielern, Inventare vollständig und gleich, Tag beendet, zurück auf der Routenkarte |
| Verkehr | rund 15 000 und 16 000 Pakete je Richtung, Ping 0 ms, Qualität 1,00, verworfen 0. Keine Fehler im Mod-Log |

Nicht geprüft: P2P mit dem Transport der Mod auf zwei PCs (der Spike-Tunnel ist dort belegt, siehe `tunnel-p2p`), Abbruch und
Neuaufbau im Spiel, mehr als ein Client. Das kommt mit `steam-lobby` und `net-fehler`.
