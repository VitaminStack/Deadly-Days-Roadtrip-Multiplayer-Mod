// Client: Die Inventar-Schleifen lesen nach dem Seamless Travel null-Eintraege. Reicht nicht, siehe README.
#include "Spike.h"

namespace Spike
{
namespace
{
constexpr uintptr_t kItemsLoopRead = 0x5963604;
constexpr uintptr_t kItemsLoopNext = 0x596365F;
constexpr uintptr_t kGridLoopRead = 0x5A7E060;
constexpr uintptr_t kGridLoopNext = 0x5A7E091;
constexpr uintptr_t kSlotLoopRead = 0x5965660;
constexpr uintptr_t kSlotLoopNext = 0x596572C;

// Das Item-Array enthaelt nach dem Seamless Travel null, das Spiel prueft das nicht.
bool PatchItemsLoop(CodeWriter& Code)
{
    const uintptr_t Site = Base + kItemsLoopRead;
    if (!Expect(Site, {0x48, 0x8B, 0x3B, 0x83, 0xFD, 0x05}, "in der Item-Schleife"))
        return false;
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x8B, 0x3B});                               // mov rdi,[rbx]
    Code.Emit({0x48, 0x85, 0xFF});                               // test rdi,rdi
    Code.Emit({0x0F, 0x84});                                     // je naechster Eintrag
    Code.EmitRel(Base + kItemsLoopNext);
    Code.Emit({0x83, 0xFD, 0x05});                               // cmp ebp,5
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 6);
    WriteJump(Site, Cave);
    return true;
}

// Dasselbe Raster wird auch fuer seine Ausdehnung durchlaufen.
bool PatchGridLoop(CodeWriter& Code)
{
    const uintptr_t Site = Base + kGridLoopRead;
    if (!Expect(Site, {0x48, 0x8B, 0x0E, 0x44, 0x8B, 0x41, 0x28}, "im Raster"))
        return false;
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x8B, 0x0E});                               // mov rcx,[rsi]
    Code.Emit({0x48, 0x85, 0xC9});                               // test rcx,rcx
    Code.Emit({0x0F, 0x84});                                     // je naechster Eintrag
    Code.EmitRel(Base + kGridLoopNext);
    Code.Emit({0x44, 0x8B, 0x41, 0x28});                         // mov r8d,[rcx+28h]
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 7);
    WriteJump(Site, Cave);
    return true;
}

// Dritte Schleife ueber dieselben Eintraege.
bool PatchSlotLoop(CodeWriter& Code)
{
    const uintptr_t Site = Base + kSlotLoopRead;
    if (!Expect(Site, {0x48, 0x8B, 0x07, 0x48, 0x8B, 0x58, 0x30}, "in der Slot-Schleife"))
        return false;
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x8B, 0x07});                               // mov rax,[rdi]
    Code.Emit({0x48, 0x85, 0xC0});                               // test rax,rax
    Code.Emit({0x0F, 0x84});                                     // je naechster Eintrag
    Code.EmitRel(Base + kSlotLoopNext);
    Code.Emit({0x48, 0x8B, 0x58, 0x30});                         // mov rbx,[rax+30h]
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 7);
    WriteJump(Site, Cave);
    return true;
}
}

bool PatchInventory(CodeWriter& Code)
{
    return PatchItemsLoop(Code) && PatchGridLoop(Code) && PatchSlotLoop(Code);
}
}
