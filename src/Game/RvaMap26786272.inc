// Erzeugt von tools/rva_map.py fuer Build 26786272. Referenz-RVA -> RVA in diesem Build, aufsteigend sortiert.
// Nur Zeilen mit geprueftem Ergebnis eintragen; fehlende Adressen bleiben Kommentar und liefern zur Laufzeit 0.
// Haendisch nachgetragene Zeilen mit "// von Hand" kennzeichnen, damit ein erneuter Lauf sie nicht still ersetzt.
{0x01272700, 0x01272700}, // eindeutig, ArrayResizeGrow, src/Game/CameraPatches.cpp:19
{0x012BC260, 0x012BC260}, // eindeutig, DamageInfoMapDestruct, src/Game/DamageNumberPatches.cpp:32
{0x012D6D80, 0x012D6D80}, // eindeutig, ControllerSparseRemove, src/Game/PlayerPatches.cpp:31
{0x012F85E0, 0x012F85E0}, // eindeutig, FreeArray, src/Game/ClientPatches.cpp:32
{0x01344650, 0x01344650}, // eindeutig, MapSparseRemove, src/Game/RemoteP2Patches.cpp:28
{0x01512430, 0x01512430}, // eindeutig, DefaultNetworkingSupport, src/Game/InventoryPatches.cpp:27
{0x0160C220, 0x0160C220}, // eindeutig, ProcessEvent, src/Game/Offsets.h:13
{0x024E95F0, 0x024E95F0}, // eindeutig, RegistrationBroadcast, src/Game/PlayerPatches.cpp:33
{0x0288B710, 0x0288B6E0}, // eindeutig, CreateChildActor, src/Game/MapPatches.cpp:23
{0x0289DD60, 0x0289DD30}, // eindeutig, IsChildActorReplicated, src/Game/MapPatches.cpp:24
{0x02A67AF0, 0x02A67AC0}, // eindeutig, BeginDeferredActorSpawnFromClass, src/Game/ClientPatches.cpp:39
{0x02CF382E, 0x02CF37FE}, // eindeutig, NetGuidExpiry, src/Game/ClientPatches.cpp:34
{0x02CF3E38, 0x02CF3E08}, // eindeutig, NetGuidNext, src/Game/ClientPatches.cpp:35
{0x031473C0, 0x03147390}, // eindeutig, FindTimer, src/Game/InventoryPatches.cpp:321
{0x0314D2B0, 0x0314D280}, // eindeutig, ClearTimer, src/Game/InventoryPatches.cpp:322
{0x0314D6D0, 0x0314D6A0}, // eindeutig, SetNextTick, src/Game/InventoryPatches.cpp:320
{0x03262A30, 0x03262A00}, // eindeutig, SetClientTravel, src/Game/TravelPatches.cpp:16
{0x04B68380, 0x04B68BE0}, // eindeutig, AttributeLookupFunction, src/Game/ClientPatches.cpp:23
{0x04BC58D0, 0x04BC6130}, // eindeutig, ExecuteCueNonReplicated, src/Game/BodyPartPatches.cpp:28
{0x04BDDD80, 0x04BDE5E0}, // eindeutig, SetNumericValueChecked, src/Game/HealthBarPatches.cpp:31
{0x056591B0, 0x05659A70}, // eindeutig, DamageInfoConstruct, src/Game/DamageNumberPatches.cpp:31
{0x056676F5, 0x05667FB5}, // von Hand (einziger jmp auf BlockadeLoadingFinished 0x058BD200, 29 eindeutige Fenster der Funktion, Verschiebung +0x130 gegen 26669225), At, src/Game/ClientPatches.cpp:360
{0x05707870, 0x05708220}, // von Hand (37 eindeutige Fenster, alle 38 Befehle maskiert gleich wie 26669225), ReloadSoundEvent, src/Game/WeaponSoundPatches.cpp:99
{0x057BDCD0, 0x057BED90}, // eindeutig, ControllerSetAdd, src/Game/PlayerPatches.cpp:30
{0x057CB5A5, 0x057CC665}, // ueber-funktion, ScheduleCall, src/Game/InventoryPatches.cpp:319
{0x057D0C60, 0x057D1DA0}, // eindeutig, DetectorEndPlay, src/Game/InventoryPatches.cpp:317
{0x057D48A0, 0x057D59E0}, // eindeutig, GetArmorAttribute, src/Game/HealthBarPatches.cpp:33
{0x057DBA40, 0x057DCB80}, // von Hand (20 eindeutige Fenster, alle 75 Befehle maskiert gleich wie 26669225, Verschiebung +0x930), OnRepArmor, src/Game/HealthBarPatches.cpp:26
{0x057DC810, 0x057DD950}, // von Hand (20 eindeutige Fenster, alle 75 Befehle maskiert gleich wie 26669225, Verschiebung +0x930), OnRepHealth, src/Game/HealthBarPatches.cpp:22
{0x057DCD60, 0x057DDEA0}, // von Hand (20 eindeutige Fenster, alle 75 Befehle maskiert gleich wie 26669225, Verschiebung +0x930), OnRepMaxArmor, src/Game/HealthBarPatches.cpp:27
{0x057DCE70, 0x057DDFB0}, // von Hand (30 eindeutige Fenster, alle 62 Befehle maskiert gleich wie 26669225, Verschiebung +0x930), OnRepMaxHealth, src/Game/HealthBarPatches.cpp:23
{0x057DD080, 0x057DE1C0}, // von Hand (20 eindeutige Fenster, alle 62 Befehle maskiert gleich wie 26669225, Verschiebung +0x930), OnRepMaxStamina, src/Game/HealthBarPatches.cpp:25
{0x057DDE40, 0x057DEF80}, // eindeutig, OnRepStamina, src/Game/HealthBarPatches.cpp:24
{0x057DF8EB, 0x057E09DB}, // eindeutig, ClampToMaxCall, src/Game/HealthBarPatches.cpp:30
{0x05837220, 0x05838370}, // eindeutig, ApplyInitial, src/Game/CharacterPatches.cpp:29
{0x0583AEEC, 0x0583C03C}, // eindeutig, ComponentControllerRead, src/Game/ClientPatches.cpp:27
{0x0583B060, 0x0583C1B0}, // eindeutig, ComponentControllerExit, src/Game/ClientPatches.cpp:28
{0x05840DF0, 0x05841F40}, // eindeutig, ApplyDefinition, src/Game/CharacterPatches.cpp:30
{0x0584DF32, 0x0584F082}, // eindeutig, RunEndLocalPlayerSite, src/Game/PlayerPatches.cpp:35
{0x0584DF40, 0x0584F090}, // eindeutig, RunEndContinue, src/Game/PlayerPatches.cpp:36
{0x0585AEA0, 0x0585BFF0}, // eindeutig, CameraFollowTick, src/Game/PlayerPatches.cpp:38
{0x058908C0, 0x05891B00}, // eindeutig, SpawnCircleTelegraph, src/Game/EnemyTelegraphPatches.cpp:32
{0x058A3DB0, 0x058A5090}, // eindeutig, ActivateLineTelegraph, src/Game/EnemyTelegraphPatches.cpp:33
{0x058A6466, 0x058A7736}, // eindeutig, SneakSameTarget, src/Game/ClientPatches.cpp:36
{0x058A646C, 0x058A773C}, // eindeutig, SneakRetry, src/Game/ClientPatches.cpp:37
{0x058A64FE, 0x058A77CE}, // eindeutig, At, src/Game/ClientPatches.cpp:393
{0x058A650D, 0x058A77DD}, // eindeutig, SneakReady, src/Game/ClientPatches.cpp:38
{0x058A7260, 0x058A8530}, // eindeutig, At, src/Game/ClientPatches.cpp:347
{0x058A9D30, 0x058AB000}, // eindeutig, CalculateLineTelegraphEndPoint, src/Game/EnemyTelegraphPatches.cpp:34
{0x058AB9A0, 0x058ACDF0}, // eindeutig, At, src/Game/ClientPatches.cpp:361
{0x058B1130, 0x058B25E0}, // von Hand (Funktionsanfang aus .pdata, 111 Fenster mit +0xEA0/+0xE9E; Rumpf geaendert, Prolog sub rsp,28h, eigene Pruefbytes), EndTackle, src/Game/EnemyTelegraphPatches.cpp:35
{0x058B1D3E, 0x058B31AE}, // eindeutig, At, src/Game/ClientPatches.cpp:348
{0x058B1F40, 0x058B33B0}, // eindeutig, ExecuteTackle, src/Game/EnemyTelegraphPatches.cpp:36
{0x058B4D30, 0x058B61A0}, // eindeutig, ExpAggregate, src/Game/ClientPatches.cpp:30
{0x058B4D6B, 0x058B61DB}, // eindeutig, At, src/Game/ClientPatches.cpp:346
{0x058BBD90, 0x058BD200}, // eindeutig, BlockadeLoadingFinished, src/Game/ClientPatches.cpp:33
{0x058BF120, 0x058C0BA0}, // eindeutig, UpdateBounds, src/Game/ExpPatches.cpp:27
{0x058BF210, 0x058C0C90}, // eindeutig, ReceiveExpImpl, src/Game/ExpPatches.cpp:26
{0x058C1B10, 0x058C3590}, // eindeutig, SpawnRequest, src/Game/ExpPatches.cpp:25
{0x058CE280, 0x058CFD10}, // von Hand (eindeutig ueber 26669225, alle 12 Befehle gleich; Anfang wie 26669225, eigene Pruefbytes), TryBuyItem, src/Game/RewardPatches.cpp:55
{0x058CF517, 0x058D0FC7}, // eindeutig, SpawnerPlayerRead, src/Game/PlayerPatches.cpp:26
{0x058CFA07, 0x058D14B7}, // eindeutig, SpawnerNoPlayerExit, src/Game/PlayerPatches.cpp:27
{0x058D1A80, 0x058D3530}, // eindeutig, MapAdd, src/Game/RemoteP2Patches.cpp:27
{0x058DA3E0, 0x058DBF90}, // eindeutig, AssignDevice, src/Game/LocalDevicePatches.cpp:19
{0x058E73B0, 0x058E91A0}, // eindeutig, DraftAssignDevice, src/Game/LocalDevicePatches.cpp:20
{0x058EA1A0, 0x058EC080}, // eindeutig, FinishMission, src/Game/ClientPatches.cpp:41
{0x058EFFD0, 0x058F1F10}, // eindeutig, GetController, src/Game/LocalDevicePatches.cpp:22
{0x058FD3A0, 0x058FF2E0}, // eindeutig, RegisterController, src/Game/PlayerPatches.cpp:32
{0x058FD49D, 0x058FF3DD}, // eindeutig, At, src/Game/PlayerPatches.cpp:480
{0x058FD4B3, 0x058FF3F3}, // eindeutig, At, src/Game/PlayerPatches.cpp:481
{0x05904DE8, 0x05906F08}, // eindeutig, SpawnNullSite, src/Game/PlayerPatches.cpp:23
{0x05904F53, 0x05907073}, // eindeutig, SpawnSkipTarget, src/Game/PlayerPatches.cpp:24
{0x05904F90, 0x059070B0}, // eindeutig, SpawnLocal, src/Game/LocalDevicePatches.cpp:21
{0x05907ED0, 0x05909FE0}, // eindeutig, TriggerEndRound, src/Game/ClientPatches.cpp:42
{0x0590B2AA, 0x0590D3BA}, // eindeutig, DeviceCheckSite, src/Game/RemoteP2Patches.cpp:31
{0x0590B2AF, 0x0590D3BF}, // eindeutig, SoloLossTest, src/Game/RemoteP2Patches.cpp:32
{0x0590B2B4, 0x0590D3C4}, // eindeutig, NeedReconnect, src/Game/RemoteP2Patches.cpp:33
{0x05936BE0, 0x05938CF0}, // eindeutig, RunSaveRoundChanged, src/Game/ClientPatches.cpp:43
{0x0593EBA0, 0x05940CB0}, // eindeutig, RunSaveResave, src/Game/ClientPatches.cpp:44
{0x05941A43, 0x05943B53}, // eindeutig, ThreatFreezeBranch, src/Game/ClientPatches.cpp:29
{0x0595DA20, 0x0595FB80}, // eindeutig, CreateItemInstance, src/Game/RewardPatches.cpp:45
{0x0596BA70, 0x0596E480}, // eindeutig, HandleItemSold, src/Game/RewardPatches.cpp:52
{0x0596D8C0, 0x059702D0}, // eindeutig, InventoryReadyForReplication, src/Game/InventoryPatches.cpp:28
{0x0597ABB0, 0x0597D860}, // eindeutig, SeamlessTravelToLevel, src/Game/TravelPatches.cpp:17
{0x0597DC10, 0x059808C0}, // eindeutig, SetRewardItems, src/Game/RewardPatches.cpp:40
{0x05984CD0, 0x059878D0}, // eindeutig, UpgradeItems, src/Game/RewardPatches.cpp:49
{0x05995D30, 0x05998940}, // eindeutig, ClearSpawnArea, src/Game/PlayerPatches.cpp:34
{0x05998D70, 0x0599B984}, // von Hand (Bytefolge mov rax,[rbx+1B8h]/test/je/movups [rax+210h] einmal in .text; Funktion stark umgebaut), LootPawnRead, src/Game/PlayerPatches.cpp:28
{0x05998D91, 0x0599B9A5}, // von Hand (Sprungziel des je nach LootPawnRead, laedt den Standardort), LootNoRootTarget, src/Game/PlayerPatches.cpp:29
{0x059AF200, 0x059B1F50}, // eindeutig, ItemLoot, src/Game/InventoryPatches.cpp:34
{0x059AF3F0, 0x059B2140}, // eindeutig, LootBaseLoot, src/Game/InventoryPatches.cpp:43
{0x059AF660, 0x059B23B0}, // eindeutig, ScrapLoot, src/Game/InventoryPatches.cpp:42
{0x059BBFC7, 0x059BED47}, // eindeutig, AttributeLookupCallSite, src/Game/ClientPatches.cpp:22
{0x059BC73A, 0x059BF4BA}, // eindeutig, AttributeTickExit, src/Game/ClientPatches.cpp:24
{0x059C7FC0, 0x059CAD40}, // eindeutig, CanBusTakeOff, src/Game/ClientPatches.cpp:40
{0x059C8AFF, 0x059CB87F}, // eindeutig, At, src/Game/ClientPatches.cpp:408
{0x059CFBA0, 0x059D2920}, // eindeutig, MapGenLambdaExecute, src/Game/InventoryPatches.cpp:328
{0x059CFD50, 0x059D2AD0}, // eindeutig, MapGenLambdaExecuteIfSafe, src/Game/InventoryPatches.cpp:329
{0x059E4B2A, 0x059E78AA}, // eindeutig, MapGenHostObjectRead, src/Game/ClientPatches.cpp:25
{0x059E4BD7, 0x059E7957}, // eindeutig, MapGenHostObjectExit, src/Game/ClientPatches.cpp:26
{0x059F3470, 0x059F61F0}, // eindeutig, SetupPcgVolume, src/Game/MapPatches.cpp:25
{0x059F44A0, 0x059F7220}, // eindeutig, StartMapGeneration, src/Game/MapPatches.cpp:22
{0x05A11750, 0x05A144D0}, // eindeutig, CameraPlayersFunction, src/Game/CameraPatches.cpp:18
{0x05A1B9EC, 0x05A1E76C}, // eindeutig, RestrictorCountSite, src/Game/CameraPatches.cpp:32
{0x05A1B9F9, 0x05A1E779}, // eindeutig, RestrictorContinue, src/Game/CameraPatches.cpp:34
{0x05A1BAA8, 0x05A1E828}, // eindeutig, RestrictorSkip, src/Game/CameraPatches.cpp:33
{0x05A22DB0, 0x05A25B30}, // eindeutig, DamageHandler, src/Game/DamageNumberPatches.cpp:30
{0x05A471D0, 0x05A49F40}, // eindeutig, OpenChoices, src/Game/AirdropPatches.cpp:13
{0x05A47660, 0x05A4A3D0}, // eindeutig, OpenCrate, src/Game/AirdropPatches.cpp:12
{0x05A71900, 0x05A74D50}, // eindeutig, PanelConfirm, src/Game/RewardPatches.cpp:41
{0x05A782FB, 0x05A7B75B}, // eindeutig, ClientRefreshCallSite, src/Game/ClientPatches.cpp:20
{0x05A90940, 0x05A941D0}, // von Hand (179 eindeutige Fenster, 639 von 656 Befehlen gleich; Feld 0x8F8 liegt jetzt bei 0x900), ClientRefreshFunction, src/Game/ClientPatches.cpp:21
{0x05ABA0F0, 0x05ABD6C0}, // eindeutig, RepresentationSize, src/Game/InventoryPatches.cpp:335
{0x05B08070, 0x05B0C050}, // eindeutig, LocalViewportCorners, src/Game/ExpPatches.cpp:28
{0x05B0CE30, 0x05B11480}, // eindeutig, GetPlayerCharacters, src/Game/ClientPatches.cpp:31
{0x05B0FA95, 0x05B14115}, // eindeutig, PlayerIndexNullSite, src/Game/PlayerPatches.cpp:25
{0x093A1F98, 0x093A7EF8}, // von Hand (82 von 84 Eintraegen umgerechnet, alle gleich), SlotVtable, src/Game/InventoryPatches.cpp:23
{0x094B9A08, 0x094BFFF0}, // vtable, ItemVtable, src/Game/InventoryPatches.cpp:24
{0x094BF710, 0x094C5D00}, // vtable, InventoryVtable, src/Game/InventoryPatches.cpp:22
{0x094C6A78, 0x094CD068}, // von Hand (162 von 164 Eintraegen umgerechnet, alle gleich), DetectorVtable, src/Game/InventoryPatches.cpp:316
{0x096191F0, 0x0961FFC0}, // vtable, MapGenLambdaVtable, src/Game/InventoryPatches.cpp:327
{0x0A999B40, 0x0A9A1C40}, // global, GNames, src/Game/Offsets.h:11
{0x0AA67B80, 0x0AA6FC80}, // global, GObjects, src/Game/Offsets.h:10
{0x0AAEA770, 0x0AAF2870}, // global, GWorld, src/Game/Offsets.h:12
