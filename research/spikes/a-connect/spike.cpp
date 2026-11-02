// Wegwerf-Spike: Fuehrt Konsolenbefehle im Spiel aus (Shipping-Build hat keine Konsole).
// Befehle kommen als Datei %TEMP%\ddspike\<pid>.cmd und laufen auf dem Game Thread.
// Offsets gelten nur fuer Build 26187885.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

namespace
{
constexpr uintptr_t kGObjects = 0x0AA61B00;
constexpr uintptr_t kNameBlocks = 0x0A993AC0 + 0x10;
constexpr uintptr_t kGWorld = 0x0AAE46F0;
constexpr int kProcessEventIndex = 0x4A;
constexpr uintptr_t kSpawnNullSite = 0x5901CE8;
constexpr uintptr_t kSpawnSkipTarget = 0x5901E53;
constexpr uintptr_t kPlayerIndexNullSite = 0x5B0A2E5;
constexpr uintptr_t kClientRefreshCallSite = 0x5A7350B;
constexpr uintptr_t kClientRefreshFunction = 0x5A8BAB0;
constexpr uintptr_t kCameraPlayersFunction = 0x5A0DA90;
constexpr uintptr_t kAttributeLookupCallSite = 0x59B7E17;
constexpr uintptr_t kAttributeLookupFunction = 0x4B68180;
constexpr uintptr_t kAttributeTickExit = 0x59B858A;
constexpr uintptr_t kArrayResizeGrow = 0x1272520;
constexpr uintptr_t kWorldGameState = 0x1B0;
constexpr uintptr_t kWorldGameInstance = 0x228;
constexpr uintptr_t kGameStatePlayerArray = 0x2C0;
constexpr uintptr_t kPlayerStatePawn = 0x320;
constexpr uintptr_t kCharacterMovementBase = 0x350;
constexpr uintptr_t kComponentMobility = 0x1AB;
constexpr uintptr_t kActorReplicatesBits = 0x5B;
constexpr uintptr_t kActorRemoteRole = 0x60;
constexpr uintptr_t kCharacterMovement = 0x330;
constexpr uintptr_t kMovementFlags = 0x579;
constexpr uintptr_t kFunctionFlags = 0xB0;
constexpr uintptr_t kFunctionExec = 0xD8;
constexpr uintptr_t kSetClientTravel = 0x3262830;
constexpr uintptr_t kSeamlessTravelToLevel = 0x5976AD0;
constexpr uintptr_t kLoadSettingsPackage = 0x10;
constexpr uintptr_t kWorldGameMode = 0x1A8;
constexpr uintptr_t kGameModeSpawnedPlayers = 0x388;
constexpr uintptr_t kActorOwner = 0x158;
constexpr uintptr_t kThreatFreezeBranch = 0x593DAF3;
constexpr uintptr_t kProcessEventNative = 0x160C040;
constexpr uintptr_t kItemsLoopRead = 0x5963604;
constexpr uintptr_t kItemsLoopNext = 0x596365F;
constexpr uintptr_t kSlotLoopRead = 0x5965660;
constexpr uintptr_t kSlotLoopNext = 0x596572C;
constexpr uintptr_t kComponentControllerRead = 0x58393CC;
constexpr uintptr_t kComponentControllerExit = 0x5839540;
constexpr uintptr_t kGridLoopRead = 0x5A7E060;
constexpr uintptr_t kGridLoopNext = 0x5A7E091;
constexpr uintptr_t kSpawnerPlayerRead = 0x58CEDC7;
constexpr uintptr_t kSpawnerNoPlayerExit = 0x58CF2B7;
constexpr uintptr_t kLootPawnRead = 0x5994BB0;
constexpr uintptr_t kLootNoRootTarget = 0x5994BD1;
constexpr uintptr_t kWorldNetDriver = 0x38;
constexpr uintptr_t kDriverServerConnection = 0x100;
constexpr UINT kRunCommand = WM_APP + 77;
constexpr UINT_PTR kSessionTimer = 7701;

struct FString
{
    const wchar_t* Data;
    int32_t Num;
    int32_t Max;
};

struct ExecuteConsoleCommandParams
{
    void* WorldContext;
    FString Command;
    void* SpecificPlayer;
};

uintptr_t Base;
WNDPROC OriginalWndProc;
std::wstring Pending;
std::wstring LogPath;
bool RedirectTravel = false;
HWND GameWindow = nullptr;

using SetClientTravelFn = void(__fastcall*)(uintptr_t, uintptr_t, const wchar_t*, uint32_t);
SetClientTravelFn OriginalSetClientTravel;
using SeamlessTravelFn = void(__fastcall*)(uintptr_t, uintptr_t);
SeamlessTravelFn OriginalSeamlessTravel;

void Log(const std::string& Line)
{
    FILE* File = nullptr;
    if (_wfopen_s(&File, LogPath.c_str(), L"a") == 0 && File)
    {
        fprintf(File, "%s\n", Line.c_str());
        fclose(File);
    }
}

template <typename T> T Read(uintptr_t Address) { return *reinterpret_cast<T*>(Address); }

std::string NameFromIndex(int32_t Index)
{
    const uintptr_t Block = Read<uintptr_t>(Base + kNameBlocks + 8 * (Index >> 16));
    const uintptr_t Entry = Block + (Index & 0xFFFF) * 2;
    const uint16_t Header = Read<uint16_t>(Entry);
    if (Header & 1)
        return {};
    return std::string(reinterpret_cast<const char*>(Entry + 2), Header >> 6);
}

std::string NameOf(uintptr_t Object) { return NameFromIndex(Read<int32_t>(Object + 0x18)); }

uintptr_t FindObject(const char* Name, const char* OuterName)
{
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        if (!Chunk)
            continue;
        const uintptr_t Object = Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8);
        if (!Object || NameOf(Object) != Name)
            continue;
        const uintptr_t Outer = Read<uintptr_t>(Object + 0x20);
        if (!OuterName || (Outer && NameOf(Outer) == OuterName))
            return Object;
    }
    return 0;
}

