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

/*
Diplomacy message types sent by the net_*() helpers. The low 12 bits double as the event code
that NetDaemon::await_diplo() waits for.
*/
enum NetMsgType {
    NET_MSG_DOUBLE_CROSS = 0x2440,
    NET_MSG_TREATY_ON = 0x2441,
    NET_MSG_TREATY_OFF = 0x2442,
    NET_MSG_SET_TREATY = 0x2443,
    NET_MSG_AGENDA_ON = 0x2444,
    NET_MSG_AGENDA_OFF = 0x2445,
    NET_MSG_SET_AGENDA = 0x2446,
    NET_MSG_ENERGY = 0x2447,
    NET_MSG_TRADE_MAPS = 0x2448,
    NET_MSG_CEDE_BASE = 0x2449,
    NET_MSG_WITHDRAWAL = 0x244A,
    NET_MSG_TECH = 0x244B,
    NET_MSG_PACT_ENDS = 0x244D,
    NET_MSG_LOAN = 0x244E,
};

/*
Every network message starts with this 16 byte header, followed by a payload whose layout
depends on the message family. Sizes passed to NetDaemon::send_message() are the full struct.
*/
struct NetMsgHeader {
    int16_t type;
    int16_t unk_1; // not initialized by the original code
    int faction_id; // sender (FactionIDCurrent)
    uint32_t time; // timeGetTime()
    int sequence; // NetMsgSequence
};

struct NetMsgVeh { // 0x18
    NetMsgHeader header;
    int data[2];
};

struct NetMsgBase { // 0x2C
    NetMsgHeader header;
    int data;
    char name[24];
};

struct NetMsgData { // 0x20
    NetMsgHeader header;
    int data[4];
};

struct NetMsgBigData { // 0x90
    NetMsgHeader header;
    int data[32];
};

struct NetMsgLock { // 0x28
    NetMsgHeader header;
    int data[6];
};

struct NetMsgLandmark { // 0x40
    NetMsgHeader header;
    int data[4];
    char name[32];
};

struct NetMsgChat { // 0x110
    NetMsgHeader header;
    char text[256];
};

DLLEXPORT void __cdecl message_veh(int16_t type, int data1, int data2, int flags);
DLLEXPORT void __cdecl message_base(int16_t type, int data, LPCSTR name, int flags);
DLLEXPORT void __cdecl message_data(int16_t type, int flags, int data1, int data2, int data3,
                                    int data4);
DLLEXPORT void __cdecl message_big_data(int16_t type, int flags, int *data, int count);
DLLEXPORT void __cdecl message_lock(int16_t type, int data3, int data1, int data2, int data6,
                                    int data4, int data5, int flags);
DLLEXPORT void __cdecl message_landmark(int16_t type, int flags, LPCSTR name, int data1,
                                        int data2, int data3, int data4);
DLLEXPORT void __cdecl message_chat(int16_t type, int flags, LPCSTR text);

DLLEXPORT void __cdecl net_treaty_on(int faction_id, int faction_id_with, int treaty, BOOL await);
DLLEXPORT void __cdecl net_treaty_off(int faction_id, int faction_id_with, int treaty,
                                      BOOL await);
DLLEXPORT void __cdecl net_set_treaty(int faction_id, int faction_id_with, int treaty, BOOL set,
                                      BOOL await);
DLLEXPORT void __cdecl net_agenda_on(int faction_id, int faction_id_with, int agenda, BOOL await);
DLLEXPORT void __cdecl net_agenda_off(int faction_id, int faction_id_with, int agenda,
                                      BOOL await);
DLLEXPORT void __cdecl net_set_agenda(int faction_id, int faction_id_with, int agenda, BOOL set,
                                      BOOL await);
DLLEXPORT void __cdecl net_energy(int faction_id, int energy, int faction_id_other,
                                  int energy_other, BOOL await);
DLLEXPORT void __cdecl net_loan(int faction_id, int faction_id_with, int balance, int payment);
DLLEXPORT void __cdecl net_maps(int faction_id, int faction_id_with, BOOL await);
DLLEXPORT void __cdecl net_tech(int faction_id, int tech_id, int faction_id_other, BOOL await);
DLLEXPORT void __cdecl net_pact_ends(int faction_id, int faction_id_with, BOOL await);
DLLEXPORT BOOL __cdecl net_withdrawal(int faction_id, int faction_id_from, BOOL await);
DLLEXPORT void __cdecl net_cede_base(int base_id, int faction_id_to, BOOL await);
DLLEXPORT void __cdecl net_double_cross(int faction_id_atk, int faction_id_def, int faction_id_other,
                                        BOOL await);
DLLEXPORT BOOL __cdecl diplo_lock(int timeout);
DLLEXPORT void __cdecl diplo_unlock();
DLLEXPORT BOOL __cdecl pop_wait();
DLLEXPORT BOOL __cdecl pop_expire();

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
