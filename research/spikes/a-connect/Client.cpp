// Client: Stellen, die Objekte erwarten, die es nur beim Host gibt oder die noch nicht repliziert sind.
#include "Spike.h"
#include "AttributeReadiness.h"
#include "BlockadeAuthority.h"
#include "NetGuidExpiryGuard.h"
#include "SneakTargetReadiness.h"

namespace Spike
{
namespace
{
const uintptr_t kClientRefreshCallSite = Rva(0x5A7350B);
const uintptr_t kClientRefreshFunction = Rva(0x5A8BAB0);
const uintptr_t kAttributeLookupCallSite = Rva(0x59B7E17);
const uintptr_t kAttributeLookupFunction = Rva(0x4B68180);
const uintptr_t kAttributeTickExit = Rva(0x59B858A);
const uintptr_t kMapGenHostObjectRead = Rva(0x59DFD5A);
const uintptr_t kMapGenHostObjectExit = Rva(0x59DFE07);
const uintptr_t kComponentControllerRead = Rva(0x58393CC);
const uintptr_t kComponentControllerExit = Rva(0x5839540);
const uintptr_t kThreatFreezeBranch = Rva(0x593DAF3);
const uintptr_t kExpAggregate = Rva(0x58B4160);
const uintptr_t kGetPlayerCharacters = Rva(0x5B07680);
const uintptr_t kFreeArray = Rva(0x12F8400);
const uintptr_t kBlockadeLoadingFinished = Rva(0x58BB360);
using BlockadeLoadingFinishedFn = void(__fastcall*)(uintptr_t);
BlockadeLoadingFinishedFn OriginalBlockadeLoadingFinished = nullptr;

// OnLoadingFinished only schedules the delayed virtual blockade start (timer +118h). That start creates objectives
// and spawns enemies through a cached authoritative GameMode. It is not component BeginPlay or replicated visuals.
// Gate the common base entry, not one day/biome/subclass, and do not manufacture a GameMode on the client.
void __fastcall AuthorityBlockadeLoadingFinished(uintptr_t Component)
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Driver = World ? Read<uintptr_t>(World + kWorldNetDriver) : 0;
    const bool ServerConnection = Driver && Read<uintptr_t>(Driver + kDriverServerConnection);
    if (!BlockadeAuthority::MayStart(World != 0, Driver != 0, ServerConnection))
    {
        Log("Blockade: lokaler Autoritaetsablauf unterdrueckt, Klasse=" + NameOf(Read<uintptr_t>(Component + 0x10)));
        return;
    }
    OriginalBlockadeLoadingFinished(Component);
}

bool PatchBlockadeAuthority(CodeWriter& Code)
{
    if (OriginalBlockadeLoadingFinished)
        return true;
    const uintptr_t Site = Base + kBlockadeLoadingFinished;
    if (!Expect(Site, {0x40, 0x53, 0x48, 0x81, 0xEC, 0xB0, 0x00, 0x00, 0x00}, "Blockade OnLoadingFinished") ||
        !ExpectBranch(Base + Rva(0x5666FA5), 0xE9, 0x58BB360, "Blockade reflected entry") ||
        !Expect(Base + Rva(0x58AA5F0), {0x48, 0x8B, 0x09, 0x48, 0x8B, 0x01, 0x48, 0xFF, 0xA0, 0xE8, 0x04, 0x00, 0x00}, "Blockade delayed virtual start"))
        return false;
    OriginalBlockadeLoadingFinished = reinterpret_cast<BlockadeLoadingFinishedFn>(Code.Here());
    Code.Emit({0x40, 0x53, 0x48, 0x81, 0xEC, 0xB0, 0x00, 0x00, 0x00}); // Whole, non-RIP-relative prologue.
    Code.Emit({0xE9});
    Code.EmitRel(Site + 9);
    WriteJump(Site, Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(&AuthorityBlockadeLoadingFinished)));
    return true;
}

