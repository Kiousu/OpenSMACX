param([Parameter(Mandatory = $true)][string]$GameDir, [string]$Save = "sw_steam20.SAV",
      [string]$Exe = "terranx_opensmacx.exe")
# Smoke check of the click pass's error log (src/clickpass.cpp) in one test folder. With a file named
# "clickpass" in the game folder (so the clickpass folder can't be made) it starts the game in the
# background with the trigger lines "load saves\<save>", "notests" and "clickpass" and expects in
# <GameDir>\clickpass_errors.log
#   ERROR create-folder <GameDir>\clickpass (GetLastError <n>)
#   ERROR write-ready <GameDir>\clickpass\ready (errno <n>)
# then posts the dump message for step 0 and expects
#   ERROR open-dump <GameDir>\clickpass\dump_0.tmp (errno <n>)
# and no dump. With the game still running it then puts a real clickpass folder there that holds a
# folder named dump_1.bin, posts step 1 and expects
#   ERROR rename-dump <GameDir>\clickpass\dump_1.bin (GetLastError <n>)
# and no dump_1.tmp left behind; step 2 must then give dump_2.bin and no further line. Ends the game
# (by PID) and removes what it made (the clickpass file or folder and the error log).
# Prints "PASS" (exit code 0) or "FAIL: <why>" (exit code 1). It refuses, touching nothing, a -GameDir
# that is, is in or holds the live game folder, and a folder a game already runs from; it only ever
# stops the game it started (by PID, see folders.ps1).
$ErrorActionPreference = "Stop"
$GameDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($GameDir)   # full paths
$script:GameDir = $GameDir.TrimEnd('\')
. "$PSScriptRoot\..\..\mp\msg.ps1"
. "$PSScriptRoot\..\folders.ps1"
$refusal = LiveRefusal $GameDir
if ($refusal) { "FAIL: $refusal"; exit 1 }
if (GameRunningIn $script:GameDir) { "FAIL: a game from $($script:GameDir) is already running (not touched)"; exit 1 }
$pidFile = Join-Path ([IO.Path]::GetTempPath()) "clickpass_smoke_errors_$PID.pid"   # Start-Game's "<pid> <start>"
if (Test-Path -LiteralPath $pidFile) { Remove-Item -LiteralPath $pidFile -Force }
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class ClickpassErrors {
  [DllImport("user32.dll", CharSet = CharSet.Ansi)] public static extern uint RegisterWindowMessageA(string name);
}
"@
$dir = "$script:GameDir\clickpass"
$errors = "$script:GameDir\clickpass_errors.log"
$e = [regex]::Escape($dir)
$expected = @("^ERROR create-folder $e \(GetLastError \d+\)$", "^ERROR write-ready $e\\ready \(errno \d+\)$",
    "^ERROR open-dump $e\\dump_0\.tmp \(errno \d+\)$", "^ERROR rename-dump $e\\dump_1\.bin \(GetLastError \d+\)$")
$fail = $null
$p = $null

function ErrorLines { if (Test-Path $errors) { return @(Get-Content $errors) } else { return @() } }
# waits up to $Seconds for the error log to have $Count lines; sets $fail if it doesn't
function WaitLines([int]$Count, [string]$What, [int]$Seconds = 10) {
    $t0 = Get-Date
    while (-not $script:fail -and (ErrorLines).Count -lt $Count) {
        if ($p.HasExited) { $script:fail = "the game ended (exit code 0x{0:X8}) before $What" -f $p.ExitCode }
        elseif (((Get-Date) - $t0).TotalSeconds -ge $Seconds) { $script:fail = "no $What within $Seconds s" }
        else { Start-Sleep -Milliseconds 250 }
    }
}

try {
    if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
    if (Test-Path $errors) { Remove-Item $errors -Force }
    New-Item -ItemType File $dir | Out-Null                                 # a file where the folder goes
    $t0 = Get-Date
    $p = Start-Game $Save -Exe $Exe -Background -Extra "clickpass" -PidFile $pidFile
    "game started (pid $($p.Id)), $Save loaded after {0:N0} s" -f ((Get-Date) - $t0).TotalSeconds
    WaitLines 2 "create-folder and write-ready lines"
    $msg = [ClickpassErrors]::RegisterWindowMessageA("OpenSMACX.ClickpassDump")
    if (-not $fail) { Post $p $msg 0 0; WaitLines 3 "open-dump line" }
    if (-not $fail) {
        Remove-Item $dir -Force                                              # now a usable folder ...
        New-Item -ItemType Directory "$dir\dump_1.bin" | Out-Null          # ... where dump_1.bin can't go
        Post $p $msg 1 0
        WaitLines 4 "rename-dump line"
    }
    if (-not $fail) {
        Post $p $msg 2 0
        $t0 = Get-Date
        while (-not $fail -and -not (Test-Path "$dir\dump_2.bin")) {
            if ($p.HasExited) { $fail = "the game ended (exit code 0x{0:X8}) before dump_2.bin" -f $p.ExitCode }
            elseif (((Get-Date) - $t0).TotalSeconds -ge 30) { $fail = "no dump_2.bin within 30 s" }
            else { Start-Sleep -Milliseconds 250 }
        }
        Start-Sleep 1                                                        # any later line too
    }
} catch {
    $fail = "$_"
} finally {
    # only the game this check started: $p, or, if Start-Game failed after the launch, the process its
    # PID file names (still running from this folder, started at that time); none if it never launched
    $game = if ($p) { $p } else { PidFileGame $pidFile $script:GameDir }
    try { StopGame $game } catch { "stopping pid $($game.Id) failed: $_" }
    if (Test-Path -LiteralPath $pidFile) { Remove-Item -LiteralPath $pidFile -Force }
}
$lines = ErrorLines
"clickpass_errors.log ({0} line(s)):" -f $lines.Count
$lines | ForEach-Object { "  $_" }
if (-not $fail) {
    if ($lines.Count -ne $expected.Count) { $fail = "clickpass_errors.log has $($lines.Count) lines, not $($expected.Count)" }
    else { for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -cnotmatch $expected[$i]) { $fail = "line $($i + 1) is not as expected" } } }
}
if (-not $fail -and ((Test-Path "$dir\dump_0.bin") -or (Test-Path "$dir\dump_0.tmp"))) { $fail = "a dump of step 0 is there" }
if (-not $fail -and (Test-Path "$dir\dump_1.tmp")) { $fail = "dump_1.tmp was left behind" }
if (-not $fail) { "dumps: " + ((Get-ChildItem $dir | ForEach-Object { $_.Name }) -join ", ") }
if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }                    # leave the folder as it was
if (Test-Path $errors) { Remove-Item $errors -Force }
if ($fail) { "FAIL: $fail"; exit 1 }
"PASS"
exit 0
