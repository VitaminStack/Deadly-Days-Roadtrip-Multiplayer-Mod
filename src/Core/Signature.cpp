#include "Core/Signature.h"

#include <windows.h>

#include <algorithm>
#include <charconv>

namespace Core
{
namespace
{
bool Matches(const uint8_t* Bytes, const Pattern& Needle)
{
    for (size_t Index = 0; Index < Needle.size(); ++Index)
    {
        if (Needle[Index] && *Needle[Index] != Bytes[Index])
            return false;
    }
    return true;
}

// Zählt bis höchstens zwei, mehr muss niemand wissen. First bleibt der erste Treffer.
int CountMatches(std::span<const uint8_t> Bytes, const Pattern& Needle, size_t& First)
{
    if (Needle.empty() || Bytes.size() < Needle.size())
        return 0;
    int Count = 0;
    const size_t Last = Bytes.size() - Needle.size();
    for (size_t Offset = 0; Offset <= Last && Count < 2; ++Offset)
    {
        if (Matches(Bytes.data() + Offset, Needle) && Count++ == 0)
            First = Offset;
    }
    return Count;
}
}

std::optional<Pattern> ParsePattern(std::string_view Text)
{
    Pattern Result;
    while (!Text.empty())
    {
        const size_t Start = Text.find_first_not_of(' ');
        if (Start == std::string_view::npos)
            break;
        Text.remove_prefix(Start);
        const std::string_view Token = Text.substr(0, Text.find(' '));
        Text.remove_prefix(Token.size());

        if (Token == "?" || Token == "??")
        {
            Result.emplace_back(std::nullopt);
            continue;
        }
        uint8_t Value = 0;
        const auto [End, Error] = std::from_chars(Token.data(), Token.data() + Token.size(), Value, 16);
        if (Token.size() != 2 || Error != std::errc{} || End != Token.data() + Token.size())
            return std::nullopt;
        Result.emplace_back(Value);
    }
    if (std::ranges::none_of(Result, [](const auto& Byte) { return Byte.has_value(); }))
        return std::nullopt;
    return Result;
}

std::optional<size_t> FindUnique(std::span<const uint8_t> Bytes, const Pattern& Needle)
{
    size_t First = 0;
    if (CountMatches(Bytes, Needle, First) != 1)
        return std::nullopt;
    return First;
}

const void* FindUniqueInImage(const void* ImageBase, const Pattern& Needle)
{
    if (!ImageBase)
        return nullptr;
    const auto Base = static_cast<const uint8_t*>(ImageBase);
    const auto Dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(Base);
    if (Dos->e_magic != IMAGE_DOS_SIGNATURE)
        return nullptr;
    const auto Nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(Base + Dos->e_lfanew);
    if (Nt->Signature != IMAGE_NT_SIGNATURE)
        return nullptr;

    const uint8_t* Found = nullptr;
    const IMAGE_SECTION_HEADER* Section = IMAGE_FIRST_SECTION(Nt);
    for (WORD Index = 0; Index < Nt->FileHeader.NumberOfSections; ++Index, ++Section)
    {
        if (!(Section->Characteristics & IMAGE_SCN_MEM_EXECUTE))
            continue;
        const std::span Bytes(Base + Section->VirtualAddress, Section->Misc.VirtualSize);
        size_t First = 0;
        const int Count = CountMatches(Bytes, Needle, First);
        if (Count > 1 || (Count == 1 && Found))
            return nullptr;
        if (Count == 1)
            Found = Bytes.data() + First;
    }
    return Found;
}
}
