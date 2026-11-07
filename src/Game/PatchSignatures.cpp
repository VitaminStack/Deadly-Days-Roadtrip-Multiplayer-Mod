#include "Game/PatchSignatures.h"

#include <array>
#include <atomic>
#include <vector>

#include "Core/Log.h"
#include "Core/Signature.h"
#include "Core/Version.h"
#include "Game/Memory.h"

namespace Game::Signatures
{
namespace
{
enum class Verdict : uint8_t
{
    Unknown,
    Good,
    Bad,
};

std::array<std::atomic<Verdict>, std::size(Table)> Verdicts{};

bool Verify(const Entry& Row, uintptr_t Address)
{
    const auto Needle = Core::ParsePattern(Row.Pattern);
    if (!Needle || Address < Row.Offset)
        return false;
    std::vector<uint8_t> Bytes(Needle->size());
    if (!Memory::ReadBytes(Address - Row.Offset, Bytes.data(), Bytes.size()))
        return false;
    return Matches(Row.Pattern, Bytes);
}
}

bool Matches(std::string_view Pattern, std::span<const uint8_t> Bytes)
{
    const auto Needle = Core::ParsePattern(Pattern);
    if (!Needle || Bytes.size() < Needle->size())
        return false;
    for (size_t Index = 0; Index < Needle->size(); ++Index)
        if ((*Needle)[Index] && *(*Needle)[Index] != Bytes[Index])
            return false;
    return true;
}

bool Usable(uint32_t Reference, uintptr_t Address)
{
    const int Index = Find(Table, Reference, Core::ActiveBuild());
    if (Index < 0)
        return true;
    std::atomic<Verdict>& Slot = Verdicts[static_cast<size_t>(Index)];
    Verdict Known = Slot.load();
    if (Known == Verdict::Unknown)
    {
        const Verdict Fresh = Verify(Table[Index], Address) ? Verdict::Good : Verdict::Bad;
        if (Slot.compare_exchange_strong(Known, Fresh) && Fresh == Verdict::Bad)
            Core::Log::Error("Signatur {} ({:#x}) passt nicht an {:#x}, Adresse gesperrt, siehe tools/patch_signatures.py",
                             Table[Index].Name, Reference, Address - Memory::ImageBase());
        Known = Slot.load();
    }
    return Known == Verdict::Good;
}

void VerifyAll()
{
    size_t Checked = 0;
    size_t Bad = 0;
    for (size_t Index = 0; Index < std::size(Table); ++Index)
    {
        if (Index > 0 && Table[Index].Reference == Table[Index - 1].Reference)
            continue;
        ++Checked;
        Bad += Memory::Rva(Table[Index].Reference) == 0;
    }
    if (Bad)
        Core::Log::Warn("Signaturen: {} von {} Adressen gesperrt", Bad, Checked);
    else
        Core::Log::Info("Signaturen: {} Adressen geprüft", Checked);
}
}
