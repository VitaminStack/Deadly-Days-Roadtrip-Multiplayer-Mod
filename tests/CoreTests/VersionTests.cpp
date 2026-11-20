#include <windows.h>

#include <doctest.h>

#include "Core/Version.h"

TEST_CASE("Versionsprüfung kennt nur exakt passende Fingerabdrücke")
{
    CHECK(Core::FindSupportedBuild({0x260774D7, 0x0B750000}) == 26786272u); // Patch 1.0.3
    CHECK_FALSE(Core::FindSupportedBuild({0x260774D7, 0x0B750001}));
    CHECK_FALSE(Core::FindSupportedBuild({0x260774D8, 0x0B750000}));
    // Nur der neueste Build: 26495302 und 26669225 seit spiel-update-26786272 nicht mehr unterstützt.
    CHECK_FALSE(Core::FindSupportedBuild({0x65BFBBFB, 0x0B748000}));
    CHECK_FALSE(Core::FindSupportedBuild({0xE9CA3AB7, 0x0B74A000}));
    CHECK_FALSE(Core::FindSupportedBuild({0x32C34474, 0x0B742000})); // 26187885
    CHECK_FALSE(Core::FindSupportedBuild({0, 0}));
}

TEST_CASE("Fingerabdruck verlangt einen gültigen Header")
{
    CHECK_FALSE(Core::ReadFingerprint(nullptr));
    const char Junk[4096]{};
    CHECK_FALSE(Core::ReadFingerprint(Junk));

    const auto Self = Core::ReadFingerprint(GetModuleHandleW(nullptr));
    REQUIRE(Self);
    CHECK(Self->SizeOfImage > 0);
}

TEST_CASE("Das Testprogramm gilt nicht als Spiel")
{
    CHECK_FALSE(Core::DetectSupportedBuild());
}
