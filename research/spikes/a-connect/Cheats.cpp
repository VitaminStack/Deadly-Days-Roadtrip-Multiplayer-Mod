// Testhilfen: CheatManager fuer die DD_-Befehle des Spiels und Laden einzelner Assets, etwa fuer einen SDK-Dump mit Bossen.
#include "Spike.h"

namespace Spike
{
namespace
{
constexpr uintptr_t kControllerCheatManager = 0x410;
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
