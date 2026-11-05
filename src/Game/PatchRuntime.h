#pragma once

#include <cstdint>
#include <span>

#include "Game/Patches.h"
#include "Game/WorldAdapter.h"

// Gemeinsames der Patch-Dateien. Nur innerhalb von src/Game benutzen.
namespace Game::Patches
{
// Image-Base plus RVA.
uintptr_t At(uintptr_t Rva);

// Aktuelle Welt und Rolle, auch aus Hooks heraus, also ohne Prüfung auf den Drain-Thread. Lesen ist abgesichert.
uintptr_t CurrentWorld();
NetRole CurrentRole();

// Klasse von Object heißt ClassName oder erbt davon.
bool IsA(uintptr_t Object, const char* ClassName);

// Patchlisten und Ticks je Datei.
std::span<const Patch> PlayerPatches();
std::span<const Patch> ClientPatches();
std::span<const Patch> CameraPatches();
std::span<const Patch> InventoryPatches();
std::span<const Patch> TravelPatches();
std::span<const Patch> WidgetPatches();
std::span<const Patch> HealthBarPatches();
std::span<const Patch> AirdropPatches();
std::span<const Patch> ExpPatches();
std::span<const Patch> RemoteP2Patches();
std::span<const Patch> RewardPatches();
// Host: Figur, die bei RewardChest.OpenAirdropChoices die native Auswahl bekommt (die des Hosts statt des Remote-P2).
uintptr_t RewardCharacter(uintptr_t Character);
// Client: Wahr, wenn ProcessEvent einen Server-RPC mit einer lokalen Anzeige-Instanz der Airdrop-Auswahl nicht senden soll.
bool FilterRewardEvent(uintptr_t Object, uintptr_t Function, void* Params);

// Host mit vollständiger P2-Bindung des Remote-Spielers: Coop-Subsystem, lokaler und Remote-Controller.
bool BoundRemoteP2(uintptr_t& Coop, uintptr_t& Host, uintptr_t& Remote);

void TickPlayers(NetRole Role, bool Second);
void TickCamera(NetRole Role, bool Second);
void TickWidgets(NetRole Role);
void TickProjectiles(NetRole Role, bool Second);
void TickMovement(NetRole Role, bool Second);
void TickRemoteP2(NetRole Role);

// Beobachter nach jedem ProcessEvent, aus WidgetPatches.cpp.
void NotifyEventObservers(uintptr_t Object, uintptr_t Function, void* Params);
}
