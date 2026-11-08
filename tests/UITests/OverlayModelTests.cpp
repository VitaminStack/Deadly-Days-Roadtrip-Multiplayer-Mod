#include <doctest.h>

#include <limits>
#include <stdexcept>
#include <thread>

#include "UI/OverlayModel.h"

namespace
{
UI::OverlayStatus Offline()
{
    UI::OverlayStatus Status;
    Status.State = UI::SessionState::Offline;
    Status.CanCreate = Status.CanJoin = true;
    return Status;
}
}

TEST_CASE("Overlay starts unavailable and cannot invent a Steam connection")
{
    Core::Dispatcher Dispatcher;
    UI::OverlayModel Model(Dispatcher);
    CHECK(Model.Snapshot().Status.State == UI::SessionState::Unavailable);
    CHECK_FALSE(Model.Snapshot().HasBackend);
    for (auto Action : { UI::LobbyAction::Create, UI::LobbyAction::Join, UI::LobbyAction::Invite, UI::LobbyAction::Leave })
        CHECK_FALSE(Model.Submit({ Action, 42 }));
    Model.Publish(Offline());
    CHECK_FALSE(Model.Submit({ UI::LobbyAction::Create }));
    CHECK(Dispatcher.Drain() == 0);
}

TEST_CASE("Actions wait for dispatcher, are bounded and run on the draining thread")
{
    Core::Dispatcher Dispatcher;
    UI::OverlayModel Model(Dispatcher);
    int Calls = 0;
    std::thread::id CalledOn;
    Model.Bind([&](UI::LobbyRequest Request)
    {
        CHECK(Request.Action == UI::LobbyAction::Join);
        CHECK(Request.LobbyId == 42);
        CalledOn = std::this_thread::get_id();
        ++Calls;
        auto Status = Offline();
        Status.State = UI::SessionState::Connecting;
        Model.Publish(Status); // Reentrant Publish must not deadlock.
    });
    Model.Publish(Offline());
    bool Accepted = false;
    std::thread Producer([&] { Accepted = Model.Submit({ UI::LobbyAction::Join, 42 }); });
    Producer.join();
    CHECK(Accepted);
    CHECK(Calls == 0);
    CHECK(Model.Snapshot().Pending);
    for (int i = 0; i < 100; ++i)
        CHECK_FALSE(Model.Submit({ UI::LobbyAction::Create }));
    CHECK(Dispatcher.Drain() == 1);
    CHECK(Calls == 1);
    CHECK(CalledOn == std::this_thread::get_id());
    CHECK_FALSE(Model.Snapshot().Pending);
    CHECK(Model.Snapshot().Status.State == UI::SessionState::Connecting);
}

TEST_CASE("State and backend replacement cancel stale queued actions")
{
    Core::Dispatcher Dispatcher;
    UI::OverlayModel Model(Dispatcher);
    int Calls = 0;
    Model.Bind([&](UI::LobbyRequest) { ++Calls; });
    Model.Publish(Offline());
    REQUIRE(Model.Submit({ UI::LobbyAction::Create }));
    auto Status = Offline();
    Status.State = UI::SessionState::Lobby;
    Status.LobbyId = 50;
    Model.Publish(Status);
    CHECK(Model.Snapshot().Pending);
    CHECK(Dispatcher.Drain() == 1);
    CHECK(Calls == 0);
    CHECK_FALSE(Model.Snapshot().Pending);
    Model.Publish(Offline());
    REQUIRE(Model.Submit({ UI::LobbyAction::Create }));
    Model.Bind({});
    CHECK(Dispatcher.Drain() == 1);
    CHECK(Calls == 0);
    CHECK_FALSE(Model.Snapshot().HasBackend);
}

TEST_CASE("Statistics updates do not starve queued actions but revoked capabilities stop them")
{
    Core::Dispatcher Dispatcher;
    UI::OverlayModel Model(Dispatcher);
    int Calls = 0;
    Model.Bind([&](UI::LobbyRequest) { ++Calls; });
    auto Status = Offline();
    Model.Publish(Status);
    REQUIRE(Model.Submit({ UI::LobbyAction::Create }));
    for (unsigned i = 0; i < 10; ++i)
    {
        Status.PingMs = i;
        Model.Publish(Status);
        CHECK_FALSE(Model.Submit({ UI::LobbyAction::Create }));
    }
    Dispatcher.Drain();
    CHECK(Calls == 1);
    REQUIRE(Model.Submit({ UI::LobbyAction::Create }));
    Status.CanCreate = false;
    Model.Publish(Status);
    Dispatcher.Drain();
    CHECK(Calls == 1);
}

