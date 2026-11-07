// Client: Reparaturen nach dem Dodge (GA_Dash), Befund in docs/GAME_ANALYSIS.md, Abschnitt Dodge beim Client.
//
// 1. Eigene Figur: GA_Dash setzt GroundFriction 0 und GravityScale 20 und stellt beides am Ende zurück. Beim Client fehlt das
//    Zurückstellen manchmal, die Figur rutscht danach nach jedem Dodge sekundenlang ohne Reibung. Hängen die Dodge-Werte
//    länger als ein echter Dodge, setzt die Mod die Werte aus dem Klassen-Defaultobjekt der Bewegungskomponente zurück.
// 2. Fremde Figur: Nach einem Dodge des Hosts, vor allem wenn er in eine Weltpause fällt (Item-Panel), bleibt seine Figur
//    beim Client stehen, in MOVE_None oder MOVE_Falling mit Dodge-Tempo. Ort und Modus kommen weiter an, werden aber nicht
//    mehr angewendet. Hängt die Kopie ohne Modus oder weit neben dem replizierten Ort, setzt die Mod Ort, Tempo und Modus
//    aus der Replikation.
// 3. Host: GA_Dash des Clients bricht beim Host vor Ausdauerkosten und Schub ab, weil die Laufrichtung des Client-
//    Controllers dort (0, 0) bleibt. Der Client bucht die Kosten nur vorhergesagt, der Host nimmt sie zurück: Der Client
//    dodgt ohne Grenze und reiht Dodges aneinander. Die Engine repliziert die Eingabe nicht, eine Schätzung aus der
//    Ortsänderung lag im Spiel in beide Richtungen daneben (dodge-client-live). Darum meldet der Client je eigenem Dodge
//    die Eingabe, die sein GA_Dash gelesen hat, mit dem Vorhersageschlüssel der Aktivierung. Der Host trägt sie direkt
//    vor dem Start ein; ist sie noch nicht da, hält er die Aktivierung kurz an. Die Schätzung bleibt nur Rückfall.
#include <windows.h>

#include <cstddef>
#include <vector>

#include "Core/Log.h"
#include "Game/DashAdapter.h"
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
constexpr uintptr_t ActorRemoteRole = 0x60;
constexpr uint8_t RoleAutonomousProxy = 2;
constexpr uintptr_t PawnController = 0x2D8;
constexpr uintptr_t PlayerStatePawn = 0x320;
constexpr uintptr_t ControllerLastMovementInput = 0x910; // DDPlayerController, FVector2D, gelesen von GetLastNormalizedMovementInput
// UGameplayAbility.CurrentActivationInfo (0xD4) + PredictionKeyWhenActivated (0x04) + FPredictionKey.Current (int16)
constexpr uintptr_t AbilityActivationKey = 0xD8;
constexpr uintptr_t AbilityIsActive = 0x388; // UGameplayAbility.bIsActive
constexpr uintptr_t DashTaskOffset = 0x400;     // GA_Dash_C.DashTask, erst nach dem Start gesetzt
constexpr uintptr_t RootMotionDuration = 0xEC;  // UAbilityTask_ApplyRootMotionConstantForce.Duration (float, s)
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
    PlanarDirection SampleLocation;
    uint64_t SampleAt = 0;
    PlanarDirection LastMoving;
    uint64_t LastMovingAt = 0;
    uint64_t RemoteDashAttempts = 0;
    uint64_t DashRestores = 0;
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

// Endet GA_Dash nicht über seine Aufgabe, sondern vorher (Ablehnung, Ende von der Gegenseite), stellt es Reibung und
// Schwerkraft nicht zurück. Hängen die Dodge-Werte länger als ein echter Dodge, kommen die Klassen-Defaults zurück.
void RestoreStuckDash(Figure& Entry, uintptr_t Component, uint64_t Now, const char* Which)
{
    const float Friction = Memory::Read<float>(Component + MovementGroundFriction).value_or(1.f);
    const float Gravity = Memory::Read<float>(Component + MovementGravityScale).value_or(1.f);
    if (Entry.Dash.Update(DashSettingsLeft(Friction, Gravity), Now, DashLimitMs) && RestoreDashSettings(Component))
    {
        const uint64_t Duration = Now - Entry.Dash.Since;
        Entry.Dash = {};
        Core::Log::Info("Bewegung: Dodge-Werte der {} hingen, Reibung und Schwerkraft zurückgesetzt "
                        "(Reparatur {}, seit {} ms, vorher Reibung={} Schwerkraft={})",
                        Which, ++Entry.DashRestores, Duration, Friction, Gravity);
    }
}

