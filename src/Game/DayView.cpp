#include "Game/DayView.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <string_view>
#include <type_traits>

#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"

namespace Game
{
namespace
{
constexpr int MaxSuperDepth = 32;
constexpr int MaxChildren = 1024;
constexpr int MaxFields = 200;
constexpr int MaxObjects = 64;
// FBoolProperty: FieldSize, ByteOffset, ByteMask, FieldMask ab +0x70 (Engine-Layout, FieldMask in Offsets.h).
constexpr uintptr_t BoolByteOffset = Offsets::FBoolProperty::FieldMask - 2;
constexpr uintptr_t BoolByteMask = Offsets::FBoolProperty::FieldMask - 1;
// FStructProperty::Struct direkt hinter FProperty (ebenfalls +0x70). Geprüft wird, dass dort ein ScriptStruct steht.
constexpr uintptr_t StructPropertyStruct = Offsets::FBoolProperty::FieldMask - 3;

// Oberklassen, ab denen nichts Spielspezifisches mehr kommt.
constexpr std::array<std::string_view, 7> StopClasses{"Object", "Actor", "ActorComponent", "Info", "GameStateBase", "GameState",
                                                      "GameInstanceSubsystem"};

// JSON-Text. Namen aus dem Spiel sind ASCII, Steuerzeichen werden ersetzt.
std::string Json(std::string_view Text)
{
    std::string Out = "\"";
    for (const char Character : Text)
    {
        if (Character == '"' || Character == '\\')
            Out.push_back('\\');
        Out.push_back(static_cast<unsigned char>(Character) < 0x20 ? '?' : Character);
    }
    return Out + "\"";
}

std::string FName(uintptr_t Address)
{
    const auto Index = Memory::Read<int32_t>(Address);
    const auto Number = Memory::Read<int32_t>(Address + 4);
    if (!Index || !Number || *Index < 0 || *Number < 0)
        return {};
    std::string Name = Memory::NameFromIndex(*Index);
    if (!Name.empty() && *Number > 0)
        Name += "_" + std::to_string(*Number - 1);
    return Name;
}

std::string Number(double Value)
{
    return std::isfinite(Value) ? std::format("{}", Value) : "null";
}

std::string ClassOf(uintptr_t Object)
{
    const auto Class = Memory::ReadPointer(Object + Offsets::UObject::Class);
    return Class ? Memory::NameOf(*Class) : std::string{};
}

std::string ObjectText(uintptr_t Object)
{
    return Object ? Memory::NameOf(Object) + "<" + ClassOf(Object) + ">" : "null";
}

// Wert eines skalaren Felds als JSON, leer, wenn der Typ nicht in Frage kommt.
std::string Value(uintptr_t Object, uintptr_t Property, const std::string& Type, int32_t Offset, int32_t Size)
{
    const uintptr_t At = Object + static_cast<uintptr_t>(Offset);
    const auto Text = [At]<typename T>(T) -> std::string
    {
        const auto Read = Memory::Read<T>(At);
        if (!Read)
            return {};
        if constexpr (std::is_floating_point_v<T>)
            return Number(*Read);
        else
            return std::to_string(*Read);
    };
    if ((Type == "IntProperty" || Type == "EnumProperty") && Size == 4)
        return Text(int32_t{});
    if (Type == "UInt32Property" && Size == 4)
        return Text(uint32_t{});
    if (Type == "Int64Property" && Size == 8)
        return Text(int64_t{});
    if ((Type == "ByteProperty" || Type == "EnumProperty") && Size == 1)
        return Text(uint8_t{});
    if (Type == "FloatProperty" && Size == 4)
        return Text(float{});
    if (Type == "DoubleProperty" && Size == 8)
        return Text(double{});
    if (Type == "BoolProperty")
    {
        const auto ByteOffset = Memory::Read<uint8_t>(Property + BoolByteOffset);
        const auto Mask = Memory::Read<uint8_t>(Property + BoolByteMask);
        const auto Byte = ByteOffset && Mask ? Memory::Read<uint8_t>(At + *ByteOffset) : std::nullopt;
        return Byte ? ((*Byte & *Mask) ? "true" : "false") : "";
    }
    if (Type == "NameProperty" && Size == 8)
        return Json(FName(At));
    if (Type == "ObjectProperty" && Size == 8)
        return Json(ObjectText(Memory::ReadPointer(At).value_or(0)));
    return {};
}

// Felder eines UStruct ab Base als "Prefix.Name":Wert. Structs werden eine Ebene tief aufgeklappt.
void AppendFields(uintptr_t Struct, uintptr_t Base, const std::string& Prefix, int Level, std::string& Out, int& Count)
{
    auto Property = Memory::ReadPointer(Struct + Offsets::UStruct::ChildProperties);
    for (int Index = 0; Property && *Property && Index < MaxChildren && Count < MaxFields; ++Index,
             Property = Memory::ReadPointer(*Property + Offsets::FField::Next))
    {
        const auto FieldClass = Memory::ReadPointer(*Property + Offsets::FField::Class);
        const auto Offset = Memory::Read<int32_t>(*Property + Offsets::FProperty::Offset);
        const auto Size = Memory::Read<int32_t>(*Property + Offsets::FProperty::ElementSize);
        const auto ArrayDim = Memory::Read<int32_t>(*Property + Offsets::FProperty::ArrayDim);
        if (!FieldClass || !Offset || !Size || ArrayDim.value_or(0) != 1 || *Offset < 0)
            continue;
        const std::string Type = FName(*FieldClass + Offsets::FFieldClass::Name);
        const std::string Name = Prefix + "." + FName(*Property + Offsets::FField::Name);
        if (Type == "StructProperty")
        {
            const uintptr_t Inner = Memory::ReadPointer(*Property + StructPropertyStruct).value_or(0);
            if (Level == 0 && Inner && Objects::IsA(Inner, "ScriptStruct"))
                AppendFields(Inner, Base + static_cast<uintptr_t>(*Offset), Name, Level + 1, Out, Count);
            continue;
        }
        const std::string Text = Value(Base, *Property, Type, *Offset, *Size);
        if (Text.empty())
            continue;
        Out += std::format("{}{}:{}", Count ? "," : "", Json(Name), Text);
        ++Count;
    }
}

// Alle skalaren Felder der spielspezifischen Klassenkette.
std::string Fields(uintptr_t Object)
{
    std::string Out;
    int Count = 0;
    auto Current = Memory::ReadPointer(Object + Offsets::UObject::Class);
    for (int Depth = 0; Current && *Current && Depth < MaxSuperDepth; ++Depth,
             Current = Memory::ReadPointer(*Current + Offsets::UStruct::SuperStruct))
    {
        const std::string ClassName = Memory::NameOf(*Current);
        if (std::find(StopClasses.begin(), StopClasses.end(), ClassName) != StopClasses.end())
            break;
        AppendFields(*Current, Object, ClassName, 0, Out, Count);
    }
    return "{" + Out + "}";
}

bool Live(uintptr_t Object)
{
    const auto Flags = Memory::Read<uint32_t>(Object + Offsets::UObject::Flags);
    return Flags && !(*Flags & Offsets::UObject::ClassDefaultObjectFlag) && Memory::NameOf(Object).find("Default__") != 0;
}

std::string Entry(uintptr_t Object)
{
    const auto Outer = Memory::ReadPointer(Object + Offsets::UObject::Outer).value_or(0);
    return std::format("{{\"object\":{},\"outer\":{},\"fields\":{}}}", Json(ObjectText(Object)),
                       Json(ObjectText(Outer)), Fields(Object));
}
}

std::string DescribeDayView()
{
    const uintptr_t World = Patches::CurrentWorld();
    const uintptr_t State = World ? Memory::ReadPointer(World + Offsets::UWorld::GameState).value_or(0) : 0;
    if (!State)
        return "\"gameState\":null";
    // Unterobjekte des GameState (Tank, Gefahrenstufe, Wallets) und alle Missionsziele, gleich wem sie gehören.
    std::string Children, Objectives;
    int ChildCount = 0, ObjectiveCount = 0;
    const int32_t Total = Objects::Count();
    for (int32_t Index = 0; Index < Total; ++Index)
    {
        const uintptr_t Object = Objects::At(Index);
        if (!Object || Object == State || !Live(Object))
            continue;
        if (ObjectiveCount < MaxObjects && Objects::IsA(Object, "DDObjective"))
            Objectives += (ObjectiveCount++ ? "," : "") + Entry(Object);
        else if (ChildCount < MaxObjects)
        {
            // Auch Enkel, etwa der Nacht-Controller unter der NightModeComponent.
            const uintptr_t Outer = Memory::ReadPointer(Object + Offsets::UObject::Outer).value_or(0);
            if (Outer == State || (Outer && Memory::ReadPointer(Outer + Offsets::UObject::Outer).value_or(0) == State))
                Children += (ChildCount++ ? "," : "") + Entry(Object);
        }
    }
    return std::format("\"gameState\":{},\"children\":[{}],\"objectives\":[{}]", Entry(State), Children, Objectives);
}
}
