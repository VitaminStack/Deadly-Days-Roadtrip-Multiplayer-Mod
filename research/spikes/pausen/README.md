# Pausen: SDK-Befunde und Host-/Client-Ablauftests

Build `26187885`. Kein fertiger Pause-Sync. Untersuchung mit selbst erzeugtem
Mission-SDK, zwei Instanzen auf einem PC und direkter Loopback-Verbindung.
Das Spiel wurde ausschliesslich mit `tools/spike/Start-Session.ps1` gestartet,
direkt in `DDDynamic_Map`, mit getrennten Testspielstaenden und Crash-Schutz.

## Was das SDK aussagt

- `UTimescaleManager` ist ein GameInstance-Subsystem. Es verwaltet Requests pro
  Requester, einen aktiven Requester und `OnGamePauseChanged`. `FTimeRequest`
  enthaelt Zeitfaktor, Prioritaet und `PauseRegardlessOfTimescale`.
- `UPauseScreenManager` verwaltet Oeffnungsanfragen und Blocker getrennt, hat
  einen eigenen `FTimeRequest` und einen Verweis auf den TimescaleManager.
- `UAbstractPanel` hat **getrennte** Schalter fuer Coop-UI-Sitzung,
  `bCoopBlocksGameplay` und `PauseGameWhileOpen`. Ein blockierendes Panel ist
  deshalb nicht schon nach dem Property-Namen als Timescale-Request nachgewiesen.
- `UCoopInputRouterSubsystem::SessionStack` speichert Sitzungen mit Session-/Parent-ID,
  UI-Autoritaet und `bBlocksGameplay`. UI-Autoritaet ist keine Netzwerkautoritaet.
- `ADDMissionGameState::SetFreezeTime` verwaltet noch eine weitere Request-Schicht.
  `UWorldSettings::TimeDilation`, `CinematicTimeDilation` und `PauserPlayerState`
  sind dagegen laut Engine-SDK Netzwerk-Properties. Die lokale Requestverwaltung
  darf nicht mit ausschliesslich lokaler Wirkung gleichgesetzt werden.
- Das native und Blueprint-Inventarpanel hatte im Default-Objekt
  `bUseCoopUISession=true`, `bCoopBlocksGameplay=true`, `PauseGameWhileOpen=false`.
  Das sind Default-Werte, kein Nachweis eines erfolgreichen Oeffnungspfads.

## Versuche und Ergebnis

1. **Read-only-Snapshot beider Prozesse:** Beim Host war bereits ein Tutorialdialog
   aktiv. Timescale-Requester: `DialogComponent`, eine belegte Request-Zelle, eine
   UI-Sitzung mit `bBlocksGameplay=true`. Client: kein Requester, keine belegte
   Request-Zelle, keine lokale UI-Sitzung. Auf **beiden** Seiten standen
   `TimeDilation` bei etwa `0.0001` und `PauserPlayerState` auf einem Spieler.
   Screenshot des Hosts bestaetigte den Tutorialdialog.
   Das belegt den Zustand, nicht den genauen verursachenden nativen Aufruf.
2. **Inventar-Methodentest:** Der separate Probe-Spike fand keinen geeigneten
   Inventar-Animationscontainer. `OpenPanel` wurde deshalb nicht ausgefuehrt.
   Kein daraus abgeleiteter Netzwerk- oder Pausenerfolg.
3. **Level-up-Methodentest:** `GameState->GetLevelSystem()->RequestLevelUp()` lief
   auf dem Fenster-/Game-Thread ohne Absturz durch. Die Snapshots blieben beim
   bestehenden Dialogrequest; kein neuer Sitzungseintrag und keine isolierte
   Level-up-Pause nachgewiesen. Der direkt geladene, bereits pausierte Testaufbau
   ist fuer den normalen XP-/Reward-Ablauf nicht ausreichend.

**Ergebnis:** Die Aussage „Timescale wirkt nur lokal“ ist nicht bestaetigt.
Die UI-Sitzungen kommen in diesem Test nicht beim Client an, waehrend der
Welt-Pausenzustand auf beiden Seiten vorhanden ist. Kein offener Inventar- oder
normaler Level-up-Ablauf wurde erfolgreich bis zum Schliessen durchgespielt.

## Werkzeuge und Wiederholung

`tools/spike/pause_state.py <host-pid> <client-pid>` liest nur Speicher. Die Ausgabe
ist nicht atomar; alte Objekte koennen noch im Objektarray liegen. Default-Werte
werden gesondert ausgewiesen, Archetypen werden aus den Instanzmessungen entfernt.
Rohdaten und Screenshots bleiben in `research/local/`.

