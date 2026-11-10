#pragma once

#include <cstdint>

// Speicherstellen im Spiel, gültig nur für Build 26495302. Quelle und Herkunft stehen in docs/OFFSETS.md.
// Ist das Dumper-7-SDK lokal eingerichtet, prüft SdkChecks.cpp jeden Wert beim Bauen gegen das SDK.
namespace Game::Offsets
{
// RVAs relativ zur Image-Base der Exe.
inline constexpr uintptr_t GObjects = 0x0AA67B80;
inline constexpr uintptr_t GNames = 0x0A999B40;
inline constexpr uintptr_t GWorld = 0x0AAEA770;
inline constexpr uintptr_t ProcessEvent = 0x0160C220;
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
inline constexpr uintptr_t ItemInternalFlags = 0x04;
inline constexpr uintptr_t ItemObject = 0x08;
inline constexpr uintptr_t ItemSerialNumber = 0x10;
}

namespace UObject
{
inline constexpr uintptr_t Flags = 0x08;
inline constexpr uintptr_t InternalIndex = 0x0C; // Platz im Objekt-Array
inline constexpr uint32_t ClassDefaultObjectFlag = 0x10;
inline constexpr uintptr_t Class = 0x10;
inline constexpr uintptr_t Name = 0x18;
inline constexpr uintptr_t Outer = 0x20;
}

namespace UStruct
{
inline constexpr uintptr_t SuperStruct = 0x40;
inline constexpr uintptr_t Children = 0x48;        // UField*, Kette der UFunctions
inline constexpr uintptr_t ChildProperties = 0x50; // FField*, Kette der Felder (FProperty)
inline constexpr uintptr_t PropertiesSize = 0x58;  // int32, Größe der Instanz
}

// Engine-Grundlayouts für die Reflexion (Game/Reflection.h).
namespace UField
{
inline constexpr uintptr_t Next = 0x28;
}

namespace FField
{
inline constexpr uintptr_t Class = 0x08; // FFieldClass*
inline constexpr uintptr_t Next = 0x18;
inline constexpr uintptr_t Name = 0x20;  // FName: ComparisonIndex, dann Number
}

namespace FFieldClass
{
inline constexpr uintptr_t Name = 0x08; // FName, etwa "IntProperty"
}

namespace FProperty
{
inline constexpr uintptr_t ArrayDim = 0x30;
inline constexpr uintptr_t ElementSize = 0x34;
inline constexpr uintptr_t Offset = 0x44;
}

namespace FBoolProperty
{
inline constexpr uintptr_t FieldMask = 0x73;
}

namespace ULevel
{
inline constexpr uintptr_t Actors = 0xA0; // TArray<AActor*>
inline constexpr int32_t MaxActors = 200000;
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

// Felder von Spielklassen löst Game/Reflection.h zur Laufzeit auf, sie stehen deshalb nicht mehr hier.
}
