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
#include "faction.h"
#include "alpha.h"
#include "base.h"
#include "general.h"
#include "text.h"
#include "strings.h"
#include "council.h"
#include "game.h"
#include "map.h"
#include "net.h"
#include "path.h"
#include "probe.h"
#include "random.h"
#include "technology.h"
#include "terraforming.h"
#include "veh.h"

Player *Players = (Player *)0x00946A50;
char *PronounBuffer = (char *)0x00945838; // get_his_her() and friends write here
PlayerData *PlayersData = (PlayerData *)0x0096C9E0;
FactionArt *FactionsArt = (FactionArt *)0x0078E978;
RulesSocialCategory *SocialCategories = (RulesSocialCategory *)0x0094B000;
RulesSocialEffect *SocialEffects = (RulesSocialEffect *)0x00946580;
LPSTR *Mood = (LPSTR *)0x0094C9E4;
LPSTR *Repute = (LPSTR *)0x00946A30;
RulesMight *Might = (RulesMight *)0x0094C558;
RulesBonusName *BonusName = (RulesBonusName *)0x009461A8;
uint8_t *FactionsStatus = (uint8_t *)0x009A64E8;
uint32_t *FactionRankings = (uint32_t *)0x009A64EC; // [8]
uint32_t *RankingFactionIDUnk1 = (uint32_t *)0x009A650C;
uint32_t *RankingFactionIDUnk2 = (uint32_t *)0x009A6510;
uint32_t *FactionRankingsUnk = (uint32_t *)0x00945DD8; // [8]
int16_t *FactionTurnMight = (int16_t *)0x009A68AC; // [1000][8] score of each faction per turn; see rankings()
int *DiploFriction = (int *)0x0093FA74; // not always bounded, should it be 0-20?
int *DiploSecondFaction = (int *)0x0093F7CC; // second faction of a joint plan; see suggest_plan()
int *DiploProtoIDBest = (int *)0x0093F804; // another faction's design wanted most; see scan_prototypes()
int *DiploProtoIDTradeable = (int *)0x0093FA40; // the one obtainable in diplomacy
// tech_analysis() results: the four techs this faction could give (best first) and the two it
// could get, with the secret project a pick unlocks
int *DiploTechGiveID1 = (int *)0x0093FAA8; // Thinker: diplo_entry_id
int *DiploTechGiveID2 = (int *)0x0093FA18; // Thinker: diplo_tech_id2
int *DiploTechGiveID3 = (int *)0x0093FA1C;
int *DiploTechGiveID4 = (int *)0x0093FA28;
int *DiploTechGiveValue1 = (int *)0x0093F814;
int *DiploTechGiveValue2 = (int *)0x0093FA44;
int *DiploTechGiveValue3 = (int *)0x0093FA20;
int *DiploTechGiveValue4 = (int *)0x0093F7C0;
int *DiploTechGetID1 = (int *)0x0093F800; // Thinker: diplo_tech_id1
int *DiploTechGetID2 = (int *)0x0093F80C;
int *DiploTechGetValue1 = (int *)0x0093FAA4;
int *DiploTechGetValue2 = (int *)0x0093F7EC;
int *DiploTechGetProject1 = (int *)0x0093FA6C;
int *DiploTechGetProject2 = (int *)0x0093FA84;
int *DiploTechProjectID = (int *)0x0093F7F4; // project index the other faction is building (or -2 - index)
uint32_t *DiploFrictionFactionIDWith = (uint32_t *)0x0093FABC;
uint32_t *DiploFrictionFactionID = (uint32_t *)0x0093FAC0;
int *DiploUnk1 = (int *)0x0093F7D0; // dont_withdrawal() counts treaty partners only while positive
// Set by diplomacy_check() (Thinker: diplo_value_93FA70/93FA98/93FA3C); read by the answers to a
// demand to withdraw and to a tech sale.
int *DiploUnk2 = (int *)0x0093FA70;
int *DiploUnk3 = (int *)0x0093FAA0;
int *DiploUnk4 = (int *)0x0093FA68;
int *DiploUnk5 = (int *)0x0093FA98;
int *DiploUnk6 = (int *)0x0093FA78;
int *DiploUnk7 = (int *)0x0093FA3C; // doubles the price buy_tech() asks
int *DiploBaseSwapAskID = (int *)0x0093FA7C; // Thinker: diplo_ask_base_swap_id
int *DiploBaseSwapBidID = (int *)0x0093FA30; // Thinker: diplo_bid_base_swap_id
int *DiploUnk8 = (int *)0x0093FA5C; // faction named in buy_tech()'s COMMLINK offer
int *DiploCounterProposalID = (int *)0x0093FAB0; // Thinker: diplo_counter_proposal_id
int *DiploUnk9 = (int *)0x0093FA60; // set by diplomacy_check(); read by enemy_diplomacy()
// Why an AI faction wants to call the player at the controls, indexed by the AI faction: 1 to
// talk, 2 first contact (comm_check), 0x40 a common enemy stronger than the AI, 4 / 0x20 units
// in its territory under a treaty, 8 / 0x10 / 0x18 units in its territory otherwise
// (enemy_diplomacy). Hotseat/PBEM games keep them per human player:
// DiploCommRequestPBEM[human * 8 + faction].
uint32_t *DiploCommRequest = (uint32_t *)0x0093F678; // [8]
uint32_t *DiploCommRequestPBEM = (uint32_t *)0x0093F698; // [8][8]

/*
Purpose: Determine if the specified faction is a Progenitor alien faction (Caretakers / Usurpers).
Original Offset: n/a
Return Value: Is faction a Progenitor? true/false
Status: Complete
*/
BOOL __cdecl is_alien_faction(uint32_t faction_id) {
    return *ExpansionEnabled && (Players[faction_id].rule_flags & RFLAG_ALIEN);
}

/*
Purpose: Determine if the specified faction is controlled by a human player or computer AI.
Original Offset: n/a
Return Value: Is faction a human? true/false
Status: Complete
*/
BOOL __cdecl is_human(uint32_t faction_id) {
    // 0 or 1: the original normalizes the flag wherever it does arithmetic with it (tech_val)
    return (FactionsStatus[0] & (1 << faction_id)) != 0;
}

/*
Purpose: Determine if the specified faction is alive or whether they've been eliminated.
Original Offset: n/a
Return Value: Is faction alive? true/false
Status: Complete
*/
BOOL __cdecl is_alive(uint32_t faction_id) {
    return FactionsStatus[1] & (1 << faction_id);
}

/*
Purpose: Calculate the base amount of talents and drones for the specified faction.
Original Offset: 004EA4A0
Return Value: n/a
Status: Complete
*/
void __cdecl psych_check(uint32_t faction_id, int *drones, int *talents) {
    *drones = 6 - (is_human(faction_id) ? PlayersData[faction_id].diff_level : DLVL_LIBRARIAN);
    *talents = (((*drones + 2) * (PlayersData[faction_id].soc_effect_pending.efficiency < 0 ? 4
        : PlayersData[faction_id].soc_effect_pending.efficiency + 4) * *MapAreaSqRoot) / 56) / 2;
}

/*
Purpose: Check whether the primary faction has at least one of the specified treaties (bitfield) 
         with the secondary faction.
Original Offset: 005002F0
Return Value: Treaty status between the two factions, generally treated as a boolean
Status: Complete
*/
uint32_t __cdecl has_treaty(uint32_t faction_id, uint32_t faction_id_with, uint32_t treaty) {
    return PlayersData[faction_id].diplo_treaties[faction_id_with] & treaty;
}

/*
Purpose: Get the faction's name adjective.
Original Offset: 0050B910
Return Value: Faction name adjective
Status: Complete
*/
LPSTR __cdecl get_adjective(uint32_t faction_id) {
    return Players[faction_id].adj_name_faction;
}

/*
Purpose: Get the faction's noun.
Original Offset: 0050B930
Return Value: Faction noun
Status: Complete
*/
LPSTR __cdecl get_noun(uint32_t faction_id) {
    parse_set(Players[faction_id].noun_gender, Players[faction_id].is_noun_plural);
    return Players[faction_id].noun_faction;
}

/*
Purpose: Get the faction leader's name, setting the parse gender to the leader's.
Original Offset: 00559060
Return Value: Leader name
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
LPSTR __cdecl get_name(int faction_id) {
    parse_set(Players[faction_id].is_leader_female, false);
    return Players[faction_id].name_leader;
}

/*
Purpose: Get the faction leader's title, setting the parse gender to the leader's.
Original Offset: 005590A0
Return Value: Leader title
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
LPSTR __cdecl get_title(int faction_id) {
    parse_set(Players[faction_id].is_leader_female, false);
    return Players[faction_id].title_leader;
}

/*
Purpose: Get the adjective describing the faction leader.
Original Offset: 005590E0
Return Value: Leader adjective
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
LPSTR __cdecl get_adjective_leader(int faction_id) {
    return Players[faction_id].adj_leader;
}

/*
Purpose: Get the insulting adjective for the faction leader.
Original Offset: 00559100
Return Value: Insulting leader adjective
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
LPSTR __cdecl get_adjective_insult_leader(int faction_id) {
    return Players[faction_id].adj_insult_leader;
}

/*
Purpose: Get the adjective describing the faction.
Original Offset: 00559120
Return Value: Faction adjective
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
LPSTR __cdecl get_adjective_faction(int faction_id) {
    return Players[faction_id].adj_faction;
}

/*
Purpose: Get the insulting adjective for the faction.
Original Offset: 00559140
Return Value: Insulting faction adjective
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
LPSTR __cdecl get_adjective_insult_faction(int faction_id) {
    return Players[faction_id].adj_insult_faction;
}

/*
Purpose: Get the insult for the faction leader, setting the parse gender to the leader's.
Original Offset: 00559160
Return Value: Leader insult
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
LPSTR __cdecl get_insult_leader(int faction_id) {
    parse_set(Players[faction_id].is_leader_female, false);
    return Players[faction_id].insult_leader;
}

/*
Purpose: Get a label from the label table (labels.txt) through the string table.
Original Offset: n/a
Return Value: Label text
Status: Complete
*/
static LPSTR label_text(int label_id) {
    return StringTable->get((int)((LPSTR *)Labels->strings_ptr)[label_id]);
}

/*
Purpose: Get a label from the 2 forms that follow label_id (male and female leader).
Original Offset: n/a
Return Value: Label text
Status: Complete
*/
static LPSTR label_gendered(int faction_id, int label_id) {
    return label_text(label_id + (Players[faction_id].is_leader_female != 0));
}

/*
Purpose: Get the word for a pact with the faction ("brotherhood"/"sisterhood"), singular.
Original Offset: 0058F9D0
Return Value: Pact word
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
LPSTR __cdecl get_pact(int faction_id) {
    parse_set(Players[faction_id].is_leader_female, false);
    return label_gendered(faction_id, 201);
}

/*
Purpose: Get the word for a pact with the faction, plural.
Original Offset: 0058FA20
Return Value: Pact word
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
LPSTR __cdecl get_pacts(int faction_id) {
    parse_set(Players[faction_id].is_leader_female, true);
    return label_gendered(faction_id, 203);
}

/*
Purpose: Get the plural word for a pact between two factions: the female form only when both
         leaders are female.
Original Offset: 0058FA70
Return Value: Pact word
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
LPSTR __cdecl get_pacts_2(int faction_id, int faction_id_2) {
    BOOL both = Players[faction_id].is_leader_female && Players[faction_id_2].is_leader_female;
    parse_set(both, true);
    return label_text(both ? 204 : 203);
}

/*
Purpose: Get the word for the bond of a pact between two factions, the female form only when both
         leaders are female.
Original Offset: 0058FB00
Return Value: Pact word
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
LPSTR __cdecl get_pact_hood(int faction_id, int faction_id_2) {
    parse_set(0, false);
    BOOL both = Players[faction_id].is_leader_female && Players[faction_id_2].is_leader_female;
    return label_text(both ? 206 : 205);
}

/*
Purpose: Copy a gendered pronoun label for the faction leader into the pronoun buffer, with the
         first letter (capitalize 1) or all letters (any other non-zero) upper case.
Original Offset: n/a
Return Value: Pronoun buffer
Status: Complete
*/
static LPSTR leader_pronoun(int faction_id, int label_id, int capitalize) {
    LPSTR label = label_gendered(faction_id, label_id);
    strcpy_s(PronounBuffer, strlen(label) + 1, label); // unbounded, as in the original
    if (capitalize == 1) {
        char first[2] = { PronounBuffer[0], 0 };
        CharUpperA(first);
        PronounBuffer[0] = first[0];
    } else if (capitalize) {
        CharUpperA(PronounBuffer);
    }
    return PronounBuffer;
}

/*
Purpose: Get the possessive pronoun of the faction leader ("his"/"her").
Original Offset: 0058FBC0
Return Value: Pronoun
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
LPSTR __cdecl get_his_her(int faction_id, int capitalize) {
    return leader_pronoun(faction_id, 207, capitalize);
}

/*
Purpose: Get the object pronoun of the faction leader ("him"/"her").
Original Offset: 0058FC50
Return Value: Pronoun
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
LPSTR __cdecl get_him_her(int faction_id, int capitalize) {
    return leader_pronoun(faction_id, 199, capitalize);
}

/*
Purpose: Get the subject pronoun of the faction leader ("he"/"she").
Original Offset: 0058FCE0
Return Value: Pronoun
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
LPSTR __cdecl get_he_she(int faction_id, int capitalize) {
    return leader_pronoun(faction_id, 209, capitalize);
}

/*
Purpose: Open the faction's definition file at its own section (or, failing that, the section
         named after the file), warning when neither exists.
Original Offset: n/a (inlined in get_phrase and get_noun_phrase)
Return Value: Was there an error? true/false
Status: Complete
*/
static BOOL faction_text_open(int faction_id) {
    LPSTR filename = Players[faction_id].filename;
    if (text_open(filename, Players[faction_id].search_key) && text_open(filename, filename)) {
        parse_says(0, Players[faction_id].search_key, -1, -1);
        parse_says(1, filename, -1, -1);
        X_pop("PLAYERFILE", 0);
        return true;
    }
    return false;
}

/*
Purpose: Get one of the faction's phrases: line phrase_id of its definition section after the
         nine header lines.
Original Offset: 0058F810
Return Value: Phrase in the text buffer (the empty base string if the file is missing)
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
LPSTR __cdecl get_phrase(int faction_id, int phrase_id) {
    if (faction_text_open(faction_id)) {
        return StringTable->get(0);
    }
    for (int i = phrase_id + 9; i > 0; i--) {
        text_get();
    }
    return *TextBufferGetPtr;
}

/*
Purpose: Get one of the faction's noun phrases: like get_phrase(), but the line holds the noun and
         its grammar ("F"/"N" for female/neuter, a second character "2" for plural), which set the
         parse gender and plurality.
Original Offset: 0058F8B0
Return Value: Noun in the text buffer (the empty base string if the file is missing)
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch16.log)
*/
LPSTR __cdecl get_noun_phrase(int faction_id, int phrase_id) {
    if (faction_text_open(faction_id)) {
        return StringTable->get(0);
    }
    for (int i = phrase_id + 9; i > 0; i--) {
        text_get();
    }
    char noun[256];
    LPSTR item = text_item();
    strcpy_s(noun, strlen(item) + 1, item); // unbounded, as in the original
    LPSTR grammar = text_item();
    char gender = grammar[0];
    *GenderDefault = 0;
    if (gender == 'F' || gender == 'f') {
        *GenderDefault = 1;
    }
    if (gender == 'N' || gender == 'n') {
        *GenderDefault = 2;
    }
    *PluralityDefault = grammar[1] == '2';
    strcpy_s(*TextBufferGetPtr, strlen(noun) + 1, noun);
    return *TextBufferGetPtr;
}

/*
Purpose: Determine whether automatic contact is enabled for net or PBEM games.
Original Offset: 00539160
Return Value: Is always contact enabled? true/false
Status: Complete
*/
BOOL __cdecl auto_contact() {
    return (*IsMultiplayerNet && Rules->tgl_human_always_contact_net) ? true
        : *IsMultiplayerPBEM && Rules->tgl_humans_always_contact_pbem;
}

/*
Purpose: Determine if the overall dominant human faction is a minor threat based on base count.
Original Offset: 00539B70
Return Value: Is faction minor threat? true/false
Status: Complete
*/
BOOL __cdecl great_beelzebub(uint32_t faction_id, BOOL is_aggressive) {
    if (is_human(faction_id) && FactionRankings[7] == faction_id) {
        uint32_t bases_threat = (*TurnCurrentNum + 25) / 50;
        if (bases_threat < 4) {
            bases_threat = 4;
        }
        if (PlayersData[faction_id].current_num_bases > bases_threat
            && (PlayersData[faction_id].diff_level > DLVL_SPECIALIST
                || *GameRules & RULES_INTENSE_RIVALRY || is_aggressive)) {
            return true;
        }
    }
    return false;
}

/*
Purpose: Determine if the specified faction is considered a threat based on the game state and 
         ranking.
Original Offset: 00539C00
Return Value: Is the specified faction a threat? true/false
Status: Complete
*/
BOOL __cdecl great_satan(uint32_t faction_id, BOOL is_aggressive) {
    if (great_beelzebub(faction_id, is_aggressive)) {
        BOOL has_intense_riv = (*GameRules & RULES_INTENSE_RIVALRY);
        if (*TurnCurrentNum <= ((has_intense_riv ? 0 
            : (DLVL_TRANSCEND - PlayersData[faction_id].diff_level) * 50) + 100)) {
            return false;
        }
        if (climactic_battle() && aah_ooga(faction_id, -1) == faction_id) {
            return true;
        }
        uint32_t diff_factor;
        uint32_t factor;
        if (has_intense_riv) {
            factor = 4;
            diff_factor = DLVL_TRANSCEND;
        } else if (PlayersData[faction_id].diff_level >= DLVL_LIBRARIAN 
            || *GameRules & RULES_VICTORY_CONQUEST || *ObjectiveReqVictory <= 1000) {
            factor = 2;
            diff_factor = DLVL_LIBRARIAN;
        } else {
            factor = 1;
            diff_factor = DLVL_TALENT;
        }
        return (factor * FactionRankingsUnk[FactionRankings[7]] 
            >= diff_factor * FactionRankingsUnk[FactionRankings[6]]);
    }
    return false;
}

/*
Purpose: Check whether the specified faction is nearing the diplomatic victory requirements to be 
         able to call a Supreme Leader vote. Optional 2nd parameter (0/-1 to disable) that specifies
         a faction to skip if they have a pact with faction from the 1st parameter.
Original Offset: 00539D40
Return Value: Faction id if nearing diplomatic victory or zero
Status: Complete
*/
uint32_t __cdecl aah_ooga(int faction_id, int pact_faction_id) {
    if (!(*GameRules & RULES_VICTORY_DIPLOMATIC)) {
        return 0; // Diplomatic Victory not allowed
    }
    uint32_t votes_total = 0;
    for (uint32_t i = 1; i < MaxPlayerNum; i++) {
        votes_total += council_votes(i);
    }
    uint32_t faction_id_ret = 0;
    for (int i = 1; i < MaxPlayerNum; i++) {
        if (i != pact_faction_id
            && (pact_faction_id <= 0 || !has_treaty(i, pact_faction_id, DTREATY_PACT)
                || !(*GameRules & RULES_VICTORY_COOPERATIVE))) {
            int proposal_preq = Proposal[PROP_UNITE_SUPREME_LEADER].preq_tech;
            if ((has_tech(proposal_preq, faction_id)
                || (proposal_preq >= 0 
                    && (has_tech(Technology[proposal_preq].preq_tech_1, faction_id)
                    || has_tech(Technology[proposal_preq].preq_tech_2, faction_id))))
                && council_votes(i) * 2 >= votes_total && (!faction_id_ret || i == faction_id)) {
                faction_id_ret = i;
            }
        }
    }
    return faction_id_ret;
}

/*
Purpose: Check if the human controlled player is nearing the endgame.
Original Offset: 00539E40
Return Value: Is human player nearing endgame? true/false
Status: Complete
*/
BOOL __cdecl climactic_battle() {
    for (uint32_t i = 1; i < MaxPlayerNum; i++) {
        if (is_human(i) && PlayersData[i].corner_market_turn > *TurnCurrentNum) {
            return true; // Human controlled player initiated Corner Global Energy Market
        }
    }
    if (aah_ooga(0, -1)) { // nearing Supreme Leader, these parameters will always return false
        return true; // TODO: Revisit in future once more end game code is complete. This may have
                     //       been effectively disabled as a design decision rather than a bug.
    }
    if (ascending(0)) {
        for (uint32_t i = 1; i < MaxPlayerNum; i++) {
            if (is_human(i) && (has_tech(Facility[FAC_PSI_GATE].preq_tech, i)
                || has_tech(Facility[FAC_VOICE_OF_PLANET].preq_tech, i))) {
                return true; // Human controlled player has tech to build PSI Gates or VoP
            }
        }
    }
    return false;
}

/*
Purpose: Determine if the specified AI faction is at the game climax based on certain conditions.
Original Offset: 00539EF0
Return Value: Is AI faction at climax? true/false
Status: Complete
*/
BOOL __cdecl at_climax(uint32_t faction_id) {
    if (is_human(faction_id) || *GameState & STATE_UNK_1 || *DiffLevelCurrent == DLVL_CITIZEN
        || !climactic_battle()) {
        return false;
    }
    if (aah_ooga(faction_id, faction_id)) {
        return true;
    }
    for (uint32_t i = 1; i < MaxPlayerNum; i++) {
        if (i != faction_id && PlayersData[faction_id].corner_market_turn > *TurnCurrentNum
            && (!has_treaty(faction_id, i, DTREATY_PACT)
                || !(*GameRules & RULES_VICTORY_COOPERATIVE))) {
            return true;
        }
    }
    int trans_most_min_them = 0;
    int trans_most_min_us = 0;
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (Bases[i].queue_production_id[0] == -FAC_ASCENT_TO_TRANSCENDENCE) {
            int min_accum = Bases[i].minerals_accumulated;
            if (Bases[i].faction_id_current == faction_id) {
                if (trans_most_min_us <= min_accum) {
                    trans_most_min_us = min_accum;
                }
            } else if (trans_most_min_them <= min_accum) {
                trans_most_min_them = min_accum;
            }
        }
    }
    for (uint32_t i = 1; i < MaxPlayerNum; i++) {
        if (i != faction_id) {
            if (ascending(i) && !ascending(faction_id)) { // both return same so irrelevant check
                return true;
            }
        }
    }
    return trans_most_min_them && trans_most_min_them > trans_most_min_us;
}

/*
Purpose: Add friction between the two specified factions.
Original Offset: 0053A030
Return Value: n/a
Status: Complete
*/
void __cdecl cause_friction(uint32_t faction_id, uint32_t faction_id_with, int friction) {
    uint32_t *diplo_friction = &PlayersData[faction_id].diplo_friction[faction_id_with];
    *diplo_friction = range(*diplo_friction + friction, 0, 20);
    if (*DiploFrictionFactionID == faction_id && *DiploFrictionFactionIDWith == faction_id_with) {
        *DiploFriction += friction; // not bounded?
    }
}

/*
Purpose: Normalize the diplomatic friction value into a mood offset.
Original Offset: 0053A090
Return Value: Mood (0-8)
Status: Complete
*/
uint32_t __cdecl get_mood(int friction) {
    if (friction <= 0) {
        return MOOD_MAGNANIMOUS;
    }
    if (friction <= 2) {
        return MOOD_SOLICITOUS;
    }
    if (friction <= 4) {
        return MOOD_COOPERATIVE;
    }
    if (friction <= 8) {
        return MOOD_NONCOMMITTAL;
    }
    if (friction <= 12) {
        return MOOD_AMBIVALENT;
    }
    if (friction <= 15) {
        return MOOD_OBSTINATE;
    }
    if (friction <= 17) {
        return MOOD_QUARRELSOME;
    }
    return (friction > 19) + MOOD_BELLIGERENT; // or MOOD_SEETHING if condition is true
}

/*
Purpose: Calculate the negative reputation the specified faction has with another.
Original Offset: 0053A100
Return Value: Bad reputation
Status: Complete
*/
uint32_t __cdecl reputation(uint32_t faction_id, uint32_t faction_id_with) {
    return range(PlayersData[faction_id].integrity_blemishes
        - PlayersData[faction_id].diplo_unk1[faction_id_with], 0, 99);
}

/*
Purpose: Calculate the amount of patience the specified faction has with another.
Original Offset: 0053A150
Return Value: Patience
Status: Complete
*/
int __cdecl get_patience(uint32_t faction_id_with, uint32_t faction_id) {
    if (has_treaty(faction_id, faction_id_with, DTREATY_VENDETTA)) {
        return 1;
    }
    if (has_treaty(faction_id, faction_id_with, DTREATY_PACT)) {
        return has_treaty(faction_id, faction_id_with, DTREATY_HAVE_SURRENDERED) ? 500 : 6;
    }
    return (has_treaty(faction_id, faction_id_with, DTREATY_TREATY) != 0) 
        - ((*DiploFriction + 3) / 8) + 3;
}

/*
Purpose: Calculate the amount of goodwill a loan will generate. This is used to reduce friction.
Original Offset: 0053A1C0
Return Value: Goodwill (friction reduction amount)
Status: Complete
*/
uint32_t __cdecl energy_value(uint32_t loan_principal) {
    uint32_t goodwill = 0;
    uint32_t divisor = 2;
    for (int weight = 10, energy = loan_principal / 5; energy > 0; energy -= weight, weight = 20) {
        goodwill += ((weight >= 0) ? ((energy > weight) ? weight : energy) : 0) / divisor++;
    }
    return (goodwill + 4) / 5;
}

/*
Purpose: Scan another faction's active, built, non-obsolete designs for the one this faction would
         most like (DiploProtoIDBest) and the one it could obtain in diplomacy
         (DiploProtoIDTradeable: not flagged against trading, a combat or recon plan). A design
         is rated by weapon, armor, plan, chassis speed and reactor; it is only worth full value
         when none of this faction's own designs of the same plan and triad matches or beats it
         (the original compares armor with arm_strat() on the weapon ids, which is kept).
Original Offset: 0053A4A0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch9.log)
*/
void __cdecl scan_prototypes(uint32_t faction_id, uint32_t faction_id_other) {
    *DiploProtoIDBest = -1;
    *DiploProtoIDTradeable = -1;
    int score_best = 0;
    int score_trade = 0;
    uint32_t treaties = PlayersData[faction_id].diplo_treaties[faction_id_other];
    for (int i = 0; i < MaxVehProtoFactionNum; i++) {
        int proto_id = faction_id_other * MaxVehProtoFactionNum + i;
        VehPrototype *proto = &VehPrototypes[proto_id];
        if (!(proto->flags & PROTO_ACTIVE) || !(proto->flags & PROTO_TYPED_COMPLETE)
            || (proto->obsolete_factions & (1 << faction_id_other))) {
            continue;
        }
        BOOL better = true;
        int score = Weapon[proto->weapon_id].offense_rating * 3
            + ((7 - proto->plan) * 0x4000 + Armor[proto->armor_id].defense_rating) * 4
            + Chassis[proto->chassis_id].speed;
        for (int j = 0; j < MaxVehProtoFactionNum && better; j++) {
            VehPrototype *own = &VehPrototypes[faction_id * MaxVehProtoFactionNum + j];
            if (!(own->flags & PROTO_ACTIVE) || !(own->flags & PROTO_TYPED_COMPLETE)
                || (own->obsolete_factions & (1 << faction_id)) || own->plan != proto->plan
                || Chassis[own->chassis_id].triad != Chassis[proto->chassis_id].triad) {
                continue;
            }
            if (proto->reactor_id < own->reactor_id) {
                better = false;
            }
            if (Chassis[proto->chassis_id].speed < Chassis[own->chassis_id].speed) {
                better = false;
            }
            int weapon_own = weap_strat(own->weapon_id, faction_id);
            int weapon_other = weap_strat(proto->weapon_id, faction_id);
            int armor_own = arm_strat(own->weapon_id, faction_id);
            int armor_other = arm_strat(proto->weapon_id, faction_id);
            if (Chassis[proto->chassis_id].speed <= Chassis[own->chassis_id].speed
                && weapon_other <= weapon_own && armor_other <= armor_own
                && (weapon_other < weapon_own || armor_other < armor_own)) {
                better = false;
            }
        }
        int value = better ? score : score >> 4;
        if (value > score_best) {
            *DiploProtoIDBest = proto_id;
            score_best = value;
        }
        uint32_t no_trade = (treaties & DTREATY_PACT) ? PROTO_UNK_20 : (PROTO_UNK_10 | PROTO_UNK_20);
        uint32_t plan = proto->plan;
        if (!(proto->flags & no_trade) && plan < 7 && plan != 3) {
            int trade = score;
            if (!better) {
                trade = ((treaties & DTREATY_VENDETTA) && plan == 2) ? score >> 4 : 0;
            }
            if (trade > score_trade) {
                *DiploProtoIDTradeable = proto_id;
                score_trade = trade;
            }
        }
    }
}

/*
Purpose: Check whether a base of faction_id can reach a base of faction_id_2 (in the specified
         region, or anywhere with region 0, in which case both must have bases there). The map's
         path markers are cleared, then for each base of faction_id not on a marked tile the
         faction's territory around it is marked and Path::find() is tried from it to each base
         of faction_id_2 on an unmarked tile of the same region (after marking that faction's
         territory around the target). unk picks the wider Path::find() mode (0xE01 over 0x201).
Original Offset: 0053A780
Return Value: Is there a path? true/false
Status: Complete
*/
BOOL __cdecl contiguous(int faction_id, int faction_id_2, uint32_t region, BOOL unk) {
    if (region && (!PlayersData[faction_id].region_total_bases[region]
        || !PlayersData[faction_id_2].region_total_bases[region])) {
        return false;
    }
    for (int i = 0; i < (int)*MapArea; i++) {
        (*MapTiles)[i].unk_1 = 0;
    }
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        Base *base = &Bases[base_id];
        if (base->faction_id_current != faction_id) {
            continue;
        }
        Map *tile = map_loc(base->x, base->y);
        uint32_t base_region = tile->region;
        if ((region && base_region != region) || tile->unk_1) {
            continue;
        }
        Paths->territory(base->x, base->y, base_region, faction_id);
        for (int base_id_2 = 0; base_id_2 < *BaseCurrentCount; base_id_2++) {
            Base *base_2 = &Bases[base_id_2];
            if (base_2->faction_id_current != faction_id_2) {
                continue;
            }
            Map *tile_2 = map_loc(base_2->x, base_2->y);
            if (tile_2->region != base_region || tile_2->unk_1) {
                continue;
            }
            Paths->territory(base_2->x, base_2->y, base_region, faction_id_2);
            if (Path_find(Paths, NULL, base->x, base->y, base_2->x, base_2->y, faction_id,
                faction_id_2, unk ? 0xE01 : 0x201, -1) >= 0) {
                return true;
            }
        }
    }
    return false;
}

/*
Purpose: Copy a faction leader's title and name and the faction's noun into three consecutive
         parse slots, each with its gender and plurality, as the diplomacy notices do.
Original Offset: n/a (inlined in the original)
Return Value: n/a
Status: Complete
*/
static void parse_leader(uint32_t slot, int faction_id) {
    parse_says(slot, get_title(faction_id), -1, -1);
    parse_says(slot + 1, get_name(faction_id), -1, -1);
    parse_says(slot + 2, get_noun(faction_id), -1, -1);
}

