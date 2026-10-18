# Click pass: every button and action, compared with the unmodified game

Date: 2026-10-18. Status: design, for Kiousu's approval.

## Why

The regression suite compares functions one at a time: the harness DLL loads a save, runs a
replaced function and the exe's original from the same snapshot, and compares what they wrote.
That only covers what those direct calls reach. Bugs can hide behind real screens instead: the
save-header crash only showed up when the game was saved through the UI. Kiousu wants the whole
suite expanded so that every button and every player action we can reach gets pressed at least
once, and wants to know **which buttons fail and how we're going to fix them**.

## What it produces

A report after every run, listing:

- every button, menu item, key command and unit action that was pressed, by screen and scenario;
- every failure: the action, the screen, how it failed (crash, hang, or a difference from the
  unmodified game), exactly what differed in readable terms ("unit 412, Gaian missile launcher:
  damage 3 in the unmodified game, 2 in OpenSMACX"), the replaced function that causes it, and
  what is being done about it (held back, fix dispatched, fixed and confirmed);
- coverage: what could not be reached or was skipped, and why.

Files: `build/clickpass/<date>/report.md` plus per-case logs; the summary is copied into
`docs/regression-runs/` alongside the sweep logs, and summarized for Kiousu in chat.

## Decided by Kiousu

- **A failure is a crash, a hang, or any difference from the unmodified game.** Doing the same
  click in both games must leave both in the same state.
- **Gameplay actions are in, not just buttons:** artillery bombarding units, attacks and every
  other unit action, as well as every screen, button and menu.
- **Situations are built in the Scenario Editor:** small purpose-made scenarios where each unit
  is set up for the action under test (an artillery piece with an enemy in range, a probe team
  next to a rival base, a former on each kind of terrain). **What the editor can't set up**
  (a Council state, exact unit damage or moves left, energy reserves, one turn from victory) is
  made by editing the saved file, and **Kiousu spot-checks each edited file by loading it**
  before the pass uses it.
- **Differences caused by vanilla reading memory it never wrote** (such as `wants_to_attack`'s
  seven stack slots, `alien_fauna`'s stale draw coordinates) are **understood and decided case
  by case**: the report explains why each one happens and Kiousu decides what to do with it.
  First decision (2026-10-18): Kiousu wants functions to do what the original code evidently
  intends rather than reproduce the shipped bug, so `wants_to_attack` starts its counts at zero.
- **Game-ending actions are in:** quitting, resigning, retiring "and such".
- **Multiplayer is in scope in principle.**
- **Never write outside the test folder.**
- **Runs after every batch, alongside the whole suite**, in the background; 20-30 minutes is fine.
- **Fix loop:** failing functions go back to the exe's version at once (removed from
  `tools/redirects.tsv`), agents are dispatched to fix them the way batches work now, and the pass
  is rerun to confirm.

## Goals

1. Press every reachable button, menu item, key command and unit action at least once.
2. Compare the result of each against the unmodified game, step by step.
3. Name the replaced function behind each failure, so the fix is aimed.
4. Keep a coverage list, so gaps are visible rather than silent.
5. Fit after every batch: one command, background windows, about 20-30 minutes.

## Non-goals

- Judging whether vanilla behaviour is *right*. Vanilla quirks are the reference, never "fixed".
- Pixel-exact screen comparison. What is compared is game state plus which screen or dialog is
  open, not rendered images.
- Replacing the function-level suite. The click pass runs after it, not instead of it.

## Anti-goals (failure even if it "works")

- Writing anything outside the test folders, above all into `G:\Sid Meier's Alpha Centauri`.
- Taking over Kiousu's PC: no real mouse or keyboard, no focus stealing, no screenshots of the
  desktop or the second monitor. Games run in background windows (`-Background` in msg.ps1).
- Bending game rules to make a case pass. The Scenario Editor sets the situation up; the action
  itself is played under normal rules (editor mode off, scenario saved and reloaded).
- Reporting false failures: a difference that also shows up between two runs of the unmodified
  game is noise, and must be reported as such, not as an OpenSMACX bug.
- Silent gaps: a button that was never pressed must show up as not covered.

## Constraints

- Existing tooling to build on: the harness DLL (`/DOPENSMACX_REGRESS`) with its input-free load
  (`load saves\X` trigger), message hooks (posted keys/modifiers/cursor work in a background
  window), watchdog (30 s heartbeat, kill at 10 min), `GameSnapshot`; `tools/mp/msg.ps1` (Click,
  Drag, Keys, ReadInt, KeyWindow, PopupOpen, Start-Game, Shot via PrintWindow); `autorun.ps1`;
  per-folder test copies from `make_testdir.ps1`; windowed mode (`[OpenSMACX] Windowed=1`).
- Several games may run at once, each in its own test folder; kill games only by PID or by exe
  path, never by name. Never take a screenshot of a game that may be hung (PrintWindow blocks).
- Never run a renamed exe that hosts a network game (Windows firewall prompt on Kiousu's screen).
- The pristine exe (`_backup_v2.1\terranx.exe`) is patch input only.
- Poll runs every 30-60 s at most; a hang must be noticed within a minute or two.

## How it works (the main session's calls, open to Kiousu's correction)

### Two games, same input

Each case runs in two games from the same starting point:

- **Unmodified side (the reference):** an exe patched with none of the replacement functions
  except the deviations Kiousu has approved, plus the test plumbing (harness DLL loaded for
  windowed mode, input hooks, input-free load and state dumps), so every other game function runs
  the original code. The approved deviations today are `game_data`'s save-header fill (without it
  the v2.1 exe takes every save on this machine for an old save and corrupts or crashes, see
  "Save header, 2026-10-17" in docs/regression-testing.md) and the negative-attacker guard in
  `battle_compute`/`best_defender`. Their rows are tagged as approved deviations in
  `tools/redirects.tsv`; bisection and the fix loop never remove them, and the report lists them
  as part of the reference. Its folder's ini sets whatever the plumbing needs (for example
  `DirectDraw=0`, which windowed mode otherwise gets from the replaced `prefs_get`).
