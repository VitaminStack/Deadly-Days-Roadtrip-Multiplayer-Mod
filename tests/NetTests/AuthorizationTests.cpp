// Zugriffskontrolle des Hosts: Nur Mitglieder der eigenen Steam-Lobby mit einer von Steam bestätigten Identität bekommen
// den Platz. Der Versions-Handshake bleibt zusätzlich bestehen. Siehe src/Net/README.md, Abschnitt Zugriffskontrolle.
#include <doctest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include "FakeSteam.h"
#include "Net/Transport.h"

using namespace Net;

namespace
{
constexpr uint64_t kHostId = 6001;
constexpr uint64_t kFriendId = 6002;
constexpr uint64_t kStrangerId = 6003;

// Lobby-Prüfung wie in einer normalen Sitzung: ohne Testmodus, ohne 127.0.0.1.
TransportOptions LobbyPorts(uint16_t Base)
{
    TransportOptions Options;
    Options.GamePort = Base;
    Options.ClientPort = static_cast<uint16_t>(Base + 1);
    Options.LoopbackPort = static_cast<uint16_t>(Base + 2);
    Options.StatsIntervalMs = 100;
    Options.AuthorizeWaitMs = 300;
    Options.RevokeAfterMs = 200;
    return Options;
}

// Ein zweiter Client auf demselben PC braucht einen eigenen UDP-Port.
TransportOptions OtherClient(TransportOptions Options, uint16_t Offset)
{
    Options.ClientPort = static_cast<uint16_t>(Options.ClientPort + Offset);
    return Options;
}

bool WaitFor(const std::function<bool()>& Condition, int Milliseconds = 3000)
{
    const auto Deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(Milliseconds);
    while (std::chrono::steady_clock::now() < Deadline)
    {
        if (Condition())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return Condition();
}

constexpr const char* kNotMember = "Der Host hat die Verbindung abgelehnt: nicht Mitglied seiner Lobby";
}

TEST_CASE("Zugriff: Mitglied der Lobby verbindet sich, Fremde nicht")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = LobbyPorts(47950);
    Transport Host(Network->Account(kHostId), Options);
    REQUIRE(Host.StartHost());
    Host.SetLobbyMembers({kHostId, kFriendId});

    SUBCASE("Berechtigter Freund in der Lobby")
    {
        Transport Friend(Network->Account(kFriendId), Options);
        REQUIRE(Friend.StartClient(kHostId));
        CHECK(WaitFor([&] { return Friend.Stats().State == TransportState::Connected && Host.Stats().Connections == 1; }));
    }
    SUBCASE("Steam-Konto ohne Lobby-Mitgliedschaft")
    {
        Transport Stranger(Network->Account(kStrangerId), Options);
        REQUIRE(Stranger.StartClient(kHostId));
        REQUIRE(WaitFor([&] { return Stranger.Stats().State == TransportState::Failed; }));
        CHECK(Stranger.Stats().Message == kNotMember);
        CHECK(Host.Stats().Connections == 0);
        CHECK(Host.Stats().State == TransportState::Listening);
        CHECK(Host.ControlPeers().empty());
    }
    SUBCASE("Fehlende Steam-Identität")
    {
        // Selbst ein Mitglied ohne bestätigte Identität kommt nicht hinein: Die SteamID stammt nur von Steam.
        Network->HideIdentityOf(kFriendId);
        Transport Friend(Network->Account(kFriendId), Options);
        REQUIRE(Friend.StartClient(kHostId));
        REQUIRE(WaitFor([&] { return Friend.Stats().State == TransportState::Failed; }));
        CHECK(Friend.Stats().Message == kNotMember);
    }
    SUBCASE("Freund mit falscher Mod-Version scheitert am Handshake")
    {
        TransportOptions Old = Options;
        Old.Identity.Mod = Options.Identity.Mod + 1;
        Transport Friend(Network->Account(kFriendId), Old);
        REQUIRE(Friend.StartClient(kHostId));
        REQUIRE(WaitFor([&] { return Friend.Stats().State == TransportState::Failed; }));
        CHECK(Friend.Stats().Message != kNotMember);
        CHECK(Host.Stats().Connections == 0);
    }
}

