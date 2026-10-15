/*
 * OpenSMACX - an open source clone of Sid Meier's Alpha Centauri.
 * Copyright (C) 2013-2021 Brendan Casey
 *
 * OpenSMACX is free software: you can redistribute it and / or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenSMACX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenSMACX. If not, see <http://www.gnu.org/licenses/>.
 */
#include "stdafx.h"
#include "temp.h"
#include "base.h"
#include "faction.h"
#include "map.h"
#include "veh.h"
#include "game.h"
#include "path.h"
#include "random.h"
#include "regress.h"

static const char *TRIGGER_FILE = "opensmacx_regress.txt";
static const char *LOG_FILE = "opensmacx_regress.log";

volatile LONG RegressPending = 0;
static HANDLE regress_thread = NULL;
static FILE *regress_log = NULL;
static char regress_filter[512];

static void logf(const char *fmt, ...) {
    if (!regress_log) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(regress_log, fmt, ap);
    va_end(ap);
    fputc('\n', regress_log);
    fflush(regress_log);
}

static BOOL test_enabled(const char *name) {
    return !regress_filter[0] || strstr(regress_filter, name) != NULL;
}

// A test that is also part of a named group (e.g. every world_* generator is in group "world").
static BOOL test_enabled(const char *group, const char *name) {
    return test_enabled(name) || (regress_filter[0] && strstr(regress_filter, group) != NULL);
}

// ------------------------------------------------------------------------------------------
// Individual tests. Each returns the number of mismatches and logs details for the first few.
// ------------------------------------------------------------------------------------------

static const int MAX_DETAIL = 25;

static int test_value_of_base() {
    int compared = 0, mismatches = 0, unstable_orig = 0, unstable_new = 0;
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        for (uint32_t req = 1; req < MaxPlayerNum; req++) {
            for (uint32_t res = 1; res < MaxPlayerNum; res++) {
                for (uint32_t overmatch = 0; overmatch <= 2; overmatch++) {
                    for (BOOL tgl = 0; tgl <= 1; tgl++) {
                        int b = value_of_base_OG(base_id, req, res, overmatch, tgl);
                        int b2 = value_of_base_OG(base_id, req, res, overmatch, tgl);
                        int a = value_of_base(base_id, req, res, overmatch, tgl);
                        int a2 = value_of_base(base_id, req, res, overmatch, tgl);
                        compared++;
                        if (b != b2) unstable_orig++;
                        if (a != a2) unstable_new++;
                        if (a != b) {
                            if (mismatches < MAX_DETAIL) {
                                logf("  value_of_base(%d, %u, %u, %u, %d): new=%d orig=%d",
                                    base_id, req, res, overmatch, tgl, a, b);
                            }
                            mismatches++;
                        }
                    }
                }
            }
        }
    }
    logf("value_of_base: %d compared, %d mismatches, unstable orig=%d new=%d", compared, mismatches, unstable_orig, unstable_new);
    return mismatches;
}

static int test_best_defender() {
    int compared = 0, mismatches = 0, unstable_orig = 0, unstable_new = 0;
    int veh_count = *VehCurrentCount;
    // every defender against a spread of attackers: stride keeps the pair count bounded
    int stride = veh_count > 64 ? veh_count / 64 : 1;
    for (int def = 0; def < veh_count; def++) {
        for (int atk = -1; atk < veh_count; atk += (atk < 0 ? 1 : stride)) {
            if (atk == def) {
                continue;
            }
            for (BOOL art = 0; art <= 1; art++) {
                uint32_t b = best_defender_OG(def, atk, art);
                uint32_t b2 = best_defender_OG(def, atk, art);
                uint32_t a = best_defender(def, atk, art);
                uint32_t a2 = best_defender(def, atk, art);
                compared++;
                if (b != b2) unstable_orig++;
                if (a != a2) unstable_new++;
                if (a != b) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  best_defender(%d, %d, %d): new=%u orig=%u", def, atk, art, a, b);
                    }
                    mismatches++;
                }
            }
        }
    }
    logf("best_defender: %d compared, %d mismatches, unstable orig=%d new=%d", compared, mismatches, unstable_orig, unstable_new);
    return mismatches;
}

struct BattleSnapshot {
    int offense, defense;
    uint32_t mod_count[2];
    int modifier[2][4];
    char display[2][4][80];
    uint32_t morale[2];
};

static void battle_snapshot(BattleSnapshot *s, int offense, int defense) {
    s->offense = offense;
    s->defense = defense;
    memcpy(s->mod_count, VehBattleModCount, sizeof(s->mod_count));
    memcpy(s->modifier, VehBattleModifier, sizeof(s->modifier));
    memcpy(s->display, VehBattleDisplay, sizeof(s->display));
    memcpy(s->morale, VehBasicBattleMorale, sizeof(s->morale));
}

