#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

// client-roboterarm: Vorschau der zusätzlichen Hände beim Client während einer lokalen Inventarbearbeitung. Ohne Spielzugriff, testbar.
//
// Der Roboterarm gibt seine Hand als GameplayEffect (GE_IncreaseHands_C, ExtraHands additiv). Den legt nur der Host an, beim
// Übernehmen des Edits. Bis dahin sähe das Panel des Clients keine zusätzliche Hand und lehnte die Waffe ab, für die der
// Spieler den Arm gerade abgelegt hat. Der Client rechnet darum aus den Item-Daten seines lokalen Rucksacks vor. Verbindlich
// bleibt der Host: Nach dem Übernehmen gilt sein replizierter Wert, bei Ablehnung der Ausgangswert.
namespace Game::HandPreview
{
// EGameplayModOp::Additive, EGameplayEffectMagnitudeCalculation::ScalableFloat der Engine.
constexpr uint8_t OpAdditive = 0, CalculationScalableFloat = 0;

// Stufe, mit der das Item seinen GameplayEffect anlegt (FItemEffectWrapperBase). Leer, wenn sie sich nicht aus den Item-Daten
// ergibt: Dann gibt es für Effekte mit Kurve keine Vorschau.
// - ForceScalingGameplayEffectLevel: Wert der ScalingGameplayEffectLevel-Daten für die Seltenheit des Items (beim Roboterarm
//   FD_RobotArm_Hands). Fehlen die Daten oder hängen sie an der Spielerstufe, ist die Stufe unbekannt.
// - sonst ForceGameplayEffectLevel: die feste GameplayEffectLevel.
constexpr std::optional<float> EffectLevel(bool ForceScaling, std::optional<float> ScalingValue, bool ForceFixed, int32_t FixedLevel)
{
    if (ForceScaling)
        return ScalingValue;
    if (ForceFixed)
        return static_cast<float>(FixedLevel);
    return std::nullopt;
}

// Hände, die ein Modifikator eines GameplayEffects sicher gibt. Factor ist der Wert der Kurve für die Stufe des Effekts, 1 ohne
// Kurve, leer, wenn er sich nicht bestimmen ließ. 0 für alles, was sich nicht ohne das AbilitySystem des Hosts ausrechnen lässt
// (anderes Attribut, andere Operation, berechnete Größe): Dann bleibt es beim Wert des Hosts.
constexpr float ExtraHandsOfModifier(std::string_view Attribute, uint8_t Operation, uint8_t Calculation, float Value, std::optional<float> Factor)
{
    if (Attribute != "ExtraHands" || Operation != OpAdditive || Calculation != CalculationScalableFloat || !Factor)
        return 0.0f;
    const float Hands = Value * *Factor;
    return Hands > 0.0f && Hands <= 16.0f ? Hands : 0.0f;
}

// Angezeigter Wert: Ausgangswert des Hosts plus die Änderung im lokalen Rucksack seit dem Beginn der Bearbeitung.
constexpr float Preview(float Start, float GrantedAtStart, float GrantedNow)
{
    return (std::max)(0.0f, Start + GrantedNow - GrantedAtStart);
}
}
