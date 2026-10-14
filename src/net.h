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
#pragma once

 /*
  * Network (multiplayer) related objects, variables and functions.
  */

/*
Message types passed to NetDaemon::synch() by the synch_*() helpers. The value is the first
parameter of NetDaemon::synch() and identifies which game object the other players must
refresh. Values not listed here are either used directly by other net code or unused.
*/
enum NetSynchType {
    NET_SYNCH_UNK_1 = 1, // only referenced by an unused helper at 0x593370
    NET_SYNCH_LEADER = 6,
    NET_SYNCH_AI = 8,
    NET_SYNCH_RESEARCH = 9,
    NET_SYNCH_RESEARCHING = 10,
    NET_SYNCH_ENERGY = 11,
    NET_SYNCH_ALLOC = 12,
    NET_SYNCH_SOC = 13,
    NET_SYNCH_PROTO = 14,
    NET_SYNCH_OBS = 16,
    NET_SYNCH_VEH = 17,
    NET_SYNCH_BASE = 19,
    NET_SYNCH_DIPLO = 22,
    NET_SYNCH_UNK_23 = 23, // only referenced by an unused helper at 0x593480
    NET_SYNCH_TEMPLATE = 24,
    NET_SYNCH_RADIUS = 35,
};

DLLEXPORT void __cdecl synch_veh(int veh_id);
DLLEXPORT void __cdecl synch_base(int base_id);
DLLEXPORT void __cdecl synch_energy(int faction_id);
DLLEXPORT void __cdecl synch_researching(int faction_id);
DLLEXPORT void __cdecl synch_leader(int faction_id);
DLLEXPORT void __cdecl synch_ai(int faction_id);
DLLEXPORT void __cdecl synch_research(int faction_id);
DLLEXPORT void __cdecl synch_alloc(int faction_id);
DLLEXPORT void __cdecl synch_soc(int faction_id);
DLLEXPORT void __cdecl synch_proto(int proto_id);
DLLEXPORT void __cdecl synch_obs(int faction_id);
DLLEXPORT void __cdecl synch_diplo(int faction_id, int faction_id_other);
DLLEXPORT void __cdecl synch_template(int template_id);
DLLEXPORT void __cdecl synch_radius(int base_id);
