#include "Net/NetSim.h"

#include <algorithm>
#include <cwchar>
#include <format>

#include "Core/Log.h"

namespace Net
{
std::optional<NetSimProfile> ParseNetSim(std::wstring_view CommandLine)
{
    constexpr std::wstring_view Key = L"-ddcoop_netsim=";
    const size_t At = CommandLine.find(Key);
    if (At == std::wstring_view::npos)
        return std::nullopt;
    std::wstring_view Value = CommandLine.substr(At + Key.size());
    Value = Value.substr(0, Value.find_first_of(L" \t\""));
    if (Value.empty() || Value.find_first_not_of(L"0123456789.,") != std::wstring_view::npos)
        return std::nullopt;

    double Parts[3] = {};
    size_t Count = 0;
    while (!Value.empty())
    {
        if (Count == 3)
            return std::nullopt;
        const size_t Comma = Value.find(L',');
        const std::wstring Part(Value.substr(0, Comma));
        if (Part.empty() || std::count(Part.begin(), Part.end(), L'.') > 1)
            return std::nullopt;
        Parts[Count++] = std::wcstod(Part.c_str(), nullptr);
        if (Comma == std::wstring_view::npos)
            break;
        Value = Value.substr(Comma + 1);
        if (Value.empty())
            return std::nullopt;
    }
    if (Parts[0] > 2000 || Parts[1] > 2000 || Parts[2] > 50)
        return std::nullopt;
    NetSimProfile Profile;
    Profile.PingMs = static_cast<unsigned>(Parts[0]);
    Profile.JitterMs = static_cast<unsigned>(Parts[1]);
    Profile.LossPercent = Parts[2];
    return Profile;
}

NetSimProfile NetSimFromCommandLine(std::wstring_view CommandLine)
{
    if (const auto Profile = ParseNetSim(CommandLine))
        return *Profile;
    if (CommandLine.find(L"-ddcoop_netsim=") != std::wstring_view::npos)
        Core::Log::Warn("Netzsimulation: Schalter ungültig, erwartet -ddcoop_netsim=<Ping>,<Schwankung>,<Verlust>");
    return {};
}

std::string Describe(const NetSimProfile& Profile)
{
    return std::format("Ping {} ms ±{} ms, Verlust {:.1f} % je Richtung", Profile.PingMs, Profile.JitterMs, Profile.LossPercent);
}

std::string Describe(const NetSimStats& Stats)
{
    const uint64_t Total = Stats.Delayed + Stats.Lost;
    const double Lost = Total ? 100.0 * static_cast<double>(Stats.Lost) / static_cast<double>(Total) : 0.0;
    const uint64_t Mean = Stats.Delayed ? Stats.DelaySumMs / Stats.Delayed : 0;
    return std::format("{} Pakete verzögert, Mittel {} ms ({} bis {} ms), {} verloren ({:.1f} %)", Stats.Delayed, Mean,
                       Stats.MinMs, Stats.MaxMs, Stats.Lost, Lost);
}

NetSimQueue::NetSimQueue(NetSimProfile ProfileIn, uint32_t Seed) : Profile(ProfileIn), Random(Seed)
{
}

bool NetSimQueue::Push(uint64_t NowMs, std::span<const uint8_t> Bytes, bool Reliable)
{
    const double Half = Profile.PingMs / 2.0;
    if (Reliable)
    {
        // Zuverlässige Nachrichten behalten ihre Reihenfolge, wie bei Steam.
        const uint64_t Release = std::max(LastReliable, NowMs + static_cast<uint64_t>(Half));
        LastReliable = Release;
        Waiting.push({Release, NextOrder++, true, {Bytes.begin(), Bytes.end()}});
        return true;
    }
    if (Profile.LossPercent > 0 && std::uniform_real_distribution<double>(0.0, 100.0)(Random) < Profile.LossPercent)
    {
        ++Current.Lost;
        return false;
    }
    const double Spread = Profile.JitterMs / 2.0;
    const double Delay = std::max(0.0, Half + (Spread > 0 ? std::uniform_real_distribution<double>(-Spread, Spread)(Random) : 0.0));
    const unsigned Ms = static_cast<unsigned>(Delay + 0.5);
    Current.MinMs = Current.Delayed ? std::min(Current.MinMs, Ms) : Ms;
    Current.MaxMs = std::max(Current.MaxMs, Ms);
    ++Current.Delayed;
    Current.DelaySumMs += Ms;
    Waiting.push({NowMs + Ms, NextOrder++, false, {Bytes.begin(), Bytes.end()}});
    return true;
}

void NetSimQueue::PopReady(uint64_t NowMs, const std::function<void(std::span<const uint8_t>, bool)>& Send)
{
    while (!Waiting.empty() && Waiting.top().Release <= NowMs)
    {
        // top() ist const, die Bytes werden vor pop() weitergegeben.
        const Item& Next = Waiting.top();
        Send(Next.Bytes, Next.Reliable);
        Waiting.pop();
    }
}

NetSimStats NetSimQueue::TakeStats()
{
    NetSimStats Result = Current;
    Current = {};
    return Result;
}
}
