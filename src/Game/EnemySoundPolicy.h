#pragma once

#include <cstdint>

// opt-gegner-sound: Regeln für Treffer- und Angriffssounds der Gegner beim Client, ohne Spielzugriff und damit testbar.
// Befund: docs/GAME_ANALYSIS.md#gegner-sounds-beim-client-opt-gegner-sound
namespace Game
{
// Obergrenze je Mod-Tick gegen Wellenlast; überzählige Sounds bleiben aus (rein kosmetisch).
inline constexpr int EnemySoundsPerTick = 12;
// Derselbe Gegner startet seinen Angriffssound frühestens nach dieser Zeit erneut (Flattern des replizierten Zustands).
inline constexpr uint64_t EnemyAttackRepeatMs = 300;
// ACharacter.ReplicatedMovementMode ist gepackt (PackNetworkMovementMode): unter 16 stehen in den unteren drei Bits der
// Bewegungsmodus, Bit 3 der Bodenmodus; ab 16 folgen eigene Modi. MOVE_Falling ist 3.
inline constexpr uint8_t MovementFalling = 3;

constexpr bool PackedModeFalling(uint8_t Packed)
{
    return Packed < 16 && (Packed & 7) == MovementFalling;
}

// Host: Dieser Treffer bekommt die Marke „Einschlagsound“, wenn ihn ein Projektil mit eingeschaltetem Einschlagsound an einem
// Gegner verursacht hat. Ein Projektil mit Flächenschaden meldet je Takt mehrere Treffer, spielt den Sound aber einmal.
constexpr bool ImpactSoundReported(bool TargetIsEnemy, bool CauserIsProjectile, bool UsesImpactSounds, bool HasSoundEvent,
                                   bool CauserMarkedThisTick)
{
    return TargetIsEnemy && CauserIsProjectile && UsesImpactSounds && HasSoundEvent && !CauserMarkedThisTick;
}

// Client: Sprungangriff beginnt, wenn ein schon beobachteter Gegner vom Boden in MOVE_Falling wechselt. Die erste Beobachtung
// zählt nie (Spawn in der Luft, Beitritt mitten im Sprung).
constexpr bool EnemyJumpStarted(bool SeenBefore, uint8_t PreviousPacked, uint8_t NowPacked)
{
    return SeenBefore && !PackedModeFalling(PreviousPacked) && PackedModeFalling(NowPacked);
}

constexpr bool EnemyAttackSoundDue(bool Client, bool JumpStarted, uint64_t LastMs, uint64_t NowMs, int PlayedThisTick)
{
    const bool Recent = LastMs != 0 && NowMs >= LastMs && NowMs - LastMs < EnemyAttackRepeatMs;
    return Client && JumpStarted && !Recent && PlayedThisTick < EnemySoundsPerTick;
}
}
