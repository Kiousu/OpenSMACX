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
#include "alpha.h"
#include "faction.h"
#include "game.h"
#include "map.h"
#include "strings.h"
#include "text.h"
#include "technology.h"
#include "veh.h"
#include "log.h"
#include "council.h"
#include "general.h"

RulesFacility *Facility = (RulesFacility *)0x009A4B68;
RulesCitizen *Citizen = (RulesCitizen *)0x00946020;
Base *Bases = (Base *)0x0097D040; // [512]
BaseSecretProject *SecretProject = (BaseSecretProject *)0x009A6514; // [64]
int *BaseIDCurrentSelected = (int *)0x00689370;
int *BaseComputeID = (int *)0x00689374; // base whose figures were last recomputed (Thinker: ComputeBaseID)
int *BaseCurrentCount = (int *)0x009A64CC;
int *BaseFindDist = (int *)0x0090EA04;
Base **BaseCurrent = (Base **)0x0090EA30;
uint32_t *ScnVictFacilityObj = (uint32_t *)0x009A6814;
uint32_t *BaseUpkeepStage = (uint32_t *)0x0090EA34;
uint32_t *BaseCurrentConvoyFrom = (uint32_t *)0x0090E904; // [4]
uint32_t *BaseCurrentConvoyTo = (uint32_t *)0x0090E93C; // [4]
int *BaseCurrentGrowthRate = (int *)0x0090E918;
uint32_t *BaseCurrentVehPacifismCount = (uint32_t *)0x0090E980;
int *BaseCurrentTalentsStage = (int *)0x0090E984; // [5] talents after each base_psych stage
int *BaseCurrentDronesStage = (int *)0x0090E920; // [5] drones after each base_psych stage
int *BaseCurrentSuperDronesStage = (int *)0x0090E94C; // [5] super drones after each stage
uint32_t *BaseCurrentForcesSupported = (uint32_t *)0x0090E8FC;
uint32_t *BaseCurrentForcesMaintCost = (uint32_t *)0x0090E91C;
BOOL *BaseBitsErrorActive = (BOOL *)0x0090EA38; // base_at() is reporting a stray base bit
int *BaseGovPickID = (int *)0x0090EA44; // [4] production item picked by base_reset() for each
                                        // governor priority (explore, discover, build, conquer)
int *BaseGovPickValue = (int *)0x0090EA54; // [4] value of each pick; see base_claimed()
int *BaseTerraformEnergy = (int *)0x0090E914; // energy_yield() of the base square, clamped to 0-99
int *BaseTerraformReduce = (int *)0x0090E998; // yield lost to the three-per-square caps; see resource_yield()
int *BaseTileFlags = (int *)0x0090E99C; // [25] availability flags of the radius tiles; see farm_compute()
uint32_t *BaseCurrentUsedTiles = (uint32_t *)0x0090EA00; // radius tiles the current base can't work
uint32_t *BaseCurrentForcesMaintCount = (uint32_t *)0x0090EA08; // units the current base pays for
int *BaseCurrentWorkersLeft = (int *)0x0090E938; // citizens base_yield() has still to place
int *BaseYieldNutrientMet = (int *)0x0090E900; // base_yield() found the base fed without bonuses
int *BaseCommerceImport = (int *)0x0090EA0C; // [8] energy the current base gains from each partner
int *BaseCommerceExport = (int *)0x0090E960; // [8] energy each partner gains from the current base

/*
Purpose: Check if the base already has a particular facility built or if it's in the queue.
Original Offset: 00421670
Return Value: Does base already have or planning on building facility? true/false
Status: Complete
*/
BOOL __cdecl has_fac(int facility_id, uint32_t base_id, int queue_count) {
    if (facility_id >= FacilityRepStart) {
        return false;
    }
    BOOL is_built = has_fac_built(facility_id, base_id);
    if (is_built || !queue_count) {
        return is_built;
    }
    if (queue_count <= 0 || queue_count > 10) { // added upper bounds check for queueCount
        return false;
    }
    for (int i = 0; i < queue_count; i++) {
        if (Bases[base_id].queue_production_id[i] == -facility_id) {
            return true;
        }
    }
    return false;
}

/*
Purpose: Set the current base globals.
Original Offset: 004E39D0
Return Value: n/a
Status: Complete
*/
void __cdecl set_base(uint32_t base_id) {
    *BaseIDCurrentSelected = base_id;
    *BaseCurrent = &Bases[base_id];
}

/*
Purpose: Get base name string from base id and store it in base_str. If base id is -1, use 'NONE'.
Original Offset: 004E3A00
Return Value: n/a
Status: Complete
*/
void __cdecl say_base(LPSTR base_str, int base_id) {
    std::string output = (base_id >= 0) ? Bases[base_id].name_string : label_get(25); // 'NONE'
    // assumes 1032 char buffer via stringTemp, eventually remove
    strcat_s(base_str, 1032, output.c_str());
}

/*
Purpose: Find the base on the specified tile. A tile flagged as holding a base without one is an
         error: it is logged, the base bits are rebuilt and, in debug mode, the player at the
         controls is told and the map is centered on the tile.
Original Offset: 004E3A50
Return Value: Base id, or -1 if the tile has no base or lies off the map
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch3.log)
*/
int __cdecl base_at(int x, int y) {
    if (y < 0 || y >= *MapLatitudeBounds || x < 0 || x >= *MapLongitudeBounds
        || !(bit_at(x, y) & BIT_BASE_IN_TILE)) {
        return -1;
    }
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (Bases[i].x == x && Bases[i].y == y) {
            return i;
        }
    }
    if (!*BaseBitsErrorActive) {
        log_say("Base Bits Error  (x, y)", x, y, 0);
    }
    if (!(*GameState & STATE_DEBUG_MODE)) {
        rebuild_base_bits();
        return -1;
    }
    if (!*BaseBitsErrorActive) {
        *BaseBitsErrorActive = true;
        if (*IsLoggingActive) {
            parse_num(0, x);
            parse_num(1, y);
            X_pop("BASEBIT", NULL);
        }
        *BaseBitsErrorActive = false;
        if (!*DialogToggle) {
            GetAsyncKeyState(VK_CONTROL); // the original discards the result
        }
        rebuild_base_bits();
        Console_focus(ConsoleVar, 0, x, y, *FactionIDCurrent);
    }
    return -1;
}

/*
Purpose: Find the base id closest to the specified coordinates.
Original Offset: 004E3B80
Return Value: Base id or -1 if not found 
Status: Complete
*/
int __cdecl base_find(int x, int y) {
    if (*BaseCurrentCount <= 0) {
        return -1;
    }
    int proximity = 9999;
    int base_id = -1;
    for (int i = 0; i < *BaseCurrentCount; i++) {
        int dist = vector_dist(x, y, Bases[i].x, Bases[i].y);
        if (dist <= proximity) {
            proximity = dist;
            base_id = i;
        }
    }
    if (base_id >= 0) {
        *BaseFindDist = proximity;
    }
    return base_id;
}

/*
Purpose: Find the base id closest to the specified coordinates owned by faction.
Original Offset: 004E3C60
Return Value: Base id or -1 if not found
Status: Complete
*/
int __cdecl base_find(int x, int y, uint32_t faction_id) {
    if (*BaseCurrentCount <= 0) {
        return -1;
    }
    int proximity = 9999;
    int base_id = -1;
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (Bases[i].faction_id_current == faction_id) {
            int dist = vector_dist(x, y, Bases[i].x, Bases[i].y);
            if (dist <= proximity) {
                proximity = dist;
                base_id = i;
            }
        }
    }
    if (base_id >= 0) {
        *BaseFindDist = proximity;
    }
    return base_id;
}

/*
Purpose: Find the base id closest to the specified coordinates meeting various conditional checks.
Original Offset: 004E3D50
Return Value: Base id or -1 if not found
Status: Complete
*/
int __cdecl base_find(int x, int y, int faction_id, int region, int faction_id_2,
                      int faction_id_3) {
    int proximity = 9999;
    int base_id = -1;
    *BaseFindDist = 9999; // difference from other two functions where this is reset at start
    if (*BaseCurrentCount <= 0) {
        return -1;
    }
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (region < 0 || region_at(Bases[i].x, Bases[i].y) == (uint32_t)region) {
            if (faction_id < 0 ? (faction_id_2 < 0 || Bases[i].faction_id_current != faction_id_2)
                : (faction_id == Bases[i].faction_id_current || (faction_id_2 == -2
                    ? has_treaty(faction_id, Bases[i].faction_id_current, DTREATY_PACT)
                    : (faction_id_2 >= 0 && faction_id_2 == Bases[i].faction_id_current)))) {
                if (faction_id_3 < 0 || Bases[i].faction_id_current == faction_id_3
                    || ((1 << faction_id_3) & Bases[i].visibility)) {
                    int dist = vector_dist(x, y, Bases[i].x, Bases[i].y);
                    if (dist <= proximity) {
                        proximity = dist;
                        base_id = i;
                    }
                }
            }
        }
    }
    if (base_id >= 0) {
        *BaseFindDist = proximity;
    }
    return base_id;
}

/*
Purpose: Find the best specialist available to the current base with more weight placed on psych.
Original Offset: 004E4020
Return Value: Best citizen id (always going to be 1, 4, or 6 based on default weights)
Status: Complete
*/
uint32_t  __cdecl best_specialist() {
    int current_bonus = -999;
    uint32_t citizen_id = 0;
    for (int i = 0; i < MaxSpecialistNum; i++) {
        if (has_tech(Citizen[i].preq_tech, (*BaseCurrent)->faction_id_current)) {
            uint32_t bonus = Citizen[i].psych_bonus * 3;
            if ((*BaseCurrent)->population_size >= (int)Rules->min_base_size_specialists) {
                bonus += Citizen[i].ops_bonus + Citizen[i].research_bonus;
            }
            if ((int)bonus > current_bonus) {
                current_bonus = bonus;
                citizen_id = i;
            }
        }
    }
    return citizen_id;
}

/*
Purpose: Generate a base name. Added some minor randomization for sea bases and ensuring offset gets
         increased for default base name. TODO: Eventually improve base name shuffling.
Original Offset: 004E4090
Return Value: n/a
Status: Complete
*/
void __cdecl name_base(uint32_t faction_id, LPSTR name_out, BOOL is_final, BOOL is_sea_base) {
    if (is_sea_base && !text_open(Players[faction_id].filename, "WATERBASES")) {
        uint32_t offset_sea = PlayersData[faction_id].base_sea_name_offset + 1;
        if (offset_sea > 1) {
            uint32_t total;
            for (total = 0; ; total++) {
                text_get();
                if (!strlen(*TextBufferGetPtr) || !_strnicmp(*TextBufferGetPtr, "#END", 4)) {
                    break;
                }
            }
            if (offset_sea <= total) {
                int seed = ((*MapRandSeed + faction_id) & 0xFE) | 1;
                uint32_t loop = 1;
                do {
                    if (seed & 1) {
                        seed ^= 0x170;
                    }
                    seed >>= 1;
                } while (seed >= (int)total || ++loop != offset_sea);
                offset_sea = seed + 1;
            }
        }
        if (!text_open(Players[faction_id].filename, "WATERBASES")) {
            uint32_t count;
            for (count = 0; count < offset_sea; count++) {
                text_get();
                if (!strlen(*TextBufferGetPtr) || !_strnicmp(*TextBufferGetPtr, "#END", 4)) {
                    break;
                }
            }
            if (count == offset_sea) {
                // water base name available
                strncpy_s(name_out, 25, text_item(), 24);
                name_out[23] = 0;
                if (is_final) {
                    PlayersData[faction_id].base_sea_name_offset++;
                }
                text_close();
                return;
            }
        }
        text_close();
    }
    // Land base names or generic
    uint32_t offset = PlayersData[faction_id].base_name_offset + 1;
    sprintf_s(name_out, 25, "%s %d", get_noun(faction_id), offset); // default if names exhausted
    if (is_final) {
        PlayersData[faction_id].base_name_offset++;
    }
    if (!text_open(Players[faction_id].filename, "BASES")) {
        if (offset > 1) {
            uint32_t total;
            for (total = 0; ; total++) {
                text_get();
                if (!strlen(*TextBufferGetPtr) || !_strnicmp(*TextBufferGetPtr, "#END", 4)) {
                    break;
                }
            }
            if (offset <= total) {
                int seed = ((*MapRandSeed + faction_id) & 0xFE) | 1;
                uint32_t loop = 1;
                do {
                    if (seed & 1) {
                        seed ^= 0x170;
                    }
                    seed >>= 1;
                } while (seed >= (int)total || ++loop != offset);
                offset = seed + 1;
            }
        }
        if (!text_open(Players[faction_id].filename, "BASES")) {
            uint32_t count;
            for (count = 0; count < offset; count++) {
                text_get();
                if (!strlen(*TextBufferGetPtr) || !_strnicmp(*TextBufferGetPtr, "#END", 4)) {
                    break;
                }
            }
            if (count != offset) {
                if (text_open("BASENAME", "GENERIC")) {
                    return;
                }
                while (count < offset) {
                    text_get();
                    if (!strlen(*TextBufferGetPtr) || !_strnicmp(*TextBufferGetPtr, "#END", 4)) {
                        return;
                    }
                    count++;
                }
            }
            strncpy_s(name_out, 25, text_item(), 24);
            name_out[23] = 0;
        }
        text_close();
    }
}

/*
Purpose: TBD
Original Offset: 004E4350
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl base_mark(uint32_t base_id) {
    int x = Bases[base_id].x;
    int y = Bases[base_id].y;
    int x_radius = 0;
    int y_radius = 0;
    uint32_t faction_id = Bases[base_id].faction_id_current;
    for (int i = 0; i < RadiusRange[3]; i++) {
        x_radius = xrange(x + RadiusOffsetX[i]);
        y_radius = y + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius)) {
            if (i < 21) {
                bit_set(x_radius, y_radius, BIT_BASE_RADIUS, true);
                using_set(x_radius, y_radius, faction_id); // rolled this into one conditional check
            } else {
                site_set(x_radius, y_radius, 0);
            }
        }
    }
    for (int f = 0; f < MaxPlayerNum; f++) {
        // TODO: verify if using last radius coordinates is best logic, seems odd
        // It would seem using the Base coordinates would be better suited versus radius
        del_site(f, AI_GOAL_COLONIZE, x_radius, y_radius, 3);
    }
}

/*
Purpose: Calculate the cost factor for the specified faction and resource type. Optional base param.
Original Offset: 004E4430
Return Value: Cost factor
Status: Complete
*/
int __cdecl cost_factor(uint32_t faction_id, uint32_t rsc_type, int base_id) {
    const uint32_t diff_cost_base[] = { 13, 12, 11, 10, 8, 7 };
    uint32_t factor = is_human(faction_id) ? 10 : diff_cost_base[*DiffLevelCurrent]
        - great_satan(FactionRankings[7], false)
        - (!*IsMultiplayerNet && is_human(FactionRankings[7]));
    uint32_t cost_multiplier = rsc_type ? Rules->mineral_cost_multi : Rules->nutrient_cost_multi;
    if (cost_multiplier != 10) {
        factor = (factor * cost_multiplier) / 10;
    }
    if (*MapSizePlanet == 1) {
        factor = (factor * 9) / 10;
    } else if (!*MapSizePlanet) {
        factor = (factor * 8) / 10;
    }
    if (rsc_type) {
        if (rsc_type == RSC_MINERALS) {
            switch (PlayersData[faction_id].soc_effect_pending.industry) {
              case -7:
              case -6:
              case -5:
              case -4:
              case -3:
                return (factor * 13 + 9) / 10;
              case -2:
                return (factor * 6 + 4) / 5;
              case -1:
                return (factor * 11 + 9) / 10;
              case 0:
                return factor;
              case 1:
                return (factor * 9 + 9) / 10;
              case 2:
                return (factor * 4 + 4) / 5;
              case 3:
                return (factor * 7 + 9) / 10;
              case 4:
                return (factor * 3 + 4) / 5;
              default:
                return (factor + 1) / 2;
            }
        } else {
            return factor; // Energy/PSI? Legacy code logic in case cost used these?
        }
    } else { // nutrient
        int growth_factor = PlayersData[faction_id].soc_effect_pending.growth;
        if (base_id >= 0) {
            if (has_fac_built(FAC_CHILDREN_CRECHE, base_id)) {
                growth_factor += 2; // +2 on growth scale
            }
            if (Bases[base_id].state & BSTATE_GOLDEN_AGE_ACTIVE) {
                growth_factor += 2;
            }
        }
        growth_factor = range(growth_factor, -2, 5);
        return (factor * (10 - growth_factor) + 9) / 10;
    }
}

/*
Purpose: Determine if the specified base has any restrictions around production item retooling.
Original Offset: 004E4700
Return Value: Fixed value (-1, 0, 1, 2, 3, -70) or productionID
Status: Complete
*/
int __cdecl base_making(int production_id, uint32_t base_id) {
    uint32_t retool = Rules->retool_strictness;
    int skn_off = facility_offset("Skunkworks");
    if ((has_fac_built(FAC_SKUNKWORKS, base_id) // has Skunkworks
        || (Players[Bases[base_id].faction_id_current].rule_flags & RFLAG_FREEPROTO // bug fix
            && skn_off >= 0 
            && has_tech(Facility[skn_off].preq_tech, Bases[base_id].faction_id_current)))
        && retool >= 1) { // don't override if retool strictness is already set to always free (0)
        retool = 1; // Skunkworks or FREEPROTO + prerequisite tech > 'Free in Category'
    }
    if (production_id < 0) { // facility or SP to build
        int queue_id = Bases[base_id].queue_production_id[0]; // current production item
        if (queue_id < 0) { // non-Veh
            queue_id = -queue_id;
            if (queue_id < FacilityRepStart && has_fac_built(queue_id, base_id)) {
                return -1; // facility completed outside normal process, no retool penalty to change
            }
        }
    }
    switch (retool) { // converted into switch to improve readability and performance
      case 0: // Always Free
        return 0;
      case 1: // Free in Category
        if (production_id >= 0) {
            return 0; // Veh
        }
        // SP (1), repeatable facility (2), non-repeatable facility (3)
        return (production_id > -FacilitySPStart) ? (production_id > -FacilityRepStart) + 2 : 1;
      case 2: // Free switching between SPs (default behavior)
        return (production_id <= -FacilitySPStart) ? -FacilitySPStart : production_id;
      case 3: // Never Free
        return production_id;
      default:
        return 1; // should never be reached unless problem with retool value
    }
}

/*
Purpose: Calculate the mineral loss if production were to changed at the specified base.
Original Offset: 004E4810
Return Value: Minerals that would be lost or 0 if not applicable.
Status: Complete
*/
int __cdecl base_lose_minerals(uint32_t base_id, int UNUSED(production_id)) {
    int min_accum;
    if (Rules->retool_pct_pen_prod_chg && is_human(Bases[base_id].faction_id_current)
        && base_making(Bases[base_id].production_id_last, base_id)
        != base_making(Bases[base_id].queue_production_id[0], base_id)
        && (min_accum = Bases[base_id].minerals_accumulated_2, 
            min_accum > (int)Rules->retool_exemption)) {
        return min_accum - (100 - Rules->retool_pct_pen_prod_chg)
            * (min_accum - Rules->retool_exemption) / 100 - Rules->retool_exemption;
    }
    return 0;
}

/*
Purpose: Set or unset the specified base's facility.
Original Offset: 004E48B0
Return Value: n/a
Status: Complete
*/
void __cdecl set_fac(uint32_t facility_id, uint32_t base_id, BOOL set) {
    uint32_t offset;
    uint32_t mask;
    bitmask(facility_id, &offset, &mask);
    if (set) {
        Bases[base_id].facilities_built[offset] |= mask;
    } else {
        Bases[base_id].facilities_built[offset] &= ~mask;
    }
}

/*
Purpose: Check whether the facility audio blurb announcement has played for the faction.
Original Offset: 004E4900
Return Value: Has the facility audio blurb played? true/false
Status: Complete
*/
BOOL __cdecl has_fac_announced(uint32_t faction_id, uint32_t facility_id) {
    if (facility_id > FacilitySPStart) {
        return true;
    }
    uint32_t offset, mask;
    bitmask(facility_id, &offset, &mask);
    return (PlayersData[faction_id].facility_announced[offset] & mask) != 0;
}

/*
Purpose: Set or unset whether a particular facility audio blurb has played for specified faction.
Original Offset: 004E4960
Return Value: n/a
Status: Complete
*/
void __cdecl set_fac_announced(uint32_t faction_id, uint32_t facility_id, BOOL set) {
    uint32_t offset;
    uint32_t mask;
    bitmask(facility_id, &offset, &mask);
    if (set) {
        PlayersData[faction_id].facility_announced[offset] |= mask;
    } else {
        PlayersData[faction_id].facility_announced[offset] &= ~mask;
    }
}

/*
Purpose: Determine what unit the specified base should start building 1st then add it to the queue.
Original Offset: 004E4AA0
Return Value: n/a
Status: Complete
*/
void __cdecl base_first(uint32_t base_id) {
    int priority = -1;
    uint32_t proto_id = BSC_SCOUT_PATROL;
    uint32_t faction_id = Bases[base_id].faction_id_current;
    for (int i = 0; i < MaxVehProtoNum; i++) {
        if (veh_avail(i, faction_id, base_id)) {
            if (i < MaxVehProtoFactionNum || VehPrototypes[i].flags & PROTO_TYPED_COMPLETE) {
                int compare = get_proto_defense_rating(i) * 32;
                uint32_t plan = VehPrototypes[i].plan;
                if (plan == PLAN_DEFENSIVE) {
                    compare *= 4;
                } else if (plan == PLAN_COMBAT) {
                    compare *= 3;
                } else if (plan == PLAN_RECONNAISANCE) {
                    compare *= 2;
                }
                if (get_proto_triad(i) != TRIAD_LAND) {
                    compare /= 4;
                }
                compare -= VehPrototypes[i].cost;
                if (compare >= priority) {
                    priority = compare;
                    proto_id = i;
                }
            }
        }
    }
    Bases[base_id].queue_production_id[0] = proto_id;
}

/*
Purpose: Reset the production of an AI faction's bases (all of them, or those in one region) through
         base_reset(). With skip_occupied set, bases whose tile holds a unit of a faction are left
         alone.
Original Offset: 004E6300
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch6.log)
*/
void __cdecl bases_reset(int region, uint32_t faction_id, BOOL skip_occupied) {
    if (is_human(faction_id)) {
        return;
    }
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (Bases[i].faction_id_current != faction_id) {
            continue;
        }
        Map *tile = map_loc(Bases[i].x, Bases[i].y);
        if (region >= 0 && tile->region != region) {
            continue;
        }
        if (skip_occupied && (tile->bit & BIT_VEH_IN_TILE) && (tile->val2 & 0xF) <= 7) {
            continue;
        }
        if (is_human(faction_id) && ((*BaseCurrent)->state & BSTATE_UNK_8000000000)) {
            continue; // unreachable for an AI faction; kept from the original
        }
        base_reset(i, 0);
    }
}

