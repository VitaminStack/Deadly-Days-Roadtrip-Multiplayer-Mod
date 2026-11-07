#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// Testkanal der Mod: Textprotokoll der lokalen Pipe, nur mit Schalter -ddcoop_control. Eine Zeile Anfrage, eine Zeile
// JSON-Antwort. Reine Text-Ebene ohne Windows und Spiel, damit sie ohne Spiel testbar ist. Siehe src/Mod/Control.cpp.
//
//   state                 Zustand als JSON (Welt, Rolle, Spieler, Sitzung, Overlay)
//   lobby create          Lobby erstellen (wie der Knopf im Overlay)
//   lobby join <id>       Lobby beitreten
//   lobby invite          Freunde einladen
//   lobby leave           Sitzung verlassen
//   console <text>        Konsolenbefehl des Spiels
//   travel <Karte>        servertravel <Karte>?listen, nur Host
namespace Net::Control
{
enum class CommandKind
{
    State,
    LobbyCreate,
    LobbyJoin,
    LobbyInvite,
    LobbyLeave,
    Console,
    Travel,
};

struct Command
{
    CommandKind Kind = CommandKind::State;
    uint64_t Lobby = 0;
    // Konsolentext oder Kartenname.
    std::string Text;
};

// Längste erlaubte Anfragezeile in Bytes, ohne Zeilenende.
inline constexpr size_t kMaxLine = 512;

// Zerlegt eine Zeile. Bei Fehler nullopt und Error mit einer kurzen Begründung.
std::optional<Command> ParseCommand(std::string_view Line, std::string* Error = nullptr);

// Kartenname für travel: Buchstaben, Ziffern, Unterstrich, Schrägstrich, Punkt. Kein "..", kein "?" und kein Leerzeichen,
// damit der Befehl keine weiteren Optionen oder Konsolenbefehle einschleust.
bool IsValidMapName(std::string_view Name);

// Konsolenbefehl für travel, etwa "servertravel /Game/Maps/DDDynamic_Map?listen".
std::string TravelConsoleCommand(std::string_view Map);

// JSON-Werte. Text wird maskiert (Anführungszeichen, Backslash, Steuerzeichen).
std::string JsonString(std::string_view Text);
std::string OkReply(std::string_view Fields = {});
std::string ErrorReply(std::string_view Message);
}
