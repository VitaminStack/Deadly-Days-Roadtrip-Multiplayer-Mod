#include <doctest.h>

#include <stdexcept>
#include <string>
#include <thread>

#include "Core/Dispatcher.h"

using Core::Dispatcher;

TEST_CASE("Dispatcher führt Aufgaben fremder Threads in Reihenfolge aus")
{
    Dispatcher Queue;
    std::string Order;
    std::thread Poster([&] {
        Queue.Post([&] { Order += 'a'; });
        Queue.Post([&] { Order += 'b'; });
    });
    Poster.join();

    CHECK(Order.empty());
    CHECK(Queue.Drain() == 2);
    CHECK(Order == "ab");
    CHECK(Queue.Drain() == 0);
}

TEST_CASE("Dispatcher verschiebt Nachzügler auf den nächsten Durchlauf")
{
    Dispatcher Queue;
    int Runs = 0;
    Queue.Post([&] {
        ++Runs;
        Queue.Post([&] { ++Runs; });
    });
    CHECK(Queue.Drain() == 1);
    CHECK(Runs == 1);
    CHECK(Queue.Drain() == 1);
    CHECK(Runs == 2);
}

TEST_CASE("Dispatcher läuft nach Ausnahmen weiter")
{
    Dispatcher Queue;
    bool Reached = false;
    Queue.Post([] { throw std::runtime_error("Test"); });
    Queue.Post([] { throw 1; });
    Queue.Post([&] { Reached = true; });
    CHECK(Queue.Drain() == 3);
    CHECK(Reached);
}

TEST_CASE("Dispatcher merkt sich seinen Thread")
{
    Dispatcher Queue;
    CHECK_FALSE(Queue.IsDrainThread());
    Queue.Drain();
    CHECK(Queue.IsDrainThread());

    bool OnOtherThread = true;
    std::thread Other([&] { OnOtherThread = Queue.IsDrainThread(); });
    Other.join();
    CHECK_FALSE(OnOtherThread);
}
