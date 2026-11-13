// Protokolliert RPCs der Spielfigur und die ersten Aufrufe der Run-Funktionen.
#include "Spike.h"

namespace Spike
{
namespace
{
constexpr uintptr_t kFunctionFlags = 0xB0;
constexpr uintptr_t kFunctionExec = 0xD8;

ProcessEventFn OriginalProcessEvent;
std::map<std::string, int> RpcCounts;

// RPCs laufen auf beiden Seiten durch ProcessEvent. Zaehlt alles, was nach Client- oder Server-RPC aussieht.
void __fastcall TraceProcessEvent(uintptr_t Object, uintptr_t Function, void* Params)
{
    const std::string Name = NameOf(Function);
    if (Name.find("Server") != std::string::npos || Name.find("Client") != std::string::npos)
    {
        const int Count = ++RpcCounts[Name];
        if (Count == 1 || Count % 100 == 0)
            Log("RPC " + Name + " #" + std::to_string(Count));
    }
    OriginalProcessEvent(Object, Function, Params);
}

using ExecFn = void(__fastcall*)(void*, void*, void*);

struct TracedFunction
{
    ExecFn Original;
    std::string Name;
    int Calls;
};

std::vector<TracedFunction> TracedFunctions;

void __fastcall TraceExec(void* Context, void* Stack, void* Result, size_t Index)
{
    TracedFunction& Function = TracedFunctions[Index];
    if (++Function.Calls <= 5)
        Log(std::to_string(GetTickCount64()) + " " + Function.Name);
    Function.Original(Context, Stack, Result);
}
}

// Tauscht den ProcessEvent-Eintrag in der VTable der Spielfigur.
void TraceCharacterRpcs()
{
    const uintptr_t Character = FindInstance("BP_DD_Character_C");
    if (!Character || OriginalProcessEvent)
        return;
    auto* Slot = &Read<ProcessEventFn*>(Character)[kProcessEventIndex];
    DWORD Old = 0;
    VirtualProtect(Slot, sizeof(*Slot), PAGE_READWRITE, &Old);
    OriginalProcessEvent = *Slot;
    *Slot = TraceProcessEvent;
    VirtualProtect(Slot, sizeof(*Slot), Old, &Old);
    Log("RPC-Zaehler: aktiv");
}

// Blueprint-Aufrufe nativer Funktionen laufen ueber UFunction::Func. Pro Funktion leitet ein Stub durch TraceExec.
void TraceRunFlow()
{
    const char* const Owners[] = {
        "DDMissionGameInstance", "LoadingSubSystem", "RunPersistencyManager", "DDMissionGameMode", "DDMissionGameState",
        "DDBlueprintHelperFunctions", "Bus", "BusPanel", "ExtractionPointBase", "MissionGenerationSubsystem",
        "CharacterSelectionManager", "CharacterSelectPanel", "EndOfDayUpgradeManager", "RoundEndPanel", "MapGenerator",
        "LocalCoopSubsystem", "PauseMenuWidget"};
    if (!TracedFunctions.empty())
        return;
    TracedFunctions.reserve(4096);

    auto* Stub = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096 * 24, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    for (int32_t i = 0; i < Count && TracedFunctions.size() < 4096; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        const uintptr_t Class = Object ? Read<uintptr_t>(Object + 0x10) : 0;
        if (!Class || NameOf(Class) != "Function" || !(Read<uint32_t>(Object + kFunctionFlags) & 0x400))
            continue;
        const uintptr_t Outer = Read<uintptr_t>(Object + 0x20);
        const std::string OuterName = Outer ? NameOf(Outer) : std::string();
        bool Wanted = false;
        for (const char* Owner : Owners)
            Wanted = Wanted || OuterName == Owner;
        if (!Wanted)
            continue;

        auto& Func = *reinterpret_cast<ExecFn*>(Object + kFunctionExec);
        char Address[32]{};
        sprintf_s(Address, "%llX", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(Func) - Base));
        if (NameOf(Object).find("Load") != std::string::npos || NameOf(Object).find("Travel") != std::string::npos)
            Log("Exec " + NameOf(Object) + " " + Address);
        const uint64_t Index = TracedFunctions.size();
        const uint64_t Hook = reinterpret_cast<uint64_t>(&TraceExec);
        TracedFunctions.push_back({Func, OuterName + "." + NameOf(Object), 0});
        Stub[0] = 0x49; Stub[1] = 0xB9;                     // mov r9,Index
        memcpy(Stub + 2, &Index, 8);
        Stub[10] = 0x48; Stub[11] = 0xB8;                   // mov rax,TraceExec
        memcpy(Stub + 12, &Hook, 8);
        Stub[20] = 0xFF; Stub[21] = 0xE0;                   // jmp rax
        Func = reinterpret_cast<ExecFn>(Stub);
        Stub += 24;
    }
    Log("Ablauf-Trace: " + std::to_string(TracedFunctions.size()) + " Funktionen");
}

