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
    if (const auto Package = Memory::ReadPointer(*World + Offsets::UObject::Outer))
        Info.Package = Memory::NameOf(*Package);
    // ClientConnections ist ein TArray: Daten, dann Anzahl.
    if (Driver)
        if (const auto Count = Memory::Read<int32_t>(*Driver + Offsets::UNetDriver::ClientConnections + 8); Count && *Count >= 0 && *Count < 64)
            Info.Clients = *Count;

    // PlayerArray ist ein TArray: Daten, dann Anzahl.
    if (const auto State = Memory::ReadPointer(*World + Offsets::UWorld::GameState))
        if (const auto Count = Memory::Read<int32_t>(*State + Offsets::AGameStateBase::PlayerArray + 8); Count && *Count >= 0 && *Count < 64)
            Info.Players = *Count;
    return Info;
}

void TickWorld()
{
    // Verglichen wird, was das Log zeigt: Paket und Clientzahl allein sollen keine Zeile erzeugen.
    static std::optional<std::string> Last;
    const auto Current = ReadWorld();
    const auto Text = Current ? std::optional(Describe(*Current)) : std::nullopt;
    if (Text == Last)
        return;
    Last = Text;
    if (Current)
        Core::Log::Info("Welt: {}", Describe(*Current));
    else
        Core::Log::Info("Welt: keine");
}
}