static int test_battle_compute() {
    int compared = 0, mismatches = 0;
    int veh_count = *VehCurrentCount;
    int stride = veh_count > 48 ? veh_count / 48 : 1;
    BattleSnapshot a, b, c, d;
    int unstable_orig = 0, unstable_new = 0;
    int by_type[3] = { 0, 0, 0 };
    for (int atk = 0; atk < veh_count; atk += stride) {
        for (int def = 0; def < veh_count; def += stride) {
            if (atk == def) {
                continue;
            }
            for (int type = 0; type <= 2; type++) {
                int off1 = 0, def1 = 0, off2 = 0, def2 = 0, off3 = 0, def3 = 0;
                battle_compute_OG(atk, def, &off2, &def2, type);
                battle_snapshot(&b, off2, def2);
                battle_compute_OG(atk, def, &off3, &def3, type);
                battle_snapshot(&c, off3, def3);
                battle_compute(atk, def, &off1, &def1, type);
                battle_snapshot(&a, off1, def1);
                battle_compute(atk, def, &off3, &def3, type);
                battle_snapshot(&d, off3, def3);
                compared++;
                if (memcmp(&b, &c, sizeof(b)) != 0) unstable_orig++;
                if (memcmp(&a, &d, sizeof(a)) != 0) unstable_new++;
                if (memcmp(&b, &c, sizeof(b)) != 0) {
                    continue; // original is not self-consistent here; do not count as mismatch
                }
                if (memcmp(&a, &b, sizeof(a)) != 0) {
                    by_type[type]++;
                    if (mismatches < MAX_DETAIL) {
                        logf("  battle_compute(%d, %d, type %d): new off=%d def=%d mods=%u/%u; "
                            "orig off=%d def=%d mods=%u/%u; display%s morale%s", atk, def, type,
                            a.offense, a.defense, a.mod_count[0], a.mod_count[1],
                            b.offense, b.defense, b.mod_count[0], b.mod_count[1],
                            memcmp(a.display, b.display, sizeof(a.display)) ? " DIFFERS" : " same",
                            memcmp(a.morale, b.morale, sizeof(a.morale)) ? " DIFFERS" : " same");
                    }
                    mismatches++;
                }
            }
        }
    }
    logf("battle_compute: %d compared, %d mismatches (orig-unstable pairs excluded), unstable orig=%d new=%d; by type: %d/%d/%d",
        compared, mismatches, unstable_orig, unstable_new, by_type[0], by_type[1], by_type[2]);
    return mismatches;
}

static int test_wants_to_attack() {
    int compared = 0, mismatches = 0, unstable_orig = 0, unstable_new = 0;
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        for (uint32_t tgt = 1; tgt < MaxPlayerNum; tgt++) {
            if (f == tgt) {
                continue;
            }
            for (int other = -1; other < (int)MaxPlayerNum; other++) {
                BOOL b = wants_to_attack_OG(f, tgt, other);
                BOOL b2 = wants_to_attack_OG(f, tgt, other);
                BOOL a = wants_to_attack(f, tgt, other);
                BOOL a2 = wants_to_attack(f, tgt, other);
                compared++;
                if (b != b2) unstable_orig++;
                if (a != a2) unstable_new++;
                if (b != b2) {
                    continue; // original is not self-consistent here; do not count as mismatch
                }
                if (a != b) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  wants_to_attack(%u, %u, %d): new=%d orig=%d", f, tgt, other, a, b);
                    }
                    mismatches++;
                }
            }
        }
    }
    logf("wants_to_attack: %d compared, %d mismatches, unstable orig=%d new=%d", compared, mismatches, unstable_orig, unstable_new);
    return mismatches;
}

// ------------------------------------------------------------------------------------------
// Snapshot / restore of the game state that the state-mutating functions touch.
// ------------------------------------------------------------------------------------------

static_assert(sizeof(PlayerData) == 0x20CC, "PlayerData size");
static_assert(sizeof(Base) == 0x134, "Base size");
static_assert(sizeof(Veh) == 0x34, "Veh size");
static_assert(sizeof(Map) == 0x2C, "Map size");
static_assert(sizeof(Landmark) == 0x28, "Landmark size");
static_assert(sizeof(Continent) == 0x1C, "Continent size");
static_assert(sizeof(Path) == 0x2C, "Path size");

struct GameSnapshot {
    PlayerData players[MaxPlayerNum];
    Base *bases;
    int base_count;
    Veh *vehs;
    int veh_count;
    Map *tiles;
    uint32_t tile_count;
    int base_count_global; // *BaseCurrentCount to restore (arrays always hold base_count entries)
    int veh_count_global;  // *VehCurrentCount to restore
    uint32_t ambience_flags;
    uint32_t draw_state;
    BOOL maps_revealed;
    // world generation state
    Landmark landmarks[MaxLandmarkNum];
    int landmark_count;
    Continent continents[MaxContinentNum];
    uint32_t brush1;
    uint32_t brush2;
    uint32_t mount_x;
    uint32_t mount_y;
    uint32_t game_state;
    int submerged_count[MaxPlayerNum];
    int submerged_veh[MaxPlayerNum];
    // pathing
    uint8_t path_obj[sizeof(Path)];
    int *path_table; // Paths->map_table_, MapArea entries
};

static void snap_alloc(GameSnapshot *s) {
    s->base_count = *BaseCurrentCount;
    s->veh_count = *VehCurrentCount;
    s->tile_count = *MapArea;
    s->bases = (Base *)malloc(sizeof(Base) * (s->base_count ? s->base_count : 1));
    s->vehs = (Veh *)malloc(sizeof(Veh) * (s->veh_count ? s->veh_count : 1));
    s->tiles = (Map *)malloc(sizeof(Map) * (s->tile_count ? s->tile_count : 1));
    s->path_table = (int *)malloc(sizeof(int) * (s->tile_count ? s->tile_count : 1));
}

