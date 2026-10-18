# Click Pass, Stage 1 (Plumbing) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Run short scripted UI cases in two games (the reference exe and OpenSMACX) from the same
save, compare their full game state after every step, name the replaced function behind each
difference by bisection, and write a report — on the existing saves, before any scenarios exist.

**Architecture:** The harness DLL (`/DOPENSMACX_REGRESS`) gains a click-pass mode: it loads the
save input-free, reseeds both random generators once the game is under way, writes a binary state
dump whenever the driver posts a registered window message, and refuses file writes outside its
own folder. A PowerShell case driver plays one case file against one game folder through
`tools/mp/msg.ps1`. A Python tool runs cases on both sides, caches the reference side, compares
dumps field by field (struct layouts parsed from `src/*.h`), bisects failures over the redirect
table, and writes the report.

**Tech Stack:** C++ (MSVC, the existing harness build), PowerShell 5.1 (`msg.ps1`), Python 3 in
`build/venv` with `pefile`; tests with the standard library's `unittest` (no new packages).

**Spec:** `docs/superpowers/specs/2026-10-18-click-pass-design.md` (stages 1 of 5; later stages
get their own plans).

## Global Constraints

- Never write anything outside the test folders; never touch `G:\Sid Meier's Alpha Centauri`
  (the pristine `_backup_v2.1\terranx.exe` is patch input only).
- No real mouse or keyboard, no focus stealing, no desktop or second-monitor screenshots; games
  run with `Start-Game -Background`. Never `Shot` a game that may be hung.
- Kill games only by PID or by exe path, never by process name.
- Never host a network game from a renamed exe.
- Poll runs every 30-60 s at most; a hang must surface within about two minutes.
- No commits or pushes unless Kiousu asks; leave work staged.
- Sources are CRLF: edit with Edit/Write or CRLF-aware Python, never `sed -i` or heredocs with
  backslashes. Build: `& cmd /c "<scratchpad>\veh\buildroot.bat" <repo> release|regress`,
  `BUILD_EXIT=0` and 0 warnings.
- Subagents run on Opus.
- OpenSMACX behaviour must not change: all new C++ is harness-only (`#ifdef OPENSMACX_REGRESS`).
- The approved deviations (`game_data`, `battle_compute`, `best_defender`) are part of the
  reference and are never removed by any tool.

## Review Focus

1. **A game that dies mid-case** must be reported as a crash at that step (with the exit code),
   not as a difference or a runner error — test in Task 5 with a fake runner.
2. **A dump that never arrives** must end only that game (by PID) and report a hang at that step
   — test in Task 4 (driver timeout path) and Task 5 (classification).
3. **A stale reference cache** after the harness DLL, the reference exe, the save or the case
   file changes must be re-recorded, never reused — test in Task 5.
4. **Two reference runs that disagree** must mark the case nondeterministic and keep it out of
   the failures — test in Task 5.
5. **Approved-deviation rows** must survive the reference table and every bisection subset —
   test in Task 1 and Task 6.

---

### Task 1: Redirect table tools and approved-deviation tags

**Files:**
- Create: `tools/clickpass/__init__.py` (empty), `tools/clickpass/tables.py`
- Create: `tools/clickpass/tests/__init__.py` (empty), `tools/clickpass/tests/test_tables.py`
- Modify: `tools/redirects.tsv` rows for `game_data` (0x001A6AC0), `battle_compute`
  (0x00101DA0), `best_defender` (0x001044D0): add a third tab-separated column `approved`

**Interfaces:**
- Produces: `Row(offset: int, name: str, approved: bool)` (a `NamedTuple`);
  `read_rows(path: str) -> list[Row]`; `write_table(rows: list[Row], path: str) -> None`
  (writes `0x%08X\t<name>` plus `\tapproved` when set, CRLF);
  `reference_rows(rows) -> list[Row]` (only approved rows);
  `bisect_halves(candidates: list[Row]) -> tuple[list[Row], list[Row]]` (splits non-approved
  rows; raises `ValueError` if given an approved row).
- `opensmacx_patch.py`'s `read_table` already ignores a third column; leave it unchanged.

