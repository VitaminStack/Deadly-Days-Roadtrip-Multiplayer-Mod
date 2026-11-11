#pragma once

#include <bit>
#include <cmath>
#include <optional>
#include <span>
#include <vector>
#include "Game/DamageNumberAdapter.h"

// opt-schadenszahlen, Host -> Client: die Treffer eines Takts, für die der Host seinen Schadenszahl-Handler aufgerufen hat.
// Nutzlast: Anzahl (1, 1 bis 64), je Treffer NetGUID (8), Schaden (float, 4), Flags (1), Ort X, Y, Z (je float, 4), also
// 1 + 25 * Anzahl Bytes. Ein verlorenes Paket heißt nur fehlende Zahlen, es gibt keine Wiederholung. Siehe
// docs/CONTROL_PROTOCOL.md, Abschnitt Schadenszahlen.
namespace Sync::DamageNumbers
{
inline constexpr uint16_t HitsType = 0xA00;
inline constexpr size_t HitBytes = 25;
inline constexpr uint64_t MaxGuid = 0x3FFFFFFFFFFFFFull;
inline constexpr float MaxDamage = 1.0e9f;
inline constexpr float MaxCoordinate = 1.0e7f;

inline bool Valid(const Game::DamageNumberHit& Hit)
{
    const auto Coordinate = [](float Value) { return std::isfinite(Value) && std::fabs(Value) <= MaxCoordinate; };
    return Hit.Guid && Hit.Guid <= MaxGuid && std::isfinite(Hit.Damage) && Hit.Damage >= 0.f && Hit.Damage <= MaxDamage &&
           Coordinate(Hit.X) && Coordinate(Hit.Y) && Coordinate(Hit.Z);
}

// Leer, wenn die Liste leer, zu lang oder ein Treffer ungültig ist.
inline std::vector<uint8_t> Encode(std::span<const Game::DamageNumberHit> Hits)
{
    if (Hits.empty() || Hits.size() > Game::MaxDamageNumberHits)
        return {};
    std::vector<uint8_t> Out;
    Out.reserve(1 + HitBytes * Hits.size());
    const auto Put = [&Out](uint64_t Bits, unsigned Bytes) {
        for (unsigned i = 0; i < Bytes; ++i)
            Out.push_back(static_cast<uint8_t>(Bits >> (8 * i)));
    };
    Put(Hits.size(), 1);
    for (const auto& Hit : Hits)
    {
        if (!Valid(Hit))
            return {};
        Put(Hit.Guid, 8);
        Put(std::bit_cast<uint32_t>(Hit.Damage), 4);
        Put(Hit.Flags, 1);
        Put(std::bit_cast<uint32_t>(Hit.X), 4);
        Put(std::bit_cast<uint32_t>(Hit.Y), 4);
        Put(std::bit_cast<uint32_t>(Hit.Z), 4);
    }
    return Out;
}

inline std::optional<std::vector<Game::DamageNumberHit>> Decode(std::span<const uint8_t> In)
{
    if (In.empty())
        return std::nullopt;
    const size_t Count = In[0];
    if (!Count || Count > Game::MaxDamageNumberHits || In.size() != 1 + HitBytes * Count)
        return std::nullopt;
    const auto Get = [In](size_t At, unsigned Bytes) {
        uint64_t Value = 0;
        for (unsigned i = 0; i < Bytes; ++i)
            Value |= uint64_t(In[At + i]) << (8 * i);
        return Value;
    };
    const auto Float = [&Get](size_t At) { return std::bit_cast<float>(static_cast<uint32_t>(Get(At, 4))); };
    std::vector<Game::DamageNumberHit> Hits;
    Hits.reserve(Count);
    for (size_t Index = 0; Index < Count; ++Index)
    {
        const size_t At = 1 + HitBytes * Index;
        const Game::DamageNumberHit Hit{Get(At, 8), Float(At + 8), static_cast<uint8_t>(Get(At + 12, 1)), Float(At + 13),
                                        Float(At + 17), Float(At + 21)};
        if (!Valid(Hit))
            return std::nullopt;
        Hits.push_back(Hit);
    }
    return Hits;
}
}
