#include "Core/Log.h"

#include <windows.h>

#include <chrono>
#include <fstream>
#include <mutex>

namespace Core::Log
{
namespace
{
std::mutex Mutex;
std::ofstream Output;
Level Minimum = Level::Info;

const char* Name(Level Severity)
{
    switch (Severity)
    {
    case Level::Debug:
        return "DEBUG";
    case Level::Info:
        return "INFO";
    case Level::Warn:
        return "WARN";
    default:
        return "ERROR";
    }
}
}

bool Open(const std::filesystem::path& File)
{
    std::error_code Error;
    std::filesystem::create_directories(File.parent_path(), Error);

    const std::scoped_lock Lock(Mutex);
    Output.close();
    Output.clear();
    Output.open(File, std::ios::out | std::ios::trunc);
    return Output.is_open();
}

void SetLevel(Level NewMinimum)
{
    const std::scoped_lock Lock(Mutex);
    Minimum = NewMinimum;
}

std::optional<Level> ParseLevel(std::string_view Text)
{
    if (Text == "debug")
        return Level::Debug;
    if (Text == "info")
        return Level::Info;
    if (Text == "warn")
        return Level::Warn;
    if (Text == "error")
        return Level::Error;
    return std::nullopt;
}

void Write(Level Severity, std::string_view Message)
{
    const std::scoped_lock Lock(Mutex);
    if (Severity < Minimum || !Output.is_open())
        return;
    const auto Now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
    // Sofort schreiben: Nach einem Absturz zählt gerade die letzte Zeile.
    Output << std::format("{:%F %T} [{}] [{}] {}\n", Now, GetCurrentThreadId(), Name(Severity), Message) << std::flush;
}
}
