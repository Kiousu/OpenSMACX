param(
    [string]$Exe = "terranx_opensmacx.exe",   # exe inside $Dir to run
    [string]$Table = "",                      # if set: patch the pristine exe with this redirect table first
    [string]$Dir = "G:\SMACX-opensmacx-test",
    [string]$Tag = "solo",
    [int]$Wait = 35
)
# Host-only network game as the Pirates: open a TCP/IP lobby, set the host to Pirates, LAUNCH,
# dismiss planetfall, screenshot. Used to bisect the redirect table: a good run shows the Pirates'
# starting units; a bad run sits at "UPKEEP 6" with nothing on the map.
$S = $PSScriptRoot
$UI = Join-Path $S "ui.ps1"
$Repo = "C:\Programming\OpenSMACX"
function Click($p, $x, $y) { & $UI -Action click -X $x -Y $y -ProcId $p | Out-Null }
function Key($p, $k) { & $UI -Action key -Keys $k -ProcId $p | Out-Null }
function Shot($p, $n) { & $UI -Action shot -Out (Join-Path $S "$Tag`_$n.png") -ProcId $p | Out-Null }
Get-Process terranx_opensmacx, terranx -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 1
if ($Table) {
    $o = Join-Path $Repo "build\mpbisect\solo"
    & (Join-Path $Repo "build\venv\Scripts\python.exe") (Join-Path $Repo "tools\opensmacx_patch.py") -e "G:\Sid Meier's Alpha Centauri\_backup_v2.1\terranx.exe" -d (Join-Path $Repo "build\OpenSMACX.dll") -o $o -t $Table | Select-Object -Last 1
    Copy-Item (Join-Path $o "terranx_opensmacx.exe") (Join-Path $Dir "terranx_opensmacx.exe") -Force
    Copy-Item (Join-Path $Repo "build\OpenSMACX.dll") (Join-Path $Dir "OpenSMACX.dll") -Force
}
Add-Type -Name M -Namespace Q2 -MemberDefinition '[DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y); [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);'
[Q2.M]::SetCursorPos(500, 400) | Out-Null
$p = Start-Process -FilePath (Join-Path $Dir $Exe) -WorkingDirectory $Dir -PassThru
$p.Id | Out-File (Join-Path $S "pidA.txt")
Start-Sleep 12; $p.Refresh()
[Q2.M]::SetWindowPos($p.MainWindowHandle, [IntPtr]::Zero, 0, 0, 0, 0, 0x0005) | Out-Null
& $UI -Action focus -ProcId $p.Id | Out-Null
Click $p.Id 512 384; Start-Sleep 8                      # title -> menu (harmless if already there)
Click $p.Id 835 587; Start-Sleep 4                      # MULTIPLAYER
Click $p.Id 510 403; Start-Sleep 1                      # TCP/IP
Click $p.Id 389 449; Start-Sleep 6                      # OK
Click $p.Id 510 383; Start-Sleep 1                      # Host
Click $p.Id 389 435; Start-Sleep 5                      # OK
Click $p.Id 570 365; Start-Sleep 1; Key $p.Id "{END}{BACKSPACE 40}osxtest"; Start-Sleep 1
Click $p.Id 570 388; Start-Sleep 1; Key $p.Id "{END}{BACKSPACE 40}HostA"; Start-Sleep 1
Click $p.Id 389 427; Start-Sleep 8                      # OK -> lobby
Click $p.Id 777 152; Start-Sleep 2; Click $p.Id 808 345; Start-Sleep 2   # faction: Pirates
Shot $p.Id "lobby"
Click $p.Id 546 306; Start-Sleep 4                      # LAUNCH GAME
Click $p.Id 510 383; Start-Sleep 1                      # Yes (if asked)
Click $p.Id 510 435; Start-Sleep $Wait                  # OK
Shot $p.Id "started"
Key $p.Id "{ENTER}"; Start-Sleep 5; Key $p.Id "{ENTER}"; Start-Sleep 4
Shot $p.Id "planetfall"
& $UI -Action status -ProcId $p.Id
