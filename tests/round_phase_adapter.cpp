// Exercises the actual Mission.cpp adapter with owned byte arrays, never a game process or window.
// Writes %TEMP%\ddspike\round-host.txt, so do not run it during a game session.
#include "../research/spikes/a-connect/Spike.h"
#include "../research/spikes/a-connect/RoundPhaseAdapter.h"
#include "../research/spikes/a-connect/RoundPhaseLocal.h"
#include <array>
#include <cassert>
#include <iostream>

namespace Spike
{
uintptr_t WorldSlot = 0;
uintptr_t Base = reinterpret_cast<uintptr_t>(&WorldSlot) - kGWorld;
std::vector<std::string> Lines;
std::map<uintptr_t, std::string> Names;
void Log(const std::string& Line) { Lines.push_back(Line); }
std::string NameOf(uintptr_t Object)
{
    const auto Found = Names.find(Object);
    return Found == Names.end() ? std::string() : Found->second;
}
uintptr_t FindObject(const char* Name, const char*)
{
    const char* Known[] = {"FinishMission", "TriggerEndRound", "OnOpenStateChanged", "DisableInput", "EnableInput"};
    for (uintptr_t i = 0; i < 5; ++i)
        if (strcmp(Name, Known[i]) == 0)
            return 0x1000 + i;
    return 0;
}
}

namespace
{
constexpr uintptr_t kFinish = 0x1000, kRound = 0x1001, kOpenState = 0x1002, kDisable = 0x1003, kEnable = 0x1004;

struct Call
{
    uintptr_t Object, Function, Param;
};
std::vector<Call> Calls;
void __fastcall RecordProcessEvent(uintptr_t Object, uintptr_t Function, void* Params)
{
    Calls.push_back({Object, Function, *static_cast<uintptr_t*>(Params)});
}
std::array<Spike::ProcessEventFn, Spike::kProcessEventIndex + 1> VTable{};

template <size_t Size, typename T>
void Store(std::array<uint8_t, Size>& Buffer, size_t Offset, T Value)
{
    assert(Offset + sizeof(T) <= Size);
    memcpy(Buffer.data() + Offset, &Value, sizeof(T));
}
template <size_t Size>
uintptr_t Address(std::array<uint8_t, Size>& Buffer) { return reinterpret_cast<uintptr_t>(Buffer.data()); }

bool LastLogHas(const char* Text) { return Spike::Lines.back().find(Text) != std::string::npos; }

std::wstring RoundFile(uint64_t World)
{
    wchar_t Temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, Temp);
    return std::wstring(Temp) + L"ddspike\\round-" + std::to_wstring(GetCurrentProcessId()) + L"-1-" +
        std::to_wstring(World) + L".bin";
}

Spike::RoundPhase::Snapshot ReadRound(uint64_t World)
{
    Spike::RoundPhase::Wire Bytes{};
    Spike::RoundPhase::Snapshot Value;
    assert(Spike::RoundPhase::ReadLocal(RoundFile(World), Bytes));
    assert(Spike::RoundPhase::Decode(Bytes.data(), Bytes.size(), Value));
    return Value;
}
}