// The server keeps using ACKed static path GUIDs across our non-seamless travels. Expiring reloadable aliases on the
// client loses that agreed identity, even after the package is loaded again. Keep just those path records until the
// connection's cache dies, not dynamic GUIDs, NoLoad level actors, broken entries or strong UObject references.
// Crucially, retain ReadOnlyTimestamp: clearing it re-enters the duplicate-object canonicalization path.
bool PatchReloadableGuidExpiry(CodeWriter& Code)
{
    const uintptr_t Site = Base + Rva(0x2CF362E);
    if (!Expect(Site, {0x0F, 0x28, 0xC6, 0xF2, 0x0F, 0x5C, 0xC1}, "NetGUID expiry arithmetic") ||
        !Expect(Site + 7, {0x66, 0x0F, 0x2F, 0xC7}, "NetGUID expiry comparison"))
        return false;
    const auto Original = reinterpret_cast<uintptr_t>(Code.Here());
    Code.Emit({0x0F, 0x28, 0xC6, 0xF2, 0x0F, 0x5C, 0xC1});
    Code.Emit({0xE9});
    Code.EmitRel(Site + 7);
    auto* Guard = Code.Here();
    NetGuidPolicy::EmitExpiryGuard(Code, Original, Base + Rva(0x2CF3C38));
    WriteJump(Site, Guard);
    Log("NetGUID: Client-Pfadidentitaeten bleiben bei Ablauf erhalten; Weak-Pointer und ReadOnly-Stempel unveraendert");
    return true;
}

// SneakApproach caches the selected pawn before its directional mesh/PlayerState may have replicated. The next tick
// used to return success just because the pawn is unchanged, then dereferenced the still-null cached mesh.
// Retry the original initialization until both dependencies exist; do not suppress all client enemy behavior.
bool PatchSneakTargetReadiness(CodeWriter& Code)
{
    const uintptr_t Site = Base + Rva(0x58A5266);
    if (!Expect(Site - 3, {0x48, 0x3B, 0xC3, 0x0F, 0x84, 0xA1, 0x00, 0x00, 0x00}, "SneakApproach same target") ||
        !Expect(Base + Rva(0x58A52FE), {0x48, 0x83, 0xBF, 0xE8, 0x00, 0x00, 0x00, 0x00}, "SneakApproach dependency check"))
        return false;
    auto* Guard = Code.Here();
    SneakTargetReadiness::Emit(Code, Base + Rva(0x58A526C), Base + Rva(0x58A530D));
    WriteJump(Site, Guard);
    Log("SneakApproach: gleiches Ziel wird erst mit Mesh und PlayerState wiederverwendet");
    return true;
}

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

const uintptr_t kSerializeNewActor = Rva(0x2D09B90);
const uintptr_t kNewActorArchetypeCheck = Rva(0x2D0A426);
using SerializeNewActorFn = bool(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t*);
SerializeNewActorFn OriginalSerializeNewActor = nullptr;
uintptr_t SeenArchetype = 0;

// Nur zur Diagnose: Actors, deren Archetyp der Client nicht aufloesen kann. Ihr Kanal bleibt fuer die Karte Broken.
bool __fastcall LoggingSerializeNewActor(uintptr_t PackageMap, uintptr_t Archive, uintptr_t Channel, uintptr_t* Actor)
{
    SeenArchetype = 1;
    const bool Spawned = OriginalSerializeNewActor(PackageMap, Archive, Channel, Actor);
    if (!(Read<uint8_t>(Archive + 0x28) & 1) || *Actor) // Nur beim Lesen, der Host schreibt hier.
        return Spawned;
    static unsigned FailedCount = 0;
    if (++FailedCount <= 32)
    {
        char Line[256];
        sprintf_s(Line, "NewActor failed: guid=0x%llX ch=%d archetype=%s", Read<unsigned long long>(Channel + 0x70),
            Read<int32_t>(Channel + 0x34), SeenArchetype > 1 ? NameOf(SeenArchetype).c_str() : (SeenArchetype ? "nicht erreicht" : "null"));
        Log(Line);
    }
    return Spawned;
}

