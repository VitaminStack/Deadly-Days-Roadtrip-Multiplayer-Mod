#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>

namespace Core
{
// Einfache Datei mit Zeilen "schluessel=wert". Kommentare beginnen mit # oder ;, Abschnitte gibt es nicht.
// Fehlt eine Datei oder ein Schlüssel, gilt der Standardwert des Aufrufers.
class Config
{
public:
    static Config Parse(std::string_view Text);
    static Config Load(const std::filesystem::path& File);

    std::string GetString(std::string_view Key, std::string_view Default) const;
    bool GetBool(std::string_view Key, bool Default) const;
    int GetInt(std::string_view Key, int Default) const;

private:
    std::map<std::string, std::string, std::less<>> Values;
};
}