// Host: Bewegung der Client-Figur aus ihrem Ort messen. Tempo und Beschleunigung sind beim Host fast immer 0.
void SampleRemoteMovement(Figure& Entry, uint64_t Now)
{
    const uintptr_t Controller = Memory::ReadPointer(Entry.Character + PawnController).value_or(0);
    const uintptr_t Root = Memory::ReadPointer(Entry.Character + ActorRootComponent).value_or(0);
    if (!Alive(Controller) || !Root || Memory::Read<uint8_t>(Controller + ActorRemoteRole).value_or(0) != RoleAutonomousProxy)
        return;
    // Kommt die Aktivierung erst nach dem Ende beim Client an, beendet der Host den Dodge sofort mit; dann hängen die
    // Werte an seiner Kopie der Client-Figur (gemessen in dodge-client-live, 214 ms Verzug).
    if (const uintptr_t Component = Memory::ReadPointer(Entry.Character + CharacterMovement).value_or(0))
        RestoreStuckDash(Entry, Component, Now, "Client-Figur beim Host");
    const auto Location = Memory::Read<Vector>(Root + SceneRelativeLocation);
    if (!Location)
        return;
    const PlanarDirection Here{Location->X, Location->Y};
    if (Entry.SampleAt && Now - Entry.SampleAt < RemoteSampleMs)
        return;
    const PlanarDirection Moved = PlanarVelocity(Entry.SampleLocation, Here, Entry.SampleAt ? Now - Entry.SampleAt : 0);
    if (PlanarMoving(Moved))
    {
        Entry.LastMoving = Moved;
        Entry.LastMovingAt = Now;
    }
    Entry.SampleLocation = Here;
    Entry.SampleAt = Now;
}

// GA_Dash_C.K2_ActivateAbility, je Welt neu gesucht. Der Controller des Hosts überschreibt die Laufrichtung des Client-
// Controllers jeden Frame mit (0, 0), darum setzt die Mod sie erst direkt vor dem Start des Dodges.
uintptr_t DashActivateFunction = 0;
uintptr_t DashEndFunction = 0; // GA_Dash_C.K2_OnEndAbility
// Client: laufende eigene GA_Dash-Instanzen. Endet die letzte, ohne dass ihre Aufgabe die Werte zurückgestellt hat
// (Abbruch, etwa weil der Host eine verkettete Aktivierung ablehnt), stellt die Mod sie sofort zurück.
// Eine abgebrochene Instanz kann laut bIsActive noch Hunderte Millisekunden aktiv bleiben (gemessen: Host lehnt eine
// verkettete Aktivierung ab, der Client rutschte 750 ms). Darum gilt ein Dodge nur so lange als laufend, wie seine
// Schub-Aufgabe dauert, plus etwas Spielraum.
struct OwnDash
{
    uintptr_t Ability = 0;
    uint64_t Since = 0;
};
std::vector<OwnDash> OwnDashes;

bool OwnDashRunning(uint64_t Now)
{
    std::erase_if(OwnDashes, [Now](const OwnDash& Dash) {
        if (!Alive(Dash.Ability) || !Memory::Read<uint8_t>(Dash.Ability + AbilityIsActive).value_or(0))
            return true;
        const uintptr_t Task = Memory::ReadPointer(Dash.Ability + DashTaskOffset).value_or(0);
        const auto Duration = Task && Alive(Task) ? Memory::Read<float>(Task + RootMotionDuration) : std::nullopt;
        return Now - Dash.Since > DashExpectedMs(Duration);
    });
    return !OwnDashes.empty();
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
    if (DashSettingsLeft(Friction, Gravity) && !OwnDashRunning(Now) && RestoreDashSettings(Component))
    {
        Entry.Dash = {};
        Core::Log::Info("Bewegung: Dodge der eigenen Figur abgebrochen, Reibung und Schwerkraft sofort zurückgesetzt (Reparatur {})",
                        ++Entry.DashRestores);
        return;
    }
    RestoreStuckDash(Entry, Component, Now, "eigenen Figur");
}
}

