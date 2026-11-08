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
    // Only the enable state; clicking would overwrite the user's clipboard.
    Status.LobbyId = 109775240000000042;
    UI::Overlay().Publish(Status);
    SendMessageW(Window, WM_TIMER, 1, 0);
    CHECK(IsWindowEnabled(GetDlgItem(Window, 106)));
    UI::Overlay().Bind({});
    SendMessageW(Window, WM_CLOSE, 0, 0);
    CHECK(IsWindow(Window)); // Closing hides, does not stop the game's message loop.
    UI::Stop();
    CHECK_FALSE(IsWindow(Window));
    UI::Stop();
    UI::Start();
    REQUIRE(WaitForOverlay() != nullptr);
}
