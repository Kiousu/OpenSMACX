# Multiplayer (network) testing notes

Status as of 2026-10-14. The net_*() / message_*() / synch_*() code in `src/net.cpp` has only had
its single-player branch exercised. The network branch needs two game instances talking over
DirectPlay. What was tried, what works, what blocks.

## Two instances on one desktop (works up to game start, unreliable after)

- Two copies of the patched game in separate folders (`G:\SMACX-opensmacx-test` as host,
  `G:\SMACX-opensmacx-test2` as client) launch fine; the exe has no single-instance guard.
- MULTIPLAYER -> "Internet TCP/IP Connection For DirectPlay" works. Host on A; on B choose Join,
  type `127.0.0.1` in DirectPlay's "Locate Session" dialog, pick the game, OK. Both players show
  in one lobby. The game pre-fills the last used names, so typed names get appended.
- Windows Firewall prompts once per exe path and once for `dplaysvr.exe` (Microsoft DirectPlay
  Helper). Allow rules now exist for both test folders and the helper.
- With all 501 redirects active both instances reached planetfall (host Svensgaard, client
  Yang), then stalled at "Upkeep Phase: Preliminary" with each spinning one core.
- Control: the STOCK exe in the same setup crashed in both instances shortly after launch
  (access violations in rankings() and load_sprites()). So the stall is not evidence of a bug
  in the redirected code: the environment is the problem. Both instances are exclusive-mode
  DirectDraw windows fighting over one desktop, and each only pumps DirectPlay while it has
  focus.

## Windows Sandbox for the second instance (blocked on the host)

Configs live in `build/sandbox/` (ignored): `smacx-client.wsb` maps the client folder and a
scripts folder, `sb_join.ps1` is the LogonCommand that launches the client and joins the host
at the vEthernet (Default Switch) address 172.27.144.1. `diag.wsb` / `diag.ps1` is a no-game
diagnostic.

On 2026-10-14 the sandbox VM never booted (vmmem at 0 CPU, "connection to the Windows Sandbox
environment was lost" at the splash) in all three attempts: with the game, with no game from a
clean state, and with vGPu disabled. The LogonCommand never ran. Host container services were
all running. Needs a check from an admin session (plain `WindowsSandbox.exe` with no config,
reboot if it also hangs). Closing the sandbox client by killing its process leaves orphaned
`vmmemWindowsSandbox` VMs; use the dialog's Close button.

## Second Windows machine on the LAN (works, 2026-10-15)

This is the route that produced a real two-player game. Setup on the client machine (a laptop,
AMD graphics, 2560x1600 at 175% scaling, Windows 11 Home), driven through a Claude session there:

- The game data must come from the host's test folder, not from the client's own Steam install:
  the Steam exe is a different build from the GOG/UP 2.1 exe we patch (3,094,576 bytes,
  SHA256 573FD44C...), so data compatibility is not guaranteed. A zip of the test folder minus
  movies and saves is about 147 MB; serve it with `python -m http.server` (the Python inbound
  firewall rule on the Private profile covers it) and fetch it over the LAN.
- The pristine-based patched exe crashes on that laptop before the menu (fault offset 0x1F7E90,
  `Win::is_visible` with a null window): the native exclusive-mode 8-bit DirectDraw path is not
  available there and the Windows compatibility shims (HIGHDPIAWARE, 256COLOR,
  DWM8And16BitMitigation, 640X480, WINXPSP3) do not help. The stock exe only works there because
  PRACX takes over the display. Solution: patch the PRACX-enabled stock exe instead
  (`opensmacx_patch.py -e <live terranx.exe>`); only one redirect site differs between the two
  exes (read_units' prologue, an Unofficial Patch change) and our DLL replaces that function
  anyway. In `Alpha Centauri.Ini` set the `[PRACX]` keys ScreenWidth/ScreenHeight/WindowWidth/
  WindowHeight to 1024/768 so the original layout and all driver coordinates apply.
- Legacy DirectPlay must be enabled on the client (elevated: `dism /online /enable-feature
  /featurename:DirectPlay /all`). Without it the launch is intercepted by the feature-on-demand
  prompt and then fails with "unable to start correctly".
- The Windows Firewall prompt at the TCP/IP step steals focus from the game, so the scripted
  join misses the Host/Join dialog and falls through to hosting its own game. Do the join by
  hand (Multiplayer > TCP/IP > OK > allow > Join > OK > name > OK > Locate Session: host IP >
  pick game > OK > Make Ready) and attach the script afterwards for the turn loop
  (`tools/mp/mpclient.ps1 -Attach`). The driver (`ui.ps1`) is DPI-aware and offsets clicks by the game
  window's client origin, so it works for both the exclusive-mode host and a windowed client.
- Host side: `tools/mp/mphost.ps1 -Phase lobby` opens the lobby; `-Phase launch` presses LAUNCH GAME and
  ends turns. ICMP and SMB are blocked on the host, so the client cannot ping it; DirectPlay
  still works (program rules for the exe and dplaysvr.exe).

Results (548 redirects):

- First run, host on the pristine-based exe and client on the PRACX-based exe: both reached
  planetfall, the client (Hive) got all its starting units, the host (Pirates) got only the Unity
  gunship and sat at "Upkeep 6". This looked like a defect but was not one: every stock pairing
  that could be tried afterwards (live PRACX exe on both; pristine host with PRACX client) crashed
  on join or at the client's planetfall, so mixing exe builds between host and client is what is
  unreliable, patched or not. A host playing alone in a network game also sits at "Upkeep 6"
  ("Checksums & Synchronization") with no units even on the stock exe, so that is not a test.
- Second run, the SAME PRACX-based patched exe on both machines (host Pirates, client Hive):
  planetfall set up both players correctly (two sea escape pods plus the gunship for the host)
  and both sides took a turn. Save: `saves\Multiplayerworks.SAV` in the host folder. This is the
  first verified run of the redirected net_* / message_* / synch_* code in a real two-player game.

Rule from this: host and client must run byte-identical exes (patch the same base exe with the
same table for both) and identical OpenSMACX.dll builds.

### Windows Sandbox (tried first, blocked by DirectPlay)

With the vGPU/dual-GPU problem fixed the sandbox boots and runs its LogonCommand, DirectPlay
files are present and the host's DirectPlay registry keys can be imported, but the legacy
DirectPlay *feature* cannot be enabled inside the sandbox (DISM error 14098, the component store
is read-only) and the feature-on-demand shim then refuses to start the game. Disabling the
compatibility engine by policy inside the sandbox was the next idea and was not tried.

## Other options

- A windowed-mode DirectDraw shim (cnc-ddraw or DDrawCompat) in both test folders would keep
  both windows visible and pumping. Third-party binaries; the user is okay with this route.
- A second Windows machine on the LAN; the firewall rules already allow inbound for the exe.

## Driving the game from a script

`ui.ps1` (scratch, not in repo) clicks/keys/screenshots by process id; the game draws at
1024x768 in the top-left of the desktop. Units move by mouse drag; numpad needs NumLock.
Menu coordinates: MULTIPLAYER (835,587); service list rows y=365/384/404/422, OK (389,449);
Host/Join rows y=383/405, OK (389,435); lobby LAUNCH/MAKE READY (546,306).
