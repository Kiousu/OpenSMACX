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
   names separated by whitespace. `world` selects every world generator; `path_sensors` selects
   `Path::sensors`.
4. A watcher thread notices the file within a second; the suite runs on the main thread at the
   next `veh_at()` call (any map interaction triggers one), writes `opensmacx_regress.log`
   beside the trigger and deletes the trigger. A full run on a Mission Year 2389 save takes about
   20 seconds; everything except the combat tests runs in about a second.

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

## State-mutating functions (snapshot / restore)

For functions that change game state the harness snapshots `PlayersData[8]`, `Bases[]`, `Vehs[]`,
the whole `MapTiles` array, the landmark and continent tables, the `Path` object and its map
table, and a few globals (AmbienceFlags, the draw-state bitfield, TradeMapsRevealedNew, the brush
and Mount Planet counters, GameState, the base/veh counts), then for each input: restore, run the
original, snapshot; restore, run the decompiled version, snapshot; compare the two snapshots byte
for byte and report the first differing structure, index, field offset and the two byte values.
"changed state" counts the inputs where the original actually altered something, so a
zero-mismatch result is known not to be vacuous. Sound cues and window redraws triggered by the
player-involved cases are allowed to happen.

Results on the same save (functions decompiled on this branch):

| function | compared | changed state | mismatches |
|---|---|---|---|
| treaty_on | 1,680 (42 pairs x 40 flag sets) | 1,470 | 0 |
| treaty_off | 1,680 | 448 | 0 |
| agenda_on | 1,680 | 1,634 | 0 |
| agenda_off | 1,680 | 122 | 0 |
| trade_maps | 42 pairs | 42 | 0 |
| do_withdrawal | 42 pairs | 14 pairs moved units, 36 moves | 0 |

## Upstream's held-back functions with side effects

Same snapshot method. Results after the fixes listed further down (the "before" column is the
first run of each test against the upstream code):

| function | inputs | before | after | notes |
|---|---|---|---|---|
| base_mark | every base (281) | 0 | 0 | matched as written; candidate for promotion |
| base_psych | every base x upkeep stage 0/1 (562) | 296 | 296 | WIP upstream, see below |
| enemy_capabilities | each faction + all in sequence (8) | 7 | 0 | treaty check used the pass index as the faction |
| social_ai | 7 factions x (AI self + 16 priority sets + 10 "optimize effect" forms) (188) | crash, then 126 | 0 | eleven transcription errors, see below |
| Path::sensors | every 4th base of each faction (72) | 71 | 0 | rewritten from the original |

The one remaining `social_ai` difference seen once (faction 5, AI self, `unk_103`) did not
reproduce in two reruns; that path calls the exe's `consider_designs`, which is not reseeded.

### base_psych (WIP upstream)

With upkeep stage 0 the two versions differ in `Base.talent_total` / `drone_total` /
`super_drone_total` for 15 bases. With upkeep stage 1 the original also accumulates into
`PlayerData.unk_39[talent][police]` (the 8x9 table at +0x6B0 that `social_ai` later reads as the
talent weight) for every base, which the decompiled version does not do at all. The Ghidra
decompile is in `build/ghidra/out_held/004ea540_base_psych.c` (the store is near its end).

## World generators

The 29 `world_*` / `brush` / `paint_land` / `build_*` functions run through the same snapshot
harness with two environment rules:

- **Empty world.** The harness zeroes `*BaseCurrentCount` and `*VehCurrentCount` for the duration
  of these tests (`SnapHarness::empty_world()`; the arrays are untouched and the counts come back
  afterwards). On a live map the game's own `alt_set()` submerges bases, kills units and throws
  modal "tidal waves" popups, which both blocks the run and makes the comparison meaningless. The
  generators only ever run on an empty map anyway.
- **One random stream.** Code still in the exe calls the C runtime `rand()` inline, while the
  DLL's `rnd()` used the DLL's own runtime. Both are the same generator, but two instances of it
  diverge as soon as one side consumes a different number of draws. `rnd()` and the generators'
  inline `rand()` calls now go through the exe's `rand()` (`_rand` in `temp.cpp`), so the
  patched game draws exactly what the original would for the same seed. Every test reseeds both
  the C runtime and the game's `Random` object before each side runs.

Inputs: 24 tiles evenly spaced over the map plus three random-placement seeds for the landmark
builders; 16 tiles x altitudes 0..6 x both flags for `world_alt_set`; 16 tiles x altitudes 1..5
for `brush`; 8 tiles x 2 altitudes x 2 radii for `paint_land`; four sizes x two seeds for
`build_continent`; three altitudes x two seeds for `build_hills`; three seeds for the whole-map
passes.

