#pragma once

#include <cstdint>

// opt-gegner-sound: Treffer- und Angriffssounds der Gegner beim Client.
// - Treffer (`zombie_Hurt`): Das ist der Einschlagsound des Projektils (AProjectile.HitImpactSoundEvent), gestartet in dessen
//   nativer Trefferbehandlung beim Host. Die replizierten Projektile des Clients treffen nur selten selbst (gemessen 13 von 35).
//   Der Host markiert darum in der vorhandenen Schadenszahl-Nachricht (0xA00) die Treffer, bei denen er den Sound spielt; der
//   Client spielt ihn dafür lokal und schaltet den seltenen eigenen Einschlagsound seiner replizierten Projektile ab.
// - Angriff (`StandardEnemyAttack`): Die Sprungfähigkeit läuft nur beim Host. Der Sprung selbst kommt nativ an
//   (ACharacter.ReplicatedMovementMode wechselt auf Falling), der Client spielt den Sound rein lokal bei diesem Wechsel.
// Nur Game Thread.
namespace Game
{
// Host: wahr, wenn Causer ein Projektil ist, das bei einem Treffer seinen Einschlagsound spielt.
bool ProjectileUsesImpactSound(uintptr_t Causer);
// Client: Einschlagsound am Ziel, ohne Ziel am gemeldeten Ort des Hosts. Zählt mit.
void PlayEnemyImpactSound(uintptr_t Target, uintptr_t WorldContext, float X, float Y, float Z);
}
