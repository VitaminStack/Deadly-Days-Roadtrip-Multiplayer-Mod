#include <doctest.h>

#include <windows.h>

#include <cmath>
#include <cstring>

#include "Game/CodeCave.h"
#include "Game/MapPolicy.h"
#include "Game/MovementPolicy.h"
#include "Game/PatchPolicy.h"
#include "Game/Patches.h"
#include "Game/RemoteP2Policy.h"

using namespace Game;
using namespace Game::Patches;

TEST_CASE("Itemsack: Vergabe nur durch Authority ausserhalb einer Client-Welt")
{
    CHECK(ItemLootMayGrant(3, NetRole::Standalone));
    CHECK(ItemLootMayGrant(3, NetRole::Host));
    for (uint8_t Role = 0; Role <= 3; ++Role)
        CHECK_FALSE(ItemLootMayGrant(Role, NetRole::Client));
    for (uint8_t Role = 0; Role < 3; ++Role)
    {
        CHECK_FALSE(ItemLootMayGrant(Role, NetRole::Host));
        CHECK_FALSE(ItemLootMayGrant(Role, NetRole::Standalone));
    }
}

namespace
{
// Ausführbarer Puffer für erzeugten Code. Es läuft nur eigener Testcode, nie Code aus dem Spiel.
struct ExecutableBuffer
{
    uint8_t* Data = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    ~ExecutableBuffer() { VirtualFree(Data, 0, MEM_RELEASE); }
    void Flush() const { FlushInstructionCache(GetCurrentProcess(), Data, 4096); }
};

int Applied = 0;
bool PassCheck() { return true; }
bool FailCheck() { return false; }
bool CountApply(Code::Writer&)
{
    ++Applied;
    return true;
}
bool FailApply(Code::Writer&) { return false; }

void RemoveSparse(ControllerSet* Set, int32_t Index, int32_t Count)
{
    REQUIRE(Count == 1);
    Set->InlineBits[Index / 32] &= ~(1u << (Index % 32));
    ++Set->NumFree;
}
}

TEST_CASE("Code: rel32 rechnet vom nächsten Befehl und lehnt zu weite Ziele ab")
{
    CHECK(Code::Rel32(0x1005, 0x1000) == -5);
    CHECK(Code::Rel32(0x1000, 0x1010) == 0x10);
    CHECK_FALSE(Code::Rel32(0x1000, 0x1000 + 0x80000000ull).has_value());
    CHECK_FALSE(Code::Rel32(0x100000000ull, 0x1000).has_value());
}

TEST_CASE("Code: Writer schreibt Befehle, Sprungweiten und absolute Sprünge")
{
    uint8_t Buffer[32]{};
    Code::Writer Code(Buffer, sizeof(Buffer));
    Code.Emit({0x90, 0x90});
    Code.EmitRel(reinterpret_cast<uintptr_t>(Buffer));
    int32_t Displacement = 0;
    std::memcpy(&Displacement, Buffer + 2, 4);
    CHECK(Displacement == -6);
    uint8_t* Jump = Code.EmitAbsoluteJump(0x1122334455667788ull);
    REQUIRE(Jump == Buffer + 6);
    CHECK(Jump[0] == 0x48);
    CHECK(Jump[1] == 0xB8);
    uint64_t Target = 0;
    std::memcpy(&Target, Jump + 2, 8);
    CHECK(Target == 0x1122334455667788ull);
    CHECK(Jump[10] == 0xFF);
    CHECK(Jump[11] == 0xE0);
    CHECK_FALSE(Code.Failed());
}

