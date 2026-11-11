#include "Game/Patches.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <vector>

#include "Core/Log.h"
#include "Core/Version.h"
#include "Game/AddressMap.h"
#include "Game/GameThread.h"
#include "Game/Objects.h"
#include "Game/Memory.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"
#include "Game/PatchSignatures.h"
#include "Game/RewardAdapter.h"

namespace Game::Patches
{
namespace
{
std::atomic<State> Current = State::Pending;
Code::Writer Caves;

// Reihenfolge wie bisher in Install. Ticks fragen über ListOn nach, ob ihre Liste steht.
enum ListIndex : size_t { Player, Client, Camera, Inventory, Travel, Widget, HealthBar, Airdrop, Exp, RemoteP2, Reward, Map, LocalDevice, Character, EnemyTelegraph, ListCount };
std::array<bool, ListCount> Enabled{};
bool PartialMode = false;

bool ListOn(ListIndex Index)
{
    return Enabled[Index];
}
}

uintptr_t At(uintptr_t Rva)
{
    return Memory::Rva(Rva);
}

uintptr_t At(uintptr_t Rva, uintptr_t Offset)
{
    const uintptr_t Base = Memory::Rva(Rva);
    return Base ? Base + Offset : 0;
}

bool ExpectBranch(uintptr_t Site, uint8_t Opcode, uintptr_t TargetRva, std::string_view Where)
{
    uint8_t Bytes[5] = {};
    const uintptr_t Target = At(TargetRva);
    if (Site && Target && Memory::ReadBytes(Site, Bytes, sizeof(Bytes)) && Bytes[0] == Opcode)
    {
        int32_t Rel = 0;
        std::memcpy(&Rel, Bytes + 1, sizeof(Rel));
        if (Site + sizeof(Bytes) + static_cast<intptr_t>(Rel) == Target)
            return true;
    }
    Core::Log::Error("Patches: unerwartetes Sprungziel {}", Where);
    return false;
}

uintptr_t CurrentWorld()
{
    return Memory::ReadPointer(At(Offsets::GWorld)).value_or(0);
}

NetRole CurrentRole()
{
    const uintptr_t World = CurrentWorld();
    const auto Driver = World ? Memory::ReadPointer(World + Offsets::UWorld::NetDriver) : std::nullopt;
    const bool HasConnection = Driver && Memory::ReadPointer(*Driver + Offsets::UNetDriver::ServerConnection);
    return RoleFrom(Driver.has_value(), HasConnection);
}

bool IsA(uintptr_t Object, const char* ClassName)
{
    return Objects::IsA(Object, ClassName);
}

State CurrentState()
{
    return Current.load();
}

bool SiteUsable(uintptr_t ReferenceRva)
{
    return Memory::Rva(ReferenceRva) != 0;
}

InstallResult InstallAll(std::span<const Patch> List, Code::Writer& Code, SiteCheck Usable, size_t* Skipped)
{
    // Erst die Patches mit gesperrten Stellen aussortieren, deren Check und Apply laufen nie.
    std::vector<const Patch*> Chosen;
    for (const Patch& Entry : List)
    {
        const bool Blocked = Usable && std::ranges::any_of(Entry.Sites, [Usable](uintptr_t Site) { return !Usable(Site); });
        if (!Blocked)
        {
            Chosen.push_back(&Entry);
            continue;
        }
        Core::Log::Warn("Patches: ausgelassen, Signatur passt nicht: {}", Entry.Name ? Entry.Name : "?");
        if (Skipped)
            ++*Skipped;
    }
    bool Ready = true;
    for (const Patch* Pointer : Chosen)
        if (const Patch& Entry = *Pointer; !Entry.Check || !Entry.Check())
        {
            Core::Log::Error("Patches: Prüfung fehlgeschlagen: {}", Entry.Name ? Entry.Name : "?");
            Ready = false;
        }
    if (!Ready)
        return InstallResult::CheckFailed;
    for (const Patch* Entry : Chosen)
        if (!Entry->Apply(Code) || Code.Failed())
        {
            Core::Log::Error("Patches: Setzen fehlgeschlagen: {}", Entry->Name);
            return InstallResult::ApplyFailed;
        }
    return InstallResult::Installed;
}

bool ListComplete(std::span<const Patch> List, SiteCheck Usable)
{
    return std::ranges::all_of(List, [Usable](const Patch& Entry) {
        return std::ranges::all_of(Entry.Sites, [Usable](uintptr_t Site) { return Usable(Site); });
    });
}

ListsResult InstallLists(std::span<const PatchList> Lists, Code::Writer& Code, std::span<bool> EnabledOut)
{
    ListsResult Result;
    for (size_t Index = 0; Index < Lists.size(); ++Index)
    {
        switch (InstallAll(Lists[Index].Patches, Code, SiteUsable, &Result.SkippedPatches))
        {
        case InstallResult::Installed:
            // Ticks greifen auf Adressen aller Patches der Liste zu, darum nur bei vollständiger Liste.
            EnabledOut[Index] = ListComplete(Lists[Index].Patches, SiteUsable);
            ++Result.Installed;
            break;
        case InstallResult::CheckFailed:
            Core::Log::Warn("Patches: Liste {} ausgelassen, Prüfung fehlgeschlagen", Lists[Index].Name);
            ++Result.Skipped;
            break;
        default:
            Core::Log::Error("Patches: Liste {} nur teilweise gesetzt", Lists[Index].Name);
            Result.ApplyFailed = true;
            return Result;
        }
    }
    return Result;
}

bool PartialInstallAllowed(bool SwitchOn, uint32_t Build)
{
    return SwitchOn && Build != 0 && Build != AddressMap::ReferenceBuild;
}

bool Install()
{
    const std::array<PatchList, ListCount> Lists = {{
        {"Player", PlayerPatches()}, {"Client", ClientPatches()}, {"Camera", CameraPatches()}, {"Inventory", InventoryPatches()},
        {"Travel", TravelPatches()}, {"Widget", WidgetPatches()}, {"HealthBar", HealthBarPatches()}, {"Airdrop", AirdropPatches()},
        {"Exp", ExpPatches()}, {"RemoteP2", RemoteP2Patches()}, {"Reward", RewardPatches()}, {"Map", MapPatches()},
        {"LocalDevice", LocalDevicePatches()}, {"Character", CharacterPatches()}, {"EnemyTelegraph", EnemyTelegraphPatches()},
    }};
    std::vector<Patch> All;
    for (const PatchList& List : Lists)
        All.insert(All.end(), List.Patches.begin(), List.Patches.end());

    if (!Caves.AllocateNear(Memory::ImageBase()))
    {
        Core::Log::Error("Patches: kein Speicher für Code-Höhlen in Sprungweite der Exe");
        Current = State::Inactive;
        return false;
    }
    // Vor dem ersten Hook: danach stehen an den geprüften Stellen andere Bytes.
    Signatures::VerifyAll();
    char Switch[2]{};
    const bool SwitchOn = GetEnvironmentVariableA("DDCOOP_PARTIAL_PATCHES", Switch, sizeof(Switch)) == 1 && Switch[0] == '1';
    if (PartialInstallAllowed(SwitchOn, Core::ActiveBuild()))
    {
        PartialMode = true;
        const ListsResult Result = InstallLists(Lists, Caves, Enabled);
        Current = Result.ApplyFailed ? State::Partial : Result.Skipped ? State::Partial : State::Active;
        Core::Log::Warn("Patches: Teilinstallation (DDCOOP_PARTIAL_PATCHES), {} von {} Listen gesetzt, {} Patches ausgelassen{}",
                        Result.Installed, Lists.size(), Result.SkippedPatches, Result.ApplyFailed ? ", Setzen abgebrochen" : "");
        if (Result.ApplyFailed)
            PartialMode = false; // Ticks bleiben aus, der Zustand ist nicht mehr listenweise sauber.
        return !Result.ApplyFailed && !Result.Skipped;
    }
    size_t Skipped = 0;
    switch (InstallAll(All, Caves, SiteUsable, &Skipped))
    {
    case InstallResult::Installed:
        for (size_t Index = 0; Index < ListCount; ++Index)
            Enabled[Index] = ListComplete(Lists[Index].Patches, SiteUsable);
        Current = State::Active;
        if (Skipped)
            Core::Log::Warn("Patches: {} gesetzt, {} wegen Signatur ausgelassen", All.size() - Skipped, Skipped);
        else
            Core::Log::Info("Patches: {} gesetzt", All.size());
        return true;
    case InstallResult::CheckFailed:
        Current = State::Inactive;
        Core::Log::Error("Patches: keiner gesetzt, eine Mission zu zweit läuft so nicht");
        return false;
    default:
        Current = State::Partial;
        Core::Log::Error("Patches: nur teilweise gesetzt, das Spiel kann instabil sein");
        return false;
    }
}

void Tick()
{
    // Teilinstallation: nur die Ticks gesetzter Listen. Projektile und Bewegung lesen nur Objekte, keine Patch-Adressen.
    if (Current != State::Active && !(Current == State::Partial && PartialMode))
        return;
    static ULONGLONG LastSecond = 0;
    const ULONGLONG Now = GetTickCount64();
    const bool Second = Now - LastSecond >= 1000;
    if (Second)
        LastSecond = Now;
    const NetRole Role = CurrentRole();
    if (ListOn(Widget))
        { const GameThread::SlowScope Watch("Widgets"); TickWidgets(Role); }
    if (ListOn(Player))
        { const GameThread::SlowScope Watch("Spieler"); TickPlayers(Role, Second); }
    if (ListOn(RemoteP2))
        { const GameThread::SlowScope Watch("RemoteP2"); TickRemoteP2(Role); }
    if (ListOn(Reward))
        { const GameThread::SlowScope Watch("Belohnungen"); Rewards::Tick(); }
    if (ListOn(HealthBar))
        { const GameThread::SlowScope Watch("Busbalken"); TickBusBar(Role); }
    if (ListOn(Widget))
        { const GameThread::SlowScope Watch("Lootbalken"); TickLootBar(Role); }
    if (ListOn(Camera))
        { const GameThread::SlowScope Watch("Kamera"); TickCamera(Role, Second); }
    { const GameThread::SlowScope Watch("Projektile"); TickProjectiles(Role, Second); }
    { const GameThread::SlowScope Watch("Bewegung"); TickMovement(Role, Second); }
    if (ListOn(Map))
        { const GameThread::SlowScope Watch("Karte"); TickMap(Role, Second); }
    if (ListOn(Character))
        { const GameThread::SlowScope Watch("Charakterwahl"); TickCharacters(Role, Second); }
    if (ListOn(EnemyTelegraph))
        { const GameThread::SlowScope Watch("Warnanzeigen"); TickEnemyTelegraphs(Role); }
}
}
