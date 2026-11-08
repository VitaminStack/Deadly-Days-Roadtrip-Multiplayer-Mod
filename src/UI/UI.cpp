#include "UI/UI.h"

#include <windows.h>
#include <commctrl.h>

#include <atomic>
#include <cstring>
#include <exception>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>

#include "Core/Config.h"
#include "Core/Log.h"
#include "Net/Protocol.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// A process-owned tool window, not a renderer hook. No engine calls on this UI thread.
namespace
{
constexpr wchar_t WindowClass[] = L"DDCoop.Overlay";
constexpr int CreateId = 101, JoinId = 102, InviteId = 103, LeaveId = 104, LobbyId = 105, CopyId = 106;
std::mutex Lifecycle;
HANDLE Thread = nullptr;
std::atomic<bool> Stopping = false;
std::atomic<bool> Visible = false;

struct WindowState
{
    HWND Owner = nullptr;
    HWND Status = nullptr, Message = nullptr, Debug = nullptr, Hint = nullptr, Diagnostics = nullptr;
    HWND Create = nullptr, Join = nullptr, Invite = nullptr, Leave = nullptr, Lobby = nullptr, Copy = nullptr;
    bool KeyDown = false;
    ULONGLONG CopiedAt = 0;
    int Dpi = 0;
    HFONT Font = nullptr;
};

std::wstring Widen(const std::string& Text)
{
    // Backend text is UTF-8, bounded before calling Windows. Invalid text has no control semantics.
    const std::string Limited = Text.substr(0, 2048);
    const int Size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Limited.data(),
        static_cast<int>(Limited.size()), nullptr, 0);
    if (!Size)
        return Limited.empty() ? L"" : L"Ungueltiger Status-Text";
    std::wstring Result(static_cast<size_t>(Size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Limited.data(), static_cast<int>(Limited.size()), Result.data(), Size);
    return Result;
}

void Text(HWND Control, const std::wstring& Value)
{
    // Do not repaint/select edit text on every timer tick.
    std::wstring Old(static_cast<size_t>(GetWindowTextLengthW(Control)) + 1, L'\0');
    const int Length = GetWindowTextW(Control, Old.data(), static_cast<int>(Old.size()));
    Old.resize(static_cast<size_t>(Length));
    if (Old != Value)
        SetWindowTextW(Control, Value.c_str());
}

std::wstring ModName()
{
    return L"DDCoop v" + std::to_wstring(Net::Protocol::ModVersion);
}

const wchar_t* StateName(UI::SessionState State)
{
    switch (State)
    {
    case UI::SessionState::Offline: return L"Offline";
    case UI::SessionState::Connecting: return L"Verbindung wird aufgebaut";
    case UI::SessionState::Lobby: return L"In Lobby";
    case UI::SessionState::Playing: return L"Im Spiel";
    case UI::SessionState::Failed: return L"Verbindungsfehler";
    default: return L"Netzwerk noch nicht angebunden";
    }
}

BOOL CALLBACK FindOwner(HWND Candidate, LPARAM Data)
{
    DWORD Process = 0;
    GetWindowThreadProcessId(Candidate, &Process);
    if (Process != GetCurrentProcessId() || !IsWindowVisible(Candidate) || GetWindow(Candidate, GW_OWNER))
        return TRUE;
    wchar_t Class[64]{};
    GetClassNameW(Candidate, Class, 64);
    if (std::wstring(Class) == WindowClass)
        return TRUE;
    RECT Rect{};
    GetClientRect(Candidate, &Rect);
    auto& Best = *reinterpret_cast<HWND*>(Data);
    RECT Previous{};
    if (Best)
        GetClientRect(Best, &Previous);
    if (static_cast<int64_t>(Rect.right) * Rect.bottom > static_cast<int64_t>(Previous.right) * Previous.bottom)
        Best = Candidate;
    return TRUE;
}

HWND Control(HWND Window, const wchar_t* Class, const wchar_t* Label, DWORD Style, int Id = 0)
{
    HWND Result = CreateWindowExW(0, Class, Label, WS_CHILD | WS_VISIBLE | Style,
        0, 0, 1, 1, Window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(Id)), GetModuleHandleW(nullptr), nullptr);
    if (!Result)
        throw std::runtime_error("Overlay-Steuerelement konnte nicht erstellt werden");
    return Result;
}

