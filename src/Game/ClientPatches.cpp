// Client: Stellen, die Objekte erwarten, die es nur beim Host gibt oder die noch nicht repliziert sind. Dazu lokale
// Kopien replizierter PowerUp-Objekte. Aus Spike Client.cpp und MapObjects.cpp.
#include <atomic>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchPolicy.h"
#include "Game/PatchRuntime.h"
#include "Game/Reflection.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t ClientRefreshCallSite = 0x5A782FB;
constexpr uintptr_t ClientRefreshFunction = 0x5A90940;
constexpr uintptr_t AttributeLookupCallSite = 0x59BBFC7;
constexpr uintptr_t AttributeLookupFunction = 0x4B68380;
constexpr uintptr_t AttributeTickExit = 0x59BC73A;
constexpr uintptr_t MapGenHostObjectRead = 0x59E4B2A;
constexpr uintptr_t MapGenHostObjectExit = 0x59E4BD7;
constexpr uintptr_t ComponentControllerRead = 0x583AEEC;
constexpr uintptr_t ComponentControllerExit = 0x583B060;
constexpr uintptr_t ThreatFreezeBranch = 0x5941A43;
constexpr uintptr_t ExpAggregate = 0x58B4D30;
constexpr uintptr_t GetPlayerCharacters = 0x5B0CE30;
constexpr uintptr_t FreeArray = 0x12F85E0;
constexpr uintptr_t BlockadeLoadingFinished = 0x58BBD90;
constexpr uintptr_t NetGuidExpiry = 0x2CF382E;
constexpr uintptr_t NetGuidNext = 0x2CF3E38;
constexpr uintptr_t SneakSameTarget = 0x58A6466;
constexpr uintptr_t SneakRetry = 0x58A646C;
constexpr uintptr_t SneakReady = 0x58A650D;
constexpr uintptr_t BeginDeferredActorSpawnFromClass = 0x2A67AF0;
constexpr uintptr_t CanBusTakeOff = 0x59C7FC0;
constexpr uintptr_t FinishMission = 0x58EA1A0;
constexpr uintptr_t TriggerEndRound = 0x5907ED0;
constexpr uint32_t RfBeginDestroyed = 0x8000, RfFinishDestroyed = 0x10000; // EObjectFlags

constexpr uintptr_t CharacterAttributeSet = 0x7D8;
constexpr uintptr_t ExpHandlerResult = 0x314;

// Nur zur Kontrolle im Log, damit ein Lauf zeigt, welche Schutzstellen griffen.
void NoteOnce(std::atomic<int>& Counter, const char* Text)
{
    if (Counter.fetch_add(1) < 3)
        Core::Log::Info("{}", Text);
}

using BlockadeLoadingFinishedFn = void(__fastcall*)(uintptr_t);
BlockadeLoadingFinishedFn OriginalBlockadeLoadingFinished = nullptr;
std::atomic<int> BlockadeSkipped = 0;

// OnLoadingFinished plant nur den verzögerten Start der Blockade, der Ziele anlegt und über einen Host-GameMode Gegner
// spawnt. Das darf nur der Host, Anzeige und Komponenten bleiben beim Client unberührt.
void __fastcall BlockadeLoadingFinishedHook(uintptr_t Component)
{
    const NetRole Role = CurrentRole();
    if (!BlockadeMayStart(CurrentWorld() != 0, Role != NetRole::Standalone, Role == NetRole::Client))
    {
        NoteOnce(BlockadeSkipped, "Blockade: Ablauf des Hosts beim Client unterdrückt");
        return;
    }
    OriginalBlockadeLoadingFinished(Component);
}

using CanBusTakeOffFn = bool(__fastcall*)(uintptr_t, uint8_t*);
CanBusTakeOffFn OriginalCanBusTakeOff = nullptr;
std::atomic<int> BusTakeOffSkipped = 0;

