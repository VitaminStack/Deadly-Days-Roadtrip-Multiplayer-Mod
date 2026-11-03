#include "../research/spikes/a-connect/RoundPhaseRuntime.h"
#include "../research/spikes/a-connect/RoundPhaseLocal.h"
#include <cassert>
#include <iostream>

using namespace Spike::RoundPhase;

int wmain(int Argc, wchar_t** Argv)
{
    if (Argc == 3 && std::wstring(Argv[1]) == L"--publish")
    {
        Wire Bytes{};
        const Snapshot Value{1234567, 1, 1, 2, 3, Phase::Stash, Reason::NewDay, 0};
        return Encode(Value, Bytes) && WriteLocal(Argv[2], Bytes) ? 0 : 1;
    }
    assert(Argc == 2);
    const std::wstring Path = Argv[1];
    Publisher Host;
    Subscriber Client;
    Wire Bytes{};
    assert(!Host.Heartbeat(Bytes));
    assert(!Host.Bind(0, 1, 1, 3));
    assert(Host.Bind(1234567, 1, 1, 3));
    assert(!Host.Bind(1234567, 1, 1, 3));
    assert(Client.Bind(1234567, 1, 1, 0));
    assert(Client.View().Waiting && Client.View().InputBlocked);
    assert(Host.Heartbeat(Bytes));
    assert(Client.Receive(Bytes.data(), Bytes.size(), 1) == Result::Applied);
    assert(!Client.View().InputBlocked);
    Client.Tick(3500);
    assert(!Client.View().Waiting);
    assert(Client.Receive(Bytes.data(), Bytes.size(), 3500) == Result::Duplicate);
    Client.Tick(3501);
    assert(Client.View().Waiting && Client.View().InputBlocked); // stale file cannot renew lease
    assert(Client.Receive(Bytes.data(), Bytes.size(), 3502) == Result::Duplicate);
    assert(Client.View().Waiting);
    assert(Host.Heartbeat(Bytes));
    assert(Client.Receive(Bytes.data(), Bytes.size(), 3503) == Result::Applied);
    assert(!Client.View().Waiting && !Client.View().InputBlocked);
    assert(!Host.Observe(Phase::RoundEnd, Reason::NoReason));
    for (auto Stage : {Phase::RoundEnd, Phase::Stash, Phase::Route, Phase::Travel})
    {
        assert(Host.Observe(Stage, Reason::NewDay));
        assert(Host.Heartbeat(Bytes));
        assert(Client.Receive(Bytes.data(), Bytes.size(), 3600) == Result::Applied);
        assert(Client.View().InputBlocked && Client.View().Host.Stage == Stage);
    }
    assert(!Host.Observe(Phase::Playing, Reason::NoReason));
    assert(!Host.Observe(Phase::Travel, Reason::NewDay, 101));
    assert(!Host.Bind(9, 1, 2, 4));
    assert(Host.Bind(1234567, 1, 2, 4));
    assert(Client.Bind(1234567, 1, 2, 3700));
    assert(Client.Receive(Bytes.data(), Bytes.size(), 3700) == Result::Rejected);
    assert(Client.View().Waiting && Client.View().InputBlocked);
    assert(Host.Heartbeat(Bytes));
    assert(Client.Receive(Bytes.data(), Bytes.size(), 3700) == Result::Applied);
    assert(!Client.View().InputBlocked);
    Client.Disconnect();
    Host.Disconnect();
    assert(!Client.View().Attached && !Client.View().InputBlocked);
    assert(!Host.Heartbeat(Bytes));

    // Real local OS transport across two owned TEST processes. No game or UI.
    wchar_t Exe[MAX_PATH]{};
    assert(GetModuleFileNameW(nullptr, Exe, MAX_PATH));
    std::wstring Command = L"\"" + std::wstring(Exe) + L"\" --publish \"" + Path + L"\"";
    STARTUPINFOW Startup{};
    Startup.cb = sizeof(Startup);
    PROCESS_INFORMATION Process{};
    assert(CreateProcessW(Exe, Command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
        nullptr, nullptr, &Startup, &Process));
    const DWORD Wait = WaitForSingleObject(Process.hProcess, 10000);
    if (Wait != WAIT_OBJECT_0)
        TerminateProcess(Process.hProcess, 1);
    DWORD Exit = 1;
    GetExitCodeProcess(Process.hProcess, &Exit);
    CloseHandle(Process.hThread);
    CloseHandle(Process.hProcess);
    assert(Wait == WAIT_OBJECT_0 && Exit == 0);
    assert(ReadLocal(Path, Bytes));
    assert(Client.Bind(1234567, 1, 1, 0));
    assert(Client.Receive(Bytes.data(), Bytes.size(), 0) == Result::Applied);
    assert(Client.View().Host.Stage == Phase::Stash && Client.View().InputBlocked);
    assert(WriteLocal(Path, Bytes)); // Atomic replacement of an existing complete snapshot.
    assert(ReadLocal(Path, Bytes));
    HANDLE File = CreateFileW(Path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    assert(File != INVALID_HANDLE_VALUE);
    DWORD Written = 0;
    assert(WriteFile(File, Bytes.data(), 47, &Written, nullptr));
    CloseHandle(File);
    assert(!ReadLocal(Path, Bytes));
    File = CreateFileW(Path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    assert(File != INVALID_HANDLE_VALUE);
    std::array<uint8_t, 49> Oversized{};
    assert(WriteFile(File, Oversized.data(), 49, &Written, nullptr));
    CloseHandle(File);
    assert(!ReadLocal(Path, Bytes));
    assert(DeleteFileW(Path.c_str()));
    assert(!ReadLocal(Path, Bytes));
    assert(Client.View().InputBlocked); // I/O failures do not invent an unlock.
    std::cout << "Round phase runtime policy and two-process local transport tests passed\n";
}