- **OpenSMACX side:** the exe patched from the current `tools/redirects.tsv`, with the same DLL.

The driver sends the identical sequence of clicks and keys to both. After each step, once the
game is idle, the harness DLL writes a **state dump** to its test folder (triggered by a posted
window message, so no extra input is needed): everything `GameSnapshot` holds (players, bases,
units, map tiles, landmarks, continents, path state), plus the turn, game state flags, the
selected unit, the random generators' state, which screen or dialog has the focus, and any files
the step wrote in the test folder (by name and content hash). Dumps are compared field by field
and the first differences are written out in readable form.

### Short independent cases

Work is split into many short **cases**: load a scenario, perform one action or a few related
steps, dump after each. A difference in one case doesn't poison the rest. Cases are spread over
several pairs of test folders run in parallel (the machine has 24 cores) to stay within the
20-30 minute budget.

### Same dice

Both games load the same file; the harness reseeds the C runtime `rand()` and the game's random
object at load, and pins `timeGetTime()` where results depend on it (as the `world_borehole`
test already does). As a **self-check**, every case runs twice on the unmodified side when its
reference result is recorded; a case whose two unmodified runs disagree is marked
nondeterministic, fixed in the harness if possible, and otherwise excluded from comparison and
listed as such in the report.

### Recorded reference results

The unmodified side's dumps are recorded once and reused: they are keyed by the hashes of the
reference exe, the harness DLL, the scenario file and the case script, and re-recorded (with the
self-check) only when one of those changes. A normal run after a batch therefore plays only the
OpenSMACX side, which is what keeps every-control coverage inside 20-30 minutes. If the full set
still doesn't fit, cases rotate so that every case runs at least every few batches, and the
report says which ran.

### Scenarios from the Scenario Editor

Each family of actions gets a small scenario built through the real Scenario Editor by a click
script (so it can be rebuilt any time), saved into the test folder, then copied into the repo so
every run starts from the same file. Expected families: combat (artillery, attacks, air strikes,
missiles, probe team actions), terraforming (every kind, on suitable terrain), colony pods and
bases (founding, joining), the base screen, diplomacy, and end-of-game set-ups. Editor mode is
off when the actions are played. Set-ups the editor can't make are applied by a script that
edits the saved file offline (so it can be redone); each edited file waits for Kiousu's
spot-check (loading it in the game) before any case uses it, and the inventory records which
files are checked.

### What gets pressed

- Every item of every menu on the main map, the F-key and report screens, the base screen and
  its buttons, the design workshop, social engineering, the Planetary Council, the datalinks,
  diplomacy/commlink, save/load and preferences.
- Every unit action, through its key command or menu entry, with the target chosen on the map.
- End of turn, including the other factions' moves, in a few cases.
- Game-ending actions (quit, resign, retire, and win/lose screens a scenario can set up), each in
  its own case from a fresh load since nothing can follow it; compared on what they show and
  write, such as the score and the Hall of Fame entry in the test folder.

