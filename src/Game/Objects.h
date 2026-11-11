#pragma once

#include <cstdint>
#include <functional>
#include <string_view>

// Suche im globalen Objekt-Array (GObjects) und Aufrufe über ProcessEvent. Nur auf dem Game Thread aufrufen.
namespace Game::Objects
{
// Objekt an Index im Objekt-Array, 0 bei leerem Platz oder Lesefehler.
uintptr_t At(int32_t Index);

// Anzahl der Plätze im Objekt-Array.
int32_t Count();

// Gemerktes Objekt mit Platz und Serial im Objekt-Array, für Zeiger, die über Frames gehalten werden.
struct Ref
{
    uintptr_t Object = 0;
    int32_t Index = -1;
    uint32_t Serial = 0;
};

// Ref auf Object, leer, wenn Object nicht (mehr) im Objekt-Array steht.
Ref RefTo(uintptr_t Object);

// Wahr, solange der Platz noch auf dasselbe, nicht abgebaute Objekt zeigt (Game::Patches::HeldObjectLive). Liest nur das
// Objekt-Array und den Kopf des Objekts, nie Felder dahinter.
bool IsLive(const Ref& Held);

// Wahr, wenn die Klasse von Object ClassName heißt oder davon erbt.
bool IsA(uintptr_t Object, std::string_view ClassName);

// Erste Instanz (kein Klassen-Defaultobjekt) von ClassName oder einer Unterklasse. Index erhält ihren Platz.
uintptr_t FindInstance(std::string_view ClassName, int32_t* Index = nullptr);

// Objekt mit Name, dessen Outer OuterName heißt, etwa eine UFunction in ihrer Klasse. Leerer OuterName
// nimmt das erste Objekt mit dem Namen. 0, wenn nicht gefunden.
uintptr_t FindObject(std::string_view Name, std::string_view OuterName);

// Erstes Objekt mit diesem Namen, gleich unter welchem Outer, etwa ein Klassen-Defaultobjekt. 0, wenn nicht gefunden.
uintptr_t FindNamed(std::string_view Name);

// Ruft Visit für jeden Actor in ULevel::Actors auf, leere Plätze ausgelassen. Viel kürzer als das globale
// Objekt-Array; für Sekunden-Abgleiche über Actors der aktuellen Welt.
void ForEachLevelActor(uintptr_t Level, const std::function<void(uintptr_t)>& Visit);

// Ruft Function über ProcessEvent an Object auf. Params muss dem Parameterlayout der Funktion entsprechen.
void ProcessEvent(uintptr_t Object, uintptr_t Function, void* Params);

// Lädt ein Asset über seinen Pfad (etwa "/Game/.../BP_X.BP_X_C") wie der Starter des Spiels, über die Blueprint-Bibliothek:
// Pfad, Soft-Verweis, Laden. Liefert das Objekt, 0 bei Fehler.
uintptr_t LoadAsset(std::string_view Path);
}
