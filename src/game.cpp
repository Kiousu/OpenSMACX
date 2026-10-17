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
#include "game.h"
#include "alpha.h"
#include "base.h"
#include "general.h"
#include "technology.h"
#include "faction.h"
#include "map.h"
#include "veh.h"

BOOL *ExpansionEnabled = (BOOL *)0x009A6488;
int *EnergyCredits = (int *)0x0093A958; // credits of the player at the controls as last shown (Thinker's name)
uint32_t *GamePreferences = (uint32_t *)0x009A6490;
uint32_t *GameMorePreferences = (uint32_t *)0x009A6494;
uint32_t *GameWarnings = (uint32_t *)0x009A6498;
uint32_t *GameRules = (uint32_t *)0x009A649C;
uint32_t *GameMoreRules = (uint32_t *)0x009A681C;
Monument *Monuments = (Monument *)0x0094CA08; // [8]
int *ControlUpkeepA = (int *)0x0093A934; // Thinker's name; selects which solar flare bit applies
uint32_t *SolarFlaresEvent = (uint32_t *)0x009A6818; // bits 0/1: energy yields tripled
uint32_t *GameState = (uint32_t *)0x009A64C0;
int *DiffLevelCurrent = (int *)0x009A64C4;
int *TurnCurrentNum = (int *)0x009A64D4;
uint32_t *ObjectiveReqVictory = (uint32_t *)0x0094B4C0;
uint32_t *ObjectivesSuddenDeathVictory = (uint32_t *)0x0094B4C4;
uint32_t *ObjectiveAchievePts = (uint32_t *)0x0094B4C8;
uint32_t *VictoryAchieveBonusPts = (uint32_t *)0x0094B4CC;
uint32_t *MissionYearCurrent = (uint32_t *)0x009A64D8;
uint32_t *StartingMissionYear = (uint32_t *)0x0094B4D0;
uint32_t *EndingMissionYear = (uint32_t *)0x0094B4D4;
uint32_t *TectonicDetonationCount = (uint32_t *)0x00946138; // [8]; value for each faction
int *SunspotDuration = (int *)0x009A6800;
uint32_t *MountPlanetX = (uint32_t *)0x009A6804;
uint32_t *MountPlanetY = (uint32_t *)0x009A6808;
int *DustCloudDuration = (int *)0x009A680C;
BOOL *IsMultiplayerNet = (BOOL *)0x0093F660; // DirectPlay - Serial, Modem, Internet (TCP/IP)
BOOL *IsMultiplayerPBEM = (BOOL *)0x0093A95C; // HotSeat / PBEM

/*
Purpose: Clear every faction's monument records (the "achieved" flag of each record and the
         trailing marker).
Original Offset: 00476B30
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch6.log)
*/
void __cdecl clear_monuments() {
    for (uint32_t faction_id = 0; faction_id < MaxPlayerNum; faction_id++) {
        for (int i = 0; i < MaxMonumentRecordNum; i++) {
            Monuments[faction_id].records[i].achieved = false;
        }
        Monuments[faction_id].record_latest = -1;
    }
}

/*
Purpose: Mark a monument record as achieved by the faction, counting how many factions got there
         first: a faction that is first gets a count of 1, otherwise the count is 0 and the first
         faction is the last one found holding a count (or first_default if none).
Original Offset: n/a (inlined in the mon_* functions)
Return Value: The record
Status: Complete
*/
static MonumentRecord *monument_achieve(int faction_id, int record_id, int first_default) {
    MonumentRecord *record = &Monuments[faction_id].records[record_id];
    record->faction_count = 0;
    for (int i = 0; i < MaxPlayerNum; i++) {
        if (i != faction_id && Monuments[i].records[record_id].achieved) {
            record->faction_count++;
        }
    }
    int faction_id_first = faction_id;
    if (!record->faction_count) {
        record->faction_count = 1;
    } else {
        record->faction_count = 0;
        faction_id_first = first_default;
        for (int i = 0; i < MaxPlayerNum; i++) {
            const MonumentRecord *other = &Monuments[i].records[record_id];
            if (i != faction_id && other->achieved && other->faction_count) {
                faction_id_first = i;
            }
        }
    }
    record->achieved = true;
    record->faction_id_first = faction_id_first;
    return record;
}