"Every" means every distinct control and action at least once. Long lists of entries that work
the same way (datalinks pages, production list items, workshop components, preference
checkboxes) get a few representative entries each (first, last and one in between), and the
inventory says so.

An **inventory file** in the repo lists every screen, control and action with its status
(covered, not reachable yet, skipped and why); the report's coverage section is generated from it.

### Naming the culprit

When a case fails, the pass finds the responsible function by **bisection**: it patches exes
from halves of the redirect table (with `opensmacx_patch.py`) and reruns only the failing case
until one function (or the smallest set) reproduces the difference. About ten reruns for the
current ~760 rows. The result goes in the report.

### Leftover-memory differences

Some vanilla functions read memory they never wrote, so their result depends on what code ran
just before; OpenSMACX's code leaves different leftovers than the exe's. A failure whose culprit
reads such memory (known ones are listed in a file next to the inventory; bisection that won't
narrow cleanly is a hint of a new one) goes into its own report section, **not** into the fix
loop. Each entry explains why: the function, the address it reads before writing, the value each
game had there, and which code left it. Kiousu decides each case (for example make the function
do what its code evidently means, as Kiousu chose for `wants_to_attack` on 2026-10-18: its
region counts now start from zero; or match the original's leftovers; or accept the
difference); the decision is recorded so the case isn't raised again unless its explanation
changes.

### Fix loop

1. Remove the culprit's row from `tools/redirects.tsv` so the game uses the exe's version.
2. Dispatch a worker (worktree, own test folder) and an independent reviewer to fix it, as in the
   batches.
3. Rerun the failing case, then the whole pass, before the function goes back in.

### Live-folder guard

Kiousu may be playing from `G:\Sid Meier's Alpha Centauri` while a pass runs, so a folder diff
can't tell the test games' writes from Kiousu's. Instead the harness DLL (test builds only) wraps
the file-writing imports of both the exe and OpenSMACX.dll (the narrow and wide versions, since the
DLL's own C runtime writes through the wide ones) and refuses, and logs as a failure, any write
outside the game's own test folder and its one allowed outside folder; the run stops on the first
one. The pass also checks after each run that nothing outside the test folders carries a test
game's process as writer in that log.

The save dialog can save anywhere, and saving to another folder is a feature the pass must test,
so each test game also has one allowed folder outside its own (Kiousu, 2026-10-18):
`G:\SMACX-clickpass-outside\<game folder name>` (for example `...\clickref`, `...\clickosx`), one
per game so parallel games never collide. The case driver names it in the trigger file and empties
it before each case; cases that save "elsewhere" navigate the dialog there.

### Stages

1. Plumbing: unmodified-side exe, state dumps, two-game comparison, self-check, bisection, a few
   cases on the existing saves.
2. Scenarios from the Scenario Editor and the unit actions.
3. Every screen and button, with the inventory and coverage report.
4. Game-ending actions.
5. Multiplayer, reusing the two-player scripts (`mphost.ps1`/`mpclient.ps1`/`mplocal.ps1`),
   without hosting from a renamed exe. Windows firewall rules are per exe path, so every test
   folder copy counts as a new exe; which paths may host gets settled with Kiousu when stage 5
   is planned, not before.

Each stage gets its own implementation plan once the one before it is in, since stage 1 settles
the unknowns (determinism, button discovery) the later ones depend on.

## Left to the builder

- How clickable controls are found: walking the game's own window and button objects from the
  harness DLL (preferred if feasible, since it finds buttons without hand-made coordinates), or
  per-screen tables made from screenshots; the inventory file is required either way.
- How map tiles are targeted on screen (for example the harness centring the map on a tile and
  reporting its screen position).
- The state dump format and how readable differences are named.
- How the Scenario Editor is driven, and the scenario file format the game uses for it.
- How many folder pairs run in parallel, case timeouts, and the exact report layout.
- Whether worker agents also run a targeted subset of the pass in their own folders.

## Risks

- **Determinism** is the biggest unknown: hidden time-based seeds or animation timing could make
  two unmodified runs differ. The self-check exists to expose this early, in stage 1.
- **Button discovery** from memory may not be feasible for every screen; hand-made tables are
  the fallback.
- **Scenario Editor scripting** is a sizeable piece of work on its own.
- **Run time:** end-of-turn cases on big saves take tens of seconds each; they must be few.
