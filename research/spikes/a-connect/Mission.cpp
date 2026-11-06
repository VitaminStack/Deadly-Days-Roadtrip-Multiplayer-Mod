// Host: Befehle, die Gegner und Rundenende von aussen ausloesen.
// Client: zeigt die Rundenphase des Hosts an und sperrt dort die eigene Eingabe.
#include "Spike.h"
#include "RoundPhaseAdapter.h"
#include "RoundPhaseRuntime.h"
#include "RoundPhaseLocal.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <unordered_set>

namespace Spike
{
namespace
{
constexpr uintptr_t kWorldGameInstance = 0x228;
constexpr uintptr_t kLastRoundReason = 0x299;     // UDDMissionGameInstance::LastRoundChangeReason
constexpr uintptr_t kCurrentDay = 0x390;          // ADDMissionGameState::CurrentDay
constexpr uintptr_t kInstanceLocalPlayers = 0x38; // UGameInstance::LocalPlayers
constexpr uintptr_t kPlayerController = 0x30;     // UPlayer::PlayerController
constexpr uintptr_t kControllerPawn = 0x2E8;      // AController::Pawn
constexpr uint64_t kAnnounceFreshMs = 5000;

// Host panels that mark the round phases, seen via AbstractPanel.OnOpenStateChanged(bOpen).
constexpr const char* kRoundStatsPanel = "WP_Round_Stats_C";
constexpr const char* kStashPanel = "WP_Inventory_Panel_C";
constexpr const char* kRoutePanel = "WI_MissionChoicePanel_C";

RoundPhase::Publisher PhaseHost;
RoundPhase::Subscriber PhaseClient;
uintptr_t PhaseWorld = 0;
uintptr_t PhaseDriver = 0;
uintptr_t PhaseConnection = 0;
bool PhaseIsClient = false;
bool PhaseAuto = true;
bool PhaseFilter = true;
bool RoundEnded = false;
uint64_t HostGeneration = 0;
uint64_t BoundSession = 0, BoundRun = 0, BoundWorld = 0;
std::wstring PhasePath;
HWND PhaseBanner = nullptr;
uintptr_t BlockedController = 0;
RoundPhase::Phase LastLoggedStage = RoundPhase::Phase::Playing;
bool LastLoggedBlocked = false;
bool ClientLogPending = false;
std::string StageSource;
std::atomic<DWORD> PhaseThread{0};
uintptr_t FinishFunction = 0, RoundFunction = 0, OpenStateFunction = 0, DisableFunction = 0, EnableFunction = 0;
// Diagnosis only: logs each new (class, function) pair seen by ProcessEvent on the game thread.
std::atomic<DWORD> ScanThread{0};
std::unordered_set<uint64_t> ScanSeen;

const wchar_t* StageLabel(RoundPhase::Phase Stage)
{
    const wchar_t* Labels[] = {L"Mission", L"Rundenuebersicht", L"Lager", L"Routenkarte", L"Reise"};
    return Labels[static_cast<uint8_t>(Stage)];
}

std::string Narrow(const wchar_t* Text)
{
    std::string Out;
    for (; *Text; ++Text)
        Out += static_cast<char>(*Text);
    return Out;
}

std::wstring PhaseDirectory()
{
    wchar_t Temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, Temp);
    const std::wstring Directory = std::wstring(Temp) + L"ddspike\\";
    CreateDirectoryW(Directory.c_str(), nullptr);
    return Directory;
}

std::wstring AnnouncePath() { return PhaseDirectory() + L"round-host.txt"; }

std::wstring RoundPath(uint64_t Session, uint64_t Run, uint64_t World)
{
    return PhaseDirectory() + L"round-" + std::to_wstring(Session) + L"-" + std::to_wstring(Run) + L"-" +
        std::to_wstring(World) + L".bin";
}

void ResolveFunctions()
{
    if (!FinishFunction)
        FinishFunction = FindObject("FinishMission", "DDMissionGameState");
    if (!RoundFunction)
        RoundFunction = FindObject("TriggerEndRound", "DDMissionGameInstance");
    if (!OpenStateFunction)
        OpenStateFunction = FindObject("OnOpenStateChanged", "AbstractPanel");
    if (!DisableFunction)
        DisableFunction = FindObject("DisableInput", "Actor");
    if (!EnableFunction)
        EnableFunction = FindObject("EnableInput", "Actor");
}

bool IsClientWorld(uintptr_t World)
{
    const uintptr_t Driver = World ? Read<uintptr_t>(World + kWorldNetDriver) : 0;
    return Driver && Read<uintptr_t>(Driver + kDriverServerConnection);
}

// MainMenu has another GameState without CurrentDay; only read it from the mission state.
bool IsMissionState(uintptr_t GameState)
{
    for (uintptr_t Class = GameState ? Read<uintptr_t>(GameState + 0x10) : 0; Class; Class = Read<uintptr_t>(Class + 0x40))
        if (NameOf(Class) == "DDMissionGameState")
            return true;
    return false;
}

uintptr_t LocalController(uintptr_t World)
{
    const uintptr_t Instance = World ? Read<uintptr_t>(World + kWorldGameInstance) : 0;
    const uintptr_t Players = Instance ? Read<uintptr_t>(Instance + kInstanceLocalPlayers) : 0;
    const int32_t Count = Instance ? Read<int32_t>(Instance + kInstanceLocalPlayers + 8) : 0;
    const uintptr_t Player = Players && Count > 0 ? Read<uintptr_t>(Players) : 0;
    return Player ? Read<uintptr_t>(Player + kPlayerController) : 0;
}

// AActor::DisableInput(PC) on the controller and its pawn takes both out of the UE input stack, so no
// device reaches gameplay through it. EnableInput restores it. Both are idempotent.
void SetGameInput(uintptr_t World, bool Enabled)
{
    const uintptr_t Function = Enabled ? EnableFunction : DisableFunction;
    const uintptr_t Controller = LocalController(World);
    if (!Function || !Controller)
        return;
    for (uintptr_t Actor : {Controller, Read<uintptr_t>(Controller + kControllerPawn)})
    {
        if (!Actor)
            continue;
        uintptr_t Params = Controller;
        Read<ProcessEventFn*>(Actor)[kProcessEventIndex](Actor, Function, &Params);
    }
    BlockedController = Enabled ? 0 : Controller;
}

void HideBanner()
{
    if (PhaseBanner)
        ShowWindow(PhaseBanner, SW_HIDE);
}

// Owned popup above the game window: GDI child windows are overdrawn by the swap chain.
void ShowBanner(HWND GameWindow, const std::wstring& Text)
{
    if (!GameWindow)
        return;
    if (!PhaseBanner)
        PhaseBanner = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"STATIC", L"",
            WS_POPUP | WS_BORDER | SS_CENTER | SS_CENTERIMAGE, 0, 0, 0, 0, GameWindow, nullptr,
            GetModuleHandleW(nullptr), nullptr);
    if (!PhaseBanner)
        return;
    RECT Client{};
    GetClientRect(GameWindow, &Client);
    POINT Origin{0, 0};
    ClientToScreen(GameWindow, &Origin);
    const int Width = (std::min)(560, static_cast<int>(Client.right) - 20);
    SetWindowTextW(PhaseBanner, Text.c_str());
    SetWindowPos(PhaseBanner, HWND_TOP, Origin.x + (Client.right - Width) / 2, Origin.y + 10, Width, 34,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

// Removes only this adapter's own banner and input lock.
void ReleaseRoundPhase(uintptr_t World)
{
    if (BlockedController && LocalController(World) == BlockedController)
        SetGameInput(World, true);
    BlockedController = 0;
    PhaseThread.store(0);
    PhaseHost.Disconnect();
    PhaseClient.Disconnect();
    PhaseWorld = PhaseDriver = PhaseConnection = 0;
    BoundSession = BoundRun = BoundWorld = 0;
    RoundEnded = false;
    PhasePath.clear();
    LastLoggedStage = RoundPhase::Phase::Playing;
    LastLoggedBlocked = false;
    StageSource.clear();
    HideBanner();
}

void PublishHost()
{
    RoundPhase::Wire Bytes{};
    if (!PhaseHost.Heartbeat(Bytes) || !RoundPhase::WriteLocal(PhasePath, Bytes))
    {
        Log("Phase: Publizieren fehlgeschlagen");
        return;
    }
    const auto& State = PhaseHost.View();
    char Line[96]{};
    sprintf_s(Line, "%llu %llu %llu\n", static_cast<unsigned long long>(State.Session),
        static_cast<unsigned long long>(State.Run), static_cast<unsigned long long>(State.World));
    const auto Path = AnnouncePath();
    const auto Temporary = Path + L".tmp";
    FILE* File = nullptr;
    if (_wfopen_s(&File, Temporary.c_str(), L"wb") == 0 && File)
    {
        fputs(Line, File);
        fclose(File);
        MoveFileExW(Temporary.c_str(), Path.c_str(), MOVEFILE_REPLACE_EXISTING);
    }
}

void ObserveHost(RoundPhase::Phase Stage, RoundPhase::Reason EndReason, const char* Source)
{
    if (Stage == PhaseHost.View().Stage)
    {
        // A second source for the same phase, e.g. the round stats panel after FinishMission.
        if (StageSource != Source)
            Log("Phase: Host " + Narrow(StageLabel(Stage)) + " bestaetigt (" + Source + ")");
        StageSource = Source;
        return;
    }
    if (!PhaseHost.Observe(Stage, EndReason))
    {
        Log(std::string("Phase: Hostwechsel abgelehnt (") + Source + ")");
        return;
    }
    if (Stage == RoundPhase::Phase::RoundEnd)
        RoundEnded = true;
    StageSource = Source;
    Log("Phase: Host " + Narrow(StageLabel(Stage)) + " (" + Source + ")");
    PublishHost();
}

void BindHost(uintptr_t World, uintptr_t Driver, uintptr_t GameState)
{
    ResolveFunctions();
    const uint64_t Session = GetCurrentProcessId();
    const int32_t Day = IsMissionState(GameState) ? Read<int32_t>(GameState + kCurrentDay) : 0;
    PhaseHost.Disconnect();
    if (!PhaseHost.Bind(Session, 1, ++HostGeneration, Day < 0 ? 0 : Day))
        return;
    PhaseWorld = World;
    PhaseDriver = Driver;
    PhaseConnection = 0;
    PhaseIsClient = false;
    RoundEnded = false;
    PhasePath = RoundPath(Session, 1, HostGeneration);
    PhaseThread.store(GetCurrentThreadId());
    Log("Phase: Host gebunden, Welt " + std::to_string(HostGeneration) + ", Tag " + std::to_string(Day));
    PhaseHost.Observe(RoundPhase::Phase::Playing, RoundPhase::Reason::NoReason);
    StageSource = "Weltstart";
    PublishHost();
}

bool ReadAnnounce(uint64_t& Session, uint64_t& Run, uint64_t& World)
{
    WIN32_FILE_ATTRIBUTE_DATA Info{};
    const auto Path = AnnouncePath();
    if (!GetFileAttributesExW(Path.c_str(), GetFileExInfoStandard, &Info))
        return false;
    FILETIME Now{};
    GetSystemTimeAsFileTime(&Now);
    const auto Ticks = [](const FILETIME& Time) { return (uint64_t(Time.dwHighDateTime) << 32) | Time.dwLowDateTime; };
    if (Ticks(Now) < Ticks(Info.ftLastWriteTime) || (Ticks(Now) - Ticks(Info.ftLastWriteTime)) / 10000 > kAnnounceFreshMs)
        return false;
    FILE* File = nullptr;
    if (_wfopen_s(&File, Path.c_str(), L"rb") != 0 || !File)
        return false;
    unsigned long long S = 0, R = 0, W = 0;
    const bool Good = fscanf_s(File, "%llu %llu %llu", &S, &R, &W) == 3 && S && R && W;
    fclose(File);
    Session = S;
    Run = R;
    World = W;
    return Good;
}

// Mit der Mod oeffnet der Client Rundenuebersicht, Lager und Routenkarte selbst und ordnet sein Inventar dort. Die Mod
// laesst ihn nicht selbst reisen, der Host waehlt den Tag (rundenende-client). Gesperrt bleibt nur Warten und Reise.
RoundPhase::Presentation ClientView()
{
    auto View = PhaseClient.View();
    if (ModOwnsPatches && !View.Waiting && View.Host.Stage != RoundPhase::Phase::Playing)
        View.InputBlocked = View.Host.Stage == RoundPhase::Phase::Travel;
    return View;
}

void TickClient(HWND GameWindow, uintptr_t World, uintptr_t Driver, uintptr_t Connection)
{
    const uint64_t Now = GetTickCount64();
    uint64_t Session = 0, Run = 0, Generation = 0;
    if (!ReadAnnounce(Session, Run, Generation))
    {
        if (BoundSession)
        {
            ReleaseRoundPhase(World);
            Log("Phase: keine frische Hostankuendigung, Anzeige und Sperre entfernt");
        }
        return;
    }
    if (Session != BoundSession || Run != BoundRun || Generation != BoundWorld)
    {
        if (Session != BoundSession || Run != BoundRun)
            PhaseClient.Disconnect();
        if (!PhaseClient.Bind(Session, Run, Generation, Now))
            return;
        BoundSession = Session;
        BoundRun = Run;
        BoundWorld = Generation;
        PhaseWorld = World;
        PhaseDriver = Driver;
        PhaseConnection = Connection;
        PhaseIsClient = true;
        PhasePath = RoundPath(Session, Run, Generation);
        ResolveFunctions();
        PhaseThread.store(GetCurrentThreadId());
        ClientLogPending = true;
        Log("Phase: Client folgt Hostwelt " + std::to_string(Generation));
    }
    RoundPhase::Wire Bytes{};
    if (RoundPhase::ReadLocal(PhasePath, Bytes))
        PhaseClient.Receive(Bytes.data(), Bytes.size(), Now);
    PhaseClient.Tick(Now);
    const auto View = ClientView();
    const bool ModRoundEnd = ModOwnsPatches && !View.Waiting && View.Host.Stage != RoundPhase::Phase::Playing;
    if (ClientLogPending || View.InputBlocked != LastLoggedBlocked || (!View.Waiting && View.Host.Stage != LastLoggedStage))
    {
        ClientLogPending = false;
        Log(std::string("Phase: Client ") + (View.Waiting ? "wartet auf Host" : Narrow(StageLabel(View.Host.Stage))) +
            (View.InputBlocked ? ", Eingabe gesperrt" : ", Eingabe frei"));
        LastLoggedBlocked = View.InputBlocked;
        LastLoggedStage = View.Host.Stage;
    }
    if (!View.InputBlocked)
    {
        if (BlockedController)
            SetGameInput(World, true);
        HideBanner();
        return;
    }
    SetGameInput(World, false);
    const std::wstring Text = View.Waiting ? std::wstring(L"Coop: warte auf den Host") :
        ModRoundEnd ? std::wstring(L"Coop: Host reist weiter (Tag ") + std::to_wstring(View.Host.Day) + L")" :
        std::wstring(L"Coop: Host ist in ") + StageLabel(View.Host.Stage) + L" (Tag " +
        std::to_wstring(View.Host.Day) + L"), Eingabe gesperrt";
    ShowBanner(GameWindow, Text);
}
}

// Diagnosis only. The adapter runs automatically; these switches exist for tests.
bool RoundPhaseCommand(const std::wstring& Command)
{
    if (Command != L"!phase" && Command.rfind(L"!phase ", 0) != 0)
        return false;
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const std::wstring Action = Command.size() > 7 ? Command.substr(7) : L"";
    if (Action == L"scan")
    {
        const bool On = ScanThread.load() == 0;
        ScanSeen.clear();
        ScanThread.store(On ? GetCurrentThreadId() : 0);
        Log(On ? "Phase: Scan an" : "Phase: Scan aus");
    }
    else if (Action == L"stop")
    {
        PhaseAuto = false;
        ReleaseRoundPhase(World);
        Log("Phase: aus, eigene Anzeige und Sperre entfernt");
    }
    else if (Action == L"auto")
    {
        PhaseAuto = true;
        Log("Phase: automatisch");
    }
    else if (Action == L"filter on" || Action == L"filter off")
    {
        PhaseFilter = Action == L"filter on";
        Log(PhaseFilter ? "Phase: Fensternachrichten werden gefiltert" : "Phase: nur DisableInput sperrt");
    }
    else if (Action == L"status")
    {
        const auto& Host = PhaseIsClient ? PhaseClient.View().Host : PhaseHost.View();
        Log(std::string("Phase: ") + (PhaseWorld ? (PhaseIsClient ? "Client" : "Host") : "ungebunden") +
            ", Welt " + std::to_string(Host.World) + ", " + Narrow(StageLabel(Host.Stage)) +
            ", Sperre " + (PhaseIsClient && ClientView().InputBlocked ? "an" : "aus"));
    }
    else
        Log("Phase: !phase scan|stop|auto|filter on|filter off|status");
    return true;
}

// Called after every ProcessEvent (Widgets.cpp). Only the host's own game thread changes state.
void ObserveRoundPhaseEvent(uintptr_t Object, uintptr_t Function, void* Params)
{
    if (ScanThread.load() == GetCurrentThreadId() && Object)
    {
        const uintptr_t Class = Read<uintptr_t>(Object + 0x10);
        const uint64_t Key = (uint64_t(Read<uint32_t>(Class + 0x18)) << 32) | Read<uint32_t>(Function + 0x18);
        if (ScanSeen.insert(Key).second)
            Log("Scan " + std::to_string(GetTickCount64()) + " " + NameOf(Class) + " :: " +
                NameOf(Read<uintptr_t>(Function + 0x20)) + "." + NameOf(Function));
    }
    if (PhaseThread.load() != GetCurrentThreadId() || !PhaseWorld || PhaseIsClient)
        return;
    if (Function != RoundFunction && Function != FinishFunction && Function != OpenStateFunction)
        return;
    if (Read<uintptr_t>(Base + kGWorld) != PhaseWorld)
        return;
    const uintptr_t Instance = Read<uintptr_t>(PhaseWorld + kWorldGameInstance);
    const uintptr_t State = Read<uintptr_t>(PhaseWorld + kWorldGameState);
    auto Reason = Instance ? static_cast<RoundPhase::Reason>(Read<uint8_t>(Instance + kLastRoundReason)) :
        RoundPhase::Reason::NoReason;
    if (Reason == RoundPhase::Reason::NoReason || static_cast<uint8_t>(Reason) > 3)
        Reason = RoundPhase::Reason::NewDay;
    if (Function == RoundFunction && Object == Instance && Params)
    {
        const auto Given = *static_cast<uint8_t*>(Params);
        ObserveHost(RoundPhase::Phase::RoundEnd, Given >= 1 && Given <= 3 ? static_cast<RoundPhase::Reason>(Given) : Reason,
            "TriggerEndRound");
    }
    else if (Function == FinishFunction && Object == State)
        ObserveHost(RoundPhase::Phase::RoundEnd, Reason, "FinishMission");
    else if (Function == OpenStateFunction && Object && Params)
    {
        const bool Open = *static_cast<uint8_t*>(Params) != 0;
        const std::string Panel = NameOf(Read<uintptr_t>(Object + 0x10));
        if (Panel == kRoundStatsPanel && Open)
            ObserveHost(RoundPhase::Phase::RoundEnd, Reason, kRoundStatsPanel);
        else if (Panel == kStashPanel && Open && RoundEnded)
            ObserveHost(RoundPhase::Phase::Stash, RoundPhase::Reason::NoReason, kStashPanel);
        else if (Panel == kRoutePanel && Open)
            ObserveHost(RoundPhase::Phase::Route, RoundPhase::Reason::NoReason, kRoutePanel);
        // Closing the route after a round end starts the next day; before it, it is "Zurueck" in the MetaBase.
        else if (Panel == kRoutePanel && PhaseHost.View().Stage == RoundPhase::Phase::Route)
            ObserveHost(RoundEnded ? RoundPhase::Phase::Travel : RoundPhase::Phase::Playing, RoundPhase::Reason::NoReason,
                kRoutePanel);
    }
}

void TickRoundPhase(HWND GameWindow)
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Driver = World ? Read<uintptr_t>(World + kWorldNetDriver) : 0;
    const uintptr_t Connection = Driver ? Read<uintptr_t>(Driver + kDriverServerConnection) : 0;
    if (PhaseWorld && (World != PhaseWorld || Driver != PhaseDriver || Connection != PhaseConnection))
    {
        ReleaseRoundPhase(World);
        Log("Phase: Welt oder Verbindung gewechselt, neu binden");
    }
    if (!PhaseAuto || !World || !Driver || !Read<uintptr_t>(World + kWorldGameState))
        return;
    if (Connection)
    {
        TickClient(GameWindow, World, Driver, Connection);
        return;
    }
    if (!PhaseWorld)
        BindHost(World, Driver, Read<uintptr_t>(World + kWorldGameState));
    else
        PublishHost();
}

