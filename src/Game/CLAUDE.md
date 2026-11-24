# Game

Zugriff auf das Spiel: Adapter lesen, Patches und Hooks greifen ein. Gilt nur für Build `26786272`. Darf einbinden: `Core`.

## Muster

- Reine Logik in `*Policy.h` (oder `*Plan.h`) als freie Funktionen, getestet in `tests/GameTests` ohne Spiel.
- Hooks und Byte-Patches in `*Patches.cpp` bleiben dünn und rufen die Policy. Prüfbytes in `PatchSignatures.inc`, RVAs als Konstanten am Dateianfang.
- Neue Feldzugriffe über `Reflection::Property`/`Reflection::Function`, nicht über feste Offsets. Feste Offsets nur in `Offsets.h`, belegt in `docs/OFFSETS.md`, geprüft in `SdkChecks.cpp`.
- Speicher nur über `Memory::Read`/`ReadPointer`, Fehler ergibt `nullopt`.
- Nur auf dem Game Thread (`Core::GameThread().IsDrainThread()`), Ticks über `GameThread::AddTick`. Logs nur bei Änderung.

## Wo was liegt

- Spieler, Kamera, Bewegung: `PlayerPatches`, `RemoteP2Patches`, `CameraPatches`, `MovementPatches`, `LocalDevicePatches`
- Karte, Reise, Runde: `MapPatches`, `TravelPatches`, `PauseAdapter`, `ProgressAdapter`, `WidgetPatches`
- Inventar und Handel: `InventoryPatches`, `InventoryEdit*`, `RewardPatches`, `Airdrop*`, `ItemAbilityPatches`
- Gegner und Bosse: `EnemyTelegraphPatches`, `DamageNumberPatches`, `BossBarAdapter`, `ThreatAdapter`, `ProjectilePatches`
- Darstellung: `HealthBar`, `BusBar`, `LootBar`, `Blood`, `BodyPart`, `Exp`, `Facing`, `WalkAnimation`, `WeaponSound`
- Grundlagen: `Reflection`, `Memory`, `Objects`, `Offsets.h`, `AddressMap.h`, `GameThread`, `Patches.cpp` (Reihenfolge der Listen)

## Tests

`tests/GameTests` (doctest, ohne Spiel und SDK), lokal `bin\Release\GameTests.exe` oder `tools/Build-Mod.ps1 -Test`.

## Nicht tun

- Keine Offsets oder RVAs anderer Builds, keine Teile des SDK oder decompilierten Code ins Repo.
- Keinen zweiten Besitzer für gemeinsamen Zustand: Vergabe, Schaden, Gegner und Fortschritt bucht nur der Host.
- `At(Rva + Abstand)` nie, immer `At(Rva, Abstand)`.

## Mehr

[README.md](README.md) (Nachschlagewerk, nicht ganz lesen), Themen unter [docs/themen/](../../docs/themen/README.md).
