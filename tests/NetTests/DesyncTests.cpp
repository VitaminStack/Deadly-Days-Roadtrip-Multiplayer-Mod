#include <doctest.h>
#include "Sync/DesyncState.h"
#include "Sync/ControlDispatch.h"

using namespace Sync::Desync;
namespace
{
Packet Example()
{
    Packet P{11, 12, 13, 14};
    P.Image.Known = P.Stable = All;
    P.Image.Values = {1, 0, 2, 123, 30, 456};
    return P;
}
Net::Protocol::Control Envelope(const Packet& P, uint64_t Revision = 1)
{
    return {70, 80, Revision, Result, Encode(P)};
}
}

TEST_CASE("Desync: festes Little-Endian-Format, exakte Laenge und semantische Grenzen")
{
    const auto P = Example();
    const auto Bytes = Encode(P);
    REQUIRE(Bytes.size() == 84);
    CHECK(Bytes[0] == 11);
    CHECK(Bytes[1] == 0);
    CHECK(Bytes[32] == All);
    auto Wide = P;
    Wide.ClientRun = 0x0807060504030201ull;
    const auto WideBytes = Encode(Wide);
    for (unsigned i = 0; i < 8; ++i) CHECK(WideBytes[i] == i + 1);
    REQUIRE(Decode(Bytes));
    CHECK(*Decode(Bytes) == P);
    for (size_t Size = 0; Size < Bytes.size(); ++Size) CHECK_FALSE(Decode(std::span(Bytes).first(Size)));
    auto Extra = Bytes; Extra.push_back(0);
    CHECK_FALSE(Decode(Extra));
    for (unsigned At : {32u, 33u, 34u, 35u})
    {
        auto Bad = Bytes; Bad[At] = 0x80;
        CHECK_FALSE(Decode(Bad));
    }
    auto Bad = P; Bad.StateGuid = 0; CHECK(Encode(Bad).empty());
    Bad = P; Bad.ClientRun = 0; CHECK(Encode(Bad).empty());
    Bad = P; Bad.Nonce = 0; CHECK(Encode(Bad).empty());
    Bad = P; Bad.Sequence = 0; CHECK(Encode(Bad).empty());
    Bad = P; Bad.StateGuid = 0x40000000000000ull; CHECK(Encode(Bad).empty());
    Bad = P; Bad.Image.Values[Game::Desync::Day] = 100001; CHECK(Encode(Bad).empty());
    Bad = P; Bad.Image.Values[Game::Desync::Phase] = 2; CHECK(Encode(Bad).empty());
    Bad = P; Bad.Image.Values[Game::Desync::Players] = 3; CHECK(Encode(Bad).empty());
    Bad = P; Bad.Image.Values[Game::Desync::Enemies] = 20001; CHECK(Encode(Bad).empty());
    Bad = P; Bad.Image.Known = 0; CHECK(Encode(Bad).empty());
    Bad = P; Bad.Ignored = Bad.Alerts = Bit(Game::Desync::Inventory); CHECK(Encode(Bad).empty());
}

TEST_CASE("Desync: unbekannte Felder sind keine leeren Inventare")
{
    auto P = Example();
    P.Image.Known &= static_cast<uint8_t>(~Bit(Game::Desync::Inventory));
    P.Stable = P.Image.Known;
    P.Image.Values[Game::Desync::Inventory] = 0;
    REQUIRE(Decode(Encode(P)));
    Monitor M;
    for (unsigned i = 0; i < 10; ++i)
    {
        const auto C = M.Compare(Example(), P, 0);
        CHECK((C.Comparable & Bit(Game::Desync::Inventory)) == 0);
        CHECK(C.Alerts == 0);
    }
}

TEST_CASE("Desync: geladene Komponenten sind nicht GUObjectArray-Garbage")
{
    CHECK(Game::Desync::LiveObjectFlags(0x00200000)); // InventoryComponent.LoadCompleted im Spiel
    CHECK(Game::Desync::LiveObjectFlags(0x10000000)); // EObjectFlags.HasExternalPackage
    for (uint32_t Flag : {0x10u, 0x20u, 0x8000u, 0x10000u, 0x40000000u})
        CHECK_FALSE(Game::Desync::LiveObjectFlags(Flag));
}

TEST_CASE("Desync: Zeitfenster stabilisiert pro Feld und verwirft Lade-/Aenderungsphasen")
{
    Stability S;
    auto P = Example();
    CHECK(S.Update(P.Image, 100) == 0);
    CHECK(S.Update(P.Image, 2099) == 0);
    CHECK(S.Update(P.Image, 2100) == All);
    ++P.Image.Values[Game::Desync::Inventory];
    CHECK(S.Update(P.Image, 2200) == (All & ~Bit(Game::Desync::Inventory)));
    CHECK(S.Update(P.Image, 4200) == All);
    CHECK(S.Update({}, 4300) == 0);
    CHECK(S.Update(P.Image, 4400) == 0);
}

