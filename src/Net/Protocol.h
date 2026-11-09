#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Explizites Little-Endian-Drahtformat, keine C++-Strukturen oder Spielzeiger.
namespace Net::Protocol
{
inline constexpr uint16_t Version = 1;
// Standard der Identität ohne erkannten Build; zur Laufzeit setzt Transport Core::ActiveBuild().
inline constexpr uint32_t GameBuild = 26187885;
inline constexpr uint32_t ModVersion = 9;
inline constexpr size_t MaxPayload = 64 * 1024;
enum class Kind : uint8_t { Hello = 1, Game = 2, Control = 3 };
struct Frame
{
    Kind Type;
    std::span<const uint8_t> Payload;
};
struct Identity
{
    uint32_t Build = GameBuild;
    uint32_t Mod = ModVersion;
    bool Host = false;
};
struct Control
{
    uint64_t Run = 0;
    uint64_t Epoch = 0;
    uint64_t Revision = 0;
    uint16_t Type = 0;
    std::vector<uint8_t> Payload;
    bool operator==(const Control&) const = default;
};
std::vector<uint8_t> Encode(Kind Type, std::span<const uint8_t> Payload);
std::optional<Frame> Decode(std::span<const uint8_t> Bytes);
std::vector<uint8_t> Hello(Identity Value);
bool Matches(std::span<const uint8_t> Bytes, Identity Local);
// Lesbarer Grund, warum eine empfangene Nachricht abgelehnt wird, für Log und Overlay. Unterscheidet eine Mod ohne dieses
// Protokoll, eine andere Protokollversion, einen anderen Spiel-Build, eine andere Mod-Version und dieselbe Rolle auf beiden
// Seiten von kaputten Daten. Bytes ist die ganze Nachricht, Local die eigene Identität.
std::string Explain(std::span<const uint8_t> Bytes, Identity Local);
std::vector<uint8_t> EncodeControl(const Control& Value);
std::optional<Control> DecodeControl(std::span<const uint8_t> Bytes);
}