/*
Purpose: Conclude a Pact of Brotherhood between the faction and the faction it is talking with:
         pact flags (relayed in a network game), the other side's patience reset, the last contact
         set, both reputations raised. MAKEPACT is shown, the maps are exchanged (HERESTHEMAP),
         prototypes are scanned and offered, and at low difficulty or the first time the PACT
         tutorial message follows (not in network games).
Original Offset: 0053CAF0
Return Value: n/a
Status: Complete
*/
void __cdecl pact_of_brotherhood(int faction_id, int faction_id_with) {
    *DiploSecondFaction = faction_id_with;
    net_treaty_on(faction_id_with, faction_id, DTREATY_PACT | DTREATY_UNK_1000000, false);
    PlayersData[faction_id_with].diplo_patience[faction_id] = 0;
    PlayersData[faction_id].diplo_spoke[faction_id_with] = *TurnCurrentNum;
    diplomacy_check(faction_id, faction_id_with, 0);
    *DiploUnk2 = 0;
    PlayersData[faction_id].global_reputation++;
    PlayersData[faction_id_with].global_reputation++;
    diplomacy_caption(faction_id, faction_id_with);
    parse_leader(0, faction_id_with);
    parse_says(3, get_pact(faction_id), -1, -1);
    parse_says(4, get_phrase(faction_id, 0), -1, -1);
    parse_says(5, get_phrase(faction_id_with, 0), -1, -1);
    X_pops_11("MAKEPACT", 0x100000, FactionPortraits[faction_id_with], pop_wait);
    net_maps(faction_id, faction_id_with, true);
    parse_says(0, get_pact(faction_id), -1, -1);
    X_pops_11("HERESTHEMAP", 0x100000, FactionPortraits[faction_id_with], pop_wait);
    scan_prototypes(faction_id, faction_id_with);
    mention_prototypes(faction_id, faction_id_with);
    if ((*DiffLevelCurrent <= DLVL_SPECIALIST || tut_check2(0x1000)) && !*IsMultiplayerNet) {
        parse_says(0, get_noun(faction_id_with), -1, -1);
        parse_says(1, get_title(faction_id_with), -1, -1);
        parse_says(2, get_name(faction_id_with), -1, -1);
        parse_says(3, Players[faction_id_with].adj_name_faction, -1, -1);
        X_pop_2(TutorTxtID, "PACT", NULL);
    }
}

/*
Purpose: Conclude a treaty between the faction and the faction it is talking with: treaty flags
         on both sides, the other side's patience reset, both reputations raised. Then show the
         MAKETREATY dialog (one of three, picked at random), the COMMERCE tutorial message and,
         if units of the other faction are inside this faction's territory, TIMETOLEAVE.
Original Offset: 0053CE10
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl make_treaty(int faction_id, int faction_id_with) {
    *DiploSecondFaction = faction_id_with;
    net_treaty_on(faction_id_with, faction_id,
        DTREATY_TREATY | DTREATY_UNK_200 | DTREATY_UNK_1000000, true);
    PlayersData[faction_id_with].diplo_patience[faction_id] = 0;
    PlayersData[faction_id].diplo_spoke[faction_id_with] = *TurnCurrentNum;
    diplomacy_check(faction_id, faction_id_with, 0);
    PlayersData[faction_id].global_reputation++;
    PlayersData[faction_id_with].global_reputation++;
    diplomacy_caption(faction_id, faction_id_with);
    parse_leader(0, faction_id);
    parse_leader(3, faction_id_with);
    parse_says(6, get_noun_phrase(faction_id, 9), -1, -1);
    parse_says(7, get_noun_phrase(faction_id_with, 9), -1, -1);
    stringTemp->str[0] = 0;
    strcat_s(stringTemp->str, sizeof(char1032), "MAKETREATY");
    char num[80];
    _itoa_s((int)random(0, 2), num, 10);
    strcat_s(stringTemp->str, sizeof(char1032), num);
    X_pops_11(stringTemp->str, 0x100000, FactionPortraits[faction_id_with], pop_wait);
    if ((*DiffLevelCurrent <= DLVL_SPECIALIST || tut_check2(0x800)) && !*IsMultiplayerNet) {
        parse_says(0, get_noun(faction_id_with), -1, -1);
        X_pop_2(TutorTxtID, "COMMERCE", NULL);
    }
    if (territory(faction_id_with, faction_id, 2, NULL, NULL)) {
        parse_says(0, get_noun(faction_id_with), -1, -1);
        parse_says(1, Players[faction_id_with].adj_name_faction, -1, -1);
        NetMsg_pop(NetMsgVar, 0, "TIMETOLEAVE", 5000, 0, NULL);
    }
}

/*
Purpose: Agree a truce between the faction and the faction it is talking with: truce flags on
         both sides and both reputations raised; with show set, the MAKETRUCE dialog follows.
         In a network game the truce is sent without waiting for it to come back.
Original Offset: 0053D160
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl pledge_truce(int faction_id, int faction_id_with, BOOL show) {
    *DiploSecondFaction = faction_id_with;
    net_treaty_on(faction_id_with, faction_id, DTREATY_TRUCE | DTREATY_UNK_200, false);
    PlayersData[faction_id].diplo_spoke[faction_id_with] = *TurnCurrentNum;
    diplomacy_check(faction_id, faction_id_with, 0);
    PlayersData[faction_id].global_reputation++;
    PlayersData[faction_id_with].global_reputation++;
    if (show) {
        diplomacy_caption(faction_id, faction_id_with);
        parse_leader(0, faction_id);
        parse_leader(3, faction_id_with);
        X_pops_11("MAKETRUCE", 0x100000, FactionPortraits[faction_id_with], pop_wait);
    }
}

/*
Purpose: Ask the faction being talked to (faction_id_with) to withdraw its units from the
         faction's territory, unless contact fails or it is a human player in a network game.
         The answer is only reported here while sunspots block the commlink.
Original Offset: 0053E4C0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl demands_withdrawal(int faction_id, int faction_id_with) {
    *DiploSecondFaction = faction_id_with;
    if (commlink_attempter(faction_id, faction_id_with)
        || (*IsMultiplayerNet && is_human(faction_id_with))) {
        return;
    }
    int answer = demand_withdrawal(faction_id, faction_id_with);
    if (*SunspotDuration > 0) {
        parse_says(0, get_noun(faction_id_with), -1, -1);
        switch (answer) {
        case 0:
            NetMsg_pop(NetMsgVar, 0, "WITHSUNYES", 5000, 0, NULL);
            break;
        case 1:
            NetMsg_pop(NetMsgVar, 0, "WITHSUNNO", 5000, 0, NULL);
            break;
        case 2:
            NetMsg_pop(NetMsgVar, 0, "WITHSUNWAR", 5000, 0, NULL);
            break;
        default:
            break;
        }
    }
}

/*
Purpose: Renounce the pact with the faction being talked to. Under the diplomatic lock: count the
         blemish on the faction's integrity, drop the pact, tell a computer player through the
         BROKEPACT dialog (unless sunspots block the commlink) and end the pact's arrangements.
Original Offset: 0053E5C0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl renounce_pact(int faction_id, int faction_id_with) {
    *DiploSecondFaction = faction_id_with;
    // the original inlines the lock with a timeout (the dialog asking whether to keep waiting)
    if (commlink_attempter(faction_id, faction_id_with) || diplo_lock(100)) {
        return;
    }
    PlayersData[faction_id].integrity_blemishes++;
    net_treaty_off(faction_id_with, faction_id, DTREATY_PACT, true);
    if (*SunspotDuration <= 0 && !is_human(faction_id_with)) {
        diplomacy_check(faction_id, faction_id_with, 0);
        diplomacy_caption(faction_id, faction_id_with);
        parse_says(0, get_pact(faction_id), -1, -1);
        parse_says(1, get_title(faction_id), -1, -1);
        parse_says(2, get_name(faction_id), -1, -1);
        X_pops_11("BROKEPACT", 0x100000, FactionPortraits[faction_id_with], pop_wait);
    }
    net_pact_ends(faction_id, faction_id_with, true);
    diplo_unlock();
}

/*
Purpose: Bring the pact partners of an attacked faction into the war. Every other living faction
         in a pact with faction_id that has neither a pact nor a vendetta with faction_id_atk
         takes a vendetta against the attacker when either side is a human player (the human
         attacker's last contact with it set to this turn), and the player at the controls is
         told: AIDSTHEM for the attacker, AIDSUS for the faction attacked. The parse slots hold the
         partner's noun, the attacked faction's noun, the partner's leader and the attacker's
         leader and noun.
Original Offset: 0050BA50
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch29.log)
*/
void __cdecl intervention(int faction_id, int faction_id_atk) {
    for (int i = 1; i < MaxPlayerNum; i++) {
        if (i == faction_id || i == faction_id_atk || !is_alive(i)
            || !(PlayersData[i].diplo_treaties[faction_id] & DTREATY_PACT)
            || (PlayersData[i].diplo_treaties[faction_id_atk] & (DTREATY_PACT | DTREATY_VENDETTA))) {
            continue;
        }
        parse_says(0, get_noun(i), -1, -1);
        parse_says(1, get_noun(faction_id), -1, -1);
        parse_says(2, get_title(i), -1, -1);
        parse_says(3, get_name(i), -1, -1);
        parse_says(4, get_title(faction_id_atk), -1, -1);
        parse_says(5, get_name(faction_id_atk), -1, -1);
        parse_says(6, get_noun(faction_id_atk), -1, -1);
        if (is_human(faction_id_atk) || is_human(faction_id)) {
            if (is_human(faction_id_atk)) {
                PlayersData[faction_id_atk].diplo_spoke[i] = *TurnCurrentNum;
            }
            treaty_on(i, faction_id_atk, DTREATY_VENDETTA | DTREATY_COMMLINK | DTREATY_UNK_80000000);
            set_treaty(i, faction_id_atk, DTREATY_UNK_40, true);
        }
        if (faction_id_atk == *FactionIDCurrent) {
            X_pop("AIDSTHEM", NULL);
        } else if (faction_id == *FactionIDCurrent) {
            X_pop("AIDSUS", NULL);
        }
    }
}

/*
Purpose: A human attacker's betrayal makes the betrayed faction want revenge: the agenda flag
         0x400 becomes 0x800, a revenge wish becomes agenda flag 0x400, and the revenge wish is set.
Original Offset: n/a (inlined three times in double_cross)
Return Value: n/a
Status: Complete
*/
static void double_cross_revenge(int faction_id_atk, int faction_id_def) {
    if (PlayersData[faction_id_def].diplo_agenda[faction_id_atk] & DAGENDA_UNK_400) {
        set_agenda(faction_id_def, faction_id_atk, DAGENDA_UNK_800, true);
    }
    if (PlayersData[faction_id_def].diplo_treaties[faction_id_atk] & DTREATY_WANT_REVENGE) {
        set_agenda(faction_id_def, faction_id_atk, DAGENDA_UNK_400, true);
    }
    set_treaty(faction_id_def, faction_id_atk, DTREATY_WANT_REVENGE, true);
}

/*
Purpose: The attacker breaks its agreements with the defender and goes to war (faction_id_other is
         the faction it does so for, or -1). In a network game the player at the controls being
         betrayed is told: INCITED (with the other faction) or VENDETTAWARNING from a human
         attacker. The defender's loan to the attacker is forgiven. Breaking a pact (unless agenda
         flag 0x20 excuses it) costs a point of reputation and integrity (none against a faction
         that committed a major atrocity or one the attacker is the atrocity victim of, none from a
         Citizen-level attacker, and none if the attacker is closer to the other faction), counts a
         wrong and a betrayal and annoys the other faction; then the pact ends with a vendetta.
         Breaking a treaty or truce does the same with a smaller toll (the reputation point unless
         flag 0x10000, integrity for a threat or a top attacker of a low ranked defender), and a
         vendetta without one only annoys the other faction. A human attacker makes the defender
         want revenge. Unless agenda flag 0x20 excuses it, war involving a human sets flag 0x40
         (the attacker's last contact this turn) and the defender's allies intervene.
Original Offset: 0050BCC0
Return Value: n/a
Status: Complete
*/
void __cdecl double_cross(int faction_id_atk, int faction_id_def, int faction_id_other) {
    if (*IsMultiplayerNet) {
        if (faction_id_def == *FactionIDCurrent && faction_id_other > 0) {
            parse_leader(0, faction_id_other);
            parse_leader(3, faction_id_atk);
            NetMsg_pop(NetMsgVar, 0, "INCITED", 5000, 0, NULL);
        } else if (faction_id_def == *FactionIDCurrent
            && FactionsStatus[0] & (1 << faction_id_atk)) {
            parse_leader(0, faction_id_atk);
            NetMsg_pop(NetMsgVar, 0, "VENDETTAWARNING", 5000, 0, NULL);
        }
    }
    PlayerData *atk = &PlayersData[faction_id_atk];
    PlayerData *def = &PlayersData[faction_id_def];
    def->loan_balance[faction_id_atk] = 0;
    uint32_t treaties = atk->diplo_treaties[faction_id_def];
    BOOL excused = (treaties & DTREATY_ATROCITY_VICTIM) || def->major_atrocities;
    BOOL closer_to_other = false; // the attacker is on better terms with the other faction
    if (faction_id_other >= 0) {
        uint32_t treaties_other = atk->diplo_treaties[faction_id_other];
        if (treaties_other & DTREATY_PACT) {
            closer_to_other = !(treaties & DTREATY_PACT);
        } else if (treaties_other & DTREATY_TREATY) {
            closer_to_other = !(treaties & DTREATY_TREATY);
        } else if (treaties_other & DTREATY_TRUCE) {
            closer_to_other = !(treaties & DTREATY_TRUCE);
        }
        atk->diplo_unk1[faction_id_other]++;
    }
    if (atk->diplo_treaties[faction_id_def] & DTREATY_PACT) {
        if (!(atk->diplo_agenda[faction_id_def] & DAGENDA_UNK_20)) {
            int reputation_new = atk->global_reputation - 1;
            atk->global_reputation = (reputation_new < 0) ? 0 : reputation_new;
            if (atk->diff_level && !excused && !closer_to_other) {
                atk->integrity_blemishes++;
                def->diplo_betrayed[faction_id_atk]++;
            }
            if (faction_id_other < 0) {
                if (!excused) {
                    atk->integrity_blemishes++;
                }
                atk->diplo_wrongs[faction_id_def]++;
                def->diplo_betrayed[faction_id_atk]++;
            } else {
                cause_friction(faction_id_other, faction_id_atk, -4);
            }
            if (FactionsStatus[0] & (1 << faction_id_atk)) {
                double_cross_revenge(faction_id_atk, faction_id_def);
            }
        }
        pact_ends(faction_id_atk, faction_id_def);
        treaty_on(faction_id_atk, faction_id_def, DTREATY_VENDETTA);
        return;
    }
    treaties = atk->diplo_treaties[faction_id_def];
    if (!(atk->diplo_agenda[faction_id_def] & DAGENDA_UNK_20)) {
        if (treaties & (DTREATY_TREATY | DTREATY_TRUCE)) {
            if (!(treaties & DTREATY_UNK_10000)) {
                int reputation_new = atk->global_reputation - 1;
                atk->global_reputation = (reputation_new < 0) ? 0 : reputation_new;
                if ((great_satan(faction_id_atk, false)
                    || ((int)atk->ranking >= 6 && (int)def->ranking <= 4))
                    && faction_id_other < 0 && !excused) {
                    atk->integrity_blemishes++;
                    def->diplo_betrayed[faction_id_atk]++;
                }
            }
            if (atk->diff_level && !excused && !closer_to_other) {
                atk->integrity_blemishes++;
                def->diplo_betrayed[faction_id_atk]++;
            }
            if (faction_id_other < 0) {
                atk->diplo_wrongs[faction_id_def]++;
            }
            if (FactionsStatus[0] & (1 << faction_id_atk)) {
                double_cross_revenge(faction_id_atk, faction_id_def);
            }
            if (faction_id_other >= 0) {
                cause_friction(faction_id_other, faction_id_atk, -2);
            }
        } else if (faction_id_other >= 0) {
            if (FactionsStatus[0] & (1 << faction_id_atk)) {
                double_cross_revenge(faction_id_atk, faction_id_def);
            }
            cause_friction(faction_id_other, faction_id_atk, -2);
        }
    }
    treaty_on(faction_id_atk, faction_id_def, DTREATY_VENDETTA);
    if (!(atk->diplo_agenda[faction_id_def] & DAGENDA_UNK_20)) {
        // vanilla quirk: compares the two masked bits, not whether each side is human, so this
        // holds for any two human factions too and fails only for two AI factions
        if ((FactionsStatus[0] & (1 << faction_id_atk))
            != (FactionsStatus[0] & (1 << faction_id_def))) {
            set_treaty(faction_id_atk, faction_id_def, DTREATY_UNK_40, true);
            atk->diplo_spoke[faction_id_def] = *TurnCurrentNum;
        }
        intervention(faction_id_def, faction_id_atk);
    }
}

/*
Purpose: Offer the faction being talked to a swap of prototypes (the design picked by
         scan_prototypes()), if it is willing to speak. Its designs are marked as offered (0x10).
         With a pact the offer text is built (PROTOPACT) but the original shows nothing and stops
         there, leaving the picked designs set. Otherwise PROTORIVAL is shown and, if accepted,
         each faction gets every design of the other it wants (as tech 97 + design), and both
         sides' designs are marked as traded (0x20).
Original Offset: 0053A230
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch29.log)
*/
void __cdecl mention_prototypes(int faction_id, int faction_id_with) {
    *DiploSecondFaction = faction_id_with;
    if (*DiploProtoIDTradeable < 0 || !wants_to_speak(faction_id, faction_id_with)) {
        return;
    }
    for (int i = 0; i < MaxVehProtoFactionNum; i++) {
        VehPrototypes[faction_id_with * MaxVehProtoFactionNum + i].flags |= PROTO_UNK_10;
    }
    int proto_id = *DiploProtoIDTradeable;
    parse_says(0, VehPrototypes[proto_id].veh_name, -1, -1);
    stringTemp->str[0] = 0;
    say_stats_2(stringTemp->str, proto_id);
    parse_says(1, stringTemp->str, -1, -1);
    VehPrototypes[proto_id].flags |= PROTO_UNK_10;
    stringTemp->str[0] = 0;
    strcat_s(stringTemp->str, sizeof(char1032), "PROTO");
    if (PlayersData[faction_id].diplo_treaties[faction_id_with] & DTREATY_PACT) {
        VehPrototypes[proto_id].flags |= PROTO_UNK_20;
        strcat_s(stringTemp->str, sizeof(char1032), "PACT");
        return; // as in the original: no dialog, the picked designs are kept
    }
    strcat_s(stringTemp->str, sizeof(char1032), "RIVAL");
    if (X_pops_11(stringTemp->str, 0x100000, FactionPortraits[faction_id_with], pop_wait)) {
        for (int i = 0; i < MaxVehProtoFactionNum; i++) {
            int proto_id_own = faction_id * MaxVehProtoFactionNum + i;
            int proto_id_with = faction_id_with * MaxVehProtoFactionNum + i;
            if (wants_prototype(proto_id_own, faction_id_with)) {
                net_tech(faction_id_with, proto_id_own + 97, faction_id, true);
            }
            VehPrototypes[proto_id_own].flags |= PROTO_UNK_20;
            if (wants_prototype(proto_id_with, faction_id)) {
                net_tech(faction_id, proto_id_with + 97, faction_id_with, true);
            }
            VehPrototypes[proto_id_with].flags |= PROTO_UNK_20;
        }
    }
    *DiploProtoIDBest = -1;
    *DiploProtoIDTradeable = -1;
}

/*
Purpose: Move the faction's units out of the territory of the faction it no longer has a pact
         with: each unit whose nearest base is that faction's, or that stands in its territory,
         goes to the closest of its own faction's bases (sea units only to bases that pass the
         coast checks below), with its orders cleared. Kept from the original: the coast check
         for sea units passes the base's x coordinate as a base index and its y coordinate as the
         radius flag of is_coast().
Original Offset: 0053C370
Return Value: Number of units moved
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch29.log)
*/
int __cdecl pact_withdraw(int faction_id, int faction_id_with) {
    int moved = 0;
    for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
        Veh *veh = &Vehs[veh_id];
        if (veh->faction_id != faction_id) {
            continue;
        }
        int base_id = base_find(veh->x, veh->y);
        if (base_id < 0 || (Bases[base_id].faction_id_current != faction_id_with
            && whose_territory(faction_id, veh->x, veh->y, NULL, false) != faction_id_with)) {
            continue;
        }
        int base_id_best = -1;
        int dist_best = 9999;
        for (int i = 0; i < *BaseCurrentCount; i++) {
            Base *base = &Bases[i];
            if (base->faction_id_current != faction_id) {
                continue;
            }
            if (Chassis[VehPrototypes[veh->proto_id].chassis_id].triad == TRIAD_SEA) {
                Base *base_x = &Bases[base->x];
                if (!is_coast(base_x->x, base_x->y, base->y)) {
                    continue;
                }
                if (is_ocean(veh->x, veh->y)
                    && !base_on_sea(i, map_loc(veh->x, veh->y)->region)) {
                    continue;
                }
            }
            int dist_x = abs(base->x - veh->x);
            if (!(*MapIsFlat & 1) && dist_x > (int)*MapLongitude) {
                dist_x = *MapLongitudeBounds - dist_x;
            }
            int dist = vector_dist(dist_x, base->y - veh->y);
            if (dist < dist_best) {
                dist_best = dist;
                base_id_best = i;
            }
        }
        if (base_id_best < 0) {
            continue;
        }
        veh_drop(veh_lift(veh_id), Bases[base_id_best].x, Bases[base_id_best].y);
        veh->order = ORDER_NONE;
        veh->state &= 0xFCFFBDFF; // clears 0x200, 0x4000, 0x1000000 and 0x2000000
        if (Chassis[VehPrototypes[veh->proto_id].chassis_id].triad == TRIAD_AIR) {
            veh->terraforming_turns = 0;
        }
        moved++;
        if (*MapWinUnitNotViewMode && veh_id == *MapWinVehIDSelected) {
            Console_set_move(ConsoleVar, NULL, 1);
        }
    }
    return moved;
}

/*
Purpose: Offer to buy from the faction being talked to the technology it has to give (type 5:
         the prototype picked by scan_prototypes(), as tech 97 + design; type 13: the commlink
         offer naming DiploUnk8's faction). The price is the tech's value to the seller scaled by
         the turn, doubled for a great satan and with DiploUnk7, raised by half with markup,
         divided by the number of other factions that have it (1 to 999), lowered for a pact
         partner ahead in rank and tech, rounded up to 25 and at least 100 (but for type 13).
         When the faction can't pay, unk1 or type 5 still makes the offer (BUY...HIGH). An
         accepted offer pays the energy, hands over the tech and refreshes the display.
Original Offset: 005401A0
Return Value: Was an offer made? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch29.log)
*/
int __cdecl buy_tech(int faction_id, int faction_id_with, int unk1, BOOL markup, int type) {
    *DiploSecondFaction = faction_id_with;
    if (type == 5) {
        if (*DiploProtoIDBest < 0) {
            scan_prototypes(faction_id, faction_id_with);
            if (*DiploProtoIDBest < 0) {
                return false;
            }
        }
        *DiploTechGetID1 = *DiploProtoIDBest + 97;
    } else if (type != 13) {
        if (*DiploTechGetID1 < 0) {
            return false;
        }
        if (!strcmp(Players[faction_id_with].filename, "UNIV") && *DiploTechGiveID1 >= 0) {
            return false;
        }
    }
    if (unk1 == 1) {
        return false;
    }
    int cost = tech_val(*DiploTechGetID1, faction_id_with, true) * ((*TurnCurrentNum + 7) / 8);
    if (great_satan(faction_id, false)) {
        cost *= 2;
    }
    if (*DiploUnk7) {
        cost *= 2;
    }
    if (markup) {
        cost = cost * 3 / 2;
    }
    int owners = (int)bit_count(GameTechAchieved[*DiploTechGetID1]) - 1;
    cost /= (owners < 1) ? 1 : ((owners > 999) ? 999 : owners);
    PlayerData *p = &PlayersData[faction_id];
    PlayerData *p_with = &PlayersData[faction_id_with];
    if ((p->diplo_treaties[faction_id_with] & DTREATY_PACT) && (int)p_with->ranking > (int)p->ranking) {
        if (p_with->tech_ranking > p->tech_ranking + 8) {
            cost /= 4;
        } else if (p_with->tech_ranking > p->tech_ranking) {
            cost /= 2;
        }
    }
    cost = (cost + 25) / 25 * 25;
    if (type != 13 && cost < 100) {
        cost = 100;
    }
    if (p->energy_reserves < cost && unk1 != 5 && type != 5) {
        return false;
    }
    stringTemp->str[0] = 0;
    say_tech(stringTemp->str, *DiploTechGetID1, true);
    parse_says(0, stringTemp->str, -1, -1);
    parse_num(0, cost);
    stringTemp->str[0] = 0;
    strcat_s(stringTemp->str, sizeof(char1032), "BUY");
    if (type == 5) {
        strcat_s(stringTemp->str, sizeof(char1032), "PROTO");
    } else if (type == 13) {
        strcat_s(stringTemp->str, sizeof(char1032), "COMMLINK");
        parse_says(0, Players[*DiploUnk8].adj_name_faction, -1, -1);
    } else {
        strcat_s(stringTemp->str, sizeof(char1032), "TECH");
    }
    char number[80];
    if (p->energy_reserves < cost) {
        strcat_s(stringTemp->str, sizeof(char1032), "HIGH");
        _itoa_s((type == 5 || type == 13) ? 0 : (int)random(0, 2), number, 10);
        strcat_s(stringTemp->str, sizeof(char1032), number);
        X_pops_11(stringTemp->str, 0x100000, FactionPortraits[faction_id_with], pop_wait);
        return true;
    }
    _itoa_s((type == 5 || type == 13) ? 0 : (int)random(0, 2), number, 10);
    strcat_s(stringTemp->str, sizeof(char1032), number);
    if (X_pops_11(stringTemp->str, 0x100000, FactionPortraits[faction_id_with], pop_wait) == 1) {
        net_energy(faction_id, -cost, faction_id_with, cost, false);
        net_tech(faction_id, *DiploTechGetID1, faction_id_with, true);
        if (type == 5) {
            *DiploProtoIDBest = -1;
        }
        *InfoWinUnk = 0;
        Console_update_data(ConsoleVar, NULL, 0);
        diplomacy_caption(faction_id, faction_id_with);
    }
    return true;
}

/*
Purpose: The faction being talked to (faction_id_with) answers a demand to withdraw its units from
         the faction's territory. Without a pact or the flag 0x200, an angry enough mood (the
         diplomacy_check() values and the friction against 5 or 15) makes it declare a vendetta
         (NEVER, answer 2). A treaty partner with a reason to stay (dont_withdrawal()) breaks the
         treaty instead (RUNNINGDOG, answer 2). Otherwise, unless it is a treaty partner, its
         friction rises and it may refuse (NOWAYJOSE, answer 1); if the faction's own units stand
         in its territory it asks for them to leave as well (TURNABOUT) and withdraws its own
         units when that is agreed. Then its units withdraw and it reports how (WITHDRAWPACT,
         WITHDRAW, WITHDRAWTRUCE, or NOWITHDRAW when nothing moved). Dialogs are skipped while
         sunspots block the commlink.
Original Offset: 00549270
Return Value: 0 withdrew, 1 refused, 2 vendetta or broken treaty
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch29.log)
*/
int __cdecl demand_withdrawal(int faction_id, int faction_id_with) {
    BOOL turnabout = false;
    *DiploSecondFaction = faction_id_with;
    diplomacy_caption(faction_id, faction_id_with);
    uint32_t treaties_with = PlayersData[faction_id_with].diplo_treaties[faction_id];
    if (!(treaties_with & DTREATY_UNK_200) && !(treaties_with & DTREATY_PACT)
        && (*DiploUnk2 || *DiploUnk3 || (*DiploFriction > (*DiploUnk4 ? 5 : 15) && !*DiploUnk5))) {
        net_treaty_on(faction_id_with, faction_id, DTREATY_VENDETTA, true);
        PlayersData[faction_id].diplo_spoke[faction_id_with] = *TurnCurrentNum;
        if (*SunspotDuration <= 0) {
            diplomacy_caption(faction_id, faction_id_with);
            parse_says(0, get_title(faction_id), -1, -1);
            parse_says(1, get_name(faction_id), -1, -1);
            stringTemp->str[0] = 0;
            strcat_s(stringTemp->str, sizeof(char1032), "NEVER");
            X_pops_11(stringTemp->str, 0x100000, FactionPortraits[faction_id_with], pop_wait);
        }
        return 2;
    }
    uint32_t *treaties = &PlayersData[faction_id].diplo_treaties[faction_id_with];
    if (!(*treaties & DTREATY_PACT)) {
        int faction_id_reason = dont_withdrawal(faction_id, faction_id_with);
        if (faction_id_reason && (*treaties & DTREATY_TREATY)) {
            net_double_cross(faction_id_with, faction_id, -1, true);
            if (*SunspotDuration > 0) {
                return 2;
            }
            parse_says(0, get_title(faction_id), -1, -1);
            parse_says(1, get_name(faction_id), -1, -1);
            parse_says(2, get_title(faction_id_reason), -1, -1);
            parse_says(3, get_name(faction_id_reason), -1, -1);
            X_pops_11("RUNNINGDOG", 0x100000, FactionPortraits[faction_id_with], pop_wait);
            *DiploUnk2 = 0;
            PlayersData[faction_id].diplo_spoke[faction_id_with] = *TurnCurrentNum;
            return 2;
        }
    }
    if (!(*treaties & DTREATY_TREATY)) {
        int *friction = (int *)&PlayersData[faction_id_with].diplo_friction[faction_id];
        int value = *friction + 1;
        *friction = (value < 0) ? 0 : ((value > 20) ? 20 : value);
        if ((int)*DiploFrictionFactionID == faction_id_with
            && (int)*DiploFrictionFactionIDWith == faction_id) {
            (*DiploFriction)++;
        }
        if ((PlayersData[faction_id_with].diplo_treaties[faction_id] & DTREATY_WANT_REVENGE)
            || *DiploUnk2 || *DiploUnk3 || *DiploUnk6 > *DiploUnk5 + 3 || (*DiploUnk4 && !*DiploUnk5)) {
            if (*SunspotDuration <= 0) {
                parse_says(0, get_title(faction_id), -1, -1);
                parse_says(1, get_name(faction_id), -1, -1);
                X_pops_11("NOWAYJOSE", 0x100000, FactionPortraits[faction_id_with], pop_wait);
            }
            return 1;
        }
        if (territory(faction_id_with, faction_id, 0, NULL, NULL)) {
            if (*SunspotDuration > 0) {
                return 1;
            }
            turnabout = true;
            parse_says(0, get_title(faction_id), -1, -1);
            if (X_pops_11("TURNABOUT", 0x100000, FactionPortraits[faction_id_with], pop_wait)) {
                net_withdrawal(faction_id_with, faction_id, false);
            }
        }
    }
    BOOL withdrew = net_withdrawal(faction_id, faction_id_with, true);
    if (turnabout || *SunspotDuration > 0) {
        return 0;
    }
    parse_says(0, get_name(faction_id), -1, -1);
    parse_says(1, Players[faction_id].adj_leader, -1, -1);
    if (!withdrew) {
        X_pops_11("NOWITHDRAW", 0x100000, FactionPortraits[faction_id_with], pop_wait);
    } else if (*treaties & DTREATY_PACT) {
        parse_says(2, get_pact(faction_id), -1, -1);
        X_pops_11("WITHDRAWPACT", 0x100000, FactionPortraits[faction_id_with], pop_wait);
    } else if (*treaties & DTREATY_TREATY) {
        X_pops_11("WITHDRAW", 0x100000, FactionPortraits[faction_id_with], pop_wait);
    } else {
        X_pops_11("WITHDRAWTRUCE", 0x100000, FactionPortraits[faction_id_with], pop_wait);
    }
    return 0;
}

