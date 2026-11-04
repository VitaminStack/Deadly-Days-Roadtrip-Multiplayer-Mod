# Bedient nur die beiden Testinstanzen: Bild aus dem Spielfenster, Klick oder Taste. Andere Fenster werden weder gelesen noch angefasst.
#   Game-Input.ps1 Host shot [datei.png]     Bild des Fensterinhalts, ohne Bildschirmaufnahme des Desktops
#   Game-Input.ps1 Host click <x> <y>        Klick in Fensterkoordinaten (800 x 450)
#   Game-Input.ps1 Host key <taste> [sek]    Taste druecken oder halten, etwa A, Esc, Enter oder einen VK-Code
#   Game-Input.ps1 Host pixel <x> <y>        Farbe eines Punkts im Fensterinhalt als RRGGBB
# Klick und Taste holen das Fenster kurz nach vorn und bewegen die Maus, waehrenddessen nicht selbst tippen.
param(
    [Parameter(Mandatory)][ValidateSet('Host', 'Client')][string]$Role,
    [Parameter(Mandatory)][ValidateSet('shot', 'click', 'key', 'pixel')][string]$Action,
    [string]$Arg1,
    [string]$Arg2
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Add-Type -ReferencedAssemblies System.Drawing @"
using System; using System.Drawing; using System.Runtime.InteropServices; using System.Threading;
public static class SpikeInput {
    [StructLayout(LayoutKind.Sequential)] struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] struct POINT { public int X, Y; }
    [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr Window, out RECT Rect);
    [DllImport("user32.dll")] static extern bool ClientToScreen(IntPtr Window, ref POINT Point);
    [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr Window, int Command);
    [DllImport("user32.dll")] static extern bool IsIconic(IntPtr Window);
    [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr Window);
    [DllImport("user32.dll")] static extern bool SetCursorPos(int X, int Y);
    [DllImport("user32.dll")] static extern void mouse_event(uint Flags, int X, int Y, uint Data, UIntPtr Extra);
    [DllImport("user32.dll")] static extern void keybd_event(byte Key, byte Scan, uint Flags, UIntPtr Extra);
    [DllImport("user32.dll")] static extern uint MapVirtualKey(uint Code, uint Type);
    [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr Window, IntPtr Dc, uint Flags);

    // Windows erlaubt den Fensterwechsel nur nach einer Tastatureingabe, Alt reicht dafuer.
    public static void Focus(IntPtr Window) {
        if (IsIconic(Window)) ShowWindow(Window, 9);
        keybd_event(0x12, 0, 0, UIntPtr.Zero); keybd_event(0x12, 0, 2, UIntPtr.Zero);
        SetForegroundWindow(Window); Thread.Sleep(250);
    }
    public static void Click(IntPtr Window, int X, int Y) {
        Focus(Window);
        POINT Point; Point.X = X; Point.Y = Y; ClientToScreen(Window, ref Point);
        SetCursorPos(Point.X, Point.Y); Thread.Sleep(80);
        mouse_event(2, 0, 0, 0, UIntPtr.Zero); Thread.Sleep(60); mouse_event(4, 0, 0, 0, UIntPtr.Zero);
    }
    public static string Pixel(IntPtr Window, int X, int Y) {
        if (IsIconic(Window)) { ShowWindow(Window, 4); Thread.Sleep(300); }
        RECT Rect; GetClientRect(Window, out Rect);
        using (var Image = new Bitmap(Rect.Right, Rect.Bottom))
        using (var Graphics = System.Drawing.Graphics.FromImage(Image)) {
            IntPtr Dc = Graphics.GetHdc(); PrintWindow(Window, Dc, 3); Graphics.ReleaseHdc(Dc);
            Color C = Image.GetPixel(X, Y);
            return C.R.ToString("X2") + C.G.ToString("X2") + C.B.ToString("X2");
        }
    }
    // Mit Scancode, sonst ignoriert die Engine die Taste.
    public static void Key(IntPtr Window, byte Key, int Milliseconds) {
        Focus(Window);
        byte Scan = (byte)MapVirtualKey(Key, 0);
        keybd_event(Key, Scan, 0, UIntPtr.Zero); Thread.Sleep(Milliseconds); keybd_event(Key, Scan, 2, UIntPtr.Zero);
    }
    // PrintWindow mit PW_CLIENTONLY | PW_RENDERFULLCONTENT liest nur dieses Fenster, auch wenn es verdeckt ist.
    public static void Shot(IntPtr Window, string Path) {
        if (IsIconic(Window)) { ShowWindow(Window, 4); Thread.Sleep(300); }
        RECT Rect; GetClientRect(Window, out Rect);
        using (var Image = new Bitmap(Rect.Right, Rect.Bottom))
        using (var Graphics = System.Drawing.Graphics.FromImage(Image)) {
            IntPtr Dc = Graphics.GetHdc(); PrintWindow(Window, Dc, 3); Graphics.ReleaseHdc(Dc);
            Image.Save(Path, System.Drawing.Imaging.ImageFormat.Png);
        }
    }
}
"@

function ConvertTo-VirtualKey([string]$Name) {
    $Names = @{ Esc = 27; Escape = 27; Enter = 13; Space = 32; Tab = 9; Back = 8; Up = 38; Down = 40; Left = 37; Right = 39 }
    if ($Names.ContainsKey($Name)) { return [byte]$Names[$Name] }
    if ($Name -match '^\d+$') { return [byte][int]$Name }
    if ($Name -match '^[A-Za-z0-9]$') { return [byte][char]$Name.ToUpper() }
    throw "Unbekannte Taste: $Name"
}

$ProcessId = Get-RolePid $Role
if (-not (Test-GameProcess $ProcessId)) { throw "$Role ($ProcessId) laeuft nicht." }
$Window = (Get-Process -Id $ProcessId).MainWindowHandle
switch ($Action) {
    'shot' {
        $Path = if ($Arg1) { $Arg1 } else {
            $Dir = Join-Path $script:SessionDir 'shots'
            New-Item -ItemType Directory -Force -Path $Dir | Out-Null
            Join-Path $Dir ("{0}-{1:HHmmss}.png" -f $Role.ToLower(), (Get-Date))
        }
        [SpikeInput]::Shot($Window, [IO.Path]::GetFullPath($Path))
        Write-Output $Path
    }
    'pixel' { [SpikeInput]::Pixel($Window, [int]$Arg1, [int]$Arg2) }
    'click' { [SpikeInput]::Click($Window, [int]$Arg1, [int]$Arg2) }
    'key' {
        $Seconds = if ($Arg2) { [double]$Arg2 } else { 0.1 }
        [SpikeInput]::Key($Window, (ConvertTo-VirtualKey $Arg1), [int]($Seconds * 1000))
    }
}
