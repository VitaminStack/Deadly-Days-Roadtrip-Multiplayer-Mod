#include "Game/NetworkObjects.h"

#include "Core/Dispatcher.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"

namespace Game
{
std::optional<std::vector<NetworkObject>> ReadNetworkObjects()
{
    if (!Core::GameThread().IsDrainThread())
        return std::nullopt;
    const auto World = Memory::ReadPointer(Memory::Rva(Offsets::GWorld));
    const auto Driver = World ? Memory::ReadPointer(*World + Offsets::UWorld::NetDriver) : std::nullopt;
    const auto Cache = Driver ? Memory::ReadPointer(*Driver + 0x1C8) : std::nullopt;
    if (!Cache || Memory::ReadPointer(*Cache + 0x150) != Driver)
        return std::nullopt;
    struct Array { uintptr_t Data; int32_t Num, Max; };
    const auto Slots = Memory::Read<Array>(*Cache + 8);
    if (!Slots || Slots->Num < 0 || Slots->Max < Slots->Num || Slots->Num > 65536 || (Slots->Num && !Slots->Data))
        return std::nullopt;
    const uintptr_t Bits = Memory::ReadPointer(*Cache + 0x28).value_or(*Cache + 0x18);
    const int32_t ObjectCount = Objects::Count();
    const auto Chunks = Memory::ReadPointer(Memory::CachedRva<Offsets::GObjects>());
    if (!Chunks)
        return std::nullopt;
    std::vector<NetworkObject> Result;
    for (int32_t Slot = 0; Slot < Slots->Num; ++Slot)
    {
        const auto Bit = Memory::Read<uint8_t>(Bits + (Slot >> 3));
        if (!Bit)
            return std::nullopt;
        if (!(*Bit & (1u << (Slot & 7))))
            continue;
        struct Entry { uint64_t Guid; int32_t Index, Serial; };
        const auto Value = Memory::Read<Entry>(Slots->Data + Slot * 0x40);
        if (!Value)
            return std::nullopt;
        if (Value->Index <= 0 || Value->Index >= ObjectCount || !Value->Serial || !Value->Guid ||
            Value->Guid > 0x3FFFFFFFFFFFFFull)
            continue;
        const auto Chunk = Memory::ReadPointer(*Chunks + 8 * (Value->Index >> 16));
        const uintptr_t Item = Chunk ? *Chunk + (Value->Index & 0xffff) * 0x18 : 0;
        if (!Item || Memory::Read<int32_t>(Item + 0x10) != Value->Serial)
            continue;
        if (const auto Object = Memory::ReadPointer(Item + 8))
            Result.push_back({Value->Guid, *Object});
    }
    return Result;
}

uint64_t NetworkGuidFor(uintptr_t Object)
{
    if (const auto Entries = ReadNetworkObjects())
        for (const auto& Entry : *Entries)
            if (Entry.Object == Object)
                return Entry.Guid;
    return 0;
}
}
