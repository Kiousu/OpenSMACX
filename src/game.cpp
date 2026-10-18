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
#include "log.h"
#include "council.h"

BOOL *ExpansionEnabled = (BOOL *)0x009A6488;
int *EnergyCredits = (int *)0x0093A958; // credits of the player at the controls as last shown (Thinker's name)
int *EnergyReportMaint = (int *)0x007492DC; // energy report (energy_compute()): facility upkeep
int *EnergyReportBaseMaint = (int *)0x007492E0; // [512] upkeep of each base
int *EnergyReportLabs = (int *)0x00749AE0; // research per turn x100
int *EnergyReportTechTurns = (int *)0x00749AE4; // turns to the next advance
int *EnergyReportEconomy = (int *)0x00749AE8;
int *EnergyReportFacilities = (int *)0x00749AEC; // [70] facility counts
int *EnergyReportCommerce = (int *)0x00749C04; // loan payments in and out
int *EnergyReportNet = (int *)0x00749C0C;
uint32_t *GamePreferences = (uint32_t *)0x009A6490;
BOOL *GameHalted = (BOOL *)0x0068F21C; // Thinker's name; set while no game is running
uint32_t *GameMorePreferences = (uint32_t *)0x009A6494;
uint32_t *GameWarnings = (uint32_t *)0x009A6498;
uint32_t *GameRules = (uint32_t *)0x009A649C;
uint32_t *GameMoreRules = (uint32_t *)0x009A681C;
Monument *Monuments = (Monument *)0x0094CA08; // [8]
int *ControlUpkeepA = (int *)0x0093A934; // Thinker's name; selects which solar flare bit applies
int *ControlTurnC = (int *)0x0093A938; // Thinker's name; while clear in a network game, alt_set() keeps
                                      // a flooded base at size 1
uint32_t *SolarFlaresEvent = (uint32_t *)0x009A6818; // bits 0/1: energy yields tripled
uint32_t *GameState = (uint32_t *)0x009A64C0;
int *DiffLevelCurrent = (int *)0x009A64C4;
int *TurnCurrentNum = (int *)0x009A64D4;
uint32_t *ObjectiveReqVictory = (uint32_t *)0x0094B4C0;
uint32_t *ObjectivesSuddenDeathVictory = (uint32_t *)0x0094B4C4;
uint32_t *ObjectiveAchievePts = (uint32_t *)0x0094B4C8;
uint32_t *VictoryAchieveBonusPts = (uint32_t *)0x0094B4CC;
// compute_score(): once the Ascent to Transcendence is built, the points its builder and their pact
// partners share by population ([0] of the scores is the total weight, [1..7] each faction's)
int *AscentScoreScale = (int *)0x009A67DC;
int *AscentScores = (int *)0x009A67E0; // [8]
uint32_t *MissionYearCurrent = (uint32_t *)0x009A64D8;
uint32_t *StartingMissionYear = (uint32_t *)0x0094B4D0;
uint32_t *EndingMissionYear = (uint32_t *)0x0094B4D4;
uint32_t *TectonicDetonationCount = (uint32_t *)0x00946138; // [8]; value for each faction
int *SunspotDuration = (int *)0x009A6800;
int *GlobalWarmingCount = (int *)0x009A6824; // fungal blooms since the last global warming step
// Sea level change in progress (Thinker's names): ClimateFutureChange is the change still to come
// (one step per climactic_changes() that fills the counter), ClimateValueA what each turn adds to
// the counter ClimateValueC, ClimateValueB the counter's threshold.
int *ClimateValueA = (int *)0x009A67CC;
int *ClimateValueB = (int *)0x009A67D0;
int *ClimateValueC = (int *)0x009A67D4;
int *ClimateFutureChange = (int *)0x009A67D8;
uint32_t *GameInterludeState = (uint32_t *)0x009A64A4; // Thinker's name; bit 0x400: Voice of Planet begun
uint32_t *MountPlanetX = (uint32_t *)0x009A6804;
uint32_t *MountPlanetY = (uint32_t *)0x009A6808;
int *DustCloudDuration = (int *)0x009A680C;
BOOL *IsMultiplayerNet = (BOOL *)0x0093F660; // DirectPlay - Serial, Modem, Internet (TCP/IP)
BOOL *IsMultiplayerPBEM = (BOOL *)0x0093A95C; // HotSeat / PBEM

/*
Purpose: Fill in the energy report for a faction: facility counts and upkeep per base (halved,
         with the odd point carried, under the Self-Aware Colony), total labs, economy and net
         income, the turns to the next advance, and loan payments to and from factions in
         contact that aren't at war. With recompute set each base is refreshed first.
Original Offset: 00445130
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch22.log)
*/
void __cdecl energy_compute(uint32_t faction_id, BOOL recompute) {
    for (int i = 0; i < FAC_STOCKPILE_ENERGY + 1; i++) {
        EnergyReportFacilities[i] = 0;
    }
    *EnergyReportLabs = 0;
    *EnergyReportNet = 0;
    *EnergyReportMaint = 0;
    *EnergyReportEconomy = 0;
    *EnergyReportCommerce = 0;
    uint32_t carry = 0;
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        EnergyReportBaseMaint[base_id] = 0;
        if (Bases[base_id].faction_id_current != faction_id) {
            continue;
        }
        if (recompute) {
            set_base(base_id);
            base_compute(true);
        }
        int maint = 0;
        for (int facility_id = 0; facility_id <= FAC_STOCKPILE_ENERGY; facility_id++) {
            if (facility_id < FacilityRepStart && has_fac_built(facility_id, base_id)) {
                EnergyReportFacilities[facility_id]++;
                maint += fac_maint(facility_id, faction_id);
            }
        }
        if (SecretProject->self_aware_colony >= 0
            && Bases[SecretProject->self_aware_colony].faction_id_current == faction_id) {
            if (carry) {
                maint++;
            }
            carry = maint & 1;
            maint /= 2;
        }
        EnergyReportBaseMaint[base_id] = maint;
        *EnergyReportLabs += Bases[base_id].labs_total;
        *EnergyReportEconomy += Bases[base_id].economy_total;
        *EnergyReportNet += Bases[base_id].economy_total - maint;
        *EnergyReportMaint += maint;
    }
    int cost = tech_rate(faction_id);
    int research = PlayersData[faction_id].soc_effect_pending.research;
    research = research < -5 ? -5 : (research > 5 ? 5 : research);
    *EnergyReportLabs = (research + 10) * *EnergyReportLabs * 10;
    *EnergyReportTechTurns = *EnergyReportLabs < 1 ? 9999
        : (*EnergyReportLabs - 1 + cost * 100) / *EnergyReportLabs;
    for (uint32_t faction_id_2 = 1; faction_id_2 < MaxPlayerNum; faction_id_2++) {
        if (faction_id_2 == faction_id || !(FactionsStatus[1] & (1 << faction_id_2))
            || !(FactionsStatus[1] & (1 << faction_id))
            || (PlayersData[faction_id].diplo_treaties[faction_id_2] & DTREATY_VENDETTA)) {
            continue;
        }
        if (PlayersData[faction_id].loan_balance[faction_id_2]
            && !PlayersData[faction_id_2].sanction_turns) {
            *EnergyReportNet -= PlayersData[faction_id].loan_payment[faction_id_2];
            *EnergyReportCommerce -= PlayersData[faction_id].loan_payment[faction_id_2];
        }
        if (PlayersData[faction_id_2].loan_balance[faction_id]
            && !PlayersData[faction_id].sanction_turns) {
            *EnergyReportNet += PlayersData[faction_id_2].loan_payment[faction_id];
            *EnergyReportCommerce += PlayersData[faction_id_2].loan_payment[faction_id];
        }
    }
}

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
Purpose: Record a winning end of game in a non-alien faction's monument (once). The data field
         holds -1 for the winner itself, or the winner for a faction sharing the win.
