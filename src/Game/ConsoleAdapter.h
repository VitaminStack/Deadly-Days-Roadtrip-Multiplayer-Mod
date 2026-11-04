#pragma once

#include <string_view>

// Konsolenbefehle des Spiels über UKismetSystemLibrary::ExecuteConsoleCommand, etwa für Listen-Server und Beitritt.
namespace Game
{
// Nur auf dem Game Thread. Falsch, wenn Welt oder Funktion nicht gefunden wurden.
bool ExecuteConsoleCommand(std::wstring_view Command);
}
