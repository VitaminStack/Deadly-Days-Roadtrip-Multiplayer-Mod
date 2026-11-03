// Opt-in integration points; all calls must run on the game/window thread.
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace Spike
{
bool RoundPhaseCommand(const std::wstring& Command);
void TickRoundPhase(HWND GameWindow);
void ObserveRoundPhaseEvent(uintptr_t Object, uintptr_t Function, void* Params);
bool RoundPhaseBlocksMessage(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);
}