`Build-Probe.ps1` baut `PauseProbe.cpp` gegen das **lokale** SDK nach
`research/local/pausen`. Nach dem Bauen eine eigene geschuetzte Testsitzung starten
und die Probe nur dort per `tools/spike/inject.py` laden. Die Probe liest
`<TEMP>/ddpause/<pid>.cmd`: `1` fordert ein Level-up an, `2` versucht das
Inventarpanel zu oeffnen, `3` es zu schliessen. Ueber eine eigene Fensternachricht
erfolgen die SDK-Aufrufe auf dem Fenster-/Game-Thread; die bestehende WndProc wird
weiter aufgerufen. Nur ein fester Kartenaufenthalt, keine Mapwechsel und kein
Hot-Unload waehrend des Tests. Zum Entfernen die **eigene** Sitzung mit
`Stop-Session.ps1` beenden. Kein Release-Loader und keine allgemeine Objektsuche.

Die Probe kann mangels Animation auf ein unpassendes altes Objekt stossen; dieser
Fall wird als Fehlversuch protokolliert. Sie umgeht natuerliche Eingaben und darf
nicht als Test der normalen Menue-/XP-Kette beschrieben werden.

## Urspruenglich fehlender Nachweis

Mit einer normal ueber das Menue gestarteten Mission und abgeschlossenem Tutorial
je einen echten XP-Level-up und eine Inventaroeffnung/-schliessung messen. Dabei
Requests, SessionStack, Input-Sperren, WorldSettings und Host-/Client-Fortschritt
vorher/waehrend/nachher vergleichen. Auch verschachtelte Fenster, beide Spieler,
Disconnect und Mapwechsel pruefen. Der damalige Stand war noch kein Abschluss.

## Nachtrag 03.11.2026: isolierte Ablaeufe

Zwei neue geschuetzte Sitzungen, Mission jeweils ueber MetaBase/Routenkarte statt
Direktladen. Tutorialdialoge wurden vor der Inventar-Messreihe auf beiden Seiten
geschlossen: kein aktiver Zeitrequest, keine UI-Sitzung, beide Weltzeitfaktoren 1.
Vorhandene UE4SS-/Mod-Installation unveraendert; keine reine Stock-Umgebung.

### Pausemenue und darin enthaltenes Inventar

Echter Eingabepfad: Escape, „Inventar ansehen“, „Zurueck“, „Fortsetzen“.
Keine erzwungenen `OpenPanel`-Aufrufe und kein Ein-/Auslagern von Items.

| Schritt | UI-Sitzungen auf oeffnender Seite | Aktiver Requester dort |
|---|---:|---|
| Laufende Mission | 0 | keiner |
| Pausemenue | 1, blockierend | PauseScreenManager |
| Inventar ansehen | 2, Kind verweist auf Elternsitzung | weiterhin PauseScreenManager |
| Zurueck zum Pausemenue | 1 | weiterhin PauseScreenManager |
| Fortsetzen | 0 | keiner |

Diese Folge wurde **beim Host und beim Client getrennt** gemessen und die
Inventaransicht jeweils im Screenshot bestaetigt. Auf der jeweils anderen Seite
entstehen dabei **keine** UI-Sitzungen und kein entsprechender lokaler Request.

- **Host oeffnet:** Beide WorldSettings gehen auf rund `0.0001`, beide haben einen
  `PauserPlayerState`. Nach Fortsetzen beide wieder Faktor 1 und kein Pauser.
- **Client oeffnet:** Client-Faktor rund `0.0001`, aber kein Pauser. Host-Faktor
  bleibt 1 und kein Pauser. Fortsetzen stellt Client-Faktor 1 wieder her.
- **Beide pausieren, Host setzt fort:** Beide haben zuerst einen lokalen Request
  und je eine UI-Sitzung. Nach Host-Fortsetzen hat der Host keine Sitzung mehr;
  der Client **behaelt** seine PauseScreenManager-Sitzung, bekommt aber Faktor 1
  und keinen Pauser. Lokaler UI-Wunsch und replizierter Weltzustand widersprechen
  sich hier. Das ist keine funktionierende gemeinsame Pausenabstimmung.

