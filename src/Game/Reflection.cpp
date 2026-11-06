#include "Game/Reflection.h"

#include <map>
#include <mutex>
#include <tuple>

#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"

namespace Game::Reflection
{
namespace
{
// Die Klassenketten sind kurz, die Feldlisten einer Klasse selten über ein paar hundert Einträge.
// Die Grenzen schützen nur vor kaputten oder zyklischen Zeigern.
constexpr int MaxSuperDepth = 64;
constexpr int MaxChildren = 4096;

std::string NameAt(uintptr_t Address, NameReader Names)
{
    const auto Index = Memory::Read<int32_t>(Address);
    const auto Number = Memory::Read<int32_t>(Address + 4);
    if (!Index || !Number || *Index < 0 || *Number < 0)
        return {};
    return Names(*Index, *Number);
}

// Für eine ganze Zahl im Feld bleibt bei einem Bitfeld-bool das Lesen als uint8 falsch, daher nur natives bool.
FieldError Check(const Field& Found, const FieldSpec& Spec, int32_t ArrayDim, int32_t StructSize)
{
    if (Found.Type != Spec.Type)
        return FieldError::TypeMismatch;
    if (Found.Size != Spec.Size || ArrayDim != 1 || (Found.Type == "BoolProperty" && Found.BoolMask != 0xFF))
        return FieldError::SizeMismatch;
    if (Found.Offset < 0 || StructSize <= 0 || Found.Offset > StructSize - Found.Size)
        return FieldError::OutOfBounds;
    return FieldError::None;
}
}

std::optional<Field> FindField(uintptr_t Struct, const FieldSpec& Spec, NameReader Names, FieldError* Error)
{
    if (Error)
        *Error = FieldError::NotFound;
    if (!Struct || !Names || Spec.Name.empty())
        return std::nullopt;

    auto Current = std::optional<uintptr_t>(Struct);
    for (int Depth = 0; Current && Depth < MaxSuperDepth; ++Depth, Current = Memory::ReadPointer(*Current + Offsets::UStruct::SuperStruct))
    {
        auto Property = Memory::ReadPointer(*Current + Offsets::UStruct::ChildProperties);
        for (int Count = 0; Property && Count < MaxChildren; ++Count, Property = Memory::ReadPointer(*Property + Offsets::FField::Next))
        {
            if (NameAt(*Property + Offsets::FField::Name, Names) != Spec.Name)
                continue;

            // Der erste Treffer zählt, wie in der Engine: ein gleichnamiges Feld der Unterklasse verdeckt das der Oberklasse.
            Field Found;
            const auto FieldClass = Memory::ReadPointer(*Property + Offsets::FField::Class);
            const auto Offset = Memory::Read<int32_t>(*Property + Offsets::FProperty::Offset);
            const auto Size = Memory::Read<int32_t>(*Property + Offsets::FProperty::ElementSize);
            const auto ArrayDim = Memory::Read<int32_t>(*Property + Offsets::FProperty::ArrayDim);
            const auto StructSize = Memory::Read<int32_t>(*Current + Offsets::UStruct::PropertiesSize);
            if (!FieldClass || !Offset || !Size || !ArrayDim || !StructSize)
                return std::nullopt;
            Found.Type = NameAt(*FieldClass + Offsets::FFieldClass::Name, Names);
            Found.Offset = *Offset;
            Found.Size = *Size;
            if (Found.Type == "BoolProperty")
            {
                const auto Mask = Memory::Read<uint8_t>(*Property + Offsets::FBoolProperty::FieldMask);
                if (!Mask)
                    return std::nullopt;
                Found.BoolMask = *Mask;
            }
            const FieldError Result = Check(Found, Spec, *ArrayDim, *StructSize);
            if (Error)
                *Error = Result;
            if (Result != FieldError::None)
                return std::nullopt;
            return Found;
        }
    }
    return std::nullopt;
}

uintptr_t FindFunction(uintptr_t Struct, std::string_view Name, NameReader Names)
{
    if (!Struct || !Names || Name.empty())
        return 0;
    auto Current = std::optional<uintptr_t>(Struct);
    for (int Depth = 0; Current && Depth < MaxSuperDepth; ++Depth, Current = Memory::ReadPointer(*Current + Offsets::UStruct::SuperStruct))
    {
        auto Child = Memory::ReadPointer(*Current + Offsets::UStruct::Children);
        for (int Count = 0; Child && Count < MaxChildren; ++Count, Child = Memory::ReadPointer(*Child + Offsets::UField::Next))
            if (NameAt(*Child + Offsets::UObject::Name, Names) == Name)
                return *Child;
    }
    return 0;
}

const char* ToString(FieldError Error)
{
    switch (Error)
    {
    case FieldError::None:
        return "ok";
    case FieldError::NotFound:
        return "nicht gefunden";
    case FieldError::TypeMismatch:
        return "anderer Typ";
    case FieldError::SizeMismatch:
        return "andere Größe";
    case FieldError::OutOfBounds:
        return "Offset außerhalb der Klasse";
    }
    return "unbekannt";
}

// FName mit Number > 0 heißt "Name_<Number-1>", so wie FName::ToString.
std::string DefaultNames(int32_t ComparisonIndex, int32_t Number)
{
    std::string Name = Memory::NameFromIndex(ComparisonIndex);
    if (!Name.empty() && Number > 0)
        Name += "_" + std::to_string(Number - 1);
    return Name;
}

namespace
{
using Key = std::tuple<std::string, std::string, std::string>;

std::mutex CacheMutex;
std::map<Key, std::optional<Field>> Fields;
std::map<Key, uintptr_t> Functions;

// Native Klassen leben bis zum Ende des Spiels, ein einmal gefundener Klassenzeiger bleibt gültig.
uintptr_t FindClass(std::string_view Package, std::string_view Class)
{
    const uintptr_t Object = Objects::FindObject(Class, Package);
    return Object && Objects::IsA(Object, "Class") ? Object : 0;
}
}

std::optional<Field> Property(const FieldSpec& Spec)
{
    Key Id{std::string(Spec.Package), std::string(Spec.Class), std::string(Spec.Name)};
    {
        std::lock_guard Lock(CacheMutex);
        if (const auto It = Fields.find(Id); It != Fields.end())
            return It->second;
    }
    // Fehlt die Klasse noch (früher Start), nichts cachen: der nächste Aufruf sucht neu.
    const uintptr_t Class = FindClass(Spec.Package, Spec.Class);
    if (!Class)
        return std::nullopt;
    FieldError Error = FieldError::None;
    auto Found = FindField(Class, Spec, DefaultNames, &Error);
    if (Found)
        Core::Log::Info("Reflexion: {}.{} bei 0x{:X}, {} Bytes, {}", Spec.Class, Spec.Name, Found->Offset, Found->Size, Found->Type);
    else
        Core::Log::Error("Reflexion: {}.{} ({}, {} Bytes) {}, betroffener Adapter bleibt aus", Spec.Class, Spec.Name, Spec.Type, Spec.Size,
                         ToString(Error));
    std::lock_guard Lock(CacheMutex);
    Fields[Id] = Found;
    return Found;
}

uintptr_t Function(std::string_view Package, std::string_view Class, std::string_view Name)
{
    Key Id{std::string(Package), std::string(Class), std::string(Name)};
    {
        std::lock_guard Lock(CacheMutex);
        if (const auto It = Functions.find(Id); It != Functions.end())
            return It->second;
    }
    const uintptr_t ClassObject = FindClass(Package, Class);
    if (!ClassObject)
        return 0;
    uintptr_t Found = FindFunction(ClassObject, Name, DefaultNames);
    if (Found && !Objects::IsA(Found, "Function"))
        Found = 0;
    if (!Found)
        Core::Log::Error("Reflexion: Funktion {}.{} nicht gefunden, betroffener Adapter bleibt aus", Class, Name);
    std::lock_guard Lock(CacheMutex);
    Functions[Id] = Found;
    return Found;
}
}