TEST_CASE("Old binding cannot clear pending work for a new binding")
{
    Core::Dispatcher Dispatcher;
    UI::OverlayModel Model(Dispatcher);
    int OldCalls = 0, NewCalls = 0;
    Model.Publish(Offline());
    Model.Bind([&](UI::LobbyRequest) { ++OldCalls; });
    REQUIRE(Model.Submit({ UI::LobbyAction::Create }));
    Model.Bind([&](UI::LobbyRequest) { ++NewCalls; });
    REQUIRE(Model.Submit({ UI::LobbyAction::Create }));
    CHECK(Dispatcher.Drain() == 2);
    CHECK(OldCalls == 0);
    CHECK(NewCalls == 1);
}

TEST_CASE("Availability follows real session state and role")
{
    UI::OverlaySnapshot View;
    View.HasBackend = true;
    View.Status = Offline();
    CHECK(UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Create }));
    CHECK_FALSE(UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Join, 0 }));
    CHECK(UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Join, 42 }));
    View.Status.CanInvite = View.Status.CanLeave = true;
    CHECK_FALSE(UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Invite }));
    CHECK_FALSE(UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Leave }));
    View.Status.State = UI::SessionState::Lobby;
    View.Status.LobbyId = 42;
    View.Status.Role = UI::SessionRole::Client;
    CHECK_FALSE(UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Create }));
    CHECK_FALSE(UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Invite }));
    CHECK(UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Leave }));
    View.Status.Role = UI::SessionRole::Host;
    CHECK(UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Invite }));
    View.Status.State = UI::SessionState::Playing;
    CHECK_FALSE(UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Invite }));
    CHECK(UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Leave }));
    View.Status.State = UI::SessionState::Connecting;
    CHECK_FALSE(UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Join, 42 }));
    CHECK(UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Leave }));
    View.Status.State = UI::SessionState::Unavailable;
    CHECK_FALSE(UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Create }));
}

TEST_CASE("Lobby IDs accept only nonzero decimal uint64 without overflow")
{
    uint64_t Value = 0;
    CHECK(UI::OverlayModel::ParseLobbyId(L"18446744073709551615", Value));
    CHECK(Value == std::numeric_limits<uint64_t>::max());
    CHECK(UI::OverlayModel::ParseLobbyId(L"42", Value));
    CHECK(Value == 42);
    for (const auto* Text : { L"", L"0", L"-1", L"+1", L" 42", L"42 ", L"4.2", L"0x42", L"abc",
        L"18446744073709551616", L"100000000000000000000" })
    {
        CHECK_FALSE(UI::OverlayModel::ParseLobbyId(Text, Value));
        CHECK(Value == 0);
    }
}

TEST_CASE("Backend exceptions do not leave the overlay permanently pending")
{
    Core::Dispatcher Dispatcher;
    UI::OverlayModel Model(Dispatcher);
    Model.Publish(Offline());
    Model.Bind([](UI::LobbyRequest) { throw std::runtime_error("test"); });
    REQUIRE(Model.Submit({ UI::LobbyAction::Create }));
    CHECK_NOTHROW(Dispatcher.Drain());
    CHECK_FALSE(Model.Snapshot().Pending);
}

TEST_CASE("Queued work keeps model state alive without touching a destroyed facade")
{
    Core::Dispatcher Dispatcher;
    int Calls = 0;
    {
        UI::OverlayModel Model(Dispatcher);
        Model.Publish(Offline());
        Model.Bind([&](UI::LobbyRequest) { ++Calls; });
        REQUIRE(Model.Submit({ UI::LobbyAction::Create }));
        Model.Bind({});
    }
    CHECK(Dispatcher.Drain() == 1);
    CHECK(Calls == 0);
}

TEST_CASE("Changing the lobby identity cancels an invite even if role and state stay unchanged")
{
    Core::Dispatcher Dispatcher;
    UI::OverlayModel Model(Dispatcher);
    auto Status = Offline();
    Status.State = UI::SessionState::Lobby;
    Status.Role = UI::SessionRole::Host;
    Status.LobbyId = 41;
    Status.CanInvite = true;
    int Calls = 0;
    Model.Bind([&](UI::LobbyRequest) { ++Calls; });
    Model.Publish(Status);
    REQUIRE(Model.Submit({ UI::LobbyAction::Invite }));
    Status.LobbyId = 42;
    Model.Publish(Status);
    Dispatcher.Drain();
    CHECK(Calls == 0);
    CHECK_FALSE(Model.Snapshot().Pending);
}