TEST_CASE("Code: voller Puffer und unerreichbares Ziel setzen Failed, danach wird nichts mehr geschrieben")
{
    uint8_t Buffer[4]{};
    Code::Writer Full(Buffer, sizeof(Buffer));
    Full.Emit({1, 2, 3});
    Full.Emit({4, 5});
    CHECK(Full.Failed());
    CHECK(Buffer[3] == 0);
    CHECK(Full.EmitAbsoluteJump(0) == nullptr);

    uint8_t Other[8]{};
    Code::Writer Far(Other, sizeof(Other));
    Far.EmitRel(reinterpret_cast<uintptr_t>(Other) + 0x100000000ull);
    CHECK(Far.Failed());
    CHECK(Code::Writer(nullptr, 16).Failed());
}

TEST_CASE("Code: Expect vergleicht Bytes und behandelt unlesbaren Speicher als Abweichung")
{
    const uint8_t Bytes[] = {0x48, 0x89, 0x5C, 0x24, 0x18};
    CHECK(Code::Expect(reinterpret_cast<uintptr_t>(Bytes), {0x48, 0x89, 0x5C, 0x24, 0x18}, "Test"));
    CHECK_FALSE(Code::Expect(reinterpret_cast<uintptr_t>(Bytes), {0x48, 0x89, 0x5C, 0x24, 0x10}, "Test"));
    CHECK_FALSE(Code::Expect(0, {0x48}, "Test"));
    const uintptr_t Entry = reinterpret_cast<uintptr_t>(&Bytes);
    CHECK(Code::ExpectVirtual(reinterpret_cast<uintptr_t>(&Entry), reinterpret_cast<uintptr_t>(&Bytes), "Test"));
    CHECK_FALSE(Code::ExpectVirtual(reinterpret_cast<uintptr_t>(&Entry), 0, "Test"));
}

TEST_CASE("Code: WriteJump springt in eigenen Code, WriteVirtual tauscht einen Eintrag")
{
    ExecutableBuffer Memory;
    REQUIRE(Memory.Data);
    // Ziel: mov eax,7; ret. Stelle: zunächst mov eax,1; ret, danach ein Sprung zum Ziel.
    const uint8_t Target[] = {0xB8, 0x07, 0x00, 0x00, 0x00, 0xC3};
    const uint8_t Site[] = {0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3};
    std::memcpy(Memory.Data + 64, Target, sizeof(Target));
    std::memcpy(Memory.Data, Site, sizeof(Site));
    Memory.Flush();
    const auto Call = reinterpret_cast<int (*)()>(Memory.Data);
    CHECK(Call() == 1);
    REQUIRE(Code::WriteJump(reinterpret_cast<uintptr_t>(Memory.Data), Memory.Data + 64));
    CHECK(Call() == 7);

    uintptr_t Table[2] = {1, 2};
    uintptr_t Old = 0;
    REQUIRE(Code::WriteVirtual(reinterpret_cast<uintptr_t>(&Table[1]), 9, &Old));
    CHECK(Old == 2);
    CHECK(Table[1] == 9);
}

TEST_CASE("Patches: erst alle prüfen, dann setzen; eine gescheiterte Prüfung ändert nichts")
{
    uint8_t Buffer[16]{};
    Code::Writer Code(Buffer, sizeof(Buffer));
    Applied = 0;
    const Patch Good[] = {{"a", &PassCheck, &CountApply}, {"b", &PassCheck, &CountApply}};
    CHECK(InstallAll(Good, Code) == InstallResult::Installed);
    CHECK(Applied == 2);

    Applied = 0;
    const Patch OneBad[] = {{"a", &PassCheck, &CountApply}, {"b", &FailCheck, &CountApply}, {"c", &PassCheck, &CountApply}};
    CHECK(InstallAll(OneBad, Code) == InstallResult::CheckFailed);
    CHECK(Applied == 0);

    const Patch NoCheck[] = {{"a", nullptr, &CountApply}};
    CHECK(InstallAll(NoCheck, Code) == InstallResult::CheckFailed);

    Applied = 0;
    const Patch BreaksLate[] = {{"a", &PassCheck, &CountApply}, {"b", &PassCheck, &FailApply}, {"c", &PassCheck, &CountApply}};
    CHECK(InstallAll(BreaksLate, Code) == InstallResult::ApplyFailed);
    CHECK(Applied == 1);
}

