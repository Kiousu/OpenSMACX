param(
    [string]$Action,     # shot | click | rclick | key | focus | status | drag | vk | origin
    [int]$X = 0, [int]$Y = 0,
    [string]$Keys = "",
    [string]$Out = "",
    [int]$W = 1024, [int]$H = 768,
    [int]$ProcId = 0,
    [string]$Title = ""   # dlg: window title to focus before sending Keys
)
# Game UI driver. Coordinates are in the game's own 1024x768 space and are offset by the game
# window's client-area origin, so it works for the exclusive-mode exe (origin 0,0) and for a
# windowed PRACX game alike. DPI-aware: all numbers are physical pixels.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class U {
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
  [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
}
"@
[U]::SetProcessDPIAware() | Out-Null
$proc = if ($ProcId) { Get-Process -Id $ProcId -ErrorAction SilentlyContinue } else { Get-Process terranx_opensmacx -ErrorAction SilentlyContinue | Select-Object -First 1 }
$OX = 0; $OY = 0
if ($proc -and $proc.MainWindowHandle -ne 0) {
    $pt = New-Object U+POINT; $pt.X = 0; $pt.Y = 0
    if ([U]::ClientToScreen($proc.MainWindowHandle, [ref]$pt)) { $OX = $pt.X; $OY = $pt.Y }
}
function Focus {
    if ($proc -and $proc.MainWindowHandle -ne 0) {
        for ($i = 0; $i -lt 10; $i++) {
            [U]::SetForegroundWindow($proc.MainWindowHandle) | Out-Null
            Start-Sleep -Milliseconds 200
            if ([U]::GetForegroundWindow() -eq $proc.MainWindowHandle) { break }
        }
        Start-Sleep -Milliseconds 700
    }
}
switch ($Action) {
  "status" {
    if (-not $proc) { "NOT RUNNING"; break }
    $proc.Refresh()
    "PID=$($proc.Id) alive=$(-not $proc.HasExited) title='$($proc.MainWindowTitle)' WS={0:N0}MB fg={1} origin={2},{3}" -f ($proc.WorkingSet64/1MB), ([U]::GetForegroundWindow() -eq $proc.MainWindowHandle), $OX, $OY
  }
  "origin" { "$OX,$OY" }
  "focus" { Focus; "focused" }
  "shot" {
    $bmp = New-Object System.Drawing.Bitmap $W, $H
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($OX, $OY, 0, 0, (New-Object System.Drawing.Size $W, $H))
    $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
    "saved $Out"
  }
  "click" {
    Focus; [U]::SetCursorPos($OX + $X, $OY + $Y) | Out-Null; Start-Sleep -Milliseconds 150
    [U]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 80; [U]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
    "clicked $X,$Y (+$OX,$OY)"
  }
  "rclick" {
    Focus; [U]::SetCursorPos($OX + $X, $OY + $Y) | Out-Null; Start-Sleep -Milliseconds 150
    [U]::mouse_event(8, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 80; [U]::mouse_event(16, 0, 0, 0, [UIntPtr]::Zero)
    "right-clicked $X,$Y"
  }
  "dlg" {
    # focus a separate native dialog (e.g. DirectPlay's "Locate Session") by title and type into it
    $h = [IntPtr]::Zero
    for ($i = 0; $i -lt 20 -and $h -eq [IntPtr]::Zero; $i++) { $h = [U]::FindWindow($null, $Title); if ($h -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 500 } }
    if ($h -eq [IntPtr]::Zero) { "dialog '$Title' not found"; break }
    for ($i = 0; $i -lt 10; $i++) { [U]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 200; if ([U]::GetForegroundWindow() -eq $h) { break } }
    Start-Sleep -Milliseconds 300
    [System.Windows.Forms.SendKeys]::SendWait($Keys); "sent $Keys to '$Title'"
  }
  "key" {
    Focus; [System.Windows.Forms.SendKeys]::SendWait($Keys); "sent $Keys"
  }
  "drag" {
    # left-button drag from (X,Y) to (W,H), game coordinates
    Focus; [U]::SetCursorPos($OX + $X, $OY + $Y) | Out-Null; Start-Sleep -Milliseconds 200
    [U]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 150
    for ($i = 1; $i -le 10; $i++) { [U]::SetCursorPos([int]($OX + $X + ($W - $X) * $i / 10), [int]($OY + $Y + ($H - $Y) * $i / 10)) | Out-Null; Start-Sleep -Milliseconds 40 }
    Start-Sleep -Milliseconds 150; [U]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
    "dragged $X,$Y -> $W,$H"
  }
  "vk" {
    Focus; [U]::keybd_event([byte]$X, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60; [U]::keybd_event([byte]$X, 0, 2, [UIntPtr]::Zero); "vk $X"
  }
}
