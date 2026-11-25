#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Game/CharacterAdapter.h"
#include "Game/RemoteP2Policy.h"

// charakter-wahl: Namen der gewählten Figurendefinitionen über den Kontrollkanal, in beide Richtungen. Jede Seite schickt bei
// Änderung und sonst alle zwei Sekunden, damit ein verlorenes Paket, ein Beitritt oder eine Reise sie nachholt.
// Nutzlast: Liste von Einträgen, je Eintrag PlayerId (int32, little endian, APlayerState::PlayerId), Namenslänge (uint8) und
// der Name als ASCII, etwa "CD_BananaPerson". Nur Buchstaben, Ziffern und Unterstrich, 1 bis MaxPlayers Einträge.
// Client an Host: ein Eintrag, die eigene Wahl. Die PlayerId darin wertet der Host nicht aus, er nimmt die des Controllers
// hinter dem Peer (Game/RemotePlayers.h). Host an Client: die Wahl jedes anderen Spielers mit dessen PlayerId.
namespace Sync::Character
{
inline constexpr uint16_t ChoiceType = 0x800;
inline constexpr size_t MaxName = 64;
using Game::Characters::PlayerChoice;

inline bool ValidName(std::string_view Name)
{
    if (Name.empty() || Name.size() > MaxName)
        return false;
    for (const char Letter : Name)
        if (!((Letter >= 'a' && Letter <= 'z') || (Letter >= 'A' && Letter <= 'Z') || (Letter >= '0' && Letter <= '9') || Letter == '_'))
            return false;
    return true;
}

// Leer, wenn die Liste leer, zu lang oder ein Name ungültig ist.
inline std::vector<uint8_t> Encode(std::span<const PlayerChoice> Choices)
{
    if (Choices.empty() || Choices.size() > static_cast<size_t>(Game::MaxPlayers))
        return {};
    std::vector<uint8_t> Out;
    for (const auto& Choice : Choices)
    {
        if (!ValidName(Choice.Name))
            return {};
        const auto Id = static_cast<uint32_t>(Choice.PlayerId);
        for (int Shift = 0; Shift < 32; Shift += 8)
            Out.push_back(static_cast<uint8_t>(Id >> Shift));
        Out.push_back(static_cast<uint8_t>(Choice.Name.size()));
        Out.insert(Out.end(), Choice.Name.begin(), Choice.Name.end());
    }
    return Out;
}

inline std::optional<std::vector<PlayerChoice>> Decode(std::span<const uint8_t> In)
{
    std::vector<PlayerChoice> Choices;
    size_t At = 0;
    while (At < In.size())
    {
        if (In.size() - At < 5 || Choices.size() == static_cast<size_t>(Game::MaxPlayers))
            return std::nullopt;
        uint32_t Id = 0;
        for (int i = 0; i < 4; ++i)
            Id |= static_cast<uint32_t>(In[At + i]) << (8 * i);
        const size_t Length = In[At + 4];
        At += 5;
        if (In.size() - At < Length)
            return std::nullopt;
        PlayerChoice Choice{static_cast<int32_t>(Id), std::string(In.begin() + At, In.begin() + At + Length)};
        if (!ValidName(Choice.Name))
            return std::nullopt;
        At += Length;
        Choices.push_back(std::move(Choice));
    }
    if (Choices.empty())
        return std::nullopt;
    return Choices;
}
}