/*
Purpose: The faction being talked to (faction_id_with) answers a request for energy. A pact partner
         well ahead in rank, reserves and tech may make a gift (ENERGYFREEBIE); as in the
         original the gift is raised to all of its reserves (the larger of the two is kept).
         With counter proposal 2 or 4 and a tech to sell, it offers energy for the tech
         (ENERGYTECH: answer 2 sells the second tech if there is one, 2 without one and 3 open
         help), at a price set by the tech's value, the turn and the number of factions owning
         DiploTechGetID1 (the tech to get, as in the original), at most half its reserves; the
         Believers never buy, the Hive never takes the second tech and only the University takes
         it for more than the first. Counter proposals 1 and 6, or 2 without a tech, ask for a
         loan (ENERGYLOAN) sized by commerce and paid back with interest. Otherwise it refuses
         (REJENERGY).
Original Offset: 00540630
Return Value: Was energy paid for a tech or lent? true/false (the gift returns false)
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch29.log)
*/
int __cdecl energy_trade(int faction_id, int faction_id_with) {
    *DiploSecondFaction = faction_id_with;
    tech_analysis(faction_id, faction_id_with);
    int proposal = *DiploCounterProposalID;
    BOOL sell_tech = (proposal == 4 || proposal == 2);
    PlayerData *p = &PlayersData[faction_id];
    PlayerData *p_with = &PlayersData[faction_id_with];
    LPCSTR filename_with = Players[faction_id_with].filename;
    char number[80];
    if ((p_with->diplo_treaties[faction_id] & DTREATY_PACT) && proposal != 6
        && (int)p_with->ranking > (int)p->ranking + 1
        && p_with->energy_reserves > p->energy_reserves * 2
        && p_with->tech_ranking > p->tech_ranking
        && p_with->energy_reserves > *TurnCurrentNum * 4
        && p_with->diplo_patience[faction_id] < 2
        && (*DiploTechGiveID1 < 0 || *DiploCounterProposalID == 1)) {
        int gift = p_with->energy_reserves / 8;
        gift = (gift < 1) ? 1 : ((gift > 500) ? 500 : gift);
        gift = (gift + 24) / 25 * 25;
        if (gift < p_with->energy_reserves) {
            gift = p_with->energy_reserves;
        }
        net_energy(faction_id_with, -gift, faction_id, gift, true);
        stringTemp->str[0] = 0;
        p_with->diplo_patience[faction_id] += 2;
        strcat_s(stringTemp->str, sizeof(char1032), "ENERGYFREEBIE");
        _itoa_s((int)random(0, 2), number, 10);
        strcat_s(stringTemp->str, sizeof(char1032), number);
        X_pops_11(stringTemp->str, 0x100000, FactionPortraits[faction_id_with], pop_wait);
        return false;
    }
    BOOL loan_asked = (proposal == 6 || proposal == 1 || (proposal == 2 && *DiploTechGiveID1 < 0));
    if (!loan_asked && sell_tech) {
        parse_says(0, get_title(faction_id), -1, -1);
        parse_says(1, get_name(faction_id), -1, -1);
        if (*DiploTechGiveID1 < 0 || !strcmp(filename_with, "BELIEVE")) {
            X_pops_11("REJSELLNONE", 0x100000, FactionPortraits[faction_id_with], pop_wait);
            return false;
        }
        int scale = (*TurnCurrentNum + 7) / 8 * 32;
        if (great_satan(faction_id, false)) {
            scale = scale * 3 / 4;
        }
        int owners = (int)bit_count(GameTechAchieved[*DiploTechGetID1]) - 1;
        int factor = scale / ((owners < 1) ? 1 : ((owners > 99) ? 99 : owners));
        int price = tech_val(*DiploTechGiveID1, faction_id_with, true) * factor / 32;
        int price_second = (*DiploTechGiveID2 < 0) ? 0
            : tech_val(*DiploTechGiveID2, faction_id_with, true) * factor / 32;
        int half = p_with->energy_reserves / 2;
        if (price < 0 || half < 0) {
            price = 0;
        } else if (price > half) {
            price = half;
        }
        price = price / 25 * 25;
        if (price < 25) {
            price = 25;
        }
        if (price_second < 25) {
            price_second = 25;
        }
        if (!price) { // never, the price is at least 25
            X_pops_11("REJSELLAFFORD", 0x100000, FactionPortraits[faction_id_with], pop_wait);
            return false;
        }
        for (;;) {
            stringTemp->str[0] = 0;
            say_tech(stringTemp->str, *DiploTechGiveID1, true);
            parse_says(0, stringTemp->str, -1, -1);
            if (*DiploTechGiveID2 >= 0) {
                stringTemp->str[0] = 0;
                say_tech(stringTemp->str, *DiploTechGiveID2, true);
                parse_says(1, stringTemp->str, -1, -1);
            }
            parse_num(0, price);
            stringTemp->str[0] = 0;
            strcat_s(stringTemp->str, sizeof(char1032), "ENERGYTECH");
            if (*DiploTechGiveID2 >= 0) {
                strcat_s(stringTemp->str, sizeof(char1032), "2");
            } else {
                _itoa_s((int)random(0, 2), number, 10);
                strcat_s(stringTemp->str, sizeof(char1032), number);
            }
            int answer = X_pops_11(stringTemp->str, 0x100000, FactionPortraits[faction_id_with],
                pop_wait);
            if (!answer) {
                return false;
            }
            if (answer == 2 && *DiploTechGiveID2 >= 0) {
                if (!strcmp(filename_with, "HIVE")
                    || (price < price_second && strcmp(filename_with, "UNIV"))) {
                    X_pops_11("REJSELLSECOND", 0x100000, FactionPortraits[faction_id_with], pop_wait);
                    return false;
                }
                X_pops_11("SELLSECOND", 0x100000, FactionPortraits[faction_id_with], pop_wait);
                net_tech(faction_id_with, *DiploTechGiveID2, faction_id, false);
                break;
            }
            if (answer == 2 || answer == 3) {
                help_topic(14, *DiploTechGiveID1);
                continue;
            }
            net_tech(faction_id_with, *DiploTechGiveID1, faction_id, false);
            *DiploTechGiveID1 = *DiploTechGiveID2;
            break;
        }
        *DiploTechGiveID2 = -1;
        net_energy(faction_id_with, -price, faction_id, price, true);
        *InfoWinUnk = 0;
        Console_update_data(ConsoleVar, NULL, 0);
        return true;
    }
    int loan = 0;
    int payment = 0;
    if (loan_asked) {
        int energy_with = p_with->energy_reserves;
        if (p_with->tech_commerce_bonus < p->tech_commerce_bonus) {
            loan = energy_with / 4;
        } else if (p_with->tech_commerce_bonus == p->tech_commerce_bonus) {
            loan = energy_with / 3;
        } else {
            loan = energy_with / 2;
        }
        if (energy_with < p->energy_reserves && strcmp(filename_with, "MORGAN")) {
            loan = 0;
        }
        if (p->loan_balance[faction_id_with] || p_with->loan_balance[faction_id]) {
            loan = 0;
        }
        uint32_t treaties_with = p_with->diplo_treaties[faction_id];
        if (treaties_with & (DTREATY_VENDETTA | DTREATY_WANT_REVENGE)) {
            loan = 0;
        }
        if (!(treaties_with & DTREATY_TREATY) || loan < 10) {
            loan = 0;
        }
        int interest = (*DiploUnk6 + (p_with->tech_commerce_bonus - p->tech_commerce_bonus) * 2 + 10) * 5;
        if (p->diplo_treaties[faction_id_with] & DTREATY_PACT) {
            interest -= 10;
            if ((int)p_with->ranking > (int)p->ranking) {
                interest -= 10;
            }
        }
        if (interest < 0) {
            interest = 0;
        }
        payment = (10 - *DiploUnk6) * 10;
        if (payment < 20) {
            payment = 20;
        }
        if (loan) {
            int total = (interest + 100) * loan / 100;
            if (payment < 10 || total < 10) {
                payment = 10;
            } else if (payment > total) {
                payment = total;
            }
            int turns = (total + payment - 1) / payment;
            parse_num(0, loan);
            parse_num(1, turns);
            parse_num(2, payment);
            parse_says(0, get_title(faction_id), -1, -1);
            parse_says(1, get_name(faction_id), -1, -1);
            stringTemp->str[0] = 0;
            strcat_s(stringTemp->str, sizeof(char1032), "ENERGYLOAN");
            _itoa_s((turns != 1) + 1, number, 10);
            strcat_s(stringTemp->str, sizeof(char1032), number);
            if (!X_pops_11(stringTemp->str, 0x100000, FactionPortraits[faction_id_with], pop_wait)) {
                return false;
            }
            net_loan(faction_id, faction_id_with, turns * payment, turns);
            net_energy(faction_id_with, -loan, faction_id, loan, true);
            return true;
        }
    }
    stringTemp->str[0] = 0;
    strcat_s(stringTemp->str, sizeof(char1032), "REJENERGY");
    _itoa_s((int)random(0, 2), number, 10);
    strcat_s(stringTemp->str, sizeof(char1032), number);
    X_pops_11(stringTemp->str, 0x100000, FactionPortraits[faction_id_with], pop_wait);
    return false;
}

/*
Purpose: Work out what technologies two factions could trade: the four most valuable techs this
         faction could give the other (by the other's valuation) and the two most valuable it
         could get (by its own valuation, halved when the tech unlocks a secret project the
         other faction is building or, at high difficulty, any unbuilt project). The project
         found is recorded, and a second pick tied to a project is dropped.
Original Offset: 0053E800
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch10.log)
*/
void __cdecl tech_analysis(int faction_id, uint32_t faction_id_other) {
    *DiploTechProjectID = -1;
    *DiploTechGiveID4 = -1;
    *DiploTechGiveID3 = -1;
    *DiploTechGiveID2 = -1;
    *DiploTechGiveID1 = -1;
    *DiploTechGetID2 = -1;
    *DiploTechGetID1 = -1;
    *DiploTechGetValue2 = 0;
    *DiploTechGiveValue2 = 0;
    *DiploTechGetValue1 = 0;
    *DiploTechGiveValue1 = 0;
    *DiploTechGiveValue4 = 0;
    *DiploTechGiveValue3 = 0;
    *DiploTechGetProject2 = -1;
    *DiploTechGetProject1 = -1;
    if (*GameRules & RULES_SCN_NO_TECH_TRADING) {
        return;
    }
    for (int tech_id = 0; tech_id < MaxTechnologyNum; tech_id++) {
        if (!has_tech(tech_id, faction_id_other)) {
            if (!has_tech(tech_id, faction_id)) {
                continue;
            }
            int value = tech_val(tech_id, faction_id_other, false);
            if (value >= *DiploTechGiveValue1) {
                *DiploTechGiveID4 = *DiploTechGiveID3;
                *DiploTechGiveID3 = *DiploTechGiveID2;
                *DiploTechGiveID2 = *DiploTechGiveID1;
                *DiploTechGiveValue4 = *DiploTechGiveValue3;
                *DiploTechGiveValue3 = *DiploTechGiveValue2;
                *DiploTechGiveValue2 = *DiploTechGiveValue1;
                *DiploTechGiveValue1 = value;
                *DiploTechGiveID1 = tech_id;
            } else if (value >= *DiploTechGiveValue2) {
                *DiploTechGiveValue4 = *DiploTechGiveValue3;
                *DiploTechGiveValue3 = *DiploTechGiveValue2;
                *DiploTechGiveID4 = *DiploTechGiveID3;
                *DiploTechGiveID3 = *DiploTechGiveID2;
                *DiploTechGiveID2 = tech_id;
                *DiploTechGiveValue2 = value;
            } else if (value >= *DiploTechGiveValue3) {
                *DiploTechGiveValue4 = *DiploTechGiveValue3;
                *DiploTechGiveID4 = *DiploTechGiveID3;
                *DiploTechGiveID3 = tech_id;
                *DiploTechGiveValue3 = value;
            } else if (value >= *DiploTechGiveValue4) {
                *DiploTechGiveValue4 = value;
                *DiploTechGiveID4 = tech_id;
            }
        } else if (!has_tech(tech_id, faction_id)) {
            int project = -1;
            if (PlayersData[faction_id].diff_level > 2
                || strcmp(Players[faction_id_other].filename, "HIVE") == 0) {
                for (uint32_t sp = 0; sp < MaxSecretProjectNum; sp++) {
                    if ((&SecretProject->human_genome_project)[sp] != -1
                        || Facility[FAC_HUMAN_GENOME_PROJ + sp].preq_tech != tech_id) {
                        continue;
                    }
                    for (int i = 0; i < *BaseCurrentCount; i++) {
                        if (Bases[i].faction_id_current == faction_id_other
                            && Bases[i].queue_production_id[0] == -(int)(FAC_HUMAN_GENOME_PROJ + sp)) {
                            *DiploTechProjectID = sp;
                            project = sp;
                        }
                    }
                    if (((PlayersData[faction_id].diff_level > 4
                        && (!*IsMultiplayerNet || faction_id != (int)*RankingFactionIDUnk2))
                        || (*IsMultiplayerNet && faction_id == (int)*RankingFactionIDUnk1))
                        && *DiploTechProjectID == -1) {
                        *DiploTechProjectID = -2 - sp;
                        project = -2 - sp;
                    }
                }
            }
            int value = tech_val(tech_id, faction_id, false);
            if (project != -1) {
                value = (value + 1) / 2;
            }
            if (value >= *DiploTechGetValue1) {
                *DiploTechGetValue2 = *DiploTechGetValue1;
                *DiploTechGetProject2 = *DiploTechGetProject1;
                *DiploTechGetID2 = *DiploTechGetID1;
                *DiploTechGetID1 = tech_id;
                *DiploTechGetProject1 = project;
                *DiploTechGetValue1 = value;
            } else if (value >= *DiploTechGetValue2) {
                *DiploTechGetValue2 = value;
                *DiploTechGetID2 = tech_id;
                *DiploTechGetProject2 = project;
            }
        }
    }
    if (*DiploTechGetProject2 != -1) {
        *DiploTechGetID2 = -1;
    }
}

/*
Purpose: Exchange world maps between two factions. Each faction gains visibility of every tile
         the other can see, including what the other faction knows about the tile. Bases become
         known to the other side, and the map is redrawn if the player is involved.
Original Offset: 00543A20
Return Value: n/a
Status: Complete
*/
void __cdecl trade_maps(int faction_id, int faction_id_with) {
    *TradeMapsRevealedNew = false;
    uint32_t traded = PlayersData[faction_id].traded_maps | (1 << faction_id_with)
        | PlayersData[faction_id_with].traded_maps;
    PlayersData[faction_id].traded_maps = traded;
    PlayersData[faction_id_with].traded_maps |= (1 << faction_id) | traded;
    uint8_t vis_bit = 1 << faction_id;
    uint8_t vis_bit_with = 1 << faction_id_with;
    for (uint32_t i = 0; i < *MapArea; i++) {
        Map *tile = &(*MapTiles)[i];
        if (tile->visibility & vis_bit) {
            tile->visibility |= vis_bit_with;
            if (faction_id && faction_id_with) {
                tile->bit_visible[faction_id_with - 1] |= tile->bit_visible[faction_id - 1]
                    & tile->bit;
            }
        } else if (tile->visibility & vis_bit_with) {
            *TradeMapsRevealedNew = true;
            tile->visibility |= vis_bit;
            if (faction_id && faction_id_with) {
                tile->bit_visible[faction_id - 1] |= tile->bit_visible[faction_id_with - 1]
                    & tile->bit;
            }
        }
    }
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        if ((Bases[base_id].visibility & vis_bit) || Bases[base_id].faction_id_current == faction_id) {
            spot_base(base_id, faction_id_with);
        } else if ((Bases[base_id].visibility & vis_bit_with)
            || Bases[base_id].faction_id_current == faction_id_with) {
            spot_base(base_id, faction_id);
        }
    }
    if (faction_id == *FactionIDCurrent || faction_id_with == *FactionIDCurrent) {
        *UnkBitfield1 |= 4;
        GraphicWin_redraw(WorldWinVar, NULL);
        draw_map(1);
    }
}

/*
Purpose: Look for a reason faction_id_with gives for not withdrawing: one of its units (info
         warfare units aside) on land in its own territory, as faction_id knows the territory,
         is moving to a base of a third faction that faction_id_with has a vendetta with and
         that is faction_id's pact partner (or treaty partner while DiploUnk1 is positive).
         Only checked when DTREATY_UNK_800000 is set; faction_id needs at least one base.
Original Offset: 00548DD0
Return Value: The third faction, or 0
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
int __cdecl dont_withdrawal(int faction_id, int faction_id_with) {
    *DiploSecondFaction = faction_id_with;
    if (!(PlayersData[faction_id].diplo_treaties[faction_id_with] & DTREATY_UNK_800000)) {
        return 0;
    }
    for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
        Veh *veh = &Vehs[veh_id];
        if (veh->faction_id != faction_id_with
            || VehPrototypes[veh->proto_id].plan == PLAN_INFO_WARFARE) {
            continue;
        }
        int x = veh->x;
        int y = veh->y;
        // the original also takes abs() of the unit's distance to that base on both axes and
        // drops the results
        if (is_ocean(x, y) || base_find(x, y, (uint32_t)faction_id) < 0
            || whose_territory(faction_id, x, y, NULL, false) != faction_id_with
            || veh->order != ORDER_MOVE_TO) {
            continue;
        }
        int base_id = base_at(veh->waypoint_x[0], veh->waypoint_y[0]);
        if (base_id < 0) {
            continue;
        }
        int faction_id_tgt = Bases[base_id].faction_id_current;
        if (faction_id_tgt == faction_id || faction_id_tgt == faction_id_with || !faction_id_tgt
            || !(PlayersData[faction_id_with].diplo_treaties[faction_id_tgt] & DTREATY_VENDETTA)) {
            continue;
        }
        uint32_t treaties = PlayersData[faction_id].diplo_treaties[faction_id_tgt];
        if (treaties & DTREATY_PACT || (treaties & DTREATY_TREATY && *DiploUnk1 > 0)) {
            return faction_id_tgt;
        }
    }
    return 0;
}

/*
Purpose: Withdraw a faction's land units from another faction's territory. Each unit standing
         on the owner's land is moved to an adjacent tile the withdrawing faction may use (own
         territory, or ocean holding one of its own transports) if it carries cargo capacity,
         and otherwise to the faction's nearest base. The original code checks every adjacent
         offset even after a move, so a unit can be moved more than once.
Original Offset: 00548F80
Return Value: Number of unit moves carried out
Status: Complete
*/
int __cdecl do_withdrawal(int faction_id_owner, int faction_id) {
    const uint32_t state_clear = VSTATE_UNK_200 | VSTATE_EXPLORE | VSTATE_UNK_1000000
        | VSTATE_UNK_2000000;
    int moved = 0;
    for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
        Veh *veh = &Vehs[veh_id];
        int proto_id = veh->proto_id;
        if (veh->faction_id != faction_id || VehPrototypes[proto_id].plan == PLAN_INFO_WARFARE) {
            continue;
        }
        int x = veh->x;
        int y = veh->y;
        if (is_ocean(x, y) || map_loc(x, y)->territory != faction_id_owner) {
            continue;
        }
        if (Chassis[VehPrototypes[proto_id].chassis_id].triad == TRIAD_LAND && veh_cargo(veh_id)) {
            for (int i = 1; i <= 8; i++) {
                int x_adj = xrange(x + RadiusOffsetX[i]);
                int y_adj = y + RadiusOffsetY[i];
                if (!on_map(x_adj, y_adj)) {
                    continue;
                }
                Map *tile = map_loc(x_adj, y_adj);
                BOOL can_move;
                if (is_ocean(x_adj, y_adj)) {
                    can_move = stack_check(stack_fix(veh_at(x_adj, y_adj)), 8, faction_id, -1, -1)
                        > 0;
                } else {
                    can_move = tile->territory == faction_id;
                }
                if (!can_move) {
                    continue;
                }
                uint32_t owner = tile->val2 & 0xF;
                if (!(tile->bit & (BIT_BASE_IN_TILE | BIT_VEH_IN_TILE)) || owner > 7
                    || owner == (uint32_t)faction_id) {
                    veh_drop(veh_lift(veh_id), x_adj, y_adj);
                    veh->visibility = 0;
                    veh->order = ORDER_NONE;
                    veh->state &= ~state_clear;
                    draw_tiles(x, y, 2);
                    moved++;
                }
            }
        }
        int base_id = base_find(x, y, (uint32_t)faction_id);
        if (base_id >= 0) {
            veh_drop(veh_lift(veh_id), Bases[base_id].x, Bases[base_id].y);
            veh->visibility = 0;
            veh->order = ORDER_NONE;
            veh->state &= ~state_clear;
            draw_tile_fixup(x, y, has_abil(proto_id, ABL_DEEP_RADAR), 2);
            moved++;
        }
    }
    if (moved) {
        set_agenda(faction_id, faction_id_owner, DAGENDA_UNK_2, true);
        set_treaty(faction_id, faction_id_owner, DTREATY_UNK_100000, true);
    }
    return moved;
}

/*
Purpose: Clear the specified treaty flags between two factions (both directions). Dropping a
         pact, treaty or truce also records the break; dropping a vendetta also clears the
         related ceasefire flags; dropping a revenge wish clears the matching agenda bits.
Original Offset: 0055B760
Return Value: n/a
Status: Complete
*/
void __cdecl treaty_off(int faction_id, int faction_id_with, uint32_t treaty) {
    if (treaty & (DTREATY_PACT | DTREATY_TREATY | DTREATY_TRUCE)) {
        treaty |= (DTREATY_UNK_1000000 | DTREATY_UNK_100000);
    }
    if (treaty & DTREATY_VENDETTA) {
        treaty |= (DTREATY_UNK_8000 | DTREATY_UNK_40);
    }
    if (treaty & DTREATY_WANT_REVENGE) {
        PlayersData[faction_id].diplo_agenda[faction_id_with] &= ~(DAGENDA_UNK_800 | DAGENDA_UNK_400);
        PlayersData[faction_id_with].diplo_agenda[faction_id] &= ~(DAGENDA_UNK_800 | DAGENDA_UNK_400);
    }
    PlayersData[faction_id].diplo_treaties[faction_id_with] &= ~treaty;
    PlayersData[faction_id_with].diplo_treaties[faction_id] &= ~treaty;
}

/*
Purpose: Clear the specified agenda flags between two factions (both directions).
Original Offset: 0055B820
Return Value: n/a
Status: Complete
*/
void __cdecl agenda_off(int faction_id, int faction_id_with, uint32_t agenda) {
    PlayersData[faction_id].diplo_agenda[faction_id_with] &= ~agenda;
    PlayersData[faction_id_with].diplo_agenda[faction_id] &= ~agenda;
}

/*
Purpose: Set the specified treaty flags between two factions (both directions). Making peace
         (pact, treaty or truce) clears the war related flags; a pact implies a treaty; a
         vendetta clears the peace related flags, resets the combat counters if the factions
         were at peace, and plays the war ambience if the player is involved.
Original Offset: 0055B870
Return Value: n/a
Status: Complete
*/
void __cdecl treaty_on(int faction_id, int faction_id_with, uint32_t treaty) {
    BOOL is_player = (faction_id == *FactionIDCurrent || faction_id_with == *FactionIDCurrent);
    if (treaty & (DTREATY_PACT | DTREATY_TREATY | DTREATY_TRUCE)) {
        if (is_player && PlayersData[faction_id].diplo_treaties[faction_id_with] & DTREATY_VENDETTA) {
            *AmbienceFlags &= ~1;
        }
        const uint32_t war_flags = DTREATY_VENDETTA | DTREATY_UNK_40 | DTREATY_UNK_8000
            | DTREATY_UNK_10000 | DTREATY_UNK_100000;
        PlayersData[faction_id].diplo_treaties[faction_id_with] &= ~war_flags;
        PlayersData[faction_id_with].diplo_treaties[faction_id] &= ~war_flags;
    }
    if (treaty & DTREATY_PACT) {
        treaty |= DTREATY_TREATY;
    }
    if (treaty & DTREATY_VENDETTA) {
        if (PlayersData[faction_id].diplo_treaties[faction_id_with]
            & (DTREATY_PACT | DTREATY_TREATY | DTREATY_TRUCE)) {
            PlayersData[faction_id_with].diplo_unk4[faction_id] = 0;
            PlayersData[faction_id].diplo_unk4[faction_id_with] = 0;
        }
        treaty |= (DTREATY_UNK_80000000 | DTREATY_UNK_100 | DTREATY_COMMLINK);
        const uint32_t peace_flags = DTREATY_PACT | DTREATY_TREATY | DTREATY_TRUCE | DTREATY_UNK_80
            | DTREATY_SHALL_BETRAY | DTREATY_UNK_800 | DTREATY_UNK_10000 | DTREATY_UNK_100000
            | DTREATY_UNK_1000000 | DTREATY_UNK_4000000;
        PlayersData[faction_id].diplo_treaties[faction_id_with] &= ~peace_flags;
        PlayersData[faction_id_with].diplo_treaties[faction_id] &= ~peace_flags;
        if (is_player) {
            *AmbienceFlags |= 1;
            ambience(204);
        }
    }
    if (treaty & (DTREATY_PACT | DTREATY_TREATY | DTREATY_TRUCE | DTREATY_VENDETTA)) {
        PlayersData[faction_id].diplo_treaties[faction_id_with] &= ~DTREATY_UNK_800000;
        PlayersData[faction_id_with].diplo_treaties[faction_id] &= ~DTREATY_UNK_800000;
    }
    PlayersData[faction_id].diplo_treaties[faction_id_with] |= treaty;
    PlayersData[faction_id_with].diplo_treaties[faction_id] |= treaty;
    if (is_player && (treaty & (DTREATY_PACT | DTREATY_TREATY | DTREATY_TRUCE | DTREATY_COMMLINK
        | DTREATY_VENDETTA)) && *MultiWinFlag) {
        GraphicWin_redraw(MultiWinVar, NULL);
    }
}

/*
Purpose: Set the specified agenda flags between two factions (both directions). Setting flag
         0x20 clears flag 0x40 first.
Original Offset: 0055BA80
Return Value: n/a
Status: Complete
*/
void __cdecl agenda_on(int faction_id, int faction_id_with, uint32_t agenda) {
    if (agenda & DAGENDA_UNK_20) {
        PlayersData[faction_id].diplo_agenda[faction_id_with] &= ~DAGENDA_UNK_40;
        PlayersData[faction_id_with].diplo_agenda[faction_id] &= ~DAGENDA_UNK_40;
    }
    PlayersData[faction_id].diplo_agenda[faction_id_with] |= agenda;
    PlayersData[faction_id_with].diplo_agenda[faction_id] |= agenda;
}

/*
Purpose: Set or unset the diplomatic treaty for the specified faction with another faction.
Original Offset: 0055BB30
Return Value: n/a
Status: Complete
*/
void __cdecl set_treaty(uint32_t faction_id, uint32_t faction_id_with, uint32_t treaty, BOOL set) {
    if (set) {
        PlayersData[faction_id].diplo_treaties[faction_id_with] |= treaty;
        if (treaty & DTREATY_UNK_40) {
            PlayersData[faction_id].diplo_merc[faction_id_with] = 50;
        }
    } else {
        PlayersData[faction_id].diplo_treaties[faction_id_with] &= ~treaty;
    }
}

/*
Purpose: Set or unset the diplomatic agenda for the specified faction with another faction.
Original Offset: 0055BBA0
Return Value: n/a
Status: Complete
*/
void __cdecl set_agenda(uint32_t faction_id, uint32_t faction_id_with, uint32_t agenda, BOOL set) {
    if (set) {
        PlayersData[faction_id].diplo_agenda[faction_id_with] |= agenda;
    } else {
        PlayersData[faction_id].diplo_agenda[faction_id_with] &= ~agenda;
    }
}

/*
Purpose: Check whether the primary faction has at least one of the specified agendas (bitfield)
         with the secondary faction.
Original Offset: 005591E0
Return Value: Agenda status between the two factions, generally treated as a boolean
Status: Complete
*/
uint32_t __cdecl has_agenda(uint32_t faction_id, uint32_t faction_id_with, uint32_t agenda) {
    return PlayersData[faction_id].diplo_agenda[faction_id_with] & agenda;
}

/*
Purpose: Check whether the faction at the controls has full intelligence on the specified faction:
         an infiltrator, the Empath Guild, or the Planetary Governorship (which sees every
         non-alien faction).
Original Offset: 0055BC00
Return Value: Is the faction being spied on? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch3.log)
*/
BOOL __cdecl spying(int faction_id) {
    int faction_id_current = *FactionIDCurrent;
    if (PlayersData[faction_id_current].diplo_treaties[faction_id] & DTREATY_HAVE_INFILTRATOR) {
        return true;
    }
    int base_id_empath = SecretProject->empath_guild;
    if (base_id_empath >= 0 && Bases[base_id_empath].faction_id_current == faction_id_current) {
        return true;
    }
    if (faction_id_current == *GovernorFactionID
        && !(Players[faction_id].rule_flags & RFLAG_ALIEN)) {
        return true;
    }
    return false;
}

/*
Purpose: Determine if the specified faction want to attack the target faction.
Original Offset: 0055BC80
Return Value: Does the faction want to attack target? true/false
Status: Complete. Deliberately differs from the original (Kiousu's choice, 2026-10-18): each
        faction's most bases in one region starts from zero instead of uninitialized stack memory;
        see "wants_to_attack, 2026-10-18" in docs/regression-testing.md.
*/
BOOL __cdecl wants_to_attack(uint32_t faction_id, uint32_t faction_id_tgt, int faction_id_unk) {
    uint32_t peace_faction_id = 0;
    BOOL unk_tgl = false;
    if (Players[faction_id].rule_flags & RFLAG_ALIEN
        && Players[faction_id_tgt].rule_flags & RFLAG_ALIEN) {
        return true;
    }
    if (has_treaty(faction_id, faction_id_tgt,
        DTREATY_WANT_REVENGE | DTREATY_UNK_40 | DTREATY_ATROCITY_VICTIM)) {
        return true;
    }
    // target has committed major atrocities while this faction has not
    if (PlayersData[faction_id_tgt].major_atrocities && !PlayersData[faction_id].major_atrocities) {
        return true;
    }
    if (has_treaty(faction_id, faction_id_tgt, DTREATY_UNK_4000000)) {
        return false;
    }
    if ((int)PlayersData[faction_id].current_num_bases <= 1) {
        return false;
    }
    if (!is_human(faction_id_tgt) && PlayersData[faction_id].flags & PFLAG_TEAM_UP_VS_HUMAN) {
        return false;
    }
    int want_to_attack = 0; // signed: the original's comparisons on it are signed
    for (uint32_t i = 1; i < MaxPlayerNum; i++) {
        if (i != faction_id && i != faction_id_tgt) {
            // both flags are required (surrendered to i and pact with i)
            if (has_treaty(faction_id, i, DTREATY_HAVE_SURRENDERED)
                && has_treaty(faction_id, i, DTREATY_PACT)) {
                peace_faction_id = i;
            }
            if (has_treaty(faction_id, i, DTREATY_VENDETTA)
                && !has_treaty(faction_id_tgt, i, DTREATY_PACT)) {
                want_to_attack++;
                if (PlayersData[i].mil_strength_1 
                    > ((PlayersData[faction_id_tgt].mil_strength_1 * 3) / 2)) {
                    want_to_attack++;
                }
            }
            if (great_beelzebub(i, false)
                && (*TurnCurrentNum >= 100 || *GameRules & RULES_INTENSE_RIVALRY)) {
                if (has_treaty(faction_id_tgt, i, DTREATY_VENDETTA)) {
                    want_to_attack++;
                }
                if (has_treaty(faction_id_tgt, i, DTREATY_COMMLINK) 
                    && has_treaty(faction_id, i, DTREATY_COMMLINK)) {
                    want_to_attack++;
                }
            }
            if (has_treaty(faction_id, i, DTREATY_PACT) && is_human(i)) {
                if (has_treaty(faction_id_tgt, i, DTREATY_PACT)) {
                    want_to_attack += 2;
                }
                BOOL has_surrender = has_treaty(faction_id, i, DTREATY_HAVE_SURRENDERED);
                if (has_surrender && has_treaty(i, faction_id_tgt, DTREATY_PACT | DTREATY_TREATY)) {
                    return false;
                }
                if (has_treaty(faction_id_tgt, i, DTREATY_VENDETTA)) {
                    unk_tgl = true;
                    want_to_attack -= (has_surrender ? 4 : 2);
                }
            }
        }
    }
    if (peace_faction_id) {
        if (has_treaty(faction_id_tgt, peace_faction_id, DTREATY_VENDETTA)) {
            return true;
        }
        if (has_treaty(faction_id_tgt, peace_faction_id, DTREATY_PACT | DTREATY_TREATY)) {
            return false;
        }
    }
    if (PlayersData[faction_id].ai_fight < 0 && !unk_tgl && FactionRankings[7] != faction_id_tgt) {
        return false;
    }
    // Each faction's most bases in any one region, which decides the regions counted as its core
    // below. The original never initializes this array (7 slots at [ebp-0x130], faction 1 first),
    // so each slot starts with whatever the stack holds and a leftover above the real maximum
    // (usually a stack or return address) leaves only the HQ regions counted. Starting from zero
    // is what the code evidently means to do; Kiousu chose that over the leftovers (2026-10-18).
    int region_top_base_count[8] = { 0 };
    for (int region = 1; region < MaxRegionLandNum; region++) {
        for (uint32_t f = 1; f < MaxPlayerNum; f++) {
            int total_bases = PlayersData[f].region_total_bases[region];
            if (total_bases > region_top_base_count[f]) {
                region_top_base_count[f] = total_bases;
            }
        }
    }
    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
        region_top_base_count[f] -= (region_top_base_count[f] / 4);
    }
    int region_target_hq = -1;
    int region_hq = -1;
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (has_fac_built(FAC_HEADQUARTERS, i)) {
            uint32_t base_faction = Bases[i].faction_id_current;
            if (base_faction == faction_id) {
                region_hq = region_at(Bases[i].x, Bases[i].y);
            } else if (base_faction == faction_id_tgt) {
                region_target_hq = region_at(Bases[i].x, Bases[i].y);
            }
        }
    }
    int factor_force_rating = 0; // signed: the original's final division/compare are signed
    int factor_count = 0;
    int factor_unk = 1;
    for (int region = 1; region < MaxRegionLandNum; region++) {
        if (!bad_reg(region)) {
            uint32_t force_rating = PlayersData[faction_id].region_force_rating[region];
            if (force_rating) {
                uint32_t total_cmbt_vehs
                    = PlayersData[faction_id_tgt].region_total_combat_vehs[region];
                uint32_t total_bases_tgt = PlayersData[faction_id_tgt].region_total_bases[region];
                if (total_cmbt_vehs || total_bases_tgt) {
                    if ((int)PlayersData[faction_id].region_total_bases[region]
                        >= ((region_top_base_count[faction_id] * 3) / 4) || region == region_hq) {
                        uint32_t compare = force_rating +
                            PlayersData[faction_id].region_total_combat_vehs[region] +
                            (faction_id_unk > 0 
                                ? PlayersData[faction_id_unk].region_force_rating[region] / 4 : 0);
                        if (PlayersData[faction_id_tgt].region_force_rating[region] > compare) {
                            return false;
                        }
                    }
                    if (total_bases_tgt) {
                        factor_force_rating += force_rating + (faction_id_unk > 0
                            ? PlayersData[faction_id_unk].region_force_rating[region] / 2 : 0);
                    }
                    if (((int)total_bases_tgt >= ((region_top_base_count[faction_id_tgt] * 3) / 4)
                        || region == region_target_hq) && force_rating > total_cmbt_vehs) {
                        factor_force_rating += force_rating + (faction_id_unk > 0
                            ? PlayersData[faction_id_unk].region_force_rating[region] / 2 : 0);
                    }
                    factor_unk += total_cmbt_vehs
                        + (PlayersData[faction_id].region_total_bases[region]
                        ? PlayersData[faction_id_tgt].region_force_rating[region] / 2 : 0);
                    if (PlayersData[faction_id].region_total_bases[region]) {
                        factor_count++;
                    }
                }
            }
        }
    }
    want_to_attack -= PlayersData[faction_id].ai_fight * 2;
    int tech_comm_bonus = PlayersData[faction_id].tech_commerce_bonus;
    int tech_comm_bonus_target = PlayersData[faction_id_tgt].tech_commerce_bonus;
    if (tech_comm_bonus > ((tech_comm_bonus_target * 3) / 2)) {
        want_to_attack++;
    }
    if (tech_comm_bonus < ((tech_comm_bonus_target * 2) / 3)) {
        want_to_attack--;
    }
    int best_armor_target = PlayersData[faction_id_tgt].best_armor_value;
    int best_weapon = PlayersData[faction_id].best_weapon_value;
    if (best_weapon > (best_armor_target * 2)) {
        want_to_attack--;
    }
    if (best_weapon <= best_armor_target) {
        want_to_attack++;
    }
    if (!has_treaty(faction_id, faction_id_tgt, DTREATY_VENDETTA)) {
        want_to_attack++;
    }
    if (has_treaty(faction_id, faction_id_tgt, DTREATY_PACT)) {
        want_to_attack++;
    }
    if (faction_id_unk > 0 && !great_satan(faction_id_unk, false)) {
        want_to_attack--;
    }
    if (has_agenda(faction_id, faction_id_tgt, DAGENDA_UNK_200)
        && !(*GameRules & RULES_INTENSE_RIVALRY)) {
        want_to_attack--;
    }
    want_to_attack -= range((PlayersData[faction_id_tgt].integrity_blemishes 
        - PlayersData[faction_id].integrity_blemishes + 2) / 3, 0, 2);
    int morale_factor = range(PlayersData[faction_id].soc_effect_pending.morale, -4, 4) 
        + Players[faction_id].rule_morale + 16;
    int morale_factor_target 
        = range(PlayersData[faction_id_tgt].soc_effect_pending.morale, -4, 4)
        + Players[faction_id_tgt].rule_morale + 16;
    if ((factor_count || want_to_attack > 0
        || has_treaty(faction_id, faction_id_tgt, DTREATY_UNK_20000000)) &&
        ((morale_factor * factor_force_rating * 6) / (morale_factor_target * factor_unk)) 
        < (want_to_attack + 6)) {
        return false;
    }
    return true;
}

