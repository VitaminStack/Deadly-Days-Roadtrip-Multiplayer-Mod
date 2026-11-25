// charakter-wahl: Figur der Gegenseite mit deren Wahl zeigen und, beim Host, mit deren Definition spielen.
//
// Native Regel (Build 26786272): UCharacterDefinitionApplier nimmt den Spielerindex aus Pawn.Controller.Player, also nur von
// einem LocalPlayer. Ohne LocalPlayer gilt Index 0. Beim Host ist das die Figur des Clients, beim Client der Proxy des Hosts:
// Beide trugen darum die eigene Wahl der jeweiligen Seite. CharacterSelectionChanged erreicht eine solche Figur nie.
//
// Was lokal bleibt: Auswahlpanel, Wahl im eigenen CharacterSelectionManager, Optik der eigenen Figur, alles nativ.
// Geteilt wird nur der Name der eigenen Definition je PlayerId (Sync/Players/CharacterSync.cpp). Jede Seite entscheidet über
// ihre eigene Figur; die Spielwerte der Client-Figuren rechnet wie bisher der Host, mit der Definition jedes Clients.
// Anwenden: dieselbe interne Funktion, die CharacterSelectionChanged aufruft. ApplyInitialCharacterDefinition einer
// Remote-Figur nimmt beim Spawn gleich die Wahl der Gegenseite, ein Sekunden-Abgleich folgt Änderungen in der MetaBase.
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/CharacterAdapter.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"
#include "Game/Reflection.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t ApplyInitial = 0x5838370;    // UCharacterDefinitionApplier::ApplyInitialCharacterDefinition (Rumpf)
constexpr uintptr_t ApplyDefinition = 0x5841F40; // intern: wendet eine Definition an, aus CharacterSelectionChanged

// Engine: UActorComponent.Owner, APawn.PlayerState und Controller, APlayerController.Player.
constexpr uintptr_t ComponentOwner = 0xA8;
constexpr uintptr_t PawnPlayerState = 0x2C8;
constexpr uintptr_t PawnController = 0x2D8;
constexpr uintptr_t ControllerPlayer = 0x348;
// TSoftObjectPtr: schwacher Verweis 8 Byte, dann FTopLevelAssetPath (PackageName, AssetName).
constexpr uintptr_t SoftAssetName = 0x10;
constexpr uint32_t GarbageOrUnreachableFlags = 0x30;

constexpr Reflection::FieldSpec AppliedSpec{"/Script/DDSurvivors", "CharacterDefinitionApplier", "AppliedCharacterDefinition",
                                            "SoftObjectProperty", 0x28};
// ADDCharacter hält seine Applier-Komponente direkt (SDK 0x840), das erspart die Suche in GObjects.
constexpr Reflection::FieldSpec ApplierSpec{"/Script/DDSurvivors", "DDCharacter", "CharacterDefinitionApplier", "ObjectProperty", 8};
constexpr Reflection::FieldSpec DefinitionsSpec{"/Script/DDSurvivors", "CharacterSelectionManager", "CharacterDefinitions",
                                                "ArrayProperty", 0x10};
// Repliziert, auf allen Seiten gleich: ordnet die Wahl der Figur zu.
constexpr Reflection::FieldSpec PlayerIdSpec{"/Script/Engine", "PlayerState", "PlayerId", "IntProperty", 4};
constexpr uintptr_t PlayerStateOwner = 0x158; // AActor.Owner des PlayerState: sein Controller

using ApplyInitialFn = void(__fastcall*)(uintptr_t);
using ApplyDefinitionFn = void(__fastcall*)(uintptr_t, uintptr_t);
ApplyInitialFn OriginalApplyInitial = nullptr;

struct
{
    std::map<int32_t, std::string> Choices; // PlayerId der anderen Spieler, gewählter Name
    uintptr_t Manager = 0;
    uintptr_t Pawn = 0, Applier = 0;
    uint64_t Applied = 0;
} C;

uintptr_t Pointer(uintptr_t Address)
{
    return Memory::ReadPointer(Address).value_or(0);
}

bool Alive(uintptr_t Object)
{
    return Object && !(Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachableFlags) & GarbageOrUnreachableFlags);
}

uintptr_t Manager()
{
    if (!Alive(C.Manager) || !Objects::IsA(C.Manager, "CharacterSelectionManager"))
        C.Manager = Objects::FindInstance("CharacterSelectionManager");
    return C.Manager;
}