void Layout(WindowState& State, HWND Window, int Dpi)
{
    const auto Scale = [Dpi](int Value) { return MulDiv(Value, Dpi, 96); };
    if (State.Font)
        DeleteObject(State.Font);
    State.Font = CreateFontW(-Scale(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    const auto Place = [&](HWND Item, int X, int Y, int Width, int Height)
    {
        MoveWindow(Item, Scale(X), Scale(Y), Scale(Width), Scale(Height), TRUE);
        SendMessageW(Item, WM_SETFONT, reinterpret_cast<WPARAM>(State.Font), TRUE);
    };
    Place(State.Status, 12, 10, 416, 24);
    Place(State.Message, 12, 36, 416, 46);
    Place(State.Create, 12, 88, 128, 30);
    Place(State.Invite, 148, 88, 144, 30);
    Place(State.Leave, 300, 88, 128, 30);
    Place(State.Lobby, 12, 128, 272, 28);
    Place(State.Join, 292, 128, 136, 28);
    Place(State.Debug, 12, 166, 416, 64);
    Place(State.Copy, 12, 234, 160, 28);
    Place(State.Diagnostics, 12, 270, 416, 64);
    Place(State.Hint, 12, 342, 416, 44);
    RECT Rect{ 0, 0, Scale(440), Scale(396) };
    AdjustWindowRectExForDpi(&Rect, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_TOOLWINDOW, static_cast<UINT>(Dpi));
    SetWindowPos(Window, nullptr, 0, 0, Rect.right - Rect.left, Rect.bottom - Rect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    State.Dpi = Dpi;
}

uint64_t JoinTarget(HWND Edit)
{
    wchar_t Buffer[22]{};
    GetWindowTextW(Edit, Buffer, 22);
    uint64_t Target = 0;
    UI::OverlayModel::ParseLobbyId(Buffer, Target);
    return Target;
}

bool CopyLobbyId(HWND Window, uint64_t Lobby)
{
    if (!Lobby || !OpenClipboard(Window))
        return false;
    const std::wstring Value = std::to_wstring(Lobby);
    bool Copied = false;
    if (EmptyClipboard())
    {
        const size_t Bytes = (Value.size() + 1) * sizeof(wchar_t);
        if (HGLOBAL Memory = GlobalAlloc(GMEM_MOVEABLE, Bytes))
        {
            if (void* Target = GlobalLock(Memory))
            {
                memcpy(Target, Value.c_str(), Bytes);
                GlobalUnlock(Memory);
                Copied = SetClipboardData(CF_UNICODETEXT, Memory) != nullptr;
            }
            if (!Copied)
                GlobalFree(Memory);
        }
    }
    CloseClipboard();
    return Copied;
}

void UpdateControls(WindowState& State)
{
    const auto View = UI::Overlay().Snapshot();
    const auto& Status = View.Status;
    // Die einzige Mod-Versionsquelle ist auch die im Handshake verwendete Konstante.
    Text(State.Status, ModName() + L" | " + StateName(Status.State));
    Text(State.Message, Widen(Status.Message));
    Text(State.Diagnostics, Widen(View.Diagnostics));
    const wchar_t* Role = Status.Role == UI::SessionRole::Host ? L"Host" :
        Status.Role == UI::SessionRole::Client ? L"Client" : L"Keine";
    Text(State.Debug, std::wstring(L"Rolle: ") + Role + L" | Spieler: " + std::to_wstring(Status.Players) +
        L"\r\nLobby: " + (Status.LobbyId ? std::to_wstring(Status.LobbyId) : L"-" ) +
        L" | Ping: " + (Status.HasPing ? std::to_wstring(Status.PingMs) + L" ms" : L"-" ) +
        L"\r\nProzess: " + std::to_wstring(GetCurrentProcessId()));
    Text(State.Hint, View.Pending ? L"Aktion wartet auf den Game-Thread-Dispatcher." :
        !View.HasBackend ? L"F8: anzeigen/verbergen. Lobby-Knoepfe warten auf Phase 3." :
        L"F8: anzeigen/verbergen. Aktionen werden im Game Thread ausgefuehrt.");
    EnableWindow(State.Create, UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Create }));
    EnableWindow(State.Join, UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Join, JoinTarget(State.Lobby) }));
    EnableWindow(State.Invite, UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Invite }));
    EnableWindow(State.Leave, UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::Leave }));
    EnableWindow(State.Copy, Status.LobbyId != 0);
    const bool Copied = State.CopiedAt && GetTickCount64() - State.CopiedAt < 1500;
    Text(State.Copy, Copied ? L"Kopiert" : L"Lobby-ID kopieren");
}

