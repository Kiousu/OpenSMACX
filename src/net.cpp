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
#include "net.h"

/*
Every synch_*() helper is a thin wrapper around NetDaemon::synch() on the global NetDaemon
instance: synch(type, id, 0, 0, NULL, 1, 0x2101). Only the message type and the object id
differ. Two further wrappers exist in the original binary with no callers (types 1 and 23 at
0x593370 and 0x593480); they are not reproduced here.
*/
static void __cdecl net_synch(int16_t type, int id, int id2 = 0) {
    NetDaemon_synch(NetDaemonVar, NULL, type, id, id2, 0, NULL, 1, 0x2101);
}

/*
Purpose: Synchronize the specified unit with the other players of a network game.
Original Offset: 00593220
Return Value: n/a
Status: Complete
*/
void __cdecl synch_veh(int veh_id) {
    net_synch(NET_SYNCH_VEH, veh_id);
}

/*
Purpose: Synchronize the specified base with the other players of a network game.
Original Offset: 00593250
Return Value: n/a
Status: Complete
*/
void __cdecl synch_base(int base_id) {
    net_synch(NET_SYNCH_BASE, base_id);
}

/*
Purpose: Synchronize the specified faction's energy reserves with the other players.
Original Offset: 00593280
Return Value: n/a
Status: Complete
*/
void __cdecl synch_energy(int faction_id) {
    net_synch(NET_SYNCH_ENERGY, faction_id);
}

/*
Purpose: Synchronize the specified faction's current research target with the other players.
Original Offset: 005932B0
Return Value: n/a
Status: Complete
*/
void __cdecl synch_researching(int faction_id) {
    net_synch(NET_SYNCH_RESEARCHING, faction_id);
}

/*
Purpose: Synchronize the specified faction's leader data with the other players.
Original Offset: 005932E0
Return Value: n/a
Status: Complete
*/
void __cdecl synch_leader(int faction_id) {
    net_synch(NET_SYNCH_LEADER, faction_id);
}

/*
Purpose: Synchronize the specified faction's AI settings with the other players.
Original Offset: 00593310
Return Value: n/a
Status: Complete
*/
void __cdecl synch_ai(int faction_id) {
    net_synch(NET_SYNCH_AI, faction_id);
}

/*
Purpose: Synchronize the specified faction's research progress with the other players.
Original Offset: 00593340
Return Value: n/a
Status: Complete
*/
void __cdecl synch_research(int faction_id) {
    net_synch(NET_SYNCH_RESEARCH, faction_id);
}

/*
Purpose: Synchronize the specified faction's energy allocation with the other players.
Original Offset: 00593390
Return Value: n/a
Status: Complete
*/
void __cdecl synch_alloc(int faction_id) {
    net_synch(NET_SYNCH_ALLOC, faction_id);
}

/*
Purpose: Synchronize the specified faction's social engineering with the other players.
Original Offset: 005933C0
Return Value: n/a
Status: Complete
*/
void __cdecl synch_soc(int faction_id) {
    net_synch(NET_SYNCH_SOC, faction_id);
}

/*
Purpose: Synchronize the specified prototype with the other players of a network game.
Original Offset: 005933F0
Return Value: n/a
Status: Complete
*/
void __cdecl synch_proto(int proto_id) {
    net_synch(NET_SYNCH_PROTO, proto_id);
}

/*
Purpose: Synchronize the specified faction's observation data with the other players.
Original Offset: 00593420
Return Value: n/a
Status: Complete
*/
void __cdecl synch_obs(int faction_id) {
    net_synch(NET_SYNCH_OBS, faction_id);
}

/*
Purpose: Synchronize the diplomatic state between the two specified factions with the other
         players of a network game.
Original Offset: 00593450
Return Value: n/a
Status: Complete
*/
void __cdecl synch_diplo(int faction_id, int faction_id_other) {
    net_synch(NET_SYNCH_DIPLO, faction_id, faction_id_other);
}

/*
Purpose: Synchronize the specified template with the other players of a network game.
Original Offset: 005934B0
Return Value: n/a
Status: Complete
*/
void __cdecl synch_template(int id) {
    net_synch(NET_SYNCH_TEMPLATE, id);
}

/*
Purpose: Synchronize the specified radius with the other players of a network game.
Original Offset: 005934E0
Return Value: n/a
Status: Complete
*/
void __cdecl synch_radius(int id) {
    net_synch(NET_SYNCH_RADIUS, id);
}