// Definition mit diesem Namen aus der Liste des eigenen Managers, 0 wenn sie fehlt.
uintptr_t Definition(const std::string& Name)
{
    const auto Field = Reflection::Property(DefinitionsSpec);
    const uintptr_t Owner = Manager();
    if (!Field || !Owner)
        return 0;
    const uintptr_t Data = Pointer(Owner + Field->Offset);
    const int32_t Count = Memory::Read<int32_t>(Owner + Field->Offset + 8).value_or(0);
    for (int32_t i = 0; Data && i < Count && i < 256; ++i)
        if (const uintptr_t Entry = Pointer(Data + 8 * i); Entry && Memory::NameOf(Entry) == Name)
            return Entry;
    return 0;
}

std::string AppliedName(uintptr_t Applier)
{
    const auto Field = Reflection::Property(AppliedSpec);
    if (!Field)
        return {};
    const auto Index = Memory::Read<int32_t>(Applier + Field->Offset + SoftAssetName);
    return Index && *Index > 0 ? Memory::NameFromIndex(*Index) : std::string{};
}

// Spielerfigur ohne LocalPlayer: beim Host die Figur des Clients, beim Client der Proxy des Hosts.
bool RemoteFigure(uintptr_t Pawn)
{
    if (!Pawn || !Pointer(Pawn + PawnPlayerState) || !Objects::IsA(Pawn, "DDCharacter"))
        return false;
    const uintptr_t Controller = Pointer(Pawn + PawnController);
    return !Controller || !Objects::IsA(Pointer(Controller + ControllerPlayer), "LocalPlayer");
}

bool Networked()
{
    return CurrentRole() != NetRole::Standalone;
}

std::optional<int32_t> PlayerIdOfState(uintptr_t PlayerState)
{
    const auto Field = Reflection::Property(PlayerIdSpec);
    if (!Field || !Alive(PlayerState))
        return std::nullopt;
    return Memory::Read<int32_t>(PlayerState + Field->Offset);
}

// Gewünschter Name für die Remote-Figur, über die PlayerId ihres PlayerState. nullptr ohne Wahl.
const std::string* WantedFor(uintptr_t Pawn)
{
    const auto Id = PlayerIdOfState(Pointer(Pawn + PawnPlayerState));
    const auto Found = Id ? C.Choices.find(*Id) : C.Choices.end();
    return Found == C.Choices.end() ? nullptr : &Found->second;
}

void Apply(uintptr_t Applier, uintptr_t Definition, const char* Why)
{
    reinterpret_cast<ApplyDefinitionFn>(At(ApplyDefinition))(Applier, Definition);
    if (++C.Applied <= 20)
        Core::Log::Info("Charakterwahl: {} eines anderen Spielers angewendet ({})", Memory::NameOf(Definition), Why);
}

void __fastcall ApplyInitialHook(uintptr_t Applier)
{
    const uintptr_t Pawn = Pointer(Applier + ComponentOwner);
    if (Networked() && !C.Choices.empty() && RemoteFigure(Pawn))
        if (const std::string* Name = WantedFor(Pawn))
            if (const uintptr_t Wanted = Definition(*Name))
            {
                Apply(Applier, Wanted, "beim Spawn");
                return;
            }
    OriginalApplyInitial(Applier);
}

// Applier-Komponente der Figur, direkt aus ADDCharacter.CharacterDefinitionApplier. Eine volle GObjects-Suche hielt den Frame
// 10 bis 11 ms auf (opt-charakter-takt); sie bleibt nur als Rückfall, falls das Feld fehlt, und dann einmal je Figur.
bool IsApplierOf(uintptr_t Applier, uintptr_t Pawn)
{
    return Alive(Applier) && Pointer(Applier + ComponentOwner) == Pawn && Objects::IsA(Applier, "CharacterDefinitionApplier");
}

uintptr_t ApplierOf(uintptr_t Pawn)
{
    if (const auto Field = Reflection::Property(ApplierSpec))
    {
        const uintptr_t Applier = Pointer(Pawn + Field->Offset);
        return IsApplierOf(Applier, Pawn) ? Applier : 0;
    }
    if (Pawn == C.Pawn)
        return IsApplierOf(C.Applier, Pawn) ? C.Applier : 0;
    static bool Warned = false;
    if (!std::exchange(Warned, true))
        Core::Log::Warn("Charakterwahl: Feld DDCharacter.CharacterDefinitionApplier fehlt, Suche in GObjects je Figur");
    C.Pawn = Pawn;
    C.Applier = 0;
    for (int32_t i = 0, Count = Objects::Count(); i < Count; ++i)
        if (const uintptr_t Object = Objects::At(i); Object && IsApplierOf(Object, Pawn))
        {
            C.Applier = Object;
            break;
        }
    return C.Applier;
}