static void snap_free(GameSnapshot *s) {
    free(s->bases);
    free(s->vehs);
    free(s->tiles);
    free(s->path_table);
}

static void snap_take(GameSnapshot *s) {
    memcpy(s->players, PlayersData, sizeof(s->players));
    memcpy(s->bases, Bases, sizeof(Base) * s->base_count);
    memcpy(s->vehs, Vehs, sizeof(Veh) * s->veh_count);
    memcpy(s->tiles, *MapTiles, sizeof(Map) * s->tile_count);
    s->base_count_global = *BaseCurrentCount;
    s->veh_count_global = *VehCurrentCount;
    s->ambience_flags = *AmbienceFlags;
    s->draw_state = *UnkBitfield1;
    s->maps_revealed = *TradeMapsRevealedNew;
    memcpy(s->landmarks, MapLandmark, sizeof(s->landmarks));
    s->landmark_count = *MapLandmarkCount;
    memcpy(s->continents, Continents, sizeof(s->continents));
    s->brush1 = *BrushVal1;
    s->brush2 = *BrushVal2;
    s->mount_x = *MountPlanetX;
    s->mount_y = *MountPlanetY;
    s->game_state = *GameState;
    memcpy(s->submerged_count, MapBaseSubmergedCount, sizeof(s->submerged_count));
    memcpy(s->submerged_veh, MapBaseIdClosestSubmergedVeh, sizeof(s->submerged_veh));
    memcpy(s->path_obj, Paths, sizeof(Path));
    int *table = *(int **)Paths; // Path::map_table_ is the first member
    if (table) {
        memcpy(s->path_table, table, sizeof(int) * s->tile_count);
    }
}

static void snap_restore(const GameSnapshot *s) {
    memcpy(PlayersData, s->players, sizeof(s->players));
    memcpy(Bases, s->bases, sizeof(Base) * s->base_count);
    memcpy(Vehs, s->vehs, sizeof(Veh) * s->veh_count);
    memcpy(*MapTiles, s->tiles, sizeof(Map) * s->tile_count);
    *BaseCurrentCount = s->base_count_global;
    *VehCurrentCount = s->veh_count_global;
    *AmbienceFlags = s->ambience_flags;
    *UnkBitfield1 = s->draw_state;
    *TradeMapsRevealedNew = s->maps_revealed;
    memcpy(MapLandmark, s->landmarks, sizeof(s->landmarks));
    *MapLandmarkCount = s->landmark_count;
    memcpy(Continents, s->continents, sizeof(s->continents));
    *BrushVal1 = s->brush1;
    *BrushVal2 = s->brush2;
    *MountPlanetX = s->mount_x;
    *MountPlanetY = s->mount_y;
    *GameState = s->game_state;
    memcpy(MapBaseSubmergedCount, s->submerged_count, sizeof(s->submerged_count));
    memcpy(MapBaseIdClosestSubmergedVeh, s->submerged_veh, sizeof(s->submerged_veh));
    memcpy(Paths, s->path_obj, sizeof(Path));
    int *table = *(int **)Paths;
    if (table) {
        memcpy(table, s->path_table, sizeof(int) * s->tile_count);
    }
}

static size_t first_diff(const void *a, const void *b, size_t n) {
    const uint8_t *x = (const uint8_t *)a, *y = (const uint8_t *)b;
    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i]) {
            return i;
        }
    }
    return (size_t)-1;
}

