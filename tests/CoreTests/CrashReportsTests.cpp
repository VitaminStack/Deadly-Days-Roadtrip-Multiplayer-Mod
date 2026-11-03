#include <doctest.h>

#include "Core/CrashReports.h"

using Core::CrashReports::IsCrashReporterLaunch;

TEST_CASE("CrashReports erkennt den Crash-Reporter am Programm")
{
    CHECK(IsCrashReporterLaunch(LR"(C:\Spiel\Engine\Binaries\Win64\CrashReportClient.exe)", L""));
    CHECK(IsCrashReporterLaunch(L"", LR"("C:\Spiel\Engine\Binaries\Win64\CRASHREPORTCLIENT.EXE" "C:\Saved\Crashes\x" -Unattended)"));
    CHECK(IsCrashReporterLaunch(L"", L"CrashReportClient.exe -Unattended"));
}

TEST_CASE("CrashReports lässt andere Programme durch")
{
    CHECK_FALSE(IsCrashReporterLaunch(L"", L""));
    CHECK_FALSE(IsCrashReporterLaunch(LR"(C:\Windows\explorer.exe)", L"explorer.exe https://example.com"));
    // Der Name als Argument eines anderen Programms zählt nicht.
    CHECK_FALSE(IsCrashReporterLaunch(L"", LR"("C:\Tools\notepad.exe" CrashReportClient.log)"));
}
