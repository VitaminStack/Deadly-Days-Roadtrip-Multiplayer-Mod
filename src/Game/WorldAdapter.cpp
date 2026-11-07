#include "Game/WorldAdapter.h"

#include <cstdint>
#include <format>

#include "Core/Dispatcher.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Offsets.h"

namespace Game
{
std::string_view ToString(NetRole Role)
{
    switch (Role)
    {
    case NetRole::Host:
        return "Host";
    case NetRole::Client:
        return "Client";
    default:
        return "allein";
    }
}

std::string Describe(const WorldInfo& Info)
{
    return std::format("{}, {}, {} Spieler", Info.Map.empty() ? "?" : Info.Map, ToString(Info.Role), Info.Players);
}

NetRole RoleFrom(bool HasNetDriver, bool HasServerConnection)
{
    if (!HasNetDriver)
        return NetRole::Standalone;
    return HasServerConnection ? NetRole::Client : NetRole::Host;
}

std::optional<WorldInfo> ReadWorld()
{
    if (!Core::GameThread().IsDrainThread())
    {
        Core::Log::Error("ReadWorld außerhalb des Game Threads aufgerufen");
        return std::nullopt;
    }
    const auto World = Memory::ReadPointer(Memory::Rva(Offsets::GWorld));
    if (!World)
        return std::nullopt;

    WorldInfo Info;
    Info.Map = Memory::NameOf(*World);
    const auto Driver = Memory::ReadPointer(*World + Offsets::UWorld::NetDriver);
    const bool HasConnection = Driver && Memory::ReadPointer(*Driver + Offsets::UNetDriver::ServerConnection);
    Info.Role = RoleFrom(Driver.has_value(), HasConnection);

    // PlayerArray ist ein TArray: Daten, dann Anzahl.
    if (const auto State = Memory::ReadPointer(*World + Offsets::UWorld::GameState))
        if (const auto Count = Memory::Read<int32_t>(*State + Offsets::AGameStateBase::PlayerArray + 8); Count && *Count >= 0 && *Count < 64)
            Info.Players = *Count;
    return Info;
}

void TickWorld()
{
    static std::optional<WorldInfo> Last;
    const auto Current = ReadWorld();
    if (Current == Last)
        return;
    Last = Current;
    if (Current)
        Core::Log::Info("Welt: {}", Describe(*Current));
    else
        Core::Log::Info("Welt: keine");
}
}
