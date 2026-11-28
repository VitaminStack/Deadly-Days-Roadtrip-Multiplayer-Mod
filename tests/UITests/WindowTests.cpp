#include <doctest.h>
#include <windows.h>
#include <vector>

#include "UI/UI.h"
#include "Net/Protocol.h"

namespace
{
BOOL CALLBACK FindOverlay(HWND Window, LPARAM Result)
{
    DWORD Process = 0;
    GetWindowThreadProcessId(Window, &Process);
    wchar_t Class[64]{};
    GetClassNameW(Window, Class, 64);
    if (Process == GetCurrentProcessId() && std::wstring(Class) == L"DDCoop.Overlay")
        *reinterpret_cast<HWND*>(Result) = Window;
    return TRUE;
}

BOOL CALLBACK FindNotice(HWND Window, LPARAM Result)
{
    DWORD Process = 0;
    GetWindowThreadProcessId(Window, &Process);
    wchar_t Class[64]{};
    GetClassNameW(Window, Class, 64);
    if (Process == GetCurrentProcessId() && std::wstring(Class) == L"DDCoop.Notice")
        *reinterpret_cast<HWND*>(Result) = Window;
    return TRUE;
}

HWND WaitForOverlay()
{
    for (int i = 0; i < 100; ++i)
    {
        HWND Result = nullptr;
        EnumWindows(FindOverlay, reinterpret_cast<LPARAM>(&Result));
        // WM_CREATE must have completed too.
        if (Result && GetDlgItem(Result, 102))
            return Result;
        Sleep(20);
    }
    return nullptr;
}

struct Shutdown
{
    ~Shutdown() { UI::Stop(); }
};
BOOL CALLBACK ReadStaticText(HWND Window, LPARAM Result)
{
    wchar_t Class[32]{}, Text[1024]{};
    GetClassNameW(Window, Class, 32);
    if (std::wstring(Class) == L"Static")
    {
        GetWindowTextW(Window, Text, 1024);
        reinterpret_cast<std::vector<std::wstring>*>(Result)->emplace_back(Text);
    }
    return TRUE;
}
}