void Refresh(WindowState& State, HWND Window)
{
    if (Stopping.load())
    {
        DestroyWindow(Window);
        return;
    }
    UpdateControls(State);
    HWND Owner = nullptr;
    EnumWindows(FindOwner, reinterpret_cast<LPARAM>(&Owner));
    if (Owner != State.Owner)
    {
        State.Owner = Owner;
        SetWindowLongPtrW(Window, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(Owner));
    }
    const HWND Foreground = GetForegroundWindow();
    const bool Focused = Owner && (Foreground == Owner || Foreground == Window);
    // F8 only toggles while the game or overlay has focus, but focus loss does not hide the overlay.
    const bool Down = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
    if (Focused && Down && !State.KeyDown)
        UI::SetVisible(!Visible.load());
    State.KeyDown = Down;
    if (!Owner || IsIconic(Owner) || !Visible.load())
    {
        const bool HadFocus = Foreground == Window;
        ShowWindow(Window, SW_HIDE);
        if (HadFocus && Owner)
            SetForegroundWindow(Owner);
        return;
    }
    const int Dpi = static_cast<int>(GetDpiForWindow(Owner));
    if (Dpi != State.Dpi)
        Layout(State, Window, Dpi ? Dpi : 96);
    RECT Bounds{}, Size{};
    GetClientRect(Owner, &Bounds);
    POINT Corner{ Bounds.right, Bounds.top };
    ClientToScreen(Owner, &Corner);
    GetWindowRect(Window, &Size);
    // Without focus only move: the owned window stays above its game, other apps may cover both.
    SetWindowPos(Window, HWND_TOP, Corner.x - (Size.right - Size.left) - 12, Corner.y + 12, 0, 0,
        SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW | (Focused ? 0 : SWP_NOZORDER));

}

LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    auto* State = reinterpret_cast<WindowState*>(GetWindowLongPtrW(Window, GWLP_USERDATA));
    try
    {
        if (Message == WM_NCCREATE)
        {
            State = static_cast<WindowState*>(reinterpret_cast<CREATESTRUCTW*>(LParam)->lpCreateParams);
            SetWindowLongPtrW(Window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(State));
        }
        if (!State)
            return DefWindowProcW(Window, Message, WParam, LParam);
        switch (Message)
        {
        case WM_CREATE:
            State->Status = Control(Window, L"STATIC", L"DDCoop", SS_LEFT);
            State->Message = Control(Window, L"STATIC", L"", SS_LEFT);
            State->Debug = Control(Window, L"STATIC", L"", SS_LEFT);
            State->Hint = Control(Window, L"STATIC", L"", SS_LEFT);
            // Bestehende STATIC-Reihenfolge fuer tools/spike/Common.ps1 erhalten.
            State->Diagnostics = Control(Window, L"STATIC", L"Desync: wartet", SS_LEFT);
            State->Create = Control(Window, L"BUTTON", L"Lobby erstellen", WS_TABSTOP, CreateId);
            State->Invite = Control(Window, L"BUTTON", L"Freunde einladen", WS_TABSTOP, InviteId);
            State->Leave = Control(Window, L"BUTTON", L"Verlassen", WS_TABSTOP, LeaveId);
            State->Lobby = Control(Window, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, LobbyId);
            // Keep one excess digit so an overlong paste stays invalid, not a different valid ID.
            SendMessageW(State->Lobby, EM_SETLIMITTEXT, 21, 0);
            SendMessageW(State->Lobby, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"Steam-Lobby-ID"));
            State->Join = Control(Window, L"BUTTON", L"Beitreten", WS_TABSTOP, JoinId);
            State->Copy = Control(Window, L"BUTTON", L"Lobby-ID kopieren", WS_TABSTOP, CopyId);
            for (HWND Button : { State->Create, State->Join, State->Invite, State->Leave, State->Copy })
                EnableWindow(Button, FALSE);
            Layout(*State, Window, 96);
            if (!SetTimer(Window, 1, 100, nullptr))
                return -1;
            return 0;
        case WM_TIMER:
            Refresh(*State, Window);
            return 0;
        case WM_COMMAND:
            if (HIWORD(WParam) == BN_CLICKED)
            {
                switch (LOWORD(WParam))
                {
                case CreateId: UI::Overlay().Submit({ UI::LobbyAction::Create }); break;
                case JoinId: UI::Overlay().Submit({ UI::LobbyAction::Join, JoinTarget(State->Lobby) }); break;
                case InviteId: UI::Overlay().Submit({ UI::LobbyAction::Invite }); break;
                case LeaveId: UI::Overlay().Submit({ UI::LobbyAction::Leave }); break;
                case CopyId:
                    // Clipboard only, no backend action: stays on the window thread.
                    if (CopyLobbyId(Window, UI::Overlay().Snapshot().Status.LobbyId))
                        State->CopiedAt = GetTickCount64();
                    else
                        Core::Log::Error("Overlay: Lobby-ID konnte nicht kopiert werden: {}", GetLastError());
                    UpdateControls(*State);
                    break;
                }
            }
            return 0;
        case WM_CLOSE:
            UI::SetVisible(false);
            Refresh(*State, Window);
            return 0;
        case WM_DESTROY:
            KillTimer(Window, 1);
            if (State->Font)
                DeleteObject(State->Font);
            State->Font = nullptr;
            PostQuitMessage(0);
            return 0;
        }
    }
    catch (const std::exception& Error)
    {
        Core::Log::Error("Overlay: {}", Error.what());
        if (Message == WM_CREATE)
            return -1;
    }
    catch (...)
    {
        Core::Log::Error("Overlay: unbekannter Fehler");
        if (Message == WM_CREATE)
            return -1;
    }
    return DefWindowProcW(Window, Message, WParam, LParam);
}

