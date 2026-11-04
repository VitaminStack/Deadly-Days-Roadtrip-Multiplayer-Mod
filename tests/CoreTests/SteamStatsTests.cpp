#include <doctest.h>

#include <cstdint>
#include <cstring>
#include <string_view>

#include "Core/Hooks.h"
#include "Core/SteamStats.h"

using Core::SteamStats::VtableOffsetFromThunk;
namespace Testing = Core::SteamStats::Testing;

// Bytes wie in steam_api64.dll (Steamworks 1.64): mov rax,[rcx], dann der Sprung über die vtable.
TEST_CASE("SteamStats liest den vtable-Abstand aus kurzem Sprung")
{
    const uint8_t SetAchievement[] = {0x48, 0x8B, 0x01, 0x48, 0xFF, 0x60, 0x30, 0xCC};
    CHECK(VtableOffsetFromThunk(SetAchievement, sizeof(SetAchievement)) == 0x30u);
}

TEST_CASE("SteamStats liest den vtable-Abstand aus langem Sprung")
{
    const uint8_t AttachLeaderboardUgc[] = {0x48, 0x8B, 0x01, 0x48, 0xFF, 0xA0, 0xF8, 0x00, 0x00, 0x00, 0xCC};
    CHECK(VtableOffsetFromThunk(AttachLeaderboardUgc, sizeof(AttachLeaderboardUgc)) == 0xF8u);
}

TEST_CASE("SteamStats liest den vtable-Abstand über r10")
{
    const uint8_t UploadLeaderboardScore[] = {0x48, 0x8B, 0x01, 0x4C, 0x8B, 0x90, 0xF0, 0x00, 0x00, 0x00, 0x49, 0xFF, 0xE2, 0xCC};
    CHECK(VtableOffsetFromThunk(UploadLeaderboardScore, sizeof(UploadLeaderboardScore)) == 0xF0u);
}

TEST_CASE("SteamStats lehnt fremden Code ab")
{
    const uint8_t Accessor[] = {0x48, 0x83, 0xEC, 0x28, 0x48, 0x8D, 0x0D, 0x35};
    CHECK_FALSE(VtableOffsetFromThunk(Accessor, sizeof(Accessor)).has_value());
    const uint8_t Short[] = {0x48, 0x8B, 0x01, 0x48, 0xFF};
    CHECK_FALSE(VtableOffsetFromThunk(Short, sizeof(Short)).has_value());
    CHECK_FALSE(VtableOffsetFromThunk(nullptr, 16).has_value());
}

// Nachgebaute Steam-Laufzeit: acht Schreibmethoden in einer vtable, flache Exporte als Code-Bytes und
// FindOrCreateUserInterface. Jede Methode zählt, wenn das Original durchkommt.
namespace
{
int OriginalWrites = 0;
int32_t SteamUser = 0;
bool FailMethodHooks = false;
bool BreakThunk = false;

#define FAKE_METHOD(Index)                                                                                                  \
    __declspec(noinline) bool Method##Index(void*)                                                                           \
    {                                                                                                                       \
        volatile int Marker = Index;                                                                                         \
        OriginalWrites += 1 + Marker * 0;                                                                                    \
        return true;                                                                                                        \
    }
FAKE_METHOD(0)
FAKE_METHOD(1)
FAKE_METHOD(2)
FAKE_METHOD(3)
FAKE_METHOD(4)
FAKE_METHOD(5)
FAKE_METHOD(6)
FAKE_METHOD(7)
#undef FAKE_METHOD

using MethodFn = bool (*)(void*);
MethodFn Vtable[8] = {&Method0, &Method1, &Method2, &Method3, &Method4, &Method5, &Method6, &Method7};
struct FakeInterface
{
    MethodFn* Table;
} Stats{Vtable}, Other{Vtable};

__declspec(noinline) void* Find(int32_t, const char* Version)
{
    volatile int Marker = 1;
    return std::string_view(Version).starts_with("STEAMUSERSTATS") && Marker ? static_cast<void*>(&Stats) : static_cast<void*>(&Other);
}

__declspec(noinline) int32_t User()
{
    volatile int32_t Value = SteamUser;
    return Value;
}

// Kurzer Sprung über Platz Index * 8, wie die echten Exporte.
uint8_t Thunks[8][8];

const char* const Names[8] = {"SteamAPI_ISteamUserStats_SetAchievement", "SteamAPI_ISteamUserStats_IndicateAchievementProgress",
                              "SteamAPI_ISteamUserStats_SetStatInt32", "SteamAPI_ISteamUserStats_SetStatFloat",
                              "SteamAPI_ISteamUserStats_UpdateAvgRateStat", "SteamAPI_ISteamUserStats_StoreStats",
                              "SteamAPI_ISteamUserStats_UploadLeaderboardScore", "SteamAPI_ISteamUserStats_AttachLeaderboardUGC"};

void* Export(const char* Name)
{
    for (int Index = 0; Index < 8; ++Index)
    {
        if (std::strcmp(Name, Names[Index]) != 0)
            continue;
        const uint8_t Bytes[8] = {0x48, 0x8B, 0x01, 0x48, 0xFF, 0x60, static_cast<uint8_t>(Index * 8), 0xCC};
        std::memcpy(Thunks[Index], Bytes, 8);
        if (BreakThunk && Index == 3)
            Thunks[Index][0] = 0x90;
        return Thunks[Index];
    }
    if (std::strcmp(Name, "SteamInternal_FindOrCreateUserInterface") == 0)
        return reinterpret_cast<void*>(&Find);
    if (std::strcmp(Name, "SteamAPI_GetHSteamUser") == 0)
        return reinterpret_cast<void*>(&User);
    return nullptr;
}

void* NoExport(const char*)
{
    return nullptr;
}

bool Install(void* Target, void* Detour, void** Original)
{
    if (FailMethodHooks && Target != reinterpret_cast<void*>(&Find))
        return false;
    return Core::Hooks::Install(Target, Detour, Original);
}

// Ruft die Schnittstelle so auf wie das Spiel: über die vtable.
bool Write(void* Interface, int Slot)
{
    return static_cast<FakeInterface*>(Interface)->Table[Slot](Interface);
}

// Wie das Spiel: Schnittstelle über den (umgeleiteten) Export holen.
void* Request(const char* Version)
{
    return reinterpret_cast<void* (*)(int32_t, const char*)>(Export("SteamInternal_FindOrCreateUserInterface"))(1, Version);
}

struct Fixture
{
    Fixture()
    {
        REQUIRE(Core::Hooks::Initialize());
        Testing::Reset();
        OriginalWrites = 0;
        SteamUser = 0;
        FailMethodHooks = false;
        BreakThunk = false;
    }
    ~Fixture()
    {
        Testing::Reset();
        Core::Hooks::Shutdown();
    }
};
}

