# Click pass report: 2026-10-18-0954

Pass folder `C:\Programming\OpenSMACX\build\clickpass\2026-10-18-0954`, started 2026-10-18T09:54:00. Reference game `G:\SMACX-opensmacx-test-clickref`, OpenSMACX game `G:\SMACX-opensmacx-test-clickosx`. The reference is the unmodified exe with only the approved deviations: hex_cost, battle_compute, best_defender, wants_to_attack, game_data, drone_riot.

## Summary

10 cases: 10 pass.

| case | save | status | step | time | reference |
|---|---|---|---|---|---|
| end_turn | Council.SAV | pass |  | 86.8 s | recorded |
| end_turn_aki2120 | sw_aki2120.SAV | pass |  | 37.3 s | recorded |
| end_turn_steam1 | sw_steam1.SAV | pass |  | 36.9 s | recorded |
| idle | Council.SAV | pass |  | 21.9 s | recorded |
| idle_aki2120 | sw_aki2120.SAV | pass |  | 21.8 s | recorded |
| idle_steam1 | sw_steam1.SAV | pass |  | 21.9 s | recorded |
| reports | Council.SAV | pass |  | 73.2 s | recorded |
| reports_aki2120 | sw_aki2120.SAV | pass |  | 72.3 s | recorded |
| reports_steam1 | sw_steam1.SAV | pass |  | 70.9 s | recorded |
| save_game | Council.SAV | pass |  | 40.5 s | recorded |

## Failures

None.

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
| Main map | TURN COMPLETE | covered | end_turn pass, end_turn_aki2120 pass, end_turn_steam1 pass | the Operations Director's question, then the popups and base screens of the other factions' moves until the turn moves on |
