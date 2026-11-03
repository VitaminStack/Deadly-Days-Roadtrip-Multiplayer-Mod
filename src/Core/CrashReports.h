#pragma once

#include <string_view>

// Hält Absturzberichte vom Hersteller fern, solange die Mod läuft. Siehe src/Core/README.md.
namespace Core::CrashReports
{
// Lässt im eigenen Prozess jeden Start von CrashReportClient.exe scheitern. Braucht Hooks::Initialize.
bool Block();

// Ob ein CreateProcessW-Aufruf den Crash-Reporter starten würde. Prüft Programm und Kommandozeile.
bool IsCrashReporterLaunch(std::wstring_view Application, std::wstring_view CommandLine);
}
