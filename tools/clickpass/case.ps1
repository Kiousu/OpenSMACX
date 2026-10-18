param([Parameter(Mandatory = $true)][string]$CaseFile, [Parameter(Mandatory = $true)][string]$GameDir,
      [Parameter(Mandatory = $true)][string]$OutDir, [int]$StepTimeoutSec = 30, [int]$TurnTimeoutSec = 150,
      [string]$OutsideRoot = "G:\SMACX-clickpass-outside",
      [Parameter(DontShow = $true)][switch]$NoClickpass,       # for the driver's own hang check only
      [Parameter(DontShow = $true)][string]$LiveDir = "G:\Sid Meier's Alpha Centauri")  # (other value: checks only)
# Click pass case driver: plays one case file (tools/clickpass/cases/*.case) in one test folder's game
# and has the harness DLL's click-pass mode (src/clickpass.cpp) dump the game state after every step.
#
# Case files have one step per line; blank lines and lines starting with # are skipped.
#   save <file.SAV>   the first step: the save to load, from <GameDir>\saves
#   keys <spec>       keys posted to the game window (SendKeys syntax, see tools/mp/msg.ps1; spaces at
#                     either end are dropped, use {SPACE})
#   click <x> <y>     a left click (game coordinates, client area 1024x768)
#   rclick <x> <y>    a right click
#   wait <ms>         a pause before the next step (no dump)
#   endturn           TURN COMPLETE, "Yes, end the turn" if the Operations Director asks, then Enter on
#                     popups and OK on base screens until the turn counter moves on, as
#                     tools/mp/autoboot.ps1 does (at most -TurnTimeoutSec); start it from the map
# The state is dumped once the game is ready (step 0) and after every other step but wait (steps
# 1, 2, ...): the driver syncs with the game (WM_NULL, 5 s), waits 300 ms, syncs again, posts
# "OpenSMACX.ClickpassDump" with the step number and waits up to -StepTimeoutSec for
# clickpass\dump_<step>.bin. With each dump it lists the files the game has written so far:
# <OutDir>\files_<step>.json holds {"step", "files": [...]}, one entry per file under the game folder
# ("game\<path>") or the outside folder ("outside\<path>") that is new, changed (size or last write
# time) or gone since just before the launch: {"path", "size", "sha256"}, {"path", "deleted": true},
# or {"path", "error"} when it can't be read. Each content is kept once, as <OutDir>\files\<sha256>.
# Not listed: clickpass\ (the harness's dumps and guard.log), the harness's logs
# (clickpass_errors.log, opensmacx_regress.log), the trigger file and a bisect's files
# (clickpass_bisect_swap.json, clickpass_bisect_original.exe).
#
# Before the case it clears <GameDir>\clickpass\, clickpass_errors.log, opensmacx_regress.log,
# saves\_cp_* (cases that save use names starting with _cp_, so every run starts from the same
# folder) and saves\auto\ (the game's autosaves at the end of a turn, which it names and rotates by
# what is already there), and empties <OutsideRoot>\<game folder name>, the one folder outside the
# game folder the game may write to. It keeps a copy of <GameDir>\Alpha Centauri.ini, which the game writes (Latest
# Save), and puts it back after its game has ended, so every case starts from the same ini. It starts
# the game in the background with the trigger lines "load saves\<save>", "notests", "clickpass",
# "outside <that folder>" and waits up to 60 s from the launch for clickpass\ready. Right after the
# launch Start-Game writes <OutDir>\game.pid ("<pid> <start time as a UTC FILETIME>"): if Start-Game
# fails after the launch, this script stops that process from it (tools/clickpass/folders.ps1,
# PidFileGame); if this script is ended before its own clean-up (the runner's timeout, Ctrl+C), the
# runner does (tools/clickpass/runner.py, stop_game). guard.log is checked during every wait and after
# every dump. At the end it ends the game by its PID, copies the dumps, clickpass\guard.log and
# opensmacx_regress.log (those that are there) to -OutDir and writes <OutDir>\result.json:
#   case, save   the case file's name (without .case) and its save
#   steps        how many dumps the case calls for (step 0 and one per step but wait)
#   outcome      ok; crash (the game ended before the case did); hang (ready, a dump or the next turn
#                did not come in time); blocked-write (clickpass\guard.log has lines: the write guard
#                refused a write outside the allowed folders); setup-failed (the case could not be
#                run: a bad case file, a missing save, a game already running in the folder, ...)
#   at_step      the step it ended at (ready counts as step 0); null for ok and setup-failed
#   exit_code    the game's exit code for a crash, unsigned (3221225477 = 0xC0000005); else null
#   seconds      the case's wall time
#   detail       what happened, in words ("" for ok)
#   errors       (unless ok) the lines of <GameDir>\clickpass_errors.log, the harness's own failed
#                writes, if any
#   blocked      (if guard.log has lines) guard.log's WRITE-BLOCKED lines
#   warning      (if any) what went wrong in the clean-up, such as an ini it could not put back
# Exit code 0 when the outcome is ok, else 1. It only ends the game it started (by PID) and only
# removes a trigger file it wrote. Before touching anything it refuses (setup-failed, nothing deleted
# or written but result.json) a -GameDir that is, is in or holds the live game folder
# (G:\Sid Meier's Alpha Centauri) or has no terranx_opensmacx.exe, and an outside folder that is, is in
# or holds the game folder or the live folder; an -OutDir in the live folder ends it with no output.
$ErrorActionPreference = "Stop"
$started = Get-Date
$GameDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($GameDir)   # full paths
$OutDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutDir)
$OutsideRoot = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutsideRoot)
$script:GameDir = $GameDir.TrimEnd('\')
. "$PSScriptRoot\..\mp\msg.ps1"
. "$PSScriptRoot\folders.ps1"                                         # FullPath, Within, PidFileGame, StopGame
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class ClickpassCase {
  [DllImport("user32.dll", CharSet = CharSet.Ansi)] public static extern uint RegisterWindowMessageA(string name);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageTimeoutA(IntPtr h, uint m, IntPtr w, IntPtr l,
      uint flags, uint timeout, out IntPtr result);
}
"@
$G = $script:GameDir
$Dir = "$G\clickpass"
$GuardLog = "$Dir\guard.log"
$ErrorsLog = "$G\clickpass_errors.log"
$HarnessLog = "$G\opensmacx_regress.log"
$Trigger = "$G\opensmacx_regress.txt"
$PidFile = "$OutDir\game.pid"
$IniPath = "$G\Alpha Centauri.ini"
$Outside = $OutsideRoot.TrimEnd('\') + "\" + (Split-Path $G -Leaf)
$ReadyTimeoutSec = 60
$Result = [ordered]@{ case = [IO.Path]::GetFileNameWithoutExtension($CaseFile); save = ""; steps = 0;
    outcome = $null; at_step = $null; exit_code = $null; seconds = 0.0; detail = "" }
$p = $null
$Baseline = @{}                                                       # the folders' files before the launch
$IniBytes = $null                                                     # Alpha Centauri.ini before the launch
$Sha = [Security.Cryptography.SHA256]::Create()
# in the game folder, but not the game's: the harness's logs, the trigger and a bisect's files
$NotGameFiles = @("clickpass_errors.log", "opensmacx_regress.log", "opensmacx_regress.txt",
    "clickpass_bisect_swap.json", "clickpass_bisect_swap.json.tmp", "clickpass_bisect_original.exe")

function Say([string]$Text) { Write-Host $Text }      # progress (Write-Host: function results stay clean)
function Lines([string]$Path) {        # plain strings (Get-Content's carry properties into the JSON)
    if (Test-Path -LiteralPath $Path) { return @([IO.File]::ReadAllLines($Path, [Text.Encoding]::Default) | Where-Object { $_ }) }
    return @()
}
function Blocked { return (Test-Path -LiteralPath $GuardLog) -and (Get-Item -LiteralPath $GuardLog).Length -gt 0 }

# "game\<path>" / "outside\<path>" -> (size, last write time) of every file in the game folder (but
# clickpass\ and $NotGameFiles) and in the outside folder.
function Listing {
    $list = @{}                                                       # (keys ignore case, as the file system)
    foreach ($root in @(@("game", $G), @("outside", $Outside))) {
        if (-not (Test-Path -LiteralPath $root[1] -PathType Container)) { continue }
        $skip = $root[1].TrimEnd('\').Length + 1
        foreach ($path in [IO.Directory]::EnumerateFiles($root[1], "*", [IO.SearchOption]::AllDirectories)) {
            $rel = $path.Substring($skip)
            if ($root[0] -eq "game" -and ($rel.StartsWith("clickpass\", [StringComparison]::OrdinalIgnoreCase) -or
                $NotGameFiles -contains $rel)) { continue }
            $info = New-Object IO.FileInfo $path
            $list[$root[0] + "\" + $rel] = @($info.Length, $info.LastWriteTimeUtc.Ticks)
        }
    }
    return $list
}

# A file's content, read while the game may still have it open.
function FileBytes([string]$Path) {
    $fs = New-Object IO.FileStream($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read,
        ([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
    try {
        $ms = New-Object IO.MemoryStream
        $fs.CopyTo($ms)
        return , $ms.ToArray()
    } finally {
        $fs.Dispose()
    }
}

# <OutDir>\files_<step>.json: the files written since the launch (see the top), contents kept in
# <OutDir>\files\.
function RecordFiles([int]$Step) {
    $now = Listing
    $entries = New-Object System.Collections.ArrayList
    foreach ($name in @(@($now.Keys) + @($Baseline.Keys) | Sort-Object -Unique)) {
        $was = $Baseline[$name]
        $is = $now[$name]
        if ($was -and $is -and $was[0] -eq $is[0] -and $was[1] -eq $is[1]) { continue }
        if (-not $is) { [void]$entries.Add([ordered]@{ path = $name; deleted = $true }); continue }
        $sep = $name.IndexOf('\')
        $full = $(if ($name.Substring(0, $sep) -eq "game") { $G } else { $Outside }) + $name.Substring($sep)
        try {
            $bytes = FileBytes $full
            $sha256 = [BitConverter]::ToString($Sha.ComputeHash($bytes)).Replace("-", "").ToLowerInvariant()
            $copy = "$OutDir\files\$sha256"
            if (-not (Test-Path -LiteralPath $copy)) { [IO.File]::WriteAllBytes($copy, $bytes) }
            [void]$entries.Add([ordered]@{ path = $name; size = $bytes.Length; sha256 = $sha256 })
        } catch {
            [void]$entries.Add([ordered]@{ path = $name; error = "$_" })
        }
    }
    $json = ConvertTo-Json -InputObject ([ordered]@{ step = $Step; files = @($entries) }) -Depth 4
    [IO.File]::WriteAllText("$OutDir\files_$Step.json", $json + "`r`n", (New-Object Text.UTF8Encoding $false))
}

# Ends the case with this outcome; the first outcome set stands.
function Fail([string]$Outcome, $Step, [string]$Detail) {
    if ($Result.outcome) { return }
    $Result.outcome = $Outcome
    $Result.at_step = $Step
    $Result.detail = $Detail
    if ($Outcome -eq "crash") {
        $code = [int64]$p.ExitCode
        if ($code -lt 0) { $code += 4294967296 }
        $Result.exit_code = $code
    }
}
function ExitText { return "the game ended (exit code 0x{0:X8})" -f $p.ExitCode }

# Polls every $PollMs until & $Done is true (returns $true), or ends the case and returns $false: crash
# when the game has ended, blocked-write when guard.log has lines, hang $Seconds after $Since.
# ($Done runs in this function's scope: it must not use the names of these parameters.)
function Await([int]$Step, [scriptblock]$Done, [double]$Seconds, [string]$What, [datetime]$Since, [int]$PollMs = 250) {
    while ($true) {
        if ($p.HasExited) { Fail "crash" $Step ("{0} before {1}" -f (ExitText), $What); return $false }
        if (Blocked) { Fail "blocked-write" $Step "the write guard refused a write (clickpass\guard.log) before $What"; return $false }
        if (& $Done) { return $true }
        if (((Get-Date) - $Since).TotalSeconds -ge $Seconds) { Fail "hang" $Step "no $What within $Seconds s"; return $false }
        Start-Sleep -Milliseconds $PollMs
    }
}

# Waits until the game has handled everything sent so far (or 5 s).
function Sync {
    $r = [IntPtr]::Zero
    [ClickpassCase]::SendMessageTimeoutA((Hwnd $p), 0, [IntPtr]::Zero, [IntPtr]::Zero, 0, 5000, [ref]$r) | Out-Null
}

function Dump([int]$Step) {
    $t = Get-Date
    Sync; Start-Sleep -Milliseconds 300; Sync
    Post $p $DumpMessage $Step 0
    $file = "$Dir\dump_$Step.bin"
    if (-not (Await $Step { Test-Path -LiteralPath $file } $StepTimeoutSec "clickpass\dump_$Step.bin" $t)) { return $false }
    if (Blocked) { Fail "blocked-write" $Step "the write guard refused a write (clickpass\guard.log) in step $Step"; return $false }
    RecordFiles $Step
    Say ("  dump_{0}.bin after {1:N1} s" -f $Step, ((Get-Date) - $t).TotalSeconds)
    return $true
}

# TURN COMPLETE, then as tools/mp/autoboot.ps1: the Operations Director's "Yes, end the turn." and OK,
# then Enter on popups and OK on base screens until the turn counter moves on.
function EndTurn([int]$Step) {
    $turnStart = Get-Date
    $turnBefore = Turn $p
    if ($null -eq $turnBefore) { throw "the turn counter can't be read" }
    Click $p 899 584; Start-Sleep 2                                   # TURN COMPLETE
    if (-not $p.HasExited -and (PopupOpen $p)) {                      # Operations Director: units left to move
        Click $p 510 372; Start-Sleep 1                               # "Yes, end the turn."
        Click $p 510 402; Start-Sleep 1                               # OK
    }
    $moved = {
        $turnNow = Turn $p
        if ($null -ne $turnNow -and $turnNow -gt $turnBefore) { $true }
        else {
            if (BaseScreenOpen $p) { Click $p 648 523 }               # the base screen's OK
            elseif (PopupOpen $p) { Keys $p "{ENTER}" }
            $false
        }
    }
    if (-not (Await $Step $moved $TurnTimeoutSec "turn $($turnBefore + 1)" $turnStart 2000)) { return $false }
    Say ("  turn {0} -> {1} after {2:N0} s" -f $turnBefore, (Turn $p), ((Get-Date) - $turnStart).TotalSeconds)
    return $true
}

function Read-Case([string]$Path) {
    $save = $null
    $steps = New-Object System.Collections.ArrayList
    $n = 0
    foreach ($raw in @(Get-Content -LiteralPath $Path)) {
        $n++
        $line = $raw.Trim()
        if (-not $line -or $line.StartsWith('#')) { continue }
        $sp = $line.IndexOf(' ')
        $verb = if ($sp -lt 0) { $line } else { $line.Substring(0, $sp) }
        $arg = if ($sp -lt 0) { "" } else { $line.Substring($sp + 1).Trim() }
        $bad = "line ${n} of $(Split-Path $Path -Leaf): '$line'"
        if (-not $save) {
            if ($verb -cne "save" -or -not $arg) { throw "$bad (the first step must be 'save <file.SAV>')" }
            $save = $arg
            continue
        }
        $step = [pscustomobject]@{ Kind = $verb; Arg = $arg; X = 0; Y = 0; Ms = 0; Text = $line }
        if ($verb -ceq "keys") {
            if (-not $arg -or $arg -match '\{[^}]*(\{|$)') { throw "$bad (keys needs a SendKeys spec with closed braces)" }
        } elseif ($verb -ceq "click" -or $verb -ceq "rclick") {
            if ($arg -notmatch '^(\d+)\s+(\d+)$') { throw "$bad ($verb needs <x> <y>)" }
            $step.X = [int]$Matches[1]; $step.Y = [int]$Matches[2]
        } elseif ($verb -ceq "wait") {
            if ($arg -notmatch '^\d+$') { throw "$bad (wait needs <ms>)" }
            $step.Ms = [int]$arg
        } elseif ($verb -ceq "endturn") {
            if ($arg) { throw "$bad (endturn takes nothing)" }
        } else {
            throw "$bad (unknown step; save only comes first)"
        }
        $steps.Add($step) | Out-Null
    }
    if (-not $save) { throw "$(Split-Path $Path -Leaf) has no 'save <file.SAV>' line" }
    return [pscustomobject]@{ Save = $save; Steps = $steps }
}

# Folder sanity, before anything is deleted or written: never a game in, around or next to the live
# folder, never an outside folder that overlaps the game folder or the live folder.
try { $outDirInLive = Within (FullPath $OutDir) (FullPath $LiveDir) } catch { $outDirInLive = $true }  # unsure: refuse
if ($outDirInLive) {
    Say "refused: -OutDir $OutDir is (in) the live game folder $LiveDir or can't be checked (no result.json written)"
    exit 1
}
$launched = $false                                                    # Start-Game was called
$completed = $false                                                   # every step done, the game alive
$triggerStamp = $null                                                 # the trigger this driver wrote, if the game left it
$at = 0                                                               # the step under way
try {
    $refusal = LiveRefusal $GameDir "the game folder" $LiveDir         # (folders.ps1, as the smoke checks)
    if ($refusal) { throw $refusal }
    $LiveFull = FullPath $LiveDir
    $GameFull = FullPath $GameDir                                     # (not $G: "G:\" would become "G:")
    if (-not (Test-Path -LiteralPath $G -PathType Container) -or
        -not (Test-Path -LiteralPath "$G\terranx_opensmacx.exe" -PathType Leaf)) {
        throw "refused: $GameDir is not a game folder with terranx_opensmacx.exe"
    }
    $OutsideFull = FullPath $Outside
    foreach ($other in @(@($GameFull, "the game folder"), @($LiveFull, "the live game folder"))) {
        if ((Within $OutsideFull $other[0]) -or (Within $other[0] $OutsideFull)) {
            throw "refused: the outside folder $Outside is, is in or holds $($other[1]) $($other[0])"
        }
    }
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null                # no results of an earlier run
    Get-ChildItem -LiteralPath $OutDir -File | Where-Object {
        $_.Name -like "dump_*.bin" -or $_.Name -like "files_*.json" -or
        $_.Name -in @("guard.log", "opensmacx_regress.log", "result.json", "game.pid") } |
        Remove-Item -Force
    if (Test-Path -LiteralPath "$OutDir\files") { Remove-Item -LiteralPath "$OutDir\files" -Recurse -Force }
    New-Item -ItemType Directory -Path "$OutDir\files" | Out-Null
    $case = Read-Case $CaseFile
    $Result.save = $case.Save
    $Result.steps = 1 + @($case.Steps | Where-Object { $_.Kind -ne "wait" }).Count
    if (-not (Test-Path -LiteralPath "$G\saves\$($case.Save)")) { throw "there is no $G\saves\$($case.Save)" }
    if (GameRunningIn $G) { throw "a game from $G is already running" }
    $DumpMessage = [ClickpassCase]::RegisterWindowMessageA("OpenSMACX.ClickpassDump")
    if (-not $DumpMessage) { throw "RegisterWindowMessageA failed" }

    # the same starting point every time: no leftovers from an earlier case in the folders
    if (Test-Path -LiteralPath $Dir) { Remove-Item -LiteralPath $Dir -Recurse -Force }
    foreach ($f in @($ErrorsLog, $HarnessLog)) { if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force } }
    Get-ChildItem -LiteralPath "$G\saves" -File | Where-Object { $_.Name -like "_cp_*" } | Remove-Item -Force
    if (Test-Path -LiteralPath "$G\saves\auto") {                     # the game numbers its autosaves by what is there
        Get-ChildItem -LiteralPath "$G\saves\auto" -Force | Remove-Item -Recurse -Force
    }
    if (Test-Path -LiteralPath $Outside) { Get-ChildItem -LiteralPath $Outside -Force | Remove-Item -Recurse -Force }
    else { New-Item -ItemType Directory -Force -Path $Outside | Out-Null }
    if (Test-Path -LiteralPath $IniPath) { $IniBytes = [IO.File]::ReadAllBytes($IniPath) }  # put back at the end
    $Baseline = Listing                                               # what the game writes is compared

    $extra = @()
    if (-not $NoClickpass) { $extra += "clickpass" }
    $extra += "outside $Outside"
    $launch = Get-Date
    $launched = $true
    $p = Start-Game $case.Save -Background -Extra $extra -PidFile $PidFile
    if (Test-Path -LiteralPath $Trigger) { $triggerStamp = (Get-Item -LiteralPath $Trigger).LastWriteTimeUtc }  # not taken
    Say ("game started (pid {0}), {1} {2} after {3:N0} s" -f $p.Id, $case.Save,
        $(if ($triggerStamp) { "not loaded (trigger not taken)" } else { "loaded" }), ((Get-Date) - $launch).TotalSeconds)

    $ok = Await 0 { Test-Path -LiteralPath "$Dir\ready" } $ReadyTimeoutSec "clickpass\ready" $launch
    if ($ok) {
        Say ("ready after {0:N0} s" -f ((Get-Date) - $launch).TotalSeconds)
        $ok = Dump 0
    }
    $n = 0
    foreach ($s in $case.Steps) {
        if (-not $ok) { break }
        if ($s.Kind -eq "wait") {                                     # belongs to the step before it
            Start-Sleep -Milliseconds $s.Ms
            if ($p.HasExited) { Fail "crash" $n ("{0} in the wait after step {1}" -f (ExitText), $n); $ok = $false; break }
            if (Blocked) {
                Fail "blocked-write" $n "the write guard refused a write (clickpass\guard.log) in the wait after step $n"
                $ok = $false; break
            }
            continue
        }
        $n++
        $at = $n
        Say "step ${n}: $($s.Text)"
        if ($p.HasExited) { Fail "crash" $n ("{0} before step {1}" -f (ExitText), $n); $ok = $false; break }
        switch ($s.Kind) {
            "keys" { Keys $p $s.Arg }
            "click" { Click $p $s.X $s.Y }
            "rclick" { Click $p $s.X $s.Y -Right }
            "endturn" { $ok = EndTurn $n }
        }
        if ($ok) { $ok = Dump $n }
    }
    if ($ok) {
        if ($p.HasExited) { Fail "crash" $n ("{0} after the last step" -f (ExitText)) }
        else { $completed = $true }                                   # ok unless guard.log says otherwise below
    }
} catch {
    if ($p -and $p.HasExited) { Fail "crash" $at ("{0} in step {1}" -f (ExitText), $at) }  # (the error follows from it)
    elseif (-not $launched) { Fail "setup-failed" $null "$_" }
    elseif (-not $p) { Fail "setup-failed" $null "starting the game failed: $_" }
    else { Fail "setup-failed" $null "driver error in step ${at}: $_" }
} finally {
    # Only the game this driver started, by its PID: $p, or, when Start-Game threw after the launch, the
    # process game.pid names (PidFileGame: still running from this folder, started at that time). When
    # Start-Game threw before the launch (e.g. another driver's game runs here) there is none, and
    # nothing is stopped, removed or put back.
    $game = $p
    if (-not $game -and $launched) {
        $game = PidFileGame $PidFile $G
        if ($game) { Say "Start-Game failed after the launch: its game (pid $($game.Id), from game.pid) is stopped" }
    }
    if ($game) {
        try {   # the trigger it wrote and the game never took, if nobody has written it since
            if ($p) {
                $mine = $triggerStamp -and (Test-Path -LiteralPath $Trigger) -and
                    (Get-Item -LiteralPath $Trigger).LastWriteTimeUtc -eq $triggerStamp
            } else {   # (no other driver writes it while this game runs)
                $mine = -not $game.HasExited -and (Test-Path -LiteralPath $Trigger)
            }
            if ($mine) { Remove-Item -LiteralPath $Trigger -Force }
        } catch {
            Say "the trigger file could not be removed: $_"
        }
        try { StopGame $game } catch { Say "stopping pid $($game.Id) failed: $_" }
        if ($null -ne $IniBytes) {
            try { [IO.File]::WriteAllBytes($IniPath, $IniBytes) }
            catch { $Result.warning = "Alpha Centauri.ini could not be put back: $_"; Say $Result.warning }
        }
    }
}
# guard.log read once, with the game ended: the last check before ok, and the "blocked" lines
$blockedLines = @(if ($p) { Lines $GuardLog })                       # (without a game of ours: stale or not ours)
if ($completed -and -not $Result.outcome) {
    if ($blockedLines.Count) {
        Fail "blocked-write" $n "the write guard refused a write (clickpass\guard.log) after the last dump"
    } else {
        $Result.outcome = "ok"
    }
}
if ($blockedLines.Count) { $Result.blocked = $blockedLines }

if ($p) {                                                             # (otherwise they are stale or not ours)
    if (Test-Path -LiteralPath $Dir) {
        Get-ChildItem -LiteralPath $Dir -File | Where-Object { $_.Name -like "dump_*.bin" -or $_.Name -eq "guard.log" } |
            Copy-Item -Destination $OutDir -Force
    }
    if (Test-Path -LiteralPath $HarnessLog) { Copy-Item -LiteralPath $HarnessLog -Destination $OutDir -Force }
    if ($Result.outcome -ne "ok") { $Result.errors = @(Lines $ErrorsLog) }
} elseif ($Result.outcome -ne "ok") {
    $Result.errors = @()
}
$Result.seconds = [math]::Round(((Get-Date) - $started).TotalSeconds, 1)
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
[IO.File]::WriteAllText("$OutDir\result.json", (ConvertTo-Json -InputObject $Result -Depth 3) + "`r`n",
    (New-Object Text.UTF8Encoding $false))
$where = if ($null -ne $Result.at_step) { " at step $($Result.at_step)" } else { "" }
Say ("{0}: {1}{2} after {3:N1} s{4}" -f $Result.case, $Result.outcome, $where, $Result.seconds,
    $(if ($Result.detail) { " ($($Result.detail))" } else { "" }))
if ($Result.outcome -eq "ok") { exit 0 }
exit 1
