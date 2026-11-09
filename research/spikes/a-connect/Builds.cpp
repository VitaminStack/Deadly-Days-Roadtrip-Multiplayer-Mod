// Adressen je Spiel-Build. Der Quelltext des Spikes nennt RVAs des unterstuetzten Builds 26495302 (seit spiel-update).
// Auf einem anderen Build liefert Rva() kMissingRva: Die Byte-Pruefung (Expect) liest dort nichts Gueltiges und der Patch bleibt aus.
#include "Spike.h"

namespace Spike
{
namespace
{
constexpr uint32_t kReferenceStamp = 0x65BFBBFB; // 26495302

uint32_t BuildStamp()
{
    const auto* Dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(GetModuleHandleW(nullptr));
    return reinterpret_cast<const IMAGE_NT_HEADERS64*>(reinterpret_cast<const uint8_t*>(Dos) + Dos->e_lfanew)->FileHeader.TimeDateStamp;
}
}

bool IsReferenceBuild()
{
    static const bool Reference = BuildStamp() == kReferenceStamp;
    return Reference;
}

bool IsSupportedBuild()
{
    return IsReferenceBuild();
}

uintptr_t Rva(uintptr_t Reference)
{
    return IsReferenceBuild() ? Reference : kMissingRva;
}

// Importeintrag (IAT-Slot) der Exe, der gerade auf Function zeigt. Unabhaengig vom Build.
uintptr_t FindImportSlot(const void* Function)
{
    const auto* Image = reinterpret_cast<const uint8_t*>(Base);
    const auto* Nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(Image + reinterpret_cast<const IMAGE_DOS_HEADER*>(Image)->e_lfanew);
    const IMAGE_DATA_DIRECTORY& Dir = Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!Dir.VirtualAddress || !Function)
        return 0;
    for (const auto* Desc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(Image + Dir.VirtualAddress); Desc->Name; ++Desc)
        for (const auto* Thunk = reinterpret_cast<const uintptr_t*>(Image + Desc->FirstThunk); *Thunk; ++Thunk)
            if (*Thunk == reinterpret_cast<uintptr_t>(Function))
                return reinterpret_cast<uintptr_t>(Thunk) - Base;
    return 0;
}
}