uintptr_t FindInstance(const char* ClassName)
{
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        if (!Chunk)
            continue;
        const uintptr_t Object = Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8);
        if (!Object || (Read<uint32_t>(Object + 8) & 0x10) != 0)
            continue;
        const uintptr_t Class = Read<uintptr_t>(Object + 0x10);
        if (Class && NameOf(Class) == ClassName)
            return Object;
    }
    return 0;
}

void RunConsoleCommand(const std::wstring& Command)
{
    const uintptr_t Function = FindObject("ExecuteConsoleCommand", "KismetSystemLibrary");
    const uintptr_t Library = FindObject("Default__KismetSystemLibrary", nullptr);
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    if (!Function || !Library || !World)
    {
        Log("Objekte nicht gefunden");
        return;
    }

    ExecuteConsoleCommandParams Params{};
    Params.WorldContext = reinterpret_cast<void*>(World);
    Params.Command = {Command.c_str(), static_cast<int32_t>(Command.size() + 1), static_cast<int32_t>(Command.size() + 1)};

    using ProcessEventFn = void(__fastcall*)(uintptr_t, uintptr_t, void*);
    const auto ProcessEvent = Read<ProcessEventFn*>(Library)[kProcessEventIndex];
    ProcessEvent(Library, Function, &Params);
    Log("ausgefuehrt");
}

// Beim Client landet die gemeinsame Kamera nicht als ViewTarget, er schaut sonst vom Weltursprung aus.
void ViewThroughSharedCamera()
{
    const uintptr_t Camera = FindInstance("BP_PlayerCameraActor_C");
    const uintptr_t Controller = FindInstance("BP_DDPlayerController_C");
    const uintptr_t Function = FindObject("SetViewTargetWithBlend", "PlayerController");
    // Beim Spawn wird das ViewTarget noch ueberschrieben, deshalb eine Weile lang wiederholen.
    static uintptr_t LastCamera = 0;
    static int Repeats = 0;
    if (!Camera || !Controller || !Function)
        return;
    if (Camera != LastCamera)
        Repeats = 15;
    LastCamera = Camera;
    if (Repeats-- <= 0)
        return;
    struct
    {
        uintptr_t Target;
        float BlendTime;
        uint8_t BlendFunction;
        float BlendExponent;
        bool LockOutgoing;
    } Params{Camera, 0.f, 0, 0.f, false};
    using ProcessEventFn = void(__fastcall*)(uintptr_t, uintptr_t, void*);
    Read<ProcessEventFn*>(Controller)[kProcessEventIndex](Controller, Function, &Params);
    Log("Kamera: gesetzt");
}

// Host: Nimmt die Position von Remote-Figuren direkt vom Client an (bIgnoreClientMovementErrorChecksAndCorrection
// und bServerAcceptClientAuthoritativePosition). Die eigene Simulation des Hosts bremst diese Figuren sonst aus.
void AcceptClientPositions()
{
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    int Changed = 0;
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x10) != 0)
            continue;
        const uintptr_t Class = Read<uintptr_t>(Object + 0x10);
        if (!Class || NameOf(Class) != "BP_DD_Character_C" || Read<uint8_t>(Object + kActorRemoteRole) != 2)
            continue;
        auto& Flags = *reinterpret_cast<uint8_t*>(Read<uintptr_t>(Object + kCharacterMovement) + kMovementFlags);
        Changed += (Flags & 0x0A) != 0x0A;
        Flags |= 0x0A;
    }
    if (Changed)
        Log("Client-Positionen: " + std::to_string(Changed));
}

// Host: Die direkt geladene Mission startet keine Gegner, fuer Trefferversuche werden sie hier angestossen.
void ForceEnemyWave()
{
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    const uintptr_t CanSpawn = FindObject("SetCanSpawn", "EnemySpawner");
    const uintptr_t AutoSpawn = FindObject("ToggleAutoSpawning", "EnemySpawner");
    const uintptr_t Wave = FindObject("ForceSpawnWave", "EnemySpawner");
    if (!CanSpawn || !AutoSpawn || !Wave)
    {
        Log("Gegner: Funktionen nicht gefunden");
        return;
    }
    using ProcessEventFn = void(__fastcall*)(uintptr_t, uintptr_t, void*);
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x10) != 0)
            continue;
        bool IsSpawner = false;
        for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class && !IsSpawner; Class = Read<uintptr_t>(Class + 0x40))
            IsSpawner = NameOf(Class) == "EnemySpawner";
        if (!IsSpawner)
            continue;
        const auto ProcessEvent = Read<ProcessEventFn*>(Object)[kProcessEventIndex];
        uint32_t Enable = 1;
        ProcessEvent(Object, CanSpawn, &Enable);
        Enable = 1;
        ProcessEvent(Object, AutoSpawn, &Enable);
        int32_t Enemies = 10;
        ProcessEvent(Object, Wave, &Enemies);
        Log("Gegner: Welle angestossen");
    }
}

using ProcessEventFn = void(__fastcall*)(uintptr_t, uintptr_t, void*);
ProcessEventFn OriginalProcessEvent;

// Host: Beendet die Mission wie der abfahrende Bus, um den Rueckweg zu pruefen.
void FinishMission()
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t GameState = World ? Read<uintptr_t>(World + kWorldGameState) : 0;
    const uintptr_t Function = FindObject("FinishMission", "DDMissionGameState");
    if (!GameState || !Function)
    {
        Log("Missionsende: nicht gefunden");
        return;
    }
    uint32_t IsLoading = 0;
    Read<ProcessEventFn*>(GameState)[kProcessEventIndex](GameState, Function, &IsLoading);
    Log("Missionsende: ausgeloest");
}

