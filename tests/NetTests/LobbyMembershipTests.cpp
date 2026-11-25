// lobby-rauswurf: Wann der Host einen verbundenen Mitspieler wegen der Steam-Lobby trennt. Reine Logik, die Zeit kommt
// als Zahl herein, keine Wartezeiten. Siehe src/Net/LobbyMembership.h und docs/faelle/lobby-rauswurf.md.
#include <doctest.h>

#include <cstdint>
#include <vector>

#include "Net/LobbyMembership.h"

using namespace Net;

namespace
{
constexpr uint64_t kOwn = 8001;
constexpr uint64_t kFriend = 8002;

// Schickt Listen im Abstand StepMs, wie der Game Thread je Frame.
struct Feed
{
    MembershipRules Rules{1000, 3, 200};
    MemberWatch Watch;
    uint64_t Sequence = 0;
    uint64_t Now = 1000;

    MemberVerdict Next(LobbyListState State, bool Contains, uint64_t StepMs = 50)
    {
        Now += StepMs;
        return Judge(Watch, Rules, ++Sequence, State, Contains, Now);
    }
    MemberVerdict Repeat(LobbyListState State, bool Contains, int Times, uint64_t StepMs = 50)
    {
        MemberVerdict Last = MemberVerdict::Present;
        for (int Index = 0; Index < Times; ++Index)
            Last = Next(State, Contains, StepMs);
        return Last;
    }
};
}

TEST_CASE("Lobby-Liste: nur mit eigenem Konto und vollständig gültig")
{
    CHECK(ClassifyLobbyList({kOwn, kFriend}, 2, kOwn) == LobbyListState::Valid);
    CHECK(ClassifyLobbyList({}, 0, kOwn) == LobbyListState::Empty);
    CHECK(ClassifyLobbyList({}, 2, kOwn) == LobbyListState::Empty);
    CHECK(ClassifyLobbyList({kFriend}, 1, kOwn) == LobbyListState::OwnMissing);
    CHECK(ClassifyLobbyList({kOwn}, 2, kOwn) == LobbyListState::Incomplete);
    CHECK(ClassifyLobbyList({kOwn, kFriend}, 2, 0) == LobbyListState::OwnMissing);
}

TEST_CASE("Lobby-Mitgliedschaft: Trennen erst nach Frist und mehreren gültigen Listen")
{
    Feed Host;
    CHECK(Host.Next(LobbyListState::Valid, true) == MemberVerdict::Present);

    SUBCASE("Dauerhaft fehlend in gültigen Listen trennt")
    {
        // Die erste fehlende Liste startet die Frist, jede weitere rechnet 50 ms an.
        CHECK(Host.Repeat(LobbyListState::Valid, false, 20) == MemberVerdict::Missing);
        CHECK(Host.Watch.MissingMs == 950);
        CHECK(Host.Next(LobbyListState::Valid, false) == MemberVerdict::Revoke);
    }
    SUBCASE("Kurze Lücke setzt die Frist zurück")
    {
        Host.Repeat(LobbyListState::Valid, false, 15);
        CHECK(Host.Next(LobbyListState::Valid, true) == MemberVerdict::Present);
        CHECK(Host.Watch.MissingMs == 0);
        CHECK(Host.Repeat(LobbyListState::Valid, false, 15) == MemberVerdict::Missing);
    }
    SUBCASE("Frist allein reicht nicht, es braucht mehrere Listen")
    {
        // Zwei Listen mit großem Abstand: Zeit wäre um, aber nur zwei Fehltreffer, und je Liste zählen höchstens 200 ms.
        CHECK(Host.Next(LobbyListState::Valid, false, 5000) == MemberVerdict::Missing);
        CHECK(Host.Next(LobbyListState::Valid, false, 5000) == MemberVerdict::Missing);
        CHECK(Host.Watch.MissingMs == 200);
    }
}

TEST_CASE("Lobby-Mitgliedschaft: leere oder veraltete Listen trennen niemanden")
{
    Feed Host;
    Host.Next(LobbyListState::Valid, true);

    SUBCASE("Leere Liste, auch sehr lange")
    {
        CHECK(Host.Repeat(LobbyListState::Empty, false, 1000) == MemberVerdict::Paused);
        CHECK(Host.Watch.MissingMs == 0);
        CHECK(Host.Next(LobbyListState::Valid, true) == MemberVerdict::Present);
    }
    SUBCASE("Liste ohne eigenes Konto oder unvollständig")
    {
        CHECK(Host.Repeat(LobbyListState::OwnMissing, false, 500) == MemberVerdict::Paused);
        CHECK(Host.Repeat(LobbyListState::Incomplete, false, 500) == MemberVerdict::Paused);
        CHECK(Host.Next(LobbyListState::Valid, true) == MemberVerdict::Present);
    }
    SUBCASE("Unbrauchbare Liste hält die Frist an, ohne sie zurückzusetzen")
    {
        Host.Repeat(LobbyListState::Valid, false, 10);
        const uint64_t Before = Host.Watch.MissingMs;
        CHECK(Before == 450);
        CHECK(Host.Repeat(LobbyListState::Empty, false, 100) == MemberVerdict::Paused);
        CHECK(Host.Watch.MissingMs == Before);
        // Die Zeit seit der unbrauchbaren Liste zählt nicht, danach läuft die Frist weiter.
        CHECK(Host.Next(LobbyListState::Valid, false) == MemberVerdict::Missing);
        CHECK(Host.Watch.MissingMs == Before);
        CHECK(Host.Repeat(LobbyListState::Valid, false, 11) == MemberVerdict::Revoke);
    }
    SUBCASE("Keine neue Liste, etwa bei stehendem Game Thread, lässt nichts altern")
    {
        Host.Next(LobbyListState::Valid, false);
        const uint64_t Sequence = Host.Sequence;
        for (uint64_t Now = Host.Now; Now < Host.Now + 60000; Now += 100)
            CHECK(Judge(Host.Watch, Host.Rules, Sequence, LobbyListState::Valid, false, Now) == MemberVerdict::Missing);
        CHECK(Host.Watch.MissingMs == 0);
        // Die nächste Liste nach der Pause rechnet höchstens MaxStepMs an.
        Host.Now += 60000;
        CHECK(Host.Next(LobbyListState::Valid, false) == MemberVerdict::Missing);
        CHECK(Host.Watch.MissingMs == 200);
    }
}
