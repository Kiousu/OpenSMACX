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

struct GameSnapshot {
    PlayerData players[MaxPlayerNum];
    Base *bases;
    int base_count;
    Veh *vehs;
    int veh_count;
    Map *tiles;
    uint32_t tile_count;
    uint32_t ambience_flags;
    uint32_t draw_state;
    BOOL maps_revealed;
};

static void snap_alloc(GameSnapshot *s) {
    s->base_count = *BaseCurrentCount;
    s->veh_count = *VehCurrentCount;
    s->tile_count = *MapArea;
    s->bases = (Base *)malloc(sizeof(Base) * (s->base_count ? s->base_count : 1));
    s->vehs = (Veh *)malloc(sizeof(Veh) * (s->veh_count ? s->veh_count : 1));
    s->tiles = (Map *)malloc(sizeof(Map) * (s->tile_count ? s->tile_count : 1));
}

static void snap_free(GameSnapshot *s) {
    free(s->bases);
    free(s->vehs);
    free(s->tiles);
}

static void snap_take(GameSnapshot *s) {
    memcpy(s->players, PlayersData, sizeof(s->players));
    memcpy(s->bases, Bases, sizeof(Base) * s->base_count);
    memcpy(s->vehs, Vehs, sizeof(Veh) * s->veh_count);
    memcpy(s->tiles, *MapTiles, sizeof(Map) * s->tile_count);
    s->ambience_flags = *AmbienceFlags;
    s->draw_state = *UnkBitfield1;
    s->maps_revealed = *TradeMapsRevealedNew;
}

static void snap_restore(const GameSnapshot *s) {
    memcpy(PlayersData, s->players, sizeof(s->players));
    memcpy(Bases, s->bases, sizeof(Base) * s->base_count);
    memcpy(Vehs, s->vehs, sizeof(Veh) * s->veh_count);
    memcpy(*MapTiles, s->tiles, sizeof(Map) * s->tile_count);
    *AmbienceFlags = s->ambience_flags;
    *UnkBitfield1 = s->draw_state;
    *TradeMapsRevealedNew = s->maps_revealed;
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
        DLOG("  %s: Base[%u] +0x%X differs", label, (unsigned)(d / sizeof(Base)),
            (unsigned)(d % sizeof(Base)));
        return true;
    }
    if ((d = first_diff(o->vehs, n->vehs, sizeof(Veh) * o->veh_count)) != (size_t)-1) {
        DLOG("  %s: Veh[%u] +0x%X differs", label, (unsigned)(d / sizeof(Veh)),
            (unsigned)(d % sizeof(Veh)));
        return true;
    }
    if ((d = first_diff(o->tiles, n->tiles, sizeof(Map) * o->tile_count)) != (size_t)-1) {
        DLOG("  %s: Map tile %u +0x%X differs", label, (unsigned)(d / sizeof(Map)),
            (unsigned)(d % sizeof(Map)));
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
    logf("TOTAL mismatches: %d (%lu ms)", total, GetTickCount() - t0);
    fclose(regress_log);
    regress_log = NULL;
}
