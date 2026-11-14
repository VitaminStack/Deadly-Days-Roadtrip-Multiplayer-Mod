# Checkliste Live-Test Inventar (zwei PCs)

Stand 14.11.2026, für den Test am 15.11.2026. Geprüft werden die Inventar-Änderungen aus den Draft-PRs #242
(`opt-rucksack-versatz`), #243 (`opt-station-zug`) und #244 (`lager-edit-ablehnung`). Alle drei stecken im Branch
`task/lager-edit-ablehnung`. **Keine davon ist bisher im Spiel geprüft**, nur Build und Unit-Tests.

Allgemeiner Ablauf und Regeln: [Test auf zwei PCs](TESTING.md#test-auf-zwei-pcs). SteamIDs und Lobby-IDs nie ins Repo.

## Vor dem Termin, beide

- [ ] **Tobi (dieser PC):** Der Branch liegt schon im Worktree `.worktrees/lager-edit-ablehnung`. Dort `git pull` und alle Skripte von dort starten.
- [ ] **VitaminStack:** Im sauberen Hauptordner `git fetch origin`, dann `git switch --detach origin/task/lager-edit-ablehnung`. Nach dem Test zurück mit `git switch main`.
- [ ] **Beide:** `git rev-parse --short HEAD` vergleichen: gleich. `Start-Session.ps1 -Build` baut aus dem Ordner, aus dem es gestartet wird.
- [ ] Spiel-Build `26495302`, Steam läuft und ist online, keine andere Testsitzung auf dem PC.
- [ ] Discord oder ein anderer Chat außerhalb des Spiels steht.
- [ ] Rollen festlegen: wer Host, wer Client. Die meisten Punkte spielt der Client.

## Start

- [ ] **Host:** `powershell -ExecutionPolicy Bypass -File tools/spike/Start-Session.ps1 -Build -Mod -Role Host -Tunnel Lobby -Brief`
- [ ] **Client:** `powershell -ExecutionPolicy Bypass -File tools/spike/Start-Session.ps1 -Build -Mod -Role Client -Tunnel Lobby -Peer <Lobby-ID>`
- [ ] **Beide:** Im Overlay (F8) steht Mod-Version **14**, der Handshake ist bestätigt. Bei 13 auf einer Seite: abbrechen, Branch prüfen.
- [ ] Run starten wie gewohnt, bis Tag 1.

## A. Rucksack-Anzeige (#242)

- [ ] **Client:** Im ersten Airdrop ein Item **gedreht** in den Rucksack legen (einmal um 90 Grad, wenn möglich ein zweites um 180 Grad), bestätigen.
- [ ] **Client:** Beim nächsten Airdrop das Panel ansehen: Liegen die gedrehten Items genau auf ihren Feldern?
- [ ] **Client:** Sobald ein Rucksack-Teil kommt, den Rucksack **nach links oder oben** erweitern, bestätigen, beim nächsten Airdrop wieder ansehen.
- [ ] **Client:** Bildschirmfoto vom Rucksack bei jedem Airdrop, in dem etwas falsch liegt. Ein Bild vom Host ist nicht nötig: Die Felder und Pivots beider Seiten stehen in den Logs.
- [ ] Notieren: Welche Items lagen falsch, gedreht oder ungedreht, vor oder nach einer Erweiterung?

Erwartet: gedrehte Items liegen richtig. Liegt ein **ungedrehtes** Item nach einer Erweiterung falsch, gibt es eine zweite Ursache; das ist ein gültiges Ergebnis.

## B. Züge an einer Station (#243)

Braucht einen Händler-Knoten auf der Route.

- [ ] **Client:** Händler betreten, über den Zurück-Knopf das Inventar öffnen.
- [ ] **Client:** Ein Item aus dem Lager gedreht in den Rucksack legen, ein anderes herausnehmen, „Bestätigen“.
- [ ] **Client:** Danach normal weiterspielen: Schießt die Figur wieder, lassen sich Items aufsammeln?
- [ ] **Beide:** Im Overlay (F8) meldet der Desync-Melder einige Sekunden nach dem Bestätigen wieder „OK“ für das Inventar. (`Get-Status.ps1 -Inventory` vergleicht nur zwei Instanzen auf einem PC und hilft hier nicht.)
- [ ] **Client, wenn möglich:** Im selben Händler-Besuch einmal kaufen und einmal an der Werkbank aufwerten (Rezept-Panel von Hand).

Erwartet: beim Client die Logzeile „Station: Zug im Rucksack beim Client“, beim Host „Edit … übernommen, vollständig=true“.

## C. Lager am Rundenende (#244 und Gegenprobe)

- [ ] **Client:** Im Lager in einer Bearbeitung mischen: mindestens zwei Verkäufe, eine Reparatur, eine Aufwertung durch Ablegen, ein Rucksack-Teil anbauen, ein Item gedreht ablegen. „Bestätigen“.
- [ ] **Beide:** Beide Knöpfe gelb, nach dem zweiten Bestätigen reisen beide weiter.
- [ ] **Beide:** Am nächsten Tag meldet der Desync-Melder im Overlay „OK“ für das Inventar.

### Ablehnung absichtlich auslösen (nur mit Spike beim Host, also ohne `-NoSpike`)

- [ ] **Client:** Im Lager ein kaputtes Item reparieren und in den Rucksack legen, **noch nicht bestätigen**. (Kaputtes Item: Host gibt vorher `Send-Command.ps1 Host '!give ItemData_Pistol broken'`.)
- [ ] **Host:** Schrott auf null: `powershell -ExecutionPolicy Bypass -File tools/spike/Send-Command.ps1 Host '!scrap -9999'`
- [ ] **Client:** „Bestätigen“. Eile ist nicht nötig: Während einer Bearbeitung übernimmt der Client den Schrott des Hosts nicht.
- [ ] **Client:** Der gelbe Knopf geht wieder aus, das reparierte Item ist wieder kaputt und liegt im Lager, nichts fehlt.
- [ ] **Client:** Noch einmal etwas umlegen und erneut bestätigen: Das geht, und beide reisen weiter.
- [ ] **Host:** Der Host ist in der Zwischenzeit **nicht** ohne den Client weitergereist.

Erwartet: beim Client „Host hat Edit … abgelehnt: Schrott reicht nicht oder Preis ungültig“, „Ausgangsstand wiederhergestellt …, vollständig=true“ und „Lager: Edit abgelehnt, Bereit zurückgenommen“.

## D. Klon nach Aufwertung im Airdrop (#244)

- [ ] **Client:** In einer Airdrop-Auswahl die Belohnung auf ein gleiches Item im Rucksack legen (Aufwertung).
- [ ] **Client:** Direkt danach ins Inventar schauen: Die verbrauchten Items sind weg, es gibt **kein** zusätzliches Item.
- [ ] **Client:** Versuchen, die schon aufgewertete Belohnung noch einmal aus der Belohnungsanzeige zu ziehen. Notieren, was das Panel zeigt.
- [ ] **Client:** Bestätigen. Der Host übernimmt, die aufgewertete Waffe wirkt im Kampf.

Erwartet: kein Klon. Unklar ist, wie das Panel beim zweiten Ziehen aussieht; jede Auffälligkeit dort notieren.

## E. Nebenbei beobachten

- [ ] Ruckelt der Client mehr als sonst? (Die Einordnung der Aufrufe läuft jetzt die ganze Mission über.)
- [ ] Kopierladen, Automat, dubioser Händler: sind **nicht** behoben. Wenn sie vorkommen, kurz benutzen, damit die neuen Logzeilen den Fall zeigen.
- [ ] Absturz: Uhrzeit notieren, `py tools/spike/crashes.py` laufen lassen, wieder beitreten.

## Nach dem Test, beide

- [ ] Sitzung beenden: `powershell -ExecutionPolicy Bypass -File tools/spike/Stop-Session.ps1`
- [ ] Das eigene Mod-Log sichern: `DDSurvivors/Binaries/Win64/DDCoop/logs/ddcoop-<pid>.log` (das größte vom Testabend). **Beide Logs werden gebraucht**, auch das des Hosts.
- [ ] Bildschirmfotos und Notizen dazulegen. Logs nicht ins Repo, sie können IDs enthalten.

## Zeilen, nach denen die Auswertung sucht

| Zeile | Seite | Bedeutung |
|---|---|---|
| `Rucksack-Anzeige(n) aus replizierten Feldern positioniert, N davon mit Ecke abseits des Pivots` | Client | A: so viele gedrehte Items wurden neu ausgerichtet |
| `Rucksack-Anzeige: <Item> Drehung … Pivot … Ecke … Position …` | Client | A: je Item, woran es ausgerichtet wurde und wo es danach steht |
| `lagen nach der Aufwertung noch im Inventar, sofort entfernt` | Client | D: Klon direkt bei der Aufwertung entfernt |
| `Bearbeitung war ungültig und ist zurückgenommen, nicht bestätigt` | Client | C: eigener Edit ungültig, Lager neu bearbeiten |
| `Station: Zug im Rucksack beim Client` | Client | B: Bearbeitung begann mit dem ersten Zug |
| `außerhalb einer Bearbeitung geht nativ an den Host` | Client | Aufruf, der weiter am Edit vorbeiläuft |
| `kam während der Bearbeitung dazu und steht nicht im Edit` | Client | Item, das der Edit nicht kennt (etwa Kopierladen) |
| `Host hat Edit … abgelehnt: <Grund>` | Client | C: Ablehnungsgrund |
| `teilweise übernommen` | Client | Host hat nur einen Teil geschafft |
| `danach abweichend: … (ist/soll)` | beide | Item, das nach Übernahme oder Zurückrollen nicht passt |
| `zweiter Durchgang` | Client | Zurückrollen brauchte einen zweiten Anlauf |
| `für schon aufgewertete Eingabe verworfen` | Client | D: Klon verhindert |
| `Takt:` | Client | E: Zahl mit früheren Läufen vergleichen |
