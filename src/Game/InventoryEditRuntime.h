#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "Game/InventoryEditPlan.h"

// Spielseite der lokalen Inventarbearbeitung (inventar-lokal), nur innerhalb von src/Game und nur auf dem Game Thread.
// Ablauf in src/Game/README.md#inventar-lokal-bearbeiten.
namespace Game::InventoryEdit
{
struct SlotInfo
{
    Tile At;
    uintptr_t Slot = 0, Occupier = 0;
};

std::vector<uintptr_t> ItemsOf(uintptr_t Inventory);
std::vector<SlotInfo> SlotsOf(uintptr_t Inventory);
uint32_t DataHashOf(uintptr_t Item);
// Zustand eines vorhandenen Items aus den nativen Feldern und der Belegung der Rucksackfelder.
ItemState ReadItem(uintptr_t Item, const std::vector<SlotInfo>& Slots, uint16_t Source);

// Führt Plan(Current, Target) mit den nativen Inventarfunktionen aus. Local: Server-RPCs laufen lokal (Client),
// sonst normal über ProcessEvent (Host mit Autorität). Destroy: lokal angelegte Felder, die wieder weg sollen.
bool Apply(uintptr_t Inventory, const std::vector<uintptr_t>& Items, const std::vector<ItemState>& Current, const std::vector<ItemState>& Target,
           const std::vector<Tile>& Create, const std::vector<uintptr_t>& Destroy, bool Local);

// Client: Bearbeitung beginnt vor dem Öffnen des Panels. Choices sind die lokalen Anzeige-Instanzen der Belohnung.
void BeginClient(uintptr_t Inventory, const std::vector<uintptr_t>& Choices);
// Client: Panel bearbeitet gerade lokal.
bool ClientEditing();
// Client: Bearbeitung läuft und gehört zu Inventory.
bool ClientEditingInventory(uintptr_t Inventory);
// Client, werkbank-client: UpgradeItems lief während der Bearbeitung lokal. Inputs sind die verbrauchten Instanzen, Output die
// neue lokale Instanz, Data und Rarity die Objekte der Ausgabe. Der Edit meldet die Aufwertung, der Host führt sie selbst aus.
void RecordUpgrade(const std::vector<uintptr_t>& Inputs, uintptr_t Output, uintptr_t Data, uintptr_t Rarity);
// Client: Endstand nach dem nativen Bestätigen. ChoiceItem ist die gewählte Anzeige-Instanz oder 0. Ohne Änderung nullopt,
// die Bearbeitung ist dann beendet; sonst bleibt der Ausgangsstand bis zur Antwort des Hosts gemerkt.
std::optional<Edit> FinishClient(uint64_t Session, uint64_t Revision, std::optional<uint8_t> Choice, uintptr_t ChoiceItem);
// Client: Ausgangsstand lokal wiederherstellen (Ablehnung, Abbruch) und Bearbeitung beenden.
void RevertClient(const char* Why);
// Client: Host hat übernommen, der Ausgangsstand wird nicht mehr gebraucht.
void ForgetClient();
// Client, aus ProcessEvent: Wahr, wenn der Aufruf ein Inventar-RPC der laufenden Bearbeitung war und lokal lief oder
// verworfen wurde.
bool RouteClientRpc(uintptr_t Object, uintptr_t Function, void* Params);
}