Original Offset: 00477940 (unify), 004779D0 (trans)
Return Value: n/a
Status: Complete
*/
static void monument_winning(int faction_id, int record_id, int faction_id_winner) {
    uint32_t year = game_year(*TurnCurrentNum);
    MonumentRecord *record = &Monuments[faction_id].records[record_id];
    if ((Players[faction_id].rule_flags & RFLAG_ALIEN) || record->achieved) {
        return;
    }
    record->year = year;
    record->faction_id_first = faction_id;
    record->faction_count = 1;
    record->achieved = true;
    record->data = faction_id_winner;
    monument_latest(faction_id, record_id);
}

/*
Purpose: Record a non-alien faction's part in a victory in its monument; called by the end of game
         code for the winner (-1) and for the player at the controls when sharing the win.
Original Offset: 00477940
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl mon_winning_unify(int faction_id, int faction_id_winner) {
    monument_winning(faction_id, MONUMENT_WINNING_UNIFY, faction_id_winner);
}

/*
Purpose: Record a non-alien faction's part in the Ascent to Transcendence in its monument: the
         ascending faction with -1, then its pact partners with the ascending faction's id.
Original Offset: 004779D0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl mon_winning_trans(int faction_id, int faction_id_winner) {
    monument_winning(faction_id, MONUMENT_WINNING_TRANS, faction_id_winner);
}

/*
Purpose: Pick a random map coordinate pair the way alien_fauna() does: a column and a row, then the
         column moved onto the row's parity. Either is 0 on a map one tile wide or high.
Original Offset: n/a (inlined three times in alien_fauna())
Return Value: n/a
Status: Complete
*/
static void fauna_random_tile(int *x, int *y) {
    int x_rand = (*MapLongitudeBounds - 1 > 0) ? _rand() % *MapLongitudeBounds : 0;
    int y_rand = (*MapLatitudeBounds - 1 > 0) ? _rand() % *MapLatitudeBounds : 0;
    *x = (y_rand & 1) - (x_rand & 1) + x_rand;
    *y = y_rand;
}

/*
Purpose: Whether the player at the controls gets to see a native unit alien_fauna() just created:
         it is theirs and not both invisible and a lurker, or they can see it.
Original Offset: n/a (inlined in alien_fauna())
Return Value: Redraw its tile? true/false
Status: Complete
*/
static BOOL fauna_seen(int veh_id) {
    Veh *veh = &Vehs[veh_id];
    return (veh->faction_id == *FactionIDCurrent
        && (veh->flags & (VFLAG_INVISIBLE | VFLAG_LURKER)) != (VFLAG_INVISIBLE | VFLAG_LURKER))
        || (veh->visibility & (1 << *FactionIDCurrent));
}

