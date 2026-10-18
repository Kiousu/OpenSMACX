param([string]$Save = "Council.SAV", [int]$Turns = 1, [int]$TurnTimeoutSec = 150, [string]$Tag = "autoboot",
      [switch]$Background, [string]$GameDir = "G:\SMACX-opensmacx-test")
# Boot / multi-turn test without the real mouse or keyboard (messages, see msg.ps1): the harness DLL
# loads $Save, then each turn: TURN COMPLETE, "Yes, end the turn" if the Operations Director asks,
# then Enter on whatever pops up during the other factions' moves, until the turn counter in the
# game's memory moves on. Needs the harness DLL and [OpenSMACX] Windowed=1. Log and screenshots in
# build\boot\.
$script:GameDir = $GameDir.TrimEnd('\')
. "$PSScriptRoot\msg.ps1"
$Out = if ($script:GameDir -eq "G:\SMACX-opensmacx-test") { "C:\Programming\OpenSMACX\build\boot" }
       else { "C:\Programming\OpenSMACX\build\boot_" + (Split-Path $script:GameDir -Leaf) }
New-Item -ItemType Directory -Force $Out | Out-Null
$Log = "$Out\$Tag.log"
function L($m) { $line = "[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $m; $line | Add-Content $Log; $line }
Remove-Item $Log -ErrorAction SilentlyContinue
$p = Start-Game $Save -Background:$Background
$first = Turn $p
L "loaded $Save, turn $first"
# Act only on what the game shows (read from its memory): the base screen is closed with its OK
# button, a popup gets Enter; with neither, TURN COMPLETE once per turn. Blind keys don't work:
# on the map, Enter opens the selected base.
function Settle($p, $seconds) {                                       # clear popups and base screens
    $quiet = 0; $t = Get-Date
    while ($quiet -lt 2 -and ((Get-Date) - $t).TotalSeconds -lt $seconds) {
        $p.Refresh(); if ($p.HasExited) { return }
        if (BaseScreenOpen $p) { Click $p 648 523; $quiet = 0 }
        elseif (PopupOpen $p) { Keys $p "{ENTER}"; $quiet = 0 }
        else { $quiet++ }
        Start-Sleep 1
    }
}
$ok = $true
for ($n = 1; $n -le $Turns -and $ok; $n++) {
    $t0 = Get-Date
    Settle $p 60
    $from = Turn $p
    Click $p 899 584; Start-Sleep 2                                   # TURN COMPLETE
    if (PopupOpen $p) {                                               # Operations Director: units left to move
        Click $p 510 372; Start-Sleep 1                               # "Yes, end the turn."
        Click $p 510 402; Start-Sleep 1                               # OK
    }
    $ok = $false
    while (((Get-Date) - $t0).TotalSeconds -lt $TurnTimeoutSec) {
        $p.Refresh()
        if ($p.HasExited) { L "game died during turn $from (exit code $($p.ExitCode))"; break }
        if ((Turn $p) -gt $from) { $ok = $true; break }
        if (BaseScreenOpen $p) { Click $p 648 523 }
        elseif (PopupOpen $p) { Keys $p "{ENTER}" }
        Start-Sleep 2
    }
    if ($ok) { L ("turn {0} -> {1} in {2:N0} s" -f $from, (Turn $p), ((Get-Date) - $t0).TotalSeconds) }
    elseif (-not $p.HasExited) { L "turn still $from after $TurnTimeoutSec s" }
    if (-not $p.HasExited) { Shot $p ("$Out\$Tag`_{0}.png" -f $n) }
}
$p.Refresh()
L "done: turn $first -> $(if ($p.HasExited) { '?' } else { Turn $p }), alive=$(-not $p.HasExited)"
if (-not $p.HasExited) { $p.Kill() }
Remove-Item "$($script:GameDir)\opensmacx_regress.txt" -ErrorAction SilentlyContinue
