#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>

namespace Core
{
// Übergibt Arbeit von beliebigen Threads an den Game Thread. Unreal-Objekte nur in solchen Aufgaben anfassen.
class Dispatcher
{
public:
    // Darf von jedem Thread aufgerufen werden und blockiert nicht auf die Ausführung.
    void Post(std::function<void()> Task);

    // Führt die bis jetzt eingereihten Aufgaben aus, Aufrufer ist der Game Thread. Aufgaben, die währenddessen
    // dazukommen, laufen beim nächsten Aufruf. Liefert die Zahl der ausgeführten Aufgaben.
    size_t Drain();

    // Falsch, solange Drain noch nie lief.
    bool IsDrainThread() const;

private:
    std::mutex Mutex;
    std::vector<std::function<void()>> Queue;
    std::atomic<unsigned long> DrainThreadId = 0;
};

Dispatcher& GameThread();
}