// Host: Beendet den Run ueber die GameInstance, Grund wie ERoundChangeReason.
void EndRound(uint8_t Reason)
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t GameInstance = World ? Read<uintptr_t>(World + kWorldGameInstance) : 0;
    const uintptr_t Function = FindObject("TriggerEndRound", "DDMissionGameInstance");
    if (!GameInstance || !Function)
    {
        Log("Rundenende: nicht gefunden");
        return;
    }
    uint32_t Params = Reason;
    Read<ProcessEventFn*>(GameInstance)[kProcessEventIndex](GameInstance, Function, &Params);
    Log("Rundenende: ausgeloest");
}
std::map<std::string, int> RpcCounts;

// RPCs laufen auf beiden Seiten durch ProcessEvent. Zaehlt alles, was nach Client- oder Server-RPC aussieht.
void __fastcall TraceProcessEvent(uintptr_t Object, uintptr_t Function, void* Params)
{
    const std::string Name = NameOf(Function);
    if (Name.find("Server") != std::string::npos || Name.find("Client") != std::string::npos)
    {
        const int Count = ++RpcCounts[Name];
        if (Count == 1 || Count % 100 == 0)
            Log("RPC " + Name + " #" + std::to_string(Count));
    }
    OriginalProcessEvent(Object, Function, Params);
}

// Tauscht den ProcessEvent-Eintrag in der VTable der Spielfigur.
void TraceCharacterRpcs()
{
    const uintptr_t Character = FindInstance("BP_DD_Character_C");
    if (!Character || OriginalProcessEvent)
        return;
    auto* Slot = &Read<ProcessEventFn*>(Character)[kProcessEventIndex];
    DWORD Old = 0;
    VirtualProtect(Slot, sizeof(*Slot), PAGE_READWRITE, &Old);
    OriginalProcessEvent = *Slot;
    *Slot = TraceProcessEvent;
    VirtualProtect(Slot, sizeof(*Slot), Old, &Old);
    Log("RPC-Zaehler: aktiv");
}

using ExecFn = void(__fastcall*)(void*, void*, void*);

struct TracedFunction
{
    ExecFn Original;
    std::string Name;
    int Calls;
};

std::vector<TracedFunction> TracedFunctions;

void __fastcall TraceExec(void* Context, void* Stack, void* Result, size_t Index)
{
    TracedFunction& Function = TracedFunctions[Index];
    if (++Function.Calls <= 5)
        Log(std::to_string(GetTickCount64()) + " " + Function.Name);
    Function.Original(Context, Stack, Result);
}

// Blueprint-Aufrufe nativer Funktionen laufen ueber UFunction::Func. Pro Funktion leitet ein Stub durch TraceExec.
void TraceRunFlow()
{
    const char* const Owners[] = {
        "DDMissionGameInstance", "LoadingSubSystem", "RunPersistencyManager", "DDMissionGameMode", "DDMissionGameState",
        "DDBlueprintHelperFunctions", "Bus", "BusPanel", "ExtractionPointBase", "MissionGenerationSubsystem",
        "CharacterSelectionManager", "CharacterSelectPanel", "EndOfDayUpgradeManager", "RoundEndPanel", "MapGenerator",
        "LocalCoopSubsystem", "PauseMenuWidget"};
    if (!TracedFunctions.empty())
        return;
    TracedFunctions.reserve(4096);

    auto* Stub = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096 * 24, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    for (int32_t i = 0; i < Count && TracedFunctions.size() < 4096; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        const uintptr_t Class = Object ? Read<uintptr_t>(Object + 0x10) : 0;
        if (!Class || NameOf(Class) != "Function" || !(Read<uint32_t>(Object + kFunctionFlags) & 0x400))
            continue;
        const uintptr_t Outer = Read<uintptr_t>(Object + 0x20);
        const std::string OuterName = Outer ? NameOf(Outer) : std::string();
        bool Wanted = false;
        for (const char* Owner : Owners)
            Wanted = Wanted || OuterName == Owner;
        if (!Wanted)
            continue;

        auto& Func = *reinterpret_cast<ExecFn*>(Object + kFunctionExec);
        char Address[32]{};
        sprintf_s(Address, "%llX", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(Func) - Base));
        if (NameOf(Object).find("Load") != std::string::npos || NameOf(Object).find("Travel") != std::string::npos)
            Log("Exec " + NameOf(Object) + " " + Address);
        const uint64_t Index = TracedFunctions.size();
        const uint64_t Hook = reinterpret_cast<uint64_t>(&TraceExec);
        TracedFunctions.push_back({Func, OuterName + "." + NameOf(Object), 0});
        Stub[0] = 0x49; Stub[1] = 0xB9;                     // mov r9,Index
        memcpy(Stub + 2, &Index, 8);
        Stub[10] = 0x48; Stub[11] = 0xB8;                   // mov rax,TraceExec
        memcpy(Stub + 12, &Hook, 8);
        Stub[20] = 0xFF; Stub[21] = 0xE0;                   // jmp rax
        Func = reinterpret_cast<ExecFn>(Stub);
        Stub += 24;
    }
    Log("Ablauf-Trace: " + std::to_string(TracedFunctions.size()) + " Funktionen");
}

// Der Spielablauf reist per SetClientTravel, das nimmt den Client nicht mit. Scharf geschaltet wird daraus servertravel.
void __fastcall SetClientTravelHook(uintptr_t Engine, uintptr_t World, const wchar_t* Url, uint32_t Type)
{
    if (!RedirectTravel || wcsstr(Url, L"listen") || wcsstr(Url, L"MainMenu"))
    {
        OriginalSetClientTravel(Engine, World, Url, Type);
        return;
    }
    const std::wstring Target(Url);
    Log("Reise umgeleitet: " + std::string(Target.begin(), Target.end()));
    Pending = L"servertravel " + Target + L"?listen";
    PostMessageW(GameWindow, kRunCommand, 0, 0);
}

