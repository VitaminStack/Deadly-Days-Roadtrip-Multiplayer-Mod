#include <doctest.h>

#include "Game/AddressMap.h"

using namespace Game::AddressMap;

TEST_CASE("Adressen: Referenz-Build bleibt unverändert, unbekannter Build liefert 0")
{
    CHECK(Map(ReferenceBuild, 0x0160C040) == 0x0160C040);
    CHECK(Known(ReferenceBuild));
    CHECK_FALSE(Known(1));
    CHECK(Map(1, 0x0160C040) == 0);
}

TEST_CASE("Adressen: nur der Referenz-Build ist bekannt, frühere Builds haben keine Tabelle")
{
    CHECK(ReferenceBuild == 26786272u);
    for (const uint32_t Old : {26495302u, 26669225u})
    {
        CHECK(Table(Old).empty());
        CHECK_FALSE(Known(Old));
        CHECK(Map(Old, 0x0160C040) == 0);
    }
    CHECK(Sorted(Table(ReferenceBuild)));
}
