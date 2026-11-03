// Eigener gezielter Laufzeit-Trace, keine Blueprint-Implementierung.
// Nur eigene geschuetzte Sitzung, feste Karte, kein Hot-Unload/Mapwechsel.
#include <windows.h>
#include <cstdio>
#include <array>
#include <string>
#include <utility>
#include "SDK/DDSurvivors_classes.hpp"
#include "SDK/BP_Effect_Add_ImpactEffect_Template_classes.hpp"
#include "SDK/WP_UI_classes.hpp"

namespace
{
using Exec = void(__fastcall*)(SDK::UObject*, void*, void*);
struct Hook { SDK::UFunction* Function{}; Exec Original{}; std::string Name; unsigned Calls{}; };
std::array<Hook, 64> Hooks;
size_t HookCount;
thread_local unsigned Depth;
std::wstring LogPath;
WNDPROC Previous;
constexpr UINT Message = WM_APP + 193;

void Log(const std::string& Text)
{
    FILE* File = nullptr;
    if (_wfopen_s(&File, LogPath.c_str(), L"a") == 0 && File)
    {
        fprintf(File, "%llu %s\n", GetTickCount64(), Text.c_str());
        fclose(File);
    }
}

bool Live(SDK::UObject* Object)
{
    return Object && !(static_cast<unsigned>(Object->Flags) & 0x30);
}

bool Relevant(const std::string& Owner, const std::string& Name)
{
    return Owner == "WP_UI_C" || Owner == "BP_PowerUpCrate_C" || Owner == "BP_Airdrop_Lootable_C" ||
        Owner == "BP_Projectile_C" || Owner == "BP_Effect_Add_ImpactEffect_Template_C" ||
        (Owner == "Actor" && (Name == "HasAuthority" || Name == "SetActorScale3D")) ||
        (Owner == "LootSpot" && Name == "SpawnLoot") ||
        (Owner == "InventoryItemInstance" && (Name == "AddImpactActor" || Name == "RemoveImpactActor")) ||
        (Owner == "RewardChest" && Name == "OpenAirdropChoices") ||
        (Owner == "FMODBlueprintStatics" && Name == "PlayEventAtLocation");
}

template <size_t I> void __fastcall Observe(SDK::UObject* Object, void* Stack, void* Result)
{
    auto& H = Hooks[I];
    const auto Class = Object ? Object->Class->GetName() : "null";
    const bool Selected = Depth || Class.find("PowerUpCrate") != std::string::npos ||
        Class.find("Airdrop_Lootable") != std::string::npos ||
        H.Name.rfind("BP_", 0) == 0 || H.Name.rfind("WP_UI_C.", 0) == 0;
    if (Selected)
    {
        ++H.Calls;
        if (H.Calls <= 100)
            Log("enter " + H.Name + " class=" + Class + " depth=" + std::to_string(Depth));
        ++Depth;
    }
    H.Original(Object, Stack, Result); // Genau eine unveraenderte Weiterleitung.
    if (Selected)
    {
        --Depth;
        if (H.Name == "Actor.HasAuthority" && Result && H.Calls <= 100)
            Log(std::string("authority-result=") + (*static_cast<bool*>(Result) ? "true" : "false"));
    }
}

template <size_t... I> auto Stubs(std::index_sequence<I...>)
{
    return std::array<Exec, sizeof...(I)>{&Observe<I>...};
}
const auto Forwarders = Stubs(std::make_index_sequence<64>{});

void Install()
{
    if (HookCount)
        return;
    for (int i = 0; i < SDK::UObject::GObjects->Num() && HookCount < Hooks.size(); ++i)
    {
        auto* Object = SDK::UObject::GObjects->GetByIndex(i);
        if (!Object || Object->Class->GetName() != "Function" || !Object->Outer)
            continue;
        const auto Owner = Object->Outer->GetName(), Name = Object->GetName();
        if (!Relevant(Owner, Name))
            continue;
        auto* Slot = reinterpret_cast<Exec*>(reinterpret_cast<uintptr_t>(Object) + 0xD8);
        DWORD Protection = 0;
        if (!VirtualProtect(Slot, sizeof(*Slot), PAGE_READWRITE, &Protection))
            continue;
        Hooks[HookCount] = {static_cast<SDK::UFunction*>(Object), *Slot, Owner + "." + Name, 0};
        InterlockedExchangePointer(reinterpret_cast<void* volatile*>(Slot),
            reinterpret_cast<void*>(Forwarders[HookCount]));
        DWORD Ignored = 0;
        VirtualProtect(Slot, sizeof(*Slot), Protection, &Ignored);
        Log("hook " + Owner + "." + Name);
        ++HookCount;
    }
    Log("hooks=" + std::to_string(HookCount));
}

SDK::UObject* Find(const char* ClassName, bool WantClass = false)
{
    for (int i = 0; i < SDK::UObject::GObjects->Num(); ++i)
    {
        auto* Object = SDK::UObject::GObjects->GetByIndex(i);
        if (!Object)
            continue;
        if (WantClass)
        {
            if (Object->GetName() == ClassName && Object->IsA(SDK::UClass::StaticClass()))
                return Object;
            continue;
        }
        if (!Live(Object) || Object->Class->GetName() != ClassName)
            continue;
        if (Object->IsA(SDK::AActor::StaticClass()) &&
            static_cast<SDK::AActor*>(Object)->GetLevel() != SDK::UWorld::GetWorld()->PersistentLevel)
            continue;
        return Object;
    }
    return nullptr;
}

bool Invoke(SDK::UObject* Object, const char* Owner, const char* Name, void* Params)
{
    if (!Object)
        return false;
    auto* Function = Object->Class->GetFunction(Owner, Name);
    if (!Function)
        return false;
    Object->ProcessEvent(Function, Params);
    return true;
}

void Perform(int Action)
{
    auto* World = SDK::UWorld::GetWorld();
    if (!World || !World->GameState)
        return;
    Install();
    auto* Controller = SDK::UGameplayStatics::GetPlayerController(World, 0);
    auto* Pawn = SDK::UGameplayStatics::GetPlayerPawn(World, 0);
    Log("action=" + std::to_string(Action));
    if (Action >= 6 && Action <= 8)
    {
        const char* Name = Action == 6 ? "BP_PowerUpCrate_C" :
            Action == 7 ? "BP_Airdrop_Lootable_C" : "BP_Projectile_C";
        auto* Class = static_cast<SDK::UClass*>(Find(Name, true));
        if (!Class || !Pawn) { Log("Spawnvoraussetzung fehlt"); return; }
        SDK::FTransform Transform{};
        Transform.Rotation.W = 1;
        Transform.Scale3D = {1, 1, 1};
        Transform.Translation = Pawn->K2_GetActorLocation();
        Transform.Translation.X += 300;
        Transform.Translation.Z += 50;
        const auto ScaleMethod = static_cast<SDK::ESpawnActorScaleMethod>(0);
        auto* Actor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(World, Class,
            Transform, SDK::ESpawnActorCollisionHandlingMethod::AlwaysSpawn, nullptr, ScaleMethod);
        if (Actor) Actor = SDK::UGameplayStatics::FinishSpawningActor(Actor, Transform, ScaleMethod);
        Log(std::string("controlled-spawn ") + Name + " success=" + std::to_string(Actor != nullptr));
    }
    else if (Action == 1 || Action == 9)
    {
        auto* Actor = static_cast<SDK::AActor*>(Find("BP_PowerUpCrate_C"));
        if (!Actor) { Log("keine Kiste im aktuellen PersistentLevel"); return; }
        Log("crate role=" + std::to_string(static_cast<int>(Actor->Role)));
        if (Action == 9)
        {
            struct DamageParams { float Damage; unsigned Padding; SDK::UObject* Type;
                SDK::AController* Instigator; SDK::AActor* Causer; } Params{1000, 0, nullptr, Controller, Pawn};
            static_assert(sizeof(Params) == 32);
            Log("controlled ReceiveAnyDamage called=" + std::to_string(
                Invoke(Actor, "BP_PowerUpCrate_C", "ReceiveAnyDamage", &Params)));
            return;
        }
        const float Damage = SDK::UGameplayStatics::ApplyDamage(Actor, 1000, Controller, Pawn, nullptr);
        Log("ApplyDamage return=" + std::to_string(Damage));
    }
    else if (Action == 2 || Action == 11)
    {
        auto* Actor = static_cast<SDK::AActor*>(Find("BP_Projectile_C"));
        if (!Actor) { Log("kein Projektil"); return; }
        const auto Before = Actor->GetActorScale3D();
        float Size = Action == 2 ? 2.0f : 3.0f;
        const bool Called = Invoke(Actor, "BP_Projectile_C", "SetBulletSize", &Size);
        const auto After = Actor->GetActorScale3D();
        Log("projectile called=" + std::to_string(Called) + " scale-before=" +
            std::to_string(Before.X) + "," + std::to_string(Before.Y) + "," + std::to_string(Before.Z) +
            " scale-after=" + std::to_string(After.X) + "," + std::to_string(After.Y) + "," + std::to_string(After.Z));
    }
    else if (Action == 3)
    {
        auto* Actor = static_cast<SDK::AActor*>(Find("BP_Airdrop_Lootable_C"));
        if (!Actor) { Log("kein Airdrop; kein Zustellungsnachweis"); return; }
        Log("airdrop role=" + std::to_string(static_cast<int>(Actor->Role)) +
            " replicates=" + std::to_string(Actor->bReplicates));
        Log("StartFallDown called=" + std::to_string(Invoke(Actor, "BP_Airdrop_Lootable_C", "StartFallDown", nullptr)));
    }
    else if (Action == 10)
    {
        auto* Actor = static_cast<SDK::AActor*>(Find("BP_Airdrop_Lootable_C"));
        if (!Actor || !Pawn || !Pawn->IsA(SDK::ADDCharacter::StaticClass()))
        { Log("kein gueltiger Airdrop-Oeffnungskontext"); return; }
        auto* Character = static_cast<SDK::ADDCharacter*>(Pawn);
        Log("controlled OnPlayCrateOpenImpl called=" + std::to_string(
            Invoke(Actor, "BP_Airdrop_Lootable_C", "OnPlayCrateOpenImpl", &Character)));
    }
    else if (Action == 12)
    {
        auto* UI = static_cast<SDK::UWP_UI_C*>(Find("WP_UI_C"));
        if (!UI) { Log("keine lokale HUD"); return; }
        int Before = UI->intseconds, Seconds = 42;
        const bool Called = Invoke(UI, "WP_UI_C", "OnSecondChanged", &Seconds);
        Log("HUD called=" + std::to_string(Called) + " seconds-before=" + std::to_string(Before) +
            " seconds-after=" + std::to_string(UI->intseconds));
        Invoke(UI, "WP_UI_C", "OnSecondChanged", &Before);
    }
    else if (Action == 4)
    {
        auto* Class = static_cast<SDK::UClass*>(Find("BP_Effect_Add_Explosive_Damage_C", true));
        auto* Inventory = Controller ? SDK::UInventoryEffectHelperLibrary::GetInventoryComponent(Controller->PlayerState) : nullptr;
        if (!Class || !Inventory || Inventory->MyItems.Num() == 0 || !Inventory->MyItems[0] ||
            !Pawn || !Pawn->IsA(SDK::ADDCharacter::StaticClass()))
        { Log("kein gueltiger Effektkontext; keine Apply-Probe"); return; }
        auto* Effect = SDK::UGameplayStatics::SpawnObject(Class, Inventory);
        if (!Effect) { Log("kein Testeffekt"); return; }
        SDK::FItemEffectContext Context{};
        Context.Character = static_cast<SDK::ADDCharacter*>(Pawn);
        Context.Item = Inventory->MyItems[0];
        Context.InventoryComponent = Inventory;
        Log("controlled effect ApplyEffect");
        Invoke(Effect, "BP_Effect_Add_ImpactEffect_Template_C", "ApplyEffect", &Context);
        Log("controlled effect RemoveEffect");
        Invoke(Effect, "BP_Effect_Add_ImpactEffect_Template_C", "RemoveEffect", &Context);
        auto* TypedEffect = static_cast<SDK::UBP_Effect_Add_ImpactEffect_Template_C*>(Effect);
        Log("effect default ApplyToSelf=" + std::to_string(TypedEffect->ApplyToSelf));
        TypedEffect->ApplyToSelf = true; // Nur das soeben erzeugte Testobjekt.
        Log("controlled effect ApplyToSelf=true ApplyEffect");
        Invoke(Effect, "BP_Effect_Add_ImpactEffect_Template_C", "ApplyEffect", &Context);
        Log("controlled effect ApplyToSelf=true RemoveEffect");
        Invoke(Effect, "BP_Effect_Add_ImpactEffect_Template_C", "RemoveEffect", &Context);
        TypedEffect->ApplyToSelf = false;
        struct OtherParams { SDK::FItemEffectContext Context; SDK::UInventoryItemInstance* Other; } Other{Context, Context.Item};
        Log("controlled effect ApplyToOther");
        Invoke(Effect, "BP_Effect_Add_ImpactEffect_Template_C", "ApplyEffectToOther", &Other);
        Log("controlled effect RemoveFromOther");
        Invoke(Effect, "BP_Effect_Add_ImpactEffect_Template_C", "RemoveEffectFromOther", &Other);
    }
    else if (Action == 5)
    {
        for (size_t i = 0; i < HookCount; ++i)
            Log("count " + Hooks[i].Name + "=" + std::to_string(Hooks[i].Calls));
        for (const char* Name : {"BP_PowerUpCrate_C", "BP_Airdrop_Lootable_C", "BP_Projectile_C"})
        {
            auto* Actor = static_cast<SDK::AActor*>(Find(Name));
            if (!Actor) { Log(std::string("survey absent ") + Name); continue; }
            const auto Scale = Actor->GetActorScale3D();
            Log(std::string("survey ") + Name + " role=" + std::to_string(static_cast<int>(Actor->Role)) +
                " replicates=" + std::to_string(Actor->bReplicates) + " scale=" +
                std::to_string(Scale.X) + "," + std::to_string(Scale.Y) + "," + std::to_string(Scale.Z));
        }
    }
}

LRESULT CALLBACK WindowProc(HWND Window, UINT Msg, WPARAM W, LPARAM L)
{
    if (Msg == Message) { Perform(static_cast<int>(W)); return 0; }
    return CallWindowProcW(Previous, Window, Msg, W, L);
}
BOOL CALLBACK FindWindow(HWND Window, LPARAM Result)
{
    DWORD Process = 0;
    GetWindowThreadProcessId(Window, &Process);
    wchar_t Class[64]{};
    GetClassNameW(Window, Class, 64);
    if (Process == GetCurrentProcessId() && wcscmp(Class, L"UnrealWindow") == 0)
    { *reinterpret_cast<HWND*>(Result) = Window; return FALSE; }
    return TRUE;
}
DWORD WINAPI Worker(void*)
{
    wchar_t Temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, Temp);
    const auto Root = std::wstring(Temp) + L"ddblueprint\\";
    CreateDirectoryW(Root.c_str(), nullptr);
    const auto Id = std::to_wstring(GetCurrentProcessId());
    LogPath = Root + Id + L".log";
    const auto Command = Root + Id + L".cmd";
    HWND Window = nullptr;
    EnumWindows(FindWindow, reinterpret_cast<LPARAM>(&Window));
    if (!Window) return 1;
    Previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(Window, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&WindowProc)));
    if (!Previous) return 2;
    Log("bereit; 0=Trace, 1=Kistenschaden, 2=Projektilgroesse, 3=Airdrop-Fall, 4=Itemeffekt, 5=Survey, 6/7/8=Testspawn, 9=Damage-Event direkt, 10=Airdrop-Oeffnungs-Event, 11=Projektilgroesse3, 12=HUD-Sekunde");
    for (;;)
    {
        Sleep(200);
        FILE* File = nullptr;
        if (_wfopen_s(&File, Command.c_str(), L"r") != 0 || !File) continue;
        int Action = -1;
        const int Read = fscanf_s(File, "%d", &Action);
        fclose(File);
        DeleteFileW(Command.c_str());
        if (Read == 1 && Action >= 0 && Action <= 12)
            SendMessageW(Window, Message, static_cast<WPARAM>(Action), 0);
    }
}
}
BOOL WINAPI DllMain(HINSTANCE Module, DWORD Reason, void*)
{
    if (Reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(Module);
        const HANDLE Thread = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        if (Thread) CloseHandle(Thread);
    }
    return TRUE;
}
