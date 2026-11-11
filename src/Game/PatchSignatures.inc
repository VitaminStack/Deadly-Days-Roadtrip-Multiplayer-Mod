// Erzeugt von tools/patch_signatures.py: Referenz-RVA, Build (0 = alle), Byte-Muster (?? beliebig), Lage der Adresse
// im Muster, Name. Aufsteigend nach RVA, je RVA erst Build 0, dann die eigenen Zeilen einzelner Builds.
// Die Mod prueft das Muster an der erwarteten Stelle, das Werkzeug zusaetzlich die Eindeutigkeit in der Exe.
{0x01272700, 0, "48 89 6C 24 18 48 89 74 24 20 57 41 54 41 56 48 83 EC 30 41 8B 30 4D 8B F0 0F B7 E9 4C 8B E2 8B C5 C1 ED 08", 0, "ArrayResizeGrow"}, // CameraPatches.cpp
{0x012D6D80, 0, "40 53 48 8B 19 48 8B C1 4C 63 CA 45 85 C0 0F 84 ?? ?? ?? ?? 4D 8B D1 48 89 7C 24 10 41 8B C9 49 C1 E2 04 41 BB 01 00 00 00 41 D3 C3", 0, "ControllerSparseRemove"}, // PlayerPatches.cpp
{0x012F85E0, 0, "48 85 C9 74 ?? 53 48 83 EC 20 48 8B D9 48 8B 0D ?? ?? ?? ??", 0, "FreeArray"}, // ClientPatches.cpp
{0x01344650, 0, "45 85 C0 0F 84 ?? ?? ?? ?? 48 89 5C 24 10 57 48 8B 39 48 8B C1 4C 63 D2 41 BB 01 00 00 00 41 8B CA 48 89 74 24 10 41 D3 C3 41 8B D8 4B 8D 0C 92 4C 8D 04 8F 83 78 34 00 74 ??", 0, "MapSparseRemove"}, // RemoteP2Patches.cpp
{0x01512430, 0, "48 8B 01 48 FF A0 F8 01 00 00 CC CC CC CC CC CC 40 53 48 83 EC 40 0F 10 02 33 C0", 0, "DefaultNetworkingSupport"}, // InventoryPatches.cpp
{0x024E95F0, 0, "4C 8B DC 53 55 56 57 41 56 48 81 EC A0 00 00 00 48 8B 05 ?? ?? ?? ??", 0, "RegistrationBroadcast"}, // PlayerPatches.cpp
{0x0288B710, 0, "40 55 53 57 41 55 48 8D AC 24 78 FF FF FF 48 81 EC 88 01 00 00", 0, "CreateChildActor"}, // MapPatches.cpp
{0x0289DD60, 0, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B F9 48 81 C1 50 02 00 00", 0, "IsChildActorReplicated"}, // MapPatches.cpp
{0x02A67AF0, 0, "40 53 55 57 41 54 41 55 41 56 41 57 48 81 EC B0 00 00 00", 0, "BeginDeferredActorSpawnFromClass"}, // ClientPatches.cpp
{0x02CF382E, 0, "0F 28 C6 F2 0F 5C C1 66 0F 2F C7 0F 86 ?? ?? ?? ??", 0, "NetGuidExpiry"}, // ClientPatches.cpp
{0x02CF3E38, 0, "8B 44 24 4C 48 8D 4C 24 48 F7 D0 21 44 24 58 E8 ?? ?? ?? ?? 48 8B 44 24 50 44 8B 4C 24 5C 44 3B 48 18 0F 8C ?? ?? ?? ?? 80 7C 24 78 00 44 0F 28 84 24 30 02 00 00 0F 28 BC 24 40 02 00 00", 0, "NetGuidNext"}, // ClientPatches.cpp
{0x031473C0, 0, "4C 8B 0A 4C 8B D1 4D 85 C9", 0, "FindTimer"}, // InventoryPatches.cpp
{0x0314D2B0, 0, "48 89 5C 24 08 48 89 54 24 10 57 48 83 EC 20 48 8B C2 48 8B F9", 0, "ClearTimer"}, // InventoryPatches.cpp
{0x0314D6D0, 0, "40 55 53 48 8D 6C 24 B1 48 81 EC A8 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 37 0F B6 45 D7", 0, "SetNextTick"}, // InventoryPatches.cpp
{0x03262A30, 0, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B 81 B0 12 00 00 41 8B F9", 0, "SetClientTravel"}, // TravelPatches.cpp
{0x04B68380, 0, "48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 48 8B 99 80 10 00 00", 0, "AttributeLookupFunction"}, // ClientPatches.cpp
{0x04BC58D0, 0, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 49 8B F8 48 8B DA 48 8B F1 E8 ?? ?? ?? ?? 48 8B C8 48 8B 10 FF 92 D8 02 00 00 48 85 C0", 0, "ExecuteCueNonReplicated"}, // BodyPartPatches.cpp
{0x056676F5, 0, "48 FF A0 68 07 00 00 CC 48 8B 42 20 45 33 C0 48 85 C0 41 0F 95 C0 4C 03 C0 4C 89 42 20 E9 ?? ?? ?? ?? CC CC CC", 29, "At_5666FA5"}, // ClientPatches.cpp
{0x057BDCD0, 0, "40 53 56 57 41 55 48 83 EC 38 48 89 6C 24 68", 0, "ControllerSetAdd"}, // PlayerPatches.cpp
{0x057CB5A5, 0, "E8 ?? ?? ?? ?? 90 0F B6 45 37 48 8D 15 ?? ?? ?? ?? 48 8B 14 C2 48 8D 05 ?? ?? ?? ?? 48 3B D0 75 ?? 48 83 7D 07 00 74 ?? 48 8B 45 2F 48 8D 4D 17 48 85 C0 48 0F 45 C8 48 8B 01 FF 50 10 90 EB ?? 48 8D 4D 07 FF D2 90 48 83 7D D7 00 74 ?? 48 8B 45 FF 48 8D 4D E7 48 85 C0 48 0F 45 C8 48 8B 01 FF 50 10 90 48 8B 4D 47 48 33 CC E8 ?? ?? ?? ?? 4C 8D 9C 24 C0 00 00 00 49 8B 5B 18 49 8B 7B 20 49 8B E3 5D C3 CC CC CC CC CC CC 48 89 5C 24 10 48 89 6C 24 20 56", 0, "ScheduleCall"}, // InventoryPatches.cpp
{0x057D0C60, 0, "40 53 48 83 EC 20 48 8B D9 E8 ?? ?? ?? ?? 48 8B 8B D8 00 00 00 48 85 C9 74 ?? 48 83 C1 60", 0, "DetectorEndPlay"}, // InventoryPatches.cpp
{0x057DC810, 0, "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 60 48 8B F2 48 8B F9 44 8B 05 ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 B9 FC 1A 00 00 4A 8B 04 C0 8B 04 01 39 05 ?? ?? ?? ?? 7F ?? 48 8B CF E8 ?? ?? ?? ?? 48 8B D8 48 8B 15 ?? ?? ?? ?? 48 8D 4C 24 20 E8 ?? ?? ?? ?? 90 4C 8D 87 A0 00 00 00 48 8D 8B D8 08 00 00 4C 8B CE 48 8B D0 E8 ?? ?? ?? ?? 90 48 8B 4C 24 40 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 8B 4C 24 20 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 8B 5C 24 78 48 8B B4 24 80 00 00 00 48 83 C4 60 5F C3 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 3D ?? ?? ?? ?? FF 75 ?? 41 B9 01 00 00 00 4C 8D 05 ?? ?? ?? ?? BA 06 00 00 00", 0, "OnRepHealth"}, // HealthBarPatches.cpp
{0x057DCE70, 0, "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 60 48 8B F2 48 8B F9 44 8B 05 ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 B9 FC 1A 00 00 4A 8B 04 C0 8B 04 01 39 05 ?? ?? ?? ?? 7F ?? 48 8B CF E8 ?? ?? ?? ?? 48 8B D8 48 8B 15 ?? ?? ?? ?? 48 8D 4C 24 20 E8 ?? ?? ?? ?? 90 4C 8D 47 30 48 8D 8B D8 08 00 00 4C 8B CE 48 8B D0 E8 ?? ?? ?? ?? 90 48 8B 4C 24 40 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 8B 4C 24 20 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 8B 5C 24 78 48 8B B4 24 80 00 00 00 48 83 C4 60 5F C3 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 3D ?? ?? ?? ?? FF 75 ?? 41 B9 01 00 00 00 4C 8D 05 ?? ?? ?? ?? BA 09 00 00 00", 0, "OnRepMaxHealth"}, // HealthBarPatches.cpp
{0x057DD080, 0, "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 60 48 8B F2 48 8B F9 44 8B 05 ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 B9 FC 1A 00 00 4A 8B 04 C0 8B 04 01 39 05 ?? ?? ?? ?? 7F ?? 48 8B CF E8 ?? ?? ?? ?? 48 8B D8 48 8B 15 ?? ?? ?? ?? 48 8D 4C 24 20 E8 ?? ?? ?? ?? 90 4C 8D 47 30 48 8D 8B D8 08 00 00 4C 8B CE 48 8B D0 E8 ?? ?? ?? ?? 90 48 8B 4C 24 40 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 8B 4C 24 20 48 85 C9 74 ?? E8 ?? ?? ?? ?? 90 48 8B 5C 24 78 48 8B B4 24 80 00 00 00 48 83 C4 60 5F C3 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 3D ?? ?? ?? ?? FF 75 ?? 41 B9 01 00 00 00 4C 8D 05 ?? ?? ?? ?? BA 0A 00 00 00", 0, "OnRepMaxStamina"}, // HealthBarPatches.cpp
{0x057DDE40, 0, "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 60 48 8B F2 48 8B F9 44 8B 05 ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 B9 FC 1A 00 00 4A 8B 04 C0 8B 04 01 39 05 ?? ?? ?? ?? 7F ?? 48 8B CF E8 ?? ?? ?? ?? 48 8B D8 48 8B 15 ?? ?? ?? ?? 48 8D 4C 24 20 E8 ?? ?? ?? ?? 90 4C 8D 47 50 48 8D 8B D8 08 00 00 4C 8B CE 48 8B D0", 0, "OnRepStamina"}, // HealthBarPatches.cpp
{0x0583AEEC, 0, "48 8B 07 48 8D 54 24 60 48 8B CF FF 90 D8 06 00 00", 0, "ComponentControllerRead"}, // ClientPatches.cpp
{0x0583B060, 0, "48 8B 9C 24 B8 00 00 00 48 81 C4 90 00 00 00 5F 5E 5D C3 CC CC CC CC CC CC CC CC CC CC CC CC CC 48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B F1", 0, "ComponentControllerExit"}, // ClientPatches.cpp
{0x0584DF32, 0, "48 8B C8 48 8B 00 FF 90 30 03 00 00 8B E8", 0, "RunEndLocalPlayerSite"}, // PlayerPatches.cpp
{0x0584DF40, 0, "48 8B BE E8 00 00 00 8B D5 48 8B CF", 0, "RunEndContinue"}, // PlayerPatches.cpp
{0x058A6466, 0, "0F 84 ?? ?? ?? ?? 80 3D ?? ?? ?? ?? 00 48 89 74 24 50 74 ??", 0, "SneakSameTarget"}, // ClientPatches.cpp
{0x058A646C, 0, "80 3D ?? ?? ?? ?? 00 48 89 74 24 50 74 ?? 48 8B CB", 0, "SneakRetry"}, // ClientPatches.cpp
{0x058A64FE, 0, "48 83 BF E8 00 00 00 00 74 ?? 48 85 DB", 0, "At_58A52FE"}, // ClientPatches.cpp
{0x058A650D, 0, "B0 01 48 8B 5C 24 58 48 83 C4 40 5F C3 48 8B 5C 24 58 32 C0 48 83 C4 40 5F C3 CC CC CC CC CC CC CC CC CC 48 89 5C 24 10 48 89 6C 24 18 56 57 41 56", 0, "SneakReady"}, // ClientPatches.cpp
{0x058A7260, 0, "E8 ?? ?? ?? ?? F3 0F 11 87 14 03 00 00 48 8B CF", 0, "At_58A6060"}, // ClientPatches.cpp
{0x058AB9A0, 0, "48 8B 09 48 8B 01 48 FF A0 E8 04 00 00 CC CC CC E9 ?? ?? ?? ?? CC", 0, "At_58AA5F0"}, // ClientPatches.cpp
{0x058B1D3E, 0, "E8 ?? ?? ?? ?? F3 0F 11 87 14 03 00 00 40 38 AF 18 03 00 00", 0, "At_58B0F6E"}, // ClientPatches.cpp
{0x058B4D30, 0, "48 89 5C 24 08 57 48 83 EC 40 0F 29 74 24 30 0F 57 F6 48 8B D1", 0, "ExpAggregate"}, // ClientPatches.cpp
{0x058B4D6B, 0, "48 8B 89 D8 07 00 00 48 83 E9 80 E8 ?? ?? ?? ??", 0, "At_58B419B"}, // ClientPatches.cpp
{0x058BBD90, 0, "40 53 48 81 EC B0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 A0 00 00 00 48 8B D9 48 8B 81 B0 00 00 00 48 85 C0", 0, "BlockadeLoadingFinished"}, // ClientPatches.cpp
{0x058BF120, 0, "40 53 48 83 EC 70 48 8B D9 E8 ?? ?? ?? ?? 0F 57 C9 4C 8D 4C 24 20", 0, "UpdateBounds"}, // ExpPatches.cpp
{0x058BF210, 0, "48 89 5C 24 18 55 56 57 48 83 EC 30 FF 41 5C", 0, "ReceiveExpImpl"}, // ExpPatches.cpp
{0x058C1B10, 0, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 40 0F 29 74 24 30", 0, "SpawnRequest"}, // ExpPatches.cpp
{0x058CF517, 0, "49 8B 86 E8 02 00 00 48 89 44 24 58 80 3D ?? ?? ?? ?? 00", 0, "SpawnerPlayerRead"}, // PlayerPatches.cpp
{0x058CFA07, 0, "32 DB C7 45 48 00 00 00 00 48 8B 4D 40", 0, "SpawnerNoPlayerExit"}, // PlayerPatches.cpp
{0x058D1A80, 0, "48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 56 41 57 48 83 EC 30 4C 8B F2 49 8B D8 48 8D 54 24 20 48 8B F1 E8 ?? ?? ?? ?? 48 8B 7C 24 28 48 85 FF 74 ?? 41 8B 06 89 07 48 8B 03 48 89 47 04 C7 47 0C FF FF FF FF 8B 46 08 2B 46 34 44 8B 7C 24 20 8B 2F", 0, "MapAdd"}, // RemoteP2Patches.cpp
{0x058DA3E0, 0, "48 89 5C 24 08 48 89 74 24 10 55 57 41 54 41 56 41 57 48 8B EC 48 83 EC 70 41 8B F0 8B DA 4C 8B F1", 0, "AssignDevice"}, // LocalDevicePatches.cpp
{0x058E73B0, 0, "80 B9 88 01 00 00 00 74 ?? 41 8D 40 01", 0, "DraftAssignDevice"}, // LocalDevicePatches.cpp
{0x058EA1A0, 0, "48 83 C4 50 5B C3 CC CC CC CC 48 89 5C 24 20 55 56 57 48 83 EC 30", 10, "FinishMission"}, // ClientPatches.cpp
{0x058EFFD0, 0, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 8B 81 08 01 00 00 48 8B D9", 0, "GetController"}, // LocalDevicePatches.cpp
{0x058FD3A0, 0, "48 89 5C 24 18 48 89 74 24 20 57 48 83 EC 20 48 8B DA 48 8B F1 48 85 D2 74 ?? 33 C9 E8 ?? ?? ?? ?? 48 8B 53 10 4C 8D 40 30 48 63 40 38 3B 42 38 7F ?? 48 8B C8 48 8B 42 30 4C 39 04 C8 75 ?? 4C 8B D3 EB ?? 45 33 D2 48 B9 CD 8C 55 ED D7 AF 51 FF 4C 89 54 24 38", 0, "RegisterController"}, // PlayerPatches.cpp
{0x058FD49D, 0, "E8 ?? ?? ?? ?? 48 8D 8E 40 03 00 00 48 89 5C 24 30", 0, "At_58FB11D"}, // PlayerPatches.cpp
{0x058FD4B3, 0, "E8 ?? ?? ?? ?? 48 8B 5C 24 40 48 8B 74 24 48 48 83 C4 20 5F C3 CC CC CC CC CC CC CC CC 48 89 5C 24 10 48 89 74 24 18 4C 89 74 24 20 55 48 8B EC 48 83 EC 70 8B 51 48 48 8D 71 48 4C 8D 71 38 48 8B D9", 0, "At_58FB133"}, // PlayerPatches.cpp
{0x05904DE8, 0, "48 8B 08 48 8B 91 30 03 00 00 48 8B C8 FF D2 8B D0 48 8B CB", 0, "SpawnNullSite"}, // PlayerPatches.cpp
{0x05904F53, 0, "4C 8B C5 48 8B D7 48 8B CE E8 ?? ?? ?? ?? 48 8B 8C 24 D0 00 00 00 48 33 CC", 0, "SpawnSkipTarget"}, // PlayerPatches.cpp
{0x05904F90, 0, "89 54 24 10 55 53 56 57 41 56 48 8B EC 48 83 EC 40", 0, "SpawnLocal"}, // LocalDevicePatches.cpp
{0x05907ED0, 0, "48 89 5C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 30 0F B6 EA 48 8B F1 E8 ?? ?? ?? ??", 0, "TriggerEndRound"}, // ClientPatches.cpp
{0x0590B2AA, 0, "48 85 DB 74 ?? 40 84 ED 74 ?? B0 01 EB ??", 0, "DeviceCheckSite"}, // RemoteP2Patches.cpp
{0x0590B2AF, 0, "40 84 ED 74 ?? B0 01 EB ?? 32 C0 44 3A F0", 0, "SoloLossTest"}, // RemoteP2Patches.cpp
{0x0590B2B4, 0, "B0 01 EB ?? 32 C0 44 3A F0 0F 84 ?? ?? ?? ?? 88 87 89 01 00 00", 0, "NeedReconnect"}, // RemoteP2Patches.cpp
{0x05941A43, 0, "74 ?? 8B 41 64 44 8B D7 39 41 38", 0, "ThreatFreezeBranch"}, // ClientPatches.cpp
{0x0595DA20, 0, "48 89 5C 24 08 55 56 57 48 81 EC C0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 B0 00 00 00 49 8B F9 41 0F B6 E8 48 8B F2 33 C9", 0, "CreateItemInstance"}, // RewardPatches.cpp
{0x0596BA70, 0, "48 89 5C 24 08 57 48 83 EC 20 45 33 C0 48 8B DA", 0, "HandleItemSold"}, // RewardPatches.cpp (Prolog aus CheckSold; Eindeutigkeit offline gegen Spiel-EXE prüfen)
{0x0596D8C0, 0, "E9 ?? ?? ?? ?? CC CC CC CC CC CC CC CC CC CC CC 48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 20 8B 41 08 4C 8B F2 48 8B F9 3B 41 0C 49 63 F0 4C 8D 41 0C 75 ??", 0, "InventoryReadyForReplication"}, // InventoryPatches.cpp
{0x0597ABB0, 0, "48 89 5C 24 18 48 89 54 24 10 55 56 57 48 8D 6C 24 B9 48 81 EC 90 00 00 00 48 8B FA", 0, "SeamlessTravelToLevel"}, // TravelPatches.cpp
{0x0597DC10, 0, "48 89 5C 24 18 48 89 54 24 10 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 50 45 8B E8 4C 8B E2", 0, "SetRewardItems"}, // RewardPatches.cpp
{0x05984CD0, 0, "48 89 5C 24 08 4C 89 4C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 50 0F 29 74 24 40 4D 8B E8", 0, "UpgradeItems"}, // RewardPatches.cpp
{0x05995D30, 0, "48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 F0 FE FF FF 48 81 EC 10 02 00 00 0F 29 B4 24 00 02 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 F0 00 00 00 4C 8B EA 48 89 55 C0", 0, "ClearSpawnArea"}, // PlayerPatches.cpp
{0x05998D70, 0, "48 8B 83 B8 01 00 00 48 85 C0 74 ?? 0F 10 80 10 02 00 00 0F 11 45 E0 F2 0F 10 80 20 02 00 00", 0, "LootPawnRead"}, // PlayerPatches.cpp
{0x05998D91, 0, "0F 10 05 ?? ?? ?? ?? 0F 11 45 E0 F2 0F 10 05 ?? ?? ?? ?? F2 0F 11 85 B8 00 00 00", 0, "LootNoRootTarget"}, // PlayerPatches.cpp
{0x059AF200, 0, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 30 48 8B F2 48 8B F9 E8 ?? ?? ?? ??", 0, "ItemLoot"}, // InventoryPatches.cpp
{0x059AF3F0, 0, "48 89 5C 24 10 57 48 83 EC 20 48 8B 92 A8 00 00 00", 0, "LootBaseLoot"}, // InventoryPatches.cpp
{0x059AF660, 0, "48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 56 41 57 48 83 EC 20 48 8B FA", 0, "ScrapLoot"}, // InventoryPatches.cpp
{0x059BBFC7, 0, "E8 ?? ?? ?? ?? 49 89 86 58 01 00 00 48 85 C0", 0, "AttributeLookupCallSite"}, // ClientPatches.cpp
{0x059BC73A, 0, "48 8B 8D A0 00 00 00 48 33 CC E8 ?? ?? ?? ?? 4C 8D 9C 24 58 02 00 00 41 0F 28 73 E8 41 0F 28 7B D8 45 0F 28 43 C8 45 0F 28 4B B8 45 0F 28 53 A8 45 0F 28 5B 98 45 0F 28 63 88 45 0F 28 AB 78 FF FF FF 45 0F 28 B3 68 FF FF FF 45 0F 28 BB 58 FF FF FF 49 8B E3 41 5F 41 5E 41 5D 41 5C 5F 5E 5B 5D C3 CC CC CC CC 48 89 5C 24 10 48 89 6C 24 18 56 57", 0, "AttributeTickExit"}, // ClientPatches.cpp
{0x059C7FC0, 0, "48 89 5C 24 10 48 89 74 24 20 55 57 41 56 48 8B EC 48 83 EC 60 48 8B F2", 0, "CanBusTakeOff"}, // ClientPatches.cpp
{0x059C8AFF, 0, "E8 ?? ?? ?? ?? 48 8B 8B F0 02 00 00 84 C0 74 ?? 48 85 C9 0F 85 ?? ?? ?? ??", 0, "At_59C494F"}, // ClientPatches.cpp
{0x059CFBA0, 0, "48 8B 51 20 48 8B 49 18 E9 ?? ?? ?? ??", 0, "MapGenLambdaExecute"}, // InventoryPatches.cpp
{0x059CFD50, 0, "48 83 EC 28 48 8B 51 20 48 8B 49 18", 0, "MapGenLambdaExecuteIfSafe"}, // InventoryPatches.cpp
{0x059E4B2A, 0, "49 8B 9E C8 00 00 00 48 8B 03 48 8B B8 D0 06 00 00", 0, "MapGenHostObjectRead"}, // ClientPatches.cpp
{0x059E4BD7, 0, "0F 28 B4 24 80 00 00 00 48 81 C4 90 00 00 00 41 5E 5F 5E 5D", 0, "MapGenHostObjectExit"}, // ClientPatches.cpp
{0x059F3470, 0, "48 89 5C 24 18 55 56 57 41 56 41 57 48 8D AC 24 20 FC FF FF 48 81 EC E0 04 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 D0 03 00 00 4C 8B F2", 0, "SetupPcgVolume"}, // MapPatches.cpp
{0x059F44A0, 0, "40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 E1 48 81 EC E8 00 00 00 48 8B F1 45 33 FF", 0, "StartMapGeneration"}, // MapPatches.cpp
{0x05A11750, 0, "48 89 5C 24 18 48 89 74 24 20 55 57 41 54 41 56 41 57 48 8D 6C 24 C9 48 81 EC F0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 27 48 8B FA 48 89 55 C7 45 33 FF 44 89 7D 87", 0, "CameraPlayersFunction"}, // CameraPatches.cpp
{0x05A1B9EC, 0, "83 B8 C8 02 00 00 01 0F 8E ?? ?? ?? ?? 80 3D ?? ?? ?? ?? 00", 0, "RestrictorCountSite"}, // CameraPatches.cpp
{0x05A1B9F9, 0, "80 3D ?? ?? ?? ?? 00 48 89 5C 24 70 48 8B 9A E8 02 00 00", 0, "RestrictorContinue"}, // CameraPatches.cpp
{0x05A1BAA8, 0, "48 8B 6C 24 78 48 8B B4 24 80 00 00 00 48 83 C4 60 5F C3 CC CC CC CC CC 48 89 5C 24 18 48 89 6C 24 20", 0, "RestrictorSkip"}, // CameraPatches.cpp
{0x05A471D0, 0, "48 89 5C 24 08 57 48 83 EC 20 80 B9 18 03 00 00 00", 0, "OpenChoices"}, // AirdropPatches.cpp
{0x05A47660, 0, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 54 41 56 41 57 48 83 EC 30 4C 8B E2 48 8B E9 80 B9 A8 02 00 00 00 0F 85 ?? ?? ?? ??", 0, "OpenCrate"}, // AirdropPatches.cpp
{0x05A71900, 0, "48 89 5C 24 18 55 56 57 48 83 EC 40 48 8B F9", 0, "PanelConfirm"}, // RewardPatches.cpp
{0x05A782FB, 0, "E8 ?? ?? ?? ?? 48 8B 4D 10 48 33 CC E8 ?? ?? ?? ?? 4C 8D 9C 24 30 01 00 00 49 8B 5B 38", 0, "ClientRefreshCallSite"}, // ClientPatches.cpp
{0x05A90940, 0, "48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 41 54 41 55 41 56 41 57 48 8D 6C 24 B0 48 81 EC 50 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 40 48 8B F9 C6 81 ?? ?? 00 00 01 E8 ?? ?? ?? ??", 0, "ClientRefreshFunction"}, // ClientPatches.cpp
{0x05ABA0F0, 0, "48 89 5C 24 10 56 48 83 EC 40 48 83 79 30 00", 0, "RepresentationSize"}, // InventoryPatches.cpp
{0x05B08070, 0, "4C 8B DC 55 53 56 57 41 56 49 8D 6B A8 48 81 EC 30 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 A0 4D 8B F1", 0, "LocalViewportCorners"}, // ExpPatches.cpp
{0x05B0CE30, 0, "48 89 5C 24 18 48 89 74 24 20 55 57 41 54 41 56 41 57 48 8D 6C 24 F0 48 81 EC 10 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 00 48 8B DA 48 8B F1 48 89 4C 24 30 45 33 F6", 0, "GetPlayerCharacters"}, // ClientPatches.cpp
{0x05B0FA95, 0, "48 8B 08 48 8B 91 30 03 00 00 48 8B C8 48 83 C4 28 48 FF E2 CC", 0, "PlayerIndexNullSite"}, // PlayerPatches.cpp