- [ ] **Step 1: Write the failing tests** in `test_tables.py`:
  `test_read_marks_approved` (a temp table with one `approved` row and one plain row → flags
  True/False, offsets parsed as hex); `test_comments_and_blank_lines_skipped`;
  `test_write_round_trip` (CRLF line endings, `approved` column preserved);
  `test_reference_rows_only_approved` (on the real `tools/redirects.tsv`: exactly the three
  names `?game_data@@YAHPAU_iobuf@@H@Z`, `?battle_compute@@YAXHHPAH0H@Z`,
  `?best_defender@@YAIIHH@Z`); `test_bisect_halves_never_takes_approved` (raises on an approved
  row; halves of 7 rows are 3 and 4, together equal to the input in order).
- [ ] **Step 2: Run** `build\venv\Scripts\python.exe -m unittest discover -s tools/clickpass/tests -t tools`
  — expected: FAIL (module missing).
- [ ] **Step 3: Implement `tables.py` and tag the three rows** in `tools/redirects.tsv` (CRLF kept).
- [ ] **Step 4: Run the tests** — expected: PASS. Also run the patcher on the tagged table
  (`opensmacx_patch.py ... -o build/patched_tagcheck`) — expected: same row count as before,
  "Functions redirected: <n>".
- [ ] **Step 5: Stage** the files (commit only when Kiousu asks).

### Task 2: Struct layouts, dump reader and comparer

**Files:**
- Create: `tools/clickpass/structs.py` (from the scratchpad's `walk.py`: parse `struct X {...};`
  in `src/*.h`, MSVC x86 natural alignment; `OSRC` env var overrides the source dir)
- Create: `tools/clickpass/dump.py`, `tools/clickpass/compare.py`, `tools/clickpass/ignore.tsv`
  (header line only: `section\tfield\treason`)
- Test: `tools/clickpass/tests/test_structs.py`, `tools/clickpass/tests/test_compare.py`

**Interfaces:**
- Dump file format (written by Task 3, read here), little-endian:
  - header: magic `b"OSXDUMP1"`, `u32 step`, `u32 section_count`;
  - each section: `char name[24]` (NUL-padded, e.g. `"Vehs"`), `char type[24]` (a struct name
    from `src/*.h`, or `"u32"` for the globals section), `u32 count`, `u32 elem_size`,
    then `count * elem_size` bytes.
  - The globals section is named `"Globals"` with type `"u32"`; its element names come from a
    second section `"GlobalNames"` of type `"char32"` (32-byte NUL-padded names, same count).
- Produces: `layout(struct_name: str) -> list[Field]` where `Field(offset, size, type, name)`
  (arrays expanded as `name[i]`); `read_dump(path: str) -> Dump` with
  `Dump.step: int` and `Dump.sections: dict[str, Section]`, `Section(type, count, elem_size, data)`;
  `diff_dumps(a: Dump, b: Dump, ignore: set[tuple[str, str]], limit: int = 50) -> list[Difference]`
  where `Difference(section: str, index: int, field: str, a, b)` and `str(Difference)` reads
  like `Vehs[412].dmg_incurred: 3 vs 2`; a count mismatch is one Difference with field `count`.
- `structs.py` must report a layout size different from the dump's `elem_size` as an error
  (the header and the exe disagree), not silently misname fields.

- [ ] **Step 1: Write the failing tests:** `test_veh_layout` (`Veh` size 0x34, `x` at 0,
  `dmg_incurred` at 0x10, `order` at 0x11); `test_diff_names_field` (two synthetic dumps built
  in the test with one `Veh` differing in byte 0x10 → exactly
  `["Vehs[1].dmg_incurred: 3 vs 2"]`); `test_diff_count_mismatch`; `test_ignore_skips_field`;
  `test_globals_by_name` (Globals `turn` 10 vs 11 → `Globals.turn: 10 vs 11`);
  `test_elem_size_mismatch_raises`.
- [ ] **Step 2: Run** the unittest command from Task 1 — expected: FAIL.
- [ ] **Step 3: Implement** `structs.py`, `dump.py`, `compare.py`. Fields not covered by the
  layout (padding) are reported as `+0xNN`.
- [ ] **Step 4: Run** — expected: PASS.
- [ ] **Step 5: Stage.**

### Task 3: Harness click-pass mode and state dumps