// ExtractionPointBase.CanBusTakeOff, nativ aus CheckTakeOffCondition und aus dem Bus-Blueprint. Beim Client immer nein,
// Grund None, damit keine falsche Meldung erscheint. Abfahrt und Rundenende erreichen ihn über den Host.
bool __fastcall CanBusTakeOffHook(uintptr_t ExtractionPoint, uint8_t* OutReason)
{
    if (!BusTakeOffMayDecide(CurrentRole() == NetRole::Client))
    {
        NoteOnce(BusTakeOffSkipped, "Bus: Abfahrtsprüfung beim Client unterdrückt, der Host entscheidet");
        if (OutReason)
            *OutReason = 0;
        return false;
    }
    return OriginalCanBusTakeOff(ExtractionPoint, OutReason);
}

using FinishMissionFn = void(__fastcall*)(uintptr_t, bool);
FinishMissionFn OriginalFinishMission = nullptr;
std::atomic<int> FinishMissionSkipped = 0;

bool FollowingHost = false; // nur Game Thread

// run-ende: FinishMission setzt beim letzten Tag den Grund der Runde auf Sieg, sonst steht dort das Tagesende.
bool LastRoundWasWin()
{
    constexpr Reflection::FieldSpec Reason{"/Script/DDSurvivors", "DDMissionGameInstance", "LastRoundChangeReason", "EnumProperty", 1};
    const uintptr_t World = CurrentWorld();
    const uintptr_t GameInstance = World ? Memory::ReadPointer(World + Offsets::UWorld::OwningGameInstance).value_or(0) : 0;
    const auto Field = Reflection::Property(Reason);
    return GameInstance && Field && Memory::Read<uint8_t>(GameInstance + Field->Offset).value_or(0) == RoundChangeWin;
}

// DDMissionGameState.FinishMission(IsLoading). Zweite Sperre für jeden anderen lokalen Weg zum Rundenende beim Client. Der
// Host meldet sein Rundenende dem Client, der es dann mit FollowHostRoundEnd lokal nachzieht (rundenende-client).
void __fastcall FinishMissionHook(uintptr_t GameState, bool IsLoading)
{
    const NetRole Role = CurrentRole();
    if (!MissionFinishAllowed(Role == NetRole::Client, IsLoading || FollowingHost))
    {
        NoteOnce(FinishMissionSkipped, "Rundenende: FinishMission beim Client unterdrückt, der Host entscheidet");
        return;
    }
    if (Role == NetRole::Host && !IsLoading)
        NoteHostRoundEnd();
    OriginalFinishMission(GameState, IsLoading);
    if (Role == NetRole::Host && !IsLoading && LastRoundWasWin())
        NoteHostRunWon();
}

using TriggerEndRoundFn = void(__fastcall*)(uintptr_t, uint8_t);
TriggerEndRoundFn OriginalTriggerEndRound = nullptr;
std::atomic<int> RunLossSkipped = 0;

// run-ende: DDMissionGameInstance.TriggerEndRound. Stirbt der letzte Spieler, ruft das Spiel es mit Niederlage auf. Der Host meldet
// das dem Client. Beim Client läuft die Niederlage nie selbst: Im Test änderte sie seinen Meta-Stand und öffnete keinen Bildschirm.
void __fastcall TriggerEndRoundHook(uintptr_t GameInstance, uint8_t Reason)
{
    const NetRole Role = CurrentRole();
    if (!RunLossAllowed(Role == NetRole::Client, Reason))
    {
        NoteOnce(RunLossSkipped, "Run-Ende: Niederlage beim Client unterdrückt, der Host entscheidet");
        return;
    }
    if (Role == NetRole::Host && Reason == RoundChangeLose)
        NoteHostRoundEnd(true);
    OriginalTriggerEndRound(GameInstance, Reason);
}

using ExpAggregateFn = float(__fastcall*)(uintptr_t);
ExpAggregateFn OriginalExpAggregate = nullptr;
std::atomic<int> ExpPending = 0;

struct CharacterArray
{
    uintptr_t* Data;
    int32_t Num;
    int32_t Max;
};

