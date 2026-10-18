# Message driver for the game: clicks, drags and keys are posted to the game window (PostMessage), so
# the real mouse and keyboard are never used and the game can stay in the background. Dot-source it:
#   . "$PSScriptRoot\msg.ps1"
#   $p = Start-Game "Council.SAV"            # harness DLL loads the save itself (trigger "notests")
#   Click $p 899 584; Keys $p "^s"; Keys $p "{END}{BACKSPACE 40}name{ENTER}"; Drag $p 500 300 556 300
# Coordinates are the game's own (client area, 1024x768). Ctrl/Shift/Alt and the mouse buttons
# reach the game through the harness DLL's input hooks (regress.cpp), so use the harness DLL.
# Key strings follow SendKeys: ^ Ctrl, + Shift, % Alt for the next key; {NAME} or {NAME n} for
# ENTER ESC TAB BACKSPACE DELETE INSERT HOME END PGUP PGDN UP DOWN LEFT RIGHT SPACE F1..F12
# NUMPAD0..NUMPAD9; any other character is typed. Key messages carry lParam bit 25 so the harness
# DLL's TranslateMessage hook leaves them alone; the character messages are posted here.
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class GM {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern short VkKeyScan(char c);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
  [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(uint a, bool i, int pid);
  [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr p, IntPtr a, byte[] b, int n, out int r);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint f);
}
"@
# The launch of every test game (Start-TestProcess below; Start-Game, tools/mp/autorun.ps1):
# CreateProcessW with the show command SW_SHOWMINNOACTIVE (7, minimized) or SW_SHOWNOACTIVATE (4,
# shown), so the game never becomes the active window (Start-Process -WindowStyle Minimized or Normal
# use SW_SHOWMINIMIZED or SW_SHOWNORMAL, which made every game the foreground window, from whatever
# Kiousu was using). The process is created suspended and its Process object (with its handle, so that
# ExitCode can be read once it has ended) taken before it runs; a launch that fails after the process
# exists ends that process (by handle).
Add-Type @"
using System; using System.ComponentModel; using System.Diagnostics; using System.Runtime.InteropServices;
public static class GMLaunch {
  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)] struct STARTUPINFO {
    public int cb; public string lpReserved, lpDesktop, lpTitle;
    public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    public short wShowWindow, cbReserved2; public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError; }
  [StructLayout(LayoutKind.Sequential)] struct PROCESS_INFORMATION {
    public IntPtr hProcess, hThread; public int dwProcessId, dwThreadId; }
  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern bool CreateProcessW(
    string app, System.Text.StringBuilder cmd, IntPtr pa, IntPtr ta, bool inherit, uint flags, IntPtr env,
    string dir, ref STARTUPINFO si, out PROCESS_INFORMATION pi);
  [DllImport("kernel32.dll", SetLastError = true)] static extern uint ResumeThread(IntPtr h);
  [DllImport("kernel32.dll")] static extern bool TerminateProcess(IntPtr h, uint code);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
  public static Process StartNoActivate(string exe, string dir, short show) {
    STARTUPINFO si = new STARTUPINFO();
    si.cb = Marshal.SizeOf(typeof(STARTUPINFO));
    si.dwFlags = 1;                                                    // STARTF_USESHOWWINDOW
    if (show != 7 && show != 4) { throw new ArgumentException("show: 7 (SW_SHOWMINNOACTIVE) or 4 (SW_SHOWNOACTIVATE)"); }
    si.wShowWindow = show;
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(exe, new System.Text.StringBuilder("\"" + exe + "\""), IntPtr.Zero, IntPtr.Zero, false,
        4 /* CREATE_SUSPENDED */, IntPtr.Zero, dir, ref si, out pi)) {
      throw new Win32Exception();
    }
    try {
      Process p;
      try {
        p = Process.GetProcessById(pi.dwProcessId);
        IntPtr keep = p.Handle;                                        // (kept for ExitCode)
        if (ResumeThread(pi.hThread) == 0xFFFFFFFF) { throw new Win32Exception(); }
      } catch {
        TerminateProcess(pi.hProcess, 1);
        throw;
      }
      return p;
    } finally {
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
    }
  }
}
"@
if (-not $script:GameDir) { $script:GameDir = "G:\SMACX-opensmacx-test" }   # set before dot-sourcing for a copy
# Starts a test game's exe in $Dir without ever activating it (GMLaunch above): minimized, or shown where
# it opens. Returns its Process.
function Start-TestProcess([string]$Exe, [string]$Dir, [switch]$Minimized) {
    return [GMLaunch]::StartNoActivate($Exe, $Dir, $(if ($Minimized) { 7 } else { 4 }))
}
function GameRunningIn([string]$dir) {      # a game started from this folder (copies run side by side)
    return [bool](Get-Process terranx* -ErrorAction SilentlyContinue | Where-Object {
        $_.Path -and $_.Path.StartsWith($dir.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) })
}
$script:VkNames = @{ ENTER = 0x0D; ESC = 0x1B; ESCAPE = 0x1B; TAB = 0x09; BACKSPACE = 0x08; BS = 0x08;
    DELETE = 0x2E; DEL = 0x2E; INSERT = 0x2D; HOME = 0x24; END = 0x23; PGUP = 0x21; PGDN = 0x22;
    UP = 0x26; DOWN = 0x28; LEFT = 0x25; RIGHT = 0x27; SPACE = 0x20 }
