#include <doctest.h>
#include "Game/RejoinPolicy.h"

using namespace Game::Rejoin;

namespace
{
constexpr uint64_t Alice = 0x1111, Bob = 0x2222;
constexpr uint64_t OldInventory = 10, NewInventory = 20, ThirdInventory = 30;

Snapshot Played()
{
    Snapshot S;
    S.Items.push_back({"ItemData_Revolver", "Rarity_2_Rare", false, true, false, 1, {0, 0}, {{0, 0}, {1, 0}}});
    S.Items.push_back({"ItemData_Pistol", "Rarity_0_Common", true, false, false, 0, {}, {}});
    S.Items.push_back({"ItemData_AstronautSuit", "Rarity_0_Common", false, true, false, 0, {0, 1}, {{0, 1}}});
    S.Slots = {{0, 0}, {0, 1}, {1, 0}, {1, 1}, {2, 0}};
    return S;
}

Snapshot Start()
{
    Snapshot S;
    S.Items.push_back({"ItemData_Pistol", "Rarity_0_Common", false, true, false, 0, {0, 0}, {{0, 0}}});
    S.Slots = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
    return S;
}

// Der Client spielt mit OldInventory, trennt sich und kommt mit NewInventory (Startinventar) zurück; Zeit ab T. Nur Key ist verbunden.
std::optional<Restore> Rejoin(Ledger& L, uint64_t Key, uint64_t Inventory, uint64_t& T)
{
    std::optional<Restore> Result;
    const uint64_t Keys[] = {Key};
    for (uint64_t Waited = 0; Waited <= SettleMs + StableMs && !Result; Waited += 500)
    {
        L.Connected(Keys);
        Result = L.Tick(Key, Inventory, Start(), false, T += 500);
    }
    return Result;
}

void Disconnect(Ledger& L)
{
    L.Connected(std::span<const uint64_t>{});
}
}

TEST_CASE("wiedereintritt-inventar: Host gibt dem neuen PlayerState nach einer Trennung das gemerkte Inventar genau einmal zurück")
{
    Ledger L;
    uint64_t T = 1000;
    CHECK_FALSE(L.Tick(Alice, OldInventory, Start(), false, T));
    CHECK_FALSE(L.Tick(Alice, OldInventory, Played(), false, T += 500)); // erspieltes Inventar wird nachgeführt
    CHECK(L.Saved(Alice) == Played());
    Disconnect(L); // Trennung
    CHECK(L.HasLeft(Alice));
    CHECK(L.Saved(Alice) == Played());

    // Neuer PlayerState mit Startinventar: nicht sofort, erst wenn es SettleMs da und StableMs unverändert ist.
    CHECK_FALSE(L.Tick(Alice, NewInventory, Start(), false, T += 500));
    CHECK(L.Saved(Alice) == Played()); // das Startinventar überschreibt das Bild nicht
    const auto R = Rejoin(L, Alice, NewInventory, T);
    REQUIRE(R);
    CHECK(R->Inventory == NewInventory);
    CHECK(R->Saved == Played());
    L.Done(Alice, true);
    CHECK_FALSE(L.HasLeft(Alice));
    // Kein zweiter Restore für dasselbe Inventar, das Bild folgt ihm wieder.
    for (int i = 0; i < 40; ++i)
        CHECK_FALSE(L.Tick(Alice, NewInventory, Played(), false, T += 500));
    Snapshot Later = Played();
    Later.Items.pop_back();
    CHECK_FALSE(L.Tick(Alice, NewInventory, Later, false, T += 500));
    CHECK(L.Saved(Alice) == Later);
}

TEST_CASE("wiedereintritt-inventar: Reise, neuer Tag und kurzer Ausfall des Kontrollkanals geben nichts doppelt")
{
    Ledger L;
    uint64_t T = 0;
    L.Tick(Alice, OldInventory, Played(), false, T += 500);
    // Reise ohne Trennung: neuer PlayerState, das Spiel trägt das Inventar selbst hinüber.
    CHECK_FALSE(Rejoin(L, Alice, NewInventory, T));
    CHECK(L.Saved(Alice) == Start());
    // Kurzer Ausfall des Kontrollkanals, dasselbe Inventar lebt weiter.
    L.Tick(Alice, NewInventory, Played(), false, T += 500);
    Disconnect(L);
    CHECK(L.HasLeft(Alice));
    CHECK_FALSE(Rejoin(L, Alice, NewInventory, T));
    CHECK_FALSE(L.HasLeft(Alice));
}