namespace
{
// Client: eigene Aktivierungen, die der Sync-Takt an den Host schickt.
std::vector<DashInput> OutgoingInputs;
// Host: gemeldete Eingaben und angehaltene Aktivierungen des Clients.
DashInputBook ReportedInputs;
struct HeldDash
{
    uintptr_t Ability = 0;
    uintptr_t Character = 0;
    int16_t Key = 0;
    uint64_t Since = 0;
};
std::vector<HeldDash> HeldDashes;
bool ResumingDash = false;
uint64_t DashFallbacks = 0;

Figure* FigureOfAbility(uintptr_t Ability)
{
    // Eine Fähigkeit je Ausführung entsteht mit dem Besitzer ihres AbilitySystems als Outer: Figur oder ihr PlayerState.
    const uintptr_t Outer = Memory::ReadPointer(Ability + Offsets::UObject::Outer).value_or(0);
    const uintptr_t OuterPawn = Outer ? Memory::ReadPointer(Outer + PlayerStatePawn).value_or(0) : 0;
    for (Figure& Entry : Movement.Figures)
        if (Entry.Character == Outer || Entry.Character == OuterPawn)
            return &Entry;
    return nullptr;
}

// Controller des Clients beim Host, sonst 0.
uintptr_t RemoteController(uintptr_t Character)
{
    const uintptr_t Controller = Memory::ReadPointer(Character + PawnController).value_or(0);
    return Alive(Controller) && Memory::Read<uint8_t>(Controller + ActorRemoteRole).value_or(0) == RoleAutonomousProxy ? Controller : 0;
}

bool WriteControllerInput(uintptr_t Controller, PlanarDirection Input)
{
    const double Raw[2]{Input.X, Input.Y};
    return Memory::WriteBytes(Controller + ControllerLastMovementInput, Raw, sizeof(Raw));
}

// Rückfall ohne Meldung des Clients: Richtung aus der gemessenen Ortsänderung.
PlanarDirection EstimatedInput(const Figure& Entry, uint64_t Now)
{
    return RemoteMovementInput(Entry.LastMoving, Entry.LastMovingAt ? Now - Entry.LastMovingAt : UINT64_MAX);
}

// Host: angehaltene Aktivierung mit der Eingabe starten. Die Fähigkeit läuft dann nativ: Kosten, Schub, Ende.
void ResumeDash(const HeldDash& Held, PlanarDirection Input)
{
    if (const uintptr_t Controller = RemoteController(Held.Character))
        WriteControllerInput(Controller, Input);
    uint8_t Params[16]{};
    ResumingDash = true;
    Objects::ProcessEvent(Held.Ability, DashActivateFunction, Params);
    ResumingDash = false;
}

void TickHeldDashes(uint64_t Now)
{
    for (size_t i = 0; i < HeldDashes.size();)
    {
        const HeldDash Held = HeldDashes[i];
        const bool Active = Alive(Held.Ability) && Memory::Read<uint8_t>(Held.Ability + AbilityIsActive).value_or(0);
        if (!Active || !DashActivateFunction)
        {
            Core::Log::Warn("Bewegung: angehaltener Client-Dodge (Schluessel {}) endete vor seiner Richtung", Held.Key);
            HeldDashes.erase(HeldDashes.begin() + i);
            continue;
        }
        const auto Reported = ReportedInputs.Take(Held.Key, Now);
        if (!Reported && Now - Held.Since < DashWaitMs)
        {
            ++i;
            continue;
        }
        HeldDashes.erase(HeldDashes.begin() + i);
        PlanarDirection Input{};
        if (Reported)
        {
            Input = {Reported->X, Reported->Y};
            Core::Log::Info("Bewegung: Client-Dodge (Schluessel {}) nach {} ms mit gemeldeter Richtung ({:.2f},{:.2f}) gestartet",
                            Held.Key, Now - Held.Since, Input.X, Input.Y);
        }
        else
        {
            for (const Figure& Known : Movement.Figures)
                if (Known.Character == Held.Character)
                    Input = EstimatedInput(Known, Now);
            Core::Log::Warn("Bewegung: Client-Dodge (Schluessel {}) ohne Meldung nach {} ms, geschaetzte Richtung ({:.2f},{:.2f}), "
                            "Rueckfall {}", Held.Key, Now - Held.Since, Input.X, Input.Y, ++DashFallbacks);
        }
        ResumeDash(Held, Input);
    }
}
}

std::vector<DashInput> TakeOwnDashInputs()
{
    std::vector<DashInput> Result;
    Result.swap(OutgoingInputs);
    return Result;
}

void ReceiveReportedDashInput(const DashInput& Value)
{
    if (CurrentRole() == NetRole::Host)
        ReportedInputs.Add(Value, GetTickCount64());
}

