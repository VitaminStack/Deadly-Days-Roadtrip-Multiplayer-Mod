// Client: Stellen, die Objekte erwarten, die es nur beim Host gibt oder die noch nicht repliziert sind.
#include "Spike.h"

namespace Spike
{
namespace
{
constexpr uintptr_t kClientRefreshCallSite = 0x5A7350B;
constexpr uintptr_t kClientRefreshFunction = 0x5A8BAB0;
constexpr uintptr_t kAttributeLookupCallSite = 0x59B7E17;
constexpr uintptr_t kAttributeLookupFunction = 0x4B68180;
constexpr uintptr_t kAttributeTickExit = 0x59B858A;
constexpr uintptr_t kComponentControllerRead = 0x58393CC;
constexpr uintptr_t kComponentControllerExit = 0x5839540;
constexpr uintptr_t kThreatFreezeBranch = 0x593DAF3;

// Die Funktion erwartet ein Objekt in this+370h, das beim Client nicht gesetzt ist. Dann Aufruf auslassen.
bool PatchClientRefresh(CodeWriter& Code)
{
    const uintptr_t Site = Base + kClientRefreshCallSite;
    if (!Expect(Site, {0xE8}, "im Client-Pfad"))
        return false;
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x83, 0xB9, 0x70, 0x03, 0x00, 0x00, 0x00}); // cmp qword ptr [rcx+370h],0
    Code.Emit({0x74, 0x05});                                     // je zurueck
    Code.Emit({0xE8});                                           // call original
    Code.EmitRel(Base + kClientRefreshFunction);
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 5);
    WriteJump(Site, Cave);
    return true;
}

// Der Tick sucht das AbilitySystem, bevor der PlayerState angekommen ist. Dann diesen Tick auslassen.
bool PatchAttributeTick(CodeWriter& Code)
{
    const uintptr_t Site = Base + kAttributeLookupCallSite;
    if (!Expect(Site, {0xE8}, "im Attribut-Tick"))
        return false;
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x85, 0xC9});                               // test rcx,rcx
    Code.Emit({0x0F, 0x84});                                     // je Tick-Ende
    Code.EmitRel(Base + kAttributeTickExit);
    Code.Emit({0xE8});                                           // call original
    Code.EmitRel(Base + kAttributeLookupFunction);
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 5);
    WriteJump(Site, Cave);
    return true;
}

// Eine Figurenkomponente holt im Tick den Controller. Die fremde Figur hat beim Client keinen.
bool PatchComponentController(CodeWriter& Code)
{
    const uintptr_t Site = Base + kComponentControllerRead;
    if (!Expect(Site, {0x48, 0x8B, 0x07, 0x48, 0x8D, 0x54, 0x24, 0x60}, "in der Figurenkomponente"))
        return false;
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x85, 0xFF});                               // test rdi,rdi
    Code.Emit({0x0F, 0x84});                                     // je Funktionsende
    Code.EmitRel(Base + kComponentControllerExit);
    Code.Emit({0x48, 0x8B, 0x07, 0x48, 0x8D, 0x54, 0x24, 0x60}); // mov rax,[rdi]; lea rdx,[rsp+60h]
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 8);
    WriteJump(Site, Cave);
    return true;
}

// FreezeThreatLevelManager entsteht nur beim Host, beim Aufraeumen der Welt wird er trotzdem benutzt.
// Das vorhandene je springt dann direkt ans Funktionsende.
bool PatchThreatLevel()
{
    const auto Branch = reinterpret_cast<uint8_t*>(Base + kThreatFreezeBranch);
    if (!Expect(Base + kThreatFreezeBranch, {0x74, 0x15}, "in ThreatLevel"))
        return false;
    DWORD Old = 0;
    VirtualProtect(Branch, 2, PAGE_EXECUTE_READWRITE, &Old);
    Branch[1] = 0x6A;
    VirtualProtect(Branch, 2, Old, &Old);
    FlushInstructionCache(GetCurrentProcess(), Branch, 2);
    return true;
}
}

bool PatchClient(CodeWriter& Code)
{
    return PatchClientRefresh(Code) && PatchAttributeTick(Code) && PatchComponentController(Code) && PatchThreatLevel();
}
}
