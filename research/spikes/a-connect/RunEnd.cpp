// Niederlage und Rueckkehr in die MetaBase (niederlage). Nach "Zurueck zur Basis" laeuft beim Host fuer jeden Controller
// ein Handler, der GetLocalPlayer()->...() ohne Nullpruefung aufruft. Der Remote-Controller hat keinen LocalPlayer.
#include "Spike.h"

namespace Spike
{
namespace
{
// Im Handler 0x584CD20: mov rcx,rax; mov rax,[rax]; call [rax+330h]; mov ebp,eax. Danach geht es bei +0xE weiter.
constexpr uintptr_t kRunEndLocalPlayerSite = 0x584CD82;
constexpr uintptr_t kRunEndContinue = 0x584CD90;
}

// Wie der Spieler-Index-Helfer in Players.cpp: Ohne LocalPlayer gilt der Remote-Spieler als P2 (Index 1).
bool PatchRunEnd(CodeWriter& Code)
{
    const uintptr_t Site = Base + kRunEndLocalPlayerSite;
    if (!Expect(Site, {0x48, 0x8B, 0xC8, 0x48, 0x8B, 0x00, 0xFF, 0x90, 0x30, 0x03, 0x00, 0x00, 0x8B, 0xE8}, "beim Run-Ende"))
        return false;
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x85, 0xC0});                               // test rax,rax
    Code.Emit({0x75, 0x0A});                                     // jne original
    Code.Emit({0xBD, 0x01, 0x00, 0x00, 0x00});                   // mov ebp,1
    Code.Emit({0xE9});                                           // jmp weiter
    Code.EmitRel(Base + kRunEndContinue);
    Code.Emit({0x48, 0x8B, 0xC8});                               // mov rcx,rax
    Code.Emit({0x48, 0x8B, 0x00});                               // mov rax,[rax]
    Code.Emit({0xFF, 0x90, 0x30, 0x03, 0x00, 0x00});             // call [rax+330h]
    Code.Emit({0x8B, 0xE8});                                     // mov ebp,eax
    Code.Emit({0xE9});                                           // jmp weiter
    Code.EmitRel(Base + kRunEndContinue);
    WriteJump(Site, Cave);
    return true;
}
}
