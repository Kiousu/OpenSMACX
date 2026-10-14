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
    logf("TOTAL mismatches: %d (%lu ms)", total, GetTickCount() - t0);
    fclose(regress_log);
    regress_log = NULL;
}
