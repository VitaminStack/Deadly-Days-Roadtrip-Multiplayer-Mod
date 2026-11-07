// Beide: Misst Netzlast und Framerate fuer Spike D. Zaehlt UDP-Bytes und Pakete an sendto und recvfrom der Exe und die
// Frames ueber GetFrameCount, und schreibt jede Sekunde eine Zeile ins Log.
#include "Spike.h"

namespace Spike
{
namespace
{
// Socket und Adresse bleiben opak, die Zaehler brauchen nur Laenge und Rueckgabewert.
using SendToFn = int(__stdcall*)(uintptr_t, const char*, int, int, const void*, int);
using RecvFromFn = int(__stdcall*)(uintptr_t, char*, int, int, void*, int*);
SendToFn OriginalSendTo;
RecvFromFn OriginalRecvFrom;

// Die Socket-Aufrufe laufen auf mehreren Threads, die Zaehler deshalb atomar.
volatile LONG64 SentBytes = 0, SentPackets = 0, ReceivedBytes = 0, ReceivedPackets = 0;

int __stdcall SendToHook(uintptr_t Socket, const char* Buffer, int Length, int Flags, const void* To, int ToLength)
{
    const int Result = OriginalSendTo(Socket, Buffer, Length, Flags, To, ToLength);
    if (Result > 0)
    {
        InterlockedAdd64(&SentBytes, Result);
        InterlockedIncrement64(&SentPackets);
    }
    return Result;
}

int __stdcall RecvFromHook(uintptr_t Socket, char* Buffer, int Length, int Flags, void* From, int* FromLength)
{
    const int Result = OriginalRecvFrom(Socket, Buffer, Length, Flags, From, FromLength);
    if (Result > 0)
    {
        InterlockedAdd64(&ReceivedBytes, Result);
        InterlockedIncrement64(&ReceivedPackets);
    }
    return Result;
}

// Tauscht einen IAT-Eintrag, aber nur, wenn er auf die erwartete Funktion von WS2_32 zeigt.
// Die IAT-Eintraege fuer WS2_32 Ordinal 20 (sendto) und 17 (recvfrom) stehen in jedem Build woanders, darum Suche in der Importtabelle.
template <typename Fn> bool SwapImport(int Ordinal, Fn Hook, Fn& Original)
{
    const HMODULE Sockets = GetModuleHandleW(L"ws2_32.dll");
    const auto Expected = Sockets ? reinterpret_cast<Fn>(GetProcAddress(Sockets, MAKEINTRESOURCEA(Ordinal))) : nullptr;
    const uintptr_t Slot = FindImportSlot(reinterpret_cast<const void*>(Expected));
    auto* Entry = reinterpret_cast<Fn*>(Base + Slot);
    if (!Expected || !Slot || *Entry != Expected)
    {
        Log("Last: IAT-Eintrag " + std::to_string(Ordinal) + " unerwartet");
        return false;
    }
    DWORD Old = 0;
    VirtualProtect(Entry, sizeof(*Entry), PAGE_READWRITE, &Old);
    Original = *Entry;
    *Entry = Hook;
    VirtualProtect(Entry, sizeof(*Entry), Old, &Old);
    return true;
}

int64_t FrameCount()
{
    const uintptr_t Library = FindObject("Default__KismetSystemLibrary", nullptr);
    const uintptr_t Function = FindObject("GetFrameCount", "KismetSystemLibrary");
    if (!Library || !Function)
        return -1;
    struct { int64_t ReturnValue; } Params{};
    Read<ProcessEventFn*>(Library)[kProcessEventIndex](Library, Function, &Params);
    return Params.ReturnValue;
}

struct FSample
{
    ULONGLONG Time = 0;
    int64_t Frames = 0;
    LONG64 SentBytes = 0, SentPackets = 0, ReceivedBytes = 0, ReceivedPackets = 0;
};
FSample Last;
}

bool PatchLoad()
{
    return SwapImport(20, static_cast<SendToFn>(SendToHook), OriginalSendTo) &&
           SwapImport(17, static_cast<RecvFromFn>(RecvFromHook), OriginalRecvFrom);
}

// Laeuft im Sekunden-Tick auf dem Spielthread. Werte je Sekunde seit dem letzten Aufruf.
void TickLoad()
{
    FSample Now;
    Now.Time = GetTickCount64();
    Now.Frames = FrameCount();
    Now.SentBytes = SentBytes;
    Now.SentPackets = SentPackets;
    Now.ReceivedBytes = ReceivedBytes;
    Now.ReceivedPackets = ReceivedPackets;
    if (Last.Time && Now.Time > Last.Time && Now.Frames >= Last.Frames)
    {
        const double Seconds = (Now.Time - Last.Time) / 1000.0;
        char Line[256];
        sprintf_s(Line, "Last: %.1f fps, aus %.0f B/s (%.0f Pakete/s), ein %.0f B/s (%.0f Pakete/s)", (Now.Frames - Last.Frames) / Seconds,
                  (Now.SentBytes - Last.SentBytes) / Seconds, (Now.SentPackets - Last.SentPackets) / Seconds,
                  (Now.ReceivedBytes - Last.ReceivedBytes) / Seconds, (Now.ReceivedPackets - Last.ReceivedPackets) / Seconds);
        Log(Line);
    }
    Last = Now;
}
}