// Missionen laufen ueber SeamlessTravelToLevel, das reist nur lokal. Settings: Level ab +8, Paketname als FName ab +0x10.
void __fastcall SeamlessTravelHook(uintptr_t Subsystem, uintptr_t Settings)
{
    const std::string Package = NameFromIndex(Read<int32_t>(Settings + kLoadSettingsPackage));
    // Das Hauptmenue beendet die Sitzung, die Reise laeuft dort lokal und mit dem Ladeablauf des Spiels.
    if (!RedirectTravel || Package.find("MainMenu") != std::string::npos)
    {
        OriginalSeamlessTravel(Subsystem, Settings);
        return;
    }
    Log("Reise umgeleitet: " + Package);
    Pending = L"servertravel " + std::wstring(Package.begin(), Package.end()) + L"?listen";
    PostMessageW(GameWindow, kRunCommand, 0, 0);
}

// Laeuft jede Sekunde auf dem Game Thread und ersetzt die Befehle !cam und !authmove nach jedem Kartenwechsel.
ProcessEventFn OriginalNativeProcessEvent;
int32_t ConstructName = -1;
int32_t PreConstructName = -1;

struct FDeferredEvent
{
    uintptr_t Object;
    uintptr_t Function;
    uint64_t Params;
};
std::vector<FDeferredEvent> DeferredWidgetEvents;

// Client: WP_UI wartet in PreConstruct in einer Schleife auf den GameState. Beim zweiten Kartenwechsel entsteht die HUD
// vor ihm, und in der Schleife kann er nie ankommen. Beide Ereignisse werden zurueckgehalten, bis er da ist.
void __fastcall ProcessEventHook(uintptr_t Object, uintptr_t Function, void* Params)
{
    const int32_t Name = Read<int32_t>(Function + 0x18);
    if (Name == ConstructName || Name == PreConstructName)
    {
        const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
        const uintptr_t Driver = World ? Read<uintptr_t>(World + kWorldNetDriver) : 0;
        const bool IsClient = Driver && Read<uintptr_t>(Driver + kDriverServerConnection);
        if (IsClient && !Read<uintptr_t>(World + kWorldGameState) && NameOf(Read<uintptr_t>(Object + 0x10)) == "WP_UI_C")
        {
            DeferredWidgetEvents.push_back({Object, Function, Params ? Read<uint64_t>(reinterpret_cast<uintptr_t>(Params)) : 0});
            Log("WP_UI: zurueckgehalten");
            return;
        }
    }
    OriginalNativeProcessEvent(Object, Function, Params);
}

void ReleaseDeferredWidgetEvents(uintptr_t World)
{
    if (DeferredWidgetEvents.empty() || !Read<uintptr_t>(World + kWorldGameState))
        return;
    const std::vector<FDeferredEvent> Events = std::move(DeferredWidgetEvents);
    DeferredWidgetEvents.clear();
    for (FDeferredEvent Event : Events)
        OriginalNativeProcessEvent(Event.Object, Event.Function, &Event.Params);
    Log("WP_UI: nachgeholt");
}

// Host: Der Seamless Travel ersetzt die Controller. In SpawnedPlayers bleibt der alte Remote-Controller stehen, nach der
// naechsten Garbage Collection als null. Der Eintrag wird direkt ersetzt, der Hash passt dann nicht mehr. Das Spiel
// durchlaeuft die Menge nur.
void RepairSpawnedPlayers(uintptr_t World)
{
    const uintptr_t GameMode = Read<uintptr_t>(World + kWorldGameMode);
    const uintptr_t GameState = Read<uintptr_t>(World + kWorldGameState);
    if (!GameMode || !GameState)
        return;
    const uintptr_t States = Read<uintptr_t>(GameState + kGameStatePlayerArray);
    const int32_t StateCount = Read<int32_t>(GameState + kGameStatePlayerArray + 8);
    const uintptr_t Entries = Read<uintptr_t>(GameMode + kGameModeSpawnedPlayers);
    const int32_t EntryCount = Read<int32_t>(GameMode + kGameModeSpawnedPlayers + 8);
    const auto Entry = [&](int32_t Index) { return reinterpret_cast<uintptr_t*>(Entries + Index * 0x10); };
    const auto IsCurrent = [&](uintptr_t Controller) {
        for (int32_t i = 0; Controller && i < StateCount; ++i)
            if (Read<uintptr_t>(Read<uintptr_t>(States + 8 * i) + kActorOwner) == Controller)
                return true;
        return false;
    };
    for (int32_t i = 0; i < StateCount; ++i)
    {
        const uintptr_t Controller = Read<uintptr_t>(Read<uintptr_t>(States + 8 * i) + kActorOwner);
        bool Found = !Controller;
        for (int32_t j = 0; j < EntryCount && !Found; ++j)
            Found = *Entry(j) == Controller;
        for (int32_t j = 0; j < EntryCount && !Found; ++j)
        {
            const bool IsFreeSlot = (*Entry(j) >> 47) != 0;
            if (IsFreeSlot || IsCurrent(*Entry(j)))
                continue;
            *Entry(j) = Controller;
            Found = true;
            Log("SpawnedPlayers: Eintrag ersetzt");
        }
    }
}

void TickSession()
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Driver = World ? Read<uintptr_t>(World + kWorldNetDriver) : 0;
    if (!Driver)
        return;
    if (Read<uintptr_t>(Driver + kDriverServerConnection))
    {
        ReleaseDeferredWidgetEvents(World);
        ViewThroughSharedCamera();
        return;
    }
    AcceptClientPositions();
    RepairSpawnedPlayers(World);
}

struct FPointerArray
{
    uintptr_t* Data;
    int32_t Num;
    int32_t Max;
};

