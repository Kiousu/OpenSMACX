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
#include "veh.h"
#include "regress.h"
#include "alpha.h"
#include "base.h"
#include "council.h"
#include "faction.h"
#include "game.h"
#include "general.h" // range
#include "log.h"
#include "map.h"
#include "net.h"
#include "path.h"
#include "probe.h"
#include "strings.h"
#include "technology.h"
#include "terraforming.h"

VehPrototype *VehPrototypes = (VehPrototype *)0x009AB868; // [512]
Veh *Vehs = (Veh *)0x00952828; // [2049]
RulesChassis *Chassis = (RulesChassis *)0x0094A330; // [9]
RulesWeapon *Weapon = (RulesWeapon *)0x0094AE60; // [26]
RulesArmor *Armor = (RulesArmor *)0x0094F278; // [14]
RulesReactor *Reactor = (RulesReactor *)0x009527F8; // [4]
RulesAbility *Ability = (RulesAbility *)0x009AB538; // [29]
RulesMorale *Morale = (RulesMorale *)0x00952328; // [7]
int *ProtoSortCount = (int *)0x00945B40; // entries of ProtoSortList counted by proto_sort()
int *ProtoSortList = (int *)0x00945B44; // [128] prototype ids sorted for the design workshop
RulesCombatMode *DefenseModes = (RulesCombatMode *)0x00946A00; // [3]
RulesCombatMode *OffenseModes = (RulesCombatMode *)0x00946178; // [3]
RulesOrder *Order = (RulesOrder *)0x0096C878; // [9]
LPSTR *PlansShortName = (LPSTR *)0x00945FE0; // [15]
LPSTR *PlansFullName = (LPSTR *)0x00952360; // [15]
LPSTR *Triad = (LPSTR *)0x0094F1A8; // [3]

int *VehCurrentCount = (int *)0x009A64C8;
int *VehFindDist = (int *)0x009B227C; // distance to the unit returned by veh_find()
BungedDesign *BungList = (BungedDesign *)0x0094B558; // [8][64] ; see is_bunged()
// design workshop suggestion: the workshop sets DesignSuggestState to 1 (and the category to
// match, or -1) and runs consider_designs(); propose_proto() records the design it would reuse
// (state 2) or create (state 3) here instead of making it
int *DesignSuggestAbilities = (int *)0x0093FC34;
int *DesignSuggestCategory = (int *)0x0093FC38;
int *DesignSuggestWeapon = (int *)0x0093FC3C;
int *DesignSuggestChassis = (int *)0x0093FC40;
int *DesignSuggestReactor = (int *)0x0093FC44;
int *DesignSuggestArmor = (int *)0x0093FC50;
int *DesignSuggestState = (int *)0x0093FC54;
int *ProtoIDNewDesign = (int *)0x0068F0F0; // first design proposed since tech_achieved() reset it to -1
int *VehDropLiftVehID = (int *)0x009B2280;
int *VehLiftX = (int *)0x009B2278;
int *VehLiftY = (int *)0x009B2284;
BOOL *VehBitError = (BOOL *)0x009B228C;
uint32_t *VehBasicBattleMorale = (uint32_t *)0x00912420; // [2] ; [0] offense, [1] defense?
int VehMoraleModifierCount; // only used by say_morale(), optimize to local var?
// Battle related globals
uint32_t *VehBattleModCount = (uint32_t *)0x00915614; // [2]
BOOL *VehBattleUnkTgl = (BOOL *)0x0091561C; // [2] ; planet_busting() + boom() + timers
int *VehBattleModifier = (int *)0x009155F0; // [2][4]
LPSTR *VehBattleDisplay = (LPSTR *)0x0090F554; // [2][4][80]
LPSTR *VehBattleDisplayTerrain = (LPSTR *)0x0090F550; // set by defense_value()

/*
Purpose: Generate an output string for the specified unit's morale.
Original Offset: 004B3FD0
Return Value: n/a
Status: Complete
*/
void __cdecl say_morale(LPSTR morale_output, uint32_t veh_id, int faction_id_vs_native) {
    // TODO: Look into inconsistencies related to morale display
    uint32_t morale = morale_veh(veh_id, false, faction_id_vs_native);
    uint32_t faction_id = Vehs[veh_id].faction_id;
    int proto_id = Vehs[veh_id].proto_id;
    std::string output = (proto_id < MaxVehProtoFactionNum &&
        (get_proto_offense_rating(proto_id) < 0 || proto_id == BSC_SPORE_LAUNCHER)) 
        ? StringTable->get((int)Morale[morale].name_lifecycle)
        : StringTable->get((int)Morale[morale].name);
    if (VehPrototypes[proto_id].plan < PLAN_COLONIZATION) {
        uint32_t morale_penalty = 0;
        int home_base_id = Vehs[veh_id].home_base_id;
        if (home_base_id >= 0 && Bases[home_base_id].state & BSTATE_DRONE_RIOTS_ACTIVE && morale > 0
            && !(Players[faction_id].rule_flags & RFLAG_MORALE)) {
            output += " (-)";
            morale_penalty = 1;
        }
        VehMoraleModifierCount = 0;
        int base_id = base_at(Vehs[veh_id].x, Vehs[veh_id].y);
        if (base_id >= 0 && morale < 6) {
            if (has_fac_built(FAC_CHILDREN_CRECHE, base_id)) {
                VehMoraleModifierCount++;
                int morale_active = range(PlayersData[faction_id].soc_effect_active.morale, -4, 4);
                if (morale_active <= -2) {
                    morale_active++;
                }
                if (morale_active < 0) {
                    int morale_cap = morale + VehMoraleModifierCount;
                    do {
                        if (morale_cap >= 6) {
                            break;
                        }
                        VehMoraleModifierCount++;
                        morale_cap++;
                        morale_active++;
                    } while (morale_active < 0);
                }
                if(has_fac_built(FAC_BROOD_PIT, base_id) && proto_id < MaxVehProtoFactionNum
                    && (get_proto_offense_rating(proto_id) < 0 || proto_id == BSC_SPORE_LAUNCHER)) {
                    VehMoraleModifierCount++;
                }
            } else if (morale < 6 && has_fac_built(FAC_BROOD_PIT, base_id)
                && proto_id < MaxVehProtoFactionNum
                && (get_proto_offense_rating(proto_id) < 0 || proto_id == BSC_SPORE_LAUNCHER)) {
                VehMoraleModifierCount++;
                int morale_active = range(PlayersData[faction_id].soc_effect_active.morale, -4, 4);
                if (morale_active <= -2) {
                    morale_active++;
                }
                if (morale_active < 0) {
                    int morale_cap = morale + VehMoraleModifierCount;
                    do {
                        if (morale_cap >= 6) {
                            break;
                        }
                        VehMoraleModifierCount++;
                        morale_cap++;
                        morale_active++;
                    } while (morale_active < 0);
                }
            }
        }
        int morale_pending = PlayersData[faction_id].soc_effect_pending.morale;
        if (morale_pending == 2 || morale_pending == 3) {
            VehMoraleModifierCount++;
        }
        if (!morale && !VehMoraleModifierCount) {
            VehMoraleModifierCount = 1;
        }
        if(VehMoraleModifierCount) {
            output += " (";
            for (int i = 0; i < VehMoraleModifierCount; i++) {
                output += "+";
            }
            output += ")";
        }
        VehMoraleModifierCount -= morale_penalty;
        if (Vehs[veh_id].state & VSTATE_DESIGNATE_DEFENDER) {
            output += "(d)";
        }
    }
    // TODO: assumes at least 1032 char nulled buffer (stringTemp), eventually remove
    strcat_s(morale_output, 1032, output.c_str());
}

/*
Purpose: Get the morale string for the specified unit and store it in the stringTemp buffer.
Original Offset: 004B43C0
Return Value: n/a
Status: Complete
*/
void __cdecl say_morale(uint32_t veh_id, int faction_id_vs_native) {
    say_morale(stringTemp->str, veh_id, faction_id_vs_native);
}

/*
Purpose: Check whether a unit may patrol to a tile. Land units stay within their region; ships
         need the same sea, or a base on that sea, or a port-to-port link between bases; aircraft
         (with limited range) need the round trip to fit their remaining range, or the one-way
         trip when both ends are bases or airbases.
Original Offset: 004CD6A0
Return Value: Is the patrol valid? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch22.log).
        The original tests base_at() != 0 for the air case, so every tile but base 0's counts as
        a base there; that is kept.
*/
BOOL __cdecl valid_patrol(int veh_id, int x, int y) {
    Veh *veh = &Vehs[veh_id];
    if ((x == veh->x && y == veh->y) || y < 0 || y >= (int)*MapLatitudeBounds || x < 0
        || x >= (int)*MapLongitudeBounds) {
        return false;
    }
    RulesChassis *chassis = &Chassis[VehPrototypes[veh->proto_id].chassis_id];
    uint8_t region_target = map_loc(x, y)->region;
    uint8_t region_here = map_loc(veh->x, veh->y)->region;
    if (chassis->triad == TRIAD_LAND) {
        return region_target == region_here;
    }
    if (chassis->triad == TRIAD_SEA) {
        int base_id_here = base_at(veh->x, veh->y);
        int base_id_target = base_at(x, y);
        if (base_id_here >= 0) {
            if (base_id_target >= 0) {
                return port_to_port(base_id_here, base_id_target) != 0;
            }
            return base_on_sea(base_id_here, region_target) != 0;
        }
        if (base_id_target >= 0) {
            return base_on_sea(base_id_target, region_here) != 0;
        }
        return region_target == region_here;
    }
    if (chassis->triad != TRIAD_AIR || !chassis->range) {
        return true;
    }
    BOOL safe_here = base_at(veh->x, veh->y) != 0
        || (map_loc(veh->x, veh->y)->bit & BIT_AIRBASE);
    BOOL safe_target = base_at(x, y) != 0 || (map_loc(x, y)->bit & BIT_AIRBASE);
    int moves_left = (int)speed(veh_id, false) - veh->moves_expended;
    moves_left = moves_left < 0 ? 0 : (moves_left > 999 ? 999 : moves_left);
    // terraforming_turns holds the turns an aircraft has been aloft
    int range = ((int)speed(veh_id, false) * (chassis->range - veh->terraforming_turns - 1)
        + moves_left) / (int)Rules->move_rate_roads;
    int dist = (x_dist(x, veh->x) + abs(y - veh->y)) >> 1;
    if (safe_here && safe_target) {
        return dist <= range;
    }
    return dist <= range / 2;
}

/*
Purpose: Calculate maximum range a faction's units can drop (air drops, Drop Pods).
Original Offset: 00500320
Return Value: Max range
Status: Complete
*/
uint32_t __cdecl drop_range(uint32_t faction_id) {
    if (!has_tech(Rules->tech_orb_insert_sans_spc_elev, faction_id) // default 'Graviton Theory'
        && !has_project(SP_SPACE_ELEVATOR, faction_id)) {
        return Rules->max_airdrop_sans_orb_insert;
    }
    return (*MapLongitudeBounds <= * MapLatitudeBounds) ? *MapLatitudeBounds : *MapLongitudeBounds;
}

/*
Purpose: Check whether the specified prototype is a planet buster.
Original Offset: 005004F0
Return Value: Reactor id if planet buster, otherwise 0
Status: Complete
*/
uint32_t __cdecl planet_buster2(int proto_id) {
    return VehPrototypes[proto_id].plan == PLAN_PLANET_BUSTER 
        ? VehPrototypes[proto_id].reactor_id : 0;
}

/*
Purpose: Check whether the specified unit is a planet buster.
Original Offset: 00500520
Return Value: Reactor id if planet buster, otherwise 0
Status: Complete
*/
uint32_t __cdecl planet_buster(int veh_id) {
    return planet_buster2(Vehs[veh_id].proto_id);
}

/*
Purpose: Calculate how defensive the specified tile is to the defending unit based on the terrain.
Original Offset: 005010C0
Return Value: Defense terrain value
Status: Complete
*/
uint32_t __cdecl defense_value(uint32_t faction_id, uint32_t x, uint32_t y, uint32_t veh_id_def, 
                               int veh_id_atk) {
    if (is_ocean(x, y) || base_who(x, y) >= 0) {
        return 2;
    }
    uint32_t bit = bit_at(x, y);
    if (bit & BIT_FUNGUS && altitude_at(x, y) >= ALT_BIT_OCEAN_SHELF && veh_id_atk >= 0
        && (!Vehs[veh_id_atk].faction_id 
            || has_project(SP_PHOLUS_MUTAGEN, Vehs[veh_id_atk].faction_id) 
            || (get_offense_rating(veh_id_atk) < 0
                && Vehs[veh_id_atk].proto_id < MaxVehProtoFactionNum))) {
                return 2;
    }
    uint32_t is_rocky = rocky_at(x, y) > TERRAIN_BIT_ROLLING;
    *VehBattleDisplayTerrain = label_get(91); // "Rocky"
    uint32_t defense = is_rocky;
    if (bit & BIT_FUNGUS && altitude_at(x, y) >= ALT_BIT_OCEAN_SHELF && !defense
        && get_triad(veh_id_def) != TRIAD_AIR) {
        if (veh_id_atk >= 0 && ((get_offense_rating(veh_id_atk) < 0
                && Vehs[veh_id_atk].proto_id < MaxVehProtoFactionNum)
            || has_project(SP_PHOLUS_MUTAGEN, Vehs[veh_id_atk].faction_id))) {
            defense = is_rocky;
        } else {
            *VehBattleDisplayTerrain = label_get(338); // "Fungus"
            defense = (has_project(SP_PHOLUS_MUTAGEN, faction_id) 
                || (Vehs[veh_id_def].proto_id < MaxVehProtoFactionNum 
                    && get_offense_rating(veh_id_def) < 0)) ? 2 : 1;
        }
    }
    if (bit & BIT_FOREST && !defense && (veh_id_atk < 0 || get_triad(veh_id_atk) == TRIAD_LAND)) {
        *VehBattleDisplayTerrain = label_get(291); // "Forest"
        defense = 1;
    }
    return defense + 2;
}

/*
Purpose: Calculate the lifecycle (morale) of the specified native life unit.
Original Offset: 00501350
Return Value: Lifecycle (morale) value
Status: Complete
*/
uint32_t __cdecl morale_alien(int veh_id, int faction_id_vs_native) {
    int morale;
    // Fungal Tower specific code, shifted to start and added veh_id check to prevent crash
    if (veh_id >= 0 && Vehs[veh_id].proto_id == BSC_FUNGAL_TOWER) {
        morale = 0;
        int16_t x = Vehs[veh_id].x;
        int16_t y = Vehs[veh_id].y;
        // similar to is_coast() > except with fungus check + Ocean Shelf included
        for (int i = RadiusRange[0]; i < RadiusRange[1]; i++) {
            int x_radius = xrange(x + RadiusOffsetX[i]);
            int y_radius = y + RadiusOffsetY[i];
            if (on_map(x_radius, y_radius) && bit_at(x_radius, y_radius) & BIT_FUNGUS
                && altitude_at(x_radius, y_radius) >= ALT_BIT_OCEAN_SHELF) {
                morale++;
            }
        }
        morale -= 2;
    } else { // everything else
        if (*TurnCurrentNum < 45) {
            morale = 0;
        } else if (*TurnCurrentNum < 90) {
            morale = 1;
        } else if (*TurnCurrentNum < 170) {
            morale = 2;
        } else if (*TurnCurrentNum < 250) {
            morale = 3;
        } else if (*TurnCurrentNum < 330) {
            morale = 4;
        } else { // 330+
            morale = 6;
        }
        if (faction_id_vs_native > 0) {
            morale += (PlayersData[faction_id_vs_native].major_atrocities != 0)
                + (TectonicDetonationCount[faction_id_vs_native] != 0);
        }
        if (veh_id >= 0) {
            if (Vehs[veh_id].state & VSTATE_MONOLITH_UPGRADED) {
                morale++;
            }
            if (Vehs[veh_id].proto_id == BSC_LOCUSTS_OF_CHIRON) {
                morale++;
            }
            morale += (Vehs[veh_id].flags >> 3) & 3; // 0x8|0x10 > +1, +2, or +3; TODO: id unk flags
        }
    }
    return range(morale, 0, 6);
}

/*
Purpose: Calculate the psi combat factor for an attacking or defending unit.
Original Offset: 00501500
Return Value: Psi factor
Status: Complete
*/
int __cdecl psi_factor(int combat_ratio, uint32_t faction_id, BOOL is_attack, BOOL is_fungal_twr) {
    int rule_psi = Players[faction_id].rule_psi;
    int factor = rule_psi ? ((rule_psi + 100) * combat_ratio) / 100 : combat_ratio;
    if (is_attack) {
        if (has_project(SP_DREAM_TWISTER, faction_id)) {
            factor += factor / 2; // Psi Attack +50%
        }
    } else {
        if (has_project(SP_NEURAL_AMPLIFIER, faction_id)) {
            factor += factor / 2; // Psi Defense +50%s
        }
        if (is_fungal_twr) {
            factor += factor / 2; // SMACX only: likely +50% Fungal Tower defense bonus
        }
    }
    return factor;
}

/*
Purpose: Get the basic offense value for an attacking unit with an optional defender unit parameter.
Original Offset: 005015B0
Return Value: Basic offense
Status: Complete
*/
int __cdecl get_basic_offense(uint32_t veh_id_atk, int veh_id_def, uint32_t psi_combat_type, 
                              BOOL is_bombard, BOOL unk_tgl) { // art/duel related? Is off of def?
    uint32_t faction_id_atk = Vehs[veh_id_atk].faction_id;
    uint32_t proto_id_atk = Vehs[veh_id_atk].proto_id;
    uint32_t morale = faction_id_atk ? morale_veh(veh_id_atk, true, 0) 
        : morale_alien(veh_id_atk, veh_id_def >= 0 ? Vehs[veh_id_def].faction_id : -1);
    int base_id_atk = base_at(Vehs[veh_id_atk].x, Vehs[veh_id_atk].y);
    if (base_id_atk >= 0) {
        if (has_fac_built(FAC_CHILDREN_CRECHE, base_id_atk)) {
            morale++;
            int morale_active = range(PlayersData[faction_id_atk].soc_effect_active.morale, -4, 4);
            if (morale_active <= -2) {
                morale_active++;
            }
            morale -= morale_active;
        } else if (has_fac_built(FAC_BROOD_PIT, base_id_atk) && proto_id_atk < MaxVehProtoFactionNum
            && (get_proto_offense_rating(proto_id_atk) < 0 || proto_id_atk == BSC_SPORE_LAUNCHER)) {
            morale++;
            int morale_active = range(PlayersData[faction_id_atk].soc_effect_active.morale, -4, 4);
            if (morale_active <= -2) {
                morale_active++;
            }
            morale -= morale_active;
        }
        if (unk_tgl) {
            int morale_pending = PlayersData[faction_id_atk].soc_effect_pending.morale;
            if (morale_pending >= 2 && morale_pending <= 3) {
                morale++;
            }
            if (veh_id_def >= 0) {
                if (Vehs[veh_id_def].faction_id) {
                    if ((proto_id_atk >= MaxVehProtoFactionNum
                        || (get_proto_offense_rating(proto_id_atk) >= 0
                            && proto_id_atk != BSC_SPORE_LAUNCHER))
                        && !has_abil(proto_id_atk, ABL_DISSOCIATIVE_WAVE)
                        && has_abil(Vehs[veh_id_def].proto_id, ABL_SOPORIFIC_GAS)) {
                        morale -= 2;
                    }
                } else {
                    morale++;
                }
            }
        }
    }
    if (unk_tgl) {
        morale = range(morale, 1, 6);
    }
    VehBasicBattleMorale[unk_tgl != 0] = morale; // shifted up from original
    morale += 6;
    uint32_t offense = offense_proto(proto_id_atk, veh_id_def, is_bombard);
    if (psi_combat_type) {
        offense = psi_factor(offense, faction_id_atk, true, false);
    }
    return offense * morale * 4;
}

/*
Purpose: Get the basic defense value for a defending unit with an optional attacker unit parameter.
Original Offset: 00501940
Return Value: Basic defense
Status: Complete
*/
int __cdecl get_basic_defense(uint32_t veh_id_def, int veh_id_atk, uint32_t psi_combat_type,
                              BOOL is_bombard) {
    uint32_t faction_id_def = Vehs[veh_id_def].faction_id;
    uint32_t proto_id_def = Vehs[veh_id_def].proto_id;
    uint32_t morale = faction_id_def ? morale_veh(veh_id_def, true, 0)
        : morale_alien(veh_id_def, veh_id_atk >= 0 ? Vehs[veh_id_atk].faction_id : -1);
    int base_id_def = base_at(Vehs[veh_id_def].x, Vehs[veh_id_def].y);
    if (base_id_def >= 0) {
        if (has_fac_built(FAC_CHILDREN_CRECHE, base_id_def)) {
            morale++;
            int morale_active = range(PlayersData[faction_id_def].soc_effect_active.morale, -4, 0);
            if (morale_active <= -2) {
                morale_active++;
            }
            morale -= morale_active;
        } else if (has_fac_built(FAC_BROOD_PIT, base_id_def) 
            &&  proto_id_def < MaxVehProtoFactionNum
            && (get_proto_offense_rating(proto_id_def) < 0 || proto_id_def == BSC_SPORE_LAUNCHER)) {
            morale++;
            int morale_active = range(PlayersData[faction_id_def].soc_effect_active.morale, -4, 4);
            if (morale_active <= -2) {
                morale_active++;
            }
            morale -= morale_active;
        }
        if (has_fac_built(FAC_HEADQUARTERS, base_id_def)) { // bug fix: per manual
            morale++; // "Units in a headquarters base automatically gain +1 Morale when defending."
        }
        int morale_pending = PlayersData[faction_id_def].soc_effect_pending.morale;
        if (morale_pending >= 2 && morale_pending <= 3) {
            morale++;
        }
        if (veh_id_atk >= 0 && !Vehs[veh_id_atk].faction_id) {
            morale++;
        }
    }
    if (veh_id_atk >= 0 && Vehs[veh_id_atk].faction_id && (proto_id_def >= MaxVehProtoFactionNum 
        || (get_proto_offense_rating(proto_id_def) >= 0 && proto_id_def != BSC_SPORE_LAUNCHER)) 
        && !has_abil(proto_id_def, ABL_DISSOCIATIVE_WAVE)
        && has_abil(Vehs[veh_id_atk].proto_id, ABL_SOPORIFIC_GAS)) {
        morale -= 2;
    }
    morale = range(morale, 1, 6);
    VehBasicBattleMorale[1] = morale;
    morale += 6;
    uint32_t plan_def = VehPrototypes[proto_id_def].plan;
    if (plan_def == PLAN_ALIEN_ARTIFACT) {
        return 1;
    }
    if (plan_def == PLAN_INFO_WARFARE && get_proto_defense_rating(proto_id_def) == 1
        // bug fix: added veh_id_atk bounds check to prevent potential arbitrary memory read
        && (veh_id_atk < 0 || VehPrototypes[Vehs[veh_id_atk].proto_id].plan != PLAN_INFO_WARFARE)) {
        return 1;
    }
    uint32_t defense = armor_proto(proto_id_def, veh_id_atk, is_bombard);
    if (psi_combat_type) {
        defense = psi_factor(defense, faction_id_def, false, proto_id_def == BSC_FUNGAL_TOWER);
    }
    return defense * morale;
}

/*
Purpose: Initialize or reset the battle related global variables.
Original Offset: 00501D30
Return Value: n/a
Status: Complete
*/
void __cdecl battle_init() {
    VehBattleModCount[0] = 0;
    VehBattleModCount[1] = 0;
    VehBattleUnkTgl[0] = false;
    VehBattleUnkTgl[1] = false;
}

/*
Purpose: Add combat battle modifier for type (offense, defense).
Original Offset: 00501D50
Return Value: n/a
Status: Complete
*/
void __cdecl add_bat(uint32_t type, int modifier, LPCSTR display_str) {
    // TODO: Revise global offsets once all references are decompiled.
    uint32_t offset = VehBattleModCount[type];
    if (modifier && offset < 4) {
        VehBattleModifier[type * 4 + offset] = modifier;
        strcpy_s((LPSTR)&VehBattleDisplay[type * 80 + offset * 20], 80, display_str);
        VehBattleModCount[type]++;
    }
}

/*
Purpose: Calculate the battle outcome between two units.
Original Offset: 00501DA0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/sweep-2026-10-16/ (13 saves); attacker -1 is a documented deviation)
*/
void __cdecl battle_compute(int veh_id_atk, int veh_id_def, int *offense_out, int *defense_out,
                            int combat_type) {
    int offense = 8;
    int defense = 8;
    // Deliberate deviation: the original reads Vehs[veh_id_atk] with no bounds check, and
    // best_defender() does call it with veh_id_atk = -1. That reads the Reactor table just below
    // the Vehs array, where the prototype id is the high half of a heap string pointer: a
    // meaningless index that crashes the exe's own code once has_abil() walks the "faction" it
    // implies (seen on 8 of 13 saves in the regression sweep). A negative attacker is treated as
    // no attacker here instead; everything the original guards with veh_id_atk >= 0 is unchanged.
    int proto_id_atk = veh_id_atk >= 0 ? Vehs[veh_id_atk].proto_id : -1;
    int proto_id_def = veh_id_def >= 0 ? Vehs[veh_id_def].proto_id : -1;
    uint32_t faction_id_atk = veh_id_atk >= 0 ? Vehs[veh_id_atk].faction_id : 0;
    uint32_t faction_id_def = veh_id_def >= 0 ? Vehs[veh_id_def].faction_id : 0;
    battle_init();
    BOOL is_artillery = combat_type & 1;
    BOOL is_bombard = (is_artillery || (veh_id_atk >= 0 && is_proto_missile(proto_id_atk)))
        ? true : false;
    uint32_t psi_combat_type = 0;
    // The PSI check only covers the PSI flags + display modifiers. The original closes this block
    // before the offense calculation so artillery/missile combat still computes offense/defense.
    if (!is_bombard || proto_id_atk == BSC_SPORE_LAUNCHER || proto_id_def == BSC_SPORE_LAUNCHER) {
        if (veh_id_atk >= 0 && get_proto_offense_rating(proto_id_atk) < 0) {
            psi_combat_type = 1; // PSI attacker
        }
        if (veh_id_def >= 0 && get_proto_defense_rating(proto_id_def) < 0) {
            psi_combat_type |= 2; // PSI defender
        }
        // NOTE: PSI bonuses below are only for display purposes, actual calculation is done in
        //       combat morale functions.
        if (psi_combat_type) {
            // Calculate PSI attack bonus
            int rule_psi_atk = Players[faction_id_atk].rule_psi;
            if (rule_psi_atk) {
                add_bat(0, rule_psi_atk, label_get(342)); // "Gaian Psi Bonus"
            }
            if (has_project(SP_DREAM_TWISTER, faction_id_atk)) {
                add_bat(0, 50, label_get(343)); // "Dream Twist"
            }
            // PSI Defense?
            if (psi_combat_type) { // shouldn't this be & 2?
                int rule_psi_def = Players[faction_id_def].rule_psi;
                if (rule_psi_def) {
                    add_bat(1, rule_psi_def, label_get(342)); // "Gaian Psi Bonus"
                }
                if (has_project(SP_NEURAL_AMPLIFIER, faction_id_def)) {
                    add_bat(1, 50, label_get(344)); // "Neural Amp"
                }
                if (veh_id_def >= 0 && proto_id_def == BSC_FUNGAL_TOWER) {
                    add_bat(1, 50, VehPrototypes[BSC_FUNGAL_TOWER].veh_name); // "Fungal Tower"
                }
            }
        }
    }
    if (veh_id_atk >= 0) {
        offense = get_basic_offense(veh_id_atk, veh_id_def, psi_combat_type, is_bombard, false);
        if (VehPrototypes[proto_id_atk].plan != PLAN_INFO_WARFARE) {
            if (veh_id_def >= 0 && !combat_type) { // checking if isn't wpn vs wpn ; air toggle
                int x_def = Vehs[veh_id_def].x;
                int y_def = Vehs[veh_id_def].y;
                if (bit_at(x_def, y_def) & BIT_FUNGUS
                    && altitude_at(x_def, y_def) >= ALT_BIT_OCEAN_SHELF
                    && ((proto_id_atk < MaxVehProtoFactionNum
                        && get_proto_offense_rating(proto_id_atk) < 0)
                        || has_project(SP_PHOLUS_MUTAGEN, faction_id_atk))
                    && get_proto_offense_rating(proto_id_def) >= 0) {
                    offense += offense / 2;
                    add_bat(0, 50, label_get(338)); // "Fungus"
                }
                if (psi_combat_type & 2
                    && (VehPrototypes[proto_id_atk].weapon_id == WPN_RESONANCE_LASER
                        || VehPrototypes[proto_id_atk].weapon_id == WPN_RESONANCE_BOLT)) {
                    offense = (offense * 125) / 100;
                    add_bat(0, 25, label_get(1110)); // "Resonance Attack"
                }
                if (!has_abil(proto_id_def, ABL_DISSOCIATIVE_WAVE) && psi_combat_type & 2
                    && has_abil(proto_id_atk, ABL_EMPATHIC)
                    && Rules->combat_pct_emp_song_atk_vs_psi) {
                    offense = (Rules->combat_pct_emp_song_atk_vs_psi + 100) * offense / 100;
                    add_bat(0, Rules->combat_pct_emp_song_atk_vs_psi,
                        Ability[abil_index(ABL_EMPATHIC)].name);
                }
            }
            if (Vehs[veh_id_atk].state & VSTATE_MADE_AIRDROP
                && has_abil(proto_id_atk, ABL_DROP_POD) && Rules->combat_pen_pct_atk_airdrop) {
                offense = (100 - Rules->combat_pen_pct_atk_airdrop) * offense / 100;
                /*
                uint32_t dropRange;
                if (has_tech(Rules->tech_orb_insert_sans_spc_elev, faction_id_atk)
                    || has_project(SP_SPACE_ELEVATOR, faction_id_atk)) {
                    dropRange = (*MapHorizontalBounds <= *MapLatitudeBounds)
                        ? *MapLatitudeBounds : *MapHorizontalBounds;
                } else {
                    dropRange = Rules->max_airdrop_sans_orb_insert;
                }
                */
                add_bat(0, -Rules->combat_pen_pct_atk_airdrop,
                    drop_range(faction_id_atk) <= Rules->max_airdrop_sans_orb_insert
                    ? label_get(437) // "Air Drop"
                    : label_get(438)); // "Orbital Insertion"
            }
            if (Players[faction_id_atk].rule_flags & RFLAG_FANATIC
                && Rules->combat_pct_fanatic_atk_bonus && !combat_type && !psi_combat_type) {
                offense = (Rules->combat_pct_fanatic_atk_bonus + 100) * offense / 100;
                add_bat(0, Rules->combat_pct_fanatic_atk_bonus, label_get(528));
            }
            int bonus_count = Players[faction_id_atk].faction_bonus_count;
            for (int i = 0; i < bonus_count; i++) {
                if (Players[faction_id_atk].faction_bonus_id[i] == RULE_OFFENSE) {
                    int rule_off_bonus = Players[faction_id_atk].faction_bonus_val1[i];
                    offense = rule_off_bonus * offense / 100;
                    add_bat(0, rule_off_bonus - 100, label_get(1108)); // "Alien Offense"
                }
            }
            if (psi_combat_type && faction_id_atk) {
                int planet_atk = PlayersData[faction_id_atk].soc_effect_active.planet;
                if (planet_atk && Rules->combat_pct_psi_atk_bonus_planet) {
                    int planet_modifier = planet_atk * Rules->combat_pct_psi_atk_bonus_planet;
                    add_bat(0, planet_modifier, label_get(625)); // "Planet"
                    offense = (planet_modifier + 100) * offense / 100;
                }
            }
        }
    }
    if (!(combat_type & 2) && veh_id_def >= 0) {
        defense = get_basic_defense(veh_id_def, veh_id_atk, psi_combat_type, is_bombard);
        int bonus_count = Players[faction_id_def].faction_bonus_count;
        for (int i = 0; i < bonus_count; i++) {
            if (Players[faction_id_def].faction_bonus_id[i] == RULE_DEFENSE) {
                int rule_def_bonus = Players[faction_id_def].faction_bonus_val1[i];
                defense = rule_def_bonus * defense / 100;
                add_bat(1, rule_def_bonus - 100, label_get(1109)); // "Alien Defense"
            }
        }
        if (veh_id_atk >= 0 && VehPrototypes[proto_id_atk].plan == PLAN_INFO_WARFARE) {
            defense *= 4;
        } else {
            int x_def = Vehs[veh_id_def].x;
            int y_def = Vehs[veh_id_def].y;
            int base_id_def = base_at(x_def, y_def);
            if (psi_combat_type) {
                if (base_id_def >= 0 && Rules->combat_pct_base_def) {
                    add_bat(1, Rules->combat_pct_base_def, label_get(332)); // "Base"
                    defense = (Rules->combat_pct_base_def + 100) * defense / 100;
                }
                defense *= 4;
            } else {
                uint32_t triad_def = get_proto_triad(proto_id_def);
                int terrain_def = (triad_def == TRIAD_AIR)
                    ? 2 : defense_value(faction_id_def, x_def, y_def, veh_id_def, veh_id_atk);
                if (veh_id_atk >= 0 && get_triad(veh_id_atk) == TRIAD_LAND) {
                    if (combat_type && Rules->combat_pct_art_bonus_lvl_alt
                        && triad_def == TRIAD_LAND) {
                        int alt_atk = alt_at(Vehs[veh_id_atk].x, Vehs[veh_id_atk].y);
                        int alt_def = alt_at(x_def, y_def);
                        if (alt_atk > alt_def) {
                            int alt_mod_atk = (alt_atk - alt_def)
                                * Rules->combat_pct_art_bonus_lvl_alt;
                            offense = (alt_mod_atk + 100) * offense / 100;
                            add_bat(0, alt_mod_atk, label_get(576)); // "Altitude"
                        }
                    }
                    if (Rules->combat_pct_mobile_open_ground && !combat_type && base_id_def < 0
                        && terrain_def == 2
                        && rocky_at(x_def, y_def) < TERRAIN_BIT_ROCKY
                        && (int)speed_proto(proto_id_atk) > (int)Rules->move_rate_roads) {
                        // original: applies when the defender is as fast or slower (<=)
                        if ((int)speed_proto(proto_id_def) <= (int)speed_proto(proto_id_atk)) {
                            offense = (Rules->combat_pct_mobile_open_ground + 100) * offense / 100;
                            // "Mobile in open"
                            add_bat(0, Rules->combat_pct_mobile_open_ground, label_get(611));
                        }
                    }
                    // original: no rockiness check here
                    if (Rules->combat_pct_def_vs_mobile_rough && !combat_type
                        && (terrain_def > 2 || base_id_def >= 0)
                        && (int)speed_proto(proto_id_atk) > (int)Rules->move_rate_roads) {
                        defense = (Rules->combat_pct_def_vs_mobile_rough + 100) * defense / 100;
                        // "Rough vs. Mobile" : "Mobile vs. Base"
                        add_bat(1, Rules->combat_pct_def_vs_mobile_rough, base_id_def < 0
                            ? label_get(548) : label_get(612));
                    }
                    if (Rules->combat_pct_atk_road && !combat_type) {
                        // TODO: add check road/tube Combat % -> attacking along road
                        //       (original applies it unconditionally)
                        offense = (Rules->combat_pct_atk_road + 100) * offense / 100;
                        add_bat(0, Rules->combat_pct_atk_road, label_get(606)); // "Road Attack"
                    }
                    if (Rules->combat_pen_pct_atk_lwr_elev && !combat_type
                        && altitude_at(x_def, y_def)
                        > altitude_at(Vehs[veh_id_atk].x, Vehs[veh_id_atk].y)
                        && !has_abil(proto_id_atk, ABL_ANTIGRAV_STRUTS)) {
                        // "Uphill"
                        add_bat(1, Rules->combat_pen_pct_atk_lwr_elev, label_get(441));
                        defense = (Rules->combat_pen_pct_atk_lwr_elev + 100) * defense / 100;
                    }
                    if (Rules->combat_pct_atk_higher_elev && !combat_type
                        && altitude_at(Vehs[veh_id_atk].x, Vehs[veh_id_atk].y)
                        > altitude_at(x_def, y_def)
                        && !has_abil(proto_id_def, ABL_ANTIGRAV_STRUTS)) {
                        // "Downhill"
                        add_bat(0, Rules->combat_pct_atk_higher_elev, label_get(330));
                        offense = (Rules->combat_pct_atk_higher_elev + 100) * offense / 100;
                    }
                }
                defense *= terrain_def;
                if (terrain_def > 2) {
                    std::string terrain_modifier = label_get(331); // "Terrain"
                    terrain_modifier += " (";
                    terrain_modifier += StringTable->get(int(*VehBattleDisplayTerrain));
                    terrain_modifier += ")";
                    add_bat(1, 10 * (5 * terrain_def - 10), terrain_modifier.c_str());
                }
                LPSTR display_def = NULL; // only one is displayed
                int def_multi = 2;
                if (bit_at(x_def, y_def) & BIT_BUNKER && (veh_id_atk < 0
                    || get_triad(veh_id_atk) != TRIAD_AIR)) {
                    def_multi = 3;
                    display_def = label_get(358); // "Bunker"
                }
                if (base_id_def >= 0) {
                    display_def = label_get(332); // "Base"
                    def_multi = 2;
                    // original: left as the (negative) attacker id if there is no attacker
                    int fac_modifier = veh_id_atk;
                    if (veh_id_atk >= 0) {
                        fac_modifier = 2;
                        switch (get_triad(veh_id_atk)) {
                          case TRIAD_LAND:
                            if (has_fac_built(FAC_PERIMETER_DEFENSE, base_id_def)
                                || has_project(SP_CITIZENS_DEFENSE_FORCE, faction_id_def)) {
                                display_def = label_get(354); // "Perimeter"
                                fac_modifier = 4;
                            }
                            break;
                          case TRIAD_SEA:
                            // original checks the SP against the base owner, not the defender
                            if (has_fac_built(FAC_NAVAL_YARD, base_id_def)
                                || has_project(SP_MARITIME_CONTROL_CENTER,
                                    Bases[base_id_def].faction_id_current)) {
                                display_def = Facility[FAC_NAVAL_YARD].name;
                                fac_modifier = 4;
                            }
                            break;
                          case TRIAD_AIR:
                            // original checks the SP against the base owner, not the defender
                            if (has_fac_built(FAC_AEROSPACE_COMPLEX, base_id_def)
                                || has_project(SP_CLOUDBASE_ACADEMY,
                                    Bases[base_id_def].faction_id_current)) {
                                display_def = Facility[FAC_AEROSPACE_COMPLEX].name;
                                fac_modifier = 4;
                            }
                            break;
                          default:
                            break;
                        }
                    }
                    if (has_fac_built(FAC_TACHYON_FIELD, base_id_def)) {
                        fac_modifier += 2;
                        display_def = label_get(357); // "Tachyon"
                    }
                    if (fac_modifier >= 2) {
                        def_multi = fac_modifier;
                    }
                    if (fac_modifier <= 2 && (veh_id_atk < 0
                        || !has_abil(proto_id_atk, ABL_BLINK_DISPLACER))
                        && Rules->combat_pct_base_def) {
                        add_bat(1, Rules->combat_pct_base_def, label_get(332)); // "Base"
                        defense = (Rules->combat_pct_base_def + 100) * defense / 100;
                    }
                }
                if (veh_id_atk >= 0 && has_abil(proto_id_atk, ABL_BLINK_DISPLACER)) {
                    if (def_multi > 2) {
                        def_multi = 2;
                    }
                    display_def = label_get(428); // "Base vs. Blink"
                }
                if (Rules->combat_pct_infantry_vs_base && !combat_type
                    && base_id_def >= 0 && faction_id_atk
                    && (proto_id_atk >= MaxVehProtoFactionNum
                        || (get_proto_offense_rating(proto_id_atk) >= 0
                            && proto_id_atk != BSC_SPORE_LAUNCHER))
                    && get_proto_triad(proto_id_atk) == TRIAD_LAND
                    && Chassis[VehPrototypes[proto_id_atk].chassis_id].speed == 1) {
                    offense = (Rules->combat_pct_infantry_vs_base + 100) * offense / 100;
                    // "Infantry vs. Base"
                    add_bat(0, Rules->combat_pct_infantry_vs_base, label_get(547));
                }
                uint32_t bit_def;
                if (is_artillery && def_multi < 3 && base_id_def < 0
                    && rocky_at(x_def, y_def) < TERRAIN_BIT_ROCKY
                    && (bit_def = bit_at(x_def, y_def), !(bit_def & BIT_FOREST))
                    && (!(bit_def & BIT_FUNGUS)
                        || altitude_at(x_def, y_def) < ALT_BIT_OCEAN_SHELF)) {
                    def_multi = 3;
                    display_def = label_get(525); // "Open Ground"
                }
                defense *= def_multi;
                if (def_multi > 2) {
                    add_bat(1, 10 * (5 * def_multi - 10), StringTable->get(int(display_def)));
                }
            }
            if (faction_id_def && veh_id_atk >= 0 && is_proto_missile(proto_id_atk)) {
                for (int i = 0; i < 25; i++) {
                    int x_radius = xrange(x_def + RadiusOffsetX[i]);
                    int y_radius = y_def + RadiusOffsetY[i];
                    if (on_map(x_radius, y_radius)) {
                        int base_id_radius = base_at(x_radius, y_radius);
                        if (base_id_radius >= 0
                            && Bases[base_id_radius].faction_id_current == faction_id_def
                            && has_fac_built(FAC_FLECHETTE_DEFENSE_SYS, base_id_radius)) {
                            defense = 3 * defense / 2;
                            add_bat(1, 50, label_get(1113)); // "Flechette"
                        }
                    }
                }
            }
            if (!is_ocean(x_def, y_def)) {
                uint32_t sensor_def = 0;
                if (faction_id_def) {
                    for (int i = 0; i < 25; i++) {
                        int x_radius = xrange(x_def + RadiusOffsetX[i]);
                        int y_radius = y_def + RadiusOffsetY[i];
                        uint32_t sensor_status;
                        if (on_map(x_radius, y_radius)
                            && (sensor_status = is_sensor(x_radius, y_radius), sensor_status)) {
                            BOOL has_sensor = false;
                            if (!is_ocean(x_radius, y_radius)) {
                                int faction_id_terr = whose_territory(faction_id_def, x_radius,
                                    y_radius, NULL, false);
                                if (faction_id_terr < 0
                                    || (uint32_t)faction_id_terr == faction_id_def) {
                                    has_sensor = true;
                                }
                            } else {
                                int base_id_find = base_find(x_radius, y_radius);
                                // assumes will find a base?
                                if (Bases[base_id_find].faction_id_current == faction_id_def) {
                                    has_sensor = true;
                                }
                            }
                            if (has_sensor) {
                                sensor_def |= 1;
                                if (sensor_status > 1) {
                                    sensor_def |= 2;
                                }
                            }
                        }
                    }
                }
                if (sensor_def & 1) {
                    defense = (Rules->combat_pct_def_range_sensor + 100) * defense / 100;
                    add_bat(1, Rules->combat_pct_def_range_sensor, label_get(613)); // "Sensor"
                }
                if (sensor_def & 2) {
                    defense = (Rules->combat_pct_def_range_sensor + 100) * defense / 100;
                    add_bat(1, Rules->combat_pct_def_range_sensor, label_get(1123)); // "GSP"
                }
            }
            if (!has_abil(proto_id_atk, ABL_DISSOCIATIVE_WAVE)
                && Rules->combat_pct_trance_def_vs_psi && veh_id_atk >= 0 && psi_combat_type & 1
                && has_abil(proto_id_def, ABL_TRANCE)) {
                defense = (Rules->combat_pct_trance_def_vs_psi + 100) * defense / 100;
                add_bat(1, Rules->combat_pct_trance_def_vs_psi, label_get(329)); // "Trance"
            }
            uint32_t armor_id_def = VehPrototypes[proto_id_def].armor_id;
            if (psi_combat_type & 1 && veh_id_atk >= 0 && (armor_id_def == ARM_RESONANCE_3_ARMOR
                || armor_id_def == ARM_RESONANCE_8_ARMOR)) {
                defense = 125 * defense / 100;
                add_bat(1, 25, label_get(1111)); // "Resonance Def."
            }
            // original jumps to the end when there is no attacker
            if (veh_id_atk >= 0) {
                if (get_proto_triad(proto_id_atk) == TRIAD_AIR
                    && has_abil(proto_id_atk, ABL_AIR_SUPERIORITY) && !psi_combat_type) {
                    if (get_proto_triad(proto_id_def) == TRIAD_AIR) {
                        int air_to_air = Rules->combat_pct_air_supr_vs_air;
                        if (air_to_air && !has_abil(proto_id_def, ABL_AIR_SUPERIORITY)) {
                            offense = (air_to_air + 100) * offense / 100;
                            add_bat(0, air_to_air, label_get(449)); // "Air-to-Air"
                        }
                    } else {
                        int ground_strike_pen = Rules->combat_pen_pct_air_supr_vs_grnd;
                        if (ground_strike_pen) {
                            offense = (100 - ground_strike_pen) * offense / 100;
                            add_bat(0, -ground_strike_pen, label_get(448)); // "Ground Strike"
                        }
                    }
                }
                if (get_proto_triad(proto_id_atk) == TRIAD_AIR
                    && get_proto_triad(proto_id_def) == TRIAD_AIR
                    && has_abil(proto_id_def, ABL_AIR_SUPERIORITY)
                    && !has_abil(proto_id_atk, ABL_AIR_SUPERIORITY) && !psi_combat_type
                    && !is_proto_missile(proto_id_atk) && !is_proto_missile(proto_id_def)
                    && Rules->combat_pct_air_supr_vs_air) {
                    defense = (Rules->combat_pct_air_supr_vs_air + 100) * defense / 100;
                    add_bat(1, Rules->combat_pct_air_supr_vs_air, label_get(449)); // "Air-to-Air"
                }
                if (!get_proto_offense_rating(proto_id_def)
                    && get_proto_defense_rating(proto_id_def) == 1 && (faction_id_atk
                        || base_id_def < 0) && Rules->combat_pen_pct_non_cbt_def_vs_cbt) {
                    defense = (100 - Rules->combat_pen_pct_non_cbt_def_vs_cbt) * defense / 100;
                    // "Non Combat"
                    add_bat(1, -Rules->combat_pen_pct_non_cbt_def_vs_cbt, label_get(439));
                }
                if (get_proto_triad(proto_id_atk) != TRIAD_SEA
                    && get_proto_triad(proto_id_def) == TRIAD_SEA && base_id_def >= 0
                    && Rules->combat_pct_bonus_vs_ship_port) {
                    offense = (Rules->combat_pct_bonus_vs_ship_port + 100) * offense / 100;
                    add_bat(0, Rules->combat_pct_bonus_vs_ship_port, label_get(335)); // "In Port"
                }
                if ((armor_id_def == ARM_PULSE_3_ARMOR || armor_id_def == ARM_PULSE_8_ARMOR)
                    && get_proto_triad(proto_id_atk) == TRIAD_LAND
                    && Chassis[VehPrototypes[proto_id_atk].chassis_id].speed > 1) {
                    defense = 125 * defense / 100;
                    add_bat(1, 25, label_get(1112)); // "Pulse Defense"
                }
                if (!has_abil(proto_id_atk, ABL_DISSOCIATIVE_WAVE)
                    && has_abil(proto_id_def, ABL_COMM_JAMMER)
                    && get_proto_triad(proto_id_atk) == TRIAD_LAND
                    && Chassis[VehPrototypes[proto_id_atk].chassis_id].speed > 1
                    && Rules->combat_pct_com_jam_def_vs_mobl) {
                    defense = (Rules->combat_pct_com_jam_def_vs_mobl + 100) * defense / 100;
                    add_bat(1, Rules->combat_pct_com_jam_def_vs_mobl, label_get(336)); // "Jammer"
                }
                if (!has_abil(proto_id_atk, ABL_DISSOCIATIVE_WAVE)
                    && has_abil(proto_id_def, ABL_AAA)
                    && get_proto_triad(proto_id_atk) == TRIAD_AIR
                    && Rules->combat_pct_aaa_bonus_vs_air) {
                    defense = (Rules->combat_pct_aaa_bonus_vs_air + 100) * defense / 100;
                    add_bat(1, Rules->combat_pct_aaa_bonus_vs_air, label_get(337)); // "Tracking"
                }
            }
        }
    } else if (veh_id_def >= 0) {
        defense = get_basic_offense(veh_id_def, veh_id_atk, psi_combat_type, is_bombard, true);
        if (!(combat_type & 0x18)) {
            uint32_t triad_atk = get_proto_triad(proto_id_atk);
            uint32_t triad_def = get_proto_triad(proto_id_def);
            if (triad_def == TRIAD_SEA && triad_atk == TRIAD_LAND) {
                int gun_vs_ship_art = Rules->combat_pct_land_gun_vs_ship_art;
                if (gun_vs_ship_art) {
                    offense = (gun_vs_ship_art + 100) * offense / 100;
                    add_bat(0, gun_vs_ship_art, label_get(435)); // "Land Based Guns"
                }
            } else if (triad_atk == TRIAD_SEA && triad_def == TRIAD_LAND) {
                int gun_vs_ship_art = Rules->combat_pct_land_gun_vs_ship_art;
                if (gun_vs_ship_art) {
                    defense = (gun_vs_ship_art + 100) * defense / 100;
                    add_bat(1, gun_vs_ship_art, label_get(435)); // "Land Based Guns"
                }
            } else if (Rules->combat_pct_art_bonus_lvl_alt) {
                int alt_atk = alt_at(Vehs[veh_id_atk].x, Vehs[veh_id_atk].y);
                int alt_def = alt_at(Vehs[veh_id_def].x, Vehs[veh_id_def].y);
                if (alt_def < alt_atk) {
                    int alt_mod_atk = (alt_atk - alt_def) * Rules->combat_pct_art_bonus_lvl_alt;
                    offense = (alt_mod_atk + 100) * offense / 100;
                    add_bat(0, alt_mod_atk, label_get(576)); // "Altitude"
                } else if (alt_def > alt_atk) {
                    int alt_mod_def = (alt_def - alt_atk) * Rules->combat_pct_art_bonus_lvl_alt;
                    defense = (alt_mod_def + 100) * defense / 100;
                    add_bat(1, alt_mod_def, label_get(576)); // "Altitude"
                }
            }
        }
    }
    if (offense_out) {
        *offense_out = offense;
    }
    if (defense_out) {
        *defense_out = defense;
    }
}

/*
Purpose: Determine the best defender in a stack.
Original Offset: 005044D0
Return Value: Unit id of the best defender
Status: Complete (verified vs original: docs/regression-runs/sweep-2026-10-16/ (13 saves); attacker -1 is a documented deviation)
*/
uint32_t __cdecl best_defender(uint32_t veh_id_def, int veh_id_atk, BOOL check_artillery) {
    int offense = veh_id_atk >= 0 
        ? get_basic_offense(veh_id_atk, veh_id_def, false, false, false) : 8;
    int x_def = Vehs[veh_id_def].x;
    int y_def = Vehs[veh_id_def].y;
    BOOL is_ocean_def = is_ocean(x_def, y_def);
    int base_id_def = base_at(x_def, y_def);
    int defender_search = -999;
    uint32_t best_def_veh_id = veh_id_def;
    for (int i = veh_top(veh_id_def); i >= 0; i = Vehs[i].next_veh_id_stack) {
        uint32_t proto_id_def = Vehs[i].proto_id;
        // Deliberate deviation (see battle_compute): the original reads Vehs[veh_id_atk] here
        // with no bounds check and crashes on a bad heap layout when veh_id_atk is -1; a
        // negative attacker is treated as a non-probe, non-artillery attacker instead.
        if ((get_proto_triad(proto_id_def) != TRIAD_LAND || !is_ocean_def || base_id_def >= 0)
            && (veh_id_atk < 0 || VehPrototypes[Vehs[veh_id_atk].proto_id].plan != PLAN_INFO_WARFARE
                || VehPrototypes[proto_id_def].plan == PLAN_INFO_WARFARE)) {
            uint32_t combat_type = (can_arty(proto_id_def, true) && veh_id_atk >= 0
                && can_arty(Vehs[veh_id_atk].proto_id, true)) ? 2 : 0;
            if (veh_id_atk >= 0 && get_triad(veh_id_atk) == TRIAD_AIR
                && has_abil(proto_id_def, ABL_AIR_SUPERIORITY)
                && !is_proto_missile(proto_id_def) && !is_missile(veh_id_atk)
                && get_offense_rating(veh_id_atk) > 0 && get_defense_rating(veh_id_atk) > 0
                && get_proto_offense_rating(proto_id_def) > 0) {
                combat_type = 0xA; // air combat (overwrites the artillery flag)
                if (get_proto_triad(proto_id_def) == TRIAD_AIR) {
                    combat_type = 0x1A;
                }
            }
            int offense_output;
            int defense_output;
            battle_compute(veh_id_atk, i, &offense_output, &defense_output, combat_type);
            if (!offense_output) {
                break;
            }
            int proto_pwr_def = proto_power(i); // signed: the original's divisions are signed
            int def_modifier = (((((range(proto_pwr_def - Vehs[i].dmg_incurred, 0, 9999)
                * defense_output) / proto_pwr_def) * offense) / offense_output) / 8)
                - get_proto_offense_rating(proto_id_def);
            uint32_t plan_def = VehPrototypes[proto_id_def].plan;
            if (plan_def < PLAN_COLONIZATION || plan_def == PLAN_TERRAFORMING) {
                def_modifier *= 16;
            }
            if (veh_id_atk >= 0 && get_triad(veh_id_atk) == TRIAD_AIR 
                && has_abil(proto_id_def, ABL_AIR_SUPERIORITY) 
                && get_proto_triad(proto_id_def) == TRIAD_AIR 
                && !is_proto_missile(proto_id_def) && !is_missile(veh_id_atk)) {
                def_modifier += 0x80000;
            } else if (veh_id_atk >= 0 && get_triad(veh_id_atk) == TRIAD_AIR
                && has_abil(Vehs[veh_id_atk].proto_id, ABL_AIR_SUPERIORITY)
                && get_proto_triad(proto_id_def) == TRIAD_AIR && base_id_def < 0
                && !(bit_at(x_def, y_def) & BIT_AIRBASE)
                && !stack_check(veh_id_def, 6, ABL_CARRIER, -1, -1)
                && !is_proto_missile(proto_id_def) && !is_missile(veh_id_atk)) {
                def_modifier += 0x80000;
            } else if (check_artillery) {
                if (can_arty(proto_id_def, true)) {
                    def_modifier += 0x80000;
                }
            } else if ((!stack_check(veh_id_def, 3, TRIAD_AIR, -1, -1) || base_id_def >= 0 
                || bit_at(x_def, y_def) & BIT_AIRBASE 
                || stack_check(veh_id_def, 6, ABL_CARRIER, -1, -1)) 
                && Vehs[i].state & VSTATE_DESIGNATE_DEFENDER) {
                def_modifier += 0x80000;
            }
            int defense = i + (def_modifier << 11);
            if (defense > defender_search) {
                defender_search = defense;
                best_def_veh_id = i;
            }
        }
    }
    return best_def_veh_id;
}

/*
Purpose: Start a unit's attack in a direction: the adjacent tile for the eight directions, or with
         is_arty an entry of the radius tables (artillery reaches two tiles out). The x coordinate
         wraps on a round map, y is passed on as it is; battle_fight_2() does the rest.
Original Offset: 00506A60
Return Value: What battle_fight_2() returns
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
int __cdecl battle_fight(int veh_id, int offset, BOOL is_arty, int unk1, int unk2) {
    int x = Vehs[veh_id].x + (is_arty ? RadiusOffsetX[offset] : RadiusBaseX[offset]);
    if (!(*MapIsFlat & 1)) {
        if (x < 0) {
            x += *MapLongitudeBounds;
        } else if (x >= *MapLongitudeBounds) {
            x -= *MapLongitudeBounds;
        }
    }
    int y = Vehs[veh_id].y + (is_arty ? RadiusOffsetY[offset] : RadiusBaseY[offset]);
    return battle_fight_2(veh_id, offset, x, y, is_arty, unk1, unk2);
}

/*
Purpose: Remove a unit that lost a battle. An Alien Artifact is captured instead when the winner
         is a non-missile unit on land (or whose stack can carry it, or when the artifact is an
         objective): it changes sides and moves to the winner's tile. Killing an objective unit
         is credited to faction_id_obj, killing the human player's first mind worm triggers the
         assistant interlude for the winner's base, and killing a native unit with a non-native
         winner adds to the morale bonus.
Original Offset: 00505D80
Return Value: Was the unit killed (false when captured)? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch6.log)
*/
BOOL __cdecl battle_kill(int veh_id, int *killed, int *captured, int *morale_bonus, int veh_id_atk,
                         int faction_id_obj) {
    uint32_t faction_id = Vehs[veh_id].faction_id;
    int proto_id = Vehs[veh_id].proto_id;
    PlayersData[faction_id].proto_id_lost[proto_id]++;
    if (veh_id_atk >= 0 && veh_id_atk != veh_id && Vehs[veh_id_atk].faction_id) {
        int x_atk = Vehs[veh_id_atk].x;
        int y_atk = Vehs[veh_id_atk].y;
        if (y_atk >= 0 && y_atk < (int)*MapLatitudeBounds && x_atk >= 0
            && x_atk < (int)*MapLongitudeBounds && VehPrototypes[proto_id].plan == PLAN_ALIEN_ARTIFACT
            && !Chassis[VehPrototypes[Vehs[veh_id_atk].proto_id].chassis_id].missile
            && ((map_loc(x_atk, y_atk)->climate & 0xE0) >= ALT_BIT_SHORE_LINE
                || stack_check(veh_id_atk, 8, -1, -1, -1) > 0
                || (Vehs[veh_id].flags & VFLAG_IS_OBJECTIVE))) {
            veh_lift(veh_id);
            proto_id = Vehs[veh_id].proto_id;
            uint8_t faction_id_atk = Vehs[veh_id_atk].faction_id;
            Vehs[veh_id].dmg_incurred = 0;
            Vehs[veh_id].faction_id = faction_id_atk;
            Vehs[veh_id].order = ORDER_NONE;
            PlayersData[faction_id].proto_id_active[proto_id]--;
            Vehs[veh_id].state &= 0xFCFFBDFF; // clears 0x3000000, VSTATE_EXPLORE and VSTATE_UNK_200
            PlayersData[faction_id_atk].proto_id_active[proto_id]++;
            veh_drop(veh_id, Vehs[veh_id_atk].x, Vehs[veh_id_atk].y);
            veh_skip(veh_id);
            if (captured) {
                (*captured)++;
            }
            return false;
        }
    }
    if ((Vehs[veh_id].flags & VFLAG_IS_OBJECTIVE) && faction_id_obj >= 0) {
        PlayersData[faction_id_obj].objective_kills++;
    }
    if ((Vehs[veh_id].state & VSTATE_ASSISTANT_WORM) && veh_id_atk >= 0
        && !(*GamePreferences & PREF_AV_INTERLUDES_DISABLED)) {
        int base_id = Vehs[veh_id_atk].home_base_id;
        if (base_id < 0) {
            base_id = base_find(Vehs[veh_id_atk].x, Vehs[veh_id_atk].y, Vehs[veh_id_atk].faction_id);
        }
        if (base_id >= 0) {
            *PluralityDefault = false;
            uint32_t faction_id_atk = Vehs[veh_id_atk].faction_id;
            Bases[base_id].state |= BSTATE_ASSISTANT_KILLER_HOME;
            PlayersData[faction_id_atk].flags |= PFLAG_UNK_1000;
            *GenderDefault = Players[faction_id_atk].is_leader_female;
            parse_says(6, Players[faction_id_atk].name_leader, -1, -1);
            *GenderDefault = Players[faction_id_atk].noun_gender;
            *PluralityDefault = Players[faction_id_atk].is_noun_plural;
            parse_says(7, Players[faction_id_atk].noun_faction, -1, -1);
            interlude(6, Bases[base_id].name_string, 1, 0);
        }
    }
    // like the original, this reads Vehs[-1] for an attacker of -1
    if (faction_id == 0 && Vehs[veh_id_atk].faction_id != 0 && morale_bonus) {
        *morale_bonus += (morale_veh(veh_id, true, 0) + 1) * 10;
    }
    if (killed) {
        (*killed)++;
    }
    veh_kill(veh_id);
    return true;
}

/*
Purpose: Run battle_kill() over every unit of a stack. Unit ids above a killed unit shift down,
         so the attacker id and the walk are adjusted after each kill.
Original Offset: 00506130
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch6.log)
*/
void __cdecl battle_kill_stack(int veh_id, int *killed, int *captured, int *morale_bonus,
                               int veh_id_atk, int faction_id_obj) {
    if (veh_id < 0) {
        return;
    }
    int i = veh_id;
    while (Vehs[i].prev_veh_id_stack >= 0) {
        i = Vehs[i].prev_veh_id_stack;
    }
    while (i >= 0) {
        int next = Vehs[i].next_veh_id_stack;
        if (battle_kill(i, killed, captured, morale_bonus, veh_id_atk, faction_id_obj)) {
            if (i < veh_id_atk) {
                veh_id_atk--;
            }
            if (i < next) {
                next--;
            }
        }
        i = next;
    }
}

/*
Purpose: Run battle_kill() over the units of a stack that cannot survive: artifacts and units whose
         damage has used up their reactor's hit points, and every native unit.
Original Offset: 005061B0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch6.log)
*/
void __cdecl battle_kill_chaff(int veh_id, int *killed, int *captured, int *morale_bonus,
                               int veh_id_atk, int faction_id_obj) {
    if (veh_id < 0) {
        return;
    }
    int i = veh_id;
    while (Vehs[i].prev_veh_id_stack >= 0) {
        i = Vehs[i].prev_veh_id_stack;
    }
    while (i >= 0) {
        int next = Vehs[i].next_veh_id_stack;
        int proto_id = Vehs[i].proto_id;
        int hit_points;
        if (VehPrototypes[proto_id].plan == PLAN_ALIEN_ARTIFACT) {
            hit_points = 1;
        } else {
            uint32_t reactor = VehPrototypes[proto_id].reactor_id;
            if (!reactor) {
                reactor = 1;
            } else if (reactor > 100) {
                reactor = 100;
            }
            hit_points = reactor * 10;
        }
        hit_points -= Vehs[i].dmg_incurred;
        if ((hit_points < 1 || Vehs[i].faction_id == 0)
            && battle_kill(i, killed, captured, morale_bonus, veh_id_atk, faction_id_obj)) {
            if (i < veh_id_atk) {
                veh_id_atk--;
            }
            if (i < next) {
                next--;
            }
        }
        i = next;
    }
}

/*
Purpose: Promote a unit after a victory. Missiles, units already elite and unarmored non-combat
         designs (plan colonization or above with armor 1 or less) are skipped; from disciplined
         upward only half the promotions succeed. The player at the controls is told when the
         morale level changes.
Original Offset: 005062B0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch3.log)
*/
void __cdecl promote(int veh_id) {
    int proto_id = Vehs[veh_id].proto_id;
    if (Chassis[VehPrototypes[proto_id].chassis_id].missile) {
        return;
    }
    if (VehPrototypes[proto_id].plan >= PLAN_COLONIZATION
        && Armor[VehPrototypes[proto_id].armor_id].defense_rating <= 1) {
        return;
    }
    if (morale_veh(veh_id, true, 0) >= MORALE_ELITE || Vehs[veh_id].morale >= MORALE_ELITE) {
        return;
    }
    // the exe's CRT generator, like the rest of the game's rolls
    if (morale_veh(veh_id, true, 0) > MORALE_DISCIPLINED && _rand() % 2 != 0) {
        return;
    }
    uint32_t morale_before = morale_veh(veh_id, true, 0);
    Vehs[veh_id].morale++;
    uint32_t faction_id = Vehs[veh_id].faction_id;
    if (faction_id == (uint32_t)*FactionIDCurrent && is_human(faction_id)
        && morale_before != morale_veh(veh_id, true, 0)) {
        parse_says(1, VehPrototypes[proto_id].veh_name, -1, -1);
        if (proto_id < MaxVehProtoFactionNum
            && Weapon[VehPrototypes[proto_id].weapon_id].offense_rating < 0) {
            parse_say(0, (int)Morale[morale_veh(veh_id, true, 0)].name_lifecycle, -1, -1);
            NetMsg_pop(NetMsgVar, 0, "MORALE2", -5000, 0, "batwon_sm.pcx");
            return;
        }
        parse_say(0, (int)Morale[morale_veh(veh_id, true, 0)].name, -1, -1);
        NetMsg_pop(NetMsgVar, 0, "MORALE", -5000, 0, "batwon_sm.pcx");
    }
}

/*
Purpose: Determine whether any enemy naval transports are carrying land units within range to 
         attack the specified base. If so, set the units to move towards the base.
Original Offset: 00506490
Return Value: n/a
Status: Complete
*/
void __cdecl invasions(uint32_t base_id) {
    uint32_t faction_id_base = Bases[base_id].faction_id_current;
    int16_t base_x = Bases[base_id].x;
    int16_t base_y = Bases[base_id].y;
    for (int i = 0; i < *VehCurrentCount; i++) {
        uint32_t veh_faction_id = Vehs[i].faction_id;
        if (veh_faction_id && !is_human(veh_faction_id) && veh_faction_id != faction_id_base
            && !has_treaty(veh_faction_id, faction_id_base, DTREATY_TREATY)) {
            int veh_x = Vehs[i].x;
            int veh_y = Vehs[i].y;
            if (is_ocean(veh_x, veh_y) && veh_cargo(i) && stack_check(i, 3, TRIAD_LAND, -1, -1)) {
                int proximity = vector_dist(base_x, base_y, veh_x, veh_y);
                if (proximity <= (int)speed(i, false)) {
                    Vehs[i].order = ORDER_MOVE_TO;
                    Vehs[i].waypoint_x[0] = base_x;
                    Vehs[i].waypoint_y[0] = base_y;
                }
            }
        }
    }
}

/*
Purpose: Scramble the specified faction's best interceptor to the tile. Candidates are its air
         units with air superiority sitting in a base, on an airbase or in a carrier's stack, near
         the tile ((dx + dy) rounded down to even at most 4); the best has the highest weapon
         strength (at least 1) times health, ties going to the later unit. When either faction is
         the player at the controls the scramble is announced and the flight shown step by step.
Original Offset: 00506650
Return Value: Did an interceptor scramble? true/false
Status: Complete
*/
BOOL __cdecl interceptor(uint32_t faction_id, uint32_t faction_id_atk, int x, int y) {
    int veh_id_best = -1;
    int value_best = 0;
    for (int i = 0; i < *VehCurrentCount; i++) {
        if (Vehs[i].faction_id != faction_id) {
            continue;
        }
        int proto_id = Vehs[i].proto_id;
        if (Chassis[VehPrototypes[proto_id].chassis_id].triad != TRIAD_AIR
            || !has_abil(proto_id, ABL_AIR_SUPERIORITY)) {
            continue;
        }
        Map *tile = map_loc(Vehs[i].x, Vehs[i].y);
        if (!(tile->bit & BIT_BASE_IN_TILE && (tile->val2 & 0xF) < 8) && !(tile->bit & BIT_AIRBASE)
            && !stack_check(i, 6, ABL_CARRIER, -1, -1)) {
            continue;
        }
        if (((x_dist(Vehs[i].x, x) + abs(Vehs[i].y - y)) & ~1) > 4) {
            continue;
        }
        int offense = Weapon[VehPrototypes[proto_id].weapon_id].offense_rating;
        if (offense < 1) {
            offense = 1;
        }
        int value = offense * (int)veh_health(i);
        if (value >= value_best) {
            value_best = value;
            veh_id_best = i;
        }
    }
    if (veh_id_best < 0) {
        return false;
    }
    if (faction_id == (uint32_t)*FactionIDCurrent || faction_id_atk == (uint32_t)*FactionIDCurrent) {
        parse_says(0, Players[faction_id].adj_name_faction, -1, -1);
        int x_veh = Vehs[veh_id_best].x;
        int y_veh = Vehs[veh_id_best].y;
        // with no base on the map base_find() returns -1 and the original reads Bases[-1]
        parse_says(1, Bases[base_find(x_veh, y_veh)].name_string, -1, -1);
        Console_focus(ConsoleVar, 0, x_veh, y_veh, faction_id);
        NetMsg_pop(NetMsgVar, 0, "AIRSCRAMBLE", 5000, 0, 0);
        while ((x_dist(x_veh, x) + abs(y_veh - y)) & ~1) {
            int dir_best = -1;
            int dist_best = 999;
            int x_best = 0; // only read once a direction was found
            int y_best = 0;
            for (int dir = 0; dir < 8; dir++) {
                int x_step = xrange(x_veh + RadiusBaseX[dir]);
                int y_step = y_veh + RadiusBaseY[dir];
                if (y_step >= 0 && y_step < *MapLatitudeBounds && x_step >= 0
                    && x_step < *MapLongitudeBounds) {
                    int dist = (x_dist(x_step, x) + abs(y_step - y)) >> 1;
                    if (dist < dist_best) {
                        dist_best = dist;
                        dir_best = dir;
                        x_best = x_step;
                        y_best = y_step;
                    }
                }
            }
            if (dir_best < 0) {
                break;
            }
            veh_scoot(veh_id_best, x_veh, y_veh, dir_best, 0);
            x_veh = x_best;
            y_veh = y_best;
        }
    }
    veh_drop(veh_lift(veh_id_best), x, y);
    Vehs[veh_id_best].order = ORDER_NONE;
    Vehs[veh_id_best].state &= ~(VSTATE_UNK_200 | VSTATE_EXPLORE | VSTATE_UNK_1000000
        | VSTATE_UNK_2000000);
    return true;
}

/*
Purpose: Pick the unit to show on top of a stack: the map window's selected unit if it belongs to
         the player at the controls and is neither exploring nor on an automated order, otherwise
         the unit with the highest triad, preferring the current player's own units. The unit
         being blown up by boom() is never picked.
Original Offset: 005593E0
Return Value: Veh id of the unit to draw on top (the input when it is negative)
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch3.log)
*/
int __cdecl pick_top_veh(int veh_id) {
    if (veh_id < 0) {
        return veh_id;
    }
    int veh_id_top = veh_id;
    int score_best = -99;
    int i = veh_id;
    while (Vehs[i].prev_veh_id_stack >= 0) {
        i = Vehs[i].prev_veh_id_stack;
    }
    for (; i >= 0; i = Vehs[i].next_veh_id_stack) {
        if (*VehDrawBoomID != -1 && i == *VehDrawBoomID) {
            continue;
        }
        BOOL is_current = Vehs[i].faction_id == *FactionIDCurrent;
        if (i == *MapWinVehIDSelected && is_current
            && !(Vehs[i].state & (VSTATE_EXPLORE | VSTATE_UNK_200)) && *MapWinUnitNotViewMode
            && *MapWinUnkFlag) {
            return i;
        }
        int score = Chassis[VehPrototypes[Vehs[i].proto_id].chassis_id].triad + (is_current ? 4 : 0);
        if (score > score_best) {
            score_best = score;
            veh_id_top = i;
        }
    }
    return veh_id_top;
}

/*
Purpose: Direct the unit to start moving automatically towards the specified tile.
Original Offset: 00560AD0
Return Value: n/a
Status: Complete
*/
void __cdecl go_to(uint32_t veh_id, uint8_t type, int x, int y) {
    Vehs[veh_id].order = ORDER_MOVE_TO;
    Vehs[veh_id].move_to_ai_type = type;
    Vehs[veh_id].waypoint_x[0] = (int16_t)x;
    Vehs[veh_id].waypoint_y[0] = (int16_t)y;
    if (VehPrototypes[Vehs[veh_id].proto_id].plan == PLAN_COLONIZATION) {
        Vehs[veh_id].terraforming_turns = 0;
    }
}

/*
Purpose: Give a sea unit a move-to order towards a destination. When the destination shares the
         unit's sea region the unit heads straight there; otherwise, out of the 21 tiles around the
         destination, the reachable ocean tile (same region, or a sea region the unit's base
         opens onto) with the most free land tiles of the destination's region next to it is
         picked. Colony pods also get their terraforming counter cleared.
Original Offset: 00564890
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch10.log)
*/
void __cdecl set_course(int veh_id, uint8_t move_to_ai_type, int x, int y) {
    Veh *veh = &Vehs[veh_id];
    int lat = *MapLatitudeBounds, lon = *MapLongitudeBounds;
    uint8_t region_veh = map_loc(veh->x, veh->y)->region;
    int base_id = base_at(veh->x, veh->y);
    Map *dest = map_loc(x, y);
    uint8_t region_dest = dest->region;
    if (region_veh == region_dest && (map_loc(veh->x, veh->y)->climate & 0xE0) < ALT_BIT_SHORE_LINE) {
        veh->order = ORDER_MOVE_TO;
        veh->move_to_ai_type = move_to_ai_type;
        veh->waypoint_x[0] = (int16_t)x;
        veh->waypoint_y[0] = (int16_t)y;
        if (VehPrototypes[veh->proto_id].plan == PLAN_COLONIZATION) {
            veh->terraforming_turns = 0;
        }
        return;
    }
    int best = ((dest->climate & 0xE0) >= ALT_BIT_SHORE_LINE) - 1;
    for (int i = 0; i <= 20; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (y_radius < 0 || y_radius >= lat || x_radius < 0 || x_radius >= lon) {
            continue;
        }
        Map *tile = map_loc(x_radius, y_radius);
        if ((tile->climate & 0xE0) >= ALT_BIT_SHORE_LINE) {
            continue;
        }
        if (base_id < 0 ? tile->region != region_veh : !base_on_sea(base_id, tile->region)) {
            continue;
        }
        int land = 0;
        for (int j = 0; j < 8; j++) {
            int x_land = xrange(x_radius + RadiusBaseX[j]);
            int y_land = y_radius + RadiusBaseY[j];
            if (y_land < 0 || y_land >= lat || x_land < 0 || x_land >= lon) {
                continue;
            }
            Map *shore = map_loc(x_land, y_land);
            if ((shore->climate & 0xE0) >= ALT_BIT_SHORE_LINE && shore->region == region_dest
                && (!(shore->bit & BIT_BASE_IN_TILE) || (shore->val2 & 0xF) > 7)) {
                land++;
            }
        }
        if (land > best) {
            best = land;
            veh->order = ORDER_MOVE_TO;
            veh->move_to_ai_type = move_to_ai_type;
            veh->waypoint_x[0] = (int16_t)x_radius;
            veh->waypoint_y[0] = (int16_t)y_radius;
            if (VehPrototypes[veh->proto_id].plan == PLAN_COLONIZATION) {
                veh->terraforming_turns = 0;
            }
        }
    }
}

/*
Purpose: Check whether a supply crawler could convoy resources from a tile: it must hold no unit
         that would block the convoy, and no base within two tiles may be working the tile.
Original Offset: 00564D90
Return Value: Can the tile be convoyed from? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch3.log)
*/
BOOL __cdecl can_convoy(int x, int y) {
    if (stack_check(veh_at(x, y), 9, 3, -1, -1)) {
        return false;
    }
    for (int i = 0; i < *BaseCurrentCount; i++) {
        int x_dist = abs(x - Bases[i].x);
        if (!*MapIsFlat && x_dist > (int)*MapLongitude) {
            x_dist = *MapLongitudeBounds - x_dist;
        }
        if (vector_dist(x_dist, y - Bases[i].y) < 3) {
            int offset = radius_move(Bases[i].x, Bases[i].y, x, y, 21);
            if (offset >= 0 && (Bases[i].resource_sqr_radius & (1 << offset))) {
                return false;
            }
        }
    }
    return true;
}

/*
Purpose: Get the unit on the top of the stack.
Original Offset: 00579920
Return Value: Unit id if found, otherwise -1
Status: Complete
*/
int __cdecl veh_top(int veh_id) {
    if (veh_id < 0) {
        return -1;
    }
    int top_veh_id = veh_id;
    for (int i = Vehs[top_veh_id].prev_veh_id_stack; i >= 0; i = Vehs[i].prev_veh_id_stack) {
        top_veh_id = i;
    }
    return top_veh_id;
}

/*
Purpose: Get the current moves left for the specified unit.
Original Offset: 00579960
Return Value: Remaining moves
Status: Complete
*/
uint32_t __cdecl veh_moves(uint32_t veh_id) {
    return range(speed(veh_id, false) - Vehs[veh_id].moves_expended, 0, 999);
}

/*
Purpose: Get the specified unit's reactor power value from its prototype.
Original Offset: 005799A0
Return Value: Power
Status: Complete
*/
uint32_t __cdecl proto_power(uint32_t veh_id) {
    int proto_id = Vehs[veh_id].proto_id;
    if (VehPrototypes[proto_id].plan == PLAN_ALIEN_ARTIFACT) {
        return 1;
    }
    return range(VehPrototypes[proto_id].reactor_id, 1, 100) * 10;
}

/*
Purpose: Determine whether the specified unit is eligible for a monolith morale upgrade.
Original Offset: 00579F80
Return Value: Is unit eligible for a monolith morale upgrade? true/false
Status: Complete
*/
BOOL __cdecl want_monolith(uint32_t veh_id) {
    if (!(Vehs[veh_id].state & VSTATE_MONOLITH_UPGRADED)
        && morale_veh(veh_id, true, 0) < MORALE_ELITE && Vehs[veh_id].morale < MORALE_ELITE
        && get_offense_rating(veh_id)) {
        return true;
    }
    return false;
}

/*
Purpose: Add the monolith tile as a site goal for each faction that sees a monolith there.
Original Offset: n/a (a loop repeated three times in monolith())
Return Value: n/a
Status: Complete
*/
static void monolith_sites(int x, int y) {
    for (int i = 1; i < MaxPlayerNum; i++) {
        if (map_loc(x, y)->bit_visible[i - 1] & BIT_MONOLITH) {
            add_site(i, 0, 2, x, y);
        }
    }
}

/*
Purpose: A unit stops at a monolith. Battle ogres are ignored, and so is a unit of the faction at the
         controls that is only passing through on other orders (outside network games). A
         non-combat unit is repaired (MONOLITHHEAL) or, once, noted (SEEMONOLITH). A combat unit
         clears the faction site goals there; a human player may decline (the monolith then stays
         a site goal for those who see it) or choose to always inspect monoliths. The unit is
         repaired and, if not yet upgraded by a monolith and below elite, gains a morale level;
         from turn 100 on, a monolith used before then has a 1 in 32 chance of being used up
         (MONOLITH0, otherwise MONOLITH1). A unit with nothing to gain gets MONOLITHNO.
Original Offset: 0057A050
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch30.log)
*/
void __cdecl monolith(uint32_t veh_id) {
    Veh *veh = &Vehs[veh_id];
    uint32_t faction_id = veh->faction_id;
    int x = veh->x;
    int y = veh->y;
    if (veh->proto_id == BSC_BATTLE_OGRE_MK1 || veh->proto_id == BSC_BATTLE_OGRE_MK2
        || veh->proto_id == BSC_BATTLE_OGRE_MK3) {
        return;
    }
    if (faction_id == (uint32_t)*FactionIDCurrent && !*IsMultiplayerNet) {
        if (veh->order == ORDER_MOVE_TO && veh->waypoint_x[0] == x && veh->waypoint_y[0] == y) {
            veh->order = ORDER_NONE;
        }
        if (veh->order) {
            return;
        }
    }
    if (!get_offense_rating(veh_id)) {
        if (veh->dmg_incurred) {
            veh->dmg_incurred = 0;
            veh_skip(veh_id);
            draw_tile(x, y, 2);
            if (faction_id == (uint32_t)*FactionIDCurrent) {
                NetMsg_pop(NetMsgVar, NULL, "MONOLITHHEAL", 5000, 0, "monolith_sm.pcx");
                *GameState |= STATE_UNK_4;
            }
        } else if (!(veh->state & VSTATE_MONOLITH_UPGRADED)) {
            veh->state |= VSTATE_MONOLITH_UPGRADED;
            if (faction_id == (uint32_t)*FactionIDCurrent
                && !(*GameMorePreferences & MPREF_AUTO_ALWAYS_INSPECT_MONOLITH) && !*IsMultiplayerNet) {
                NetMsg_pop(NetMsgVar, NULL, "SEEMONOLITH", -5000, 0, "monolith_sm.pcx");
                *GameState |= STATE_UNK_4;
            }
        }
        return;
    }
    for (int i = 1; i < MaxPlayerNum; i++) {
        del_site(i, 0, x, y, 0);
    }
    if (FactionsStatus[0] & (1 << faction_id)) {
        if (!*IsMultiplayerNet && !(*GameMorePreferences & MPREF_AUTO_ALWAYS_INSPECT_MONOLITH)) {
            FX_play(FXVar, NULL, 6);
            int choice = popp(ScriptTxtID, "MONOLITH", 0, "monolith_sm.pcx", NULL);
            FX_stop(FXVar, NULL, 6);
            if (choice == 2) {
                *GameMorePreferences |= MPREF_AUTO_ALWAYS_INSPECT_MONOLITH;
                prefs_save(false);
            } else if (!choice) {
                monolith_sites(x, y);
                return;
            }
        }
    } else if (!want_monolith(veh_id)) { // inlined in the original
        if (veh->dmg_incurred) {
            veh->dmg_incurred = 0;
            veh_skip(veh_id);
            draw_tile(x, y, 2);
        }
        monolith_sites(x, y);
        return;
    }
    if (veh->dmg_incurred) {
        veh->dmg_incurred = 0;
        veh_skip(veh_id);
        draw_tile(x, y, 2);
        if ((veh->state & VSTATE_MONOLITH_UPGRADED) || morale_veh(veh_id, true, 0) >= MORALE_ELITE) {
            if (faction_id == (uint32_t)*FactionIDCurrent) {
                NetMsg_pop(NetMsgVar, NULL, "MONOLITHHEAL",
                    (*GameMorePreferences & MPREF_AUTO_ALWAYS_INSPECT_MONOLITH) ? 5000 : -5000, 0,
                    "monolith_sm.pcx");
                *GameState |= STATE_UNK_4;
            }
            return;
        }
    }
    if ((veh->state & VSTATE_MONOLITH_UPGRADED) || morale_veh(veh_id, true, 0) >= MORALE_ELITE) {
        if (faction_id == (uint32_t)*FactionIDCurrent) {
            NetMsg_pop(NetMsgVar, NULL, "MONOLITHNO",
                (*GameMorePreferences & MPREF_AUTO_ALWAYS_INSPECT_MONOLITH) ? 5000 : -5000, 0,
                "monolith_sm.pcx");
        }
        return;
    }
    int lasts = _rand() % 32;
    if (!(map_loc(x, y)->bit & BIT_UNK_2000000) || *TurnCurrentNum < 100) {
        lasts = 1;
        bit_set(x, y, BIT_UNK_2000000, true);
    }
    veh->state |= VSTATE_MONOLITH_UPGRADED;
    veh->morale = (uint8_t)((veh->morale + 1 > MORALE_ELITE) ? MORALE_ELITE : veh->morale + 1);
    veh_skip(veh_id);
    stringTemp->str[0] = 0;
    say_morale(veh_id, 0);
    parse_says(0, stringTemp->str, -1, -1);
    CharLowerA(ParseStrBuffer[0].str);
    if (!lasts) {
        bit_set(x, y, BIT_MONOLITH, false);
        synch_bit(x, y, faction_id);
    } else {
        monolith_sites(x, y);
    }
    draw_tile(x, y, 2);
    if (faction_id == (uint32_t)*FactionIDCurrent) {
        FX_play(FXVar, NULL, 0x22);
        char number[80];
        stringTemp->str[0] = 0;
        strcat(stringTemp->str, "MONOLITH");
        _itoa(lasts != 0, number, 10);
        strcat(stringTemp->str, number);
        NetMsg_pop(NetMsgVar, NULL, stringTemp->str,
            (*GameMorePreferences & MPREF_AUTO_ALWAYS_INSPECT_MONOLITH) ? 5000 : -5000, 0,
            "monolith_sm.pcx");
        *GameState |= STATE_UNK_4;
    }
}

/*
Purpose: Calculate the armor strategy for the specified armor id.
Original Offset: 0057D270
Return Value: Armor strategy
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-ptrfix.log)
*/
int __cdecl arm_strat(uint32_t armor_id, uint32_t faction_id) {
    if (!*ExpansionEnabled && armor_id > ARM_PSI_DEFENSE) {
        return 1;
    }
    int defense_rating = Armor[armor_id].defense_rating;
    if (defense_rating < 0) {
        return psi_factor(PlayersData[faction_id].enemy_best_weapon_value
            * (int)Rules->psi_combat_ratio_def[TRIAD_LAND]
            / (int)Rules->psi_combat_ratio_atk[TRIAD_LAND], faction_id, false, false);
    }
    return defense_rating;
}

/*
Purpose: Calculate the weapon strategy for the specified weapon id.
Original Offset: 0057D2E0
Return Value: Weapon strategy
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-ptrfix.log)
*/
int __cdecl weap_strat(uint32_t weapon_id, uint32_t faction_id) {
    if (!*ExpansionEnabled && (weapon_id == WPN_RESONANCE_LASER || weapon_id == WPN_RESONANCE_BOLT
        || weapon_id == WPN_STRING_DISRUPTOR))
        return 1;
    int offense_rating = Weapon[weapon_id].offense_rating;
    if (offense_rating < 0) {
        return psi_factor(PlayersData[faction_id].enemy_best_armor_value
            * (int)Rules->psi_combat_ratio_atk[TRIAD_LAND]
            / (int)Rules->psi_combat_ratio_def[TRIAD_LAND], faction_id, true, false);
    }
    return offense_rating;
}

/*
Purpose: Calculate the weapon value for the specified prototype.
Original Offset: 0057D360
Return Value: Weapon value
Status: Complete
*/
int __cdecl weap_val(uint32_t proto_id, uint32_t faction_id) {
    return weap_strat(VehPrototypes[proto_id].weapon_id, faction_id);
}

/*
Purpose: Calculate the armor value for the specified armor id.
Original Offset: 0057D3F0
Return Value: Armor value
Status: Complete
*/
int __cdecl arm_val(uint32_t armor_id, int faction_id) {
    return ((faction_id >= 0) ? arm_strat(armor_id, faction_id) : Armor[armor_id].defense_rating)
        * 2;
}

/*
Purpose: Calculate the armor value for the specified prototype.
Original Offset: 0057D480
Return Value: Armor value
Status: Complete
*/
int __cdecl armor_val(uint32_t proto_id, int faction_id) {
    return arm_val(VehPrototypes[proto_id].armor_id, faction_id);
}

/*
Purpose: Calculate the carrying/transport capacity for the specified chassis, abilities and reactor.
Original Offset: 0057D510
Return Value: Transport capacity
Status: Complete
*/
uint32_t __cdecl transport_val(uint32_t chassis_id, uint32_t ability, uint32_t reactor_id) {
    uint32_t transport = Chassis[chassis_id].cargo;
    if (Chassis[chassis_id].triad == TRIAD_SEA) {
        transport *= reactor_id;
    }
    if (ability & ABL_SLOW) {
        transport /= 2; // -50%, rounded down
    }
    if (ability & ABL_HEAVY_TRANSPORT) {
        transport = (3 * transport + 1) / 2; // +50%, rounded up
    }
    return transport;
}

/*
Purpose: Generate offensive display string for the specified prototype. Replaced existing non-safe
         strcat with string. Original function took a 2nd parameter with char buffer and didn't
         return anything.
Original Offset: 0057D560
Return Value: Offensive display string
Status: Complete
*/
std::string __cdecl say_offense(uint32_t proto_id) {
    std::string output = std::to_string(offense_proto(proto_id, -1, 0));
    if (has_abil(proto_id, ABL_DROP_POD)) {
        output += "^";
    }
    if (has_abil(proto_id, ABL_AMPHIBIOUS)) {
        output += "~";
    }
    if (has_abil(proto_id, ABL_NERVE_GAS)) {
        output += "x";
    }
    if (has_abil(proto_id, ABL_EMPATHIC)) {
        output += "e";
    }
    if (has_abil(proto_id, ABL_BLINK_DISPLACER)) {
        output += "!";
    }
    if (can_arty(proto_id, true)) {
        output.insert(output.begin(), '(');
        output += ")";
    }
    if (has_abil(proto_id, ABL_AIR_SUPERIORITY)) {
        output.insert(output.begin(), '<');
        output += ">";
    }
    uint32_t weapon_id = VehPrototypes[proto_id].weapon_id;
    if (weapon_id == WPN_RESONANCE_LASER || weapon_id == WPN_RESONANCE_BOLT) {
        output += "r";
    }
    return output;
}

/*
Purpose: Generate defense display string for the specified prototype. Replaced existing non-safe 
         strcat with string. Original function took a 2nd parameter with char buffer and didn't 
         return anything.
Original Offset: 0057D6D0
Return Value: Defense display string
Status: Complete
*/
std::string __cdecl say_defense(uint32_t proto_id) {
    std::string output = std::to_string(armor_proto(proto_id, -1, 0));
    if (has_abil(proto_id, ABL_COMM_JAMMER)) {
        output += "+";
    }
    if (has_abil(proto_id, ABL_TRANCE)) {
        output += "t";
    }
    if (has_abil(proto_id, ABL_AAA)) {
        output.insert(output.begin(), '<');
        output += ">";
    }
    uint32_t armor_id = VehPrototypes[proto_id].armor_id;
    if (armor_id == ARM_PULSE_3_ARMOR || armor_id == ARM_PULSE_8_ARMOR) {
        output += "p";
    } else if (armor_id == ARM_RESONANCE_3_ARMOR || armor_id == ARM_RESONANCE_8_ARMOR) {
        output += "r";
    }
    return output;
}

/*
Purpose: Generate stats string for specified prototype. Replaced existing non-safe strcat with 
         string. Reworked to integrate with existing C code.
Original Offset: 0057D7D0
Return Value: n/a
Status: Complete
*/
void __cdecl say_stats_3(LPSTR stat, uint32_t proto_id) {
    std::string output;
    int off_rating = get_proto_offense_rating(proto_id);
    if (off_rating >= 0) {
        output = (off_rating < 99) ? say_offense(proto_id) : "*";
    } else {
        output = "?"; // PSI
    }
    output += "-";
    output += get_proto_defense_rating(proto_id) >= 0 ? say_defense(proto_id) : "?";
    output += "-";
    output += std::to_string(speed_proto(proto_id) / Rules->move_rate_roads);
    uint8_t reactor = VehPrototypes[proto_id].reactor_id;
    if (reactor > RECT_FISSION) {
        output += "*";
        output += std::to_string(reactor);
    }
    // assumes at least 256 char buffer, eventually remove
    // all calls but one use stringTemp (1032 buffer) > ProdPicker::calculate uses local 256 buffer
    // TODO: Eventually remove LPSTR stat param and return std::string instead
    strcat_s(stat, 256, output.c_str());
}

/*
Purpose: Generate prototype stats using stringTemp buffer.
Original Offset: 0050B9A0
Return Value: n/a
Status: Complete
*/
void __cdecl say_stats_3(uint32_t proto_id) {
    say_stats_3(stringTemp->str, proto_id);
}

/*
Purpose: Generate stats string for specified prototype. List whether prototype is psi, sea or air.
         Replaced existing non-safe strcat with string. Reworked to integrate with existing C code.
Original Offset: 0057D8E0
Return Value: n/a
Status: Complete
*/
void __cdecl say_stats_2(LPSTR stat, uint32_t proto_id) {
    std::string output;
    int off_rating = get_proto_offense_rating(proto_id);
    if (off_rating >= 0) {
        output = (off_rating < 99) ? say_offense(proto_id) : "*";
    } else {
        output = label_get(196); // 'Psi'
    }
    output += "-";
    output += get_proto_defense_rating(proto_id) >= 0 ? say_defense(proto_id) 
        : label_get(196); // 'Psi'
    output += "-";
    output += std::to_string(speed_proto(proto_id) / Rules->move_rate_roads);
    uint32_t triad = get_proto_triad(proto_id);
    if (triad == TRIAD_SEA) {
        output += " ";
        output += label_get(163); // 'Sea'
    } else if (triad == TRIAD_AIR) {
        output += " ";
        output += label_get(162); // 'Air'
    }
    uint32_t reactor = VehPrototypes[proto_id].reactor_id;
    if (reactor > 1) {
        output += "*";
        output += std::to_string(reactor);
    }
    // assumes at least 1032 char buffer (stringTemp), eventually remove
    // TODO: Eventually remove LPSTR stat param and return std::string instead
    strcat_s(stat, 1032, output.c_str());
}

/*
Purpose: Generate verbose stats string for specified prototype. Used by Design Workshop and Military
         Command Nexus. Replaced existing non-safe strcat with string. Reworked to integrate with 
         existing C code.
Original Offset: 0057DAA0
Return Value: n/a
Status: Complete
*/
void __cdecl say_stats(LPSTR stat, uint32_t proto_id, LPSTR custom_spacer) {
    std::string output;
    uint8_t plan = VehPrototypes[proto_id].plan;
    uint8_t chas = VehPrototypes[proto_id].chassis_id;
    uint8_t triad = Chassis[chas].triad;
    uint8_t mode = Weapon[VehPrototypes[proto_id].weapon_id].mode;
    int off_rating = get_proto_offense_rating(proto_id);
    int def_rating = get_proto_defense_rating(proto_id);
    if (plan == PLAN_RECONNAISANCE && triad == TRIAD_LAND && off_rating == 1 && def_rating == 1
        && !VehPrototypes[proto_id].ability_flags) {
        output = StringTable->get((int)PlansFullName[3]); // 'Explore/Defense'
        output += ", ";
    } else if (mode < 3) { // Projectile, energy, missile
        output = (plan != PLAN_DEFENSIVE || (off_rating >= 0 && off_rating <= def_rating))
            ? StringTable->get((int)PlansShortName[plan]) : label_get(312); // 'Combat'
        output += ", ";
    }
    if (off_rating < 0 || mode < 3) {
        output += (off_rating < 0) ? label_get(196) : say_offense(proto_id); // 'Psi'
        output += custom_spacer ? custom_spacer : "/";
        output += (def_rating < 0) ? label_get(196) : say_defense(proto_id); // 'Psi'
        output += custom_spacer ? custom_spacer : "/";
        output += std::to_string(speed_proto(proto_id) / Rules->move_rate_roads);
    } else if (def_rating != 1 || VehPrototypes[proto_id].ability_flags || (Chassis[chas].speed != 1
        && (mode != WPN_MODE_TRANSPORT || chas != CHSI_FOIL))) {
        output += StringTable->get(int(PlansShortName[mode]));
        if (plan == PLAN_NAVAL_TRANSPORT) {
            output += "(";
            output += std::to_string(VehPrototypes[proto_id].carry_capacity);
            output += ")";
        }
        output += ", ";
        output += (def_rating < 0) ? label_get(196) : say_defense(proto_id); // 'Psi'
        output += custom_spacer ? custom_spacer : "/";
        output += std::to_string(speed_proto(proto_id) / Rules->move_rate_roads);
    } else {
        output += StringTable->get((int)PlansFullName[mode]);
        if (plan == PLAN_NAVAL_TRANSPORT) {
            output += "(";
            output += std::to_string(VehPrototypes[proto_id].carry_capacity);
            output += ")";
        }
        if (triad) { // sea, air
            output += ","; // Bug fix: removed extra space
        }
    }
    if (triad == TRIAD_SEA) {
        output += " ";
        output += label_get(163); // 'Sea'
    } else if (triad == TRIAD_AIR) {
        output += " ";
        output += label_get(162); // 'Air'
    }
    uint32_t reactor = VehPrototypes[proto_id].reactor_id;
    if (reactor > 1) {
        output += " (*";
        output += std::to_string(reactor);
        output += ")";
    }
    // assumes at least 1032 char buffer (stringTemp), eventually remove
    // TODO: Eventually remove LPSTR stat param and return std::string instead
    strcat_s(stat, 1032, output.c_str());
}

/*
Purpose: Check whether the specified human player has already rejected ("bunged") this exact unit
         design combination, so it isn't proposed again. AI factions don't keep a list.
Original Offset: 0057DFC0
Return Value: Has the design been rejected? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-veh-helpers.log)
*/
BOOL __cdecl is_bunged(uint32_t faction_id, uint32_t chassis_id, uint32_t weapon_id,
                       uint32_t armor_id, int ability_flags, uint32_t reactor_id) {
    if (is_human(faction_id)) {
        for (int i = 0; i < MaxBungedDesignNum; i++) {
            BungedDesign *design = &BungList[faction_id * MaxBungedDesignNum + i];
            if (design->chassis_id == chassis_id && design->weapon_id == weapon_id
                && design->armor_id == armor_id && design->ability_flags == ability_flags
                && design->reactor_id == reactor_id) {
                return true;
            }
        }
    }
    return false;
}

/*
Purpose: Reset the list of rejected unit designs for the specified faction.
Original Offset: 0057DF00
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-veh-helpers.log)
*/
void __cdecl clear_bunglist(uint32_t faction_id) {
    for (int i = 0; i < MaxBungedDesignNum; i++) {
        BungedDesign *design = &BungList[faction_id * MaxBungedDesignNum + i];
        design->ability_flags = -1;
        design->chassis_id = 0xFF;
        design->weapon_id = 0xFF;
        design->armor_id = 0xFF;
        design->reactor_id = 0xFF;
    }
}

/*
Purpose: Mark a unit design name as a later model: append " Mk2" when it has no Mk number at the
         end, otherwise raise the number (up to 9). Used by name_proto().
Original Offset: n/a (inlined twice in name_proto)
Return Value: Is the number already past 8, so it can't be raised? true/false
Status: Complete
*/
static BOOL name_proto_mk(LPSTR name) {
    LPSTR mk = NULL; // last occurrence of the Mk label
    do {
        mk = strstr(mk ? mk + 1 : name, label_get(919));
        if (!mk) {
            break;
        }
    } while (strstr(mk + 1, label_get(919)));
    if (!mk || strlen(label_get(919)) + 1 < strlen(mk)) {
        char num[80];
        strcat(name, " ");
        strcat(name, label_get(919));
        strcat(name, _itoa(2, num, 10));
        return false;
    }
    size_t len = strlen(name);
    char last = name[len - 1];
    if (last < '2' || last > '8') {
        return true;
    }
    name[len - 1] = last + 1;
    return false;
}

/*
Purpose: Generate the default name of a unit design. Two candidates are built side by side, each
         from an ability prefix, the armor, weapon or reactor and one of the chassis' noun forms;
         the faction id, reactor, weapon or armor parity and the proto slot decide which one is
         used. A name already taken by a different design gets " Mk 2" or its Mk number raised.
Original Offset: 0057E040
Return Value: n/a (the name is written to the name buffer, at least 256 bytes)
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch23.log
        and 2026-10-16-sweep-batch23.log)
*/
void __cdecl name_proto(LPSTR name, int proto_id, int faction_id, uint32_t chassis_id,
                        uint32_t weapon_id, uint32_t armor_id, uint32_t abilities,
                        uint32_t reactor_id) {
    char alt_name[256];
    *name = 0;
    for (int i = 0; i < MaxAbilityNum; i++) {
        if (abilities & (1 << i) && strlen(StringTable->get((int)Ability[i].abbreviation))) {
            // no prefix for air superiority on an air chassis or for artillery; amphibious only
            // names a slow (speed 1) unit when it's defensive or artillery
            if ((i != 5 || Chassis[chassis_id].triad != TRIAD_AIR) && i != 15) { // abil_index()
                if (i != 3 || Chassis[chassis_id].speed != 1
                    || Weapon[weapon_id].offense_rating < Armor[armor_id].defense_rating
                    || (abilities & ABL_ARTILLERY)) {
                    strcat(name, StringTable->get((int)Ability[i].abbreviation));
                    strcat(name, " ");
                    break;
                }
            }
        }
    }
    strcpy(alt_name, name);
    int8_t offense = Weapon[weapon_id].offense_rating;
    int8_t defense = Armor[armor_id].defense_rating;
    BOOL large = false; // use the chassis' large noun
    BOOL not_sea = false;
    if (Weapon[weapon_id].mode < 3) {
        int threshold;
        int lesser;
        if (offense < defense) {
            threshold = 6;
            lesser = offense;
        } else {
            threshold = 3;
            lesser = defense;
        }
        uint8_t triad = Chassis[chassis_id].triad;
        if (triad == TRIAD_SEA) {
            threshold++;
        }
        if (triad == TRIAD_AIR) {
            threshold += (uint32_t)threshold / 2;
        }
        large = threshold < lesser;
        not_sea = triad != TRIAD_SEA;
        if (!strlen(StringTable->get((int)Chassis[chassis_id].offsv_lrg_name))
            || !strlen(StringTable->get((int)Chassis[chassis_id].defsv_lrg_name))) {
            large = false;
        }
    }
    BOOL no_component = false; // the weapon and armor aren't part of the name
    BOOL use_alt = ((reactor_id - 1) ^ faction_id) & 1;
    if ((VehPrototypes[proto_id].unk_1 == 10 && !(*FactionsStatus & (1 << faction_id))
        && Chassis[chassis_id].speed > 1)
        || (offense <= Chassis[chassis_id].speed && defense <= Chassis[chassis_id].speed
            && offense >= 0 && Chassis[chassis_id].triad == TRIAD_LAND)) {
        if (Weapon[weapon_id].mode < 3) {
            if (!has_abil(proto_id, ABL_POLICE_2X)) {
                int label = (offense < 2) ? 146 : 182;
                strcat(name, label_get(label));
                strcat(alt_name, label_get(label));
            } else {
                if (strlen(name)) {
                    name[strlen(name) - 1] = 0;
                }
                if (strlen(alt_name)) {
                    alt_name[strlen(name) - 1] = 0; // original measures the already shortened name
                }
            }
            no_component = true;
            use_alt = Chassis[chassis_id].speed > 1;
        }
    }
    BOOL use_reactor = false; // the faction's matching designs all have weaker reactors
    if (!no_component) {
        for (int i = 0; i < 128; i++) {
            int id = (i < MaxVehProtoFactionNum ? 0 : faction_id * MaxVehProtoFactionNum)
                + i % MaxVehProtoFactionNum;
            if (VehPrototypes[id].flags & PROTO_ACTIVE
                && VehPrototypes[id].plan == VehPrototypes[proto_id].plan
                && VehPrototypes[id].chassis_id == VehPrototypes[proto_id].chassis_id) {
                if (VehPrototypes[proto_id].reactor_id <= VehPrototypes[id].reactor_id) {
                    use_reactor = false;
                    break;
                }
                use_reactor = true;
            }
        }
    }
    LPSTR reactor_name = (LPSTR)Reactor[reactor_id - 1].name_short;
    LPSTR alt_part; // string table key of the last part of the alternative name
    if (offense < defense && Weapon[weapon_id].mode < 3 && offense >= 0
        && !(abilities & ABL_ARTILLERY)) {
        // defensive: armor (or reactor) and a defensive chassis noun
        if (!no_component) {
            use_alt ^= Armor[armor_id].defense_rating & 1;
            LPSTR part = use_reactor ? reactor_name : Armor[armor_id].name_short;
            strcat(name, StringTable->get((int)part));
            strcat(alt_name, StringTable->get((int)part));
        }
        strcat(name, " ");
        strcat(alt_name, " ");
        strcat(name, StringTable->get((int)Chassis[chassis_id].defsv_1_name));
        if (!no_component || !(proto_id & 1)) {
            alt_part = (!large || not_sea) ? Chassis[chassis_id].defsv_2_name
                : Chassis[chassis_id].defsv_lrg_name;
        } else {
            alt_part = (!large || !not_sea) ? Chassis[chassis_id].defsv_1_name
                : Chassis[chassis_id].defsv_lrg_name;
        }
    } else if (Chassis[chassis_id].triad == TRIAD_AIR && abilities & ABL_AIR_SUPERIORITY
        && Weapon[weapon_id].mode < 3 && offense >= 0) {
        // interceptor: weapon (or reactor) and a defensive chassis noun
        if (!no_component) {
            LPSTR part = use_reactor ? reactor_name : Weapon[weapon_id].name_short;
            strcat(name, StringTable->get((int)part));
            strcat(alt_name, StringTable->get((int)part));
        }
        strcat(name, " ");
        strcat(alt_name, " ");
        strcat(name, StringTable->get((int)Chassis[chassis_id].defsv_1_name));
        if (!no_component || !(proto_id & 1)) {
            alt_part = (!large || not_sea) ? Chassis[chassis_id].defsv_2_name
                : Chassis[chassis_id].defsv_lrg_name;
        } else {
            alt_part = (!large || !not_sea) ? Chassis[chassis_id].defsv_1_name
                : Chassis[chassis_id].defsv_lrg_name;
        }
    } else if (Chassis[chassis_id].missile && offense > 98) {
        // planet buster: reactor and weapon
        strcat(name, StringTable->get((int)reactor_name));
        strcat(alt_name, StringTable->get((int)reactor_name));
        strcat(name, " ");
        strcat(alt_name, " ");
        strcat(name, StringTable->get((int)Weapon[weapon_id].name_short));
        alt_part = Weapon[weapon_id].name_short;
    } else if (Chassis[chassis_id].missile
        && (weapon_id == WPN_TECTONIC_PAYLOAD || weapon_id == WPN_FUNGAL_PAYLOAD)) {
        // payload missile: weapon, reactor and the chassis noun
        strcat(name, StringTable->get((int)Weapon[weapon_id].name_short));
        strcat(alt_name, StringTable->get((int)Weapon[weapon_id].name_short));
        strcat(name, " ");
        strcat(alt_name, " ");
        strcat(name, StringTable->get((int)reactor_name));
        strcat(alt_name, StringTable->get((int)reactor_name));
        strcat(name, " ");
        strcat(alt_name, " ");
        strcat(name, StringTable->get((int)Chassis[chassis_id].offsv_1_name));
        alt_part = Chassis[chassis_id].offsv_1_name;
    } else {
        uint8_t mode = Weapon[weapon_id].mode;
        if (mode > 2 && chassis_id) {
            // non-combat unit off foot: reactor (above fission) and the chassis noun
            if ((int)reactor_id > 1) {
                strcat(name, StringTable->get((int)reactor_name));
                strcat(alt_name, StringTable->get((int)reactor_name));
                strcat(name, " ");
                strcat(alt_name, " ");
            }
            if ((mode != WPN_MODE_TRANSPORT || chassis_id != CHSI_FOIL)
                && (mode != WPN_MODE_TERRAFORMER || Chassis[chassis_id].triad != TRIAD_LAND
                    || !strchr(name, ' '))) {
                if ((mode == WPN_MODE_COLONIST || mode == WPN_MODE_TERRAFORMER)
                    && chassis_id == CHSI_FOIL) {
                    strcat(name, label_get(163));
                    strcat(alt_name, label_get(163));
                } else {
                    strcat(name, StringTable->get((int)Chassis[chassis_id].offsv_1_name));
                    strcat(alt_name, StringTable->get((int)Chassis[chassis_id].offsv_2_name));
                }
                strcat(name, " ");
                strcat(alt_name, " ");
            }
        }
        if (!no_component) {
            use_alt ^= Weapon[weapon_id].offense_rating & 1;
            strcat(name, StringTable->get((int)Weapon[weapon_id].name_short));
            strcat(alt_name, StringTable->get((int)Weapon[weapon_id].name_short));
        }
        if (mode > 2) {
            alt_part = NULL;
        } else if (abilities & ABL_ARTILLERY) {
            strcat(name, " ");
            strcat(name, label_get(543));
            strcat(alt_name, " ");
            alt_part = ((LPSTR *)Labels->strings_ptr)[544];
        } else if (abilities & ABL_AMPHIBIOUS && Chassis[chassis_id].speed == 1
            && offense > defense) {
            strcat(name, " ");
            strcat(name, label_get(614));
            strcat(alt_name, " ");
            alt_part = ((LPSTR *)Labels->strings_ptr)[615];
        } else {
            // offensive: weapon and an offensive chassis noun (or the reactor)
            strcat(name, " ");
            if (use_reactor) {
                strcat(name, StringTable->get((int)reactor_name));
            } else {
                strcat(name, StringTable->get((int)((!large || !not_sea)
                    ? Chassis[chassis_id].offsv_1_name : Chassis[chassis_id].offsv_lrg_name)));
            }
            strcat(alt_name, " ");
            if (use_reactor) {
                alt_part = reactor_name;
            } else {
                alt_part = (!large || not_sea) ? Chassis[chassis_id].offsv_2_name
                    : Chassis[chassis_id].offsv_lrg_name;
            }
        }
    }
    if (alt_part) {
        strcat(alt_name, StringTable->get((int)alt_part));
    }
    BOOL names_differ = strcmp(name, alt_name) != 0;
    BOOL stop = false; // the Mk number can't be raised any further
    for (int i = 0; i < MaxVehProtoNum; i++) {
        int owner = i / MaxVehProtoFactionNum;
        if (i != proto_id && (!owner || owner == faction_id)
            && (VehPrototypes[i].chassis_id != chassis_id
                || VehPrototypes[i].weapon_id != weapon_id
                || VehPrototypes[i].armor_id != armor_id
                || VehPrototypes[i].reactor_id != reactor_id
                || VehPrototypes[i].ability_flags != abilities)) {
            if (!stop && !strcmp(name, VehPrototypes[i].veh_name)) {
                if (names_differ) {
                    use_alt = true;
                }
                if (name_proto_mk(name)) {
                    stop = true;
                }
                i = -1;
            }
            // Original compares against the slot before the first after a restart (index -1).
            if (names_differ && !strcmp(alt_name, VehPrototypes[i].veh_name)) {
                use_alt = false;
                if (name_proto_mk(alt_name)) {
                    stop = true;
                }
                i = -1;
            }
        }
    }
    if (use_alt) {
        strcpy(name, alt_name);
    }
}

/*
Purpose: Check against faction's available tech for best available reactor.
Original Offset: 0057EFA0
Return Value: Best reactor available (1-4)
Status: Complete
*/
uint32_t __cdecl best_reactor(int faction_id) {
    for (int i = MaxReactorNum - 1; i >= 0; i--) {
        if (has_tech(Reactor[i].preq_tech, faction_id)) {
            return i + 1;
        }
    }
    return RECT_FISSION; // default
}

/*
Purpose: Check against faction's available tech for best available chassis meeting requirements.
Original Offset: 0057EFF0
Return Value: Best chassis available (0-8)
Status: Complete
*/
int __cdecl pick_chassis(int faction_id, int triad_chk, int speed_chk) {
    int chassis_id = -1;
    int best_speed = 0;
    for (int i = 0; i < MaxChassisNum; i++) {
        if (has_tech(Chassis[i].preq_tech, faction_id) && (triad_chk < 0 
            || triad_chk == Chassis[i].triad) && (!Chassis[i].missile || speed_chk == -1)) {
            int speed_compare = 1;
            if (speed_chk <= 0) { // 0, -1, -2
                if (!speed_chk || triad_chk != TRIAD_AIR || (speed_chk == -1 && Chassis[i].missile)
                    || (speed_chk == -2 && Chassis[i].range == 1)) {
                    speed_compare = Chassis[i].speed + 1;
                    if (speed_compare > best_speed) {
                        best_speed = speed_compare;
                        chassis_id = i;
                    }
                }
            } else if (Chassis[i].speed >= speed_chk) { // 1, 2, 3
                if (Chassis[i].speed == speed_chk) {
                    speed_compare = 2;
                }
                if (speed_compare > best_speed) {
                    best_speed = speed_compare;
                    chassis_id = i;
                }
            }
        }
    }
    return chassis_id;
}

/*
Purpose: Check against faction's available tech for the best available weapon meeting requirements.
         The condition variable has a dual purpose of either max cost or search for 1st instance of
         a particular weapon mode.
Original Offset: 0057F0B0
Return Value: Best weapon available (0-25)
Status: Complete
*/
int __cdecl weapon_budget(int faction_id, int condition, BOOL check_mode) {
    int weapon_id = 0;
    int best_offense = -1;
    for (int i = 0; i < MaxWeaponNum; i++) {
        if (has_tech(Weapon[i].preq_tech, faction_id)) {
            if (condition >= 0) {
                if ((!check_mode || Weapon[i].mode < 2) && Weapon[i].cost <= condition
                    && Weapon[i].mode < 3 && Weapon[i].offense_rating < 99
                    && i != 23) { // Exclude Conventional Payload
                    int offense_compare = Weapon[i].offense_rating * 2;
                    if (offense_compare >= best_offense) {
                        best_offense = offense_compare;
                        weapon_id = i;
                    }
                }
            } else if (Weapon[i].mode == -condition) {
                weapon_id = i;
                break;
            }
        }
    }
    return weapon_id;
}

/*
Purpose: Check against the faction's available tech for the best available armor meeting the cost 
         requirement.
Original Offset: 0057F150
Return Value: Best armor available (0-13)
Status: Complete
*/
int __cdecl armor_budget(int faction_id, int max_cost) {
    int armor_id = 0;
    int best_defense = -1;
    for (int i = 0; i < MaxArmorNum; i++) {
        if (has_tech(Armor[i].preq_tech, faction_id) && Armor[i].cost <= max_cost
            && Armor[i].defense_rating >= 0) { // excludes PSI
            int defense_compare = Armor[i].defense_rating;
            if (faction_id >= 0 && !*ExpansionEnabled && i > 9) {
                defense_compare = 1; // Pulse + Resonance in non-expansion mode
            }                       // This really only affects Pulse 8 Armor (Super Tensile Solids)
            defense_compare *= 2;
            if (defense_compare >= best_defense) {
                armor_id = i;
                best_defense = defense_compare;
            }
        }
    }
    return armor_id;
}

/*
Purpose: Record a prototype's design in the faction's bunglist so the design workshop won't offer
         the same combination again. The list is a 64 entry ring per faction; see is_bunged().
Original Offset: 0057DF30
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch2.log)
*/
void __cdecl bung_proto(uint32_t faction_id, int proto_id) {
    int index = PlayersData[faction_id].bunglist_index;
    PlayersData[faction_id].bunglist_index = (index + 1) % MaxBungedDesignNum;
    BungedDesign *design = &BungList[faction_id * MaxBungedDesignNum + index];
    design->ability_flags = VehPrototypes[proto_id].ability_flags;
    design->chassis_id = VehPrototypes[proto_id].chassis_id;
    design->weapon_id = VehPrototypes[proto_id].weapon_id;
    design->armor_id = VehPrototypes[proto_id].armor_id;
    design->reactor_id = VehPrototypes[proto_id].reactor_id;
}

/*
Purpose: Retire a prototype for the faction. The design is added to the bunglist, then every unit
         of the prototype is dealt with: an AI faction's unit is upgraded to the best remaining
         active prototype of the same category (when no human can see it, or it sits in a base),
         otherwise a unit inside a base is turned into a Scout Patrol if the base would be left
         undefended or disbanded into the base's production for half its cost; anything else is
         killed. A custom prototype is also removed from every production queue and deactivated.
Original Offset: 0057F1D0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch2.log)
*/
void __cdecl retire_proto(int proto_id, int faction_id) {
    bung_proto(faction_id, proto_id);
    int8_t category = VehPrototypes[proto_id].unk_1;
    int rank_best = -1;
    int proto_id_best = -1;
    for (int i = 0; i < MaxVehProtoFactionNum; i++) {
        int id = faction_id * MaxVehProtoFactionNum + i;
        if (id != proto_id && (VehPrototypes[id].flags & PROTO_ACTIVE)
            && VehPrototypes[id].unk_1 == category) {
            int rank = (VehPrototypes[id].obsolete_factions & (1 << faction_id)) ? 1 : 2;
            if (rank > rank_best) {
                rank_best = rank;
                proto_id_best = id;
            }
        }
    }
    for (int veh_id = *VehCurrentCount - 1; veh_id >= 0; veh_id--) {
        if (veh_id < *VehCurrentCount && Vehs[veh_id].proto_id == proto_id) {
            if (!is_human(faction_id)) {
                int x = Vehs[veh_id].x;
                int y = Vehs[veh_id].y;
                if (proto_id_best >= 0) {
                    BOOL upgrade = !(Vehs[veh_id].visibility & FactionsStatus[0]); // unseen by humans
                    if (!upgrade) {
                        Map *tile = map_loc(x, y);
                        int owner = tile->val2 & 0xF;
                        upgrade = (tile->bit & BIT_BASE_IN_TILE) && owner > 0 && owner < MaxPlayerNum;
                    }
                    if (upgrade && Vehs[veh_id].faction_id == faction_id) {
                        Vehs[veh_id].proto_id = (int16_t)proto_id_best;
                        PlayersData[faction_id].proto_id_active[proto_id]--;
                        PlayersData[faction_id].proto_id_active[proto_id_best]++;
                        continue;
                    }
                }
                int base_id = base_at(x, y);
                if (base_id >= 0) {
                    if (stack_check(veh_id, 1, -1, -1, -1) == 1) {
                        Vehs[veh_id].proto_id = BSC_SCOUT_PATROL;
                        PlayersData[faction_id].proto_id_active[proto_id]--;
                        PlayersData[faction_id].proto_id_active[BSC_SCOUT_PATROL]++;
                        continue;
                    }
                    int minerals = Bases[base_id].minerals_accumulated
                        + VehPrototypes[Vehs[veh_id].proto_id].cost / 2;
                    Bases[base_id].minerals_accumulated = minerals;
                    Bases[base_id].minerals_accumulated_2 = minerals;
                    Bases[base_id].production_id_last = Bases[base_id].queue_production_id[0];
                }
            }
            kill(veh_id);
        }
    }
    if (proto_id >= MaxVehProtoFactionNum) {
        for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
            Base *base = &Bases[base_id];
            for (int i = 0; i <= (int)base->queue_size; i++) {
                if (base->queue_production_id[i] == proto_id) {
                    if (!i) {
                        base->queue_production_id[0] = -FAC_STOCKPILE_ENERGY;
                    } else {
                        for (int j = i; j < (int)base->queue_size; j++) {
                            base->queue_production_id[j] = base->queue_production_id[j + 1];
                        }
                        base->queue_size--;
                    }
                }
            }
        }
        VehPrototypes[proto_id].flags &= ~PROTO_ACTIVE;
    }
}

/*
Purpose: Write a prototype's name, stats and ability abbreviations ("Name (stats abilities)") into
         stringTemp for the prune_protos() popups. Unlike upgrade_prototype_say() it reads the
         abilities from the prototype's flags only, without the faction's free abilities.
Original Offset: n/a (inlined four times in prune_protos)
Return Value: n/a
Status: Complete
*/
static void prune_protos_say(int proto_id) {
    stringTemp->str[0] = 0;
    strcat(stringTemp->str, VehPrototypes[proto_id].veh_name);
    strcat(stringTemp->str, " ");
    strcat(stringTemp->str, "(");
    say_stats_2(stringTemp->str, proto_id);
    if (VehPrototypes[proto_id].ability_flags) {
        strcat(stringTemp->str, " ");
        for (int i = 0; i < MaxAbilityNum; i++) {
            if (VehPrototypes[proto_id].ability_flags & (1 << i)) {
                strcat(stringTemp->str, StringTable->get((int)Ability[i].abbreviation));
            }
        }
    }
    strcat(stringTemp->str, ")");
}

/*
Purpose: Mark the designs a faction no longer needs as obsolete for it. First, when the kept
         prototype is basic or typed complete, each of the faction's untyped designs whose weapon,
         armor and chassis (or better ones) are all in designs it can build is flagged typed
         complete and upgrade pending. A computer player then obsoletes, for each design it can
         build, every basic design and every design of its own of the same category, triad and
         plan that has a smaller reactor, or is no better in range, weapon, carrying capacity,
         armor, speed and abilities. Humans only get the rest with the "auto prune obsolete units"
         preference or in a network game: a design no better than another one in every respect
         is marked obsolete and replaced by it in the faction's production queues. The player at
         the controls is asked first (0: replace, 1: keep the old design and stop calling it
         obsolete, 2: replace it everywhere without asking again); in a network game the player is
         told afterwards instead. The kept prototype is only flagged (PROTO_UNK_8), never
         replaced. With synch set the changes are sent to the other players and each base is
         locked while its queue changes.
Original Offset: 0057F4B0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl prune_protos(int faction_id, int proto_id_kept, BOOL synch) {
    const uint32_t ability_roles = ABL_AMPHIBIOUS | ABL_DROP_POD | ABL_ANTIGRAV_STRUTS
        | ABL_ARTILLERY | ABL_POLICE_2X;
    if (proto_id_kept < MaxVehProtoFactionNum
        || VehPrototypes[proto_id_kept].flags & PROTO_TYPED_COMPLETE) {
        for (int i = 0; i < MaxVehProtoFactionNum; i++) {
            VehPrototype *proto = &VehPrototypes[faction_id * MaxVehProtoFactionNum + i];
            if (!(proto->flags & PROTO_ACTIVE) || proto->flags & PROTO_TYPED_COMPLETE) {
                continue;
            }
            BOOL has_weapon = false;
            BOOL has_armor = false;
            BOOL has_chassis = false;
            for (int j = 0; j < 2 * MaxVehProtoFactionNum; j++) {
                int id = (j < MaxVehProtoFactionNum ? 0 : faction_id * MaxVehProtoFactionNum)
                    + j % MaxVehProtoFactionNum;
                VehPrototype *other = &VehPrototypes[id];
                if (!(other->flags & PROTO_ACTIVE)) {
                    continue;
                }
                if (id < MaxVehProtoFactionNum) {
                    if (!has_tech(other->preq_tech, faction_id)) {
                        continue;
                    }
                } else if (!(other->flags & PROTO_TYPED_COMPLETE)) {
                    continue;
                }
                if (other->weapon_id == proto->weapon_id) {
                    has_weapon = true;
                }
                if (other->armor_id == proto->armor_id) {
                    has_armor = true;
                }
                if (other->chassis_id == proto->chassis_id) {
                    has_chassis = true;
                }
                int8_t offense = Weapon[proto->weapon_id].offense_rating;
                if (offense > 0 && Weapon[other->weapon_id].offense_rating >= offense) {
                    has_weapon = true;
                }
                int8_t defense = Armor[proto->armor_id].defense_rating;
                if (defense > 0 && Armor[other->armor_id].defense_rating >= defense) {
                    has_armor = true;
                }
                if (Chassis[other->chassis_id].triad == Chassis[proto->chassis_id].triad
                    && Chassis[other->chassis_id].speed >= Chassis[proto->chassis_id].speed) {
                    has_chassis = true;
                }
            }
            if (has_weapon && has_armor && has_chassis) {
                proto->flags |= PROTO_TYPED_COMPLETE | PROTO_UNK_100;
            }
        }
    }
    uint8_t faction_bit = (uint8_t)(1 << faction_id);
    if (!(*FactionsStatus & faction_bit)) {
        for (int i = 0; i < MaxVehProtoFactionNum; i++) {
            int proto_id = faction_id * MaxVehProtoFactionNum + i;
            VehPrototype *proto = &VehPrototypes[proto_id];
            if (!(proto->flags & PROTO_ACTIVE) || proto->obsolete_factions & faction_bit) {
                continue;
            }
            BOOL typed;
            if (proto_id < MaxVehProtoFactionNum) {
                if (!has_tech(proto->preq_tech, faction_id)) {
                    continue;
                }
                typed = true;
            } else {
                typed = (proto->flags & PROTO_TYPED_COMPLETE) != 0;
            }
            if (typed) {
                // basic designs (the original also checks the id is basic, always true here)
                for (int id = 0; id < MaxVehProtoFactionNum; id++) {
                    VehPrototype *basic = &VehPrototypes[id];
                    if (!(basic->flags & PROTO_ACTIVE) || id == proto_id
                        || Weapon[basic->weapon_id].offense_rating < 0 || id == BSC_SPORE_LAUNCHER
                        || basic->unk_1 != proto->unk_1
                        || Chassis[basic->chassis_id].triad != Chassis[proto->chassis_id].triad
                        || basic->plan != proto->plan) {
                        continue;
                    }
                    if (basic->reactor_id >= proto->reactor_id
                        || Weapon[basic->weapon_id].offense_rating < 0) {
                        if (Chassis[basic->chassis_id].range != Chassis[proto->chassis_id].range) {
                            continue;
                        }
                        int8_t offense = Weapon[basic->weapon_id].offense_rating;
                        if (offense > 0 ? offense > Weapon[proto->weapon_id].offense_rating
                            : basic->weapon_id != proto->weapon_id) {
                            continue;
                        }
                        if (basic->carry_capacity > proto->carry_capacity) {
                            continue;
                        }
                        if (Armor[basic->armor_id].defense_rating > 0
                            ? arm_val(basic->armor_id, faction_id) > arm_val(proto->armor_id, faction_id)
                            : basic->armor_id != proto->armor_id) {
                            continue;
                        }
                        if (Chassis[basic->chassis_id].speed > Chassis[proto->chassis_id].speed
                            || (int)bit_count(basic->ability_flags & proto->ability_flags)
                            < (int)bit_count(basic->ability_flags)
                            || (basic->ability_flags ^ proto->ability_flags) & ability_roles) {
                            continue;
                        }
                    }
                    basic->obsolete_factions |= faction_bit;
                    if (synch) {
                        synch_obs(id);
                    }
                }
            }
            for (int id = faction_id * MaxVehProtoFactionNum;
                id < (faction_id + 1) * MaxVehProtoFactionNum; id++) {
                VehPrototype *other = &VehPrototypes[id];
                if (!(other->flags & PROTO_ACTIVE) || id == proto_id
                    || other->unk_1 != proto->unk_1
                    || Chassis[other->chassis_id].triad != Chassis[proto->chassis_id].triad
                    || other->plan != proto->plan
                    || (!typed && other->flags & PROTO_TYPED_COMPLETE)) {
                    continue;
                }
                if (other->reactor_id >= proto->reactor_id) {
                    if (other->carry_capacity > proto->carry_capacity
                        || Chassis[other->chassis_id].range != Chassis[proto->chassis_id].range) {
                        continue;
                    }
                    int8_t offense = Weapon[other->weapon_id].offense_rating;
                    if (offense > 0 ? offense > Weapon[proto->weapon_id].offense_rating
                        : other->weapon_id != proto->weapon_id) {
                        continue;
                    }
                    if (Armor[other->armor_id].defense_rating > 0
                        ? arm_val(other->armor_id, faction_id) > arm_val(proto->armor_id, faction_id)
                        : other->armor_id != proto->armor_id) {
                        continue;
                    }
                    if (Chassis[other->chassis_id].speed > Chassis[proto->chassis_id].speed) {
                        continue;
                    }
                    if (other->flags & PROTO_TYPED_COMPLETE
                        && (int)bit_count(proto->ability_flags & other->ability_flags)
                        < (int)bit_count(other->ability_flags)) {
                        continue;
                    }
                    if ((proto->ability_flags ^ other->ability_flags) & ability_roles) {
                        continue;
                    }
                }
                other->obsolete_factions |= faction_bit;
                if (synch) {
                    synch_obs(id);
                }
            }
        }
        return;
    }
    if (!*IsMultiplayerNet && !(*GameMorePreferences & MPREF_BSC_AUTO_PRUNE_OBS_VEH)) {
        return;
    }
    // abilities a design may have or lack and still be called no better than another
    const uint32_t ability_extras = ABL_SUPER_TERRAFORMER | ABL_DEEP_RADAR | ABL_COMM_JAMMER
        | ABL_EMPATHIC | ABL_FUNGICIDAL | ABL_TRANCE;
    for (int i = 0; i < 2 * MaxVehProtoFactionNum; i++) {
        int proto_id = i % MaxVehProtoFactionNum;
        if (i >= MaxVehProtoFactionNum) {
            proto_id += faction_id * MaxVehProtoFactionNum;
        } else if (!has_tech(VehPrototypes[proto_id].preq_tech, faction_id)) {
            continue;
        }
        VehPrototype *proto = &VehPrototypes[proto_id];
        if (proto->obsolete_factions & faction_bit || !(proto->flags & PROTO_ACTIVE)) {
            continue;
        }
        for (int j = 0; j < 2 * MaxVehProtoFactionNum; j++) {
            int id = j % MaxVehProtoFactionNum;
            if (j >= MaxVehProtoFactionNum) {
                id += faction_id * MaxVehProtoFactionNum;
            }
            if (j == i) {
                continue;
            }
            VehPrototype *old = &VehPrototypes[id];
            uint16_t flags = old->flags;
            if (!(flags & PROTO_ACTIVE) || old->obsolete_factions & faction_bit
                || (id < MaxVehProtoFactionNum && (Weapon[old->weapon_id].offense_rating < 0
                || id == BSC_SPORE_LAUNCHER)) || flags & PROTO_UNK_200) {
                continue;
            }
            BOOL kept_before = (flags & PROTO_UNK_8) != 0;
            if (proto_id >= MaxVehProtoFactionNum && !(proto->flags & PROTO_TYPED_COMPLETE)
                && (id < MaxVehProtoFactionNum || flags & PROTO_TYPED_COMPLETE)) {
                continue;
            }
            if (kept_before && old->reactor_id >= proto->reactor_id) {
                continue;
            }
            int8_t offense = Weapon[old->weapon_id].offense_rating;
            if (offense > 0 ? offense > Weapon[proto->weapon_id].offense_rating
                : old->weapon_id != proto->weapon_id) {
                continue;
            }
            if (Armor[old->armor_id].defense_rating > 0
                ? arm_val(old->armor_id, faction_id) > arm_val(proto->armor_id, faction_id)
                : old->armor_id != proto->armor_id) {
                continue;
            }
            if (old->plan != PLAN_RECONNAISANCE
                && ((Weapon[old->weapon_id].offense_rating > 1)
                != (Weapon[proto->weapon_id].offense_rating > 1)
                || (Armor[old->armor_id].defense_rating > 1)
                != (Armor[proto->armor_id].defense_rating > 1))) {
                continue;
            }
            if (old->reactor_id > proto->reactor_id || old->carry_capacity > proto->carry_capacity) {
                continue;
            }
            // the original compares the range with the kept prototype's chassis, not this one's
            if (Chassis[old->chassis_id].range
                != Chassis[VehPrototypes[proto_id_kept].chassis_id].range
                || Chassis[old->chassis_id].triad != Chassis[proto->chassis_id].triad
                || Chassis[old->chassis_id].speed > Chassis[proto->chassis_id].speed) {
                continue;
            }
            uint32_t abls_old = old->ability_flags;
            uint32_t abls = proto->ability_flags;
            if (abls_old != abls) {
                if ((abls ^ abls_old) & ~ability_extras) {
                    continue;
                }
                if (old->reactor_id >= proto->reactor_id && (abls & abls_old) != abls_old
                    && (proto_id < MaxVehProtoFactionNum || PlayersData[faction_id].proto_id_active[id]
                    || PlayersData[faction_id].proto_id_queue[id])) {
                    continue;
                }
            }
            if (old->plan != proto->plan) {
                continue;
            }
            if (kept_before && (old->unk_1 != proto->unk_1 || (abls & abls_old) != abls_old)) {
                continue;
            }
            if (old->cost < proto->cost && old->reactor_id >= proto->reactor_id
                && (id < MaxVehProtoFactionNum || old->flags & PROTO_UNK_8)) {
                continue;
            }
            if (old->weapon_id == proto->weapon_id && old->armor_id == proto->armor_id
                && old->chassis_id == proto->chassis_id && old->reactor_id == proto->reactor_id
                && (id < MaxVehProtoFactionNum || proto_id < MaxVehProtoFactionNum)) {
                continue;
            }
            if (id == proto_id_kept) {
                old->flags |= PROTO_UNK_8;
                if (synch) {
                    synch_obs(id);
                }
                continue;
            }
            old->obsolete_factions |= faction_bit;
            if (synch) {
                synch_obs(id);
            }
            if (id < MaxVehProtoFactionNum && !has_tech(old->preq_tech, faction_id)) {
                continue;
            }
            BOOL queued = false;
            for (int base_id = 0; base_id < *BaseCurrentCount && !queued; base_id++) {
                if (Bases[base_id].faction_id_current == faction_id) {
                    for (int k = 0; k <= (int)Bases[base_id].queue_size; k++) {
                        if (Bases[base_id].queue_production_id[k] == id) {
                            queued = true;
                            break;
                        }
                    }
                }
            }
            BOOL replaced = false;
            BOOL replace_all = false;
            BOOL kept = false;
            for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
                Base *base = &Bases[base_id];
                if (base->faction_id_current != faction_id || !queued) {
                    continue;
                }
                BOOL ask = !*IsMultiplayerNet && faction_id == *FactionIDCurrent && !replaced;
                if (!ask && synch && NetDaemon_lock_base(NetDaemonVar, NULL, base_id, 0, -1, -1)) {
                    continue;
                }
                if (ask) {
                    prune_protos_say(id);
                    parse_says(0, stringTemp->str, -1, -1);
                    prune_protos_say(proto_id);
                    parse_says(1, stringTemp->str, -1, -1);
                    int choice = 1;
                    if (!(old->flags & PROTO_UNK_200) && !replace_all) {
                        choice = (int)X_pop("ASKPRODUPGRADE", NULL);
                        if (choice > 1) {
                            replace_all = true;
                        }
                    }
                    if (!replace_all && (old->flags & PROTO_UNK_200 || choice == 1)) {
                        old->obsolete_factions &= ~faction_bit;
                        if (old->flags & PROTO_UNK_8) {
                            old->flags |= PROTO_UNK_200;
                        }
                        old->flags |= PROTO_UNK_8;
                        kept = true;
                        break;
                    }
                }
                replaced = true;
                for (int k = 0; k <= (int)base->queue_size; k++) {
                    if (base->queue_production_id[k] == id) {
                        base->queue_production_id[k] = proto_id;
                    }
                }
                if (*IsMultiplayerNet && synch) {
                    NetDaemon_unlock_base(NetDaemonVar, NULL, base_id);
                }
            }
            if (!kept && replaced && *IsMultiplayerNet && faction_id == *FactionIDCurrent) {
                prune_protos_say(id);
                parse_says(0, stringTemp->str, -1, -1);
                prune_protos_say(proto_id);
                parse_says(1, stringTemp->str, -1, -1);
                popp(ScriptTxtID, "PRODUPGRADE", 0,
                    (Players[faction_id].rule_flags & RFLAG_ALIEN) ? "alunit_sm.pcx" : "unit_sm.pcx",
                    0);
            }
        }
    }
}

/*
Purpose: Propose a unit design for a faction. An existing design with the same parts is reused
         (made current again); otherwise the design goes into the best free slot of the faction's
         64: an empty slot, an obsolete design with no units, or a little used one, which is retired
         first. The new prototype is named (name_proto() unless a name is given), the faction's
         prototypes are pruned and every queued obsolete design of the same category in its bases
         is replaced by it. While the design workshop asks consider_designs() for a suggestion
         (DesignSuggestState set) nothing is created; the matching or new design is recorded in
         the DesignSuggest globals instead. A category of -2 marks the design as typed complete.
Original Offset: 00580860
Return Value: Prototype id, or -1 if rejected, already owned by a human player or only suggested
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch23.log
        and 2026-10-16-sweep-batch23.log)
*/
int __cdecl propose_proto(uint32_t faction_id, uint32_t chassis_id, uint32_t weapon_id,
                          uint32_t armor_id, uint32_t abilities, uint32_t reactor_id,
                          int category, LPCSTR name) {
    if (!*DesignSuggestState) {
        if (is_bunged(faction_id, chassis_id, weapon_id, armor_id, abilities, reactor_id)) {
            return -1;
        }
    } else if (*DesignSuggestCategory >= 0 && *DesignSuggestCategory != category) {
        return -1;
    }
    if (Chassis[chassis_id].speed == 1 && Armor[armor_id].defense_rating == 1
        && Weapon[weapon_id].offense_rating == 1 && !*DesignSuggestState) {
        return BSC_SCOUT_PATROL;
    }
    int offense = weap_strat(weapon_id, faction_id); // inlined in the original
    int defense = arm_strat(armor_id, faction_id);
    uint8_t faction_bit = (uint8_t)(1 << faction_id);
    BOOL is_human_faction = *FactionsStatus & faction_bit;
    int score_best = 0;
    int proto_id_best = -1;
    for (int i = 0; i < 128; i++) {
        int proto_id = (i < MaxVehProtoFactionNum ? 0 : faction_id * MaxVehProtoFactionNum)
            + i % MaxVehProtoFactionNum;
        VehPrototype *proto = &VehPrototypes[proto_id];
        uint8_t active = PlayersData[faction_id].proto_id_active[proto_id];
        uint8_t queued = PlayersData[faction_id].proto_id_queue[proto_id];
        int score;
        if (proto_id >= MaxVehProtoFactionNum && proto->flags & PROTO_ACTIVE
            && proto->obsolete_factions & faction_bit && !active && !queued) {
            // unused obsolete design, best of the same category
            score = ((int)(uint8_t)proto->unk_1 != category) ? 0x1000 : 0x2000;
        } else if (!(proto->flags & PROTO_ACTIVE)) {
            if (proto_id < MaxVehProtoFactionNum) {
                continue;
            }
            score = 0x800; // empty slot
        } else {
            if (proto_id < MaxVehProtoFactionNum && !has_tech(proto->preq_tech, faction_id)) {
                continue;
            }
            if (proto->chassis_id == chassis_id) {
                if (proto->weapon_id == weapon_id && proto->armor_id == armor_id
                    && proto->ability_flags == abilities && proto->reactor_id == reactor_id
                    && !*DesignSuggestState && is_human_faction) {
                    return -1;
                }
                if (proto->weapon_id == weapon_id && proto->armor_id == armor_id
                    && !((proto->ability_flags ^ abilities) & 0x44AE1B)
                    && ((proto->ability_flags & abilities) == proto->ability_flags
                        || !is_human_faction)
                    && proto->reactor_id == reactor_id) {
                    // reuse the existing design
                    if (category == -2) {
                        proto->flags |= PROTO_TYPED_COMPLETE;
                    }
                    if (*DesignSuggestState && *DesignSuggestState <= 2) {
                        *DesignSuggestAbilities = abilities;
                        *DesignSuggestWeapon = weapon_id;
                        *DesignSuggestArmor = armor_id;
                        *DesignSuggestChassis = chassis_id;
                        *DesignSuggestReactor = reactor_id;
                        *DesignSuggestState = 2;
                    }
                    proto->obsolete_factions &= ~faction_bit;
                    return proto_id;
                }
            }
            if (proto_id < MaxVehProtoFactionNum) {
                continue;
            }
            if (proto->obsolete_factions & faction_bit && !active) {
                score = 0x400; // obsolete design with only queued units
            } else {
                if (proto->chassis_id == chassis_id && proto->reactor_id == reactor_id
                    && !((proto->ability_flags ^ abilities) & 0x48841A)) {
                    // an AI faction keeps a design that's close enough and stronger
                    int proto_offense = weap_strat(proto->weapon_id, faction_id);
                    int proto_defense = arm_strat(proto->armor_id, faction_id);
                    int high = (proto_offense > proto_defense) ? proto_offense : proto_defense;
                    int high_tie = high + (proto_offense == proto_defense);
                    if (offense < high && abs(offense - proto_offense) < 2 && defense < high_tie
                        && abs(defense - proto_defense) < 2 && !is_human_faction
                        && strcmp(Players[faction_id].filename, "UNIV")) {
                        return proto_id;
                    }
                }
                if ((int)(uint8_t)proto->unk_1 != category && (active || queued)) {
                    continue;
                }
                score = active ? 0x40 : 0x240;
                if (proto->obsolete_factions & faction_bit) {
                    score |= 0x100;
                }
                if (!queued) {
                    score |= 0x80;
                }
                int used = proto->cost * active + queued;
                score -= (used > 63) ? 63 : used;
            }
        }
        if (score > score_best) {
            score_best = score;
            proto_id_best = proto_id;
        }
    }
    if (proto_id_best < 0) {
        return -1;
    }
    if (*DesignSuggestState) {
        if (*DesignSuggestState <= 2) {
            *DesignSuggestWeapon = weapon_id;
            *DesignSuggestArmor = armor_id;
            *DesignSuggestChassis = chassis_id;
            *DesignSuggestAbilities = abilities;
            *DesignSuggestReactor = reactor_id;
            *DesignSuggestCategory = category;
            *DesignSuggestState = 3;
        }
        return -1;
    }
    VehPrototype *proto = &VehPrototypes[proto_id_best];
    BOOL was_empty = !(proto->flags & PROTO_ACTIVE);
    if (!was_empty && !*IsMultiplayerNet) {
        retire_proto(proto_id_best, faction_id);
    }
    make_proto(proto_id_best, chassis_id, weapon_id, armor_id, abilities, reactor_id);
    if (category == -2) {
        proto->flags |= PROTO_TYPED_COMPLETE;
    }
    if (*DiffLevelCurrent <= 1) {
        proto->flags |= PROTO_TYPED_COMPLETE;
    }
    if (was_empty) {
        proto->flags |= 0x40;
    }
    proto->flags |= 0x80;
    if (category >= 0) {
        proto->unk_1 = (int8_t)category;
        if (category == 10) {
            proto->plan = 3;
        }
    }
    if (name) {
        // no length check: the original copies any name in full
        strcpy(proto->veh_name, name);
        for (int i = 0; i < MaxVehProtoNum; i++) {
            int owner = i / MaxVehProtoFactionNum;
            if (i != proto_id_best && (!owner || owner == (int)faction_id)
                && !strcmp(proto->veh_name, VehPrototypes[i].veh_name)) {
                strcat(proto->veh_name, "*");
                i = -1;
            }
        }
    } else {
        char proto_name[256];
        name_proto(proto_name, proto_id_best, faction_id, chassis_id, weapon_id, armor_id,
            abilities, reactor_id);
        strncpy(proto->veh_name, proto_name, 32);
        proto->veh_name[31] = 0;
        proto->flags &= ~PROTO_CUSTOM_NAME_SET;
    }
    prune_protos(faction_id, proto_id_best, 0);
    // The original first looks for a queued obsolete design of the category and drops the result.
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        if (Bases[base_id].faction_id_current == faction_id) {
            for (int i = 0; i <= (int)Bases[base_id].queue_size; i++) {
                int queued_id = Bases[base_id].queue_production_id[i];
                if (queued_id >= 0 && VehPrototypes[queued_id].obsolete_factions & faction_bit
                    && VehPrototypes[queued_id].unk_1 == proto->unk_1) {
                    Bases[base_id].queue_production_id[i] = proto_id_best;
                }
            }
        }
    }
    if (*ProtoIDNewDesign < 0) {
        *ProtoIDNewDesign = proto_id_best;
    }
    return proto_id_best;
}

/*
Purpose: Get the index value of a particular ability's bitfield.
Original Offset: 00581170
Return Value: Ability index
Status: Complete
*/
int __cdecl abil_index(int ability_id) {
    int index = 0;
    for (int check = ability_id; !(check & 1); index++) {
        check >>= 1;
    }
    return index;
}

/*
Purpose: Add the specified ability to an empty design ability bitfield if the faction has the
         prerequisite tech. Abilities the faction gets for free (FREEABIL bonus) are skipped since
         they are granted elsewhere.
Original Offset: 00581190
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-veh-helpers.log)
*/
void __cdecl add_abil(uint32_t *ability_flags, uint32_t ability, uint32_t faction_id) {
    if (*ability_flags) {
        return;
    }
    int abil_id = abil_index(ability);
    if (!has_tech(Ability[abil_id].preq_tech, faction_id)) {
        return;
    }
    for (int i = 0; i < Players[faction_id].faction_bonus_count; i++) {
        if (Players[faction_id].faction_bonus_id[i] == RULE_FREEABIL
            && has_tech(Ability[Players[faction_id].faction_bonus_val1[i]].preq_tech, faction_id)
            && Players[faction_id].faction_bonus_val1[i] == abil_id) {
            return;
        }
    }
    *ability_flags |= (1 << abil_id);
}

/*
Purpose: The highest armor defense worth fitting to a combat design: the reactor power times the
         factor against the weapon's offense, between 2 and that offense. Used by
         consider_designs().
Original Offset: n/a (inlined three times in consider_designs)
Return Value: Defense limit
Status: Complete
*/
static int design_defense_limit(int factor, int power, int offense) {
    int limit = factor * power * 4 / offense;
    if (limit < 2 || offense < 2) {
        return 2;
    }
    return (limit > offense) ? offense : limit;
}

/*
Purpose: Propose the unit designs a faction should have now (for the player too while the design
         workshop asks for a suggestion; see propose_proto()). The faction's combat designs are
         first offered the best weapon and armor better than any design it can build, then new
         designs are proposed in turn: a defender with AAA and trance variants, an attacker with
         drop pod and amphibious variants, artillery, an air superiority unit, a scout, a warship
         and a defensive ship, a sea transport, a sea former and a sea colony pod, a bomber and an
         interceptor, missiles and a planet buster, and a former. Abilities follow the faction's
         priorities and its enemies: humans with a grievance or psi weapons, atrocity victims, a
         repealed UN Charter, enemy air power.
Original Offset: 00581260
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl consider_designs(int faction_id) {
    *ProtoIDNewDesign = -1;
    // grievances: atrocity victims, and factions wanting revenge (more so for humans above us)
    int threat = 0;
    for (int i = 1; i < MaxPlayerNum; i++) {
        if (i == faction_id) {
            continue;
        }
        uint32_t treaties = PlayersData[faction_id].diplo_treaties[i];
        if (treaties & DTREATY_ATROCITY_VICTIM) {
            threat += (PlayersData[i].atrocities + 2) / 3;
        } else if (treaties & DTREATY_WANT_REVENGE) {
            if (*FactionsStatus & (1 << i)
                && ((int)PlayersData[i].ranking > (int)PlayersData[faction_id].ranking
                || PlayersData[i].diplo_unk4[faction_id] > PlayersData[faction_id].diplo_unk4[i])) {
                threat += (*GameRules & RULES_INTENSE_RIVALRY) ? 1 : 0;
            }
            threat += (PlayersData[i].integrity_blemishes - 1) / 4;
        }
    }
    enemy_capabilities(faction_id);
    int reactor = best_reactor(faction_id);
    int power = 2 << reactor;
    uint8_t faction_bit = (uint8_t)(1 << faction_id);
    // the best weapon and armor that beat every design the faction can build
    int weapon_best = -1;
    for (int weapon_id = 0; weapon_id < MaxWeaponNum; weapon_id++) {
        int8_t offense = Weapon[weapon_id].offense_rating;
        if (!has_tech(Weapon[weapon_id].preq_tech, faction_id) || offense <= 0 || offense >= 99
            || weapon_id == WPN_CONVENTIONAL_PAYLOAD
            || (weapon_best >= 0 && Weapon[weapon_best].offense_rating > offense)) {
            continue;
        }
        for (int i = 0; i < 2 * MaxVehProtoFactionNum; i++) {
            int id = i % MaxVehProtoFactionNum
                + (i < MaxVehProtoFactionNum ? 0 : faction_id * MaxVehProtoFactionNum);
            VehPrototype *proto = &VehPrototypes[id];
            if (!(proto->flags & PROTO_ACTIVE) || proto->obsolete_factions & faction_bit
                || (id < MaxVehProtoFactionNum && !has_tech(proto->preq_tech, faction_id))
                || Chassis[proto->chassis_id].missile) {
                continue;
            }
            if (Weapon[proto->weapon_id].offense_rating >= offense) {
                break;
            }
            weapon_best = weapon_id; // (set before the rest of the designs are checked)
        }
    }
    // The original keeps the armor loop's counter in the slot later used for the combat factor:
    // it stays MaxArmorNum unless a combat design was offered a new part below.
    int factor = MaxArmorNum;
    int armor_best = -1;
    for (int armor_id = 0; armor_id < MaxArmorNum; armor_id++) {
        int8_t defense = Armor[armor_id].defense_rating;
        if (!has_tech(Armor[armor_id].preq_tech, faction_id) || defense <= 1
            || (armor_best >= 0 && Armor[armor_best].defense_rating > defense)) {
            continue;
        }
        for (int i = 0; i < 2 * MaxVehProtoFactionNum; i++) {
            int id = i % MaxVehProtoFactionNum
                + (i < MaxVehProtoFactionNum ? 0 : faction_id * MaxVehProtoFactionNum);
            VehPrototype *proto = &VehPrototypes[id];
            if (!(proto->flags & PROTO_ACTIVE) || proto->obsolete_factions & faction_bit
                || (id < MaxVehProtoFactionNum && !has_tech(proto->preq_tech, faction_id))) {
                continue;
            }
            if (Armor[proto->armor_id].defense_rating >= defense) {
                break;
            }
            armor_best = armor_id;
        }
    }
    if (weapon_best >= 0 || armor_best >= 0) {
        for (int i = 0; i < MaxVehProtoFactionNum; i++) {
            VehPrototype *proto = &VehPrototypes[faction_id * MaxVehProtoFactionNum + i];
            if (!(proto->flags & PROTO_ACTIVE) || proto->obsolete_factions & faction_bit
                || Chassis[proto->chassis_id].missile) {
                continue;
            }
            int chassis_id = proto->chassis_id;
            int category = (uint8_t)proto->unk_1;
            switch (proto->plan) {
              case PLAN_OFFENSIVE:
              case PLAN_AIR_SUPERIORITY:
                if (weapon_best >= 0) {
                    propose_proto(faction_id, chassis_id, weapon_best, proto->armor_id,
                        proto->ability_flags, reactor, category, NULL);
                }
                break;
              case PLAN_COMBAT:
                if (weapon_best >= 0) {
                    int armor_id = proto->armor_id;
                    if (armor_best >= 0) {
                        factor = 2 - (reactor != 1);
                        if (Armor[armor_best].defense_rating <= design_defense_limit(factor, power,
                            Weapon[weapon_best].offense_rating)) {
                            armor_id = armor_best;
                        }
                    }
                    propose_proto(faction_id, chassis_id, weapon_best, armor_id,
                        proto->ability_flags, reactor, category, NULL);
                } else {
                    factor = 2 - (reactor != 1);
                    if (Armor[armor_best].defense_rating <= design_defense_limit(factor, power,
                        Weapon[proto->weapon_id].offense_rating)) {
                        propose_proto(faction_id, chassis_id, proto->weapon_id, armor_best,
                            proto->ability_flags, reactor, category, NULL);
                    }
                }
                break;
              case PLAN_DEFENSIVE:
                if (armor_best >= 0) {
                    propose_proto(faction_id, chassis_id, proto->weapon_id, armor_best,
                        proto->ability_flags, reactor, category, NULL);
                }
                break;
              case PLAN_NAVAL_SUPERIORITY:
              {
                int weapon_id = proto->weapon_id;
                int armor_id = proto->armor_id;
                int budget = PlayersData[faction_id].ai_power + PlayersData[faction_id].ai_growth
                    + ((reactor * reactor < 5) ? 5 : reactor * reactor);
                if (weapon_best >= 0 && (int)proto_cost(chassis_id, weapon_best, armor_id,
                    proto->ability_flags, reactor) <= budget) {
                    weapon_id = weapon_best;
                }
                if (armor_best >= 0 && (int)proto_cost(chassis_id, weapon_id, armor_best,
                    proto->ability_flags, reactor) <= budget) {
                    armor_id = armor_best;
                }
                if (armor_id == armor_best || weapon_id == weapon_best) {
                    propose_proto(faction_id, chassis_id, weapon_id, armor_id,
                        proto->ability_flags, reactor, category, NULL);
                }
                break;
              }
              default:
                break;
            }
        }
    }
    // psi threats from humans this computer player is at odds with: bit 0 psi attack (psi
    // faction or the Dream Twister), bit 1 psi defense (psi faction or the Neural Amplifier)
    int psi_targets = 0;
    if (Players[faction_id].rule_psi <= 0 && !(*FactionsStatus & faction_bit)) {
        for (int i = 1; i < MaxPlayerNum; i++) {
            if (!(*FactionsStatus & (1 << i))) {
                continue;
            }
            if (!great_satan(i, false)) {
                uint32_t hostile = (PlayersData[faction_id].diff_level >= 4
                    || *GameRules & RULES_INTENSE_RIVALRY) ? DTREATY_VENDETTA : 0;
                if (!(PlayersData[faction_id].diplo_treaties[i] & (hostile | DTREATY_WANT_REVENGE))) {
                    continue;
                }
            }
            int base_id = SecretProject->dream_twister;
            if (Players[i].rule_psi > 0 || (base_id >= 0 && Bases[base_id].faction_id_current == i)) {
                psi_targets |= 1;
            }
            base_id = SecretProject->neural_amplifier;
            if (Players[i].rule_psi > 0 || (base_id >= 0 && Bases[base_id].faction_id_current == i)) {
                psi_targets |= 2;
            }
        }
    }
    BOOL human = (*FactionsStatus & faction_bit) != 0;
    PlayerData *data = &PlayersData[faction_id];
    BOOL un_charter_repealed
        = ProposalPassCount[PROP_REPEAL_UN_CHARTER] > ProposalPassCount[PROP_REINSTATE_UN_CHARTER];
    int two_abilities_tech = Rules->tech_two_spec_abilities;
    // defender, then AAA and trance garrisons
    int chassis_id = pick_chassis(faction_id, TRIAD_LAND, 1);
    if (chassis_id >= 0) {
        uint32_t abls = 0;
        int armor_id = armor_budget(faction_id, 99);
        add_abil(&abls, ABL_COMM_JAMMER, faction_id);
        if (data->ai_fight < 0 || (data->soc_effect_pending.morale < 0 && !human)
            || Players[faction_id].rule_morale < 0) {
            add_abil(&abls, ABL_TRAINED, faction_id);
        }
        if (!human && (data->enemy_best_psi_offense >= data->enemy_best_weapon_value * 2
            || psi_targets & 1)) {
            add_abil(&abls, ABL_TRANCE, faction_id);
        }
        if (has_tech(Ability[abil_index(ABL_TRANCE)].preq_tech, faction_id)
            && (!abls || has_tech(two_abilities_tech, faction_id)) && !human
            && (psi_targets || (data->planet_ecology >= 4 && data->soc_effect_pending.morale <= 0))) {
            abls |= ABL_TRANCE;
        }
        int weapon_id = weapon_budget(faction_id, 1, false);
        for (int budget = 2;; budget++) {
            int weapon_next = weapon_budget(faction_id, budget, false);
            if (weapon_next == weapon_id
                || (int)proto_cost(chassis_id, weapon_next, armor_id, abls, reactor)
                > (int)proto_cost(chassis_id, weapon_id, armor_id, abls, reactor)) {
                break;
            }
            weapon_id = weapon_next;
        }
        if (Armor[armor_id].defense_rating > 1 || *DesignSuggestState) {
            propose_proto(faction_id, chassis_id, weapon_id, armor_id, abls, reactor, 2, NULL);
        }
        if (has_tech(Ability[abil_index(ABL_TRANCE)].preq_tech, faction_id) && !(abls & ABL_TRANCE)
            && !human && ((data->planet_ecology >= 4 && data->soc_effect_pending.morale <= 0)
            || psi_targets)) {
            propose_proto(faction_id, chassis_id, weapon_id, armor_id, ABL_TRANCE, reactor, 18,
                NULL);
        }
        if (has_tech(Ability[abil_index(ABL_AAA)].preq_tech, faction_id) && !human
            && (data->unk_66 || data->unk_68)) {
            propose_proto(faction_id, chassis_id, weapon_id, armor_id, ABL_AAA, reactor, 20, NULL);
        }
    }
    // attacker, with drop pod and amphibious variants
    chassis_id = pick_chassis(faction_id, TRIAD_LAND, 1);
    if (chassis_id >= 0) {
        uint32_t abls = 0;
        int power2 = power * 2;
        int budget = 1;
        int cost_limit = data->ai_power + 5;
        if (2 / power2 <= cost_limit) {
            int size = 2;
            do {
                budget++;
                size++;
            } while ((size + 1) / 2 * size / power2 <= cost_limit);
        }
        int weapon_id = weapon_budget(faction_id, human ? 99 : budget, false);
        int offense = Weapon[weapon_id].offense_rating;
        int armor_cost = design_defense_limit(factor, power, offense);
        if (armor_cost <= budget / 2) {
            armor_cost = budget / 2;
        }
        armor_cost = (armor_cost < 1) ? 1 : ((armor_cost > 99) ? 99 : armor_cost);
        int armor_id = armor_budget(faction_id, armor_cost);
        int defense = Armor[armor_id].defense_rating;
        int value = (offense + 1) * defense;
        if (value / power2 <= cost_limit) {
            do {
                offense++;
                value += defense;
            } while (value / power2 <= cost_limit);
        }
        weapon_id = weapon_budget(faction_id, offense, false);
        if (!human && data->unk_66 > data->unk_105 * 2 && data->unk_66 > 1
            && data->unk_66 > data->unk_106) {
            add_abil(&abls, ABL_AAA, faction_id);
        }
        if (un_charter_repealed && data->ai_fight + data->ai_power > 0 && !human) {
            add_abil(&abls, ABL_NERVE_GAS, faction_id);
        }
        if (threat && threat > 1 - data->ai_fight && data->ai_fight >= 0 && !human) {
            add_abil(&abls, ABL_NERVE_GAS, faction_id);
        }
        if (!human && ((data->enemy_best_psi_defense >= data->best_armor_value * 3 / 2
            && data->enemy_best_psi_defense > Weapon[weapon_id].offense_rating / 2)
            || psi_targets)) {
            add_abil(&abls, ABL_EMPATHIC, faction_id);
        }
        if ((int)data->current_num_bases >= 8 && (int)data->ranking >= 6
            && (data->ai_wealth || data->ai_tech)) {
            add_abil(&abls, ABL_CLEAN_REACTOR, faction_id);
        }
        if (Players[faction_id].rule_morale < 0) {
            add_abil(&abls, ABL_TRAINED, faction_id);
        }
        if (Armor[armor_id].defense_rating > 1 && !abls && data->ai_tech > 0
            && data->enemy_best_land_speed > 1
            && data->enemy_best_weapon_value >= data->best_armor_value * 2) {
            add_abil(&abls, ABL_COMM_JAMMER, faction_id);
        }
        if (!abls && (data->ai_fight < 0 || (data->soc_effect_pending.morale < 0 && !human))) {
            add_abil(&abls, ABL_TRAINED, faction_id);
        }
        if (has_tech(Ability[abil_index(ABL_ANTIGRAV_STRUTS)].preq_tech, faction_id)
            && (!abls || has_tech(two_abilities_tech, faction_id))) {
            abls |= ABL_ANTIGRAV_STRUTS;
        }
        offense = Weapon[weapon_id].offense_rating;
        if ((offense > 1 && Armor[armor_id].defense_rating > 1) || offense > 2
            || *DesignSuggestState) {
            propose_proto(faction_id, chassis_id, weapon_id, armor_id, abls, reactor, 3, NULL);
            abls &= ~ABL_ANTIGRAV_STRUTS;
            if (!has_tech(two_abilities_tech, faction_id)) {
                abls = 0;
            }
            if (has_tech(Ability[abil_index(ABL_DROP_POD)].preq_tech, faction_id)) {
                propose_proto(faction_id, chassis_id, weapon_id, armor_id, abls | ABL_DROP_POD,
                    reactor, 11, NULL);
            }
            if (has_tech(Ability[abil_index(ABL_AMPHIBIOUS)].preq_tech, faction_id)) {
                propose_proto(faction_id, chassis_id, weapon_id, armor_id, abls | ABL_AMPHIBIOUS,
                    reactor, 12, NULL);
            }
        }
    }
    // artillery
    int weapon_top = weapon_budget(faction_id, 99, false);
    if (has_tech(Ability[abil_index(ABL_ARTILLERY)].preq_tech, faction_id)
        && (chassis_id = pick_chassis(faction_id, TRIAD_LAND, 1)) >= 0) {
        uint32_t abls = 0;
        int armor_id = armor_budget(faction_id, 1);
        for (int i = 0; i < 100; i++) {
            BOOL changed = false;
            if (Chassis[chassis_id].speed == 1) {
                int chassis_fast = pick_chassis(faction_id, TRIAD_LAND, 2);
                if (chassis_fast >= 0 && chassis_fast != chassis_id) {
                    if (((int)proto_cost(chassis_fast, weapon_top, armor_id, abls, reactor)
                        <= reactor + 5 && (int)proto_cost(chassis_fast, weapon_top, armor_id, abls,
                        reactor) <= (int)proto_cost(chassis_id, weapon_top, armor_id, abls, reactor)
                        + (faction_id & 1) + 1)
                        || (int)proto_cost(chassis_fast, weapon_top, armor_id, abls, reactor)
                        <= (int)proto_cost(chassis_id, weapon_top, armor_id, abls, reactor)) {
                        chassis_id = chassis_fast;
                        changed = true;
                    }
                }
            }
            int armor_new = armor_budget(faction_id, Armor[armor_id].defense_rating + 1);
            if (Armor[armor_new].defense_rating > Armor[armor_id].defense_rating
                && (int)proto_cost(chassis_id, weapon_top, armor_new, abls, reactor)
                <= (int)proto_cost(chassis_id, weapon_top, armor_id, abls, reactor)) {
                armor_id = armor_new;
            } else if (!changed) {
                break;
            }
        }
        // (the original also prices the last armor tried here and drops the result)
        if (!has_tech(Ability[abil_index(ABL_ARTILLERY)].preq_tech, faction_id)
            || has_tech(two_abilities_tech, faction_id)) {
            add_abil(&abls, ABL_BLINK_DISPLACER, faction_id);
            add_abil(&abls, ABL_ANTIGRAV_STRUTS, faction_id);
            add_abil(&abls, ABL_DEEP_RADAR, faction_id);
        }
        if (has_tech(Ability[abil_index(ABL_ARTILLERY)].preq_tech, faction_id)) {
            abls |= ABL_ARTILLERY;
        }
        if (Weapon[weapon_top].offense_rating >= Armor[armor_id].defense_rating * 3
            || has_tech(Ability[abil_index(ABL_ARTILLERY)].preq_tech, faction_id)
            || *DesignSuggestState) {
            propose_proto(faction_id, chassis_id, weapon_top, armor_id, abls, reactor, 1, NULL);
        }
    }
    // fast attacker
    chassis_id = pick_chassis(faction_id, TRIAD_LAND, 3);
    if (chassis_id < 0) {
        chassis_id = pick_chassis(faction_id, TRIAD_LAND, 2);
    }
    if (chassis_id >= 0) {
        uint32_t abls = 0;
        int armor_id = armor_budget(faction_id, 1);
        int cost_limit = data->ai_power + reactor * 2 + data->ai_fight + 3;
        int budget = 0;
        int size = 2;
        do {
            size += 2;
            budget++;
        } while (size / power <= cost_limit);
        if (human) {
            budget = 99;
        }
        int weapon_id = weapon_budget(faction_id, budget, false);
        for (int i = 0; i < 100; i++) {
            int offense = Weapon[weapon_id].offense_rating;
            int defense = Armor[armor_id].defense_rating;
            if (defense >= offense - 1) {
                break;
            }
            int armor_cost = (offense / 2 > defense + 1) ? offense / 2 : defense + 1;
            int armor_new = armor_budget(faction_id, armor_cost);
            if (Armor[armor_new].defense_rating <= Armor[armor_id].defense_rating
                || (int)proto_cost(chassis_id, weapon_id, armor_new, abls, reactor)
                > data->ai_power + data->ai_fight + reactor + 3) {
                break;
            }
            armor_id = armor_new;
        }
        if (threat && threat > 3 - data->ai_fight && data->ai_fight >= 0 && !human) {
            add_abil(&abls, ABL_NERVE_GAS, faction_id);
        }
        if (un_charter_repealed && data->ai_power + data->ai_fight > 0 && !human) {
            add_abil(&abls, ABL_NERVE_GAS, faction_id);
        }
        if (!human && psi_targets & 2) {
            add_abil(&abls, ABL_EMPATHIC, faction_id);
        }
        if (Players[faction_id].rule_morale < 0
            || (data->ai_fight > 0 && data->soc_effect_pending.industry >= 0)
            || (data->soc_effect_pending.morale < 0 && !human)) {
            add_abil(&abls, ABL_TRAINED, faction_id);
        }
        if (has_tech(Ability[abil_index(ABL_ANTIGRAV_STRUTS)].preq_tech, faction_id)
            && (!abls || has_tech(two_abilities_tech, faction_id))) {
            abls |= ABL_ANTIGRAV_STRUTS;
        }
        propose_proto(faction_id, chassis_id, weapon_id, armor_id, abls, reactor, 4, NULL);
    }
    // scout
    uint32_t abls = 0;
    chassis_id = pick_chassis(faction_id, TRIAD_LAND, 3);
    if (chassis_id < 0) {
        chassis_id = pick_chassis(faction_id, TRIAD_LAND, 2);
        if (chassis_id < 0) {
            chassis_id = pick_chassis(faction_id, TRIAD_LAND, 1);
            add_abil(&abls, ABL_ANTIGRAV_STRUTS, faction_id);
        }
    }
    if (chassis_id >= 0 && (Weapon[weapon_budget(faction_id, 2, false)].offense_rating >= 2
        || *DesignSuggestState || (has_tech(Ability[abil_index(ABL_EMPATHIC)].preq_tech,
        faction_id) && !human))) {
        int weapon_id = weapon_budget(faction_id, 1, false);
        int armor_id = armor_budget(faction_id, 1);
        if (has_tech(Ability[abil_index(ABL_EMPATHIC)].preq_tech, faction_id) && !human) {
            abls |= ABL_EMPATHIC;
            if (has_tech(two_abilities_tech, faction_id)) {
                if (has_tech(Ability[abil_index(ABL_ANTIGRAV_STRUTS)].preq_tech, faction_id)) {
                    abls |= ABL_ANTIGRAV_STRUTS;
                } else if (has_tech(Ability[abil_index(ABL_TRAINED)].preq_tech, faction_id)) {
                    abls |= ABL_TRAINED;
                }
            }
        } else {
            if (!abls) {
                add_abil(&abls, ABL_DEEP_RADAR, faction_id);
                if (!abls) {
                    add_abil(&abls, ABL_CLOAKED, faction_id);
                }
            }
            if (has_tech(Ability[abil_index(ABL_ANTIGRAV_STRUTS)].preq_tech, faction_id)) {
                if (has_tech(two_abilities_tech, faction_id)) {
                    abls |= ABL_ANTIGRAV_STRUTS;
                } else {
                    abls = ABL_ANTIGRAV_STRUTS;
                }
            }
        }
        propose_proto(faction_id, chassis_id, weapon_id, armor_id, abls, reactor,
            (abls & ABL_TRANCE) ? 19 : 10, NULL);
    }
    // warship, then a defensive ship (computer players only)
    chassis_id = pick_chassis(faction_id, TRIAD_SEA, 0);
    if (chassis_id >= 0) {
        abls = 0;
        if (data->unk_66 > data->unk_106 && data->unk_66 > data->unk_105 && !human) {
            add_abil(&abls, ABL_AAA, faction_id);
        }
        add_abil(&abls, ABL_DEEP_RADAR, faction_id);
        add_abil(&abls, ABL_AAA, faction_id);
        if (psi_targets & 1 && has_tech(Ability[abil_index(ABL_TRANCE)].preq_tech, faction_id)
            && (!abls || has_tech(two_abilities_tech, faction_id))) {
            abls |= ABL_TRANCE;
        }
        if (has_tech(Ability[abil_index(ABL_AAA)].preq_tech, faction_id) && !(abls & ABL_AAA)
            && (int)bit_count(abls) < 2 && (!abls || has_tech(two_abilities_tech, faction_id))) {
            abls |= ABL_AAA;
        }
        if (has_tech(Ability[abil_index(ABL_DEEP_RADAR)].preq_tech, faction_id)
            && !(abls & ABL_DEEP_RADAR) && (int)bit_count(abls) < 2
            && (!abls || has_tech(two_abilities_tech, faction_id))) {
            abls |= ABL_DEEP_RADAR;
        }
        int weapon_id = weapon_budget(faction_id, 1, false);
        int armor_id = armor_budget(faction_id, 1);
        int budget_base = data->ai_power + data->ai_growth;
        for (int i = 0; i < 100; i++) {
            BOOL changed = false;
            int weapon_new = weapon_id;
            for (int j = 1; j < 4 && weapon_new == weapon_id; j++) {
                weapon_new = weapon_budget(faction_id, Weapon[weapon_id].offense_rating + j, false);
            }
            int margin = reactor * 2 - reactor / 2;
            if (margin < 5) {
                margin = 5;
            }
            if (weapon_new != weapon_id
                && (int)proto_cost(chassis_id, weapon_new, armor_id, 0, reactor) <= budget_base
                + margin) {
                weapon_id = weapon_new;
                changed = true;
            }
            int armor_new = armor_id;
            for (int j = 1; j < 4 && armor_new == armor_id; j++) {
                armor_new = armor_budget(faction_id, Armor[armor_id].defense_rating + j);
            }
            if (armor_new != armor_id
                && Armor[armor_new].defense_rating > Armor[armor_id].defense_rating
                && (int)proto_cost(chassis_id, weapon_id, armor_new, abls, reactor) <= budget_base
                + margin) {
                armor_id = armor_new;
            } else if (!changed) {
                break;
            }
        }
        int weapon_ship = human ? weapon_budget(faction_id, 99, false) : weapon_id;
        if (Weapon[weapon_ship].offense_rating >= 1) {
            propose_proto(faction_id, chassis_id, weapon_ship, armor_id, abls, reactor, 6, NULL);
        }
        if (!human) {
            weapon_id = weapon_budget(faction_id, 1, false);
            armor_id = armor_budget(faction_id, 99);
            abls = 0;
            add_abil(&abls, ABL_AAA, faction_id);
            if (has_tech(Ability[abil_index(ABL_DEEP_RADAR)].preq_tech, faction_id)
                && has_tech(two_abilities_tech, faction_id)) {
                abls |= ABL_DEEP_RADAR;
            }
            if (Armor[armor_id].defense_rating > 1) {
                propose_proto(faction_id, chassis_id, weapon_id, armor_id, abls, reactor, 21,
                    NULL);
            }
        }
    }
    // sea transport, sea former and sea colony pod
    chassis_id = pick_chassis(faction_id, TRIAD_SEA, 0);
    if (chassis_id >= 0) {
        abls = 0;
        int armor_id = armor_budget(faction_id, 1);
        int weapon_id = weapon_budget(faction_id, -7, false);
        if (data->unk_66 > data->unk_106 && data->unk_66 > data->unk_105) {
            armor_id = armor_budget(faction_id, 99);
        }
        propose_proto(faction_id, chassis_id, weapon_id, armor_id, abls, reactor, 7, NULL);
        abls = 0;
        if (chassis_id != CHSI_FOIL && has_tech(Chassis[CHSI_FOIL].preq_tech, faction_id)) {
            chassis_id = CHSI_FOIL;
        }
        armor_id = armor_budget(faction_id, 1);
        weapon_id = weapon_budget(faction_id, -9, false);
        add_abil(&abls, ABL_SUPER_TERRAFORMER, faction_id);
        add_abil(&abls, ABL_FUNGICIDAL, faction_id);
        if (abls == ABL_SUPER_TERRAFORMER
            && has_tech(Ability[abil_index(ABL_FUNGICIDAL)].preq_tech, faction_id)
            && has_tech(two_abilities_tech, faction_id)) {
            abls |= ABL_FUNGICIDAL;
        }
        if (weapon_id >= 0) {
            propose_proto(faction_id, chassis_id, weapon_id, armor_id, abls, reactor, 15, NULL);
        }
        abls = 0;
        weapon_id = weapon_budget(faction_id, -8, false);
        if (weapon_id >= 0) {
            propose_proto(faction_id, chassis_id, weapon_id, armor_id, abls, reactor, 16, NULL);
        }
    }
    // bomber, then an interceptor
    chassis_id = pick_chassis(faction_id, TRIAD_AIR, 0);
    if (chassis_id >= 0) {
        int weapon_id = weapon_budget(faction_id, 99, false);
        int armor_id = armor_budget(faction_id, 1);
        abls = 0;
        if ((int)data->ranking >= 6 && (data->ai_wealth || data->ai_tech)
            && (!data->ai_power || data->ai_fight < 1)) {
            add_abil(&abls, ABL_CLEAN_REACTOR, faction_id);
        }
        if (psi_targets & 2) {
            add_abil(&abls, ABL_EMPATHIC, faction_id);
        }
        if (threat && threat > 5 - data->ai_fight && data->ai_fight >= 0 && !human) {
            add_abil(&abls, ABL_NERVE_GAS, faction_id);
        }
        if (un_charter_repealed && data->ai_fight + data->ai_power > 0 && !human) {
            add_abil(&abls, ABL_NERVE_GAS, faction_id);
        }
        if (has_tech(Ability[abil_index(ABL_DEEP_RADAR)].preq_tech, faction_id)
            && (!abls || has_tech(two_abilities_tech, faction_id))) {
            abls |= ABL_DEEP_RADAR;
        }
        propose_proto(faction_id, chassis_id, weapon_id, armor_id, abls, reactor, 8, NULL);
        weapon_id = weapon_budget(faction_id, 99, false);
        abls = ABL_AIR_SUPERIORITY;
        if (has_tech(Ability[abil_index(ABL_DEEP_RADAR)].preq_tech, faction_id)
            && has_tech(two_abilities_tech, faction_id)) {
            abls |= ABL_DEEP_RADAR;
        } else if (psi_targets & 1
            && has_tech(Ability[abil_index(ABL_TRANCE)].preq_tech, faction_id)
            && has_tech(two_abilities_tech, faction_id)) {
            abls |= ABL_TRANCE;
        }
        propose_proto(faction_id, chassis_id, weapon_id, armor_id, abls, reactor, 9, NULL);
    }
    // short range air attacker
    chassis_id = pick_chassis(faction_id, TRIAD_AIR, -2);
    if (chassis_id >= 0) {
        int weapon_id = weapon_budget(faction_id, 99, false);
        int armor_id = armor_budget(faction_id, 1);
        abls = 0;
        if ((int)data->ranking >= 6 && (data->ai_wealth || data->ai_tech)
            && (!data->ai_power || data->ai_fight < 1)) {
            add_abil(&abls, ABL_CLEAN_REACTOR, faction_id);
        }
        if (psi_targets & 2) {
            add_abil(&abls, ABL_EMPATHIC, faction_id);
        }
        if (threat && threat > 4 - data->ai_fight && data->ai_fight >= 0 && !human) {
            add_abil(&abls, ABL_NERVE_GAS, faction_id);
        }
        if (un_charter_repealed && data->ai_fight + data->ai_power > 0 && !human) {
            add_abil(&abls, ABL_NERVE_GAS, faction_id);
        }
        if (has_tech(Ability[abil_index(ABL_DEEP_RADAR)].preq_tech, faction_id)
            && (!abls || has_tech(two_abilities_tech, faction_id))) {
            abls |= ABL_DEEP_RADAR;
        }
        propose_proto(faction_id, chassis_id, weapon_id, armor_id, abls, reactor, 17, NULL);
    }
    // missiles and a planet buster
    chassis_id = pick_chassis(faction_id, TRIAD_AIR, -1);
    if (chassis_id >= 0) {
        int armor_id = armor_budget(faction_id, 1);
        abls = 0;
        if (has_tech(Weapon[WPN_CONVENTIONAL_PAYLOAD].preq_tech, faction_id)) {
            propose_proto(faction_id, chassis_id, WPN_CONVENTIONAL_PAYLOAD, armor_id, abls,
                reactor, 13, NULL);
        }
        if (has_tech(Weapon[WPN_TECTONIC_PAYLOAD].preq_tech, faction_id)) {
            propose_proto(faction_id, chassis_id, WPN_TECTONIC_PAYLOAD, armor_id, abls, reactor,
                22, NULL);
        }
        if (has_tech(Weapon[WPN_FUNGAL_PAYLOAD].preq_tech, faction_id)) {
            propose_proto(faction_id, chassis_id, WPN_FUNGAL_PAYLOAD, armor_id, abls, reactor, 23,
                NULL);
        }
        for (int weapon_id = 0; weapon_id < MaxWeaponNum; weapon_id++) {
            if (has_tech(Weapon[weapon_id].preq_tech, faction_id)
                && Weapon[weapon_id].offense_rating >= 99) {
                propose_proto(faction_id, chassis_id, weapon_id, armor_id, abls, reactor, 14,
                    NULL);
                break;
            }
        }
    }
    // former: fungicidal only until a better reactor or super former is available
    abls = 0;
    if (reactor <= 1 && !has_tech(Ability[abil_index(ABL_SUPER_TERRAFORMER)].preq_tech,
        faction_id)) {
        if (!has_tech(Ability[abil_index(ABL_FUNGICIDAL)].preq_tech, faction_id)) {
            return;
        }
        chassis_id = pick_chassis(faction_id, TRIAD_LAND, 1);
        if (chassis_id < 0) {
            return;
        }
        int armor_id = armor_budget(faction_id, 1);
        propose_proto(faction_id, chassis_id, WPN_TERRAFORMING_UNIT, armor_id, ABL_FUNGICIDAL,
            reactor, -1, NULL);
        return;
    }
    chassis_id = pick_chassis(faction_id, TRIAD_LAND, 2);
    if (chassis_id < 0) {
        return;
    }
    int armor_id = armor_budget(faction_id, 1);
    if (has_tech(Ability[abil_index(ABL_SUPER_TERRAFORMER)].preq_tech, faction_id)) {
        abls |= ABL_SUPER_TERRAFORMER;
    }
    if (has_tech(Ability[abil_index(ABL_FUNGICIDAL)].preq_tech, faction_id)
        && (!abls || has_tech(two_abilities_tech, faction_id))) {
        abls |= ABL_FUNGICIDAL;
    }
    propose_proto(faction_id, chassis_id, WPN_TERRAFORMING_UNIT, armor_id, abls, reactor, -1,
        NULL);
}

/*
Purpose: Check whether an ability may be fitted to a design with the given chassis, weapon and
         armor: the ability's allowed unit flags against the chassis triad, the weapon's combat/
         terraform/noncombat role, probe team restrictions, the SMACX-only abilities, psi
         restrictions, transport-only and slow-unit restrictions. Missile chassis take no abilities.
Original Offset: 00584B50
Return Value: Is the ability allowed on this design? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch2.log)
*/
BOOL __cdecl abil_cond(int ability_id, int chassis_id, int weapon_id, int armor_id) {
    if (Chassis[chassis_id].missile) {
        return false;
    }
    uint32_t flags = Ability[ability_id].flags;
    switch (Chassis[chassis_id].triad) {
    case TRIAD_LAND:
        if (!(flags & AFLAG_ALLOWED_LAND_UNIT)) {
            return false;
        }
        break;
    case TRIAD_SEA:
        if (!(flags & AFLAG_ALLOWED_SEA_UNIT)) {
            return false;
        }
        break;
    case TRIAD_AIR:
        if (!(flags & AFLAG_ALLOWED_AIR_UNIT)) {
            return false;
        }
        break;
    default:
        break;
    }
    int8_t offense = Weapon[weapon_id].offense_rating;
    uint8_t mode = Weapon[weapon_id].mode;
    if (offense) {
        if (!(flags & AFLAG_ALLOWED_COMBAT_UNIT)) {
            return false;
        }
    } else if (mode == WPN_MODE_TERRAFORMER) {
        if (!(flags & AFLAG_ALLOWED_TERRAFORM_UNIT)) {
            return false;
        }
    } else if (!(flags & AFLAG_ALLOWED_NONCOMBAT_UNIT)) {
        return false;
    }
    if (mode == WPN_MODE_INFOWAR) {
        if (flags & AFLAG_NOT_ALLOWED_PROBE_TEAM) {
            return false;
        }
    } else if (flags & AFLAG_ONLY_PROBE_TEAM) {
        return false;
    }
    if (!*ExpansionEnabled) {
        uint32_t ability = 1 << ability_id;
        if (ability == ABL_SOPORIFIC_GAS || ability == ABL_DISSOCIATIVE_WAVE
            || ability == ABL_MARINE_DETACHMENT || ability == ABL_FUEL_NANOCELLS
            || ability == ABL_ALGO_ENHANCEMENT) {
            return false;
        }
    }
    if ((offense < 0 || Armor[armor_id].defense_rating < 0) && (flags & AFLAG_NOT_ALLOWED_PSI_UNIT)) {
        return false;
    }
    if ((flags & AFLAG_TRANSPORT_ONLY_UNIT) && mode != WPN_MODE_TRANSPORT) {
        return false;
    }
    if ((flags & AFLAG_NOT_ALLOWED_FAST_UNIT) && Chassis[chassis_id].speed > 1) {
        return false;
    }
    return true;
}

/*
Purpose: Calculate the movement penalty/cost.
Original Offset: 00593510
Return Value: Movement cost
Status: Complete. Deliberately differs from the original (upstream's fix, kept by Kiousu, 2026-10-18):
        a Sealurk entering fungus on the ocean shelf pays the normal sea cost, as Isle of the Deep
        does, instead of triple; see "hex_cost Sealurks, 2026-10-18" in docs/regression-testing.md.
*/
int __cdecl hex_cost(int proto_id, int faction_id, uint32_t x_src, uint32_t y_src, uint32_t x_dst,
                     uint32_t y_dst, BOOL toggle) {
    uint32_t bit_dst = bit_at(x_dst, y_dst);
    if (is_ocean(x_dst, y_dst)) {
        if (bit_dst & BIT_FUNGUS && altitude_at(x_dst, y_dst) == ALT_BIT_OCEAN_SHELF
            && get_proto_triad(proto_id) == TRIAD_SEA
            && proto_id != BSC_SEALURK // Bug fix
            && proto_id != BSC_ISLE_OF_THE_DEEP && !has_project(SP_XENOEMPATYH_DOME, faction_id)) {
            return Rules->move_rate_roads * 3;
        }
        return Rules->move_rate_roads;
    }
    if (is_ocean(x_src, y_src)) {
        return Rules->move_rate_roads;
    }
    if (proto_id >= 0 && get_proto_triad(proto_id) != TRIAD_LAND) {
        return Rules->move_rate_roads;
    }
    // Land only conditions
    uint32_t bit_src = bit_at(x_src, y_src);
    if (bit_src & (BIT_MAGTUBE | BIT_BASE_IN_TILE) && bit_dst & (BIT_MAGTUBE | BIT_BASE_IN_TILE)
        && faction_id) {
        return 0;
    }
    if ((bit_src & (BIT_ROAD | BIT_BASE_IN_TILE) || bit_src & BIT_FUNGUS && faction_id > 0
        && has_project(SP_XENOEMPATYH_DOME, faction_id)) && bit_dst & (BIT_ROAD | BIT_BASE_IN_TILE)
        && faction_id) {
        return 1;
    }
    if (faction_id >= 0 && (has_project(SP_XENOEMPATYH_DOME, faction_id) || !faction_id
        || proto_id == BSC_MIND_WORMS || proto_id == BSC_SPORE_LAUNCHER) && bit_dst & BIT_FUNGUS) {
        return 1;
    }
    if (bit_src & BIT_RIVER && bit_dst & BIT_RIVER && x_dist(x_src, x_dst) == 1
        && abs((int)y_src - (int)y_dst) == 1 && faction_id) {
        return 1;
    }
    if (VehPrototypes[proto_id].chassis_id == CHSI_HOVERTANK
        || has_abil(proto_id, ABL_ANTIGRAV_STRUTS)) {
        return Rules->move_rate_roads;
    }
    uint32_t cost = Rules->move_rate_roads;
    if (rocky_at(x_dst, y_dst) > TERRAIN_BIT_ROLLING && !toggle) {
        cost += Rules->move_rate_roads;
    }
    if (bit_dst & BIT_FOREST && !toggle) {
        cost += Rules->move_rate_roads;
    }
    if (faction_id && bit_dst & BIT_FUNGUS && (proto_id >= MaxVehProtoFactionNum
        || get_proto_offense_rating(proto_id) >= 0)) {
        uint8_t plan = VehPrototypes[proto_id].plan;
        if (plan != PLAN_TERRAFORMING && plan != PLAN_ALIEN_ARTIFACT
            && PlayersData[faction_id].soc_effect_active.planet <= 0) {
            return cost + Rules->move_rate_roads * 2;
        }
        uint32_t speed = speed_proto(proto_id);
        if (cost <= speed) {
            return speed;
        }
    }
    return cost;
}

/*
Purpose: Relocate an existing unit to the specified tile.
Original Offset: 005A59B0
Return Value: n/a
Status: Complete
*/
void __cdecl veh_put(uint32_t veh_id, int x, int y) {
    veh_drop(veh_lift(veh_id), x, y);
}

/*
Purpose: Get the current health of the specified unit factoring in damage.
Original Offset: 005A59E0
Return Value: Unit health
Status: Complete
*/
uint32_t __cdecl veh_health(uint32_t veh_id) {
    int proto_id = Vehs[veh_id].proto_id;
    int health = VehPrototypes[proto_id].plan != PLAN_ALIEN_ARTIFACT
        ? range(VehPrototypes[proto_id].reactor_id, 1, 100) * 10 : 1;
    return range(health - Vehs[veh_id].dmg_incurred, 0, 9999);
}

/*
Purpose: Calculates cost of the prototype based on various factors. Optimized logic flow from 
         the original without any differences to the final calculation.
Original Offset: 005A5A60
Return Value: Cost of prototype
Status: Complete
*/
uint32_t __cdecl proto_cost(uint32_t chassis_id, uint32_t weapon_id, uint32_t armor_id, 
                            uint32_t ability, uint32_t reactor_id) {
    uint8_t weap_cost = Weapon[weapon_id].cost;
    // PB check: moved to start vs after 1st triad checks in original > no difference in logic
    if (Chassis[chassis_id].missile && Weapon[weapon_id].offense_rating >= 99) {
        return weap_cost;
    }
    uint8_t triad = Chassis[chassis_id].triad;
    uint32_t armor_cost = Armor[armor_id].cost;
    uint32_t speed_cost = Chassis[chassis_id].cost;
    int abil_modifier = 0;
    int flags_modifier = 0;
    if (ability) {
        for (int i = 0; i < MaxAbilityNum; i++) {
            if ((1 << i) & ability) {
                if (abil_modifier) {
                    abil_modifier++; // Increased cost with more than one ability
                }
                int factor = Ability[i].cost_factor;
                if (factor > 0) { // 1+ = Straight Cost; 25% increase per unit of cost
                    abil_modifier += factor;
                } else {
                    switch (factor) {
                      case 0: // None
                      default:
                        break;
                        // Increases w/ ratio of weapon to armor: 0, 1, or 2. Rounded DOWN.
                        // Never higher than 2.
                      case -1:
                        // fixed potential crash: will never trigger in vanilla but could with mods
                        if (armor_cost) {
                            abil_modifier += range(weap_cost / armor_cost, 0, 2);
                        }
                        break;
                      case -2: // Increases w/ weapon value
                        abil_modifier += weap_cost - 1;
                        break;
                      case -3: // Increases w/ armor value
                        abil_modifier += armor_cost - 1;
                        break;
                      case -4: // Increases w/ speed value
                        abil_modifier += speed_cost - 1;
                        break;
                      case -5: // Increases w/ weapon+armor value
                        abil_modifier += weap_cost + armor_cost - 2;
                        break;
                      case -6: // Increases w/ weapon+speed value
                        abil_modifier += weap_cost + speed_cost - 2;
                        break;
                      case -7: // Increases w/ armor+speed value
                        abil_modifier += armor_cost + speed_cost - 2;
                        break;
                    }
                }
                // 010000000000 - Cost increased for land units; Deep Radar
                // Shifted flag check into main ability loop rather than its
                // own loop at 1st triad checks
                if (Ability[i].flags & AFLAG_COST_INC_LAND_UNIT && triad == TRIAD_LAND) {
                    // separate variable keeps logic same (two abilities, both with cost 0,
                    // one with cost increase flag will trigger above "if (abil_modifier)" if
                    // this is directly abil_modifier++)
                    flags_modifier++;
                }
            }
        }
        abil_modifier += flags_modifier; // adding here keeps logic consistent after optimization
    }
    if (triad == TRIAD_SEA) {
        armor_cost /= 2;
        speed_cost += reactor_id;
    } else if (triad == TRIAD_AIR) {
        if (armor_cost > 1) {
            armor_cost *= reactor_id * 2;
        }
        speed_cost += reactor_id * 2;
    } // TRIAD_LAND ability flag check moved into ability loop above
    uint32_t combat_mod = armor_cost / 2 + 1;
    if (combat_mod < weap_cost) { // which ever is greater
        combat_mod = weap_cost;
    }
    int proto_mod;
    // shifted this check to top vs at end > no difference in logic
    if (combat_mod == 1 && armor_cost == 1 && speed_cost == 1 && reactor_id == RECT_FISSION) {
        proto_mod = 1;
    } else {
        // (2 << n) == 2^(n + 1) ; (2 << n) / 2 == 2 ^ n;
        // will crash if reactor_id is 0xFF > divide by zero; not putting in error checking
        // since this is unlikely even with modding however noting for future
        proto_mod = ((speed_cost + armor_cost) * combat_mod + ((2 << reactor_id) / 2))
            / (2 << reactor_id);
        if (speed_cost == 1) {
            proto_mod = (proto_mod / 2) + 1;
        }
        if (weap_cost > 1 && Armor[armor_id].cost > 1) {
            proto_mod++;
            // moved inside nested check vs separate triad checks below > no difference in logic
            if (triad == TRIAD_LAND && speed_cost > 1) {
                proto_mod++;
            }
        }
        // excludes sea probes
        if (triad == TRIAD_SEA && Weapon[weapon_id].mode != WPN_MODE_INFOWAR) {
            proto_mod = (proto_mod + 1) / 2;
        } else if (triad == TRIAD_AIR) {
            proto_mod /= (Weapon[weapon_id].mode > WPN_MODE_MISSILE) ? 2 : 4; // Non-combat : Combat
        }
        int reactor_mod = (reactor_id * 3 + 1) / 2;
        if (proto_mod < reactor_mod) { // which ever is greater
            proto_mod = reactor_mod;
        }
    }
    return (proto_mod * (abil_modifier + 4) + 2) / 4;
}

/*
Purpose: Calculates the base cost of the specified prototype.
Original Offset: 005A5D00
Return Value: Base cost of the prototype
Status: Complete
*/
uint32_t __cdecl base_cost(uint32_t proto_id) {
    return proto_cost(VehPrototypes[proto_id].chassis_id, VehPrototypes[proto_id].weapon_id,
        VehPrototypes[proto_id].armor_id, 0, VehPrototypes[proto_id].reactor_id);
}

/*
Purpose: Create a new prototype. Sets initial values for everything except veh_name and preq_tech.
Original Offset: 005A5D40
Return Value: n/a
Status: Complete
*/
void __cdecl make_proto(int proto_id, uint32_t chassis_id, uint32_t weapon_id, uint32_t armor_id, 
                        uint32_t ability, uint32_t reactor_id) {
    int unk_local_1 = 0; // TODO: Identify
    if (proto_id >= MaxVehProtoFactionNum) {
        BOOL cond1 = false;
        BOOL cond2 = false;
        BOOL cond3 = false;
        int proto_id_loop;
        for (int i = 0; i < 128; i++) {
            proto_id_loop = i % MaxVehProtoFactionNum;
            if ((i % MaxVehProtoFactionNum) != i) {
                proto_id_loop += proto_id - proto_id % MaxVehProtoFactionNum;
            }
            int flags_check = VehPrototypes[proto_id_loop].flags;
            if (flags_check & PROTO_ACTIVE) {
                if ((proto_id_loop <= MaxVehProtoFactionNum &&
                    has_tech(VehPrototypes[proto_id_loop].preq_tech, 
                        proto_id / MaxVehProtoFactionNum))
                    || (proto_id_loop > MaxVehProtoFactionNum
                        && (flags_check & PROTO_TYPED_COMPLETE))) {
                    uint32_t loop_weapon_id = VehPrototypes[proto_id_loop].weapon_id;
                    if (loop_weapon_id == weapon_id) {
                        cond1 = true;
                    }
                    uint32_t loop_armor_id = VehPrototypes[proto_id_loop].armor_id;
                    if (loop_armor_id == armor_id) {
                        cond2 = true;
                    }
                    uint32_t loop_chassis_id = VehPrototypes[proto_id_loop].chassis_id;
                    if (loop_chassis_id == chassis_id) {
                        cond3 = true;
                    }
                    int off_rating = Weapon[weapon_id].offense_rating;
                    if (off_rating > 0 && Weapon[loop_weapon_id].offense_rating >= off_rating) {
                        cond1 = true;
                    }
                    int def_rating = Armor[armor_id].defense_rating;
                    if (def_rating > 0 && Armor[loop_armor_id].defense_rating >= def_rating) {
                        cond2 = true;
                    }
                    if (Chassis[chassis_id].triad == Chassis[loop_chassis_id].triad
                        && Chassis[loop_chassis_id].speed >= Chassis[chassis_id].speed) {
                        cond3 = true;
                    }
                }
            }
        }
        if (cond1 && cond2 && cond3) {
            unk_local_1 = (proto_id_loop >= MaxVehProtoFactionNum
                && VehPrototypes[proto_id_loop].flags & 0x10) ? 3 : 1;
        }
    }
    VehPrototypes[proto_id].chassis_id = (uint8_t)chassis_id;
    VehPrototypes[proto_id].weapon_id = (uint8_t)weapon_id;
    VehPrototypes[proto_id].armor_id = (uint8_t)armor_id;
    VehPrototypes[proto_id].ability_flags = ability;
    VehPrototypes[proto_id].reactor_id = (uint8_t)reactor_id;
    VehPrototypes[proto_id].cost = (uint8_t)proto_cost(chassis_id, weapon_id, armor_id, ability,
        reactor_id);
    VehPrototypes[proto_id].carry_capacity = (Weapon[weapon_id].mode == WPN_MODE_TRANSPORT) 
        ? (uint8_t)transport_val(chassis_id, ability, reactor_id) : 0;
    // set plan & unk1
    if (Chassis[chassis_id].missile) {
        if (Weapon[weapon_id].offense_rating < 99) { // non-PB missiles
            if (weapon_id == WPN_TECTONIC_PAYLOAD) {
                VehPrototypes[proto_id].plan = PLAN_TECTONIC_MISSILE;
                VehPrototypes[proto_id].unk_1 = 22;
            } else if (weapon_id == WPN_FUNGAL_PAYLOAD) {
                VehPrototypes[proto_id].plan = PLAN_FUNGAL_MISSILE;
                VehPrototypes[proto_id].unk_1 = 23;
            } else { // Conventional Payload
                VehPrototypes[proto_id].plan = PLAN_OFFENSIVE;
                VehPrototypes[proto_id].unk_1 = 13;
            }
        } else {
            VehPrototypes[proto_id].plan = PLAN_PLANET_BUSTER;
            VehPrototypes[proto_id].unk_1 = 14;
        }
    } else if (Weapon[weapon_id].mode >= WPN_MODE_TRANSPORT) { // Non-combat
        VehPrototypes[proto_id].plan = Weapon[weapon_id].mode;
        VehPrototypes[proto_id].unk_1 = Weapon[weapon_id].mode + 0x20;
    } else if (Chassis[chassis_id].triad == TRIAD_SEA) { // combat sea
        VehPrototypes[proto_id].plan = PLAN_NAVAL_SUPERIORITY;
        VehPrototypes[proto_id].unk_1 = 6; // same value as plan
    } else if (Chassis[chassis_id].triad == TRIAD_AIR) { // combat air
        if (has_abil(proto_id, ABL_AIR_SUPERIORITY)) {
            VehPrototypes[proto_id].plan = PLAN_AIR_SUPERIORITY;
            VehPrototypes[proto_id].unk_1 = 9;
        } else {
            VehPrototypes[proto_id].plan = PLAN_OFFENSIVE;
            VehPrototypes[proto_id].unk_1 = 8;
        }
    } else { // TRIAD_LAND combat unit
        VehPrototypes[proto_id].plan = PLAN_OFFENSIVE;
        if (Armor[armor_id].defense_rating > 1) {
            VehPrototypes[proto_id].plan = PLAN_DEFENSIVE;
        }
        if (Weapon[weapon_id].offense_rating >= Armor[armor_id].defense_rating
            && VehPrototypes[proto_id].plan == PLAN_DEFENSIVE) {
            VehPrototypes[proto_id].plan = PLAN_COMBAT;
        }
        if (Chassis[chassis_id].speed > 1
            && Weapon[weapon_id].offense_rating > Armor[armor_id].defense_rating) {
            VehPrototypes[proto_id].plan = PLAN_OFFENSIVE;
        }
        if (ability & (ABL_ARTILLERY | ABL_DROP_POD | ABL_AMPHIBIOUS)) {
            VehPrototypes[proto_id].plan = PLAN_OFFENSIVE;
        }
        VehPrototypes[proto_id].unk_1 = 3;
        if (Armor[armor_id].defense_rating > Weapon[weapon_id].offense_rating) {
            VehPrototypes[proto_id].unk_1 = 2;
        }
        if (Weapon[weapon_id].offense_rating > Armor[armor_id].defense_rating * 2) {
            VehPrototypes[proto_id].unk_1 = 1;
        }
        // changed from '>= 2' since '>= 3' will always overwrite it
        if (Chassis[chassis_id].speed == 2) {
            VehPrototypes[proto_id].unk_1 = 4;
        } else if (Chassis[chassis_id].speed >= 3) { // added else, if speed <= 2 would always skip
            VehPrototypes[proto_id].unk_1 = 5;
        }
        if (Weapon[weapon_id].offense_rating == 1 && Armor[armor_id].defense_rating == 1) {
            if (Chassis[chassis_id].speed > 1) {
                VehPrototypes[proto_id].plan = PLAN_RECONNAISANCE;
            }
            if (has_abil(proto_id, ABL_POLICE_2X)) {
                VehPrototypes[proto_id].plan = PLAN_DEFENSIVE;
            }
            VehPrototypes[proto_id].unk_1 = 10;
        }
    }
    // set final values
    VehPrototypes[proto_id].obsolete_factions = 0;
    VehPrototypes[proto_id].combat_factions = (proto_id >= MaxVehProtoFactionNum) ?
        1 << (proto_id / MaxVehProtoFactionNum) : -1;
    VehPrototypes[proto_id].flags = unk_local_1 
        ? ((unk_local_1 & 2) ? 0x115 : 0x105) : PROTO_ACTIVE;
    VehPrototypes[proto_id].icon_offset = -1;
}

/*
Purpose: Fill ProtoSortList with the 64 basic and the faction's 64 custom prototypes, sorted for
         the design workshop: by plan and triad, then by weapon, speed and armor (best first).
         Designs that are retired or not yet available sort to the end. ProtoSortCount counts
         the available designs (retired custom designs included).
Original Offset: 005A6270
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch3.log)
*/
void __cdecl proto_sort(uint32_t faction_id) {
    int values[2 * MaxVehProtoFactionNum];
    *ProtoSortCount = 0;
    for (int i = 0; i < 2 * MaxVehProtoFactionNum; i++) {
        int proto_id = (i < MaxVehProtoFactionNum ? 0 : faction_id * MaxVehProtoFactionNum)
            + (i % MaxVehProtoFactionNum);
        ProtoSortList[i] = proto_id;
        int value;
        if (!(VehPrototypes[proto_id].flags & PROTO_ACTIVE)
            || (proto_id < MaxVehProtoFactionNum
                && !has_tech(VehPrototypes[proto_id].preq_tech, faction_id))) {
            if (proto_id < MaxVehProtoFactionNum) {
                value = 999999999;
            } else {
                value = 888888888;
                (*ProtoSortCount)++;
            }
        } else {
            uint32_t plan = VehPrototypes[proto_id].plan;
            uint32_t triad = Chassis[VehPrototypes[proto_id].chassis_id].triad;
            int weapon_value = weap_val(proto_id, faction_id);
            uint32_t speed = Chassis[VehPrototypes[proto_id].chassis_id].speed;
            int armor_value = arm_val(VehPrototypes[proto_id].armor_id, faction_id);
            value = (triad + plan * 16) * 0x40000 + (32 - weapon_value) * 0x200
                + (8 - speed) * 0x40 + (8 - armor_value);
            (*ProtoSortCount)++;
        }
        values[i] = value;
    }
    sort(2 * MaxVehProtoFactionNum, ProtoSortList, values);
}

/*
Purpose: Fill ProtoSortList with the 64 basic and the faction's 64 custom prototypes, sorted by id
         in groups: available designs the faction still uses first, then its obsolete custom
         designs that still have units, other obsolete custom designs, retired custom designs,
         obsolete basic designs and finally unavailable basic designs. Basic designs need their
         prerequisite tech, a non-psi weapon and must not be the Spore Launcher. ProtoSortCount
         counts everything but the unavailable basic designs.
Original Offset: 005A63D0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch3.log)
*/
void __cdecl proto_sort_2(uint32_t faction_id) {
    int values[2 * MaxVehProtoFactionNum];
    *ProtoSortCount = 0;
    for (int i = 0; i < 2 * MaxVehProtoFactionNum; i++) {
        int proto_id = (i < MaxVehProtoFactionNum ? 0 : faction_id * MaxVehProtoFactionNum)
            + (i % MaxVehProtoFactionNum);
        ProtoSortList[i] = proto_id;
        BOOL avail = (VehPrototypes[proto_id].flags & PROTO_ACTIVE)
            && (proto_id >= MaxVehProtoFactionNum
                || (Weapon[VehPrototypes[proto_id].weapon_id].offense_rating >= 0
                    && proto_id != BSC_SPORE_LAUNCHER
                    && has_tech(VehPrototypes[proto_id].preq_tech, faction_id)));
        int value;
        if (!avail) {
            if (proto_id < MaxVehProtoFactionNum) {
                value = 999999999;
            } else {
                value = proto_id + 0x400;
                (*ProtoSortCount)++;
            }
        } else if (!(VehPrototypes[proto_id].obsolete_factions & (1 << faction_id))) {
            value = proto_id;
            (*ProtoSortCount)++;
        } else if (proto_id < MaxVehProtoFactionNum) {
            value = proto_id + 0x2000;
            (*ProtoSortCount)++;
        } else {
            value = proto_id + (PlayersData[faction_id].proto_id_active[proto_id] ? 0x800 : 0x1000);
            (*ProtoSortCount)++;
        }
        values[i] = value;
    }
    sort(2 * MaxVehProtoFactionNum, ProtoSortList, values);
}

/*
Purpose: Search for the 1st prototype (basic + faction specific) that matches the specified plan.
Original Offset: 005AED50
Return Value: Prototype id or -1 if not found
Status: Complete
*/
int __cdecl get_plan(uint32_t faction_id, uint32_t plan) {
    for (int i = 0; i < 128; i++) {
        uint32_t proto_id = (i < MaxVehProtoFactionNum) ? i
            : (faction_id * MaxVehProtoFactionNum) + i - MaxVehProtoFactionNum;
        if (VehPrototypes[proto_id].flags & PROTO_ACTIVE
            && !(VehPrototypes[proto_id].obsolete_factions & (1 << faction_id))
            && (proto_id >= MaxVehProtoFactionNum
                || has_tech(VehPrototypes[proto_id].preq_tech, faction_id))
            && VehPrototypes[proto_id].plan == plan) {
            return proto_id;
        }
    }
    return -1;
}

/*
Purpose: Mark the unit's tile and its whole stack as spotted by the faction. The first time the
         faction at the controls sees the tile (outside omniscient view and without a revealed
         map) the tile is flagged for the map redraw.
Original Offset: 005B58E0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch2.log)
*/
void __cdecl spot_stack(int veh_id, int faction_id) {
    if (veh_id >= 0) {
        int x = Vehs[veh_id].x;
        int y = Vehs[veh_id].y;
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
    for (int i = veh_top(veh_id); i >= 0; i = Vehs[i].next_veh_id_stack) {
        Vehs[i].visibility |= (1 << faction_id);
        if (faction_id) {
            Vehs[i].flags &= ~(VFLAG_LURKER | VFLAG_INVISIBLE);
        }
    }
}

/*
Purpose: Clear the visibility bitfield of every unit in the specified unit's stack.
Original Offset: 005B5A70
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-veh-helpers.log)
*/
void __cdecl unspot_stack(int veh_id) {
    for (int i = veh_top(veh_id); i >= 0; i = Vehs[i].next_veh_id_stack) {
        Vehs[i].visibility = 0;
    }
}

// Mark a tile as seen by a faction; the faction at the controls also gets the first-sight redraw
// flag (see spot_stack()). The tile must be on the map.
static void spot_tile(int x, int y, uint32_t faction_id) {
    Map *tile = map_loc(x, y);
    uint8_t bit = (uint8_t)(1 << faction_id);
    if (faction_id == (uint32_t)*FactionIDCurrent && !(tile->visibility & bit)
        && !(*GameState & STATE_OMNISCIENT_VIEW)
        && !(PlayersData[faction_id].flags & PFLAG_MAP_REVEALED)) {
        tile->bit2 |= BIT2_UNK_400000;
        *UnkBitfield1 |= 1;
    }
    tile->visibility |= bit;
    synch_bit(x, y, faction_id);
}

/*
Purpose: Reveal a location to a faction: the tile itself, and whatever sits there (a base, with
         its population intel, or a stack of units, which also lose their lurker/invisible
         flags for a non-native spotter).
Original Offset: 005B5AD0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch8.log)
*/
void __cdecl spot_loc(int x, int y, uint32_t faction_id) {
    int lat = *MapLatitudeBounds, lon = *MapLongitudeBounds;
    if (y >= 0 && y < lat && x >= 0 && x < lon) {
        spot_tile(x, y, faction_id);
    }
    int base_id = base_at(x, y);
    if (base_id >= 0) {
        Bases[base_id].visibility |= (uint8_t)(1 << faction_id);
        Bases[base_id].faction_pop_size_intel[faction_id] = Bases[base_id].population_size;
        int x_base = Bases[base_id].x, y_base = Bases[base_id].y;
        if (y_base >= 0 && y_base < lat && x_base >= 0 && x_base < lon) {
            spot_tile(x_base, y_base, faction_id);
        }
        return;
    }
    int veh_id = veh_at(x, y);
    if (veh_id < 0) {
        return;
    }
    int x_veh = Vehs[veh_id].x, y_veh = Vehs[veh_id].y;
    if (y_veh >= 0 && y_veh < lat && x_veh >= 0 && x_veh < lon) {
        spot_tile(x_veh, y_veh, faction_id);
    }
    int i = veh_id;
    while (Vehs[i].prev_veh_id_stack >= 0) {
        i = Vehs[i].prev_veh_id_stack;
    }
    for (; i >= 0; i = Vehs[i].next_veh_id_stack) {
        Vehs[i].visibility |= (uint8_t)(1 << faction_id);
        if (faction_id) {
            Vehs[i].flags &= ~(VFLAG_LURKER | VFLAG_INVISIBLE);
        }
    }
}

/*
Purpose: Determine if the specified unit wants to wake up based on certain conditions and 
         preferences. Optional parameter for spotted veh_id (-1 to skip).
Original Offset: 005B5EA0
Return Value: Does unit want to wake? true/false
Status: Complete
*/
BOOL __cdecl want_to_wake(uint32_t faction_id, uint32_t veh_id, int spotted_veh_id) {
    int base_faction_id;
    uint32_t triad = get_triad(veh_id);
    uint32_t veh_faction_id = Vehs[veh_id].faction_id;
    if (triad == TRIAD_AIR && Vehs[veh_id].terraforming_turns 
        && Vehs[veh_id].order == ORDER_MOVE_TO
        && (base_faction_id = base_who(Vehs[veh_id].waypoint_x[0], Vehs[veh_id].waypoint_y[0]),
            base_faction_id >= 0)) {
        if (((uint32_t)base_faction_id == veh_faction_id) 
            || has_treaty(veh_faction_id, base_faction_id, DTREATY_PACT)) {
            return false;
        }
    }
    BOOL wants_to_wake;
    if (*IsMultiplayerNet) { // restructured to be more efficient with same logic
        if (has_treaty(veh_faction_id, faction_id, DTREATY_PACT)) {
            wants_to_wake = false;
        } else if (has_treaty(veh_faction_id, faction_id, DTREATY_TREATY)) {
            wants_to_wake = true;
        } else if (has_treaty(veh_faction_id, faction_id, DTREATY_TRUCE)) {
            wants_to_wake = true;
        } else {
            wants_to_wake = true;
        }
    } else {
        if (has_treaty(veh_faction_id, faction_id, DTREATY_PACT)) {
            wants_to_wake = *GamePreferences & PREF_AUTO_END_MOVE_SPOT_VEH_PACT;
        } else if (has_treaty(veh_faction_id, faction_id, DTREATY_TREATY)) {
            wants_to_wake = *GamePreferences & PREF_AUTO_END_MOVE_SPOT_VEH_TREATY;
        } else if (has_treaty(veh_faction_id, faction_id, DTREATY_TRUCE)) {
            wants_to_wake = *GamePreferences & PREF_AUTO_END_MOVE_SPOT_VEH_TRUCE;
        } else {
            wants_to_wake = *GamePreferences & PREF_AUTO_END_MOVE_SPOT_VEH_WAR;
        }
        if (!(*GamePreferences & PREF_AUTO_DONT_END_MOVE_DIFF_TRIAD)) {
            return wants_to_wake;
        }
    }
    if (spotted_veh_id >= 0) { // removed a check if veh_id is also >= 0 (always unsigned)
        uint32_t triad_spotted = get_triad(spotted_veh_id);
        if (triad != triad_spotted && ((Vehs[veh_id].state & (VSTATE_UNK_1000000 | VSTATE_UNK_200))
            != (VSTATE_UNK_1000000 | VSTATE_UNK_200) || triad_spotted == TRIAD_LAND)) {
            wants_to_wake = false;
        }
    }
    return wants_to_wake;
}

/*
Purpose: Wake up units meeting specific conditions from the stack where specified unit is located.
Original Offset: 005B6060
Return Value: n/a
Status: Complete
*/
void __cdecl wake_stack(int veh_id) {
    for (int i = veh_top(veh_id); i >= 0; i = Vehs[i].next_veh_id_stack) {
        int veh_id_way_point;
        int x;
        int y;
        if (Vehs[i].order == ORDER_SENTRY_BOARD 
            && (get_triad(i) || (x = Vehs[i].x, y = Vehs[i].y, !is_ocean(x, y)
            && ((veh_id_way_point = Vehs[i].waypoint_x[0], veh_id_way_point < 0)
                || get_triad(veh_id_way_point) != TRIAD_AIR || base_who(x, y) >= 0
                || bit_at(x, y) & BIT_AIRBASE)))) {
            Vehs[i].order = ORDER_NONE;
            Vehs[i].state &= ~VSTATE_EXPLORE;
        }
    }
}

/*
Purpose: Move a stack of units in the same stack as the specified unit to a destination.
Original Offset: 005B8AF0
Return Value: n/a
Status: Complete
*/
void __cdecl stack_put(int veh_id, int x, int y) {
    int next_veh_id = veh_top(veh_id);
    int veh_id_loop;
    if (next_veh_id >= 0) {
        do {
            veh_id_loop = Vehs[next_veh_id].next_veh_id_stack;
            veh_put(next_veh_id, x, y);
            next_veh_id = veh_id_loop;
        } while (veh_id_loop >= 0);
    }
}

/*
Purpose: Sort a stack of units with the transports moved to the top.
Original Offset: 005B8B60
Return Value: n/a
Status: Complete
*/
void __cdecl stack_sort(uint32_t veh_id) {
    int16_t x = Vehs[veh_id].x;
    int16_t y = Vehs[veh_id].y;
    int next_veh_id = veh_top(veh_id);
    int veh_id_put = -1;
    int veh_id_loop;
    if (next_veh_id >= 0) {
        do {
            veh_id_loop = Vehs[next_veh_id].next_veh_id_stack; // removed redundant < 0 check
            if (veh_cargo(next_veh_id) || has_abil(Vehs[next_veh_id].proto_id, ABL_CARRIER)) {
                veh_id_put = next_veh_id;
                veh_put(next_veh_id, -3, -3);
            }
            next_veh_id = veh_id_loop;
        } while (veh_id_loop >= 0);
        stack_put(veh_id_put, x, y);
    }
}

/*
Purpose: Sort a stack of units with colony pods at the top followed by combat/offensive units.
Original Offset: 005B8C90
Return Value: n/a
Status: Complete
*/
void __cdecl stack_sort_2(uint32_t veh_id) {
    int16_t x = Vehs[veh_id].x;
    int16_t y = Vehs[veh_id].y;
    int next_veh_id = veh_top(veh_id);
    int veh_id_put = -1;
    int veh_id_loop;
    if (next_veh_id >= 0) {
        do {
            veh_id_loop = Vehs[next_veh_id].next_veh_id_stack; // removed redundant < 0 check
            if (VehPrototypes[Vehs[next_veh_id].proto_id].plan == PLAN_COLONIZATION) { // Colony Pod
                veh_id_put = next_veh_id;
                veh_put(next_veh_id, -3, -3);
            }
            next_veh_id = veh_id_loop;
        } while (veh_id_loop >= 0);
    }
    next_veh_id = veh_at(x, y);
    if (next_veh_id >= 0) {
        do {
            veh_id_loop = Vehs[next_veh_id].next_veh_id_stack; // removed redundant < 0 check
            if (VehPrototypes[Vehs[next_veh_id].proto_id].plan <= PLAN_COMBAT) {
                veh_id_put = next_veh_id;
                veh_put(next_veh_id, -3, -3);
            }
            next_veh_id = veh_id_loop;
        } while (veh_id_loop >= 0);
    }
    stack_put(veh_id_put, x, y);
}

/*
Purpose: Refresh and fix the stack. Used by DirectPlay multiplayer only.
Original Offset: 005B8E10
Return Value: Either the parameter unit id or unit id of the stack top; Return is checked if >= 0
Status: Complete
*/
int __cdecl stack_fix(int veh_id) {
    if (veh_id < 0 || !*IsMultiplayerNet
        || (Vehs[veh_id].next_veh_id_stack < 0 && Vehs[veh_id].prev_veh_id_stack < 0)) {
        return veh_id; // invalid veh_id, not DirectPlay MP or no stack
    }
    for (int i = 0; i < *VehCurrentCount; i++) {
        // Bug fix: Original would compare against the exact same source coordinates (both veh_id
        // instead of one being iterator) likely causing a performance hit.
        if (Vehs[veh_id].x == Vehs[i].x && Vehs[veh_id].y == Vehs[i].y) {
            veh_promote(i);
            stack_sort(veh_id);
        }
    }
    return veh_top(veh_id);
}

/*
Purpose: Load a transport's stack. Units of the transport's faction on the same tile are taken
         aboard (land units into cargo space, aircraft onto a carrier): in a first pass those
         already assigned to this transport (sentried with it as their carrier), in a second pass
         anyone suitable, where an AI unit needs the right regional plan, a human unit must be
         sentried waiting for a carrier, and at sea everything qualifies. With mode 0 the units
         are only marked (VSTATE_UNK_1) and the free space is returned; otherwise they are lifted
         and dropped into the transport. Mode 2 or more at a base, and any mode on a unit that
         carries nothing, just re-drops the unit itself.
Original Offset: 005B8EE0
Return Value: Free cargo space (mode 0) or the veh_drop() result
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch11.log)
*/
uint32_t __cdecl stack_veh(uint32_t veh_id, int mode) {
    if ((int)veh_id < 0) {
        return mode ? veh_id : 0;
    }
    Veh *veh = &Vehs[veh_id];
    BOOL transport = veh_cargo(veh_id) != 0 || has_abil(veh->proto_id, ABL_CARRIER);
    if (!transport) {
        if (!mode) {
            return 0;
        }
    } else {
        if (*IsMultiplayerNet) {
            stack_fix(veh_id);
        }
        if (!mode) {
            int i = veh_id;
            while (Vehs[i].prev_veh_id_stack >= 0) {
                i = Vehs[i].prev_veh_id_stack;
            }
            for (; i >= 0; i = Vehs[i].next_veh_id_stack) {
                Vehs[i].state &= ~VSTATE_UNK_1;
            }
        } else {
            Map *here = map_loc(veh->x, veh->y);
            if (mode > 1 && (here->bit & BIT_BASE_IN_TILE) && (here->val2 & 0xF) < 8) {
                veh_lift(veh_id);
                return veh_drop(veh_id, -2, -2);
            }
            stack_sort(veh_id);
        }
        int lat = *MapLatitudeBounds, lon = *MapLongitudeBounds;
        BOOL at_sea = false;
        if (veh->y >= 0 && veh->y < lat && veh->x >= 0 && veh->x < lon) {
            Map *tile = map_loc(veh->x, veh->y);
            at_sea = (tile->climate & 0xE0) < ALT_BIT_SHORE_LINE
                && !((tile->bit & BIT_BASE_IN_TILE) && (tile->val2 & 0xF) < 8);
        }
        uint8_t faction_id = veh->faction_id;
        uint8_t triad = Chassis[VehPrototypes[veh->proto_id].chassis_id].triad;
        int space = veh_cargo(veh_id);
        int carrier_space = 0;
        if (has_abil(veh->proto_id, ABL_CARRIER)) {
            carrier_space = (int)veh_cargo(veh_id) < 1 ? 1 : veh_cargo(veh_id);
        }
        if (!faction_id) {
            space = 99;
        }
        int passes = mode < 2 ? 2 : 1;
        for (int pass = 0; pass < passes; pass++) {
            int i = veh_id;
            if (!mode) {
                while (Vehs[i].prev_veh_id_stack >= 0) {
                    i = Vehs[i].prev_veh_id_stack;
                }
            }
            while (i >= 0 && (space || carrier_space)) {
                int next = Vehs[i].next_veh_id_stack;
                Veh *other = &Vehs[i];
                uint8_t triad_other = Chassis[VehPrototypes[other->proto_id].chassis_id].triad;
                BOOL loadable = other->faction_id == faction_id
                    && ((space && triad_other == TRIAD_LAND && !veh_cargo(i))
                        || (carrier_space && triad_other == TRIAD_AIR
                            && !has_abil(other->proto_id, ABL_CARRIER)));
                if (!loadable) {
                    i = next;
                    continue;
                }
                BOOL take = false;
                if (!pass) {
                    take = other->order == ORDER_SENTRY_BOARD && other->waypoint_x[0] == (int)veh_id;
                } else {
                    Map *here = map_loc(veh->x, veh->y);
                    BOOL no_base = !(here->bit & BIT_BASE_IN_TILE) || (here->val2 & 0xF) > 7
                        || !(here->val2 & 0xF);
                    if (triad == TRIAD_AIR && no_base && !(here->bit & BIT_AIRBASE)) {
                        i = next;
                        continue;
                    }
                    uint8_t faction_id_other = other->faction_id;
                    take = at_sea;
                    if (!is_human(faction_id_other)
                        || ((veh->state & VSTATE_UNK_200) && (other->state & VSTATE_UNK_200))) {
                        if (other->state & VSTATE_UNK_40000) {
                            if (!(other->state & VSTATE_UNK_20000)) {
                                i = next;
                                continue;
                            }
                            take = PlayersData[faction_id_other].region_base_plan[map_loc(other->x, other->y)->region] == 7;
                        } else {
                            uint8_t plan = VehPrototypes[other->proto_id].plan;
                            if (other->order != ORDER_HOLD && plan != PLAN_ALIEN_ARTIFACT
                                && plan != PLAN_TERRAFORMING) {
                                if (!at_sea && other->y >= 0 && other->y < lat && other->x >= 0
                                    && other->x < lon
                                    && PlayersData[faction_id_other].region_base_plan[map_loc(other->x, other->y)->region] == 2
                                    && plan < 8) {
                                    i = next;
                                    continue;
                                }
                                take = true;
                            }
                        }
                    } else if (other->order == ORDER_SENTRY_BOARD) {
                        take = other->waypoint_x[0] < 0;
                    }
                }
                if (take && (mode || !(other->state & VSTATE_UNK_1))) {
                    sleep(i);
                    other->waypoint_x[0] = (int16_t)veh_id;
                    if (!mode) {
                        other->state |= VSTATE_UNK_1;
                    } else {
                        veh_lift(i);
                        veh_drop(i, -2, -2);
                        other->state &= ~VSTATE_UNK_10000;
                    }
                    if (carrier_space > 0) {
                        carrier_space--;
                    }
                    if (space > 0) {
                        space--;
                    }
                }
                i = next;
            }
        }
        if (!mode) {
            if (!veh_cargo(veh_id)) {
                space = carrier_space;
            }
            return space;
        }
    }
    veh_lift(veh_id);
    return veh_drop(veh_id, -2, -2);
}

/*
Purpose: Kill every unit in the specified unit's stack. veh_kill() compacts the unit array, so the
         next id in the stack is adjusted when it comes after the one just removed.
Original Offset: 005B9510
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-veh-helpers.log)
*/
void __cdecl stack_kill(int veh_id) {
    for (int i = veh_top(veh_id); i >= 0;) {
        int next_veh_id = Vehs[i].next_veh_id_stack;
        veh_kill(i);
        if (next_veh_id > i) {
            next_veh_id--;
        }
        i = next_veh_id;
    }
}

/*
Purpose: Various unit stack related calculations based on type parameter (0-19) and conditions.
Original Offset: 005B9580
Return Value: Dependent on type parameter
Status: Complete
*/
int __cdecl stack_check(int veh_id, uint32_t type, int cond1, int cond2, int cond3) {
    int retn_val = 0;
    uint32_t plan;
    uint32_t chas;
    for (int i = veh_top(veh_id); i >= 0; i = Vehs[i].next_veh_id_stack) {
        switch (type) {
          case 0:
            if ((cond2 < 0 || Vehs[i].faction_id == cond2) && Vehs[i].proto_id == cond1) {
                retn_val++;
            }
            break;
          case 1:
            if (cond1 < 0 || Vehs[i].faction_id == cond1) {
                retn_val++;
            }
            break;
          case 2:
            if ((cond2 < 0 || Vehs[i].faction_id == cond2) 
                && VehPrototypes[Vehs[i].proto_id].plan == cond1) {
                retn_val++;
            }
            break;
          case 3:
            if ((cond2 < 0 || Vehs[i].faction_id == cond2) && get_triad(i) == cond1) {
                retn_val++;
            }
            break;
          case 4:
            if (cond1 < 0 || Vehs[i].faction_id == cond1) {
                retn_val += get_offense_rating(i);
            }
            break;
          case 5:
            if (cond1 < 0 || Vehs[i].faction_id == cond1) {
                retn_val += get_defense_rating(i);
            }
            break;
          case 6:
            if ((cond2 < 0 || Vehs[i].faction_id == cond2) && has_abil(Vehs[i].proto_id, cond1)) {
                retn_val++;
            }
            break;
          case 7:
            if (cond1 < 0 || Vehs[i].faction_id == cond1) {
                retn_val += VehPrototypes[Vehs[i].proto_id].cost;
            }
            break;
          case 8:
            if (cond1 < 0 || Vehs[i].faction_id == cond1) {
                uint32_t triad = get_triad(i);
                if (triad == TRIAD_LAND) {
                    retn_val--;
                } else if (triad == TRIAD_SEA) {
                    retn_val += veh_cargo(i);
                }
            }
            break;
          case 9:
            if ((cond2 < 0 || Vehs[i].faction_id == cond2) && Vehs[i].order == cond1) {
                retn_val++;
            }
            break;
          case 10:
            if (Vehs[i].faction_id == cond1) {
                retn_val++;
            }
            break;
          case 11:
            if ((cond3 < 0 || Vehs[i].faction_id == cond3)
                && (Vehs[i].state & cond1) == (uint32_t)cond2) {
                retn_val++;
            }
            break;
          case 12:
            if (cond1 < 0 || Vehs[i].faction_id == cond1) {
                uint32_t faction_id = Vehs[i].faction_id;
                int proto_id = Vehs[i].proto_id;
                if (arm_strat(VehPrototypes[proto_id].armor_id, faction_id) 
                    > weap_strat(VehPrototypes[proto_id].weapon_id, faction_id)) {
                    retn_val++;
                }
            }
            break;
          case 13:
            if ((cond1 < 0 || Vehs[i].faction_id == cond1) && is_missile(i)) {
                retn_val++;
            }
            break;
          case 14:
            if ((cond1 < 0 || Vehs[i].faction_id == cond1)
                && (plan = VehPrototypes[Vehs[i].proto_id].plan,
                    plan == PLAN_DEFENSIVE || plan == PLAN_COMBAT)) {
                retn_val++;
            }
            break;
          case 15:
            if ((cond1 < 0 || Vehs[i].faction_id == cond1) && can_arty(Vehs[i].proto_id, TRUE)) {
                retn_val++;
            }
            break;
          case 16:
            if ((cond1 < 0 || Vehs[i].faction_id == cond1)
                && (plan = VehPrototypes[Vehs[i].proto_id].plan, plan == PLAN_DEFENSIVE
                    || plan == PLAN_RECONNAISANCE || plan == PLAN_COMBAT)) {
                retn_val++;
            }
            break;
          case 17:
            if ((cond2 < 0 || Vehs[i].faction_id == cond2)
                && VehPrototypes[Vehs[i].proto_id].unk_1 == cond1) {
                retn_val++;
            }
            break;
          case 18:
            if ((cond1 < 0 || Vehs[i].faction_id == cond1)
                && (chas = VehPrototypes[Vehs[i].proto_id].chassis_id,
                    Chassis[chas].triad == TRIAD_AIR && Chassis[chas].range > 1)) {
                retn_val++;
            }
            break;
          case 19:
            if ((cond1 < 0 || Vehs[i].faction_id == cond1) && !get_offense_rating(i)) {
                retn_val++;
            }
            break;
          default:
            break;
        }
    }
    return retn_val;
}

/*
Purpose: Check to see whether provided faction and base can build a specific prototype. Checks are
         included to prevent SMACX specific units from being built in SMAC mode.
Original Offset: 005BA910
Return Value: Is unit available to faction/base? true/false
Status: Complete
*/
BOOL __cdecl veh_avail(uint32_t proto_id, uint32_t faction_id, int base_id) {
    if (!(VehPrototypes[proto_id].flags & PROTO_ACTIVE)
        || (VehPrototypes[proto_id].obsolete_factions & (1 << faction_id))) {
        return false;
    }
    if (proto_id < MaxVehProtoFactionNum) {
        if (!has_tech(VehPrototypes[proto_id].preq_tech, faction_id)) {
            return false;
        }
    }
    if (VehPrototypes[proto_id].plan == PLAN_COLONIZATION 
        && *GameRules & RULES_SCN_NO_COLONY_PODS) {
        return false;
    }
    if (base_id >= 0 && get_proto_triad(proto_id) == TRIAD_SEA && !is_port(base_id, false)) {
        return false;
    }
    uint8_t weap_id;
    uint32_t abil_flag;
    if (!*ExpansionEnabled && (VehPrototypes[proto_id].armor_id > ARM_PSI_DEFENSE
        || (weap_id = VehPrototypes[proto_id].weapon_id, weap_id == WPN_RESONANCE_LASER
            || weap_id == WPN_RESONANCE_BOLT || weap_id == WPN_STRING_DISRUPTOR
            || weap_id == WPN_TECTONIC_PAYLOAD || weap_id == WPN_FUNGAL_PAYLOAD)
        || (abil_flag = VehPrototypes[proto_id].ability_flags, abil_flag == ABL_SOPORIFIC_GAS
            || abil_flag == ABL_DISSOCIATIVE_WAVE || abil_flag == ABL_MARINE_DETACHMENT
            || abil_flag == ABL_FUEL_NANOCELLS || abil_flag == ABL_ALGO_ENHANCEMENT)
        || proto_id == BSC_SEALURK || proto_id == BSC_SPORE_LAUNCHER
        || proto_id == BSC_BATTLE_OGRE_MK1 || proto_id == BSC_BATTLE_OGRE_MK2
        || proto_id == BSC_BATTLE_OGRE_MK3 || proto_id == BSC_FUNGAL_TOWER
        || proto_id == BSC_UNITY_MINING_LASER)) {
        return false;
    }
    if (proto_id < MaxVehProtoFactionNum) {
        return true;
    }
    return (proto_id / MaxVehProtoFactionNum) == faction_id;
}

/*
Purpose: Determine whether a faction wants the specified prototype based on the faction's current
         prototype designs.
Original Offset: 005BE100
Return Value: Does faction want prototype? true/false
Status: Complete
*/
BOOL __cdecl wants_prototype(uint32_t proto_id, uint32_t faction_id) {
    uint32_t flags = VehPrototypes[proto_id].flags;
    if (!(flags & PROTO_ACTIVE) || !(flags & PROTO_TYPED_COMPLETE)) {
        return false;
    }
    uint32_t proto_offset = faction_id * MaxVehProtoFactionNum;
    for (uint32_t i = 0; i < MaxVehProtoFactionNum; i++) {
        uint32_t proto_id_cmp = proto_offset + i;
        uint32_t flags_cmp = VehPrototypes[proto_id_cmp].flags;
        if (flags_cmp & PROTO_ACTIVE && flags_cmp & PROTO_TYPED_COMPLETE
            && VehPrototypes[proto_id].plan == VehPrototypes[proto_id_cmp].plan) {
            uint8_t chas = VehPrototypes[proto_id].chassis_id;
            uint8_t chas_cmp = VehPrototypes[proto_id_cmp].chassis_id;
            if (Chassis[chas].triad == Chassis[chas_cmp].triad) {
                uint8_t weap_id_cmp = VehPrototypes[proto_id_cmp].weapon_id;
                uint8_t mode_cmp = Weapon[weap_id_cmp].mode;
                int8_t off_rating_cmp;
                if ((mode_cmp > WPN_MODE_MISSILE)
                    ? (mode_cmp == Weapon[VehPrototypes[proto_id].weapon_id].mode)
                    : (off_rating_cmp = Weapon[weap_id_cmp].offense_rating, off_rating_cmp < 0)
                    ? (off_rating_cmp == get_proto_offense_rating(proto_id))
                    : (off_rating_cmp >= get_proto_offense_rating(proto_id))) {
                    uint8_t arm_id_cmp = VehPrototypes[proto_id_cmp].armor_id;
                    if ((Armor[arm_id_cmp].defense_rating <= 0)
                        ? (arm_id_cmp == VehPrototypes[proto_id].armor_id)
                        : (armor_val(proto_id_cmp, faction_id) 
                            >= armor_val(proto_id, faction_id))) {
                        if (Chassis[chas_cmp].speed >= Chassis[chas].speed) {
                            return false;
                        }
                    }
                }
            }
        }
    }
    return true;
}

/*
Purpose: Check the coordinates for units and if at least one is found return the top most id.
Original Offset: 005BFE90
Return Value: Unit id or -1 if nothing is found/error
Status: Complete
*/
int __cdecl veh_at(int x, int y) {
    if (RegressPending) { // see regress.h; inert unless the trigger file exists
        regress_run();
    }
    if (on_map(x, y) && !(bit_at(x, y) & BIT_VEH_IN_TILE)) {
        return -1; // not found
    }
    for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
        if (Vehs[veh_id].x == x && Vehs[veh_id].y == y) {
            return veh_top(veh_id);
        }
    }
    if (!on_map(x, y)) {
        return -1;
    }
    if (!*VehBitError) {
        log_say("Vehicle Bit Error  (x, y)", x, y, 0);
    }
    if (*GameState & STATE_SCENARIO_EDITOR || *GameState & STATE_DEBUG_MODE || *IsMultiplayerNet) {
        if (*VehBitError) {
            return -1;
        }
        *VehBitError = true;
    }
    rebuild_vehicle_bits();
    return -1;
}

/*
Purpose: Check whether the prototype has a specific ability.
Original Offset: 005BF1F0
Return Value: Does prototype have ability? true/false
Status: Complete
*/
BOOL __cdecl has_abil(uint32_t proto_id, uint32_t ability_id) {
    if ((int)proto_id < 0) { 
        // TODO: Remove eventually, temp workaround fix for bug in base_build
        //       that incorrectly uses negative queue id
        log_say("Skipping has_abil negative prototype param", (int)proto_id, ability_id, 0);
        return false;
    }
    if (VehPrototypes[proto_id].ability_flags & ability_id) {
        return true;
    }
    uint32_t faction_id = proto_id / MaxVehProtoFactionNum;
    if (!faction_id) {
        return false; // skip basic prototypes from #UNITS
    }
    if (is_alien_faction(faction_id) && ability_id == ABL_DEEP_RADAR) {
        return true; // Caretakers + Usurpers > "Deep Radar" ability for all units
    }
    for (int i = 0; i < Players[faction_id].faction_bonus_count; i++) {
        if (Players[faction_id].faction_bonus_id[i] == RULE_FREEABIL) {
            int abil_bonus_id = Players[faction_id].faction_bonus_val1[i];
            if (has_tech(Ability[abil_bonus_id].preq_tech, faction_id) &&
                (ability_id & (1 << abil_bonus_id))) {
                // Pirates > "Marine Detachment" ability for combat sea units with Adaptive Doctrine
                return true;
            }
        }
    }
    if (VehPrototypes[proto_id].weapon_id == WPN_PROBE_TEAM && ability_id == ABL_ALGO_ENHANCEMENT
        && has_project(SP_NETHACK_TERMINUS, faction_id)) {
        return true; // All Probe Teams act as though they have the "Algorithmic Enhancement"
    }
    return false;
}

/*
Purpose: Temporarily remove the specified unit from its current square and stack in preparation for 
         another action such as interacting with the stack, moving or killing it.
Original Offset: 005BFFA0
Return Value: Unit id
Status: Complete
*/
int __cdecl veh_lift(uint32_t veh_id) {
    BOOL prev_stack_exists = false;
    int16_t prev_veh_id = Vehs[veh_id].prev_veh_id_stack;
    int16_t next_veh_id = Vehs[veh_id].next_veh_id_stack;
    if (prev_veh_id >= 0) {
        prev_stack_exists = true;
        Vehs[prev_veh_id].next_veh_id_stack = next_veh_id;
    }
    int x = Vehs[veh_id].x;
    int y = Vehs[veh_id].y;
    if (next_veh_id >= 0) {
        Vehs[next_veh_id].prev_veh_id_stack = prev_veh_id;
    } else if (!prev_stack_exists && on_map(x, y)) {
        bit_set(x, y, BIT_VEH_IN_TILE, false);
    }
    *VehDropLiftVehID = veh_id;
    *VehLiftX = x;
    *VehLiftY = y;
    Vehs[veh_id].x = -1;
    Vehs[veh_id].y = -1;
    Vehs[veh_id].next_veh_id_stack = -1;
    Vehs[veh_id].prev_veh_id_stack = -1;
    return veh_id;
}

/*
Purpose: Move the specified unit to the provided coordinates.
Original Offset: 005C0080
Return Value: Unit id (1st param), doesn't seem to be used
Status: Complete
*/
int __cdecl veh_drop(uint32_t veh_id, int x, int y) {
    int veh_id_dest = veh_at(x, y);
    Vehs[veh_id].next_veh_id_stack = (int16_t)veh_id_dest;
    Vehs[veh_id].prev_veh_id_stack = -1;
    Vehs[veh_id].x = (int16_t)x;
    Vehs[veh_id].y = (int16_t)y;
    *VehDropLiftVehID = -1;
    if (veh_id_dest < 0) {
        if (y < 0) {
            return veh_id;
        }
        if (on_map(x, y) && !(bit_at(x, y) & BIT_BASE_IN_TILE)) {
            owner_set(x, y, Vehs[veh_id].faction_id);
        }
    } else {
        Vehs[veh_id_dest].prev_veh_id_stack = (int16_t)veh_id;
    }
    if (on_map(x, y)) {
        uint32_t flags = (Vehs[veh_id].faction_id && get_triad(veh_id) != TRIAD_AIR)
            ? BIT_SUPPLY_REMOVE | BIT_VEH_IN_TILE : BIT_VEH_IN_TILE;
        bit_set(x, y, flags, true);
    }
    return veh_id;
}

/*
Purpose: Set the unit's status to sentry/board.
Original Offset: 005C01A0
Return Value: n/a
Status: Complete
*/
void __cdecl sleep(uint32_t veh_id) {
    Vehs[veh_id].order = ORDER_SENTRY_BOARD;
    Vehs[veh_id].waypoint_x[0] = -1;
    Vehs[veh_id].waypoint_y[0] = 0;
}

/*
Purpose: Move the specified unit to the bottom of the stack.
Original Offset: 005C01D0
Return Value: n/a
Status: Complete
*/
void __cdecl veh_demote(int veh_id) {
    if (veh_id >= 0) {
        int16_t next_veh_id = Vehs[veh_id].next_veh_id_stack;
        if (next_veh_id >= 0) {
            int16_t last_veh_id;
            do {
                last_veh_id = next_veh_id;
                next_veh_id = Vehs[last_veh_id].next_veh_id_stack;
            } while (next_veh_id >= 0);
            if (last_veh_id != veh_id) {
                veh_lift(veh_id);
                Vehs[last_veh_id].next_veh_id_stack = (int16_t)veh_id;
                Vehs[veh_id].prev_veh_id_stack = last_veh_id;
                Vehs[veh_id].next_veh_id_stack = -1;
                Vehs[veh_id].x = Vehs[last_veh_id].x;
                Vehs[veh_id].y = Vehs[last_veh_id].y;
            }
        }
    }
}

/*
Purpose: Move the specified unit to the top of the stack.
Original Offset: 005C0260
Return Value: n/a
Status: Complete
*/
void __cdecl veh_promote(int veh_id) {
    int veh_id_top = veh_top(veh_id);
    if (veh_id_top >= 0 && veh_id_top != veh_id) {
        veh_put(veh_id, Vehs[veh_id_top].x, Vehs[veh_id_top].y);
    }
}

/*
Purpose: Clear the specified unit.
Original Offset: 005C02D0
Return Value: n/a
Status: Complete
*/
void __cdecl veh_clear(uint32_t veh_id, int proto_id, uint32_t faction_id) {
    Vehs[veh_id].x = -4;
    Vehs[veh_id].y = -4;
    Vehs[veh_id].year_end_lurking = 0;
    Vehs[veh_id].unk_1 = 0;
    Vehs[veh_id].flags = 0;
    Vehs[veh_id].faction_id = (uint8_t)faction_id;
    Vehs[veh_id].proto_id = (int16_t)proto_id;
    Vehs[veh_id].next_veh_id_stack = -1;
    Vehs[veh_id].prev_veh_id_stack = -1;
    Vehs[veh_id].waypoint_count = 0;
    Vehs[veh_id].patrol_current_point = 0;
    Vehs[veh_id].order = ORDER_NONE;
    for (uint32_t i = 0; i < 4; i++) {
        Vehs[veh_id].waypoint_x[i] = -1;
        Vehs[veh_id].waypoint_y[i] = -1;
    }
    Vehs[veh_id].state = 0;
    Vehs[veh_id].moves_expended = 0;
    Vehs[veh_id].dmg_incurred = 0;
    Vehs[veh_id].order_auto_type = 0;
    Vehs[veh_id].terraforming_turns = 0;
    Vehs[veh_id].unk_6 = 0;
    Vehs[veh_id].move_to_ai_type = 0;
    Vehs[veh_id].visibility = 0;
    Vehs[veh_id].home_base_id = -1;
    Vehs[veh_id].morale = (uint8_t)(Players[faction_id].rule_morale + 1);
    Vehs[veh_id].unk_5 = 2;
    Vehs[veh_id].probe_action = 0;
    Vehs[veh_id].probe_sabotage_id = 0;
}

/*
Purpose: Create a unit of the specified prototype for a faction on the given tile. It is homed to a
         base of the same faction on that tile (an alien artifact is instead marked as an objective
         when artifacts are scenario objectives), placed and lets the faction see the tiles around
         it. A land combat unit or probe created on land in the region of the faction's target
         location then may be assigned to the target (VSTATE_UNK_40000, counted in unk_71 up to a
         quota set by the faction's bases in the region; unk_72 flag 1 = a defender was assigned,
         flag 2 = a probe) or, if a better attacker than defender, left out at random
         (VSTATE_UNK_20000). With a faction at vendetta, a unit farther from the target than from
         that faction's nearest base in the region is left alone.
Original Offset: 005C03D0
Return Value: Unit id or -1 if there is no room for another unit
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch27.log
        and 2026-10-17-sweep-batch27.log)
*/
int __cdecl veh_init(int proto_id, uint32_t faction_id, int x, int y) {
    int veh_id = *VehCurrentCount;
    if (veh_id >= MaxVehNum) {
        return -1;
    }
    (*VehCurrentCount)++;
    log_say("Adding Vehicle (v,who,v2)", veh_id, faction_id, proto_id);
    if (VehPrototypes[proto_id].plan < PLAN_COLONIZATION) {
        PlayersData[faction_id].total_mil_veh++;
    }
    PlayersData[faction_id].proto_id_active[proto_id]++;
    veh_clear(veh_id, proto_id, faction_id);
    int base_id = base_find(x, y);
    if (VehPrototypes[proto_id].plan == PLAN_ALIEN_ARTIFACT) {
        if (*GameState & STATE_SCN_VICT_ALL_ARTIFACTS_OBJ_UNIT) {
            Vehs[veh_id].flags |= VFLAG_IS_OBJECTIVE;
        }
    } else if (base_id >= 0 && Bases[base_id].faction_id_current == faction_id) {
        Vehs[veh_id].home_base_id = (int16_t)base_id;
        if (*BaseComputeID == base_id) {
            *BaseComputeID = -1;
        }
    }
    veh_drop(veh_id, x, y);
    spot_all(veh_id, 1);
    PlayerData *player = &PlayersData[faction_id];
    if (player->x_target < 0 || is_ocean(x, y)) {
        return veh_id;
    }
    uint8_t plan = VehPrototypes[proto_id].plan;
    if (plan > PLAN_DEFENSIVE && plan != PLAN_INFO_WARFARE) {
        return veh_id;
    }
    uint32_t region = map_loc(x, y)->region;
    if (region_at(player->x_target, player->y_target) != region) {
        return veh_id;
    }
    // first faction at vendetta (the comparison is with this faction's own base count)
    int enemy_id = -1;
    int enemy_bases = 0;
    for (int i = 1; i < MaxPlayerNum; i++) {
        if (i != (int)faction_id && player->diplo_treaties[i] & DTREATY_VENDETTA
            && player->region_total_bases[region] > enemy_bases) {
            enemy_bases = player->region_total_bases[region];
            enemy_id = i;
        }
    }
    if (enemy_id >= 0) {
        base_find(x, y, enemy_id, region, -1, -1);
        if (vector_dist(x, y, player->x_target, player->y_target) > *BaseFindDist) {
            return veh_id;
        }
    }
    int quota = player->region_total_bases[region] * 2 / 3;
    quota = (quota < 3) ? 3 : ((quota > 8) ? 8 : quota);
    if (player->region_base_plan[region] == PLAN_NAVAL_TRANSPORT) {
        int naval_quota = player->unk_70 * 2 / 0x400000;
        if (quota >= naval_quota) {
            quota = naval_quota;
        }
    }
    if (player->unk_71 - (int)bit_count(player->unk_72) >= quota) {
        return veh_id;
    }
    int weapon = weap_strat(VehPrototypes[proto_id].weapon_id, faction_id);
    int armor = arm_strat(VehPrototypes[proto_id].armor_id, faction_id);
    if (Chassis[VehPrototypes[proto_id].chassis_id].triad != TRIAD_LAND
        || has_abil(proto_id, ABL_DROP_POD)) {
        return veh_id;
    }
    if (plan == PLAN_INFO_WARFARE) {
        if (!(player->unk_72 & 2)) {
            Vehs[veh_id].state |= VSTATE_UNK_40000;
            player->unk_71++;
            player->unk_72 |= 2;
        }
        return veh_id;
    }
    if (weapon < armor) {
        if (player->unk_72 & 1 || player->region_base_plan[region] == PLAN_NAVAL_TRANSPORT) {
            return veh_id;
        }
    } else if (player->region_base_plan[region] != PLAN_NAVAL_TRANSPORT
        && !rnd(quota + 2, NULL)) { // quota is not capped here: the region plan isn't naval
        Vehs[veh_id].state |= VSTATE_UNK_20000;
        return veh_id;
    }
    Vehs[veh_id].state |= VSTATE_UNK_40000;
    player->unk_71++;
    if (weapon < armor) {
        player->unk_72 |= 1;
    }
    return veh_id;
}

// Unit ids a DLL function follows while the code it calls may kill units: veh_kill() renumbers
// them as it does its own references, -1 once killed (only give_units(), see action_give()).
static int *VehKillWatch = NULL;
static int VehKillWatchNum = 0;

/*
Purpose: Remove the specified unit from the game: the faction's unit counts, its stack and its
         slot in the unit table. The later units move down one slot, so every unit id kept
         elsewhere (stack links, the transport of sentried/boarded units, the selection globals)
         that is above it is decremented; references to the unit itself are cleared. The ids in
         VehKillWatch (none unless action_give() is handing units over) are treated the same way.
Original Offset: 005C08C0
Return Value: n/a
Status: Complete. Not in the original: the renumbering of the ids in VehKillWatch, which only
        action_give() registers (its deliberate change, chosen by Kiousu 2026-10-18).
*/
void __cdecl veh_kill(int veh_id) {
    if (veh_id < 0) {
        return;
    }
    if (*IsMultiplayerNet) {
        DeletionList_add_deletion(DeletionListVar, 0, veh_id);
    }
    log_say("Killing vehicle (v,who,v2)", veh_id, Vehs[veh_id].faction_id, Vehs[veh_id].proto_id);
    int x = Vehs[veh_id].x;
    int home_base_id = Vehs[veh_id].home_base_id;
    int y = Vehs[veh_id].y;
    int proto_id = Vehs[veh_id].proto_id;
    uint32_t faction_id = Vehs[veh_id].faction_id;
    if (VehPrototypes[proto_id].plan < PLAN_COLONIZATION) {
        PlayersData[faction_id].total_mil_veh--;
    }
    PlayersData[faction_id].proto_id_active[proto_id]--;
    veh_lift(veh_id);
    if (veh_id < *VehCurrentCount - 1) {
        memmove(&Vehs[veh_id], &Vehs[veh_id + 1], (*VehCurrentCount - veh_id - 1) * sizeof(Veh));
    }
    if (veh_id < MaxVehNum) {
        (*VehCurrentCount)--;
    }
    for (int i = 0; i < *VehCurrentCount; i++) {
        if (Vehs[i].prev_veh_id_stack > veh_id) {
            Vehs[i].prev_veh_id_stack--;
        }
        if (Vehs[i].next_veh_id_stack > veh_id) {
            Vehs[i].next_veh_id_stack--;
        }
        if (Vehs[i].order == ORDER_SENTRY_BOARD) {
            if (Vehs[i].waypoint_x[0] == veh_id) {
                Vehs[i].order = ORDER_NONE;
            } else if (Vehs[i].waypoint_x[0] > veh_id) {
                Vehs[i].waypoint_x[0]--;
            }
        }
    }
    int *const refs[4] = { MapWinVehIDSelected, MapWinVehIDOther, StatusWinVehIDA, StatusWinVehIDB };
    for (int i = 0; i < 4; i++) {
        if (*refs[i] == veh_id) {
            *refs[i] = -1;
        } else if (*refs[i] > veh_id) {
            (*refs[i])--;
        }
    }
    for (int i = 0; i < VehKillWatchNum; i++) { // not in the original, see VehKillWatch
        if (VehKillWatch[i] == veh_id) {
            VehKillWatch[i] = -1;
        } else if (VehKillWatch[i] > veh_id) {
            VehKillWatch[i]--;
        }
    }
    // a current unit id equal to the killed one moves down too (0 is never decremented)
    if (*VehIDCurrent && *VehIDCurrent >= veh_id) {
        (*VehIDCurrent)--;
    }
    if (*IsMultiplayerNet && *NetVehIDRef && *NetVehIDRef != MapWinVehIDSelected
        && *NetVehIDRef != VehIDCurrent && *NetVehIDRef != MapWinVehIDOther
        && **NetVehIDRef >= veh_id) {
        (**NetVehIDRef)--;
    }
    BaseWin_check_loc(BaseWinVar, 0, x, y, 0);
    BaseWin_check_base(BaseWinVar, 0, home_base_id);
}

/*
Purpose: Kill the specified unit. A unit with cargo at sea or off the map takes its stack down with
         it (at x -2 only stack_kill() runs and the function ends there); on land the cargo stays
         on the tile. Then the base window checks the home base and, unless VehBattleUnkTgl is
         set, the tile is redrawn: with the radius around it when the unit belonged to the player
         at the controls and none of that faction's units is left there, or it had deep radar.
Original Offset: 005C0B00
Return Value: n/a
Status: Complete
*/
void __cdecl kill(int veh_id) {
    int x = Vehs[veh_id].x;
    int y = Vehs[veh_id].y;
    uint32_t faction_id = Vehs[veh_id].faction_id;
    BOOL deep_radar = has_abil(Vehs[veh_id].proto_id, ABL_DEEP_RADAR);
    int home_base_id = Vehs[veh_id].home_base_id;
    // the original has veh_cargo() inlined here
    if (!veh_cargo(veh_id) || (on_map(x, y) && !is_ocean(x, y))) {
        veh_kill(veh_id);
    } else {
        if (!on_map(x, y) && x == -2) {
            stack_kill(veh_id);
            return;
        }
        stack_veh(veh_id, 2);
        stack_kill(veh_id);
    }
    BaseWin_check_base(BaseWinVar, 0, home_base_id);
    if (!VehBattleUnkTgl[0] && on_map(x, y)) {
        if (faction_id == (uint32_t)*FactionIDCurrent
            && (!stack_check(veh_at(x, y), 1, faction_id, -1, -1) || deep_radar)) {
            draw_radius(x, y, 2, 2);
        } else {
            draw_tile(x, y, 2);
        }
    }
}

/*
Purpose: Find the unit closest to the specified tile, optionally restricted to one faction and
         skipping one unit id. The distance to it is stored in VehFindDist. Ties go to the unit with
         the highest id.
Original Offset: 005C0CB0
Return Value: Unit id or -1 if none found
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-veh-helpers.log)
*/
int __cdecl veh_find(int x, int y, int faction_id, int skip_veh_id) {
    *VehFindDist = 9999;
    int dist_best = 9999;
    int veh_id_best = -1;
    for (int i = 0; i < *VehCurrentCount; i++) {
        if ((faction_id < 0 || Vehs[i].faction_id == faction_id) && i != skip_veh_id) {
            int dist = vector_dist(x, y, Vehs[i].x, Vehs[i].y);
            if (dist <= dist_best) {
                veh_id_best = i;
                dist_best = dist;
            }
        }
    }
    if (veh_id_best >= 0) {
        *VehFindDist = dist_best;
    }
    return veh_id_best;
}

/*
Purpose: Check if the prototype can perform artillery combat. The 2nd parameter determines how sea 
         units are treated.
Original Offset: 005C0DB0
Return Value: Has artillery ability? true/false
Status: Complete
*/
BOOL __cdecl can_arty(uint32_t proto_id, BOOL sea_triad_retn) {
    if ((get_proto_offense_rating(proto_id) <= 0 || get_proto_defense_rating(proto_id) < 0)
        && proto_id != BSC_SPORE_LAUNCHER) {
        return false;
    }
    uint8_t triad = get_proto_triad(proto_id);
    if (triad == TRIAD_SEA) {
        return sea_triad_retn; // cursory check shows this value always being set to true
    }
    if (triad == TRIAD_AIR) {
        return false;
    }
    return has_abil(proto_id, ABL_ARTILLERY); // TRIAD_LAND
}

/*
Purpose: Calculate a unit's morale. TODO: Determine if 2nd param is a toggle for display vs actual 
         morale.
Original Offset: 005C0E40
Return Value: Morale value
Status: Complete
*/
uint32_t __cdecl morale_veh(uint32_t veh_id, BOOL check_drone_riot, int faction_id_vs_native) {
    uint32_t faction_id = Vehs[veh_id].faction_id;
    if (!faction_id) {
        return morale_alien(veh_id, faction_id_vs_native);
    }
    int16_t proto_id = Vehs[veh_id].proto_id;
    if (VehPrototypes[proto_id].plan == PLAN_INFO_WARFARE) { // probe
        int probe_morale = range(PlayersData[faction_id].soc_effect_active.probe, 0, 3);
        probe_morale += has_project(SP_TELEPATHIC_MATRIX, faction_id) ? 2 : 0;
        for (int i = 0; i < MaxTechnologyNum; i++) {
            if (Technology[i].flags & TFLAG_IMPROVED_PROBES && has_tech(i, faction_id)) {
                probe_morale++;
            }
        }
        probe_morale += Vehs[veh_id].morale;
        return range(probe_morale, 2, 6);
    }
    int8_t offense_rating = get_proto_offense_rating(proto_id);
    if (proto_id < MaxVehProtoFactionNum && offense_rating < 0) {
        return range(Vehs[veh_id].morale, 0, 6); // Basic Psi Veh
    }
    // everything else
    int morale_modifier = range(PlayersData[faction_id].soc_effect_active.morale, -4, 4);
    if (morale_modifier <= -2) {
        morale_modifier++;
    } else if (morale_modifier >= 2) {
        morale_modifier--;
    }
    int rule_morale = Players[faction_id].rule_morale; // different from 'SOCIAL, MORALE'
    if (rule_morale < 0) { // negative effects 1st
        morale_modifier += rule_morale;
    }
    int home_base_id = Vehs[veh_id].home_base_id;
    if (home_base_id >= 0) { // home base countering negative effects
        if (has_fac_built(FAC_CHILDREN_CRECHE, home_base_id) && morale_modifier < 0) {
            morale_modifier /= 2;
        }
        if (has_fac_built(FAC_BROOD_PIT, home_base_id) && proto_id < MaxVehProtoFactionNum
            && (offense_rating < 0 || proto_id == BSC_SPORE_LAUNCHER) && morale_modifier < 0) {
            morale_modifier /= 2; // never reached due to above 'Basic Psi Veh' checks
        }
    }
    if (rule_morale > 0) {
        morale_modifier += rule_morale;
    }
    BOOL morale_flag = Players[faction_id].rule_flags & RFLAG_MORALE;
    if (morale_flag && morale_modifier < 0) {
        morale_modifier = 0;
    }
    if (check_drone_riot && home_base_id >= 0 
        && Bases[home_base_id].state & BSTATE_DRONE_RIOTS_ACTIVE && !morale_flag) {
        // bug fix: removed premature range bounding negating negative morale effects
        morale_modifier--;
    }
    return range(Vehs[veh_id].morale + morale_modifier, 0, 6);
}

/*
Purpose: Calculate the offense of the specified prototype. Optional param of the unit defending 
         against (-1 to ignore) as well as whether artillery or missile combat is being utilized.
Original Offset: 005C1150
Return Value: Prototype's offense
Status: Complete
*/
uint32_t __cdecl offense_proto(uint32_t proto_id, int veh_id_def, BOOL is_bombard) {
    uint32_t weapon_id = VehPrototypes[proto_id].weapon_id;
    if (Weapon[weapon_id].mode == WPN_MODE_INFOWAR && veh_id_def >= 0
        && VehPrototypes[Vehs[veh_id_def].proto_id].plan == PLAN_INFO_WARFARE) {
        return 16; // probe attacking another probe
    }
    // Bug fix: Vehs.proto_id with veh_id_def -1 could cause arbitrary memory read (Reactor 
    // struct) due to lack of bounds checking when comparing veh_id_def proto_id to Spore Launcher
    if ((is_bombard || (Weapon[weapon_id].offense_rating >= 0
        && (veh_id_def < 0 || get_defense_rating(veh_id_def) >= 0)))
        && (veh_id_def < 0 || Vehs[veh_id_def].proto_id != BSC_SPORE_LAUNCHER)
        && proto_id != BSC_SPORE_LAUNCHER) {
        int off_rating = get_proto_offense_rating(proto_id);
        if (off_rating < 0) {
            off_rating = -off_rating;
        }
        if (is_proto_missile(proto_id) && off_rating < 99) {
            off_rating = (off_rating * 3) / 2;
        }
        return (veh_id_def < 0) ? off_rating : off_rating * 8; // conventional
    }
    return (veh_id_def < 0) ? Rules->psi_combat_ratio_atk[TRIAD_LAND] : // PSI
        Rules->psi_combat_ratio_atk[get_triad(veh_id_def)] * 8;
}

/*
Purpose: Calculate the defense of the specified prototype. Optional param if unit is being attacked 
         (-1 to ignore) as well as whether artillery or missile combat is being utilized.
Original Offset: 005C1290
Return Value: Prototype's defense
Status: Complete
*/
uint32_t __cdecl armor_proto(uint32_t proto_id, int veh_id_atk, BOOL is_bombard) {
    if (Weapon[VehPrototypes[proto_id].weapon_id].mode == WPN_MODE_INFOWAR && veh_id_atk >= 0
        && VehPrototypes[Vehs[veh_id_atk].proto_id].plan == PLAN_INFO_WARFARE) {
        return 16; // probe defending against another probe
    }
    // Bug fix: Vehs[].proto_id with veh_id_atk -1 could cause arbitrary memory read (Reactor 
    // struct) due to lack of bounds checking when comparing veh_id_atk prototype to Spore Launcher
    if (is_bombard && (veh_id_atk < 0 || Vehs[veh_id_atk].proto_id != BSC_SPORE_LAUNCHER)
        && proto_id != BSC_SPORE_LAUNCHER || (get_proto_defense_rating(proto_id) >= 0 
            && (veh_id_atk < 0 || get_offense_rating(veh_id_atk) >= 0))) {
        uint32_t def_rating = range(get_proto_defense_rating(proto_id), 1, 9999);
        return (veh_id_atk < 0) ? def_rating : def_rating * 8; // conventional
    }
    return (veh_id_atk < 0) ? Rules->psi_combat_ratio_def[TRIAD_LAND] : // PSI
        Rules->psi_combat_ratio_def[get_proto_triad(proto_id)] * 8;
}

/*
Purpose: Calculate the speed of the specified prototype on roads.
Original Offset: 005C13B0
Return Value: Prototype's speed on roads
Status: Complete
*/
uint32_t __cdecl speed_proto(uint32_t proto_id) {
    if (proto_id == BSC_FUNGAL_TOWER) {
        return 0; // cannot move
    }
    int chas_id = VehPrototypes[proto_id].chassis_id;
    int speed_val = Chassis[chas_id].speed;
    uint8_t triad = Chassis[chas_id].triad;
    uint32_t weap_id = VehPrototypes[proto_id].weapon_id;
    if (triad == TRIAD_AIR) {
        speed_val += VehPrototypes[proto_id].reactor_id * 2;
    }
    if (Weapon[weap_id].mode == WPN_MODE_TRANSPORT) {
        speed_val--;
    }
    if (has_abil(proto_id, ABL_SLOW)) {
        speed_val--;
    }
    if (has_abil(proto_id, ABL_ANTIGRAV_STRUTS)) {
        speed_val += (triad == TRIAD_AIR) ? VehPrototypes[proto_id].reactor_id * 2 : 1;
    }
    if (triad == TRIAD_AIR) {
        if (has_abil(proto_id, ABL_FUEL_NANOCELLS)) {
            speed_val += 2;
        }
        if (has_project(SP_CLOUDBASE_ACADEMY, proto_id / MaxVehProtoFactionNum)) {
            speed_val += 2; // bonus from Aerospace Complex
        }
        if (has_abil(proto_id, ABL_AIR_SUPERIORITY)) {
            // generally -20% to -25%, in some cases higher due to lossy division rounding
            speed_val = (speed_val * 4) / 5;
        }
        if (Weapon[weap_id].mode == WPN_MODE_TRANSPORT) {
            speed_val /= 2; // 2nd penalty for air transports: -50%
        }
    }
    return range(speed_val, 1, 99) * Rules->move_rate_roads;
}

/*
Purpose: Calculate the speed of a unit on roads taking into consideration prototype speed, 
         elite morale, if the unit is damaged and other factors. The skip_morale parameter seems to 
         only be set to true for certain combat calculations in battle_fight().
Original Offset: 005C1540
Return Value: Speed
Status: Complete
*/
uint32_t __cdecl speed(uint32_t veh_id, BOOL skip_morale) {
    uint32_t proto_id = Vehs[veh_id].proto_id;
    if (proto_id == BSC_FUNGAL_TOWER) { // moved this check to top vs bottom, same logic
        return 0; // cannot move
    }
    uint32_t speed_val = speed_proto(proto_id);
    uint8_t triad = get_proto_triad(proto_id);
    if (triad == TRIAD_SEA && has_project(SP_MARITIME_CONTROL_CENTER, Vehs[veh_id].faction_id)) {
        speed_val += Rules->move_rate_roads * 2;
    }
    if (!skip_morale && morale_veh(veh_id, true, 0) == MORALE_ELITE
        && (proto_id >= MaxVehProtoFactionNum || get_proto_offense_rating(proto_id) >= 0)) {
        speed_val += Rules->move_rate_roads;
    }
    if (Vehs[veh_id].dmg_incurred && triad != TRIAD_AIR) {
        // optimized code without changes to logic
        uint32_t moves = speed_val / Rules->move_rate_roads;
        uint32_t reactor_fac;
        if (VehPrototypes[proto_id].plan == PLAN_ALIEN_ARTIFACT) {
            speed_val = reactor_fac = 1;
        } else {
            reactor_fac = range(VehPrototypes[proto_id].reactor_id, 1, 100) * 10;
            speed_val = range(reactor_fac, 1, 99);
        }
        speed_val = (moves 
            * range(reactor_fac - Vehs[veh_id].dmg_incurred, 0, 9999) + speed_val - 1) / speed_val;
        speed_val = range(speed_val, (triad == TRIAD_SEA) ? 2 : 1, 999) * Rules->move_rate_roads;
    }
    return speed_val;
}

/*
Purpose: Calculate the cargo capacity of a unit. It seems Spore Launchers were considered to have 
         cargo capacity at one time.
Original Offset: 005C1760
Return Value: Cargo capacity
Status: Complete
*/
uint32_t __cdecl veh_cargo(uint32_t veh_id) {
    uint32_t proto_id = Vehs[veh_id].proto_id;
    uint32_t cargo = VehPrototypes[proto_id].carry_capacity;
    return (cargo && proto_id < MaxVehProtoFactionNum && (get_proto_offense_rating(proto_id) < 0 
        || proto_id == BSC_SPORE_LAUNCHER)) ? Vehs[veh_id].morale + 1 : cargo;
}

/*
Purpose: Determine the extra percent cost for building a prototype. Includes a check if the faction 
         has the free prototype flag set or if the player is using one of the easier difficulties.
Original Offset: 005C17D0
Return Value: Percent extra prototype cost
Status: Complete
*/
uint32_t __cdecl prototype_factor(uint32_t proto_id) {
    uint32_t faction_id = proto_id / MaxVehProtoFactionNum;
    if (Players[faction_id].rule_flags & RFLAG_FREEPROTO
        || PlayersData[faction_id].diff_level <= DLVL_SPECIALIST) {
        return 0;
    }
    uint8_t triad = get_proto_triad(proto_id);
    switch (triad) {
      case TRIAD_SEA:
        return Rules->extra_pct_cost_proto_sea;
      case TRIAD_AIR:
        return Rules->extra_pct_cost_proto_air;
      case TRIAD_LAND:
      default:
        return Rules->extra_pct_cost_proto_land;
    }
}

/*
Purpose: Calculate the specified prototype's overall cost to build. Optional output parameter
         whether there is an associated 1st time prototype cost (true) or just the base (false).
Original Offset: 005C1850
Return Value: Mineral cost
Status: Complete
*/
uint32_t __cdecl veh_cost(uint32_t proto_id, int base_id, BOOL *has_proto_cost) {
    uint32_t cost = VehPrototypes[proto_id].cost;
    if (base_id >= 0 && proto_id < MaxVehProtoFactionNum // bug fix: added base_id bounds check
        && (get_proto_offense_rating(proto_id) < 0 || proto_id == BSC_SPORE_LAUNCHER)
        && has_fac_built(FAC_BROOD_PIT, base_id)) {
        cost = (cost * 3) / 4; // Decrease the cost of alien units by 25%
    }
    if (VehPrototypes[proto_id].plan == PLAN_COLONIZATION && base_id >= 0) {
        cost = range(cost, 1, 999);
    }
    uint32_t proto_cost_first = 0;
    if (proto_id >= MaxVehProtoFactionNum 
        && !(VehPrototypes[proto_id].flags & PROTO_TYPED_COMPLETE)) {
        proto_cost_first = (base_id >= 0 && has_fac_built(FAC_SKUNKWORKS, base_id))
            ? 0 : (prototype_factor(proto_id) * base_cost(proto_id) + 50) / 100; // moved checks up
        cost += proto_cost_first;
    }
    if (has_proto_cost) {
        *has_proto_cost = proto_cost_first != 0;
    }
    return cost;
}

/*
Purpose: Check whether the current player can select the specified unit: it has to be on the map,
         belong to the current faction, not be an invisible lurker and not be stuck inside an air
         transport.
Original Offset: 005C1A20
Return Value: Is the unit selectable? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-veh-helpers.log)
*/
BOOL __cdecl veh_selectable(int veh_id) {
    if (veh_id < 0) {
        return false;
    }
    int x = Vehs[veh_id].x;
    int y = Vehs[veh_id].y;
    if (y < 0 || y >= *MapLatitudeBounds || x < 0 || x >= *MapLongitudeBounds) {
        return false;
    }
    if (Vehs[veh_id].faction_id != *FactionIDCurrent) {
        return false;
    }
    if (Vehs[veh_id].flags & VFLAG_LURKER && Vehs[veh_id].flags & VFLAG_INVISIBLE) {
        return false;
    }
    return !veh_jail(veh_id);
}

/*
Purpose: Check whether the specified selectable unit still has moves left this turn. Units busy
         terraforming (orders farm through monolith) count as unmoved unless they are also crawling.
Original Offset: 005C1AB0
Return Value: Does the unit still have moves? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-veh-helpers.log)
*/
BOOL __cdecl veh_unmoved(int veh_id) {
    if (!veh_selectable(veh_id)) { // same checks as the original, which inlines them
        return false;
    }
    int8_t order = Vehs[veh_id].order;
    if (Vehs[veh_id].state & VSTATE_CRAWLING || order < ORDER_FARM || order > ORDER_PLACE_MONOLITH) {
        if ((int)speed(veh_id, false) - Vehs[veh_id].moves_expended <= 0) {
            return false;
        }
    }
    return true;
}

/*
Purpose: Check whether the specified selectable unit is waiting for orders: it has moves left and is
         not sentried, holding or convoying (unless it is exploring or flagged VSTATE_UNK_200).
Original Offset: 005C1B70
Return Value: Is the unit ready for orders? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-veh-helpers.log)
*/
BOOL __cdecl veh_ready(int veh_id) {
    if (!veh_selectable(veh_id)) { // same checks as the original, which inlines them
        return false;
    }
    int8_t order = Vehs[veh_id].order;
    if (!(Vehs[veh_id].state & (VSTATE_EXPLORE | VSTATE_UNK_200))
        && (order == ORDER_SENTRY_BOARD || order == ORDER_HOLD || order == ORDER_CONVOY)) {
        return false;
    }
    return (int)speed(veh_id, false) - Vehs[veh_id].moves_expended > 0;
}

/*
Purpose: Check if the land unit inside an air transport can disembark. The transport must be in 
         either a base or an airbase to do so.
Original Offset: 005C1C40
Return Value: Can the specified unit disembark? true/false
Status: Complete
*/
BOOL __cdecl veh_jail(uint32_t veh_id) {
    if (get_triad(veh_id) == TRIAD_LAND && Vehs[veh_id].order == ORDER_SENTRY_BOARD
        && Vehs[veh_id].waypoint_x[0] >= 0 && get_triad(Vehs[veh_id].waypoint_x[0]) == TRIAD_AIR
        && (!(bit_at(Vehs[veh_id].x, Vehs[veh_id].y) & BIT_BASE_IN_TILE)
            || owner_at(Vehs[veh_id].x, Vehs[veh_id].y) >= 8
            || owner_at(Vehs[veh_id].x, Vehs[veh_id].y) <= 0)
        && !(bit_at(Vehs[veh_id].x, Vehs[veh_id].y) & BIT_AIRBASE)) {
        return true;
    }
    return false;
}

/*
Purpose: Sets all moves for the specified unit as expended.
Original Offset: 005C1D20
Return Value: n/a
Status: Complete
*/
void __cdecl veh_skip(uint32_t veh_id) {
    // TODO Bug: Due to size of moves_expended, speeds over 255 will be incorrect. The speed()
    //           function can return a value from 1-999. Eventually increase size to 16 bits.
    Vehs[veh_id].moves_expended = (uint8_t)speed(veh_id, false);
}

/*
Purpose: Initialize/reset the fake unit id (2048) used as a placeholder for various UI elements.
Original Offset: 005C1D50
Return Value: Fake unit id (2048)
Status: Complete
*/
int __cdecl veh_fake(int proto_id, uint32_t faction_id) {
    veh_clear(2048, proto_id, faction_id);
    return 2048;
}

/*
Purpose: Activate the specified unit and clear the current action.
Original Offset: 005C1D70
Return Value: Unit id (doesn't look to be used on return)
Status: Complete
*/
int __cdecl veh_wake(uint32_t veh_id) {
    uint8_t order = Vehs[veh_id].order;
    uint32_t state = Vehs[veh_id].state;
    if (order >= ORDER_FARM && order < ORDER_MOVE_TO && !(state & VSTATE_CRAWLING)) {
        // TODO bug fix: Issue with moves_expended size / speed return, see veh_skip()
        Vehs[veh_id].moves_expended = (uint8_t)(speed(veh_id, false) - Rules->move_rate_roads);
        int terra_turns = Vehs[veh_id].terraforming_turns;
        if (terra_turns) {
            terra_turns -= contribution(veh_id, order - 4);
            if (terra_turns < 0) {
                terra_turns = 0;
            }
            Vehs[veh_id].terraforming_turns = (uint8_t)terra_turns;
        }
    }
    if (state & VSTATE_UNK_200 && Vehs[veh_id].order_auto_type == ORDERA_ON_ALERT
        && !(state & VSTATE_UNK_4)) {
        Vehs[veh_id].moves_expended = 0;
    }
    Vehs[veh_id].order = ORDER_NONE;
    Vehs[veh_id].state &= ~(VSTATE_UNK_200 | VSTATE_EXPLORE | VSTATE_UNK_1000000 
        | VSTATE_UNK_2000000 | VSTATE_UNK_8000000); // TODO: Identify VSTATE flags
    return veh_id;
}

/*
Purpose: One AI step for the specified unit: enemy_move() picks its move (skipped for a unit
         already under way on a move to order that isn't a transport, isn't exploring and isn't
         next to an enemy), then, if it has moves left, action() carries out its order (no
         action possible: the unit's moves are spent). In a network game the unit of the faction
         at the controls is synchronized instead and NetDaemon::action() acts for it.
Original Offset: 00579240
Return Value: 1 if the unit is done for this turn, 0 if it can be given another step
Status: Complete
*/
int __cdecl enemy_veh(uint32_t veh_id) {
    uint32_t faction_id = Vehs[veh_id].faction_id;
    BOOL net_sync = (faction_id == (uint32_t)*FactionIDCurrent && *IsMultiplayerNet);
    if (*IsMultiplayerNet) {
        *VehAttackFlags = 3;
    }
    do_all_non_input();
    if (!Vehs[veh_id].moves_expended || Vehs[veh_id].order != ORDER_MOVE_TO
        || VehPrototypes[Vehs[veh_id].proto_id].plan == PLAN_NAVAL_TRANSPORT
        || Vehs[veh_id].state & VSTATE_EXPLORE
        || zoc_any(Vehs[veh_id].x, Vehs[veh_id].y, faction_id)) {
        if (enemy_move(veh_id)) {
            return 1;
        }
        if ((int)speed(veh_id, false) - Vehs[veh_id].moves_expended <= 0) {
            if (*IsMultiplayerNet) {
                log_say("Enemy Vehicle DONE", veh_id, 0, 0);
            }
            if (net_sync) {
                synch_veh(veh_id);
                NetDaemon_await_synch(NetDaemonVar, 0);
                NetDaemon_unlock_veh(NetDaemonVar, 0);
            }
            return 1;
        }
    }
    if (net_sync) {
        synch_veh(veh_id);
        NetDaemon_await_synch(NetDaemonVar, 0);
        return NetDaemon_action(NetDaemonVar, 0, veh_id, 1) ? 1 : 0;
    }
    if (*IsMultiplayerNet) {
        char veh_id_str[12];
        _itoa_s(veh_id, veh_id_str, 10);
        log_say("Enemy Vehicle ACTION", veh_id_str, Vehs[veh_id].order, Vehs[veh_id].waypoint_x[0],
            Vehs[veh_id].waypoint_y[0]);
    }
    if (!action(veh_id)) {
        veh_skip(veh_id);
    }
    if (*IsMultiplayerNet) {
        char ai_type_str[2] = { (char)Vehs[veh_id].move_to_ai_type, 0 };
        log_say("Enemy Vehicle", ai_type_str, veh_id, Vehs[veh_id].x, Vehs[veh_id].y);
    }
    return 0;
}

/*
Purpose: Move all the units of the specified faction with the AI, in ten passes over the unit list
         (from the end): artillery, then defenders and transports with cargo, air units except air
         superiority, combat units, undamaged units, then everyone. Drop pod, air superiority and
         group 21 designs wait for pass 7; pass 8 gives the transports of the non-alien factions
         an extra step before their moves are checked. A unit gets steps until it's done, has
         no moves left, boards (sentry) or the unit count changes; after 32 steps (moves to
         count once, other surface units twice) its moves are spent.
Original Offset: 00579510
Return Value: n/a
Status: Complete
*/
void __cdecl enemy_turn(uint32_t faction_id) {
    for (int pass = 0; pass < 10; pass++) {
        for (int veh_id = *VehCurrentCount - 1; veh_id >= 0; veh_id--) {
            if (veh_id >= *VehCurrentCount || Vehs[veh_id].faction_id != faction_id) {
                continue;
            }
            BOOL design_check = true; // drop pods, air superiority and group 21 wait
            switch (pass) {
              case 0:
                if (!can_arty(Vehs[veh_id].proto_id, true)) {
                    continue;
                }
                break;
              case 1:
                if (VehPrototypes[Vehs[veh_id].proto_id].plan != PLAN_DEFENSIVE
                    && (VehPrototypes[Vehs[veh_id].proto_id].plan != PLAN_NAVAL_TRANSPORT
                        || !stack_check(veh_id, 3, 0, -1, -1))) {
                    continue;
                }
                break;
              case 2:
                if (Chassis[VehPrototypes[Vehs[veh_id].proto_id].chassis_id].triad != TRIAD_AIR
                    || VehPrototypes[Vehs[veh_id].proto_id].plan == PLAN_AIR_SUPERIORITY) {
                    continue;
                }
                break;
              case 3:
                if (VehPrototypes[Vehs[veh_id].proto_id].plan != PLAN_COMBAT) {
                    continue;
                }
                break;
              case 4:
                if (Vehs[veh_id].dmg_incurred) {
                    continue;
                }
                break;
              case 5:
              case 6:
                break;
              default:
                design_check = false;
                break;
            }
            if (design_check && (has_abil(Vehs[veh_id].proto_id, ABL_DROP_POD)
                || VehPrototypes[Vehs[veh_id].proto_id].plan == PLAN_AIR_SUPERIORITY
                || VehPrototypes[Vehs[veh_id].proto_id].unk_1 == 21)) {
                continue;
            }
            if (pass == 8 && (VehPrototypes[Vehs[veh_id].proto_id].plan != PLAN_NAVAL_TRANSPORT
                || !faction_id)) {
                continue;
            }
            int steps = 0;
            while (Vehs[veh_id].order != ORDER_SENTRY_BOARD) {
                if (pass != 8 || steps) {
                    if ((int)speed(veh_id, false) - Vehs[veh_id].moves_expended <= 0) {
                        break;
                    }
                }
                int veh_count = *VehCurrentCount;
                if (enemy_veh(veh_id) || veh_count != *VehCurrentCount) {
                    break;
                }
                if (Vehs[veh_id].order != ORDER_MOVE_TO
                    && Chassis[VehPrototypes[Vehs[veh_id].proto_id].chassis_id].triad != TRIAD_AIR) {
                    steps++;
                }
                if (++steps > 32) {
                    veh_skip(veh_id); // and on: the loop ends once the moves check fails
                }
            }
        }
    }
    PlayersData[faction_id].flags &= ~PFLAG_UNK_10000;
}

/*
Purpose: Get the triad for the specified prototype.
Original Offset: n/a
Return Value: Triad (see VehTriad)
Status: Complete
*/
uint8_t __cdecl get_proto_triad(uint32_t proto_id) {
    return Chassis[VehPrototypes[proto_id].chassis_id].triad;
}

/*
Purpose: Get the triad for the specified unit.
Original Offset: n/a
Return Value: Triad (see VehTriad)
Status: Complete
*/
uint8_t __cdecl get_triad(uint32_t veh_id) {
    return get_proto_triad(Vehs[veh_id].proto_id);
}

/*
Purpose: Get the offense rating for the specified prototype.
Original Offset: n/a
Return Value: Offense rating
Status: Complete
*/
int8_t __cdecl get_proto_offense_rating(uint32_t proto_id) {
    return Weapon[VehPrototypes[proto_id].weapon_id].offense_rating;
}

/*
Purpose: Get the offense rating for the specified unit.
Original Offset: n/a
Return Value: Offense rating
Status: Complete
*/
int8_t __cdecl get_offense_rating(uint32_t veh_id) {
    return get_proto_offense_rating(Vehs[veh_id].proto_id);
}

/*
Purpose: Get the defense rating for the specified prototype.
Original Offset: n/a
Return Value: Defense rating
Status: Complete
*/
int8_t __cdecl get_proto_defense_rating(uint32_t proto_id) {
    return Armor[VehPrototypes[proto_id].armor_id].defense_rating;
}

/*
Purpose: Get the defense rating for the specified unit.
Original Offset: n/a
Return Value: Defense rating
Status: Complete
*/
int8_t __cdecl get_defense_rating(uint32_t veh_id) {
    return get_proto_defense_rating(Vehs[veh_id].proto_id);
}

/*
Purpose: Determine if the specified prototype is a missile.
Original Offset: n/a
Return Value: Is prototype a missile? true/false
Status: Complete
*/
BOOL __cdecl is_proto_missile(uint32_t proto_id) {
    return Chassis[VehPrototypes[proto_id].chassis_id].missile;
}

/*
Purpose: Determine if the specified unit is a missile.
Original Offset: n/a
Return Value: Is unit a missile? true/false
Status: Complete
*/
BOOL __cdecl is_missile(uint32_t veh_id) {
    return is_proto_missile(Vehs[veh_id].proto_id);
}

/*
Purpose: Tell a network game's players that the unit can't do that here (FEATURE message); in a
         single player game nothing happens.
Original Offset: 004CD060
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch30.log)
*/
void __cdecl action_feature() {
    if (*IsMultiplayerNet) {
        NetMsg_pop(NetMsgVar, NULL, "FEATURE", 5000, 0, 0);
    }
}

/*
Purpose: Order a unit to patrol between its tile and the specified tile, if it can: the target and
         the unit's own tile are appended as waypoints (from the current waypoint count, past the end
         of the four slot array when it is full, as in the original) and the patrol restarts.
Original Offset: 004CDA30
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch30.log)
*/
void __cdecl action_patrol(int veh_id, int x, int y) {
    if (!valid_patrol(veh_id, x, y)) {
        return;
    }
    Veh *veh = &Vehs[veh_id];
    veh->order = ORDER_MOVE_TO;
    veh->waypoint_x[veh->waypoint_count] = (int16_t)x;
    veh->waypoint_y[veh->waypoint_count] = (int16_t)y;
    veh->waypoint_count++;
    veh->waypoint_x[veh->waypoint_count] = veh->x;
    veh->waypoint_y[veh->waypoint_count] = veh->y;
    veh->state |= VSTATE_UNK_1000000 | VSTATE_UNK_200;
    veh->patrol_current_point = 0;
}

/*
Purpose: Carry out a unit's standing order for this turn: terraforming orders (farm .. place
         monolith) through action_terraform(), road and magtube building toward a tile through
         action_road_to(), move orders through action_go_to(). An order with the bit 0x40 (the AI's
         one-time orders) is cleared afterwards.
Original Offset: 004CF740
Return Value: Did the unit have an order to carry out? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch30.log)
*/
BOOL __cdecl action(int veh_id) {
    int8_t order = Vehs[veh_id].order;
    if (order >= ORDER_FARM && order < ORDER_MOVE_TO) {
        action_terraform(veh_id, order - ORDER_FARM, 1);
    } else if (order == ORDER_ROAD_TO || order == ORDER_MAGTUBE_TO) {
        action_road_to(veh_id);
    } else if (order == ORDER_MOVE_TO || order == ORDER_AI_MOVE_TO || order == ORDER_MOVE) {
        action_go_to(veh_id);
    } else {
        return false;
    }
    if (Vehs[veh_id].order & 0x40) {
        Vehs[veh_id].order = ORDER_NONE;
    }
    return true;
}

/*
Purpose: Send a unit through the Psi Gate to the specified base: both bases' gates are marked as
         used this turn, the unit is moved, both tiles are redrawn and the map view follows the unit
         if it is the one selected.
Original Offset: 004CF380
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch30.log)
*/
void __cdecl action_gate(int veh_id, int base_id) {
    int base_id_from = base_at(Vehs[veh_id].x, Vehs[veh_id].y);
    if (base_id_from >= 0) {
        Bases[base_id_from].state |= BSTATE_PSI_GATE_USED;
    }
    Bases[base_id].state |= BSTATE_PSI_GATE_USED;
    veh_drop(veh_lift(veh_id), Bases[base_id].x, Bases[base_id].y);
    if (base_id_from >= 0) {
        draw_tile(Bases[base_id_from].x, Bases[base_id_from].y, 2);
    }
    draw_tile(Bases[base_id].x, Bases[base_id].y, 2);
    if (veh_id == *MapWinVehIDSelected) {
        Console_focus(ConsoleVar, NULL, Vehs[veh_id].x, Vehs[veh_id].y, Vehs[veh_id].faction_id);
    }
}

/*
Purpose: Nerve staple a base. The first two times always work, the next six half of the time
         (rand), later ones never: on success the riot ends, the staple lasts 10 more turns and the
         base's owner (if at the controls) or everyone else hears of it. Either way the base's
         count goes up and the act is an atrocity.
Original Offset: 004CA7F0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch30.log)
*/
void __cdecl action_staple(int base_id) {
    Base *base = &Bases[base_id];
    int count = ++base->nerve_staple_count;
    uint32_t faction_id = base->faction_id_current;
    BOOL is_alien = (Players[faction_id].rule_flags & RFLAG_ALIEN) != 0;
    if (count <= 2 || (count <= 8 && _rand() % 2)) {
        base->state &= ~BSTATE_DRONE_RIOTS_ACTIVE;
        base->nerve_staple_turns_left += 10;
        draw_tile(base->x, base->y, 2);
        if (base->faction_id_current == *FactionIDCurrent) {
            parse_says(0, base->name_string, -1, -1);
            popp(ScriptTxtID, "NERVESTAPLEDONE", 0, is_alien ? "ALdrone_sm.pcx" : "drone_sm.pcx", 0);
        } else {
            parse_says(0, base->name_string, -1, -1);
            *PluralityDefault = false;
            *GenderDefault = Players[base->faction_id_current].is_leader_female;
            parse_says(1, Players[base->faction_id_current].title_leader, -1, -1);
            *PluralityDefault = false;
            *GenderDefault = Players[base->faction_id_current].is_leader_female;
            parse_says(2, Players[base->faction_id_current].name_leader, -1, -1);
            NetMsg_pop(NetMsgVar, NULL, "NERVESTAPLETHEM", 5000, 0, 0);
        }
    } else if (base->faction_id_current == *FactionIDCurrent) {
        parse_says(0, base->name_string, -1, -1);
        popp(ScriptTxtID, "NERVESTAPLEFAIL", 0, is_alien ? "ALdrone_sm.pcx" : "drone_sm.pcx", 0);
    }
    atrocity(base->faction_id_current, base->faction_id_former,
        base->faction_id_former != base->faction_id_current, true);
}

/*
Purpose: Self-destruct a unit: an explosion damages every unit on the unit's tile and the eight
         around it (bases are spared) by half its reactor times its weapon value (1 to 20), then
         the units the damage destroys are killed (artifacts at any damage).
Original Offset: 004CD2F0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch30.log)
*/
void __cdecl action_destruct(int veh_id) {
    int x = Vehs[veh_id].x;
    int y = Vehs[veh_id].y;
    boom(x, y, 0);
    int power = weap_val(Vehs[veh_id].proto_id, Vehs[veh_id].faction_id);
    power = (power < 1) ? 1 : ((power > 20) ? 20 : power);
    int damage = VehPrototypes[Vehs[veh_id].proto_id].reactor_id * power / 2;
    kill(veh_id);
    for (int i = 0; i < 9; i++) {
        int x_radius = x + RadiusOffsetX[i];
        if (!(*MapIsFlat & 1)) {
            if (x_radius < 0) {
                x_radius += *MapLongitudeBounds;
            } else if (x_radius >= *MapLongitudeBounds) {
                x_radius -= *MapLongitudeBounds;
            }
        }
        int y_radius = y + RadiusOffsetY[i];
        if (y_radius < 0 || y_radius >= *MapLatitudeBounds || x_radius < 0
            || x_radius >= *MapLongitudeBounds || base_at(x_radius, y_radius) >= 0) {
            continue;
        }
        for (int v = veh_at(x_radius, y_radius); v >= 0; v = Vehs[v].next_veh_id_stack) {
            Vehs[v].dmg_incurred += (uint8_t)damage;
        }
        for (;;) {
            int v = stack_fix(veh_at(x_radius, y_radius));
            for (; v >= 0; v = Vehs[v].next_veh_id_stack) {
                VehPrototype *proto = &VehPrototypes[Vehs[v].proto_id];
                int lethal = 1;
                if (proto->plan != PLAN_ALIEN_ARTIFACT) {
                    int reactor = proto->reactor_id;
                    lethal = ((reactor < 1) ? 1 : ((reactor > 100) ? 100 : reactor)) * 10;
                }
                if (Vehs[v].dmg_incurred >= lethal) {
                    break;
                }
            }
            if (v < 0) {
                break;
            }
            stack_veh(v, 1);
            stack_kill(v);
        }
    }
}

/*
Purpose: A planet buster obliterates a base: the notice's parse slots are filled first (the base's
         size in thousands, its name, the title of the player at the controls, the former owner's
         faction name, the attacker's title and name), then the base is removed and the map
         redrawn. The attacker at the controls is told OBLITTED; the player at the controls is told
         OBLITTED2 if the base was or had been theirs or they could see it. With the #RULES toggle
         "obliterating a base counts as an atrocity" the attacker commits one against the former
         owner.
Original Offset: 004CD4E0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch32-unitfx.log)
*/
void __cdecl action_oblit(int veh_id, int base_id) {
    uint32_t faction_id = Vehs[veh_id].faction_id;
    Base *base = &Bases[base_id];
    uint32_t faction_id_former = base->faction_id_former;
    parse_num(0, base->population_size * 10);
    parse_says(0, base->name_string, -1, -1);
    *PluralityDefault = false;
    *GenderDefault = Players[*FactionIDCurrent].is_leader_female;
    parse_says(1, Players[*FactionIDCurrent].title_leader, -1, -1);
    parse_says(2, Players[faction_id_former].adj_name_faction, -1, -1);
    *PluralityDefault = false;
    *GenderDefault = Players[faction_id].is_leader_female;
    parse_says(3, Players[faction_id].title_leader, -1, -1);
    *GenderDefault = Players[faction_id].is_leader_female;
    *PluralityDefault = false;
    parse_says(4, Players[faction_id].name_leader, -1, -1);
    BOOL seen = base->faction_id_current == *FactionIDCurrent
        || (base->visibility & (uint8_t)(1 << *FactionIDCurrent));
    base_kill(base_id);
    draw_map(1);
    if (faction_id == (uint32_t)*FactionIDCurrent) {
        popp(ScriptTxtID, "OBLITTED", 0, "baseobl_sm.pcx", NULL);
    } else if (faction_id_former == (uint32_t)*FactionIDCurrent || seen) {
        popp(ScriptTxtID, "OBLITTED2", 0, "baseobl_sm.pcx", NULL);
    }
    if (Rules->tgl_oblit_base_atrocity) {
        atrocity(faction_id, faction_id_former, 2, false);
    }
}

/*
Purpose: Let the factions near a tectonic or fungal payload's target try to shoot it down
         (shoot_it(): orbital defense pods, flechette defense): the owner of the top unit of each
         stack on the first radius tiles around it (RadiusOffsetX/Y order) that isn't the attacker
         or its pact partner, each faction once, then the owner of the target's territory unless it
         already tried. Asking stops once one has shot it down: the original goes on to ask the territory
         owner even then, and takes that second answer as the result (see action_tectonic()).
Return Value: Non-zero if the missile was shot down
*/
static int missile_shot_down(uint32_t faction_id, int x, int y, int radius, int is_fungal) {
    int shot = 0;
    BOOL tried[MaxPlayerNum] = {};
    for (int i = 0; i < radius; i++) {
        int x_radius = x + RadiusOffsetX[i];
        if (!(*MapIsFlat & 1)) {
            if (x_radius < 0) {
                x_radius += *MapLongitudeBounds;
            } else if (x_radius >= *MapLongitudeBounds) {
                x_radius -= *MapLongitudeBounds;
            }
        }
        int y_radius = y + RadiusOffsetY[i];
        if (y_radius < 0 || y_radius >= *MapLatitudeBounds || x_radius < 0
            || x_radius >= *MapLongitudeBounds) {
            continue;
        }
        int veh_id_tgt = stack_fix(veh_at(x_radius, y_radius));
        if (veh_id_tgt < 0) {
            continue;
        }
        uint32_t faction_id_tgt = Vehs[veh_id_tgt].faction_id;
        if (faction_id_tgt == faction_id
            || (PlayersData[faction_id].diplo_treaties[faction_id_tgt] & DTREATY_PACT)
            || tried[faction_id_tgt]) {
            continue;
        }
        shot = shoot_it(faction_id, faction_id_tgt, x, y, is_fungal);
        if (shot) {
            return shot;
        }
        tried[faction_id_tgt] = true;
    }
    int owner = whose_territory(faction_id, x, y, NULL, false);
    if (owner >= 0 && (uint32_t)owner != faction_id && !tried[owner]) {
        shot = shoot_it(faction_id, owner, x, y, is_fungal);
    }
    return shot;
}

/*
Purpose: Detonate a tectonic payload at the specified tile. If the target's nearest base is the
         player's and the player can see the tile, the map view goes there; the missile is used up
         and, if another faction fired it, the player is told (TECTONICMISSILE). Unless the
         factions around shoot it down (see missile_shot_down(); radius by its reactor), the tile
         is raised once per reactor level, the climate redone and the map redrawn, the player
         whose base it was is told (TECMOFIED) and the attacker's tectonic detonation count goes
         up (native life fights it harder, and a human player's bases suffer more ecological
         damage).
Original Offset: 004CE210
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch32-unitfx.log).
        Two deliberate changes from the v2.1 exe, chosen by Kiousu (2026-10-18): the exe reads the
        attacker for the TECMOFIED notice and the detonation count from the unit slot it has just
        freed (the unit after the missile in the table, or a later one), this credits the faction
        that fired; and the exe asks the territory owner again after the missile was shot down and
        takes that answer, here a payload once shot down stays down.
*/
void __cdecl action_tectonic(int veh_id, int x, int y) {
    BOOL notify = false;
    FX_play2(FXVar, NULL, 0x53, Vehs[veh_id].x, Vehs[veh_id].y);
    uint32_t reactor_id = VehPrototypes[Vehs[veh_id].proto_id].reactor_id;
    // the exe's own copy of RadiusRange[] (0x0066F8C4), as in action_arty()
    int radius = ((int *)0x0066F8C4)[reactor_id];
    int base_id = base_find(x, y);
    uint32_t faction_id = Vehs[veh_id].faction_id;
    if (faction_id != (uint32_t)*FactionIDCurrent
        && ((PlayersData[*FactionIDCurrent].flags & PFLAG_MAP_REVEALED)
            || (map_loc(x, y)->visibility & (uint8_t)(1 << *FactionIDCurrent)))
        && base_id >= 0 && Bases[base_id].faction_id_current == *FactionIDCurrent) {
        Console_focus(ConsoleVar, NULL, x, y, faction_id);
        notify = true;
    }
    kill(veh_id);
    if (faction_id != (uint32_t)*FactionIDCurrent && base_id >= 0) {
        Player *player = &Players[faction_id];
        *PluralityDefault = false;
        *GenderDefault = player->is_leader_female;
        parse_says(0, player->title_leader, -1, -1);
        *GenderDefault = player->is_leader_female;
        *PluralityDefault = false;
        parse_says(1, player->name_leader, -1, -1);
        *GenderDefault = player->noun_gender;
        *PluralityDefault = player->is_noun_plural;
        parse_says(2, player->noun_faction, -1, -1);
        parse_says(3, Bases[base_id].name_string, -1, -1);
        popp(ScriptTxtID, "TECTONICMISSILE", 0, "tectonic_sm.pcx", NULL);
    }
    if (missile_shot_down(faction_id, x, y, radius, false)) {
        return;
    }
    for (int i = (int)reactor_id; i > 0; i--) {
        world_raise_alt(x, y);
    }
    world_climate();
    draw_map(1);
    if (notify) {
        // the exe reads this faction from Vehs[veh_id] again, after kill() freed the slot
        Player *player = &Players[faction_id];
        parse_says(0, Bases[base_id].name_string, -1, -1);
        *PluralityDefault = false;
        *GenderDefault = player->is_leader_female;
        parse_says(1, player->title_leader, -1, -1);
        *PluralityDefault = false;
        *GenderDefault = player->is_leader_female;
        parse_says(2, player->name_leader, -1, -1);
        *GenderDefault = player->noun_gender;
        *PluralityDefault = player->is_noun_plural;
        parse_says(3, player->noun_faction, -1, -1);
        popp(ScriptTxtID, "TECMOFIED", 0, "tectonic_sm.pcx", NULL);
    }
    TectonicDetonationCount[faction_id]++; // the exe: Vehs[veh_id].faction_id after kill()
    *UnkBitfield1 |= 4;
}

/*
Purpose: Detonate a fungal payload at the specified tile. The missile is used up and, if another
         faction fired it, the player is told (FUNGALMISSILE). Unless the factions around shoot it
         down (see missile_shot_down(); radius 1), the target and then random neighbours, twice the
         reactor level in all, turn to fungus: on each, half the time, mind worms are born where no
         base or unit of any faction stands, natives included (by the tile's flags, or its owner
         nibble unset). A tile with fungus already at ocean shelf level or above (shelf, shore or
         land) is left alone, which also leaves it out of the map view and the notice; otherwise
         improvements fungus removes are cleared and deep ocean is raised to the shelf; the player
         sees the change if the tile is visible (and is shown it, with a FUNGMOTIZED notice at the
         end, if the target's nearest base is theirs). A fungal tower grows on a land target.
Original Offset: 004CE790
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch32-unitfx.log).
        Two deliberate changes from the v2.1 exe, chosen by Kiousu (2026-10-18): the exe reads the
        reactor (the number of fungus tiles) and the attacker (the player's notice) from the unit
        slot it has just freed, this uses the missile's own; and the exe asks the territory owner
        again after the missile was shot down, here a payload once shot down stays down.
*/
void __cdecl action_fungal(int veh_id, int x, int y) {
    BOOL notify = false;
    uint32_t faction_id = Vehs[veh_id].faction_id;
    int base_id = base_find(x, y);
    // the exe reads its own RadiusRange[1] (0x0066F8C8): 9, the target and its neighbours
    int radius = RadiusRange[1];
    FX_play2(FXVar, NULL, 0x54, Vehs[veh_id].x, Vehs[veh_id].y);
    int proto_id = Vehs[veh_id].proto_id; // the exe reads it from the freed slot, after kill()
    kill(veh_id);
    if (faction_id != (uint32_t)*FactionIDCurrent && base_id >= 0) {
        Player *player = &Players[faction_id];
        *PluralityDefault = false;
        *GenderDefault = player->is_leader_female;
        parse_says(0, player->title_leader, -1, -1);
        *GenderDefault = player->is_leader_female;
        *PluralityDefault = false;
        parse_says(1, player->name_leader, -1, -1);
        *GenderDefault = player->noun_gender;
        *PluralityDefault = player->is_noun_plural;
        parse_says(2, player->noun_faction, -1, -1);
        parse_says(3, Bases[base_id].name_string, -1, -1);
        popp(ScriptTxtID, "FUNGALMISSILE", 0, "fungpayld_sm.pcx", NULL);
    }
    if (missile_shot_down(faction_id, x, y, radius, true)) {
        return;
    }
    int count = VehPrototypes[proto_id].reactor_id * 2;
    for (int i = 0; i < count; i++) {
        int offset = i ? _rand() % 8 + 1 : 0;
        int x_radius = x + RadiusOffsetX[offset];
        if (!(*MapIsFlat & 1)) {
            if (x_radius < 0) {
                x_radius += *MapLongitudeBounds;
            } else if (x_radius >= *MapLongitudeBounds) {
                x_radius -= *MapLongitudeBounds;
            }
        }
        int y_radius = y + RadiusOffsetY[offset];
        if (y_radius < 0 || y_radius >= *MapLatitudeBounds || x_radius < 0
            || x_radius >= *MapLongitudeBounds) {
            continue;
        }
        if (_rand() % 2) {
            Map *tile = map_loc(x_radius, y_radius);
            if (!(tile->bit & (BIT_BASE_IN_TILE | BIT_VEH_IN_TILE)) || (tile->val2 & 0xF) >= 8) {
                veh_init(BSC_MIND_WORMS, 0, x_radius, y_radius);
            }
        }
        Map *tile = map_loc(x_radius, y_radius);
        if ((tile->bit & BIT_FUNGUS) && (tile->climate & 0xE0) >= ALT_BIT_OCEAN_SHELF) {
            continue;
        }
        // the exe reads this faction from Vehs[veh_id] again, after kill() freed the slot
        if (faction_id != (uint32_t)*FactionIDCurrent
            && ((PlayersData[*FactionIDCurrent].flags & PFLAG_MAP_REVEALED)
                || (tile->visibility & (uint8_t)(1 << *FactionIDCurrent)))
            && base_id >= 0 && Bases[base_id].faction_id_current == *FactionIDCurrent) {
            Console_focus(ConsoleVar, NULL, x_radius, y_radius, faction_id);
            notify = true;
        }
        bit_set(x_radius, y_radius, BIT_FUNGUS, true);
        bit_set(x_radius, y_radius, BIT_FOREST | BIT_SOIL_ENRICHER | BIT_FARM | BIT_SOLAR_TIDAL
            | BIT_MINE | BIT_MAGTUBE | BIT_ROAD, false); // 0x28805C
        bit_set(x_radius, y_radius, *TerrainBitsFungusClears, false);
        if ((map_loc(x_radius, y_radius)->climate & 0xE0) < ALT_BIT_OCEAN_SHELF) {
            world_alt_set(x_radius, y_radius, ALT_OCEAN_SHELF, false);
        }
        if ((PlayersData[*FactionIDCurrent].flags & PFLAG_MAP_REVEALED)
            || (map_loc(x_radius, y_radius)->visibility & (uint8_t)(1 << *FactionIDCurrent))
            || (*GameState & STATE_OMNISCIENT_VIEW)) {
            synch_bit(x_radius, y_radius, *FactionIDCurrent);
            draw_tile(x_radius, y_radius, 2);
        }
    }
    if ((map_loc(x, y)->climate & 0xE0) >= ALT_BIT_SHORE_LINE) {
        veh_init(BSC_FUNGAL_TOWER, 0, x, y);
    }
    *UnkBitfield1 |= 4;
    if (notify) {
        Player *player = &Players[faction_id]; // the exe: Vehs[veh_id].faction_id after kill()
        parse_says(0, Bases[base_id].name_string, -1, -1);
        *PluralityDefault = false;
        *GenderDefault = player->is_leader_female;
        parse_says(1, player->title_leader, -1, -1);
        *PluralityDefault = false;
        *GenderDefault = player->is_leader_female;
        parse_says(2, player->name_leader, -1, -1);
        *GenderDefault = player->noun_gender;
        *PluralityDefault = player->is_noun_plural;
        parse_says(3, player->noun_faction, -1, -1);
        popp(ScriptTxtID, "FUNGMOTIZED", 0, "fungpayld_sm.pcx", NULL);
    }
}

/*
Purpose: Find the receiving faction's prototype for a unit it is given: its active design with the
         same chassis, weapon, armor, abilities and reactor, else a design proposed for it with
         those parts, else its first active design with the same chassis, weapon and armor, the
         same plan or the same chassis, else the Transport Foil (sea chassis) or Scout Patrol.
Return Value: Prototype id
*/
static int give_proto(int proto_id, uint32_t faction_id) {
    VehPrototype *src = &VehPrototypes[proto_id];
    const int first = faction_id * MaxVehProtoFactionNum;
    for (int i = 0; i < MaxVehProtoFactionNum; i++) {
        VehPrototype *proto = &VehPrototypes[first + i];
        if ((proto->flags & PROTO_ACTIVE) && proto->chassis_id == src->chassis_id
            && proto->weapon_id == src->weapon_id && proto->armor_id == src->armor_id
            && proto->ability_flags == src->ability_flags && proto->reactor_id == src->reactor_id) {
            return first + i;
        }
    }
    int result = propose_proto(faction_id, src->chassis_id, src->weapon_id, src->armor_id,
        src->ability_flags, src->reactor_id, (uint8_t)src->unk_1, NULL);
    if (result >= 0) {
        return result;
    }
    for (int i = 0; i < MaxVehProtoFactionNum; i++) {
        VehPrototype *proto = &VehPrototypes[first + i];
        if ((proto->flags & PROTO_ACTIVE) && proto->chassis_id == src->chassis_id
            && proto->weapon_id == src->weapon_id && proto->armor_id == src->armor_id) {
            return first + i;
        }
    }
    for (int i = 0; i < MaxVehProtoFactionNum; i++) {
        VehPrototype *proto = &VehPrototypes[first + i];
        if ((proto->flags & PROTO_ACTIVE) && proto->plan == src->plan) {
            return first + i;
        }
    }
    for (int i = 0; i < MaxVehProtoFactionNum; i++) {
        VehPrototype *proto = &VehPrototypes[first + i];
        if ((proto->flags & PROTO_ACTIVE) && proto->chassis_id == src->chassis_id) {
            return first + i;
        }
    }
    return (Chassis[src->chassis_id].triad == TRIAD_SEA) ? BSC_TRANSPORT_FOIL : BSC_SCOUT_PATROL;
}

/*
Purpose: Give the units in the list (unit ids, in stack order) to the faction: each keeps a basic
         design or gets the receiver's matching one (give_proto()) and is counted against it (the
         giver's count of its old design down, the receiver's of the new one up, as give_a_base()
         does), loses its orders and a few state flags and is homed at the receiver's nearest base,
         and the tile's owner becomes the receiver unless a base with an owner stands there.
         Proposing a design can retire another and disband its units (retire_proto()), which
         moves the later units down the table, so veh_kill() renumbers the first watch_count ids
         meanwhile (VehKillWatch) and a unit disbanded on the way (-1) is skipped. The watch is
         cleared in a __finally block, which also runs when a crash is caught further up (the
         regression harness's __except); a guard object's destructor would not, as /EHsc doesn't
         unwind C++ objects for structured exceptions.
Return Value: n/a
*/
static void give_units(int *ids, int count, int watch_count, int faction_id, int x, int y) {
    VehKillWatch = ids;
    VehKillWatchNum = watch_count;
    __try {
        for (int n = 0; n < count; n++) {
            if (ids[n] < 0) {
                continue;
            }
            int proto_id = Vehs[ids[n]].proto_id;
            if (proto_id >= MaxVehProtoFactionNum) {
                proto_id = give_proto(proto_id, faction_id);
            }
            const int i = ids[n]; // after give_proto(): renumbered, or -1 if disbanded
            if (i < 0) {
                continue;
            }
            // counted against its new design as give_a_base() counts units changing sides (the exe
            // doesn't), so a design just made for it isn't the unused one the next proposal retires
            PlayersData[Vehs[i].faction_id].proto_id_active[Vehs[i].proto_id]--;
            Vehs[i].faction_id = (uint8_t)faction_id;
            Vehs[i].proto_id = (int16_t)proto_id;
            PlayersData[faction_id].proto_id_active[proto_id]++;
            Vehs[i].order = ORDER_NONE;
            Vehs[i].state &= ~(VSTATE_UNK_200 | VSTATE_EXPLORE | VSTATE_UNK_1000000
                | VSTATE_UNK_2000000);
            int base_id = base_find(x, y, (uint32_t)faction_id);
            Vehs[i].home_base_id = (int16_t)((base_id < 0) ? -1 : base_id);
            Map *tile = map_loc(x, y);
            if (!(tile->bit & BIT_BASE_IN_TILE) || (tile->val2 & 0xF) >= 8) {
                owner_set(x, y, faction_id);
            }
        }
    } __finally {
        VehKillWatch = NULL;
        VehKillWatchNum = 0;
    }
}

/*
Purpose: Give a unit, with the units sentried aboard it, to another faction (0: release it into
         the wild). Each one keeps a basic design or gets the receiver's matching one (see
         give_proto()) and is counted against it, loses its orders and a few state flags and is
         homed at the receiver's nearest base; the tile's owner becomes the receiver unless a base
         with an owner stands there. The receiver at the controls is shown the unit and told about
         it (GOTUNIT in a network game, from a human or in a translated game, else a dialog that
         depends on whether the unit or the plan of the region is defensive), the giver at the
         controls is told GAVEUNIT or GAVEWILD.
Original Offset: 004CEDE0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch32-unitfx.log).
        Deliberate changes from the v2.1 exe, chosen by Kiousu (2026-10-18): the exe finds the units
        as it goes, walking the stack with slot numbers that proposing a design can make stale (a
        retired design disbands its units), and then gives away the wrong unit; this takes the
        units aboard first and follows them through veh_kill() (give_units()), so the units
        actually given go over. That includes the rest of the cargo when the given transport
        itself is disbanded on the way (converted before its cargo, then its new design retired),
        which the exe leaves with the giver even with the right slot numbers (veh_kill() clears
        their orders): one gift is never split between the two factions. And each unit given is
        counted against its new design (proto_id_active) as give_a_base() counts units changing
        sides; the exe never updates these counts, so the design just made for a given unit looked
        unused and was the one the next unit's proposal retired, disbanding the unit just given.
*/
void __cdecl action_give(int veh_id, int faction_id) {
    int y = Vehs[veh_id].y;
    int x = Vehs[veh_id].x;
    uint32_t faction_id_from = Vehs[veh_id].faction_id;
    const int veh_id_given = veh_id;
    if (veh_id >= 0) {
        // the unit and the units aboard it, in stack order, taken before any is given (the given
        // unit's id is valid here, so a unit sentried outside a transport, waypoint_x[0] -1, never
        // counts); ids[count] keeps the given unit's id through the kills
        static int ids[MaxVehNum + 2];
        int count = 0;
        int top = veh_id;
        for (int prev = Vehs[veh_id].prev_veh_id_stack; prev >= 0;
            prev = Vehs[prev].prev_veh_id_stack) {
            top = prev;
        }
        for (int i = top; i >= 0 && count <= MaxVehNum; i = Vehs[i].next_veh_id_stack) {
            if (i == veh_id
                || (Vehs[i].order == ORDER_SENTRY_BOARD && Vehs[i].waypoint_x[0] == veh_id)) {
                ids[count++] = i;
            }
        }
        ids[count] = veh_id;
        give_units(ids, count, count + 1, faction_id, x, y);
        veh_id = ids[count];
    }
    if (veh_id < 0) {
        veh_id = veh_id_given; // disbanded on the way: read its old slot, as the exe does
    }
    draw_radius(x, y, 3, 2);
    if (faction_id == *FactionIDCurrent) {
        parse_says(0, VehPrototypes[Vehs[veh_id].proto_id].veh_name, -1, -1);
        Player *player = &Players[faction_id_from];
        *PluralityDefault = false;
        *GenderDefault = player->is_leader_female;
        parse_says(1, player->title_leader, -1, -1);
        *GenderDefault = player->is_leader_female;
        *PluralityDefault = false;
        parse_says(2, player->name_leader, -1, -1);
        *GenderDefault = player->noun_gender;
        *PluralityDefault = player->is_noun_plural;
        parse_says(3, player->noun_faction, -1, -1);
        Console_focus(ConsoleVar, NULL, x, y, faction_id_from);
        int base_id = base_find(x, y);
        if (base_id < 0) {
            return;
        }
        parse_says(4, Bases[base_id].name_string, -1, -1);
        if (*Language || (*FactionsStatus & (uint8_t)(1 << faction_id_from))) {
            NetMsg_pop(NetMsgVar, NULL, "GOTUNIT", 5000, 0, 0);
        } else if (PlayersData[faction_id].region_base_plan[map_loc(x, y)->region] == PLAN_DEFENSIVE
            || VehPrototypes[Vehs[veh_id].proto_id].plan == PLAN_DEFENSIVE) {
            X_pop("GOTUNITDEFEND", NULL);
        } else {
            X_pop("GOTUNITOFFEND", NULL);
        }
    } else if (faction_id_from == (uint32_t)*FactionIDCurrent) {
        *GenderDefault = Players[faction_id].noun_gender;
        *PluralityDefault = Players[faction_id].is_noun_plural;
        parse_says(3, Players[faction_id].noun_faction, -1, -1);
        NetMsg_pop(NetMsgVar, NULL, faction_id ? "GAVEUNIT" : "GAVEWILD", 5000, 0, 0);
    }
}

/*
Purpose: A faction's orbital defense pods shoot at another faction's satellite (sat_type 0..3:
         nutrient, mineral, energy satellites, orbital defense pods) or at a base's Geosynchronous
         Survey Pod (sat_type 4). Unless already at vendetta the attacker double crosses the target
         first. Half the time (rand) the target is destroyed, otherwise the attacker loses a pod;
         the player at the controls on either side is told, and an open report on that is redrawn.
Original Offset: 004CF480
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch30.log)
*/
void __cdecl action_sat_attack(int faction_id, int faction_id_def, int sat_type, int base_id) {
    if (!(PlayersData[faction_id].diplo_treaties[faction_id_def] & DTREATY_VENDETTA)) {
        double_cross(faction_id, faction_id_def, -1);
    }
    if (faction_id == *FactionIDCurrent || faction_id_def == *FactionIDCurrent) {
        const int factions[2] = { faction_id, faction_id_def };
        for (int i = 0; i < 2; i++) {
            Player *player = &Players[factions[i]];
            *PluralityDefault = false;
            *GenderDefault = player->is_leader_female;
            parse_says(i * 3, player->title_leader, -1, -1);
            *GenderDefault = player->is_leader_female;
            *PluralityDefault = false;
            parse_says(i * 3 + 1, player->name_leader, -1, -1);
            *GenderDefault = player->noun_gender;
            *PluralityDefault = player->is_noun_plural;
            parse_says(i * 3 + 2, player->noun_faction, -1, -1);
        }
        parse_say(6, (int)Facility[(sat_type == 4) ? FAC_GEOSYNC_SURVEY_POD
            : FacilityRepStart + sat_type].name, -1, -1);
        parse_says(7, Players[faction_id_def].adj_name_faction, -1, -1);
    }
    LPCSTR label = NULL;
    if (_rand() % 100 < 50) {
        if (sat_type == 4) {
            set_fac(FAC_GEOSYNC_SURVEY_POD, base_id, false);
        } else {
            (&PlayersData[faction_id_def].satellites_nutrient)[sat_type]--;
        }
        PlayersData[faction_id].sat_odp_deployed++;
        if (faction_id == *FactionIDCurrent) {
            label = "WEKILLEDIT";
        } else if (faction_id_def == *FactionIDCurrent) {
            label = "THEYKILLEDIT";
        }
    } else {
        PlayersData[faction_id].satellites_odp_total--;
        if (faction_id == *FactionIDCurrent) {
            label = "WEMUFFEDIT";
        } else if (faction_id_def == *FactionIDCurrent) {
            label = "THEYMUFFEDIT";
        }
    }
    if (label) {
        popp(ScriptTxtID, label, 0, "satbat_sm.pcx", 0);
    }
    if (*ReportIfMode == 6 && ReportIfPageTypes[*ReportIfPage] == 5) {
        ReportIf_on_redraw(ReportIf, NULL);
    }
}

/*
Purpose: Found a base with a colony unit where it stands. The tile is levelled to rolling if rocky
         and the map view moves there when the player at the controls can see it. Founding inside
         another faction's territory without a pact or vendetta while either side is human ends a
         treaty (double cross) or else starts a vendetta, if the founder already has bases. The new
         base gets the given name (made unique) and, for a human founder, the unit's home base
         governor settings (plus the governor bit 0x80000000 for units with the state bit 0x200).
         The unit is used up, the map is redrawn if visible, the nearby sites of every faction are
         cleared and the founding is recorded in the monuments.
Original Offset: 004C96E0
Return Value: The new base id, or -1 if no base could be founded (the unit then skips its turn)
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch30.log)
*/
int __cdecl action_build(int veh_id, LPCSTR name) {
    Veh *veh = &Vehs[veh_id];
    uint32_t faction_id = veh->faction_id;
    int x = veh->x;
    int y = veh->y;
    log_say("Action Base", x, y, faction_id);
    log_say("Action Base Turn", *TurnCurrentNum, 0, 0);
    if ((map_loc(x, y)->val3 & 0xC0) > 0x40) {
        rocky_set(x, y, 1);
    }
    if (faction_id == (uint32_t)*FactionIDCurrent
        || (veh->faction_id == *FactionIDCurrent && (veh->flags & 0x440) != 0x440)
        || (veh->visibility & (uint8_t)(1 << *FactionIDCurrent))
        || (*GameState & STATE_OMNISCIENT_VIEW)) {
        Console_focus(ConsoleVar, NULL, x, y, faction_id);
    }
    int owner = whose_territory(faction_id, x, y, NULL, false);
    if (PlayersData[faction_id].current_num_bases && owner >= 0 && owner != (int)faction_id) {
        uint32_t treaties = PlayersData[faction_id].diplo_treaties[owner];
        uint32_t humans = FactionsStatus[0];
        if (!(treaties & (DTREATY_PACT | DTREATY_VENDETTA))
            && ((humans & (1 << faction_id)) || (humans & (1 << owner)))) {
            if (treaties & DTREATY_TREATY) {
                double_cross(faction_id, owner, -1);
            } else {
                treaty_on(faction_id, owner, DTREATY_VENDETTA);
            }
        }
    }
    int base_id = base_init(faction_id, x, y);
    if (base_id < 0) {
        veh_skip(veh_id);
        return base_id;
    }
    if (FactionsStatus[0] & (uint8_t)(1 << faction_id)) {
        if (veh->home_base_id >= 0) {
            Bases[base_id].governor = Bases[veh->home_base_id].governor;
        }
        if (veh->state & VSTATE_UNK_200) {
            Bases[base_id].governor |= 0x80000000;
        }
    }
    PlayersData[faction_id].last_turn_new_base = *TurnCurrentNum;
    if (name && strlen(name)) {
        strcpy(Bases[base_id].name_string, name); // unbounded, as in the original
        make_base_unique(base_id);
    }
    veh_kill(veh_id);
    if (faction_id == (uint32_t)*FactionIDCurrent
        || Bases[base_id].faction_id_current == *FactionIDCurrent
        || (Bases[base_id].visibility & (uint8_t)(1 << *FactionIDCurrent))
        || (*GameState & STATE_OMNISCIENT_VIEW)) {
        draw_map(1);
    }
    for (uint32_t i = 1; i < MaxPlayerNum; i++) {
        del_site(i, 8, x, y, 2);
    }
    mon_colony_founded(faction_id, Bases[base_id].name_string);
    return base_id;
}

// The pictures of the tutorial's "MADE..." messages for a farm, a soil enricher, a mine and a solar
// collector (the sea versions use the farm, mine and solar ones): picture objects in the exe.
static void *const TutTerraformPicture[4] = { (void *)0x0079204C, (void *)0x0079A794,
    (void *)0x00776A50, (void *)0x007991F8 };

/*
Purpose: Work on a unit's terraforming job (terraform_id: TERRA_FARM .. TERRA_MONOLITH) on its tile.
         A road ordered on a road becomes a mag tube, and a farm ordered on a land farm a soil
         enricher, when the faction has that technology. A job takes twice its rate in work: solar
         collectors more on rocky ground; roads and tubes their own rate (a tube's once a road is
         there) plus 1 across a river, 2 for fungus (not deep sea) and 2 for a forest or else the
         rockiness; fungus removal half for a fungicidal unit; one less (rates over 3) for computer
         factions above Librarian. Each unit works 2 a turn (4 for super formers; x1.5 with the
         Weather Paradigm, or x2 on fungus with the Xenoempathy Dome) and the stack's units on the
         same job add theirs. With execute false only the turns still needed are worked out (the
         unit's own work counted only while it can still move). With execute true the unit takes the
         order, gathers the stack's past work on the job and uses its turn; no work is done on a base
         tile. A finished job ends the stack's orders for it, puts the improvement in place of those
         it excludes and brings its effects: sites, the climate and the map for the big works, the
         land raised, lowered or levelled, nearby bases' yields recomputed, a forest cleared on land
         handing its minerals to the unit's home (or nearest) base, a computer faction whose base got
         drier taking offence at a human terraformer, the player at the controls told of a borehole,
         the harvest and rainfall changes (and of units lost, a message the climate update before it
         keeps from ever showing), the tutorial's first "MADE..." message for an automated unit and,
         once a turn, the completion sound for a unit that isn't.
Original Offset: 004C9B00
Return Value: execute false: the turns still needed; execute true: 1 if the job was completed, else 0
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-sweep-batch32-unitterra.log).
        Two deliberate changes from the v2.1 exe, chosen by Kiousu (2026-10-18). A farm ordered on a
        land farm becomes a soil enricher only with the Soil Enricher's technology; the exe checks
        the Farm's, which has none (in play Console::terraform already refuses that order with
        HASFARM by the enricher's technology, so the exe's slip only showed in the turn estimate
        next to "Farm" on a farm tile, which counted the enricher's work). After raising or lowering
        land the rest of the job reads the former's own state and order; the exe reads its slot
        again, which another unit moves into when the change loses units listed before it on the
        neighbouring tiles (ships where coastal sea is lifted into land, land units and bases where
        land sinks below sea level), so the completion chime went by that other unit.
*/
int __cdecl action_terraform(int veh_id, int terraform_id, BOOL execute) {
    const int veh_count = *VehCurrentCount;
    Veh *veh = &Vehs[veh_id];
    const int x = veh->x;
    const int y = veh->y;
    const uint32_t faction_id = veh->faction_id;
    const uint32_t state = veh->state; // the unit's own (see the end)
    Map *tile = map_loc(x, y);
    const BOOL is_sea = (tile->climate & 0xE0) < ALT_BIT_SHORE_LINE;
    if (terraform_id == TERRA_ROAD) {
        if (tile->bit & BIT_ROAD
            && has_tech(*(&Terraforming[TERRA_MAGTUBE].preq_tech + is_sea), faction_id)) {
            terraform_id = TERRA_MAGTUBE;
        }
    } else if (terraform_id == TERRA_FARM && tile->bit & BIT_FARM && !is_sea
        && has_tech(Terraforming[TERRA_SOIL_ENR].preq_tech, faction_id)) {
        terraform_id = TERRA_SOIL_ENR; // the exe checks Terraforming[TERRA_FARM].preq_tech
    }
    if (execute) {
        veh->order = (int8_t)(terraform_id + ORDER_FARM);
    }
    int rate = Terraforming[terraform_id].rate;
    switch (terraform_id) {
      case TERRA_SOLAR_TIDAL:
        rate += (int)(tile->val3 >> 6) * rate / 2;
        break;
      case TERRA_ROAD:
      case TERRA_MAGTUBE:
      {
        uint32_t bit = tile->bit;
        rate = Terraforming[(bit & BIT_ROAD) ? TERRA_MAGTUBE : TERRA_ROAD].rate;
        if (bit & BIT_RIVER) {
            rate++;
        }
        if (bit & BIT_FUNGUS && (tile->climate & 0xE0) >= ALT_BIT_OCEAN_SHELF) {
            rate += 2;
        }
        rate += (bit & BIT_FOREST) ? 2 : tile->val3 >> 6;
        break;
      }
      case TERRA_REMOVE_FUNGUS:
        if (has_abil(veh->proto_id, ABL_FUNGICIDAL)) {
            rate /= 2;
        }
        break;
      default:
        break;
    }
    if (!(*FactionsStatus & (1 << faction_id)) && *DiffLevelCurrent > 3 && rate > 3) {
        rate--;
    }
    int work = 0; // this turn's, of every unit on the job
    int done = 0; // so far
    if (veh_id >= 0) {
        int veh_id_top = veh_id;
        while (Vehs[veh_id_top].prev_veh_id_stack >= 0) {
            veh_id_top = Vehs[veh_id_top].prev_veh_id_stack;
        }
        for (int v = veh_id_top; v >= 0; v = Vehs[v].next_veh_id_stack) {
            if (v == veh_id || Vehs[v].order != terraform_id + ORDER_FARM) {
                continue;
            }
            if (!execute) {
                work += contribution(v, terraform_id);
                done += Vehs[v].terraforming_turns;
            } else {
                veh->terraforming_turns += Vehs[v].terraforming_turns;
                Vehs[v].terraforming_turns = 0;
            }
        }
    }
    work += contribution(veh_id, terraform_id);
    done += veh->terraforming_turns;
    if (!execute) {
        if ((int)(speed(veh_id, false) - veh->moves_expended) > 0) {
            done += work;
        }
        int left = rate * 2 - done;
        if (work < 1) {
            work = 1;
        } else if (work > 255) {
            return (left + 254) / 255;
        }
        return (left + work - 1) / work;
    }
    veh_skip(veh_id);
    uint8_t turns = (uint8_t)(veh->terraforming_turns + work);
    veh->terraforming_turns = turns;
    tile = map_loc(x, y);
    if (tile->bit & BIT_BASE_IN_TILE && (tile->val2 & 0xF) < 8) {
        veh->order = ORDER_NONE;
        return 0;
    }
    if (turns < rate * 2) {
        draw_tile(x, y, 2);
        BaseWin_check_veh(BaseWinVar, NULL, veh_id);
        return 0;
    }
    veh->terraforming_turns = 0;
    if (veh_id >= 0) {
        int veh_id_top = veh_id;
        while (Vehs[veh_id_top].prev_veh_id_stack >= 0) {
            veh_id_top = Vehs[veh_id_top].prev_veh_id_stack;
        }
        for (int v = veh_id_top; v >= 0; v = Vehs[v].next_veh_id_stack) {
            if (Vehs[v].order == terraform_id + ORDER_FARM) {
                Vehs[v].order = ORDER_NONE;
            }
        }
    }
    // Raising or lowering the land can lose units on the neighbouring tiles; one listed before this
    // unit moves another unit into its slot. What follows reads the unit's own order and state (the
    // exe reads the slot again).
    const int8_t order = veh->order;
    const BOOL had_forest = (tile->bit & BIT_FOREST) != 0;
    bit_set(x, y, Terraforming[terraform_id].bit_incompatible, false);
    bit_set(x, y, Terraforming[terraform_id].bit, true);
    synch_bit(x, y, faction_id);
    switch (terraform_id) {
      case TERRA_REMOVE_FUNGUS:
      case TERRA_PLANT_FUNGUS:
        site_radius(x, y, 0);
        break;
      case TERRA_CONDENSER:
      case TERRA_THERMAL_BORE:
        site_radius(x, y, 0);
        world_climate();
        draw_map(1);
        break;
      case TERRA_AQUIFER:
        bit_set(x, y, BIT_RIVERBED, true);
        world_climate();
        draw_map(1);
        break;
      case TERRA_RAISE_LAND:
        world_raise_alt(x, y);
        world_climate();
        draw_map(1);
        break;
      case TERRA_LOWER_LAND:
        world_lower_alt(x, y);
        world_climate();
        draw_map(1);
        break;
      case TERRA_LEVEL_TERRAIN:
      {
        uint8_t val3 = map_loc(x, y)->val3;
        if (val3 & 0xC0) {
            rocky_set(x, y, (uint8_t)((val3 >> 6) - 1));
            site_radius(x, y, 0);
        }
        break;
      }
      default:
        break;
    }
    if (Terraforming[terraform_id].bit
        & (BIT_THERMAL_BORE | BIT_FOREST | BIT_FARM | BIT_SOLAR_TIDAL | BIT_MINE)) {
        int base_id = base_find(x, y, faction_id);
        if (base_id >= 0 && *BaseFindDist <= 2) {
            Bases[base_id].state |= BSTATE_UNK_100;
        }
        if (terraform_id == TERRA_THERMAL_BORE && faction_id == (uint32_t)*FactionIDCurrent
            && base_id >= 0 && Bases[base_id].faction_id_current == faction_id) {
            parse_says(0, Bases[base_id].name_string, -1, -1);
            NetMsg_pop(NetMsgVar, NULL, "BOREHOLEMINE", 5000, 0, 0);
        }
    }
    tile = map_loc(x, y);
    if (had_forest && !(tile->bit & BIT_FOREST) && (tile->climate & 0xE0) >= ALT_BIT_SHORE_LINE
        && terraform_id != TERRA_RAISE_LAND) {
        int base_id = veh->home_base_id;
        if (base_id >= 0 || (base_id = base_find(x, y, faction_id)) >= 0) {
            Base *base = &Bases[base_id];
            base->minerals_accumulated += Rules->minerals_harvesting_forest;
            base->minerals_accumulated_2 = base->minerals_accumulated;
            base->production_id_last = base->queue_production_id[0];
            if (faction_id == (uint32_t)*FactionIDCurrent && *FactionsStatus & (1 << faction_id)
                && Rules->minerals_harvesting_forest) {
                parse_says(0, base->name_string, -1, -1);
                parse_num(0, Rules->minerals_harvesting_forest);
                NetMsg_pop(NetMsgVar, NULL, "HARVESTFOREST", 5000, 0, 0);
            }
            BaseWin_check_base(BaseWinVar, NULL, base_id);
        }
    }
    if (terraform_id == TERRA_RAISE_LAND || terraform_id == TERRA_LOWER_LAND
        || terraform_id == TERRA_CONDENSER) {
        int faction_id_dry = *MapRainfallChangeFactionID; // a computer faction whose base got drier
        if (faction_id_dry >= 0 && !(*FactionsStatus & (1 << faction_id_dry))
            && *FactionsStatus & (1 << faction_id)
            && !(PlayersData[faction_id].diplo_treaties[faction_id_dry]
                & (DTREATY_PACT | DTREATY_VENDETTA))) {
            if (PlayersData[faction_id_dry].diplo_treaties[faction_id] & DTREATY_WANT_REVENGE) {
                set_treaty(faction_id_dry, faction_id, DTREATY_SHALL_BETRAY, true);
            } else {
                cause_friction(faction_id_dry, faction_id, 5);
            }
        }
        if (*MapRainfallChangeBaseID >= 0) {
            parse_says(0, Bases[*MapRainfallChangeBaseID].name_string, -1, -1);
            parse_says(1, Players[faction_id].adj_name_faction, -1, -1);
            NetMsg_pop(NetMsgVar, NULL, (faction_id == (uint32_t)*FactionIDCurrent) ? "TERRAMINE"
                : "TERRAYOURS", 5000, 0, 0);
        }
        // Units of the player at the controls lost to the land changes. world_climate() above has
        // already reset this record: world_shorelines() goes through world_alt_set() for nearly
        // every tile, so it only holds what the last of those calls lost, practically nothing. The
        // base is the one nearest the terraforming faction's losses, as in the original.
        if (MapBaseIdClosestSubmergedVeh[*FactionIDCurrent] >= 0) {
            parse_says(0, Bases[MapBaseIdClosestSubmergedVeh[faction_id]].name_string, -1, -1);
            parse_says(1, Players[faction_id].adj_name_faction, -1, -1);
            NetMsg_pop(NetMsgVar, NULL, (faction_id == (uint32_t)*FactionIDCurrent) ? "DROWNMINE"
                : "DROWNYOURS", 5000, 0, 0);
        }
    }
    synch_bit(x, y, faction_id);
    draw_tiles(x, y, 2);
    if (veh_count == *VehCurrentCount) {
        BaseWin_check_veh(BaseWinVar, NULL, veh_id);
        BaseWin_check_loc(BaseWinVar, NULL, x, y, 2);
    } else {
        GraphicWin_redraw(BaseWinVar, NULL);
    }
    if (faction_id != (uint32_t)*FactionIDCurrent) {
        return 1;
    }
    if (state & VSTATE_UNK_200 && *GamePreferences & PREF_BSC_TUTORIAL_MSGS && !*IsMultiplayerNet) {
        uint32_t mask = 1u << terraform_id;
        uint32_t shown = TutTerraformShown[is_sea];
        if (!(shown & mask)) {
            TutTerraformShown[is_sea] = shown | mask;
            LPCSTR label = NULL;
            void *picture = NULL;
            int veh_id_shown = veh_id;
            if (!is_sea) {
                switch (terraform_id) {
                  case TERRA_FARM: label = "MADEFARM"; break;
                  case TERRA_SOIL_ENR: label = "MADESOIL"; break;
                  case TERRA_MINE: label = "MADEMINE"; break;
                  case TERRA_SOLAR_TIDAL: label = "MADESOLAR"; break;
                  case TERRA_FOREST: label = "MADEFOREST"; break;
                  case TERRA_ROAD: label = "MADEROAD"; break;
                  case TERRA_MAGTUBE: label = "MADETUBE"; break;
                  case TERRA_SENSOR: label = "MADESENSOR"; break;
                  case TERRA_CONDENSER: label = "MADECONDENSER"; break;
                  case TERRA_THERMAL_BORE: label = "MADEBOREHOLE"; break;
                  case TERRA_AQUIFER: label = "MADEAQUIFER"; break;
                  default: break;
                }
            } else {
                switch (terraform_id) {
                  case TERRA_FARM: label = "MADESEAFARM"; break;
                  case TERRA_MINE: label = "MADESEAMINE"; break;
                  case TERRA_SOLAR_TIDAL: label = "MADESEASOLAR"; break;
                  default: break;
                }
            }
            if (terraform_id <= TERRA_SOLAR_TIDAL) {
                picture = TutTerraformPicture[terraform_id]; // these point at the tile, not the unit
                veh_id_shown = -1;
            }
            if (label) {
                TutWin_tut_map(TutWinVar, NULL, label, x, y, veh_id_shown, picture, 1, -1, -1);
            } else {
                TutTerraformShown[is_sea] = shown; // only a message shown is remembered
            }
        }
    }
    if (faction_id == (uint32_t)*FactionIDCurrent && order == ORDER_NONE
        && !(state & VSTATE_UNK_200) && !*TerraformSoundPlayed) {
        *TerraformSoundPlayed = 1;
        wave_it(20);
        ambience(0xCF);
    }
    return 1;
}

/*
Purpose: Fire a unit's artillery at a terrain improvement, or a bomber at the tile below, or let a
         native unit or a unit without a target tile destroy one at its own tile: bits picks the
         improvements (any when 0 or less). The unit uses its turn. Shelling a tile (x, y on the map)
         at a base only shows the explosion to the attacker at the controls; half the time it fails;
         otherwise the explosion shows when the attacker or the tile's owner is at the controls, and
         when the tile lies in another faction's territory a stack of the attacker's enemies within
         reach near the tile draws the fire instead (a battle in that direction). A bomber fails a
         quarter of the time. The first improvement found (the sensor array first; fungus only for a
         fungicidal unit) is removed and named (parse slot 2), every view of the tile updated and
         the map redrawn (the climate for a condenser, mirror or borehole). A faction in contact
         with the tile's owner which sees the tile makes that owner set a goal at the attacker, and
         the owner at the controls hears of it; native units tell the player at the controls about
         improvements in its territory near its bases. The unit's order is then cleared (order_veh,
         left in the exe), except after a failed shelling or a battle.
Original Offset: 004CAA50
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-sweep-batch32-unitterra.log)
*/
void __cdecl action_destroy(int veh_id, int bits, int x, int y) {
    BOOL is_shelling = false;
    BOOL ate_forest = false;
    const uint32_t faction_id = Vehs[veh_id].faction_id;
    veh_skip(veh_id);
    Vehs[veh_id].state |= VSTATE_UNK_40;
    if (y < 0 || y >= *MapLatitudeBounds || x < 0 || x >= *MapLongitudeBounds) {
        x = Vehs[veh_id].x;
        y = Vehs[veh_id].y;
    } else {
        Map *tile = map_loc(x, y);
        if (tile->bit & BIT_BASE_IN_TILE && (tile->val2 & 0xF) < 8) {
            if (faction_id == (uint32_t)*FactionIDCurrent) {
                boom(x, y, 0x80);
            }
            return;
        }
        if (_rand() % 2) {
            if (faction_id == (uint32_t)*FactionIDCurrent) {
                NetMsg_pop(NetMsgVar, NULL, "ARTYBOMBFAILED", 5000, 0, 0);
            }
            return;
        }
        is_shelling = true;
        if (faction_id == (uint32_t)*FactionIDCurrent
            || whose_territory(*FactionIDCurrent, x, y, NULL, false) == *FactionIDCurrent) {
            if (faction_id && faction_id != (uint32_t)*FactionIDCurrent) {
                boom(x, y, 0x80);
                boom(Vehs[veh_id].x, Vehs[veh_id].y, 0x80);
            }
            boom(x, y, 0x80);
        }
        int owner = whose_territory(faction_id, x, y, NULL, false);
        if (owner != (int)faction_id) {
            for (int i = 1; i < 25; i++) {
                int x_radius = Vehs[veh_id].x + RadiusOffsetX[i];
                if (!(*MapIsFlat & 1)) {
                    if (x_radius < 0) {
                        x_radius += *MapLongitudeBounds;
                    } else if (x_radius >= *MapLongitudeBounds) {
                        x_radius -= *MapLongitudeBounds;
                    }
                }
                int y_radius = Vehs[veh_id].y + RadiusOffsetY[i];
                if (y_radius < 0 || y_radius >= *MapLatitudeBounds || x_radius < 0
                    || x_radius >= *MapLongitudeBounds) {
                    continue;
                }
                int dist_x = abs(x_radius - x);
                if (!(*MapIsFlat & 1) && dist_x > (int)*MapLongitude) {
                    dist_x = *MapLongitudeBounds - dist_x;
                }
                if (((abs(y_radius - y) + dist_x) & ~1) > 4) {
                    continue;
                }
                int veh_id_def = stack_fix(veh_at(x_radius, y_radius));
                if (veh_id_def >= 0 && stack_check(veh_id_def, 15, owner, -1, -1)) {
                    battle_fight(veh_id, i, true, 1, 0);
                    return;
                }
            }
        }
    }
    const RulesChassis *chassis = &Chassis[VehPrototypes[Vehs[veh_id].proto_id].chassis_id];
    if (chassis->triad == TRIAD_AIR && !chassis->missile && !(_rand() % 4)) {
        if (faction_id == (uint32_t)*FactionIDCurrent) {
            NetMsg_pop(NetMsgVar, NULL, "BOMBFAILED", 5000, 0, 0);
        }
        order_veh(veh_id, -1, 3);
        return;
    }
    uint32_t found_bits; // the improvements looked for
    if (bits > 0) {
        bits &= map_loc(x, y)->bit;
        found_bits = bits;
        if (!bits) {
            order_veh(veh_id, -1, 3);
            return;
        }
        bit_set(x, y, bits, false);
    } else {
        found_bits = map_loc(x, y)->bit;
    }
    if (!has_abil(Vehs[veh_id].proto_id, ABL_FUNGICIDAL)) {
        found_bits &= ~BIT_FUNGUS;
    }
    int terraform_id = -1;
    for (int i = -1; i < TERRA_MONOLITH; i++) {
        int id = (i < 0) ? TERRA_SENSOR : i; // the sensor array first
        if (Terraforming[id].bit & found_bits) {
            terraform_id = id;
            break;
        }
    }
    if (terraform_id < 0) {
        log_say("Arty Failed The Hard Way", bits, (int)found_bits, 0);
        if (is_shelling) {
            if (faction_id == (uint32_t)*FactionIDCurrent) {
                NetMsg_pop(NetMsgVar, NULL, "ARTYBOMBFAILED", 5000, 0, 0);
            }
        } else if (chassis->triad == TRIAD_AIR && faction_id == (uint32_t)*FactionIDCurrent) {
            NetMsg_pop(NetMsgVar, NULL, "BOMBFAILED", 5000, 0, 0);
        }
        order_veh(veh_id, -1, 3);
        return;
    }
    bits |= Terraforming[terraform_id].bit;
    bit_set(x, y, Terraforming[terraform_id].bit, false);
    BOOL is_sea = (map_loc(x, y)->climate & 0xE0) < ALT_BIT_SHORE_LINE;
    parse_says(2, StringTable->get((int)*(&Terraforming[terraform_id].name + is_sea)), -1, -1);
    if (terraform_id == TERRA_FOREST) {
        ate_forest = true;
    }
    if (chassis->triad == TRIAD_AIR && faction_id == (uint32_t)*FactionIDCurrent) {
        NetMsg_pop(NetMsgVar, NULL, "BOMBSUCCEEDED", 5000, 0, 0);
    }
    synch_bit(x, y, faction_id);
    int owner = whose_territory(faction_id ? faction_id : *FactionIDCurrent, x, y, NULL, false);
    synch_bit(x, y, owner); // with no owner (-1) this writes the tile's own bits back to them
    if (bits & (BIT_CONDENSER | BIT_ECH_MIRROR | BIT_THERMAL_BORE)) {
        world_climate();
        draw_map(1);
    } else if (bits & BIT_SENSOR_ARRAY && owner == *FactionIDCurrent) {
        draw_radius(x, y, 2, 2);
    } else {
        draw_tiles(x, y, 2);
    }
    if (faction_id) {
        if (is_shelling && faction_id == (uint32_t)*FactionIDCurrent) {
            NetMsg_pop(NetMsgVar, NULL, "BOMBARDSUCCEEDED", 5000, 0, 0);
        }
        if (owner >= 0 && owner != (int)faction_id
            && PlayersData[faction_id].diplo_treaties[owner] & DTREATY_COMMLINK
            && (PlayersData[owner].flags & PFLAG_MAP_REVEALED
                || map_loc(x, y)->visibility & (1 << owner))) {
            synch_bit(x, y, owner);
            add_goal(owner, 0, 5, Vehs[veh_id].x, Vehs[veh_id].y, -1);
            if (owner == *FactionIDCurrent) {
                int base_id = base_find(x, y, -1, -1, -1, *FactionIDCurrent);
                *GenderDefault = Players[faction_id].noun_gender;
                *PluralityDefault = Players[faction_id].is_noun_plural;
                parse_says(0, Players[faction_id].noun_faction, -1, -1);
                parse_says(1, Bases[base_id].name_string, -1, -1); // the owner has a base here
                parse_says(3, Players[faction_id].adj_name_faction, -1, -1);
                NetMsg_pop(NetMsgVar, NULL, is_shelling ? "HAVEDESTROYED1" : "HAVEDESTROYED", 5000,
                    0, 0);
            }
        }
    } else {
        int faction_id_cur = *FactionIDCurrent;
        if ((PlayersData[faction_id_cur].flags & PFLAG_MAP_REVEALED
            || map_loc(x, y)->visibility & (1 << faction_id_cur)) && owner == faction_id_cur) {
            int base_id = base_find(x, y, -1, -1, -1, faction_id_cur);
            if (base_id >= 0 && Bases[base_id].faction_id_current == faction_id_cur) {
                parse_says(0, Bases[base_id].name_string, -1, -1);
                parse_says(1, VehPrototypes[Vehs[veh_id].proto_id].veh_name, -1, -1);
                Console_focus(ConsoleVar, NULL, x, y, 0);
                BOOL is_spore = Vehs[veh_id].proto_id == BSC_SPORE_LAUNCHER;
                if (is_spore) {
                    popp(ScriptTxtID, ate_forest ? "SPOREFOREST" : "SPORESLAUNCHED", 0,
                        "sporlnch_sm.pcx", NULL);
                } else {
                    NetMsg_pop(NetMsgVar, NULL, ate_forest ? "ATEFOREST" : "ATESTUFF", 5000, 0, 0);
                }
            }
        }
    }
    order_veh(veh_id, -1, 3);
}

/*
Purpose: Find where a unit should go to rest or refuel and send it there (a move order, or none
         when it is there already; a computer faction's unit then skips its turn). Modes 1
         (order_veh()) and -1 first ask whether the fuel left runs short and stop if not, -1 then
         only answers; mode 4 gives up when the place is out of reach; mode 2 leaves out bases with
         the state bit 0x200000. Bases of the unit's faction or a pact partner count (for land units
         in their region, for ships on their sea or under them); the nearest wins, unless an
         automated unit on alert can still reach its home spot (second waypoint). Aircraft also
         count their faction's carriers and air supply units (in modes 1, -1 and 4 a computer
         faction adds a carrier's own move, and a supply unit must be within this turn's moves)
         and the airbases within the distance found that others' units don't hold (a computer
         faction's only where the tile is its own, in its territory). Reach is what the fuel (and
         for one-turn aircraft the hit points) leaves; a human player's aircraft isn't sent beyond.
Original Offset: 004CBAA0
Return Value: 1 when a place was found (the order given, mode -1 only answers), 0 when none, while
              the fuel lasts (modes 1 and -1) or when it is out of reach (mode 4)
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-sweep-batch32-unitterra.log).
        Deliberate change from the v2.1 exe, chosen by Kiousu (2026-10-18): an aircraft on alert
        heading back to its home airbase is checked against that airbase's distance; the exe keeps
        the shortest distance found before it (bases, carriers, air supply units and airbases
        scanned earlier), which can be longer or shorter than the airbase's.
*/
int __cdecl action_home(int veh_id, int mode) {
    Veh *veh = &Vehs[veh_id];
    const int x = veh->x;
    const int y = veh->y;
    const uint32_t faction_id = veh->faction_id;
    const uint32_t region = map_loc(x, y)->region;
    int dist_best = 9999;
    int x_best = -1;
    int y_best = -1;
    VehPrototype *proto = &VehPrototypes[veh->proto_id];
    RulesChassis *chassis = &Chassis[proto->chassis_id];
    int moves = range((int)speed(veh_id, false) - veh->moves_expended, 0, 999);
    int reach = (int)speed(veh_id, false) * (chassis->range - veh->terraforming_turns - 1) + moves;
    if (chassis->range == 1) {
        int hp = (proto->plan == PLAN_ALIEN_ARTIFACT) ? 1 : range(proto->reactor_id, 1, 100) * 10;
        hp = range(hp - veh->dmg_incurred, 0, 9999);
        reach += (int)speed(veh_id, false) * ((hp - 1) / proto->reactor_id);
    }
    reach /= (int)Rules->move_rate_roads;
    for (int i = 0; i < *BaseCurrentCount; i++) {
        Base *base = &Bases[i];
        if (base->faction_id_current != faction_id
            && !(PlayersData[faction_id].diplo_treaties[base->faction_id_current] & DTREATY_PACT)) {
            continue;
        }
        uint8_t triad = Chassis[VehPrototypes[veh->proto_id].chassis_id].triad;
        if (triad == TRIAD_LAND) {
            if (map_loc(base->x, base->y)->region != region) {
                continue;
            }
        } else if (triad == TRIAD_SEA) {
            if ((base->x != x || base->y != y) && !base_on_sea(i, region)) {
                continue;
            }
        }
        if (mode == 2 && base->state & BSTATE_UNK_200000) {
            continue;
        }
        int dist_x = abs(x - base->x);
        if (!(*MapIsFlat & 1) && dist_x > (int)*MapLongitude) {
            dist_x = *MapLongitudeBounds - dist_x;
        }
        int dist = (abs(y - base->y) + dist_x) >> 1;
        if (veh->state & VSTATE_UNK_200 && veh->order_auto_type == ORDERA_ON_ALERT
            && base->x == veh->waypoint_x[1] && base->y == veh->waypoint_y[1] && dist <= reach) {
            dist_best = dist;
            x_best = base->x;
            y_best = base->y;
            goto found;
        }
        if (dist < dist_best) {
            dist_best = dist;
            y_best = base->y;
            x_best = base->x;
        }
    }
    if (Chassis[VehPrototypes[veh->proto_id].chassis_id].triad == TRIAD_AIR) {
        for (int i = 0; i < *VehCurrentCount; i++) {
            Veh *veh_dst = &Vehs[i];
            if (veh_dst->faction_id != faction_id || i == veh_id) {
                continue;
            }
            VehPrototype *proto_dst = &VehPrototypes[veh_dst->proto_id];
            if (!has_abil(veh_dst->proto_id, ABL_CARRIER) && (proto_dst->plan != PLAN_SUPPLY_CONVOY
                || Chassis[proto_dst->chassis_id].triad != TRIAD_AIR)) {
                continue;
            }
            int dist_x = abs(x - veh_dst->x);
            if (!(*MapIsFlat & 1) && dist_x > (int)*MapLongitude) {
                dist_x = *MapLongitudeBounds - dist_x;
            }
            int dist = (abs(y - veh_dst->y) + dist_x) >> 1;
            if (mode & 1 || mode == 4) {
                if (!(*FactionsStatus & (1 << faction_id))) {
                    dist += (int)speed(i, false) / (int)Rules->move_rate_roads; // it can come closer
                }
                if (proto_dst->plan == PLAN_SUPPLY_CONVOY) {
                    int dist_x_dst = abs(x - veh_dst->x);
                    if (!(*MapIsFlat & 1) && dist_x_dst > (int)*MapLongitude) {
                        dist_x_dst = *MapLongitudeBounds - dist_x_dst;
                    }
                    int moves_left = range((int)speed(veh_id, false) - veh->moves_expended, 0, 999);
                    if ((abs(y - veh_dst->y) + dist_x_dst) >> 1
                        > moves_left / (int)Rules->move_rate_roads) {
                        continue;
                    }
                }
            }
            if (dist < dist_best) {
                dist_best = dist;
                x_best = veh_dst->x;
                y_best = veh_dst->y;
            }
        }
        int radius = (dist_best >= 8) ? 8 : dist_best;
        for (int i = 0; i < RadiusRange[radius]; i++) {
            int x_radius = x + RadiusOffsetX[i];
            if (!(*MapIsFlat & 1)) {
                if (x_radius < 0) {
                    x_radius += *MapLongitudeBounds;
                } else if (x_radius >= *MapLongitudeBounds) {
                    x_radius -= *MapLongitudeBounds;
                }
            }
            int y_radius = y + RadiusOffsetY[i];
            if (y_radius < 0 || y_radius >= *MapLatitudeBounds || x_radius < 0
                || x_radius >= *MapLongitudeBounds) {
                continue;
            }
            Map *tile = map_loc(x_radius, y_radius);
            if (!(tile->bit & BIT_AIRBASE)) {
                continue;
            }
            if (tile->bit & BIT_VEH_IN_TILE) {
                uint32_t owner = tile->val2 & 0xF;
                if (owner < 8 && owner != faction_id
                    && !(PlayersData[faction_id].diplo_treaties[owner] & DTREATY_PACT)) {
                    continue;
                }
            }
            if (!(*FactionsStatus & (1 << faction_id))) {
                if (whose_territory(faction_id, x_radius, y_radius, NULL, false) != (int)faction_id) {
                    continue;
                }
                uint32_t owner = tile->val2 & 0xF;
                if (owner >= 8 || owner != faction_id) {
                    continue;
                }
            }
            int dist_x = abs(x - x_radius);
            if (!(*MapIsFlat & 1) && dist_x > (int)*MapLongitude) {
                dist_x = *MapLongitudeBounds - dist_x;
            }
            int dist = (abs(y - y_radius) + dist_x) >> 1;
            if (veh->state & VSTATE_UNK_200 && veh->order_auto_type == ORDERA_ON_ALERT
                && !veh->dmg_incurred && x_radius == veh->waypoint_x[1]
                && y_radius == veh->waypoint_y[1] && dist <= reach) {
                dist_best = dist; // the exe keeps the shortest distance found before this one
                x_best = x_radius;
                y_best = y_radius;
                goto found;
            }
            if (dist < dist_best) {
                dist_best = dist;
                x_best = x_radius;
                y_best = y_radius;
            }
        }
    }
    if ((mode & 1 || mode == 4) && !Chassis[VehPrototypes[veh->proto_id].chassis_id].range) {
        return 0;
    }
found:
    if (y_best < 0 || y_best >= *MapLatitudeBounds || x_best < 0 || x_best >= *MapLongitudeBounds) {
        return 0;
    }
    if (mode & 1 && dist_best + 1 <= reach - 1) {
        return 0; // fuel enough for now
    }
    if (mode == 4) {
        if (reach < dist_best) {
            return 0;
        }
    } else if (mode < 0) {
        return 1;
    }
    if (x_best == x && y_best == y) {
        veh->order = ORDER_NONE;
        if (!(*FactionsStatus & (1 << faction_id))) {
            veh_skip(veh_id);
        }
        return 1;
    }
    veh->order = ORDER_MOVE_TO;
    veh->waypoint_x[0] = (int16_t)x_best;
    veh->waypoint_y[0] = (int16_t)y_best;
    if (Chassis[VehPrototypes[veh->proto_id].chassis_id].range && *FactionsStatus & (1 << faction_id)
        && reach < dist_best) {
        veh->order = ORDER_NONE; // out of reach: a human player's unit doesn't go
        veh->state &= ~(VSTATE_UNK_200 | VSTATE_EXPLORE | VSTATE_UNK_1000000 | VSTATE_UNK_2000000);
    }
    return 1;
}

/*
Purpose: Fire a unit's artillery at a tile at most two steps away (the summed distances, rounded
         down to even, at most 4). A unit of the player at the controls must still be able to move.
         An empty tile is shelled through action_destroy(); a stack of another faction without a
         pact is attacked through battle_fight() in the direction radius_move() finds. In a network
         game the unit is locked first and the other players are sent the action.
Original Offset: 004CD090
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch30.log)
*/
void __cdecl action_arty(int veh_id, int x, int y) {
    if (Vehs[veh_id].faction_id == *FactionIDCurrent && !veh_ready(veh_id)) {
        NetMsg_pop(NetMsgVar, NULL, "UNITMOVED", 5000, 0, 0);
        return;
    }
    int dist_x = abs(Vehs[veh_id].x - x);
    if (!(*MapIsFlat & 1) && dist_x > (int)*MapLongitude) {
        dist_x = *MapLongitudeBounds - dist_x;
    }
    int dist_y = abs(Vehs[veh_id].y - y);
    if (((dist_x + dist_y) & ~1) > 4) {
        NetMsg_pop(NetMsgVar, NULL, "OUTOFRANGE", 5000, 0, 0);
        return;
    }
    int veh_id_tgt = stack_fix(veh_at(x, y));
    if (veh_id_tgt < 0) {
        if (!*IsMultiplayerNet) {
            action_destroy(veh_id, 0, x, y);
        } else if (!NetDaemon_lock_veh(NetDaemonVar, NULL, &veh_id, 4, x, y, 0)) {
            message_data(0x2413, 0, veh_id, x, y, 0);
            NetDaemon_await_exec(NetDaemonVar, NULL, 1);
        }
        return;
    }
    uint32_t faction_id = Vehs[veh_id].faction_id;
    uint32_t faction_id_tgt = Vehs[veh_id_tgt].faction_id;
    if (faction_id_tgt == faction_id
        || (PlayersData[faction_id].diplo_treaties[faction_id_tgt] & DTREATY_PACT)) {
        return;
    }
    // the exe's own copy of RadiusRange[] (0x0066F8C4): same values for 0-8, and a modded range of
    // 9+ reads whatever follows that table in the exe, as vanilla does
    int offset = radius_move(Vehs[veh_id].x, Vehs[veh_id].y, x, y,
        ((int *)0x0066F8C4)[Rules->artillery_max_rng]);
    if (offset < 0) {
        return;
    }
    if (!*IsMultiplayerNet) {
        *VehAttackFlags = 3;
        battle_fight(veh_id, offset, true, 1, 0);
        return;
    }
    if (NetDaemon_maybe_lock(NetDaemonVar, NULL, &veh_id, 0, 0, x, y, 4)) {
        return;
    }
    *VehAttackFlags = 1;
    if (battle_fight(veh_id, offset, true, 1, 0)) {
        NetDaemon_unlock_veh(NetDaemonVar, NULL);
        return;
    }
    message_veh(0x2402, veh_id, offset, 0);
    NetDaemon_await_exec(NetDaemonVar, NULL, 1);
}

// The map tile at (x, y), computed as the exe does it (signed, without a bounds check).
static inline Map *order_tile(int x, int y) {
    return &(*MapTiles)[(int)*MapLongitude * y + (x >> 1)];
}

/*
Purpose: Carry a unit's move order one step further. An ORDER_MOVE steps in the direction stored
         in its first waypoint; the other move orders take Path::move()'s next step toward the first
         waypoint (an order with the bit 0x40, the AI's, plans without a faction). Without a step the
         order ends. After the step an ORDER_MOVE ends on its second waypoint and an ORDER_MOVE_TO on
         its first; a multi-waypoint go to (state bits 0x2000200) then moves on to its next waypoint
         instead, the waypoints shifted down by one (the copy reads index waypoint_count, past the
         four-slot arrays when there are four, as in the original; the map view's waypoint mode
         stops at three). A computer faction's unit whose chassis has a cargo rating (every chassis
         but the missile) that arrives in a base ends its turn. In a network game the step goes
         through NetDaemon::order_veh() and the unit id is kept where veh_kill() can renumber it.
Original Offset: 004CB310
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch32-unitmove.log)
*/
void __cdecl action_go_to(int veh_id) {
    if (*IsMultiplayerNet && !*ControlTurnC) {
        *NetVehIDRef = &veh_id; // veh_kill() renumbers it; every later use reads it again
    }
    int dir;
    if (Vehs[veh_id].order == ORDER_MOVE) {
        dir = Vehs[veh_id].waypoint_x[0];
    } else {
        dir = Path_move(Paths, NULL, veh_id, (Vehs[veh_id].order & 0x40) ? -1 : Vehs[veh_id].faction_id);
    }
    if (dir < 0) {
        Vehs[veh_id].order = ORDER_NONE;
        if (*IsMultiplayerNet && !*ControlTurnC) {
            synch_veh(veh_id);
            NetDaemon_await_synch(NetDaemonVar, NULL);
            NetDaemon_unlock_veh(NetDaemonVar, NULL);
        }
        return;
    }
    if (*IsMultiplayerNet && !*ControlTurnC) {
        if (NetDaemon_order_veh(NetDaemonVar, NULL, veh_id, dir, 1)) {
            NetDaemon_await_exec(NetDaemonVar, NULL, 1);
        }
        return;
    }
    if (!order_veh(veh_id, dir, 3)) {
        return;
    }
    Veh *veh = &Vehs[veh_id];
    if (veh->order == ORDER_MOVE) {
        if (veh->x == veh->waypoint_x[1] && veh->y == veh->waypoint_y[1]) {
            veh->order = ORDER_NONE;
        }
        return;
    }
    if (veh->order != ORDER_MOVE_TO || veh->x != veh->waypoint_x[0] || veh->y != veh->waypoint_y[0]) {
        return;
    }
    veh->order = ORDER_NONE;
    if ((veh->state & (VSTATE_UNK_2000000 | VSTATE_UNK_200)) == (VSTATE_UNK_2000000 | VSTATE_UNK_200)
        && veh->patrol_current_point < veh->waypoint_count) {
        int16_t *waypoint = veh->waypoint_x; // waypoint_y[i] is waypoint[i + 4]
        for (int i = 0; i < veh->waypoint_count; i++) {
            waypoint[i] = waypoint[i + 1];
            waypoint[i + 4] = waypoint[i + 5];
        }
        veh->order = ORDER_MOVE_TO;
        if (!--veh->waypoint_count) {
            veh->state &= ~(VSTATE_UNK_2000000 | VSTATE_UNK_10000 | VSTATE_UNK_200);
        }
    }
    if (!(*FactionsStatus & (uint8_t)(1 << veh->faction_id))
        && Chassis[VehPrototypes[veh->proto_id].chassis_id].cargo) {
        Map *tile = order_tile(veh->x, veh->y);
        if ((tile->bit & BIT_BASE_IN_TILE) && (tile->val2 & 0xF) < 8) {
            veh_skip(veh_id);
        }
    }
}

/*
Purpose: Carry a unit's road to (ORDER_ROAD_TO) or tube to (ORDER_MAGTUBE_TO) order one step
         further. The order ends when the unit is at sea or the target off the map. On its way,
         outside bases, the unit first clears fungus it can't build roads through (if it knows how),
         then builds a road where there is none and, for a tube to, a magtube where there is a road
         but no tube (through action_terraform() with the order kept). On fungus it can't build in
         it just moves on.
         Otherwise it takes Path::find()'s next step toward the target (prototype 0, no faction,
         flags 0x10 without the roads-in-fungus tech); without one (0 to 7) the order ends. On
         arrival the order ends and the player is told if the unit is theirs and lacks the state
         bit 0x200. In a network game the work and steps go through the network daemon.
Original Offset: 004CB580
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch32-unitmove.log)
*/
void __cdecl action_road_to(int veh_id) {
    if (*IsMultiplayerNet && !*ControlTurnC) {
        *NetVehIDRef = &veh_id; // veh_kill() renumbers it; every later use reads it again
    }
    uint32_t faction_id = Vehs[veh_id].faction_id;
    int x_dst = Vehs[veh_id].waypoint_x[0];
    int y = Vehs[veh_id].y;
    int y_dst = Vehs[veh_id].waypoint_y[0];
    int x = Vehs[veh_id].x;
    Map *tile = order_tile(x, y);
    int altitude = tile->climate & 0xE0;
    if (altitude < 0x60 || y_dst < 0 || y_dst >= *MapLatitudeBounds || x_dst < 0
        || x_dst >= *MapLongitudeBounds) {
        Vehs[veh_id].order = ORDER_NONE;
        if (*IsMultiplayerNet && !*ControlTurnC) {
            synch_veh(veh_id);
            NetDaemon_await_synch(NetDaemonVar, NULL);
            NetDaemon_unlock_veh(NetDaemonVar, NULL);
        }
        return;
    }
    if ((tile->bit & BIT_FUNGUS) && altitude >= 0x40
        && !has_tech(Rules->tech_build_roads_fungus, faction_id)
        && has_tech(Terraforming[TERRA_REMOVE_FUNGUS].preq_tech, faction_id)
        && !((tile->bit & BIT_BASE_IN_TILE) && (tile->val2 & 0xF) < 8)) {
        if (*IsMultiplayerNet && !*ControlTurnC) {
            message_data(0x2405, 0, veh_id, ORDER_REMOVE_FUNGUS, Vehs[veh_id].order, 0);
            NetDaemon_await_exec(NetDaemonVar, NULL, 1);
            return;
        }
        int8_t order = Vehs[veh_id].order;
        action_terraform(veh_id, TERRA_REMOVE_FUNGUS, 1);
        Vehs[veh_id].order = order;
        return;
    }
    BOOL stuck_in_fungus = (tile->bit & BIT_FUNGUS) && (tile->climate & 0xE0) >= 0x40
        && !has_tech(Rules->tech_build_roads_fungus, faction_id);
    if (!stuck_in_fungus && !((tile->bit & BIT_BASE_IN_TILE) && (tile->val2 & 0xF) < 8)) {
        if (!(tile->bit & BIT_ROAD)) {
            if (*IsMultiplayerNet && !*ControlTurnC) {
                message_data(0x2405, 0, veh_id, ORDER_ROAD, Vehs[veh_id].order, 0);
                NetDaemon_await_exec(NetDaemonVar, NULL, 1);
                return;
            }
            int8_t order = Vehs[veh_id].order;
            action_terraform(veh_id, TERRA_ROAD, 1);
            Vehs[veh_id].order = order;
            return;
        }
        // action_terraform() makes the road order a magtube where there is a road already
        if (Vehs[veh_id].order == ORDER_MAGTUBE_TO && !(tile->bit & BIT_MAGTUBE)
            && has_tech(Terraforming[TERRA_MAGTUBE].preq_tech, faction_id)) {
            if (*IsMultiplayerNet && !*ControlTurnC) {
                message_data(0x2405, 0, veh_id, ORDER_ROAD, ORDER_MAGTUBE_TO, 0);
                NetDaemon_await_exec(NetDaemonVar, NULL, 1);
                return;
            }
            action_terraform(veh_id, TERRA_ROAD, 1);
            Vehs[veh_id].order = ORDER_MAGTUBE_TO;
            return;
        }
    }
    if (x == x_dst && y == y_dst) {
        int8_t order = Vehs[veh_id].order;
        Vehs[veh_id].order = ORDER_NONE;
        if (*IsMultiplayerNet && !*ControlTurnC) {
            synch_veh(veh_id);
            NetDaemon_await_synch(NetDaemonVar, NULL);
            NetDaemon_unlock_veh(NetDaemonVar, NULL);
        }
        if (Vehs[veh_id].state & VSTATE_UNK_200 || faction_id != (uint32_t)*FactionIDCurrent) {
            return;
        }
        stringTemp->str[0] = 0;
        say_loc(stringTemp->str, x_dst, y_dst, 0, 2, 0);
        parse_says(0, stringTemp->str, -1, -1);
        NetMsg_pop(NetMsgVar, NULL, (order == ORDER_ROAD_TO) ? "ROADTODONE" : "TUBETODONE", 5000, 0, 0);
        return;
    }
    int dir = Path_find(Paths, NULL, x, y, x_dst, y_dst, 0, -1,
        has_tech(Rules->tech_build_roads_fungus, faction_id) ? 0 : 0x10, -1);
    if (dir >= 0 && dir < 8) {
        if (*IsMultiplayerNet && !*ControlTurnC) {
            if (NetDaemon_order_veh(NetDaemonVar, NULL, veh_id, dir, 1)) {
                NetDaemon_await_exec(NetDaemonVar, NULL, 1);
            }
            return;
        }
        order_veh(veh_id, dir, 3);
        return;
    }
    Vehs[veh_id].order = ORDER_NONE;
    if (*IsMultiplayerNet && !*ControlTurnC) {
        synch_veh(veh_id);
        NetDaemon_await_synch(NetDaemonVar, NULL);
        NetDaemon_unlock_veh(NetDaemonVar, NULL);
    }
}

/*
Purpose: Air drop a unit onto the tile (x, y). Flags (also stored in VehAttackFlags): 1 tells the
         player at the controls why a drop is refused, 2 makes the drop (without it the call only
         checks). A drop is refused farther than drop_range() (half the summed x and y distances),
         into the sea, into a tile with another faction's units (unless pact), for a non-combat unit
         into a zone of control or a foreign base, into a base whose owner the player at the
         controls declines to break a treaty with, and within two tiles of a base at vendetta with
         air defenses (an Aerospace Complex, its owner's Cloudbase Academy or an air superiority
         unit). The unit comes in from a neighbouring tile scored with rand(): empty ones first,
         then its own faction's, then a pact partner's, some directions favoured. The player at the
         controls is told when another faction drops where they can see it, and the flight is shown
         if the preferences allow. The unit is marked as having dropped, a non-combat unit ends its
         turn, the units it carries end theirs, a foreign base without pact dropped into is
         captured (an act of aggression; the unit is repaired), anywhere else the landing costs
         twice the reactor in damage (never the last hit point). Then the tiles around are spotted
         and a supply pod there is opened. Another faction's drop the player sees is followed by a
         short pause (none in a network game or with shift held).
Original Offset: 004CC360
Return Value: true if the drop is allowed (flag 2 unset) or was made, otherwise false
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch32-unitmove.log).
        Deliberate change from the v2.1 exe, chosen by Kiousu (2026-10-18): the "air drop near
        $BASENAME0" notice names the base nearest the drop that the player at the controls owns or
        has seen (the original passes faction 1 to base_find()), and an empty name when there is
        none (the original reads Bases[-1]); see docs/regression-testing.md, "action_airdrop notice".
*/
BOOL __cdecl action_airdrop(int veh_id, int x, int y, int flags) {
    *VehAttackFlags = flags;
    uint32_t faction_id = Vehs[veh_id].faction_id;
    int y_veh = Vehs[veh_id].y;
    int dist_x = abs(Vehs[veh_id].x - x);
    if (!(*MapIsFlat & 1) && dist_x > (int)*MapLongitude) {
        dist_x = *MapLongitudeBounds - dist_x;
    }
    int dist = (dist_x + abs(y_veh - y)) >> 1;
    if (dist > (int)drop_range(faction_id)) {
        if ((*VehAttackFlags & 1) && faction_id == (uint32_t)*FactionIDCurrent) {
            parse_num(0, Rules->max_airdrop_sans_orb_insert);
            NetMsg_pop(NetMsgVar, NULL, "DROPRANGE", -5000, 0,
                (Players[faction_id].rule_flags & RFLAG_ALIEN) ? "al_drop_sm.pcx" : "airdp_sm.pcx");
        }
        return false;
    }
    Map *tile = order_tile(x, y);
    if ((tile->climate & 0xE0) < 0x60) {
        if ((*VehAttackFlags & 1) && faction_id == (uint32_t)*FactionIDCurrent) {
            NetMsg_pop(NetMsgVar, NULL, "DROPSEA", -5000, 0,
                (Players[faction_id].rule_flags & RFLAG_ALIEN) ? "al_drop_sm.pcx" : "airdp_sm.pcx");
        }
        return false;
    }
    if (tile->bit & BIT_VEH_IN_TILE) {
        uint32_t owner = tile->val2 & 0xF;
        if (owner < 8 && owner != faction_id
            && !(PlayersData[faction_id].diplo_treaties[owner] & DTREATY_PACT)) {
            if ((*VehAttackFlags & 1) && faction_id == (uint32_t)*FactionIDCurrent) {
                NetMsg_pop(NetMsgVar, NULL, "DROPUNIT", -5000, 0,
                    (Players[faction_id].rule_flags & RFLAG_ALIEN) ? "al_drop_sm.pcx" : "airdp_sm.pcx");
            }
            return false;
        }
    }
    if (!Weapon[VehPrototypes[Vehs[veh_id].proto_id].weapon_id].offense_rating
        && zoc_move(x, y, faction_id)) {
        if ((*VehAttackFlags & 1) && faction_id == (uint32_t)*FactionIDCurrent) {
            NetMsg_pop(NetMsgVar, NULL, "NONCOMDROP", -5000, 0,
                (Players[faction_id].rule_flags & RFLAG_ALIEN) ? "al_drop_sm.pcx" : "airdp_sm.pcx");
        }
        return false;
    }
    tile = order_tile(x, y);
    if (tile->bit & BIT_BASE_IN_TILE) {
        uint32_t owner = tile->val2 & 0xF;
        if (owner < 8 && owner != faction_id
            && !(PlayersData[faction_id].diplo_treaties[owner] & DTREATY_PACT)) {
            if (!Weapon[VehPrototypes[Vehs[veh_id].proto_id].weapon_id].offense_rating) {
                if ((*VehAttackFlags & 1) && faction_id == (uint32_t)*FactionIDCurrent) {
                    NetMsg_pop(NetMsgVar, NULL, "NONCOMDROP", -5000, 0,
                        (Players[faction_id].rule_flags & RFLAG_ALIEN) ? "al_drop_sm.pcx"
                        : "airdp_sm.pcx");
                }
                return false;
            }
            if ((*VehAttackFlags & 1) && faction_id == (uint32_t)*FactionIDCurrent
                && break_treaty(faction_id, owner, DTREATY_PACT | DTREATY_TREATY | DTREATY_TRUCE)) {
                return false;
            }
        }
    }
    // air defenses: a base at vendetta within two tiles
    for (int i = 0; i < 25; i++) {
        int x_radius = x + RadiusOffsetX[i];
        if (!(*MapIsFlat & 1)) {
            if (x_radius < 0) {
                x_radius += *MapLongitudeBounds;
            } else if (x_radius >= *MapLongitudeBounds) {
                x_radius -= *MapLongitudeBounds;
            }
        }
        int y_radius = y + RadiusOffsetY[i];
        if (y_radius < 0 || y_radius >= *MapLatitudeBounds || x_radius < 0
            || x_radius >= *MapLongitudeBounds) {
            continue;
        }
        Map *tile_radius = order_tile(x_radius, y_radius);
        if (!(tile_radius->bit & BIT_BASE_IN_TILE)) {
            continue;
        }
        int owner = tile_radius->val2 & 0xF;
        if (owner >= 8 || owner <= 0 || owner == (int)faction_id
            || !(PlayersData[faction_id].diplo_treaties[owner] & DTREATY_VENDETTA)) {
            continue;
        }
        int base_id = base_at(x_radius, y_radius);
        if (base_id < 0) {
            continue;
        }
        uint32_t offset, mask;
        bitmask(FAC_AEROSPACE_COMPLEX, &offset, &mask);
        if ((Bases[base_id].facilities_built[offset] & (uint8_t)mask)
            || (SecretProject->cloudbase_academy >= 0
                && Bases[SecretProject->cloudbase_academy].faction_id_current == owner)
            || stack_check(veh_at(x_radius, y_radius), 2, PLAN_AIR_SUPERIORITY, -1, -1)) {
            if ((*VehAttackFlags & 1) && faction_id == (uint32_t)*FactionIDCurrent) {
                parse_says(0, Bases[base_id].name_string, -1, -1);
                NetMsg_pop(NetMsgVar, NULL, "AIRDEFENSES", -5000, 0,
                    (Players[faction_id].rule_flags & RFLAG_ALIEN) ? "alair_sm.pcx" : "air_sm.pcx");
            }
            return false;
        }
    }
    if (!(*VehAttackFlags & 2)) {
        return true;
    }
    uint32_t goody = goody_at(x, y);
    // seen: the player at the controls drops, or can see the target or a tile next to it
    BOOL seen;
    if (faction_id == (uint32_t)*FactionIDCurrent) {
        seen = true;
    } else {
        tile = order_tile(x, y);
        int owner = ((tile->bit & (BIT_BASE_IN_TILE | BIT_VEH_IN_TILE)) && (tile->val2 & 0xF) < 8)
            ? (tile->val2 & 0xF) : -1;
        seen = owner == *FactionIDCurrent || (*GameState & STATE_OMNISCIENT_VIEW);
    }
    if (!seen) {
        for (int i = 0; i < 8; i++) {
            int x_radius = x + RadiusBaseX[i];
            if (!(*MapIsFlat & 1)) {
                if (x_radius < 0) {
                    x_radius += *MapLongitudeBounds;
                } else if (x_radius >= *MapLongitudeBounds) {
                    x_radius -= *MapLongitudeBounds;
                }
            }
            int y_radius = y + RadiusBaseY[i];
            if (y_radius < 0 || y_radius >= *MapLatitudeBounds || x_radius < 0
                || x_radius >= *MapLongitudeBounds) {
                continue;
            }
            Map *tile_radius = order_tile(x_radius, y_radius);
            int owner = ((tile_radius->bit & (BIT_BASE_IN_TILE | BIT_VEH_IN_TILE))
                && (tile_radius->val2 & 0xF) < 8) ? (tile_radius->val2 & 0xF) : -1;
            if (owner == *FactionIDCurrent) {
                seen = true;
                break;
            }
        }
    }
    // animate: the player's own drop, or another's the preferences don't quick move
    BOOL animate = false;
    if (seen) {
        if (faction_id == (uint32_t)*FactionIDCurrent) {
            animate = true;
        } else if ((PlayersData[faction_id].diplo_treaties[*FactionIDCurrent] & DTREATY_PACT)
            ? (*GamePreferences & PREF_BSC_DONT_QUICK_MOVE_ALLY_VEH)
            : (*GamePreferences & PREF_BSC_DONT_QUICK_MOVE_ENEMY_VEH)) {
            animate = true;
        }
    }
    // the neighbouring tile the unit comes in from
    int score_best = 999999;
    int dir_best = -1;
    for (int i = 0; i < 8; i++) {
        int x_radius = x + RadiusBaseX[i];
        if (!(*MapIsFlat & 1)) {
            if (x_radius < 0) {
                x_radius += *MapLongitudeBounds;
            } else if (x_radius >= *MapLongitudeBounds) {
                x_radius -= *MapLongitudeBounds;
            }
        }
        int y_radius = y + RadiusBaseY[i];
        if (y_radius < 0 || y_radius >= *MapLatitudeBounds || x_radius < 0
            || x_radius >= *MapLongitudeBounds) {
            continue;
        }
        int score = _rand() % 10 + 50;
        if (i >= 6 || i <= 0) {
            score -= 25;
        }
        if (i == 5 || i == 1) {
            score -= 10;
        }
        Map *tile_radius = order_tile(x_radius, y_radius);
        if (tile_radius->bit & (BIT_BASE_IN_TILE | BIT_VEH_IN_TILE)) {
            uint32_t owner = tile_radius->val2 & 0xF;
            if (owner < 8) {
                score += 100;
                if (owner != faction_id) {
                    score += 1000;
                    if (!(PlayersData[faction_id].diplo_treaties[owner] & DTREATY_PACT)) {
                        score += 10000;
                    }
                }
            }
        }
        if (score < score_best) {
            score_best = score;
            dir_best = i;
        }
    }
    if (dir_best < 0) {
        return false;
    }
    if (seen && faction_id != (uint32_t)*FactionIDCurrent) {
        // the original passes faction 1 here and reads Bases[-1] when nothing is found, see the
        // function comment
        int base_id_near = base_find(x, y, -1, -1, -1, *FactionIDCurrent);
        parse_says(0, (base_id_near >= 0) ? Bases[base_id_near].name_string : "", -1, -1);
        *GenderDefault = Players[faction_id].noun_gender;
        *PluralityDefault = Players[faction_id].is_noun_plural;
        parse_says(1, Players[faction_id].noun_faction, -1, -1);
        if (Players[faction_id].rule_flags & RFLAG_ALIEN) {
            popp(ScriptTxtID, ((int)drop_range(faction_id) > (int)Rules->max_airdrop_sans_orb_insert)
                ? "MADEORBITAL" : "MADEAIRDROP", 0, "al_drop_sm.pcx", 0);
        } else {
            popp(ScriptTxtID, ((int)drop_range(faction_id) > (int)Rules->max_airdrop_sans_orb_insert)
                ? "MADEORBITAL" : "MADEAIRDROP", 0, "airdp_sm.pcx", 0);
        }
    }
    Vehs[veh_id].state |= VSTATE_MADE_AIRDROP;
    if (!Weapon[VehPrototypes[Vehs[veh_id].proto_id].weapon_id].offense_rating) {
        veh_skip(veh_id);
    }
    int x_from = Vehs[veh_id].x;
    int y_from = Vehs[veh_id].y;
    int x_in = x + RadiusBaseX[dir_best];
    if (!(*MapIsFlat & 1)) {
        if (x_in < 0) {
            x_in += *MapLongitudeBounds;
        } else if (x_in >= *MapLongitudeBounds) {
            x_in -= *MapLongitudeBounds;
        }
    }
    int y_in = y + RadiusBaseY[dir_best];
    stack_veh(veh_id, 1);
    draw_tile(x_from, y_from, 2);
    if (seen && animate) {
        veh_scoot(veh_id, x_in, y_in, dir_best ^ 4, 0);
    }
    unspot_stack(veh_id);
    if (veh_id >= 0) { // the units it carries end their turn
        int veh_id_top = veh_id;
        while (Vehs[veh_id_top].prev_veh_id_stack >= 0) {
            veh_id_top = Vehs[veh_id_top].prev_veh_id_stack;
        }
        for (int i = veh_id_top; i >= 0; i = Vehs[i].next_veh_id_stack) {
            if (i != veh_id) {
                veh_skip(i);
            }
        }
    }
    if (veh_id == *MapWinVehIDSelected && *MapWinUnitNotViewMode) {
        Console_cursor_next(ConsoleVar, NULL, x, y);
    }
    stack_put(veh_id, x, y);
    int base_id = base_at(x, y);
    if (base_id >= 0) {
        uint32_t owner = Bases[base_id].faction_id_current;
        if (faction_id != owner && !(PlayersData[faction_id].diplo_treaties[owner] & DTREATY_PACT)) {
            if (!(*FactionsStatus & (uint8_t)(1 << faction_id))) {
                veh_skip(veh_id);
            }
            *VehIDCurrent = veh_id;
            Vehs[veh_id].dmg_incurred = 0;
            act_of_aggression(faction_id, Bases[base_id].faction_id_current);
            capture_base(base_id, faction_id, 0);
            veh_id = *VehIDCurrent; // capture_base() can renumber the unit
        }
    } else {
        Veh *veh = &Vehs[veh_id];
        veh->dmg_incurred += (uint8_t)(VehPrototypes[veh->proto_id].reactor_id * 2);
        for (;;) { // the landing never takes the last hit point
            VehPrototype *proto = &VehPrototypes[veh->proto_id];
            int lethal = 1;
            if (proto->plan != PLAN_ALIEN_ARTIFACT) {
                int reactor = proto->reactor_id;
                lethal = ((reactor < 1) ? 1 : ((reactor > 100) ? 100 : reactor)) * 10;
            }
            if (lethal - veh->dmg_incurred > 0) {
                break;
            }
            veh->dmg_incurred--;
        }
    }
    spot_all(veh_id, 1);
    Console_update_data(ConsoleVar, NULL, 0);
    if (goody) {
        goody_box(veh_id, goody);
    }
    if (faction_id != (uint32_t)*FactionIDCurrent && !(*GameState & STATE_OMNISCIENT_VIEW) && seen
        && !*MapWinUnk23BF0 && !(GetAsyncKeyState(VK_SHIFT) & 0xFF00) && !*IsMultiplayerNet) {
        clock_wait((animate && !*MapWinUnk23BF0) ? 200 : 50);
    }
    return true;
}
