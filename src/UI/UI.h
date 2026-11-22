#pragma once

#include "UI/OverlayModel.h"

// Overlay der Mod. Die Mod ruft Start einmal nach bestandener Versionsprüfung auf.
namespace UI
{
void Start();
// Explicit shutdown outside DllMain, before unloading code or destroying a backend.
void Stop();
void SetVisible(bool Visible);
OverlayModel& Overlay();
// live-protokoll: Zahl der F9-Drücke (Testermarke) seit dem letzten Aufruf, nur mit Fokus auf Spiel oder Overlay.
uint32_t TakeMarks();
}
