#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// Löst Felder und Funktionen von Spielklassen zur Laufzeit über die Engine-Reflexion auf, statt über feste Offsets.
// Ein Feld wird über UStruct::ChildProperties gesucht (FProperty: Offset, Größe, Typ), eine Funktion über
// UStruct::Children (UFunction). Beide Suchen gehen die Oberklassen mit, jede Schleife hat eine feste Grenze und
// jeder Speicherzugriff läuft über Memory::Read. Passt etwas nicht, gibt es kein Ergebnis statt eines geratenen Werts.
namespace Game::Reflection
{
// Erwartetes Feld: Klasse im Paket, Feldname, Typname der FProperty-Klasse (etwa "IntProperty") und Größe in Bytes.
struct FieldSpec
{
    std::string_view Package; // etwa "/Script/DDSurvivors"
    std::string_view Class;   // ohne Präfix, etwa "TutorialStateSubsystem"
    std::string_view Name;
    std::string_view Type;
    int32_t Size;
};

// Aufgelöstes Feld. Für BoolProperty steht in BoolMask die Bitmaske, 0xFF heißt ganzes Byte (natives bool).
struct Field
{
    int32_t Offset = 0;
    int32_t Size = 0;
    std::string Type;
    uint8_t BoolMask = 0;
};

enum class FieldError
{
    None,
    NotFound,      // kein Feld mit dem Namen in der Klassenkette
    TypeMismatch,  // anderer FProperty-Typ als erwartet
    SizeMismatch,  // andere Elementgröße, ein statisches Array oder Bitfeld-bool
    OutOfBounds    // Offset liegt nicht innerhalb der Klasse
};

// Liefert den Namen zu einem FName (ComparisonIndex, Number). Im Spiel die Namenstabelle, in Tests eine Tabelle.
using NameReader = std::string (*)(int32_t ComparisonIndex, int32_t Number);

// Reine Suche ohne Cache und ohne Log, testbar mit nachgebauten Layouts.
// Struct ist die Adresse eines UStruct (meist UClass). Error erhält den Grund, wenn kein Ergebnis kommt.
std::optional<Field> FindField(uintptr_t Struct, const FieldSpec& Spec, NameReader Names, FieldError* Error = nullptr);

// UFunction mit dem Namen in Struct oder einer Oberklasse, 0 wenn nicht gefunden.
uintptr_t FindFunction(uintptr_t Struct, std::string_view Name, NameReader Names);

// Text zu FieldError für Logmeldungen.
const char* ToString(FieldError Error);

// Laufzeit, nur auf dem Game Thread: sucht die Klasse über das Objekt-Array und cacht das Ergebnis einmal je Start.
// Fehlt die Klasse noch, wird beim nächsten Aufruf neu gesucht. Fehlt das Feld oder passt es nicht, bleibt das Ergebnis
// leer, mit einer einzigen Logmeldung. Der Aufrufer schaltet dann nur seinen eigenen Adapter ab.
std::optional<Field> Property(const FieldSpec& Spec);

// UFunction Name in Class (Paket wie bei FieldSpec), gecacht. 0, wenn sie fehlt; dann eine Logmeldung.
uintptr_t Function(std::string_view Package, std::string_view Class, std::string_view Name);
}
