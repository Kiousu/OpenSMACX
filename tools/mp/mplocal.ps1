param(
    [ValidateSet("host", "join", "launch", "turns", "shots", "kill")][string]$Phase = "host",
    [string]$DirA = "G:\SMACX-opensmacx-test",
    [string]$DirB = "G:\SMACX-opensmacx-test2",
    [int]$Turns = 6
)
# Two game instances on ONE desktop, both PRACX-based builds in 1024x768 windows (set the [PRACX]
# Screen/Window keys in both inis first). Phases: host (A hosts, stays in lobby), join (B joins
# over 127.0.0.1 and makes ready), launch (A presses LAUNCH GAME), turns (both end turns),
# shots (screenshot both), kill. Pids are kept in pidA.txt / pidB.txt next to this script.
$S = $PSScriptRoot
$UI = Join-Path $S "ui.ps1"
$Log = Join-Path $S "mplocal.log"
function L($m) { $line = "[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $m; $line | Add-Content $Log; $line }
function Click($p, $x, $y) { & $UI -Action click -X $x -Y $y -ProcId $p | Out-Null }
function Key($p, $k) { & $UI -Action key -Keys $k -ProcId $p | Out-Null }
function Shot($p, $n) { & $UI -Action focus -ProcId $p | Out-Null; Start-Sleep -Milliseconds 800; & $UI -Action shot -Out (Join-Path $S "ml_$n.png") -ProcId $p | Out-Null }
function Launch($dir, $tag) {
    $p = Start-Process -FilePath (Join-Path $dir "terranx_opensmacx.exe") -WorkingDirectory $dir -PassThru
    $p.Id | Out-File (Join-Path $S "pid$tag.txt")
    Start-Sleep 12
    & $UI -Action focus -ProcId $p.Id | Out-Null
    Click $p.Id 512 384; Start-Sleep 8                     # title screen -> menu
    $null = L ("$tag pid $($p.Id) " + (& $UI -Action status -ProcId $p.Id))
    return [int]$p.Id
}
Add-Type -AssemblyName System.Windows.Forms
switch ($Phase) {
  "host" {
    "start" | Set-Content $Log
    [int]$a = Launch $DirA "A"
    Click $a 835 587; Start-Sleep 4                        # MULTIPLAYER
    Click $a 510 403; Start-Sleep 1                        # TCP/IP
    Click $a 389 449; Start-Sleep 6                        # OK
    Click $a 510 383; Start-Sleep 1                        # Host
    Click $a 389 435; Start-Sleep 5                        # OK
    Click $a 570 365; Start-Sleep 1; Key $a "{END}{BACKSPACE 40}osxtest"; Start-Sleep 1
    Click $a 570 388; Start-Sleep 1; Key $a "{END}{BACKSPACE 40}HostA"; Start-Sleep 1
    Click $a 389 427; Start-Sleep 8                        # OK -> lobby
    Shot $a "A_lobby"
    L "host in lobby"
  }
  "join" {
    [int]$b = Launch $DirB "B"
    Click $b 835 587; Start-Sleep 4
    Click $b 510 403; Start-Sleep 1
    Click $b 389 449; Start-Sleep 6
    Click $b 510 405; Start-Sleep 1                        # Join
    Click $b 389 435; Start-Sleep 5
    Click $b 570 377; Start-Sleep 1; Key $b "{END}{BACKSPACE 40}ClientB"; Start-Sleep 1
    Click $b 389 416; Start-Sleep 8                        # OK -> Locate Session
    Shot $b "B_locate"
    & $UI -Action dlg -Title "Locate Session" -Keys "127.0.0.1{ENTER}" | Out-Null; Start-Sleep 10
    Shot $b "B_pick"
    Click $b 510 378; Start-Sleep 1                        # first game
    Click $b 510 411; Start-Sleep 8                        # OK -> lobby
    Shot $b "B_lobby"
    Click $b 546 306; Start-Sleep 3                        # MAKE READY
    Shot $b "B_ready"
    L "client joined"
  }
  "launch" {
    $a = [int](Get-Content (Join-Path $S "pidA.txt")); $b = [int](Get-Content (Join-Path $S "pidB.txt"))
    Shot $a "A_lobby_joined"
    Click $a 546 306; Start-Sleep 4                        # LAUNCH GAME
    Click $a 510 383; Start-Sleep 1                        # Yes (if asked)
    Click $a 510 435; Start-Sleep 30                       # OK
    Shot $a "A_started"; Shot $b "B_started"
    foreach ($p in @($a, $b)) { & $UI -Action focus -ProcId $p | Out-Null; Start-Sleep 1; Key $p "{ENTER}"; Start-Sleep 2; Click $p 707 738; Start-Sleep 3 }
    Shot $a "A_planetfall"; Shot $b "B_planetfall"
    L "launched"
  }
  "turns" {
    $a = [int](Get-Content (Join-Path $S "pidA.txt")); $b = [int](Get-Content (Join-Path $S "pidB.txt"))
    for ($t = 0; $t -lt $Turns; $t++) {
        foreach ($p in @($a, $b)) {
            & $UI -Action focus -ProcId $p | Out-Null; Start-Sleep 1
            Key $p "{ENTER}"; Click $p 899 584; Start-Sleep 4; Key $p "{ENTER}"
        }
        Start-Sleep 4
        Shot $a ("A_turn{0:D2}" -f $t); Shot $b ("B_turn{0:D2}" -f $t)
        L ("turn $t A=" + (& $UI -Action status -ProcId $a) + " B=" + (& $UI -Action status -ProcId $b))
    }
  }
  "shots" {
    $a = [int](Get-Content (Join-Path $S "pidA.txt")); $b = [int](Get-Content (Join-Path $S "pidB.txt"))
    Shot $a "A_now"; Shot $b "B_now"
    & $UI -Action status -ProcId $a; & $UI -Action status -ProcId $b
  }
  "kill" {
    Get-Process terranx_opensmacx -ErrorAction SilentlyContinue | Stop-Process -Force
    "killed"
  }
}
