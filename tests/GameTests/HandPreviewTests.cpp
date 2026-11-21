#include <doctest.h>

#include "Game/HandPreview.h"

using namespace Game;

TEST_CASE("client-roboterarm: nur ein additiver Modifikator auf ExtraHands mit bekannter Größe zählt für die Vorschau")
{
    // GE_IncreaseHands_C des Roboterarms: ExtraHands, Additive, ScalableFloat 1.0 mal Kurve CT_Occupy_Hands (Stufe 1 gibt 1).
    CHECK(HandPreview::ExtraHandsOfModifier("ExtraHands", 0, 0, 1.0f, 1.0f) == 1.0f);
    CHECK(HandPreview::ExtraHandsOfModifier("ExtraHands", 0, 0, 1.0f, 2.0f) == 2.0f); // höhere Stufe
    CHECK(HandPreview::ExtraHandsOfModifier("ExtraHands", 0, 0, 2.0f, 1.0f) == 2.0f);
    // GE_Occupy_Hands_C: anderes Attribut, berechnete Größe (MMC_OccupyHandsCalculation).
    CHECK(HandPreview::ExtraHandsOfModifier("OccupiedHands", 0, 2, 0.0f, 1.0f) == 0.0f);
    CHECK(HandPreview::ExtraHandsOfModifier("ExtraHands", 1, 0, 2.0f, 1.0f) == 0.0f); // multiplikativ
    CHECK(HandPreview::ExtraHandsOfModifier("ExtraHands", 0, 3, 1.0f, 1.0f) == 0.0f); // SetByCaller
    CHECK(HandPreview::ExtraHandsOfModifier("ExtraHands", 0, 0, 1.0f, std::nullopt) == 0.0f); // Kurve nicht auswertbar
    CHECK(HandPreview::ExtraHandsOfModifier("ExtraHands", 0, 0, -1.0f, 1.0f) == 0.0f);
    CHECK(HandPreview::ExtraHandsOfModifier("ExtraHands", 0, 0, 1000.0f, 1.0f) == 0.0f); // unlesbarer Speicher
    CHECK(HandPreview::ExtraHandsOfModifier("", 0, 0, 1.0f, 1.0f) == 0.0f);
}

TEST_CASE("client-roboterarm: die Stufe des Effekts kommt aus den Item-Daten oder bleibt unbekannt")
{
    // Roboterarm: ForceScalingGameplayEffectLevel mit FD_RobotArm_Hands (1.0 für die Seltenheit des Items).
    CHECK(HandPreview::EffectLevel(true, 1.0f, true, 1) == 1.0f);
    CHECK(HandPreview::EffectLevel(true, 2.0f, false, 1) == 2.0f);
    CHECK_FALSE(HandPreview::EffectLevel(true, std::nullopt, true, 1).has_value()); // Daten fehlen oder hängen an der Spielerstufe
    CHECK(HandPreview::EffectLevel(false, std::nullopt, true, 3) == 3.0f);
    CHECK_FALSE(HandPreview::EffectLevel(false, 2.0f, false, 3).has_value()); // Stufe legt erst der Host fest
}

TEST_CASE("client-roboterarm: die Vorschau folgt dem lokalen Rucksack und nimmt sich wieder zurück")
{
    CHECK(HandPreview::Preview(0.0f, 0.0f, 1.0f) == 1.0f); // Arm abgelegt
    CHECK(HandPreview::Preview(1.0f, 1.0f, 1.0f) == 1.0f); // Arm lag schon, nichts geändert
    CHECK(HandPreview::Preview(1.0f, 1.0f, 0.0f) == 0.0f); // Arm herausgenommen
    CHECK(HandPreview::Preview(1.0f, 1.0f, 2.0f) == 2.0f); // zweiter Arm
    // Der Host hat mehr Hände als die Items erklären (Figur, Upgrade): Der Überschuss bleibt.
    CHECK(HandPreview::Preview(2.0f, 1.0f, 0.0f) == 1.0f);
    CHECK(HandPreview::Preview(0.0f, 1.0f, 0.0f) == 0.0f); // nie negativ
}
