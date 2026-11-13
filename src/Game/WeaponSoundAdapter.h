#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// opt-waffen-sound: Nachladesound beim Client. Das Nachladen läuft nur in der Feuerschleife der Hände beim Host
// (AHands::FireIfPossible), die WeaponInstance repliziert nicht, der Client sieht davon nichts. Der Host meldet darum jeden Aufruf
// von AHands.PlayReloadSound mit den Händen (NetGUID) und dem Namen des Sounds, der Client ruft damit dieselbe native Funktion an
// seinen eigenen replizierten Händen auf: Ort, Standardsound und Abspielen bleiben lokal. Der Schusssound braucht keine Nachricht,
// siehe WeaponSoundPatches.cpp. Befund: docs/GAME_ANALYSIS.md#waffensounds-beim-client-opt-waffen-sound. Nur Game Thread.
namespace Game
{
struct ReloadSound
{
    uint64_t Hands = 0;   // NetGUID des Hände-Actors
    std::string Override; // Objektname des UFMODEvent (WeaponData.ReloadWeaponSoundEvent), leer: Standardsound der Hände
    bool operator==(const ReloadSound&) const = default;
};

inline constexpr size_t MaxReloadSounds = 16;
inline constexpr size_t MaxReloadSoundName = 64;

// Host: Nachladesounds seit dem letzten Aufruf mit auflösbarer NetGUID, höchstens MaxReloadSounds, der Rest verfällt.
std::vector<ReloadSound> TakeReloadSounds();
// Client: spielt sie über AHands.PlayReloadSound der eigenen Hände. Unbekannte Hände oder Sounds werden übersprungen.
void PlayReloadSounds(std::span<const ReloadSound> Sounds);
}
