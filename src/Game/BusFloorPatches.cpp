// Client (busplatz-ohne-boden): Boden unter der eigenen Figur am Busplatz. Die eigene Kartenerzeugung setzt die Figur an den
// Bus, während noch Generatoren laufen. Die Mod prüft dort den Boden so, wie die Bewegungskomponente der Figur ihn sieht
// (K2_ComputeFloorDist, mit Kanal und Antworten ihrer Kapsel), schreibt Ort, Modus, Fallgeschwindigkeit und Boden der ersten
// Frames ins Log und hält die Figur am Busort, solange dort kein Boden ist. Lokal, ohne Nachricht. Regeln in BusFloorPolicy.h.
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include "Core/Log.h"
#include "Game/BusFloorPolicy.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"
#include "Game/Reflection.h"
#include "Game/WorldAdapter.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t ObjectClass = 0x10;
constexpr uintptr_t ActorRole = 0x168;
constexpr uintptr_t ComponentOwner = 0xA8;
constexpr uint8_t RoleAutonomousProxy = 2;
constexpr uint8_t ModeNone = 0, ModeWalking = 1, ModeFalling = 3;

constexpr Reflection::FieldSpec CharacterMovementField{"/Script/Engine", "Character", "CharacterMovement", "ObjectProperty", 8};
constexpr Reflection::FieldSpec CapsuleField{"/Script/Engine", "Character", "CapsuleComponent", "ObjectProperty", 8};
constexpr Reflection::FieldSpec HalfHeightField{"/Script/Engine", "CapsuleComponent", "CapsuleHalfHeight", "FloatProperty", 4};
constexpr Reflection::FieldSpec RadiusField{"/Script/Engine", "CapsuleComponent", "CapsuleRadius", "FloatProperty", 4};
constexpr Reflection::FieldSpec MovementModeField{"/Script/Engine", "CharacterMovementComponent", "MovementMode", "ByteProperty", 1};
constexpr Reflection::FieldSpec VelocityField{"/Script/Engine", "MovementComponent", "Velocity", "StructProperty", 24};
constexpr Reflection::FieldSpec FloorHitField{"/Script/Engine", "FindFloorResult", "HitResult", "StructProperty", 0};
constexpr Reflection::FieldSpec ImpactPointField{"/Script/Engine", "HitResult", "ImpactPoint", "StructProperty", 24};
constexpr Reflection::FieldSpec HitComponentField{"/Script/Engine", "HitResult", "Component", "WeakObjectProperty", 8};

struct Vector
{
    double X = 0, Y = 0, Z = 0;
};

// Parameterpuffer für ProcessEvent, groß genug für jede hier gerufene Funktion samt FHitResult.
constexpr size_t ParamsCapacity = 0x400;
struct alignas(16) Params
{
    std::array<uint8_t, ParamsCapacity> Bytes{};
    template <typename T> void Set(int32_t Offset, T Value) { std::memcpy(Bytes.data() + Offset, &Value, sizeof(T)); }
    template <typename T> T Get(int32_t Offset) const
    {
        T Value{};
        std::memcpy(&Value, Bytes.data() + Offset, sizeof(T));
        return Value;
    }
};

int32_t ParamsSize(uintptr_t Function)
{
    return Memory::Read<int32_t>(Function + Offsets::UStruct::PropertiesSize).value_or(-1);
}

// K2_ComputeFloorDist und die Felder des Ergebnisses, einmal je Start aufgelöst.
struct ProbeLayout
{
    uintptr_t Function = 0;
    int32_t Location = 0, Line = 0, Sweep = 0, Radius = 0, Result = 0, Impact = 0, Component = 0;
};

const std::optional<ProbeLayout>& FloorProbeLayout()
{
    static const std::optional<ProbeLayout> Layout = []() -> std::optional<ProbeLayout>
    {
        ProbeLayout L;
        L.Function = Reflection::Function("/Script/Engine", "CharacterMovementComponent", "K2_ComputeFloorDist");
        if (!L.Function)
            return std::nullopt;
        const auto Param = [&](std::string_view Name, std::string_view Type, int32_t Size)
        { return Reflection::Parameter(L.Function, {"", "", Name, Type, Size}); };
        const auto Location = Param("CapsuleLocation", "StructProperty", 24);
        const auto Line = Param("LineDistance", "FloatProperty", 4);
        const auto Sweep = Param("SweepDistance", "FloatProperty", 4);
        const auto Radius = Param("SweepRadius", "FloatProperty", 4);
        const auto Result = Param("FloorResult", "StructProperty", 0);
        const auto Hit = Reflection::Property(FloorHitField);
        const auto Impact = Reflection::Property(ImpactPointField);
        const auto Component = Reflection::Property(HitComponentField);
        const int32_t Size = ParamsSize(L.Function);
        if (!Location || !Line || !Sweep || !Radius || !Result || !Hit || !Impact || !Component || Size <= 0 ||
            Size > static_cast<int32_t>(ParamsCapacity))
        {
            Core::Log::Error("Busboden: Bodenprüfung ohne passendes Layout, Busplatz bleibt wie ohne Mod");
            return std::nullopt;
        }
        L.Location = Location->Offset, L.Line = Line->Offset, L.Sweep = Sweep->Offset, L.Radius = Radius->Offset;
        L.Result = Result->Offset;
        L.Impact = Result->Offset + Hit->Offset + Impact->Offset, L.Component = Result->Offset + Hit->Offset + Component->Offset;
        return L;
    }();
    return Layout;
}

