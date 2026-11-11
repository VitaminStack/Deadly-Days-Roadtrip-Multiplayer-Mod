#pragma once

#include <bit>
#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "Game/TelegraphAdapter.h"

// Host -> Client: Warnanzeige eines Gegnerangriffs (opt-gegner-indikatoren, Mod-Version 11). Nur der Auslöser, die Anzeige
// baut der Client mit dem nativen Niagara-System selbst auf. Siehe docs/CONTROL_PROTOCOL.md, Abschnitt Warnanzeigen der Gegner.
namespace Sync::Telegraph
{
inline constexpr uint16_t Type = 0x900;
// Art (1), Gegner (8), Ort (3 x 8), Delay, Lifetime, Size, Percentage (je 4), dann vier Texte mit je einem Längenbyte.
inline constexpr size_t FixedBytes = 1 + 8 + 24 + 16;
inline constexpr size_t MaxText = 200;
// Weltkoordinaten der Karten liegen weit darunter, die Grenze fängt nur kaputte Werte ab.
inline constexpr double MaxCoordinate = 1.0e7;
inline constexpr float MaxSeconds = 600.0f;

inline bool ValidText(const std::string& Text)
{
    if (Text.size() > MaxText)
        return false;
    for (const char Char : Text)
        if (Char < 0x20 || Char > 0x7E)
            return false;
    return true;
}

inline bool Valid(const Game::EnemyTelegraph& Value)
{
    using Game::TelegraphKind;
    if (Value.Kind < TelegraphKind::Circle || Value.Kind > TelegraphKind::LineEnd)
        return false;
    for (const double Coordinate : {Value.X, Value.Y, Value.Z})
        if (!std::isfinite(Coordinate) || std::abs(Coordinate) > MaxCoordinate)
            return false;
    for (const float Number : {Value.Delay, Value.Lifetime, Value.Size, Value.Percentage})
        if (!std::isfinite(Number) || std::abs(Number) > MaxSeconds * 100.0f)
            return false;
    if (!ValidText(Value.System) || !ValidText(Value.SizeName) || !ValidText(Value.LifetimeName) || !ValidText(Value.DelayName))
        return false;
    if (Value.Kind == TelegraphKind::Circle)
        return !Value.System.empty() && Value.Delay >= 0 && Value.Delay <= MaxSeconds && Value.Lifetime >= 0 &&
               Value.Lifetime <= MaxSeconds;
    // Der Strich hängt am Gegner und trägt keine Texte.
    return Value.Enemy != 0 && Value.System.empty() && Value.SizeName.empty() && Value.LifetimeName.empty() &&
           Value.DelayName.empty();
}

inline std::vector<uint8_t> Encode(const Game::EnemyTelegraph& Value)
{
    if (!Valid(Value))
        return {};
    std::vector<uint8_t> Out;
    const auto Put = [&Out](uint64_t Bits, unsigned Bytes) {
        for (unsigned i = 0; i < Bytes; ++i)
            Out.push_back(static_cast<uint8_t>(Bits >> (8 * i)));
    };
    Put(static_cast<uint8_t>(Value.Kind), 1);
    Put(Value.Enemy, 8);
    for (const double Coordinate : {Value.X, Value.Y, Value.Z})
        Put(std::bit_cast<uint64_t>(Coordinate), 8);
    for (const float Number : {Value.Delay, Value.Lifetime, Value.Size, Value.Percentage})
        Put(std::bit_cast<uint32_t>(Number), 4);
    for (const std::string* Text : {&Value.System, &Value.SizeName, &Value.LifetimeName, &Value.DelayName})
    {
        Put(Text->size(), 1);
        Out.insert(Out.end(), Text->begin(), Text->end());
    }
    return Out;
}

inline std::optional<Game::EnemyTelegraph> Decode(std::span<const uint8_t> In)
{
    if (In.size() < FixedBytes + 4)
        return std::nullopt;
    size_t At = 0;
    const auto Get = [In, &At](unsigned Bytes) {
        uint64_t Value = 0;
        for (unsigned i = 0; i < Bytes; ++i)
            Value |= uint64_t(In[At + i]) << (8 * i);
        At += Bytes;
        return Value;
    };
    Game::EnemyTelegraph Value;
    Value.Kind = static_cast<Game::TelegraphKind>(Get(1));
    Value.Enemy = Get(8);
    Value.X = std::bit_cast<double>(Get(8));
    Value.Y = std::bit_cast<double>(Get(8));
    Value.Z = std::bit_cast<double>(Get(8));
    Value.Delay = std::bit_cast<float>(static_cast<uint32_t>(Get(4)));
    Value.Lifetime = std::bit_cast<float>(static_cast<uint32_t>(Get(4)));
    Value.Size = std::bit_cast<float>(static_cast<uint32_t>(Get(4)));
    Value.Percentage = std::bit_cast<float>(static_cast<uint32_t>(Get(4)));
    for (std::string* Text : {&Value.System, &Value.SizeName, &Value.LifetimeName, &Value.DelayName})
    {
        if (At >= In.size())
            return std::nullopt;
        const size_t Length = static_cast<size_t>(Get(1));
        if (Length > In.size() - At)
            return std::nullopt;
        Text->assign(reinterpret_cast<const char*>(In.data() + At), Length);
        At += Length;
    }
    if (At != In.size())
        return std::nullopt;
    return Valid(Value) ? std::optional(Value) : std::nullopt;
}
}
