// Client: Lebensleisten über den Figuren. Sie hängen an UCommonAttributeSet::OnHealthChanged und OnMaxHealthChanged,
// die das Spiel nur beim Host auslöst. Beim Client läuft für den replizierten Wert nur das OnRep, danach ruft der Hook
// die angemeldeten Empfänger auf. Aus Spike HealthBars.cpp.
#include <atomic>
#include <vector>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t OnRepHealth = 0x57DC810;
constexpr uintptr_t OnRepMaxHealth = 0x57DCE70;
// FGameplayAttributeData: VTable, BaseValue, CurrentValue.
constexpr uintptr_t AttributeCurrentValue = 0xC;
constexpr uintptr_t MaxHealth = 0x30;
constexpr uintptr_t OnMaxHealthChanged = 0x40;
constexpr uintptr_t Health = 0xA0;
constexpr uintptr_t OnHealthChanged = 0xC0;
constexpr uintptr_t ObjectItemSerial = 0x10;

using OnRepFn = void(__fastcall*)(uintptr_t, const void*);
OnRepFn OriginalOnRepHealth = nullptr;
OnRepFn OriginalOnRepMaxHealth = nullptr;

// FScriptDelegate: schwacher Zeiger (Index, Seriennummer), dann der FName der Funktion.
struct ScriptDelegate
{
    int32_t ObjectIndex;
    int32_t ObjectSerial;
    uint64_t FunctionName;
};

uintptr_t ResolveWeak(int32_t Index, int32_t Serial)
{
    if (Index <= 0 || Index >= Objects::Count())
        return 0;
    const auto Chunks = Memory::ReadPointer(At(Offsets::GObjects));
    const auto Chunk = Chunks ? Memory::ReadPointer(*Chunks + 8 * (Index / Offsets::ObjectArray::ElementsPerChunk)) : std::nullopt;
    const uintptr_t Item = Chunk ? *Chunk + (Index % Offsets::ObjectArray::ElementsPerChunk) * Offsets::ObjectArray::ItemSize : 0;
    if (!Item || Memory::Read<int32_t>(Item + ObjectItemSerial) != Serial)
        return 0;
    return Memory::ReadPointer(Item + Offsets::ObjectArray::ItemObject).value_or(0);
}

// UFunction mit diesem FName in der Klassenkette des Objekts.
uintptr_t FindFunction(uintptr_t Object, uint64_t Name)
{
    auto Class = Memory::ReadPointer(Object + Offsets::UObject::Class);
    for (int Depth = 0; Class && Depth < 64; ++Depth, Class = Memory::ReadPointer(*Class + Offsets::UStruct::SuperStruct))
    {
        auto Field = Memory::ReadPointer(*Class + Offsets::UStruct::Children);
        for (int Count = 0; Field && Count < 4096; ++Count, Field = Memory::ReadPointer(*Field + Offsets::UField::Next))
            if (Memory::Read<uint64_t>(*Field + Offsets::UObject::Name) == Name)
                return *Field;
    }
    return 0;
}

std::atomic<bool> Reported = false;

// Wie ein Broadcast des Multicast-Delegates mit einem float. Die Liste wird vorher kopiert, ein Empfänger darf sie ändern.
void Broadcast(uintptr_t Set, uintptr_t DelegateOffset, float Value)
{
    const uintptr_t Data = Memory::ReadPointer(Set + DelegateOffset).value_or(0);
    const int32_t Num = Memory::Read<int32_t>(Set + DelegateOffset + 8).value_or(0);
    if (!Data || Num <= 0 || Num > 64)
        return;
    std::vector<ScriptDelegate> Bindings(static_cast<size_t>(Num));
    if (!Memory::ReadBytes(Data, Bindings.data(), Bindings.size() * sizeof(ScriptDelegate)))
        return;
    for (const ScriptDelegate& Binding : Bindings)
    {
        const uintptr_t Object = ResolveWeak(Binding.ObjectIndex, Binding.ObjectSerial);
        const uintptr_t Function = Object ? FindFunction(Object, Binding.FunctionName) : 0;
        if (!Function)
            continue;
        struct
        {
            float NewValue;
        } Params{Value};
        Objects::ProcessEvent(Object, Function, &Params);
    }
    if (!Reported.exchange(true))
        Core::Log::Info("Lebensleisten: erster Broadcast beim Client, {} Empfänger", Num);
}

void __fastcall OnRepHealthHook(uintptr_t Set, const void* Old)
{
    OriginalOnRepHealth(Set, Old);
    Broadcast(Set, OnHealthChanged, Memory::Read<float>(Set + Health + AttributeCurrentValue).value_or(0.0f));
}

void __fastcall OnRepMaxHealthHook(uintptr_t Set, const void* Old)
{
    OriginalOnRepMaxHealth(Set, Old);
    Broadcast(Set, OnMaxHealthChanged, Memory::Read<float>(Set + MaxHealth + AttributeCurrentValue).value_or(0.0f));
}

bool CheckHealthBars()
{
    return Code::Expect(At(OnRepHealth), {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18}, "OnRep_Health") &&
           Code::Expect(At(OnRepMaxHealth), {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18}, "OnRep_MaxHealth");
}

// Beim Host laufen die OnRep-Funktionen nicht, der Patch wirkt also nur beim Client.
bool ApplyHealthBars(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(OnRepHealth)), reinterpret_cast<void*>(&OnRepHealthHook),
                                reinterpret_cast<void**>(&OriginalOnRepHealth)) &&
           Core::Hooks::Install(reinterpret_cast<void*>(At(OnRepMaxHealth)), reinterpret_cast<void*>(&OnRepMaxHealthHook),
                                reinterpret_cast<void**>(&OriginalOnRepMaxHealth));
}

// Je Patch die benutzten Adressen der Signaturtabelle, erzeugt von tools/patch_signatures.py sites.
constexpr uintptr_t HealthBarsSites[] = {OnRepHealth, OnRepMaxHealth};
const Patch List[] = {
    {"Lebensleisten beim Client", &CheckHealthBars, &ApplyHealthBars, HealthBarsSites},
};
}

std::span<const Patch> HealthBarPatches()
{
    return List;
}
}