namespace
{
// vfx-sound: native Sound- und Effektfunktionen, gezaehlt je Aufrufer (Klasse und Funktion aus dem FFrame).
constexpr uintptr_t kFrameNode = 0x10, kFrameObject = 0x18;

struct FxFunction
{
    ExecFn Original;
    std::string Name;
};

struct FxKey
{
    size_t Index;
    uintptr_t CallerClass;
    uintptr_t CallerNode;
    bool operator<(const FxKey& Other) const
    {
        if (Index != Other.Index)
            return Index < Other.Index;
        if (CallerClass != Other.CallerClass)
            return CallerClass < Other.CallerClass;
        return CallerNode < Other.CallerNode;
    }
};

std::vector<FxFunction> FxFunctions;
// Namen beim ersten Aufruf aufloesen: Nach einer Reise koennen Aufruferklassen schon freigegeben sein.
std::map<FxKey, std::pair<int, std::string>> FxCounts;
SRWLOCK FxLock = SRWLOCK_INIT;

void __fastcall FxExec(void* Context, void* Stack, void* Result, size_t Index)
{
    const uintptr_t Frame = reinterpret_cast<uintptr_t>(Stack);
    const uintptr_t Caller = Frame ? Read<uintptr_t>(Frame + kFrameObject) : 0;
    const FxKey Key{Index, Caller ? Read<uintptr_t>(Caller + 0x10) : 0, Frame ? Read<uintptr_t>(Frame + kFrameNode) : 0};
    AcquireSRWLockExclusive(&FxLock);
    auto& Entry = FxCounts[Key];
    if (Entry.second.empty())
        Entry.second = FxFunctions[Index].Name + " <- " + (Key.CallerClass ? NameOf(Key.CallerClass) : std::string("?")) + "." +
                       (Key.CallerNode ? NameOf(Key.CallerNode) : std::string("?"));
    ++Entry.first;
    ReleaseSRWLockExclusive(&FxLock);
    FxFunctions[Index].Original(Context, Stack, Result);
}

bool WantedFx(const std::string& Owner, const std::string& Name)
{
    if (Owner == "FMODBlueprintStatics" || Owner == "FMODAudioComponent" || Owner == "NiagaraFunctionLibrary")
        return true;
    if (Owner == "NiagaraComponent" || Owner == "ParticleSystemComponent")
        return Name == "Activate" || Name == "ActivateSystem" || Name == "ResetSystem";
    if (Owner == "GameplayStatics")
        return Name.find("Sound") != std::string::npos || Name.find("Emitter") != std::string::npos;
    return Owner.find("Pool") != std::string::npos;
}

void InstallFxCounter()
{
    auto* Stub = static_cast<uint8_t*>(VirtualAlloc(nullptr, 1024 * 24, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    FxFunctions.reserve(1024);
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    for (int32_t i = 0; i < Count && FxFunctions.size() < 1024; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        const uintptr_t Class = Object ? Read<uintptr_t>(Object + 0x10) : 0;
        if (!Class || NameOf(Class) != "Function" || !(Read<uint32_t>(Object + kFunctionFlags) & 0x400))
            continue;
        const uintptr_t Outer = Read<uintptr_t>(Object + 0x20);
        const std::string OuterName = Outer ? NameOf(Outer) : std::string();
        const std::string Name = NameOf(Object);
        if (!WantedFx(OuterName, Name))
            continue;
        auto& Func = *reinterpret_cast<ExecFn*>(Object + kFunctionExec);
        const uint64_t Index = FxFunctions.size();
        const uint64_t Hook = reinterpret_cast<uint64_t>(&FxExec);
        FxFunctions.push_back({Func, OuterName + "." + Name});
        Stub[0] = 0x49; Stub[1] = 0xB9;                     // mov r9,Index
        memcpy(Stub + 2, &Index, 8);
        Stub[10] = 0x48; Stub[11] = 0xB8;                   // mov rax,FxExec
        memcpy(Stub + 12, &Hook, 8);
        Stub[20] = 0xFF; Stub[21] = 0xE0;                   // jmp rax
        Func = reinterpret_cast<ExecFn>(Stub);
        Stub += 24;
    }
    Log("Fx-Zaehler: " + std::to_string(FxFunctions.size()) + " Funktionen");
}

// Sounds aus C++ laufen nicht ueber UFunction::Func. Jeder FMOD-Sound startet aber ueber EventInstance::start aus fmodstudio.dll,
// gezaehlt je Ereignispfad ueber den Importeintrag der Exe.
using StartFn = int(__fastcall*)(void*);
using GetDescriptionFn = int(__fastcall*)(void*, void**);
using GetPathFn = int(__fastcall*)(void*, char*, int, int*);
StartFn OriginalStart;
GetDescriptionFn GetDescription;
GetPathFn GetPath;
// Je Ereignispfad, beim Start aufgeloest: Beschreibungen gehoeren zu Banks, die beim Kartenwechsel entladen werden.
std::map<std::string, int> SoundCounts;
// "!fx stack <Teil des Pfads>": fuer passende Sounds zusaetzlich die Aufrufkette (RVAs der Exe) zaehlen.
std::string StackFilter;

std::string CallStack()
{
    void* Frames[10]{};
    const USHORT Count = RtlCaptureStackBackTrace(2, 10, Frames, nullptr);
    std::string Text;
    for (USHORT i = 0; i < Count; ++i)
    {
        const uintptr_t Address = reinterpret_cast<uintptr_t>(Frames[i]);
        HMODULE Module = nullptr;
        char File[MAX_PATH]{}, Part[96]{};
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<const char*>(Address), &Module) &&
            GetModuleFileNameA(Module, File, MAX_PATH))
        {
            const char* Name = strrchr(File, '\\');
            sprintf_s(Part, " %s+%llX", reinterpret_cast<uintptr_t>(Module) == Base ? "exe" : (Name ? Name + 1 : File),
                      static_cast<unsigned long long>(Address - reinterpret_cast<uintptr_t>(Module)));
        }
        else
            sprintf_s(Part, " ?");
        Text += Part;
    }
    return Text;
}

int __fastcall CountedStart(void* Instance)
{
    void* Description = nullptr;
    char Path[256]{};
    int Length = 0;
    if (GetDescription && GetDescription(Instance, &Description) == 0 && Description &&
        GetPath(Description, Path, sizeof(Path), &Length) == 0)
    {
        AcquireSRWLockExclusive(&FxLock);
        ++SoundCounts[Path];
        if (!StackFilter.empty() && strstr(Path, StackFilter.c_str()))
            ++SoundCounts[std::string(Path) + " @" + CallStack()];
        ReleaseSRWLockExclusive(&FxLock);
    }
    return OriginalStart(Instance);
}

void InstallSoundCounter()
{
    const HMODULE Studio = GetModuleHandleW(L"fmodstudio.dll");
    if (!Studio)
        return Log("Fx-Zaehler: fmodstudio.dll fehlt");
    const auto Start = reinterpret_cast<StartFn>(GetProcAddress(Studio, "?start@EventInstance@Studio@FMOD@@QEAA?AW4FMOD_RESULT@@XZ"));
    GetDescription = reinterpret_cast<GetDescriptionFn>(GetProcAddress(
        Studio, "?getDescription@EventInstance@Studio@FMOD@@QEBA?AW4FMOD_RESULT@@PEAPEAVEventDescription@23@@Z"));
    GetPath = reinterpret_cast<GetPathFn>(GetProcAddress(Studio, "?getPath@EventDescription@Studio@FMOD@@QEBA?AW4FMOD_RESULT@@PEADHPEAH@Z"));
    if (!Start || !GetDescription || !GetPath)
        return Log("Fx-Zaehler: FMOD-Exporte fehlen");
    OriginalStart = Start;
    int Patched = 0;
    const auto Image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto* Headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(Image + reinterpret_cast<const IMAGE_DOS_HEADER*>(Image)->e_lfanew);
    // Die Exe laedt fmodstudio.dll verzoegert (Delay-Load), der Eintrag wird ueber die Namensliste gefunden.
    const auto& Directory = Headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
    for (auto* Import = reinterpret_cast<const IMAGE_DELAYLOAD_DESCRIPTOR*>(Image + Directory.VirtualAddress);
         Directory.VirtualAddress && Import->DllNameRVA; ++Import)
    {
        if (_stricmp(reinterpret_cast<const char*>(Image + Import->DllNameRVA), "fmodstudio.dll") != 0)
            continue;
        auto* Thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(Image + Import->ImportAddressTableRVA);
        for (auto* Name = reinterpret_cast<const IMAGE_THUNK_DATA*>(Image + Import->ImportNameTableRVA); Name->u1.AddressOfData;
             ++Name, ++Thunk)
        {
            if (IMAGE_SNAP_BY_ORDINAL(Name->u1.Ordinal) ||
                strcmp(reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(Image + Name->u1.AddressOfData)->Name,
                       "?start@EventInstance@Studio@FMOD@@QEAA?AW4FMOD_RESULT@@XZ") != 0)
                continue;
            DWORD Old = 0;
            VirtualProtect(&Thunk->u1.Function, sizeof(uintptr_t), PAGE_READWRITE, &Old);
            Thunk->u1.Function = reinterpret_cast<uintptr_t>(&CountedStart);
            VirtualProtect(&Thunk->u1.Function, sizeof(uintptr_t), Old, &Old);
            ++Patched;
        }
    }
    Log("Fx-Zaehler: EventInstance::start an " + std::to_string(Patched) + " Importen");
}
}

