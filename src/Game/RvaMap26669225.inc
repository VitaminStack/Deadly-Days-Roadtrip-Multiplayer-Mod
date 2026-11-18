// Erzeugt von tools/rva_map.py fuer Build 26669225. Referenz-RVA -> RVA in diesem Build, aufsteigend sortiert.
// Nur Zeilen mit geprueftem Ergebnis eintragen; fehlende Adressen bleiben Kommentar und liefern zur Laufzeit 0.
// Haendisch nachgetragene Zeilen mit "// von Hand" kennzeichnen, damit ein erneuter Lauf sie nicht still ersetzt.
{0x01272700, 0x01272700}, // eindeutig, ArrayResizeGrow, src/Game/CameraPatches.cpp:19
{0x012BC260, 0x012BC260}, // eindeutig, DamageInfoMapDestruct, src/Game/DamageNumberPatches.cpp:32
{0x012D6D80, 0x012D6D80}, // eindeutig, ControllerSparseRemove, src/Game/PlayerPatches.cpp:31
{0x012F85E0, 0x012F85E0}, // eindeutig, FreeArray, src/Game/ClientPatches.cpp:30
{0x01344650, 0x01344650}, // eindeutig, MapSparseRemove, src/Game/RemoteP2Patches.cpp:28
{0x01512430, 0x01512430}, // eindeutig, DefaultNetworkingSupport, src/Game/InventoryPatches.cpp:27
{0x0160C220, 0x0160C220}, // eindeutig, ProcessEvent, src/Game/Offsets.h:13
{0x024E95F0, 0x024E95F0}, // eindeutig, RegistrationBroadcast, src/Game/PlayerPatches.cpp:33
{0x0288B710, 0x0288B6E0}, // eindeutig, CreateChildActor, src/Game/MapPatches.cpp:23
{0x0289DD60, 0x0289DD30}, // eindeutig, IsChildActorReplicated, src/Game/MapPatches.cpp:24
{0x02A67AF0, 0x02A67AC0}, // eindeutig, BeginDeferredActorSpawnFromClass, src/Game/ClientPatches.cpp:37
{0x02CF382E, 0x02CF37FE}, // eindeutig, NetGuidExpiry, src/Game/ClientPatches.cpp:32
{0x02CF3E38, 0x02CF3E08}, // eindeutig, NetGuidNext, src/Game/ClientPatches.cpp:33
{0x031473C0, 0x03147390}, // eindeutig, FindTimer, src/Game/InventoryPatches.cpp:321
{0x0314D2B0, 0x0314D280}, // eindeutig, ClearTimer, src/Game/InventoryPatches.cpp:322
{0x0314D6D0, 0x0314D6A0}, // eindeutig, SetNextTick, src/Game/InventoryPatches.cpp:320
{0x03262A30, 0x03262A00}, // eindeutig, SetClientTravel, src/Game/TravelPatches.cpp:16
{0x04B68380, 0x04B68B40}, // eindeutig, AttributeLookupFunction, src/Game/ClientPatches.cpp:21
{0x04BC58D0, 0x04BC6090}, // eindeutig, ExecuteCueNonReplicated, src/Game/BodyPartPatches.cpp:28
{0x056591B0, 0x05659940}, // eindeutig, DamageInfoConstruct, src/Game/DamageNumberPatches.cpp:31
{0x056676F5, 0x05667E85}, // von Hand (einziger jmp auf BlockadeLoadingFinished 0x058BC360, Verschiebung +0x790 wie die Nachbarn), At, src/Game/ClientPatches.cpp:320
{0x05707870, 0x05708000}, // von Hand (10 eindeutige Fenster, 400 Befehle maskiert gleich, Verschiebung +0x790), ReloadSoundEvent, src/Game/WeaponSoundPatches.cpp:99
{0x057BDCD0, 0x057BE450}, // eindeutig, ControllerSetAdd, src/Game/PlayerPatches.cpp:30
{0x057CB5A5, 0x057CBD25}, // ueber-funktion, ScheduleCall, src/Game/InventoryPatches.cpp:319
{0x057D0C60, 0x057D1470}, // eindeutig, DetectorEndPlay, src/Game/InventoryPatches.cpp:317
{0x057DBA40, 0x057DC250}, // von Hand (Exec-Thunk von OnRep_Armor aus der Native-Tabelle, Rumpf liest +0x1B0), OnRepArmor, src/Game/HealthBarPatches.cpp:25
{0x057DC810, 0x057DD020}, // von Hand (60 Befehle eindeutig, Attributname Health im Kaltpfad), OnRepHealth, src/Game/HealthBarPatches.cpp:21
{0x057DCD60, 0x057DD570}, // von Hand (Exec-Thunk von OnRep_MaxArmor aus der Native-Tabelle, Rumpf liest +0x190), OnRepMaxArmor, src/Game/HealthBarPatches.cpp:26
{0x057DCE70, 0x057DD680}, // von Hand (60 Befehle eindeutig, Attributname MaxHealth im Kaltpfad), OnRepMaxHealth, src/Game/HealthBarPatches.cpp:22
{0x057DD080, 0x057DD890}, // von Hand (60 Befehle eindeutig, Attributname MaxStamina im Kaltpfad), OnRepMaxStamina, src/Game/HealthBarPatches.cpp:24
{0x057DDE40, 0x057DE650}, // eindeutig, OnRepStamina, src/Game/HealthBarPatches.cpp:23
{0x05837220, 0x05837820}, // eindeutig, ApplyInitial, src/Game/CharacterPatches.cpp:29
{0x0583AEEC, 0x0583B4EC}, // eindeutig, ComponentControllerRead, src/Game/ClientPatches.cpp:25
{0x0583B060, 0x0583B660}, // eindeutig, ComponentControllerExit, src/Game/ClientPatches.cpp:26
{0x05840DF0, 0x058413F0}, // eindeutig, ApplyDefinition, src/Game/CharacterPatches.cpp:30
{0x0584DF32, 0x0584E532}, // eindeutig, RunEndLocalPlayerSite, src/Game/PlayerPatches.cpp:35
{0x0584DF40, 0x0584E540}, // eindeutig, RunEndContinue, src/Game/PlayerPatches.cpp:36
{0x0585AEA0, 0x0585B4A0}, // eindeutig, CameraFollowTick, src/Game/PlayerPatches.cpp:38
{0x058908C0, 0x05890E90}, // eindeutig, SpawnCircleTelegraph, src/Game/EnemyTelegraphPatches.cpp:32
{0x058A3DB0, 0x058A4380}, // eindeutig, ActivateLineTelegraph, src/Game/EnemyTelegraphPatches.cpp:33
{0x058A6466, 0x058A6A36}, // eindeutig, SneakSameTarget, src/Game/ClientPatches.cpp:34
{0x058A646C, 0x058A6A3C}, // eindeutig, SneakRetry, src/Game/ClientPatches.cpp:35
{0x058A64FE, 0x058A6ACE}, // eindeutig, At, src/Game/ClientPatches.cpp:353
{0x058A650D, 0x058A6ADD}, // eindeutig, SneakReady, src/Game/ClientPatches.cpp:36
{0x058A7260, 0x058A7830}, // eindeutig, At, src/Game/ClientPatches.cpp:307
{0x058A9D30, 0x058AA300}, // eindeutig, CalculateLineTelegraphEndPoint, src/Game/EnemyTelegraphPatches.cpp:34
{0x058AB9A0, 0x058ABF70}, // eindeutig, At, src/Game/ClientPatches.cpp:321
{0x058B1130, 0x058B1700}, // eindeutig, EndTackle, src/Game/EnemyTelegraphPatches.cpp:35
{0x058B1D3E, 0x058B230E}, // eindeutig, At, src/Game/ClientPatches.cpp:308
{0x058B1F40, 0x058B2510}, // eindeutig, ExecuteTackle, src/Game/EnemyTelegraphPatches.cpp:36
{0x058B4D30, 0x058B5300}, // eindeutig, ExpAggregate, src/Game/ClientPatches.cpp:28
{0x058B4D6B, 0x058B533B}, // eindeutig, At, src/Game/ClientPatches.cpp:306
{0x058BBD90, 0x058BC360}, // eindeutig, BlockadeLoadingFinished, src/Game/ClientPatches.cpp:31
{0x058BF120, 0x058BF6F0}, // eindeutig, UpdateBounds, src/Game/ExpPatches.cpp:27
{0x058BF210, 0x058BF7E0}, // eindeutig, ReceiveExpImpl, src/Game/ExpPatches.cpp:26
{0x058C1B10, 0x058C20E0}, // eindeutig, SpawnRequest, src/Game/ExpPatches.cpp:25
{0x058CE280, 0x058CE890}, // von Hand (Ziel des Exec-Thunks 0x0579B580, 43 eindeutige Fenster; Anfang geaendert, eigene Pruefbytes), TryBuyItem, src/Game/RewardPatches.cpp:54
{0x058CF517, 0x058CFB47}, // eindeutig, SpawnerPlayerRead, src/Game/PlayerPatches.cpp:26
{0x058CFA07, 0x058D0037}, // eindeutig, SpawnerNoPlayerExit, src/Game/PlayerPatches.cpp:27
{0x058D1A80, 0x058D20B0}, // eindeutig, MapAdd, src/Game/RemoteP2Patches.cpp:27
{0x058DA3E0, 0x058DAB10}, // eindeutig, AssignDevice, src/Game/LocalDevicePatches.cpp:19
{0x058E73B0, 0x058E7D20}, // eindeutig, DraftAssignDevice, src/Game/LocalDevicePatches.cpp:20
{0x058EA1A0, 0x058EAC00}, // eindeutig, FinishMission, src/Game/ClientPatches.cpp:39
{0x058EFFD0, 0x058F0A90}, // eindeutig, GetController, src/Game/LocalDevicePatches.cpp:22
{0x058FD3A0, 0x058FDE60}, // eindeutig, RegisterController, src/Game/PlayerPatches.cpp:32
{0x058FD49D, 0x058FDF5D}, // eindeutig, At, src/Game/PlayerPatches.cpp:480
{0x058FD4B3, 0x058FDF73}, // eindeutig, At, src/Game/PlayerPatches.cpp:481
{0x05904DE8, 0x05905A88}, // eindeutig, SpawnNullSite, src/Game/PlayerPatches.cpp:23
{0x05904F53, 0x05905BF3}, // eindeutig, SpawnSkipTarget, src/Game/PlayerPatches.cpp:24
{0x05904F90, 0x05905C30}, // eindeutig, SpawnLocal, src/Game/LocalDevicePatches.cpp:21
{0x05907ED0, 0x05908B60}, // eindeutig, TriggerEndRound, src/Game/ClientPatches.cpp:40
{0x0590B2AA, 0x0590BF3A}, // eindeutig, DeviceCheckSite, src/Game/RemoteP2Patches.cpp:31
{0x0590B2AF, 0x0590BF3F}, // eindeutig, SoloLossTest, src/Game/RemoteP2Patches.cpp:32
{0x0590B2B4, 0x0590BF44}, // eindeutig, NeedReconnect, src/Game/RemoteP2Patches.cpp:33
{0x05936BE0, 0x05937870}, // eindeutig, RunSaveRoundChanged, src/Game/ClientPatches.cpp:43
{0x0593EBA0, 0x0593F830}, // eindeutig, RunSaveResave, src/Game/ClientPatches.cpp:44
{0x05941A43, 0x059426D3}, // eindeutig, ThreatFreezeBranch, src/Game/ClientPatches.cpp:27
{0x0595DA20, 0x0595E6E0}, // eindeutig, CreateItemInstance, src/Game/RewardPatches.cpp:44
{0x0596BA70, 0x0596C820}, // eindeutig, HandleItemSold, src/Game/RewardPatches.cpp:51
{0x0596D8C0, 0x0596E670}, // eindeutig, InventoryReadyForReplication, src/Game/InventoryPatches.cpp:28
{0x0597ABB0, 0x0597B960}, // eindeutig, SeamlessTravelToLevel, src/Game/TravelPatches.cpp:17
{0x0597DC10, 0x0597E9C0}, // eindeutig, SetRewardItems, src/Game/RewardPatches.cpp:39
{0x05984CD0, 0x05985A80}, // eindeutig, UpgradeItems, src/Game/RewardPatches.cpp:48
{0x05995D30, 0x05996AF0}, // eindeutig, ClearSpawnArea, src/Game/PlayerPatches.cpp:34
{0x05998D70, 0x05999B30}, // eindeutig, LootPawnRead, src/Game/PlayerPatches.cpp:28
{0x05998D91, 0x05999B51}, // eindeutig, LootNoRootTarget, src/Game/PlayerPatches.cpp:29
{0x059AF200, 0x059AFFC0}, // eindeutig, ItemLoot, src/Game/InventoryPatches.cpp:34
{0x059AF3F0, 0x059B01B0}, // eindeutig, LootBaseLoot, src/Game/InventoryPatches.cpp:43
{0x059AF660, 0x059B0420}, // eindeutig, ScrapLoot, src/Game/InventoryPatches.cpp:42
{0x059BBFC7, 0x059BCD87}, // eindeutig, AttributeLookupCallSite, src/Game/ClientPatches.cpp:20
{0x059BC73A, 0x059BD4FA}, // eindeutig, AttributeTickExit, src/Game/ClientPatches.cpp:22
{0x059C7FC0, 0x059C8D80}, // eindeutig, CanBusTakeOff, src/Game/ClientPatches.cpp:38
{0x059C8AFF, 0x059C98BF}, // eindeutig, At, src/Game/ClientPatches.cpp:368
{0x059CFBA0, 0x059D0960}, // eindeutig, MapGenLambdaExecute, src/Game/InventoryPatches.cpp:328
{0x059CFD50, 0x059D0B10}, // eindeutig, MapGenLambdaExecuteIfSafe, src/Game/InventoryPatches.cpp:329
{0x059E4B2A, 0x059E58EA}, // eindeutig, MapGenHostObjectRead, src/Game/ClientPatches.cpp:23
{0x059E4BD7, 0x059E5997}, // eindeutig, MapGenHostObjectExit, src/Game/ClientPatches.cpp:24
{0x059F3470, 0x059F4230}, // eindeutig, SetupPcgVolume, src/Game/MapPatches.cpp:25
{0x059F44A0, 0x059F5260}, // eindeutig, StartMapGeneration, src/Game/MapPatches.cpp:22
{0x05A11750, 0x05A12510}, // eindeutig, CameraPlayersFunction, src/Game/CameraPatches.cpp:18
{0x05A1B9EC, 0x05A1C7AC}, // eindeutig, RestrictorCountSite, src/Game/CameraPatches.cpp:32
{0x05A1B9F9, 0x05A1C7B9}, // eindeutig, RestrictorContinue, src/Game/CameraPatches.cpp:34
{0x05A1BAA8, 0x05A1C868}, // eindeutig, RestrictorSkip, src/Game/CameraPatches.cpp:33
{0x05A22DB0, 0x05A23B70}, // eindeutig, DamageHandler, src/Game/DamageNumberPatches.cpp:30
{0x05A471D0, 0x05A47F90}, // eindeutig, OpenChoices, src/Game/AirdropPatches.cpp:13
{0x05A47660, 0x05A48420}, // eindeutig, OpenCrate, src/Game/AirdropPatches.cpp:12
{0x05A71900, 0x05A72720}, // eindeutig, PanelConfirm, src/Game/RewardPatches.cpp:40
{0x05A782FB, 0x05A7911B}, // eindeutig, ClientRefreshCallSite, src/Game/ClientPatches.cpp:18
{0x05A90940, 0x05A91760}, // eindeutig, ClientRefreshFunction, src/Game/ClientPatches.cpp:19
{0x05ABA0F0, 0x05ABAFC0}, // eindeutig, RepresentationSize, src/Game/InventoryPatches.cpp:335
{0x05B08070, 0x05B08460}, // eindeutig, LocalViewportCorners, src/Game/ExpPatches.cpp:28
{0x05B0CE30, 0x05B0D620}, // eindeutig, GetPlayerCharacters, src/Game/ClientPatches.cpp:29
{0x05B0FA95, 0x05B102B5}, // eindeutig, PlayerIndexNullSite, src/Game/PlayerPatches.cpp:25
{0x093A1F98, 0x093A2EF8}, // von Hand (82 von 84 Eintraegen umgerechnet, alle gleich), SlotVtable, src/Game/InventoryPatches.cpp:23
{0x094B9A08, 0x094BAB18}, // vtable, ItemVtable, src/Game/InventoryPatches.cpp:24
{0x094BF710, 0x094C0820}, // vtable, InventoryVtable, src/Game/InventoryPatches.cpp:22
{0x094C6A78, 0x094C7B88}, // von Hand (162 von 164 Eintraegen umgerechnet, alle gleich), DetectorVtable, src/Game/InventoryPatches.cpp:316
{0x096191F0, 0x0961A4C0}, // vtable, MapGenLambdaVtable, src/Game/InventoryPatches.cpp:327
{0x0A999B40, 0x0A99BB40}, // global, GNames, src/Game/Offsets.h:11
{0x0AA67B80, 0x0AA69B80}, // global, GObjects, src/Game/Offsets.h:10
{0x0AAEA770, 0x0AAEC770}, // global, GWorld, src/Game/Offsets.h:12