/*
Purpose: Calculate the new unit morale bonus modifier provided by the base and faction for a triad.
Original Offset: 004E6400
Return Value: Morale bonus modifier
Status: Complete
*/
uint32_t __cdecl morale_mod(uint32_t base_id, uint32_t faction_id, uint32_t triad) {
    uint32_t morale_modifier = 0;
    if (triad == TRIAD_LAND) {
        if (has_fac_built(FAC_COMMAND_CENTER, base_id) 
            || has_project(SP_COMMAND_NEXUS, faction_id)) {
            morale_modifier = 2;
        }
    } else if (triad == TRIAD_SEA) {
        if (has_fac_built(FAC_NAVAL_YARD, base_id)
            || has_project(SP_MARITIME_CONTROL_CENTER, faction_id)) {
            morale_modifier = 2;
        }
    } else if (triad == TRIAD_AIR) {
        if (has_fac_built(FAC_AEROSPACE_COMPLEX, base_id)
            || has_project(SP_CLOUDBASE_ACADEMY, faction_id)) {
            morale_modifier = 2;
        }
    }
    if (has_fac_built(FAC_BIOENHANCEMENT_CENTER, base_id)
        || has_project(SP_CYBORG_FACTORY, faction_id)) {
        morale_modifier += 2;
    }
    if (PlayersData[faction_id].soc_effect_pending.morale < -1) {
        morale_modifier /= 2;
    }
    return morale_modifier;
}

/*
Purpose: Calculate the new native unit lifecycle bonus modifier provided by a base and faction.
Original Offset: 004E65C0
Return Value: Lifecycle bonus
Status: Complete
*/
uint32_t __cdecl breed_mod(uint32_t base_id, uint32_t faction_id) {
    uint32_t lifecycle_modifier = has_project(SP_XENOEMPATYH_DOME, faction_id) ? 1 : 0;
    if (has_project(SP_PHOLUS_MUTAGEN, faction_id)) {
        lifecycle_modifier++;
    }
    if (has_project(SP_VOICE_OF_PLANET, faction_id)) {
        lifecycle_modifier++;
    }
    if (has_fac_built(FAC_CENTAURI_PRESERVE, base_id)) {
        lifecycle_modifier++;
    }
    if (has_fac_built(FAC_TEMPLE_OF_PLANET, base_id)) {
        lifecycle_modifier++;
    }
    if (has_fac_built(FAC_BIOLOGY_LAB, base_id)) {
        lifecycle_modifier++;
    }
    if (has_fac_built(FAC_BIOENHANCEMENT_CENTER, base_id)
        || has_project(SP_CYBORG_FACTORY, faction_id)) {
        lifecycle_modifier++;
    }
    return lifecycle_modifier;
}

/*
Purpose: Calculate the count of lifecycle/psi bonuses that are provided by a base and faction.
Original Offset: 004E6740
Return Value: Native life modifier count
Status: Complete
*/
uint32_t __cdecl worm_mod(uint32_t base_id, uint32_t faction_id) {
    uint32_t worm_modifier_count = breed_mod(base_id, faction_id);
    if (Players[faction_id].rule_psi) {
        worm_modifier_count++;
    }
    if (has_project(SP_DREAM_TWISTER, faction_id)) {
        worm_modifier_count++;
    }
    if (has_project(SP_NEURAL_AMPLIFIER, faction_id)) {
        worm_modifier_count++;
    }
    return worm_modifier_count;
}

// Facility test the yield functions inline: no base id check, so a base id of -1 reads the memory
// before the base array exactly like the original (the callers never let that matter).
static BOOL yield_fac(int base_id, uint32_t facility_id) {
    uint32_t offset, mask;
    bitmask(facility_id, &offset, &mask);
    return (Bases[base_id].facilities_built[offset] & mask) != 0;
}

// Planet rating as the fungus yields use it: positive ratings count as zero, negative ones down to -3.
static int yield_planet_penalty(uint32_t faction_id) {
    int planet = PlayersData[faction_id].soc_effect_pending.planet;
    if (planet < -3) {
        return -3;
    }
    return planet > 0 ? 0 : planet;
}

/*
Purpose: Work out which of the current base's 21 radius tiles it may farm. Each tile gets flags:
         1 outside the radius, 2 off the map or never seen, 4 holding a base, 8 holding a
         convoy-like stack or a visible unit of a faction without a treaty or pact, 0x10
         worked by a nearby base (its own, or at sea any faction's), 0x20 another faction's
         territory. Tiles with any flag are marked unavailable in BaseCurrentUsedTiles. During
         the AI upkeep pass, supply pods on land in the radius become scout, defend and attack
         goals.
Original Offset: 004E68B0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch13.log)
*/
void __cdecl farm_compute() {
    Base *base = *BaseCurrent;
    int x = base->x;
    int y = base->y;
    uint32_t faction_id = base->faction_id_current;
    for (int i = 0; i < 25; i++) {
        if (i >= BaseRadiusTiles) {
            BaseTileFlags[i] = 1;
            continue;
        }
        int x_radius = x + RadiusOffsetX[i];
        if (!*MapIsFlat) {
            if (x_radius < 0) {
                x_radius += *MapLongitudeBounds;
            } else if (x_radius >= (int)*MapLongitudeBounds) {
                x_radius -= *MapLongitudeBounds;
            }
        }
        int y_radius = y + RadiusOffsetY[i];
        if (y_radius < 0 || y_radius >= (int)*MapLatitudeBounds || x_radius < 0
            || x_radius >= (int)*MapLongitudeBounds) {
            BaseTileFlags[i] = 2;
            continue;
        }
        Map *tile = map_loc(x_radius, y_radius);
        if (*BaseUpkeepStage == 1 && goody_at(x_radius, y_radius)
            && (tile->climate & 0xE0) >= ALT_BIT_SHORE_LINE) {
            add_goal(faction_id, AI_GOAL_SCOUT, 1, x_radius, y_radius, -1);
            add_goal(faction_id, AI_GOAL_DEFEND, 1, x_radius, y_radius, -1);
            add_goal(faction_id, AI_GOAL_ATTACK, 1, x_radius, y_radius, -1);
        }
        if (!(PlayersData[faction_id].flags & PFLAG_MAP_REVEALED)
            && !(tile->visibility & (1 << faction_id))) {
            BaseTileFlags[i] = 2;
            continue;
        }
        BaseTileFlags[i] = 0;
        if ((tile->bit & BIT_BASE_IN_TILE) && (tile->val2 & 0xF) < 8) {
            BaseTileFlags[i] = 4;
        }
        int veh_id = stack_fix(veh_at(x_radius, y_radius));
        if (veh_id >= 0) {
            if (stack_check(veh_id, 9, 3, -1, -1)) {
                BaseTileFlags[i] |= 8;
            }
            while (Vehs[veh_id].prev_veh_id_stack >= 0) {
                veh_id = Vehs[veh_id].prev_veh_id_stack;
            }
            for (; veh_id >= 0; veh_id = Vehs[veh_id].next_veh_id_stack) {
                uint32_t faction_id_veh = Vehs[veh_id].faction_id;
                if (faction_id_veh != faction_id && (Vehs[veh_id].visibility & (1 << faction_id))
                    && !(PlayersData[faction_id].diplo_treaties[faction_id_veh]
                        & (DTREATY_PACT | DTREATY_TREATY))) {
                    BaseTileFlags[i] |= 8;
                }
            }
        }
        // veh_id is always negative here, so any foreign territory is flagged
        int owner = whose_territory(faction_id, x_radius, y_radius, NULL, false);
        if (owner >= 0 && (uint32_t)owner != faction_id
            && ((PlayersData[faction_id].diplo_treaties[owner] & (DTREATY_PACT | DTREATY_TREATY))
                || veh_id < 0 || Vehs[veh_id].faction_id != faction_id)) {
            BaseTileFlags[i] |= 0x20;
        }
    }
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        if (base_id == *BaseIDCurrentSelected) {
            continue;
        }
        Base *other = &Bases[base_id];
        if (vector_dist(x, y, other->x, other->y) >= 6) {
            continue;
        }
        for (int j = 0; j < BaseRadiusTiles; j++) {
            if (j && !(other->resource_sqr_radius & (1 << j))) {
                continue;
            }
            int x_worked = other->x + RadiusOffsetX[j];
            if (!*MapIsFlat) {
                if (x_worked < 0) {
                    x_worked += *MapLongitudeBounds;
                } else if (x_worked >= (int)*MapLongitudeBounds) {
                    x_worked -= *MapLongitudeBounds;
                }
            }
            int y_worked = other->y + RadiusOffsetY[j];
            if (y_worked < 0 || y_worked >= (int)*MapLatitudeBounds || x_worked < 0
                || x_worked >= (int)*MapLongitudeBounds) {
                continue;
            }
            if (other->faction_id_current != faction_id
                && (map_loc(x_worked, y_worked)->climate & 0xE0) >= ALT_BIT_SHORE_LINE) {
                continue;
            }
            if (vector_dist(x, y, x_worked, y_worked) >= 3) {
                continue;
            }
            for (int k = 0; k < BaseRadiusTiles; k++) {
                int x_radius = x + RadiusOffsetX[k];
                if (!*MapIsFlat) {
                    if (x_radius < 0) {
                        x_radius += *MapLongitudeBounds;
                    } else if (x_radius >= (int)*MapLongitudeBounds) {
                        x_radius -= *MapLongitudeBounds;
                    }
                }
                if (x_radius == x_worked && y + RadiusOffsetY[k] == y_worked) {
                    BaseTileFlags[k] |= 0x10;
                }
            }
        }
    }
    *BaseCurrentUsedTiles = 0;
    for (int i = 0; i < BaseRadiusTiles; i++) {
        if (BaseTileFlags[i]) {
            *BaseCurrentUsedTiles |= 1 << i;
        }
    }
}

/*
Purpose: Calculate the nutrients a tile yields for a faction (and base): base squares, boreholes,
         monoliths, fungus, land by rainfall (zero on rocky ground) with farms and the mine
         penalty, forests with tree farm facilities, ocean with kelp farms and the aquafarm, plus
         bonus resources, landmarks, soil enrichers and condensers, capped at two without the
         prerequisite tech (the excess goes to BaseTerraformReduce) and adjusted for bumper
         crops and famine. With assume_improved the fungus yield is skipped.
Original Offset: 004E6E50
Return Value: Nutrient yield
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch7.log)
*/
uint32_t __cdecl crop_yield(uint32_t faction_id, int base_id, uint32_t x, uint32_t y,
                            BOOL assume_improved) {
    Map *tile = map_loc(x, y);
    uint32_t bit = tile->bit;
    BOOL is_ocean = (tile->climate & 0xE0) < ALT_BIT_SHORE_LINE;
    BOOL has_base = (bit & BIT_BASE_IN_TILE) && (tile->val2 & 0xF) <= 7;
    BOOL bonus = bonus_at(x, y, 0) == 1;
    uint32_t bit2 = tile->bit2;
    BOOL landmark = ((bit2 & (BIT2_JUNGLE | BIT2_UNK_80000000)) == BIT2_JUNGLE && !is_ocean)
        || ((bit2 & (BIT2_FRESH | BIT2_UNK_80000000)) == BIT2_FRESH && is_ocean);
    int yield;
    if (has_base) {
        yield = ResourceInfo[RSCINFO_BASE_SQ].nutrients;
        if (bonus) {
            yield *= 2;
        }
        if (landmark) {
            yield++;
        }
        if (base_id < 0) {
            return yield;
        }
        if (yield_fac(base_id, FAC_RECYCLING_TANKS) || yield_fac(base_id, FAC_PRESSURE_DOME)) {
            yield += ResourceInfo[RSCINFO_RECYCLING_TANKS].nutrients;
        }
    } else if (bit & BIT_THERMAL_BORE) {
        yield = ResourceInfo[RSCINFO_BOREHOLE_SQ].nutrients;
        if (yield) {
            yield += ResourceInfo[RSCINFO_BONUS_SQ].nutrients;
        }
    } else if (bit & BIT_MONOLITH) {
        yield = ResourceInfo[RSCINFO_MONOLITH].nutrients;
        if (bonus) {
            yield += ResourceInfo[RSCINFO_BONUS_SQ].nutrients;
        }
        int base_id_harmonics = SecretProject->manifold_harmonics;
        if (base_id_harmonics >= 0 && Bases[base_id_harmonics].faction_id_current == faction_id
            && PlayersData[faction_id].soc_effect_pending.planet > 0) {
            yield++;
        }
    } else if ((bit & BIT_FUNGUS) && !assume_improved
        && (tile->climate & 0xE0) >= ALT_BIT_OCEAN_SHELF) {
        yield = PlayersData[faction_id].tech_fungus_nutrient + yield_planet_penalty(faction_id);
        if (yield < 0) {
            yield = 0;
        } else if (yield > 99) {
            yield = 99;
        }
        int base_id_harmonics = SecretProject->manifold_harmonics;
        if (base_id_harmonics >= 0 && Bases[base_id_harmonics].faction_id_current == faction_id
            && PlayersData[faction_id].soc_effect_pending.planet > 0) {
            yield++;
        }
    } else {
        BOOL mine_check = false;
        if (!is_ocean) {
            if (!(bit & BIT_FOREST)) {
                yield = (tile->climate & 0x18) >> 3;
                if ((tile->val3 >> 6) > 1) {
                    yield = 0;
                }
                if (bonus) {
                    yield += ResourceInfo[RSCINFO_BONUS_SQ].nutrients;
                }
                if (landmark) {
                    yield++;
                }
                if (yield < 0) {
                    yield = 0;
                }
                if ((bit & BIT_FARM) && (tile->val3 >> 6) < 2) {
                    yield += ResourceInfo[RSCINFO_IMPROVED_LAND].nutrients;
                }
                mine_check = true;
            } else {
                yield = (bonus ? ResourceInfo[RSCINFO_BONUS_SQ].nutrients : 0)
                    + ResourceInfo[RSCINFO_FOREST_SQ].nutrients;
                if (base_id >= 0) {
                    if (yield_fac(base_id, FAC_HYBRID_FOREST)) {
                        yield++;
                    }
                    if (yield_fac(base_id, FAC_TREE_FARM)) {
                        yield++;
                    }
                }
                if (landmark) {
                    yield++;
                }
            }
        } else {
            yield = ResourceInfo[RSCINFO_OCEAN_SQ].nutrients;
            if (bonus) {
                yield += ResourceInfo[RSCINFO_BONUS_SQ].nutrients;
            }
            if (landmark) {
                yield++;
            }
            if ((tile->climate & 0xE0) == ALT_BIT_OCEAN_SHELF || *ExpansionEnabled) {
                if (bit & BIT_FARM) {
                    yield += ResourceInfo[RSCINFO_IMPROVED_SEA].nutrients;
                    if (yield_fac(base_id, FAC_AQUAFARM)) {
                        yield++;
                    }
                }
                mine_check = true;
            }
        }
        if (mine_check && (bit & BIT_MINE) && yield > 1) {
            yield += Rules->tgl_nutrient_effect_with_mine;
        }
        if (bit & BIT_SOIL_ENRICHER) {
            yield += yield / 2;
        }
        BOOL condenser = (bit & BIT_CONDENSER) != 0;
        if (condenser) {
            yield += yield / 2;
        }
        if (yield > 2 && !has_tech(Rules->tech_three_nutrients_sqr, faction_id) && !bonus
            && !condenser) {
            *BaseTerraformReduce += yield - 2;
            yield = 2;
        }
    }
    if (base_id >= 0) {
        if (Bases[base_id].event & BEVENT_BUMPER) {
            yield++;
        }
        if ((Bases[base_id].event & BEVENT_FAMINE) && yield) {
            yield--;
        }
    }
    return yield;
}

/*
Purpose: Calculate the minerals a tile yields for a faction (and base): base squares, monoliths,
         boreholes, fungus, land by rockiness with mines (limited without a road) or unimproved
         rolling/rocky ground, forests, and ocean with mining platforms (shelf, or anywhere in the
         expansion), plus bonus resources and mineral landmarks, capped at two without the
         prerequisite tech and adjusted for industry booms and busts. With assume_improved the
         tile is treated as mined.
Original Offset: 004E7310
Return Value: Mineral yield
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch7.log)
*/
int __cdecl mine_yield(uint32_t faction_id, int base_id, uint32_t x, uint32_t y, BOOL assume_improved) {
    Map *tile = map_loc(x, y);
    int code = (int)tile->bit2 >> 24;
    uint32_t bit = tile->bit;
    BOOL bonus = bonus_at(x, y, 0) == 2;
    uint32_t bit2 = tile->bit2;
    BOOL landmark = ((bit2 & (BIT2_VOLCANO | BIT2_UNK_80000000)) == BIT2_VOLCANO && code < 9)
        || ((bit2 & (BIT2_CRATER | BIT2_UNK_80000000)) == BIT2_CRATER && code < 9)
        || ((bit2 & (BIT2_FOSSIL | BIT2_UNK_80000000)) == BIT2_FOSSIL && code < 6)
        || (bit2 & (BIT2_CANYON | BIT2_UNK_80000000)) == BIT2_CANYON;
    int yield = (bonus ? ResourceInfo[RSCINFO_BONUS_SQ].minerals : 0) + (landmark ? 1 : 0);
    if ((bit & BIT_BASE_IN_TILE) && (tile->val2 & 0xF) < 8) {
        yield += ResourceInfo[RSCINFO_BASE_SQ].minerals;
        if (base_id < 0) {
            return yield;
        }
        if (yield_fac(base_id, FAC_RECYCLING_TANKS) || yield_fac(base_id, FAC_PRESSURE_DOME)) {
            yield += ResourceInfo[RSCINFO_RECYCLING_TANKS].minerals;
        }
    } else if (bit & BIT_MONOLITH) {
        yield = ResourceInfo[RSCINFO_MONOLITH].minerals;
        if (bonus) {
            yield += ResourceInfo[RSCINFO_BONUS_SQ].minerals;
        }
        int base_id_harmonics = SecretProject->manifold_harmonics;
        if (base_id_harmonics >= 0 && Bases[base_id_harmonics].faction_id_current == faction_id
            && PlayersData[faction_id].soc_effect_pending.planet > 1) {
            yield++;
        }
    } else {
        BOOL capped = true;
        if (bit & BIT_THERMAL_BORE) {
            yield = ResourceInfo[RSCINFO_BOREHOLE_SQ].minerals;
            if (bonus) {
                yield += ResourceInfo[RSCINFO_BONUS_SQ].minerals;
            }
        } else if ((bit & BIT_FUNGUS) && (tile->climate & 0xE0) >= ALT_BIT_OCEAN_SHELF) {
            int planet = PlayersData[faction_id].soc_effect_pending.planet;
            yield = PlayersData[faction_id].tech_fungus_mineral + yield_planet_penalty(faction_id);
            if (yield < 0) {
                yield = 0;
            } else if (yield > 99) {
                yield = 99;
            }
            int base_id_harmonics = SecretProject->manifold_harmonics;
            if (base_id_harmonics >= 0 && Bases[base_id_harmonics].faction_id_current == faction_id
                && planet > 1) {
                yield++;
            }
            capped = false;
        } else if ((tile->climate & 0xE0) >= ALT_BIT_SHORE_LINE) {
            uint32_t rocky = tile->val3 >> 6;
            int add = ResourceInfo[RSCINFO_FOREST_SQ].minerals;
            if (!(bit & BIT_FOREST)) {
                if (!(bit & (BIT_THERMAL_BORE | BIT_MINE)) && !assume_improved) {
                    if (rocky) {
                        yield++;
                    }
                    add = 0;
                } else {
                    yield += rocky;
                    if (!rocky) {
                        rocky = 1;
                    }
                    if (bonus || landmark) {
                        rocky++;
                    }
                    add = rocky;
                    if (!(bit & BIT_ROAD) && !assume_improved
                        && (int)Rules->limit_mineral_mine_sans_road < (int)rocky) {
                        *BaseTerraformReduce += rocky - Rules->limit_mineral_mine_sans_road;
                        add = Rules->limit_mineral_mine_sans_road;
                    }
                }
            }
            yield += add;
        } else {
            yield += ResourceInfo[RSCINFO_OCEAN_SQ].minerals;
            BOOL is_shelf = (tile->climate & 0xE0) == ALT_BIT_OCEAN_SHELF;
            if ((Players[Bases[base_id].faction_id_current].rule_flags & RFLAG_AQUATIC) && is_shelf) {
                yield++;
            }
            if ((is_shelf || *ExpansionEnabled) && ((bit & BIT_MINE) || assume_improved)) {
                yield += ResourceInfo[RSCINFO_IMPROVED_SEA].minerals;
                if (has_tech(Rules->tech_mining_platform_bonus, faction_id)) {
                    yield++;
                }
                if (yield_fac(base_id, FAC_SUBSEA_TRUNKLINE)) {
                    yield++;
                }
            }
        }
        if (capped && yield > 2 && !has_tech(Rules->tech_three_minerals_sqr, faction_id) && !bonus) {
            *BaseTerraformReduce += yield - 2;
            yield = 2;
        }
    }
    if (base_id >= 0) {
        if (Bases[base_id].event & BEVENT_INDUSTRY) {
            yield++;
        }
        if ((Bases[base_id].event & BEVENT_BUST) && yield) {
            yield--;
        }
    }
    return yield;
}