/*
Purpose: Finish a monument record update: note it as the latest and show the monument screen to
         the player at the controls.
Original Offset: n/a (inlined in the mon_* functions)
Return Value: n/a
Status: Complete
*/
static void monument_latest(int faction_id, int record_id) {
    Monuments[faction_id].record_latest = record_id;
    if (faction_id == *FactionIDCurrent) {
        monument(1);
    }
}

/*
Purpose: Record a non-alien faction's first base in its monument, with the base's name.
Original Offset: 00476B70
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch21.log)
*/
void __cdecl mon_colony_founded(int faction_id, LPCSTR base_name) {
    uint32_t year = game_year(*TurnCurrentNum);
    if ((Players[faction_id].rule_flags & RFLAG_ALIEN)
        || Monuments[faction_id].records[MONUMENT_COLONY_FOUNDED].achieved) {
        return;
    }
    monument_achieve(faction_id, MONUMENT_COLONY_FOUNDED, faction_id)->year = year;
    strncpy(Monuments[faction_id].colony_name, base_name, sizeof(Monuments->colony_name));
    monument_latest(faction_id, MONUMENT_COLONY_FOUNDED);
}

/*
Purpose: Record a faction's first technology discovery in its monument records: the year, the tech,
         how many other factions got there first (1 marks the first discoverer) and which faction
         was first. The player at the controls gets the monument screen.
Original Offset: 00476C90
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch6.log)
*/
void __cdecl mon_tech_discovered(int faction_id, int tech_id) {
    uint32_t year = game_year(*TurnCurrentNum);
    if ((Players[faction_id].rule_flags & RFLAG_ALIEN)
        || Monuments[faction_id].records[MONUMENT_TECH_DISCOVERED].achieved) {
        return;
    }
    MonumentRecord *record = &Monuments[faction_id].records[MONUMENT_TECH_DISCOVERED];
    record->faction_count = 0;
    for (int i = 0; i < MaxPlayerNum; i++) {
        if (i != faction_id && Monuments[i].records[MONUMENT_TECH_DISCOVERED].achieved) {
            record->faction_count++;
        }
    }
    int faction_id_first = faction_id;
    if (!record->faction_count) {
        record->faction_count = 1;
    } else {
        record->faction_count = 0;
        for (int i = 0; i < MaxPlayerNum; i++) {
            const MonumentRecord *other = &Monuments[i].records[MONUMENT_TECH_DISCOVERED];
            if (i != faction_id && other->achieved && other->faction_count) {
                faction_id_first = i;
            }
        }
    }
    record->achieved = true;
    record->year = year;
    record->faction_id_first = faction_id_first;
    record->data = tech_id;
    Monuments[faction_id].record_latest = MONUMENT_TECH_DISCOVERED;
    if (faction_id == *FactionIDCurrent) {
        monument(1);
    }
}

/*
Purpose: Record a non-alien faction's discoveries of the secrets of technology: the first sets the
         record (with its year), the second and third only note their years.
Original Offset: 00476DA0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch21.log)
*/
void __cdecl mon_secrets_of_tech(int faction_id) {
    uint32_t year = game_year(*TurnCurrentNum);
    if (Players[faction_id].rule_flags & RFLAG_ALIEN) {
        return;
    }
    MonumentRecord *record = &Monuments[faction_id].records[MONUMENT_SECRETS_OF_TECH];
    if (!record->achieved) {
        monument_achieve(faction_id, MONUMENT_SECRETS_OF_TECH, faction_id);
        record->year = year;
        record->data = -1; // year of the second secret
        record->unk_1 = -1; // year of the third secret
    } else if (record->data == -1) {
        record->data = year;
    } else if (record->unk_1 == -1) {
        record->unk_1 = year;
    }
    monument_latest(faction_id, MONUMENT_SECRETS_OF_TECH);
}

/*
Purpose: Record a non-alien faction's first prototype in its monument.
Original Offset: 00476EE0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch21.log)
*/
void __cdecl mon_prototype_built(int faction_id) {
    uint32_t year = game_year(*TurnCurrentNum);
    if ((Players[faction_id].rule_flags & RFLAG_ALIEN)
        || Monuments[faction_id].records[MONUMENT_PROTOTYPE_BUILT].achieved) {
        return;
    }
    monument_achieve(faction_id, MONUMENT_PROTOTYPE_BUILT, faction_id)->year = year;
    monument_latest(faction_id, MONUMENT_PROTOTYPE_BUILT);
}

