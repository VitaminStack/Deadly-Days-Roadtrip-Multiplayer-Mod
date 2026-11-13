#pragma once

#include <optional>
#include <span>
#include <vector>
#include "Game/WeaponSoundAdapter.h"

// opt-waffen-sound, Host -> Client: Nachladesounds eines Takts. Nutzlast: Anzahl (1, 1 bis 16), je Sound NetGUID der Hände (8),
// Länge des Soundnamens (1, 0 bis 64) und der Name (druckbares ASCII ohne Leerzeichen). Länge 0 heißt Standardsound der Hände.
// Ein verlorenes Paket heißt nur ein fehlender Sound, es gibt keine Wiederholung. Siehe docs/CONTROL_PROTOCOL.md, Abschnitt
// Nachladesound.
namespace Sync::WeaponSounds
{
inline constexpr uint16_t ReloadType = 0xB00;
inline constexpr uint64_t MaxGuid = 0x3FFFFFFFFFFFFFull;

inline bool Valid(const Game::ReloadSound& Sound)
{
    if (!Sound.Hands || Sound.Hands > MaxGuid || Sound.Override.size() > Game::MaxReloadSoundName)
        return false;
    for (const char Letter : Sound.Override)
        if (Letter <= ' ' || Letter > '~')
            return false;
    return true;
}

// Leer, wenn die Liste leer, zu lang oder ein Eintrag ungültig ist.
inline std::vector<uint8_t> Encode(std::span<const Game::ReloadSound> Sounds)
{
    if (Sounds.empty() || Sounds.size() > Game::MaxReloadSounds)
        return {};
    std::vector<uint8_t> Out;
    Out.push_back(static_cast<uint8_t>(Sounds.size()));
    for (const auto& Sound : Sounds)
    {
        if (!Valid(Sound))
            return {};
        for (unsigned i = 0; i < 8; ++i)
            Out.push_back(static_cast<uint8_t>(Sound.Hands >> (8 * i)));
        Out.push_back(static_cast<uint8_t>(Sound.Override.size()));
        Out.insert(Out.end(), Sound.Override.begin(), Sound.Override.end());
    }
    return Out;
}

inline std::optional<std::vector<Game::ReloadSound>> Decode(std::span<const uint8_t> In)
{
    if (In.empty() || !In[0] || In[0] > Game::MaxReloadSounds)
        return std::nullopt;
    std::vector<Game::ReloadSound> Sounds;
    size_t At = 1;
    for (size_t Index = 0; Index < In[0]; ++Index)
    {
        if (In.size() - At < 9)
            return std::nullopt;
        Game::ReloadSound Sound;
        for (unsigned i = 0; i < 8; ++i)
            Sound.Hands |= uint64_t(In[At + i]) << (8 * i);
        const size_t Length = In[At + 8];
        At += 9;
        if (In.size() - At < Length)
            return std::nullopt;
        Sound.Override.assign(In.begin() + At, In.begin() + At + Length);
        At += Length;
        if (!Valid(Sound))
            return std::nullopt;
        Sounds.push_back(std::move(Sound));
    }
    if (At != In.size())
        return std::nullopt;
    return Sounds;
}
}