TEST_CASE("Desync: zehn Minuten gleiche Bilder ohne Alarm, anschliessend absichtlicher Inventarunterschied")
{
    Monitor M;
    auto Host = Example(), Client = Host;
    for (uint64_t Ms = 0; Ms <= 600000; Ms += IntervalMs)
    {
        const auto C = M.Compare(Host, Client, Bit(Game::Desync::Enemies));
        CHECK(C.Alerts == 0);
        CHECK(C.Different == 0);
        CHECK(C.Comparable == (All & ~Bit(Game::Desync::Enemies)));
    }
    ++Host.Image.Values[Game::Desync::Inventory];
    CHECK(M.Compare(Host, Client, 0).Alerts == 0);
    CHECK(M.Compare(Host, Client, 0).Alerts == 0);
    CHECK(M.Compare(Host, Client, 0).Alerts == Bit(Game::Desync::Inventory));
    Client = Host;
    CHECK(M.Compare(Host, Client, 0).Alerts == 0);
}

TEST_CASE("Desync: kurze Replikationsabweichung, wechselnde Messwerte und unbekannte Werte bestaetigen keinen Alarm")
{
    Monitor M;
    auto Host = Example(), Client = Host;
    for (unsigned i = 0; i < 20; ++i)
    {
        ++Host.Image.Values[Game::Desync::Health];
        CHECK(M.Compare(Host, Client, 0).Alerts == 0);
    }
    Client = Host;
    CHECK(M.Compare(Host, Client, 0).Alerts == 0);
    ++Host.Image.Values[Game::Desync::Inventory];
    CHECK(M.Compare(Host, Client, 0).Alerts == 0);
    CHECK(M.Compare(Host, Client, 0).Alerts == 0);
    M.Invalidate(static_cast<uint8_t>(All & ~Bit(Game::Desync::Inventory)));
    CHECK(M.Compare(Host, Client, 0).Alerts == 0);
}

TEST_CASE("Desync: jede bekannte Ausnahme ist einzeln abschaltbar, andere Unterschiede bleiben sichtbar")
{
    for (unsigned i = 0; i < Count; ++i)
    {
        Monitor M;
        auto Host = Example(), Client = Host;
        Host.Image.Values[i] ^= 1;
        for (unsigned j = 0; j < 4; ++j) CHECK(M.Compare(Host, Client, Bit(i)).Alerts == 0);
        Comparison Last;
        for (unsigned j = 1; j < ConfirmationsFor(i); ++j) CHECK(M.Compare(Host, Client, 0).Alerts == 0);
        Last = M.Compare(Host, Client, 0);
        CHECK(Last.Alerts == Bit(i));
    }
}

TEST_CASE("Desync: kanonische Mengen ignorieren Reihenfolge, aber nicht Duplikate oder Positionen")
{
    using Game::Desync::Multiset;
    CHECK(Multiset({3, 1, 2}) == Multiset({2, 3, 1}));
    CHECK(Multiset({3, 1, 2}) != Multiset({3, 1, 2, 2}));
    CHECK(Multiset({}) != Multiset({0}));
    Game::Desync::Hash A, B, C;
    A.Text("a/b"); A.Number(123);
    B.Text("a"); B.Text("b"); B.Number(123);
    C.Text("a/b"); C.Number(124);
    CHECK(A.Value() != B.Value());
    CHECK(A.Value() != C.Value());
}

TEST_CASE("Desync: Antwortbindung verwirft fremde Tokens, Nonces, Welten, Duplikate und alte Revisionen")
{
    ReplyGate Gate;
    auto P = Example();
    Gate.Bind(99, P.ClientRun, P.Nonce, P.StateGuid);
    Gate.Sent(P.Sequence, 100);
    CHECK(Gate.Waiting(2101)); // langsame Antwort darf nicht durch die naechste Probe ersetzt werden
    CHECK_FALSE(Gate.Waiting(100 + FreshMs + 1));
    CHECK_FALSE(Gate.Accept(98, Envelope(P), P, 200));
    auto Bad = P; ++Bad.Nonce;
    CHECK_FALSE(Gate.Accept(99, Envelope(Bad, 999), Bad, 200));
    Bad = P; ++Bad.StateGuid;
    CHECK_FALSE(Gate.Accept(99, Envelope(Bad, 999), Bad, 200));
    auto E = Envelope(P); E.Type = Probe;
    CHECK_FALSE(Gate.Accept(99, E, P, 200));
    CHECK(Gate.Accept(99, Envelope(P), P, 200));
    CHECK_FALSE(Gate.Waiting(200));
    CHECK_FALSE(Gate.Accept(99, Envelope(P), P, 200));
    ++P.Sequence; Gate.Sent(P.Sequence, 300);
    CHECK_FALSE(Gate.Accept(99, Envelope(P), P, 400));
    CHECK(Gate.Accept(99, Envelope(P, 2), P, 400));
    ++P.Sequence; Gate.Sent(P.Sequence, 500);
    E = Envelope(P, 3); ++E.Run;
    CHECK_FALSE(Gate.Accept(99, E, P, 600));
    E = Envelope(P, 3); --E.Epoch;
    CHECK_FALSE(Gate.Accept(99, E, P, 600));
    CHECK(Gate.Accept(99, Envelope(P, 3), P, 600));
}