TEST_CASE("Patches: ein voller Code-Puffer gilt als gescheitertes Setzen")
{
    uint8_t Buffer[2]{};
    Code::Writer Code(Buffer, sizeof(Buffer));
    Code.Emit({1, 2, 3});
    Applied = 0;
    const Patch List[] = {{"a", &PassCheck, &CountApply}};
    CHECK(InstallAll(List, Code) == InstallResult::ApplyFailed);
}

TEST_CASE("Patches: SpawnedPlayers-Menge mit Datenmüll, Null-Schlüssel, Kette und Duplikat")
{
    ControllerSetEntry Entries[] = {{0x1234, 1, 0}, {0, -1, 0}, {0x1234, -1, 1}, {~uintptr_t(0), 99, 99}};
    ControllerSet Set{};
    Set.Data = Entries;
    Set.Num = Set.Max = Set.NumBits = 4;
    Set.MaxBits = 128;
    Set.InlineBits[0] = 7; // Der freie Platz enthält Müll, keinen Controller.
    Set.NumFree = 1;
    Set.HashSize = 2;
    Set.InlineHash[0] = 0;
    Set.InlineHash[1] = 2;
    CHECK(Set.Valid());
    CHECK(Set.Contains(0x1234));
    CHECK_FALSE(Set.Contains(~uintptr_t(0)));
    CHECK_FALSE(Set.Remove(3, RemoveSparse));
    CHECK(Set.Remove(1, RemoveSparse)); // Null-Schlüssel über den gespeicherten Bucket, Ende der Kette.
    CHECK((Entries[0].HashNext == -1 && Set.Valid()));
    CHECK(Set.Remove(0, RemoveSparse)); // Kopf, das Duplikat im anderen Bucket bleibt.
    CHECK((Set.InlineHash[0] == -1 && Set.Contains(0x1234) && Set.Valid()));
    Entries[2].HashNext = 2;
    CHECK_FALSE(Set.Valid()); // Zyklus
    Entries[2].HashNext = 99;
    CHECK_FALSE(Set.Valid()); // ungültiger Index
    Entries[2].HashNext = -1;
    Entries[2].HashIndex = 0;
    CHECK_FALSE(Set.Valid()); // falscher Bucket
    Entries[2].HashIndex = 1;
    CHECK(Set.Remove(2, RemoveSparse));
    CHECK((Set.Valid() && !Set.Contains(0x1234)));
    Set.NumBits = 3;
    CHECK_FALSE(Set.Valid());
}

TEST_CASE("Patches: Exp-Aggregation wartet, bis jede Figur ihr AttributeSet hat")
{
    const uintptr_t Characters[] = {1, 0, 3};
    const auto At = [&](int32_t Index) { return Characters[Index]; };
    CHECK(AttributesComplete(3, 3, At, [](uintptr_t) { return true; }));
    CHECK_FALSE(AttributesComplete(3, 3, At, [](uintptr_t Character) { return Character != 3; }));
    CHECK(AttributesComplete(0, 0, At, [](uintptr_t) { return false; }));
    CHECK_FALSE(AttributesComplete(-1, 0, At, [](uintptr_t) { return true; }));
    CHECK_FALSE(AttributesComplete(3, 2, At, [](uintptr_t) { return true; }));
    CHECK(PendingAttributeValue(12.5f) == 12.5f);
    CHECK(PendingAttributeValue(-1.0f) == 0.0f);
    CHECK(PendingAttributeValue(NAN) == 0.0f);
}

TEST_CASE("Patches: Blockaden startet nur, wer keine Verbindung zu einem Server hat")
{
    for (int World = 0; World < 2; ++World)
        for (int Driver = 0; Driver < 2; ++Driver)
            for (int Connection = 0; Connection < 2; ++Connection)
                CHECK(BlockadeMayStart(World != 0, Driver != 0, Connection != 0) == (World != 0 && (Driver == 0 || Connection == 0)));
}