TEST_CASE("An executing handler cannot enqueue duplicate work before publishing its next state")
{
    Core::Dispatcher Dispatcher;
    UI::OverlayModel Model(Dispatcher);
    Model.Publish(Offline());
    bool Accepted = true;
    Model.Bind([&](UI::LobbyRequest)
    {
        CHECK(Model.Snapshot().Pending);
        std::thread Producer([&] { Accepted = Model.Submit({ UI::LobbyAction::Create }); });
        Producer.join();
        Model.Publish(Offline()); // Even an unchanged state refresh keeps the in-flight guard.
        CHECK_FALSE(Model.Submit({ UI::LobbyAction::Create }));
    });
    REQUIRE(Model.Submit({ UI::LobbyAction::Create }));
    CHECK(Dispatcher.Drain() == 1);
    CHECK_FALSE(Accepted);
    CHECK_FALSE(Model.Snapshot().Pending);
    CHECK(Dispatcher.Drain() == 0);
}

TEST_CASE("Concurrent publishers and readers only exchange copied snapshots")
{
    Core::Dispatcher Dispatcher;
    UI::OverlayModel Model(Dispatcher);
    std::thread Writer([&]
    {
        for (unsigned i = 0; i < 1000; ++i)
        {
            auto Status = Offline();
            Status.Players = i;
            Status.PingMs = i;
            Model.Publish(Status);
        }
    });
    for (int i = 0; i < 1000; ++i)
    {
        const auto View = Model.Snapshot();
        CHECK(View.Status.Players == View.Status.PingMs);
    }
    Writer.join();
    CHECK(Model.Snapshot().Status.Players == 999);
}

TEST_CASE("Diagnostics remain independent of lobby refreshes and pending actions")
{
    Core::Dispatcher Dispatcher;
    UI::OverlayModel Model(Dispatcher);
    int Calls = 0;
    Model.Bind([&](UI::LobbyRequest) { ++Calls; });
    Model.Publish(Offline());
    REQUIRE(Model.Submit({UI::LobbyAction::Create}));
    Model.PublishDiagnostics("Desync: ABWEICHUNG Inventar");
    auto Status = Offline();
    Status.Message = "Lobby refresh";
    Model.Publish(Status);
    const auto View = Model.Snapshot();
    CHECK(View.Diagnostics == "Desync: ABWEICHUNG Inventar");
    CHECK(View.Status.Message == "Lobby refresh");
    CHECK(View.Pending);
    Dispatcher.Drain();
    CHECK(Calls == 1);
    Model.PublishDiagnostics(std::string(600, 'x'));
    CHECK(Model.Snapshot().Diagnostics.size() == 512);
    CHECK(View.Diagnostics == "Desync: ABWEICHUNG Inventar");
}

TEST_CASE("Concurrent diagnostics and lobby producers do not overwrite each other")
{
    Core::Dispatcher Dispatcher;
    UI::OverlayModel Model(Dispatcher);
    std::thread Diagnostics([&] { for (unsigned i = 0; i < 1000; ++i) Model.PublishDiagnostics(std::to_string(i)); });
    std::thread Lobby([&]
    {
        for (unsigned i = 0; i < 1000; ++i)
        {
            auto Status = Offline();
            Status.PingMs = i;
            Model.Publish(Status);
        }
    });
    for (unsigned i = 0; i < 1000; ++i) CHECK(Model.Snapshot().Diagnostics.size() <= 512);
    Diagnostics.join();
    Lobby.join();
    CHECK(Model.Snapshot().Diagnostics == "999");
    CHECK(Model.Snapshot().Status.PingMs == 999);
}

TEST_CASE("Dev buttons are only accepted while playing and are taken once")
{
    Core::Dispatcher Dispatcher;
    UI::OverlayModel Model(Dispatcher);
    CHECK_FALSE(Model.RequestDev(UI::DevButton::SpawnXp));
    auto Status = Offline();
    Status.State = UI::SessionState::Playing;
    Status.Role = UI::SessionRole::Client;
    Model.Publish(Status);
    CHECK(Model.RequestDev(UI::DevButton::SpawnXp));
    CHECK(Model.RequestDev(UI::DevButton::SpawnXp));
    CHECK(Model.RequestDev(UI::DevButton::FullTank));
    CHECK(Model.TakeDevRequests() == ((1u << 1) | (1u << 2)));
    CHECK(Model.TakeDevRequests() == 0);
    Status.State = UI::SessionState::Lobby;
    Model.Publish(Status);
    CHECK_FALSE(Model.RequestDev(UI::DevButton::FullTank));
    CHECK(Model.TakeDevRequests() == 0);
}
