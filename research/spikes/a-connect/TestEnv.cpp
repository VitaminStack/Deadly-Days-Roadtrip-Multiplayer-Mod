// Testumgebung (Aufgabe testumgebung), nur Host.
// Diagnose: haengt sich an ProcessEvent der Blueprint-Figur und meldet EndPlay jeder Figur mit Aufrufstapel als RVAs.
// Korrektur: die Freiraeumung am Busplatz schont alle Spielerfiguren, siehe PatchTestEnv.
#include "Spike.h"

namespace Spike
{
namespace
{
ProcessEventFn OriginalCharacterProcessEvent = nullptr;
int32_t EndPlayName = -1; // FName-Index von ReceiveEndPlay, auch Blueprint-Ueberschreibungen tragen ihn

void __fastcall CharacterProcessEvent(uintptr_t Object, uintptr_t Function, void* Params)
{
    if (Function && Read<int32_t>(Function + 0x18) == EndPlayName)
    {
        void* Frames[48] = {};
        const USHORT Count = RtlCaptureStackBackTrace(1, 48, Frames, nullptr);
        std::string Stack;
        int Shown = 0;
        for (USHORT i = 0; i < Count && Shown < 24; ++i)
        {
            const uintptr_t Address = reinterpret_cast<uintptr_t>(Frames[i]);
            if (Address < Base || Address >= Base + 0x10000000)
                continue;
            char Text[16];
            snprintf(Text, sizeof(Text), " %llx", static_cast<unsigned long long>(Address - Base));
            Stack += Text;
            ++Shown;
        }
        const uintptr_t Controller = Read<uintptr_t>(Object + 0x2D8 /* APawn::Controller */);
        Log("Figur EndPlay: " + NameOf(Object) + " Grund=" + std::to_string(Params ? *static_cast<uint8_t*>(Params) : 255) +
            " Controller=" + (Controller ? NameOf(Controller) : std::string("null")) + " Stapel" + Stack);
    }
    OriginalCharacterProcessEvent(Object, Function, Params);
}
}

// Host: Beim Rundenstart setzt BusSpawnPositionGenerator (vtable-Platz 86, 0x59A6CE0) Bus und Figuren an den Spawnplatz
// und raeumt dann ueber 0x5991B70 alles frei, was die gesetzten Actors ueberlappt. Geschont wird nur, was in der
// uebergebenen Liste steht oder einem Eintrag gehoert. Die Figur des Remote-Spielers steht nicht darin, liegt aber auf
// derselben Stelle wie die Host-Figur und wurde deshalb zerstoert. Der Hook ergaenzt die Liste um alle Spielerfiguren.
namespace
{
const uintptr_t kClearSpawnArea = Rva(0x5991B70);
constexpr uintptr_t kPlayerStatePawn = 0x320;
struct ActorArray
{
    uintptr_t* Data;
    int32_t Num;
    int32_t Max;
};
using ClearSpawnAreaFn = void(__fastcall*)(const ActorArray*, uintptr_t);
ClearSpawnAreaFn OriginalClearSpawnArea = nullptr;

void __fastcall ClearSpawnAreaHook(const ActorArray* Keep, uintptr_t Context)
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t GameState = World ? Read<uintptr_t>(World + kWorldGameState) : 0;
    if (!Keep || Keep->Num <= 0 || Keep->Num > 60 || !GameState)
    {
        OriginalClearSpawnArea(Keep, Context);
        return;
    }
    // Nur lesend verwendet, deshalb reicht ein eigener Puffer fuer die Dauer des Aufrufs.
    uintptr_t Buffer[64];
    ActorArray Extended{Buffer, 0, 64};
    for (int32_t i = 0; i < Keep->Num; ++i)
        Buffer[Extended.Num++] = Keep->Data[i];
    const uintptr_t States = Read<uintptr_t>(GameState + kGameStatePlayerArray);
    const int32_t Count = Read<int32_t>(GameState + kGameStatePlayerArray + 8);
    int32_t Added = 0;
    for (int32_t i = 0; States && i < Count && i < 16 && Extended.Num < 64; ++i)
    {
        const uintptr_t Pawn = Read<uintptr_t>(Read<uintptr_t>(States + 8 * i) + kPlayerStatePawn);
        bool Known = !Pawn;
        for (int32_t j = 0; j < Extended.Num && !Known; ++j)
            Known = Buffer[j] == Pawn;
        if (!Known)
        {
            Buffer[Extended.Num++] = Pawn;
            ++Added;
        }
    }
    Log("Busplatz: " + std::to_string(Keep->Num) + " geschuetzt, " + std::to_string(Added) + " Spielerfiguren ergaenzt");
    OriginalClearSpawnArea(&Extended, Context);
}
}

bool PatchTestEnv(CodeWriter& Code)
{
    const uintptr_t Site = Base + kClearSpawnArea;
    if (!Expect(Site, {0x48, 0x89, 0x5C, 0x24, 0x18}, "in der Freiraeumung am Busplatz"))
        return false;
    OriginalClearSpawnArea = reinterpret_cast<ClearSpawnAreaFn>(Code.Here());
    Code.Emit({0x48, 0x89, 0x5C, 0x24, 0x18}); // mov [rsp+18h],rbx
    Code.Emit({0xE9});                          // jmp zurueck
    Code.EmitRel(Site + 5);
    WriteJump(Site, Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(&ClearSpawnAreaHook)));
    return true;
}

// Einmal beim Host aufrufen, sobald die Blueprint-Klasse der Spielerfigur geladen ist. Ihre Instanzen teilen die vtable der CDO.
void TickTestEnv()
{
    if (OriginalCharacterProcessEvent)
        return;
    const uintptr_t Defaults = FindObject("Default__BP_DD_Character_C", nullptr);
    const uintptr_t EndPlayFunction = FindObject("ReceiveEndPlay", "Actor");
    if (!Defaults || !EndPlayFunction)
        return;
    EndPlayName = Read<int32_t>(EndPlayFunction + 0x18);
    auto* Slot = &Read<ProcessEventFn*>(Defaults)[kProcessEventIndex];
    DWORD Old = 0;
    VirtualProtect(Slot, sizeof(*Slot), PAGE_READWRITE, &Old);
    OriginalCharacterProcessEvent = *Slot;
    *Slot = CharacterProcessEvent;
    VirtualProtect(Slot, sizeof(*Slot), Old, &Old);
    Log("Diagnose: EndPlay der Figuren wird protokolliert");
}
}
