#include "Core/Hooks.h"

#include <MinHook.h>

#include "Core/Log.h"

namespace Core::Hooks
{
bool Initialize()
{
    const MH_STATUS Status = MH_Initialize();
    if (Status == MH_OK || Status == MH_ERROR_ALREADY_INITIALIZED)
        return true;
    Log::Error("Hooks: Initialisierung fehlgeschlagen: {}", MH_StatusToString(Status));
    return false;
}

void Shutdown()
{
    const MH_STATUS Status = MH_Uninitialize();
    if (Status != MH_OK && Status != MH_ERROR_NOT_INITIALIZED)
        Log::Error("Hooks: Beenden fehlgeschlagen: {}", MH_StatusToString(Status));
}

bool Install(void* Target, void* Detour, void** Original)
{
    MH_STATUS Status = MH_CreateHook(Target, Detour, Original);
    if (Status != MH_OK)
    {
        Log::Error("Hooks: Anlegen bei {} fehlgeschlagen: {}", Target, MH_StatusToString(Status));
        return false;
    }
    Status = MH_EnableHook(Target);
    if (Status != MH_OK)
    {
        Log::Error("Hooks: Aktivieren bei {} fehlgeschlagen: {}", Target, MH_StatusToString(Status));
        MH_RemoveHook(Target);
        return false;
    }
    return true;
}

bool Remove(void* Target)
{
    const MH_STATUS Status = MH_RemoveHook(Target);
    if (Status != MH_OK)
        Log::Error("Hooks: Entfernen bei {} fehlgeschlagen: {}", Target, MH_StatusToString(Status));
    return Status == MH_OK;
}
}
