#include <doctest.h>
#include "Sync/Players/PauseState.h"

using namespace Sync::Pause;

TEST_CASE("Pause: neun Byte, Sitzung ungleich 0, Flag nur 0 oder 1")
{
    const Report Value{0x1122334455667789ull, true};
    const auto Bytes = Encode(Value);
    REQUIRE(Bytes.size() == ClientPauseBytes);
    CHECK(Bytes[0] == 0x89);
    CHECK(Bytes[7] == 0x11);
    CHECK(Bytes[8] == 1);
    CHECK(*Decode(Bytes) == Value);
    CHECK(*Decode(Encode({5, false})) == Report{5, false});
    CHECK(Encode({0, true}).empty());
    for (size_t Size = 0; Size < Bytes.size(); ++Size)
        CHECK_FALSE(Decode(std::span(Bytes).first(Size)));
    auto Extra = Bytes;
    Extra.push_back(0);
    CHECK_FALSE(Decode(Extra));
    auto BadFlag = Bytes;
    BadFlag[8] = 2;
    CHECK_FALSE(Decode(BadFlag));
    std::vector<uint8_t> Zero(ClientPauseBytes, 0);
    CHECK_FALSE(Decode(Zero));
}

TEST_CASE("Pause: Client meldet Öffnen sofort, wiederholt und schickt die Freigabe 3 s lang")
{
    ClientReporter Client(100);
    CHECK_FALSE(Client.Update(false, 0)); // nie offen: nichts
    const auto First = Client.Update(true, 1000);
    REQUIRE(First);
    CHECK(First->Open);
    CHECK(First->Session == 101);
    CHECK_FALSE(Client.Update(true, 1200));
    CHECK(Client.Update(true, 1500) == First);
    const auto Closed = Client.Update(false, 1600);
    REQUIRE(Closed);
    CHECK(*Closed == Report{101, false});
    CHECK_FALSE(Client.Update(false, 1700));
    CHECK(Client.Update(false, 2100) == Closed);
    CHECK(Client.Update(false, 4599) == Closed);
    CHECK_FALSE(Client.Update(false, 4600)); // nach 3 s Ruhe
    CHECK_FALSE(Client.Update(false, 9000));
    const auto Second = Client.Update(true, 10000);
    REQUIRE(Second);
    CHECK(Second->Session == 103); // jedes Öffnen eine neue Sitzung
}

TEST_CASE("Pause: Client nach Trennung, offenes Menü beginnt neue Sitzung")
{
    ClientReporter Client(7);
    REQUIRE(Client.Update(true, 0)->Session == 7);
    Client.Reset();
    const auto Again = Client.Update(true, 100);
    REQUIRE(Again);
    CHECK(Again->Session == 9);
    Client.Reset();
    CHECK_FALSE(Client.Update(false, 200)); // geschlossen nach Reset: keine Freigabe nötig
}

namespace
{
constexpr uint64_t Peer = 42, World = 0xABC;
}

TEST_CASE("Pause: Host hält, bis der Client schließt")
{
    HostHold Host;
    Host.Tick(Peer, World, 0);
    CHECK_FALSE(Host.Held());
    Host.Receive({11, true}, 100);
    Host.Tick(Peer, World, 100);
    CHECK(Host.Held());
    CHECK(Host.HeldSession() == 11);
    Host.Receive({11, true}, 600);
    Host.Tick(Peer, World, 3500); // 2,9 s nach der letzten Meldung
    CHECK(Host.Held());
    Host.Receive({11, false}, 3600);
    Host.Tick(Peer, World, 3600);
    CHECK_FALSE(Host.Held());
    CHECK(Host.TakeRelease() == Release::Closed);
    CHECK(Host.TakeRelease() == Release::None);
    Host.Receive({11, false}, 4000); // Wiederholte Freigabe ändert nichts
    CHECK_FALSE(Host.Held());
    CHECK(Host.TakeRelease() == Release::None);
}

TEST_CASE("Pause: Freigabe einer fremden Sitzung lässt die gehaltene stehen")
{
    HostHold Host;
    Host.Tick(Peer, World, 0);
    Host.Receive({11, true}, 0);
    Host.Receive({9, false}, 10);
    CHECK(Host.Held());
    Host.Receive({13, true}, 20); // neue Sitzung ersetzt die alte
    CHECK(Host.HeldSession() == 13);
    Host.Receive({11, false}, 30);
    CHECK(Host.Held());
}

TEST_CASE("Pause: Zeitablauf gibt frei, die späte Wiederholung pausiert nicht erneut")
{
    HostHold Host;
    Host.Tick(Peer, World, 0);
    Host.Receive({11, true}, 1000);
    Host.Tick(Peer, World, 1000 + LeaseMs);
    CHECK(Host.Held());
    Host.Tick(Peer, World, 1001 + LeaseMs);
    CHECK_FALSE(Host.Held());
    CHECK(Host.TakeRelease() == Release::Timeout);
    Host.Receive({11, true}, 5000);
    CHECK_FALSE(Host.Held());
    Host.Receive({13, true}, 5100); // erst ein neues Öffnen gilt wieder
    CHECK(Host.Held());
}

TEST_CASE("Pause: Trennung und Weltwechsel geben frei")
{
    HostHold Host;
    Host.Tick(Peer, World, 0);
    Host.Receive({11, true}, 0);
    Host.Tick(Peer, World + 1, 10);
    CHECK_FALSE(Host.Held());
    CHECK(Host.TakeRelease() == Release::World);
    Host.Receive({11, true}, 20); // dieselbe Sitzung aus der alten Welt
    CHECK_FALSE(Host.Held());
    Host.Receive({13, true}, 30);
    CHECK(Host.Held());
    Host.Tick(0, World + 1, 40);
    CHECK_FALSE(Host.Held());
    CHECK(Host.TakeRelease() == Release::Peer);
    // Neue Verbindung: Sitzungen der alten zählen nicht mehr als beendet, der neue Client zählt neu.
    Host.Tick(Peer + 1, World + 1, 50);
    Host.Receive({13, true}, 60);
    CHECK(Host.Held());
    Host.Tick(Peer + 1, 0, 70); // Welt nicht lesbar: frei
    CHECK_FALSE(Host.Held());
}
