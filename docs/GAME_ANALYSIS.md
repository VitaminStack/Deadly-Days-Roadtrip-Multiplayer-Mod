# Spielanalyse

Stand: 02.11.2026. Neue Erkenntnisse bitte hier ergänzen, ausführliche Notizen gehören nach `research/`.

Vertrauensgrad:

- **[Belegt]** direkt an der aktuellen Installation geprüft
- **[Alt]** aus dem Dump vom 23.07.2026 (UE 5.7), für die aktuelle Version nicht bestätigt
- **[Hypothese]** Schlussfolgerung, noch nicht getestet

## Unterstützte Spielversion

| | |
|---|---|
| Steam-AppID | `3026450` |
| Steam-Build-ID | `26187885` |
| Engine-Version der Exe | `++UE5+Release-5.8-CL-56057345` |
| Spiel-Exe | `DDSurvivors/Binaries/Win64/DDSurvivors-Win64-Shipping.exe` (179 MB) |

Die Build-ID steht in `steamapps/appmanifest_3026450.acf`. Bei einem Spiel-Update siehe
[Update-Regel](../CONTRIBUTING.md#spiel-updates).

## Offene Blocker

### Dumper-7 stürzt bei der aktuellen Version ab

- **[Belegt]** Vier Abstürze am 02.11.2026 mit Dumper-7 7.0.1 auf Engine `5.8.1-56057345`, jeweils
  `EXCEPTION_ACCESS_VIOLATION reading address 0x000000000000cdb0` im Spielcode (`Exe + 0x143d6d1`).
- **[Belegt]** Aufrufkette: `ObjectArray::DumpObjectsWithProperties` → `UEProperty::GetPropClassName` →
  `FName::ToWString` → Name-Funktion des Spiels. Beim Dump der Objekte ohne Properties tritt kein Fehler auf, der erste Absturz
  kommt beim Auflösen eines Property-Klassennamens.
- **[Belegt]** Berichte liegen unter `%LOCALAPPDATA%/DDSurvivors/Saved/Crashes` (Ordner `UECC-Windows-*`).
- **[Hypothese]** Dumper-7 7.0.1 liest unter UE 5.8 ein falsches Layout für `FField`/`FFieldClass`
  (`Off::FField`, `Off::FFieldClass::Name`), übergibt dadurch einen ungültigen FName. Die feste Adresse in allen Läufen spricht
  für einen systematischen Layoutfehler statt für ein Timing-Problem. Bestätigt ist das noch nicht.
- Nächste Schritte:
  1. Prüfen, ob eine neuere Dumper-7-Version UE 5.8 bereits unterstützt.
  2. Sonst den `FField`-/`FFieldClass`-Aufbau aus dem Spielspeicher ablesen und mit den Offsets in `Offsets.h` abgleichen.
  3. Das Überspringen von `DumpObjectsWithProperties` löst das Problem vermutlich nicht, weil die SDK-Erzeugung dieselben
     Property-Daten braucht.
  4. Unabhängig davon: Die Paks sind unverschlüsselt, Teile lassen sich also statisch auslesen (FModel).
- Bis dahin dient der Juli-Dump nur zur Orientierung. Offsets daraus sind für die aktuelle Exe **nicht** gültig.

## Technische Basis

- **[Belegt]** Unreal Engine 5.8, Shipping-Build, monolithische Exe. Es gibt also keine Game-Module als einzelne DLLs.
- **[Belegt]** Keine PDB mitgeliefert. Sie steht zwar in `Manifest_DebugFiles_Win64.txt`, liegt aber nicht in der Installation.
- **[Belegt]** Paks sind **nicht verschlüsselt** (Pak-Footer und `.utoc` ohne Encryption-GUID, IoStore-Container komprimiert).
  Assets und Configs lassen sich also auslesen.
- **[Belegt]** In der Plugin-Liste steht kein Anti-Cheat.
- **[Belegt]** Plugins des Spiels: `OnlineSubsystemSteam`, `PixelSteam`, `PixelLeaderboards`, `PixelAchievements`,
  `PaperZD`, `FMODStudio`, `PoolManager`, `TwitchSDK`, `XInputDevice`, `Prismatiscape` und weitere.
- **[Belegt]** Steamworks-Laufzeit `Steamv164` (`steam_api64.dll`) ist vorhanden.
- **[Belegt]** Spielinhalte liegen unter anderem in `Content/DDSurvivors`, `Content/Map_Generation`, `Content/Characters`,
  `Content/Inventory`. Die Dateinamen enthalten keine Netzwerk-Assets.

## Netzwerk

- **[Belegt]** Die Exe enthält `IpNetDriver`, `IrisCore` (`net.Iris.*`) und `OnlineSubsystemSteam` mit Lobby-Code.
- **[Belegt]** Die Strings `SteamNetDriver` und `SteamSocketsNetDriver` kommen in der Exe nicht vor. Vorhanden sind nur
  `SteamSocketsIP` und `SteamSocketsP2P`. Für Verbindungen über Steam ist also vermutlich ein eigener Tunnel nötig.
- **[Belegt]** Die Exe enthält viele `OnRep_*`-Namen für Spielwerte: `OnRep_Health`, `OnRep_Armor`, `OnRep_Exp`,
  `OnRep_Level`, `OnRep_CurrentFuel`, `OnRep_DayNum`. Das passt zu Attributen im Gameplay Ability System.
- **[Alt]** 47 native RPCs in Spielklassen (von 3251 Funktionen), unter anderem:
  - `DDCharacter`: `StartFiring`, `FinishFiring`, `Server_SpawnBullet`, `ProxyRotateTowards`, `ProxySetAutoFire`
  - `DDCharacterBase`: `Multi_OnDie`
  - `Hands`: `ExecuteMeleeWeaponAttack`, `SetAutoFire`
  - `InventoryComponent`: `AddToBackpack`, `RemoveFromBackpack`, `AddToInventory`, `RepairItem` und weitere
  - `CharacterEffectManager`: `AddSpec`, `ApplyItemToBackpack`, `RemoveEffectsFromItem` und weitere
  - `LevelSystemComponent`: `Server_CheckExpRecipient`
  - `Crate`: `Multi_PlayOpenCrate`
  - `Weapon`: `TriggerWeaponShotFX`
  - `LootBase`: `Client_PlaySound`
- **[Hypothese]** Das Spiel wurde zumindest anfangs netzwerkfähig angelegt. Ob dieser Code noch funktioniert, ist offen.
  Er kann auch Altlast sein, die seit Monaten niemand mehr getestet hat.

## Lokales Coop

- **[Belegt]** Zentrale Klassen laut Exe-Strings: `LocalCoopSubsystem`, `CoopInputRouterSubsystem`, `CoopInputPreProcessor`,
  `CoopGameViewportClient`, `CoopUISessionHost`, `CoopInventoryCoordinatorComponent`, `CoopPlayerPresentationComponent`,
  `CoopSpeakerAssignment`, `LocalCoopRunPersistData`.
- **[Belegt]** Die Coop-Logik ist fest auf zwei Spieler ausgelegt: `ECoopInventoryLocationType::P1Backpack` und `P2Backpack`,
  `CoopP1Field`, `CoopP2Field`.
- **[Belegt]** Inventar-Aktionen laufen als Anfragen über `CoopInventoryTransferRequest` und melden Fehler wie `NotAuthoritative`.
  Rewards haben eine Warteschlange mit `AwaitingClaims`.
- **[Belegt]** Es gibt Coop-Konsolenvariablen: `dd.Coop.UIAuthorityMode`, `dd.Coop.UIHandoffMode`, `dd.Coop.SplitBackpackView`.
- **[Belegt]** Auch in Exe-Strings: `CoopDiagnostics.json`, `bEnableDevCoopButtons`, `OnCoopReconnectRequiredChanged`,
  `bCoopBlocksGameplay`. Was sie genau tun, ist noch offen.
- **[Hypothese]** „Authority“ meint hier, welcher lokale Spieler die UI bedient, nicht die Netzwerk-Autorität.
- **[Hypothese]** Das Anfrage-Muster im Inventar lässt sich gut auf Netzwerk-Anfragen übertragen.

Der Juli-Dump kennt nur vier Coop-Klassen. Der Großteil davon kam also erst nach Juli dazu und ist **nicht** im vorhandenen SDK.

## Systeme, die die Mod respektieren muss

- **[Belegt]** Bestenlisten: `PixelLeaderboards`, `DDLeaderboardSettings`
- **[Belegt]** Telemetrie und Berichte: `DDBalanceTelemetrySettings`, `DDCorruptedSaveAutoReportSettings`, `DDBugReportForm`, `PixelsplitFeedbackForm`
- **[Belegt]** Spielstände: `PixelsplitSaveSystem`, `LocalPlayerSaveGame`
- **[Belegt]** Zeitsteuerung: `PixelsplitTimescaleSystem`. Für Online-Spiel relevant, weil Pausen bei beiden gelten müssen.

## Allgemeines

- Entwickler laut SteamSpy: Pixelsplit. Version 1.0 mit fertigem Local-Coop erschien am 29.10.2026, also kurz vor Projektstart.
  Es ist mit weiteren Patches zu rechnen.
- Zu Mod-Support, Workshop oder offiziellem Online-Coop wurde nichts gefunden. Das heißt nicht, dass nichts geplant ist.
