# Aufgaben-Details

Vollständige Beschreibungen zu Aufgaben in [TODO.md](../TODO.md), dort steht je Aufgabe nur eine Kurzfassung. Wer eine davon nimmt, liest hier nur ihren Abschnitt.

## `weekly-sperre`

**Bestätigungstest ausstehend:** Umsetzung mit #188 gemergt: ddrweekly.deadlydays.com in Core::ReportHosts gesperrt, Unit-Tests bestehen. Offen: Solo-Spieltest mit Mod, Weekly-Eintrag im Hauptmenü anklicken, Menü bleibt bedienbar, Log zeigt keine Auflösung; ohne Mod unverändert **Aufgabe und Abnahme:** Nutzerentscheidung vom 09.11.2026: optional, keine Release-Voraussetzung mehr. Die Weekly-Challenge sperren, solange die Mod geladen ist, auch solo, wie Erfolge und Bestenlisten (Entscheidung `weeklymode`). `ddrweekly.deadlydays.com` in `Core::ReportHosts` aufnehmen und vorher prüfen, wie das Menü reagiert: Hängt oder stürzt der Weekly-Eintrag ohne Antwort, ihn sperren oder mit Hinweis versehen. **Fertig, wenn** mit Mod nichts an den Weekly-Server geht, das Menü normal bedienbar bleibt und ohne Mod alles unverändert ist. **Einstieg:** [Telemetrie und Berichte](archive/GAME_ANALYSIS.md#telemetrie-und-berichte-telemetrie-aus). Ordner `src/Core`, `src/Game`

## `opt-waffen-hover`

Nutzerentscheidung vom 11.11.2026: vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Beim Client hebt das Inventar beim Überfahren einer Waffe mit der Maus die Items nicht hervor, die auf die Waffe wirken; beim Host schon (Ein-PC-Test 09.11.2026, Bildvergleich). Lokal zuerst: die native Hervorhebung beim Client aus den replizierten Inventardaten anzeigen, keine neue Nachricht. **Fertig, wenn** der Client beim Überfahren dieselben Items hervorgehoben sieht wie der Host. Ordner `src/Game`

## `opt-haende-vorschau-items`

