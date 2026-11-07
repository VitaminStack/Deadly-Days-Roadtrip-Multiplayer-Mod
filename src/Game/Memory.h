#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>

// Lesender Zugriff auf Spielspeicher. Jeder Zugriff fängt Zugriffsfehler ab, ein veralteter Zeiger kostet
// also nur einen Wert, nie das Spiel. Nur auf dem Game Thread aufrufen, sonst kann sich das Objekt beim Lesen ändern.
namespace Game::Memory
{
// Image-Base der laufenden Exe.
uintptr_t ImageBase();

// Absolute Adresse zu einer RVA des Referenz-Builds, für den laufenden Build umgerechnet (Game/AddressMap.h).
// 0, wenn die Adresse im laufenden Build unbekannt ist; das wird einmal je RVA protokolliert.
uintptr_t Rva(uintptr_t ReferenceRva);

// Kopiert Size Bytes von Address nach Out. Falsch bei Address 0 oder einem Zugriffsfehler.
bool ReadBytes(uintptr_t Address, void* Out, size_t Size);

template <typename T> std::optional<T> Read(uintptr_t Address)
{
    static_assert(std::is_trivially_copyable_v<T>);
    T Value{};
    if (!ReadBytes(Address, &Value, sizeof(T)))
        return std::nullopt;
    return Value;
}

// Schreibt Size Bytes nach Address. Falsch bei Address 0 oder einem Zugriffsfehler. Nur für Werte in Objekten,
// nie für Code: Seitenschutz wird nicht geändert.
bool WriteBytes(uintptr_t Address, const void* Data, size_t Size);

// Zeiger an Address, nullopt bei Lesefehler oder Nullzeiger.
std::optional<uintptr_t> ReadPointer(uintptr_t Address);

// Name eines FName-Index aus der Namenstabelle, nur ANSI-Namen. Leer, wenn nicht lesbar.
std::string NameFromIndex(int32_t Index);

// Objektname eines UObject.
std::string NameOf(uintptr_t Object);
}
