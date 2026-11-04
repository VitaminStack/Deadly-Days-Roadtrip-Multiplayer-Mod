// Testhilfen: CheatManager fuer die DD_-Befehle des Spiels und Laden einzelner Assets, etwa fuer einen SDK-Dump mit Bossen.
#include "Spike.h"
#include "TestGodmodePolicy.h"

namespace Spike
{
namespace
{
constexpr uintptr_t kControllerCheatManager = 0x410;
bool TestGodmodeEnabled = false; // Explicit opt-in; Spike only, never a production mod setting.
struct ProtectedPawn
{
    uintptr_t Pawn;
    uintptr_t AbilitySystem;
    int32_t Name;
    int32_t Index;
};
std::vector<ProtectedPawn> GodmodeOwned;
}

// Uses the game's loose-tag API, not healing or fabricated replicated health. A private +1 contribution can be
// removed without clearing invincibility granted by gameplay effects. Never dereference a cached old-world pawn.
void TickTestGodmode()
{
    if (!TestGodmodeEnabled && GodmodeOwned.empty())
        return;
    const uint64_t Tag = Read<uint64_t>(Base + 0xAC61FE8);
    if (!Tag || NameFromIndex(static_cast<int32_t>(Tag)) != "State.Invincible")
        return;
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t State = World ? Read<uintptr_t>(World + kWorldGameState) : 0;
    const uintptr_t Players = State ? Read<uintptr_t>(State + kGameStatePlayerArray) : 0;
    const int32_t Count = State ? Read<int32_t>(State + kGameStatePlayerArray + 8) : 0;
    if (!Players || Count <= 0 || Count > 16)
        return;
    std::vector<ProtectedPawn> Roster;
    // Do not forget ownership while the roster is partially replicated; the same pawn may return next tick.
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Player = Read<uintptr_t>(Players + i * 8);
        const uintptr_t Pawn = Player ? Read<uintptr_t>(Player + 0x320) : 0;
        if (!Pawn || (Read<uint32_t>(Pawn + 8) & 0x10))
            return;
        bool IsCharacter = false;
        for (uintptr_t Class = Read<uintptr_t>(Pawn + 0x10); Class; Class = Read<uintptr_t>(Class + 0x40))
            if (NameOf(Class) == "DDCharacter") { IsCharacter = true; break; }
        if (!IsCharacter)
            return;
        const uintptr_t Asc = Read<uintptr_t>(Pawn + 0x6E0);
        if (!Asc)
            return;
        bool Duplicate = false;
        for (const auto& Existing : Roster)
            Duplicate |= Existing.AbilitySystem == Asc;
        if (!Duplicate)
            Roster.push_back({Pawn, Asc, Read<int32_t>(Pawn + 0x18), Read<int32_t>(Pawn + 0x0C)});
    }
    std::vector<ProtectedPawn> Current;
    for (const auto& Entry : Roster)
    {
        const uintptr_t Pawn = Entry.Pawn;
        const uintptr_t Asc = Entry.AbilitySystem;
        bool Owned = false;
        for (const auto& Old : GodmodeOwned)
            Owned |= Old.Pawn == Pawn && Old.AbilitySystem == Asc && Old.Name == Entry.Name && Old.Index == Entry.Index;
        using HasTagFn = bool(__fastcall*)(uintptr_t, uint64_t);
        const uintptr_t Interface = Asc + 0x148;
        const bool HasTag = Read<HasTagFn*>(Interface)[3](Interface, Tag);
        using UpdateTagFn = void(__fastcall*)(uintptr_t, const uint64_t*, int32_t, uint8_t);
        const auto Update = reinterpret_cast<UpdateTagFn>(Base + 0x4B7D7D0);
        const int Delta = TestGodmodePolicy::Delta(TestGodmodeEnabled, Owned, HasTag);
        if (Delta > 0)
        {
            Update(Asc, &Tag, 1, 0);
            Owned = true;
            Log("Test-Godmode: State.Invincible gesetzt fuer " + NameOf(Pawn));
        }
        else if (!TestGodmodeEnabled && Owned)
        {
            if (Delta < 0)
                Update(Asc, &Tag, -1, 0);
            Owned = false;
            Log("Test-Godmode: eigener Tag-Beitrag entfernt fuer " + NameOf(Pawn));
        }
        if (Owned)
            Current.push_back(Entry);
    }
    GodmodeOwned = std::move(Current);
}

void SetTestGodmode(bool Enabled)
{
    TestGodmodeEnabled = Enabled;
    Log(Enabled ? "Test-Godmode: EIN, bleibt ueber Reisen aktiv" : "Test-Godmode: AUS");
    TickTestGodmode();
}