TEST_CASE("Patches: NetGUID-Wächter im erzeugten Code entspricht der Regel")
{
    ExecutableBuffer Memory;
    REQUIRE(Memory.Data);
    Code::Writer Code(Memory.Data, 4096);
    // Original: eax=0, Next: eax=1, beide räumen den Rahmen des Testaufrufers ab.
    const auto Original = reinterpret_cast<uintptr_t>(Code.Here());
    Code.Emit({0xB8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x70, 0x5F, 0xC3});
    const auto Next = reinterpret_cast<uintptr_t>(Code.Here());
    Code.Emit({0xB8, 0x01, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x70, 0x5F, 0xC3});
    using GuardFn = int(__fastcall*)(const void*, const void*);
    const auto Guard = reinterpret_cast<GuardFn>(Code.Here());
    // push rdi; sub rsp,70h; mov rdi,rcx; mov [rsp+68h],rdx
    Code.Emit({0x57, 0x48, 0x83, 0xEC, 0x70, 0x48, 0x89, 0xCF, 0x48, 0x89, 0x54, 0x24, 0x68});
    EmitNetGuidExpiryGuard(Code, Original, Next);
    REQUIRE_FALSE(Code.Failed());
    Memory.Flush();

    alignas(8) uint8_t Driver[0x200]{}, Cache[0x200]{}, Entry[0x40]{};
    *reinterpret_cast<uintptr_t*>(Cache + 0x150) = reinterpret_cast<uintptr_t>(Driver);
    *reinterpret_cast<uintptr_t*>(Driver + 0x100) = 1;
    *reinterpret_cast<uintptr_t*>(Driver + 0x1C8) = reinterpret_cast<uintptr_t>(Cache);
    *reinterpret_cast<uint32_t*>(Entry + 0x18) = 42;
    bool AllMatch = true;
    for (uint64_t Guid = 0; Guid < 64; ++Guid)
        for (unsigned Flags = 0; Flags < 256; ++Flags)
        {
            *reinterpret_cast<uint64_t*>(Entry) = Guid;
            Entry[0x30] = static_cast<uint8_t>(Flags);
            const bool Expected = RetainNetGuidPath(true, Guid, 42, static_cast<uint8_t>(Flags));
            AllMatch = AllMatch && Guard(Entry + 8, Cache + 8) == static_cast<int>(Expected) && Guard(Entry + 8, Cache + 16) == 0;
        }
    CHECK(AllMatch);
    *reinterpret_cast<uint64_t*>(Entry) = 3;
    Entry[0x30] = 0;
    CHECK(Guard(Entry + 8, Cache + 8) == 1);
    *reinterpret_cast<uintptr_t*>(Driver + 0x100) = 0;
    CHECK(Guard(Entry + 8, Cache + 8) == 0); // Host
    *reinterpret_cast<uintptr_t*>(Driver + 0x100) = 1;
    *reinterpret_cast<uintptr_t*>(Driver + 0x1C8) = 0;
    CHECK(Guard(Entry + 8, Cache + 8) == 0); // Cache gehört zu keinem Treiber mehr
    CHECK(Guard(Entry + 8, nullptr) == 0);
}

