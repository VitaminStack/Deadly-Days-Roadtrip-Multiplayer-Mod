// Client: Reparaturen nach dem Dodge (GA_Dash), Befund in docs/GAME_ANALYSIS.md, Abschnitt Dodge beim Client.
//
// 1. Eigene Figur: GA_Dash setzt GroundFriction 0 und GravityScale 20 und stellt beides am Ende zurück. Beim Client fehlt das
//    Zurückstellen manchmal, die Figur rutscht danach nach jedem Dodge sekundenlang ohne Reibung. Hängen die Dodge-Werte
//    länger als ein echter Dodge, setzt die Mod die Werte aus dem Klassen-Defaultobjekt der Bewegungskomponente zurück.
// 2. Fremde Figur: Nach einem Dodge des Hosts, vor allem wenn er in eine Weltpause fällt (Item-Panel), bleibt seine Figur
//    beim Client stehen, in MOVE_None oder MOVE_Falling mit Dodge-Tempo. Ort und Modus kommen weiter an, werden aber nicht
//    mehr angewendet. Hängt die Kopie ohne Modus oder weit neben dem replizierten Ort, setzt die Mod Ort, Tempo und Modus
//    aus der Replikation.
#include <windows.h>

#include <cstddef>
#include <vector>

#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/MovementPolicy.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t ActorRole = 0x168;
constexpr uint8_t RoleSimulatedProxy = 1;
constexpr uintptr_t CharacterMovement = 0x330;
constexpr uintptr_t ActorRootComponent = 0x1B8;
constexpr uintptr_t SceneRelativeLocation = 0x148;
constexpr uintptr_t ActorReplicatedMovement = 0xD0; // FRepMovement: LinearVelocity +0x00, Location +0x30
constexpr uintptr_t RepMovementLocation = 0x30;
constexpr uintptr_t MovementVelocity = 0xD8;
constexpr uintptr_t CharacterReplicatedBasedMovement = 0x398; // FBasedMovementInfo
constexpr uintptr_t BasedServerHasBase = 0x02;               // Bit 0: bServerHasBaseComponent
constexpr uintptr_t BasedMovementBase = 0x10;
constexpr uintptr_t CharacterReplicatedMovementMode = 0x472;
constexpr uintptr_t MovementGravityScale = 0x1A8;
constexpr uintptr_t MovementMode = 0x241;
constexpr uintptr_t MovementGroundFriction = 0x244;
constexpr uintptr_t ClassDefaultObject = 0x118;
constexpr uint64_t DashLimitMs = 750;
constexpr uint64_t ProxyLimitMs = 500;

struct Figure
{
    uintptr_t Character = 0;
    StuckTimer Dash;
    StuckTimer Proxy;
    uint64_t LastFollow = 0;
    bool Following = false;
    struct
    {
        double X = 0, Y = 0, Z = 0;
    } LastReplicated;
    uint64_t ReplicatedAt = 0;
};

struct State
{
    uintptr_t World = 0;
    std::vector<Figure> Figures;
} Movement;

bool Alive(uintptr_t Object)
{
    return Object && !(Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(0x30) & 0x30);
}

// Spielerfiguren der aktuellen Welt. Die Timer bleiben für Figuren erhalten, die schon bekannt waren.
void RefreshFigures(uintptr_t World)
{
    const uintptr_t Level = Memory::ReadPointer(World + Offsets::UWorld::PersistentLevel).value_or(0);
    std::vector<Figure> Found;
    const int32_t Total = Objects::Count();
    for (int32_t i = 0; Level && i < Total; ++i)
    {
        const uintptr_t Object = Objects::At(i);
        if (!Object || Memory::ReadPointer(Object + Offsets::UObject::Outer).value_or(0) != Level)
            continue;
        if (Memory::NameOf(Memory::ReadPointer(Object + Offsets::UObject::Class).value_or(0)) != "BP_DD_Character_C" || !Alive(Object))
            continue;
        Figure Entry{Object};
        if (World == Movement.World)
            for (const Figure& Known : Movement.Figures)
                if (Known.Character == Object)
                    Entry = Known;
        Found.push_back(Entry);
    }
    Movement.World = World;
    Movement.Figures = std::move(Found);
}