// Im Shipping-Build bleibt APlayerController::CheatManager leer, die Konsole verwirft dann alle DD_-Befehle.
// GameplayStatics.SpawnObject legt den Manager mit dem Controller als Outer an, wie es die Engine selbst taete.
void EnableCheats()
{
    const uintptr_t Statics = FindObject("Default__GameplayStatics", nullptr);
    const uintptr_t SpawnObject = FindObject("SpawnObject", "GameplayStatics");
    const uintptr_t CheatClass = FindObject("DDCheatManager", nullptr);
    if (!Statics || !SpawnObject || !CheatClass)
    {
        Log("Cheats: Klassen nicht gefunden");
        return;
    }
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    int Enabled = 0;
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x10) != 0)
            continue;
        bool IsController = false;
        for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class && !IsController; Class = Read<uintptr_t>(Class + 0x40))
            IsController = NameOf(Class) == "PlayerController";
        // Das Feld erst lesen, wenn das Objekt sicher ein Controller ist, kleinere Objekte enden davor.
        if (!IsController || Read<uintptr_t>(Object + kControllerCheatManager))
            continue;
        struct { uintptr_t ObjectClass; uintptr_t Outer; uintptr_t ReturnValue; } Params{CheatClass, Object, 0};
        Read<ProcessEventFn*>(Statics)[kProcessEventIndex](Statics, SpawnObject, &Params);
        if (!Params.ReturnValue)
            continue;
        *reinterpret_cast<uintptr_t*>(Object + kControllerCheatManager) = Params.ReturnValue;
        ++Enabled;
    }
    Log("Cheats: CheatManager fuer " + std::to_string(Enabled) + " Controller angelegt");
}

// Laedt ein Asset ueber seinen Pfad, etwa "/Game/.../BP_X.BP_X_C". Dumper-7 erfasst danach auch diese Klasse.
void LoadAssetCommand(const std::wstring& Path)
{
    const std::string Narrow(Path.begin(), Path.end());
    const uintptr_t Object = LoadObjectByPath(Narrow);
    Log(std::string("Laden: ") + (Object ? NameOf(Object) + " (" + NameOf(Read<uintptr_t>(Object + 0x10)) + ")" : "fehlgeschlagen") + " " + Narrow);
}
// Tutorial-Hinweise pausieren die ganze Welt ueber den PauserPlayerState, auch beim Client. Ein Hinweis, der beim
// Kartenwechsel offen bleibt, haelt jede weitere Runde an: Niemand steigt am Bus aus, die Uhr steht bei 00:00.
// Das Spiel hat dafuer einen eigenen Schalter, der Tick setzt ihn in jeder Instanz, sobald das Subsystem existiert.
namespace
{
constexpr uintptr_t kTutorialSkipFlag = 0xB8; // UTutorialStateSubsystem::SkipTutorialFlag
uintptr_t TutorialSubsystem = 0;
int32_t TutorialSubsystemIndex = -1;

uintptr_t FindTutorialSubsystem()
{
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    // Das Subsystem lebt mit der GameInstance. Solange der Platz im Objekt-Array dasselbe Objekt haelt, gilt der Treffer.
    if (TutorialSubsystem && TutorialSubsystemIndex >= 0)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (TutorialSubsystemIndex >> 16));
        if (Chunk && Read<uintptr_t>(Chunk + (TutorialSubsystemIndex & 0xFFFF) * 0x18 + 8) == TutorialSubsystem)
            return TutorialSubsystem;
    }
    TutorialSubsystem = 0;
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x10) != 0)
            continue;
        for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class; Class = Read<uintptr_t>(Class + 0x40))
            if (NameOf(Class) == "TutorialStateSubsystem")
            {
                TutorialSubsystem = Object;
                TutorialSubsystemIndex = i;
                return Object;
            }
    }
    return 0;
}
}

void SkipTutorials(bool Report)
{
    const uintptr_t Tutorials = FindTutorialSubsystem();
    if (!Tutorials)
    {
        if (Report)
            Log("Tutorial: Subsystem nicht gefunden");
        return;
    }
    // Einmal festhalten, ob der Schalter einen Neustart ueberlebt, also im Spielstand liegt.
    static bool FirstCheck = true;
    if (FirstCheck)
    {
        FirstCheck = false;
        Log(Read<bool>(Tutorials + kTutorialSkipFlag) ? "Tutorial: Schalter beim Start schon gesetzt" : "Tutorial: Schalter beim Start aus");
    }
    if (Read<bool>(Tutorials + kTutorialSkipFlag))
    {
        if (Report)
            Log("Tutorial: Hinweise sind abgeschaltet");
        return;
    }
    static const uintptr_t Function = FindObject("SetForceSkipTutorials", "TutorialStateSubsystem");
    if (!Function)
    {
        Log("Tutorial: SetForceSkipTutorials nicht gefunden");
        return;
    }
    struct { bool bSkip; } Params{true};
    Read<ProcessEventFn*>(Tutorials)[kProcessEventIndex](Tutorials, Function, &Params);
    Log(Read<bool>(Tutorials + kTutorialSkipFlag) ? "Tutorial: Hinweise abgeschaltet" : "Tutorial: Schalter nicht gesetzt");
}
}