int main()
{
    using namespace Spike;
    using RoundPhase::Phase;
    for (auto& Entry : VTable)
        Entry = RecordProcessEvent;
    std::array<uint8_t, 0x300> World{}, World2{};
    std::array<uint8_t, 0x110> Driver{}, Driver2{};
    std::array<uint8_t, 0x300> Instance{};
    std::array<uint8_t, 0x400> State{};
    std::array<uint8_t, 0x50> StateClass{}, StatsClass{}, StashClass{}, RouteClass{};
    std::array<uint8_t, 0x20> Stats{}, Stash{}, Route{};
    std::array<uintptr_t, 1> LocalPlayers{};
    std::array<uint8_t, 0x40> Player{};
    std::array<uint8_t, 0x300> Controller{};
    std::array<uint8_t, 0x20> Pawn{};

    Names[Address(StateClass)] = "DDMissionGameState";
    Names[Address(StatsClass)] = "WP_Round_Stats_C";
    Names[Address(StashClass)] = "WP_Inventory_Panel_C";
    Names[Address(RouteClass)] = "WI_MissionChoicePanel_C";
    Store(State, 0x10, Address(StateClass));
    Store(State, 0x390, int32_t(3));
    Store(Stats, 0x10, Address(StatsClass));
    Store(Stash, 0x10, Address(StashClass));
    Store(Route, 0x10, Address(RouteClass));
    for (auto* Target : {&World, &World2})
    {
        Store(*Target, kWorldGameState, Address(State));
        Store(*Target, 0x228, Address(Instance));
    }
    Store(World, kWorldNetDriver, Address(Driver));
    Store(World2, kWorldNetDriver, Address(Driver2));
    LocalPlayers[0] = Address(Player);
    Store(Instance, 0x38, reinterpret_cast<uintptr_t>(LocalPlayers.data()));
    Store(Instance, 0x40, int32_t(1));
    Store(Player, 0x30, Address(Controller));
    Store(Controller, 0, reinterpret_cast<uintptr_t>(VTable.data()));
    Store(Controller, 0x2E8, Address(Pawn));
    Store(Pawn, 0, reinterpret_cast<uintptr_t>(VTable.data()));
    const auto State0 = Address(State);
    bool Open = true, Closed = false;

    assert(!RoundPhaseCommand(L"!finish"));

    // Host: binds itself in a new world and publishes the mission phase with the day.
    WorldSlot = Address(World);
    TickRoundPhase(nullptr);
    auto Value = ReadRound(1);
    assert(Value.Stage == Phase::Playing && Value.Day == 3);

    // Only the current GameState counts for FinishMission; the stats panel then only confirms.
    Store(Instance, 0x299, uint8_t(1));
    ObserveRoundPhaseEvent(0, kFinish, nullptr);
    assert(ReadRound(1).Stage == Phase::Playing);
    ObserveRoundPhaseEvent(State0, kFinish, nullptr);
    Value = ReadRound(1);
    assert(Value.Stage == Phase::RoundEnd && Value.EndReason == RoundPhase::Reason::NewDay);
    ObserveRoundPhaseEvent(Address(Stats), kOpenState, &Open);
    assert(LastLogHas("bestaetigt"));
    ObserveRoundPhaseEvent(Address(Stash), kOpenState, &Open);
    assert(ReadRound(1).Stage == Phase::Stash);
    ObserveRoundPhaseEvent(Address(Route), kOpenState, &Open);
    assert(ReadRound(1).Stage == Phase::Route);
    ObserveRoundPhaseEvent(Address(Route), kOpenState, &Closed);
    assert(ReadRound(1).Stage == Phase::Travel);

    // New world (MetaBase): the inventory outside a round end is no stash; "Zurueck" on the route returns.
    WorldSlot = Address(World2);
    TickRoundPhase(nullptr);
    assert(ReadRound(2).Stage == Phase::Playing);
    ObserveRoundPhaseEvent(Address(Stash), kOpenState, &Open);
    assert(ReadRound(2).Stage == Phase::Playing);
    ObserveRoundPhaseEvent(Address(Route), kOpenState, &Open);
    assert(ReadRound(2).Stage == Phase::Route);
    ObserveRoundPhaseEvent(Address(Route), kOpenState, &Closed);
    assert(ReadRound(2).Stage == Phase::Playing);
    ObserveRoundPhaseEvent(Address(Route), kOpenState, &Open);
    assert(ReadRound(2).Stage == Phase::Route);
    // The round reason from TriggerEndRound's own parameter.
    uint8_t Lose = 3;
    ObserveRoundPhaseEvent(Address(Instance), kRound, &Lose);
    Value = ReadRound(2);
    assert(Value.Stage == Phase::RoundEnd && Value.EndReason == RoundPhase::Reason::Lose);
    ObserveRoundPhaseEvent(Address(Route), kOpenState, &Open);
    assert(ReadRound(2).Stage == Phase::Route);

    // Client in the same world: follows the announced host world and locks while the host is on the route.
    Store(Driver2, kDriverServerConnection, uintptr_t(99));
    Calls.clear();
    TickRoundPhase(nullptr);
    assert(LastLogHas("Routenkarte, Eingabe gesperrt"));
    assert(Calls.size() == 2 && Calls[0].Function == kDisable && Calls[0].Object == Address(Controller) &&
        Calls[1].Object == Address(Pawn) && Calls[1].Param == Address(Controller));
    assert(RoundPhaseBlocksMessage(nullptr, WM_KEYDOWN, 'W', 0));
    assert(RoundPhaseBlocksMessage(nullptr, WM_LBUTTONDOWN, 0, 0));
    assert(!RoundPhaseBlocksMessage(nullptr, WM_KEYUP, 'W', 0)); // Releases pass, no stuck keys.
    assert(!RoundPhaseBlocksMessage(nullptr, WM_LBUTTONUP, 0, 0));
    assert(!RoundPhaseBlocksMessage(nullptr, WM_TIMER, 0, 0));
    assert(!RoundPhaseBlocksMessage(nullptr, WM_CLOSE, 0, 0));
    assert(RoundPhaseCommand(L"!phase filter off"));
    assert(!RoundPhaseBlocksMessage(nullptr, WM_KEYDOWN, 'W', 0));
    assert(RoundPhaseCommand(L"!phase filter on"));
    assert(RoundPhaseCommand(L"!phase status") && LastLogHas("Client, Welt 2, Routenkarte, Sperre an"));

    // Host goes back to the mission: newer sequence unlocks and restores input.
    Value = ReadRound(2);
    Value.Stage = Phase::Playing;
    Value.EndReason = RoundPhase::Reason::NoReason;
    ++Value.Sequence;
    RoundPhase::Wire Bytes{};
    assert(RoundPhase::Encode(Value, Bytes) && RoundPhase::WriteLocal(RoundFile(2), Bytes));
    Calls.clear();
    TickRoundPhase(nullptr);
    assert(LastLogHas("Mission, Eingabe frei"));
    assert(Calls.size() == 2 && Calls[0].Function == kEnable && Calls[1].Function == kEnable);
    assert(!RoundPhaseBlocksMessage(nullptr, WM_KEYDOWN, 'W', 0));

    // Locked again, then the host announcement goes stale: lock and banner are removed.
    Value.Stage = Phase::Stash;
    ++Value.Sequence;
    assert(RoundPhase::Encode(Value, Bytes) && RoundPhase::WriteLocal(RoundFile(2), Bytes));
    TickRoundPhase(nullptr);
    assert(RoundPhaseBlocksMessage(nullptr, WM_KEYDOWN, 'W', 0));
    wchar_t Temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, Temp);
    const auto Announce = std::wstring(Temp) + L"ddspike\\round-host.txt";
    HANDLE File = CreateFileW(Announce.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    assert(File != INVALID_HANDLE_VALUE);
    FILETIME Old{0, 0x01D00000};
    assert(SetFileTime(File, nullptr, nullptr, &Old));
    CloseHandle(File);
    Calls.clear();
    TickRoundPhase(nullptr);
    assert(LastLogHas("keine frische Hostankuendigung"));
    assert(!Calls.empty() && Calls[0].Function == kEnable);
    assert(!RoundPhaseBlocksMessage(nullptr, WM_KEYDOWN, 'W', 0));

    // !phase stop switches the adapter off until !phase auto.
    assert(RoundPhaseCommand(L"!phase stop"));
    Store(Driver2, kDriverServerConnection, uintptr_t(0));
    TickRoundPhase(nullptr);
    assert(LastLogHas("Phase: aus"));
    assert(RoundPhaseCommand(L"!phase auto"));
    TickRoundPhase(nullptr);
    assert(LastLogHas("Host gebunden, Welt 3"));

    for (uint64_t Generation : {1, 2, 3})
        DeleteFileW(RoundFile(Generation).c_str());
    DeleteFileW(Announce.c_str());
    std::cout << "Mission adapter tests passed: host sources, client follow, input lock, filter, host loss (no game/window)\n";
}
