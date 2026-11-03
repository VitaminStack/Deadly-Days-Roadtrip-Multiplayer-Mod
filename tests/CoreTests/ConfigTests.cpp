#include <doctest.h>

#include "Core/Config.h"

using Core::Config;

TEST_CASE("Config liest Werte und ignoriert Kommentare")
{
    const auto Parsed = Config::Parse("# Kommentar\n; Kommentar\n name = wert \r\nohne Trenner\n=leer\n");
    CHECK(Parsed.GetString("name", "x") == "wert");
    CHECK(Parsed.GetString("ohne Trenner", "x") == "x");
    CHECK(Parsed.GetString("# Kommentar", "x") == "x");
}

TEST_CASE("Config fällt bei unlesbaren Werten auf den Standard zurück")
{
    const auto Parsed = Config::Parse("an=true\naus=0\nbool=vielleicht\nzahl=42\nminus=-7\nkaputt=4x\n");
    CHECK(Parsed.GetBool("an", false));
    CHECK_FALSE(Parsed.GetBool("aus", true));
    CHECK(Parsed.GetBool("bool", true));
    CHECK(Parsed.GetInt("zahl", 0) == 42);
    CHECK(Parsed.GetInt("minus", 0) == -7);
    CHECK(Parsed.GetInt("kaputt", 5) == 5);
    CHECK(Parsed.GetInt("fehlt", 3) == 3);
}

TEST_CASE("Config ohne Datei liefert Standardwerte")
{
    CHECK(Config::Load("gibt-es-nicht.ini").GetBool("enabled", true));
}
