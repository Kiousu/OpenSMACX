# Regression harness for "Complete - testing" functions

Upstream redirected only functions marked `Status: Complete`. The 43 marked `Complete - testing`
and the 4 marked `WIP` are compiled into the DLL but the exe still runs its own code for them.
`src/regress.cpp` compares a decompiled function with the original, in-process, on the live game
state, so they can be promoted (or fixed) with evidence.

## How to run

1. Build the DLL and patch an exe as usual; the harness is always compiled in and inert.
2. Start the game and load a save. Late saves give the widest coverage (hundreds of bases,
   >1000 units, all factions in contact).
3. Create `opensmacx_regress.txt` in the game directory. Empty = all tests; otherwise list test
   names separated by whitespace.
4. A watcher thread notices the file within a second; the suite runs on the main thread at the
   next `veh_at()` call (any map interaction triggers one), writes `opensmacx_regress.log`
   beside the trigger and deletes the trigger. A full run on a Mission Year 2389 save takes about
   20 seconds.

Each test calls the original twice and the decompiled version twice, in that order, and reports:
`compared`, `mismatches` (orig vs new, first 25 with inputs and outputs), and `unstable orig`
/ `unstable new` (a side that disagrees with itself on a back-to-back call). Pairs where the
original is unstable are excluded from the mismatch count.

The originals are reached through the `*_OG` pointers in `src/temp.cpp`, the same hooks upstream
used for its own (unpublished) regression tests.

## Results, 2026-10-14, save "Council.SAV" (M.Y. 2389, 281 bases, 1741 vehs), 507 redirects

| function | compared | mismatches | orig unstable | verdict |
|---|---|---|---|---|
| value_of_base | 82,614 | 0 | 0 | matches; candidate for promotion |
| world_validate | 1 | 0 | 0 | matches on this map; needs more maps |
| battle_compute | 7,056 | 5,148 | 0 | decompile bug, see below |
| best_defender | 229,682 | 40,131 | 0 | calls the DLL's battle_compute; retest after the fix |
| wants_to_attack | 378 | 9 | 16 | original is not self-consistent; the 9 are all faction 1 vs 7, new=0 orig=1 |

### battle_compute

Every artillery/bombard case (`combat_type & 1`) returns the defaults (offense 8, defense 8,
no modifiers) from the decompiled version. Cause: in the original the block guarded by
`!is_bombard || spore launcher involved` closes before `get_basic_offense()` is called
(Ghidra decompile of 0x501DA0, the block ends at the `}` before the call); in
`src/veh.cpp` that `if` wraps the whole offense/defense computation, so bombard attacks skip it.
Type 0 and type 2 cases also differ in the defense value for about a third of pairs (e.g.
new def=0 vs orig 168; new 576 vs orig 1350), which is a second, separate problem to chase
after the nesting is fixed.

Interleaving the two versions also makes the original's next result change (first run showed
746 "unstable" original pairs when new ran between two original calls; 0 when the originals run
back to back). So the decompiled version leaves some battle global in a different state than the
original does. Worth finding, since battle_compute is called from inside best_defender.

### wants_to_attack

16 of 378 inputs give a different answer when the original is called twice in a row with no
intervening game activity, so it depends on something it (or a callee) mutates, or on
uninitialized stack. Resolve that before reading anything into the 9 mismatches.

## Not covered yet

- base_mark, social_ai, enemy_capabilities, base_psych, Path::*: side effects on game state;
  need a snapshot/restore of the affected structures around each call, or a save-reload loop.
- The world_* map generator chain: generate the same seed twice (once through the DLL, once
  through the original) and diff the map files. Different mechanism from the in-process harness.
- do_non_input / do_draw / do_keyboard: event-loop functions; need a different approach.