// Das Original liest [Figur+7D8h]+80h für jede Figur. Ohne AttributeSet stürzte das ab, beim Host genauso.
float __fastcall ExpAggregateHook(uintptr_t Handler)
{
    CharacterArray Characters{};
    using CharactersFn = CharacterArray*(__fastcall*)(CharacterArray*, uintptr_t);
    reinterpret_cast<CharactersFn>(At(GetPlayerCharacters))(&Characters, Handler);
    const bool Complete = (!Characters.Num || Characters.Data) &&
                          AttributesComplete(Characters.Num, Characters.Max, [&](int32_t Index) { return Characters.Data[Index]; },
                                             [](uintptr_t Character) { return Memory::ReadPointer(Character + CharacterAttributeSet).has_value(); });
    if (Characters.Data)
        reinterpret_cast<void(__fastcall*)(void*)>(At(FreeArray))(Characters.Data);
    // Der Handler fragt bei jedem Update erneut, eine fehlende Figur kommt also von selbst nach.
    if (!Complete)
    {
        NoteOnce(ExpPending, "Exp: warte auf das AttributeSet einer Figur");
        return PendingAttributeValue(Memory::Read<float>(Handler + ExpHandlerResult).value_or(0.0f));
    }
    return OriginalExpAggregate(Handler);
}

using BeginDeferredSpawnFn = uintptr_t(__fastcall*)(uintptr_t, uintptr_t*, const void*, uint8_t, uintptr_t, uint8_t);
BeginDeferredSpawnFn OriginalBeginDeferredSpawn = nullptr;
std::atomic<int> PowerUpSkipped = 0;

// BP_PowerUpCrate_C spawnt in BeginPlay ohne Autoritätsprüfung seine BP_ChaosDestructable_PowerUp_C. Beim Client
// kommt dieselbe vom Host repliziert an, die lokale wäre doppelt.
uintptr_t __fastcall BeginDeferredSpawnHook(uintptr_t WorldContext, uintptr_t* ActorClass, const void* Transform, uint8_t Collision,
                                            uintptr_t Owner, uint8_t ScaleMethod)
{
    if (WorldContext && ActorClass && *ActorClass && CurrentRole() == NetRole::Client &&
        Memory::NameOf(*ActorClass) == "BP_ChaosDestructable_PowerUp_C" && IsA(WorldContext, "BP_PowerUpCrate_C"))
    {
        NoteOnce(PowerUpSkipped, "PowerUp: lokale Kopie beim Client unterdrückt");
        return 0;
    }
    return OriginalBeginDeferredSpawn(WorldContext, ActorClass, Transform, Collision, Owner, ScaleMethod);
}

bool CheckClientRefresh()
{
    return ExpectBranch(At(ClientRefreshCallSite), CallRel32, ClientRefreshFunction, "im Client-Refresh");
}

// Die Funktion erwartet ein Objekt in this+370h, das beim Client fehlt. Dann den Aufruf auslassen.
bool ApplyClientRefresh(Code::Writer& Code)
{
    const uintptr_t Site = At(ClientRefreshCallSite);
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x83, 0xB9, 0x70, 0x03, 0x00, 0x00, 0x00}); // cmp qword ptr [rcx+370h],0
    Code.Emit({0x74, 0x05});                                     // je zurück
    Code.Emit({0xE8});                                           // call original
    Code.EmitRel(At(ClientRefreshFunction));
    Code.Emit({0xE9});
    Code.EmitRel(Site + 5);
    return !Code.Failed() && Code::WriteJump(Site, Cave);
}

bool CheckAttributeTick()
{
    return ExpectBranch(At(AttributeLookupCallSite), CallRel32, AttributeLookupFunction, "im Attribut-Tick");
}

// Der Tick sucht das AbilitySystem, bevor der PlayerState da ist. Dann den Rest des Ticks auslassen.
bool ApplyAttributeTick(Code::Writer& Code)
{
    const uintptr_t Site = At(AttributeLookupCallSite);
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x85, 0xC9}); // test rcx,rcx
    Code.Emit({0x0F, 0x84});       // je Ausgang
    Code.EmitRel(At(AttributeTickExit));
    Code.Emit({0xE8});
    Code.EmitRel(At(AttributeLookupFunction));
    Code.Emit({0xE9});
    Code.EmitRel(Site + 5);
    return !Code.Failed() && Code::WriteJump(Site, Cave);
}

