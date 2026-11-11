// opt-gegner-indikatoren: Warnanzeigen der Gegnerangriffe beim Client. Die Angriffsfähigkeiten laufen nur beim Host, ihre
// Niagara-Anzeigen entstehen dort lokal und replizieren nicht (kein Net-Feld, kein RPC). Lokal zuerst: Der Host meldet über
// TakeEnemyTelegraphs nur den Auslöser, der Client baut die Anzeige mit dem nativen Niagara-System selbst auf:
// - Kreis (UEnemyAttackAbility::SpawnCircleTelegraph, Spucken, Sprünge, Bosse): dasselbe System am selben Ort mit denselben
//   Parametern über UNiagaraFunctionLibrary::SpawnSystemAtLocation.
// - Strich (UTackleAbilityExecution, Sprinter): die eigene Komponente des Gegners beim Client, über ihre nativen Funktionen
//   ActivateLineTelegraph und CalculateLineTelegraphEndPoint; Farbwechsel und Länge rechnet sie selbst. Aus: Deactivate.
// Schaden, Ziel, Zeitpunkt und Sprint bleiben beim Host. Befund: docs/GAME_ANALYSIS.md#warnanzeigen-der-gegner-beim-client-opt-gegner-indikatoren
#include <windows.h>

#include <array>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/CodeCave.h"
#include "Game/Memory.h"
#include "Game/NetworkObjects.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"
#include "Game/TelegraphAdapter.h"