/*
Purpose: Calculate the energy a tile yields for a faction (and base): base squares by economy
         rating, headquarters, golden ages and the governorship; monoliths, boreholes, fungus,
         land by altitude with solar collectors (plus adjacent echelon mirrors of the current
         base's owner), forests, and ocean with tidal harnesses; plus rivers, bonus resources,
         energy landmarks and the Merchant Exchange; capped at two without the prerequisite tech
         (base squares are clamped to 0-99 and recorded in BaseTerraformEnergy); adjusted for
         heat waves, cloud cover, dust clouds and tripled during solar flares.
Original Offset: 004E7750
Return Value: Energy yield
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch7.log)
*/
uint32_t __cdecl energy_yield(uint32_t faction_id, int base_id, int x, uint32_t y,
                              BOOL assume_improved) {
    Map *tile = map_loc(x, y);
    uint32_t bit = tile->bit;
    uint32_t bit2 = tile->bit2;
    BOOL is_land = (tile->climate & 0xE0) >= ALT_BIT_SHORE_LINE;
    BOOL has_base = (bit & BIT_BASE_IN_TILE) && (tile->val2 & 0xF) <= 7;
    int bonus_type = bonus_at(x, y, 0);
    BOOL landmark = ((bit2 & (BIT2_VOLCANO | BIT2_UNK_80000000)) == BIT2_VOLCANO
            && ((int)bit2 >> 24) < 9)
        || (bit2 & (BIT2_URANIUM | BIT2_UNK_80000000)) == BIT2_URANIUM
        || (bit2 & (BIT2_GEOTHERMAL | BIT2_UNK_80000000)) == BIT2_GEOTHERMAL
        || (bit2 & (BIT2_RIDGE | BIT2_UNK_80000000)) == BIT2_RIDGE;
    int yield = 0;
    if (!has_base) {
        if (bit & BIT_MONOLITH) {
            yield = ResourceInfo[RSCINFO_MONOLITH].energy;
            int base_id_harmonics = SecretProject->manifold_harmonics;
            if (base_id_harmonics >= 0 && Bases[base_id_harmonics].faction_id_current == faction_id) {
                if (PlayersData[faction_id].soc_effect_pending.planet >= 0) {
                    yield++;
                }
                if (PlayersData[faction_id].soc_effect_pending.planet > 2) {
                    yield++;
                }
            }
        } else if (bit & BIT_THERMAL_BORE) {
            yield = ResourceInfo[RSCINFO_BOREHOLE_SQ].energy;
        } else if ((bit & BIT_FUNGUS) && (tile->climate & 0xE0) >= ALT_BIT_OCEAN_SHELF) {
            int planet = PlayersData[faction_id].soc_effect_pending.planet;
            yield = PlayersData[faction_id].tech_fungus_energy + yield_planet_penalty(faction_id);
            if (yield < 0) {
                yield = 0;
            } else if (yield > 99) {
                yield = 99;
            }
            int base_id_harmonics = SecretProject->manifold_harmonics;
            if (base_id_harmonics >= 0 && Bases[base_id_harmonics].faction_id_current == faction_id) {
                if (planet >= 0) {
                    yield++;
                }
                if (planet > 2) {
                    yield++;
                }
            }
            goto events;
        } else if (is_land) {
            if (!(bit & BIT_FOREST)) {
                int altitude = (tile->climate >> 5) - 2;
                if ((bit & (BIT_THERMAL_BORE | BIT_ECH_MIRROR | BIT_SOLAR_TIDAL)) || assume_improved) {
                    yield = altitude < 0 ? 0 : altitude;
                }
                if (bit & BIT_SOLAR_TIDAL) {
                    for (int i = 0; i < 8; i++) {
                        int x_radius = xrange(x + RadiusBaseX[i]);
                        int y_radius = y + RadiusBaseY[i];
                        if (y_radius >= 0 && y_radius < (int)*MapLatitudeBounds && x_radius >= 0
                            && x_radius < (int)*MapLongitudeBounds) {
                            Map *mirror = map_loc(x_radius, y_radius);
                            if ((mirror->bit & BIT_ECH_MIRROR)
                                && mirror->territory == (int)(*BaseCurrent)->faction_id_current) {
                                yield++;
                            }
                        }
                    }
                }
            } else {
                yield = ResourceInfo[RSCINFO_FOREST_SQ].energy;
                if (base_id >= 0 && yield_fac(base_id, FAC_HYBRID_FOREST)) {
                    yield = ResourceInfo[RSCINFO_FOREST_SQ].energy + 1;
                }
            }
        } else {
            if ((tile->climate & 0xE0) == ALT_BIT_OCEAN_SHELF || *ExpansionEnabled) {
                yield = ResourceInfo[RSCINFO_OCEAN_SQ].energy;
                if ((bit & BIT_SOLAR_TIDAL) || assume_improved) {
                    yield += ResourceInfo[RSCINFO_IMPROVED_SEA].energy;
                    if (yield_fac(base_id, FAC_THERMOCLINE_TRANSDUCER)) {
                        yield++;
                    }
                }
            }
            if (bit & BIT_FOREST) {
                yield--;
            }
        }
    } else {
        int base_sq = ResourceInfo[RSCINFO_BASE_SQ].energy;
        if (yield_fac(base_id, FAC_HEADQUARTERS)) {
            base_sq++;
        }
        yield = base_sq;
        int golden_age = 0;
        if (base_id >= 0) {
            if (yield_fac(base_id, FAC_RECYCLING_TANKS) || yield_fac(base_id, FAC_PRESSURE_DOME)) {
                yield = base_sq + ResourceInfo[RSCINFO_RECYCLING_TANKS].energy;
            }
            golden_age = (Bases[base_id].state & BSTATE_GOLDEN_AGE_ACTIVE) ? 1 : 0;
        }
        int economy = PlayersData[faction_id].soc_effect_pending.economy + golden_age;
        if (economy < 0) {
            if (!yield_fac(base_id, FAC_HEADQUARTERS) || economy < -1
                || PlayersData[faction_id].current_num_bases == 1) {
                economy++;
            }
        } else if (economy > 1) {
            if (economy > 4) {
                economy = 4;
            }
            economy = economy * 2 - 4;
        }
        yield += economy;
        if (*GovernorFactionID == (int)faction_id) {
            yield++;
        }
    }
    if ((bit & BIT_RIVER) && is_land) {
        yield++;
    }
    if (bonus_type == 3) {
        yield += ResourceInfo[RSCINFO_BONUS_SQ].energy;
    }
    if (landmark) {
        yield++;
    }
    {
        int golden_age = 0;
        if (base_id >= 0) {
            if (SecretProject->merchant_exchange == base_id) {
                yield++;
            }
            golden_age = (Bases[base_id].state & BSTATE_GOLDEN_AGE_ACTIVE) ? 1 : 0;
        }
        if (PlayersData[faction_id].soc_effect_pending.economy + golden_age > 1) {
            yield++;
        }
    }
    if (!has_base) {
        if (yield > 2 && !has_tech(Rules->tech_three_energy_sqr, faction_id) && bonus_type != 3) {
            *BaseTerraformReduce += yield - 2;
            yield = 2;
        }
    } else if (yield < 0) {
        yield = 0;
        *BaseTerraformEnergy = 0;
    } else {
        *BaseTerraformEnergy = yield;
        if (yield > 99) {
            yield = 99;
            *BaseTerraformEnergy = 99;
        }
    }
events:
    if (base_id >= 0) {
        if (Bases[base_id].event & BEVENT_HEAT_WAVE) {
            yield++;
        }
        if ((Bases[base_id].event & BEVENT_CLOUD_COVER) && yield) {
            yield--;
        }
    }
    if (*DustCloudDuration && yield) {
        yield--;
    }
    uint32_t flares = *ControlUpkeepA ? (*SolarFlaresEvent & 2) : (*SolarFlaresEvent & 1);
    return flares ? yield * 3 : yield;
}

/*
Purpose: Yield of a tile for one resource type: 0 nutrients, 1 minerals, 2 energy. Type 3 resets
         BaseTerraformReduce (the yield lost to the "three per square" caps).
Original Offset: 004E7DC0
Return Value: Yield (0 for type 3 and unknown types)
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch7.log)
*/
int __cdecl resource_yield(uint32_t rsc_type, uint32_t faction_id, int base_id, int x, int y) {
    switch (rsc_type) {
    case 0:
        return crop_yield(faction_id, base_id, x, y, false);
    case 1:
        return mine_yield(faction_id, base_id, x, y, false);
    case 2:
        return energy_yield(faction_id, base_id, x, y, false);
    case 3:
        *BaseTerraformReduce = 0;
        break;
    }
    return 0;
}

/*
Purpose: Get the specialist type in a slot of the current base. Only the first 16 slots are stored;
         beyond them the best available specialist is assumed.
Original Offset: n/a (inlined in base_yield)
Return Value: Citizen type
Status: Complete
*/
static uint32_t base_spec_get(int slot) {
    if (slot < 16) {
        return ((*BaseCurrent)->spec[slot >> 3] >> ((slot & 7) * 4)) & 0xF;
    }
    return best_specialist();
}

/*
Purpose: Set the specialist type in a slot of the current base; slots past 16 aren't stored.
Original Offset: n/a (inlined in base_yield)
Return Value: n/a
Status: Complete
*/
static void base_spec_set(int slot, uint32_t citizen_id) {
    if (slot < 16) {
        int shift = (slot & 7) * 4;
        (*BaseCurrent)->spec[slot >> 3] &= ~(0xFu << shift);
        (*BaseCurrent)->spec[slot >> 3] |= citizen_id << shift;
    }
}

/*
Purpose: Add (sign 1) or remove (sign -1) the yield of the radius tile at offset index i to the
         current base's intake, flagging the base as working the sea when the tile is ocean.
Original Offset: n/a (inlined in base_yield)
Return Value: n/a
Status: Complete
*/
static void base_yield_tile(int i, int sign) {
    Base *base = *BaseCurrent;
    int x = base->x + RadiusOffsetX[i];
    if (!*MapIsFlat) {
        if (x < 0) {
            x += *MapLongitudeBounds;
        } else if (x >= (int)*MapLongitudeBounds) {
            x -= *MapLongitudeBounds;
        }
    }
    int y = base->y + RadiusOffsetY[i];
    if (y < 0 || y >= (int)*MapLatitudeBounds || x < 0 || x >= (int)*MapLongitudeBounds) {
        return;
    }
    if ((map_loc(x, y)->climate & 0xE0) < ALT_BIT_SHORE_LINE) {
        (*BaseCurrent)->state |= BSTATE_UNK_8000;
    }
    for (int k = 0; k < 4; k++) {
        int yield = resource_yield(k, (*BaseCurrent)->faction_id_current, *BaseIDCurrentSelected,
            x, y);
        (&(*BaseCurrent)->nutrient_intake_2)[k] += sign * yield;
    }
}

/*
Purpose: Assign the current base's citizens to tiles and specialists. Orbital yields come first
         (halved without an aerospace complex, space elevator or cloudbase academy, capped by the
         population). A human base without an active governor keeps the tiles it works; otherwise
         the base tile is worked and the remaining workers go one at a time to the best-scoring
         free tile (or, for a large governed base late in the game, the best specialist), the
         score weighing nutrients, minerals and energy against what the base lacks and its
         faction's priorities. Once every worker is placed one worst/best tile swap is tried,
         reverted if the base would starve. Leftover workers become the best specialist. Ends by
         copying the intake and summing the radius terraforming into eco_damage.
Original Offset: 004E80B0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch14.log)
*/
void __cdecl base_yield() {
    farm_compute();
    Base *base = *BaseCurrent;
    uint32_t faction_id = base->faction_id_current;
    *BaseCurrentWorkersLeft = base->population_size + 1;
    uint32_t faction_bit = 1 << faction_id;
    BOOL governed = !(*FactionsStatus & faction_bit)
        || ((base->governor & GOV_MANAGES_CITIZENS_SPECS) && (base->governor & GOV_ACTIVE));
    uint32_t worked = 0;
    base->state &= ~BSTATE_UNK_8000;
    (*BaseCurrent)->specialist_unk_1 = 0;
    (*BaseCurrent)->resource_sqr_radius &= ~*BaseCurrentUsedTiles | 1;
    for (int k = 0; k < 4; k++) {
        int *intake = &(&(*BaseCurrent)->nutrient_intake_2)[k];
        if (k == 3) {
            *intake = 0;
            continue;
        }
        *intake = (&PlayersData[faction_id].satellites_nutrient)[k];
        if (!has_fac_built(FAC_AEROSPACE_COMPLEX, *BaseIDCurrentSelected)
            && (SecretProject->space_elevator < 0
                || Bases[SecretProject->space_elevator].faction_id_current != faction_id)
            && (SecretProject->cloudbase_academy < 0
                || Bases[SecretProject->cloudbase_academy].faction_id_current != faction_id)) {
            *intake = (*intake + 1) / 2;
        }
        int pop = (*BaseCurrent)->population_size;
        *intake = pop < *intake ? pop : *intake;
    }
    BOOL placed = false;
    if (!governed || !*ControlUpkeepA) {
        if (governed) {
            // set aside the specialists that do nothing for psych
            base = *BaseCurrent;
            for (int i = 0; i < base->specialist_total; i++) {
                if (!Citizen[base_spec_get(i)].psych_bonus) {
                    base->specialist_unk_1++;
                }
            }
            base->specialist_total -= base->specialist_unk_1;
        }
        base = *BaseCurrent;
        if ((int)bit_count(base->resource_sqr_radius) <= *BaseCurrentWorkersLeft) {
            for (int i = 0; i < BaseRadiusTiles; i++) {
                if (i && !((1 << i) & (*BaseCurrent)->resource_sqr_radius)) {
                    continue;
                }
                worked |= 1 << i;
                base_yield_tile(i, 1);
                if (!--*BaseCurrentWorkersLeft) {
                    break;
                }
            }
            base = *BaseCurrent;
            if (*BaseCurrentWorkersLeft == base->specialist_unk_1 + base->specialist_total) {
                placed = true;
            }
        }
    }
    if (!placed) {
        base = *BaseCurrent;
        if (governed && base->specialist_total) {
            for (int i = 0; i < base->specialist_unk_1 + base->specialist_total; i++) {
                if (Citizen[base_spec_get(i)].psych_bonus) {
                    continue;
                }
                for (int j = i; j < base->specialist_unk_1 + base->specialist_total; j++) {
                    uint32_t next = base_spec_get(j + 1);
                    base_spec_set(j, next);
                }
                if (base->specialist_unk_1) {
                    base->specialist_unk_1--;
                }
            }
        }
        if (!(worked & 1)) {
            base_yield_tile(0, 1);
            worked |= 1;
            (*BaseCurrentWorkersLeft)--;
        }
        BOOL human = (*FactionsStatus & faction_bit) != 0;
        *BaseYieldNutrientMet = 0;
        int growth, tech, wealth, power, fight;
        base = *BaseCurrent;
        if (human && (base->governor & GOV_ACTIVE)) {
            uint32_t gov = base->governor;
            tech = (gov >> 24) & 2;
            growth = (gov >> 24) & 1;
            wealth = (gov >> 25) & 2;
            power = (gov >> 26) & 2;
            if (power) {
                fight = 2;
            } else if (growth || wealth) {
                fight = 0;
            } else if (tech) {
                fight = -2;
            } else {
                fight = PlayersData[faction_id].ai_fight;
            }
        } else if (!human) {
            growth = PlayersData[faction_id].ai_growth;
            tech = PlayersData[faction_id].ai_tech;
            wealth = PlayersData[faction_id].ai_wealth;
            power = PlayersData[faction_id].ai_power;
            fight = PlayersData[faction_id].ai_fight;
        } else {
            growth = tech = wealth = power = fight = 0;
        }
        int nutrient_req = Rules->nutrient_req_citizen;
        do {
            base = *BaseCurrent;
            BOOL full = *BaseCurrentWorkersLeft <= base->specialist_unk_1 + base->specialist_total;
            int best_id = 0;
            int best_value = 0;
            int nutrient_best = 0;
            uint32_t skip = full ? 0 : worked;
            int worst_value = 9999;
            int worst_id = -1;
            for (int i = 1; i < BaseRadiusTiles; i++) {
                uint32_t bit = 1 << i;
                if (bit & (skip | *BaseCurrentUsedTiles)) {
                    continue;
                }
                base = *BaseCurrent;
                int x = base->x + RadiusOffsetX[i];
                if (!*MapIsFlat) {
                    if (x < 0) {
                        x += *MapLongitudeBounds;
                    } else if (x >= (int)*MapLongitudeBounds) {
                        x -= *MapLongitudeBounds;
                    }
                }
                int y = base->y + RadiusOffsetY[i];
                if (y < 0 || y >= (int)*MapLatitudeBounds || x < 0 || x >= (int)*MapLongitudeBounds) {
                    continue;
                }
                int nutrient = crop_yield(base->faction_id_current, *BaseIDCurrentSelected, x, y, false);
                int mineral = mine_yield((*BaseCurrent)->faction_id_current, *BaseIDCurrentSelected,
                    x, y, false);
                int energy = energy_yield((*BaseCurrent)->faction_id_current, *BaseIDCurrentSelected,
                    x, y, false);
                if (nutrient_best < nutrient) {
                    nutrient_best = nutrient;
                }
                base = *BaseCurrent;
                int workers = *BaseCurrentWorkersLeft;
                int pop = base->population_size;
                int pop_req = pop * nutrient_req;
                int bonus = 0;
                if (workers / nutrient_req + base->nutrient_intake_2 < pop_req) {
                    // short of food: favour nutrients, the more so when the base can't make up
                    // the shortfall even with every worker
                    bonus = (base->nutrient_intake_2 + workers >= pop_req) ? 2 : 8;
                    if (nutrient < 0) {
                        // unchanged
                    } else if (nutrient <= bonus) {
                        nutrient *= 2;
                    } else {
                        nutrient += bonus;
                    }
                } else {
                    *BaseYieldNutrientMet = 1;
                }
                nutrient *= 2;
                mineral *= 2;
                energy *= 2;
                if (base->nutrient_intake_2 <= pop + pop_req && !nutrient) {
                    mineral /= 2;
                    energy /= 2;
                }
                if (!mineral && !bonus) {
                    nutrient /= 2;
                }
                int nutrient_div = ((base->nutrient_intake_2 - pop / 3 - pop_req + workers) * 2 - 2);
                nutrient_div = nutrient_div < 1 ? 1 : (nutrient_div > 99 ? 99 : nutrient_div);
                int mineral_mod = base->queue_production_id[0] < -70 ? 2 : -tech;
                int mineral_div = (base->mineral_intake_2 - (int)*BaseCurrentForcesMaintCost + 1) / 2;
                mineral_div = mineral_div < 1 ? 1 : (mineral_div > 99 ? 99 : mineral_div);
                int energy_mult = pop - power;
                energy_mult = energy_mult < 1 ? 1 : (energy_mult > 99 ? 99 : energy_mult);
                int energy_div = base->energy_intake_2;
                energy_div = energy_div < 1 ? 1 : (energy_div > 99 ? 99 : energy_div);
                int value = ((growth + 4) * nutrient * 256) / nutrient_div
                    + ((mineral_mod + fight + power + 6 + wealth) * pop * mineral * 64) / mineral_div
                    + ((wealth + 1 + tech * 2) * energy_mult * energy * 64) / energy_div;
                if (!(worked & bit)) {
                    if (best_value < value) {
                        best_id = i;
                        best_value = value;
                    }
                } else if (value < worst_value) {
                    worst_id = i;
                    worst_value = value;
                }
            }
            base = *BaseCurrent;
            if (governed && base->population_size > 4
                && (base->population_size * nutrient_req < base->nutrient_intake_2 || !nutrient_best)
                && *TurnCurrentNum > 100) {
                for (int citizen_id = 0; citizen_id < MaxSpecialistNum; citizen_id++) {
                    if (!has_tech(Citizen[citizen_id].preq_tech, faction_id)
                        || has_tech(Citizen[citizen_id].obsol_tech, faction_id)) {
                        continue;
                    }
                    base = *BaseCurrent;
                    int div = base->specialist_unk_1 + base->energy_intake_2;
                    div = div < 1 ? 1 : (div > 99 ? 99 : div);
                    int pop = base->population_size;
                    int value = Citizen[citizen_id].psych_bonus * 64
                        + ((wealth + 2) * pop * Citizen[citizen_id].ops_bonus * 64) / div
                        + ((tech + 2) * pop * Citizen[citizen_id].research_bonus * 64) / div;
                    if (best_value < value) {
                        best_id = -1 - citizen_id;
                        best_value = value;
                    }
                }
            }
            if (full) {
                if (best_id > 0 && worst_value < best_value && worst_id > 0) {
                    worked = (worked & ~(1 << worst_id)) | (1 << best_id);
                    base_yield_tile(worst_id, -1);
                    base_yield_tile(best_id, 1);
                    base = *BaseCurrent;
                    if (base->nutrient_intake_2 < (base->population_size + 1) * nutrient_req) {
                        worked = (worked & ~(1 << best_id)) | (1 << worst_id);
                        base_yield_tile(best_id, -1);
                        base_yield_tile(worst_id, 1);
                    }
                }
                break;
            }
            if (best_id < 0) {
                base = *BaseCurrent;
                base_spec_set(base->specialist_unk_1, -1 - best_id);
                (*BaseCurrent)->specialist_unk_1++;
            } else {
                if (best_id < 1) {
                    break;
                }
                worked |= 1 << best_id;
                base_yield_tile(best_id, 1);
                (*BaseCurrentWorkersLeft)--;
            }
            base = *BaseCurrent;
        } while (base->specialist_unk_1 + base->specialist_total < *BaseCurrentWorkersLeft
            || base->nutrient_intake_2 < base->population_size * nutrient_req);
    }
    for (int i = (*BaseCurrent)->specialist_total; i < *BaseCurrentWorkersLeft; i++) {
        base_spec_set(i, best_specialist());
    }
    base = *BaseCurrent;
    base->resource_sqr_radius = worked;
    base->specialist_total = *BaseCurrentWorkersLeft;
    base->state &= ~BSTATE_UNK_100;
    base->nutrient_intake_1 = base->nutrient_intake_2;
    base->mineral_intake_1 = base->mineral_intake_2;
    base->energy_intake_1 = base->energy_intake_2;
    base->unused_intake_1 = base->unused_intake_2;
    base->eco_damage = 0;
    for (int i = 0; i < BaseRadiusTiles; i++) {
        base = *BaseCurrent;
        int x = base->x + RadiusOffsetX[i];
        if (!*MapIsFlat) {
            if (x < 0) {
                x += *MapLongitudeBounds;
            } else if (x >= (int)*MapLongitudeBounds) {
                x -= *MapLongitudeBounds;
            }
        }
        int y = base->y + RadiusOffsetY[i];
        if (y < 0 || y >= (int)*MapLatitudeBounds || x < 0 || x >= (int)*MapLongitudeBounds) {
            continue;
        }
        uint32_t bits = map_loc(x, y)->bit;
        int damage = bit_count(bits & (BIT_ROAD | BIT_MAGTUBE | BIT_MINE | BIT_SOLAR_TIDAL | BIT_FARM
            | BIT_SOIL_ENRICHER | BIT_CONDENSER | BIT_ECH_MIRROR | BIT_THERMAL_BORE));
        if (base->resource_sqr_radius & (1 << i)) {
            damage *= 2;
        }
        if (bits & BIT_THERMAL_BORE) {
            damage += 8;
        }
        if (bits & BIT_ECH_MIRROR) {
            damage += 6;
        }
        if (bits & BIT_CONDENSER) {
            damage += 4;
        }
        if (bits & BIT_FOREST) {
            damage--;
        }
        BOOL tree_farm = has_fac_built(FAC_TREE_FARM, *BaseIDCurrentSelected);
        BOOL hybrid_forest = has_fac_built(FAC_HYBRID_FOREST, *BaseIDCurrentSelected);
        if (tree_farm && hybrid_forest) {
            damage = 0;
        }
        if (tree_farm || hybrid_forest) {
            damage /= 2;
        }
        (*BaseCurrent)->eco_damage += damage;
    }
}