struct Floor
{
    bool Valid = false; // Prüfung lief
    bool Hit = false;
    double ImpactZ = 0;
    uintptr_t Component = 0;
};

// Objekt hinter einem FWeakObjectPtr (Index, Seriennummer), 0 wenn es nicht mehr lebt.
uintptr_t Resolve(uint64_t Weak)
{
    const int32_t Index = static_cast<int32_t>(Weak & 0xFFFFFFFF);
    const uint32_t Serial = static_cast<uint32_t>(Weak >> 32);
    const uintptr_t Object = Index > 0 ? Objects::At(Index) : 0;
    const Objects::Ref Held = Objects::RefTo(Object);
    return Held.Object && Held.Serial == Serial && Objects::IsLive(Held) ? Object : 0;
}

// Sweep der Kapsel nach unten am Ort Bus, mit dem Kanal und den Antworten der Figur.
Floor ProbeFloor(uintptr_t Movement, const Vector& Bus)
{
    const auto& L = FloorProbeLayout();
    if (!L || !Movement)
        return {};
    Params P;
    P.Set(L->Location, Vector{Bus.X, Bus.Y, Bus.Z + BusFloorProbeAbove});
    const float Distance = static_cast<float>(BusFloorProbeAbove + BusFloorProbeBelow);
    P.Set(L->Line, Distance);
    P.Set(L->Sweep, Distance);
    P.Set(L->Radius, 20.f);
    Objects::ProcessEvent(Movement, L->Function, P.Bytes.data());
    Floor Result;
    Result.Valid = true;
    Result.Component = Resolve(P.Get<uint64_t>(L->Component));
    Result.Hit = Result.Component != 0;
    Result.ImpactZ = P.Get<Vector>(L->Impact).Z;
    return Result;
}

std::optional<Vector> ActorLocation(uintptr_t Actor)
{
    static const uintptr_t Function = Objects::FindObject("K2_GetActorLocation", "Actor");
    if (!Actor || !Function)
        return std::nullopt;
    Vector Value;
    Objects::ProcessEvent(Actor, Function, &Value);
    return Value;
}

// Teleport ohne Sweep, Parameter über die Reflexion.
bool Teleport(uintptr_t Actor, const Vector& To)
{
    static const uintptr_t Function = Objects::FindObject("K2_SetActorLocation", "Actor");
    static const auto Location = Function ? Reflection::Parameter(Function, {"", "", "NewLocation", "StructProperty", 24}) : std::nullopt;
    static const auto Flag = Function ? Reflection::Parameter(Function, {"", "", "bTeleport", "BoolProperty", 1}) : std::nullopt;
    if (!Actor || !Location || !Flag || ParamsSize(Function) > static_cast<int32_t>(ParamsCapacity))
        return false;
    Params P;
    P.Set(Location->Offset, To);
    P.Set<uint8_t>(Flag->Offset, 1);
    Objects::ProcessEvent(Actor, Function, P.Bytes.data());
    return true;
}

void SetMode(uintptr_t Movement, uint8_t Mode)
{
    static const uintptr_t Function = Objects::FindObject("SetMovementMode", "CharacterMovementComponent");
    if (!Movement || !Function)
        return;
    struct
    {
        uint8_t Mode;
        uint8_t Custom;
        uint8_t Pad[6];
    } Args{Mode, 0, {}};
    Objects::ProcessEvent(Movement, Function, &Args);
}

std::string OwnerText(uintptr_t Component)
{
    if (!Component)
        return "-";
    const uintptr_t Owner = Memory::ReadPointer(Component + ComponentOwner).value_or(0);
    return Memory::NameOf(Component) + " von " + (Owner ? Memory::NameOf(Owner) : std::string("-"));
}