TEST_CASE("Patches: Sneak-Wächter im erzeugten Code entspricht der Regel")
{
    ExecutableBuffer Memory;
    REQUIRE(Memory.Data);
    Code::Writer Code(Memory.Data, 4096);
    const auto Retry = reinterpret_cast<uintptr_t>(Code.Here());
    Code.Emit({0xB8, 0x00, 0x00, 0x00, 0x00, 0x5F, 0xC3});
    const auto Ready = reinterpret_cast<uintptr_t>(Code.Here());
    Code.Emit({0xB8, 0x01, 0x00, 0x00, 0x00, 0x5F, 0xC3});
    using GuardFn = int(__fastcall*)(const void*, int);
    const auto Guard = reinterpret_cast<GuardFn>(Code.Here());
    Code.Emit({0x57, 0x48, 0x89, 0xCF, 0x83, 0xFA, 0x01}); // rdi = Komponente, ZF = gleiches Ziel
    EmitSneakTargetGuard(Code, Retry, Ready);
    REQUIRE_FALSE(Code.Failed());
    Memory.Flush();
    alignas(8) uint8_t Component[0x138]{};
    for (int Same = 0; Same < 2; ++Same)
        for (int Mesh = 0; Mesh < 2; ++Mesh)
            for (int State = 0; State < 2; ++State)
            {
                *reinterpret_cast<uintptr_t*>(Component + 0xE8) = static_cast<uintptr_t>(Mesh);
                *reinterpret_cast<uintptr_t*>(Component + 0xD8) = static_cast<uintptr_t>(State);
                CHECK(Guard(Component, Same) == static_cast<int>(ReuseSneakTarget(Same != 0, Mesh != 0, State != 0)));
            }
}

TEST_CASE("Patches: nur der Host leitet die Reise um, nie ins Hauptmenü oder bei einem Listen-Server")
{
    CHECK(ShouldRedirectTravel(NetRole::Host, "/Game/Maps/DDDynamic_Map"));
    CHECK(ShouldRedirectTravel(NetRole::Host, "MetaBase"));
    CHECK_FALSE(ShouldRedirectTravel(NetRole::Host, "/Game/Maps/MainMenu"));
    CHECK_FALSE(ShouldRedirectTravel(NetRole::Host, "/Game/Maps/MetaBase?listen"));
    CHECK_FALSE(ShouldRedirectTravel(NetRole::Host, ""));
    CHECK_FALSE(ShouldRedirectTravel(NetRole::Client, "/Game/Maps/DDDynamic_Map"));
    CHECK_FALSE(ShouldRedirectTravel(NetRole::Standalone, "/Game/Maps/DDDynamic_Map"));
}

TEST_CASE("Patches: Busplatz ergänzt die geschonte Liste um fehlende Spielerfiguren")
{
    const uintptr_t Keep[] = {10, 20};
    const uintptr_t Pawns[] = {20, 0, 30, 30};
    uintptr_t Out[8]{};
    size_t Added = 0;
    CHECK(MergeKeepList(Keep, 2, Pawns, 4, Out, 8, Added) == 3);
    CHECK(Added == 1);
    CHECK((Out[0] == 10 && Out[1] == 20 && Out[2] == 30));
    // Zu wenig Platz: die Liste wird nie überschrieben, nur nicht mehr ergänzt.
    uintptr_t Small[2]{};
    CHECK(MergeKeepList(Keep, 2, Pawns, 4, Small, 2, Added) == 2);
    CHECK(Added == 0);
}

TEST_CASE("Patches: Beobachter nach ProcessEvent werden je einmal angemeldet")
{
    const EventObserver First = [](uintptr_t, uintptr_t, void*) {};
    CHECK(AddEventObserver(First));
    CHECK(AddEventObserver(First));
    CHECK_FALSE(AddEventObserver(nullptr));
}

namespace
{
void RemoveActivePlayer(ActivePlayersMap* Map, int32_t Index, int32_t Count)
{
    REQUIRE(Count == 1);
    ++Map->NumFree;
    Map->InlineBits[0] &= ~(1u << Index);
    Map->FirstFree = Index;
}

bool __fastcall RemotePresent(uintptr_t Value)
{
    return Value == 1;
}
}

