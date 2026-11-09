# Core

Grundfunktionen für alle Module: Logging, Config, Hook-Framework, Game-Thread-Dispatcher und Versionsprüfung.
Kennt keine Spielklassen und bindet kein SDK ein. Includes laufen über den Modulordner, etwa `#include "Core/Log.h"`.

Die Schnittstellen hier nutzen alle Module. Änderungen daran gehen in kurzen eigenen Pull Requests, siehe [Gemeinsame Dateien](../../TODO.md#wo-kann-ich-anfangen).

## Logging: `Core/Log.h`

```cpp
Core::Log::Info("Lobby {} erstellt", LobbyId);
Core::Log::Warn("...");
Core::Log::Error("...");
Core::Log::Debug("...");
```

- Von jedem Thread aus aufrufbar, jede Zeile wird sofort geschrieben. Vor `Open` gehen Zeilen verloren.
- Zeitstempel sind UTC, dazu Thread-ID und Stufe.
- `Open` und `SetLevel` ruft nur der Start der Mod auf.
- Keine vollständigen lokalen Pfade, Steam-IDs oder IP-Adressen loggen, Logs landen in Bug-Reports.

## Config: `Core/Config.h`

Zeilen `schluessel=wert`, Kommentare mit `#` oder `;`, keine Abschnitte. Eine fehlende Datei oder ein fehlender oder unlesbarer Wert ergibt den übergebenen Standardwert, das ist kein Fehler.

```cpp
const auto Config = Core::Config::Load(ModDir / "ddcoop.ini");
const bool Enabled = Config.GetBool("enabled", true);
```

`GetString`, `GetBool` (`true`, `false`, `1`, `0`) und `GetInt` sind die ganze Schnittstelle.

## Game Thread: `Core/Dispatcher.h`

Unreal-Objekte dürfen nur auf dem Game Thread angefasst werden. Andere Threads geben Arbeit ab:

```cpp
Core::GameThread().Post([Value] { /* läuft auf dem Game Thread */ });
```

- `Post` darf jeder Thread aufrufen, die Aufgabe läuft später, ohne dass der Aufrufer wartet. Aufgaben laufen in Reihenfolge.
- Aufgaben kopieren Daten hinein. UObject-Zeiger können bis zur Ausführung ungültig werden.
- `Drain` ruft der Game-Thread-Takt aus `src/Game` (`Game::GameThread`) etwa alle 16 ms auf, sobald das Spielfenster da ist.
- `IsDrainThread` ist für Prüfungen da und bleibt falsch, bis `Drain` einmal lief.
- Eine Ausnahme in einer Aufgabe wird geloggt, die übrigen Aufgaben laufen weiter.

## Versionsprüfung: `Core/Version.h`

`DetectSupportedBuild` liefert die Steam-Build-ID, wenn der Prozess das Spiel ist und der Fingerabdruck aus dem Exe-Header (`TimeDateStamp`, `SizeOfImage`) bekannt ist. Sonst liefert sie nichts, loggt den Grund, und die Mod bleibt aus. Die Liste steht in `Core/Version.cpp`.

Das Spiel schreibt die Build-ID nur ins Steam-Manifest, dessen Pfad die Mod nicht kennt. Der Fingerabdruck steht dagegen schon im Speicher. Neue Werte liefert `dumpbin /headers` auf die Exe, siehe [Spiel-Updates](../../CONTRIBUTING.md#spiel-updates).

## Hooks: `Core/Hooks.h`

Inline-Hooks über MinHook. `Initialize` ruft der Start der Mod erst nach bestandener Versionsprüfung auf, die Module rufen nur `Install` und `Remove` auf.

```cpp
using ProcessEventFn = void(__fastcall*)(void*, void*, void*);
ProcessEventFn Original = nullptr;

void __fastcall Detour(void* Object, void* Function, void* Params)
{
    Original(Object, Function, Params);
}

Core::Hooks::Install(reinterpret_cast<ProcessEventFn>(Target), &Detour, &Original);
```

- `Install` meldet einen Fehlschlag mit `false` und loggt ihn. Der Aufrufer lässt seine Funktion dann aus, statt weiterzulaufen.
- `Original` steht fest, bevor der Hook aktiv wird.
- Detours laufen auf dem Thread des Aufrufers. Eng filtern und das Original genau einmal rufen, siehe [Andockpunkte](../../docs/ARCHITECTURE.md#technische-andockpunkte-build-26187885).

## Absturzberichte: `Core/CrashReports.h`

`Block` lässt im eigenen Prozess jeden Start von `CrashReportClient.exe` scheitern, damit Abstürze mit Mod nie beim Hersteller landen. `Protection::Install` ruft es als ersten Schritt auf. Der lokale Bericht unter `Saved/Crashes` bleibt erhalten. Hintergrund in der [Spike-README](../../research/spikes/a-connect/README.md#sperre-in-mod-und-spike-crash-reporter).

## Schutz beim Start: `Core/Protection.h`

`Install` richtet nach `Hooks::Initialize` alle Sperren gegen Daten an den Hersteller ein: Crash-Reporter, Berichtsziele, dann Steam-Erfolge und -Bestenlisten. Scheitert eine davon, loggt es den Schritt und liefert falsch. Dann startet die Mod nicht, Game, Net, Sync und UI bleiben aus. Neue Sperren kommen als weiterer Schritt hinzu.

## Telemetrie und Berichte: `Core/ReportHosts.h`

`Block` lässt im eigenen Prozess die Namensauflösung für die Berichtsziele des Spiels scheitern: Hooks auf `getaddrinfo`, `GetAddrInfoW` und `GetAddrInfoExW` in `ws2_32`, unabhängig von Spiel-Offsets. Gesperrt sind `codecks.io` (Bug-Reports, Berichte über beschädigte Spielstände) , `pixelsplit.games` (Telemetrie, Spielstand-Analytics) `deadlydays.layerapps.com` (Anfrage beim Run-Ende) und `ddrweekly.deadlydays.com` (Weekly-Challenge, mit Mod gesperrt) samt Subdomains, `Add` sperrt weitere Hosts. `Protection::Install` ruft es direkt nach `CrashReports::Block` auf, danach prüft ein Selbsttest, dass `api.codecks.io` ohne Netzanfrage scheitert. Jeder aufgelöste Name steht einmal im Log, IP-Adressen nicht. Hintergrund in [Telemetrie und Berichte](../../docs/GAME_ANALYSIS.md#telemetrie-und-berichte-telemetrie-aus).

## Erfolge und Bestenlisten: `Core/SteamStats.h`

`Block` sperrt die schreibenden Methoden von `ISteamUserStats` (Erfolge, Statistiken, Bestenlisten-Einträge), damit Mod-Runs nichts in Steam verändern. Lesen bleibt erlaubt. `Protection::Install` ruft es nach den Berichtssperren auf. Die Sperre greift in `SteamInternal_FindOrCreateUserInterface`, bevor das Spiel die Schnittstelle bekommt; ist `steam_api64.dll` noch nicht geladen, beim Laden über einen Hook auf `LoadLibraryExW`. Ohne Sperre bekommt das Spiel keine Statistik-Schnittstelle oder kein Steam, die Mod startet nicht, wenn die Sperre beim Start scheitert. Die vtable-Plätze kommen aus dem Code der flachen Steam-Exporte, nicht aus Spiel-Offsets. `Testing` erlaubt Tests mit nachgebauter Steam-Laufzeit. Hintergrund in [Erfolge und Bestenlisten](../../docs/GAME_ANALYSIS.md#erfolge-und-bestenlisten-online-sperren).

## Signaturen: `Core/Signature.h`

```cpp
const auto Pattern = Core::ParsePattern("48 8B ?? 05");
const void* Target = Pattern ? Core::FindUniqueInImage(GetModuleHandleW(nullptr), *Pattern) : nullptr;
```

`FindUniqueInImage` sucht nur in ausführbaren Sektionen und liefert einen Treffer nur, wenn es genau einen gibt. Bei keinem oder mehreren kommt `nullptr`, einen beliebigen ersten Treffer zu patchen ist verboten.
