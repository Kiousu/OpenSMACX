param([Parameter(Mandatory = $true)][string]$GameDir, [string]$Save = "Council.SAV", [string]$OutDir = "",
      [int]$SampleMs = 100, [string]$Command = "")
# Smoke check that a click-pass game never takes the foreground and never moves the real cursor (Kiousu:
# no focus stealing, no real mouse). Plays a short case with the case driver (tools/clickpass/case.ps1:
# the load, step 0, F1, Escape, a click on the map, F2, Escape) in -GameDir and meanwhile samples, every
# -SampleMs ms from before the launch until the driver has ended, which process owns the foreground window
# (GetForegroundWindow) and where the cursor is (GetCursorPos). Writes the samples to <OutDir>\focus.csv
# (default: a folder in %TEMP%) and prints a summary: the processes seen running from -GameDir, the
# foreground's owners, the cursor positions and the focus guard's lines from the game's
# opensmacx_regress.log. Each sample also lists the processes running from -GameDir, and a foreground
# owner's exe is looked up when it first shows, while it still runs (a game may be gone by the end), so
# a sample takes -SampleMs ms plus that time. Fails when a sample has a window of the game or of any
# process seen running from -GameDir in the foreground, when a foreground window's owner can't be
# identified, when the cursor ever stands elsewhere than before the launch (moving the mouse by hand
# during the check also fails it: run it again), or when the case is not ok. Prints "PASS" (exit code 0)
# or "FAIL: <why>" (exit code 1). It refuses, touching nothing, a -GameDir
# that is, is in or holds the live game folder or that a game already runs from; it only ever stops the
# game it started (case.ps1 does, by PID).
# -Command: a PowerShell command to run instead of the case, such as another script that starts a test
# game in -GameDir (tools/mp/autorun.ps1, autoboot.ps1); its output goes to <OutDir>\driver.log and only
# the foreground and the cursor are judged (its own results are its own).
$ErrorActionPreference = "Stop"
$GameDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($GameDir).TrimEnd('\')
$script:GameDir = $GameDir
. "$PSScriptRoot\..\..\mp\msg.ps1"
. "$PSScriptRoot\..\folders.ps1"
$refusal = LiveRefusal $GameDir
if ($refusal) { "FAIL: $refusal"; exit 1 }
if (GameRunningIn $GameDir) { "FAIL: a game from $GameDir is already running (not touched)"; exit 1 }
if (-not $OutDir) { $OutDir = Join-Path ([IO.Path]::GetTempPath()) ("clickpass_smoke_focus_{0}" -f $PID) }
$OutDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutDir)
$refusal = LiveRefusal $OutDir "the out folder"
if ($refusal) { "FAIL: $refusal"; exit 1 }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type @"
using System; using System.Collections.Generic; using System.Runtime.InteropServices; using System.Text;
public static class SmokeFocus {
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
  [DllImport("kernel32.dll")] static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
  [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] static extern bool QueryFullProcessImageNameW(IntPtr h,
      uint flags, StringBuilder name, ref uint size);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
  [DllImport("psapi.dll")] static extern bool EnumProcesses([Out] uint[] ids, uint size, out uint needed);
  // the exe of a running process, or null (gone, or it can't be queried)
  public static string ExePath(uint pid) {
    IntPtr h = OpenProcess(0x1000, false, pid);                     // PROCESS_QUERY_LIMITED_INFORMATION
    if (h == IntPtr.Zero) { return null; }
    try {
      StringBuilder name = new StringBuilder(32768);
      uint size = (uint)name.Capacity;
      return QueryFullProcessImageNameW(h, 0, name, ref size) ? name.ToString() : null;
    } finally { CloseHandle(h); }
  }
  // the running processes whose exe is inside folder (a full path), by exe path
  public static Dictionary<uint, string> ProcessesIn(string folder) {
    Dictionary<uint, string> found = new Dictionary<uint, string>();
    uint[] ids = new uint[4096];
    uint needed;
    if (!EnumProcesses(ids, (uint)(ids.Length * 4), out needed)) { return found; }
    string prefix = folder.TrimEnd('\\') + "\\";
    for (int i = 0; i < needed / 4; i++) {
      string exe = ExePath(ids[i]);
      if (exe != null && exe.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)) { found[ids[i]] = exe; }
    }
    return found;
  }
}
"@

$case = Join-Path $OutDir "smoke_focus.case"
[IO.File]::WriteAllText($case, ("save $Save", "keys {F1}", "keys {ESCAPE}", "click 512 400", "keys {F2}",
    "keys {ESCAPE}", "") -join "`r`n")
$run = Join-Path $OutDir "run"
$driverLog = Join-Path $OutDir "driver.log"
$start = New-Object SmokeFocus+POINT
[SmokeFocus]::GetCursorPos([ref]$start) | Out-Null
$driverArgs = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$PSScriptRoot\..\case.ps1`"", "-CaseFile", "`"$case`"",
    "-GameDir", "`"$GameDir`"", "-OutDir", "`"$run`"")