/*
Purpose: Native life upkeep for a turn, in three parts:
         - Forests and kelp spread: random tiles are tried (fewer as the game goes on); a forest
           (land) or kelp farm (sea) tile grows into its best neighbour of the same kind (wetter and
           flatter scores higher), which loses any fungus. Factions that see the source tile see
           the growth; the player at the controls gets a popup when it is near one of their bases.
         - Unless native life is off for the scenario, the Voice of Planet exists or the unit table
           is nearly full, every few turns (more often with abundant native life) native life is
           born on a random fungus tile or a sea tile of a large region: a Sealurk (SMACX, on an odd
           clock) or an Isle of the Deep carrying mind worms and spore launchers, then Locusts of
           Chiron once any faction has either of their technologies.
         - A few random land fungus tiles without a base are tried: next to a faction's unit
           standing there a mind worm or spore launcher appears two tiles away, on a land tile the
           faction neither sees nor has the map of; with no unit there and a base 2 to 5 tiles
           away, it appears on the tile itself unless it holds a supply pod.
         slots holds the original's stack slots [ebp-4] and [ebp-0xc]: the redraw after a native is
         born on the fungus tile itself takes its coordinates from them (and from the original's
         ebx, loaded from [ebp-0xc]) although they last held another loop's tile, possibly from
         before the call; see alien_fauna(). The original also has code for a mind worm cluster
         next to a base (with the XMINDWORMS message) after the third part's loop; it can never run
         (the loop only ends when its counter reaches the limit, which skips it) and is left out.
Original Offset: 00522180
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch29.log)
*/
static void __cdecl alien_fauna_body(int *slots) {
    BOOL early_game = false;
    // forests and kelp spread
    for (int i = 0; i < (int)*MapArea / (*TurnCurrentNum / 4 + 32); i++) {
        int x, y;
        fauna_random_tile(&x, &y);
        if (y < 0 || y >= *MapLatitudeBounds || x < 0 || x >= *MapLongitudeBounds) {
            continue;
        }
        Map *tile = map_loc(x, y);
        BOOL is_sea = (tile->climate & 0xE0) < 0x60;
        if (!(tile->bit & (is_sea ? BIT_FARM : BIT_FOREST))) {
            continue;
        }
        int best = 0;
        int x_best = -1;
        int y_best = 0;
        BOOL is_flat = *MapIsFlat & 1;
        for (int j = 0; j < 8; j++) {
            int x_near = x + RadiusBaseX[j];
            if (!is_flat) {
                if (x_near < 0) {
                    x_near += *MapLongitudeBounds;
                } else if (x_near >= *MapLongitudeBounds) {
                    x_near -= *MapLongitudeBounds;
                }
            }
            int y_near = y + RadiusBaseY[j];
            slots[1] = x_near;
            slots[0] = y_near;
            if (y_near < 0 || y_near >= *MapLatitudeBounds || x_near < 0
                || x_near >= *MapLongitudeBounds) {
                continue;
            }
            Map *near_tile = map_loc(x_near, y_near);
            uint32_t climate = near_tile->climate;
            uint32_t alt = climate >> 5;
            if (((climate & 0xE0) < 0x60) != is_sea || alt < 2 || (alt > 4 && !(climate & 0x18))
                || (near_tile->bit2 & (BIT2_VOLCANO | BIT2_UNK_80000000)) == BIT2_VOLCANO
                || (near_tile->bit2 & (BIT2_DUNES | BIT2_UNK_80000000)) == BIT2_DUNES) {
                continue;
            }
            int value = (3 - (int)((climate >> 3) & 3)) * 4 - (near_tile->val3 >> 6);
            if ((near_tile->val3 & 0xC0) > 0x40 || (near_tile->bit & (BIT_FOREST | BIT_FARM
                | BIT_SOLAR_TIDAL | BIT_MINE | BIT_BASE_IN_TILE))) {
                continue;
            }
            if (value > best) {
                best = value;
                x_best = x_near;
                y_best = y_near;
            }
        }
        if (x_best < 0) {
            continue;
        }
        Map *grow_tile = map_loc(x_best, y_best);
        uint32_t growth = is_sea ? BIT_FARM : BIT_FOREST;
        if (grow_tile->bit & growth) {
            continue;
        }
        grow_tile->bit = (grow_tile->bit | growth) & ~BIT_FUNGUS;
        for (uint32_t faction_id = 1; faction_id < MaxPlayerNum; faction_id++) {
            Map *source = map_loc(x, y); // the original checks the source tile, not the new one
            if ((!(PlayersData[faction_id].flags & PFLAG_MAP_REVEALED)
                && !(source->visibility & (1 << faction_id)))
                || !(source->bit_visible[faction_id - 1] & growth)) {
                continue;
            }
            grow_tile->bit_visible[faction_id - 1] = (grow_tile->bit_visible[faction_id - 1] | growth)
                & ~BIT_FUNGUS;
            if (faction_id != (uint32_t)*FactionIDCurrent) {
                continue;
            }
            int base_id = base_find(x, y, -1, -1, -1, *FactionIDCurrent);
            if (base_id >= 0 && Bases[base_id].faction_id_current == *FactionIDCurrent
                && (*BaseFindDist <= 3
                    || whose_territory(*FactionIDCurrent, x, y, 0, 0) == *FactionIDCurrent)) {
                if (!MapWin_focus(MapWinList[0], 0, x, y)) {
                    draw_tiles(x, y, 2);
                }
                parse_says(0, Bases[base_id].name_string, -1, -1);
                if (is_sea) {
                    POP2("KELPGROWS", "kelp_sm.pcx", -1);
                } else {
                    POP2("FORESTGROWS", "forgr_sm.pcx", -1);
                }
            } else {
                draw_tiles(x, y, 2);
            }
        }
    }
    if (*GameRules & RULES_SCN_NO_NATIVE_LIFE) {
        return;
    }
    int native = (int)*MapNativeLifeForms;
    if (*TurnCurrentNum < ((3 - native) * 10 - *DiffLevelCurrent) * 2) {
        early_game = true;
    }
    if (SecretProject->voice_of_planet != -1 || *VehCurrentCount >= 0x700) {
        return;
    }
    // native life is born
    if (!(*TurnCurrentNum % ((5 - native) * 2))) {
        int tries = 0;
        int x, y;
        for (;;) {
            fauna_random_tile(&x, &y);
            Map *tile = map_loc(x, y);
            uint32_t region = tile->region;
            if (++tries >= 1000) {
                break;
            }
            if ((tile->bit & BIT_FUNGUS) && (tile->climate & 0xE0) >= 0x40) {
                break;
            }
            if ((tile->climate & 0xE0) >= 0x60
                || ((tile->bit & (BIT_BASE_IN_TILE | BIT_VEH_IN_TILE)) && (tile->val2 & 0xF) < 8)
                || (region & 0x3F) == 0x3F || !(region & 0x3F) || Continents[region].tile_count < 50) {
                continue;
            }
            break;
        }
        if (tries < 1000) {
            int count = (((*GameState & STATE_PERIHELION_ACTIVE) ? 2 : 1) * *TurnCurrentNum)
                / (((8 - native) * 4 - *DiffLevelCurrent) * 4) + native + 1;
            if (early_game) {
                count = 1;
            }
            int veh_id;
            if (*ExpansionEnabled && (timeGetTime() & 1)) {
                veh_id = veh_init(BSC_SEALURK, 0, x, y);
                if (veh_id >= 0) {
                    veh_promote(veh_id);
                }
            } else {
                veh_id = veh_init(BSC_ISLE_OF_THE_DEEP, 0, x, y);
                if (veh_id >= 0) {
                    if (count >= (int)veh_cargo(veh_id)) {
                        count = veh_cargo(veh_id);
                    }
                    for (; count > 0; count--) {
                        sleep(veh_init((!(_rand() % 5) && *ExpansionEnabled) ? BSC_SPORE_LAUNCHER
                            : BSC_MIND_WORMS, 0, x, y));
                    }
                    veh_promote(veh_id);
                }
            }
            if (veh_id >= 0 && (GameTechAchieved[TECH_CENTPSI] || GameTechAchieved[TECH_QUANTUM])) {
                veh_init(BSC_LOCUSTS_OF_CHIRON, 0, x, y);
            }
        }
    }
    // mind worms and spore launchers on fungus
    int x_draw = slots[1]; // the original's ebx
    for (int tries = 0; tries < native * 2 + 2; tries++) {
        int x, y;
        fauna_random_tile(&x, &y);
        slots[1] = y;
        Map *tile = map_loc(x, y);
        uint32_t alt = tile->climate & 0xE0;
        uint32_t region = tile->region;
        if (alt < 0x60 || !(tile->bit & BIT_FUNGUS) || alt < 0x40 || (region & 0x3F) == 0x3F
            || !(region & 0x3F) || ((tile->bit & BIT_BASE_IN_TILE) && (tile->val2 & 0xF) < 8)) {
            continue;
        }
        int base_id = base_find(x, y, -1, region, -1, -1);
        tile = map_loc(x, y);
        if (tile->bit & BIT_VEH_IN_TILE) {
            int owner = tile->val2 & 0xF;
            if (owner < 8) {
                if (owner > 0 && (base_id < 0 || *BaseFindDist > 2)) {
                    for (int i = 9; i < 25; i++) { // the ring two tiles out
                        int x_near = x + RadiusOffsetX[i];
                        if (!(*MapIsFlat & 1)) {
                            if (x_near < 0) {
                                x_near += *MapLongitudeBounds;
                            } else if (x_near >= *MapLongitudeBounds) {
                                x_near -= *MapLongitudeBounds;
                            }
                        }
                        x_draw = x_near;
                        int y_near = y + RadiusOffsetY[i];
                        slots[0] = y_near;
                        if (y_near < 0 || y_near >= *MapLatitudeBounds || x_near < 0
                            || x_near >= *MapLongitudeBounds) {
                            continue;
                        }
                        Map *near_tile = map_loc(x_near, y_near);
                        if ((near_tile->climate & 0xE0) < 0x60
                            || (PlayersData[owner].flags & PFLAG_MAP_REVEALED)
                            || (near_tile->visibility & (1 << owner))
                            || ((near_tile->bit & (BIT_BASE_IN_TILE | BIT_VEH_IN_TILE))
                                && (near_tile->val2 & 0xF) < 8)
                            || zoc_any(x_near, y_near, 0)) {
                            continue;
                        }
                        int veh_id = veh_init((!(_rand() % 5) && *ExpansionEnabled)
                            ? BSC_SPORE_LAUNCHER : BSC_MIND_WORMS, 0, x_near, y_near);
                        if (veh_id >= 0 && fauna_seen(veh_id)) {
                            draw_tile(x_near, y_near, 2);
                        }
                        break;
                    }
                }
                continue;
            }
        }
        if (*BaseFindDist > 1 && *BaseFindDist < 6) {
            tile = map_loc(x, y);
            if ((tile->bit & BIT_FUNGUS) && (tile->climate & 0xE0) >= 0x40 && !goody_at(x, y)) {
                int veh_id = veh_init((!(_rand() % 5) && *ExpansionEnabled)
                    ? BSC_SPORE_LAUNCHER : BSC_MIND_WORMS, 0, x, y);
                if (veh_id >= 0 && fauna_seen(veh_id)) {
                    draw_tile(x_draw, slots[0], 2); // a stale tile, as in the original
                }
            }
        }
    }
}

