param([Parameter(Mandatory = $true)][string]$GameDir, [string]$Save = "sw_steam20.SAV",
      [string]$Exe = "terranx_opensmacx.exe", [string]$OutsideRoot = "G:\SMACX-clickpass-outside")
# Smoke check of the harness DLL's write guard (src/clickpass.cpp) in one test folder. Plays two games
# in the background, both with the trigger lines "load saves\<save>", "notests", "clickpass" and
# "guardprobe <outside>" (see clickpass_guard_probe()), where <outside> is
# <OutsideRoot>\<game folder name>; the first also has the line "outside <outside>", the second not.
# Each waits up to 60 s for clickpass\ready and up to 10 s for the expected lines in clickpass\guard.log,
# then ends the game (by PID). Expected:
# - always refused and logged: <parent folder>\clickpass_guard_probe.txt (the exe's CreateFileA),
#   <parent folder>\clickpass_guard_probe_w.txt (the DLL's C runtime, CreateFileW),
#   <OutsideRoot>\clickpass_guard_probe.txt and <outside>2\clickpass_guard_probe.txt;
# - with the "outside" line, <outside> is made and holds clickpass_guard_probe.txt and
#   clickpass_guard_probe_w.txt (all removed again afterwards); without it those three are refused;
# - guard.log holds exactly those lines in that order, there is no clickpass_errors.log and no probe
#   file anywhere else (one there is removed and fails the check).
# Prints "PASS" (exit code 0) or "FAIL: <why>" (exit code 1). Makes <OutsideRoot> if it is missing.
# It refuses, touching nothing, a -GameDir or -OutsideRoot that is, is in or holds the live game
# folder, and a folder a game already runs from; it only ever stops the games it started (by PID,
# see folders.ps1).
$ErrorActionPreference = "Stop"
$GameDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($GameDir)   # full paths
$OutsideRoot = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutsideRoot)
$script:GameDir = $GameDir.TrimEnd('\')
. "$PSScriptRoot\..\..\mp\msg.ps1"
. "$PSScriptRoot\..\folders.ps1"
$refusal = LiveRefusal $GameDir
if (-not $refusal) { $refusal = LiveRefusal $OutsideRoot "the outside root" }
if ($refusal) { "FAIL: $refusal"; exit 1 }
if (GameRunningIn $script:GameDir) { "FAIL: a game from $($script:GameDir) is already running (not touched)"; exit 1 }
$pidFile = Join-Path ([IO.Path]::GetTempPath()) "clickpass_smoke_guard_$PID.pid"   # Start-Game's "<pid> <start>"
$dir = "$script:GameDir\clickpass"
$log = "$dir\guard.log"
$errors = "$script:GameDir\clickpass_errors.log"
$parent = Split-Path $script:GameDir -Parent
$OutsideRoot = $OutsideRoot.TrimEnd('\')
$outside = "$OutsideRoot\" + (Split-Path $script:GameDir -Leaf)
$a = "clickpass_guard_probe.txt"
$w = "clickpass_guard_probe_w.txt"
$strays = @((Join-Path $parent $a), (Join-Path $parent $w), "$OutsideRoot\$a", "$OutsideRoot\$w", "${outside}2")
foreach ($s in $strays) {
    if (Test-Path -LiteralPath $s) { "FAIL: $s is there before the run (not made by this check; remove it first)"; exit 1 }
}
if (-not (Test-Path $OutsideRoot)) { New-Item -ItemType Directory $OutsideRoot | Out-Null }
if (Test-Path $outside) { Remove-Item $outside -Recurse -Force }            # the click pass's own folder there

$always = @("CreateFileA $(Join-Path $parent $a)", "CreateFileW $(Join-Path $parent $w)")
$after = @("CreateFileA $OutsideRoot\$a", "CreateFileA ${outside}2\$a")
$cases = @(
    @{ Name = "with outside"; Extra = @("clickpass", "guardprobe $outside", "outside $outside")
       Expected = $always + $after; Made = @("$outside\$a", "$outside\$w") },
    @{ Name = "without outside"; Extra = @("clickpass", "guardprobe $outside")
       Expected = $always + @("CreateDirectoryA $outside", "CreateFileA $outside\$a", "CreateFileW $outside\$w") + $after
       Made = @() })
$fail = $null
foreach ($case in $cases) {
    "--- $($case.Name)"
    $expected = @($case.Expected | ForEach-Object { "WRITE-BLOCKED $_" })
    $p = $null
    if (Test-Path -LiteralPath $pidFile) { Remove-Item -LiteralPath $pidFile -Force }
    try {
        if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }            # no stale ready or guard.log
        if (Test-Path $errors) { Remove-Item $errors -Force }
        $t0 = Get-Date
        $p = Start-Game $Save -Exe $Exe -Background -Extra $case.Extra -PidFile $pidFile
        "game started (pid $($p.Id)), $Save loaded after {0:N0} s" -f ((Get-Date) - $t0).TotalSeconds
        $t0 = Get-Date
        while (-not $fail -and -not (Test-Path "$dir\ready")) {
            if ($p.HasExited) { $fail = "the game ended (exit code 0x{0:X8}) before clickpass\ready" -f $p.ExitCode }
            elseif (((Get-Date) - $t0).TotalSeconds -ge 60) { $fail = "no clickpass\ready within 60 s" }
            else { Start-Sleep -Milliseconds 500 }
        }
        $t0 = Get-Date                                                       # the probes run right after ready
        while (-not $fail -and -not ((Test-Path $log) -and @(Get-Content $log).Count -ge $expected.Count)) {
            if ($p.HasExited) { $fail = "the game ended (exit code 0x{0:X8}) during the probes" -f $p.ExitCode }
            elseif (((Get-Date) - $t0).TotalSeconds -ge 10) { $fail = "guard.log did not get $($expected.Count) lines within 10 s" }
            else { Start-Sleep -Milliseconds 250 }
        }
        if (-not $fail) { Start-Sleep 1 }                                    # any later line too
    } catch {
        $fail = "$_"
    } finally {
        # only the game this check started: $p, or, if Start-Game failed after the launch, the process its
        # PID file names (still running from this folder, started at that time); none if it never launched
        $game = if ($p) { $p } else { PidFileGame $pidFile $script:GameDir }
        try { StopGame $game } catch { "stopping pid $($game.Id) failed: $_" }
        if (Test-Path -LiteralPath $pidFile) { Remove-Item -LiteralPath $pidFile -Force }
    }
    $lines = if (Test-Path $log) { @(Get-Content $log) } else { @() }
    "guard.log ({0} line(s)):" -f $lines.Count
    $lines | ForEach-Object { "  $_" }
    if (-not $fail -and ($lines.Count -ne $expected.Count -or
            @(for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -cne $expected[$i]) { $i } }).Count)) {
        $fail = "guard.log is not exactly the lines:`n  " + ($expected -join "`n  ")
    }
    if (Test-Path $errors) {
        "clickpass_errors.log:"
        Get-Content $errors | ForEach-Object { "  $_" }
        if (-not $fail) { $fail = "clickpass_errors.log is not empty" }
    }
    foreach ($m in $case.Made) {
        if (Test-Path -LiteralPath $m) { "made: $m" }
        elseif (-not $fail) { $fail = "$m was not made" }
    }
    if (-not $case.Made.Count -and (Test-Path -LiteralPath $outside) -and -not $fail) { $fail = "$outside was made" }
    foreach ($s in $strays) {
        if (Test-Path -LiteralPath $s) {
            Remove-Item -LiteralPath $s -Recurse -Force                     # made by this run's probes
            $fail = "a probe made $s (removed again)"
        }
    }
    if (Test-Path $outside) { Remove-Item $outside -Recurse -Force }
    if ($fail) { break }
}
if ($fail) { "FAIL: $fail"; exit 1 }
"PASS"
exit 0