TEST_CASE("Desync: nach Travel und Reconnect keine spaete gruene Anzeige oder alter Alarm")
{
    ReplyGate Gate;
    const auto Old = Example();
    Gate.Bind(99, Old.ClientRun, Old.Nonce, Old.StateGuid);
    Gate.Sent(Old.Sequence, 100);
    auto New = Old; ++New.Nonce; ++New.StateGuid;
    Gate.Bind(99, New.ClientRun, New.Nonce, New.StateGuid);
    Gate.Sent(New.Sequence, 200);
    CHECK_FALSE(Gate.Accept(99, Envelope(Old, 99), Old, 300));
    CHECK(Gate.Accept(99, Envelope(New), New, 300));
    Gate.Bind(100, New.ClientRun, New.Nonce, New.StateGuid);
    Gate.Sent(New.Sequence, 400);
    CHECK_FALSE(Gate.Accept(99, Envelope(New, 1000), New, 500));
    CHECK_FALSE(Gate.Accept(100, Envelope(New), New, 400 + FreshMs + 1));
    CHECK_FALSE(Gate.Accept(100, Envelope(New), New, 399));
    CHECK(Gate.Accept(100, Envelope(New), New, 500));
}

TEST_CASE("Kontrollkanal: Diagnose und Progress erhalten dieselben Nachrichten, kein zweiter Queue-Drain")
{
    const std::vector<Net::ControlMessage> Messages{{99, {1, 1, 1, 0x101, {}}}, {99, {1, 1, 2, Probe, {}}}};
    unsigned Progress = 0, Desync = 0;
    Sync::DispatchControls(Messages, [&](auto In) { REQUIRE(In.size() == 2); Progress += In[0].Value.Type == 0x101; },
        [&](auto In) { REQUIRE(In.size() == 2); Desync += In[1].Value.Type == Probe; });
    CHECK(Progress == 1);
    CHECK(Desync == 1);
}

TEST_CASE("Desync: Gegnerzahl ist standardmaessig im Abgleich, Wellen bestaetigen keinen Alarm (gegnertyp)")
{
    CHECK(DefaultIgnored == 0);
    CHECK(ConfirmationsFor(Game::Desync::Enemies) == EnemyConfirmations);
    CHECK(ConfirmationsFor(Game::Desync::Inventory) == Confirmations);
    CHECK((DefaultIgnored & Bit(Game::Desync::Enemies)) == 0);
    // Wie im Spiel: Jede Seite misst je Sekunde, ein Feld gilt erst nach zwei Sekunden ohne Aenderung als stabil.
    // Waehrend einer Welle aendert sich die Zahl auf beiden Seiten laufend, der Client sieht sie etwas spaeter.
    Stability HostStability, ClientStability;
    Monitor M;
    auto Host = Example(), Client = Host;
    uint64_t Now = 0;
    const auto Step = [&](uint64_t HostEnemies, uint64_t ClientEnemies)
    {
        Host.Image.Values[Game::Desync::Enemies] = HostEnemies;
        Client.Image.Values[Game::Desync::Enemies] = ClientEnemies;
        Now += 1000;
        Host.Stable = HostStability.Update(Host.Image, Now);
        Client.Stable = ClientStability.Update(Client.Image, Now);
        return M.Compare(Host, Client, DefaultIgnored);
    };
    for (unsigned i = 0; i < 4; ++i) Step(20, 20);
    CHECK(Step(20, 20).Comparable == All);
    for (uint64_t Enemies = 20; Enemies < 45; ++Enemies)
    {
        const auto C = Step(Enemies + 1, Enemies);
        CHECK(C.Alerts == 0);
        CHECK((C.Comparable & Bit(Game::Desync::Enemies)) == 0);
    }
    Step(45, 45); Step(45, 45);
    const auto Settled = Step(45, 45);
    CHECK(Settled.Comparable == All);
    CHECK(Settled.Different == 0);
    CHECK(Settled.Alerts == 0);
    // Ein spaet replizierter Gegner (im Spiel bis rund 8 s) loest noch keinen Alarm aus.
    for (unsigned i = 0; i < 8; ++i) CHECK(Step(45, 44).Alerts == 0);
    CHECK(Step(45, 45).Alerts == 0);
    Step(45, 45); Step(45, 45);
    // Ein bleibender Unterschied (etwa ein Gegner nur beim Client) wird nach EnemyConfirmations stabilen Proben gemeldet.
    for (unsigned i = 0; i < 2; ++i) Step(45, 46);
    for (unsigned i = 1; i < EnemyConfirmations; ++i) CHECK(Step(45, 46).Alerts == 0);
    CHECK(Step(45, 46).Alerts == Bit(Game::Desync::Enemies));
    CHECK(Step(45, 45).Alerts == 0);
}