// Die Kamera holt ihre Spieler aus GameMode->SpawnedPlayers. Der Client hat keinen GameMode, also kommen sie hier
// aus der replizierten PlayerArray.
FPointerArray* __fastcall CameraPlayersWithoutGameMode(uintptr_t, FPointerArray* Out)
{
    Out->Data = nullptr;
    Out->Num = 0;
    Out->Max = 0;
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t GameState = World ? Read<uintptr_t>(World + kWorldGameState) : 0;
    if (!GameState)
        return Out;

    using ResizeGrowFn = int32_t(__fastcall*)(uint32_t, FPointerArray*, int32_t*);
    const auto ResizeGrow = reinterpret_cast<ResizeGrowFn>(Base + kArrayResizeGrow);
    const uintptr_t States = Read<uintptr_t>(GameState + kGameStatePlayerArray);
    const int32_t Count = Read<int32_t>(GameState + kGameStatePlayerArray + 8);
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t State = Read<uintptr_t>(States + 8 * i);
        const uintptr_t Pawn = State ? Read<uintptr_t>(State + kPlayerStatePawn) : 0;
        if (!Pawn)
            continue;

        // Bewegliche Boeden lassen den Client relativ zum Boden senden. Bodenkacheln sind nicht repliziert, der Host
        // kann sie nicht aufloesen und liest die Werte als Weltposition. Static erzwingt Weltkoordinaten.
        const uintptr_t Floor = Read<uintptr_t>(Pawn + kCharacterMovementBase);
        const uintptr_t FloorOwner = Floor ? Read<uintptr_t>(Floor + 0x20) : 0;
        if (FloorOwner && Read<uint8_t>(Floor + kComponentMobility) == 2 && !(Read<uint8_t>(FloorOwner + kActorReplicatesBits) & 8))
            *reinterpret_cast<uint8_t*>(Floor + kComponentMobility) = 0;

        const int32_t Index = Out->Num == Out->Max ? ResizeGrow(0x808, Out, &Out->Max) : Out->Num;
        Out->Num = Index + 1;
        Out->Data[Index] = Pawn;
    }
    return Out;
}

