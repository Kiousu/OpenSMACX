# Shared by tools/clickpass/case.ps1 and its smoke checks (dot-source it): folder checks against the
# live game folder, and finding and stopping the one game a driver started, by the PID file
# Start-Game -PidFile writes (tools/mp/msg.ps1). Nothing here is ever done by process name.
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class ClickpassFolders {
  [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] public static extern uint GetLongPathNameW(string path,
      System.Text.StringBuilder longPath, uint size);
}
"@
$LiveGameDir = "G:\Sid Meier's Alpha Centauri"   # never written by a test game or a driver

# The full path, its existing part in long form (no 8.3 short names), without a trailing backslash.
function FullPath([string]$Path) {
    $head = [IO.Path]::GetFullPath($Path)
    $rest = ""
    while ($head -and -not (Test-Path -LiteralPath $head)) {
        $rest = "\" + (Split-Path $head -Leaf) + $rest
        $head = Split-Path $head -Parent
    }
    if ($head) {
        $long = New-Object Text.StringBuilder 1024
        if ([ClickpassFolders]::GetLongPathNameW($head, $long, 1024)) { $head = $long.ToString() }
    }
    return $head.TrimEnd('\') + $rest
}

# Whether $Path is $Folder or inside it (both from FullPath; any case, whole folder names only).
function Within([string]$Path, [string]$Folder) {
    return $Path.Equals($Folder, [StringComparison]::OrdinalIgnoreCase) -or
        $Path.StartsWith($Folder + "\", [StringComparison]::OrdinalIgnoreCase)
}

# Why $Dir may not be used ("" when it may): it is, is in or holds the live game folder, or it can't be
# checked. $What names it in the message.
function LiveRefusal([string]$Dir, [string]$What = "the game folder", [string]$Live = $LiveGameDir) {
    try {
        $full = FullPath $Dir
        $liveFull = FullPath $Live
    } catch {
        return "refused: $What $Dir can't be checked against the live game folder $Live ($_)"
    }
    if ((Within $full $liveFull) -or (Within $liveFull $full)) {
        return "refused: $What $Dir is, is in or holds the live game folder $Live"
    }
    return ""
}

# The game a driver started, from the file Start-Game -PidFile wrote ("<pid> <start time as a UTC
# FILETIME>"): that process while it still runs from inside $Dir and started at that time (so a
# reused PID is never taken), else $null.
function PidFileGame([string]$PidFile, [string]$Dir) {
    if (-not $PidFile -or -not (Test-Path -LiteralPath $PidFile)) { return $null }
    try {
        $parts = ([IO.File]::ReadAllText($PidFile)).Split([char[]]" `r`n", [StringSplitOptions]::RemoveEmptyEntries)
        $proc = Get-Process -Id ([int]$parts[0]) -ErrorAction Stop
        if ($proc.StartTime.ToFileTimeUtc() -ne [int64]$parts[1]) { return $null }
        if (-not $proc.Path -or -not (Within (FullPath $proc.Path) (FullPath $Dir))) { return $null }
        return $proc
    } catch {
        return $null
    }
}

# Ends a game this driver started (a process object it holds) and waits up to 15 s for it.
function StopGame($Game) {
    if ($Game -and -not $Game.HasExited) {
        Stop-Process -Id $Game.Id -Force
        $Game.WaitForExit(15000) | Out-Null
    }
}
