#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace Core
{
// Je Byte ein Wert, nichts steht für ein beliebiges Byte.
using Pattern = std::vector<std::optional<uint8_t>>;

// "48 8B ?? 05", Leerzeichen als Trenner, ? oder ?? als Platzhalter.
// Ungültiger Text und ein Muster nur aus Platzhaltern ergeben nichts.
std::optional<Pattern> ParsePattern(std::string_view Text);

// Position des einzigen Treffers. Bei keinem oder mehreren Treffern nichts, auch bei überlappenden.
std::optional<size_t> FindUnique(std::span<const uint8_t> Bytes, const Pattern& Needle);

// Wie FindUnique über alle ausführbaren Sektionen eines geladenen Moduls, nullptr ohne eindeutigen Treffer.
const void* FindUniqueInImage(const void* ImageBase, const Pattern& Needle);
}