/*
Compare two snapshots taken after the original and after the decompiled version ran from the
same baseline. Returns true if they differ; logs the first differing region and location.
*/
static BOOL snap_differs(const GameSnapshot *o, const GameSnapshot *n, const char *label) {
#define DLOG(...) do { if (label) logf(__VA_ARGS__); } while (0)
    size_t d;
    if ((d = first_diff(o->players, n->players, sizeof(o->players))) != (size_t)-1) {
        DLOG("  %s: PlayerData[%u] +0x%X differs (orig %02X new %02X)", label,
            (unsigned)(d / sizeof(PlayerData)), (unsigned)(d % sizeof(PlayerData)),
            ((const uint8_t *)o->players)[d], ((const uint8_t *)n->players)[d]);
        return true;
    }
    if ((d = first_diff(o->bases, n->bases, sizeof(Base) * o->base_count)) != (size_t)-1) {
        DLOG("  %s: Base[%u] +0x%X differs (orig %02X new %02X)", label, (unsigned)(d / sizeof(Base)),
            (unsigned)(d % sizeof(Base)), ((const uint8_t *)o->bases)[d], ((const uint8_t *)n->bases)[d]);
        return true;
    }
    if ((d = first_diff(o->vehs, n->vehs, sizeof(Veh) * o->veh_count)) != (size_t)-1) {
        DLOG("  %s: Veh[%u] +0x%X differs (orig %02X new %02X)", label, (unsigned)(d / sizeof(Veh)),
            (unsigned)(d % sizeof(Veh)), ((const uint8_t *)o->vehs)[d], ((const uint8_t *)n->vehs)[d]);
        return true;
    }
    if ((d = first_diff(o->tiles, n->tiles, sizeof(Map) * o->tile_count)) != (size_t)-1) {
        DLOG("  %s: Map tile %u +0x%X differs (orig %02X new %02X)", label, (unsigned)(d / sizeof(Map)),
            (unsigned)(d % sizeof(Map)), ((const uint8_t *)o->tiles)[d], ((const uint8_t *)n->tiles)[d]);
        return true;
    }
    if ((d = first_diff(o->landmarks, n->landmarks, sizeof(o->landmarks))) != (size_t)-1
        || o->landmark_count != n->landmark_count) {
        DLOG("  %s: Landmark[%u] +0x%X differs (count %d/%d)", label,
            (unsigned)(d / sizeof(Landmark)), (unsigned)(d % sizeof(Landmark)),
            o->landmark_count, n->landmark_count);
        return true;
    }
    if ((d = first_diff(o->continents, n->continents, sizeof(o->continents))) != (size_t)-1) {
        DLOG("  %s: Continent[%u] +0x%X differs", label, (unsigned)(d / sizeof(Continent)),
            (unsigned)(d % sizeof(Continent)));
        return true;
    }
    if (o->brush1 != n->brush1 || o->brush2 != n->brush2 || o->mount_x != n->mount_x
        || o->mount_y != n->mount_y || o->game_state != n->game_state
        || memcmp(o->submerged_count, n->submerged_count, sizeof(o->submerged_count))
        || memcmp(o->submerged_veh, n->submerged_veh, sizeof(o->submerged_veh))) {
        DLOG("  %s: world globals differ (brush %u,%u/%u,%u mount %u,%u/%u,%u state %X/%X)", label,
            o->brush1, o->brush2, n->brush1, n->brush2, o->mount_x, o->mount_y, n->mount_x,
            n->mount_y, o->game_state, n->game_state);
        return true;
    }
    if ((d = first_diff(o->path_obj, n->path_obj, sizeof(o->path_obj))) != (size_t)-1) {
        DLOG("  %s: Path object +0x%X differs", label, (unsigned)d);
        return true;
    }
    if ((d = first_diff(o->path_table, n->path_table, sizeof(int) * o->tile_count)) != (size_t)-1) {
        DLOG("  %s: Path table entry %u differs (orig %d new %d)", label, (unsigned)(d / 4),
            o->path_table[d / 4], n->path_table[d / 4]);
        return true;
    }
    if (o->base_count_global != n->base_count_global || o->veh_count_global != n->veh_count_global) {
        DLOG("  %s: counts differ (bases %d/%d vehs %d/%d)", label, o->base_count_global,
            n->base_count_global, o->veh_count_global, n->veh_count_global);
        return true;
    }
    if (o->ambience_flags != n->ambience_flags || o->draw_state != n->draw_state
        || o->maps_revealed != n->maps_revealed) {
        DLOG("  %s: globals differ (ambience %X/%X draw %X/%X revealed %d/%d)", label,
            o->ambience_flags, n->ambience_flags, o->draw_state, n->draw_state,
            o->maps_revealed, n->maps_revealed);
        return true;
    }
    return false;
#undef DLOG
}

typedef void (*diplo_flag_fn)(int, int, uint32_t);

/*
Run one treaty/agenda style function (faction, faction, flags) over every faction pair and
every single flag bit plus a few combinations, comparing the resulting state.
*/
static int test_diplo_flag_fn(const char *name, diplo_flag_fn fn_new, diplo_flag_fn fn_orig) {
    static const uint32_t combos[] = { 0x7, 0x17, 0x3, 0x11, 0x80000108, 0x1100000, 0x8040,
        0xFFFFFFFF };
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0, effective = 0;
    char label[96];
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            for (int k = 0; k < 32 + (int)(sizeof(combos) / sizeof(combos[0])); k++) {
                uint32_t flags = k < 32 ? (1u << k) : combos[k - 32];
                snap_restore(&base);
                fn_orig(f1, f2, flags);
                snap_take(&after_orig);
                if (snap_differs(&base, &after_orig, NULL)) {
                    effective++;
                }
                snap_restore(&base);
                fn_new(f1, f2, flags);
                snap_take(&after_new);
                compared++;
                sprintf_s(label, "%s(%d, %d, 0x%X)", name, f1, f2, flags);
                if (snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL)) {
                    mismatches++;
                }
            }
        }
    }
    snap_restore(&base);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("%s: %d compared (%d changed state), %d mismatches", name, compared, effective, mismatches);
    return mismatches;
}

static int test_treaty_on() { return test_diplo_flag_fn("treaty_on", treaty_on, treaty_on_OG); }
static int test_treaty_off() { return test_diplo_flag_fn("treaty_off", treaty_off, treaty_off_OG); }
static int test_agenda_on() { return test_diplo_flag_fn("agenda_on", agenda_on, agenda_on_OG); }
static int test_agenda_off() { return test_diplo_flag_fn("agenda_off", agenda_off, agenda_off_OG); }

