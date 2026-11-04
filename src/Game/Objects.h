#pragma once

#include <cstdint>
#include <string_view>

// Suche im globalen Objekt-Array (GObjects) und Aufrufe über ProcessEvent. Nur auf dem Game Thread aufrufen.
namespace Game::Objects
{
// Objekt an Index im Objekt-Array, 0 bei leerem Platz oder Lesefehler.
uintptr_t At(int32_t Index);

// Anzahl der Plätze im Objekt-Array.
int32_t Count();

// Wahr, wenn die Klasse von Object ClassName heißt oder davon erbt.
bool IsA(uintptr_t Object, std::string_view ClassName);

// Erste Instanz (kein Klassen-Defaultobjekt) von ClassName oder einer Unterklasse. Index erhält ihren Platz.
uintptr_t FindInstance(std::string_view ClassName, int32_t* Index = nullptr);

// Objekt mit Name, dessen Outer OuterName heißt, etwa eine UFunction in ihrer Klasse. 0, wenn nicht gefunden.
uintptr_t FindObject(std::string_view Name, std::string_view OuterName);

// Erstes Objekt mit diesem Namen, gleich unter welchem Outer, etwa ein Klassen-Defaultobjekt. 0, wenn nicht gefunden.
uintptr_t FindNamed(std::string_view Name);

// Ruft Function über ProcessEvent an Object auf. Params muss dem Parameterlayout der Funktion entsprechen.
void ProcessEvent(uintptr_t Object, uintptr_t Function, void* Params);
}
