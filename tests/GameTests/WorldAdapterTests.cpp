#include <doctest.h>

#include <cstdint>

#include "Game/Memory.h"
#include "Game/WorldAdapter.h"

TEST_CASE("Role follows NetDriver and server connection")
{
    CHECK(Game::RoleFrom(false, false) == Game::NetRole::Standalone);
    CHECK(Game::RoleFrom(true, false) == Game::NetRole::Host);
    CHECK(Game::RoleFrom(true, true) == Game::NetRole::Client);
}

TEST_CASE("World description is readable in the log")
{
    CHECK(Game::Describe({ "DDDynamic_Map", Game::NetRole::Host, 2 }) == "DDDynamic_Map, Host, 2 Spieler");
    CHECK(Game::Describe({ "", Game::NetRole::Standalone, 0 }) == "?, allein, 0 Spieler");
}

TEST_CASE("Guarded reads survive bad addresses")
{
    const uint64_t Value = 0x1122334455667788ull;
    CHECK(Game::Memory::Read<uint64_t>(reinterpret_cast<uintptr_t>(&Value)) == Value);
    CHECK_FALSE(Game::Memory::Read<uint64_t>(0).has_value());
    CHECK_FALSE(Game::Memory::Read<uint64_t>(0x10).has_value());
    CHECK_FALSE(Game::Memory::ReadPointer(0x10).has_value());
    const uintptr_t Null = 0;
    CHECK_FALSE(Game::Memory::ReadPointer(reinterpret_cast<uintptr_t>(&Null)).has_value());
}

TEST_CASE("ReadWorld refuses to run outside the game thread")
{
    // Im Testprozess hat noch kein Drain auf diesem Thread stattgefunden.
    CHECK_FALSE(Game::ReadWorld().has_value());
}
