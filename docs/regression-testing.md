# Regression harness for "Complete - testing" functions

Upstream redirected only functions marked `Status: Complete`. The 43 marked `Complete - testing`
and the 4 marked `WIP` are compiled into the DLL but the exe still runs its own code for them.
`src/regress.cpp` compares a decompiled function with the original, in-process, on the live game
state, so they can be promoted (or fixed) with evidence.

## How to run

1. Build the DLL with `/DOPENSMACX_REGRESS` (the harness, its watcher thread and the `veh_at()`
   hook are compiled out otherwise) and patch an exe as usual. To test a function that is
   already redirected, patch with a table that omits it (`opensmacx_patch.py -t <table>`).
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
| battle_compute | 7,056 | 5,148, then 0 | 0 | 20 transcription errors, see below |
| best_defender | 229,682 | 40,131, then 0 | 0 | 5 errors of its own plus battle_compute's |
| wants_to_attack | 378 | 9, then 0 | 16, then 0 | 8 errors; the instability was an uninitialised stack buffer |

The "then" columns are the 2026-10-16 rerun after the fixes
(`docs/regression-runs/2026-10-16-council-full.log`), with every unit pair of the save, the
attacker -1 case included.

### battle_compute

Re-transcribed line by line against the Ghidra decompile of 0x501DA0. The bombard block
(`!is_bombard || spore launcher involved`) wrapped the whole computation in the C++, so every
artillery/missile case returned 8/8; the original closes it after the PSI display modifiers.
About twenty percentage multipliers were written as `x *= (rule + 100) / 100` (truncating the
factor to 1, or 0 for penalties) where the original computes `(rule + 100) * x / 100`: Empath,
airdrop, alien offense/defense, altitude, mobile, rough, road, uphill, downhill, psi base,
sensor, trance, air-to-air, ground strike, non-combat, jammer, AAA. Further differences:
Empath requires a PSI defender; airdrop uses `combat_pen_pct_atk_airdrop` as a penalty; alien
modifiers display `val - 100`; info-warfare x4 checks the attacker's plan; artillery altitude
displays `diff * rule`; mobile-in-open uses `<=`; rough-vs-mobile has no rockiness test;
downhill displays the elevation label; the base block starts `fac_modifier` at 2, checks the
Maritime/Cloudbase projects against the base owner, applies Tachyon outside the attacker test
and the blink displacer outside the base block; "Infantry vs Base" and "Open Ground" sit outside
the base block (the latter was unreachable) with the original's conditions; `defense *=
def_multi` applies to a bunker without a base; with no attacker the function ends after the
resonance check; the attacker/defender air-superiority branches were swapped; "In Port"
requires a non-sea attacker; the Pulse armor `||`/`&&` precedence applied Pulse-3 to every
unit; speed vs `move_rate_roads` compares signed.

Two more things the harness needed: the terrain label that `defense_value()` sets and
battle_compute appends is the exe global at 0x90F550 (`VehBattleDisplayTerrain` is now a
pointer to it; the DLL had a private copy, so the exe side showed stale text); and the
original reads `Vehs[veh_id_atk]` without a bounds check. `best_defender()` calls it with
attacker -1, which reads the Reactor table just below the Vehs array (the prototype id lands on
the high half of a string pointer, a small valid index), so the C++ now does the same instead
of defaulting the attacker fields.

### best_defender

Five errors of its own: the stack filter `triad != LAND || (!ocean && base >= 0)` is
`triad != LAND || !ocean || base >= 0` in the original; the artillery flag tested
`veh_id_atk || can_arty(atk)` (true for any attacker but 0) instead of `can_arty(atk proto)`;
the air-combat flag tested the attacker triad `!= AIR` instead of `== AIR`; the added bounds
checks on the attacker hid the original's `Vehs[-1]` read; and `proto_pwr_def` was unsigned, so
the `/ power ... / 8` chain divided unsigned where the original uses `idiv`.

### wants_to_attack

Superseded on 2026-10-17 (see "wants_to_attack stack leftovers" below): the buffer is 7 slots,
and a trace of the exe found large leftovers in most of them on the game's own call paths.
The original never initialises a 64-entry buffer on its stack; on the game's own call paths the
slots hold small leftovers and it behaves as if zero-filled, but between back-to-back harness
calls the DLL leaves return addresses there, which is what made 16 of 378 inputs unstable. The
harness now zeroes 2 KB of stack before each original call (`scrub_stack()`), and the C++
zero-initialises the buffer (with signed compares). Eight transcription errors: the atrocity
test returned true for "both have atrocities" instead of "target has, we have none"; the peace
test accepted either of SURRENDERED/PACT instead of both; the base threshold was
`(top / 4) * 3` instead of `(top * 3) / 4`; `cmbt + bases_f ? x : 0` parsed as
`(cmbt + bases_f) ? x : 0`; the best-weapon value was read from the target instead of this
faction; the pact bonus and the Intense Rivalry penalty were inverted; and the score and factors
were unsigned.

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
| base_psych | every base x upkeep stage 0/1 (562) | 296 | 0 | retranscribed, see below |
| enemy_capabilities | each faction + all in sequence (8) | 7 | 0 | treaty check used the pass index as the faction |
| social_ai | 7 factions x (AI self + 16 priority sets + 10 "optimize effect" forms) (188) | crash, then 126 | 0 | eleven transcription errors, see below |
| Path::sensors | every 4th base of each faction (72) | 71 | 0 | rewritten from the original |

The one remaining `social_ai` difference seen once (faction 5, AI self, `unk_103`) did not
reproduce in two reruns; that path calls the exe's `consider_designs`, which is not reseeded.

### base_psych (was WIP upstream)

The upstream version stopped after the SE talent/police adjustment, so it never produced the
real totals and never filled `PlayerData.unk_39[talent][police]` (the 8x9 table at +0x6B0 that
`social_ai` reads as the talent weight). Retranscribed from
`build/ghidra/out_held/004ea540_base_psych.c`: five stages (SE talent + bureaucracy, psych
allocation, facilities, police/pacifism, secret projects/punishment sphere/nerve staple), each
clamped to the population, fitted to the non-specialist citizens (talents cancel super drones
then drones; with no talents left drones are shed) and recorded in the three per-stage arrays
at 0x90E984/0x90E920/0x90E94C (now `BaseCurrentTalentsStage` etc.). With upkeep stage 1 the
whole talent (-3..4) x police (-5..3) grid is evaluated and `(talents - drones)` (halved when
positive unless the base is drone-free and more than half talents; minus the population when
negative, doubled below size 6) is added to the table cell. Mistakes found while matching the
original: the -2 drones facility is the Recreation Commons (id 6), not the Children's Creche;
the assimilation drones are `range((pop + diff - 2) / 4, 0, (turns + 9) / 10)` (arguments were
swapped); the bureaucracy formula must be signed (`current_num_bases` is unsigned, the
subtraction wraps and the result clamps to the population); stage 0 clamps the drones but not
the talents before the fit. Police units are counted in two passes (police-2x and, for
`RFLAG_WORMPOLICE` factions, native land units first), the Self-Aware Colony counts as one unit,
and under police -5 the pacifism drones double. Clinical Immortality gives two talents in the
original (the manual says one), kept as is.

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
| world_borehole | 27 | 27 | 0 with the clock pinned (see below) |
| brush | 80 | 74 | 0 |
| paint_land | 32 | 32 | 0 |
| build_continent | 8 | 8 | 0 |
| build_hills | 6 | 6 | 0 |
| world_riverbeds, world_temperature, world_analysis, world_linearize_contours, world_landmarks | 3 each | 3 each | 0 |
| world_polar_caps | 3 | 0 | 0 (2 on 160-wide maps until the sweep fix, see below) |

`world_borehole` reads `timeGetTime()` to pick its layout; the two sides read it a few
milliseconds apart and occasionally straddled a timer tick (1-2 mismatches per run). The harness
now pins the clock for that test: `TimeGetTimePin` in regress.cpp walks the import tables of the
exe and of OpenSMACX.dll, points both `winmm.dll!timeGetTime` slots at a stub, and each sample
sets the stub's value from its seed so the four layout branches are all exercised. 27/27 match.

## Save sweep (2026-10-16)

One save is not a standard. `tools/mp/regress_sweep.ps1` (built on the same ui.ps1 driver as the multiplayer scripts) loads a list of
saves into one running game (Ctrl+L), triggers the suite on each and keeps every log under
`docs/regression-runs/sweep-2026-10-16/`. The exe for a sweep is patched from the 507-row table
(`build/redirects_507.tsv`), so every held-back function still runs on the exe side and is
compared, not just the ones promoted last. Thirteen saves from three games were swept: the
2026 game at turns 1, 48, 53, 71, 74, 75 and 272, the 2023 Aki Zeta-5 game at turns 20, 52, 113
and 147, and two 2024 Steam autosaves (turns 20 and 32); 128x128 and 160x160 maps, 5 to 300
bases, 51 to 1637 units. Result after the fixes below: every test 0 mismatches on every save,
no crashes; the one exception is the known timing-dependent social_ai case on Bugged.SAV.

What the sweep found that Council.SAV had hidden:

- `world_polar_caps` placed the bottom-row detail at `x - 1` instead of `x + 1`, and its loop
  counter was a byte. Invisible on a 128-wide map, two tiles off on 160-wide ones.
- `world_landmarks` in the C++ placed a Nessus Canyon on random maps, an upstream addition the
  original does not make. Removed to match the original (a comment marks the spot; it is a
  one-line change to put back as an option). The borehole inside it also needed the clock pin.
- `best_defender(def, -1, ...)` crashed inside `has_abil()` on 8 of 13 saves, in the original's
  own code path: with no attacker the exe reads `Vehs[-1]`, which is the Reactor table, and
  takes the high half of a heap string pointer as the attacker's prototype id. On a late save
  that pointer happened to be low (and the test even matched); on most others it is not, the
  implied "faction" is in the thousands and the exe faults. The DLL now treats a negative
  attacker as no attacker in `battle_compute` and `best_defender` (the only intentional
  deviation from the original in this set; both functions say so in a comment), and the
  harness no longer compares attacker -1.

`tools/savescan.py` reads a .SAV offline and reports the states these tests depend on, so saves
can be chosen for coverage instead of guessed at: per faction the SE values, rules (talent,
drone, worm police), base counts with nerve staples, punishment spheres, brood pits and the
drone-related facilities, combat units outside their own territory (the pacifism source), and
who owns the psych-related secret projects. The save is a dump of the game's own structures;
the fixed offsets are the game globals block at 0x3xx (file = exe address - 0x9A6168), the
faction data at 0x6C4, the faction rules at 0x10D24, the map globals at 0x13A24 and the tiles
at 0x144C8; the prototype table is found by its first name, the units follow it by 0x7DB0, the
bases follow the units by 0x2BC and end 0x21E4 before the end of the file. `--table` gives one
line per save with coverage flags; the table for all 106 saves on this machine is kept next to
the sweep logs. It showed that no save has a nerve-stapled base or a Genejack Factory, and
that the pacifism count is never set outside base_upkeep, so `base_psych` now also runs with
synthetic inputs (pacifism 1..3 across the grid and stapled bases); 846 comparisons per save,
0 mismatches on Council, Bugged and Starcraft.

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

## Promotion (552 redirects)

`tools/redirects.tsv` now redirects value_of_base, world_validate, base_mark,
enemy_capabilities, social_ai, Path::sensors, the 29 world generators and the six `do_*`
wrappers (41 new rows, 548 in total), then base_psych (549), then battle_compute, best_defender
and wants_to_attack (552). Boot-tested with a Quick Start game (map generation,
founding a base, turns) and with the 2389 save (a turn, then the whole suite: 45 tests, no
crash). Promoting `do_non_input` exposed one more transcription error that the earlier
disassembly reading had missed: the three `PeekMessage` ranges are tried in turn and the first
one that has a message wins; the DLL required all three (`||` on the failure tests instead of
`&&`), so with the wrapper redirected most non-input messages were never dispatched.

Once a function is redirected its `*_OG` pointer lands on the `jmp` into the DLL, so the
harness then compares the DLL with itself. To re-test a promoted function, patch a build with a
table that omits it (`opensmacx_patch.py -t <table>`), e.g. the previous 507-row table from
`git show 851367d:tools/redirects.tsv`.

## Status lines and logs

Every redirected function's `Status:` line now reads `Complete (verified vs original: <log>)`,
naming the run under `docs/regression-runs/` that compared it with the exe. The 2026-10-15 full
run covers the held-back set and base_psych, the time-pinned run covers world_borehole, and the
2026-10-16 full run covers the combat trio and the message-pump wrappers. The combat and wrapper
rows of that run were produced with an exe patched from a table without the six `do_*` rows, so
the wrappers ran on the exe side; everything already redirected compared the DLL with itself.

## do_* message-pump wrappers (message-level test)

`do_non_input`, `do_all_non_input`, `do_draw`, `do_all_draws`, `do_keyboard`,
`do_all_keyboard` were first compared instruction by instruction (see the fix above). The
harness now also tests them at message level (`do_wrappers`): probe messages that the game's
window procedure ignores are posted to the main window on each range boundary the wrappers
filter on (0xFE, WM_KEYUP, 0x108, 0x109, 0x1F5, WM_MOUSEMOVE, WM_MOUSEWHEEL, WM_USER+0x123) in
fifteen orderings, one wrapper call is made, and the return value, the probes still queued and
`MsgStatus` are compared between the exe and the DLL; the draw pair is checked by invalidating
the window and seeing whether a WM_PAINT is left. Stray timer messages are removed right before
the call and a case is retried up to three times. 62 cases, 0 mismatches, 0 retries needed.

## Not covered

- `Path::find` and `Path::move` are stubs upstream (`return 0`), nothing to test yet.

## VEH helper batch, 2026-10-16 (Council.SAV, 552-redirect exe)

Eleven small functions decompiled from Ghidra output and checked against the originals before
being added to the redirect table: `veh_selectable`, `veh_unmoved`, `veh_ready`, `veh_find`,
`is_bunged`, `clear_bunglist`, `add_abil`, `unspot_stack`, `stack_kill`, `get_rating` and
`most_objectives`. Trigger group `veh` runs all nine tests (`stack_kill` goes through the original
`veh_kill()` on both sides and snapshots the whole game state around every call; it also saves the
five selection globals `veh_kill()` renumbers, which the snapshot does not cover).

| function | inputs | compared | mismatches |
|---|---|---|---|
| veh_selectable / veh_unmoved / veh_ready | every unit x every faction as current player | 13,936 | 0 |
| veh_find | 9 faction filters x 4 skip ids x map grid (step 6/7) | 15,048 | 0 |
| is_bunged | every list entry x (own values, 5 single-field changes, 2 wildcards) | 4,096 | 0 |
| clear_bunglist | every faction | 8 | 0 |
| add_abil | 7 factions x 29 abilities x empty/non-empty bitfield | 406 | 0 |
| unspot_stack | every unit | 1,742 | 0 |
| stack_kill | every 5th unit | 350 (1,105 units killed) | 0 |
| get_rating | 8 factions x difficulty 0-6 x Intense Rivalry on/off x values -2000..2000 | 34,496 | 0 |
| most_objectives | cooperative victory on/off x "all bases count" on/off x 4 pointer combos | 16 | 0 |

Log: `regression-runs/2026-10-16-council-veh-helpers.log`. Notes: the bunglist (0x94B558, 8 x 64
entries of `{ability_flags, chassis, weapon, armor, reactor}`) is only consulted for human players
(`FactionsStatus[0]`); the odd `< 1000` test Ghidra shows in `veh_unmoved`/`veh_ready` is the
compiler's rendering of `speed() - moves_expended > 0`.

## Batch 2, 2026-10-16 (Council.SAV, 563-redirect exe)

Seven more functions: `abil_cond`, `bung_proto` (the previously unnamed 0x57DF30 that writes the
bunglist), `retire_proto`, `spot_base`, `spot_stack`, `study_artifact` and `tech_secret`. Trigger
group `batch2` runs the six tests; `tech_secret` only builds a `#THESECRET0/1` popup and has none.
`retire_proto` goes through the exe's `kill()` on both sides with `VehBattleUnkTgl[0]` set so the
map redraw is skipped, and compares the bunglist and the prototype table on top of the snapshot.
`study_artifact` ends in the exe's `tech_achieved()`, which runs the whole AI tech/design chain
over state the snapshot does not hold (prototypes, bunglist, research choice), so the first run
showed divergence after the call; the test now replaces the entry point of `tech_achieved()` with
a ten byte stub that records the tech id and returns, for both sides, and compares that id.

| function | inputs | compared | mismatches |
|---|---|---|---|
| abil_cond | 29 abilities x 9 chassis x 26 weapons x 14 armors x SMACX on/off | 190,008 | 0 |
| bung_proto | 8 factions x every 3rd prototype, ring index varied | 1,368 | 0 |
| spot_base | every 3rd base x 8 factions | 752 | 0 |
| spot_stack | every 7th unit (and -1) x 8 factions | 2,000 | 0 |
| retire_proto | factions 1-7 x 64 basic prototypes + their active custom prototypes | 777 (3,111 units killed) | 0 |
| study_artifact | every 9th unit turned into an AI faction's artifact, with/without a linked base | 388 (301 techs awarded) | 0 |

Log: `regression-runs/2026-10-16-council-batch2.log`. Notes: `PlayerData+0x208C` is the
bunglist ring index (`bunglist_index`); `study_artifact` toggles the unnamed player flags 0x10
then 0x4; the tile flag `bit2 & 0x400000` plus `UnkBitfield1 |= 1` mark a tile the current
player sees for the first time; 0x9A6564 (`BaseIDUnk`) is compared against base ids in five
functions but never written by the exe. Boot test: the 570-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with the game alive.

## Batch 3, 2026-10-16 (Council.SAV, 570-redirect exe)

Seventeen functions across seven files: `base_at`, `upgrade_cost`, `do_upgrade`,
`upgrade_prototypes`, `upgrade_any_prototypes`, `base_claimed` (base.cpp), `site_at` (map.cpp),
`spying` (faction.cpp), `act_of_aggression` (probe.cpp), `council_get_vote`, `can_call_council`
(council.cpp), `pick_top_veh`, `can_convoy`, `promote`, `proto_sort`, `proto_sort_2` (veh.cpp) and
`yearmotize` (game.cpp). Trigger group `batch3` runs sixteen tests (`proto_sort` covers both sort
orders). The upgrade tests and `act_of_aggression` go through exe code on both sides
(`upgrade_prototype()`, `double_cross()`) and are limited to AI factions other than the one at
the controls, since that code redraws the map or talks to a human; `base_at` is also fed tiles
with a stray base bit (debug mode off, so both sides rebuild the base bits). The first run showed
296 `base_claimed` mismatches: the transcription had the value comparison the wrong way round
(the original claims an item when the governor pick's value is at or below the requested one).

| function | inputs | compared | mismatches |
|---|---|---|---|
| base_at | every tile plus a two-tile ring off the map; 24 stray-bit tiles | 17,448 | 0 |
| upgrade_cost | factions 1-7 x active old design x every 3rd active new design, with/without the Nano Factory | 24,116 | 0 |
| do_upgrade | AI factions x designs with units (plus every 8th without) x 4 targets | 998 (771 changed) | 0 |
| upgrade_prototypes | AI factions x active designs, with/without every other design marked obsolete | 838 (533 changed) | 0 |
| upgrade_any_prototypes | 7 factions; AI factions also with every built design flagged pending | 13 (12 changed) | 0 |
| base_claimed | synthetic picks x items -1..14 x values -1..10 x 16 skip masks | 3,072 | 0 |
| site_at | every tile, three values of the third argument | 8,192 (1,134 computed) | 0 |
| spying | 7 current factions x governor yes/no x 8 factions x infiltrator yes/no | 224 | 0 |
| act_of_aggression | AI pairs, with/without an existing treaty | 60 (30 broke an agreement) | 0 |
| council_get_vote | factions 1-7 x 11 proposals x 7 proposers x 3 pledge states | 1,617 | 0 |
| can_call_council | factions 1-7 x 64 combinations of sunspots, PBEM, pending session, rule, net, omniscient | 448 (60 allowed) | 0 |
| pick_top_veh | every unit (and -1) x 8 selection/boom/mode variants | 13,936 | 0 |
| can_convoy | every tile | 8,192 (2,129 blocked) | 0 |
| promote | every unit not owned by the human at the controls, as is and reset to green | 2,702 (1,664 promotions) | 0 |
| proto_sort / proto_sort_2 | factions 0-7 | 16 | 0 |
| yearmotize | 13 sample strings | 13 | 0 |

Log: `regression-runs/2026-10-16-council-batch3.log`. Notes: `PlayerData+0x400/+0x42C`
(`unk_29`/`unk_30`) are the council pledge arrays (`council_pledge_vote` /
`council_pledge_faction`); 0x90EA44/0x90EA54 are the four governor production picks
(`BaseGovPickID`/`BaseGovPickValue`) written by `base_reset()`; 0x945B40/0x945B44 are
`ProtoSortCount`/`ProtoSortList`; 0x93A94C (`IsLoggingActive`) is set by `net_end_of_turn()` when
the per-faction log file could be opened; 0x689F30 (`VehDrawBoomID`) is the unit `boom()` is
blowing up; 0x9A681C is `GameMoreRules` (bit 4 = no Planetary Council) and 0x9A6614 the
`GovernorFactionID`. The `has_abil()` mask 0x408418 in `upgrade_prototypes` is amphibious, drop
pod, antigrav struts, artillery and police 2x. Boot test: the 587-redirect exe loaded Council.SAV and advanced the
turn 289 -> 290 (38 s) with the game alive.

## Batch 4, 2026-10-16 (Council.SAV, 587-redirect exe): world generators

`world_rocky`, `world_fungus`, `world_shorelines` and `world_rainfall` (map.cpp), plus the
`WorldNoise` lattice the fungus pass samples (a stack object in the original, 0x5C1E20/0x5C1F20,
not redirected). Trigger group `batch4`; each generator runs from the loaded map with seeds 1-3
through the snapshot harness, `world_fungus` and `world_shorelines` in empty-world mode because
they call the exe's `veh_init()` (fungal towers) and `alt_set()`. `world_rainfall` runs on the
full map so its base_find branch (the two "rainfall changed near a base" globals, compared
separately) is exercised.

| function | runs | mismatches |
|---|---|---|
| world_rocky | 3 seeds, full map | 0 |
| world_fungus | 3 seeds, empty world | 0 |
| world_shorelines | 3 seeds, empty world | 0 |
| world_rainfall | 3 seeds, full map | 0 |

Log: `regression-runs/2026-10-16-council-batch4.log`. Notes: `Map.unk_1` is scratch for the
rainfall pass (air-flow flags); 0x9B22E0/0x9B22DC (`MapRainfallChangeBaseID`/`...FactionID`) are
written by `world_rainfall()`; 0x9B22E8/0x9B22EC (`WorldBuildUnk1`/`2`) are set for the whole of
`world_build()`; 0x6919EC (`TerrainBitsFungusClears`) is the improvement mask fungus removes.
The noise constructor's neighbour sum is dead code in the exe (the result is never stored).
Boot test: the 591-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s)
with the game alive, and a hosted solo game (tools/mp/mpsolo.ps1) generated a new world and
reached planetfall with the Pirates' starting unit on the map (screenshots tools/mp/wb591_*.png).

