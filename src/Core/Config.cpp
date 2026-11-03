#include "Core/Config.h"

#include <charconv>
#include <fstream>
#include <sstream>

namespace Core
{
namespace
{
std::string_view Trim(std::string_view Text)
{
    constexpr std::string_view Blank = " \t\r\n";
    const size_t First = Text.find_first_not_of(Blank);
    if (First == std::string_view::npos)
        return {};
    return Text.substr(First, Text.find_last_not_of(Blank) - First + 1);
}
}

Config Config::Parse(std::string_view Text)
{
    Config Result;
    while (!Text.empty())
    {
        const size_t End = Text.find('\n');
        const std::string_view Line = Trim(Text.substr(0, End));
        Text = End == std::string_view::npos ? std::string_view{} : Text.substr(End + 1);

        const size_t Separator = Line.find('=');
        if (Line.empty() || Line[0] == '#' || Line[0] == ';' || Separator == std::string_view::npos)
            continue;
        Result.Values.insert_or_assign(std::string(Trim(Line.substr(0, Separator))), std::string(Trim(Line.substr(Separator + 1))));
    }
    return Result;
}

Config Config::Load(const std::filesystem::path& File)
{
    std::ifstream Input(File);
    if (!Input)
        return {};
    std::ostringstream Text;
    Text << Input.rdbuf();
    return Parse(Text.str());
}

std::string Config::GetString(std::string_view Key, std::string_view Default) const
{
    const auto Found = Values.find(Key);
    return Found != Values.end() ? Found->second : std::string(Default);
}

bool Config::GetBool(std::string_view Key, bool Default) const
{
    const auto Found = Values.find(Key);
    if (Found == Values.end())
        return Default;
    if (Found->second == "true" || Found->second == "1")
        return true;
    if (Found->second == "false" || Found->second == "0")
        return false;
    return Default;
}

int Config::GetInt(std::string_view Key, int Default) const
{
    const auto Found = Values.find(Key);
    if (Found == Values.end())
        return Default;
    const std::string& Text = Found->second;
    int Value = 0;
    const auto [End, Error] = std::from_chars(Text.data(), Text.data() + Text.size(), Value);
    return Error == std::errc{} && End == Text.data() + Text.size() ? Value : Default;
}
}
