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

BOOL *ExpansionEnabled = (BOOL *)0x009A6488;
uint32_t *GamePreferences = (uint32_t *)0x009A6490;
uint32_t *GameMorePreferences = (uint32_t *)0x009A6494;
uint32_t *GameWarnings = (uint32_t *)0x009A6498;
uint32_t *GameRules = (uint32_t *)0x009A649C;
uint32_t *GameMoreRules = (uint32_t *)0x009A681C;
Monument *Monuments = (Monument *)0x0094CA08; // [8]
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
        Monuments[faction_id].unk_1 = -1;
    }
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
    Monuments[faction_id].unk_1 = 1;
    if (faction_id == *FactionIDCurrent) {
        monument(1);
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
