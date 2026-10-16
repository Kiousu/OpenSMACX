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
#include "council.h"
#include "base.h"
#include "game.h"

RulesProposal *Proposal = (RulesProposal *)0x009A6828;
int *GovernorFactionID = (int *)0x009A6614; // current Planetary Governor, 0 if none

/*
Purpose: Calculate faction's vote count. Used for Planetary Governor and Supreme Leader.
Original Offset: 0052AD30
Return Value: Faction vote count
Status: Complete
*/
uint32_t __cdecl council_votes(uint32_t faction_id) {
    if (is_alien_faction(faction_id)) {
        return 0;
    }
    uint32_t votes = 0;
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (faction_id == Bases[i].faction_id_current) {
            votes += Bases[i].population_size;
        }
    }
    if (has_project(SP_EMPATH_GUILD, faction_id)) {
        votes += votes / 2; // +50% votes
    }
    if (has_project(SP_CLINICAL_IMMORTALITY, faction_id)) {
        votes *= 2; // Doubles votes
    }
    int bonus_count = Players[faction_id].faction_bonus_count;
    for (int i = 0; i < bonus_count; i++) {
        if (Players[faction_id].faction_bonus_id[i] == RULE_VOTES) {
            int votes_bonus = Players[faction_id].faction_bonus_val1[i];
            if (votes_bonus >= 0) {
                votes *= votes_bonus; // Peacekeeper bonus
            }
        }
    }
    return votes;
}

/*
Purpose: Check whether a faction's leader is eligible to be a Planetary Governor candidate.
Original Offset: 0052AE20
Return Value: Is the leader eligible (top two vote totals)? true/false
Status: Complete
*/
BOOL __cdecl eligible(uint32_t faction_id) {
    if (is_alien_faction(faction_id)) {
        return false;
    }
    uint32_t votes = council_votes(faction_id);
    uint32_t faction_count = 0;
    for (uint32_t i = 1; i < MaxPlayerNum; i++) {
        if (faction_id != i && is_alive(i) && council_votes(i) > votes) {
            faction_count++;
        }
    }
    return faction_count < 2;
}

/*
Purpose: Decide a faction's council vote. For the governor election (proposal 0) this is the
         candidate wanted most, or the one the faction pledged to vote for unless relations have
         soured; for any other proposal a pledged vote is honored, otherwise -1 (yes) or -2 (no)
         from wants_prop() adjusted for vendetta, revenge and pact with the proposer, with a
         turn-based tie breaker between -1 and -3.
Original Offset: 0052BE60
Return Value: Vote: candidate faction id for the election, otherwise -1 (yes), -2 (no), -3 (abstain)
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch3.log)
*/
int __cdecl council_get_vote(int faction_id, int proposal, int faction_id_proposer) {
    if (proposal == PROP_ELECT_PLANETARY_GOVERNOR) {
        int want_best = 0;
        int vote = -3;
        for (int candidate = 1; candidate < MaxPlayerNum; candidate++) {
            int want = wants_prop(faction_id, PROP_ELECT_PLANETARY_GOVERNOR, candidate);
            if (want > want_best) {
                want_best = want;
                vote = candidate;
            }
        }
        if (PlayersData[faction_id].council_pledge_vote[0]) {
            int pledged = PlayersData[faction_id].council_pledge_faction[0];
            if (!(PlayersData[faction_id].diplo_treaties[pledged]
                & (DTREATY_VENDETTA | DTREATY_WANT_REVENGE | DTREATY_SHALL_BETRAY
                    | DTREATY_UNK_800 | DTREATY_ATROCITY_VICTIM))) {
                return pledged;
            }
        }
        return vote;
    }
    int want = wants_prop(faction_id, proposal, faction_id_proposer);
    uint32_t treaties = PlayersData[faction_id].diplo_treaties[faction_id_proposer];
    if (treaties & DTREATY_VENDETTA) {
        want--;
    }
    if (treaties & DTREATY_WANT_REVENGE) {
        want--;
    }
    if (treaties & DTREATY_PACT) {
        want++;
    }
    int pledged = PlayersData[faction_id].council_pledge_vote[proposal];
    if (pledged) {
        return pledged;
    }
    if (want > 0) {
        return -1;
    }
    if (want < 0) {
        return -2;
    }
    return -1 - (*TurnCurrentNum + faction_id) % 3;
}

/*
Purpose: Check whether a faction may call the Planetary Council: not alien, council not disabled
         by the rules, no sunspots, no session already pending in hotseat/PBEM, and either the
         Empath Guild or commlinks with every living non-alien faction. With announce set the
         player at the controls is told why not.
Original Offset: 0052C670
Return Value: Can the faction call the council? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch3.log)
*/
BOOL __cdecl can_call_council(uint32_t faction_id, BOOL announce) {
    if (Players[faction_id].rule_flags & RFLAG_ALIEN) {
        return false;
    }
    if ((*GameMoreRules & MRULES_NO_PLANETARY_COUNCIL)
        && (!(*GameState & STATE_SCENARIO_EDITOR) || !(*GameState & STATE_OMNISCIENT_VIEW))) {
        return false;
    }
    BOOL tell = announce && faction_id == (uint32_t)*FactionIDCurrent && is_human(faction_id);
    if (*SunspotDuration > 0) {
        if (tell) {
            NetMsg_pop(NetMsgVar, 0, "SUNSPOTSNO2", -5000, 0, "solar_sm.pcx");
        }
        return false;
    }
    if (*IsMultiplayerPBEM && *CouncilSessionPending) {
        if (tell) {
            NetMsg_pop(NetMsgVar, 0, "COUNCILHOT", -5000, 0, NULL);
        }
        return false;
    }
    int base_id_empath = SecretProject->empath_guild;
    if (base_id_empath >= 0 && Bases[base_id_empath].faction_id_current == faction_id) {
        return true;
    }
    for (uint32_t i = 1; i < MaxPlayerNum; i++) {
        if ((Players[i].rule_flags & RFLAG_ALIEN) || !is_alive(i) || i == faction_id
            || (PlayersData[faction_id].diplo_treaties[i] & (DTREATY_COMMLINK | DTREATY_UNK_8000000))
            || ((*GameState & STATE_OMNISCIENT_VIEW) && is_human(faction_id)
                && faction_id == (uint32_t)*FactionIDCurrent)
            || (*IsMultiplayerNet && is_human(faction_id) && is_human(i))) {
            continue;
        }
        if (tell) {
            NetMsg_pop(NetMsgVar, 0, "COUNCILNEEDALL", -5000, 0, "council_sm.pcx");
        }
        return false;
    }
    return true;
}
