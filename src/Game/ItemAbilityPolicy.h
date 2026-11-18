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
}