**Files:**
- Create: `src/iat.h`, `src/iat.cpp` — move `iat_slot` and `iat_patch` out of `src/regress.cpp`
  (made non-static, declared in `iat.h`; whole file inside `#ifdef OPENSMACX_REGRESS`)
- Create: `src/clickpass.h`, `src/clickpass.cpp` (harness-only)
- Modify: `src/regress.cpp`: `regress_init` trigger parsing (new line `clickpass`),
  `regress_auto_timer` (call `clickpass_start()` in the `AutoNoTests` branch when enabled),
  `input_hook` (call `clickpass_on_message(msg)` for every removed message)
- Modify: `opensmacx.cbp` (add the four files)

**Interfaces:**
- Produces (C++): `extern BOOL ClickpassMode;` `void clickpass_start();`
  `void clickpass_on_message(const MSG *msg);`
- Trigger file for a click-pass game: `load saves\<file>`, `notests`, `clickpass`.
- On `clickpass_start()` (game under way, as the timer already detects): create
  `<game folder>\clickpass\`, `my_srand(1)` and `random_reseed(1)`, then write
  `clickpass\ready` (empty).
- Dump trigger: `RegisterWindowMessageA("OpenSMACX.ClickpassDump")`, posted to the main window
  with `wParam` = step number. The hook writes `clickpass\dump_<step>.tmp` and renames it to
  `dump_<step>.bin` (so a reader never sees half a file).
- Dump sections (Task 2 format): `PlayersData` (`PlayerData` x 8), `Players` (`Player` x 8,
  0x00946A50), `Bases` (x `*BaseCurrentCount`), `Vehs` (x `*VehCurrentCount`), `Tiles`
  (`Map` x `*MapArea`), `Landmarks`, `Continents`, `Random` (raw bytes of the object at
  0x009BB568, type `"bytes"`), and `Globals`/`GlobalNames` with at least: `turn` (0x9A64D4),
  `game_state`, `ambience_flags`, `draw_state`, `maps_revealed`, `base_count`, `veh_count`,
  `brush1`, `brush2`, `mount_x`, `mount_y`, `crt_holdrand` (the exe CRT's per-thread state:
  call 0x006491C3 on the game thread and read `+0x14`), `key_window` (the exe's
  `Win_get_key_window` logic from msg.ps1's `KeyWindow`; heap addresses written as 1, static
  ones as their address), `base_win_visible`.
- Windowed mode in the reference exe: its folder's ini sets `[Alpha Centauri] DirectDraw=0`
  (both folders get it, so the inis match).

- [ ] **Step 1: Write the failing check** `tools/clickpass/tests/smoke_dump.ps1 -GameDir <folder>`:
  writes the trigger, `Start-Game -Background`, waits up to 60 s for `clickpass\ready`, posts
  the dump message for step 0, waits up to 30 s for `dump_0.bin`, kills the game by PID, then
  runs `python -c "from clickpass.dump import read_dump; ..."` to print the section names and
  counts. Expected before the change: no `ready` file within 60 s (FAIL).
- [ ] **Step 2: Implement** the files above; build regress (0 warnings) and release (release must
  be byte-for-byte unaffected in behaviour: everything new is inside `#ifdef OPENSMACX_REGRESS`).
- [ ] **Step 3: Make the two folders** with `tools\mp\make_testdir.ps1 -Name clickref` and
  `-Name clickosx`; add `DirectDraw=0` under `[Alpha Centauri]` in both inis; install the
  current OpenSMACX exe + regress DLL in `clickosx`, and a reference exe (patched from
  `reference_rows` via a temp table and `opensmacx_patch.py -t`) + the same DLL in `clickref`.
- [ ] **Step 4: Run the smoke check in both folders** — expected: PASS in both, sections listed,
  `Vehs` count equal to the save's unit count. Check the reference game's window is a captioned
  1024x768 client area (`GetClientRect` from msg.ps1). If it is not (the exe's imports were
  bound after `DllMain`), add a harness-only `JmpStub` on the exe's `prefs_get` (0x0059DB40)
  that calls `window_hook_imports()` and continues into the original through `OriginalEntry`,
  and rerun.
- [ ] **Step 5: Determinism probe:** run the smoke check twice in `clickref` and compare the two
  `dump_0.bin` with `compare.py` — expected: no differences (anything that differs goes into
  `ignore.tsv` only with a reason, e.g. a heap pointer).