Die Inventaransicht selbst hatte weiterhin `PauseGameWhileOpen=false`. Ihre
blockierende Kind-Sitzung ist deshalb nicht als eigener Zeitrequest auszugeben.
`AnimationContainer` war an den gescannten AbstractPanel-Objekten oft null,
obwohl die UI sichtbar war. Der Snapshot listet jetzt zusaetzlich die echten
PanelAnimationContainer mit ihrem Open-Bit. Ein null Panel-Feld beweist nicht,
dass kein Inventar offen ist; die urspruengliche Probe hatte genau diese Grenze.

### XP und Level-up ohne erzwungenes RequestLevelUp

In Sitzung 2 wurde ueber `ExpSubsystem.RequestExpSpawning` ein Testpaket von
200 XP **am lokalen Host-Pawn erzeugt**. Die Aufnahme erfolgte im Spiel; kein
direktes Setzen des Levels und kein `AddExperience` fuer diesen Nachweis.
Das ist ein kontrollierter XP-Quelltest, kein Nachweis eines bestimmten Gegnertods.

- Vorher: Host Level 1, Fortschritt 0.10; Client Level 1, Fortschritt 0.
- Nach Aufnahme: Host Level 2, Fortschritt 0.547264; Client unveraendert Level 1.
- Vorher/nachher auf **beiden** Seiten: kein aktiver Request, keine UI-Sitzung,
  Faktor 1, kein Pauser. Screenshot bestaetigt Level 2 und laufende Mission.
- Damit **keine anhaltende Level-up-Pause/Upgradeauswahl in diesem Ablauf**.
  Ein kurzer Effekt zwischen den Snapshots ist damit nicht ausgeschlossen.
  Nicht ungeprueft eine aus anderen Spielen erwartete Level-up-Pause nachbauen.
- Ein clientseitiger XP-Spawnversuch aenderte in der Beobachtung keines der Level.
  Rueckkehr aus `RequestExpSpawning` allein beweist weder Spawn noch Aufnahme.
- Eine weitere Host-Anforderung blieb zunaechst an einem neuen Tutorialdialog
  haengen. Nach dessen Schliessen stieg der Host auf Level 3 / 0.392739. Diese
  ueberlagerte Messung ist **kein** isolierter Pausennachweis.

Die frueheren direkten Methodenversuche bleiben getrennt: Einzelschritte per
`AddExperience(1)` erhoehten den Host bis Level 2, `RequestLevelUp` danach auf 3.
Sie sind nicht der Beleg fuer den Aufnahmeweg und ersetzen keinen Reward-Test.

### Konsequenz fuer den Sync

1. Client-Pausenwuensche muessen zum Host; keine clientseitige globale
   Zeitentscheidung als Ersatz fuer einen bestaetigten Host-Pausenzustand.
2. Host verwaltet Gruende je Spieler/Session. Erst fortsetzen, wenn kein
   blockierender Grund mehr besteht; Kind schliessen darf Eltern nicht freigeben.
3. An den Client gehoeren bestaetigter Weltzustand **und** Session-ID, Parent-ID,
   UI-Autoritaet, blockierende Gruende und erlaubte Aktionen. Widgets bleiben lokal.
4. XP/Level getrennt synchronisieren; lokale XP-Erzeugung oder Host-Widgetaufrufe
   sind kein Transport. Persistente Fortschrittswerte brauchen einen Snapshot.
5. Session-Epochen, Disconnect-/Travel-Bereinigung und Reward-Bestaetigungen sind
   Tests der spaeteren Sync-Implementierung, nicht durch diese Analyse erledigt.

Damit sind die urspruenglichen Fragen zu Level-up, Inventar und Client-Bedarf am
kleinen Beispiel beantwortet. Kein Pause-Sync implementiert; keine Aussage ueber
alle Rewards, beliebige Mods, alle Level oder einen kompletten Run.

Rohdaten lokal: `isolated-{host,client}-{pause,inventory,return,resume}.jsonl`,
`isolated-running.jsonl`, `overlap-*.jsonl`, `pickup-{before,after}.jsonl` und
`host-probe-session{1,2}.log`. Keine persoenlichen Screenshots im Repo.
Die Probe versteht zusaetzlich `4` (ein XP per Methode), `5` (Level lesen) und
`6` (200 XP am lokalen Pawn anfordern). Befehle nur in der eigenen festen Karte;
vor Mapwechsel/Neubau Sitzung beenden. Beide Sitzungen wurden beendet.

## Umsetzung pausen-sync (08.11.2026)