static int test_trade_maps() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0, effective = 0;
    char label[64];
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            snap_restore(&base);
            trade_maps_OG(f1, f2);
            snap_take(&after_orig);
            if (snap_differs(&base, &after_orig, NULL)) {
                effective++;
            }
            snap_restore(&base);
            trade_maps(f1, f2);
            snap_take(&after_new);
            compared++;
            sprintf_s(label, "trade_maps(%d, %d)", f1, f2);
            if (snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL)) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("trade_maps: %d compared (%d changed state), %d mismatches", compared, effective, mismatches);
    return mismatches;
}

static int test_do_withdrawal() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0, effective = 0, moves = 0;
    char label[64];
    for (int owner = 1; owner < MaxPlayerNum; owner++) {
        for (int f = 1; f < MaxPlayerNum; f++) {
            if (owner == f) {
                continue;
            }
            snap_restore(&base);
            int r_orig = do_withdrawal_OG(owner, f);
            snap_take(&after_orig);
            if (r_orig) {
                effective++;
                moves += r_orig;
            }
            snap_restore(&base);
            int r_new = do_withdrawal(owner, f);
            snap_take(&after_new);
            compared++;
            sprintf_s(label, "do_withdrawal(%d, %d)", owner, f);
            BOOL bad = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
            if (r_orig != r_new) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: return orig=%d new=%d", label, r_orig, r_new);
                }
                bad = true;
            }
            if (bad) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("do_withdrawal: %d compared (%d moved units, %d moves total), %d mismatches", compared, effective, moves, mismatches);
    return mismatches;
}

static int test_world_validate() {
    BOOL a = world_validate();
    BOOL b = world_validate_OG();
    logf("world_validate: 1 compared, %d mismatches (new=%d orig=%d)", a != b, a, b);
    return a != b;
}

// ------------------------------------------------------------------------------------------
// Snapshot harness: baseline + run original + run new + compare, as a reusable object.
// ------------------------------------------------------------------------------------------

struct SnapHarness {
    const char *name;
    GameSnapshot base;
    GameSnapshot after_orig;
    GameSnapshot after_new;
    int compared;
    int mismatches;
    int effective;

    explicit SnapHarness(const char *test_name)
        : name(test_name), compared(0), mismatches(0), effective(0) {
        snap_alloc(&base);
        snap_alloc(&after_orig);
        snap_alloc(&after_new);
        snap_take(&base);
    }
    // Run with no bases and no units, as during world generation: the live alt_set() otherwise
    // submerges bases, kills units and throws modal popups. Arrays stay intact and the real
    // counts come back when the harness is destroyed.
    void empty_world() {
        saved_base_count = base.base_count_global;
        saved_veh_count = base.veh_count_global;
        base.base_count_global = 0;
        base.veh_count_global = 0;
    }
    int saved_base_count = -1;
    int saved_veh_count = -1;
    ~SnapHarness() {
        if (saved_base_count >= 0) {
            base.base_count_global = saved_base_count;
            base.veh_count_global = saved_veh_count;
        }
        snap_restore(&base);
        snap_free(&base);
        snap_free(&after_orig);
        snap_free(&after_new);
    }
    // Run the original from the baseline, then the new version from the same baseline, and
    // compare the resulting state. Returns true if a mismatch was counted.
    template <typename FO, typename FN>
    BOOL compare(const char *label, FO run_orig, FN run_new) {
        snap_restore(&base);
        run_orig();
        snap_take(&after_orig);
        if (snap_differs(&base, &after_orig, NULL)) {
            effective++;
        }
        snap_restore(&base);
        run_new();
        snap_take(&after_new);
        compared++;
        if (snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL)) {
            mismatches++;
            return true;
        }
        return false;
    }
    int report() {
        logf("%s: %d compared (%d changed state), %d mismatches", name, compared, effective,
            mismatches);
        return mismatches;
    }
};

// ------------------------------------------------------------------------------------------
// Upstream "Complete - testing" / WIP functions with side effects
// ------------------------------------------------------------------------------------------

static int test_base_mark() {
    SnapHarness h("base_mark");
    char label[64];
    for (int i = 0; i < *BaseCurrentCount; i++) {
        sprintf_s(label, "base_mark(%d)", i);
        h.compare(label, [&] { base_mark_OG(i); }, [&] { base_mark(i); });
    }
    return h.report();
}

static int test_base_psych() {
    SnapHarness h("base_psych");
    uint32_t stage_saved = *BaseUpkeepStage;
    int selected_saved = *BaseIDCurrentSelected;
    Base *current_saved = *BaseCurrent;
    char label[64];
    for (uint32_t stage = 0; stage < 2; stage++) { // stage 1 enables the full psych loop
        for (int i = 0; i < *BaseCurrentCount; i++) {
            sprintf_s(label, "base_psych(base %d, upkeep stage %u)", i, stage);
            h.compare(label,
                [&] { *BaseUpkeepStage = stage; set_base(i); base_psych_OG(); },
                [&] { *BaseUpkeepStage = stage; set_base(i); base_psych(); });
        }
    }
    *BaseUpkeepStage = stage_saved;
    *BaseIDCurrentSelected = selected_saved;
    *BaseCurrent = current_saved;
    return h.report();
}

