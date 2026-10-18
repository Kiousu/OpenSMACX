param([Parameter(Mandatory = $true)][string]$GameDir, [string]$Save = "sw_steam20.SAV", [int]$Step = 0,
      [string]$Exe = "terranx_opensmacx.exe",
      [string]$Python = "C:\Programming\OpenSMACX\build\venv\Scripts\python.exe")
# Smoke check of the harness DLL's click-pass mode (src/clickpass.cpp) in one test folder: starts the
# game in the background with the trigger lines "load saves\<save>", "notests" and "clickpass", waits
# up to 60 s for clickpass\ready (game under way, random generators reseeded), checks the window is
# a captioned 1024x768 one, posts the dump message for step $Step, waits up to 30 s for
# clickpass\dump_<step>.bin, ends the game (by PID) and lists the dump's sections; the Vehs count
# must equal the save's unit count. Prints "PASS" (exit code 0) or "FAIL: <why>" (exit code 1).
# The dump stays in <GameDir>\clickpass\ for a comparison (python -m clickpass.compare ...).
# It refuses, touching nothing, a -GameDir that is, is in or holds the live game folder, and a folder
# a game already runs from; it only ever stops the game it started (by PID, see folders.ps1).
$ErrorActionPreference = "Stop"
$GameDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($GameDir)   # full paths
$script:GameDir = $GameDir.TrimEnd('\')
. "$PSScriptRoot\..\..\mp\msg.ps1"
. "$PSScriptRoot\..\folders.ps1"
$refusal = LiveRefusal $GameDir
if ($refusal) { "FAIL: $refusal"; exit 1 }
if (GameRunningIn $script:GameDir) { "FAIL: a game from $($script:GameDir) is already running (not touched)"; exit 1 }
$pidFile = Join-Path ([IO.Path]::GetTempPath()) "clickpass_smoke_dump_$PID.pid"   # Start-Game's "<pid> <start>"
if (Test-Path -LiteralPath $pidFile) { Remove-Item -LiteralPath $pidFile -Force }
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class ClickpassSmoke {
  [DllImport("user32.dll", CharSet = CharSet.Ansi)] public static extern uint RegisterWindowMessageA(string name);
  [DllImport("user32.dll")] public static extern int GetWindowLongA(IntPtr h, int index);
}
"@
$dir = "$script:GameDir\clickpass"
$dump = "$dir\dump_$Step.bin"
$fail = $null
$p = $null
try {
    if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }                # no stale ready or dumps
    $t0 = Get-Date
    $p = Start-Game $Save -Exe $Exe -Background -Extra "clickpass" -PidFile $pidFile
    "game started (pid $($p.Id)), $Save loaded after {0:N0} s" -f ((Get-Date) - $t0).TotalSeconds
    $t0 = Get-Date
    while (-not $fail -and -not (Test-Path "$dir\ready")) {
        if ($p.HasExited) { $fail = "the game ended (exit code 0x{0:X8}) before clickpass\ready" -f $p.ExitCode }
        elseif (((Get-Date) - $t0).TotalSeconds -ge 60) { $fail = "no clickpass\ready within 60 s" }
        else { Start-Sleep -Milliseconds 500 }
    }
    if (-not $fail) {
        $h = Hwnd $p
        $r = New-Object GM+RECT
        [GM]::GetClientRect($h, [ref]$r) | Out-Null
        $caption = ([ClickpassSmoke]::GetWindowLongA($h, -16) -band 0x00C00000) -eq 0x00C00000   # GWL_STYLE, WS_CAPTION
        "window: client area {0}x{1}, caption {2}" -f $r.R, $r.B, $caption
        if ($r.R -ne 1024 -or $r.B -ne 768 -or -not $caption) { $fail = "the window is not a captioned 1024x768 one" }
    }
    if (-not $fail) {
        $msg = [ClickpassSmoke]::RegisterWindowMessageA("OpenSMACX.ClickpassDump")
        Post $p $msg $Step 0
        $t0 = Get-Date
        while (-not $fail -and -not (Test-Path $dump)) {
            if ($p.HasExited) { $fail = "the game ended (exit code 0x{0:X8}) before dump_$Step.bin" -f $p.ExitCode }
            elseif (((Get-Date) - $t0).TotalSeconds -ge 30) { $fail = "no dump_$Step.bin within 30 s" }
            else { Start-Sleep -Milliseconds 250 }
        }
        if (-not $fail) { "dump_$Step.bin after {0:N1} s" -f ((Get-Date) - $t0).TotalSeconds }
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
if (-not $fail) {
    # list the sections and the globals; the Vehs count must equal the save's (savescan.py: VEH_COUNT)
    $code = @'
import struct, sys
from clickpass.dump import read_dump
d = read_dump(sys.argv[1])
print('step %d, %d sections' % (d.step, len(d.sections)))
for name, s in d.sections.items():
    print('  %-12s %-12s count %7d  elem_size %5d' % (name, s.type, s.count, s.elem_size))
g, n = d.sections['Globals'], d.sections['GlobalNames']
for i in range(g.count):
    print('    %-18s 0x%08X' % (n.data[i * 32:(i + 1) * 32].split(b'\0')[0].decode(), struct.unpack_from('<I', g.data, i * 4)[0]))
with open(sys.argv[2], 'rb') as f:
    units = struct.unpack_from('<i', f.read(), 0x9A64C8 - 0x9A6168)[0]
print('Vehs %d, units in the save %d' % (d.sections['Vehs'].count, units))
sys.exit(0 if d.sections['Vehs'].count == units else 3)
'@
    $ErrorActionPreference = "Continue"                                     # python's stderr is not an error here
    Push-Location "$PSScriptRoot\..\.."
    try { & $Python -c $code $dump "$script:GameDir\saves\$Save" } finally { Pop-Location }
    if ($LASTEXITCODE -eq 3) { $fail = "the Vehs count differs from the save's unit count" }
    elseif ($LASTEXITCODE) { $fail = "reading the dump failed (python exit code $LASTEXITCODE)" }
}
if ($fail) { "FAIL: $fail"; exit 1 }
"PASS"
exit 0
