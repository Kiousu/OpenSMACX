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
#include "technology.h"
#include "council.h"
#include "probe.h"
#include "alpha.h"
#include "net.h"
#include "iat.h"
#include "clickpass.h"
#include "regress.h"

#ifdef OPENSMACX_REGRESS

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

// Crash guard: a test that faults (access violation in either version) is logged with the
// faulting address and the last input the test announced, and the suite moves on.
static char regress_ctx[160];
#define REGRESS_CTX(...) sprintf_s(regress_ctx, __VA_ARGS__)
static int regress_crash_filter(EXCEPTION_POINTERS *ep, const char *name) {
    void *addr = ep->ExceptionRecord->ExceptionAddress;
    HMODULE mod = NULL;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)addr, &mod);
    char mod_name[MAX_PATH] = "?";
    if (mod) {
        GetModuleFileNameA(mod, mod_name, sizeof(mod_name));
    }
    logf("CRASH in %s: exception %08X at %p (%s+%X), last input: %s", name,
        ep->ExceptionRecord->ExceptionCode, addr, strrchr(mod_name, '\\') ? strrchr(mod_name, '\\') + 1 : mod_name,
        (unsigned)((uintptr_t)addr - (uintptr_t)mod), regress_ctx);
    CONTEXT *c = ep->ContextRecord;
    logf("  access %s %08X; eax %08X ebx %08X ecx %08X edx %08X esi %08X edi %08X ebp %08X esp %08X",
        ep->ExceptionRecord->ExceptionInformation[0] ? "write" : "read",
        (unsigned)ep->ExceptionRecord->ExceptionInformation[1], c->Eax, c->Ebx, c->Ecx, c->Edx,
        c->Esi, c->Edi, c->Ebp, c->Esp);
    // code addresses on the stack near esp: the callers
    char chain[400] = "";
    uint32_t *sp = (uint32_t *)c->Esp;
    for (int i = 0; i < 64 && strlen(chain) < 360; i++) {
        uint32_t v = sp[i];
        HMODULE m2 = NULL;
        if ((v >= 0x401000 && v < 0x700000)
            || (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCSTR)(uintptr_t)v, &m2) && m2 == mod)) {
            char item[32];
            sprintf_s(item, " %s%X", m2 == mod ? "dll+" : "exe:", m2 == mod ? (unsigned)(v - (uintptr_t)mod) : v);
            strcat_s(chain, item);
        }
    }
    logf("  code pointers on stack:%s", chain);
    logf("  Vehs[-1].proto_id %d faction %u", Vehs[-1].proto_id, Vehs[-1].faction_id);
    return EXCEPTION_EXECUTE_HANDLER;
}
// Hang watchdog (input-free runs): run_guarded() notes which test runs since when; a watchdog
// thread ends the game when one test runs longer than AutoHangSeconds, logging the test and its
// last announced input, so a stuck run costs minutes instead of waiting for someone to notice.
static const char *volatile RegressTestName = NULL;
static volatile DWORD RegressTestStart = 0;
static int run_guarded(const char *name, int (*fn)()) {
    regress_ctx[0] = 0;
    RegressTestStart = GetTickCount();
    RegressTestName = name;
    __try {
        return fn();
    } __except (regress_crash_filter(GetExceptionInformation(), name)) {
        return 1;
    }
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
    // every defender against a spread of attackers: stride keeps the pair count bounded.
    // Attacker -1 is not compared: the original reads Vehs[-1] for it and faults on most heap
    // layouts (the DLL treats it as "no attacker", a documented deviation).
    int stride = veh_count > 64 ? veh_count / 64 : 1;
    for (int def = 0; def < veh_count; def++) {
        for (int atk = 0; atk < veh_count; atk += stride) {
            if (atk == def) {
                continue;
            }
            for (BOOL art = 0; art <= 1; art++) {
                REGRESS_CTX("best_defender(%d, %d, %d)", def, atk, art);
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
                REGRESS_CTX("battle_compute(%d, %d, type %d)", atk, def, type);
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
    int st_orig[15], st_new[15]; // talents[5], drones[5], super drones[5] after each psych stage
    auto grab = [](int *st) {
        memcpy(st, BaseCurrentTalentsStage, 5 * sizeof(int));
        memcpy(st + 5, BaseCurrentDronesStage, 5 * sizeof(int));
        memcpy(st + 10, BaseCurrentSuperDronesStage, 5 * sizeof(int));
    };
    for (uint32_t stage = 0; stage < 2; stage++) { // stage 1 enables the full talent/police grid
        for (int i = 0; i < *BaseCurrentCount; i++) {
            sprintf_s(label, "base_psych(base %d, upkeep stage %u)", i, stage);
            BOOL bad = h.compare(label,
                [&] { *BaseUpkeepStage = stage; set_base(i); base_psych_OG(); grab(st_orig); },
                [&] { *BaseUpkeepStage = stage; set_base(i); base_psych(); grab(st_new); });
            if (bad) { // the stage arrays hold the last grid cell evaluated (the only one at stage 0)
                Base *b = &Bases[i];
                logf("    faction %u pop %d spec %d psych %d SE talent %d police %d pacifism %u\n",
                    b->faction_id_current, b->population_size, b->specialist_total, b->psych_total,
                    PlayersData[b->faction_id_current].soc_effect_pending.talent,
                    PlayersData[b->faction_id_current].soc_effect_pending.police,
                    *BaseCurrentVehPacifismCount);
                for (int side = 0; side < 2; side++) {
                    int *st = side ? st_new : st_orig;
                    logf("    %s T %d %d %d %d %d D %d %d %d %d %d S %d %d %d %d %d\n",
                        side ? "new " : "orig", st[0], st[1], st[2], st[3], st[4], st[5], st[6],
                        st[7], st[8], st[9], st[10], st[11], st[12], st[13], st[14]);
                }
            }
        }
    }
    // Synthetic inputs for branches no save exercises: pacifism drones (the count is only set
    // during base_upkeep, so it is 0 whenever the harness runs; police -3/-4/-5 in the grid use
    // it) and nerve-stapled bases (none in any available save). Every 4th base, grid pass.
    uint32_t pacifism_saved = *BaseCurrentVehPacifismCount;
    for (uint32_t pacifism = 1; pacifism <= 3; pacifism++) {
        for (int i = 0; i < *BaseCurrentCount; i += 4) {
            sprintf_s(label, "base_psych(base %d, upkeep stage 1, pacifism %u)", i, pacifism);
            h.compare(label,
                [&] { *BaseUpkeepStage = 1; *BaseCurrentVehPacifismCount = pacifism; set_base(i); base_psych_OG(); },
                [&] { *BaseUpkeepStage = 1; *BaseCurrentVehPacifismCount = pacifism; set_base(i); base_psych(); });
        }
    }
    *BaseCurrentVehPacifismCount = pacifism_saved;
    for (int i = 0; i < *BaseCurrentCount; i += 4) {
        sprintf_s(label, "base_psych(base %d, upkeep stage 1, nerve stapled)", i);
        h.compare(label,
            [&] { *BaseUpkeepStage = 1; Bases[i].nerve_staple_turns_left = 3; set_base(i); base_psych_OG(); },
            [&] { *BaseUpkeepStage = 1; Bases[i].nerve_staple_turns_left = 3; set_base(i); base_psych(); });
    }
    *BaseUpkeepStage = stage_saved;
    *BaseIDCurrentSelected = selected_saved;
    *BaseCurrent = current_saved;
    return h.report();
}

// --- do_* message-pump wrappers: message-level test -------------------------------------------
// Probe messages are posted to the game window, one wrapper call is made, and the result is the
// return value plus which probes are still queued. The probes sit on the range boundaries the
// wrappers filter on (0..0xFF, 0x100..0x108, 0x109..0x1FF, 0x200..0x209, 0x20A..0xFFFF) and are
// ones the game's window procedure ignores. Needs an exe patched WITHOUT the do_* rows, else
// the *_OG pointers land in the DLL.
typedef BOOL(__cdecl *msg_fn_bool)();
typedef void(__cdecl *msg_fn_void)();
static msg_fn_bool do_non_input_OG = (msg_fn_bool)0x005FCA30;
static msg_fn_void do_all_non_input_OG = (msg_fn_void)0x005FCB20;
static msg_fn_bool do_draw_OG = (msg_fn_bool)0x005FCB60;
static msg_fn_void do_all_draws_OG = (msg_fn_void)0x005FCBB0;
static msg_fn_bool do_keyboard_OG = (msg_fn_bool)0x005FCC20;
static msg_fn_void do_all_keyboard_OG = (msg_fn_void)0x005FCC70;

static const uint32_t MsgProbe[] = {
    0x00FE,          // WM_INPUT_DEVICE_CHANGE: last-but-one of the first range (0 cannot be peeked alone)
    WM_KEYUP,        // 0x101: keyboard range
    0x0108,          // WM_KEYLAST of the game's SDK: top of the keyboard range
    0x0109,          // WM_UNICHAR: first id above it
    0x01F5,          // unassigned, inside 0x109..0x1FF
    WM_MOUSEMOVE,    // 0x200: mouse range, excluded from do_non_input
    WM_MOUSEWHEEL,   // 0x20A: first id above the mouse range (newer SDKs put WM_MOUSELAST at 0x20E)
    WM_USER + 0x123, // 0x523
};
static const int MsgProbeNum = sizeof(MsgProbe) / sizeof(MsgProbe[0]);

static void msg_drain(HWND hwnd) {
    MSG m;
    for (int n = 0; n < 500 && PeekMessage(&m, NULL, 0, 0, PM_REMOVE); n++) {
        if (m.message == WM_PAINT) {
            ValidateRect(m.hwnd, NULL);
        } else if (m.message == WM_QUIT) {
            PostQuitMessage((int)m.wParam);
            break;
        }
    }
    (void)hwnd;
}

// remove every queued message that is not one of the probes (timer ticks and the like)
static void msg_drain_others(HWND hwnd) {
    static const uint32_t ranges[][2] = {
        { 0x0001, 0x00FD }, { 0x00FF, 0x0100 }, { 0x0102, 0x0107 }, { 0x010A, 0x01F4 },
        { 0x01F6, 0x01FF }, { 0x0201, 0x0209 }, { 0x020B, 0x0522 }, { 0x0524, 0xFFFF },
    };
    MSG m;
    for (size_t r = 0; r < sizeof(ranges) / sizeof(ranges[0]); r++) {
        for (int n = 0; n < 100 && PeekMessage(&m, NULL, ranges[r][0], ranges[r][1], PM_REMOVE); n++) {
            if (m.message == WM_PAINT) {
                ValidateRect(m.hwnd, NULL);
            }
        }
    }
    (void)hwnd;
}

static uint32_t msg_pending(HWND hwnd) {
    uint32_t mask = 0;
    MSG m;
    for (int i = 0; i < MsgProbeNum; i++) {
        if (PeekMessage(&m, hwnd, MsgProbe[i], MsgProbe[i], PM_NOREMOVE)) {
            mask |= 1u << i;
        }
    }
    return mask;
}

static void msg_post(HWND hwnd, const uint32_t *ids, int n) {
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(hwnd, &p);
    for (int i = 0; i < n; i++) {
        WPARAM w = ids[i] == WM_KEYUP ? VK_F24 : 0;
        LPARAM l = ids[i] == WM_MOUSEMOVE ? MAKELPARAM(p.x, p.y) : 0;
        PostMessage(hwnd, ids[i], w, l);
        Sleep(2); // distinct MSG.time per probe: do_non_input picks the oldest of its ranges
    }
}

struct MsgCase {
    uint32_t ids[8];
    int n;
};

static int test_do_wrappers() {
    HWND hwnd = *HandleMain;
    int compared = 0, mismatches = 0;
    if (!hwnd || !IsWindow(hwnd)) {
        logf("do_wrappers: no main window, skipped");
        return 0;
    }
    static const MsgCase cases[] = {
        { { 0x00FE }, 1 }, { { WM_KEYUP }, 1 }, { { 0x0108 }, 1 }, { { 0x0109 }, 1 },
        { { 0x01F5 }, 1 }, { { WM_MOUSEMOVE }, 1 }, { { WM_MOUSEWHEEL }, 1 }, { { WM_USER + 0x123 }, 1 },
        { { WM_USER + 0x123, 0x00FE }, 2 }, { { 0x00FE, 0x0109, WM_MOUSEWHEEL }, 3 },
        { { WM_KEYUP, 0x01F5 }, 2 }, { { WM_MOUSEWHEEL, 0x0109, 0x00FE }, 3 },
        { { WM_KEYUP, 0x0108 }, 2 }, { { WM_MOUSEMOVE, 0x0108, WM_KEYUP }, 3 },
        { { 0x00FE, WM_KEYUP, 0x0108, 0x0109, 0x01F5, WM_MOUSEMOVE, WM_MOUSEWHEEL, WM_USER + 0x123 }, 8 },
    };
    struct Wrapper {
        const char *name;
        msg_fn_bool one_orig, one_new;  // single-message form (returns BOOL)
        msg_fn_void all_orig, all_new;  // process-all form
    };
    static const Wrapper wrappers[] = {
        { "do_non_input", do_non_input_OG, do_non_input, NULL, NULL },
        { "do_all_non_input", NULL, NULL, do_all_non_input_OG, do_all_non_input },
        { "do_keyboard", do_keyboard_OG, do_keyboard, NULL, NULL },
        { "do_all_keyboard", NULL, NULL, do_all_keyboard_OG, do_all_keyboard },
    };
    int retried = 0;
    for (size_t w = 0; w < sizeof(wrappers) / sizeof(wrappers[0]); w++) {
        for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
            BOOL ret[2] = { 0, 0 };
            uint32_t left[2] = { 0, 0 };
            uint32_t status[2] = { 0, 0 };
            BOOL same = false;
            for (int attempt = 0; attempt < 3 && !same; attempt++) {
                for (int side = 0; side < 2; side++) {
                    msg_drain(hwnd);
                    msg_post(hwnd, cases[c].ids, cases[c].n);
                    msg_drain_others(hwnd);
                    *MsgStatus = 0;
                    if (wrappers[w].one_orig) {
                        ret[side] = side ? wrappers[w].one_new() : wrappers[w].one_orig();
                    } else {
                        side ? wrappers[w].all_new() : wrappers[w].all_orig();
                    }
                    status[side] = *MsgStatus;
                    left[side] = msg_pending(hwnd);
                    msg_drain(hwnd);
                }
                same = ret[0] == ret[1] && left[0] == left[1] && status[0] == status[1];
                if (!same) {
                    retried++;
                }
            }
            compared++;
            if (!same) {
                mismatches++;
                logf("  %s case %u: orig ret=%d left=%02X status=%u; new ret=%d left=%02X status=%u",
                    wrappers[w].name, (unsigned)c, ret[0], left[0], status[0], ret[1], left[1],
                    status[1]);
            }
        }
    }
    // do_draw / do_all_draws: WM_PAINT is synthesised from the window's update region
    for (int form = 0; form < 2; form++) {
        BOOL ret[2][2] = { { 0, 0 }, { 0, 0 } };
        BOOL paint_left[2];
        MSG m;
        for (int side = 0; side < 2; side++) {
            msg_drain(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            if (form == 0) {
                ret[side][0] = side ? do_draw() : do_draw_OG();
                ret[side][1] = side ? do_draw() : do_draw_OG(); // nothing left to paint afterwards?
            } else {
                side ? do_all_draws() : do_all_draws_OG();
            }
            paint_left[side] = PeekMessage(&m, hwnd, WM_PAINT, WM_PAINT, PM_NOREMOVE) != 0;
            msg_drain(hwnd);
        }
        compared++;
        if (memcmp(ret[0], ret[1], sizeof(ret[0])) != 0 || paint_left[0] != paint_left[1]) {
            mismatches++;
            logf("  %s: orig ret=%d/%d paint left=%d; new ret=%d/%d paint left=%d",
                form ? "do_all_draws" : "do_draw", ret[0][0], ret[0][1], paint_left[0],
                ret[1][0], ret[1][1], paint_left[1]);
        }
    }
    logf("do_wrappers: %d compared, %d mismatches, %d attempts retried (stray messages)",
        compared, mismatches, retried);
    return mismatches;
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

// world_borehole seeds its layout from timeGetTime(), so the original and the DLL only agree when
// both calls land in the same millisecond. During that test both modules' import slots for
// timeGetTime are pointed at a stub that returns a value chosen per sample.
static DWORD FixedTimeValue = 0;
static DWORD WINAPI fixed_time_get_time() { return FixedTimeValue; }

struct TimeGetTimePin {
    void **exe_slot, **dll_slot;
    void *exe_saved, *dll_saved;
    TimeGetTimePin() {
        exe_slot = iat_slot(GetModuleHandle(NULL), "winmm.dll", "timeGetTime");
        dll_slot = iat_slot(GetModuleHandle("OpenSMACX.dll"), "winmm.dll", "timeGetTime");
        exe_saved = iat_patch(exe_slot, (void *)fixed_time_get_time);
        dll_saved = iat_patch(dll_slot, (void *)fixed_time_get_time);
    }
    ~TimeGetTimePin() {
        iat_patch(exe_slot, exe_saved);
        iat_patch(dll_slot, dll_saved);
    }
    BOOL ok() const { return exe_slot && dll_slot; }
};

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
        BOOL is_borehole = strcmp(fns[i].name, "world_borehole") == 0;
        TimeGetTimePin *pin = is_borehole ? new TimeGetTimePin() : NULL;
        if (pin && !pin->ok()) {
            logf("  timeGetTime import not found (exe %p, dll %p); layout may differ by timing",
                pin->exe_slot, pin->dll_slot);
        }
        auto prepare = [&](uint32_t seed) {
            reseed(seed);
            FixedTimeValue = seed * 2654435761u; // varies seed % 8 and (seed / 8) % 4 across samples
        };
        const int samples = 24;
        for (int k = 0; k < samples; k++) {
            int x, y;
            sample_tile(k, samples, &x, &y);
            sprintf_s(label, "%s(%d, %d)", fns[i].name, x, y);
            uint32_t seed = k + 1;
            h.compare(label, [&] { prepare(seed); fns[i].orig(x, y); },
                [&] { prepare(seed); fns[i].neu(x, y); });
        }
        for (uint32_t seed = 1; seed <= 3; seed++) { // random placement
            sprintf_s(label, "%s(-1, -1) seed %u", fns[i].name, seed);
            h.compare(label, [&] { prepare(seed); fns[i].orig(-1, -1); },
                [&] { prepare(seed); fns[i].neu(-1, -1); });
        }
        delete pin;
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
        TimeGetTimePin pin; // world_landmarks runs world_borehole, which reads the clock
        auto prepare = [&](uint32_t seed) {
            reseed(seed);
            FixedTimeValue = seed * 2654435761u;
        };
        for (uint32_t seed = 1; seed <= 3; seed++) {
            sprintf_s(label, "%s() seed %u", fns[i].name, seed);
            h.compare(label, [&] { prepare(seed); fns[i].orig(); },
                [&] { prepare(seed); fns[i].neu(); });
        }
        total += h.report();
    }
    return total;
}

// ------------------------------------------------------------------------------------------

// One-line-per-faction summary of the loaded save: the inputs the psych/combat/diplomacy tests
// depend on, so a sweep over saves shows which branches each save exercises.
static void log_survey() {
    logf("survey: map %dx%d, SP owners: SAC %d LV %d HGP %d CI %d VW %d PTS %d",
        *MapLongitudeBounds, *MapLatitudeBounds, SecretProject->self_aware_colony,
        SecretProject->longevity_vaccine, SecretProject->human_genome_project,
        SecretProject->clinical_immortality, SecretProject->virtual_world,
        SecretProject->planetary_transit_system);
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        if (!PlayersData[f].current_num_bases && !is_alive(f)) {
            continue;
        }
        int staple = 0, punish = 0, brood = 0, rec = 0, holo = 0, paradise = 0, assim = 0;
        int away = 0, units = 0;
        for (int i = 0; i < *BaseCurrentCount; i++) {
            if (Bases[i].faction_id_current != f) {
                continue;
            }
            staple += Bases[i].nerve_staple_turns_left != 0;
            punish += has_fac_built(FAC_PUNISHMENT_SPHERE, i);
            brood += has_fac_built(FAC_BROOD_PIT, i);
            rec += has_fac_built(FAC_RECREATION_COMMONS, i);
            holo += has_fac_built(FAC_HOLOGRAM_THEATRE, i);
            paradise += has_fac_built(FAC_PARADISE_GARDEN, i);
            assim += Bases[i].assimilation_turns_left != 0;
        }
        for (int v = 0; v < *VehCurrentCount; v++) {
            if (Vehs[v].faction_id != f) {
                continue;
            }
            units++;
            if (VehPrototypes[Vehs[v].proto_id].plan <= PLAN_RECONNAISANCE
                && whose_territory(f, Vehs[v].x, Vehs[v].y, NULL, false) != (int)f) {
                away++; // combat unit outside own territory: the pacifism drone source
            }
        }
        logf("survey: faction %u %-12s %s diff %u bases %u units %d away %d | SE talent %d police %d"
            " econ %u eff %d | rules talent %d drone %d%s | staple %d punish %d brood %d rec %d holo %d"
            " paradise %d assim %d",
            f, Players[f].noun_faction, is_human(f) ? "human" : "AI", PlayersData[f].diff_level,
            PlayersData[f].current_num_bases, units, away,
            PlayersData[f].soc_effect_pending.talent, PlayersData[f].soc_effect_pending.police,
            PlayersData[f].soc_category_pending.economics, PlayersData[f].soc_effect_pending.efficiency,
            Players[f].rule_talent, Players[f].rule_drone,
            (Players[f].rule_flags & RFLAG_WORMPOLICE) ? " WORMPOLICE" : "",
            staple, punish, brood, rec, holo, paradise, assim);
    }
}

// ------------------------------------------------------------------------------------------
// VEH helper batch: veh_selectable/unmoved/ready, veh_find, is_bunged/clear_bunglist, add_abil,
// unspot_stack, stack_kill, get_rating, most_objectives.
// ------------------------------------------------------------------------------------------

/*
The three selection predicates over every unit with every faction as the current player (they are
all read-only, so only *FactionIDCurrent has to be restored).
*/
static int test_veh_predicates() {
    int compared = 0, mismatches = 0, selectable = 0, unmoved = 0, ready = 0;
    int faction_saved = *FactionIDCurrent;
    for (int f = 0; f < MaxPlayerNum; f++) {
        *FactionIDCurrent = f;
        for (int veh_id = -1; veh_id < *VehCurrentCount; veh_id++) {
            BOOL s_orig = veh_selectable_OG(veh_id), s_new = veh_selectable(veh_id);
            BOOL u_orig = veh_unmoved_OG(veh_id), u_new = veh_unmoved(veh_id);
            BOOL r_orig = veh_ready_OG(veh_id), r_new = veh_ready(veh_id);
            compared++;
            selectable += s_orig != 0;
            unmoved += u_orig != 0;
            ready += r_orig != 0;
            if (s_orig != s_new || u_orig != u_new || r_orig != r_new) {
                if (mismatches < MAX_DETAIL) {
                    logf("  veh %d faction %d: selectable new=%d orig=%d, unmoved new=%d orig=%d, "
                        "ready new=%d orig=%d", veh_id, f, s_new, s_orig, u_new, u_orig, r_new, r_orig);
                }
                mismatches++;
            }
        }
    }
    *FactionIDCurrent = faction_saved;
    logf("veh_selectable/unmoved/ready: %d compared (%d/%d/%d true), %d mismatches", compared,
        selectable, unmoved, ready, mismatches);
    return mismatches;
}

static int test_veh_find() {
    int compared = 0, mismatches = 0, found = 0;
    const int skips[] = { -1, 0, *VehCurrentCount / 2, *VehCurrentCount - 1 };
    for (int f = -1; f < MaxPlayerNum; f++) {
        for (int k = 0; k < (int)(sizeof(skips) / sizeof(skips[0])); k++) {
            for (int y = 0; y < *MapLatitudeBounds; y += 7) {
                for (int x = 0; x < *MapLongitudeBounds; x += 6) {
                    int b = veh_find_OG(x, y, f, skips[k]);
                    int b_dist = *VehFindDist;
                    int a = veh_find(x, y, f, skips[k]);
                    int a_dist = *VehFindDist;
                    compared++;
                    found += b >= 0;
                    if (a != b || a_dist != b_dist) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  veh_find(%d, %d, %d, %d): new=%d dist %d, orig=%d dist %d", x, y,
                                f, skips[k], a, a_dist, b, b_dist);
                        }
                        mismatches++;
                    }
                }
            }
        }
    }
    logf("veh_find: %d compared (%d found), %d mismatches", compared, found, mismatches);
    return mismatches;
}

/*
Every list entry is queried with its own values, with one field changed at a time and with the
"empty" wildcard, for every faction including the inactive ones.
*/
static int test_is_bunged() {
    int compared = 0, mismatches = 0, hits = 0, entries = 0;
    for (uint32_t f = 0; f < MaxPlayerNum; f++) {
        int entries_f = 0;
        for (int i = 0; i < MaxBungedDesignNum; i++) {
            const BungedDesign d = BungList[f * MaxBungedDesignNum + i];
            entries_f += d.ability_flags != -1 || d.chassis_id != 0xFF;
            const struct { uint32_t c, w, a; int ab; uint32_t r; } cases[] = {
                { d.chassis_id, d.weapon_id, d.armor_id, d.ability_flags, d.reactor_id },
                { d.chassis_id + 1u, d.weapon_id, d.armor_id, d.ability_flags, d.reactor_id },
                { d.chassis_id, d.weapon_id + 1u, d.armor_id, d.ability_flags, d.reactor_id },
                { d.chassis_id, d.weapon_id, d.armor_id + 1u, d.ability_flags, d.reactor_id },
                { d.chassis_id, d.weapon_id, d.armor_id, d.ability_flags ^ 1, d.reactor_id },
                { d.chassis_id, d.weapon_id, d.armor_id, d.ability_flags, d.reactor_id + 1u },
                { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, -1, 0xFFFFFFFF },
                { 0xFF, 0xFF, 0xFF, -1, 0xFF },
            };
            for (int k = 0; k < (int)(sizeof(cases) / sizeof(cases[0])); k++) {
                BOOL b = is_bunged_OG(f, cases[k].c, cases[k].w, cases[k].a, cases[k].ab, cases[k].r);
                BOOL a = is_bunged(f, cases[k].c, cases[k].w, cases[k].a, cases[k].ab, cases[k].r);
                compared++;
                hits += b != 0;
                if (a != b) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  is_bunged(%u, %u, %u, %u, %d, %u): new=%d orig=%d", f, cases[k].c,
                            cases[k].w, cases[k].a, cases[k].ab, cases[k].r, a, b);
                    }
                    mismatches++;
                }
            }
        }
        entries += entries_f;
        logf("  bunglist faction %u: %d used entries, status %s", f, entries_f,
            (*FactionsStatus & (1 << f)) ? "active" : "inactive");
    }
    logf("is_bunged: %d compared (%d true, %d list entries), %d mismatches", compared, hits, entries,
        mismatches);
    return mismatches;
}

static int test_clear_bunglist() {
    const size_t size = sizeof(BungedDesign) * MaxBungedDesignNum * MaxPlayerNum;
    BungedDesign *saved = (BungedDesign *)malloc(size);
    BungedDesign *after_orig = (BungedDesign *)malloc(size);
    memcpy(saved, BungList, size);
    int mismatches = 0;
    for (uint32_t f = 0; f < MaxPlayerNum; f++) {
        memcpy(BungList, saved, size);
        clear_bunglist_OG(f);
        memcpy(after_orig, BungList, size);
        memcpy(BungList, saved, size);
        clear_bunglist(f);
        size_t d = first_diff(after_orig, BungList, size);
        if (d != (size_t)-1) {
            logf("  clear_bunglist(%u): byte %u differs (orig %02X new %02X)", f, (unsigned)d,
                ((uint8_t *)after_orig)[d], ((uint8_t *)BungList)[d]);
            mismatches++;
        }
    }
    memcpy(BungList, saved, size);
    free(saved);
    free(after_orig);
    logf("clear_bunglist: %d compared, %d mismatches", MaxPlayerNum, mismatches);
    return mismatches;
}

static int test_add_abil() {
    int compared = 0, mismatches = 0, added = 0;
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        for (int k = 0; k < MaxAbilityNum; k++) {
            for (uint32_t init = 0; init <= 1; init++) {
                uint32_t flags_orig = init, flags_new = init;
                add_abil_OG(&flags_orig, 1u << k, f);
                add_abil(&flags_new, 1u << k, f);
                compared++;
                added += flags_orig != init;
                if (flags_orig != flags_new) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  add_abil(0x%X, 0x%X, %u): new=0x%X orig=0x%X", init, 1u << k, f,
                            flags_new, flags_orig);
                    }
                    mismatches++;
                }
            }
        }
    }
    logf("add_abil: %d compared (%d added), %d mismatches", compared, added, mismatches);
    return mismatches;
}

static int test_unspot_stack() {
    const int count = *VehCurrentCount;
    const size_t size = sizeof(Veh) * count;
    Veh *saved = (Veh *)malloc(size);
    Veh *after_orig = (Veh *)malloc(size);
    memcpy(saved, Vehs, size);
    int compared = 0, mismatches = 0, effective = 0;
    for (int veh_id = -1; veh_id < count; veh_id++) {
        memcpy(Vehs, saved, size);
        unspot_stack_OG(veh_id);
        memcpy(after_orig, Vehs, size);
        effective += memcmp(after_orig, saved, size) != 0;
        memcpy(Vehs, saved, size);
        unspot_stack(veh_id);
        compared++;
        size_t d = first_diff(after_orig, Vehs, size);
        if (d != (size_t)-1) {
            if (mismatches < MAX_DETAIL) {
                logf("  unspot_stack(%d): Veh[%u] +0x%X differs (orig %02X new %02X)", veh_id,
                    (unsigned)(d / sizeof(Veh)), (unsigned)(d % sizeof(Veh)),
                    ((uint8_t *)after_orig)[d], ((uint8_t *)Vehs)[d]);
            }
            mismatches++;
        }
    }
    memcpy(Vehs, saved, size);
    free(saved);
    free(after_orig);
    logf("unspot_stack: %d compared (%d changed state), %d mismatches", compared, effective, mismatches);
    return mismatches;
}

/*
stack_kill() goes through the original veh_kill() on both sides; the whole game state is snapshot
around each call. veh_kill() also renumbers a few selection globals the snapshot doesn't cover, so
those are saved by hand. Every 5th stack is tried to keep the run short.
*/
static int test_stack_kill() {
    static int *const extra_globals[] = { (int *)0x0093928C, (int *)0x00939290, (int *)0x008C6B2C,
        (int *)0x008C6B34, (int *)0x009A64DC };
    const int extra_count = (int)(sizeof(extra_globals) / sizeof(extra_globals[0]));
    int extra_saved[5];
    for (int i = 0; i < extra_count; i++) {
        extra_saved[i] = *extra_globals[i];
    }
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0, killed = 0;
    char label[64];
    for (int veh_id = -1; veh_id < base.veh_count; veh_id += (veh_id < 0) ? 1 : 5) {
        snap_restore(&base);
        for (int i = 0; i < extra_count; i++) {
            *extra_globals[i] = extra_saved[i];
        }
        stack_kill_OG(veh_id);
        snap_take(&after_orig);
        killed += base.veh_count_global - after_orig.veh_count_global;
        snap_restore(&base);
        for (int i = 0; i < extra_count; i++) {
            *extra_globals[i] = extra_saved[i];
        }
        stack_kill(veh_id);
        snap_take(&after_new);
        compared++;
        sprintf_s(label, "stack_kill(%d)", veh_id);
        if (snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL)) {
            mismatches++;
        }
    }
    snap_restore(&base);
    for (int i = 0; i < extra_count; i++) {
        *extra_globals[i] = extra_saved[i];
    }
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("stack_kill: %d compared (%d units killed by the original), %d mismatches", compared, killed,
        mismatches);
    return mismatches;
}

/*
get_rating() over every faction, difficulty level 0-6 and both Intense Rivalry settings; the
difficulty and rules are restored afterwards.
*/
static int test_get_rating() {
    int compared = 0, mismatches = 0;
    uint32_t rules_saved = *GameRules;
    for (uint32_t f = 0; f < MaxPlayerNum; f++) {
        int diff_saved = PlayersData[f].diff_level;
        for (int diff = 0; diff <= 6; diff++) {
            PlayersData[f].diff_level = diff;
            for (int rivalry = 0; rivalry <= 1; rivalry++) {
                *GameRules = rivalry ? (rules_saved | RULES_INTENSE_RIVALRY)
                    : (rules_saved & ~RULES_INTENSE_RIVALRY);
                for (int value = -2000; value <= 2000; value += 13) {
                    int b = get_rating_OG(f, value);
                    int a = get_rating(f, value);
                    compared++;
                    if (a != b) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  get_rating(%u, %d) diff %d rivalry %d: new=%d orig=%d", f, value,
                                diff, rivalry, a, b);
                        }
                        mismatches++;
                    }
                }
            }
        }
        PlayersData[f].diff_level = diff_saved;
    }
    *GameRules = rules_saved;
    logf("get_rating: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

/*
most_objectives() with every combination of output pointers, cooperative victory on/off and the
"all bases count" rule on/off (so that the tie logic sees real objective counts).
*/
static int test_most_objectives() {
    int compared = 0, mismatches = 0;
    uint32_t rules_saved = *GameRules;
    for (int coop = 0; coop <= 1; coop++) {
        for (int all_bases = 0; all_bases <= 1; all_bases++) {
            *GameRules = rules_saved;
            *GameRules = coop ? (*GameRules | RULES_VICTORY_COOPERATIVE)
                : (*GameRules & ~RULES_VICTORY_COOPERATIVE);
            *GameRules = all_bases ? (*GameRules | RULES_SCN_VICT_ALL_BASE_COUNT_OBJ)
                : (*GameRules & ~RULES_SCN_VICT_ALL_BASE_COUNT_OBJ);
            for (int mode = 0; mode < 4; mode++) {
                int f_orig = -99, t_orig = -99, f_new = -99, t_new = -99;
                int b = most_objectives_OG((mode & 1) ? &f_orig : NULL, (mode & 2) ? &t_orig : NULL);
                int a = most_objectives((mode & 1) ? &f_new : NULL, (mode & 2) ? &t_new : NULL);
                compared++;
                if (a != b || f_orig != f_new || t_orig != t_new) {
                    logf("  most_objectives coop %d all_bases %d mode %d: new=%d (f %d, ties %d) "
                        "orig=%d (f %d, ties %d)", coop, all_bases, mode, a, f_new, t_new, b, f_orig,
                        t_orig);
                    mismatches++;
                } else if (mode == 3) {
                    logf("  most_objectives coop %d all_bases %d: %d objectives, faction %d, ties %d",
                        coop, all_bases, b, f_orig, t_orig);
                }
            }
        }
    }
    *GameRules = rules_saved;
    logf("most_objectives: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

// ------------------------------------------------------------------------------------------
// Batch 2 (2026-10-16): abil_cond, bung_proto, spot_base, spot_stack, retire_proto,
// study_artifact. tech_secret only pops a dialog and has no test. The spot/retire/study tests
// compare whole game snapshots taken after the original and after the decompiled version ran
// from the same baseline; retire_proto also compares the bunglist and the prototype table.
// ------------------------------------------------------------------------------------------

static int test_abil_cond() {
    BOOL expansion_saved = *ExpansionEnabled;
    int compared = 0, mismatches = 0, allowed = 0;
    for (int expansion = 0; expansion < 2; expansion++) {
        *ExpansionEnabled = expansion;
        for (int abil_id = 0; abil_id < MaxAbilityNum; abil_id++) {
            for (int chas_id = 0; chas_id < MaxChassisNum; chas_id++) {
                for (int wpn_id = 0; wpn_id < MaxWeaponNum; wpn_id++) {
                    for (int arm_id = 0; arm_id < MaxArmorNum; arm_id++) {
                        BOOL orig = abil_cond_OG(abil_id, chas_id, wpn_id, arm_id);
                        BOOL mine = abil_cond(abil_id, chas_id, wpn_id, arm_id);
                        compared++;
                        allowed += (orig != 0);
                        if (orig != mine) {
                            if (mismatches < MAX_DETAIL) {
                                logf("  abil_cond(%d, %d, %d, %d) expansion=%d: orig %d new %d",
                                    abil_id, chas_id, wpn_id, arm_id, expansion, orig, mine);
                            }
                            mismatches++;
                        }
                    }
                }
            }
        }
    }
    *ExpansionEnabled = expansion_saved;
    logf("abil_cond: %d compared (%d allowed), %d mismatches", compared, allowed, mismatches);
    return mismatches;
}

static int test_bung_proto() {
    const size_t bung_size = sizeof(BungedDesign) * MaxPlayerNum * MaxBungedDesignNum;
    uint8_t *saved = (uint8_t *)malloc(bung_size);
    uint8_t *after_orig = (uint8_t *)malloc(bung_size);
    uint8_t *after_new = (uint8_t *)malloc(bung_size);
    memcpy(saved, BungList, bung_size);
    int index_saved[MaxPlayerNum];
    for (int f = 0; f < MaxPlayerNum; f++) {
        index_saved[f] = PlayersData[f].bunglist_index;
    }
    int compared = 0, mismatches = 0;
    for (int faction_id = 0; faction_id < MaxPlayerNum; faction_id++) {
        for (int proto_id = 0; proto_id < MaxVehProtoNum; proto_id += 3) {
            int start = (proto_id * 7 + faction_id) % MaxBungedDesignNum;
            memcpy(BungList, saved, bung_size);
            PlayersData[faction_id].bunglist_index = start;
            bung_proto_OG(faction_id, proto_id);
            memcpy(after_orig, BungList, bung_size);
            int index_orig = PlayersData[faction_id].bunglist_index;
            memcpy(BungList, saved, bung_size);
            PlayersData[faction_id].bunglist_index = start;
            bung_proto(faction_id, proto_id);
            memcpy(after_new, BungList, bung_size);
            int index_new = PlayersData[faction_id].bunglist_index;
            compared++;
            size_t d = first_diff(after_orig, after_new, bung_size);
            if (d != (size_t)-1 || index_orig != index_new) {
                if (mismatches < MAX_DETAIL) {
                    logf("  bung_proto(%d, %d) start %d: BungList byte %d differs, index %d/%d",
                        faction_id, proto_id, start, (int)d, index_orig, index_new);
                }
                mismatches++;
            }
        }
    }
    memcpy(BungList, saved, bung_size);
    for (int f = 0; f < MaxPlayerNum; f++) {
        PlayersData[f].bunglist_index = index_saved[f];
    }
    free(saved); free(after_orig); free(after_new);
    logf("bung_proto: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

static int test_spot_base() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0;
    char label[64];
    for (int base_id = 0; base_id < base.base_count; base_id += 3) {
        for (int faction_id = 0; faction_id < MaxPlayerNum; faction_id++) {
            snap_restore(&base);
            spot_base_OG(base_id, faction_id);
            snap_take(&after_orig);
            snap_restore(&base);
            spot_base(base_id, faction_id);
            snap_take(&after_new);
            compared++;
            sprintf_s(label, "spot_base(%d, %d)", base_id, faction_id);
            BOOL diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
            if (!diff && after_orig.draw_state != after_new.draw_state) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: draw state differs (orig %X new %X)", label, after_orig.draw_state,
                        after_new.draw_state);
                }
                diff = true;
            }
            if (diff) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("spot_base: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

static int test_spot_stack() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0;
    char label[64];
    for (int veh_id = -1; veh_id < base.veh_count; veh_id += (veh_id < 0) ? 1 : 7) {
        for (int faction_id = 0; faction_id < MaxPlayerNum; faction_id++) {
            snap_restore(&base);
            spot_stack_OG(veh_id, faction_id);
            snap_take(&after_orig);
            snap_restore(&base);
            spot_stack(veh_id, faction_id);
            snap_take(&after_new);
            compared++;
            sprintf_s(label, "spot_stack(%d, %d)", veh_id, faction_id);
            BOOL diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
            if (!diff && after_orig.draw_state != after_new.draw_state) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: draw state differs (orig %X new %X)", label, after_orig.draw_state,
                        after_new.draw_state);
                }
                diff = true;
            }
            if (diff) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("spot_stack: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

static int test_retire_proto() {
    // selection globals touched by kill()/veh_kill(), same set as the stack_kill test
    static int *const extra_globals[] = { (int *)0x0093928C, (int *)0x00939290, (int *)0x008C6B2C,
        (int *)0x008C6B34, (int *)0x009A64DC };
    const int extra_count = (int)(sizeof(extra_globals) / sizeof(extra_globals[0]));
    int extra_saved[5];
    for (int i = 0; i < extra_count; i++) {
        extra_saved[i] = *extra_globals[i];
    }
    const size_t bung_size = sizeof(BungedDesign) * MaxPlayerNum * MaxBungedDesignNum;
    const size_t proto_size = sizeof(VehPrototype) * MaxVehProtoNum;
    uint8_t *bung_saved = (uint8_t *)malloc(bung_size);
    uint8_t *bung_orig = (uint8_t *)malloc(bung_size);
    uint8_t *bung_new = (uint8_t *)malloc(bung_size);
    uint8_t *proto_saved = (uint8_t *)malloc(proto_size);
    uint8_t *proto_orig = (uint8_t *)malloc(proto_size);
    uint8_t *proto_new = (uint8_t *)malloc(proto_size);
    memcpy(bung_saved, BungList, bung_size);
    memcpy(proto_saved, VehPrototypes, proto_size);
    BOOL draw_saved = VehBattleUnkTgl[0];
    VehBattleUnkTgl[0] = true; // kill() skips its map redraw while this is set
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    auto prepare = [&]() {
        snap_restore(&base);
        memcpy(BungList, bung_saved, bung_size);
        memcpy(VehPrototypes, proto_saved, proto_size);
        for (int i = 0; i < extra_count; i++) {
            *extra_globals[i] = extra_saved[i];
        }
    };
    int compared = 0, mismatches = 0, killed = 0, changed = 0;
    char label[64];
    for (int faction_id = 1; faction_id < MaxPlayerNum; faction_id++) {
        for (int slot = 0; slot < 2 * MaxVehProtoFactionNum; slot++) {
            int proto_id = slot < MaxVehProtoFactionNum ? slot
                : faction_id * MaxVehProtoFactionNum + (slot - MaxVehProtoFactionNum);
            if (proto_id >= MaxVehProtoFactionNum
                && !(((VehPrototype *)proto_saved)[proto_id].flags & PROTO_ACTIVE)) {
                continue;
            }
            prepare();
            retire_proto_OG(proto_id, faction_id);
            snap_take(&after_orig);
            memcpy(bung_orig, BungList, bung_size);
            memcpy(proto_orig, VehPrototypes, proto_size);
            killed += base.veh_count_global - after_orig.veh_count_global;
            prepare();
            retire_proto(proto_id, faction_id);
            snap_take(&after_new);
            memcpy(bung_new, BungList, bung_size);
            memcpy(proto_new, VehPrototypes, proto_size);
            compared++;
            if (first_diff(after_orig.vehs, base.vehs, sizeof(Veh) * base.veh_count) != (size_t)-1
                || after_orig.veh_count_global != base.veh_count_global) {
                changed++;
            }
            sprintf_s(label, "retire_proto(%d, %d)", proto_id, faction_id);
            BOOL diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
            size_t d;
            if (!diff && (d = first_diff(bung_orig, bung_new, bung_size)) != (size_t)-1) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: BungList byte %u differs (orig %02X new %02X)", label, (unsigned)d,
                        bung_orig[d], bung_new[d]);
                }
                diff = true;
            }
            if (!diff && (d = first_diff(proto_orig, proto_new, proto_size)) != (size_t)-1) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: VehPrototype[%u] +0x%X differs (orig %02X new %02X)", label,
                        (unsigned)(d / sizeof(VehPrototype)), (unsigned)(d % sizeof(VehPrototype)),
                        proto_orig[d], proto_new[d]);
                }
                diff = true;
            }
            if (diff) {
                mismatches++;
            }
        }
    }
    prepare();
    VehBattleUnkTgl[0] = draw_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    free(bung_saved); free(bung_orig); free(bung_new);
    free(proto_saved); free(proto_orig); free(proto_new);
    logf("retire_proto: %d compared (%d changed units, %d units killed by the original), %d mismatches",
        compared, changed, killed, mismatches);
    return mismatches;
}

// tech_achieved() is exe code outside the test and runs the whole AI tech/design chain (nondeterministic
// state outside the snapshot), so while the test runs the exe's entry point is replaced with a stub
// that records the tech id and returns. Both sides call the same address.
static int StudyArtifactTechRecorded = -2;

static int test_study_artifact() {
    static int *const extra_globals[] = { (int *)0x0093928C, (int *)0x00939290, (int *)0x008C6B2C,
        (int *)0x008C6B34, (int *)0x009A64DC };
    const int extra_count = (int)(sizeof(extra_globals) / sizeof(extra_globals[0]));
    int extra_saved[5];
    for (int i = 0; i < extra_count; i++) {
        extra_saved[i] = *extra_globals[i];
    }
    // stub: mov eax,[esp+8] ; mov [StudyArtifactTechRecorded],eax ; ret
    uint8_t *entry = (uint8_t *)tech_achieved;
    uint8_t stub[10] = { 0x8B, 0x44, 0x24, 0x08, 0xA3, 0, 0, 0, 0, 0xC3 };
    int *recorder = &StudyArtifactTechRecorded;
    memcpy(stub + 5, &recorder, 4);
    uint8_t saved_code[10];
    DWORD old_protect = 0;
    if (!VirtualProtect(entry, sizeof(stub), PAGE_EXECUTE_READWRITE, &old_protect)) {
        logf("study_artifact: cannot unprotect tech_achieved (%u)", GetLastError());
        return 1;
    }
    memcpy(saved_code, entry, sizeof(stub));
    memcpy(entry, stub, sizeof(stub));
    FlushInstructionCache(GetCurrentProcess(), entry, sizeof(stub));

    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0, consumed = 0, techs = 0;
    char label[80];
    int current = *FactionIDCurrent;
    for (int veh_id = 0; veh_id < base.veh_count; veh_id += 9) {
        for (int variant = 0; variant < 2; variant++) {
            // an AI faction other than the one at the controls, so no dialog pops up
            int faction_id = 1 + (veh_id + variant) % (MaxPlayerNum - 1);
            if (faction_id == current) {
                faction_id = 1 + faction_id % (MaxPlayerNum - 1);
            }
            // identical baseline patch on both sides: turn the unit into this faction's artifact,
            // and for the second variant mark a base on its tile as already linked
            auto prepare = [&]() {
                snap_restore(&base);
                for (int i = 0; i < extra_count; i++) {
                    *extra_globals[i] = extra_saved[i];
                }
                Vehs[veh_id].proto_id = BSC_ALIEN_ARTIFACT;
                Vehs[veh_id].faction_id = (uint8_t)faction_id;
                if (variant) {
                    int base_id = base_at(Vehs[veh_id].x, Vehs[veh_id].y);
                    if (base_id >= 0) {
                        Bases[base_id].state |= BSTATE_ARTIFACT_LINKED;
                    }
                }
                StudyArtifactTechRecorded = -2;
                reseed(veh_id * 131 + variant);
            };
            prepare();
            BOOL orig = study_artifact_OG(veh_id);
            int tech_orig = StudyArtifactTechRecorded;
            snap_take(&after_orig);
            prepare();
            BOOL mine = study_artifact(veh_id);
            int tech_new = StudyArtifactTechRecorded;
            snap_take(&after_new);
            compared++;
            consumed += (orig != 0);
            techs += (tech_orig >= 0);
            sprintf_s(label, "study_artifact(%d) faction %d variant %d", veh_id, faction_id, variant);
            BOOL diff = false;
            if (orig != mine || tech_orig != tech_new) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: return orig %d new %d, tech orig %d new %d", label, orig, mine, tech_orig,
                        tech_new);
                }
                diff = true;
            }
            if (!diff) {
                diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
            }
            if (!diff && after_orig.draw_state != after_new.draw_state) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: draw state differs (orig %X new %X)", label, after_orig.draw_state,
                        after_new.draw_state);
                }
                diff = true;
            }
            if (diff) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    for (int i = 0; i < extra_count; i++) {
        *extra_globals[i] = extra_saved[i];
    }
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    memcpy(entry, saved_code, sizeof(stub));
    FlushInstructionCache(GetCurrentProcess(), entry, sizeof(stub));
    VirtualProtect(entry, sizeof(stub), old_protect, &old_protect);
    logf("study_artifact: %d compared (%d consumed, %d techs awarded by the original), %d mismatches",
        compared, consumed, techs, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 3

static int test_base_at() {
    int compared = 0, mismatches = 0, synthetic = 0;
    int w = *MapLongitudeBounds, h = *MapLatitudeBounds;
    for (int y = -2; y <= h + 1; y++) {
        for (int x = -2; x <= w + 1; x++) {
            int b = base_at_OG(x, y);
            int a = base_at(x, y);
            compared++;
            if (a != b) {
                if (mismatches < MAX_DETAIL) {
                    logf("  base_at(%d, %d): new=%d orig=%d", x, y, a, b);
                }
                mismatches++;
            }
        }
    }
    // a tile flagged as holding a base without one: with debug mode off both sides log the error
    // and rebuild the base bits
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    uint32_t state_saved = *GameState;
    *GameState &= ~STATE_DEBUG_MODE;
    char label[64];
    for (int i = 0; i < base.veh_count; i += 37) {
        int x = Vehs[i].x, y = Vehs[i].y;
        if (x < 0 || y < 0 || x >= w || y >= h || base_at_OG(x, y) >= 0) {
            continue;
        }
        snap_restore(&base);
        map_loc(x, y)->bit |= BIT_BASE_IN_TILE;
        int b = base_at_OG(x, y);
        snap_take(&after_orig);
        snap_restore(&base);
        map_loc(x, y)->bit |= BIT_BASE_IN_TILE;
        int a = base_at(x, y);
        snap_take(&after_new);
        compared++;
        synthetic++;
        sprintf_s(label, "base_at(%d, %d) bad bit", x, y);
        BOOL diff = false;
        if (a != b) {
            if (mismatches < MAX_DETAIL) {
                logf("  %s: new=%d orig=%d", label, a, b);
            }
            diff = true;
        }
        if (!diff) {
            diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
        }
        if (diff) {
            mismatches++;
        }
    }
    snap_restore(&base);
    *GameState = state_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("base_at: %d compared (%d with a stray base bit), %d mismatches", compared, synthetic,
        mismatches);
    return mismatches;
}

static int test_upgrade_cost() {
    int compared = 0, mismatches = 0;
    int nano_saved = SecretProject->nano_factory;
    for (int variant = 0; variant < 2; variant++) {
        for (uint32_t f = 1; f < MaxPlayerNum; f++) {
            // second pass: the faction's first base owns the Nano Factory
            int base_id_nano = -1;
            if (variant) {
                for (int i = 0; i < *BaseCurrentCount; i++) {
                    if (Bases[i].faction_id_current == f) {
                        base_id_nano = i;
                        break;
                    }
                }
                if (base_id_nano < 0) {
                    continue;
                }
            }
            SecretProject->nano_factory = base_id_nano;
            for (int slot_old = 0; slot_old < 2 * MaxVehProtoFactionNum; slot_old++) {
                int proto_old = slot_old < MaxVehProtoFactionNum ? slot_old
                    : f * MaxVehProtoFactionNum + (slot_old - MaxVehProtoFactionNum);
                if (!(VehPrototypes[proto_old].flags & PROTO_ACTIVE)) {
                    continue;
                }
                for (int slot_new = 0; slot_new < 2 * MaxVehProtoFactionNum; slot_new += 3) {
                    int proto_new = slot_new < MaxVehProtoFactionNum ? slot_new
                        : f * MaxVehProtoFactionNum + (slot_new - MaxVehProtoFactionNum);
                    if (!(VehPrototypes[proto_new].flags & PROTO_ACTIVE)) {
                        continue;
                    }
                    int b = upgrade_cost_OG(f, proto_new, proto_old);
                    int a = upgrade_cost(f, proto_new, proto_old);
                    compared++;
                    if (a != b) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  upgrade_cost(%u, %d, %d) nano %d: new=%d orig=%d", f, proto_new,
                                proto_old, base_id_nano, a, b);
                        }
                        mismatches++;
                    }
                }
            }
        }
    }
    SecretProject->nano_factory = nano_saved;
    logf("upgrade_cost: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

// Snapshot comparison for a call that may also change the prototype table and the upgrade flag.
struct ProtoCompare {
    GameSnapshot base, after_orig, after_new;
    uint8_t *saved, *orig, *fresh;
    int flag_saved, flag_orig, flag_new;
    static const size_t size = sizeof(VehPrototype) * MaxVehProtoNum;
    void init() {
        snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
        snap_take(&base);
        saved = (uint8_t *)malloc(size); orig = (uint8_t *)malloc(size); fresh = (uint8_t *)malloc(size);
        memcpy(saved, VehPrototypes, size);
        flag_saved = *VehUpgradeUnk;
    }
    void prepare() {
        snap_restore(&base);
        memcpy(VehPrototypes, saved, size);
        *VehUpgradeUnk = flag_saved;
    }
    void take_orig() { snap_take(&after_orig); memcpy(orig, VehPrototypes, size); flag_orig = *VehUpgradeUnk; }
    void take_new() { snap_take(&after_new); memcpy(fresh, VehPrototypes, size); flag_new = *VehUpgradeUnk; }
    BOOL changed() {
        return first_diff(after_orig.vehs, base.vehs, sizeof(Veh) * base.veh_count) != (size_t)-1
            || first_diff(orig, saved, size) != (size_t)-1;
    }
    BOOL differs(const char *label, BOOL detail) {
        BOOL diff = snap_differs(&after_orig, &after_new, detail ? label : NULL);
        size_t d;
        if (!diff && (d = first_diff(orig, fresh, size)) != (size_t)-1) {
            if (detail) {
                logf("  %s: VehPrototype[%u] +0x%X differs (orig %02X new %02X)", label,
                    (unsigned)(d / sizeof(VehPrototype)), (unsigned)(d % sizeof(VehPrototype)), orig[d],
                    fresh[d]);
            }
            diff = true;
        }
        if (!diff && flag_orig != flag_new) {
            if (detail) {
                logf("  %s: upgrade flag differs (orig %d new %d)", label, flag_orig, flag_new);
            }
            diff = true;
        }
        return diff;
    }
    void done() {
        prepare();
        snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
        free(saved); free(orig); free(fresh);
    }
};

// AI factions other than the one at the controls: the upgrade code redraws the map and talks to
// humans, which the harness cannot compare
static BOOL quiet_faction(uint32_t faction_id) {
    return faction_id > 0 && faction_id < MaxPlayerNum && !is_human(faction_id)
        && faction_id != (uint32_t)*FactionIDCurrent;
}

static int test_do_upgrade() {
    ProtoCompare pc;
    pc.init();
    int compared = 0, mismatches = 0, changed = 0;
    char label[64];
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        if (!quiet_faction(f)) {
            continue;
        }
        for (int slot_old = 0; slot_old < 2 * MaxVehProtoFactionNum; slot_old++) {
            int proto_old = slot_old < MaxVehProtoFactionNum ? slot_old
                : f * MaxVehProtoFactionNum + (slot_old - MaxVehProtoFactionNum);
            // designs with units, plus every 8th without so the "nothing paid" branch runs
            if (!PlayersData[f].proto_id_active[proto_old] && (slot_old % 8)) {
                continue;
            }
            int news[4] = { BSC_SCOUT_PATROL, BSC_UNITY_ROVER, (int)f * MaxVehProtoFactionNum,
                (int)f * MaxVehProtoFactionNum + 9 };
            for (int k = 0; k < 4; k++) {
                int proto_new = news[k];
                if (proto_new == proto_old) {
                    continue;
                }
                pc.prepare();
                do_upgrade_OG(f, proto_new, proto_old);
                pc.take_orig();
                changed += pc.changed();
                pc.prepare();
                do_upgrade(f, proto_new, proto_old);
                pc.take_new();
                compared++;
                sprintf_s(label, "do_upgrade(%u, %d, %d)", f, proto_new, proto_old);
                if (pc.differs(label, mismatches < MAX_DETAIL)) {
                    mismatches++;
                }
            }
        }
    }
    pc.done();
    logf("do_upgrade: %d compared (%d changed something), %d mismatches", compared, changed, mismatches);
    return mismatches;
}

static int test_upgrade_prototypes() {
    ProtoCompare pc;
    pc.init();
    int compared = 0, mismatches = 0, changed = 0;
    char label[64];
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        if (!quiet_faction(f)) {
            continue;
        }
        for (int slot = 0; slot < 2 * MaxVehProtoFactionNum; slot++) {
            int proto_id = slot < MaxVehProtoFactionNum ? slot
                : f * MaxVehProtoFactionNum + (slot - MaxVehProtoFactionNum);
            if (!(((VehPrototype *)pc.saved)[proto_id].flags & PROTO_ACTIVE)) {
                continue;
            }
            for (int variant = 0; variant < 2; variant++) {
                // second pass: every other active design of the faction is obsolete for it
                auto patch = [&]() {
                    if (variant) {
                        for (int s = 0; s < 2 * MaxVehProtoFactionNum; s++) {
                            int id = s < MaxVehProtoFactionNum ? s
                                : f * MaxVehProtoFactionNum + (s - MaxVehProtoFactionNum);
                            if (id != proto_id) {
                                VehPrototypes[id].obsolete_factions |= (uint8_t)(1 << f);
                            }
                        }
                    }
                    reseed(proto_id * 7 + variant);
                };
                pc.prepare(); patch();
                upgrade_prototypes_OG(f, proto_id);
                pc.take_orig();
                changed += pc.changed();
                pc.prepare(); patch();
                upgrade_prototypes(f, proto_id);
                pc.take_new();
                compared++;
                sprintf_s(label, "upgrade_prototypes(%u, %d) variant %d", f, proto_id, variant);
                if (pc.differs(label, mismatches < MAX_DETAIL)) {
                    mismatches++;
                }
            }
        }
    }
    pc.done();
    logf("upgrade_prototypes: %d compared (%d changed something), %d mismatches", compared, changed,
        mismatches);
    return mismatches;
}

static int test_upgrade_any_prototypes() {
    ProtoCompare pc;
    pc.init();
    uint32_t prefs_saved = *GameMorePreferences;
    int compared = 0, mismatches = 0, changed = 0;
    char label[64];
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        BOOL quiet = quiet_faction(f);
        for (int variant = 0; variant < 2; variant++) {
            // humans only run the early return (preference off); the second pass marks every
            // built design of an AI faction as having an upgrade pending
            if (!quiet && variant) {
                continue;
            }
            auto patch = [&]() {
                *GameMorePreferences = prefs_saved & ~MPREF_BSC_AUTO_PRUNE_OBS_VEH;
                if (variant) {
                    for (int s = 0; s < 2 * MaxVehProtoFactionNum; s++) {
                        int id = s < MaxVehProtoFactionNum ? s
                            : f * MaxVehProtoFactionNum + (s - MaxVehProtoFactionNum);
                        if ((VehPrototypes[id].flags & PROTO_ACTIVE)
                            && (VehPrototypes[id].flags & PROTO_TYPED_COMPLETE)) {
                            VehPrototypes[id].flags |= PROTO_UNK_100;
                            if (s % 2) {
                                VehPrototypes[id].obsolete_factions |= (uint8_t)(1 << f);
                            }
                        }
                    }
                }
                reseed(f * 13 + variant);
            };
            pc.prepare(); patch();
            upgrade_any_prototypes_OG(f);
            pc.take_orig();
            changed += pc.changed();
            pc.prepare(); patch();
            upgrade_any_prototypes(f);
            pc.take_new();
            compared++;
            sprintf_s(label, "upgrade_any_prototypes(%u) variant %d", f, variant);
            if (pc.differs(label, mismatches < MAX_DETAIL)) {
                mismatches++;
            }
        }
    }
    pc.done();
    *GameMorePreferences = prefs_saved;
    logf("upgrade_any_prototypes: %d compared (%d changed something), %d mismatches", compared, changed,
        mismatches);
    return mismatches;
}

static int test_base_claimed() {
    int saved_id[4], saved_val[4];
    memcpy(saved_id, BaseGovPickID, sizeof(saved_id));
    memcpy(saved_val, BaseGovPickValue, sizeof(saved_val));
    static const int ids[4] = { 3, 7, 3, 12 };
    static const int vals[4] = { 5, 9, 2, 0 };
    memcpy(BaseGovPickID, ids, sizeof(ids));
    memcpy(BaseGovPickValue, vals, sizeof(vals));
    int compared = 0, mismatches = 0;
    for (int item = -1; item <= 14; item++) {
        for (int value = -1; value <= 10; value++) {
            for (int skip = 0; skip < 16; skip++) {
                BOOL b = base_claimed_OG(item, value, skip & 1, skip & 2, skip & 4, skip & 8);
                BOOL a = base_claimed(item, value, skip & 1, skip & 2, skip & 4, skip & 8);
                compared++;
                if ((a != 0) != (b != 0)) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  base_claimed(%d, %d, skip %X): new=%d orig=%d", item, value, skip, a, b);
                    }
                    mismatches++;
                }
            }
        }
    }
    memcpy(BaseGovPickID, saved_id, sizeof(saved_id));
    memcpy(BaseGovPickValue, saved_val, sizeof(saved_val));
    logf("base_claimed: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

static int test_site_at() {
    int compared = 0, mismatches = 0, computed = 0;
    int w = *MapLongitudeBounds, h = *MapLatitudeBounds;
    for (int y = 0; y < h; y++) {
        for (int x = y & 1; x < w; x += 2) {
            Map *tile = map_loc(x, y);
            Map saved = *tile;
            int unk = 1 + (x + y) % 3;
            uint32_t b = site_at_OG(x, y, unk);
            Map tile_orig = *tile;
            *tile = saved;
            uint32_t a = site_at(x, y, unk);
            Map tile_new = *tile;
            *tile = saved;
            compared++;
            computed += (saved.val2 >> 4) == 0 && !(saved.bit & BIT_BASE_RADIUS);
            if (a != b || memcmp(&tile_orig, &tile_new, sizeof(Map)) != 0) {
                if (mismatches < MAX_DETAIL) {
                    logf("  site_at(%d, %d, %d): new=%u orig=%u, tile byte %u", x, y, unk, a, b,
                        (unsigned)first_diff(&tile_orig, &tile_new, sizeof(Map)));
                }
                mismatches++;
            }
        }
    }
    logf("site_at: %d compared (%d computed by world_site), %d mismatches", compared, computed,
        mismatches);
    return mismatches;
}

static int test_spying() {
    int current_saved = *FactionIDCurrent;
    int governor_saved = *GovernorFactionID;
    int compared = 0, mismatches = 0;
    for (int current = 1; current < MaxPlayerNum; current++) {
        *FactionIDCurrent = current;
        for (int gov = 0; gov < 2; gov++) {
            *GovernorFactionID = gov ? current : 0;
            for (int f = 0; f < MaxPlayerNum; f++) {
                uint32_t treaties_saved = PlayersData[current].diplo_treaties[f];
                for (int infil = 0; infil < 2; infil++) {
                    PlayersData[current].diplo_treaties[f] = infil
                        ? (treaties_saved | DTREATY_HAVE_INFILTRATOR)
                        : (treaties_saved & ~DTREATY_HAVE_INFILTRATOR);
                    BOOL b = spying_OG(f);
                    BOOL a = spying(f);
                    compared++;
                    if ((a != 0) != (b != 0)) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  spying(%d) current %d governor %d infiltrator %d: new=%d orig=%d",
                                f, current, gov, infil, a, b);
                        }
                        mismatches++;
                    }
                }
                PlayersData[current].diplo_treaties[f] = treaties_saved;
            }
        }
    }
    *FactionIDCurrent = current_saved;
    *GovernorFactionID = governor_saved;
    logf("spying: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

static int test_act_of_aggression() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0, broken = 0;
    char label[64];
    for (uint32_t f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (uint32_t f2 = 1; f2 < MaxPlayerNum; f2++) {
            // double_cross() may address a human; AI pairs only
            if (f1 == f2 || is_human(f1) || is_human(f2)) {
                continue;
            }
            for (int variant = 0; variant < 2; variant++) {
                auto prepare = [&]() {
                    snap_restore(&base);
                    if (variant) {
                        PlayersData[f2].diplo_treaties[f1] |= DTREATY_TREATY;
                        PlayersData[f1].diplo_treaties[f2] |= DTREATY_TREATY;
                    }
                    reseed(f1 * 31 + f2 * 7 + variant);
                };
                prepare();
                BOOL b = act_of_aggression_OG(f1, f2);
                snap_take(&after_orig);
                prepare();
                BOOL a = act_of_aggression(f1, f2);
                snap_take(&after_new);
                compared++;
                broken += (b != 0);
                sprintf_s(label, "act_of_aggression(%u, %u) variant %d", f1, f2, variant);
                BOOL diff = false;
                if ((a != 0) != (b != 0)) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  %s: new=%d orig=%d", label, a, b);
                    }
                    diff = true;
                }
                if (!diff) {
                    diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
                }
                if (diff) {
                    mismatches++;
                }
            }
        }
    }
    snap_restore(&base);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("act_of_aggression: %d compared (%d broke an agreement), %d mismatches", compared, broken,
        mismatches);
    return mismatches;
}

static int test_council_get_vote() {
    int compared = 0, mismatches = 0;
    static const int pledges[3] = { 0, -1, 3 };
    for (int f = 1; f < MaxPlayerNum; f++) {
        int vote_saved[MaxProposalNum], faction_saved[MaxProposalNum];
        memcpy(vote_saved, PlayersData[f].council_pledge_vote, sizeof(vote_saved));
        memcpy(faction_saved, PlayersData[f].council_pledge_faction, sizeof(faction_saved));
        for (int proposal = 0; proposal < MaxProposalNum; proposal++) {
            for (int proposer = 1; proposer < MaxPlayerNum; proposer++) {
                for (int variant = 0; variant < 3; variant++) {
                    PlayersData[f].council_pledge_vote[proposal] = pledges[variant];
                    PlayersData[f].council_pledge_faction[proposal] = variant == 2
                        ? proposer : 1 + (proposer + variant) % (MaxPlayerNum - 1);
                    int b = council_get_vote_OG(f, proposal, proposer);
                    int a = council_get_vote(f, proposal, proposer);
                    compared++;
                    if (a != b) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  council_get_vote(%d, %d, %d) pledge %d: new=%d orig=%d", f, proposal,
                                proposer, pledges[variant], a, b);
                        }
                        mismatches++;
                    }
                }
            }
        }
        memcpy(PlayersData[f].council_pledge_vote, vote_saved, sizeof(vote_saved));
        memcpy(PlayersData[f].council_pledge_faction, faction_saved, sizeof(faction_saved));
    }
    logf("council_get_vote: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

static int test_can_call_council() {
    int sunspot_saved = *SunspotDuration;
    BOOL pbem_saved = *IsMultiplayerPBEM;
    int pending_saved = *CouncilSessionPending;
    uint32_t more_rules_saved = *GameMoreRules;
    uint32_t state_saved = *GameState;
    BOOL net_saved = *IsMultiplayerNet;
    int compared = 0, mismatches = 0, allowed = 0;
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        for (int variant = 0; variant < 64; variant++) {
            *SunspotDuration = (variant & 1) ? 5 : 0;
            *IsMultiplayerPBEM = (variant & 2) != 0;
            *CouncilSessionPending = (variant & 4) != 0;
            *GameMoreRules = (variant & 8) ? (more_rules_saved | MRULES_NO_PLANETARY_COUNCIL)
                : (more_rules_saved & ~MRULES_NO_PLANETARY_COUNCIL);
            *IsMultiplayerNet = (variant & 16) != 0;
            *GameState = (variant & 32) ? (state_saved | STATE_OMNISCIENT_VIEW)
                : (state_saved & ~STATE_OMNISCIENT_VIEW);
            // the announcement only pops for the faction at the controls
            BOOL announce = f != (uint32_t)*FactionIDCurrent;
            BOOL b = can_call_council_OG(f, announce);
            BOOL a = can_call_council(f, announce);
            compared++;
            allowed += (b != 0);
            if ((a != 0) != (b != 0)) {
                if (mismatches < MAX_DETAIL) {
                    logf("  can_call_council(%u, %d) variant %X: new=%d orig=%d", f, announce, variant, a, b);
                }
                mismatches++;
            }
        }
    }
    *SunspotDuration = sunspot_saved;
    *IsMultiplayerPBEM = pbem_saved;
    *CouncilSessionPending = pending_saved;
    *GameMoreRules = more_rules_saved;
    *GameState = state_saved;
    *IsMultiplayerNet = net_saved;
    logf("can_call_council: %d compared (%d allowed), %d mismatches", compared, allowed, mismatches);
    return mismatches;
}

static int test_pick_top_veh() {
    int selected_saved = *MapWinVehIDSelected;
    int boom_saved = *VehDrawBoomID;
    BOOL mode_saved = *MapWinUnitNotViewMode;
    int flag_saved = *MapWinUnkFlag;
    int compared = 0, mismatches = 0;
    for (int veh_id = -1; veh_id < *VehCurrentCount; veh_id++) {
        for (int variant = 0; variant < 8; variant++) {
            *MapWinVehIDSelected = (variant & 1) ? veh_id : -1;
            *VehDrawBoomID = (variant & 2) ? veh_id : -1;
            *MapWinUnitNotViewMode = (variant & 4) != 0;
            *MapWinUnkFlag = 1;
            int b = pick_top_veh_OG(veh_id);
            int a = pick_top_veh(veh_id);
            compared++;
            if (a != b) {
                if (mismatches < MAX_DETAIL) {
                    logf("  pick_top_veh(%d) variant %d: new=%d orig=%d", veh_id, variant, a, b);
                }
                mismatches++;
            }
        }
    }
    *MapWinVehIDSelected = selected_saved;
    *VehDrawBoomID = boom_saved;
    *MapWinUnitNotViewMode = mode_saved;
    *MapWinUnkFlag = flag_saved;
    logf("pick_top_veh: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

static int test_can_convoy() {
    int compared = 0, mismatches = 0, blocked = 0;
    int w = *MapLongitudeBounds, h = *MapLatitudeBounds;
    for (int y = 0; y < h; y++) {
        for (int x = y & 1; x < w; x += 2) {
            BOOL b = can_convoy_OG(x, y);
            BOOL a = can_convoy(x, y);
            compared++;
            blocked += (b == 0);
            if ((a != 0) != (b != 0)) {
                if (mismatches < MAX_DETAIL) {
                    logf("  can_convoy(%d, %d): new=%d orig=%d", x, y, a, b);
                }
                mismatches++;
            }
        }
    }
    logf("can_convoy: %d compared (%d blocked), %d mismatches", compared, blocked, mismatches);
    return mismatches;
}

static int test_promote() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0, promoted = 0;
    char label[64];
    for (int veh_id = 0; veh_id < base.veh_count; veh_id++) {
        // the faction at the controls gets a popup
        if (Vehs[veh_id].faction_id == *FactionIDCurrent && is_human(Vehs[veh_id].faction_id)) {
            continue;
        }
        for (int variant = 0; variant < 2; variant++) {
            auto prepare = [&]() {
                snap_restore(&base);
                if (variant) {
                    Vehs[veh_id].morale = MORALE_GREEN;
                }
                reseed(veh_id * 17 + variant);
            };
            prepare();
            promote_OG(veh_id);
            snap_take(&after_orig);
            promoted += after_orig.vehs[veh_id].morale != (variant ? MORALE_GREEN : base.vehs[veh_id].morale);
            prepare();
            promote(veh_id);
            snap_take(&after_new);
            compared++;
            sprintf_s(label, "promote(%d) variant %d", veh_id, variant);
            if (snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL)) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("promote: %d compared (%d promotions by the original), %d mismatches", compared, promoted,
        mismatches);
    return mismatches;
}

static int test_proto_sort() {
    const int n = 2 * MaxVehProtoFactionNum;
    int count_saved = *ProtoSortCount;
    int list_saved[n], list_orig[n], list_new[n];
    memcpy(list_saved, ProtoSortList, sizeof(list_saved));
    int compared = 0, mismatches = 0;
    for (int which = 0; which < 2; which++) {
        for (uint32_t f = 0; f < MaxPlayerNum; f++) {
            *ProtoSortCount = -1;
            memset(ProtoSortList, 0xEE, sizeof(list_saved));
            (which ? proto_sort_2_OG : proto_sort_OG)(f);
            int count_orig = *ProtoSortCount;
            memcpy(list_orig, ProtoSortList, sizeof(list_orig));
            *ProtoSortCount = -1;
            memset(ProtoSortList, 0xEE, sizeof(list_saved));
            (which ? proto_sort_2 : proto_sort)(f);
            int count_new = *ProtoSortCount;
            memcpy(list_new, ProtoSortList, sizeof(list_new));
            compared++;
            if (count_orig != count_new || memcmp(list_orig, list_new, sizeof(list_orig)) != 0) {
                if (mismatches < MAX_DETAIL) {
                    int i = 0;
                    while (i < n && list_orig[i] == list_new[i]) i++;
                    logf("  proto_sort%s(%u): count orig %d new %d, first list difference at %d (orig %d new %d)",
                        which ? "_2" : "", f, count_orig, count_new, i, i < n ? list_orig[i] : -1,
                        i < n ? list_new[i] : -1);
                }
                mismatches++;
            }
        }
    }
    *ProtoSortCount = count_saved;
    memcpy(ProtoSortList, list_saved, sizeof(list_saved));
    logf("proto_sort: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

static int test_yearmotize() {
    static const char *samples[] = { "", "2101", "In 2200 the", "x12345y 2345 1", "a2101", "2101b 99 999 0000",
        "12 34 5678 9", "Year 2345 and 2346.", "1234", "123", "M.Y. 2389: Turn 289", "00001", "1000 1001 100" };
    int compared = 0, mismatches = 0;
    for (int i = 0; i < (int)(sizeof(samples) / sizeof(samples[0])); i++) {
        char orig[128], fresh[128];
        strcpy_s(orig, 128, samples[i]);
        strcpy_s(fresh, 128, samples[i]);
        yearmotize_OG(orig);
        yearmotize(fresh);
        compared++;
        if (strcmp(orig, fresh) != 0) {
            if (mismatches < MAX_DETAIL) {
                logf("  yearmotize(\"%s\"): new=\"%s\" orig=\"%s\"", samples[i], fresh, orig);
            }
            mismatches++;
        }
    }
    logf("yearmotize: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 4: world generators

struct WorldGenFn {
    const char *name;
    func_void *orig;
    func_void *neu;
    BOOL empty_world; // the generator calls alt_set()/veh_init(), so run without bases and units
};

static int test_world_batch4() {
    static const WorldGenFn fns[] = {
        { "world_rocky", world_rocky_OG, world_rocky, false },
        { "world_fungus", world_fungus_OG, world_fungus, true },
        { "world_shorelines", world_shorelines_OG, world_shorelines, true },
        { "world_rainfall", world_rainfall_OG, world_rainfall, false },
    };
    int total = 0;
    char label[96];
    for (size_t i = 0; i < sizeof(fns) / sizeof(fns[0]); i++) {
        if (!test_enabled("batch4", fns[i].name)) {
            continue;
        }
        SnapHarness h(fns[i].name);
        if (fns[i].empty_world) {
            h.empty_world();
        }
        int base_saved = *MapRainfallChangeBaseID, faction_saved = *MapRainfallChangeFactionID;
        int extra_mismatches = 0;
        for (uint32_t seed = 1; seed <= 3; seed++) {
            int base_orig = 0, faction_orig = 0, base_new = 0, faction_new = 0;
            sprintf_s(label, "%s() seed %u", fns[i].name, seed);
            h.compare(label,
                [&] {
                    reseed(seed);
                    fns[i].orig();
                    base_orig = *MapRainfallChangeBaseID;
                    faction_orig = *MapRainfallChangeFactionID;
                },
                [&] {
                    reseed(seed);
                    fns[i].neu();
                    base_new = *MapRainfallChangeBaseID;
                    faction_new = *MapRainfallChangeFactionID;
                });
            if (base_orig != base_new || faction_orig != faction_new) {
                logf("  %s: rainfall change globals differ (orig base %d faction %d, new base %d faction %d)",
                    label, base_orig, faction_orig, base_new, faction_new);
                extra_mismatches++;
            }
        }
        *MapRainfallChangeBaseID = base_saved;
        *MapRainfallChangeFactionID = faction_saved;
        total += h.report() + extra_mismatches;
    }
    return total;
}

// ---------------------------------------------------------------- batch 5: tech awards

// tech_achieved() runs the whole AI tech/design chain over state outside the snapshot, so for the
// duration of a test its entry point becomes a stub that records the tech id and returns (both
// sides call the same address). Same technique as the study_artifact test.
static int TechAchievedRecorded = -2;
static int TechAchievedCalls = 0;
struct TechAchievedStub {
    uint8_t *entry;
    uint8_t saved_code[10];
    DWORD old_protect;
    BOOL ok;
    TechAchievedStub() : entry((uint8_t *)tech_achieved), old_protect(0), ok(false) {
        uint8_t stub[10] = { 0x8B, 0x44, 0x24, 0x08, 0xA3, 0, 0, 0, 0, 0xC3 };
        int *recorder = &TechAchievedRecorded;
        memcpy(stub + 5, &recorder, 4);
        if (!VirtualProtect(entry, sizeof(stub), PAGE_EXECUTE_READWRITE, &old_protect)) {
            logf("cannot unprotect tech_achieved (%u)", GetLastError());
            return;
        }
        memcpy(saved_code, entry, sizeof(stub));
        memcpy(entry, stub, sizeof(stub));
        FlushInstructionCache(GetCurrentProcess(), entry, sizeof(stub));
        ok = true;
    }
    ~TechAchievedStub() {
        if (ok) {
            memcpy(entry, saved_code, sizeof(saved_code));
            FlushInstructionCache(GetCurrentProcess(), entry, sizeof(saved_code));
            VirtualProtect(entry, sizeof(saved_code), old_protect, &old_protect);
        }
    }
};

// mon_tech_discovered() writes the monument table, which the snapshot does not hold
static uint8_t *const MonumentTable = (uint8_t *)0x0094CA08;
static const size_t MonumentTableSize = 0x94F1F4 - 0x94CA08;

struct TechCompare {
    GameSnapshot base, after_orig, after_new;
    uint8_t *mon_saved, *mon_orig, *mon_new;
    int tech_orig, tech_new, calls_orig, calls_new;
    int selected_saved;
    void init() {
        snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
        snap_take(&base);
        mon_saved = (uint8_t *)malloc(MonumentTableSize);
        mon_orig = (uint8_t *)malloc(MonumentTableSize);
        mon_new = (uint8_t *)malloc(MonumentTableSize);
        memcpy(mon_saved, MonumentTable, MonumentTableSize);
        selected_saved = *BaseIDCurrentSelected;
    }
    void prepare(uint32_t seed) {
        snap_restore(&base);
        memcpy(MonumentTable, mon_saved, MonumentTableSize);
        *BaseIDCurrentSelected = selected_saved;
        TechAchievedRecorded = -2;
        TechAchievedCalls = 0;
        reseed(seed);
    }
    void take_orig() { snap_take(&after_orig); memcpy(mon_orig, MonumentTable, MonumentTableSize); tech_orig = TechAchievedRecorded; }
    void take_new() { snap_take(&after_new); memcpy(mon_new, MonumentTable, MonumentTableSize); tech_new = TechAchievedRecorded; }
    BOOL differs(const char *label, BOOL detail) {
        if (tech_orig != tech_new) {
            if (detail) {
                logf("  %s: tech_achieved got orig %d new %d", label, tech_orig, tech_new);
            }
            return true;
        }
        if (snap_differs(&after_orig, &after_new, detail ? label : NULL)) {
            return true;
        }
        size_t d = first_diff(mon_orig, mon_new, MonumentTableSize);
        if (d != (size_t)-1) {
            if (detail) {
                logf("  %s: monument table byte %u differs (orig %02X new %02X)", label, (unsigned)d,
                    mon_orig[d], mon_new[d]);
            }
            return true;
        }
        return false;
    }
    void done() {
        prepare(0);
        snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
        free(mon_saved); free(mon_orig); free(mon_new);
    }
};

static int test_tech_advance() {
    TechAchievedStub stub;
    if (!stub.ok) {
        return 1;
    }
    TechCompare tc;
    tc.init();
    int compared = 0, mismatches = 0, awarded = 0;
    char label[80];
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        if (!quiet_faction(f)) {
            continue;
        }
        for (int variant = 0; variant < 3; variant++) {
            // as loaded; no research goal set (tech_selection picks one); a goal of 5
            auto patch = [&]() {
                if (variant == 1) {
                    PlayersData[f].tech_id_researching = -1;
                } else if (variant == 2) {
                    PlayersData[f].tech_id_researching = 5;
                }
            };
            tc.prepare(f * 11 + variant); patch();
            int b = tech_advance_OG(f);
            tc.take_orig();
            tc.prepare(f * 11 + variant); patch();
            int a = tech_advance(f);
            tc.take_new();
            compared++;
            awarded += (b >= 0);
            sprintf_s(label, "tech_advance(%u) variant %d", f, variant);
            BOOL diff = false;
            if (a != b) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: return orig %d new %d", label, b, a);
                }
                diff = true;
            }
            if (!diff) {
                diff = tc.differs(label, mismatches < MAX_DETAIL);
            }
            if (diff) {
                mismatches++;
            }
        }
    }
    tc.done();
    logf("tech_advance: %d compared (%d awarded), %d mismatches", compared, awarded, mismatches);
    return mismatches;
}

static int test_tech_research() {
    TechAchievedStub stub;
    if (!stub.ok) {
        return 1;
    }
    TechCompare tc;
    tc.init();
    int compared = 0, mismatches = 0, awarded = 0;
    char label[80];
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        if (!quiet_faction(f)) {
            continue;
        }
        int cost = tech_rate(f);
        int labs_list[5] = { 0, 1, cost / 2, cost + 5, 3 * cost + 7 };
        for (int k = 0; k < 5; k++) {
            for (int variant = 0; variant < 3; variant++) {
                // as loaded; with a secret advance pending; with no research goal
                auto patch = [&]() {
                    if (variant == 1) {
                        PlayersData[f].flags |= PFLAG_UNK_40;
                    } else if (variant == 2) {
                        PlayersData[f].tech_id_researching = -1;
                    }
                };
                tc.prepare(f * 29 + k * 5 + variant); patch();
                tech_research_OG(f, labs_list[k]);
                tc.take_orig();
                awarded += (tc.tech_orig >= 0);
                tc.prepare(f * 29 + k * 5 + variant); patch();
                tech_research(f, labs_list[k]);
                tc.take_new();
                compared++;
                sprintf_s(label, "tech_research(%u, %d) variant %d", f, labs_list[k], variant);
                if (tc.differs(label, mismatches < MAX_DETAIL)) {
                    mismatches++;
                }
            }
        }
    }
    tc.done();
    logf("tech_research: %d compared (%d awards by the original), %d mismatches", compared, awarded,
        mismatches);
    return mismatches;
}

static int test_steal_tech() {
    TechAchievedStub stub;
    if (!stub.ok) {
        return 1;
    }
    TechCompare tc;
    tc.init();
    int compared = 0, mismatches = 0, empty = 0;
    char label[80];
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        if (!quiet_faction(f)) {
            continue;
        }
        for (uint32_t tgt = 1; tgt < MaxPlayerNum; tgt++) {
            // the victim at the controls gets a popup
            if (tgt == f || tgt == (uint32_t)*FactionIDCurrent) {
                continue;
            }
            for (int is_steal = 0; is_steal < 2; is_steal++) {
                tc.prepare(f * 17 + tgt * 3 + is_steal);
                BOOL b = steal_tech_OG(f, tgt, is_steal);
                tc.take_orig();
                tc.prepare(f * 17 + tgt * 3 + is_steal);
                BOOL a = steal_tech(f, tgt, is_steal);
                tc.take_new();
                compared++;
                empty += (b != 0);
                sprintf_s(label, "steal_tech(%u, %u, %d)", f, tgt, is_steal);
                BOOL diff = false;
                if ((a != 0) != (b != 0)) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  %s: return orig %d new %d", label, b, a);
                    }
                    diff = true;
                }
                if (!diff) {
                    diff = tc.differs(label, mismatches < MAX_DETAIL);
                }
                if (diff) {
                    mismatches++;
                }
            }
        }
    }
    tc.done();
    logf("steal_tech: %d compared (%d came up empty), %d mismatches", compared, empty, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 6

static int test_num_objectives() {
    uint32_t rules_saved = *GameRules, state_saved = *GameState;
    static const uint32_t rule_bits[5] = { RULES_SCN_VICT_OBJ_UNITS_REACH_FRIEND_OBJ_BASE,
        RULES_SCN_VICT_OBJ_UNITS_REACH_FRIEND_HQ_BASE, RULES_SCN_VICT_ALL_BASE_COUNT_OBJ,
        RULES_SCN_VICT_SP_COUNT_OBJ, RULES_VICTORY_COOPERATIVE };
    static const uint32_t state_bits[6] = { (uint32_t)STATE_SCN_VICT_BASE_FACIL_COUNT_OBJ,
        (uint32_t)STATE_SCN_VICT_POPULATION_COUNT_OBJ, (uint32_t)STATE_SCN_VICT_TECH_COUNT_OBJ,
        (uint32_t)STATE_SCN_VICT_CREDITS_COUNT_OBJ, (uint32_t)STATE_SCN_VICT_TERRAIN_ENH_COUNT_OBJ,
        (uint32_t)STATE_SCN_VICT_TERRITORY_COUNT_OBJ };
    // a few objective units and objective bases so the unit/base branches have something to count
    int patched_vehs[6], patched_bases[3];
    for (int k = 0; k < 6; k++) {
        int v = (k * 7919) % *VehCurrentCount;
        patched_vehs[k] = v;
        Vehs[v].flags |= VFLAG_IS_OBJECTIVE;
    }
    for (int k = 0; k < 3; k++) {
        int b = (k * 97) % *BaseCurrentCount;
        patched_bases[k] = b;
        Bases[b].event |= BEVENT_OBJECTIVE;
    }
    int compared = 0, mismatches = 0;
    for (uint32_t rv = 0; rv < 32; rv++) {
        uint32_t rules = rules_saved;
        for (int k = 0; k < 5; k++) {
            rules = (rv & (1 << k)) ? (rules | rule_bits[k]) : (rules & ~rule_bits[k]);
        }
        *GameRules = rules;
        for (uint32_t sv = 0; sv < 64; sv += 9) {
            uint32_t state = state_saved;
            for (int k = 0; k < 6; k++) {
                state = (sv & (1 << k)) ? (state | state_bits[k]) : (state & ~state_bits[k]);
            }
            *GameState = state;
            for (uint32_t f = 0; f < MaxPlayerNum; f++) {
                for (int pact = 0; pact < 2; pact++) {
                    int b = num_objectives_OG(f, pact);
                    int a = num_objectives(f, pact);
                    compared++;
                    if (a != b) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  num_objectives(%u, %d) rules %X state %X: new=%d orig=%d", f, pact,
                                rv, sv, a, b);
                        }
                        mismatches++;
                    }
                }
            }
        }
    }
    *GameRules = rules_saved;
    *GameState = state_saved;
    for (int k = 0; k < 6; k++) {
        Vehs[patched_vehs[k]].flags &= ~VFLAG_IS_OBJECTIVE;
    }
    for (int k = 0; k < 3; k++) {
        Bases[patched_bases[k]].event &= ~BEVENT_OBJECTIVE;
    }
    logf("num_objectives: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

typedef BOOL func_battle_kill(int, int *, int *, int *, int, int);
typedef void func_battle_kill_n(int, int *, int *, int *, int, int);

static int test_battle_kill_family() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    BOOL draw_saved = VehBattleUnkTgl[0];
    VehBattleUnkTgl[0] = true;
    int compared = 0, mismatches = 0, killed_total = 0, captured_total = 0;
    char label[96];
    // an attacker of each faction (the first unit found), plus "no attacker"
    int attackers[MaxPlayerNum + 1];
    int attacker_count = 0;
    attackers[attacker_count++] = -1;
    for (uint32_t f = 0; f < MaxPlayerNum; f++) {
        for (int i = 0; i < base.veh_count; i++) {
            if (Vehs[i].faction_id == f && Vehs[i].x >= 0) {
                attackers[attacker_count++] = i;
                break;
            }
        }
    }
    for (int which = 0; which < 3; which++) {
        const char *name = which == 0 ? "battle_kill" : which == 1 ? "battle_kill_stack" : "battle_kill_chaff";
        for (int veh_id = which; veh_id < base.veh_count; veh_id += 3) {
            // the human player's assistant worm opens an interlude
            if (base.vehs[veh_id].state & VSTATE_ASSISTANT_WORM) {
                continue;
            }
            for (int a = 0; a < attacker_count; a += (which == 0 ? 1 : 3)) {
                int veh_id_atk = attackers[a];
                int faction_obj = (veh_id + a) % 2 ? 1 + (veh_id % 7) : -1;
                int k_o = 0, c_o = 0, m_o = 0, k_n = 0, c_n = 0, m_n = 0;
                BOOL r_o = 0, r_n = 0;
                // every third victim of battle_kill() becomes an Alien Artifact so the capture branch runs
                BOOL artifact = which == 0 && (veh_id / 3) % 3 == 0;
                snap_restore(&base);
                if (artifact) {
                    Vehs[veh_id].proto_id = BSC_ALIEN_ARTIFACT;
                }
                reseed(veh_id * 13 + a);
                if (which == 0) {
                    r_o = battle_kill_OG(veh_id, &k_o, &c_o, &m_o, veh_id_atk, faction_obj);
                } else if (which == 1) {
                    battle_kill_stack_OG(veh_id, &k_o, &c_o, &m_o, veh_id_atk, faction_obj);
                } else {
                    battle_kill_chaff_OG(veh_id, &k_o, &c_o, &m_o, veh_id_atk, faction_obj);
                }
                snap_take(&after_orig);
                snap_restore(&base);
                if (artifact) {
                    Vehs[veh_id].proto_id = BSC_ALIEN_ARTIFACT;
                }
                reseed(veh_id * 13 + a);
                if (which == 0) {
                    r_n = battle_kill(veh_id, &k_n, &c_n, &m_n, veh_id_atk, faction_obj);
                } else if (which == 1) {
                    battle_kill_stack(veh_id, &k_n, &c_n, &m_n, veh_id_atk, faction_obj);
                } else {
                    battle_kill_chaff(veh_id, &k_n, &c_n, &m_n, veh_id_atk, faction_obj);
                }
                snap_take(&after_new);
                compared++;
                killed_total += k_o;
                captured_total += c_o;
                sprintf_s(label, "%s(%d, atk %d, obj %d)", name, veh_id, veh_id_atk, faction_obj);
                BOOL diff = false;
                if ((r_o != 0) != (r_n != 0) || k_o != k_n || c_o != c_n || m_o != m_n) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  %s: orig ret %d killed %d captured %d morale %d; new ret %d killed %d captured %d morale %d",
                            label, r_o, k_o, c_o, m_o, r_n, k_n, c_n, m_n);
                    }
                    diff = true;
                }
                if (!diff) {
                    diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
                }
                if (diff) {
                    mismatches++;
                }
            }
        }
    }
    snap_restore(&base);
    VehBattleUnkTgl[0] = draw_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("battle_kill family: %d compared (%d killed, %d captured by the original), %d mismatches",
        compared, killed_total, captured_total, mismatches);
    return mismatches;
}

static int test_monuments() {
    uint8_t *saved = (uint8_t *)malloc(MonumentTableSize);
    uint8_t *orig = (uint8_t *)malloc(MonumentTableSize);
    uint8_t *fresh = (uint8_t *)malloc(MonumentTableSize);
    memcpy(saved, MonumentTable, MonumentTableSize);
    int compared = 0, mismatches = 0;
    char label[64];
    auto compare = [&](const char *what) {
        compared++;
        size_t d = first_diff(orig, fresh, MonumentTableSize);
        if (d != (size_t)-1) {
            if (mismatches < MAX_DETAIL) {
                logf("  %s: monument table byte %u differs (orig %02X new %02X)", what, (unsigned)d,
                    orig[d], fresh[d]);
            }
            mismatches++;
        }
    };
    memcpy(MonumentTable, saved, MonumentTableSize);
    clear_monuments_OG();
    memcpy(orig, MonumentTable, MonumentTableSize);
    memcpy(MonumentTable, saved, MonumentTableSize);
    clear_monuments();
    memcpy(fresh, MonumentTable, MonumentTableSize);
    compare("clear_monuments()");
    // from the loaded table and from a cleared one, with the discovery flag of other factions varied
    for (int cleared = 0; cleared < 2; cleared++) {
        for (uint32_t f = 1; f < MaxPlayerNum; f++) {
            if (f == (uint32_t)*FactionIDCurrent) {
                continue; // opens the monument screen
            }
            for (int variant = 0; variant < 3; variant++) {
                auto prepare = [&]() {
                    memcpy(MonumentTable, saved, MonumentTableSize);
                    if (cleared) {
                        clear_monuments();
                    }
                    if (variant) {
                        for (uint32_t i = 1; i < MaxPlayerNum; i += variant) {
                            if (i != f) {
                                Monuments[i].records[MONUMENT_TECH_DISCOVERED].achieved = true;
                                Monuments[i].records[MONUMENT_TECH_DISCOVERED].faction_count = (i == 2);
                            }
                        }
                    }
                };
                prepare();
                mon_tech_discovered_OG(f, 40 + variant);
                memcpy(orig, MonumentTable, MonumentTableSize);
                prepare();
                mon_tech_discovered(f, 40 + variant);
                memcpy(fresh, MonumentTable, MonumentTableSize);
                sprintf_s(label, "mon_tech_discovered(%u) cleared %d variant %d", f, cleared, variant);
                compare(label);
            }
        }
    }
    memcpy(MonumentTable, saved, MonumentTableSize);
    free(saved); free(orig); free(fresh);
    logf("monuments: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

static int test_social_set() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0, changed = 0;
    char label[64];
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        if (!quiet_faction(f)) {
            continue; // humans get their bases recomputed, the current player a redraw
        }
        for (int variant = 0; variant < 4; variant++) {
            auto prepare = [&]() {
                snap_restore(&base);
                SocialCategory *cat = &PlayersData[f].soc_category_pending;
                if (variant == 1) {
                    cat->economics = (cat->economics + 1) % 4;
                } else if (variant == 2) {
                    cat->politics = (cat->politics + 1) % 4;
                    cat->values = (cat->values + 2) % 4;
                } else if (variant == 3) {
                    cat->future = (cat->future + 1) % 4;
                }
            };
            prepare();
            social_set_OG(f);
            snap_take(&after_orig);
            changed += snap_differs(&base, &after_orig, NULL);
            prepare();
            social_set(f);
            snap_take(&after_new);
            compared++;
            sprintf_s(label, "social_set(%u) variant %d", f, variant);
            if (snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL)) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("social_set: %d compared (%d changed state), %d mismatches", compared, changed, mismatches);
    return mismatches;
}

static int test_bases_reset() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int pick_saved[8];
    memcpy(pick_saved, BaseGovPickID, 16);
    memcpy(pick_saved + 4, BaseGovPickValue, 16);
    int compared = 0, mismatches = 0, changed = 0;
    char label[64];
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        if (!quiet_faction(f)) {
            continue;
        }
        // every base, one region, with and without the occupied-tile skip
        int region = -1;
        for (int i = 0; i < *BaseCurrentCount; i++) {
            if (Bases[i].faction_id_current == f) {
                region = map_loc(Bases[i].x, Bases[i].y)->region;
                break;
            }
        }
        for (int variant = 0; variant < 4; variant++) {
            int region_arg = (variant & 1) ? region : -1;
            BOOL skip = (variant & 2) != 0;
            auto prepare = [&]() {
                snap_restore(&base);
                memcpy(BaseGovPickID, pick_saved, 16);
                memcpy(BaseGovPickValue, pick_saved + 4, 16);
                reseed(f * 7 + variant);
            };
            prepare();
            bases_reset_OG(region_arg, f, skip);
            snap_take(&after_orig);
            int pick_orig[8];
            memcpy(pick_orig, BaseGovPickID, 16);
            memcpy(pick_orig + 4, BaseGovPickValue, 16);
            changed += snap_differs(&base, &after_orig, NULL);
            prepare();
            bases_reset(region_arg, f, skip);
            snap_take(&after_new);
            int pick_new[8];
            memcpy(pick_new, BaseGovPickID, 16);
            memcpy(pick_new + 4, BaseGovPickValue, 16);
            compared++;
            sprintf_s(label, "bases_reset(%d, %u, %d)", region_arg, f, skip);
            BOOL diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
            if (!diff && memcmp(pick_orig, pick_new, sizeof(pick_orig)) != 0) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: governor picks differ", label);
                }
                diff = true;
            }
            if (diff) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    memcpy(BaseGovPickID, pick_saved, 16);
    memcpy(BaseGovPickValue, pick_saved + 4, 16);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("bases_reset: %d compared (%d changed state), %d mismatches", compared, changed, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 7: tile yields

typedef int func_yield(uint32_t, int, int, int, BOOL);

static int test_yields() {
    struct YieldFn { const char *name; func_yield *orig; func_yield *neu; };
    static const YieldFn fns[3] = {
        { "crop_yield", (func_yield *)crop_yield_OG, (func_yield *)crop_yield },
        { "mine_yield", (func_yield *)mine_yield_OG, (func_yield *)mine_yield },
        { "energy_yield", (func_yield *)energy_yield_OG, (func_yield *)energy_yield },
    };
    int reduce_saved = *BaseTerraformReduce, energy_saved = *BaseTerraformEnergy;
    int selected_saved = *BaseIDCurrentSelected;
    set_base(0); // energy_yield reads the current base's owner for echelon mirrors
    int total = 0;
    int w = *MapLongitudeBounds, h = *MapLatitudeBounds;
    for (int k = 0; k < 3; k++) {
        int compared = 0, mismatches = 0, nonzero = 0;
        for (uint32_t f = 1; f < MaxPlayerNum; f++) {
            int base_ids[2] = { -1, -1 };
            for (int i = 0; i < *BaseCurrentCount; i++) {
                if (Bases[i].faction_id_current == f) {
                    base_ids[1] = i;
                    break;
                }
            }
            for (int b = 0; b < 2; b++) {
                if (b == 1 && base_ids[1] < 0) {
                    continue;
                }
                for (int flag = 0; flag < 2; flag++) {
                    for (int y = (int)(f + k) % 2; y < h; y += 2) {
                        for (int x = y & 1; x < w; x += 2) {
                            *BaseTerraformReduce = 0; *BaseTerraformEnergy = -7;
                            int r_o = fns[k].orig(f, base_ids[b], x, y, flag);
                            int red_o = *BaseTerraformReduce, en_o = *BaseTerraformEnergy;
                            *BaseTerraformReduce = 0; *BaseTerraformEnergy = -7;
                            int r_n = fns[k].neu(f, base_ids[b], x, y, flag);
                            int red_n = *BaseTerraformReduce, en_n = *BaseTerraformEnergy;
                            compared++;
                            nonzero += (r_o != 0);
                            if (r_o != r_n || red_o != red_n || en_o != en_n) {
                                if (mismatches < MAX_DETAIL) {
                                    logf("  %s(%u, %d, %d, %d, %d): orig %d (reduce %d energy %d) new %d (reduce %d energy %d)",
                                        fns[k].name, f, base_ids[b], x, y, flag, r_o, red_o, en_o, r_n, red_n, en_n);
                                }
                                mismatches++;
                            }
                        }
                    }
                }
            }
        }
        logf("%s: %d compared (%d non-zero), %d mismatches", fns[k].name, compared, nonzero, mismatches);
        total += mismatches;
    }
    // resource_yield: the dispatcher, every type on a sample of tiles
    {
        int compared = 0, mismatches = 0;
        for (uint32_t type = 0; type < 5; type++) {
            for (int y = 0; y < h; y += 7) {
                for (int x = y & 1; x < w; x += 6) {
                    *BaseTerraformReduce = 5;
                    int r_o = resource_yield_OG(type, 2, -1, x, y);
                    int red_o = *BaseTerraformReduce;
                    *BaseTerraformReduce = 5;
                    int r_n = resource_yield(type, 2, -1, x, y);
                    int red_n = *BaseTerraformReduce;
                    compared++;
                    if (r_o != r_n || red_o != red_n) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  resource_yield(%u, 2, -1, %d, %d): orig %d (reduce %d) new %d (reduce %d)",
                                type, x, y, r_o, red_o, r_n, red_n);
                        }
                        mismatches++;
                    }
                }
            }
        }
        logf("resource_yield: %d compared, %d mismatches", compared, mismatches);
        total += mismatches;
    }
    *BaseTerraformReduce = reduce_saved;
    *BaseTerraformEnergy = energy_saved;
    if (selected_saved >= 0 && selected_saved < *BaseCurrentCount) {
        set_base(selected_saved);
    }
    return total;
}

// ---------------------------------------------------------------- batch 8

static int test_world_site() {
    int compared = 0, mismatches = 0, usable = 0;
    int w = *MapLongitudeBounds, h = *MapLatitudeBounds;
    for (uint32_t sea = 0; sea < 2; sea++) {
        for (int y = 0; y < h; y++) {
            for (int x = y & 1; x < w; x += 2) {
                int b = world_site_OG(x, y, sea);
                int a = world_site(x, y, sea);
                compared++;
                usable += (b != 0);
                if (a != b) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  world_site(%d, %d, %u): new=%d orig=%d", x, y, sea, a, b);
                    }
                    mismatches++;
                }
            }
        }
    }
    logf("world_site: %d compared (%d usable), %d mismatches", compared, usable, mismatches);
    return mismatches;
}

static int test_good_sensor() {
    int compared = 0, mismatches = 0, good = 0;
    int w = *MapLongitudeBounds, h = *MapLatitudeBounds;
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        for (int y = 0; y < h; y++) {
            for (int x = y & 1; x < w; x += 2) {
                BOOL b = good_sensor_OG(f, x, y);
                BOOL a = good_sensor(f, x, y);
                compared++;
                good += (b != 0);
                if ((a != 0) != (b != 0)) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  good_sensor(%u, %d, %d): new=%d orig=%d", f, x, y, a, b);
                    }
                    mismatches++;
                }
            }
        }
    }
    logf("good_sensor: %d compared (%d good), %d mismatches", compared, good, mismatches);
    return mismatches;
}

static int test_spot_loc() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0;
    char label[64];
    int w = *MapLongitudeBounds, h = *MapLatitudeBounds;
    for (int y = 0; y < h; y += 5) {
        for (int x = y & 1; x < w; x += 6) {
            for (uint32_t f = 0; f < MaxPlayerNum; f++) {
                snap_restore(&base);
                spot_loc_OG(x, y, f);
                snap_take(&after_orig);
                snap_restore(&base);
                spot_loc(x, y, f);
                snap_take(&after_new);
                compared++;
                sprintf_s(label, "spot_loc(%d, %d, %u)", x, y, f);
                BOOL diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
                if (!diff && after_orig.draw_state != after_new.draw_state) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  %s: draw state differs (orig %X new %X)", label, after_orig.draw_state,
                            after_new.draw_state);
                    }
                    diff = true;
                }
                if (diff) {
                    mismatches++;
                }
            }
        }
    }
    snap_restore(&base);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("spot_loc: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

static int test_territory() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int friction_saved = *DiploFriction;
    int compared = 0, mismatches = 0, intrusions = 0;
    char label[80];
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        for (uint32_t tgt = 1; tgt < MaxPlayerNum; tgt++) {
            if (f == tgt) {
                continue;
            }
            for (int flags = 0; flags < 4; flags++) {
                for (int variant = 0; variant < 3; variant++) {
                    // as loaded; no pact/vendetta/unk_200 between the two; high friction with
                    // some units already flagged
                    auto prepare = [&]() {
                        snap_restore(&base);
                        *DiploFriction = friction_saved;
                        if (variant) {
                            PlayersData[f].diplo_treaties[tgt] &= ~(DTREATY_PACT | DTREATY_VENDETTA
                                | DTREATY_UNK_200);
                        }
                        if (variant == 2) {
                            *DiploFriction = 15;
                            for (int v = 0; v < *VehCurrentCount; v += 2) {
                                Vehs[v].flags |= VFLAG_UNK_1;
                                if (v % 4 == 0) {
                                    Vehs[v].state |= VSTATE_UNK_800;
                                }
                            }
                        }
                    };
                    int b1_o = -5, c_o = -5, b1_n = -5, c_n = -5;
                    prepare();
                    int r_o = territory_OG(f, tgt, flags, &b1_o, &c_o);
                    snap_take(&after_orig);
                    prepare();
                    int r_n = territory(f, tgt, flags, &b1_n, &c_n);
                    snap_take(&after_new);
                    compared++;
                    intrusions += r_o;
                    sprintf_s(label, "territory(%u, %u, %d) variant %d", f, tgt, flags, variant);
                    BOOL diff = false;
                    if (r_o != r_n || b1_o != b1_n || c_o != c_n) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  %s: orig %d (base %d count %d) new %d (base %d count %d)", label, r_o,
                                b1_o, c_o, r_n, b1_n, c_n);
                        }
                        diff = true;
                    }
                    if (!diff) {
                        diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
                    }
                    if (diff) {
                        mismatches++;
                    }
                }
            }
        }
    }
    snap_restore(&base);
    *DiploFriction = friction_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("territory: %d compared (%d intrusions counted by the original), %d mismatches", compared,
        intrusions, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 9: diplomacy helpers

static int test_suggest_plan() {
    int second_saved = *DiploSecondFaction, dist_saved = *BaseFindDist;
    int compared = 0, mismatches = 0, found = 0;
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        int x_saved = PlayersData[f].x_target, y_saved = PlayersData[f].y_target;
        int atk_saved = PlayersData[f].base_id_atk_target;
        for (uint32_t f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f == f2) {
                continue;
            }
            for (int variant = 0; variant < 4; variant++) {
                // as loaded; no target; target on the last base; standing attack target on base 3
                PlayersData[f].x_target = x_saved;
                PlayersData[f].y_target = y_saved;
                PlayersData[f].base_id_atk_target = atk_saved;
                if (variant == 1) {
                    PlayersData[f].x_target = -1;
                } else if (variant == 2) {
                    PlayersData[f].x_target = Bases[*BaseCurrentCount - 1].x;
                    PlayersData[f].y_target = Bases[*BaseCurrentCount - 1].y;
                } else if (variant == 3) {
                    PlayersData[f].base_id_atk_target = 3;
                }
                *DiploSecondFaction = -9; *BaseFindDist = -9;
                int b = suggest_plan_OG(f, f2);
                int s_o = *DiploSecondFaction, d_o = *BaseFindDist;
                *DiploSecondFaction = -9; *BaseFindDist = -9;
                int a = suggest_plan(f, f2);
                int s_n = *DiploSecondFaction, d_n = *BaseFindDist;
                compared++;
                found += (b >= 0);
                if (a != b || s_o != s_n || d_o != d_n) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  suggest_plan(%u, %u) variant %d: orig %d (second %d dist %d) new %d (second %d dist %d)",
                            f, f2, variant, b, s_o, d_o, a, s_n, d_n);
                    }
                    mismatches++;
                }
            }
        }
        PlayersData[f].x_target = x_saved;
        PlayersData[f].y_target = y_saved;
        PlayersData[f].base_id_atk_target = atk_saved;
    }
    *DiploSecondFaction = second_saved;
    *BaseFindDist = dist_saved;
    logf("suggest_plan: %d compared (%d targets found), %d mismatches", compared, found, mismatches);
    return mismatches;
}

static int test_scan_prototypes() {
    int best_saved = *DiploProtoIDBest, trade_saved = *DiploProtoIDTradeable;
    int compared = 0, mismatches = 0, found = 0;
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        for (uint32_t other = 1; other < MaxPlayerNum; other++) {
            if (f == other) {
                continue;
            }
            uint32_t treaties_saved = PlayersData[f].diplo_treaties[other];
            for (int variant = 0; variant < 3; variant++) {
                // as loaded; pact; vendetta
                PlayersData[f].diplo_treaties[other] = variant == 1
                    ? ((treaties_saved | DTREATY_PACT) & ~DTREATY_VENDETTA)
                    : variant == 2 ? ((treaties_saved | DTREATY_VENDETTA) & ~DTREATY_PACT)
                    : treaties_saved;
                *DiploProtoIDBest = -9; *DiploProtoIDTradeable = -9;
                scan_prototypes_OG(f, other);
                int b_o = *DiploProtoIDBest, t_o = *DiploProtoIDTradeable;
                *DiploProtoIDBest = -9; *DiploProtoIDTradeable = -9;
                scan_prototypes(f, other);
                int b_n = *DiploProtoIDBest, t_n = *DiploProtoIDTradeable;
                compared++;
                found += (b_o >= 0);
                if (b_o != b_n || t_o != t_n) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  scan_prototypes(%u, %u) variant %d: orig best %d trade %d, new best %d trade %d",
                            f, other, variant, b_o, t_o, b_n, t_n);
                    }
                    mismatches++;
                }
            }
            PlayersData[f].diplo_treaties[other] = treaties_saved;
        }
    }
    *DiploProtoIDBest = best_saved;
    *DiploProtoIDTradeable = trade_saved;
    logf("scan_prototypes: %d compared (%d with a wanted design), %d mismatches", compared, found,
        mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 10

static int test_set_course() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0, ordered = 0;
    char label[80];
    int w = *MapLongitudeBounds, h = *MapLatitudeBounds;
    for (int veh_id = 0; veh_id < base.veh_count; veh_id += 4) {
        // the unit's own tile, three fixed tiles, and a tile near the unit
        int targets[5][2] = { { base.vehs[veh_id].x, base.vehs[veh_id].y }, { 10, 10 }, { 61, 31 },
            { 40, 100 }, { xrange(base.vehs[veh_id].x + 6), (base.vehs[veh_id].y + 4) % h } };
        for (int k = 0; k < 5; k++) {
            int x = targets[k][0], y = targets[k][1];
            if (((x ^ y) & 1) || x < 0 || x >= w || y < 0 || y >= h) {
                continue;
            }
            uint8_t type = (uint8_t)(k % 3);
            snap_restore(&base);
            set_course_OG(veh_id, type, x, y);
            snap_take(&after_orig);
            ordered += after_orig.vehs[veh_id].order == ORDER_MOVE_TO;
            snap_restore(&base);
            set_course(veh_id, type, x, y);
            snap_take(&after_new);
            compared++;
            sprintf_s(label, "set_course(%d, %u, %d, %d)", veh_id, type, x, y);
            if (snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL)) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("set_course: %d compared (%d move orders set by the original), %d mismatches", compared, ordered,
        mismatches);
    return mismatches;
}

// tech_achieved() replaced by a five byte jump into a C++ recorder that keeps the arguments of
// every call (the callers' cdecl frames pass straight through). Used where a test triggers more
// than one call.
struct TechAchievedCall { int faction_id, tech_id, faction_id_from; };
static TechAchievedCall TechAchievedCallLog[8];
static int TechAchievedCallCount = 0;
static void __cdecl tech_achieved_recorder(int faction_id, int tech_id, int faction_id_from, int) {
    if (TechAchievedCallCount < 8) {
        TechAchievedCallLog[TechAchievedCallCount].faction_id = faction_id;
        TechAchievedCallLog[TechAchievedCallCount].tech_id = tech_id;
        TechAchievedCallLog[TechAchievedCallCount].faction_id_from = faction_id_from;
    }
    TechAchievedCallCount++;
    TechAchievedRecorded = tech_id;
}
struct TechAchievedJmpStub {
    uint8_t *entry;
    uint8_t saved_code[5];
    DWORD old_protect;
    BOOL ok;
    TechAchievedJmpStub() : entry((uint8_t *)tech_achieved), old_protect(0), ok(false) {
        uint8_t stub[5] = { 0xE9, 0, 0, 0, 0 };
        int32_t rel = (int32_t)((uint8_t *)tech_achieved_recorder - (entry + 5));
        memcpy(stub + 1, &rel, 4);
        if (!VirtualProtect(entry, sizeof(stub), PAGE_EXECUTE_READWRITE, &old_protect)) {
            logf("cannot unprotect tech_achieved (%u)", GetLastError());
            return;
        }
        memcpy(saved_code, entry, sizeof(stub));
        memcpy(entry, stub, sizeof(stub));
        FlushInstructionCache(GetCurrentProcess(), entry, sizeof(stub));
        ok = true;
    }
    ~TechAchievedJmpStub() {
        if (ok) {
            memcpy(entry, saved_code, sizeof(saved_code));
            FlushInstructionCache(GetCurrentProcess(), entry, sizeof(saved_code));
            VirtualProtect(entry, sizeof(saved_code), old_protect, &old_protect);
        }
    }
};

static int test_enemies_trade_tech() {
    TechAchievedJmpStub stub;
    if (!stub.ok) {
        return 1;
    }
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0, trades = 0;
    char label[80];
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            for (int variant = 0; variant < 2; variant++) {
                // as loaded, and as pact partners
                auto prepare = [&]() {
                    snap_restore(&base);
                    if (variant) {
                        PlayersData[f1].diplo_treaties[f2] |= DTREATY_PACT;
                        PlayersData[f2].diplo_treaties[f1] |= DTREATY_PACT;
                    }
                    TechAchievedCallCount = 0;
                    memset(TechAchievedCallLog, 0, sizeof(TechAchievedCallLog));
                    reseed(f1 * 23 + f2 * 5 + variant);
                };
                prepare();
                enemies_trade_tech_OG(f1, f2);
                snap_take(&after_orig);
                int count_o = TechAchievedCallCount;
                TechAchievedCall calls_o[8];
                memcpy(calls_o, TechAchievedCallLog, sizeof(calls_o));
                prepare();
                enemies_trade_tech(f1, f2);
                snap_take(&after_new);
                int count_n = TechAchievedCallCount;
                compared++;
                trades += count_o;
                sprintf_s(label, "enemies_trade_tech(%d, %d) variant %d", f1, f2, variant);
                BOOL diff = false;
                if (count_o != count_n || memcmp(calls_o, TechAchievedCallLog, sizeof(calls_o)) != 0) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  %s: orig %d calls (first %d/%d/%d), new %d calls (first %d/%d/%d)", label,
                            count_o, calls_o[0].faction_id, calls_o[0].tech_id, calls_o[0].faction_id_from,
                            count_n, TechAchievedCallLog[0].faction_id, TechAchievedCallLog[0].tech_id,
                            TechAchievedCallLog[0].faction_id_from);
                    }
                    diff = true;
                }
                if (!diff) {
                    diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
                }
                if (diff) {
                    mismatches++;
                }
            }
        }
    }
    snap_restore(&base);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("enemies_trade_tech: %d compared (%d techs traded by the original), %d mismatches", compared,
        trades, mismatches);
    return mismatches;
}

static int test_tech_analysis() {
    int *const globals[15] = { DiploTechProjectID, DiploTechGiveID4, DiploTechGiveID3, DiploTechGiveID2,
        DiploTechGiveID1, DiploTechGetID2, DiploTechGetID1, DiploTechGetValue2, DiploTechGiveValue2,
        DiploTechGetValue1, DiploTechGiveValue1, DiploTechGiveValue4, DiploTechGiveValue3,
        DiploTechGetProject2, DiploTechGetProject1 };
    int saved[15], orig[15], fresh[15];
    for (int i = 0; i < 15; i++) {
        saved[i] = *globals[i];
    }
    int compared = 0, mismatches = 0, with_project = 0;
    for (int f = 1; f < MaxPlayerNum; f++) {
        int diff_saved = PlayersData[f].diff_level;
        for (uint32_t other = 1; other < MaxPlayerNum; other++) {
            if ((int)other == f) {
                continue;
            }
            for (int variant = 0; variant < 5; variant++) {
                // difficulty as loaded, 1 and 5; then, at difficulty 5 and 1, a secret project made
                // unbuilt whose prerequisite the other faction has and this one lacks, queued at
                // one of the other faction's bases
                PlayersData[f].diff_level = variant == 0 ? diff_saved : variant == 1 || variant == 4 ? 1 : 5;
                int sp_patched = -1, sp_base_saved = -1, sp_queue_saved = 0, sp_base = -1;
                uint8_t tech_saved = 0;
                int tech_patched = -1;
                if (variant >= 3) {
                    for (uint32_t sp = 0; sp < MaxSecretProjectNum && sp_patched < 0; sp++) {
                        int tech = Facility[FAC_HUMAN_GENOME_PROJ + sp].preq_tech;
                        if (tech >= 0 && has_tech(tech, other)) {
                            sp_patched = sp;
                            tech_patched = tech;
                        }
                    }
                    if (sp_patched >= 0) {
                        sp_base_saved = (&SecretProject->human_genome_project)[sp_patched];
                        (&SecretProject->human_genome_project)[sp_patched] = -1;
                        tech_saved = GameTechAchieved[tech_patched];
                        GameTechAchieved[tech_patched] &= ~(uint8_t)(1 << f);
                        for (int i = 0; i < *BaseCurrentCount; i++) {
                            if (Bases[i].faction_id_current == other) {
                                sp_base = i;
                                sp_queue_saved = Bases[i].queue_production_id[0];
                                Bases[i].queue_production_id[0] = -(int)(FAC_HUMAN_GENOME_PROJ + sp_patched);
                                break;
                            }
                        }
                    }
                }
                for (int i = 0; i < 15; i++) {
                    *globals[i] = -77;
                }
                tech_analysis_OG(f, other);
                for (int i = 0; i < 15; i++) {
                    orig[i] = *globals[i];
                    *globals[i] = -77;
                }
                tech_analysis(f, other);
                for (int i = 0; i < 15; i++) {
                    fresh[i] = *globals[i];
                }
                if (sp_patched >= 0) {
                    (&SecretProject->human_genome_project)[sp_patched] = sp_base_saved;
                    GameTechAchieved[tech_patched] = tech_saved;
                    if (sp_base >= 0) {
                        Bases[sp_base].queue_production_id[0] = sp_queue_saved;
                    }
                }
                compared++;
                with_project += orig[0] != -1;
                if (memcmp(orig, fresh, sizeof(orig)) != 0) {
                    if (mismatches < MAX_DETAIL) {
                        int i = 0;
                        while (i < 15 && orig[i] == fresh[i]) i++;
                        logf("  tech_analysis(%d, %u) variant %d: global %d differs (orig %d new %d)", f, other,
                            variant, i, orig[i], fresh[i]);
                    }
                    mismatches++;
                }
            }
        }
        PlayersData[f].diff_level = diff_saved;
    }
    for (int i = 0; i < 15; i++) {
        *globals[i] = saved[i];
    }
    logf("tech_analysis: %d compared (%d with a project found), %d mismatches", compared, with_project,
        mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 11

static int test_rankings() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    uint32_t unk_saved[8], rank_saved[8];
    memcpy(unk_saved, FactionRankingsUnk, sizeof(unk_saved));
    memcpy(rank_saved, FactionRankings, sizeof(rank_saved));
    uint32_t id1_saved = *RankingFactionIDUnk1, id2_saved = *RankingFactionIDUnk2;
    uint32_t rules_saved = *GameRules;
    int16_t might_saved[8];
    int turn = *TurnCurrentNum;
    memcpy(might_saved, FactionTurnMight + turn * 8, sizeof(might_saved));
    int compared = 0, mismatches = 0, betrayals = 0;
    char label[64];
    for (int full = 0; full < 2; full++) {
        for (int variant = 0; variant < 3; variant++) {
            // rules as loaded, intense rivalry, and sudden death objectives with no achieve points
            uint32_t req_saved = *ObjectiveReqVictory, sudden_saved = *ObjectivesSuddenDeathVictory;
            uint32_t pts_saved = *ObjectiveAchievePts;
            auto prepare = [&]() {
                snap_restore(&base);
                memcpy(FactionRankingsUnk, unk_saved, sizeof(unk_saved));
                memcpy(FactionRankings, rank_saved, sizeof(rank_saved));
                *RankingFactionIDUnk1 = id1_saved;
                *RankingFactionIDUnk2 = id2_saved;
                memcpy(FactionTurnMight + turn * 8, might_saved, sizeof(might_saved));
                *GameRules = variant == 1 ? (rules_saved | RULES_INTENSE_RIVALRY) : rules_saved;
                *ObjectiveReqVictory = req_saved;
                *ObjectivesSuddenDeathVictory = variant == 2 ? 50 : sudden_saved;
                *ObjectiveAchievePts = variant == 2 ? 0 : pts_saved;
                reseed(full * 7 + variant);
            };
            uint32_t unk_o[8], rank_o[8], unk_n[8], rank_n[8];
            int16_t might_o[8], might_n[8];
            uint32_t ids_o[2], ids_n[2];
            prepare();
            rankings_OG(full);
            snap_take(&after_orig);
            memcpy(unk_o, FactionRankingsUnk, 32); memcpy(rank_o, FactionRankings, 32);
            memcpy(might_o, FactionTurnMight + turn * 8, 16);
            ids_o[0] = *RankingFactionIDUnk1; ids_o[1] = *RankingFactionIDUnk2;
            betrayals += first_diff(after_orig.players, base.players, sizeof(base.players)) != (size_t)-1;
            prepare();
            rankings(full);
            snap_take(&after_new);
            memcpy(unk_n, FactionRankingsUnk, 32); memcpy(rank_n, FactionRankings, 32);
            memcpy(might_n, FactionTurnMight + turn * 8, 16);
            ids_n[0] = *RankingFactionIDUnk1; ids_n[1] = *RankingFactionIDUnk2;
            compared++;
            sprintf_s(label, "rankings(%d) variant %d", full, variant);
            BOOL diff = false;
            if (memcmp(unk_o, unk_n, 32) || memcmp(rank_o, rank_n, 32) || memcmp(might_o, might_n, 16)
                || memcmp(ids_o, ids_n, 8)) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: scores/rankings differ (orig score[1] %u rank[7] %u ids %u/%u; new %u %u %u/%u)",
                        label, unk_o[1], rank_o[7], ids_o[0], ids_o[1], unk_n[1], rank_n[7], ids_n[0], ids_n[1]);
                }
                diff = true;
            }
            if (!diff) {
                diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
            }
            if (diff) {
                mismatches++;
            }
            *ObjectiveReqVictory = req_saved;
            *ObjectivesSuddenDeathVictory = sudden_saved;
            *ObjectiveAchievePts = pts_saved;
        }
    }
    snap_restore(&base);
    memcpy(FactionRankingsUnk, unk_saved, sizeof(unk_saved));
    memcpy(FactionRankings, rank_saved, sizeof(rank_saved));
    *RankingFactionIDUnk1 = id1_saved;
    *RankingFactionIDUnk2 = id2_saved;
    *GameRules = rules_saved;
    memcpy(FactionTurnMight + turn * 8, might_saved, sizeof(might_saved));
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("rankings: %d compared (%d changed player data), %d mismatches", compared, betrayals, mismatches);
    return mismatches;
}

static int test_stack_veh() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0, transports = 0;
    char label[64];
    for (int veh_id = 0; veh_id < base.veh_count; veh_id++) {
        BOOL transport = veh_cargo(veh_id) != 0 || has_abil(base.vehs[veh_id].proto_id, ABL_CARRIER);
        // every transport, every 9th other unit
        if (!transport && veh_id % 9) {
            continue;
        }
        transports += transport;
        for (int mode = 0; mode < 3; mode++) {
            for (int variant = 0; variant < 2; variant++) {
                // as loaded; the stack mates sentried waiting for a carrier
                auto prepare = [&]() {
                    snap_restore(&base);
                    if (variant) {
                        int i = veh_id;
                        while (Vehs[i].prev_veh_id_stack >= 0) {
                            i = Vehs[i].prev_veh_id_stack;
                        }
                        for (; i >= 0; i = Vehs[i].next_veh_id_stack) {
                            if (i != veh_id) {
                                Vehs[i].order = ORDER_SENTRY_BOARD;
                                Vehs[i].waypoint_x[0] = (i % 2) ? -1 : (int16_t)veh_id;
                            }
                        }
                    }
                };
                prepare();
                uint32_t r_o = stack_veh_OG(veh_id, mode);
                snap_take(&after_orig);
                prepare();
                uint32_t r_n = stack_veh(veh_id, mode);
                snap_take(&after_new);
                compared++;
                sprintf_s(label, "stack_veh(%d, %d) variant %d", veh_id, mode, variant);
                BOOL diff = false;
                if (r_o != r_n) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  %s: return orig %u new %u", label, r_o, r_n);
                    }
                    diff = true;
                }
                if (!diff) {
                    diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
                }
                if (diff) {
                    mismatches++;
                }
            }
        }
    }
    snap_restore(&base);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("stack_veh: %d compared (%d transports), %d mismatches", compared, transports, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 12

static int test_reset_territory() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int diff_saved = *DiffLevelCurrent;
    uint32_t dist_saved = Rules->territory_max_dist_base;
    int compared = 0, mismatches = 0, pods = 0, flagged = 0;
    char label[64];
    for (int variant = 0; variant < 6; variant++) {
        // as loaded; every tile unclaimed and unflagged; every site rating unknown; pods on every
        // fourth tile (hard AI); the same on easy with a revealed map and the objectives flag;
        // a doubled territory distance
        auto prepare = [&]() {
            snap_restore(&base);
            *DiffLevelCurrent = diff_saved;
            Rules->territory_max_dist_base = dist_saved;
            reseed(variant);
            for (uint32_t i = 0; i < base.tile_count; i++) {
                Map *tile = &(*MapTiles)[i];
                if (variant == 1) {
                    tile->territory = -1;
                    tile->bit2 &= ~BIT2_UNK_400000;
                } else if (variant == 2) {
                    tile->val2 &= 0x0F;
                } else if ((variant == 3 || variant == 4) && i % 4 == 0) {
                    tile->bit &= ~BIT_SUPPLY_REMOVE;
                }
            }
            if (variant == 1) {
                *UnkBitfield1 &= ~1;
            } else if (variant == 3) {
                *DiffLevelCurrent = DLVL_TRANSCEND;
            } else if (variant == 4) {
                *DiffLevelCurrent = DLVL_CITIZEN;
                PlayersData[1].flags |= PFLAG_MAP_REVEALED;
                PlayersData[2].flags |= PFLAG_STRAT_SEARCH_OBJECTIVES;
            } else if (variant == 5) {
                Rules->territory_max_dist_base = dist_saved * 2;
            }
        };
        prepare();
        reset_territory_OG();
        snap_take(&after_orig);
        for (int r = 0; r < MaxContinentNum; r++) {
            pods += after_orig.continents[r].pods;
        }
        flagged += (after_orig.draw_state & 1) != 0;
        prepare();
        reset_territory();
        snap_take(&after_new);
        compared++;
        sprintf_s(label, "reset_territory variant %d", variant);
        if (snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL)) {
            mismatches++;
        }
    }
    snap_restore(&base);
    *DiffLevelCurrent = diff_saved;
    Rules->territory_max_dist_base = dist_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("reset_territory: %d compared (%d pods counted, %d redraws flagged by the original), %d mismatches",
        compared, pods, flagged, mismatches);
    return mismatches;
}

static int test_base_research() {
    TechAchievedStub stub;
    if (!stub.ok) {
        return 1;
    }
    TechCompare tc;
    tc.init();
    int turn_saved = *TurnCurrentNum;
    uint32_t rules_saved = *GameRules;
    int compared = 0, mismatches = 0, awarded = 0;
    char label[80];
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        uint32_t f = Bases[base_id].faction_id_current;
        if (!quiet_faction(f)) {
            continue;
        }
        for (int variant = 0; variant < 6; variant++) {
            // as loaded; enough labs for an advance; a research penalty still running; the first
            // turns on an easy level; a punishment sphere, top rating and a big carried remainder;
            // a scenario without research
            auto patch = [&]() {
                *TurnCurrentNum = turn_saved;
                *GameRules = rules_saved;
                set_base(base_id);
                PlayerData *p = &PlayersData[f];
                if (variant == 1) {
                    Bases[base_id].labs_total = p->tech_cost + 7;
                } else if (variant == 2) {
                    p->soc_effect_pending.research = -5;
                    *TurnCurrentNum = 20;
                } else if (variant == 3) {
                    *TurnCurrentNum = 3;
                    p->diff_level = DLVL_SPECIALIST;
                    Bases[base_id].labs_total += 11;
                } else if (variant == 4) {
                    uint32_t offset, mask;
                    bitmask(FAC_PUNISHMENT_SPHERE, &offset, &mask);
                    Bases[base_id].facilities_built[offset] |= mask;
                    Bases[base_id].labs_total += 13;
                    p->soc_effect_pending.research = 7;
                    p->net_random_event = 95;
                } else if (variant == 5) {
                    *GameRules |= RULES_SCN_NO_TECH_ADVANCES;
                    Bases[base_id].labs_total = p->tech_cost + 7;
                }
            };
            tc.prepare(base_id * 7 + variant); patch();
            base_research_OG();
            tc.take_orig();
            awarded += (tc.tech_orig >= 0);
            tc.prepare(base_id * 7 + variant); patch();
            base_research();
            tc.take_new();
            compared++;
            sprintf_s(label, "base_research(%d) variant %d", base_id, variant);
            if (tc.differs(label, mismatches < MAX_DETAIL)) {
                mismatches++;
            }
        }
    }
    *TurnCurrentNum = turn_saved;
    *GameRules = rules_saved;
    tc.done();
    logf("base_research: %d compared (%d awards by the original), %d mismatches", compared, awarded,
        mismatches);
    return mismatches;
}

static int test_base_check_support() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int selected_saved = *BaseIDCurrentSelected;
    Base *current_saved = *BaseCurrent;
    uint32_t maint_saved = *BaseCurrentForcesMaintCost;
    int compared = 0, mismatches = 0, disbanded = 0, convoys = 0;
    char label[80];
    for (int base_id = 0; base_id < base.base_count; base_id++) {
        uint32_t f = base.bases[base_id].faction_id_current;
        for (int variant = 0; variant < 7; variant++) {
            // a shortfall of 1, 4 and 12 minerals; a mineral convoy parked in the base with a
            // shortfall of 3; a riot under weak police (all units pacifism drones), the same with
            // a human governor and a warlike AI, the same with a rioting nutrient deficit
            BOOL shortfall = variant <= 3;
            if (shortfall && !quiet_faction(f)) {
                continue; // a human player gets a popup
            }
            int convoy_id = -1;
            auto prepare = [&]() {
                snap_restore(&base);
                reseed(base_id * 11 + variant);
                set_base(base_id);
                Base *b = &Bases[base_id];
                int extra[4] = { 1, 4, 12, 3 };
                *BaseCurrentForcesMaintCost = b->mineral_intake_2 + (shortfall ? extra[variant] : 0);
                if (variant == 3) {
                    for (int v = 0; v < *VehCurrentCount; v++) {
                        if (Vehs[v].home_base_id == base_id
                            && VehPrototypes[Vehs[v].proto_id].plan == PLAN_SUPPLY_CONVOY) {
                            Vehs[v].order = ORDER_CONVOY;
                            Vehs[v].order_auto_type = RSC_MINERALS;
                            Vehs[v].x = b->x;
                            Vehs[v].y = b->y;
                            convoy_id = v;
                            break;
                        }
                    }
                }
                if (!shortfall) {
                    b->state |= BSTATE_DRONE_RIOTS_ACTIVE;
                    b->drone_total = b->talent_total + 2;
                    PlayersData[f].soc_effect_pending.police = -3;
                    if (variant >= 5) {
                        b->governor |= GOV_MANAGES_CITIZENS_SPECS | GOV_ACTIVE;
                        PlayersData[f].ai_fight = variant == 5 ? 1 : -1;
                    }
                    if (variant == 6) {
                        b->drone_total = b->talent_total;
                        b->nutrient_surplus = -2;
                    }
                    for (int v = 0; v < *VehCurrentCount; v++) {
                        if (Vehs[v].home_base_id == base_id) {
                            Vehs[v].state |= VSTATE_PACIFISM_DRONE;
                            if (v % 2) {
                                Vehs[v].state |= VSTATE_UNK_200;
                            }
                        }
                    }
                }
            };
            prepare();
            base_check_support_OG();
            snap_take(&after_orig);
            uint32_t maint_o = *BaseCurrentForcesMaintCost;
            disbanded += base.veh_count_global - *VehCurrentCount;
            convoys += convoy_id >= 0 && after_orig.vehs[convoy_id].order == ORDER_NONE;
            prepare();
            base_check_support();
            snap_take(&after_new);
            uint32_t maint_n = *BaseCurrentForcesMaintCost;
            compared++;
            sprintf_s(label, "base_check_support(%d) variant %d", base_id, variant);
            BOOL diff = false;
            if (maint_o != maint_n) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: maintenance orig %u new %u", label, maint_o, maint_n);
                }
                diff = true;
            }
            if (!diff) {
                diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
            }
            if (diff) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    *BaseIDCurrentSelected = selected_saved;
    *BaseCurrent = current_saved;
    *BaseCurrentForcesMaintCost = maint_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("base_check_support: %d compared (%d units disbanded, %d convoys recalled by the original), %d mismatches",
        compared, disbanded, convoys, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 13

// base_support() and farm_compute() write the current-base globals between 0x90E8F0 and 0x90EA60
static uint8_t *const BaseGlobals = (uint8_t *)0x0090E8F0;
static const size_t BaseGlobalsSize = 0x90EA60 - 0x90E8F0;

static BOOL base_globals_differ(const uint8_t *a, const uint8_t *b, const char *label) {
    size_t d = first_diff(a, b, BaseGlobalsSize);
    if (d == (size_t)-1) {
        return false;
    }
    if (label) {
        logf("  %s: base global at %08X differs (orig %02X new %02X)", label,
            (unsigned)(0x90E8F0 + d), a[d], b[d]);
    }
    return true;
}

static int test_base_support() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    uint8_t globals_saved[BaseGlobalsSize], globals_o[BaseGlobalsSize], globals_n[BaseGlobalsSize];
    memcpy(globals_saved, BaseGlobals, BaseGlobalsSize);
    int selected_saved = *BaseIDCurrentSelected;
    int compared = 0, mismatches = 0, supported = 0, drones = 0, convoys = 0;
    char label[80];
    for (int base_id = 0; base_id < base.base_count; base_id++) {
        uint32_t f = base.bases[base_id].faction_id_current;
        for (int variant = 0; variant < 7; variant++) {
            // as loaded; the AI upkeep pass; SUPPORT -4 under police -3; SUPPORT +3 under police
            // -5; SUPPORT -2, police -3 and a brood pit; every convoy of the base working (some
            // parked here, one other base's convoy too); SUPPORT +3 at size 7 under police -3
            auto prepare = [&]() {
                snap_restore(&base);
                memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
                set_base(base_id);
                Base *b = &Bases[base_id];
                PlayerData *p = &PlayersData[f];
                *BaseUpkeepStage = (variant >= 1 && variant <= 3) ? 1 : 0;
                if (variant == 2) {
                    p->soc_effect_pending.support = -4;
                    p->soc_effect_pending.police = -3;
                } else if (variant == 3) {
                    p->soc_effect_pending.support = 3;
                    p->soc_effect_pending.police = -5;
                } else if (variant == 4) {
                    p->soc_effect_pending.support = -2;
                    p->soc_effect_pending.police = -3;
                    uint32_t offset, mask;
                    bitmask(FAC_BROOD_PIT, &offset, &mask);
                    b->facilities_built[offset] |= mask;
                } else if (variant == 5) {
                    int n = 0;
                    for (int v = 0; v < *VehCurrentCount; v++) {
                        if (Vehs[v].faction_id != f
                            || VehPrototypes[Vehs[v].proto_id].plan != PLAN_SUPPLY_CONVOY) {
                            continue;
                        }
                        if (Vehs[v].home_base_id == base_id) {
                            Vehs[v].order = ORDER_CONVOY;
                            Vehs[v].order_auto_type = (uint8_t)(n % 4);
                            if (n % 3 == 2) {
                                Vehs[v].x = b->x;
                                Vehs[v].y = b->y;
                            }
                            n++;
                        } else if (n < 100 && Vehs[v].home_base_id >= 0) {
                            Vehs[v].order = ORDER_CONVOY;
                            Vehs[v].order_auto_type = RSC_ENERGY;
                            Vehs[v].x = b->x;
                            Vehs[v].y = b->y;
                            n += 100;
                        }
                    }
                } else if (variant == 6) {
                    p->soc_effect_pending.support = 3;
                    p->soc_effect_pending.police = -3;
                    b->population_size = 7;
                }
            };
            prepare();
            base_support_OG();
            snap_take(&after_orig);
            memcpy(globals_o, BaseGlobals, BaseGlobalsSize);
            supported += *BaseCurrentForcesSupported;
            drones += *BaseCurrentVehPacifismCount;
            convoys += BaseCurrentConvoyTo[0] + BaseCurrentConvoyTo[1] + BaseCurrentConvoyTo[2]
                + BaseCurrentConvoyFrom[0] + BaseCurrentConvoyFrom[1] + BaseCurrentConvoyFrom[2];
            prepare();
            base_support();
            snap_take(&after_new);
            memcpy(globals_n, BaseGlobals, BaseGlobalsSize);
            compared++;
            sprintf_s(label, "base_support(%d) variant %d", base_id, variant);
            BOOL detail = mismatches < MAX_DETAIL;
            if (base_globals_differ(globals_o, globals_n, detail ? label : NULL)
                || snap_differs(&after_orig, &after_new, detail ? label : NULL)) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
    *BaseIDCurrentSelected = selected_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("base_support: %d compared (%d units supported, %d pacifism drones, %d convoy yield by the original), %d mismatches",
        compared, supported, drones, convoys, mismatches);
    return mismatches;
}

static int test_farm_compute() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    uint8_t globals_saved[BaseGlobalsSize], globals_o[BaseGlobalsSize], globals_n[BaseGlobalsSize];
    memcpy(globals_saved, BaseGlobals, BaseGlobalsSize);
    int selected_saved = *BaseIDCurrentSelected;
    int compared = 0, mismatches = 0, flags_seen[6] = {};
    char label[80];
    for (int base_id = 0; base_id < base.base_count; base_id++) {
        uint32_t f = base.bases[base_id].faction_id_current;
        for (int variant = 0; variant < 5; variant++) {
            // as loaded; the AI upkeep pass with pods back on the radius; half the radius unseen;
            // the map revealed and no treaties; every nearby base working its whole radius
            auto prepare = [&]() {
                snap_restore(&base);
                memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
                set_base(base_id);
                Base *b = &Bases[base_id];
                *BaseUpkeepStage = variant == 1 ? 1 : 0;
                for (int i = 0; i < 21; i++) {
                    int x = b->x + RadiusOffsetX[i], y = b->y + RadiusOffsetY[i];
                    if (!*MapIsFlat) {
                        x = x < 0 ? x + *MapLongitudeBounds
                            : (x >= (int)*MapLongitudeBounds ? x - *MapLongitudeBounds : x);
                    }
                    if (y < 0 || y >= (int)*MapLatitudeBounds || x < 0 || x >= (int)*MapLongitudeBounds) {
                        continue;
                    }
                    Map *tile = map_loc(x, y);
                    if (variant == 1) {
                        tile->bit &= ~BIT_SUPPLY_REMOVE;
                    } else if (variant == 2 && i % 2) {
                        tile->visibility &= ~(1 << f);
                    }
                }
                if (variant == 2) {
                    PlayersData[f].flags &= ~PFLAG_MAP_REVEALED;
                } else if (variant == 3) {
                    PlayersData[f].flags |= PFLAG_MAP_REVEALED;
                    for (int g = 0; g < MaxPlayerNum; g++) {
                        PlayersData[f].diplo_treaties[g] &= ~(DTREATY_PACT | DTREATY_TREATY);
                    }
                } else if (variant == 4) {
                    for (int i = 0; i < *BaseCurrentCount; i++) {
                        Bases[i].resource_sqr_radius = 0x1FFFFF;
                    }
                }
            };
            prepare();
            farm_compute_OG();
            snap_take(&after_orig);
            memcpy(globals_o, BaseGlobals, BaseGlobalsSize);
            for (int i = 0; i < 21; i++) {
                for (int k = 0; k < 6; k++) {
                    flags_seen[k] += (BaseTileFlags[i] >> k) & 1;
                }
            }
            prepare();
            farm_compute();
            snap_take(&after_new);
            memcpy(globals_n, BaseGlobals, BaseGlobalsSize);
            compared++;
            sprintf_s(label, "farm_compute(%d) variant %d", base_id, variant);
            BOOL detail = mismatches < MAX_DETAIL;
            if (base_globals_differ(globals_o, globals_n, detail ? label : NULL)
                || snap_differs(&after_orig, &after_new, detail ? label : NULL)) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
    *BaseIDCurrentSelected = selected_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("farm_compute: %d compared (tile flags 1/2/4/8/10/20 set %d/%d/%d/%d/%d/%d times by the original), %d mismatches",
        compared, flags_seen[0], flags_seen[1], flags_seen[2], flags_seen[3], flags_seen[4],
        flags_seen[5], mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 14

static int *const SecretProjectTable = (int *)0x009A6514;
static int *const ProposalPassTable = (int *)0x009A6618; // [?] only entries 3 and 4 are touched

static void set_fac(int base_id, int facility_id) {
    uint32_t offset, mask;
    bitmask(facility_id, &offset, &mask);
    Bases[base_id].facilities_built[offset] |= mask;
}

static int test_base_yield() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    uint8_t globals_saved[BaseGlobalsSize], globals_o[BaseGlobalsSize], globals_n[BaseGlobalsSize];
    memcpy(globals_saved, BaseGlobals, BaseGlobalsSize);
    int selected_saved = *BaseIDCurrentSelected;
    int control_saved = *ControlUpkeepA;
    int turn_saved = *TurnCurrentNum;
    int compared = 0, mismatches = 0, specialists = 0, swaps_or_moves = 0;
    char label[80];
    for (int base_id = 0; base_id < base.base_count; base_id++) {
        uint32_t f = base.bases[base_id].faction_id_current;
        BOOL human = is_human(f);
        for (int variant = 0; variant < 7; variant++) {
            // as loaded; governed with the first pass skipped; turn 150 at size +3; nothing
            // worked; five mixed specialists at size +2; conquer priority building a project;
            // starving at size +6 with orbital yields and an aerospace complex
            auto prepare = [&]() {
                snap_restore(&base);
                memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
                *ControlUpkeepA = variant == 1 ? 1 : 0;
                *TurnCurrentNum = (variant == 2 || variant == 4 || variant == 5) ? 150 : turn_saved;
                set_base(base_id);
                Base *b = &Bases[base_id];
                PlayerData *p = &PlayersData[f];
                if (human && (variant == 1 || variant == 4 || variant == 5)) {
                    b->governor |= GOV_MANAGES_CITIZENS_SPECS | GOV_ACTIVE;
                }
                if (variant == 2) {
                    b->population_size += 3;
                } else if (variant == 3) {
                    b->resource_sqr_radius = 1;
                    b->specialist_total = 0;
                } else if (variant == 4) {
                    b->population_size += 2;
                    b->specialist_total = 5;
                    b->spec[0] = 0x31413141;
                } else if (variant == 5) {
                    if (human) {
                        b->governor |= GOV_PRIORITY_CONQUER;
                    } else {
                        p->ai_power = 1;
                        p->ai_fight = 1;
                        p->ai_wealth = 0;
                    }
                    b->queue_production_id[0] = -75;
                } else if (variant == 6) {
                    b->population_size += 6;
                    b->resource_sqr_radius = 1;
                    p->satellites_nutrient = 4;
                    p->satellites_mineral = 3;
                    p->satellites_energy = 2;
                    set_fac(base_id, FAC_AEROSPACE_COMPLEX);
                }
            };
            prepare();
            uint32_t worked_before = Bases[base_id].resource_sqr_radius;
            base_yield_OG();
            snap_take(&after_orig);
            memcpy(globals_o, BaseGlobals, BaseGlobalsSize);
            specialists += Bases[base_id].specialist_total;
            swaps_or_moves += Bases[base_id].resource_sqr_radius != worked_before;
            prepare();
            base_yield();
            snap_take(&after_new);
            memcpy(globals_n, BaseGlobals, BaseGlobalsSize);
            compared++;
            sprintf_s(label, "base_yield(%d) variant %d", base_id, variant);
            BOOL detail = mismatches < MAX_DETAIL;
            if (base_globals_differ(globals_o, globals_n, detail ? label : NULL)
                || snap_differs(&after_orig, &after_new, detail ? label : NULL)) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
    *BaseIDCurrentSelected = selected_saved;
    *ControlUpkeepA = control_saved;
    *TurnCurrentNum = turn_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("base_yield: %d compared (%d specialists, %d worked-tile changes by the original), %d mismatches",
        compared, specialists, swaps_or_moves, mismatches);
    return mismatches;
}

// The patcher replaces a redirected function's first six bytes with jmp [IAT]; this puts the
// original bytes back while it lives, so the exe's own code runs again.
struct OriginalEntry {
    uint8_t *entry;
    uint8_t saved[6];
    DWORD old_protect;
    BOOL ok;
    OriginalEntry(uint32_t address, const uint8_t (&original)[6])
        : entry((uint8_t *)address), old_protect(0), ok(false) {
        if (!VirtualProtect(entry, 6, PAGE_EXECUTE_READWRITE, &old_protect)) {
            logf("cannot unprotect %08X (%u)", address, GetLastError());
            return;
        }
        memcpy(saved, entry, 6);
        memcpy(entry, original, 6);
        FlushInstructionCache(GetCurrentProcess(), entry, 6);
        ok = true;
    }
    ~OriginalEntry() {
        if (ok) {
            memcpy(entry, saved, 6);
            FlushInstructionCache(GetCurrentProcess(), entry, 6);
            VirtualProtect(entry, 6, old_protect, &old_protect);
        }
    }
};

// Run two functions back to back at the same stack depth, as the game's base_support ..
// base_energy chain does, so the second sees what the first left in the stack slots it doesn't
// initialize.
static void __declspec(noinline) call_pair(func_void *first, func_void *second) {
    __asm {
        call first
        call second
    }
}

static int test_base_energy() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    uint8_t globals_saved[BaseGlobalsSize], globals_o[BaseGlobalsSize], globals_n[BaseGlobalsSize];
    memcpy(globals_saved, BaseGlobals, BaseGlobalsSize);
    int sp_saved[MaxSecretProjectNum];
    memcpy(sp_saved, SecretProjectTable, sizeof(sp_saved));
    int pass_saved[2] = { ProposalPassTable[PROP_GLOBAL_TRADE_PACT], ProposalPassTable[PROP_REPEAL_GLOBAL_TRADE_PACT] };
    int governor_saved = *GovernorFactionID;
    int selected_saved = *BaseIDCurrentSelected;
    // run the exe's own base_minerals and base_energy, not the jumps into the DLL
    static const uint8_t minerals_entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10 };
    static const uint8_t energy_entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x28 };
    OriginalEntry vanilla_minerals(0x004E9CB0, minerals_entry);
    OriginalEntry vanilla_energy(0x004EB560, energy_entry);
    if (!vanilla_minerals.ok || !vanilla_energy.ok) {
        return 1;
    }
    int compared = 0, mismatches = 0, commerce = 0, grids = 0;
    char label[80];
    for (int base_id = 0; base_id < base.base_count; base_id++) {
        uint32_t f = base.bases[base_id].faction_id_current;
        BOOL alien = (Players[f].rule_flags & RFLAG_ALIEN) != 0;
        for (int variant = 0; variant < 8; variant++) {
            // as loaded; the upkeep pass with a global trade pact and this faction governing;
            // labs-heavy allocation at efficiency -4; no allocation with four specialists of
            // types 0-3; every multiplier facility and project here; stockpile energy (energy
            // grid project owned on odd bases); sanctions with no treaties; no treaties at all.
            // Each run pairs base_minerals and base_energy at one stack depth as the game always
            // does, because base_energy starts an alien base's energy grid from what
            // base_minerals leaves on the stack.
            auto prepare = [&]() {
                snap_restore(&base);
                memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
                memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
                ProposalPassTable[PROP_GLOBAL_TRADE_PACT] = pass_saved[0];
                ProposalPassTable[PROP_REPEAL_GLOBAL_TRADE_PACT] = pass_saved[1];
                *GovernorFactionID = governor_saved;
                set_base(base_id);
                Base *b = &Bases[base_id];
                PlayerData *p = &PlayersData[f];
                *BaseUpkeepStage = variant == 1 ? 1 : 0;
                if (variant == 1) {
                    ProposalPassTable[PROP_GLOBAL_TRADE_PACT] = 1;
                    ProposalPassTable[PROP_REPEAL_GLOBAL_TRADE_PACT] = 0;
                    *GovernorFactionID = f;
                    BaseCurrentConvoyTo[RSC_ENERGY] = 3;
                    BaseCurrentConvoyFrom[RSC_ENERGY] = 1;
                } else if (variant == 2) {
                    p->se_alloc_labs = 8;
                    p->se_alloc_psych = 2;
                    p->soc_effect_pending.efficiency = -4;
                } else if (variant == 3) {
                    p->se_alloc_labs = 0;
                    p->se_alloc_psych = 0;
                    p->soc_effect_pending.efficiency = 1;
                    b->specialist_total = 4;
                    b->spec[0] = 0x3210;
                } else if (variant == 4) {
                    int facs[] = { FAC_ENERGY_BANK, FAC_NETWORK_NODE, FAC_BIOLOGY_LAB,
                        FAC_HOLOGRAM_THEATRE, FAC_TREE_FARM, FAC_HYBRID_FOREST, FAC_FUSION_LAB,
                        FAC_QUANTUM_LAB, FAC_RESEARCH_HOSPITAL, FAC_NANOHOSPITAL };
                    for (int k = 0; k < 10; k++) {
                        set_fac(base_id, facs[k]);
                    }
                    SecretProject->supercollider = base_id;
                    SecretProject->theory_of_everything = base_id;
                    SecretProject->space_elevator = base_id;
                    SecretProject->longevity_vaccine = base_id;
                    SecretProject->network_backbone = base_id;
                    SecretProject->planetary_energy_grid = base_id;
                    p->soc_category_pending.economics = SE_FREE_MARKET;
                    p->se_alloc_labs = 3;
                    p->se_alloc_psych = 1;
                } else if (variant == 5) {
                    b->queue_production_id[0] = -FAC_STOCKPILE_ENERGY;
                    b->production_id_last = -FAC_STOCKPILE_ENERGY;
                    b->mineral_surplus = 7 + base_id % 5;
                    SecretProject->planetary_energy_grid = (base_id % 2) ? base_id : -1;
                } else if (variant == 6) {
                    p->sanction_turns = 3;
                    for (int g = 0; g < MaxPlayerNum; g++) {
                        p->diplo_treaties[g] &= ~DTREATY_TREATY;
                        PlayersData[g].diplo_treaties[f] &= ~DTREATY_TREATY;
                    }
                }
                if (variant == 7) {
                    for (int g = 0; g < MaxPlayerNum; g++) {
                        p->diplo_treaties[g] &= ~(DTREATY_TREATY | DTREATY_PACT);
                        PlayersData[g].diplo_treaties[f] &= ~(DTREATY_TREATY | DTREATY_PACT);
                    }
                }
            };
            prepare();
            call_pair(base_minerals_OG, base_energy_OG);
            snap_take(&after_orig);
            memcpy(globals_o, BaseGlobals, BaseGlobalsSize);
            for (int g = 1; g < MaxPlayerNum; g++) {
                commerce += BaseCommerceImport[g];
            }
            grids += alien;
            prepare();
            call_pair(base_minerals, base_energy);
            snap_take(&after_new);
            memcpy(globals_n, BaseGlobals, BaseGlobalsSize);
            compared++;
            sprintf_s(label, "base_minerals+base_energy(%d) variant %d", base_id, variant);
            BOOL detail = mismatches < MAX_DETAIL;
            if (base_globals_differ(globals_o, globals_n, detail ? label : NULL)
                || snap_differs(&after_orig, &after_new, detail ? label : NULL)) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
    memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
    ProposalPassTable[PROP_GLOBAL_TRADE_PACT] = pass_saved[0];
    ProposalPassTable[PROP_REPEAL_GLOBAL_TRADE_PACT] = pass_saved[1];
    *GovernorFactionID = governor_saved;
    *BaseIDCurrentSelected = selected_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("base_energy (after base_minerals): %d compared (%d commerce imported, %d alien-base runs), %d mismatches",
        compared, commerce, grids, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 15

static int test_base_growth() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    uint8_t globals_saved[BaseGlobalsSize], globals_o[BaseGlobalsSize], globals_n[BaseGlobalsSize];
    memcpy(globals_saved, BaseGlobals, BaseGlobalsSize);
    int sp_saved[MaxSecretProjectNum];
    memcpy(sp_saved, SecretProjectTable, sizeof(sp_saved));
    int selected_saved = *BaseIDCurrentSelected;
    int compute_saved = *BaseComputeID;
    uint32_t state_saved = *GameState;
    int compared = 0, mismatches = 0, grew = 0, starved = 0;
    char label[80];
    for (int base_id = 0; base_id < base.base_count; base_id++) {
        uint32_t f = base.bases[base_id].faction_id_current;
        if (!quiet_faction(f)) {
            continue; // popups for the player at the controls
        }
        for (int variant = 0; variant < 9; variant++) {
            // as loaded; starving with a nutrient convoy parked here; starving once the game is
            // over; just enough nutrients; a pop boom; at the habitation limit with plenty;
            // stagnation; starvation ahead; a pop boom with cloning vats at the limit
            auto prepare = [&]() {
                snap_restore(&base);
                memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
                memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
                *GameState = state_saved;
                *BaseComputeID = compute_saved;
                set_base(base_id);
                Base *b = &Bases[base_id];
                int cost = cost_factor(f, RSC_NUTRIENTS, base_id) * (b->population_size + 1);
                if (variant == 1 || variant == 2) {
                    b->nutrients_accumulated = -3;
                    if (b->population_size < 2) {
                        b->population_size = 2;
                    }
                    if (variant == 2) {
                        *GameState |= STATE_UNK_1;
                        *GameState &= ~STATE_UNK_8;
                    }
                    BaseCurrentConvoyFrom[RSC_NUTRIENTS] = 1;
                    for (int v = 0; v < *VehCurrentCount; v++) {
                        if (Vehs[v].home_base_id == base_id
                            && VehPrototypes[Vehs[v].proto_id].plan == PLAN_SUPPLY_CONVOY) {
                            Vehs[v].order = ORDER_CONVOY;
                            Vehs[v].order_auto_type = RSC_NUTRIENTS;
                            Vehs[v].x = b->x;
                            Vehs[v].y = b->y;
                            break;
                        }
                    }
                } else if (variant == 3) {
                    b->nutrients_accumulated = cost;
                    *BaseCurrentGrowthRate = 0;
                } else if (variant == 4) {
                    *BaseCurrentGrowthRate = 6;
                    b->nutrient_surplus = Rules->nutrient_req_citizen + 1;
                    b->nutrients_accumulated = cost * 2;
                } else if (variant == 5 || variant == 8) {
                    b->population_size = (int8_t)Rules->pop_limit_sans_hab_complex;
                    uint32_t offset, mask;
                    bitmask(FAC_HAB_COMPLEX, &offset, &mask);
                    b->facilities_built[offset] &= ~mask;
                    b->nutrients_accumulated = 999;
                    *BaseCurrentGrowthRate = 0;
                    if (variant == 8) {
                        SecretProject->cloning_vats = base_id;
                        b->nutrient_surplus = Rules->nutrient_req_citizen;
                    }
                } else if (variant == 6) {
                    *BaseCurrentGrowthRate = -3;
                    b->nutrients_accumulated = cost + 5;
                } else if (variant == 7) {
                    b->nutrients_accumulated = 5;
                    b->nutrient_surplus = -3;
                    *BaseCurrentGrowthRate = 0;
                }
            };
            prepare();
            int pop_before = Bases[base_id].population_size;
            int r_o = base_growth_OG();
            snap_take(&after_orig);
            memcpy(globals_o, BaseGlobals, BaseGlobalsSize);
            int compute_o = *BaseComputeID;
            grew += Bases[base_id].population_size > pop_before;
            starved += Bases[base_id].population_size < pop_before;
            prepare();
            int r_n = base_growth();
            snap_take(&after_new);
            memcpy(globals_n, BaseGlobals, BaseGlobalsSize);
            int compute_n = *BaseComputeID;
            compared++;
            sprintf_s(label, "base_growth(%d) variant %d", base_id, variant);
            BOOL detail = mismatches < MAX_DETAIL;
            BOOL diff = false;
            if (r_o != r_n || compute_o != compute_n) {
                if (detail) {
                    logf("  %s: return orig %d new %d, computed base orig %d new %d", label, r_o, r_n,
                        compute_o, compute_n);
                }
                diff = true;
            }
            if (!diff) {
                diff = base_globals_differ(globals_o, globals_n, detail ? label : NULL)
                    || snap_differs(&after_orig, &after_new, detail ? label : NULL);
            }
            if (diff) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
    memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
    *GameState = state_saved;
    *BaseComputeID = compute_saved;
    *BaseIDCurrentSelected = selected_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("base_growth: %d compared (%d grew, %d starved by the original), %d mismatches",
        compared, grew, starved, mismatches);
    return mismatches;
}

static int test_base_doctors() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    uint8_t globals_saved[BaseGlobalsSize], globals_o[BaseGlobalsSize], globals_n[BaseGlobalsSize];
    memcpy(globals_saved, BaseGlobals, BaseGlobalsSize);
    int sp_saved[MaxSecretProjectNum];
    memcpy(sp_saved, SecretProjectTable, sizeof(sp_saved));
    int selected_saved = *BaseIDCurrentSelected;
    int compute_saved = *BaseComputeID;
    int control_saved = *ControlUpkeepA;
    int compared = 0, mismatches = 0, changed = 0;
    char label[80];
    for (int base_id = 0; base_id < base.base_count; base_id++) {
        uint32_t f = base.bases[base_id].faction_id_current;
        BOOL quiet = quiet_faction(f);
        for (int variant = 0; variant < 6; variant++) {
            // as loaded; drones ahead of talents in a fed base; three spare specialists in an
            // unfed one; rioting under weak police while about to grow; drones ahead with the
            // Telepathic Matrix; a human base under its governor (no slider advice)
            if (variant < 5 ? !quiet : (quiet || !is_human(f))) {
                continue;
            }
            auto prepare = [&]() {
                snap_restore(&base);
                memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
                memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
                *BaseComputeID = compute_saved;
                *ControlUpkeepA = control_saved;
                set_base(base_id);
                Base *b = &Bases[base_id];
                if (variant == 1 || variant == 4) {
                    *BaseYieldNutrientMet = 1;
                    b->drone_total = b->talent_total + 3;
                    if (variant == 4) {
                        SecretProject->telepathic_matrix = base_id;
                    }
                } else if (variant == 2) {
                    *BaseYieldNutrientMet = 0;
                    b->specialist_total += 3;
                    b->specialist_unk_1 = 0;
                    b->spec[0] = 0x14141414;
                } else if (variant == 3) {
                    b->state |= BSTATE_DRONE_RIOTS_ACTIVE;
                    PlayersData[f].soc_effect_pending.police = -3;
                    b->nutrients_accumulated = 999;
                    *BaseYieldNutrientMet = 1;
                    b->drone_total = b->talent_total + 1;
                } else if (variant == 5) {
                    b->governor |= GOV_MANAGES_CITIZENS_SPECS | GOV_ACTIVE;
                    *ControlUpkeepA = 0;
                    *BaseYieldNutrientMet = 1;
                    b->drone_total = b->talent_total + 2;
                }
            };
            prepare();
            int spec_before = Bases[base_id].specialist_total;
            base_doctors_OG();
            snap_take(&after_orig);
            memcpy(globals_o, BaseGlobals, BaseGlobalsSize);
            int compute_o = *BaseComputeID;
            changed += Bases[base_id].specialist_total != spec_before;
            prepare();
            base_doctors();
            snap_take(&after_new);
            memcpy(globals_n, BaseGlobals, BaseGlobalsSize);
            int compute_n = *BaseComputeID;
            compared++;
            sprintf_s(label, "base_doctors(%d) variant %d", base_id, variant);
            BOOL detail = mismatches < MAX_DETAIL;
            BOOL diff = false;
            if (compute_o != compute_n) {
                if (detail) {
                    logf("  %s: computed base orig %d new %d", label, compute_o, compute_n);
                }
                diff = true;
            }
            if (!diff) {
                diff = base_globals_differ(globals_o, globals_n, detail ? label : NULL)
                    || snap_differs(&after_orig, &after_new, detail ? label : NULL);
            }
            if (diff) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
    memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
    *BaseComputeID = compute_saved;
    *ControlUpkeepA = control_saved;
    *BaseIDCurrentSelected = selected_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("base_doctors: %d compared (%d specialist changes by the original), %d mismatches",
        compared, changed, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 16

// compare a string-returning call: same pointer, same text, same parse gender and plurality
struct StrResult {
    LPSTR ptr;
    char text[256];
    int gender;
    BOOL plural;
    void take(LPSTR p) {
        ptr = p;
        strncpy_s(text, p ? p : "(null)", _TRUNCATE);
        gender = *GenderDefault;
        plural = *PluralityDefault;
    }
};

static BOOL str_result_differs(const StrResult &a, const StrResult &b, const char *label) {
    if (a.ptr == b.ptr && !strcmp(a.text, b.text) && a.gender == b.gender && a.plural == b.plural) {
        return false;
    }
    if (label) {
        logf("  %s: orig %p \"%s\" g%d p%d, new %p \"%s\" g%d p%d", label, a.ptr, a.text, a.gender,
            a.plural, b.ptr, b.text, b.gender, b.plural);
    }
    return true;
}

static int test_faction_strings() {
    int compared = 0, mismatches = 0;
    char label[80];
    int female_saved[MaxPlayerNum];
    for (int f = 0; f < MaxPlayerNum; f++) {
        female_saved[f] = Players[f].is_leader_female;
    }
    int gender_saved = *GenderDefault;
    BOOL plural_saved = *PluralityDefault;
    struct { const char *name; func_s_i *orig; func_s_i *repl; } singles[] = {
        { "get_name", get_name_OG, get_name },
        { "get_title", get_title_OG, get_title },
        { "get_adjective_leader", get_adjective_leader_OG, get_adjective_leader },
        { "get_adjective_insult_leader", get_adjective_insult_leader_OG, get_adjective_insult_leader },
        { "get_adjective_faction", get_adjective_faction_OG, get_adjective_faction },
        { "get_adjective_insult_faction", get_adjective_insult_faction_OG, get_adjective_insult_faction },
        { "get_insult_leader", get_insult_leader_OG, get_insult_leader },
        { "get_pact", get_pact_OG, get_pact },
        { "get_pacts", get_pacts_OG, get_pacts },
    };
    struct { const char *name; func_s_ii *orig; func_s_ii *repl; BOOL pair; } doubles[] = {
        { "get_pacts_2", get_pacts_2_OG, get_pacts_2, true },
        { "get_pact_hood", get_pact_hood_OG, get_pact_hood, true },
        { "get_his_her", get_his_her_OG, get_his_her, false },
        { "get_him_her", get_him_her_OG, get_him_her, false },
        { "get_he_she", get_he_she_OG, get_he_she, false },
    };
    StrResult r_o, r_n;
    for (int variant = 0; variant < 3; variant++) {
        // leaders as loaded, all female (raw value 1), all with a raw value of 2
        for (int f = 0; f < MaxPlayerNum; f++) {
            Players[f].is_leader_female = variant ? variant : female_saved[f];
        }
        for (int f = 0; f < MaxPlayerNum; f++) {
            for (auto &s : singles) {
                *GenderDefault = 9; *PluralityDefault = 9;
                r_o.take(s.orig(f));
                *GenderDefault = 9; *PluralityDefault = 9;
                r_n.take(s.repl(f));
                compared++;
                sprintf_s(label, "%s(%d) variant %d", s.name, f, variant);
                mismatches += str_result_differs(r_o, r_n, mismatches < MAX_DETAIL ? label : NULL);
            }
            for (auto &d : doubles) {
                for (int k = 0; k < (d.pair ? MaxPlayerNum : 4); k++) {
                    // the second argument is another faction or the capitalization mode
                    *GenderDefault = 9; *PluralityDefault = 9;
                    r_o.take(d.orig(f, k));
                    *GenderDefault = 9; *PluralityDefault = 9;
                    r_n.take(d.repl(f, k));
                    compared++;
                    sprintf_s(label, "%s(%d, %d) variant %d", d.name, f, k, variant);
                    mismatches += str_result_differs(r_o, r_n, mismatches < MAX_DETAIL ? label : NULL);
                }
            }
        }
    }
    for (int f = 0; f < MaxPlayerNum; f++) {
        Players[f].is_leader_female = female_saved[f];
    }
    // the faction files: only factions in play, whose files are known to exist
    for (int f = 1; f < MaxPlayerNum; f++) {
        if (!is_alive(f) || !Players[f].filename[0]) {
            continue;
        }
        for (int phrase = 0; phrase < 4; phrase++) {
            *GenderDefault = 9; *PluralityDefault = 9;
            r_o.take(get_phrase_OG(f, phrase));
            *GenderDefault = 9; *PluralityDefault = 9;
            r_n.take(get_phrase(f, phrase));
            compared++;
            sprintf_s(label, "get_phrase(%d, %d)", f, phrase);
            mismatches += str_result_differs(r_o, r_n, mismatches < MAX_DETAIL ? label : NULL);
            *GenderDefault = 9; *PluralityDefault = 9;
            r_o.take(get_noun_phrase_OG(f, phrase));
            *GenderDefault = 9; *PluralityDefault = 9;
            r_n.take(get_noun_phrase(f, phrase));
            compared++;
            sprintf_s(label, "get_noun_phrase(%d, %d)", f, phrase);
            mismatches += str_result_differs(r_o, r_n, mismatches < MAX_DETAIL ? label : NULL);
        }
    }
    *GenderDefault = gender_saved;
    *PluralityDefault = plural_saved;
    logf("faction strings: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

static int test_small_helpers() {
    int compared = 0, mismatches = 0;
    for (int m = -2; m < 6; m++) {
        int a = mandate_color_OG(m), b = mandate_color(m);
        compared++;
        if (a != b) {
            if (mismatches < MAX_DETAIL) {
                logf("  mandate_color(%d): orig %d new %d", m, a, b);
            }
            mismatches++;
        }
    }
    RECT rect = { 3, -2, 9, 5 };
    for (int x = -1; x < 12; x++) {
        for (int y = -4; y < 8; y++) {
            BOOL a = in_box_OG(x, y, 3, -2, 6, 7), b = in_box(x, y, 3, -2, 6, 7);
            BOOL c = in_box_2_OG(x, y, &rect), d = in_box_2(x, y, &rect);
            compared += 2;
            if ((a != 0) != (b != 0) || (c != 0) != (d != 0)) {
                if (mismatches < MAX_DETAIL) {
                    logf("  in_box(%d, %d): orig %d/%d new %d/%d", x, y, a, c, b, d);
                }
                mismatches++;
            }
        }
    }
    char saved[sizeof(stringTemp->str)], after_o[sizeof(stringTemp->str)];
    memcpy(saved, stringTemp->str, sizeof(saved));
    for (int k = 0; k < 4; k++) {
        // clear, then space on an empty, a short and a long buffer
        const char *seeds[] = { "", "", "abc", "the quick brown fox" };
        func_void *orig = k ? space_OG : clear_OG;
        func_void *repl = k ? space : clear;
        memset(stringTemp->str, 'x', sizeof(saved) - 1);
        stringTemp->str[sizeof(saved) - 1] = 0;
        if (k) {
            strcpy_s(stringTemp->str, seeds[k]);
        }
        orig();
        memcpy(after_o, stringTemp->str, sizeof(after_o));
        memset(stringTemp->str, 'x', sizeof(saved) - 1);
        stringTemp->str[sizeof(saved) - 1] = 0;
        if (k) {
            strcpy_s(stringTemp->str, seeds[k]);
        }
        repl();
        compared++;
        size_t d = first_diff((uint8_t *)after_o, (uint8_t *)stringTemp->str, sizeof(after_o));
        if (d != (size_t)-1) {
            if (mismatches < MAX_DETAIL) {
                logf("  %s seed %d: buffer byte %u differs", k ? "space" : "clear", k, (unsigned)d);
            }
            mismatches++;
        }
    }
    memcpy(stringTemp->str, saved, sizeof(saved));
    logf("small helpers: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

static int test_generators() {
    GameSnapshot base;
    snap_alloc(&base);
    snap_take(&base);
    uint32_t size_saved = Rules->size_base_subspace_gen;
    int compared = 0, mismatches = 0, working = 0;
    char label[64];
    for (int variant = 0; variant < 3; variant++) {
        // as loaded; a generator in every third base; the same with the size rule at 1
        snap_restore(&base);
        Rules->size_base_subspace_gen = variant == 2 ? 1 : size_saved;
        if (variant) {
            for (int i = 0; i < *BaseCurrentCount; i += 3) {
                set_fac(i, FAC_SUBSPACE_GENERATOR);
            }
        }
        for (uint32_t f = 0; f < MaxPlayerNum; f++) {
            for (int with = 0; with < 2; with++) {
                int in_o = -5, in_n = -5;
                int a = generators_OG(f, with ? &in_o : NULL);
                int b = generators(f, with ? &in_n : NULL);
                compared++;
                working += a;
                if (a != b || in_o != in_n) {
                    if (mismatches < MAX_DETAIL) {
                        sprintf_s(label, "generators(%u, %d) variant %d", f, with, variant);
                        logf("  %s: orig %d (%d) new %d (%d)", label, a, in_o, b, in_n);
                    }
                    mismatches++;
                }
            }
        }
    }
    snap_restore(&base);
    Rules->size_base_subspace_gen = size_saved;
    snap_free(&base);
    logf("generators: %d compared (%d working generators counted by the original), %d mismatches",
        compared, working, mismatches);
    return mismatches;
}

static int test_base_compute() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    uint8_t globals_saved[BaseGlobalsSize], globals_o[BaseGlobalsSize], globals_n[BaseGlobalsSize];
    memcpy(globals_saved, BaseGlobals, BaseGlobalsSize);
    int selected_saved = *BaseIDCurrentSelected;
    int compute_saved = *BaseComputeID;
    int compared = 0, mismatches = 0;
    char label[80];
    for (int base_id = 0; base_id < base.base_count; base_id += 2) {
        for (int variant = 0; variant < 4; variant++) {
            // already computed or not, forced or not
            auto prepare = [&]() {
                snap_restore(&base);
                memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
                set_base(base_id);
                *BaseComputeID = (variant & 1) ? base_id : -1;
                Bases[base_id].population_size++; // so a recompute shows
            };
            prepare();
            base_compute_OG(variant >= 2);
            snap_take(&after_orig);
            memcpy(globals_o, BaseGlobals, BaseGlobalsSize);
            int compute_o = *BaseComputeID;
            prepare();
            base_compute(variant >= 2);
            snap_take(&after_new);
            memcpy(globals_n, BaseGlobals, BaseGlobalsSize);
            int compute_n = *BaseComputeID;
            compared++;
            sprintf_s(label, "base_compute(%d) variant %d", base_id, variant);
            BOOL detail = mismatches < MAX_DETAIL;
            BOOL diff = compute_o != compute_n;
            if (diff && detail) {
                logf("  %s: computed base orig %d new %d", label, compute_o, compute_n);
            }
            if (!diff) {
                diff = base_globals_differ(globals_o, globals_n, detail ? label : NULL)
                    || snap_differs(&after_orig, &after_new, detail ? label : NULL);
            }
            if (diff) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
    *BaseIDCurrentSelected = selected_saved;
    *BaseComputeID = compute_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("base_compute: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 17

static int test_assemble_passengers() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int turn_saved = *TurnCurrentNum;
    int compared = 0, mismatches = 0;
    char label[80];
    int plans[] = { PLAN_OFFENSIVE, PLAN_DEFENSIVE, PLAN_RECONNAISANCE, PLAN_NAVAL_TRANSPORT,
        PLAN_COLONIZATION };
    for (int base_id = 0; base_id < base.base_count; base_id += 3) {
        uint32_t f = base.bases[base_id].faction_id_current;
        for (int p = 0; p < 5; p++) {
            for (int variant = 0; variant < 3; variant++) {
                // as loaded; vendetta with everyone, a colonization region and an early turn;
                // the same with a colony pod in the base and a cautious faction
                auto prepare = [&]() {
                    snap_restore(&base);
                    *TurnCurrentNum = variant ? 40 : turn_saved;
                    if (variant) {
                        uint32_t region = map_loc(Bases[base_id].x, Bases[base_id].y)->region;
                        PlayersData[f].unk_77[region] |= 0x800;
                        for (int g = 1; g < MaxPlayerNum; g++) {
                            if (g != (int)f) {
                                PlayersData[f].diplo_treaties[g] |= DTREATY_VENDETTA;
                            }
                        }
                        PlayersData[f].ai_growth = 1;
                        PlayersData[f].ai_fight = variant == 2 ? 1 : -1;
                    }
                    if (variant == 2) {
                        for (int v = 0; v < *VehCurrentCount; v++) {
                            if (Vehs[v].faction_id == f
                                && VehPrototypes[Vehs[v].proto_id].plan == PLAN_COLONIZATION) {
                                Vehs[v].x = Bases[base_id].x;
                                Vehs[v].y = Bases[base_id].y;
                                break;
                            }
                        }
                    }
                };
                int priority = 2 + variant;
                prepare();
                assemble_passengers_OG(base_id, plans[p], priority);
                snap_take(&after_orig);
                prepare();
                assemble_passengers(base_id, plans[p], priority);
                snap_take(&after_new);
                compared++;
                sprintf_s(label, "assemble_passengers(%d, %d) variant %d", base_id, plans[p], variant);
                if (snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL)) {
                    mismatches++;
                }
            }
        }
    }
    snap_restore(&base);
    *TurnCurrentNum = turn_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("assemble_passengers: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

static int test_alien_base() {
    GameSnapshot base;
    snap_alloc(&base);
    snap_take(&base);
    int dist_saved = *BaseFindDist;
    int compared = 0, mismatches = 0, found = 0, natives = 0;
    char label[80];
    for (int veh_id = 0; veh_id < base.veh_count; veh_id++) {
        BOOL native = base.vehs[veh_id].faction_id == 0;
        // every native unit, and every 25th other unit
        if (!native && veh_id % 25) {
            continue;
        }
        natives += native;
        for (int variant = 0; variant < 3; variant++) {
            // where it stands, at a tile across the map, and as a sea lurk at sea
            snap_restore(&base);
            int x = base.vehs[veh_id].x, y = base.vehs[veh_id].y;
            if (variant == 1) {
                x = (x + *MapLongitudeBounds / 2) % *MapLongitudeBounds;
                y = (y + 7) % *MapLatitudeBounds;
                x = (x & ~1) | (y & 1);
            } else if (variant == 2) {
                Vehs[veh_id].proto_id = BSC_SEALURK;
                for (uint32_t i = (veh_id * 37) % *MapArea; i < *MapArea; i++) {
                    if ((*MapTiles)[i].climate < ALT_BIT_SHORE_LINE) {
                        y = i / *MapLongitude;
                        x = (i % *MapLongitude) * 2 + (y & 1);
                        break;
                    }
                }
            }
            *BaseFindDist = -7;
            int a = alien_base_OG(veh_id, x, y);
            int dist_o = *BaseFindDist;
            *BaseFindDist = -7;
            int b = alien_base(veh_id, x, y);
            int dist_n = *BaseFindDist;
            compared++;
            found += a >= 0;
            if (a != b || dist_o != dist_n) {
                if (mismatches < MAX_DETAIL) {
                    sprintf_s(label, "alien_base(%d, %d, %d) variant %d", veh_id, x, y, variant);
                    logf("  %s: orig %d (%d) new %d (%d)", label, a, dist_o, b, dist_n);
                }
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    *BaseFindDist = dist_saved;
    snap_free(&base);
    logf("alien_base: %d compared (%d native units, %d bases found by the original), %d mismatches",
        compared, natives, found, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 18

static int test_repair_phase() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int sp_saved[MaxSecretProjectNum];
    memcpy(sp_saved, SecretProjectTable, sizeof(sp_saved));
    int turn_saved = *TurnCurrentNum;
    int compared = 0, mismatches = 0, repaired = 0;
    char label[64];
    for (uint32_t f = 0; f < MaxPlayerNum; f++) {
        int own_base = -1;
        for (int i = 0; i < base.base_count; i++) {
            if (base.bases[i].faction_id_current == f) {
                own_base = i;
                break;
            }
        }
        for (int variant = 0; variant < 5; variant++) {
            // as loaded; every unit hurt and rested, some sentried, on the next three turns;
            // the same with the Nano Factory; with the Xenoempathy Dome; with every repair
            // facility project of the faction's
            auto prepare = [&]() {
                snap_restore(&base);
                memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
                *TurnCurrentNum = turn_saved + variant;
                if (variant) {
                    for (int v = 0; v < *VehCurrentCount; v++) {
                        if (Vehs[v].faction_id != f) {
                            continue;
                        }
                        Vehs[v].dmg_incurred = (uint8_t)(3 + v % 9);
                        if (v % 3) {
                            Vehs[v].state &= ~VSTATE_UNK_4;
                        }
                        if (v % 4 == 0) {
                            Vehs[v].order = ORDER_SENTRY_BOARD;
                        } else if (v % 4 == 1) {
                            Vehs[v].order = ORDER_HOLD;
                            Vehs[v].waypoint_y[0] = (int16_t)(v % 3);
                        }
                        if (v % 5 == 0) {
                            Vehs[v].state |= VSTATE_UNK_8;
                        }
                    }
                }
                if (own_base >= 0) {
                    if (variant == 2) {
                        SecretProject->nano_factory = own_base;
                    } else if (variant == 3) {
                        SecretProject->xenoempathy_dome = own_base;
                    } else if (variant == 4) {
                        SecretProject->command_nexus = own_base;
                        SecretProject->maritime_control_center = own_base;
                        SecretProject->cloudbase_academy = own_base;
                    }
                }
            };
            prepare();
            repair_phase_OG(f);
            snap_take(&after_orig);
            prepare();
            for (int v = 0; v < *VehCurrentCount; v++) {
                repaired += Vehs[v].faction_id == f && after_orig.vehs[v].dmg_incurred < Vehs[v].dmg_incurred;
            }
            repair_phase(f);
            snap_take(&after_new);
            compared++;
            sprintf_s(label, "repair_phase(%u) variant %d", f, variant);
            if (snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL)) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
    *TurnCurrentNum = turn_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("repair_phase: %d compared (%d units repaired by the original), %d mismatches", compared,
        repaired, mismatches);
    return mismatches;
}

static int test_allocate_energy() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    uint8_t globals_saved[BaseGlobalsSize], globals_o[BaseGlobalsSize], globals_n[BaseGlobalsSize];
    memcpy(globals_saved, BaseGlobals, BaseGlobalsSize);
    int sp_saved[MaxSecretProjectNum];
    memcpy(sp_saved, SecretProjectTable, sizeof(sp_saved));
    int selected_saved = *BaseIDCurrentSelected;
    int compute_saved = *BaseComputeID;
    int turn_saved = *TurnCurrentNum;
    int compared = 0, mismatches = 0, changed = 0;
    char label[64];
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        if (f == (uint32_t)*FactionIDCurrent) {
            continue; // the player at the controls may get the infrastructure popups
        }
        int own_base = -1;
        for (int i = 0; i < base.base_count; i++) {
            if (base.bases[i].faction_id_current == f) {
                own_base = i;
                break;
            }
        }
        for (int variant = 0; variant < 8; variant++) {
            // as loaded; the current base rioting under police -3 (quelled base by base); the
            // same at police -2; calm with spare specialists; researching Transcendent Thought
            // with big reserves early on; a defensive region; a wealth/power drive with every
            // region offensive; the Telepathic Matrix with all the energy in economy
            auto prepare = [&]() {
                snap_restore(&base);
                memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
                memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
                *BaseComputeID = compute_saved;
                *TurnCurrentNum = turn_saved;
                PlayerData *p = &PlayersData[f];
                set_base(own_base >= 0 ? own_base : 0);
                Base *b = *BaseCurrent;
                if (variant == 1 || variant == 2) {
                    b->state |= BSTATE_DRONE_RIOTS_ACTIVE;
                    b->drone_total = b->talent_total + 2;
                    p->soc_effect_pending.police = variant == 1 ? -3 : -2;
                    p->se_alloc_psych = 0;
                    if (own_base >= 0) {
                        Bases[own_base].state |= BSTATE_DRONE_RIOTS_ACTIVE;
                    }
                } else if (variant == 3) {
                    b->state &= ~BSTATE_DRONE_RIOTS_ACTIVE;
                    b->specialist_unk_1 = 0;
                    b->specialist_total = 2;
                    p->se_alloc_psych = 3;
                } else if (variant == 4) {
                    p->tech_id_researching = TECH_TRANT;
                    p->energy_reserves = 5000;
                    *TurnCurrentNum = 20;
                } else if (variant == 5) {
                    for (int r = 1; r < MaxContinentNum; r++) {
                        if (p->region_total_bases[r]) {
                            p->region_base_plan[r] = 2;
                            break;
                        }
                    }
                } else if (variant == 6) {
                    p->ai_wealth = 1;
                    p->ai_power = 1;
                    p->ai_tech = 0;
                    for (int r = 1; r < MaxContinentNum; r++) {
                        p->region_base_plan[r] = 0;
                    }
                } else if (variant == 7) {
                    if (own_base >= 0) {
                        SecretProject->telepathic_matrix = own_base;
                    }
                    p->se_alloc_psych = 0;
                    p->se_alloc_labs = 0;
                    p->ai_tech = 1;
                    p->ai_wealth = 0;
                    p->ai_power = 0;
                }
            };
            prepare();
            int labs_before = PlayersData[f].se_alloc_labs, psych_before = PlayersData[f].se_alloc_psych;
            allocate_energy_OG(f);
            snap_take(&after_orig);
            memcpy(globals_o, BaseGlobals, BaseGlobalsSize);
            int sel_o = *BaseIDCurrentSelected, compute_o = *BaseComputeID;
            changed += PlayersData[f].se_alloc_labs != labs_before || PlayersData[f].se_alloc_psych != psych_before;
            prepare();
            allocate_energy(f);
            snap_take(&after_new);
            memcpy(globals_n, BaseGlobals, BaseGlobalsSize);
            int sel_n = *BaseIDCurrentSelected, compute_n = *BaseComputeID;
            compared++;
            sprintf_s(label, "allocate_energy(%u) variant %d", f, variant);
            BOOL detail = mismatches < MAX_DETAIL;
            BOOL diff = sel_o != sel_n || compute_o != compute_n;
            if (diff && detail) {
                logf("  %s: selected base orig %d new %d, computed orig %d new %d", label, sel_o,
                    sel_n, compute_o, compute_n);
            }
            if (!diff) {
                diff = base_globals_differ(globals_o, globals_n, detail ? label : NULL)
                    || snap_differs(&after_orig, &after_new, detail ? label : NULL);
            }
            if (diff) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
    memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
    *BaseIDCurrentSelected = selected_saved;
    *BaseComputeID = compute_saved;
    *TurnCurrentNum = turn_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("allocate_energy: %d compared (%d allocations changed by the original), %d mismatches",
        compared, changed, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 19

static int test_map_dirty() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0;
    for (int which = 0; which < 2; which++) {
        // set_dirty, clear_dirty, starting from half the tiles unflagged and no refresh pending
        auto prepare = [&]() {
            snap_restore(&base);
            for (uint32_t i = 0; i < base.tile_count; i += 2) {
                (*MapTiles)[i].bit2 &= ~BIT2_UNK_400000;
            }
            *UnkBitfield1 &= ~4;
        };
        prepare();
        (which ? clear_dirty_OG : set_dirty_OG)();
        snap_take(&after_orig);
        prepare();
        (which ? clear_dirty : set_dirty)();
        snap_take(&after_new);
        compared++;
        if (snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL
            ? (which ? "clear_dirty" : "set_dirty") : NULL)) {
            mismatches++;
        }
    }
    snap_restore(&base);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("set_dirty/clear_dirty: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

static int test_world_UNK1() {
    uint32_t draw_saved = *UnkBitfield1;
    int compared = 0, mismatches = 0;
    for (int y = 0; y < (int)*MapLatitudeBounds; y++) {
        for (int x = y & 1; x < (int)*MapLongitudeBounds; x += 2) {
            Map *tile = map_loc(x, y);
            Map saved = *tile, after_o;
            *UnkBitfield1 = draw_saved & ~1;
            world_UNK1_OG(x, y);
            after_o = *tile;
            uint32_t draw_o = *UnkBitfield1;
            *tile = saved;
            *UnkBitfield1 = draw_saved & ~1;
            world_UNK1(x, y);
            compared++;
            if (memcmp(&after_o, tile, sizeof(Map)) || draw_o != *UnkBitfield1) {
                if (mismatches < MAX_DETAIL) {
                    logf("  world_UNK1(%u, %u): climate orig %02X new %02X", x, y, after_o.climate,
                        tile->climate);
                }
                mismatches++;
            }
            *tile = saved;
        }
    }
    *UnkBitfield1 = draw_saved;
    logf("world_UNK1: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

static int test_do_fungal_towers() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0, towers = 0;
    char label[64];
    for (int variant = 0; variant < 4; variant++) {
        // as loaded, then every native unit (and every 40th other unit) turned into a fungal tower
        // under three seeds
        auto prepare = [&]() {
            snap_restore(&base);
            reseed(variant * 31 + 5);
            if (variant) {
                for (int v = 0; v < *VehCurrentCount; v++) {
                    if (Vehs[v].faction_id == 0 || v % 40 == 0) {
                        Vehs[v].proto_id = BSC_FUNGAL_TOWER;
                    }
                }
            }
        };
        prepare();
        for (int v = 0; v < *VehCurrentCount; v++) {
            towers += Vehs[v].proto_id == BSC_FUNGAL_TOWER;
        }
        do_fungal_towers_OG();
        snap_take(&after_orig);
        prepare();
        do_fungal_towers();
        snap_take(&after_new);
        compared++;
        sprintf_s(label, "do_fungal_towers variant %d", variant);
        if (snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL)) {
            mismatches++;
        }
    }
    snap_restore(&base);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("do_fungal_towers: %d compared (%d towers), %d mismatches", compared, towers, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 20

static int test_base_drones() {
    TimeGetTimePin pin; // drone_riot's revolt check reads the clock
    if (!pin.ok()) {
        logf("base_drones: cannot pin timeGetTime");
        return 1;
    }
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    uint8_t globals_saved[BaseGlobalsSize], globals_o[BaseGlobalsSize], globals_n[BaseGlobalsSize];
    memcpy(globals_saved, BaseGlobals, BaseGlobalsSize);
    int sp_saved[MaxSecretProjectNum];
    memcpy(sp_saved, SecretProjectTable, sizeof(sp_saved));
    int selected_saved = *BaseIDCurrentSelected;
    int compared = 0, mismatches = 0, riots = 0, golden = 0;
    char label[80];
    for (int base_id = 0; base_id < base.base_count; base_id++) {
        uint32_t f = base.bases[base_id].faction_id_current;
        if (!quiet_faction(f)) {
            continue; // popups for human players
        }
        for (int variant = 0; variant < 7; variant++) {
            // as loaded; a golden age starting; a golden age ending; riots ending; riots
            // breaking out; riots held down by a nerve staple; riots under the Telepathic Matrix
            auto prepare = [&]() {
                snap_restore(&base);
                memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
                memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
                reseed(base_id * 13 + variant);
                set_base(base_id);
                Base *b = &Bases[base_id];
                if (variant == 1) {
                    if (b->population_size < 3) {
                        b->population_size = 3;
                    }
                    b->drone_total = 0;
                    b->talent_total = b->population_size;
                    b->state &= ~BSTATE_GOLDEN_AGE_ACTIVE;
                } else if (variant == 2) {
                    b->state |= BSTATE_GOLDEN_AGE_ACTIVE | BSTATE_DRONE_RIOTS_ACTIVE;
                    b->drone_total = 0;
                    b->talent_total = 0;
                } else if (variant == 3) {
                    b->state |= BSTATE_DRONE_RIOTS_ACTIVE;
                    b->state &= ~BSTATE_GOLDEN_AGE_ACTIVE;
                    b->drone_total = 1;
                    b->talent_total = 1;
                } else if (variant >= 4) {
                    b->drone_total = b->talent_total + 2;
                    b->nerve_staple_turns_left = variant == 5 ? 3 : 0;
                    b->state |= BSTATE_DRONE_RIOTS_ACTIVE;
                    if (variant == 6) {
                        SecretProject->telepathic_matrix = base_id;
                    }
                }
            };
            prepare();
            base_drones_OG();
            snap_take(&after_orig);
            memcpy(globals_o, BaseGlobals, BaseGlobalsSize);
            riots += (after_orig.bases[base_id].state & BSTATE_DRONE_RIOTS_ACTIVE) != 0;
            golden += (after_orig.bases[base_id].state & BSTATE_GOLDEN_AGE_ACTIVE) != 0;
            prepare();
            base_drones();
            snap_take(&after_new);
            memcpy(globals_n, BaseGlobals, BaseGlobalsSize);
            compared++;
            sprintf_s(label, "base_drones(%d) variant %d", base_id, variant);
            BOOL detail = mismatches < MAX_DETAIL;
            if (base_globals_differ(globals_o, globals_n, detail ? label : NULL)
                || snap_differs(&after_orig, &after_new, detail ? label : NULL)) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
    memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
    *BaseIDCurrentSelected = selected_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("base_drones: %d compared (%d rioting, %d golden ages after the original), %d mismatches",
        compared, riots, golden, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 21

struct MonumentFn {
    const char *name;
    int kind; // 0: (faction), 1: (faction, name), 2: (faction, int)
    void *orig;
    void *repl;
};

static int test_mon_records() {
    typedef void mon_i(int);
    typedef void mon_s(int, LPCSTR);
    typedef void mon_ii(int, int);
    const MonumentFn fns[] = {
        { "mon_colony_founded", 1, (void *)mon_colony_founded_OG, (void *)mon_colony_founded },
        { "mon_secrets_of_tech", 0, (void *)mon_secrets_of_tech_OG, (void *)mon_secrets_of_tech },
        { "mon_prototype_built", 0, (void *)mon_prototype_built_OG, (void *)mon_prototype_built },
        { "mon_facility_built", 1, (void *)mon_facility_built_OG, (void *)mon_facility_built },
        { "mon_secret_project", 2, (void *)mon_secret_project_OG, (void *)mon_secret_project },
        { "mon_enemy_destroyed", 2, (void *)mon_enemy_destroyed_OG, (void *)mon_enemy_destroyed },
        { "mon_conquer_base", 1, (void *)mon_conquer_base_OG, (void *)mon_conquer_base },
        { "mon_naval_unit_built", 0, (void *)mon_naval_unit_built_OG, (void *)mon_naval_unit_built },
        { "mon_air_unit_built", 0, (void *)mon_air_unit_built_OG, (void *)mon_air_unit_built },
        { "mon_native_life_bred", 0, (void *)mon_native_life_bred_OG, (void *)mon_native_life_bred },
        { "mon_first_in_space", 0, (void *)mon_first_in_space_OG, (void *)mon_first_in_space },
        { "mon_built_preserve", 0, (void *)mon_built_preserve_OG, (void *)mon_built_preserve },
        { "mon_killed_faction", 2, (void *)mon_killed_faction_OG, (void *)mon_killed_faction },
    };
    uint8_t *saved = (uint8_t *)malloc(MonumentTableSize);
    uint8_t *after_o = (uint8_t *)malloc(MonumentTableSize);
    memcpy(saved, MonumentTable, MonumentTableSize);
    int compared = 0, mismatches = 0, recorded = 0;
    char label[96];
    const char *names[] = { "Firstville", "A rather long base name for strncpy" };
    for (const MonumentFn &fn : fns) {
        for (int f = 0; f < MaxPlayerNum; f++) {
            if (f == *FactionIDCurrent) {
                continue; // the player at the controls gets the monument screen
            }
            for (int variant = 0; variant < 6; variant++) {
                // as loaded; every record clear; this faction's records clear while others hold
                // them; others holding them with counts; the second and third calls on top
                // (secrets of tech, killed-faction slots); all six killed slots taken
                auto prepare = [&]() {
                    memcpy(MonumentTable, saved, MonumentTableSize);
                    if (variant == 1) {
                        for (int g = 0; g < MaxPlayerNum; g++) {
                            for (int r = 0; r < MaxMonumentRecordNum; r++) {
                                Monuments[g].records[r].achieved = false;
                            }
                        }
                    } else if (variant == 2 || variant == 3) {
                        for (int g = 0; g < MaxPlayerNum; g++) {
                            for (int r = 0; r < MaxMonumentRecordNum; r++) {
                                Monuments[g].records[r].achieved = g != f && (g + r) % 2;
                                Monuments[g].records[r].faction_count = variant == 3 ? (g + r) % 3 == 0 : 0;
                            }
                        }
                    } else if (variant == 4) {
                        for (int r = 0; r < MaxMonumentRecordNum; r++) {
                            Monuments[f].records[r].achieved = r % 2 == 0;
                        }
                        Monuments[f].records[MONUMENT_SECRETS_OF_TECH].data = (f % 2) ? -1 : 2100;
                        Monuments[f].records[MONUMENT_SECRETS_OF_TECH].unk_1 = -1;
                    } else if (variant == 5) {
                        for (int r = MONUMENT_KILLED_FACTION; r < MaxMonumentRecordNum; r++) {
                            Monuments[f].records[r].achieved = true;
                        }
                    }
                };
                for (int arg = 0; arg < 2; arg++) {
                    int int_arg = arg ? 3 : 0;
                    prepare();
                    if (fn.kind == 0) {
                        ((mon_i *)fn.orig)(f);
                    } else if (fn.kind == 1) {
                        ((mon_s *)fn.orig)(f, names[arg]);
                    } else {
                        ((mon_ii *)fn.orig)(f, int_arg);
                    }
                    memcpy(after_o, MonumentTable, MonumentTableSize);
                    recorded += memcmp(after_o, saved, MonumentTableSize) != 0;
                    prepare();
                    if (fn.kind == 0) {
                        ((mon_i *)fn.repl)(f);
                    } else if (fn.kind == 1) {
                        ((mon_s *)fn.repl)(f, names[arg]);
                    } else {
                        ((mon_ii *)fn.repl)(f, int_arg);
                    }
                    compared++;
                    size_t d = first_diff(after_o, MonumentTable, MonumentTableSize);
                    if (d != (size_t)-1) {
                        if (mismatches < MAX_DETAIL) {
                            sprintf_s(label, "%s(%d) variant %d arg %d", fn.name, f, variant, arg);
                            logf("  %s: monument byte %u differs (orig %02X new %02X)", label,
                                (unsigned)d, after_o[d], MonumentTable[d]);
                        }
                        mismatches++;
                    }
                }
            }
        }
    }
    memcpy(MonumentTable, saved, MonumentTableSize);
    free(saved);
    free(after_o);
    logf("mon_records: %d compared (%d changed the table in the original), %d mismatches", compared,
        recorded, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 22

static int test_valid_patrol() {
    int compared = 0, mismatches = 0, valid = 0;
    for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
        uint8_t triad = Chassis[VehPrototypes[Vehs[veh_id].proto_id].chassis_id].triad;
        if (triad != TRIAD_AIR && veh_id % 4) {
            continue; // every aircraft, every 4th other unit
        }
        int x0 = Vehs[veh_id].x, y0 = Vehs[veh_id].y;
        for (int k = 0; k < 40; k++) {
            // the unit's own tile, then targets spiralling out to 16 tiles, then a few off the map
            int x = x0 + ((k * 7) % 33) - 16;
            int y = y0 + ((k * 11) % 33) - 16;
            if (k == 0) {
                x = x0;
                y = y0;
            } else if (k >= 37) {
                x = k == 37 ? -2 : x;
                y = k == 38 ? -1 : (k == 39 ? (int)*MapLatitudeBounds : y);
            }
            if (x >= 0 && y >= 0 && (x & 1) != (y & 1)) {
                x ^= 1;
            }
            BOOL a = valid_patrol_OG(veh_id, x, y);
            BOOL b = valid_patrol(veh_id, x, y);
            compared++;
            valid += a != 0;
            if ((a != 0) != (b != 0)) {
                if (mismatches < MAX_DETAIL) {
                    logf("  valid_patrol(%d, %d, %d): orig %d new %d", veh_id, x, y, a, b);
                }
                mismatches++;
            }
        }
    }
    logf("valid_patrol: %d compared (%d valid by the original), %d mismatches", compared, valid,
        mismatches);
    return mismatches;
}

// energy_compute() fills the energy report between 0x7492DC and 0x749C10
static uint8_t *const EnergyReport = (uint8_t *)0x007492DC;
static const size_t EnergyReportSize = 0x749C10 - 0x7492DC;

static int test_energy_compute() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    uint8_t globals_saved[BaseGlobalsSize], globals_o[BaseGlobalsSize], globals_n[BaseGlobalsSize];
    memcpy(globals_saved, BaseGlobals, BaseGlobalsSize);
    uint8_t *report_saved = (uint8_t *)malloc(EnergyReportSize);
    uint8_t *report_o = (uint8_t *)malloc(EnergyReportSize);
    memcpy(report_saved, EnergyReport, EnergyReportSize);
    int sp_saved[MaxSecretProjectNum];
    memcpy(sp_saved, SecretProjectTable, sizeof(sp_saved));
    int selected_saved = *BaseIDCurrentSelected;
    int compute_saved = *BaseComputeID;
    int compared = 0, mismatches = 0;
    char label[64];
    for (uint32_t f = 0; f < MaxPlayerNum; f++) {
        for (int variant = 0; variant < 6; variant++) {
            // as loaded with and without the refresh; loans both ways with a sanction; the
            // Self-Aware Colony; a research penalty; vendetta with everyone
            BOOL recompute = variant == 1;
            auto prepare = [&]() {
                snap_restore(&base);
                memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
                memcpy(EnergyReport, report_saved, EnergyReportSize);
                memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
                *BaseComputeID = compute_saved;
                PlayerData *p = &PlayersData[f];
                if (variant == 2) {
                    for (int g = 1; g < MaxPlayerNum; g++) {
                        p->loan_balance[g] = g % 2 ? 50 : 0;
                        p->loan_payment[g] = 3 + g;
                        PlayersData[g].loan_balance[f] = g % 3 ? 40 : 0;
                        PlayersData[g].loan_payment[f] = 2 + g;
                    }
                    PlayersData[2].sanction_turns = 4;
                } else if (variant == 3) {
                    for (int i = 0; i < *BaseCurrentCount; i++) {
                        if (Bases[i].faction_id_current == f) {
                            SecretProject->self_aware_colony = i;
                            break;
                        }
                    }
                } else if (variant == 4) {
                    p->soc_effect_pending.research = -7;
                } else if (variant == 5) {
                    for (int g = 0; g < MaxPlayerNum; g++) {
                        p->diplo_treaties[g] |= DTREATY_VENDETTA;
                    }
                }
            };
            prepare();
            energy_compute_OG(f, recompute);
            snap_take(&after_orig);
            memcpy(globals_o, BaseGlobals, BaseGlobalsSize);
            memcpy(report_o, EnergyReport, EnergyReportSize);
            int sel_o = *BaseIDCurrentSelected, compute_o = *BaseComputeID;
            prepare();
            energy_compute(f, recompute);
            snap_take(&after_new);
            memcpy(globals_n, BaseGlobals, BaseGlobalsSize);
            compared++;
            sprintf_s(label, "energy_compute(%u) variant %d", f, variant);
            BOOL detail = mismatches < MAX_DETAIL;
            BOOL diff = false;
            size_t d = first_diff(report_o, EnergyReport, EnergyReportSize);
            if (d != (size_t)-1 || sel_o != *BaseIDCurrentSelected || compute_o != *BaseComputeID) {
                if (detail) {
                    logf("  %s: report byte %d differs (selected %d/%d, computed %d/%d)", label,
                        (int)d, sel_o, *BaseIDCurrentSelected, compute_o, *BaseComputeID);
                }
                diff = true;
            }
            if (!diff) {
                diff = base_globals_differ(globals_o, globals_n, detail ? label : NULL)
                    || snap_differs(&after_orig, &after_new, detail ? label : NULL);
            }
            if (diff) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
    memcpy(EnergyReport, report_saved, EnergyReportSize);
    memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
    *BaseIDCurrentSelected = selected_saved;
    *BaseComputeID = compute_saved;
    free(report_saved);
    free(report_o);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("energy_compute: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 23

static const uint32_t ProtoTestAbilities[] = { 0, ABL_AMPHIBIOUS, ABL_AIR_SUPERIORITY, ABL_ARTILLERY,
    ABL_POLICE_2X, ABL_TRANCE, ABL_CLEAN_REACTOR | ABL_DEEP_RADAR, ABL_AMPHIBIOUS | ABL_ARTILLERY,
    ABL_AIR_SUPERIORITY | ABL_ARTILLERY, ABL_SUPER_TERRAFORMER, ABL_DROP_POD | ABL_AMPHIBIOUS,
    ABL_EMPATHIC | ABL_POLICE_2X, ABL_AAA, ABL_HEAVY_TRANSPORT };
static const int ProtoTestAbilityCount = (int)(sizeof(ProtoTestAbilities) / sizeof(uint32_t));

// a missile chassis carries a payload or a planet buster, nothing else carries a payload
static BOOL proto_test_plausible(uint32_t chassis_id, uint32_t weapon_id) {
    BOOL payload = weapon_id == WPN_PLANET_BUSTER || weapon_id >= WPN_CONVENTIONAL_PAYLOAD;
    if (Chassis[chassis_id].missile) {
        return payload;
    }
    return weapon_id < WPN_CONVENTIONAL_PAYLOAD;
}

static int test_name_proto() {
    char name_o[256], name_n[256];
    int compared = 0, mismatches = 0, marked = 0, category_10 = 0, police_slots = 0, police = 0;
    auto check = [&](int proto_id, int faction_id, uint32_t chs, uint32_t wpn, uint32_t arm,
        uint32_t abl, uint32_t rec) {
        memset(name_o, 0x5A, sizeof(name_o));
        memset(name_n, 0x5A, sizeof(name_n));
        name_proto_OG(name_o, proto_id, faction_id, chs, wpn, arm, abl, rec);
        name_proto(name_n, proto_id, faction_id, chs, wpn, arm, abl, rec);
        compared++;
        if (strstr(name_o, label_get(919))) {
            marked++;
        }
        int8_t off = Weapon[wpn].offense_rating, def = Armor[arm].defense_rating;
        uint8_t speed = Chassis[chs].speed;
        if (VehPrototypes[proto_id].ability_flags & ABL_POLICE_2X && Weapon[wpn].mode < 3
            && ((VehPrototypes[proto_id].unk_1 == 10 && !(*FactionsStatus & (1 << faction_id))
                && speed > 1) || (off <= speed && def <= speed && off >= 0
                && Chassis[chs].triad == TRIAD_LAND))) {
            police++; // the name loses its trailing space instead of getting a unit noun
        }
        if (strcmp(name_o, name_n)) {
            if (mismatches < MAX_DETAIL) {
                logf("  name_proto(%d, %d, %u, %u, %u, %X, %u): orig \"%s\" new \"%s\"", proto_id,
                    faction_id, chs, wpn, arm, abl, rec, name_o, name_n);
            }
            mismatches++;
        }
    };
    // every active prototype under its own parts, for its owner (predefined ones for everyone)
    for (int id = 0; id < MaxVehProtoNum; id++) {
        VehPrototype *p = &VehPrototypes[id];
        if (!(p->flags & PROTO_ACTIVE)) {
            continue;
        }
        int owner = id / MaxVehProtoFactionNum;
        for (int f = owner; f < (owner ? owner + 1 : MaxPlayerNum); f++) {
            check(id, f, p->chassis_id, p->weapon_id, p->armor_id, p->ability_flags, p->reactor_id);
        }
    }
    // a grid of parts in four slots per faction: two active designs of either parity, an empty
    // slot and a predefined one
    for (int f = 1; f < MaxPlayerNum; f++) {
        int slots[4] = { -1, -1, -1, BSC_SCOUT_PATROL };
        for (int i = 0; i < MaxVehProtoFactionNum; i++) {
            int id = f * MaxVehProtoFactionNum + i;
            if (VehPrototypes[id].flags & PROTO_ACTIVE) {
                if (slots[id & 1] < 0) {
                    slots[id & 1] = id;
                }
            } else if (slots[2] < 0) {
                slots[2] = id;
            }
        }
        for (int s = 0; s < 4; s++) {
            int slot = slots[s];
            if (slot < 0) {
                continue;
            }
            category_10 += VehPrototypes[slot].unk_1 == 10;
            police_slots += (VehPrototypes[slot].ability_flags & ABL_POLICE_2X) != 0;
            int k = 0;
            for (uint32_t chs = 0; chs < MaxChassisNum; chs++) {
                for (uint32_t wpn = 0; wpn < MaxWeaponNum; wpn++) {
                    if (!proto_test_plausible(chs, wpn)) {
                        continue;
                    }
                    for (uint32_t arm = 0; arm < MaxArmorNum; arm++) {
                        for (uint32_t rec = 1; rec <= MaxReactorNum; rec++, k++) {
                            check(slot, f, chs, wpn, arm, 0, rec);
                            check(slot, f, chs, wpn, arm,
                                ProtoTestAbilities[1 + k % (ProtoTestAbilityCount - 1)], rec);
                        }
                    }
                }
            }
        }
    }
    logf("name_proto: %d compared (%d with a Mk number, slots: %d of category 10, %d police; "
        "%d police names trimmed), %d mismatches", compared, marked, category_10, police_slots,
        police, mismatches);
    return mismatches;
}

static int test_propose_proto() {
    // selection globals touched by kill()/veh_kill(), same set as the stack_kill test
    static int *const extra_globals[] = { (int *)0x0093928C, (int *)0x00939290, (int *)0x008C6B2C,
        (int *)0x008C6B34, (int *)0x009A64DC };
    const int extra_count = (int)(sizeof(extra_globals) / sizeof(extra_globals[0]));
    int extra_saved[5], extra_o[5];
    for (int i = 0; i < extra_count; i++) {
        extra_saved[i] = *extra_globals[i];
    }
    const size_t bung_size = sizeof(BungedDesign) * MaxPlayerNum * MaxBungedDesignNum;
    const size_t proto_size = sizeof(VehPrototype) * MaxVehProtoNum;
    uint8_t *bung_saved = (uint8_t *)malloc(bung_size);
    uint8_t *bung_orig = (uint8_t *)malloc(bung_size);
    uint8_t *proto_saved = (uint8_t *)malloc(proto_size);
    uint8_t *proto_orig = (uint8_t *)malloc(proto_size);
    memcpy(bung_saved, BungList, bung_size);
    memcpy(proto_saved, VehPrototypes, proto_size);
    // DesignSuggestAbilities .. DesignSuggestState
    uint8_t *const suggest = (uint8_t *)DesignSuggestAbilities;
    const size_t suggest_size = 0x0093FC58 - 0x0093FC34;
    uint8_t suggest_saved[0x24], suggest_o[0x24];
    memcpy(suggest_saved, suggest, suggest_size);
    int new_design_saved = *ProtoIDNewDesign;
    uint8_t status_saved = *FactionsStatus;
    int diff_saved = *DiffLevelCurrent;
    BOOL expansion_saved = *ExpansionEnabled;
    BOOL draw_saved = VehBattleUnkTgl[0];
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    VehBattleUnkTgl[0] = true; // kill() skips its map redraw while this is set
    snap_take(&base);
    struct Design {
        uint32_t chs, wpn, arm, abl, rec;
        int category;
    };
    int compared = 0, mismatches = 0, rejected = 0, reused = 0, created = 0, retired = 0;
    int requeued = 0, suggested = 0;
    char label[128];
    // variant: 0 as loaded; 1-4 the design workshop's suggestion states (1 open, 1 with the
    // design's category, 2, 3); 5 the faction is a second human player (hotseat) who declined a
    // design; 6 Citizen difficulty; 7 no expansion; 8 a given name; 9 a name already taken
    auto run = [&](uint32_t f, const Design &d, int variant, int bunged_proto) {
        LPCSTR name = NULL;
        if (variant == 8) {
            name = "Proposal";
        } else if (variant == 9) {
            for (int i = 0; i < MaxVehProtoFactionNum && !name; i++) {
                int id = f * MaxVehProtoFactionNum + i;
                if (((VehPrototype *)proto_saved)[id].flags & PROTO_ACTIVE) {
                    name = ((VehPrototype *)proto_saved)[id].veh_name;
                }
            }
        }
        char name_copy[64];
        if (name) {
            strcpy_s(name_copy, name); // the slot holding the name may be overwritten
            name = name_copy;
        }
        auto prepare = [&]() {
            snap_restore(&base);
            memcpy(BungList, bung_saved, bung_size);
            memcpy(VehPrototypes, proto_saved, proto_size);
            memcpy(suggest, suggest_saved, suggest_size);
            *ProtoIDNewDesign = new_design_saved;
            *FactionsStatus = status_saved;
            *DiffLevelCurrent = diff_saved;
            *ExpansionEnabled = expansion_saved;
            for (int i = 0; i < extra_count; i++) {
                *extra_globals[i] = extra_saved[i];
            }
            if (variant >= 1 && variant <= 4) {
                *DesignSuggestState = variant <= 2 ? 1 : variant - 1;
                *DesignSuggestCategory = variant == 2 ? d.category : -1;
            } else if (variant == 5) {
                *FactionsStatus |= (uint8_t)(1 << f);
                if (bunged_proto >= 0) {
                    bung_proto(f, bunged_proto);
                }
            } else if (variant == 6) {
                *DiffLevelCurrent = 1;
            } else if (variant == 7) {
                *ExpansionEnabled = false;
            }
        };
        prepare();
        int ret_o = propose_proto_OG(f, d.chs, d.wpn, d.arm, d.abl, d.rec, d.category, name);
        snap_take(&after_orig);
        memcpy(bung_orig, BungList, bung_size);
        memcpy(proto_orig, VehPrototypes, proto_size);
        memcpy(suggest_o, suggest, suggest_size);
        int new_design_o = *ProtoIDNewDesign;
        for (int i = 0; i < extra_count; i++) {
            extra_o[i] = *extra_globals[i];
        }
        prepare();
        int ret_n = propose_proto(f, d.chs, d.wpn, d.arm, d.abl, d.rec, d.category, name);
        snap_take(&after_new);
        compared++;
        if (ret_o < 0) {
            rejected++;
        } else {
            VehPrototype *was = &((VehPrototype *)proto_saved)[ret_o];
            VehPrototype *now = &((VehPrototype *)proto_orig)[ret_o];
            BOOL same_parts = was->chassis_id == now->chassis_id && was->weapon_id == now->weapon_id
                && was->armor_id == now->armor_id && was->reactor_id == now->reactor_id
                && was->ability_flags == now->ability_flags;
            if (was->flags & PROTO_ACTIVE && same_parts) {
                reused++;
            } else {
                created++;
                retired += (was->flags & PROTO_ACTIVE) != 0;
            }
        }
        if (first_diff(after_orig.bases, base.bases, sizeof(Base) * base.base_count) != (size_t)-1) {
            requeued++;
        }
        if (memcmp(suggest_o, suggest_saved, suggest_size)) {
            suggested++;
        }
        sprintf_s(label, "propose_proto(%u, %u, %u, %u, %X, %u, %d) variant %d", f, d.chs, d.wpn,
            d.arm, d.abl, d.rec, d.category, variant);
        BOOL diff = false;
        if (ret_o != ret_n) {
            if (mismatches < MAX_DETAIL) {
                logf("  %s: orig returned %d, new %d", label, ret_o, ret_n);
            }
            diff = true;
        }
        if (!diff) {
            diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
        }
        size_t at;
        if (!diff && (at = first_diff(bung_orig, BungList, bung_size)) != (size_t)-1) {
            if (mismatches < MAX_DETAIL) {
                logf("  %s: BungList byte %u differs (orig %02X new %02X)", label, (unsigned)at,
                    bung_orig[at], ((uint8_t *)BungList)[at]);
            }
            diff = true;
        }
        if (!diff && (at = first_diff(proto_orig, VehPrototypes, proto_size)) != (size_t)-1) {
            if (mismatches < MAX_DETAIL) {
                logf("  %s: VehPrototype[%u] +0x%X differs (orig %02X new %02X)", label,
                    (unsigned)(at / sizeof(VehPrototype)), (unsigned)(at % sizeof(VehPrototype)),
                    proto_orig[at], ((uint8_t *)VehPrototypes)[at]);
            }
            diff = true;
        }
        if (!diff && (at = first_diff(suggest_o, suggest, suggest_size)) != (size_t)-1) {
            if (mismatches < MAX_DETAIL) {
                logf("  %s: design suggestion +0x%X differs", label, (unsigned)at);
            }
            diff = true;
        }
        if (!diff && new_design_o != *ProtoIDNewDesign) {
            if (mismatches < MAX_DETAIL) {
                logf("  %s: new design id orig %d new %d", label, new_design_o, *ProtoIDNewDesign);
            }
            diff = true;
        }
        for (int i = 0; i < extra_count && !diff; i++) {
            if (extra_o[i] != *extra_globals[i]) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: selection global %08X differs", label, (unsigned)extra_globals[i]);
                }
                diff = true;
            }
        }
        if (diff) {
            mismatches++;
        }
    };
    static const int categories[] = { -1, -2, 0, 3, 6, 8, 10, 13, 18, 23 };
    const int category_count = (int)(sizeof(categories) / sizeof(categories[0]));
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        if ((int)f == *FactionIDCurrent) {
            continue; // prune_protos() opens popups for the player at the controls
        }
        static Design designs[1600];
        int n = 0;
        int bunged_proto = -1;
        // every active design the faction can build: the same parts, as typed complete, with an
        // extra ability inside and outside the 0x44AE1B mask, and with each part one step off
        for (int i = 0; i < 2 * MaxVehProtoFactionNum && n < 1000; i++) {
            int id = i < MaxVehProtoFactionNum ? i : f * MaxVehProtoFactionNum + i - MaxVehProtoFactionNum;
            VehPrototype *p = &((VehPrototype *)proto_saved)[id];
            if (!(p->flags & PROTO_ACTIVE)
                || (id < MaxVehProtoFactionNum && !has_tech(p->preq_tech, f))) {
                continue;
            }
            if (id >= MaxVehProtoFactionNum && bunged_proto < 0) {
                bunged_proto = id;
            }
            Design same = { p->chassis_id, p->weapon_id, p->armor_id, p->ability_flags, p->reactor_id,
                (uint8_t)p->unk_1 };
            designs[n++] = same;
            same.category = -2;
            designs[n++] = same;
            Design d = same;
            d.category = -1;
            d.abl = same.abl | ABL_AAA;
            designs[n++] = d;
            d.abl = same.abl | ABL_DEEP_RADAR;
            designs[n++] = d;
            d = same;
            d.category = -1;
            if (d.wpn + 1 < MaxWeaponNum && proto_test_plausible(d.chs, d.wpn + 1)) {
                d.wpn++;
                designs[n++] = d;
                d.wpn--;
            }
            if (d.arm + 1 < MaxArmorNum) {
                d.arm++;
                designs[n++] = d;
                d.arm--;
            }
            if (d.rec < MaxReactorNum) {
                d.rec++;
                designs[n++] = d;
            }
        }
        // a sample of the part grid (the faction may lack the techs for some of it)
        int k = 0;
        for (uint32_t chs = 0; chs < MaxChassisNum; chs++) {
            for (uint32_t wpn = 0; wpn < MaxWeaponNum; wpn++) {
                if (!proto_test_plausible(chs, wpn)) {
                    continue;
                }
                for (uint32_t arm = 0; arm < MaxArmorNum; arm++) {
                    for (uint32_t rec = 1; rec <= MaxReactorNum; rec++, k++) {
                        if (k % 11 || n >= 1600) {
                            continue;
                        }
                        Design d = { chs, wpn, arm, ProtoTestAbilities[(k / 11) % ProtoTestAbilityCount],
                            rec, categories[(k / 11) % category_count] };
                        designs[n++] = d;
                    }
                }
            }
        }
        for (int i = 0; i < n; i++) {
            run(f, designs[i], 0, bunged_proto);
            if (i % 9 == 0) {
                for (int variant = 1; variant <= 9; variant++) {
                    run(f, designs[i], variant, bunged_proto);
                }
            }
        }
        // the hotseat human proposes the design it declined, and one it already has
        if (bunged_proto >= 0) {
            VehPrototype *p = &((VehPrototype *)proto_saved)[bunged_proto];
            Design d = { p->chassis_id, p->weapon_id, p->armor_id, p->ability_flags, p->reactor_id, -1 };
            run(f, d, 5, bunged_proto);
            run(f, d, 5, -1);
        }
    }
    snap_restore(&base);
    memcpy(BungList, bung_saved, bung_size);
    memcpy(VehPrototypes, proto_saved, proto_size);
    memcpy(suggest, suggest_saved, suggest_size);
    *ProtoIDNewDesign = new_design_saved;
    *FactionsStatus = status_saved;
    *DiffLevelCurrent = diff_saved;
    *ExpansionEnabled = expansion_saved;
    for (int i = 0; i < extra_count; i++) {
        *extra_globals[i] = extra_saved[i];
    }
    VehBattleUnkTgl[0] = draw_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    free(bung_saved); free(bung_orig); free(proto_saved); free(proto_orig);
    logf("propose_proto: %d compared (%d rejected or suggested, %d new designs, %d reused, "
        "%d slots retired, %d with queues updated, %d suggestions recorded), %d mismatches",
        compared, rejected, created, reused, retired, requeued, suggested, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- pointer-for-value fixes

static int test_strat() {
    // arm_strat and weap_strat are upstream redirects: run the exe's own code for comparison
    static const uint8_t strat_entry[6] = { 0x55, 0x8B, 0xEC, 0xA1, 0x88, 0x64 };
    OriginalEntry vanilla_arm(0x0057D270, strat_entry);
    OriginalEntry vanilla_weap(0x0057D2E0, strat_entry);
    if (!vanilla_arm.ok || !vanilla_weap.ok) {
        return 1;
    }
    func_uu *arm_strat_exe = (func_uu *)0x0057D270;
    func_uu *weap_strat_exe = (func_uu *)0x0057D2E0;
    BOOL expansion_saved = *ExpansionEnabled;
    int compared = 0, mismatches = 0, flat = 0;
    for (int expansion = 1; expansion >= 0; expansion--) {
        *ExpansionEnabled = expansion; // SMACX and SMAC mode
        for (uint32_t f = 0; f < MaxPlayerNum; f++) {
            for (uint32_t id = 0; id < MaxArmorNum; id++) {
                int a = arm_strat_exe(id, f);
                int b = arm_strat(id, f);
                compared++;
                flat += !expansion && id > ARM_PSI_DEFENSE;
                if (a != b) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  arm_strat(%u, %u) expansion %d: orig %d new %d", id, f, expansion, a, b);
                    }
                    mismatches++;
                }
            }
            for (uint32_t id = 0; id < MaxWeaponNum; id++) {
                int a = weap_strat_exe(id, f);
                int b = weap_strat(id, f);
                compared++;
                flat += !expansion && id >= WPN_RESONANCE_LASER && id <= WPN_STRING_DISRUPTOR;
                if (a != b) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  weap_strat(%u, %u) expansion %d: orig %d new %d", id, f, expansion, a, b);
                    }
                    mismatches++;
                }
            }
        }
    }
    *ExpansionEnabled = expansion_saved;
    logf("arm_strat/weap_strat: %d compared (%d SMAC-mode parts rated 1), %d mismatches", compared,
        flat, mismatches);
    return mismatches;
}

static int test_goody_at() {
    static const uint8_t goody_entry[6] = { 0x55, 0x8B, 0xEC, 0xA1, 0xF0, 0xFA };
    OriginalEntry vanilla_goody(0x00592140, goody_entry);
    if (!vanilla_goody.ok) {
        return 1;
    }
    func_uu *goody_at_exe = (func_uu *)0x00592140;
    uint32_t seed_saved = *MapRandSeed;
    int compared = 0, mismatches = 0, found = 0;
    for (int pass = 0; pass < 2; pass++) {
        *MapRandSeed = pass ? 0 : seed_saved; // the map's seed, and a map with seed 0
        for (uint32_t y = 0; y < (uint32_t)*MapLatitudeBounds; y++) {
            for (uint32_t x = y & 1; x < (uint32_t)*MapLongitudeBounds; x += 2) {
                uint32_t a = (uint32_t)goody_at_exe(x, y);
                uint32_t b = goody_at(x, y);
                compared++;
                found += a != 0;
                if (a != b) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  goody_at(%u, %u) seed %u: orig %u new %u", x, y, *MapRandSeed, a, b);
                    }
                    mismatches++;
                }
            }
        }
    }
    *MapRandSeed = seed_saved;
    logf("goody_at: %d compared (seed %u and 0; %d tiles with something by the original), "
        "%d mismatches", compared, seed_saved, found, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 24

static int test_can_terraform() {
    int compared = 0, mismatches = 0;
    int orders[32] = {};
    int need_states = 0;
    auto check = [&](int f, int x, int y, BOOL base_work, int base_id) {
        int a = can_terraform_OG(f, x, y, base_work, base_id);
        int b = can_terraform(f, x, y, base_work, base_id);
        compared++;
        orders[(a >= 0 && a < 32) ? a : 31]++;
        if (a != b) {
            if (mismatches < MAX_DETAIL) {
                logf("  can_terraform(%d, %d, %d, %d, %d): orig %d new %d", f, x, y, base_work,
                    base_id, a, b);
            }
            mismatches++;
        }
    };
    // every tile for every faction, without a base
    for (int f = 0; f < MaxPlayerNum; f++) {
        for (int y = 0; y < *MapLatitudeBounds; y++) {
            for (int x = y & 1; x < *MapLongitudeBounds; x += 2) {
                check(f, x, y, false, -1);
                check(f, x, y, true, -1);
            }
        }
    }
    // the tiles around every base, for its owner
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        int f = Bases[base_id].faction_id_current;
        need_states += (Bases[base_id].state & (BSTATE_UNK_1000 | BSTATE_UNK_2000
            | BSTATE_UNK_4000)) != 0;
        for (int i = 0; i < BaseRadiusTiles; i++) {
            int x = RadiusOffsetX[i] + Bases[base_id].x;
            if (!(*MapIsFlat & 1)) {
                if (x < 0) {
                    x += *MapLongitudeBounds;
                } else if (x >= *MapLongitudeBounds) {
                    x -= *MapLongitudeBounds;
                }
            }
            int y = RadiusOffsetY[i] + Bases[base_id].y;
            if (x < 0 || y < 0 || x >= *MapLongitudeBounds || y >= *MapLatitudeBounds) {
                continue;
            }
            check(f, x, y, false, base_id);
            check(f, x, y, true, base_id);
        }
    }
    logf("can_terraform: %d compared (orders by the original: none %d, farm %d, mine %d, solar %d, "
        "forest %d, remove fungus %d, plant fungus %d, raise %d; %d bases with a need flag), "
        "%d mismatches", compared, orders[0], orders[ORDER_FARM], orders[ORDER_MINE],
        orders[ORDER_SOLAR_COLLECTOR], orders[ORDER_PLANT_FOREST], orders[ORDER_REMOVE_FUNGUS],
        orders[ORDER_PLANT_FUNGUS], orders[ORDER_TERRAFORM_UP], need_states, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 25

static int test_balance() {
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    uint8_t globals_saved[BaseGlobalsSize], globals_o[BaseGlobalsSize];
    memcpy(globals_saved, BaseGlobals, BaseGlobalsSize);
    int compute_saved = *BaseComputeID;
    int diff_saved = *DiffLevelCurrent;
    BOOL draw_saved = VehBattleUnkTgl[0];
    int compared = 0, mismatches = 0, units = 0, pods = 0;
    char label[64];
    static Veh added[2048]; // Vehs holds at most 2048 units (veh_init stops at 0x7FF)
    // every difficulty level, a few random streams each
    for (int diff = 0; diff < 6; diff++) {
        for (uint32_t seed = 1; seed <= 4; seed++) {
            auto prepare = [&]() {
                snap_restore(&base);
                memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
                *BaseComputeID = compute_saved;
                *DiffLevelCurrent = diff;
                reseed(seed * 7919);
            };
            prepare();
            balance_OG();
            snap_take(&after_orig);
            memcpy(globals_o, BaseGlobals, BaseGlobalsSize);
            int compute_o = *BaseComputeID;
            units += after_orig.veh_count_global - base.veh_count_global;
            int added_o = after_orig.veh_count_global - base.veh_count_global;
            if (added_o > 0) {
                memcpy(added, &Vehs[base.veh_count_global], sizeof(Veh) * added_o);
            }
            for (uint32_t i = 0; i < base.tile_count; i++) {
                pods += ((after_orig.tiles[i].bit ^ base.tiles[i].bit)
                    & (BIT_UNK_4000000 | BIT_UNK_8000000)) != 0;
            }
            prepare();
            balance();
            snap_take(&after_new);
            compared++;
            sprintf_s(label, "balance() difficulty %d seed %u", diff, seed);
            BOOL diff_found = snap_differs(&after_orig, &after_new,
                mismatches < MAX_DETAIL ? label : NULL);
            if (!diff_found && base_globals_differ(globals_o, BaseGlobals,
                mismatches < MAX_DETAIL ? label : NULL)) {
                diff_found = true;
            }
            size_t at;
            if (!diff_found && added_o > 0 && after_new.veh_count_global == after_orig.veh_count_global
                && (at = first_diff(added, &Vehs[base.veh_count_global], sizeof(Veh) * added_o))
                    != (size_t)-1) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: added Veh[%u] +0x%X differs", label,
                        (unsigned)(base.veh_count_global + at / sizeof(Veh)), (unsigned)(at % sizeof(Veh)));
                }
                diff_found = true;
            }
            if (!diff_found && compute_o != *BaseComputeID) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: BaseComputeID orig %d new %d", label, compute_o, *BaseComputeID);
                }
                diff_found = true;
            }
            if (diff_found) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
    *BaseComputeID = compute_saved;
    *DiffLevelCurrent = diff_saved;
    VehBattleUnkTgl[0] = draw_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("balance: %d compared (%d colony pods and %d pod tiles placed by the original), "
        "%d mismatches", compared, units, pods, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- is_human() normalization

static int test_tech_val() {
    // tech_val is an upstream redirect: run the exe's own code for comparison
    static const uint8_t tech_val_entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x20 };
    OriginalEntry vanilla(0x005BCBE0, tech_val_entry);
    if (!vanilla.ok) {
        return 1;
    }
    typedef int func_tech_val(uint32_t, int, BOOL);
    func_tech_val *tech_val_exe = (func_tech_val *)0x005BCBE0;
    uint8_t status_saved = *FactionsStatus;
    int compared = 0, mismatches = 0, human_runs = 0;
    // as loaded, and every faction played by a human (hotseat)
    for (int hotseat = 0; hotseat < 2; hotseat++) {
        *FactionsStatus = hotseat ? (uint8_t)(status_saved | 0xFE) : status_saved;
        for (int f = 1; f < MaxPlayerNum; f++) {
            for (uint32_t id = 0; id < 97 + 2 * MaxVehProtoFactionNum; id++) {
                if (id >= 97) {
                    int proto_id = id - 97 < MaxVehProtoFactionNum ? id - 97
                        : f * MaxVehProtoFactionNum + (id - 97 - MaxVehProtoFactionNum);
                    if (!(VehPrototypes[proto_id].flags & PROTO_ACTIVE)) {
                        continue;
                    }
                }
                uint32_t tech_id = id;
                if (id >= 97 + MaxVehProtoFactionNum) {
                    tech_id = 97 + f * MaxVehProtoFactionNum + (id - 97 - MaxVehProtoFactionNum);
                }
                for (int simple = 0; simple < 2; simple++) {
                    int a = tech_val_exe(tech_id, f, simple);
                    int b = tech_val(tech_id, f, simple);
                    compared++;
                    human_runs += is_human(f) != 0;
                    if (a != b) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  tech_val(%u, %d, %d) hotseat %d: orig %d new %d", tech_id, f,
                                simple, hotseat, a, b);
                        }
                        mismatches++;
                    }
                }
            }
        }
    }
    *FactionsStatus = status_saved;
    logf("tech_val: %d compared (%d for human factions), %d mismatches", compared, human_runs,
        mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 26

// Put a jmp to target over an exe function's first five bytes while this lives.
struct JmpStub {
    uint8_t *entry;
    uint8_t saved_code[5];
    DWORD old_protect;
    BOOL ok;
    JmpStub(uint32_t address, void *target) : entry((uint8_t *)address), old_protect(0), ok(false) {
        uint8_t stub[5] = { 0xE9, 0, 0, 0, 0 };
        int32_t rel = (int32_t)((uint8_t *)target - (entry + 5));
        memcpy(stub + 1, &rel, 4);
        if (!VirtualProtect(entry, sizeof(stub), PAGE_EXECUTE_READWRITE, &old_protect)) {
            logf("cannot unprotect %08X (%u)", address, GetLastError());
            return;
        }
        memcpy(saved_code, entry, sizeof(stub));
        memcpy(entry, stub, sizeof(stub));
        FlushInstructionCache(GetCurrentProcess(), entry, sizeof(stub));
        ok = true;
    }
    ~JmpStub() {
        if (ok) {
            memcpy(entry, saved_code, sizeof(saved_code));
            FlushInstructionCache(GetCurrentProcess(), entry, sizeof(saved_code));
            VirtualProtect(entry, sizeof(saved_code), old_protect, &old_protect);
        }
    }
};

// popp() is a dialog: record what it was asked and answer with PoppAnswer
static int PoppAnswer;
static int PoppCount;
static char PoppLabel[32];
static char PoppPic[32];
static int __cdecl popp_recorder(LPCSTR file, LPCSTR label, int unk, LPCSTR pic,
                                 int(__cdecl *callback)()) {
    PoppCount++;
    strncpy_s(PoppLabel, label ? label : "", _TRUNCATE);
    strncpy_s(PoppPic, pic ? pic : "", _TRUNCATE);
    return PoppAnswer;
}

static int test_upgrade_prototype() {
    JmpStub popp_stub(0x0048C0A0, (void *)popp_recorder);
    if (!popp_stub.ok) {
        return 1;
    }
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    const size_t parse_size = 2 * sizeof(char256);
    static uint8_t temp_saved[sizeof(char1032)], temp_o[sizeof(char1032)];
    static uint8_t parse_saved[2 * sizeof(char256)], parse_o[2 * sizeof(char256)];
    int nums_saved[3], nums_o[3];
    memcpy(temp_saved, stringTemp, sizeof(char1032));
    memcpy(parse_saved, ParseStrBuffer, parse_size);
    memcpy(nums_saved, ParseNumTable, sizeof(nums_saved));
    int upgrade_unk_saved = *VehUpgradeUnk;
    const size_t proto_size = sizeof(VehPrototype) * MaxVehProtoNum;
    uint8_t *proto_saved = (uint8_t *)malloc(proto_size);
    uint8_t *proto_o = (uint8_t *)malloc(proto_size);
    memcpy(proto_saved, VehPrototypes, proto_size);
    uint32_t warnings_saved = *GameWarnings;
    uint8_t status_saved = *FactionsStatus;
    int compared = 0, mismatches = 0, upgraded = 0, asked = 0, refused = 0;
    char label[96];
    for (int f = 1; f < MaxPlayerNum; f++) {
        BOOL player = f == *FactionIDCurrent;
        int pair = 0;
        for (int i = 0; i < 2 * MaxVehProtoFactionNum; i++) {
            int old_id = i < MaxVehProtoFactionNum ? i : f * MaxVehProtoFactionNum + i - MaxVehProtoFactionNum;
            VehPrototype *protos = (VehPrototype *)proto_saved; // as loaded
            if (!(protos[old_id].flags & PROTO_ACTIVE) || !base.players[f].proto_id_active[old_id]) {
                continue;
            }
            for (int j = 0; j < MaxVehProtoFactionNum; j++) {
                int new_id = f * MaxVehProtoFactionNum + j;
                if (new_id == old_id || !(protos[new_id].flags & PROTO_ACTIVE)
                    || Chassis[protos[new_id].chassis_id].triad
                    != Chassis[protos[old_id].chassis_id].triad) {
                    continue;
                }
                pair++;
                // AI: quiet and not. The player: every answer, asked before or not, quiet with
                // and without the warning, every 3rd pair. Also once as a hotseat human.
                int variants = player ? ((pair % 3) ? 0 : 24) : 3;
                for (int v = 0; v < variants; v++) {
                    BOOL quiet = v & 1;
                    auto prepare = [&]() {
                        snap_restore(&base);
                        memcpy(VehPrototypes, proto_saved, proto_size);
                        memcpy(stringTemp, temp_saved, sizeof(char1032));
                        memcpy(ParseStrBuffer, parse_saved, parse_size);
                        memcpy(ParseNumTable, nums_saved, sizeof(nums_saved));
                        *VehUpgradeUnk = player ? (v >> 1) & 1 : upgrade_unk_saved;
                        *GameWarnings = (player && (v >> 2) & 1) ? (warnings_saved ^ 4)
                            : warnings_saved;
                        *FactionsStatus = (!player && v == 2) ? (uint8_t)(status_saved | (1 << f))
                            : status_saved;
                        PoppAnswer = player ? (v >> 3) % 3 : 0;
                        PoppCount = 0;
                        PoppLabel[0] = PoppPic[0] = 0;
                    };
                    prepare();
                    BOOL ret_o = upgrade_prototype_OG(f, new_id, old_id, quiet);
                    snap_take(&after_orig);
                    memcpy(temp_o, stringTemp, sizeof(char1032));
                    memcpy(proto_o, VehPrototypes, proto_size);
                    memcpy(parse_o, ParseStrBuffer, parse_size);
                    memcpy(nums_o, ParseNumTable, sizeof(nums_o));
                    int unk_o = *VehUpgradeUnk;
                    int count_o = PoppCount;
                    char label_o[32], pic_o[32];
                    strcpy_s(label_o, PoppLabel);
                    strcpy_s(pic_o, PoppPic);
                    prepare();
                    BOOL ret_n = upgrade_prototype(f, new_id, old_id, quiet);
                    snap_take(&after_new);
                    compared++;
                    upgraded += ret_o != 0;
                    asked += count_o;
                    refused += count_o && !ret_o;
                    sprintf_s(label, "upgrade_prototype(%d, %d, %d, %d) variant %d", f, new_id,
                        old_id, quiet, v);
                    BOOL diff = false;
                    if ((ret_o != 0) != (ret_n != 0) || count_o != PoppCount
                        || strcmp(label_o, PoppLabel) || strcmp(pic_o, PoppPic)
                        || unk_o != *VehUpgradeUnk) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  %s: orig %d popp %d '%s' '%s' unk %d; new %d popp %d '%s' '%s' unk %d",
                                label, ret_o, count_o, label_o, pic_o, unk_o, ret_n, PoppCount,
                                PoppLabel, PoppPic, *VehUpgradeUnk);
                        }
                        diff = true;
                    }
                    if (!diff) {
                        diff = snap_differs(&after_orig, &after_new,
                            mismatches < MAX_DETAIL ? label : NULL);
                    }
                    size_t at;
                    if (!diff && (at = first_diff(proto_o, VehPrototypes, proto_size)) != (size_t)-1) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  %s: VehPrototype[%u] +0x%X differs", label,
                                (unsigned)(at / sizeof(VehPrototype)), (unsigned)(at % sizeof(VehPrototype)));
                        }
                        diff = true;
                    }
                    if (!diff && (memcmp(temp_o, stringTemp, sizeof(char1032))
                        || memcmp(parse_o, ParseStrBuffer, parse_size)
                        || memcmp(nums_o, ParseNumTable, sizeof(nums_o)))) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  %s: popup text differs: orig '%s' new '%s'", label,
                                ((char256 *)parse_o)[1].str, ParseStrBuffer[1].str);
                        }
                        diff = true;
                    }
                    if (diff) {
                        mismatches++;
                    }
                }
            }
        }
    }
    snap_restore(&base);
    memcpy(VehPrototypes, proto_saved, proto_size);
    free(proto_saved);
    free(proto_o);
    memcpy(stringTemp, temp_saved, sizeof(char1032));
    memcpy(ParseStrBuffer, parse_saved, parse_size);
    memcpy(ParseNumTable, nums_saved, sizeof(nums_saved));
    *VehUpgradeUnk = upgrade_unk_saved;
    *GameWarnings = warnings_saved;
    *FactionsStatus = status_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("upgrade_prototype: %d compared (%d upgraded by the original, %d popups, %d refused "
        "after a popup), %d mismatches", compared, upgraded, asked, refused, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 27

/*
Tiles to create one faction's units on: a few of its bases (homing), its target tile and tiles of
the target's region (assignment to the target), and a spread over the whole map.
*/
static int veh_init_tiles(int faction_id, int (*tiles)[2], int max) {
    int n = 0;
    for (int base_id = 0; base_id < *BaseCurrentCount && n < 4; base_id++) {
        if (Bases[base_id].faction_id_current == faction_id) {
            tiles[n][0] = Bases[base_id].x;
            tiles[n][1] = Bases[base_id].y;
            n++;
        }
    }
    PlayerData *player = &PlayersData[faction_id];
    if (player->x_target >= 0) {
        tiles[n][0] = player->x_target;
        tiles[n][1] = player->y_target;
        n++;
        uint32_t region = region_at(player->x_target, player->y_target);
        int matches = 0;
        for (uint32_t i = 0; i < *MapArea; i++) {
            matches += (*MapTiles)[i].region == region;
        }
        for (int k = 0; k < 6 && matches && n < max; k++) {
            int want = k * matches / 6;
            int seen = 0;
            for (uint32_t i = 0; i < *MapArea; i++) {
                if ((*MapTiles)[i].region == region && seen++ == want) {
                    tiles[n][1] = i / *MapLongitude;
                    tiles[n][0] = (i % *MapLongitude) * 2 + (tiles[n][1] & 1);
                    n++;
                    break;
                }
            }
        }
    }
    for (int k = 0; k < 6 && n < max; k++) {
        sample_tile(k, 6, &tiles[n][0], &tiles[n][1]);
        n++;
    }
    return n;
}

// spot_all() hands first contacts to encounter() (diplomacy dialogs with the player) and the
// player's first alien sightings to interlude(): record those calls instead of making them
struct SpotCall { int fn, args[6]; };
static SpotCall SpotCalls[32];
static int SpotCallCount;
static void spot_call_record(int fn, int a0, int a1, int a2, int a3, int a4, int a5) {
    if (SpotCallCount < 32) {
        SpotCall c = { fn, { a0, a1, a2, a3, a4, a5 } };
        SpotCalls[SpotCallCount] = c;
    }
    SpotCallCount++;
}
static void __cdecl encounter_recorder(int a0, int a1, int a2, int a3, int a4, int a5) {
    spot_call_record(1, a0, a1, a2, a3, a4, a5);
}
static void __cdecl interlude_recorder(int a0, int a1, int a2, int a3) {
    spot_call_record(2, a0, a1, a2, a3, 0, 0);
}

static int test_veh_init() {
    JmpStub encounter_stub(0x0055E370, (void *)encounter_recorder);
    JmpStub interlude_stub(0x005230E0, (void *)interlude_recorder);
    if (!encounter_stub.ok || !interlude_stub.ok) {
        return 1;
    }
    SpotCall calls_o[32];
    int call_count_o = 0, encounters = 0;
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    const int count = base.veh_count_global;
    if (count >= MaxVehNum) {
        logf("veh_init: unit table full, skipped");
        snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
        return 0;
    }
    Veh slot_saved = Vehs[count]; // the slot the new unit goes into
    Veh slot_o;
    int compute_saved = *BaseComputeID;
    int compared = 0, mismatches = 0, homed = 0, objectives = 0, assigned = 0, left_out = 0;
    char label[96];
    // variant 0: as loaded; 1: the faction's target counters as reset by the AI strategy pass;
    // 2: artifacts are scenario objectives (alien artifacts only)
    auto run = [&](BOOL orig, int proto_id, int f, int x, int y, int variant, int *ret, int *rand_next) {
        snap_restore(&base);
        Vehs[count] = slot_saved;
        *BaseComputeID = compute_saved;
        if (variant == 1) {
            PlayersData[f].unk_71 = 0;
            PlayersData[f].unk_72 = 0;
        } else if (variant == 2) {
            *GameState |= STATE_SCN_VICT_ALL_ARTIFACTS_OBJ_UNIT;
        } else if (variant == 3) {
            *VehCurrentCount = MaxVehNum;
        }
        reseed((uint32_t)(proto_id * 131 + x * 7 + y + variant));
        SpotCallCount = 0;
        *ret = orig ? veh_init_OG(proto_id, f, x, y) : veh_init(proto_id, f, x, y);
        *rand_next = _rand(); // same position in the random stream?
    };
    static int tiles[32][2];
    for (int f = 0; f < MaxPlayerNum; f++) {
        int tile_count = veh_init_tiles(f, tiles, 32);
        for (int i = 0; i < 2 * MaxVehProtoFactionNum; i++) {
            if (!f && i >= MaxVehProtoFactionNum) {
                break;
            }
            int proto_id = i < MaxVehProtoFactionNum ? i : f * MaxVehProtoFactionNum + i - MaxVehProtoFactionNum;
            if (!(VehPrototypes[proto_id].flags & PROTO_ACTIVE)) {
                continue;
            }
            int variants = VehPrototypes[proto_id].plan == PLAN_ALIEN_ARTIFACT ? 3 : 2;
            for (int t = 0; t < tile_count; t++) {
                int x = tiles[t][0], y = tiles[t][1];
                for (int v = 0; v < variants; v++) {
                    if (v == 2 && *GameState & STATE_SCN_VICT_ALL_ARTIFACTS_OBJ_UNIT) {
                        continue;
                    }
                    int ret_o, ret_n, rand_o, rand_n;
                    REGRESS_CTX("veh_init(%d, %d, %d, %d) variant %d", proto_id, f, x, y, v);
                    run(true, proto_id, f, x, y, v, &ret_o, &rand_o);
                    snap_take(&after_orig);
                    slot_o = Vehs[count];
                    int compute_o = *BaseComputeID;
                    call_count_o = SpotCallCount;
                    memcpy(calls_o, SpotCalls, sizeof(calls_o));
                    encounters += call_count_o;
                    run(false, proto_id, f, x, y, v, &ret_n, &rand_n);
                    snap_take(&after_new);
                    compared++;
                    if (ret_o >= 0) {
                        homed += slot_o.home_base_id >= 0;
                        objectives += (slot_o.flags & VFLAG_IS_OBJECTIVE) != 0;
                        assigned += (slot_o.state & VSTATE_UNK_40000) != 0;
                        left_out += (slot_o.state & VSTATE_UNK_20000) != 0;
                    }
                    sprintf_s(label, "veh_init(%d, %d, %d, %d) variant %d", proto_id, f, x, y, v);
                    BOOL diff = false;
                    int recorded = call_count_o < 32 ? call_count_o : 32;
                    if (ret_o != ret_n || rand_o != rand_n || compute_o != *BaseComputeID
                        || call_count_o != SpotCallCount
                        || memcmp(calls_o, SpotCalls, sizeof(SpotCall) * recorded)) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  %s: orig %d rand %d compute %d contacts %d; new %d rand %d "
                                "compute %d contacts %d", label, ret_o, rand_o, compute_o,
                                call_count_o, ret_n, rand_n, *BaseComputeID, SpotCallCount);
                        }
                        diff = true;
                    }
                    if (!diff) {
                        diff = snap_differs(&after_orig, &after_new,
                            mismatches < MAX_DETAIL ? label : NULL);
                    }
                    size_t at;
                    if (!diff && ret_o >= 0
                        && (at = first_diff(&slot_o, &Vehs[count], sizeof(Veh))) != (size_t)-1) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  %s: new Veh +0x%X differs (orig %02X new %02X)", label, (unsigned)at,
                                ((uint8_t *)&slot_o)[at], ((uint8_t *)&Vehs[count])[at]);
                        }
                        diff = true;
                    }
                    if (diff) {
                        mismatches++;
                    }
                }
            }
        }
    }
    // a full unit table
    {
        int ret_o, ret_n, rand_o, rand_n;
        run(true, 0, 1, 0, 0, 3, &ret_o, &rand_o);
        snap_take(&after_orig);
        run(false, 0, 1, 0, 0, 3, &ret_n, &rand_n);
        snap_take(&after_new);
        compared++;
        if (ret_o != ret_n || rand_o != rand_n
            || snap_differs(&after_orig, &after_new, "veh_init() full table")) {
            logf("  veh_init() full table: orig %d new %d", ret_o, ret_n);
            mismatches++;
        }
    }
    snap_restore(&base);
    Vehs[count] = slot_saved;
    *BaseComputeID = compute_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("veh_init: %d compared (by the original: %d homed, %d objectives, %d assigned to the "
        "target, %d left out, %d contact/interlude calls), %d mismatches", compared, homed,
        objectives, assigned, left_out, encounters, mismatches);
    return mismatches;
}

static int test_alien_start() {
    JmpStub popp_stub(0x0048C0A0, (void *)popp_recorder); // base_kill can report a foiled corner
    if (!popp_stub.ok) {
        return 1;
    }
    // selection globals touched by veh_kill(), same set as the stack_kill test
    static int *const extra_globals[] = { (int *)0x0093928C, (int *)0x00939290, (int *)0x008C6B2C,
        (int *)0x008C6B34, (int *)0x009A64DC };
    const int extra_count = (int)(sizeof(extra_globals) / sizeof(extra_globals[0]));
    int extra_saved[5], extra_o[5];
    for (int i = 0; i < extra_count; i++) {
        extra_saved[i] = *extra_globals[i];
    }
    // base_kill() appends to the replay record (8 byte entries at 0x950324, offset 0x9469F8)
    int *const replay_offset = (int *)0x009469F8;
    uint8_t *const replay = (uint8_t *)0x00950324;
    int replay_saved = *replay_offset;
    static uint8_t replay_o[0x800];
    int sp_saved[MaxSecretProjectNum], sp_o[MaxSecretProjectNum];
    memcpy(sp_saved, SecretProjectTable, sizeof(sp_saved));
    uint8_t globals_saved[BaseGlobalsSize], globals_o[BaseGlobalsSize];
    memcpy(globals_saved, BaseGlobals, BaseGlobalsSize);
    static uint8_t temp_saved[sizeof(char1032)], temp_o[sizeof(char1032)];
    memcpy(temp_saved, stringTemp, sizeof(char1032));
    uint8_t status_saved[2] = { FactionsStatus[0], FactionsStatus[1] };
    int turn_saved = *TurnCurrentNum;
    int current_saved = *FactionIDCurrent;
    int compute_saved = *BaseComputeID;
    BOOL pbem_saved = *IsMultiplayerPBEM;
    BOOL net_saved = *IsMultiplayerNet;
    BOOL expansion_saved = *ExpansionEnabled;
    BOOL draw_saved = VehBattleUnkTgl[0];
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    int compared = 0, mismatches = 0, removed = 0;
    char label[96];
    // variant 0: new game (turn 0), each faction as the player; 1: as loaded (turn > 0);
    // 2: hotseat/PBEM; 3: network game; 4: SMAC (no expansion); 5: every faction human
    auto prepare = [&](int player, int variant) {
        snap_restore(&base);
        memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
        memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
        memcpy(stringTemp, temp_saved, sizeof(char1032));
        for (int i = 0; i < extra_count; i++) {
            *extra_globals[i] = extra_saved[i];
        }
        *replay_offset = replay_saved;
        *BaseComputeID = compute_saved;
        *TurnCurrentNum = variant == 1 ? turn_saved : 0;
        *FactionIDCurrent = player;
        FactionsStatus[0] = variant == 1 ? status_saved[0]
            : (uint8_t)((status_saved[0] & 1) | (variant == 5 ? 0xFE : 1 << player));
        FactionsStatus[1] = status_saved[1];
        *IsMultiplayerPBEM = variant == 2;
        *IsMultiplayerNet = variant == 3;
        *ExpansionEnabled = variant != 4;
        VehBattleUnkTgl[0] = true; // kill() skips its map redraw while this is set
        PoppAnswer = 0;
        PoppCount = 0;
    };
    for (int player = 1; player < MaxPlayerNum; player++) {
        for (int v = 0; v < 6; v++) {
            if (v && player != current_saved) {
                continue;
            }
            REGRESS_CTX("alien_start() player %d variant %d", player, v);
            prepare(player, v);
            alien_start_OG();
            snap_take(&after_orig);
            memcpy(sp_o, SecretProjectTable, sizeof(sp_o));
            memcpy(globals_o, BaseGlobals, BaseGlobalsSize);
            memcpy(temp_o, stringTemp, sizeof(char1032));
            for (int i = 0; i < extra_count; i++) {
                extra_o[i] = *extra_globals[i];
            }
            uint8_t status_o[2] = { FactionsStatus[0], FactionsStatus[1] };
            int replay_end_o = *replay_offset;
            int replay_len = replay_end_o - replay_saved;
            if (replay_len > 0 && replay_len <= (int)sizeof(replay_o)) {
                memcpy(replay_o, replay + replay_saved, replay_len);
            }
            int popp_o = PoppCount;
            int compute_o = *BaseComputeID;
            removed += base.base_count_global - after_orig.base_count_global;
            prepare(player, v);
            alien_start();
            snap_take(&after_new);
            compared++;
            sprintf_s(label, "alien_start() player %d variant %d", player, v);
            BOOL diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
            if (!diff && (memcmp(sp_o, SecretProjectTable, sizeof(sp_o))
                || memcmp(status_o, FactionsStatus, 2) || popp_o != PoppCount
                || compute_o != *BaseComputeID || replay_end_o != *replay_offset
                || (replay_len > 0 && replay_len <= (int)sizeof(replay_o)
                    && memcmp(replay_o, replay + replay_saved, replay_len)))) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: projects/status/popups/replay differ (status %02X%02X/%02X%02X "
                        "popp %d/%d replay %d/%d)", label, status_o[0], status_o[1],
                        FactionsStatus[0], FactionsStatus[1], popp_o, PoppCount, replay_end_o,
                        *replay_offset);
                }
                diff = true;
            }
            if (!diff && base_globals_differ(globals_o, BaseGlobals, mismatches < MAX_DETAIL ? label : NULL)) {
                diff = true;
            }
            if (!diff && memcmp(temp_o, stringTemp, sizeof(char1032))) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: stringTemp orig '%s' new '%s'", label, (char *)temp_o, stringTemp->str);
                }
                diff = true;
            }
            for (int i = 0; i < extra_count && !diff; i++) {
                if (extra_o[i] != *extra_globals[i]) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  %s: selection global %08X differs", label, (unsigned)extra_globals[i]);
                    }
                    diff = true;
                }
            }
            if (diff) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
    memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
    memcpy(stringTemp, temp_saved, sizeof(char1032));
    for (int i = 0; i < extra_count; i++) {
        *extra_globals[i] = extra_saved[i];
    }
    *replay_offset = replay_saved;
    *BaseComputeID = compute_saved;
    *TurnCurrentNum = turn_saved;
    *FactionIDCurrent = current_saved;
    FactionsStatus[0] = status_saved[0];
    FactionsStatus[1] = status_saved[1];
    *IsMultiplayerPBEM = pbem_saved;
    *IsMultiplayerNet = net_saved;
    *ExpansionEnabled = expansion_saved;
    VehBattleUnkTgl[0] = draw_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("alien_start: %d compared (%d bases removed by the original), %d mismatches", compared,
        removed, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 28: diplomacy notices

// The notices end in popups, and the exe functions around them open dialogs or read the
// keyboard: record each call (function, texts, arguments) and answer with NoticeAnswer[fn].
enum NoticeFn {
    NF_NETMSG_POP, NF_POPP, NF_X_POPS_11, NF_X_POP_2, NF_TUT_CHECK2, NF_COMMLINK,
    NF_DEMAND_WITHDRAWAL, NF_PACT_ENDS, NF_X_POP, NF_WANTS_TO_SPEAK, NF_TECH_ACHIEVED,
    NF_DOUBLE_CROSS, NF_CONSOLE_UPDATE, NF_CONSOLE_SET_MOVE, NF_CAPTION, NF_HELP_TOPIC,
    NF_DRAW_TILE, NF_CONSOLE_FOCUS, NF_ATROCITY, NF_BOOM, NF_ACT_TERRAFORM, NF_ACT_GO_TO,
    NF_ACT_ROAD_TO, NF_ACT_DESTROY, NF_BATTLE_FIGHT_2, NF_DRAW_MAP, NF_REPORT_REDRAW, NF_MONUMENT,
    NF_X_POPS, NF_TECH_PICK, NF_NET_TASKS, NF_NETMSG_CLOSE, NF_NET_SYNCH, NF_BASE_KILL, NF_FX_PLAY,
    NF_FX_STOP, NF_PREFS_SAVE, NF_COMMUNICATE, NF_ENCOUNTER, NF_NET_SEND, NF_AWAIT_DIPLO,
    NF_COUNT
};
static const char *const NoticeFnName[NF_COUNT] = { "NetMsg_pop", "popp", "X_pops_11", "X_pop_2",
    "tut_check2", "commlink_attempter", "demand_withdrawal", "pact_ends", "X_pop", "wants_to_speak",
    "tech_achieved", "double_cross", "Console::update_data", "Console::set_move",
    "diplomacy_caption", "help_topic", "draw_tile", "Console::focus", "atrocity", "boom",
    "action_terraform", "action_go_to", "action_road_to", "action_destroy", "battle_fight_2",
    "draw_map", "ReportIf::on_redraw", "monument", "X_pops", "tech_pick",
    "NetDaemon::net_tasks", "NetMsg::close", "NetDaemon::synch", "base_kill", "FX::play", "FX::stop",
    "prefs_save", "communicate", "encounter", "NetDaemon::send_message", "NetDaemon::await_diplo" };
struct NoticeCall {
    int fn;
    char label[32];
    char pic[32];
    char file[16];
    int args[3];
};
const int MaxNoticeCalls = 32;
static NoticeCall NoticeCalls[MaxNoticeCalls];
static int NoticeCallCount;
static int NoticeAnswer[NF_COUNT];

static int notice_record(int fn, LPCSTR label, LPCSTR pic, LPCSTR file, int a0, int a1, int a2) {
    if (NoticeCallCount < MaxNoticeCalls) {
        NoticeCall *call = &NoticeCalls[NoticeCallCount];
        memset(call, 0, sizeof(*call));
        call->fn = fn;
        strncpy_s(call->label, label ? label : "", _TRUNCATE);
        strncpy_s(call->pic, pic ? pic : "", _TRUNCATE);
        strncpy_s(call->file, file ? file : "", _TRUNCATE);
        call->args[0] = a0;
        call->args[1] = a1;
        call->args[2] = a2;
    }
    NoticeCallCount++;
    return NoticeAnswer[fn];
}

// The exe passes its own pop_wait (0x5398E0, which jumps into the DLL), the DLL its export.
static int notice_callback_id(int(__cdecl *callback)()) {
    if (!callback) {
        return 0;
    }
    return (callback == pop_wait || (uint32_t)callback == 0x005398E0) ? 1 : 2;
}
static int __fastcall notice_netmsg_pop(void *This, void *, LPCSTR label, int delay, int flags,
                                        LPCSTR pcx_file) {
    return notice_record(NF_NETMSG_POP, label, pcx_file, NULL, This == NetMsgVar, delay, flags);
}
static int __cdecl notice_popp(LPCSTR file, LPCSTR label, int unk, LPCSTR pic,
                               int(__cdecl *callback)()) {
    return notice_record(NF_POPP, label, pic, file, unk, notice_callback_id(callback), 0);
}
// When set, X_pops_11's answer to its next call only (energy_trade() asks again after help).
static int NoticeFirstAnswer = -1;
static int __cdecl notice_x_pops_11(LPCSTR label, int flags, Sprite *picture,
                                    int(__cdecl *callback)()) {
    int answer = notice_record(NF_X_POPS_11, label, NULL, NULL, flags, (int)picture,
        notice_callback_id(callback));
    if (NoticeFirstAnswer >= 0) {
        answer = NoticeFirstAnswer;
        NoticeFirstAnswer = -1;
    }
    return answer;
}
static int __cdecl notice_x_pop_2(LPCSTR file, LPCSTR label, int(__cdecl *callback)()) {
    return notice_record(NF_X_POP_2, label, NULL, file, notice_callback_id(callback), 0, 0);
}
static int __cdecl notice_tut_check2(int flag) {
    return notice_record(NF_TUT_CHECK2, NULL, NULL, NULL, flag, 0, 0);
}
static int __cdecl notice_commlink(int faction_id, int faction_id_with) {
    return notice_record(NF_COMMLINK, NULL, NULL, NULL, faction_id, faction_id_with, 0);
}
static void __cdecl notice_pact_ends(int faction_id, int faction_id_with) {
    notice_record(NF_PACT_ENDS, NULL, NULL, NULL, faction_id, faction_id_with, 0);
}

// demand_withdrawal() is in the DLL since batch 29: both sides of the demands_withdrawal test now
// reach the real one (the exe entry is redirected), so it is no longer stubbed here.
struct NoticeStubs {
    JmpStub netmsg_pop_stub, popp_stub, x_pops_11_stub, x_pop_2_stub, tut_check2_stub,
        commlink_stub, pact_ends_stub;
    NoticeStubs()
        : netmsg_pop_stub(0x0047A890, (void *)notice_netmsg_pop),
          popp_stub(0x0048C0A0, (void *)notice_popp),
          x_pops_11_stub(0x005BF7D0, (void *)notice_x_pops_11),
          x_pop_2_stub(0x005BF330, (void *)notice_x_pop_2),
          tut_check2_stub(0x004BA650, (void *)notice_tut_check2),
          commlink_stub(0x005589E0, (void *)notice_commlink),
          pact_ends_stub(0x0053C650, (void *)notice_pact_ends) {}
    BOOL ok() const {
        return netmsg_pop_stub.ok && popp_stub.ok && x_pops_11_stub.ok && x_pop_2_stub.ok
            && tut_check2_stub.ok && commlink_stub.ok && pact_ends_stub.ok;
    }
};

// Diplomacy dialog globals: DiploSecondFaction, DiploUnk1, DiploFriction, the values
// diplomacy_check() leaves (0x93F7B8..0x93FAA0) and the tech_analysis() results.
static uint8_t *const DiploGlobals = (uint8_t *)0x0093F678;
const size_t DiploGlobalsSize = 0x0093FAE0 - 0x0093F678;

// What a notice can change besides the game state in GameSnapshot.
struct NoticeState {
    char256 parse[10];
    int parse_gender[10];
    int parse_plural[10];
    int gender_default;
    BOOL plural_default;
    char1032 temp;
    char pronoun[64];
    uint8_t diplo[DiploGlobalsSize];
    int multiwin_flag;
    uint32_t seed;
    NoticeCall calls[MaxNoticeCalls];
    int call_count;
    int ret;
};

static void notice_take(NoticeState *s, int ret) {
    memcpy(s->parse, ParseStrBuffer, sizeof(s->parse));
    memcpy(s->parse_gender, ParseStrGender, sizeof(s->parse_gender));
    memcpy(s->parse_plural, ParseStrPlurality, sizeof(s->parse_plural));
    s->gender_default = *GenderDefault;
    s->plural_default = *PluralityDefault;
    memcpy(&s->temp, stringTemp, sizeof(s->temp));
    memcpy(s->pronoun, PronounBuffer, sizeof(s->pronoun));
    memcpy(s->diplo, DiploGlobals, sizeof(s->diplo));
    s->multiwin_flag = *MultiWinFlag;
    s->seed = random_get();
    memcpy(s->calls, NoticeCalls, sizeof(s->calls));
    s->call_count = NoticeCallCount;
    s->ret = ret;
}

static void notice_restore(const NoticeState *s) {
    memcpy(ParseStrBuffer, s->parse, sizeof(s->parse));
    memcpy(ParseStrGender, s->parse_gender, sizeof(s->parse_gender));
    memcpy(ParseStrPlurality, s->parse_plural, sizeof(s->parse_plural));
    *GenderDefault = s->gender_default;
    *PluralityDefault = s->plural_default;
    memcpy(stringTemp, &s->temp, sizeof(s->temp));
    memcpy(PronounBuffer, s->pronoun, sizeof(s->pronoun));
    memcpy(DiploGlobals, s->diplo, sizeof(s->diplo));
    *MultiWinFlag = s->multiwin_flag;
    random_reseed(s->seed);
}

static BOOL notice_differs(const NoticeState *o, const NoticeState *n, const char *label) {
#define DLOG(...) do { if (label) logf(__VA_ARGS__); } while (0)
    if (o->ret != n->ret || o->call_count != n->call_count) {
        DLOG("  %s: orig returned %d after %d calls, new %d after %d calls", label, o->ret,
            o->call_count, n->ret, n->call_count);
        return true;
    }
    for (int i = 0; i < o->call_count && i < MaxNoticeCalls; i++) {
        const NoticeCall *a = &o->calls[i], *b = &n->calls[i];
        if (a->fn != b->fn || strcmp(a->label, b->label) || strcmp(a->pic, b->pic)
            || strcmp(a->file, b->file) || memcmp(a->args, b->args, sizeof(a->args))) {
            DLOG("  %s: call %d: orig %s('%s' '%s' '%s' %d %d %d) new %s('%s' '%s' '%s' %d %d %d)",
                label, i, NoticeFnName[a->fn], a->label, a->pic, a->file, a->args[0], a->args[1],
                a->args[2], NoticeFnName[b->fn], b->label, b->pic, b->file, b->args[0], b->args[1],
                b->args[2]);
            return true;
        }
    }
    for (int i = 0; i < 10; i++) {
        if (memcmp(&o->parse[i], &n->parse[i], sizeof(char256))
            || o->parse_gender[i] != n->parse_gender[i] || o->parse_plural[i] != n->parse_plural[i]) {
            DLOG("  %s: parse slot %d: orig '%s' %d/%d new '%s' %d/%d", label, i, o->parse[i].str,
                o->parse_gender[i], o->parse_plural[i], n->parse[i].str, n->parse_gender[i],
                n->parse_plural[i]);
            return true;
        }
    }
    if (o->gender_default != n->gender_default || o->plural_default != n->plural_default) {
        DLOG("  %s: parse defaults orig %d/%d new %d/%d", label, o->gender_default,
            o->plural_default, n->gender_default, n->plural_default);
        return true;
    }
    if (memcmp(&o->temp, &n->temp, sizeof(o->temp))) {
        DLOG("  %s: stringTemp orig '%s' new '%s'", label, o->temp.str, n->temp.str);
        return true;
    }
    if (memcmp(o->pronoun, n->pronoun, sizeof(o->pronoun))) {
        DLOG("  %s: pronoun buffer orig '%s' new '%s'", label, o->pronoun, n->pronoun);
        return true;
    }
    size_t d = first_diff(o->diplo, n->diplo, sizeof(o->diplo));
    if (d != (size_t)-1) {
        DLOG("  %s: diplomacy global %08X differs (orig %02X new %02X)", label,
            (unsigned)((uintptr_t)DiploGlobals + d), o->diplo[d], n->diplo[d]);
        return true;
    }
    if (o->multiwin_flag != n->multiwin_flag || o->seed != n->seed) {
        DLOG("  %s: MultiWinFlag %d/%d, random seed %08X/%08X", label, o->multiwin_flag,
            n->multiwin_flag, o->seed, n->seed);
        return true;
    }
    return false;
#undef DLOG
}

// Run notices as the exe and as the DLL from the same state and compare everything they leave.
struct NoticeHarness {
    GameSnapshot base, after_orig, after_new;
    NoticeState saved, orig, now;
    uint8_t status_saved;
    int diff_saved, sunspot_saved;
    int compared, mismatches, with_calls, changed, nonzero;
    NoticeHarness() : compared(0), mismatches(0), with_calls(0), changed(0), nonzero(0) {
        snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
        snap_take(&base);
        NoticeCallCount = 0;
        notice_take(&saved, 0);
        status_saved = *FactionsStatus;
        diff_saved = *DiffLevelCurrent;
        sunspot_saved = *SunspotDuration;
    }
    ~NoticeHarness() {
        prepare();
        snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    }
    void prepare() {
        snap_restore(&base);
        notice_restore(&saved);
        *FactionsStatus = status_saved;
        *DiffLevelCurrent = diff_saved;
        *SunspotDuration = sunspot_saved;
        NoticeCallCount = 0;
        memset(NoticeAnswer, 0, sizeof(NoticeAnswer));
    }
    // setup() puts the variant in place after each restore; exe() and dll() make the call.
    template <class S, class E, class D>
    void run(const char *label, S setup, E exe, D dll) {
        prepare();
        setup();
        int ret_o = exe();
        snap_take(&after_orig);
        notice_take(&orig, ret_o);
        prepare();
        setup();
        int ret_n = dll();
        snap_take(&after_new);
        notice_take(&now, ret_n);
        compared++;
        with_calls += orig.call_count > 0;
        nonzero += ret_o != 0;
        changed += snap_differs(&base, &after_orig, NULL);
        const char *detail = mismatches < MAX_DETAIL ? label : NULL;
        if (notice_differs(&orig, &now, detail) || snap_differs(&after_orig, &after_new, detail)) {
            mismatches++;
        }
    }
};

static const uint8_t NoticeEntry55_56_57[6] = { 0x55, 0x8B, 0xEC, 0x56, 0x57, 0x8B };
static const uint8_t NoticeEntry55_53_56_57[6] = { 0x55, 0x8B, 0xEC, 0x53, 0x56, 0x57 };

// pact_attacks, pact_unpact, pact_truce and pact_treaty: parse slots and a notice
static int test_pact_notices() {
    OriginalEntry vanilla_attacks(0x0055C730, NoticeEntry55_56_57);
    OriginalEntry vanilla_unpact(0x0055CCA0, NoticeEntry55_53_56_57);
    OriginalEntry vanilla_truce(0x0055D040, NoticeEntry55_53_56_57);
    OriginalEntry vanilla_treaty(0x0055D190, NoticeEntry55_56_57);
    NoticeStubs stubs;
    if (!vanilla_attacks.ok || !vanilla_unpact.ok || !vanilla_truce.ok || !vanilla_treaty.ok
        || !stubs.ok()) {
        return 1;
    }
    struct NoticeFnPair { const char *name; func_diplo2 *exe; func_diplo2 *dll; };
    const NoticeFnPair fns[] = {
        { "pact_attacks", (func_diplo2 *)0x0055C730, pact_attacks },
        { "pact_unpact", (func_diplo2 *)0x0055CCA0, pact_unpact },
        { "pact_truce", (func_diplo2 *)0x0055D040, pact_truce },
        { "pact_treaty", (func_diplo2 *)0x0055D190, pact_treaty },
    };
    int total = 0;
    char label[96];
    for (const NoticeFnPair &fn : fns) {
        NoticeHarness h;
        for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
            for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
                if (f1 == f2) {
                    continue;
                }
                sprintf_s(label, "%s(%d, %d)", fn.name, f1, f2);
                h.run(label, [] {}, [&] { fn.exe(f1, f2); return 0; },
                    [&] { fn.dll(f1, f2); return 0; });
            }
        }
        logf("%s: %d compared (%d with a notice), %d mismatches", fn.name, h.compared,
            h.with_calls, h.mismatches);
        total += h.mismatches;
    }
    return total;
}

static int test_enemies_unpact() {
    OriginalEntry vanilla(0x0055CE20, NoticeEntry55_56_57);
    OriginalEntry vanilla_unpact(0x0055CCA0, NoticeEntry55_53_56_57);
    NoticeStubs stubs;
    if (!vanilla.ok || !vanilla_unpact.ok || !stubs.ok()) {
        return 1;
    }
    func_diplo2 *exe = (func_diplo2 *)0x0055CE20;
    NoticeHarness h;
    char label[96];
    int cur = *FactionIDCurrent;
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            // as loaded; a pact between them; the same with omniscient view toggled; and with
            // either of them also in a pact with the player
            for (int v = 0; v < 5; v++) {
                auto setup = [&] {
                    if (v) {
                        PlayersData[f1].diplo_treaties[f2] |= DTREATY_PACT;
                        PlayersData[f2].diplo_treaties[f1] |= DTREATY_PACT;
                    }
                    if (v == 2) {
                        *GameState ^= STATE_OMNISCIENT_VIEW;
                    }
                    if (v == 3) {
                        PlayersData[f1].diplo_treaties[cur] |= DTREATY_PACT;
                    }
                    if (v == 4) {
                        PlayersData[f2].diplo_treaties[cur] |= DTREATY_PACT;
                    }
                };
                sprintf_s(label, "enemies_unpact(%d, %d) variant %d", f1, f2, v);
                h.run(label, setup, [&] { exe(f1, f2); return 0; },
                    [&] { enemies_unpact(f1, f2); return 0; });
            }
        }
    }
    logf("enemies_unpact: %d compared (%d changed state, %d with a notice), %d mismatches",
        h.compared, h.changed, h.with_calls, h.mismatches);
    return h.mismatches;
}

static int test_enemies_team_up() {
    OriginalEntry vanilla(0x0055D2A0, NoticeEntry55_53_56_57);
    NoticeStubs stubs;
    if (!vanilla.ok || !stubs.ok()) {
        return 1;
    }
    func_xyi *exe = (func_xyi *)0x0055D2A0;
    NoticeHarness h;
    char label[96];
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            for (int f3 = 1; f3 < MaxPlayerNum; f3++) {
                if (f1 == f2 || f1 == f3 || f2 == f3) {
                    continue;
                }
                // as loaded; then with the target human and the ally at war with it (with and
                // without the faction holding a grudge) or not (with and without the ally
                // holding one)
                for (int v = 0; v < 5; v++) {
                    auto setup = [&] {
                        if (!v) {
                            return;
                        }
                        *FactionsStatus |= (uint8_t)(1 << f3);
                        PlayersData[f1].diplo_treaties[f3] &= ~DTREATY_VENDETTA;
                        if (v <= 2) {
                            PlayersData[f2].diplo_treaties[f3] |= DTREATY_VENDETTA;
                        } else {
                            PlayersData[f2].diplo_treaties[f3] &= ~DTREATY_VENDETTA;
                        }
                        if (v == 2) {
                            PlayersData[f1].diplo_treaties[f3] |= DTREATY_WANT_REVENGE;
                        }
                        if (v == 4) {
                            PlayersData[f2].diplo_treaties[f3] |= DTREATY_WANT_REVENGE;
                        }
                    };
                    sprintf_s(label, "enemies_team_up(%d, %d, %d) variant %d", f1, f2, f3, v);
                    h.run(label, setup, [&] { exe(f1, f2, f3); return 0; },
                        [&] { enemies_team_up(f1, f2, f3); return 0; });
                }
            }
        }
    }
    logf("enemies_team_up: %d compared (%d changed state, %d with a notice), %d mismatches",
        h.compared, h.changed, h.with_calls, h.mismatches);
    return h.mismatches;
}

typedef BOOL func_break_treaty(int, int, uint32_t);

static int test_break_treaty() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x53, 0x8B, 0x5D };
    OriginalEntry vanilla(0x0055F770, entry);
    NoticeStubs stubs;
    if (!vanilla.ok || !stubs.ok()) {
        return 1;
    }
    func_break_treaty *exe = (func_break_treaty *)0x0055F770;
    static const uint32_t states[] = { 0, DTREATY_PACT | DTREATY_TREATY, DTREATY_TREATY,
        DTREATY_TRUCE, DTREATY_TRUCE | DTREATY_UNK_10000, DTREATY_VENDETTA | DTREATY_TREATY };
    static const uint32_t masks[] = { DTREATY_PACT, DTREATY_TREATY, DTREATY_TRUCE,
        DTREATY_PACT | DTREATY_TREATY | DTREATY_TRUCE, DTREATY_UNK_10000,
        DTREATY_TRUCE | DTREATY_UNK_10000, 0xFFFFFFFF };
    NoticeHarness h;
    char label[96];
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            // state 0 is the treaty word as loaded; the popup answers 0 and 1
            for (int s = 0; s < (int)(sizeof(states) / sizeof(states[0])); s++) {
                for (uint32_t mask : masks) {
                    for (int answer = 0; answer < 2; answer++) {
                        auto setup = [&] {
                            if (s) {
                                PlayersData[f1].diplo_treaties[f2] = states[s];
                            }
                            NoticeAnswer[NF_POPP] = answer;
                        };
                        sprintf_s(label, "break_treaty(%d, %d, 0x%X) state %d answer %d", f1, f2,
                            mask, s, answer);
                        h.run(label, setup, [&] { return (int)exe(f1, f2, mask); },
                            [&] { return (int)break_treaty(f1, f2, mask); });
                    }
                }
            }
        }
    }
    logf("break_treaty: %d compared (%d returned true, %d with a popup), %d mismatches",
        h.compared, h.nonzero, h.with_calls, h.mismatches);
    return h.mismatches;
}

static int test_make_treaty() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x50 };
    OriginalEntry vanilla(0x0053CE10, entry);
    NoticeStubs stubs;
    if (!vanilla.ok || !stubs.ok()) {
        return 1;
    }
    func_diplo2 *exe = (func_diplo2 *)0x0053CE10;
    NoticeHarness h;
    char label[96];
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            // the tutorial check answering 0 and 1, and the lowest two difficulty levels that
            // skip it
            for (int v = 0; v < 3; v++) {
                auto setup = [&] {
                    NoticeAnswer[NF_TUT_CHECK2] = v != 0;
                    if (v == 2) {
                        *DiffLevelCurrent = DLVL_SPECIALIST;
                    }
                };
                sprintf_s(label, "make_treaty(%d, %d) variant %d", f1, f2, v);
                h.run(label, setup, [&] { exe(f1, f2); return 0; },
                    [&] { make_treaty(f1, f2); return 0; });
            }
        }
    }
    logf("make_treaty: %d compared (%d changed state), %d mismatches", h.compared, h.changed,
        h.mismatches);
    return h.mismatches;
}

static int test_pledge_truce() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0xA1, 0x60, 0xF6 };
    OriginalEntry vanilla(0x0053D160, entry);
    NoticeStubs stubs;
    if (!vanilla.ok || !stubs.ok()) {
        return 1;
    }
    func_xyi *exe = (func_xyi *)0x0053D160;
    NoticeHarness h;
    char label[96];
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            for (int show = 0; show < 2; show++) {
                sprintf_s(label, "pledge_truce(%d, %d, %d)", f1, f2, show);
                h.run(label, [] {}, [&] { exe(f1, f2, show); return 0; },
                    [&] { pledge_truce(f1, f2, show); return 0; });
            }
        }
    }
    logf("pledge_truce: %d compared (%d changed state, %d with a dialog), %d mismatches",
        h.compared, h.changed, h.with_calls, h.mismatches);
    return h.mismatches;
}

static int test_demands_withdrawal() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x56, 0x8B, 0x75 };
    OriginalEntry vanilla(0x0053E4C0, entry);
    NoticeStubs stubs;
    if (!vanilla.ok || !stubs.ok()) {
        return 1;
    }
    func_diplo2 *exe = (func_diplo2 *)0x0053E4C0;
    NoticeHarness h;
    char label[96];
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            // variant 0: no contact; then contact without and with sunspots (since batch 29 the
            // real demand_withdrawal() answers on both sides; the answer set here goes unused)
            for (int v = 0; v < 9; v++) {
                auto setup = [&] {
                    NoticeAnswer[NF_COMMLINK] = !v;
                    NoticeAnswer[NF_DEMAND_WITHDRAWAL] = v ? (v - 1) % 4 : 0;
                    *SunspotDuration = (!v || v > 4) ? 3 : 0;
                };
                sprintf_s(label, "demands_withdrawal(%d, %d) variant %d", f1, f2, v);
                h.run(label, setup, [&] { exe(f1, f2); return 0; },
                    [&] { demands_withdrawal(f1, f2); return 0; });
            }
        }
    }
    logf("demands_withdrawal: %d compared (%d with a call out), %d mismatches", h.compared,
        h.with_calls, h.mismatches);
    return h.mismatches;
}

static int test_renounce_pact() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x53, 0x8B, 0x5D };
    OriginalEntry vanilla(0x0053E5C0, entry);
    NoticeStubs stubs;
    if (!vanilla.ok || !stubs.ok()) {
        return 1;
    }
    func_diplo2 *exe = (func_diplo2 *)0x0053E5C0;
    NoticeHarness h;
    char label[96];
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            // no contact; contact without and with sunspots; contact with a pact in force
            for (int v = 0; v < 4; v++) {
                auto setup = [&] {
                    NoticeAnswer[NF_COMMLINK] = !v;
                    *SunspotDuration = v == 2 ? 3 : 0;
                    if (v == 3) {
                        PlayersData[f1].diplo_treaties[f2] |= DTREATY_PACT | DTREATY_TREATY;
                        PlayersData[f2].diplo_treaties[f1] |= DTREATY_PACT | DTREATY_TREATY;
                    }
                };
                sprintf_s(label, "renounce_pact(%d, %d) variant %d", f1, f2, v);
                h.run(label, setup, [&] { exe(f1, f2); return 0; },
                    [&] { renounce_pact(f1, f2); return 0; });
            }
        }
    }
    logf("renounce_pact: %d compared (%d changed state), %d mismatches", h.compared, h.changed,
        h.mismatches);
    return h.mismatches;
}

static int test_dont_withdrawal() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C };
    OriginalEntry vanilla(0x00548DD0, entry);
    NoticeStubs stubs;
    if (!vanilla.ok || !stubs.ok()) {
        return 1;
    }
    func_diplo2r *exe = (func_diplo2r *)0x00548DD0;
    NoticeHarness h;
    char label[96];
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            // a third faction's base for the units to head for
            int base_tgt = -1;
            for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
                int owner = Bases[base_id].faction_id_current;
                if (owner && owner != f1 && owner != f2) {
                    base_tgt = base_id;
                    break;
                }
            }
            // as loaded; the flag set; then every unit of f2 moving to that base, the third
            // faction at war with f2 and f1's pact partner, or its treaty partner with
            // DiploUnk1 positive or not
            for (int v = 0; v < 5; v++) {
                auto setup = [&] {
                    if (!v) {
                        return;
                    }
                    PlayersData[f1].diplo_treaties[f2] |= DTREATY_UNK_800000;
                    if (v < 2 || base_tgt < 0) {
                        return;
                    }
                    int t = Bases[base_tgt].faction_id_current;
                    for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
                        if (Vehs[veh_id].faction_id == f2) {
                            Vehs[veh_id].order = ORDER_MOVE_TO;
                            Vehs[veh_id].waypoint_x[0] = Bases[base_tgt].x;
                            Vehs[veh_id].waypoint_y[0] = Bases[base_tgt].y;
                        }
                    }
                    PlayersData[f2].diplo_treaties[t] |= DTREATY_VENDETTA;
                    if (v == 2) {
                        PlayersData[f1].diplo_treaties[t] |= DTREATY_PACT;
                    } else {
                        PlayersData[f1].diplo_treaties[t] =
                            (PlayersData[f1].diplo_treaties[t] & ~DTREATY_PACT) | DTREATY_TREATY;
                        *DiploUnk1 = v == 3;
                    }
                };
                sprintf_s(label, "dont_withdrawal(%d, %d) variant %d", f1, f2, v);
                h.run(label, setup, [&] { return exe(f1, f2); },
                    [&] { return dont_withdrawal(f1, f2); });
            }
        }
    }
    logf("dont_withdrawal: %d compared (%d found a reason), %d mismatches", h.compared,
        h.nonzero, h.mismatches);
    return h.mismatches;
}

// ---------------------------------------------------------------- batch 29: diplomacy part two

static void *__cdecl notice_x_pop(LPCSTR label, int(__cdecl *callback)()) {
    return (void *)notice_record(NF_X_POP, label, NULL, NULL, notice_callback_id(callback), 0, 0);
}
static int __cdecl notice_wants_to_speak(int faction_id, int faction_id_with) {
    return notice_record(NF_WANTS_TO_SPEAK, NULL, NULL, NULL, faction_id, faction_id_with, 0);
}
static void __cdecl notice_tech_achieved(int faction_id, int tech_id, int faction_id_from, int unk) {
    notice_record(NF_TECH_ACHIEVED, NULL, NULL, NULL, faction_id, tech_id, faction_id_from * 16 + unk);
}
static void __cdecl notice_double_cross(int faction_id, int faction_id_with, int faction_id_other) {
    notice_record(NF_DOUBLE_CROSS, NULL, NULL, NULL, faction_id, faction_id_with, faction_id_other);
}
static void __fastcall notice_console_update(void *This, void *, int unk) {
    notice_record(NF_CONSOLE_UPDATE, NULL, NULL, NULL, This == ConsoleVar, unk, 0);
}
static void __fastcall notice_console_set_move(void *This, void *, int unk) {
    notice_record(NF_CONSOLE_SET_MOVE, NULL, NULL, NULL, This == ConsoleVar, unk, 0);
}
static void __cdecl notice_caption(int faction_id, int faction_id_with) {
    notice_record(NF_CAPTION, NULL, NULL, NULL, faction_id, faction_id_with, 0);
}
static void __cdecl notice_help_topic(int topic, int id) {
    notice_record(NF_HELP_TOPIC, NULL, NULL, NULL, topic, id, 0);
}

// The further dialogs, UI refreshes and game events the batch 29 diplomacy functions reach.
// double_cross() is in the DLL since batch 31: its stub goes on the DLL function, which the exe's
// calls reach through the redirect and the DLL's (net_double_cross) directly.
struct DiploTwoStubs {
    JmpStub x_pop_stub, speak_stub, tech_stub, double_cross_stub, update_stub, set_move_stub,
        caption_stub, help_stub;
    DiploTwoStubs()
        : x_pop_stub(0x005BF310, (void *)notice_x_pop),
          speak_stub(0x0053DCE0, (void *)notice_wants_to_speak),
          tech_stub(0x005BB000, (void *)notice_tech_achieved),
          double_cross_stub((uint32_t)&double_cross, (void *)notice_double_cross),
          update_stub(0x00514880, (void *)notice_console_update),
          set_move_stub(0x00511520, (void *)notice_console_set_move),
          caption_stub(0x005399A0, (void *)notice_caption),
          help_stub(0x0044CB60, (void *)notice_help_topic) {}
    BOOL ok() const {
        return x_pop_stub.ok && speak_stub.ok && tech_stub.ok && double_cross_stub.ok
            && update_stub.ok && set_move_stub.ok && caption_stub.ok && help_stub.ok;
    }
};

static int test_intervention() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08 };
    OriginalEntry vanilla(0x0050BA50, entry);
    NoticeStubs stubs;
    DiploTwoStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok()) {
        return 1;
    }
    func_diplo2 *exe = (func_diplo2 *)0x0050BA50;
    const int current_saved = *FactionIDCurrent;
    NoticeHarness h;
    char label[96];
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            // as loaded; every other faction in a pact with f1 and free of f2; then f2 human,
            // f1 human, f2 or f1 at the controls
            for (int v = 0; v < 6; v++) {
                auto setup = [&] {
                    *FactionIDCurrent = (v == 4) ? f2 : ((v == 5) ? f1 : current_saved);
                    if (!v) {
                        return;
                    }
                    for (int i = 1; i < MaxPlayerNum; i++) {
                        if (i != f1 && i != f2) {
                            PlayersData[i].diplo_treaties[f1] |= DTREATY_PACT;
                            PlayersData[i].diplo_treaties[f2] &= ~(DTREATY_PACT | DTREATY_VENDETTA);
                        }
                    }
                    *FactionsStatus &= ~((1 << f1) | (1 << f2));
                    if (v == 2) {
                        *FactionsStatus |= 1 << f2;
                    } else if (v == 3) {
                        *FactionsStatus |= 1 << f1;
                    }
                };
                sprintf_s(label, "intervention(%d, %d) variant %d", f1, f2, v);
                h.run(label, setup, [&] { exe(f1, f2); return 0; },
                    [&] { intervention(f1, f2); return 0; });
            }
        }
    }
    *FactionIDCurrent = current_saved;
    logf("intervention: %d compared (%d changed state, %d with calls), %d mismatches", h.compared,
        h.changed, h.with_calls, h.mismatches);
    return h.mismatches;
}

static int test_demand_withdrawal() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x51, 0x53, 0x56 };
    OriginalEntry vanilla(0x00549270, entry);
    NoticeStubs stubs;
    DiploTwoStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok()) {
        return 1;
    }
    func_diplo2r *exe = (func_diplo2r *)0x00549270;
    NoticeHarness h;
    char label[96];
    int answers[3] = {};
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            // 0 as loaded; 1-2 DiploUnk2 (vendetta), with sunspots; 3 friction over 15;
            // 4 friction over 5 with DiploUnk4; 5 a treaty, calm; 6-7 a truce, calm, TURNABOUT
            // answered 1 and 0; 8-9 refused by DiploUnk6, with sunspots; 10 a pact; 11 the 0x200
            // flag with the vendetta values set; 12 calm with revenge wanted and the friction
            // faction pair matching
            for (int v = 0; v < 13; v++) {
                auto setup = [&] {
                    if (!v) {
                        return;
                    }
                    uint32_t &t_with = PlayersData[f2].diplo_treaties[f1];
                    uint32_t &t = PlayersData[f1].diplo_treaties[f2];
                    t_with &= ~(DTREATY_PACT | DTREATY_UNK_200 | DTREATY_WANT_REVENGE);
                    t &= ~(DTREATY_PACT | DTREATY_TREATY);
                    *DiploUnk2 = (v == 1 || v == 2 || v == 11);
                    *DiploUnk3 = 0;
                    *DiploUnk4 = (v == 4);
                    *DiploUnk5 = (v >= 5 && v != 8 && v != 9) ? 1 : 0;
                    *DiploUnk6 = (v == 8 || v == 9) ? 5 : 0;
                    *DiploFriction = (v == 3) ? 16 : ((v == 4) ? 6 : 0);
                    *SunspotDuration = (v == 2 || v == 9) ? 3 : 0;
                    if (v == 5) {
                        t |= DTREATY_TREATY;
                    } else if (v == 10) {
                        t |= DTREATY_PACT | DTREATY_TREATY;
                    } else if (v == 11) {
                        t_with |= DTREATY_UNK_200;
                        t |= DTREATY_TREATY;
                    } else if (v == 12) {
                        t_with |= DTREATY_WANT_REVENGE;
                    }
                    *DiploFrictionFactionID = (v == 12) ? f2 : 0;
                    *DiploFrictionFactionIDWith = (v == 12) ? f1 : 0;
                    NoticeAnswer[NF_X_POPS_11] = (v != 7);
                };
                sprintf_s(label, "demand_withdrawal(%d, %d) variant %d", f1, f2, v);
                h.run(label, setup, [&] { return exe(f1, f2); },
                    [&] { return demand_withdrawal(f1, f2); });
                int ret = h.orig.ret;
                if (ret >= 0 && ret < 3) {
                    answers[ret]++;
                }
            }
        }
    }
    logf("demand_withdrawal: %d compared (answers by the original: %d withdrew, %d refused, %d war; "
        "%d changed state), %d mismatches", h.compared, answers[0], answers[1], answers[2], h.changed,
        h.mismatches);
    return h.mismatches;
}

// A design of faction_id_with for mention_prototypes() to offer: its first active one.
static int diplotwo_proto(int faction_id) {
    for (int i = 0; i < MaxVehProtoFactionNum; i++) {
        int proto_id = faction_id * MaxVehProtoFactionNum + i;
        if (VehPrototypes[proto_id].flags & PROTO_ACTIVE) {
            return proto_id;
        }
    }
    return -1;
}

static int test_mention_prototypes() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08 };
    OriginalEntry vanilla(0x0053A230, entry);
    NoticeStubs stubs;
    DiploTwoStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok()) {
        return 1;
    }
    func_diplo2 *exe = (func_diplo2 *)0x0053A230;
    static VehPrototype protos_saved[MaxVehProtoNum];
    memcpy(protos_saved, VehPrototypes, sizeof(protos_saved));
    NoticeHarness h;
    char label[96];
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            int proto_id = diplotwo_proto(f2);
            // 0 as loaded; 1 nothing to offer; 2 unwilling to speak; 3 a pact; 4-5 offered,
            // refused and accepted
            for (int v = 0; v < 6; v++) {
                auto setup = [&] {
                    memcpy(VehPrototypes, protos_saved, sizeof(protos_saved));
                    if (!v) {
                        return;
                    }
                    *DiploProtoIDTradeable = (v == 1) ? -1 : proto_id;
                    *DiploProtoIDBest = proto_id;
                    NoticeAnswer[NF_WANTS_TO_SPEAK] = (v != 2);
                    NoticeAnswer[NF_X_POPS_11] = (v == 5);
                    if (v == 3) {
                        PlayersData[f1].diplo_treaties[f2] |= DTREATY_PACT;
                    } else {
                        PlayersData[f1].diplo_treaties[f2] &= ~DTREATY_PACT;
                    }
                };
                sprintf_s(label, "mention_prototypes(%d, %d) variant %d", f1, f2, v);
                VehPrototype *after_o = (VehPrototype *)malloc(sizeof(protos_saved));
                h.run(label, setup,
                    [&] { exe(f1, f2); memcpy(after_o, VehPrototypes, sizeof(protos_saved)); return 0; },
                    [&] { mention_prototypes(f1, f2); return 0; });
                size_t d = first_diff(after_o, VehPrototypes, sizeof(protos_saved));
                if (d != (size_t)-1) {
                    if (h.mismatches < MAX_DETAIL) {
                        logf("  %s: VehPrototypes byte %u differs", label, (unsigned)d);
                    }
                    h.mismatches++;
                }
                free(after_o);
            }
        }
    }
    memcpy(VehPrototypes, protos_saved, sizeof(protos_saved));
    logf("mention_prototypes: %d compared (%d with calls), %d mismatches", h.compared, h.with_calls,
        h.mismatches);
    return h.mismatches;
}

static int test_buy_tech() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x50 };
    OriginalEntry vanilla(0x005401A0, entry);
    NoticeStubs stubs;
    DiploTwoStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok()) {
        return 1;
    }
    typedef int func_buy(int, int, int, BOOL, int);
    func_buy *exe = (func_buy *)0x005401A0;
    NoticeHarness h;
    char label[96];
    const int types[] = { 0, 5, 13 };
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            // a tech f2 has: the first one that f1 lacks, else the first it has
            int tech_id = -1;
            for (int t = 0; t < MaxTechnologyNum && tech_id < 0; t++) {
                if (has_tech(t, f2) && !has_tech(t, f1)) {
                    tech_id = t;
                }
            }
            for (int t = 0; t < MaxTechnologyNum && tech_id < 0; t++) {
                if (has_tech(t, f2)) {
                    tech_id = t;
                }
            }
            for (int type : types) {
                // 0 as loaded; 1 the tech picked, rich, accepted; 2 refused; 3 poor; 4 poor with
                // unk1 5; 5 unk1 1; 6 markup, pact and DiploUnk7; 7 nothing picked
                for (int v = 0; v < 8; v++) {
                    auto setup = [&] {
                        if (!v) {
                            return;
                        }
                        *DiploTechGetID1 = (v == 7) ? -1 : tech_id;
                        *DiploProtoIDBest = (v == 7) ? -1 : diplotwo_proto(f2);
                        *DiploUnk7 = (v == 6);
                        *DiploUnk8 = f2;
                        PlayersData[f1].energy_reserves = (v == 3 || v == 4) ? 10 : 100000;
                        if (v == 6) {
                            PlayersData[f1].diplo_treaties[f2] |= DTREATY_PACT;
                        }
                        NoticeAnswer[NF_X_POPS_11] = (v != 2);
                    };
                    int unk1 = (v == 4) ? 5 : ((v == 5) ? 1 : 0);
                    BOOL markup = (v == 6);
                    sprintf_s(label, "buy_tech(%d, %d, %d, %d, %d) variant %d", f1, f2, unk1, markup,
                        type, v);
                    h.run(label, setup, [&] { return exe(f1, f2, unk1, markup, type); },
                        [&] { return buy_tech(f1, f2, unk1, markup, type); });
                }
            }
        }
    }
    logf("buy_tech: %d compared (%d made an offer, %d with calls), %d mismatches", h.compared,
        h.nonzero, h.with_calls, h.mismatches);
    return h.mismatches;
}

static int test_energy_trade() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x60 };
    OriginalEntry vanilla(0x00540630, entry);
    NoticeStubs stubs;
    DiploTwoStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok()) {
        return 1;
    }
    func_diplo2r *exe = (func_diplo2r *)0x00540630;
    NoticeHarness h;
    char label[96];
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            // 0 as loaded; 1 a gift from a rich pact partner; 2-4 a loan (agreed, refused, with
            // a pact and rank); 5-8 a tech sale (first tech, help then first, second, declined);
            // 9 nothing on offer
            for (int v = 0; v < 10; v++) {
                auto setup = [&] {
                    NoticeFirstAnswer = -1;
                    if (!v) {
                        return;
                    }
                    PlayerData *p = &PlayersData[f1];
                    PlayerData *p_with = &PlayersData[f2];
                    const int proposals[] = { 0, 1, 6, 6, 1, 4, 4, 2, 4, 0 };
                    *DiploCounterProposalID = proposals[v];
                    p_with->diplo_treaties[f1] &= ~(DTREATY_PACT | DTREATY_VENDETTA | DTREATY_WANT_REVENGE);
                    p->diplo_treaties[f2] &= ~DTREATY_PACT;
                    p_with->loan_balance[f1] = 0;
                    p->loan_balance[f2] = 0;
                    if (v == 1) {
                        p_with->diplo_treaties[f1] |= DTREATY_PACT;
                        p_with->ranking = 7;
                        p->ranking = 0;
                        p_with->energy_reserves = 100000;
                        p->energy_reserves = 0;
                        p_with->tech_ranking = 100;
                        p->tech_ranking = 0;
                        p_with->diplo_patience[f1] = 0;
                    } else if (v >= 2 && v <= 4) {
                        p_with->diplo_treaties[f1] |= DTREATY_TREATY;
                        p_with->energy_reserves = 1000;
                        if (v == 4) {
                            p->diplo_treaties[f2] |= DTREATY_PACT;
                            p_with->ranking = 7;
                            p->ranking = 0;
                        }
                    } else if (v >= 5) {
                        p_with->energy_reserves = 5000;
                    }
                    NoticeAnswer[NF_X_POPS_11] = (v == 3 || v == 7 || v == 8) ? 0 : 1;
                    if (v == 6) {
                        NoticeFirstAnswer = 3;
                    } else if (v == 7) {
                        // the second tech once, then decline: with one tech on offer the answer 2
                        // is help and the question comes again, in the exe as in the DLL
                        NoticeFirstAnswer = 2;
                    }
                };
                sprintf_s(label, "energy_trade(%d, %d) variant %d", f1, f2, v);
                h.run(label, setup, [&] { return exe(f1, f2); },
                    [&] { return energy_trade(f1, f2); });
            }
        }
    }
    NoticeFirstAnswer = -1;
    logf("energy_trade: %d compared (%d returned true, %d changed state), %d mismatches", h.compared,
        h.nonzero, h.changed, h.mismatches);
    return h.mismatches;
}

static int test_pact_withdraw() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x1C };
    OriginalEntry vanilla(0x0053C370, entry);
    NoticeStubs stubs;
    DiploTwoStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok()) {
        return 1;
    }
    func_diplo2r *exe = (func_diplo2r *)0x0053C370;
    const BOOL view_saved = *MapWinUnitNotViewMode;
    const int selected_saved = *MapWinVehIDSelected;
    NoticeHarness h;
    char label[96];
    int moved = 0;
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            // as loaded; with f1's last unit in f2's territory selected in the map window
            int selected = -1;
            for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
                if (Vehs[veh_id].faction_id == f1
                    && whose_territory(f1, Vehs[veh_id].x, Vehs[veh_id].y, NULL, false) == f2) {
                    selected = veh_id;
                }
            }
            for (int v = 0; v < 2; v++) {
                auto setup = [&] {
                    *MapWinUnitNotViewMode = v ? true : view_saved;
                    *MapWinVehIDSelected = v ? selected : selected_saved;
                };
                sprintf_s(label, "pact_withdraw(%d, %d) variant %d", f1, f2, v);
                h.run(label, setup, [&] { return exe(f1, f2); },
                    [&] { return pact_withdraw(f1, f2); });
                moved += h.orig.ret;
            }
        }
    }
    *MapWinUnitNotViewMode = view_saved;
    *MapWinVehIDSelected = selected_saved;
    logf("pact_withdraw: %d compared (%d units moved by the original, %d with calls), %d mismatches",
        h.compared, moved, h.with_calls, h.mismatches);
    return h.mismatches;
}

// ---------------------------------------------------------------- wants_to_attack
// The original reads 7 slots of its stack it never writes first (slot f at esp_entry - 0x138 +
// 4 * f); the DLL starts them at zero instead (Kiousu's choice, 2026-10-18). wta_seeded_call() puts
// chosen values in those slots before calling an entry and reads back what the call left there.
static __declspec(naked) BOOL __cdecl wta_seeded_call(func_wants *UNUSED(entry),
    uint32_t UNUSED(faction_id), uint32_t UNUSED(faction_id_tgt), int UNUSED(faction_id_unk),
    int *UNUSED(slots)) {
    __asm {
        mov edx, [esp + 20]                 // slots
        mov ecx, 7
    wta_seed:
        mov eax, [edx + ecx * 4]
        mov [esp + ecx * 4 - 0x148], eax    // the callee's esp_entry is esp - 16
        dec ecx
        jnz wta_seed
        push dword ptr [esp + 16]           // faction_id_unk
        push dword ptr [esp + 16]           // faction_id_tgt
        push dword ptr [esp + 16]           // faction_id
        call dword ptr [esp + 16]           // entry
        add esp, 12
        push esi
        mov edx, [esp + 24]
        mov ecx, 7
    wta_read_back:
        mov esi, [esp + ecx * 4 - 0x144]
        mov [edx + ecx * 4], esi
        dec ecx
        jnz wta_read_back
        pop esi
        ret
    }
}

static uint32_t WtaSeedState = 0x2545F491;

static uint32_t wta_seed_rand() {
    WtaSeedState ^= WtaSeedState << 13;
    WtaSeedState ^= WtaSeedState >> 17;
    WtaSeedState ^= WtaSeedState << 5;
    return WtaSeedState;
}

typedef int __cdecl func_hex_cost(int, int, uint32_t, uint32_t, uint32_t, uint32_t, BOOL);

// hex_cost keeps upstream's fix (Kiousu, 2026-10-18): a Sealurk entering fungus on the ocean shelf
// pays the normal sea cost, as Isle of the Deep does, where the v2.1 exe charges it triple. Every
// other answer must be the exe's.
static int test_hex_cost() {
    // run the exe's own hex_cost, not the jump into the DLL
    static const uint8_t hex_cost_entry[6] = { 0x55, 0x8B, 0xEC, 0x51, 0x8B, 0x0D };
    OriginalEntry vanilla(0x00593510, hex_cost_entry);
    if (!vanilla.ok) {
        return 1;
    }
    func_hex_cost *hex_cost_orig = (func_hex_cost *)0x00593510;
    // the basic units and every active prototype
    static int protos[MaxVehProtoNum];
    int proto_count = 0;
    for (int p = 0; p < MaxVehProtoNum; p++) {
        if (p <= BSC_UNITY_GUNSHIP || VehPrototypes[p].flags & PROTO_ACTIVE) {
            protos[proto_count++] = p;
        }
    }
    int move = Rules->move_rate_roads;
    int compared = 0, mismatches = 0, shelf_tiles = 0, sealurk_fixed = 0;
    for (uint32_t i = 0; i < *MapArea; i++) {
        uint32_t y_dst = i / *MapLongitude, x_dst = (i % *MapLongitude) * 2 + (y_dst & 1);
        BOOL shelf = is_ocean(x_dst, y_dst) && bit_at(x_dst, y_dst) & BIT_FUNGUS
            && altitude_at(x_dst, y_dst) == ALT_BIT_OCEAN_SHELF;
        shelf_tiles += shelf;
        // every shelf fungus tile with every prototype, every 5th other tile with every 8th
        if (!shelf && i % 5) {
            continue;
        }
        for (int n = 1; n < 9; n++) {
            int x_src = x_dst + RadiusOffsetX[n], y_src = y_dst + RadiusOffsetY[n];
            if (!(*MapIsFlat & 1)) {
                x_src = x_src < 0 ? x_src + *MapLongitudeBounds
                    : (x_src >= (int)*MapLongitudeBounds ? x_src - *MapLongitudeBounds : x_src);
            }
            if (y_src < 0 || y_src >= (int)*MapLatitudeBounds || x_src < 0
                || x_src >= (int)*MapLongitudeBounds) {
                continue;
            }
            for (int k = shelf ? 0 : (int)(i + n) % 8; k < proto_count; k += shelf ? 1 : 8) {
                int p = protos[k];
                for (uint32_t f = 0; f < MaxPlayerNum; f++) {
                    BOOL fix = shelf && p == BSC_SEALURK && get_proto_triad(p) == TRIAD_SEA
                        && !has_project(SP_XENOEMPATYH_DOME, f);
                    for (BOOL tgl = 0; tgl <= 1; tgl++) {
                        REGRESS_CTX("hex_cost(%d, %u, %d, %d, %u, %u, %d)", p, f, x_src, y_src, x_dst, y_dst, tgl);
                        int b = hex_cost_orig(p, f, x_src, y_src, x_dst, y_dst, tgl);
                        int a = hex_cost(p, f, x_src, y_src, x_dst, y_dst, tgl);
                        compared++;
                        if (fix ? !(b == move * 3 && a == move) : a != b) {
                            if (mismatches < MAX_DETAIL) {
                                logf("  hex_cost(%d, %u, %d, %d, %u, %u, %d): new=%d orig=%d%s", p, f,
                                    x_src, y_src, x_dst, y_dst, tgl, a, b, fix ? " (Sealurk fix expected)" : "");
                            }
                            mismatches++;
                        } else if (fix) {
                            sealurk_fixed++;
                        }
                    }
                }
            }
        }
    }
    logf("hex_cost: %d compared (%d prototypes, %d shelf fungus tiles), %d mismatches; %d Sealurk moves "
        "onto shelf fungus at the normal sea cost (the kept fix; the exe charges triple)",
        compared, proto_count, shelf_tiles, mismatches, sealurk_fixed);
    return mismatches;
}

static int test_wants_to_attack() {
    // run the exe's own wants_to_attack, not the jump into the DLL
    static const uint8_t wta_entry[6] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x34 };
    OriginalEntry vanilla(0x0055BC80, wta_entry);
    if (!vanilla.ok) {
        return 1;
    }
    // the slots as a trace of the exe found them on Council.SAV, turn 289 (stack and return
    // addresses): what the shipped game usually decides with
    static const int traced[8] = { 0, 1746744, 1310034, 4624118, 1310058, 1746680, 9672888, 0x468EF6 };
    int compared = 0, mismatches = 0, shipped_differs = 0;
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        for (uint32_t tgt = 1; tgt < MaxPlayerNum; tgt++) {
            if (f == tgt) {
                continue;
            }
            for (int other = -1; other < (int)MaxPlayerNum; other++) {
                int junk[8] = { 0 };
                for (int s = 1; s < 8; s++) {
                    junk[s] = (int)wta_seed_rand();
                }
                int slots[8] = { 0 };
                BOOL o = wta_seeded_call((func_wants *)0x0055BC80, f, tgt, other, slots); // from zero
                memcpy(slots, traced, sizeof(slots));
                BOOL shipped = wta_seeded_call((func_wants *)0x0055BC80, f, tgt, other, slots);
                // the DLL gives the zero-start answer whatever its stack holds
                memset(slots, 0, sizeof(slots));
                BOOL n_zero = wta_seeded_call(wants_to_attack, f, tgt, other, slots);
                memcpy(slots, traced, sizeof(slots));
                BOOL n_traced = wta_seeded_call(wants_to_attack, f, tgt, other, slots);
                memcpy(slots, junk, sizeof(slots));
                BOOL n_junk = wta_seeded_call(wants_to_attack, f, tgt, other, slots);
                compared++;
                if (shipped != o) {
                    shipped_differs++;
                }
                if (n_zero != o || n_traced != o || n_junk != o) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  wants_to_attack(%u, %u, %d): orig from zero %d, new %d/%d/%d (zero/traced/junk stack)",
                            f, tgt, other, o, n_zero, n_traced, n_junk);
                    }
                    mismatches++;
                }
            }
        }
    }
    logf("wants_to_attack: %d compared (%d where the shipped game's leftovers change the original's answer), "
        "%d mismatches", compared, shipped_differs, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- save header version slot
// game_data() in save mode writes a 0x31C-byte header buffer it never fills and then reads its
// version field ([ebp-0x70]): below 11 it copies the buffer into the game state as an old save.
// The probe records that slot at game_data's entry and sets it to 11 so a probe save cannot
// corrupt the game, while save_daemon() runs with the DLL's or the exe's env_open_old and
// header_write (both run inside save_daemon before game_data).
static int GameDataSlots[64];
static int GameDataSlotCount;

static __declspec(naked) void game_data_probe() {
    __asm {
        mov eax, [esp - 0x74]               // [ebp-0x70] of game_data's frame
        mov ecx, GameDataSlotCount
        cmp ecx, 64
        jae probe_full
        mov GameDataSlots[ecx * 4], eax
        inc ecx
        mov GameDataSlotCount, ecx
    probe_full:
        mov dword ptr [esp - 0x74], 11
        push ebp                            // game_data's prologue, replaced by the jump here
        mov ebp, esp
        sub esp, 0x374
        mov eax, 0x005A6AC9
        jmp eax
    }
}

static int test_save_slot() {
    static const uint8_t header_write_entry[6] = { 0x55, 0x8B, 0xEC, 0x53, 0x56, 0x8B };
    static const uint8_t env_open_old_entry[6] = { 0x56, 0x8B, 0x74, 0x24, 0x08, 0x56 };
    static const char *probe = "G:\\SMACX-opensmacx-test\\saves\\_slot_probe";
    JmpStub hook(0x005A6AC0, game_data_probe);
    if (!hook.ok) {
        return 1;
    }
    for (int variant = 0; variant < 4; variant++) {
        OriginalEntry *exe_header = (variant & 1) ? new OriginalEntry(0x0057D240, header_write_entry) : NULL;
        OriginalEntry *exe_open = (variant & 2) ? new OriginalEntry(0x00634BB0, env_open_old_entry) : NULL;
        GameDataSlotCount = 0;
        int failed = 0, low = 0;
        for (int i = 0; i < 16; i++) {
            if (save_daemon(probe)) {
                failed++;
            }
        }
        delete exe_header;
        delete exe_open;
        char line[1024];
        int len = sprintf_s(line, "save_slot: header_write %s, env_open_old %s: %d saves, %d failed, slot:",
            (variant & 1) ? "exe" : "DLL", (variant & 2) ? "exe" : "DLL", GameDataSlotCount, failed);
        for (int i = 0; i < GameDataSlotCount && len < (int)sizeof(line) - 16; i++) {
            len += sprintf_s(line + len, sizeof(line) - len, " %d", GameDataSlots[i]);
            if (GameDataSlots[i] < 11) {
                low++;
            }
        }
        logf("%s (%d below 11)", line, low);
    }
    DeleteFileA("G:\\SMACX-opensmacx-test\\saves\\_slot_probe.SAV");
    return 0;
}

static int read_whole_file(const char *path, uint8_t **data) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return -1;
    }
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fseek(file, 0, SEEK_SET);
    *data = (uint8_t *)malloc(size);
    int got = (int)fread(*data, 1, size, file);
    fclose(file);
    return got == size ? size : -1;
}

// The game_data() entry (general.cpp) fills the save header the exe leaves uninitialized: a save
// through it must have a header of zeros with version 12 and otherwise match, byte for byte, a
// save through the exe's own entry (taken with the probe, which keeps that save's version >= 11).
// _header_check.SAV is kept for the sweep to load back.
static int test_save_header() {
    static const char *path_dll = "G:\\SMACX-opensmacx-test\\saves\\_header_check";
    static const char *path_exe = "G:\\SMACX-opensmacx-test\\saves\\_header_exe";
    if (save_daemon(path_dll)) {
        logf("save_header: the save through the DLL entry failed");
        return 1;
    }
    {
        JmpStub hook(0x005A6AC0, game_data_probe);
        if (!hook.ok || save_daemon(path_exe)) {
            logf("save_header: the save through the exe's entry failed");
            return 1;
        }
    }
    // control: a second save through the DLL entry, after the exe's
    if (save_daemon("G:\\SMACX-opensmacx-test\\saves\\_header_again")) {
        logf("save_header: the second save through the DLL entry failed");
        return 1;
    }
    uint8_t *dll = NULL, *exe = NULL, *again = NULL;
    int size_dll = read_whole_file("G:\\SMACX-opensmacx-test\\saves\\_header_check.SAV", &dll);
    int size_exe = read_whole_file("G:\\SMACX-opensmacx-test\\saves\\_header_exe.SAV", &exe);
    int size_again = read_whole_file("G:\\SMACX-opensmacx-test\\saves\\_header_again.SAV", &again);
    DeleteFileA("G:\\SMACX-opensmacx-test\\saves\\_header_exe.SAV");
    DeleteFileA("G:\\SMACX-opensmacx-test\\saves\\_header_again.SAV");
    if (size_again == size_dll) {
        int same_entry_diff = 0, first = -1;
        for (int i = 0; i < size_dll; i++) {
            if (dll[i] != again[i]) {
                if (first < 0) {
                    first = i;
                }
                same_entry_diff++;
            }
        }
        logf("save_header: control, two saves through the DLL entry: %d bytes differ (first at 0x%X)",
            same_entry_diff, first);
    }
    free(again);
    int mismatches = 0;
    if (size_dll < 0 || size_dll != size_exe) {
        logf("save_header: sizes %d / %d", size_dll, size_exe);
        mismatches++;
    } else {
        // header text, 0x1A, 4-byte magic, then the 0x31C-byte header block
        uint8_t *text_end = (uint8_t *)memchr(dll, 0x1A, size_dll);
        int block = text_end ? (int)(text_end - dll) + 5 : -1;
        int header_bad = 0, body_diff = 0, first_diff = -1;
        for (int i = 0; block >= 0 && i < 0x31C; i++) {
            if (dll[block + i] != (i == 0x304 ? 12 : 0)) {
                header_bad++;
            }
        }
        for (int i = 0; i < size_dll; i++) {
            if ((i < block || i >= block + 0x31C) && dll[i] != exe[i]) {
                if (first_diff < 0) {
                    first_diff = i;
                }
                body_diff++;
            }
        }
        int exe_junk = 0;
        for (int i = 0; block >= 0 && i < 0x31C; i++) {
            exe_junk += (exe[block + i] != 0);
        }
        logf("save_header: %d bytes, header block at 0x%X: %d bytes not as Thinker's (zeros, version 12), "
            "exe entry's header %d non-zero bytes; rest of the file: %d bytes differ (first at 0x%X)",
            size_dll, block, header_bad, exe_junk, body_diff, first_diff);
        mismatches += (block < 0) + header_bad + body_diff;
    }
    free(dll);
    free(exe);
    return mismatches;
}

// ---------------------------------------------------------------- batch 28: prototype design

// X_pop() is a dialog: record what it was asked and answer with XPopAnswer
static int XPopAnswer;
static int XPopCount;
static char XPopLabel[32];
static int __cdecl xpop_recorder(LPCSTR label, int(*UNUSED(callback))(void)) {
    XPopCount++;
    strncpy_s(XPopLabel, label ? label : "", _TRUNCATE);
    return XPopAnswer;
}

// NetDaemon::synch(), lock_base() and unlock_base() as recorders: one (kind, id) entry per call,
// kind = synch message type, or 1000 for lock_base and 2000 for unlock_base
struct NetCall {
    int kind;
    int id;
};
static const int MaxNetCalls = 256;
static NetCall NetCalls[MaxNetCalls];
static int NetCallCount;
static int LockBaseAnswer;
static void net_call(int kind, int id) {
    if (NetCallCount < MaxNetCalls) {
        NetCalls[NetCallCount].kind = kind;
        NetCalls[NetCallCount].id = id;
    }
    NetCallCount++;
}
static void __fastcall synch_recorder(void *UNUSED(This), void *UNUSED(edx_unused), int16_t type,
    int id, int UNUSED(id2), int UNUSED(unk), char *UNUSED(data), uint32_t UNUSED(delay),
    int16_t UNUSED(unk2)) {
    net_call(type, id);
}
static int __fastcall lock_base_recorder(void *UNUSED(This), void *UNUSED(edx_unused), int base_id,
    int UNUSED(lock_flags), int UNUSED(unk1), int UNUSED(unk2)) {
    net_call(1000, base_id);
    return LockBaseAnswer;
}
static void __fastcall unlock_base_recorder(void *UNUSED(This), void *UNUSED(edx_unused),
    int base_id) {
    net_call(2000, base_id);
}

static int test_prune_protos() {
    // run the exe's own prune_protos, not the jump into the DLL
    static const uint8_t prune_entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x30 };
    OriginalEntry vanilla_prune(0x0057F4B0, prune_entry);
    JmpStub popp_stub(0x0048C0A0, (void *)popp_recorder);
    JmpStub xpop_stub(0x005BF310, (void *)xpop_recorder);
    JmpStub synch_stub(0x00532E00, (void *)synch_recorder);
    JmpStub lock_stub(0x00530D50, (void *)lock_base_recorder);
    JmpStub unlock_stub(0x00530E50, (void *)unlock_base_recorder);
    if (!vanilla_prune.ok || !popp_stub.ok || !xpop_stub.ok || !synch_stub.ok || !lock_stub.ok
        || !unlock_stub.ok) {
        return 1;
    }
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    const size_t proto_size = sizeof(VehPrototype) * MaxVehProtoNum;
    uint8_t *proto_saved = (uint8_t *)malloc(proto_size);
    uint8_t *proto_o = (uint8_t *)malloc(proto_size);
    memcpy(proto_saved, VehPrototypes, proto_size);
    const size_t parse_size = 2 * sizeof(char256);
    static uint8_t temp_saved[sizeof(char1032)], temp_o[sizeof(char1032)];
    static uint8_t parse_saved[2 * sizeof(char256)], parse_o[2 * sizeof(char256)];
    int gender_saved[2] = { ParseStrGender[0], ParseStrGender[1] };
    int plural_saved[2] = { ParseStrPlurality[0], ParseStrPlurality[1] };
    memcpy(temp_saved, stringTemp, sizeof(char1032));
    memcpy(parse_saved, ParseStrBuffer, parse_size);
    static NetCall calls_o[MaxNetCalls];
    uint8_t status_saved = *FactionsStatus;
    BOOL net_saved = *IsMultiplayerNet;
    uint32_t prefs_saved = *GameMorePreferences;
    int player = *FactionIDCurrent;
    int compared = 0, mismatches = 0, obsoleted = 0, typed = 0, flagged = 0, requeued = 0;
    int asked = 0, kept = 0, told = 0, net_calls = 0;
    char label[96];
    // variant bits: 1 = this faction's obsolete marks and every PROTO_UNK_8/200 flag cleared
    // first, 2 = synch; mode: 0 computer player, 1 human with the auto prune preference,
    // 2 human in a network game, 3 human without either (returns after the first part).
    // The player at the controls answers the question 0, 1 or 2; in a network game lock_base
    // refuses every base on odd answers.
    auto prepare = [&](int f, int variant, int mode, int answer) {
        snap_restore(&base);
        memcpy(VehPrototypes, proto_saved, proto_size);
        if (variant & 1) {
            for (int id = 0; id < MaxVehProtoNum; id++) {
                VehPrototypes[id].obsolete_factions &= ~(1 << f);
                VehPrototypes[id].flags &= ~(PROTO_UNK_8 | PROTO_UNK_200);
            }
        }
        memcpy(stringTemp, temp_saved, sizeof(char1032));
        memcpy(ParseStrBuffer, parse_saved, parse_size);
        ParseStrGender[0] = gender_saved[0]; ParseStrGender[1] = gender_saved[1];
        ParseStrPlurality[0] = plural_saved[0]; ParseStrPlurality[1] = plural_saved[1];
        *FactionsStatus = mode ? (uint8_t)(status_saved | (1 << f))
            : (uint8_t)(status_saved & ~(1 << f));
        *IsMultiplayerNet = mode == 2;
        *GameMorePreferences = mode == 1 ? (prefs_saved | MPREF_BSC_AUTO_PRUNE_OBS_VEH)
            : (prefs_saved & ~MPREF_BSC_AUTO_PRUNE_OBS_VEH);
        XPopAnswer = answer;
        XPopCount = 0;
        XPopLabel[0] = 0;
        PoppAnswer = 0;
        PoppCount = 0;
        PoppLabel[0] = PoppPic[0] = 0;
        LockBaseAnswer = mode == 2 && (answer & 1);
        NetCallCount = 0;
    };
    VehPrototype *protos = (VehPrototype *)proto_saved; // as loaded
    for (int f = 0; f < MaxPlayerNum; f++) {
        // kept prototypes: for a computer player only whether it is basic or typed complete
        // matters (one of each kind); humans also compare ranges with it and flag it: all of them
        int kept_ids[2 * MaxVehProtoFactionNum];
        int kept_count = 0;
        int basic_first = -1, typed_first = -1, untyped_first = -1;
        for (int i = 0; i < 2 * MaxVehProtoFactionNum; i++) {
            int id = i < MaxVehProtoFactionNum ? i : f * MaxVehProtoFactionNum + i - MaxVehProtoFactionNum;
            if ((!f && i >= MaxVehProtoFactionNum) || !(protos[id].flags & PROTO_ACTIVE)) {
                continue;
            }
            kept_ids[kept_count++] = id;
            if (id < MaxVehProtoFactionNum) {
                basic_first = basic_first < 0 ? id : basic_first;
            } else if (protos[id].flags & PROTO_TYPED_COMPLETE) {
                typed_first = typed_first < 0 ? id : typed_first;
            } else {
                untyped_first = untyped_first < 0 ? id : untyped_first;
            }
        }
        for (int k = 0; k < kept_count; k++) {
            int kept_id = kept_ids[k];
            BOOL ai_kept = kept_id == basic_first || kept_id == typed_first
                || kept_id == untyped_first;
            for (int mode = 0; mode < 4; mode++) {
                BOOL human_rules = mode == 1 || mode == 2;
                if (!human_rules && !ai_kept) {
                    continue;
                }
                int answers = (human_rules && f == player) ? 3 : 1;
                for (int variant = 0; variant < 4; variant++) {
                    // other humans: as loaded without synch, and cleared with synch
                    if (human_rules && f != player && (variant == 1 || variant == 2)) {
                        continue;
                    }
                    for (int answer = 0; answer < answers; answer++) {
                        BOOL synch = (variant & 2) != 0;
                        REGRESS_CTX("prune_protos(%d, %d, %d) variant %d mode %d answer %d", f,
                            kept_id, synch, variant, mode, answer);
                        prepare(f, variant, mode, answer);
                        prune_protos_OG(f, kept_id, synch);
                        snap_take(&after_orig);
                        memcpy(proto_o, VehPrototypes, proto_size);
                        memcpy(temp_o, stringTemp, sizeof(char1032));
                        memcpy(parse_o, ParseStrBuffer, parse_size);
                        int xpop_o = XPopCount, popp_o = PoppCount, net_o = NetCallCount;
                        char xpop_label_o[32], popp_label_o[32], popp_pic_o[32];
                        strcpy_s(xpop_label_o, XPopLabel);
                        strcpy_s(popp_label_o, PoppLabel);
                        strcpy_s(popp_pic_o, PoppPic);
                        memcpy(calls_o, NetCalls, sizeof(calls_o));
                        prepare(f, variant, mode, answer);
                        static VehPrototype before[MaxVehProtoNum]; // what both calls started from
                        memcpy(before, VehPrototypes, proto_size);
                        prune_protos(f, kept_id, synch);
                        snap_take(&after_new);
                        compared++;
                        VehPrototype *after = (VehPrototype *)proto_o;
                        for (int id = 0; id < MaxVehProtoNum; id++) {
                            obsoleted += (after[id].obsolete_factions & ~before[id].obsolete_factions
                                & (1 << f)) != 0;
                            typed += (after[id].flags & ~before[id].flags & PROTO_TYPED_COMPLETE) != 0;
                            flagged += (after[id].flags & ~before[id].flags & PROTO_UNK_8) != 0;
                            kept += (before[id].obsolete_factions & ~after[id].obsolete_factions
                                & (1 << f)) != 0;
                        }
                        for (int b = 0; b < base.base_count; b++) {
                            requeued += memcmp(after_orig.bases[b].queue_production_id,
                                base.bases[b].queue_production_id,
                                sizeof(base.bases[b].queue_production_id)) != 0;
                        }
                        asked += xpop_o;
                        told += popp_o;
                        net_calls += net_o;
                        sprintf_s(label, "prune_protos(%d, %d, %d) variant %d mode %d answer %d",
                            f, kept_id, synch, variant, mode, answer);
                        BOOL diff = false;
                        if (xpop_o != XPopCount || strcmp(xpop_label_o, XPopLabel)
                            || popp_o != PoppCount || strcmp(popp_label_o, PoppLabel)
                            || strcmp(popp_pic_o, PoppPic) || net_o != NetCallCount
                            || memcmp(calls_o, NetCalls,
                                sizeof(NetCall) * (net_o < MaxNetCalls ? net_o : MaxNetCalls))) {
                            if (mismatches < MAX_DETAIL) {
                                logf("  %s: orig X_pop %d '%s' popp %d '%s' '%s' net %d; new X_pop %d "
                                    "'%s' popp %d '%s' '%s' net %d", label, xpop_o, xpop_label_o,
                                    popp_o, popp_label_o, popp_pic_o, net_o, XPopCount, XPopLabel,
                                    PoppCount, PoppLabel, PoppPic, NetCallCount);
                            }
                            diff = true;
                        }
                        if (!diff) {
                            diff = snap_differs(&after_orig, &after_new,
                                mismatches < MAX_DETAIL ? label : NULL);
                        }
                        size_t at;
                        if (!diff && (at = first_diff(proto_o, VehPrototypes, proto_size)) != (size_t)-1) {
                            if (mismatches < MAX_DETAIL) {
                                logf("  %s: VehPrototype[%u] +0x%X differs", label,
                                    (unsigned)(at / sizeof(VehPrototype)),
                                    (unsigned)(at % sizeof(VehPrototype)));
                            }
                            diff = true;
                        }
                        if (!diff && (memcmp(temp_o, stringTemp, sizeof(char1032))
                            || memcmp(parse_o, ParseStrBuffer, parse_size))) {
                            if (mismatches < MAX_DETAIL) {
                                logf("  %s: popup text differs: orig '%s' / '%s' new '%s' / '%s'",
                                    label, ((char256 *)parse_o)[0].str, ((char256 *)parse_o)[1].str,
                                    ParseStrBuffer[0].str, ParseStrBuffer[1].str);
                            }
                            diff = true;
                        }
                        if (diff) {
                            mismatches++;
                        }
                    }
                }
            }
        }
    }
    snap_restore(&base);
    memcpy(VehPrototypes, proto_saved, proto_size);
    free(proto_saved);
    free(proto_o);
    memcpy(stringTemp, temp_saved, sizeof(char1032));
    memcpy(ParseStrBuffer, parse_saved, parse_size);
    ParseStrGender[0] = gender_saved[0]; ParseStrGender[1] = gender_saved[1];
    ParseStrPlurality[0] = plural_saved[0]; ParseStrPlurality[1] = plural_saved[1];
    *FactionsStatus = status_saved;
    *IsMultiplayerNet = net_saved;
    *GameMorePreferences = prefs_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("prune_protos: %d compared (original: %d marks obsolete, %d typed complete, %d kept "
        "designs flagged, %d marks removed, %d queues changed, %d questions, %d network popups, "
        "%d net calls), %d mismatches", compared, obsoleted, typed, flagged, kept, requeued,
        asked, told, net_calls, mismatches);
    return mismatches;
}

static int test_consider_designs() {
    // run the exe's own consider_designs, not the jump into the DLL (its propose_proto calls go
    // to the DLL on both sides)
    static const uint8_t consider_entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x3C };
    OriginalEntry vanilla_consider(0x00581260, consider_entry);
    JmpStub popp_stub(0x0048C0A0, (void *)popp_recorder);
    JmpStub xpop_stub(0x005BF310, (void *)xpop_recorder);
    JmpStub synch_stub(0x00532E00, (void *)synch_recorder);
    if (!vanilla_consider.ok || !popp_stub.ok || !xpop_stub.ok || !synch_stub.ok) {
        return 1;
    }
    // selection globals touched by kill()/veh_kill(), same set as the stack_kill test
    static int *const extra_globals[] = { (int *)0x0093928C, (int *)0x00939290, (int *)0x008C6B2C,
        (int *)0x008C6B34, (int *)0x009A64DC };
    const int extra_count = (int)(sizeof(extra_globals) / sizeof(extra_globals[0]));
    int extra_saved[5], extra_o[5];
    for (int i = 0; i < extra_count; i++) {
        extra_saved[i] = *extra_globals[i];
    }
    const size_t bung_size = sizeof(BungedDesign) * MaxPlayerNum * MaxBungedDesignNum;
    const size_t proto_size = sizeof(VehPrototype) * MaxVehProtoNum;
    uint8_t *bung_saved = (uint8_t *)malloc(bung_size);
    uint8_t *bung_o = (uint8_t *)malloc(bung_size);
    uint8_t *proto_saved = (uint8_t *)malloc(proto_size);
    uint8_t *proto_o = (uint8_t *)malloc(proto_size);
    memcpy(bung_saved, BungList, bung_size);
    memcpy(proto_saved, VehPrototypes, proto_size);
    // DesignSuggestAbilities .. DesignSuggestState
    uint8_t *const suggest = (uint8_t *)DesignSuggestAbilities;
    const size_t suggest_size = 0x0093FC58 - 0x0093FC34;
    uint8_t suggest_saved[0x24], suggest_o[0x24];
    memcpy(suggest_saved, suggest, suggest_size);
    const size_t parse_size = 2 * sizeof(char256);
    static uint8_t temp_saved[sizeof(char1032)], temp_o[sizeof(char1032)];
    static uint8_t parse_saved[2 * sizeof(char256)], parse_o[2 * sizeof(char256)];
    int gender_saved[2] = { ParseStrGender[0], ParseStrGender[1] };
    int plural_saved[2] = { ParseStrPlurality[0], ParseStrPlurality[1] };
    memcpy(temp_saved, stringTemp, sizeof(char1032));
    memcpy(parse_saved, ParseStrBuffer, parse_size);
    int new_design_saved = *ProtoIDNewDesign;
    uint8_t status_saved = *FactionsStatus;
    uint32_t rules_saved = *GameRules;
    int un_saved[2] = { ProposalPassCount[PROP_REPEAL_UN_CHARTER],
        ProposalPassCount[PROP_REINSTATE_UN_CHARTER] };
    BOOL draw_saved = VehBattleUnkTgl[0];
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    VehBattleUnkTgl[0] = true; // kill() skips its map redraw while this is set
    snap_take(&base);
    int compared = 0, mismatches = 0, changed_designs = 0, runs_changed = 0, suggested = 0;
    int requeued = 0;
    char label[96];
    // variant 0: as loaded; 1: the design workshop asks for a suggestion (any category); 2: the
    // faction's obsolete marks cleared; 3: a computer player as human, the human as a computer
    // player; 4: the UN Charter repealed and intense rivalry switched
    auto prepare = [&](int f, int variant) {
        snap_restore(&base);
        memcpy(BungList, bung_saved, bung_size);
        memcpy(VehPrototypes, proto_saved, proto_size);
        memcpy(suggest, suggest_saved, suggest_size);
        memcpy(stringTemp, temp_saved, sizeof(char1032));
        memcpy(ParseStrBuffer, parse_saved, parse_size);
        ParseStrGender[0] = gender_saved[0]; ParseStrGender[1] = gender_saved[1];
        ParseStrPlurality[0] = plural_saved[0]; ParseStrPlurality[1] = plural_saved[1];
        *ProtoIDNewDesign = new_design_saved;
        *FactionsStatus = status_saved;
        *GameRules = rules_saved;
        ProposalPassCount[PROP_REPEAL_UN_CHARTER] = un_saved[0];
        ProposalPassCount[PROP_REINSTATE_UN_CHARTER] = un_saved[1];
        for (int i = 0; i < extra_count; i++) {
            *extra_globals[i] = extra_saved[i];
        }
        if (variant == 1) {
            *DesignSuggestState = 1;
            *DesignSuggestCategory = -1;
        } else if (variant == 2) {
            for (int id = 0; id < MaxVehProtoNum; id++) {
                VehPrototypes[id].obsolete_factions &= ~(1 << f);
            }
        } else if (variant == 3) {
            *FactionsStatus ^= (uint8_t)(1 << f);
        } else if (variant == 4) {
            ProposalPassCount[PROP_REPEAL_UN_CHARTER] = un_saved[1] + 1;
            *GameRules ^= RULES_INTENSE_RIVALRY;
        }
        XPopAnswer = 0;
        XPopCount = 0;
        XPopLabel[0] = 0;
        PoppAnswer = 0;
        PoppCount = 0;
        PoppLabel[0] = PoppPic[0] = 0;
        NetCallCount = 0;
    };
    for (int f = 1; f < MaxPlayerNum; f++) {
        for (int variant = 0; variant < 5; variant++) {
            REGRESS_CTX("consider_designs(%d) variant %d", f, variant);
            prepare(f, variant);
            consider_designs_OG(f);
            snap_take(&after_orig);
            memcpy(bung_o, BungList, bung_size);
            memcpy(proto_o, VehPrototypes, proto_size);
            memcpy(suggest_o, suggest, suggest_size);
            memcpy(temp_o, stringTemp, sizeof(char1032));
            memcpy(parse_o, ParseStrBuffer, parse_size);
            int new_design_o = *ProtoIDNewDesign;
            for (int i = 0; i < extra_count; i++) {
                extra_o[i] = *extra_globals[i];
            }
            int xpop_o = XPopCount, popp_o = PoppCount, net_o = NetCallCount;
            prepare(f, variant);
            static VehPrototype before[MaxVehProtoNum]; // what both calls started from
            memcpy(before, VehPrototypes, proto_size);
            consider_designs(f);
            snap_take(&after_new);
            compared++;
            int changed = 0;
            for (int id = 0; id < MaxVehProtoNum; id++) {
                changed += memcmp(&before[id], &((VehPrototype *)proto_o)[id],
                    sizeof(VehPrototype)) != 0;
            }
            changed_designs += changed;
            runs_changed += changed != 0;
            suggested += memcmp(suggest_o, suggest_saved, suggest_size) != 0;
            for (int b = 0; b < base.base_count; b++) {
                requeued += memcmp(after_orig.bases[b].queue_production_id,
                    base.bases[b].queue_production_id, sizeof(base.bases[b].queue_production_id)) != 0;
            }
            sprintf_s(label, "consider_designs(%d) variant %d", f, variant);
            BOOL diff = false;
            if (xpop_o != XPopCount || popp_o != PoppCount || net_o != NetCallCount) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: orig X_pop %d popp %d net %d; new X_pop %d popp %d net %d", label,
                        xpop_o, popp_o, net_o, XPopCount, PoppCount, NetCallCount);
                }
                diff = true;
            }
            if (!diff) {
                diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
            }
            size_t at;
            if (!diff && (at = first_diff(proto_o, VehPrototypes, proto_size)) != (size_t)-1) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: VehPrototype[%u] +0x%X differs (orig %02X new %02X)", label,
                        (unsigned)(at / sizeof(VehPrototype)), (unsigned)(at % sizeof(VehPrototype)),
                        proto_o[at], ((uint8_t *)VehPrototypes)[at]);
                }
                diff = true;
            }
            if (!diff && (at = first_diff(bung_o, BungList, bung_size)) != (size_t)-1) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: BungList byte %u differs", label, (unsigned)at);
                }
                diff = true;
            }
            if (!diff && (at = first_diff(suggest_o, suggest, suggest_size)) != (size_t)-1) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: design suggestion +0x%X differs", label, (unsigned)at);
                }
                diff = true;
            }
            if (!diff && new_design_o != *ProtoIDNewDesign) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: new design id orig %d new %d", label, new_design_o,
                        *ProtoIDNewDesign);
                }
                diff = true;
            }
            if (!diff && (memcmp(temp_o, stringTemp, sizeof(char1032))
                || memcmp(parse_o, ParseStrBuffer, parse_size))) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: popup text differs", label);
                }
                diff = true;
            }
            for (int i = 0; i < extra_count && !diff; i++) {
                if (extra_o[i] != *extra_globals[i]) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  %s: selection global %08X differs", label,
                            (unsigned)extra_globals[i]);
                    }
                    diff = true;
                }
            }
            if (diff) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    memcpy(BungList, bung_saved, bung_size);
    memcpy(VehPrototypes, proto_saved, proto_size);
    memcpy(suggest, suggest_saved, suggest_size);
    memcpy(stringTemp, temp_saved, sizeof(char1032));
    memcpy(ParseStrBuffer, parse_saved, parse_size);
    ParseStrGender[0] = gender_saved[0]; ParseStrGender[1] = gender_saved[1];
    ParseStrPlurality[0] = plural_saved[0]; ParseStrPlurality[1] = plural_saved[1];
    *ProtoIDNewDesign = new_design_saved;
    *FactionsStatus = status_saved;
    *GameRules = rules_saved;
    ProposalPassCount[PROP_REPEAL_UN_CHARTER] = un_saved[0];
    ProposalPassCount[PROP_REINSTATE_UN_CHARTER] = un_saved[1];
    for (int i = 0; i < extra_count; i++) {
        *extra_globals[i] = extra_saved[i];
    }
    VehBattleUnkTgl[0] = draw_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    free(bung_saved); free(bung_o); free(proto_saved); free(proto_o);
    logf("consider_designs: %d compared (original: %d prototype slots changed in %d runs, %d "
        "suggestions recorded, %d base queues changed), %d mismatches", compared,
        changed_designs, runs_changed, suggested, requeued, mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 28: landmarks, map view caches

static int test_mon_winning() {
    typedef void mon_ii(int, int);
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0xA1, 0xD4, 0x64 };
    const struct { const char *name; uint32_t address; int record_id; mon_ii *repl; } fns[] = {
        { "mon_winning_unify", 0x00477940, MONUMENT_WINNING_UNIFY, mon_winning_unify },
        { "mon_winning_trans", 0x004779D0, MONUMENT_WINNING_TRANS, mon_winning_trans },
    };
    uint8_t *saved = (uint8_t *)malloc(MonumentTableSize);
    uint8_t *after_o = (uint8_t *)malloc(MonumentTableSize);
    memcpy(saved, MonumentTable, MonumentTableSize);
    int compared = 0, mismatches = 0, recorded = 0;
    for (const auto &fn : fns) {
        for (int f = 0; f < MaxPlayerNum; f++) {
            if (f == *FactionIDCurrent) {
                continue; // the player at the controls gets the monument screen
            }
            uint32_t rule_flags = Players[f].rule_flags;
            // as loaded; the record clear for everyone; the record already held; alien flag flipped
            for (int variant = 0; variant < 4; variant++) {
                auto prepare = [&]() {
                    memcpy(MonumentTable, saved, MonumentTableSize);
                    Players[f].rule_flags = rule_flags ^ (variant == 3 ? RFLAG_ALIEN : 0);
                    if (variant == 1 || variant == 3) {
                        for (int g = 0; g < MaxPlayerNum; g++) {
                            Monuments[g].records[fn.record_id].achieved = false;
                        }
                    } else if (variant == 2) {
                        Monuments[f].records[fn.record_id].achieved = true;
                    }
                };
                for (int winner = -1; winner < 4; winner += 4) {
                    prepare();
                    {
                        OriginalEntry orig(fn.address, entry);
                        ((mon_ii *)fn.address)(f, winner);
                    }
                    memcpy(after_o, MonumentTable, MonumentTableSize);
                    prepare();
                    recorded += memcmp(after_o, MonumentTable, MonumentTableSize) != 0;
                    fn.repl(f, winner);
                    compared++;
                    size_t d = first_diff(after_o, MonumentTable, MonumentTableSize);
                    if (d != (size_t)-1) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  %s(%d, %d) variant %d: monument byte %u differs (orig %02X new %02X)",
                                fn.name, f, winner, variant, (unsigned)d, after_o[d], MonumentTable[d]);
                        }
                        mismatches++;
                    }
                }
            }
            Players[f].rule_flags = rule_flags;
        }
    }
    memcpy(MonumentTable, saved, MonumentTableSize);
    free(saved);
    free(after_o);
    logf("mon_winning: %d compared (%d changed the table in the original), %d mismatches", compared,
        recorded, mismatches);
    return mismatches;
}

static int test_energy_limit() {
    static const uint8_t entry[6] = { 0xB8, 0x0A, 0x00, 0x00, 0x00, 0xC3 };
    int compared = 0, mismatches = 0;
    for (int f = -1; f <= MaxPlayerNum; f++) {
        int orig_value;
        {
            OriginalEntry orig(0x00445440, entry);
            orig_value = ((func_i *)0x00445440)(f);
        }
        int new_value = energy_limit(f);
        compared++;
        if (orig_value != new_value) {
            if (mismatches < MAX_DETAIL) {
                logf("  energy_limit(%d): orig %d new %d", f, orig_value, new_value);
            }
            mismatches++;
        }
    }
    logf("energy_limit: %d compared, %d mismatches", compared, mismatches);
    return mismatches;
}

static const uint32_t LandmarkReliefBits = BIT2_CRATER | BIT2_VOLCANO | BIT2_CANYON | BIT2_MESA | BIT2_RIDGE;

static void landmark_tables(uint8_t *tables[5]) {
    tables[0] = LandmarkAltCrater;
    tables[1] = LandmarkAltVolcano;
    tables[2] = LandmarkAltCanyon;
    tables[3] = LandmarkAltMesa;
    tables[4] = LandmarkAltRidge;
}

// Tile index to coordinates (the map holds every other x on a row).
static void landmark_tile_xy(uint32_t i, int *x, int *y) {
    *y = (int)(i / *MapLongitude);
    *x = (int)(i % *MapLongitude) * 2 + (*y & 1);
}

// Mark a relief landmark tile as deleted (code -1 - code), as delete_landmark() leaves it.
static BOOL landmark_mark_deleted(Map *tile) {
    int code = (int)tile->bit2 >> 24;
    if (!(tile->bit2 & LandmarkReliefBits) || code < 0) {
        return false;
    }
    tile->bit2 = ((uint32_t)(-1 - code) << 24) | (tile->bit2 & 0xFFFFFF);
    return true;
}

static int test_fixup_landmarks() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08 };
    uint8_t *tables[5];
    landmark_tables(tables);
    uint8_t saved[5][LandmarkAltSize], after_o[5][LandmarkAltSize];
    for (int t = 0; t < 5; t++) {
        memcpy(saved[t], tables[t], LandmarkAltSize);
    }
    uint32_t *bit2_saved = (uint32_t *)malloc(*MapArea * sizeof(uint32_t));
    for (uint32_t i = 0; i < *MapArea; i++) {
        bit2_saved[i] = (*MapTiles)[i].bit2;
    }
    int compared = 0, mismatches = 0, cleared = 0;
    for (int variant = 0; variant < 3; variant++) {
        // as loaded; every other relief landmark tile deleted; every relief landmark tile deleted
        int deleted = 0;
        for (uint32_t i = 0; i < *MapArea; i++) {
            Map *tile = &(*MapTiles)[i];
            tile->bit2 = bit2_saved[i];
            if (variant == 2 || (variant == 1 && i % 2)) {
                landmark_mark_deleted(tile);
            }
            deleted += tile->bit2 && (int)tile->bit2 >> 24 < 0;
        }
        for (int t = 0; t < 5; t++) {
            memset(tables[t], 0x5A, LandmarkAltSize);
        }
        {
            OriginalEntry orig(0x00592940, entry);
            ((func_void *)0x00592940)();
        }
        for (int t = 0; t < 5; t++) {
            memcpy(after_o[t], tables[t], LandmarkAltSize);
            memset(tables[t], 0xA5, LandmarkAltSize);
        }
        fixup_landmarks();
        compared++;
        for (int t = 0; t < 5; t++) {
            size_t d = first_diff(after_o[t], tables[t], LandmarkAltSize);
            if (d != (size_t)-1) {
                if (mismatches < MAX_DETAIL) {
                    logf("  fixup_landmarks variant %d: table %d entry %u differs (orig %02X new %02X)",
                        variant, t, (unsigned)d, after_o[t][d], tables[t][d]);
                }
                mismatches++;
            }
        }
        int zeroed = 0;
        uint8_t *defaults[5] = { LandmarkAltCraterDefault, LandmarkAltVolcanoDefault,
            LandmarkAltCanyonDefault, LandmarkAltMesaDefault, LandmarkAltRidgeDefault };
        for (int t = 0; t < 5; t++) {
            for (int e = 0; e < LandmarkAltSize; e++) {
                zeroed += defaults[t][e] && !after_o[t][e];
            }
        }
        logf("  fixup_landmarks variant %d: %d tiles with a negative code, %d entries zeroed by the original",
            variant, deleted, zeroed);
        cleared += zeroed;
    }
    for (uint32_t i = 0; i < *MapArea; i++) {
        (*MapTiles)[i].bit2 = bit2_saved[i];
    }
    for (int t = 0; t < 5; t++) {
        memcpy(tables[t], saved[t], LandmarkAltSize);
    }
    free(bit2_saved);
    logf("fixup_landmarks: %d compared (%d entries zeroed in all), %d mismatches", compared, cleared,
        mismatches);
    return mismatches;
}

static int test_delete_landmark() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x51, 0xA1, 0xF0 };
    const uint32_t cache_size = *MapArea * 12;
    uint8_t *tables[5];
    landmark_tables(tables);
    uint8_t saved[5][LandmarkAltSize], after_o[5][LandmarkAltSize];
    for (int t = 0; t < 5; t++) {
        memcpy(saved[t], tables[t], LandmarkAltSize);
    }
    uint32_t bitfield_saved = *UnkBitfield1;
    int unk_saved[MaxMapWinNum], unk_o[MaxMapWinNum];
    uint8_t *cache_saved[MaxMapWinNum] = {}, *cache_o[MaxMapWinNum] = {};
    int views = 0;
    for (int w = 0; w < MaxMapWinNum; w++) {
        unk_saved[w] = MapWinList[w]->unk_1;
        if (MapWinList[w]->draw_toggle_a && MapWinList[w]->tile_cache) {
            cache_saved[w] = (uint8_t *)malloc(cache_size);
            cache_o[w] = (uint8_t *)malloc(cache_size);
            memcpy(cache_saved[w], MapWinList[w]->tile_cache, cache_size);
            views++;
        }
    }
    int compared = 0, mismatches = 0, relief = 0, changed = 0;
    for (int pass = 0; pass < 2; pass++) {
        // pass 0 as loaded, pass 1 with the tile's landmark already deleted
        for (uint32_t i = 0; i < *MapArea; i++) {
            Map *tile = &(*MapTiles)[i];
            const uint32_t bit2_saved = tile->bit2;
            BOOL is_relief = (bit2_saved & LandmarkReliefBits) != 0;
            if (!is_relief && (pass || i % (bit2_saved ? 13 : 97))) {
                continue; // every relief tile, a sample of the others
            }
            int x, y;
            landmark_tile_xy(i, &x, &y);
            auto prepare = [&]() {
                tile->bit2 = bit2_saved;
                if (pass) {
                    landmark_mark_deleted(tile);
                }
                for (int t = 0; t < 5; t++) {
                    memcpy(tables[t], saved[t], LandmarkAltSize);
                }
                *UnkBitfield1 = bitfield_saved;
                for (int w = 0; w < MaxMapWinNum; w++) {
                    MapWinList[w]->unk_1 = unk_saved[w];
                    if (cache_saved[w]) {
                        memcpy(MapWinList[w]->tile_cache, cache_saved[w], cache_size);
                    }
                }
            };
            prepare();
            {
                OriginalEntry orig(0x005927D0, entry);
                ((func_xy *)0x005927D0)(x, y);
            }
            uint32_t bit2_o = tile->bit2, bitfield_o = *UnkBitfield1;
            for (int t = 0; t < 5; t++) {
                memcpy(after_o[t], tables[t], LandmarkAltSize);
            }
            for (int w = 0; w < MaxMapWinNum; w++) {
                unk_o[w] = MapWinList[w]->unk_1;
                if (cache_o[w]) {
                    memcpy(cache_o[w], MapWinList[w]->tile_cache, cache_size);
                }
            }
            prepare();
            changed += bit2_o != tile->bit2;
            delete_landmark(x, y);
            compared++;
            relief += is_relief;
            char what[96] = "";
            if (bit2_o != tile->bit2) {
                sprintf_s(what, "bit2 %08X vs %08X", bit2_o, tile->bit2);
            } else if (bitfield_o != *UnkBitfield1) {
                sprintf_s(what, "UnkBitfield1 %08X vs %08X", bitfield_o, *UnkBitfield1);
            }
            for (int t = 0; t < 5 && !what[0]; t++) {
                size_t d = first_diff(after_o[t], tables[t], LandmarkAltSize);
                if (d != (size_t)-1) {
                    sprintf_s(what, "table %d entry %u: %02X vs %02X", t, (unsigned)d, after_o[t][d],
                        tables[t][d]);
                }
            }
            for (int w = 0; w < MaxMapWinNum && !what[0]; w++) {
                if (unk_o[w] != MapWinList[w]->unk_1) {
                    sprintf_s(what, "view %d unk_1 %d vs %d", w, unk_o[w], MapWinList[w]->unk_1);
                } else if (cache_o[w]) {
                    size_t d = first_diff(cache_o[w], MapWinList[w]->tile_cache, cache_size);
                    if (d != (size_t)-1) {
                        sprintf_s(what, "view %d cache byte %u: %02X vs %02X", w, (unsigned)d,
                            cache_o[w][d], MapWinList[w]->tile_cache[d]);
                    }
                }
            }
            if (what[0]) {
                if (mismatches < MAX_DETAIL) {
                    logf("  delete_landmark(%d, %d) pass %d (bit2 %08X): %s", x, y, pass, bit2_saved, what);
                }
                mismatches++;
            }
            tile->bit2 = bit2_saved;
        }
    }
    for (int t = 0; t < 5; t++) {
        memcpy(tables[t], saved[t], LandmarkAltSize);
    }
    *UnkBitfield1 = bitfield_saved;
    for (int w = 0; w < MaxMapWinNum; w++) {
        MapWinList[w]->unk_1 = unk_saved[w];
        if (cache_saved[w]) {
            memcpy(MapWinList[w]->tile_cache, cache_saved[w], cache_size);
            free(cache_saved[w]);
            free(cache_o[w]);
        }
    }
    logf("delete_landmark: %d compared (%d on relief landmark tiles, %d changed the tile in the original, "
        "%d views in use), %d mismatches", compared, relief, changed, views, mismatches);
    return mismatches;
}

// Bytes 0 to 9 of every 12-byte cache entry (terrain_fixup() leaves 10 and 11 as allocated).
static size_t tile_cache_diff(const uint8_t *a, const uint8_t *b) {
    for (uint32_t i = 0; i < *MapArea; i++) {
        size_t d = first_diff(a + i * 12, b + i * 12, 10);
        if (d != (size_t)-1) {
            return i * 12 + d;
        }
    }
    return (size_t)-1;
}

static int test_terrain_fixup() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x56, 0x8B, 0x75 };
    typedef void func_win(void *);
    struct FakeWin {
        void *vtable;
        uint8_t *tile_cache;
    };
    const uint8_t climate_saved = (*MapTiles)->climate;
    const uint8_t climates[] = { climate_saved, 0x00, 0xD0, 0xFF, 0x25, 0x90 };
    int compared = 0, mismatches = 0;
    for (uint8_t climate : climates) {
        (*MapTiles)->climate = climate;
        for (int start = 0; start < 2; start++) {
            // no cache yet; an old cache to free first
            FakeWin win_o = { NULL, start ? (uint8_t *)_malloc(64) : NULL };
            FakeWin win_n = { NULL, start ? (uint8_t *)_malloc(64) : NULL };
            {
                OriginalEntry orig(0x004711A0, entry);
                ((func_win *)0x004711A0)(&win_o);
            }
            terrain_fixup((MapWin *)&win_n);
            compared++;
            size_t d = (win_o.tile_cache && win_n.tile_cache)
                ? tile_cache_diff(win_o.tile_cache, win_n.tile_cache) : 0;
            if (d != (size_t)-1) {
                if (mismatches < MAX_DETAIL) {
                    logf("  terrain_fixup climate %02X start %d: cache byte %d differs (orig %02X new %02X)",
                        climate, start, (int)d, win_o.tile_cache ? win_o.tile_cache[d] : 0,
                        win_n.tile_cache ? win_n.tile_cache[d] : 0);
                }
                mismatches++;
            }
            _free(win_o.tile_cache);
            _free(win_n.tile_cache);
        }
    }
    (*MapTiles)->climate = climate_saved;
    {
        OriginalEntry orig(0x004711A0, entry);
        ((func_win *)0x004711A0)(NULL);
    }
    terrain_fixup(NULL);
    compared++;
    logf("terrain_fixup: %d compared (map area %u), %d mismatches", compared, *MapArea, mismatches);
    return mismatches;
}

static int test_mapwin_terrain_fixup() {
    static const uint8_t entry[6] = { 0x56, 0xBE, 0x3C, 0x3C, 0x7D, 0x00 };
    static const uint8_t entry_tf[6] = { 0x55, 0x8B, 0xEC, 0x56, 0x8B, 0x75 };
    const uint32_t cache_size = *MapArea * 12;
    const int view_count = MaxMapWinNum + 3;
    MapWin *views[MaxMapWinNum + 3];
    for (int w = 0; w < MaxMapWinNum; w++) {
        views[w] = MapWinList[w];
    }
    views[MaxMapWinNum] = MapWinUnk;
    views[MaxMapWinNum + 1] = MapWinUnk2;
    views[MaxMapWinNum + 2] = BaseMapWin;
    uint8_t *saved[MaxMapWinNum + 3] = {}, *after_o[MaxMapWinNum + 3] = {};
    for (int w = 0; w < view_count; w++) {
        if (views[w]->tile_cache) {
            saved[w] = (uint8_t *)malloc(cache_size);
            memcpy(saved[w], views[w]->tile_cache, cache_size);
        }
        after_o[w] = (uint8_t *)malloc(cache_size);
    }
    // mark the caches so a rebuilt one shows, on both runs
    auto mark = [&]() {
        for (int w = 0; w < view_count; w++) {
            if (views[w]->tile_cache) {
                memset(views[w]->tile_cache, 0x33, cache_size);
            }
        }
    };
    int compared = 0, mismatches = 0, rebuilt = 0;
    BOOL null_o[MaxMapWinNum + 3];
    mark();
    {
        OriginalEntry orig(0x00471240, entry);
        OriginalEntry orig_tf(0x004711A0, entry_tf);
        ((func_void *)0x00471240)();
    }
    for (int w = 0; w < view_count; w++) {
        null_o[w] = !views[w]->tile_cache;
        if (!null_o[w]) {
            memcpy(after_o[w], views[w]->tile_cache, cache_size);
            rebuilt += after_o[w][0] != 0x33;
        }
    }
    mark();
    mapwin_terrain_fixup();
    for (int w = 0; w < view_count; w++) {
        compared++;
        size_t d = (size_t)-1;
        if (null_o[w] != !views[w]->tile_cache) {
            d = 0;
        } else if (!null_o[w]) {
            d = tile_cache_diff(after_o[w], views[w]->tile_cache);
        }
        if (d != (size_t)-1) {
            if (mismatches < MAX_DETAIL) {
                logf("  mapwin_terrain_fixup view %d: cache byte %d differs (orig null %d, new null %d)", w,
                    (int)d, null_o[w], !views[w]->tile_cache);
            }
            mismatches++;
        }
    }
    // the rebuilt caches are valid as they are (all heights to be worked out again); put back the
    // ones that were not rebuilt
    for (int w = 0; w < view_count; w++) {
        if (saved[w] && views[w]->tile_cache) {
            memcpy(views[w]->tile_cache, saved[w], cache_size);
        }
        free(saved[w]);
        free(after_o[w]);
    }
    logf("mapwin_terrain_fixup: %d views compared (%d rebuilt by the original), %d mismatches", compared,
        rebuilt, mismatches);
    return mismatches;
}

// Tiles without a base for base_init(): land and sea, own territory, others' and unclaimed.
static int base_init_tiles(int faction_id, int (*tiles)[2], int max) {
    int count = 0, land = 0, sea = 0, own = 0;
    const uint32_t step = 97 + faction_id * 2;
    for (uint32_t n = 0, i = faction_id * 31; n < *MapArea && count < max; n++, i = (i + step) % *MapArea) {
        Map *tile = &(*MapTiles)[i];
        if (tile->bit & BIT_BASE_IN_TILE) {
            continue;
        }
        int x, y;
        landmark_tile_xy(i, &x, &y);
        BOOL ocean = is_ocean(x, y);
        BOOL mine = tile->territory == faction_id;
        if ((ocean && sea >= max / 3) || (!ocean && land >= max - max / 3) || (mine && own >= max / 2)) {
            continue;
        }
        tiles[count][0] = x;
        tiles[count][1] = y;
        count++;
        sea += ocean;
        land += !ocean;
        own += mine;
    }
    return count;
}

static int test_base_init() {
    typedef int func_base_init(uint32_t, int, int);
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C };
    int *const replay_offset = (int *)0x009469F8;
    uint8_t *const replay = (uint8_t *)0x00950324;
    const int replay_saved = *replay_offset;
    uint8_t replay_o[8];
    uint8_t globals_saved[BaseGlobalsSize], globals_o[BaseGlobalsSize];
    memcpy(globals_saved, BaseGlobals, BaseGlobalsSize);
    const int turn_saved = *TurnCurrentNum;
    const BOOL net_saved = *IsMultiplayerNet;
    const int selected_saved = *BaseIDCurrentSelected;
    const int compute_saved = *BaseComputeID;
    Base *const current_saved = *BaseCurrent;
    const uint32_t bitfield_saved = *UnkBitfield1;
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    const int count = base.base_count_global;
    if (count >= MaxBaseNum) {
        logf("base_init: base table full, skipped");
        snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
        return 0;
    }
    Base slot_saved = Bases[count]; // the slot the new base goes into
    Base slot_o;
    int compared = 0, mismatches = 0, first_bases = 0, sea_bases = 0;
    char label[96];
    // variant 0: as loaded; 1: turn 0; 2: the faction's first base; 3: first base in a network
    // game; 4: first base at turn 80; 5: first base at turn 0; 6: base table full
    auto run = [&](BOOL orig, int f, int x, int y, int variant, int *ret, int *rand_next) {
        snap_restore(&base);
        Bases[count] = slot_saved;
        memset(&Bases[count].facilities_built[9], 0x5A, 3); // left as they were by the original
        *replay_offset = replay_saved;
        memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
        *BaseIDCurrentSelected = selected_saved;
        *BaseComputeID = compute_saved;
        *BaseCurrent = current_saved;
        *UnkBitfield1 = bitfield_saved;
        *TurnCurrentNum = (variant == 1 || variant == 5) ? 0 : (variant == 4 ? 80 : turn_saved);
        *IsMultiplayerNet = variant == 3;
        if (variant >= 2 && variant <= 5) {
            PlayersData[f].current_num_bases = 0;
        } else if (variant == 6) {
            *BaseCurrentCount = MaxBaseNum;
        }
        reseed((uint32_t)(f * 977 + x * 7 + y + variant));
        if (orig) {
            OriginalEntry original(0x004E4B80, entry);
            *ret = ((func_base_init *)0x004E4B80)(f, x, y);
        } else {
            *ret = base_init(f, x, y);
        }
        *rand_next = _rand();
    };
    static int tiles[8][2];
    for (int f = 1; f < MaxPlayerNum; f++) {
        int tile_count = base_init_tiles(f, tiles, 6);
        for (int t = 0; t < tile_count; t++) {
            int x = tiles[t][0], y = tiles[t][1];
            for (int v = 0; v < 7; v++) {
                if (v == 6 && (f != 1 || t)) {
                    continue;
                }
                int ret_o, ret_n, rand_o, rand_n;
                REGRESS_CTX("base_init(%d, %d, %d) variant %d", f, x, y, v);
                run(true, f, x, y, v, &ret_o, &rand_o);
                snap_take(&after_orig);
                slot_o = Bases[count];
                int replay_end_o = *replay_offset;
                memcpy(replay_o, replay + replay_saved, sizeof(replay_o));
                memcpy(globals_o, BaseGlobals, BaseGlobalsSize);
                int selected_o = *BaseIDCurrentSelected, compute_o = *BaseComputeID;
                Base *current_o = *BaseCurrent;
                uint32_t bitfield_o = *UnkBitfield1;
                run(false, f, x, y, v, &ret_n, &rand_n);
                snap_take(&after_new);
                compared++;
                if (ret_o >= 0) {
                    first_bases += v >= 2;
                    sea_bases += is_ocean(x, y);
                }
                sprintf_s(label, "base_init(%d, %d, %d) variant %d", f, x, y, v);
                BOOL diff = false;
                if (ret_o != ret_n || rand_o != rand_n || replay_end_o != *replay_offset
                    || selected_o != *BaseIDCurrentSelected || compute_o != *BaseComputeID
                    || current_o != *BaseCurrent || bitfield_o != *UnkBitfield1
                    || (replay_end_o != replay_saved && memcmp(replay_o, replay + replay_saved, sizeof(replay_o)))) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  %s: orig %d rand %d replay %d selected %d compute %d bits %08X; new %d "
                            "rand %d replay %d selected %d compute %d bits %08X", label, ret_o, rand_o,
                            replay_end_o, selected_o, compute_o, bitfield_o, ret_n, rand_n,
                            *replay_offset, *BaseIDCurrentSelected, *BaseComputeID, *UnkBitfield1);
                    }
                    diff = true;
                }
                if (!diff) {
                    diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
                }
                size_t at;
                if (!diff && (at = first_diff(&slot_o, &Bases[count], sizeof(Base))) != (size_t)-1) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  %s: new Base +0x%X differs (orig %02X new %02X)", label, (unsigned)at,
                            ((uint8_t *)&slot_o)[at], ((uint8_t *)&Bases[count])[at]);
                    }
                    diff = true;
                }
                if (!diff && base_globals_differ(globals_o, BaseGlobals, mismatches < MAX_DETAIL ? label : NULL)) {
                    diff = true;
                }
                if (diff) {
                    mismatches++;
                }
            }
        }
    }
    snap_restore(&base);
    Bases[count] = slot_saved;
    *replay_offset = replay_saved;
    memcpy(BaseGlobals, globals_saved, BaseGlobalsSize);
    *BaseIDCurrentSelected = selected_saved;
    *BaseComputeID = compute_saved;
    *BaseCurrent = current_saved;
    *UnkBitfield1 = bitfield_saved;
    *TurnCurrentNum = turn_saved;
    *IsMultiplayerNet = net_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("base_init: %d compared (by the original: %d first bases, %d at sea), %d mismatches", compared,
        first_bases, sea_bases, mismatches);
    return mismatches;
}

// battle_fight_2() fights the battle: record what battle_fight() hands it instead
static int BattleFight2Args[7];
static int BattleFight2Count;
static int __cdecl battle_fight_2_recorder(int veh_id, int offset, int x, int y, BOOL is_arty, int unk1,
                                           int unk2) {
    const int args[7] = { veh_id, offset, x, y, is_arty, unk1, unk2 };
    memcpy(BattleFight2Args, args, sizeof(args));
    BattleFight2Count++;
    return veh_id * 3 + offset; // something the caller passes back
}

static int test_battle_fight() {
    typedef int func_bf(int, int, BOOL, int, int);
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x53, 0x8B, 0x5D };
    JmpStub stub(0x00506AF0, (void *)battle_fight_2_recorder);
    if (!stub.ok) {
        return 1;
    }
    const BOOL flat_saved = *MapIsFlat;
    int compared = 0, mismatches = 0, wrapped = 0;
    for (int flat = 0; flat < 2; flat++) {
        *MapIsFlat = flat ? !flat_saved : flat_saved;
        for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id += (Vehs[veh_id].x < 3
                || Vehs[veh_id].x >= *MapLongitudeBounds - 3) ? 1 : 7) {
            for (int arty = 0; arty < 2; arty++) {
                for (int offset = 0; offset < (arty ? 25 : 9); offset++) {
                    int args_o[7], ret_o, ret_n;
                    BattleFight2Count = 0;
                    {
                        OriginalEntry orig(0x00506A60, entry);
                        ret_o = ((func_bf *)0x00506A60)(veh_id, offset, arty, veh_id & 1, offset & 2);
                    }
                    memcpy(args_o, BattleFight2Args, sizeof(args_o));
                    int count_o = BattleFight2Count;
                    BattleFight2Count = 0;
                    ret_n = battle_fight(veh_id, offset, arty, veh_id & 1, offset & 2);
                    compared++;
                    int x_raw = Vehs[veh_id].x + (arty ? RadiusOffsetX[offset] : RadiusBaseX[offset]);
                    wrapped += x_raw != args_o[2];
                    if (ret_o != ret_n || count_o != BattleFight2Count
                        || memcmp(args_o, BattleFight2Args, sizeof(args_o))) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  battle_fight(%d, %d, %d) flat %d: orig (%d, %d) ret %d, new (%d, %d) ret %d",
                                veh_id, offset, arty, *MapIsFlat, args_o[2], args_o[3], ret_o,
                                BattleFight2Args[2], BattleFight2Args[3], ret_n);
                        }
                        mismatches++;
                    }
                }
            }
        }
    }
    *MapIsFlat = flat_saved;
    logf("battle_fight: %d compared (%d wrapped x), %d mismatches", compared, wrapped, mismatches);
    return mismatches;
}

static int test_compute_odds() {
    typedef int func_odds(int, uint32_t, int, int, int);
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x20 };
    const int odds_values[] = { 1, 7, 100, 12345 };
    int compared = 0, mismatches = 0, air = 0, at_base = 0, changed = 0;
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        const uint32_t flags_saved = PlayersData[f].flags;
        // attackers: up to four units of each triad of the faction
        int attackers[12], atk_count = 0, per_triad[3] = {};
        for (int v = 0; v < *VehCurrentCount && atk_count < 12; v++) {
            if (Vehs[v].faction_id != f) {
                continue;
            }
            int t = Chassis[VehPrototypes[Vehs[v].proto_id].chassis_id].triad;
            if (t <= TRIAD_AIR && per_triad[t] < 4) {
                per_triad[t]++;
                attackers[atk_count++] = v;
            }
        }
        for (int a = 0; a < atk_count; a++) {
            int veh_id_atk = attackers[a];
            BOOL is_air = Chassis[VehPrototypes[Vehs[veh_id_atk].proto_id].chassis_id].triad == TRIAD_AIR;
            for (int veh_id_def = (int)(f * 13 + a) % 17; veh_id_def < *VehCurrentCount; veh_id_def += 17) {
                if (Vehs[veh_id_def].faction_id == f) {
                    continue;
                }
                int base_at_def = base_at(Vehs[veh_id_def].x, Vehs[veh_id_def].y);
                const int base_ids[] = { -2, -1, base_at_def };
                for (int b = 0; b < 3; b++) {
                    if (b == 2 && base_at_def < 0) {
                        continue;
                    }
                    for (int flag = 0; flag < 2; flag++) {
                        PlayersData[f].flags = flag ? flags_saved ^ PFLAG_UNK_80000 : flags_saved;
                        for (int odds : odds_values) {
                            int ret_o, ret_n;
                            {
                                OriginalEntry orig(0x00565F20, entry);
                                ret_o = ((func_odds *)0x00565F20)(odds, f, veh_id_atk, veh_id_def, base_ids[b]);
                            }
                            ret_n = compute_odds(odds, f, veh_id_atk, veh_id_def, base_ids[b]);
                            compared++;
                            air += is_air;
                            at_base += base_at_def >= 0 && b != 1;
                            changed += ret_o != odds;
                            if (ret_o != ret_n) {
                                if (mismatches < MAX_DETAIL) {
                                    logf("  compute_odds(%d, %u, %d, %d, %d) flag %d: orig %d new %d", odds, f,
                                        veh_id_atk, veh_id_def, base_ids[b], flag, ret_o, ret_n);
                                }
                                mismatches++;
                            }
                        }
                    }
                }
            }
        }
        PlayersData[f].flags = flags_saved;
    }
    logf("compute_odds: %d compared (%d air attackers, %d at a base, %d changed the odds), %d mismatches",
        compared, air, at_base, changed, mismatches);
    return mismatches;
}

// name_base() was redirected upstream ("added some minor randomization for sea bases"); base_init()
// calls it on both sides, so check it against the exe's own code separately.
static int test_name_base() {
    typedef void func_name_base(uint32_t, LPSTR, BOOL, BOOL);
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x54 };
    const uint32_t offsets[] = { 0, 1, 2, 3, 5, 8, 13, 21, 34, 60, 120 };
    const int gender_saved = *GenderDefault, plural_saved = *PluralityDefault;
    int compared = 0, mismatches = 0, generic = 0;
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        const uint32_t land_saved = PlayersData[f].base_name_offset;
        const uint32_t sea_saved = PlayersData[f].base_sea_name_offset;
        for (uint32_t offset : offsets) {
            for (int sea = 0; sea < 2; sea++) {
                for (int is_final = 0; is_final < 2; is_final++) {
                    char name_o[32], name_n[32];
                    memset(name_o, 0x77, sizeof(name_o));
                    memset(name_n, 0x77, sizeof(name_n));
                    PlayersData[f].base_name_offset = offset;
                    PlayersData[f].base_sea_name_offset = offset / 2;
                    *GenderDefault = 9;
                    *PluralityDefault = 9;
                    {
                        OriginalEntry orig(0x004E4090, entry);
                        ((func_name_base *)0x004E4090)(f, name_o, is_final, sea);
                    }
                    uint32_t land_o = PlayersData[f].base_name_offset;
                    uint32_t sea_o = PlayersData[f].base_sea_name_offset;
                    int gender_o = *GenderDefault, plural_o = *PluralityDefault;
                    PlayersData[f].base_name_offset = offset;
                    PlayersData[f].base_sea_name_offset = offset / 2;
                    *GenderDefault = 9;
                    *PluralityDefault = 9;
                    name_base(f, name_n, is_final, sea);
                    compared++;
                    generic += strchr(name_o, ' ') && isdigit((uint8_t)name_o[strlen(name_o) - 1]);
                    if (memcmp(name_o, name_n, sizeof(name_o)) || land_o != PlayersData[f].base_name_offset
                        || sea_o != PlayersData[f].base_sea_name_offset || gender_o != *GenderDefault
                        || plural_o != *PluralityDefault) {
                        if (mismatches < MAX_DETAIL) {
                            logf("  name_base(%u) offset %u sea %d final %d: orig '%.24s' (%u/%u) new '%.24s' (%u/%u)",
                                f, offset, sea, is_final, name_o, land_o, sea_o, name_n,
                                PlayersData[f].base_name_offset, PlayersData[f].base_sea_name_offset);
                        }
                        mismatches++;
                    }
                }
            }
        }
        PlayersData[f].base_name_offset = land_saved;
        PlayersData[f].base_sea_name_offset = sea_saved;
    }
    *GenderDefault = gender_saved;
    *PluralityDefault = plural_saved;
    logf("name_base: %d compared (%d default names by the original), %d mismatches", compared, generic,
        mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 29: native life and world

// alien_fauna() draws, focuses the map and opens popups for the player at the controls: record
// those calls instead (with the parse text at the time of a popup).
struct FaunaCall { int fn, args[3]; char text[40]; };
static FaunaCall FaunaCalls[64];
static int FaunaCallCount;
static int FaunaFocusAnswer;
static void fauna_call_record(int fn, int a0, int a1, int a2, const char *text) {
    if (FaunaCallCount < 64) {
        FaunaCall c = { fn, { a0, a1, a2 }, "" };
        if (text) {
            strncpy_s(c.text, text, _TRUNCATE);
        }
        FaunaCalls[FaunaCallCount] = c;
    }
    FaunaCallCount++;
}
static void __cdecl fauna_draw_tile_recorder(int x, int y, int mode) {
    fauna_call_record(1, x, y, mode, NULL);
}
static void __cdecl fauna_draw_tiles_recorder(int x, int y, int mode) {
    fauna_call_record(2, x, y, mode, NULL);
}
static int __fastcall fauna_focus_recorder(void *This, void *, int x, int y) {
    fauna_call_record(3, (int)This, x, y, NULL);
    return FaunaFocusAnswer;
}
static int __cdecl fauna_pop2_recorder(LPCSTR label, LPCSTR pcx, int unk) {
    char text[40];
    // truncating: sprintf_s ends the process (0xC0000409) when the text is longer than the buffer
    _snprintf_s(text, _TRUNCATE, "%s|%s|%s", label ? label : "", pcx ? pcx : "", ParseStrBuffer[0].str);
    fauna_call_record(4, unk, 0, 0, text);
    return 0;
}

// Call fn (alien_fauna or the original) with the two stack slots it can read before writing them
// seeded: at its entry [esp_entry - 0x10] (the original's [ebp-0xc]) and [esp_entry - 8] ([ebp-4]);
// returns what it leaves there.
static __declspec(naked) void fauna_seeded_call(void *fn, int seed_c, int seed_4, int *out_c,
                                                int *out_4) {
    __asm {
        mov eax, [esp + 8]
        mov [esp - 0x14], eax       // esp_entry = esp - 4 at the call
        mov eax, [esp + 12]
        mov [esp - 0xC], eax
        mov eax, [esp + 4]
        call eax
        mov eax, [esp - 0x14]
        mov ecx, [esp + 16]
        mov [ecx], eax
        mov eax, [esp - 0xC]
        mov ecx, [esp + 20]
        mov [ecx], eax
        ret
    }
}

static int test_alien_fauna() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x3C };
    JmpStub encounter_stub(0x0055E370, (void *)encounter_recorder);
    JmpStub interlude_stub(0x005230E0, (void *)interlude_recorder);
    JmpStub draw_tile_stub(0x0046AF40, (void *)fauna_draw_tile_recorder);
    JmpStub draw_tiles_stub(0x0046B140, (void *)fauna_draw_tiles_recorder);
    JmpStub focus_stub(0x0046B310, (void *)fauna_focus_recorder);
    JmpStub pop2_stub(0x00405140, (void *)fauna_pop2_recorder);
    TimeGetTimePin pin; // the SMACX sealurk choice reads the clock
    if (!encounter_stub.ok || !interlude_stub.ok || !draw_tile_stub.ok || !draw_tiles_stub.ok
        || !focus_stub.ok || !pop2_stub.ok || !pin.ok()) {
        logf("alien_fauna: cannot stub the UI calls or pin timeGetTime");
        return 1;
    }
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    const int count = base.veh_count_global;
    const int spare = (MaxVehNum - count) < 160 ? (MaxVehNum - count) : 160;
    Veh *slots_saved = (Veh *)malloc(sizeof(Veh) * (spare > 0 ? spare : 1));
    Veh *slots_o = (Veh *)malloc(sizeof(Veh) * (spare > 0 ? spare : 1));
    memcpy(slots_saved, &Vehs[count], sizeof(Veh) * spare);
    const int turn_saved = *TurnCurrentNum;
    const int current_saved = *FactionIDCurrent;
    const BOOL expansion_saved = *ExpansionEnabled;
    const uint32_t rules_saved = *GameRules;
    const int dist_saved = *BaseFindDist;
    const int compute_saved = *BaseComputeID;
    char256 parse_saved[2];
    memcpy(parse_saved, ParseStrBuffer, sizeof(parse_saved));
    const int every = (5 - (int)*MapNativeLifeForms) * 2;
    const int birth_turn = (turn_saved / every) * every ? (turn_saved / every) * every : every;
    int compared = 0, mismatches = 0, born = 0, draws = 0, popups = 0, stale_draws = 0;
    int seed_c = 0, seed_4 = 0;
    char label[96];
    // variant 0: as loaded; 1: a turn when native life is born; 2: the same, other clock parity
    // and SMACX flipped; 3: early game birth turn; 4: no forest or kelp left (the stack slots
    // reach the redraw); 5: no native life (scenario rule); 6: unit table nearly full
    auto run = [&](BOOL orig, int variant, int seed, int *slot_c, int *slot_4, int *rand_next) {
        snap_restore(&base);
        memcpy(&Vehs[count], slots_saved, sizeof(Veh) * spare);
        *BaseFindDist = dist_saved;
        *BaseComputeID = compute_saved;
        memcpy(ParseStrBuffer, parse_saved, sizeof(parse_saved));
        *TurnCurrentNum = variant == 0 || variant >= 5 ? turn_saved : (variant == 3 ? every : birth_turn);
        *FactionIDCurrent = 1 + seed % (MaxPlayerNum - 1);
        *ExpansionEnabled = variant == 2 ? !expansion_saved : expansion_saved;
        *GameRules = variant == 5 ? rules_saved | RULES_SCN_NO_NATIVE_LIFE : rules_saved;
        if (variant == 4) {
            for (uint32_t i = 0; i < *MapArea; i++) {
                (*MapTiles)[i].bit &= ~(BIT_FOREST | BIT_FARM);
            }
        } else if (variant == 6) {
            *VehCurrentCount = 0x700;
        }
        FixedTimeValue = (DWORD)(seed * 7 + variant);
        FaunaFocusAnswer = seed & 1;
        FaunaCallCount = 0;
        SpotCallCount = 0;
        reseed((uint32_t)(seed * 977 + variant * 31 + 5));
        seed_c = (seed % 3) ? seed * 13 % *MapLongitudeBounds : 0x7FFF0000 + seed;
        seed_4 = (seed % 3) ? seed * 7 % *MapLatitudeBounds : -1 - seed;
        if (orig) {
            OriginalEntry original(0x00522180, entry);
            fauna_seeded_call((void *)0x00522180, seed_c, seed_4, slot_c, slot_4);
        } else {
            fauna_seeded_call((void *)alien_fauna, seed_c, seed_4, slot_c, slot_4);
        }
        *rand_next = _rand();
    };
    static FaunaCall calls_o[64];
    static SpotCall spots_o[32];
    for (int variant = 0; variant < 7; variant++) {
        for (int seed = 0; seed < 12; seed++) {
            int c_o, d_o, r_o, c_n, d_n, r_n;
            REGRESS_CTX("alien_fauna() variant %d seed %d", variant, seed);
            run(true, variant, seed, &c_o, &d_o, &r_o);
            snap_take(&after_orig);
            memcpy(slots_o, &Vehs[count], sizeof(Veh) * spare);
            int calls_count_o = FaunaCallCount, spots_count_o = SpotCallCount;
            memcpy(calls_o, FaunaCalls, sizeof(calls_o));
            memcpy(spots_o, SpotCalls, sizeof(spots_o));
            int dist_o = *BaseFindDist, compute_o = *BaseComputeID;
            char parse_o[256];
            memcpy(parse_o, ParseStrBuffer[0].str, sizeof(parse_o));
            int new_count = *VehCurrentCount - (variant == 6 ? 0x700 : count);
            run(false, variant, seed, &c_n, &d_n, &r_n);
            snap_take(&after_new);
            compared++;
            born += new_count > 0 ? new_count : 0;
            for (int i = 0; i < calls_count_o && i < 64; i++) {
                draws += calls_o[i].fn <= 2;
                popups += calls_o[i].fn == 4;
                stale_draws += calls_o[i].fn == 1 && seed % 3 == 0
                    && (calls_o[i].args[0] == seed_c || calls_o[i].args[1] == seed_4);
            }
            sprintf_s(label, "alien_fauna() variant %d seed %d", variant, seed);
            BOOL diff = false;
            if (r_o != r_n || c_o != c_n || d_o != d_n || calls_count_o != FaunaCallCount
                || spots_count_o != SpotCallCount || dist_o != *BaseFindDist
                || compute_o != *BaseComputeID || strcmp(parse_o, ParseStrBuffer[0].str)) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: orig rand %d slots %d,%d calls %d spots %d dist %d; new rand %d slots %d,%d "
                        "calls %d spots %d dist %d", label, r_o, c_o, d_o, calls_count_o, spots_count_o,
                        dist_o, r_n, c_n, d_n, FaunaCallCount, SpotCallCount, *BaseFindDist);
                }
                diff = true;
            }
            int recorded = calls_count_o < 64 ? calls_count_o : 64;
            for (int i = 0; i < recorded && !diff; i++) {
                if (calls_o[i].fn != FaunaCalls[i].fn || memcmp(calls_o[i].args, FaunaCalls[i].args,
                    sizeof(calls_o[i].args)) || strcmp(calls_o[i].text, FaunaCalls[i].text)) {
                    if (mismatches < MAX_DETAIL) {
                        logf("  %s: call %d orig %d(%d, %d, %d) '%s' new %d(%d, %d, %d) '%s'", label, i,
                            calls_o[i].fn, calls_o[i].args[0], calls_o[i].args[1], calls_o[i].args[2],
                            calls_o[i].text, FaunaCalls[i].fn, FaunaCalls[i].args[0],
                            FaunaCalls[i].args[1], FaunaCalls[i].args[2], FaunaCalls[i].text);
                    }
                    diff = true;
                }
            }
            int spots = spots_count_o < 32 ? spots_count_o : 32;
            if (!diff && memcmp(spots_o, SpotCalls, sizeof(SpotCall) * spots)) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: contact/interlude calls differ", label);
                }
                diff = true;
            }
            if (!diff) {
                diff = snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL);
            }
            size_t at;
            if (!diff && (at = first_diff(slots_o, &Vehs[count], sizeof(Veh) * spare)) != (size_t)-1) {
                if (mismatches < MAX_DETAIL) {
                    logf("  %s: new Veh %u +0x%X differs", label, (unsigned)(count + at / sizeof(Veh)),
                        (unsigned)(at % sizeof(Veh)));
                }
                diff = true;
            }
            if (diff) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    memcpy(&Vehs[count], slots_saved, sizeof(Veh) * spare);
    *TurnCurrentNum = turn_saved;
    *FactionIDCurrent = current_saved;
    *ExpansionEnabled = expansion_saved;
    *GameRules = rules_saved;
    *BaseFindDist = dist_saved;
    *BaseComputeID = compute_saved;
    memcpy(ParseStrBuffer, parse_saved, sizeof(parse_saved));
    free(slots_saved);
    free(slots_o);
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("alien_fauna: %d compared (by the original: %d units born, %d redraws, %d popups, %d redraws "
        "at the seeded junk stack values), %d mismatches", compared, born, draws, popups, stale_draws,
        mismatches);
    return mismatches;
}

static int test_world_rivers() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x8C };
    SnapHarness h("world_rivers");
    const BOOL flat_saved = *MapIsFlat;
    int extra = 0;
    char label[96];
    // variant 0: as loaded; 1: every 23rd land tile becomes a source too; 2: the same on a flat
    // map; 3: every 7th land tile a source
    for (int variant = 0; variant < 4; variant++) {
        auto prepare = [&]() {
            *MapIsFlat = variant == 2 ? !flat_saved : flat_saved;
            if (variant) {
                int every = variant == 3 ? 7 : 23;
                for (uint32_t i = 0; i < *MapArea; i++) {
                    Map *tile = &(*MapTiles)[i];
                    if ((tile->climate & 0xE0) >= 0x60 && !(i % every)) {
                        tile->bit |= BIT_RIVERBED;
                    }
                }
            }
        };
        uint32_t seed_o = 0, seed_n = 0;
        int rand_o = 0, rand_n = 0;
        sprintf_s(label, "world_rivers() variant %d", variant);
        REGRESS_CTX("%s", label);
        h.compare(label,
            [&] {
                prepare();
                reseed(variant + 1);
                {
                    OriginalEntry orig(0x005C38B0, entry);
                    ((func_void *)0x005C38B0)();
                }
                seed_o = Rand->get_seed();
                rand_o = _rand();
            },
            [&] {
                prepare();
                reseed(variant + 1);
                world_rivers();
                seed_n = Rand->get_seed();
                rand_n = _rand();
            });
        if (seed_o != seed_n || rand_o != rand_n) {
            logf("  %s: random state differs (orig %08X/%d new %08X/%d)", label, seed_o, rand_o, seed_n,
                rand_n);
            extra++;
        }
    }
    *MapIsFlat = flat_saved;
    return h.report() + extra;
}

// world_climate() ends by redrawing the map views; record those calls instead.
static int ClimateClearCalls[16];
static int ClimateClearCount;
static void __fastcall climate_mapwin_clear_recorder(void *This, void *) {
    if (ClimateClearCount < 16) {
        ClimateClearCalls[ClimateClearCount] = (int)This;
    }
    ClimateClearCount++;
}
static void __fastcall climate_worldwin_clear_recorder(void *This, void *) {
    if (ClimateClearCount < 16) {
        ClimateClearCalls[ClimateClearCount] = -(int)This;
    }
    ClimateClearCount++;
}

static int test_world_climate() {
    static const uint8_t entry[6] = { 0x6A, 0x02, 0xE8, 0x29, 0x8D, 0xF5 };
    JmpStub mapwin_stub(0x0046FD90, (void *)climate_mapwin_clear_recorder);
    JmpStub worldwin_stub(0x004C45E0, (void *)climate_worldwin_clear_recorder);
    TimeGetTimePin pin; // the climate ends by reseeding from the clock
    if (!mapwin_stub.ok || !worldwin_stub.ok || !pin.ok()) {
        logf("world_climate: cannot stub the redraws or pin timeGetTime");
        return 1;
    }
    SnapHarness h("world_climate");
    h.empty_world(); // world_shorelines() runs alt_set(), which submerges bases and kills units
    const BOOL halted_saved = *GameHalted;
    const BOOL build_saved = *WorldBuildUnk2;
    int extra = 0;
    char label[96];
    // variants: game halted or not, world building or not, two clock values
    for (int variant = 0; variant < 4; variant++) {
        int calls_o[16], count_o = 0;
        uint32_t seed_o = 0, seed_n = 0;
        int rand_o = 0, rand_n = 0;
        auto prepare = [&]() {
            *GameHalted = variant & 1;
            *WorldBuildUnk2 = (variant & 2) != 0;
            FixedTimeValue = 1000003u * (variant + 1);
            ClimateClearCount = 0;
            reseed(variant + 11);
        };
        sprintf_s(label, "world_climate() variant %d", variant);
        REGRESS_CTX("%s", label);
        h.compare(label,
            [&] {
                prepare();
                {
                    OriginalEntry orig(0x005C5A30, entry);
                    ((func_void *)0x005C5A30)();
                }
                seed_o = Rand->get_seed();
                rand_o = _rand();
                count_o = ClimateClearCount;
                memcpy(calls_o, ClimateClearCalls, sizeof(calls_o));
            },
            [&] {
                prepare();
                world_climate();
                seed_n = Rand->get_seed();
                rand_n = _rand();
            });
        int recorded = count_o < 16 ? count_o : 16;
        if (seed_o != seed_n || rand_o != rand_n || count_o != ClimateClearCount
            || memcmp(calls_o, ClimateClearCalls, sizeof(int) * recorded)) {
            logf("  %s: random state or redraws differ (orig %08X/%d %d redraws, new %08X/%d %d redraws)",
                label, seed_o, rand_o, count_o, seed_n, rand_n, ClimateClearCount);
            extra++;
        }
    }
    *GameHalted = halted_saved;
    *WorldBuildUnk2 = build_saved;
    return h.report() + extra;
}

static int test_world_erosion() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x14 };
    SnapHarness h("world_erosion");
    h.empty_world(); // world_shorelines() runs alt_set(), which submerges bases and kills units
    const uint32_t land_saved = *MapLandCoverage, ocean_saved = *MapOceanCoverage;
    const uint32_t erosive_saved = *MapErosiveForces;
    const BOOL flat_saved = *MapIsFlat;
    int extra = 0;
    char label[96];
    // variant 0: as loaded; 1: land coverage 1 (plains and beach scale with the map); 2: no
    // erosive forces (capped); 3: ocean coverage 0 and erosive 2; 4: ocean coverage 3 on a flat map
    for (int variant = 0; variant < 5; variant++) {
        auto prepare = [&]() {
            *MapLandCoverage = variant == 1 ? 1 : land_saved;
            *MapErosiveForces = variant == 2 ? 0 : (variant == 3 ? 2 : erosive_saved);
            *MapOceanCoverage = variant == 3 ? 0 : (variant == 4 ? 3 : ocean_saved);
            *MapIsFlat = variant == 4 ? !flat_saved : flat_saved;
            reseed(variant * 7 + 3);
        };
        uint32_t seed_o = 0, seed_n = 0;
        int rand_o = 0, rand_n = 0;
        sprintf_s(label, "world_erosion() variant %d", variant);
        REGRESS_CTX("%s", label);
        h.compare(label,
            [&] {
                prepare();
                {
                    OriginalEntry orig(0x005C2CB0, entry);
                    ((func_void *)0x005C2CB0)();
                }
                seed_o = Rand->get_seed();
                rand_o = _rand();
            },
            [&] {
                prepare();
                world_erosion();
                seed_n = Rand->get_seed();
                rand_n = _rand();
            });
        if (seed_o != seed_n || rand_o != rand_n) {
            logf("  %s: random state differs (orig %08X/%d new %08X/%d)", label, seed_o, rand_o, seed_n,
                rand_n);
            extra++;
        }
    }
    *MapLandCoverage = land_saved;
    *MapOceanCoverage = ocean_saved;
    *MapErosiveForces = erosive_saved;
    *MapIsFlat = flat_saved;
    return h.report() + extra;
}

// ---------------------------------------------------------------- batch 29: base production

// popp, wave_it, interlude, draw_radius and help_topic are recorded; popp answers ProdPoppAnswers
// in turn (then 0, which also ends the GOVPROJECT help loop).
enum ProdFn { PF_POPP, PF_WAVE_IT, PF_INTERLUDE, PF_DRAW_RADIUS, PF_HELP_TOPIC };
struct ProdCall {
    int fn;
    char label[32];
    char pic[32];
    char file[16];
    int args[4];
};
const int MaxProdCalls = 16;
static ProdCall ProdCalls[MaxProdCalls];
static int ProdCallCount;
static int ProdPoppAnswers[4];
static int ProdPoppAsked;

static void prod_record(int fn, LPCSTR label, LPCSTR pic, LPCSTR file, int a0, int a1, int a2, int a3) {
    if (ProdCallCount < MaxProdCalls) {
        ProdCall *call = &ProdCalls[ProdCallCount];
        memset(call, 0, sizeof(*call));
        call->fn = fn;
        strncpy_s(call->label, label ? label : "", _TRUNCATE);
        strncpy_s(call->pic, pic ? pic : "", _TRUNCATE);
        strncpy_s(call->file, file ? file : "", _TRUNCATE);
        call->args[0] = a0;
        call->args[1] = a1;
        call->args[2] = a2;
        call->args[3] = a3;
    }
    ProdCallCount++;
}
static int __cdecl prod_popp(LPCSTR file, LPCSTR label, int unk, LPCSTR pic, int(__cdecl *callback)()) {
    prod_record(PF_POPP, label, pic, file, unk, callback != NULL, 0, 0);
    int answer = ProdPoppAsked < 4 ? ProdPoppAnswers[ProdPoppAsked] : 0;
    ProdPoppAsked++;
    return answer;
}
static void __cdecl prod_wave_it(int sound_id) {
    prod_record(PF_WAVE_IT, NULL, NULL, NULL, sound_id, 0, 0, 0);
}
static void __cdecl prod_interlude(int event_id, LPCSTR text, int unk1, int unk2) {
    prod_record(PF_INTERLUDE, text, NULL, NULL, event_id, unk1, unk2, 0);
}
static void __cdecl prod_draw_radius(int x, int y, int unk1, int unk2) {
    prod_record(PF_DRAW_RADIUS, NULL, NULL, NULL, x, y, unk1, unk2);
}
static void __cdecl prod_help_topic(int topic, int id) {
    prod_record(PF_HELP_TOPIC, NULL, NULL, NULL, topic, id, 0, 0);
}

struct ProdStubs {
    JmpStub popp_stub, wave_stub, interlude_stub, draw_stub, help_stub;
    ProdStubs()
        : popp_stub(0x0048C0A0, (void *)prod_popp),
          wave_stub(0x004455F0, (void *)prod_wave_it),
          interlude_stub(0x005230E0, (void *)prod_interlude),
          draw_stub(0x0046AEF0, (void *)prod_draw_radius),
          help_stub(0x0044CB60, (void *)prod_help_topic) {}
    BOOL ok() const {
        return popp_stub.ok && wave_stub.ok && interlude_stub.ok && draw_stub.ok && help_stub.ok;
    }
};

// What the production functions can change besides the game state in GameSnapshot.
struct ProdState {
    NoticeState notice; // parse slots and defaults, stringTemp, pronouns, diplomacy globals, seed
    uint8_t globals[BaseGlobalsSize];
    int pick_id[4];
    int pick_value[4];
    uint32_t interlude_state;
    Base *current;
    int selected;
    int sp[MaxSecretProjectNum];
    int rand_next; // the exe's rand() after the call
    ProdCall calls[MaxProdCalls];
    int call_count;
};

static void prod_take(ProdState *s, int rand_next) {
    notice_take(&s->notice, 0);
    memcpy(s->globals, BaseGlobals, BaseGlobalsSize);
    memcpy(s->pick_id, BaseGovPickID, sizeof(s->pick_id));
    memcpy(s->pick_value, BaseGovPickValue, sizeof(s->pick_value));
    s->interlude_state = *GameInterludeState;
    s->current = *BaseCurrent;
    s->selected = *BaseIDCurrentSelected;
    memcpy(s->sp, SecretProjectTable, sizeof(s->sp));
    s->rand_next = rand_next;
    memcpy(s->calls, ProdCalls, sizeof(s->calls));
    s->call_count = ProdCallCount;
}

static void prod_restore(const ProdState *s) {
    notice_restore(&s->notice);
    memcpy(BaseGlobals, s->globals, BaseGlobalsSize);
    memcpy(BaseGovPickID, s->pick_id, sizeof(s->pick_id));
    memcpy(BaseGovPickValue, s->pick_value, sizeof(s->pick_value));
    *GameInterludeState = s->interlude_state;
    *BaseCurrent = s->current;
    *BaseIDCurrentSelected = s->selected;
    memcpy(SecretProjectTable, s->sp, sizeof(s->sp));
}

static BOOL prod_differs(const ProdState *o, const ProdState *n, const char *label) {
#define DLOG(...) do { if (label) logf(__VA_ARGS__); } while (0)
    if (o->call_count != n->call_count) {
        DLOG("  %s: orig made %d calls, new %d", label, o->call_count, n->call_count);
        return true;
    }
    for (int i = 0; i < o->call_count && i < MaxProdCalls; i++) {
        const ProdCall *a = &o->calls[i], *b = &n->calls[i];
        if (a->fn != b->fn || strcmp(a->label, b->label) || strcmp(a->pic, b->pic)
            || strcmp(a->file, b->file) || memcmp(a->args, b->args, sizeof(a->args))) {
            DLOG("  %s: call %d: orig fn %d '%s' '%s' (%d, %d, %d, %d), new fn %d '%s' '%s' (%d, %d, %d, %d)",
                label, i, a->fn, a->label, a->pic, a->args[0], a->args[1], a->args[2], a->args[3],
                b->fn, b->label, b->pic, b->args[0], b->args[1], b->args[2], b->args[3]);
            return true;
        }
    }
    if (notice_differs(&o->notice, &n->notice, label) || base_globals_differ(o->globals, n->globals, label)) {
        return true;
    }
    if (memcmp(o->pick_id, n->pick_id, sizeof(o->pick_id))
        || memcmp(o->pick_value, n->pick_value, sizeof(o->pick_value))) {
        DLOG("  %s: governor picks orig %d/%d %d/%d %d/%d %d/%d new %d/%d %d/%d %d/%d %d/%d", label,
            o->pick_id[0], o->pick_value[0], o->pick_id[1], o->pick_value[1], o->pick_id[2],
            o->pick_value[2], o->pick_id[3], o->pick_value[3], n->pick_id[0], n->pick_value[0],
            n->pick_id[1], n->pick_value[1], n->pick_id[2], n->pick_value[2], n->pick_id[3],
            n->pick_value[3]);
        return true;
    }
    if (o->interlude_state != n->interlude_state || o->current != n->current
        || o->selected != n->selected || o->rand_next != n->rand_next
        || memcmp(o->sp, n->sp, sizeof(o->sp))) {
        DLOG("  %s: interlude state %08X/%08X, current base %p/%p, selected %d/%d, rand %d/%d", label,
            o->interlude_state, n->interlude_state, (void *)o->current, (void *)n->current,
            o->selected, n->selected, o->rand_next, n->rand_next);
        return true;
    }
    return false;
#undef DLOG
}

// Run a production function as the exe and as the DLL from the same state and compare.
struct ProdHarness {
    GameSnapshot base, after_orig, after_new;
    ProdState saved, orig, now;
    uint8_t status_saved;
    int diff_saved, sunspot_saved, current_saved, skip_saved;
    uint32_t game_state_saved;
    BOOL net_saved;
    int compared, mismatches, with_calls, changed;
    ProdHarness() : compared(0), mismatches(0), with_calls(0), changed(0) {
        snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
        snap_take(&base);
        ProdCallCount = 0;
        prod_take(&saved, 0);
        status_saved = *FactionsStatus;
        diff_saved = *DiffLevelCurrent;
        sunspot_saved = *SunspotDuration;
        current_saved = *FactionIDCurrent;
        skip_saved = *SkipTechScreenB;
        game_state_saved = *GameState;
        net_saved = *IsMultiplayerNet;
    }
    ~ProdHarness() {
        prepare();
        snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    }
    void prepare() {
        snap_restore(&base);
        prod_restore(&saved);
        *FactionsStatus = status_saved;
        *DiffLevelCurrent = diff_saved;
        *SunspotDuration = sunspot_saved;
        *FactionIDCurrent = current_saved;
        *SkipTechScreenB = skip_saved;
        *GameState = game_state_saved;
        *IsMultiplayerNet = net_saved;
        ProdCallCount = 0;
        ProdPoppAsked = 0;
        memset(ProdPoppAnswers, 0, sizeof(ProdPoppAnswers));
    }
    // setup() puts the variant in place after each restore; exe() and dll() make the call.
    template <class S, class E, class D>
    void run(const char *label, uint32_t seed, S setup, E exe, D dll) {
        REGRESS_CTX("%s", label);
        prepare();
        setup();
        reseed(seed);
        exe();
        int rand_o = _rand();
        snap_take(&after_orig);
        prod_take(&orig, rand_o);
        prepare();
        setup();
        reseed(seed);
        dll();
        int rand_n = _rand();
        snap_take(&after_new);
        prod_take(&now, rand_n);
        compared++;
        with_calls += orig.call_count > 0;
        changed += snap_differs(&base, &after_orig, NULL);
        const char *detail = mismatches < MAX_DETAIL ? label : NULL;
        if (prod_differs(&orig, &now, detail) || snap_differs(&after_orig, &after_new, detail)) {
            mismatches++;
        }
    }
};

static const uint8_t BaseChangeEntry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x24 };
static const uint8_t BaseResetEntry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08 };
static const uint8_t BaseHurryEntry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x1C };

// a prototype of the faction (or a basic one) other than the item the base builds
static int prod_other_unit(uint32_t faction_id, int current) {
    for (int i = 0; i < MaxVehProtoFactionNum; i++) {
        int proto_id = faction_id * MaxVehProtoFactionNum + i;
        if ((VehPrototypes[proto_id].flags & PROTO_ACTIVE) && proto_id != current) {
            return proto_id;
        }
    }
    return current == BSC_SCOUT_PATROL ? BSC_FORMERS : BSC_SCOUT_PATROL;
}

static int test_base_change() {
    OriginalEntry vanilla(0x004E5A60, BaseChangeEntry);
    ProdStubs stubs;
    if (!vanilla.ok || !stubs.ok()) {
        return 1;
    }
    typedef void func_change(uint32_t, int);
    func_change *exe = (func_change *)0x004E5A60;
    int sp_free[2] = { -1, -1 }, sp_built = -1;
    for (int k = 0; k < MaxSecretProjectNum; k++) {
        if (SecretProjectTable[k] == -1) {
            if (sp_free[0] < 0) {
                sp_free[0] = k;
            } else if (sp_free[1] < 0) {
                sp_free[1] = k;
            }
        } else if (SecretProjectTable[k] >= 0 && sp_built < 0) {
            sp_built = k;
        }
    }
    ProdHarness h;
    char label[112];
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id += 5) {
        Base *b = &Bases[base_id];
        uint32_t f = b->faction_id_current;
        int other_base = -1;
        for (int j = 0; j < *BaseCurrentCount && other_base < 0; j++) {
            if (j != base_id && Bases[j].faction_id_current == f) {
                other_base = j;
            }
        }
        const int items[] = { b->queue_production_id[0], prod_other_unit(f, b->queue_production_id[0]),
            -FAC_RECYCLING_TANKS, sp_free[0] >= 0 ? -(FAC_HUMAN_GENOME_PROJ + sp_free[0]) : 99999,
            sp_built >= 0 ? -(FAC_HUMAN_GENOME_PROJ + sp_built) : 99999,
            SecretProjectTable[SP_VOICE_OF_PLANET] == -1 ? -(FAC_HUMAN_GENOME_PROJ + SP_VOICE_OF_PLANET)
                : 99999 };
        // as loaded; human (retooling); moving off another free project the faction's other base
        // also builds; the same alone; sunspots; game state bit 1; no project intel yet
        for (int v = 0; v < 7; v++) {
            for (int item : items) {
                auto setup = [&] {
                    if (v == 1) {
                        *FactionsStatus ^= (uint8_t)(1 << f);
                        b->production_id_last = b->queue_production_id[0];
                        b->minerals_accumulated_2 = 60;
                    } else if ((v == 2 || v == 3) && sp_free[1] >= 0) {
                        b->queue_production_id[0] = -(FAC_HUMAN_GENOME_PROJ + sp_free[1]);
                        if (v == 2 && other_base >= 0) {
                            Bases[other_base].queue_production_id[0] = b->queue_production_id[0];
                        }
                    } else if (v == 4) {
                        *SunspotDuration = 3;
                    } else if (v == 5) {
                        *GameState |= STATE_UNK_1;
                    } else if (v == 6) {
                        memset(PlayersData[f].secret_project_intel, 0,
                            sizeof(PlayersData[f].secret_project_intel));
                    }
                };
                sprintf_s(label, "base_change(%d, %d) variant %d", base_id, item, v);
                h.run(label, (uint32_t)(base_id * 37 + item + v), setup, [&] { exe(base_id, item); },
                    [&] { base_change(base_id, item); });
            }
        }
    }
    logf("base_change: %d compared (%d with a call out, %d changed the game state), %d mismatches",
        h.compared, h.with_calls, h.changed, h.mismatches);
    return h.mismatches;
}

static int test_base_reset() {
    OriginalEntry vanilla(0x004E5FE0, BaseResetEntry);
    OriginalEntry vanilla_change(0x004E5A60, BaseChangeEntry);
    ProdStubs stubs;
    if (!vanilla.ok || !vanilla_change.ok || !stubs.ok()) {
        return 1;
    }
    func_xy *exe = (func_xy *)0x004E5FE0;
    const uint32_t priorities = GOV_PRIORITY_EXPLORE | GOV_PRIORITY_DISCOVER | GOV_PRIORITY_BUILD
        | GOV_PRIORITY_CONQUER;
    ProdHarness h;
    char label[96];
    int asked = 0;
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        Base *b = &Bases[base_id];
        uint32_t f = b->faction_id_current;
        // as loaded with and without the governor priorities; as a human base with the governor
        // and all four priorities; the governor allowed projects, answering help then 2, or 1; the
        // governor without priorities
        for (int v = 0; v < 6; v++) {
            if (v >= 2 && base_id % 3) {
                continue;
            }
            BOOL priorities_flag = v == 1 || v == 2 || v == 5;
            auto setup = [&] {
                if (v >= 2) {
                    *FactionsStatus |= (uint8_t)(1 << f);
                    b->governor |= GOV_ACTIVE;
                }
                if (v == 2) {
                    b->governor |= priorities;
                } else if (v == 3 || v == 4) {
                    b->governor |= GOV_MAY_PROD_SP;
                    ProdPoppAnswers[0] = v == 3 ? 3 : 1;
                    ProdPoppAnswers[1] = v == 3 ? 2 : 0;
                } else if (v == 5) {
                    b->governor &= ~priorities;
                }
            };
            sprintf_s(label, "base_reset(%d, %d) variant %d", base_id, priorities_flag, v);
            h.run(label, (uint32_t)(base_id * 31 + v), setup, [&] { exe(base_id, priorities_flag); },
                [&] { base_reset(base_id, priorities_flag); });
            asked += h.orig.call_count > 0 && h.orig.calls[0].fn == PF_POPP
                && !strcmp(h.orig.calls[0].label, "GOVPROJECT");
        }
    }
    logf("base_reset: %d compared (%d with a call out, %d asked GOVPROJECT first, %d changed the game "
        "state), %d mismatches", h.compared, h.with_calls, asked, h.changed, h.mismatches);
    return h.mismatches;
}

static int test_base_hurry() {
    OriginalEntry vanilla(0x004F3FE0, BaseHurryEntry);
    ProdStubs stubs;
    if (!vanilla.ok || !stubs.ok()) {
        return 1;
    }
    func_void *exe = (func_void *)0x004F3FE0;
    int sp_free = -1;
    for (int k = 0; k < MaxSecretProjectNum && sp_free < 0; k++) {
        if (SecretProjectTable[k] == -1) {
            sp_free = k;
        }
    }
    ProdHarness h;
    char label[96];
    int spent = 0;
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        Base *b = &Bases[base_id];
        uint32_t f = b->faction_id_current;
        PlayerData *p = &PlayersData[f];
        // 0 as loaded; 1 minerals in stock and 3000 energy; 2 a free project with 5000; 3 a
        // perimeter defense with 2500 at vendetta; 4 the headquarters; 5 a psi gate; 6 the Ascent
        // with 4000; 7 a riot under police -3 building recycling tanks; 8 human with the governor
        // allowed to hurry; 9 the Voice of Planet built; 10 a colony pod at size 1; 11 at size 3
        for (int v = 0; v < 12; v++) {
            if (v >= 2 && base_id % 2) {
                continue;
            }
            auto setup = [&] {
                *BaseCurrent = b;
                *BaseIDCurrentSelected = base_id;
                if (v >= 1 && b->minerals_accumulated < 1) {
                    b->minerals_accumulated = 1 + base_id % 7;
                }
                if (v == 1) {
                    p->energy_reserves = 3000;
                } else if (v == 2 && sp_free >= 0) {
                    b->queue_production_id[0] = -(FAC_HUMAN_GENOME_PROJ + sp_free);
                    p->energy_reserves = 5000;
                } else if (v == 3) {
                    b->queue_production_id[0] = -FAC_PERIMETER_DEFENSE;
                    p->energy_reserves = 2500;
                    p->diplo_treaties[f % 7 + 1] |= DTREATY_VENDETTA;
                } else if (v == 4) {
                    b->queue_production_id[0] = -FAC_HEADQUARTERS;
                } else if (v == 5) {
                    b->queue_production_id[0] = -FAC_PSI_GATE;
                } else if (v == 6) {
                    b->queue_production_id[0] = -(FAC_HUMAN_GENOME_PROJ + SP_ASCENT_TO_TRANSCENDENCE);
                    p->energy_reserves = 4000;
                } else if (v == 7) {
                    b->queue_production_id[0] = -FAC_RECYCLING_TANKS;
                    b->state |= BSTATE_DRONE_RIOTS_ACTIVE;
                    b->drone_total = b->talent_total + 2;
                    p->soc_effect_pending.police = -3;
                } else if (v == 8) {
                    *FactionsStatus |= (uint8_t)(1 << f);
                    b->governor |= GOV_ACTIVE | GOV_MAY_HURRY_PRODUCTION;
                } else if (v == 9) {
                    SecretProjectTable[SP_VOICE_OF_PLANET] = base_id;
                } else if (v == 10 || v == 11) {
                    b->queue_production_id[0] = BSC_COLONY_POD;
                    b->population_size = v == 10 ? 1 : 3;
                }
            };
            sprintf_s(label, "base_hurry() base %d variant %d", base_id, v);
            h.run(label, (uint32_t)(base_id * 41 + v), setup, [&] {
                int energy = p->energy_reserves;
                exe();
                spent += p->energy_reserves != energy;
            }, [&] { base_hurry(); });
        }
    }
    logf("base_hurry: %d compared (%d spent energy, %d with a call out), %d mismatches", h.compared,
        spent, h.with_calls, h.mismatches);
    return h.mismatches;
}

// ---------------------------------------------------------------- batch 30: unit actions

// Exe functions the unit actions reach (drawing, the map view, sounds, diplomacy, combat and the
// other actions) are recorded with the notice recorders instead of being run.
static void __cdecl unitact_draw_tile(int x, int y, int mode) {
    notice_record(NF_DRAW_TILE, NULL, NULL, NULL, x, y, mode);
}
static int __fastcall unitact_console_focus(void *This, void *, int x, int y, int faction_id) {
    return notice_record(NF_CONSOLE_FOCUS, NULL, NULL, NULL, x, y, faction_id + (This == ConsoleVar) * 16);
}
static void __cdecl unitact_atrocity(uint32_t faction_id, uint32_t faction_id_victim, int unk, BOOL staple) {
    notice_record(NF_ATROCITY, NULL, NULL, NULL, faction_id, faction_id_victim, unk * 2 + (staple != 0));
}
static void __cdecl unitact_boom(int x, int y, int unk) {
    notice_record(NF_BOOM, NULL, NULL, NULL, x, y, unk);
}
static void __cdecl unitact_terraform(int veh_id, int terraform_id, int unk) {
    notice_record(NF_ACT_TERRAFORM, NULL, NULL, NULL, veh_id, terraform_id, unk);
}
static void __cdecl unitact_go_to(int veh_id) {
    notice_record(NF_ACT_GO_TO, NULL, NULL, NULL, veh_id, Vehs[veh_id].order, 0);
}
static void __cdecl unitact_road_to(int veh_id) {
    notice_record(NF_ACT_ROAD_TO, NULL, NULL, NULL, veh_id, Vehs[veh_id].order, 0);
}
static void __cdecl unitact_destroy(int veh_id, int unk, int x, int y) {
    notice_record(NF_ACT_DESTROY, NULL, NULL, NULL, veh_id, unk, x * 0x10000 + y);
}
static int __cdecl unitact_battle_fight_2(int veh_id, int offset, int x, int y, BOOL is_arty, int unk1,
                                          int unk2) {
    return notice_record(NF_BATTLE_FIGHT_2, NULL, NULL, NULL, veh_id,
        offset + is_arty * 0x100 + unk1 * 0x1000 + unk2 * 0x10000, x * 0x10000 + y);
}
static void __cdecl unitact_draw_map(int mode) {
    notice_record(NF_DRAW_MAP, NULL, NULL, NULL, mode, 0, 0);
}
static void __fastcall unitact_report_redraw(void *This, void *) {
    notice_record(NF_REPORT_REDRAW, NULL, NULL, NULL, This == ReportIf, 0, 0);
}
// the monument screen waits for the player (mon_colony_founded() on an early save)
static void __cdecl unitact_monument(int unk) {
    notice_record(NF_MONUMENT, NULL, NULL, NULL, unk, 0, 0);
}

struct UnitActStubs {
    JmpStub draw_tile_stub, focus_stub, atrocity_stub, boom_stub, terraform_stub, go_to_stub,
        road_to_stub, destroy_stub, fight_stub, draw_map_stub, report_stub, double_cross_stub,
        monument_stub;
    UnitActStubs()
        : draw_tile_stub(0x0046AF40, (void *)unitact_draw_tile),
          focus_stub(0x005108A0, (void *)unitact_console_focus),
          atrocity_stub((uint32_t)&atrocity, (void *)unitact_atrocity),
          boom_stub(0x00504AA0, (void *)unitact_boom),
          terraform_stub(0x004C9B00, (void *)unitact_terraform),
          go_to_stub(0x004CB310, (void *)unitact_go_to),
          road_to_stub(0x004CB580, (void *)unitact_road_to),
          destroy_stub(0x004CAA50, (void *)unitact_destroy),
          fight_stub(0x00506AF0, (void *)unitact_battle_fight_2),
          draw_map_stub(0x0046B190, (void *)unitact_draw_map),
          report_stub(0x004A3FC0, (void *)unitact_report_redraw),
          double_cross_stub((uint32_t)&double_cross, (void *)notice_double_cross),
          monument_stub(0x00476A50, (void *)unitact_monument) {}
    BOOL ok() const {
        return draw_tile_stub.ok && focus_stub.ok && atrocity_stub.ok && boom_stub.ok
            && terraform_stub.ok && go_to_stub.ok && road_to_stub.ok && destroy_stub.ok
            && fight_stub.ok && draw_map_stub.ok && report_stub.ok && double_cross_stub.ok
            && monument_stub.ok;
    }
};

// What the actions can change besides GameSnapshot and NoticeState: the base globals, the
// selection globals kill()/veh_kill() renumber, the current base, the replay record, the base
// slot after the last base (base_init()), the monuments and the position in rand()'s stream.
struct UnitActExtra {
    int ret;
    int rand_next;
    uint8_t base_globals[BaseGlobalsSize];
    int selection[5];
    int attack_flags;
    uint32_t bitfield;
    int selected, compute;
    Base *current;
    int replay_end;
    uint8_t replay[16];
    Base slot;
    uint8_t monuments[MonumentTableSize];
};
static int *const UnitActSelection[5] = { (int *)0x0093928C, (int *)0x00939290, (int *)0x008C6B2C,
    (int *)0x008C6B34, (int *)0x009A64DC };
static int *const UnitActReplayEnd = (int *)0x009469F8;
static uint8_t *const UnitActReplay = (uint8_t *)0x00950324;
static UnitActExtra UnitActSaved, UnitActOrig, UnitActNew;
static int UnitActSlot;
static int UnitActReplayStart;

static void unitact_take(UnitActExtra *e, int ret) {
    e->ret = ret;
    e->rand_next = _rand(); // same position in rand()'s stream?
    memcpy(e->base_globals, BaseGlobals, BaseGlobalsSize);
    for (int i = 0; i < 5; i++) {
        e->selection[i] = *UnitActSelection[i];
    }
    e->attack_flags = *VehAttackFlags;
    e->bitfield = *UnkBitfield1;
    e->selected = *BaseIDCurrentSelected;
    e->compute = *BaseComputeID;
    e->current = *BaseCurrent;
    e->replay_end = *UnitActReplayEnd;
    memcpy(e->replay, UnitActReplay + UnitActReplayStart, sizeof(e->replay));
    e->slot = Bases[UnitActSlot];
    memcpy(e->monuments, MonumentTable, MonumentTableSize);
}

static void unitact_restore(uint32_t seed) {
    const UnitActExtra *e = &UnitActSaved;
    memcpy(BaseGlobals, e->base_globals, BaseGlobalsSize);
    for (int i = 0; i < 5; i++) {
        *UnitActSelection[i] = e->selection[i];
    }
    *VehAttackFlags = e->attack_flags;
    *UnkBitfield1 = e->bitfield;
    *BaseIDCurrentSelected = e->selected;
    *BaseComputeID = e->compute;
    *BaseCurrent = e->current;
    *UnitActReplayEnd = e->replay_end;
    memcpy(UnitActReplay + UnitActReplayStart, e->replay, sizeof(e->replay));
    Bases[UnitActSlot] = e->slot;
    memcpy(MonumentTable, e->monuments, MonumentTableSize);
    reseed(seed);
}

static const char *unitact_differs(const UnitActExtra *o, const UnitActExtra *n) {
    if (o->ret != n->ret) return "return value";
    if (o->rand_next != n->rand_next) return "rand() position";
    if (memcmp(o->base_globals, n->base_globals, BaseGlobalsSize)) return "base globals";
    if (memcmp(o->selection, n->selection, sizeof(o->selection))) return "selection globals";
    if (o->attack_flags != n->attack_flags) return "VehAttackFlags";
    if (o->bitfield != n->bitfield) return "UnkBitfield1";
    if (o->selected != n->selected || o->compute != n->compute || o->current != n->current) {
        return "current base";
    }
    if (o->replay_end != n->replay_end || memcmp(o->replay, n->replay, sizeof(o->replay))) {
        return "replay record";
    }
    if (memcmp(&o->slot, &n->slot, sizeof(Base))) return "new base slot";
    if (memcmp(o->monuments, n->monuments, MonumentTableSize)) return "monuments";
    return NULL;
}

// One action call from the same state on both sides through NoticeHarness, plus UnitActExtra.
template <class S, class E, class D>
static void unitact_run(NoticeHarness &h, const char *label, uint32_t seed, S setup, E exe, D dll) {
    REGRESS_CTX("%s", label); // named in a crash or hang report
    int before = h.mismatches;
    h.run(label, [&] { unitact_restore(seed); setup(); },
        [&] { unitact_take(&UnitActOrig, exe()); return 0; },
        [&] { unitact_take(&UnitActNew, dll()); return 0; });
    const char *what = unitact_differs(&UnitActOrig, &UnitActNew);
    if (what) {
        if (h.mismatches == before) {
            h.mismatches++;
        }
        if (h.mismatches <= MAX_DETAIL) {
            logf("  %s: %s differs (orig returned %d, new %d)", label, what, UnitActOrig.ret,
                UnitActNew.ret);
        }
    }
}

struct UnitActScope {
    BOOL draw_saved, net_saved;
    int current_saved;
    UnitActScope() {
        UnitActSlot = (*BaseCurrentCount < MaxBaseNum) ? *BaseCurrentCount : MaxBaseNum - 1;
        UnitActReplayStart = *UnitActReplayEnd;
        unitact_take(&UnitActSaved, 0);
        draw_saved = VehBattleUnkTgl[0];
        VehBattleUnkTgl[0] = true; // kill() skips its map redraw while this is set
        net_saved = *IsMultiplayerNet;
        *IsMultiplayerNet = false;
        current_saved = *FactionIDCurrent;
    }
    ~UnitActScope() {
        unitact_restore(0);
        VehBattleUnkTgl[0] = draw_saved;
        *IsMultiplayerNet = net_saved;
        *FactionIDCurrent = current_saved;
    }
};

static int test_action_small() {
    static const uint8_t entry_feature[6] = { 0xA1, 0x60, 0xF6, 0x93, 0x00, 0x85 };
    static const uint8_t entry_patrol[6] = { 0x55, 0x8B, 0xEC, 0x53, 0x8B, 0x5D };
    static const uint8_t entry_action[6] = { 0x55, 0x8B, 0xEC, 0x8B, 0x4D, 0x08 };
    OriginalEntry vanilla_feature(0x004CD060, entry_feature);
    OriginalEntry vanilla_patrol(0x004CDA30, entry_patrol);
    OriginalEntry vanilla_action(0x004CF740, entry_action);
    NoticeStubs stubs;
    UnitActStubs stubs2;
    if (!vanilla_feature.ok || !vanilla_patrol.ok || !vanilla_action.ok || !stubs.ok() || !stubs2.ok()) {
        return 1;
    }
    UnitActScope scope;
    int total = 0;
    char label[96];
    {
        NoticeHarness h;
        for (int net = 0; net < 2; net++) {
            unitact_run(h, "action_feature", 1, [&] { *IsMultiplayerNet = net; },
                [&] { ((func_void *)0x004CD060)(); return 0; }, [&] { action_feature(); return 0; });
        }
        *IsMultiplayerNet = false;
        logf("action_feature: %d compared (%d with a notice), %d mismatches", h.compared, h.with_calls,
            h.mismatches);
        total += h.mismatches;
    }
    {
        NoticeHarness h;
        int patrols = 0;
        for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id += 11) {
            const int x0 = Vehs[veh_id].x, y0 = Vehs[veh_id].y;
            // as loaded (when the waypoint count is small) and with 0, 1 and 3 waypoints set
            for (int v = 0; v < 4; v++) {
                if (!v && Vehs[veh_id].waypoint_count > 3) {
                    continue;
                }
                for (int i = 0; i < 25; i += 3) {
                    int x = x0 + RadiusOffsetX[i], y = y0 + RadiusOffsetY[i];
                    if (i == 24) {
                        x = x0 + 10;
                    }
                    sprintf_s(label, "action_patrol(%d, %d, %d) variant %d", veh_id, x, y, v);
                    unitact_run(h, label, veh_id + i, [&] {
                            if (v) {
                                Vehs[veh_id].waypoint_count = (uint8_t)(v == 3 ? 3 : v - 1);
                            }
                        },
                        [&] { ((func_xyi *)0x004CDA30)(veh_id, x, y); return 0; },
                        [&] { action_patrol(veh_id, x, y); return 0; });
                    patrols += Vehs[veh_id].order == ORDER_MOVE_TO; // state after the DLL call
                }
            }
        }
        logf("action_patrol: %d compared (%d changed state, %d left a patrol order), %d mismatches",
            h.compared, h.changed, patrols, h.mismatches);
        total += h.mismatches;
    }
    {
        NoticeHarness h;
        static const int orders[] = { 0, 1, 2, 3, 4, 5, 9, 10, 14, 19, 20, 22, 23, 24, 25, 26, 27, 28,
            29, 0x40, 0x44, 0x48, 0x57, 0x58, 0x59, 0x5B, 0x5C, 0x7F, -1, -128 };
        int returned = 0;
        for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id += 23) {
            for (int order : orders) {
                sprintf_s(label, "action(%d) order %d", veh_id, order);
                unitact_run(h, label, veh_id, [&] { Vehs[veh_id].order = (int8_t)order; },
                    [&] { return ((func_i *)0x004CF740)(veh_id); },
                    [&] { return (int)action(veh_id); });
                returned += UnitActOrig.ret != 0;
            }
        }
        logf("action: %d compared (%d returned true, %d with a call), %d mismatches", h.compared,
            returned, h.with_calls, h.mismatches);
        total += h.mismatches;
    }
    return total;
}

static int test_action_gate() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x53, 0x8B, 0x5D };
    OriginalEntry vanilla(0x004CF380, entry);
    NoticeStubs stubs;
    UnitActStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok() || !*BaseCurrentCount) {
        return !*BaseCurrentCount ? 0 : 1;
    }
    UnitActScope scope;
    NoticeHarness h;
    char label[96];
    const int step = (*BaseCurrentCount + 7) / 8;
    for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id += 29) {
        for (int base_id = veh_id % step; base_id < *BaseCurrentCount; base_id += step) {
            for (int selected = 0; selected < 2; selected++) {
                sprintf_s(label, "action_gate(%d, %d) selected %d", veh_id, base_id, selected);
                unitact_run(h, label, veh_id, [&] { if (selected) *MapWinVehIDSelected = veh_id; },
                    [&] { ((func_xy *)0x004CF380)(veh_id, base_id); return 0; },
                    [&] { action_gate(veh_id, base_id); return 0; });
            }
        }
    }
    logf("action_gate: %d compared (%d changed state), %d mismatches", h.compared, h.changed,
        h.mismatches);
    return h.mismatches;
}

static int test_action_staple() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08 };
    OriginalEntry vanilla(0x004CA7F0, entry);
    NoticeStubs stubs;
    UnitActStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok()) {
        return 1;
    }
    UnitActScope scope;
    NoticeHarness h;
    char label[96];
    int done = 0;
    static const int counts[] = { 0, 1, 2, 4, 7, 8, 12 };
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id += 4) {
        for (int count : counts) {
            // the base's owner at the controls or not; a former owner different from the owner
            for (int v = 0; v < 3; v++) {
                sprintf_s(label, "action_staple(%d) count %d variant %d", base_id, count, v);
                unitact_run(h, label, base_id * 16 + count, [&] {
                        Bases[base_id].nerve_staple_count = count;
                        *FactionIDCurrent = (v == 1) ? Bases[base_id].faction_id_current : scope.current_saved;
                        if (v == 2) {
                            Bases[base_id].faction_id_former = (uint8_t)(Bases[base_id].faction_id_current % 7 + 1);
                        }
                    },
                    [&] { ((func5 *)0x004CA7F0)(base_id); return 0; },
                    [&] { action_staple(base_id); return 0; });
                done += Bases[base_id].nerve_staple_turns_left > 0;
            }
        }
    }
    logf("action_staple: %d compared (%d with calls, bases left stapled %d), %d mismatches", h.compared,
        h.with_calls, done, h.mismatches);
    return h.mismatches;
}

static int test_action_destruct() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C };
    OriginalEntry vanilla(0x004CD2F0, entry);
    NoticeStubs stubs;
    UnitActStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok()) {
        return 1;
    }
    UnitActScope scope;
    NoticeHarness h;
    char label[96];
    int killed = 0;
    for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id += 17) {
        // as loaded; every unit already badly damaged
        for (int v = 0; v < 2; v++) {
            sprintf_s(label, "action_destruct(%d) variant %d", veh_id, v);
            unitact_run(h, label, veh_id, [&] {
                    if (v) {
                        for (int i = 0; i < *VehCurrentCount; i++) {
                            Vehs[i].dmg_incurred = (uint8_t)(150 + i % 100);
                        }
                    }
                },
                [&] { ((func5 *)0x004CD2F0)(veh_id); return 0; },
                [&] { action_destruct(veh_id); return 0; });
            killed += h.base.veh_count_global - *VehCurrentCount;
        }
    }
    logf("action_destruct: %d compared (%d units destroyed by the DLL), %d mismatches", h.compared,
        killed, h.mismatches);
    return h.mismatches;
}

static int test_action_sat_attack() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x53, 0x57, 0x8B };
    OriginalEntry vanilla(0x004CF480, entry);
    NoticeStubs stubs;
    UnitActStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok()) {
        return 1;
    }
    UnitActScope scope;
    const int mode_saved = *ReportIfMode;
    const int page = *ReportIfPage;
    const BOOL page_ok = page >= 0 && page < 8;
    const int page_type_saved = page_ok ? ReportIfPageTypes[page] : 0;
    NoticeHarness h;
    char label[96];
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            int base_id = 0;
            for (int i = 0; i < *BaseCurrentCount; i++) {
                if (Bases[i].faction_id_current == f2) {
                    base_id = i;
                    break;
                }
            }
            for (int sat_type = 0; sat_type < 5; sat_type++) {
                // as loaded; at vendetta; f1 at the controls; f2 at the controls with the
                // report open on that page
                for (int v = 0; v < 4; v++) {
                    if (v == 3 && !page_ok) {
                        continue;
                    }
                    sprintf_s(label, "action_sat_attack(%d, %d, %d, %d) variant %d", f1, f2, sat_type,
                        base_id, v);
                    unitact_run(h, label, f1 * 64 + f2 * 8 + sat_type, [&] {
                            *ReportIfMode = (v == 3) ? 6 : mode_saved;
                            if (page_ok) {
                                ReportIfPageTypes[page] = (v == 3) ? 5 : page_type_saved;
                            }
                            if (v == 1) {
                                PlayersData[f1].diplo_treaties[f2] |= DTREATY_VENDETTA;
                            }
                            *FactionIDCurrent = (v == 2) ? f1 : ((v == 3) ? f2 : scope.current_saved);
                        },
                        [&] { ((void(__cdecl *)(int, int, int, int))0x004CF480)(f1, f2, sat_type, base_id); return 0; },
                        [&] { action_sat_attack(f1, f2, sat_type, base_id); return 0; });
                }
            }
        }
    }
    *ReportIfMode = mode_saved;
    if (page_ok) {
        ReportIfPageTypes[page] = page_type_saved;
    }
    logf("action_sat_attack: %d compared (%d with calls), %d mismatches", h.compared, h.with_calls,
        h.mismatches);
    return h.mismatches;
}

static int test_action_build() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x14 };
    OriginalEntry vanilla(0x004C96E0, entry);
    NoticeStubs stubs;
    UnitActStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok() || *BaseCurrentCount >= MaxBaseNum) {
        return *BaseCurrentCount >= MaxBaseNum ? 0 : 1;
    }
    UnitActScope scope;
    const uint8_t humans_saved = *FactionsStatus;
    const uint32_t state_saved = *GameState;
    NoticeHarness h;
    char label[96];
    int founded = 0, foreign = 0;
    for (int f = 1; f < MaxPlayerNum; f++) {
        int picked = 0, picked_foreign = 0;
        for (int veh_id = f * 7 % 13; veh_id < *VehCurrentCount && picked < 6; veh_id += 13) {
            Map *tile = map_loc(Vehs[veh_id].x, Vehs[veh_id].y);
            if (Vehs[veh_id].faction_id != f || (tile->bit & BIT_BASE_IN_TILE)) {
                continue;
            }
            BOOL in_foreign = tile->territory >= 0 && tile->territory != f;
            if (!in_foreign && picked - picked_foreign >= 4) {
                continue;
            }
            picked++;
            picked_foreign += in_foreign;
            // 0 as loaded; 1 a name; 2 an empty name, human with the governor state bit; 3 not
            // human, omniscient view; 4 at the controls; 5 the faction's first base, human
            for (int v = 0; v < 6; v++) {
                LPCSTR name = (v == 1) ? "Testville" : ((v == 2) ? "" : NULL);
                sprintf_s(label, "action_build(%d) faction %d variant %d", veh_id, f, v);
                unitact_run(h, label, veh_id + v, [&] {
                        *FactionsStatus = humans_saved;
                        *GameState = state_saved;
                        if (v == 2 || v == 5) {
                            *FactionsStatus |= (uint8_t)(1 << f);
                        }
                        if (v == 2) {
                            Vehs[veh_id].state |= VSTATE_UNK_200;
                        } else if (v == 3) {
                            *FactionsStatus &= (uint8_t)~(1 << f);
                            *GameState |= STATE_OMNISCIENT_VIEW;
                        } else if (v == 4) {
                            *FactionIDCurrent = f;
                        } else if (v == 5) {
                            PlayersData[f].current_num_bases = 0;
                        }
                    },
                    [&] { return ((int(__cdecl *)(int, LPCSTR))0x004C96E0)(veh_id, name); },
                    [&] { return action_build(veh_id, name); });
                founded += UnitActNew.ret >= 0;
                foreign += in_foreign;
            }
        }
    }
    *FactionsStatus = humans_saved;
    *GameState = state_saved;
    logf("action_build: %d compared (%d bases founded, %d in another faction's territory, %d with calls), "
        "%d mismatches", h.compared, founded, foreign, h.with_calls, h.mismatches);
    return h.mismatches;
}

static int test_action_arty() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08 };
    OriginalEntry vanilla(0x004CD090, entry);
    NoticeStubs stubs;
    UnitActStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok()) {
        return 1;
    }
    UnitActScope scope;
    NoticeHarness h;
    char label[96];
    int fights = 0, shells = 0;
    for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id += 13) {
        const int x0 = Vehs[veh_id].x, y0 = Vehs[veh_id].y;
        for (int i = 0; i < 26; i += 2) {
            int x = (i < 25) ? x0 + RadiusOffsetX[i] : x0 + 6;
            int y = (i < 25) ? y0 + RadiusOffsetY[i] : y0;
            if (x < 0) {
                x += *MapLongitudeBounds;
            } else if (x >= *MapLongitudeBounds) {
                x -= *MapLongitudeBounds;
            }
            if (y < 0 || y >= *MapLatitudeBounds) {
                continue;
            }
            // as loaded; the unit's faction at the controls; the same with the unit moved
            for (int v = 0; v < 3; v++) {
                sprintf_s(label, "action_arty(%d, %d, %d) variant %d", veh_id, x, y, v);
                unitact_run(h, label, veh_id + i, [&] {
                        *FactionIDCurrent = v ? Vehs[veh_id].faction_id : scope.current_saved;
                        if (v == 2) {
                            Vehs[veh_id].moves_expended = 0xFF;
                        }
                    },
                    [&] { ((func_xyi *)0x004CD090)(veh_id, x, y); return 0; },
                    [&] { action_arty(veh_id, x, y); return 0; });
                for (int c = 0; c < NoticeCallCount && c < MaxNoticeCalls; c++) {
                    fights += NoticeCalls[c].fn == NF_BATTLE_FIGHT_2;
                    shells += NoticeCalls[c].fn == NF_ACT_DESTROY;
                }
            }
        }
    }
    logf("action_arty: %d compared (%d with calls; the DLL fought %d times and shelled %d empty tiles), "
        "%d mismatches", h.compared, h.with_calls, fights, shells, h.mismatches);
    return h.mismatches;
}

// ---------------------------------------------------------------- batch 30: base removal

// The message window's list (see base_kill()): its fields and the id of each entry, so a run can
// be compared and put back. Entries for the base itself are deleted (freed) by base_kill, so
// the tests leave such bases out.
struct MessageListState {
    MessageList fields;
    int ids[256];
    MessageListNode *nodes[256];
    int count; // entries recorded (the list's count, at most 256)
};
static MessageList *message_list() {
    return (MessageList *)((uint8_t *)MessageListBox + 0xBC + (*(int **)MessageListBox)[2]);
}
static void message_list_take(MessageListState *s) {
    MessageList *list = message_list();
    s->fields = *list;
    s->count = 0;
    MessageListNode *node = list->head;
    for (int i = 0; node && i < list->count && i < 256; i++) {
        s->nodes[i] = node;
        s->ids[i] = node->id;
        s->count++;
        node = node->next;
    }
}
static void message_list_put(const MessageListState *s) {
    *message_list() = s->fields;
    for (int i = 0; i < s->count; i++) {
        s->nodes[i]->id = s->ids[i];
    }
}
static BOOL message_list_has(const MessageListState *s, int base_id) {
    for (int i = 0; i < s->count; i++) {
        if (s->ids[i] == (base_id | 0x40000000)) {
            return true;
        }
    }
    return false;
}

static int DrawFixupCount;
static int DrawFixupArgs[4];
static void __cdecl draw_tile_fixup_recorder(int x, int y, int unk, int mode) {
    DrawFixupCount++;
    DrawFixupArgs[0] = x;
    DrawFixupArgs[1] = y;
    DrawFixupArgs[2] = unk;
    DrawFixupArgs[3] = mode;
}

// Everything base_kill() and give_a_base() write outside the snapshot.
struct BaseLifeState {
    int sp[MaxSecretProjectNum];
    int replay_offset;
    uint8_t replay[16];
    uint8_t globals[0x90EA60 - 0x90E8F0];
    int compute_id;
    int selected_id;
    Base *current;
    uint32_t bitfield;
    int extra[5];
    int base_win_id;
    MessageListState list;
    int popp_count;
    char popp_label[32];
    char parse0[64];
    int draw_count;
    int draw_args[4];
    int rand_next;
};
static int *const BaseLifeExtra[] = { (int *)0x0093928C, (int *)0x00939290, (int *)0x008C6B2C,
    (int *)0x008C6B34, (int *)0x009A64DC }; // selection globals veh_kill() renumbers
static int *const ReplayOffset = (int *)0x009469F8;
static uint8_t *const ReplayRecord = (uint8_t *)0x00950324;

static void base_life_take(BaseLifeState *s, int replay_start) {
    memcpy(s->sp, SecretProjectTable, sizeof(s->sp));
    s->replay_offset = *ReplayOffset;
    memcpy(s->replay, ReplayRecord + replay_start, sizeof(s->replay));
    memcpy(s->globals, BaseGlobals, BaseGlobalsSize);
    s->compute_id = *BaseComputeID;
    s->selected_id = *BaseIDCurrentSelected;
    s->current = *BaseCurrent;
    s->bitfield = *UnkBitfield1;
    for (int i = 0; i < 5; i++) {
        s->extra[i] = *BaseLifeExtra[i];
    }
    s->base_win_id = *BaseWinBaseID;
    message_list_take(&s->list);
    s->popp_count = PoppCount;
    memcpy(s->popp_label, PoppLabel, sizeof(s->popp_label));
    memcpy(s->parse0, ParseStrBuffer[0].str, sizeof(s->parse0));
    s->draw_count = DrawFixupCount;
    memcpy(s->draw_args, DrawFixupArgs, sizeof(s->draw_args));
}
static void base_life_put(const BaseLifeState *s) {
    memcpy(SecretProjectTable, s->sp, sizeof(s->sp));
    *ReplayOffset = s->replay_offset;
    memcpy(BaseGlobals, s->globals, BaseGlobalsSize);
    *BaseComputeID = s->compute_id;
    *BaseIDCurrentSelected = s->selected_id;
    *BaseCurrent = s->current;
    *UnkBitfield1 = s->bitfield;
    for (int i = 0; i < 5; i++) {
        *BaseLifeExtra[i] = s->extra[i];
    }
    *BaseWinBaseID = s->base_win_id;
    message_list_put(&s->list);
    PoppCount = 0;
    PoppLabel[0] = 0;
    memcpy(ParseStrBuffer[0].str, s->parse0, sizeof(s->parse0));
    DrawFixupCount = 0;
    memset(DrawFixupArgs, 0, sizeof(DrawFixupArgs));
}
static BOOL base_life_differs(const BaseLifeState *o, const BaseLifeState *n, const char *label) {
    const char *what = NULL;
    if (memcmp(o->sp, n->sp, sizeof(o->sp))) {
        what = "Secret Project table";
    } else if (o->replay_offset != n->replay_offset || memcmp(o->replay, n->replay, sizeof(o->replay))) {
        what = "replay record";
    } else if (o->compute_id != n->compute_id || o->selected_id != n->selected_id
        || o->current != n->current || o->bitfield != n->bitfield || o->base_win_id != n->base_win_id) {
        what = "current base / compute id / UnkBitfield1 / BaseWin";
    } else if (memcmp(o->extra, n->extra, sizeof(o->extra))) {
        what = "selection globals";
    } else if (memcmp(&o->list.fields, &n->list.fields, sizeof(MessageList)) || o->list.count != n->list.count
        || memcmp(o->list.ids, n->list.ids, sizeof(int) * o->list.count)) {
        what = "message list";
    } else if (o->popp_count != n->popp_count || strcmp(o->popp_label, n->popp_label)
        || strcmp(o->parse0, n->parse0)) {
        what = "popp / parse slot 0";
    } else if (o->draw_count != n->draw_count || memcmp(o->draw_args, n->draw_args, sizeof(o->draw_args))) {
        what = "draw_tile_fixup calls";
    } else if (o->rand_next != n->rand_next) {
        what = "random stream";
    }
    if (what) {
        if (label) {
            logf("  %s: %s differs", label, what);
        }
        return true;
    }
    if (base_globals_differ(o->globals, n->globals, label)) {
        return true;
    }
    return false;
}

static int test_base_kill() {
    typedef void func_base(uint32_t);
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x2C };
    JmpStub popp_stub(0x0048C0A0, (void *)popp_recorder);
    if (!popp_stub.ok) {
        return 1;
    }
    const BOOL draw_saved = VehBattleUnkTgl[0];
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    BaseLifeState saved, state_o, state_n;
    base_life_take(&saved, *ReplayOffset);
    const int count = base.base_count_global;
    int compared = 0, mismatches = 0, in_list = 0, corner = 0, rehome_none = 0, sea = 0;
    int units_killed = 0, projects = 0, list_renumbered = 0;
    char label[96];
    // variant 0: as loaded; 1: an energy market corner run from this headquarters; 2: the
    // faction's other bases handed to faction 0 for the run, so its units can't be rehomed
    auto run = [&](BOOL orig, int base_id, int variant, BaseLifeState *out) {
        snap_restore(&base);
        base_life_put(&saved);
        VehBattleUnkTgl[0] = true; // kill() skips its map redraw
        uint32_t faction_id = Bases[base_id].faction_id_current;
        if (variant == 1) {
            PlayersData[faction_id].corner_market_turn = *TurnCurrentNum + 5;
            PlayersData[faction_id].corner_market_active = 1;
        } else if (variant == 2) {
            for (int i = 0; i < count; i++) {
                if (i != base_id && Bases[i].faction_id_current == faction_id) {
                    Bases[i].faction_id_current = 0;
                }
            }
        }
        reseed((uint32_t)(base_id * 31 + variant));
        if (orig) {
            OriginalEntry original(0x004E5250, entry);
            ((func_base *)0x004E5250)(base_id);
        } else {
            base_kill(base_id);
        }
        base_life_take(out, saved.replay_offset);
        out->rand_next = _rand();
        VehBattleUnkTgl[0] = draw_saved;
    };
    uint32_t hq_offset, hq_mask;
    bitmask(FAC_HEADQUARTERS, &hq_offset, &hq_mask);
    for (int base_id = 0; base_id < count; base_id++) {
        // from the snapshot: the previous run left the table shifted
        BOOL hq = (base.bases[base_id].facilities_built[hq_offset] & hq_mask) != 0;
        BOOL ocean = is_ocean(base.bases[base_id].x, base.bases[base_id].y);
        if (base_id % 7 && !hq && !(ocean && base_id % 3 == 0)) {
            continue; // every 7th base, every headquarters, every 3rd sea base
        }
        if (message_list_has(&saved.list, base_id)) {
            in_list++; // the original would free its message list entries: not repeatable
            continue;
        }
        for (int v = 0; v < 3; v++) {
            if ((v == 1 && !hq) || (v == 2 && base_id % 3)) {
                continue;
            }
            REGRESS_CTX("base_kill(%d) variant %d", base_id, v);
            int vehs_before = base.veh_count_global;
            run(true, base_id, v, &state_o);
            snap_take(&after_orig);
            units_killed += vehs_before - after_orig.veh_count_global;
            corner += state_o.popp_count;
            sea += ocean;
            rehome_none += v == 2;
            for (int i = 0; i < MaxSecretProjectNum; i++) {
                projects += state_o.sp[i] == -2 && saved.sp[i] == base_id;
            }
            for (int i = 0; i < saved.list.count && i < state_o.list.count; i++) {
                if (state_o.list.ids[i] != saved.list.ids[i]) {
                    list_renumbered++;
                    break;
                }
            }
            run(false, base_id, v, &state_n);
            snap_take(&after_new);
            compared++;
            sprintf_s(label, "base_kill(%d) variant %d", base_id, v);
            if (snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL)
                || base_life_differs(&state_o, &state_n, mismatches < MAX_DETAIL ? label : NULL)) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    base_life_put(&saved);
    VehBattleUnkTgl[0] = draw_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    int list_bases = 0;
    for (int i = 0; i < saved.list.count; i++) {
        list_bases += (saved.list.ids[i] & 0x40000000) != 0;
    }
    logf("base_kill: %d compared (by the original: %d units killed, %d corners foiled, %d sea bases, "
        "%d runs with no base to rehome to, %d projects lost, %d runs renumbered list entries; %d "
        "bases skipped for their message list entries, list %d entries, %d of bases), %d mismatches",
        compared, units_killed, corner, sea, rehome_none, projects, list_renumbered, in_list,
        saved.list.count, list_bases, mismatches);
    return mismatches;
}

static int test_give_a_base() {
    typedef void func_give(uint32_t, uint32_t);
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x1C };
    JmpStub draw_stub(0x0046AF90, (void *)draw_tile_fixup_recorder);
    JmpStub popp_stub(0x0048C0A0, (void *)popp_recorder);
    if (!draw_stub.ok || !popp_stub.ok) {
        return 1;
    }
    const BOOL draw_saved = VehBattleUnkTgl[0];
    const int proposal_saved = *DiploCounterProposalID;
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    BaseLifeState saved, state_o, state_n;
    base_life_take(&saved, *ReplayOffset);
    const int count = base.base_count_global;
    int compared = 0, mismatches = 0, changed_sides = 0, moved = 0, assimilating = 0, killed = 0;
    char label[96];
    // variant 0: to the next faction, proposal as loaded; 1: the same taken by force (proposal
    // 3); 2: back to its former owner (bases that changed hands), proposal 3
    auto run = [&](BOOL orig, int base_id, uint32_t faction_id, int variant, BaseLifeState *out) {
        snap_restore(&base);
        base_life_put(&saved);
        VehBattleUnkTgl[0] = true;
        *DiploCounterProposalID = variant ? 3 : proposal_saved;
        reseed((uint32_t)(base_id * 17 + variant));
        if (orig) {
            OriginalEntry original(0x0054CF40, entry);
            ((func_give *)0x0054CF40)(base_id, faction_id);
        } else {
            give_a_base(base_id, faction_id);
        }
        base_life_take(out, saved.replay_offset);
        out->rand_next = _rand();
        VehBattleUnkTgl[0] = draw_saved;
    };
    for (int base_id = 0; base_id < count; base_id += 4) {
        uint32_t owner = base.bases[base_id].faction_id_current; // from the snapshot
        uint32_t former = base.bases[base_id].faction_id_former;
        for (int v = 0; v < 3; v++) {
            uint32_t faction_id = owner % 7 + 1; // the next of factions 1..7
            if (v == 2) {
                if (former == owner) {
                    continue;
                }
                faction_id = former;
            }
            REGRESS_CTX("give_a_base(%d, %u) variant %d", base_id, faction_id, v);
            run(true, base_id, faction_id, v, &state_o);
            snap_take(&after_orig);
            killed += base.veh_count_global - after_orig.veh_count_global;
            if (after_orig.veh_count_global == base.veh_count_global) { // same indices
                for (int i = 0; i < base.veh_count_global; i++) {
                    const Veh *before = &base.vehs[i], *after = &after_orig.vehs[i];
                    changed_sides += before->faction_id == owner && after->faction_id == faction_id;
                    moved += before->faction_id == owner && after->faction_id == owner
                        && (before->x != after->x || before->y != after->y);
                }
            }
            assimilating += after_orig.bases[base_id].assimilation_turns_left == 50;
            run(false, base_id, faction_id, v, &state_n);
            snap_take(&after_new);
            compared++;
            sprintf_s(label, "give_a_base(%d, %u) variant %d", base_id, faction_id, v);
            if (snap_differs(&after_orig, &after_new, mismatches < MAX_DETAIL ? label : NULL)
                || base_life_differs(&state_o, &state_n, mismatches < MAX_DETAIL ? label : NULL)) {
                mismatches++;
            }
        }
    }
    snap_restore(&base);
    base_life_put(&saved);
    *DiploCounterProposalID = proposal_saved;
    VehBattleUnkTgl[0] = draw_saved;
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    logf("give_a_base: %d compared (by the original: %d units changed sides, %d moved to another "
        "base, %d killed, %d bases assimilating), %d mismatches", compared, changed_sides, moved,
        killed, assimilating, mismatches);
    return mismatches;
}

static int test_base_swap() {
    typedef int func_swap(uint32_t, uint32_t);
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08 };
    OriginalEntry vanilla(0x0054D410, entry);
    NoticeStubs stubs;
    DiploTwoStubs stubs2;
    JmpStub draw_stub(0x0046AF90, (void *)draw_tile_fixup_recorder);
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok() || !draw_stub.ok) {
        return 1;
    }
    const BOOL draw_saved = VehBattleUnkTgl[0];
    GameSnapshot base, after_orig, after_new;
    snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
    snap_take(&base);
    NoticeState notice_saved, notice_o, notice_n;
    NoticeCallCount = 0;
    notice_take(&notice_saved, 0);
    BaseLifeState life_saved, life_o, life_n;
    base_life_take(&life_saved, *ReplayOffset);
    static const char *const outcomes[] = { "NOHQ", "NOMYHQ", "NEVERBASESWAP", "YESBASESWAP",
        "NOBASESWAP3", "PAYBASESWAP", "NOBASESWAP", "NOBASESWAP2", "TECHBASESWAP", "BASEOK" };
    const int outcome_count = (int)(sizeof(outcomes) / sizeof(outcomes[0]));
    int outcome_seen[16] = {};
    int compared = 0, mismatches = 0, made = 0, skipped = 0, paid_energy = 0, paid_tech = 0;
    int tech_calls = 0; // trades with more calls than the offer and BASEOK
    char label[96];
    // the bases a faction can ask for or offer: a plain one and its headquarters (from the
    // snapshot: the runs change owners and counts)
    auto bases_of = [&](uint32_t faction_id, int *plain, int *hq) {
        *plain = -1;
        *hq = -1;
        uint32_t offset, mask;
        bitmask(FAC_HEADQUARTERS, &offset, &mask);
        for (int i = 0; i < base.base_count_global; i++) {
            if (base.bases[i].faction_id_current == faction_id) {
                if (base.bases[i].facilities_built[offset] & mask) {
                    *hq = i;
                } else if (*plain < 0) {
                    *plain = i;
                }
            }
        }
    };
    struct SwapCase { int proposal, ask_hq, bid, bid_hq, answer, energy, revenge, single; };
    // bid: 0 none, 1 a plain base, 2 the headquarters; energy: 0 as loaded, 1 rich, 2 none
    static const SwapCase cases[] = {
        { 0, 0, 1, 0, 1, 0, 0, 0 }, // two bases, by chance
        { 2, 0, 0, 0, 1, 1, 0, 0 }, // pay energy, accepted
        { 2, 0, 0, 0, 0, 1, 0, 0 }, // pay energy, declined
        { 5, 0, 0, 0, 1, 2, 0, 0 }, // too poor to pay
        { 4, 0, 0, 0, 1, 0, 0, 0 }, // technology
        { 4, 0, 0, 0, 0, 0, 0, 0 }, // technology, declined
        { 2, 0, 0, 0, 1, 2, 0, 0 }, // too poor, so technology
        { 0, 0, 0, 0, 1, 0, 0, 0 }, // other proposals
        { 0, 1, 1, 0, 1, 0, 0, 0 }, // its headquarters asked for
        { 0, 0, 2, 1, 1, 0, 0, 0 }, // a headquarters offered
        { 0, 0, 1, 0, 1, 0, 1, 0 }, // wants revenge
        { 3, 0, 1, 0, 1, 0, 1, 0 }, // wants revenge, taken by force
        { 0, 0, 1, 0, 1, 0, 0, 1 }, // its only base offered
    };
    for (uint32_t f1 = 1; f1 < MaxPlayerNum; f1++) {
        int partners = 0;
        for (uint32_t step = 1; step < MaxPlayerNum && partners < 2; step++) {
            uint32_t f2 = (f1 + step - 1) % (MaxPlayerNum - 1) + 1;
            if (f2 == f1 || base.players[f1].current_num_bases < 2
                || base.players[f2].current_num_bases < 2) {
                continue;
            }
            partners++;
            int ask_plain, ask_hq, bid_plain, bid_hq;
            bases_of(f2, &ask_plain, &ask_hq);
            bases_of(f1, &bid_plain, &bid_hq);
            for (int c = 0; c < (int)(sizeof(cases) / sizeof(cases[0])); c++) {
                const SwapCase &sc = cases[c];
                int ask = sc.ask_hq ? ask_hq : ask_plain;
                int bid = sc.bid == 2 ? bid_hq : (sc.bid == 1 ? bid_plain : -1);
                if (ask < 0 || (sc.bid && bid < 0)) {
                    continue;
                }
                if (bid < 0 && !base.players[f2].ranking) {
                    skipped++; // the price divides by the partner's ranking: 0 crashes both
                    continue;
                }
                auto prepare = [&]() {
                    snap_restore(&base);
                    notice_restore(&notice_saved);
                    base_life_put(&life_saved);
                    NoticeCallCount = 0;
                    memset(NoticeAnswer, 0, sizeof(NoticeAnswer));
                    NoticeFirstAnswer = -1;
                    NoticeAnswer[NF_X_POPS_11] = sc.answer;
                    VehBattleUnkTgl[0] = true;
                    *DiploCounterProposalID = sc.proposal;
                    *DiploBaseSwapAskID = ask;
                    *DiploBaseSwapBidID = bid;
                    if (sc.energy == 1) {
                        PlayersData[f1].energy_reserves = 100000;
                    } else if (sc.energy == 2) {
                        PlayersData[f1].energy_reserves = 0;
                    }
                    if (sc.revenge) {
                        PlayersData[f2].diplo_treaties[f1] |= DTREATY_WANT_REVENGE;
                    }
                    if (sc.single) {
                        PlayersData[f1].current_num_bases = 1;
                    }
                    reseed(f1 * 131 + f2 * 7 + c);
                };
                REGRESS_CTX("base_swap(%u, %u) case %d", f1, f2, c);
                prepare();
                int ret_o = ((func_swap *)0x0054D410)(f1, f2);
                snap_take(&after_orig);
                notice_take(&notice_o, ret_o);
                base_life_take(&life_o, life_saved.replay_offset);
                life_o.rand_next = _rand();
                prepare();
                int ret_n = base_swap(f1, f2);
                snap_take(&after_new);
                notice_take(&notice_n, ret_n);
                base_life_take(&life_n, life_saved.replay_offset);
                life_n.rand_next = _rand();
                compared++;
                made += !ret_o;
                // the last popup (when every call was recorded) and the offer a trade was made on
                if (notice_o.call_count > 0 && notice_o.call_count <= MaxNoticeCalls) {
                    const char *last = notice_o.calls[notice_o.call_count - 1].label;
                    for (int k = 0; k < outcome_count; k++) {
                        outcome_seen[k] += !strcmp(last, outcomes[k]);
                    }
                }
                for (int k = 0; !ret_o && k < notice_o.call_count && k < MaxNoticeCalls; k++) {
                    paid_energy += !strcmp(notice_o.calls[k].label, "PAYBASESWAP");
                    paid_tech += !strcmp(notice_o.calls[k].label, "TECHBASESWAP");
                }
                tech_calls += !ret_o && notice_o.call_count > 2;
                sprintf_s(label, "base_swap(%u, %u) case %d", f1, f2, c);
                const char *detail = mismatches < MAX_DETAIL ? label : NULL;
                if (notice_differs(&notice_o, &notice_n, detail)
                    || snap_differs(&after_orig, &after_new, detail)
                    || base_life_differs(&life_o, &life_n, detail)) {
                    mismatches++;
                }
            }
        }
    }
    snap_restore(&base);
    notice_restore(&notice_saved);
    base_life_put(&life_saved);
    VehBattleUnkTgl[0] = draw_saved;
    NoticeCallCount = 0;
    memset(NoticeAnswer, 0, sizeof(NoticeAnswer));
    snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    char seen[256] = "";
    for (int k = 0; k < outcome_count; k++) {
        char item[32];
        _snprintf_s(item, _TRUNCATE, " %s %d", outcomes[k], outcome_seen[k]);
        strcat_s(seen, item);
    }
    logf("base_swap: %d compared (by the original: %d trades made, %d for energy, %d for "
        "technology, %d with calls between; last popup:%s; %d skipped for a partner ranked 0), %d "
        "mismatches", compared, made, paid_energy, paid_tech, tech_calls, seen, skipped,
        mismatches);
    return mismatches;
}

// ---------------------------------------------------------------- batch 30: planet events

static int EventNetTasksCalls;
static int EventNetTasksFaction = -1;
static int __cdecl event_tech_pick(int faction_id, int unk, int faction_id_from, LPCSTR label) {
    return notice_record(NF_TECH_PICK, label, NULL, NULL, faction_id, unk, faction_id_from);
}
// NetDaemon::net_tasks(): a faction's research goal "arrives" on the third call (NoticeAnswer)
static int __fastcall event_net_tasks(void *This, void *) {
    notice_record(NF_NET_TASKS, NULL, NULL, NULL, This == NetDaemonVar, 0, 0);
    if (++EventNetTasksCalls >= 3 && EventNetTasksFaction >= 0) {
        PlayersData[EventNetTasksFaction].tech_id_researching = NoticeAnswer[NF_NET_TASKS];
    }
    return 0;
}
static int __fastcall event_netmsg_close(void *This, void *) {
    return notice_record(NF_NETMSG_CLOSE, NULL, NULL, NULL, This == NetMsgVar, 0, 0);
}
static void __fastcall event_net_synch(void *This, void *, int16_t type, int id, int id2,
    int UNUSED(unk), char *UNUSED(data), uint32_t UNUSED(delay), int16_t UNUSED(unk2)) {
    notice_record(NF_NET_SYNCH, NULL, NULL, NULL, type, id, (This == NetDaemonVar) ? id2 : -99);
}
static int __cdecl event_x_pops(LPCSTR label, Sprite *picture, int(__cdecl *callback)()) {
    return notice_record(NF_X_POPS, label, NULL, NULL, (int)picture, notice_callback_id(callback), 0);
}
static void __cdecl event_draw_map(int mode) {
    notice_record(NF_DRAW_MAP, NULL, NULL, NULL, mode, 0, 0);
}

static int test_tech_selection() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0xA1, 0x60, 0xF6 };
    OriginalEntry vanilla(0x005BE380, entry);
    NoticeStubs stubs;
    JmpStub pick_stub(0x005BEB70, (void *)event_tech_pick);
    JmpStub tasks_stub(0x00530360, (void *)event_net_tasks);
    JmpStub close_stub(0x0047A7D0, (void *)event_netmsg_close);
    JmpStub synch_stub(0x00532E00, (void *)event_net_synch);
    if (!vanilla.ok || !stubs.ok() || !pick_stub.ok || !tasks_stub.ok || !close_stub.ok
        || !synch_stub.ok) {
        return 1;
    }
    const BOOL net_saved = *IsMultiplayerNet;
    const int turn_a_saved = *ControlTurnA;
    const int current_saved = *FactionIDCurrent;
    char label[96];
    int waited = 0, sent = 0;
    {
        NoticeHarness h;
        for (int f = 1; f < MaxPlayerNum; f++) {
            // 0 single player; 1 network, another human player whose goal is already there;
            // 2 the same, the goal arrives on the third net_tasks(); 3 network, the player at the
            // controls (pick, send, wait for the echo); 4 network, a computer faction; 5 network,
            // another human player with the wait aborted (ControlTurnA)
            for (int v = 0; v < 6; v++) {
                auto setup = [&] {
                    *IsMultiplayerNet = v > 0;
                    *ControlTurnA = v == 5;
                    EventNetTasksCalls = 0;
                    EventNetTasksFaction = f;
                    NoticeAnswer[NF_TECH_PICK] = 10 + f + v;
                    NoticeAnswer[NF_NET_TASKS] = 20 + f;
                    *FactionIDCurrent = (v == 3) ? f : (f == current_saved ? f % 7 + 1 : current_saved);
                    if (v == 4) {
                        FactionsStatus[0] &= ~(1 << f);
                    } else if (v) {
                        FactionsStatus[0] |= 1 << f;
                    }
                    PlayersData[f].tech_id_researching = (v == 1) ? 7 : -1;
                };
                sprintf_s(label, "tech_selection(%d) variant %d", f, v);
                REGRESS_CTX("%s", label);
                h.run(label, setup, [&] { return ((func_i *)0x005BE380)(f); },
                    [&] { return tech_selection(f); });
                for (int i = 0; i < h.orig.call_count && i < MaxNoticeCalls; i++) {
                    waited += h.orig.calls[i].fn == NF_NET_TASKS;
                    sent += h.orig.calls[i].fn == NF_NET_SYNCH;
                }
            }
        }
        logf("tech_selection: %d compared (by the original: %d net_tasks waits, %d goals sent), "
            "%d mismatches", h.compared, waited, sent, h.mismatches);
        *IsMultiplayerNet = net_saved;
        *ControlTurnA = turn_a_saved;
        *FactionIDCurrent = current_saved;
        EventNetTasksFaction = -1;
        return h.mismatches;
    }
}

static int test_climactic_changes() {
    static const uint8_t entry[6] = { 0xA1, 0xD8, 0x67, 0x9A, 0x00, 0x85 };
    OriginalEntry vanilla(0x00524210, entry);
    NoticeStubs stubs;
    JmpStub draw_map_stub(0x0046B190, (void *)event_draw_map);
    JmpStub mapwin_stub(0x0046FD90, (void *)climate_mapwin_clear_recorder);
    JmpStub worldwin_stub(0x004C45E0, (void *)climate_worldwin_clear_recorder);
    TimeGetTimePin pin; // world_climate() ends by reseeding from the clock
    if (!vanilla.ok || !stubs.ok() || !draw_map_stub.ok || !mapwin_stub.ok || !worldwin_stub.ok
        || !pin.ok()) {
        return 1;
    }
    int *const values[5] = { ClimateFutureChange, ClimateValueA, ClimateValueB, ClimateValueC,
        MapSeaLevel };
    int saved[5];
    for (int i = 0; i < 5; i++) {
        saved[i] = *values[i];
    }
    // future change, rate, threshold, counter: nothing pending; pending, counter short of the
    // threshold; rising one step; falling with the counter carrying over; a counter far past it
    const int cases[][4] = { { 0, 3, 20, 5 }, { 2, 3, 20, 5 }, { 2, 3, 20, 18 }, { -2, 7, 20, 15 },
        { 5, 45, 20, 0 }, { -1, 20, 20, 0 } };
    int extra = 0, stepped = 0;
    char label[96];
    SnapHarness h("climactic_changes");
    h.empty_world(); // world_climate() runs alt_set(), which submerges bases and kills units
    for (int c = 0; c < (int)(sizeof(cases) / sizeof(cases[0])); c++) {
        int state_o[5], calls_o = 0, clears_o = 0, rand_o = 0, rand_n = 0;
        uint32_t seed_o = 0, seed_n = 0;
        NoticeCall log_o[MaxNoticeCalls];
        auto prepare = [&] {
            *ClimateFutureChange = cases[c][0];
            *ClimateValueA = cases[c][1];
            *ClimateValueB = cases[c][2];
            *ClimateValueC = cases[c][3];
            *MapSeaLevel = saved[4];
            NoticeCallCount = 0;
            memset(NoticeAnswer, 0, sizeof(NoticeAnswer));
            ClimateClearCount = 0;
            FixedTimeValue = 1000003u * (c + 1);
            reseed(c + 21);
        };
        sprintf_s(label, "climactic_changes() case %d", c);
        REGRESS_CTX("%s", label);
        h.compare(label,
            [&] {
                prepare();
                ((func_void *)0x00524210)();
                for (int i = 0; i < 5; i++) {
                    state_o[i] = *values[i];
                }
                calls_o = NoticeCallCount;
                memcpy(log_o, NoticeCalls, sizeof(log_o));
                clears_o = ClimateClearCount;
                seed_o = Rand->get_seed();
                rand_o = _rand();
            },
            [&] {
                prepare();
                climactic_changes();
                seed_n = Rand->get_seed();
                rand_n = _rand();
            });
        stepped += state_o[4] != saved[4];
        BOOL diff = calls_o != NoticeCallCount || clears_o != ClimateClearCount || seed_o != seed_n
            || rand_o != rand_n;
        for (int i = 0; i < 5; i++) {
            diff |= state_o[i] != *values[i];
        }
        for (int i = 0; i < calls_o && i < MaxNoticeCalls && !diff; i++) {
            diff = log_o[i].fn != NoticeCalls[i].fn || strcmp(log_o[i].label, NoticeCalls[i].label)
                || strcmp(log_o[i].pic, NoticeCalls[i].pic) || memcmp(log_o[i].args, NoticeCalls[i].args,
                sizeof(log_o[i].args));
        }
        if (diff) {
            if (extra < MAX_DETAIL) {
                logf("  %s: orig future %d C %d sea %d, %d calls %d clears; new future %d C %d sea %d, "
                    "%d calls %d clears", label, state_o[0], state_o[3], state_o[4], calls_o, clears_o,
                    *ClimateFutureChange, *ClimateValueC, *MapSeaLevel, NoticeCallCount, ClimateClearCount);
            }
            extra++;
        }
    }
    for (int i = 0; i < 5; i++) {
        *values[i] = saved[i];
    }
    logf("climactic_changes: %d sea level steps by the original", stepped);
    return h.report() + extra;
}

static int test_major_atrocity() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x58 };
    OriginalEntry vanilla(0x0055F450, entry);
    NoticeStubs stubs;
    if (!vanilla.ok || !stubs.ok()) {
        return 1;
    }
    const int repeal_saved = ProposalPassCount[PROP_REPEAL_UN_CHARTER];
    char label[96];
    int vendettas = 0;
    NoticeHarness h;
    for (int f = 1; f < MaxPlayerNum; f++) {
        for (int tgt = -1; tgt < MaxPlayerNum; tgt++) {
            // 0 as loaded; 1 the UN Charter repealed; 2 the target marked DTREATY_UNK_80000;
            // 3 pacts and no vendettas with everyone, blemishes near the cap; 4 the faction human
            for (int v = 0; v < 5; v++) {
                if (tgt == f && v) {
                    continue;
                }
                auto setup = [&] {
                    ProposalPassCount[PROP_REPEAL_UN_CHARTER] = (v == 1)
                        ? ProposalPassCount[PROP_REINSTATE_UN_CHARTER] + 1 : repeal_saved;
                    if (v == 2 && tgt >= 0) {
                        PlayersData[f].diplo_treaties[tgt] |= DTREATY_UNK_80000;
                    } else if (v == 3) {
                        for (int i = 1; i < MaxPlayerNum; i++) {
                            PlayersData[f].diplo_treaties[i] = (PlayersData[f].diplo_treaties[i]
                                & ~(DTREATY_VENDETTA | DTREATY_UNK_80000)) | DTREATY_PACT | DTREATY_COMMLINK;
                        }
                        PlayersData[f].integrity_blemishes = 98;
                    } else if (v == 4) {
                        FactionsStatus[0] |= 1 << f;
                    }
                };
                sprintf_s(label, "major_atrocity(%d, %d) variant %d", f, tgt, v);
                REGRESS_CTX("%s", label);
                h.run(label, setup, [&] { ((func_diplo2 *)0x0055F450)(f, tgt); return 0; },
                    [&] { major_atrocity(f, tgt); return 0; });
                for (int i = 0; i < h.orig.call_count && i < MaxNoticeCalls; i++) {
                    vendettas += h.orig.calls[i].fn == NF_POPP;
                }
            }
        }
    }
    ProposalPassCount[PROP_REPEAL_UN_CHARTER] = repeal_saved;
    logf("major_atrocity: %d compared (%d changed the game, by the original %d vendetta notices), "
        "%d mismatches", h.compared, h.changed, vendettas, h.mismatches);
    return h.mismatches;
}

static int test_atrocity() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C };
    static const uint8_t entry_major[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x58 };
    OriginalEntry vanilla(0x0055EEE0, entry);
    OriginalEntry vanilla_major(0x0055F450, entry_major); // the exe's atrocity calls the exe's
    NoticeStubs stubs;
    DiploTwoStubs stubs2; // diplomacy_caption
    JmpStub x_pops_stub(0x005BF5D0, (void *)event_x_pops);
    if (!vanilla.ok || !vanilla_major.ok || !stubs.ok() || !stubs2.ok() || !x_pops_stub.ok) {
        return 1;
    }
    typedef void func_atrocity(int, int, int, int);
    const int repeal_saved = ProposalPassCount[PROP_REPEAL_UN_CHARTER];
    const int current_saved = *FactionIDCurrent;
    const BOOL expansion_saved = *ExpansionEnabled;
    char label[96];
    int majors = 0, dialogs = 0, notices = 0;
    NoticeHarness h;
    for (int f = 1; f < MaxPlayerNum; f++) {
        for (int tgt = 1; tgt < MaxPlayerNum; tgt++) {
            // 0 as loaded, counted; 1 not counted; 2 sunspots; 3 the UN Charter repealed; 4 the
            // faction human, in contact and watched by everyone (vendetta by chance); 5 the same,
            // everyone wanting revenge, the faction at the controls (dialog); 6 the fifth
            // atrocity of a human (the target marked); 7 one past the major atrocity limit;
            // 8 aliens counted; 9 SMAC rules (no alien exemption)
            for (int v = 0; v < 10; v++) {
                int counted = (v == 1) ? 0 : (v == 6 ? 2 : 1);
                int aliens = v == 8;
                auto setup = [&] {
                    reseed((uint32_t)(f * 131 + tgt * 17 + v));
                    ProposalPassCount[PROP_REPEAL_UN_CHARTER] = (v == 3)
                        ? ProposalPassCount[PROP_REINSTATE_UN_CHARTER] + 1 : repeal_saved;
                    *SunspotDuration = (v == 2) ? 3 : 0;
                    *ExpansionEnabled = (v == 9) ? false : expansion_saved;
                    *FactionIDCurrent = (v == 5) ? f : current_saved;
                    if (v == 4 || v == 5 || v == 6) {
                        FactionsStatus[0] = (uint8_t)((FactionsStatus[0] & 1) | (1 << f));
                        for (int i = 1; i < MaxPlayerNum; i++) {
                            if (i == f) {
                                continue;
                            }
                            PlayersData[i].diplo_treaties[f] = (PlayersData[i].diplo_treaties[f]
                                & ~(DTREATY_VENDETTA | DTREATY_PACT | (v == 5 ? 0 : DTREATY_WANT_REVENGE)))
                                | DTREATY_COMMLINK | DTREATY_UNK_200000 | (v == 5 ? DTREATY_WANT_REVENGE : 0);
                            PlayersData[f].diplo_treaties[i] = (PlayersData[f].diplo_treaties[i]
                                & ~(DTREATY_VENDETTA | DTREATY_PACT)) | DTREATY_COMMLINK;
                        }
                        PlayersData[tgt].diplo_treaties[f] &= ~DTREATY_ATROCITY_VICTIM;
                    }
                    if (v == 1) {
                        PlayersData[f].atrocities = 1 + (f + tgt) % 3;
                    } else if (v == 6) {
                        PlayersData[f].atrocities = 4;
                        PlayersData[f].diff_level = 2;
                    } else if (v == 7) {
                        PlayersData[f].atrocities = (8 - PlayersData[f].diff_level) * 4;
                    }
                };
                sprintf_s(label, "atrocity(%d, %d, %d, %d) variant %d", f, tgt, counted, aliens, v);
                REGRESS_CTX("%s", label);
                h.run(label, setup,
                    [&] { ((func_atrocity *)0x0055EEE0)(f, tgt, counted, aliens); return _rand(); },
                    [&] { atrocity(f, tgt, counted, aliens); return _rand(); });
                for (int i = 0; i < h.orig.call_count && i < MaxNoticeCalls; i++) {
                    majors += h.orig.calls[i].fn == NF_POPP;
                    dialogs += h.orig.calls[i].fn == NF_X_POPS;
                    notices += h.orig.calls[i].fn == NF_NETMSG_POP;
                }
            }
        }
    }
    ProposalPassCount[PROP_REPEAL_UN_CHARTER] = repeal_saved;
    *FactionIDCurrent = current_saved;
    *ExpansionEnabled = expansion_saved;
    *SunspotDuration = 0;
    logf("atrocity: %d compared (%d changed the game; by the original %d notices, %d vendetta "
        "dialogs, %d major atrocity notices), %d mismatches", h.compared, h.changed, notices,
        dialogs, majors, h.mismatches);
    return h.mismatches;
}

// base_kill() stays exe code (popups, replay, the Secret Project table): record it instead.
// veh_kill() runs for real: alt_set() steps its stack loop back after each kill, which only
// ends when the unit is really gone.
static void __cdecl event_base_kill(int base_id) {
    notice_record(NF_BASE_KILL, NULL, NULL, NULL, base_id, 0, 0);
}
static int __fastcall event_console_focus(void *This, void *, int x, int y, int faction_id) {
    return notice_record(NF_CONSOLE_FOCUS, NULL, NULL, NULL, x, y, (This == ConsoleVar) ? faction_id : -99);
}

static int test_alt_set_game() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10 };
    OriginalEntry vanilla(0x00591290, entry);
    NoticeStubs stubs; // popp, X_pop_2
    JmpStub base_kill_stub((uint32_t)&base_kill, (void *)event_base_kill);
    JmpStub focus_stub(0x005108A0, (void *)event_console_focus);
    if (!vanilla.ok || !stubs.ok() || !base_kill_stub.ok || !focus_stub.ok) {
        return 1;
    }
    // selection globals veh_kill() renumbers (as in the alien_start and stack_kill tests)
    static int *const extra_globals[] = { (int *)0x0093928C, (int *)0x00939290, (int *)0x008C6B2C,
        (int *)0x008C6B34, (int *)0x009A64DC };
    int extra_saved[5], extra_o[5];
    for (int i = 0; i < 5; i++) {
        extra_saved[i] = *extra_globals[i];
    }
    typedef void func_alt_set(int, int, uint32_t);
    // what delete_landmark() touches outside GameSnapshot: the relief tables and the map views
    uint8_t *tables[5];
    landmark_tables(tables);
    static uint8_t tables_saved[5][LandmarkAltSize], tables_o[5][LandmarkAltSize];
    for (int t = 0; t < 5; t++) {
        memcpy(tables_saved[t], tables[t], LandmarkAltSize);
    }
    const uint32_t cache_size = *MapArea * 12;
    int unk_saved[MaxMapWinNum], unk_o[MaxMapWinNum];
    uint8_t *cache_saved[MaxMapWinNum] = {}, *cache_o[MaxMapWinNum] = {};
    for (int w = 0; w < MaxMapWinNum; w++) {
        unk_saved[w] = MapWinList[w]->unk_1;
        if (MapWinList[w]->draw_toggle_a && MapWinList[w]->tile_cache) {
            cache_saved[w] = (uint8_t *)malloc(cache_size);
            cache_o[w] = (uint8_t *)malloc(cache_size);
            memcpy(cache_saved[w], MapWinList[w]->tile_cache, cache_size);
        }
    }
    const int turn_saved = *TurnCurrentNum, current_saved = *FactionIDCurrent;
    const BOOL net_saved = *IsMultiplayerNet, build_saved = *WorldBuildUnk2;
    const BOOL draw_saved = VehBattleUnkTgl[0];
    const int turn_c_saved = *ControlTurnC;
    // tiles: every 3rd base, units outside bases (every 23rd), and 48 tiles across the map
    static int tiles[512][3]; // x, y, base id or -1
    int tile_count = 0;
    for (int i = 0; i < *BaseCurrentCount && tile_count < 200; i += 3) {
        tiles[tile_count][0] = Bases[i].x;
        tiles[tile_count][1] = Bases[i].y;
        tiles[tile_count++][2] = i;
    }
    for (int i = 0; i < *VehCurrentCount && tile_count < 300; i += 23) {
        if (base_at(Vehs[i].x, Vehs[i].y) < 0) {
            tiles[tile_count][0] = Vehs[i].x;
            tiles[tile_count][1] = Vehs[i].y;
            tiles[tile_count++][2] = -1;
        }
    }
    for (int k = 0; k < 48; k++) {
        sample_tile(k, 48, &tiles[tile_count][0], &tiles[tile_count][1]);
        tiles[tile_count][2] = base_at(tiles[tile_count][0], tiles[tile_count][1]);
        tile_count++;
    }
    char label[96];
    int kills = 0, flooded = 0, popups = 0, rockier = 0, flatter = 0;
    int extra = 0;
    {
        NoticeHarness h;
        for (int t = 0; t < tile_count; t++) {
            const int x = tiles[t][0], y = tiles[t][1], base_id = tiles[t][2];
            const int old = map_loc(x, y)->climate >> 5;
            // 0 one step up; 1 one step down; 2 under the sea; 3 dry land; for bases also: 4 under
            // the sea with the owner at the controls; 5 the owner's only base; 6 a network game
            // (ControlTurnC clear) during a battle redraw; 7 turn 0; 8 world building; 9 a base of
            // size 2
            for (int v = 0; v < (base_id >= 0 ? 10 : 4); v++) {
                const uint32_t alt = v == 0 ? (old < 7 ? old + 1 : 7) : v == 1 ? (old > 0 ? old - 1 : 0)
                    : v == 3 ? 3 : 2;
                auto setup = [&] {
                    for (int i = 0; i < 5; i++) {
                        memcpy(tables[i], tables_saved[i], LandmarkAltSize);
                    }
                    for (int w = 0; w < MaxMapWinNum; w++) {
                        MapWinList[w]->unk_1 = unk_saved[w];
                        if (cache_saved[w]) {
                            memcpy(MapWinList[w]->tile_cache, cache_saved[w], cache_size);
                        }
                    }
                    *TurnCurrentNum = (v == 7) ? 0 : turn_saved;
                    *FactionIDCurrent = (v == 4) ? Bases[base_id].faction_id_current : current_saved;
                    *IsMultiplayerNet = (v == 6) ? true : net_saved;
                    *ControlTurnC = (v == 6) ? 0 : turn_c_saved;
                    VehBattleUnkTgl[0] = (v == 6) ? true : draw_saved;
                    *WorldBuildUnk2 = (v == 8) ? true : build_saved;
                    for (int i = 0; i < 5; i++) {
                        *extra_globals[i] = extra_saved[i];
                    }
                    if (v == 5) {
                        PlayersData[Bases[base_id].faction_id_current].current_num_bases = 1;
                    } else if (v == 9) {
                        Bases[base_id].population_size = 2;
                    }
                    reseed((uint32_t)(x * 7 + y * 131 + v));
                };
                sprintf_s(label, "alt_set(%d, %d, %u) old %d variant %d", x, y, alt, old, v);
                REGRESS_CTX("%s", label);
                h.run(label, setup,
                    [&] {
                        ((func_alt_set *)0x00591290)(x, y, alt);
                        for (int i = 0; i < 5; i++) {
                            memcpy(tables_o[i], tables[i], LandmarkAltSize);
                        }
                        for (int w = 0; w < MaxMapWinNum; w++) {
                            unk_o[w] = MapWinList[w]->unk_1;
                            if (cache_o[w]) {
                                memcpy(cache_o[w], MapWinList[w]->tile_cache, cache_size);
                            }
                        }
                        for (int i = 0; i < 5; i++) {
                            extra_o[i] = *extra_globals[i];
                        }
                        return _rand();
                    },
                    [&] { alt_set(x, y, alt); return _rand(); });
                BOOL diff = false;
                for (int i = 0; i < 5 && !diff; i++) {
                    diff = extra_o[i] != *extra_globals[i];
                }
                for (int i = 0; i < 5 && !diff; i++) {
                    diff = memcmp(tables_o[i], tables[i], LandmarkAltSize) != 0;
                }
                for (int w = 0; w < MaxMapWinNum && !diff; w++) {
                    diff = unk_o[w] != MapWinList[w]->unk_1
                        || (cache_o[w] && memcmp(cache_o[w], MapWinList[w]->tile_cache, cache_size));
                }
                if (diff) {
                    if (extra < MAX_DETAIL) {
                        logf("  %s: landmark tables, map view caches or selection globals differ", label);
                    }
                    extra++;
                }
                kills += h.base.veh_count_global - h.after_orig.veh_count_global;
                for (int i = 0; i < h.orig.call_count && i < MaxNoticeCalls; i++) {
                    int fn = h.orig.calls[i].fn;
                    flooded += fn == NF_BASE_KILL;
                    popups += fn == NF_POPP || fn == NF_X_POP_2;
                }
                const Map *tile_o = &h.after_orig.tiles[(x >> 1) + y * *MapLongitude];
                const Map *tile_b = &h.base.tiles[(x >> 1) + y * *MapLongitude];
                rockier += (tile_o->val3 >> 6) > (tile_b->val3 >> 6);
                flatter += (tile_o->val3 >> 6) < (tile_b->val3 >> 6);
            }
        }
        logf("alt_set_game: %d compared (by the original: %d units lost, %d bases lost, %d popups, "
            "%d tiles rockier, %d flatter), %d mismatches", h.compared, kills, flooded, popups, rockier,
            flatter, h.mismatches + extra);
        extra += h.mismatches;
    }
    for (int i = 0; i < 5; i++) {
        memcpy(tables[i], tables_saved[i], LandmarkAltSize);
    }
    for (int w = 0; w < MaxMapWinNum; w++) {
        MapWinList[w]->unk_1 = unk_saved[w];
        if (cache_saved[w]) {
            memcpy(MapWinList[w]->tile_cache, cache_saved[w], cache_size);
            free(cache_saved[w]);
            free(cache_o[w]);
        }
    }
    *TurnCurrentNum = turn_saved;
    *FactionIDCurrent = current_saved;
    *IsMultiplayerNet = net_saved;
    *ControlTurnC = turn_c_saved;
    VehBattleUnkTgl[0] = draw_saved;
    *WorldBuildUnk2 = build_saved;
    for (int i = 0; i < 5; i++) {
        *extra_globals[i] = extra_saved[i];
    }
    return extra;
}

static void __fastcall event_fx_play(void *This, void *, int fx_id) {
    notice_record(NF_FX_PLAY, NULL, NULL, NULL, fx_id, This == FXVar, 0);
}
static void __fastcall event_fx_stop(void *This, void *, int fx_id) {
    notice_record(NF_FX_STOP, NULL, NULL, NULL, fx_id, This == FXVar, 0);
}
static void __cdecl event_prefs_save(BOOL save_factions) {
    notice_record(NF_PREFS_SAVE, NULL, NULL, NULL, save_factions, 0, 0);
}
static void __cdecl event_draw_tile(int x, int y, int mode) {
    notice_record(NF_DRAW_TILE, NULL, NULL, NULL, x, y, mode);
}

// monolith(): 40 combat and 40 other units in 14 variants (the ones with a chance of using the
// monolith up in 8 random streams each)
static int test_monolith_visit() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x5C };
    OriginalEntry vanilla(0x0057A050, entry);
    NoticeStubs stubs; // NetMsg_pop, popp
    JmpStub fx_play_stub(0x00446A00, (void *)event_fx_play);
    JmpStub fx_stop_stub(0x00446A90, (void *)event_fx_stop);
    JmpStub draw_tile_stub(0x0046AF40, (void *)event_draw_tile);
    // prefs_save() is DLL code reached from both sides (its exe entry is redirected): stub the DLL
    // function itself so that neither side writes Alpha Centauri.ini
    JmpStub prefs_save_stub((uint32_t)&prefs_save, (void *)event_prefs_save);
    if (!vanilla.ok || !stubs.ok() || !fx_play_stub.ok || !fx_stop_stub.ok || !draw_tile_stub.ok
        || !prefs_save_stub.ok) {
        return 1;
    }
    typedef void func_monolith(uint32_t);
    const int current_saved = *FactionIDCurrent, turn_saved = *TurnCurrentNum;
    const BOOL net_saved = *IsMultiplayerNet;
    const uint32_t prefs_saved = *GameMorePreferences;
    int units[80];
    int unit_count = 0, combat = 0, other = 0;
    for (int i = 0; i < *VehCurrentCount && unit_count < 80; i += 7) {
        if (get_offense_rating(i) ? combat++ < 40 : other++ < 40) {
            units[unit_count++] = i;
        }
    }
    char label[96];
    int extra = 0, upgrades = 0, used_up = 0, repaired = 0, notices = 0, dialogs = 0, saves = 0;
    {
        NoticeHarness h;
        for (int u = 0; u < unit_count; u++) {
            const int veh_id = units[u];
            // 0 as loaded; with the owner at the controls (1-6, 9, 10, 12, 13), human (1-4, 9,
            // 10, 12, 13): 1 damaged, morale 2, inspect; 2 the same, declined, the monolith seen
            // by all; 3 the same, always inspect; 4 always inspect, damaged and upgraded; 5 a
            // network game, morale 5, a monolith used before seen by all; 6 the same on turn 50;
            // a computer owner not at the controls: 7 damaged, morale 3, a monolith used before;
            // 8 damaged and upgraded; 9 at the end of a go to, always inspect; 10 passing through
            // on a go to; 11 a battle ogre; 12 upgraded, inspect; 13 elite, damaged, always inspect
            for (int v = 0; v < 14; v++) {
                for (int s = 0; s < ((v == 5 || v == 7) ? 8 : 1); s++) {
                    auto setup = [&] {
                        Veh *veh = &Vehs[veh_id];
                        const uint32_t owner = veh->faction_id;
                        Map *tile = map_loc(veh->x, veh->y);
                        *FactionIDCurrent = current_saved;
                        *TurnCurrentNum = (v == 6) ? 50 : turn_saved;
                        *IsMultiplayerNet = (v == 5 || v == 6) ? true : net_saved;
                        *GameMorePreferences = (v == 0) ? prefs_saved
                            : (v == 4 || v == 9 || v == 13)
                            ? (prefs_saved | MPREF_AUTO_ALWAYS_INSPECT_MONOLITH)
                            : (prefs_saved & ~MPREF_AUTO_ALWAYS_INSPECT_MONOLITH);
                        NoticeAnswer[NF_POPP] = (v == 2) ? 0 : (v == 3) ? 2 : 1;
                        if (v == 0) {
                            reseed((uint32_t)(veh_id * 37 + v * 1000 + s));
                            return;
                        }
                        if (v != 7 && v != 8 && v != 11) {
                            *FactionIDCurrent = owner;
                        }
                        if (v == 7 || v == 8) {
                            *FactionsStatus &= ~(1 << owner);
                        } else if (v != 11) {
                            *FactionsStatus |= (1 << owner);
                        }
                        veh->order = ORDER_NONE;
                        if (v == 9 || v == 10) {
                            veh->order = ORDER_MOVE_TO;
                            veh->waypoint_x[0] = (int16_t)(veh->x + (v == 10 ? 2 : 0));
                            veh->waypoint_y[0] = veh->y;
                        } else if (v == 11) {
                            veh->proto_id = BSC_BATTLE_OGRE_MK1;
                        }
                        veh->dmg_incurred = (v == 5 || v == 6 || v == 12) ? 0 : 3;
                        if (v == 4 || v == 8 || v == 12) {
                            veh->state |= VSTATE_MONOLITH_UPGRADED;
                        } else {
                            veh->state &= ~VSTATE_MONOLITH_UPGRADED;
                        }
                        veh->morale = (uint8_t)((v == 5 || v == 6) ? 5 : (v == 7) ? 3 : (v == 13) ? 6
                            : (v <= 3) ? 2 : veh->morale);
                        if (v == 1 || v == 2 || v == 5 || v == 6 || v == 7) {
                            tile->bit |= BIT_MONOLITH | ((v >= 5) ? BIT_UNK_2000000 : 0);
                        }
                        if (v == 2 || v == 5 || v == 6) {
                            for (int i = 0; i < MaxPlayerNum - 1; i++) {
                                tile->bit_visible[i] |= BIT_MONOLITH;
                            }
                        }
                        reseed((uint32_t)(veh_id * 37 + v * 1000 + s));
                    };
                    uint32_t prefs_o = 0;
                    sprintf_s(label, "monolith(%d) variant %d stream %d", veh_id, v, s);
                    REGRESS_CTX("%s", label);
                    h.run(label, setup,
                        [&] {
                            ((func_monolith *)0x0057A050)(veh_id);
                            prefs_o = *GameMorePreferences;
                            return _rand();
                        },
                        [&] { monolith(veh_id); return _rand(); });
                    if (prefs_o != *GameMorePreferences) {
                        if (extra < MAX_DETAIL) {
                            logf("  %s: preferences %08X vs %08X", label, prefs_o, *GameMorePreferences);
                        }
                        extra++;
                    }
                    const Veh *veh_b = &h.base.vehs[veh_id], *veh_o = &h.after_orig.vehs[veh_id];
                    upgrades += (veh_o->state & VSTATE_MONOLITH_UPGRADED) && veh_o->morale > veh_b->morale
                        && v != 0;
                    repaired += !veh_o->dmg_incurred && v != 0 && v != 5 && v != 6 && v != 12 && v != 11;
                    const Map *tile_o = &h.after_orig.tiles[(veh_o->x >> 1) + veh_o->y * *MapLongitude];
                    used_up += (v == 5 || v == 7) && !(tile_o->bit & BIT_MONOLITH);
                    for (int i = 0; i < h.orig.call_count && i < MaxNoticeCalls; i++) {
                        int fn = h.orig.calls[i].fn;
                        notices += fn == NF_NETMSG_POP;
                        dialogs += fn == NF_POPP;
                        saves += fn == NF_PREFS_SAVE;
                    }
                }
            }
        }
        logf("monolith_visit: %d compared (%d combat units, %d others; by the original %d upgrades, "
            "%d repairs, %d monoliths used up, %d notices, %d dialogs, %d preference saves), "
            "%d mismatches", h.compared, combat < 40 ? combat : 40, other < 40 ? other : 40, upgrades,
            repaired, used_up, notices, dialogs, saves, h.mismatches + extra);
        extra += h.mismatches;
    }
    *FactionIDCurrent = current_saved;
    *TurnCurrentNum = turn_saved;
    *IsMultiplayerNet = net_saved;
    *GameMorePreferences = prefs_saved;
    return extra;
}

// ---------------------------------------------------------------- batch 31: AI diplomacy

// The exe's C runtime keeps rand()'s seed in its per-thread data (_getptd() + 0x14). enemies_war()
// and enemy_diplomacy() draw from it, so these tests seed it before each side runs and compare
// where it ended up.
typedef uint8_t *func_getptd();
static uint32_t *crt_rand_seed() {
    return (uint32_t *)(((func_getptd *)0x006491C3)() + 0x14);
}

static void __cdecl notice_communicate(int faction_id, int faction_id_with, int unk) {
    notice_record(NF_COMMUNICATE, NULL, NULL, NULL, faction_id, faction_id_with, unk);
}
static void __cdecl notice_encounter(int faction_id, int faction_id_with, int unk1, int unk2,
                                     int unk3, int unk4) {
    notice_record(NF_ENCOUNTER, NULL, NULL, NULL, faction_id, faction_id_with,
        (unk1 & 0xFF) | (unk2 & 0xFF) << 8 | (unk3 & 0xFF) << 16 | (unk4 & 0xFF) << 24);
}
// A network message: type and flags, size and sender, and a hash of the sequence number and the
// payload (the header's time stamp differs between the two runs).
static void __fastcall notice_net_send(void *This, void *, void *msg, uint32_t size, int flags) {
    const NetMsgHeader *header = (const NetMsgHeader *)msg;
    uint32_t hash = (uint32_t)header->sequence;
    for (uint32_t i = sizeof(NetMsgHeader); i < size; i++) {
        hash = hash * 31 + ((const uint8_t *)msg)[i];
    }
    notice_record(NF_NET_SEND, NULL, NULL, NULL, (header->type & 0xFFFF) | flags << 16,
        (int)size | header->faction_id << 16 | (This == NetDaemonVar) << 24, (int)hash);
}
static void __fastcall notice_await_diplo(void *This, void *, int msg_code) {
    notice_record(NF_AWAIT_DIPLO, NULL, NULL, NULL, This == NetDaemonVar, msg_code, 0);
}

// The dialogs, network traffic and further game events the AI diplomacy functions reach besides
// NoticeStubs. double_cross() is tested here, so its stub (in DiploTwoStubs) is left out.
struct AiDiploStubs {
    JmpStub communicate_stub, encounter_stub, send_stub, await_stub, x_pop_stub, speak_stub,
        tech_stub, caption_stub;
    AiDiploStubs()
        : communicate_stub(0x0054FFD0, (void *)notice_communicate),
          encounter_stub(0x0055E370, (void *)notice_encounter),
          send_stub(0x00532940, (void *)notice_net_send),
          await_stub(0x005315C0, (void *)notice_await_diplo),
          x_pop_stub(0x005BF310, (void *)notice_x_pop),
          speak_stub(0x0053DCE0, (void *)notice_wants_to_speak),
          tech_stub(0x005BB000, (void *)notice_tech_achieved),
          caption_stub(0x005399A0, (void *)notice_caption) {}
    BOOL ok() const {
        return communicate_stub.ok && encounter_stub.ok && send_stub.ok && await_stub.ok
            && x_pop_stub.ok && speak_stub.ok && tech_stub.ok && caption_stub.ok;
    }
};

// The globals the AI diplomacy tests vary; put back before each variant and at the end.
struct AiDiploEnv {
    int current, turn, diff, sunspot, empath;
    uint8_t status[2];
    BOOL net, pbem;
    uint32_t rules, seed;
    AiDiploEnv() : current(*FactionIDCurrent), turn(*TurnCurrentNum), diff(*DiffLevelCurrent),
        sunspot(*SunspotDuration), empath(SecretProject->empath_guild), net(*IsMultiplayerNet),
        pbem(*IsMultiplayerPBEM), rules(*GameRules), seed(*crt_rand_seed()) {
        status[0] = FactionsStatus[0];
        status[1] = FactionsStatus[1];
    }
    ~AiDiploEnv() {
        restore();
        *crt_rand_seed() = seed;
    }
    void restore() {
        *FactionIDCurrent = current;
        *TurnCurrentNum = turn;
        *DiffLevelCurrent = diff;
        *SunspotDuration = sunspot;
        SecretProject->empath_guild = empath;
        FactionsStatus[0] = status[0];
        FactionsStatus[1] = status[1];
        *IsMultiplayerNet = net;
        *IsMultiplayerPBEM = pbem;
        *GameRules = rules;
    }
};

// NoticeHarness::run() with the exe's rand() seeded the same way for both sides; a different seed
// afterwards counts as a mismatch too.
template <class S, class E, class D>
static void aidiplo_run(NoticeHarness &h, const char *label, uint32_t seed, S setup, E exe, D dll) {
    uint32_t seed_orig = 0, seed_new = 0;
    int mismatches = h.mismatches;
    h.run(label, [&] { setup(); _srand(seed); },
        [&] { int ret = exe(); seed_orig = *crt_rand_seed(); return ret; },
        [&] { int ret = dll(); seed_new = *crt_rand_seed(); return ret; });
    if (seed_orig != seed_new && h.mismatches == mismatches) {
        if (h.mismatches < MAX_DETAIL) {
            logf("  %s: rand() seed orig %08X new %08X", label, seed_orig, seed_new);
        }
        h.mismatches++;
    }
}

typedef int func_contiguous_exe(int, int, uint32_t, int);

static int test_contiguous() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C };
    OriginalEntry vanilla(0x0053A780, entry);
    if (!vanilla.ok) {
        return 1;
    }
    func_contiguous_exe *exe = (func_contiguous_exe *)0x0053A780;
    NoticeHarness h;
    char label[96];
    int found = 0;
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            // anywhere, the first two regions where both have bases and one where only f1 has
            uint32_t regions[4] = { 0, 0, 0, 0 };
            int count = 1, shared = 0;
            BOOL lone = false;
            for (uint32_t r = 1; r < 128; r++) {
                BOOL has1 = PlayersData[f1].region_total_bases[r] != 0;
                BOOL has2 = PlayersData[f2].region_total_bases[r] != 0;
                if (has1 && has2 && shared < 2) {
                    regions[count++] = r;
                    shared++;
                } else if (has1 && !has2 && !lone) {
                    regions[count++] = r;
                    lone = true;
                }
            }
            for (int k = 0; k < count; k++) {
                for (int unk = 0; unk < 2; unk++) {
                    uint32_t region = regions[k];
                    sprintf_s(label, "contiguous(%d, %d, %u, %d)", f1, f2, region, unk);
                    REGRESS_CTX("%s", label);
                    h.run(label, [] {}, [&] { return exe(f1, f2, region, unk); },
                        [&] { return (int)contiguous(f1, f2, region, unk); });
                    found += h.orig.ret != 0;
                }
            }
        }
    }
    logf("contiguous: %d compared (%d found a path), %d mismatches", h.compared, found,
        h.mismatches);
    return h.mismatches;
}

static int test_comm_check() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x51, 0xA1, 0xE8 };
    OriginalEntry vanilla(0x0055C430, entry);
    NoticeStubs stubs;
    AiDiploStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok()) {
        return 1;
    }
    func_diplo2 *exe = (func_diplo2 *)0x0055C430;
    AiDiploEnv env;
    NoticeHarness h;
    char label[96];
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            // f1 human; mode (single player, network, hotseat/PBEM) x f2 human or not x at the
            // controls f1, f2 or as loaded x contact (none; commlink, spoke this turn; the same
            // wanting to talk; spoke 20 turns ago; never spoke) x flag 0x8000000. Variant 180:
            // f1 not human.
            for (int v = 0; v <= 180; v++) {
                auto setup = [&] {
                    env.restore();
                    if (v == 180) {
                        FactionsStatus[0] &= ~(1 << f1);
                        return;
                    }
                    int mode = v % 3, w = v / 3;
                    *IsMultiplayerNet = mode == 1;
                    *IsMultiplayerPBEM = mode == 2;
                    FactionsStatus[0] |= (uint8_t)(1 << f1);
                    if (w & 1) {
                        FactionsStatus[0] |= (uint8_t)(1 << f2);
                    } else {
                        FactionsStatus[0] &= ~(1 << f2);
                    }
                    int cur = (w / 2) % 3;
                    if (cur < 2) {
                        *FactionIDCurrent = cur ? f2 : f1;
                    }
                    int contact = (w / 6) % 5;
                    uint32_t &t = PlayersData[f1].diplo_treaties[f2];
                    t &= ~(DTREATY_COMMLINK | DTREATY_WANT_TO_TALK | DTREATY_UNK_8000000);
                    if (contact) {
                        t |= DTREATY_COMMLINK;
                        PlayersData[f1].diplo_spoke[f2] = (contact == 3) ? *TurnCurrentNum - 20
                            : ((contact == 4) ? -1 : *TurnCurrentNum);
                        if (contact == 2) {
                            t |= DTREATY_WANT_TO_TALK;
                        }
                    }
                    if (w / 30) {
                        t |= DTREATY_UNK_8000000;
                    }
                };
                sprintf_s(label, "comm_check(%d, %d) variant %d", f1, f2, v);
                REGRESS_CTX("%s", label);
                h.run(label, setup, [&] { exe(f1, f2); return 0; },
                    [&] { comm_check(f1, f2); return 0; });
            }
        }
    }
    logf("comm_check: %d compared (%d changed state, %d with calls), %d mismatches", h.compared,
        h.changed, h.with_calls, h.mismatches);
    return h.mismatches;
}

static int test_enemies_war() {
    OriginalEntry vanilla(0x0055C840, NoticeEntry55_53_56_57);
    NoticeStubs stubs;
    AiDiploStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok()) {
        return 1;
    }
    func_diplo2 *exe = (func_diplo2 *)0x0055C840;
    AiDiploEnv env;
    NoticeHarness h;
    char label[96];
    const int cur = env.current;
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            // 0 as loaded; 1 already at war; 2/3 the player in a pact with f1/f2; 4 the player
            // with an infiltrator at f1; 5 omniscient view; 6-9 revenge wanted on both sides at
            // difficulty 1-4 (9: f2 the atrocity victim); 10 flag 0x40000000; 11 revenge at
            // difficulty 5. Variants above 0 start at peace.
            for (int v = 0; v < 12; v++) {
                auto setup = [&] {
                    env.restore();
                    if (!v) {
                        return;
                    }
                    uint32_t &t = PlayersData[f1].diplo_treaties[f2];
                    uint32_t &t_with = PlayersData[f2].diplo_treaties[f1];
                    t &= ~DTREATY_VENDETTA;
                    t_with &= ~DTREATY_VENDETTA;
                    if (v == 1) {
                        t |= DTREATY_VENDETTA;
                    } else if (v == 2 && cur != f1) {
                        PlayersData[cur].diplo_treaties[f1] |= DTREATY_PACT;
                    } else if (v == 3 && cur != f2) {
                        PlayersData[cur].diplo_treaties[f2] |= DTREATY_PACT;
                    } else if (v == 4) {
                        PlayersData[cur].diplo_treaties[f1] |= DTREATY_HAVE_INFILTRATOR;
                    } else if (v == 5) {
                        *GameState |= STATE_OMNISCIENT_VIEW;
                    } else if (v == 10) {
                        t |= DTREATY_UNK_40000000;
                    } else if (v >= 6) {
                        t = (t | DTREATY_WANT_REVENGE) & ~DTREATY_ATROCITY_VICTIM;
                        t_with = (t_with | DTREATY_WANT_REVENGE) & ~DTREATY_ATROCITY_VICTIM;
                        PlayersData[f1].diplo_agenda[f2] |= DAGENDA_UNK_400;
                        PlayersData[f2].diplo_agenda[f1] |= DAGENDA_UNK_800;
                        *DiffLevelCurrent = (v == 11) ? 5 : v - 5;
                        if (v == 9) {
                            t_with |= DTREATY_ATROCITY_VICTIM;
                        }
                    }
                };
                sprintf_s(label, "enemies_war(%d, %d) variant %d", f1, f2, v);
                REGRESS_CTX("%s", label);
                aidiplo_run(h, label, f1 * 131 + f2 * 17 + v, setup,
                    [&] { exe(f1, f2); return 0; }, [&] { enemies_war(f1, f2); return 0; });
            }
        }
    }
    logf("enemies_war: %d compared (%d changed state, %d with a notice), %d mismatches", h.compared,
        h.changed, h.with_calls, h.mismatches);
    return h.mismatches;
}

static int test_pact_of_brotherhood() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0xA1, 0x60, 0xF6 };
    OriginalEntry vanilla(0x0053CAF0, entry);
    NoticeStubs stubs;
    AiDiploStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok()) {
        return 1;
    }
    func_diplo2 *exe = (func_diplo2 *)0x0053CAF0;
    AiDiploEnv env;
    NoticeHarness h;
    char label[96];
    for (int f1 = 1; f1 < MaxPlayerNum; f1++) {
        for (int f2 = 1; f2 < MaxPlayerNum; f2++) {
            if (f1 == f2) {
                continue;
            }
            // the tutorial check answering 0 and 1; difficulty Specialist (no check); a network
            // game with the check answering 0 and 1
            for (int v = 0; v < 5; v++) {
                auto setup = [&] {
                    env.restore();
                    NoticeAnswer[NF_TUT_CHECK2] = (v == 1 || v == 4);
                    if (v == 2) {
                        *DiffLevelCurrent = DLVL_SPECIALIST;
                    }
                    *IsMultiplayerNet = v >= 3;
                };
                sprintf_s(label, "pact_of_brotherhood(%d, %d) variant %d", f1, f2, v);
                REGRESS_CTX("%s", label);
                h.run(label, setup, [&] { exe(f1, f2); return 0; },
                    [&] { pact_of_brotherhood(f1, f2); return 0; });
            }
        }
    }
    logf("pact_of_brotherhood: %d compared (%d changed state), %d mismatches", h.compared,
        h.changed, h.mismatches);
    return h.mismatches;
}

static int test_double_cross() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C };
    OriginalEntry vanilla(0x0050BCC0, entry);
    NoticeStubs stubs;
    AiDiploStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok()) {
        return 1;
    }
    typedef void func_double_cross(int, int, int);
    func_double_cross *exe = (func_double_cross *)0x0050BCC0;
    AiDiploEnv env;
    NoticeHarness h;
    char label[96];
    const uint32_t peace = DTREATY_PACT | DTREATY_TREATY | DTREATY_TRUCE | DTREATY_UNK_10000;
    for (int atk = 1; atk < MaxPlayerNum; atk++) {
        for (int def = 1; def < MaxPlayerNum; def++) {
            if (atk == def) {
                continue;
            }
            int third = (atk + def) % 7 + 1;
            while (third == atk || third == def) {
                third = third % 7 + 1;
            }
            for (int o = 0; o < 2; o++) {
                int other = o ? third : -1;
                // s: 0 as loaded, 1 pact, 2 treaty, 3 truce, 4 truce with flag 0x10000, 5 none;
                // e: 0 both computer players, 1 agenda flag 0x20, 2 attacker human, 3 defender
                // human, 4 the attacker the atrocity victim, 5 a network game with a human
                // attacker and the defender at the controls, 6 the attacker in a pact with the
                // other faction
                for (int s = 0; s < 6; s++) {
                    for (int e = 0; e < 7; e++) {
                        auto setup = [&] {
                            env.restore();
                            uint32_t &t = PlayersData[atk].diplo_treaties[def];
                            uint32_t &t_def = PlayersData[def].diplo_treaties[atk];
                            if (s) {
                                t &= ~peace;
                                t_def &= ~peace;
                            }
                            uint32_t set = (s == 1) ? DTREATY_PACT | DTREATY_TREATY
                                : (s == 2) ? DTREATY_TREATY : (s == 3) ? DTREATY_TRUCE
                                : (s == 4) ? DTREATY_TRUCE | DTREATY_UNK_10000 : 0;
                            t |= set;
                            t_def |= set;
                            if (e == 0) {
                                FactionsStatus[0] &= ~((1 << atk) | (1 << def));
                            } else if (e == 1) {
                                PlayersData[atk].diplo_agenda[def] |= DAGENDA_UNK_20;
                            } else if (e == 2 || e == 5) {
                                FactionsStatus[0] |= (uint8_t)(1 << atk);
                            } else if (e == 3) {
                                FactionsStatus[0] |= (uint8_t)(1 << def);
                            } else if (e == 4) {
                                t |= DTREATY_ATROCITY_VICTIM;
                            } else if (e == 6 && other >= 0) {
                                PlayersData[atk].diplo_treaties[other] |= DTREATY_PACT
                                    | DTREATY_TREATY;
                            }
                            if (e == 5) {
                                *IsMultiplayerNet = true;
                                *FactionIDCurrent = def;
                            }
                        };
                        sprintf_s(label, "double_cross(%d, %d, %d) state %d variant %d", atk, def,
                            other, s, e);
                        REGRESS_CTX("%s", label);
                        h.run(label, setup, [&] { exe(atk, def, other); return 0; },
                            [&] { double_cross(atk, def, other); return 0; });
                    }
                }
            }
        }
    }
    logf("double_cross: %d compared (%d with calls), %d mismatches", h.compared, h.with_calls,
        h.mismatches);
    return h.mismatches;
}

// The first base of a faction, or -1.
static int aidiplo_base_of(int faction_id) {
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        if (Bases[base_id].faction_id_current == faction_id) {
            return base_id;
        }
    }
    return -1;
}

static int test_enemy_diplomacy() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x34 };
    OriginalEntry vanilla(0x0055F930, entry);
    NoticeStubs stubs;
    AiDiploStubs stubs2;
    if (!vanilla.ok || !stubs.ok() || !stubs2.ok()) {
        return 1;
    }
    typedef void func_enemy_diplomacy(uint32_t);
    func_enemy_diplomacy *exe = (func_enemy_diplomacy *)0x0055F930;
    AiDiploEnv env;
    NoticeHarness h;
    char label[96];
    int pods_changed = 0, betrayed = 0, requested = 0;
    int calls_by_fn[NF_COUNT] = {};
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        int human = (f % 7) + 1; // made the human player at the controls in some variants
        int ally = (human % 7) + 1; // a third faction
        if (ally == (int)f) {
            ally = (ally % 7) + 1;
        }
        // 0-15: as loaded at 16 consecutive turns; 16 sunspots; 17 hotseat/PBEM; 18 network;
        // 19 betrayals planned against everyone; 20 the same under pacts; 21 revenge agendas
        // aging; 22 flag 0x80000000 with every computer faction while the player is at war with
        // them; 23 every computer faction wants to talk (pacts with some); 24 a human in contact
        // who wants to talk; 25 the same after 40 silent turns with flag 0x20000000 on turn
        // f + 8; 26 a stronger common enemy of the AI and the human; 27-28 talks overdue with a
        // revenge wish (28 under sunspots); 29 orbital defense pods against rivals' satellites;
        // 30 the same against a Geosynchronous Survey Pod; 31 cooperative victory and intense
        // rivalry after turn 200; 32 the Empath Guild with f; 33 f human
        for (int v = 0; v < 34; v++) {
            auto setup = [&] {
                env.restore();
                *TurnCurrentNum = env.turn + v % 16;
                PlayerData *p = &PlayersData[f];
                if (v >= 24 && v <= 28) {
                    FactionsStatus[0] |= (uint8_t)(1 << human);
                    FactionsStatus[1] |= (uint8_t)(1 << human);
                    *FactionIDCurrent = human;
                    p->diplo_treaties[human] = (p->diplo_treaties[human] & ~(DTREATY_VENDETTA
                        | DTREATY_ATROCITY_VICTIM | DTREATY_PACT)) | DTREATY_COMMLINK;
                    PlayersData[human].diplo_spoke[f] = *TurnCurrentNum - 40;
                }
                switch (v) {
                case 16:
                    *SunspotDuration = 5;
                    break;
                case 17:
                    *IsMultiplayerPBEM = true;
                    break;
                case 18:
                    *IsMultiplayerNet = true;
                    break;
                case 19:
                case 20:
                    for (int i = 1; i < MaxPlayerNum; i++) {
                        p->diplo_treaties[i] |= DTREATY_SHALL_BETRAY;
                        if (v == 20) {
                            p->diplo_treaties[i] |= DTREATY_PACT | DTREATY_TREATY;
                        }
                        PlayersData[i].integrity_blemishes = i % 4;
                        p->diplo_agenda[i] |= (i & 1) ? DAGENDA_UNK_400 : 0;
                    }
                    break;
                case 21:
                    for (int i = 1; i < MaxPlayerNum; i++) {
                        p->diplo_agenda[i] |= DAGENDA_UNK_20 | ((i & 1) ? DAGENDA_UNK_40 : 0);
                    }
                    break;
                case 22:
                case 23:
                    for (int i = 1; i < MaxPlayerNum; i++) {
                        if (FactionsStatus[0] & (1 << i)) {
                            continue;
                        }
                        if (v == 22) {
                            p->diplo_treaties[i] = (p->diplo_treaties[i] | DTREATY_UNK_80000000
                                | DTREATY_UNK_4000000) & ~DTREATY_WANT_TO_TALK;
                            PlayersData[i].diplo_treaties[env.current] |= DTREATY_VENDETTA;
                        } else {
                            p->diplo_treaties[i] |= DTREATY_WANT_TO_TALK
                                | ((i % 3) ? 0 : DTREATY_PACT | DTREATY_TREATY);
                        }
                    }
                    break;
                case 24:
                    p->diplo_treaties[human] |= DTREATY_WANT_TO_TALK;
                    break;
                case 25:
                    p->diplo_treaties[human] |= DTREATY_UNK_20000000;
                    *TurnCurrentNum = f + 8;
                    break;
                case 26:
                    p->diplo_treaties[human] |= DTREATY_WANT_TO_TALK;
                    p->diplo_treaties[ally] |= DTREATY_COMMLINK | DTREATY_VENDETTA
                        | DTREATY_UNK_8000;
                    PlayersData[human].diplo_treaties[ally] = (PlayersData[human].diplo_treaties[
                        ally] | DTREATY_COMMLINK) & ~DTREATY_VENDETTA;
                    PlayersData[ally].mil_strength_1 = p->mil_strength_1 + 100;
                    FactionsStatus[1] |= (uint8_t)(1 << ally);
                    break;
                case 27:
                case 28:
                    p->diplo_treaties[human] |= DTREATY_WANT_REVENGE;
                    if (v == 28) {
                        *SunspotDuration = 5;
                    }
                    break;
                case 29:
                case 30:
                    p->satellites_odp_total = 12;
                    p->sat_odp_deployed = 0;
                    for (int i = 1; i < MaxPlayerNum; i++) {
                        if (i == (int)f) {
                            continue;
                        }
                        p->diplo_treaties[i] = (p->diplo_treaties[i] & ~DTREATY_PACT)
                            | ((i & 1) ? DTREATY_VENDETTA : DTREATY_UNK_800);
                        PlayersData[i].planet_busters = 0;
                        PlayersData[i].satellites_nutrient = (v == 29 && i % 3 == 0) ? 2 : 0;
                        PlayersData[i].satellites_mineral = (v == 29 && i % 3 == 1) ? 1 : 0;
                        PlayersData[i].satellites_energy = (v == 29 && i % 3 == 2) ? 1 : 0;
                        PlayersData[i].satellites_odp_total = (v == 29) ? i % 2 : 0;
                        if (v == 30) {
                            int base_id = aidiplo_base_of(i);
                            if (base_id >= 0) {
                                set_fac(FAC_GEOSYNC_SURVEY_POD, base_id, true);
                            }
                        }
                    }
                    break;
                case 31:
                    *GameRules |= RULES_VICTORY_COOPERATIVE | RULES_INTENSE_RIVALRY;
                    *TurnCurrentNum = ((env.turn > 200) ? env.turn : 200) + 4;
                    for (int i = 1; i < MaxPlayerNum; i++) {
                        if (FactionsStatus[0] & (1 << i)) {
                            p->diplo_treaties[i] |= DTREATY_PACT | DTREATY_TREATY
                                | DTREATY_COMMLINK;
                        }
                    }
                    break;
                case 32:
                    SecretProject->empath_guild = aidiplo_base_of(f);
                    for (int i = 1; i < MaxPlayerNum; i++) {
                        p->diplo_treaties[i] |= DTREATY_WANT_TO_TALK;
                    }
                    break;
                case 33:
                    FactionsStatus[0] |= (uint8_t)(1 << f);
                    break;
                }
            };
            sprintf_s(label, "enemy_diplomacy(%u) variant %d", f, v);
            REGRESS_CTX("%s", label);
            aidiplo_run(h, label, f * 977 + v, setup, [&] {
                uint32_t pods = PlayersData[f].sat_odp_deployed
                    + PlayersData[f].satellites_odp_total;
                uint32_t betrayals = 0;
                for (int i = 1; i < MaxPlayerNum; i++) {
                    betrayals |= PlayersData[f].diplo_treaties[i] & DTREATY_SHALL_BETRAY;
                }
                exe(f);
                pods_changed += pods != PlayersData[f].sat_odp_deployed
                    + PlayersData[f].satellites_odp_total;
                for (int i = 1; i < MaxPlayerNum; i++) {
                    if (betrayals && !(PlayersData[f].diplo_treaties[i] & DTREATY_SHALL_BETRAY)) {
                        betrayed++;
                        break;
                    }
                }
                for (int i = 0; i < MaxPlayerNum; i++) {
                    requested += DiploCommRequest[i] != 0;
                }
                return 0;
            }, [&] { enemy_diplomacy(f); return 0; });
            for (int i = 0; i < h.orig.call_count && i < MaxNoticeCalls; i++) {
                int fn = h.orig.calls[i].fn;
                calls_by_fn[fn]++;
            }
        }
    }
    logf("enemy_diplomacy: %d compared (%d changed state, %d with calls, %d changed the orbital "
        "defense pod count, %d carried out a betrayal, %d requests to talk left), %d mismatches",
        h.compared, h.changed, h.with_calls, pods_changed, betrayed, requested, h.mismatches);
    logf("enemy_diplomacy: calls by the original: %d communicate, %d encounter, %d tech_achieved, "
        "%d popp, %d X_pop, %d NetMsg_pop", calls_by_fn[NF_COMMUNICATE], calls_by_fn[NF_ENCOUNTER],
        calls_by_fn[NF_TECH_ACHIEVED], calls_by_fn[NF_POPP], calls_by_fn[NF_X_POP],
        calls_by_fn[NF_NETMSG_POP]);
    return h.mismatches;
}

// ---------------------------------------------------------------- batch 31: AI strategy

static const uint8_t EnemyStrategyEntry[6] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x88 };
static const uint8_t EnemyVehEntry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x58 };
static const uint8_t EnemyTurnEntry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08 };

// wants_to_attack() used to start from 7 stack slots it never writes first, so these tests pinned
// them. Since 2026-10-18 it starts them at zero (Kiousu's choice) and its answer no longer depends
// on the stack, so there is nothing to pin: the pin values are still made per variant but unused,
// and the call count stays 0 on both sides.
static int AistratWtaPin[8];
static int AistratWtaCalls;

struct AistratWtaStub {
    BOOL ok;
    AistratWtaStub() : ok(true) {}
};

static uint32_t AistratSeedState = 0x9E3779B9;
static uint32_t aistrat_rand() {
    AistratSeedState ^= AistratSeedState << 13;
    AistratSeedState ^= AistratSeedState >> 17;
    AistratSeedState ^= AistratSeedState << 5;
    return AistratSeedState;
}

// Leftovers for wants_to_attack: 0 none (the real maxima count); 1 the junk a trace of the game
// found (a stack address); 2 random values near each faction's real maximum; 3 values whose * 3
// wraps; 4 any 32-bit values.
static void aistrat_make_pin(const GameSnapshot *s, int pattern, int *pin) {
    static const int wrapping[] = { 0x7FFFFFFF, (int)0x80000000, 0x2AAAAAAB, -1, 0x55555556,
        (int)0xAAAAAAAB, 1 };
    pin[0] = 0;
    for (int f = 1; f < MaxPlayerNum; f++) {
        int top = 0;
        for (int region = 1; region < MaxRegionLandNum; region++) {
            if (s->players[f].region_total_bases[region] > top) {
                top = s->players[f].region_total_bases[region];
            }
        }
        switch (pattern) {
          case 0: pin[f] = 0; break;
          case 1: pin[f] = 1746744; break;
          case 2: pin[f] = (int)(aistrat_rand() % (uint32_t)(2 * top + 4)) - 1; break;
          case 3: pin[f] = wrapping[aistrat_rand() % 7]; break;
          default: pin[f] = (int)aistrat_rand(); break;
        }
    }
}

// What these functions change besides GameSnapshot and ProdState.
static int *const AistratSelection[5] = { (int *)0x0093928C, (int *)0x00939290, (int *)0x008C6B2C,
    (int *)0x008C6B34, (int *)0x009A64DC }; // unit ids veh_kill() renumbers
struct AistratExtra {
    VehPrototype protos[MaxVehProtoNum]; // Console::upgrade()
    int find_dist; // base_find() and friends
    int attack_flags; // enemy_veh() in a network game
    int selection[5];
    int wta_calls;
    // a supply pod (goody_box) can hand out a tech (tech_achieved) and record monuments: without
    // these the next run from the same snapshot finds the tech already owned and picks again
    uint8_t techs[MaxTechnologyNum];
    uint8_t monuments[MonumentTableSize];
};

static void aistrat_take(AistratExtra *s) {
    memcpy(s->techs, GameTechAchieved, sizeof(s->techs));
    memcpy(s->monuments, MonumentTable, sizeof(s->monuments));
    memcpy(s->protos, VehPrototypes, sizeof(s->protos));
    s->find_dist = *BaseFindDist;
    s->attack_flags = *VehAttackFlags;
    for (int i = 0; i < 5; i++) {
        s->selection[i] = *AistratSelection[i];
    }
    s->wta_calls = AistratWtaCalls;
}

static void aistrat_restore(const AistratExtra *s) {
    memcpy(GameTechAchieved, s->techs, sizeof(s->techs));
    memcpy(MonumentTable, s->monuments, sizeof(s->monuments));
    memcpy(VehPrototypes, s->protos, sizeof(s->protos));
    *BaseFindDist = s->find_dist;
    *VehAttackFlags = s->attack_flags;
    for (int i = 0; i < 5; i++) {
        *AistratSelection[i] = s->selection[i];
    }
}

static BOOL aistrat_differs(const AistratExtra *o, const AistratExtra *n, const char *label) {
    if (memcmp(o->techs, n->techs, sizeof(o->techs))
        || memcmp(o->monuments, n->monuments, sizeof(o->monuments))) {
        if (label) {
            logf("  %s: GameTechAchieved or the monument table differs", label);
        }
        return true;
    }
    size_t d = first_diff(o->protos, n->protos, sizeof(o->protos));
    if (d != (size_t)-1) {
        if (label) {
            logf("  %s: VehPrototypes[%u] +0x%X differs", label, (unsigned)(d / sizeof(VehPrototype)),
                (unsigned)(d % sizeof(VehPrototype)));
        }
        return true;
    }
    if (o->find_dist != n->find_dist || o->attack_flags != n->attack_flags
        || memcmp(o->selection, n->selection, sizeof(o->selection)) || o->wta_calls != n->wta_calls) {
        if (label) {
            logf("  %s: BaseFindDist %d/%d, VehAttackFlags %d/%d, selection %d,%d,%d,%d,%d / %d,%d,%d,%d,%d, "
                "wants_to_attack calls %d/%d", label, o->find_dist, n->find_dist, o->attack_flags,
                n->attack_flags, o->selection[0], o->selection[1], o->selection[2], o->selection[3],
                o->selection[4], n->selection[0], n->selection[1], n->selection[2], n->selection[3],
                n->selection[4], o->wta_calls, n->wta_calls);
        }
        return true;
    }
    return false;
}

// The callees of enemy_veh() and enemy_turn() a test replaces: each call is recorded (the first
// MaxAistratCalls in full, all of them in a running hash) and answered as the variant says.
enum AistratFn { AF_ENEMY_MOVE, AF_ACTION, AF_NON_INPUT, AF_SYNCH, AF_AWAIT, AF_UNLOCK, AF_NET_ACTION,
    AF_ENEMY_VEH, AF_UI };
struct AistratCall {
    int fn;
    int args[3];
};
const int MaxAistratCalls = 48;
struct AistratLog {
    AistratCall calls[MaxAistratCalls];
    int count;
    uint32_t hash;
    int ret;
};
static AistratLog AistratCalls;
static int AistratMoveRet;
static int AistratMoveSpend; // enemy_move: 0 leaves the moves, 1 spends them all, 2 spends one
static int AistratActionRet;
static int AistratNetActionRet;
static int AistratVehPattern; // what the enemy_veh stand-in does, see aistrat_enemy_veh()

static void aistrat_calls_reset() {
    memset(&AistratCalls, 0, sizeof(AistratCalls));
    AistratCalls.hash = 2166136261u;
}

static void aistrat_record(int fn, int a0, int a1, int a2) {
    if (AistratCalls.count < MaxAistratCalls) {
        AistratCall *call = &AistratCalls.calls[AistratCalls.count];
        call->fn = fn;
        call->args[0] = a0;
        call->args[1] = a1;
        call->args[2] = a2;
    }
    AistratCalls.count++;
    int values[4] = { fn, a0, a1, a2 };
    for (int i = 0; i < 4; i++) {
        AistratCalls.hash = (AistratCalls.hash ^ (uint32_t)values[i]) * 16777619u;
    }
}

static BOOL aistrat_log_differs(const AistratLog *o, const AistratLog *n, const char *label) {
    if (o->count != n->count || o->hash != n->hash || o->ret != n->ret) {
        if (label) {
            logf("  %s: orig %d calls (hash %08X) returned %d, new %d calls (hash %08X) returned %d", label,
                o->count, o->hash, o->ret, n->count, n->hash, n->ret);
            int shown = o->count < MaxAistratCalls ? o->count : MaxAistratCalls;
            for (int i = 0; i < shown; i++) {
                const AistratCall *a = &o->calls[i], *b = &n->calls[i];
                if (i >= n->count || a->fn != b->fn || memcmp(a->args, b->args, sizeof(a->args))) {
                    logf("    first difference at call %d: orig %d(%d, %d, %d) new %d(%d, %d, %d)", i,
                        a->fn, a->args[0], a->args[1], a->args[2], b->fn, b->args[0], b->args[1],
                        b->args[2]);
                    break;
                }
            }
        }
        return true;
    }
    return false;
}

static int __cdecl aistrat_enemy_move(int veh_id) {
    aistrat_record(AF_ENEMY_MOVE, veh_id, Vehs[veh_id].moves_expended, Vehs[veh_id].order);
    if (AistratMoveSpend == 1) {
        Vehs[veh_id].moves_expended = (uint8_t)speed(veh_id, false);
    } else if (AistratMoveSpend == 2) {
        Vehs[veh_id].moves_expended++;
    }
    return AistratMoveRet;
}
static int __cdecl aistrat_action(int veh_id) {
    aistrat_record(AF_ACTION, veh_id, Vehs[veh_id].moves_expended, Vehs[veh_id].order);
    return AistratActionRet;
}
static void __cdecl aistrat_non_input() {
    aistrat_record(AF_NON_INPUT, 0, 0, 0);
}
static void __cdecl aistrat_synch_veh(int veh_id) {
    aistrat_record(AF_SYNCH, veh_id, 0, 0);
}
static int __fastcall aistrat_await_synch(void *This, void *UNUSED(edx_unused)) {
    aistrat_record(AF_AWAIT, (int)This, 0, 0);
    return 0;
}
static int __fastcall aistrat_unlock_veh(void *This, void *UNUSED(edx_unused)) {
    aistrat_record(AF_UNLOCK, (int)This, 0, 0);
    return 0;
}
static int __fastcall aistrat_net_action(void *This, void *UNUSED(edx_unused), int veh_id, int unk) {
    aistrat_record(AF_NET_ACTION, (int)This, veh_id, unk);
    return AistratNetActionRet;
}

// Stand-in for enemy_veh() while enemy_turn() runs: 0 one step and done; 1 spends the moves and
// asks for more; 2 never done and leaves the moves (the 32 step limit); 3 a mix: done on every
// third call, moves spent on every fifth, boarding (sentry) on every seventh; 4 as 3, and every
// eleventh call the unit is killed (the unit count changes).
static int __cdecl aistrat_enemy_veh(int veh_id) {
    int n = AistratCalls.count;
    aistrat_record(AF_ENEMY_VEH, veh_id, Vehs[veh_id].moves_expended, Vehs[veh_id].order);
    switch (AistratVehPattern) {
      case 0:
        return 1;
      case 1:
        Vehs[veh_id].moves_expended = (uint8_t)speed(veh_id, false);
        return 0;
      case 2:
        return 0;
      default:
        if (AistratVehPattern == 4 && n % 11 == 10) {
            veh_kill(veh_id);
            return 0;
        }
        if (n % 5 == 4) {
            Vehs[veh_id].moves_expended = (uint8_t)speed(veh_id, false);
        }
        if (n % 7 == 6) {
            Vehs[veh_id].order = ORDER_SENTRY_BOARD;
        }
        return (veh_id + n) % 3 == 0;
    }
}

// Modal UI a live enemy_move() may reach: recorded and answered 0. (NetMsg::pop only shows a
// message and is left alone.)
template <int N> static int __cdecl aistrat_ui() {
    aistrat_record(AF_UI, N, 0, 0);
    return 0;
}
struct AistratUiStubs {
    JmpStub x_pop, x_pops, x_pop_2, pop2, encounter;
    AistratUiStubs()
        : x_pop(0x005BF310, (void *)aistrat_ui<1>),
          x_pops(0x005BF7D0, (void *)aistrat_ui<2>),
          x_pop_2(0x005BF330, (void *)aistrat_ui<3>),
          pop2(0x00405140, (void *)aistrat_ui<4>),
          encounter(0x0055E370, (void *)aistrat_ui<5>) {}
    BOOL ok() const {
        return x_pop.ok && x_pops.ok && x_pop_2.ok && pop2.ok && encounter.ok;
    }
};

// A target for the attack planning: cleared if the faction has one, else set at its base with the
// most land units (with launch set, those gather there so the launch checks are reached; otherwise
// every third land unit of the faction gathers, wherever it is).
static void aistrat_toggle_target(uint32_t faction_id, BOOL launch) {
    PlayerData *p = &PlayersData[faction_id];
    if (p->x_target >= 0 && !launch) {
        p->x_target = -1;
        p->y_target = -1;
        return;
    }
    int base_id_best = -1;
    int units_best = -1;
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        if (Bases[base_id].faction_id_current == faction_id) {
            int veh_id = veh_at(Bases[base_id].x, Bases[base_id].y);
            int units = veh_id >= 0 ? stack_check(veh_id, 3, TRIAD_LAND, faction_id, -1) : 0;
            if (units > units_best) {
                units_best = units;
                base_id_best = base_id;
            }
        }
    }
    if (base_id_best < 0) {
        return;
    }
    int x = Bases[base_id_best].x;
    int y = Bases[base_id_best].y;
    p->x_target = x;
    p->y_target = y;
    int n = 0;
    for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
        if (Vehs[veh_id].faction_id == faction_id && get_triad(veh_id) == TRIAD_LAND
            && (launch ? (Vehs[veh_id].x == x && Vehs[veh_id].y == y) : n++ % 3 == 0)) {
            Vehs[veh_id].state = (Vehs[veh_id].state & ~VSTATE_UNK_20000) | VSTATE_UNK_40000;
        }
    }
}

// What the original reached in one run of enemy_strategy (from the state it left).
struct AistratReach {
    int scouts_disbanded;
    int units_disbanded;
    int formers_rehomed;
    int transports_flagged;
    int landing_goals;
    int launched;
    int targeted;
    int plans_changed;
};

static void aistrat_reach(const GameSnapshot *b, const GameSnapshot *o, uint32_t faction_id,
    BOOL sequence, AistratReach *r) {
    int scouts[2] = { 0, 0 };
    for (int side = 0; side < 2; side++) {
        const GameSnapshot *s = side ? o : b;
        for (int i = 0; i < s->veh_count_global && i < s->veh_count; i++) {
            scouts[side] += s->vehs[i].proto_id == BSC_SCOUT_PATROL
                && (sequence || s->vehs[i].faction_id == faction_id);
        }
    }
    r->scouts_disbanded += scouts[0] - scouts[1];
    r->units_disbanded += b->veh_count_global - o->veh_count_global;
    if (b->veh_count_global == o->veh_count_global) { // same unit ids on both sides
        for (int i = 0; i < b->veh_count_global; i++) {
            const Veh *vb = &b->vehs[i], *vo = &o->vehs[i];
            r->formers_rehomed += vb->home_base_id != vo->home_base_id
                && VehPrototypes[vb->proto_id].plan == PLAN_TERRAFORMING;
            r->transports_flagged += !(vb->state & VSTATE_UNK_20000000)
                && (vo->state & VSTATE_UNK_20000000);
        }
    }
    for (uint32_t f = 0; f < MaxPlayerNum; f++) {
        if (!sequence && f != faction_id) {
            continue;
        }
        const PlayerData *pb = &b->players[f], *po = &o->players[f];
        for (int g = 0; g < MaxGoalsNum; g++) {
            r->landing_goals += po->goals[g].type == AI_GOAL_LANDING_SITE
                && (pb->goals[g].type != AI_GOAL_LANDING_SITE || pb->goals[g].x != po->goals[g].x
                    || pb->goals[g].y != po->goals[g].y);
        }
        r->launched += pb->x_target >= 0 && po->x_target < 0;
        r->targeted += pb->x_target < 0 && po->x_target >= 0;
        r->plans_changed += memcmp(pb->region_base_plan, po->region_base_plan,
            sizeof(pb->region_base_plan)) != 0;
    }
}

static int test_enemy_strategy() {
    OriginalEntry vanilla(0x00561080, EnemyStrategyEntry);
    ProdStubs stubs;
    AistratWtaStub wta;
    if (!vanilla.ok || !stubs.ok() || !wta.ok) {
        return 1;
    }
    func5 *exe = (func5 *)0x00561080;
    ProdHarness h;
    GameSnapshot pre;
    snap_alloc(&pre);
    static AistratExtra saved, orig, now;
    aistrat_take(&saved);
    const int turn_saved = *TurnCurrentNum;
    AistratReach reach;
    memset(&reach, 0, sizeof(reach));
    int wta_calls = 0;
    char label[96];
    // 0 as loaded; 1 the next turn; 2 this faction's human/computer status swapped; 3 the attack
    // target cleared, or one set with every third land unit gathering; 4 the first turn; 5 no human
    // players; 6 an attack staged at the faction's biggest land stack, all of it gathering. The
    // wants_to_attack leftovers cycle through the five patterns.
    const int variants = 7;
    for (int run = 0; run < MaxPlayerNum * variants + 1; run++) {
        BOOL sequence = run == MaxPlayerNum * variants; // every faction in turn, as the game does
        uint32_t f = sequence ? 0 : run / variants;
        int v = sequence ? 0 : run % variants;
        int pin[8];
        aistrat_make_pin(&h.base, sequence ? 1 : (run % 5), pin);
        auto setup = [&] {
            aistrat_restore(&saved);
            memcpy(AistratWtaPin, pin, sizeof(pin));
            AistratWtaCalls = 0;
            *TurnCurrentNum = v == 1 ? turn_saved + 1 : (v == 4 ? 1 : turn_saved);
            if (v == 2) {
                *FactionsStatus ^= (uint8_t)(1 << f);
            } else if (v == 3 || v == 6) {
                aistrat_toggle_target(f, v == 6);
            } else if (v == 5) {
                *FactionsStatus = 0;
            }
        };
        if (sequence) {
            sprintf_s(label, "enemy_strategy(0..7 in turn)");
        } else {
            sprintf_s(label, "enemy_strategy(%u) variant %d", f, v);
        }
        int before = h.mismatches;
        h.run(label, (uint32_t)(run * 131 + 7), setup,
            [&] {
                snap_take(&pre);
                if (sequence) {
                    for (uint32_t i = 0; i < MaxPlayerNum; i++) {
                        exe(i);
                    }
                } else {
                    exe(f);
                }
                aistrat_take(&orig);
            },
            [&] {
                if (sequence) {
                    for (uint32_t i = 0; i < MaxPlayerNum; i++) {
                        enemy_strategy(i);
                    }
                } else {
                    enemy_strategy(f);
                }
                aistrat_take(&now);
            });
        aistrat_reach(&pre, &h.after_orig, f, sequence, &reach);
        wta_calls += orig.wta_calls;
        if (h.mismatches == before && aistrat_differs(&orig, &now, label)) {
            h.mismatches++;
        }
    }
    snap_free(&pre);
    aistrat_restore(&saved);
    *TurnCurrentNum = turn_saved;
    logf("enemy_strategy: %d compared (%d changed the game state; by the original: %d units disbanded "
        "(%d scout patrols), %d formers rehomed, %d transports flagged, %d landing site goals, %d "
        "attacks launched or given up, %d new attack targets, %d runs changed region plans, %d "
        "wants_to_attack calls), %d mismatches", h.compared, h.changed, reach.units_disbanded,
        reach.scouts_disbanded, reach.formers_rehomed, reach.transports_flagged, reach.landing_goals,
        reach.launched, reach.targeted, reach.plans_changed, wta_calls, h.mismatches);
    return h.mismatches;
}

// Live runs (the real enemy_move) reach battle animations and waits that read timeGetTime(); both
// modules' import slots are pointed at a clock that advances 16 ms per call from the same start
// on both sides.
static DWORD AistratClock;
static DWORD WINAPI aistrat_time_get_time() {
    return AistratClock += 16;
}
struct AistratClockPin {
    void **exe_slot, **dll_slot;
    void *exe_saved, *dll_saved;
    AistratClockPin() {
        exe_slot = iat_slot(GetModuleHandle(NULL), "winmm.dll", "timeGetTime");
        dll_slot = iat_slot(GetModuleHandle("OpenSMACX.dll"), "winmm.dll", "timeGetTime");
        exe_saved = iat_patch(exe_slot, (void *)aistrat_time_get_time);
        dll_saved = iat_patch(dll_slot, (void *)aistrat_time_get_time);
    }
    ~AistratClockPin() {
        iat_patch(exe_slot, exe_saved);
        iat_patch(dll_slot, dll_saved);
    }
    BOOL ok() const { return exe_slot && dll_slot; }
};

// A JmpStub that is only put in place when asked for.
struct MaybeStub {
    JmpStub *stub;
    MaybeStub(BOOL use, uint32_t address, void *target)
        : stub(use ? new JmpStub(address, target) : NULL) {}
    ~MaybeStub() {
        delete stub;
    }
    BOOL ok() const {
        return !stub || stub->ok;
    }
};

// enemy_move and action (unless live), do_all_non_input (the DLL's, which the exe's calls also
// reach) and the network calls: synch_veh (the DLL's) and NetDaemon's await_synch, unlock_veh and
// action.
struct AistratStubs {
    MaybeStub move, action;
    JmpStub non_input, synch, await, unlock, net_action;
    explicit AistratStubs(BOOL live)
        : move(!live, 0x0056B5B0, (void *)aistrat_enemy_move),
          action(!live, (uint32_t)(void *)::action, (void *)aistrat_action), // DLL code since batch 30
          non_input((uint32_t)(void *)do_all_non_input, (void *)aistrat_non_input),
          synch((uint32_t)(void *)synch_veh, (void *)aistrat_synch_veh),
          await(0x00531480, (void *)aistrat_await_synch),
          unlock(0x005310F0, (void *)aistrat_unlock_veh),
          net_action(0x00532570, (void *)aistrat_net_action) {}
    BOOL ok() const {
        return move.ok() && action.ok() && non_input.ok && synch.ok && await.ok && unlock.ok
            && net_action.ok;
    }
};

// draw_radius() during enemy_veh's live runs: recorded as ProdStubs does, plus the chain of exe
// return addresses above it (the exe's functions all keep an ebp frame).
static uint32_t AistratDrawChains[MaxProdCalls][4];
static void __cdecl aistrat_draw_radius(int x, int y, int unk1, int unk2) {
    uint32_t *frame;
    __asm mov frame, ebp
    int n = ProdCallCount;
    for (int i = 0; i < 4; i++) { // the call site, then three callers up
        uint32_t ret = 0;
        if (frame && (uint32_t)frame > 0x10000) {
            ret = frame[1];
            frame = (uint32_t *)frame[0];
        }
        if (n < MaxProdCalls) {
            AistratDrawChains[n][i] = ret;
        }
    }
    prod_record(PF_DRAW_RADIUS, NULL, NULL, NULL, x, y, unk1, unk2);
}

// The exe's rand() during enemy_veh's live runs: the same generator on the same per-thread state
// (the CRT's _getptd at 0x6491C3, seed at +0x14), with each caller noted.
typedef uint8_t *__cdecl func_getptd();
const int MaxAistratRandCalls = 512;
static uint32_t AistratRandCallers[MaxAistratRandCalls];
static int AistratRandCount;
static int __cdecl aistrat_exe_rand() {
    uint32_t caller;
    __asm mov eax, [ebp + 4]
    __asm mov caller, eax
    if (AistratRandCount < MaxAistratRandCalls) {
        AistratRandCallers[AistratRandCount] = caller;
    }
    AistratRandCount++;
    uint8_t *ptd = ((func_getptd *)0x006491C3)();
    uint32_t seed = *(uint32_t *)(ptd + 0x14) * 0x343FD + 0x269EC3;
    *(uint32_t *)(ptd + 0x14) = seed;
    return (seed >> 16) & 0x7FFF;
}

// The popp/wave_it/interlude/draw_radius/help_topic calls a run recorded, when the two runs differ.
static void aistrat_log_prod_calls(const char *label, const ProdState *o, const ProdState *n) {
    if (o->call_count == n->call_count && !memcmp(o->calls, n->calls, sizeof(o->calls))) {
        return;
    }
    for (int side = 0; side < 2; side++) {
        const ProdState *s = side ? n : o;
        logf("    %s: %s made %d recorded calls", label, side ? "second run" : "first run", s->call_count);
        for (int i = 0; i < s->call_count && i < MaxProdCalls; i++) {
            const ProdCall *c = &s->calls[i];
            logf("      %d: fn %d label '%s' pic '%s' file '%s' args %d, %d, %d, %d", i, c->fn, c->label,
                c->pic, c->file, c->args[0], c->args[1], c->args[2], c->args[3]);
        }
    }
}

static int test_enemy_veh() {
    OriginalEntry vanilla(0x00579240, EnemyVehEntry);
    ProdStubs prod_stubs;
    if (!vanilla.ok || !prod_stubs.ok()) {
        return 1;
    }
    func_i *exe = (func_i *)0x00579240;
    ProdHarness h;
    ProdHarness control; // live runs: the original against itself first
    static AistratExtra saved, orig, now;
    static AistratLog log_o, log_n;
    aistrat_take(&saved);
    int moved = 0, done = 0, net_runs = 0, live_runs = 0, unstable = 0;
    char label[96];
    const int count = h.base.veh_count_global;
    const int stride = count > 96 ? count / 96 : 1;
    // stubbed: enemy_move, action and the network calls replaced. 0 as loaded; 1 enemy_move done;
    // 2 enemy_move spends the moves; 3 no action possible; 4 on a move to order with moves spent
    // (enemy_move skipped unless next to an enemy or a transport); 5 the same exploring; 6 the same,
    // enemy_move spending the moves; 7-11 a network game: the unit's faction at the controls, then
    // NetDaemon::action done, moves spent, another faction at the controls, a move to order.
    // live: enemy_move and action as they are, on every 8th unit of each computer faction (up to 6
    // each) with its moves restored, as at the start of its turn.
    for (int live = 0; live <= 1; live++) {
        AistratStubs stubs(live);
        AistratUiStubs ui;
        AistratClockPin clock;
        MaybeStub draw(live, 0x0046AEF0, (void *)aistrat_draw_radius); // over ProdStubs' recorder
        MaybeStub exe_rand(live, 0x0064601D, (void *)aistrat_exe_rand);
        if (!stubs.ok() || !ui.ok() || !clock.ok() || !draw.ok() || !exe_rand.ok()) {
            logf("enemy_veh: cannot stub the callees");
            return h.mismatches + 1;
        }
        int picked[MaxPlayerNum] = { 0 };
        int seen[MaxPlayerNum] = { 0 };
        for (int veh_id = 0; veh_id < count; veh_id += live ? 1 : stride) {
            uint32_t f = h.base.vehs[veh_id].faction_id;
            if (live && ((*FactionsStatus & (1 << f)) || seen[f]++ % 8 || picked[f] >= 6)) {
                continue;
            }
            picked[f]++;
            for (int v = 0; v < (live ? 1 : 12); v++) {
                auto setup = [&] {
                    aistrat_restore(&saved);
                    aistrat_calls_reset();
                    AistratClock = 1000000;
                    if (live) {
                        Vehs[veh_id].moves_expended = 0;
                    }
                    AistratMoveRet = v == 1;
                    AistratMoveSpend = (v == 2 || v == 6 || v == 9) ? 1 : (v == 11 ? 2 : 0);
                    AistratActionRet = v != 3;
                    AistratNetActionRet = v == 8;
                    Veh *veh = &Vehs[veh_id];
                    if (v == 4 || v == 5 || v == 6 || v == 11) {
                        veh->order = ORDER_MOVE_TO;
                        veh->moves_expended = 1;
                    }
                    if (v == 5) {
                        veh->state |= VSTATE_EXPLORE;
                    }
                    if (v >= 7) {
                        *IsMultiplayerNet = 1;
                        *FactionIDCurrent = v == 10 ? (veh->faction_id + 1) % MaxPlayerNum : veh->faction_id;
                    }
                };
                sprintf_s(label, "enemy_veh(%d) %s %d", veh_id, live ? "live" : "variant", v);
                REGRESS_CTX("%s", label);
                BOOL self_consistent = true;
                if (live) {
                    char control_label[112];
                    sprintf_s(control_label, "%s, original against itself", label);
                    int before = control.mismatches;
                    static uint32_t chains_o[MaxProdCalls][4], chains_n[MaxProdCalls][4];
                    static uint32_t rands_o[MaxAistratRandCalls], rands_n[MaxAistratRandCalls];
                    int rand_count_o = 0, rand_count_n = 0;
                    control.run(control_label, (uint32_t)(veh_id * 17 + v), setup,
                        [&] { AistratRandCount = 0; AistratCalls.ret = exe(veh_id); log_o = AistratCalls;
                            aistrat_take(&orig); memcpy(chains_o, AistratDrawChains, sizeof(chains_o));
                            memcpy(rands_o, AistratRandCallers, sizeof(rands_o)); rand_count_o = AistratRandCount; },
                        [&] { AistratRandCount = 0; AistratCalls.ret = exe(veh_id); log_n = AistratCalls;
                            aistrat_take(&now); memcpy(chains_n, AistratDrawChains, sizeof(chains_n));
                            memcpy(rands_n, AistratRandCallers, sizeof(rands_n)); rand_count_n = AistratRandCount; });
                    self_consistent = control.mismatches == before
                        && !aistrat_log_differs(&log_o, &log_n, control_label)
                        && !aistrat_differs(&orig, &now, control_label);
                    aistrat_log_prod_calls(control_label, &control.orig, &control.now);
                    if (!self_consistent) {
                        for (int side = 0; side < 2; side++) {
                            const ProdState *s = side ? &control.now : &control.orig;
                            for (int i = 0; i < s->call_count && i < MaxProdCalls; i++) {
                                const uint32_t *c = side ? chains_n[i] : chains_o[i];
                                char where[4][80];
                                for (int k = 0; k < 4; k++) {
                                    HMODULE mod = NULL;
                                    char name[MAX_PATH] = "?";
                                    if (c[k] && GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                        | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)(uintptr_t)c[k],
                                        &mod) && mod) {
                                        GetModuleFileNameA(mod, name, sizeof(name));
                                    }
                                    const char *leaf = strrchr(name, '\\') ? strrchr(name, '\\') + 1 : name;
                                    _snprintf_s(where[k], _TRUNCATE, "%08X (%s+%X)", c[k], leaf,
                                        mod ? (unsigned)(c[k] - (uintptr_t)mod) : 0);
                                }
                                logf("      %s run call %d from %s <- %s <- %s <- %s",
                                    side ? "second" : "first", i, where[0], where[1], where[2], where[3]);
                            }
                        }
                        snap_differs(&control.after_orig, &control.after_new, control_label);
                        int first = -1;
                        int shown_o = rand_count_o < MaxAistratRandCalls ? rand_count_o : MaxAistratRandCalls;
                        int shown_n = rand_count_n < MaxAistratRandCalls ? rand_count_n : MaxAistratRandCalls;
                        for (int i = 0; i < shown_o || i < shown_n; i++) {
                            if (i >= shown_o || i >= shown_n || rands_o[i] != rands_n[i]) {
                                first = i;
                                break;
                            }
                        }
                        logf("    exe rand() calls %d / %d, first different caller at call %d", rand_count_o,
                            rand_count_n, first);
                        for (int i = first < 3 ? 0 : first - 3; first >= 0 && i < first + 6; i++) {
                            logf("      rand call %d: first run from %08X, second run from %08X", i,
                                i < shown_o ? rands_o[i] : 0, i < shown_n ? rands_n[i] : 0);
                        }
                        logf("    unit count after the runs %d / %d, random %08X / %08X, rand() %d / %d",
                            control.after_orig.veh_count_global, control.after_new.veh_count_global,
                            control.orig.notice.seed, control.now.notice.seed, control.orig.rand_next,
                            control.now.rand_next);
                        const Veh *veh = &h.base.vehs[veh_id];
                        logf("    unit %d: faction %u proto %d (%s) at %d,%d order %d ai type %d, moves "
                            "%d; after the runs at %d,%d / %d,%d", veh_id, veh->faction_id,
                            veh->proto_id, VehPrototypes[veh->proto_id].veh_name, veh->x, veh->y,
                            veh->order, veh->move_to_ai_type, veh->moves_expended,
                            control.after_orig.vehs[veh_id].x, control.after_orig.vehs[veh_id].y,
                            control.after_new.vehs[veh_id].x, control.after_new.vehs[veh_id].y);
                    }
                    unstable += !self_consistent;
                }
                int before = h.mismatches;
                h.run(label, (uint32_t)(veh_id * 17 + v), setup,
                    [&] { AistratCalls.ret = exe(veh_id); log_o = AistratCalls; aistrat_take(&orig); },
                    [&] { AistratCalls.ret = enemy_veh(veh_id); log_n = AistratCalls; aistrat_take(&now); });
                if (h.mismatches == before && (aistrat_log_differs(&log_o, &log_n, label)
                    || aistrat_differs(&orig, &now, label))) {
                    h.mismatches++;
                }
                if (h.mismatches > before) {
                    const Veh *veh = &h.base.vehs[veh_id];
                    logf("    unit %d: faction %u proto %d (%s) at %d,%d order %d ai type %d", veh_id,
                        veh->faction_id, veh->proto_id, VehPrototypes[veh->proto_id].veh_name, veh->x,
                        veh->y, veh->order, veh->move_to_ai_type);
                    aistrat_log_prod_calls(label, &h.orig, &h.now);
                    if (!self_consistent) {
                        logf("  %s: not counted, the original is not self-consistent here", label);
                        h.mismatches = before;
                    }
                }
                done += log_o.ret;
                moved += memcmp(&h.after_orig.vehs[veh_id], &h.base.vehs[veh_id], 4) != 0;
                net_runs += v >= 7;
                live_runs += live;
            }
        }
    }
    aistrat_restore(&saved);
    logf("enemy_veh: %d compared (%d live, %d of them with the original not self-consistent, %d network; "
        "by the original: %d done, %d moved), %d mismatches", h.compared, live_runs, unstable, net_runs,
        done, moved, h.mismatches);
    return h.mismatches;
}

static int test_enemy_turn() {
    OriginalEntry vanilla(0x00579510, EnemyTurnEntry);
    ProdStubs prod_stubs;
    if (!vanilla.ok || !prod_stubs.ok()) {
        return 1;
    }
    func5 *exe = (func5 *)0x00579510;
    ProdHarness h;
    static AistratExtra saved, orig, now;
    static AistratLog log_o, log_n;
    aistrat_take(&saved);
    int steps = 0, killed = 0, live_runs = 0, live_moved = 0;
    char label[96];
    // stubbed: enemy_veh replaced (patterns 0-4, see aistrat_enemy_veh), every faction; live: the
    // DLL's enemy_veh with the real enemy_move (the same code on both sides; the network calls and
    // do_all_non_input stay stubbed) for the natives and the two smallest computer factions, their
    // units' moves restored as at the start of their turn.
    uint32_t live_factions[3] = { 0, 0, 0 };
    {
        int units[MaxPlayerNum] = { 0 };
        for (int i = 0; i < h.base.veh_count_global; i++) {
            units[h.base.vehs[i].faction_id]++;
        }
        for (int k = 1; k < 3; k++) {
            int best = -1;
            for (uint32_t f = 1; f < MaxPlayerNum; f++) {
                if (!(*FactionsStatus & (1 << f)) && units[f] && f != live_factions[1]
                    && (best < 0 || units[f] < units[best])) {
                    best = f;
                }
            }
            live_factions[k] = best > 0 ? best : 0;
        }
    }
    ProdHarness control; // live runs: the original against itself first
    int unstable = 0;
    for (int live = 0; live <= 1; live++) {
        AistratStubs stubs(true);
        AistratUiStubs ui;
        MaybeStub veh_exe(!live, 0x00579240, (void *)aistrat_enemy_veh);
        MaybeStub veh_dll(!live, (uint32_t)(void *)enemy_veh, (void *)aistrat_enemy_veh);
        AistratClockPin clock;
        if (!stubs.ok() || !ui.ok() || !veh_exe.ok() || !veh_dll.ok() || !clock.ok()) {
            logf("enemy_turn: cannot stub the callees");
            return h.mismatches + 1;
        }
        for (int k = 0; k < (live ? 3 : MaxPlayerNum); k++) {
            uint32_t f = live ? live_factions[k] : k;
            if (live && k && (!f || f == live_factions[k - 1])) {
                continue;
            }
            for (int pattern = 0; pattern < (live ? 1 : 5); pattern++) {
                auto setup = [&] {
                    aistrat_restore(&saved);
                    aistrat_calls_reset();
                    AistratVehPattern = pattern;
                    AistratClock = 1000000;
                    if (live) {
                        for (int i = 0; i < *VehCurrentCount; i++) {
                            if (Vehs[i].faction_id == f) {
                                Vehs[i].moves_expended = 0;
                            }
                        }
                    }
                };
                sprintf_s(label, "enemy_turn(%u) %s %d", f, live ? "live" : "pattern", pattern);
                REGRESS_CTX("%s", label);
                BOOL self_consistent = true;
                if (live) {
                    char control_label[112];
                    sprintf_s(control_label, "%s, original against itself", label);
                    int before = control.mismatches;
                    control.run(control_label, (uint32_t)(f * 29 + pattern), setup,
                        [&] { exe(f); log_o = AistratCalls; aistrat_take(&orig); },
                        [&] { exe(f); log_n = AistratCalls; aistrat_take(&now); });
                    self_consistent = control.mismatches == before
                        && !aistrat_log_differs(&log_o, &log_n, control_label)
                        && !aistrat_differs(&orig, &now, control_label);
                    unstable += !self_consistent;
                }
                int before = h.mismatches;
                h.run(label, (uint32_t)(f * 29 + pattern), setup,
                    [&] { exe(f); log_o = AistratCalls; aistrat_take(&orig); },
                    [&] { enemy_turn(f); log_n = AistratCalls; aistrat_take(&now); });
                if (h.mismatches == before && (aistrat_log_differs(&log_o, &log_n, label)
                    || aistrat_differs(&orig, &now, label))) {
                    h.mismatches++;
                }
                if (!self_consistent && h.mismatches > before) {
                    logf("  %s: not counted, the original is not self-consistent here", label);
                    h.mismatches = before;
                }
                steps += log_o.count;
                killed += h.base.veh_count_global - h.after_orig.veh_count_global;
                if (live) {
                    live_runs++;
                    for (int i = 0; i < h.base.veh_count_global && i < h.after_orig.veh_count_global; i++) {
                        live_moved += h.base.vehs[i].faction_id == f
                            && memcmp(&h.base.vehs[i], &h.after_orig.vehs[i], 4) != 0;
                    }
                }
            }
        }
    }
    aistrat_restore(&saved);
    logf("enemy_turn: %d compared (%d live, factions %u/%u/%u, %d of them with the original not "
        "self-consistent; by the original: %d recorded calls, %d units gone, %d units moved in the live "
        "runs), %d mismatches", h.compared, live_runs, live_factions[0], live_factions[1],
        live_factions[2], unstable, steps, killed, live_moved, h.mismatches);
    return h.mismatches;
}

// ---------------------------------------------------------------- batch 31: faction turn

// Stub a function wherever it runs: at its exe address and, when the patched exe redirects that
// address into the DLL (jmp [IAT]), at the DLL function as well, since DLL code calls it directly.
// (production_phase, enemy_diplomacy and enemy_strategy may move into the DLL in a later batch.)
static uint32_t redirect_target(uint32_t address) {
    const uint8_t *p = (const uint8_t *)address;
    return (p[0] == 0xFF && p[1] == 0x25) ? **(uint32_t **)(p + 2) : 0;
}
struct StubAnywhere {
    uint32_t dll_address;
    JmpStub *dll_stub;
    JmpStub exe_stub;
    StubAnywhere(uint32_t address, void *target)
        : dll_address(redirect_target(address)),
          dll_stub(dll_address ? new JmpStub(dll_address, target) : NULL),
          exe_stub(address, target) {}
    ~StubAnywhere() {
        delete dll_stub;
    }
    BOOL ok() const {
        return exe_stub.ok && (!dll_stub || dll_stub->ok);
    }
};

// Interface and turn phase calls are recorded (function, arguments, text) on both sides.
enum FtFn { FT_CHECK_LOC, FT_CHECK_BASE, FT_DELETION, FT_DRAW_TILE, FT_DRAW_RADIUS, FT_FOCUS,
    FT_BOOM, FT_NETMSG_POP, FT_VEH_SCOOT, FT_POPP, FT_SOCIAL_UPKEEP, FT_REPAIR_PHASE,
    FT_PRODUCTION_PHASE, FT_ALLOCATE_ENERGY, FT_ENEMY_DIPLOMACY, FT_ENEMY_STRATEGY, FT_SOCIAL_AI,
    FT_NON_INPUT, FT_ELIMINATE, FT_CALL_COUNCIL, FT_AUTO_SAVE };
static const char *const FtFnName[] = { "BaseWin::check_loc", "BaseWin::check_base",
    "DeletionList::add_deletion", "draw_tile", "draw_radius", "Console::focus", "boom",
    "NetMsg::pop", "veh_scoot", "popp", "social_upkeep", "repair_phase", "production_phase",
    "allocate_energy", "enemy_diplomacy", "enemy_strategy", "social_ai", "do_all_non_input",
    "eliminate_player", "call_council", "auto_save" };
struct FtCall {
    int fn;
    int args[6];
    char text[48];
};
const int MaxFtCalls = 192;
static FtCall FtCalls[MaxFtCalls];
static int FtCallCount;

static void ft_record(int fn, int a0, int a1, int a2, int a3, int a4, int a5, LPCSTR text) {
    if (FtCallCount < MaxFtCalls) {
        FtCall *call = &FtCalls[FtCallCount];
        memset(call, 0, sizeof(*call));
        call->fn = fn;
        call->args[0] = a0;
        call->args[1] = a1;
        call->args[2] = a2;
        call->args[3] = a3;
        call->args[4] = a4;
        call->args[5] = a5;
        strncpy_s(call->text, text ? text : "", _TRUNCATE);
    }
    FtCallCount++;
}
static void __fastcall ft_check_loc(void *This, void *, int x, int y, int unk) {
    ft_record(FT_CHECK_LOC, This == BaseWinVar, x, y, unk, 0, 0, NULL);
}
static void __fastcall ft_check_base(void *This, void *, int base_id) {
    ft_record(FT_CHECK_BASE, This == BaseWinVar, base_id, 0, 0, 0, 0, NULL);
}
static void __fastcall ft_deletion(void *This, void *, int veh_id) {
    ft_record(FT_DELETION, This == DeletionListVar, veh_id, 0, 0, 0, 0, NULL);
}
static void __cdecl ft_draw_tile(int x, int y, int mode) {
    ft_record(FT_DRAW_TILE, x, y, mode, 0, 0, 0, NULL);
}
static void __cdecl ft_draw_radius(int x, int y, int unk1, int unk2) {
    ft_record(FT_DRAW_RADIUS, x, y, unk1, unk2, 0, 0, NULL);
}
static int __fastcall ft_focus(void *This, void *, int x, int y, int faction_id) {
    ft_record(FT_FOCUS, This == ConsoleVar, x, y, faction_id, 0, 0, NULL);
    return 0;
}
static void __cdecl ft_boom(int x, int y, int flags) {
    ft_record(FT_BOOM, x, y, flags, 0, 0, 0, NULL);
}
static int __fastcall ft_netmsg_pop(void *This, void *, LPCSTR label, int delay, int flags,
                                    LPCSTR pcx_file) {
    char text[48];
    _snprintf_s(text, _TRUNCATE, "%s|%s", label ? label : "", pcx_file ? pcx_file : "");
    ft_record(FT_NETMSG_POP, This == NetMsgVar, delay, flags, 0, 0, 0, text);
    return 0;
}
static void __cdecl ft_veh_scoot(int veh_id, int x, int y, int dir, int unk) {
    ft_record(FT_VEH_SCOOT, veh_id, x, y, dir, unk, 0, NULL);
}
static int __cdecl ft_popp(LPCSTR file, LPCSTR label, int unk, LPCSTR pic, int(__cdecl *callback)()) {
    char text[48];
    _snprintf_s(text, _TRUNCATE, "%s|%s|%s", file ? file : "", label ? label : "", pic ? pic : "");
    ft_record(FT_POPP, unk, callback != NULL, 0, 0, 0, 0, text);
    return 0;
}
static void __cdecl ft_social_upkeep(int faction_id) {
    ft_record(FT_SOCIAL_UPKEEP, faction_id, 0, 0, 0, 0, 0, NULL);
}
static void __cdecl ft_repair_phase(int faction_id) {
    ft_record(FT_REPAIR_PHASE, faction_id, 0, 0, 0, 0, 0, NULL);
}
static void __cdecl ft_production_phase(int faction_id) {
    ft_record(FT_PRODUCTION_PHASE, faction_id, 0, 0, 0, 0, 0, NULL);
}
static void __cdecl ft_allocate_energy(int faction_id) {
    ft_record(FT_ALLOCATE_ENERGY, faction_id, 0, 0, 0, 0, 0, NULL);
}
static void __cdecl ft_enemy_diplomacy(int faction_id) {
    ft_record(FT_ENEMY_DIPLOMACY, faction_id, 0, 0, 0, 0, 0, NULL);
}
static void __cdecl ft_enemy_strategy(int faction_id) {
    ft_record(FT_ENEMY_STRATEGY, faction_id, 0, 0, 0, 0, 0, NULL);
}
static void __cdecl ft_social_ai(int faction_id, int growth, int tech, int wealth, int power,
                                 void *output) {
    ft_record(FT_SOCIAL_AI, faction_id, growth, tech, wealth, power, (int)output, NULL);
}
static void __cdecl ft_non_input() {
    ft_record(FT_NON_INPUT, 0, 0, 0, 0, 0, 0, NULL);
}
static int __cdecl ft_eliminate(int faction_id, int faction_id_victor) {
    ft_record(FT_ELIMINATE, faction_id, faction_id_victor, 0, 0, 0, 0, NULL);
    return 0;
}
static void __cdecl ft_call_council(int faction_id) {
    ft_record(FT_CALL_COUNCIL, faction_id, 0, 0, 0, 0, 0, NULL);
}
static void __cdecl ft_auto_save() {
    ft_record(FT_AUTO_SAVE, 0, 0, 0, 0, 0, 0, NULL); // never writes a save during the tests
}

// The interface calls of veh_kill, kill, mash_planes and interceptor.
struct FtUiStubs {
    StubAnywhere check_loc, check_base, deletion, draw_tile, draw_radius, focus, boom, netmsg, scoot;
    FtUiStubs()
        : check_loc(0x0041A7E0, (void *)ft_check_loc),
          check_base(0x0041A7A0, (void *)ft_check_base),
          deletion(0x0052DE70, (void *)ft_deletion),
          draw_tile(0x0046AF40, (void *)ft_draw_tile),
          draw_radius(0x0046AEF0, (void *)ft_draw_radius),
          focus(0x005108A0, (void *)ft_focus),
          boom(0x00504AA0, (void *)ft_boom),
          netmsg(0x0047A890, (void *)ft_netmsg_pop),
          scoot(0x0055A150, (void *)ft_veh_scoot) {}
    BOOL ok() const {
        return check_loc.ok() && check_base.ok() && deletion.ok() && draw_tile.ok()
            && draw_radius.ok() && focus.ok() && boom.ok() && netmsg.ok() && scoot.ok();
    }
};

// faction_upkeep's callees: each turn phase and every dialog is recorded instead of run, so the
// comparison is of faction_upkeep's own logic (the phases have tests of their own).
struct FtTurnStubs {
    StubAnywhere social_upkeep, repair, production, allocate, diplomacy, strategy, social_ai,
        non_input, eliminate, council, auto_save, popp;
    FtTurnStubs()
        : social_upkeep(0x005B44D0, (void *)ft_social_upkeep),
          repair(0x00526030, (void *)ft_repair_phase),
          production(0x00526E70, (void *)ft_production_phase),
          allocate(0x005267B0, (void *)ft_allocate_energy),
          diplomacy(0x0055F930, (void *)ft_enemy_diplomacy),
          strategy(0x00561080, (void *)ft_enemy_strategy),
          social_ai(0x005B4790, (void *)ft_social_ai),
          non_input(0x005FCB20, (void *)ft_non_input),
          eliminate(0x005B3380, (void *)ft_eliminate),
          council(0x0052C880, (void *)ft_call_council),
          auto_save(0x005ABD20, (void *)ft_auto_save),
          popp(0x0048C0A0, (void *)ft_popp) {}
    BOOL ok() const {
        return social_upkeep.ok() && repair.ok() && production.ok() && allocate.ok()
            && diplomacy.ok() && strategy.ok() && social_ai.ok() && non_input.ok() && eliminate.ok()
            && council.ok() && auto_save.ok() && popp.ok();
    }
};

static int FtNetRef; // what NetVehIDRef points at in the veh_kill test
static int FtOut[10]; // compute_score's outputs when the caller passes its own: [0] total, [1..9] lines
static BOOL FtEntryFailed; // an OriginalEntry could not be put back: the "original" would be the DLL

// What the faction turn functions can change besides the game state in GameSnapshot.
struct FtState {
    NoticeState notice; // parse slots and defaults, stringTemp, diplomacy globals, game seed
    uint8_t globals[BaseGlobalsSize];
    Base *current;
    int selected;
    int compute_id;
    int refs[5]; // MapWinVehIDSelected, MapWinVehIDOther, StatusWinVehIDA/B, VehIDCurrent
    int net_ref;
    int lift[3]; // VehDropLiftVehID, VehLiftX, VehLiftY
    int control_upkeep;
    int ascent_scale;
    int ascent[MaxPlayerNum];
    uint8_t tech[MaxTechnologyNum];
    int out[10];
    int ret;
    int rand_next; // the exe's rand() after the call
    FtCall calls[MaxFtCalls];
    int call_count;
};

static void ft_take(FtState *s, int ret, int rand_next) {
    notice_take(&s->notice, 0);
    memcpy(s->globals, BaseGlobals, BaseGlobalsSize);
    s->current = *BaseCurrent;
    s->selected = *BaseIDCurrentSelected;
    s->compute_id = *BaseComputeID;
    s->refs[0] = *MapWinVehIDSelected;
    s->refs[1] = *MapWinVehIDOther;
    s->refs[2] = *StatusWinVehIDA;
    s->refs[3] = *StatusWinVehIDB;
    s->refs[4] = *VehIDCurrent;
    s->net_ref = FtNetRef;
    s->lift[0] = *VehDropLiftVehID;
    s->lift[1] = *VehLiftX;
    s->lift[2] = *VehLiftY;
    s->control_upkeep = *ControlUpkeepA;
    s->ascent_scale = *AscentScoreScale;
    memcpy(s->ascent, AscentScores, sizeof(s->ascent));
    memcpy(s->tech, GameTechAchieved, sizeof(s->tech));
    memcpy(s->out, FtOut, sizeof(s->out));
    s->ret = ret;
    s->rand_next = rand_next;
    memcpy(s->calls, FtCalls, sizeof(s->calls));
    s->call_count = FtCallCount;
}

static void ft_restore(const FtState *s) {
    notice_restore(&s->notice);
    memcpy(BaseGlobals, s->globals, BaseGlobalsSize);
    *BaseCurrent = s->current;
    *BaseIDCurrentSelected = s->selected;
    *BaseComputeID = s->compute_id;
    *MapWinVehIDSelected = s->refs[0];
    *MapWinVehIDOther = s->refs[1];
    *StatusWinVehIDA = s->refs[2];
    *StatusWinVehIDB = s->refs[3];
    *VehIDCurrent = s->refs[4];
    FtNetRef = s->net_ref;
    *VehDropLiftVehID = s->lift[0];
    *VehLiftX = s->lift[1];
    *VehLiftY = s->lift[2];
    *ControlUpkeepA = s->control_upkeep;
    *AscentScoreScale = s->ascent_scale;
    memcpy(AscentScores, s->ascent, sizeof(s->ascent));
    memcpy(GameTechAchieved, s->tech, sizeof(s->tech));
}

static BOOL ft_differs(const FtState *o, const FtState *n, const char *label) {
#define DLOG(...) do { if (label) logf(__VA_ARGS__); } while (0)
    if (o->ret != n->ret || o->call_count != n->call_count || o->rand_next != n->rand_next) {
        DLOG("  %s: orig returned %d after %d calls (rand %d), new %d after %d calls (rand %d)",
            label, o->ret, o->call_count, o->rand_next, n->ret, n->call_count, n->rand_next);
        return true;
    }
    for (int i = 0; i < o->call_count && i < MaxFtCalls; i++) {
        const FtCall *a = &o->calls[i], *b = &n->calls[i];
        if (a->fn != b->fn || memcmp(a->args, b->args, sizeof(a->args)) || strcmp(a->text, b->text)) {
            DLOG("  %s: call %d: orig %s(%d, %d, %d, %d, %d, %d) '%s' new %s(%d, %d, %d, %d, %d, %d) '%s'",
                label, i, FtFnName[a->fn], a->args[0], a->args[1], a->args[2], a->args[3], a->args[4],
                a->args[5], a->text, FtFnName[b->fn], b->args[0], b->args[1], b->args[2], b->args[3],
                b->args[4], b->args[5], b->text);
            return true;
        }
    }
    if (notice_differs(&o->notice, &n->notice, label)
        || base_globals_differ(o->globals, n->globals, label)) {
        return true;
    }
    if (o->current != n->current || o->selected != n->selected || o->compute_id != n->compute_id
        || memcmp(o->refs, n->refs, sizeof(o->refs)) || o->net_ref != n->net_ref
        || memcmp(o->lift, n->lift, sizeof(o->lift)) || o->control_upkeep != n->control_upkeep) {
        DLOG("  %s: base %p/%p selected %d/%d compute %d/%d unit refs %d,%d,%d,%d,%d/%d,%d,%d,%d,%d "
            "net %d/%d lift %d,%d,%d/%d,%d,%d upkeep %d/%d", label, (void *)o->current,
            (void *)n->current, o->selected, n->selected, o->compute_id, n->compute_id, o->refs[0],
            o->refs[1], o->refs[2], o->refs[3], o->refs[4], n->refs[0], n->refs[1], n->refs[2],
            n->refs[3], n->refs[4], o->net_ref, n->net_ref, o->lift[0], o->lift[1], o->lift[2],
            n->lift[0], n->lift[1], n->lift[2], o->control_upkeep, n->control_upkeep);
        return true;
    }
    if (o->ascent_scale != n->ascent_scale || memcmp(o->ascent, n->ascent, sizeof(o->ascent))
        || memcmp(o->tech, n->tech, sizeof(o->tech)) || memcmp(o->out, n->out, sizeof(o->out))) {
        DLOG("  %s: ascent %d [%d %d %d %d] / %d [%d %d %d %d], outputs %d %d %d %d %d %d %d %d %d %d"
            " / %d %d %d %d %d %d %d %d %d %d", label, o->ascent_scale, o->ascent[0], o->ascent[1],
            o->ascent[2], o->ascent[3], n->ascent_scale, n->ascent[0], n->ascent[1], n->ascent[2],
            n->ascent[3], o->out[0], o->out[1], o->out[2], o->out[3], o->out[4], o->out[5], o->out[6],
            o->out[7], o->out[8], o->out[9], n->out[0], n->out[1], n->out[2], n->out[3], n->out[4],
            n->out[5], n->out[6], n->out[7], n->out[8], n->out[9]);
        return true;
    }
    return false;
#undef DLOG
}

// Run a faction turn function as the exe and as the DLL from the same state and compare.
struct FtHarness {
    const char *name;
    GameSnapshot base, after_orig, after_new;
    FtState saved, orig, now;
    uint8_t status_saved[2];
    int current_saved, sp_saved[MaxSecretProjectNum];
    uint32_t rules_saved, prefs_saved, obj_req_saved, obj_sudden_saved, native_saved;
    BOOL net_saved, battle_saved;
    int *net_ref_saved;
    int compared, mismatches, with_calls, changed;
    FtHarness(const char *test_name) : name(test_name), compared(0), mismatches(0), with_calls(0),
        changed(0) {
        snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
        snap_take(&base);
        FtCallCount = 0;
        FtNetRef = -1;
        memset(FtOut, 0x5A, sizeof(FtOut));
        ft_take(&saved, 0, 0);
        memcpy(status_saved, FactionsStatus, sizeof(status_saved));
        current_saved = *FactionIDCurrent;
        memcpy(sp_saved, SecretProjectTable, sizeof(sp_saved));
        rules_saved = *GameRules;
        prefs_saved = *GamePreferences;
        obj_req_saved = *ObjectiveReqVictory;
        obj_sudden_saved = *ObjectivesSuddenDeathVictory;
        native_saved = *MapNativeLifeForms;
        net_saved = *IsMultiplayerNet;
        battle_saved = VehBattleUnkTgl[0];
        net_ref_saved = *NetVehIDRef;
        FtEntryFailed = false;
    }
    ~FtHarness() {
        prepare();
        snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    }
    void prepare() {
        snap_restore(&base);
        ft_restore(&saved);
        memcpy(FactionsStatus, status_saved, sizeof(status_saved));
        *FactionIDCurrent = current_saved;
        memcpy(SecretProjectTable, sp_saved, sizeof(sp_saved));
        *GameRules = rules_saved;
        *GamePreferences = prefs_saved;
        *ObjectiveReqVictory = obj_req_saved;
        *ObjectivesSuddenDeathVictory = obj_sudden_saved;
        *MapNativeLifeForms = native_saved;
        *IsMultiplayerNet = net_saved;
        VehBattleUnkTgl[0] = battle_saved;
        *NetVehIDRef = net_ref_saved;
        FtCallCount = 0;
        memset(FtOut, 0x5A, sizeof(FtOut));
    }
    // setup() puts the variant in place after each restore; exe() and dll() make the call and
    // return its result.
    template <class S, class E, class D>
    void run(const char *label, uint32_t seed, S setup, E exe, D dll) {
        REGRESS_CTX("%s", label);
        prepare();
        setup();
        reseed(seed);
        int ret_o = exe();
        int rand_o = _rand();
        snap_take(&after_orig);
        ft_take(&orig, ret_o, rand_o);
        prepare();
        setup();
        reseed(seed);
        int ret_n = dll();
        int rand_n = _rand();
        snap_take(&after_new);
        ft_take(&now, ret_n, rand_n);
        compared++;
        with_calls += orig.call_count > 0;
        changed += snap_differs(&base, &after_orig, NULL);
        const char *detail = mismatches < MAX_DETAIL ? label : NULL;
        if (ft_differs(&orig, &now, detail) || snap_differs(&after_orig, &after_new, detail)) {
            mismatches++;
        }
    }
    int report() {
        if (FtEntryFailed) {
            logf("%s: the original entry could not be restored", name);
            return mismatches + 1;
        }
        return mismatches;
    }
};

// The exe's own code for each function, with its entry put back for the call.
static int veh_kill_exe(int veh_id) {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08 };
    OriginalEntry original(0x005C08C0, entry);
    FtEntryFailed |= !original.ok;
    ((void(__cdecl *)(int))0x005C08C0)(veh_id);
    return 0;
}
static int kill_exe(int veh_id) {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10 };
    OriginalEntry original(0x005C0B00, entry);
    FtEntryFailed |= !original.ok;
    ((void(__cdecl *)(int))0x005C0B00)(veh_id);
    return 0;
}
static int mash_planes_exe() {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x51, 0xA1, 0xC8 };
    OriginalEntry original(0x00529C00, entry);
    FtEntryFailed |= !original.ok;
    ((void(__cdecl *)())0x00529C00)();
    return 0;
}
static int interceptor_exe(int faction_id, int faction_id_atk, int x, int y) {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x18 };
    OriginalEntry original(0x00506650, entry);
    FtEntryFailed |= !original.ok;
    return ((int(__cdecl *)(int, int, int, int))0x00506650)(faction_id, faction_id_atk, x, y);
}
static int compute_score_exe(int faction_id, int *score_total, int *scores, int is_sub) {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x38 };
    OriginalEntry original(0x005ACBE0, entry);
    FtEntryFailed |= !original.ok;
    ((void(__cdecl *)(int, int *, int *, int))0x005ACBE0)(faction_id, score_total, scores, is_sub);
    return 0;
}
static int faction_upkeep_exe(int faction_id) {
    static const uint8_t entry[6] = { 0x55, 0x8B, 0xEC, 0x53, 0x56, 0x57 };
    OriginalEntry original(0x00527290, entry);
    FtEntryFailed |= !original.ok;
    ((void(__cdecl *)(int))0x00527290)(faction_id);
    return 0;
}

// first unit (from index start) that some unit is sentried or boarded on, or -1
static int ft_find_carrier(int start, BOOL at_sea) {
    for (int i = start; i < *VehCurrentCount; i++) {
        if (veh_cargo(i) && on_map(Vehs[i].x, Vehs[i].y) && is_ocean(Vehs[i].x, Vehs[i].y) == at_sea) {
            for (int j = 0; j < *VehCurrentCount; j++) {
                if (Vehs[j].order == ORDER_SENTRY_BOARD && Vehs[j].waypoint_x[0] == i) {
                    return i;
                }
            }
        }
    }
    return -1;
}

// Sample units for the veh_kill and kill tests: -1, every step-th, the last one, and units others
// are boarded on (at sea and on land) or that share a tile.
static int ft_sample_units(int *samples, int max, int step) {
    int n = 0;
    samples[n++] = -1;
    for (int i = 0; i < *VehCurrentCount && n < max - 20; i += step) {
        samples[n++] = i;
    }
    samples[n++] = *VehCurrentCount - 1;
    for (int k = 0, i = 0; k < 6 && (i = ft_find_carrier(i, k & 1)) >= 0; k++, i++) {
        samples[n++] = i;
    }
    for (int k = 0, i = 1; k < 6 && i < *VehCurrentCount; i += 37) {
        if (Vehs[i].next_veh_id_stack >= 0 || Vehs[i].prev_veh_id_stack >= 0) {
            samples[n++] = i;
            k++;
        }
    }
    return n;
}

/*
veh_kill() on sampled units, each with the unit ids it renumbers set equal to, above and below
the unit, and in a network game with NetVehIDRef pointing at a test value, at the excluded
globals or nowhere.
*/
static int test_veh_kill() {
    FtUiStubs stubs;
    if (!stubs.ok()) {
        logf("veh_kill: cannot stub the interface calls");
        return 1;
    }
    FtHarness *h = new FtHarness("veh_kill");
    static int samples[1024];
    int sample_count = ft_sample_units(samples, 1024, 3);
    const int count = *VehCurrentCount;
    int removed = 0;
    char label[96];
    for (int s = 0; s < sample_count; s++) {
        int veh_id = samples[s];
        for (int v = 0; v < 6; v++) {
            auto setup = [&] {
                if (v == 0) {
                    *MapWinVehIDSelected = *MapWinVehIDOther = veh_id;
                    *StatusWinVehIDA = *StatusWinVehIDB = *VehIDCurrent = veh_id;
                } else if (v == 1) {
                    *MapWinVehIDSelected = veh_id + 1;
                    *MapWinVehIDOther = veh_id - 1;
                    *StatusWinVehIDA = count - 1;
                    *StatusWinVehIDB = -1;
                    *VehIDCurrent = 0;
                } else {
                    *IsMultiplayerNet = true;
                    FtNetRef = veh_id + 2 * (veh_id & 1);
                    *MapWinVehIDSelected = veh_id + 2;
                    *MapWinVehIDOther = veh_id;
                    *StatusWinVehIDA = -1;
                    *StatusWinVehIDB = veh_id + 5;
                    *VehIDCurrent = veh_id + 1;
                    *NetVehIDRef = v == 2 ? &FtNetRef : (v == 3 ? MapWinVehIDSelected
                        : (v == 4 ? VehIDCurrent : NULL));
                }
            };
            sprintf_s(label, "veh_kill(%d) variant %d", veh_id, v);
            h->run(label, (uint32_t)(veh_id * 7 + v), setup, [&] {
                int r = veh_kill_exe(veh_id);
                removed += *VehCurrentCount < count;
                return r;
            }, [&] { veh_kill(veh_id); return 0; });
        }
    }
    logf("veh_kill: %d compared (%d units removed by the original, %d changed state), %d mismatches",
        h->compared, removed, h->changed, h->mismatches);
    int result = h->report();
    delete h;
    return result;
}

/*
kill() on sampled units (transports at sea and on land with units aboard among them) with no
redraw, with the redraw for the unit's own faction at the controls and for another; transports
are also killed off the map at x -2.
*/
static int test_unit_kill() {
    FtUiStubs stubs;
    if (!stubs.ok()) {
        logf("unit_kill: cannot stub the interface calls");
        return 1;
    }
    FtHarness *h = new FtHarness("unit_kill");
    static int samples[1024];
    int sample_count = ft_sample_units(samples, 1024, 3);
    const int count = *VehCurrentCount;
    int removed = 0, stacks = 0, radius = 0;
    char label[96];
    for (int s = 1; s < sample_count; s++) { // kill() has no check for -1
        int veh_id = samples[s];
        BOOL cargo = veh_cargo(veh_id) != 0;
        for (int v = 0; v < (cargo ? 4 : 3); v++) {
            auto setup = [&] {
                VehBattleUnkTgl[0] = v == 0;
                uint32_t faction_id = Vehs[veh_id].faction_id;
                *FactionIDCurrent = v == 2 ? (int)(faction_id % 7 + 1) : (int)faction_id;
                if (v == 3) {
                    veh_lift(veh_id);
                    Vehs[veh_id].x = -2;
                    Vehs[veh_id].y = 0;
                }
            };
            sprintf_s(label, "kill(%d) variant %d", veh_id, v);
            h->run(label, (uint32_t)(veh_id * 5 + v), setup, [&] {
                int r = kill_exe(veh_id);
                removed += count - *VehCurrentCount;
                stacks += count - *VehCurrentCount > 1;
                for (int i = 0; i < FtCallCount && i < MaxFtCalls; i++) {
                    radius += FtCalls[i].fn == FT_DRAW_RADIUS;
                }
                return r;
            }, [&] { kill(veh_id); return 0; });
        }
    }
    logf("unit_kill: %d compared (by the original: %d units removed, %d with their cargo, %d radius "
        "redraws), %d mismatches", h->compared, removed, stacks, radius, h->mismatches);
    int result = h->report();
    delete h;
    return result;
}

// an adjacent tile with no base and no airbase, or false
static BOOL ft_open_tile(int x, int y, int *x_out, int *y_out) {
    for (int dir = 0; dir < 8; dir++) {
        int x2 = xrange(x + RadiusBaseX[dir]);
        int y2 = y + RadiusBaseY[dir];
        if (on_map(x2, y2) && !(bit_at(x2, y2) & (BIT_BASE_IN_TILE | BIT_AIRBASE))) {
            *x_out = x2;
            *y_out = y2;
            return true;
        }
    }
    return false;
}

/*
mash_planes() with each faction at the controls; with every air unit rested and undamaged; with
copters damaged so the 30% loss is fatal; with every other unit out of moves; and with every
third air unit in a base moved out to a free neighbouring tile.
*/
static int test_mash_planes() {
    FtUiStubs stubs;
    if (!stubs.ok()) {
        logf("mash_planes: cannot stub the interface calls");
        return 1;
    }
    FtHarness *h = new FtHarness("mash_planes");
    const int count = *VehCurrentCount;
    int air = 0, removed = 0, damaged = 0;
    for (int i = 0; i < count; i++) {
        air += Chassis[VehPrototypes[Vehs[i].proto_id].chassis_id].triad == TRIAD_AIR;
    }
    char label[96];
    for (int v = 0; v < 13; v++) {
        auto setup = [&] {
            *FactionIDCurrent = v < 8 ? v : 1 + v % 7;
            for (int i = 0, k = 0; i < count; i++) {
                VehPrototype *proto = &VehPrototypes[Vehs[i].proto_id];
                if (Chassis[proto->chassis_id].triad != TRIAD_AIR) {
                    continue;
                }
                k++;
                if (v == 8) {
                    Vehs[i].moves_expended = 0;
                    Vehs[i].dmg_incurred = 0;
                } else if (v == 9 && proto->chassis_id == CHSI_COPTER) {
                    Vehs[i].dmg_incurred = (uint8_t)(proto_power(i) - proto->reactor_id * 3);
                } else if (v == 10 && k & 1) {
                    Vehs[i].moves_expended = (uint8_t)speed(i, false);
                } else if (v >= 11 && k % 3 == v - 11 && base_at(Vehs[i].x, Vehs[i].y) >= 0) {
                    int x2, y2;
                    if (ft_open_tile(Vehs[i].x, Vehs[i].y, &x2, &y2)) {
                        veh_drop(veh_lift(i), x2, y2);
                    }
                }
            }
        };
        sprintf_s(label, "mash_planes() variant %d", v);
        h->run(label, (uint32_t)(v * 13 + 1), setup, [&] {
            int marked = 0; // survivors newly grounded (VSTATE_UNK_4)
            for (int i = 0; i < *VehCurrentCount; i++) {
                marked -= (Vehs[i].state & VSTATE_UNK_4) != 0;
            }
            int before = *VehCurrentCount;
            int r = mash_planes_exe();
            removed += before - *VehCurrentCount;
            for (int i = 0; i < *VehCurrentCount; i++) {
                marked += (Vehs[i].state & VSTATE_UNK_4) != 0;
            }
            damaged += marked > 0 ? marked : 0;
            return r;
        }, [&] { mash_planes(); return 0; });
    }
    logf("mash_planes: %d compared (%d air units; by the original: %d killed, %d damaged and left, %d "
        "with interface calls), %d mismatches", h->compared, air, removed, damaged, h->with_calls,
        h->mismatches);
    int result = h->report();
    delete h;
    return result;
}

/*
interceptor() for each faction around up to four of its air superiority units: targets on the
unit's tile and out to three tiles away, against the next faction, with either side or neither at
the controls (the announcement and the step by step flight).
*/
static int test_interceptor() {
    FtUiStubs stubs;
    if (!stubs.ok()) {
        logf("interceptor: cannot stub the interface calls");
        return 1;
    }
    FtHarness *h = new FtHarness("interceptor");
    static const int offsets[] = { 0, 1, 4, 9, 13, 21, 25, 30, 46 };
    int scrambled = 0, flights = 0;
    char label[96];
    for (int f = 1; f < MaxPlayerNum; f++) {
        int found = 0;
        for (int i = 0; i < *VehCurrentCount && found < 4; i++) {
            int proto_id = Vehs[i].proto_id;
            if (Vehs[i].faction_id != f || Chassis[VehPrototypes[proto_id].chassis_id].triad != TRIAD_AIR
                || !has_abil(proto_id, ABL_AIR_SUPERIORITY) || !on_map(Vehs[i].x, Vehs[i].y)) {
                continue;
            }
            found++;
            int atk = f % 7 + 1;
            for (int k = 0; k < (int)(sizeof(offsets) / sizeof(offsets[0])); k++) {
                int x = xrange(Vehs[i].x + RadiusOffsetX[offsets[k]]);
                int y = Vehs[i].y + RadiusOffsetY[offsets[k]];
                if (!on_map(x, y)) {
                    continue;
                }
                int viewer = (k + found) % 3;
                for (int repaired = 0; repaired < 2; repaired++) {
                    auto setup = [&] {
                        *FactionIDCurrent = viewer == 0 ? f : (viewer == 1 ? atk : 0);
                        // all of the faction's units at full health: equal candidates tie
                        for (int j = 0; repaired && j < *VehCurrentCount; j++) {
                            if (Vehs[j].faction_id == f) {
                                Vehs[j].dmg_incurred = 0;
                            }
                        }
                    };
                    sprintf_s(label, "interceptor(%d, %d, %d, %d) viewer %d repaired %d", f, atk, x, y,
                        viewer, repaired);
                    h->run(label, (uint32_t)(i * 11 + k), setup, [&] {
                        int r = interceptor_exe(f, atk, x, y);
                        scrambled += r != 0;
                        for (int c = 0; c < FtCallCount && c < MaxFtCalls; c++) {
                            flights += FtCalls[c].fn == FT_VEH_SCOOT;
                        }
                        return r;
                    }, [&] { return (int)interceptor(f, atk, x, y); });
                }
            }
        }
    }
    logf("interceptor: %d compared (by the original: %d scrambles, %d flight steps), %d mismatches",
        h->compared, scrambled, flights, h->mismatches);
    int result = h->report();
    delete h;
    return result;
}

/*
compute_score() for every faction: into its own fields, into the caller's arrays and as a sub
score; under STATE_UNK_8; with the cooperative and conquest rules, conquest under way; with the
victory flags on the faction and on a pact partner; with the Ascent to Transcendence built; with
no objective target, a low one and both targets out of reach; other native life levels in a
scenario; "highest score wins" (every faction scored); Iron Man, cheated or not; and the faction
dead.
*/
static int test_compute_score() {
    FtHarness *h = new FtHarness("compute_score");
    // a base of a living faction with citizens, to hold the Ascent (its owner's population is
    // the divisor's first term)
    int ascent_base = -1;
    for (int i = 0; i < *BaseCurrentCount && ascent_base < 0; i++) {
        int owner = Bases[i].faction_id_current;
        if (owner > 0 && PlayersData[owner].pop_total > 0) {
            ascent_base = i;
        }
    }
    int recorded = 0;
    char label[96];
    for (int f = 0; f < MaxPlayerNum; f++) {
        for (int v = 0; v < 21; v++) {
            if (v == 12 && ascent_base < 0) {
                continue;
            }
            int partner = f % 7 + 1;
            auto setup = [&] {
                if (v == 3 || v == 4) {
                    *GameState |= STATE_UNK_8;
                } else if (v == 5) {
                    *GameRules ^= RULES_VICTORY_COOPERATIVE;
                } else if (v == 6 || v == 7) {
                    *GameRules |= RULES_VICTORY_CONQUEST;
                    *GameState = v == 7 ? *GameState | STATE_VICTORY_CONQUER
                        : *GameState & ~STATE_VICTORY_CONQUER;
                } else if (v == 8) {
                    PlayersData[f].flags |= PFLAG_UNK_20000;
                } else if (v == 9) {
                    PlayersData[f].flags |= PFLAG_UNK_40000;
                    *GameRules |= RULES_VICTORY_COOPERATIVE;
                } else if (v == 10) {
                    PlayersData[f].flags |= PFLAG_UNK_100000;
                } else if (v == 11) {
                    PlayersData[partner].flags |= PFLAG_UNK_20000;
                    *GameRules |= RULES_VICTORY_COOPERATIVE;
                    PlayersData[f].diplo_treaties[partner] |= DTREATY_PACT;
                    PlayersData[partner].diplo_treaties[f] |= DTREATY_PACT;
                } else if (v == 12) {
                    SecretProjectTable[SP_ASCENT_TO_TRANSCENDENCE] = ascent_base;
                } else if (v == 13) {
                    *ObjectiveReqVictory = 0;
                } else if (v == 14) {
                    *ObjectiveReqVictory = 1;
                } else if (v == 15) {
                    *ObjectiveReqVictory = 9999;
                    *ObjectivesSuddenDeathVictory = 9999;
                } else if (v == 16) {
                    *MapNativeLifeForms = 0;
                } else if (v == 17) {
                    *MapNativeLifeForms = 2;
                    *GameState |= STATE_IS_SCENARIO;
                } else if (v == 18) {
                    *GameState |= STATE_SCN_VICT_HIGHEST_AC_SCORE_WINS;
                } else if (v == 19) {
                    *GameRules |= RULES_IRONMAN;
                    *GameState = (f & 1) ? *GameState | STATE_SCENARIO_CHEATED_FLAG
                        : *GameState & ~STATE_SCENARIO_CHEATED_FLAG;
                } else if (v == 20) {
                    FactionsStatus[1] &= ~(1 << f);
                }
            };
            // 1, 2, 4: the caller's arrays (2 and 4 as a sub score); the others the faction's fields
            BOOL own = v == 1 || v == 2 || v == 4;
            BOOL sub = v == 2 || v == 4;
            sprintf_s(label, "compute_score(%d) variant %d", f, v);
            h->run(label, (uint32_t)(f * 23 + v), setup, [&] {
                compute_score_exe(f, own ? &FtOut[0] : NULL, own ? &FtOut[1] : NULL, sub);
                recorded += PlayersData[f].unk_107 != h->base.players[f].unk_107;
                return 0;
            }, [&] {
                compute_score(f, own ? &FtOut[0] : NULL, own ? &FtOut[1] : NULL, sub);
                return 0;
            });
        }
    }
    logf("compute_score: %d compared (Ascent base %d; %d recorded scores changed by the original), "
        "%d mismatches", h->compared, ascent_base, recorded, h->mismatches);
    int result = h->report();
    delete h;
    return result;
}

/*
faction_upkeep() for every faction with its phases and dialogs recorded instead of run: as
loaded; at the controls with the council not yet announced and autosave on; between network
turns (STATE_UNK_1, with and without STATE_UNK_8); a computer player able to corner the Global
Energy Market; no bases or pods left; hurry costs above the reserves; the council already
announced; a network game with autosave on.
*/
static int test_faction_upkeep() {
    FtTurnStubs stubs;
    if (!stubs.ok()) {
        logf("faction_upkeep: cannot stub the turn phases");
        return 1;
    }
    FtHarness *h = new FtHarness("faction_upkeep");
    int corner = 0, eliminated = 0, councils = 0, saves = 0;
    char label[96];
    for (int f = 0; f < MaxPlayerNum; f++) {
        for (int v = 0; v < 9; v++) {
            auto setup = [&] {
                PlayerData *p = &PlayersData[f];
                if (v == 1 || v == 7 || v == 8) {
                    *FactionIDCurrent = f;
                    *GamePreferences |= PREF_BSC_AUTOSAVE_EACH_TURN;
                    *GameState = v == 7 ? *GameState | STATE_DISPLAYED_COUNCIL_AVAIL_MSG
                        : *GameState & ~(STATE_DISPLAYED_COUNCIL_AVAIL_MSG | STATE_COUNCIL_HAS_CONVENED);
                    *IsMultiplayerNet = v == 8;
                } else if (v == 2) {
                    *GameState = (*GameState | STATE_UNK_1) & ~STATE_UNK_8;
                } else if (v == 3) {
                    *GameState |= STATE_UNK_1 | STATE_UNK_8;
                } else if (v == 4) {
                    FactionsStatus[0] &= ~(1 << f);
                    p->energy_reserves = 0x40000000;
                    *GameRules |= RULES_VICTORY_ECONOMIC;
                    int tech = Rules->tech_economic_victory;
                    if (tech >= 0 && tech < MaxTechnologyNum) {
                        GameTechAchieved[tech] |= 1 << f;
                    }
                } else if (v == 5) {
                    p->current_num_bases = 0;
                    p->proto_id_active[BSC_COLONY_POD] = 0;
                    p->proto_id_active[BSC_SEA_ESCAPE_POD] = 0;
                } else if (v == 6) {
                    p->hurry_cost_total = p->energy_reserves + 50;
                }
            };
            sprintf_s(label, "faction_upkeep(%d) variant %d", f, v);
            h->run(label, (uint32_t)(f * 19 + v), setup, [&] {
                int r = faction_upkeep_exe(f);
                for (int i = 0; i < FtCallCount && i < MaxFtCalls; i++) {
                    corner += FtCalls[i].fn == FT_POPP && strstr(FtCalls[i].text, "CORNERWARNING");
                    eliminated += FtCalls[i].fn == FT_ELIMINATE;
                    councils += FtCalls[i].fn == FT_CALL_COUNCIL
                        || (FtCalls[i].fn == FT_POPP && strstr(FtCalls[i].text, "COUNCILOPEN"));
                    saves += FtCalls[i].fn == FT_AUTO_SAVE;
                }
                return r;
            }, [&] { faction_upkeep(f); return 0; });
        }
    }
    logf("faction_upkeep: %d compared (by the original: %d market corners, %d eliminations, %d "
        "council calls or offers, %d autosaves), %d mismatches", h->compared, corner, eliminated,
        councils, saves, h->mismatches);
    int result = h->report();
    delete h;
    return result;
}

// ---------------------------------------------------------------- batch 31: base turn

// The base turn draws, opens popups, plays animations, centers the map and opens the base window:
// record those calls instead. tech_achieved() (research can end in its dialogs) is recorded too,
// and base_production() in the variants that stub it: it has its own dialogs and movies for the
// player at the controls, so it only runs for real for bases of the other factions.
enum TurnFn {
    TF_POPP, TF_POPB, TF_POP2, TF_INTERLUDE, TF_BOOM, TF_DRAW_TILE, TF_DRAW_TILES, TF_DRAW_MAP,
    TF_SET_CENTER, TF_FOCUS, TF_CLOCK_WAIT, TF_ZOOM, TF_REDRAW, TF_DRAW_RADIUS, TF_TECH_ACHIEVED,
    TF_PRODUCTION, TF_TECH_SELECTION, TF_COUNT
};
static const char *const TurnFnName[TF_COUNT] = { "popp", "popb", "POP2", "interlude", "boom",
    "draw_tile", "draw_tiles", "draw_map", "MapWin::set_center", "Console::focus", "clock_wait",
    "BaseWin::zoom", "BaseWin::on_redraw", "draw_radius", "tech_achieved", "base_production", "tech_selection" };
struct TurnCall {
    int fn;
    char label[24];
    char pic[20];
    char file[8];
    int args[4];
};
const int MaxTurnCalls = 64;
static TurnCall TurnCalls[MaxTurnCalls];
static int TurnCallCount;
static int TurnFocusAnswer; // Console::focus(): non-zero when the map view moved
static BOOL TurnZoomAnswer; // popb() sets BaseUpkeepZoom, as when the player asks to see the base

static void turn_record(int fn, LPCSTR label, LPCSTR pic, LPCSTR file, int a0, int a1, int a2, int a3) {
    if (TurnCallCount < MaxTurnCalls) {
        TurnCall *call = &TurnCalls[TurnCallCount];
        memset(call, 0, sizeof(*call));
        call->fn = fn;
        strncpy_s(call->label, label ? label : "", _TRUNCATE);
        strncpy_s(call->pic, pic ? pic : "", _TRUNCATE);
        strncpy_s(call->file, file ? file : "", _TRUNCATE);
        call->args[0] = a0;
        call->args[1] = a1;
        call->args[2] = a2;
        call->args[3] = a3;
    }
    TurnCallCount++;
}
static int __cdecl turn_popp(LPCSTR file, LPCSTR label, int unk, LPCSTR pic, int(__cdecl *callback)()) {
    turn_record(TF_POPP, label, pic, file, unk, callback != NULL, 0, 0);
    return 0;
}
static int __cdecl turn_popb(LPCSTR label, int flags, int sound, LPCSTR pic, Sprite *sprite) {
    turn_record(TF_POPB, label, pic, NULL, flags, sound, sprite != NULL, 0);
    if (TurnZoomAnswer) {
        *BaseUpkeepZoom = true;
    }
    return 0;
}
static int __cdecl turn_pop2(LPCSTR label, LPCSTR pcx, int unk) {
    turn_record(TF_POP2, label, pcx, NULL, unk, 0, 0, 0);
    return 0;
}
static void __cdecl turn_interlude(int event_id, LPCSTR text, int unk1, int unk2) {
    turn_record(TF_INTERLUDE, text, NULL, NULL, event_id, unk1, unk2, 0);
}
static void __cdecl turn_boom(int x, int y, int flags) {
    turn_record(TF_BOOM, NULL, NULL, NULL, x, y, flags, 0);
}
static void __cdecl turn_draw_tile(int x, int y, int mode) {
    turn_record(TF_DRAW_TILE, NULL, NULL, NULL, x, y, mode, 0);
}
static void __cdecl turn_draw_tiles(int x, int y, int mode) {
    turn_record(TF_DRAW_TILES, NULL, NULL, NULL, x, y, mode, 0);
}
static void __cdecl turn_draw_map(int mode) {
    turn_record(TF_DRAW_MAP, NULL, NULL, NULL, mode, 0, 0, 0);
}
static void __fastcall turn_set_center(void *This, void *, int x, int y, int unk) {
    turn_record(TF_SET_CENTER, NULL, NULL, NULL, This == *MapWinList, x, y, unk);
}
static int __fastcall turn_focus(void *This, void *, int x, int y, int faction_id) {
    turn_record(TF_FOCUS, NULL, NULL, NULL, This == ConsoleVar, x, y, faction_id);
    return TurnFocusAnswer;
}
static void __cdecl turn_clock_wait(int ms) {
    turn_record(TF_CLOCK_WAIT, NULL, NULL, NULL, ms, 0, 0, 0);
}
static void __fastcall turn_zoom(void *This, void *, int base_id, int unk) {
    turn_record(TF_ZOOM, NULL, NULL, NULL, This == BaseWinVar, base_id, unk, 0);
}
static void __fastcall turn_redraw(void *This, void *) {
    turn_record(TF_REDRAW, NULL, NULL, NULL, This == BaseWinVar, 0, 0, 0);
}
static void __cdecl turn_draw_radius(int x, int y, int unk1, int unk2) {
    turn_record(TF_DRAW_RADIUS, NULL, NULL, NULL, x, y, unk1, unk2);
}
static void __cdecl turn_tech_achieved(int faction_id, int tech_id, int unk1, int unk2) {
    turn_record(TF_TECH_ACHIEVED, NULL, NULL, NULL, faction_id, tech_id, unk1, unk2);
}
static int __cdecl turn_tech_selection(int faction_id) {
    turn_record(TF_TECH_SELECTION, NULL, NULL, NULL, faction_id, 0, 0, 0);
    return -1;
}
static void __cdecl turn_production() {
    turn_record(TF_PRODUCTION, NULL, NULL, NULL, *BaseIDCurrentSelected, (int)*BaseCurrent, 0, 0);
}

struct TurnStubs {
    JmpStub popp_stub, popb_stub, pop2_stub, interlude_stub, boom_stub, draw_tile_stub,
        draw_tiles_stub, draw_map_stub, set_center_stub, focus_stub, clock_wait_stub, zoom_stub,
        redraw_stub, draw_radius_stub, tech_achieved_stub, tech_selection_stub;
    TurnStubs()
        : popp_stub(0x0048C0A0, (void *)turn_popp),
          popb_stub(0x0048C650, (void *)turn_popb),
          pop2_stub(0x00405140, (void *)turn_pop2),
          interlude_stub(0x005230E0, (void *)turn_interlude),
          boom_stub(0x00504AA0, (void *)turn_boom),
          draw_tile_stub(0x0046AF40, (void *)turn_draw_tile),
          draw_tiles_stub(0x0046B140, (void *)turn_draw_tiles),
          draw_map_stub(0x0046B190, (void *)turn_draw_map),
          set_center_stub(0x0046B1F0, (void *)turn_set_center),
          focus_stub(0x005108A0, (void *)turn_focus),
          clock_wait_stub(0x0050DD90, (void *)turn_clock_wait),
          zoom_stub(0x0041A920, (void *)turn_zoom),
          redraw_stub(0x0041E790, (void *)turn_redraw),
          draw_radius_stub(0x0046AEF0, (void *)turn_draw_radius),
          tech_achieved_stub(0x005BB000, (void *)turn_tech_achieved),
          tech_selection_stub((uint32_t)(void *)tech_selection, (void *)turn_tech_selection) {} // DLL code since batch 30
    BOOL ok() const {
        return popp_stub.ok && popb_stub.ok && pop2_stub.ok && interlude_stub.ok && boom_stub.ok
            && draw_tile_stub.ok && draw_tiles_stub.ok && draw_map_stub.ok && set_center_stub.ok
            && focus_stub.ok && clock_wait_stub.ok && zoom_stub.ok && redraw_stub.ok
            && draw_radius_stub.ok && tech_achieved_stub.ok && tech_selection_stub.ok;
    }
};


// What the base turn can change besides the game state in GameSnapshot.
struct TurnState {
    NoticeState notice; // parse slots and defaults, stringTemp, pronouns, diplomacy globals, seed
    uint8_t globals[BaseGlobalsSize]; // BaseUpkeepStage, BaseUpkeepZoom, BaseFindDist, ...
    int sp[MaxSecretProjectNum];
    Base *current;
    int selected;
    int compute;
    int sea_level;
    int sea_level_council;
    int climate[4]; // ClimateValueA, B, C, ClimateFutureChange
    int warming;
    int credits;
    int skip_tech;
    int parse_num[10];
    int replay_end;
    uint8_t replay[0x400]; // what was appended to the replay record
    int rand_next; // the exe's rand() after the call
    int ret;
    TurnCall calls[MaxTurnCalls];
    int call_count;
};

static void turn_take(TurnState *s, int replay_from, int ret, int rand_next) {
    notice_take(&s->notice, 0);
    memcpy(s->globals, BaseGlobals, BaseGlobalsSize);
    memcpy(s->sp, SecretProjectTable, sizeof(s->sp));
    s->current = *BaseCurrent;
    s->selected = *BaseIDCurrentSelected;
    s->compute = *BaseComputeID;
    s->sea_level = *MapSeaLevel;
    s->sea_level_council = *MapSeaLevelCouncil;
    s->climate[0] = *ClimateValueA;
    s->climate[1] = *ClimateValueB;
    s->climate[2] = *ClimateValueC;
    s->climate[3] = *ClimateFutureChange;
    s->warming = *GlobalWarmingCount;
    s->credits = *EnergyCredits;
    s->skip_tech = *SkipTechScreenA;
    memcpy(s->parse_num, ParseNumTable, sizeof(s->parse_num));
    s->replay_end = *ReplayOffset;
    memset(s->replay, 0, sizeof(s->replay));
    int len = *ReplayOffset - replay_from;
    if (len > 0) {
        memcpy(s->replay, ReplayRecord + replay_from, len < (int)sizeof(s->replay) ? len : sizeof(s->replay));
    }
    s->rand_next = rand_next;
    s->ret = ret;
    memcpy(s->calls, TurnCalls, sizeof(s->calls));
    s->call_count = TurnCallCount;
}

static void turn_restore(const TurnState *s) {
    notice_restore(&s->notice);
    memcpy(BaseGlobals, s->globals, BaseGlobalsSize);
    memcpy(SecretProjectTable, s->sp, sizeof(s->sp));
    *BaseCurrent = s->current;
    *BaseIDCurrentSelected = s->selected;
    *BaseComputeID = s->compute;
    *MapSeaLevel = s->sea_level;
    *MapSeaLevelCouncil = s->sea_level_council;
    *ClimateValueA = s->climate[0];
    *ClimateValueB = s->climate[1];
    *ClimateValueC = s->climate[2];
    *ClimateFutureChange = s->climate[3];
    *GlobalWarmingCount = s->warming;
    *EnergyCredits = s->credits;
    *SkipTechScreenA = s->skip_tech;
    memcpy(ParseNumTable, s->parse_num, sizeof(s->parse_num));
    *ReplayOffset = s->replay_end;
}

static BOOL turn_differs(const TurnState *o, const TurnState *n, const char *label) {
#define DLOG(...) do { if (label) logf(__VA_ARGS__); } while (0)
    if (o->ret != n->ret || o->call_count != n->call_count) {
        DLOG("  %s: orig returned %d after %d calls, new %d after %d calls", label, o->ret,
            o->call_count, n->ret, n->call_count);
        for (int i = 0; label && i < 12 && (i < o->call_count || i < n->call_count) && i < MaxTurnCalls; i++) {
            const TurnCall *a = &o->calls[i], *b = &n->calls[i];
            logf("    %d: orig %s('%s' %d %d %d %d) new %s('%s' %d %d %d %d)", i,
                i < o->call_count ? TurnFnName[a->fn] : "-", a->label, a->args[0], a->args[1], a->args[2],
                a->args[3], i < n->call_count ? TurnFnName[b->fn] : "-", b->label, b->args[0], b->args[1],
                b->args[2], b->args[3]);
        }
        return true;
    }
    for (int i = 0; i < o->call_count && i < MaxTurnCalls; i++) {
        const TurnCall *a = &o->calls[i], *b = &n->calls[i];
        if (a->fn != b->fn || strcmp(a->label, b->label) || strcmp(a->pic, b->pic)
            || strcmp(a->file, b->file) || memcmp(a->args, b->args, sizeof(a->args))) {
            DLOG("  %s: call %d: orig %s('%s' '%s' %d %d %d %d) new %s('%s' '%s' %d %d %d %d)", label, i,
                TurnFnName[a->fn], a->label, a->pic, a->args[0], a->args[1], a->args[2], a->args[3],
                TurnFnName[b->fn], b->label, b->pic, b->args[0], b->args[1], b->args[2], b->args[3]);
            return true;
        }
    }
    if (notice_differs(&o->notice, &n->notice, label) || base_globals_differ(o->globals, n->globals, label)) {
        return true;
    }
    if (memcmp(o->sp, n->sp, sizeof(o->sp)) || o->current != n->current || o->selected != n->selected
        || o->compute != n->compute || o->rand_next != n->rand_next) {
        DLOG("  %s: current base %p/%p, selected %d/%d, computed %d/%d, rand %d/%d (or the SP table)",
            label, (void *)o->current, (void *)n->current, o->selected, n->selected, o->compute,
            n->compute, o->rand_next, n->rand_next);
        return true;
    }
    if (o->sea_level != n->sea_level || o->sea_level_council != n->sea_level_council
        || memcmp(o->climate, n->climate, sizeof(o->climate)) || o->warming != n->warming) {
        DLOG("  %s: sea level %d/%d, council %d/%d, sea change %d/%d (rate %d/%d), warming count %d/%d",
            label, o->sea_level, n->sea_level, o->sea_level_council, n->sea_level_council, o->climate[3],
            n->climate[3], o->climate[0], n->climate[0], o->warming, n->warming);
        return true;
    }
    if (o->credits != n->credits || o->skip_tech != n->skip_tech
        || memcmp(o->parse_num, n->parse_num, sizeof(o->parse_num))) {
        DLOG("  %s: credits shown %d/%d, skip tech screens %d/%d (or the parse numbers)", label,
            o->credits, n->credits, o->skip_tech, n->skip_tech);
        return true;
    }
    if (o->replay_end != n->replay_end || memcmp(o->replay, n->replay, sizeof(o->replay))) {
        DLOG("  %s: replay record end %d/%d (or its new entries)", label, o->replay_end, n->replay_end);
        return true;
    }
    return false;
#undef DLOG
}

// Run a base turn function as the exe and as the DLL from the same state and compare.
struct TurnHarness {
    GameSnapshot base, after_orig, after_new;
    TurnState saved, orig, now;
    Player players_saved[MaxPlayerNum];
    uint8_t status_saved[2];
    int diff_saved, current_saved, turn_saved, win_saved, replay_saved;
    uint32_t rules_saved, prefs_saved;
    BOOL net_saved, expansion_saved;
    int veh_count, spare;
    Veh *spare_saved, *spare_o;
    // prototypes (propose_proto(), the production AI) and monuments (the mon_* calls)
    VehPrototype *protos_saved, *protos_o;
    Monument monuments_saved[MaxPlayerNum], monuments_o[MaxPlayerNum];
    int compared, mismatches, with_calls, changed, born;
    BOOL time_set; // the clock reads time_value instead of a value from the seed
    DWORD time_value;
    TurnHarness() : compared(0), mismatches(0), with_calls(0), changed(0), born(0), time_set(false),
        time_value(0) {
        snap_alloc(&base); snap_alloc(&after_orig); snap_alloc(&after_new);
        snap_take(&base);
        replay_saved = *ReplayOffset;
        TurnCallCount = 0;
        turn_take(&saved, replay_saved, 0, 0);
        memcpy(players_saved, Players, sizeof(players_saved));
        memcpy(status_saved, FactionsStatus, sizeof(status_saved));
        diff_saved = *DiffLevelCurrent;
        current_saved = *FactionIDCurrent;
        turn_saved = *TurnCurrentNum;
        win_saved = *BaseWinBaseID;
        rules_saved = *GameRules;
        prefs_saved = *GamePreferences;
        net_saved = *IsMultiplayerNet;
        expansion_saved = *ExpansionEnabled;
        veh_count = *VehCurrentCount;
        spare = MaxVehNum - veh_count < 160 ? MaxVehNum - veh_count : 160;
        spare_saved = (Veh *)malloc(sizeof(Veh) * (spare > 0 ? spare : 1));
        spare_o = (Veh *)malloc(sizeof(Veh) * (spare > 0 ? spare : 1));
        memcpy(spare_saved, &Vehs[veh_count], sizeof(Veh) * spare);
        protos_saved = (VehPrototype *)malloc(sizeof(VehPrototype) * MaxVehProtoNum);
        protos_o = (VehPrototype *)malloc(sizeof(VehPrototype) * MaxVehProtoNum);
        memcpy(protos_saved, VehPrototypes, sizeof(VehPrototype) * MaxVehProtoNum);
        memcpy(monuments_saved, Monuments, sizeof(monuments_saved));
    }
    ~TurnHarness() {
        prepare();
        free(spare_saved);
        free(spare_o);
        free(protos_saved);
        free(protos_o);
        snap_free(&base); snap_free(&after_orig); snap_free(&after_new);
    }
    void prepare() {
        snap_restore(&base);
        memcpy(&Vehs[veh_count], spare_saved, sizeof(Veh) * spare);
        memcpy(VehPrototypes, protos_saved, sizeof(VehPrototype) * MaxVehProtoNum);
        memcpy(Monuments, monuments_saved, sizeof(monuments_saved));
        turn_restore(&saved);
        memcpy(Players, players_saved, sizeof(players_saved));
        memcpy(FactionsStatus, status_saved, sizeof(status_saved));
        *DiffLevelCurrent = diff_saved;
        *FactionIDCurrent = current_saved;
        *TurnCurrentNum = turn_saved;
        *BaseWinBaseID = win_saved;
        *GameRules = rules_saved;
        *GamePreferences = prefs_saved;
        *IsMultiplayerNet = net_saved;
        *ExpansionEnabled = expansion_saved;
        *ReplayOffset = replay_saved;
        TurnCallCount = 0;
        TurnFocusAnswer = 0;
        TurnZoomAnswer = false;
    }
    // setup() puts the variant in place after each restore; exe() and dll() make the call.
    template <class S, class E, class D>
    void run(const char *label, uint32_t seed, S setup, E exe, D dll) {
        REGRESS_CTX("%s", label);
        prepare();
        setup();
        reseed(seed);
        FixedTimeValue = time_set ? time_value : seed * 2654435761u;
        REGRESS_CTX("%s (exe)", label);
        int ret_o = exe();
        int rand_o = _rand();
        snap_take(&after_orig);
        turn_take(&orig, replay_saved, ret_o, rand_o);
        memcpy(spare_o, &Vehs[veh_count], sizeof(Veh) * spare);
        memcpy(protos_o, VehPrototypes, sizeof(VehPrototype) * MaxVehProtoNum);
        memcpy(monuments_o, Monuments, sizeof(monuments_o));
        int count_o = *VehCurrentCount;
        prepare();
        setup();
        reseed(seed);
        FixedTimeValue = time_set ? time_value : seed * 2654435761u;
        REGRESS_CTX("%s (DLL)", label);
        int ret_n = dll();
        int rand_n = _rand();
        snap_take(&after_new);
        turn_take(&now, replay_saved, ret_n, rand_n);
        compared++;
        with_calls += orig.call_count > 0;
        born += count_o > veh_count ? count_o - veh_count : 0;
        changed += snap_differs(&base, &after_orig, NULL);
        const char *detail = mismatches < MAX_DETAIL ? label : NULL;
        BOOL diff = turn_differs(&orig, &now, detail) || snap_differs(&after_orig, &after_new, detail);
        size_t at;
        if (!diff && (at = first_diff(spare_o, &Vehs[veh_count], sizeof(Veh) * spare)) != (size_t)-1) {
            if (detail) {
                logf("  %s: new Veh %u +0x%X differs", label, (unsigned)(veh_count + at / sizeof(Veh)),
                    (unsigned)(at % sizeof(Veh)));
            }
            diff = true;
        }
        if (!diff && (at = first_diff(protos_o, VehPrototypes, sizeof(VehPrototype) * MaxVehProtoNum))
            != (size_t)-1) {
            if (detail) {
                logf("  %s: VehPrototype %u +0x%X differs", label, (unsigned)(at / sizeof(VehPrototype)),
                    (unsigned)(at % sizeof(VehPrototype)));
            }
            diff = true;
        }
        if (!diff && (at = first_diff(monuments_o, Monuments, sizeof(monuments_o))) != (size_t)-1) {
            if (detail) {
                logf("  %s: Monument %u +0x%X differs", label, (unsigned)(at / sizeof(Monument)),
                    (unsigned)(at % sizeof(Monument)));
            }
            diff = true;
        }
        if (diff) {
            mismatches++;
        }
    }
};

static const uint8_t DroneRiotEntry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x5C };
static const uint8_t BaseEcologyEntry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x3C };
static const uint8_t BaseUpkeepEntry[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C };
static const uint8_t ProductionPhaseEntry[6] = { 0x55, 0x8B, 0xEC, 0x51, 0x53, 0x8B };

// Call the original drone_riot with the seven stack slots it reads before writing them seeded
// (faction f at esp_entry - 0x60 + 4 * f).
static __declspec(naked) void __cdecl riot_seeded_call(void *UNUSED(entry), int *UNUSED(slots)) {
    __asm {
        mov edx, [esp + 8]                  // slots
        mov ecx, 7
    riot_seed:
        mov eax, [edx + ecx * 4]
        mov [esp + ecx * 4 - 0x64], eax     // the callee's esp_entry is esp - 4
        dec ecx
        jnz riot_seed
        mov eax, [esp + 4]
        call eax
        ret
    }
}

// a faction other than f that is alive, or 0
static int turn_other_faction(uint32_t f) {
    for (int k = 1; k < MaxPlayerNum; k++) {
        int g = (f + k - 1) % (MaxPlayerNum - 1) + 1;
        if (g != (int)f && is_alive(g)) {
            return g;
        }
    }
    return 0;
}

// give faction f the REVOLT bonus, or take every REVOLT bonus away (in Players, which the harness
// restores)
static void turn_add_revolt(uint32_t f, int chance) {
    Player *p = &Players[f];
    if (p->faction_bonus_count < 8) {
        p->faction_bonus_id[p->faction_bonus_count] = RULE_REVOLT;
        p->faction_bonus_val1[p->faction_bonus_count] = chance;
        p->faction_bonus_count++;
    }
}
static void turn_strip_revolt() {
    for (int f = 1; f < MaxPlayerNum; f++) {
        Player *p = &Players[f];
        int n = 0;
        for (int i = 0; i < p->faction_bonus_count; i++) {
            if (p->faction_bonus_id[i] != RULE_REVOLT) {
                p->faction_bonus_id[n] = p->faction_bonus_id[i];
                p->faction_bonus_val1[n] = p->faction_bonus_val1[i];
                p->faction_bonus_val2[n] = p->faction_bonus_val2[i];
                n++;
            }
        }
        p->faction_bonus_count = n;
    }
}

// Where a rioting base of faction f defects without a REVOLT faction: its former owner, else the
// faction with the nearest base, weighed by the treaty with that base's owner; with slip, by the
// treaty with the closest faction so far, as the original does.
static int riot_destination(int base_id, uint32_t f, BOOL slip) {
    Base *b = &Bases[base_id];
    if (b->faction_id_former != f) {
        return b->faction_id_former;
    }
    int to = -1;
    int best = 9999;
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (Bases[i].faction_id_current == f) {
            continue;
        }
        int dist = vector_dist(x_dist(Bases[i].x, b->x), Bases[i].y - b->y);
        uint32_t treaty = slip ? *(uint32_t *)((uint8_t *)PlayersData[f].diplo_treaties + to * 4)
            : PlayersData[f].diplo_treaties[Bases[i].faction_id_current];
        if (treaty & DTREATY_PACT) {
            dist *= 2;
        }
        if (treaty & DTREATY_TREATY) {
            dist = dist * 3 / 2;
        }
        if (treaty & DTREATY_TRUCE) {
            dist /= 2;
        }
        if (dist < best) {
            best = dist;
            to = Bases[i].faction_id_current;
        }
    }
    return to;
}

static BOOL riot_can_take(int faction_id) {
    return faction_id > 0 && faction_id < MaxPlayerNum && !(Players[faction_id].rule_flags & RFLAG_ALIEN);
}

static int test_drone_riot() {
    OriginalEntry vanilla(0x004F4FB0, DroneRiotEntry);
    TurnStubs stubs;
    TimeGetTimePin pin; // the REVOLT roll reads the clock
    if (!vanilla.ok || !stubs.ok() || !pin.ok()) {
        logf("drone_riot: cannot restore the original, stub the UI calls or pin timeGetTime");
        return 1;
    }
    // The original's slots seeded so that its REVOLT roll never fires (a factor of -1, the product
    // below 0): with no REVOLT faction the defections compared with it are the ones where the shipped
    // and the intended code agree.
    static const int pinned[8] = { 0, 200, -1, -1, -1, -1, -1, -1 };
    TurnHarness h;
    char label[96];
    int defected = 0, destroyed = 0;
    for (int base_id = 0; base_id < h.base.base_count; base_id++) {
        Base *b = &Bases[base_id];
        uint32_t f = b->faction_id_current;
        if (f < 1 || f >= MaxPlayerNum) {
            continue;
        }
        BOOL alien = (Players[f].rule_flags & RFLAG_ALIEN) != 0;
        int other = turn_other_faction(f);
        // 0 a riot breaking out, as loaded; 1 the same with a governor managing the citizens, the
        // player's base; 2 as an AI base; a riot going on in an AI base: 3 as loaded, 4 under police
        // -5 at size 6, 5 the same at size 2, 6 a headquarters; with nothing left to destroy under
        // police -4 in a human base at size 6 and no REVOLT faction: 7 a captured base going back to
        // its former owner, 8 the nearest faction with no treaties anywhere (the slip can't matter)
        for (int v = 0; v < 9; v++) {
            if (v >= 4 && base_id % 2) {
                continue;
            }
            if (v >= 7 && (alien || !other)) {
                continue;
            }
            int runs = v >= 7 ? 4 : 1;
            for (int r = 0; r < runs; r++) {
                int slots[8];
                auto setup = [&] {
                    *BaseCurrent = b;
                    *BaseIDCurrentSelected = base_id;
                    PlayerData *p = &PlayersData[f];
                    if (v <= 2) {
                        b->state &= ~BSTATE_DRONE_RIOTS_ACTIVE;
                    } else {
                        b->state |= BSTATE_DRONE_RIOTS_ACTIVE;
                    }
                    if (v >= 3 && v <= 6) {
                        FactionsStatus[0] &= (uint8_t)~(1 << f); // no defection: AI
                    }
                    if (v == 1) {
                        b->governor |= GOV_ACTIVE | GOV_MANAGES_CITIZENS_SPECS;
                        *FactionIDCurrent = f;
                        FactionsStatus[0] |= (uint8_t)(1 << f);
                    } else if (v == 2) {
                        FactionsStatus[0] &= (uint8_t)~(1 << f);
                    } else if (v == 4 || v == 5) {
                        p->soc_effect_pending.police = -5;
                        b->population_size = v == 4 ? 6 : 2;
                        b->assimilation_turns_left = 0;
                    } else if (v == 6) {
                        set_fac(FAC_HEADQUARTERS, base_id, true);
                        p->soc_effect_pending.police = -5;
                    } else if (v >= 7) {
                        turn_strip_revolt();
                        memset(b->facilities_built, 0, sizeof(b->facilities_built));
                        p->soc_effect_pending.police = -4;
                        b->population_size = 6;
                        b->assimilation_turns_left = 0;
                        if ((int)p->current_num_bases < 2) {
                            p->current_num_bases = 2;
                        }
                        FactionsStatus[0] |= (uint8_t)(1 << f);
                        *FactionIDCurrent = (r & 1) ? f : other;
                        b->faction_id_former = (uint8_t)(v == 7 ? other : f);
                        if (v == 8) {
                            memset(p->diplo_treaties, 0, sizeof(p->diplo_treaties));
                            p->last_turn_new_base &= ~(DTREATY_PACT | DTREATY_TREATY | DTREATY_TRUCE);
                        }
                    }
                };
                sprintf_s(label, "drone_riot() base %d variant %d run %d", base_id, v, r);
                uint32_t seed = (uint32_t)(base_id * 53 + v * 7 + r);
                h.run(label, seed, setup,
                    [&] { memcpy(slots, pinned, sizeof(slots)); riot_seeded_call((void *)0x004F4FB0, slots);
                        return 0; },
                    [&] { drone_riot(); return 0; });
                defected += h.after_orig.bases[base_id].faction_id_current != f;
                destroyed += memcmp(h.after_orig.bases[base_id].facilities_built,
                    h.base.bases[base_id].facilities_built, 12) != 0 && v < 7;
            }
        }
    }
    int compared = h.compared, mismatches = h.mismatches;
    // The intended defection (Kiousu's choice, 2026-10-18), checked against the rule itself: a base
    // that can't be held (police -6: the riot always goes on; nothing to destroy) of a human faction
    // over the clock's 100 last-two-digit values.
    auto defecting = [&](int base_id) {
        Base *b = &Bases[base_id];
        uint32_t f = b->faction_id_current;
        PlayerData *p = &PlayersData[f];
        *BaseCurrent = b;
        *BaseIDCurrentSelected = base_id;
        b->state |= BSTATE_DRONE_RIOTS_ACTIVE;
        memset(b->facilities_built, 0, sizeof(b->facilities_built));
        p->soc_effect_pending.police = -6;
        b->population_size = 6;
        b->assimilation_turns_left = 0;
        b->faction_id_former = (uint8_t)f;
        if ((int)p->current_num_bases < 2) {
            p->current_num_bases = 2;
        }
        FactionsStatus[0] |= (uint8_t)(1 << f);
    };
    int bases_checked = 0, picks_75 = 0, runs_75 = 0, revolt_none = 0, runs_none = 0;
    int picks_two = 0, runs_two = 0, wrong_pick = 0, slip_cases = 0, nearest_runs = 0, rule_bad = 0;
    int qualifying = 0;
    for (int base_id = 0; base_id < h.base.base_count && bases_checked < 8; base_id++) {
        uint32_t f = h.base.bases[base_id].faction_id_current;
        if (!riot_can_take(f)) {
            continue;
        }
        // two other factions that can take bases, neither the nearest destination
        h.prepare();
        turn_strip_revolt();
        defecting(base_id);
        int nearest = riot_destination(base_id, f, false);
        int g1 = 0, g2 = 0;
        for (int g = 1; g < MaxPlayerNum; g++) {
            if (g != (int)f && g != nearest && is_alive(g) && riot_can_take(g)) {
                if (!g1) {
                    g1 = g;
                } else if (!g2) {
                    g2 = g;
                }
            }
        }
        if (!riot_can_take(nearest) || !g1 || !g2 || qualifying++ % 10) {
            continue;
        }
        bases_checked++;
        for (int c = 0; c < 3; c++) {
            for (int d = 0; d < 100; d++) {
                h.prepare();
                turn_strip_revolt();
                if (c == 0) {
                    turn_add_revolt(g1, 75); // only one REVOLT faction at 75
                } else if (c == 2) {
                    turn_add_revolt(g1, 30); // two: 30 and 50, take 100 * (1 - 0.7 * 0.5) = 65
                    turn_add_revolt(g2, 50);
                }
                defecting(base_id);
                int k = 3 + 7 * d + 11 * base_id;
                FixedTimeValue = (DWORD)(100 * k + d);
                reseed((uint32_t)(base_id * 101 + c * 7 + d));
                REGRESS_CTX("drone_riot rule base %d case %d clock %d", base_id, c, d);
                drone_riot();
                int owner = Bases[base_id].faction_id_current;
                if (c == 0) {
                    runs_75++;
                    picks_75 += owner == g1;
                    rule_bad += owner != (d < 75 ? g1 : nearest);
                } else if (c == 1) {
                    runs_none++;
                    revolt_none += owner != nearest;
                    rule_bad += owner != nearest;
                } else {
                    runs_two++;
                    int expect = nearest;
                    if (d < 65) {
                        int roll = k % 80;
                        int first = g1 < g2 ? g1 : g2;
                        int first_chance = first == g1 ? 30 : 50;
                        expect = roll < first_chance ? first : (first == g1 ? g2 : g1);
                    }
                    picks_two += owner == g1 || owner == g2;
                    if (owner != expect) {
                        wrong_pick++;
                        rule_bad++;
                        if (rule_bad <= MAX_DETAIL) {
                            logf("  drone_riot rule: base %d two REVOLT factions clock %d: owner %d, expected %d",
                                base_id, 100 * k + d, owner, expect);
                        }
                    }
                }
            }
        }
    }
    // the nearest faction weighed by the treaty with each base's own owner: pacts, treaties and
    // truces spread over the factions, and the word before the treaties with the pact bit
    for (int base_id = 0; base_id < h.base.base_count; base_id += 2) {
        uint32_t f = h.base.bases[base_id].faction_id_current;
        if (!riot_can_take(f)) {
            continue;
        }
        h.prepare();
        turn_strip_revolt();
        defecting(base_id);
        PlayerData *p = &PlayersData[f];
        for (int g = 1; g < MaxPlayerNum; g++) {
            if (g != (int)f) {
                p->diplo_treaties[g] = (g % 3 == 0) ? DTREATY_PACT : (g % 3 == 1) ? DTREATY_TREATY : DTREATY_TRUCE;
            }
        }
        p->last_turn_new_base = DTREATY_PACT;
        int intended = riot_destination(base_id, f, false);
        int slipped = riot_destination(base_id, f, true);
        FixedTimeValue = 1000 + base_id;
        reseed((uint32_t)(base_id * 17));
        REGRESS_CTX("drone_riot nearest base %d", base_id);
        drone_riot();
        int expect = riot_can_take(intended) ? intended : (int)f;
        nearest_runs++;
        slip_cases += intended != slipped;
        if ((int)Bases[base_id].faction_id_current != expect) {
            rule_bad++;
            if (rule_bad <= MAX_DETAIL) {
                logf("  drone_riot rule: base %d nearest: owner %d, expected %d (the slip: %d)", base_id,
                    Bases[base_id].faction_id_current, expect, slipped);
            }
        }
    }
    h.prepare();
    logf("drone_riot: %d compared with the original (by the original: %d facilities destroyed, %d bases "
        "defected; %d with a call out), %d mismatches; the intended defection on %d bases: REVOLT 75 alone "
        "picked in %d of %d clock values, no REVOLT faction %d of %d, two at 30/50 %d of %d (%d wrong "
        "faction); nearest faction %d bases (%d where the original's slip picks another), %d off the rule",
        compared, destroyed, defected, h.with_calls, mismatches, bases_checked, picks_75, runs_75,
        revolt_none, runs_none, picks_two, runs_two, wrong_pick, nearest_runs, slip_cases, rule_bad);
    return mismatches + rule_bad + (picks_75 != 75 * bases_checked) + (revolt_none != 0)
        + (picks_two != 65 * bases_checked) + (bases_checked == 0);
}

static int test_base_ecology() {
    OriginalEntry vanilla(0x004F67F0, BaseEcologyEntry);
    TurnStubs stubs;
    TimeGetTimePin pin; // world_climate() after a volcano reseeds from the clock
    if (!vanilla.ok || !stubs.ok() || !pin.ok()) {
        logf("base_ecology: cannot restore the original, stub the UI calls or pin timeGetTime");
        return 1;
    }
    TurnHarness h;
    char label[96];
    int volcanoes = 0, warmings = 0;
    for (int base_id = 0; base_id < h.base.base_count; base_id++) {
        Base *b = &Bases[base_id];
        uint32_t f = b->faction_id_current;
        if (f < 1 || f >= MaxPlayerNum) {
            continue;
        }
        // 0 as loaded; with 100 damage (a bloom for sure): 1 the faction's blooms as loaded; 2 at
        // 3 blooms (a sea bloom can spread); 3 at 7, the player's human base (native life, the
        // messages, global warming due); 4 at 9, AI on the hardest level, global warming due; 5 at
        // 12 (a volcano can erupt); 6 at perihelion with the skip flag set; 7 native life off;
        // 8 the Voice of Planet built
        for (int v = 0; v < 9; v++) {
            if ((v == 5 && base_id % 4) || (v >= 6 && base_id % 2)) {
                continue;
            }
            for (int r = 0; r < 2; r++) {
                auto setup = [&] {
                    *BaseCurrent = b;
                    *BaseIDCurrentSelected = base_id;
                    PlayerData *p = &PlayersData[f];
                    if (v >= 1) {
                        b->eco_damage = 100;
                    }
                    if (v == 2) {
                        p->planet_ecology = 3;
                    } else if (v == 3) {
                        p->planet_ecology = 7;
                        *FactionIDCurrent = f;
                        FactionsStatus[0] |= (uint8_t)(1 << f);
                        *GlobalWarmingCount = 1000;
                        TurnFocusAnswer = r;
                    } else if (v == 4) {
                        p->planet_ecology = 9;
                        FactionsStatus[0] &= (uint8_t)~(1 << f);
                        *DiffLevelCurrent = DLVL_TRANSCEND;
                        *GlobalWarmingCount = 1000;
                        *ExpansionEnabled = r;
                    } else if (v == 5) {
                        p->planet_ecology = 12;
                        *GameState &= ~STATE_VOLCANO_ERUPTED;
                        *FactionIDCurrent = r ? f : turn_other_faction(f);
                    } else if (v == 6) {
                        *GameState |= STATE_PERIHELION_ACTIVE;
                        p->flags |= PFLAG_UNK_4000;
                        p->planet_ecology = 8;
                    } else if (v == 7) {
                        *GameRules |= RULES_SCN_NO_NATIVE_LIFE;
                    } else if (v == 8) {
                        SecretProject->voice_of_planet = base_id;
                        p->planet_ecology = 8;
                    }
                };
                sprintf_s(label, "base_ecology() base %d variant %d run %d", base_id, v, r);
                uint32_t seed = (uint32_t)(base_id * 61 + v * 11 + r);
                h.run(label, seed, setup, [&] { ((func_void *)0x004F67F0)(); return 0; },
                    [&] { base_ecology(); return 0; });
                volcanoes += v == 5 && (h.after_orig.game_state & STATE_VOLCANO_ERUPTED);
                warmings += h.orig.sea_level_council != h.saved.sea_level_council;
            }
        }
    }
    logf("base_ecology: %d compared (by the original: %d units born, %d global warming steps, %d "
        "volcanoes; %d with a call out), %d mismatches", h.compared, h.born, warmings, volcanoes,
        h.with_calls, h.mismatches);
    return h.mismatches;
}

static int test_base_upkeep() {
    OriginalEntry vanilla(0x004F79C0, BaseUpkeepEntry);
    OriginalEntry vanilla_ecology(0x004F67F0, BaseEcologyEntry);
    TurnStubs stubs;
    TimeGetTimePin pin;
    if (!vanilla.ok || !vanilla_ecology.ok || !stubs.ok() || !pin.ok()) {
        logf("base_upkeep: cannot restore the originals, stub the UI calls or pin timeGetTime");
        return 1;
    }
    func_i *exe = (func_i *)0x004F79C0;
    TurnHarness h;
    char label[96];
    int ended = 0, renamed = 0, zoomed = 0;
    for (int base_id = 0; base_id < h.base.base_count; base_id++) {
        Base *b = &Bases[base_id];
        uint32_t f = b->faction_id_current;
        if (f < 1 || f >= MaxPlayerNum) {
            continue;
        }
        BOOL quiet = quiet_faction(f);
        // with base_production() (AI bases not the player's): 0 as loaded; 1 turn 1; 2 to be renamed,
        // shown in the base window; 3 a message asks to see the base; 4 assimilation (back with
        // its former owner on odd runs) and a nerve staple counting down; 5 heavy ecological
        // damage, 8 blooms. With base_production() stubbed (any base, as the player's): 6 human,
        // to be renamed, shown, asked to see; 7 AI on turn 1 after the game ended
        for (int v = 0; v < 8; v++) {
            if (v < 6 && !quiet) {
                continue;
            }
            if (v >= 6 && base_id % 3) {
                continue;
            }
            for (int r = 0; r < 2; r++) {
                if (r && v != 2 && v != 4 && v != 6) {
                    continue;
                }
                auto setup = [&] {
                    PlayerData *p = &PlayersData[f];
                    if (v == 1 || v == 7) {
                        *TurnCurrentNum = 1;
                    }
                    if (v == 2 || v == 6) {
                        b->state |= BSTATE_UNK_10000;
                        *BaseWinBaseID = base_id;
                        TurnFocusAnswer = r;
                    }
                    if (v == 3 || v == 6) {
                        TurnZoomAnswer = true;
                    }
                    if (v == 4) {
                        b->assimilation_turns_left = 3;
                        b->faction_id_former = (uint8_t)(r ? f : turn_other_faction(f));
                        b->nerve_staple_turns_left = 2;
                    } else if (v == 5) {
                        b->eco_damage = 90;
                        p->planet_ecology = 8;
                    }
                    if (v >= 6) {
                        *FactionIDCurrent = f;
                        // no research: tech_selection() is a dialog for the player at the controls
                        *GameRules |= RULES_SCN_NO_TECH_ADVANCES;
                        if (v == 6) {
                            FactionsStatus[0] |= (uint8_t)(1 << f);
                            // police of -3 or worse could reach drone_riot()'s uninitialized slots,
                            // which differ between the two call paths (see test_drone_riot)
                            if (p->soc_effect_pending.police < -2) {
                                p->soc_effect_pending.police = -2;
                            }
                        } else {
                            FactionsStatus[0] &= (uint8_t)~(1 << f);
                            *GameState |= STATE_UNK_1;
                        }
                    }
                };
                sprintf_s(label, "base_upkeep(%d) variant %d run %d", base_id, v, r);
                uint32_t seed = (uint32_t)(base_id * 43 + v * 5 + r);
                if (v >= 6) {
                    h.run(label, seed, setup,
                        [&] { JmpStub prod(0x004F07E0, (void *)turn_production); return exe(base_id); },
                        [&] { JmpStub prod(0x004F07E0, (void *)turn_production); return base_upkeep(base_id); });
                } else {
                    h.run(label, seed, setup, [&] { return exe(base_id); },
                        [&] { return base_upkeep(base_id); });
                }
                ended += h.orig.ret != 0;
                renamed += (h.after_orig.bases[base_id].state & BSTATE_UNK_80)
                    && !(h.base.bases[base_id].state & BSTATE_UNK_80);
                for (int i = 0; i < h.orig.call_count && i < MaxTurnCalls; i++) {
                    zoomed += h.orig.calls[i].fn == TF_ZOOM;
                }
            }
        }
    }
    logf("base_upkeep: %d compared (by the original: %d bases ended, %d renamed, %d base windows "
        "opened, %d units born; %d with a call out), %d mismatches", h.compared, ended, renamed, zoomed,
        h.born, h.with_calls, h.mismatches);
    return h.mismatches;
}

static int test_production_phase() {
    OriginalEntry vanilla(0x00526E70, ProductionPhaseEntry);
    OriginalEntry vanilla_upkeep(0x004F79C0, BaseUpkeepEntry);
    OriginalEntry vanilla_ecology(0x004F67F0, BaseEcologyEntry);
    TurnStubs stubs;
    TimeGetTimePin pin;
    if (!vanilla.ok || !vanilla_upkeep.ok || !vanilla_ecology.ok || !stubs.ok() || !pin.ok()) {
        logf("production_phase: cannot restore the originals, stub the UI calls or pin timeGetTime");
        return 1;
    }
    func5 *exe = (func5 *)0x00526E70;
    TurnHarness h;
    char label[96];
    int paid = 0, freebies = 0, sanctions_ended = 0;
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        if (!is_alive(f)) {
            continue;
        }
        BOOL quiet = quiet_faction(f);
        int other = turn_other_faction(f);
        int third = turn_other_faction(other);
        // AI factions not the player's: 0 as loaded; 1 Thinker level and 2 Transcend (maintenance
        // refunds); 3 loans to two partners, one at vendetta, the third partner under sanctions,
        // little energy left; 4 turn 1 (free techs); 5 the interest rule at 1 per base and 6 at
        // 15%. As the player's: 7 human, sanctions ending; 8 the same as an alien faction; 9 AI on
        // Transcend with loans. base_production() is stubbed in 1, 2 and 7-9: on the hard levels it
        // can open its own production dialog (a stack Popup) even for an AI faction
        for (int v = 0; v < 10; v++) {
            if (v < 7 && !quiet) {
                continue;
            }
            BOOL stub = v >= 7 || v == 1 || v == 2;
            auto setup = [&] {
                PlayerData *p = &PlayersData[f];
                if (v == 1) {
                    *DiffLevelCurrent = DLVL_THINKER;
                } else if (v == 2 || v == 9) {
                    *DiffLevelCurrent = DLVL_TRANSCEND;
                }
                if (v == 3 || v == 9) {
                    p->energy_reserves = 12;
                    if (other) {
                        p->loan_balance[other] = 60;
                        p->loan_payment[other] = 9;
                        p->diplo_treaties[other] &= ~DTREATY_VENDETTA;
                    }
                    if (third && third != (int)f) {
                        p->loan_balance[third] = 40;
                        p->loan_payment[third] = 7;
                        p->diplo_treaties[third] |= DTREATY_VENDETTA;
                    }
                    int fourth = turn_other_faction(third);
                    if (fourth && fourth != (int)f && fourth != other) {
                        p->loan_balance[fourth] = 30;
                        p->loan_payment[fourth] = 5;
                        PlayersData[fourth].sanction_turns = 3;
                    }
                } else if (v == 4) {
                    *TurnCurrentNum = 1;
                    if (Players[f].rule_tech_selected < 1) {
                        Players[f].rule_tech_selected = 2;
                    }
                } else if (v == 5 || v == 6) {
                    Players[f].rule_flags |= RFLAG_INTEREST;
                    Players[f].rule_interest = v == 5 ? 0 : 15;
                    p->energy_reserves = 333;
                }
                if (stub) {
                    // no research: tech_selection() is a dialog for the player at the controls
                    *GameRules |= RULES_SCN_NO_TECH_ADVANCES;
                }
                if (v >= 7) {
                    *FactionIDCurrent = f;
                    if (v <= 8) {
                        FactionsStatus[0] |= (uint8_t)(1 << f);
                        p->sanction_turns = 1;
                        if (p->soc_effect_pending.police < -2) {
                            p->soc_effect_pending.police = -2; // see test_base_upkeep
                        }
                        if (v == 8) {
                            Players[f].rule_flags |= RFLAG_ALIEN;
                        }
                    } else {
                        FactionsStatus[0] &= (uint8_t)~(1 << f);
                    }
                }
            };
            sprintf_s(label, "production_phase(%u) variant %d", f, v);
            uint32_t seed = (uint32_t)(f * 101 + v * 13);
            if (stub) {
                h.run(label, seed, setup,
                    [&] { JmpStub prod(0x004F07E0, (void *)turn_production); exe(f); return 0; },
                    [&] { JmpStub prod(0x004F07E0, (void *)turn_production); production_phase(f); return 0; });
            } else {
                h.run(label, seed, setup, [&] { exe(f); return 0; }, [&] { production_phase(f); return 0; });
            }
            paid += (v == 3 || v == 9) && other
                && h.after_orig.players[other].energy_reserves != h.base.players[other].energy_reserves;
            for (int i = 0; i < h.orig.call_count && i < MaxTurnCalls; i++) {
                freebies += h.orig.calls[i].fn == TF_TECH_ACHIEVED && v == 4;
                sanctions_ended += h.orig.calls[i].fn == TF_POPP && !strncmp(h.orig.calls[i].label, "SANCTIONS", 9);
            }
        }
    }
    logf("production_phase: %d compared (by the original: %d loans paid, %d free techs, %d sanctions "
        "ended, %d units born; %d with a call out), %d mismatches", h.compared, paid, freebies,
        sanctions_ended, h.born, h.with_calls, h.mismatches);
    return h.mismatches;
}

static DWORD WINAPI regress_watcher(LPVOID) {
    for (;;) {
        Sleep(1000);
        if (!RegressPending && GetFileAttributesA(TRIGGER_FILE) != INVALID_FILE_ATTRIBUTES) {
            InterlockedExchange(&RegressPending, 1);
        }
    }
}

// Input-free runs. A trigger file that is already there when the game starts and holds a line
// "load <save file>" makes the harness load that save in place of the main menu (as Thinker's
// command line load does), run the tests named on the other lines once the game is under way
// (from a timer, so no map click is needed) and, with a line "exit", end the game afterwards; a
// line "notests" only loads the save, and with "clickpass" as well starts the click-pass mode
// (clickpass.h) once the game is under way; "outside <folder>" lets it write there too and
// "guardprobe [<folder>]" then tests its write guard.
static char AutoLoadPath[MAX_PATH];
static BOOL AutoExit = false;
static BOOL AutoNoTests = false; // a line "notests": only load the save (for boot tests)
static BOOL AutoGuardProbe = false; // a line "guardprobe": clickpass_guard_probe() after the start
static char AutoGuardProbeFolder[MAX_PATH]; // "guardprobe <folder>"
static char AutoOutsideFolder[MAX_PATH]; // a line "outside <folder>"
static int AutoHangSeconds = 600; // a line "hang <seconds>": the watchdog's limit per test

// Every 30 s of one test the watchdog logs a heartbeat with the test's last announced input (a
// working test shows it changing, a stuck one doesn't), so whoever watches the log knows early;
// only after AutoHangSeconds does it end the game.
static DWORD WINAPI regress_hang_watchdog(LPVOID) {
    const char *beat_name = NULL;
    DWORD beat_next = 0;
    for (;;) {
        Sleep(2000);
        const char *name = RegressTestName;
        if (!name) {
            beat_name = NULL;
            continue;
        }
        DWORD elapsed = GetTickCount() - RegressTestStart;
        if (name != beat_name) {
            beat_name = name;
            beat_next = 30000;
        }
        if (elapsed >= beat_next && regress_log) {
            fprintf(regress_log, "  ... still in %s after %lu s, last input: %s\n", name, elapsed / 1000,
                regress_ctx);
            fflush(regress_log);
            beat_next += 30000;
        }
        if (elapsed > (DWORD)AutoHangSeconds * 1000) {
            if (regress_log) {
                fprintf(regress_log, "HANG in %s: no result after %d s, last input: %s\n", name,
                    AutoHangSeconds, regress_ctx);
                fprintf(regress_log, "TOTAL mismatches: -1 (hang)\n");
                fflush(regress_log);
            }
            TerminateProcess(GetCurrentProcess(), 3);
        }
    }
}
static JmpStub *AutoMenuStub = NULL;
static UINT_PTR AutoTimer = 0;
static int AutoTicks = 0;
typedef int __cdecl func_top_menu(int);
static func_top_menu *const top_menu_exe = (func_top_menu *)0x0058E360;

static void auto_log(const char *fmt, const char *arg, unsigned value) {
    FILE *log = fopen(LOG_FILE, "w");
    if (log) {
        fprintf(log, fmt, arg, value);
        fputs("\nTOTAL mismatches: -1 (auto run failed)\n", log);
        fclose(log);
    }
}

static void CALLBACK regress_auto_timer(HWND, UINT, UINT_PTR, DWORD) {
    if (*GameHalted || ++AutoTicks < 3) {
        return; // let the turn get under way first
    }
    KillTimer(NULL, AutoTimer);
    AutoTimer = 0;
    if (AutoNoTests) {
        if (ClickpassMode) {
            clickpass_start(AutoLoadPath); // first, so clickpass\ready is there when the trigger is gone
            if (AutoGuardProbe) {
                clickpass_guard_probe(AutoGuardProbeFolder); // likewise logged by then
            }
        }
        DeleteFileA(TRIGGER_FILE);
        return;
    }
    regress_run();
    if (AutoExit) {
        TerminateProcess(GetCurrentProcess(), 0); // no prefs or saves written on the way out
    }
}

// Message-driven input for input-free runs: the UI scripts (tools/mp/msg.ps1) post clicks and keys
// to the game window instead of using the real mouse and keyboard. The game also asks Windows
// directly whether Ctrl, Shift, Alt or a mouse button is down and where the cursor is, which
// posted messages don't change, so a message hook on the game's thread notes what the messages
// it takes off the queue say, and the exe's GetKeyState, GetAsyncKeyState and GetCursorPos
// imports add that to the real state. Mouse messages from the real mouse are noted the same way,
// so the two agree while someone uses the game window by hand.
static BOOL VirtualKeyDown[256];
static POINT VirtualCursor; // client coordinates of the last mouse message
static HWND VirtualCursorWindow = NULL;
static HHOOK InputHook = NULL;
static FARPROC *const ImportGetCursorPos = (FARPROC *)0x00669284;
static FARPROC *const ImportGetKeyState = (FARPROC *)0x0066932C;
static FARPROC *const ImportGetAsyncKeyState = (FARPROC *)0x00669330;

static LRESULT CALLBACK input_hook(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION && wparam == PM_REMOVE) {
        const MSG *msg = (const MSG *)lparam;
        switch (msg->message) {
          case WM_KEYDOWN:
          case WM_SYSKEYDOWN:
            VirtualKeyDown[msg->wParam & 0xFF] = true;
            break;
          case WM_KEYUP:
          case WM_SYSKEYUP:
            VirtualKeyDown[msg->wParam & 0xFF] = false;
            break;
          case WM_LBUTTONDOWN:
            VirtualKeyDown[VK_LBUTTON] = true;
            break;
          case WM_LBUTTONUP:
            VirtualKeyDown[VK_LBUTTON] = false;
            break;
          case WM_RBUTTONDOWN:
            VirtualKeyDown[VK_RBUTTON] = true;
            break;
          case WM_RBUTTONUP:
            VirtualKeyDown[VK_RBUTTON] = false;
            break;
        }
        if (msg->message >= WM_MOUSEMOVE && msg->message <= WM_MBUTTONDBLCLK) {
            VirtualCursor.x = (int16_t)LOWORD(msg->lParam);
            VirtualCursor.y = (int16_t)HIWORD(msg->lParam);
            VirtualCursorWindow = msg->hwnd;
        }
        clickpass_on_message(msg); // the click pass's state dumps
    }
    return CallNextHookEx(InputHook, code, wparam, lparam);
}

static BOOL virtual_key_down(int vk) {
    if (vk == VK_LCONTROL || vk == VK_RCONTROL) {
        vk = VK_CONTROL;
    } else if (vk == VK_LSHIFT || vk == VK_RSHIFT) {
        vk = VK_SHIFT;
    } else if (vk == VK_LMENU || vk == VK_RMENU) {
        vk = VK_MENU;
    }
    return VirtualKeyDown[vk & 0xFF];
}

static SHORT WINAPI input_get_key_state(int vk) {
    SHORT state = GetKeyState(vk);
    return virtual_key_down(vk) ? (SHORT)(state | 0x8000) : state;
}

static SHORT WINAPI input_get_async_key_state(int vk) {
    SHORT state = GetAsyncKeyState(vk);
    return virtual_key_down(vk) ? (SHORT)(state | 0x8000) : state;
}

static BOOL WINAPI input_get_cursor_pos(LPPOINT point) {
    if (!VirtualCursorWindow) {
        return GetCursorPos(point);
    }
    *point = VirtualCursor;
    return ClientToScreen(VirtualCursorWindow, point);
}

static void input_hook_import(FARPROC *entry, FARPROC replacement) {
    DWORD protect;
    if (VirtualProtect(entry, sizeof(FARPROC), PAGE_READWRITE, &protect)) {
        *entry = replacement;
        VirtualProtect(entry, sizeof(FARPROC), protect, &protect);
    }
}

// The driver posts the character messages itself (with the right Shift and Ctrl) and marks its
// key messages with lParam bit 25 (reserved); the game's TranslateMessage would otherwise add a
// second character, made from the real keyboard state.
static FARPROC *const ImportTranslateMessage = (FARPROC *)0x0066935C;
static BOOL WINAPI input_translate_message(const MSG *msg) {
    if ((msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) && (msg->lParam & (1 << 25))) {
        return false;
    }
    return TranslateMessage(msg);
}

static void install_input_hooks() {
    InputHook = SetWindowsHookExA(WH_GETMESSAGE, input_hook, NULL, GetCurrentThreadId());
    input_hook_import(ImportTranslateMessage, (FARPROC)input_translate_message);
    input_hook_import(ImportGetKeyState, (FARPROC)input_get_key_state);
    input_hook_import(ImportGetAsyncKeyState, (FARPROC)input_get_async_key_state);
    input_hook_import(ImportGetCursorPos, (FARPROC)input_get_cursor_pos);
}

static int __cdecl regress_auto_top_menu(int unk) {
    delete AutoMenuStub; // the real main menu from now on
    AutoMenuStub = NULL;
    install_input_hooks(); // on the game's own thread

    uint32_t result = load_daemon(AutoLoadPath, false);
    if (result) {
        auto_log("auto run: loading %s failed (%u)", AutoLoadPath, result);
        if (AutoExit) {
            TerminateProcess(GetCurrentProcess(), 1);
        }
        return top_menu_exe(unk);
    }
    for (int i = 1; i < MaxPlayerNum; i++) {
        load_faction_art(i);
    }
    AutoTicks = 0;
    AutoTimer = SetTimer(NULL, 0, 1000, regress_auto_timer);
    return 0;
}

void regress_init() {
    FILE *trig = fopen(TRIGGER_FILE, "r");
    if (trig) {
        char line[MAX_PATH + 8];
        while (fgets(line, sizeof(line), trig)) {
            size_t n = strlen(line);
            while (n && isspace((unsigned char)line[n - 1])) {
                line[--n] = 0;
            }
            if (!strncmp(line, "load ", 5)) {
                strcpy_s(AutoLoadPath, line + 5);
            } else if (!strcmp(line, "exit")) {
                AutoExit = true;
            } else if (!strcmp(line, "notests")) {
                AutoNoTests = true;
            } else if (!strcmp(line, "clickpass")) {
                ClickpassMode = true;
            } else if (!strcmp(line, "guardprobe")) {
                AutoGuardProbe = true;
            } else if (!strncmp(line, "guardprobe ", 11)) {
                AutoGuardProbe = true;
                strcpy_s(AutoGuardProbeFolder, line + 11);
            } else if (!strncmp(line, "outside ", 8)) {
                strcpy_s(AutoOutsideFolder, line + 8);
            } else if (!strncmp(line, "hang ", 5) && atoi(line + 5) > 0) {
                AutoHangSeconds = atoi(line + 5);
            }
        }
        fclose(trig);
    }
    if (ClickpassMode) {
        clickpass_guard_install(AutoOutsideFolder); // before the game itself runs
    }
    if (AutoLoadPath[0]) {
        AutoMenuStub = new JmpStub(0x0058E360, (void *)regress_auto_top_menu);
        CreateThread(NULL, 0, regress_hang_watchdog, NULL, 0, NULL);
        return; // no watcher: the timer runs the tests
    }
    if (!regress_thread) {
        regress_thread = CreateThread(NULL, 0, regress_watcher, NULL, 0, NULL);
    }
}

void regress_run() {
    InterlockedExchange(&RegressPending, 0);
    regress_filter[0] = 0;
    FILE *trig = fopen(TRIGGER_FILE, "r");
    if (trig) {
        char line[MAX_PATH + 8];
        while (fgets(line, sizeof(line), trig)) { // skip the input-free run's "load" and "exit"
            if (strncmp(line, "load ", 5) && strncmp(line, "exit", 4) && strncmp(line, "notests", 7)
                && strncmp(line, "hang ", 5) && strncmp(line, "clickpass", 9)
                && strncmp(line, "guardprobe", 10) && strncmp(line, "outside ", 8)) {
                strncat_s(regress_filter, line, _TRUNCATE);
            }
        }
        fclose(trig);
        size_t n = strlen(regress_filter);
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
    log_survey();
    int total = 0;
    if (test_enabled("value_of_base")) total += run_guarded("value_of_base", test_value_of_base);
    if (test_enabled("best_defender")) total += run_guarded("best_defender", test_best_defender);
    if (test_enabled("battle_compute")) total += run_guarded("battle_compute", test_battle_compute);
    if (test_enabled("wants_to_attack")) total += run_guarded("wants_to_attack", test_wants_to_attack);
    if (test_enabled("hex_cost")) total += run_guarded("hex_cost", test_hex_cost);
    // writes save files: only when named
    if (regress_filter[0] && strstr(regress_filter, "save_slot")) total += run_guarded("save_slot", test_save_slot);
    if (regress_filter[0] && strstr(regress_filter, "save_header")) total += run_guarded("save_header", test_save_header);
    if (test_enabled("world_validate")) total += run_guarded("world_validate", test_world_validate);
    if (test_enabled("treaty_on")) total += run_guarded("treaty_on", test_treaty_on);
    if (test_enabled("treaty_off")) total += run_guarded("treaty_off", test_treaty_off);
    if (test_enabled("agenda_on")) total += run_guarded("agenda_on", test_agenda_on);
    if (test_enabled("agenda_off")) total += run_guarded("agenda_off", test_agenda_off);
    if (test_enabled("trade_maps")) total += run_guarded("trade_maps", test_trade_maps);
    if (test_enabled("do_withdrawal")) total += run_guarded("do_withdrawal", test_do_withdrawal);
    if (test_enabled("base_mark")) total += run_guarded("base_mark", test_base_mark);
    if (test_enabled("base_psych")) total += run_guarded("base_psych", test_base_psych);
    if (test_enabled("do_wrappers")) total += run_guarded("do_wrappers", test_do_wrappers);
    if (test_enabled("veh", "veh_predicates")) total += run_guarded("veh_predicates", test_veh_predicates);
    if (test_enabled("veh", "veh_find")) total += run_guarded("veh_find", test_veh_find);
    if (test_enabled("veh", "is_bunged")) total += run_guarded("is_bunged", test_is_bunged);
    if (test_enabled("veh", "clear_bunglist")) total += run_guarded("clear_bunglist", test_clear_bunglist);
    if (test_enabled("veh", "add_abil")) total += run_guarded("add_abil", test_add_abil);
    if (test_enabled("veh", "unspot_stack")) total += run_guarded("unspot_stack", test_unspot_stack);
    if (test_enabled("veh", "stack_kill")) total += run_guarded("stack_kill", test_stack_kill);
    if (test_enabled("veh", "get_rating")) total += run_guarded("get_rating", test_get_rating);
    if (test_enabled("veh", "most_objectives")) total += run_guarded("most_objectives", test_most_objectives);
    if (test_enabled("batch2", "abil_cond")) total += run_guarded("abil_cond", test_abil_cond);
    if (test_enabled("batch2", "bung_proto")) total += run_guarded("bung_proto", test_bung_proto);
    if (test_enabled("batch2", "spot_base")) total += run_guarded("spot_base", test_spot_base);
    if (test_enabled("batch2", "spot_stack")) total += run_guarded("spot_stack", test_spot_stack);
    if (test_enabled("batch2", "retire_proto")) total += run_guarded("retire_proto", test_retire_proto);
    if (test_enabled("batch2", "study_artifact")) total += run_guarded("study_artifact", test_study_artifact);
    if (test_enabled("batch3", "base_at")) total += run_guarded("base_at", test_base_at);
    if (test_enabled("batch3", "upgrade_cost")) total += run_guarded("upgrade_cost", test_upgrade_cost);
    if (test_enabled("batch3", "do_upgrade")) total += run_guarded("do_upgrade", test_do_upgrade);
    if (test_enabled("batch3", "upgrade_prototypes")) total += run_guarded("upgrade_prototypes", test_upgrade_prototypes);
    if (test_enabled("batch3", "upgrade_any_prototypes")) total += run_guarded("upgrade_any_prototypes", test_upgrade_any_prototypes);
    if (test_enabled("batch3", "base_claimed")) total += run_guarded("base_claimed", test_base_claimed);
    if (test_enabled("batch3", "site_at")) total += run_guarded("site_at", test_site_at);
    if (test_enabled("batch3", "spying")) total += run_guarded("spying", test_spying);
    if (test_enabled("batch3", "act_of_aggression")) total += run_guarded("act_of_aggression", test_act_of_aggression);
    if (test_enabled("batch3", "council_get_vote")) total += run_guarded("council_get_vote", test_council_get_vote);
    if (test_enabled("batch3", "can_call_council")) total += run_guarded("can_call_council", test_can_call_council);
    if (test_enabled("batch3", "pick_top_veh")) total += run_guarded("pick_top_veh", test_pick_top_veh);
    if (test_enabled("batch3", "can_convoy")) total += run_guarded("can_convoy", test_can_convoy);
    if (test_enabled("batch3", "promote")) total += run_guarded("promote", test_promote);
    if (test_enabled("batch3", "proto_sort")) total += run_guarded("proto_sort", test_proto_sort);
    if (test_enabled("batch3", "yearmotize")) total += run_guarded("yearmotize", test_yearmotize);
    if (test_enabled("batch4") || test_enabled("world_rocky") || test_enabled("world_fungus")
        || test_enabled("world_shorelines") || test_enabled("world_rainfall")) {
        total += run_guarded("world_batch4", test_world_batch4);
    }
    if (test_enabled("batch5", "tech_advance")) total += run_guarded("tech_advance", test_tech_advance);
    if (test_enabled("batch5", "tech_research")) total += run_guarded("tech_research", test_tech_research);
    if (test_enabled("batch5", "steal_tech")) total += run_guarded("steal_tech", test_steal_tech);
    if (test_enabled("batch6", "num_objectives")) total += run_guarded("num_objectives", test_num_objectives);
    if (test_enabled("batch6", "battle_kill")) total += run_guarded("battle_kill", test_battle_kill_family);
    if (test_enabled("batch6", "monuments")) total += run_guarded("monuments", test_monuments);
    if (test_enabled("batch6", "social_set")) total += run_guarded("social_set", test_social_set);
    if (test_enabled("batch6", "bases_reset")) total += run_guarded("bases_reset", test_bases_reset);
    if (test_enabled("batch7", "yields")) total += run_guarded("yields", test_yields);
    if (test_enabled("batch8", "world_site")) total += run_guarded("world_site", test_world_site);
    if (test_enabled("batch8", "good_sensor")) total += run_guarded("good_sensor", test_good_sensor);
    if (test_enabled("batch8", "spot_loc")) total += run_guarded("spot_loc", test_spot_loc);
    if (test_enabled("batch8", "territory")) total += run_guarded("territory", test_territory);
    if (test_enabled("batch9", "suggest_plan")) total += run_guarded("suggest_plan", test_suggest_plan);
    if (test_enabled("batch9", "scan_prototypes")) total += run_guarded("scan_prototypes", test_scan_prototypes);
    if (test_enabled("batch10", "set_course")) total += run_guarded("set_course", test_set_course);
    if (test_enabled("batch10", "enemies_trade_tech")) total += run_guarded("enemies_trade_tech", test_enemies_trade_tech);
    if (test_enabled("batch10", "tech_analysis")) total += run_guarded("tech_analysis", test_tech_analysis);
    if (test_enabled("batch11", "rankings")) total += run_guarded("rankings", test_rankings);
    if (test_enabled("batch11", "stack_veh")) total += run_guarded("stack_veh", test_stack_veh);
    if (test_enabled("batch12", "reset_territory")) total += run_guarded("reset_territory", test_reset_territory);
    if (test_enabled("batch12", "base_research")) total += run_guarded("base_research", test_base_research);
    if (test_enabled("batch12", "base_check_support")) total += run_guarded("base_check_support", test_base_check_support);
    if (test_enabled("batch13", "base_support")) total += run_guarded("base_support", test_base_support);
    if (test_enabled("batch13", "farm_compute")) total += run_guarded("farm_compute", test_farm_compute);
    if (test_enabled("batch14", "base_yield")) total += run_guarded("base_yield", test_base_yield);
    if (test_enabled("batch14", "base_energy")) total += run_guarded("base_energy", test_base_energy);
    if (test_enabled("batch15", "base_growth")) total += run_guarded("base_growth", test_base_growth);
    if (test_enabled("batch15", "base_doctors")) total += run_guarded("base_doctors", test_base_doctors);
    if (test_enabled("batch16", "faction_strings")) total += run_guarded("faction_strings", test_faction_strings);
    if (test_enabled("batch16", "small_helpers")) total += run_guarded("small_helpers", test_small_helpers);
    if (test_enabled("batch16", "generators")) total += run_guarded("generators", test_generators);
    if (test_enabled("batch16", "base_compute")) total += run_guarded("base_compute", test_base_compute);
    if (test_enabled("batch17", "assemble_passengers")) total += run_guarded("assemble_passengers", test_assemble_passengers);
    if (test_enabled("batch17", "alien_base")) total += run_guarded("alien_base", test_alien_base);
    if (test_enabled("batch18", "repair_phase")) total += run_guarded("repair_phase", test_repair_phase);
    if (test_enabled("batch18", "allocate_energy")) total += run_guarded("allocate_energy", test_allocate_energy);
    if (test_enabled("batch19", "map_dirty")) total += run_guarded("map_dirty", test_map_dirty);
    if (test_enabled("batch19", "world_UNK1")) total += run_guarded("world_UNK1", test_world_UNK1);
    if (test_enabled("batch19", "do_fungal_towers")) total += run_guarded("do_fungal_towers", test_do_fungal_towers);
    if (test_enabled("batch20", "base_drones")) total += run_guarded("base_drones", test_base_drones);
    if (test_enabled("batch21", "mon_records")) total += run_guarded("mon_records", test_mon_records);
    if (test_enabled("batch22", "valid_patrol")) total += run_guarded("valid_patrol", test_valid_patrol);
    if (test_enabled("batch22", "energy_compute")) total += run_guarded("energy_compute", test_energy_compute);
    if (test_enabled("batch23", "name_proto")) total += run_guarded("name_proto", test_name_proto);
    if (test_enabled("batch23", "propose_proto")) total += run_guarded("propose_proto", test_propose_proto);
    if (test_enabled("ptrfix", "strat")) total += run_guarded("strat", test_strat);
    if (test_enabled("ptrfix", "goody_at")) total += run_guarded("goody_at", test_goody_at);
    if (test_enabled("batch24", "can_terraform")) total += run_guarded("can_terraform", test_can_terraform);
    if (test_enabled("batch25", "balance")) total += run_guarded("balance", test_balance);
    if (test_enabled("ishuman", "tech_val")) total += run_guarded("tech_val", test_tech_val);
    if (test_enabled("batch26", "upgrade_prototype")) total += run_guarded("upgrade_prototype", test_upgrade_prototype);
    if (test_enabled("batch27", "veh_init")) total += run_guarded("veh_init", test_veh_init);
    if (test_enabled("batch27", "alien_start")) total += run_guarded("alien_start", test_alien_start);
    if (test_enabled("diplonote", "pact_notices")) total += run_guarded("pact_notices", test_pact_notices);
    if (test_enabled("diplonote", "enemies_unpact")) total += run_guarded("enemies_unpact", test_enemies_unpact);
    if (test_enabled("diplonote", "enemies_team_up")) total += run_guarded("enemies_team_up", test_enemies_team_up);
    if (test_enabled("diplonote", "break_treaty")) total += run_guarded("break_treaty", test_break_treaty);
    if (test_enabled("diplonote", "make_treaty")) total += run_guarded("make_treaty", test_make_treaty);
    if (test_enabled("diplonote", "pledge_truce")) total += run_guarded("pledge_truce", test_pledge_truce);
    if (test_enabled("diplonote", "demands_withdrawal")) total += run_guarded("demands_withdrawal", test_demands_withdrawal);
    if (test_enabled("diplonote", "renounce_pact")) total += run_guarded("renounce_pact", test_renounce_pact);
    if (test_enabled("diplonote", "dont_withdrawal")) total += run_guarded("dont_withdrawal", test_dont_withdrawal);
    if (test_enabled("diplotwo", "intervention")) total += run_guarded("intervention", test_intervention);
    if (test_enabled("diplotwo", "demand_withdrawal")) total += run_guarded("demand_withdrawal", test_demand_withdrawal);
    if (test_enabled("diplotwo", "mention_prototypes")) total += run_guarded("mention_prototypes", test_mention_prototypes);
    if (test_enabled("diplotwo", "buy_tech")) total += run_guarded("buy_tech", test_buy_tech);
    if (test_enabled("diplotwo", "pact_withdraw")) total += run_guarded("pact_withdraw", test_pact_withdraw);
    if (test_enabled("diplotwo", "energy_trade")) total += run_guarded("energy_trade", test_energy_trade);
    if (test_enabled("b28main", "mon_winning")) total += run_guarded("mon_winning", test_mon_winning);
    if (test_enabled("b28main", "energy_limit")) total += run_guarded("energy_limit", test_energy_limit);
    if (test_enabled("b28main", "fixup_landmarks")) total += run_guarded("fixup_landmarks", test_fixup_landmarks);
    if (test_enabled("b28main", "delete_landmark")) total += run_guarded("delete_landmark", test_delete_landmark);
    if (test_enabled("b28main", "terrain_fixup")) total += run_guarded("terrain_fixup", test_terrain_fixup);
    if (test_enabled("b28main", "mapwin_terrain_fixup")) total += run_guarded("mapwin_terrain_fixup", test_mapwin_terrain_fixup);
    if (test_enabled("b28main", "base_init")) total += run_guarded("base_init", test_base_init);
    if (test_enabled("b28main", "name_base")) total += run_guarded("name_base", test_name_base);
    if (test_enabled("b28main", "battle_fight")) total += run_guarded("battle_fight", test_battle_fight);
    if (test_enabled("b28main", "compute_odds")) total += run_guarded("compute_odds", test_compute_odds);
    if (test_enabled("protodesign", "prune_protos")) total += run_guarded("prune_protos", test_prune_protos);
    if (test_enabled("protodesign", "consider_designs")) total += run_guarded("consider_designs", test_consider_designs);
    if (test_enabled("baseprod", "base_change")) total += run_guarded("base_change", test_base_change);
    if (test_enabled("baseprod", "base_reset")) total += run_guarded("base_reset", test_base_reset);
    if (test_enabled("baseprod", "base_hurry")) total += run_guarded("base_hurry", test_base_hurry);
    if (test_enabled("enemy_capabilities")) total += run_guarded("enemy_capabilities", test_enemy_capabilities);
    if (test_enabled("social_ai")) total += run_guarded("social_ai", test_social_ai);
    if (test_enabled("Path::sensors") || test_enabled("path_sensors")) total += run_guarded("path_sensors", test_path_sensors);
    total += run_guarded("world_xy", test_world_xy);
    if (test_enabled("world", "world_volcano")) total += run_guarded("world_volcano", test_world_volcano);
    if (test_enabled("world", "world_alt_set")) total += run_guarded("world_alt_set", test_world_alt_set);
    if (test_enabled("world", "brush")) total += run_guarded("brush", test_brush);
    if (test_enabled("world", "paint_land")) total += run_guarded("paint_land", test_paint_land);
    if (test_enabled("world", "build_continent")) total += run_guarded("build_continent", test_build_continent);
    if (test_enabled("world", "build_hills")) total += run_guarded("build_hills", test_build_hills);
    total += run_guarded("world_void", test_world_void);
    if (test_enabled("natlife", "alien_fauna")) total += run_guarded("alien_fauna", test_alien_fauna);
    if (test_enabled("natlife", "world_rivers")) total += run_guarded("world_rivers", test_world_rivers);
    if (test_enabled("natlife", "world_climate")) total += run_guarded("world_climate", test_world_climate);
    if (test_enabled("natlife", "world_erosion")) total += run_guarded("world_erosion", test_world_erosion);
    if (test_enabled("baseturn", "drone_riot")) total += run_guarded("drone_riot", test_drone_riot);
    if (test_enabled("baseturn", "base_ecology")) total += run_guarded("base_ecology", test_base_ecology);
    if (test_enabled("baseturn", "base_upkeep")) total += run_guarded("base_upkeep", test_base_upkeep);
    if (test_enabled("baseturn", "production_phase")) total += run_guarded("production_phase", test_production_phase);
    if (test_enabled("facturn", "veh_kill")) total += run_guarded("veh_kill", test_veh_kill);
    if (test_enabled("facturn", "unit_kill")) total += run_guarded("unit_kill", test_unit_kill);
    if (test_enabled("facturn", "mash_planes")) total += run_guarded("mash_planes", test_mash_planes);
    if (test_enabled("facturn", "interceptor")) total += run_guarded("interceptor", test_interceptor);
    if (test_enabled("facturn", "compute_score")) total += run_guarded("compute_score", test_compute_score);
    if (test_enabled("facturn", "faction_upkeep")) total += run_guarded("faction_upkeep", test_faction_upkeep);
    // batch 31: AI strategy (group "aiplan"; "aistrat" would also run the ptrfix test "strat")
    if (test_enabled("aiplan", "enemy_strategy")) total += run_guarded("enemy_strategy", test_enemy_strategy);
    if (test_enabled("aiplan", "enemy_veh")) total += run_guarded("enemy_veh", test_enemy_veh);
    if (test_enabled("aiplan", "enemy_turn")) total += run_guarded("enemy_turn", test_enemy_turn);
    if (test_enabled("aidiplo", "contiguous")) total += run_guarded("contiguous", test_contiguous);
    if (test_enabled("aidiplo", "comm_check")) total += run_guarded("comm_check", test_comm_check);
    if (test_enabled("aidiplo", "enemies_war")) total += run_guarded("enemies_war", test_enemies_war);
    if (test_enabled("aidiplo", "pact_of_brotherhood")) total += run_guarded("pact_of_brotherhood", test_pact_of_brotherhood);
    if (test_enabled("aidiplo", "double_cross")) total += run_guarded("double_cross", test_double_cross);
    if (test_enabled("aidiplo", "enemy_diplomacy")) total += run_guarded("enemy_diplomacy", test_enemy_diplomacy);
    if (test_enabled("unitact", "action_small")) total += run_guarded("action_small", test_action_small);
    if (test_enabled("unitact", "action_gate")) total += run_guarded("action_gate", test_action_gate);
    if (test_enabled("unitact", "action_staple")) total += run_guarded("action_staple", test_action_staple);
    if (test_enabled("unitact", "action_destruct")) total += run_guarded("action_destruct", test_action_destruct);
    if (test_enabled("unitact", "action_sat_attack")) total += run_guarded("action_sat_attack", test_action_sat_attack);
    if (test_enabled("unitact", "action_build")) total += run_guarded("action_build", test_action_build);
    if (test_enabled("unitact", "action_arty")) total += run_guarded("action_arty", test_action_arty);
    if (test_enabled("baselife", "base_kill")) total += run_guarded("base_kill", test_base_kill);
    if (test_enabled("baselife", "give_a_base")) total += run_guarded("give_a_base", test_give_a_base);
    if (test_enabled("baselife", "base_swap")) total += run_guarded("base_swap", test_base_swap);
    if (test_enabled("events", "tech_selection")) total += run_guarded("tech_selection", test_tech_selection);
    if (test_enabled("events", "climactic_changes")) total += run_guarded("climactic_changes", test_climactic_changes);
    if (test_enabled("events", "major_atrocity")) total += run_guarded("major_atrocity", test_major_atrocity);
    if (test_enabled("events", "atrocity")) total += run_guarded("atrocity", test_atrocity);
    if (test_enabled("events", "alt_set_game")) total += run_guarded("alt_set_game", test_alt_set_game);
    if (test_enabled("events", "monolith_visit")) total += run_guarded("monolith_visit", test_monolith_visit);
    RegressTestName = NULL; // the hang watchdog stands down
    logf("TOTAL mismatches: %d (%lu ms)", total, GetTickCount() - t0);
    fclose(regress_log);
    regress_log = NULL;
}

#endif // OPENSMACX_REGRESS