bool CheckAttributeLookup()
{
    const uintptr_t Site = At(AttributeLookupFunction);
    return Code::Expect(Site, {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18}, "Anfang der AttributeSet-Suche") &&
           Code::Expect(Site + 0x15, {0x48, 0x8B, 0x99, 0x80, 0x10, 0x00, 0x00}, "Lesen der AttributeSets");
}

// Die AttributeSet-Suche selbst liest [rcx+1080h] ohne Prüfung, über 30 Aufrufer reichen beim Client nach einem
// Kartenwechsel null herein. Dann null zurück wie für ein fehlendes Set, das prüfen die Aufrufer schon.
bool ApplyAttributeLookup(Code::Writer& Code)
{
    const uintptr_t Site = At(AttributeLookupFunction);
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x85, 0xC9});             // test rcx,rcx
    Code.Emit({0x75, 0x03});                   // jne original
    Code.Emit({0x31, 0xC0, 0xC3});             // xor eax,eax; ret
    Code.Emit({0x48, 0x89, 0x5C, 0x24, 0x10}); // mov [rsp+10h],rbx
    Code.Emit({0xE9});
    Code.EmitRel(Site + 5);
    return !Code.Failed() && Code::WriteJump(Site, Cave);
}

bool CheckMapGenHostObject()
{
    return Code::Expect(At(MapGenHostObjectRead), {0x49, 0x8B, 0x9E, 0xC8, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x03}, "im Kartengenerator-Schritt") &&
           Code::Expect(At(MapGenHostObjectExit), {0x0F, 0x28, 0xB4, 0x24, 0x80, 0x00, 0x00, 0x00}, "Ende des Kartengenerator-Schritts");
}

// Ein Kartengenerator-Schritt nutzt +C8h, das nur beim Host existiert. Ohne den Wert ans Funktionsende.
bool ApplyMapGenHostObject(Code::Writer& Code)
{
    const uintptr_t Site = At(MapGenHostObjectRead);
    uint8_t* Cave = Code.Here();
    Code.Emit({0x49, 0x8B, 0x9E, 0xC8, 0x00, 0x00, 0x00}); // mov rbx,[r14+C8h]
    Code.Emit({0x48, 0x85, 0xDB});                         // test rbx,rbx
    Code.Emit({0x0F, 0x84});
    Code.EmitRel(At(MapGenHostObjectExit));
    Code.Emit({0xE9});
    Code.EmitRel(Site + 7);
    return !Code.Failed() && Code::WriteJump(Site, Cave);
}

bool CheckComponentController()
{
    return Code::Expect(At(ComponentControllerRead), {0x48, 0x8B, 0x07, 0x48, 0x8D, 0x54, 0x24, 0x60}, "in der Figurenkomponente");
}

// Eine Figurenkomponente holt im Tick den Controller. Die fremde Figur hat beim Client keinen.
bool ApplyComponentController(Code::Writer& Code)
{
    const uintptr_t Site = At(ComponentControllerRead);
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x85, 0xFF}); // test rdi,rdi
    Code.Emit({0x0F, 0x84});
    Code.EmitRel(At(ComponentControllerExit));
    Code.Emit({0x48, 0x8B, 0x07, 0x48, 0x8D, 0x54, 0x24, 0x60}); // mov rax,[rdi]; lea rdx,[rsp+60h]
    Code.Emit({0xE9});
    Code.EmitRel(Site + 8);
    return !Code.Failed() && Code::WriteJump(Site, Cave);
}

bool CheckThreatLevel()
{
    return Code::Expect(At(ThreatFreezeBranch), {0x74, 0x15}, "in ThreatLevel");
}

