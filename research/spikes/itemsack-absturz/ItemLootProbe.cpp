// Nur eigene geschuetzte Testsitzung, Build 26187885. Kein Hot-Unload.
#include "../a-connect/Spike.h"

namespace
{
using namespace Spike;
WNDPROC Previous = nullptr;
constexpr UINT Message = WM_APP + 195;

std::string SafeName(uintptr_t Object) { return Object ? NameOf(Object) : "null"; }

bool IsA(uintptr_t Object, const char* Name)
{
    if (!Object) return false;
    for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class; Class = Read<uintptr_t>(Class + 0x40))
        if (NameOf(Class) == Name) return true;
    return false;
}

bool Invoke(uintptr_t Object, const char* Owner, const char* Name, void* Params)
{
    const uintptr_t Function = FindObject(Name, Owner);
    if (!Object || !Function) return false;
    Read<ProcessEventFn*>(Object)[kProcessEventIndex](Object, Function, Params);
    return true;
}

void Perform(int Action)
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t State = World ? Read<uintptr_t>(World + kWorldGameState) : 0;
    if (!State) return;
    const uintptr_t Driver = Read<uintptr_t>(World + kWorldNetDriver);
    const bool Client = Driver && Read<uintptr_t>(Driver + kDriverServerConnection);
    if (Action != 0 && Client && Action != 5)
    {
        Log("abgelehnt: Spawnen/Vergabe nur beim Host");
        return;
    }
    const int Player = Action == 2 || Action == 4 ? 1 : 0;
    const uintptr_t Players = Read<uintptr_t>(State + kGameStatePlayerArray);
    if (Read<int32_t>(State + kGameStatePlayerArray + 8) <= Player) return;
    const uintptr_t PS = Read<uintptr_t>(Players + 8 * Player);
    uintptr_t Pawn = 0;
    if (!Invoke(PS, "PlayerState", "GetPawn", &Pawn) || !Pawn) return;
    const uintptr_t Level = Read<uintptr_t>(World + 0x30);
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    uintptr_t Loot = 0, Data = 0, Attractor = 0;
    // Das Funktionsbibliothek-CDO traegt RF_ClassDefaultObject und wird in der Actor-Schleife ausgelassen.
    const uintptr_t Library = FindObject("Default__LootBlueprintFunctionLibrary", nullptr);
    const uintptr_t Template = FindObject("BP_ItemLoot_C", nullptr);
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xffff) * 0x18 + 8) : 0;
        if (!Object || Read<uint32_t>(Object + 8) & 0x30) continue;
        const auto Name = NameOf(Object);
        if (Read<uintptr_t>(Object + 0x20) == Pawn && IsA(Object, "LootAttractor")) Attractor = Object;
        if (Read<uintptr_t>(Object + 0x20) == Level && IsA(Object, "ItemLoot") &&
            !(Read<uint8_t>(Object + 0x58) & 0x10))
        {
            Log("loot=" + Name + " role=" + std::to_string(Read<uint8_t>(Object + 0x168)) +
                " data=" + SafeName(Read<uintptr_t>(Object + 0x2E0)));
            Loot = Object;
        }
        if (!Data && Read<uintptr_t>(Object + 0x20) == PS && IsA(Object, "InventoryComponent"))
        {
            const uintptr_t Items = Read<uintptr_t>(Object + 0x120);
            if (Items && Read<int32_t>(Object + 0x128) > 0)
            {
                const uintptr_t Item = Read<uintptr_t>(Items);
                if (Item) Data = Read<uintptr_t>(Item + 0xE0);
            }
        }
    }
    if (Action == 0) return;
    if (Action == 3 || Action == 4 || Action == 5)
    {
        if (!Loot || !Attractor) { Log("kein Loot/Attractor"); return; }
        Log("invoke-loot player=" + std::to_string(Player) + " client=" + std::to_string(Client));
        Invoke(Loot, "ItemLoot", "Loot", &Attractor);
        Log("invoke-loot returned");
        return;
    }
    if (!Library || !Template || !Data)
    {
        Log("Spawn-Voraussetzung fehlt: library=" + std::to_string(Library != 0) +
            " template=" + std::to_string(Template != 0) + " data=" + std::to_string(Data != 0));
        return;
    }
    struct Vector { double X, Y, Z; };
    Vector Position{};
    Invoke(Pawn, "Actor", "K2_GetActorLocation", &Position);
    Position.X += 300;
    struct SpawnParams
    {
        uintptr_t World, Data;
        bool Broken; uint8_t Pad[7]{};
        Vector Position;
        int32_t Number = 1; float Radius = 0;
        uintptr_t Template;
        float Arc = 30; uint32_t Padding = 0;
        uintptr_t Result = 0;
    } Params{World, Data, false, {}, Position, 1, 0, Template};
    static_assert(sizeof(Params) == 0x50);
    Invoke(Library, "LootBlueprintFunctionLibrary", "SpawnSpecificItemLoot", &Params);
    Log("spawn player=" + std::to_string(Player) + " data=" + SafeName(Data) + " result=" + SafeName(Params.Result));
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
    Base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    wchar_t Temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, Temp);
    const auto Root = std::wstring(Temp) + L"dditemsack\\";
    CreateDirectoryW(Root.c_str(), nullptr);
    const auto Id = std::to_wstring(GetCurrentProcessId());
    SetLogPath(Root + Id + L".log");
    const auto Command = Root + Id + L".cmd";
    HWND Window = nullptr;
    EnumWindows(FindWindow, reinterpret_cast<LPARAM>(&Window));
    if (!Window) return 1;
    Previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(Window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProc)));
    if (!Previous) return 2;
    Log("bereit: 0=lesen, 1/2=Host spawnt bei P1/P2, 3/4=Host Loot fuer P1/P2, 5=Client Loot negativ");
    for (;;)
    {
        Sleep(200);
        FILE* File = nullptr;
        if (_wfopen_s(&File, Command.c_str(), L"r") != 0 || !File) continue;
        int Action = -1;
        const int Parsed = fscanf_s(File, "%d", &Action);
        fclose(File);
        DeleteFileW(Command.c_str());
        if (Parsed == 1 && Action >= 0 && Action <= 5) SendMessageW(Window, Message, Action, 0);
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