/*
Purpose: Tally what the current base's units cost and bring in. Supply convoys out in the field
         add their tile's yield to the base (BaseCurrentConvoyTo), those parked in a base count
         one unit per resource (BaseCurrentConvoyFrom, or ConvoyTo for another base's convoy in
         this one). Each home unit up to the supply convoy plan is counted for support, beyond
         the faction's free allowance costing 1 mineral (2 at SUPPORT -4); native units on fungus
         are free and clean reactors exempt. Armed units in foreign territory, or aircraft other
         than interceptors, become pacifism drones under police -3 or lower (the first is free
         at -3). Armed units also feed the faction's military weight, and during the AI upkeep
         pass the support cost at every SUPPORT level is recorded for social engineering.
Original Offset: 004E9550
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch13.log)
*/
void __cdecl base_support() {
    uint32_t faction_id = (*BaseCurrent)->faction_id_current;
    for (int i = 0; i < 4; i++) {
        BaseCurrentConvoyTo[i] = 0;
        BaseCurrentConvoyFrom[i] = 0;
    }
    *BaseCurrentVehPacifismCount = 0;
    *BaseCurrentForcesMaintCount = 0;
    *BaseCurrentForcesSupported = 0;
    *BaseCurrentForcesMaintCost = 0;
    int support_free[8]; // units supported for free at SUPPORT -4 to +3
    for (int level = -4; level < 4; level++) {
        int free;
        if (level >= 3) {
            free = (*BaseCurrent)->population_size < 5 ? 4 : (*BaseCurrent)->population_size;
        } else if (level < -2) {
            free = 0;
        } else if (level == -2) {
            free = 1;
        } else {
            free = level + 2;
        }
        support_free[level + 4] = free;
    }
    for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
        Veh *veh = &Vehs[veh_id];
        if (veh->faction_id != faction_id) {
            continue;
        }
        int base_id = *BaseIDCurrentSelected;
        if (veh->home_base_id != base_id) {
            if (VehPrototypes[veh->proto_id].plan == PLAN_SUPPLY_CONVOY
                && veh->x == (*BaseCurrent)->x && veh->y == (*BaseCurrent)->y
                && veh->order == ORDER_CONVOY && veh->home_base_id >= 0) {
                BaseCurrentConvoyTo[veh->order_auto_type]++;
            }
            continue;
        }
        if (VehPrototypes[veh->proto_id].plan == PLAN_SUPPLY_CONVOY && base_who(veh->x, veh->y) < 0
            && veh->order == ORDER_CONVOY) {
            BaseCurrentConvoyTo[veh->order_auto_type] += resource_yield(veh->order_auto_type,
                faction_id, base_id, veh->x, veh->y);
        }
        int offense = Weapon[VehPrototypes[veh->proto_id].weapon_id].offense_rating;
        uint8_t plan = VehPrototypes[veh->proto_id].plan;
        if (offense && (offense > 0 || veh->proto_id >= MaxVehProtoFactionNum)
            && plan != PLAN_RECONNAISANCE && plan != PLAN_PLANET_BUSTER) {
            PlayersData[faction_id].armed_unit_weight += (plan == PLAN_OFFENSIVE || plan == PLAN_COMBAT) ? 2 : 1;
        }
        veh->state &= ~(VSTATE_REQUIRES_SUPPORT | VSTATE_PACIFISM_DRONE | VSTATE_PACIFISM_FREE_SKIP);
        if (plan == PLAN_SUPPLY_CONVOY && veh->order == ORDER_CONVOY && base_who(veh->x, veh->y) >= 0) {
            BaseCurrentConvoyFrom[veh->order_auto_type]++;
        }
        if (plan >= PLAN_SUPPLY_CONVOY) {
            continue;
        }
        if (!has_abil(veh->proto_id, ABL_CLEAN_REACTOR)) {
            Map *tile = map_loc(veh->x, veh->y);
            BOOL native = veh->proto_id < MaxVehProtoFactionNum
                && (offense < 0 || veh->proto_id == BSC_SPORE_LAUNCHER);
            if (!native || !(tile->bit & BIT_FUNGUS) || (tile->climate & 0xE0) < ALT_BIT_OCEAN_SHELF) {
                (*BaseCurrentForcesSupported)++;
                int support = PlayersData[faction_id].soc_effect_pending.support;
                int level = support + 4;
                level = level < 0 ? 0 : (level > 7 ? 7 : level);
                if (support_free[level] < (int)*BaseCurrentForcesSupported) {
                    (*BaseCurrentForcesMaintCount)++;
                    *BaseCurrentForcesMaintCost += (support < -3) + 1;
                    veh->state |= VSTATE_REQUIRES_SUPPORT;
                }
            }
            if (*BaseUpkeepStage == 1) {
                for (int level = -4; level < 4; level++) {
                    if (support_free[level + 4] < (int)*BaseCurrentForcesSupported) {
                        int cost = (level < -3) + 1;
                        int intake = (*BaseCurrent)->mineral_intake_2;
                        if (cost < intake) {
                            if (intake / 2 < cost) {
                                cost = cost * 2 - intake / 2;
                            }
                        } else {
                            cost = cost * 3 - (intake / 2) * 2;
                        }
                        PlayersData[faction_id].support_cost_by_level[level + 4] += cost;
                    }
                }
            }
        }
        if (Weapon[VehPrototypes[veh->proto_id].weapon_id].offense_rating) {
            if ((base_who(veh->x, veh->y) < 0
                && whose_territory(faction_id, veh->x, veh->y, NULL, false) != (int)faction_id)
                || (Chassis[VehPrototypes[veh->proto_id].chassis_id].triad == TRIAD_AIR
                    && VehPrototypes[veh->proto_id].plan != PLAN_AIR_SUPERIORITY)) {
                (*BaseCurrentVehPacifismCount)++;
                int police = PlayersData[faction_id].soc_effect_pending.police
                    + (has_fac_built(FAC_BROOD_PIT, *BaseIDCurrentSelected) ? 2 : 0);
                if (police == -3) {
                    veh->state |= (*BaseCurrentVehPacifismCount == 1) ? VSTATE_PACIFISM_FREE_SKIP
                        : VSTATE_PACIFISM_DRONE;
                } else if (police < -3) {
                    veh->state |= VSTATE_PACIFISM_DRONE;
                }
            }
        }
    }
}

/*
Purpose: Calculate nutrients and growth for the current base.
Original Offset: 004E9B70
Return Value: n/a
Status: Complete
*/
void __cdecl base_nutrient() {
    uint32_t faction_id = (*BaseCurrent)->faction_id_current;
    *BaseCurrentGrowthRate = PlayersData[faction_id].soc_effect_pending.growth;
    if (has_fac_built(FAC_CHILDREN_CRECHE, *BaseIDCurrentSelected)) {
        *BaseCurrentGrowthRate += 2; // +2 on growth scale
    }
    if ((*BaseCurrent)->state & BSTATE_GOLDEN_AGE_ACTIVE) {
        *BaseCurrentGrowthRate += 2;
    }
    (*BaseCurrent)->nutrient_intake_2 += BaseCurrentConvoyTo[RSC_NUTRIENTS];
    (*BaseCurrent)->nutrient_consumption = BaseCurrentConvoyFrom[RSC_NUTRIENTS]
        + (*BaseCurrent)->population_size * Rules->nutrient_req_citizen;
    (*BaseCurrent)->nutrient_surplus = (*BaseCurrent)->nutrient_intake_2
        - (*BaseCurrent)->nutrient_consumption;
    if ((*BaseCurrent)->nutrient_surplus >= 0) {
        if ((*BaseCurrent)->nutrients_accumulated < 0) {
            (*BaseCurrent)->nutrients_accumulated = 0;
        }
    } else if (!((*BaseCurrent)->nutrients_accumulated)) {
        (*BaseCurrent)->nutrients_accumulated = -1;
    }
    if (*BaseUpkeepStage == 1) {
        PlayersData[faction_id].nutrient_surplus_total
            += range((*BaseCurrent)->nutrient_surplus, 0, 99);
    }
}

/*
Purpose: Calculate minerals and ecological damage for the current base.
Original Offset: 004E9CB0
Return Value: n/a
Status: Complete
*/
void __cdecl base_minerals() {
    uint32_t faction_id = (*BaseCurrent)->faction_id_current;
    (*BaseCurrent)->mineral_intake_2 += BaseCurrentConvoyTo[RSC_MINERALS];
    uint32_t mineral_bonus = (has_fac_built(FAC_QUANTUM_CONVERTER, *BaseIDCurrentSelected)
        || has_project(SP_SINGULARITY_INDUCTOR, faction_id)) ? 1 : 0;
    if (has_fac_built(FAC_ROBOTIC_ASSEMBLY_PLANT, *BaseIDCurrentSelected)) {
        mineral_bonus++;
    }
    if (has_fac_built(FAC_GENEJACK_FACTORY, *BaseIDCurrentSelected)) {
        mineral_bonus++;
    }
    if (has_fac_built(FAC_NANOREPLICATOR, *BaseIDCurrentSelected)) {
        mineral_bonus++;
    }
    if (has_project(SP_BULK_MATTER_TRANSMITTER, faction_id)) {
        mineral_bonus++;
    }
    (*BaseCurrent)->mineral_intake_2 = ((*BaseCurrent)->mineral_intake_2 * (mineral_bonus + 2)) / 2;
    (*BaseCurrent)->mineral_consumption = *BaseCurrentForcesMaintCost
        + BaseCurrentConvoyFrom[RSC_MINERALS];
    (*BaseCurrent)->mineral_surplus = (*BaseCurrent)->mineral_intake_2
        - (*BaseCurrent)->mineral_consumption;
    (*BaseCurrent)->mineral_inefficiency = 0; // ?
    (*BaseCurrent)->mineral_surplus -= (*BaseCurrent)->mineral_inefficiency; // ?
    (*BaseCurrent)->mineral_surplus_final = (*BaseCurrent)->mineral_surplus;
    (*BaseCurrent)->eco_damage /= 8;
    int planet_eco_factor = PlayersData[faction_id].planet_ecology + 16;
    if ((*BaseCurrent)->eco_damage > 0) {
        int planet_modifier = (*BaseCurrent)->eco_damage;
        if ((*BaseCurrent)->eco_damage >= planet_eco_factor) {
            planet_modifier = planet_eco_factor;
        }
        planet_eco_factor -= planet_modifier;
        (*BaseCurrent)->eco_damage -= planet_modifier;
    }
    int eco_dmg_reduction = (has_fac_built(FAC_NANOREPLICATOR, *BaseIDCurrentSelected)
        || has_project(SP_SINGULARITY_INDUCTOR, faction_id)) ? 2 : 1;
    if (has_fac_built(FAC_CENTAURI_PRESERVE, *BaseIDCurrentSelected)) {
        eco_dmg_reduction++;
    }
    if (has_fac_built(FAC_TEMPLE_OF_PLANET, *BaseIDCurrentSelected)) {
        eco_dmg_reduction++;
    }
    if (has_project(SP_PHOLUS_MUTAGEN, faction_id)) {
        eco_dmg_reduction++;
    }
    (*BaseCurrent)->eco_damage += ((*BaseCurrent)->mineral_intake_2 - planet_eco_factor
        - range(PlayersData[faction_id].satellites_mineral, 0, (*BaseCurrent)->population_size))
        / eco_dmg_reduction;
    if (is_human(faction_id)) {
        (*BaseCurrent)->eco_damage += ((PlayersData[faction_id].major_atrocities
            + TectonicDetonationCount[faction_id]) * 5) / (range(*MapSeaLevel, 0, 100)
                / range(WorldBuilder->sea_level_rises, 1, 100) + eco_dmg_reduction);
    }
    if ((*BaseCurrent)->eco_damage < 0) {
        (*BaseCurrent)->eco_damage = 0;
    }
    if (ascending(faction_id) && *GameRules & RULES_VICTORY_TRANSCENDENCE) {
        (*BaseCurrent)->eco_damage *= 2;
    }
    if (*GameState & STATE_PERIHELION_ACTIVE) {
        (*BaseCurrent)->eco_damage *= 2;
    }
    uint32_t diff_factor;
    if (is_human(faction_id)) {
        int diff_lvl = PlayersData[faction_id].diff_level;
        diff_factor = !diff_lvl ? DLVL_TALENT
            : ((diff_lvl <= DLVL_LIBRARIAN) ? DLVL_LIBRARIAN : DLVL_TRANSCEND);
    } else {
        diff_factor = range(6 - *DiffLevelCurrent, DLVL_SPECIALIST, DLVL_LIBRARIAN);
    }
    (*BaseCurrent)->eco_damage = ((PlayersData[faction_id].tech_ranking
        - PlayersData[faction_id].theory_of_everything)
        * (3 - range(PlayersData[faction_id].soc_effect_pending.planet, -3, 2))
        * (*MapNativeLifeForms + 1) * (*BaseCurrent)->eco_damage * diff_factor) / 6;
    (*BaseCurrent)->eco_damage = ((*BaseCurrent)->eco_damage + 50) / 100;
    int queue_id;
    if (has_project(SP_SPACE_ELEVATOR, faction_id) // orbital facilities double mineral prod rate
        && (queue_id = (*BaseCurrent)->queue_production_id[0], queue_id == -FAC_SKY_HYDRO_LAB
            || queue_id == -FAC_NESSUS_MINING_STATION || queue_id == -FAC_ORBITAL_POWER_TRANS
            || queue_id == -FAC_ORBITAL_DEFENSE_POD)) {
        (*BaseCurrent)->mineral_intake_2 *= 2;
        (*BaseCurrent)->mineral_surplus = // doesn't update mineral_surplus_final?
            (*BaseCurrent)->mineral_intake_2 - (*BaseCurrent)->mineral_consumption;
    }
}

/*
Purpose: Calculate the current base's energy loss/inefficiency for an amount of energy.
Original Offset: 004EA1F0
Return Value: Amount of energy inefficiency
Status: Complete
*/
uint32_t __cdecl black_market(int energy) {
    if (energy <= 0) {
        return 0;
    }
    uint32_t faction_id = (*BaseCurrent)->faction_id_current;
    int x = (*BaseCurrent)->x;
    int y = (*BaseCurrent)->y;
    int dist_hq = 999;
    for (int i = 0; i < *BaseCurrentCount; i++) { // modified version of vulnerable()
        if (Bases[i].faction_id_current == faction_id && has_fac_built(FAC_HEADQUARTERS, i)) {
            int dist = vector_dist(Bases[i].x, Bases[i].y, x, y);
            if (dist < dist_hq) {
                dist_hq = dist;
            }
        }
    }
    if (dist_hq == 999) {
        dist_hq = 16;
    } else if (dist_hq == 0) {
        return 0;
    }
    BOOL has_creche = has_fac_built(FAC_CHILDREN_CRECHE, *BaseIDCurrentSelected);
    if (*BaseUpkeepStage == 1) {
        for (int i = 0, j = 0; i >= -64; i -= 8, j++) {
            int ineff;
            int factor;
            if (has_creche) {
                ineff = j - 2; // +2 on efficiency scale
                factor = i + 16; // ?
            } else {
                ineff = j;
                factor = i;
            }
            if (ineff >= 8) {
                PlayersData[faction_id].unk_46[j] += energy;
            } else {
                PlayersData[faction_id].unk_46[j] += energy * dist_hq / (factor + 64);
            }
        }
    }
    int ineffciency = 4 - PlayersData[faction_id].soc_effect_pending.efficiency;
    if (has_creche) {
        ineffciency -= 2; // +2 on efficiency scale
    }
    if (ineffciency >= 8) {
        return energy;
    }
    return range(energy * dist_hq / ((8 - ineffciency) * 8), 0, energy);
}

/*
Purpose: Reduce the current base's talents/drones/super drones until they fit the citizens that are
         not specialists. Talents cancel drones (super drones first); with no talents left, drones
         are shed until they fit and super drones are capped at the drone count.
Original Offset: n/a (inlined five times in base_psych)
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
static void base_psych_fit() {
    Base *base = *BaseCurrent;
    while (base->population_size - base->specialist_total
        < base->super_drone_total + base->talent_total + base->drone_total) {
        if (!base->talent_total) {
            if (base->drone_total <= base->population_size - base->specialist_total) {
                base->super_drone_total = range(base->super_drone_total, 0, base->drone_total);
                break;
            }
            base->drone_total--;
        } else {
            base->talent_total--;
            if (base->super_drone_total) {
                base->super_drone_total--;
            } else if (base->drone_total) {
                base->drone_total--;
            }
        }
    }
}

/*
Purpose: Clamp the current base's talents and drones to its population (stages 1-4), fit them and
         record the running totals for the given psych stage (0-4; shown in the psych screen).
Original Offset: n/a (inlined in base_psych)
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
static void base_psych_stage(uint32_t stage) {
    Base *base = *BaseCurrent;
    if (stage) { // stage 0 clamps the drones itself and leaves the talents unclamped
        base->drone_total = range(base->drone_total, 0, base->population_size);
        base->talent_total = range(base->talent_total, 0, base->population_size);
    }
    base_psych_fit();
    BaseCurrentTalentsStage[stage] = base->talent_total;
    BaseCurrentDronesStage[stage] = base->drone_total;
    BaseCurrentSuperDronesStage[stage] = base->super_drone_total;
}

/*
Purpose: Calculate the current base's psych values. With BaseUpkeepStage set to 1 the full
         talent/police grid is also evaluated and the talent-drone balance of every combination
         is accumulated into PlayersData.unk_39 for social_ai().
Original Offset: 004EA540
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl base_psych() {
    Base *base = *BaseCurrent;
    uint32_t faction_id = base->faction_id_current;
    int pop_size = base->population_size;
    int diff_lvl = is_human(faction_id) ? PlayersData[faction_id].diff_level : DLVL_LIBRARIAN;
    int drones_base;
    int drones_num_bases; // bureaucracy drones from the number of bases (psych_check talents)
    psych_check(faction_id, &drones_base, &drones_num_bases);
    if (drones_num_bases) {
        drones_num_bases = range(((int)PlayersData[faction_id].current_num_bases // signed: can go negative
            + *BaseIDCurrentSelected % drones_num_bases - drones_num_bases) / drones_num_bases,
            0, pop_size);
    }
    int drones = range(pop_size - drones_base, 0, pop_size);
    drones += range((pop_size + diff_lvl - 2) / 4, 0, (base->assimilation_turns_left + 9) / 10);
    if (Players[faction_id].rule_drone) {
        drones += pop_size / Players[faction_id].rule_drone;
    }
    int talents = 0;
    if (Players[faction_id].rule_talent) {
        talents = (Players[faction_id].rule_talent + pop_size - 1) / Players[faction_id].rule_talent;
    }
    for (int i = 0; i < Players[faction_id].faction_bonus_count; i++) {
        if (Players[faction_id].faction_bonus_id[i] == RULE_NODRONE) {
            drones -= Players[faction_id].faction_bonus_val1[i];
            if (drones < 0) {
                drones = 0;
            }
            break;
        }
    }
    int drones_fac = has_fac_built(FAC_GENEJACK_FACTORY) ? Rules->drones_genejack_factory : 0;
    if (has_fac_built(FAC_RECREATION_COMMONS)) {
        drones_fac -= 2;
    }
    if ((has_fac_built(FAC_NETWORK_NODE) && has_project(SP_VIRTUAL_WORLD, faction_id))
        || has_fac_built(FAC_HOLOGRAM_THEATRE)) {
        drones_fac -= 2;
    }
    if (has_project(SP_PLANETARY_TRANS_SYS, faction_id) && pop_size <= 3) {
        drones_fac--;
    }
    int talents_fac = has_fac_built(FAC_PARADISE_GARDEN) ? 2 : 0;
    if (has_fac_built(FAC_RESEARCH_HOSPITAL)) {
        drones_fac--;
    }
    if (has_fac_built(FAC_NANOHOSPITAL)) {
        drones_fac--;
    }
    int talents_sp = has_project(SP_HUMAN_GENOME_PROJ, faction_id) ? 1 : 0;
    if (has_project(SP_CLINICAL_IMMORTALITY, faction_id)) {
        talents_sp += 2; // original gives two talents; the manual says one
    }
    BOOL has_punishment_sphere = has_fac_built(FAC_PUNISHMENT_SPHERE);
    for (int i = 8; i >= 0; i--) {
        int talent_val;
        if (!i) {
            talent_val = PlayersData[faction_id].soc_effect_pending.talent;
        } else if (*BaseUpkeepStage == 1) {
            talent_val = i - 4;
        } else {
            continue;
        }
        for (int j = 9; j >= 0; j--) {
            int police_val;
            if (!j) {
                if (i) {
                    continue; // the grid pass skips the current police value
                }
                police_val = PlayersData[faction_id].soc_effect_pending.police
                    + has_fac_built(FAC_BROOD_PIT) * 2;
            } else if (*BaseUpkeepStage == 1 && i) {
                police_val = j - 6;
            } else {
                continue; // the current-value pass only runs j == 0
            }
            int police_count = range(police_val + 2, 0, 4);
            if (police_count >= 2) {
                police_count--;
            }
            // stage 0: social engineering talent effect and bureaucracy
            base->drone_total = drones;
            base->talent_total = talents;
            if (talent_val < 0) {
                base->drone_total -= talent_val;
            } else {
                base->talent_total += talent_val;
            }
            base->drone_total = range(base->drone_total, 0, pop_size) + drones_num_bases;
            base->super_drone_total = range(base->drone_total - pop_size, 0, pop_size);
            base->drone_total = range(base->drone_total, 0, pop_size);
            base_psych_stage(0);
            // stage 1: psych energy allocation
            base->talent_total += base->psych_total / 2;
            base_psych_stage(1);
            // stage 2: facilities
            base->drone_total += drones_fac;
            base->talent_total += talents_fac;
            base_psych_stage(2);
            // stage 3: police units and pacifism
            if (police_count > 0) {
                int policed = 0;
                if (has_project(SP_SELF_AWARE_COLONY, faction_id)) {
                    policed = 1;
                    if (base->drone_total) {
                        base->drone_total--;
                    }
                    if (police_val > 2 && base->drone_total) {
                        base->drone_total--;
                    }
                }
                // pass 0 counts the police-2x and native police units first, pass 1 the rest
                for (int pass = 0; pass < 2 && policed < police_count; pass++) {
                    for (int veh_id = stack_fix(veh_at(base->x, base->y)); veh_id >= 0;
                        veh_id = Vehs[veh_id].next_veh_id_stack) {
                        int proto_id = Vehs[veh_id].proto_id;
                        if (VehPrototypes[proto_id].plan > PLAN_RECONNAISANCE) {
                            continue;
                        }
                        BOOL is_native_police = (Players[faction_id].rule_flags & RFLAG_WORMPOLICE)
                            && proto_id < MaxVehProtoFactionNum
                            && (Weapon[VehPrototypes[proto_id].weapon_id].offense_rating < 0
                                || proto_id == BSC_SPORE_LAUNCHER)
                            && Chassis[VehPrototypes[proto_id].chassis_id].triad == TRIAD_LAND;
                        BOOL is_police_2x = has_abil(proto_id, ABL_POLICE_2X);
                        if ((is_police_2x || is_native_police) != (pass == 0)) {
                            continue;
                        }
                        if (base->drone_total) {
                            base->drone_total--;
                        }
                        if (is_police_2x && base->drone_total) {
                            base->drone_total--;
                        }
                        if (police_val > 2 && base->drone_total) {
                            base->drone_total--;
                        }
                        if (is_native_police && base->drone_total) {
                            base->drone_total--;
                        }
                        if (++policed >= police_count) {
                            break;
                        }
                    }
                }
            }
            if (police_val < -2) {
                if (police_val == -4) {
                    base->drone_total += *BaseCurrentVehPacifismCount;
                } else if (police_val == -3) {
                    if (*BaseCurrentVehPacifismCount > 1) {
                        base->drone_total += *BaseCurrentVehPacifismCount - 1;
                    }
                } else {
                    base->drone_total += *BaseCurrentVehPacifismCount * 2;
                }
            }
            base_psych_stage(3);
            // stage 4: secret projects, punishment sphere and nerve stapling
            base->talent_total += talents_sp;
            if (has_project(SP_LONGEVITY_VACCINE, faction_id)) {
                if (PlayersData[faction_id].soc_category_pending.economics == SE_PLANNED) {
                    base->drone_total--;
                }
                if (PlayersData[faction_id].soc_category_pending.economics != SE_FREE_MARKET) {
                    base->drone_total--;
                }
            }
            if (has_punishment_sphere || base->nerve_staple_turns_left) {
                base->drone_total = 0;
                base->talent_total = 0;
            }
            base_psych_stage(4);
            if (i) {
                int balance = base->talent_total - base->drone_total;
                if (balance > 0 && (base->drone_total || base->talent_total * 2 < pop_size)) {
                    balance /= 2;
                }
                if (balance < 0) {
                    balance -= pop_size;
                    if (pop_size < 6) {
                        balance *= 2;
                    }
                }
                PlayersData[faction_id].unk_39[i - 1][j - 1] += balance;
            }
        }
    }
}

/*
Purpose: Determine the faction's base with the specified position sorted by the most energy output.
Original Offset: 004EB490
Return Value: Base id for the specified rank position or -1 for invalid requests
Status: Complete
*/
int __cdecl base_rank(uint32_t faction_id, int rank_position) {
    if (rank_position < 0) { // Added parameter bounds check
        return -1;
    }
    int base_id[MaxBaseNum];
    int base_energy[MaxBaseNum];
    int bases_found = 0;
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (Bases[i].faction_id_current == faction_id) {
            base_id[bases_found] = i;
            base_energy[bases_found++] = (Bases[i].energy_intake_1 * 512) + i;
        }
    }
    if (!bases_found || rank_position >= bases_found) { // Added check to skip if no bases are found
        return -1;
    }
    sort_descending(bases_found, base_id, base_energy);
    return base_id[rank_position];
}

