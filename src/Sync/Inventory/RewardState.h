#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Airdrop-Auswahl über den Kontrollkanal (airdrop-auswahl). Host schickt das Bild der nativen Koop-Reward-Sitzung,
// Client antwortet nur mit seiner Wahl. Little-Endian, Namen UTF-8 und begrenzt.
namespace Sync::Reward
{
inline constexpr uint16_t OfferType = 0x300, ChoiceType = 0x301;
inline constexpr size_t MaxChoices = 4, MaxName = 120;

struct Offer
{
    uint64_t Session = 0; // 0 = keine offene Sitzung, Client schließt die Auswahl
    uint64_t Revision = 0;
    int32_t Level = 0;
    bool ClientClaimed = false, HostClaimed = false, HostWaiting = false;
    std::vector<std::string> Choices;
    bool operator==(const Offer&) const = default;
};

struct Choice
{
    uint64_t Session = 0, Revision = 0;
    uint8_t Index = 0;
    bool operator==(const Choice&) const = default;
};

namespace Detail
{
inline void Put(std::vector<uint8_t>& Out, uint64_t Value, unsigned Bytes)
{
    for (unsigned i = 0; i < Bytes; ++i)
        Out.push_back(static_cast<uint8_t>(Value >> (8 * i)));
}
inline uint64_t Get(std::span<const uint8_t> In, size_t At, unsigned Bytes)
{
    uint64_t Value = 0;
    for (unsigned i = 0; i < Bytes; ++i)
        Value |= uint64_t(In[At + i]) << (8 * i);
    return Value;
}
}

inline std::string Clip(std::string Name)
{
    if (Name.size() <= MaxName)
        return Name;
    size_t End = MaxName;
    while (End && (static_cast<uint8_t>(Name[End]) & 0xC0) == 0x80) // nicht mitten in einem UTF-8-Zeichen trennen
        --End;
    Name.resize(End);
    return Name;
}

inline bool Valid(const Offer& Value)
{
    if (Value.Choices.size() > MaxChoices)
        return false;
    for (const auto& Name : Value.Choices)
        if (Name.size() > MaxName)
            return false;
    return Value.Session ? Value.Revision != 0 : Value.Choices.empty();
}

inline std::vector<uint8_t> Encode(const Offer& Value)
{
    if (!Valid(Value))
        return {};
    std::vector<uint8_t> Out;
    Detail::Put(Out, Value.Session, 8);
    Detail::Put(Out, Value.Revision, 8);
    Detail::Put(Out, static_cast<uint32_t>(Value.Level), 4);
    Out.push_back(static_cast<uint8_t>((Value.ClientClaimed ? 1 : 0) | (Value.HostClaimed ? 2 : 0) | (Value.HostWaiting ? 4 : 0)));
    Out.push_back(static_cast<uint8_t>(Value.Choices.size()));
    for (const auto& Name : Value.Choices)
    {
        Out.push_back(static_cast<uint8_t>(Name.size()));
        Out.insert(Out.end(), Name.begin(), Name.end());
    }
    return Out;
}

inline std::optional<Offer> DecodeOffer(std::span<const uint8_t> In)
{
    if (In.size() < 22 || In[20] > 7 || In[21] > MaxChoices)
        return std::nullopt;
    Offer Value;
    Value.Session = Detail::Get(In, 0, 8);
    Value.Revision = Detail::Get(In, 8, 8);
    Value.Level = static_cast<int32_t>(Detail::Get(In, 16, 4));
    Value.ClientClaimed = In[20] & 1;
    Value.HostClaimed = In[20] & 2;
    Value.HostWaiting = In[20] & 4;
    size_t At = 22;
    for (uint8_t i = 0; i < In[21]; ++i)
    {
        if (At >= In.size() || In[At] > MaxName || At + 1 + In[At] > In.size())
            return std::nullopt;
        Value.Choices.emplace_back(reinterpret_cast<const char*>(In.data() + At + 1), In[At]);
        At += 1 + In[At];
    }
    if (At != In.size() || !Valid(Value))
        return std::nullopt;
    return Value;
}

inline std::vector<uint8_t> Encode(const Choice& Value)
{
    if (!Value.Session || !Value.Revision || Value.Index >= MaxChoices)
        return {};
    std::vector<uint8_t> Out;
    Detail::Put(Out, Value.Session, 8);
    Detail::Put(Out, Value.Revision, 8);
    Out.push_back(Value.Index);
    return Out;
}

inline std::optional<Choice> DecodeChoice(std::span<const uint8_t> In)
{
    if (In.size() != 17)
        return std::nullopt;
    Choice Value{Detail::Get(In, 0, 8), Detail::Get(In, 8, 8), In[16]};
    if (!Value.Session || !Value.Revision || Value.Index >= MaxChoices)
        return std::nullopt;
    return Value;
}
}