- [ ] **Step 6: Stage.**

### Task 4: Write guard

**Files:**
- Modify: `src/clickpass.cpp`, `src/clickpass.h`, `src/regress.cpp` (trigger line `guardprobe`)

**Interfaces:**
- In click-pass mode, the import slots of both the exe and `OpenSMACX.dll` for
  `kernel32.dll!CreateFileA`, `DeleteFileA`, `MoveFileA`, `MoveFileExA`, `CopyFileA`,
  `CreateDirectoryA`, `RemoveDirectoryA` and `WritePrivateProfileStringA` go through wrappers.
  A call that would create, write, delete, rename or overwrite a path whose full path
  (`GetFullPathNameA`) is not under the game's own folder fails with
  `ERROR_ACCESS_DENIED`, and appends `WRITE-BLOCKED <api> <full path>` to
  `clickpass\guard.log`. Reads anywhere stay allowed.
- Trigger line `guardprobe`: right after `clickpass_start()` the harness tries
  `CreateFileA("..\\clickpass_guard_probe.txt", GENERIC_WRITE, ..., CREATE_ALWAYS, ...)` through
  the exe's slot, so the guard can be tested without a game action.

- [ ] **Step 1: Write the failing check** `tools/clickpass/tests/smoke_guard.ps1 -GameDir <folder>`:
  trigger with `guardprobe`, start, wait for `ready`, kill by PID; expected: `guard.log` has one
  `WRITE-BLOCKED CreateFileA` line naming the parent folder, and
  `<parent>\clickpass_guard_probe.txt` does not exist. Before the change: FAIL.
- [ ] **Step 2: Implement.**
- [ ] **Step 3: Run** the guard check in `clickosx` and `clickref` — expected: PASS; then rerun
  Task 3's smoke check — expected: still PASS (dumps inside the folder are allowed).
- [ ] **Step 4: Stage.**

### Task 5: Case driver

**Files:**
- Create: `tools/clickpass/case.ps1`
- Create: `tools/clickpass/cases/idle.case`, `reports.case`, `save_game.case`, `end_turn.case`

**Interfaces:**
- `case.ps1 -CaseFile <path> -GameDir <folder> -OutDir <folder> [-StepTimeoutSec 30] [-TurnTimeoutSec 150]`
  dot-sources `tools\mp\msg.ps1` with `$script:GameDir` set; clears `<GameDir>\clickpass\`;
  writes the trigger (`load saves\<save>`, `notests`, `clickpass`); `Start-Game -Background`;
  waits for `ready` (60 s); then runs the steps. After every action step it syncs with the game
  (`SendMessageTimeout(WM_NULL)`, 5 s), sleeps 300 ms, syncs again, posts the dump message with
  the step number and waits for `dump_<n>.bin`. At the end it kills the game by PID and copies
  the dumps, `guard.log` and the harness log to `-OutDir`.
- Writes `<OutDir>\result.json`: `{"case": str, "save": str, "steps": int, "outcome":
  "ok"|"crash"|"hang"|"blocked-write"|"setup-failed", "at_step": int|null, "exit_code":
  int|null, "seconds": float}`. `crash` = the process exited before the case ended; `hang` = a
  dump or `ready` didn't arrive in time (that game is killed by PID); `blocked-write` =
  `guard.log` not empty.
- Case file grammar (one step per line, `#` comments): first line `save <file.SAV>`; then
  `keys <SendKeys spec>`, `click <x> <y>`, `rclick <x> <y>`, `wait <ms>` (no dump),
  `endturn` (TURN COMPLETE plus clearing popups and base screens until the turn counter moves,
  as `tools/mp/autoboot.ps1` does; one dump at the end). Every step except `wait` and `save` is
  dumped.
- Stage-1 cases: `idle` (`save Council.SAV` only: dump 0 right after `ready`); `reports`
  (F1-F12 one at a time, each followed by `keys {ESC}`); `save_game` (Ctrl+S, filename
  `_cp_save` in the test folder, OK, as `tools/mp/autosave.ps1` does); `end_turn` (`endturn`).