/*
Purpose: Record a non-alien faction's first facility in its monument, with the base's name.
Original Offset: 00476FE0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch21.log)
*/
void __cdecl mon_facility_built(int faction_id, LPCSTR base_name) {
    uint32_t year = game_year(*TurnCurrentNum);
    if (Monuments[faction_id].records[MONUMENT_FACILITY_BUILT].achieved
        || (Players[faction_id].rule_flags & RFLAG_ALIEN)) {
        return;
    }
    monument_achieve(faction_id, MONUMENT_FACILITY_BUILT, faction_id)->year = year;
    strncpy(Monuments[faction_id].facility_base_name, base_name, sizeof(Monuments->facility_base_name));
    monument_latest(faction_id, MONUMENT_FACILITY_BUILT);
}

/*
Purpose: Record a non-alien faction's first secret project in its monument.
Original Offset: 00477100
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch21.log)
*/
void __cdecl mon_secret_project(int faction_id, int project_id) {
    uint32_t year = game_year(*TurnCurrentNum);
    if ((Players[faction_id].rule_flags & RFLAG_ALIEN)
        || Monuments[faction_id].records[MONUMENT_SECRET_PROJECT].achieved) {
        return;
    }
    MonumentRecord *record = monument_achieve(faction_id, MONUMENT_SECRET_PROJECT, faction_id);
    record->year = year;
    record->data = project_id;
    monument_latest(faction_id, MONUMENT_SECRET_PROJECT);
}

/*
Purpose: Record a non-alien faction's first destruction of an enemy (not the natives) in its
         monument; when others got there first and none holds a count, the enemy stands in as
         the first.
Original Offset: 00477210
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch21.log)
*/
void __cdecl mon_enemy_destroyed(int faction_id, int faction_id_enemy) {
    uint32_t year = game_year(*TurnCurrentNum);
    if ((Players[faction_id].rule_flags & RFLAG_ALIEN)
        || Monuments[faction_id].records[MONUMENT_ENEMY_DESTROYED].achieved || !faction_id_enemy) {
        return;
    }
    MonumentRecord *record = monument_achieve(faction_id, MONUMENT_ENEMY_DESTROYED, faction_id_enemy);
    record->year = year;
    record->data = faction_id_enemy;
    monument_latest(faction_id, MONUMENT_ENEMY_DESTROYED);
}

/*
Purpose: Record a non-alien faction's first conquered base in its monument, with the base's name.
Original Offset: 00477320
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch21.log)
*/
void __cdecl mon_conquer_base(int faction_id, LPCSTR base_name) {
    uint32_t year = game_year(*TurnCurrentNum);
    if ((Players[faction_id].rule_flags & RFLAG_ALIEN)
        || Monuments[faction_id].records[MONUMENT_CONQUER_BASE].achieved) {
        return;
    }
    monument_achieve(faction_id, MONUMENT_CONQUER_BASE, faction_id)->year = year;
    strncpy(Monuments[faction_id].conquered_base_name, base_name, sizeof(Monuments->conquered_base_name));
    monument_latest(faction_id, MONUMENT_CONQUER_BASE);
}

/*
Purpose: Record a non-alien faction's first achievement of a simple monument record (no extra data).
Original Offset: n/a (the body of the following mon_* functions)
Return Value: n/a
Status: Complete
*/
static void monument_simple(int faction_id, int record_id) {
    uint32_t year = game_year(*TurnCurrentNum);
    if ((Players[faction_id].rule_flags & RFLAG_ALIEN)
        || Monuments[faction_id].records[record_id].achieved) {
        return;
    }
    monument_achieve(faction_id, record_id, faction_id)->year = year;
    monument_latest(faction_id, record_id);
}

/*
Purpose: Record a non-alien faction's first naval unit in its monument.
Original Offset: 00477440
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch21.log)
*/
void __cdecl mon_naval_unit_built(int faction_id) {
    monument_simple(faction_id, MONUMENT_NAVAL_UNIT_BUILT);
}

/*
Purpose: Record a non-alien faction's first air unit in its monument.
Original Offset: 00477540
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch21.log)
*/
void __cdecl mon_air_unit_built(int faction_id) {
    monument_simple(faction_id, MONUMENT_AIR_UNIT_BUILT);
}

/*
Purpose: Record a non-alien faction's first bred native life form in its monument.
Original Offset: 00477640
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch21.log)
*/
void __cdecl mon_native_life_bred(int faction_id) {
    monument_simple(faction_id, MONUMENT_NATIVE_LIFE_BRED);
}

