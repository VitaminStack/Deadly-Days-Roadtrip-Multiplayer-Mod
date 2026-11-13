#include <doctest.h>
#include <string>
#include <vector>
#include "Sync/Players/WeaponSoundState.h"

using namespace Sync::WeaponSounds;
using Game::ReloadSound;

TEST_CASE("Nachladesound: Anzahl, NetGUID und Name je Eintrag, verlustfrei")
{
    const std::vector<ReloadSound> Sounds = {{0x1234567890ABull, "weapon_CrossbowReload"}, {2, ""}};
    const auto Bytes = Encode(Sounds);
    REQUIRE(Bytes.size() == 1 + 9 + Sounds[0].Override.size() + 9);
    CHECK(Bytes[0] == 2);
    const auto Back = Decode(Bytes);
    REQUIRE(Back);
    CHECK(*Back == Sounds);
}

TEST_CASE("Nachladesound: Grenzen der Liste und des Namens")
{
    CHECK(Encode({}).empty());
    std::vector<ReloadSound> Full(Game::MaxReloadSounds, ReloadSound{7, std::string(Game::MaxReloadSoundName, 'a')});
    const auto Bytes = Encode(Full);
    REQUIRE_FALSE(Bytes.empty());
    CHECK(Decode(Bytes)->size() == Game::MaxReloadSounds);
    Full.push_back(Full.front());
    CHECK(Encode(Full).empty());
    CHECK(Encode(std::vector<ReloadSound>{{7, std::string(Game::MaxReloadSoundName + 1, 'a')}}).empty());
}

TEST_CASE("Nachladesound: ungültige Einträge und falsche Längen werden abgelehnt")
{
    CHECK(Encode(std::vector<ReloadSound>{{0, ""}}).empty());
    CHECK(Encode(std::vector<ReloadSound>{{MaxGuid + 1, ""}}).empty());
    CHECK(Encode(std::vector<ReloadSound>{{5, "mit leer"}}).empty());
    CHECK(Encode(std::vector<ReloadSound>{{5, std::string("a\0b", 3)}}).empty());
    CHECK(Encode(std::vector<ReloadSound>{{5, "\x80"}}).empty());

    const auto Good = Encode(std::vector<ReloadSound>{{5, "weapon_Reload"}});
    REQUIRE_FALSE(Good.empty());
    CHECK_FALSE(Decode({}));
    for (size_t Length = 0; Length < Good.size(); ++Length) // jede Kürzung
        CHECK_FALSE(Decode(std::span(Good).first(Length)));
    auto Longer = Good;
    Longer.push_back(0);
    CHECK_FALSE(Decode(Longer));
    auto ZeroCount = Good;
    ZeroCount[0] = 0;
    CHECK_FALSE(Decode(ZeroCount));
    auto TooMany = Good;
    TooMany[0] = static_cast<uint8_t>(Game::MaxReloadSounds + 1);
    CHECK_FALSE(Decode(TooMany));
    auto NoGuid = Good;
    for (size_t i = 1; i <= 8; ++i)
        NoGuid[i] = 0;
    CHECK_FALSE(Decode(NoGuid));
    auto BadLetter = Good;
    BadLetter.back() = ' ';
    CHECK_FALSE(Decode(BadLetter));
    auto LongName = Good;
    LongName[9] = static_cast<uint8_t>(Game::MaxReloadSoundName + 1);
    CHECK_FALSE(Decode(LongName));
}