// Beim Spawnen ruft das Spiel GetLocalPlayer()->...() ohne Nullpruefung auf. Ein Remote-Controller hat keinen
// LocalPlayer, der Host stuerzt also ab. Der Patch springt in dem Fall in den vorhandenen Pfad ohne Coop-Subsystem.
void PatchRemoteSpawn()
{
    const uintptr_t Site = Base + kSpawnNullSite;
    const uint8_t Expected[] = {0x48, 0x8B, 0x08, 0x48, 0x8B, 0x91, 0x30, 0x03, 0x00, 0x00};
    if (memcmp(reinterpret_cast<void*>(Site), Expected, sizeof(Expected)) != 0)
    {
        Log("Patch: unerwartete Bytes");
        return;
    }

    uint8_t* Cave = nullptr;
    for (uintptr_t Try = (Base - 0x10000000) & ~uintptr_t(0xFFFF); !Cave && Try > Base - 0x70000000; Try -= 0x10000)
        Cave = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(Try), 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!Cave)
    {
        Log("Patch: kein Speicher");
        return;
    }

    const auto Rel = [](uintptr_t Next, uintptr_t Target) { return static_cast<int32_t>(Target - Next); };
    uint8_t* Out = Cave;
    const auto Emit = [&](std::initializer_list<uint8_t> Bytes) { for (uint8_t Byte : Bytes) *Out++ = Byte; };
    const auto EmitRel = [&](uintptr_t Target) {
        const int32_t Value = Rel(reinterpret_cast<uintptr_t>(Out) + 4, Target);
        memcpy(Out, &Value, 4);
        Out += 4;
    };

    Emit({0x48, 0x85, 0xC0});                               // test rax,rax
    Emit({0x0F, 0x84});                                     // je ohne Coop-Logik
    EmitRel(Base + kSpawnSkipTarget);
    Emit({0x48, 0x8B, 0x08});                               // mov rcx,[rax]
    Emit({0x48, 0x8B, 0x91, 0x30, 0x03, 0x00, 0x00});       // mov rdx,[rcx+330h]
    Emit({0x48, 0x8B, 0xC8});                               // mov rcx,rax
    Emit({0xFF, 0xD2});                                     // call rdx
    Emit({0x8B, 0xD0});                                     // mov edx,eax
    Emit({0xE9});                                           // jmp zurueck
    EmitRel(Site + 17);

    const auto WriteJump = [&](uintptr_t From, uint8_t* To) {
        DWORD Old = 0;
        VirtualProtect(reinterpret_cast<void*>(From), 5, PAGE_EXECUTE_READWRITE, &Old);
        *reinterpret_cast<uint8_t*>(From) = 0xE9;
        const int32_t Jump = Rel(From + 5, reinterpret_cast<uintptr_t>(To));
        memcpy(reinterpret_cast<void*>(From + 1), &Jump, 4);
        VirtualProtect(reinterpret_cast<void*>(From), 5, Old, &Old);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(From), 5);
    };
    WriteJump(Site, Cave);

    // Hilfsfunktion PC -> Spielerindex: gleiche Annahme. Ohne LocalPlayer gilt der Remote-Spieler als P2 (Index 1).
    const uintptr_t IndexSite = Base + kPlayerIndexNullSite;
    if (memcmp(reinterpret_cast<void*>(IndexSite), Expected, sizeof(Expected)) != 0)
    {
        Log("Patch: unerwartete Bytes im Index-Helfer");
        return;
    }
    uint8_t* IndexCave = Out;
    Emit({0x48, 0x85, 0xC0});                               // test rax,rax
    Emit({0x75, 0x0A});                                     // jne original
    Emit({0xB8, 0x01, 0x00, 0x00, 0x00});                   // mov eax,1
    Emit({0x48, 0x83, 0xC4, 0x28});                         // add rsp,28h
    Emit({0xC3});                                           // ret
    Emit({0x48, 0x8B, 0x08});                               // mov rcx,[rax]
    Emit({0x48, 0x8B, 0x91, 0x30, 0x03, 0x00, 0x00});       // mov rdx,[rcx+330h]
    Emit({0x48, 0x8B, 0xC8});                               // mov rcx,rax
    Emit({0x48, 0x83, 0xC4, 0x28});                         // add rsp,28h
    Emit({0xFF, 0xE2});                                     // jmp rdx
    WriteJump(IndexSite, IndexCave);

    // Client: Die Funktion erwartet ein Objekt in this+370h, das beim Client nicht gesetzt ist. Dann Aufruf auslassen.
    const uintptr_t ClientSite = Base + kClientRefreshCallSite;
    if (Read<uint8_t>(ClientSite) != 0xE8)
    {
        Log("Patch: unerwartete Bytes im Client-Pfad");
        return;
    }
    uint8_t* ClientCave = Out;
    Emit({0x48, 0x83, 0xB9, 0x70, 0x03, 0x00, 0x00, 0x00}); // cmp qword ptr [rcx+370h],0
    Emit({0x74, 0x05});                                     // je zurueck
    Emit({0xE8});                                           // call original
    EmitRel(Base + kClientRefreshFunction);
    Emit({0xE9});                                           // jmp zurueck
    EmitRel(ClientSite + 5);
    WriteJump(ClientSite, ClientCave);

    // Client: Die Kamera in AllPlayerFramingCamera+318h (GameMode) ist leer, dann die Ersatzfunktion nutzen.
    const uintptr_t CameraSite = Base + kCameraPlayersFunction;
    const uint8_t CameraExpected[] = {0x48, 0x89, 0x5C, 0x24, 0x18};
    if (memcmp(reinterpret_cast<void*>(CameraSite), CameraExpected, sizeof(CameraExpected)) != 0)
    {
        Log("Patch: unerwartete Bytes in der Kamera");
        return;
    }
    uint8_t* CameraCave = Out;
    Emit({0x48, 0x83, 0xB9, 0x18, 0x03, 0x00, 0x00, 0x00}); // cmp qword ptr [rcx+318h],0
    Emit({0x75, 0x0C});                                     // jne original
    Emit({0x48, 0xB8});                                     // mov rax,Ersatzfunktion
    const uintptr_t Replacement = reinterpret_cast<uintptr_t>(&CameraPlayersWithoutGameMode);
    memcpy(Out, &Replacement, 8);
    Out += 8;
    Emit({0xFF, 0xE0});                                     // jmp rax
    Emit({0x48, 0x89, 0x5C, 0x24, 0x18});                   // mov [rsp+18h],rbx
    Emit({0xE9});                                           // jmp zurueck
    EmitRel(CameraSite + 5);
    WriteJump(CameraSite, CameraCave);

    // Client: Der Tick sucht das AbilitySystem, bevor der PlayerState angekommen ist. Dann diesen Tick auslassen.
    const uintptr_t AttributeSite = Base + kAttributeLookupCallSite;
    if (Read<uint8_t>(AttributeSite) != 0xE8)
    {
        Log("Patch: unerwartete Bytes im Attribut-Tick");
        return;
    }
    uint8_t* AttributeCave = Out;
    Emit({0x48, 0x85, 0xC9});                               // test rcx,rcx
    Emit({0x0F, 0x84});                                     // je Tick-Ende
    EmitRel(Base + kAttributeTickExit);
    Emit({0xE8});                                           // call original
    EmitRel(Base + kAttributeLookupFunction);
    Emit({0xE9});                                           // jmp zurueck
    EmitRel(AttributeSite + 5);
    WriteJump(AttributeSite, AttributeCave);

    // Host: Nach der Kartenerzeugung verteilt MapLootSpawner die Beute um die Figur des ersten Spielers. Beim Spawn
    // mit zwei Spielern gibt es die noch nicht, dann gilt der vorhandene Pfad ohne RootComponent.
    const uintptr_t LootSite = Base + kLootPawnRead;
    const uint8_t LootExpected[] = {0x48, 0x8B, 0x83, 0xB8, 0x01, 0x00, 0x00};
    if (memcmp(reinterpret_cast<void*>(LootSite), LootExpected, sizeof(LootExpected)) != 0)
    {
        Log("Patch: unerwartete Bytes im Loot-Spawner");
        return;
    }
    uint8_t* LootCave = Out;
    Emit({0x48, 0x85, 0xDB});                               // test rbx,rbx
    Emit({0x0F, 0x84});                                     // je ohne Figur
    EmitRel(Base + kLootNoRootTarget);
    Emit({0x48, 0x8B, 0x83, 0xB8, 0x01, 0x00, 0x00});       // mov rax,[rbx+1B8h]
    Emit({0xE9});                                           // jmp zurueck
    EmitRel(LootSite + 7);
    WriteJump(LootSite, LootCave);

    const uintptr_t TravelSite = Base + kSetClientTravel;
    const uint8_t TravelExpected[] = {0x48, 0x89, 0x5C, 0x24, 0x08};
    if (memcmp(reinterpret_cast<void*>(TravelSite), TravelExpected, sizeof(TravelExpected)) != 0)
    {
        Log("Patch: unerwartete Bytes in SetClientTravel");
        return;
    }
    OriginalSetClientTravel = reinterpret_cast<SetClientTravelFn>(Out);
    Emit({0x48, 0x89, 0x5C, 0x24, 0x08});                   // mov [rsp+8],rbx
    Emit({0xE9});                                           // jmp zurueck
    EmitRel(TravelSite + 5);
    uint8_t* TravelCave = Out;
    Emit({0x48, 0xB8});                                     // mov rax,SetClientTravelHook
    const uintptr_t TravelHook = reinterpret_cast<uintptr_t>(&SetClientTravelHook);
    memcpy(Out, &TravelHook, 8);
    Out += 8;
    Emit({0xFF, 0xE0});                                     // jmp rax
    WriteJump(TravelSite, TravelCave);

    const uintptr_t SeamlessSite = Base + kSeamlessTravelToLevel;
    const uint8_t SeamlessExpected[] = {0x48, 0x89, 0x5C, 0x24, 0x18};
    if (memcmp(reinterpret_cast<void*>(SeamlessSite), SeamlessExpected, sizeof(SeamlessExpected)) != 0)
    {
        Log("Patch: unerwartete Bytes in SeamlessTravelToLevel");
        return;
    }
    OriginalSeamlessTravel = reinterpret_cast<SeamlessTravelFn>(Out);
    Emit({0x48, 0x89, 0x5C, 0x24, 0x18});                   // mov [rsp+18h],rbx
    Emit({0xE9});                                           // jmp zurueck
    EmitRel(SeamlessSite + 5);
    uint8_t* SeamlessCave = Out;
    Emit({0x48, 0xB8});                                     // mov rax,SeamlessTravelHook
    const uintptr_t SeamlessHook = reinterpret_cast<uintptr_t>(&SeamlessTravelHook);
    memcpy(Out, &SeamlessHook, 8);
    Out += 8;
    Emit({0xFF, 0xE0});                                     // jmp rax
    WriteJump(SeamlessSite, SeamlessCave);

    // Host: Der Spawner waehlt zufaellig einen Eintrag aus SpawnedPlayers, dort kann null stehen. Dann faellt der Spawn aus.
    const uintptr_t SpawnerSite = Base + kSpawnerPlayerRead;
    const uint8_t SpawnerExpected[] = {0x49, 0x8B, 0x86, 0xE8, 0x02, 0x00, 0x00};
    if (memcmp(reinterpret_cast<void*>(SpawnerSite), SpawnerExpected, sizeof(SpawnerExpected)) != 0)
    {
        Log("Patch: unerwartete Bytes im Gegner-Spawner");
        return;
    }
    uint8_t* SpawnerCave = Out;
    Emit({0x4D, 0x85, 0xF6});                               // test r14,r14
    Emit({0x0F, 0x84});                                     // je Spawn-Ende
    EmitRel(Base + kSpawnerNoPlayerExit);
    Emit({0x49, 0x8B, 0x86, 0xE8, 0x02, 0x00, 0x00});       // mov rax,[r14+2E8h]
    Emit({0xE9});                                           // jmp zurueck
    EmitRel(SpawnerSite + 7);
    WriteJump(SpawnerSite, SpawnerCave);

    // Client: Das Item-Array enthaelt nach dem Seamless Travel null, das Spiel prueft das nicht.
    const uintptr_t ItemsSite = Base + kItemsLoopRead;
    const uint8_t ItemsExpected[] = {0x48, 0x8B, 0x3B, 0x83, 0xFD, 0x05};
    if (memcmp(reinterpret_cast<void*>(ItemsSite), ItemsExpected, sizeof(ItemsExpected)) != 0)
    {
        Log("Patch: unerwartete Bytes in der Item-Schleife");
        return;
    }
    uint8_t* ItemsCave = Out;
    Emit({0x48, 0x8B, 0x3B});                               // mov rdi,[rbx]
    Emit({0x48, 0x85, 0xFF});                               // test rdi,rdi
    Emit({0x0F, 0x84});                                     // je naechster Eintrag
    EmitRel(Base + kItemsLoopNext);
    Emit({0x83, 0xFD, 0x05});                               // cmp ebp,5
    Emit({0xE9});                                           // jmp zurueck
    EmitRel(ItemsSite + 6);
    WriteJump(ItemsSite, ItemsCave);

    // Client: Dasselbe Raster wird auch fuer seine Ausdehnung durchlaufen.
    const uintptr_t GridSite = Base + kGridLoopRead;
    const uint8_t GridExpected[] = {0x48, 0x8B, 0x0E, 0x44, 0x8B, 0x41, 0x28};
    if (memcmp(reinterpret_cast<void*>(GridSite), GridExpected, sizeof(GridExpected)) != 0)
    {
        Log("Patch: unerwartete Bytes im Raster");
        return;
    }
    uint8_t* GridCave = Out;
    Emit({0x48, 0x8B, 0x0E});                               // mov rcx,[rsi]
    Emit({0x48, 0x85, 0xC9});                               // test rcx,rcx
    Emit({0x0F, 0x84});                                     // je naechster Eintrag
    EmitRel(Base + kGridLoopNext);
    Emit({0x44, 0x8B, 0x41, 0x28});                         // mov r8d,[rcx+28h]
    Emit({0xE9});                                           // jmp zurueck
    EmitRel(GridSite + 7);
    WriteJump(GridSite, GridCave);

    // Client: Eine Figurenkomponente holt im Tick den Controller. Die fremde Figur hat beim Client keinen.
    const uintptr_t ControllerSite = Base + kComponentControllerRead;
    const uint8_t ControllerExpected[] = {0x48, 0x8B, 0x07, 0x48, 0x8D, 0x54, 0x24, 0x60};
    if (memcmp(reinterpret_cast<void*>(ControllerSite), ControllerExpected, sizeof(ControllerExpected)) != 0)
    {
        Log("Patch: unerwartete Bytes in der Figurenkomponente");
        return;
    }
    uint8_t* ControllerCave = Out;
    Emit({0x48, 0x85, 0xFF});                               // test rdi,rdi
    Emit({0x0F, 0x84});                                     // je Funktionsende
    EmitRel(Base + kComponentControllerExit);
    Emit({0x48, 0x8B, 0x07, 0x48, 0x8D, 0x54, 0x24, 0x60}); // mov rax,[rdi]; lea rdx,[rsp+60h]
    Emit({0xE9});                                           // jmp zurueck
    EmitRel(ControllerSite + 8);
    WriteJump(ControllerSite, ControllerCave);

    // Client: Dritte Schleife ueber dieselben Eintraege.
    const uintptr_t SlotSite = Base + kSlotLoopRead;
    const uint8_t SlotExpected[] = {0x48, 0x8B, 0x07, 0x48, 0x8B, 0x58, 0x30};
    if (memcmp(reinterpret_cast<void*>(SlotSite), SlotExpected, sizeof(SlotExpected)) != 0)
    {
        Log("Patch: unerwartete Bytes in der Slot-Schleife");
        return;
    }
    uint8_t* SlotCave = Out;
    Emit({0x48, 0x8B, 0x07});                               // mov rax,[rdi]
    Emit({0x48, 0x85, 0xC0});                               // test rax,rax
    Emit({0x0F, 0x84});                                     // je naechster Eintrag
    EmitRel(Base + kSlotLoopNext);
    Emit({0x48, 0x8B, 0x58, 0x30});                         // mov rbx,[rax+30h]
    Emit({0xE9});                                           // jmp zurueck
    EmitRel(SlotSite + 7);
    WriteJump(SlotSite, SlotCave);

    // Client: FreezeThreatLevelManager entsteht nur beim Host, beim Aufraeumen der Welt wird er trotzdem benutzt.
    // Das vorhandene je springt dann direkt ans Funktionsende.
    const auto ThreatBranch = reinterpret_cast<uint8_t*>(Base + kThreatFreezeBranch);
    if (ThreatBranch[0] != 0x74 || ThreatBranch[1] != 0x15)
    {
        Log("Patch: unerwartete Bytes in ThreatLevel");
        return;
    }
    DWORD ThreatOld = 0;
    VirtualProtect(ThreatBranch, 2, PAGE_EXECUTE_READWRITE, &ThreatOld);
    ThreatBranch[1] = 0x6A;
    VirtualProtect(ThreatBranch, 2, ThreatOld, &ThreatOld);
    FlushInstructionCache(GetCurrentProcess(), ThreatBranch, 2);
    ConstructName = Read<int32_t>(FindObject("Construct", "UserWidget") + 0x18);
    PreConstructName = Read<int32_t>(FindObject("PreConstruct", "UserWidget") + 0x18);
    const uintptr_t EventSite = Base + kProcessEventNative;
    const uint8_t EventExpected[] = {0x40, 0x55, 0x56, 0x57, 0x41, 0x54};
    if (memcmp(reinterpret_cast<void*>(EventSite), EventExpected, sizeof(EventExpected)) != 0)
    {
        Log("Patch: unerwartete Bytes in ProcessEvent");
        return;
    }
    OriginalNativeProcessEvent = reinterpret_cast<ProcessEventFn>(Out);
    Emit({0x40, 0x55, 0x56, 0x57, 0x41, 0x54});             // push rbp,rsi,rdi,r12
    Emit({0xE9});                                           // jmp zurueck
    EmitRel(EventSite + 6);
    uint8_t* EventCave = Out;
    Emit({0x48, 0xB8});                                     // mov rax,ProcessEventHook
    const uintptr_t EventHook = reinterpret_cast<uintptr_t>(&ProcessEventHook);
    memcpy(Out, &EventHook, 8);
    Out += 8;
    Emit({0xFF, 0xE0});                                     // jmp rax
    WriteJump(EventSite, EventCave);
    Log("Patch: aktiv");
}

