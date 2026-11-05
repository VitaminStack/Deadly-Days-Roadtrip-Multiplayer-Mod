#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace Game::Desync
{
enum Field : unsigned { Day, Phase, Players, Health, Enemies, Inventory, Count };
inline constexpr uint8_t All = (1u << Count) - 1;
constexpr uint8_t Bit(unsigned Index) { return static_cast<uint8_t>(1u << Index); }
inline constexpr std::array<std::string_view, Count> Names{"Tag", "Phase", "Spieler", "HP", "Gegner", "Inventar"};
// EObjectFlags, NICHT die internen GUObjectArray-Flags: 0x00200000 ist LoadCompleted.
inline constexpr uint32_t ExcludedObjectFlags = 0x30u | 0x8000u | 0x10000u | 0x40000000u;
constexpr bool LiveObjectFlags(uint32_t Flags) { return !(Flags & ExcludedObjectFlags); }
struct Sample
{
    uint8_t Known = 0;
    std::array<uint64_t, Count> Values{};
    bool operator==(const Sample&) const = default;
};

// Explizite Bytes, keine Adressen, Struktur-Padding oder lokalen FName-Indizes hashen.
class Hash
{
public:
    void Number(uint64_t Value)
    {
        for (unsigned i = 0; i < 8; ++i)
        {
            ValueHash ^= static_cast<uint8_t>(Value >> (8 * i));
            ValueHash *= 1099511628211ull;
        }
    }
    void Text(std::string_view Value)
    {
        Number(Value.size());
        for (const unsigned char Byte : Value)
        {
            ValueHash ^= Byte;
            ValueHash *= 1099511628211ull;
        }
    }
    uint64_t Value() const { return ValueHash; }
private:
    uint64_t ValueHash = 14695981039346656037ull;
};
inline uint64_t Multiset(std::vector<uint64_t> Values)
{
    std::sort(Values.begin(), Values.end());
    Hash Out;
    Out.Number(Values.size());
    for (const auto Value : Values) Out.Number(Value);
    return Out.Value();
}
}