1..12 | ForEach-Object { $script:VkNames["F$_"] = 0x6F + $_ }
0..9 | ForEach-Object { $script:VkNames["NUMPAD$_"] = 0x60 + $_ }

function Hwnd($p) { $p.Refresh(); return $p.MainWindowHandle }
function Post($p, $m, $w, $l) { [GM]::PostMessage((Hwnd $p), [uint32]$m, [IntPtr]$w, [IntPtr]$l) | Out-Null }
function MouseL($x, $y) { return [int](($y -shl 16) -bor ($x -band 0xFFFF)) }

function Click($p, $x, $y, [switch]$Right) {
    $lp = MouseL $x $y
    Post $p 0x200 0 $lp                                        # WM_MOUSEMOVE
    if ($Right) { Post $p 0x204 2 $lp; Start-Sleep -Milliseconds 60; Post $p 0x205 0 $lp }
    else { Post $p 0x201 1 $lp; Start-Sleep -Milliseconds 60; Post $p 0x202 0 $lp }
    Start-Sleep -Milliseconds 60
}

function Drag($p, $x1, $y1, $x2, $y2, [int]$Steps = 8) {
    Post $p 0x200 0 (MouseL $x1 $y1)
    Post $p 0x201 1 (MouseL $x1 $y1); Start-Sleep -Milliseconds 120
    for ($i = 1; $i -le $Steps; $i++) {
        $x = [int]($x1 + ($x2 - $x1) * $i / $Steps); $y = [int]($y1 + ($y2 - $y1) * $i / $Steps)
        Post $p 0x200 1 (MouseL $x $y); Start-Sleep -Milliseconds 40      # WM_MOUSEMOVE, MK_LBUTTON
    }
    Start-Sleep -Milliseconds 120
    Post $p 0x202 0 (MouseL $x2 $y2); Start-Sleep -Milliseconds 100
}

function KeyMsg($p, $vk, $up, $alt) {
    $scan = [GM]::MapVirtualKey([uint32]$vk, 0)
    $lp = 1 -bor ($scan -shl 16) -bor (1 -shl 25)                       # bit 25: posted by this driver
    if ($alt) { $lp = $lp -bor (1 -shl 29) }
    if ($up) { $lp = $lp -bor 0xC0000000 }
    $msg = if ($alt -or $vk -eq 0x12) { if ($up) { 0x105 } else { 0x104 } } else { if ($up) { 0x101 } else { 0x100 } }
    Post $p $msg $vk ([int64]$lp -band 0xFFFFFFFF)
}

