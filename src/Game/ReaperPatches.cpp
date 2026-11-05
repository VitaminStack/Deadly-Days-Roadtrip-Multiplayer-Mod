// Sensenmann am regulaeren Rundenende (rundenende-absturz). Laeuft die Zeit eines Tages ab, erscheint der Reaper mit einer
// UReaperComponent. Zwei Stellen setzen Objekte voraus, die es zu zweit nicht immer gibt:
// - Beim Start bindet die Komponente OnNewPlayerSpawned/OnPlayerDespawned des ADDMissionGameMode (+0x188). Den GameMode
//   gibt es nur beim Host, der Client las null+0x348 und stuerzte ab. Ohne GameMode ueberspringt der Patch nur diese
//   beiden Bindungen; Sound, Kontrast und Saettigung des Reapers laufen beim Client lokal weiter.
// - Eine Abstandsschleife liest jede Figur aus Players (+0x178). Verschwindet eine Figur (Client getrennt oder
//   abgestuerzt), steht dort null, und der Host las null+0x1B8. Leere Eintraege ueberspringt der Patch.
// Beide Patches greifen nur im Fehlerfall und aendern das Verhalten mit vorhandenen Objekten nicht.
#include "Game/CodeCave.h"
#include "Game/PatchRuntime.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t ReaperBindGameMode = 0x586F780; // lea rcx,[rbx+340h], rbx = GameMode
constexpr uintptr_t ReaperAfterBindings = 0x586F7BB; // nach der zweiten Bindung
constexpr uintptr_t ReaperPlayerRead = 0x5897A20; // mov rax,[rcx]; mov rdx,[rax+1B8h]
constexpr uintptr_t ReaperNextPlayer = 0x5897A70; // add rcx,8

bool CheckBind()
{
    return Code::Expect(At(ReaperBindGameMode), {0x48, 0x8D, 0x8B, 0x40, 0x03, 0x00, 0x00}, "beim Start des Reapers") &&
           Code::Expect(At(ReaperAfterBindings), {0x48, 0x8B, 0x87, 0xB0, 0x00, 0x00, 0x00}, "nach den GameMode-Bindungen des Reapers");
}

bool ApplyBind(Code::Writer& Code)
{
    const uintptr_t Site = At(ReaperBindGameMode);
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x85, 0xDB}); // test rbx,rbx
    Code.Emit({0x0F, 0x84});
    Code.EmitRel(At(ReaperAfterBindings));
    Code.Emit({0x48, 0x8D, 0x8B, 0x40, 0x03, 0x00, 0x00}); // lea rcx,[rbx+340h]
    Code.Emit({0xE9});
    Code.EmitRel(Site + 7);
    return !Code.Failed() && Code::WriteJump(Site, Cave);
}

bool CheckPlayers()
{
    return Code::Expect(At(ReaperPlayerRead), {0x48, 0x8B, 0x01, 0x48, 0x8B, 0x90, 0xB8, 0x01, 0x00, 0x00}, "in der Spielerschleife des Reapers") &&
           Code::Expect(At(ReaperNextPlayer), {0x48, 0x83, 0xC1, 0x08}, "am Ende der Spielerschleife des Reapers");
}

bool ApplyPlayers(Code::Writer& Code)
{
    const uintptr_t Site = At(ReaperPlayerRead);
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x8B, 0x01}); // mov rax,[rcx]
    Code.Emit({0x48, 0x85, 0xC0}); // test rax,rax
    Code.Emit({0x0F, 0x84});
    Code.EmitRel(At(ReaperNextPlayer));
    Code.Emit({0x48, 0x8B, 0x90, 0xB8, 0x01, 0x00, 0x00}); // mov rdx,[rax+1B8h]
    Code.Emit({0xE9});
    Code.EmitRel(Site + 10);
    return !Code.Failed() && Code::WriteJump(Site, Cave);
}

const Patch List[] = {
    {"Reaper ohne GameMode", &CheckBind, &ApplyBind},
    {"Reaper ohne verschwundene Figur", &CheckPlayers, &ApplyPlayers},
};
}

std::span<const Patch> ReaperPatches()
{
    return List;
}
}
