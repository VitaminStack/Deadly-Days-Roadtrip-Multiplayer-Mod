#include <windows.h>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <format>
#include <string>

#include "Core/Hooks.h"
#include "Core/Signature.h"

using namespace Core;

namespace
{
using Fn = int (*)(int);

__declspec(noinline) int Plain(int Value)
{
    volatile int Result = Value * 3;
    return Result + 1;
}

Fn Original = nullptr;
int DetourCalls = 0;

int Detour(int Value)
{
    ++DetourCalls;
    return Original(Value) + 100;
}

// In Debug-Builds zeigt die Adresse auf einen Sprung ins Linker-Thunk.
const uint8_t* Body(Fn Function)
{
    auto Code = reinterpret_cast<const uint8_t*>(Function);
    if (Code[0] == 0xE9)
        Code += 5 + *reinterpret_cast<const int32_t*>(Code + 1);
    return Code;
}
}

TEST_CASE("Muster werden streng geparst")
{
    const auto Parsed = ParsePattern("48 8b ?? 05 ?");
    REQUIRE(Parsed);
    CHECK(Parsed->size() == 5);
    CHECK((*Parsed)[1] == 0x8B);
    CHECK_FALSE((*Parsed)[2]);
    CHECK(ParsePattern("  AA   BB ")->size() == 2);

    CHECK_FALSE(ParsePattern(""));
    CHECK_FALSE(ParsePattern("?? ?"));
    CHECK_FALSE(ParsePattern("4"));
    CHECK_FALSE(ParsePattern("4X"));
    CHECK_FALSE(ParsePattern("123"));
}

TEST_CASE("Nur ein einziger Treffer zählt")
{
    const std::array<uint8_t, 9> Bytes = {1, 2, 3, 1, 2, 4, 9, 9, 9};
    CHECK(FindUnique(Bytes, *ParsePattern("01 02 03")) == 0u);
    CHECK(FindUnique(Bytes, *ParsePattern("01 02 04")) == 3u);
    CHECK(FindUnique(Bytes, *ParsePattern("03 ?? 02")) == 2u);
    CHECK_FALSE(FindUnique(Bytes, *ParsePattern("01 02")));
    CHECK_FALSE(FindUnique(Bytes, *ParsePattern("07")));
    CHECK_FALSE(FindUnique(Bytes, *ParsePattern("09 09")));
    CHECK_FALSE(FindUnique(std::span(Bytes).first(2), *ParsePattern("01 02 03")));
}

TEST_CASE("Suche im Modul trifft nur ausführbaren Code")
{
    const auto Module = GetModuleHandleW(nullptr);
    const uint8_t* Code = Body(&Plain);
    std::string Text;
    for (int Index = 0; Index < 24; ++Index)
        Text += std::format("{:02X} ", Code[Index]);

    CHECK(FindUniqueInImage(Module, *ParsePattern(Text)) == Code);
    CHECK_FALSE(FindUniqueInImage(Module, *ParsePattern("CC CC")));
    CHECK_FALSE(FindUniqueInImage(nullptr, *ParsePattern("CC")));
}

TEST_CASE("Hook leitet um und lässt sich entfernen")
{
    volatile Fn Target = &Plain;
    Fn Function = Target;
    CHECK_FALSE(Hooks::Install(Function, &Detour, &Original));

    REQUIRE(Hooks::Initialize());
    CHECK(Hooks::Initialize());
    REQUIRE(Hooks::Install(Function, &Detour, &Original));
    CHECK(Original != nullptr);
    CHECK(Target(2) == 107);
    CHECK(DetourCalls == 1);
    CHECK_FALSE(Hooks::Install(Function, &Detour, &Original));

    CHECK(Hooks::Remove(reinterpret_cast<void*>(Function)));
    CHECK(Target(2) == 7);
    CHECK(DetourCalls == 1);
    CHECK_FALSE(Hooks::Remove(reinterpret_cast<void*>(Function)));
    Hooks::Shutdown();
}