if ($Command) {
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($Command))   # (no quoting to get wrong)
    $driverArgs = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-EncodedCommand", $encoded)
}
$t0 = Get-Date
$driver = Start-Process powershell.exe -ArgumentList $driverArgs -NoNewWindow -PassThru -RedirectStandardOutput $driverLog
$null = $driver.Handle
$samples = New-Object System.Collections.ArrayList
$gamePid = 0
$gameRoot = FullPath $GameDir
$exes = @{}                 # foreground owner pid -> its exe, looked up while it runs ("?" when it can't be)
$fromGameDir = @{}          # every process seen running from -GameDir during the sampling, pid -> exe
while ($true) {
    $ended = $driver.HasExited
    if (-not $gamePid -and (Test-Path -LiteralPath "$run\game.pid")) {
        try { $gamePid = [int](([IO.File]::ReadAllText("$run\game.pid")).Split(" ")[0]) } catch { }
    }
    $seen = [SmokeFocus]::ProcessesIn($gameRoot)
    foreach ($id in $seen.Keys) { $fromGameDir[[uint32]$id] = $seen[$id] }
    $fg = [SmokeFocus]::GetForegroundWindow()
    $fgPid = [uint32]0
    [SmokeFocus]::GetWindowThreadProcessId($fg, [ref]$fgPid) | Out-Null
    if ($fg -ne [IntPtr]::Zero -and -not $exes.ContainsKey($fgPid)) {   # (at once: a game may be gone by the end)
        $exe = [SmokeFocus]::ExePath($fgPid)
        $exes[$fgPid] = $(if ($exe) { $exe } else { "?" })
    }
    $pt = New-Object SmokeFocus+POINT
    [SmokeFocus]::GetCursorPos([ref]$pt) | Out-Null
    [void]$samples.Add([pscustomobject]@{ ms = [int]((Get-Date) - $t0).TotalMilliseconds; fg_window = [int64]$fg
        fg_pid = $fgPid; fg_exe = $(if ($fg -eq [IntPtr]::Zero) { "" } else { $exes[$fgPid] }); game_pid = $gamePid
        x = $pt.X; y = $pt.Y })
    if ($ended) { break }
    Start-Sleep -Milliseconds $SampleMs
}
$samples | Export-Csv -NoTypeInformation -Path (Join-Path $OutDir "focus.csv")

$fail = $null
$result = $null
try { $result = Get-Content -Raw "$run\result.json" | ConvertFrom-Json } catch { }
if ($Command) { "command: $Command"; Get-Content -LiteralPath $driverLog | ForEach-Object { "  | $_" } }
else { "driver: " + $(if ($result) { "$($result.case): $($result.outcome) after $($result.seconds) s" } else { "no result.json" }) }
"{0} samples over {1:N1} s, game pid {2}" -f $samples.Count, ($samples[-1].ms / 1000.0), $gamePid
"processes seen running from {0} (pid, exe):" -f $GameDir
foreach ($id in $fromGameDir.Keys) { "  {0} {1}" -f $id, $fromGameDir[$id] }
"foreground owners (pid, exe, samples):"
foreach ($g in ($samples | Group-Object fg_pid, fg_exe)) { "  {0} {1}" -f $g.Name, $g.Count }
$inGame = @($samples | Where-Object { $_.fg_window -ne 0 -and (($gamePid -and $_.fg_pid -eq $gamePid) -or
    $fromGameDir.ContainsKey([uint32]$_.fg_pid) -or
    ($_.fg_exe -ne "?" -and (Within (FullPath $_.fg_exe) $gameRoot))) })
$unknown = @($samples | Where-Object { $_.fg_window -ne 0 -and $_.fg_exe -eq "?" })
if ($inGame.Count) {
    $fail = "a window of a process from {0} was in the foreground in {1} samples (first at {2} ms, pid {3})" -f `
        $GameDir, $inGame.Count, $inGame[0].ms, $inGame[0].fg_pid
} elseif ($unknown.Count) {
    $fail = "the foreground window's owner could not be identified in {0} samples (first at {1} ms, pid {2})" -f `
        $unknown.Count, $unknown[0].ms, $unknown[0].fg_pid
}
"cursor positions (x, y, samples): before the launch {0},{1}" -f $start.X, $start.Y
foreach ($g in ($samples | Group-Object x, y)) { "  {0} {1}" -f $g.Name, $g.Count }
$moved = @($samples | Where-Object { $_.x -ne $start.X -or $_.y -ne $start.Y })
if ($moved.Count -and -not $fail) {
    $fail = "the cursor stood elsewhere than before the launch in {0} samples (first at {1} ms: {2},{3})" -f `
        $moved.Count, $moved[0].ms, $moved[0].x, $moved[0].y
}
if (-not $Command -and (-not $result -or $result.outcome -ne "ok")) {
    if (-not $fail) { $fail = "the case was not ok (see $driverLog)" }
}
if (-not $Command) {
    "the focus guard (opensmacx_regress.log):"
    $log = "$run\opensmacx_regress.log"
    if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log | ForEach-Object { "  $_" } } else { "  (no log)" }
}
"samples: $(Join-Path $OutDir 'focus.csv')"
if ($fail) { "FAIL: $fail"; exit 1 }
"PASS"
exit 0
