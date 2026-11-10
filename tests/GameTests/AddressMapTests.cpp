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