/*
Purpose: Entry for alien_fauna_body(): hands it the two stack slots the original can read before
         writing them ([ebp-0xc] = esp_entry - 0x10, [ebp-4] = esp_entry - 8) and writes back the
         values it leaves there, so the stale redraw coordinates come out as in the original.
Original Offset: 00522180
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch29.log)
*/
__declspec(naked) void __cdecl alien_fauna() {
    __asm {
        mov eax, [esp - 0x10]       // the original's [ebp-0xc]
        sub esp, 8                  // [esp] is esp_entry - 8, the original's [ebp-4], left in place
        mov [esp + 4], eax
        mov eax, esp
        push eax
        call alien_fauna_body
        add esp, 4
        mov eax, [esp + 4]
        mov [esp - 8], eax          // back to esp_entry - 0x10
        add esp, 8
        ret
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
Purpose: Move a pending sea level change on: each turn ClimateValueA is added to the counter and
         when it reaches ClimateValueB the sea rises or falls by one step (the rest of the counter
         carries over), the climate and the map are redone and the SEARISING / SEAFALLING notice is
         shown.
Original Offset: 00524210
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch30.log)
*/
void __cdecl climactic_changes() {
    if (!*ClimateFutureChange) {
        return;
    }
    *ClimateValueC += *ClimateValueA;
    if (*ClimateValueC < *ClimateValueB) {
        return;
    }
    *ClimateValueC -= *ClimateValueB;
    int step = *ClimateFutureChange > 0 ? 1 : (*ClimateFutureChange < 0 ? -1 : 0);
    *ClimateFutureChange -= step;
    *MapSeaLevel += step;
    world_climate();
    draw_map(1);
    if (step > 0) {
        popp(ScriptTxtID, "SEARISING", 0, "searis_sm.pcx", NULL);
    } else {
        popp(ScriptTxtID, "SEAFALLING", 0, "seafal_sm.pcx", NULL);
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
Purpose: The largest share, in tenths, a faction may allocate to economy or labs (allocate_energy()
         caps both with it). The faction is ignored.
Original Offset: 00445440
Return Value: Always 10
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
int __cdecl energy_limit(int UNUSED(faction_id)) {
    return 10;
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
Purpose: Run the specified faction's upkeep for the turn: social engineering, repairs and
         production, then (in single player and hotseat, or in a network game with STATE_UNK_8)
         energy allocation, diplomacy, strategy and social engineering choices; a computer player
         with more energy than the cost of cornering the Global Energy Market starts doing so when
         the economic victory is allowed. Then its bases' BSTATE_UNK_1 and hurried flags are
         cleared, the turn's hurry costs are taken out of its reserves, a faction without bases,
         colony pods or sea colony pods is eliminated, and the Planetary Council is offered to
         the player at the controls (once) or called by a computer player. Ends with the
         autosave for the player at the controls.
Original Offset: 00527290
Return Value: n/a
Status: Complete
*/
void __cdecl faction_upkeep(uint32_t faction_id) {
    *ControlUpkeepA = 1;
    social_upkeep(faction_id);
    do_all_non_input();
    repair_phase(faction_id);
    do_all_non_input();
    production_phase(faction_id);
    do_all_non_input();
    PlayerData *p = &PlayersData[faction_id];
    if (!(*GameState & STATE_UNK_1) || *GameState & STATE_UNK_8) {
        allocate_energy(faction_id);
        do_all_non_input();
        enemy_diplomacy(faction_id);
        do_all_non_input();
        enemy_strategy(faction_id);
        do_all_non_input();
        social_ai(faction_id, -1, -1, -1, -1, 0);
        do_all_non_input();
        if (!is_human(faction_id) && p->energy_reserves > (int)corner_market(faction_id)
            && has_tech(Rules->tech_economic_victory, faction_id)
            && *GameRules & RULES_VICTORY_ECONOMIC) {
            p->corner_market_turn = Rules->turns_corner_gbl_energy_mrkt + *TurnCurrentNum;
            p->corner_market_active = corner_market(faction_id);
            p->energy_reserves -= corner_market(faction_id);
            parse_set(Players[faction_id].is_leader_female, false);
            parse_says(0, Players[faction_id].title_leader, -1, -1);
            parse_set(Players[faction_id].is_leader_female, false);
            parse_says(1, Players[faction_id].name_leader, -1, -1);
            parse_set(Players[faction_id].noun_gender, Players[faction_id].is_noun_plural);
            parse_says(2, Players[faction_id].noun_faction, -1, -1);
            parse_says(3, Players[faction_id].adj_name_faction, -1, -1);
            parse_num(0, game_year(p->corner_market_turn));
            popp(ScriptTxtID, "CORNERWARNING", 0, "econwin_sm.pcx", 0);
        }
    }
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        if (Bases[base_id].faction_id_current == faction_id) {
            Bases[base_id].state &= ~(BSTATE_UNK_1 | BSTATE_PRODUCTION_HURRIED);
        }
    }
    p->energy_reserves -= p->hurry_cost_total;
    p->hurry_cost_total = 0;
    if (p->energy_reserves < 0) {
        p->energy_reserves = 0;
    }
    if (!p->current_num_bases && !p->proto_id_active[BSC_COLONY_POD]
        && !p->proto_id_active[BSC_SEA_ESCAPE_POD]) {
        eliminate_player(faction_id, 0);
    }
    *ControlUpkeepA = 0;
    *PathXDst = -1;
    *PathYDst = -1;
    if (!(*GameState & STATE_UNK_1) || *GameState & STATE_UNK_8) {
        if (faction_id == (uint32_t)*FactionIDCurrent
            && !(*GameState & (STATE_DISPLAYED_COUNCIL_AVAIL_MSG | STATE_COUNCIL_HAS_CONVENED))
            && can_call_council(faction_id, false) && !(*GameState & STATE_UNK_1)) {
            *GameState |= STATE_DISPLAYED_COUNCIL_AVAIL_MSG;
            popp(ScriptTxtID, "COUNCILOPEN", 0, "council_sm.pcx", 0);
        }
        if (!is_human(faction_id)) {
            call_council(faction_id);
        }
    }
    if (!*IsMultiplayerNet && *GamePreferences & PREF_BSC_AUTOSAVE_EACH_TURN
        && faction_id == (uint32_t)*FactionIDCurrent) {
        auto_save();
    }
}

/*
Purpose: Ground the air units that end the turn away from a base, an airbase or a carrier with
         moves left over (only chassis with a range): copters lose 30% of their reactor's hit
         points, every other air unit all of them, and a unit with nothing left is killed. A
         crash the player at the controls can see is centred on and shown with an explosion.
         Units are visited from the last to the first, as kill() compacts the unit table.
Original Offset: 00529C00
Return Value: n/a
Status: Complete
*/
void __cdecl mash_planes() {
    for (int veh_id = *VehCurrentCount - 1; veh_id >= 0; veh_id--) {
        uint32_t chassis_id = VehPrototypes[Vehs[veh_id].proto_id].chassis_id;
        if (Chassis[chassis_id].triad != TRIAD_AIR || !Chassis[chassis_id].range
            || !veh_moves(veh_id)) {
            continue;
        }
        Map *tile = map_loc(Vehs[veh_id].x, Vehs[veh_id].y);
        if ((tile->bit & BIT_BASE_IN_TILE && (tile->val2 & 0xF) < 8) || tile->bit & BIT_AIRBASE
            || stack_check(veh_id, 6, ABL_CARRIER, -1, -1)) {
            continue;
        }
        BOOL seen = false;
        uint32_t faction_id = Vehs[veh_id].faction_id;
        if (faction_id == (uint32_t)*FactionIDCurrent
            || Vehs[veh_id].visibility & (1 << *FactionIDCurrent)) {
            Console_focus(ConsoleVar, 0, Vehs[veh_id].x, Vehs[veh_id].y, faction_id);
            seen = true;
        }
        Vehs[veh_id].state |= VSTATE_UNK_4;
        if (seen) {
            boom(Vehs[veh_id].x, Vehs[veh_id].y, 0x80);
        }
        int proto_id = Vehs[veh_id].proto_id;
        chassis_id = VehPrototypes[proto_id].chassis_id;
        if (Chassis[chassis_id].range == 1 && chassis_id == CHSI_COPTER) {
            Vehs[veh_id].dmg_incurred += (uint8_t)(VehPrototypes[proto_id].reactor_id * 3);
        } else {
            Vehs[veh_id].dmg_incurred += (uint8_t)veh_health(veh_id);
        }
        if (!veh_health(veh_id)) {
            kill(veh_id);
        } else {
            Vehs[veh_id].terraforming_turns = 0;
            draw_tile(Vehs[veh_id].x, Vehs[veh_id].y, 2);
        }
    }
}

/*
Purpose: A faction's production phase. Clears the per-turn energy totals and recomputes the tech
         effects; pays the faction's loans to its living partners not under sanctions (all it can,
         the rest added back to the balance; at vendetta the payment is added to the balance
         instead); clears the AI's economy tallies; runs every one of its bases' upkeep, keeping
         the message queue going in between. On the two hardest levels an AI faction gets a
         third (Thinker) or two thirds (Transcend) of its facility maintenance back. A faction
         with the interest rule earns 1 energy per base or its rate in percent on the reserves.
         On the first turn the faction's free techs are awarded, and the end of sanctions is
         announced to the player at the controls.
Original Offset: 00526E70
Return Value: n/a
Status: Complete
*/
void __cdecl production_phase(uint32_t faction_id) {
    PlayerData *player = &PlayersData[faction_id];
    player->unk_28 = 0;
    player->energy_surplus_total = 0;
    player->maint_cost_total = 0;
    player->turn_commerce_income = 0;
    tech_effects(faction_id);
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        if (f == faction_id || !(FactionsStatus[1] & (1 << f)) || !(FactionsStatus[1] & (1 << faction_id))) {
            continue;
        }
        int balance = player->loan_balance[f];
        if (!balance || PlayersData[f].sanction_turns) {
            continue;
        }
        if (player->diplo_treaties[f] & DTREATY_VENDETTA) {
            player->loan_balance[f] = player->loan_payment[f] + balance;
        } else {
            int payment = player->loan_payment[f];
            if (payment >= player->energy_reserves) {
                payment = player->energy_reserves;
            }
            PlayersData[f].energy_reserves += payment;
            player->loan_balance[f] -= payment;
            player->energy_reserves -= payment;
            if (payment < player->loan_payment[f]) {
                player->loan_balance[f] += player->loan_payment[f] - payment;
            }
        }
    }
    player->flags &= ~PFLAG_SELF_AWARE_COLONY_LOST_MAINT;
    *EnergyCredits = player->energy_reserves;
    memset(player->unk_46, 0, sizeof(player->unk_46));
    for (int i = 0; i < 8; i++) {
        player->support_cost_by_level[i] = 0;
        memset(player->unk_39[i], 0, sizeof(player->unk_39[i]));
    }
    player->unk_47 = 0;
    player->armed_unit_weight = 0;
    player->unk_49 = 0;
    player->nutrient_surplus_total = 0;
    player->labs_total = 0;
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        if (Bases[base_id].faction_id_current == faction_id) {
            if (base_upkeep(base_id)) {
                base_id--; // the base is gone or no longer the faction's: look at its slot again
            }
            do_all_non_input();
        }
    }
    int maint = (int)player->maint_cost_total;
    if (!is_human(faction_id) && *DiffLevelCurrent > DLVL_LIBRARIAN && maint) {
        if (*DiffLevelCurrent == DLVL_THINKER) {
            player->energy_reserves += maint / 3;
            maint = (int)player->maint_cost_total;
            player->maint_cost_total = maint - maint / 3;
        } else {
            player->energy_reserves += maint * 2 / 3;
            maint = (int)player->maint_cost_total;
            player->maint_cost_total = maint - maint * 2 / 3;
        }
    }
    if (Players[faction_id].rule_flags & RFLAG_INTEREST) {
        if (!Players[faction_id].rule_interest) {
            for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
                if (Bases[base_id].faction_id_current == faction_id) {
                    player->energy_reserves++;
                    player->energy_surplus_total++;
                }
            }
        } else {
            player->energy_reserves = Players[faction_id].rule_interest * player->energy_reserves / 100
                + player->energy_reserves;
        }
    }
    player->unk_18 = player->energy_surplus_total;
    player->unk_17 = player->turn_commerce_income - player->maint_cost_total + player->energy_surplus_total;
    if (*TurnCurrentNum == 1) {
        *SkipTechScreenA = 1;
        for (int i = 0; i < Players[faction_id].rule_tech_selected; i++) {
            log_say("Freebie Advance", Players[faction_id].name_leader, faction_id, 0, 0);
            tech_advance(faction_id);
        }
        *SkipTechScreenA = 0;
    }
    if ((!(*GameState & STATE_UNK_1) || (*GameState & STATE_UNK_8)) && player->sanction_turns
        && !--player->sanction_turns && faction_id == (uint32_t)*FactionIDCurrent) {
        if (Players[faction_id].rule_flags & RFLAG_ALIEN) {
            popp(ScriptTxtID, "SANCTIONSENDALIEN", 0, "Alopdir.pcx", 0);
            return;
        }
        popp(ScriptTxtID, "SANCTIONSEND", 0, "council_sm.pcx", 0);
    }
}