function Stroke($p, $vk, $char, $ctrl, $shift, $alt) {
    if ($ctrl) { KeyMsg $p 0x11 $false $false }
    if ($shift) { KeyMsg $p 0x10 $false $alt }
    if ($alt) { KeyMsg $p 0x12 $false $true }
    KeyMsg $p $vk $false $alt
    if ($char) {
        $scan = [GM]::MapVirtualKey([uint32]$vk, 0)
        $c = [int]$char
        if ($ctrl -and $c -ge 0x61 -and $c -le 0x7A) { $c -= 0x60 }      # Ctrl+letter: control code
        elseif ($ctrl -and $c -ge 0x41 -and $c -le 0x5A) { $c -= 0x40 }
        Post $p ($(if ($alt) { 0x106 } else { 0x102 })) $c (1 -bor ($scan -shl 16))  # WM_(SYS)CHAR
    }
    KeyMsg $p $vk $true $alt
    if ($alt) { KeyMsg $p 0x12 $true $true }
    if ($shift) { KeyMsg $p 0x10 $true $alt }
    if ($ctrl) { KeyMsg $p 0x11 $true $false }
    Start-Sleep -Milliseconds 40
}

function Keys($p, [string]$spec) {
    $i = 0; $ctrl = $false; $shift = $false; $alt = $false
    while ($i -lt $spec.Length) {
        $c = $spec[$i]
        if ($c -eq '^') { $ctrl = $true; $i++; continue }
        if ($c -eq '+') { $shift = $true; $i++; continue }
        if ($c -eq '%') { $alt = $true; $i++; continue }
        if ($c -eq '{') {
            $end = $spec.IndexOf('}', $i + 1)
            $parts = $spec.Substring($i + 1, $end - $i - 1).Split(' ')
            $name = $parts[0].ToUpper(); $n = if ($parts.Count -gt 1) { [int]$parts[1] } else { 1 }
            $i = $end + 1
            if ($script:VkNames.ContainsKey($name)) {
                $vk = $script:VkNames[$name]
                $ch = if ($vk -eq 0x0D) { [char]13 } elseif ($vk -eq 0x1B) { [char]27 } elseif ($vk -eq 0x08) { [char]8 } elseif ($vk -eq 0x09) { [char]9 } elseif ($vk -eq 0x20) { [char]32 } else { $null }
                for ($k = 0; $k -lt $n; $k++) { Stroke $p $vk $ch $ctrl $shift $alt }
            } else {
                for ($k = 0; $k -lt $n; $k++) { foreach ($ch in $name.ToCharArray()) { TypeChar $p $ch $ctrl $shift $alt } }
            }
        } else {
            TypeChar $p $c $ctrl $shift $alt
            $i++
        }
        $ctrl = $false; $shift = $false; $alt = $false
    }
}

function TypeChar($p, [char]$ch, $ctrl, $shift, $alt) {
    $scan = [GM]::VkKeyScan($ch)
    $vk = $scan -band 0xFF
    $needShift = ($scan -band 0x100) -ne 0
    Stroke $p $vk $ch $ctrl ($shift -or $needShift) $alt
}