| function | compared | before | after |
|---|---|---|---|
| world_alt_set | 224 | 223 | 0 |
| world_raise_alt / world_lower_alt | 27 each | 24 / 24 | 0 / 0 |
| world_crater, monsoon, sargasso, ruin, dune, diamond, fresh, temple, unity, fossil, canyon, mesa, ridge, geothermal | 27 each | 14..27 each | 0 |
| world_volcano | 54 | 54 | 0 |
| world_borehole | 27 | 27 | 1-2, varies per run (see below) |
| brush | 80 | 74 | 0 |
| paint_land | 32 | 32 | 0 |
| build_continent | 8 | 8 | 0 |
| build_hills | 6 | 6 | 0 |
| world_riverbeds, world_temperature, world_analysis, world_linearize_contours, world_landmarks | 3 each | 3 each | 0 |
| world_polar_caps | 3 | 0 | 0 |

`world_borehole` reads `timeGetTime()` to pick its layout; the two sides read it a few
milliseconds apart and occasionally straddle a timer tick. Pinning the clock during the compare
(exe IAT entry plus a DLL-side pointer) would close it; nothing in the transcription differs.

## Fixes made while testing (all verified by the runs above)

- `do_non_input` / `do_keyboard` (`temp.cpp`): the exe was built against an SDK where
  `WM_MOUSELAST` is 0x20A and `WM_KEYLAST` is 0x108; current SDKs define 0x20E and 0x109, so the
  DLL skipped mouse-wheel/X-button and processed `WM_UNICHAR` as a key. Verified by disassembling
  the exe (0x5FCA30..0x5FCD00) and the built DLL; the other four `do_*` wrappers match as written.
- `enemy_capabilities`: `has_treaty(i, j, ...)` used the pass counter as the faction.
- `social_ai`: category index written with the combination counter (stack overrun, crashed the
  game inside `social_calc`); 1024 combinations instead of 256; invalid combinations were still
  scored; `preq_tech` indexed `[model][category]`; anti-ideology needed only one of category/model
  to match; ideology bonus/penalty logic; `soc_ideology_effect` read from the wrong field; region
  plan counter inverted; economy weight precedence; police clamp missing `+2` and halving
  condition reversed; planet modifiers for the AI-self path; research/industry/upheaval/probe
  blocks nested inside the wrong `else`; probe loop included faction 0; unsigned arithmetic turned
  negative weights into huge positives (`unk_sum1`, `unk_count*`, `unk_val3`, the upheaval
  `/ -3`); best-combination index stored a stale sum.
- `Path::sensors`: candidate loop used the abstract-map bounds; mine/solar and
  monolith/condenser/borehole tests inverted; fungus rules compared against shore line instead of
  ocean shelf; inner-ring/any-ring flags and the own-base/uncovered-base penalties inverted; the
  table stored 0 instead of proximity + 1; treaty test used PACT instead of VENDETTA on the raw
  `zoc_veh` return.
- `world_alt_set`: rings indexed one too far out (`RadiusRange[i]..[i+1]` instead of
  `[i-1]..[i]`), and the outer rings must be pulled down to `ring + altitude`, not raised.
- `world_raise_alt` / `world_lower_alt`: post-increment passed the old altitude.
- `world_linearize_contours`: `memcpy_s(&AltNatural, ...)` overwrote the DLL's own pointer
  variables (the crash that ended the first world run); ternary precedence; normalised against
  the constant default table (0x68FAF4), not the live one; signed divide.
- `alt_natural`: signed compare against the sea level (returns 0 below the first step).
- Eleven landmark builders passed `LM_x` ids to `bit2_set` instead of `BIT2_x` masks.
- `world_ruin` (two `y` instead of `y_radius`, landmark at y+2), `world_volcano` (landmark at
  y+2), `world_ridge` (table entry 455 -> 45, 13 entries, raise-all-then-mark), `world_geothermal`
  (three lines copied from `world_mesa`), `brush` (shore-line check inverted, loop one short),
  `paint_land` (random walk restarted from the origin each step), `build_continent` (altitude
  compared in the wrong units, retry off by one, pointer comparison), `build_hills` (no retry on a
  rejected location), `world_riverbeds` (placed a riverbed after abandoning the candidate),
  `world_temperature` (bore loop 8 instead of 9 tiles, unsigned orbit term), `world_analysis`
  (byte offset 0x20 read as index 32, thresholds).

## Verified by disassembly only

`do_non_input`, `do_all_non_input`, `do_draw`, `do_all_draws`, `do_keyboard`,
`do_all_keyboard`: message-pump wrappers; compared instruction by instruction against the exe
(see the fix above).

## Not covered

- `Path::find` and `Path::move` are stubs upstream (`return 0`), nothing to test yet.
- `base_psych` needs the missing table accumulation before a test can pass.
- `battle_compute` / `best_defender` / `wants_to_attack`: see the first results table.
