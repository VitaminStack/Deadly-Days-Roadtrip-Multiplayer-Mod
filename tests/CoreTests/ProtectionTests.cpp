#include <doctest.h>

#include "Core/Hooks.h"
#include "Core/Protection.h"
#include "Core/ReportHosts.h"

using Core::Protection::InstallAll;
using Core::Protection::Step;

namespace
{
int Calls = 0;
bool Succeeds()
{
    ++Calls;
    return true;
}
bool Fails()
{
    ++Calls;
    return false;
}
}

TEST_CASE("Schutz: alle Schritte erfolgreich")
{
    Calls = 0;
    const Step Steps[] = {{"A", &Succeeds}, {"B", &Succeeds}};
    CHECK(InstallAll(Steps));
    CHECK(Calls == 2);
}

TEST_CASE("Schutz: ein fehlgeschlagener Schritt verhindert den Start")
{
    Calls = 0;
    const Step Steps[] = {{"A", &Succeeds}, {"B", &Fails}, {"C", &Succeeds}};
    CHECK_FALSE(InstallAll(Steps));
    // Nach dem Fehler läuft nichts mehr.
    CHECK(Calls == 2);

    const Step Missing[] = {{"ohne Funktion", nullptr}};
    CHECK_FALSE(InstallAll(Missing));
}

TEST_CASE("Schutz: ohne Hooks scheitern die echten Sperren und die Mod startet nicht")
{
    // Hooks::Initialize fehlt, also kann kein Hook installiert werden.
    Core::Hooks::Shutdown();
    CHECK_FALSE(Core::ReportHosts::Block());
    CHECK_FALSE(Core::Protection::Install());
}
