#include "Game/StallCapture.h"

#include <windows.h>

#include <algorithm>
#include <format>

#include "Game/Memory.h"
#include "Game/Offsets.h"

namespace Game::Stall
{
namespace
{
// Ohne C++-Objekte, damit __try erlaubt ist. Kopiert bis zur ersten unlesbaren Seite.
size_t CopyStack(uint64_t From, uint8_t* To, size_t Size)
{
    size_t Done = 0;
    __try
    {
        for (; Done + 8 <= Size; Done += 8)
            *reinterpret_cast<uint64_t*>(To + Done) = *reinterpret_cast<const uint64_t*>(From + Done);
    }
    __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
    }
    return Done;
}

// Abwicklung auf der Kopie: Zeiger in den echten Stapel werden in die Kopie umgerechnet, alles außerhalb beendet die Abwicklung.
size_t Unwind(CONTEXT* Context, uint64_t RealRsp, uint8_t* Copy, size_t Copied, uint64_t* Frames, size_t MaxFrames)
{
    const uint64_t CopyBase = reinterpret_cast<uint64_t>(Copy);
    const auto ToCopy = [&](DWORD64& Value) {
        if (Value >= RealRsp && Value < RealRsp + Copied)
            Value = Value - RealRsp + CopyBase;
    };
    size_t Count = 0;
    __try
    {
        ToCopy(Context->Rsp);
        ToCopy(Context->Rbp);
        while (Count < MaxFrames && Context->Rip)
        {
            Frames[Count++] = Context->Rip;
            if (Context->Rsp < CopyBase || Context->Rsp + 8 > CopyBase + Copied)
                break;
            DWORD64 ImageBase = 0;
            const PRUNTIME_FUNCTION Entry = RtlLookupFunctionEntry(Context->Rip, &ImageBase, nullptr);
            if (!Entry)
            {
                // Blattfunktion ohne Abwicklungsdaten: Rücksprungadresse liegt oben auf dem Stapel.
                Context->Rip = *reinterpret_cast<const DWORD64*>(Context->Rsp);
                Context->Rsp += 8;
            }
            else
            {
                PVOID Handler = nullptr;
                DWORD64 Establisher = 0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, ImageBase, Context->Rip, Entry, Context, &Handler, &Establisher, nullptr);
            }
            ToCopy(Context->Rsp);
            ToCopy(Context->Rbp);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return Count;
}

bool IsFunction(uint64_t Value)
{
    if (Value < 0x10000 || Value > 0x7FFFFFFFFFFF || (Value & 7))
        return false;
    const auto Class = Memory::ReadPointer(Value + Offsets::UObject::Class);
    if (!Class)
        return false;
    const std::string Name = Memory::NameOf(*Class);
    return Name == "Function" || Name == "DelegateFunction";
}
}

StackSample CaptureStack(unsigned long ThreadId, size_t StackBytes, size_t MaxFrames, size_t MaxFunctions)
{
    StackSample Result;
    if (ThreadId == GetCurrentThreadId() || !StackBytes || !MaxFrames)
        return Result;
    // Alles Nötige vor dem Anhalten anlegen: Der angehaltene Thread kann die Heap-Sperre halten.
    std::vector<uint8_t> Copy(StackBytes);
    std::vector<uint64_t> Frames(MaxFrames);
    alignas(16) CONTEXT Context{};
    Context.ContextFlags = CONTEXT_FULL;
    const HANDLE Thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, ThreadId);
    if (!Thread)
        return Result;
    size_t Copied = 0;
    if (SuspendThread(Thread) != static_cast<DWORD>(-1))
    {
        if (GetThreadContext(Thread, &Context))
            Copied = CopyStack(Context.Rsp, Copy.data(), Copy.size());
        ResumeThread(Thread);
    }
    CloseHandle(Thread);
    if (!Copied)
        return Result;
    const uint64_t RealRsp = Context.Rsp;
    Frames.resize(Unwind(&Context, RealRsp, Copy.data(), Copied, Frames.data(), Frames.size()));
    Result.Frames = std::move(Frames);
    for (size_t At = 0; At + 8 <= Copied && Result.Functions.size() < MaxFunctions; At += 8)
    {
        const uint64_t Value = *reinterpret_cast<const uint64_t*>(Copy.data() + At);
        if (!IsFunction(Value))
            continue;
        const auto Outer = Memory::ReadPointer(Value + Offsets::UObject::Outer);
        std::string Name = (Outer ? Memory::NameOf(*Outer) : std::string{}) + "." + Memory::NameOf(Value);
        if (std::find(Result.Functions.begin(), Result.Functions.end(), Name) == Result.Functions.end())
            Result.Functions.push_back(std::move(Name));
    }
    Result.Ok = !Result.Frames.empty();
    return Result;
}

std::string DescribeAddress(uint64_t Address)
{
    HMODULE Module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(Address), &Module) ||
        !Module)
        return std::format("0x{:X}", Address);
    wchar_t Path[MAX_PATH]{};
    const DWORD Length = GetModuleFileNameW(Module, Path, MAX_PATH);
    std::wstring_view View(Path, Length);
    if (const auto Slash = View.find_last_of(L"\\/"); Slash != std::wstring_view::npos)
        View.remove_prefix(Slash + 1);
    std::string Name;
    for (const wchar_t Char : View)
        Name += Char < 0x80 ? static_cast<char>(Char) : '?';
    return std::format("{}+0x{:X}", Name.empty() ? "?" : Name, Address - reinterpret_cast<uint64_t>(Module));
}

std::string DescribeSample(const StackSample& Sample)
{
    if (!Sample.Ok)
        return "Stapel nicht lesbar";
    std::string Text;
    for (const uint64_t Frame : Sample.Frames)
        Text += (Text.empty() ? "" : " ") + DescribeAddress(Frame);
    if (!Sample.Functions.empty())
    {
        Text += " | Blueprint:";
        for (const std::string& Name : Sample.Functions)
            Text += " " + Name;
    }
    return Text;
}
}