/*
Purpose: A human faction (faction_id) meets another faction. Unless they already have a commlink,
         spoke within the last 16 turns and neither wants to talk, the diplomacy dialog opens. In
         a network game the player at the controls instead marks an AI faction as wanting to talk
         (flag 2 and a first contact truce, flag 0x10000, if there was no commlink), and two humans
         get that truce and the NETMET notice. In a hotseat/PBEM game a human met by another
         player gets the truce, and an AI met by a player not at the controls queues its request
         (and the truce).
Original Offset: 0055C430
Return Value: n/a
Status: Complete
*/
void __cdecl comm_check(int faction_id, int faction_id_with) {
    uint32_t human = FactionsStatus[0];
    if (!(human & (1 << faction_id))) {
        return;
    }
    uint32_t treaties = PlayersData[faction_id].diplo_treaties[faction_id_with];
    uint32_t commlink = treaties & DTREATY_COMMLINK;
    if (commlink && !(treaties & DTREATY_WANT_TO_TALK)
        && PlayersData[faction_id].diplo_spoke[faction_id_with] >= 0
        && *TurnCurrentNum - PlayersData[faction_id].diplo_spoke[faction_id_with] < 16) {
        return;
    }
    const uint32_t truce_new = DTREATY_TRUCE | DTREATY_UNK_10000; // the truce of a first contact
    if (*IsMultiplayerNet) {
        if (faction_id == *FactionIDCurrent && !(human & (1 << faction_id_with))) {
            DiploCommRequest[faction_id_with] |= 1;
            if (!commlink) {
                DiploCommRequest[faction_id_with] |= 2;
                net_treaty_on(faction_id, faction_id_with, truce_new, false);
            }
            return;
        }
        if ((human & (1 << faction_id_with)) && !commlink) {
            treaty_on(faction_id, faction_id_with, truce_new);
        }
        if (faction_id == *FactionIDCurrent
            && !(PlayersData[faction_id].diplo_treaties[faction_id_with] & DTREATY_UNK_8000000)) {
            parse_leader(0, faction_id_with);
            NetMsg_pop(NetMsgVar, 0, "NETMET", 5000, 0, NULL);
        }
        if (faction_id_with == *FactionIDCurrent
            && !(PlayersData[faction_id].diplo_treaties[faction_id_with] & DTREATY_UNK_8000000)) {
            parse_leader(0, faction_id);
            NetMsg_pop(NetMsgVar, 0, "NETMET", 5000, 0, NULL);
        }
        return;
    }
    if (*IsMultiplayerPBEM) {
        if (human & (1 << faction_id_with)) {
            if (!commlink) {
                treaty_on(faction_id, faction_id_with, truce_new);
            }
            return;
        }
        if (faction_id != *FactionIDCurrent) {
            DiploCommRequestPBEM[faction_id * 8 + faction_id_with] |= 1;
            if (!commlink) {
                DiploCommRequestPBEM[faction_id * 8 + faction_id_with] |= 2;
                treaty_on(faction_id, faction_id_with, truce_new);
            }
            return;
        }
    }
    communicate(faction_id, faction_id_with, 0);
}

/*
Purpose: Notice (PACTATTACKS) that a pact partner of the player attacks another faction.
Original Offset: 0055C730
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl pact_attacks(int faction_id, int faction_id_tgt) {
    parse_leader(0, faction_id);
    parse_says(3, get_he_she(faction_id, 0), -1, -1);
    parse_says(4, get_noun(faction_id_tgt), -1, -1);
    NetMsg_pop(NetMsgVar, 0, "PACTATTACKS", 5000, 0, NULL);
}

/*
Purpose: Two factions go to war. The player at the controls hears of it through a pact partner
         (PACTATTACKS, inlined) or, with full intelligence on either side or omniscient view,
         ENEMYWAR. The vendetta is set and the maps exchanged (unless flag 0x40000000). A revenge
         wish on either side (not after an atrocity) is usually settled by the war at difficulty
         above Specialist: rand() % difficulty != 0. For the first faction the wish becomes flag
         0x40 for 50 turns; for the second the flag is set through set_treaty().
Original Offset: 0055C840
Return Value: n/a (the original leaves a meaningless value in eax; its one caller ignores it)
Status: Complete
*/
void __cdecl enemies_war(int faction_id, int faction_id_with) {
    if (PlayersData[faction_id].diplo_treaties[faction_id_with] & DTREATY_VENDETTA) {
        return;
    }
    int faction_id_current = *FactionIDCurrent;
    if (PlayersData[faction_id_current].diplo_treaties[faction_id] & DTREATY_PACT) {
        pact_attacks(faction_id, faction_id_with);
    } else if (PlayersData[faction_id_current].diplo_treaties[faction_id_with] & DTREATY_PACT) {
        pact_attacks(faction_id_with, faction_id);
    } else if (spying(faction_id) || spying(faction_id_with)
        || *GameState & STATE_OMNISCIENT_VIEW) { // spying() inlined in the original
        parse_says(0, get_noun(faction_id), -1, -1);
        parse_says(1, get_noun(faction_id_with), -1, -1);
        NetMsg_pop(NetMsgVar, 0, "ENEMYWAR", 5000, 0, NULL);
    }
    treaty_on(faction_id, faction_id_with, DTREATY_VENDETTA);
    if (!(PlayersData[faction_id].diplo_treaties[faction_id_with] & DTREATY_UNK_40000000)) {
        trade_maps(faction_id, faction_id_with);
    }
    const int agenda_revenge = DAGENDA_UNK_400 | DAGENDA_UNK_800;
    uint32_t treaties = PlayersData[faction_id].diplo_treaties[faction_id_with];
    int diff_level = *DiffLevelCurrent;
    if ((treaties & DTREATY_WANT_REVENGE) && !(treaties & DTREATY_ATROCITY_VICTIM)
        && diff_level - 1 > 0 && _rand() % diff_level) {
        PlayersData[faction_id].diplo_agenda[faction_id_with] &= ~agenda_revenge;
        PlayersData[faction_id_with].diplo_agenda[faction_id] &= ~agenda_revenge;
        PlayersData[faction_id].diplo_treaties[faction_id_with] &= ~DTREATY_WANT_REVENGE;
        PlayersData[faction_id_with].diplo_treaties[faction_id] &= ~DTREATY_WANT_REVENGE;
        PlayersData[faction_id].diplo_treaties[faction_id_with] |= DTREATY_UNK_40;
        PlayersData[faction_id].diplo_merc[faction_id_with] = 50;
    }
    treaties = PlayersData[faction_id_with].diplo_treaties[faction_id];
    diff_level = *DiffLevelCurrent;
    if ((treaties & DTREATY_WANT_REVENGE) && !(treaties & DTREATY_ATROCITY_VICTIM)
        && diff_level - 1 > 0 && _rand() % diff_level) {
        PlayersData[faction_id_with].diplo_agenda[faction_id] &= ~agenda_revenge;
        PlayersData[faction_id].diplo_agenda[faction_id_with] &= ~agenda_revenge;
        PlayersData[faction_id_with].diplo_treaties[faction_id] &= ~DTREATY_WANT_REVENGE;
        PlayersData[faction_id].diplo_treaties[faction_id_with] &= ~DTREATY_WANT_REVENGE;
        set_treaty(faction_id_with, faction_id, DTREATY_UNK_40, true);
    }
}

/*
Purpose: Notice (PACTUNPACT) that a pact partner of the player ends its pact with another
         faction.
Original Offset: 0055CCA0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl pact_unpact(int faction_id, int faction_id_with) {
    parse_leader(0, faction_id);
    parse_says(3, get_he_she(faction_id, 0), -1, -1);
    parse_says(4, get_his_her(faction_id, 0), -1, -1);
    parse_says(5, get_pact_hood(faction_id, faction_id_with), -1, -1);
    parse_leader(6, faction_id_with);
    NetMsg_pop(NetMsgVar, 0, "PACTUNPACT", 5000, 0, NULL);
}

/*
Purpose: End the pact between two factions. A pact partner of the player tells about it through
         pact_unpact(); a faction the player is spying on (or omniscient view) shows
         ENEMYPACTENDS. Then the pact flags are cleared on both sides.
Original Offset: 0055CE20
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl enemies_unpact(int faction_id, int faction_id_with) {
    if (!(PlayersData[faction_id].diplo_treaties[faction_id_with] & DTREATY_PACT)) {
        return;
    }
    int faction_id_current = *FactionIDCurrent;
    if (PlayersData[faction_id].diplo_treaties[faction_id_current] & DTREATY_PACT) {
        pact_unpact(faction_id, faction_id_with);
    } else if (PlayersData[faction_id_with].diplo_treaties[faction_id_current] & DTREATY_PACT) {
        pact_unpact(faction_id_with, faction_id);
    }
    if (spying(faction_id) || spying(faction_id_with) || *GameState & STATE_OMNISCIENT_VIEW) {
        parse_says(0, get_noun(faction_id), -1, -1);
        parse_says(1, get_noun(faction_id_with), -1, -1);
        parse_says(2, get_pact_hood(faction_id, faction_id_with), -1, -1);
        NetMsg_pop(NetMsgVar, 0, "ENEMYPACTENDS", 5000, 0, NULL);
    }
    const uint32_t pact_flags = DTREATY_PACT | DTREATY_UNK_100000 | DTREATY_UNK_1000000;
    PlayersData[faction_id].diplo_treaties[faction_id_with] &= ~pact_flags;
    PlayersData[faction_id_with].diplo_treaties[faction_id] &= ~pact_flags;
}

/*
Purpose: Notice (PACTTRUCE) that a pact partner of the player agrees a truce with another
         faction.
Original Offset: 0055D040
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl pact_truce(int faction_id, int faction_id_with) {
    parse_leader(0, faction_id);
    parse_says(3, get_he_she(faction_id, 0), -1, -1);
    parse_leader(4, faction_id_with);
    NetMsg_pop(NetMsgVar, 0, "PACTTRUCE", 5000, 0, NULL);
}

/*
Purpose: Notice (PACTTREATY) that a pact partner of the player signs a treaty with another
         faction.
Original Offset: 0055D190
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl pact_treaty(int faction_id, int faction_id_with) {
    parse_leader(0, faction_id);
    parse_says(3, get_he_she(faction_id, 0), -1, -1);
    parse_says(4, get_noun(faction_id_with), -1, -1);
    NetMsg_pop(NetMsgVar, 0, "PACTTREATY", 5000, 0, NULL);
}

/*
Purpose: The faction is asked by its ally (faction_id_with) to join a war against a human
         faction (faction_id_tgt). If the ally is at war with the target, the faction declares
         war too (the target is told with TEAM if at the controls) unless the target is no
         threat and the faction holds no grudge, in which case it only wants to talk. Otherwise
         it wants to talk if neither holds a grudge and the target is no threat, or it marks the
         target for betrayal.
Original Offset: 0055D2A0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl enemies_team_up(int faction_id, int faction_id_with, int faction_id_tgt) {
    if (PlayersData[faction_id].diplo_treaties[faction_id_tgt] & DTREATY_VENDETTA
        || !is_human(faction_id_tgt)) {
        return;
    }
    if (PlayersData[faction_id_with].diplo_treaties[faction_id_tgt] & DTREATY_VENDETTA) {
        if (!great_satan(faction_id_tgt, false)
            && !(PlayersData[faction_id].diplo_treaties[faction_id_tgt] & DTREATY_WANT_REVENGE)) {
            treaty_on(faction_id, faction_id_tgt, DTREATY_WANT_TO_TALK);
            return;
        }
        if (faction_id_tgt == *FactionIDCurrent) {
            parse_says(0, get_noun(faction_id), -1, -1);
            NetMsg_pop(NetMsgVar, 0, "TEAM", 5000, 0, NULL);
        }
        PlayersData[faction_id_tgt].diplo_spoke[faction_id_with] = *TurnCurrentNum;
        PlayersData[faction_id_tgt].diplo_spoke[faction_id] = *TurnCurrentNum;
        treaty_on(faction_id, faction_id_tgt, DTREATY_VENDETTA);
        return;
    }
    if (!great_satan(faction_id_tgt, false)
        && !(PlayersData[faction_id].diplo_treaties[faction_id_tgt] & DTREATY_WANT_REVENGE)
        && !(PlayersData[faction_id_with].diplo_treaties[faction_id_tgt] & DTREATY_WANT_REVENGE)) {
        treaty_on(faction_id, faction_id_tgt, DTREATY_WANT_TO_TALK);
        return;
    }
    PlayersData[faction_id].diplo_treaties[faction_id_tgt] |= DTREATY_SHALL_BETRAY;
}

/*
Purpose: Let two pact factions swap technologies: each side rolls for the tech it lacks and the other
         has (weighted by tech_val(), skipping techs whose source is a pact partner or a treaty
         partner the holder is not annoyed enough with) and both get their pick. If only one
         side has a pick, the exchange still happens one way when the top-ranked faction is a
         threat to everyone and the receiver is not ahead of the technology leader.
Original Offset: 0055D430
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch10.log)
*/
void __cdecl enemies_trade_tech(int faction_id, int faction_id_2) {
    if (*GameRules & RULES_SCN_NO_TECH_TRADING) {
        return;
    }
    int tech_for_1 = -1, tech_for_2 = -1;
    int roll_1 = -9999, roll_2 = -9999;
    BOOL pact = (PlayersData[faction_id].diplo_treaties[faction_id_2] & DTREATY_PACT) != 0;
    int friction_needed = pact ? 6 : 16;
    for (int tech_id = 0; tech_id < MaxTechnologyNum; tech_id++) {
        if (!has_tech(tech_id, faction_id) && has_tech(tech_id, faction_id_2)) {
            int source = PlayersData[faction_id_2].tech_trade_source[tech_id];
            if ((source == 0 || !(PlayersData[faction_id_2].diplo_treaties[source] & DTREATY_PACT))
                && (!(PlayersData[faction_id_2].diplo_treaties[source] & DTREATY_TREATY)
                    || (int)PlayersData[faction_id_2].diplo_friction[source] >= friction_needed)) {
                int value = tech_val(tech_id, faction_id, false);
                int roll = (value == 1 || value - 1 < 0) ? 0 : _rand() % value;
                if (roll > roll_1) {
                    tech_for_1 = tech_id;
                    roll_1 = roll;
                }
            }
        } else if (!has_tech(tech_id, faction_id_2) && has_tech(tech_id, faction_id)) {
            int source = PlayersData[faction_id].tech_trade_source[tech_id];
            // the friction test looks up the second faction's source (as the original does)
            int source_2 = PlayersData[faction_id_2].tech_trade_source[tech_id];
            if ((source == 0 || !(PlayersData[faction_id].diplo_treaties[source] & DTREATY_PACT))
                && (!(PlayersData[faction_id].diplo_treaties[source] & DTREATY_TREATY)
                    || (int)PlayersData[faction_id_2].diplo_friction[source_2] >= friction_needed)) {
                int value = tech_val(tech_id, faction_id_2, false);
                int roll = (value == 1 || value - 1 < 0) ? 0 : _rand() % value;
                if (roll > roll_2) {
                    roll_2 = roll;
                    tech_for_2 = tech_id;
                }
            }
        }
    }
    if (tech_for_1 >= 0 && tech_for_2 >= 0) {
        tech_achieved(faction_id, tech_for_1, faction_id_2, 0);
        tech_achieved(faction_id_2, tech_for_2, faction_id, 0);
        return;
    }
    if (pact && great_satan(FactionRankings[7], false)) {
        int tech_ranking_leader = PlayersData[*RankingFactionIDUnk1].tech_ranking;
        if (tech_for_1 >= 0 && PlayersData[faction_id_2].tech_ranking <= tech_ranking_leader) {
            tech_achieved(faction_id, tech_for_1, faction_id_2, 0);
        }
        if (tech_for_2 >= 0 && PlayersData[faction_id].tech_ranking <= tech_ranking_leader) {
            tech_achieved(faction_id_2, tech_for_2, faction_id, 0);
        }
    }
}

/*
Purpose: Count another faction's units inside this faction's territory (combat and terraforming
         units, and convoying crawlers) on land, marking each with VSTATE_UNK_400. Nothing counts
         under a pact or vendetta, under the unk_200 treaty flag unless bit 2 of flags is set,
         or in regions where a faction both are at war with has bases (unless this faction wants
         revenge or the intruder's integrity is poor). Units on improved tiles count double. With
         bit 1 of flags, intruders near the base (or any, given bad relations or high friction)
         are flagged: the first time they get VSTATE_UNK_800, afterwards they count once more
         (five times under a treaty) and the number of repeat offenders goes to count_out.
Original Offset: 0055EB80
Return Value: Intrusion count; base_id_out gets the last base whose territory was entered
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch8.log)
*/
int __cdecl territory(uint32_t faction_id, uint32_t faction_id_tgt, uint8_t flags, int *base_id_out,
                      int *count_out) {
    int result = 0;
    if (base_id_out) {
        *base_id_out = -1;
    }
    if (count_out) {
        *count_out = 0;
    }
    uint32_t treaties = PlayersData[faction_id].diplo_treaties[faction_id_tgt];
    if (treaties & (DTREATY_PACT | DTREATY_VENDETTA)) {
        return 0;
    }
    if (!(flags & 2) && (treaties & DTREATY_UNK_200)) {
        return 0;
    }
    BOOL region_shared[64];
    for (int region = 0; region < 64; region++) {
        region_shared[region] = false;
        if ((treaties & DTREATY_WANT_REVENGE) || PlayersData[faction_id_tgt].integrity_blemishes >= 5) {
            continue;
        }
        for (uint32_t i = 1; i < MaxPlayerNum; i++) {
            if (i != faction_id && i != faction_id_tgt
                && (PlayersData[faction_id].diplo_treaties[i] & DTREATY_VENDETTA)
                && (PlayersData[faction_id_tgt].diplo_treaties[i] & DTREATY_VENDETTA)
                && PlayersData[i].region_total_bases[region]) {
                region_shared[region] = true;
                break;
            }
        }
    }
    for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
        Veh *veh = &Vehs[veh_id];
        veh->state &= ~VSTATE_UNK_400;
        if (veh->faction_id != faction_id_tgt) {
            continue;
        }
        uint32_t plan = VehPrototypes[veh->proto_id].plan;
        if (plan >= PLAN_TERRAFORMING && !(plan == PLAN_SUPPLY_CONVOY && veh->order == ORDER_CONVOY)) {
            continue;
        }
        Map *tile = map_loc(veh->x, veh->y);
        if ((tile->climate & 0xE0) < ALT_BIT_SHORE_LINE || region_shared[tile->region]) {
            continue;
        }
        int base_id;
        if (whose_territory(faction_id_tgt, veh->x, veh->y, &base_id, true) != (int)faction_id
            || base_id < 0) {
            continue;
        }
        if (base_id_out) {
            *base_id_out = base_id;
        }
        int before = result;
        result++;
        veh->state |= VSTATE_UNK_400;
        if (tile->bit & 0x81608850) {
            result = before + 2; // an improved tile counts double
        }
        if (flags & 1) {
            int x_dist = abs(Bases[base_id].x - veh->x);
            if (!*MapIsFlat && x_dist > (int)*MapLongitude) {
                x_dist = *MapLongitudeBounds - x_dist;
            }
            int y_dist = abs(Bases[base_id].y - veh->y);
            if (((x_dist + y_dist) & ~1) < 5
                || (treaties & (DTREATY_UNK_800 | DTREATY_SHALL_BETRAY | DTREATY_WANT_REVENGE))
                || *DiploFriction > 12) {
                if (count_out && (veh->flags & VFLAG_UNK_1)) {
                    (*count_out)++;
                }
                veh->flags |= VFLAG_UNK_1 | VFLAG_UNK_2;
                if (!(veh->state & VSTATE_UNK_800) && *SunspotDuration < 1) {
                    veh->state |= VSTATE_UNK_800;
                } else {
                    result += ((treaties & DTREATY_TREATY) << 1) | 1;
                }
            }
        }
    }
    if (result) {
        PlayersData[faction_id].flags |= PFLAG_UNK_10000;
    }
    return result;
}

/*
Purpose: Handle an atrocity by the faction against faction_id_tgt (nerve staple, genetic warfare,
         and the like). The victim always holds it against the faction. Unless the UN Charter is
         repealed, sunspots hide it (ATROCITYSPOTS to the player at the controls) or, in SMACX
         with is_alien_counted clear, an alien faction is on either side, the factions in contact
         react: those keeping watch (DTREATY_UNK_200000) gain friction and a computer faction may
         declare a vendetta on a human faction (always when it wants revenge, else by chance:
         less likely against a "great satan"); every faction not at war marks the faction
         (DTREATY_UNK_400000). The atrocity counts (when counted or the first one), adds to the
         integrity blemishes unless the target had wronged the faction this way first, and
         beyond (8 - difficulty) * 4 atrocities it becomes a major atrocity. Otherwise the
         ATROCITY notice and sanctions of ten turns per atrocity follow.
Original Offset: 0055EEE0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch30.log)
*/
void __cdecl atrocity(int faction_id, int faction_id_tgt, int counted, BOOL is_alien_counted) {
    PlayerData *p = &PlayersData[faction_id];
    PlayerData *p_tgt = &PlayersData[faction_id_tgt];
    uint32_t was_victim = p_tgt->diplo_treaties[faction_id] & DTREATY_ATROCITY_VICTIM;
    p_tgt->diplo_treaties[faction_id] |= DTREATY_WANT_REVENGE | DTREATY_ATROCITY_VICTIM;
    p_tgt->diplo_agenda[faction_id] |= DAGENDA_UNK_4;
    if (ProposalPassCount[PROP_REPEAL_UN_CHARTER] > ProposalPassCount[PROP_REINSTATE_UN_CHARTER]) {
        return;
    }
    if (*SunspotDuration > 0) {
        if (faction_id == *FactionIDCurrent) {
            NetMsg_pop(NetMsgVar, NULL, "ATROCITYSPOTS", 5000, 0, NULL);
        }
        return;
    }
    if (*ExpansionEnabled && !is_alien_counted && ((Players[faction_id].rule_flags & RFLAG_ALIEN)
        || (Players[faction_id_tgt].rule_flags & RFLAG_ALIEN))) {
        return;
    }
    int i;
    for (i = 1; i < MaxPlayerNum; i++) {
        PlayerData *p_i = &PlayersData[i];
        if (i == faction_id || i == faction_id_tgt || !(p_i->diplo_treaties[faction_id] & DTREATY_COMMLINK)
            || (*ExpansionEnabled && is_alien_counted && !(Players[faction_id].rule_flags & RFLAG_ALIEN)
                && (Players[i].rule_flags & RFLAG_ALIEN))) {
            continue;
        }
        if (p_i->diplo_treaties[faction_id] & DTREATY_UNK_200000) {
            cause_friction(i, faction_id, 5);
            if (!(FactionsStatus[0] & (1 << i)) && (FactionsStatus[0] & (1 << faction_id))
                && (p->diplo_treaties[i] & DTREATY_COMMLINK)
                && !(p->diplo_treaties[i] & (DTREATY_PACT | DTREATY_VENDETTA)) && !was_victim) {
                BOOL vendetta = true;
                if (!(p_i->diplo_treaties[faction_id] & DTREATY_WANT_REVENGE)) {
                    if (great_satan(faction_id, false)) {
                        vendetta = _rand() % 16 - p->atrocities <= 8;
                    } else {
                        int range = (8 - ((*GameRules & RULES_INTENSE_RIVALRY) ? 5 : *DiffLevelCurrent)) * 2;
                        vendetta = !((range - 1 > 0) ? _rand() % range : 0);
                    }
                }
                if (vendetta) {
                    if (faction_id == *FactionIDCurrent) {
                        parse_says(0, get_title(faction_id), -1, -1);
                        parse_says(1, get_name(faction_id), -1, -1);
                        diplomacy_caption(faction_id, i);
                        X_pops("ATROCIOUSITY", FactionPortraits[i], NULL);
                    }
                    treaty_on(faction_id, i, DTREATY_VENDETTA);
                    p_i->diplo_treaties[faction_id] |= DTREATY_UNK_40;
                    p_i->diplo_merc[faction_id] = 50;
                }
            }
        }
        if (!(p_i->diplo_treaties[faction_id] & DTREATY_VENDETTA)) {
            treaty_on(i, faction_id, DTREATY_UNK_400000);
        }
    }
    if (counted || !p->atrocities) {
        p->atrocities++;
        if (!(p->diplo_treaties[faction_id_tgt] & DTREATY_ATROCITY_VICTIM)) {
            p->integrity_blemishes++;
        }
    }
    int atrocities = p->atrocities;
    if (atrocities > (8 - p->diff_level) * 4) {
        major_atrocity(faction_id, faction_id_tgt);
        return;
    }
    // The original tests the loop counter (8 here) where the target is meant: 1 << 8 is never
    // among the human bits, so a human's repeated atrocities always mark the target.
    if ((FactionsStatus[0] & (1 << faction_id)) && !(FactionsStatus[0] & (1 << i)) && !was_victim
        && atrocities >= 5) {
        p_tgt->flags |= PFLAG_COMMIT_ATROCIT_WANTONLY;
    }
    parse_says(0, get_title(faction_id), -1, -1);
    parse_says(1, get_name(faction_id), -1, -1);
    parse_says(2, get_title(faction_id_tgt), -1, -1);
    parse_says(3, get_name(faction_id_tgt), -1, -1);
    parse_num(0, p->atrocities * 10);
    stringTemp->str[0] = 0;
    strcat(stringTemp->str, "ATROCITY");
    if (faction_id == *FactionIDCurrent) {
        strcat(stringTemp->str, "2");
    } else if (faction_id_tgt == faction_id) {
        strcat(stringTemp->str, "1");
    }
    if (!(Players[faction_id].rule_flags & RFLAG_ALIEN)) {
        NetMsg_pop(NetMsgVar, NULL, stringTemp->str, 5000, 0, NULL);
    }
    p->sanction_turns += atrocities * 10;
}

/*
Purpose: Handle a major atrocity by the faction (faction_id_tgt -1: against no one in
         particular). The victim holds it against the faction. Unless the UN Charter is repealed,
         the integrity blemishes rise by 2 (at least to 6, at most 99, against a faction not
         marked DTREATY_UNK_80000; not touched otherwise), the major atrocities count goes up,
         and every other computer faction still in play that is not at war with it (a pact is
         ended first) declares a vendetta on it with the MAJORATROCITY notice.
Original Offset: 0055F450
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch30.log)
*/
void __cdecl major_atrocity(int faction_id, int faction_id_tgt) {
    if (faction_id_tgt >= 0) {
        PlayersData[faction_id_tgt].diplo_treaties[faction_id]
            |= DTREATY_WANT_REVENGE | DTREATY_ATROCITY_VICTIM | DTREATY_UNK_80000;
        PlayersData[faction_id_tgt].diplo_agenda[faction_id] |= DAGENDA_UNK_4;
    }
    if (ProposalPassCount[PROP_REPEAL_UN_CHARTER] > ProposalPassCount[PROP_REINSTATE_UN_CHARTER]) {
        return;
    }
    PlayerData *p = &PlayersData[faction_id];
    if (faction_id_tgt < 0) {
        p->integrity_blemishes += 2;
    } else if (!(p->diplo_treaties[faction_id_tgt] & DTREATY_UNK_80000)) {
        int blemishes = p->integrity_blemishes + 2;
        p->integrity_blemishes = blemishes < 6 ? 6 : (blemishes > 99 ? 99 : blemishes);
    }
    p->major_atrocities++;
    for (int i = 1; i < MaxPlayerNum; i++) {
        if (i == faction_id || i == faction_id_tgt || (FactionsStatus[0] & (1 << i))
            || !(FactionsStatus[1] & (1 << i)) || (p->diplo_treaties[i] & DTREATY_VENDETTA)) {
            continue;
        }
        if (p->diplo_treaties[i] & DTREATY_PACT) {
            pact_ends(faction_id, i);
        }
        treaty_on(i, faction_id, DTREATY_COMMLINK | DTREATY_VENDETTA);
        PlayersData[i].diplo_treaties[faction_id] |= DTREATY_UNK_40;
        PlayersData[i].diplo_merc[faction_id] = 50;
        p->diplo_spoke[i] = *TurnCurrentNum;
        parse_leader(0, i);
        parse_leader(3, faction_id);
        char number[80];
        stringTemp->str[0] = 0;
        strcat(stringTemp->str, "MAJORATROCITY");
        _itoa(faction_id != *FactionIDCurrent, number, 10);
        strcat(stringTemp->str, number);
        popp(ScriptTxtID, stringTemp->str, 0, "council_sm.pcx", NULL);
    }
}

