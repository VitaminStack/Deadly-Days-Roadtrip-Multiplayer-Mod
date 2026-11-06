#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

#include <windows.h>
#include <timeapi.h>

#pragma comment(lib, "winmm.lib")

int main(int Argc, char** Argv)
{
    // Die Tests warten auf Transport-Threads, die mit Sleep(1) und select() takten. Ohne feinere Zeitscheibe rundet
    // Windows jede dieser Pausen auf etwa 15,6 ms auf, das verlängert jeden Verbindungsaufbau um ein Vielfaches.
    timeBeginPeriod(1);
    const int Result = doctest::Context(Argc, Argv).run();
    timeEndPeriod(1);
    return Result;
}