LRESULT CALLBACK WndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    if (Message == kRunCommand)
    {
        if (Pending == L"!arm")
        {
            RedirectTravel = true;
        }
        else if (Pending == L"!cam")
        {
            ViewThroughSharedCamera();
        }
        else if (Pending == L"!trace")
        {
            TraceCharacterRpcs();
        }
        else if (Pending == L"!authmove")
        {
            AcceptClientPositions();
        }
        else if (Pending == L"!wave")
        {
            ForceEnemyWave();
        }
        else if (Pending == L"!flow")
        {
            TraceRunFlow();
        }
        else if (Pending == L"!finish")
        {
            FinishMission();
        }
        else if (Pending.rfind(L"!round ", 0) == 0)
        {
            EndRound(static_cast<uint8_t>(_wtoi(Pending.c_str() + 7)));
        }
        else
        {
            RunConsoleCommand(Pending);
        }
        return 0;
    }
    if (Message == WM_TIMER && WParam == kSessionTimer)
    {
        TickSession();
        return 0;
    }
    return CallWindowProcW(OriginalWndProc, Window, Message, WParam, LParam);
}

BOOL CALLBACK FindGameWindow(HWND Window, LPARAM Result)
{
    DWORD Process = 0;
    GetWindowThreadProcessId(Window, &Process);
    wchar_t ClassName[64]{};
    GetClassNameW(Window, ClassName, 64);
    if (Process == GetCurrentProcessId() && wcscmp(ClassName, L"UnrealWindow") == 0)
    {
        *reinterpret_cast<HWND*>(Result) = Window;
        return FALSE;
    }
    return TRUE;
}

