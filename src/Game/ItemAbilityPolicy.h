#pragma once

#include <cstdint>

// client-items-wirkung: Welche Ausführungsrichtlinie einer Item-Fähigkeit der Host umstellt. Ohne Spielzugriff, testbar.
namespace Game::ItemAbility
{
// EGameplayAbilityNetExecutionPolicy der Engine.
constexpr uint8_t LocalPredicted = 0, LocalOnly = 1, ServerInitiated = 2, ServerOnly = 3;

// Wahr, wenn die Engine die Aktivierung für die Figur eines Netz-Clients nur an den Client weiterreicht statt sie beim Host
// auszuführen. ServerInitiated und ServerOnly laufen schon beim Host, unbekannte Werte bleiben unberührt.
constexpr bool RunsOnlyForLocalOwner(uint8_t Policy)
{
    return Policy == LocalPredicted || Policy == LocalOnly;
}

// Wahr, wenn der Host die Fähigkeit eines Items auf ServerOnly stellt: Sie liefe sonst nicht beim Host, und das Item löst sie
// selbst aus (beim Vergeben oder über seinen Auslöse-Effekt). Fähigkeiten, die eine Eingabe oder eine andere Fähigkeit startet,
// bleiben beim Client vorhersagbar.
constexpr bool MovesToHost(uint8_t Policy, bool ActivateOnGive, bool Triggerable, bool HasInputTag)
{
    return RunsOnlyForLocalOwner(Policy) && (ActivateOnGive || Triggerable) && !HasInputTag;
}
}