// FreezeThreatLevelManager gibt es nur beim Host, das Aufräumen der Welt nutzt ihn trotzdem. Das vorhandene je
// springt dann direkt ans Funktionsende.
bool ApplyThreatLevel(Code::Writer&)
{
    const uint8_t Displacement = 0x6A;
    return Code::WriteCode(At(ThreatFreezeBranch) + 1, &Displacement, 1);
}

bool CheckExp()
{
    return Code::Expect(At(ExpAggregate), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x40}, "Exp-Aggregation") &&
           Code::Expect(At(GetPlayerCharacters), {0x48, 0x89, 0x5C, 0x24, 0x18, 0x48, 0x89, 0x74, 0x24, 0x20}, "Figurenliste") &&
           Code::Expect(At(0x58B4D6B), {0x48, 0x8B, 0x89, 0xD8, 0x07, 0x00, 0x00}, "AttributeSet in der Exp-Aggregation") &&
           ExpectBranch(At(0x58A7260), CallRel32, ExpAggregate, "Exp-Aggregation aus BeginPlay") &&
           ExpectBranch(At(0x58B1D3E), CallRel32, ExpAggregate, "Exp-Aggregation aus dem Update");
}

bool ApplyExp(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(ExpAggregate)), reinterpret_cast<void*>(&ExpAggregateHook),
                                reinterpret_cast<void**>(&OriginalExpAggregate));
}

bool CheckBlockade()
{
    return Code::Expect(At(BlockadeLoadingFinished), {0x40, 0x53, 0x48, 0x81, 0xEC, 0xB0, 0x00, 0x00, 0x00}, "Blockade OnLoadingFinished") &&
           ExpectBranch(At(0x56676F5), JumpRel32, BlockadeLoadingFinished, "Blockade, reflektierter Einsprung") &&
           Code::Expect(At(0x58AB9A0), {0x48, 0x8B, 0x09, 0x48, 0x8B, 0x01, 0x48, 0xFF, 0xA0, 0xE8, 0x04, 0x00, 0x00}, "verzögerter Blockadenstart");
}

bool ApplyBlockade(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(BlockadeLoadingFinished)), reinterpret_cast<void*>(&BlockadeLoadingFinishedHook),
                                reinterpret_cast<void**>(&OriginalBlockadeLoadingFinished));
}

bool CheckNetGuid()
{
    return Code::Expect(At(NetGuidExpiry), {0x0F, 0x28, 0xC6, 0xF2, 0x0F, 0x5C, 0xC1}, "Ablauf im NetGUID-Cache") &&
           Code::Expect(At(NetGuidExpiry, 7), {0x66, 0x0F, 0x2F, 0xC7}, "Vergleich im NetGUID-Cache");
}

// Der Server nutzt bestätigte statische Pfad-GUIDs über die Reisen hinweg weiter. Laufen sie beim Client ab, geht diese
// Identität verloren. Nur diese Pfadeinträge bleiben, bis der Cache der Verbindung endet; der Zeitstempel bleibt.
bool ApplyNetGuid(Code::Writer& Code)
{
    const uintptr_t Site = At(NetGuidExpiry);
    const auto Original = reinterpret_cast<uintptr_t>(Code.Here());
    Code.Emit({0x0F, 0x28, 0xC6, 0xF2, 0x0F, 0x5C, 0xC1}); // movaps xmm0,xmm6; subsd xmm0,xmm1
    Code.Emit({0xE9});
    Code.EmitRel(Site + 7);
    uint8_t* Guard = Code.Here();
    EmitNetGuidExpiryGuard(Code, Original, At(NetGuidNext));
    return !Code.Failed() && Code::WriteJump(Site, Guard);
}

bool CheckSneak()
{
    return Code::Expect(At(SneakSameTarget) - 3, {0x48, 0x3B, 0xC3, 0x0F, 0x84, 0xA1, 0x00, 0x00, 0x00}, "SneakApproach, gleiches Ziel") &&
           Code::Expect(At(0x58A64FE), {0x48, 0x83, 0xBF, 0xE8, 0x00, 0x00, 0x00, 0x00}, "SneakApproach, Prüfung der Abhängigkeiten");
}

