#include "Mod/Control.h"

#include <windows.h>
#include <sddl.h>

#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>

#include "Core/Dispatcher.h"
#include "Core/Log.h"
#include "Mod/Lobby.h"
#include "Net/ControlProtocol.h"

#pragma comment(lib, "advapi32.lib")

namespace Mod::Control
{
namespace
{
// Läuft der Game Thread nicht (Ladebildschirm, Pause), antwortet der Kanal mit Fehler statt zu hängen.
constexpr auto kReplyTimeout = std::chrono::seconds(10);

// Liest bis zum Zeilenende. Falsch, wenn der Client trennt, ohne etwas zu senden.
bool ReadLine(HANDLE Pipe, std::string& Line, bool& TooLong)
{
    Line.clear();
    TooLong = false;
    char Buffer[128];
    for (;;)
    {
        DWORD Read = 0;
        if (!ReadFile(Pipe, Buffer, sizeof(Buffer), &Read, nullptr) || Read == 0)
            return !Line.empty();
        for (DWORD Index = 0; Index < Read; ++Index)
        {
            if (Buffer[Index] == '\n')
                return true;
            if (Line.size() > Net::Control::kMaxLine + 2)
                TooLong = true;
            else
                Line.push_back(Buffer[Index]);
        }
    }
}

std::string Handle(const std::string& Line, bool TooLong)
{
    if (TooLong)
        return Net::Control::ErrorReply("Anfrage zu lang");
    std::string Error;
    const auto Command = Net::Control::ParseCommand(Line, &Error);
    if (!Command)
        return Net::Control::ErrorReply(Error);
    // Spielobjekte nur auf dem Game Thread. Die Zusage liegt im shared_ptr, damit eine zu spät fertige Aufgabe nicht ins Leere schreibt.
    auto Reply = std::make_shared<std::promise<std::string>>();
    auto Future = Reply->get_future();
    Core::GameThread().Post([Reply, Command = *Command] {
        try
        {
            Reply->set_value(Lobby::Execute(Command));
        }
        catch (...)
        {
            Reply->set_value(Net::Control::ErrorReply("Befehl fehlgeschlagen"));
        }
    });
    if (Future.wait_for(kReplyTimeout) != std::future_status::ready)
        return Net::Control::ErrorReply("Zeitüberschreitung, Game Thread antwortet nicht");
    return Future.get();
}

void Serve(std::wstring Name)
{
    // Nur der eigene Benutzer (Owner Rights), kein Zugriff von anderen Konten.
    SECURITY_ATTRIBUTES Security{sizeof(Security), nullptr, FALSE};
    ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;GA;;;OW)", SDDL_REVISION_1, &Security.lpSecurityDescriptor, nullptr);
    for (;;)
    {
        const HANDLE Pipe = CreateNamedPipeW(Name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1,
                                             8192, 8192, 0, &Security);
        if (Pipe == INVALID_HANDLE_VALUE)
        {
            Core::Log::Error("Testkanal: Pipe nicht angelegt (Fehler {}), Kanal aus", GetLastError());
            return;
        }
        if (ConnectNamedPipe(Pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED)
        {
            std::string Line;
            bool TooLong = false;
            if (ReadLine(Pipe, Line, TooLong))
            {
                const std::string Reply = Handle(Line, TooLong) + "\n";
                DWORD Written = 0;
                WriteFile(Pipe, Reply.data(), static_cast<DWORD>(Reply.size()), &Written, nullptr);
                FlushFileBuffers(Pipe);
            }
        }
        DisconnectNamedPipe(Pipe);
        CloseHandle(Pipe);
    }
}
}

void Install()
{
    const std::wstring CommandLine = GetCommandLineW();
    if (CommandLine.find(L"-ddcoop_control") == std::wstring::npos)
        return;
    const std::wstring Name = L"\\\\.\\pipe\\ddcoop-ctl-" + std::to_wstring(GetCurrentProcessId());
    Core::Log::Warn("Testkanal: Pipe ddcoop-ctl-{} aktiv (-ddcoop_control), nur für Tests", GetCurrentProcessId());
    std::thread(Serve, Name).detach();
}
}