DWORD WINAPI Worker(void*)
{
    wchar_t Temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, Temp);
    const std::wstring Directory = std::wstring(Temp) + L"ddspike\\";
    CreateDirectoryW(Directory.c_str(), nullptr);
    const std::wstring Id = std::to_wstring(GetCurrentProcessId());
    const std::wstring CommandPath = Directory + Id + L".cmd";
    LogPath = Directory + Id + L".log";

    Base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));

    HWND Window = nullptr;
    while (!Window)
    {
        EnumWindows(FindGameWindow, reinterpret_cast<LPARAM>(&Window));
        Sleep(200);
    }
    OriginalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(Window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    GameWindow = Window;
    SetTimer(Window, kSessionTimer, 1000, nullptr);
    Log("bereit");
    PatchRemoteSpawn();

    for (;;)
    {
        Sleep(200);
        HANDLE File = CreateFileW(CommandPath.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (File == INVALID_HANDLE_VALUE)
            continue;
        char Buffer[512]{};
        DWORD Size = 0;
        ReadFile(File, Buffer, sizeof(Buffer) - 1, &Size, nullptr);
        CloseHandle(File);
        DeleteFileW(CommandPath.c_str());

        Log(std::string("Befehl: ") + Buffer);
        wchar_t Wide[512]{};
        MultiByteToWideChar(CP_UTF8, 0, Buffer, -1, Wide, 512);
        Pending = Wide;
        while (!Pending.empty() && (Pending.back() == L'\n' || Pending.back() == L'\r'))
            Pending.pop_back();
        PostMessageW(Window, kRunCommand, 0, 0);
    }
}
}

BOOL WINAPI DllMain(HINSTANCE Module, DWORD Reason, void*)
{
    if (Reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(Module);
        CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    }
    return TRUE;
}