/*
Purpose: Work out the current base's energy. Energy convoys and commerce with every treaty partner
         (scaled by the trading bases' energy, commerce techs and the global trade pact; halved
         without a pact, +1 for the Planetary Governor, none under sanctions or with aliens) are
         added, and an alien base taps the energy grid (half its facilities plus 2.5 per secret
         project). After inefficiency the surplus (0-9999) is split by the economy/psych/labs
         allocation, specialists add their bonuses (obsolete types upgraded in place), facilities
         and projects multiply the totals, and unbalanced allocations lose output unless the
         faction's efficiency is high. Stockpile energy turns surplus minerals into credits.
         Finishes with base_psych().
Original Offset: 004EB560
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch14.log).
        One deliberate change: the alien energy grid starts from zero, where the original
        starts from an uninitialized local unless some treaty partner passed the commerce checks.
*/
void __cdecl base_energy() {
    Base *base = *BaseCurrent;
    uint32_t faction_id = base->faction_id_current;
    base->energy_intake_2 += BaseCurrentConvoyTo[RSC_ENERGY];
    base->energy_consumption = BaseCurrentConvoyFrom[RSC_ENERGY];
    base->energy_inefficiency = black_market(base->energy_intake_2 - base->energy_consumption);
    base->energy_surplus = base->energy_intake_2 - base->energy_consumption - base->energy_inefficiency;
    int rank = 0;
    int base_id = 0;
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (Bases[i].faction_id_current != faction_id) {
            continue;
        }
        if (base->energy_intake_1 * 0x200 + *BaseIDCurrentSelected < Bases[i].energy_intake_1 * 0x200 + i) {
            rank++;
        }
        if (&Bases[i] == base) {
            base_id = i;
        }
    }
    int commerce = 0; // the last import computed; seeds the alien energy grid below
    for (uint32_t faction_id_2 = 1; faction_id_2 < MaxPlayerNum; faction_id_2++) {
        BaseCommerceImport[faction_id_2] = 0;
        BaseCommerceExport[faction_id_2] = 0;
        if (faction_id_2 == faction_id
            || (!(PlayersData[faction_id].diplo_treaties[faction_id_2] & DTREATY_TREATY)
                && !(PlayersData[faction_id_2].diplo_treaties[faction_id] & DTREATY_TREATY))
            || !PlayersData[faction_id_2].current_num_bases) {
            continue;
        }
        int base_id_2 = base_rank(faction_id_2, rank);
        if (base_id_2 < 0) {
            continue;
        }
        int value = (Bases[base_id_2].energy_intake_1 + 7 + base->energy_intake_1) / 8;
        if (ProposalPassCount[PROP_REPEAL_GLOBAL_TRADE_PACT] < ProposalPassCount[PROP_GLOBAL_TRADE_PACT]) {
            value *= 2;
        }
        int techs = *TechCommerceCount + 1;
        int value_export = ((PlayersData[faction_id_2].tech_commerce_bonus + 1) * value + techs / 2) / techs;
        commerce = ((PlayersData[faction_id].tech_commerce_bonus + 1) * value + techs / 2) / techs;
        if (!(PlayersData[faction_id].diplo_treaties[faction_id_2] & DTREATY_PACT)) {
            commerce /= 2;
            value_export /= 2;
        }
        if ((uint32_t)*GovernorFactionID == faction_id) {
            commerce++;
        }
        if ((uint32_t)*GovernorFactionID == faction_id_2) {
            value_export++;
        }
        if (PlayersData[faction_id].sanction_turns || PlayersData[faction_id_2].sanction_turns
            || (Players[faction_id].rule_flags & RFLAG_ALIEN)
            || (Players[faction_id_2].rule_flags & RFLAG_ALIEN)) {
            value_export = 0;
            commerce = 0;
        }
        BaseCommerceImport[faction_id_2] = commerce;
        BaseCommerceExport[faction_id_2] = value_export;
        (*BaseCurrent)->energy_intake_2 += commerce;
    }
    if (*ExpansionEnabled && (Players[faction_id].rule_flags & RFLAG_ALIEN)) {
        for (int facility_id = 0; facility_id <= FAC_STOCKPILE_ENERGY; facility_id++) {
            if ((*BaseCurrent)->facilities_built[facility_id / 8] & (1 << (facility_id % 8))) {
                commerce++;
            }
        }
        for (int sp = 0; sp < MaxSecretProjectNum; sp++) {
            if ((&SecretProject->human_genome_project)[sp] == base_id) {
                commerce += 5;
            }
        }
        commerce /= 2;
        if (PlayersData[faction_id].sanction_turns) {
            commerce = 0;
        }
        (*BaseCurrent)->energy_intake_2 += commerce;
    }
    base = *BaseCurrent;
    if (*BaseUpkeepStage == 1) {
        int surplus = base->energy_surplus;
        PlayersData[faction_id].energy_surplus_total += surplus < 0 ? 0 : (surplus > 99999 ? 99999 : surplus);
    }
    base->energy_inefficiency = black_market(base->energy_intake_2 - base->energy_consumption);
    base->energy_surplus = base->energy_intake_2 - base->energy_consumption - base->energy_inefficiency;
    int energy = base->energy_surplus;
    energy = energy < 0 ? 0 : (energy > 9999 ? 9999 : energy);
    PlayerData *player = &PlayersData[faction_id];
    int psych = (player->se_alloc_psych * energy + 4) / 10;
    base->psych_total = (psych < 0 || energy < 0) ? 0 : (energy < psych ? energy : psych);
    int left = energy - base->psych_total;
    int economy = ((10 - player->se_alloc_labs - player->se_alloc_psych) * energy + 4) / 10;
    base->economy_total = (economy < 0 || left < 0) ? 0 : (economy <= left ? economy : left);
    base->labs_total = energy - base->psych_total - base->economy_total;
    int unk = ((9 - player->se_alloc_labs - player->se_alloc_psych) * energy + 4) / 10;
    base->unk_10 = (unk < 0 || left < 0) ? 0 : (left < unk ? left : unk);
    for (int i = 0; i < (*BaseCurrent)->specialist_total; i++) {
        int citizen_id = base_spec_get(i);
        citizen_id = citizen_id < 0 ? 0 : (citizen_id > MaxSpecialistNum - 1 ? MaxSpecialistNum - 1 : citizen_id);
        if (has_tech(Citizen[citizen_id].obsol_tech, faction_id)) {
            for (int j = 0; j < MaxSpecialistNum; j++) {
                if (has_tech(Citizen[j].preq_tech, faction_id) && !has_tech(Citizen[j].obsol_tech, faction_id)
                    && Citizen[citizen_id].ops_bonus <= Citizen[j].ops_bonus
                    && Citizen[citizen_id].psych_bonus <= Citizen[j].psych_bonus
                    && Citizen[citizen_id].research_bonus <= Citizen[j].research_bonus) {
                    citizen_id = j;
                    base_spec_set(i, citizen_id);
                    break;
                }
            }
        }
        (*BaseCurrent)->economy_total += Citizen[citizen_id].ops_bonus;
        (*BaseCurrent)->psych_total += Citizen[citizen_id].psych_bonus;
        (*BaseCurrent)->labs_total += Citizen[citizen_id].research_bonus;
    }
    base = *BaseCurrent;
    base_id = *BaseIDCurrentSelected;
    base->labs_total += Players[faction_id].rule_research;
    if (has_fac_built(FAC_BIOLOGY_LAB, base_id)) {
        base->labs_total += 2;
    }
    int mult_economy = 4, mult_labs = 4, mult_psych = 4;
    if (has_fac_built(FAC_ENERGY_BANK, base_id) || (SecretProject->planetary_energy_grid >= 0
        && Bases[SecretProject->planetary_energy_grid].faction_id_current == faction_id)) {
        mult_economy = 6;
    }
    if (has_fac_built(FAC_NETWORK_NODE, base_id)) {
        mult_labs = 6;
    }
    if (has_fac_built(FAC_HOLOGRAM_THEATRE, base_id) || (has_fac_built(FAC_NETWORK_NODE, base_id)
        && SecretProject->virtual_world >= 0
        && Bases[SecretProject->virtual_world].faction_id_current == faction_id)) {
        mult_psych = 6;
    }
    if (has_fac_built(FAC_RESEARCH_HOSPITAL, base_id)) {
        mult_psych++;
        mult_labs += 2;
    }
    if (has_fac_built(FAC_NANOHOSPITAL, base_id)) {
        mult_psych++;
        mult_labs += 2;
    }
    if (has_fac_built(FAC_TREE_FARM, base_id)) {
        mult_psych += 2;
        mult_economy += 2;
    }
    if (has_fac_built(FAC_HYBRID_FOREST, base_id)) {
        mult_psych += 2;
        mult_economy += 2;
    }
    if (has_fac_built(FAC_FUSION_LAB, base_id)) {
        mult_economy += 2;
        mult_labs += 2;
    }
    if (has_fac_built(FAC_QUANTUM_LAB, base_id)) {
        mult_economy += 2;
        mult_labs += 2;
    }
    base->psych_total = (base->psych_total * mult_psych + 3) / 4;
    base->labs_total = (base->labs_total * mult_labs + 3) / 4;
    if (SecretProject->supercollider == base_id) {
        base->labs_total *= 2;
    }
    if (SecretProject->theory_of_everything == base_id) {
        base->labs_total *= 2;
    }
    base->economy_total = (base->economy_total * mult_economy + 3) / 4;
    if (SecretProject->space_elevator == base_id) {
        base->economy_total *= 2;
    }
    if (SecretProject->longevity_vaccine == base_id && player->soc_category_pending.economics == SE_FREE_MARKET) {
        base->economy_total += base->economy_total / 2;
    }
    base->unk_10 = (base->unk_10 * mult_economy + 3) / 4;
    if (SecretProject->network_backbone == base_id) {
        for (int i = 0; i < *BaseCurrentCount; i++) {
            if (has_fac_built(FAC_NETWORK_NODE, i)) {
                base->labs_total++;
            }
        }
        if (Players[faction_id].rule_flags & RFLAG_ALIEN) {
            base->labs_total += commerce;
        }
        for (uint32_t faction_id_2 = 1; faction_id_2 < MaxPlayerNum; faction_id_2++) {
            if (!player->sanction_turns && !PlayersData[faction_id_2].sanction_turns
                && !(Players[faction_id].rule_flags & RFLAG_ALIEN)
                && !(Players[faction_id_2].rule_flags & RFLAG_ALIEN)) {
                base->labs_total += BaseCommerceImport[faction_id_2];
            }
        }
    }
    int alloc_labs = player->se_alloc_labs;
    int alloc_psych = player->se_alloc_psych;
    int imbalance = alloc_psych - 10 + alloc_labs * 2;
    if (imbalance < 0) {
        imbalance = (5 - alloc_labs) * 2 - alloc_psych;
    }
    imbalance /= 2;
    int efficiency = player->soc_effect_pending.efficiency;
    efficiency = efficiency < -4 ? -4 : (efficiency > 4 ? 4 : efficiency);
    int inefficiency = (4 - efficiency) * 2;
    if (imbalance && inefficiency) {
        int penalty = inefficiency * imbalance;
        int penalty_labs, penalty_economy;
        if (10 - alloc_labs - alloc_psych < alloc_labs) {
            penalty_labs = penalty < 0 ? 0 : (penalty > 40 ? 40 : penalty);
            penalty_economy = penalty * 2 < 0 ? 0 : (penalty * 2 > 80 ? 80 : penalty * 2);
        } else {
            penalty_labs = penalty * 2 < 0 ? 0 : (penalty * 2 > 80 ? 80 : penalty * 2);
            penalty_economy = penalty < 0 ? 0 : (penalty > 40 ? 40 : penalty);
        }
        base->labs_total = ((100 - penalty_labs) * base->labs_total + 50) / 100;
        base->economy_total = ((100 - penalty_economy) * base->economy_total + 50) / 100;
    }
    if (base->queue_production_id[0] == -FAC_STOCKPILE_ENERGY && base->mineral_surplus > 0
        && base->production_id_last == -FAC_STOCKPILE_ENERGY) {
        int stockpile = (base->mineral_surplus + 1) / 2;
        if (SecretProject->planetary_energy_grid >= 0
            && Bases[SecretProject->planetary_energy_grid].faction_id_current == faction_id) {
            stockpile = (stockpile * 5 + 3) / 4;
        }
        base->economy_total += stockpile;
    }
    base_psych();
}

/*
Purpose: Recompute the current base's support, yields, nutrients, minerals and energy, unless they
         are already up to date for this base and no refresh is forced.
Original Offset: 004EC3B0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
void __cdecl base_compute(BOOL update_prev) {
    if (update_prev || *BaseComputeID != *BaseIDCurrentSelected) {
        *BaseComputeID = *BaseIDCurrentSelected;
        base_support();
        base_yield();
        base_nutrient();
        base_minerals();
        base_energy();
    }
}

/*
Purpose: Check what facility (if any) a base needs for additional population growth. Stand alone
         function unused in original game and likely optimized out.
Original Offset: 004EEF80
Return Value: Facility id needed for pop growth or zero if base already has Hab Complex and Dome.
Status: Complete
*/
uint32_t __cdecl pop_goal_fac(uint32_t base_id) {
    uint32_t faction_id = Bases[base_id].faction_id_current;
    uint32_t limit_mod = has_project(SP_ASCETIC_VIRTUES, faction_id) ? 2 : 0;
    int pop = Bases[base_id].population_size - limit_mod + Players[faction_id].rule_population;
    if (pop >= (int)Rules->pop_limit_sans_hab_complex && !has_fac_built(FAC_HAB_COMPLEX, base_id)) {
        return FAC_HAB_COMPLEX;
    }
    if (pop >= (int)Rules->pop_limit_sans_hab_dome 
        && !has_fac_built(FAC_HABITATION_DOME, base_id)) {
        return FAC_HABITATION_DOME;
    }
    return 0; // Pop hasn't reached capacity or Base already has Hab Complex and Dome
}

/*
Purpose: Calculate population goal for a base.
Original Offset: 004EF090
Return Value: Goal population
Status: Complete
*/
uint32_t __cdecl pop_goal(uint32_t base_id) {
    uint32_t faction_id = Bases[base_id].faction_id_current;
    uint32_t limit_mod = has_project(SP_ASCETIC_VIRTUES, faction_id) ? 2 : 0;
    int goal = (36 - Bases[base_id].population_size) / 6 + Bases[base_id].population_size;
    if (goal <= 6) {
        goal = 6;
    }
    if (!has_fac_built(FAC_HAB_COMPLEX, base_id)) {
        int compare = Rules->pop_limit_sans_hab_complex - Players[faction_id].rule_population 
            + limit_mod;
        if (goal >= compare) {
            goal = compare;
        }
    }
    if (!has_fac_built(FAC_HABITATION_DOME, base_id)) {
        int compare = Rules->pop_limit_sans_hab_dome - Players[faction_id].rule_population 
            + limit_mod;
        if (goal >= compare) {
            goal = compare;
        }
    }
    return goal;
}

/*
Purpose: Calculate the per-unit cost of upgrading one prototype to another: the new design's cost
         (squared and a half for a custom design whose prototype was never built), plus any
         gain in speed, weapon cost and armor cost; halved for the Nano Factory's owner.
Original Offset: 004EFD50
Return Value: Upgrade cost per unit
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch3.log)
*/
int __cdecl upgrade_cost(uint32_t faction_id, int proto_id_new, int proto_id_old) {
    int cost = VehPrototypes[proto_id_new].cost;
    if (proto_id_new >= MaxVehProtoFactionNum
        && !(VehPrototypes[proto_id_new].flags & PROTO_TYPED_COMPLETE)) {
        int cost_untyped = cost + (cost + 1) / 2;
        cost = (cost_untyped + 1) * cost_untyped;
    }
    int speed_gain = Chassis[VehPrototypes[proto_id_new].chassis_id].speed
        - Chassis[VehPrototypes[proto_id_old].chassis_id].speed;
    if (speed_gain < 0) {
        speed_gain = 0;
    }
    int weapon_gain = Weapon[VehPrototypes[proto_id_new].weapon_id].cost
        - Weapon[VehPrototypes[proto_id_old].weapon_id].cost;
    if (weapon_gain < 0) {
        weapon_gain = 0;
    }
    int armor_gain = Armor[VehPrototypes[proto_id_new].armor_id].cost
        - Armor[VehPrototypes[proto_id_old].armor_id].cost;
    if (armor_gain < 0) {
        armor_gain = 0;
    }
    int total = cost + speed_gain + weapon_gain + armor_gain;
    int base_id_nano = SecretProject->nano_factory;
    if (base_id_nano >= 0 && Bases[base_id_nano].faction_id_current == faction_id) {
        total /= 2;
    }
    return total;
}

/*
Purpose: Grow or starve the current base at the end of its turn. A base out of nutrients recalls
         its nutrient convoys parked in bases, warns, and loses a citizen (an objective base
         keeps its last one); losing the last kills the base. A pop boom (cloning vats or growth
         rating above 5 with a full citizen's worth of surplus) adds a citizen at once; otherwise
         the base grows when its store reaches the cost, unless growth is -3 or lower or a
         habitation limit is in the way (then it warns and keeps half the cost). The surplus is
         then banked, and a human base heading for starvation gets advice on why.
Original Offset: 004EF1C0
Return Value: 1 if the base starved to death, otherwise 0
Status: Complete (verified vs original for AI bases: docs/regression-runs/2026-10-16-council-batch15.log)
*/
int __cdecl base_growth() {
    Base *base = *BaseCurrent;
    uint32_t faction_id = base->faction_id_current;
    int base_id = *BaseIDCurrentSelected;
    int x = base->x;
    int y = base->y;
    BOOL alien = (Players[faction_id].rule_flags & RFLAG_ALIEN) != 0;
    if (base->nutrients_accumulated < 0) {
        if (!(*GameState & STATE_UNK_1) || (*GameState & STATE_UNK_8)) {
            if (BaseCurrentConvoyFrom[RSC_NUTRIENTS]) {
                for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
                    Veh *veh = &Vehs[veh_id];
                    if (veh->home_base_id == base_id
                        && VehPrototypes[veh->proto_id].plan == PLAN_SUPPLY_CONVOY
                        && veh->order == ORDER_CONVOY && veh->order_auto_type == RSC_NUTRIENTS
                        && base_who(veh->x, veh->y) >= 0) {
                        veh->order = ORDER_NONE;
                    }
                }
            }
            popb("STARVE", 0x4000, 14, alien ? "al_starv_sm.pcx" : "starv_sm.pcx", 0);
            base = *BaseCurrent;
            if (!is_objective(base_id) || base->population_size > 1) {
                base->population_size--;
                if (base->population_size < 1) {
                    base_kill(base_id);
                    draw_map(1);
                    eliminate_player(faction_id, 0);
                    return 1;
                }
            }
        }
        (*BaseCurrent)->nutrients_accumulated = (*BaseCurrent)->nutrient_surplus;
        draw_tile(x, y, 2);
        return 0;
    }
    int cost = cost_factor(faction_id, RSC_NUTRIENTS, base_id) * (base->population_size + 1);
    // the population allowed below each habitation facility, the faction's rule and Ascetic Virtues
    auto pop_level = [&]() {
        Base *b = &Bases[base_id];
        int ascetic = (SecretProject->ascetic_virtues >= 0
            && Bases[SecretProject->ascetic_virtues].faction_id_current == b->faction_id_current) ? 2 : 0;
        return b->population_size - ascetic + Players[b->faction_id_current].rule_population;
    };
    if (((SecretProject->cloning_vats >= 0
        && Bases[SecretProject->cloning_vats].faction_id_current == faction_id)
        || *BaseCurrentGrowthRate > 5)
        && Rules->nutrient_req_citizen && (int)Rules->nutrient_req_citizen <= base->nutrient_surplus) {
        if ((pop_level() < (int)Rules->pop_limit_sans_hab_complex || has_fac(FAC_HAB_COMPLEX, base_id, 0))
            && (pop_level() < (int)Rules->pop_limit_sans_hab_dome || has_fac(FAC_HABITATION_DOME, base_id, 0))) {
            if (base->population_size < 127) {
                base->population_size++;
            }
            *BaseComputeID = *BaseIDCurrentSelected;
            base_support();
            base_yield();
            base_nutrient();
            base_minerals();
            base_energy();
            draw_tile(x, y, 2);
            if ((*BaseCurrent)->nutrients_accumulated > cost) {
                (*BaseCurrent)->nutrients_accumulated = cost;
            }
            return 0;
        }
    }
    base = *BaseCurrent;
    if (cost <= base->nutrients_accumulated) {
        if (*BaseCurrentGrowthRate < -2) {
            base->nutrients_accumulated = cost;
            return 0;
        }
        int facility_id;
        if (pop_level() >= (int)Rules->pop_limit_sans_hab_complex && !has_fac(FAC_HAB_COMPLEX, base_id, 0)) {
            facility_id = FAC_HAB_COMPLEX;
        } else if (pop_level() >= (int)Rules->pop_limit_sans_hab_dome
            && !has_fac(FAC_HABITATION_DOME, *BaseIDCurrentSelected, 0)) {
            facility_id = FAC_HABITATION_DOME;
        } else {
            facility_id = 0;
            base->nutrients_accumulated = 0;
            base->population_size++;
            *BaseComputeID = *BaseIDCurrentSelected;
            base_support();
            base_yield();
            base_nutrient();
            base_minerals();
            base_energy();
            draw_tile(x, y, 2);
        }
        if (facility_id) {
            base = *BaseCurrent;
            base->nutrients_accumulated = (cost_factor(base->faction_id_current, RSC_NUTRIENTS,
                *BaseIDCurrentSelected) * (base->population_size + 1)) / 2;
            parse_say(1, (int)Facility[facility_id].name, -1, -1);
            popb("POPULATIONLIMIT", 0x800, 16, alien ? "al_pop_sm.pcx" : "pop_sm.pcx", 0);
        }
    }
    base = *BaseCurrent;
    base->nutrients_accumulated += base->nutrient_surplus;
    int surplus = base->nutrient_surplus;
    if (surplus >= 0 || base->nutrients_accumulated - surplus < 0
        || base->nutrients_accumulated + surplus * 5 >= 0
        || (base->governor & (GOV_ACTIVE | GOV_MANAGES_CITIZENS_SPECS))
            == (GOV_ACTIVE | GOV_MANAGES_CITIZENS_SPECS)) {
        return 0;
    }
    // starvation ahead: count the land the base could farm better to choose the advice
    int farmable = 0;
    for (int i = 1; i < BaseRadiusTiles; i++) {
        int x_radius = x + RadiusOffsetX[i];
        if (!*MapIsFlat) {
            if (x_radius < 0) {
                x_radius += *MapLongitudeBounds;
            } else if (x_radius >= (int)*MapLongitudeBounds) {
                x_radius -= *MapLongitudeBounds;
            }
        }
        int y_radius = y + RadiusOffsetY[i];
        if (y_radius < 0 || y_radius >= (int)*MapLatitudeBounds || x_radius < 0
            || x_radius >= (int)*MapLongitudeBounds) {
            continue;
        }
        Map *tile = map_loc(x_radius, y_radius);
        uint32_t alt = tile->climate & 0xE0;
        if (alt < ALT_BIT_SHORE_LINE || (tile->bit & BIT_FARM)) {
            continue;
        }
        if ((tile->bit & BIT_FOREST) && has_fac_built(FAC_TREE_FARM, *BaseIDCurrentSelected)) {
            continue;
        }
        if ((tile->bit & BIT_FUNGUS) && alt >= ALT_BIT_OCEAN_SHELF
            && PlayersData[faction_id].tech_fungus_nutrient >= 2) {
            continue;
        }
        uint32_t rainfall = (tile->climate >> 3) & 3;
        if (((*BaseCurrent)->resource_sqr_radius & (1 << i))
            && (((tile->bit & BIT_FUNGUS) && alt > 0x3F) || (tile->bit & BIT_FOREST)
                || (tile->val3 & 0xC0) == 0x80 || !rainfall)) {
            continue;
        }
        if (rainfall == 1 || has_tech(Rules->tech_three_nutrients_sqr, faction_id)) {
            farmable++;
        }
    }
    LPCSTR pcx = alien ? "al_starv_sm.pcx" : "starv_sm.pcx";
    base = *BaseCurrent;
    if (farmable && has_tech(VehPrototypes[BSC_FORMERS].preq_tech, faction_id)) {
        if (PlayersData[faction_id].proto_id_active[BSC_FORMERS]) {
            popb("LOWNUTRIENTFARMS", 0, -1, pcx, 0);
        } else {
            popb("LOWNUTRIENTFORMERS", 0, -1, pcx, 0);
        }
    } else if (base->specialist_total < 2 && (!base->specialist_total || base->population_size > 2)) {
        popb("LOWNUTRIENT", 0x80, 16, pcx, 0);
    } else if (BaseCurrentDronesStage[0]) {
        popb("LOWNUTRIENTSPEC1", 0, -1, pcx, 0);
    } else {
        popb("LOWNUTRIENTSPEC2", 0, -1, pcx, 0);
    }
    return 0;
}

