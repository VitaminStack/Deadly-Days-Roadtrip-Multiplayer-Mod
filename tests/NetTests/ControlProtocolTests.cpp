#include <doctest.h>

#include <string>

#include "Net/ControlProtocol.h"

using namespace Net::Control;

namespace
{
std::string ErrorOf(std::string_view Line)
{
    std::string Error;
    CHECK_FALSE(ParseCommand(Line, &Error).has_value());
    return Error;
}
}

TEST_CASE("Testkanal: Befehle werden zerlegt")
{
    CHECK(ParseCommand("state")->Kind == CommandKind::State);
    CHECK(ParseCommand("  state \r\n")->Kind == CommandKind::State);
    CHECK(ParseCommand("day")->Kind == CommandKind::Day);
    CHECK_FALSE(ParseCommand("day 1"));
    CHECK(ParseCommand("lobby create")->Kind == CommandKind::LobbyCreate);
    CHECK(ParseCommand("lobby invite")->Kind == CommandKind::LobbyInvite);
    CHECK(ParseCommand("lobby leave")->Kind == CommandKind::LobbyLeave);

    const auto Join = ParseCommand("lobby join 109775241234567890");
    REQUIRE(Join);
    CHECK(Join->Kind == CommandKind::LobbyJoin);
    CHECK(Join->Lobby == 109775241234567890ull);

    const auto Console = ParseCommand("console open /Game/Maps/MetaBase?listen");
    REQUIRE(Console);
    CHECK(Console->Kind == CommandKind::Console);
    CHECK(Console->Text == "open /Game/Maps/MetaBase?listen");

    const auto Travel = ParseCommand("travel /Game/Maps/DDDynamic_Map");
    REQUIRE(Travel);
    CHECK(Travel->Kind == CommandKind::Travel);
    CHECK(Travel->Text == "/Game/Maps/DDDynamic_Map");
}

TEST_CASE("Testkanal: ungültige Anfragen werden abgelehnt")
{
    CHECK(ErrorOf("") == "leere Anfrage");
    CHECK(ErrorOf("   ") == "leere Anfrage");
    CHECK(ErrorOf("dance") == "unbekannter Befehl");
    CHECK(ErrorOf("state now") == "state hat keine Argumente");
    CHECK(ErrorOf("console") == "console braucht einen Befehl");
    CHECK(ErrorOf("lobby") == "lobby kennt create, join, invite und leave");
    CHECK(ErrorOf("lobby create now") == "lobby create, invite und leave haben keine Argumente");
    CHECK(ErrorOf("lobby join") == "lobby join braucht eine Lobby-ID");
    CHECK(ErrorOf("lobby join abc") == "lobby join braucht eine Lobby-ID");
    CHECK(ErrorOf("lobby join 0") == "lobby join braucht eine Lobby-ID");
    CHECK(ErrorOf("lobby join 12 34") == "lobby join braucht eine Lobby-ID");
    CHECK(ErrorOf("lobby join 99999999999999999999999") == "lobby join braucht eine Lobby-ID");
    CHECK(ErrorOf(std::string("console ") + std::string(kMaxLine, 'a')) == "Anfrage zu lang");
    CHECK(ErrorOf(std::string("console a\x01") + "b") == "Anfrage enthält Steuerzeichen oder Nicht-ASCII");
    CHECK(ErrorOf("console \xC3\xA4") == "Anfrage enthält Steuerzeichen oder Nicht-ASCII");
}

TEST_CASE("Testkanal: travel nimmt nur Kartennamen")
{
    CHECK(IsValidMapName("MetaBase"));
    CHECK(IsValidMapName("/Game/Maps/DDDynamic_Map"));
    CHECK_FALSE(IsValidMapName(""));
    CHECK_FALSE(IsValidMapName("Map?listen"));
    CHECK_FALSE(IsValidMapName("Map option"));
    CHECK_FALSE(IsValidMapName("../Map"));
    CHECK_FALSE(IsValidMapName("Map;quit"));
    CHECK_FALSE(IsValidMapName(std::string(201, 'a')));
    CHECK(ErrorOf("travel Map?listen&x=1") == "travel braucht einen gültigen Kartennamen");
    CHECK(TravelConsoleCommand("/Game/Maps/DDDynamic_Map") == "servertravel /Game/Maps/DDDynamic_Map?listen");
}

TEST_CASE("Testkanal: Antworten sind gültiges JSON")
{
    CHECK(OkReply() == "{\"ok\":true}");
    CHECK(OkReply("\"world\":\"MetaBase\"") == "{\"ok\":true,\"world\":\"MetaBase\"}");
    CHECK(ErrorReply("kaputt") == "{\"ok\":false,\"error\":\"kaputt\"}");
    CHECK(JsonString("a\"b\\c\nd") == "\"a\\\"b\\\\c\\nd\"");
    CHECK(JsonString(std::string("x\x01")) == "\"x\\u0001\"");
}