/*
Purpose: Check an action against the agreements with another faction it would break (any of the
         treaty flags given in force, factions not at war). A pact can't be broken this way
         (BREAKINGPACT); breaking a treaty, a truce (not one marked DTREATY_UNK_10000) or
         starting a vendetta (unless the flags include DTREATY_UNK_10000) is put to the player.
Original Offset: 0055F770
Return Value: Should the action be called off? true (pact, or the player declined) / false
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
BOOL __cdecl break_treaty(int faction_id, int faction_id_with, uint32_t treaty) {
    uint32_t treaties = PlayersData[faction_id].diplo_treaties[faction_id_with];
    if (treaties & DTREATY_VENDETTA || !(treaty & treaties)) {
        return false;
    }
    parse_leader(0, faction_id_with);
    parse_says(3, get_pact_hood(faction_id, faction_id_with), -1, -1);
    parse_says(4, Players[faction_id_with].adj_name_faction, -1, -1);
    if (treaties & DTREATY_PACT) {
        NetMsg_pop(NetMsgVar, 0, "BREAKINGPACT", -5000, 0, "break_sm.pcx");
        return true;
    }
    if (treaties & DTREATY_TREATY) {
        return !popp(ScriptTxtID, "BREAKINGTREATY", 0, "break_sm.pcx", NULL);
    }
    if (treaties & DTREATY_TRUCE && !(treaties & DTREATY_UNK_10000)) {
        return !popp(ScriptTxtID, "BREAKINGTRUCE", 0, "break_sm.pcx", NULL);
    }
    if (!(treaty & DTREATY_UNK_10000)) {
        return !popp(ScriptTxtID, "BEGINVENDETTA", 0, "break_sm.pcx", NULL);
    }
    return false;
}

/*
Purpose: Ask the player at the controls to talk (or queue the request in network and
         hotseat/PBEM games), as enemy_diplomacy() does in two places.
Original Offset: n/a (inlined in enemy_diplomacy)
Return Value: n/a
Status: Complete
*/
static void diplomacy_call(uint32_t faction_id, uint32_t faction_id_human) {
    if (!*IsMultiplayerNet && !*IsMultiplayerPBEM) {
        communicate(faction_id_human, faction_id, 0);
        treaty_on(faction_id_human, faction_id, DTREATY_UNK_80000000 | DTREATY_UNK_8000000
            | DTREATY_COMMLINK);
    } else {
        DiploCommRequest[faction_id] |= 1;
        DiploCommRequestPBEM[faction_id_human * 8 + faction_id] |= 1;
    }
}

/*
Purpose: Each turn's diplomacy of an AI faction. Revenge agendas age (flag 0x20 sets 0x40; the
         next turn 0x20 clears and 0x40 stays). Then for each other living faction: count the
         wars, run down the 50 turn timer of flag 0x40, recover patience on odd turns, carry out a
         planned betrayal (always under a pact, else with odds 1 in 8 - blemishes, less with
         agenda 0x400), clear flag 0x8000 every 16 turns. A human faction in contact (sunspots
         permitting) may be called: to ask for help against a stronger common enemy (0x41), or
         after 32 turns of silence; diplomacy_check() may make the AI plan a betrayal (0x800 /
         0x40). With a lower numbered AI faction, every 8 turns or when one wants to talk: a pact
         trades tech; a treaty under game state flag 0x200, the Empath Guild or flags
         0x20000000 / 0x10000000 lead to an encounter().
         Next the friction with each human faction in contact: units in the AI's territory raise
         requests to talk (or, under sunspots, plans to betray), then (count + 3 + odd turn) / 5 is
         adjusted for tech, rank, military strength, threats, ideology, aggression, energy (Morgan)
         and cooperative victory, and applied unless it would calm a war. Last, while the faction
         has more free orbital defense pods than any non-pact rival has planet busters (half
         under a treaty without a revenge wish), it shoots at the satellites of factions at war
         with it (or flagged 0x800) that have fewer pods, one per action_sat_attack() call.
Original Offset: 0055F930
Return Value: n/a
Status: Complete
*/
void __cdecl enemy_diplomacy(uint32_t faction_id) {
    if (!faction_id) {
        return;
    }
    PlayerData *player = &PlayersData[faction_id];
    for (int i = 1; i < MaxPlayerNum; i++) {
        player->diplo_treaties[i] &= ~DTREATY_UNK_100;
        int agenda = player->diplo_agenda[i];
        if (agenda & DAGENDA_UNK_20) {
            if (!(agenda & DAGENDA_UNK_40)) {
                player->diplo_agenda[i] = agenda | DAGENDA_UNK_40;
                PlayersData[i].diplo_agenda[faction_id] |= DAGENDA_UNK_40;
            } else {
                player->diplo_agenda[i] = agenda & ~DAGENDA_UNK_20;
                PlayersData[i].diplo_agenda[faction_id] &= ~DAGENDA_UNK_20;
            }
        }
    }
    if (FactionsStatus[0] & (1 << faction_id)) {
        return;
    }
    const uint32_t betray_flags = DTREATY_SHALL_BETRAY | DTREATY_UNK_20000;
    int war_count = 0;
    for (uint32_t i = 1; i < MaxPlayerNum; i++) {
        if (i == faction_id || !(FactionsStatus[1] & (1 << i))) {
            continue;
        }
        PlayerData *other = &PlayersData[i];
        uint32_t *treaties = &player->diplo_treaties[i];
        uint32_t *treaties_with = &other->diplo_treaties[faction_id];
        if (*treaties & DTREATY_VENDETTA) {
            war_count++;
        }
        if (player->diplo_merc[i] && !--player->diplo_merc[i]) {
            *treaties &= ~DTREATY_UNK_40;
        }
        if (*TurnCurrentNum & 1 && player->diplo_patience[i] > 0) {
            player->diplo_patience[i]--;
        }
        if (*treaties & DTREATY_SHALL_BETRAY) {
            int odds = 8 - (((uint32_t)player->diplo_agenda[i] >> 9) & 2)
                - other->integrity_blemishes;
            if (odds < 1) {
                odds = 1;
            } else if (odds > 8) {
                odds = 8;
            }
            if (odds - 1 <= 0 || !(_rand() % odds) || (*treaties & DTREATY_PACT)) {
                if (*treaties & DTREATY_PACT) {
                    cause_friction(faction_id, i, 20);
                    *treaties &= ~(betray_flags | DTREATY_UNK_800);
                    treaty_on(faction_id, i, DTREATY_WANT_TO_TALK);
                } else if (other->x_target < 0) {
                    int *spoke = &other->diplo_spoke[faction_id];
                    int turn_min = *TurnCurrentNum - 8;
                    *spoke = (*spoke > turn_min) ? *spoke : turn_min;
                    *treaties = (*treaties & ~betray_flags) | DTREATY_UNK_800 | DTREATY_UNK_40;
                    player->diplo_merc[i] = 50;
                } else {
                    *treaties |= DTREATY_UNK_20000;
                }
            }
        }
        if ((*TurnCurrentNum & 0xF) == (int)faction_id * 2 + 1) {
            *treaties &= ~DTREATY_UNK_8000;
            *treaties_with &= ~DTREATY_UNK_8000;
        }
        uint32_t human = FactionsStatus[0];
        if (human & (1 << i) && *SunspotDuration <= 0) {
            if (i == (uint32_t)*FactionIDCurrent && (*treaties & (DTREATY_COMMLINK
                | DTREATY_VENDETTA | DTREATY_ATROCITY_VICTIM)) == DTREATY_COMMLINK) {
                BOOL called = false;
                uint32_t alive = FactionsStatus[1];
                for (uint32_t j = 1; j < MaxPlayerNum; j++) {
                    if (!(alive & (1 << j)) || j == faction_id || j == i
                        || (player->diplo_treaties[j] & (DTREATY_COMMLINK | DTREATY_VENDETTA
                        | DTREATY_UNK_8000)) != (DTREATY_COMMLINK | DTREATY_VENDETTA
                        | DTREATY_UNK_8000)
                        || (other->diplo_treaties[j] & (DTREATY_COMMLINK | DTREATY_VENDETTA))
                        != DTREATY_COMMLINK
                        || PlayersData[j].mil_strength_1 <= player->mil_strength_1) {
                        continue;
                    }
                    // a common enemy stronger than this faction: ask the human for help
                    if (j < MaxPlayerNum && !(*treaties & DTREATY_PACT)
                        && (int)reputation(i, faction_id) < 5
                        && ((*TurnCurrentNum - other->diplo_spoke[faction_id] > 8
                        && (*TurnCurrentNum & 0xF) == (int)faction_id * 2)
                        || (*treaties & DTREATY_WANT_TO_TALK))) {
                        DiploCommRequest[faction_id] |= 0x41;
                        DiploCommRequestPBEM[i * 8 + faction_id] |= 0x41;
                        called = true;
                    }
                    break;
                }
                if (called) {
                    if (!*IsMultiplayerNet && !*IsMultiplayerPBEM) {
                        communicate(i, faction_id, 0);
                        treaty_on(i, faction_id, DTREATY_UNK_80000000 | DTREATY_UNK_8000000
                            | DTREATY_COMMLINK);
                    }
                } else {
                    uint32_t t = *treaties;
                    if (!(t & DTREATY_WANT_REVENGE)
                        && (*TurnCurrentNum - other->diplo_spoke[faction_id] > 32
                        || (t & DTREATY_UNK_20000000 && *TurnCurrentNum == (int)faction_id + 8)
                        || (t & DTREATY_PACT && t & DTREATY_WANT_TO_TALK))
                        && (!random(0, 16) || *treaties & DTREATY_WANT_TO_TALK
                        || *TurnCurrentNum == (int)faction_id + 8)) {
                        diplomacy_call(faction_id, i);
                    }
                }
            }
            uint32_t requested = *IsMultiplayerPBEM ? DiploCommRequestPBEM[i * 8 + faction_id]
                : DiploCommRequest[faction_id];
            if (!requested) {
                diplomacy_check(i, faction_id, 0);
                if (*DiploUnk3) {
                    uint32_t t = *treaties;
                    if (t & DTREATY_WANT_REVENGE || player->flags_ext & 0x20
                        || (other->diff_level > 4 && *GameRules & RULES_INTENSE_RIVALRY)) {
                        *treaties = (t & ~betray_flags) | DTREATY_UNK_800 | DTREATY_UNK_40;
                        player->diplo_merc[i] = 50;
                    }
                }
            }
        } else if (i < faction_id && *SunspotDuration <= 0
            && (!((*TurnCurrentNum + i + faction_id) & 7) || *treaties & DTREATY_WANT_TO_TALK)) {
            uint32_t t = *treaties;
            if (t & DTREATY_UNK_80000000 && !(t & DTREATY_WANT_TO_TALK)) {
                *treaties = t & ~DTREATY_UNK_80000000;
                *treaties_with &= ~DTREATY_UNK_80000000;
                for (int j = 1; j < MaxPlayerNum; j++) {
                    if (human & (1 << j) && (player->diplo_treaties[j] & DTREATY_VENDETTA
                        || other->diplo_treaties[j] & DTREATY_VENDETTA)) {
                        *treaties &= ~DTREATY_UNK_4000000;
                        *treaties_with &= ~DTREATY_UNK_4000000;
                    }
                }
            } else {
                *treaties = t & ~DTREATY_WANT_TO_TALK;
                *treaties_with &= ~DTREATY_WANT_TO_TALK;
                if (*treaties & DTREATY_PACT) {
                    enemies_trade_tech(faction_id, i);
                } else {
                    int base_id_empath = SecretProject->empath_guild;
                    if ((*treaties & DTREATY_TREATY && *GameState & STATE_UNK_200)
                        || (base_id_empath >= 0
                        && Bases[base_id_empath].faction_id_current == faction_id)
                        || (base_id_empath >= 0 && Bases[base_id_empath].faction_id_current == i)
                        || *treaties & DTREATY_UNK_20000000
                        || (*treaties & DTREATY_COMMLINK && *treaties & DTREATY_UNK_10000000)) {
                        encounter(faction_id, i, -1, -1, 0, 0);
                    }
                }
                treaty_on(faction_id, i, DTREATY_UNK_80000000);
                *treaties &= ~DTREATY_UNK_4000000;
                *treaties_with &= ~DTREATY_UNK_4000000;
            }
        }
    }
    // friction with each human faction in contact
    int base_id_scan = -1; // see the orbital defense part below
    int repeat_count;
    for (uint32_t i = 1; i < MaxPlayerNum; i++) {
        if (!(FactionsStatus[0] & (1 << i))) {
            continue;
        }
        PlayerData *other = &PlayersData[i];
        uint32_t *treaties = &player->diplo_treaties[i];
        if (!(*treaties & DTREATY_COMMLINK)) {
            continue;
        }
        int friction;
        if (*TurnCurrentNum - other->diplo_spoke[faction_id] < 4 && *SunspotDuration <= 0
            && !(*treaties & DTREATY_WANT_REVENGE)) {
            friction = 0;
        } else {
            friction = territory(faction_id, i, 1, &base_id_scan, &repeat_count);
            if (friction && i == (uint32_t)*FactionIDCurrent && !(*treaties & DTREATY_VENDETTA)) {
                if (*SunspotDuration > 0) {
                    diplomacy_check(i, faction_id, 0);
                    if (*DiploUnk9 || *treaties & DTREATY_WANT_REVENGE
                        || *DiploFriction + other->integrity_blemishes * 2 + friction
                        >= (9 - player->ai_fight) * 2) {
                        uint32_t t = *treaties;
                        if (t & DTREATY_TREATY) {
                            *treaties = t | DTREATY_UNK_800;
                        } else if (!(t & (DTREATY_SHALL_BETRAY | DTREATY_UNK_800))) {
                            *treaties = t | ((t & DTREATY_WANT_REVENGE) ? DTREATY_UNK_800
                                : DTREATY_SHALL_BETRAY);
                        }
                    }
                } else {
                    uint32_t t = *treaties;
                    uint32_t *request = &DiploCommRequest[faction_id];
                    uint32_t *request_pbem = &DiploCommRequestPBEM[i * 8 + faction_id];
                    BOOL roll = false;
                    int roll_max = 0;
                    if (t & DTREATY_TREATY) {
                        if (friction >= 5) {
                            *request_pbem |= 4;
                            *request |= 4;
                        } else if (!((*TurnCurrentNum + faction_id) & 3)) {
                            *request |= 0x20;
                            *request_pbem |= 0x20;
                        }
                    } else {
                        int agenda = player->diplo_agenda[i];
                        if (agenda & DAGENDA_UNK_800
                            || (agenda & DAGENDA_UNK_400 && repeat_count)) {
                            *request |= 0x18;
                            *request_pbem |= 0x18;
                        } else if (t & DTREATY_WANT_REVENGE && !repeat_count) {
                            *request |= 8;
                            *request_pbem |= 8;
                            roll = true;
                            roll_max = friction + (other->integrity_blemishes + 1) * 5;
                        } else if (!((*TurnCurrentNum + faction_id) & 3)) {
                            *request |= 8;
                            *request_pbem |= 8;
                            roll = true;
                            roll_max = ((t & DTREATY_WANT_REVENGE) ? 5 : 0) + friction
                                + (other->integrity_blemishes + repeat_count) * 5;
                        }
                    }
                    if (roll && (int)random(0, roll_max) > 10) {
                        if (i == (uint32_t)*FactionIDCurrent) {
                            *request |= 0x10;
                        }
                        *request_pbem |= 0x10;
                    }
                    if (*request && !*IsMultiplayerNet && !*IsMultiplayerPBEM) {
                        communicate(i, faction_id, 0);
                        treaty_on(i, faction_id, DTREATY_UNK_80000000 | DTREATY_UNK_8000000
                            | DTREATY_COMMLINK);
                    }
                }
            }
        }
        friction = ((*TurnCurrentNum & 1) + friction + 3) / 5;
        if (*treaties & DTREATY_PACT) {
            if (!war_count) {
                int tech_half = player->tech_ranking / 2;
                int tech_half_other = other->tech_ranking / 2;
                if (tech_half_other > player->ai_tech - player->ai_power - player->ai_fight
                    + tech_half + 5) {
                    friction++;
                }
                if (tech_half > tech_half_other + 5) {
                    friction--;
                }
                if ((int)other->ranking > (int)player->ranking + 1 && other->diff_level) {
                    friction++;
                }
            } else {
                friction += 1 - war_count;
            }
            if (!(*treaties & DTREATY_UNK_1000000)) {
                for (int j = 1; j < MaxPlayerNum; j++) {
                    if (player->diplo_treaties[j] & DTREATY_VENDETTA
                        && other->diplo_treaties[j] & DTREATY_COMMLINK
                        && !(other->diplo_treaties[j] & DTREATY_VENDETTA)) {
                        friction++;
                    }
                }
            }
            if (*GameRules & RULES_VICTORY_COOPERATIVE && (int)player->ranking < 6
                && *TurnCurrentNum >= 200 && friction >= 0) {
                friction = -1;
            }
        } else {
            if (*GameRules & RULES_INTENSE_RIVALRY && *TurnCurrentNum & 1) {
                friction++;
            }
            int strength = player->mil_strength_1;
            int strength_other = other->mil_strength_1;
            int div_other = strength_other * 3;
            div_other = (div_other < 1) ? 1 : ((div_other > 99999) ? 99999 : div_other);
            friction += (strength * 2) / div_other;
            int div = strength * 3;
            div = (div < 1) ? 1 : ((div > 99999) ? 99999 : div);
            friction += (-strength_other * 2) / div;
            if (great_satan(i, false)) {
                friction++;
            }
            if (aah_ooga(i, -1)) {
                friction++;
            }
        }
        if (*TurnCurrentNum & 1) {
            int category = Players[faction_id].soc_ideology_category;
            int model = Players[faction_id].soc_ideology_model;
            if (category >= 0 && model) {
                uint32_t pending = *(&other->soc_category_pending.politics + category);
                if (pending == (uint32_t)model) {
                    friction--;
                } else if (pending) {
                    friction++;
                }
            }
        }
        friction += player->ai_fight;
        if (!(*TurnCurrentNum & 3)) {
            if (*treaties & DTREATY_TREATY) {
                friction--;
            }
            int rank = player->ranking;
            if (rank >= 6 && (int)other->ranking >= 6) {
                friction += 3;
            } else if (rank >= 5 && (int)other->ranking >= 5) {
                if (*TurnCurrentNum >= 100) {
                    friction++;
                }
            } else {
                int rank_diff = (int)other->ranking - rank;
                int sign = (rank_diff > 0) ? 1 : ((rank_diff >= 0) ? 0 : -1);
                friction += sign * 3 - (rank_diff + sign) / 2;
            }
            if (other->energy_reserves > player->energy_reserves
                && !strcmp(Players[faction_id].filename, "MORGAN")) {
                friction++;
            }
            if (other->tech_ranking > player->tech_ranking) {
                friction += 1 + player->ai_power - player->ai_tech;
            }
        }
        if (*treaties & DTREATY_PACT && *GameRules & RULES_VICTORY_COOPERATIVE
            && (int)other->ranking > (int)player->ranking && *TurnCurrentNum >= 200) {
            friction = (friction > 0) ? 0 : friction;
        }
        *treaties &= ~DTREATY_UNK_200;
        other->diplo_treaties[faction_id] &= ~DTREATY_UNK_200;
        if (!(*treaties & DTREATY_VENDETTA) || friction >= 0) {
            cause_friction(faction_id, i, friction);
        }
    }
    // orbital defense pods beyond the planet buster threat attack enemy satellites
    int threat = 0;
    for (uint32_t i = 1; i < MaxPlayerNum; i++) {
        uint32_t t = player->diplo_treaties[i];
        if (i == faction_id || t & DTREATY_PACT) {
            continue;
        }
        int busters = PlayersData[i].planet_busters;
        int busters_weighed = (t & DTREATY_TREATY && !(t & DTREATY_WANT_REVENGE))
            ? busters / 2 : busters;
        if (threat <= busters_weighed) {
            threat = busters_weighed;
        }
    }
    // base_id_scan is only meaningful for a type 4 attack, which sets it. Otherwise the original
    // passes whatever its stack slot holds: the last base scan's end, territory()'s last base,
    // or (if neither ran) an uninitialized value; action_sat_attack() ignores it.
    int type = 0;
    while ((int)(player->satellites_odp_total - player->sat_odp_deployed) > threat) {
        if (type < 0) {
            return;
        }
        type = -1;
        uint32_t i;
        for (i = 1; i < MaxPlayerNum; i++) {
            PlayerData *other = &PlayersData[i];
            int pods_other = other->satellites_odp_total;
            if (!(player->diplo_treaties[i] & (DTREATY_VENDETTA | DTREATY_UNK_800))
                || (int)player->satellites_odp_total <= pods_other) {
                continue;
            }
            if (pods_other && pods_other >= player->planet_busters) {
                type = 3;
                break;
            }
            int survey_pods = 0;
            for (base_id_scan = 0; base_id_scan < *BaseCurrentCount; base_id_scan++) {
                if (Bases[base_id_scan].faction_id_current == i
                    && has_fac_built(FAC_GEOSYNC_SURVEY_POD, base_id_scan)) {
                    survey_pods++;
                }
            }
            if (other->satellites_mineral) {
                type = 1;
            } else if (other->satellites_energy) {
                type = 2;
            } else if (other->satellites_nutrient) {
                type = 0;
            } else if (survey_pods) {
                type = 4;
                for (base_id_scan = 0; base_id_scan < *BaseCurrentCount; base_id_scan++) {
                    if (Bases[base_id_scan].faction_id_current == i
                        && has_fac_built(FAC_GEOSYNC_SURVEY_POD, base_id_scan)) {
                        break;
                    }
                }
            } else if (other->satellites_odp_total) {
                type = 3;
            } else {
                continue;
            }
            break;
        }
        if (type >= 0) {
            action_sat_attack(faction_id, i, type, base_id_scan);
        }
    }
}

/*
Purpose: Determine the ideal unit count to protect a faction's bases in the specified land region.
Original Offset: 00560D50
Return Value: Amount of non-offensive units needed to guard region
Status: Complete
*/
uint32_t __cdecl guard_check(uint32_t faction_id, uint32_t region) {
    if (region >= MaxRegionLandNum) {
        return 0;
    }
    int factor = 2 - PlayersData[faction_id].ai_fight;
    uint32_t plan_region = PlayersData[faction_id].region_base_plan[region];
    if (plan_region == PLAN_COLONIZATION) {
        factor += 2;
    } else if (plan_region == PLAN_DEFENSIVE) {
        factor = 1; // 1-1 unit per base ratio
    }
    if (PlayersData[faction_id].flags & PFLAG_STRAT_DEF_OBJECTIVES) {
        factor = 1; // 1-1 unit per base ratio
    }
    return (PlayersData[faction_id].region_total_bases[region] + factor - 1) / factor;
}

/*
Purpose: Add the specific goal to the faction's goals for the specified tile. Optional base param.
Original Offset: 00579A30
Return Value: n/a
Status:  Complete
*/
void __cdecl add_goal(uint32_t faction_id, int type, int priority, int x, int y, int base_id) {
    if (!on_map(x, y)) {
        return;
    }
    for (int i = 0; i < MaxGoalsNum; i++) {
        Goal &goals = PlayersData[faction_id].goals[i];
        if (goals.x == x && goals.y == y && goals.type == type) {
            if (goals.priority <= priority) {
                goals.priority = (int16_t)priority;
            }
            return;
        }
    }
    int priority_search = 0;
    int goal_id = -1;
    for (int i = 0; i < MaxGoalsNum; i++) {
        Goal &goals = PlayersData[faction_id].goals[i];
        int type_cmp = goals.type;
        int priroty_cmp = goals.priority;
        if (type_cmp < 0 || priroty_cmp < priority) {
            int cmp = type_cmp >= 0 ? 0 : 1000;
            if (!cmp) {
                cmp = priroty_cmp > 0 ? 20 - priroty_cmp : priroty_cmp + 100;
            }
            if (cmp > priority_search) {
                priority_search = cmp;
                goal_id = i;
            }
        }
    }
    if (goal_id >= 0) {
        Goal &goals = PlayersData[faction_id].goals[goal_id];
        goals.type = (int16_t)type;
        goals.priority = (int16_t)priority;
        goals.x = x;
        goals.y = y;
        goals.base_id = base_id;
    }
}

/*
Purpose: Add the specific site to the faction's site goals for the specified tile.
Original Offset: 00579B70
Return Value: n/a
Status: Complete
*/
void __cdecl add_site(uint32_t faction_id, int type, int priority, int x, int y) {
    if ((x ^ y) & 1 && *GameState & STATE_DEBUG_MODE) {
        danger("Bad SITE", "", x, y, type);
    }
    for (int i = 0; i < MaxSitesNum; i++) {
        Goal &sites = PlayersData[faction_id].sites[i];
        if (sites.x == x && sites.y == y && sites.type == type) {
            if (sites.priority <= priority) {
                sites.priority = (int16_t)priority;
            }
            return;
        }
    }
    int priority_search = 0;
    int site_id = -1;
    for (int i = 0; i < MaxSitesNum; i++) {
        Goal &sites = PlayersData[faction_id].sites[i];
        int type_cmp = sites.type;
        int priroty_cmp = sites.priority;
        if (type_cmp < 0 || priroty_cmp < priority) {
            int cmp = type_cmp >= 0 ? 0 : 1000;
            if (!cmp) {
                cmp = 20 - priroty_cmp;
            }
            if (cmp > priority_search) {
                priority_search = cmp;
                site_id = i;
            }
        }
    }
    if (site_id >= 0) {
        Goal &sites = PlayersData[faction_id].sites[site_id];
        sites.type = (int16_t)type;
        sites.priority = (int16_t)priority;
        sites.x = x;
        sites.y = y;
        add_goal(faction_id, type, priority, x, y, -1);
    }
}

/*
Purpose: Check if a goal exists at the tile for the specified faction and type.
Original Offset: 00579CC0
Return Value: Does the specific goal exist for the faction at tile? true/false
Status: Complete
*/
BOOL __cdecl at_goal(uint32_t faction_id, int type, int x, int y) {
    for (int i = 0; i < MaxGoalsNum; i++) {
        Goal &goals = PlayersData[faction_id].goals[i];
        if (goals.x == x && goals.y == y && goals.type == type) {
            return true;
        }
    }
    return false;
}

/*
Purpose: Check if a site exists at the tile for the specified faction and type.
Original Offset: 00579D20
Return Value: Does specific site exist for faction at tile? true/false
Status: Complete
*/
BOOL __cdecl at_site(uint32_t faction_id, int type, int x, int y) {
    for (int i = 0; i < MaxSitesNum; i++) {
        Goal &sites = PlayersData[faction_id].sites[i];
        if (sites.x == x && sites.y == y && sites.type == type) {
            return true;
        }
    }
    return false;
}

/*
Purpose: Wipe all goals for the specified faction then recreate any active site related goals.
Original Offset: 00579D80
Return Value: n/a
Status: Complete
*/
void __cdecl wipe_goals(uint32_t faction_id) {
    for (int i = 0; i < MaxGoalsNum; i++) {
        Goal &goals = PlayersData[faction_id].goals[i];
        int16_t priority = goals.priority;
        if (priority < 0) {
            goals.type = AI_GOAL_UNUSED;
        } else {
            goals.priority = -priority;
        }
    }
    for (int i = 0; i < MaxSitesNum; i++) {
        Goal &sites = PlayersData[faction_id].sites[i];
        int16_t type = sites.type;
        if (type >= 0) {
            add_goal(faction_id, type, sites.priority, sites.x, sites.y, -1);
        }
    }
}

/*
Purpose: Initialize all goals for the specified faction.
Original Offset: 00579E00
Return Value: n/a
Status: Complete
*/
void __cdecl init_goals(uint32_t faction_id) {
    for (int i = 0; i < MaxGoalsNum; i++) {
        Goal &goals = PlayersData[faction_id].goals[i];
        goals.type = -1;
        goals.priority = 0;
        goals.x = 0;
        goals.y = 0;
        goals.base_id = 0;
    }
    for (int i = 0; i < MaxSitesNum; i++) {
        Goal &sites = PlayersData[faction_id].sites[i];
        sites.type = -1;
        sites.priority = 0;
        sites.x = 0;
        sites.y = 0;
        sites.base_id = 0;
    }
}

/*
Purpose: Delete sites of the specified type within proximity of the tile along with related goals.
Original Offset: 00579E70
Return Value: n/a
Status: Complete
*/
void __cdecl del_site(uint32_t faction_id, int type, int x, int y, int proximity) {
    for (int i = 0; i < MaxSitesNum; i++) {
        Goal &sites = PlayersData[faction_id].sites[i];
        if (sites.type == type) {
            int dist = vector_dist(x, y, sites.x, sites.y);
            if (dist <= proximity) {
                sites.type = AI_GOAL_UNUSED;
                sites.priority = 0;
                for (int j = 0; j < MaxGoalsNum; j++) {
                    Goal &goal_compare = PlayersData[faction_id].goals[j];
                    if (goal_compare.x == sites.x && goal_compare.y == sites.y &&
                        goal_compare.type == type) {
                        goal_compare.type = AI_GOAL_UNUSED;
                    }
                }
            }
        }
    }
}

/*
Purpose: Calculate the cost for the faction to corner the Global Energy Market (Economic Victory).
Original Offset: 0059EE50
Return Value: Cost to corner the Global Energy Market
Status: Complete
*/
uint32_t __cdecl corner_market(uint32_t faction_id) {
    int cost = 0;
    for (int i = 0; i < *BaseCurrentCount; i++) {
        uint32_t target_faction_id = Bases[i].faction_id_current;
        if (target_faction_id != faction_id) {
            if (!has_treaty(target_faction_id, faction_id, DTREATY_PACT)
                || !has_treaty(target_faction_id, faction_id, DTREATY_HAVE_SURRENDERED)) {
                cost += mind_control(i, faction_id, true);
            }
        }
    }
    return (cost < 1000) ? 1000 : cost;
}

/*
Purpose: Validate whether each faction meets the requirements to have the Map revealed. Added some
         minor tweaks to improve performance without changing the logic.
Original Offset: 005A96D0
Return Value: n/a
Status: Complete
*/
void __cdecl see_map_check() {
    for (int faction_id = 1; faction_id < MaxPlayerNum; faction_id++) {
        PlayersData[faction_id].flags &= ~PFLAG_MAP_REVEALED;
        uint32_t *satellites = &PlayersData[faction_id].satellites_nutrient;
        for (int i = 0; i < 4; i++, satellites++) {
            if (*satellites) {
                PlayersData[faction_id].flags |= PFLAG_MAP_REVEALED;
                break; // end satellite loop early once set
            }
        }
        if (!(PlayersData[faction_id].flags & PFLAG_MAP_REVEALED)) { // skip Tech check if set
            for (int tech_id = 0; tech_id < MaxTechnologyNum; tech_id++) {
                if (Technology[tech_id].flags & TFLAG_REVEALS_MAP 
                    && has_tech(tech_id, faction_id)) {
                    PlayersData[faction_id].flags |= PFLAG_MAP_REVEALED;
                }
            }
        }
    }
}

/*
Purpose: Calculate the basic social engineering modifiers for the specified faction.
Original Offset: 005B0D70
Return Value: n/a
Status: Complete
*/
void __cdecl compute_faction_modifiers(uint32_t faction_id) {
    ZeroMemory(&PlayersData[faction_id].soc_effect_base, sizeof(SocialEffect));
    int count = Players[faction_id].faction_bonus_count;
    for (int i = 0; i < count; i++) {
        if (Players[faction_id].faction_bonus_id[i] == RULE_SOCIAL) {
            *(&PlayersData[faction_id].soc_effect_base.economy
                + Players[faction_id].faction_bonus_val1[i]) 
                += Players[faction_id].faction_bonus_val2[i];
        }
    }
}

