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
static int run_guarded(const char *name, int (*fn)()) {
    regress_ctx[0] = 0;
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

// wants_to_attack() in the exe never initialises a 64-entry buffer on its stack; on the game's
// own call paths the slots hold small leftovers and the function behaves as if zero-filled.
// Between harness calls the DLL leaves return addresses there, so the stack is zeroed first.
static __declspec(noinline) void scrub_stack() {
    volatile char pad[0x800];
    for (size_t i = 0; i < sizeof(pad); i++) {
        pad[i] = 0;
    }
}

static int test_wants_to_attack() {
    int compared = 0, mismatches = 0, unstable_orig = 0, unstable_new = 0;
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        for (uint32_t tgt = 1; tgt < MaxPlayerNum; tgt++) {
            if (f == tgt) {
                continue;
            }
            for (int other = -1; other < (int)MaxPlayerNum; other++) {
                scrub_stack();
                BOOL b = wants_to_attack_OG(f, tgt, other);
                scrub_stack();
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

static void **iat_slot(HMODULE module, const char *dll_name, const char *func_name) {
    uint8_t *base = (uint8_t *)module;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir->VirtualAddress) {
        return NULL;
    }
    for (IMAGE_IMPORT_DESCRIPTOR *desc = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir->VirtualAddress);
        desc->Name; desc++) {
        if (_stricmp((const char *)(base + desc->Name), dll_name) != 0) {
            continue;
        }
        IMAGE_THUNK_DATA *names = (IMAGE_THUNK_DATA *)(base + desc->OriginalFirstThunk);
        IMAGE_THUNK_DATA *slots = (IMAGE_THUNK_DATA *)(base + desc->FirstThunk);
        for (; names->u1.AddressOfData; names++, slots++) {
            if (names->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
                continue;
            }
            IMAGE_IMPORT_BY_NAME *imp = (IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData);
            if (strcmp((const char *)imp->Name, func_name) == 0) {
                return (void **)&slots->u1.Function;
            }
        }
    }
    return NULL;
}

static void *iat_patch(void **slot, void *replacement) {
    if (!slot) {
        return NULL;
    }
    DWORD old_protect;
    VirtualProtect(slot, sizeof(void *), PAGE_READWRITE, &old_protect);
    void *previous = *slot;
    *slot = replacement;
    VirtualProtect(slot, sizeof(void *), old_protect, &old_protect);
    return previous;
}

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
            // double_cross() is exe code that may address a human; AI pairs only
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
    log_survey();
    int total = 0;
    if (test_enabled("value_of_base")) total += run_guarded("value_of_base", test_value_of_base);
    if (test_enabled("best_defender")) total += run_guarded("best_defender", test_best_defender);
    if (test_enabled("battle_compute")) total += run_guarded("battle_compute", test_battle_compute);
    if (test_enabled("wants_to_attack")) total += run_guarded("wants_to_attack", test_wants_to_attack);
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
    logf("TOTAL mismatches: %d (%lu ms)", total, GetTickCount() - t0);
    fclose(regress_log);
    regress_log = NULL;
}

#endif // OPENSMACX_REGRESS