namespace
{
// opt-item-sound: Testhilfe ohne Maus und Tastatur. Setzt bis zu Limit sichtbare Actors, deren Klassenname Part enthaelt, im Kreis
// um die erste Spielerfigur ab (K2_SetActorLocation mit Teleport), damit ihr Attraktor sie aufsammelt. Nur in dieser Instanz.
void PullActors(const std::string& Part, int Limit)
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Level = World ? Read<uintptr_t>(World + 0x30) : 0;
    const uintptr_t SetLocation = FindObject("K2_SetActorLocation", "Actor");
    if (!Level || !SetLocation)
        return Log("Fx-Pull: keine Welt oder K2_SetActorLocation fehlt");
    const uintptr_t Data = Read<uintptr_t>(Level + 0xA0);
    const int32_t Count = Read<int32_t>(Level + 0xA8);
    double Center[3]{};
    bool Found = false;
    std::vector<uintptr_t> Matches;
    for (int32_t i = 0; Data && i < Count && i < 200000; ++i)
    {
        const uintptr_t Actor = Read<uintptr_t>(Data + 8 * i);
        if (!Actor)
            continue;
        const std::string Class = NameOf(Read<uintptr_t>(Actor + 0x10));
        const uintptr_t Root = Read<uintptr_t>(Actor + 0x1B8);
        if (!Found && Class == "BP_DD_Character_C" && Root)
        {
            memcpy(Center, reinterpret_cast<const void*>(Root + 0x148), sizeof(Center));
            Found = true;
        }
        else if (Class.find(Part) != std::string::npos && Class.find("Spot") == std::string::npos && Root &&
                 !(Read<uint8_t>(Actor + 0x58) & 0x80) && static_cast<int>(Matches.size()) < Limit)
            Matches.push_back(Actor);
    }
    if (!Found)
        return Log("Fx-Pull: keine Spielerfigur");
    struct alignas(8)
    {
        double Location[3];
        bool Sweep;
        uint8_t Pad[7];
        uint8_t Hit[0x100];
        bool Teleport;
        bool Result;
        uint8_t Pad2[6];
    } Params{};
    static_assert(sizeof(Params) == 0x128);
    for (size_t i = 0; i < Matches.size(); ++i)
    {
        const double Angle = 6.2831853 * static_cast<double>(i) / static_cast<double>(Matches.size());
        Params = {};
        Params.Location[0] = Center[0] + 120.0 * cos(Angle);
        Params.Location[1] = Center[1] + 120.0 * sin(Angle);
        Params.Location[2] = Center[2];
        Params.Teleport = true;
        const auto ProcessEvent = Read<ProcessEventFn*>(Matches[i])[kProcessEventIndex];
        ProcessEvent(Matches[i], SetLocation, &Params);
    }
    Log("Fx-Pull: " + std::to_string(Matches.size()) + " Actors mit '" + Part + "' zur Figur gesetzt");
}

