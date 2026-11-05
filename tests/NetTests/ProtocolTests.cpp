#include <doctest.h>
#include "Net/Protocol.h"

using namespace Net::Protocol;

TEST_CASE("Protokoll trennt binäre UE-Pakete und Kontrollrahmen eindeutig")
{
    const std::vector<uint8_t> Payload{0, 255, 'D', 'D', 'C', 'P', 0};
    const auto Bytes = Encode(Kind::Game, Payload);
    const auto Frame = Decode(Bytes);
    REQUIRE(Frame);
    CHECK(Frame->Type == Kind::Game);
    CHECK(std::vector<uint8_t>(Frame->Payload.begin(), Frame->Payload.end()) == Payload);
    CHECK_FALSE(Decode(Payload)); // Kein stiller Fallback auf alte ungepruefte Tunnel.
}

TEST_CASE("Protokoll lehnt alle abgeschnittenen und unpassenden Rahmen ab")
{
    const auto Valid = Hello({GameBuild, ModVersion, true});
    for (size_t Size = 0; Size < Valid.size(); ++Size)
        CHECK_FALSE(Decode(std::span(Valid).first(Size)));
    for (size_t At : {size_t(0), size_t(4), size_t(6), size_t(7), size_t(8)})
    {
        auto Bad = Valid;
        Bad[At] = 255;
        CHECK_FALSE(Decode(Bad));
    }
    auto Extra = Valid;
    Extra.push_back(0);
    CHECK_FALSE(Decode(Extra));
    CHECK(Encode(Kind::Control, std::vector<uint8_t>(MaxPayload)).size() == MaxPayload + 12);
    CHECK(Encode(Kind::Control, std::vector<uint8_t>(MaxPayload + 1)).empty());
}

TEST_CASE("Handshake fordert gleiche Builds und Mod-Versionen und entgegengesetzte Rollen")
{
    const Identity Local{GameBuild, ModVersion, false};
    const auto HelloBytes = Hello({GameBuild, ModVersion, true});
    const auto Frame = Decode(HelloBytes);
    REQUIRE(Frame);
    CHECK(Matches(Frame->Payload, Local));
    CHECK_FALSE(Matches(Frame->Payload, {GameBuild + 1, ModVersion, false}));
    CHECK_FALSE(Matches(Frame->Payload, {GameBuild, ModVersion + 1, false}));
    CHECK_FALSE(Matches(Frame->Payload, {GameBuild, ModVersion, true}));
    CHECK_FALSE(Matches(Frame->Payload.first(8), Local));
}

TEST_CASE("Kontrollzustand hat feste Identitaet und Grenzen, keine rohen Prozessdaten")
{
    const Control Value{0x0102030405060708, 3, 4, 5, {0, 1, 255}};
    const auto Bytes = EncodeControl(Value);
    const auto Frame = Decode(Bytes);
    REQUIRE(Frame);
    CHECK(Frame->Payload[0] == 8); // LE, unabhaengig vom C++-Strukturlayout.
    CHECK(Frame->Payload[7] == 1);
    REQUIRE(DecodeControl(Frame->Payload));
    CHECK(*DecodeControl(Frame->Payload) == Value);
    for (size_t Size = 0; Size < 26; ++Size)
        CHECK_FALSE(DecodeControl(Frame->Payload.first(Size)));
    for (auto Bad : {Control{0, 1, 1, 1}, Control{1, 0, 1, 1}, Control{1, 1, 0, 1}, Control{1, 1, 1, 0}})
        CHECK(EncodeControl(Bad).empty());
    auto Large = Value;
    Large.Payload.resize(MaxPayload - 26);
    CHECK_FALSE(EncodeControl(Large).empty());
    Large.Payload.push_back(0);
    CHECK(EncodeControl(Large).empty());
}
