#include "Game/ConsoleAdapter.h"

#include <cstdint>
#include <string>

#include "Core/Dispatcher.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"

namespace Game
{
namespace
{
// Parameterlayout von UKismetSystemLibrary::ExecuteConsoleCommand, wie im Spike (research/spikes/a-connect/Core.cpp).
struct FString
{
    const wchar_t* Data;
    int32_t Num;
    int32_t Max;
};

struct ExecuteConsoleCommandParams
{
    uintptr_t WorldContext;
    FString Command;
    uintptr_t SpecificPlayer;
};
}

bool ExecuteConsoleCommand(std::wstring_view Command)
{
    if (!Core::GameThread().IsDrainThread())
    {
        Core::Log::Error("Konsole: nicht auf dem Game Thread aufgerufen");
        return false;
    }
    static uintptr_t Function = 0;
    static uintptr_t Library = 0;
    if (!Function)
        Function = Objects::FindObject("ExecuteConsoleCommand", "KismetSystemLibrary");
    if (!Library)
        Library = Objects::FindObject("Default__KismetSystemLibrary", "/Script/Engine");
    const auto World = Memory::ReadPointer(Memory::ImageBase() + Offsets::GWorld);
    if (!Function || !Library || !World)
    {
        Core::Log::Warn("Konsole: Funktion, Bibliothek oder Welt fehlt");
        return false;
    }
    const std::wstring Text(Command);
    ExecuteConsoleCommandParams Params{};
    Params.WorldContext = *World;
    Params.Command = {Text.c_str(), static_cast<int32_t>(Text.size() + 1), static_cast<int32_t>(Text.size() + 1)};
    Objects::ProcessEvent(Library, Function, &Params);
    return true;
}
}