/*
Purpose: Record a non-alien faction's first step into space in its monument.
Original Offset: 00477740
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch21.log)
*/
void __cdecl mon_first_in_space(int faction_id) {
    monument_simple(faction_id, MONUMENT_FIRST_IN_SPACE);
}

/*
Purpose: Record a non-alien faction's first nature preserve in its monument.
Original Offset: 00477840
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch21.log)
*/
void __cdecl mon_built_preserve(int faction_id) {
    monument_simple(faction_id, MONUMENT_BUILT_PRESERVE);
}

/*
Purpose: Record a faction wiped out by this one in the first free killed-faction slot of its
         monument (six slots). Aliens get the record too, but never the monument screen.
Original Offset: 00477A60
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch21.log)
*/
void __cdecl mon_killed_faction(int faction_id, int faction_id_killed) {
    int record_id = -1;
    uint32_t year = game_year(*TurnCurrentNum);
    for (int i = 0; i < 6; i++) {
        if (!Monuments[faction_id].records[MONUMENT_KILLED_FACTION + i].achieved && record_id == -1) {
            record_id = MONUMENT_KILLED_FACTION + i;
        }
    }
    if (record_id == -1) {
        return;
    }
    MonumentRecord *record = &Monuments[faction_id].records[record_id];
    record->year = year;
    record->data = faction_id_killed;
    record->faction_count = 1;
    record->achieved = true;
    record->faction_id_first = faction_id;
    Monuments[faction_id].record_latest = record_id;
    if (!(Players[faction_id].rule_flags & RFLAG_ALIEN) && faction_id == *FactionIDCurrent) {
        monument(1);
    }
}

/*
Purpose: Let every fungal tower spread fungus: the tower's own tile and, one turn in five, a random
         neighbour turn to fungus, losing any roads, mines, farms and the like, and every faction
         sees the change. Towers are marked as having moved, get their morale refreshed and
         reveal their surroundings.
Original Offset: 00522F00
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch19.log)
*/
void __cdecl do_fungal_towers() {
    const uint32_t cleared = BIT_ROAD | BIT_MAGTUBE | BIT_MINE | BIT_SOLAR_TIDAL | BIT_FARM
        | BIT_SOIL_ENRICHER | BIT_FOREST;
    for (int veh_id = *VehCurrentCount - 1; veh_id >= 0; veh_id--) {
        Veh *veh = &Vehs[veh_id];
        if (veh->proto_id != BSC_FUNGAL_TOWER) {
            continue;
        }
        veh->state |= VSTATE_UNK_4;
        int x = veh->x;
        int y = veh->y;
        if (!(_rand() % 5)) {
            int dir = _rand() & 7;
            int x_spread = x + RadiusOffsetX[dir + 1];
            if (!*MapIsFlat) {
                if (x_spread < 0) {
                    x_spread += *MapLongitudeBounds;
                } else if (x_spread >= (int)*MapLongitudeBounds) {
                    x_spread -= *MapLongitudeBounds;
                }
            }
            int y_spread = y + RadiusOffsetY[dir + 1];
            if (y_spread >= 0 && y_spread < (int)*MapLatitudeBounds && x_spread >= 0
                && x_spread < (int)*MapLongitudeBounds) {
                bit_set(x_spread, y_spread, BIT_FUNGUS, true);
                bit_set(x_spread, y_spread, cleared, false);
                for (uint32_t faction_id = 1; faction_id < MaxPlayerNum; faction_id++) {
                    synch_bit(x_spread, y_spread, faction_id);
                }
                draw_tile(x_spread, y_spread, 2);
            }
        }
        bit_set(x, y, BIT_FUNGUS, true);
        bit_set(x, y, cleared, false);
        for (uint32_t faction_id = 1; faction_id < MaxPlayerNum; faction_id++) {
            synch_bit(x, y, faction_id);
        }
        draw_tile(x, y, 2);
        veh->morale = (uint8_t)morale_alien(veh_id, 0);
        spot_all(veh_id, 1);
    }
}