- [ ] **Step 1: Write the failing check:** run `case.ps1` with `idle.case` against `clickref`
  — expected before the script exists: error.
- [ ] **Step 2: Implement** `case.ps1` and the four case files.
- [ ] **Step 3: Run each case once in `clickref`** — expected: `outcome` `ok` for all four, one
  dump per action step, `_cp_save.SAV` present in `clickref\saves`, nothing in the live folder.
- [ ] **Step 4: Hang path:** run `idle.case` with a trigger that omits `clickpass` (a hidden
  `-NoClickpass` switch for this test) — expected: `outcome` `hang` at step 0 within 60 s, and
  the game process (that PID only) gone.
- [ ] **Step 5: Stage.**

### Task 6: Runner, reference cache and self-check

**Files:**
- Create: `tools/clickpass/clickpass.py` (command line), `tools/clickpass/runner.py`
- Test: `tools/clickpass/tests/test_runner.py`

**Interfaces:**
- Consumes: `case.ps1` (Task 5) through `run_case(case: str, game_dir: str, out_dir: str) ->
  CaseResult` (wraps `powershell -NoProfile -File case.ps1 ...` and reads `result.json`);
  `read_dump`/`diff_dumps` (Task 2).
- Produces: `CaseResult(outcome: str, at_step: int|None, exit_code: int|None, dumps: list[str])`;
  `reference_key(ref_exe: str, dll: str, save: str, case_file: str) -> str` (sha256 over the
  four files' contents, hex); `run_pass(cases: list[str], ref_dir: str, osx_dir: str,
  out_root: str, run_case=run_case) -> PassResult` with one `CaseOutcome(case, status, at_step,
  differences: list[Difference], detail: str)` per case, `status` in `"pass"`, `"fail"`,
  `"crash"`, `"hang"`, `"blocked-write"`, `"nondeterministic"`, `"reference-broken"`.
- Cache layout: `build/clickpass/reference/<case>/<key>/` holding the reference dumps and
  `meta.json`; a miss runs the reference twice, compares the two runs, and on any difference
  stores the case as `nondeterministic` (with the differences) instead of a reference.
- Command line: `clickpass.py run [--cases idle,reports,...] [--ref-dir G:\SMACX-opensmacx-test-clickref]
  [--osx-dir G:\SMACX-opensmacx-test-clickosx]` writes `build/clickpass/<YYYY-MM-DD-HHMM>/`
  (per-case folders plus `result.json`).

- [ ] **Step 1: Write the failing tests** with a fake `run_case` that copies prepared dumps:
  `test_cache_hit_runs_osx_only` (second pass: the fake is called once per case, on the osx
  folder only); `test_key_changes_with_dll` (different DLL bytes → different key → reference
  re-recorded); `test_self_check_disagreement_is_nondeterministic` (two reference runs differ →
  status `nondeterministic`, not `fail`); `test_osx_crash_reported_as_crash` (fake returns
  `crash` at step 3 with exit code `0xC0000005` → status `crash`, `at_step` 3);
  `test_osx_hang_reported_as_hang`; `test_reference_crash_is_reference_broken`;
  `test_first_difference_step` (osx differs from step 2 on → status `fail`, `at_step` 2,
  differences from step 2 only).
- [ ] **Step 2: Run** — expected: FAIL.
- [ ] **Step 3: Implement** `runner.py` and the `run` command.
- [ ] **Step 4: Run** the unit tests — expected: PASS. Then live:
  `clickpass.py run --cases idle,reports,save_game` twice — expected: the first records the
  references (with self-check), the second runs only the OpenSMACX side; results printed per
  case.
- [ ] **Step 5: Stage.**

### Task 7: Bisection

**Files:**
- Modify: `tools/clickpass/clickpass.py` (command `bisect`), create `tools/clickpass/bisect.py`
- Test: `tools/clickpass/tests/test_bisect.py`

**Interfaces:**
- Consumes: `read_rows`, `reference_rows`, `bisect_halves`, `write_table` (Task 1); `run_case`,
  `diff_dumps` (Tasks 5, 2).
- Produces: `find_culprits(rows: list[Row], reproduces: Callable[[list[Row]], bool]) ->
  BisectResult(culprits: list[Row], runs: int, clean: bool)`; `reproduces(subset)` patches an
  exe from `reference_rows + subset` (temp table, `opensmacx_patch.py -t`), installs it in the
  osx folder, runs the failing case and returns whether it still differs from the reference at
  the same step. Plain halving while exactly one half reproduces; when both halves reproduce
  (two culprits) or neither does (an interaction), recurse into each half / report the
  smallest reproducing set and set `clean = False`.
- Command: `clickpass.py bisect <case> [--run <pass folder>]` prints the culprit rows and writes
  `bisect.json` into the case folder; it restores the full exe in the osx folder afterwards.

- [ ] **Step 1: Write the failing tests** with a fake `reproduces`: `test_single_culprit`
  (culprit row 37 of 700 → found in ≤ 11 runs, `clean` True); `test_two_culprits` (rows 5 and
  600 → both found); `test_interaction_not_clean` (reproduces only when rows 10 and 20 are both
  present → result contains both, `clean` False); `test_approved_rows_always_present` (every
  subset passed to `reproduces` contains the three approved rows, none is ever a culprit).
- [ ] **Step 2: Run** — expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** — expected: PASS. Live: `clickpass.py bisect idle` on a passing case —
  expected: "no difference to bisect", osx exe restored (same hash as before).
- [ ] **Step 5: Stage.**

### Task 8: Report, inventory, decisions and the first real pass

**Files:**
- Create: `tools/clickpass/report.py`, `tools/clickpass/inventory.tsv`,
  `tools/clickpass/leftover_reads.tsv`, `tools/clickpass/decisions.tsv`
- Modify: `tools/clickpass/clickpass.py` (command `report`; `run` calls it at the end)
- Modify: `docs/regression-testing.md` (new section "Click pass, stage 1")
- Test: `tools/clickpass/tests/test_report.py`

**Interfaces:**
- `inventory.tsv` columns: `screen\tcontrol\tcase\tstatus\tnote` (`status` in `covered`,
  `not-reachable`, `skipped`); stage 1 lists the map's F1-F12 screens, Ctrl+S save, and
  TURN COMPLETE as covered, everything else not yet listed.
- `leftover_reads.tsv` columns: `function\toffset\treads\tnote`; seeded with `alien_fauna`
  0x522180 (stale draw coordinates) from docs/regression-testing.md.
- `decisions.tsv` columns: `case\tfunction\tdecision\tdate\twhy`; seeded with the first one:
  `*\twants_to_attack\tstart the region counts at zero (the intended design)\t2026-10-18\tKiousu:
  replicate the original's intent, not the shipped uninitialized buffer`.
