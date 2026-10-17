param([string[]]$Saves, [string]$Exe = "terranx_sweep.exe", [string]$Tests = " ", [int]$WaitSec = 90, [switch]$KeepOpen)
# Regression sweep, fast form: launch the game once, load every save from inside the game (Ctrl+L),
# trigger the harness, poll the log twice a second. Logs under build\sweep\<save>.log, progress in
# build\sweep\sweep.log. A screenshot of the first Load dialog is kept as sweep_loaddlg.png.
$S = $PSScriptRoot
$G = "G:\SMACX-opensmacx-test"
$Out = "C:\Programming\OpenSMACX\build\sweep"
$UI = "$S\ui.ps1"
New-Item -ItemType Directory -Force $Out | Out-Null
$Log = "$Out\sweep.log"
function L($m) { $line = "[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $m; $line | Add-Content $Log; $line }
function Click($id, $x, $y) { & $UI -Action click -X $x -Y $y -ProcId $id | Out-Null }
function Key($id, $k) { & $UI -Action key -Keys $k -ProcId $id | Out-Null }
function Alive($id) { $pp = Get-Process -Id $id -ErrorAction SilentlyContinue; return ($pp -and -not $pp.HasExited) }
Add-Type -Name WM8 -Namespace Q8 -MemberDefinition '[DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f); [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);'

# launch once, from the main menu load the first save the long way (its dialog is known)
Get-Process terranx_opensmacx, terranx_sweep -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item "$G\opensmacx_regress.txt", "$G\opensmacx_regress.log" -ErrorAction SilentlyContinue
Start-Sleep 1
[Q8.WM8]::SetCursorPos(500, 400) | Out-Null
$p = Start-Process -FilePath "$G\$Exe" -WorkingDirectory $G -PassThru
$id = $p.Id
for ($i = 0; $i -lt 30; $i++) { Start-Sleep 1; $p.Refresh(); if ($p.MainWindowHandle -ne 0) { break } }
Start-Sleep 6
[Q8.WM8]::SetWindowPos($p.MainWindowHandle, [IntPtr]::Zero, 0, 0, 0, 0, 0x0005) | Out-Null
& $UI -Action focus -ProcId $id | Out-Null
$first = $true
foreach ($save in $Saves) {
    if (-not (Alive $id)) { L "$save : game not running, stopping"; break }
    Remove-Item "$G\opensmacx_regress.txt", "$G\opensmacx_regress.log" -ErrorAction SilentlyContinue
    if ($first) {
        Click $id 835 512; Start-Sleep 1                  # main menu: Load Game
    } else {
        Key $id "^l"; Start-Sleep 1                       # in game: Ctrl+L
        if ($Saves.IndexOf($save) -eq 1) { & $UI -Action shot -Out "$Out\sweep_loaddlg.png" -ProcId $id | Out-Null }
    }
    Click $id 520 494; Start-Sleep -Milliseconds 200      # filename field
    Key $id "{END}{BACKSPACE 40}$save"; Start-Sleep -Milliseconds 200
    Click $id 445 521                                     # OK
    Start-Sleep 2                                         # load + summary popup
    Click $id 356 478; Start-Sleep 1                      # summary OK
    if ($first) { Start-Sleep 1; $first = $false }
    if (-not (Alive $id)) { L "$save : game died while loading"; break }
    & $UI -Action shot -Out "$Out\$($save -replace '\.SAV$','').png" -ProcId $id | Out-Null
    Set-Content "$G\opensmacx_regress.txt" $Tests -Encoding ASCII
    & $UI -Action focus -ProcId $id | Out-Null
    $done = $false
    for ($i = 0; $i -lt ($WaitSec * 2); $i++) {
        if ($i % 2 -eq 0) { Click $id 260 430 }           # a map click runs veh_at()
        Start-Sleep -Milliseconds 500
        if (-not (Alive $id)) { if (Test-Path "$G\opensmacx_regress.log") { Copy-Item "$G\opensmacx_regress.log" "$Out\$($save -replace '\.SAV$','')_partial.log" -Force }; L "$save : game died during the run"; break }
        if ((Test-Path "$G\opensmacx_regress.log") -and (Select-String -Path "$G\opensmacx_regress.log" -Pattern "^TOTAL" -Quiet)) { $done = $true; break }
    }
    if ($done) {
        Copy-Item "$G\opensmacx_regress.log" "$Out\$($save -replace '\.SAV$','').log" -Force
        $tot = (Select-String -Path "$G\opensmacx_regress.log" -Pattern "^TOTAL" | Select-Object -First 1).Line
        $hdr = (Select-String -Path "$G\opensmacx_regress.log" -Pattern "^OpenSMACX" | Select-Object -First 1).Line
        L "$save : $hdr | $tot"
    } elseif (Alive $id) {
        L "$save : no result after $WaitSec s"
    }
}
if (-not $KeepOpen) { Get-Process -Id $id -ErrorAction SilentlyContinue | Stop-Process -Force }
L "sweep done"