/*
Purpose: Start-of-turn upkeep for a faction's units: reset their per-turn counters and flags, count
         down timed sentry/hold orders, lift the "needs cover" flag at a base, bunker or airbase,
         hand fungal towers to the natives, and repair damage. Units that didn't move repair one
         point per reactor level, more on friendly territory, at a bunker or airbase, on fungus
         (natives) and in a base; a base with the matching facility or project, a breeding base
         for natives, or the Nano Factory repairs fully, and a repair unit in the stack doubles
         it. Out in the field units only heal down to two points per reactor level, unless they
         are natives on fungus or the faction owns the Xenoempathy Dome there. A human unit
         sentried until healed wakes up. Finally redraws the visible units.
Original Offset: 00526030
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch18.log)
*/
void __cdecl repair_phase(uint32_t faction_id) {
    PlayersData[faction_id].sat_odp_deployed = 0;
    for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
        Veh *veh = &Vehs[veh_id];
        if (veh->faction_id != faction_id) {
            continue;
        }
        int x = veh->x;
        int y = veh->y;
        uint8_t damage_before = veh->dmg_incurred;
        veh->unk_6 = 0;
        veh->moves_expended = 0;
        veh->state &= ~(VSTATE_UNK_2 | VSTATE_UNK_2000 | VSTATE_CRAWLING);
        veh->flags &= ~VFLAG_UNK_1000;
        if (!((*TurnCurrentNum + veh_id) & 3)) {
            veh->state &= ~VSTATE_UNK_800;
            veh->flags &= (veh->flags & VFLAG_UNK_2) ? ~VFLAG_UNK_2 : ~VFLAG_UNK_1;
        }
        if ((veh->order == ORDER_SENTRY_BOARD || veh->order == ORDER_HOLD) && veh->waypoint_y[0]
            && !--veh->waypoint_y[0]) {
            veh->order = ORDER_NONE;
        }
        Map *tile = map_loc(x, y);
        uint8_t triad = Chassis[VehPrototypes[veh->proto_id].chassis_id].triad;
        if ((veh->state & VSTATE_UNK_8)
            && (((tile->bit & BIT_BASE_IN_TILE) && (tile->val2 & 0xF) < 8)
                || (triad == TRIAD_LAND && (tile->bit & BIT_BUNKER))
                || (triad == TRIAD_AIR && (tile->bit & BIT_AIRBASE)))) {
            veh->state &= ~VSTATE_UNK_8;
        }
        if (veh->proto_id == BSC_FUNGAL_TOWER) {
            veh->faction_id = 0;
        }
        int proto_id = veh->proto_id;
        if (proto_id == BSC_BATTLE_OGRE_MK1 || proto_id == BSC_BATTLE_OGRE_MK2
            || proto_id == BSC_BATTLE_OGRE_MK3
            || (proto_id != BSC_FUNGAL_TOWER && (veh->state & VSTATE_UNK_4)) || !veh->dmg_incurred) {
            continue;
        }
        BOOL native = Weapon[VehPrototypes[proto_id].weapon_id].offense_rating < 0
            && proto_id < MaxVehProtoFactionNum;
        int value = 1;
        if (proto_id != BSC_FUNGAL_TOWER) {
            if (native && (tile->bit & BIT_FUNGUS) && (tile->climate & 0xE0) > 0x3F) {
                value = 2;
            }
            if (whose_territory(faction_id, x, y, NULL, false) == (int)faction_id) {
                value++;
            }
            if (triad == TRIAD_AIR && (tile->bit & BIT_AIRBASE)) {
                value++;
            }
            if (triad == TRIAD_LAND && (tile->bit & BIT_BUNKER)) {
                value++;
            }
        }
        int base_id = base_at(x, y);
        if (base_id >= 0 && !(Bases[base_id].state & BSTATE_DRONE_RIOTS_ACTIVE)) {
            value++;
            if (native) {
                if (breed_mod(base_id, Bases[base_id].faction_id_current)) {
                    value = veh->dmg_incurred;
                }
            } else {
                int facility_id, sp_base;
                if (triad == TRIAD_SEA) {
                    facility_id = FAC_NAVAL_YARD;
                    sp_base = SecretProject->maritime_control_center;
                } else if (triad == TRIAD_AIR) {
                    facility_id = FAC_AEROSPACE_COMPLEX;
                    sp_base = SecretProject->cloudbase_academy;
                } else {
                    facility_id = FAC_COMMAND_CENTER;
                    sp_base = SecretProject->command_nexus;
                }
                if (has_fac_built(facility_id, base_id)
                    || (sp_base >= 0 && Bases[sp_base].faction_id_current == faction_id)) {
                    value = veh->dmg_incurred;
                }
            }
        }
        BOOL repair_unit = false;
        if (triad == TRIAD_LAND && veh_cargo(veh_id)
            && (veh->order == ORDER_SENTRY_BOARD || (tile->climate & 0xE0) < ALT_BIT_SHORE_LINE)) {
            for (int i = veh_top(veh_id); i >= 0; i = Vehs[i].next_veh_id_stack) {
                if (i != veh_id && has_abil(Vehs[i].proto_id, ABL_REPAIR)) {
                    repair_unit = true;
                }
            }
            if (repair_unit) {
                value *= 2;
            }
        }
        BOOL nano_factory = SecretProject->nano_factory >= 0
            && Bases[SecretProject->nano_factory].faction_id_current == faction_id;
        if (nano_factory) {
            value = veh->dmg_incurred;
        }
        int reactor = VehPrototypes[veh->proto_id].reactor_id;
        int repair = reactor * value;
        int limit = 0;
        if (faction_id && !repair_unit && base_id < 0 && !nano_factory) {
            limit = reactor * 2;
            if (reactor && (tile->bit & BIT_FUNGUS) && (tile->climate & 0xE0) > 0x3F) {
                if (native) {
                    limit = 0;
                }
                if (SecretProject->xenoempathy_dome >= 0
                    && Bases[SecretProject->xenoempathy_dome].faction_id_current == faction_id) {
                    limit = 0;
                    repair += reactor;
                }
            }
        }
        int damage = veh->dmg_incurred;
        int damage_new = damage - repair;
        if (damage_new < limit) {
            damage_new = limit;
        } else if (damage_new > 999) {
            damage_new = 999;
        }
        if (damage_new >= damage) {
            damage_new = damage;
        }
        veh->dmg_incurred = (uint8_t)damage_new;
        if ((damage_new & 0xFF) <= limit && is_human(faction_id) && veh->order == ORDER_SENTRY_BOARD
            && (damage_new & 0xFF) != damage_before
            && (triad != TRIAD_LAND || (tile->climate & 0xE0) >= ALT_BIT_SHORE_LINE)) {
            veh->order = ORDER_NONE;
        }
    }
    for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
        Veh *veh = &Vehs[veh_id];
        if (veh->faction_id != faction_id || base_who(veh->x, veh->y) >= 0) {
            continue;
        }
        if (veh->faction_id == (uint32_t)*FactionIDCurrent
            || (veh->visibility & (1 << *FactionIDCurrent))) {
            draw_tile(veh->x, veh->y, -1);
        }
    }
    do_all_draws();
}