DWORD RunWindow()
{
    WNDCLASSW Class{};
    Class.lpfnWndProc = WindowProc;
    Class.hInstance = GetModuleHandleW(nullptr);
    Class.lpszClassName = WindowClass;
    Class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    Class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassW(&Class))
    {
        Core::Log::Error("Overlay-Fensterklasse konnte nicht registriert werden: {}", GetLastError());
        return 1;
    }
    WindowState State;
    const std::wstring Title = ModName() + L" - Status (F8)";
    HWND Window = CreateWindowExW(WS_EX_TOOLWINDOW, WindowClass, Title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU, 0, 0, 460, 340, nullptr, nullptr, Class.hInstance, &State);
    if (Window)
    {
        Core::Log::Info("Overlay bereit, F8 im Spielfenster zum Anzeigen");
        MSG Message{};
        while (GetMessageW(&Message, nullptr, 0, 0) > 0)
        {
            if (!IsDialogMessageW(Window, &Message))
            {
                TranslateMessage(&Message);
                DispatchMessageW(&Message);
            }
        }
        if (IsWindow(Window))
            DestroyWindow(Window);
    }
    else
        Core::Log::Error("Overlay-Fenster konnte nicht erstellt werden: {}", GetLastError());
    if (State.Font)
        DeleteObject(State.Font);
    UnregisterClassW(WindowClass, Class.hInstance);
    return Window ? 0 : 1;
}

DWORD WINAPI Run(void*)
{
    try { return RunWindow(); }
    catch (const std::exception& Error) { Core::Log::Error("Overlay-Thread: {}", Error.what()); }
    catch (...) { Core::Log::Error("Overlay-Thread: unbekannter Fehler"); }
    return 1;
}
}

namespace UI
{
OverlayModel& Overlay()
{
    static OverlayModel Model;
    return Model;
}

void Start()
{
    const std::lock_guard Lock(Lifecycle);
    if (Thread)
        return;
    HMODULE Module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&Start), &Module);
    std::wstring Path(32768, L'\0');
    const DWORD Length = GetModuleFileNameW(Module, Path.data(), static_cast<DWORD>(Path.size()));
    if (!Length || Length >= Path.size())
    {
        Core::Log::Error("Overlay-Konfigurationspfad nicht verfuegbar");
        return;
    }
    Path.resize(Length);
    const auto Config = Core::Config::Load(std::filesystem::path(Path).parent_path() / "ddcoop.ini");
    if (!Config.GetBool("overlay_enabled", true))
        return;
    Visible.store(Config.GetBool("overlay_visible", false));
    Stopping.store(false);
    Thread = CreateThread(nullptr, 0, Run, nullptr, 0, nullptr);
    if (!Thread)
        Core::Log::Error("Overlay-Thread konnte nicht gestartet werden: {}", GetLastError());
}

void SetVisible(bool Value) { Visible.store(Value); }

void Stop()
{
    // No game thread wait in the window procedure; Stop must not run on the overlay thread/DllMain.
    const std::lock_guard Lock(Lifecycle);
    Overlay().Bind({});
    Stopping.store(true);
    if (Thread)
    {
        WaitForSingleObject(Thread, INFINITE);
        CloseHandle(Thread);
        Thread = nullptr;
    }
    Visible.store(false);
}
}