std::string FloorText(const Floor& F)
{
    if (!F.Valid)
        return "nicht geprüft";
    if (!F.Hit)
        return "kein Boden";
    return std::format("Boden bei Z {:.0f} ({})", F.ImpactZ, OwnerText(F.Component));
}

// ---- Testschalter ----

// DDCOOP_TEST_CLIENT_FLOOR_DELAY_MS=<ms>: Beim Busplatz des Clients schaltet die Mod die Kollision der Komponenten unter dem
// Busort ab und nach so vielen Millisekunden wieder ein, wie bei einem Boden, der spät kommt. Ohne die Variable 0.
ULONGLONG TestFloorDelay()
{
    static const ULONGLONG Delay = []
    {
        char Value[16]{};
        const DWORD Length = GetEnvironmentVariableA("DDCOOP_TEST_CLIENT_FLOOR_DELAY_MS", Value, sizeof(Value));
        return Length > 0 && Length < sizeof(Value) ? static_cast<ULONGLONG>(std::max(0, atoi(Value))) : 0ull;
    }();
    return Delay;
}

// DDCOOP_TEST_CLIENT_BUS_HOLD_OFF=1: die Figur nicht halten, nur messen. Für den Gegentest mit dem Schalter oben.
bool TestHoldOff()
{
    static const bool Off = []
    {
        char Value[4]{};
        return GetEnvironmentVariableA("DDCOOP_TEST_CLIENT_BUS_HOLD_OFF", Value, sizeof(Value)) == 1 && Value[0] == '1';
    }();
    return Off;
}

struct Disabled
{
    Objects::Ref Component;
    uint8_t Previous = 0;
};

struct
{
    std::vector<Disabled> Components;
    uintptr_t World = 0;
    ULONGLONG Until = 0;
} Delay;

uint8_t CollisionOf(uintptr_t Component)
{
    static const uintptr_t Function = Objects::FindObject("GetCollisionEnabled", "PrimitiveComponent");
    struct
    {
        uint8_t Value;
        uint8_t Pad[7];
    } Args{};
    if (Function)
        Objects::ProcessEvent(Component, Function, &Args);
    return Args.Value;
}

void SetCollision(uintptr_t Component, uint8_t Value)
{
    static const uintptr_t Function = Objects::FindObject("SetCollisionEnabled", "PrimitiveComponent");
    struct
    {
        uint8_t Value;
        uint8_t Pad[7];
    } Args{Value, {}};
    if (Function)
        Objects::ProcessEvent(Component, Function, &Args);
}

void DelayFloor(uintptr_t Movement, const Vector& Bus)
{
    if (!TestFloorDelay())
        return;
    // Unter dem Busort können mehrere Komponenten liegen (Kachel, Bodenplatte). Höchstens acht, bis keine mehr trifft.
    for (int Round = 0; Round < 8; ++Round)
    {
        const Floor F = ProbeFloor(Movement, Bus);
        if (!F.Hit)
            break;
        Delay.Components.push_back({Objects::RefTo(F.Component), CollisionOf(F.Component)});
        SetCollision(F.Component, 0); // ECollisionEnabled::NoCollision
        Core::Log::Warn("Busboden: Testschalter DDCOOP_TEST_CLIENT_FLOOR_DELAY_MS, Kollision von {} für {} ms aus", OwnerText(F.Component),
                        TestFloorDelay());
    }
    Delay.World = CurrentWorld();
    Delay.Until = GetTickCount64() + TestFloorDelay();
}

void TickFloorDelay()
{
    if (Delay.Components.empty() || (Delay.World == CurrentWorld() && GetTickCount64() < Delay.Until))
        return;
    int Restored = 0;
    for (const Disabled& Entry : Delay.Components)
        if (Delay.World == CurrentWorld() && Objects::IsLive(Entry.Component))
        {
            SetCollision(Entry.Component.Object, Entry.Previous);
            ++Restored;
        }
    Core::Log::Warn("Busboden: Testschalter, Kollision von {} Komponenten wieder an", Restored);
    Delay.Components.clear();
}

// ---- Messung und Halten ----

struct
{
    uintptr_t World = 0;
    Objects::Ref Figure;
    uintptr_t Movement = 0;
    Vector Bus;
    ULONGLONG Since = 0;
    bool Tracing = false, Deciding = false, Holding = false, LastHit = false;
    uint8_t LastMode = 0xFF;
    int Frames = 0, Steady = 0;
} Bus;

constexpr int BusFloorEveryFrames = 10;