TEST_CASE("wiedereintritt-inventar: Schlüssel ist die Steam-Identität, ein anderes Konto bekommt das Startinventar")
{
    Ledger L;
    uint64_t T = 0;
    L.Tick(Alice, OldInventory, Played(), false, T += 500);
    Disconnect(L);
    // Bob tritt bei: kein Restore, sein Inventar wird eigens gemerkt.
    CHECK_FALSE(Rejoin(L, Bob, NewInventory, T));
    CHECK(L.Saved(Bob) == Start());
    // Alice kommt nach Bob wieder, Bob ist nicht mehr verbunden.
    const auto R = Rejoin(L, Alice, ThirdInventory, T);
    REQUIRE(R);
    CHECK(R->Saved == Played());
    CHECK(L.HasLeft(Bob));
    // Ohne Peer gibt es nichts zu entscheiden.
    CHECK_FALSE(L.Tick(0, NewInventory, Start(), false, T += 500));
}

TEST_CASE("wiedereintritt-inventar: offene Auswahl verschiebt, Fehlschlag bucht nicht doppelt, Run-Ende verwirft")
{
    Ledger L;
    uint64_t T = 0;
    L.Tick(Alice, OldInventory, Played(), false, T += 500);
    Disconnect(L);
    // Während einer Airdrop-Auswahl oder im Lager kein Ersetzen.
    for (int i = 0; i < 40; ++i)
        CHECK_FALSE(L.Tick(Alice, NewInventory, Start(), true, T += 500));
    // Startinventar ändert sich noch (Startausrüstung kommt nach): StableMs ab der letzten Änderung.
    Snapshot Growing = Start();
    Growing.Items.push_back({"ItemData_Knife", "Rarity_0_Common", false, false, false, 0, {}, {}});
    CHECK_FALSE(L.Tick(Alice, NewInventory, Growing, false, T += 500));
    CHECK_FALSE(L.Tick(Alice, NewInventory, Growing, false, T += StableMs - 500));
    auto R = L.Tick(Alice, NewInventory, Growing, false, T += 500);
    REQUIRE(R);
    // Fehlgeschlagen: dieses Inventar nie wieder, Bild bleibt eingefroren für den nächsten Wiederbeitritt.
    L.Done(Alice, false);
    for (int i = 0; i < 40; ++i)
        CHECK_FALSE(L.Tick(Alice, NewInventory, Start(), false, T += 500));
    CHECK(L.Saved(Alice) == Played());
    Disconnect(L);
    R = Rejoin(L, Alice, ThirdInventory, T);
    REQUIRE(R);
    CHECK(R->Saved == Played());
    L.Done(Alice, true);

    // Run-Ende: nichts mehr zurückgeben.
    Disconnect(L);
    CHECK(L.Forget());
    CHECK_FALSE(L.Forget());
    CHECK_FALSE(Rejoin(L, Alice, NewInventory + 100, T));
    CHECK(L.Saved(Alice) == Start());
}

TEST_CASE("wiedereintritt-inventar: mehrere Clients, nur wer sich getrennt hat, bekommt sein Inventar zurück")
{
    Ledger L;
    uint64_t T = 0;
    const uint64_t Both[] = {Alice, Bob}, OnlyBob[] = {Bob};
    L.Connected(Both);
    L.Tick(Alice, OldInventory, Played(), false, T += 500);
    L.Tick(Bob, ThirdInventory, Start(), false, T += 500);
    L.Connected(OnlyBob); // Alice trennt sich, Bob spielt weiter
    CHECK(L.HasLeft(Alice));
    CHECK_FALSE(L.HasLeft(Bob));
    L.Tick(Bob, ThirdInventory, Played(), false, T += 500);
    CHECK(L.Saved(Bob) == Played());
    std::optional<Restore> R;
    for (uint64_t Waited = 0; Waited <= SettleMs + StableMs && !R; Waited += 500)
    {
        L.Connected(Both);
        CHECK_FALSE(L.Tick(Bob, ThirdInventory, Played(), false, T += 500));
        R = L.Tick(Alice, NewInventory, Start(), false, T);
    }
    REQUIRE(R);
    CHECK(R->Inventory == NewInventory);
    CHECK(R->Saved == Played());
    L.Done(Bob, false); // kein offener Restore für Bob, ohne Wirkung
    L.Done(Alice, true);
    CHECK_FALSE(L.HasLeft(Alice));
    CHECK_FALSE(L.HasLeft(Bob));
}

TEST_CASE("wiedereintritt-inventar: leeres Bild eines abgebauten Inventars überschreibt das volle nicht")
{
    Ledger L;
    uint64_t T = 0;
    L.Tick(Alice, OldInventory, Played(), false, T += 500);
    L.Tick(Alice, OldInventory, Snapshot{{}, Played().Slots}, false, T += 500);
    CHECK(L.Saved(Alice) == Played());
}

TEST_CASE("wiedereintritt-inventar: Rucksackfelder angleichen")
{
    const auto [Create, Destroy] = SlotChanges(Start().Slots, Played().Slots);
    CHECK(Create == std::vector<Tile>{{2, 0}});
    CHECK(Destroy.empty());
    const auto [Again, Remove] = SlotChanges({{0, 0}, {5, 5}}, {{0, 0}});
    CHECK(Again.empty());
    CHECK(Remove == std::vector<Tile>{{5, 5}});
}
