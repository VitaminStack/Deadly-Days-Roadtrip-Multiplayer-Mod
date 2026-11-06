#pragma once

#include <cmath>
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

// Host: GA_Dash liest die Laufrichtung über DDPlayerController.GetLastNormalizedMovementInput. Die setzt nur die lokale
// Eingabe, beim Controller des Clients bleibt sie (0, 0), und der Dodge bricht beim Host vor Kosten und Schub ab. Die
// Mod schätzt sie aus der Bewegung, die der Client ohnehin schickt. Tempo und Beschleunigung der Figur sind beim Host
// fast immer 0, verlässlich ist nur ihr Ort: Aus der Ortsänderung je RemoteSampleMs entsteht das Tempo. Ort kommt in
// Schüben an, darum gilt das letzte Tempo über einer Mindestgeschwindigkeit noch RemoteMoveHoldMs lang.
struct PlanarDirection
{
    double X = 0, Y = 0;
};

inline constexpr uint64_t RemoteSampleMs = 50;
inline constexpr uint64_t RemoteMoveHoldMs = 200;
inline constexpr double RemoteInputMinSpeed = 50.0;

// Tempo aus zwei Orten, in Einheiten je Sekunde.
constexpr PlanarDirection PlanarVelocity(PlanarDirection From, PlanarDirection To, uint64_t ElapsedMs)
{
    return ElapsedMs ? PlanarDirection{(To.X - From.X) * 1000.0 / ElapsedMs, (To.Y - From.Y) * 1000.0 / ElapsedMs} : PlanarDirection{};
}

inline bool PlanarMoving(PlanarDirection Velocity)
{
    return Velocity.X * Velocity.X + Velocity.Y * Velocity.Y > RemoteInputMinSpeed * RemoteInputMinSpeed;
}

// Eingabe des Clients als normierter Vektor aus dem letzten Tempo, das schnell genug war. Gemessen: Eingabe (X, Y) läuft
// in Weltrichtung (Y, X). Steht die Figur länger, ist die Eingabe (0, 0) wie beim Client.
inline PlanarDirection RemoteMovementInput(PlanarDirection LastMoving, uint64_t SinceLastMovingMs)
{
    if (SinceLastMovingMs > RemoteMoveHoldMs || !PlanarMoving(LastMoving))
        return {};
    const double Length = std::sqrt(LastMoving.X * LastMoving.X + LastMoving.Y * LastMoving.Y);
    return {LastMoving.Y / Length, LastMoving.X / Length};
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