uintptr_t OwnFigure()
{
    const uintptr_t World = CurrentWorld();
    const uintptr_t Level = World ? Memory::ReadPointer(World + Offsets::UWorld::PersistentLevel).value_or(0) : 0;
    uintptr_t Found = 0;
    Objects::ForEachLevelActor(Level, [&](uintptr_t Object)
    {
        if (!Found && Memory::NameOf(Memory::ReadPointer(Object + ObjectClass).value_or(0)) == "BP_DD_Character_C" &&
            Memory::Read<uint8_t>(Object + ActorRole).value_or(0) == RoleAutonomousProxy)
            Found = Object;
    });
    return Found;
}

// Bus der aktuellen Welt: BP_SpawnAndLeaveBus_C mit Countdown-Widget (die gleichnamige Vorlage hat keines, BusBarPatches.cpp).
constexpr uintptr_t BusCountDownWidget = 0x3B8;
uintptr_t FindBus()
{
    const uintptr_t World = CurrentWorld();
    const uintptr_t Level = World ? Memory::ReadPointer(World + Offsets::UWorld::PersistentLevel).value_or(0) : 0;
    uintptr_t Found = 0;
    Objects::ForEachLevelActor(Level, [&](uintptr_t Object)
    {
        if (!Found && Objects::IsA(Object, "BP_SpawnAndLeaveBus_C") && Memory::ReadPointer(Object + BusCountDownWidget).value_or(0))
            Found = Object;
    });
    return Found;
}

uintptr_t MovementOf(uintptr_t Figure)
{
    const auto Field = Reflection::Property(CharacterMovementField);
    return Field && Figure ? Memory::ReadPointer(Figure + Field->Offset).value_or(0) : 0;
}

uint8_t ModeOf(uintptr_t Movement)
{
    const auto Field = Reflection::Property(MovementModeField);
    return Field ? Memory::Read<uint8_t>(Movement + Field->Offset).value_or(0xFF) : 0xFF;
}

Vector VelocityOf(uintptr_t Movement)
{
    const auto Field = Reflection::Property(VelocityField);
    return Field ? Memory::Read<Vector>(Movement + Field->Offset).value_or(Vector{}) : Vector{};
}

void StopFigure(uintptr_t Movement)
{
    if (const auto Field = Reflection::Property(VelocityField))
    {
        const Vector Zero;
        Memory::WriteBytes(Movement + Field->Offset, &Zero, sizeof(Zero));
    }
}

std::string CapsuleText(uintptr_t Figure)
{
    const auto Capsule = Reflection::Property(CapsuleField);
    const auto Half = Reflection::Property(HalfHeightField);
    const auto Radius = Reflection::Property(RadiusField);
    const uintptr_t Component = Capsule ? Memory::ReadPointer(Figure + Capsule->Offset).value_or(0) : 0;
    if (!Component || !Half || !Radius)
        return "Kapsel unbekannt";
    return std::format("Kapsel halbe Höhe {:.0f}, Radius {:.0f}", Memory::Read<float>(Component + Half->Offset).value_or(0),
                       Memory::Read<float>(Component + Radius->Offset).value_or(0));
}

// Hält, lässt los oder gibt auf. Wahr, solange weiter entschieden werden muss.
bool Decide(const Floor& F, ULONGLONG Now)
{
    const bool Ground = F.Valid && FloorAtBus(F.Hit, Bus.Bus.Z, F.ImpactZ);
    switch (NextBusHold(Bus.Holding, Ground, Now - Bus.Since))
    {
    case BusHold::Done:
        return false;
    case BusHold::Hold:
        if (!Bus.Holding)
            Core::Log::Warn("Busboden: kein Boden am Bus bei ({:.0f}, {:.0f}, {:.0f}), eigene Figur wird gehalten", Bus.Bus.X, Bus.Bus.Y,
                            Bus.Bus.Z);
        Bus.Holding = true;
        SetMode(Bus.Movement, ModeNone);
        StopFigure(Bus.Movement);
        Teleport(Bus.Figure.Object, Bus.Bus);
        return true;
    case BusHold::Release:
    case BusHold::GiveUp:
        break;
    }
    const bool Released = Ground;
    Teleport(Bus.Figure.Object, Bus.Bus);
    StopFigure(Bus.Movement);
    SetMode(Bus.Movement, ModeFalling);
    Bus.Holding = false;
    if (Released)
        Core::Log::Info("Busboden: Boden am Bus nach {} ms da, eigene Figur losgelassen", Now - Bus.Since);
    else
        Core::Log::Warn("Busboden: nach {} ms kein Boden am Bus, eigene Figur losgelassen wie ohne Mod", Now - Bus.Since);
    return false;
}
}

