#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "Game/PowerUpPolicy.h"

// opt-powerups-client: Power-up-Slots der Client-Figur. Der Host füllt und löst die Slots aller Figuren aus; der Client bucht
// keine Power-ups, zeigt den Stand des Hosts und meldet das Auslösen eines eigenen Slots. Little-Endian.
// 0xE00 Host -> Client, Stand der verbrauchbaren Slots der Figur des Empfängers: bestätigte Nummer (uint32), Anzahl (1, höchstens
//       Game::PowerUp::MaxSlots), je Slot Stufe (uint16), Namenslänge (1) und Name der PowerupDefinition.
// 0xE01 Client -> Host, ein lokal ausgelöster Slot: Nummer (uint32, ab 1), Namenslänge (1) und Name der PowerupDefinition.
// Siehe docs/CONTROL_PROTOCOL.md, Abschnitt Power-ups.
namespace Sync::PowerUps
{
inline constexpr uint16_t StateType = 0xE00, ActivateType = 0xE01;

struct State
{
    uint32_t Acked = 0;
    std::vector<Game::PowerUp::Slot> Slots;
    bool operator==(const State&) const = default;
};

struct Activation
{
    uint32_t Sequence = 0;
    std::string Definition;
    bool operator==(const Activation&) const = default;
};

namespace Detail
{
inline void Put(std::vector<uint8_t>& Out, uint32_t Value, unsigned Bytes)
{
    for (unsigned i = 0; i < Bytes; ++i)
        Out.push_back(static_cast<uint8_t>(Value >> (8 * i)));
}

inline bool PutName(std::vector<uint8_t>& Out, const std::string& Name)
{
    if (!Game::PowerUp::ValidName(Name))
        return false;
    Out.push_back(static_cast<uint8_t>(Name.size()));
    Out.insert(Out.end(), Name.begin(), Name.end());
    return true;
}

struct Reader
{
    std::span<const uint8_t> In;
    size_t At = 0;
    bool Failed = false;

    uint32_t Number(unsigned Bytes)
    {
        if (At > In.size() || In.size() - At < Bytes)
        {
            Failed = true;
            return 0;
        }
        uint32_t Value = 0;
        for (unsigned i = 0; i < Bytes; ++i)
            Value |= uint32_t(In[At + i]) << (8 * i);
        At += Bytes;
        return Value;
    }
    std::string Name()
    {
        const uint32_t Size = Number(1);
        if (Failed || In.size() - At < Size)
        {
            Failed = true;
            return {};
        }
        std::string Text(reinterpret_cast<const char*>(In.data() + At), Size);
        At += Size;
        if (!Game::PowerUp::ValidName(Text))
            Failed = true;
        return Text;
    }
};
}

// Leer, wenn der Stand ungültig ist.
inline std::vector<uint8_t> Encode(const State& Value)
{
    if (!Game::PowerUp::ValidSlots(Value.Slots))
        return {};
    std::vector<uint8_t> Out;
    Detail::Put(Out, Value.Acked, 4);
    Out.push_back(static_cast<uint8_t>(Value.Slots.size()));
    for (const Game::PowerUp::Slot& Entry : Value.Slots)
    {
        Detail::Put(Out, static_cast<uint32_t>(Entry.Level), 2);
        if (!Detail::PutName(Out, Entry.Definition))
            return {};
    }
    return Out;
}

inline std::optional<State> DecodeState(std::span<const uint8_t> In)
{
    Detail::Reader Read{In};
    State Value;
    Value.Acked = Read.Number(4);
    const uint32_t Count = Read.Number(1);
    if (Read.Failed || Count > Game::PowerUp::MaxSlots)
        return std::nullopt;
    for (uint32_t i = 0; i < Count; ++i)
    {
        Game::PowerUp::Slot Entry;
        Entry.Level = static_cast<int32_t>(Read.Number(2));
        Entry.Definition = Read.Name();
        if (Read.Failed)
            return std::nullopt;
        Value.Slots.push_back(std::move(Entry));
    }
    if (Read.At != In.size())
        return std::nullopt;
    return Value;
}

inline std::vector<uint8_t> Encode(const Activation& Value)
{
    if (Value.Sequence == 0)
        return {};
    std::vector<uint8_t> Out;
    Detail::Put(Out, Value.Sequence, 4);
    if (!Detail::PutName(Out, Value.Definition))
        return {};
    return Out;
}

inline std::optional<Activation> DecodeActivation(std::span<const uint8_t> In)
{
    Detail::Reader Read{In};
    Activation Value;
    Value.Sequence = Read.Number(4);
    Value.Definition = Read.Name();
    if (Read.Failed || Value.Sequence == 0 || Read.At != In.size())
        return std::nullopt;
    return Value;
}
}