static int test_enemy_capabilities() {
    SnapHarness h("enemy_capabilities");
    char label[64];
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        sprintf_s(label, "enemy_capabilities(%u)", f);
        h.compare(label, [&] { enemy_capabilities_OG(f); }, [&] { enemy_capabilities(f); });
    }
    // the game runs every faction in turn, each seeing the previous results
    h.compare("enemy_capabilities(1..7 in sequence)",
        [&] { for (uint32_t f = 1; f < MaxPlayerNum; f++) { enemy_capabilities_OG(f); } },
        [&] { for (uint32_t f = 1; f < MaxPlayerNum; f++) { enemy_capabilities(f); } });
    return h.report();
}

static int test_social_ai() {
    SnapHarness h("social_ai");
    char label[96];
    SocialCategory out_orig;
    SocialCategory out_new;
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        if (!is_human(f)) {
            // AI deciding for itself: writes the pending SE choice and pays for the change
            sprintf_s(label, "social_ai(%u, AI self)", f);
            h.compare(label, [&] { social_ai_OG(f, 0, 0, 0, 0, NULL); },
                [&] { social_ai(f, 0, 0, 0, 0, NULL); });
        }
        // advisory mode: 16 priority combinations plus the >100 "optimize one effect" form
        for (int combo = 0; combo < 16 + 10; combo++) {
            int growth, tech, wealth, power;
            if (combo < 16) {
                growth = combo & 1;
                tech = (combo >> 1) & 1;
                wealth = (combo >> 2) & 1;
                power = (combo >> 3) & 1;
            } else {
                growth = 101 + (combo - 16);
                tech = wealth = power = 0;
            }
            sprintf_s(label, "social_ai(%u, %d, %d, %d, %d, &out)", f, growth, tech, wealth, power);
            memset(&out_orig, 0xFF, sizeof(out_orig));
            memset(&out_new, 0xFF, sizeof(out_new));
            BOOL bad = h.compare(label,
                [&] { social_ai_OG(f, growth, tech, wealth, power, &out_orig); },
                [&] { social_ai(f, growth, tech, wealth, power, &out_new); });
            if (!bad && memcmp(&out_orig, &out_new, sizeof(out_orig))) {
                if (h.mismatches < MAX_DETAIL) {
                    logf("  %s: output orig=%u/%u/%u/%u new=%u/%u/%u/%u", label,
                        out_orig.politics, out_orig.economics, out_orig.values, out_orig.future,
                        out_new.politics, out_new.economics, out_new.values, out_new.future);
                }
                h.mismatches++;
            }
        }
    }
    return h.report();
}

static int test_path_sensors() {
    SnapHarness h("Path::sensors");
    char label[96];
    int found = 0;
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        int seen = 0;
        for (int b = 0; b < *BaseCurrentCount; b++) {
            if (Bases[b].faction_id_current != f || (seen++ % 4) != 0) {
                continue; // every 4th base of each faction
            }
            int x_orig = Bases[b].x, y_orig = Bases[b].y, x_new = x_orig, y_new = y_orig;
            BOOL r_orig = false, r_new = false;
            sprintf_s(label, "Path::sensors(%u, base %d at %d,%d)", f, b, x_orig, y_orig);
            BOOL bad = h.compare(label,
                [&] { r_orig = Path_sensors_OG(Paths, NULL, f, &x_orig, &y_orig); },
                [&] { r_new = Paths->sensors(f, &x_new, &y_new); });
            if (!r_orig) {
                found++; // returns false when it found a sensor site
            }
            if (!bad && (r_orig != r_new || x_orig != x_new || y_orig != y_new)) {
                if (h.mismatches < MAX_DETAIL) {
                    logf("  %s: result orig=%d (%d,%d) new=%d (%d,%d)", label, r_orig, x_orig,
                        y_orig, r_new, x_new, y_new);
                }
                h.mismatches++;
            }
        }
    }
    logf("Path::sensors: %d of %d calls found a site", found, h.compared);
    return h.report();
}

// ------------------------------------------------------------------------------------------
// World generation chain. Both sides draw from the same generators (rnd() and random() are
// already redirected; the exe's own rand() is reseeded through my_srand), so reseeding before
// each run makes the two sides deterministic and comparable on the live map.
// ------------------------------------------------------------------------------------------

static void reseed(uint32_t seed) {
    my_srand(seed);
    random_reseed(seed);
}

// k-th of n evenly spaced tiles across the map
static void sample_tile(int k, int n, int *x, int *y) {
    uint32_t idx = (uint32_t)(((uint64_t)k * *MapArea) / n);
    *y = idx / *MapLongitude;
    *x = (idx % *MapLongitude) * 2 + (*y & 1);
}

struct WorldXYFn {
    const char *name;
    func_xy *orig;
    func_xy *neu;
};

