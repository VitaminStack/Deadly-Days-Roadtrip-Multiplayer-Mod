#include "Game/Memory.h"

#include <windows.h>

#include <cstring>
#include <mutex>
#include <unordered_set>

#include "Core/Log.h"
#include "Core/Version.h"
#include "Game/AddressMap.h"
#include "Game/Offsets.h"
#include "Game/PatchSignatures.h"

namespace Game::Memory
{
namespace
{
// Eigene Funktion ohne C++-Objekte, damit __try erlaubt ist.
bool CopyGuarded(const void* From, void* To, size_t Size)
{
    __try
    {
        std::memcpy(To, From, Size);
        return true;
    }
    __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        return false;
    }
}
}

uintptr_t ImageBase()
{
    return reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
}

uintptr_t Rva(uintptr_t ReferenceRva)
{
    const uint32_t Build = Core::ActiveBuild();
    const uint32_t Mapped = AddressMap::Map(Build, static_cast<uint32_t>(ReferenceRva));
    if (Mapped)
    {
        // Mit Signaturzeile nur, wenn das Muster an der Stelle passt (patch-signaturen). Usable protokolliert selbst.
        const uintptr_t Address = ImageBase() + Mapped;
        return Signatures::Usable(static_cast<uint32_t>(ReferenceRva), Address) ? Address : 0;
    }
    static std::mutex Lock;
    static std::unordered_set<uintptr_t> Reported;
    std::scoped_lock Guard(Lock);
    if (Reported.insert(ReferenceRva).second)
        Core::Log::Error("Adresse {:#x} fehlt für Spiel-Build {}, siehe tools/rva_map.py", ReferenceRva, Build);
    return 0;
}

bool ReadBytes(uintptr_t Address, void* Out, size_t Size)
{
    if (!Address || !Out)
        return false;
    return CopyGuarded(reinterpret_cast<const void*>(Address), Out, Size);
}

bool WriteBytes(uintptr_t Address, const void* Data, size_t Size)
{
    if (!Address || !Data)
        return false;
    return CopyGuarded(Data, reinterpret_cast<void*>(Address), Size);
}

std::optional<uintptr_t> ReadPointer(uintptr_t Address)
{
    const auto Value = Read<uintptr_t>(Address);
    if (!Value || !*Value)
        return std::nullopt;
    return Value;
}

std::string NameFromIndex(int32_t Index)
{
    if (Index < 0)
        return {};
    const auto Block = ReadPointer(Rva(Offsets::GNames) + Offsets::NameBlocks + 8 * (static_cast<uint32_t>(Index) >> 16));
    if (!Block)
        return {};
    const uintptr_t Entry = *Block + (Index & 0xFFFF) * Offsets::NameStride;
    const auto Header = Read<uint16_t>(Entry);
    // Bit 0 markiert Wide-Namen, die braucht die Mod bisher nicht.
    if (!Header || (*Header & 1))
        return {};
    std::string Name(*Header >> 6, '\0');
    if (Name.empty() || !ReadBytes(Entry + 2, Name.data(), Name.size()))
        return {};
    return Name;
}

std::string NameOf(uintptr_t Object)
{
    const auto Index = Read<int32_t>(Object + Offsets::UObject::Name);
    return Index ? NameFromIndex(*Index) : std::string{};
}
}