/*
Purpose: Set a faction's energy allocation for the turn. A human player at the controls with few
         credits is reminded to adjust the sliders or build infrastructure. An AI faction raises
         psych spending while bases riot (up to a cap set by its police rating) and lowers it
         when calm, then sets labs from its research drive, the turn, its credits and its regional
         plans, shifts between economy and labs by how its bases' energy compares with
         facility upkeep, and finally balances labs against economy by its efficiency.
Original Offset: 005267B0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch18.log).
        The original checks the current base, not each of the faction's bases, for riots and
        spare specialists; that is kept.
*/
void __cdecl allocate_energy(uint32_t faction_id) {
    PlayerData *p = &PlayersData[faction_id];
    if (is_human(faction_id)) {
        if (p->diff_level < DLVL_TALENT || faction_id != (uint32_t)*FactionIDCurrent
            || (int)p->current_num_bases < 2 || p->energy_reserves > 99
            || p->energy_reserves * 11 - *EnergyCredits * 10 >= 0) {
            return;
        }
        if (10 - p->se_alloc_labs - p->se_alloc_psych < 5) {
            if (popp(ScriptTxtID, "INFRASTRUCTURE", 0, "genwarning_sm.pcx", 0)) {
                social_select(faction_id);
            }
            return;
        }
        if (!has_tech(VehPrototypes[BSC_FORMERS].preq_tech, faction_id) || *TurnCurrentNum % 5) {
            return;
        }
        popp(ScriptTxtID, "INFRASTRUCTURE2", 0, "genwarning_sm.pcx", 0);
        return;
    }
    int economy_before = 10 - p->se_alloc_psych - p->se_alloc_labs;
    int police = p->soc_effect_pending.police;
    int psych_cap = police < -2 ? 4 : (police == -2 ? 2 : (police < 2 ? 1 : 0));
    BOOL settled = true;
    int stage = 0; // 0: maybe ease psych, 1: settle the sliders, 2: quell riots base by base
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        if (Bases[base_id].faction_id_current != faction_id) {
            continue;
        }
        if ((*BaseCurrent)->state & BSTATE_DRONE_RIOTS_ACTIVE) {
            stage = (psych_cap > p->se_alloc_psych && ++p->se_alloc_psych < psych_cap) ? 2 : 1;
            break;
        }
        if ((*BaseCurrent)->specialist_unk_1 < (*BaseCurrent)->specialist_total) {
            settled = false;
        }
    }
    if (!stage && settled && psych_cap / 2 < p->se_alloc_psych) {
        p->se_alloc_psych--;
    }
    while (stage == 2) {
        stage = 1;
        if (p->soc_effect_pending.police >= -2) {
            break;
        }
        for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
            if (Bases[base_id].faction_id_current != faction_id) {
                continue;
            }
            set_base(base_id);
            base_compute(true);
            Base *base = *BaseCurrent;
            if (base->talent_total < base->drone_total && (base->state & BSTATE_DRONE_RIOTS_ACTIVE)) {
                if (++p->se_alloc_psych < psych_cap) {
                    stage = 2;
                }
                break;
            }
        }
    }
    int psych = p->se_alloc_psych;
    int psych_min = p->soc_effect_pending.police > -2 ? 0 : 2;
    p->se_alloc_psych = psych < psych_min ? psych_min : (psych < 5 ? psych : 4);
    if (SecretProject->telepathic_matrix >= 0
        && Bases[SecretProject->telepathic_matrix].faction_id_current == faction_id) {
        p->se_alloc_psych = 0;
    }
    int labs = (p->ai_tech - p->ai_power - p->se_alloc_psych + 11) / 2 - p->ai_wealth;
    p->se_alloc_labs = labs;
    if (*TurnCurrentNum < (p->ai_tech - p->ai_power - p->ai_wealth + 3) * 25) {
        p->se_alloc_labs = labs + 1;
    }
    p->se_alloc_labs += p->energy_reserves / ((Players[faction_id].rule_techcost
        * (p->ai_power + 1 + p->ai_wealth) + *TurnCurrentNum) * 2);
    int region;
    for (region = 1; region < MaxContinentNum; region++) {
        if (!p->region_total_bases[region]) {
            continue;
        }
        if (p->region_base_plan[region] == 2) {
            p->se_alloc_labs--;
            break;
        }
        if (!p->region_base_plan[region] && (p->ai_wealth || p->ai_power)) {
            p->se_alloc_labs--;
        }
    }
    if (p->ai_tech && !p->ai_wealth && !p->ai_power && region >= MaxContinentNum) {
        p->se_alloc_labs++;
    }
    if (economy_before && economy_before <= 10 - p->se_alloc_labs - p->se_alloc_psych) {
        int income = 0;
        for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
            if (Bases[base_id].faction_id_current != faction_id) {
                continue;
            }
            income += Bases[base_id].unk_10;
            for (int facility_id = 0; facility_id < FacilityRepStart; facility_id++) {
                if (has_fac_built(facility_id, base_id)) {
                    income -= fac_maint(facility_id, faction_id);
                }
            }
        }
        int quarter = ((p->ai_wealth * 2 + 4) - p->ai_tech) * (int)p->current_num_bases / 4;
        if (quarter < income) {
            while (economy_before <= 10 - p->se_alloc_labs - p->se_alloc_psych) {
                p->se_alloc_labs++;
            }
        } else if (income < quarter / 2) {
            p->se_alloc_labs--;
        }
    }
    if (p->tech_id_researching == TECH_TRANT) {
        p->se_alloc_labs--;
    }
    int cap = 10 - p->se_alloc_psych;
    if (energy_limit(faction_id) <= cap) {
        cap = energy_limit(faction_id);
    }
    labs = p->se_alloc_labs;
    p->se_alloc_labs = (labs < 0 || cap < 0) ? 0 : (labs <= cap ? labs : cap);
    while (energy_limit(faction_id) < 10 - p->se_alloc_labs - p->se_alloc_psych) {
        p->se_alloc_labs++;
    }
    int efficiency = p->soc_effect_temp.efficiency;
    int bases_few = 3 - (int)p->current_num_bases;
    int balance = (efficiency < 0 ? 0 : (efficiency > 99 ? 99 : efficiency))
        + (bases_few < 0 ? 0 : (bases_few > 3 ? 3 : bases_few)) + 1;
    labs = p->se_alloc_labs;
    p->se_alloc_labs = labs < 0 ? 0 : (labs > 10 ? 10 : labs);
    while (balance - p->se_alloc_labs - p->se_alloc_psych + 10 < p->se_alloc_labs) {
        p->se_alloc_labs--;
    }
    while (p->se_alloc_labs + balance < 10 - p->se_alloc_labs - p->se_alloc_psych) {
        p->se_alloc_labs++;
    }
}

