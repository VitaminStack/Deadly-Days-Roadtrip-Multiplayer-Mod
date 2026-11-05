#pragma once

#include <cstdint>

// Reine Regeln der Bewegungs-Reparaturen beim Client, ohne Spielzugriff, damit tests/GameTests sie prüfen kann. Befund:
// docs/GAME_ANALYSIS.md, Abschnitt Dodge beim Client (`bewegung`).
namespace Game::Patches
{
// GA_Dash setzt für die Dauer des Dodges GroundFriction 0 und GravityScale 20 und stellt beides am Ende zurück.
inline constexpr float DashFriction = 0.f;
inline constexpr float DashGravity = 20.f;

// Die eigene Figur hängt noch in den Dodge-Werten. Ein echter Dodge dauert 0,12 bis 0,16 s.
constexpr bool DashSettingsLeft(float GroundFriction, float GravityScale)
{
    return GroundFriction == DashFriction && GravityScale > DashGravity - 0.5f && GravityScale < DashGravity + 0.5f;
}

// Fremde Figur beim Client: Die Engine bewegt eine Figur in MOVE_None nicht mehr, auch wenn der Host Ort und Modus
// weiter repliziert. ReplicatedMovementMode ist gepackt: unteres Nibble MovementMode, oberes Nibble CustomMovementMode.
constexpr bool ProxyMovementStopped(uint8_t MovementMode, uint8_t ReplicatedMovementMode)
{
    return MovementMode == 0 && (ReplicatedMovementMode & 0x0F) != 0;
}

// Fremde Figur beim Client steht weit neben dem Ort, den der Host repliziert. Gemessen nach einem Dodge des Hosts, der in
// eine Weltpause fiel (Item-/Belohnungs-Panel): Die Kopie blieb in MOVE_Falling mit Dodge-Tempo stehen und übernahm keinen
// replizierten Ort mehr. Ein Dodge selbst legt in 0,15 s rund 8 m zurück, die Grenze greift darum erst nach einer Wartezeit.
inline constexpr double ProxyDriftLimit = 300.0;

constexpr bool ProxyDrifted(double DeltaX, double DeltaY)
{
    return DeltaX * DeltaX + DeltaY * DeltaY > ProxyDriftLimit * ProxyDriftLimit;
}

// Der Host meldet eine Unterlage (bServerHasBaseComponent), beim Client kam sie aber nicht an: Nicht replizierte,
// bewegliche Objekte wie die Nebelränder haben dort keine Entsprechung.
constexpr bool BaseUnresolved(uint8_t BasedFlags, uintptr_t MovementBase)
{
    return (BasedFlags & 0x01) && !MovementBase;
}

// Anteil des Wegs zum Zielort je Frame, etwa 25 je Sekunde: weich, aber ohne merklichen Nachlauf.
constexpr double FollowShare(uint64_t ElapsedMs)
{
    const double Share = static_cast<double>(ElapsedMs) / 40.0;
    return Share < 0.05 ? 0.05 : Share > 1.0 ? 1.0 : Share;
}

// Wie weit der zuletzt replizierte Ort mit dem replizierten Tempo weitergerechnet wird: so lange wie seit dem Update
// vergangen, höchstens 150 ms. Länger ohne Update heißt meist Stillstand oder Paketverlust, dann nicht weiter raten.
constexpr double ExtrapolationSeconds(uint64_t SinceUpdateMs)
{
    return (SinceUpdateMs > 150 ? 150 : SinceUpdateMs) / 1000.0;
}

constexpr uint8_t PackedMovementMode(uint8_t Packed)
{
    return Packed & 0x0F;
}

constexpr uint8_t PackedCustomMode(uint8_t Packed)
{
    return Packed >> 4;
}

// Wahr erst, wenn ein Zustand länger als Limit am Stück gilt. Kurze Übergänge, etwa ein laufender Dodge oder ein Frame
// zwischen zwei Replikationen, lösen nichts aus.
struct StuckTimer
{
    uint64_t Since = 0;
    bool Running = false;

    bool Update(bool Stuck, uint64_t Now, uint64_t Limit)
    {
        if (!Stuck)
        {
            Running = false;
            return false;
        }
        if (!Running)
        {
            Running = true;
            Since = Now;
        }
        return Now - Since >= Limit;
    }
};
}
