#pragma once
#include <cstdint>
#include <optional>
#include <vector>

namespace Game
{
struct NetworkObject { uint64_t Guid; uintptr_t Object; };
// Nur Game Thread. Originale UE-NetGUIDs, niemals Prozess-/FName-Indizes uebertragen.
// nullopt = Cache unlesbar/inkonsistent/zu gross, nicht "keine replizierten Objekte".
std::optional<std::vector<NetworkObject>> ReadNetworkObjects();
uint64_t NetworkGuidFor(uintptr_t Object);
}
