#include <doctest.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include "Core/Dispatcher.h"
#include "Game/GameThread.h"

namespace
{
constexpr UINT kBlockMessage = WM_APP + 7;

// Ersetzt das Spielfenster: gleiche Klasse wie bei Unreal, eigener Thread mit Nachrichtenschleife.
class FakeGameWindow
{
public:
    FakeGameWindow()
    {
        Thread = std::thread([this] { Run(); });
        while (!Window)
            Sleep(5);
    }

    ~FakeGameWindow()
    {
        PostMessageW(Window, WM_CLOSE, 0, 0);
        Thread.join();
    }

    HWND Handle() const { return Window; }
    DWORD ThreadId() const { return Id; }

    static LRESULT CALLBACK Proc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam)
    {
        if (Message == WM_DESTROY)
            PostQuitMessage(0);
        if (Message == kBlockMessage)
        {
            Sleep(static_cast<DWORD>(WParam)); // steht wie ein hängender Game Thread
            return 0;
        }
        return DefWindowProcW(Hwnd, Message, WParam, LParam);
    }

private:
    void Run()
    {
        WNDCLASSW Class{};
        Class.lpfnWndProc = Proc;
        Class.hInstance = GetModuleHandleW(nullptr);
        Class.lpszClassName = L"UnrealWindow";
        RegisterClassW(&Class);
        Id = GetCurrentThreadId();
        Window = CreateWindowExW(0, L"UnrealWindow", L"Test", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, Class.hInstance, nullptr);
        MSG Message;
        while (GetMessageW(&Message, nullptr, 0, 0) > 0)
            DispatchMessageW(&Message);
    }

    std::thread Thread;
    std::atomic<HWND> Window = nullptr;
    std::atomic<DWORD> Id = 0;
};

template <typename Condition> bool WaitFor(Condition Check)
{
    for (int i = 0; i < 200; ++i)
    {
        if (Check())
            return true;
        Sleep(10);
    }
    return false;
}
}

TEST_CASE("Pump drains the dispatcher and runs ticks on the window thread, then detaches cleanly")
{
    FakeGameWindow Game;
    const auto Before = GetWindowLongPtrW(Game.Handle(), GWLP_WNDPROC);

    std::atomic<DWORD> TickThread = 0;
    std::atomic<int> Ticks = 0;
    std::atomic<bool> BlockTick = false;
    Game::GameThread::AddTick([&] {
        TickThread = GetCurrentThreadId();
        ++Ticks;
    });
    Game::GameThread::AddTick([&] {
        if (BlockTick.exchange(false))
            Sleep(1500); // steht mitten in einem Tick der Mod
    }, "Testabschnitt");
    Game::GameThread::Start(std::chrono::milliseconds(5));
    REQUIRE(WaitFor([] { return Game::GameThread::IsAttached(); }));
    CHECK(GetWindowLongPtrW(Game.Handle(), GWLP_WNDPROC) != Before);

    std::atomic<DWORD> TaskThread = 0;
    Core::GameThread().Post([&] { TaskThread = GetCurrentThreadId(); });
    REQUIRE(WaitFor([&] { return TaskThread != 0 && Ticks > 2; }));
    CHECK(TaskThread == Game.ThreadId());
    CHECK(TickThread == Game.ThreadId());

    // client-haenger-nach-niederlage: Steht der Game Thread, nimmt der Wächter eine Stapelprobe, und das Fenster läuft danach
    // weiter. Im selben Testfall, weil der Tick oben lokale Variablen nutzt.
    Game::GameThread::SetStallSettings({300, 300, 1});
    const int SamplesBefore = Game::GameThread::StallSamples();
    PostMessageW(Game.Handle(), kBlockMessage, 1500, 0);
    REQUIRE(WaitFor([&] { return Game::GameThread::StallSamples() > SamplesBefore; }));
    const std::string Report = Game::GameThread::LastStallReport();
    INFO(Report);
    CHECK(Report.find("GameTests.exe+0x") != std::string::npos);
    CHECK(Report.find("außerhalb der Mod-Ticks") != std::string::npos);
    CHECK(Report.find(":\\") == std::string::npos); // keine lokalen Pfade
    const int TicksBefore = Ticks;
    REQUIRE(WaitFor([&] { return Ticks > TicksBefore + 2; }));

    // Steht er in einem Tick, nennt die Probe dessen Namen, auch ohne Symbole der DLL.
    const int SamplesInTick = Game::GameThread::StallSamples();
    BlockTick = true;
    REQUIRE(WaitFor([&] { return Game::GameThread::StallSamples() > SamplesInTick; }));
    CHECK(Game::GameThread::LastStallReport().find("im Abschnitt Testabschnitt") != std::string::npos);
    const int TicksAfterBlock = Ticks;
    REQUIRE(WaitFor([&] { return Ticks > TicksAfterBlock + 2; }));
    Game::GameThread::SetStallSettings({});

    Game::GameThread::Stop();
    CHECK_FALSE(Game::GameThread::IsAttached());
    CHECK(GetWindowLongPtrW(Game.Handle(), GWLP_WNDPROC) == Before);
    const int After = Ticks;
    Sleep(50);
    CHECK(Ticks == After);
}
