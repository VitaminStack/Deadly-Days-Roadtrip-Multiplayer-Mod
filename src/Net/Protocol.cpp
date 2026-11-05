#include "Net/Protocol.h"

namespace Net::Protocol
{
namespace
{
void Put(std::vector<uint8_t>& Bytes, uint64_t Value, unsigned Size)
{
    for (unsigned i = 0; i < Size; ++i)
        Bytes.push_back(static_cast<uint8_t>(Value >> (8 * i)));
}
uint64_t Get(std::span<const uint8_t> Bytes, size_t At, unsigned Size)
{
    uint64_t Value = 0;
    for (unsigned i = 0; i < Size; ++i)
        Value |= uint64_t(Bytes[At + i]) << (8 * i);
    return Value;
}
}

std::vector<uint8_t> Encode(Kind Type, std::span<const uint8_t> Payload)
{
    if (Payload.size() > MaxPayload || Type < Kind::Hello || Type > Kind::Control)
        return {};
    std::vector<uint8_t> Bytes{'D', 'D', 'C', 'P'};
    Put(Bytes, Version, 2);
    Put(Bytes, static_cast<uint8_t>(Type), 1);
    Put(Bytes, 0, 1);
    Put(Bytes, Payload.size(), 4);
    Bytes.insert(Bytes.end(), Payload.begin(), Payload.end());
    return Bytes;
}

std::optional<Frame> Decode(std::span<const uint8_t> Bytes)
{
    if (Bytes.size() < 12 || Bytes[0] != 'D' || Bytes[1] != 'D' || Bytes[2] != 'C' || Bytes[3] != 'P' ||
        Get(Bytes, 4, 2) != Version || Bytes[7] != 0 || Bytes[6] < 1 || Bytes[6] > 3 ||
        Bytes.size() - 12 > MaxPayload || Get(Bytes, 8, 4) != Bytes.size() - 12)
        return std::nullopt;
    return Frame{static_cast<Kind>(Bytes[6]), Bytes.subspan(12)};
}

std::vector<uint8_t> Hello(Identity Value)
{
    std::vector<uint8_t> Bytes;
    Put(Bytes, Value.Build, 4);
    Put(Bytes, Value.Mod, 4);
    Put(Bytes, Value.Host ? 1 : 0, 1);
    return Encode(Kind::Hello, Bytes);
}

bool Matches(std::span<const uint8_t> Bytes, Identity Local)
{
    return Bytes.size() == 9 && Get(Bytes, 0, 4) == Local.Build && Get(Bytes, 4, 4) == Local.Mod &&
           Bytes[8] == (Local.Host ? 0 : 1);
}

std::string Explain(std::span<const uint8_t> Bytes, Identity Local)
{
    if (Bytes.size() < 8 || Bytes[0] != 'D' || Bytes[1] != 'D' || Bytes[2] != 'C' || Bytes[3] != 'P')
        return "Mitspieler sendet ohne Mod-Protokoll, vermutlich eine aeltere Mod-Version. Beide denselben Stand installieren";
    if (const auto Theirs = Get(Bytes, 4, 2); Theirs != Version)
        return "Mitspieler hat Protokollversion " + std::to_string(Theirs) + ", du " + std::to_string(Version) +
               ". Beide denselben Mod-Stand installieren";
    const auto Frame = Decode(Bytes);
    if (!Frame)
        return "Ungueltige Nachricht vom Mitspieler (beschaedigter Rahmen)";
    if (Frame->Type != Kind::Hello || Frame->Payload.size() != 9)
        return "Ungueltige oder unerwartete Kontrollnachricht vom Mitspieler";
    const auto Build = Get(Frame->Payload, 0, 4), Mod = Get(Frame->Payload, 4, 4);
    if (Build != Local.Build)
        return "Mitspieler hat Spiel-Build " + std::to_string(Build) + ", du " + std::to_string(Local.Build);
    if (Mod != Local.Mod)
        return "Mitspieler hat Mod-Version " + std::to_string(Mod) + ", du " + std::to_string(Local.Mod) +
               ". Beide denselben Mod-Stand installieren";
    if ((Frame->Payload[8] == 1) == Local.Host)
        return Local.Host ? "Beide Seiten sind Host" : "Beide Seiten sind Client";
    return "Ungueltige oder unerwartete Kontrollnachricht vom Mitspieler";
}

std::vector<uint8_t> EncodeControl(const Control& Value)
{
    if (!Value.Run || !Value.Epoch || !Value.Revision || !Value.Type || Value.Payload.size() > MaxPayload - 26)
        return {};
    std::vector<uint8_t> Bytes;
    Put(Bytes, Value.Run, 8);
    Put(Bytes, Value.Epoch, 8);
    Put(Bytes, Value.Revision, 8);
    Put(Bytes, Value.Type, 2);
    Bytes.insert(Bytes.end(), Value.Payload.begin(), Value.Payload.end());
    return Encode(Kind::Control, Bytes);
}

std::optional<Control> DecodeControl(std::span<const uint8_t> Bytes)
{
    if (Bytes.size() < 26 || Bytes.size() > MaxPayload)
        return std::nullopt;
    Control Value{Get(Bytes, 0, 8), Get(Bytes, 8, 8), Get(Bytes, 16, 8), static_cast<uint16_t>(Get(Bytes, 24, 2))};
    if (!Value.Run || !Value.Epoch || !Value.Revision || !Value.Type)
        return std::nullopt;
    Value.Payload.assign(Bytes.begin() + 26, Bytes.end());
    return Value;
}
}
