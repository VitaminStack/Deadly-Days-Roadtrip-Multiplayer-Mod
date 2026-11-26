// Erzeugt von tools/patch_signatures.py: Referenz-RVA, Build (0 = alle), Byte-Muster (?? beliebig), Lage der Adresse
// im Muster, Name. Aufsteigend nach RVA, je RVA erst Build 0, dann die eigenen Zeilen einzelner Builds.
// Die Mod prueft das Muster an der erwarteten Stelle, das Werkzeug zusaetzlich die Eindeutigkeit in der Exe.
{0x01272700, 0, "48 89 6C 24 18 48 89 74 24 20 57 41 54 41 56 48 83 EC 30 41 8B 30 4D 8B F0 0F B7 E9 4C 8B E2 8B C5 C1 ED 08", 0, "ArrayResizeGrow"}, // CameraPatches.cpp
{0x012BC260, 0, "48 89 5C 24 08 57 48 83 EC 30 33 FF 48 8D 41 0C 48 8B D9 89 79 08 39 38 74 ?? 4C 8B C9 48 89 44 24 28 B9 14 00 00 00", 0, "DamageInfoMapDestruct"}, // DamageNumberPatches.cpp
{0x012D6D80, 0, "40 53 48 8B 19 48 8B C1 4C 63 CA 45 85 C0 0F 84 ?? ?? ?? ?? 4D 8B D1 48 89 7C 24 10 41 8B C9 49 C1 E2 04 41 BB 01 00 00 00 41 D3 C3", 0, "ControllerSparseRemove"}, // PlayerPatches.cpp
{0x012F85E0, 0, "48 85 C9 74 ?? 53 48 83 EC 20 48 8B D9 48 8B 0D ?? ?? ?? ??", 0, "FreeArray"}, // ClientPatches.cpp
{0x01304360, 0, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B F1 41 8B D8 48 8B 0D ?? ?? ?? ?? 48 8B FA", 0, "MemoryRealloc"}, // RouteMapPatches.cpp
{0x01344650, 0, "45 85 C0 0F 84 ?? ?? ?? ?? 48 89 5C 24 10 57 48 8B 39 48 8B C1 4C 63 D2 41 BB 01 00 00 00 41 8B CA 48 89 74 24 10 41 D3 C3 41 8B D8 4B 8D 0C 92 4C 8D 04 8F 83 78 34 00 74 ??", 0, "MapSparseRemove"}, // RemoteP2Patches.cpp
{0x01512430, 0, "48 8B 01 48 FF A0 F8 01 00 00 CC CC CC CC CC CC 40 53 48 83 EC 40 0F 10 02 33 C0", 0, "DefaultNetworkingSupport"}, // InventoryPatches.cpp
{0x024E95F0, 0, "4C 8B DC 53 55 56 57 41 56 48 81 EC A0 00 00 00 48 8B 05 ?? ?? ?? ??", 0, "RegistrationBroadcast"}, // PlayerPatches.cpp
{0x0288B6E0, 0, "40 55 53 57 41 55 48 8D AC 24 78 FF FF FF 48 81 EC 88 01 00 00", 0, "CreateChildActor"}, // MapPatches.cpp
{0x0289DD30, 0, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B F9 48 81 C1 50 02 00 00", 0, "IsChildActorReplicated"}, // MapPatches.cpp
{0x02A67AC0, 0, "40 53 55 57 41 54 41 55 41 56 41 57 48 81 EC B0 00 00 00", 0, "BeginDeferredActorSpawnFromClass"}, // ClientPatches.cpp
{0x02CF37FE, 0, "0F 28 C6 F2 0F 5C C1 66 0F 2F C7 0F 86 ?? ?? ?? ??", 0, "NetGuidExpiry"}, // ClientPatches.cpp
{0x02CF3E08, 0, "8B 44 24 4C 48 8D 4C 24 48 F7 D0 21 44 24 58 E8 ?? ?? ?? ?? 48 8B 44 24 50 44 8B 4C 24 5C 44 3B 48 18 0F 8C ?? ?? ?? ?? 80 7C 24 78 00 44 0F 28 84 24 30 02 00 00 0F 28 BC 24 40 02 00 00", 0, "NetGuidNext"}, // ClientPatches.cpp
{0x03147390, 0, "4C 8B 0A 4C 8B D1 4D 85 C9", 0, "FindTimer"}, // InventoryPatches.cpp
{0x0314D280, 0, "48 89 5C 24 08 48 89 54 24 10 57 48 83 EC 20 48 8B C2 48 8B F9", 0, "ClearTimer"}, // InventoryPatches.cpp
{0x0314D6A0, 0, "40 55 53 48 8D 6C 24 B1 48 81 EC A8 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 37 0F B6 45 D7", 0, "SetNextTick"}, // InventoryPatches.cpp
{0x03262A00, 0, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B 81 B0 12 00 00 41 8B F9", 0, "SetClientTravel"}, // TravelPatches.cpp
{0x04B68BE0, 0, "48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 48 8B 99 80 10 00 00", 0, "AttributeLookupFunction"}, // ClientPatches.cpp
{0x04BC6130, 0, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 49 8B F8 48 8B DA 48 8B F1 E8 ?? ?? ?? ?? 48 8B C8 48 8B 10 FF 92 D8 02 00 00 48 85 C0", 0, "ExecuteCueNonReplicated"}, // BodyPartPatches.cpp
{0x04BDE5E0, 0, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 30 0F 29 74 24 20 4D 8B F0 4C 8B FA 48 8B E9 E8 ?? ?? ?? ??", 0, "SetNumericValueChecked"}, // HealthBarPatches.cpp
{0x05659A70, 0, "48 89 5C 24 10 48 89 4C 24 08 57 48 83 EC 20 48 8B D9 48 8D 54 24 30", 0, "DamageInfoConstruct"}, // DamageNumberPatches.cpp
{0x05667FB5, 0, "48 FF A0 68 07 00 00 CC 48 8B 42 20 45 33 C0 48 85 C0 41 0F 95 C0 4C 03 C0 4C 89 42 20 E9 ?? ?? ?? ?? CC CC CC", 29, "At_5667FB5"}, // ClientPatches.cpp
{0x056ADF20, 0, "85 D2 74 ?? 48 89 5C 24 08 57 48 83 EC 20 8B FA 48 8D 59 10 8B 53 08 48 8B 0B E8 ?? ?? ?? ?? 90 48 8B 0B 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 83 C3 20 83 EF 01 75 ?? 48 8B 5C 24 30 48 83 C4 20 5F C3 CC", 0, "DestructGroups"}, // RouteMapPatches.cpp
{0x056ADFB0, 0, "85 D2 74 ?? 48 89 5C 24 08 57 48 83 EC 20 8B FA 48 8D 59 08 48 8B 4B 28", 0, "DestructRoutes"}, // RouteMapPatches.cpp
{0x056C6B4B, 0, "48 8B CF E8 ?? ?? ?? ?? 48 8B 5C 24 30 48 83 C4 20 5F C3 CC CC CC CC CC 48 8B 42 20 45 33 C0 48 85 C0 41 0F 95 C0 4C 03 C0 4C 89 42 20 E9 ?? ?? ?? ?? CC CC CC CC CC CC 48 89 5C 24 08 48 89 74 24 18 57 48 83 EC 20 33 FF 48 8B DA 89 7C 24 38 48 8B F1", 3, "RoundChangedThunk"}, // RoundStatsPatches.cpp
{0x05708220, 0, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B FA 48 8B D9 48 8B 15 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B C8 48 8B F0 E8 ?? ?? ?? ?? 4C 8B 0B 48 8B CB F6 80 D8 00 00 00 80 75 ?? 4C 8D 44 24 40 48 89 7C 24 40 48 8B D6 41 FF 91 50 02 00 00 48 8B 5C 24 30 48 8B 74 24 38 48 83 C4 20 5F C3 48 8B D7 48 8B 5C 24 30 48 8B 74 24 38 48 83 C4 20 5F 49 FF A1 40 07 00 00 CC CC CC CC CC CC CC CC 40 53 48 83 EC 20 48 89 54 24 30 48 8B D9 48 8B 15 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B 13 4C 8D 44 24 30 48 8B CB 4C 8B 8A 50 02 00 00", 0, "ReloadSoundEvent"}, // WeaponSoundPatches.cpp
{0x057487D0, 0, "48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 40 48 8B 02 4C 8D 72 08 48 8D 71 08", 0, "RouteAssign"}, // RouteMapPatches.cpp
{0x057BED90, 0, "40 53 56 57 41 55 48 83 EC 38 48 89 6C 24 68", 0, "ControllerSetAdd"}, // PlayerPatches.cpp
{0x057CC665, 0, "E8 ?? ?? ?? ?? 90 0F B6 45 37 48 8D 15 ?? ?? ?? ?? 48 8B 14 C2 48 8D 05 ?? ?? ?? ?? 48 3B D0 75 ?? 48 83 7D 07 00 74 ?? 48 8B 45 2F 48 8D 4D 17 48 85 C0 48 0F 45 C8 48 8B 01 FF 50 10 90 EB ?? 48 8D 4D 07 FF D2 90 48 83 7D D7 00 74 ?? 48 8B 45 FF 48 8D 4D E7 48 85 C0 48 0F 45 C8 48 8B 01 FF 50 10 90 48 8B 4D 47 48 33 CC E8 ?? ?? ?? ?? 4C 8D 9C 24 C0 00 00 00 49 8B 5B 18 49 8B 7B 20 49 8B E3 5D C3 CC CC CC CC CC CC 48 89 5C 24 10 48 89 6C 24 20 56", 0, "ScheduleCall"}, // InventoryPatches.cpp
{0x057D1DA0, 0, "40 53 48 83 EC 20 48 8B D9 E8 ?? ?? ?? ?? 48 8B 8B D8 00 00 00 48 85 C9 74 ?? 48 83 C1 60", 0, "DetectorEndPlay"}, // InventoryPatches.cpp
{0x057D59E0, 0, "40 53 48 83 EC 20 8B 0D ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 BA FC 1A 00 00 48 8B 04 C8 8B 04 02 39 05 ?? ?? ?? ?? 7F ?? 48 8D 05 ?? ?? ?? ?? 48 83 C4 20 5B C3 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 3D ?? ?? ?? ?? FF 75 ?? 41 B9 01 00 00 00 4C 8D 05 ?? ?? ?? ?? BA 05 00 00 00 48 8D 4C 24 30 E8 ?? ?? ?? ?? 48 8B 18 33 C9", 0, "GetArmorAttribute"}, // HealthBarPatches.cpp
{0x057DCB80, 0, "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 60 48 8B F2 48 8B F9 44 8B 05 ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 B9 FC 1A 00 00 4A 8B 04 C0 8B 04 01 39 05 ?? ?? ?? ?? 7F ?? 48 8B CF E8 ?? ?? ?? ?? 48 8B D8 48 8B 15 ?? ?? ?? ?? 48 8D 4C 24 20 E8 ?? ?? ?? ?? 90 4C 8D 87 B0 01 00 00 48 8D 8B D8 08 00 00 4C 8B CE 48 8B D0 E8 ?? ?? ?? ?? 90 48 8B 4C 24 40 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 8B 4C 24 20 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 8B 5C 24 78 48 8B B4 24 80 00 00 00 48 83 C4 60 5F C3 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 3D ?? ?? ?? ?? FF 75 ?? 41 B9 01 00 00 00 4C 8D 05 ?? ?? ?? ?? BA 05 00 00 00", 0, "OnRepArmor"}, // HealthBarPatches.cpp
{0x057DD950, 0, "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 60 48 8B F2 48 8B F9 44 8B 05 ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 B9 FC 1A 00 00 4A 8B 04 C0 8B 04 01 39 05 ?? ?? ?? ?? 7F ?? 48 8B CF E8 ?? ?? ?? ?? 48 8B D8 48 8B 15 ?? ?? ?? ?? 48 8D 4C 24 20 E8 ?? ?? ?? ?? 90 4C 8D 87 A0 00 00 00 48 8D 8B D8 08 00 00 4C 8B CE 48 8B D0 E8 ?? ?? ?? ?? 90 48 8B 4C 24 40 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 8B 4C 24 20 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 8B 5C 24 78 48 8B B4 24 80 00 00 00 48 83 C4 60 5F C3 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 3D ?? ?? ?? ?? FF 75 ?? 41 B9 01 00 00 00 4C 8D 05 ?? ?? ?? ?? BA 06 00 00 00", 0, "OnRepHealth"}, // HealthBarPatches.cpp
{0x057DDEA0, 0, "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 60 48 8B F2 48 8B F9 44 8B 05 ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 B9 FC 1A 00 00 4A 8B 04 C0 8B 04 01 39 05 ?? ?? ?? ?? 7F ?? 48 8B CF E8 ?? ?? ?? ?? 48 8B D8 48 8B 15 ?? ?? ?? ?? 48 8D 4C 24 20 E8 ?? ?? ?? ?? 90 4C 8D 87 90 01 00 00 48 8D 8B D8 08 00 00 4C 8B CE 48 8B D0 E8 ?? ?? ?? ?? 90 48 8B 4C 24 40 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 8B 4C 24 20 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 8B 5C 24 78 48 8B B4 24 80 00 00 00 48 83 C4 60 5F C3 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 3D ?? ?? ?? ?? FF 75 ?? 41 B9 01 00 00 00 4C 8D 05 ?? ?? ?? ?? BA 08 00 00 00", 0, "OnRepMaxArmor"}, // HealthBarPatches.cpp
{0x057DDFB0, 0, "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 60 48 8B F2 48 8B F9 44 8B 05 ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 B9 FC 1A 00 00 4A 8B 04 C0 8B 04 01 39 05 ?? ?? ?? ?? 7F ?? 48 8B CF E8 ?? ?? ?? ?? 48 8B D8 48 8B 15 ?? ?? ?? ?? 48 8D 4C 24 20 E8 ?? ?? ?? ?? 90 4C 8D 47 30 48 8D 8B D8 08 00 00 4C 8B CE 48 8B D0 E8 ?? ?? ?? ?? 90 48 8B 4C 24 40 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 8B 4C 24 20 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 8B 5C 24 78 48 8B B4 24 80 00 00 00 48 83 C4 60 5F C3 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 3D ?? ?? ?? ?? FF 75 ?? 41 B9 01 00 00 00 4C 8D 05 ?? ?? ?? ?? BA 09 00 00 00", 0, "OnRepMaxHealth"}, // HealthBarPatches.cpp
{0x057DE1C0, 0, "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 60 48 8B F2 48 8B F9 44 8B 05 ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 B9 FC 1A 00 00 4A 8B 04 C0 8B 04 01 39 05 ?? ?? ?? ?? 7F ?? 48 8B CF E8 ?? ?? ?? ?? 48 8B D8 48 8B 15 ?? ?? ?? ?? 48 8D 4C 24 20 E8 ?? ?? ?? ?? 90 4C 8D 47 30 48 8D 8B D8 08 00 00 4C 8B CE 48 8B D0 E8 ?? ?? ?? ?? 90 48 8B 4C 24 40 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 8B 4C 24 20 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 8B 5C 24 78 48 8B B4 24 80 00 00 00 48 83 C4 60 5F C3 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 3D ?? ?? ?? ?? FF 75 ?? 41 B9 01 00 00 00 4C 8D 05 ?? ?? ?? ?? BA 0A 00 00 00", 0, "OnRepMaxStamina"}, // HealthBarPatches.cpp
{0x057DEF80, 0, "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 60 48 8B F2 48 8B F9 44 8B 05 ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 B9 FC 1A 00 00 4A 8B 04 C0 8B 04 01 39 05 ?? ?? ?? ?? 7F ?? 48 8B CF E8 ?? ?? ?? ?? 48 8B D8 48 8B 15 ?? ?? ?? ?? 48 8D 4C 24 20 E8 ?? ?? ?? ?? 90 4C 8D 47 50 48 8D 8B D8 08 00 00 4C 8B CE 48 8B D0", 0, "OnRepStamina"}, // HealthBarPatches.cpp
{0x057E09DB, 0, "E8 ?? ?? ?? ?? EB ?? E8 ?? ?? ?? ?? 48 8B D0 49 8B CF E8 ?? ?? ?? ?? 84 C0 74 ?? F3 0F 11 7C 24 30 48 8D 8D C0 01 00 00 EB ?? E8 ?? ?? ?? ??", 0, "ClampToMaxCall"}, // HealthBarPatches.cpp
{0x05838370, 0, "40 57 48 83 EC 20 80 B9 10 01 00 00 00", 0, "ApplyInitial"}, // CharacterPatches.cpp
{0x0583C03C, 0, "48 8B 07 48 8D 54 24 60 48 8B CF FF 90 D8 06 00 00", 0, "ComponentControllerRead"}, // ClientPatches.cpp
{0x0583C1B0, 0, "48 8B 9C 24 B8 00 00 00 48 81 C4 90 00 00 00 5F 5E 5D C3 CC CC CC CC CC CC CC CC CC CC CC CC CC 48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B F1", 0, "ComponentControllerExit"}, // ClientPatches.cpp
{0x05841F40, 0, "48 89 5C 24 10 48 89 74 24 18 55 57 41 54 41 56 41 57 48 8D 6C 24 C9 48 81 EC B0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 27 4C 8B FA 48 8B D9 45 33 E4 4C 39 A1 18 01 00 00", 0, "ApplyDefinition"}, // CharacterPatches.cpp
{0x0584F082, 0, "48 8B C8 48 8B 00 FF 90 30 03 00 00 8B E8", 0, "RunEndLocalPlayerSite"}, // PlayerPatches.cpp
{0x0584F090, 0, "48 8B BE E8 00 00 00 8B D5 48 8B CF", 0, "RunEndContinue"}, // PlayerPatches.cpp
{0x0585BFF0, 0, "40 53 48 83 EC 60 48 8B D9 E8 ?? ?? ?? ?? 48 8B 83 D8 00 00 00 45 33 C9", 0, "CameraFollowTick"}, // PlayerPatches.cpp
{0x05872149, 0, "48 89 9F 88 01 00 00 48 8D 15 ?? ?? ?? ?? 48 8D 4D 40", 0, "ReaperGameModeStore"}, // ClientPatches.cpp
{0x05872150, 0, "48 8D 15 ?? ?? ?? ?? 48 8D 4D 40 E8 ?? ?? ?? ?? 48 8D 8B 40 03 00 00", 0, "ReaperBindContinue"}, // ClientPatches.cpp
{0x0587219B, 0, "48 8B 87 B0 00 00 00 48 85 C0 75 ?? 48 8B CF E8 ?? ?? ?? ?? 48 8B C8 E8 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? 48 89 4D F0 44 89 65 F8 0F 28 45 F0 66 0F 7F 45 F0", 0, "ReaperBindSkip"}, // ClientPatches.cpp
{0x05891B00, 0, "48 8B C4 48 89 58 08 48 89 70 18 57 48 81 EC 00 01 00 00 0F 29 70 E8", 0, "SpawnCircleTelegraph"}, // EnemyTelegraphPatches.cpp
{0x058A5090, 0, "48 89 5C 24 08 57 48 83 EC 70 48 8B D9 B2 01 48 8B 89 38 02 00 00", 0, "ActivateLineTelegraph"}, // EnemyTelegraphPatches.cpp
{0x058A7736, 0, "0F 84 ?? ?? ?? ?? 80 3D ?? ?? ?? ?? 00 48 89 74 24 50 74 ??", 0, "SneakSameTarget"}, // ClientPatches.cpp
{0x058A773C, 0, "80 3D ?? ?? ?? ?? 00 48 89 74 24 50 74 ?? 48 8B CB", 0, "SneakRetry"}, // ClientPatches.cpp
{0x058A77CE, 0, "48 83 BF E8 00 00 00 00 74 ?? 48 85 DB", 0, "At_58A77CE"}, // ClientPatches.cpp
{0x058A77DD, 0, "B0 01 48 8B 5C 24 58 48 83 C4 40 5F C3 48 8B 5C 24 58 32 C0 48 83 C4 40 5F C3 CC CC CC CC CC CC CC CC CC 48 89 5C 24 10 48 89 6C 24 18 56 57 41 56", 0, "SneakReady"}, // ClientPatches.cpp
{0x058A8530, 0, "E8 ?? ?? ?? ?? F3 0F 11 87 14 03 00 00 48 8B CF", 0, "At_58A8530"}, // ClientPatches.cpp
{0x058AB000, 0, "40 53 48 81 EC D0 00 00 00 48 8B 81 38 02 00 00", 0, "CalculateLineTelegraphEndPoint"}, // EnemyTelegraphPatches.cpp
{0x058ACDF0, 0, "48 8B 09 48 8B 01 48 FF A0 E8 04 00 00 CC CC CC E9 ?? ?? ?? ?? CC", 0, "At_58ACDF0"}, // ClientPatches.cpp
{0x058B25E0, 0, "40 53 57 48 83 EC 28 80 B9 69 02 00 00 00", 0, "EndTackle"}, // EnemyTelegraphPatches.cpp
{0x058B31AE, 0, "E8 ?? ?? ?? ?? F3 0F 11 87 14 03 00 00 40 38 AF 18 03 00 00", 0, "At_58B31AE"}, // ClientPatches.cpp
{0x058B33B0, 0, "48 89 5C 24 10 57 48 83 EC 60 0F 29 74 24 50 0F 28 F1 48 8B F9", 0, "ExecuteTackle"}, // EnemyTelegraphPatches.cpp
{0x058B4250, 0, "48 89 74 24 18 57 48 83 EC 30 48 8B 91 88 03 00 00", 0, "FinalizeCopyItem"}, // RewardPatches.cpp
{0x058B61A0, 0, "48 89 5C 24 08 57 48 83 EC 40 0F 29 74 24 30 0F 57 F6 48 8B D1", 0, "ExpAggregate"}, // ClientPatches.cpp
{0x058B61DB, 0, "48 8B 89 D8 07 00 00 48 83 E9 80 E8 ?? ?? ?? ??", 0, "At_58B61DB"}, // ClientPatches.cpp
{0x058BD200, 0, "40 53 48 81 EC B0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 A0 00 00 00 48 8B D9 48 8B 81 B0 00 00 00 48 85 C0", 0, "BlockadeLoadingFinished"}, // ClientPatches.cpp
{0x058C0BA0, 0, "40 53 48 83 EC 70 48 8B D9 E8 ?? ?? ?? ?? 0F 57 C9 4C 8D 4C 24 20", 0, "UpdateBounds"}, // ExpPatches.cpp
{0x058C0C90, 0, "48 89 5C 24 18 55 56 57 48 83 EC 30 FF 41 5C", 0, "ReceiveExpImpl"}, // ExpPatches.cpp
{0x058C3590, 0, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 40 0F 29 74 24 30", 0, "SpawnRequest"}, // ExpPatches.cpp
{0x058C9E20, 0, "48 8B C4 48 89 58 10 48 89 70 18 48 89 78 20 55 41 54 41 55 41 56 41 57 48 8D A8 78 FE FF FF 48 81 EC 60 02 00 00 0F 29 70 C8", 0, "ReaperSpawn"}, // ClientPatches.cpp
{0x058CFD10, 0, "40 57 48 83 EC 40 48 8B 81 68 08 00 00", 0, "TryBuyItem"}, // RewardPatches.cpp
{0x058D0FC7, 0, "49 8B 86 E8 02 00 00 48 89 44 24 58 80 3D ?? ?? ?? ?? 00", 0, "SpawnerPlayerRead"}, // PlayerPatches.cpp
{0x058D14B7, 0, "32 DB C7 45 48 00 00 00 00 48 8B 4D 40", 0, "SpawnerNoPlayerExit"}, // PlayerPatches.cpp
{0x058D3530, 0, "48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 56 41 57 48 83 EC 30 4C 8B F2 49 8B D8 48 8D 54 24 20 48 8B F1 E8 ?? ?? ?? ?? 48 8B 7C 24 28 48 85 FF 74 ?? 41 8B 06 89 07 48 8B 03 48 89 47 04 C7 47 0C FF FF FF FF 8B 46 08 2B 46 34 44 8B 7C 24 20 8B 2F", 0, "MapAdd"}, // RemoteP2Patches.cpp
{0x058D56F0, 0, "5F C3 CC 40 53 48 83 EC 30 8B 41 08 4D 8B D0 48 8B D9 44 8B DA 3B 41 34 74 ?? 4C 8B 49 40 48 8D 41 38 8B 49 48 4D 85 C9 4C 0F 44 C8 FF C9 49 23 CB 41 8B 04 89 83 F8 FF 74 ?? 41 8B 10 4C 8B 03 0F 1F 00 48 98 48 8D 04 40 48 C1 E0 05 49 03 C0 39 10 74 ?? 8B 40 58 83 F8 FF", 3, "PlayerFindOrAdd"}, // RoundStatsPatches.cpp
{0x058DBF90, 0, "48 89 5C 24 08 48 89 74 24 10 55 57 41 54 41 56 41 57 48 8B EC 48 83 EC 70 41 8B F0 8B DA 4C 8B F1", 0, "AssignDevice"}, // LocalDevicePatches.cpp
{0x058E91A0, 0, "80 B9 88 01 00 00 00 74 ?? 41 8D 40 01", 0, "DraftAssignDevice"}, // LocalDevicePatches.cpp
{0x058EBFD0, 0, "4C 89 44 24 18 53 48 83 EC 50 0F B6 DA", 0, "StatFind"}, // RoundStatsPatches.cpp
{0x058EC080, 0, "48 89 5C 24 20 55 56 57 48 83 EC 30 80 B9 10 04 00 00 00", 0, "FinishMission"}, // ClientPatches.cpp
{0x058EF260, 0, "48 83 EC 28 48 81 C1 98 01 00 00 E8 ?? ?? ?? ??", 0, "GetGameStatImpl"}, // RoundStatsPatches.cpp
{0x058EFD10, 0, "48 8B C4 48 89 58 20 48 89 50 10 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 88 FB FF FF 48 81 EC 40 05 00 00", 0, "GetMissionStructure"}, // RouteMapPatches.cpp
{0x058F1F10, 0, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 8B 81 08 01 00 00 48 8B D9", 0, "GetController"}, // LocalDevicePatches.cpp
{0x058F4420, 0, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 30 48 8B E9 49 8B F9 48 81 C1 98 01 00 00", 0, "IncrementImpl"}, // RoundStatsPatches.cpp
{0x058F4520, 0, "85 D2 78 ?? 48 89 5C 24 08 48 89 74 24 18", 0, "IncrementPlayer"}, // RoundStatsPatches.cpp
{0x058F45A0, 0, "4C 8B DC 49 89 5B 10 4D 89 43 18 55 56 57 48 83 EC 60 48 8B 05 ?? ?? ?? ??", 0, "StatIncrement"}, // RoundStatsPatches.cpp
{0x058FC4B0, 0, "48 89 5C 24 08 57 48 83 EC 20 0F B6 DA 48 8B F9 E8 ?? ?? ?? ?? 8D 43 FE 3C 01", 0, "RoundChanged"}, // RoundStatsPatches.cpp
{0x058FF2E0, 0, "48 89 5C 24 18 48 89 74 24 20 57 48 83 EC 20 48 8B DA 48 8B F1 48 85 D2 74 ?? 33 C9 E8 ?? ?? ?? ?? 48 8B 53 10 4C 8D 40 30 48 63 40 38 3B 42 38 7F ?? 48 8B C8 48 8B 42 30 4C 39 04 C8 75 ?? 4C 8B D3 EB ?? 45 33 D2 48 B9 CD 8C 55 ED D7 AF 51 FF 4C 89 54 24 38", 0, "RegisterController"}, // PlayerPatches.cpp
{0x058FF3DD, 0, "E8 ?? ?? ?? ?? 48 8D 8E 40 03 00 00 48 89 5C 24 30", 0, "At_58FF3DD"}, // PlayerPatches.cpp
{0x058FF3F3, 0, "E8 ?? ?? ?? ?? 48 8B 5C 24 40 48 8B 74 24 48 48 83 C4 20 5F C3 CC CC CC CC CC CC CC CC 48 89 5C 24 10 48 89 74 24 18 4C 89 74 24 20 55 48 8B EC 48 83 EC 70 8B 51 48 48 8D 71 48 4C 8D 71 38 48 8B D9", 0, "At_58FF3F3"}, // PlayerPatches.cpp
{0x059056C0, 0, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 8B 02 48 8B DA 48 8B F1 39 81 A8 02 00 00", 0, "SetCurrentRoute"}, // RouteMapPatches.cpp
{0x05906F08, 0, "48 8B 08 48 8B 91 30 03 00 00 48 8B C8 FF D2 8B D0 48 8B CB", 0, "SpawnNullSite"}, // PlayerPatches.cpp
{0x05907073, 0, "4C 8B C5 48 8B D7 48 8B CE E8 ?? ?? ?? ?? 48 8B 8C 24 D0 00 00 00 48 33 CC", 0, "SpawnSkipTarget"}, // PlayerPatches.cpp
{0x059070B0, 0, "89 54 24 10 55 53 56 57 41 56 48 8B EC 48 83 EC 40", 0, "SpawnLocal"}, // LocalDevicePatches.cpp
{0x05909FE0, 0, "48 89 5C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 30 0F B6 EA 48 8B F1 E8 ?? ?? ?? ??", 0, "TriggerEndRound"}, // ClientPatches.cpp
{0x0590D3BA, 0, "48 85 DB 74 ?? 40 84 ED 74 ?? B0 01 EB ??", 0, "DeviceCheckSite"}, // RemoteP2Patches.cpp
{0x0590D3BF, 0, "40 84 ED 74 ?? B0 01 EB ?? 32 C0 44 3A F0", 0, "SoloLossTest"}, // RemoteP2Patches.cpp
{0x0590D3C4, 0, "B0 01 EB ?? 32 C0 44 3A F0 0F 84 ?? ?? ?? ?? 88 87 89 01 00 00", 0, "NeedReconnect"}, // RemoteP2Patches.cpp
{0x0590D930, 0, "48 83 EC 38 48 8D 44 24 48 48 89 44 24 28 48 8D 54 24 20 48 8D 05 ?? ?? ?? ?? 48 89 44 24 20 E8 ?? ?? ?? ?? 48 83 C4 38 C3 CC CC CC CC CC CC CC 40 53 48 83 EC 20 48 8B D9 E8 ?? ?? ?? ?? 80 BB 10 04 00 00 00 74 ?? F3 0F 10 83 88 03 00 00 F2 0F 10 88 68 08 00 00", 0, "WasFinalMission"}, // RoundStatsPatches.cpp
{0x05938CF0, 0, "40 57 48 83 EC 20 48 8B F9 80 FA 01", 0, "RunSaveRoundChanged"}, // ClientPatches.cpp
{0x05940CB0, 0, "40 53 48 83 EC 20 48 83 3A 00 48 8B D9 74 ?? 48 8B 4A 08", 0, "RunSaveResave"}, // ClientPatches.cpp
{0x05943B53, 0, "74 ?? 8B 41 64 44 8B D7 39 41 38", 0, "ThreatFreezeBranch"}, // ClientPatches.cpp
{0x0595FB80, 0, "48 89 5C 24 08 55 56 57 48 81 EC C0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 B0 00 00 00 49 8B F9 41 0F B6 E8 48 8B F2 33 C9", 0, "CreateItemInstance"}, // RewardPatches.cpp
{0x0596E480, 0, "48 89 5C 24 08 57 48 83 EC 20 45 33 C0 48 8B DA 48 8B F9", 0, "HandleItemSold"}, // RewardPatches.cpp
{0x059702D0, 0, "E9 ?? ?? ?? ?? CC CC CC CC CC CC CC CC CC CC CC 48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 20 8B 41 08 4C 8B F2 48 8B F9 3B 41 0C 49 63 F0 4C 8D 41 0C 75 ??", 0, "InventoryReadyForReplication"}, // InventoryPatches.cpp
{0x0597D860, 0, "48 89 5C 24 18 48 89 54 24 10 55 56 57 48 8D 6C 24 B9 48 81 EC 90 00 00 00 48 8B FA", 0, "SeamlessTravelToLevel"}, // TravelPatches.cpp
{0x059808C0, 0, "48 89 5C 24 18 48 89 54 24 10 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 50 45 8B E8 4C 8B E2", 0, "SetRewardItems"}, // RewardPatches.cpp
{0x059878D0, 0, "48 89 5C 24 08 4C 89 4C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 50 0F 29 74 24 40 4D 8B E8", 0, "UpgradeItems"}, // RewardPatches.cpp
{0x05998940, 0, "48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 F0 FE FF FF 48 81 EC 10 02 00 00 0F 29 B4 24 00 02 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 F0 00 00 00 4C 8B EA 48 89 55 C0", 0, "ClearSpawnArea"}, // PlayerPatches.cpp
{0x0599B984, 0, "48 8B 83 B8 01 00 00 48 85 C0 74 ?? 0F 10 88 10 02 00 00 0F 11 4D 20 F2 0F 10 80 20 02 00 00", 0, "LootPawnRead"}, // PlayerPatches.cpp
{0x0599B9A5, 0, "0F 10 05 ?? ?? ?? ?? 0F 11 45 20 F2 0F 10 05 ?? ?? ?? ?? F2 0F 11 85 B0 00 00 00", 0, "LootNoRootTarget"}, // PlayerPatches.cpp
{0x059B1F50, 0, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 30 48 8B F2 48 8B F9 E8 ?? ?? ?? ??", 0, "ItemLoot"}, // InventoryPatches.cpp
{0x059B2140, 0, "48 89 5C 24 10 57 48 83 EC 20 48 8B 92 A8 00 00 00", 0, "LootBaseLoot"}, // InventoryPatches.cpp
{0x059B23B0, 0, "48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 56 41 57 48 83 EC 20 48 8B FA", 0, "ScrapLoot"}, // InventoryPatches.cpp
{0x059BED47, 0, "E8 ?? ?? ?? ?? 49 89 86 58 01 00 00 48 85 C0", 0, "AttributeLookupCallSite"}, // ClientPatches.cpp
{0x059BF4BA, 0, "48 8B 8D A0 00 00 00 48 33 CC E8 ?? ?? ?? ?? 4C 8D 9C 24 58 02 00 00 41 0F 28 73 E8 41 0F 28 7B D8 45 0F 28 43 C8 45 0F 28 4B B8 45 0F 28 53 A8 45 0F 28 5B 98 45 0F 28 63 88 45 0F 28 AB 78 FF FF FF 45 0F 28 B3 68 FF FF FF 45 0F 28 BB 58 FF FF FF 49 8B E3 41 5F 41 5E 41 5D 41 5C 5F 5E 5B 5D C3 CC CC CC CC 48 89 5C 24 10 48 89 6C 24 18 56 57", 0, "AttributeTickExit"}, // ClientPatches.cpp
{0x059CAD40, 0, "48 89 5C 24 10 48 89 74 24 20 55 57 41 56 48 8B EC 48 83 EC 60 48 8B F2", 0, "CanBusTakeOff"}, // ClientPatches.cpp
{0x059CB87F, 0, "E8 ?? ?? ?? ?? 48 8B 8B F0 02 00 00 84 C0 74 ?? 48 85 C9 0F 85 ?? ?? ?? ??", 0, "At_59CB87F"}, // ClientPatches.cpp
{0x059D2920, 0, "48 8B 51 20 48 8B 49 18 E9 ?? ?? ?? ??", 0, "MapGenLambdaExecute"}, // InventoryPatches.cpp
{0x059D2AD0, 0, "48 83 EC 28 48 8B 51 20 48 8B 49 18", 0, "MapGenLambdaExecuteIfSafe"}, // InventoryPatches.cpp
{0x059E78AA, 0, "49 8B 9E C8 00 00 00 48 8B 03 48 8B B8 D0 06 00 00", 0, "MapGenHostObjectRead"}, // ClientPatches.cpp
{0x059E7957, 0, "0F 28 B4 24 80 00 00 00 48 81 C4 90 00 00 00 41 5E 5F 5E 5D", 0, "MapGenHostObjectExit"}, // ClientPatches.cpp
{0x059F61F0, 0, "48 89 5C 24 18 55 56 57 41 56 41 57 48 8D AC 24 20 FC FF FF 48 81 EC E0 04 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 D0 03 00 00 4C 8B F2", 0, "SetupPcgVolume"}, // MapPatches.cpp
{0x059F7220, 0, "40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 E1 48 81 EC E8 00 00 00 48 8B F1 45 33 FF", 0, "StartMapGeneration"}, // MapPatches.cpp
{0x05A144D0, 0, "48 89 5C 24 18 48 89 74 24 20 55 57 41 54 41 56 41 57 48 8D 6C 24 C9 48 81 EC F0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 27 48 8B FA 48 89 55 C7 45 33 FF 44 89 7D 87", 0, "CameraPlayersFunction"}, // CameraPatches.cpp
{0x05A1E76C, 0, "83 B8 C8 02 00 00 01 0F 8E ?? ?? ?? ?? 80 3D ?? ?? ?? ?? 00", 0, "RestrictorCountSite"}, // CameraPatches.cpp
{0x05A1E779, 0, "80 3D ?? ?? ?? ?? 00 48 89 5C 24 70 48 8B 9A E8 02 00 00", 0, "RestrictorContinue"}, // CameraPatches.cpp
{0x05A1E828, 0, "48 8B 6C 24 78 48 8B B4 24 80 00 00 00 48 83 C4 60 5F C3 CC CC CC CC CC 48 89 5C 24 18 48 89 6C 24 20", 0, "RestrictorSkip"}, // CameraPatches.cpp
{0x05A25B30, 0, "48 8B C4 48 89 58 18 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 C8 FE FF FF 48 81 EC 00 02 00 00 0F 29 70 B8 0F 29 78 A8 44 0F 29 40 98 44 0F 29 48 88 44 0F 29 90 78 FF FF FF 44 0F 29 98 68 FF FF FF 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 90 00 00 00 4C 8B F2 48 8B F1 F6 81 38 03 00 00 02 0F 84 ?? ?? ?? ??", 0, "DamageHandler"}, // DamageNumberPatches.cpp
{0x05A49F40, 0, "48 89 5C 24 08 57 48 83 EC 20 80 B9 18 03 00 00 00", 0, "OpenChoices"}, // AirdropPatches.cpp
{0x05A4A3D0, 0, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 54 41 56 41 57 48 83 EC 30 4C 8B E2 48 8B E9 80 B9 A8 02 00 00 00 0F 85 ?? ?? ?? ??", 0, "OpenCrate"}, // AirdropPatches.cpp
{0x05A70050, 0, "48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 55 41 54 41 55 41 56 41 57 48 8D 6C 24 C9 48 81 EC B0 00 00 00 48 8B F9 E8 ?? ?? ?? ??", 0, "PanelForceUpdate"}, // RewardPatches.cpp
{0x05A74D50, 0, "48 89 5C 24 18 55 56 57 48 83 EC 40 48 8B F9", 0, "PanelConfirm"}, // RewardPatches.cpp
{0x05A7B75B, 0, "E8 ?? ?? ?? ?? 48 8B 4D 10 48 33 CC E8 ?? ?? ?? ?? 4C 8D 9C 24 30 01 00 00 49 8B 5B 38", 0, "ClientRefreshCallSite"}, // ClientPatches.cpp
{0x05A941D0, 0, "48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 41 54 41 55 41 56 41 57 48 8D 6C 24 B0 48 81 EC 50 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 40 48 8B F9 C6 81 00 09 00 00 01 E8 ?? ?? ?? ??", 0, "ClientRefreshFunction"}, // ClientPatches.cpp
{0x05ABA540, 0, "48 89 5C 24 10 48 89 6C 24 18 56 57 41 56 48 81 EC 80 00 00 00 48 8B F9 83 B9 5C 03 00 00 FF", 0, "OpenPanel"}, // RouteMapPatches.cpp
{0x05ABA627, 0, "48 8B 07 48 8B CF FF 90 C8 02 00 00 80 B8 68 01 00 00 03", 0, "OpenPanelRoleCheck"}, // RouteMapPatches.cpp
{0x05ABD6C0, 0, "48 89 5C 24 10 56 48 83 EC 40 48 83 79 30 00", 0, "RepresentationSize"}, // InventoryPatches.cpp
{0x05AE3510, 0, "48 8B C4 48 89 58 10 48 89 70 18 48 89 78 20 55 41 54 41 55 41 56 41 57 48 8D A8 B8 FE FF FF 48 81 EC 20 02 00 00 0F 29 70 C8 0F 29 78 B8 44 0F 29 40 A8 44 0F 29 48 98 4C 8B F1", 0, "GenerateStats"}, // RoundStatsPatches.cpp
{0x05AE367D, 0, "E8 ?? ?? ?? ?? 84 C0 75 ?? 84 DB 75 ?? 88 44 24 40", 0, "GenerateStatsFinalCall"}, // RoundStatsPatches.cpp
{0x05B0C050, 0, "4C 8B DC 55 53 56 57 41 56 49 8D 6B A8 48 81 EC 30 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 A0 4D 8B F1", 0, "LocalViewportCorners"}, // ExpPatches.cpp
{0x05B11480, 0, "48 89 5C 24 18 48 89 74 24 20 55 57 41 54 41 56 41 57 48 8D 6C 24 F0 48 81 EC 10 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 00 48 8B DA 48 8B F1 48 89 4C 24 30 45 33 F6", 0, "GetPlayerCharacters"}, // ClientPatches.cpp
{0x05B14115, 0, "48 8B 08 48 8B 91 30 03 00 00 48 8B C8 48 83 C4 28 48 FF E2 CC", 0, "PlayerIndexNullSite"}, // PlayerPatches.cpp
