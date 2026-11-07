#include <doctest.h>

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "Game/Offsets.h"
#include "Game/Reflection.h"

// Nachgebaute Engine-Layouts (UStruct, FField, FProperty, UFunction) in gewöhnlichem Speicher.
namespace
{
namespace Offsets = Game::Offsets;
using Game::Reflection::FieldError;
using Game::Reflection::FieldSpec;

const std::array<const char*, 12> NameTable = {"None",          "Base",         "Derived",   "IntProperty", "BoolProperty", "StrProperty",
                                               "SkipFlag",      "Enabled",      "Url",       "Shadowed",    "DoThing",      "Bits"};

std::string Names(int32_t Index, int32_t Number)
{
    if (Index < 0 || Index >= static_cast<int32_t>(NameTable.size()))
        return {};
    std::string Name = NameTable[Index];
    if (Number > 0)
        Name += "_" + std::to_string(Number - 1);
    return Name;
}

int32_t IndexOf(std::string_view Name)
{
    for (size_t i = 0; i < NameTable.size(); ++i)
        if (Name == NameTable[i])
            return static_cast<int32_t>(i);
    FAIL("Name fehlt in der Tabelle");
    return -1;
}

struct Block
{
    std::vector<uint8_t> Bytes;
    explicit Block(size_t Size) : Bytes(Size, 0) {}
    uintptr_t Address() { return reinterpret_cast<uintptr_t>(Bytes.data()); }
    template <typename T> void Set(uintptr_t Offset, T Value) { std::memcpy(Bytes.data() + Offset, &Value, sizeof(T)); }
    void SetName(uintptr_t Offset, std::string_view Name, int32_t Number = 0)
    {
        Set<int32_t>(Offset, IndexOf(Name));
        Set<int32_t>(Offset + 4, Number);
    }
};

struct Layout
{
    std::vector<std::unique_ptr<Block>> Blocks;

    Block& New(size_t Size) { return *Blocks.emplace_back(std::make_unique<Block>(Size)); }

    Block& FieldClass(std::string_view Name)
    {
        Block& B = New(0x30);
        B.SetName(Offsets::FFieldClass::Name, Name);
        return B;
    }

    Block& Struct(std::string_view Name, int32_t Size, Block* Super = nullptr)
    {
        Block& B = New(0xB0);
        B.SetName(Offsets::UObject::Name, Name);
        B.Set<int32_t>(Offsets::UStruct::PropertiesSize, Size);
        if (Super)
            B.Set<uintptr_t>(Offsets::UStruct::SuperStruct, Super->Address());
        return B;
    }

    // Hängt die Property vorne an die Kette von Owner.
    Block& Property(Block& Owner, Block& Class, std::string_view Name, int32_t Offset, int32_t Size, int32_t ArrayDim = 1, uint8_t Mask = 0xFF)
    {
        Block& B = New(0x78);
        B.Set<uintptr_t>(Offsets::FField::Class, Class.Address());
        B.SetName(Offsets::FField::Name, Name);
        B.Set<int32_t>(Offsets::FProperty::ArrayDim, ArrayDim);
        B.Set<int32_t>(Offsets::FProperty::ElementSize, Size);
        B.Set<int32_t>(Offsets::FProperty::Offset, Offset);
        B.Set<uint8_t>(Offsets::FBoolProperty::FieldMask, Mask);
        B.Set<uintptr_t>(Offsets::FField::Next, *reinterpret_cast<uintptr_t*>(Owner.Bytes.data() + Offsets::UStruct::ChildProperties));
        Owner.Set<uintptr_t>(Offsets::UStruct::ChildProperties, B.Address());
        return B;
    }

    Block& Function(Block& Owner, std::string_view Name)
    {
        Block& B = New(0xE0);
        B.SetName(Offsets::UObject::Name, Name);
        B.Set<uintptr_t>(Offsets::UField::Next, *reinterpret_cast<uintptr_t*>(Owner.Bytes.data() + Offsets::UStruct::Children));
        Owner.Set<uintptr_t>(Offsets::UStruct::Children, B.Address());
        return B;
    }
};

FieldSpec Spec(std::string_view Name, std::string_view Type, int32_t Size)
{
    return FieldSpec{"/Script/Test", "Derived", Name, Type, Size};
}
}

TEST_CASE("Reflexion findet Felder mit Offset, Größe und Typ, auch in der Oberklasse")
{
    Layout L;
    Block& Int = L.FieldClass("IntProperty");
    Block& Bool = L.FieldClass("BoolProperty");
    Block& Str = L.FieldClass("StrProperty");
    Block& Base = L.Struct("Base", 0x50);
    Block& Derived = L.Struct("Derived", 0xC8, &Base);
    L.Property(Base, Bool, "Enabled", 0x38, 1);
    L.Property(Base, Str, "Url", 0x40, 0x10);
    L.Property(Derived, Int, "SkipFlag", 0xB8, 4);

    const auto Skip = Game::Reflection::FindField(Derived.Address(), Spec("SkipFlag", "IntProperty", 4), Names);
    REQUIRE(Skip);
    CHECK(Skip->Offset == 0xB8);
    CHECK(Skip->Size == 4);
    CHECK(Skip->Type == "IntProperty");

    const auto Enabled = Game::Reflection::FindField(Derived.Address(), Spec("Enabled", "BoolProperty", 1), Names);
    REQUIRE(Enabled);
    CHECK(Enabled->Offset == 0x38);
    CHECK(Enabled->BoolMask == 0xFF);

    const auto Url = Game::Reflection::FindField(Derived.Address(), Spec("Url", "StrProperty", 0x10), Names);
    REQUIRE(Url);
    CHECK(Url->Offset == 0x40);
}

