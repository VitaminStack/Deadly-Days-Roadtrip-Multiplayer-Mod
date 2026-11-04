#include <doctest.h>

#include <cstdint>

#include "Core/SteamStats.h"

using Core::SteamStats::VtableOffsetFromThunk;

// Bytes wie in steam_api64.dll (Steamworks 1.64): mov rax,[rcx], dann der Sprung über die vtable.
TEST_CASE("SteamStats liest den vtable-Abstand aus kurzem Sprung")
{
    const uint8_t SetAchievement[] = {0x48, 0x8B, 0x01, 0x48, 0xFF, 0x60, 0x30, 0xCC};
    CHECK(VtableOffsetFromThunk(SetAchievement, sizeof(SetAchievement)) == 0x30u);
}

TEST_CASE("SteamStats liest den vtable-Abstand aus langem Sprung")
{
    const uint8_t AttachLeaderboardUgc[] = {0x48, 0x8B, 0x01, 0x48, 0xFF, 0xA0, 0xF8, 0x00, 0x00, 0x00, 0xCC};
    CHECK(VtableOffsetFromThunk(AttachLeaderboardUgc, sizeof(AttachLeaderboardUgc)) == 0xF8u);
}

TEST_CASE("SteamStats liest den vtable-Abstand über r10")
{
    const uint8_t UploadLeaderboardScore[] = {0x48, 0x8B, 0x01, 0x4C, 0x8B, 0x90, 0xF0, 0x00, 0x00, 0x00, 0x49, 0xFF, 0xE2, 0xCC};
    CHECK(VtableOffsetFromThunk(UploadLeaderboardScore, sizeof(UploadLeaderboardScore)) == 0xF0u);
}

TEST_CASE("SteamStats lehnt fremden Code ab")
{
    const uint8_t Accessor[] = {0x48, 0x83, 0xEC, 0x28, 0x48, 0x8D, 0x0D, 0x35};
    CHECK_FALSE(VtableOffsetFromThunk(Accessor, sizeof(Accessor)).has_value());
    const uint8_t Short[] = {0x48, 0x8B, 0x01, 0x48, 0xFF};
    CHECK_FALSE(VtableOffsetFromThunk(Short, sizeof(Short)).has_value());
    CHECK_FALSE(VtableOffsetFromThunk(nullptr, 16).has_value());
}
