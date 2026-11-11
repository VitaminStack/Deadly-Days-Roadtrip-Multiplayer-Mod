#pragma once

#include <cstdint>

// opt-blut: Regeln der Tod-Effekte beim Client, ohne Spielzugriff und damit testbar. Der Host spielt Blut und Fleisch in seiner
// Sterbe-Ability, der Client spielt sie lokal, sobald der native zuverlässige Multicast Multi_OnDie eines Gegners ankommt.
namespace Game
{
// Obergrenze je Mod-Tick gegen Wellenlast; überzählige Tode bleiben ohne Effekt (rein kosmetisch).
inline constexpr int BloodMaxPerTick = 16;
// Derselbe Gegner innerhalb dieser Zeit gilt als doppelte Zustellung. Gepoolte Gegner sterben später erneut und bekommen dann
// wieder Effekte, darum kein dauerhaftes „einmal je Objekt“.
inline constexpr uint64_t BloodRepeatMs = 1000;

// Wahr, wenn derselbe Gegner zuletzt LastMs (0: nie) und damit eben schon Effekte bekam.
constexpr bool BloodRecent(uint64_t LastMs, uint64_t NowMs)
{
    return LastMs != 0 && NowMs >= LastMs && NowMs - LastMs < BloodRepeatMs;
}

// Wahr, wenn der Client die nativen Tod-Effekte eines Gegners jetzt lokal abspielen soll. Der Host nie, er hat sie schon.
constexpr bool BloodShouldPlay(bool Client, bool Enemy, bool Alive, bool Recent, int PlayedThisTick)
{
    return Client && Enemy && Alive && !Recent && PlayedThisTick < BloodMaxPerTick;
}
}
