// Host: Protokolliert jeden Treffer auf einen Gegner mit Verursacher und Lebenspunkten, um Treffer des Clients direkt zu belegen.
#include "Spike.h"

namespace Spike
{
namespace
{
// FDamageInformation: Damage, dann TWeakObjectPtr DamagedActor, DamagedAvatarActor, InstigatedBy, DamageCauser.
constexpr uintptr_t kInfoDamage = 0x00;
constexpr uintptr_t kInfoDamagedActor = 0x04;
constexpr uintptr_t kInfoInstigatedBy = 0x14;
constexpr uintptr_t kInfoDamageCauser = 0x1C;
constexpr uintptr_t kCharacterCommonAttributeSet = 0x700;
constexpr uintptr_t kAttributeHealthCurrent = 0xA0 + 0xC; // FGameplayAttributeData: VTable, BaseValue, CurrentValue
constexpr uintptr_t kActorOwner = 0x158;
constexpr uintptr_t kActorRemoteRole = 0x60;
constexpr uintptr_t kPlayerStatePawn = 0x320;
constexpr uint8_t kRoleAutonomousProxy = 2;

ProcessEventFn OriginalProcessEvent;
uintptr_t EnemyHitFunction = 0;

struct FHitStats
{
    int Hits = 0;
    int Kills = 0;
    double Damage = 0;
};
std::map<std::string, FHitStats> HitStats;
int TotalHits = 0;

uintptr_t ObjectAt(int32_t Index)
{
    if (Index <= 0 || Index >= Read<int32_t>(Base + kGObjects + 8))
        return 0;
    const uintptr_t Chunk = Read<uintptr_t>(Read<uintptr_t>(Base + kGObjects) + 8 * (Index >> 16));
    return Chunk ? Read<uintptr_t>(Chunk + (Index & 0xFFFF) * 0x18 + 8) : 0;
}

uintptr_t WeakObject(const uint8_t* Params, uintptr_t Offset)
{
    return ObjectAt(*reinterpret_cast<const int32_t*>(Params + Offset));
}

// Die Figur des Clients ist beim Host AutonomousProxy, die eigene des Hosts SimulatedProxy. Projektile und Waffen
// zaehlen ueber ihre Besitzerkette zum Spieler.
std::string ShooterOf(uintptr_t Actor)
{
    for (int Depth = 0; Actor && Depth < 6; ++Depth, Actor = Read<uintptr_t>(Actor + kActorOwner))
    {
        std::string Class = NameOf(Read<uintptr_t>(Actor + 0x10));
        // Das Spiel traegt als Ausloeser den PlayerState ein, dessen Figur entscheidet.
        if (Class.find("PlayerState") != std::string::npos && Read<uintptr_t>(Actor + kPlayerStatePawn))
        {
            Actor = Read<uintptr_t>(Actor + kPlayerStatePawn);
            Class = NameOf(Read<uintptr_t>(Actor + 0x10));
        }
        if (Class.find("DD_Character") != std::string::npos)
            return Read<uint8_t>(Actor + kActorRemoteRole) == kRoleAutonomousProxy ? "Client" : "Host";
    }
    return "Sonstige";
}

void LogHit(const uint8_t* Params)
{
    const float Damage = *reinterpret_cast<const float*>(Params + kInfoDamage);
    const uintptr_t Enemy = WeakObject(Params, kInfoDamagedActor);
    const uintptr_t Instigator = WeakObject(Params, kInfoInstigatedBy);
    const uintptr_t Causer = WeakObject(Params, kInfoDamageCauser);
    const std::string Shooter = ShooterOf(Instigator ? Instigator : Causer);
    FHitStats& Stats = HitStats[Shooter];
    ++Stats.Hits;
    Stats.Damage += Damage;
    ++TotalHits;
    // Das Spiel meldet den Treffer nach dem Abzug, die Lebenspunkte sind also schon die neuen.
    const uintptr_t Attributes = Enemy ? Read<uintptr_t>(Enemy + kCharacterCommonAttributeSet) : 0;
    const float Health = Attributes ? Read<float>(Attributes + kAttributeHealthCurrent) : -1.0f;
    if (Attributes && Health <= 0.0f)
        ++Stats.Kills;
    if (Stats.Hits <= 5)
    {
        char Line[512];
        sprintf_s(Line, "Treffer %s: %.1f Schaden an %s (Leben %.1f), Ausloeser %s, Verursacher %s", Shooter.c_str(), Damage,
                  Enemy ? NameOf(Enemy).c_str() : "-", Health, Instigator ? NameOf(Instigator).c_str() : "-",
                  Causer ? NameOf(Read<uintptr_t>(Causer + 0x10)).c_str() : "-");
        Log(Line);
    }
    if (TotalHits % 10 == 0)
    {
        std::string Line = "Treffer gesamt " + std::to_string(TotalHits) + ":";
        for (const auto& [Name, Entry] : HitStats)
            Line += " " + Name + " " + std::to_string(Entry.Hits) + " (" + std::to_string(static_cast<int>(Entry.Damage)) + " Schaden, " +
                    std::to_string(Entry.Kills) + " Kills)";
        Log(Line);
    }
}

void __fastcall HitsProcessEvent(uintptr_t Object, uintptr_t Function, void* Params)
{
    if (Params && (Function == EnemyHitFunction || NameOf(Function) == "OnEnemyHit"))
        LogHit(static_cast<const uint8_t*>(Params));
    OriginalProcessEvent(Object, Function, Params);
}
}

// Haengt sich in ProcessEvent der Klasse UDDGameStatsSubsystem. Das Spiel meldet dort jeden Treffer auf einen Gegner.
void TickHits()
{
    if (OriginalProcessEvent)
        return;
    // Das Spiel kann eine Blueprint-Unterklasse verwenden, also die Klassenkette pruefen.
    uintptr_t Stats = 0;
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    for (int32_t i = 0; i < Count && !Stats; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x10) != 0)
            continue;
        for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class && !Stats; Class = Read<uintptr_t>(Class + 0x40))
            if (NameOf(Class) == "DDGameStatsSubsystem")
                Stats = Object;
    }
    EnemyHitFunction = FindObject("OnEnemyHit", "DDGameStatsSubsystem");
    if (!Stats || !EnemyHitFunction)
        return;
    auto* Slot = &Read<ProcessEventFn*>(Stats)[kProcessEventIndex];
    DWORD Old = 0;
    VirtualProtect(Slot, sizeof(*Slot), PAGE_READWRITE, &Old);
    OriginalProcessEvent = *Slot;
    *Slot = HitsProcessEvent;
    VirtualProtect(Slot, sizeof(*Slot), Old, &Old);
    Log("Treffer-Zaehler: aktiv");
}
}
