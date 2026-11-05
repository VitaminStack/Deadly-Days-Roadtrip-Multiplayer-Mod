# UI

Overlay der Mod: Lobby, Einladungen, Verbindungsstatus und Debug-Anzeige. Ändert den Spielzustand nur über `Net` oder `Sync`.

## Stand (`overlay`)

`UI::Start` startet nach der Versionsprüfung ein eigenes Win32-Toolfenster im Spielprozess. **F8** zeigt oder verbirgt es, solange das zugehörige Spielfenster oder das Overlay im Vordergrund ist. Es wird ohne Fokuswechsel eingeblendet; erst ein Klick gibt ihm den Tastaturfokus. Schließen verbirgt das Fenster, beendet aber weder Spiel noch Mod. Seit `overlay-fokus` bleibt ein eingeschaltetes Overlay auch sichtbar, wenn das Spiel den Fokus verliert, etwa beim Wechsel zu Discord oder Browser. Es wird dann nur mitbewegt, nicht nach vorn geholt: Als Fenster im Besitz des Spielfensters liegt es über dem Spiel, andere Anwendungen können beide verdecken. Bei minimiertem Spiel verschwindet es. F8 schaltet weiterhin nur bei Fokus auf Spiel oder Overlay um. Der Ziel-Viewport wird nur unter sichtbaren Hauptfenstern der eigenen Prozess-ID gesucht, auch nach einem Fensterwechsel.

Das Fenster zeigt Verbindungszustand, Meldung, Rolle, Spielerzahl, Lobby-ID, Ping (nur wenn bekannt) und Prozess-ID. Der Knopf „Lobby-ID kopieren“ legt die aktuelle Lobby-ID als Text in die Zwischenablage und zeigt kurz „Kopiert“; ohne Lobby ist er deaktiviert. Er läuft nur im Fensterthread und löst keine Backend-Aktion aus. Es folgt der oberen rechten Ecke des Spiel-Clientbereichs; Schrift und Steuerelemente werden mit dessen DPI skaliert. Tab bedient die Steuerelemente. Es gibt keinen global registrierten Hotkey, keinen Renderer-/WndProc-Hook und keinen SDK-Zugriff.

**Grenze:** Das ist ein einfaches, überlagertes Windows-Fenster für Fenster-/randlosen Betrieb, kein in den D3D-Framebuffer gezeichnetes HUD. Exklusives Vollbild ist nicht unterstützt. F8 wird nicht aus der Spieleingabe entfernt. Das Overlay pausiert das Spiel nicht und ersetzt keine Gameplay-Eingabesperre bei Inventar-/Reward-Dialogen.

## Konfiguration

Optionale Zeilen in `DDCoop/ddcoop.ini` neben der geladenen Mod-DLL:

```ini
overlay_enabled=true
overlay_visible=false
```

Standard: aktiv, zunächst verborgen. `overlay_enabled=false` startet keinen Fensterthread. `overlay_visible=true` zeigt das Fenster beim nächsten Fokus auf das Spiel, ohne sich den Fokus zu holen. Die Konfiguration wird wie bisher beim Mod-Start gelesen, nicht laufend neu geladen.

## Lobby-Anbindung

