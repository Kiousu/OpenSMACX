param([ValidateSet("lobby", "launch")][string]$Phase = "lobby", [int]$Turns = 8)
# Desktop side of the LAN multiplayer test.
#   -Phase lobby : launch the host game, host a TCP/IP DirectPlay game, leave it in the lobby.
#   -Phase launch: with the client joined and ready, press LAUNCH GAME, dismiss planetfall, end turns.
# Logs to mphost.log and mp_*.png in the scratchpad; the host game's pid is kept in pidA.txt.
$S = $PSScriptRoot
$UI = Join-Path $S "ui.ps1"
$DirA = "G:\SMACX-opensmacx-test"
$Log = Join-Path $S "mphost.log"
function L($m) { $line = "[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $m; $line | Add-Content $Log; $line }
function Click($p, $x, $y) { & $UI -Action click -X $x -Y $y -ProcId $p | Out-Null }
function Key($p, $k) { & $UI -Action key -Keys $k -ProcId $p | Out-Null }
function Shot($n) { & $UI -Action shot -Out (Join-Path $S "mp_$n.png") | Out-Null }
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class WH {
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
}
"@
if ($Phase -eq "lobby") {
    "start" | Set-Content $Log
    [WH]::SetCursorPos(500, 400) | Out-Null
    $a = Start-Process -FilePath (Join-Path $DirA "terranx_opensmacx.exe") -WorkingDirectory $DirA -PassThru
    $a.Id | Out-File (Join-Path $S "pidA.txt")
    Start-Sleep 14; $a.Refresh()
    [WH]::SetWindowPos($a.MainWindowHandle, [IntPtr]::Zero, 0, 0, 0, 0, 0x0005) | Out-Null
    Start-Sleep 1
    L "host pid $($a.Id)"
    & $UI -Action focus -ProcId $a.Id | Out-Null; Start-Sleep 2
    Click $a.Id 835 587; Start-Sleep 4; Shot "01_mp"       # MULTIPLAYER
    Click $a.Id 510 403; Start-Sleep 1                     # TCP/IP
    Click $a.Id 389 449; Start-Sleep 6; Shot "02_hostjoin" # OK
    Click $a.Id 510 383; Start-Sleep 1                     # Host
    Click $a.Id 389 435; Start-Sleep 5; Shot "03_names"    # OK
    Click $a.Id 570 365; Start-Sleep 1
    Key $a.Id "{END}{BACKSPACE 40}osxtest"; Start-Sleep 1
    Click $a.Id 570 388; Start-Sleep 1
    Key $a.Id "{END}{BACKSPACE 40}HostA"; Start-Sleep 1
    Click $a.Id 389 427; Start-Sleep 8; Shot "04_lobby"    # OK -> lobby
    L "host in lobby"
    & $UI -Action status -ProcId $a.Id
    exit 0
}
$id = [int](Get-Content (Join-Path $S "pidA.txt"))
$a = Get-Process -Id $id -ErrorAction SilentlyContinue
if (-not $a) { L "host game not running"; exit 1 }
& $UI -Action focus -ProcId $id | Out-Null; Start-Sleep 2
Shot "05_lobby_joined"
Click $id 546 306; Start-Sleep 4; Shot "06_launch"         # LAUNCH GAME
Click $id 510 383; Start-Sleep 1                           # Yes (if asked)
Click $id 510 435; Start-Sleep 25; Shot "07_started"       # OK
Click $id 707 738; Start-Sleep 4; Shot "08_planetfall_ok"
for ($t = 0; $t -lt $Turns; $t++) {
    $pp = Get-Process -Id $id -ErrorAction SilentlyContinue
    if (-not $pp -or $pp.HasExited) { L "host died at turn $t"; break }
    Key $id "{ENTER}"; Click $id 899 584; Start-Sleep 6; Key $id "{ENTER}"; Start-Sleep 4
    Shot ("turn_{0:D2}" -f $t)
    L ("turn $t cpu=" + [int]$pp.CPU)
}
& $UI -Action status -ProcId $id
L "done"