TEST_CASE("RemoteP2: ActivePlayers mit freiem Platz, falschen Ketten und Entfernen")
{
    ActivePlayerEntry Entries[] = {{0, {10, 20}, -1, 0}, {1, {11, 21}, 0, 0}, {99, {}, -1, 0}};
    ActivePlayersMap Map{};
    Map.Data = Entries;
    Map.Num = Map.Max = Map.NumBits = Map.MaxBits = 3;
    Map.NumFree = 1;
    Map.InlineBits[0] = 3;
    Map.HashSize = 1;
    Map.InlineHash[0] = 1;
    CHECK(Map.Valid()); // freier Platz mit fremden Bytes ist kein Spieler
    CHECK(Map.Find(1) == 1);
    CHECK(Map.Find(2) == -1);
    Entries[1].Key = 0;
    CHECK_FALSE(Map.Valid()); // doppelter Schlüssel
    Entries[1].Key = 1;
    Entries[0].HashNext = 1;
    CHECK_FALSE(Map.Valid()); // Kette mit Schleife
    Entries[0].HashNext = -1;
    Entries[1].HashIndex = 1;
    CHECK_FALSE(Map.Valid());
    Entries[1].HashIndex = 0;
    Map.InlineHash[0] = 2;
    CHECK_FALSE(Map.Valid()); // Kette zeigt auf freien Platz
    Map.InlineHash[0] = 1;
    Map.NumFree = 0;
    CHECK_FALSE(Map.Valid());
    Map.NumFree = 1;
    REQUIRE(Map.Valid());
    CHECK(Map.Remove(1, RemoveActivePlayer));
    CHECK((Map.Valid() && Map.InlineHash[0] == 0 && Map.NumFree == 2));
    CHECK_FALSE(Map.Remove(1, RemoveActivePlayer));
    CHECK(Map.Remove(0, RemoveActivePlayer));
    CHECK((Map.Valid() && Map.InlineHash[0] == -1 && Map.NumFree == 3));
    CHECK(ActivePlayersMap{}.Valid());
}

TEST_CASE("RemoteP2: Beitrittsmeldung nur einmal je Welt und Controller")
{
    CHECK(SameRemoteNotification(100, {10, 20}, 100, {10, 20}));
    CHECK_FALSE(SameRemoteNotification(101, {10, 20}, 100, {10, 20}));
    CHECK_FALSE(SameRemoteNotification(100, {11, 20}, 100, {10, 20}));
    CHECK_FALSE(SameRemoteNotification(100, {10, 21}, 100, {10, 20}));
}

TEST_CASE("RemoteP2: Geräte-Wächter im erzeugten Code nimmt nur die lokale P2-Pflicht aus")
{
    ExecutableBuffer Memory;
    REQUIRE(Memory.Data);
    uint8_t* const Solo = Memory.Data + 0x200;
    uint8_t* const Reconnect = Memory.Data + 0x300;
    Code::Writer Code(Memory.Data, 0x200);
    // Nachbau der echten Stelle: RDI Subsystem (hier: Remote-P2 vorhanden), RBX lokales P2-Gerät, BPL Solo-Verlust,
    // R9B P1-Gerät. Nichtflüchtige Register sichern, Stack wie im Spiel ausgerichtet.
    using ProbeFn = bool(__fastcall*)(uintptr_t, uintptr_t, bool, bool);
    const auto Probe = reinterpret_cast<ProbeFn>(Code.Here());
    Code.Emit({0x53, 0x57, 0x55, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xF9, 0x48, 0x8B, 0xDA, 0x41, 0x8A, 0xE8});
    Code.Emit({0x45, 0x84, 0xC9, 0x0F, 0x84}); // native P1-Geräteprüfung davor
    Code.EmitRel(reinterpret_cast<uintptr_t>(Reconnect));
    EmitRemoteDeviceGuard(Code, reinterpret_cast<uintptr_t>(&RemotePresent), reinterpret_cast<uintptr_t>(Solo),
                          reinterpret_cast<uintptr_t>(Reconnect));
    REQUIRE_FALSE(Code.Failed());
    Code::Writer Tail(Solo, 0x200);
    Tail.Emit({0x40, 0x84, 0xED, 0x0F, 0x85}); // native Solo-Verlustprüfung danach
    Tail.EmitRel(reinterpret_cast<uintptr_t>(Reconnect));
    Tail.Emit({0x31, 0xC0, 0x48, 0x83, 0xC4, 0x20, 0x5D, 0x5F, 0x5B, 0xC3});
    Code::Writer Need(Reconnect, 0x100);
    Need.Emit({0xB8, 1, 0, 0, 0, 0x48, 0x83, 0xC4, 0x20, 0x5D, 0x5F, 0x5B, 0xC3});
    REQUIRE_FALSE(Tail.Failed());
    REQUIRE_FALSE(Need.Failed());
    Memory.Flush();
    bool AllMatch = true;
    for (int Remote = 0; Remote < 2; ++Remote)
        for (int LocalP2 = 0; LocalP2 < 2; ++LocalP2)
            for (int SoloLoss = 0; SoloLoss < 2; ++SoloLoss)
                for (int P1 = 0; P1 < 2; ++P1)
                    AllMatch = AllMatch && Probe(Remote, LocalP2, SoloLoss != 0, P1 != 0) == (!P1 || (!Remote && !LocalP2) || SoloLoss);
    CHECK(AllMatch);
}

