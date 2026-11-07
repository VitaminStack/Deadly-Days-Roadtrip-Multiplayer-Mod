#include <doctest.h>

#include "Game/SavePaths.h"

using namespace Game::SavePaths;

namespace
{
std::vector<Root> TestRoots()
{
    std::vector<Root> Roots;
    AddRoot(Roots, RootForSaved(L"C:\\Test\\ud_host\\Saved"));
    AddRoot(Roots, RootForSaved(L"C:/Users/Someone/AppData/Local/DDSurvivors/Saved/"));
    AddRoot(Roots, RootForSaved(L"c:\\test\\UD_HOST\\saved")); // doppelt, Groß/klein egal
    return Roots;
}
}

TEST_CASE("Save roots map SaveGames to the mod folder next to it")
{
    const auto Roots = TestRoots();
    REQUIRE(Roots.size() == 2);
    CHECK(Roots[0].Normal == L"C:\\Test\\ud_host\\Saved\\SaveGames");
    CHECK(Roots[0].Mod == L"C:\\Test\\ud_host\\Saved\\SaveGames_DDCoop");
    CHECK(Roots[1].Normal == L"C:\\Users\\Someone\\AppData\\Local\\DDSurvivors\\Saved\\SaveGames");
}

TEST_CASE("Every save file and folder is redirected")
{
    const auto Roots = TestRoots();
    CHECK(Redirect(L"C:\\Test\\ud_host\\Saved\\SaveGames\\SaveSlot_DDR_0.sav", Roots) ==
          L"C:\\Test\\ud_host\\Saved\\SaveGames_DDCoop\\SaveSlot_DDR_0.sav");
    CHECK(Redirect(L"C:/Test/ud_host/Saved/SaveGames/Management_SaveSlot_0.sav", Roots) ==
          L"C:\\Test\\ud_host\\Saved\\SaveGames_DDCoop\\Management_SaveSlot_0.sav");
    CHECK(Redirect(L"c:\\test\\ud_host\\saved\\savegames\\Backups\\SaveSlot_DDR_0_bak.sav", Roots) ==
          L"C:\\Test\\ud_host\\Saved\\SaveGames_DDCoop\\Backups\\SaveSlot_DDR_0_bak.sav");
    CHECK(Redirect(L"C:\\Test\\ud_host\\Saved\\SaveGames", Roots) == L"C:\\Test\\ud_host\\Saved\\SaveGames_DDCoop");
    CHECK(Redirect(L"C:\\Test\\ud_host\\Saved\\SaveGames\\", Roots) == L"C:\\Test\\ud_host\\Saved\\SaveGames_DDCoop");
    CHECK(Redirect(L"C:\\Test\\ud_host\\Saved\\SaveGames\\*.sav", Roots) == L"C:\\Test\\ud_host\\Saved\\SaveGames_DDCoop\\*.sav");
    CHECK(Redirect(L"C:\\Users\\Someone\\AppData\\Local\\DDSurvivors\\Saved\\SaveGames\\x__y__z.sav", Roots) ==
          L"C:\\Users\\Someone\\AppData\\Local\\DDSurvivors\\Saved\\SaveGames_DDCoop\\x__y__z.sav");
}

TEST_CASE("Long path prefix is kept")
{
    const auto Roots = TestRoots();
    CHECK(Redirect(L"\\\\?\\C:\\Test\\ud_host\\Saved\\SaveGames\\SaveSlot_DDR_0.sav", Roots) ==
          L"\\\\?\\C:\\Test\\ud_host\\Saved\\SaveGames_DDCoop\\SaveSlot_DDR_0.sav");
    CHECK_FALSE(Redirect(L"\\\\?\\UNC\\server\\share\\Saved\\SaveGames\\a.sav", Roots));
}

TEST_CASE("Other paths and the mod folder stay unchanged")
{
    const auto Roots = TestRoots();
    CHECK_FALSE(Redirect(L"C:\\Test\\ud_host\\Saved\\SaveGames_DDCoop\\SaveSlot_DDR_0.sav", Roots));
    CHECK_FALSE(Redirect(L"C:\\Test\\ud_host\\Saved\\SaveGamesOld\\a.sav", Roots));
    CHECK_FALSE(Redirect(L"C:\\Test\\ud_host\\Saved\\Config\\Windows\\GameUserSettings.ini", Roots));
    CHECK_FALSE(Redirect(L"C:\\Test\\ud_host\\Saved", Roots));
    CHECK_FALSE(Redirect(L"C:\\Test\\ud_client\\Saved\\SaveGames\\SaveSlot_DDR_0.sav", Roots));
    CHECK_FALSE(Redirect(L"D:\\Elsewhere\\SaveGames\\a.sav", Roots));
}

TEST_CASE("Redirect is idempotent")
{
    const auto Roots = TestRoots();
    const auto Once = Redirect(L"C:\\Test\\ud_host\\Saved\\SaveGames\\SaveSlot_DDR_0.sav", Roots);
    REQUIRE(Once);
    CHECK_FALSE(Redirect(*Once, Roots));
}

TEST_CASE("Cheap filter only passes paths naming SaveGames")
{
    CHECK(MayConcernSaves(L"C:\\x\\Saved\\SaveGames\\a.sav"));
    CHECK(MayConcernSaves(L"..\\..\\savegames"));
    CHECK_FALSE(MayConcernSaves(L"C:\\x\\Content\\Paks\\pakchunk0.pak"));
    CHECK_FALSE(MayConcernSaves(nullptr));
}

TEST_CASE("UserDir is parsed like the engine")
{
    CHECK(ParseUserDir(L"\"game.exe\" -windowed -UserDir=\"C:\\a b\\ud_host\" -nosplash") == L"C:\\a b\\ud_host");
    CHECK(ParseUserDir(L"game.exe -userdir=C:\\ud -x") == L"C:\\ud");
    CHECK(ParseUserDir(L"game.exe \"-UserDir=C:\\ud\"") == L"C:\\ud");
    CHECK_FALSE(ParseUserDir(L"game.exe -windowed"));
    CHECK_FALSE(ParseUserDir(L"game.exe -UserDir="));
    CHECK_FALSE(ParseUserDir(L"game.exe -NotUserDir=C:\\x"));
}

TEST_CASE("Foreign save warning and seeding filter")
{
    CHECK(LooksLikeForeignSave(L"D:\\Other\\Saved\\SaveGames\\a.sav"));
    CHECK_FALSE(LooksLikeForeignSave(L"D:\\Other\\Saved\\SaveGames\\a.txt"));
    CHECK(ShouldSeedFile(L"SaveSlot_DDR_0.sav"));
    CHECK(ShouldSeedFile(L"Management_SaveSlot_0.SAV"));
    CHECK_FALSE(ShouldSeedFile(L"steam_autocloud.vdf"));
    CHECK(FileName(L"C:\\x\\SaveGames\\a.sav") == L"a.sav");
}
