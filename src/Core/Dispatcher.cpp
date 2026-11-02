#include "Core/Dispatcher.h"

#include <windows.h>

#include <exception>
#include <utility>

#include "Core/Log.h"

namespace Core
{
void Dispatcher::Post(std::function<void()> Task)
{
    const std::scoped_lock Lock(Mutex);
    Queue.push_back(std::move(Task));
}

size_t Dispatcher::Drain()
{
    DrainThreadId.store(GetCurrentThreadId());

    std::vector<std::function<void()>> Pending;
    {
        const std::scoped_lock Lock(Mutex);
        Pending.swap(Queue);
    }

    // Eine Ausnahme darf nicht in den Spielcode laufen und die übrigen Aufgaben nicht verhindern.
    for (const auto& Task : Pending)
    {
        try
        {
            Task();
        }
        catch (const std::exception& Failure)
        {
            Log::Error("Game-Thread-Aufgabe fehlgeschlagen: {}", Failure.what());
        }
        catch (...)
        {
            Log::Error("Game-Thread-Aufgabe fehlgeschlagen");
        }
    }
    return Pending.size();
}

bool Dispatcher::IsDrainThread() const
{
    return DrainThreadId.load() == GetCurrentThreadId();
}

Dispatcher& GameThread()
{
    static Dispatcher Instance;
    return Instance;
}
}