/*
Purpose: Upgrade every unit of one prototype to another: pay the energy, re-type the units
         (adjusting morale when the Trained ability comes or goes), update the active prototype
         counts and base production queues, mark the new design as built and retire the old one.
Original Offset: 004EFB50
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch3.log)
*/
void __cdecl do_upgrade(uint32_t faction_id, int proto_id_new, int proto_id_old) {
    int cost = upgrade_cost(faction_id, proto_id_new, proto_id_old);
    uint32_t count_old = PlayersData[faction_id].proto_id_active[proto_id_old];
    PlayersData[faction_id].energy_reserves -= cost * count_old * 10;
    for (int i = 0; i < *VehCurrentCount; i++) {
        if (Vehs[i].faction_id == faction_id && Vehs[i].proto_id == proto_id_old) {
            PlayersData[faction_id].proto_id_active[proto_id_old]--;
            Vehs[i].proto_id = (int16_t)proto_id_new;
            PlayersData[faction_id].proto_id_active[proto_id_new]++;
            if (has_abil(proto_id_new, ABL_TRAINED) && !has_abil(proto_id_old, ABL_TRAINED)) {
                uint32_t morale = Vehs[i].morale + 1;
                if (morale > MORALE_ELITE) {
                    morale = MORALE_ELITE;
                }
                Vehs[i].morale = (uint8_t)morale;
            }
            if (!has_abil(proto_id_new, ABL_TRAINED) && has_abil(proto_id_old, ABL_TRAINED)) {
                int morale = Vehs[i].morale - 1;
                if (morale < 0) {
                    morale = 0;
                } else if (morale > MORALE_ELITE) {
                    morale = MORALE_ELITE;
                }
                Vehs[i].morale = (uint8_t)morale;
            }
        }
    }
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (Bases[i].faction_id_current == faction_id && (int)Bases[i].queue_size >= 0) {
            // the original walks one slot past the queue size
            for (int j = 0; j <= (int)Bases[i].queue_size; j++) {
                if (Bases[i].queue_production_id[j] == proto_id_old) {
                    Bases[i].queue_production_id[j] = proto_id_new;
                }
            }
        }
    }
    if (proto_id_new >= MaxVehProtoFactionNum && cost * count_old * 10 != 0) {
        VehPrototypes[proto_id_new].flags |= PROTO_TYPED_COMPLETE;
    }
    if (proto_id_old >= MaxVehProtoFactionNum) {
        VehPrototypes[proto_id_old].flags &= ~PROTO_ACTIVE;
    }
    if (is_human(faction_id)) {
        draw_map(1);
    }
    if (faction_id == (uint32_t)*FactionIDCurrent) {
        Console_update_data(ConsoleVar, 0, 0);
    }
}

/*
Purpose: Offer upgrades from the specified prototype: every active design the faction still has
         units of that is marked obsolete for it, has the same plan, class, triad and chassis and
         is at least as good in weapon, armor and speed goes through upgrade_prototype().
Original Offset: 004F0460
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch3.log)
*/
void __cdecl upgrade_prototypes(int faction_id, uint32_t proto_id) {
    VehPrototype *proto = &VehPrototypes[proto_id];
    proto->flags &= ~PROTO_UNK_100;
    const uint32_t ability_roles = ABL_AMPHIBIOUS | ABL_DROP_POD | ABL_ANTIGRAV_STRUTS
        | ABL_ARTILLERY | ABL_POLICE_2X;
    for (int i = 0; i < 2 * MaxVehProtoFactionNum; i++) {
        uint32_t id = i < MaxVehProtoFactionNum ? i
            : faction_id * MaxVehProtoFactionNum + (i % MaxVehProtoFactionNum);
        VehPrototype *other = &VehPrototypes[id];
        if (id == proto_id || !(other->flags & PROTO_ACTIVE)
            || !(other->obsolete_factions & (1 << faction_id)) || other->plan != proto->plan
            || other->unk_1 != proto->unk_1
            || Chassis[other->chassis_id].triad != Chassis[proto->chassis_id].triad
            || !PlayersData[faction_id].proto_id_active[id]) {
            continue;
        }
        if (Weapon[proto->weapon_id].offense_rating == 0) {
            if (proto->weapon_id != other->weapon_id) {
                continue;
            }
        } else if (Weapon[proto->weapon_id].offense_rating
            > Weapon[other->weapon_id].offense_rating) {
            continue;
        }
        BOOL roles_other = has_abil(id, ability_roles);
        BOOL roles_proto = has_abil(proto_id, ability_roles);
        if (roles_proto == roles_other
            && Armor[proto->armor_id].defense_rating <= Armor[other->armor_id].defense_rating
            && Chassis[proto->chassis_id].speed <= Chassis[other->chassis_id].speed
            && proto->chassis_id == other->chassis_id) {
            upgrade_prototype(faction_id, proto_id, id, 1);
        }
    }
}

/*
Purpose: Run upgrade_prototypes() for every active, built design of the faction that has an
         upgrade pending. Humans only get this with the "auto prune obsolete units" preference.
Original Offset: 004F0650
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch3.log)
*/
void __cdecl upgrade_any_prototypes(int faction_id) {
    *VehUpgradeUnk = 0;
    if (is_human(faction_id) && !(*GameMorePreferences & MPREF_BSC_AUTO_PRUNE_OBS_VEH)) {
        return;
    }
    for (int i = 0; i < 2 * MaxVehProtoFactionNum; i++) {
        uint32_t id = i < MaxVehProtoFactionNum ? i
            : faction_id * MaxVehProtoFactionNum + (i % MaxVehProtoFactionNum);
        uint32_t flags = VehPrototypes[id].flags;
        if ((flags & PROTO_ACTIVE) && (flags & PROTO_TYPED_COMPLETE) && (flags & PROTO_UNK_100)) {
            upgrade_prototypes(faction_id, id);
        }
    }
}

/*
Purpose: Move the specified base's production queue forward if applicable.
Original Offset: 004F06E0
Return Value: Is there a valid item in queue to be built? true/false
Status: Complete
*/
BOOL __cdecl base_queue(uint32_t base_id) {
    uint32_t faction_id = Bases[base_id].faction_id_current;
    while (Bases[base_id].queue_size) {
        int queue_prod_id = Bases[base_id].queue_production_id[0];
        if (queue_prod_id >= 0) {
            PlayersData[faction_id].proto_id_queue[queue_prod_id]--;
        }
        for (uint32_t i = 0; i < Bases[base_id].queue_size; i++) {
            Bases[base_id].queue_production_id[i] = Bases[base_id].queue_production_id[i + 1];
        }
        Bases[base_id].queue_size--;
        queue_prod_id = Bases[base_id].queue_production_id[0];
        if((queue_prod_id >= 0) ? veh_avail(queue_prod_id, faction_id, base_id) 
            : facility_avail(-queue_prod_id, faction_id, base_id, 0)) {
            if (queue_prod_id <= -FacilitySPStart) {
                wave_it(36); // CPU project initiated
            }
            draw_radius(Bases[base_id].x, Bases[base_id].y, 2, 2);
            return true;
        }
    }
    return false;
}

/*
Purpose: Make the current base pay for its units. While the mineral intake can't cover the forces
         maintenance, it first calls off mineral convoys parked in bases (each saving a mineral),
         then disbands the supported unit farthest away. A human player is warned instead; an AI
         colony pod is dropped from support and charged off twice before being disbanded. Units
         with a clean reactor stop the purge. A rioting base under weak police that the AI runs
         (or a human governor set to do so) also disbands its most distant pacifism drone unit,
         a roll weighted by the faction's appetite for war deciding which units are eligible.
Original Offset: 004F4830
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch12.log)
*/
void __cdecl base_check_support() {
    Base *base = *BaseCurrent;
    uint32_t faction_id = base->faction_id_current;
    int base_id = *BaseIDCurrentSelected;
    while ((*BaseCurrent)->mineral_intake_2 < (int)*BaseCurrentForcesMaintCost) {
        base = *BaseCurrent;
        int score_best = -1;
        int veh_id_best = -1;
        for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
            Veh *veh = &Vehs[veh_id];
            if (veh->faction_id != base->faction_id_current || veh->home_base_id != base_id) {
                continue;
            }
            int score;
            uint8_t plan = VehPrototypes[veh->proto_id].plan;
            if (plan == PLAN_SUPPLY_CONVOY) {
                if (veh->order != ORDER_CONVOY || veh->order_auto_type != RSC_MINERALS
                    || base_who(veh->x, veh->y) < 0) {
                    continue;
                }
                score = 99999;
            } else if ((veh->state & VSTATE_REQUIRES_SUPPORT) && plan < PLAN_SUPPLY_CONVOY) {
                score = vector_dist(base->x, base->y, veh->x, veh->y);
            } else {
                continue;
            }
            if (score_best < score) {
                veh_id_best = veh_id;
                score_best = score;
            }
        }
        if (score_best < 0) {
            break;
        }
        Veh *veh = &Vehs[veh_id_best];
        uint8_t plan = VehPrototypes[veh->proto_id].plan;
        if (plan == PLAN_SUPPLY_CONVOY) {
            veh->order = ORDER_NONE;
            (*BaseCurrentForcesMaintCost)--;
            continue;
        }
        if (is_human(faction_id)) {
            parse_says(1, VehPrototypes[veh->proto_id].veh_name, -1, -1);
            popb("NOSUPPORT", 0x8000, 13, "genwarning_sm.pcx", 0);
        } else if (plan == PLAN_COLONIZATION) {
            veh->state &= ~VSTATE_REQUIRES_SUPPORT;
            int support = PlayersData[faction_id].soc_effect_pending.support;
            if (!has_abil(veh->proto_id, ABL_CLEAN_REACTOR)) {
                *BaseCurrentForcesMaintCost -= (support < -3) + 1;
            }
        }
        int support = PlayersData[faction_id].soc_effect_pending.support;
        if (has_abil(veh->proto_id, ABL_CLEAN_REACTOR)) {
            break;
        }
        *BaseCurrentForcesMaintCost -= (support < -3) + 1;
        kill(veh_id_best);
    }
    base = *BaseCurrent;
    if ((!is_human(faction_id)
        || ((base->governor & GOV_MANAGES_CITIZENS_SPECS) && (base->governor & GOV_ACTIVE)))
        && (base->state & BSTATE_DRONE_RIOTS_ACTIVE)
        && PlayersData[faction_id].soc_effect_pending.police
            + (has_fac_built(FAC_BROOD_PIT, base_id) ? 2 : 0) < -2
        && (base->talent_total < base->drone_total || base->nutrient_surplus < 0)) {
        int score_best = -1;
        int veh_id_best = -1;
        for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
            Veh *veh = &Vehs[veh_id];
            uint32_t state = veh->state;
            if (veh->faction_id != faction_id || veh->home_base_id != base_id
                || !(state & VSTATE_PACIFISM_DRONE)) {
                continue;
            }
            if (is_human(faction_id)
                && (!(state & VSTATE_UNK_200) || (state & VSTATE_UNK_1000000)
                    || ((state & VSTATE_UNK_2000000)
                        && veh->patrol_current_point < veh->waypoint_count)
                    || veh->order_auto_type)) {
                continue;
            }
            int ai_fight = PlayersData[faction_id].ai_fight;
            if (ai_fight != -1 && ai_fight + 1 >= 0 && _rand() % (ai_fight + 2)) {
                continue;
            }
            int dist = x_dist(veh->x, (*BaseCurrent)->x) + abs(veh->y - (*BaseCurrent)->y);
            int cost = VehPrototypes[veh->proto_id].cost;
            cost = cost < 1 ? 1 : (cost > 99 ? 99 : cost);
            int score = ((dist >> 1) * 100) / cost;
            uint8_t plan = VehPrototypes[veh->proto_id].plan;
            if (plan == PLAN_DEFENSIVE) {
                score *= 2;
            }
            if (plan == PLAN_COMBAT) {
                score /= 2;
            }
            if (score_best < score) {
                veh_id_best = veh_id;
                score_best = score;
            }
        }
        if (veh_id_best >= 0) {
            kill(veh_id_best);
            (*BaseCurrent)->state &= ~BSTATE_DRONE_RIOTS_ACTIVE;
        }
    }
}

/*
Purpose: Check if current base has had an energy shortfall. If so, reset all existing energy convoy
         orders for the faction. TODO: Revisit and find a way to only reset specific base convoys.
Original Offset: 004F4DC0
Return Value: n/a
Status: Complete
*/
void __cdecl base_energy_costs() {
    if ((*BaseCurrent)->energy_surplus >= 0 || *VehCurrentCount <= 0) {
        return;
    }
    uint32_t faction_id = (*BaseCurrent)->faction_id_current;
    for (int i = 0; i < *VehCurrentCount; i++) {
        if (Vehs[i].faction_id == faction_id 
            && VehPrototypes[Vehs[i].proto_id].plan == PLAN_SUPPLY_CONVOY
            && Vehs[i].order == ORDER_CONVOY && Vehs[i].order_auto_type == RSC_ENERGY
            && base_who(Vehs[i].x, Vehs[i].y) >= 0) {
            Vehs[i].order = ORDER_NONE;
        }
    }
}

/*
Purpose: Add the current base's labs to its faction's research. A punishment sphere halves them,
         and none count early on: before turn 5 on the two easiest levels, or for five turns per
         point of negative research rating. The rating (clamped to -5..+5) scales the labs by 10%
         a point, the fractional remainder carrying over between bases and turns.
Original Offset: 004F4E80
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch12.log)
*/
void __cdecl base_research() {
    if (*GameRules & RULES_SCN_NO_TECH_ADVANCES) {
        return;
    }
    uint32_t faction_id = (*BaseCurrent)->faction_id_current;
    int labs = (*BaseCurrent)->labs_total;
    if (has_fac_built(FAC_PUNISHMENT_SPHERE, *BaseIDCurrentSelected)) {
        labs /= 2;
    }
    PlayerData *player = &PlayersData[faction_id];
    if (*TurnCurrentNum < player->soc_effect_pending.research * -5) {
        labs = 0;
    }
    if (*TurnCurrentNum < 5 && player->diff_level < DLVL_TALENT) {
        labs = 0;
    }
    player->labs_total += labs;
    int rating = player->soc_effect_pending.research;
    rating = rating < -5 ? -5 : (rating > 5 ? 5 : rating);
    int value = (rating + 10) * labs * 10;
    int remainder = player->net_random_event + value % 100;
    player->net_random_event = remainder;
    if (remainder > 99) {
        value += 100;
        player->net_random_event = remainder - 100;
    }
    tech_research(faction_id, value / 100);
}

/*
Purpose: Choose the best specialist type available to the current base, weighing psych triple
         and, once the base is big enough for specialists, economy and labs too. Obsolescence is
         not checked here, unlike in base_yield().
Original Offset: n/a (inlined in base_doctors)
Return Value: Citizen type
Status: Complete
*/
static int base_doctor_type() {
    int best_value = -999;
    int best_id = 0;
    for (int citizen_id = 0; citizen_id < MaxSpecialistNum; citizen_id++) {
        if (!has_tech(Citizen[citizen_id].preq_tech, (*BaseCurrent)->faction_id_current)) {
            continue;
        }
        int value = Citizen[citizen_id].psych_bonus * 3;
        if ((*BaseCurrent)->population_size >= (int)Rules->min_base_size_specialists) {
            value += Citizen[citizen_id].ops_bonus + Citizen[citizen_id].research_bonus;
        }
        if (best_value < value) {
            best_id = citizen_id;
            best_value = value;
        }
    }
    return best_id;
}

/*
Purpose: Settle the current base's drones by moving citizens between work and doctoring, for AI
         bases and human bases whose governor manages citizens. While drones outnumber talents
         (counting one more if a rioting base under weak police is about to grow) and the base
         is fed, a specialist is added; otherwise a specialist goes back to work as long as that
         doesn't start a riot (Telepathic Matrix bases ignore drones). Up to nine steps. A human
         player who could have used the psych slider instead is told so.
Original Offset: 004F5F30
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch15.log)
*/
void __cdecl base_doctors() {
    Base *base = *BaseCurrent;
    uint32_t faction_id = base->faction_id_current;
    int growing = 0; // a rioting base about to grow needs a spare talent
    if ((base->state & BSTATE_DRONE_RIOTS_ACTIVE)
        && PlayersData[faction_id].soc_effect_pending.police
            + has_fac(FAC_BROOD_PIT, *BaseIDCurrentSelected, 0) * 2 < -1) {
        int base_id = *BaseIDCurrentSelected;
        Base *b = &Bases[base_id];
        int ascetic = (SecretProject->ascetic_virtues >= 0
            && Bases[SecretProject->ascetic_virtues].faction_id_current == b->faction_id_current) ? 2 : 0;
        int pop_level = b->population_size - ascetic + Players[b->faction_id_current].rule_population;
        if ((pop_level < (int)Rules->pop_limit_sans_hab_complex || has_fac(FAC_HAB_COMPLEX, base_id, 0))
            && (pop_level < (int)Rules->pop_limit_sans_hab_dome || has_fac(FAC_HABITATION_DOME, base_id, 0))
            && ((((SecretProject->cloning_vats >= 0
                && Bases[SecretProject->cloning_vats].faction_id_current == faction_id)
                || *BaseCurrentGrowthRate > 5)
                && Rules->nutrient_req_citizen
                && (int)Rules->nutrient_req_citizen <= base->nutrient_surplus)
                || cost_factor(faction_id, RSC_NUTRIENTS, base_id) * (base->population_size + 1)
                    <= base->nutrient_surplus + base->nutrients_accumulated)) {
            growing = 1;
        }
    }
    for (int step = 1; step < 10; step++) {
        base = *BaseCurrent;
        if (is_human(faction_id) && (!(base->governor & GOV_MANAGES_CITIZENS_SPECS)
            || !(base->governor & GOV_ACTIVE))) {
            return;
        }
        BOOL telepathic = SecretProject->telepathic_matrix >= 0
            && Bases[SecretProject->telepathic_matrix].faction_id_current == faction_id;
        if (*BaseYieldNutrientMet && base->drone_total + growing > base->talent_total && !telepathic) {
            int citizen_id = base_doctor_type();
            base = *BaseCurrent;
            int slot = base->specialist_total++;
            base_spec_set(slot, citizen_id);
            *BaseComputeID = *BaseIDCurrentSelected;
            base_support();
            base_yield();
            base_nutrient();
            base_minerals();
            base_energy();
            base = *BaseCurrent;
            if (base->drone_total + growing <= base->talent_total) {
                // riots quelled with doctors: suggest the psych slider to the player at the controls
                if (!*ControlUpkeepA || faction_id != (uint32_t)*MapWinFactionID
                    || PlayersData[faction_id].se_alloc_psych > 1
                    || base->specialist_total - base->specialist_unk_1 < 2) {
                    return;
                }
                int queue = base->queue_production_id[0];
                if (queue == -FAC_RECREATION_COMMONS || queue == -FAC_HOLOGRAM_THEATRE
                    || queue == -FAC_PARADISE_GARDEN || queue == -FAC_RESEARCH_HOSPITAL
                    || queue == -FAC_NANOHOSPITAL) {
                    return;
                }
                // the original compares with +8 here rather than -FAC_NETWORK_NODE
                if (queue == FAC_NETWORK_NODE && SecretProject->virtual_world >= 0
                    && Bases[SecretProject->virtual_world].faction_id_current == faction_id) {
                    return;
                }
                if (Players[faction_id].rule_flags & RFLAG_ALIEN) {
                    popb("PSYCHREQUEST", 0, -1, "Alopdir.pcx", 0);
                } else {
                    popb("PSYCHREQUEST", 0, -1, "talent_sm.pcx", 0);
                }
                return;
            }
            continue;
        }
        int specialists = base->specialist_total;
        if (specialists - base->specialist_unk_1 <= 0 || (base->state & BSTATE_DRONE_RIOTS_ACTIVE)) {
            return;
        }
        if (base->drone_total && base->drone_total + growing >= base->talent_total && !telepathic) {
            return;
        }
        base->specialist_total = specialists - 1;
        *BaseComputeID = *BaseIDCurrentSelected;
        base_support();
        base_yield();
        base_nutrient();
        base_minerals();
        base_energy();
        base = *BaseCurrent;
        if (base->drone_total + growing > base->talent_total
            && !(SecretProject->telepathic_matrix >= 0
                && Bases[SecretProject->telepathic_matrix].faction_id_current == faction_id)) {
            // that started a riot: put a specialist back
            int citizen_id = base_doctor_type();
            base = *BaseCurrent;
            int slot = base->specialist_total++;
            base_spec_set(slot, citizen_id);
            return;
        }
        if (specialists <= base->specialist_total) {
            return;
        }
    }
}