// Host: LootBlueprintFunctionLibrary.DropRequiredFuel(World, BP_FuelLoot_C, Ort der ersten Figur, Radius, Additional), wie der
// Tankpfad des Spiels. Der Kanister repliziert, der Attraktor der Figuren sammelt ihn ohne Laufen.
void DropFuel(int Additional)
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Level = World ? Read<uintptr_t>(World + 0x30) : 0;
    const uintptr_t Drop = FindObject("DropRequiredFuel", "LootBlueprintFunctionLibrary");
    const uintptr_t Library = FindObject("Default__LootBlueprintFunctionLibrary", nullptr);
    const uintptr_t FuelClass = FindObject("BP_FuelLoot_C", nullptr);
    if (!Level || !Drop || !Library || !FuelClass)
        return Log("Fx-Fuel: Welt, DropRequiredFuel oder BP_FuelLoot_C fehlt");
    const uintptr_t Data = Read<uintptr_t>(Level + 0xA0);
    const int32_t Count = Read<int32_t>(Level + 0xA8);
    for (int32_t i = 0; Data && i < Count && i < 200000; ++i)
    {
        const uintptr_t Actor = Read<uintptr_t>(Data + 8 * i);
        const uintptr_t Root = Actor ? Read<uintptr_t>(Actor + 0x1B8) : 0;
        if (!Root || NameOf(Read<uintptr_t>(Actor + 0x10)) != "BP_DD_Character_C")
            continue;
        struct
        {
            uintptr_t World, FuelClass;
            double Location[3];
            float Radius;
            int32_t Additional;
        } Params{World, FuelClass, {}, 150.0f, Additional};
        static_assert(sizeof(Params) == 0x30);
        memcpy(Params.Location, reinterpret_cast<const void*>(Root + 0x148), sizeof(Params.Location));
        Read<ProcessEventFn*>(Library)[kProcessEventIndex](Library, Drop, &Params);
        return Log("Fx-Fuel: DropRequiredFuel an der Figur, zusaetzlich " + std::to_string(Additional));
    }
    Log("Fx-Fuel: keine Spielerfigur");
}
}

