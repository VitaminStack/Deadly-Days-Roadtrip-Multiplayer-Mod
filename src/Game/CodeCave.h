#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>

// Werkzeuge für Code-Patches mitten in Spielfunktionen. Ganze Funktionen hängt Core::Hooks ein, hier geht es um
// Sprünge an einzelne Befehle und kleine Code-Höhlen, die von dort zurück ins Spiel springen.
namespace Game::Code
{
// rel32 eines Sprungs oder Aufrufs, dessen nächster Befehl bei Next beginnt. nullopt, wenn Target zu weit weg liegt.
std::optional<int32_t> Rel32(uintptr_t Next, uintptr_t Target);

// Schreibt Code nacheinander in einen Puffer. Ein voller Puffer oder ein unerreichbares Ziel setzt Failed, danach
// schreibt der Writer nichts mehr. Sprünge ins Spiel brauchen rel32, der Puffer muss also nahe an der Exe liegen.
class Writer
{
public:
    Writer() = default;
    // Für Tests: fremder, schon ausführbarer Puffer.
    Writer(uint8_t* Buffer, size_t Size);

    // Holt eine ausführbare Seite unterhalb von Anchor, höchstens 1,75 GB entfernt. Falsch, wenn keine frei ist.
    bool AllocateNear(uintptr_t Anchor, size_t Size = 0x2000);

    uint8_t* Here() const { return Out; }
    bool Failed() const { return Broken; }

    void Emit(std::initializer_list<uint8_t> Bytes);
    void EmitRel(uintptr_t Target);
    // mov rax,Target; jmp rax. Liefert den Anfang, damit eine Stelle dorthin springen kann.
    uint8_t* EmitAbsoluteJump(uintptr_t Target);

private:
    bool Reserve(size_t Size);

    uint8_t* Out = nullptr;
    uint8_t* End = nullptr;
    bool Broken = false;
};

// Vergleicht die Bytes an Site und protokolliert eine Abweichung mit Where. Ein Lesefehler zählt als Abweichung.
bool Expect(uintptr_t Site, std::initializer_list<uint8_t> Bytes, std::string_view Where);

// Wahr, wenn der vtable-Eintrag Entry auf Expected zeigt. Protokolliert sonst.
bool ExpectVirtual(uintptr_t Entry, uintptr_t Expected, std::string_view Where);

// Überschreibt Code im Spiel (Seitenschutz auf Zeit aufheben, Befehlscache leeren).
bool WriteCode(uintptr_t Site, const void* Bytes, size_t Size);

// jmp rel32 von From nach To, 5 Byte.
bool WriteJump(uintptr_t From, const void* To);

// Setzt einen vtable-Eintrag. Old bekommt den bisherigen Wert.
bool WriteVirtual(uintptr_t Entry, uintptr_t Replacement, uintptr_t* Old = nullptr);
}