bool PrepareRemoteDash(uintptr_t Object, uintptr_t Function)
{
    if (!Object || !Function || ResumingDash || (Function != DashActivateFunction && Function != DashEndFunction))
        return false;
    const NetRole Role = CurrentRole();
    if (Function == DashEndFunction)
    {
        if (Role == NetRole::Client)
            std::erase_if(OwnDashes, [Object](const OwnDash& Dash) { return Dash.Ability == Object; });
        return false;
    }
    Figure* Entry = Role == NetRole::Host || Role == NetRole::Client ? FigureOfAbility(Object) : nullptr;
    if (!Entry)
    {
        if (Role == NetRole::Host)
            Core::Log::Warn("Bewegung: Dodge-Aktivierung keiner bekannten Figur zugeordnet ({} Figuren)", Movement.Figures.size());
        return false;
    }
    const int16_t Key = Memory::Read<int16_t>(Object + AbilityActivationKey).value_or(0);
    if (Role == NetRole::Client)
    {
        // Nur die eigene Figur. Genau diese Eingabe liest GA_Dash gleich über GetLastNormalizedMovementInput.
        if (Memory::Read<uint8_t>(Entry->Character + ActorRole).value_or(0) != RoleAutonomousProxy)
            return false;
        const uintptr_t Controller = Memory::ReadPointer(Entry->Character + PawnController).value_or(0);
        const auto Raw = Controller ? Memory::Read<PlanarDirection>(Controller + ControllerLastMovementInput) : std::nullopt;
        if (OutgoingInputs.size() >= 16)
            OutgoingInputs.erase(OutgoingInputs.begin());
        // Hängen noch Werte eines abgebrochenen Dodges, nähme GA_Dash sie als „vorher“ und stellte sie am Ende wieder her.
        const uintptr_t Component = Memory::ReadPointer(Entry->Character + CharacterMovement).value_or(0);
        const uint64_t Now = GetTickCount64();
        if (Component && !OwnDashRunning(Now) &&
            DashSettingsLeft(Memory::Read<float>(Component + MovementGroundFriction).value_or(1.f),
                             Memory::Read<float>(Component + MovementGravityScale).value_or(1.f)) &&
            RestoreDashSettings(Component))
            Core::Log::Info("Bewegung: Dodge-Werte vor neuem Dodge zurückgesetzt");
        OwnDashes.push_back({Object, Now});
        const DashInput Value = ReportedDashInput(Key, Raw.value_or(PlanarDirection{}));
        OutgoingInputs.push_back(Value);
        Core::Log::Info("Bewegung: eigener Dodge (Schluessel {}) mit Richtung ({:.2f},{:.2f}) an den Host gemeldet", Key, Value.X, Value.Y);
        return false;
    }
    const uintptr_t Controller = RemoteController(Entry->Character);
    if (!Controller)
        return false; // Eigene Figur des Hosts: Eingabe ist lokal.
    const uint64_t Now = GetTickCount64();
    if (const auto Reported = ReportedInputs.Take(Key, Now))
    {
        WriteControllerInput(Controller, {Reported->X, Reported->Y});
        Core::Log::Info("Bewegung: Client-Dodge (Schluessel {}) mit gemeldeter Richtung ({:.2f},{:.2f})", Key, Reported->X, Reported->Y);
        return false;
    }
    // Die Meldung ist noch unterwegs: Aktivierung anhalten, der Takt startet sie mit der Richtung oder nach DashWaitMs.
    HeldDashes.push_back({Object, Entry->Character, Key, Now});
    return true;
}

void TickMovement(NetRole Role, bool Second)
{
    static NetRole LastRole = NetRole::Standalone;
    if (Role == NetRole::Standalone || Role != LastRole || CurrentWorld() != Movement.World)
        DashActivateFunction = DashEndFunction = 0;
    if (Role == NetRole::Standalone || Role != LastRole)
    {
        LastRole = Role;
        Movement = {};
        HeldDashes.clear();
        ReportedInputs = {};
        OutgoingInputs.clear();
        OwnDashes.clear();
        if (Role == NetRole::Standalone)
            return;
    }
    const uintptr_t World = CurrentWorld();
    if (!World)
        return;
    if (Second || World != Movement.World)
        RefreshFigures(World);
    if (Second && !DashActivateFunction)
    {
        DashActivateFunction = Objects::FindObject("K2_ActivateAbility", "GA_Dash_C");
        DashEndFunction = DashActivateFunction ? Objects::FindObject("K2_OnEndAbility", "GA_Dash_C") : 0;
        if (DashActivateFunction && Role == NetRole::Host)
            Core::Log::Info("Bewegung: Host-Beobachter fuer GA_Dash bereit; Richtung wird je Client-Aktivierung protokolliert");
    }
    const uint64_t Now = GetTickCount64();
    for (Figure& Entry : Movement.Figures)
        if (Alive(Entry.Character))
            Role == NetRole::Client ? CheckFigure(Entry, Now) : SampleRemoteMovement(Entry, Now);
    if (Role == NetRole::Host)
        TickHeldDashes(Now);
}
}

namespace Game
{
std::vector<DashInput> TakeDashInputs()
{
    return Patches::TakeOwnDashInputs();
}

void ReceiveDashInput(const DashInput& Value)
{
    Patches::ReceiveReportedDashInput(Value);
}
}