bool RoundPhaseBlocksMessage(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    if (!PhaseFilter || !PhaseWorld || !PhaseIsClient || !ClientView().InputBlocked)
        return false;
    if (Message == WM_INPUT)
    {
        // Foreground raw-input handles require DefWindowProc cleanup even when not forwarded to UE.
        DefWindowProcW(Window, Message, WParam, LParam);
        return true;
    }
    // Releases always pass, so a key held before the lock never stays pressed afterwards.
    if (Message == WM_KEYUP || Message == WM_SYSKEYUP || Message == WM_LBUTTONUP || Message == WM_RBUTTONUP ||
        Message == WM_MBUTTONUP || Message == WM_XBUTTONUP)
        return false;
    return (Message >= WM_KEYFIRST && Message <= WM_KEYLAST) ||
        (Message >= WM_MOUSEFIRST && Message <= WM_MOUSELAST) || Message == WM_INPUT_DEVICE_CHANGE;
}

// Die direkt geladene Mission startet keine Gegner, fuer Trefferversuche werden sie hier angestossen.
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

// Beendet die Mission wie der abfahrende Bus, um den Rueckweg zu pruefen.
void FinishMission()
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    // FinishMission ist kein Client-RPC. Ein erneuter lokaler Aufruf waere kein UI-Sync,
    // sondern wuerde den unvollstaendigen Run-/Persistenzpfad des Clients betreten.
    if (IsClientWorld(World))
    {
        Log("Missionsende: Client-Aufruf abgelehnt; Host-Phase muss separat synchronisiert werden");
        return;
    }
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

// Beendet den Run ueber die GameInstance, Grund wie ERoundChangeReason.
void EndRound(uint8_t Reason)
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    if (IsClientWorld(World))
    {
        Log("Rundenende: Client-Aufruf abgelehnt; keine lokale Run-Persistenz ausfuehren");
        return;
    }
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
}
