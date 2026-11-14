#include "Game/PatchPolicy.h"

namespace Game::Patches
{
bool ShouldRedirectTravel(NetRole Role, std::string_view Target)
{
    return Role == NetRole::Host && !Target.empty() && Target.find("MainMenu") == std::string_view::npos &&
           Target.find("listen") == std::string_view::npos;
}

size_t MergeKeepList(const uintptr_t* Keep, size_t KeepCount, const uintptr_t* Pawns, size_t PawnCount, uintptr_t* Out,
                     size_t Capacity, size_t& Added)
{
    Added = 0;
    size_t Count = 0;
    for (size_t i = 0; i < KeepCount && Count < Capacity; ++i)
        Out[Count++] = Keep[i];
    for (size_t i = 0; i < PawnCount && Count < Capacity; ++i)
    {
        bool Known = !Pawns[i];
        for (size_t j = 0; j < Count && !Known; ++j)
            Known = Out[j] == Pawns[i];
        if (Known)
            continue;
        Out[Count++] = Pawns[i];
        ++Added;
    }
    return Count;
}

size_t LocalControllerBehindFront(const bool* Local, size_t Count)
{
    if (Count == 0 || Local[0])
        return Count;
    for (size_t i = 1; i < Count; ++i)
        if (Local[i])
            return i;
    return Count;
}
}
