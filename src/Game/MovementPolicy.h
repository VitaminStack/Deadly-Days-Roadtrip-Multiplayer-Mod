#pragma once

#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "Game/DashAdapter.h"

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

// Lauftempo der eigenen Figur beim Client (`tod-client`). Das Spiel setzt MaxWalkSpeed nur beim Host aus dem replizierten
// Attribut UCommonAttributeSet.MovementSpeed (OnMovementSpeedChanged_Server). Beim Client kommt das Attribut an, die
// Bewegungskomponente bleibt aber auf ihrem Startwert: Ein toter Client kroch beim Host mit Tempo 63, lief bei sich mit 600
// weiter, und der Host übernahm seine Position. Der Client übernimmt darum jede Änderung des Attributs wie der Host.
inline constexpr float WalkSpeedLimit = 10000.f;

// Nur ein initialisiertes, plausibles Attribut gilt. NaN fällt durch jeden Vergleich.
constexpr bool WalkSpeedAttributeValid(float BaseValue, float CurrentValue)
{
    return BaseValue > 0.f && BaseValue <= WalkSpeedLimit && CurrentValue >= 0.f && CurrentValue <= WalkSpeedLimit;
}

// Neues Tempo nur bei einer Änderung des Attributs, wie der native Delegate. Andere lokale Schreiber der
// Bewegungskomponente bleiben so unberührt, solange das Attribut gleich bleibt.
constexpr std::optional<float> WalkSpeedToApply(std::optional<float> LastAttribute, float BaseValue, float CurrentValue)
{
    if (!WalkSpeedAttributeValid(BaseValue, CurrentValue))
        return std::nullopt;
    if (LastAttribute && *LastAttribute - CurrentValue < 0.01f && CurrentValue - *LastAttribute < 0.01f)
        return std::nullopt;
    return CurrentValue;
}

// Dodge der eigenen Figur beim Client, während sie tot ist (`tod-client`). Der Host lehnt GA_Dash eines toten Spielers nativ
// ab, die Vorhersage des Clients lief aber trotzdem: Die tote Figur sprang je Dodge mehrere Meter, und der Host übernahm die
// Position. Maßgeblich sind die replizierten HP des Hosts. Nicht initialisierte Werte (MaxHealth 0) sperren nichts.
constexpr bool OwnDashBlockedByDeath(float Health, float MaxHealth)
{
    return MaxHealth > 0.f && Health <= 0.f;
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

// Diagnose derselben Freigabe, die RemoteMovementInput benutzt. Eine erfolgreiche Speicher-Schreiboperation allein
// bedeutet nicht, dass GA_Dash eine nutzbare Richtung bekommt; auch (0, 0) kann erfolgreich geschrieben werden.
enum class RemoteInputState
{
    Moving,
    Expired,
    NoMovement,
};

inline RemoteInputState RemoteMovementState(PlanarDirection LastMoving, uint64_t SinceLastMovingMs)
{
    if (SinceLastMovingMs > RemoteMoveHoldMs)
        return RemoteInputState::Expired;
    return PlanarMoving(LastMoving) ? RemoteInputState::Moving : RemoteInputState::NoMovement;
}

// Eingabe des Clients als normierter Vektor aus dem letzten Tempo, das schnell genug war. Gemessen: Eingabe (X, Y) läuft
// in Weltrichtung (Y, X). Steht die Figur länger, ist die Eingabe (0, 0) wie beim Client.
inline PlanarDirection RemoteMovementInput(PlanarDirection LastMoving, uint64_t SinceLastMovingMs)
{
    if (RemoteMovementState(LastMoving, SinceLastMovingMs) != RemoteInputState::Moving)
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

// Client: So lange gilt ein eigener Dodge als laufend. Mit Schub-Aufgabe deren Dauer plus 60 ms, ohne (noch nicht gestartet,
// unlesbar oder unplausibel) 250 ms; ein echter Dodge dauert 0,12 bis 0,16 s und stellt am Ende selbst zurück.
inline uint64_t DashExpectedMs(std::optional<float> DurationSeconds)
{
    if (!DurationSeconds || !std::isfinite(*DurationSeconds) || *DurationSeconds <= 0.f || *DurationSeconds > 2.f)
        return 250;
    return static_cast<uint64_t>(*DurationSeconds * 1000.f) + 60;
}

// Host: Wie lange eine Aktivierung des Clients auf seine gemeldete Richtung wartet. Die Meldung geht zuverlässig, das
// Spielpaket mit der Aktivierung unzuverlässig; es kann sie überholen, mit Ping-Schwankung um einige zehn Millisekunden.
// Danach startet der Dodge mit der geschätzten Richtung, damit er nie hängen bleibt.
inline constexpr uint64_t DashWaitMs = 250;
// Gemeldete Richtungen ohne passende Aktivierung verfallen danach, etwa wenn der Host die Aktivierung ablehnt.
inline constexpr uint64_t DashInputKeepMs = 2000;

// Client: Eingabe so, wie GA_Dash sie über GetLastNormalizedMovementInput liest, normiert oder (0, 0).
inline DashInput ReportedDashInput(int16_t Key, PlanarDirection Raw)
{
    const double Length = std::sqrt(Raw.X * Raw.X + Raw.Y * Raw.Y);
    if (!std::isfinite(Length) || Length < 1e-4)
        return {Key, 0.f, 0.f};
    return {Key, static_cast<float>(Raw.X / Length), static_cast<float>(Raw.Y / Length)};
}

// Host: gemeldete Richtungen je Vorhersageschlüssel. Eine Meldung gilt für genau eine Aktivierung.
class DashInputBook
{
public:
    void Add(const DashInput& Value, uint64_t Now)
    {
        Prune(Now);
        for (auto& Entry : Entries)
            if (Entry.Value.Key == Value.Key)
            {
                Entry = {Value, Now};
                return;
            }
        if (Entries.size() >= 32)
            Entries.erase(Entries.begin());
        Entries.push_back({Value, Now});
    }

    std::optional<DashInput> Take(int16_t Key, uint64_t Now)
    {
        Prune(Now);
        for (size_t i = 0; i < Entries.size(); ++i)
            if (Entries[i].Value.Key == Key)
            {
                const DashInput Value = Entries[i].Value;
                Entries.erase(Entries.begin() + i);
                return Value;
            }
        return std::nullopt;
    }

    size_t Size() const { return Entries.size(); }

private:
    struct Entry
    {
        DashInput Value;
        uint64_t At = 0;
    };
    void Prune(uint64_t Now)
    {
        std::erase_if(Entries, [Now](const Entry& Item) { return Now - Item.At > DashInputKeepMs; });
    }
    std::vector<Entry> Entries;
};
}
