# Mod

Die eigentliche Mod-DLL `DDCoop.dll`. Bindet alle Module zusammen und enthält sonst nichts. Der Loader kann das nicht übernehmen, weil er laut [Modulgrenzen](../../docs/ARCHITECTURE.md#module) nichts außer dem Start kennt.

## Ablauf von `ModStart`

Läuft einmal auf dem Thread des Loaders:

1. `ddcoop.ini` neben der DLL lesen, fehlt sie, gelten die Standardwerte.
2. Log unter `logs/ddcoop-<pid>.log` öffnen. Je Prozess eine Datei, weil Host und Client im Test aus demselben Ordner laufen.
3. Mit `enabled=false` hier aufhören.
4. Versionsprüfung, bei unbekanntem Build oder fremdem Prozess hier aufhören.
5. Hooks vorbereiten.
6. `Game::Start`, `Net::Start`, `Sync::Start` und `UI::Start` aufrufen.

Keine Ausnahme verlässt `ModStart`. Das Spiel läuft in jedem Fall normal weiter, nur ohne Mod.

## `ddcoop.ini`

```ini
# Schaltet die Mod ab, ohne Dateien zu entfernen
enabled=true
# debug, info, warn oder error
log_level=info
```

Ein neuer Einstieg kommt als Funktion ins jeweilige Modul und einen Aufruf hier. Die Reihenfolge folgt den Abhängigkeiten.
