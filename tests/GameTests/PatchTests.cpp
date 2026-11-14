#include <doctest.h>
#include <limits>

#include <windows.h>

#include <cmath>
#include <cstring>

#include "Game/CodeCave.h"
#include "Game/LocalDevicePolicy.h"
#include "Game/MapPolicy.h"
#include "Game/MovementPolicy.h"
#include "Game/PatchPolicy.h"
#include "Game/PatchSignatures.h"
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

TEST_CASE("Schrott: Haufen bucht nur Host oder Einzelspieler")
{
    CHECK(ScrapLootMayCredit(NetRole::Standalone));
    CHECK(ScrapLootMayCredit(NetRole::Host));
    CHECK_FALSE(ScrapLootMayCredit(NetRole::Client));
}

TEST_CASE("Loot: Aufsammelklang wird nur beim Client je Objekt gebremst")
{
    CHECK(LootSoundOncePerObject(NetRole::Client));
    CHECK_FALSE(LootSoundOncePerObject(NetRole::Host));
    CHECK_FALSE(LootSoundOncePerObject(NetRole::Standalone));
}

TEST_CASE("Schrott: Client spielt das Aufsammeln je Haufen nur einmal")
{
    LootedPiles Piles;
    uint64_t Now = 10'000;
    CHECK(Piles.FirstLoot(0x1000, Now));
    for (int Tick = 0; Tick < 600; ++Tick) // zehn Sekunden jeden Takt erneut: bleibt stumm
        CHECK_FALSE(Piles.FirstLoot(0x1000, Now += 16));
    CHECK(Piles.FirstLoot(0x2000, Now)); // anderer Haufen
    CHECK_FALSE(Piles.FirstLoot(0x1000, Now + LootedPiles::QuietMs - 1));
    // Nach Ruhe aus dem Pool wiederverwendet: klingt wieder
    Now += 2 * LootedPiles::QuietMs;
    CHECK(Piles.FirstLoot(0x1000, Now));
    CHECK_FALSE(Piles.FirstLoot(0x1000, Now + 16));
    CHECK(Piles.Size() == 2);
    Piles.Clear();
    CHECK(Piles.FirstLoot(0x1000, Now + 32));
}