TEST_CASE("Zugriff: ohne Lobby nimmt der Host niemanden an")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = LobbyPorts(47960);
    Transport Host(Network->Account(kHostId), Options);
    REQUIRE(Host.StartHost());

    Transport Friend(Network->Account(kFriendId), Options);
    REQUIRE(Friend.StartClient(kHostId));
    REQUIRE(WaitFor([&] { return Friend.Stats().State == TransportState::Failed; }));
    CHECK(Friend.Stats().Message == kNotMember);
    Friend.Stop();

    // Lobby geschlossen: Auch ein früheres Mitglied kommt nicht mehr herein.
    Host.SetLobbyMembers({kHostId, kFriendId});
    Host.ClearLobbyMembers();
    Transport Again(Network->Account(kFriendId), Options);
    REQUIRE(Again.StartClient(kHostId));
    REQUIRE(WaitFor([&] { return Again.Stats().State == TransportState::Failed; }));
    CHECK(Host.Stats().Connections == 0);
}

TEST_CASE("Zugriff: Steam meldet den Beitritt erst nach dem Verbindungsversuch")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    TransportOptions Options = LobbyPorts(47970);
    Options.AuthorizeWaitMs = 2000;
    Transport Host(Network->Account(kHostId), Options);
    REQUIRE(Host.StartHost());
    Host.SetLobbyMembers({kHostId});

    Transport Friend(Network->Account(kFriendId), Options);
    REQUIRE(Friend.StartClient(kHostId));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(Friend.Stats().State == TransportState::Connecting);
    CHECK(Host.Stats().Connections == 0);
    Host.SetLobbyMembers({kHostId, kFriendId});
    CHECK(WaitFor([&] { return Friend.Stats().State == TransportState::Connected && Host.Stats().Connections == 1; }));
}

TEST_CASE("Zugriff: Fremder Versuch vor dem Freund belegt keinen Platz")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    TransportOptions Options = LobbyPorts(47980);
    Options.AuthorizeWaitMs = 2000;
    Transport Host(Network->Account(kHostId), Options);
    REQUIRE(Host.StartHost());
    Host.SetLobbyMembers({kHostId, kFriendId});

    // Der Fremde wartet noch auf die Lobby, der Freund kommt trotzdem sofort an.
    Transport Stranger(Network->Account(kStrangerId), OtherClient(Options, 10));
    REQUIRE(Stranger.StartClient(kHostId));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Transport Friend(Network->Account(kFriendId), Options);
    REQUIRE(Friend.StartClient(kHostId));
    CHECK(WaitFor([&] { return Friend.Stats().State == TransportState::Connected && Host.Stats().Connections == 1; }));
    CHECK(WaitFor([&] { return Stranger.Stats().State == TransportState::Failed; }));
    CHECK(Stranger.Stats().Message == kNotMember);
    CHECK(Friend.Stats().State == TransportState::Connected);

    SUBCASE("Ein zweites Mitglied bekommt Busy, nicht den Platz")
    {
        constexpr uint64_t kSecondFriend = 6004;
        Host.SetLobbyMembers({kHostId, kFriendId, kSecondFriend});
        Transport Second(Network->Account(kSecondFriend), OtherClient(Options, 20));
        REQUIRE(Second.StartClient(kHostId));
        REQUIRE(WaitFor([&] { return Second.Stats().State == TransportState::Failed; }));
        CHECK(Second.Stats().Message == "Der Host hat schon einen Mitspieler");
        CHECK(Friend.Stats().State == TransportState::Connected);
    }
}