namespace Game::Patches
{
namespace
{
// Native Implementierungen (Build 26495302), über die Exec-Thunks der UFunctions gefunden, siehe docs/OFFSETS.md.
constexpr uintptr_t SpawnCircleTelegraph = 0x58908C0;
constexpr uintptr_t ActivateLineTelegraph = 0x58A3DB0;
constexpr uintptr_t CalculateLineTelegraphEndPoint = 0x58A9D30;
constexpr uintptr_t EndTackle = 0x58B1130;
constexpr uintptr_t ExecuteTackle = 0x58B1F40;

// UEnemyAttackAbility: Namen der Niagara-Parameter (FName).
constexpr uintptr_t AbilitySizeName = 0x440;
constexpr uintptr_t AbilityLifetimeName = 0x448;
constexpr uintptr_t AbilityDelayName = 0x450;
// UTackleAbilityExecution.
constexpr uintptr_t LineTelegraph = 0x238;     // UNiagaraComponent*
constexpr uintptr_t TackleBlocked = 0x268;     // bool aus der Disassembly: gesetzt, rechnet CalculateLineTelegraphEndPoint nichts
// UActorComponent::OwnerPrivate (ohne Reflexionsdaten), aus der Disassembly von ActivateLineTelegraph.
constexpr uintptr_t ComponentOwner = 0xA8;
constexpr uint32_t GarbageOrUnreachable = 0x30;

constexpr size_t MaxQueued = 64;
// Der Host ruft CalculateLineTelegraphEndPoint jeden Frame; 20 je Sekunde reichen für die Anzeige.
constexpr ULONGLONG LineUpdateMs = 50;
// Client: ohne Nachricht so lange nach dem letzten Stand blendet er den Strich selbst aus (Verlust, Tod, Weltwechsel).
constexpr ULONGLONG LineTimeoutMs = 3000;
// Client: Kreise, die das System nicht selbst beendet, nach Delay + Lifetime + Rand ausblenden.
constexpr ULONGLONG CircleGraceMs = 1000;

using CircleFn = uintptr_t(__fastcall*)(uintptr_t Ability, uintptr_t System, const double* Location, float Delay, float Lifetime,
                                         float Size, bool AutoDestroy, uint8_t Pool, bool Attached);
using LineFn = void(__fastcall*)(uintptr_t Execution);
using EndPointFn = void(__fastcall*)(uintptr_t Execution, const double* Target, float Percentage);
using EndTackleFn = void(__fastcall*)(uintptr_t Execution, bool Cancelled);
using ExecuteTackleFn = void(__fastcall*)(uintptr_t Execution, float Damage);
CircleFn OriginalCircle = nullptr;
LineFn OriginalActivateLine = nullptr;
EndPointFn OriginalEndPoint = nullptr;
EndTackleFn OriginalEndTackle = nullptr;
ExecuteTackleFn OriginalExecuteTackle = nullptr;

// Gemerktes Objekt, geprüft über seinen Platz im Objekt-Array und seine Klasse; freigegebener Speicher wird nie benutzt.
struct Known
{
    uintptr_t Object = 0;
    uintptr_t Class = 0;
    int32_t Index = -1;
};

Known Remember(uintptr_t Object)
{
    const int32_t Index = Memory::Read<int32_t>(Object + Offsets::UObject::InternalIndex).value_or(-1);
    const uintptr_t Class = Memory::ReadPointer(Object + Offsets::UObject::Class).value_or(0);
    if (!Class || Objects::At(Index) != Object)
        return {};
    return {Object, Class, Index};
}

bool StillThere(const Known& Entry)
{
    if (!Entry.Object || Objects::At(Entry.Index) != Entry.Object)
        return false;
    if (Memory::ReadPointer(Entry.Object + Offsets::UObject::Class).value_or(0) != Entry.Class)
        return false;
    return !(Memory::Read<uint32_t>(Entry.Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachable) & GarbageOrUnreachable);
}

// FName als Text samt Nummer, der Index gilt nur je Prozess.
std::string NameText(uintptr_t Address)
{
    const auto Index = Memory::Read<int32_t>(Address);
    if (!Index || *Index <= 0)
        return {};
    const int32_t Number = Memory::Read<int32_t>(Address + 4).value_or(0);
    return Memory::NameFromIndex(*Index) + (Number > 0 ? "_" + std::to_string(Number - 1) : "");
}

// „/Game/…/NS_X.NS_X“: Paket (Outer) und Objektname.
std::string PathOf(uintptr_t Object)
{
    const uintptr_t Package = Memory::ReadPointer(Object + Offsets::UObject::Outer).value_or(0);
    const std::string Outer = Package ? Memory::NameOf(Package) : std::string{};
    const std::string Name = Memory::NameOf(Object);
    return Outer.empty() || Name.empty() ? std::string{} : Outer + "." + Name;
}

// ---- Host ----

// Nur Game Thread: die Fähigkeiten laufen dort.
std::deque<EnemyTelegraph> Queue;
struct HostLine
{
    uintptr_t Execution = 0;
    uint64_t Enemy = 0;
    ULONGLONG LastSent = 0;
};
std::array<HostLine, 32> HostLines{};
uintptr_t HostWorld = 0;
int HostReported = 0;

// Zähler für die Logzeile je 30 s, damit Host und Client im Test vergleichbar sind.
struct Counts
{
    unsigned Circles = 0, Lines = 0, Skipped = 0;
    bool operator==(const Counts&) const = default;
};
Counts Counted, Logged;
ULONGLONG NextCountLog = 0;

void LogCounts(const char* Side)
{
    const ULONGLONG Now = GetTickCount64();
    if (Now < NextCountLog || Counted == Logged)
        return;
    NextCountLog = Now + 30000;
    Logged = Counted;
    Core::Log::Info("Warnanzeigen: {} bisher {} Kreise, {} Sprint-Striche, {} übersprungen", Side, Counted.Circles, Counted.Lines,
                    Counted.Skipped);
}

bool Publishing()
{
    return CurrentRole() == NetRole::Host;
}

void Push(EnemyTelegraph Value)
{
    if (Queue.size() >= MaxQueued)
        Queue.pop_front();
    Queue.push_back(std::move(Value));
}

HostLine* FindHostLine(uintptr_t Execution, bool Create)
{
    const uintptr_t World = CurrentWorld();
    if (World != HostWorld)
    {
        HostLines.fill({});
        HostWorld = World;
    }
    for (HostLine& Line : HostLines)
        if (Line.Execution == Execution)
            return &Line;
    if (!Create)
        return nullptr;
    // Ohne NetGUID (Gegner noch nicht repliziert) bleibt Enemy 0: gemerkt, damit nicht jeder Frame neu sucht, aber nie gemeldet.
    const uintptr_t Enemy = Memory::ReadPointer(Execution + Offsets::UObject::Outer).value_or(0);
    const uint64_t Guid = Enemy ? NetworkGuidFor(Enemy) : 0;
    HostLine* Slot = &HostLines[0];
    for (HostLine& Line : HostLines)
        if (!Line.Execution || Line.LastSent < Slot->LastSent)
            Slot = &Line;
    *Slot = {Execution, Guid, 0};
    return Slot;
}

void PushLine(TelegraphKind Kind, const HostLine& Line, const double* Target = nullptr, float Percentage = 0)
{
    if (!Line.Enemy)
        return;
    EnemyTelegraph Value;
    Value.Kind = Kind;
    Value.Enemy = Line.Enemy;
    if (Target)
    {
        Value.X = Target[0];
        Value.Y = Target[1];
        Value.Z = Target[2];
    }
    Value.Percentage = Percentage;
    Push(std::move(Value));
}

uintptr_t __fastcall CircleHook(uintptr_t Ability, uintptr_t System, const double* Location, float Delay, float Lifetime, float Size,
                                bool AutoDestroy, uint8_t Pool, bool Attached)
{
    const uintptr_t Component = OriginalCircle(Ability, System, Location, Delay, Lifetime, Size, AutoDestroy, Pool, Attached);
    if (!Component || !System || !Location || !Publishing())
        return Component;
    EnemyTelegraph Value;
    Value.Kind = TelegraphKind::Circle;
    Value.X = Location[0];
    Value.Y = Location[1];
    Value.Z = Location[2];
    Value.Delay = Delay;
    Value.Lifetime = Lifetime;
    Value.Size = Size;
    Value.System = PathOf(System);
    Value.SizeName = NameText(Ability + AbilitySizeName);
    Value.LifetimeName = NameText(Ability + AbilityLifetimeName);
    Value.DelayName = NameText(Ability + AbilityDelayName);
    if (Value.System.empty())
        return Component;
    if (HostReported < 4 && ++HostReported)
        Core::Log::Info("Warnanzeigen: Host meldet Kreis {} ({}, {} s){}", Value.System, Memory::NameOf(Memory::ReadPointer(Ability + Offsets::UObject::Class).value_or(0)),
                        Lifetime, Attached ? ", angehängt" : "");
    ++Counted.Circles;
    Push(std::move(Value));
    return Component;
}

void __fastcall ActivateLineHook(uintptr_t Execution)
{
    OriginalActivateLine(Execution);
    if (!Publishing())
        return;
    if (HostLine* Line = FindHostLine(Execution, true); Line && Line->Enemy)
    {
        Line->LastSent = 0;
        ++Counted.Lines;
        PushLine(TelegraphKind::LineStart, *Line);
        if (HostReported < 8 && ++HostReported)
            Core::Log::Info("Warnanzeigen: Host meldet Sprint-Strich von {}",
                            Memory::NameOf(Memory::ReadPointer(Memory::ReadPointer(Execution + Offsets::UObject::Outer).value_or(0) +
                                                               Offsets::UObject::Class).value_or(0)));
    }
}

void __fastcall EndPointHook(uintptr_t Execution, const double* Target, float Percentage)
{
    OriginalEndPoint(Execution, Target, Percentage);
    if (!Target || !Publishing())
        return;
    HostLine* Line = FindHostLine(Execution, true);
    const ULONGLONG Now = GetTickCount64();
    if (!Line || (Now - Line->LastSent < LineUpdateMs && Percentage < 1.0f))
        return;
    Line->LastSent = Now;
    PushLine(TelegraphKind::LineUpdate, *Line, Target, Percentage);
}

void EndLine(uintptr_t Execution)
{
    if (!Publishing())
        return;
    if (HostLine* Line = FindHostLine(Execution, false))
    {
        PushLine(TelegraphKind::LineEnd, *Line);
        *Line = {};
    }
}

void __fastcall EndTackleHook(uintptr_t Execution, bool Cancelled)
{
    OriginalEndTackle(Execution, Cancelled);
    EndLine(Execution);
}

void __fastcall ExecuteTackleHook(uintptr_t Execution, float Damage)
{
    OriginalExecuteTackle(Execution, Damage);
    EndLine(Execution);
}

// ---- Client ----

struct ClientState
{
    uintptr_t World = 0;
    // Gegner-NetGUID -> eigene UTackleAbilityExecution des Gegners beim Client.
    struct Line
    {
        uint64_t Enemy = 0;
        Known Execution;
        ULONGLONG LastSeen = 0;
        bool Active = false;
    };
    std::vector<Line> Lines;
    struct Circle
    {
        Known Component;
        ULONGLONG Until = 0;
    };
    std::vector<Circle> Circles;
    std::unordered_map<std::string, Known> Systems;
    std::unordered_map<std::string, ULONGLONG> MissingSystems;
    std::unordered_map<std::string, uint64_t> Names;
    // Gegner-Actor -> seine UTackleAbilityExecution, aus der letzten Suche über das Objekt-Array.
    std::unordered_map<uintptr_t, Known> Tackles;
    ULONGLONG NextTackleScan = 0;
    uintptr_t TackleClass = 0;
    int Reported = 0;
} C;

void ResetClient(uintptr_t World)
{
    // Die Komponenten gehören zur alten Welt und sind mit ihr verschwunden, nichts mehr anfassen.
    C.World = World;
    C.Lines.clear();
    C.Circles.clear();
    C.Systems.clear();
    C.MissingSystems.clear();
    C.Tackles.clear();
    C.NextTackleScan = 0;
}

uint64_t MakeName(const std::string& Text)
{
    if (Text.empty())
        return 0;
    if (const auto It = C.Names.find(Text); It != C.Names.end())
        return It->second;
    const uintptr_t Library = Objects::FindNamed("Default__KismetStringLibrary");
    static const uintptr_t Function = Objects::FindObject("Conv_StringToName", "KismetStringLibrary");
    if (!Library || !Function)
        return 0;
    struct FString
    {
        const wchar_t* Data;
        int32_t Num;
        int32_t Max;
    };
    const std::wstring Wide(Text.begin(), Text.end());
    struct
    {
        FString InString;
        uint64_t ReturnValue;
    } Params{{Wide.c_str(), static_cast<int32_t>(Wide.size() + 1), static_cast<int32_t>(Wide.size() + 1)}, 0};
    Objects::ProcessEvent(Library, Function, &Params);
    if (Params.ReturnValue)
        C.Names.emplace(Text, Params.ReturnValue);
    return Params.ReturnValue;
}

uintptr_t FindSystem(const std::string& Path, ULONGLONG Now)
{
    if (const auto It = C.Systems.find(Path); It != C.Systems.end() && StillThere(It->second))
        return It->second.Object;
    if (const auto It = C.MissingSystems.find(Path); It != C.MissingSystems.end() && Now < It->second)
        return 0;
    const size_t Dot = Path.rfind('.');
    uintptr_t System = Dot == std::string::npos ? 0 : Objects::FindObject(Path.substr(Dot + 1), Path.substr(0, Dot));
    if (!System)
        System = Objects::LoadAsset(Path);
    const Known Entry = System && Objects::IsA(System, "NiagaraSystem") ? Remember(System) : Known{};
    if (!Entry.Object)
    {
        C.MissingSystems[Path] = Now + 10000;
        Core::Log::Warn("Warnanzeigen: Niagara-System {} beim Client nicht gefunden", Path);
        return 0;
    }
    C.Systems[Path] = Entry;
    return Entry.Object;
}

void SetFloat(uintptr_t Component, const std::string& Name, float Value)
{
    static const uintptr_t Function = Objects::FindObject("SetVariableFloat", "NiagaraComponent");
    const uint64_t Key = MakeName(Name);
    if (!Function || !Key)
        return;
    struct
    {
        uint64_t Name;
        float Value;
        uint8_t Pad[4];
    } Params{Key, Value, {}};
    Objects::ProcessEvent(Component, Function, &Params);
}

void ShowCircle(const EnemyTelegraph& Value, ULONGLONG Now)
{
    static const uintptr_t Function = Objects::FindObject("SpawnSystemAtLocation", "NiagaraFunctionLibrary");
    const uintptr_t Library = Objects::FindNamed("Default__NiagaraFunctionLibrary");
    const uintptr_t System = FindSystem(Value.System, Now);
    if (!Function || !Library || !System)
    {
        ++Counted.Skipped;
        return;
    }
    struct
    {
        uintptr_t WorldContext;
        uintptr_t System;
        double Location[3];
        double Rotation[3];
        double Scale[3];
        bool AutoDestroy, AutoActivate;
        uint8_t Pool;
        bool PreCull;
        uint8_t Pad[4];
        uintptr_t ReturnValue;
    } Params{C.World, System, {Value.X, Value.Y, Value.Z}, {0, 0, 0}, {1, 1, 1}, true, true, 0, false, {}, 0};
    static_assert(sizeof(Params) == 0x68);
    Objects::ProcessEvent(Library, Function, &Params);
    if (!Params.ReturnValue)
    {
        ++Counted.Skipped;
        return;
    }
    ++Counted.Circles;
    // Reihenfolge wie beim Host: Size, Lifetime, Delay.
    SetFloat(Params.ReturnValue, Value.SizeName, Value.Size);
    SetFloat(Params.ReturnValue, Value.LifetimeName, Value.Lifetime);
    SetFloat(Params.ReturnValue, Value.DelayName, Value.Delay);
    if (const Known Entry = Remember(Params.ReturnValue); Entry.Object && C.Circles.size() < 128)
        C.Circles.push_back({Entry, Now + static_cast<ULONGLONG>((Value.Delay + Value.Lifetime) * 1000.0f) + CircleGraceMs});
    if (C.Reported < 4 && ++C.Reported)
        Core::Log::Info("Warnanzeigen: Client zeigt Kreis {} ({} s)", Value.System, Value.Lifetime);
}

void Deactivate(uintptr_t Component)
{
    static const uintptr_t Function = Objects::FindObject("Deactivate", "ActorComponent");
    if (Function && Component)
        Objects::ProcessEvent(Component, Function, nullptr);
}

// Eigene UTackleAbilityExecution des Gegners beim Client: Default-Subobjekt mit dem Gegner als Outer.
Known FindExecution(uint64_t Enemy)
{
    const auto Entries = ReadNetworkObjects();
    uintptr_t Actor = 0;
    if (Entries)
        for (const NetworkObject& Entry : *Entries)
            if (Entry.Guid == Enemy)
                Actor = Entry.Object;
    if (!Actor || !Objects::IsA(Actor, "EnemyBase"))
        return {};
    const auto Cached = [Actor]() -> Known {
        const auto It = C.Tackles.find(Actor);
        if (It == C.Tackles.end() || !StillThere(It->second) ||
            Memory::ReadPointer(It->second.Object + Offsets::UObject::Outer).value_or(0) != Actor)
            return {};
        return It->second;
    };
    if (const Known Entry = Cached(); Entry.Object)
        return Entry;
    // Eine Suche über das ganze Objekt-Array kostet rund 10 ms, darum höchstens vier je Sekunde für alle Gegner zugleich.
    const ULONGLONG Now = GetTickCount64();
    if (Now < C.NextTackleScan)
        return {};
    C.NextTackleScan = Now + 250;
    if (!C.TackleClass)
        C.TackleClass = Objects::FindObject("TackleAbilityExecution", "/Script/DDSurvivors");
    if (!C.TackleClass)
        return {};
    C.Tackles.clear();
    const int32_t Count = Objects::Count();
    for (int32_t Index = 0; Index < Count; ++Index)
    {
        const uintptr_t Object = Objects::At(Index);
        if (!Object || Memory::ReadPointer(Object + Offsets::UObject::Class).value_or(0) != C.TackleClass)
            continue;
        if (const uintptr_t Outer = Memory::ReadPointer(Object + Offsets::UObject::Outer).value_or(0); Outer && C.Tackles.size() < 256)
            if (const Known Entry = Remember(Object); Entry.Object)
                C.Tackles[Outer] = Entry;
    }
    return Cached();
}

// Vorbedingungen der nativen Funktionen, sonst greifen sie ins Leere: Niagara-Komponente und Besitzer gesetzt.
bool LineReady(const Known& Execution)
{
    return StillThere(Execution) && Memory::ReadPointer(Execution.Object + LineTelegraph).value_or(0) &&
           Memory::ReadPointer(Execution.Object + ComponentOwner).value_or(0);
}

ClientState::Line* ClientLine(uint64_t Enemy, bool Create, ULONGLONG Now)
{
    for (auto& Line : C.Lines)
        if (Line.Enemy == Enemy)
            return &Line;
    if (!Create || C.Lines.size() >= 32)
        return nullptr;
    const Known Execution = FindExecution(Enemy);
    if (!Execution.Object)
    {
        if (C.Reported < 8 && ++C.Reported)
            Core::Log::Warn("Warnanzeigen: Sprint-Strich ohne Gegner oder Komponente beim Client");
        return nullptr;
    }
    C.Lines.push_back({Enemy, Execution, Now, false});
    return &C.Lines.back();
}

void HideLine(ClientState::Line& Line)
{
    if (Line.Active && StillThere(Line.Execution))
        Deactivate(Memory::ReadPointer(Line.Execution.Object + LineTelegraph).value_or(0));
    Line.Active = false;
}

void ApplyLine(const EnemyTelegraph& Value, ULONGLONG Now)
{
    ClientState::Line* Line = ClientLine(Value.Enemy, Value.Kind != TelegraphKind::LineEnd, Now);
    if (!Line)
    {
        if (Value.Kind == TelegraphKind::LineStart)
            ++Counted.Skipped;
        return;
    }
    Line->LastSeen = Now;
    if (Value.Kind == TelegraphKind::LineEnd)
    {
        HideLine(*Line);
        return;
    }
    if (!LineReady(Line->Execution))
    {
        if (C.Reported < 8 && ++C.Reported)
            Core::Log::Warn("Warnanzeigen: Sprint-Strich beim Client nicht vorbereitet (LineTelegraph fehlt)");
        return;
    }
    if (Value.Kind == TelegraphKind::LineStart || !Line->Active)
    {
        static const uintptr_t Activate = Objects::FindObject("ActivateLineTelegraph", "TackleAbilityExecution");
        if (!Activate)
            return;
        Objects::ProcessEvent(Line->Execution.Object, Activate, nullptr);
        Line->Active = true;
        ++Counted.Lines;
        if (C.Reported < 8 && ++C.Reported)
            Core::Log::Info("Warnanzeigen: Client zeigt Sprint-Strich");
    }
    if (Value.Kind != TelegraphKind::LineUpdate || Memory::Read<uint8_t>(Line->Execution.Object + TackleBlocked).value_or(1))
        return;
    static const uintptr_t EndPoint = Objects::FindObject("CalculateLineTelegraphEndPoint", "TackleAbilityExecution");
    if (!EndPoint)
        return;
    struct
    {
        double Target[3];
        float Percentage;
        uint8_t Pad[4];
    } Params{{Value.X, Value.Y, Value.Z}, Value.Percentage, {}};
    Objects::ProcessEvent(Line->Execution.Object, EndPoint, &Params);
}

bool CheckEnemyTelegraphs()
{
    return Code::Expect(At(SpawnCircleTelegraph), {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x70, 0x18, 0x57}, "SpawnCircleTelegraph") &&
           Code::Expect(At(ActivateLineTelegraph), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x70}, "ActivateLineTelegraph") &&
           Code::Expect(At(CalculateLineTelegraphEndPoint), {0x40, 0x53, 0x48, 0x81, 0xEC, 0xD0, 0x00, 0x00, 0x00}, "CalculateLineTelegraphEndPoint") &&
           Code::Expect(At(EndTackle), {0x40, 0x53, 0x57, 0x48, 0x83, 0xEC, 0x38}, "EndTackle") &&
           Code::Expect(At(ExecuteTackle), {0x48, 0x89, 0x5C, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x60}, "ExecuteTackle");
}

// Die Hooks melden nur beim Host. Beim Client laufen die Fähigkeiten nicht; ruft die Mod dort selbst ActivateLineTelegraph
// oder CalculateLineTelegraphEndPoint, entsteht keine Meldung.
bool ApplyEnemyTelegraphs(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(SpawnCircleTelegraph)), reinterpret_cast<void*>(&CircleHook),
                                reinterpret_cast<void**>(&OriginalCircle)) &&
           Core::Hooks::Install(reinterpret_cast<void*>(At(ActivateLineTelegraph)), reinterpret_cast<void*>(&ActivateLineHook),
                                reinterpret_cast<void**>(&OriginalActivateLine)) &&
           Core::Hooks::Install(reinterpret_cast<void*>(At(CalculateLineTelegraphEndPoint)), reinterpret_cast<void*>(&EndPointHook),
                                reinterpret_cast<void**>(&OriginalEndPoint)) &&
           Core::Hooks::Install(reinterpret_cast<void*>(At(EndTackle)), reinterpret_cast<void*>(&EndTackleHook),
                                reinterpret_cast<void**>(&OriginalEndTackle)) &&
           Core::Hooks::Install(reinterpret_cast<void*>(At(ExecuteTackle)), reinterpret_cast<void*>(&ExecuteTackleHook),
                                reinterpret_cast<void**>(&OriginalExecuteTackle));
}

