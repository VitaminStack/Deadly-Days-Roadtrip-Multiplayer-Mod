#include <windows.h>

#include <doctest.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>

#include "Core/Log.h"

using namespace Core;

TEST_CASE("Log legt Ordner an und filtert nach Stufe")
{
    const auto File = std::filesystem::temp_directory_path() / std::format("ddcoop-test-{}", GetCurrentProcessId()) / "unter" / "test.log";
    REQUIRE(Log::Open(File));

    Log::SetLevel(Log::Level::Info);
    Log::Info("sichtbar {}", 1);
    Log::Debug("versteckt");
    Log::SetLevel(Log::Level::Debug);
    Log::Debug("jetzt da");

    std::ostringstream Text;
    Text << std::ifstream(File).rdbuf();
    CHECK(Text.str().find("[INFO] sichtbar 1") != std::string::npos);
    CHECK(Text.str().find("versteckt") == std::string::npos);
    CHECK(Text.str().find("jetzt da") != std::string::npos);
    Log::SetLevel(Log::Level::Info);
}

TEST_CASE("Log kennt die Stufen aus der Config")
{
    CHECK(Log::ParseLevel("warn") == Log::Level::Warn);
    CHECK_FALSE(Log::ParseLevel("laut"));
}
