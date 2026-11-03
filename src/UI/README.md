# UI

Overlay der Mod: Lobby, Einladungen, Verbindungsstatus und Debug-Anzeige. Ändert den Spielzustand nur über `Net` oder `Sync`.

## Stand (`overlay`)

`UI::Start` startet nach der Versionsprüfung ein eigenes Win32-Toolfenster im Spielprozess. **F8** zeigt oder verbirgt es, solange das zugehörige Spielfenster oder das Overlay im Vordergrund ist. Es wird ohne Fokuswechsel eingeblendet; erst ein Klick gibt ihm den Tastaturfokus. Schließen verbirgt das Fenster, beendet aber weder Spiel noch Mod. Andere Anwendungen, minimierte Spiele und die zweite Spielinstanz werden nicht überlagert. Der Ziel-Viewport wird nur unter sichtbaren Hauptfenstern der eigenen Prozess-ID gesucht, auch nach einem Fensterwechsel.

Das Fenster zeigt Verbindungszustand, Meldung, Rolle, Spielerzahl, Lobby-ID, Ping (nur wenn bekannt) und Prozess-ID. Es folgt der oberen rechten Ecke des Spiel-Clientbereichs; Schrift und Steuerelemente werden mit dessen DPI skaliert. Tab bedient die Steuerelemente. Es gibt keinen global registrierten Hotkey, keinen Renderer-/WndProc-Hook und keinen SDK-Zugriff.

**Grenze:** Das ist ein einfaches, überlagertes Windows-Fenster für Fenster-/randlosen Betrieb, kein in den D3D-Framebuffer gezeichnetes HUD. Exklusives Vollbild ist nicht unterstützt. F8 wird nicht aus der Spieleingabe entfernt. Das Overlay pausiert das Spiel nicht und ersetzt keine Gameplay-Eingabesperre bei Inventar-/Reward-Dialogen.

## Konfiguration

Optionale Zeilen in `DDCoop/ddcoop.ini` neben der geladenen Mod-DLL:

```ini
overlay_enabled=true
overlay_visible=false
```

Standard: aktiv, zunächst verborgen. `overlay_enabled=false` startet keinen Fensterthread. `overlay_visible=true` zeigt das Fenster beim nächsten Fokus auf das Spiel, ohne sich den Fokus zu holen. Die Konfiguration wird wie bisher beim Mod-Start gelesen, nicht laufend neu geladen.

## Lobby-Anbindung für Phase 3

Erstellen, Beitreten (dezimal eingegebene Steam-Lobby-ID), Einladen und Verlassen sind vorbereitet, **aber ohne echtes Backend deaktiviert**. Der derzeitige `Net::Start` liefert noch keines: Standardstatus ist ausdrücklich „Netzwerk noch nicht angebunden“, keine fingierte Verbindung. Das Aktivieren der Steam-Funktionen bleibt Phase 3.

Die Komposition in `src/Mod` kann später `UI::Overlay()` nutzen, ohne eine umgekehrte Abhängigkeit `Net -> UI` einzuführen:

- `Bind(ActionHandler)` setzt einen Handler. Er läuft ausschließlich aus `Core::GameThread().Drain`, niemals auf dem Fensterthread. Erst nach vorhandenem Dispatcher-/Netzwerkanschluss binden. Captures müssen bis zum Ende bereits laufender Aufrufe gültig bleiben, z. B. durch eigene Shared-Ownership; bloße UObject-Zeiger sind ungeeignet.
- `Publish(OverlayStatus)` übernimmt einen vollständigen, kopierten Status inklusive der vom Backend freigegebenen Aktionen. Thread-sicher, keine Engine-/Steam-Aufrufe. `Message` ist UTF-8, die Darstellung ist auf 2048 Bytes begrenzt; unbekannten Ping durch `HasPing=false` kennzeichnen. `LobbyRequest::LobbyId` enthält das Beitrittsziel bei `Join`, sonst 0.
- `Submit` validiert Zustand, Rolle, Fähigkeiten und Beitrittsziel vor dem Einreihen **und erneut vor der Ausführung**. Nur eine ausstehende UI-Aktion gleichzeitig. Neue Rolle, Session-State oder Lobby-ID verwerfen alte Aufträge beim nächsten Drain; reine Statistik-/Meldungsupdates lassen sie bestehen. Ein Backend-Wechsel bzw. `Bind({})` invalidiert alte Aufträge ebenfalls.
- Der Handler muss die Aktion erneut gegen seinen autoritativen Zustand prüfen und den Folgezustand veröffentlichen, etwa `Connecting` statt weiterhin `Offline`. Nur die Backend-Antwort darf einen erfolgreichen Beitritt anzeigen. Keine fremden UI-/Engine-Zeiger austauschen.
- Ohne Drain bleibt sichtbar „Aktion wartet auf den Game-Thread-Dispatcher“. Der Fensterthread führt nichts ersatzweise selbst aus. `UI::Stop()` invalidiert ausstehende Aufträge und beendet den Fensterthread; **nicht** aus `DllMain` oder vom Fensterthread aufrufen. Kein Hot-Unload während eines laufenden Backend-Aufrufs. Der produktive Loader hält die Mod derzeit bis Prozessende geladen.

Das Modul bleibt auf `Core` und `Net` begrenzt. Der Game-Adapter ist dafür nicht geändert worden.

## Tests und offene Spielabnahme

`UITests` ist ein eigenes Projekt in der Solution und läuft über `Build-Mod.ps1 -Test` sowie in Debug-/Release-CI ohne SDK oder Spiel. Tests prüfen Zustands-/Rollenfreigaben, ungültige Lobby-IDs und Überlauf, begrenzte Aufträge (auch während der Ausführung), Dispatcher-Thread, Reentranz, Session-/Backendwechsel, konkurrierende Statusupdates und nativen Fensterstart/-schluss/-neustart. Der Fenstertest bedient die echten Edit-/Button-Controls gegen ein reines Testbackend, bleibt dabei unsichtbar und verändert keinen Benutzerfokus. Er prüft keine Grafikabnahme im Spiel.

Lokal am 03.11.2026: Debug und Release jeweils erfolgreich gebaut, 18 Core-Tests (77 Assertions), 13 UI-Tests (1227 Assertions) und 58 Python-Tests bestanden. Dateilinks, Modulgrenzen und unveränderte Fortschrittszähler geprüft; kein Eingriff in die fremde Sitzung.

**Noch offen:** sichtbarer Host-/Client-Test im Fenster-/randlosen Betrieb (F8, Fokuswechsel, Folgen beim Verschieben/Minimieren, Lesbarkeit/DPI und Tab-Bedienung). Beim Ausbau am 03.11.2026 belegte die fremde Aufgabe `tageswechsel` die Spielsitzung; sie blieb unberührt. Deshalb ist der Phase-2-Punkt noch nicht abgehakt. Kein Steam- oder Internet-Test und keine neue Laufzeitbehauptung.