// SneakApproach merkt sich die Figur, bevor ihr Mesh und PlayerState repliziert sind, und las danach das leere Mesh.
bool ApplySneak(Code::Writer& Code)
{
    uint8_t* Guard = Code.Here();
    EmitSneakTargetGuard(Code, At(SneakRetry), At(SneakReady));
    return !Code.Failed() && Code::WriteJump(At(SneakSameTarget), Guard);
}

bool CheckBusTakeOff()
{
    return Code::Expect(At(CanBusTakeOff), {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x20, 0x55, 0x57, 0x41, 0x56},
                        "ExtractionPointBase.CanBusTakeOff") &&
           Code::Expect(At(0x59C8AFF), {0xE8, 0xBC, 0xF4, 0xFF, 0xFF}, "CanBusTakeOff aus CheckTakeOffCondition") &&
           Code::Expect(At(FinishMission), {0x48, 0x89, 0x5C, 0x24, 0x20, 0x55, 0x56, 0x57, 0x48, 0x83, 0xEC, 0x30}, "DDMissionGameState.FinishMission");
}

bool ApplyBusTakeOff(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(CanBusTakeOff)), reinterpret_cast<void*>(&CanBusTakeOffHook),
                                reinterpret_cast<void**>(&OriginalCanBusTakeOff)) &&
           Core::Hooks::Install(reinterpret_cast<void*>(At(FinishMission)), reinterpret_cast<void*>(&FinishMissionHook),
                                reinterpret_cast<void**>(&OriginalFinishMission));
}

bool CheckRunLoss()
{
    return Code::Expect(At(TriggerEndRound), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55}, "DDMissionGameInstance.TriggerEndRound");
}

bool ApplyRunLoss(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(TriggerEndRound)), reinterpret_cast<void*>(&TriggerEndRoundHook),
                                reinterpret_cast<void**>(&OriginalTriggerEndRound));
}

bool CheckPowerUp()
{
    return Code::Expect(At(BeginDeferredActorSpawnFromClass), {0x40, 0x53, 0x55, 0x57, 0x41, 0x54}, "in BeginDeferredActorSpawnFromClass");
}

bool ApplyPowerUp(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(BeginDeferredActorSpawnFromClass)), reinterpret_cast<void*>(&BeginDeferredSpawnHook),
                                reinterpret_cast<void**>(&OriginalBeginDeferredSpawn));
}

