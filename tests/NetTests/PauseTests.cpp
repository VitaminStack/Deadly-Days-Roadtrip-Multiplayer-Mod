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

TEST_CASE("Pause: Client räumt Zeitanfragen einmal je Reise ab, nicht in der ersten Welt")
{
    TravelCleanup Cleanup;
    CHECK_FALSE(Cleanup.Due(0, 0));     // noch keine Welt
    CHECK_FALSE(Cleanup.Due(World, 10)); // erste Welt: keine alte Welt, nichts abzuräumen
    CHECK_FALSE(Cleanup.Due(World, 20));
    CHECK(Cleanup.Due(World + 1, 30)); // Reise
    CHECK(Cleanup.Due(World + 1, 40)); // Manager noch nicht erreichbar: weiter fällig
    Cleanup.Done();
    CHECK_FALSE(Cleanup.Due(World + 1, 50));
    CHECK(Cleanup.Due(World + 2, 60)); // nächste Reise
    Cleanup.Done();
    CHECK_FALSE(Cleanup.Due(World + 2, 70));
}

// opt-boss-tafel-client: Die Abschlusstafel des Hosts pausiert die Welt (Pauser repliziert), der Client hat dabei keine eigene
// Zeitanfrage und kein offenes Pausemenü (gemessen 19.11. und 21.11.2026).
TEST_CASE("Pause: Hinweis beim Client, solange nur der Host die Welt anhält")
{
    RemotePauseNotice Notice;
    CHECK_FALSE(Notice.Update(true, false, false, World, 0));  // Welt läuft
    CHECK_FALSE(Notice.Update(true, true, false, World, 100)); // Pause beginnt, Wartezeit
    CHECK_FALSE(Notice.Update(true, true, false, World, 100 + RemotePauseNoticeDelayMs - 1));
    CHECK(Notice.Update(true, true, false, World, 100 + RemotePauseNoticeDelayMs));
    CHECK(Notice.Update(true, true, false, World, 60000)); // bleibt, solange der Host die Tafel offen hat
    CHECK_FALSE(Notice.Update(true, false, false, World, 60100)); // Host hat geschlossen: sofort weg
    // Die nächste Pause wartet wieder.
    CHECK_FALSE(Notice.Update(true, true, false, World, 60200));
    CHECK(Notice.Update(true, true, false, World, 60200 + RemotePauseNoticeDelayMs));
}

TEST_CASE("Pause: kein Hinweis bei eigener Pause, kurzer Pause, ohne Verbindung oder ohne Welt")
{
    RemotePauseNotice Notice;
    // Eigenes Pausemenü oder eigene Zeitanfrage (etwa Rundenübersicht): Der Client sieht selbst, warum die Welt steht.
    CHECK_FALSE(Notice.Update(true, true, true, World, 0));
    CHECK_FALSE(Notice.Update(true, true, true, World, 5000));
    // Schließt der Client sein Menü, während der Host weiter pausiert, beginnt die Wartezeit erst dann.
    CHECK_FALSE(Notice.Update(true, true, false, World, 5100));
    CHECK(Notice.Update(true, true, false, World, 5100 + RemotePauseNoticeDelayMs));
    // Öffnet er es wieder, ist der Hinweis sofort weg.
    CHECK_FALSE(Notice.Update(true, true, true, World, 7000));
    // Kurze Pause unter der Wartezeit zeigt nichts.
    RemotePauseNotice Short;
    CHECK_FALSE(Short.Update(true, true, false, World, 0));
    CHECK_FALSE(Short.Update(true, false, false, World, 500));
    CHECK_FALSE(Short.Update(true, true, false, World, 900));
    CHECK_FALSE(Short.Update(true, true, false, World, 1500)); // zählt ab 900, nicht ab 0
    // Host oder getrennt, und Welt nicht lesbar.
    RemotePauseNotice Off;
    CHECK_FALSE(Off.Update(false, true, false, World, 0));
    CHECK_FALSE(Off.Update(false, true, false, World, 9000));
    CHECK_FALSE(Off.Update(true, true, false, 0, 9000));
    CHECK_FALSE(Off.Update(true, true, false, 0, 19000));
}

TEST_CASE("Pause: Weltwechsel und Trennung nehmen den Hinweis weg")
{
    RemotePauseNotice Notice;
    CHECK_FALSE(Notice.Update(true, true, false, World, 0));
    CHECK(Notice.Update(true, true, false, World, RemotePauseNoticeDelayMs));
    // Neue Welt, dort steht die Zeit ebenfalls (Laden): Wartezeit beginnt neu.
    CHECK_FALSE(Notice.Update(true, true, false, World + 1, RemotePauseNoticeDelayMs + 10));
    CHECK(Notice.Update(true, true, false, World + 1, 2 * RemotePauseNoticeDelayMs + 10));
    CHECK_FALSE(Notice.Update(false, true, false, World + 1, 2 * RemotePauseNoticeDelayMs + 20)); // Verbindung weg
    CHECK_FALSE(Notice.Update(true, true, false, World + 1, 2 * RemotePauseNoticeDelayMs + 30));  // wieder da: neu warten
}

TEST_CASE("Pause: Abräumen gibt nach 30 s ohne Manager auf, eine nicht lesbare Welt ist kein Wechsel")
{
    TravelCleanup Cleanup;
    CHECK_FALSE(Cleanup.Due(World, 0));
    CHECK(Cleanup.Due(World + 1, 1000));
    CHECK(Cleanup.Due(World + 1, 1000 + TravelCleanupGiveUpMs));
    CHECK_FALSE(Cleanup.Due(World + 1, 1001 + TravelCleanupGiveUpMs)); // spätere Anfragen gehören der neuen Welt
    CHECK_FALSE(Cleanup.Due(0, 40000));          // Welt kurz nicht lesbar
    CHECK_FALSE(Cleanup.Due(World + 1, 40100));  // dieselbe Welt: kein Wechsel, gültige Anfragen bleiben
    CHECK_FALSE(Cleanup.Due(0, 41000));          // Laden
    CHECK(Cleanup.Due(World + 2, 42000));        // neue Welt nach dem Laden
    CHECK_FALSE(Cleanup.Due(0, 42100));          // ohne Welt kein Versuch
    CHECK(Cleanup.Due(World + 2, 42200));        // steht weiter aus
    Cleanup.Done();
    CHECK_FALSE(Cleanup.Due(World + 2, 42300));
}