static int test_world_xy() {
    static const WorldXYFn fns[] = {
        { "world_raise_alt", world_raise_alt_OG, world_raise_alt },
        { "world_lower_alt", world_lower_alt_OG, world_lower_alt },
        { "world_crater", world_crater_OG, world_crater },
        { "world_monsoon", world_monsoon_OG, world_monsoon },
        { "world_sargasso", world_sargasso_OG, world_sargasso },
        { "world_ruin", world_ruin_OG, world_ruin },
        { "world_dune", world_dune_OG, world_dune },
        { "world_diamond", world_diamond_OG, world_diamond },
        { "world_fresh", world_fresh_OG, world_fresh },
        { "world_borehole", world_borehole_OG, world_borehole },
        { "world_temple", world_temple_OG, world_temple },
        { "world_unity", world_unity_OG, world_unity },
        { "world_fossil", world_fossil_OG, world_fossil },
        { "world_canyon", world_canyon_OG, world_canyon },
        { "world_mesa", world_mesa_OG, world_mesa },
        { "world_ridge", world_ridge_OG, world_ridge },
        { "world_geothermal", world_geothermal_OG, world_geothermal },
    };
    int total = 0;
    char label[96];
    for (size_t i = 0; i < sizeof(fns) / sizeof(fns[0]); i++) {
        if (!test_enabled("world", fns[i].name)) {
            continue;
        }
        SnapHarness h(fns[i].name);
        h.empty_world();
        const int samples = 24;
        for (int k = 0; k < samples; k++) {
            int x, y;
            sample_tile(k, samples, &x, &y);
            sprintf_s(label, "%s(%d, %d)", fns[i].name, x, y);
            uint32_t seed = k + 1;
            h.compare(label, [&] { reseed(seed); fns[i].orig(x, y); },
                [&] { reseed(seed); fns[i].neu(x, y); });
        }
        for (uint32_t seed = 1; seed <= 3; seed++) { // random placement
            sprintf_s(label, "%s(-1, -1) seed %u", fns[i].name, seed);
            h.compare(label, [&] { reseed(seed); fns[i].orig(-1, -1); },
                [&] { reseed(seed); fns[i].neu(-1, -1); });
        }
        total += h.report();
    }
    return total;
}

static int test_world_volcano() {
    SnapHarness h("world_volcano");
    h.empty_world();
    char label[96];
    const int samples = 24;
    for (BOOL not_landmark = 0; not_landmark < 2; not_landmark++) {
        for (int k = 0; k < samples; k++) {
            int x, y;
            sample_tile(k, samples, &x, &y);
            sprintf_s(label, "world_volcano(%d, %d, %d)", x, y, not_landmark);
            uint32_t seed = k + 1;
            h.compare(label, [&] { reseed(seed); world_volcano_OG(x, y, not_landmark); },
                [&] { reseed(seed); world_volcano(x, y, not_landmark); });
        }
        for (uint32_t seed = 1; seed <= 3; seed++) {
            sprintf_s(label, "world_volcano(-1, -1, %d) seed %u", not_landmark, seed);
            h.compare(label, [&] { reseed(seed); world_volcano_OG(-1, -1, not_landmark); },
                [&] { reseed(seed); world_volcano(-1, -1, not_landmark); });
        }
    }
    return h.report();
}

static int test_world_alt_set() {
    SnapHarness h("world_alt_set");
    h.empty_world();
    char label[96];
    const int samples = 16;
    for (int k = 0; k < samples; k++) {
        int x, y;
        sample_tile(k, samples, &x, &y);
        for (int alt = 0; alt <= 6; alt++) {
            for (BOOL both = 0; both < 2; both++) {
                sprintf_s(label, "world_alt_set(%d, %d, %d, %d)", x, y, alt, both);
                uint32_t seed = k * 20 + alt * 2 + both;
                h.compare(label, [&] { reseed(seed); world_alt_set_OG(x, y, alt, both); },
                    [&] { reseed(seed); world_alt_set(x, y, alt, both); });
            }
        }
    }
    return h.report();
}

static int test_brush() {
    SnapHarness h("brush");
    h.empty_world();
    char label[96];
    const int samples = 16;
    for (int k = 0; k < samples; k++) {
        int x, y;
        sample_tile(k, samples, &x, &y);
        for (int alt = 1; alt <= 5; alt++) {
            sprintf_s(label, "brush(%d, %d, %d)", x, y, alt);
            uint32_t seed = k * 8 + alt;
            h.compare(label, [&] { reseed(seed); brush_OG(x, y, alt); },
                [&] { reseed(seed); brush(x, y, alt); });
        }
    }
    return h.report();
}

static int test_paint_land() {
    SnapHarness h("paint_land");
    h.empty_world();
    char label[96];
    const int samples = 8;
    for (int k = 0; k < samples; k++) {
        int x, y;
        sample_tile(k, samples, &x, &y);
        for (uint32_t alt = 3; alt <= 4; alt++) {
            for (uint32_t radius = 2; radius <= 6; radius += 4) {
                sprintf_s(label, "paint_land(%d, %d, %u, %u)", x, y, alt, radius);
                uint32_t seed = k * 7 + alt + radius;
                h.compare(label, [&] { reseed(seed); paint_land_OG(x, y, alt, radius); },
                    [&] { reseed(seed); paint_land(x, y, alt, radius); });
            }
        }
    }
    return h.report();
}