/*
Purpose: Calculate facility maintenance cost for specified faction.
Original Offset: 004F6510
Return Value: Facility maintenance cost
Status: Complete
*/
uint32_t __cdecl fac_maint(uint32_t facility_id, uint32_t faction_id) {
    if (facility_id == FAC_COMMAND_CENTER) {
        int reactor = best_reactor(faction_id);
        int diff_factor = (PlayersData[faction_id].diff_level + 1) / 2;
        if (reactor < 0 || diff_factor < 0) {
            return 0;
        }
        return (reactor > diff_factor) ? diff_factor : reactor;
    }
    int bonus_count = Players[faction_id].faction_bonus_count;
    for (int i = 0; i < bonus_count; i++) {
        int bonus_id = Players[faction_id].faction_bonus_id[i];
        if ((bonus_id == RULE_FACILITY || (bonus_id == RULE_FREEFAC
            && has_tech(Facility[Players[faction_id].faction_bonus_val1[i]].preq_tech, faction_id)))
            && Players[faction_id].faction_bonus_val1[i] == (int)facility_id) {
            return 0;
        }
    }
    return Facility[facility_id].maint;
}

/*
Purpose: Calculate overall maintenance cost for the currently selected base.
Original Offset: 004F65F0
Return Value: Base maintenance cost
Status: Complete
*/
void __cdecl base_maint() {
    uint32_t faction_id = (*BaseCurrent)->faction_id_current;
    for (int fac = 1; fac < FacilitySPStart; fac++) {
        if (has_fac_built(fac)) {
            uint32_t maint = fac_maint(fac, faction_id);
            if (has_project(SP_SELF_AWARE_COLONY, faction_id)) {
                if (PlayersData[faction_id].flags & PFLAG_SELF_AWARE_COLONY_LOST_MAINT) {
                    maint++; // attempt to even out maintenance costs from lossy integer division
                }
                if (maint & 1) {
                    PlayersData[faction_id].flags |= PFLAG_SELF_AWARE_COLONY_LOST_MAINT;
                } else {
                    PlayersData[faction_id].flags &= ~PFLAG_SELF_AWARE_COLONY_LOST_MAINT;
                }
                maint /= 2;
            }
            PlayersData[faction_id].energy_reserves -= maint;
            PlayersData[faction_id].maint_cost_total += maint;
            if (PlayersData[faction_id].energy_reserves < 0) {
                if (PlayersData[faction_id].diff_level <= DLVL_SPECIALIST
                    || (*BaseCurrent)->queue_production_id[0] == -fac) {
                    PlayersData[faction_id].energy_reserves = 0;
                } else {
                    set_fac(fac, *BaseIDCurrentSelected, false);
                    PlayersData[faction_id].energy_reserves 
                        = cost_factor(faction_id, RSC_MINERALS, -1) * Facility[fac].cost;
                    parse_say(1, (int)*(&Facility[fac].name), -1, -1);
                    popb("POWERSHORT", 0x10000, 14, "genwarning_sm.pcx", NULL);
                }
            }
        }
    }
}

/*
Purpose: Check if the initial base name is unique. If not, append a sequential number after it.
         The function has been rewritten to work as intended.
Original Offset: 004F7FE0
Return Value: n/a
Status: Complete
*/
void __cdecl make_base_unique(uint32_t base_id) {
    purge_spaces(Bases[base_id].name_string); // added to remove extraneous whitespace
    uint32_t found = 0;
    std::string core_unique_name;
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (base_id != (uint32_t)i && !strcmp(Bases[i].name_string, Bases[base_id].name_string)) {
            if (!found) { // only create core part of base name on first collision
                Bases[base_id].name_string[21] = 0;
                core_unique_name = Bases[base_id].name_string;
            }
            std::string unique_name = core_unique_name;
            unique_name += "-" + std::to_string(++found + 1); // start with appending "-2"
            strcpy_s(Bases[base_id].name_string, 25, unique_name.c_str());
            i = 0; // reset loop to verify base name is still unique
        }
    }
}

/*
Purpose: Check if a faction has the specified secret project built in a base they control.
Original Offset: 004F80D0
Return Value: Does faction have Secret Project? true/false
Status: Complete
*/
BOOL __cdecl has_project(uint32_t project_id, uint32_t faction_id) {
    int base_id = base_project(project_id);
    return (base_id < 0) ? false : (Bases[base_id].faction_id_current == faction_id);
}

/*
Purpose: Check whether a production item has already been picked by base_reset() for one of the
         governor priorities (explore, discover, build, conquer) at a value no higher than the one
         specified. Each skip flag leaves that priority's pick out of the check.
Original Offset: 004F8110
Return Value: Is the item claimed by an earlier pick? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch3.log)
*/
BOOL __cdecl base_claimed(int item_id, int value, BOOL skip_explore, BOOL skip_discover,
                          BOOL skip_build, BOOL skip_conquer) {
    if (!skip_explore && item_id == BaseGovPickID[0] && value >= BaseGovPickValue[0]) {
        return true;
    }
    if (!skip_discover && item_id == BaseGovPickID[1] && value >= BaseGovPickValue[1]) {
        return true;
    }
    if (!skip_build && item_id == BaseGovPickID[2] && value >= BaseGovPickValue[2]) {
        return true;
    }
    if (!skip_conquer && item_id == BaseGovPickID[3] && value >= BaseGovPickValue[3]) {
        return true;
    }
    return false;
}

/*
Purpose: Checks whether the facility (non-SP) has been build in the currently selected base.
Original Offset: 00500290
Return Value: Does current base have facility? true/false
Status: Complete
*/
BOOL __cdecl has_fac_built(uint32_t facility_id) {
    return (facility_id >= FacilityRepStart) ? false
        : has_fac_built(facility_id, *BaseIDCurrentSelected);
}

/*
Purpose: Check if the base already has a particular facility built.
Original Offset: n/a
Return Value: Does base have facility built? true/false
Status: Complete
*/
BOOL __cdecl has_fac_built(uint32_t facility_id, uint32_t base_id) {
    uint32_t offset;
    uint32_t mask;
    bitmask(facility_id, &offset, &mask);
    return (Bases[base_id].facilities_built[offset] & mask) != 0;
}

/*
Purpose: Get the current status of the specified project.
Original Offset: 005002E0
Return Value: Base id, if not built (-1) or destroyed (-2)
Status: Complete
*/
int __cdecl base_project(uint32_t project_id) {
    return *(&SecretProject->human_genome_project + project_id);
}

/*
Purpose: Count an alien faction's subspace generators that are big enough to work, optionally
         counting the ones in bases still too small.
Original Offset: 005247B0
Return Value: Working generators (0 for non-alien factions)
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
int __cdecl generators(uint32_t faction_id, int *inactive) {
    if (inactive) {
        *inactive = 0;
    }
    if (!(Players[faction_id].rule_flags & RFLAG_ALIEN)) {
        return 0;
    }
    int active = 0;
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        if (Bases[base_id].faction_id_current == faction_id
            && has_fac_built(FAC_SUBSPACE_GENERATOR, base_id)) {
            if (Bases[base_id].population_size < (int)Rules->size_base_subspace_gen) {
                if (inactive) {
                    (*inactive)++;
                }
            } else {
                active++;
            }
        }
    }
    return active;
}

/*
Purpose: Pick the enemy base two allied factions should plan an attack on: a base both are at war
         with that the first faction can see, closest (with a preference for the first faction's
         current target region) to the first faction's target and to both factions' nearest
         bases. The first faction's standing attack target wins outright.
Original Offset: 0054ACC0
Return Value: Base id to attack, or -1 if there is none
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch9.log)
*/
int __cdecl suggest_plan(uint32_t faction_id, uint32_t faction_id_2) {
    *DiploSecondFaction = faction_id_2;
    int dist_best = 9999;
    int base_id_best = -1;
    for (int i = 0; i < *BaseCurrentCount; i++) {
        uint32_t owner = Bases[i].faction_id_current;
        if (owner == faction_id || owner == faction_id_2
            || !(PlayersData[faction_id].diplo_treaties[owner] & DTREATY_VENDETTA)
            || !(PlayersData[faction_id_2].diplo_treaties[owner] & DTREATY_VENDETTA)
            || (owner != faction_id && !(Bases[i].visibility & (1 << faction_id)))) {
            continue;
        }
        int x = Bases[i].x, y = Bases[i].y;
        uint8_t region = map_loc(x, y)->region;
        int dist = 0;
        int x_target = PlayersData[faction_id].x_target;
        if (x_target >= 0) {
            int y_target = PlayersData[faction_id].y_target;
            int x_dist = abs(x_target - x);
            if (!*MapIsFlat && x_dist > (int)*MapLongitude) {
                x_dist = *MapLongitudeBounds - x_dist;
            }
            dist = vector_dist(x_dist, y_target - y) * 2;
            if (map_loc(x, y)->region != map_loc(x_target, y_target)->region) {
                dist += 0x200;
            }
        }
        if (base_find(x, y, faction_id, region, -1, -1) < 0) {
            base_find(x, y, faction_id);
            *BaseFindDist += 0x400;
        }
        int dist_own = ((x_target >= 0) + 1) * *BaseFindDist;
        if (base_find(x, y, faction_id_2, region, -1, -1) < 0) {
            base_find(x, y, faction_id_2);
            *BaseFindDist += 0x100;
        }
        int total = dist + dist_own + *BaseFindDist;
        if (PlayersData[faction_id].base_id_atk_target == i) {
            return i;
        }
        if (total < dist_best) {
            base_id_best = i;
            dist_best = total;
        }
    }
    return base_id_best;
}

/*
Purpose: Determine the faction's best base to attack the specified base from.
Original Offset: 0054AFA0
Return Value: Base id to attack from or 0 
Status: Complete
*/
int __cdecl attack_from(uint32_t base_id_to_atk, uint32_t faction_id) {
    uint32_t region_src = region_at(Bases[base_id_to_atk].x, Bases[base_id_to_atk].y);
    int base_id_atk_from = 0; // TODO: eventually change this to -1 and add handling to the return
    int search = 9999;        // value to prevent the edge case of base id 0 being incorrect target
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (Bases[i].faction_id_current == faction_id) {
            int x = Bases[i].x;
            int y = Bases[i].y;
            int compare = (x == PlayersData[faction_id].x_target
                && y == PlayersData[faction_id].y_target) 
                ? 0 : vector_dist(x, y, Bases[base_id_to_atk].x, Bases[base_id_to_atk].y);
            if (region_at(x, y) != region_src) {
                compare += 1000;
            }
            if (compare <= search) {
                search = compare;
                base_id_atk_from = i;
            }
        }
    }
    return base_id_atk_from;
}

/*
Purpose: Determine the value of the specified base between the requester and the respondent faction.
         This valuation could be triggered either from a gift, a threat or a base swap.
Original Offset: 0054CB50
Return Value: Value of base or -1 for invalid requests
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
int __cdecl value_of_base(int base_id, uint32_t faction_id_req, uint32_t faction_id_res, 
                          uint32_t overmatch_deg, BOOL tgl) {
    if (base_id < 0) {
        return -1;
    }
    int x = Bases[base_id].x;
    int y = Bases[base_id].y;
    int dist_factor = vulnerable(faction_id_req, x, y);
    if (dist_factor <= 0) {
        return -1;
    }
    uint32_t region_base = region_at(x, y);
    for (int i = 1; i < RadiusRange[6]; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius)) {
            int base_id_found = base_at(x_radius, y_radius);
            if (base_id_found >= 0) {
                uint32_t faction_id_base = Bases[base_id_found].faction_id_current;
                uint32_t region_found = region_at(x_radius, y_radius);
                if (faction_id_base != faction_id_req) {
                    if (region_base == region_found) {
                        if (faction_id_base == faction_id_res) {
                            dist_factor++;
                        } else {
                            break;
                        }
                    }
                } else {
                    if (region_base == region_found) {
                        dist_factor--;
                    }
                    break;
                }
            }
        }
    }
    int veh_id = stack_fix(veh_at(x, y)); // reason to define here rather than below?
    int most_reserves = PlayersData[faction_id_res].energy_reserves;
    if (PlayersData[faction_id_req].energy_reserves > most_reserves) {
        most_reserves = PlayersData[faction_id_req].energy_reserves;
    }
    uint32_t base_pop_factor = Bases[base_id].population_size;
    if (base_pop_factor < 3) {
        base_pop_factor = 3;
    }
    // Bug fix: treat value as unsigned, original uses signed which could cause incorrect valuation
    uint32_t value = ((most_reserves + 1000) / (dist_factor + 4)) * base_pop_factor;
    if (value < 100) {
        value = 100;
    }
    if (veh_who(x, y) < 0) {
        value /= 2;
    }
    uint32_t res_base_count_region = PlayersData[faction_id_res].region_total_bases[region_base];
    if (!res_base_count_region) {
        value *= 2;
    }
    uint32_t req_base_count_region = PlayersData[faction_id_req].region_total_bases[region_base];
    if (req_base_count_region == 1) {
        if (!res_base_count_region) {
            value *= 2;
        }
        if (res_base_count_region == 1) {
            value *= 2;
        }
    }
    if (tgl) {
        value *= 4;
    }
    if (res_base_count_region && req_base_count_region) {
        if (res_base_count_region >= req_base_count_region * 5) {
            value /= 2;
        }
        if (overmatch_deg) {
            value /= 2;
        }
    }
    uint32_t facil_count = 0;
    for (uint32_t fac = 1; fac < FacilitySPStart; fac++) {
        if (fac < FacilityRepStart && has_fac_built(fac, base_id)) {
            facil_count++;
            value += (Facility[fac].cost * facil_count);
        }
    }
    for (uint32_t proj = 0; proj < MaxSecretProjectNum; proj++) {
        if (base_project(proj) == base_id) {
            value += (Facility[FacilitySPStart + proj].cost * 25);
        }
    }
    for (int i = veh_top(veh_id); i >= 0; i = Vehs[i].next_veh_id_stack) {
        if (Vehs[i].faction_id == faction_id_req) {
            value += (VehPrototypes[Vehs[i].proto_id].cost * 2);
        }
    }
    if (!_stricmp(Players[faction_id_req].filename, "BELIEVE")) {
        value *= 2;
    }
    if (is_objective(base_id)) {
        value *= 4;
    }
    return value;
}

/*
Purpose: Determine ideal non-offense (defense, combat, recon) unit count for the specified base.
Original Offset: 00560B30
Return Value: Amount of non-offensive units needed (1-10)
Status: Complete
*/
uint32_t __cdecl garrison_check(uint32_t base_id) {
    int x = Bases[base_id].x;
    int y = Bases[base_id].y;
    uint32_t faction_id = Bases[base_id].faction_id_current;
    uint32_t sp_count = 0;
    for (uint32_t i = 0; i < MaxSecretProjectNum; i++) {
        if (base_project(i) == (int)base_id) {
            sp_count++;
        }
    }
    int garrison = (sp_count + 2) / 3 + (Bases[base_id].population_size + 1) / 4 + 1;
    BOOL is_obj = is_objective(base_id);
    if (has_fac_built(FAC_HEADQUARTERS, base_id) || bit_at(x, y) & BIT_UNK_40000000 || is_obj) {
        garrison++;
    }
    if (is_obj && PlayersData[faction_id].flags & PFLAG_STRAT_DEF_OBJECTIVES) {
        garrison++;
    }
    if (PlayersData[faction_id].region_base_plan[region_at(x, y)] == PLAN_COLONIZATION) {
        garrison--;
    }
    int faction_id_sea = zoc_sea(x, y, faction_id) - 1;
    if (faction_id_sea > 0
        && (has_treaty(faction_id, faction_id_sea, DTREATY_VENDETTA | DTREATY_WANT_REVENGE)
        || has_treaty(faction_id_sea, faction_id, DTREATY_WANT_REVENGE)
        || PlayersData[faction_id_sea].integrity_blemishes >= 4)) {
        garrison++;
    }
    int support = PlayersData[faction_id].soc_effect_pending.support;
    if (support <= -4) {
        garrison--;
    }
    if (support <= -3 && garrison > 2) {
        garrison--;
    }
    return range(garrison, 1, 10);
}

/*
Purpose: Determine the number of defensive units needed to protect the specified base.
Original Offset: 00560D30
Return Value: Amount of defensive units needed (1-8)
Status: Complete
*/
uint32_t __cdecl defensive_check(uint32_t base_id) {
    uint32_t defenses = garrison_check(base_id);
    if (defenses > 5) {
        defenses--;
    }
    if (defenses > 2) {
        defenses--;
    }
    return defenses;
}

/*
Purpose: Set the AI goals for gathering passengers at a base about to build a transport or a
         colony pod. Bases are flagged as assembling; colonization is wanted while fewer vendetta
         enemies lack a foothold in the region than the faction's growth drive allows, and other
         plans (or a colony pod in a region marked for it) also call for defense, attack and
         staging goals; an exploration plan adds a scouting goal.
Original Offset: 00564B90
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch17.log)
*/
void __cdecl assemble_passengers(int base_id, int plan, int priority) {
    int threats = 0;
    uint32_t faction_id = Bases[base_id].faction_id_current;
    int x = Bases[base_id].x;
    int y = Bases[base_id].y;
    uint32_t region = map_loc(x, y)->region;
    for (uint32_t faction_id_2 = 1; faction_id_2 < MaxPlayerNum; faction_id_2++) {
        if (faction_id_2 != faction_id
            && (PlayersData[faction_id].diplo_treaties[faction_id_2] & DTREATY_VENDETTA)
            && (!PlayersData[faction_id_2].region_total_bases[region]
                || !contiguous(faction_id_2, faction_id, region, 1))) {
            threats++;
        }
    }
    Bases[base_id].state |= BSTATE_UNK_1;
    Bases[base_id].unk_1 = 2;
    BOOL skip_military = false;
    if (plan != PLAN_COLONIZATION || (PlayersData[faction_id].unk_77[region] & 0x800)) {
        int limit = PlayersData[faction_id].ai_growth - PlayersData[faction_id].ai_fight
            - *TurnCurrentNum / 100 + 2;
        limit = limit < 0 ? 0 : (limit > 99 ? 99 : limit);
        if (threats < limit) {
            int goal_priority = stack_check(veh_at(x, y), 2, PLAN_COLONIZATION, faction_id, -1)
                ? 1 : priority;
            add_goal(faction_id, AI_GOAL_COLONIZE, goal_priority, x, y, -1);
        }
        skip_military = plan < PLAN_RECONNAISANCE;
    }
    if (!skip_military) {
        add_goal(faction_id, AI_GOAL_DEFEND, priority, x, y, -1);
        add_goal(faction_id, AI_GOAL_ATTACK, priority, x, y, -1);
        add_goal(faction_id, AI_GOAL_UNK_3, priority, x, y, -1);
    }
    if (plan == PLAN_NAVAL_TRANSPORT) {
        add_goal(faction_id, AI_GOAL_SCOUT, 1, x, y, -1);
    }
}

/*
Purpose: Pick the base a native unit should head for: the nearest reachable base relative to its
         size (energy and mineral intake), with half the distance for bases holding an alien
         artifact or that are the unit's home, and double for a base of another faction whose
         Planet rating is positive and whose land is unspoiled. Sea lurks also reach bases on
         the sea they swim in.
Original Offset: 005665D0
Return Value: Base id or -1; BaseFindDist holds the winning score
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch17.log)
*/
int __cdecl alien_base(int veh_id, int x, int y) {
    int best_value = 9999;
    *BaseFindDist = 9999;
    int best_id = -1;
    uint8_t region = map_loc(x, y)->region;
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        Base *base = &Bases[base_id];
        if (region <= RegionBounds && region != map_loc(base->x, base->y)->region
            && (Vehs[veh_id].proto_id != BSC_SEALURK || !base_on_sea(base_id, region))) {
            continue;
        }
        int value = (vector_dist(x, y, base->x, base->y) * 32)
            / (base->energy_intake_2 + 32 + base->mineral_intake_2);
        if (stack_check(veh_at(base->x, base->y), 2, PLAN_ALIEN_ARTIFACT, -1, -1)) {
            value /= 2;
        }
        if (base_id == Vehs[veh_id].home_base_id) {
            value /= 2;
        }
        if (Vehs[veh_id].order_auto_type != base->faction_id_current
            && PlayersData[base->faction_id_current].soc_effect_active.planet > 0
            && !base->eco_damage) {
            value *= 2;
        }
        if (value <= best_value) {
            best_id = base_id;
            best_value = value;
        }
    }
    if (best_id >= 0) {
        *BaseFindDist = best_value;
    }
    return best_id;
}

/*
Purpose: Determine if base is a port.
Original Offset: 00579A00
Return Value: Is base a port? true/false
Status: Complete
*/
BOOL __cdecl is_port(uint32_t base_id, BOOL is_base_radius) {
    return is_coast(Bases[base_id].x, Bases[base_id].y, is_base_radius);
}

/*
Purpose: Calculate how vulnerable the coordinates are for the specified faction based on how far
         away this tile is from the faction's Headquarters.
Original Offset: 0059E980
Return Value: Radial distance between coordinates and faction's HQ or 12 if no HQ/bases
Status: Complete
*/
int __cdecl vulnerable(uint32_t faction_id, int x, int y) {
    int dist = 12; // default value for no bases or no HQ
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (Bases[i].faction_id_current == faction_id && has_fac_built(FAC_HEADQUARTERS, i)) {
            dist = vector_dist(x, y, Bases[i].x, Bases[i].y);
            break;
        }
    }
    return dist;
}

/*
Purpose: Scale a value by the specified faction's difficulty level: the rating grows faster at the
         higher levels and Intense Rivalry adds one more step.
Original Offset: 005ABFF0
Return Value: Scaled value
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-veh-helpers.log)
*/
int __cdecl get_rating(uint32_t faction_id, int value) {
    int diff_level = PlayersData[faction_id].diff_level;
    int rating = diff_level;
    for (int i = 3; i <= diff_level; i++) {
        rating += (i - 1) / 2;
    }
    if (*GameRules & RULES_INTENSE_RIVALRY) {
        rating++;
    }
    return ((rating + 6) * value) / 160;
}

/*
Purpose: Determine whether the specified base is considered an objective.
Original Offset: 005AC060
Return Value: Is base an objective? true/false
Status: Complete
*/
BOOL __cdecl is_objective(uint32_t base_id) {
    if (*GameRules & RULES_SCN_VICT_ALL_BASE_COUNT_OBJ || Bases[base_id].event & BEVENT_OBJECTIVE) {
        return true;
    }
    if (*GameRules & RULES_SCN_VICT_SP_COUNT_OBJ) {
        for (int i = 0; i < MaxSecretProjectNum; i++) {
            if (base_project(i) == (int)base_id) {
                return true;
            }
        }
    }
    if (*GameState & STATE_SCN_VICT_BASE_FACIL_COUNT_OBJ
        && has_fac(*ScnVictFacilityObj, base_id, 0)) {
        return true;
    }
    return false;
}

