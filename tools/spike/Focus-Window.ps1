# Holt das Fenster einer Instanz nach vorn, sonst gehen Tastatur und Maus ins falsche Programm.
param([Parameter(Mandatory)][ValidateSet('Host', 'Client')][string]$Role)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Add-Type @"
using System; using System.Runtime.InteropServices;
public class SpikeWindow {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr Handle);
    [DllImport("user32.dll")] public static extern void keybd_event(byte Key, byte Scan, uint Flags, UIntPtr Extra);
}
"@
$Handle = (Get-Process -Id (Get-RolePid $Role)).MainWindowHandle
# Windows erlaubt den Fensterwechsel nur nach einer Tastatureingabe, Alt reicht dafuer.
[SpikeWindow]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)
[SpikeWindow]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
[SpikeWindow]::SetForegroundWindow($Handle) | Out-Null