/*
Purpose: When a new SMACX game is set up and the player's faction is neither an alien faction nor
         FUNGBOY, remove every computer controlled alien or FUNGBOY faction from a single player
         game: its bases and units are destroyed, it no longer counts as alive and its flags and
         difficulty level are set to -1. The uppercased faction file names are left in stringTemp.
Original Offset: 00588F90
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch27.log
        and 2026-10-17-sweep-batch27.log)
*/
void __cdecl alien_start() {
    if (*TurnCurrentNum || !*ExpansionEnabled || is_alien_faction(*FactionIDCurrent)) {
        return;
    }
    strcpy(stringTemp->str, Players[*FactionIDCurrent].filename);
    CharUpperA(stringTemp->str);
    if (!strcmp(stringTemp->str, "FUNGBOY")) {
        return;
    }
    for (uint32_t faction_id = 1; faction_id < MaxPlayerNum; faction_id++) {
        if (*FactionsStatus & (1 << faction_id) || *IsMultiplayerPBEM || *IsMultiplayerNet) {
            continue;
        }
        strcpy(stringTemp->str, Players[faction_id].filename);
        CharUpperA(stringTemp->str);
        if (!is_alien_faction(faction_id) && strcmp(stringTemp->str, "FUNGBOY")) {
            continue;
        }
        FactionsStatus[0] &= ~(1 << faction_id);
        FactionsStatus[1] &= ~(1 << faction_id);
        for (int base_id = *BaseCurrentCount - 1; base_id >= 0; base_id--) {
            if (Bases[base_id].faction_id_current == faction_id) {
                base_kill(base_id);
            }
        }
        for (int veh_id = *VehCurrentCount - 1; veh_id >= 0; veh_id--) {
            if (veh_id < *VehCurrentCount && Vehs[veh_id].faction_id == faction_id) {
                veh_kill(veh_id);
            }
        }
        PlayersData[faction_id].current_num_bases = 0;
        PlayersData[faction_id].flags = 0xFFFFFFFF;
        PlayersData[faction_id].diff_level = -1;
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
Purpose: Compute the specified faction's score into score_total and nine score lines (with NULL
         pointers, the faction's own fields): [0] citizens (its bases, plus those of pact
         partners or factions surrendered to it, depending on the rules), [1] commerce, [2] techs,
         [3] theory_of_everything x 10, [4] 25 per Secret Project, [5] the native life bonus, [6]
         objectives (its share of the Ascent points once the Ascent to Transcendence is built), [7]
         victory points and [8], left at 0. is_sub is set when a "highest score wins" scenario
         scores every faction: the function then ends without the scenario check and without
         recording the total; otherwise the total is recorded (doubled under Iron Man unless
         the scenario was cheated).
Original Offset: 005ACBE0
Return Value: n/a
Status: Complete
*/
void __cdecl compute_score(uint32_t faction_id, int *score_total, int *scores, BOOL is_sub) {
    if (*GameState & STATE_UNK_8 && !is_sub) {
        return;
    }
    PlayerData *p = &PlayersData[faction_id];
    int *stored = &p->unk_110; // [9] the faction's own score lines: unk_110 .. unk_118
    if (!score_total) {
        score_total = &p->unk_109;
    }
    if (!scores) {
        scores = stored;
    }
    *score_total = 0;
    if (!is_sub) {
        p->unk_107 = 0;
    }
    memset(scores, 0, sizeof(int) * 9);
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        uint32_t owner = Bases[base_id].faction_id_current;
        int pop = Bases[base_id].population_size;
        if (owner == faction_id) {
            scores[0] += pop;
            set_base(base_id);
            base_compute(false);
            for (uint32_t i = 1; i < MaxPlayerNum; i++) {
                if (!(Players[faction_id].rule_flags & RFLAG_ALIEN)
                    && !(Players[i].rule_flags & RFLAG_ALIEN) && !p->sanction_turns
                    && !PlayersData[i].sanction_turns) {
                    scores[1] += BaseCommerceImport[i];
                }
            }
        } else if (!(p->flags & (PFLAG_UNK_20000 | PFLAG_UNK_40000))) {
            uint32_t treaty = PlayersData[owner].diplo_treaties[faction_id];
            if (treaty & DTREATY_HAVE_SURRENDERED) {
                if (treaty & DTREATY_PACT) {
                    scores[0] += pop;
                }
            } else if (treaty & DTREATY_PACT && *GameRules & RULES_VICTORY_COOPERATIVE) {
                scores[0] += pop / 2;
            }
        } else if (p->diplo_treaties[owner] & DTREATY_PACT
            && *GameRules & RULES_VICTORY_COOPERATIVE) {
            scores[0] += pop;
        } else {
            scores[0] += pop / 2;
        }
    }
    for (int tech_id = 0; tech_id < MaxTechnologyNum; tech_id++) {
        if (has_tech(tech_id, faction_id)) {
            scores[2]++;
        }
    }
    scores[3] = p->theory_of_everything * 10;
    int objective_pts = (int)*ObjectiveAchievePts;
    if (objective_pts < 1) {
        objective_pts = 1;
    }
    scores[6] = num_objectives(faction_id, *GameRules & RULES_VICTORY_COOPERATIVE) * objective_pts;
    for (int i = 0; i < MaxSecretProjectNum; i++) {
        int base_id = base_project(i);
        if (base_id >= 0 && Bases[base_id].faction_id_current == faction_id) {
            scores[4] += 25;
        }
    }
    *AscentScoreScale = 0;
    memset(AscentScores, 0, sizeof(int) * MaxPlayerNum);
    int ascent_base_id = base_project(SP_ASCENT_TO_TRANSCENDENCE);
    if (ascent_base_id >= 0) {
        *AscentScoreScale = 300;
        int scale = (1000 - *TurnCurrentNum) * 2;
        if (scale >= 300) {
            *AscentScoreScale = scale;
        }
        int faction_id_ascent = -1;
        int pop_total = 0;
        int pop_share = 0;
        for (int i = 1; i < MaxPlayerNum; i++) {
            if (Bases[ascent_base_id].faction_id_current == i) {
                faction_id_ascent = i;
                pop_total = PlayersData[i].pop_total * 2;
                AscentScores[i] = pop_total;
                if (i == (int)faction_id) {
                    pop_share = pop_total;
                }
                break;
            }
        }
        for (int i = 1; i < MaxPlayerNum; i++) {
            // with no faction 1-7 owning the base the original reads diplo_treaties[-1]
            if (faction_id_ascent != i
                && (&PlayersData[i].diplo_treaties[0])[faction_id_ascent] & DTREATY_PACT) {
                pop_total += PlayersData[i].pop_total;
                AscentScores[i] = PlayersData[i].pop_total;
                if (i == (int)faction_id) {
                    pop_share += PlayersData[i].pop_total;
                }
            }
        }
        scores[6] = *AscentScoreScale * pop_share / pop_total; // no check for 0 in the original
        AscentScores[0] = pop_total;
    }
    int conquest = (500 - *TurnCurrentNum) * 2;
    scores[7] = (conquest < 0) ? 0 : conquest;
    if (!(*GameRules & RULES_VICTORY_CONQUEST)) {
        scores[7] = 0;
    }
    if (*GameState & STATE_VICTORY_CONQUER && *GameRules & RULES_VICTORY_CONQUEST && scores[7]) {
        int pop_share = 0;
        int pop_total = 0;
        for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
            int pop = Bases[base_id].population_size;
            pop_total += pop;
            uint32_t owner = Bases[base_id].faction_id_current;
            uint32_t treaty = PlayersData[owner].diplo_treaties[faction_id];
            if (owner == faction_id
                || (treaty & DTREATY_PACT && treaty & DTREATY_HAVE_SURRENDERED)) {
                pop_share += pop;
            }
        }
        if (pop_total) {
            scores[7] = stored[7] * pop_share / pop_total;
        }
    } else {
        scores[7] = 0;
        if (p->flags & (PFLAG_UNK_20000 | PFLAG_UNK_40000)) {
            int points = (600 - *TurnCurrentNum) * 2;
            scores[7] = (points < 200) ? 200 : points;
        } else if (p->flags & PFLAG_UNK_100000) {
            int points = (1000 - *TurnCurrentNum) * 2;
            scores[7] = (points < 300) ? 300 : points;
        } else {
            for (uint32_t i = 1; i < MaxPlayerNum; i++) {
                if (PlayersData[i].flags & (PFLAG_UNK_20000 | PFLAG_UNK_40000)
                    && p->diplo_treaties[i] & DTREATY_PACT
                    && *GameRules & RULES_VICTORY_COOPERATIVE) {
                    int points = 600 - *TurnCurrentNum;
                    scores[7] = (points < 100) ? 100 : points;
                }
            }
        }
    }
    if (!(FactionsStatus[1] & (1 << faction_id))) {
        scores[7] = 0;
        scores[8] = 0;
    }
    if (((int)*ObjectiveReqVictory < 9000 || (int)*ObjectivesSuddenDeathVictory < 9000)
        && !(*GameState & STATE_SCN_VICT_HIGHEST_AC_SCORE_WINS)) {
        for (int i = 0; i < 9; i++) {
            if (i != 6) {
                stored[i] = 0; // the faction's own lines, even when scores points elsewhere
            }
        }
    }
    if (!stored[7]) {
        if (!*ObjectiveReqVictory) {
            int most = (most_objectives(NULL, NULL) < 1) ? 1 : most_objectives(NULL, NULL);
            if (num_objectives(faction_id, *GameRules & RULES_VICTORY_COOPERATIVE) >= most) {
                scores[7] = *VictoryAchieveBonusPts;
            }
        } else if (num_objectives(faction_id, *GameRules & RULES_VICTORY_COOPERATIVE)
            >= (int)*ObjectiveReqVictory) {
            scores[7] = *VictoryAchieveBonusPts;
        }
    }
    for (int i = 0; i < 9; i++) {
        *score_total += scores[i];
    }
    if (*MapNativeLifeForms != 1 && !(*GameState & STATE_IS_SCENARIO)) {
        int bonus = (int)(*MapNativeLifeForms - 1) * *score_total / 4;
        scores[5] = bonus;
        *score_total += bonus;
    }
    if (*GameState & STATE_SCN_VICT_HIGHEST_AC_SCORE_WINS) {
        if (is_sub) {
            return;
        }
        int score_best = 0;
        for (uint32_t i = 1; i < MaxPlayerNum; i++) {
            int total;
            int lines[9];
            compute_score(i, &total, lines, true);
            if (total > score_best) {
                score_best = total;
            }
        }
        if (*score_total >= score_best) {
            // set after the total was summed: the original leaves it out of this total
            scores[7] = *VictoryAchieveBonusPts ? *VictoryAchieveBonusPts : 1000;
        }
    } else if (is_sub) {
        return;
    }
    int total = *score_total;
    p->unk_107 = total;
    if (*GameRules & RULES_IRONMAN && !(*GameState & STATE_SCENARIO_CHEATED_FLAG)) {
        p->unk_107 = total * 2;
    }
}