- `write_report(pass_result, bisects, inventory, leftovers, decisions, path)` writes
  `report.md` with sections in this order: Summary (counts per status), Failures (case, step,
  status, first differences, culprit and its row, what's being done: "held back" / "fix
  dispatched" / "open"), Needs Kiousu's decision (culprit in `leftover_reads.tsv` or bisection
  not clean, with what each game had and why, unless `decisions.tsv` already settles it),
  Nondeterministic cases, Coverage (from the inventory).

- [ ] **Step 1: Write the failing tests:** `test_leftover_culprit_goes_to_decision_section`
  (culprit `alien_fauna` → listed under "Needs Kiousu's decision", not under Failures);
  `test_decided_case_not_raised_again`; `test_unclean_bisect_goes_to_decision_section`;
  `test_coverage_counts` (inventory with 3 covered, 2 not-reachable → "3 covered, 2 not
  reachable"); `test_section_order`.
- [ ] **Step 2: Run** — expected: FAIL.
- [ ] **Step 3: Implement**, and write the docs section (how to run, what each status means, the
  fix loop and the decision loop from the spec).
- [ ] **Step 4: First real pass:** `clickpass.py run --cases idle,reports,save_game,end_turn` on
  Council.SAV, then repeat `idle`, `reports` and `end_turn` on `sw_aki2120.SAV` and
  `sw_steam1.SAV` (case files take the save from their first line, so add `*_aki2120.case` /
  `*_steam1.case` copies). Expected: a `report.md` with every case in some status; for each
  failure run `clickpass.py bisect`; copy the report to
  `docs/regression-runs/2026-10-18-clickpass-stage1.md`. Report the results to Kiousu.
- [ ] **Step 5: Stage.**