// Werte, mit denen GA_Dash startet: die des Klassen-Defaultobjekts der Bewegungskomponente.
bool RestoreDashSettings(uintptr_t Component)
{
    const uintptr_t Class = Memory::ReadPointer(Component + Offsets::UObject::Class).value_or(0);
    const uintptr_t Defaults = Class ? Memory::ReadPointer(Class + ClassDefaultObject).value_or(0) : 0;
    const auto Friction = Defaults ? Memory::Read<float>(Defaults + MovementGroundFriction) : std::nullopt;
    const auto Gravity = Defaults ? Memory::Read<float>(Defaults + MovementGravityScale) : std::nullopt;
    if (!Friction || !Gravity || DashSettingsLeft(*Friction, *Gravity))
        return false;
    return Memory::WriteBytes(Component + MovementGroundFriction, &*Friction, sizeof(float)) &&
           Memory::WriteBytes(Component + MovementGravityScale, &*Gravity, sizeof(float));
}

struct Vector
{
    double X, Y, Z;
};

bool SetActorLocation(uintptr_t Character, const Vector& Location, bool Teleport)
{
    static const uintptr_t Function = Objects::FindObject("K2_SetActorLocation", "Actor");
    if (!Function)
        return false;
    struct
    {
        Vector NewLocation;
        bool Sweep;
        uint8_t Padding[7];
        uint8_t Hit[0x100];
        bool Teleport;
        bool Result;
        uint8_t Tail[6];
    } Params{Location, false, {}, {}, Teleport, false, {}};
    static_assert(offsetof(decltype(Params), Hit) == 0x20 && offsetof(decltype(Params), Teleport) == 0x120 && sizeof(Params) == 0x128);
    Objects::ProcessEvent(Character, Function, &Params);
    return Params.Result;
}

bool ApplyReplicatedMode(uintptr_t Component, uint8_t Packed);

// Ort, Tempo und Bewegungsmodus der fremden Figur aus ihrer Replikation übernehmen.
bool ResyncProxy(uintptr_t Character, uintptr_t Component, uint8_t Packed)
{
    const auto Location = Memory::Read<Vector>(Character + ActorReplicatedMovement + RepMovementLocation);
    const auto Velocity = Memory::Read<Vector>(Character + ActorReplicatedMovement);
    if (!Location || !Velocity || !SetActorLocation(Character, *Location, true))
        return false;
    ApplyReplicatedMode(Component, Packed);
    Memory::WriteBytes(Component + MovementVelocity, &*Velocity, sizeof(Vector));
    return true;
}

// Der Host steht auf einer beweglichen Unterlage, die nicht repliziert (etwa BP_Fog_Edge_BASE_C). Die Engine schickt dann
// seinen Ort relativ zu dieser Unterlage. Beim Client fehlt sie, und die Kopie wartet ohne Ende auf sie. Solange das so ist,
// folgt die Kopie jeden Frame weich dem absoluten Ort aus ReplicatedMovement, den der Host weiter mitschickt.
void FollowReplicatedLocation(Figure& Entry, uintptr_t Component, uint8_t Packed, const Vector& Actual, uint64_t Now)
{
    const auto Replicated = Memory::Read<Vector>(Entry.Character + ActorReplicatedMovement + RepMovementLocation);
    const auto Velocity = Memory::Read<Vector>(Entry.Character + ActorReplicatedMovement);
    if (!Replicated || !Velocity)
        return;
    const uint64_t Elapsed = Entry.LastFollow && Now > Entry.LastFollow ? Now - Entry.LastFollow : 16;
    Entry.LastFollow = Now;
    // Der Host-Ort kommt nur mit jedem Netz-Update. Dazwischen mit dem replizierten Tempo weiterrechnen, höchstens 150 ms.
    if (Replicated->X != Entry.LastReplicated.X || Replicated->Y != Entry.LastReplicated.Y || Replicated->Z != Entry.LastReplicated.Z)
    {
        Entry.LastReplicated.X = Replicated->X;
        Entry.LastReplicated.Y = Replicated->Y;
        Entry.LastReplicated.Z = Replicated->Z;
        Entry.ReplicatedAt = Now;
    }
    const double Ahead = ExtrapolationSeconds(Now - Entry.ReplicatedAt);
    const auto Target = std::optional<Vector>(Vector{Replicated->X + Velocity->X * Ahead, Replicated->Y + Velocity->Y * Ahead,
                                                     Replicated->Z + Velocity->Z * Ahead});
    const double Share = FollowShare(Elapsed);
    // Mehr als 6 m daneben: springen statt gleiten, etwa nach einem Dodge des Hosts.
    const bool Far = ProxyDrifted((Actual.X - Target->X) / 2, (Actual.Y - Target->Y) / 2);
    const Vector Next = Far ? *Target
                            : Vector{Actual.X + (Target->X - Actual.X) * Share, Actual.Y + (Target->Y - Actual.Y) * Share,
                                     Actual.Z + (Target->Z - Actual.Z) * Share};
    SetActorLocation(Entry.Character, Next, Far);
    if (Memory::Read<uint8_t>(Component + MovementMode).value_or(0) != PackedMovementMode(Packed))
        ApplyReplicatedMode(Component, Packed);
    Memory::WriteBytes(Component + MovementVelocity, &*Velocity, sizeof(Vector));
}