## Batch 5, 2026-10-16 (Council.SAV, 591-redirect exe): tech awards

`tech_advance` and `tech_research` (technology.cpp) and `steal_tech` (probe.cpp). Trigger group
`batch5`. All three end in the exe's `tech_achieved()`, so the tests install the ten byte
recording stub from the study_artifact test (now a small RAII helper) and compare the recorded
tech id, the snapshot and the monument table that `mon_tech_discovered()` writes
(0x94CA08-0x94F1F4). Inputs are limited to AI factions other than the one at the controls:
`tech_selection()`/`tech_pick()` open the research dialog for humans and the stolen-tech popup
goes to the victim at the controls.

| function | inputs | compared | mismatches |
|---|---|---|---|
| tech_advance | 6 AI factions x goal as loaded / none (tech_selection picks) / tech 5 | 18 (18 awarded) | 0 |
| tech_research | 6 AI factions x 5 labs amounts (0, 1, half cost, cost+5, 3x cost+7) x as loaded / secret advance pending / no goal | 90 (39 awards) | 0 |
| steal_tech | 6 AI thieves x 5 victims x steal/acquire | 60 (18 empty) | 0 |

Log: `regression-runs/2026-10-16-council-batch5.log`. Notes: the climactic-battle research bonus
checks the prerequisite techs of the Psi Gate and the Ascent to Transcendence (facilities 33
and 102) with the inlined body of `has_tech()`; `PlayerData.flags & 0x40` (`PFLAG_UNK_40`) means a
secret advance is owed; 0x945F40/44/48 are `SkipTechScreenA/B` and `NetUpkeepState` (Thinker's
names); `tech_advance` reads `BaseIDCurrentSelected` before `bases_reset()` and uses it after.
Boot test: the 594-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s,
every AI faction's research going through the new code) with the game alive.

## Batch 6, 2026-10-16 (Council.SAV, 594-redirect exe)

