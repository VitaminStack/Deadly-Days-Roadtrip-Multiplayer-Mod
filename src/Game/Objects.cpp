#include "Game/Objects.h"

#include "Game/Memory.h"
#include "Game/Offsets.h"

namespace Game::Objects
{
uintptr_t At(int32_t Index)
{
    if (Index < 0)
        return 0;
    const auto Chunks = Memory::ReadPointer(Memory::ImageBase() + Offsets::GObjects);
    if (!Chunks)
        return 0;
    const auto Chunk = Memory::ReadPointer(*Chunks + 8 * (Index / Offsets::ObjectArray::ElementsPerChunk));
    if (!Chunk)
        return 0;
    const uintptr_t Item = *Chunk + (Index % Offsets::ObjectArray::ElementsPerChunk) * Offsets::ObjectArray::ItemSize;
    return Memory::ReadPointer(Item + Offsets::ObjectArray::ItemObject).value_or(0);
}

int32_t Count()
{
    const auto Value = Memory::Read<int32_t>(Memory::ImageBase() + Offsets::GObjects + Offsets::ObjectArray::NumElements);
    return Value && *Value > 0 ? *Value : 0;
}

bool IsA(uintptr_t Object, std::string_view ClassName)
{
    auto Class = Memory::ReadPointer(Object + Offsets::UObject::Class);
    // Die Kette ist kurz, die Grenze schützt nur vor einem kaputten Zeiger.
    for (int Depth = 0; Class && Depth < 64; ++Depth, Class = Memory::ReadPointer(*Class + Offsets::UStruct::SuperStruct))
        if (Memory::NameOf(*Class) == ClassName)
            return true;
    return false;
}

uintptr_t FindInstance(std::string_view ClassName, int32_t* Index)
{
    const int32_t Total = Count();
    for (int32_t i = 0; i < Total; ++i)
    {
        const uintptr_t Object = At(i);
        if (!Object)
            continue;
        const auto Flags = Memory::Read<uint32_t>(Object + Offsets::UObject::Flags);
        if (!Flags || (*Flags & Offsets::UObject::ClassDefaultObjectFlag) || !IsA(Object, ClassName))
            continue;
        if (Index)
            *Index = i;
        return Object;
    }
    return 0;
}

uintptr_t FindObject(std::string_view Name, std::string_view OuterName)
{
    const int32_t Total = Count();
    for (int32_t i = 0; i < Total; ++i)
    {
        const uintptr_t Object = At(i);
        if (!Object || Memory::NameOf(Object) != Name)
            continue;
        if (OuterName.empty())
            return Object;
        if (const auto Outer = Memory::ReadPointer(Object + Offsets::UObject::Outer); Outer && Memory::NameOf(*Outer) == OuterName)
            return Object;
    }
    return 0;
}

void ProcessEvent(uintptr_t Object, uintptr_t Function, void* Params)
{
    using ProcessEventFn = void(__fastcall*)(uintptr_t, uintptr_t, void*);
    const auto VTable = Memory::ReadPointer(Object);
    if (!VTable)
        return;
    const auto Target = Memory::ReadPointer(*VTable + 8 * Offsets::ProcessEventIndex);
    if (Target)
        reinterpret_cast<ProcessEventFn>(*Target)(Object, Function, Params);
}
}