// Je Patch die benutzten Adressen der Signaturtabelle, erzeugt von tools/patch_signatures.py sites.
constexpr uintptr_t ClientRefreshSites[] = {ClientRefreshCallSite, ClientRefreshFunction};
constexpr uintptr_t AttributeTickSites[] = {AttributeLookupFunction, AttributeLookupCallSite, AttributeTickExit};
constexpr uintptr_t AttributeLookupSites[] = {AttributeLookupFunction};
constexpr uintptr_t MapGenHostObjectSites[] = {MapGenHostObjectRead, MapGenHostObjectExit};
constexpr uintptr_t ComponentControllerSites[] = {ComponentControllerRead, ComponentControllerExit};
constexpr uintptr_t ThreatLevelSites[] = {ThreatFreezeBranch};
constexpr uintptr_t ExpSites[] = {FreeArray, 0x58A7260, 0x58B1D3E, ExpAggregate, 0x58B4D6B, GetPlayerCharacters};
constexpr uintptr_t BlockadeSites[] = {0x56676F5, 0x58AB9A0, BlockadeLoadingFinished};
constexpr uintptr_t NetGuidSites[] = {NetGuidExpiry, NetGuidNext};
constexpr uintptr_t SneakSites[] = {SneakSameTarget, SneakRetry, 0x58A64FE, SneakReady};
constexpr uintptr_t PowerUpSites[] = {BeginDeferredActorSpawnFromClass};
constexpr uintptr_t BusTakeOffSites[] = {FinishMission, CanBusTakeOff, 0x59C8AFF};
constexpr uintptr_t RunLossSites[] = {TriggerEndRound};
const Patch List[] = {
    {"Client-Refresh ohne Host-Objekt", &CheckClientRefresh, &ApplyClientRefresh, ClientRefreshSites},
    {"Attribut-Tick ohne PlayerState", &CheckAttributeTick, &ApplyAttributeTick, AttributeTickSites},
    {"AttributeSet-Suche ohne AbilitySystem", &CheckAttributeLookup, &ApplyAttributeLookup, AttributeLookupSites},
    {"Kartengenerator ohne Host-Objekt", &CheckMapGenHostObject, &ApplyMapGenHostObject, MapGenHostObjectSites},
    {"Figurenkomponente ohne Controller", &CheckComponentController, &ApplyComponentController, ComponentControllerSites},
    {"ThreatLevel ohne Freeze-Manager", &CheckThreatLevel, &ApplyThreatLevel, ThreatLevelSites},
    {"Exp-Aggregation wartet auf AttributeSets", &CheckExp, &ApplyExp, ExpSites},
    {"Blockade nur beim Host", &CheckBlockade, &ApplyBlockade, BlockadeSites},
    {"NetGUID-Pfade bleiben beim Client", &CheckNetGuid, &ApplyNetGuid, NetGuidSites},
    {"SneakApproach wartet auf Mesh und PlayerState", &CheckSneak, &ApplySneak, SneakSites},
    {"PowerUp ohne lokale Kopie", &CheckPowerUp, &ApplyPowerUp, PowerUpSites},
    {"Busabfahrt und Rundenende nur beim Host", &CheckBusTakeOff, &ApplyBusTakeOff, BusTakeOffSites},
    {"Niederlage entscheidet der Host", &CheckRunLoss, &ApplyRunLoss, RunLossSites},
};
}

std::span<const Patch> ClientPatches()
{
    return List;
}

bool FollowHostRoundEnd()
{
    const uintptr_t World = CurrentWorld();
    const uintptr_t GameState = World ? Memory::ReadPointer(World + Offsets::UWorld::GameState).value_or(0) : 0;
    if (!OriginalFinishMission || !GameState || !IsA(GameState, "DDMissionGameState"))
        return false;
    FollowingHost = true;
    OriginalFinishMission(GameState, false);
    FollowingHost = false;
    return true;
}

// Nativ (0x5936BE0): Bei Win und Lose setzt der RunPersistencyManager den Run-Container zurück und speichert den Run-Slot, bei
// NewDay schreibt er EndOfDay. Nur dieser Teil des Run-Endes läuft hier, nicht TriggerEndRound mit Wertung und Meta-Stand.
bool ClearClientRunSave(bool Won)
{
    const uintptr_t Manager = Objects::FindInstance("RunPersistencyManager");
    static const uintptr_t Function = Objects::FindObject("OnRoundChanged", "RunPersistencyManager");
    if (!Manager || !Function)
    {
        Core::Log::Warn("Run-Ende: Run-Stand des Clients nicht geleert, RunPersistencyManager fehlt");
        return false;
    }
    struct
    {
        uint8_t Reason;
    } Params{Won ? RoundChangeWin : RoundChangeLose};
    Objects::ProcessEvent(Manager, Function, &Params);
    Core::Log::Info("Run-Ende: eigener Run-Stand des Clients geleert ({}), der nächste Run beginnt neu", Won ? "Sieg" : "Niederlage");
    return true;
}

namespace
{
bool OwnedByGameInstance(uintptr_t Object)
{
    constexpr int MaxOuterDepth = 8;
    uintptr_t Outer = Memory::ReadPointer(Object + Offsets::UObject::Outer).value_or(0);
    for (int Depth = 0; Outer && Depth < MaxOuterDepth; ++Depth)
    {
        if (IsA(Outer, "DDMissionGameInstance"))
            return true;
        Outer = Memory::ReadPointer(Outer + Offsets::UObject::Outer).value_or(0);
    }
    return false;
}
}