bool PatchNewActorLog(CodeWriter& Code)
{
    const uintptr_t Site = Base + kSerializeNewActor;
    const uintptr_t Check = Base + kNewActorArchetypeCheck;
    if (!Expect(Site, {0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x55}, "SerializeNewActor") ||
        !Expect(Check, {0x48, 0x83, 0x7C, 0x24, 0x38, 0x00, 0x0F, 0x84}, "Archetyp-Pruefung in SerializeNewActor"))
        return false;
    auto* Original = Code.Here();
    Code.Emit({0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x55});       // push rbp/rbx/rsi/rdi/r13
    Code.Emit({0xE9});
    Code.EmitRel(Site + 7);
    OriginalSerializeNewActor = reinterpret_cast<SerializeNewActorFn>(Original);
    WriteJump(Site, Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(&LoggingSerializeNewActor)));

    // Den aufgeloesten Archetyp dort merken, wo die Funktion ueber das Spawnen entscheidet.
    uint8_t* Cave = Code.Here();
    const uintptr_t Slot = reinterpret_cast<uintptr_t>(&SeenArchetype);
    Code.Emit({0x48, 0x8B, 0x44, 0x24, 0x38});                   // mov rax,[rsp+38h]
    Code.Emit({0x48, 0xA3});                                     // mov [Slot],rax
    for (int Byte = 0; Byte < 8; ++Byte)
        Code.Emit({static_cast<uint8_t>(Slot >> (8 * Byte))});
    Code.Emit({0x48, 0x83, 0x7C, 0x24, 0x38, 0x00});             // cmp qword ptr [rsp+38h],0
    Code.Emit({0xE9});                                           // jmp zurueck zum je
    Code.EmitRel(Check + 6);
    WriteJump(Check, Cave);
    return true;
}

bool PatchExpReadiness(CodeWriter& Code)
{
    const uintptr_t Site = Base + kExpAggregate;
    if (!Expect(Site, {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x40}, "exp aggregate") ||
        !Expect(Base + kGetPlayerCharacters, {0x48, 0x89, 0x5C, 0x24, 0x18, 0x48, 0x89, 0x74, 0x24, 0x20}, "player character snapshot") ||
        !Expect(Base + Rva(0x58B419B), {0x48, 0x8B, 0x89, 0xD8, 0x07, 0x00, 0x00}, "exp AttributeSet read") ||
        !ExpectBranch(Base + Rva(0x58A6060), 0xE8, 0x58B4160, "exp BeginPlay aggregate") ||
        !ExpectBranch(Base + Rva(0x58B0F6E), 0xE8, 0x58B4160, "exp update aggregate"))
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

// Die AttributeSet-Suche selbst: Beim Client ist das AbilitySystem nach einem Kartenwechsel oft noch nicht da, ueber 30
// Aufrufer reichen dann null herein. Ohne AbilitySystem gibt sie null zurueck wie fuer ein fehlendes Set, das pruefen
// die Aufrufer schon. Ersetzt die frueheren Schutzstellen je Aufrufer nach OnRep_DayNum.
bool PatchAttributeLookupEntry(CodeWriter& Code)
{
    const uintptr_t Site = Base + kAttributeLookupFunction;
    if (!Expect(Site, {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18}, "Anfang der AttributeSet-Suche") ||
        !Expect(Site + 0x15, {0x48, 0x8B, 0x99, 0x80, 0x10, 0x00, 0x00}, "Lesen der AttributeSets"))
        return false;
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x85, 0xC9});                               // test rcx,rcx
    Code.Emit({0x75, 0x03});                                     // jne original
    Code.Emit({0x31, 0xC0, 0xC3});                               // xor eax,eax; ret
    Code.Emit({0x48, 0x89, 0x5C, 0x24, 0x10});                   // mov [rsp+10h],rbx
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 5);
    WriteJump(Site, Cave);
    return true;
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
    return PatchClientRefresh(Code) && PatchAttributeTick(Code) && PatchAttributeLookupEntry(Code) && PatchMapGenHostObject(Code) && PatchComponentController(Code) && PatchThreatLevel() &&
        PatchExpReadiness(Code) && PatchNewActorLog(Code) && PatchBlockadeAuthority(Code) && PatchReloadableGuidExpiry(Code) &&
        PatchSneakTargetReadiness(Code);
}
}
