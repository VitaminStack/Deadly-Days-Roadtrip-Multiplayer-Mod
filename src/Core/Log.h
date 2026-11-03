#pragma once

#include <filesystem>
#include <format>
#include <optional>
#include <string_view>
#include <utility>

// Thread-sicheres Logging in eine Datei. Vor Open gehen Zeilen verloren, ein Absturz gibt es dadurch nie.
// Keine vollständigen lokalen Pfade loggen: Logs landen in Bug-Reports.
namespace Core::Log
{
enum class Level
{
    Debug,
    Info,
    Warn,
    Error
};

bool Open(const std::filesystem::path& File);
void SetLevel(Level Minimum);
std::optional<Level> ParseLevel(std::string_view Name);
void Write(Level Severity, std::string_view Message);

template <typename... Args> void Debug(std::format_string<Args...> Format, Args&&... Arguments)
{
    Write(Level::Debug, std::format(Format, std::forward<Args>(Arguments)...));
}

template <typename... Args> void Info(std::format_string<Args...> Format, Args&&... Arguments)
{
    Write(Level::Info, std::format(Format, std::forward<Args>(Arguments)...));
}

template <typename... Args> void Warn(std::format_string<Args...> Format, Args&&... Arguments)
{
    Write(Level::Warn, std::format(Format, std::forward<Args>(Arguments)...));
}

template <typename... Args> void Error(std::format_string<Args...> Format, Args&&... Arguments)
{
    Write(Level::Error, std::format(Format, std::forward<Args>(Arguments)...));
}
}
