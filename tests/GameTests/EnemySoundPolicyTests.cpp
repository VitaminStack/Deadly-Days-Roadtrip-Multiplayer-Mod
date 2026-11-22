#include <doctest.h>

#include "Game/EnemySoundPolicy.h"

using namespace Game;

TEST_CASE("opt-gegner-sound: gepackter Bewegungsmodus, nur Falling zählt als Sprung")
{
    CHECK(PackedModeFalling(3));
    CHECK(PackedModeFalling(3 | 8)); // Bodenmodus NavWalking
    CHECK_FALSE(PackedModeFalling(1));
    CHECK_FALSE(PackedModeFalling(2 | 8));
    CHECK_FALSE(PackedModeFalling(0));
    CHECK_FALSE(PackedModeFalling(19)); // eigener Modus, untere Bits zufällig 3
}

TEST_CASE("opt-gegner-sound: Angriffssound beim Wechsel vom Boden in den Sprung, nie bei der ersten Beobachtung")
{
    CHECK(EnemyJumpStarted(true, 10, 3));
    CHECK(EnemyJumpStarted(true, 1, 11));
    CHECK_FALSE(EnemyJumpStarted(false, 10, 3)); // Spawn in der Luft, Beitritt mitten im Sprung
    CHECK_FALSE(EnemyJumpStarted(true, 3, 3));   // noch im selben Sprung
    CHECK_FALSE(EnemyJumpStarted(true, 3, 10));  // Landung
    CHECK_FALSE(EnemyJumpStarted(true, 10, 10));
}

TEST_CASE("opt-gegner-sound: Angriffssound nur beim Client, mit Ruhezeit je Gegner und Obergrenze je Tick")
{
    CHECK(EnemyAttackSoundDue(true, true, 0, 5000, 0));
    CHECK_FALSE(EnemyAttackSoundDue(false, true, 0, 5000, 0)); // Host spielt ihn in der Fähigkeit
    CHECK_FALSE(EnemyAttackSoundDue(true, false, 0, 5000, 0));
    CHECK_FALSE(EnemyAttackSoundDue(true, true, 5000, 5000 + EnemyAttackRepeatMs - 1, 0));
    CHECK(EnemyAttackSoundDue(true, true, 5000, 5000 + EnemyAttackRepeatMs, 0));
    CHECK(EnemyAttackSoundDue(true, true, 9000, 5000, 0)); // Uhr zurück: nicht unterdrücken
    CHECK(EnemyAttackSoundDue(true, true, 0, 5000, EnemySoundsPerTick - 1));
    CHECK_FALSE(EnemyAttackSoundDue(true, true, 0, 5000, EnemySoundsPerTick));
}

TEST_CASE("opt-gegner-sound: Host markiert nur Projektiltreffer mit Einschlagsound an Gegnern, je Projektil und Takt einmal")
{
    CHECK(ImpactSoundReported(true, true, true, true, false));
    CHECK_FALSE(ImpactSoundReported(false, true, true, true, false)); // Treffer an Spieler oder Kiste
    CHECK_FALSE(ImpactSoundReported(true, false, true, true, false)); // Nahkampf, Dauerschaden
    CHECK_FALSE(ImpactSoundReported(true, true, false, true, false)); // bUseImpactSounds aus (Granate)
    CHECK_FALSE(ImpactSoundReported(true, true, true, false, false)); // kein Ereignis gesetzt
    CHECK_FALSE(ImpactSoundReported(true, true, true, true, true));   // Flächenschaden: zweiter Treffer desselben Projektils
}