TEST_CASE("Zugriff: Verlässt der Mitspieler die Lobby, verliert er den Platz; neu verbinden wird neu geprüft")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = LobbyPorts(47990);
    Transport Host(Network->Account(kHostId), Options);
    REQUIRE(Host.StartHost());
    Host.SetLobbyMembers({kHostId, kFriendId});
    {
        Transport Friend(Network->Account(kFriendId), Options);
        REQUIRE(Friend.StartClient(kHostId));
        REQUIRE(WaitFor([&] { return Friend.Stats().State == TransportState::Connected && Host.Stats().Connections == 1; }));

        // Eine kurze Lücke in der Mitgliederliste trennt niemanden.
        Host.SetLobbyMembers({kHostId});
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Host.SetLobbyMembers({kHostId, kFriendId});
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        CHECK(Friend.Stats().State == TransportState::Connected);

        Host.SetLobbyMembers({kHostId});
        REQUIRE(WaitFor([&] { return Friend.Stats().State == TransportState::Failed; }));
        CHECK(Friend.Stats().Message == kNotMember);
        CHECK(WaitFor([&] { return Host.Stats().State == TransportState::Listening && Host.Stats().Connections == 0; }));
        CHECK(Host.ControlPeers().empty());
    }
    Transport Again(Network->Account(kFriendId), Options);
    REQUIRE(Again.StartClient(kHostId));
    REQUIRE(WaitFor([&] { return Again.Stats().State == TransportState::Failed; }));
    CHECK(Again.Stats().Message == kNotMember);
    Again.Stop();

    Host.SetLobbyMembers({kHostId, kFriendId});
    Transport Back(Network->Account(kFriendId), Options);
    REQUIRE(Back.StartClient(kHostId));
    CHECK(WaitFor([&] { return Back.Stats().State == TransportState::Connected && Host.Stats().Connections == 1; }));
}

TEST_CASE("Zugriff: 127.0.0.1 nur im ausdrücklichen Testmodus")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    SUBCASE("Normale Sitzung")
    {
        const TransportOptions Options = LobbyPorts(48000);
        Transport Host(Network->Account(kHostId), Options);
        REQUIRE(Host.StartHost());
        Host.SetLobbyMembers({kHostId, kFriendId});
        Transport Local(Network->Account(kFriendId), Options);
        // Ohne Listen-Socket auf 127.0.0.1 findet Steam niemanden.
        CHECK_FALSE(Local.StartLoopbackClient());
        CHECK(Host.Stats().Connections == 0);
    }
    SUBCASE("Testmodus")
    {
        TransportOptions Options = LobbyPorts(48010);
        Options.AllowLoopback = true;
        Transport Host(Network->Account(kHostId), Options);
        REQUIRE(Host.StartHost());
        Transport Local(Network->Account(kFriendId), Options);
        REQUIRE(Local.StartLoopbackClient());
        CHECK(WaitFor([&] { return Local.Stats().State == TransportState::Connected && Host.Stats().Connections == 1; }));
    }
}

TEST_CASE("Zugriff: viele Fremdversuche stauen sich nicht auf")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    TransportOptions Options = LobbyPorts(48020);
    Options.AuthorizeWaitMs = 2000;
    Transport Host(Network->Account(kHostId), Options);
    REQUIRE(Host.StartHost());
    Host.SetLobbyMembers({kHostId, kFriendId});

    std::vector<std::unique_ptr<Transport>> Strangers;
    for (uint16_t Index = 0; Index < 6; ++Index)
    {
        Strangers.push_back(std::make_unique<Transport>(Network->Account(7000 + Index), OtherClient(Options, 10 + Index)));
        REQUIRE(Strangers.back()->StartClient(kHostId));
    }
    // Mehr als vier wartende Versuche lehnt der Host sofort ab, lange bevor AuthorizeWaitMs abläuft.
    CHECK(WaitFor([&] {
        int Failed = 0;
        for (const auto& Each : Strangers)
            Failed += Each->Stats().State == TransportState::Failed;
        return Failed >= 2;
    }, 1000));

    Transport Friend(Network->Account(kFriendId), Options);
    REQUIRE(Friend.StartClient(kHostId));
    CHECK(WaitFor([&] { return Friend.Stats().State == TransportState::Connected && Host.Stats().Connections == 1; }));
}

TEST_CASE("Zugriff: nur einzelne Konten im öffentlichen Steam-Universum zählen als Identität")
{
    CHECK(IsIndividualSteamId(76561197960287930ull));
    CHECK_FALSE(IsIndividualSteamId(0));
    // Konto-Nummer 0.
    CHECK_FALSE(IsIndividualSteamId(76561197960265728ull));
    // Lobby (Kontotyp 8, Chat).
    CHECK_FALSE(IsIndividualSteamId(109775240000000001ull));
    CHECK(EndMessage(false, SteamConnectionState::ClosedByPeer, EndReason::NotAuthorized, false) == kNotMember);
}
