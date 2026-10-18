param([Parameter(Mandatory = $true)][string]$Name, [string]$From = "G:\SMACX-opensmacx-test")
# Make a copy of the test folder for one parallel worker: G:\SMACX-opensmacx-test-<Name>. Each copy
# has its own exe, DLL, trigger file and log, so several input-free runs (autorun.ps1 -GameDir ...)
# can go at once. Leaves out the movies (the opening movie is off), the save backups, old exe copies
# and any trigger/log; of the saves only Council.SAV and the sweep saves are copied. ~250 MB.
$To = "$From-$Name"
$sweep = @("Council.SAV", "sw_aki2120.SAV", "sw_aki2152.SAV", "sw_aki2213.SAV", "sw_aki2247.SAV",
    "sw_artifacts.SAV", "sw_bugged.SAV", "sw_cheese.SAV", "sw_morgan.SAV", "sw_quake.SAV",
    "sw_starcraft.SAV", "sw_steam1.SAV", "sw_steam20.SAV", "sw_urfl.SAV")
robocopy $From $To /E /NFL /NDL /NJH /NJS /NP /XD "$From\movies" "$From\saves" "$From\saves_backup_20261017" `
    /XF "terranx_*.exe" "opensmacx_regress.*" "*.bak_*" | Out-Null
Copy-Item "$From\terranx_opensmacx.exe" "$To\terranx_opensmacx.exe" -Force
New-Item -ItemType Directory -Force "$To\saves" | Out-Null
foreach ($s in $sweep) { Copy-Item "$From\saves\$s" "$To\saves\$s" -Force }
"{0}: {1:N0} MB" -f $To, ((Get-ChildItem $To -Recurse -File | Measure-Object Length -Sum).Sum / 1MB)
