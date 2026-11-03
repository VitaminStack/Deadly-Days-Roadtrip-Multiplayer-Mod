// Client: Stellen, die Objekte erwarten, die es nur beim Host gibt oder die noch nicht repliziert sind.
#include "Spike.h"
#include "AttributeReadiness.h"

namespace Spike
{
namespace
{
constexpr uintptr_t kClientRefreshCallSite = 0x5A7350B;
constexpr uintptr_t kClientRefreshFunction = 0x5A8BAB0;
constexpr uintptr_t kAttributeLookupCallSite = 0x59B7E17;
constexpr uintptr_t kAttributeLookupFunction = 0x4B68180;
constexpr uintptr_t kAttributeTickExit = 0x59B858A;
constexpr uintptr_t kDayLookupCallSite = 0x5847AE6;
constexpr uintptr_t kDayLookupExit = 0x5847AF2;
constexpr uintptr_t kMapGenHostObjectRead = 0x59DFD5A;
constexpr uintptr_t kMapGenHostObjectExit = 0x59DFE07;
constexpr uintptr_t kComponentControllerRead = 0x58393CC;
constexpr uintptr_t kComponentControllerExit = 0x5839540;
constexpr uintptr_t kThreatFreezeBranch = 0x593DAF3;
constexpr uintptr_t kExpAggregate = 0x58B4160;
constexpr uintptr_t kGetPlayerCharacters = 0x5B07680;
constexpr uintptr_t kFreeArray = 0x12F8400;
using ExpAggregateFn = float(__fastcall*)(uintptr_t);
ExpAggregateFn OriginalExpAggregate = nullptr;

// The original reads [Character+7D8h]+80h for every character. A character without AttributeSet would crash the host too,
// so the check only changes cases that crash otherwise. Both callers store the result at +314h.
float __fastcall ReadyExpAggregate(uintptr_t Handler)
{
    struct CharacterArray { uintptr_t* Data; int32_t Num; int32_t Max; } Characters{};
    using CharactersFn = CharacterArray*(__fastcall*)(CharacterArray*, uintptr_t);
    reinterpret_cast<CharactersFn>(Base + kGetPlayerCharacters)(&Characters, Handler);
    const bool Complete = (!Characters.Num || Characters.Data) && AttributeReadiness::Complete(
        Characters.Num, Characters.Max,
        [&](int32_t Index) { return Characters.Data[Index]; },
        [](uintptr_t Character) { return Read<uintptr_t>(Character + 0x7D8) != 0; });
    if (Characters.Data)
        reinterpret_cast<void(__fastcall*)(void*)>(Base + kFreeArray)(Characters.Data);
    // No skipped characters, synthetic AttributeSets or replayed GAS initialization.
    // The native handler queries this again on every update, so a pending dependency retries naturally.
    if (!Complete)
    {
        static unsigned PendingCount = 0;
        if (++PendingCount <= 16)
            Log("Exp aggregate: awaiting character AttributeSet, sample=" + std::to_string(PendingCount));
        return AttributeReadiness::PendingValue(Read<float>(Handler + 0x314));
    }
    return OriginalExpAggregate(Handler);
}

bool PatchExpReadiness(CodeWriter& Code)
{
    const uintptr_t Site = Base + kExpAggregate;
    if (!Expect(Site, {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x40}, "exp aggregate") ||
        !Expect(Base + kGetPlayerCharacters, {0x48, 0x89, 0x5C, 0x24, 0x18, 0x48, 0x89, 0x74, 0x24, 0x20}, "player character snapshot") ||
        !Expect(Base + 0x58B419B, {0x48, 0x8B, 0x89, 0xD8, 0x07, 0x00, 0x00}, "exp AttributeSet read") ||
        !Expect(Base + 0x58A6060, {0xE8, 0xFB, 0xE0, 0x00, 0x00}, "exp BeginPlay aggregate") ||
        !Expect(Base + 0x58B0F6E, {0xE8, 0xED, 0x31, 0x00, 0x00}, "exp update aggregate"))
        return false;
    auto* Original = Code.Here();
    Code.Emit({0x48, 0x89, 0x5C, 0x24, 0x08}); // Complete, non-RIP-relative first instruction.
    Code.Emit({0xE9});
    Code.EmitRel(Site + 5);
    OriginalExpAggregate = reinterpret_cast<ExpAggregateFn>(Original);
    WriteJump(Site, Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(&ReadyExpAggregate)));
    return true;
}

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

// Die AbilitySystem-Suche liest [rcx+1080h] ohne Pruefung. Fehlt der Besitzer noch, den Rest bis Exit auslassen.
bool PatchAttributeLookup(CodeWriter& Code, uintptr_t Site, uintptr_t Exit)
{
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x85, 0xC9});                               // test rcx,rcx
    Code.Emit({0x0F, 0x84});                                     // je Exit
    Code.EmitRel(Exit);
    Code.Emit({0xE8});                                           // call original
    Code.EmitRel(Base + kAttributeLookupFunction);
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 5);
    WriteJump(Site, Cave);
    return true;
}