TEST_CASE("Schrott: ein dauernd berührter Haufen fällt nicht aus der Liste")
{
    LootedPiles Piles;
    uint64_t Now = 0;
    CHECK(Piles.FirstLoot(0x1, Now));
    for (uintptr_t Other = 2; Other < 2 + 3 * LootedPiles::Capacity; ++Other)
    {
        CHECK(Piles.FirstLoot(Other, ++Now));
        CHECK_FALSE(Piles.FirstLoot(0x1, ++Now));
    }
    CHECK(Piles.Size() == LootedPiles::Capacity);
    CHECK(Piles.FirstLoot(0x2, ++Now)); // ältester herausgefallen, zählt wieder neu
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

namespace
{
int Checked = 0;
bool CountCheck()
{
    ++Checked;
    return true;
}
bool OnlyFirstUsable(uintptr_t Site) { return Site == 0x100; }
}

TEST_CASE("Patches: ein Patch mit gesperrter Stelle fällt allein aus, ohne Check und Apply")
{
    uint8_t Buffer[16]{};
    Code::Writer Code(Buffer, sizeof(Buffer));
    constexpr uintptr_t Good[] = {0x100};
    constexpr uintptr_t Mixed[] = {0x100, 0x200};
    const Patch List[] = {{"a", &CountCheck, &CountApply, Good}, {"b", &CountCheck, &CountApply, Mixed}, {"c", &CountCheck, &CountApply}};
    Applied = 0;
    Checked = 0;
    size_t Skipped = 0;
    CHECK(InstallAll(List, Code, &OnlyFirstUsable, &Skipped) == InstallResult::Installed);
    CHECK(Skipped == 1);
    CHECK(Checked == 2);
    CHECK(Applied == 2);

    // Ohne Prüfer gilt jede Stelle als brauchbar.
    Applied = 0;
    CHECK(InstallAll(List, Code, nullptr, nullptr) == InstallResult::Installed);
    CHECK(Applied == 3);

    // Der Tick einer Liste läuft nur, wenn keiner ihrer Patches ausgelassen wurde.
    CHECK_FALSE(ListComplete(List, &OnlyFirstUsable));
    CHECK(ListComplete(std::span(List, 1), &OnlyFirstUsable));
}

TEST_CASE("Signaturen: Tabelle sortiert, eigene Build-Zeile vor der gemeinsamen, Muster mit Platzhaltern")
{
    using namespace Game::Signatures;
    constexpr Entry Rows[] = {
        {0x10, 0, "AA BB", 0, "Shared"},
        {0x20, 0, "CC", 0, "Both"},
        {0x20, 26495302, "DD", 0, "Own"},
        {0x30, 26495302, "EE", 0, "OnlyOwn"},
    };
    static_assert(Sorted(Rows));
    static_assert(Sorted(Table));
    CHECK(Find(Rows, 0x10, 26495302) == 0);
    CHECK(Find(Rows, 0x20, 26495302) == 2);
    CHECK(Find(Rows, 0x20, 26187885) == 1);
    CHECK(Find(Rows, 0x30, 26187885) == -1);
    CHECK(Find(Rows, 0x40, 26495302) == -1);
    // lager-handel-client: neue Hook-Adresse muss auch unter der zentralen Signaturpruefung liegen.
    constexpr uint32_t SoldRva = 0x0596BA70;
    const int SoldReference = Find(Table, SoldRva, 26495302);
    REQUIRE(SoldReference >= 0);
    CHECK(std::strcmp(Table[SoldReference].Name, "HandleItemSold") == 0);
    const uint8_t SoldProlog[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
                                  0xEC, 0x20, 0x45, 0x33, 0xC0, 0x48, 0x8B, 0xDA};
    CHECK(Matches(Table[SoldReference].Pattern, SoldProlog));
    constexpr Entry Duplicate[] = {{0x10, 0, "AA", 0, "a"}, {0x10, 0, "AA", 0, "b"}};
    static_assert(!Sorted(Duplicate));

    const uint8_t Bytes[] = {0x48, 0x8B, 0x05, 0x90};
    CHECK(Matches("48 8B ?? 90", Bytes));
    CHECK(Matches("48 8B", Bytes));
    CHECK_FALSE(Matches("48 8C", Bytes));
    CHECK_FALSE(Matches("48 8B 05 90 CC", Bytes));
    CHECK_FALSE(Matches("?? ??", Bytes));
    CHECK_FALSE(Matches("4X", Bytes));
}

TEST_CASE("Patches: Teilinstallation setzt jede Liste ganz oder gar nicht")
{
    uint8_t Buffer[16]{};
    Code::Writer Code(Buffer, sizeof(Buffer));
    Applied = 0;
    const Patch Good[] = {{"a", &PassCheck, &CountApply}, {"b", &PassCheck, &CountApply}};
    const Patch Bad[] = {{"c", &PassCheck, &CountApply}, {"d", &FailCheck, &CountApply}};
    const Patch Late[] = {{"e", &PassCheck, &FailApply}};
    const PatchList Lists[] = {{"gut", Good}, {"schlecht", Bad}, {"gut2", Good}};
    bool On[3]{};
    const ListsResult Result = InstallLists(Lists, Code, On);
    CHECK(Result.Installed == 2);
    CHECK(Result.Skipped == 1);
    CHECK_FALSE(Result.ApplyFailed);
    CHECK(On[0]);
    CHECK_FALSE(On[1]);
    CHECK(On[2]);
    CHECK(Applied == 4);

    const PatchList Breaking[] = {{"bricht", Late}, {"gut", Good}};
    bool After[2]{};
    Applied = 0;
    const ListsResult Broken = InstallLists(Breaking, Code, After);
    CHECK(Broken.ApplyFailed);
    CHECK_FALSE(After[0]);
    CHECK_FALSE(After[1]);
    CHECK(Applied == 0);
}

TEST_CASE("Patches: Teilinstallation nie im Referenz-Build oder ohne Schalter")
{
    // 26600000 steht für einen künftigen Build mit eigener Tabelle.
    CHECK(PartialInstallAllowed(true, 26600000));
    CHECK_FALSE(PartialInstallAllowed(false, 26600000));
    CHECK_FALSE(PartialInstallAllowed(true, 26495302));
    CHECK_FALSE(PartialInstallAllowed(true, 0));
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

TEST_CASE("Patches: der lokale Controller kommt in der Controller-Liste nach vorn")
{
    const bool LateClient[] = {false, true};
    CHECK(LocalControllerBehindFront(LateClient, 2) == 1);
    const bool Normal[] = {true, false};
    CHECK(LocalControllerBehindFront(Normal, 2) == 2);
    // Alter Host-Controller (ohne Player) und Client vor dem neuen: der neue kommt nach vorn.
    const bool Swapped[] = {false, false, true};
    CHECK(LocalControllerBehindFront(Swapped, 3) == 2);
    const bool NoLocal[] = {false, false};
    CHECK(LocalControllerBehindFront(NoLocal, 2) == 2);
    CHECK(LocalControllerBehindFront(nullptr, 0) == 0);
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

TEST_CASE("Bewegung: Tempo der eigenen Figur folgt dem Attribut (tod-client)")
{
    // Erster Wert wird übernommen, auch volles Tempo.
    CHECK(WalkSpeedToApply(std::nullopt, 630.f, 630.f) == 630.f);
    // Tod: Kriechtempo vom Host, danach Wiederbelebung zurück auf volles Tempo.
    CHECK(WalkSpeedToApply(630.f, 630.f, 63.f) == 63.f);
    CHECK(WalkSpeedToApply(63.f, 630.f, 630.f) == 630.f);
    // Unverändertes Attribut: nichts schreiben, andere lokale Schreiber bleiben unberührt.
    CHECK_FALSE(WalkSpeedToApply(63.f, 630.f, 63.f));
    CHECK_FALSE(WalkSpeedToApply(63.f, 630.f, 63.005f));
    // Stillstand durch das Spiel ist erlaubt, wenn das Attribut initialisiert ist.
    CHECK(WalkSpeedToApply(630.f, 630.f, 0.f) == 0.f);
    // Nicht initialisiert oder Datenmüll: nie anwenden, sonst stünde die Figur still oder raste.
    CHECK_FALSE(WalkSpeedToApply(std::nullopt, 0.f, 0.f));
    CHECK_FALSE(WalkSpeedToApply(std::nullopt, 630.f, -1.f));
    CHECK_FALSE(WalkSpeedToApply(std::nullopt, 630.f, 1.0e6f));
    CHECK_FALSE(WalkSpeedToApply(std::nullopt, 630.f, std::numeric_limits<float>::quiet_NaN()));
    CHECK_FALSE(WalkSpeedToApply(std::nullopt, std::numeric_limits<float>::quiet_NaN(), 630.f));
}

TEST_CASE("Bewegung: tote eigene Figur dodgt beim Client nicht (tod-client)")
{
    CHECK(OwnDashBlockedByDeath(0.f, 100.f));
    CHECK(OwnDashBlockedByDeath(-5.f, 100.f));   // Überschaden
    CHECK_FALSE(OwnDashBlockedByDeath(50.f, 100.f)); // wiederbelebt mit halben HP
    CHECK_FALSE(OwnDashBlockedByDeath(0.f, 0.f));    // Attribut noch nicht initialisiert
    CHECK_FALSE(OwnDashBlockedByDeath(std::numeric_limits<float>::quiet_NaN(), 100.f));
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

TEST_CASE("Bewegung: Laufrichtung des Clients für den Dodge beim Host")
{
    // Gemessen beim Client: Eingabe (X, Y) läuft in Weltrichtung (Y, X). Beim Host aus der Ortsänderung.
    auto Input = RemoteMovementInput({0, 0}, 0);
    CHECK(Input.X == 0.0);
    CHECK(Input.Y == 0.0); // steht still: keine Eingabe, wie beim Client
    Input = RemoteMovementInput(PlanarVelocity({100, 100}, {100, 70}, 50), 0); // gemessen: Ort (0, -5084)/s
    CHECK(Input.X == doctest::Approx(-1.0));
    CHECK(Input.Y == doctest::Approx(0.0)); // Eingabe (-1, 0)
    Input = RemoteMovementInput({-4384, 4466}, 0);
    CHECK(Input.X == doctest::Approx(0.7071).epsilon(0.02));
    CHECK(Input.Y == doctest::Approx(-0.7071).epsilon(0.02)); // gemessen: Eingabe (0,71, -0,71)
    Input = RemoteMovementInput({0, 600}, RemoteMoveHoldMs); // Ort kommt in Schüben, kurz danach gilt er noch
    CHECK(Input.X == doctest::Approx(1.0));
    Input = RemoteMovementInput({0, 600}, RemoteMoveHoldMs + 1);
    CHECK(Input.X == 0.0);
    CHECK(Input.Y == 0.0); // länger ohne Bewegung: steht
    Input = RemoteMovementInput({RemoteInputMinSpeed, 0}, 0);
    CHECK(Input.X == 0.0);
    CHECK(Input.Y == 0.0); // kaum Tempo: steht praktisch
    CHECK(PlanarVelocity({0, 0}, {10, 0}, 0).X == 0.0);
}

TEST_CASE("Bewegung: Dodge-Diagnose unterscheidet fehlende und veraltete Richtung")
{
    CHECK(RemoteMovementState({0, 600}, RemoteMoveHoldMs) == RemoteInputState::Moving);
    CHECK(RemoteMovementState({0, 600}, RemoteMoveHoldMs + 1) == RemoteInputState::Expired);
    CHECK(RemoteMovementState({0, 600}, UINT64_MAX) == RemoteInputState::Expired); // noch keine Bewegungsprobe
    CHECK(RemoteMovementState({0, 0}, 0) == RemoteInputState::NoMovement);
    CHECK(RemoteMovementState({RemoteInputMinSpeed, 0}, 0) == RemoteInputState::NoMovement);
    for (const uint64_t Age : {uint64_t{0}, RemoteMoveHoldMs, RemoteMoveHoldMs + 1, UINT64_MAX})
        for (const PlanarDirection Velocity : {PlanarDirection{}, PlanarDirection{0, 600}, PlanarDirection{-600, 0}})
        {
            const PlanarDirection Input = RemoteMovementInput(Velocity, Age);
            CHECK((Input.X != 0 || Input.Y != 0) == (RemoteMovementState(Velocity, Age) == RemoteInputState::Moving));
        }
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

TEST_CASE("Busbalken beim Client folgt Tank und Figuren im Bereich")
{
    CHECK(BusBarActive(true, 2, 2));
    CHECK_FALSE(BusBarActive(false, 2, 2));
    CHECK_FALSE(BusBarActive(true, 1, 2));
    CHECK_FALSE(BusBarActive(true, 0, 0));
    CHECK(BusBarElapsed(true, 0.0, 3.0, 0.5) == 0.5);
    CHECK(BusBarElapsed(true, 2.9, 3.0, 0.5) == 3.0);
    CHECK(BusBarElapsed(true, 1.0, 3.0, -1.0) == 1.0);
    CHECK(BusBarElapsed(false, 2.0, 3.0, 0.1) == 0.0);
    CHECK(BusBarElapsed(true, 1.0, 0.0, 0.1) == 0.0);
}

TEST_CASE("Blickrichtung beim Client folgt dem Gierwinkel der Hände")
{
    CHECK(FacingYawFromQuat(0, 0, 0, 1) == doctest::Approx(0.0));
    const double Half = std::sqrt(0.5);
    CHECK(FacingYawFromQuat(0, 0, Half, Half) == doctest::Approx(90.0));
    CHECK(FacingYawFromQuat(0, 0, -Half, Half) == doctest::Approx(-90.0));
    CHECK(std::fabs(FacingYawFromQuat(0, 0, 1, 0)) == doctest::Approx(180.0));
    // Gemessen beim Host: Quaternion zu Gierwinkel -127,9 Grad.
    const double Angle = -127.9 / 57.29577951308232 / 2;
    CHECK(FacingYawFromQuat(0, 0, std::sin(Angle), std::cos(Angle)) == doctest::Approx(-127.9).epsilon(0.001));
    CHECK(std::isnan(FacingYawFromQuat(0, 0, 0, 0)));
    CHECK(std::isnan(FacingYawFromQuat(0, 0, 0, 2)));
    CHECK(std::isnan(FacingYawFromQuat(std::numeric_limits<double>::quiet_NaN(), 0, 0, 1)));

    CHECK(FacingNeedsWrite(0.0f, 45.0f));
    CHECK_FALSE(FacingNeedsWrite(45.0f, 45.001f));
    CHECK_FALSE(FacingNeedsWrite(0.0f, std::numeric_limits<float>::quiet_NaN()));
    CHECK(FacingNeedsWrite(std::numeric_limits<float>::quiet_NaN(), 10.0f));
}

TEST_CASE("Lootbalken beim Client folgt dem replizierten Fortschritt")
{
    CHECK(LootBarValidProgress(0.0));
    CHECK(LootBarValidProgress(1.0));
    CHECK_FALSE(LootBarValidProgress(-0.1));
    CHECK_FALSE(LootBarValidProgress(4e37));
    const double NotANumber = std::numeric_limits<double>::quiet_NaN();
    CHECK_FALSE(LootBarValidProgress(NotANumber));

    CHECK(LootBarNeedsUpdate(0.0, 0.1));
    CHECK(LootBarNeedsUpdate(0.5, 0.0));
    CHECK_FALSE(LootBarNeedsUpdate(0.5, 0.5005));

    CHECK(LootBarDone(false, 0.0, 0.0, 1.0));
    CHECK_FALSE(LootBarDone(true, 0.0, 0.0, 9.0));
    CHECK_FALSE(LootBarDone(false, 0.0, 0.0, 0.5));
    CHECK_FALSE(LootBarDone(false, 0.0, 0.5, 9.0));
}

TEST_CASE("Schusssound beim Client je Schuss einmal")
{
    CHECK(WeaponShotRepeated(0));
    CHECK(WeaponShotRepeated(WeaponShotRepeatMs - 1));
    CHECK_FALSE(WeaponShotRepeated(WeaponShotRepeatMs));
    CHECK_FALSE(WeaponShotRepeated(50));
    CHECK_FALSE(WeaponShotRepeated(~0ull)); // erster Schuss einer Waffe
}

TEST_CASE("Körperteile beim Client je Tod einmal und begrenzt je Frame")
{
    CHECK(BodyPartsRepeated(0.0));
    CHECK(BodyPartsRepeated(0.9));
    CHECK_FALSE(BodyPartsRepeated(1.0));
    CHECK_FALSE(BodyPartsRepeated(30.0));
    CHECK_FALSE(BodyPartsRepeated(-1.0));

    CHECK(BodyPartsMayShow(0));
    CHECK(BodyPartsMayShow(BodyPartsPerFrame - 1));
    CHECK_FALSE(BodyPartsMayShow(BodyPartsPerFrame));
    CHECK_FALSE(BodyPartsMayShow(-1));
}

TEST_CASE("Laufanimation der Client-Figur misst der Host aus ihrem Ort")
{
    CHECK(WalkAnimationNeedsManualTracking(true, true, false));
    CHECK_FALSE(WalkAnimationNeedsManualTracking(true, true, true));
    CHECK_FALSE(WalkAnimationNeedsManualTracking(true, false, false));
    CHECK_FALSE(WalkAnimationNeedsManualTracking(false, true, false));
}

TEST_CASE("Busabfahrt und Rundenende entscheidet nur der Host")
{
    CHECK(BusTakeOffMayDecide(false));
    CHECK_FALSE(BusTakeOffMayDecide(true));
    CHECK(MissionFinishAllowed(false, false));
    CHECK(MissionFinishAllowed(false, true));
    CHECK(MissionFinishAllowed(true, true));
    CHECK_FALSE(MissionFinishAllowed(true, false));
}

TEST_CASE("Rundenende: Client zieht das Ende des Hosts nur für dieselbe Karte und einmal nach")
{
    CHECK(ClientMayFollowRoundEnd(42, 42, false));
    CHECK_FALSE(ClientMayFollowRoundEnd(42, 42, true));
    CHECK_FALSE(ClientMayFollowRoundEnd(42, 43, false));
    CHECK_FALSE(ClientMayFollowRoundEnd(42, 0, false));
    CHECK_FALSE(ClientMayFollowRoundEnd(0, 0, false));
}

TEST_CASE("Run-Ende: Eine Niederlage löst der Client nur auf Meldung des Hosts aus")
{
    CHECK(RunLossAllowed(false, RoundChangeLose)); // Host und Solo
    CHECK_FALSE(RunLossAllowed(true, RoundChangeLose));
    CHECK(RunLossAllowed(true, 1)); // Tagesende sperrt MissionFinishAllowed
    CHECK(RunLossAllowed(true, 2));
    CHECK(ShouldRetryLossPanel(0, 5));
    CHECK(ShouldRetryLossPanel(4, 5));
    CHECK_FALSE(ShouldRetryLossPanel(5, 5));
}

TEST_CASE("Reise: Der Client startet keine eigene Reise außer ins Hauptmenü")
{
    CHECK(ClientMayStartTravel(false, "/Game/Maps/DDDynamic_Map"));
    CHECK(ClientMayStartTravel(false, ""));
    CHECK_FALSE(ClientMayStartTravel(true, "/Game/Maps/DDDynamic_Map"));
    CHECK_FALSE(ClientMayStartTravel(true, "/Game/Maps/MetaBase"));
    CHECK_FALSE(ClientMayStartTravel(true, ""));
    CHECK(ClientMayStartTravel(true, "/Game/Maps/MainMenu"));
}

TEST_CASE("Lager am Rundenende: Host wartet nur auf einen Client, der dieses Ende geöffnet hat und noch nicht bereit ist")
{
    CHECK(HostHoldsRoundEndConfirm(7, 0, true, 7, 0));
    CHECK_FALSE(HostHoldsRoundEndConfirm(7, 0, true, 7, 7));  // Client schon bereit
    CHECK_FALSE(HostHoldsRoundEndConfirm(7, 0, false, 7, 0)); // Client getrennt
    CHECK_FALSE(HostHoldsRoundEndConfirm(7, 0, true, 0, 0));  // Client hat die Runde nicht nachgezogen
    CHECK_FALSE(HostHoldsRoundEndConfirm(7, 0, true, 5, 0));  // altes Ende des Clients
    CHECK_FALSE(HostHoldsRoundEndConfirm(7, 7, true, 7, 0));  // schon bestätigt
    CHECK_FALSE(HostHoldsRoundEndConfirm(0, 0, true, 0, 0));  // kein Rundenende
    CHECK(HostHoldsRoundEndConfirm(7, 0, true, 7, 5));        // Bereit für ein altes Ende zählt nicht
}

TEST_CASE("Lager am Rundenende: Client wartet nur mit frischem Stand des Hosts und gibt nie hängend auf")
{
    CHECK(ClientHoldsRoundEndConfirm(7, 0, 7));
    CHECK_FALSE(ClientHoldsRoundEndConfirm(7, 0, 0)); // kein Stand vom Host
    CHECK_FALSE(ClientHoldsRoundEndConfirm(7, 0, 5)); // Host in anderem Ende
    CHECK_FALSE(ClientHoldsRoundEndConfirm(7, 7, 7)); // schon bestätigt
    CHECK_FALSE(ClientHoldsRoundEndConfirm(0, 0, 0));

    CHECK_FALSE(ClientMayReleaseRoundEnd(7, 7, false, false));
    CHECK_FALSE(ClientMayReleaseRoundEnd(7, 7, true, false)); // Host hat das Bereit des Clients noch nicht
    CHECK_FALSE(ClientMayReleaseRoundEnd(7, 7, false, true)); // Host noch nicht bereit
    CHECK(ClientMayReleaseRoundEnd(7, 7, true, true));
    CHECK(ClientMayReleaseRoundEnd(7, 0, false, false)); // Stand fehlt: nicht hängen bleiben
    CHECK(ClientMayReleaseRoundEnd(7, 5, false, false));
}

TEST_CASE("Lager: nachgeholtes Bestätigen nur mit lebendem Panel in derselben Welt (lager-bestaetigen-absturz)")
{
    constexpr uintptr_t Panel = 0x1000, Other = 0x2000;
    CHECK(HeldObjectLive(Panel, Panel, 5, 5, 0, 0));
    CHECK(HeldObjectLive(Panel, Panel, 0, 9, 0, 0)); // beim Merken noch ohne Serial
    CHECK_FALSE(HeldObjectLive(0, 0, 0, 0, 0, 0));
    CHECK_FALSE(HeldObjectLive(Panel, 0, 5, 5, 0, 0));     // Platz frei: Objekt nach der Reise freigegeben
    CHECK_FALSE(HeldObjectLive(Panel, Other, 5, 5, 0, 0)); // Platz von einem anderen Objekt belegt
    CHECK_FALSE(HeldObjectLive(Panel, Panel, 5, 6, 0, 0)); // gleiche Adresse, neues Objekt
    CHECK_FALSE(HeldObjectLive(Panel, Panel, 5, 5, 0x00200000, 0)); // Garbage
    CHECK_FALSE(HeldObjectLive(Panel, Panel, 5, 5, 0x10000000, 0)); // Unreachable
    CHECK(HeldObjectLive(Panel, Panel, 5, 5, 0x40000000, 0));       // RootSet ist kein Abbau
    CHECK_FALSE(HeldObjectLive(Panel, Panel, 5, 5, 0, 0x8000));     // BeginDestroyed
    CHECK_FALSE(HeldObjectLive(Panel, Panel, 5, 5, 0, 0x10000));    // FinishDestroyed
    CHECK_FALSE(HeldObjectLive(Panel, Panel, 5, 5, 0, 0x20));

    constexpr uintptr_t World = 0x7000, Next = 0x8000;
    CHECK(MayReplayHeldConfirm(true, World, World));
    CHECK_FALSE(MayReplayHeldConfirm(false, World, World));
    CHECK_FALSE(MayReplayHeldConfirm(true, World, Next)); // Host ist schon weitergereist
    CHECK_FALSE(MayReplayHeldConfirm(true, World, 0));    // GWorld während der Reise leer
    CHECK_FALSE(MayReplayHeldConfirm(true, 0, 0));
}

TEST_CASE("Eingaben: lokale Geräte im Netzwerkspiel nur für den eigenen Spieler")
{
    for (const NetRole Role : {NetRole::Host, NetRole::Client})
    {
        CHECK_FALSE(BlocksDeviceAssignment(Role, 0));  // eigener Spieler, etwa Gamepad statt Tastatur
        CHECK_FALSE(BlocksDeviceAssignment(Role, -1)); // Zuordnung lösen
        CHECK(BlocksDeviceAssignment(Role, 1));        // P2 ist beim Host der Remote-Spieler, beim Client gibt es keinen
        CHECK(BlocksDeviceAssignment(Role, 2));
    }
    for (const int32_t Index : {-1, 0, 1, 2})
        CHECK_FALSE(BlocksDeviceAssignment(NetRole::Standalone, Index)); // lokaler Koop ohne Netzwerk bleibt nativ
}

TEST_CASE("Eingaben: kein zweiter LocalPlayer im Netzwerkspiel")
{
    CHECK_FALSE(BlocksLocalSpawn(NetRole::Host, 0, false));
    CHECK_FALSE(BlocksLocalSpawn(NetRole::Client, 0, false));
    CHECK_FALSE(BlocksLocalSpawn(NetRole::Host, 1, true)); // nativ: gibt den eingetragenen Remote-P2 zurück, legt nichts an
    CHECK(BlocksLocalSpawn(NetRole::Host, 1, false));      // würde einen lokalen P2 neben dem Remote-Spieler anlegen
    CHECK(BlocksLocalSpawn(NetRole::Client, 1, false));
    CHECK(BlocksLocalSpawn(NetRole::Client, 1, true));     // beim Client zählt ein Controller an Index 1 nicht
    CHECK_FALSE(BlocksLocalSpawn(NetRole::Standalone, 1, false));
}

TEST_CASE("Bewegung: gemeldete Dodge-Richtung des Clients je Vorhersageschlüssel")
{
    // Client: genau die Eingabe, die GA_Dash liest, normiert; aus dem Stand (0, 0).
    auto Value = ReportedDashInput(5, {0.0, 0.5});
    CHECK(Value.Key == 5);
    CHECK(Value.X == doctest::Approx(0.0));
    CHECK(Value.Y == doctest::Approx(1.0));
    Value = ReportedDashInput(6, {3.0, -4.0});
    CHECK(Value.X == doctest::Approx(0.6));
    CHECK(Value.Y == doctest::Approx(-0.8));
    CHECK(ReportedDashInput(7, {0.0, 0.0}) == DashInput{7, 0.f, 0.f});

    // Host: eine Meldung gilt für genau eine Aktivierung, unabhängig von der Ankunftsreihenfolge.
    DashInputBook Book;
    Book.Add({10, 1.f, 0.f}, 1000);
    Book.Add({11, 0.f, 1.f}, 1001);
    CHECK_FALSE(Book.Take(12, 1002));
    REQUIRE(Book.Take(11, 1002));
    CHECK_FALSE(Book.Take(11, 1003));
    REQUIRE(Book.Take(10, 1003));
    CHECK(Book.Size() == 0);

    // Wiederholter Schlüssel ersetzt, alte Meldungen ohne Aktivierung verfallen.
    Book.Add({20, 1.f, 0.f}, 2000);
    Book.Add({20, -1.f, 0.f}, 2001);
    CHECK(Book.Size() == 1);
    CHECK(Book.Take(20, 2002)->X == -1.f);
    Book.Add({21, 1.f, 0.f}, 3000);
    CHECK_FALSE(Book.Take(21, 3000 + DashInputKeepMs + 1));
    CHECK(DashWaitMs < DashInputKeepMs);
}

TEST_CASE("Bewegung: eigener Dodge gilt nur so lange als laufend, wie sein Schub dauert")
{
    CHECK(DashExpectedMs(0.135f) == 195);
    CHECK(DashExpectedMs(std::nullopt) == 250);
    CHECK(DashExpectedMs(0.f) == 250);
    CHECK(DashExpectedMs(5.f) == 250);
    CHECK(DashExpectedMs(std::numeric_limits<float>::quiet_NaN()) == 250);
    CHECK(DashExpectedMs(0.3f) < 750); // alte Zeitgrenze bleibt nur Rückfall
}

TEST_CASE("lager-edit-stabil: Aufwertung nach dem Bereit im Lager beginnt eine neue Bearbeitung statt nativ zu laufen")
{
    // Live-Test 14.11.2026 (Mod-Version 13): Client bereit, wartet auf den Host, legt dann zwei Pistolen aufeinander.
    CHECK(ClientReopensLager(7, 0, true, false, true));
    // Ziehen allein (kein Spielerzug wie Aufwerten, Verkaufen, Reparieren) nimmt das Bereit nicht zurück.
    CHECK_FALSE(ClientReopensLager(7, 0, true, false, false));
    // Bestätigen schon nativ gelaufen, Edit wartet auf Antwort, kein Rundenende oder nicht bereit: nichts zurücknehmen.
    CHECK_FALSE(ClientReopensLager(7, 7, true, false, true));
    CHECK_FALSE(ClientReopensLager(7, 0, true, true, true));
    CHECK_FALSE(ClientReopensLager(0, 0, true, false, true));
    CHECK_FALSE(ClientReopensLager(7, 0, false, false, true));
    // Der Zug im Rucksack bleibt beim gehaltenen Bestätigen wie bisher nativ.
    CHECK(ClientMoveBegins(false, false, false, 7, 0, true, true) == ClientMove::Native);
}

TEST_CASE("opt-station-zug: erster Zug des Clients beginnt eine Bearbeitung, im Lager oder an einer Station")
{
    // Lager am Rundenende: wie bisher, solange nicht bestätigt.
    CHECK(ClientMoveBegins(false, false, false, 7, 0, false, true) == ClientMove::BeginLager);
    CHECK(ClientMoveBegins(false, false, false, 7, 7, false, true) == ClientMove::Native); // Lager schon bestätigt
    CHECK(ClientMoveBegins(false, false, false, 7, 0, true, true) == ClientMove::Native);  // Bestätigen wird gehalten
    // Ohne Rundenende in einer Mission: Stations-Bearbeitung.
    CHECK(ClientMoveBegins(false, false, false, 0, 0, false, true) == ClientMove::BeginStation);
    CHECK(ClientMoveBegins(false, false, false, 0, 5, false, true) == ClientMove::BeginStation); // altes Rundenende ist vorbei
    // Außerhalb einer Mission (MetaBase) bleibt es nativ.
    CHECK(ClientMoveBegins(false, false, false, 0, 0, false, false) == ClientMove::Native);
    // Laufende Bearbeitung, offener Edit oder offene Airdrop-Auswahl: deren eigener Weg.
    CHECK(ClientMoveBegins(true, false, false, 0, 0, false, true) == ClientMove::Native);
    CHECK(ClientMoveBegins(false, true, false, 7, 0, false, true) == ClientMove::Native);
    CHECK(ClientMoveBegins(false, false, true, 0, 0, false, true) == ClientMove::Native);
}
