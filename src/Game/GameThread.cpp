#include "Game/GameThread.h"

#include <windows.h>

#include <atomic>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Core/Dispatcher.h"
#include "Core/Log.h"

namespace Game::GameThread
{
namespace
{
constexpr UINT_PTR kTimerId = 0x44444301;

std::mutex Mutex;
std::vector<Tick> Ticks;
std::jthread Finder;
std::atomic<HWND> Window = nullptr;
std::atomic<WNDPROC> Original = nullptr;
std::atomic<bool> Attached = false;
std::atomic<UINT> IntervalMs = 16;
bool Running = false; // Nur auf dem Game Thread benutzt.

UINT StartMessage()
{
    static const UINT Message = RegisterWindowMessageW(L"DDCoop.GameThread.Start");
    return Message;
}

UINT StopMessage()
{
    static const UINT Message = RegisterWindowMessageW(L"DDCoop.GameThread.Stop");
    return Message;
}

void RunTicks()
{
    std::vector<Tick> Current;
    {
        const std::scoped_lock Lock(Mutex);
        Current = Ticks;
    }
    for (const auto& Callback : Current)
    {
        try
        {
            Callback();
        }
        catch (const std::exception& Failure)
        {
            Core::Log::Error("Game-Thread-Tick fehlgeschlagen: {}", Failure.what());
        }
        catch (...)
        {
            Core::Log::Error("Game-Thread-Tick fehlgeschlagen");
        }
    }
}

LRESULT CALLBACK WndProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam);

// Hängt die eigene Prozedur wieder aus, wenn niemand nach uns eine eigene eingehängt hat. Sonst bleibt sie als
// reine Weiterleitung, damit die Kette der anderen nicht reißt.
void Detach(HWND Hwnd)
{
    KillTimer(Hwnd, kTimerId);
    const WNDPROC Previous = Original.load();
    if (Previous && reinterpret_cast<WNDPROC>(GetWindowLongPtrW(Hwnd, GWLP_WNDPROC)) == WndProc)
        SetWindowLongPtrW(Hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(Previous));
    if (Attached.exchange(false))
        Core::Log::Info("Game-Thread-Takt abgehängt");
}

LRESULT CALLBACK WndProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam)
{
    const WNDPROC Previous = Original.load();
    if (Message == WM_TIMER && WParam == kTimerId)
    {
        // Ein Tick kann selbst Nachrichten pumpen, dann darf kein zweiter Tick hineinlaufen.
        if (Attached && !Running)
        {
            Running = true;
            Core::GameThread().Drain();
            RunTicks();
            Running = false;
        }
        return 0;
    }
    if (Message == StartMessage() && StartMessage())
    {
        if (!Attached.exchange(true))
        {
            SetTimer(Hwnd, kTimerId, IntervalMs.load(), nullptr);
            Core::Log::Info("Game-Thread-Takt aktiv, alle {} ms", IntervalMs.load());
        }
        return 0;
    }
    if (Message == StopMessage() && StopMessage())
    {
        Detach(Hwnd);
        return 0;
    }
    if (Message == WM_NCDESTROY)
    {
        Detach(Hwnd);
        Window = nullptr;
    }
    return Previous ? CallWindowProcW(Previous, Hwnd, Message, WParam, LParam) : DefWindowProcW(Hwnd, Message, WParam, LParam);
}

BOOL CALLBACK FindGameWindow(HWND Candidate, LPARAM Result)
{
    DWORD Process = 0;
    GetWindowThreadProcessId(Candidate, &Process);
    wchar_t Class[32]{};
    GetClassNameW(Candidate, Class, 32);
    if (Process == GetCurrentProcessId() && std::wstring(Class) == L"UnrealWindow")
    {
        *reinterpret_cast<HWND*>(Result) = Candidate;
        return FALSE;
    }
    return TRUE;
}

void Find(std::stop_token Stop)
{
    while (!Stop.stop_requested())
    {
        HWND Found = nullptr;
        EnumWindows(FindGameWindow, reinterpret_cast<LPARAM>(&Found));
        if (Found)
        {
            // Erst die Prozedur einhängen, dann den Timer vom Game Thread selbst setzen lassen.
            // Hängt die Prozedur nach einem Stop noch als Weiterleitung, bleibt die alte Kette gültig.
            const auto Previous = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(Found, GWLP_WNDPROC));
            if (Previous != WndProc)
            {
                Original = Previous;
                SetWindowLongPtrW(Found, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc));
            }
            Window = Found;
            PostMessageW(Found, StartMessage(), 0, 0);
            return;
        }
        Sleep(100);
    }
}
}

void Start(std::chrono::milliseconds Interval)
{
    if (Window || Finder.joinable())
        return;
    IntervalMs = static_cast<UINT>(Interval.count() > 0 ? Interval.count() : 1);
    // Die Nachrichten-IDs vor dem ersten Aufruf auf dem Game Thread anlegen.
    StartMessage();
    StopMessage();
    Finder = std::jthread(Find);
}

void Stop()
{
    if (Finder.joinable())
    {
        Finder.request_stop();
        Finder.join();
    }
    if (const HWND Hwnd = Window.exchange(nullptr))
    {
        DWORD_PTR Result = 0;
        SendMessageTimeoutW(Hwnd, StopMessage(), 0, 0, SMTO_ABORTIFHUNG, 2000, &Result);
    }
    // Original bleibt gesetzt: Hängt die Prozedur noch als Weiterleitung, braucht sie das Ziel weiter.
    Finder = {};
}

bool IsAttached()
{
    return Attached;
}

void AddTick(Tick Callback)
{
    const std::scoped_lock Lock(Mutex);
    Ticks.push_back(std::move(Callback));
}
}