/*
Purpose: Count a faction's scenario objectives: objective units (at a friendly objective base or
         headquarters when the rules say so), objective bases, population, techs, credits, the
         victory facility, terrain enhancements and territory in its tiles, secret projects, and
         with include_pact the objectives of its pact partners under cooperative victory.
Original Offset: 005AC110
Return Value: Objective count (0 for a faction that is not alive)
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch6.log)
*/
int __cdecl num_objectives(uint32_t faction_id, BOOL include_pact) {
    if (!is_alive(faction_id)) {
        return 0;
    }
    int objectives = PlayersData[faction_id].objective_kills;
    uint32_t offset, mask;
    for (int i = 0; i < *VehCurrentCount; i++) {
        if (Vehs[i].faction_id != faction_id || !(Vehs[i].flags & VFLAG_IS_OBJECTIVE)) {
            continue;
        }
        if (*GameRules & RULES_SCN_VICT_OBJ_UNITS_REACH_FRIEND_OBJ_BASE) {
            int base_id = base_at(Vehs[i].x, Vehs[i].y);
            if (base_id < 0) {
                continue;
            }
            BOOL counts = (*GameRules & RULES_SCN_VICT_ALL_BASE_COUNT_OBJ)
                || (Bases[base_id].event & BEVENT_OBJECTIVE);
            if (!counts && (*GameRules & RULES_SCN_VICT_SP_COUNT_OBJ)) {
                for (uint32_t p = 0; p < MaxSecretProjectNum; p++) {
                    if ((&SecretProject->human_genome_project)[p] == base_id) {
                        counts = true;
                        break;
                    }
                }
            }
            if (!counts) {
                if (!(*GameState & STATE_SCN_VICT_BASE_FACIL_COUNT_OBJ) || *ScnVictFacilityObj > 0x40) {
                    continue;
                }
                bitmask(*ScnVictFacilityObj, &offset, &mask);
                if (!(Bases[base_id].facilities_built[offset] & mask)) {
                    continue;
                }
            }
            uint32_t owner = Bases[base_id].faction_id_current;
            if (owner == faction_id) {
                objectives++;
            } else if ((PlayersData[faction_id].diplo_treaties[owner] & DTREATY_PACT)
                && (*GameRules & RULES_VICTORY_COOPERATIVE)) {
                objectives++;
            }
        } else if (*GameRules & RULES_SCN_VICT_OBJ_UNITS_REACH_FRIEND_HQ_BASE) {
            int base_id = base_at(Vehs[i].x, Vehs[i].y);
            if (base_id < 0) {
                continue;
            }
            bitmask(FAC_HEADQUARTERS, &offset, &mask);
            if (!(Bases[base_id].facilities_built[offset] & mask)) {
                continue;
            }
            uint32_t owner = Bases[base_id].faction_id_current;
            if (owner == faction_id) {
                objectives++;
            } else if ((PlayersData[faction_id].diplo_treaties[owner] & DTREATY_PACT)
                && (*GameRules & RULES_VICTORY_COOPERATIVE)) {
                objectives++;
            }
        } else {
            objectives++;
        }
    }
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (Bases[i].faction_id_current != faction_id) {
            continue;
        }
        if (*GameState & STATE_SCN_VICT_POPULATION_COUNT_OBJ) {
            objectives += Bases[i].population_size;
        }
        if ((*GameRules & RULES_SCN_VICT_ALL_BASE_COUNT_OBJ) || (Bases[i].event & BEVENT_OBJECTIVE)) {
            objectives++;
        }
    }
    if (*GameState & STATE_SCN_VICT_TECH_COUNT_OBJ) {
        for (int tech_id = 0; tech_id < MaxTechnologyNum; tech_id++) {
            if (has_tech(tech_id, faction_id)) {
                objectives++;
            }
        }
        objectives += PlayersData[faction_id].theory_of_everything;
    }
    if (*GameState & STATE_SCN_VICT_CREDITS_COUNT_OBJ) {
        objectives += PlayersData[faction_id].energy_reserves;
    }
    if (*GameState & STATE_SCN_VICT_BASE_FACIL_COUNT_OBJ) {
        for (int i = 0; i < *BaseCurrentCount; i++) {
            if (Bases[i].faction_id_current == faction_id && *ScnVictFacilityObj <= 0x40) {
                bitmask(*ScnVictFacilityObj, &offset, &mask);
                if (Bases[i].facilities_built[offset] & mask) {
                    objectives++;
                }
            }
        }
    }
    if (*GameState & STATE_SCN_VICT_TERRAIN_ENH_COUNT_OBJ) {
        // the terrain enhancement bits the objective counts
        const uint32_t enhancements = 0x81E4885C;
        for (int y = 0; y < (int)*MapLatitudeBounds; y++) {
            for (int x = y & 1; x < (int)*MapLongitudeBounds; x += 2) {
                Map *tile = map_loc(x, y);
                if (tile->territory == (int)faction_id) {
                    objectives += bit_count(tile->bit & enhancements);
                }
            }
        }
    }
    if (*GameState & STATE_SCN_VICT_TERRITORY_COUNT_OBJ) {
        for (int y = 0; y < (int)*MapLatitudeBounds; y++) {
            for (int x = y & 1; x < (int)*MapLongitudeBounds; x += 2) {
                if (map_loc(x, y)->territory == (int)faction_id) {
                    objectives++;
                }
            }
        }
    }
    if (*GameRules & RULES_SCN_VICT_SP_COUNT_OBJ) {
        for (uint32_t p = 0; p < MaxSecretProjectNum; p++) {
            int base_id = (&SecretProject->human_genome_project)[p];
            if (base_id >= 0 && Bases[base_id].faction_id_current == faction_id) {
                objectives++;
            }
        }
    }
    if (include_pact && (*GameRules & RULES_VICTORY_COOPERATIVE)) {
        for (uint32_t i = 1; i < MaxPlayerNum; i++) {
            if (i != faction_id && (PlayersData[faction_id].diplo_treaties[i] & DTREATY_PACT)) {
                objectives += num_objectives(i, false);
            }
        }
    }
    return objectives;
}

/*
Purpose: Find the faction holding the most scenario objectives. Optionally returns that faction and
         how many other factions tie with it; with cooperative victory, pact mates of the leader
         do not count as ties. On an equal count the later faction id wins.
Original Offset: 005AC5A0
Return Value: Highest objective count
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-veh-helpers.log)
*/
int __cdecl most_objectives(int *faction_id_most, int *tie_count) {
    int faction_id_best = 0;
    int objectives_best = 0;
    if (tie_count) {
        *tie_count = 0;
    }
    for (int i = 1; i < MaxPlayerNum; i++) {
        int objectives = num_objectives(i, *GameRules & RULES_VICTORY_COOPERATIVE);
        if (objectives >= objectives_best) {
            if (tie_count) {
                if (objectives > objectives_best) {
                    *tie_count = 0;
                } else if (!(PlayersData[i].diplo_treaties[faction_id_best] & DTREATY_PACT)
                    || !(*GameRules & RULES_VICTORY_COOPERATIVE)) {
                    (*tie_count)++;
                }
            }
            objectives_best = objectives;
            faction_id_best = i;
        }
    }
    if (faction_id_most) {
        *faction_id_most = faction_id_best;
    }
    return objectives_best;
}

/*
Purpose: Check if specified faction is currently building Ascent to Transcendence. This code isn't
         used by original game. There was also a bug where it compares to a non-negative queue id.
Original Offset: 005AC630
Return Value: Is faction transcending? true/false
Status: Complete
*/
BOOL __cdecl transcending(int faction_id) {
    if (!ascending(faction_id)) {
        return false;
    }
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (Bases[i].faction_id_current == faction_id
            && Bases[i].queue_production_id[0] == -FAC_ASCENT_TO_TRANSCENDENCE) {
            return true;
        }
    }
    return false;
}

/*
Purpose: Check if Voice of Planet has been built that starts the Ascent to Transcendence sequence.
Original Offset: 005AC680
Return Value: Has Voice of Planet been built? true/false
Status: Complete
*/
BOOL __cdecl ascending(int UNUSED(faction_id)) {
    return base_project(SP_VOICE_OF_PLANET) != SP_Unbuilt;
}

/*
Purpose: Score every faction (objectives alone when the scenario demands them, otherwise population,
         techs, secret projects, objectives and the military weighted by weapon strength) and
         record the scores in the turn history. With is_full set the factions are ranked, the
         top and bottom human players noted, and the "great satan" is picked: the top faction
         when it threatens everyone, or every strong human during the climactic battle. Its
         enemies then roll to betray it.
Original Offset: 005AC690
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch11.log)
*/
void __cdecl rankings(BOOL is_full) {
    int weapon_best = 1;
    for (int proto_id = MaxVehProtoFactionNum; proto_id < MaxVehProtoNum; proto_id++) {
        if ((VehPrototypes[proto_id].flags & PROTO_ACTIVE)
            && (VehPrototypes[proto_id].flags & PROTO_TYPED_COMPLETE)) {
            int offense = Weapon[VehPrototypes[proto_id].weapon_id].offense_rating;
            if (offense >= weapon_best && offense < 99) {
                weapon_best = offense;
            }
        }
    }
    for (uint32_t faction_id = 0; faction_id < MaxPlayerNum; faction_id++) {
        if (is_full) {
            PlayersData[faction_id].ranking = 0;
        }
        if (!faction_id) {
            continue;
        }
        if (!is_alive(faction_id)) {
            FactionRankingsUnk[faction_id] = 0;
        } else if ((*ObjectiveReqVictory < 9000 || *ObjectivesSuddenDeathVictory < 9000)
            && !*ObjectiveAchievePts) {
            FactionRankingsUnk[faction_id] = num_objectives(faction_id, false) * 10;
        } else {
            int score = (PlayersData[faction_id].theory_of_everything + PlayersData[faction_id].pop_total) * 4;
            for (int tech_id = 0; tech_id < MaxTechnologyNum; tech_id++) {
                if (has_tech(tech_id, faction_id)) {
                    score += Technology[tech_id].growth_value + Technology[tech_id].tech_value
                        + Technology[tech_id].wealth_value + Technology[tech_id].power_value;
                }
            }
            for (uint32_t sp = 0; sp < MaxSecretProjectNum; sp++) {
                int base_id = (&SecretProject->human_genome_project)[sp];
                if (base_id >= 0 && Bases[base_id].faction_id_current == faction_id) {
                    score += 10;
                }
            }
            score += num_objectives(faction_id, false) * *ObjectiveAchievePts;
            for (int proto_id = 0; proto_id < MaxVehProtoNum; proto_id++) {
                if (proto_id < MaxVehProtoFactionNum
                    && !has_tech(VehPrototypes[proto_id].preq_tech, faction_id)) {
                    continue;
                }
                uint32_t count = PlayersData[faction_id].proto_id_active[proto_id];
                uint32_t value = VehPrototypes[proto_id].cost * count;
                if (count >= 0xFB || !value) {
                    continue;
                }
                int offense = Weapon[VehPrototypes[proto_id].weapon_id].offense_rating;
                if (!offense) {
                    value = (int)value >> 2;
                } else if (offense < 0) {
                    value /= 2;
                } else if (offense <= 98) {
                    value = (offense / weapon_best) * value;
                }
                score += value;
            }
            FactionRankingsUnk[faction_id] = score;
        }
        if (*TurnCurrentNum < 1000) {
            FactionTurnMight[*TurnCurrentNum * MaxPlayerNum + faction_id] = (int16_t)FactionRankingsUnk[faction_id];
        }
    }
    if (!is_full) {
        return;
    }
    for (int rank = 7; rank > 0; rank--) {
        int score_best = -1;
        uint32_t faction_id_best = 0;
        for (uint32_t faction_id = 1; faction_id < MaxPlayerNum; faction_id++) {
            if ((int)FactionRankingsUnk[faction_id] > score_best) {
                faction_id_best = faction_id;
                score_best = FactionRankingsUnk[faction_id];
            }
        }
        FactionRankingsUnk[faction_id_best] = -1;
        PlayersData[faction_id_best].ranking = rank;
    }
    for (int i = 0; i < MaxPlayerNum; i++) {
        FactionRankings[i] = 0;
    }
    for (uint32_t faction_id = 1; faction_id < MaxPlayerNum; faction_id++) {
        if (is_alive(faction_id)) {
            FactionRankings[PlayersData[faction_id].ranking] = faction_id;
        }
    }
    *RankingFactionIDUnk1 = 0;
    *RankingFactionIDUnk2 = 0;
    for (int i = 7; i >= 0; i--) {
        if (is_human(FactionRankings[i])) {
            *RankingFactionIDUnk1 = FactionRankings[i];
            break;
        }
    }
    for (int i = 0; i < MaxPlayerNum; i++) {
        if (is_human(FactionRankings[i])) {
            *RankingFactionIDUnk2 = FactionRankings[i];
            break;
        }
    }
    uint32_t faction_id_satan = great_satan(FactionRankings[7], false) ? FactionRankings[7] : 0;
    if (climactic_battle()) {
        for (uint32_t faction_id = 1; faction_id < MaxPlayerNum; faction_id++) {
            if (is_human(faction_id)
                && (PlayersData[faction_id].diff_level > 3 || (*GameRules & RULES_INTENSE_RIVALRY))) {
                faction_id_satan = faction_id;
            }
        }
    }
    if (!faction_id_satan) {
        *GameState &= ~STATE_UNK_200;
        return;
    }
    *GameState |= STATE_UNK_200;
    int vendettas = 0;
    for (uint32_t faction_id = 1; faction_id < MaxPlayerNum; faction_id++) {
        if (PlayersData[faction_id].diplo_treaties[faction_id_satan] & DTREATY_VENDETTA) {
            vendettas++;
        }
    }
    for (uint32_t faction_id = 1; faction_id < MaxPlayerNum; faction_id++) {
        uint32_t treaties = PlayersData[faction_id].diplo_treaties[faction_id_satan];
        if (is_human(faction_id) || !(treaties & (DTREATY_PACT | DTREATY_TREATY | DTREATY_TRUCE))
            || (treaties & (DTREATY_UNK_800 | DTREATY_SHALL_BETRAY | DTREATY_PACT))
            || ((treaties & DTREATY_UNK_4000000) && !(treaties & DTREATY_UNK_800))) {
            continue;
        }
        int diff_level = (*GameRules & RULES_INTENSE_RIVALRY) ? 5
            : PlayersData[faction_id_satan].diff_level;
        uint32_t treaties_satan = PlayersData[faction_id_satan].diplo_treaties[faction_id];
        int rep = reputation(faction_id_satan, faction_id);
        BOOL climactic = climactic_battle();
        uint32_t flags_ext = PlayersData[faction_id].flags_ext;
        int range = ((vendettas - PlayersData[faction_id].ai_fight) + 2) * 0x100;
        int roll = (range == 1 || range - 1 < 0) ? 0 : _rand() % range;
        if (roll < (rep + (climactic ? 4 : 0) + (int)((flags_ext >> 3) & 4)
            + ((treaties_satan & DTREATY_WANT_REVENGE) ? 4 : 0)) * diff_level) {
            set_treaty(faction_id, faction_id_satan, DTREATY_SHALL_BETRAY, true);
        }
    }
}

/*
Purpose: Mark the base and its tile as spotted by the faction and record the population size the
         faction now knows about. The first time the faction at the controls sees the tile
         (outside omniscient view and without a revealed map) the tile is flagged for the redraw.
Original Offset: 005B57D0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch2.log)
*/
void __cdecl spot_base(int base_id, int faction_id) {
    Base *base = &Bases[base_id];
    base->visibility |= (1 << faction_id);
    base->faction_pop_size_intel[faction_id] = base->population_size;
    int x = base->x;
    int y = base->y;
    if (on_map(x, y)) {
        Map *tile = map_loc(x, y);
        if (faction_id == *FactionIDCurrent && !(tile->visibility & (1 << faction_id))
            && !(*GameState & STATE_OMNISCIENT_VIEW)
            && !(PlayersData[faction_id].flags & PFLAG_MAP_REVEALED)) {
            tile->bit2 |= BIT2_UNK_400000;
            *UnkBitfield1 |= 1;
        }
        tile->visibility |= (1 << faction_id);
        synch_bit(x, y, faction_id);
    }
}

/*
Purpose: Determine if the facility is redundant due to a Secret Project counting as that facility.
Original Offset: 005BA030
Return Value: Is facility redundant? true/false
Status: Complete
*/
BOOL __cdecl redundant(int facility_id, uint32_t faction_id) {
    uint32_t project_id;
    switch (facility_id) {
      case FAC_NAVAL_YARD:
        project_id = SP_MARITIME_CONTROL_CENTER;
        break;
      case FAC_PERIMETER_DEFENSE:
        project_id = SP_CITIZENS_DEFENSE_FORCE;
        break;
      case FAC_COMMAND_CENTER:
        project_id = SP_COMMAND_NEXUS;
        break;
      case FAC_BIOENHANCEMENT_CENTER:
        project_id = SP_CYBORG_FACTORY;
        break;
      case FAC_QUANTUM_CONVERTER:
        project_id = SP_SINGULARITY_INDUCTOR;
        break;
      case FAC_AEROSPACE_COMPLEX:
        project_id = SP_CLOUDBASE_ACADEMY;
        break;
      case FAC_ENERGY_BANK:
        project_id = SP_PLANETARY_ENERGY_GRID;
        break;
      default:
        return false;
    }
    return has_project(project_id, faction_id);
}

/*
Purpose: Determine if the provided faction can build a specific facility or Secret Project in the
         specified base. Checks are included to prevent SMACX specific facilities from being built
         in SMAC mode.
Original Offset: 005BA0E0
Return Value: Is facility or Secret Project available to faction, base and game mode? true/false
Status: Complete
*/
BOOL __cdecl facility_avail(uint32_t facility_id, uint32_t faction_id, int base_id, 
                            int queue_count) {
    // initial checks
    if (!facility_id || (facility_id == FAC_SKUNKWORKS && *DiffLevelCurrent <= DLVL_SPECIALIST)
        || (facility_id >= FAC_HUMAN_GENOME_PROJ && *GameRules & RULES_SCN_NO_BUILDING_SP)) {
        return false; // Skunkworks removed if there are no prototype costs
    }
    if (facility_id == FAC_ASCENT_TO_TRANSCENDENCE) { // at top since anyone can build it
        return ascending(faction_id) && *GameRules & RULES_VICTORY_TRANSCENDENCE
            && _stricmp(Players[faction_id].filename, "CARETAKE"); // bug fix for Caretakers
    }
    if (!has_tech(Facility[facility_id].preq_tech, faction_id)) { // Check tech for facility + SP
        return false;
    }
    // Secret Project availability
    if (!*ExpansionEnabled && (facility_id == FAC_MANIFOLD_HARMONICS
        || facility_id == FAC_NETHACK_TERMINUS || facility_id == FAC_CLOUDBASE_ACADEMY
        || facility_id == FAC_PLANETARY_ENERGY_GRID)) {
        return false;
    }
    if (facility_id == FAC_VOICE_OF_PLANET && !_stricmp(Players[faction_id].filename, "CARETAKE")) {
        return false; // shifted Caretaker Ascent check to top (never reached here)
    }
    if (facility_id >= FAC_HUMAN_GENOME_PROJ) {
        return base_project(facility_id - FAC_HUMAN_GENOME_PROJ) == SP_Unbuilt;
    }
    // Facility availability
    if (base_id < 0) {
        return true;
    }
    if (has_fac(facility_id, base_id, queue_count)) {
        return false; // already built or in queue
    }
    if (redundant(facility_id, faction_id)) {
        return false; // has SP that counts as facility
    }
    switch (facility_id) { // consolidated into switch to improve readability and performance
      case FAC_RECYCLING_TANKS:
        return !has_fac(FAC_PRESSURE_DOME, base_id, queue_count); // count as Recycling Tank, skip
      case FAC_TACHYON_FIELD:
        return has_fac(FAC_PERIMETER_DEFENSE, base_id, queue_count)
            || has_project(SP_CITIZENS_DEFENSE_FORCE, faction_id); // Cumulative
      case FAC_SKUNKWORKS:
        return !(Players[faction_id].rule_flags & RFLAG_FREEPROTO); // no prototype costs? skip
      case FAC_HOLOGRAM_THEATRE:
        return has_fac(FAC_RECREATION_COMMONS, base_id, queue_count) // not documented in manual
            && !has_project(SP_VIRTUAL_WORLD, faction_id); // Network Nodes replaces Theater
      case FAC_HYBRID_FOREST:
        return has_fac(FAC_TREE_FARM, base_id, queue_count); // Cumulative
      case FAC_QUANTUM_LAB:
        return has_fac(FAC_FUSION_LAB, base_id, queue_count); // Cumulative
      case FAC_NANOHOSPITAL:
        return has_fac(FAC_RESEARCH_HOSPITAL, base_id, queue_count); // Cumulative
      case FAC_PARADISE_GARDEN: // bug fix: added check
        return !has_fac(FAC_PUNISHMENT_SPHERE, base_id, queue_count); // antithetical
      case FAC_PUNISHMENT_SPHERE:
        return !has_fac(FAC_PARADISE_GARDEN, base_id, queue_count); // antithetical
      case FAC_NANOREPLICATOR:
        return has_fac(FAC_ROBOTIC_ASSEMBLY_PLANT, base_id, queue_count) // Cumulative
            || has_fac(FAC_GENEJACK_FACTORY, base_id, queue_count);
      case FAC_HABITATION_DOME:
        return has_fac(FAC_HAB_COMPLEX, base_id, queue_count); // must have Complex
      case FAC_TEMPLE_OF_PLANET:
        return has_fac(FAC_CENTAURI_PRESERVE, base_id, queue_count); // must have Preserve
      case FAC_QUANTUM_CONVERTER:
        return has_fac(FAC_ROBOTIC_ASSEMBLY_PLANT, base_id, queue_count); // Cumulative
      case FAC_NAVAL_YARD:
        return is_coast(Bases[base_id].x, Bases[base_id].y, false); // needs ocean
      case FAC_AQUAFARM:
      case FAC_SUBSEA_TRUNKLINE:
      case FAC_THERMOCLINE_TRANSDUCER:
        return *ExpansionEnabled && is_coast(Bases[base_id].x, Bases[base_id].y, false);
      case FAC_COVERT_OPS_CENTER:
      case FAC_BROOD_PIT:
      case FAC_FLECHETTE_DEFENSE_SYS:
        return *ExpansionEnabled;
      case FAC_GEOSYNC_SURVEY_POD: // SMACX only & must have Aerospace Complex
        return *ExpansionEnabled && (has_fac(FAC_AEROSPACE_COMPLEX, base_id, queue_count)
            || has_project(SP_CLOUDBASE_ACADEMY, faction_id)
            || has_project(SP_SPACE_ELEVATOR, faction_id));
      case FAC_SKY_HYDRO_LAB:
      case FAC_NESSUS_MINING_STATION:
      case FAC_ORBITAL_POWER_TRANS:
      case FAC_ORBITAL_DEFENSE_POD:  // must have Aerospace Complex
        return has_fac(FAC_AEROSPACE_COMPLEX, base_id, queue_count)
            || has_project(SP_CLOUDBASE_ACADEMY, faction_id)
            || has_project(SP_SPACE_ELEVATOR, faction_id);
      case FAC_SUBSPACE_GENERATOR: // Progenitor factions only
        return is_alien_faction(faction_id);
      default:
        break;
    }
    return true;
}

/*
Purpose: To assist with locating a facility based on a name rather than using hardcoded offsets.
Original Offset: n/a
Return Value: Offset value otherwise -1 if not found
Status: Complete
*/
int __cdecl facility_offset(LPCSTR facil_search) {
    int offset = -1;
    for (uint32_t i = 0; i < MaxFacilityNum; i++) {
        LPSTR fac_name = Facility[i].name;
        if (fac_name && !_stricmp(facil_search, fac_name)) {
            offset = i;
            break;
        }
    }
    return offset;
}