TEST_CASE("Patches: Restrictor-Wächter bremst ohne GameState nicht und stürzt nicht ab")
{
    ExecutableBuffer Memory;
    REQUIRE(Memory.Data);
    Code::Writer Code(Memory.Data, 4096);
    const auto Skip = reinterpret_cast<uintptr_t>(Code.Here());
    Code.Emit({0x31, 0xC0, 0xC3}); // xor eax,eax; ret
    const auto Continue = reinterpret_cast<uintptr_t>(Code.Here());
    Code.Emit({0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3}); // mov eax,1; ret
    using GuardFn = int(__fastcall*)(const void*);
    const auto Guard = reinterpret_cast<GuardFn>(Code.Here());
    Code.Emit({0x48, 0x89, 0xC8}); // mov rax,rcx (an der echten Stelle steht MissionGameState in RAX)
    EmitRestrictorGuard(Code, Skip, Continue);
    REQUIRE_FALSE(Code.Failed());
    Memory.Flush();

    alignas(8) uint8_t GameState[0x300]{};
    CHECK(Guard(nullptr) == 0); // früher Absturz beim Lesen von 0x2C8
    for (int32_t Players : {-1, 0, 1, 2, 3})
    {
        *reinterpret_cast<int32_t*>(GameState + 0x2C8) = Players;
        CHECK(Guard(GameState) == (Players > 1 ? 1 : 0));
    }
}

TEST_CASE("Bewegung: Dodge-Werte und Figur ohne Bewegungsmodus erkennen")
{
    CHECK(DashSettingsLeft(0.f, 20.f));
    CHECK_FALSE(DashSettingsLeft(8.f, 1.f));
    CHECK_FALSE(DashSettingsLeft(0.f, 1.f));  // andere Ursache, etwa Eis, nicht anfassen
    CHECK_FALSE(DashSettingsLeft(8.f, 20.f));
    CHECK(ProxyMovementStopped(0, 1));       // None, Host meldet Walking
    CHECK(ProxyMovementStopped(0, 0x23));    // Custom-Modus 2 im oberen Nibble, MovementMode 3
    CHECK_FALSE(ProxyMovementStopped(1, 1));
    CHECK_FALSE(ProxyMovementStopped(0, 0)); // Host meldet selbst None
    CHECK(PackedMovementMode(0x23) == 3);
    CHECK(PackedCustomMode(0x23) == 2);
}