Build `26495302`, ein PC, Lobby-Tunnel der Mod, Godmode, Tag 1 und 2 über die echte Route. Code in `src/Sync/Players` und
`src/Game/PauseAdapter.cpp`, Drahtformat `0x700` im [Kontrollkanal](../../../docs/CONTROL_PROTOCOL.md#pausemenü-mod-kompatibilität-9).

**Was es gibt und wie es wirkt:**

| Pause oder blockierende UI | Vorher | Jetzt |
|---|---|---|
| Pausemenü des Hosts, auch mit Inventaransicht | Weltpause repliziert nativ zum Client | unverändert, keine Nachricht |
| Pausemenü des Clients, auch mit Inventaransicht | nur der Client stand, der Host lief weiter | Host hält eine Zeitanfrage, Welt steht bei beiden |
| Airdrop-Auswahl | Koop-Reward-Sitzung des Hosts hält die Pause (`airdrop-warten`, `bestaetigen-alle`) | unverändert, das Panel des Clients zählt nicht als Pausemenü |
| Lager am Rundenende | Freeze aus `FinishMission`, Bestätigen wartet auf alle (`bestaetigen-rundenende`) | unverändert |
| Routenkarte, Reise | Zeitanfragen räumt die Reise ab (`TravelPatches`) | unverändert, Weltwechsel gibt eine gehaltene Client-Pause frei |
| Level-up | keine anhaltende Pause gemessen (oben) | nichts zu teilen |
| Tutorial-Dialoge | im Coop abgeschaltet (`TutorialAdapter`) | nichts zu teilen |

**Was lokal bleibt:** Pausemenü, Inventaransicht, Eingabe und Darstellung beim Client. **Geteilt** wird nur, ob das Pausemenü des
Clients offen ist (eigene Öffnungsanfrage im `PauseScreenManager` und `GetIsPaused`). Eine vom Host replizierte Weltpause setzt keine
Öffnungsanfrage, der Client hält deshalb keine Pause des Hosts fest. **Wer entscheidet:** der Host. Er ruft `RequestTimescale` mit den
Werten seines eigenen Pausemenüs (Faktor 0, Priorität 1) und seiner WorldSettings als Anfragendem, `RemoveRequest` beim Freigeben.
Fehlt etwas, pausiert er nicht. **Freigabe ohne Nachricht:** 3 s ohne Meldung, Weltwechsel, Trennung. Eine so beendete Sitzung
pausiert nicht wieder, erst ein neues Öffnen.

**Geprüft**, Belege in den Mod-Logs beider Seiten (`Pause:`-Zeilen mit Weltzeit und Pauser):

- Client öffnet: Host hält die Pause in unter 0,3 s, Weltzeit bei beiden `1e-04`, Bild des Hosts nach 5 s unverändert (Uhr steht).
  Client schließt: Freigabe beim Host, Weltzeit bei beiden wieder 1. Inventaransicht im Pausemenü und „Zurück“ halten die Pause, erst
  „Fortsetzen“ gibt frei.
- Host öffnet und schließt: Weltzeit und Pauser bei beiden gleich, wie vorher nativ.
- Überlappung: Ist die Welt pausiert, nimmt der jeweils andere kein Esc an, beim Client wie beim Host. Es gibt also immer genau einen
  Besitzer der Pause, wie im lokalen Koop. Der im Spike gemessene Widerspruch (Host setzt fort, Client behält seine Sitzung) tritt
  so nicht mehr auf.
- Client stürzt bei offenem Pausemenü ab (Instanz beendet): Host gibt nach 3 s per Zeitablauf frei, bevor der Transport die Trennung
  bemerkt. Danach Wiederbeitritt, Welt läuft.
- Gegenproben ohne Halte-Pause des Clients: Lager am Rundenende mit Bestätigen bei beiden, Reise zu Tag 2 (Weltzeit danach bei beiden 1),
  Pause in der neuen Welt in beide Richtungen, Airdrop über `DD_OpenAirdrop 1` mit Wahl und Bestätigen bei beiden (Welt steht während
  der Auswahl, läuft danach bei beiden). Keine Abstürze.

**Grenzen:** Lässt der Client sein Pausemenü offen, steht der Host, bis der Client fortsetzt oder sich trennt, wie im lokalen Koop. Der
Host kann diese Pause nicht selbst aufheben, sein Esc wirkt dann nicht. Nicht geprüft: zwei PCs über Internet, Latenzsimulation,
Pausemenü des Clients auf der Routenkarte oder in der MetaBase (dort ist ohnehin keine Mission aktiv).
