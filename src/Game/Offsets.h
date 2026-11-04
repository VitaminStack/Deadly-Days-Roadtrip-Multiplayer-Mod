#pragma once

#include <cstdint>

// Speicherstellen im Spiel, gültig nur für Build 26187885. Quelle und Herkunft stehen in docs/OFFSETS.md.
// Ist das Dumper-7-SDK lokal eingerichtet, prüft SdkChecks.cpp jeden Wert beim Bauen gegen das SDK.
namespace Game::Offsets
{
// RVAs relativ zur Image-Base der Exe.
inline constexpr uintptr_t GObjects = 0x0AA61B00;
inline constexpr uintptr_t GNames = 0x0A993AC0;
inline constexpr uintptr_t GWorld = 0x0AAE46F0;
inline constexpr uintptr_t ProcessEvent = 0x0160C040;
inline constexpr int ProcessEventIndex = 0x4A;

// Namenstabelle: Blöcke ab +0x10, Index >> 16 wählt den Block, (Index & 0xFFFF) * 2 den Eintrag.
inline constexpr uintptr_t NameBlocks = 0x10;
inline constexpr int NameStride = 2;

// GObjects (TUObjectArray): Zeiger auf die Chunks, dann die Anzahl. Je Chunk 0x10000 Einträge, im Eintrag liegt das Objekt bei +8.
namespace ObjectArray
{
inline constexpr uintptr_t NumElements = 0x08;
inline constexpr int ElementsPerChunk = 0x10000;
inline constexpr uintptr_t ItemSize = 0x18;
inline constexpr uintptr_t ItemObject = 0x08;
}

namespace UObject
{
inline constexpr uintptr_t Flags = 0x08;
inline constexpr uint32_t ClassDefaultObjectFlag = 0x10;
inline constexpr uintptr_t Class = 0x10;
inline constexpr uintptr_t Name = 0x18;
inline constexpr uintptr_t Outer = 0x20;
}

namespace UStruct
{
inline constexpr uintptr_t SuperStruct = 0x40;
}

namespace UWorld
{
inline constexpr uintptr_t PersistentLevel = 0x30;
inline constexpr uintptr_t NetDriver = 0x38;
inline constexpr uintptr_t AuthorityGameMode = 0x1A8;
inline constexpr uintptr_t GameState = 0x1B0;
inline constexpr uintptr_t OwningGameInstance = 0x228;
}

namespace UNetDriver
{
inline constexpr uintptr_t ServerConnection = 0x100;
inline constexpr uintptr_t ClientConnections = 0x108;
}

namespace AGameStateBase
{
inline constexpr uintptr_t PlayerArray = 0x2C0;
}

namespace UTutorialStateSubsystem
{
inline constexpr uintptr_t SkipTutorialFlag = 0xB8; // int32, nach jedem Spielstart wieder 0
}

// Einstellungen der Telemetrie und der automatischen Berichte, gelesen am Klassen-Defaultobjekt.
namespace UDDBalanceTelemetrySettings
{
inline constexpr uintptr_t Enabled = 0x38;        // bool
inline constexpr uintptr_t ServerBaseURL = 0x40;  // FString
inline constexpr uintptr_t AttachSaveFile = 0x60; // bool
}

namespace UDDCorruptedSaveAutoReportSettings
{
inline constexpr uintptr_t EnableAutomaticReports = 0x38; // bool
}
}