/*
Purpose: Even out the factions' starting positions at the start of a game. Each faction's start
         (its first unit, or the one nearest to it on land if that is at sea) is rated by the
         distance to the nearest rival on the same continent (or the continent's size if there is
         none), the good tiles around it and the continent's size. Factions below the best get
         the difference back: an extra colony pod on harder difficulties (none for aquatic
         factions) and unity pods and supply pods on the ring two tiles out; free units from the
         faction's bonuses count against it, a colony pod as three.
Original Offset: 005B0420
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch25.log
        and 2026-10-16-sweep-batch25.log)
*/
void __cdecl balance() {
    int start_veh[MaxPlayerNum - 1];
    int rating[MaxPlayerNum - 1];
    for (int f = 1; f < MaxPlayerNum; f++) {
        int veh_id = veh_find(0, 0, f, -1);
        start_veh[f - 1] = veh_id;
        if (veh_id >= 0 && map_loc(Vehs[veh_id].x, Vehs[veh_id].y)->region >= MaxRegionLandNum) {
            start_veh[f - 1] = veh_find(Vehs[veh_id].x, Vehs[veh_id].y, f, veh_id);
        }
    }
    int rating_best = 0;
    for (int f = 1; f < MaxPlayerNum; f++) {
        int veh_id = start_veh[f - 1];
        if (veh_id < 0) {
            continue;
        }
        int x = Vehs[veh_id].x;
        int y = Vehs[veh_id].y;
        uint32_t region = map_loc(x, y)->region;
        int nearest = 9999; // distance to the nearest rival start on the continent
        for (int g = 1; g < MaxPlayerNum; g++) {
            int veh_id_g = start_veh[g - 1];
            if (f != g && veh_id_g >= 0
                && region == map_loc(Vehs[veh_id_g].x, Vehs[veh_id_g].y)->region) {
                int dx = abs(x - Vehs[veh_id_g].x);
                if (!(*MapIsFlat & 1) && dx > (int)*MapLongitude) {
                    dx = *MapLongitudeBounds - dx;
                }
                int dist = vector_dist(dx, y - Vehs[veh_id_g].y) * 3200 / (int)*MapArea;
                if (dist < nearest) {
                    nearest = dist;
                }
            }
        }
        if (nearest < 1000) {
            nearest = nearest / 5 - 3;
        } else {
            nearest = ((int)Continents[region].tile_count + 31) / 32;
            if (nearest < 1) {
                nearest = 1;
            } else if (nearest > 8) {
                nearest = 8;
            }
        }
        int good = 0;
        for (int i = 0; i < BaseRadiusTiles; i++) {
            int x2 = RadiusOffsetX[i] + x;
            if (!(*MapIsFlat & 1)) {
                if (x2 < 0) {
                    x2 += *MapLongitudeBounds;
                } else if (x2 >= *MapLongitudeBounds) {
                    x2 -= *MapLongitudeBounds;
                }
            }
            int y2 = RadiusOffsetY[i] + y;
            if (y2 < 0 || y2 >= *MapLatitudeBounds || x2 < 0 || x2 >= *MapLongitudeBounds) {
                continue;
            }
            Map *tile = map_loc(x2, y2);
            uint32_t rainfall = (tile->climate >> 3) & 3;
            uint32_t altitude = tile->climate >> 5;
            uint32_t rocky = tile->val3 >> 6;
            if (rainfall) {
                int above_sea = altitude - 2;
                if (((above_sea >= 0 && altitude != 2) || tile->bit & BIT_RIVER) && rocky < 2
                    && (rocky == 1 || above_sea > 1 || rainfall > 1)) {
                    good++;
                }
                if (rocky == 1 && above_sea > 1) {
                    good++;
                }
            }
        }
        good /= 2;
        if (good < 0) {
            good = 0;
        } else if (good > 8) {
            good = 8;
        }
        int size = (int)Continents[region].tile_count / 64;
        if (size < 0) {
            size = 0;
        } else if (size > 3) {
            size = 3;
        }
        int total = nearest + good + size;
        if (total > rating_best) {
            rating_best = total;
        }
        rating[f - 1] = total;
    }
    for (int f = 1; f < MaxPlayerNum; f++) {
        int veh_id = start_veh[f - 1];
        if (veh_id < 0) {
            continue;
        }
        int x = Vehs[veh_id].x;
        int y = Vehs[veh_id].y;
        int ring_start = _rand() % 16;
        int unity_pods = 0;
        int extra = rating_best - rating[f - 1];
        Player *player = &Players[f];
        for (int i = 0; i < player->faction_bonus_count; i++) {
            if (player->faction_bonus_id[i] == RULE_UNIT) {
                extra -= (VehPrototypes[player->faction_bonus_val1[i]].plan == PLAN_COLONIZATION)
                    ? 3 : 1;
            }
        }
        if (*DiffLevelCurrent > 1
            && (extra > 2 || 5 - is_human(f) <= *DiffLevelCurrent)) {
            extra -= 3;
            if (!(player->rule_flags & RFLAG_AQUATIC)) {
                veh_id = veh_init(BSC_COLONY_POD, f, x, y);
            }
            Vehs[veh_id].home_base_id = -1; // the start unit's own when no pod was added
        }
        if (*DiffLevelCurrent < 2) {
            extra = (extra + 1) / 2;
        }
        int ring = ring_start * 7;
        for (int i = 0; i < extra; i++) {
            ring += 7;
            int k = 9 + ring % 16; // the 16 tiles two out
            int x2 = RadiusOffsetX[k] + x;
            if (!(*MapIsFlat & 1)) {
                if (x2 < 0) {
                    x2 += *MapLongitudeBounds;
                } else if (x2 >= *MapLongitudeBounds) {
                    x2 -= *MapLongitudeBounds;
                }
            }
            int y2 = RadiusOffsetY[k] + y;
            if (y2 < 0 || y2 >= *MapLatitudeBounds || x2 < 0 || x2 >= *MapLongitudeBounds) {
                continue;
            }
            if (goody_at(x2, y2) && !(map_loc(x2, y2)->bit & (BIT_UNK_4000000 | BIT_UNK_8000000))) {
                rating[f - 1]--; // a pod already there (the rating isn't used again)
            }
            if (unity_pods < 2 && extra - i > 2 && !((i ^ unity_pods) & 1)) {
                bit_set(x2, y2, BIT_UNK_4000000, true);
                extra--;
                unity_pods++;
            } else {
                bit_set(x2, y2, BIT_UNK_8000000, true);
            }
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