TEST_CASE("Reflexion liefert nichts, wenn Feld, Typ, Größe oder Grenze nicht passen")
{
    Layout L;
    Block& Int = L.FieldClass("IntProperty");
    Block& Bool = L.FieldClass("BoolProperty");
    Block& Derived = L.Struct("Derived", 0x40);
    L.Property(Derived, Int, "SkipFlag", 0x38, 4);
    L.Property(Derived, Bool, "Bits", 0x3C, 1, 1, 0x04);
    L.Property(Derived, Int, "Url", 0x3E, 4);
    L.Property(Derived, Int, "Shadowed", 0x30, 4, 2);

    FieldError Error = FieldError::None;
    CHECK_FALSE(Game::Reflection::FindField(Derived.Address(), Spec("Enabled", "IntProperty", 4), Names, &Error));
    CHECK(Error == FieldError::NotFound);
    CHECK_FALSE(Game::Reflection::FindField(Derived.Address(), Spec("SkipFlag", "BoolProperty", 4), Names, &Error));
    CHECK(Error == FieldError::TypeMismatch);
    CHECK_FALSE(Game::Reflection::FindField(Derived.Address(), Spec("SkipFlag", "IntProperty", 8), Names, &Error));
    CHECK(Error == FieldError::SizeMismatch);
    // Bitfeld-bool ließe sich nicht als ganzes Byte lesen.
    CHECK_FALSE(Game::Reflection::FindField(Derived.Address(), Spec("Bits", "BoolProperty", 1), Names, &Error));
    CHECK(Error == FieldError::SizeMismatch);
    // Statisches Array.
    CHECK_FALSE(Game::Reflection::FindField(Derived.Address(), Spec("Shadowed", "IntProperty", 4), Names, &Error));
    CHECK(Error == FieldError::SizeMismatch);
    // 0x3E + 4 ragt über die Klassengröße 0x40.
    CHECK_FALSE(Game::Reflection::FindField(Derived.Address(), Spec("Url", "IntProperty", 4), Names, &Error));
    CHECK(Error == FieldError::OutOfBounds);
}

TEST_CASE("Reflexion nimmt das Feld der Unterklasse vor dem gleichnamigen der Oberklasse")
{
    Layout L;
    Block& Int = L.FieldClass("IntProperty");
    Block& Base = L.Struct("Base", 0x40);
    Block& Derived = L.Struct("Derived", 0x60, &Base);
    L.Property(Base, Int, "Shadowed", 0x30, 4);
    L.Property(Derived, Int, "Shadowed", 0x50, 4);
    const auto Found = Game::Reflection::FindField(Derived.Address(), Spec("Shadowed", "IntProperty", 4), Names);
    REQUIRE(Found);
    CHECK(Found->Offset == 0x50);
}

TEST_CASE("Reflexion beachtet die Nummer im FName")
{
    Layout L;
    Block& Int = L.FieldClass("IntProperty");
    Block& Derived = L.Struct("Derived", 0x40);
    Block& Numbered = L.Property(Derived, Int, "SkipFlag", 0x30, 4);
    Numbered.SetName(Offsets::FField::Name, "SkipFlag", 1);
    CHECK_FALSE(Game::Reflection::FindField(Derived.Address(), Spec("SkipFlag", "IntProperty", 4), Names));
    const auto Found = Game::Reflection::FindField(Derived.Address(), Spec("SkipFlag_0", "IntProperty", 4), Names);
    REQUIRE(Found);
    CHECK(Found->Offset == 0x30);
}

TEST_CASE("Reflexion endet bei zyklischen Ketten und ungültigen Eingaben")
{
    Layout L;
    Block& Int = L.FieldClass("IntProperty");
    Block& Derived = L.Struct("Derived", 0x40);
    Block& First = L.Property(Derived, Int, "SkipFlag", 0x30, 4);
    First.Set<uintptr_t>(Offsets::FField::Next, First.Address()); // Feldliste zeigt auf sich selbst
    Derived.Set<uintptr_t>(Offsets::UStruct::SuperStruct, Derived.Address()); // Klasse ist ihre eigene Oberklasse
    L.Function(Derived, "DoThing").Set<uintptr_t>(Offsets::UField::Next, 0);
    Block& Loop = L.Function(Derived, "Base");
    Loop.Set<uintptr_t>(Offsets::UField::Next, Loop.Address());

    CHECK_FALSE(Game::Reflection::FindField(Derived.Address(), Spec("Enabled", "IntProperty", 4), Names));
    CHECK(Game::Reflection::FindFunction(Derived.Address(), "Missing", Names) == 0);
    CHECK_FALSE(Game::Reflection::FindField(0, Spec("SkipFlag", "IntProperty", 4), Names));
    CHECK_FALSE(Game::Reflection::FindField(Derived.Address(), Spec("SkipFlag", "IntProperty", 4), nullptr));
    CHECK(Game::Reflection::FindFunction(0, "DoThing", Names) == 0);
}

TEST_CASE("Reflexion findet Funktionen nach Namen, auch in der Oberklasse")
{
    Layout L;
    Block& Base = L.Struct("Base", 0x40);
    Block& Derived = L.Struct("Derived", 0x60, &Base);
    Block& Inherited = L.Function(Base, "DoThing");
    Block& Own = L.Function(Derived, "Shadowed");
    CHECK(Game::Reflection::FindFunction(Derived.Address(), "Shadowed", Names) == Own.Address());
    CHECK(Game::Reflection::FindFunction(Derived.Address(), "DoThing", Names) == Inherited.Address());
    CHECK(Game::Reflection::FindFunction(Base.Address(), "Shadowed", Names) == 0);
}