// Je Patch die benutzten Adressen der Signaturtabelle, erzeugt von tools/patch_signatures.py sites.
constexpr uintptr_t EnemyTelegraphsSites[] = {SpawnCircleTelegraph, ActivateLineTelegraph, CalculateLineTelegraphEndPoint, EndTackle, ExecuteTackle};
const Patch List[] = {
    {"Warnanzeigen der Gegner beim Client", &CheckEnemyTelegraphs, &ApplyEnemyTelegraphs, EnemyTelegraphsSites},
};
}

std::span<const Patch> EnemyTelegraphPatches()
{
    return List;
}

// Client: Striche ohne Nachricht und abgelaufene Kreise ausblenden, Weltwechsel vergisst alles. Host: nichts.
void TickEnemyTelegraphs(NetRole Role)
{
    if (Role != NetRole::Client)
    {
        if (!C.Lines.empty() || !C.Circles.empty())
            ResetClient(0);
        if (Role == NetRole::Host)
            LogCounts("Host meldete");
        return;
    }
    LogCounts("Client zeigte");
    const uintptr_t World = CurrentWorld();
    if (World != C.World)
        ResetClient(World);
    const ULONGLONG Now = GetTickCount64();
    std::erase_if(C.Lines, [Now](ClientState::Line& Line) {
        if (!StillThere(Line.Execution))
            return true;
        if (Line.Active && Now - Line.LastSeen > LineTimeoutMs)
            HideLine(Line);
        return false;
    });
    std::erase_if(C.Circles, [Now](const ClientState::Circle& Circle) {
        if (!StillThere(Circle.Component))
            return true;
        if (Now < Circle.Until)
            return false;
        Deactivate(Circle.Component.Object);
        return true;
    });
}
}

namespace Game
{
std::vector<EnemyTelegraph> TakeEnemyTelegraphs()
{
    std::vector<EnemyTelegraph> Result(Patches::Queue.begin(), Patches::Queue.end());
    Patches::Queue.clear();
    return Result;
}

void ReceiveEnemyTelegraph(const EnemyTelegraph& Value)
{
    if (Patches::CurrentRole() != NetRole::Client)
        return;
    const uintptr_t World = Patches::CurrentWorld();
    if (!World)
        return;
    if (World != Patches::C.World)
        Patches::ResetClient(World);
    const ULONGLONG Now = GetTickCount64();
    if (Value.Kind == TelegraphKind::Circle)
        Patches::ShowCircle(Value, Now);
    else
        Patches::ApplyLine(Value, Now);
}
}
