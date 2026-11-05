# Bedient das Overlay der Mod (F8) einer Testinstanz ohne Maus: Knoepfe per WM_COMMAND, Zustand aus den Texten.
# Das Fenster muss nicht sichtbar sein. Nur Instanzen der eigenen Sitzung, keine fremden Fenster.
#   Overlay.ps1 Host status           Zustand, Meldung, Rolle, Lobby-ID, Ping, freie Knoepfe
#   Overlay.ps1 Host create           Lobby erstellen, gibt die Lobby-ID aus
#   Overlay.ps1 Client join [<id>]    Beitreten, ohne ID der Lobby des Hosts dieser Sitzung
#   Overlay.ps1 Client leave          Verlassen
#   Overlay.ps1 Host invite           Freundesliste im Steam-Overlay oeffnen
param(
    [Parameter(Mandatory)][ValidateSet('Host', 'Client')][string]$Role,
    [Parameter(Mandatory)][ValidateSet('status', 'create', 'join', 'leave', 'invite')][string]$Action,
    [string]$LobbyId,
    [switch]$Json
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$ProcessId = Get-RolePid $Role
switch ($Action) {
    'status' {
        $State = Get-OverlayState $ProcessId
        if ($Json) { $State | ConvertTo-Json -Compress } else { $State }
    }
    'create' { New-OverlayLobby $ProcessId }
    'join' {
        if (-not $LobbyId) { $LobbyId = (Get-OverlayState (Get-RolePid 'Host')).LobbyId }
        if (-not $LobbyId) { throw 'Keine Lobby-ID: Der Host hat keine Lobby, vorher "Overlay.ps1 Host create".' }
        $State = Join-OverlayLobby $ProcessId $LobbyId
        if ($Json) { $State | ConvertTo-Json -Compress } else { $State }
        if ($State.State -ne 'Im Spiel') { exit 1 }
    }
    'leave' { Invoke-OverlayAction $ProcessId Leave; 'Verlassen ausgeloest' }
    'invite' { Invoke-OverlayAction $ProcessId Invite; 'Einladen ausgeloest' }
}