template <class VisitT> void ForEachRemoteFigure(VisitT Visit)
{
    const uintptr_t World = CurrentWorld();
    const uintptr_t Level = World ? Pointer(World + Offsets::UWorld::PersistentLevel) : 0;
    if (Level)
        Objects::ForEachLevelActor(Level, [&](uintptr_t Actor)
        {
            if (RemoteFigure(Actor))
                Visit(Actor);
        });
}

bool CheckApplyInitial()
{
    return Code::Expect(At(ApplyInitial), {0x40, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x80, 0xB9, 0x10, 0x01}, "ApplyInitialCharacterDefinition") &&
           Code::Expect(At(ApplyDefinition), {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18}, "Definition anwenden");
}

bool ApplyApplyInitial(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(ApplyInitial)), reinterpret_cast<void*>(&ApplyInitialHook),
                                reinterpret_cast<void**>(&OriginalApplyInitial));
}

// Je Patch die benutzten Adressen der Signaturtabelle, erzeugt von tools/patch_signatures.py sites.
constexpr uintptr_t ApplyInitialSites[] = {ApplyInitial, ApplyDefinition};
const Patch List[] = {
    {"Figur der Gegenseite mit deren Charakterwahl", CheckApplyInitial, ApplyApplyInitial, ApplyInitialSites},
};
}

std::span<const Patch> CharacterPatches()
{
    return List;
}

// Sekunden-Abgleich: Änderungen der Gegenseite in der MetaBase und Figuren, die vor der ersten Nachricht entstanden.
void TickCharacters(NetRole Role, bool Second)
{
    if (!Second || Role == NetRole::Standalone || C.Choices.empty())
        return;
    ForEachRemoteFigure([](uintptr_t Pawn)
    {
        const std::string* Name = WantedFor(Pawn);
        const uintptr_t Applier = Name ? ApplierOf(Pawn) : 0;
        if (!Applier || AppliedName(Applier) == *Name)
            return;
        if (const uintptr_t Wanted = Definition(*Name))
            Apply(Applier, Wanted, "Änderung");
    });
}
}

namespace Game::Characters
{
std::optional<std::string> OwnChoice()
{
    const uintptr_t Owner = Patches::Manager();
    const uintptr_t Function = Owner ? Reflection::Function("/Script/DDSurvivors", "CharacterSelectionManager", "GetSelectedCharacter") : 0;
    if (!Function)
        return std::nullopt;
    struct
    {
        int32_t PlayerIndex = 0;
        int32_t Pad = 0;
        uintptr_t ReturnValue = 0;
    } Params;
    Objects::ProcessEvent(Owner, Function, &Params);
    if (!Params.ReturnValue)
        return std::nullopt;
    return Memory::NameOf(Params.ReturnValue);
}

std::optional<int32_t> PlayerIdOf(uintptr_t Controller)
{
    if (!Patches::Alive(Controller))
        return std::nullopt;
    const auto Field = Reflection::Property({"/Script/Engine", "Controller", "PlayerState", "ObjectProperty", 8});
    return Field ? Patches::PlayerIdOfState(Patches::Pointer(Controller + Field->Offset)) : std::nullopt;
}

std::optional<int32_t> LocalPlayerId()
{
    const uintptr_t World = Patches::CurrentWorld();
    const uintptr_t State = World ? Patches::Pointer(World + Offsets::UWorld::GameState) : 0;
    const uintptr_t Array = State ? Patches::Pointer(State + Offsets::AGameStateBase::PlayerArray) : 0;
    const int32_t Count = State ? Memory::Read<int32_t>(State + Offsets::AGameStateBase::PlayerArray + 8).value_or(0) : 0;
    for (int32_t i = 0; Array && i < Count && i < 16; ++i)
    {
        const uintptr_t PlayerState = Patches::Pointer(Array + 8 * i);
        const uintptr_t Controller = PlayerState ? Patches::Pointer(PlayerState + Patches::PlayerStateOwner) : 0;
        if (Controller && Objects::IsA(Patches::Pointer(Controller + Patches::ControllerPlayer), "LocalPlayer"))
            return Patches::PlayerIdOfState(PlayerState);
    }
    return std::nullopt;
}

void SetPeerChoices(std::span<const PlayerChoice> Choices)
{
    std::map<int32_t, std::string> Next;
    for (const auto& Choice : Choices)
        Next[Choice.PlayerId] = Choice.Name;
    if (Patches::C.Choices == Next)
        return;
    for (const auto& [Id, Name] : Next)
        if (const auto Old = Patches::C.Choices.find(Id); Old == Patches::C.Choices.end() || Old->second != Name)
            Core::Log::Info("Charakterwahl: Spieler {} wählt {}", Id, Name);
    Patches::C.Choices = std::move(Next);
}
}