Erstellen, Beitreten (dezimal eingegebene Steam-Lobby-ID), Einladen und Verlassen laufen seit `steam-lobby` über die Steam-Lobby
der Mod: `src/Mod/Lobby.cpp` bindet die Knöpfe an `Net::Session` und veröffentlicht deren Status alle 200 ms. Bis Steam bereit ist,
steht dort „Wartet auf Steam“ und alle Knöpfe bleiben aus. Mit dem Testschalter `-ddcoop_transport=...` bleibt die Lobby aus.
Ablauf und Grenzen in [Net](../Net/README.md#steam-lobby-steam-lobby).

Die Komposition in `src/Mod` nutzt `UI::Overlay()`, ohne eine umgekehrte Abhängigkeit `Net -> UI` einzuführen:

- `Bind(ActionHandler)` setzt einen Handler. Er läuft ausschließlich aus `Core::GameThread().Drain`, niemals auf dem Fensterthread. Erst nach vorhandenem Dispatcher-/Netzwerkanschluss binden. Captures müssen bis zum Ende bereits laufender Aufrufe gültig bleiben, z. B. durch eigene Shared-Ownership; bloße UObject-Zeiger sind ungeeignet.
- `Publish(OverlayStatus)` übernimmt einen vollständigen, kopierten Status inklusive der vom Backend freigegebenen Aktionen. Thread-sicher, keine Engine-/Steam-Aufrufe. `Message` ist UTF-8, die Darstellung ist auf 2048 Bytes begrenzt; unbekannten Ping durch `HasPing=false` kennzeichnen. `LobbyRequest::LobbyId` enthält das Beitrittsziel bei `Join`, sonst 0.
- `Submit` validiert Zustand, Rolle, Fähigkeiten und Beitrittsziel vor dem Einreihen **und erneut vor der Ausführung**. Nur eine ausstehende UI-Aktion gleichzeitig. Neue Rolle, Session-State oder Lobby-ID verwerfen alte Aufträge beim nächsten Drain; reine Statistik-/Meldungsupdates lassen sie bestehen. Ein Backend-Wechsel bzw. `Bind({})` invalidiert alte Aufträge ebenfalls.
- Der Handler muss die Aktion erneut gegen seinen autoritativen Zustand prüfen und den Folgezustand veröffentlichen, etwa `Connecting` statt weiterhin `Offline`. Nur die Backend-Antwort darf einen erfolgreichen Beitritt anzeigen. Keine fremden UI-/Engine-Zeiger austauschen.
- Ohne Drain bleibt sichtbar „Aktion wartet auf den Game-Thread-Dispatcher“. Der Fensterthread führt nichts ersatzweise selbst aus. `UI::Stop()` invalidiert ausstehende Aufträge und beendet den Fensterthread; **nicht** aus `DllMain` oder vom Fensterthread aufrufen. Kein Hot-Unload während eines laufenden Backend-Aufrufs. Der produktive Loader hält die Mod derzeit bis Prozessende geladen.

Das Modul bleibt auf `Core` und `Net` begrenzt. Der Game-Adapter ist dafür nicht geändert worden.

## Tests

`UITests` ist ein eigenes Projekt in der Solution und läuft über `Build-Mod.ps1 -Test` sowie in Debug-/Release-CI ohne SDK oder Spiel. Tests prüfen Zustands-/Rollenfreigaben, ungültige Lobby-IDs und Überlauf, begrenzte Aufträge (auch während der Ausführung), Dispatcher-Thread, Reentranz, Session-/Backendwechsel, konkurrierende Statusupdates und nativen Fensterstart/-schluss/-neustart. Der Fenstertest bedient die echten Edit-/Button-Controls gegen ein reines Testbackend, bleibt dabei unsichtbar und verändert keinen Benutzerfokus. Er prüft keine Grafikabnahme im Spiel.

Lokal am 03.11.2026: Debug und Release jeweils erfolgreich gebaut, 18 Core-Tests (77 Assertions), 13 UI-Tests (1227 Assertions) und 58 Python-Tests bestanden. Dateilinks, Modulgrenzen und unveränderte Fortschrittszähler geprüft; kein Eingriff in die fremde Sitzung.

### Sichtbare Spielabnahme (03.11.2026)

Nach Freigabe der vorher fremd belegten Sitzung eine eigene Sitzung ausschließlich mit `Start-Session.ps1 -Build -Mod`/CrashGuard gestartet, ohne `-unattended`. Verwendet wurde die geprüfte Entwicklungskopie von Build `26187885`, nur in der lokalen Worktree-Konfiguration aktiviert. UI-Code im getesteten Release-Build: `d9fcb36`; das danach übernommene `main` änderte daran nichts. Host und Client waren in der `MetaBase` verbunden, auf beiden Seiten `players=2`, beim Host `clients=1`. Beide Prozesspfade zeigten auf die eingefrorene Kopie; beide Mod-Logs bestätigten den Build, den gesperrten Crash-Reporter und „Overlay bereit“.

| Prüfung, jeweils Host und Client | Ergebnis |
|---|---|
| F8 aus dem Spielfenster | Eigenes Overlay erscheint, ohne den Fokus zu übernehmen; das Overlay der anderen Instanz bleibt verborgen |
| Klick in die Lobby-Eingabe, danach Tab | Fokus liegt auf der Eingabe; Tab überspringt die vier deaktivierten Lobby-Knöpfe und bleibt beim einzigen verfügbaren Eingabefeld |
| F8 bei fokussiertem Overlay | Verbirgt das Overlay und gibt den Fokus an das eigene Spiel zurück; erneutes F8 öffnet wieder |
| Tatsächlicher Klick auf das Fenster-X | Verbirgt nur das Overlay, Spiel bleibt aktiv und fokussiert; Wiederöffnen mit F8 funktioniert |
| Wechsel Host ↔ Client | Nur das eingeschaltete Overlay der gerade aktiven Instanz ist sichtbar |
| Fokus auf unabhängiges, eigens erzeugtes Testfenster | Beide Overlays verschwinden. F8 dort schaltet keines um; nach Rückkehr bleiben die vorher eingeschalteten Zustände erhalten |
| Spielfenster verschieben | Overlay folgt der oberen rechten Ecke des Clientbereichs |
| Minimieren und Wiederherstellen | Eigenes Overlay verschwindet und erscheint nach Wiederherstellung/Fokus erneut. Wird automatisch die andere Spielinstanz aktiv, darf deren eigenes eingeschaltetes Overlay erscheinen |
| Wechsel zwischen Monitoren mit 150 % und 100 %, danach zurück | Beide Overlays bleiben passend positioniert und lesbar; Fensteraufnahmen bei beiden Skalierungen visuell geprüft, keine abgeschnittenen Steuerelemente. Die Spielprozesse melden 96 logische DPI, Windows skaliert auf dem 150-%-Monitor |
| Zustand ohne Phase-3-Backend | „Netzwerk noch nicht angebunden“, Rolle „Keine“, unbekannter Ping/Lobby „-“. Alle vier Lobby-Knöpfe deaktiviert; kein vorgetäuschter Steam-Beitritt |
| Zustand nach den Prüfungen | Beide Instanzen weiterhin in der `MetaBase` verbunden; keine Fehler in ihren Mod-Logs |

Anschließend nur die eigene Sitzung mit `Stop-Session.ps1` beendet und die Test-Mod entfernt. `Test-Setup.ps1` bestand erneut, alle 54 eingefrorenen Originaldateien unverändert. Bilder, Fenster-/Prozessdaten und die lokalen Prüfscripte bleiben privat unter `research/local/session`, nicht im öffentlichen Repo. Der Phase-2-Overlay-Punkt ist damit abgeschlossen; die Game-Adapter-Schicht bleibt separat offen.

**Messhinweise:** Erste `PrintWindow`-Aufnahmen waren wegen der DPI-Virtualisierung unvollständig und wurden nicht als visuelle Abnahme verwendet. Die geprüften Aufnahmen stammen anschließend ausschließlich aus dem sichtbaren Rechteck des eigenen Overlays mit DPI-korrekten Koordinaten. Eine anfangs zu strenge Minimier-Assertion erwartete beide Overlays verborgen; Windows aktivierte aber korrekt die andere Spielinstanz. Der Wiederholungstest prüfte daher gezielt das Overlay des minimierten Spiels und bestand. Keine Änderung am produktiven UI-Code erforderlich.

**Nicht abgenommen:** exklusives Vollbild (nicht unterstützt), Steam-/Internet-Verbindung, ein echter Lobby-Backend-Aufruf oder eine blockierende Gameplay-Eingabesperre. Der Test war eine Overlay-Abnahme in der `MetaBase`, kein Missions-/Run-Nachweis und keine Bestätigung, dass der Spiel-Coop bereits im Netzwerkmodul der Mod umgesetzt ist.
