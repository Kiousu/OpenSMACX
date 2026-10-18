param([string[]]$Saves, [string]$Tests = "", [string]$Exe = "terranx_opensmacx.exe", [int]$TimeoutSec = 1800,
      [int]$StallSec = 120,
      [switch]$Minimized)
# Input-free regression sweep: no mouse or keyboard input at all. For each save the trigger file
# gets "load saves\<save>", "exit" and the test names; the harness DLL (built with
# /DOPENSMACX_REGRESS) loads the save in place of the main menu, runs the tests once the game is
# under way and ends the game. With [OpenSMACX] Windowed=1 in the test folder's Alpha Centauri.ini
# the game runs in an ordinary window and can stay in the background. Logs go to build\sweep\.
$G = "G:\SMACX-opensmacx-test"
$Out = "C:\Programming\OpenSMACX\build\sweep"
New-Item -ItemType Directory -Force $Out | Out-Null
$Log = "$Out\sweep.log"
function L($m) { $line = "[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $m; $line | Add-Content $Log; $line }
foreach ($save in $Saves) {
    if (Get-Process ($Exe -replace '\.exe$', '') -ErrorAction SilentlyContinue) { L "a game is already running, stopping"; break }
    Remove-Item "$G\opensmacx_regress.log" -ErrorAction SilentlyContinue
    Set-Content "$G\opensmacx_regress.txt" @("load saves\$save", "exit", $Tests) -Encoding ASCII
    $t0 = Get-Date
    $style = if ($Minimized) { "Minimized" } else { "Normal" } # minimized: never takes the focus
    $p = Start-Process -FilePath "$G\$Exe" -WorkingDirectory $G -PassThru -WindowStyle $style
    # give up: no exit within $TimeoutSec, or the log not growing for $StallSec (the DLL logs a heartbeat
    # every 30 s while a test runs and ends a test after 10 min; this covers a stuck load or start-up)
    $last = Get-Date; $size = -1
    while (-not $p.WaitForExit(1000)) {
        $now = (Get-Item "$G\opensmacx_regress.log" -ErrorAction SilentlyContinue).Length
        if ($now -ne $size) { $size = $now; $last = Get-Date }
        $why = if (((Get-Date) - $t0).TotalSeconds -gt $TimeoutSec) { "no exit within $TimeoutSec s" }
               elseif (((Get-Date) - $last).TotalSeconds -gt $StallSec) { "log stalled for $StallSec s" } else { $null }
        if ($why) { $p.Kill(); L "$save : $why, killed"; break }
    }
    if ($p.HasExited -and $p.ExitCode) { L ("$save : game exit code 0x{0:X8}" -f $p.ExitCode) }
    $name = $save -replace '\.SAV$', ''
    if (Test-Path "$G\opensmacx_regress.log") {
        Copy-Item "$G\opensmacx_regress.log" "$Out\$name.log" -Force
        $hdr = (Select-String -Path "$G\opensmacx_regress.log" -Pattern "^OpenSMACX|^auto run" | Select-Object -First 1).Line
        $tot = (Select-String -Path "$G\opensmacx_regress.log" -Pattern "^TOTAL" | Select-Object -First 1).Line
        L ("$save : $hdr | $tot ({0:N0} s)" -f ((Get-Date) - $t0).TotalSeconds)
    } else {
        L "$save : no log"
    }
}
Remove-Item "$G\opensmacx_regress.txt" -ErrorAction SilentlyContinue
L "autorun done"