TEST_CASE("Native overlay lifecycle works without game SDK, Steam, focus changes or renderer hooks")
{
    Shutdown Cleanup;
    UI::Start();
    HWND Window = WaitForOverlay();
    REQUIRE(Window != nullptr);
    // Die Fensterbeschriftung bleibt sichtbar, auch wenn später ein anderer Tab offen ist.
    wchar_t Title[128]{};
    GetWindowTextW(Window, Title, 128);
    CHECK(std::wstring(Title) == L"DDCoop v" + std::to_wstring(Net::Protocol::ModVersion) + L" - Status (F8)");
    UI::Start(); // Idempotent.
    CHECK(WaitForOverlay() == Window);
    CHECK_FALSE(IsWindowVisible(Window));
    for (int Id : { 101, 102, 103, 104 })
        CHECK_FALSE(IsWindowEnabled(GetDlgItem(Window, Id)));
    // A synthetic click cannot bypass missing backend/capabilities even on a disabled button.
    SendMessageW(Window, WM_COMMAND, MAKEWPARAM(101, BN_CLICKED), 0);
    CHECK(Core::GameThread().Drain() == 0);

    // Only a test backend: exercise real edit/button -> model -> dispatcher routing while hidden.
    UI::OverlayStatus Status;
    Status.State = UI::SessionState::Offline;
    Status.CanCreate = Status.CanJoin = true;
    Status.Message = "Lobby test message";
    UI::Overlay().PublishDiagnostics("Desync: test diagnostics");
    int Calls = 0;
    uint64_t Target = 0;
    UI::Overlay().Bind([&](UI::LobbyRequest Request)
    {
        ++Calls;
        Target = Request.LobbyId;
        Status.State = UI::SessionState::Connecting;
        UI::Overlay().Publish(Status);
    });
    UI::Overlay().Publish(Status);
    SendMessageW(Window, WM_TIMER, 1, 0);
    std::vector<std::wstring> Texts;
    EnumChildWindows(Window, ReadStaticText, reinterpret_cast<LPARAM>(&Texts));
    REQUIRE(Texts.size() == 5);
    const std::wstring ExpectedPrefix = L"DDCoop v" + std::to_wstring(Net::Protocol::ModVersion) + L" | ";
    CHECK(Texts[0] == ExpectedPrefix + L"Offline");
    // Auch bei Missionsstart und danach in der MetaBase bleibt die Versionsquelle identisch.
    Status.State = UI::SessionState::Playing;
    UI::Overlay().Publish(Status);
    SendMessageW(Window, WM_TIMER, 1, 0);
    Texts.clear();
    EnumChildWindows(Window, ReadStaticText, reinterpret_cast<LPARAM>(&Texts));
    REQUIRE(Texts.size() == 5);
    CHECK(Texts[0] == ExpectedPrefix + L"Im Spiel");
    Status.State = UI::SessionState::Offline;
    UI::Overlay().Publish(Status);
    SendMessageW(Window, WM_TIMER, 1, 0);
    CHECK(Texts[1] == L"Lobby test message");
    CHECK(Texts[2].find(L"Rolle:") != std::wstring::npos);
    CHECK(Texts[4] == L"Desync: test diagnostics");
    CHECK(IsWindowEnabled(GetDlgItem(Window, 101)));
    CHECK_FALSE(IsWindowEnabled(GetDlgItem(Window, 106))); // Nothing to copy without a lobby.
    CHECK_FALSE(IsWindowEnabled(GetDlgItem(Window, 102)));
    SetWindowTextW(GetDlgItem(Window, 105), L"18446744073709551616");
    SendMessageW(Window, WM_TIMER, 1, 0);
    CHECK_FALSE(IsWindowEnabled(GetDlgItem(Window, 102)));
    SetWindowTextW(GetDlgItem(Window, 105), L"100000000000000000000");
    SendMessageW(Window, WM_TIMER, 1, 0);
    CHECK_FALSE(IsWindowEnabled(GetDlgItem(Window, 102)));
    SetWindowTextW(GetDlgItem(Window, 105), L"42");
    SendMessageW(Window, WM_TIMER, 1, 0);
    CHECK(IsWindowEnabled(GetDlgItem(Window, 102)));
    SendMessageW(GetDlgItem(Window, 102), BM_CLICK, 0, 0);
    CHECK(Calls == 0);
    SendMessageW(Window, WM_TIMER, 1, 0);
    CHECK_FALSE(IsWindowEnabled(GetDlgItem(Window, 101)));
    CHECK_FALSE(IsWindowEnabled(GetDlgItem(Window, 102)));
    CHECK(Core::GameThread().Drain() == 1);
    CHECK(Calls == 1);
    CHECK(Target == 42);
    SendMessageW(Window, WM_TIMER, 1, 0);
    CHECK_FALSE(IsWindowEnabled(GetDlgItem(Window, 101)));
    CHECK_FALSE(IsWindowVisible(Window));
    // coop-modus-wahl: Auswahl 108 zeigt den Modus, wählbar nur mit CanChooseMode ohne Sitzung und nur über das Modell.
    const HWND Mode = GetDlgItem(Window, 108);
    REQUIRE(Mode != nullptr);
    CHECK(SendMessageW(Mode, CB_GETCOUNT, 0, 0) == 2);
    UI::LobbyRequest Picked{ UI::LobbyAction::Create };
    UI::Overlay().Bind([&](UI::LobbyRequest Request) { Picked = Request; });
    Status.State = UI::SessionState::Offline;
    Status.Mode = Net::CoopMode::Own;
    UI::Overlay().Publish(Status);
    SendMessageW(Window, WM_TIMER, 1, 0);
    CHECK_FALSE(IsWindowEnabled(Mode));
    Status.CanChooseMode = true;
    UI::Overlay().Publish(Status);
    SendMessageW(Window, WM_TIMER, 1, 0);
    CHECK(IsWindowEnabled(Mode));
    CHECK(SendMessageW(Mode, CB_GETCURSEL, 0, 0) == 0);
    SendMessageW(Mode, CB_SETCURSEL, 1, 0);
    SendMessageW(Window, WM_COMMAND, MAKEWPARAM(108, CBN_SELCHANGE), reinterpret_cast<LPARAM>(Mode));
    CHECK(Core::GameThread().Drain() == 1);
    CHECK(Picked.Action == UI::LobbyAction::SetMode);
    CHECK(Picked.Mode == Net::CoopMode::Shared);
    // Host oder Client in einer Sitzung: Anzeige des festen Modus, keine Auswahl.
    Status.State = UI::SessionState::Playing;
    Status.Mode = Net::CoopMode::Shared;
    Status.ModeLocked = true;
    UI::Overlay().Publish(Status);
    SendMessageW(Window, WM_TIMER, 1, 0);
    CHECK_FALSE(IsWindowEnabled(Mode));
    CHECK(SendMessageW(Mode, CB_GETCURSEL, 0, 0) == 1);
    Status = {};
    Status.State = UI::SessionState::Offline;
    Status.CanCreate = Status.CanJoin = true;
    Status.Message = "Lobby test message";
    // Only the enable state; clicking would overwrite the user's clipboard.
    Status.LobbyId = 109775240000000042;
    UI::Overlay().Publish(Status);
    SendMessageW(Window, WM_TIMER, 1, 0);
    CHECK(IsWindowEnabled(GetDlgItem(Window, 106)));
    // Hinweisfenster (opt-boss-tafel-client): nie aktivierbar, Klicks gehen durch, ohne Spielfenster bleibt es verborgen.
    HWND Notice = nullptr;
    EnumWindows(FindNotice, reinterpret_cast<LPARAM>(&Notice));
    REQUIRE(Notice != nullptr);
    const LONG_PTR NoticeStyle = GetWindowLongPtrW(Notice, GWL_EXSTYLE);
    CHECK((NoticeStyle & WS_EX_NOACTIVATE) != 0);
    CHECK((NoticeStyle & WS_EX_TRANSPARENT) != 0);
    CHECK((NoticeStyle & WS_EX_TOOLWINDOW) != 0);
    CHECK(SendMessageW(Notice, WM_NCHITTEST, 0, 0) == HTTRANSPARENT);
    CHECK(SendMessageW(Notice, WM_MOUSEACTIVATE, 0, 0) == MA_NOACTIVATE);
    const HWND ForegroundBefore = GetForegroundWindow();
    UI::Overlay().PublishNotice("Der Host hat das Spiel angehalten");
    SendMessageW(Window, WM_TIMER, 1, 0);
    CHECK_FALSE(IsWindowVisible(Notice)); // Der Testprozess hat kein Spielfenster.
    CHECK(GetForegroundWindow() == ForegroundBefore);
    UI::Overlay().PublishNotice("");
    SendMessageW(Window, WM_TIMER, 1, 0);
    CHECK_FALSE(IsWindowVisible(Notice));
    UI::Overlay().Bind({});
    SendMessageW(Window, WM_CLOSE, 0, 0);
    CHECK(IsWindow(Window)); // Closing hides, does not stop the game's message loop.
    UI::Stop();
    CHECK_FALSE(IsWindow(Window));
    CHECK_FALSE(IsWindow(Notice));
    UI::Stop();
    UI::Start();
    REQUIRE(WaitForOverlay() != nullptr);
}
