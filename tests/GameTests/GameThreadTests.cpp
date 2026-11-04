#include <doctest.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "Core/Dispatcher.h"
#include "Game/GameThread.h"

namespace
{
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
    Game::GameThread::AddTick([&] {
        TickThread = GetCurrentThreadId();
        ++Ticks;
    });
    Game::GameThread::Start(std::chrono::milliseconds(5));
    REQUIRE(WaitFor([] { return Game::GameThread::IsAttached(); }));
    CHECK(GetWindowLongPtrW(Game.Handle(), GWLP_WNDPROC) != Before);

    std::atomic<DWORD> TaskThread = 0;
    Core::GameThread().Post([&] { TaskThread = GetCurrentThreadId(); });
    REQUIRE(WaitFor([&] { return TaskThread != 0 && Ticks > 2; }));
    CHECK(TaskThread == Game.ThreadId());
    CHECK(TickThread == Game.ThreadId());

    Game::GameThread::Stop();
    CHECK_FALSE(Game::GameThread::IsAttached());
    CHECK(GetWindowLongPtrW(Game.Handle(), GWLP_WNDPROC) == Before);
    const int After = Ticks;
    Sleep(50);
    CHECK(Ticks == After);
}
