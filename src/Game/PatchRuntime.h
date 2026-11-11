#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "Game/Patches.h"
#include "Game/WorldAdapter.h"

// Gemeinsames der Patch-Dateien. Nur innerhalb von src/Game benutzen.
namespace Game::Patches
{
// Absolute Adresse zu einer RVA des Referenz-Builds, für den laufenden Build umgerechnet. 0, wenn unbekannt.
uintptr_t At(uintptr_t Rva);
// Wie At(Rva) plus Offset, etwa ein vtable-Eintrag. 0, wenn Rva unbekannt ist.
uintptr_t At(uintptr_t Rva, uintptr_t Offset);

constexpr uint8_t CallRel32 = 0xE8;
constexpr uint8_t JumpRel32 = 0xE9;
// Wahr, wenn an Site ein call/jmp rel32 (Opcode) auf At(TargetRva) steht. Statt fester rel32-Bytes in Code::Expect, die sich
// mit jedem Spiel-Build verschieben. Protokolliert sonst wie Code::Expect.
bool ExpectBranch(uintptr_t Site, uint8_t Opcode, uintptr_t TargetRva, std::string_view Where);

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
std::span<const Patch> MapPatches();
std::span<const Patch> LocalDevicePatches();
std::span<const Patch> CharacterPatches();
// Host: Figur, die bei RewardChest.OpenAirdropChoices die native Auswahl bekommt (die des Hosts statt des Remote-P2).
uintptr_t RewardCharacter(uintptr_t Character);
// Client: Wahr, wenn ProcessEvent einen Server-RPC mit einer lokalen Anzeige-Instanz der Airdrop-Auswahl nicht senden soll.
bool FilterRewardEvent(uintptr_t Object, uintptr_t Function, void* Params);

// Host mit vollständiger P2-Bindung des Remote-Spielers: Coop-Subsystem, lokaler und Remote-Controller.
bool BoundRemoteP2(uintptr_t& Coop, uintptr_t& Host, uintptr_t& Remote);
// Host, aus ProcessEvent: Wahr, wenn das Ereignis Betreten oder Verlassen eines MetaBase-POI durch die Remote-Figur ist und
// nicht laufen soll (charakter-wahl).
bool FilterRemotePoiOverlap(uintptr_t Function, void* Params);

void TickPlayers(NetRole Role, bool Second);
void TickCamera(NetRole Role, bool Second);
// opt-busbalken, Client: Ladebalken der Busabfahrt aus replizierten Daten.
void TickBusBar(NetRole Role);
// opt-lootbalken, Client: Fortschrittsbalken der Lootstellen aus dem replizierten Wert.
void TickLootBar(NetRole Role);
// opt-blut, Client: Blut und Fleisch beim Tod eines Gegners aus dem nativen Multi_OnDie.
void TickBlood(NetRole Role);
// opt-blut, Client: Weitere Anzeige beim Gegnertod (etwa opt-koerperteile) ohne eigenen ProcessEvent-Beobachter. Der Aufruf kommt
// nach den nativen Tod-Effekten, auf dem Game Thread, nur fuer lebende Gegner. Zwei Plaetze.
using EnemyDeathListener = void (*)(uintptr_t Enemy);
bool AddEnemyDeathListener(EnemyDeathListener Listener);
void TickWidgets(NetRole Role);
void TickProjectiles(NetRole Role, bool Second);
void TickMovement(NetRole Role, bool Second);
void TickRemoteP2(NetRole Role);
void TickMap(NetRole Role, bool Second);
void TickCharacters(NetRole Role, bool Second);

// rundenende-client, aus MapPatches.cpp: Host merkt sich sein Rundenende in der aktuellen Welt, run-ende: Lost bei einer Niederlage.
void NoteHostRoundEnd(bool Lost = false);
// run-ende, Host, aus MapPatches.cpp: Das gemerkte Rundenende der aktuellen Welt ist der Sieg am letzten Tag.
void NoteHostRunWon();
// Client, aus ClientPatches.cpp: lokales FinishMission wie beim Bus, damit Rundenübersicht, Lager und Routenkarte beim
// Client selbst aufgehen. Falsch ohne Missions-GameState. Nur Game Thread.
bool FollowHostRoundEnd();
// run-ende, Client, aus ClientPatches.cpp: Schaltet die offene Rundenübersicht auf den nativen Bildschirm des Run-Endes um. Bei einer
// Niederlage blendet er ihre Knöpfe aus, der Host entscheidet die Rückkehr. Beim Sieg bleiben sie, Fortsetzen führt wie bisher ins
// Lager, falls der Host den Endlosmodus startet. Falsch, solange keine Übersicht offen ist. Nur Game Thread.
bool ShowRunEndPanel(bool Won);
// run-ende, Client: Das Spiel zeigt die Knöpfe der Übersicht nach ihrer Animation wieder an, darum solange erneut ausblenden.
void KeepRunLossButtonsHidden();

// Diagnose für inventar-transaktion: Mit der Umgebungsvariable DDCOOP_TRACE_INVENTORY=1 protokolliert jeder Aufruf einer
// Inventar-, Item-, Rucksackfeld- oder Inventar-Panel-Funktion eine Logzeile. Sonst ohne Wirkung.
void TraceInventoryEvent(uintptr_t Object, uintptr_t Function, void* Params);
// Host: Wahr, wenn die Aktivierung eines Client-Dodges auf seine gemeldete Richtung wartet; dann nicht weiterreichen.
bool PrepareRemoteDash(uintptr_t Object, uintptr_t Function);

// Server-RPC beim Client lokal ausführen statt senden (inventar-lokal), aus WidgetPatches.cpp. Nur Game Thread.
void ProcessEventLocal(uintptr_t Object, uintptr_t Function, void* Params);

// Beobachter nach jedem ProcessEvent, aus WidgetPatches.cpp.
void NotifyEventObservers(uintptr_t Object, uintptr_t Function, void* Params);
}
