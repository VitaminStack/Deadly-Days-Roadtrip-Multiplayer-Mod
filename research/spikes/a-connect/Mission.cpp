// Host: Befehle, die Gegner und Rundenende von aussen ausloesen.
#include "Spike.h"
#include "RoundPhaseAdapter.h"
#include "RoundPhaseRuntime.h"
#include "RoundPhaseLocal.h"
#include <sstream>
#include <atomic>

namespace Spike
{
namespace
{
constexpr uintptr_t kWorldGameInstance = 0x228;
// Existing roundstate.py / documented build-26187885 observations, no new offsets.
constexpr uintptr_t kLastRoundReason = 0x299;
constexpr uintptr_t kCurrentDay = 0x390;

RoundPhase::Publisher PhaseHost;
RoundPhase::Subscriber PhaseClient;
uintptr_t PhaseWorld = 0;
uintptr_t PhaseDriver = 0;
uintptr_t PhaseConnection = 0;
uintptr_t FinishFunction = 0;
uintptr_t RoundFunction = 0;
bool PhaseIsClient = false;
bool PhaseHostReady = false;
std::wstring PhasePath;
HWND PhaseBanner = nullptr;
std::atomic<DWORD> PhaseThread{0};

void StopRoundPhase()
{
    PhaseThread.store(0);
    PhaseHost.Disconnect();
    PhaseClient.Disconnect();
    PhaseWorld = PhaseDriver = PhaseConnection = 0;
    PhaseHostReady = false;
    PhasePath.clear();
    if (PhaseBanner)
        DestroyWindow(PhaseBanner);
    PhaseBanner = nullptr;
}

bool ReadId(std::wistringstream& Input, uint64_t& Value)
{
    std::wstring Token;
    if (!(Input >> Token) || Token.empty())
        return false;
    Value = 0;
    for (wchar_t C : Token)
    {
        if (C < L'0' || C > L'9' || Value > (UINT64_MAX - (C - L'0')) / 10)
            return false;
        Value = Value * 10 + (C - L'0');
    }
    return Value != 0;
}

bool IsClientWorld(uintptr_t World)
{
    const uintptr_t Driver = World ? Read<uintptr_t>(World + kWorldNetDriver) : 0;
    return Driver && Read<uintptr_t>(Driver + kDriverServerConnection);
}
}

// Explicit operator binding is intentionally NOT an automatic handshake. Tokens must be fresh per test.
bool RoundPhaseCommand(const std::wstring& Command)
{
    if (Command != L"!phase" && Command.rfind(L"!phase ", 0) != 0)
        return false;
    std::wistringstream Input(Command);
    std::wstring Prefix, Action, Role, Extra;
    Input >> Prefix >> Action;
    if (Action == L"stop" && !(Input >> Extra))
    {
        StopRoundPhase();
        Log("Phase: gestoppt; eigene Anzeige/Nachrichtensperre entfernt");
        return true;
    }
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Driver = World ? Read<uintptr_t>(World + kWorldNetDriver) : 0;
    if (Action == L"bind")
    {
        uint64_t Session = 0, Run = 0, Generation = 0;
        Input >> Role;
        const uintptr_t GameState = World ? Read<uintptr_t>(World + kWorldGameState) : 0;
        if (!Driver || !GameState || (Role != L"host" && Role != L"client") ||
            (Role == L"client") != IsClientWorld(World) ||
            !ReadId(Input, Session) || !ReadId(Input, Run) || !ReadId(Input, Generation) || (Input >> Extra))
        {
            Log("Phase: bind abgelehnt (Rolle/Welt/positive dezimale IDs pruefen)");
            return true;
        }
        const bool Client = Role == L"client";
        if (PhaseWorld && Client != PhaseIsClient)
        {
            Log("Phase: Rollenwechsel erfordert !phase stop");
            return true;
        }
        const bool Bound = Client ? PhaseClient.Bind(Session, Run, Generation, GetTickCount64()) :
            PhaseHost.Bind(Session, Run, Generation, Read<int32_t>(GameState + kCurrentDay));
        if (!Bound)
        {
            Log("Phase: alte/fremde Bindung abgelehnt; neue Sitzung nur nach !phase stop");
            return true;
        }
        wchar_t Temp[MAX_PATH]{};
        GetTempPathW(MAX_PATH, Temp);
        const std::wstring Directory = std::wstring(Temp) + L"ddspike\\";
        CreateDirectoryW(Directory.c_str(), nullptr);
        PhasePath = Directory + L"round-" + std::to_wstring(Session) + L"-" +
            std::to_wstring(Run) + L"-" + std::to_wstring(Generation) + L".bin";
        PhaseWorld = World;
        PhaseDriver = Driver;
        PhaseConnection = Read<uintptr_t>(Driver + kDriverServerConnection);
        PhaseIsClient = Client;
        PhaseHostReady = false; // Never infer Playing just because a world exists.
        FinishFunction = FindObject("FinishMission", "DDMissionGameState");
        RoundFunction = FindObject("TriggerEndRound", "DDMissionGameInstance");
        PhaseThread.store(GetCurrentThreadId());
        Log("Phase: lokale Diagnosebindung aktiv; Hostquelle/Stage noch zu bestaetigen");
        return true;
    }
    if (Action == L"stage" && PhaseWorld && World == PhaseWorld && Driver == PhaseDriver &&
        Driver && !IsClientWorld(World) && !PhaseIsClient)
    {
        std::wstring Stage;
        Input >> Stage;
        const bool HasExtra = static_cast<bool>(Input >> Extra);
        const wchar_t* Names[] = {L"playing", L"roundend", L"stash", L"route", L"travel"};
        for (uint8_t i = 0; i < 5 && !HasExtra; ++i)
        {
            if (Stage != Names[i])
                continue;
            const uintptr_t Instance = Read<uintptr_t>(World + kWorldGameInstance);
            const auto Reason = Instance ? static_cast<RoundPhase::Reason>(Read<uint8_t>(Instance + kLastRoundReason)) :
                RoundPhase::Reason::NoReason;
            if (PhaseHost.Observe(static_cast<RoundPhase::Phase>(i), Reason))
            {
                PhaseHostReady = true;
                Log("Phase: MANUELLE Diagnosephase, kein nachgewiesenes Panelereignis");
                return true;
            }
        }
    }
    Log("Phase: Syntax/Grenze: !phase bind host|client <session> <run> <world>; !phase stage playing|roundend|stash|route|travel (Host); !phase stop");
    return true;
}

// Call AFTER the existing ProcessEvent trampoline. Never call host persistence on the client.
void ObserveRoundPhaseEvent(uintptr_t Object, uintptr_t Function, void* Params)
{
    // Global ProcessEvent also has non-game-thread callers. They must not touch adapter state.
    if (PhaseThread.load() != GetCurrentThreadId())
        return;
    if (!PhaseWorld || PhaseIsClient || Read<uintptr_t>(Base + kGWorld) != PhaseWorld || IsClientWorld(PhaseWorld))
        return;
    const uintptr_t Instance = Read<uintptr_t>(PhaseWorld + kWorldGameInstance);
    const uintptr_t State = Read<uintptr_t>(PhaseWorld + kWorldGameState);
    RoundPhase::Reason Reason = RoundPhase::Reason::NoReason;
    if (RoundFunction && Function == RoundFunction && Object == Instance && Params)
        Reason = static_cast<RoundPhase::Reason>(*static_cast<uint8_t*>(Params));
    else if (FinishFunction && Function == FinishFunction && Object == State && Instance)
        Reason = static_cast<RoundPhase::Reason>(Read<uint8_t>(Instance + kLastRoundReason));
    else
        return;
    if (PhaseHost.Observe(RoundPhase::Phase::RoundEnd, Reason))
    {
        PhaseHostReady = true;
        Log("Phase: reflektierter Host-Rundenendeaufruf beobachtet (nicht Panelsichtbarkeit)");
    }
    else
        Log("Phase: Hostereignis ohne gueltigen Grund/monotone Phase; keine Nachricht erzeugt");
}

void TickRoundPhase(HWND GameWindow)
{
    if (!PhaseWorld)
        return;
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Driver = World ? Read<uintptr_t>(World + kWorldNetDriver) : 0;
    if (World != PhaseWorld || Driver != PhaseDriver || !Driver ||
        Read<uintptr_t>(Driver + kDriverServerConnection) != PhaseConnection)
    {
        StopRoundPhase();
        Log("Phase: Welt/Verbindung gewechselt; alte Bindung verworfen, explizite Neubindung erforderlich");
        return;
    }
    RoundPhase::Wire Bytes{};
    if (!PhaseIsClient)
    {
        if (PhaseHostReady && (!PhaseHost.Heartbeat(Bytes) || !RoundPhase::WriteLocal(PhasePath, Bytes)))
            Log("Phase: lokales Publizieren fehlgeschlagen");
        return;
    }
    const uint64_t Now = GetTickCount64();
    if (RoundPhase::ReadLocal(PhasePath, Bytes))
        PhaseClient.Receive(Bytes.data(), Bytes.size(), Now);
    PhaseClient.Tick(Now);
    const auto View = PhaseClient.View();
    if (!View.InputBlocked)
    {
        if (PhaseBanner)
            ShowWindow(PhaseBanner, SW_HIDE);
        return;
    }
    if (!PhaseBanner)
        PhaseBanner = CreateWindowExW(WS_EX_NOACTIVATE, L"STATIC", L"", WS_CHILD | SS_CENTER,
            20, 20, 580, 50, GameWindow, nullptr, GetModuleHandleW(nullptr), nullptr);
    const wchar_t* Labels[] = {L"Mission", L"Rundenende", L"Lager", L"Routenkarte", L"Reise"};
    const auto Text = View.Waiting ? std::wstring(L"Coop-Spike: Warte auf frischen Hostzustand") :
        std::wstring(L"Coop-Spike: Host - ") + Labels[static_cast<uint8_t>(View.Host.Stage)] +
        L" / Tag " + std::to_wstring(View.Host.Day) + L" (nur Anzeige)";
    if (PhaseBanner)
    {
        SetWindowTextW(PhaseBanner, Text.c_str());
        ShowWindow(PhaseBanner, SW_SHOWNOACTIVATE);
    }
}

bool RoundPhaseBlocksMessage(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    if (!PhaseWorld || !PhaseIsClient || !PhaseClient.View().InputBlocked)
        return false;
    if (Message == WM_INPUT)
    {
        // Foreground raw-input handles require DefWindowProc cleanup even when not forwarded to UE.
        DefWindowProcW(Window, Message, WParam, LParam);
        return true;
    }
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
    ObserveRoundPhaseEvent(GameState, Function, &IsLoading);
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
    ObserveRoundPhaseEvent(GameInstance, Function, &Params);
    Log("Rundenende: ausgeloest");
}
}
