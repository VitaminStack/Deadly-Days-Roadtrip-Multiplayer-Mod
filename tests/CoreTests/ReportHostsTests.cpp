#include <doctest.h>

#include "Core/ReportHosts.h"

using Core::ReportHosts::HostFromUrl;
using Core::ReportHosts::IsBlocked;

TEST_CASE("ReportHosts sperrt die Berichtsziele des Spiels samt Subdomains")
{
    CHECK(IsBlocked(L"api.codecks.io"));
    CHECK(IsBlocked(L"API.Codecks.IO"));
    CHECK(IsBlocked(L"codecks.io"));
    CHECK(IsBlocked(L"pixelsplit.games"));
    CHECK(IsBlocked(L"pixelsplit.games."));
    CHECK(IsBlocked(L"deadlydays.layerapps.com"));
    CHECK(IsBlocked(L"DeadlyDays.LayerApps.com"));
}

TEST_CASE("ReportHosts sperrt bei layerapps nur den Dienst des Spiels")
{
    CHECK_FALSE(IsBlocked(L"layerapps.com"));
    CHECK_FALSE(IsBlocked(L"other.layerapps.com"));
}

TEST_CASE("ReportHosts lässt andere Ziele durch")
{
    CHECK_FALSE(IsBlocked(L""));
    CHECK_FALSE(IsBlocked(L"steampowered.com"));
    CHECK_FALSE(IsBlocked(L"ddrweekly.deadlydays.com"));
    // Nur ganze Namensteile zählen.
    CHECK_FALSE(IsBlocked(L"notcodecks.io"));
    CHECK_FALSE(IsBlocked(L"codecks.io.example.com"));
}

TEST_CASE("ReportHosts sperrt zusätzliche Hosts")
{
    CHECK_FALSE(IsBlocked(L"telemetry.example.test"));
    Core::ReportHosts::Add(L"Telemetry.Example.Test");
    CHECK(IsBlocked(L"telemetry.example.test"));
    CHECK(IsBlocked(L"eu.telemetry.example.test"));
}

TEST_CASE("ReportHosts liest den Host aus einer URL")
{
    CHECK(HostFromUrl(L"https://pixelsplit.games/gamelytics/ddr/save_analytics.php") == L"pixelsplit.games");
    CHECK(HostFromUrl(L"https://api.codecks.io/user-report/v1/create-report?token=") == L"api.codecks.io");
    CHECK(HostFromUrl(L"http://User@Example.Test:8080/x") == L"example.test");
    CHECK(HostFromUrl(L"example.test") == L"example.test");
    CHECK(HostFromUrl(L"") == L"");
}
