// Adressen je Spiel-Build. Der Quelltext des Spikes nennt RVAs des Referenz-Builds 26187885; fuer 26495302 rechnet Rva() sie ueber
// die Tabelle der Mod (src/Game/RvaMap26495302.inc, erzeugt von tools/rva_map.py) und die wenigen eigenen Eintraege unten um.
// Was die Tabelle nicht kennt, liefert kMissingRva: Die Byte-Pruefung (Expect) liest dort nichts Gueltiges und der Patch bleibt aus.
#include "Spike.h"

#include <algorithm>
#include <iterator>

namespace Spike
{
namespace
{
constexpr uint32_t kReferenceStamp = 0x32C34474; // 26187885
constexpr uint32_t kStamp26495302 = 0x65BFBBFB;

struct Entry
{
    uint32_t Reference;
    uint32_t Mapped;
};

// Aus der Mod uebernommen, aufsteigend nach Reference.
const Entry ModTable[] = {
#include "../../../src/Game/RvaMap26495302.inc"
};

// Nur der Spike braucht diese Stellen (tools/rva_map.py, eindeutig bzw. global). Aufsteigend nach Reference.
const Entry SpikeTable[] = {
    {0x02D09B90, 0x02D09D90}, // SerializeNewActor
    {0x02D0A426, 0x02D0A626}, // NewActorArchetypeCheck
    {0x04B7D7D0, 0x04B7D9D0}, // UpdateTag
    {0x0A993AC0, 0x0A999B40}, // GNames
    {0x0AA61B00, 0x0AA67B80}, // GObjects
    {0x0AAE46F0, 0x0AAEA770}, // GWorld
    {0x0AC61FE8, 0x0AC680D8}, // Tag State.Invincible
};

template <size_t N> const Entry* Find(const Entry (&Table)[N], uintptr_t Reference)
{
    const Entry* End = std::end(Table);
    const Entry* Found = std::lower_bound(std::begin(Table), End, Reference, [](const Entry& E, uintptr_t R) { return E.Reference < R; });
    return Found != End && Found->Reference == Reference ? Found : nullptr;
}

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
    return IsReferenceBuild() || BuildStamp() == kStamp26495302;
}

uintptr_t Rva(uintptr_t Reference)
{
    if (IsReferenceBuild())
        return Reference;
    if (BuildStamp() != kStamp26495302)
        return kMissingRva;
    if (const Entry* Hit = Find(SpikeTable, Reference))
        return Hit->Mapped;
    if (const Entry* Hit = Find(ModTable, Reference))
        return Hit->Mapped;
    return kMissingRva;
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
