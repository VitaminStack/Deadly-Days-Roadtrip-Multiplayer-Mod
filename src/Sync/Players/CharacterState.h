#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// charakter-wahl: Name der eigenen Figurendefinition über den Kontrollkanal, in beide Richtungen. Jede Seite schickt ihn bei
// Änderung und sonst alle zwei Sekunden, damit ein verlorenes Paket, ein Beitritt oder eine Reise ihn nachholt. Nutzlast:
// der Name als ASCII ohne Länge und Abschluss, etwa "CD_BananaPerson". Nur Buchstaben, Ziffern und Unterstrich.
namespace Sync::Character
{
inline constexpr uint16_t ChoiceType = 0x800;
inline constexpr size_t MaxName = 64;

inline bool ValidName(std::string_view Name)
{
    if (Name.empty() || Name.size() > MaxName)
        return false;
    for (const char Letter : Name)
        if (!((Letter >= 'a' && Letter <= 'z') || (Letter >= 'A' && Letter <= 'Z') || (Letter >= '0' && Letter <= '9') || Letter == '_'))
            return false;
    return true;
}

// Leer, wenn der Name nicht gültig ist.
inline std::vector<uint8_t> Encode(std::string_view Name)
{
    if (!ValidName(Name))
        return {};
    return {Name.begin(), Name.end()};
}

inline std::optional<std::string> Decode(std::span<const uint8_t> In)
{
    std::string Name(In.begin(), In.end());
    if (!ValidName(Name))
        return std::nullopt;
    return Name;
}
}
