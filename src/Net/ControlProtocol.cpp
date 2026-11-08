#include "Net/ControlProtocol.h"

#include <charconv>
#include <format>

namespace Net::Control
{
namespace
{
std::string_view Trim(std::string_view Text)
{
    while (!Text.empty() && (Text.front() == ' ' || Text.front() == '\t'))
        Text.remove_prefix(1);
    while (!Text.empty() && (Text.back() == ' ' || Text.back() == '\t' || Text.back() == '\r' || Text.back() == '\n'))
        Text.remove_suffix(1);
    return Text;
}

// Erstes Wort und Rest hinter den Leerzeichen.
std::pair<std::string_view, std::string_view> SplitWord(std::string_view Text)
{
    const auto End = Text.find_first_of(" \t");
    if (End == std::string_view::npos)
        return {Text, {}};
    return {Text.substr(0, End), Trim(Text.substr(End))};
}

std::nullopt_t Fail(std::string* Error, std::string_view Message)
{
    if (Error)
        *Error = Message;
    return std::nullopt;
}
}

bool IsValidMapName(std::string_view Name)
{
    if (Name.empty() || Name.size() > 200 || Name.find("..") != std::string_view::npos)
        return false;
    for (const char Character : Name)
    {
        const bool Allowed = (Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z') ||
                             (Character >= '0' && Character <= '9') || Character == '_' || Character == '/' || Character == '.';
        if (!Allowed)
            return false;
    }
    return true;
}

std::string TravelConsoleCommand(std::string_view Map)
{
    return std::format("servertravel {}?listen", Map);
}

std::optional<Command> ParseCommand(std::string_view Line, std::string* Error)
{
    const auto Text = Trim(Line);
    if (Text.empty())
        return Fail(Error, "leere Anfrage");
    if (Text.size() > kMaxLine)
        return Fail(Error, "Anfrage zu lang");
    for (const char Character : Text)
        if (static_cast<unsigned char>(Character) < 0x20 || static_cast<unsigned char>(Character) >= 0x7F)
            return Fail(Error, "Anfrage enthält Steuerzeichen oder Nicht-ASCII");

    const auto [Word, Rest] = SplitWord(Text);
    Command Result;
    if (Word == "state")
    {
        if (!Rest.empty())
            return Fail(Error, "state hat keine Argumente");
        return Result;
    }
    if (Word == "day")
    {
        if (!Rest.empty())
            return Fail(Error, "day hat keine Argumente");
        Result.Kind = CommandKind::Day;
        return Result;
    }
    if (Word == "console")
    {
        if (Rest.empty())
            return Fail(Error, "console braucht einen Befehl");
        Result.Kind = CommandKind::Console;
        Result.Text = std::string(Rest);
        return Result;
    }
    if (Word == "travel")
    {
        if (!IsValidMapName(Rest))
            return Fail(Error, "travel braucht einen gültigen Kartennamen");
        Result.Kind = CommandKind::Travel;
        Result.Text = std::string(Rest);
        return Result;
    }
    if (Word == "lobby")
    {
        const auto [Action, Argument] = SplitWord(Rest);
        if (Action == "create" || Action == "invite" || Action == "leave")
        {
            if (!Argument.empty())
                return Fail(Error, "lobby create, invite und leave haben keine Argumente");
            Result.Kind = Action == "create" ? CommandKind::LobbyCreate : Action == "invite" ? CommandKind::LobbyInvite : CommandKind::LobbyLeave;
            return Result;
        }
        if (Action == "join")
        {
            uint64_t Id = 0;
            const auto [End, Code] = std::from_chars(Argument.data(), Argument.data() + Argument.size(), Id);
            if (Argument.empty() || Code != std::errc() || End != Argument.data() + Argument.size() || Id == 0)
                return Fail(Error, "lobby join braucht eine Lobby-ID");
            Result.Kind = CommandKind::LobbyJoin;
            Result.Lobby = Id;
            return Result;
        }
        return Fail(Error, "lobby kennt create, join, invite und leave");
    }
    return Fail(Error, "unbekannter Befehl");
}

std::string JsonString(std::string_view Text)
{
    std::string Result = "\"";
    for (const char Character : Text)
    {
        switch (Character)
        {
        case '"': Result += "\\\""; break;
        case '\\': Result += "\\\\"; break;
        case '\n': Result += "\\n"; break;
        case '\r': Result += "\\r"; break;
        case '\t': Result += "\\t"; break;
        default:
            if (static_cast<unsigned char>(Character) < 0x20)
                Result += std::format("\\u{:04x}", static_cast<unsigned>(Character));
            else
                Result.push_back(Character);
        }
    }
    Result.push_back('"');
    return Result;
}

std::string OkReply(std::string_view Fields)
{
    return Fields.empty() ? "{\"ok\":true}" : std::format("{{\"ok\":true,{}}}", Fields);
}

std::string ErrorReply(std::string_view Message)
{
    return std::format("{{\"ok\":false,\"error\":{}}}", JsonString(Message));
}
}
