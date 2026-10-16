param(
    [string]$HostIP = "192.168.50.219",
    [string]$GameDir = "",
    [string]$Name = "ClientB",
    [int]$Turns = 8,
    [switch]$Probe,         # only launch, check the layout (menu + Load dialog), then quit
    [switch]$Attach         # the game is already running and joined by hand: only run the turn loop
)
# Client side of the OpenSMACX netplay test, run on a second Windows machine with the PRACX-based
# client exe. Forces PRACX into a 1024x768 window so the game uses its original layout, launches
# the game from $GameDir, joins the host's DirectPlay TCP/IP session at $HostIP, makes ready, then
# keeps ending turns. Writes mpclient.log and mp_*.png screenshots next to this script.
$ErrorActionPreference = "Continue"
$S = $PSScriptRoot
if (-not $GameDir) { $GameDir = $S }
$UI = Join-Path $S "ui.ps1"
$Log = Join-Path $S "mpclient.log"
function L($m) { $line = "[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $m; $line | Add-Content $Log; $line }
function Click($p, $x, $y) { & $UI -Action click -X $x -Y $y -ProcId $p | Out-Null }
function Key($p, $k) { & $UI -Action key -Keys $k -ProcId $p | Out-Null }
function Shot($n) { & $UI -Action shot -Out (Join-Path $S "mp_$n.png") -ProcId $p.Id | Out-Null }
Add-Type -AssemblyName System.Windows.Forms
"start" | Set-Content $Log
if ($Attach) {
    $p = Get-Process terranx_opensmacx -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $p) { L "attach: game not running"; exit 1 }
    L "attached to pid $($p.Id)"
    & $UI -Action focus -ProcId $p.Id | Out-Null
    Shot "attach_start"
    for ($i = 0; $i -lt (12 + $Turns * 4); $i++) {
        Start-Sleep 8
        $pp = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
        $alive = $pp -and -not $pp.HasExited
        Shot ("poll_{0:D2}" -f $i)
        L ("poll $i alive=$alive cpu=" + [int]($pp.CPU))
        if (-not $alive) { break }
        & $UI -Action focus -ProcId $p.Id | Out-Null
        Key $p.Id "{ENTER}"
        Click $p.Id 707 738                               # planetfall OK (harmless otherwise)
        Click $p.Id 899 584                               # TURN COMPLETE
        Key $p.Id "{ENTER}"
    }
    L "done"
    exit 0
}
Remove-Item (Join-Path $S "mp_*.png") -ErrorAction SilentlyContinue
# PRACX: render and window at 1024x768 so the original layout and coordinates apply
$ini = Join-Path $GameDir "Alpha Centauri.Ini"
$txt = Get-Content $ini -Raw
foreach ($k in @("ScreenWidth=1024", "ScreenHeight=768", "WindowWidth=1024", "WindowHeight=768")) {
    $key = $k.Split("=")[0]
    $txt = [regex]::Replace($txt, "(?m)^$key=.*$", $k)
}
Set-Content $ini $txt -Encoding ASCII
L ("ini PRACX: " + ((Select-String -Path $ini -Pattern '^(Screen|Window)(Width|Height)=' | ForEach-Object { $_.Line }) -join ' '))
L "host=$HostIP gamedir=$GameDir"
L ("ping host: " + (Test-Connection $HostIP -Count 1 -Quiet))
$p = Start-Process -FilePath (Join-Path $GameDir "terranx_opensmacx.exe") -WorkingDirectory $GameDir -PassThru
L "launched pid $($p.Id)"
Start-Sleep 12; $p.Refresh()
& $UI -Action focus -ProcId $p.Id | Out-Null
L (& $UI -Action status -ProcId $p.Id)
Shot "00_title"
Click $p.Id 512 384; Start-Sleep 9                         # title screen -> main menu
L (& $UI -Action status -ProcId $p.Id)
Shot "01_menu"
if ($Probe) {
    Click $p.Id 835 512; Start-Sleep 4; Shot "02_probe_load"  # Load Game dialog should appear
    Key $p.Id "{ESC}"; Start-Sleep 2; Shot "03_probe_back"
    L "probe done; quitting"
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    exit 0
}
Click $p.Id 835 587; Start-Sleep 4; Shot "02_mp"          # MULTIPLAYER
Click $p.Id 510 403; Start-Sleep 1                        # Internet TCP/IP Connection For DirectPlay
Click $p.Id 389 449; Start-Sleep 8; Shot "03_hostjoin"    # OK (a firewall prompt may appear here)
Key $p.Id "{ENTER}"; Start-Sleep 2                        # in case the firewall prompt took focus
& $UI -Action focus -ProcId $p.Id | Out-Null
Click $p.Id 510 405; Start-Sleep 1                        # Join
Click $p.Id 389 435; Start-Sleep 5; Shot "04_name"        # OK
Click $p.Id 570 377; Start-Sleep 1
Key $p.Id "{END}{BACKSPACE 40}$Name"; Start-Sleep 1
Click $p.Id 389 416; Start-Sleep 8; Shot "05_locate"      # OK -> DirectPlay "Locate Session" dialog
[System.Windows.Forms.SendKeys]::SendWait("$HostIP{ENTER}"); Start-Sleep 12; Shot "06_pick"
Click $p.Id 510 378; Start-Sleep 1                        # pick the first listed game
Click $p.Id 510 411; Start-Sleep 8; Shot "07_lobby"       # OK -> lobby
Click $p.Id 546 306; Start-Sleep 3; Shot "08_ready"       # MAKE READY
L "lobby flow done; waiting for the host to launch"
# game phase: dismiss popups and end turns until the process dies or the turn budget is used up
for ($i = 0; $i -lt (12 + $Turns * 4); $i++) {
    Start-Sleep 8
    $pp = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
    $alive = $pp -and -not $pp.HasExited
    Shot ("poll_{0:D2}" -f $i)
    L ("poll $i alive=$alive cpu=" + [int]($pp.CPU))
    if (-not $alive) { break }
    & $UI -Action focus -ProcId $p.Id | Out-Null
    Key $p.Id "{ENTER}"
    Click $p.Id 707 738                                   # planetfall OK (harmless otherwise)
    Click $p.Id 899 584                                   # TURN COMPLETE
    Key $p.Id "{ENTER}"
}
L "done"
