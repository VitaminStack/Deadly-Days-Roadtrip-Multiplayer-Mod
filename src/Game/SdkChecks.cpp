// Prüft Offsets.h beim Bauen gegen das Dumper-7-SDK. Ohne lokales SDK (etwa in der CI) bleibt die Datei leer.
// Schlägt hier ein static_assert fehl, passt Offsets.h nicht mehr zum SDK, siehe CONTRIBUTING.md#spiel-updates.
#ifdef DDCOOP_WITH_SDK

#include <cstddef>

#include "Game/Offsets.h"

#pragma warning(push, 0)
#include <SDK/Engine_classes.hpp>
#include <SDK/DDSurvivors_classes.hpp>
#pragma warning(pop)

namespace Game::Offsets
{
static_assert(GObjects == static_cast<uintptr_t>(SDK::Offsets::GObjects));
static_assert(GNames == static_cast<uintptr_t>(SDK::Offsets::GNames));
static_assert(GWorld == static_cast<uintptr_t>(SDK::Offsets::GWorld));
static_assert(ProcessEvent == static_cast<uintptr_t>(SDK::Offsets::ProcessEvent));
static_assert(ProcessEventIndex == SDK::Offsets::ProcessEventIdx);

static_assert(ObjectArray::NumElements == offsetof(SDK::TUObjectArray, NumElements));
static_assert(ObjectArray::ElementsPerChunk == SDK::TUObjectArray::ElementsPerChunk);
static_assert(ObjectArray::ItemSize == sizeof(SDK::FUObjectItem));
static_assert(ObjectArray::ItemObject == offsetof(SDK::FUObjectItem, Object));

static_assert(UObject::Flags == offsetof(SDK::UObject, Flags));
static_assert(UObject::ClassDefaultObjectFlag == static_cast<uint32_t>(SDK::EObjectFlags::ClassDefaultObject));
static_assert(UObject::Class == offsetof(SDK::UObject, Class));
static_assert(UObject::Name == offsetof(SDK::UObject, Name));
static_assert(UObject::Outer == offsetof(SDK::UObject, Outer));

static_assert(UStruct::SuperStruct == offsetof(SDK::UStruct, SuperStruct));
static_assert(UStruct::Children == offsetof(SDK::UStruct, Children));
static_assert(UStruct::ChildProperties == offsetof(SDK::UStruct, ChildProperties));
static_assert(UStruct::PropertiesSize == offsetof(SDK::UStruct, Size));

static_assert(UField::Next == offsetof(SDK::UField, Next));
static_assert(FField::Class == offsetof(SDK::FField, ClassPrivate));
static_assert(FField::Next == offsetof(SDK::FField, Next));
static_assert(FField::Name == offsetof(SDK::FField, Name));
static_assert(FFieldClass::Name == offsetof(SDK::FFieldClass, Name));
static_assert(FProperty::ArrayDim == offsetof(SDK::FProperty, ArrayDim));
static_assert(FProperty::ElementSize == offsetof(SDK::FProperty, ElementSize));
static_assert(FProperty::Offset == offsetof(SDK::FProperty, Offset));
static_assert(FBoolProperty::FieldMask == offsetof(SDK::FBoolProperty, FieldMask));

static_assert(UWorld::PersistentLevel == offsetof(SDK::UWorld, PersistentLevel));
static_assert(UWorld::NetDriver == offsetof(SDK::UWorld, NetDriver));
static_assert(UWorld::AuthorityGameMode == offsetof(SDK::UWorld, AuthorityGameMode));
static_assert(UWorld::GameState == offsetof(SDK::UWorld, GameState));
static_assert(UWorld::OwningGameInstance == offsetof(SDK::UWorld, OwningGameInstance));

static_assert(UNetDriver::ServerConnection == offsetof(SDK::UNetDriver, ServerConnection));
static_assert(UNetDriver::ClientConnections == offsetof(SDK::UNetDriver, ClientConnections));

static_assert(AGameStateBase::PlayerArray == offsetof(SDK::AGameStateBase, PlayerArray));

}

#endif