/*
Purpose: Calculate the social engineering effect modifiers for the specified faction.
Original Offset: 005B4210
Return Value: n/a
Status: Complete
*/
void __cdecl social_calc(SocialCategory *category, SocialEffect *effect, uint32_t faction_id,
    BOOL UNUSED(toggle), BOOL is_quick_calc) {
    ZeroMemory(effect, sizeof(SocialEffect));
    for (int cat = 0; cat < MaxSocialCatNum; cat++) {
        uint32_t model = *(&category->politics + cat);
        for (int eff = 0; eff < MaxSocialEffectNum; eff++) {
            int effect_val = *(&SocialCategories[cat].model_effect[model].economy + eff);
            if (effect_val < 0) {
                if (cat == SOCIAL_CAT_FUTURE) {
                    if (model == SE_CYBERNETIC) {
                        if (has_project(SP_NETWORK_BACKBONE, faction_id)) {
                            effect_val = 0;
                        }
                    } else if (model == SE_THOUGHT_CONTROL) {
                        if (has_project(SP_CLONING_VATS, faction_id)) {
                            effect_val = 0;
                        }
                    }
                } else if (cat == SOCIAL_CAT_VALUES && model == SE_POWER 
                    && has_project(SP_CLONING_VATS, faction_id)) {
                    effect_val = 0;
                }
                if (effect_val < 0) {
                    for (int i = 0; i < Players[faction_id].faction_bonus_count; i++) {
                        if (Players[faction_id].faction_bonus_val1[i] == cat
                            && Players[faction_id].faction_bonus_val2[i] == (int)model) {
                            if (Players[faction_id].faction_bonus_id[i] == RULE_IMPUNITY) {
                                *(&effect->economy + eff) -= effect_val; // negates neg effects
                            } else if (Players[faction_id].faction_bonus_id[i] == RULE_PENALTY) {
                                *(&effect->economy + eff) += effect_val; // doubles neg effects
                            }
                        }
                    }
                }
            }
            *(&effect->economy + eff) += effect_val;
        }
    }
    if (!is_quick_calc) {
        if (has_project(SP_ASCETIC_VIRTUES, faction_id)) {
            effect->police++;
        }
        if (has_project(SP_LIVING_REFINERY, faction_id)) {
            effect->support += 2;
        }
        if (has_temple(faction_id)) {
            effect->planet++;
            if (is_alien_faction(faction_id)) {
                effect->research++; // bonus documented in conceptsx.txt but not manual
            }
        }
        SocialEffect *effect_calc = effect;
        SocialEffect *effect_base = &PlayersData[faction_id].soc_effect_base;
        for (int eff = 0; eff < MaxSocialEffectNum; eff++) {
            *(&effect_calc->economy + eff) += *(&effect_base->economy + eff);
        }
        for (int i = 0; i < Players[faction_id].faction_bonus_count; i++) {
            if (Players[faction_id].faction_bonus_id[i] == RULE_IMMUNITY) { // cancels neg effects
                int *eff_faction_mod 
                    = (&effect->economy + Players[faction_id].faction_bonus_val1[i]);
                *eff_faction_mod = range(*eff_faction_mod, 0, 999);
            } else if (Players[faction_id].faction_bonus_id[i] == RULE_ROBUST) {
                int *eff_faction_mod 
                    = (&effect->economy + Players[faction_id].faction_bonus_val1[i]);
                if (*eff_faction_mod < 0) {
                    *eff_faction_mod /= 2; // halves neg effects
                }
            }
        }
    }
}

/*
Purpose: Handle the social engineering turn upkeep for the specified faction.
Original Offset: 005B44D0
Return Value: n/a
Status: Complete
*/
void __cdecl social_upkeep(uint32_t faction_id) {
    for (int i = 0; i < MaxSocialCatNum; i++) {
        *(&PlayersData[faction_id].soc_category_active.politics + i) =
            *(&PlayersData[faction_id].soc_category_pending.politics + i);
    }
    SocialCategory *soc_cat_pen = &PlayersData[faction_id].soc_category_pending;
    social_calc(soc_cat_pen, &PlayersData[faction_id].soc_effect_pending, faction_id, false, false);
    social_calc(soc_cat_pen, &PlayersData[faction_id].soc_effect_active, faction_id, false, false);
    social_calc(soc_cat_pen, &PlayersData[faction_id].soc_effect_temp, faction_id, true, false);
    PlayersData[faction_id].soc_upheaval_cost_paid = 0;
}

/*
Purpose: Calculate the cost of the social upheaval for the specified faction.
Original Offset: 005B4550
Return Value: Social upheaval cost
Status: Complete
*/
uint32_t __cdecl social_upheaval(uint32_t faction_id, SocialCategory *category_new) {
    uint32_t change_count = 0;
    for (int i = 0; i < MaxSocialCatNum; i++) {
        if (*(&category_new->politics + i) != 
            *(&PlayersData[faction_id].soc_category_active.politics + i)) {
            change_count++;
        }
    }
    if (!change_count) {
        return 0;
    }
    change_count++;
    uint32_t diff_lvl = is_human(faction_id) ? PlayersData[faction_id].diff_level : DLVL_LIBRARIAN;
    uint32_t cost = change_count * change_count * change_count * diff_lvl;
    if (is_alien_faction(faction_id)) {
        cost += cost / 2;
    }
    return cost;
}

/*
Purpose: Apply a faction's pending social engineering choice. The stored nutrients and minerals of
         its bases are rescaled when the nutrient or mineral cost factors change, a human's bases
         are recomputed, and the base window is redrawn for the player at the controls.
Original Offset: 005B4600
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch6.log)
*/
void __cdecl social_set(uint32_t faction_id) {
    int nutrient_factor_old = cost_factor(faction_id, 0, -1);
    int mineral_factor_old = cost_factor(faction_id, 1, -1);
    social_calc(&PlayersData[faction_id].soc_category_pending,
        &PlayersData[faction_id].soc_effect_pending, faction_id, false, false);
    int nutrient_factor = cost_factor(faction_id, 0, -1);
    int mineral_factor = cost_factor(faction_id, 1, -1);
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (Bases[i].faction_id_current != faction_id) {
            continue;
        }
        if (nutrient_factor != nutrient_factor_old) {
            Bases[i].nutrients_accumulated = (nutrient_factor_old / 2
                + nutrient_factor * Bases[i].nutrients_accumulated) / nutrient_factor_old;
        }
        if (mineral_factor != mineral_factor_old) {
            int minerals = (mineral_factor_old / 2 + Bases[i].minerals_accumulated * mineral_factor)
                / mineral_factor_old;
            Bases[i].minerals_accumulated = minerals;
            Bases[i].minerals_accumulated_2 = minerals;
        }
        if (!*IsMultiplayerNet && is_human(faction_id)) {
            set_base(i);
            base_compute(1);
        }
    }
    if (faction_id == (uint32_t)*FactionIDCurrent) {
        GraphicWin_redraw(BaseWinVar, 0);
    }
}

/*
Purpose: Check to see whether the faction can utilize a specific social category and model.
Original Offset: 005B4730
Return Value: Is social category/model available to faction? true/false
Status: Complete
*/
BOOL __cdecl society_avail(int soc_category, int soc_model, uint32_t faction_id) {
    if (Players[faction_id].soc_anti_ideology_category == soc_category
        && Players[faction_id].soc_anti_ideology_model == soc_model) {
        return false;
    }
    return has_tech(SocialCategories[soc_category].preq_tech[soc_model], faction_id);
}

/*
Purpose: Calculate an AI faction's social engineering.
Original Offset: 005B4790
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log; one timing-dependent case, see docs/regression-testing.md)
*/
void __cdecl social_ai(uint32_t faction_id, int growth_val, int tech_val, int wealth_val, 
                       int power_val, SocialCategory *output) {
    // setup
    int fight_val;
    int unk_val1 = -1;
    if (!output) {
        if (is_human(faction_id)) {
            return;
        } else {
            growth_val = PlayersData[faction_id].ai_growth;
            tech_val = PlayersData[faction_id].ai_tech;
            wealth_val = PlayersData[faction_id].ai_wealth;
            power_val = PlayersData[faction_id].ai_power;
            fight_val = PlayersData[faction_id].ai_fight;
        }
    } else if (growth_val < 0) {
        return;
    } else if (growth_val > 100) {
        unk_val1 = growth_val - 100;
        growth_val = PlayersData[faction_id].ai_growth;
        tech_val = PlayersData[faction_id].ai_tech;
        wealth_val = PlayersData[faction_id].ai_wealth;
        power_val = PlayersData[faction_id].ai_power;
        fight_val = PlayersData[faction_id].ai_fight;
    } else {
        if (!power_val) {
            fight_val = -(wealth_val != 0);
        } else {
            fight_val = 1;
        }
    }
    uint32_t unk_val2 = 4; // 4 << 6 = 256: every combination of 4 models in 4 categories
    for (int i = 3; i > 0; i--) {
        unk_val2 *= 4;
    }
    // future pop growth
    int pop_goal_growth = 0;
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (Bases[i].faction_id_current == faction_id) {
            pop_goal_growth += pop_goal(i) - Bases[i].population_size;
        }
    }
    // plan and enemy region metrics
    int unk_sum1 = 0;
    int unk_count1 = 0;
    int unk_count2 = 0;
    uint32_t unk_sum2 = 0;
    for (uint32_t region = 1; region < MaxRegionLandNum; region++) {
        if (!bad_reg(region)) {
            uint8_t base_count = PlayersData[faction_id].region_total_bases[region];
            if (base_count) {
                uint8_t plan = PlayersData[faction_id].region_base_plan[region];
                if (plan == PLAN_DEFENSIVE || PlayersData[faction_id].unk_77[region] & 0x400) {
                    unk_sum1 += PlayersData[faction_id].region_total_combat_vehs[region] * 2;
                } else if (plan == PLAN_OFFENSIVE) {
                    unk_sum1 += PlayersData[faction_id].region_total_combat_vehs[region];
                    unk_count2++;
                } else if (plan == PLAN_COLONIZATION) {
                    unk_count1++;
                }
                if (plan <= PLAN_DEFENSIVE) {
                    for (uint32_t f = 1; f < MaxPlayerNum; f++) {
                        if (f != faction_id) {
                            if (has_treaty(faction_id, f, DTREATY_VENDETTA | DTREATY_WANT_REVENGE)
                                && PlayersData[f].enemy_best_psi_offense 
                                >= PlayersData[f].enemy_best_weapon_value 
                                && PlayersData[f].proto_id_active[BSC_MIND_WORMS] > 1) {
                                unk_sum2 += base_count; // not used
                            }
                        }
                    }
                }
            }
        }
    }
    if (output && power_val && unk_sum1 < 1) {
        unk_sum1 = 1;
    }
    // tech ranking
    int unk_val3 = 1;
    if (*GameState & 0x200 // set in rankings(), related to intense riv + end game
        && PlayersData[faction_id].ranking < 6) {
        int tech_rank_diff = PlayersData[*RankingFactionIDUnk1].tech_ranking / 2
            - PlayersData[faction_id].tech_ranking / 2;
        if (tech_rank_diff > 5) {
            unk_val3 = 2;
        }
        if (tech_rank_diff > 10) {
            unk_val3++;
        }
    }
    // search / calculate
    int unk_val4 = -9999; // search val?
    int soc_cat_bits = -1;
    // unk_sum2 = 0 > used as iterator, optimization re-use, var unused
    for (uint32_t i = 0; i < unk_val2; i++) {
        SocialCategory soc_cat;
        SocialEffect soc_eff;
        int k = i;
        BOOL is_valid = true;
        for (int j = 0; j < MaxSocialCatNum; j++) {
            int model = k & 3;
            *(&soc_cat.politics + j) = model;
            if (model) {
                if ((Players[faction_id].soc_anti_ideology_category == j
                    && Players[faction_id].soc_anti_ideology_model == model)
                    || !has_tech(SocialCategories[j].preq_tech[model], faction_id)) {
                    is_valid = false;
                    break;
                }
            }
            k >>= 2;
        }
        if (!is_valid) {
            continue; // combination not available to this faction
        }
        do_all_non_input();
        social_calc(&soc_cat, &soc_eff, faction_id, false, false);
        int unk_val6 = 0;
        if (unk_val1 >= 0) {
            unk_val6 = *(&soc_eff.economy + unk_val1) * 1000;
        }
        BOOL has_ideology = false;
        int ideology_cat = Players[faction_id].soc_ideology_category;
        if (ideology_cat >= 0) {
            int ideology_mod = Players[faction_id].soc_ideology_model;
            if (ideology_mod) {
                int unk_val7 = *(&soc_cat.politics + ideology_cat);
                if (unk_val7 == ideology_mod) {
                    unk_val6 += PlayersData[faction_id].current_num_bases;
                } else if (unk_val7) {
                    has_ideology = true; // a different model was chosen in the ideology's category
                }
            }
        }
        int ideology_eff = Players[faction_id].soc_ideology_effect;
        if (ideology_eff >= 0) {
            int unk_val8 = *(&soc_eff.economy + ideology_eff);
            if (unk_val8 > 0) {
                unk_val6 += PlayersData[faction_id].current_num_bases;
            }
            if (unk_val8 < 0) {
                has_ideology = true;
            }
        }
        if (!has_ideology) {
            // economy
            int effic_val = PlayersData[faction_id].unk_46[range(4 - soc_eff.efficiency, 0, 8)];
            int econ_val = soc_eff.economy;
            int econ_weight = 0;
            if (econ_val < 2) {
                if (econ_val <= 0) {
                    if (econ_val < -1) {
                        for (int region = 0; region < MaxContinentNum; region++) {
                            econ_weight -= (soc_eff.economy + 1)
                                * PlayersData[faction_id].region_total_bases[region]
                                * ((PlayersData[faction_id].region_base_plan[region] 
                                    != PLAN_DEFENSIVE) + 1);
                        }
                        econ_weight /= (unk_sum1 + 1);
                    } else {
                        econ_weight = -1;
                    }
                } else {
                    for (int region = 0; region < MaxContinentNum; region++) {
                        econ_weight += PlayersData[faction_id].region_total_bases[region]
                            * ((PlayersData[faction_id].region_base_plan[region] != PLAN_DEFENSIVE) 
                                + 1);
                    }
                    econ_weight /= (unk_sum1 + 1);
                }
            } else {
                if (econ_val > 4) {
                    econ_val = 4;
                }
                econ_weight = (PlayersData[faction_id].unk_47 - 4 + econ_val * 2) / (unk_sum1 + 1);
            }
            for (; econ_weight > 0; econ_weight--, effic_val--) {
                if (effic_val <= 0) {
                    break;
                }
            }
            if (!unk_sum1) {
                econ_weight *= 2;
            }
            econ_weight *= ((PlayersData[faction_id].ai_fight < 0 && unk_sum1 < 2) 
                + tech_val * 2 + wealth_val + 1);
            if (output) {
                if (wealth_val || tech_val) {
                    econ_weight *= 2;
                }
                if (growth_val || power_val) {
                    econ_weight /= 2;
                }
            } else if (wealth_val && !power_val && !growth_val) {
                econ_weight *= 2;
            }
            unk_val6 += econ_weight / unk_val3;
            // support
            int support_val = range(soc_eff.support + 4, 0, 7);
            int support_weight = PlayersData[faction_id].support_cost_by_level[support_val];
            if (soc_eff.support <= -4) {
                support_weight *= 3;
            }
            if (unk_sum1) {
                support_weight *= 2;
                if (soc_eff.support <= -3) {
                    support_weight *= 2;
                }
                if ((soc_eff.support == -1 || soc_eff.support == -2) && soc_eff.economy < 2) {
                    support_weight += support_weight / 2;
                }
            }
            if (output) {
                if (power_val) {
                    support_weight *= 3;
                }
                if (growth_val) {
                    support_weight *= 2;
                }
                if (tech_val) {
                    support_weight /= 2;
                }
            } else {
                if (fight_val > 0) {
                    support_weight *= 2;
                }
                if (growth_val > 0) {
                    support_weight += support_weight / 2;
                }
                if (wealth_val > 0) {
                    support_weight /= 2;
                }
                if (tech_val > 0) {
                    support_weight += support_weight / -4;
                }
                if (power_val > 0) {
                    support_weight += support_weight / 2;
                }
            }
            unk_val6 -= support_weight;
            // morale
            int morale_val = range(soc_eff.morale, -4, 4) * 2;
            if (morale_val < -6) {
                morale_val++;
            }
            if (morale_val < -2) {
                morale_val++;
            }
            if (morale_val > 6) {
                morale_val--;
            }
            if (morale_val > 2) {
                morale_val--;
            }
            int morale_weight = (unk_val3 * morale_val * (fight_val + 2) * (unk_sum1 + 1)
                * ((int)PlayersData[faction_id].current_num_bases + PlayersData[faction_id].armed_unit_weight)) / 8;
            if (output) {
                if (power_val) {
                    morale_weight *= 3;
                }
                if (growth_val || wealth_val) {
                    morale_weight /= 2;
                }
            } else {
                if (fight_val > 1) {
                    morale_weight *= 2;
                }
                if (power_val && !growth_val && !wealth_val) {
                    morale_weight *= 2;
                }
                if (fight_val < 0 && !power_val && (wealth_val || tech_val) && !unk_sum1) {
                    morale_weight /= 2;
                }
            }
            unk_val6 += morale_weight;
            // efficiency
            int effic_weight = effic_val;
            if (soc_eff.efficiency == -3) {
                effic_weight = 2 * effic_val;
            } else if (soc_eff.efficiency <= -4) {
                effic_weight = 4 * effic_val;
            }
            if (!unk_sum1) {
                effic_weight *= 2;
            }
            if (output) {
                if (tech_val) {
                    effic_weight *= 3;
                }
                if (growth_val || power_val) {
                    effic_weight /= 2;
                }
            } else {
                if (!wealth_val && !tech_val) {
                    if (power_val) {
                        effic_weight /= 2;
                    }
                } else if (power_val) {
                    if (!wealth_val) {
                        effic_weight /= 2;
                    }
                } else if (!growth_val) {
                    effic_weight *= 2;
                }
            }
            unk_val6 -= effic_weight;
            // growth
            int growth_weight = (soc_eff.growth * (unk_count1 + 1) 
                * ((growth_val + 1) * 2 - wealth_val)
                * (pop_goal_growth + (int)PlayersData[faction_id].nutrient_surplus_total)) / 5;
            if (aah_ooga(faction_id, faction_id)) {
                growth_weight *= 2;
            }
            if (Players[faction_id].rule_population > 0) {
                growth_weight /= 2;
            }
            if (output) {
                if (growth_val || wealth_val) {
                    growth_weight *= 2;
                }
                if (tech_val) {
                    growth_weight /= 2;
                }
            } else {
                if (growth_val && power_val) {
                    growth_weight *= 2;
                }
            }
            unk_val6 += growth_weight;
            // police
            int police_weight 
                = range(soc_eff.police + 2, -2, 2) * (int)PlayersData[faction_id].current_num_bases 
                * (unk_count2 * 2 + 1);
            if (PlayersData[faction_id].tech_ranking 
                < PlayersData[*RankingFactionIDUnk2].tech_ranking
                && PlayersData[faction_id].ranking < PlayersData[*RankingFactionIDUnk2].ranking) {
                police_weight *= 2;
            }
            if (PlayersData[faction_id].tech_ranking
                >= PlayersData[*RankingFactionIDUnk1].tech_ranking
                && PlayersData[faction_id].ranking > PlayersData[*RankingFactionIDUnk1].ranking) {
                police_weight /= 2;
            }
            if (output && (power_val || growth_val)) {
                police_weight *= 2;
            }
            unk_val6 += police_weight / (2 - fight_val);
            // talent
            int talent_weight = PlayersData[faction_id].unk_39[range(soc_eff.talent + 4, 0, 7)]
                [range(soc_eff.police + 5, 0, 8)];
            if (unk_sum1) {
                talent_weight += soc_eff.police * PlayersData[faction_id].current_num_bases;
            }
            if (output) {
                if (power_val || growth_val) {
                    talent_weight *= 2;
                }
                if (wealth_val) {
                    talent_weight /= 2;
                }
            } else {
                if (growth_val && tech_val) {
                    talent_weight *= 2;
                }
                if (power_val && (tech_val || wealth_val)) {
                    talent_weight /= 2;
                }
            }
            unk_val6 += talent_weight;
            // planet
            int planet_weight = (4 - soc_eff.planet) * PlayersData[faction_id].unk_49 * 4 / 4; // ?
            if (PlayersData[faction_id].soc_effect_base.planet > 0
                || PlayersData[faction_id].best_psi_offense 
                > PlayersData[faction_id].best_weapon_value) {
                planet_weight += PlayersData[faction_id].total_mil_veh;
            } else if (unk_val3 > 1) {
                planet_weight /= 2;
            }
            if (output) {
                if (growth_val) {
                    planet_weight *= 2;
                }
                if (wealth_val) {
                    planet_weight /= 2;
                }
            } else if (!power_val) {
                if (growth_val) {
                    if (!tech_val) {
                        planet_weight *= 4;
                    }
                } else if (tech_val || wealth_val) {
                    planet_weight /= 2;
                }
            } else if (!growth_val) {
                planet_weight /= 2;
                if (tech_val || wealth_val) {
                    planet_weight /= 2;
                }
            }
            unk_val6 -= planet_weight;
            // research
            int research_weight = range(soc_eff.research, -5, 5)
                * PlayersData[faction_id].labs_total / 10;
            if (!unk_sum1) {
                research_weight *= 2;
            }
            if (output) {
                if (tech_val) {
                    research_weight *= 2;
                }
                if (power_val) {
                    research_weight /= 2;
                }
            }
            unk_val6 += research_weight;
            // industry
            int unk_val3_temp = range(unk_val3, 1, 2);
            int industry_weight = soc_eff.industry * unk_val3_temp
                * PlayersData[faction_id].pop_total * (2 * (wealth_val + unk_sum1) + 1) / 10;
            if (power_val) {
                industry_weight *= 2;
            }
            if (wealth_val) {
                industry_weight += industry_weight / 2;
            }
            if (tech_val) {
                industry_weight /= 2;
            }
            if (fight_val < 0 && !wealth_val) {
                industry_weight /= 2;
            }
            unk_val6 += industry_weight;
            // cost of switching to this combination (signed divide: the cost is a penalty)
            unk_val6 += (int)social_upheaval(faction_id, &soc_cat) / -3;
            // probe
            if (soc_eff.probe) {
                int probe_weight;
                for (uint32_t f = 1; f < MaxPlayerNum; f++) {
                    if (f != faction_id) {
                        probe_weight = PlayersData[f].mind_control_total / 4 + 1;
                        if (has_treaty(faction_id, f, DTREATY_PACT)) {
                            probe_weight /= 2;
                        }
                        if (!has_treaty(faction_id, f, DTREATY_WANT_REVENGE)) {
                            if (has_treaty(faction_id, f, DTREATY_TREATY)) {
                                probe_weight /= 2;
                            }
                            if (!has_treaty(faction_id, f, DTREATY_COMMLINK)) {
                                probe_weight /= 2;
                            }
                            if (!has_treaty(faction_id, f, DTREATY_VENDETTA)) {
                                probe_weight /= 2;
                            }
                        }
                        probe_weight = ((int)(PlayersData[f].current_num_bases
                            + PlayersData[faction_id].current_num_bases)
                            * range(soc_eff.probe, -2, 3) * probe_weight) / 2;
                        if (tech_val || wealth_val) {
                            probe_weight *= 2;
                        }
                        unk_val6 += probe_weight;
                    }
                }
            }
            if (unk_val6 >= unk_val4) {
                unk_val4 = unk_val6;
                soc_cat_bits = i; // the SE combination being evaluated
            }
        }
    }
    if (soc_cat_bits < 0) {
        if (output) {
            for (int i = 0; i < MaxSocialCatNum; i++) {
                *(&output->politics + i)
                    = *(&PlayersData[faction_id].soc_category_pending.politics + i);
            }
        }
    } else {
        if (!output) {
            BOOL no_cat_change = true;
            for (int i = 0; i < MaxSocialCatNum; i++) {
                uint32_t cat_bit = soc_cat_bits & 3;
                if (cat_bit != *(&PlayersData[faction_id].soc_category_pending.politics + i)) {
                    *(&PlayersData[faction_id].soc_category_pending.politics + i) = cat_bit;
                    no_cat_change = false;
                }
                soc_cat_bits >>= 2;
            }
            if (!no_cat_change) {
                social_set(faction_id);
                uint32_t cost 
                    = social_upheaval(faction_id, &PlayersData[faction_id].soc_category_pending);
                PlayersData[faction_id].energy_reserves 
                    = range(PlayersData[faction_id].energy_reserves - cost, 0, 999999999);
                PlayersData[faction_id].soc_upheaval_cost_paid += cost;
                if (!is_human(faction_id)) {
                    consider_designs(faction_id);
                }
            }
        } else {
            for (int i = 0; i < MaxSocialCatNum; i++) {
                *(&output->politics + i) = soc_cat_bits & 3;
                soc_cat_bits >>= 2;
            }
        }
    }
}

