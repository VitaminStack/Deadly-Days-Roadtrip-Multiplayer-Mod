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
constexpr uintptr_t kGameStatePlayerArray = 0x2C0;
constexpr uintptr_t kPlayerStatePawn = 0x320;
constexpr uintptr_t kCharacterMovementBase = 0x350;
constexpr uintptr_t kComponentMobility = 0x1AB;
constexpr uintptr_t kActorReplicatesBits = 0x5B;
constexpr uintptr_t kActorRemoteRole = 0x60;
constexpr uintptr_t kCharacterMovement = 0x330;
constexpr uintptr_t kMovementFlags = 0x579;
constexpr uintptr_t kGameInstanceWorldContext = 0x30;
constexpr uintptr_t kWorldContextTravelUrl = 0xA8;
constexpr UINT kRunCommand = WM_APP + 77;
constexpr UINT_PTR kTravelTimer = 7701;

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

std::string NameOf(uintptr_t Object)
{
    const int32_t Index = Read<int32_t>(Object + 0x18);
    const uintptr_t Block = Read<uintptr_t>(Base + kNameBlocks + 8 * (Index >> 16));
    const uintptr_t Entry = Block + (Index & 0xFFFF) * 2;
    const uint16_t Header = Read<uint16_t>(Entry);
    if (Header & 1)
        return {};
    return std::string(reinterpret_cast<const char*>(Entry + 2), Header >> 6);
}

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

void CallGameInstance(const char* FunctionName)
{
    const uintptr_t Instance = FindInstance("GI_DDMissionGameInstance_C");
    const uintptr_t Function = FindObject(FunctionName, "DDMissionGameInstance");
    if (!Instance || !Function)
    {
        Log(std::string("nicht gefunden: ") + FunctionName);
        return;
    }
    using ProcessEventFn = void(__fastcall*)(uintptr_t, uintptr_t, void*);
    char Params[16]{};
    Read<ProcessEventFn*>(Instance)[kProcessEventIndex](Instance, Function, Params);
    Log(std::string("aufgerufen: ") + FunctionName);
}

// Beim Client landet die gemeinsame Kamera nicht als ViewTarget, er schaut sonst vom Weltursprung aus.
void ViewThroughSharedCamera()
{
    const uintptr_t Camera = FindInstance("BP_PlayerCameraActor_C");
    const uintptr_t Controller = FindInstance("BP_DDPlayerController_C");
    const uintptr_t Function = FindObject("SetViewTargetWithBlend", "PlayerController");
    if (!Camera || !Controller || !Function)
    {
        Log("Kamera: nicht gefunden");
        return;
    }
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
        *reinterpret_cast<uint8_t*>(Read<uintptr_t>(Object + kCharacterMovement) + kMovementFlags) |= 0x0A;
        ++Changed;
    }
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

// Der Spielablauf reist ohne ?listen. Sobald TravelURL gesetzt ist, wird daraus ein servertravel mit ?listen.
void RedirectPendingTravel()
{
    const uintptr_t Instance = FindInstance("GI_DDMissionGameInstance_C");
    const uintptr_t Context = Instance ? Read<uintptr_t>(Instance + kGameInstanceWorldContext) : 0;
    if (!Context)
        return;
    auto& TravelUrl = *reinterpret_cast<FString*>(Context + kWorldContextTravelUrl);
    if (TravelUrl.Num <= 1 || !TravelUrl.Data)
        return;

    const std::wstring Url(TravelUrl.Data);
    const_cast<wchar_t*>(TravelUrl.Data)[0] = 0;
    TravelUrl.Num = 0;
    RedirectTravel = false;
    Log("TravelURL umgeleitet: " + std::string(Url.begin(), Url.end()));
    RunConsoleCommand(L"servertravel " + Url + L"?listen");
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
    Log("Patch: aktiv");
}

LRESULT CALLBACK WndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    if (Message == kRunCommand)
    {
        if (Pending == L"!startrun")
        {
            RedirectTravel = true;
            CallGameInstance("TriggerStartRun");
            RedirectPendingTravel();
        }
        else if (Pending == L"!arm")
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
        else
        {
            RunConsoleCommand(Pending);
        }
        return 0;
    }
    if (Message == WM_TIMER && WParam == kTravelTimer)
    {
        if (RedirectTravel)
            RedirectPendingTravel();
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
    SetTimer(Window, kTravelTimer, 1, nullptr);
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