`num_objectives`, `bases_reset` (base.cpp), `battle_kill`, `battle_kill_stack`,
`battle_kill_chaff` (veh.cpp), `clear_monuments`, `mon_tech_discovered` (game.cpp) and
`social_set` (faction.cpp). Trigger group `batch6`, five tests. `num_objectives` runs with six
synthetic objective units and three objective bases over 32 combinations of the scenario victory
rules and 8 of the victory state bits. The battle_kill tests go through the exe's `veh_kill()`
(`VehBattleUnkTgl[0]` set) with an attacker of each faction and none; the first run never reached
the artifact capture branch (Council.SAV has no artifacts left), so every third victim of
`battle_kill()` is turned into an Alien Artifact first. Monument tests compare the whole table;
`social_set` and `bases_reset` (which runs the exe's `base_reset()`) are limited to AI factions
other than the one at the controls.

| function | inputs | compared | mismatches |
|---|---|---|---|
| num_objectives | 8 factions x pact on/off x 32 rule combos x 8 state combos | 4,096 | 0 |
| battle_kill / _stack / _chaff | every 3rd unit x 9 attackers (every 3rd for the stack variants), synthetic artifacts | 8,128 (9,067 killed, 1,164 captured) | 0 |
| clear_monuments / mon_tech_discovered | loaded and cleared tables, other factions' flags varied | 37 | 0 |
| social_set | AI factions x pending choice as loaded / 3 changed categories | 24 | 0 |
| bases_reset | AI factions x all/one region x occupied-skip on/off | 24 | 0 |

Log: `regression-runs/2026-10-16-council-batch6.log`. Notes: `PlayerData+0x2084` (`unk_101`) counts
objective units destroyed (`objective_kills`); the monument table at 0x94CA08 is 8 x 0x4F4 bytes of
21 records of 0x38 (`Monument`/`MonumentRecord` in game.h; record 1 is the tech discovery);
`social_set` redraws the BaseWin (0x6A7628); `battle_kill` reads `Vehs[-1]` for an attacker of -1
like best_defender, and the C++ keeps that.
Boot test: the 602-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.

## Batch 7, 2026-10-16 (Council.SAV, 602-redirect exe): tile yields

`crop_yield`, `mine_yield`, `energy_yield` and `resource_yield` (base.cpp). Trigger group `batch7`,
one test. Each yield function runs for factions 1-7, with no base and with the faction's first
base, with the improvement flag off and on, over every other row of the map, comparing the
return value and the two side-effect globals (`BaseTerraformReduce`, the yield lost to the
three-per-square caps, and `BaseTerraformEnergy`, the base square energy). The dispatcher is
checked for all five types on a tile sample.

| function | compared | non-zero yields | mismatches |
|---|---|---|---|
| crop_yield | 106,496 | 91,234 | 0 |
| mine_yield | 106,496 | 69,045 | 0 |
| energy_yield | 106,496 | 79,470 | 0 |
| resource_yield | 2,090 | - | 0 |

Log: `regression-runs/2026-10-16-council-batch7.log`. Notes: the yield functions test facilities
with `bitmask()` on `Bases[base_id]` without checking the base id, so a base id of -1 reads the
memory before the array; the C++ does the same (`yield_fac`). 0x90E914/0x90E998 are
`BaseTerraformEnergy`/`BaseTerraformReduce` (Thinker's names), 0x9A6818 `SolarFlaresEvent` (energy
tripled) and 0x93A934 `ControlUpkeepA` selects which of its two bits applies. The Manifold
Harmonics owner gets the planet-rating bonuses on fungus and monoliths.
Boot test: the 606-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.

## Batch 8, 2026-10-16 (Council.SAV, 606-redirect exe)

`world_site` and `good_sensor` (map.cpp), `spot_loc` (veh.cpp) and `territory` (faction.cpp).
Trigger group `batch8`, four tests: the two pure ratings over every tile (`world_site` for land
and sea sites, `good_sensor` for every faction), `spot_loc` through the snapshot with the draw
flag compared, and `territory` for every faction pair, all four flag values, as loaded, with the
blocking treaty bits cleared, and with high friction and half the units pre-flagged.

| function | inputs | compared | mismatches |
|---|---|---|---|
| world_site | every tile x land/sea | 16,384 (12,229 usable) | 0 |
| good_sensor | every tile x factions 1-7 | 57,344 (277 good) | 0 |
| spot_loc | every 5th row, every 3rd tile x 8 factions | 4,576 | 0 |
| territory | 42 faction pairs x 4 flags x 3 variants | 504 (82 intrusions) | 0 |

Log: `regression-runs/2026-10-16-council-batch8.log`. Notes: `world_site` checks the
`BIT_UNK_40000000 | BIT_UNK_4000` bits of the last radius tile it visited rather than the center
(kept); `territory` sets `VSTATE_UNK_400` on every intruder, flags 1 and 2 of `Veh.flags`
(`VFLAG_UNK_1/2`) and `VSTATE_UNK_800` on repeat offenders, and `PFLAG_UNK_10000` on the
faction whose territory was entered; the shared-region test uses `region_total_bases` of
factions both sides are at war with.
Boot test: the 610-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.

## Batch 9, 2026-10-16 (Council.SAV, 610-redirect exe)

`suggest_plan` (base.cpp) and `scan_prototypes` (faction.cpp). Trigger group `batch9`, two pure
tests over every faction pair: `suggest_plan` as loaded, without a target, with the target on
the last base and with a standing attack target (return value, `DiploSecondFaction` and
`BaseFindDist` compared); `scan_prototypes` as loaded, under a pact and under vendetta (both
result globals compared).

| function | inputs | compared | mismatches |
|---|---|---|---|
| suggest_plan | 42 pairs x 4 variants | 168 (120 targets) | 0 |
| scan_prototypes | 42 pairs x 3 variants | 126 | 0 |

Log: `regression-runs/2026-10-16-council-batch9.log`. Notes: 0x93F7CC is `DiploSecondFaction`
(Thinker's diplo_second_faction), 0x93F804/0x93FA40 are `DiploProtoIDBest`/`DiploProtoIDTradeable`;
`scan_prototypes` rates armor with `arm_strat()` on the weapon ids (an original quirk, kept);
prototype flags 0x10/0x20 keep a design out of diplomacy offers.
Boot test: the 612-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.

## Batch 10, 2026-10-16 (Council.SAV, 612-redirect exe)

`set_course` (veh.cpp), `enemies_trade_tech` and `tech_analysis` (faction.cpp). Trigger group
`batch10`, three tests. `set_course` runs every 4th unit against five destinations through the
snapshot. `enemies_trade_tech` ends in up to two `tech_achieved()` calls, so the test replaces
that entry point with a five byte jump into a C++ recorder (`TechAchievedJmpStub`, which keeps
every call's arguments) and compares the call list and the snapshot for every faction pair, as
loaded and as pact partners. `tech_analysis` compares its fifteen result globals for every
faction pair at the loaded difficulty, 1 and 5, plus two synthetic passes with an unbuilt secret
project whose prerequisite the other faction holds and this one lacks, queued at one of the
other faction's bases.

| function | inputs | compared | mismatches |
|---|---|---|---|
| set_course | every 4th unit x 5 destinations x 3 move types | 2,180 (1,029 orders set) | 0 |
| enemies_trade_tech | 42 pairs x as loaded / pact | 84 (78 techs traded) | 0 |
| tech_analysis | 42 pairs x 5 variants | 210 (42 projects found) | 0 |

Log: `regression-runs/2026-10-16-council-batch10.log`. Notes: `enemies_trade_tech` looks up the
friction of the second faction's tech source in both directions (original quirk, kept) and
uses the top-ranked faction (`FactionRankings[7]`) for the great_satan test; `tech_analysis`
keeps its four give and two get candidates in fifteen scattered globals (named
`DiploTechGive*`/`DiploTechGet*`/`DiploTechProjectID` in faction.cpp, Thinker's names noted).
Boot test: the 615-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.

## Batch 11, 2026-10-16 (Council.SAV, 615-redirect exe)

`rankings` (base.cpp) and `stack_veh` (veh.cpp). Trigger group `batch11`, two snapshot tests.
`rankings` runs the scoring pass and the full ranking pass, as loaded, under intense rivalry
and with a sudden-death objective count and no achievement points (the objectives-only
score), comparing the eight scores, the ranking table, the top/bottom human ids, the current
turn's row of the might history and the snapshot (the betrayal rolls use the seeded exe
generator). `stack_veh` runs every transport and every 9th other unit in all three modes, as
loaded and with the stack mates sentried waiting for a carrier.

| function | inputs | compared | mismatches |
|---|---|---|---|
| rankings | score-only / full x 3 rule variants | 6 | 0 |
| stack_veh | 81 transports + every 9th unit x 3 modes x 2 variants | 1,602 | 0 |

Log: `regression-runs/2026-10-16-council-batch11.log`. Notes: 0x9A68AC is `FactionTurnMight`
([1000][8] int16, the per-turn score history); `rankings` leaves `FactionRankingsUnk` at -1 for
ranked factions and may assign rank 7 downward to faction 0 when fewer than seven factions are
scored (original behaviour, kept); `stack_veh` mode 0 only marks units with `VSTATE_UNK_1`,
modes 1+ lift and drop them and clear `VSTATE_UNK_10000`.
Boot test: the 617-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.

## Batch 12, 2026-10-16 (Council.SAV, 617-redirect exe)

`reset_territory` (map.cpp), `base_check_support` and `base_research` (base.cpp). Trigger group
`batch12`, three snapshot tests. `reset_territory` runs as loaded, with every tile unclaimed and
its redraw flag cleared, with every site rating unknown (so `world_site` rates them all), with
supply pods restored on every fourth tile at the hardest and easiest levels (the latter with one
faction's map revealed and another's objectives flag set) and with the territory distance doubled.
`base_research` runs every AI base as loaded and in five variants (enough labs for an advance, a
research penalty still running, the first turns on an easy level, a punishment sphere with a
top rating and a carried remainder, a scenario without research) with `tech_achieved` stubbed as
in batch 5. `base_check_support` runs every AI base with mineral shortfalls of 1, 4 and 12 and
with a mineral convoy parked in the base, and every base (humans included, where no popup can
run) in three riot variants under weak police, comparing the forces maintenance total and the
snapshot.

| function | inputs | compared | mismatches |
|---|---|---|---|
| reset_territory | 6 world variants (500 pods counted) | 6 | 0 |
| base_research | 246 AI bases x 6 variants (163 advances) | 1,476 | 0 |
| base_check_support | 281 bases x up to 7 variants (2,532 units disbanded, 66 convoys recalled) | 1,827 | 0 |

Log: `regression-runs/2026-10-16-council-batch12.log`. Notes: PlayerData 0x147C-0x18FC renamed
from `unk_78`..`unk_83` to the region tallies `reset_territory` fills (territory, visible, good
and good-in-radius tiles, best site rating, pods); the pod rule reuses `add_site` with
`AI_GOAL_SCOUT`. Original behaviour kept: an AI colony pod that loses support is charged off
the maintenance total twice before it is disbanded, and a unit with a clean reactor ends the
purge even when the shortfall remains. `base_research` keeps its fractional carry in the field
named `net_random_event` (PlayerData 0x3D8).
Boot test: the 620-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.

## Batch 13, 2026-10-16 (Council.SAV, 620-redirect exe)

`farm_compute` and `base_support` (base.cpp). Trigger group `batch13`, two snapshot tests that
also compare the current-base globals from 0x90E8F0 to 0x90EA60 byte for byte (convoy tallies,
support counts, tile flags, the used-tiles mask). `base_support` runs every base as loaded, in
the AI upkeep pass, at SUPPORT -4/police -3, SUPPORT +3/police -5, SUPPORT -2/police -3 with a
brood pit, with every convoy of the base working (some parked in it, plus another base's convoy)
and at SUPPORT +3 with size 7. `farm_compute` runs every base as loaded, in the AI upkeep pass
with the radius pods restored (so it adds goals), with half the radius unseen, with the map
revealed and no treaties, and with every nearby base working its whole radius.

| function | inputs | compared | mismatches |
|---|---|---|---|
| base_support | 281 bases x 7 variants (7,560 units supported, 1,106 pacifism drones) | 1,967 | 0 |
| farm_compute | 281 bases x 5 variants (every tile flag set at least 900 times) | 1,405 | 0 |

Log: `regression-runs/2026-10-16-council-batch13.log`. Notes: 0x90E99C is `BaseTileFlags`
([25]), 0x90EA00 `BaseCurrentUsedTiles`, 0x90EA08 `BaseCurrentForcesMaintCount`; PlayerData
`unk_38` renamed `support_cost_by_level` (support minerals at each SUPPORT level, summed in the
AI upkeep pass with a penalty that grows as costs approach the base's mineral intake) and
`unk_48` renamed `armed_unit_weight`. Original behaviour kept: `farm_compute` tests the tile's
territory after the stack walk has run the unit index to -1, so any foreign territory is
flagged whatever the treaty; convoy tallies index by the unit's resource type unchecked.
Boot test: the 622-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.

## Batch 14, 2026-10-16 (Council.SAV, 622-redirect exe)

`base_yield` and `base_energy` (base.cpp). Trigger group `batch14`, two snapshot tests that also
compare the current-base globals block 0x90E8F0..0x90EA60. `base_yield` runs every base as
loaded, governed with the first placement pass skipped (`ControlUpkeepA` set), at turn 150 and
size +3, with nothing worked, with five mixed specialists at size +2 (the psych-less ones get
dropped), with a conquer priority while building a secret project, and starving at size +6 with
orbital yields and an aerospace complex. `base_energy` runs every base as loaded, in the upkeep
pass with a global trade pact, this faction governing and energy convoys, with a labs-heavy
allocation at efficiency -4, with no allocation and four specialists of types 0-3, with every
multiplying facility and project in the base, on stockpile energy (energy grid owned on odd
bases) and under sanctions without treaties. The secret project table and proposal counts are
saved and restored around each run.

| function | inputs | compared | mismatches |
|---|---|---|---|
| base_yield | 281 bases x 7 variants (7,998 specialists, 1,084 worked-tile changes) | 1,967 | 0 |
| base_energy | 281 bases x 7 variants (10,024 commerce imported, 154 alien-base runs) | 1,967 | 0 |

Log: `regression-runs/2026-10-16-council-batch14.log`. Notes: 0x90E938 is
`BaseCurrentWorkersLeft`, 0x90E900 `BaseYieldNutrientMet`, 0x90EA0C/0x90E960 the commerce
import/export arrays (indexed by faction), 0x9A6618 `ProposalPassCount`; PlayerData `unk_13`
renamed `energy_surplus_total` (summed from the surplus before commerce, as the original does).
The alien energy grid as first promoted here started from zero; that was wrong and is
corrected in "Alien energy grid correction" below. Original behaviour kept: the specialist value divides by the energy intake plus the specialists already added, and
`base_yield` writes new specialists from slot 0 while counting leftovers from the old total.
Boot test: the 624-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.

## Batch 15, 2026-10-16 (Council.SAV, 624-redirect exe)

`base_growth` and `base_doctors` (base.cpp). Trigger group `batch15`, two snapshot tests that
also compare the current-base globals block and `BaseComputeID` (0x689374). `base_growth` runs
every AI base (the player at the controls would get popups) as loaded, starving with a nutrient
convoy parked in the base, starving once the game is over, with exactly enough nutrients, in a
pop boom, at the habitation limit with plenty stored, stagnating at growth -3, heading for
starvation (the advice branch, a no-op popup for AI bases), and in a cloning-vats boom at the
limit. Starving bases keep at least two citizens so no base dies. `base_doctors` runs every AI
base as loaded, with drones ahead of talents in a fed base, with three spare specialists in an
unfed base, rioting under weak police while about to grow, and with the Telepathic Matrix, plus
every human base under an active governor with the slider advice switched off.

| function | inputs | compared | mismatches |
|---|---|---|---|
| base_growth | 246 AI bases x 9 variants (367 grew, 248 starved) | 2,214 | 0 |
| base_doctors | 246 AI bases x 5 variants + 35 governed human bases | 1,265 | 0 |

Log: `regression-runs/2026-10-16-council-batch15.log`. Notes: 0x689374 is `BaseComputeID` (the
base whose figures the base_support..base_energy chain last refreshed), 0x939284
`MapWinFactionID`; `base_kill` (0x4E5250) and `eliminate_player` (0x5B3380) are exe pointers.
`base_growth` returns 1 only when the base starves to death (the original's fall-through path
also returns 0). Original behaviour kept: the slider advice in `base_doctors` skips bases
building `+8` (a unit design) rather than the network node `-8`; `base_doctors` picks its
specialists without the obsolescence check `base_yield` applies. Not promoted: `base_drones`,
whose Ghidra output didn't settle its types.
Boot test: the 626-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.

## Batch 16, 2026-10-16 (Council.SAV, 626-redirect exe)

Twenty-three small functions. faction.cpp: the leader and faction name getters (`get_name`,
`get_title`, `get_adjective_leader`, `get_adjective_insult_leader`, `get_adjective_faction`,
`get_adjective_insult_faction`, `get_insult_leader`), the pact words (`get_pact`, `get_pacts`,
`get_pacts_2`, `get_pact_hood`), the leader pronouns (`get_his_her`, `get_him_her`,
`get_he_she`) and the faction-file readers `get_phrase` and `get_noun_phrase`. general.cpp:
`mandate_color`, `in_box`, `in_box_2`, `clear` and `space`. base.cpp: `base_compute` (until now
an exe pointer) and `generators`. Trigger group `batch16`, four tests. The string getters are
compared on the returned pointer, its text and the parse gender and plurality they leave, for
every faction with leaders as loaded and with the raw female flag forced to 1 and 2; the
pronouns in all capitalization modes; the pair functions for every faction pair; the file
readers for the first four phrases of every faction in play. `base_compute` runs on every other
base, already computed or not, forced or not.

| test | compared | mismatches |
|---|---|---|
| faction_strings | 936 | 0 |
| small_helpers (mandate colors, 312 box points, clear/space buffers) | 324 | 0 |
| generators (every faction, with and without the inactive count, 3 variants) | 48 | 0 |
| base_compute | 564 | 0 |

Log: `regression-runs/2026-10-16-council-batch16.log`. Notes: 0x945838 is `PronounBuffer`,
0x8C6DE4 `MandateColors` (every other int), labels 199-210 are the gendered pact and pronoun
words. The getters store the leader's raw female flag as the parse gender, so a value other than
0 or 1 passes through.
Boot test: the 649-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (34 s) with
the game alive.

## Batch 17, 2026-10-16 (Council.SAV, 649-redirect exe)

`assemble_passengers` and `alien_base` (base.cpp). Trigger group `batch17`, two tests.
`assemble_passengers` runs every third base with five plans (offensive, defensive,
reconnaissance, naval transport, colonization) as loaded, with vendetta against everyone, a
colonization region flag and turn 40, and the same with a colony pod moved into the base and a
cautious faction, comparing the snapshot (the AI goals live in PlayerData). `alien_base` runs
every native unit and every 25th other unit from where it stands, from a tile across the map,
and as a sea lurk on an ocean tile, comparing the base picked and `BaseFindDist`.

| function | inputs | compared | mismatches |
|---|---|---|---|
| assemble_passengers | 94 bases x 5 plans x 3 variants | 1,410 | 0 |
| alien_base | 76 units x 3 positions (223 bases found) | 228 | 0 |

Log: `regression-runs/2026-10-16-council-batch17.log`. Notes: `contiguous` (0x53A780) is now an
exe pointer in temp.cpp. Original behaviour kept: `assemble_passengers` counts vendetta enemies
that have no bases in the region, or no contiguous link to it, against the growth limit;
`alien_base` reports its winning score (not a distance) in `BaseFindDist`.
Boot test: the 651-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.

## Batch 18, 2026-10-16 (Council.SAV, 651-redirect exe)

`repair_phase` and `allocate_energy` (game.cpp), two of the per-faction turn phases. Trigger
group `batch18`, two snapshot tests. `repair_phase` runs every faction as loaded and, with every
unit of the faction hurt, mostly rested, some sentried or on timed hold and some flagged for
cover, on the next four turns, with the Nano Factory, the Xenoempathy Dome and the three repair
projects owned. `allocate_energy` runs every faction but the one at the controls (whose branch
pops up advice) as loaded, with the current base rioting at police -3 and -2, calm with spare
specialists, researching Transcendent Thought with big reserves early on, with a defensive
region, with a wealth/power drive and offensive regions, and with the Telepathic Matrix;
it also compares the current-base globals, the selected base and `BaseComputeID`.

| function | inputs | compared | mismatches |
|---|---|---|---|
| repair_phase | 8 factions x 5 variants (6,201 units repaired) | 40 | 0 |
| allocate_energy | 6 factions x 8 variants (37 allocations changed) | 48 | 0 |

Log: `regression-runs/2026-10-16-council-batch18.log`. Notes: 0x93A958 is `EnergyCredits` (the
credits of the player at the controls as last shown); `energy_limit` (0x445440, always 10) and
`social_select` (0x5B5620) are exe pointers; Veh flag 0x1000 is `VFLAG_UNK_1000`. Original
behaviour kept: `allocate_energy` tests the current base, not each of the faction's bases, for
riots and spare specialists (Thinker's notes call this out too); `repair_phase` only lets a
repair unit help land units that can themselves carry cargo.
Boot test: the 653-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.

## Batch 19, 2026-10-16 (Council.SAV, 653-redirect exe)

`set_dirty`, `clear_dirty` and `world_UNK1` (map.cpp), `do_fungal_towers` (game.cpp). Trigger
group `batch19`, three tests. The redraw-flag pair runs from a map with half its tiles unflagged
and no refresh pending; `world_UNK1` runs on every tile, comparing the tile and the draw state;
`do_fungal_towers` runs as loaded and with every native unit plus every 40th other unit turned
into a fungal tower under three seeds of the exe's generator.

| function | inputs | compared | mismatches |
|---|---|---|---|
| set_dirty, clear_dirty | one run each | 2 | 0 |
| world_UNK1 | every tile | 8,192 | 0 |
| do_fungal_towers | 4 variants (152 towers) | 4 | 0 |

Log: `regression-runs/2026-10-16-council-batch19.log`. Notes: `world_UNK1` makes a tile one step
wetter (rainy stays rainy); `clear_dirty` sets the same per-tile redraw flag as `set_dirty`,
only without the whole-map refresh bit. `spot_all` (0x5B6260) is now an exe pointer.
Boot test: the 657-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.

## Batch 20, 2026-10-16 (Council.SAV, 657-redirect exe)

`base_drones` (base.cpp), transcribed from the disassembly because Ghidra could not settle its
types. Trigger group `batch20` (the filter also matches `batch2`, whose six tests re-ran with 0
mismatches). One snapshot test over every AI base (popups for human players) with the
current-base globals compared too, under a pinned `timeGetTime` and a seeded generator because
the riot path calls `drone_riot`: as loaded, a golden age starting, a golden age ending, riots
ending, riots breaking out, riots held down by a nerve staple, and riots under the Telepathic
Matrix.

| function | inputs | compared | mismatches |
|---|---|---|---|
| base_drones | 246 AI bases x 7 variants (255 riots, 246 golden ages after the original) | 1,722 | 0 |

Log: `regression-runs/2026-10-16-council-batch20.log`. Notes: `drone_riot` (0x4F4FB0) stays in
the exe as a pointer; its revolt branch only applies to human factions and reads the clock and
floating point, so it was not promoted. `base_drones` tail-calls it, so the DLL version returns
nothing either way.
Boot test: the 658-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.

## Batch 21, 2026-10-16 (Council.SAV, 658-redirect exe)

The thirteen remaining monument recorders (game.cpp): `mon_colony_founded`,
`mon_secrets_of_tech`, `mon_prototype_built`, `mon_facility_built`, `mon_secret_project`,
`mon_enemy_destroyed`, `mon_conquer_base`, `mon_naval_unit_built`, `mon_air_unit_built`,
`mon_native_life_bred`, `mon_first_in_space`, `mon_built_preserve` and `mon_killed_faction`.
Trigger group `batch21` (the filter also re-runs `batch2`, still 0 mismatches). One test calls
each for every faction except the one at the controls (which would get the monument screen), in
six table states (as loaded; every record clear; this faction's records clear while others hold
them, with and without counts; half of its records set with the secrets-of-tech years part
filled; all six killed-faction slots taken) and with two argument values (a short and an
over-long base name; enemy/project/faction 0 and 3), comparing the whole monument table.

| test | compared | mismatches |
|---|---|---|
| mon_records (13 functions x 7 factions x 6 states x 2 arguments; 977 changed the table) | 1,092 | 0 |

Log: `regression-runs/2026-10-16-council-batch21.log`. Notes: the monument layout is now mapped:
records 0-12 are colony founded, tech discovered, secrets of tech, prototype, facility, secret
project, enemy destroyed, base conquered, naval unit, air unit, native life bred, first in
space and nature preserve; 15-20 are killed-faction slots; after the records come
`record_latest`, four unknown ints and the three base names. The secrets-of-tech record keeps
its second and third years in `data` and `unk_1`. `mon_enemy_destroyed` makes the destroyed
faction the "first" when others hold the record without a count; `mon_killed_faction` records
aliens too. The batch 15 name `MapWinFactionID` duplicated `FactionIDCurrent` (both 0x939284)
and was folded into it.
Boot test: the 671-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (34 s) with
the game alive.

## Batch 22, 2026-10-16 (Council.SAV, 671-redirect exe)

`valid_patrol` (veh.cpp) and `energy_compute` (game.cpp). Trigger group `batch22` (the filter
also re-runs `batch2`, still 0 mismatches). `valid_patrol` runs every aircraft and every 4th
other unit against 40 targets: its own tile, 36 tiles out to 16 away, and three off the map.
`energy_compute` runs every faction as loaded with and without the base refresh, with loans
both ways and a sanction, with the Self-Aware Colony, with a research penalty and at war with
everyone, comparing the energy report block (0x7492DC-0x749C10), the snapshot, the current-base
globals, the selected base and `BaseComputeID`.

| function | inputs | compared | mismatches |
|---|---|---|---|
| valid_patrol | 518 units x 40 targets (8,648 valid) | 20,720 | 0 |
| energy_compute | 8 factions x 6 variants | 48 | 0 |

Log: `regression-runs/2026-10-16-council-batch22.log`. Notes: the energy report globals are
named (`EnergyReportMaint`, `EnergyReportBaseMaint[512]`, `EnergyReportLabs`,
`EnergyReportTechTurns`, `EnergyReportEconomy`, `EnergyReportFacilities[70]`,
`EnergyReportCommerce`, `EnergyReportNet`). Original behaviour kept: `valid_patrol` treats
`base_at() != 0` as "at a base" for aircraft, so only base 0's tile counts as open ground; Veh
offset 0x25 holds an aircraft's turns aloft.
Boot test: the 673-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (34 s) with
the game alive.

## Alien energy grid correction, 2026-10-16 (Council.SAV, 673-redirect exe)

Batch 14 started the alien energy grid in `base_energy` from zero and called that a deliberate
change, and its test gave alien factions treaty partners to avoid the case. Both were wrong:
aliens don't trade, so the no-partner case is the normal one, and the original's value there is
not random. `base_energy` keeps the commerce import in a local it never initializes (its frame
is `sub esp, 0x28` and the slot is first written inside the treaty branch), then adds the grid
(half of the facilities plus 5 per secret project) to it. All twelve call sites in the exe run
`base_minerals` immediately before `base_energy` at the same stack depth, and `base_minerals`
always leaves its eco-damage divisor in that slot (1, or 2 with a nanoreplicator or the
Singularity Inductor, plus one each for a Centauri Preserve, a Temple of Planet and the Pholus
Mutagen). So in the game an alien base's grid starts from that divisor unless some treaty
partner passed the commerce checks, whose zeroed import replaces it. The DLL now does exactly
that (`base_eco_divisor`, shared with `base_minerals`).

The `batch14` test no longer adds treaties; it adds a no-treaty variant and runs the exe's own
`base_minerals` + `base_energy` back to back (the harness restores their first six bytes in
place of the patcher's jumps) against the DLL pair, comparing the snapshot and the base globals.

| run | compared | alien-base runs | mismatches |
|---|---|---|---|
| zero start (the batch 14 version) | 2,248 | 176 | 108 |
| divisor start (now) | 2,248 | 176 | 0 |

Logs: `regression-runs/2026-10-16-council-alien-grid.log` and
`regression-runs/2026-10-16-council-alien-grid-zero-start.log` (the mismatches are alien bases'
energy intake one lower than vanilla). Before batch 14 the patched exe already differed here,
because the DLL's `base_minerals` leaves something else in that slot; this restores vanilla.
Boot test: the 673-redirect exe with the corrected DLL loaded Council.SAV and advanced the turn
289 -> 290 (38 s) with the game alive.

## Batch 23, 2026-10-16 (Council.SAV and the 14 sweep saves, 673-redirect exe)

`name_proto` and `propose_proto` (veh.cpp), the unit design naming and proposal used by the AI's
`consider_designs` and the design workshop. Trigger group `batch23`. `propose_proto` calls the
exe's `prune_protos` (0x57F4B0, now a temp.cpp pointer) on both sides.

`name_proto` runs every active prototype under its own parts for its owner (predefined ones for
every faction), then a grid in four slots per faction (an active design of each parity, an empty
slot, the Scout Patrol slot): every chassis, plausible weapon (payloads and planet busters only on
missiles), armor and reactor, once without abilities and once with an ability set from a rotating
list. The output strings are compared. `propose_proto` runs each AI faction (the player at the
controls is skipped: `prune_protos` opens popups for them) on every design it can build (same
parts, typed complete, an extra ability inside and outside the 0x44AE1B mask, each part one step
off) and on a sample of the part grid with rotating abilities and categories. Every ninth design
also runs nine variants: the design workshop's suggestion states (open, with the design's
category, 2, 3); the faction as a second human player (hotseat) who declined a design; Citizen
difficulty; no expansion; a given name; a name already taken. Compared: the return value, the
snapshot, `VehPrototypes`, `BungList`, the suggestion globals (0x93FC34..0x93FC58), the
new-design id (0x68F0F0) and the selection globals `kill()` touches.

| function | Council.SAV | all 14 sweep saves | mismatches |
|---|---|---|---|
| name_proto | 590,081 | 6,341,555 | 0 |
| propose_proto | 16,136 | 177,751 | 0 |

Coverage on Council.SAV: 3,309 names with a Mk number; 10,712 new designs (all into retired
slots), 2,251 reuses, 3,173 rejections or suggestions, 972 runs that requeued base production. The
early saves cover new designs in empty slots, and sw_aki2213's police design covers the police
name trim (168 names). Logs: `regression-runs/2026-10-16-council-batch23.log` and
`regression-runs/2026-10-16-sweep-batch23.log`.

Names: `DesignSuggestAbilities`/`Category`/`Weapon`/`Chassis`/`Reactor`/`Armor`/`State`
(0x93FC34..0x93FC54) and `ProtoIDNewDesign` (0x68F0F0) in veh.cpp. Original behaviour kept: for a
police unit with no unit noun, `name_proto` cuts the trailing space off the main name and then
clears the alternative name at the main name's new length, so the alternative loses two
characters; after a Mk renumbering restart it compares the alternative name against slot -1;
`propose_proto` copies a given name without a length check and compares the design category as an
unsigned byte; it also scans each base queue once without using the result (omitted).
Boot test: the 675-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.

## Pointer-for-value corrections, 2026-10-16 (Council.SAV, 675-redirect exe)

Five upstream redirects tested a global's pointer where the original reads its value, so the
check was always true or always false (found with a scan for pointer globals used as values):
`arm_strat` and `weap_strat` (`!ExpansionEnabled`): in SMAC mode the original rates the armors
past id 9 and the resonance weapons and string disruptor at 1, the DLL used their real ratings;
`goody_at` (`!MapRandSeed`): with a map seed of 0 the original finds nothing, the DLL still
scattered unity pods and supplies; `read_factions` (`ExpansionEnabled ?`) always read
NEWFACTIONS where the original reads FACTIONS in SMAC mode; `prefs_fac_load` (`if
(ExpansionEnabled)`) always loaded the SMACX faction preferences, the original only with the
expansion (the DLL's `read_factions` calls it unconditionally and relies on that check). All
five now read the value, as the original does at 0x57D273, 0x57D2E3, 0x5921A8, 0x586F83 and
0x59DBE9. `arm_strat` and `weap_strat` also do the psi ratio in signed arithmetic like the
original (same results for the non-negative values the game keeps), and `propose_proto` now calls
them where the original inlined them.

Trigger group `ptrfix`: `arm_strat`/`weap_strat` for every armor and weapon and faction with the
expansion on and off, and `goody_at` on every tile with the map's seed and with 0, each against
the exe's own code (OriginalEntry). With the old checks: 159 mismatches (56 SMAC-mode parts, 103
tiles with seed 0); with the fix: 0. `batch23` re-run on Council.SAV, sw_aki2213 and sw_steam1
(673 exe) with `propose_proto` on the shared functions: 0 mismatches. `read_factions` and
`prefs_fac_load` are checked against the disassembly only: launching with `-smac` leaves the
expansion flag set at the main menu for the vanilla exe too, so no SMAC-mode run was possible.

Logs: `regression-runs/2026-10-16-council-ptrfix.log` and
`regression-runs/2026-10-16-council-ptrfix-before.log`. Boot test: the 675-redirect exe with the
corrected DLL loaded Council.SAV and advanced the turn 289 -> 290 (34 s) with the game alive.

## Batch 24, 2026-10-16 (Council.SAV and the 14 sweep saves, 675-redirect exe)

`can_terraform` (map.cpp), the terraforming order the AI's formers and the governor (with the
base-work flag) pick for a tile. Trigger group `batch24`. It runs every tile for every faction
with and without the base-work flag and no base, and the 21 tiles around every base for its
owner with that base, comparing the order returned.

| function | Council.SAV | all 14 sweep saves | mismatches |
|---|---|---|---|
| can_terraform | 142,874 | 2,354,992 | 0 |

Every order the function can return comes up (Council.SAV: 20,345 farms, 2,058 mines, 845 solar
collectors, 3,068 forests, 12,690 fungus removals, 17,506 raised sea floors; sw_aki2213 has the
only fungus plantings, 6, and 255 bases with a need flag). Logs:
`regression-runs/2026-10-16-council-batch24.log` and `regression-runs/2026-10-16-sweep-batch24.log`.
Original behaviour kept: removing land fungus checks the sea fungus removal tech, as for sea
fungus; a farm without an enricher gets ORDER_FARM again (the order builds the enricher); the
results of one `bonus_at` and one `has_tech` call are unused. map.cpp now includes
terraforming.h. Boot test: the 676-redirect exe loaded Council.SAV and advanced the turn
289 -> 290 (38 s) with the game alive.

## Batch 25, 2026-10-16 (Council.SAV and the 14 sweep saves, 676-redirect exe)

`balance` (game.cpp), the start-of-game evening out of the factions' starting positions: each
start is rated by the distance to the nearest rival on its continent (or the continent's size),
the good tiles around it and the continent's size, and the factions below the best get an extra
colony pod (none for aquatic factions; the start unit is made independent instead) and unity and
supply pods on the ring two tiles out. Trigger group `batch25`: every difficulty level with four
random streams on each save, comparing the snapshot, the units it appends, the current-base
globals and `BaseComputeID`.

| function | runs | colony pods added | pod tiles set | mismatches |
|---|---|---|---|---|
| balance | 336 (14 saves x 24) | 1,220 | 6,868 | 0 |

The first run had 4 mismatches on sw_aki2152 at difficulty 3: the DLL's `is_human()` returns the
faction's raw status bit (2 for faction 1), so `5 - is_human(f)` gave 3 where the original's
normalized flag gives 4 and the human got a colony pod. `balance` now normalizes the flag; the
same slip in `tech_val` is fixed separately. The test also compares the units appended past the
snapshot's arrays. Logs: `regression-runs/2026-10-16-council-batch25.log` and
`regression-runs/2026-10-16-sweep-batch25.log`. Boot tests with the 677-redirect exe: Council.SAV
advanced the turn 289 -> 290 (38 s) with the game alive, and a Quick Start (Svensgaard, aquatic)
reached turn 1 with 29 units, the player's pods on the map and the start pod independent.
`tools/mp/mpsolo.ps1` (host-only network game) is no check here: the vanilla exe also ends with
no units and a garbage mission year.

## is_human() normalization, 2026-10-16 (Council.SAV, 677-redirect exe)

The DLL helper `is_human()` returned the faction's raw status bit (2 for faction 1, 4 for faction
2, ...) although its contract says true/false. Every use is boolean except arithmetic in
`tech_val` (an upstream redirect): it divides by `base_count * (is_human + 1)` where the original
normalizes the flag (0x5BD0B6: `neg; sbb; neg; inc`), so a human faction's tech values for its
defensive, offensive and contested regions came out too small. `is_human()` now returns 0 or 1.
Trigger group `ishuman`: `tech_val` against the exe's own code for every faction, every tech,
the factions' commlink values and active prototypes, both calculation modes, as loaded and with
every faction human (hotseat). Before: 353 mismatches of 4,676, all for human factions; after:
0 (sw_aki2152 also 0 of 3,464). `balance` keeps matching (it had the same slip, caught in batch
25). The full harness on Council.SAV with the 507-redirect sweep exe
(`regression-runs/2026-10-16-council-full-ishuman.log`) has 15 mismatches, none from this
change: `social_ai`'s known one (bunglist index, also in the earlier full logs) and 14 in
`tech_val`'s faction values (ids 89-96). Those call `wants_to_attack`, which in that exe is
still the original and reads a 64-entry stack buffer it never initializes; the DLL version
starts it zeroed (see the combat trio notes), so the two differ with whatever the stack
holds. With the 677-redirect exe both sides call the DLL's `wants_to_attack` and match. Logs:
`regression-runs/2026-10-16-council-ishuman.log` and
`regression-runs/2026-10-16-council-ishuman-before.log`.

## Batch 26, 2026-10-17 (Council.SAV and the 14 sweep saves, 677-redirect exe)

`upgrade_prototype` (base.cpp), which upgrades all of a faction's units of one prototype to
another for the upgrade cost: an AI pays when the cost, less a discount for difficulty and
standing, fits its reserves weighted by its priorities; the player at the controls is asked (or
told it can't afford it); other humans are left alone. The exe pointer of the same name in
temp.cpp is gone, so `upgrade_any_prototypes` now calls the DLL version. `NetDaemon_receive`
(0x530320) is a new temp.cpp pointer for the network path. Trigger group `batch26`: every pair of
an active prototype the faction has units of and another active prototype of its own with the
same triad. AI factions run quiet and not, and once as a hotseat human. The player at the
controls runs every third pair with 24 variants: each popup answer (0, 1, 2), asked before or
not, quiet or not, and the "can't afford" warning on or off. `popp` (0x48C0A0) is replaced
by a recorder for the test, and the label and picture it was asked for are compared along with
the return value, the snapshot, `VehPrototypes`, the popup text (`stringTemp`, the first two
parse strings, three parse numbers) and `VehUpgradeUnk`.

| function | Council.SAV | all 14 sweep saves | mismatches |
|---|---|---|---|
| upgrade_prototype | 17,202 | 41,826 | 0 |

Council.SAV: 8,364 upgrades by the original, 2,256 popups, 752 refusals after a popup. A first
run showed 2 mismatches that came from the test: it did not restore `VehPrototypes`, which
`do_upgrade` changes, so each run saw the previous run's prototypes. Logs:
`regression-runs/2026-10-16-council-batch26.log` and `regression-runs/2026-10-16-sweep-batch26.log`.
Original behaviour kept: the popup text lists the ability abbreviations with no separator.
Boot test: the 678-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.

## Batch 27, 2026-10-17 (Council.SAV and the 14 sweep saves, 678-redirect exe)

`veh_init` (veh.cpp), which creates every unit: it was a temp.cpp pointer into the exe and is now
`veh_init_OG` for the harness. It homes the unit to a base of its faction on the tile (an alien
artifact instead becomes an objective when artifacts are scenario objectives), places it and
reveals the tiles around it, then may assign a land combat unit or probe in the region of the
faction's target location to the target (`VSTATE_UNK_40000`, quota from the faction's bases in
the region, capped for a naval region plan) or leave a better attacker than defender out at
random (`VSTATE_UNK_20000`). `alien_start` (game.cpp), called by `config_game` when a SMACX game
is set up: if the player's faction is neither alien nor FUNGBOY, every computer alien or FUNGBOY
faction in a single player game is removed (bases and units killed, alive bit cleared, flags and
difficulty set to -1).

Trigger group `batch27` (filter with the names `veh_init alien_start`: the group name also matches
`batch2`). `veh_init`: every faction, every active basic and faction prototype, on up to four of
its bases, its target tile, six tiles of the target's region and six tiles across the map; as
loaded and with the target counters reset as the AI strategy pass leaves them, plus the scenario
artifact rule for alien artifacts and a full unit table. Compared: the return value, the
snapshot, the new unit, `BaseComputeID` and the position in the random stream. `spot_all` hands
first contacts to `encounter` (a diplomacy dialog with the player) and alien sightings to
`interlude`; both are stubbed with a recorder and the recorded calls are compared. Without the
stubs the first sweep stopped on sw_aki2152 with Cha Dawn's first contact open inside the test.
`alien_start`: each faction as the player at turn 0, and for the loaded player as loaded (turn >
0), hotseat/PBEM, network, no expansion and every faction human. `popp` is stubbed (`base_kill`
can report a foiled energy market corner); compared: the snapshot, the Secret Project table,
both status bytes, the replay record `base_kill` appends to, the current-base globals,
`stringTemp` and the selection globals `veh_kill` renumbers.

| function | Council.SAV | 14 sweep saves | mismatches |
|---|---|---|---|
| veh_init | 10,615 | 74,246 | 0 |
| alien_start | 12 | 168 | 0 |

By the original over the sweep saves: 31,268 units homed, 1,068 objectives, 1,531 assigned to
the target, 49 left out at random (sw_steam1/sw_steam20 only), 9,769 contact/interlude calls;
`alien_start` removed 1,532 bases. Logs:
`regression-runs/2026-10-17-council-batch27.log` and `regression-runs/2026-10-17-sweep-batch27.log`.
Original behaviour kept: the vendetta check compares the faction's own base count in the region,
so the first faction at vendetta is taken. Boot tests with the 680-redirect exe: Council.SAV
advanced the turn 289 -> 290 (38 s) with the game alive; a Quick Start (Svensgaard) reached
planetfall with FUNGBOY removed by `alien_start`, as the 678-redirect exe (exe code) also does.

## wants_to_attack stack leftovers, 2026-10-17 (Council.SAV and 13 sweep saves, 680-redirect exe)

Superseded on 2026-10-18: `wants_to_attack` now starts these counts from zero (see "wants_to_attack,
2026-10-18" below); this section describes the version it replaced.

`wants_to_attack` (0x55BC80) reads 7 slots on its stack (`[ebp-0x130]`, faction 1 first) that it
never writes before reading: each starts with whatever the stack holds there and is raised to the
faction's most bases in one region, and the thresholds that decide which regions count as a
faction's core use that value. A trace of the exe on Council.SAV's turn 289 found a leftover above
the real count in 132 of 187 calls (stack and return addresses such as 1,746,744), so in the game as
shipped those checks mostly apply only to the headquarters region. The DLL used to start the
buffer at zero; Kiousu chose to match the original instead. `wants_to_attack` is now a naked entry
that copies slot f from `esp_entry - 0x138 + 4 * f` (where the original's frame puts it) before
the DLL's own frame overwrites it, runs `wants_to_attack_body` on the copy and writes the values
back, where the original leaves them for the next call. The `* 3 / 4` thresholds wrap like the
original's `lea` before the signed divide, since a leftover can be any 32-bit value. The
leftovers stay exact while the code that ran before at that stack depth is the exe's; DLL
functions use the stack differently, so they leave different values behind.

Trigger name `wants_to_attack` (replaces the 2026-10-16 test and its stack scrub). `OriginalEntry`
puts the exe's first six bytes back so the original runs in the fully redirected exe;
`wta_seeded_call` writes the seven slots where the callee's frame will find them, calls the entry
and reads back what it left. Every attacker, target and third faction (-1..7), each with 18 seed
sets: zero, the real maximum, one above and one below it, the traced Council values, what the
previous call left (back-to-back calls), and four each of random values near the maximum, values
whose `* 3` wraps (0x7FFFFFFF, 0x80000000, 0x2AAAAAAB, -1, ...) and any 32-bit value. Compared: the
result and all seven slots afterwards.

| | Council.SAV | 13 sweep saves | mismatches |
|---|---|---|---|
| compared | 6,804 | 88,452 | 0 |
| a used slot above the real maximum | 4,458 | 57,670 | |
| original's result changed by the leftovers | 91 | 199 | |

The leftovers changed the original's answer on 5 of the 14 saves (Council.SAV 91, sw_bugged 70,
sw_aki2120 64, sw_aki2247 34, sw_aki2213 31) and never on the other nine; on sw_urfl (turn 1)
every call returns before the buffer. sw_council.SAV is no longer swept: it is the same game state as Council.SAV,
and loading it made the test game write a save into the live game folder (vanilla's save path then
copied stack junk into the game: year 2100, a junk Secret Project table, a crash in `rankings`).
Logs: `regression-runs/2026-10-17-council-wta-stack.log` and
`regression-runs/2026-10-17-sweep-wta-stack.log`. Boot test with the release DLL: Council.SAV
advanced the turn 289 -> 290 (34 s, the AI calling the new entry) with the game alive.

## Save header, 2026-10-17 (deliberate change from the v2.1 exe, chosen by Kiousu)

Saving from the game crashed on 2026-10-17 (in the old-save conversion loop at 0x5A7E82, after
writing an 83 MB file). The cause is in the exe's own `game_data` (0x5A6AC0), which saves and loads
the game state: it starts with a 0x31C-byte header buffer on its stack (`[ebp-0x374]`) that it
writes to the file in save mode without ever filling it, and then reads the buffer's version field
(+0x304, `[ebp-0x70]`). Below 11 it takes the game for an old save, copies the buffer over the game
state at 0x9A6490 (turn counter, Secret Projects, ...) and converts it: the game is corrupted and
usually crashes. Whether a save works therefore depends on what the code before `game_data` left
on the stack, in effect a hash of everything that ran before at that depth, Windows' own code
included.

Measured with the `save_slot` probe (hooks `game_data`'s entry, records the field and sets it to
11, then `save_daemon` 16 times per variant; `OriginalEntry` switches between the DLL's and the
exe's `env_open_old` and `header_write`, which run inside `save_daemon` before `game_data`):

| header_write | env_open_old | field on all 16 saves |
|---|---|---|
| DLL | DLL or exe | 6,582,575 (0x64712F, a return address in the exe's CRT), safe |
| exe | DLL or exe | -2, below 11 on every save |

So with the exe's own code on this machine (Windows 11) every save would take the old-save path;
the DLL's `header_write` happened to leave a safe value, which is why OpenSMACX saves mostly
worked, and every save an OpenSMACX build ever wrote has stack junk in its header (363 non-zero
bytes in that day's autosave). Kiousu's own game never crashed on save: it runs with Thinker
(`thinker.dll` holds the addresses of `game_data`, `save_daemon` and `header_write`; PRACX's DLLs
none of them), and every save in the live game folder has a header of zeros with version 12. The
exe's `game_data` is byte for byte the same in both exes, so the fix is Thinker's, made at run time.

Kiousu chose to match Kiousu's game. `game_data` (general.cpp) is now a naked entry redirected from
0x5A6AC0 (681 redirects): it zero-fills the buffer, sets the version to 12 and runs the exe's
`game_data` from its prologue. In load mode the buffer is read from the file right after, so only
saves change, and their header is Thinker's.

Trigger name `save_header` (only when named: it writes files in the test saves folder): a save
through the new entry, a save through the exe's own entry (with the probe, so it stays >= 11) and
a second save through the new entry as a control. The new header must be zeros with version 12 and
the rest of the file equal to the exe-entry save. Council.SAV, sw_steam1.SAV and `_header_check.SAV`
loaded back (the save the previous run wrote): header as Thinker's, rest of the file identical,
control identical. One earlier run had 1 byte different in the last 4 bytes of the file (the tail
of globals after the bases), not seen again in 4 runs with the control. From the game itself:
Council.SAV saved with Ctrl+S (header: one non-zero byte, the version) and loaded back with Ctrl+L
(year 2389, alive); boot test turn 289 -> 290 alive. Log: `regression-runs/2026-10-17-save-header.log`.

Test saves: every save copied from the live game stores the path it was saved under (file offset
0x561, game state 0x9A66C9), and after a load the game saves there by default, so saves from the
test folder landed in the live folder. The stored path of the 326 such saves in
`G:\SMACX-opensmacx-test\saves` now points to that folder (same name, same file size); the
originals are in `G:\SMACX-opensmacx-test\saves_backup_20261017`.

## Batch 28, 2026-10-17 (Council.SAV and the 13 sweep saves, 701-705-redirect exes)

Done in parallel: two agents worked in their own git worktrees (one on
the diplomacy notices in faction.cpp, one on the prototype design AI in veh.cpp) while the main
session did the third part; the main session merged both worktrees into the branch and ran every
game test, since only one game can use the test folder at a time. 705 redirects with all three
parts.

### Landmarks, map view caches, base founding, combat helpers (main session)

`fixup_landmarks` and `delete_landmark` (map.cpp) keep the five landmark corner height tables the
map view adds to crater, volcano, canyon, mesa and ridge tiles (`LandmarkAlt*`, 113 entries each,
picked through `LandmarkAltIndex[code][5]`; the draw code reads them at 0x46FF27).
`fixup_landmarks` (was a temp.cpp pointer, called after a map is read or built) copies the
defaults back and zeroes the entries of crater and volcano tiles whose code is negative (deleted).
`delete_landmark` (called by `alt_set`) zeroes the tile's entries, stores the code as -1 - code,
sets `UnkBitfield1` bit 4 and makes every map view in use drop its cached heights around the tile
(`MapWin_clear_alt`, exe). Kept from the original: it clears bit 0x80 (`BIT2_FRESH`) before
looking at the code, and ridges zero the mesa table (the view reads ridges from their own table).
`terrain_fixup` and `mapwin_terrain_fixup` (map.cpp; the latter was a temp.cpp pointer, called by
`map_init`) give each map view a fresh 12-byte-per-tile drawing cache (Thinker: paTilePos): bytes
0..8 set to 0xFF, byte 9 the bits 0xD0 of the first tile's climate byte for every entry (as in the
original), bytes 10..11 left as allocated. The buffer comes from the exe's allocator and the old
one goes back to the exe's `free`, as before. `mon_winning_unify` / `mon_winning_trans` (game.cpp)
fill monument records 13 and 14 (`MONUMENT_WINNING_UNIFY/TRANS`) like the other recorders.
`energy_limit` (game.cpp, was a temp.cpp pointer) returns 10 and ignores the faction.

`base_init` (base.cpp) founds a base: tile bits, owner and visibility, the base and region
counters, a replay record (`replay_base`, exe), then the base itself (size 3 with the Planetary
Transit System, else 1; the first production and starting minerals, from the turn 0 rules or the
retool exemption rule). A faction's first base gets its headquarters plus, in a network game, a
size of 3 or 4 by traits and some facilities, or in a single player game a size and facilities by
the turn (75/100/150); a sea base gets a pressure dome; faction bonuses and the techs that give a
facility in every base add theirs. Kept from the original: only the first 9 bytes of
`facilities_built` are cleared (the last 3 keep what an earlier base in the slot left). The base
figures are then worked out and it becomes the current base. `battle_fight` (veh.cpp) works out
the target tile of an attack from a direction (radius tables for artillery, x wraps, y does not)
and returns what `battle_fight_2` (exe) returns (the caller at 0x597262 tests it).
`compute_odds` (faction.cpp) is the AI's weighting of an attack: the odds times the defender
side's cost (the stack's outside a base) over the attacker's cost, raised for non-combat or
objective defenders, a base or the faction's goal tile, the attacker's own terrain, thin stacks,
supporting land units next to a base, air raids on terraformers or at sea and air drop units ready
to follow, lowered for air superiority units against armed surface units and for bombers. Kept
from the original: a neighbour tile without a unit reads `Vehs[-1]` (veh_at returns -1).

`name_base` (base.cpp, redirected upstream) did not match the exe: a test against the exe's own
code found 304 of 308 calls different. Upstream had added a shuffle to the sea base names (the
exe takes them in order), advanced the land-name counter before a name was found (the exe only
when one was taken, so not when a list can't be opened or runs out), closed the text file between
the two lists, and used `strncpy_s`, which does not zero-pad the 24-byte name as `strncpy` does
(the bytes after the name go into the save). It is now transcribed from the asm: 0 mismatches.

Trigger group `b28main`. `mon_winning`: every faction but the player at the controls, as loaded,
record clear, record held, alien flag flipped, winner -1 and 3; compared: the monument table.
`energy_limit`: factions -1..8. `fixup_landmarks`: as loaded and with every other / every relief
landmark tile marked deleted, the tables pre-filled with junk; compared: the five tables.
`delete_landmark`: every relief landmark tile and a sample of the others, as loaded and with the
landmark already deleted; compared: the tile, the five tables, `UnkBitfield1`, each view's
`unk_1` and the whole drawing cache of each view in use. `terrain_fixup`: six values of the first
tile's climate byte, with and without an old cache, plus a null window; compared: bytes 0..9 of
every entry. `mapwin_terrain_fixup`: the original with `terrain_fixup` also put back to the exe's
code; compared: which of the 11 views were rebuilt and bytes 0..9 of their caches. `base_init`:
factions 1..7 on six tiles each without a base (land and sea, own, others' and unclaimed), as
loaded, at turn 0, as the faction's first base (as loaded, in a network game, at turn 80, at turn
0) and with a full base table; compared: the return value, the snapshot, the new base slot (its
last 3 facility bytes pre-set), the replay record, the base globals block, the current-base
globals, `UnkBitfield1` and the position in the random stream. `name_base`: factions 1..7, eleven
name counters, land and sea, final or not; compared: the 32-byte buffer, both counters and the
parse gender/plural globals. `battle_fight` (`battle_fight_2` stubbed with a recorder): units near
the map edges and every 7th other, 9 directions and 25 artillery offsets, round and flat map;
compared: the arguments handed on and the return value. `compute_odds`: per faction up to four
attackers of each triad against every 17th enemy unit, base id -2/-1/the base on the tile, the
flag 0x80000 both ways, four odds values; compared: the return value.

### Diplomacy notices (worktree agent)

`make_treaty`, `pledge_truce`, `demands_withdrawal`, `renounce_pact`, `dont_withdrawal`,
`pact_attacks`, `pact_unpact`, `enemies_unpact`, `pact_truce`, `pact_treaty`, `enemies_team_up` and
`break_treaty` (faction.cpp): the treaty changes and the notices that go with them (parse slots and
`NetMsg_pop`, `popp` or `X_pop*` for the player at the controls). None builds a Popup object on the
stack. In the original the network branches of `make_treaty`, `pledge_truce` and `renounce_pact`
are exactly `net_treaty_on`, `net_treaty_off` and `net_pact_ends`, the lock loop in
`renounce_pact` is `diplo_lock(100)` and the inline check in `enemies_unpact` is `spying()`; the
DLL calls those. `dont_withdrawal` drops a CRT `abs()` call whose result the exe throws away; the
unit must stand in faction_id_with's territory. `break_treaty` returns true for "call the action
off". New exe pointers: `commlink_attempter`, `demand_withdrawal`, `diplomacy_check`,
`diplomacy_caption`, `tut_check2`, `X_pops_11`, `X_pop_2`, `FactionPortraits`; `DiploUnk1`
(0x93F7D0, only ever read); 0x9A6524 is the Empath Guild's base.

Trigger group `diplonote` (tests `pact_notices` for the four pact_* functions, `enemies_unpact`,
`enemies_team_up`, `break_treaty`, `make_treaty`, `pledge_truce`, `demands_withdrawal`,
`renounce_pact`, `dont_withdrawal`). Every ordered pair of factions 1..7 (triples for
`enemies_team_up`), plus variants the saves don't reach on their own: forced pacts, omniscient
view, a human target, vendetta and grudge bits, each treaty state against each mask with the
answers 0 and 1, the tutorial answer and difficulty, failed contact, demand answers 0..3 with and
without sunspots, units sent to a third faction's base. `NetMsg_pop`, `popp`, `X_pops_11`,
`X_pop_2`, `tut_check2`, `commlink_attempter`, `demand_withdrawal` and `pact_ends` are stubbed with
recorders. Compared: the return value, the full call sequence with labels, pictures and
arguments, parse slots 0..9 with gender and plurality and the parse defaults, `stringTemp`, the
pronoun buffer, `MultiWinFlag`, the diplomacy globals block 0x93F678..0x93FAE0, the random seed and
the snapshot. Not reachable in a single player test: the network branches. `diplomacy_caption`
runs unstubbed (it only redraws a visible diplomacy window).

### Prototype design AI (worktree agent)

`prune_protos` (0x57F4B0) and `consider_designs` (0x581260) in veh.cpp; the temp.cpp pointers are
now `prune_protos_OG` / `consider_designs_OG` and their callers (`propose_proto`, `social_ai`) call
the DLL. Kept from the original: in the human branch of `prune_protos` the range check uses the
kept design's chassis; its popup text reads the raw ability flags; the computer branch's first
loop covers only the 64 basic designs. In `consider_designs` the attacker block's armor limit
reads a stack slot that holds the armor loop's leftover counter (14 unless a combat design got a
new part earlier in the call), reproduced as `factor = MaxArmorNum`; the cost budget is
`ai_power + ai_growth + margin` (0/1 flags) as in the exe; one discarded `proto_cost` call is left
out. 0x9A6638/0x9A663C are the pass counts for repealing and reinstating the UN Charter. Neither
uses random numbers or the clock. As in the exe, a combat design with a 0-offense weapon would
divide by zero.

Trigger group `protodesign` (tests `prune_protos`, `consider_designs`). `prune_protos`: factions
0..7, kept designs as the AI and human branches use them, computer player as loaded and with the
obsolete marks and flags 8/0x200 cleared (synch on and off), human with the auto-prune preference,
human in a network game (lock refused on some runs), human with neither, the player at the
controls answering 0, 1 and 2; `popp`, `X_pop`, `NetDaemon::synch`, `lock_base` and `unlock_base`
recorded. `consider_designs`: factions 1..7 as loaded, as a design workshop suggestion, with the
obsolete marks cleared, human/computer swapped, and with the UN Charter repealed and intense
rivalry switched. Compared: `VehPrototypes`, the snapshot, the bunglist, the DesignSuggest block,
`ProtoIDNewDesign`, the selection globals, popup text and the recorded calls.

### Results

| test | Council.SAV | 13 sweep saves | mismatches |
|---|---|---|---|
| mon_winning | 112 | 1,456 | 0 |
| energy_limit | 10 | 130 | 0 |
| fixup_landmarks | 3 | 39 | 0 |
| delete_landmark | 879 | 13,367 | 0 |
| terrain_fixup | 13 | 169 | 0 |
| mapwin_terrain_fixup | 11 | 143 | 0 |
| base_init | 253 | 3,289 | 0 |
| name_base | 308 | 4,004 | 0 (304 on Council before the rewrite) |
| battle_fight | 17,136 | 64,668 | 0 |
| compute_odds | 93,296 | 304,408 | 0 |
| pact_attacks / pact_unpact / pact_truce / pact_treaty | 42 each | 546 each | 0 |
| enemies_unpact | 210 | 2,730 | 0 |
| enemies_team_up | 1,050 | 13,650 | 0 |
| break_treaty | 3,528 | 45,864 | 0 |
| make_treaty | 126 | 1,638 | 0 |
| pledge_truce | 84 | 1,092 | 0 |
| demands_withdrawal | 378 | 4,914 | 0 |
| renounce_pact | 168 | 2,184 | 0 |
| dont_withdrawal | 210 | 2,730 | 0 |
| prune_protos | 3,632 | 22,396 | 0 |
| consider_designs | 35 | 455 | 0 |

Loading Council.SAV with the 688-redirect exe ran the new `fixup_landmarks` and
`mapwin_terrain_fixup` through `map_read`/`map_init`; the map drew normally, landmark relief
included. Logs: `regression-runs/2026-10-17-council-batch28.log` (latest run of each test, with
the exe each ran on, and the name_base details before the rewrite) and
`regression-runs/2026-10-17-sweep-batch28.log`. With all three parts merged (705 redirects) every
test was run again on Council.SAV and the 13 sweep saves, the first runs of `protodesign`: all 0
mismatches (the counts above), through the input-free runs described below. By the original on
Council.SAV, `prune_protos` set 32,316 obsolete marks and changed 3,304 base queues over its runs;
its question to the player (`X_pop`) was not reached in any run (0 questions on every save), so
that path is checked by reading only.

## Windowed mode and input-free runs, 2026-10-18 (Kiousu's request)

So that tests can run while Kiousu uses the machine, the DLL has an optional windowed mode and the
harness can run without any mouse or keyboard input.

Windowed mode (window.cpp, all builds) is off unless the game's ini has `Windowed=1` under
`[OpenSMACX]`; without it the game runs exactly as before. It works like Thinker's: `prefs_get()`
answers 0 for the `DirectDraw` key (the game then draws through GDI), the exe's
`GetSystemMetrics` import reports the window size (1024x768 unless `WindowWidth`/`WindowHeight`
say otherwise, divisible by 8) as the screen size, its `CreateWindowExA` import gives the main
window a caption and a client area of that size at `WindowX`/`WindowY` instead of a popup
covering the screen, and its `SetCursorPos` import is dropped while the game is not the foreground
window, so a game in the background never moves the mouse. The vanilla `DirectDraw=0` setting
alone gives a borderless window at the desktop resolution (2560x1440 here), which looks like full
screen and moves every button. The test folder's ini has `Windowed=1`.

Input-free runs (regress.cpp, harness builds only): when `opensmacx_regress.txt` is already there
at start-up and holds a line `load <save file>`, the harness loads that save in place of the main
menu (`load_daemon` and the faction art, as Thinker's command line load does), runs the tests
named on the other lines from a timer once the game is under way (`GameHalted` 0x68F21C clear,
3 ticks), and with a line `exit` ends the process (no prefs or saves written). Without a `load`
line the trigger works as before (map click, `veh_at()`). `tools/mp/autorun.ps1 -Saves ... -Tests
... [-Minimized]` runs a list of saves this way, one launch per save: Council.SAV with every
batch 28 test takes 10 s, the 13 sweep saves 90 s (the click-driven sweep took about 4.5 minutes),
and a minimized game never takes the focus. A trigger line `notests` only loads the save:
`tools/mp/autoboot.ps1` uses it for the boot test, posting the clicks and keys to the game window
(PostMessage: TURN COMPLETE, the Operations Director's "end the turn", then Enter on each popup)
and reading the turn counter from the game's memory, so the real mouse and keyboard are never
used. Boot test with the 705-redirect exe (harness DLL, as the load needs it): Council.SAV turn
289 -> 290 in 41 s, alive.

Message-driven UI scripts (2026-10-18). `tools/mp/msg.ps1` (dot-sourced) posts clicks, drags and
keys to the game window (`Click`, `Drag`, `Keys` with SendKeys-style strings such as `^s` or
`{END}{BACKSPACE 40}name`), takes screenshots of the game window's own content (`PrintWindow`, so
covered windows are fine and the rest of the screen is never captured), reads game memory
(`Turn`, `KeyWindow`, `PopupOpen`, `BaseScreenOpen`) and starts the game with the save already
loaded (`Start-Game`, with `-Background`: started minimized, then shown again behind the other
windows without activation; a minimized game ignores clicks and spins a CPU core). Posted clicks
and plain keys work as they are; Ctrl/Shift/Alt shortcuts did not, because the game asks Windows
for the modifier state. The harness DLL therefore (input-free runs only) installs a message hook
on the game's thread that notes the key, mouse button and cursor state of the messages the game
takes off its queue, and points the exe's `GetKeyState`, `GetAsyncKeyState` and `GetCursorPos`
imports at versions that add that state; the driver marks its key messages with lParam bit 25 and
posts the character messages itself, and the exe's `TranslateMessage` import skips marked
messages (it would add a second character from the real keyboard). Checked: Ctrl+S opens the save
dialog, typed names (with Shift characters) arrive exactly. Not yet checked in the game: drags.

Scripts: `autoboot.ps1 -Turns N [-Background]` (boot and multi-turn test) acts only on what the
game shows: a visible base screen (BaseWin 0x6A7628, the exe's `Win::is_visible`) gets its OK, a
dialog with the keyboard focus (the exe's `Win_get_key_window` logic, anything but the map window
0x939444) gets Enter, otherwise TURN COMPLETE once per turn and the Operations Director's "end the
turn" only if a dialog came up. A first version sent blind clicks and Enter and kept opening and
closing a base screen (on the map, Enter opens the selected base); Thinker's `PopupDialogState`
(0x6A721C) stays 0 for that kind of dialog, so it is not used. Council.SAV 289 -> 291 in 81 s in
the background. `autosave.ps1` (Ctrl+S under a new name, file checked in the test saves and the
live folder checked untouched, Ctrl+L back): Council.SAV saved and reloaded, alive.
`regress_sweep.ps1` and the old click scripts are superseded.

## Batch 29, 2026-10-18 (Council.SAV and the 13 sweep saves, 718-redirect exe)

Three worktree agents in parallel (base production, the rest of diplomacy, native life and world);
the main session merged them (the two `help_topic` pointers and a second `GameHalted` were folded
into one each) and ran every test through the input-free runs (`autorun.ps1`). 718 redirects.

### Base production (worktree agent)

`base_change` (0x4E5A60), `base_reset` (0x4E5FE0; was a temp.cpp pointer, `bases_reset` now calls
the DLL) and `base_hurry` (0x4F3FE0) in base.cpp; `base_upkeep` left alone (it needs
`base_production`, which completes builds, creates units and plays Secret Project movies).
New: `base_build` (0x4F81A0) and `help_topic` exe pointers, `GameInterludeState` (0x9A64A4).
Kept from the original: 0x9A6590 is Secret Project 31 (Voice of Planet), which halves the AI's
hurry; facility 33 (Psi Gate) doubles the hurry under `at_climax`; the Brood Pit check reads
`Bases[*BaseIDCurrentSelected]`; the perimeter/tachyon check reads
`PlayersData[i].region_total_bases[i]` (faction i's counter at its own id); `rule_hurry` divides
even at 100 and the rebate uses `PlayersData[*RankingFactionIDUnk1].ranking`; `base_change` writes
the parse gender/plural directly and returns early under `SkipTechScreenB`; `base_reset`'s last
`base_build` writes into the loop counter and its GOVPROJECT branch clears `GOV_MAY_PROD_SP` on
`*BaseCurrent`.

Trigger group `baseprod` (`base_change`, `base_reset`, `base_hurry`): the exe's code (with
`base_change`'s entry also put back for `base_reset`) against the DLL from the same snapshot and
seed, `popp`, `wave_it`, `interlude`, `draw_radius` and `help_topic` recorded. Compared: the
snapshot, the call log, parse slots and defaults, `stringTemp`, the pronouns and diplomacy block,
the base globals block, the governor picks, `GameInterludeState`, `BaseCurrent`,
`BaseIDCurrentSelected`, the Secret Project table, the random seed and the next `rand()`.

### Diplomacy part two (worktree agent)

`intervention` (0x50BA50), `mention_prototypes` (0x53A230), `pact_withdraw` (0x53C370),
`buy_tech` (0x5401A0), `energy_trade` (0x540630) and `demand_withdrawal` (0x549270, was a temp.cpp
pointer) in faction.cpp; `call_off_vendetta` skipped (it builds a Popup with an SEH frame). New exe
pointers `wants_to_speak`, `Console_set_move`; globals `InfoWinUnk` 0x7AD330,
`DiploCounterProposalID` 0x93FAB0 and `DiploUnk2`..`DiploUnk8`. Kept from the original:
`pact_withdraw`'s coast check for sea units passes the base's x as a base index and its y as
`is_coast`'s radius flag; `mention_prototypes` with a pact builds "PROTOPACT" and returns without a
dialog; `energy_trade`'s gift is raised to all of the partner's reserves, its sale divisor counts
the owners of `DiploTechGetID1`, and its REJSELLAFFORD branch is unreachable; `buy_tech` type 5
reads tech ids from 97 + design. The batch 28 `demands_withdrawal` test now runs the real
`demand_withdrawal` on both sides.

Trigger group `diplotwo`: every ordered pair of factions 1..7 with 2..13 variants each (forced
treaty states, friction, sunspots, human flags, dialog answers including help, reserves, rank and
tech gaps, loans), the batch 28 notice recorders plus `X_pop`, `wants_to_speak`, `tech_achieved`,
`double_cross`, `Console::update_data`, `Console::set_move`, `diplomacy_caption` and `help_topic`.
Compared as for `diplonote`, plus `VehPrototypes` for `mention_prototypes`.

### Native life and world (worktree agent)

`alien_fauna` (0x522180, game.cpp), `world_erosion` (0x5C2CB0), `world_rivers` (0x5C38B0) and
`world_climate` (0x5C5A30) in map.cpp. `alien_fauna`'s mind worm swarm near a base (XMINDWORMS) is
dead code in the original (the loop before it only ends at its limit, and the same test then
returns); it is left out with a comment. After a native is born on the fungus tile itself, the
original redraws a tile whose coordinates are left in two stack slots by an earlier loop, possibly
from before the call: as with `wants_to_attack`, a naked entry hands those slots in and writes them
back, and only that `draw_tile` uses them. Forest and kelp growth is shown to whoever sees the
source tile, as in the original. `world_erosion` reads nothing stale (the earlier note came from
Ghidra's merged structure). `world_climate` is the orchestrator; its clock reseed is pinned in the
test.

Trigger group `natlife` (not `natworld`: any filter with "world" runs the whole world group).
`alien_fauna`: 7 variants x 12 seeds, the player at the controls rotated, the two stack slots
seeded with real coordinates or junk; `draw_tile`, `draw_tiles`, `MapWin::focus`, `POP2`,
`encounter` and `interlude` recorded, `timeGetTime` pinned. `world_rivers`: 4 variants;
`world_climate` and `world_erosion` in empty-world mode with the clock pinned. Compared: the
snapshot, new units, the call and contact logs, the stack slots left behind, the random state.

### Results

| test | Council.SAV | 13 sweep saves | mismatches |
|---|---|---|---|
| intervention | 252 | 3,276 | 0 |
| demand_withdrawal | 546 | 7,098 | 0 |
| mention_prototypes | 252 | 3,276 | 0 |
| buy_tech | 1,008 | 13,104 | 0 |
| pact_withdraw | 84 | 1,092 | 0 |
| energy_trade | 420 | 5,460 | 0 |
| base_change | 2,394 | 13,398 | 0 |
| base_reset | 938 | 5,238 | 0 |
| base_hurry | 1,972 | 11,004 | 0 |
| alien_fauna | 84 | 1,092 | 0 |
| world_rivers | 4 | 52 | 0 |
| world_climate | 4 | 52 | 0 |
| world_erosion | 5 | 65 | 0 |

The batch 28 groups (`b28main`, `diplonote`, `protodesign`) ran again in the same runs on the
merged tree: 0 mismatches on all 14 saves. Not reached by any run: `base_reset`'s GOVPROJECT
question (0 asked) and `alien_fauna`'s redraw at the stale stack coordinates (0), both checked by
reading only. By the original on Council.SAV: `demand_withdrawal` 248 withdrew, 130 refused, 168
war; `buy_tech` made 756 offers; `base_hurry` spent energy in 1,036 cases; `alien_fauna` bred 157
natives with 127 popups. Logs: `regression-runs/2026-10-18-council-batch29.log` and
`regression-runs/2026-10-18-sweep-batch29.log`.

Two test problems were found on the way, neither in the transcriptions: the first `energy_trade`
run hung because variant 7 answered every question with 2 ("sell the second tech"), which with one
tech on offer means help and the question again, in the exe as in the DLL (now 2 once, then
decline); and the first `alien_fauna` run ended the game with 0xC0000409 because the test's popup
recorder used `sprintf_s` into a 40-byte buffer, which ends the process when the text is longer
(now `_snprintf_s` with truncation). Since then the harness has a watchdog for input-free runs:
every 30 s of one test it logs `... still in <test> after N s, last input: ...` (a working test
shows the input changing, a stuck one doesn't), and after 10 minutes (trigger line
`hang <seconds>`) it logs `HANG in <test>` and ends the game. `autorun.ps1` gives up on a save
after 30 minutes or 2 minutes without log output, and logs the game's exit code. Runs are
watched by polling the logs every few seconds, so a stuck test is seen within a minute.

## Batch 30, 2026-10-18 (Council.SAV and the 13 sweep saves, 736-redirect exe)

Three worktree agents in parallel (unit actions, base removal and handover, planet events and tech
choice), each testing its own group in its own copy of the test folder with input-free runs, and an
independent reviewer per group reading the C++ against the disassembly line by line (no mismatches
found in any of the three). The main session merged them into a 736-redirect tree and ran the whole
suite on all 14 saves.

Merge fixes: the unit actions group had added an exe pointer named `atrocity` and stubbed
`atrocity` at its exe address, and the planet events group stubbed `base_kill` at its exe address;
both functions are DLL code after the merge, and DLL callers reach them directly, skipping a stub on
the exe address. Both stubs now sit on the DLL function's own entry, which the exe's calls also
reach through the redirect jump (the planet events group did the same for `prefs_save`). No other
test stubs a redirected function at its exe address. The shared notice recorders (`draw_tile`,
`Console::focus`, `draw_map`) were merged into one list. `action_arty` reads the artillery range
from the exe's own copy of the range table (0x66F8C4), so a modded range of 9 or more reads what the
original reads; the two tables agree for 0 to 8.

### Unit actions (worktree agent)

`action_feature` (0x4CD060), `action_patrol` (0x4CDA30), `action` (0x4CF740), `action_gate`
(0x4CF380), `action_staple` (0x4CA7F0), `action_destruct` (0x4CD2F0), `action_sat_attack`
(0x4CF480), `action_build` (0x4C96E0) and `action_arty` (0x4CD090) in veh.cpp. Left in the exe:
`action_go_to` and `action_road_to` (they need `Path::move`/`Path::find`, `order_veh` and network
code), `action_tectonic` (calls `kill()` and then reads the freed unit slot again), `action_give`,
`action_fungal`, `action_destroy` and `action_home` (not started). Kept from the original:
`action_patrol` writes a waypoint past the 4-slot arrays when there are 4 or more; `action_staple`
checks alien status with the faction captured at entry; `action_destruct` adds its damage as one
byte and restarts its kill loop from `veh_at` after each `stack_kill`; `action_arty` rounds the
range test down to even (`& ~1`); `action_build` checks the unit's faction twice before focusing
the view, copies the name unbounded and keeps the 0x80000000 governor bit for state 0x200.

Trigger group `unitact`: the exe's code against the DLL from the same snapshot; `draw_tile`,
`Console::focus`, `atrocity`, `boom`, the terraform/go-to/road-to/destroy actions, `battle_fight_2`,
`draw_map`, the report window's redraw, `double_cross` and `monument` recorded (`monument` was added
after the first sweep hung on sw_urfl: founding a base there opens the monument screen, which waits
for the player). Compared: the snapshot, the globals written, the next random number, the return
value, the selection globals, `VehAttackFlags`, the base and replay records, the monument table and
the recorder logs.

### Base removal and handover (worktree agent)

`base_kill` (0x4E5250), `give_a_base` (0x54CF40) and `base_swap` (0x54D410) in base.cpp; the temp.cpp
pointers for `base_kill` and `give_a_base` are gone, and `net_cede_base` calls the DLL's
`give_a_base`. New: the message list box with its list and node structs, `ListBox_update_changes`
and `BaseWin_base_killed` exe pointers, `DiploBaseSwapAskID` 0x93FA7C and `DiploBaseSwapBidID`
0x93FA30 (Thinker's names). Kept from the original: `base_kill`'s sea and land kill loops start one
past the last unit, and its attack-target renumbering covers factions 1..7 only; `give_a_base`'s
unit loop has no count check; `base_swap` divides its price by twice the partner's ranking, so a
partner ranked 0 divides by zero as in vanilla (the test skips that case). Ghidra's `base_find_2`
call in `give_a_base` is really `base_find` (0x4E3C60).

Trigger group `baselife`: `base_kill` on every 7th base, every HQ (also with an energy market corner
active), every 3rd sea base and runs with no base left to rehome units to; `give_a_base` on every
4th base to the next faction, taken by force, and back to its former owner; `base_swap` in 12 cases
per pair of factions (swaps, energy and technology offers accepted, declined or unaffordable, HQs
asked for or offered, the only base offered, revenge with and without force). Bases are picked from
the snapshot, not the live table. Compared: the snapshot, the Secret Project table, the replay
record, the base globals, the selection globals and the base window's base id, the message list,
popup and parse slots, `draw_tile_fixup` calls, the random stream, and for `base_swap` the return
value and dialog calls. Every popup outcome was reached. Not reached by any save: `base_kill`'s
deletion of a message-list entry pointing at the base (checked by reading only), `base_swap`'s two
early returns without a popup, and the partner-ranked-0 skip.

### Planet events and tech choice (worktree agent)

`atrocity` (0x55EEE0) and `major_atrocity` (0x55F450) in faction.cpp, `tech_selection` (0x5BE380) in
technology.cpp, `climactic_changes` (0x524210) in game.cpp, `alt_set` (0x591290) in map.cpp and
`monolith` (0x57A050) in veh.cpp; the temp.cpp pointers `alt_set` and `tech_selection` are gone.
None builds a Popup object on the stack. New exe pointers: `X_pops` (0x5BF5D0),
`NetDaemon_net_tasks` (0x530360), `NetMsg_close` (0x47A7D0), `FX_stop` (0x446A90); globals
`ClimateValueA/B/C` and `ClimateFutureChange` (0x9A67CC..0x9A67D8) and `ControlTurnC` (0x93A938),
Thinker's names. Kept from the original: in `atrocity` the "wantonly" mark tests `FactionsStatus[0]
& (1 << i)` with the loop counter left at 8, so the test always passes; in `alt_set` a dry tile that
sinks gets `(rocky + 3) << 6` stored in a byte (one step flatter); `monolith` calls `morale_veh` a
second time after a repair, as the exe does. `alt_set` calls `base_kill` by name (the DLL's since the merge); `monolith`'s
computer-player test is `want_monolith` inlined.

Trigger group `events` (tests `tech_selection`, `climactic_changes`, `major_atrocity`, `atrocity`,
`alt_set_game`, `monolith_visit`). `tech_selection`: factions 1..7 in six variants (single player;
in a network game another human player whose goal is already there, or arrives on the third
`net_tasks` call, or whose wait is aborted by `ControlTurnA`; the player at the controls; a computer
faction); `tech_pick`, `NetDaemon::net_tasks`, `NetMsg::close` and `NetDaemon::synch` recorded.
`climactic_changes`: six states of the four climate values (nothing pending, counter short of the
threshold, rising one step, falling with the counter carrying over, a counter far past it) on an
empty world, `draw_map` and both view clears recorded, the clock pinned. `major_atrocity`: targets
-1..7 for factions 1..7 in five variants (as loaded, the UN Charter repealed, the target marked
DTREATY_UNK_80000, pacts and no vendettas with everyone with blemishes near the cap, the faction
human). `atrocity`: every ordered pair of factions 1..7 in ten variants (counted or not, sunspots,
the UN Charter repealed, a human faction watched by everyone with a vendetta by chance, everyone
wanting revenge with the faction at the controls, the fifth atrocity of a human, one past the major
atrocity limit, aliens counted, SMAC rules); the exe's `major_atrocity` is put back for the
original's run. `alt_set_game`: every 3rd base, units outside bases and 48 tiles across the map,
each one level up, one down, under the sea and dry land, and for bases also with the owner at the
controls, as the owner's last base, in a network game during a battle redraw, at turn 0, during
world building and at size 2; `base_kill` (stubbed at the DLL function since the merge), `Console::focus`, `popp` and `X_pop_2` recorded,
`veh_kill` real. `monolith_visit`: 40 combat and 40 other units in 14 variants (as loaded; human
with the inspect answers 0/1/2; always inspect; a network game; turn 50; a computer owner; at the
end of a go to and passing through; a battle ogre; upgraded; elite), the two with a chance of using
the monolith up in 8 random streams each; `NetMsg_pop`, `popp`, `FX::play`, `FX::stop`, `draw_tile`
and `prefs_save` recorded (`prefs_save` is stubbed at the DLL function itself, as both sides reach
it there). Compared: the snapshot, the recorded calls with labels and arguments, parse slots and
`stringTemp`, the random stream after the call (`_rand()`), and per test the globals written outside
the snapshot (the landmark relief tables, view caches and selection globals for `alt_set`,
`GameMorePreferences` for `monolith`).

Results:

| test | Council.SAV | 13 sweep saves | mismatches |
|---|---|---|---|
| tech_selection | 42 | 546 | 0 |
| climactic_changes | 6 | 78 | 0 |
| major_atrocity | 287 | 3,731 | 0 |
| atrocity | 490 | 6,370 | 0 |
| alt_set_game | 1,310 | 8,560 | 0 |
| monolith_visit | 2,240 | 14,896 | 0 |

724-redirect exe (all six redirected), harness DLL, input-free runs in the test folder copy
G:\SMACX-opensmacx-test-events. The sweep ran `events b28main diplonote protodesign baseprod
diplotwo natlife`: every test 0 mismatches on all 13 saves (alt_set and tech_selection are also
reached from earlier tested code). By the original on Council.SAV: 4 sea level steps; 320 vendetta
notices from major_atrocity; 202 notices, 166 vendetta dialogs and 22 major atrocities from
atrocity; alt_set 405 units and 93 bases lost, 108 popups, 23 tiles rockier, 9 flatter; monolith 886
upgrades, 1,160 repairs, 18 monoliths used up, 1,073 notices, 160 dialogs, 40 preference saves. A
monolith was used up only on the saves at turn 100 or later (Council, aki2213, aki2247, bugged), as
the rule says. Not reachable in a single player run: real network traffic in tech_selection (the
waits are driven by the recorders).

### Results

Whole suite (`-Tests ''`) on the merged 736-redirect tree, after the `wants_to_attack` change below:

| test | Council.SAV | 13 sweep saves | mismatches |
|---|---|---|---|
| action_feature | 2 | 26 | 0 |
| action_patrol | 5,724 | 21,060 | 0 |
| action | 2,280 | 8,550 | 0 |
| action_gate | 952 | 3,512 | 0 |
| action_staple | 1,491 | 8,316 | 0 |
| action_destruct | 206 | 760 | 0 |
| action_sat_attack | 840 | 10,920 | 0 |
| action_build | 144 | 912 | 0 |
| action_arty | 5,187 | 19,197 | 0 |
| base_kill | 124 | 606 | 0 |
| give_a_base | 155 | 799 | 0 |
| base_swap | 156 | 2,028 | 0 |
| tech_selection | 42 | 546 | 0 |
| climactic_changes | 6 | 78 | 0 |
| major_atrocity | 287 | 3,731 | 0 |
| atrocity | 490 | 6,370 | 0 |
| alt_set_game | 1,310 | 8,560 | 0 |
| monolith_visit | 2,240 | 14,896 | 0 |

Every other test was 0 on every save except `social_ai`'s known one on Council.SAV and sw_bugged
(faction 5, AI self, `bunglist_index` 34 vs 33, the same line as in the 2026-10-15 and 2026-10-16
full logs). By the original on Council.SAV: `action_patrol` changed state in 4,636 cases;
`action_staple` left 747 bases stapled; `action_destruct` destroyed 539 units; `base_kill` killed
295 units and lost 60 projects; `give_a_base` moved 59 units to the new owner and killed 655;
`base_swap` made 37 trades; `alt_set` lost 405 units and 93 bases; `monolith` upgraded 886 units and
was used up 18 times (only on the saves at turn 100 or later). Boot test through the
message-driven UI: Council.SAV 289 -> 291 in 86 s, the game alive, nothing new in the live game
folder. Logs: `regression-runs/2026-10-18-council-batch30.log` and
`regression-runs/2026-10-18-sweep-batch30.log`.

## wants_to_attack, 2026-10-18 (deliberate change from the v2.1 exe, chosen by Kiousu)

`wants_to_attack` is the AI's "would I go to war with them?" check, asked by `enemy_strategy`, the
diplomacy code (`communicate`, `propose_attack`, `threaten`, `call_off_vendetta`, `encounter`), the
Planetary Council (`wants_prop`) and `tech_val`. Part of it decides, per faction, which land regions
are its core: those where it has about three quarters of its largest number of bases in any one
region, or its headquarters. That largest number is a running maximum over the regions, and the
original never sets its starting value: it starts from whatever its stack holds there (a trace found
stack and return addresses such as 1,746,744), so in the game as shipped usually only the
headquarters regions count. The 2026-10-17 version copied those leftovers in from where the
original's frame would have them, which is exact only while the code that ran just before is the
exe's; every function moved into the DLL changes them (the whole-suite `tech_val` comparisons
already differed, 27 on Council.SAV, because the exe's `tech_val` and the DLL's leave different
leftovers; the 718-redirect build shows the same 27). Kiousu chose to have the function do what its
code evidently means: the counts start from zero, so every region where a faction has most of its
bases counts as core. The naked entry is gone. Against the shipped game's leftovers this changes the
answer in 51 of 5,292 whole-suite cases (Council.SAV 16, sw_bugged 12, sw_aki2120 11, sw_aki2213 6,
sw_aki2247 6).

Trigger name `wants_to_attack`: every attacker, target and third faction (-1..7); the original run
with the seven slots zeroed (through `OriginalEntry` and `wta_seeded_call`) against the DLL called
with zeroed, traced and random stack contents, so the DLL's answer is shown not to depend on the
stack. 378 compared on Council.SAV and 4,914 on the 13 sweep saves, 0 mismatches; `tech_val` is back
to 0 (4,676 and 46,752 compared).

## Batch 31, 2026-10-18 (Council.SAV and the 13 sweep saves, 755-redirect exe)

Four worktree agents in parallel (base turn, AI strategy, AI diplomacy, faction turn), each testing
its own group in its own copy of the test folder, and an independent reviewer per group reading the
C++ against the disassembly line by line: no transcription mismatches in any of the four. The main
session merged them on top of batch 30 and ran the whole suite on all 14 saves.

Merge fixes. Tests that stub a function at its exe address are bypassed once the function is DLL
code, because DLL callers reach it directly; four such stubs were moved to the DLL function's own
entry (which the exe's calls also reach through the redirect jump): `double_cross` in the batch 29
`diplotwo` stubs and in `unitact`, `action` in the AI strategy tests (the merged DLL `enemy_veh`
called the real `action` and the game waited on its dialog), and `tech_selection` in the base turn
tests. Exe pointers made obsolete by the merge were dropped (`action_sat_attack`, `action`,
`production_phase`, `enemy_strategy`, `enemy_diplomacy`, `veh_kill`, `kill`), duplicates kept once
(`boom`, the climate values, `NetDaemon_unlock_veh`, `VehAttackFlags`, the replay and base window
test globals), and the AI strategy tests' pin of `wants_to_attack`'s stack slots and the faction
turn group's stack clearing before `tech_val` were removed: `wants_to_attack` no longer reads its
stack (see "wants_to_attack, 2026-10-18"). The AI strategy group's trigger name is `aiplan`
(`aistrat` would also run the older test `strat`).

### Base turn (worktree agent)

`drone_riot` (0x4F4FB0), `base_ecology` (0x4F67F0) and `base_upkeep` (0x4F79C0) in base.cpp and
`production_phase` (0x526E70) in game.cpp; `base_drones` now calls the DLL's `drone_riot`.
`base_production` (14 KB, a Popup on the stack) stays in the exe. Kept from the original:
`base_ecology`'s improvement-name search reuses the tile loop's counter, so a sea fungus bloom only
ever covers one tile; converted units with prototype id exactly 64 are not redesigned (`> 64`); the
new owner's free facilities are checked against the old owner's techs; the queue loop runs to
`i <= queue_size`. `drone_riot`'s defection deliberately differs from the shipped exe, see
"drone_riot defection, 2026-10-18" below.

Trigger group `baseturn`: each function as the exe (`OriginalEntry`, `base_ecology` also put back
for the `base_upkeep` and `production_phase` runs) against the DLL from the same state. Compared:
bases, units (new ones too), tiles, players, prototypes, monuments, the base globals block, the
Secret Project table, the replay record, parse text and numbers, sea level and climate values, the
random streams and every UI call in order (`popp`, `popb`, `POP2`, `interlude`, `boom`, the draw
calls, map centring, console focus, `clock_wait`, the base window, `tech_achieved`,
`tech_selection` recorded). `base_production` runs for real only for computer factions' bases; the
variants that make the faction the player's stub it (its production dialog and `tech_selection`'s
research dialog otherwise wait for the player). Not reached: `base_upkeep` returning 1 (a base
starving to death or defecting during its upkeep), and with it `production_phase`'s second look at
that slot.

### AI strategy (worktree agent)

`enemy_strategy` (0x561080, 14 KB) in faction.cpp, `enemy_veh` (0x579240) and `enemy_turn`
(0x579510) in veh.cpp. `enemy_move` (56 KB) stays in the exe. Kept from the original: the
defensive-unit upgrade check passes prototype ids to `arm_val()`; the expand test adds the aliens'
(faction 0) good-tile count; the "move to an adjacent tile of the unit's own domain" loop compares
the unit's own tile and never matches; terraformer re-homing doesn't check whose base it picks;
`enemy_turn` keeps calling `enemy_veh` after `veh_skip()`. The direction tables at 0x66EF50/0x66EF74
are `RadiusBaseX/Y`.

Trigger group `aiplan`: `enemy_strategy` for each faction in 7 variants and all factions in
sequence; `enemy_veh` on sampled units in 12 variants with `enemy_move`, `action` and the network
calls recorded, plus live runs with the real `enemy_move`; `enemy_turn` with stand-in `enemy_veh`
patterns plus live runs. Live runs pin `timeGetTime` and first run the original against itself.
Compared: game state, base globals, production call records, random state, `VehPrototypes`,
`BaseFindDist`, `VehAttackFlags`, the unit ids `veh_kill` renumbers, `GameTechAchieved` and the
monument table. The last two were added after a live `enemy_veh` run on sw_artifacts differed: a
supply pod gave faction 3 a tech in the original's run, and the DLL's run from the same snapshot
found the tech already owned and gave a unit instead (a test gap, not a transcription error).

### AI diplomacy (worktree agent)

`double_cross` (0x50BCC0), `contiguous` (0x53A780), `pact_of_brotherhood` (0x53CAF0), `comm_check`
(0x55C430), `enemies_war` (0x55C840) and `enemy_diplomacy` (0x55F930) in faction.cpp; new exe
pointers `communicate`, `encounter` and `Path_find` (the DLL's `Path::find` is still a stub), globals
`DiploCommRequest` (0x93F678), `DiploCommRequestPBEM` (0x93F698) and `DiploUnk9` (0x93FA60). Kept from
the original: `double_cross`'s human-versus-computer test compares the two masked status bits, so
two different human factions also pass it. `enemy_diplomacy` passes `action_sat_attack` a base
argument it never set for attack types 0-3; `action_sat_attack` only uses it for type 4, which
always sets it, so the DLL passes -1 there. `comm_check`'s 0x10004 is a first-contact truce.

Trigger group `aidiplo`: the exe against the DLL from the same snapshot over faction pairs and
treaty states; `communicate`, `encounter`, `NetDaemon::send_message` (type and payload hash),
`NetDaemon::await_diplo`, `X_pop`, `wants_to_speak`, `tech_achieved` and `diplomacy_caption`
recorded. Compared: game state, parse slots, the diplomacy globals 0x93F678-0x93FAE0, the game's
random seed, the exe C runtime's `rand()` seed and every recorded call. Not reached:
`enemy_diplomacy`'s `NetMsg_pop` and `X_pop`.

### Faction turn and unit removal (worktree agent)

`veh_kill` (0x5C08C0), `kill` (0x5C0B00) and `interceptor` (0x506650) in veh.cpp; `faction_upkeep`
(0x527290), `mash_planes` (0x529C00) and `compute_score` (0x5ACBE0, ending at 0x5AD3AC; the score
window code after it stays in the exe) in game.cpp. Every DLL caller of `veh_kill`/`kill` now
reaches the DLL's. New: the unit-id globals `veh_kill` renumbers, `DeletionList` and `BaseWin` check
bindings, `veh_scoot`, `call_council`, `PathXDst`/`PathYDst`, `AscentScoreScale`/`AscentScores`.
Kept from the original: `interceptor` reads `Bases[-1]` when there is no base on the map;
`compute_score` reads `diplo_treaties[-1]` when no faction owns the Ascent base, doesn't guard its
division by zero, zeroes the faction's own score fields even when given other arrays, and in a
"highest score wins" scenario leaves its victory bonus out of that call's total; `veh_kill`
decrements `VehIDCurrent` when it equals the killed unit and never when it is 0.

Trigger group `facturn`: `veh_kill` with every unit-id global equal to, above and below the unit;
`unit_kill` (`kill`) with and without the redraw; `mash_planes` in 13 variants; `interceptor` on
targets out to 3 tiles; `compute_score` in 21 variants per faction; `faction_upkeep` with every
phase and dialog recorded (market corners, eliminations, council calls, autosaves recorded and never
written). Its stubs patch both the exe address and the DLL function the address redirects to. The
worker planted one bug in each function and every test caught its own.

### Results

Whole suite (`-Tests ''`) on the merged 755-redirect tree:

| test | Council.SAV | 13 sweep saves | mismatches |
|---|---|---|---|
| drone_riot | 2,555 | 14,565 | 0 |
| base_ecology | 3,798 | 21,184 | 0 |
| base_upkeep | 1,962 | 11,906 | 0 |
| production_phase | 53 | 809 | 0 |
| enemy_strategy | 57 | 741 | 0 |
| enemy_veh | 1,190 | 15,827 | 0 |
| enemy_turn | 43 | 559 | 0 |
| contiguous | 256 | 2,394 | 0 |
| comm_check | 7,602 | 98,826 | 0 |
| enemies_war | 504 | 6,552 | 0 |
| pact_of_brotherhood | 210 | 2,730 | 0 |
| double_cross | 3,528 | 45,864 | 0 |
| enemy_diplomacy | 238 | 3,094 | 0 |
| veh_kill | 3,546 | 13,212 | 0 |
| unit_kill | 1,795 | 6,625 | 0 |
| mash_planes | 13 | 169 | 0 |
| interceptor | 216 | 306 | 0 |
| compute_score | 168 | 2,176 | 0 |
| faction_upkeep | 72 | 936 | 0 |

Every other test was 0 on every save except `social_ai`'s known line on Council.SAV and sw_bugged.
By the original on Council.SAV: `drone_riot` destroyed 222 facilities and 250 bases defected;
`base_ecology` bred 6,231 units with 848 global warming steps and 6 volcanoes; `production_phase`
paid 9 loans and gave 5 free techs; `faction_upkeep` ran 7 market corners and 24 eliminations;
`enemy_strategy` disbanded 34 units and launched or gave up 12 attacks. Boot test through the
message-driven UI: Council.SAV 289 -> 291 in 86 s, the game alive, nothing new in the live game
folder. Logs:
`regression-runs/2026-10-18-council-batch31.log` and `regression-runs/2026-10-18-sweep-batch31.log`.

## drone_riot defection, 2026-10-18 (deliberate change from the v2.1 exe, chosen by Kiousu)

When a human player's base keeps rioting under police -3 or worse and the rioters find nothing
left to destroy, the base defects: to its former owner if it was captured, otherwise to the faction
with the nearest base (a pact doubles that distance, a treaty adds half, a truce halves it).
Factions with the REVOLT rule (in SMACX the Free Drones, `REVOLT, 75`: "Revolting bases tend to join
drones") can take it instead. The shipped code has three slips here:

- It keeps each faction's REVOLT chance in 7 stack slots that it only fills for REVOLT factions;
  the rest keep whatever is on the stack, though the code reads -1 as "no REVOLT" (the exe even
  stores a -1 per faction into a scratch variable it never uses). A trace of 303 calls on
  Council.SAV found leftovers from `base_psych` there, one of them huge, which makes the odds
  negative: in shipped SMACX the REVOLT roll never fires, unless the Drones happen to be faction 3.
- It hands the base to a REVOLT faction when the clock roll is at or below the chance that none
  takes it, the wrong way round (about 23-25% for the Drones instead of 75%), and with clean slots
  and no REVOLT faction in the game it would always roll and divide by zero.
- It weighs each candidate base by the treaty with the closest faction found so far, not with that
  base's owner (before any is found it reads the word before the treaty table).

Kiousu chose the intended behaviour: the slots start at -1; a REVOLT faction takes the base when
`timeGetTime() % 100 < round(100 * (1 - product of (1 - chance / 100)))`, so exactly 75% with only
the Free Drones and never without a REVOLT faction; the pick among REVOLT factions is weighted by
their chances as in the original; each candidate base is weighed by its own owner's treaty. The
test compares everything else with the exe (riots, facility destruction, no-defection cases, and
defections where both agree, with the original's roll pinned off) and checks the new rule directly:
REVOLT 75 alone takes the base on 75 of the 100 clock values (600 of 800 on Council.SAV), no REVOLT
faction on none, two REVOLT factions at 30/50 on 65 per base with every pick following the weights,
and the treaty fix on 126 Council.SAV bases (66 of which the slip would have sent elsewhere).

## hex_cost Sealurks, 2026-10-18 (deliberate change from the v2.1 exe, chosen by Kiousu)

A sea unit entering fungus on the ocean shelf pays triple movement in the v2.1 exe unless it is the
Isle of the Deep or its faction has the Xenoempathy Dome. Upstream's `hex_cost` also spares the
Sealurk (`proto_id != BSC_SEALURK // Bug fix`): like the Isle of the Deep it is native life, which
the game text describes as haunting the fungal patches. The click pass found the difference (a
Sealurk on sw_steam1.SAV moving two tiles further in one turn) and bisection named `hex_cost`;
Kiousu kept the fix ("Keep fix"), and its row is an approved row, so the click pass's reference
plays it too.

`hex_cost` had no function test; `test_hex_cost` (trigger `hex_cost`) now runs the exe's own code
(its entry bytes put back while the test runs) against the DLL's for every shelf fungus tile with
every prototype and every 5th other tile with every 8th prototype, from all 8 neighbours, for all
8 factions and both values of `toggle`. Only the Sealurk entering shelf fungus without the Dome
may differ, and there the exe must charge triple and the DLL the normal cost. On Council.SAV and
the 13 sweep saves (`regression-runs/2026-10-18-sweep-hex_cost.log`): 0 mismatches on every save,
3.7 to 34.4 million calls each (Council.SAV: 34,410,112 with 352 prototypes and 578 shelf fungus
tiles, 64,316 of them the Sealurk fix), which also shows the test reached the exe's code.

## Click pass, stage 1, 2026-10-18

The click pass (design: `docs/superpowers/specs/2026-10-18-click-pass-design.md`) plays short
scripted UI cases in the unmodified game and in OpenSMACX from the same save and compares their
whole game state after every step. Stage 1 is the plumbing and four cases on the existing saves.

Folders. `G:\SMACX-opensmacx-test-clickref` is the reference: the pristine exe patched with only
the approved rows of `tools/redirects.tsv` (tagged `approved`; no tool ever removes them): the
changes from the v2.1 exe that Kiousu chose, so the reference plays the intended rules and the pass
judges OpenSMACX against them. They are `game_data` (the save header), `battle_compute` and
`best_defender` (no read of `Vehs[-1]`), and since 2026-10-18 `hex_cost` (the Sealurk fix),
`wants_to_attack` (zero-start counts), `drone_riot` (the intended defection) and, with batch 32,
`action_airdrop` (the notice names a base the player at the controls knows), `action_terraform`
(the enricher's own technology, the former's own state after raise/lower), `action_home` (the
airbase's own distance), `action_tectonic` and `action_fungal` (the missile's own faction and
reactor, a payload once shot down stays down) and `action_give` (the units actually given, counted
against their new designs): twelve in all. The reference runs these whole functions from the DLL,
so the pass doesn't compare the rest of them with the exe; their function tests do, with only the
chosen change allowed: `hex_cost`, `wants_to_attack`, `drone_riot`, `save_header`,
`action_airdrop`, `action_terraform`, `action_home`, `action_tectonic`, `action_fungal` and
`action_give` run the exe's own code on any build, `battle_compute` and `best_defender` only on an
exe patched without their rows (see How to run). `G:\SMACX-opensmacx-test-clickosx` is OpenSMACX: the exe
patched from the whole table. Both are `make_testdir.ps1` copies with the harness DLL
(`/DOPENSMACX_REGRESS`), `Windowed=1` and `DirectDraw=0`. Each game may also write to
`G:\SMACX-clickpass-outside\<its folder name>` (for "save elsewhere"), which the driver empties
before each case. The reference folder keeps its own harness DLL and it is replaced only when the
harness plumbing changes (clickpass/regress/window code, the dump format); batches update only the
OpenSMACX folder, so the recorded references survive them (ruling R19). `clickpass.py reference`
builds the reference folder: it patches the pristine exe with only the approved rows (in a temp
folder, with `opensmacx_patch.py`), installs that exe and the harness DLL given (default
`build\regress\OpenSMACX.dll`) and checks both by sha256; it refuses the live folder, a folder a
game runs from and one with a bisect's note. Before every pass `run` repeats that patch with the
reference folder's own DLL and refuses to start when the folder's exe differs, so the reference is
never anything but the unmodified game plus the approved rows.

How to run (Python from `build\venv`: the standard library, plus `pefile`, which `run`'s exe checks,
`reference` and `bisect` need through `tools\opensmacx_patch.py`):

    python tools\clickpass\clickpass.py run [--cases idle,reports,...]   (default: every case)
    python tools\clickpass\clickpass.py reference [--dll build\regress\OpenSMACX.dll]
    python tools\clickpass\clickpass.py bisect <case> [--run <pass folder>]
    python tools\clickpass\clickpass.py bisect --recover
    python tools\clickpass\clickpass.py report [--run <pass folder>]

`run`, `bisect` and `report` refuse an `--out-root`, `--ref-dir`, `--osx-dir` or `--run` that is,
is in or holds `G:\Sid Meier's Alpha Centauri` before they make anything.

A case file (`tools/clickpass/cases/*.case`) starts with `save <file.SAV>` and has one step per
line (`keys <SendKeys spec>`, `click x y`, `rclick x y`, `wait ms`, `endturn`); a case on another
save is a copy with another first line (`reports_aki2120.case`). `tools/clickpass/case.ps1` plays
it in one folder: an input-free load (trigger lines `load`, `notests`, `clickpass`, `outside`), keys
and clicks posted to the background window through `tools/mp/msg.ps1`, and after every step a
state dump from the harness and the list of files the game has written. In click-pass mode the
harness pins `timeGetTime` on the game thread from start-up (DllMain) on to a clock that advances 16
per call, so the load sees it too (end_turn was not repeatable before the pin); at `ready` it
reseeds `rand()` and the game's random object and points the stored save path at the folder's own
saves. The case driver keeps a copy of the folder's `Alpha Centauri.ini` and puts it back after
each case, so `Latest Save` doesn't drift from case to case, and empties `saves\auto\` before each
case: the game names and rotates its end-of-turn autosaves by what is already there, which made
end_turn_steam1's two reference runs write different files.

What a step's dump holds (`src/clickpass.cpp`, read by `tools/clickpass/dump.py`): PlayersData,
Players, Bases, Vehs, the map tiles, Landmarks, Continents, the Random object; the game state block
the save stores first (0x9A6490, 0x398 bytes, laid out as `ClickpassGameState` in
`src/clickpass.h`: preferences, rules, game state, turn, Secret Projects, Council counts, the techs'
factions (GameTechAchieved), climate, sunspots and the rest of it); the prototypes (VehPrototypes),
the monument table, the path finder (Path's members and its map table, as GameSnapshot keeps them),
the submerged-base counts; and globals: turn, game state, the selected unit (MapWinVehIDSelected)
and base (BaseIDCurrentSelected), the CRT `rand()` state, and which screen is up: the window with
the focus (its address, 1 for one on the heap) and its vtable (which tells heap windows apart), the
F2-F8 report screen shown (2..8), the label of the popup with the focus (`REALLYQUIT`,
`MENUSIMPLE`, noted by a hook on the exe's BasePop::start() that only records it), and whether the
base screen is open. The stored save path is compared without the folder (`SavePath`:
`<game folder>\saves\Council`); its raw copy in the block is left out by `ignore.tsv` (R13).

The files a step has written: after every dump the driver lists each file under the game folder
and the outside folder that is new, changed or gone since just before the launch (not the
harness's own `clickpass\`, its logs, the trigger and a bisect's files), by name, size and sha256,
and keeps a copy (`files_<step>.json`, `files\<sha256>` in the run's folder and the reference
cache). The runner compares the two games' lists by name and the contents byte by byte
(`files[game\saves\_cp_save.SAV]+0x1A2B: 1f vs 20`), except the bytes `tools/clickpass/file_masks.tsv`
masks, each with its reason: in `.SAV` files the stored save path (0x561, 256 bytes, R13) and the
two heap addresses saved with the map block (`*MapTiles` 0x94A30C and `*MapAbstract` 0x94A310, file
offset 0x144C0, which two runs of the reference don't agree on either), and the `Latest Save=` line
of the ini (each game's own folder). Not compared: the rest of the map block (planet parameters,
bounds), the path finder's x/y queues, the other exe globals, the screen's pixels, anything outside
the two folders (the guard refuses writes there), and the masked and ignored bytes above. `run` writes
`build/clickpass/<YYYY-MM-DD-HHMM>/` (a folder per case, `result.json`, `report.md`) and exits 1
when a case fails. The reference side is recorded once, with the self-check (played twice, both
runs compared), in `build/clickpass/reference/<case>/<key>/`: the key covers the reference exe and
DLL, the save and the case file, and an entry recorded with another `ignore.tsv` or case driver
(`case.ps1`, `folders.ps1`, `msg.ps1`) or file masks is recorded again. A run after a batch
therefore plays only the OpenSMACX side. `run` warns when the OpenSMACX exe is not the table's patch
and refuses to start while a bisect's note (`clickpass_bisect_swap.json`) is in a folder. An
unexpected error in one case (a file that can't be written, PowerShell missing) makes that case
setup-failed and the pass goes on.

| status | meaning |
|---|---|
| pass | every step's dump and written files equal the reference's |
| fail | a step differs; the report gives that step's first differences in readable form, unmodified game first (`Vehs[412].dmg_incurred: 3 vs 2`, `files[game\saves\_cp_save.SAV]: written (686926 bytes) vs not written`) |
| crash, hang, blocked-write | the OpenSMACX game ended (with its exit code), a dump or the next turn never came, or it tried to write outside its folders, at that step |
| setup-failed | the OpenSMACX side could not be played (driver error, a save that differs between the folders, a bisect's note) |
| nondeterministic | the reference's own two runs differ: not compared, listed apart |
| reference-broken | the reference side did not complete, or its cached dump can't be read (recorded again next time) |

Guard. The harness wraps the file-writing imports of the exe and the DLL (the A and W versions)
and refuses, and logs to `clickpass\guard.log`, every write outside the game's own folder and its
outside folder; the driver ends the case there as blocked-write, so a test game can't write into
`G:\Sid Meier's Alpha Centauri` through them.

Bisection. `bisect <case>` plays the failing case in the pass's OpenSMACX folder with exes patched
(`opensmacx_patch.py -t`) from halves of the table, the approved rows in every one, and names the
row (or the smallest set) that reproduces the failure: for one culprit among the 752 candidate rows
of the 755-row table, two runs to check the ends and about ten searching (the first real
bisection, on the 736-row table, took 12 in all). Copying an exe over one that is still held for a moment (a game that
has just ended, a virus scan of the new file) is tried again for up to 10 s. The folder's
exe is swapped meanwhile and put back however the bisection ends, checked by sha256; if a bisect
was killed, the next one (or `bisect --recover`) puts it back. "Not clean" means that neither half
of the named set reproduces alone: rows that act together, or a read of leftover memory.

Report. `report.md` (rewritten by `bisect` and by `report`) has, in this order: Summary (counts per
status, every case with its save and step), Failures (step, first differences, the culprit and
its row, what's being done: "held back" when the row is out of the table, a recorded decision, or
"open"), Needs Kiousu's decision, Nondeterministic cases, and Coverage from
`tools/clickpass/inventory.tsv` (screen, control, case, status `covered` / `not-reachable` /
`skipped`, note; stage 1 lists the map's F1-F12, Ctrl+S and TURN COMPLETE, nothing else yet).

Fix loop (from the spec): the culprit's row comes out of `tools/redirects.tsv` at once, so the game
uses the exe's code; a worker in its own worktree and test folder and an independent reviewer fix
it as in the batches; the failing case and then the whole pass are rerun before the row goes back.

Decision loop: a failure whose culprit is in `tools/clickpass/leftover_reads.tsv` (functions whose
original reads memory it never wrote, so the result depends on what ran before; today
`alien_fauna` 0x522180, the stale draw coordinates) or whose bisection is not clean goes under
"Needs Kiousu's decision" with what each game had and why, not into the fix loop. Kiousu decides
(do what the code evidently means, match the original's leftovers, or accept the difference); the
decision goes into `tools/clickpass/decisions.tsv` (`case` or `*`, `function`, `decision`,
`date`, `why`), and the case is not raised again while every culprit row is covered there; it stays
under Failures with the decision as what's being done. Recorded on 2026-10-18: `wants_to_attack`
starts its region counts at zero, and `drone_riot`'s REVOLT slots start at -1 with
take = round(100 * (1 - prod(1 - c/100))), picked when time % 100 < take, the nearest-faction
distance using each base's own owner's treaty (the intended design in both); `hex_cost` keeps
upstream's Sealurk fix (Kiousu: "Keep fix"). With batch 32: `action_airdrop`, `action_terraform`,
`action_home`, `action_tectonic`, `action_fungal` and `action_give`, one row each with Kiousu's
decision (see their sections after "Batch 32, 2026-10-18").

### First results

Run on 2026-10-18 with the click-pass worktree's build in the OpenSMACX folder (736 rows, 3 of
them approved; its exe is the table's patch). First `run --cases idle,reports,save_game,end_turn` on
Council.SAV: 4 pass (167 s). Then the first full pass, the four Council cases and copies of idle,
reports and end_turn on sw_aki2120.SAV and sw_steam1.SAV, in about 5 minutes (report:
`regression-runs/2026-10-18-clickpass-stage1.md`):

| case | Council.SAV | sw_aki2120.SAV | sw_steam1.SAV |
|---|---|---|---|
| idle | pass | pass | pass |
| reports (25 steps) | pass | pass | pass |
| save_game (5 steps) | pass | | |
| end_turn | pass (turn 289 -> 290) | pass (20 -> 21) | **fail** at step 1 (32 -> 33) |

Every reference recorded agreed with its self-check run (no nondeterministic case), and nothing
was blocked or written in the live game folder. end_turn on sw_steam1: a native Sealurk (unit 79)
moves from (35,59) to (33,57) in the unmodified game and to (31,55) in OpenSMACX; its counters, the
visibility of the tiles it passes and the `rand()` state follow. Bisection (12 runs, 158 s, clean):
`hex_cost` (0x593510) alone. The reason is in the code: upstream's `hex_cost` has a "Bug fix",
`proto_id != BSC_SEALURK`, that spares Sealurks the triple cost the v2.1 exe charges sea units
entering fungus on the ocean shelf. Kiousu kept the fix (2026-10-18), and its row is now an
approved row (below).

Rerun after the final review's fixes (same day, pass `2026-10-18-0700`, the report in
`regression-runs/2026-10-18-clickpass-stage1.md`): the bigger dumps, the written files compared and
the clock pinned from start-up. Every reference was recorded again with its self-check, and every
self-check agreed: 9 pass, 1 fail, the same end_turn_steam1 Sealurk difference, which now also shows
in the autosave the turn writes (`saves\auto\Alpha Centauri Autosave 1.SAV`). In save_game the saved
file and the ini are compared: unmasked they differ only in the stored save path (file 0x57E-0x580,
`ref` vs `osx`), the two map pointers (0x144C0-0x144C7) and the ini's `Latest Save`; masked, not at
all. The pass before it (`2026-10-18-0642`) had marked end_turn_steam1 nondeterministic: its two
reference runs wrote different autosaves, because `saves\auto\` still held earlier runs' autosaves
(the driver now empties it). In that pass one OpenSMACX run of end_turn on Council.SAV also left two
faction-2 units at (104,102) without their hold order; eight reference and six OpenSMACX runs since
have all agreed, so it is a rare variation of the OpenSMACX side (a timing race in the endturn step,
or memory a function reads before writing), not understood yet. The self-check plays only the
reference twice, so such a variation on the OpenSMACX side shows up as a fail.

### On desktop-msvc-net, 755 rows

After the merge into `desktop-msvc-net` (batches 30 and 31, the zero-start `wants_to_attack` and
the intended `drone_riot` defection included), `clickpass.py reference` reinstalled the reference
folder with the merged harness DLL (the exe is unchanged, be1c508a) and the OpenSMACX folder got
the 755-row exe (9bb81af4, the batch 31 sweep's exe) and the same DLL. Pass `2026-10-18-0923`
(report: `regression-runs/2026-10-18-clickpass-755.md`; its paths are on the local disk): every
reference recorded again with its self-check, 9 pass, 1 fail. end_turn on Council.SAV passes (no
repeat of the hold-order variation), and so do end_turn_aki2120, the idle and reports cases and
save_game with its saved file. end_turn_steam1 fails at step 1 with the same Sealurk (unit 79 at
(31,55) instead of (33,57), 39 differences), and bisection over the 752 candidate rows names
`hex_cost` (0x593510) alone, clean, in 13 runs (179 s); the other 751 rows together don't
reproduce it. Neither `wants_to_attack` nor `drone_riot` changes anything these cases see.

Kiousu then kept the Sealurk fix and asked for the pass to test the intended behaviour, so
`hex_cost`, `wants_to_attack` and `drone_riot` became approved rows (six in all) and
`clickpass.py reference` rebuilt the reference exe (b394757c) with the harness DLL that has
`test_hex_cost`; the OpenSMACX exe is unchanged. Pass `2026-10-18-0954` (report:
`regression-runs/2026-10-18-clickpass-755-approved.md`): every reference recorded again with its
self-check, 10 pass, end_turn_steam1 included.

### On desktop-msvc-net, 765 rows

Batch 32 added six approved rows (`action_airdrop`, `action_terraform`, `action_home`,
`action_tectonic`, `action_fungal`, `action_give`: twelve in all). The merge's pass ran in two new
folders, `G:\SMACX-opensmacx-test-mainref` (the pristine exe with the twelve approved rows,
9f1e9875) and `-mainosx` (the 765-row exe, 06150821), both with the merged harness DLL and the focus
guard: pass `2026-10-18-1451` (report: `regression-runs/2026-10-18-clickpass-765.md`), every
reference recorded again with its self-check, 10 pass. See "Batch 32, 2026-10-18".

## Batch 32, 2026-10-18 (Council.SAV and the 13 sweep saves, 765-redirect exe)

Three worktree agents in parallel (movement orders, unit terraforming, special payloads and handing
units over), each testing its own group in its own copy of the test folder, and an independent
reviewer per group reading the C++ against the disassembly line by line: no transcription
mismatches in any of the three (the reviews' findings were in the tests and the comments, fixed in
the fix rounds). Six of the ten functions have a deliberate change from the v2.1 exe that Kiousu
chose; their rows are `approved` and each has a section after this one. The main session merged the
three groups on top of 2f0e050 and ran the whole suite on all 14 saves.

Merge. The groups went in as 3-way patches (movement orders, unit terraforming, special payloads);
the conflicts were textual. The exe pointers the first two groups removed sat next to each other in
temp.h and temp.cpp, and all four are gone (`action_go_to`, `action_road_to`, `action_terraform`,
`action_destroy` with its `func_action_destroy`); `order_veh` (0x5947C0), which both added, is kept
once (the movement orders' `func_iii`, the same type as the terraforming group's own typedef).
`UnitActStubs` has all four of its stubs for the batch's functions (`terraform`, `go_to`, `road_to`,
`destroy`) on the DLL entries, the two test blocks after `test_action_arty` and their dispatch lines
are in group order, and the three redirect blocks are at the end of `tools/redirects.tsv`, each row
once: 765 rows, twelve of them `approved`. Stub check: no test stubs any of the ten functions at its
exe address any more. The movement orders' own `action_terraform` recorder is set at
`(uint32_t)action_terraform`, which became the DLL entry with the merge; every other use of the ten
exe addresses is an `OriginalEntry` (the exe's own code put back for the test) or a call into it.

Focus guard. The suite, the boot test and the click pass below ran with the click pass's focus
guard, brought over from its stage 2 (ruling S11): every test game is launched without activation
(`tools/mp/msg.ps1`'s `Start-TestProcess`: CreateProcess with SW_SHOWMINNOACTIVE or
SW_SHOWNOACTIVATE, used by `Start-Game` and `autorun.ps1`), and the harness DLL
(`src/focusguard.cpp`, regress build only, on for input-free loads) keeps the game from activating
itself, moving or clipping the real cursor (SetForegroundWindow, SetActiveWindow, BringWindowToTop,
SetCursorPos and ClipCursor do nothing, ShowWindow and SetWindowPos get their no-activate forms, the
windowed main window gets WS_EX_NOACTIVATE) and from reading the real keyboard and cursor:
GetKeyState, GetAsyncKeyState and GetCursorPos answer from the posted messages alone, for the DLL's
own calls too. Each kind of changed call is noted once in opensmacx_regress.log (`FOCUS-GUARD
ShowWindow(5) shown as ShowWindow(7)`). Normal play is unchanged (release builds don't have it).
`tools/clickpass/tests/smoke_focus.ps1 -Command` around one autorun of Council.SAV
(`-Tests value_of_base`) in the test folder: PASS, 44 samples over 5.1 s, the foreground Firefox's
in all of them and the cursor where it stood before the launch.

### Movement orders (worktree agent)

`action_go_to` (0x4CB310), `action_road_to` (0x4CB580) and `action_airdrop` (0x4CC360) in veh.cpp;
the temp.cpp pointers `action_go_to` and `action_road_to` are gone. The path finder and the network
daemon stay in the exe: new exe pointers `Path_move` (0x59BC10; the DLL's Path::move is unfinished),
`order_veh` (0x5947C0), `NetDaemon_order_veh` (0x531780), `say_loc` (0x592250), `capture_base`
(0x50C510), `goody_box` (0x57A660), `Console_cursor_next` (0x5109B0) and the Console field
`MapWinUnk23BF0` (0x9392A0); `Path_find` is batch 31's, and `action_terraform` (the unit
terraforming group's) is called directly since the merge. Kept from the original: go_to and road_to
point NetVehIDRef at their own unit id parameter in a network game and never clear it (the DLL's
parameter slot is the exe's); go_to's multi-waypoint shift copies index `waypoint_count`, past the
arrays at 4 (the map view's waypoint mode never gets there); a computer faction's unit whose chassis
has a cargo rating (all but the missile) ends its turn when its go to arrives in a base; road_to's
tube to passes TERRA_ROAD to action_terraform, which makes it a magtube on a road.
`action_airdrop`'s notice is a deliberate change from the v2.1 exe, chosen by Kiousu (2026-10-18),
see "action_airdrop notice"; its redirect row is marked `approved`.

Trigger group `unitmove` (tests `action_go_to`, `action_road_to`, `action_airdrop`): the exe's code
(OriginalEntry) against the DLL's from the same snapshot through `unitact_run`, plus every call into
foreign code recorded in order with its arguments and the unit id NetVehIDRef points at (Path::move,
Path::find, order_veh, the NetDaemon calls, synch_veh, message_data, action_terraform, spot_all,
veh_scoot, clock_wait, Console::cursor_next, capture_base, goody_box, ambience, GetAsyncKeyState),
answered from a per-case script (order_veh moves the unit to a chosen tile, the network waits can
renumber the unit as veh_kill would, capture_base can renumber VehIDCurrent), and the lift and bit
error globals. go_to: 23 cases (no step, steps, arrivals at either waypoint and in bases, human or
computer, multi-waypoint go tos with 1 to 4 waypoints, network games) x 3-4 orders, plus the exe's
own Path::move toward four targets. road_to: 9 tile kinds x 5 tech sets x 2 orders x 3 targets, 14
path and network cases (two of them in a network game knowing fungus removal but not roads in
fungus, which sends the "remove fungus" message), and the exe's own Path::find along a road.
airdrop: 19 targets x 22 cases (flags 0 to 3, watchers, preferences, shift, treaties, air defenses,
captures, network, non-combat, Cloudbase Academy, alien factions), the exe's `push 1` at 0x4CCC4C
made `push ecx` (FactionIDCurrent) for that pass, and a second pass with the shipped exe that may
differ only where the two base lookups name different bases. A test-only snag: a faked unit bit made
veh_at rebuild the unit bits, which in a network game happens once per VehBitError, so the exe's run
used it up; the cases now move a real unit, and the lift and bit error globals are put back and
compared each run.

| test | Council.SAV | 13 sweep saves | mismatches |
|---|---|---|---|
| action_go_to (scripted) | 7,725 | 28,500 | 0 |
| action_go_to (exe's Path::move) | 606 | 2,224 | 0 |
| action_road_to (tiles x techs) | 11,610 | 43,740 | 0 |
| action_road_to (paths and network) | 16,856 (172 "remove fungus" messages) | 62,720 (576) | 0 |
| action_road_to (exe's Path::find) | 720 | 2,700 | 0 |
| action_airdrop (exe made to ask for the player at the controls) | 13,222 | 49,698 | 0 |
| action_airdrop (shipped exe) | 3,005: 358 predicted differences | 11,295: 2,331 predicted | 0 unexplained |

By the DLL on Council: go_to ended 1,133 orders after a move and shifted 412 waypoint lists; road_to
started 2,864 roads or tubes and 360 fungus removals; airdrop made 5,499 drops (18 captures, 3,993
flights shown, 130 pauses, 18 pods) and refused 1,036 out of range, 1,731 at sea, 163 onto units,
251 non-combat and 48 by air defenses. unitact unchanged (its go_to/road_to stubs now sit on the DLL
entries). Boot test: Council.SAV 289 -> 290 in 29 s, alive. Logs:
`regression-runs/2026-10-18-council-batch32-unitmove.log`,
`regression-runs/2026-10-18-sweep-batch32-unitmove.log`.

### Unit terraforming (worktree agent)

`action_terraform` (0x4C9B00), `action_destroy` (0x4CAA50) and `action_home` (0x4CBAA0) in veh.cpp;
the temp.cpp pointers `action_terraform` and `action_destroy` are gone (`action()` and
`action_arty()` call the DLL's), and the unitact recorders for them sit on the DLL entries. New exe
pointers: `BaseWin_check_veh`, `TutWin_tut_map` with `TutWinVar`, `order_veh`, and the globals
`TutTerraformShown` (0x9A64B4, the tutorial's "MADE..." bits for land and sea) and
`TerraformSoundPlayed` (0x90DB84, the completion chime, cleared by `turn_upkeep()`). `action_home`
does not set a home base: it finds where a unit rests or refuels and sends it there (mode 1 is
`order_veh()`'s low-fuel check, -1 only answers it, 4 is `air_power()`, 2 `enemy_move()`). Kept from
the original: computer factions above Librarian work one rate step faster; the units-lost messages
(DROWNMINE/DROWNYOURS) stay silent as shipped, because `world_climate()` before the check goes
through `world_alt_set()` for nearly every tile (`world_shorelines()`) and every call resets the
record (the tests found the player's units lost in 268 inputs and the record set at the check in
none); `action_destroy` returns without `order_veh` after a failed shelling, and its notice to the
tile's owner reads `Bases[base_find(...)]` unchecked (the owner at the controls always has a base
there); `synch_bit(x, y, -1)` writes the tile's own bit word onto itself. Two deliberate changes in
`action_terraform` and one in `action_home`, chosen by Kiousu, see "action_terraform farm and
raise/lower" and "action_home airbase distance" below; the units-lost messages stay silent by
Kiousu's choice, see "Units-lost messages".

Trigger group `unitterra`: each input runs the exe's code (`OriginalEntry`) against the DLL from the
same snapshot. For `action_terraform` and `action_home` the exe runs with its slips fixed in its own
code (one byte, altitude wrappers that restore the unit's own slot, a 4-instruction cave) against
the DLL, and the shipped exe against the fixed one counts the decision cases (any other difference
fails the test). `action_terraform`: up to 48 units (formers first), all 20 jobs, 10 variants
(estimates, pooled stacks, completion, tutorial and chime for the player at the controls, harvests,
farm or road already there, raise/lower near another faction, a planted units-lost record).
`action_destroy`: units, natives and aircraft on their own tile and tiles two steps out, 6 variants
(shelling, bombing, sensor/forest/condenser, the owner in contact, spore launchers). `action_home`:
aircraft and a spread of other units, 5 modes, 11 variants (on alert to a base or a planted
airbase, low fuel, computer and human, aircraft placed in the field). The test inputs aren't
limited to orders the game accepts: completions are forced, so some formers drown in their own
raise/lower. Recorded: the notice and unitact recorders plus `draw_tiles`, `draw_radius`, the base
window checks, `GraphicWin::redraw`, `TutWin::tut_map`, `wave_it`, `ambience`, `order_veh`,
`base_kill` and the terrain clears; `timeGetTime` pinned. Compared: the snapshot, parse slots,
diplomacy globals, random streams, return value, base and selection globals, every recorded call,
the tutorial bits, the chime flag and the rainfall globals. One planted bug per function was caught
(14, 46 and 90 mismatches).

| test | Council.SAV | 13 sweep saves | mismatches |
|---|---|---|---|
| action_terraform | 4,885 | 40,246 | 0 |
| action_destroy | 4,604 | 27,603 | 0 |
| action_home | 8,930 | 15,260 | 0 |

The unitact group's counts are unchanged and 0 on all 14 saves. Logs:
`regression-runs/2026-10-18-council-batch32-unitterra.log` and
`regression-runs/2026-10-18-sweep-batch32-unitterra.log`.

### Special payloads and handing units over (worktree agent)

**Functions.** `action_oblit` (0x4CD4E0), `action_tectonic` (0x4CE210), `action_fungal` (0x4CE790)
and `action_give` (0x4CEDE0) are in veh.cpp, with three helpers: `missile_shot_down()` (the
shoot-down step both payloads share), `give_proto()` (the receiver's design for a given unit) and
`give_units()` (the conversion, following the units through `veh_kill`). New exe pointers:
`FX_play2` (0x446960) and `shoot_it` (0x4CDAF0, left in the exe). `Rules->tgl_oblit_base_atrocity`
is 0x949860, `TectonicDetonationCount` 0x946138.

**Kept from the original:**
- the tectonic ring's radius comes from the exe's RadiusRange copy;
- `propose_proto` gets the design's `unk_1` zero-extended;
- the payload notices name the nearest base of any owner.

**Deliberate changes from the v2.1 exe, chosen by Kiousu (2026-10-18)** (see "action_tectonic and
action_fungal payloads" and "action_give handover" below):
1. Both payloads read the attacker (and the fungal one its reactor) from the missile's unit slot
   after `kill()` freed it, i.e. from the next unit in the table. Now the faction that fired gets
   the credit, and the fungal payload uses the missile's own reactor.
2. A payload shot down was still offered to the territory owner, whose answer replaced the first.
   Now a payload once shot down stays down.
3. When proposing a design for a given unit disbanded units, the exe carried on with stale slot
   numbers and gave away the wrong unit. Now the units actually given go over: the list is taken
   first, and `veh_kill` renumbers the ids in `VehKillWatch`. This includes the rest of the cargo
   when the transport itself is disbanded on the way, so one gift is never split between the two
   factions.
4. Each unit given is counted against its new design (`proto_id_active`: the giver's
   count of the old design down, the receiver's of the new one up), as `give_a_base` counts units
   changing sides. In the exe a design just made for a given unit looked unused and was the one the
   next unit's proposal retired, disbanding the unit just given.

**Trigger group `unitfx`.** The exe's code (`OriginalEntry`) runs against the DLL from the same
snapshot.
- Recorded: the unitact recorders plus FX::play2, draw_radius, X_pop, base_kill (DLL entry), the
  terrain clears of `world_climate`, and `shoot_it`. `shoot_it` gets scripted answers, or the real
  one runs with orbital defense pods, or with a flechette defense where it looks. The clock is
  pinned.
- Payloads: a pinned copy of the exe (its post-`kill()` loads and its shot-down jump patched to the
  intended ones) must match the DLL. The shipped exe is compared with it, and each difference is
  counted and attributed.
- `action_give`: the exe is pinned for the counts (a stub on `propose_proto` counts the units given
  so far before each proposal) and must match the DLL exactly, except in runs that disband units.
  Those are checked against an identity rule (units followed by a tag in `Veh::unk_1`: sides,
  designs, notice), and the pinned exe's breaches are counted. Two constructed cases per save cover
  the counts and the disbanded transport.
- Inputs are read from the game as loaded.
- Compared: the snapshot, notice state and calls, the unitact extras, detonation counts, new units,
  prototypes, BungList, ProtoIDNewDesign and the design suggestion globals.

| test | Council.SAV | 13 sweep saves | mismatches |
|---|---|---|---|
| action_oblit | 2,820 | 18,354 | 0 |
| action_tectonic | 536 | 6,079 | 0 |
| action_fungal | 1,213 | 13,104 | 0 |
| action_give | 3,985 | 15,712 | 0 |

**How often the shipped game differs from the intended:**
- 3,088 of 6,615 tectonic cases: freed slot 2,209, territory owner 879. As loaded, the detonation
  count went to another faction in 455 of 1,014.
- 3,744 of 14,317 fungal cases: freed slot 2,033, territory owner 1,711.
- With a flechette defense alone, tectonic payloads went off 353 times as shipped and 302 as
  intended (of 531), fungal 652 and 535 (of 996).
- `action_give`: the missing counts changed the outcome in 110 runs. The shipped game disbanded
  given units the intended one keeps in 73 (64 natural, on Council.SAV and sw_bugged). The
  intended game disbands only in the constructed case, where the rest of the cargo goes over (9
  saves).

Logs: `regression-runs/2026-10-18-council-batch32-unitfx.log` and
`regression-runs/2026-10-18-sweep-batch32-unitfx.log`.

### Results

Whole suite (`-Tests ''`) on the merged 765-redirect tree, harness DLL with the focus guard:

| test | Council.SAV | 13 sweep saves | mismatches |
|---|---|---|---|
| action_go_to (scripted) | 7,725 | 28,500 | 0 |
| action_go_to (exe's Path::move) | 606 | 2,224 | 0 |
| action_road_to (tiles x techs) | 11,610 | 43,740 | 0 |
| action_road_to (paths and network) | 16,856 | 62,720 | 0 |
| action_road_to (exe's Path::find) | 720 | 2,700 | 0 |
| action_airdrop | 13,222 | 49,698 | 0 |
| action_airdrop, shipped exe | 3,005 (358 predicted) | 11,295 (2,331 predicted) | 0 unexplained |
| action_terraform | 4,885 | 40,246 | 0 |
| action_destroy | 4,604 | 27,603 | 0 |
| action_home | 8,930 | 15,260 | 0 |
| action_oblit | 2,820 | 18,354 | 0 |
| action_tectonic | 536 | 6,079 | 0 |
| action_fungal | 1,213 | 13,104 | 0 |
| action_give | 3,985 | 15,712 | 0 |

Every count equals the group's own run, and the batch's lines on Council.SAV are the groups' lines
word for word but one: the live `Path::find` pass of `action_road_to` took 470 steps and found no
path 250 times, against 472 and 248 in the group's run. That pass runs the exe's own path finder on
whatever state the earlier tests leave; the merged build run with the group's filter
(`unitmove unitact`) gives 472 and 248 again. The `unitact` tests, `veh_kill` and `unit_kill` have
batch 31's counts (`veh_kill` 3,546 and 13,212, so `VehKillWatch` changes nothing outside
`action_give`). Every other test was 0 on every save except `social_ai`'s known line on Council.SAV
and sw_bugged (faction 5, AI self, `bunglist_index` 34 vs 33). Every save's log has the focus
guard's one note (`FOCUS-GUARD ShowWindow(5) shown as ShowWindow(7)`). Boot test through the
message-driven UI: Council.SAV 289 -> 291 (29 s and 56 s), the game alive, nothing new in the live
game folder. Logs: `regression-runs/2026-10-18-council-batch32.log` and
`regression-runs/2026-10-18-sweep-batch32.log`.

Click pass (stage 1 runner, one pair) in two new folders made for the merge with `make_testdir.ps1`,
`G:\SMACX-opensmacx-test-mainref` and `-mainosx`, with the click-pass ini settings (`Windowed=1`,
`DirectDraw=0`): `clickpass.py reference` gave the reference the pristine exe with the twelve
approved rows (9f1e9875) and the merged harness DLL (f9716d32), and the OpenSMACX folder has the
765-row exe (06150821) and the same DLL. Pass `2026-10-18-1451` (report:
`regression-runs/2026-10-18-clickpass-765.md`): every reference recorded with its self-check, all
self-checks agreed, 10 pass (end_turn, idle and reports on Council.SAV, sw_aki2120.SAV and
sw_steam1.SAV, and save_game), in about 8 minutes.

## action_tectonic and action_fungal payloads, 2026-10-18 (deliberate change from the v2.1 exe, chosen by Kiousu)

When a tectonic or fungal payload goes off, the v2.1 exe gets two things wrong.

- Who fired it, and for a fungal payload how big its reactor is. The exe reads them from the
  missile's unit slot after the missile is used up; removing a unit moves every later unit down one
  slot, so the answer comes from the next unit in the game's unit list, usually the next unit built
  anywhere, of any faction (on the saves the next unit belongs to another faction in 32 % of slots
  on Council.SAV and up to 64 % on sw_morgan). A tectonic detonation is credited to that unit's
  owner: native life fights the credited faction harder and a human faction's bases take more
  ecological damage, so the attacker escapes it and someone else, possibly the victim, takes it, and
  the TECMOFIED notice names that faction. A fungal payload's fungus picks follow that unit's
  reactor (a Fusion missile followed by a Fission unit makes 2 picks instead of 4, by a Singularity
  unit 8), and whether the target's owner is shown the fungus, and the FUNGMOTIZED notice, follow
  that unit's owner.
- Whether it was shot down. The factions near the target try in turn to shoot it down (orbital
  defense pods, a flechette defense); once one succeeds the exe still asks the owner of the target's
  territory, and that answer replaces the first. A payload just shot down goes off anyway when a
  third faction without defenses owns the territory; a defender with pods spends a second
  interception (a human defender gets the satellite-sacrifice dialog again) and sees a second
  notice; with a flechette defense alone the stop chance is 25 % instead of 50 % when the defender
  owns the territory, and 50 % instead of 75 % when someone else does.

Kiousu chose: the faction that fired gets the credit and the fungal payload uses the missile's own
reactor (both read before the missile is removed), and a payload once shot down stays down. The
same rule is to apply to `planet_busting` (0x500B60, its shoot-down 0x500560) when it is converted.
That the re-ask is a defect was known: the base game's terran.exe (SMAC, 2002) carries an
unofficial patch that skips it (0x515879, a test/jne, and `nop nop` around a call to a stub at
0x6328C6); terranx v2.1 has no such fix (`planet_busting` at 0x500E7D falls into `whose_territory`
at 0x500E8D). Code: `action_tectonic` reads `Vehs[veh_id].faction_id` after `kill()` at
0x4CE5A9/0x4CE5E3/0x4CE61D (the notice) and 0x4CE679 (`TectonicDetonationCount`); `action_fungal`
reads `.proto_id` at 0x4CEA1B and `.faction_id` at 0x4CEB46 and 0x4CED04/40/7A; the shot-down
`break` at 0x4CE4E5 / 0x4CE9AC jumps to the `whose_territory` call instead of past it.

The test runs a pinned copy of the exe (its post-`kill()` loads and its shot-down jump patched to
the intended ones) against the DLL: 0 mismatches. The shipped exe against the pinned one, every
difference attributed, on the 14 saves: 3,088 of 6,615 tectonic cases differ (freed slot 2,209,
territory owner 879; with the save as loaded the detonation count went to a faction that didn't
fire in 455 of 1,014), 3,744 of 14,317 fungal cases (2,033 and 1,711); with a flechette defense
alone tectonic payloads went off 353 times as shipped and 302 as intended (of 531), fungal 652 and
535 (of 996). Both rows are `approved`.

## action_give handover, 2026-10-18 (deliberate change from the v2.1 exe, chosen by Kiousu)

Giving a unit to another faction gives the units sentried aboard it too. For each unit the receiver
may need a new design, and proposing one can retire one of the receiver's designs, which disbands
that design's units (all of them for a human receiver; for a computer one, those a human can see
outside a base). In the v2.1 exe:

- `action_give` doesn't count the given units against their new designs, so the design just made
  for an earlier unit of the same stack looks unused and is the one retired: the unit just given is
  disbanded.
- It walks the stack with the slot numbers from before the proposal. After a disbanding every later
  unit has moved down one slot, so it converts whichever unit moved into the slot (often a unit of
  another faction elsewhere on the map changes sides), follows that unit's stack, and leaves the
  real cargo or transport with the giver; the receiver's notice may name that other unit's design.
- When the transport itself is disbanded on the way, the rest of its cargo stays with the giver,
  even with the right slot numbers.

Kiousu chose: the units actually given go over (the list is taken first, and `veh_kill` renumbers
the ids in `VehKillWatch`, a three-slot list only `action_give` fills while it proposes a design);
the rest of the cargo goes to the receiver, so one gift is never split between the two factions;
and each unit given counts against its new design (`proto_id_active`: the giver's count of the old
design down, the receiver's of the new one up), as `give_a_base` counts units changing sides
(0x54D225-0x54D258), so given units survive. A disbanding can still happen, as ordinary
`propose_proto` behaviour, only when the receiver has no free design slot, no unused obsolete design
and no other design without units, and the next unit's design falls in the category of a design
just made for a given unit that is the least used of it; with full design slots that can also move
a disbanding onto the receiver's own units (the first unit of a gift already works this way).

The test runs the exe pinned for the counts (a stub on `propose_proto` counts the units given so far
before each proposal) against the DLL: equal except in runs that disband units, which are checked
against an identity rule (sides, designs, notice). The shipped game's missing counts changed the
outcome in 110 runs on the 14 saves (Council.SAV 15, sw_bugged 88, and a constructed case on 7
saves); it disbanded given units the intended game keeps in 73 (64 natural: Council.SAV 7, sw_bugged
57). The intended game disbands only in the constructed case, where the rest of the cargo goes over
(9 saves). The row is `approved`.

## action_airdrop notice, 2026-10-18 (deliberate change from the v2.1 exe, chosen by Kiousu)

When another faction makes an air drop or orbital insertion the player can see, the notice
"<faction> makes air drop near <base>!" names a base near the drop. The shipped exe asks
base_find() for the base nearest the drop that faction 1 owns or has seen (`push 1` at 0x4CCC4C;
say_loc() and every other caller pass the player at the controls). With no such base it reads the
name from `Bases[-1]`, an empty string on all 14 saves ("near !"). With the player in slot 1
nothing changes. A player in another slot (Morganites in slot 6 on six of the sweep saves) can be
shown a base faction 1 knows and they don't. Kiousu chose the intended behaviour: OpenSMACX names
the nearest base the player at the controls owns or has seen, and leaves the name blank on purpose
when there is none. The redirect row is marked `approved`.

The test runs the exe with that push made `push ecx` (FactionIDCurrent) against the DLL: 0
mismatches. Every no-base notice is counted; the exe's Bases[-1] read was empty in all of them
(Council 269, sweep 909, all in a constructed case with a player who owns and has seen no base).
The shipped exe differs from OpenSMACX on exactly the predicted inputs (Council 358 of 3,005, sweep
2,331 of 11,295, nothing else). The save's own player watching AI drops saw another base name in 2
of 18 notices on the sweep saves.

## action_terraform farm and raise/lower, 2026-10-18 (deliberate change from the v2.1 exe, chosen by Kiousu)

- **Farm on a farm:** a farm ordered on a land farm becomes a Soil Enricher only with the Soil
  Enricher's technology (Advanced Ecological Engineering), just as a road becomes a mag tube only
  with the Mag Tube's. The v2.1 exe checks the Farm's technology, which has none, so the change
  always happened. In play the player's order is already refused (HASFARM, by `Console::terraform`
  with the enricher's technology), and computer players ask for it only with the technology. The
  slip's only visible effect was the turn estimate shown next to "Farm" on a farm tile, which
  counted the enricher's work. Kiousu kept the corrected check (the original script.txt's #HASFARM
  states the intent, and Thinker 5.4 checks the enricher's technology too). In the tests: 473
  inputs, 407 with a different outcome.
- **Raise or lower land:** the change can lose units on the neighbouring tiles (ships where coastal
  sea is lifted into land; land units, and bases flooded, where land sinks below sea level). The
  former itself never drowns, since TOOLOW/SEATOOHIGH refuse that, and automated formers and the AI
  only raise. When a lost unit comes before the former in the unit table, another unit moves into
  the former's slot. The v2.1 exe reads that slot again, so the "terraforming complete" chime went
  by the other unit's order and automation. The DLL uses the former's own state and order. In the
  tests (completions forced, formers' own tiles included): 274 inputs, 130 with a different chime.

The test runs the exe with both slips fixed in its own code (one byte at 0x4C9BBE, wrappers on its
`world_raise_alt`/`world_lower_alt` calls that put the former's own state and order back into its
slot) against the DLL: 0 mismatches; the shipped exe against the fixed one counts the cases above
and fails on any other difference. The row is `approved`.

## action_home airbase distance, 2026-10-18 (deliberate change from the v2.1 exe, chosen by Kiousu)

An aircraft on alert (automated) heading back to its home airbase (its second waypoint) is checked
against that airbase's own distance. The v2.1 exe keeps the shortest distance found before it
(bases, carriers, air supply units and airbases scanned earlier; at 0x4CC255 the airbase path
stores x and y but not the distance, which the base path at 0x4CBE9A does), and that can be longer
or shorter than the airbase's. With a longer one, a human player's return order is cancelled with
its automation (the aircraft stays out and can run dry) and the AI's `air_power()` gives up; with a
shorter one, the low-fuel check waits too long. For a home base the exe records the base's own
distance. The right-click "go to nearest airbase" of air units (`Console::go_home`, 0x4D7DE0) calls
`action_home(veh, 0)`, the plain nearest search, like the go-home key; only the on-alert return
takes this path. In the tests: path taken in 2,156 inputs (mostly constructed: airbases and alert
orders planted next to aircraft), 101 with a different outcome (Council.SAV 48, sw_bugged 53); the
exe with a 4-instruction cave that also stores the distance matches the DLL. The row is `approved`.

## Units-lost messages, 2026-10-18 (kept as shipped, chosen by Kiousu)

The "units lost to terraforming" messages (DROWNMINE/DROWNYOURS) never show in the v2.1 exe:
`action_terraform` checks the record of the raise or lower after `world_climate()`, whose
`world_shorelines()` calls `world_alt_set()` for nearly every tile, and each call resets the record
(`MapBaseIdClosestSubmergedVeh`, 0x9B22BC). The tests found the player's units lost in 268 inputs
and the record still set at the check in none. Kiousu kept them silent, as shipped: reviving them
would be new behaviour (and would expose two more slips: the check also runs after condensers, and
the base is named by the terraformer's index, `Bases[-1]`, when it lost nothing).
