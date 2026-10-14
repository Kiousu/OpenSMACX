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

## Other options

- A windowed-mode DirectDraw shim (cnc-ddraw or DDrawCompat) in both test folders would keep
  both windows visible and pumping. Third-party binaries; the user is okay with this route.
- A second Windows machine on the LAN; the firewall rules already allow inbound for the exe.

## Driving the game from a script

`ui.ps1` (scratch, not in repo) clicks/keys/screenshots by process id; the game draws at
1024x768 in the top-left of the desktop. Units move by mouse drag; numpad needs NumLock.
Menu coordinates: MULTIPLAYER (835,587); service list rows y=365/384/404/422, OK (389,449);
Host/Join rows y=383/405, OK (389,435); lobby LAUNCH/MAKE READY (546,306).