/*
Purpose: Handle creation of pop-up message on Planetfall.
Original Offset: 00589180
Return Value: n/a
Status: Complete
*/
void __cdecl planetfall(uint32_t faction_id) {
    parse_set(Players[faction_id].is_leader_female, false);
    parse_says(0, Players[faction_id].adj_name_faction, -1, -1);
    parse_says(2, Players[faction_id].title_leader, -1, -1);
    parse_says(3, Players[faction_id].name_leader, -1, -1);
    parse_set(Players[faction_id].noun_gender, Players[faction_id].is_noun_plural);
    parse_says(1, Players[faction_id].noun_faction, -1, -1); // unused in script, leaving in for now
    char script_id[13];
    if (*TurnCurrentNum) { // shifted logic to top to fix nonexistent accelerated script ids
        parse_num(0, *TurnCurrentNum);
        for (uint32_t i = 0; i < MaxSecretProjectNum; i++) {
            if (has_project(i, faction_id)) { // script assumes at least one SP is built per faction
                parse_says(4, Facility[FAC_HUMAN_GENOME_PROJ + i].name, -1, -1);
            }
        }
        strcpy_s(script_id, 13, "PLANETFALL2"); // bug fix: changed to fixed id rather than concat 2
    } else if (is_alien_faction(faction_id) && !_stricmp(Players[faction_id].filename, "USURPER")) {
        strcpy_s(script_id, 13, "PLANETFALLX");
    } else if (!_stricmp(Players[faction_id].filename, "FUNGBOY")) {
        strcpy_s(script_id, 13, "PLANETFALLF");
    } else {
        strcpy_s(script_id, 13, "PLANETFALL");
    }
    X_pop(script_id, NULL);
}