TEST_CASE("Bewegung: erst ein Zustand am Stück über der Grenze löst aus")
{
    StuckTimer Timer;
    CHECK_FALSE(Timer.Update(true, 1000, 750));
    CHECK_FALSE(Timer.Update(true, 1700, 750));
    CHECK(Timer.Update(true, 1750, 750));
    CHECK_FALSE(Timer.Update(false, 1760, 750)); // laufender Dodge endete normal
    CHECK_FALSE(Timer.Update(true, 1800, 750));
    CHECK_FALSE(Timer.Update(true, 2549, 750));
    CHECK(Timer.Update(true, 2550, 750));
    StuckTimer AtZero;
    CHECK_FALSE(AtZero.Update(true, 0, 10)); // Zeit 0 gilt als gestartet, nicht als „nie“
    CHECK_FALSE(AtZero.Update(true, 9, 10));
    CHECK(AtZero.Update(true, 10, 10));
}

TEST_CASE("Bewegung: fremde Figur gilt erst ab 3 m neben dem Host-Ort als abgehängt")
{
    CHECK_FALSE(ProxyDrifted(0, 0));
    CHECK_FALSE(ProxyDrifted(200, 200)); // 2,8 m, etwa Glättung während eines Dodges
    CHECK(ProxyDrifted(250, 4555 - 4507 + 200));
    CHECK(ProxyDrifted(-1722, 1545)); // gemessen: Kopie bei (1837,4430), Host bei (116,5975)
}

TEST_CASE("Bewegung: unbekannte Unterlage und weiches Folgen")
{
    int Dummy = 0;
    const auto Some = reinterpret_cast<uintptr_t>(&Dummy);
    CHECK(BaseUnresolved(0x01, 0));
    CHECK(BaseUnresolved(0x07, 0));
    CHECK_FALSE(BaseUnresolved(0x01, Some)); // Unterlage beim Client bekannt
    CHECK_FALSE(BaseUnresolved(0x00, 0));    // Host steht auf nichts Beweglichem
    CHECK(FollowShare(0) == doctest::Approx(0.05));
    CHECK(FollowShare(16) == doctest::Approx(16.0 / 40.0));
    CHECK(ExtrapolationSeconds(0) == doctest::Approx(0.0));
    CHECK(ExtrapolationSeconds(80) == doctest::Approx(0.08));
    CHECK(ExtrapolationSeconds(5000) == doctest::Approx(0.15));
    CHECK(FollowShare(66) == doctest::Approx(1.0));
    CHECK(FollowShare(500) == doctest::Approx(1.0));
}

TEST_CASE("Karte: Client wartet auf die Werte des Hosts, aber nicht ewig")
{
    CHECK(ClientMapStart(false, false, 0, 0) == MapStart::Wait);
    CHECK(ClientMapStart(false, false, HostMapWaitMs - 1, 0) == MapStart::Wait);
    CHECK(ClientMapStart(false, false, HostMapWaitMs, 0) == MapStart::Own);
    CHECK(ClientMapStart(true, true, 0, 0) == MapStart::HostValues);
    CHECK(ClientMapStart(true, false, 0, 0) == MapStart::Wait);
    CHECK(ClientMapStart(true, false, HostMapWaitMs * 2, HostGraphWaitMs - 1) == MapStart::Wait);
    CHECK(ClientMapStart(true, false, 0, HostGraphWaitMs) == MapStart::HostValues);
}

TEST_CASE("Itemsack zählt beim Host für alle Spieler")
{
    CHECK(ShareItemLoot(NetRole::Host, 2, 1));
    CHECK_FALSE(ShareItemLoot(NetRole::Host, 1, 1));
    CHECK_FALSE(ShareItemLoot(NetRole::Host, 2, 0));
    CHECK_FALSE(ShareItemLoot(NetRole::Host, 2, 2));
    CHECK_FALSE(ShareItemLoot(NetRole::Client, 2, 1));
    CHECK_FALSE(ShareItemLoot(NetRole::Standalone, 2, 1));
}
