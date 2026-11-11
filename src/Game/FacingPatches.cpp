// opt-blickrichtung-client, Client: Blickrichtung der Spielerfiguren. Das Richtungs-Mesh (DirectionalCharacterMesh) wählt Vorder-
// oder Rückenansicht und links/rechts aus dem Winkel AHands+0x31C. Den schreibt nur Hands::RotateTowards, und das läuft nur über
// den Server-RPC ADDCharacter.ProxyRotateTowards, also nur beim Host. Beim Client bleibt der Winkel 0, beide Figuren zeigen dauerhaft
// den Rücken. Die Drehung des Hände-Actors repliziert aber nativ. Lokal zuerst: Der Client trägt den Gierwinkel dieser replizierten
// Drehung als Winkel ein, das native Mesh rechnet daraus die Ansicht wie beim Host. Keine Nachricht, kein Hook, kein Aufruf; Ziel-
// wahl, Schüsse und Schaden bleiben beim Host. Befund: docs/GAME_ANALYSIS.md#blickrichtung-beim-client-opt-blickrichtung-client
#include <windows.h>

#include <array>

#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchPolicy.h"
#include "Game/PatchRuntime.h"

#ifdef DDCOOP_WITH_SDK
#pragma warning(push, 0)
#include <SDK/Engine_classes.hpp>
#include <SDK/DDSurvivors_classes.hpp>
#pragma warning(pop)
static_assert(offsetof(SDK::ADDCharacter, Hands) == 0x7B0);
static_assert(offsetof(SDK::AActor, RootComponent) == 0x1B8);
#endif

namespace Game::Patches
{
namespace
{
constexpr uintptr_t CharacterHands = 0x7B0;      // ADDCharacter.Hands (Net)
constexpr uintptr_t HandsFacingAngle = 0x31C;    // AHands, float, Grad; gelesen vom DirectionalCharacterMesh (0x586BE20)
constexpr uintptr_t ActorRootComponent = 0x1B8;  // AActor.RootComponent
constexpr uintptr_t ComponentWorldRotation = 0x1F0; // USceneComponent.ComponentToWorld.Rotation (FQuat, 4 double)
constexpr uintptr_t ActorRole = 0x168;
constexpr uint8_t RoleAuthority = 3;
constexpr uint32_t GarbageOrUnreachable = 0x30;
constexpr const char* CharacterClass = "BP_DD_Character_C";
constexpr size_t MaxFigures = 8;

struct Tracked
{
    uintptr_t Object = 0;
    uintptr_t Class = 0;
    int32_t Index = -1;
};

struct Figure
{
    Tracked Character;
    Tracked Hands;
    bool Reported = false;
};

// Nur auf dem Game Thread: Patches::Tick läuft dort.
std::array<Figure, MaxFigures> Figures{};
size_t FigureCount = 0;
uintptr_t LastWorld = 0;
int Reported = 0;

void ForgetAll()
{
    FigureCount = 0;
}

// Der Platz im Objekt-Array beweist, dass das Objekt noch existiert; freigegebener Speicher wird nie beschrieben.
bool StillThere(const Tracked& Entry)
{
    if (!Entry.Object || Objects::At(Entry.Index) != Entry.Object)
        return false;
    if (Memory::Read<uintptr_t>(Entry.Object + Offsets::UObject::Class).value_or(0) != Entry.Class)
        return false;
    return !(Memory::Read<uint32_t>(Entry.Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachable) & GarbageOrUnreachable);
}

Tracked Track(uintptr_t Object)
{
    const int32_t Index = Memory::Read<int32_t>(Object + Offsets::UObject::InternalIndex).value_or(-1);
    const uintptr_t Class = Memory::Read<uintptr_t>(Object + Offsets::UObject::Class).value_or(0);
    Tracked Entry{Object, Class, Index};
    return Class && StillThere(Entry) ? Entry : Tracked{};
}

// Spielerfiguren der aktuellen Welt, einmal je Sekunde. Bekannte Figuren behalten ihren Stand.
void RefreshFigures(uintptr_t World)
{
    const uintptr_t Level = Memory::ReadPointer(World + Offsets::UWorld::PersistentLevel).value_or(0);
    std::array<Figure, MaxFigures> Found{};
    size_t Count = 0;
    Objects::ForEachLevelActor(Level, [&](uintptr_t Object)
    {
        if (Count == MaxFigures ||
            Memory::NameOf(Memory::ReadPointer(Object + Offsets::UObject::Class).value_or(0)) != CharacterClass)
            return;
        Figure Entry{Track(Object)};
        if (!Entry.Character.Object)
            return;
        for (size_t i = 0; i < FigureCount; ++i)
            if (Figures[i].Character.Object == Object)
                Entry = Figures[i];
        Found[Count++] = Entry;
    });
    Figures = Found;
    FigureCount = Count;
}

// Hände der Figur; ein neuer Zeiger wird erst nach Prüfung von Platz und Klasse benutzt.
uintptr_t HandsOf(Figure& Entry)
{
    const uintptr_t Hands = Memory::ReadPointer(Entry.Character.Object + CharacterHands).value_or(0);
    if (!Hands)
        return 0;
    if (Hands != Entry.Hands.Object)
    {
        Entry.Hands = Objects::IsA(Hands, "Hands") ? Track(Hands) : Tracked{};
        if (!Entry.Hands.Object)
            return 0;
    }
    return StillThere(Entry.Hands) ? Hands : 0;
}

void Apply(Figure& Entry)
{
    if (Memory::Read<uint8_t>(Entry.Character.Object + ActorRole).value_or(RoleAuthority) == RoleAuthority)
        return;
    const uintptr_t Hands = HandsOf(Entry);
    const uintptr_t Root = Hands ? Memory::ReadPointer(Hands + ActorRootComponent).value_or(0) : 0;
    double Rotation[4]{};
    if (!Root || !Memory::ReadBytes(Root + ComponentWorldRotation, Rotation, sizeof(Rotation)))
        return;
    const float Target = FacingYawFromQuat(Rotation[0], Rotation[1], Rotation[2], Rotation[3]);
    const float Current = Memory::Read<float>(Hands + HandsFacingAngle).value_or(0.0f);
    if (!FacingNeedsWrite(Current, Target) || !Memory::WriteBytes(Hands + HandsFacingAngle, &Target, sizeof(Target)))
        return;
    if (!Entry.Reported && Reported < 8)
    {
        Entry.Reported = true;
        ++Reported;
        Core::Log::Info("Blickrichtung: Client übernimmt den Winkel der Hände aus der replizierten Drehung ({:.0f} Grad)", Target);
    }
}
}

void TickFacing(NetRole Role, bool Second)
{
    if (Role != NetRole::Client)
    {
        if (FigureCount)
            ForgetAll();
        return;
    }
    const uintptr_t World = CurrentWorld();
    if (World != LastWorld)
    {
        ForgetAll();
        LastWorld = World;
        if (World)
            RefreshFigures(World);
    }
    else if (Second && World)
        RefreshFigures(World);
    if (!World)
        return;

    for (size_t i = FigureCount; i-- > 0;)
    {
        if (!StillThere(Figures[i].Character))
        {
            Figures[i] = Figures[--FigureCount];
            continue;
        }
        Apply(Figures[i]);
    }
}
}
