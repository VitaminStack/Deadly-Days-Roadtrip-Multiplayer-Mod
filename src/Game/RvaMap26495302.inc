// Erzeugt von tools/rva_map.py fuer Build 26495302. Referenz-RVA -> RVA in diesem Build, aufsteigend sortiert.
// Nur Zeilen mit geprueftem Ergebnis eintragen; fehlende Adressen bleiben Kommentar und liefern zur Laufzeit 0.
// Haendisch nachgetragene Zeilen mit "// von Hand" kennzeichnen, damit ein erneuter Lauf sie nicht still ersetzt.
{0x01272520, 0x01272700}, // eindeutig, ArrayResizeGrow, src/Game/CameraPatches.cpp:19
{0x012D6BA0, 0x012D6D80}, // eindeutig, ControllerSparseRemove, src/Game/PlayerPatches.cpp:29
{0x012F8400, 0x012F85E0}, // eindeutig, FreeArray, src/Game/ClientPatches.cpp:28
{0x01344470, 0x01344650}, // eindeutig, MapSparseRemove, src/Game/RemoteP2Patches.cpp:25
{0x01512250, 0x01512430}, // eindeutig, DefaultNetworkingSupport, src/Game/InventoryPatches.cpp:27
{0x0160C040, 0x0160C220}, // eindeutig, ProcessEvent, src/Game/Offsets.h:13
{0x024E9410, 0x024E95F0}, // eindeutig, DelegateBroadcast, src/Game/RemoteP2Patches.cpp:27
{0x0288B510, 0x0288B710}, // eindeutig, CreateChildActor, src/Game/MapPatches.cpp:23
{0x0289DB60, 0x0289DD60}, // eindeutig, IsChildActorReplicated, src/Game/MapPatches.cpp:24
{0x02A678F0, 0x02A67AF0}, // eindeutig, BeginDeferredActorSpawnFromClass, src/Game/ClientPatches.cpp:35
{0x02CF362E, 0x02CF382E}, // eindeutig, NetGuidExpiry, src/Game/ClientPatches.cpp:30
{0x02CF3C38, 0x02CF3E38}, // eindeutig, NetGuidNext, src/Game/ClientPatches.cpp:31
{0x031471C0, 0x031473C0}, // eindeutig, FindTimer, src/Game/InventoryPatches.cpp:191
{0x0314D0B0, 0x0314D2B0}, // eindeutig, ClearTimer, src/Game/InventoryPatches.cpp:192
{0x0314D4D0, 0x0314D6D0}, // eindeutig, SetNextTick, src/Game/InventoryPatches.cpp:190
{0x03262830, 0x03262A30}, // eindeutig, SetClientTravel, src/Game/TravelPatches.cpp:16
{0x04B68180, 0x04B68380}, // eindeutig, AttributeLookupFunction, src/Game/ClientPatches.cpp:19
// FEHLT 0x05666FA5 // fehlt, At, src/Game/ClientPatches.cpp:285
{0x057BD5E0, 0x057BDCD0}, // eindeutig, ControllerSetAdd, src/Game/PlayerPatches.cpp:28
{0x057CB3C5, 0x057CB5A5}, // ueber-funktion, ScheduleCall, src/Game/InventoryPatches.cpp:189
{0x057D0A40, 0x057D0C60}, // eindeutig, DetectorEndPlay, src/Game/InventoryPatches.cpp:187
{0x057DC840, 0x057DC810}, // von Hand, Exec-Thunk OnRep_Health, OnRepHealth, src/Game/HealthBarPatches.cpp:18
{0x057DCEA0, 0x057DCE70}, // von Hand, Exec-Thunk OnRep_MaxHealth, OnRepMaxHealth, src/Game/HealthBarPatches.cpp:19
{0x058393CC, 0x0583AEEC}, // eindeutig, ComponentControllerRead, src/Game/ClientPatches.cpp:23
{0x05839540, 0x0583B060}, // eindeutig, ComponentControllerExit, src/Game/ClientPatches.cpp:24
{0x0584CD82, 0x0584DF32}, // eindeutig, RunEndLocalPlayerSite, src/Game/PlayerPatches.cpp:32
{0x0584CD90, 0x0584DF40}, // eindeutig, RunEndContinue, src/Game/PlayerPatches.cpp:33
{0x058A5266, 0x058A6466}, // eindeutig, SneakSameTarget, src/Game/ClientPatches.cpp:32
{0x058A526C, 0x058A646C}, // eindeutig, SneakRetry, src/Game/ClientPatches.cpp:33
{0x058A52FE, 0x058A64FE}, // eindeutig, At, src/Game/ClientPatches.cpp:318
{0x058A530D, 0x058A650D}, // eindeutig, SneakReady, src/Game/ClientPatches.cpp:34
{0x058A6060, 0x058A7260}, // eindeutig, At, src/Game/ClientPatches.cpp:272
{0x058AA5F0, 0x058AB9A0}, // eindeutig, At, src/Game/ClientPatches.cpp:286
{0x058B0F6E, 0x058B1D3E}, // eindeutig, At, src/Game/ClientPatches.cpp:273
{0x058B4160, 0x058B4D30}, // eindeutig, ExpAggregate, src/Game/ClientPatches.cpp:26
{0x058B419B, 0x058B4D6B}, // eindeutig, At, src/Game/ClientPatches.cpp:271
{0x058BB360, 0x058BBD90}, // eindeutig, BlockadeLoadingFinished, src/Game/ClientPatches.cpp:29
{0x058BE7C0, 0x058BF120}, // eindeutig, UpdateBounds, src/Game/ExpPatches.cpp:27
{0x058BE8B0, 0x058BF210}, // eindeutig, ReceiveExpImpl, src/Game/ExpPatches.cpp:26
{0x058C1030, 0x058C1B10}, // eindeutig, SpawnRequest, src/Game/ExpPatches.cpp:25
{0x058CEDC7, 0x058CF517}, // eindeutig, SpawnerPlayerRead, src/Game/PlayerPatches.cpp:24
{0x058CF2B7, 0x058CFA07}, // eindeutig, SpawnerNoPlayerExit, src/Game/PlayerPatches.cpp:25
{0x058D1940, 0x058D1A80}, // eindeutig, MapAdd, src/Game/RemoteP2Patches.cpp:24
{0x058D93B0, 0x058DA3E0}, // eindeutig, AssignDevice, src/Game/LocalDevicePatches.cpp:19
{0x058E5690, 0x058E73B0}, // eindeutig, DraftAssignDevice, src/Game/LocalDevicePatches.cpp:20
// FEHLT 0x058E84C0 // fehlt, FinishMission, src/Game/ClientPatches.cpp:37
{0x058EE0D0, 0x058EFFD0}, // eindeutig, GetController, src/Game/LocalDevicePatches.cpp:22
{0x058FB020, 0x058FD3A0}, // eindeutig, RegisterController, src/Game/PlayerPatches.cpp:30
{0x058FB11D, 0x058FD49D}, // eindeutig, At, src/Game/PlayerPatches.cpp:370
{0x058FB133, 0x058FD4B3}, // eindeutig, At, src/Game/PlayerPatches.cpp:371
{0x05901CE8, 0x05904DE8}, // eindeutig, SpawnNullSite, src/Game/PlayerPatches.cpp:21
{0x05901E53, 0x05904F53}, // eindeutig, SpawnSkipTarget, src/Game/PlayerPatches.cpp:22
{0x05901E90, 0x05904F90}, // eindeutig, SpawnLocal, src/Game/LocalDevicePatches.cpp:21
{0x059072AA, 0x0590B2AA}, // eindeutig, DeviceCheckSite, src/Game/RemoteP2Patches.cpp:28
{0x059072AF, 0x0590B2AF}, // eindeutig, SoloLossTest, src/Game/RemoteP2Patches.cpp:29
{0x059072B4, 0x0590B2B4}, // eindeutig, NeedReconnect, src/Game/RemoteP2Patches.cpp:30
{0x0593DAF3, 0x05941A43}, // eindeutig, ThreatFreezeBranch, src/Game/ClientPatches.cpp:25
{0x05959700, 0x0595DA20}, // eindeutig, CreateItemInstance, src/Game/RewardPatches.cpp:43
{0x05969750, 0x0596D8C0}, // eindeutig, InventoryReadyForReplication, src/Game/InventoryPatches.cpp:28
{0x05976AD0, 0x0597ABB0}, // eindeutig, SeamlessTravelToLevel, src/Game/TravelPatches.cpp:17
{0x05979B30, 0x0597DC10}, // eindeutig, SetRewardItems, src/Game/RewardPatches.cpp:38
{0x05980AE0, 0x05984CD0}, // eindeutig, UpgradeItems, src/Game/RewardPatches.cpp:47
{0x05991B70, 0x05995D30}, // eindeutig, ClearSpawnArea, src/Game/PlayerPatches.cpp:31
{0x05994BB0, 0x05998D70}, // eindeutig, LootPawnRead, src/Game/PlayerPatches.cpp:26
{0x05994BD1, 0x05998D91}, // eindeutig, LootNoRootTarget, src/Game/PlayerPatches.cpp:27
{0x059AB040, 0x059AF200}, // eindeutig, ItemLoot, src/Game/InventoryPatches.cpp:34
{0x059B7E17, 0x059BBFC7}, // eindeutig, AttributeLookupCallSite, src/Game/ClientPatches.cpp:18
{0x059B858A, 0x059BC73A}, // eindeutig, AttributeTickExit, src/Game/ClientPatches.cpp:20
{0x059C3E10, 0x059C7FC0}, // eindeutig, CanBusTakeOff, src/Game/ClientPatches.cpp:36
{0x059C494F, 0x059C8AFF}, // eindeutig, At, src/Game/ClientPatches.cpp:333
{0x059CAE70, 0x059CFBA0}, // eindeutig, MapGenLambdaExecute, src/Game/InventoryPatches.cpp:198
{0x059CB020, 0x059CFD50}, // eindeutig, MapGenLambdaExecuteIfSafe, src/Game/InventoryPatches.cpp:199
{0x059DFD5A, 0x059E4B2A}, // eindeutig, MapGenHostObjectRead, src/Game/ClientPatches.cpp:21
{0x059DFE07, 0x059E4BD7}, // eindeutig, MapGenHostObjectExit, src/Game/ClientPatches.cpp:22
{0x059EE6A0, 0x059F3470}, // eindeutig, SetupPcgVolume, src/Game/MapPatches.cpp:25
{0x059EF6D0, 0x059F44A0}, // eindeutig, StartMapGeneration, src/Game/MapPatches.cpp:22
{0x05A0DA90, 0x05A11750}, // eindeutig, CameraPlayersFunction, src/Game/CameraPatches.cpp:18
{0x05A17ACC, 0x05A1B9EC}, // eindeutig, RestrictorCountSite, src/Game/CameraPatches.cpp:32
{0x05A17AD9, 0x05A1B9F9}, // eindeutig, RestrictorContinue, src/Game/CameraPatches.cpp:34
{0x05A17B88, 0x05A1BAA8}, // eindeutig, RestrictorSkip, src/Game/CameraPatches.cpp:33
{0x05A42F50, 0x05A471D0}, // eindeutig, OpenChoices, src/Game/AirdropPatches.cpp:13
{0x05A43400, 0x05A47660}, // eindeutig, OpenCrate, src/Game/AirdropPatches.cpp:12
{0x05A6CB20, 0x05A71900}, // von Hand, Exec-Thunk HandleConfirmButton, PanelConfirm, src/Game/RewardPatches.cpp:39
{0x05A7350B, 0x05A782FB}, // eindeutig, ClientRefreshCallSite, src/Game/ClientPatches.cpp:16
{0x05A8BAB0, 0x05A90940}, // von Hand, Exec-Thunk SetupInventory, ClientRefreshFunction, src/Game/ClientPatches.cpp:17 und PanelSetupInventory, src/Game/RewardPatches.cpp:40
{0x05AB5120, 0x05ABA0F0}, // von Hand, Aufrufer, Code umgeordnet, RepresentationSize, src/Game/InventoryPatches.cpp:205
{0x05B028D0, 0x05B08070}, // eindeutig, LocalViewportCorners, src/Game/ExpPatches.cpp:28
{0x05B07680, 0x05B0CE30}, // eindeutig, GetPlayerCharacters, src/Game/ClientPatches.cpp:27
{0x05B0A2E5, 0x05B0FA95}, // eindeutig, PlayerIndexNullSite, src/Game/PlayerPatches.cpp:23
{0x0939CE08, 0x093A1F98}, // von Hand, Konstruktor-Verweis, SlotVtable, src/Game/InventoryPatches.cpp:23
{0x094B46E8, 0x094B9A08}, // vtable, ItemVtable, src/Game/InventoryPatches.cpp:24
{0x094BA3F0, 0x094BF710}, // vtable, InventoryVtable, src/Game/InventoryPatches.cpp:22
{0x094C1758, 0x094C6A78}, // von Hand, Konstruktor-Verweis, DetectorVtable, src/Game/InventoryPatches.cpp:186
{0x09613280, 0x096191F0}, // vtable, MapGenLambdaVtable, src/Game/InventoryPatches.cpp:197
{0x0A993AC0, 0x0A999B40}, // global, GNames, src/Game/Offsets.h:11
{0x0AA61B00, 0x0AA67B80}, // global, GObjects, src/Game/Offsets.h:10
{0x0AAE46F0, 0x0AAEA770}, // global, GWorld, src/Game/Offsets.h:12
