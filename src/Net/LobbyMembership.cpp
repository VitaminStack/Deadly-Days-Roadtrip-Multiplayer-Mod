#include "Net/LobbyMembership.h"

#include <algorithm>

namespace Net
{
std::string_view ToString(LobbyListState State)
{
    switch (State)
    {
    case LobbyListState::Valid:
        return "gültig";
    case LobbyListState::Empty:
        return "leer";
    case LobbyListState::OwnMissing:
        return "eigenes Konto fehlt";
    case LobbyListState::Incomplete:
        return "unvollständig";
    }
    return "unbekannt";
}

LobbyListState ClassifyLobbyList(const std::vector<uint64_t>& Members, int ReportedCount, uint64_t Own)
{
    if (Members.empty())
        return LobbyListState::Empty;
    if (!Own || std::find(Members.begin(), Members.end(), Own) == Members.end())
        return LobbyListState::OwnMissing;
    if (ReportedCount != static_cast<int>(Members.size()))
        return LobbyListState::Incomplete;
    return LobbyListState::Valid;
}

namespace
{
MemberVerdict Current(const MemberWatch& Watch)
{
    if (Watch.Paused)
        return MemberVerdict::Paused;
    return Watch.Misses ? MemberVerdict::Missing : MemberVerdict::Present;
}
}

MemberVerdict Judge(MemberWatch& Watch, const MembershipRules& Rules, uint64_t Sequence, LobbyListState State, bool Contains,
                    uint64_t NowMs)
{
    if (Sequence == Watch.Sequence)
        return Current(Watch);
    const uint64_t Step = Watch.SampleMs && NowMs > Watch.SampleMs
                              ? std::min<uint64_t>(NowMs - Watch.SampleMs, Rules.MaxStepMs)
                              : 0;
    Watch.Sequence = Sequence;
    Watch.SampleMs = NowMs;
    if (State != LobbyListState::Valid)
    {
        Watch.Paused = true;
        return MemberVerdict::Paused;
    }
    // Die Zeit seit einer unbrauchbaren Liste zählt nicht.
    const bool WasPaused = Watch.Paused;
    Watch.Paused = false;
    if (Contains)
    {
        Watch.Misses = 0;
        Watch.MissingMs = 0;
        return MemberVerdict::Present;
    }
    if (Watch.Misses && !WasPaused)
        Watch.MissingMs += Step;
    ++Watch.Misses;
    if (Watch.Misses >= Rules.MinMisses && Watch.MissingMs >= Rules.RevokeAfterMs)
        return MemberVerdict::Revoke;
    return MemberVerdict::Missing;
}
}