namespace
{
constexpr std::string_view GamePackage = "/Script/DDSurvivors";
constexpr Reflection::FieldSpec StandardHeading{GamePackage, "RoundEndPanel", "StandardHeadingContainer", "ObjectProperty", 8};
constexpr Reflection::FieldSpec FinishedHeading{GamePackage, "RoundEndPanel", "RunFinishedHeading", "ObjectProperty", 8};
constexpr uint8_t SlateCollapsed = 1, SlateSelfHitTestInvisible = 4; // ESlateVisibility

struct
{
    int32_t Index = -1;
    uintptr_t Object = 0;
} LossPanel; // nur Game Thread

void SetWidgetVisibility(uintptr_t Panel, const Reflection::FieldSpec& Spec, uint8_t Visibility)
{
    static const uintptr_t Function = Reflection::Function("/Script/UMG", "Widget", "SetVisibility");
    const auto Field = Reflection::Property(Spec);
    const uintptr_t Widget = Field ? Memory::ReadPointer(Panel + Field->Offset).value_or(0) : 0;
    if (!Function || !Widget)
        return;
    struct
    {
        uint8_t Visibility;
    } Params{Visibility};
    Objects::ProcessEvent(Widget, Function, &Params);
}

void HideLossPanelButtons(uintptr_t Panel)
{
    static const uintptr_t SetButtons = Objects::FindObject("SetButtonsVisibility", "RoundEndPanel");
    if (!SetButtons)
        return;
    struct
    {
        bool Visible;
    } Params{false};
    Objects::ProcessEvent(Panel, SetButtons, &Params);
}
}

void KeepRunLossButtonsHidden()
{
    if (LossPanel.Object && Objects::At(LossPanel.Index) == LossPanel.Object)
        HideLossPanelButtons(LossPanel.Object);
}

bool ShowRunEndPanel(bool Won)
{
    static const uintptr_t Refresh = Objects::FindObject("RefreshAsGameOver", "RoundEndPanel");
    static const uintptr_t NegativeText = Objects::FindObject("SetNegativeText", "RoundEndPanel");
    static const uintptr_t PositiveText = Objects::FindObject("SetPositiveText", "RoundEndPanel");
    static const uintptr_t FinishedAnimation = Objects::FindObject("PlayRunFinishedAnimation", "WP_Round_Stats_C");
    if (!Refresh || !NegativeText || !PositiveText)
        return false;
    LossPanel = {};
    int Panels = 0;
    const int32_t Count = Objects::Count();
    for (int32_t Index = 0; Index < Count; ++Index)
    {
        const uintptr_t Object = Objects::At(Index);
        if (!Object || !IsA(Object, "WP_Round_Stats_C"))
            continue;
        if (Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(Offsets::UObject::ClassDefaultObjectFlag) &
            (Offsets::UObject::ClassDefaultObjectFlag | RfBeginDestroyed | RfFinishDestroyed))
            continue;
        // Die offene Übersicht hängt, teils über das WI_BusScreen, an der GameInstance. Die Vorlage im Paket hat keinen Besitzer und stürzt ab.
        if (!OwnedByGameInstance(Object))
            continue;
        // Beim Sieg ohne RefreshAsGameOver: Der Knopf Fortsetzen muss für den Endlosmodus unverändert bleiben.
        if (!Won)
            Objects::ProcessEvent(Object, Refresh, nullptr);
        Objects::ProcessEvent(Object, Won ? PositiveText : NegativeText, nullptr);
        SetWidgetVisibility(Object, StandardHeading, SlateCollapsed);
        SetWidgetVisibility(Object, FinishedHeading, SlateSelfHitTestInvisible);
        if (FinishedAnimation)
        {
            struct
            {
                double FirstDelayTimeScale;
            } Params{1.0};
            Objects::ProcessEvent(Object, FinishedAnimation, &Params);
        }
        if (!Won)
        {
            HideLossPanelButtons(Object);
            LossPanel.Index = Index;
            LossPanel.Object = Object;
        }
        ++Panels;
    }
    return Panels > 0;
}
}