void NoteBusKeep(const uintptr_t* Keep, int32_t Num, bool GameState)
{
    if (CurrentRole() != NetRole::Client)
        return;
    std::string Text;
    for (int32_t Index = 0; Keep && Index < std::min(Num, 8); ++Index)
    {
        const auto At = ActorLocation(Keep[Index]);
        Text += std::format("{}{} ({:.0f}, {:.0f}, {:.0f})", Index ? ", " : "", Memory::NameOf(Memory::ReadPointer(Keep[Index] + ObjectClass).value_or(0)),
                            At ? At->X : 0, At ? At->Y : 0, At ? At->Z : 0);
    }
    const auto BusAt = ActorLocation(FindBus());
    Core::Log::Info("Busort: Freiräumung des Clients, {} geschont{}: {}; Bus {}", Num, GameState ? "" : ", ohne GameState", Text.empty() ? "-" : Text,
                    BusAt ? std::format("bei ({:.0f}, {:.0f}, {:.0f})", BusAt->X, BusAt->Y, BusAt->Z) : std::string("nicht gefunden"));
}

void NoteBusFloor(bool AfterClear)
{
    if (CurrentRole() != NetRole::Client)
        return;
    const ULONGLONG Now = GetTickCount64();
    if (!AfterClear)
    {
        const uintptr_t Figure = OwnFigure();
        const uintptr_t Movement = MovementOf(Figure);
        const auto At = ActorLocation(Figure);
        if (!Figure || !Movement || !At)
        {
            Core::Log::Warn("Busboden: Busplatz des Clients ohne eigene Figur, keine Bodenprüfung");
            Bus = {};
            return;
        }
        Bus = {};
        Bus.World = CurrentWorld();
        Bus.Figure = Objects::RefTo(Figure);
        Bus.Movement = Movement;
        Bus.Bus = *At;
        Bus.Since = Now;
        Bus.Tracing = true;
        Bus.Deciding = !TestHoldOff();
        if (TestHoldOff())
            Core::Log::Warn("Busboden: Testschalter DDCOOP_TEST_CLIENT_BUS_HOLD_OFF, eigene Figur wird nicht gehalten");
        DelayFloor(Movement, Bus.Bus);
    }
    else if (!Bus.Tracing || Bus.World != CurrentWorld() || !Objects::IsLive(Bus.Figure))
        return;
    const Floor F = ProbeFloor(Bus.Movement, Bus.Bus);
    const auto At = ActorLocation(Bus.Figure.Object);
    Core::Log::Info("Busboden: {} Modus {} bei ({:.0f}, {:.0f}, {:.0f}), Tempo Z {:.0f}, {}, {}",
                    AfterClear ? "nach der Freiräumung" : "Busplatz,", ModeOf(Bus.Movement), At ? At->X : 0, At ? At->Y : 0, At ? At->Z : 0,
                    VelocityOf(Bus.Movement).Z, FloorText(F), CapsuleText(Bus.Figure.Object));
    if (Bus.Deciding)
        Bus.Deciding = Decide(F, Now);
}

void TickBusFloor(NetRole Role)
{
    if (Role != NetRole::Client)
        return;
    TickFloorDelay();
    if (!Bus.Tracing && !Bus.Deciding)
        return;
    if (Bus.World != CurrentWorld() || !Objects::IsLive(Bus.Figure))
    {
        Bus = {};
        return;
    }
    const ULONGLONG Now = GetTickCount64();
    const Floor F = ProbeFloor(Bus.Movement, Bus.Bus);
    if (Bus.Deciding)
        Bus.Deciding = Decide(F, Now);
    if (!Bus.Tracing)
        return;
    const uint8_t Mode = ModeOf(Bus.Movement);
    Bus.Steady = Mode == ModeWalking && F.Hit ? Bus.Steady + 1 : 0;
    const bool Over = BusFloorTraceOver(Bus.Steady, Now - Bus.Since);
    // Jeder der ersten Frames, danach nur bei Änderung von Modus oder Boden und am Ende.
    const bool Changed = Mode != Bus.LastMode || F.Hit != Bus.LastHit;
    Bus.LastMode = Mode, Bus.LastHit = F.Hit;
    if (++Bus.Frames <= BusFloorEveryFrames || Changed || Over)
    {
        const auto At = ActorLocation(Bus.Figure.Object);
        Core::Log::Info("Busboden: Frame {}, +{} ms, Modus {}, Z {:.0f}, Tempo Z {:.0f}, {}", Bus.Frames, Now - Bus.Since, Mode, At ? At->Z : 0,
                        VelocityOf(Bus.Movement).Z, FloorText(F));
    }
    if (Over)
        Bus.Tracing = false;
}
}