function ReadInt($p, [int64]$addr, [int]$size = 4) {
    $hp = [GM]::OpenProcess(0x10, $false, $p.Id)                        # PROCESS_VM_READ
    $b = New-Object byte[] 4; $r = 0
    $ok = [GM]::ReadProcessMemory($hp, [IntPtr]$addr, $b, $size, [ref]$r)
    [GM]::CloseHandle($hp) | Out-Null
    if (-not $ok) { return $null }
    if ($size -eq 2) { return [BitConverter]::ToInt16($b, 0) }
    if ($size -eq 1) { return [int]$b[0] }
    return [BitConverter]::ToInt32($b, 0)
}
function Turn($p) { return ReadInt $p 0x9A64D4 }                         # TurnCurrentNum
function WinVisible($p, [int64]$win) {                                  # the exe's Win::is_visible (0x5F7E90)
    while ($win) {
        if (((ReadInt $p ($win + 0x9C) 1) -band 1) -eq 0) { return $false }
        $win = ReadInt $p ($win + 0xC4)
    }
    return $true
}
function BaseScreenOpen($p) { return WinVisible $p 0x6A7628 }           # BaseWin
# The game window that has the keyboard focus, as the exe's Win_get_key_window (0x5F6A50) finds it;
# 0x939444 is the main map window (Thinker: MainWinHandle).
function KeyWindow($p) {
    $w = ReadInt $p 0x9B7AE0
    if ($w) { $x = ReadInt $p 0x9B8D7C; if ($x) { $w = $x } }
    else { $w = ReadInt $p 0x9B7AC4; if (-not $w) { $w = ReadInt $p 0x9B7AEC } }
    if (-not $w) { return 0 }
    $w = ReadInt $p ($w + 0xA8)
    if (-not $w -or ((ReadInt $p ($w + 0x9C) 1) -band 1) -eq 0) { return 0 }
    $parent = ReadInt $p ($w + 0xC4)
    if ($parent -and -not (WinVisible $p $parent)) { return 0 }
    return $w
}
function PopupOpen($p) { $w = KeyWindow $p; return ($w -ne 0 -and $w -ne 0x939444) }  # a dialog has the focus

# Launch the game and let the harness DLL load $Save in place of the main menu (no tests run). The game
# never becomes the active window (Start-TestProcess; the harness DLL's focus guard, src/focusguard.cpp,
# keeps it from activating itself). -Background: start minimized, then once loaded show the window again
# without activating it, behind the other windows; without it the window is shown from the start, not
# active. A minimized game ignores clicks and spins a CPU core, so UI scripts must not leave it
# minimized. -Extra: more trigger lines (e.g. "clickpass").
# A game that ends during the load is returned as it is (check $p.HasExited). -PidFile: a file that
# gets "<pid> <start time as a UTC FILETIME>" right after the launch (tools/clickpass/case.ps1).
function Start-Game([string]$Save, [string]$Exe = "terranx_opensmacx.exe", [switch]$Background,
    [string[]]$Extra = @(), [string]$PidFile = "") {
    if (GameRunningIn $script:GameDir) { throw "a game from $($script:GameDir) is already running" }
    Set-Content "$script:GameDir\opensmacx_regress.txt" (@("load saves\$Save", "notests") + $Extra) -Encoding ASCII
    $p = Start-TestProcess "$script:GameDir\$Exe" $script:GameDir -Minimized:$Background
    if ($PidFile) {   # "<pid> <start time, UTC FILETIME>" at once, for whoever must stop this game if the caller can't
        try { [IO.File]::WriteAllText($PidFile, ("{0} {1}`r`n" -f $p.Id, $p.StartTime.ToFileTimeUtc())) }
        catch { Write-Host "Start-Game: $PidFile not written: $_" }
    }
    for ($i = 0; $i -lt 60 -and -not $p.HasExited -and (Test-Path "$script:GameDir\opensmacx_regress.txt"); $i++) {
        Start-Sleep 1                                                          # deleted once loaded
    }
    if ($Background -and -not $p.HasExited) {
        $h = Hwnd $p
        [GM]::ShowWindow($h, 4) | Out-Null                                     # SW_SHOWNOACTIVATE
        [GM]::SetWindowPos($h, [IntPtr]1, 0, 0, 0, 0, 0x13) | Out-Null        # HWND_BOTTOM, no move/size/activate
    }
    Start-Sleep 2
    return $p
}
function Shot($p, [string]$Path) {
    # the game window's own content (PrintWindow), even when other windows cover it; never the screen
    Add-Type -AssemblyName System.Drawing
    $h = Hwnd $p
    $r = New-Object GM+RECT
    [GM]::GetClientRect($h, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ([Math]::Max(1, $r.R)), ([Math]::Max(1, $r.B))
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [GM]::PrintWindow($h, $hdc, 3) | Out-Null                              # PW_CLIENTONLY | PW_RENDERFULLCONTENT
    $g.ReleaseHdc($hdc); $g.Dispose()
    $bmp.Save($Path); $bmp.Dispose()
}
