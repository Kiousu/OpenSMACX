param([string]$Save = "Council.SAV", [string]$Name = "_msg_save", [string]$Tag = "autosave",
      [string]$GameDir = "G:\SMACX-opensmacx-test")
# UI save and load check without the real mouse or keyboard (messages, see msg.ps1): the harness
# DLL loads $Save, then Ctrl+S saves it from the game under $Name, the file must appear in the test
# folder's saves (and nothing new in the live game folder), and Ctrl+L loads it back. Needs the
# harness DLL and [OpenSMACX] Windowed=1. Log and screenshots in build\boot\.
$script:GameDir = $GameDir.TrimEnd('\')
. "$PSScriptRoot\msg.ps1"
$G = $script:GameDir
$Live = "G:\Sid Meier's Alpha Centauri\saves"
$Out = if ($G -eq "G:\SMACX-opensmacx-test") { "C:\Programming\OpenSMACX\build\boot" }
       else { "C:\Programming\OpenSMACX\build\boot_" + (Split-Path $G -Leaf) }
New-Item -ItemType Directory -Force $Out | Out-Null
$Log = "$Out\$Tag.log"
function L($m) { $line = "[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $m; $line | Add-Content $Log; $line }
Remove-Item $Log, "$G\saves\$Name.SAV" -ErrorAction SilentlyContinue
$t0 = Get-Date
$p = Start-Game $Save -Background
L "loaded $Save, turn $(Turn $p)"
Keys $p "^s"; Start-Sleep 1                                          # Save Game
Click $p 520 494; Start-Sleep -Milliseconds 200                      # filename field
Keys $p "{END}{BACKSPACE 60}$Name"; Start-Sleep -Milliseconds 300
Shot $p "$Out\$Tag`_typed.png"
Click $p 445 521; Start-Sleep 3                                      # OK
$p.Refresh(); if ($p.HasExited) { L "died while saving"; exit 1 }
$f = Get-Item "$G\saves\$Name.SAV" -ErrorAction SilentlyContinue
if ($f) { L "saved $($f.FullName), $($f.Length) bytes" } else { L "no $Name.SAV in the test saves folder" }
$leak = Get-ChildItem $Live -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $t0 }
if ($leak) { L "WARNING: new file(s) in the live saves folder: $($leak.Name -join ', ')" } else { L "live saves folder untouched" }
Keys $p "^l"; Start-Sleep 1                                          # Load Game
Click $p 520 494; Start-Sleep -Milliseconds 200
Keys $p "{END}{BACKSPACE 60}$Name.SAV"; Start-Sleep -Milliseconds 300
Click $p 445 521; Start-Sleep 3
Click $p 356 478; Start-Sleep 2                                      # summary OK
Shot $p "$Out\$Tag`_reloaded.png"
$p.Refresh()
L ("after reload alive={0}, turn {1} ({2:N0} s)" -f (-not $p.HasExited), (Turn $p), ((Get-Date) - $t0).TotalSeconds)
if (-not $p.HasExited) { $p.Kill() }
Remove-Item "$G\opensmacx_regress.txt" -ErrorAction SilentlyContinue
