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
#pragma comment(lib, "comctl32.lib")

// A process-owned tool window, not a renderer hook. No engine calls on this UI thread.
namespace
{
constexpr wchar_t WindowClass[] = L"DDCoop.Overlay";
// Hinweis über dem Spielbild (opt-boss-tafel-client): eigenes kleines Fenster ohne Rahmen, nie aktivierbar, Klicks gehen durch.
constexpr wchar_t NoticeClass[] = L"DDCoop.Notice";
constexpr int CreateId = 101, JoinId = 102, InviteId = 103, LeaveId = 104, LobbyId = 105, CopyId = 106, TabId = 107, ModeId = 108, DevXpId = 110, DevTankId = 111, DevGodId = 112, DevZombiesId = 113, DevItemId = 114;
constexpr int TabHeight = 34;
std::mutex Lifecycle;
HANDLE Thread = nullptr;
std::atomic<bool> Stopping = false;
std::atomic<bool> Visible = false;
std::atomic<uint32_t> Marks = 0;

struct WindowState
{
    HWND Owner = nullptr;
    HWND Status = nullptr, Message = nullptr, Debug = nullptr, Hint = nullptr, Diagnostics = nullptr;
    HWND Create = nullptr, Join = nullptr, Invite = nullptr, Leave = nullptr, Lobby = nullptr, Copy = nullptr, Mode = nullptr;
    HWND Tab = nullptr, DevXp = nullptr, DevTank = nullptr, DevGod = nullptr, DevZombies = nullptr, DevItem = nullptr, DevInfo = nullptr;
    bool KeyDown = false;
    bool MarkDown = false;
    uint32_t HotkeysDown = 0;
    ULONGLONG CopiedAt = 0;
    int Dpi = 0;
    HFONT Font = nullptr;
    HWND Notice = nullptr;
    std::wstring NoticeText;
    HFONT NoticeFont = nullptr;
    int NoticeDpi = 0;
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
    // Alles unterhalb der Tab-Leiste (Status-Tab und Dev-Tab teilen sich die Flaeche).
    const auto Place = [&](HWND Item, int X, int Y, int Width, int Height)
    {
        MoveWindow(Item, Scale(X), Scale(Y + TabHeight), Scale(Width), Scale(Height), TRUE);
        SendMessageW(Item, WM_SETFONT, reinterpret_cast<WPARAM>(State.Font), TRUE);
    };
    MoveWindow(State.Tab, 0, 0, Scale(440), Scale(TabHeight - 6), TRUE);
    SendMessageW(State.Tab, WM_SETFONT, reinterpret_cast<WPARAM>(State.Font), TRUE);
    Place(State.DevXp, 12, 10, 200, 34);
    Place(State.DevTank, 12, 52, 200, 34);
    Place(State.DevGod, 12, 94, 200, 34);
    Place(State.DevZombies, 12, 136, 200, 34);
    Place(State.DevItem, 12, 178, 200, 34);
    Place(State.DevInfo, 12, 222, 416, 64);
    Place(State.Status, 12, 10, 416, 24);
    Place(State.Message, 12, 36, 416, 46);
    // Höhe einer Dropdown-Liste schließt die aufgeklappte Liste ein.
    Place(State.Mode, 12, 88, 416, 120);
    Place(State.Create, 12, 124, 128, 30);
    Place(State.Invite, 148, 124, 144, 30);
    Place(State.Leave, 300, 124, 128, 30);
    Place(State.Lobby, 12, 164, 272, 28);
    Place(State.Join, 292, 164, 136, 28);
    Place(State.Debug, 12, 202, 416, 64);
    Place(State.Copy, 12, 270, 160, 28);
    Place(State.Diagnostics, 12, 306, 416, 64);
    Place(State.Hint, 12, 378, 416, 44);
    RECT Rect{ 0, 0, Scale(440), Scale(432 + TabHeight) };
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

void ShowTab(WindowState& State)
{
    const bool Dev = TabCtrl_GetCurSel(State.Tab) == 1;
    for (HWND Item : { State.Status, State.Message, State.Debug, State.Hint, State.Diagnostics, State.Create, State.Invite,
                       State.Leave, State.Lobby, State.Join, State.Copy, State.Mode })
        ShowWindow(Item, Dev ? SW_HIDE : SW_SHOW);
    for (HWND Item : { State.DevXp, State.DevTank, State.DevGod, State.DevZombies, State.DevItem, State.DevInfo })
        ShowWindow(Item, Dev ? SW_SHOW : SW_HIDE);
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
    // coop-modus-wahl: zeigt den Modus der Sitzung (Host und Client) oder die Wahl für die nächste Lobby. Wählbar nur ohne Sitzung
    // und außerhalb eines Runs; die aufgeklappte Liste wird nicht überschrieben.
    const int Selected = Status.Mode == Net::CoopMode::Shared ? 1 : 0;
    if (!SendMessageW(State.Mode, CB_GETDROPPEDSTATE, 0, 0) && SendMessageW(State.Mode, CB_GETCURSEL, 0, 0) != Selected)
        SendMessageW(State.Mode, CB_SETCURSEL, static_cast<WPARAM>(Selected), 0);
    EnableWindow(State.Mode, UI::OverlayModel::CanSubmit(View, { UI::LobbyAction::SetMode, 0, Status.Mode }));
    const bool Dev = UI::OverlayModel::CanDev(View);
    EnableWindow(State.DevXp, Dev);
    EnableWindow(State.DevTank, Dev);
    EnableWindow(State.DevGod, Dev);
    EnableWindow(State.DevZombies, Dev);
    EnableWindow(State.DevItem, Dev);
    Text(State.DevInfo, L"Solo lokal; Koop ueber Host. Nur in Mission wirksam. Zombies am letzten Cursorpunkt im Spielbild. Hotkeys: Numpad 1 bis 5.");
    const bool Copied = State.CopiedAt && GetTickCount64() - State.CopiedAt < 1500;
    Text(State.Copy, Copied ? L"Kopiert" : L"Lobby-ID kopieren");
}

LRESULT CALLBACK NoticeProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    const auto* State = reinterpret_cast<const WindowState*>(GetWindowLongPtrW(Window, GWLP_USERDATA));
    switch (Message)
    {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT Paint{};
        const HDC Dc = BeginPaint(Window, &Paint);
        RECT Area{};
        GetClientRect(Window, &Area);
        const HBRUSH Back = CreateSolidBrush(RGB(24, 24, 28));
        FillRect(Dc, &Area, Back);
        DeleteObject(Back);
        if (State)
        {
            SetBkMode(Dc, TRANSPARENT);
            SetTextColor(Dc, RGB(255, 255, 255));
            const HGDIOBJ Old = State->NoticeFont ? SelectObject(Dc, State->NoticeFont) : nullptr;
            DrawTextW(Dc, State->NoticeText.c_str(), -1, &Area, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            if (Old)
                SelectObject(Dc, Old);
        }
        EndPaint(Window, &Paint);
        return 0;
    }
    }
    return DefWindowProcW(Window, Message, WParam, LParam);
}

// Zeigt den Hinweis oben in der Mitte des Spielbilds, unabhängig vom Statusfenster. Ohne Spielfenster, bei minimiertem Spiel
// oder ohne Text bleibt er verborgen. Nie mit Fokus: SWP_NOACTIVATE, WS_EX_NOACTIVATE, Klicks gehen ans Spiel.
void RefreshNotice(WindowState& State, HWND Owner)
{
    if (!State.Notice)
        return;
    const std::wstring Text = Widen(UI::Overlay().Snapshot().Notice);
    if (!Owner || IsIconic(Owner) || Text.empty())
    {
        if (IsWindowVisible(State.Notice))
            ShowWindow(State.Notice, SW_HIDE);
        return;
    }
    if (GetWindowLongPtrW(State.Notice, GWLP_HWNDPARENT) != reinterpret_cast<LONG_PTR>(Owner))
        SetWindowLongPtrW(State.Notice, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(Owner));
    const int Dpi = GetDpiForWindow(Owner) ? static_cast<int>(GetDpiForWindow(Owner)) : 96;
    if (Dpi != State.NoticeDpi || !State.NoticeFont)
    {
        if (State.NoticeFont)
            DeleteObject(State.NoticeFont);
        State.NoticeFont = CreateFontW(-MulDiv(13, Dpi, 72), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        State.NoticeDpi = Dpi;
        State.NoticeText.clear();
    }
    RECT Bounds{};
    GetClientRect(Owner, &Bounds);
    if (Text != State.NoticeText)
    {
        State.NoticeText = Text;
        SetWindowTextW(State.Notice, Text.c_str());
        InvalidateRect(State.Notice, nullptr, TRUE);
    }
    SIZE Extent{};
    const HDC Dc = GetDC(State.Notice);
    const HGDIOBJ Old = State.NoticeFont ? SelectObject(Dc, State.NoticeFont) : nullptr;
    GetTextExtentPoint32W(Dc, Text.c_str(), static_cast<int>(Text.size()), &Extent);
    if (Old)
        SelectObject(Dc, Old);
    ReleaseDC(State.Notice, Dc);
    const int Width = Extent.cx + MulDiv(32, Dpi, 96), Height = Extent.cy + MulDiv(14, Dpi, 96);
    // Unter dem Ziel-Banner des Spiels (obere Mitte), rund ein Fünftel der Bildhöhe von oben.
    POINT Origin{ (Bounds.right - Width) / 2, Bounds.bottom / 5 };
    ClientToScreen(Owner, &Origin);
    SetWindowPos(State.Notice, nullptr, Origin.x, Origin.y, Width, Height,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOOWNERZORDER);
}

void Refresh(WindowState& State, HWND Window)
{
    if (Stopping.load())
    {
        if (State.Notice)
            DestroyWindow(State.Notice);
        State.Notice = nullptr;
        DestroyWindow(Window);
        return;
    }
    UpdateControls(State);
    HWND Owner = nullptr;
    EnumWindows(FindOwner, reinterpret_cast<LPARAM>(&Owner));
    RefreshNotice(State, Owner);
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
    // live-protokoll: F9 setzt eine Testermarke im eigenen Log (Zustandszeile dieser Seite zum Zeitpunkt eines Fundes). Kurzer Ton als
    // Bestätigung, das Overlay bleibt unberührt.
    const bool MarkNow = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
    if (Focused && MarkNow && !State.MarkDown)
    {
        Marks.fetch_add(1);
        MessageBeep(MB_OK);
    }
    State.MarkDown = MarkNow;
    // Dev-Hotkeys Numpad 1 bis 5 (Wert wie UI::DevButton), nur mit Fokus auf Spiel oder Overlay, auch bei verstecktem Overlay.
    // Der Zeiger liegt dann ueber dem Spielbild, so trifft Spawn Zombies den Cursorpunkt. Bei ausgeschaltetem NumLock
    // meldet Windows andere Tasten, dann wirken sie nicht.
    uint32_t HotkeysNow = 0;
    for (uint8_t Button = 1; Button <= 5; ++Button)
        if (Focused && (GetAsyncKeyState(VK_NUMPAD0 + Button) & 0x8000))
            HotkeysNow |= 1u << Button;
    for (uint8_t Button = 1; Button <= 5; ++Button)
        if ((HotkeysNow & ~State.HotkeysDown) & (1u << Button))
            UI::Overlay().RequestDev(static_cast<UI::DevButton>(Button));
    State.HotkeysDown = HotkeysNow;
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
            State->DevXp = Control(Window, L"BUTTON", L"Spawn XP [Num1]", WS_TABSTOP, DevXpId);
            State->DevTank = Control(Window, L"BUTTON", L"Full Tank [Num2]", WS_TABSTOP, DevTankId);
            // Gruppenrahmen statt STATIC: Tests und Spike-Skripte lesen die STATIC-Reihenfolge des Status-Tabs.
            State->DevGod = Control(Window, L"BUTTON", L"Godmode an/aus [Num3]", WS_TABSTOP, DevGodId);
            State->DevZombies = Control(Window, L"BUTTON", L"Spawn Zombies (Cursor) [Num4]", WS_TABSTOP, DevZombiesId);
            State->DevItem = Control(Window, L"BUTTON", L"Spawn Random Item [Num5]", WS_TABSTOP, DevItemId);
            State->DevInfo = Control(Window, L"BUTTON", L"", BS_GROUPBOX);
            for (HWND Button : { State->DevXp, State->DevTank, State->DevGod, State->DevZombies, State->DevItem })
                EnableWindow(Button, FALSE);
            // Zuletzt angelegt: tools/spike/Common.ps1 liest die Texte der Steuerelemente in Anlagereihenfolge.
            State->Mode = Control(Window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, ModeId);
            SendMessageW(State->Mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Inventar: Eigene Inventare (Standard)"));
            SendMessageW(State->Mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Inventar: Geteiltes Inventar"));
            SendMessageW(State->Mode, CB_SETCURSEL, 0, 0);
            EnableWindow(State->Mode, FALSE);
            {
                INITCOMMONCONTROLSEX Common{ sizeof(Common), ICC_TAB_CLASSES };
                InitCommonControlsEx(&Common);
                State->Tab = Control(Window, WC_TABCONTROLW, L"", WS_TABSTOP | WS_CLIPSIBLINGS, TabId);
                TCITEMW Item{};
                Item.mask = TCIF_TEXT;
                wchar_t Status[] = L"Status", DevName[] = L"Dev";
                Item.pszText = Status;
                TabCtrl_InsertItem(State->Tab, 0, &Item);
                Item.pszText = DevName;
                TabCtrl_InsertItem(State->Tab, 1, &Item);
            }
            Layout(*State, Window, 96);
            ShowTab(*State);
            if (!SetTimer(Window, 1, 100, nullptr))
                return -1;
            return 0;
        case WM_TIMER:
            Refresh(*State, Window);
            return 0;
        case WM_NOTIFY:
            if (reinterpret_cast<NMHDR*>(LParam)->idFrom == TabId && reinterpret_cast<NMHDR*>(LParam)->code == TCN_SELCHANGE)
                ShowTab(*State);
            return 0;
        case WM_COMMAND:
            if (LOWORD(WParam) == ModeId && HIWORD(WParam) == CBN_SELCHANGE)
            {
                const auto Picked = SendMessageW(State->Mode, CB_GETCURSEL, 0, 0);
                UI::Overlay().Submit({ UI::LobbyAction::SetMode, 0, Picked == 1 ? Net::CoopMode::Shared : Net::CoopMode::Own });
                UpdateControls(*State);
                return 0;
            }
            if (HIWORD(WParam) == BN_CLICKED)
            {
                switch (LOWORD(WParam))
                {
                case CreateId: UI::Overlay().Submit({ UI::LobbyAction::Create }); break;
                case JoinId: UI::Overlay().Submit({ UI::LobbyAction::Join, JoinTarget(State->Lobby) }); break;
                case InviteId: UI::Overlay().Submit({ UI::LobbyAction::Invite }); break;
                case LeaveId: UI::Overlay().Submit({ UI::LobbyAction::Leave }); break;
                case DevXpId: UI::Overlay().RequestDev(UI::DevButton::SpawnXp); break;
                case DevGodId: UI::Overlay().RequestDev(UI::DevButton::ToggleGodmode); break;
                case DevTankId: UI::Overlay().RequestDev(UI::DevButton::FullTank); break;
                case DevZombiesId: UI::Overlay().RequestDev(UI::DevButton::SpawnZombies); break;
                case DevItemId: UI::Overlay().RequestDev(UI::DevButton::SpawnRandomItem); break;
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
    WNDCLASSW Notice{};
    Notice.lpfnWndProc = NoticeProc;
    Notice.hInstance = Class.hInstance;
    Notice.lpszClassName = NoticeClass;
    if (Window && RegisterClassW(&Notice))
    {
        State.Notice = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_LAYERED, NoticeClass, L"",
            WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, Class.hInstance, nullptr);
        if (State.Notice)
        {
            SetWindowLongPtrW(State.Notice, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&State));
            SetLayeredWindowAttributes(State.Notice, 0, 230, LWA_ALPHA);
        }
        else
            Core::Log::Error("Overlay: Hinweisfenster konnte nicht erstellt werden: {}", GetLastError());
    }
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
    if (State.Notice && IsWindow(State.Notice))
        DestroyWindow(State.Notice);
    if (State.NoticeFont)
        DeleteObject(State.NoticeFont);
    if (State.Font)
        DeleteObject(State.Font);
    UnregisterClassW(NoticeClass, Class.hInstance);
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

uint32_t TakeMarks()
{
    return Marks.exchange(0);
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
