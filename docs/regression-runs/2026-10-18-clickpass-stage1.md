# Click pass report: 2026-10-18-0700

> Copied from the pass's report.md (as `clickpass.py bisect end_turn_steam1` rewrote it). The pass
> folders named here, `build\clickpass\2026-10-18-0700` and the earlier `2026-10-18-0536`, were in
> the clickpass-stage1 worktree (`C:\Programming\OpenSMACX\.claude\worktrees\clickpass-stage1`),
> which goes away after the merge: those paths are historical. This pass is the rerun after the final
> review's fixes (written files compared, bigger dumps, clock pinned from start-up, every reference
> recorded again with its self-check). The first full pass, `2026-10-18-0536`, before those fixes,
> had the same outcome: 9 pass, end_turn_steam1 fail at step 1, culprit `hex_cost` (clean, 12 runs).

Pass folder `C:\Programming\OpenSMACX\.claude\worktrees\clickpass-stage1\build\clickpass\2026-10-18-0700`, started 2026-10-18T07:00:19. Reference game `G:\SMACX-opensmacx-test-clickref`, OpenSMACX game `G:\SMACX-opensmacx-test-clickosx`. The reference is the unmodified exe with only the approved deviations: battle_compute, best_defender, game_data.

## Summary

10 cases: 9 pass, 1 fail.

| case | save | status | step | time | reference |
|---|---|---|---|---|---|
| end_turn | Council.SAV | pass |  | 84.8 s | recorded |
| end_turn_aki2120 | sw_aki2120.SAV | pass |  | 37.3 s | recorded |
| end_turn_steam1 | sw_steam1.SAV | fail | 1 | 37.2 s | recorded |
| idle | Council.SAV | pass |  | 22.0 s | recorded |
| idle_aki2120 | sw_aki2120.SAV | pass |  | 22.0 s | recorded |
| idle_steam1 | sw_steam1.SAV | pass |  | 22.0 s | recorded |
| reports | Council.SAV | pass |  | 73.4 s | recorded |
| reports_aki2120 | sw_aki2120.SAV | pass |  | 68.9 s | recorded |
| reports_steam1 | sw_steam1.SAV | pass |  | 70.4 s | recorded |
| save_game | Council.SAV | pass |  | 41.1 s | recorded |

## Failures

### end_turn_steam1: fail at step 1

- On sw_steam1.SAV: differs from the reference from step 1 on (39 differences shown)
- First differences (unmodified game vs OpenSMACX):
  - `Vehs[79].x: 33 vs 31`
  - `Vehs[79].y: 57 vs 55`
  - `Vehs[79].waypoint_x[0]: 33 vs 31`
  - `Vehs[79].waypoint_y[0]: 57 vs 55`
  - `Vehs[79].terraforming_turns: 4 vs 6`
  - `Vehs[79].unk_5: 6 vs 7`
  - `Tiles[3407].visibility: 0 vs 1`
  - `Tiles[3471].visibility: 0 vs 1`
  - `Tiles[3472].visibility: 0 vs 1`
  - `Tiles[3534].visibility: 0 vs 1`
  - ... 29 more in the pass's result.json
- Culprit (bisection: clean, 12 runs): `hex_cost` (0x593510, row `0x00193510 ?hex_cost@@YAHHHIIIIH@Z`)
- Being done: open

## Needs Kiousu's decision

None.

## Nondeterministic cases

None.

## Coverage

Inventory (tools/clickpass/inventory.tsv): 14 covered. Controls not listed there are not covered.

| screen | control | status | its cases in this pass | note |
|---|---|---|---|---|
| Main map | F1 Datalinks | covered | reports pass, reports_aki2120 pass, reports_steam1 pass | Escape closes it |
| Main map | F2 Laboratories Status | covered | reports pass, reports_aki2120 pass, reports_steam1 pass | Escape closes it |
| Main map | F3 Energy Banks | covered | reports pass, reports_aki2120 pass, reports_steam1 pass | Escape closes it |
| Main map | F4 Base Operations Status | covered | reports pass, reports_aki2120 pass, reports_steam1 pass | Escape closes it |
| Main map | F5 Secret Project Data | covered | reports pass, reports_aki2120 pass, reports_steam1 pass | Escape closes it |
| Main map | F6 Orbital and Space Status | covered | reports pass, reports_aki2120 pass, reports_steam1 pass | Escape closes it |
| Main map | F7 Military Command Nexus | covered | reports pass, reports_aki2120 pass, reports_steam1 pass | Escape closes it |
| Main map | F8 Alpha Centauri Score | covered | reports pass, reports_aki2120 pass, reports_steam1 pass | Escape closes it |
| Main map | F9 View Monuments | covered | reports pass, reports_aki2120 pass, reports_steam1 pass | Escape closes it |
| Main map | F10 View Hall of Fame | covered | reports pass, reports_aki2120 pass, reports_steam1 pass | Escape closes it |
| Main map | F11 Switch To Detailed/Simple Menus | covered | reports pass, reports_aki2120 pass, reports_steam1 pass | opens the MENUSIMPLE popup (the dump's PopupLabel); Escape closes it |
| Main map | F12 Communications and Protocol | covered | reports pass, reports_aki2120 pass, reports_steam1 pass | a faction and Council panel on the map that doesn't take the keys; F12 again hides it |
| Main map | Ctrl+S Save Game | covered | save_game pass | saved as _cp_save in the test folder's saves: the dialog's file name field, the name, OK; the saved file is compared byte by byte (tools/clickpass/file_masks.tsv) |
| Main map | TURN COMPLETE | covered | end_turn pass, end_turn_aki2120 pass, end_turn_steam1 fail | the Operations Director's question, then the popups and base screens of the other factions' moves until the turn moves on |