Gefunden bei `client-roboterarm` (21.11.2026): Hantel (`ItemData_Dumbell`), Hand des Blutes (`ItemData_HandOfBlood`) und Plasmahand (`ItemData_PlasmaHand`) senken über Standardwirkungen die Hände, die eine Waffe braucht. Der Client legt Wirkungen während einer lokalen Bearbeitung nicht an; vermutlich lässt sein Panel darum eine Waffe nicht zu, die erst mit einem dieser Items in derselben Bearbeitung passt (nicht gemessen, die Hantel passte im Test nicht in den Rucksack). **Fertig, wenn** gemessen ist, ob `GetHandsRequiredForItem` beim Client in der offenen Bearbeitung dem Wert des Hosts nach dem Übernehmen folgt, und eine Abweichung behoben ist. Ordner `src/Game`. Einstieg: [Roboterarm beim Client](archive/GAME_ANALYSIS.md#roboterarm-beim-client-client-roboterarm), Testweg `!shop place`

## `opt-client-takt`

Nutzerentscheidung vom 11.11.2026: vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Rest aus `host-lagspike` (09.11.2026): Beim Client halten die Widget-Suche der Fortschritts-Anzeige (`ShowSharedProgress`, rund 12 ms) und die Objektsuche der Boss-Leiste (`BossBarAdapter::ScanObjects`, rund 10 ms) einmal pro Sekunde den Frame auf, Log `Takt: …`. Im Spiel nicht mehr spürbar gemeldet. **Fertig, wenn** beim Client kein Sekunden-Abgleich mehr über 8 ms im Log steht

## `opt-tempo-upgrades`

**Bestätigungstest ausstehend:** Der Client übernimmt `MaxWalkSpeed` und `MaxAcceleration` (3,5-faches Tempo) aus dem replizierten Attribut, für eigene und fremde Figuren. Gemessen auf einem PC mit `tools/spike/movement_speed.py`, mit gesetztem Attribut (`DD_SetAttribute`) und mit Item-Effekt (`ItemData_HeavyBoots`), mit einem und mit drei Clients. Offen: ein echtes Tempo-Upgrade (Meta oder Level-up) im Run und ein Test auf zwei PCs. **Aufgabe und Abnahme:** Tempo-Upgrades (`MovementSpeed`, Beschleunigung) kommen beim Client nicht an (Host 819 und 2866, Client 600 und 2048). **Fertig, wenn** beide Werte beim Client nach jedem Upgrade denen des Hosts entsprechen.

## `opt-gegner-spawnverzug`

Unter Wellenlast (`!wave`) erscheinen neue Gegner beim Client bis rund 8 s später als beim Host. **Fertig, wenn** neue Gegner beim Client höchstens 1 s nach dem Host erscheinen, auch unter Wellenlast.

## `opt-blut`

**Bestätigungstest ausstehend:** Live-Test 13.11.2026 abends, zwei PCs: Tod-Effekte beim Client vollständig (356 von 356, 364 von 364), kein Absturz. Offen: Treffereffekte (HitVfx), brauchen das Treffer-Signal von opt-schadenszahlen. [Live-Test 13.11. abends](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-13112026-abends-livetest-1311-abend) **Aufgabe und Abnahme:** Nutzerentscheidung vom 11.11.2026: vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Blut- und Treffereffekte erscheinen nur beim Host

## `opt-dash-aufprall`

Nutzerentscheidung vom 11.11.2026: vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Prallt der Client mit dem Dash gegen ein Objekt, hört er kein Geräusch (Live-Spiel 07.11.2026). Seit `ausweichen-client` führt der Host den Dodge des Clients aus, vermutlich laufen Aufprall und Sound nur dort. Mit `vfx-sound` abstimmen. **Fertig, wenn** der Client beim Aufprall seines Dashes denselben Sound hört wie der Host bei seinem eigenen, ohne doppelten Sound. Ordner `src/Game`. **Einstieg:** [Client-Darstellung 07.11.](archive/GAME_ANALYSIS.md#client-darstellung-im-live-spiel-am-07112026-todo-client-optik)

## `opt-ergebnis-werte`

Nutzerentscheidung vom 11.11.2026: vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Der Ergebnisbildschirm des Clients am Run-Ende zeigt nur Nullen, die Run-Statistik des Hosts wird nicht repliziert (`run-ende`, 10.11.2026). **Fertig, wenn** der Client dieselbe Wertung sieht wie der Host, ohne dass sich sein Meta-Stand ändert. Ordner `src/Game`. **Einstieg:** [Run-Ende](archive/GAME_ANALYSIS.md#run-ende-bei-host-und-client-run-ende)

## `opt-rundenstempel`

Nutzerentscheidung vom 11.11.2026: vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Die Rundenübersicht des Clients stempelt das geschaffte Level nicht ab (Live-Test 10.11.2026), vermutlich wie `opt-routenkarte` mangels Run-Info des Hosts. Mit `opt-routenkarte` zusammen lösen. **Fertig, wenn** der Client am Rundenende denselben Stempel sieht wie der Host. Ordner `src/Game`. **Einstieg:** [Live-Test 10.11.](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-10112026-livetest-1011)

## `opt-werkbank-station`

**Bestätigungstest ausstehend:** Live-Test 24.11.2026, zwei PCs: Zusammenlegen im Lager übernommen, ein Rezept (Molotov) lehnt der Host ab (passt zu keinem der 20 Rezepte) und verwirft damit den ganzen Edit. Offen: Rezept-Aufwertung des Clients beim Host gebucht, [Live-Test 24.11.](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-24112026-livetest-2411) **Aufgabe und Abnahme:** Gefunden im Live-Test am 13.11.2026 (zwei PCs, Build `26495302`): Die Werkbank im Händler-Event wertet beim Client nur lokal auf (Client-Log: „Aufwertung beim Client außerhalb einer Bearbeitung … läuft nativ“), beim Host kommt nichts an. Aufwerten durch Ablegen im Panel geht (`werkbank-client`). **Fertig, wenn** eine Aufwertung des Clients an der Werkbank beim Host gebucht wird und beide Seiten dasselbe Item zeigen. Ordner `src/Game`. **Einstieg:** [Live-Test 13.11.](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-13112026-livetest-1311)

## `opt-airdrop-edit-ungueltig`

**Bestätigungstest ausstehend:** Klon-Schutz aus #230/#246: eine als Aufwertungseingabe verbrauchte Belohnung gilt als gewählt. Live-Test 17.11.2026, Build 26669225: zehnmal Belohnung als Eingabe einer Aufwertung, jedes Mal ohne Rücknahme (airdrop-belohnung-aufwertung). Offen: im Client-Log oder Bild prüfen, dass kein Klon der aufgewerteten Eingabe sichtbar wird ('erneut aufgenommen, wieder entfernt' tritt nicht auf) **Aufgabe und Abnahme:** Gefunden im Live-Test am 13.11.2026 (zwei PCs, Build `26495302`): Zweimal nahm der Client eine Auswahl mit vorheriger Aufwertung zurück („Edit mit 1 Aufwertungen ungültig, Bearbeitung zurückgenommen“). **Fertig, wenn** die Ursache belegt ist und ein solcher Edit übernommen oder dem Spieler verständlich abgelehnt wird. Ordner `src/Game`. **Einstieg:** [Live-Test 13.11.](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-13112026-livetest-1311) **Letzter Bestätigungstest:** Live-Test 13.11.2026 abends nicht bestanden: Klon der aufgewerteten Eingabe (Luftpumpe) war beim Client rund 2,5 min sichtbar und verschwand erst beim Bestätigen ('erneut aufgenommen, wieder entfernt', Edit übernommen). Nutzerentscheidung: Der Klon darf gar nicht erst entstehen; verbrauchte Eingaben schon beim Aufzeichnen der Aufwertung (RecordUpgrade) lokal aus Inventar und Belohnungsanzeige nehmen, Solo-Verhalten als Referenz. Vier weitere Aufwertungen ohne erneutes Aufnehmen liefen sauber. [Live-Test 13.11. abends](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-13112026-abends-livetest-1311-abend)

## `opt-airdrop-uebertrag`

**Bestätigungstest ausstehend:** Live-Test 17.11.2026: nach liegen gebliebener Kiste am Rundenende kamen im nächsten Level wieder Airdrops und ließen sich öffnen. Offen: Host-Queue im Host-Log gegenprüfen, [Live-Test 17.11.](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-17112026-livetest-1711) **Aufgabe und Abnahme:** Gefunden im Live-Test am 13.11.2026 (zwei PCs, Build `26495302`): Bleibt ein Airdrop in einem Level liegen, erscheint im nächsten Level keiner mehr (gemeldet). Vermutung: Die Reward-Queue bleibt auf der alten Kiste stehen. **Fertig, wenn** nach einem liegen gelassenen Airdrop im nächsten Level wieder Airdrops kommen und sich öffnen lassen. Ordner `src/Game`. **Einstieg:** [Live-Test 13.11.](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-13112026-livetest-1311)

## `opt-highway-ziel`

Gefunden im Live-Test am 13.11.2026 (zwei PCs, Build `26495302`): Im Event „Räume den Highway!“ erschien kein Benzin, das Ziel verlangte einen Abschluss (2/10). Erst ohne Mod klären, wie das Event endet. **Fertig, wenn** belegt ist, ob das natives Verhalten ist, und das Ziel bei Host und Client gleich zählt und die Runde regulär endet. Ordner `src/Game`. **Einstieg:** [Live-Test 13.11.](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-13112026-livetest-1311)

## `opt-schuss-animation`

Gefunden im Live-Test am 13.11.2026 (zwei PCs, Build `26495302`): In höheren Leveln ist die Schuss-Animation der Client-Figur beim Client kaputt (Waffen übereinander), und die Figur gleitet über den Boden. **Fertig, wenn** die Client-Figur mit mehreren Waffen wie beim Host schießt und läuft. Ordner `src/Game`. **Einstieg:** [Live-Test 13.11.](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-13112026-livetest-1311) **Letzter Bestätigungstest:** Live-Test 13.11.2026 abends nicht bestanden: Die Client-Figur gleitet beim Client weiter ohne richtige Laufanimation; hands.py nicht gelaufen. [Live-Test 13.11. abends](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-13112026-abends-livetest-1311-abend)

## `opt-zonenziel-anzeige`

Gefunden im Live-Test am 13.11.2026 abends (zwei PCs, Build `26495302`): In der Zone „Töte die Titanen“ sieht der Client die Fortschrittsanzeige des Zonenziels nicht. Lokal zuerst: die native Anzeige aus replizierten Daten befüllen. **Fertig, wenn** der Client den Fortschritt des Zonenziels wie der Host sieht. Ordner `src/Game`. **Einstieg:** [Live-Test 13.11. abends](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-13112026-abends-livetest-1311-abend)

## `opt-itemsack-flug`

Gefunden im Live-Test am 13.11.2026 abends (zwei PCs, Build `26495302`): Itemsäcke aus Containern werden beim Client erst direkt an der Figur sichtbar, der Ausstoß fehlt. **Fertig, wenn** der Client die Säcke wie der Host aus dem Container fliegen sieht. Ordner `src/Game`. **Einstieg:** [Live-Test 13.11. abends](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-13112026-abends-livetest-1311-abend)

## `opt-boss-phasenleiste`

**Bestätigungstest ausstehend:** Live-Test 24.11.2026, zwei PCs: nur der Lazy-Boss, nicht phasenbezogen, Leiste korrekt. Offen wie bisher: phasenbezogene Bosse zu zweit, Beitritt mitten im Kampf, BlobBall, HQ-Endboss, [Live-Test 24.11.](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-24112026-livetest-2411) **Aufgabe und Abnahme:** Gefunden am 18.11.2026 in `opt-endboss-leiste` (ein PC, beide Builds): Der Host zeigt bei Ballerina, Frankenstein, BlobBall und HQ-Endboss in der ersten Phase nur die HP über der Phasenmarke (etwa 47.363/150.000), der Client die vollen HP (197.363/300.000); beim SpikeBoss zeigen beide die vollen HP. Außerdem erscheint die Leiste beim Client schon vor dem Kampfstart. Vermutung: Der Host rechnet über die `BossComponent` mit Zustand, den nur er hat. **Fertig, wenn** Zahl und Nenner der Client-Leiste in jeder Phase und der Zeitpunkt des Erscheinens dem Host entsprechen, ohne eigene Nachricht, sofern nicht belegt nötig. Ordner `src/Game`. **Einstieg:** [Boss-Leiste aller Bosse](archive/GAME_ANALYSIS.md#boss-leiste-aller-bosse-beim-client-opt-endboss-leiste)

## `opt-boss-tafel-client`

**Bestätigungstest ausstehend:** Live-Test 24.11.2026, zwei PCs, echte Route: Hinweis beim Client, solange der Host nach dem Boss pausiert, weg nach dem Fortsetzen. Offen: eigenes Pausemenü des Clients während einer Host-Pause, Vollbild, [Live-Test 24.11.](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-24112026-livetest-2411) **Aufgabe und Abnahme:** Gefunden am 19.11.2026 in `boss-client-einfrieren` (ein PC, beide Builds): Nach dem Bosstod zeigt nur der Host die Abschlusstafel „Quest abgeschlossen“ (`WP_EndGameUnlockSummaryPanel_C`, Bombenteil, Vergeltungsstufe, Geländeart). Ihre Pause repliziert zum Client, der ohne Tafel und ohne Hinweis steht, bis der Host fünf- bis sechsmal „Fortsetzen“ gedrückt hat. **Fertig, wenn** der Client während dieser Pause die Tafel oder einen Hinweis sieht, lokal und ohne eigene Autorität über die Weltzeit. Ordner `src/Game`. **Einstieg:** [Stillstand des Clients nach dem Bosstod](archive/GAME_ANALYSIS.md#stillstand-des-clients-nach-dem-bosstod-boss-client-einfrieren)

## `opt-loot-aufnahme-client`

**Bestätigungstest ausstehend:** Live-Test 24.11.2026, zwei PCs: Loot beim Client je Objekt einmal, 0 nach 6000 ms wieder gezeigt; Power-ups (Energy Drink, Level-Up) wirken beim Client laut Spieler nicht, ohne Logzeile dazu. Offen: Auto looten mit Benzinvergleich, Power-ups, Itemsäcke, [Live-Test 24.11.](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-24112026-livetest-2411) **Aufgabe und Abnahme:** Gefunden im Live-Test am 21.11.2026 (zwei PCs, Build `26786272`): Schrott und Benzin aus einem gelooteten Auto nimmt der Client scheinbar endlos auf, die Aufnahme wiederholt sich sichtbar. Client-Log: 680 Loot-Objekte je einmal gespielt, 74546 Wiederholungen ausgelassen, in der Spitze rund 12000 je Minute; der Ton ist geschützt, die Aufnahme selbst läuft mehrfach. Der Schrott-Stand folgt dem Host, eine Doppelbuchung ist nicht belegt. Vermutung: Das Objekt bleibt beim Client bis zur Bestätigung des Hosts liegen und löst die lokale Aufnahme jeden Frame neu aus. **Fertig, wenn** die Ursache belegt ist und jedes Loot-Objekt beim Client genau einmal sichtbar aufgenommen wird, Schrott und Benzin danach wie beim Host. Ordner `src/Game`. **Einstieg:** [Live-Test 21.11.](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-21112026-livetest-2111)

## `opt-sensenmann-ansage-client`

Seit `karte-nach-labyrinth-absturz` erzeugt nur der Host den Sensenmann, der Spawner des Clients kehrt vor `OnReaperSpawned` zurück. Vermutung: Dem Client fehlen dadurch Ansage, Ton und der Wechsel der Tagesuhr (`WBP_DayClock`, `WasReaperSpawned`), wenn der Sensenmann erscheint; gemessen ist das nicht. Offen ist auch, warum er im Live-Test schon 3,6 s nach der Ankunft kam (Vermutung `OME_EarlyReaper`). **Fertig, wenn** gemessen ist, was dem Client fehlt, und Ansage und Uhr beim Erscheinen des replizierten Sensenmanns lokal laufen, ohne zweiten Sensenmann. Ordner `src/Game`. **Einstieg:** [Client-Absturz am Sensenmann](archive/GAME_ANALYSIS.md#client-absturz-am-sensenmann-karte-nach-labyrinth-absturz)

## `opt-endboss-leiste`

**Bestätigungstest ausstehend:** Live-Test 24.11.2026, zwei PCs: Lazy-Boss über die Route, Leiste beim Client ab Kampfbeginn bis zum Tod. Offen: Tod von HQ und BlobBall zu zweit, [Live-Test 24.11.](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-24112026-livetest-2411) **Aufgabe und Abnahme:** Gefunden im Live-Test am 17.11.2026 (zwei PCs, Build `26669225`): Der Endboss auf `MGP_Burger_Boss_Nature` hatte beim Client keine Lebensleiste (gemeldet). Das Log nannte dort `BP_BallerinaBoss_C`; ob das der Endboss war, ist Vermutung. Die Boss-Leiste des Clients (`BossBarAdapter`) ist nur für SpikeBoss und FrankensteinBoss belegt, die übrigen Bosse blieben in `gegner-alle` offen. Nicht mit `boss-karte-absturz` verwechseln. **Fertig, wenn** alle erreichbaren Bosse beim Client eine Leiste mit HP-Verlauf und Tod wie beim Host haben und SpikeBoss und FrankensteinBoss unverändert laufen. Ordner `src/Game`. **Einstieg:** [Live-Test 17.11.](archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-17112026-livetest-1711)

## `opt-routenkarte`

Zurückgestellt aus `routenkarte-client` (09.11.2026): Der Client sieht dieselbe Routenkarte wie der Host (Knoten, Biome, Bus, Tag) und dieselbe Gefahrenstufe im HUD, wählen kann er weiter nicht. Belegt: Die Run-Info der GameInstance repliziert nicht, und `UBusPanel::OpenMapIfRequired` (`0x5AB7090`) befüllt die Karte nur bei `ROLE_Authority` des eigenen Controllers. Weg: Run-Info über den Kontrollkanal, Rollenprüfung beim Client umgehen oder `SetChoices` selbst aufrufen. **Fertig, wenn** der Client nach jedem Rundenende bis Tag 5 über den Biomwechsel nach Tag 4 dieselbe Karte und Gefahrenstufe wie der Host sieht und nicht reisen kann. **Einstieg:** [Routenkarte beim Client](archive/GAME_ANALYSIS.md#routenkarte-beim-client-routenkarte-client), Umsetzung in der Fallakte [opt-routenkarte](faelle/opt-routenkarte.md) (die Rollenprüfung sitzt in `UBusPanel::OpenPanel`)

## `sync-aufraeumen`

Nach dem ersten kompletten Run alle Patches, Hooks und Kontrollkanal-Nachrichten durchgehen: Was deckt inzwischen native Replikation oder lokale Spiellogik ab, was synchronisiert doppelt, was ist Spike-Rest? Grundlage ist der [Client-lokal-Audit](CLIENT_LOCAL_AUDIT.md). **Fertig, wenn** im Audit je Patch und Nachricht eine Entscheidung steht, die überflüssigen entfernt sind und ein Routenlauf bis Tag 4 mit Desync-Melder ohne Befund bleibt

## `coop-spielstand`

Der Host speichert einen Coop-Run und setzt ihn später mit demselben Client fort. **Fertig, wenn** nach Speichern, Beenden beider Spiele und erneutem Laden beim Host der Client beitritt und der Run mit gleichem Tag, gleicher Route und gleichen Inventaren weiterläuft

## `update-startpruefung`

Mod-/Spiel-Kompatibilität vor dem Laden prüfen: gemeinsame Quelle für Mod-Version, Protokollversion, unterstützte Game-Builds, Kompatibilitätsprofil und Datei-Hashes; Starter prüft die tatsächlich gestartete Installation (auch eingefrorene Kopien, Exe und relevante Inhaltscontainer), nicht nur das Steam-Manifest. Bei direktem Steam-Start prüft der `version.dll`-Loader erneut vor dem Laden von `DDCoop.dll`; die interne Prüfung bleibt bestehen. **Fertig, wenn** unbekannte/veränderte Builds und widersprüchliche Paketangaben die Mod nicht laden, ein sichtbarer Hinweis Mod-Version, gefundenen/erwarteten Build und Grund nennt und Abbrechen oder ein nachweislich modfreier Start möglich sind; keine Meldung unter dem Loader-Lock. Eine ältere, weiterhin kompatible Mod bleibt erlaubt; Internet ist für die lokale Freigabe nicht nötig. Pflicht-Patch-/Adapterfehler verhindern die Coop-Freigabe, Teilinstallation darf nicht als erfolgreicher Start gelten. Positiv-, Negativ- und Direktstarttests dokumentieren. **Einstieg:** `src/Loader/Loader.cpp`, `src/Core/Version.cpp`, `src/Mod/Mod.cpp`, `src/Game/Patches.cpp`, `tools/game_build.py`

## `update-releasefeed`

Vertrauenswürdig prüfbare Release-/Kompatibilitätsinformationen anbieten: signierte Metadaten mit Mod-Version, unterstützten Build-/Profilidentitäten, Protokollversion, Paket-Hashes und ausdrücklichen Rücknahmen kritischer Releases. Zunächst nur nach einem passenden Update suchen und es anbieten, nichts automatisch installieren. **Fertig, wenn** nur ausdrücklich freigegebene Pakete für den tatsächlich geprüften Game-Build angeboten werden, Signatur/Schema sowie veraltete oder zurückgespielte Metadaten geprüft werden und ungültige Antworten keine Freigabe bewirken; Offlinebetrieb nutzt die lokale Kompatibilitätsprüfung und kennzeichnet den Update-Status als unbekannt. Bekannte gültige Rücknahmen werden nicht durch Offlinebetrieb vergessen; eine bloß ältere kompatible Mod wird nicht gesperrt. Kein automatischer Support durch gefundene Signaturen. Tests für passende/unpassende Releases, Rücknahmen, ungültige Signaturen, veraltete Antworten und Netzausfall bestehen

## `update-autoupdater`

Später optionales automatisches Mod-Update auf dem Releasefeed aufbauen, nur nach ausdrücklicher Aktivierung. **Fertig, wenn** ein für den vorhandenen Game-Build freigegebenes Paket zunächst separat heruntergeladen, anhand signierter Metadaten und Hashes geprüft und ausschließlich bei beendetem Spiel transaktional installiert wird; Archive dürfen keine fremden Pfade überschreiben. Abbruch, beschädigter Download, Dateisperren und Installationsfehler lassen das alte vollständige Paket erhalten oder stellen es wieder her. Vorversion und Rückkehrmöglichkeit bleiben verfügbar, Konfiguration und Saves bleiben erhalten; Save-Kompatibilität vor Rückkehr prüfen. Keine Game-Updates, keine Änderung eingefrorener Spielkopien und kein Austausch geladener DLLs. Tests für Fehlerfälle, Wiederanlauf und Rückkehr dokumentieren

## `coop-inventar-analyse`

Das native geteilte Koop-Inventar ohne Mod untersuchen, Referenz ist lokaler Koop mit zwei Controllern auf einem PC: Aufbau des gemeinsamen Rucksacks mit Bereichen je Spieler (`ECoopInventoryLocationType::P1Backpack`/`P2Backpack`, `CoopP1Field`/`CoopP2Field`, `dd.Coop.SplitBackpackView`), Regeln, nach denen Unterstützungs-Items über die Bereichsgrenze auf Waffen wirken, Zuordnung von Waffe zu Figur, Ablauf von Lager, Airdrop-Wahl (`ClaimReward`, `AwaitingClaims`) und Rundenende mit `bCoopEnabled`. Außerdem neu bewerten, warum `inventar-sync` den Weg über `CoopInventoryCoordinatorComponent` verworfen hat und warum das Lager mit Koop-Modus hing. **Fertig, wenn** ein Abschnitt in `docs/themen/INVENTAR_UND_HANDEL.md` Datenmodell, Wirkregeln über Bereichsgrenzen, native Andockpunkte und die Grenzen der fest auf zwei Spieler ausgelegten Typen mit Belegt/Hypothese nennt, mit Bildvergleich. **Einstieg:** [Lokale Coop-Schnittstellen](archive/GAME_ANALYSIS.md#lokale-coop-schnittstellen), [Bis zu vier Spieler](MEHR_SPIELER.md)

## `coop-inventar-konzept`

Konzept für das geteilte Inventar online, ausgelegt auf zwei bis vier Spieler. Klären: Was bleibt lokal (Anzeige, Drag, Hervorhebung, Wirkungsberechnung), was geht über die Leitung, wer bucht; stabile Spieler-Slots statt fester P1/P2; Anordnung und Größe der Bereiche bei drei und vier Spielern; ob ein Spieler nur im eigenen Bereich ablegt; wie beide Modi nebeneinander bestehen (gemeinsame Teile, modusabhängige Pfade, keine doppelte Buchung) und wo der Modus gewählt und festgehalten wird; Beitritt, Ausstieg eines Spielers mitten im Run, Spielstand. Native Wege nur nutzen, wo die Analyse sie trägt. **Fertig, wenn** das Konzept in `docs/` steht, die Fragen aus dem Architekturprinzip beantwortet und das Team es freigegeben hat

## `coop-inventar-zwei`

Geteiltes Inventar für zwei Spieler online nach dem Konzept: ein gemeinsamer Rucksack mit einem Bereich je Spieler, beide sehen dasselbe Raster, Unterstützungs-Items wirken über die Bereichsgrenze auf Waffen des Mitspielers, die Hervorhebung beim Überfahren zeigt auch diese Wirkungen (mit `opt-waffen-hover` abstimmen). Der Host bucht, der Client bearbeitet lokal und bekommt Bestätigung oder Rücknahme. **Fertig, wenn** auf zwei PCs ein Unterstützungs-Item im Bereich von Spieler A die Waffe von Spieler B nachweislich verstärkt, die Waffenwerte bei beiden gleich sind und der Desync-Melder über ein Rundenende ohne Befund bleibt. Ordner `src/Game`, `src/Sync`

## `coop-inventar-ablauf`

Alle Inventarabläufe auf das geteilte Inventar umstellen: Airdrop-Wahl, Itemsäcke, Werkbank, Lager mit Verkauf und Reparatur, Rundenende mit Bestätigen. Der Modus Eigene Inventare bleibt unverändert nutzbar, beide Modi teilen sich vorhandene Pfade, wo das Konzept es vorsieht. **Fertig, wenn** ein Run auf zwei PCs von Tag 1 bis Tag 4 im Modus Geteiltes Inventar ohne Absturz und ohne Desync-Befund läuft und ein Run bis Tag 2 im Modus Eigene Inventare weiter wie in Release 1.0 läuft

## `coop-modus-wahl`

Der Host wählt vor dem Hosten der Lobby zwischen den Modi Eigene Inventare und Geteiltes Inventar (`coop-inventar-geteilt`). Der Modus geht beim Beitritt an alle Clients, gilt für den ganzen Run und lässt sich währenddessen nicht wechseln; Standard bleibt Eigene Inventare. Clients ohne passende Mod-Version lehnen ab statt im falschen Modus zu spielen. **Fertig, wenn** auf zwei PCs beide Modi nacheinander wählbar sind, jeder Run im gewählten Modus läuft und der Client den Modus des Hosts sieht. Mit `coop-spielstand` abstimmen: ein gespeicherter Run behält seinen Modus

## `coop-inventar-geteilt`

„Seperate new shared Inventory Logic like real coop“: eine neue Inventarlogik, getrennt von der bisherigen, die wie der lokale Koop des Spiels einen gemeinsamen Rucksack mit einem Bereich je Spieler zeigt, für zwei bis vier Spieler. Die bisherige Logik (jeder Spieler hat sein eigenes Inventar, Release 1.0 und `vier-spieler`) bleibt unverändert und ist Standard; die neue bekommt eigene Pfade statt Sonderfällen in der alten. Die nativen Typen kennen nur P1 und P2 (`ECoopInventoryLocationType`), darum eigene Bereichszuordnung je Spieler-Slot; Unterstützungs-Items wirken über die Bereichsgrenzen, jeder Spieler sieht alle Bereiche. Der Host bucht, Clients bearbeiten lokal mit Bestätigung oder Rücknahme. Sobald die neue Logik vorhanden ist, wählt der Host **vor dem Hosten der Lobby**, welche Logik gilt (Auswahl: `coop-modus-wahl`). **Fertig, wenn** vor dem Hosten zwischen bisheriger und neuer Logik gewählt werden kann, mit zwei, drei und vier Spielern ein Run bis Tag 2 im geteilten Inventar ohne Absturz und ohne Desync-Befund läuft, Unterstützungs-Items über jede Bereichsgrenze wirken und ein Run mit der bisherigen Logik weiter wie vorher läuft. **Einstieg:** `coop-inventar-analyse`, `coop-inventar-konzept`, [Bis zu vier Spieler](MEHR_SPIELER.md). Ordner `src/Game`, `src/Sync`

