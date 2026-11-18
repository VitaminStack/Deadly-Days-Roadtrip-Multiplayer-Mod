#pragma once

#include <cstdint>

// client-items-wirkung: Welche Ausführungsrichtlinie einer Item-Fähigkeit der Host umstellt. Ohne Spielzugriff, testbar.
namespace Game::ItemAbility
{
// EGameplayAbilityNetExecutionPolicy der Engine.
constexpr uint8_t LocalPredicted = 0, LocalOnly = 1, ServerInitiated = 2, ServerOnly = 3;

// Wahr, wenn der Host die Fähigkeit eines Items auf ServerOnly stellt.
// - Nur LocalOnly: Für die Figur eines Netz-Clients läuft sie sonst allein beim Client, ohne Autorität. LocalPredicted reicht
//   die Engine über den Client zum Host zurück, sie läuft dort also schon (gemessen an GA_DelayedHeal_C) und behält beim Client
//   ihre Vorhersage. ServerInitiated, ServerOnly und unbekannte Werte bleiben unberührt.
// - Nur, was das Item selbst auslöst (beim Vergeben oder über seinen Auslöse-Effekt) und keinen Eingabe-Tag trägt.
constexpr bool MovesToHost(uint8_t Policy, bool ActivateOnGive, bool Triggerable, bool HasInputTag)
{
    return Policy == LocalOnly && (ActivateOnGive || Triggerable) && !HasInputTag;
}
}