bool ApplyReplicatedMode(uintptr_t Component, uint8_t Packed)
{
    static const uintptr_t Function = Objects::FindObject("SetMovementMode", "CharacterMovementComponent");
    if (!Function)
        return false;
    struct
    {
        uint8_t Mode;
        uint8_t Custom;
    } Params{PackedMovementMode(Packed), PackedCustomMode(Packed)};
    Objects::ProcessEvent(Component, Function, &Params);
    return true;
}

void CheckFigure(Figure& Entry, uint64_t Now)
{
    const uintptr_t Component = Memory::ReadPointer(Entry.Character + CharacterMovement).value_or(0);
    const auto Role = Memory::Read<uint8_t>(Entry.Character + ActorRole);
    if (!Component || !Role)
        return;
    if (*Role == RoleSimulatedProxy)
    {
        const uint8_t Mode = Memory::Read<uint8_t>(Component + MovementMode).value_or(1);
        const uint8_t Packed = Memory::Read<uint8_t>(Entry.Character + CharacterReplicatedMovementMode).value_or(0);
        const uintptr_t Root = Memory::ReadPointer(Entry.Character + ActorRootComponent).value_or(0);
        const auto Actual = Root ? Memory::Read<Vector>(Root + SceneRelativeLocation) : std::nullopt;
        const auto Replicated = Memory::Read<Vector>(Entry.Character + ActorReplicatedMovement + RepMovementLocation);
        const uint8_t BaseFlags =
            Memory::Read<uint8_t>(Entry.Character + CharacterReplicatedBasedMovement + BasedServerHasBase).value_or(0);
        const uintptr_t Base = Memory::ReadPointer(Entry.Character + CharacterReplicatedBasedMovement + BasedMovementBase).value_or(0);
        if (Actual && PackedMovementMode(Packed) != 0 && BaseUnresolved(BaseFlags, Base))
        {
            if (!Entry.Following)
                Core::Log::Info("Bewegung: fremde Figur steht auf einer Unterlage, die der Client nicht kennt, folge dem Host-Ort");
            Entry.Following = true;
            FollowReplicatedLocation(Entry, Component, Packed, *Actual, Now);
            Entry.Proxy = {};
            return;
        }
        if (Entry.Following)
            Core::Log::Info("Bewegung: Unterlage der fremden Figur wieder bekannt, die Engine übernimmt");
        Entry.Following = false;
        Entry.LastFollow = 0;
        const bool Drifted = Actual && Replicated && (PackedMovementMode(Packed) != 0) &&
                             ProxyDrifted(Actual->X - Replicated->X, Actual->Y - Replicated->Y);
        const bool Stopped = ProxyMovementStopped(Mode, Packed);
        if (Entry.Proxy.Update(Drifted || Stopped, Now, ProxyLimitMs))
        {
            Entry.Proxy = {};
            if (Drifted ? ResyncProxy(Entry.Character, Component, Packed) : ApplyReplicatedMode(Component, Packed))
                Core::Log::Info("Bewegung: fremde Figur hing ({}), Ort und Modus {} aus der Replikation gesetzt",
                                Drifted ? "weit neben dem Host-Ort" : "ohne Bewegungsmodus", PackedMovementMode(Packed));
        }
        return;
    }
    const float Friction = Memory::Read<float>(Component + MovementGroundFriction).value_or(1.f);
    const float Gravity = Memory::Read<float>(Component + MovementGravityScale).value_or(1.f);
    if (Entry.Dash.Update(DashSettingsLeft(Friction, Gravity), Now, DashLimitMs) && RestoreDashSettings(Component))
    {
        Entry.Dash = {};
        Core::Log::Info("Bewegung: Dodge-Werte der eigenen Figur hingen, Reibung und Schwerkraft zurückgesetzt");
    }
}
}

void TickMovement(NetRole Role, bool Second)
{
    if (Role != NetRole::Client)
    {
        Movement = {};
        return;
    }
    const uintptr_t World = CurrentWorld();
    if (!World)
        return;
    if (Second || World != Movement.World)
        RefreshFigures(World);
    const uint64_t Now = GetTickCount64();
    for (Figure& Entry : Movement.Figures)
        if (Alive(Entry.Character))
            CheckFigure(Entry, Now);
}
}
