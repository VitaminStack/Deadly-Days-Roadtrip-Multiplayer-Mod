#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

// Hält Mod-Runs aus Steam-Erfolgen, -Statistiken und -Bestenlisten heraus. Siehe src/Core/README.md.
namespace Core::SteamStats
{
// Sperrt im eigenen Prozess die schreibenden Funktionen von ISteamUserStats: Erfolge setzen und Fortschritt melden,
// Statistiken setzen und speichern, Bestenlisten-Einträge hochladen. Lesen bleibt erlaubt. Ist Steam beim Aufruf noch
// nicht bereit, versucht es ein Hintergrund-Thread bis zu 120 s lang weiter. Braucht Hooks::Initialize.
void Block();

// Wie oft seit dem Start ein Schreibversuch abgewiesen wurde.
int BlockedCalls();

// Liest aus dem Code einer flachen Steam-Funktion (SteamAPI_ISteamUserStats_*) den Abstand des Methodenzeigers in der
// vtable: mov rax,[rcx] und dann jmp [rax+X] oder mov r10,[rax+X]; jmp r10. Sonst nichts.
std::optional<uint32_t> VtableOffsetFromThunk(const uint8_t* Code, size_t Size);
}