/*
Purpose: Calculate specified faction's best available weapon and armor ratings as well as the
         fastest moving ground Veh chassis. Compare these capabilities to faction's best opponent
         capabilities based on current diplomacy.
Original Offset: 00560DD0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl enemy_capabilities(uint32_t faction_id) {
    BOOL has_worms = veh_avail(BSC_MIND_WORMS, faction_id, -1);
    PlayersData[faction_id].best_psi_offense = has_worms 
        ? weap_strat(WPN_PSI_ATTACK, faction_id) : 0;
    PlayersData[faction_id].best_weapon_value = 1;
    for (int i = 0; i < MaxWeaponNum; i++) {
        if (has_tech(Weapon[i].preq_tech, faction_id) && Weapon[i].offense_rating < 99) {
            int weap_val = weap_strat(i, faction_id);
            if (Weapon[i].offense_rating < 0 && weap_val 
                > PlayersData[faction_id].best_psi_offense) {
                PlayersData[faction_id].best_psi_offense = weap_val;
            }
            if (weap_val > PlayersData[faction_id].best_weapon_value) {
                PlayersData[faction_id].best_weapon_value = weap_val;
            }
        }
    }
    PlayersData[faction_id].best_psi_defense = has_worms 
        ? arm_strat(ARM_PSI_DEFENSE, faction_id) : 0;
    PlayersData[faction_id].best_armor_value = 1;
    for (int i = 0; i < MaxArmorNum; i++) {
        if (has_tech(Armor[i].preq_tech, faction_id)) {
            int arm_val = arm_strat(i, faction_id);
            if (Armor[i].defense_rating < 0 && arm_val > PlayersData[faction_id].best_psi_defense) {
                PlayersData[faction_id].best_psi_defense = arm_val;
            }
            if (arm_val > PlayersData[faction_id].best_armor_value) {
                PlayersData[faction_id].best_armor_value = arm_val;
            }
        }
    }
    PlayersData[faction_id].best_land_speed = 1;
    for (int i = 0; i < MaxChassisNum; i++) {
        if (has_tech(Chassis[i].preq_tech, faction_id) && Chassis[i].triad == TRIAD_LAND) {
            if (Chassis[i].speed > PlayersData[faction_id].best_land_speed) {
                PlayersData[faction_id].best_land_speed = Chassis[i].speed;
            }
        }
    }
    PlayersData[faction_id].enemy_best_weapon_value = 0;
    PlayersData[faction_id].enemy_best_armor_value = 0;
    PlayersData[faction_id].enemy_best_land_speed = 0;
    PlayersData[faction_id].enemy_best_psi_offense = 0;
    PlayersData[faction_id].enemy_best_psi_defense = 0;
    for (int i = 0; i < 4 && !PlayersData[faction_id].enemy_best_weapon_value; i++) {
        // 1st pass: vendetta, no treaty, has commlink
        // 2nd pass: no treaty, has commlink
        // 3rd pass: has commlink
        // 4th pass: any non-pact faction
        for (uint32_t j = 1; j < MaxPlayerNum; j++) {
            if (j != faction_id
                && !has_treaty(faction_id, j, DTREATY_PACT)
                && ((!i && has_treaty(faction_id, j, DTREATY_VENDETTA) && !has_treaty(faction_id, j, DTREATY_TREATY)
                    && has_treaty(faction_id, j, DTREATY_COMMLINK)) 
                    || (i == 1 && !has_treaty(faction_id, j, DTREATY_TREATY) 
                        && has_treaty(faction_id, j, DTREATY_COMMLINK))
                    || (i == 2 && has_treaty(faction_id, j, DTREATY_COMMLINK)) || (i == 3))) {
                if (PlayersData[faction_id].enemy_best_weapon_value 
                    < PlayersData[j].best_weapon_value) {
                    PlayersData[faction_id].enemy_best_weapon_value 
                        = PlayersData[j].best_weapon_value;
                }
                if (PlayersData[faction_id].enemy_best_armor_value 
                    < PlayersData[j].best_armor_value) {
                    PlayersData[faction_id].enemy_best_armor_value 
                        = PlayersData[j].best_armor_value;
                }
                if (PlayersData[faction_id].enemy_best_land_speed 
                    < PlayersData[j].best_land_speed) {
                    PlayersData[faction_id].enemy_best_land_speed = PlayersData[j].best_land_speed;
                }
                if (PlayersData[faction_id].enemy_best_psi_offense 
                    < PlayersData[j].best_psi_offense) {
                    PlayersData[faction_id].enemy_best_psi_offense 
                        = PlayersData[j].best_psi_offense;
                }
                if (PlayersData[faction_id].enemy_best_psi_defense 
                    < PlayersData[j].best_psi_defense) {
                    PlayersData[faction_id].enemy_best_psi_defense 
                        = PlayersData[j].best_psi_defense;
                }
            }
        }
    }
}

/*
Purpose: The AI's strategic planning for a faction at the start of its turn. It recounts the
         faction's bases, units and strength (in total and per region), adds attack, defend,
         scout and landing goals for the enemy bases and sea formers it can see, upgrades or
         disbands obsolete units and stray scout patrols, decides each region's plan (attack,
         defend, expand, colonize), gathers units for a combined attack on an enemy region and
         finally sends each unit towards the best goal for its plan (the unit's order becomes
         move to and move_to_ai_type 0x31; 0x30 for an attack gathering at the target staging
         base). Human factions only get the counts, the per-region plans and the goals of
         automated units.
Original Offset: 00561080
Return Value: n/a
Status: Complete
*/
void __cdecl enemy_strategy(uint32_t faction_id) {
    PlayerData *p = &PlayersData[faction_id];
    uint8_t faction_bit = (uint8_t)(1 << faction_id);
    int transport_count = 0;
    int transport_cargo = 2; // largest cargo capacity of the faction's transports, at least 2
    int attack_count = 0; // units sent to attack, or gathering for one outside the target region
    p->total_mil_veh = 0;
    p->current_num_bases = 0;
    p->mil_strength_1 = 0;
    p->mil_strength_2 = 0;
    p->pop_total = 0;
    p->unk_70 = 0;
    p->planet_busters = 0;
    p->unk_71 = 0;
    p->unk_72 = 0;
    for (uint32_t region = 0; region < MaxContinentNum; region++) {
        p->region_total_combat_vehs[region] = 0;
        p->region_total_bases[region] = 0;
        p->region_force_rating[region] = 0;
        // only flag 0x8 survives, and it's cleared too every 16 turns (staggered by region)
        p->unk_77[region] &= ((*TurnCurrentNum + faction_id + region) & 0xF) ? 8 : 0;
    }
    for (int i = 0; i < MaxVehProtoNum; i++) {
        p->proto_id_active[i] = 0;
        p->proto_id_queue[i] = 0;
    }
    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
        Base *base = &Bases[base_id];
        int x = base->x;
        int y = base->y;
        uint32_t owner = base->faction_id_current;
        if (owner == faction_id) {
            base->state &= ~(BSTATE_UNK_4000000 | BSTATE_UNK_8000000);
            if (base->unk_1) {
                base->unk_1--;
            }
            p->current_num_bases++;
            p->pop_total += base->population_size;
            int veh_id = stack_fix(veh_at(x, y));
            if (!stack_check(veh_id, 14, faction_id, -1, -1)
                && !stack_check(veh_id, 2, PLAN_RECONNAISANCE, faction_id, -1)) {
                add_goal(faction_id, AI_GOAL_DEFEND, 4, x, y, -1);
            }
            if ((int)base->queue_size >= 0) {
                for (int i = 0; i <= (int)base->queue_size; i++) {
                    int item_id = base->queue_production_id[i];
                    if (item_id >= 0) {
                        p->proto_id_queue[item_id]++;
                    }
                }
            }
            uint32_t region = region_at(x, y);
            p->region_total_bases[region]++;
            if (is_coast(base->x, base->y, false)) {
                p->unk_77[region] |= 0x80;
            }
            if (!((*TurnCurrentNum + base_id) & 3)) {
                base->event &= 0xFFFFFF00;
            }
        } else if (base->visibility & faction_bit) {
            uint32_t treaty = p->diplo_treaties[owner];
            if (treaty & (DTREATY_VENDETTA | DTREATY_SHALL_BETRAY | DTREATY_UNK_800)
                && !(treaty & DTREATY_PACT)) {
                int priority = 1;
                if (veh_who(x, y) < 0) { // no unit guarding the base
                    priority = 2;
                    add_goal(faction_id, AI_GOAL_DEFEND, 2, x, y, -1);
                    add_goal(faction_id, AI_GOAL_SCOUT, 2, x, y, -1);
                }
                if (base->faction_id_former == faction_id) {
                    priority++;
                }
                if (p->region_total_bases[region_at(x, y)]) {
                    priority++;
                }
                uint32_t offset;
                uint32_t mask;
                bitmask(FAC_PERIMETER_DEFENSE, &offset, &mask);
                if (!(base->facilities_built[offset] & mask)) {
                    priority++;
                }
                if (base->queue_production_id[0] == -FAC_ASCENT_TO_TRANSCENDENCE) {
                    priority *= 4;
                }
                bitmask(FAC_HEADQUARTERS, &offset, &mask);
                if (base->facilities_built[offset] & mask) {
                    priority++;
                    if (p->flags & PFLAG_STRAT_ATK_ENEMY_HQ) {
                        priority *= 2;
                    }
                    if (PlayersData[owner].corner_market_turn > *TurnCurrentNum) {
                        priority *= 8;
                    }
                }
                if (p->base_id_atk_target == base_id && p->diplo_treaties[owner] & DTREATY_VENDETTA) {
                    priority *= 4;
                } else if (is_objective(base_id)) {
                    priority *= 4;
                }
                add_goal(faction_id, AI_GOAL_ATTACK, priority, x, y, -1);
                int x_unk = base->unk_x;
                int y_unk = base->unk_y;
                if (y_unk >= 0 && y_unk < *MapLatitudeBounds && x_unk >= 0
                    && x_unk < *MapLongitudeBounds) {
                    int base_id_find = base_find(x_unk, y_unk);
                    if (base_id_find >= 0
                        && Bases[base_id_find].faction_id_current == base->faction_id_current
                        && (int)terraform_cost(base->unk_x, base->unk_y, faction_id)
                        <= p->energy_reserves / 8) {
                        add_goal(faction_id, AI_GOAL_LANDING_SITE, 2, base->unk_x, base->unk_y, -1);
                    }
                }
            }
        }
    }
    if (!(*FactionsStatus & faction_bit)) {
        for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
            if (Bases[base_id].faction_id_current == faction_id
                && Bases[base_id].queue_production_id[0] == -FAC_STOCKPILE_ENERGY) {
                base_reset(base_id, false);
            }
        }
    }
    int region_formers[MaxContinentNum]; // land terraformers per region
    memset(region_formers, 0, sizeof(region_formers));
    enemy_capabilities(faction_id);
    p->unk_68 = 0;
    p->unk_66 = 0;
    p->unk_65 = 0;
    p->unk_64 = 0;
    p->unk_106 = 0;
    p->unk_105 = 0;
    for (int veh_id = *VehCurrentCount - 1; veh_id >= 0; veh_id--) {
        if (veh_id >= *VehCurrentCount) {
            continue;
        }
        Veh *veh = &Vehs[veh_id];
        if (veh->faction_id != faction_id) {
            uint32_t treaty = p->diplo_treaties[veh->faction_id];
            if (!(treaty & DTREATY_COMMLINK) || (treaty & (DTREATY_PACT | DTREATY_TREATY)
                && !(treaty & (DTREATY_WANT_REVENGE | DTREATY_SHALL_BETRAY | DTREATY_UNK_800)))) {
                continue;
            }
            int proto_id = veh->proto_id;
            uint8_t plan = VehPrototypes[proto_id].plan;
            if (plan == PLAN_NAVAL_TRANSPORT) {
                p->unk_64++;
            }
            if (plan == PLAN_NAVAL_SUPERIORITY) {
                p->unk_65 += VehPrototypes[proto_id].reactor_id;
            }
            uint8_t chassis_id = VehPrototypes[proto_id].chassis_id;
            if (Chassis[chassis_id].triad == TRIAD_AIR) {
                if (plan == PLAN_OFFENSIVE && !Chassis[chassis_id].missile
                    && weap_val(proto_id, faction_id) >= p->enemy_best_armor_value) {
                    p->unk_66++;
                }
                if (Chassis[VehPrototypes[veh->proto_id].chassis_id].missile
                    && !planet_buster(veh_id)) {
                    p->unk_68++;
                }
            }
            // sea formers outside a base: a goal to hunt them, if they can be seen (the own faction
            // test can never pass here)
            if (VehPrototypes[veh->proto_id].plan == PLAN_TERRAFORMING
                && Chassis[VehPrototypes[veh->proto_id].chassis_id].triad == TRIAD_SEA
                && altitude_at(veh->x, veh->y) < ALT_BIT_SHORE_LINE && base_who(veh->x, veh->y) < 0
                && ((veh->faction_id == faction_id
                    && (veh->flags & (VFLAG_LURKER | VFLAG_INVISIBLE))
                    != (VFLAG_LURKER | VFLAG_INVISIBLE)) || veh->visibility & faction_bit)) {
                add_goal(faction_id, AI_GOAL_UNK_1, (p->flags & PFLAG_UNK_80000) ? 4 : 2,
                    veh->x, veh->y, -1);
            }
            continue;
        }
        int x = veh->x;
        int y = veh->y;
        int proto_id = veh->proto_id;
        uint32_t region = region_at(x, y);
        BOOL is_ocean_tile = altitude_at(x, y) < ALT_BIT_SHORE_LINE;
        uint32_t plan = VehPrototypes[proto_id].plan;
        p->proto_id_active[proto_id]++;
        uint32_t triad = Chassis[VehPrototypes[proto_id].chassis_id].triad;
        if (veh->year_end_lurking && !--veh->year_end_lurking) {
            veh->flags &= ~(VFLAG_LURKER | VFLAG_INVISIBLE);
        }
        // obsolete design the Console couldn't upgrade: disband it if a better active design of the
        // same plan and triad exists (or always, outside the combat plans), unless it's needed where
        // it stands; every 4th turn per unit, after turn 50
        if (!(*FactionsStatus & faction_bit)
            && VehPrototypes[veh->proto_id].obsolete_factions & faction_bit
            && !Console_upgrade(ConsoleVar, 0, veh_id)
            && !((*TurnCurrentNum + veh_id) & 3) && *TurnCurrentNum > 50
            && veh->move_to_ai_type != 0x46 && veh->move_to_ai_type != 0x31
            && veh->move_to_ai_type != 0x32) {
            uint8_t plan_now = VehPrototypes[veh->proto_id].plan;
            if ((plan_now != PLAN_COMBAT || p->region_base_plan[region] > 2)
                && plan_now < PLAN_COLONIZATION && plan_now != p->region_base_plan[region]
                && (plan_now != PLAN_RECONNAISANCE || p->region_base_plan[region] != 8
                    || veh->order == ORDER_HOLD)
                && (base_who(x, y) < 0 || stack_check(veh_id, 1, faction_id, -1, -1) > 1)
                && altitude_at(x, y) >= ALT_BIT_SHORE_LINE && p->region_base_plan[region] != 2
                && !zoc_sea(x, y, faction_id)) {
                BOOL replaced = false;
                plan_now = VehPrototypes[veh->proto_id].plan;
                if (plan_now <= PLAN_COMBAT) {
                    for (int i = 0; i < 64; i++) {
                        int proto_id_new = faction_id * 64 + i;
                        VehPrototype *proto = &VehPrototypes[proto_id_new];
                        if (proto->flags & PROTO_ACTIVE && !(proto->obsolete_factions & faction_bit)
                            && proto->plan == VehPrototypes[veh->proto_id].plan
                            && Chassis[proto->chassis_id].triad
                            == Chassis[VehPrototypes[veh->proto_id].chassis_id].triad) {
                            int value_new = weap_val(proto_id_new, faction_id);
                            int value_old = weap_val(proto_id, faction_id);
                            if (value_new > value_old) {
                                replaced = true;
                            }
                        }
                    }
                } else if (plan_now == PLAN_DEFENSIVE) {
                    for (int i = 0; i < 64; i++) {
                        int proto_id_new = faction_id * 64 + i;
                        VehPrototype *proto = &VehPrototypes[proto_id_new];
                        if (proto->flags & PROTO_ACTIVE && !(proto->obsolete_factions & faction_bit)
                            && proto->plan == VehPrototypes[veh->proto_id].plan
                            && Chassis[proto->chassis_id].triad
                            == Chassis[VehPrototypes[veh->proto_id].chassis_id].triad) {
                            // vanilla passes prototype ids to arm_val(), which takes an armor id
                            int value_old = arm_val(proto_id, faction_id);
                            int value_new = arm_val(proto_id_new, faction_id);
                            if (value_new > value_old) {
                                replaced = true;
                            }
                        }
                    }
                } else {
                    replaced = true;
                }
                if (replaced) {
                    if (!(veh->flags & VFLAG_IS_OBJECTIVE)) {
                        kill(veh_id);
                    }
                    continue;
                }
            }
        }
        // scout patrols are disbanded after turn 25 in expansion regions or when holding, unless
        // they're the base's only defender (3 turns out of 4 per unit)
        if (!(*FactionsStatus & faction_bit) && (*TurnCurrentNum + veh_id) & 3
            && *TurnCurrentNum > 25 && proto_id == BSC_SCOUT_PATROL
            && (p->region_base_plan[region] == 7 || veh->order == ORDER_HOLD)
            && (base_who(x, y) < 0 || stack_check(veh_id, 14, faction_id, -1, -1) > 1)
            && altitude_at(x, y) >= ALT_BIT_SHORE_LINE && p->region_base_plan[region] != 2
            && !zoc_sea(x, y, faction_id) && veh->move_to_ai_type != 0x31) {
            kill(veh_id);
            continue;
        }
        veh->state &= ~(VSTATE_UNK_4 | VSTATE_MADE_AIRDROP | VSTATE_UNK_10000000
            | VSTATE_UNK_20000000);
        if (!(*FactionsStatus & faction_bit) && veh->order == ORDER_SENTRY_BOARD
            && (triad != TRIAD_LAND || !is_ocean_tile)) {
            veh->order = ORDER_NONE;
        }
        if (is_ocean_tile) {
            // land unit at sea outside a base: dies without a transport, otherwise sleeps aboard
            if (triad == TRIAD_LAND && base_who(x, y) < 0) {
                if (!stack_check(veh_id, 2, PLAN_NAVAL_TRANSPORT, -1, -1)) {
                    if (!(veh->flags & VFLAG_IS_OBJECTIVE)) {
                        veh_kill(veh_id);
                    }
                    continue;
                }
                if (veh->order != ORDER_SENTRY_BOARD) {
                    sleep(veh_id);
                }
            }
        } else if (triad == TRIAD_SEA && base_who(x, y) < 0) { // sea unit on land outside a base
            if (!(veh->flags & VFLAG_IS_OBJECTIVE)) {
                veh_kill(veh_id);
            }
            continue;
        }
        if (plan <= PLAN_COMBAT && !(veh->state & VSTATE_UNK_40000)) {
            int base_id = base_at(veh->x, veh->y);
            if (base_id < 0 && veh->move_to_ai_type == 0x46) {
                base_id = veh->home_base_id;
            }
            if (base_id >= 0) { // a base short of garrison keeps the unit (0x10000000)
                int garrison = garrison_check(base_id);
                int spare = garrison_check(base_id);
                if (spare > 5) {
                    spare--;
                }
                if (spare > 2) {
                    spare--;
                }
                if (garrison > stack_check(veh_at(Bases[base_id].x, Bases[base_id].y), 11,
                    VSTATE_UNK_10000000, VSTATE_UNK_10000000, faction_id) + spare) {
                    veh->state |= VSTATE_UNK_10000000;
                }
            }
        }
        BOOL is_military = false;
        if (veh->move_to_ai_type == 0x46 && plan == PLAN_DEFENSIVE) {
            if (veh->home_base_id >= 0) {
                Base *home = &Bases[veh->home_base_id];
                if (home->x != veh->x || home->y != veh->y) {
                    home->state |= BSTATE_UNK_4000000;
                }
            }
            is_military = true;
        } else if (plan == PLAN_TERRAFORMING) {
            veh->flags &= ~0x800;
            region_formers[region]++;
            int16_t home_base_id = veh->home_base_id;
            if (home_base_id >= 0) {
                // one former per base and triad (0x20 land, 0x80000 sea): a second one moves home
                // to the nearest base in the region without one, mineral surplus below 2 (any
                // faction's base: vanilla doesn't check)
                uint32_t home_flag = (triad != TRIAD_SEA) ? BSTATE_UNK_20 : BSTATE_UNK_80000;
                if (Bases[home_base_id].state & home_flag && !(*FactionsStatus & faction_bit)) {
                    int dist_best = 9999;
                    int base_id_best = -1;
                    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
                        if (base_id != veh->home_base_id
                            && region_at(Bases[base_id].x, Bases[base_id].y) == region
                            && !(Bases[base_id].state & home_flag)
                            && Bases[base_id].mineral_surplus < 2) {
                            int dist = vector_dist(veh->x, veh->y, Bases[base_id].x, Bases[base_id].y);
                            if (dist < dist_best) {
                                dist_best = dist;
                                base_id_best = base_id;
                            }
                        }
                    }
                    if (base_id_best >= 0) {
                        Bases[base_id_best].state |= home_flag;
                        veh->home_base_id = (int16_t)base_id_best;
                    }
                } else {
                    Bases[home_base_id].state |= home_flag;
                }
            }
        } else if (plan == PLAN_COLONIZATION) {
            if (p->unk_77[region] & 2) {
                p->unk_77[region] |= 0x800;
            } else {
                p->unk_77[region] |= 2;
            }
        } else if (plan == PLAN_INFO_WARFARE) {
            p->unk_77[region] |= 0x1000;
        } else if (plan == PLAN_RECONNAISANCE) {
            if (base_who(x, y) < 0) {
                p->unk_77[region] |= 0x2000;
            }
            is_military = true;
        } else if (plan <= PLAN_NAVAL_TRANSPORT) {
            is_military = true;
        }
        if (is_military) {
            p->total_mil_veh++;
        }
        if (is_military && triad == TRIAD_LAND) {
            if (!is_ocean_tile) {
                p->region_total_combat_vehs[region]++;
            }
        } else if (triad == TRIAD_SEA) {
            p->unk_70++;
        } else if (triad == TRIAD_AIR) {
            if (plan == PLAN_OFFENSIVE) {
                p->unk_105++;
            } else if (plan == PLAN_AIR_SUPERIORITY) {
                p->unk_106++;
            }
        }
        if (plan == PLAN_NAVAL_TRANSPORT) {
            transport_count++;
            if (transport_cargo <= (int)veh_cargo(veh_id)) {
                transport_cargo = veh_cargo(veh_id);
            }
        }
        if (planet_buster(veh_id)) {
            p->planet_busters++;
            continue;
        }
        int weap = weap_strat(VehPrototypes[veh->proto_id].weapon_id, faction_id);
        int arm = arm_strat(VehPrototypes[veh->proto_id].armor_id, faction_id);
        int morale = morale_veh(veh_id, false, 0) + 6;
        p->mil_strength_1 += (arm + weap) * morale / 8;
        if (weap > arm || veh->order != ORDER_HOLD) {
            int strength = morale * weap / 8;
            p->mil_strength_2 += strength;
            p->region_force_rating[region] += (uint16_t)strength;
        }
        uint32_t state = veh->state;
        if ((state & (VSTATE_UNK_20000 | VSTATE_UNK_40000)) == VSTATE_UNK_40000) { // gathering
            int x_target = p->x_target;
            int y_target = p->y_target;
            if (y_target >= 0 && y_target < *MapLatitudeBounds && x_target >= 0
                && x_target < *MapLongitudeBounds && region == region_at(x_target, y_target)) {
                p->unk_71++;
                if (plan == PLAN_INFO_WARFARE) {
                    p->unk_72 |= 2;
                } else if (arm > weap) {
                    p->unk_72 |= 1;
                }
            } else {
                veh->state = state | VSTATE_UNK_20000;
                attack_count++;
            }
        } else if ((state & (VSTATE_UNK_20000 | VSTATE_UNK_40000))
            == (VSTATE_UNK_20000 | VSTATE_UNK_40000)) { // attacking
            attack_count++;
        }
    }
    p->unk_70 += ((transport_cargo << 11) + transport_count) << 11;
    int enemy_sea = 0;
    for (uint32_t i = 1; i < MaxPlayerNum; i++) {
        if (i != faction_id && p->diplo_treaties[i] & DTREATY_COMMLINK
            && !(p->diplo_treaties[i] & DTREATY_PACT)) {
            int value = PlayersData[i].unk_70 / 0x400000;
            if (enemy_sea <= value) {
                enemy_sea = value;
            }
        }
    }
    p->unk_64 += enemy_sea << 11;
    if (transport_count > 1) {
        for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
            if (Vehs[veh_id].faction_id == faction_id) {
                int16_t proto_id = Vehs[veh_id].proto_id;
                if (Chassis[VehPrototypes[proto_id].chassis_id].triad == TRIAD_SEA
                    && VehPrototypes[proto_id].plan == PLAN_NAVAL_TRANSPORT
                    && (proto_id == BSC_UNITY_FOIL || p->proto_id_active[proto_id] >= 2)) {
                    Vehs[veh_id].state |= VSTATE_UNK_20000000;
                    break;
                }
            }
        }
    }
    for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
        Veh *veh = &Vehs[veh_id];
        if (veh->faction_id == faction_id
            && VehPrototypes[veh->proto_id].plan == PLAN_TERRAFORMING) {
            uint32_t region = region_at(veh->x, veh->y);
            if (region < MaxRegionLandNum && region_formers[region] >= 3
                && p->region_total_bases[region] >= 3) {
                veh->flags |= 0x800;
                if (veh->home_base_id >= 0) {
                    Bases[veh->home_base_id].state &= ~BSTATE_UNK_20;
                }
                region_formers[region] = 0;
            }
        }
    }
    // the attack being gathered at the staging base (x_target, y_target): launch it once enough
    // units are there, or give it up
    int launch = *FactionsStatus & faction_bit; // humans never gather
    int region_target = -1;
    BOOL end_gathering = false;
    if (p->x_target >= 0) {
        int base_id = base_at(p->x_target, p->y_target);
        region_target = region_at(p->x_target, p->y_target);
        if (base_id >= 0 && Bases[base_id].faction_id_current == faction_id) {
            int veh_id = stack_fix(veh_at(Bases[base_id].x, Bases[base_id].y));
            if (veh_id >= 0) {
                int gathered = stack_check(veh_id, 11, VSTATE_UNK_20000 | VSTATE_UNK_40000,
                    VSTATE_UNK_40000, -1);
                int needed = range(p->region_total_bases[region_target] * 2 / 3, 3, 8);
                if (gathered - (int)bit_count(p->unk_72) >= needed) {
                    launch = 1;
                }
                if (p->region_base_plan[region_target] == 2
                    || p->unk_77[region_target] & 0x4200) {
                    launch = 1;
                }
                if (p->region_base_plan[region_target] > 2
                    && stack_check(veh_id, 2, PLAN_NAVAL_TRANSPORT, -1, -1)) {
                    if (gathered >= p->unk_70 / 0x400000 * 2) {
                        launch = 1;
                    }
                    if (stack_check(veh_id, 2, PLAN_NAVAL_TRANSPORT, -1, -1) > 1) {
                        int cargo_free = stack_check(veh_id, 8, -1, -1, -1);
                        int land_units = stack_check(veh_id, 3, TRIAD_LAND, -1, -1);
                        if (gathered >= cargo_free + land_units) { // as many as the transports hold
                            launch = 1;
                        }
                    }
                }
            }
            if (launch) {
                end_gathering = true;
            } else if (p->region_base_plan[region_target] <= 2) {
                p->flags &= ~0x800;
            } else if (p->region_base_plan[region_target] != 8
                && is_coast(Bases[base_id].x, Bases[base_id].y, false)) {
                p->flags |= 0x800;
            } else {
                launch = 1;
                end_gathering = true;
            }
        } else {
            launch = 1;
            end_gathering = true;
        }
    } else if (launch) {
        end_gathering = true;
    }
    if (end_gathering) {
        for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
            if (Vehs[veh_id].faction_id == faction_id && Vehs[veh_id].state & VSTATE_UNK_40000) {
                Vehs[veh_id].state |= VSTATE_UNK_20000;
            }
        }
        p->y_target = -1;
        p->x_target = -1;
        p->unk_72 = 0;
        p->unk_71 = 0;
    }
    // region plans: 0 attack, 2 defend, 7 expand, 8 hold
    int target_value_best = 0;
    int region_target_best = -1;
    for (uint32_t region = 1; region < MaxContinentNum; region++) {
        p->region_total_offensive_vehs[region] = 0;
        if (((region & RegionBounds) == RegionBounds || !(region & RegionBounds))
            && !p->region_total_bases[region]) {
            p->region_base_plan[region] = 8;
            continue;
        }
        uint8_t base_plan_old = p->region_base_plan[region];
        uint32_t threat = 0;
        int weaker_count = 0; // hostile factions in the region we aren't stronger than
        int stronger_count = 0;
        int factions_with_bases = 0;
        int reachable_count = 0;
        int adjacent_count = 0;
        BOOL at_war = false;
        int our_combat = p->region_total_combat_vehs[region];
        int our_force = p->region_force_rating[region];
        int our_bases = p->region_total_bases[region];
        for (uint32_t i = 1; i < MaxPlayerNum; i++) {
            int their_force = PlayersData[i].region_force_rating[region];
            int their_combat = PlayersData[i].region_total_combat_vehs[region];
            int their_bases = PlayersData[i].region_total_bases[region];
            if (p->diplo_treaties[i] & DTREATY_PACT) {
                their_combat *= 2;
                their_bases *= 2;
            }
            if (their_bases) {
                factions_with_bases++;
                p->unk_77[region] |= (faction_id != i) ? 0x20 : 0x10;
                if (p->diplo_treaties[i] & DTREATY_VENDETTA) {
                    p->unk_77[region] |= 0x40;
                }
            } else if (!their_combat || our_force > their_force * 5) {
                continue;
            }
            if (faction_id == i) {
                continue;
            }
            uint32_t treaty = p->diplo_treaties[i];
            if (treaty & DTREATY_PACT || !(treaty & DTREATY_COMMLINK)) {
                continue;
            }
            // contiguous() is costly on big regions: there it's only redone every 8th/16th turn
            // (staggered by faction), in between bases in a region not planned for attack count
            // as unreachable, anything else as adjacent
            BOOL adjacent = false;
            BOOL reachable = false;
            BOOL test_contiguous = true;
            if (*TurnCurrentNum > 1) {
                int tiles = Continents[region].tile_count;
                if (tiles >= 500 || PlayersData[i].region_total_bases[region] * our_bases >= 50) {
                    int skip = (tiles < 1000) ? ((*TurnCurrentNum + faction_id) & 7)
                        : ((*TurnCurrentNum + faction_id * 2) & 0xF);
                    if (skip) {
                        test_contiguous = false;
                        if (their_bases && p->region_base_plan[region] > 2) {
                            reachable = false;
                            adjacent = false;
                        } else {
                            reachable = true;
                            adjacent = true;
                        }
                    }
                }
            }
            if (test_contiguous) {
                if (!their_bases || contiguous(faction_id, i, region, 0)) {
                    adjacent = true;
                    reachable = true;
                } else {
                    adjacent = false;
                    reachable = contiguous(faction_id, i, region, 1) != 0;
                }
            }
            if (p->diplo_treaties[i] & (DTREATY_VENDETTA | DTREATY_SHALL_BETRAY | DTREATY_UNK_800)) {
                if (reachable) {
                    reachable_count++;
                }
                if (adjacent && ++adjacent_count > 1) {
                    p->unk_77[region] |= 0x200;
                }
                if ((our_force > their_combat && our_force > their_force)
                    || our_combat > their_force || !our_bases || !reachable) {
                    stronger_count++;
                } else {
                    weaker_count++;
                }
                if (their_force >= our_force * 3 / 2 && our_bases > their_bases && reachable) {
                    p->unk_77[region] |= 0x4000;
                }
                if (*FactionsStatus & (1 << i) && (adjacent || reachable)) {
                    for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
                        if (Bases[base_id].faction_id_current == i
                            && Bases[base_id].faction_id_former == faction_id
                            && region_at(Bases[base_id].x, Bases[base_id].y) == region) {
                            p->unk_77[region] |= 0x400; // a human holds one of our former bases
                            break;
                        }
                    }
                }
            } else if (our_bases && p->diplo_treaties[i] & DTREATY_WANT_REVENGE
                && their_force > our_combat) {
                threat |= ((our_force > their_combat && their_bases && their_force < our_force * 2)
                    || !reachable) ? 2 : 1;
            }
            if (!(p->diplo_treaties[i] & (DTREATY_PACT | DTREATY_TREATY))
                || ((wants_to_attack(faction_id, i, 0) || wants_to_attack(i, faction_id, 0))
                    && reachable)) {
                at_war = true;
            }
        }
        uint8_t best_site = p->region_best_site[region];
        int threshold = great_satan(FactionRankings[7], false);
        threshold += (best_site < 4) - (int)((*GameState >> 7) & 2);
        threshold += ((int)p->current_num_bases > 4);
        threshold += 3 + (*TurnCurrentNum > 75) + ((int)*MapArea > 3200) + factions_with_bases;
        BOOL expand = false;
        if (our_bases) {
            int spare_tiles = p->region_good_tiles[region] - p->region_good_radius_tiles[region];
            expand = (best_site < 2 && p->region_total_bases[region]) || p->unk_77[region] & 8
                || our_bases >= 20 || spare_tiles < 4;
            // vanilla adds the aliens' (faction 0) good tile count, not this faction's twice
            if (((p->ai_growth - p->ai_wealth) + threshold) * our_bases * 2
                > PlayersData[0].region_good_tiles[region] + p->region_good_tiles[region]
                && spare_tiles / ((our_bases + 1) / 2) + best_site < threshold * 2) {
                expand = true;
            }
        }
        if (at_war) {
            p->unk_77[region] |= 4;
        }
        if (expand) {
            p->unk_77[region] |= 1;
        }
        p->region_base_plan[region] = expand ? 7 : 8;
        if (!reachable_count && p->region_total_bases[region]) {
            p->unk_77[region] |= 0x100;
        } else {
            if (stronger_count) {
                threat = 0;
                p->region_base_plan[region] = 0;
            } else if (threat & 2) {
                p->region_base_plan[region] = 0;
            }
            if (weaker_count || threat & 1) {
                p->region_base_plan[region] = 2;
            }
        }
        if (region >= MaxRegionLandNum) {
            p->region_base_plan[region] = 7;
            continue;
        }
        if (!launch && (int)region == region_target && adjacent_count >= 2) {
            for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
                if (Vehs[veh_id].faction_id == faction_id && Vehs[veh_id].state & VSTATE_UNK_40000) {
                    Vehs[veh_id].state |= VSTATE_UNK_20000;
                }
            }
            p->y_target = -1;
            p->x_target = -1;
            p->unk_72 = 0;
            p->unk_71 = 0;
            region_target = -1;
        }
        if (p->x_target < 0 && !launch && !(*FactionsStatus & faction_bit)) {
            if (p->region_base_plan[region] < 2 && adjacent_count < 2
                && !(p->unk_77[region] & 0x4000)) {
                int value = p->region_total_bases[region] + 1000;
                if (value > target_value_best) {
                    target_value_best = value;
                    region_target_best = region;
                    int base_id_atk = p->base_id_atk_target;
                    if (base_id_atk >= 0
                        && p->diplo_treaties[Bases[base_id_atk].faction_id_current] & DTREATY_VENDETTA
                        && region_at(Bases[base_id_atk].x, Bases[base_id_atk].y) == region) {
                        target_value_best = 0;
                    }
                }
            } else if (p->region_base_plan[region] == 7
                && p->region_total_bases[region] > target_value_best) {
                target_value_best = p->region_total_bases[region];
                region_target_best = region;
            }
        }
        // landing site: on the abstract map (5x5 tiles per cell), the coast tile at the end of a
        // land walk out of the region that borders the most other cells
        if ((!(*FactionsStatus & faction_bit) || (faction_id == (uint32_t)*FactionIDCurrent
            && *GamePreferences & PREF_AUTO_FORMER_RAISE_LWR_TERRAIN))
            && p->region_total_bases[region] && (p->region_base_plan[region] == 7
                || (p->region_base_plan[region] == 8 && (int)Continents[region].tile_count < 150))) {
            int score_best = 0;
            int x_best = 0;
            int y_best = 0;
            int x_site = 0;
            int y_site = 0;
            for (int y_abs = 0; y_abs < (int)*MapAbstractLatBounds; y_abs++) {
                for (int x_abs = y_abs & 1; x_abs < (int)*MapAbstractLongBounds; x_abs += 2) {
                    if (abstract_at(x_abs, y_abs) != region) {
                        continue;
                    }
                    int score = 0;
                    int dir_best = -1;
                    for (int dir = 0; dir < 8; dir++) {
                        int x_abs2 = x_abs + RadiusBaseX[dir];
                        if (!(*MapIsFlat & 1)) {
                            if (x_abs2 < 0) {
                                x_abs2 += *MapAbstractLongBounds;
                            } else if (x_abs2 >= (int)*MapAbstractLongBounds) {
                                x_abs2 -= *MapAbstractLongBounds;
                            }
                        }
                        int y_abs2 = y_abs + RadiusBaseY[dir];
                        if (x_abs2 < 0 || x_abs2 >= (int)*MapAbstractLongBounds || y_abs2 < 0
                            || y_abs2 >= (int)*MapAbstractLatBounds) {
                            continue;
                        }
                        uint32_t region_adj = abstract_at(x_abs2, y_abs2);
                        if (region_adj == region) {
                            score--;
                            continue;
                        }
                        score += 2;
                        BOOL is_flat = *MapIsFlat & 1;
                        for (int i = 0; i < 8; i++) {
                            int x_walk = x_abs * 5 + RadiusBaseX[i];
                            if (!is_flat) {
                                if (x_walk < 0) {
                                    x_walk += *MapLongitudeBounds;
                                } else if (x_walk >= *MapLongitudeBounds) {
                                    x_walk -= *MapLongitudeBounds;
                                }
                            }
                            int y_walk = y_abs * 5 + RadiusBaseY[i];
                            if (y_walk < 0 || y_walk >= *MapLatitudeBounds || x_walk < 0
                                || x_walk >= *MapLongitudeBounds
                                || altitude_at(x_walk, y_walk) < ALT_BIT_SHORE_LINE) {
                                continue;
                            }
                            // walk over land in direction dir up to the coast (or the map edge)
                            int steps = 1;
                            for (;;) {
                                int x_next = x_walk + RadiusBaseX[dir];
                                if (!is_flat) {
                                    if (x_next < 0) {
                                        x_next += *MapLongitudeBounds;
                                    } else if (x_next >= *MapLongitudeBounds) {
                                        x_next -= *MapLongitudeBounds;
                                    }
                                }
                                int y_next = y_walk + RadiusBaseY[dir];
                                if (y_next < 0 || y_next >= *MapLatitudeBounds || x_next < 0
                                    || x_next >= *MapLongitudeBounds
                                    || altitude_at(x_next, y_next) < ALT_BIT_SHORE_LINE) {
                                    break;
                                }
                                x_walk = x_next;
                                y_walk = y_next;
                                if (++steps >= 100) {
                                    break;
                                }
                            }
                            if (steps >= 100) {
                                continue;
                            }
                            if (base_who(x_walk, y_walk) < 0) {
                                dir_best = dir;
                                score += 4;
                                x_site = x_walk;
                                y_site = y_walk;
                                if (region_adj && (int)Continents[region_adj].tile_count >= 40) {
                                    score += 2;
                                }
                            }
                            break; // first land tile only (next direction even if a base is there)
                        }
                    }
                    if (score > score_best && dir_best >= 0) {
                        score_best = score;
                        x_best = x_site;
                        y_best = y_site;
                    }
                }
            }
            if (score_best > 0
                && (int)terraform_cost(x_best, y_best, faction_id) <= p->energy_reserves / 8) {
                add_goal(faction_id, AI_GOAL_LANDING_SITE, 2, x_best, y_best, -1);
            }
        }
        if (!(*FactionsStatus & faction_bit)) {
            bases_reset(region, faction_id, p->region_base_plan[region] == base_plan_old);
        }
    }
    // a new attack: on the enemy with the most bases in the chosen region (vendetta first), from the
    // own base there nearest to the enemy
    if (region_target_best > 0 && region_target_best < RegionBounds && !launch
        && !(*FactionsStatus & faction_bit) && !(p->flags & PFLAG_STRAT_SEARCH_OBJECTIVES)) {
        int value_best = -1;
        int faction_id_target = -1;
        for (uint32_t i = 1; i < MaxPlayerNum; i++) {
            if (i == faction_id) {
                continue;
            }
            uint32_t treaty = p->diplo_treaties[i];
            if (!(treaty & (DTREATY_TRUCE | DTREATY_VENDETTA | DTREATY_SHALL_BETRAY
                | DTREATY_UNK_800))) {
                continue;
            }
            uint8_t their_bases = PlayersData[i].region_total_bases[region_target_best];
            if (!(treaty & (DTREATY_VENDETTA | DTREATY_UNK_800))
                && (attack_count > 3 || !their_bases)) {
                continue;
            }
            int value = (treaty & DTREATY_VENDETTA) * 0x80 + their_bases;
            if (value > value_best) {
                value_best = value;
                faction_id_target = i;
            }
        }
        if (faction_id_target > 0) {
            BOOL by_sea = (value_best == 0x800); // vendetta, no enemy base in the region
            int dist_best = 9999;
            int base_id_best = -1;
            for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
                Base *base = &Bases[base_id];
                if (base->faction_id_current != faction_id
                    || altitude_at(base->x, base->y) < ALT_BIT_SHORE_LINE
                    || region_at(base->x, base->y) != (uint32_t)region_target_best
                    || (by_sea && !is_coast(base->x, base->y, false))) {
                    continue;
                }
                int base_id_atk = p->base_id_atk_target;
                if (base_id_atk >= 0 && p->diplo_treaties[Bases[base_id_atk].faction_id_current]
                    & (DTREATY_VENDETTA | DTREATY_UNK_800) && region_at(Bases[base_id_atk].x,
                        Bases[base_id_atk].y) == (uint32_t)region_target_best) {
                    *BaseFindDist = vector_dist(base->x, base->y, Bases[base_id_atk].x,
                        Bases[base_id_atk].y);
                } else if (base_find(base->x, base->y, faction_id_target, region_target_best,
                    -1, -1) < 0) {
                    base_find(base->x, base->y, faction_id_target);
                    *BaseFindDist *= 2;
                }
                if (*BaseFindDist < dist_best) {
                    dist_best = *BaseFindDist;
                    base_id_best = base_id;
                }
            }
            if (base_id_best >= 0) {
                p->x_target = Bases[base_id_best].x;
                p->y_target = Bases[base_id_best].y;
                if (by_sea) {
                    p->flags |= 0x800;
                } else {
                    p->flags &= ~0x800;
                }
                // gather the free land combat units of the region that are nearer the staging base
                // than the enemy's nearest base (at most 8; by sea, as many as the transports hold)
                for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
                    Veh *veh = &Vehs[veh_id];
                    if (veh->faction_id != faction_id
                        || veh->state & (VSTATE_UNK_20000 | VSTATE_UNK_40000)) {
                        continue;
                    }
                    int proto_id = veh->proto_id;
                    if (VehPrototypes[proto_id].plan > PLAN_DEFENSIVE
                        || Chassis[VehPrototypes[proto_id].chassis_id].triad != TRIAD_LAND
                        || has_abil(proto_id, ABL_DROP_POD)
                        || region_at(veh->x, veh->y) != (uint32_t)region_target_best
                        || (veh->order && veh->order != ORDER_MOVE_TO)) {
                        continue;
                    }
                    base_find(veh->x, veh->y, faction_id_target);
                    if (*BaseFindDist > vector_dist(veh->x, veh->y, p->x_target, p->y_target)) {
                        int weap = weap_strat(VehPrototypes[veh->proto_id].weapon_id, faction_id);
                        int arm = arm_strat(VehPrototypes[veh->proto_id].armor_id, faction_id);
                        if (weap >= arm || (!(p->unk_72 & 1) && !by_sea)) {
                            int gathering = p->unk_71;
                            veh->state = (veh->state & ~VSTATE_UNK_20000) | VSTATE_UNK_40000;
                            p->unk_71 = gathering + 1;
                            if (weap < arm) {
                                p->unk_72 |= 1;
                            }
                            if (p->unk_71 >= 8
                                || (by_sea && p->unk_71 >= p->unk_70 * 2 / 0x400000)) {
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    if (*TurnCurrentNum && faction_id) {
        // goals for the units: pass 0 the ones that had a goal last turn (0x31 becomes 0x32), then
        // the rest; goal_counts[] counts the units sent to each goal
        int goal_counts[MaxGoalsNum];
        memset(goal_counts, 0, sizeof(goal_counts));
        for (int veh_id = *VehCurrentCount - 1; veh_id >= 0; veh_id--) {
            if (Vehs[veh_id].faction_id == faction_id && Vehs[veh_id].move_to_ai_type == 0x31) {
                Vehs[veh_id].move_to_ai_type = 0x32;
            }
        }
        for (int pass = 0; pass < 2; pass++) {
            for (int veh_id = *VehCurrentCount - 1; veh_id >= 0; veh_id--) {
                if (veh_id >= *VehCurrentCount) {
                    continue;
                }
                Veh *veh = &Vehs[veh_id];
                if (veh->faction_id != faction_id || (!pass && veh->move_to_ai_type != 0x32)) {
                    continue;
                }
                int x = veh->x;
                int y = veh->y;
                uint32_t region = region_at(x, y);
                int plan = VehPrototypes[veh->proto_id].plan;
                if (pass && veh->move_to_ai_type == 0x31) {
                    if (plan == PLAN_TERRAFORMING
                        || (p->region_base_plan[region] <= 2 && plan <= PLAN_DEFENSIVE)) {
                        continue;
                    }
                }
                uint32_t state = veh->state & ~VSTATE_UNK_40000000;
                veh->state = state;
                if (veh->order >= ORDER_FARM && veh->order < ORDER_MOVE_TO) { // terraforming
                    continue;
                }
                if (*FactionsStatus & faction_bit) { // human: automated units only
                    if (!(state & VSTATE_UNK_200)
                        || (VehPrototypes[veh->proto_id].plan == PLAN_TERRAFORMING
                            && veh->order_auto_type)
                        || (state & (VSTATE_UNK_1000000 | VSTATE_UNK_200))
                        == (VSTATE_UNK_1000000 | VSTATE_UNK_200)
                        || ((state & (VSTATE_UNK_2000000 | VSTATE_UNK_200))
                            == (VSTATE_UNK_2000000 | VSTATE_UNK_200)
                            && veh->patrol_current_point < veh->waypoint_count)) {
                        continue;
                    }
                }
                if (state & VSTATE_UNK_10000000) {
                    p->region_total_offensive_vehs[region]++;
                    plan = PLAN_DEFENSIVE;
                } else if (region < MaxRegionLandNum) {
                    // garrison quota: one unit per (2 - ai_fight) bases, more in held regions
                    int divisor = 2 - p->ai_fight;
                    if (p->region_base_plan[region] == 8) {
                        divisor += 2;
                    } else if (p->region_base_plan[region] == 2) {
                        divisor = 1;
                    }
                    if (p->flags & PFLAG_STRAT_DEF_OBJECTIVES) {
                        divisor = 1;
                    }
                    int quota = (p->region_total_bases[region] + divisor - 1) / divisor;
                    if (quota && plan <= PLAN_COMBAT
                        && (p->region_base_plan[region] > 2 || (base_who(x, y) == (int)faction_id
                            && (p->region_base_plan[region] == 2 || p->unk_77[region] & 0x4000)
                            && stack_check(veh_id, 11, VSTATE_UNK_10000000, VSTATE_UNK_10000000,
                                -1) < 1))) {
                        uint32_t state_now = veh->state;
                        if (!(state_now & (VSTATE_UNK_20000 | VSTATE_UNK_40000))
                            && p->region_total_offensive_vehs[region] < quota) {
                            p->region_total_offensive_vehs[region]++;
                            veh->state = state_now | VSTATE_UNK_10000000;
                            plan = PLAN_DEFENSIVE;
                        }
                    }
                }
                uint8_t triad = Chassis[VehPrototypes[veh->proto_id].chassis_id].triad;
                if (triad != TRIAD_AIR) {
                    BOOL is_ocean_tile = altitude_at(x, y) < ALT_BIT_SHORE_LINE;
                    BOOL is_sea_unit = (triad == TRIAD_SEA);
                    if (is_ocean_tile != is_sea_unit) { // aboard a transport or in a base
                        if (base_who(x, y) < 0) {
                            continue;
                        }
                        BOOL is_flat = *MapIsFlat & 1;
                        for (int i = 0; i < 8; i++) {
                            int x_adj = x + RadiusBaseX[i];
                            if (!is_flat) {
                                if (x_adj < 0) {
                                    x_adj += *MapLongitudeBounds;
                                } else if (x_adj >= *MapLongitudeBounds) {
                                    x_adj -= *MapLongitudeBounds;
                                }
                            }
                            int y_adj = y + RadiusBaseY[i];
                            // vanilla tests the unit's own tile again instead of the adjacent one,
                            // so this never matches and the unit plans from where it is
                            if (y_adj >= 0 && y_adj < *MapLatitudeBounds && x_adj >= 0
                                && x_adj < *MapLongitudeBounds && is_ocean_tile == is_sea_unit) {
                                x = x_adj;
                                y = y_adj;
                                region = region_at(x_adj, y_adj);
                                break;
                            }
                        }
                    }
                }
                if (plan == PLAN_COMBAT) {
                    if (p->region_base_plan[region] == 2 || p->region_base_plan[region] == 8) {
                        plan = PLAN_DEFENSIVE;
                    } else {
                        plan = PLAN_OFFENSIVE;
                    }
                }
                if ((veh->state & (VSTATE_UNK_20000 | VSTATE_UNK_40000)) == VSTATE_UNK_40000
                    && p->x_target >= 0) { // gathering: go to the staging base
                    veh->order = ORDER_MOVE_TO;
                    veh->move_to_ai_type = 0x30;
                    veh->waypoint_x[0] = (int16_t)p->x_target;
                    veh->waypoint_y[0] = (int16_t)p->y_target;
                    if (VehPrototypes[veh->proto_id].plan == PLAN_COLONIZATION) {
                        veh->terraforming_turns = 0;
                    }
                    continue;
                }
                if (plan == PLAN_NAVAL_TRANSPORT) { // transports wait to be full
                    int cargo = stack_check(veh_id, 8, -1, -1, -1);
                    if (cargo < (int)veh_cargo(veh_id)) {
                        continue;
                    }
                }
                if (veh->move_to_ai_type == 0x46 && plan == PLAN_DEFENSIVE) {
                    continue;
                }
                BOOL attacking = false;
                if ((veh->state & (VSTATE_UNK_20000 | VSTATE_UNK_40000))
                    == (VSTATE_UNK_20000 | VSTATE_UNK_40000)) {
                    plan = PLAN_OFFENSIVE;
                    attacking = true;
                }
                if (veh->order == ORDER_HOLD && !attacking && base_who(x, y) >= 0) {
                    int artifacts = stack_check(veh_id, 2, PLAN_ALIEN_ARTIFACT, -1, -1);
                    if (stack_check(veh_id, 1, -1, -1, -1) - artifacts <= 1) {
                        continue;
                    }
                    if (plan == PLAN_DEFENSIVE || plan == PLAN_RECONNAISANCE || plan == PLAN_COMBAT) {
                        int base_id = base_at(x, y);
                        int defenders = stack_check(veh_id, 16, -1, -1, -1);
                        if (defenders < (int)garrison_check(base_id)) {
                            continue;
                        }
                    }
                }
                if (plan == PLAN_TERRAFORMING && veh->flags & 0x800) {
                    continue;
                }
                int value_best = 99999;
                int goal_id_best = -1;
                for (int goal_id = 0; goal_id < MaxGoalsNum; goal_id++) {
                    Goal *goal = &p->goals[goal_id];
                    if (goal->priority <= 0 || goal->type < 0) {
                        continue;
                    }
                    if ((goal->type & 0xF) != plan && (attacking || goal->type != AI_GOAL_SCOUT
                        || plan > PLAN_RECONNAISANCE || veh->order == ORDER_HOLD)) {
                        continue;
                    }
                    if (goal->y < 0 || goal->y >= *MapLatitudeBounds || goal->x < 0
                        || goal->x >= *MapLongitudeBounds) {
                        goal->x = -1;
                        goal->y = -1;
                        continue;
                    }
                    Map *goal_tile = map_loc(goal->x, goal->y);
                    if (region != goal_tile->region) {
                        continue;
                    }
                    int proto_id = veh->proto_id;
                    uint8_t triad_goal = Chassis[VehPrototypes[proto_id].chassis_id].triad;
                    if (triad_goal == TRIAD_AIR
                        || ((goal_tile->climate & 0xE0) < ALT_BIT_SHORE_LINE)
                        != (triad_goal == TRIAD_SEA) || VehPrototypes[proto_id].unk_1 == 21) {
                        continue;
                    }
                    if (plan == PLAN_OFFENSIVE) {
                        if (goal_tile->bit & BIT_MONOLITH
                            && (!want_monolith(veh_id) || attacking || goal_counts[goal_id])) {
                            continue;
                        }
                    } else if (plan == PLAN_COLONIZATION) {
                        if (base_territory(faction_id, goal->x, goal->y) >= 0) {
                            del_site(faction_id, AI_GOAL_COLONIZE, goal->x, goal->y, 0);
                            continue;
                        }
                    } else if (plan == PLAN_RECONNAISANCE) {
                        if (has_abil(proto_id, ABL_EMPATHIC)) {
                            continue;
                        }
                    }
                    int base_id_goal = base_at(goal->x, goal->y);
                    int score = vector_dist(x, y, goal->x, goal->y) << 8;
                    int taken = (!attacking && (base_id_goal < 0
                        || Bases[base_id_goal].faction_id_current != faction_id
                        || !Bases[base_id_goal].unk_1)) ? goal_counts[goal_id] : 0;
                    int priority = goal->priority - taken;
                    if (priority <= 0) {
                        continue;
                    }
                    if (attacking && base_who(goal->x, goal->y) < 0) {
                        continue;
                    }
                    if (goal->type == AI_GOAL_SCOUT && plan != PLAN_RECONNAISANCE) {
                        score *= 2;
                        if (veh->state & VSTATE_UNK_10000000 || base_who(goal->x, goal->y) < 0) {
                            continue;
                        }
                    }
                    if (taken && (goal->type == AI_GOAL_SCOUT || goal->type == AI_GOAL_COLONIZE)) {
                        continue;
                    }
                    if (score > 6 / (taken + 1)
                        * Chassis[VehPrototypes[veh->proto_id].chassis_id].speed * priority - taken + 4
                        && p->region_base_plan[region] != plan && goal->type != AI_GOAL_SCOUT
                        && (taken || goal->type != AI_GOAL_TERRAFORM_LAND)) {
                        continue;
                    }
                    if (goal->type == AI_GOAL_COLONIZE) {
                        int site;
                        Map *site_tile = map_loc(goal->x, goal->y);
                        if (site_tile->bit & BIT_BASE_RADIUS) {
                            site = 1;
                        } else {
                            site = site_tile->val2 >> 4;
                            if (!site) {
                                site = world_site(goal->x, goal->y, 0);
                                site_set(goal->x, goal->y, site);
                            }
                            if (site < 1) {
                                site = 1;
                            } else if (site > 15) {
                                site = 15;
                            }
                        }
                        priority *= site;
                    }
                    if (veh->state & VSTATE_UNK_10000000) {
                        int veh_id_goal = veh_at(goal->x, goal->y);
                        if (veh_id_goal >= 0 && stack_check(veh_id_goal, 11, VSTATE_UNK_10000000,
                            VSTATE_UNK_10000000, faction_id) > (score ? 0 : 1)) {
                            continue;
                        }
                    }
                    if (goal->type == AI_GOAL_TERRAFORM_LAND) {
                        // a former of the goal's base (other formers home elsewhere count triple)
                        int base_id_former = goal->base_id;
                        if (base_id_former >= 0 && Bases[base_id_former].state & BSTATE_UNK_20
                            && veh->home_base_id != base_id_former) {
                            score *= 3;
                        }
                        BOOL closer = false;
                        for (int i = 0; i < *VehCurrentCount; i++) {
                            if (Vehs[i].faction_id == faction_id
                                && VehPrototypes[Vehs[i].proto_id].plan == PLAN_TERRAFORMING
                                && i != veh_id) {
                                int score_other = vector_dist(Vehs[i].x, Vehs[i].y,
                                    goal->x, goal->y) << 8;
                                if (base_id_former >= 0
                                    && Bases[base_id_former].state & BSTATE_UNK_20
                                    && Vehs[i].home_base_id != base_id_former) {
                                    score_other *= 3;
                                }
                                if (score_other < score) {
                                    closer = true;
                                    break;
                                }
                            }
                        }
                        if (closer || (goal_counts[goal_id] && (base_id_former < 0
                            || veh->home_base_id != base_id_former))) {
                            continue;
                        }
                    }
                    int owner = whose_territory(faction_id, goal->x, goal->y, NULL, false);
                    int score_adj = score;
                    if (owner >= 0 && owner != (int)faction_id) {
                        uint32_t treaty = p->diplo_treaties[owner];
                        if (!(treaty & (DTREATY_VENDETTA | DTREATY_SHALL_BETRAY | DTREATY_UNK_800))) {
                            if (treaty & DTREATY_UNK_100000) {
                                continue;
                            }
                            if (!(treaty & DTREATY_PACT)) {
                                if (treaty & DTREATY_TREATY) {
                                    continue;
                                }
                                score_adj = score * 2;
                            }
                        }
                    }
                    if (goal->type == AI_GOAL_PRIORITY_COMBAT) {
                        if (!has_abil(veh->proto_id, ABL_ARTILLERY)
                            || Chassis[VehPrototypes[veh->proto_id].chassis_id].triad != TRIAD_LAND) {
                            continue;
                        }
                        score_adj /= 2;
                    }
                    int value = score_adj / priority;
                    if (value < value_best) {
                        value_best = value;
                        goal_id_best = goal_id;
                    }
                }
                if (goal_id_best >= 0) {
                    if (veh->move_to_ai_type < 0x33 || veh->move_to_ai_type > 0x35) {
                        goal_counts[goal_id_best]++;
                    }
                    Goal *goal = &p->goals[goal_id_best];
                    veh->order = ORDER_MOVE_TO;
                    veh->move_to_ai_type = 0x31;
                    veh->waypoint_x[0] = (int16_t)goal->x;
                    veh->waypoint_y[0] = (int16_t)goal->y;
                    if (VehPrototypes[veh->proto_id].plan == PLAN_COLONIZATION) {
                        veh->terraforming_turns = 0;
                    }
                    if (goal->type == AI_GOAL_SCOUT) {
                        veh->state |= VSTATE_UNK_40000000;
                    }
                    if (goal->type == AI_GOAL_DEFEND) {
                        int base_id = base_at(goal->x, goal->y);
                        if (base_id >= 0 && Bases[base_id].faction_id_current == faction_id) {
                            Bases[base_id].state |= BSTATE_UNK_8000000;
                            if (!(*FactionsStatus & faction_bit)) {
                                base_reset(base_id, false);
                            }
                        }
                    }
                }
            }
        }
        wipe_goals(faction_id);
    }
}

/*
Purpose: The AI's weighting of an attack on veh_id_def's stack: the odds times what the attacker's
         faction takes from the defender's side (the stack's cost outside a base, the defender's
         cost in one) over the attacker's cost, then raised for good or important targets (a
         non-combat defender, an objective unit, a base or the faction's goal tile, the attacker's
         own kind of terrain, a stack with few fighting units, supporting land units next to a
         base, air raids on terraformers or at sea, air drop units ready to follow) and lowered
         for air superiority units against armed surface units and for bombers.
Original Offset: 00565F20
Return Value: Weighted odds
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
int __cdecl compute_odds(int odds, uint32_t faction_id, int veh_id_atk, int veh_id_def, int base_id) {
    int x = Vehs[veh_id_def].x;
    int y = Vehs[veh_id_def].y;
    VehPrototype *proto_atk = &VehPrototypes[Vehs[veh_id_atk].proto_id];
    uint8_t triad = Chassis[proto_atk->chassis_id].triad;
    uint8_t plan = proto_atk->plan;
    if (base_id < -1) {
        base_id = base_at(x, y);
    }
    PlayerData *p = &PlayersData[faction_id];
    uint8_t region_plan = p->region_base_plan[map_loc(x, y)->region];
    if (base_id < 0) {
        int cost = VehPrototypes[Vehs[veh_id_def].proto_id].cost;
        odds *= (stack_check(veh_id_def, 7, -1, -1, -1) - cost) / 5 + cost; // the stack's cost
        if (stack_check(veh_id_def, 2, PLAN_ALIEN_ARTIFACT, -1, -1)) {
            odds = odds * 3 / 2;
        }
    } else {
        odds *= VehPrototypes[Vehs[veh_id_def].proto_id].cost;
    }
    odds /= proto_atk->cost;
    if (VehPrototypes[Vehs[veh_id_def].proto_id].plan >= PLAN_NAVAL_TRANSPORT) {
        odds *= 2; // non-combat defender
    }
    if (plan <= PLAN_COMBAT && region_plan == PLAN_OFFENSIVE && triad == TRIAD_LAND) {
        odds = odds * 3 / 2;
    }
    if (base_id >= 0 || at_goal(faction_id, 0, x, y)) {
        if ((triad == TRIAD_SEA) == is_ocean(x, y) && triad != TRIAD_AIR) {
            odds *= 2;
        } else if (base_id >= 0) {
            int noncombat = stack_check(veh_id_def, 19, -1, -1, -1);
            if (stack_check(veh_id_def, 1, -1, -1, -1) - noncombat <= 1) {
                odds *= 2;
            }
        }
    }
    if (Vehs[veh_id_def].flags & VFLAG_IS_OBJECTIVE) {
        odds *= 4;
    }
    if (triad == TRIAD_AIR) {
        if (Chassis[proto_atk->chassis_id].missile && is_ocean(x, y) && base_id < 0) {
            odds *= 2;
        }
        if (stack_check(veh_id_def, 2, PLAN_TERRAFORMING, -1, -1)) {
            if (!Chassis[proto_atk->chassis_id].missile) {
                odds *= 2;
            }
            if (is_ocean(x, y) && (p->flags & PFLAG_UNK_80000)) {
                odds *= 2;
            }
            if (Vehs[veh_id_def].order == ORDER_TERRAFORM_UP) {
                odds *= 2;
            }
            if (Vehs[veh_id_def].order == ORDER_TERRAFORM_DOWN) {
                odds *= 2;
            }
        }
        if (base_id >= 0) {
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
                if (y_radius >= 0 && y_radius < *MapLatitudeBounds && x_radius >= 0
                    && x_radius < *MapLongitudeBounds) {
                    int veh_id = veh_at(x_radius, y_radius); // -1 reads Vehs[-1], as in the original
                    if (Vehs[veh_id].faction_id == faction_id && stack_check(veh_id, 3, TRIAD_LAND, -1, -1)) {
                        odds *= 2;
                        break;
                    }
                }
            }
            int noncombat = stack_check(veh_id_def, 19, -1, -1, -1);
            if (stack_check(veh_id_def, 1, -1, -1, -1) - noncombat
                <= p->proto_id_active[Vehs[veh_id_atk].proto_id] / 10 + 1) {
                for (int i = 0; i < MaxVehProtoFactionNum; i++) {
                    int proto_id = faction_id * MaxVehProtoFactionNum + i;
                    if ((VehPrototypes[proto_id].flags & PROTO_ACTIVE)
                        && (VehPrototypes[proto_id].flags & PROTO_TYPED_COMPLETE)
                        && p->proto_id_active[proto_id] && has_abil(proto_id, ABL_DROP_POD)) {
                        odds *= 2;
                        if (Chassis[proto_atk->chassis_id].missile) {
                            odds *= 2;
                        }
                        break;
                    }
                }
            }
        }
    } else if (base_id >= 0) {
        int land_units = 0;
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
            if (y_radius >= 0 && y_radius < *MapLatitudeBounds && x_radius >= 0
                && x_radius < *MapLongitudeBounds) {
                int veh_id = veh_at(x_radius, y_radius); // -1 reads Vehs[-1], as in the original
                if (Vehs[veh_id].faction_id == faction_id) {
                    land_units += stack_check(veh_id, 3, TRIAD_LAND, -1, -1);
                }
            }
        }
        int defenders = stack_check(veh_id_def, 16, -1, -1, -1);
        if (defenders) {
            while (land_units > defenders && odds < 1000000) {
                odds *= 2;
                land_units -= defenders;
            }
        }
    }
    if (plan == PLAN_AIR_SUPERIORITY) {
        VehPrototype *proto_def = &VehPrototypes[Vehs[veh_id_def].proto_id];
        if (Chassis[proto_def->chassis_id].triad != TRIAD_AIR && Weapon[proto_def->weapon_id].offense_rating) {
            return odds / 2;
        }
    } else if (triad == TRIAD_AIR && plan == PLAN_OFFENSIVE && !Chassis[proto_atk->chassis_id].missile) {
        return (region_plan == PLAN_DEFENSIVE) ? odds * 2 / 3 : odds / 3;
    }
    return odds;
}

void __cdecl enemy_capabilities_t(uint32_t faction_id) {
    // * PSI could potentially be best weapon?
    // * PSI should always be last Weapon
    // * faction order will affect initial run through
    // > potential fix would be to calculate all factions at once before enemy best compares

    //BOOL has_worms = veh_avail(BSC_MIND_WORMS, faction_id, -1);
    //PlayersData[faction_id].bestPsiAtkVal = has_worms ? weap_strat(WPN_PSI_ATTACK, faction_id) : 0;
    PlayersData[faction_id].best_weapon_value = 1;
    for (int i = 0; i < MaxWeaponNum; i++) {
        if (has_tech(Weapon[i].preq_tech, faction_id) && Weapon[i].offense_rating < 99) {
            int weap_val = weap_strat(i, faction_id);
            //if (Weapon[i].offense_rating < 0 && weap_val > PlayersData[faction_id].bestPsiAtkVal) {
            //    PlayersData[faction_id].bestPsiAtkVal = weap_val;
            //}
            if (Weapon[i].offense_rating >= 0 && weap_val 
                > PlayersData[faction_id].best_weapon_value) {
                PlayersData[faction_id].best_weapon_value = weap_val;
            }
        }
    }
    //PlayersData[faction_id].bestPsiDefVal = has_worms ? arm_strat(ARM_PSI_DEFENSE, faction_id) : 0;
    PlayersData[faction_id].best_armor_value = 1;
    for (int i = 0; i < MaxArmorNum; i++) {
        if (has_tech(Armor[i].preq_tech, faction_id)) {
            int arm_val = arm_strat(i, faction_id);
            //if (Armor[i].defense_rating < 0 && arm_val > PlayersData[faction_id].bestPsiDefVal) {
            //    PlayersData[faction_id].bestPsiDefVal = arm_val;
            //}
            if (Armor[i].defense_rating >= 0 
                && arm_val > PlayersData[faction_id].best_armor_value) {
                PlayersData[faction_id].best_armor_value = arm_val;
            }
        }
    }
    PlayersData[faction_id].best_land_speed = 1;
    for (int i = 0; i < MaxChassisNum; i++) {
        if (has_tech(Chassis[i].preq_tech, faction_id) && Chassis[i].triad == TRIAD_LAND) {
            if (Chassis[i].speed > PlayersData[faction_id].best_land_speed) {
                PlayersData[faction_id].best_land_speed = Chassis[i].speed;
            }
        }
    }
    PlayersData[faction_id].enemy_best_weapon_value = 0;
    PlayersData[faction_id].enemy_best_armor_value = 0;
    PlayersData[faction_id].enemy_best_land_speed = 0;
    //PlayersData[faction_id].enemyBestPsiAtkVal = 0;
    //PlayersData[faction_id].enemyBestPsiDefVal = 0;
    for (int i = 0; i < 4 && !PlayersData[faction_id].enemy_best_weapon_value; i++) {
        // 1st pass: vendetta, no treaty, has commlink
        // 2nd pass: no treaty, has commlink
        // 3rd pass: has commlink
        // 4th pass: any non-pact faction
        for (uint32_t j = 1, treaties; j < MaxPlayerNum; j++) {
            if (j != faction_id
                && (treaties = PlayersData[i].diplo_treaties[j], !(treaties & DTREATY_PACT))
                && ((!i && treaties & DTREATY_VENDETTA && !(treaties & DTREATY_TREATY)
                    && treaties & DTREATY_COMMLINK)
                    || (i == 1 && !(treaties & DTREATY_TREATY) && treaties & DTREATY_COMMLINK)
                    || (i == 2 && treaties & DTREATY_COMMLINK) || (i == 3))) {
                if (PlayersData[faction_id].enemy_best_weapon_value 
                    < PlayersData[j].best_weapon_value) {
                    PlayersData[faction_id].enemy_best_weapon_value 
                        = PlayersData[j].best_weapon_value;
                }
                if (PlayersData[faction_id].enemy_best_armor_value 
                    < PlayersData[j].best_armor_value) {
                    PlayersData[faction_id].enemy_best_armor_value
                        = PlayersData[j].best_armor_value;
                }
                if (PlayersData[faction_id].enemy_best_land_speed 
                    < PlayersData[j].best_land_speed) {
                    PlayersData[faction_id].enemy_best_land_speed = PlayersData[j].best_land_speed;
                }
                /*
                if (PlayersData[faction_id].enemyBestPsiAtkVal < PlayersData[j].bestPsiAtkVal) {
                    PlayersData[faction_id].enemyBestPsiAtkVal = PlayersData[j].bestPsiAtkVal;
                }
                if (PlayersData[faction_id].enemyBestPsiDefVal < PlayersData[j].bestPsiDefVal) {
                    PlayersData[faction_id].enemyBestPsiDefVal = PlayersData[j].bestPsiDefVal;
                }
                */
            }
        }
    }
    // PSI
    BOOL has_worms = veh_avail(BSC_MIND_WORMS, faction_id, -1);
    PlayersData[faction_id].best_psi_offense = has_worms 
        ? weap_strat(WPN_PSI_ATTACK, faction_id) : 0;
    for (int i = 0; i < MaxWeaponNum; i++) {
        if (has_tech(Weapon[i].preq_tech, faction_id) && Weapon[i].offense_rating < 99) {
            int weap_val = weap_strat(i, faction_id);
            if (Weapon[i].offense_rating < 0 && weap_val 
                > PlayersData[faction_id].best_psi_offense) {
                PlayersData[faction_id].best_psi_offense = weap_val;
            }
        }
    }
    PlayersData[faction_id].best_psi_defense = has_worms 
        ? arm_strat(ARM_PSI_DEFENSE, faction_id) : 0;
    for (int i = 0; i < MaxArmorNum; i++) {
        if (has_tech(Armor[i].preq_tech, faction_id)) {
            int arm_val = arm_strat(i, faction_id);
            if (Armor[i].defense_rating < 0 && arm_val > PlayersData[faction_id].best_psi_defense) {
                PlayersData[faction_id].best_psi_defense = arm_val;
            }
        }
    }
    PlayersData[faction_id].enemy_best_psi_offense = 0;
    PlayersData[faction_id].enemy_best_psi_defense = 0;
    for (int i = 0; i < 4 && !PlayersData[faction_id].enemy_best_psi_offense; i++) {
        // 1st pass: vendetta, no treaty, has commlink
        // 2nd pass: no treaty, has commlink
        // 3rd pass: has commlink
        // 4th pass: any non-pact faction
        for (uint32_t j = 1, treaties; j < MaxPlayerNum; j++) {
            if (j != faction_id
                && (treaties = PlayersData[i].diplo_treaties[j], !(treaties & DTREATY_PACT))
                && ((!i && treaties & DTREATY_VENDETTA && !(treaties & DTREATY_TREATY)
                    && treaties & DTREATY_COMMLINK)
                    || (i == 1 && !(treaties & DTREATY_TREATY) && treaties & DTREATY_COMMLINK)
                    || (i == 2 && treaties & DTREATY_COMMLINK) || (i == 3))) {
                if (PlayersData[faction_id].enemy_best_psi_offense 
                    < PlayersData[j].best_psi_offense) {
                    PlayersData[faction_id].enemy_best_psi_offense 
                        = PlayersData[j].best_psi_offense;
                }
                if (PlayersData[faction_id].enemy_best_psi_defense
                    < PlayersData[j].best_psi_defense) {
                    PlayersData[faction_id].enemy_best_psi_defense 
                        = PlayersData[j].best_psi_defense;
                }
            }
        }
    }
}
