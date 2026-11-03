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
}