// "!fx" haengt den Zaehler ein, "!fx dump" schreibt alle Zaehler ins Log und setzt sie zurueck,
// "!fx stack <Text>" zaehlt Sounds mit <Text> im Pfad zusaetzlich je Aufrufkette,
// "!fx pull <Klassenteil> [Anzahl]" setzt passende Actors zur ersten Spielerfigur (Testhilfe fuer Aufsammelsounds).
void FxCommand(const std::wstring& Args)
{
    if (Args.rfind(L"fuel", 0) == 0)
        return DropFuel(Args.size() > 5 ? _wtoi(Args.c_str() + 5) : 3);
    if (Args.rfind(L"pull ", 0) == 0)
    {
        std::string Rest(Args.begin() + 5, Args.end());
        int Limit = 5;
        if (const size_t Space = Rest.find(' '); Space != std::string::npos)
        {
            Limit = atoi(Rest.c_str() + Space + 1);
            Limit = Limit < 1 ? 1 : Limit > 50 ? 50 : Limit;
            Rest.resize(Space);
        }
        return PullActors(Rest, Limit);
    }
    if (FxFunctions.empty())
    {
        InstallFxCounter();
        InstallSoundCounter();
    }
    if (Args.rfind(L"stack ", 0) == 0)
    {
        AcquireSRWLockExclusive(&FxLock);
        StackFilter = std::string(Args.begin() + 6, Args.end());
        ReleaseSRWLockExclusive(&FxLock);
        return Log("Fx-Zaehler: Aufrufkette fuer " + StackFilter);
    }
    if (Args != L"dump")
        return;
    std::map<FxKey, std::pair<int, std::string>> Counts;
    std::map<std::string, int> Sounds;
    AcquireSRWLockExclusive(&FxLock);
    Counts.swap(FxCounts);
    Sounds.swap(SoundCounts);
    ReleaseSRWLockExclusive(&FxLock);
    Log("Fx-Zaehler: " + std::to_string(Counts.size() + Sounds.size()) + " Eintraege");
    for (const auto& [Path, Starts] : Sounds)
        Log("Fx " + std::to_string(Starts) + " sound " + Path);
    for (const auto& [Key, Entry] : Counts)
        Log("Fx " + std::to_string(Entry.first) + " " + Entry.second);
}
}
