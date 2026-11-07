// Gemeinsame Schnittstelle der Spike-Dateien. Code- und Datenadressen nennt der Quelltext fuer Build 26187885; Rva() rechnet sie
// fuer 26495302 um (Builds.cpp). Feld-Offsets sind in beiden Builds gleich.
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

namespace Spike
{
// Builds.cpp
constexpr uintptr_t kMissingRva = 0x7F000000; // Base + kMissingRva liegt hinter dem Abbild, ein Lesen scheitert statt Fremdes zu treffen
uintptr_t Rva(uintptr_t ReferenceRva);
bool IsReferenceBuild();
bool IsSupportedBuild();
uintptr_t FindImportSlot(const void* Function);

void TickRemoteP2();
bool RemoteP2Command(const std::wstring& Command);
const uintptr_t kGObjects = Rva(0x0AA61B00);
const uintptr_t kNameBlocks = Rva(0x0A993AC0) + 0x10;
const uintptr_t kGWorld = Rva(0x0AAE46F0);
constexpr int kProcessEventIndex = 0x4A;
constexpr uintptr_t kWorldNetDriver = 0x38;
constexpr uintptr_t kWorldGameMode = 0x1A8;
constexpr uintptr_t kWorldGameState = 0x1B0;
constexpr uintptr_t kGameStatePlayerArray = 0x2C0;
constexpr uintptr_t kDriverServerConnection = 0x100;

using ProcessEventFn = void(__fastcall*)(uintptr_t, uintptr_t, void*);

extern uintptr_t Base;

// Wahr, wenn die Mod (DDCoop.dll) die Spielfehler-Fixes selbst gesetzt hat. Dann setzt der Spike seine Fassungen nicht,
// siehe README "Mod und Spike".
extern bool ModOwnsPatches;

template <typename T> T Read(uintptr_t Address) { return *reinterpret_cast<T*>(Address); }

// Core.cpp
void SetLogPath(const std::wstring& Path);
void Log(const std::string& Line);
std::string NameFromIndex(int32_t Index);
std::string NameOf(uintptr_t Object);
uintptr_t FindObject(const char* Name, const char* OuterName);
uintptr_t FindInstance(const char* ClassName);
void RunConsoleCommand(const std::wstring& Command);

// Patch-Speicher in Sprungweite der Exe. Alle Patches schreiben nacheinander hinein.
class CodeWriter
{
public:
    bool Allocate();
    uint8_t* Here() const { return Out; }
    void Emit(std::initializer_list<uint8_t> Bytes);
    void EmitRel(uintptr_t Target);
    // mov rax,Target; jmp rax. Liefert den Anfang, damit eine Stelle dorthin springen kann.
    uint8_t* EmitAbsoluteJump(uintptr_t Target);

private:
    uint8_t* Out = nullptr;
};

// Prueft die Originalbytes einer Stelle und protokolliert eine Abweichung.
bool Expect(uintptr_t Site, std::initializer_list<uint8_t> Bytes, const char* Where);
bool ExpectBranch(uintptr_t Site, uint8_t Opcode, uintptr_t TargetReferenceRva, const char* Where);
void WriteJump(uintptr_t From, uint8_t* To);

// Main.cpp
void QueueCommand(const std::wstring& Command);

// Players.cpp
bool PatchPlayers(CodeWriter& Code);
void AcceptClientPositions();
void RepairSpawnedPlayers(uintptr_t World);

// Client.cpp
bool PatchClient(CodeWriter& Code);

// Camera.cpp
bool PatchCamera(CodeWriter& Code);
void ViewThroughSharedCamera();

// Inventory.cpp
bool PatchInventory(CodeWriter& Code);

// Travel.cpp
bool PatchTravel(CodeWriter& Code);
void ArmTravelRedirect();

// Widgets.cpp
bool PatchWidgets(CodeWriter& Code);
void ReleaseDeferredWidgetEvents(uintptr_t World);
// Hält die Mod ProcessEvent, meldet sich der Phasenbeobachter dort an.
bool ObserveEventsThroughMod(HMODULE Mod);

// MapObjects.cpp
bool PatchMapObjects(CodeWriter& Code);

// CrashReports.cpp
bool PatchCrashReports(CodeWriter& Code);

// MapGen.cpp
bool PatchMapGen(CodeWriter& Code);
void TickMapGen();
uintptr_t LoadObjectByPath(const std::string& Path);

// Pcg.cpp
bool PatchPcg(CodeWriter& Code);
void TickPcg();

// Cheats.cpp
void EnableCheats();
void TickTestGodmode();
void SetTestGodmode(bool Enabled);
void LoadAssetCommand(const std::wstring& Path);
void GiveItemCommand(const std::wstring& Args);
// Setzt den Tutorial-Schalter, falls noch nicht gesetzt. Report meldet auch, wenn nichts zu tun war.
void SkipTutorials(bool Report);
// Testumgebung: protokolliert beim Host das Ende jeder Spielerfigur mit Aufrufstapel.
void TickTestEnv();
// Testumgebung: Spielerfiguren beim Freiraeumen des Busplatzes schonen.
bool PatchTestEnv(CodeWriter& Code);

// Hits.cpp
void TickHits();

// Mission.cpp
void ForceEnemyWave();
void FinishMission();
void EndRound(uint8_t Reason);

// Trace.cpp
void TraceCharacterRpcs();
void TraceRunFlow();

// Load.cpp
bool PatchLoad();
void TickLoad();

// HealthBars.cpp
bool PatchHealthBars(CodeWriter& Code);

// Tunnel.cpp
void TunnelCommand(const std::wstring& Arguments);

// RunEnd.cpp
bool PatchRunEnd(CodeWriter& Code);

}
