// Exercises the actual Mission.cpp adapter with owned byte arrays, never a game process or window.
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
uintptr_t FinishToken = 11;
uintptr_t RoundToken = 12;
std::vector<std::string> Lines;
void Log(const std::string& Line) { Lines.push_back(Line); }
std::string NameOf(uintptr_t) { return {}; }
uintptr_t FindObject(const char* Name, const char*)
{
    if (strcmp(Name, "FinishMission") == 0)
        return FinishToken;
    if (strcmp(Name, "TriggerEndRound") == 0)
        return RoundToken;
    return 0;
}
}

template <size_t Size, typename T>
void Store(std::array<uint8_t, Size>& Buffer, size_t Offset, T Value)
{
    assert(Offset + sizeof(T) <= Size);
    memcpy(Buffer.data() + Offset, &Value, sizeof(T));
}

int main()
{
    using namespace Spike;
    std::array<uint8_t, 0x300> World{};
    std::array<uint8_t, 0x110> Driver{};
    std::array<uint8_t, 0x300> Instance{};
    std::array<uint8_t, 0x400> State{};
    WorldSlot = reinterpret_cast<uintptr_t>(World.data());
    Store(World, kWorldNetDriver, reinterpret_cast<uintptr_t>(Driver.data()));
    Store(World, kWorldGameState, reinterpret_cast<uintptr_t>(State.data()));
    Store(World, 0x228, reinterpret_cast<uintptr_t>(Instance.data()));
    Store(State, 0x390, int32_t(3));
    wchar_t Temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, Temp);
    const auto Session = std::to_wstring(GetTickCount64()) + std::to_wstring(GetCurrentProcessId());
    const auto Path = std::wstring(Temp) + L"ddspike\\round-" + Session + L"-1-1.bin";
    const auto HostBind = L"!phase bind host " + Session + L" 1 1";
    RoundPhase::Wire Bytes{};
    assert(!RoundPhaseCommand(L"!finish"));
    assert(RoundPhaseCommand(L"!phase bind host -1 1 1"));
    assert(Lines.back().find("abgelehnt") != std::string::npos);
    assert(RoundPhaseCommand(HostBind));
    TickRoundPhase(nullptr);
    assert(!RoundPhase::ReadLocal(Path, Bytes)); // Bind cannot invent initial Playing.
    assert(RoundPhaseCommand(L"!phase stage playing"));
    TickRoundPhase(nullptr);
    assert(RoundPhase::ReadLocal(Path, Bytes));
    RoundPhase::Snapshot Value;
    assert(RoundPhase::Decode(Bytes.data(), Bytes.size(), Value));
    assert(Value.Stage == RoundPhase::Phase::Playing && Value.Day == 3);
    Store(Instance, 0x299, uint8_t(1));
    ObserveRoundPhaseEvent(0, FinishToken, nullptr); // Wrong object is not a host event.
    TickRoundPhase(nullptr);
    assert(RoundPhase::ReadLocal(Path, Bytes));
    assert(RoundPhase::Decode(Bytes.data(), Bytes.size(), Value));
    assert(Value.Stage == RoundPhase::Phase::Playing);
    ObserveRoundPhaseEvent(reinterpret_cast<uintptr_t>(State.data()), FinishToken, nullptr);
    TickRoundPhase(nullptr);
    assert(RoundPhase::ReadLocal(Path, Bytes));
    assert(RoundPhase::Decode(Bytes.data(), Bytes.size(), Value));
    assert(Value.Stage == RoundPhase::Phase::RoundEnd && Value.EndReason == RoundPhase::Reason::NewDay);
    assert(RoundPhaseCommand(L"!phase stage stash"));
    assert(RoundPhaseCommand(L"!phase stage route"));
    TickRoundPhase(nullptr);
    assert(RoundPhase::ReadLocal(Path, Bytes));
    assert(RoundPhase::Decode(Bytes.data(), Bytes.size(), Value));
    assert(Value.Stage == RoundPhase::Phase::Route);
    assert(RoundPhaseCommand(L"!phase stage playing")); // Monotone rejection.
    TickRoundPhase(nullptr);
    assert(RoundPhase::ReadLocal(Path, Bytes));
    assert(RoundPhase::Decode(Bytes.data(), Bytes.size(), Value));
    assert(Value.Stage == RoundPhase::Phase::Route);
    WorldSlot = 0;
    TickRoundPhase(nullptr);
    assert(Lines.back().find("Neubindung") != std::string::npos);
    WorldSlot = reinterpret_cast<uintptr_t>(World.data());
    Store(Driver, kDriverServerConnection, uintptr_t(99));
    assert(RoundPhaseCommand(L"!phase bind client " + Session + L" 1 1"));
    assert(RoundPhaseBlocksMessage(nullptr, WM_KEYDOWN, 'W', 0));
    assert(RoundPhaseBlocksMessage(nullptr, WM_LBUTTONDOWN, 0, 0));
    assert(!RoundPhaseBlocksMessage(nullptr, WM_TIMER, 0, 0));
    assert(!RoundPhaseBlocksMessage(nullptr, WM_CLOSE, 0, 0));
    assert(RoundPhaseCommand(L"!phase stage route"));
    assert(Lines.back().find("Syntax/Grenze") != std::string::npos);
    assert(RoundPhaseCommand(L"!phase stop"));
    assert(!RoundPhaseBlocksMessage(nullptr, WM_KEYDOWN, 'W', 0));
    assert(DeleteFileW(Path.c_str()));
    std::cout << "Actual Mission adapter source/binding/message-policy tests passed (no game/window)\n";
}
