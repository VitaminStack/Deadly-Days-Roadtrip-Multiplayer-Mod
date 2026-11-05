#include <doctest.h>

#include <vector>

#include "Net/NetSim.h"

using namespace Net;

namespace
{
std::vector<uint8_t> Packet(uint8_t Value)
{
    return {Value};
}

// Sammelt, was die Warteschlange bis NowMs freigibt.
std::vector<uint8_t> Drain(NetSimQueue& Queue, uint64_t NowMs)
{
    std::vector<uint8_t> Out;
    Queue.PopReady(NowMs, [&](std::span<const uint8_t> Bytes, bool) { Out.push_back(Bytes[0]); });
    return Out;
}
}

TEST_CASE("Netzsimulation: Schalter aus der Kommandozeile")
{
    CHECK_FALSE(ParseNetSim(L"game.exe -windowed").has_value());

    const auto Full = ParseNetSim(L"game.exe -ddcoop_netsim=120,30,2 -nosplash");
    REQUIRE(Full);
    CHECK(Full->PingMs == 120);
    CHECK(Full->JitterMs == 30);
    CHECK(Full->LossPercent == doctest::Approx(2.0));
    CHECK(Full->Enabled());

    const auto PingOnly = ParseNetSim(L"game.exe -ddcoop_netsim=80");
    REQUIRE(PingOnly);
    CHECK(PingOnly->PingMs == 80);
    CHECK(PingOnly->JitterMs == 0);
    CHECK(PingOnly->LossPercent == 0);

    const auto Fraction = ParseNetSim(L"\"game.exe\" -ddcoop_netsim=0,0,0.5");
    REQUIRE(Fraction);
    CHECK(Fraction->LossPercent == doctest::Approx(0.5));
    CHECK(Fraction->Enabled());
    CHECK_FALSE(ParseNetSim(L"-ddcoop_netsim=0")->Enabled());

    CHECK_FALSE(ParseNetSim(L"-ddcoop_netsim=").has_value());
    CHECK_FALSE(ParseNetSim(L"-ddcoop_netsim=120,,2").has_value());
    CHECK_FALSE(ParseNetSim(L"-ddcoop_netsim=120,30,").has_value());
    CHECK_FALSE(ParseNetSim(L"-ddcoop_netsim=120,30,2,1").has_value());
    CHECK_FALSE(ParseNetSim(L"-ddcoop_netsim=-5").has_value());
    CHECK_FALSE(ParseNetSim(L"-ddcoop_netsim=1.2.3").has_value());
    CHECK_FALSE(ParseNetSim(L"-ddcoop_netsim=5000").has_value());
    CHECK_FALSE(ParseNetSim(L"-ddcoop_netsim=100,0,60").has_value());
    CHECK(NetSimFromCommandLine(L"-ddcoop_netsim=x").Enabled() == false);
}

TEST_CASE("Netzsimulation: Spielpakete kommen nach dem halben Ping")
{
    NetSimQueue Queue({120, 0, 0}, 1);
    CHECK(Queue.Push(1000, Packet(1), false));
    CHECK(Queue.Push(1010, Packet(2), false));
    CHECK(Drain(Queue, 1059).empty());
    CHECK(Drain(Queue, 1060) == std::vector<uint8_t>{1});
    CHECK(Drain(Queue, 1070) == std::vector<uint8_t>{2});
    CHECK(Queue.Pending() == 0);

    const NetSimStats Stats = Queue.TakeStats();
    CHECK(Stats.Delayed == 2);
    CHECK(Stats.MinMs == 60);
    CHECK(Stats.MaxMs == 60);
    CHECK(Stats.Lost == 0);
    CHECK(Queue.TakeStats().Delayed == 0);
}

TEST_CASE("Netzsimulation: Schwankung bleibt im Profil, Verlust trifft etwa den Anteil")
{
    NetSimQueue Queue({120, 30, 2}, 7);
    for (int Index = 0; Index < 20000; ++Index)
        Queue.Push(0, Packet(0), false);
    const NetSimStats Stats = Queue.TakeStats();
    CHECK(Stats.Delayed + Stats.Lost == 20000);
    CHECK(Stats.MinMs >= 45);
    CHECK(Stats.MaxMs <= 75);
    CHECK(Stats.MinMs <= 47);
    CHECK(Stats.MaxMs >= 73);
    const double Mean = static_cast<double>(Stats.DelaySumMs) / static_cast<double>(Stats.Delayed);
    CHECK(Mean == doctest::Approx(60.0).epsilon(0.02));
    const double LostPercent = 100.0 * static_cast<double>(Stats.Lost) / 20000.0;
    CHECK(LostPercent > 1.5);
    CHECK(LostPercent < 2.5);
    CHECK(Describe(Stats).find("verloren") != std::string::npos);
}

TEST_CASE("Netzsimulation: zuverlässige Nachrichten gehen nie verloren und behalten ihre Reihenfolge")
{
    NetSimQueue Queue({100, 80, 50}, 3);
    std::vector<uint8_t> Expected;
    for (uint8_t Index = 0; Index < 50; ++Index)
    {
        CHECK(Queue.Push(Index, Packet(Index), true));
        Expected.push_back(Index);
    }
    // Ohne Schwankung: die erste Nachricht nach genau dem halben Ping.
    CHECK(Drain(Queue, 49).empty());
    CHECK(Drain(Queue, 50) == std::vector<uint8_t>{0});
    std::vector<uint8_t> Rest = Drain(Queue, 10000);
    Rest.insert(Rest.begin(), 0);
    CHECK(Rest == Expected);
    CHECK(Queue.TakeStats().Lost == 0);
}

TEST_CASE("Netzsimulation: Profil wird lesbar beschrieben")
{
    CHECK(Describe(NetSimProfile{120, 30, 2}) == "Ping 120 ms ±30 ms, Verlust 2.0 % je Richtung");
}