/*
Purpose: Replace every standalone four digit number in a string with the current game year.
Original Offset: 005A9DB0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch3.log)
*/
void __cdecl yearmotize(LPSTR str) {
    for (LPSTR p = str; *p; p++) {
        if (strlen(p) < 4) {
            return;
        }
        if (isdigit(p[0]) && isdigit(p[1]) && isdigit(p[2]) && isdigit(p[3]) && !isdigit(p[4])
            && (p == str || !isdigit(p[-1]))) {
            char year[80];
            _itoa_s(game_year(*TurnCurrentNum), year, 80, 10);
            char digits[80] = "";
            strcat_s(digits, 80, year);
            p[0] = digits[0];
            p[1] = digits[1];
            p[2] = digits[2];
            p[3] = digits[3];
        }
    }
}

/*
Purpose: Clear and reset scenario game variables.
Original Offset: 005B38D0
Return Value: n/a
Status: Complete
*/
void __cdecl clear_scenario() {
    *ObjectiveReqVictory = 9999;
    *ObjectivesSuddenDeathVictory = 9999;
    *ObjectiveAchievePts = 0;
    *VictoryAchieveBonusPts = 0;
    *StartingMissionYear = Rules->normal_starting_year;
    *EndingMissionYear = *DiffLevelCurrent < DLVL_LIBRARIAN
        ? Rules->normal_end_year_low_three_diff : Rules->normal_end_year_high_three_diff;
}

/*
Purpose: Calculate game year from start date and turn number.
Original Offset: 005C89A0
Return Value: Game year
Status: Complete
*/
uint32_t __cdecl game_year(int turn) {
    return *StartingMissionYear + turn;
}

/*
Purpose: Craft game year string. Assume buffer of at least 80. Revisit and convert to std::string.
Original Offset: 005C89B0
Return Value: n/a
Status: Complete
*/
void __cdecl say_year(LPSTR output) {
    char year[80];
    _itoa_s(game_year(*TurnCurrentNum), year, 80, 10);
    strcat_s(output, 80, year);
}
