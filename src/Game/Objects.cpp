#include "Game/Objects.h"

#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include "Game/Memory.h"
#include "Game/Offsets.h"

namespace Game::Objects
{
uintptr_t LoadAsset(std::string_view Path)
{
    struct FString
    {
        const wchar_t* Data;
        int32_t Num;
        int32_t Max;
    };
    const uintptr_t Library = FindNamed("Default__KismetSystemLibrary");
    const uintptr_t MakePath = FindObject("MakeSoftObjectPath", "KismetSystemLibrary");
    const uintptr_t ToReference = FindObject("Conv_SoftObjPathToSoftObjRef", "KismetSystemLibrary");
    const uintptr_t Load = FindObject("LoadAsset_Blocking", "KismetSystemLibrary");
    if (!Library || !MakePath || !ToReference || !Load)
        return 0;
    const std::wstring Wide(Path.begin(), Path.end());
    const int32_t Size = static_cast<int32_t>(Wide.size() + 1);
    struct
    {
        FString PathString;
        uint8_t ReturnValue[0x20];
    } MakeParams{{Wide.c_str(), Size, Size}, {}};
    ProcessEvent(Library, MakePath, &MakeParams);
    struct
    {
        uint8_t SoftObjectPath[0x20];
        uint8_t ReturnValue[0x28];
    } ReferenceParams{};
    memcpy(ReferenceParams.SoftObjectPath, MakeParams.ReturnValue, sizeof(MakeParams.ReturnValue));
    ProcessEvent(Library, ToReference, &ReferenceParams);
    struct
    {
        uint8_t Asset[0x28];
        uintptr_t ReturnValue;
    } LoadParams{};
    memcpy(LoadParams.Asset, ReferenceParams.ReturnValue, sizeof(ReferenceParams.ReturnValue));
    ProcessEvent(Library, Load, &LoadParams);
    return LoadParams.ReturnValue;
}

uintptr_t At(int32_t Index)
{
    if (Index < 0)
        return 0;
    const auto Chunks = Memory::ReadPointer(Memory::CachedRva<Offsets::GObjects>());
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
    const auto Value = Memory::Read<int32_t>(Memory::CachedRva<Offsets::GObjects>() + Offsets::ObjectArray::NumElements);
    return Value && *Value > 0 ? *Value : 0;
}

namespace
{
bool WalkIsA(uintptr_t Class, std::string_view ClassName)
{
    // Die Kette ist kurz, die Grenze schützt nur vor einem kaputten Zeiger.
    std::optional<uintptr_t> Current = Class;
    for (int Depth = 0; Current && Depth < 64; ++Depth, Current = Memory::ReadPointer(*Current + Offsets::UStruct::SuperStruct))
        if (Memory::NameOf(*Current) == ClassName)
            return true;
    return false;
}

struct NameHash
{
    using is_transparent = void;
    size_t operator()(std::string_view Text) const { return std::hash<std::string_view>{}(Text); }
};

struct Verdict
{
    int32_t ClassName;
    bool Result;
};
}

bool IsA(uintptr_t Object, std::string_view ClassName)
{
    const auto Class = Memory::ReadPointer(Object + Offsets::UObject::Class);
    if (!Class)
        return false;
    const auto OwnName = Memory::Read<int32_t>(*Class + Offsets::UObject::Name);
    if (!OwnName)
        return false;
    // Suchen über alle Objekte fragen je Objekt die Klassenkette ab, mit Namen als Text sind das Millionen
    // Lesezugriffe pro Suche. Das Ergebnis je Klasse merken; der eigene Namensindex der Klasse erkennt eine
    // wiederverwendete Adresse. Je Thread, weil Hooks auch außerhalb des Game Threads fragen können.
    thread_local std::unordered_map<std::string, std::unordered_map<uintptr_t, Verdict>, NameHash, std::equal_to<>> Cache;
    auto Query = Cache.find(ClassName);
    if (Query == Cache.end())
        Query = Cache.emplace(std::string(ClassName), std::unordered_map<uintptr_t, Verdict>{}).first;
    auto& Known = Query->second;
    if (const auto Found = Known.find(*Class); Found != Known.end() && Found->second.ClassName == *OwnName)
        return Found->second.Result;
    const bool Result = WalkIsA(*Class, ClassName);
    Known[*Class] = {*OwnName, Result};
    return Result;
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

void ForEachLevelActor(uintptr_t Level, const std::function<void(uintptr_t)>& Visit)
{
    const uintptr_t Data = Level ? Memory::ReadPointer(Level + Offsets::ULevel::Actors).value_or(0) : 0;
    const int32_t Total = Data ? Memory::Read<int32_t>(Level + Offsets::ULevel::Actors + 8).value_or(-1) : -1;
    if (Total < 0 || Total > Offsets::ULevel::MaxActors)
        return;
    for (int32_t Index = 0; Index < Total; ++Index)
        if (const uintptr_t Actor = Memory::ReadPointer(Data + Index * sizeof(uintptr_t)).value_or(0))
            Visit(Actor);
}

uintptr_t FindNamed(std::string_view Name)
{
    const int32_t Total = Count();
    for (int32_t i = 0; i < Total; ++i)
        if (const uintptr_t Object = At(i); Object && Memory::NameOf(Object) == Name)
            return Object;
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
