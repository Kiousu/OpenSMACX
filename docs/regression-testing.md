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
Deliberate change: the original seeds the alien energy grid from an uninitialized local unless
some treaty partner passed the commerce checks; the DLL starts it from zero. The test gives
alien factions a treaty partner so both sides start from the same value. Original behaviour
kept: the specialist value divides by the energy intake plus the specialists already added, and
`base_yield` writes new specialists from slot 0 while counting leftovers from the old total.
Boot test: the 624-redirect exe loaded Council.SAV and advanced the turn 289 -> 290 (38 s) with
the game alive.