// Der Tick sucht das AbilitySystem, bevor der PlayerState angekommen ist. Dann diesen Tick auslassen.
bool PatchAttributeTick(CodeWriter& Code)
{
    if (!Expect(Base + kAttributeLookupCallSite, {0xE8}, "im Attribut-Tick"))
        return false;
    return PatchAttributeLookup(Code, Base + kAttributeLookupCallSite, Base + kAttributeTickExit);
}

// Nach OnRep_DayNum an Tag 4 liefert die virtuelle Funktion +10h beim Client noch keinen Besitzer. Die Funktion hat
// +C8h dann schon auf null gesetzt, der naechste Aufruf versucht es also erneut; nur +D0h bleibt bis dahin leer.
bool PatchDayAttributeLookup(CodeWriter& Code)
{
    if (!Expect(Base + kDayLookupCallSite - 3, {0x48, 0x8B, 0xCF, 0xE8, 0x95, 0x06, 0x32, 0xFF}, "AbilitySystem-Suche nach OnRep_DayNum") ||
        !Expect(Base + kDayLookupExit, {0x48, 0x8B, 0x5C, 0x24, 0x38}, "Ende der AbilitySystem-Suche nach OnRep_DayNum"))
        return false;
    return PatchAttributeLookup(Code, Base + kDayLookupCallSite, Base + kDayLookupExit);
}

// Ebenfalls nach OnRep_DayNum: ein Kartengenerator-Schritt (0x59DFC70) nutzt +C8h, das nur beim Host existiert.
// Der Rest der Funktion arbeitet nur damit; der fruehe Ausgang 0x59DFE07 laesst ihn samt Freigabe seiner Ausgabe aus.
bool PatchMapGenHostObject(CodeWriter& Code)
{
    const uintptr_t Site = Base + kMapGenHostObjectRead;
    if (!Expect(Site, {0x49, 0x8B, 0x9E, 0xC8, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x03}, "im Kartengenerator-Schritt") ||
        !Expect(Base + kMapGenHostObjectExit, {0x0F, 0x28, 0xB4, 0x24, 0x80, 0x00, 0x00, 0x00}, "Ende des Kartengenerator-Schritts"))
        return false;
    uint8_t* Cave = Code.Here();
    Code.Emit({0x49, 0x8B, 0x9E, 0xC8, 0x00, 0x00, 0x00});       // mov rbx,[r14+C8h]
    Code.Emit({0x48, 0x85, 0xDB});                               // test rbx,rbx
    Code.Emit({0x0F, 0x84});                                     // je Funktionsende
    Code.EmitRel(Base + kMapGenHostObjectExit);
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 7);
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
    return PatchClientRefresh(Code) && PatchAttributeTick(Code) && PatchDayAttributeLookup(Code) && PatchMapGenHostObject(Code) && PatchComponentController(Code) && PatchThreatLevel() &&
        PatchExpReadiness(Code);
}
}
