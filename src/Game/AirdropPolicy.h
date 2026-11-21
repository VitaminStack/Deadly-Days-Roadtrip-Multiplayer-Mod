#pragma once
#include <cstdint>

namespace Game
{
// Auch ein lokal erzeugter Client-Actor kann Role=Authority tragen. Das ist keine Host-Berechtigung.
constexpr bool AirdropMayRunGameplay(bool IsAirdrop, uint8_t ActorRole, bool IsNetworkClient)
{
    return !IsAirdrop || (ActorRole == 3 && !IsNetworkClient);
}

// opt-airdrop-sound: Landesound der Kiste beim Client. Der Blueprint des Hosts spielt `PlaceAirdrop` 2,5 s nach dem Erscheinen
// der Kiste (gemessen, 2,0 s bevor sie bereit ist), aus einer verzögerten Fortsetzung nach StartFallDown, die beim Client nie
// läuft. Der Client leitet den Zeitpunkt aus dem ab, was er schon bekommt: der Position der Fallanimation des Hosts.
inline constexpr float AirdropLandSeconds = 2.5f;

struct AirdropLanding
{
    bool Seen = false;   // Kiste schon einmal beobachtet
    bool Played = false; // Landesound gespielt oder nicht mehr fällig
};

// Wahr genau einmal je Kiste: wenn die Fallanimation die Landung erreicht. Eine Kiste, die bei der ersten Beobachtung schon
// gelandet oder bereit ist (Beitritt, Reise, spät repliziert), bleibt stumm. Kommt keine Animationsposition an, spielt der
// Sound ersatzweise, sobald die Kiste bereit wird.
constexpr bool AirdropLandingSound(AirdropLanding& State, bool Ready, bool HasAnimation, float Position)
{
    const bool Landed = Ready || (HasAnimation && Position >= AirdropLandSeconds);
    if (!State.Seen)
    {
        State.Seen = true;
        State.Played = Landed;
        return false;
    }
    if (State.Played || !Landed)
        return false;
    State.Played = true;
    return true;
}
}