static int test_build_continent() {
    SnapHarness h("build_continent");
    h.empty_world();
    char label[96];
    static const uint32_t sizes[] = { 1, 4, 16, 64 };
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        for (uint32_t seed = 1; seed <= 2; seed++) {
            sprintf_s(label, "build_continent(%u) seed %u", sizes[i], seed);
            h.compare(label, [&] { reseed(seed); build_continent_OG(sizes[i]); },
                [&] { reseed(seed); build_continent(sizes[i]); });
        }
    }
    return h.report();
}

static int test_build_hills() {
    SnapHarness h("build_hills");
    h.empty_world();
    char label[96];
    for (int alt = 4; alt <= 6; alt++) {
        for (uint32_t seed = 1; seed <= 2; seed++) {
            sprintf_s(label, "build_hills(%d) seed %u", alt, seed);
            h.compare(label, [&] { reseed(seed); build_hills_OG(alt); },
                [&] { reseed(seed); build_hills(alt); });
        }
    }
    return h.report();
}

struct WorldVoidFn {
    const char *name;
    func_void *orig;
    func_void *neu;
};

static int test_world_void() {
    static const WorldVoidFn fns[] = {
        { "world_riverbeds", world_riverbeds_OG, world_riverbeds },
        { "world_temperature", world_temperature_OG, world_temperature },
        { "world_analysis", world_analysis_OG, world_analysis },
        { "world_polar_caps", world_polar_caps_OG, world_polar_caps },
        { "world_linearize_contours", world_linearize_contours_OG, world_linearize_contours },
        { "world_landmarks", world_landmarks_OG, world_landmarks },
    };
    int total = 0;
    char label[96];
    for (size_t i = 0; i < sizeof(fns) / sizeof(fns[0]); i++) {
        if (!test_enabled("world", fns[i].name)) {
            continue;
        }
        SnapHarness h(fns[i].name);
        h.empty_world();
        for (uint32_t seed = 1; seed <= 3; seed++) {
            sprintf_s(label, "%s() seed %u", fns[i].name, seed);
            h.compare(label, [&] { reseed(seed); fns[i].orig(); },
                [&] { reseed(seed); fns[i].neu(); });
        }
        total += h.report();
    }
    return total;
}

// ------------------------------------------------------------------------------------------

static DWORD WINAPI regress_watcher(LPVOID) {
    for (;;) {
        Sleep(1000);
        if (!RegressPending && GetFileAttributesA(TRIGGER_FILE) != INVALID_FILE_ATTRIBUTES) {
            InterlockedExchange(&RegressPending, 1);
        }
    }
}

void regress_init() {
    if (!regress_thread) {
        regress_thread = CreateThread(NULL, 0, regress_watcher, NULL, 0, NULL);
    }
}

void regress_run() {
    InterlockedExchange(&RegressPending, 0);
    regress_filter[0] = 0;
    FILE *trig = fopen(TRIGGER_FILE, "r");
    if (trig) {
        size_t n = fread(regress_filter, 1, sizeof(regress_filter) - 1, trig);
        regress_filter[n] = 0;
        fclose(trig);
        while (n && isspace((unsigned char)regress_filter[n - 1])) { // whitespace-only = all tests
            regress_filter[--n] = 0;
        }
    }
    DeleteFileA(TRIGGER_FILE);
    regress_log = fopen(LOG_FILE, "w");
    if (!regress_log) {
        return;
    }
    DWORD t0 = GetTickCount();
    logf("OpenSMACX regression run: turn %d, %d bases, %d vehs, filter='%s'",
        *TurnCurrentNum, *BaseCurrentCount, *VehCurrentCount, regress_filter);
    int total = 0;
    if (test_enabled("value_of_base")) total += test_value_of_base();
    if (test_enabled("best_defender")) total += test_best_defender();
    if (test_enabled("battle_compute")) total += test_battle_compute();
    if (test_enabled("wants_to_attack")) total += test_wants_to_attack();
    if (test_enabled("world_validate")) total += test_world_validate();
    if (test_enabled("treaty_on")) total += test_treaty_on();
    if (test_enabled("treaty_off")) total += test_treaty_off();
    if (test_enabled("agenda_on")) total += test_agenda_on();
    if (test_enabled("agenda_off")) total += test_agenda_off();
    if (test_enabled("trade_maps")) total += test_trade_maps();
    if (test_enabled("do_withdrawal")) total += test_do_withdrawal();
    if (test_enabled("base_mark")) total += test_base_mark();
    if (test_enabled("base_psych")) total += test_base_psych();
    if (test_enabled("enemy_capabilities")) total += test_enemy_capabilities();
    if (test_enabled("social_ai")) total += test_social_ai();
    if (test_enabled("Path::sensors") || test_enabled("path_sensors")) total += test_path_sensors();
    total += test_world_xy();
    if (test_enabled("world", "world_volcano")) total += test_world_volcano();
    if (test_enabled("world", "world_alt_set")) total += test_world_alt_set();
    if (test_enabled("world", "brush")) total += test_brush();
    if (test_enabled("world", "paint_land")) total += test_paint_land();
    if (test_enabled("world", "build_continent")) total += test_build_continent();
    if (test_enabled("world", "build_hills")) total += test_build_hills();
    total += test_world_void();
    logf("TOTAL mismatches: %d (%lu ms)", total, GetTickCount() - t0);
    fclose(regress_log);
    regress_log = NULL;
}