TEST_CASE_FIXTURE(Fixture, "SteamStats: Steam startet nach der Mod, erster Schreibaufruf ist schon gesperrt")
{
    REQUIRE(Testing::Attach({&Export, &Install}));
    // Steam noch nicht gestartet: Nichts wird vorab angefragt.
    void* Interface = Request("STEAMUSERSTATS_INTERFACE_VERSION013");
    REQUIRE(Interface == &Stats);
    const int Before = Core::SteamStats::BlockedCalls();
    for (int Slot = 0; Slot < 8; ++Slot)
        CHECK_FALSE(Write(Interface, Slot));
    CHECK(OriginalWrites == 0);
    CHECK(Core::SteamStats::BlockedCalls() == Before + 8);
}

TEST_CASE_FIXTURE(Fixture, "SteamStats: Schnittstelle gab es schon, Attach sperrt sofort")
{
    SteamUser = 7;
    REQUIRE(Testing::Attach({&Export, &Install}));
    // Direkt über die vtable, ohne erneute Anfrage.
    CHECK_FALSE(Write(&Stats, 0));
    CHECK_FALSE(Write(&Stats, 5));
    CHECK(OriginalWrites == 0);
}

TEST_CASE_FIXTURE(Fixture, "SteamStats: Scheitert das Sperren, bekommt das Spiel keine Schnittstelle")
{
    FailMethodHooks = true;
    REQUIRE(Testing::Attach({&Export, &Install}));
    CHECK(Request("STEAMUSERSTATS_INTERFACE_VERSION013") == nullptr);
    CHECK(OriginalWrites == 0);
}

TEST_CASE_FIXTURE(Fixture, "SteamStats: Scheitert das Sperren einer schon vorhandenen Schnittstelle, scheitert Attach")
{
    FailMethodHooks = true;
    SteamUser = 7;
    CHECK_FALSE(Testing::Attach({&Export, &Install}));
}

TEST_CASE_FIXTURE(Fixture, "SteamStats: unbekannter Einsprung oder fehlende DLL lassen Attach scheitern")
{
    BreakThunk = true;
    CHECK_FALSE(Testing::Attach({&Export, &Install}));
    Testing::Reset();
    BreakThunk = false;
    CHECK_FALSE(Testing::Attach({&NoExport, &Install}));
}

TEST_CASE_FIXTURE(Fixture, "SteamStats: andere Versionen und Schnittstellen")
{
    REQUIRE(Testing::Attach({&Export, &Install}));
    // Unbekannte Version: Methodenplätze unbekannt, also keine Schnittstelle.
    CHECK(Request("STEAMUSERSTATS_INTERFACE_VERSION012") == nullptr);
    // Andere Schnittstellen bleiben unberührt.
    CHECK(Request("SteamUser023") == &Other);
    CHECK(Testing::OnInterface(nullptr, &Other) == &Other);
    CHECK(Testing::OnInterface("STEAMUSERSTATS_INTERFACE_VERSION013", nullptr) == nullptr);
}
