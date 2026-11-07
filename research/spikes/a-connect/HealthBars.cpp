// Client: Lebensleisten ueber den Figuren aktualisieren. Sie haengen an UCommonAttributeSet::OnHealthChanged und
// OnMaxHealthChanged. Das Spiel loest beide nur beim Host aus; beim Client laeuft fuer einen replizierten Wert nur das
// Standard-OnRep, die Leisten blieben auf ihrem Startwert. Der Spike ruft nach dem OnRep die angemeldeten Empfaenger auf.
#include "Spike.h"

#include <vector>

namespace Spike
{
namespace
{
// Implementierungen von UCommonAttributeSet::OnRep_Health und OnRep_MaxHealth (hinter den exec-Thunks).
const uintptr_t kOnRepHealth = Rva(0x57DC840);
const uintptr_t kOnRepMaxHealth = Rva(0x57DCEA0);
// FGameplayAttributeData: VTable, BaseValue, CurrentValue.
constexpr uintptr_t kAttributeCurrentValue = 0xC;
constexpr uintptr_t kMaxHealth = 0x30;
constexpr uintptr_t kOnMaxHealthChanged = 0x40;
constexpr uintptr_t kHealth = 0xA0;
constexpr uintptr_t kOnHealthChanged = 0xC0;
constexpr uintptr_t kObjectItemSerial = 0x10;
constexpr uintptr_t kStructChildren = 0x48;
constexpr uintptr_t kFieldNext = 0x28;

using OnRepFn = void(__fastcall*)(uintptr_t, const void*);
OnRepFn OriginalOnRepHealth = nullptr;
OnRepFn OriginalOnRepMaxHealth = nullptr;

// FScriptDelegate: schwacher Zeiger (Index, Seriennummer), dann der FName der Funktion.
struct FScriptDelegate
{
    int32_t ObjectIndex;
    int32_t ObjectSerial;
    uint64_t FunctionName;
};

uintptr_t ResolveWeak(int32_t Index, int32_t Serial)
{
    if (Index <= 0 || Index >= Read<int32_t>(Base + kGObjects + 8))
        return 0;
    const uintptr_t Chunk = Read<uintptr_t>(Read<uintptr_t>(Base + kGObjects) + 8 * (Index >> 16));
    const uintptr_t Item = Chunk ? Chunk + (Index & 0xFFFF) * 0x18 : 0;
    if (!Item || Read<int32_t>(Item + kObjectItemSerial) != Serial)
        return 0;
    return Read<uintptr_t>(Item + 8);
}

// Sucht die UFunction mit diesem FName in der Klassenkette des Objekts.
uintptr_t FindFunction(uintptr_t Object, uint64_t Name)
{
    for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class; Class = Read<uintptr_t>(Class + 0x40))
        for (uintptr_t Field = Read<uintptr_t>(Class + kStructChildren); Field; Field = Read<uintptr_t>(Field + kFieldNext))
            if (Read<uint64_t>(Field + 0x18) == Name)
                return Field;
    return 0;
}

// Wie ein Broadcast des Multicast-Delegates mit einem float. Die Liste wird vorher kopiert, ein Empfaenger darf sie aendern.
void Broadcast(uintptr_t Set, uintptr_t DelegateOffset, float Value)
{
    const auto* Data = Read<const FScriptDelegate*>(Set + DelegateOffset);
    const int32_t Num = Read<int32_t>(Set + DelegateOffset + 8);
    if (!Data || Num <= 0 || Num > 64)
        return;
    const std::vector<FScriptDelegate> Bindings(Data, Data + Num);
    for (const FScriptDelegate& Binding : Bindings)
    {
        const uintptr_t Object = ResolveWeak(Binding.ObjectIndex, Binding.ObjectSerial);
        const uintptr_t Function = Object ? FindFunction(Object, Binding.FunctionName) : 0;
        if (!Function)
            continue;
        struct
        {
            float NewValue;
        } Params{Value};
        Read<ProcessEventFn*>(Object)[kProcessEventIndex](Object, Function, &Params);
    }
    static bool Reported = false;
    if (!Reported)
    {
        Reported = true;
        Log("Lebensleisten: erster Broadcast beim Client, " + std::to_string(Num) + " Empfaenger");
    }
}

void __fastcall OnRepHealthHook(uintptr_t Set, const void* Old)
{
    OriginalOnRepHealth(Set, Old);
    Broadcast(Set, kOnHealthChanged, Read<float>(Set + kHealth + kAttributeCurrentValue));
}

void __fastcall OnRepMaxHealthHook(uintptr_t Set, const void* Old)
{
    OriginalOnRepMaxHealth(Set, Old);
    Broadcast(Set, kOnMaxHealthChanged, Read<float>(Set + kMaxHealth + kAttributeCurrentValue));
}

// Erste Anweisung mov [rsp+10h],rbx (5 Byte, ohne RIP-Bezug) ins Trampolin, dann zurueck hinter die Stelle.
bool HookOnRep(CodeWriter& Code, uintptr_t Rva, void* Hook, OnRepFn& Original, const char* Where)
{
    const uintptr_t Site = Base + Rva;
    if (!Expect(Site, {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18}, Where))
        return false;
    Original = reinterpret_cast<OnRepFn>(Code.Here());
    Code.Emit({0x48, 0x89, 0x5C, 0x24, 0x10});
    Code.Emit({0xE9});
    Code.EmitRel(Site + 5);
    WriteJump(Site, Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(Hook)));
    return true;
}
}

// Beim Host laufen die OnRep-Funktionen nicht, der Patch wirkt also nur beim Client.
bool PatchHealthBars(CodeWriter& Code)
{
    return HookOnRep(Code, kOnRepHealth, reinterpret_cast<void*>(&OnRepHealthHook), OriginalOnRepHealth, "OnRep_Health") &&
           HookOnRep(Code, kOnRepMaxHealth, reinterpret_cast<void*>(&OnRepMaxHealthHook), OriginalOnRepMaxHealth, "OnRep_MaxHealth");
}
}
