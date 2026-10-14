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
#include "game.h"
#include "log.h"
#include "net.h"

static_assert(sizeof(NetMsgHeader) == 0x10, "NetMsgHeader size");
static_assert(sizeof(NetMsgVeh) == 0x18, "NetMsgVeh size");
static_assert(sizeof(NetMsgBase) == 0x2C, "NetMsgBase size");
static_assert(sizeof(NetMsgData) == 0x20, "NetMsgData size");
static_assert(sizeof(NetMsgBigData) == 0x90, "NetMsgBigData size");
static_assert(sizeof(NetMsgLock) == 0x28, "NetMsgLock size");
static_assert(sizeof(NetMsgLandmark) == 0x40, "NetMsgLandmark size");
static_assert(sizeof(NetMsgChat) == 0x110, "NetMsgChat size");
static_assert(offsetof(PlayerData, loan_balance) == 0xC4, "PlayerData::loan_balance offset");
static_assert(offsetof(PlayerData, loan_payment) == 0xE4, "PlayerData::loan_payment offset");
static_assert(offsetof(PlayerData, energy_reserves) == 0x220, "PlayerData::energy_reserves offset");

/*
Fill the common message header and hand the message to NetDaemon::send_message(). The send
depth counter brackets the call exactly as the original code does.
*/
static void net_send(NetMsgHeader *header, int16_t type, uint32_t size, int flags) {
    header->type = type;
    header->unk_1 = 0;
    header->faction_id = *FactionIDCurrent;
    header->time = timeGetTime();
    header->sequence = *NetMsgSequence;
    (*NetMsgSendDepth)++;
    NetDaemon_send_message(NetDaemonVar, NULL, header, size, flags);
    (*NetMsgSendDepth)--;
}

/*
Purpose: Send a unit related message with two data values to the other players.
Original Offset: 00592E10
Return Value: n/a
Status: Complete
*/
void __cdecl message_veh(int16_t type, int data1, int data2, int flags) {
    NetMsgVeh msg;
    msg.data[0] = data1;
    msg.data[1] = data2;
    net_send(&msg.header, type, sizeof(msg), flags);
}

/*
Purpose: Send a base related message with a data value and a base name to the other players.
Original Offset: 00592E70
Return Value: n/a
Status: Complete
*/
void __cdecl message_base(int16_t type, int data, LPCSTR name, int flags) {
    NetMsgBase msg;
    memset(msg.name, 0, sizeof(msg.name));
    msg.data = data;
    strncpy_s(msg.name, name, _TRUNCATE);
    net_send(&msg.header, type, sizeof(msg), flags);
}

/*
Purpose: Send a message with four data values to the other players.
Original Offset: 00592EE0
Return Value: n/a
Status: Complete
*/
void __cdecl message_data(int16_t type, int flags, int data1, int data2, int data3, int data4) {
    NetMsgData msg;
    msg.data[0] = data1;
    msg.data[1] = data2;
    msg.data[2] = data3;
    msg.data[3] = data4;
    net_send(&msg.header, type, sizeof(msg), flags);
}

/*
Purpose: Send a message with up to 32 data values to the other players. Unused slots are zeroed.
Original Offset: 00592FF0
Return Value: n/a
Status: Complete
*/
void __cdecl message_big_data(int16_t type, int flags, int *data, int count) {
    NetMsgBigData msg;
    int i = 0;
    for (; i < count && i < 32; i++) {
        msg.data[i] = data[i];
    }
    for (; i < 32; i++) {
        msg.data[i] = 0;
    }
    net_send(&msg.header, type, sizeof(msg), flags);
}

/*
Purpose: Send a lock related message with six data values to the other players. The parameter
         order differs from the on-wire order; this matches the original call signature.
Original Offset: 00593090
Return Value: n/a
Status: Complete
*/
void __cdecl message_lock(int16_t type, int data3, int data1, int data2, int data6, int data4,
                          int data5, int flags) {
    NetMsgLock msg;
    msg.data[0] = data1;
    msg.data[1] = data2;
    msg.data[2] = data3;
    msg.data[3] = data4;
    msg.data[4] = data5;
    msg.data[5] = data6;
    net_send(&msg.header, type, sizeof(msg), flags);
}

/*
Purpose: Send a landmark related message with four data values and an optional landmark name
         to the other players.
Original Offset: 00593110
Return Value: n/a
Status: Complete
*/
void __cdecl message_landmark(int16_t type, int flags, LPCSTR name, int data1, int data2,
                              int data3, int data4) {
    NetMsgLandmark msg;
    memset(msg.name, 0, sizeof(msg.name));
    msg.data[0] = data1;
    msg.data[1] = data2;
    msg.data[2] = data3;
    msg.data[3] = data4;
    if (name) {
        strncpy_s(msg.name, name, _TRUNCATE);
    }
    net_send(&msg.header, type, sizeof(msg), flags);
}

/*
Purpose: Send a chat text message (up to 255 characters) to the other players.
Original Offset: 00593190
Return Value: n/a
Status: Complete
*/
void __cdecl message_chat(int16_t type, int flags, LPCSTR text) {
    NetMsgChat msg;
    memset(msg.text, 0, sizeof(msg.text));
    strncpy_s(msg.text, text, _TRUNCATE);
    net_send(&msg.header, type, sizeof(msg), flags);
}

/*
Common body of the net_*() diplomacy helpers in a network game: log the action, send the message
and optionally block until the matching DIPLO event comes back.
*/
static void net_diplo_send(int16_t type, LPCSTR log_label, int log1, int log2, int log3,
                           int data1, int data2, int data3, int data4, BOOL await) {
    log_say(log_label, log1, log2, log3);
    message_data(type, 0, data1, data2, data3, data4);
    if (await) {
        NetDaemon_await_diplo(NetDaemonVar, NULL, type & 0xFFF);
    }
}

/*
Purpose: Set treaty flags between two factions, relayed over the network in a multiplayer game.
Original Offset: 005391C0
Return Value: n/a
Status: Complete
*/
void __cdecl net_treaty_on(int faction_id, int faction_id_with, int treaty, BOOL await) {
    if (*IsMultiplayerNet) {
        net_diplo_send(NET_MSG_TREATY_ON, "Sending TREATY ON", faction_id, faction_id_with,
            treaty, faction_id, faction_id_with, treaty, 0, await);
    } else {
        treaty_on(faction_id, faction_id_with, treaty);
    }
}

/*
Purpose: Clear treaty flags between two factions, relayed over the network in a multiplayer game.
Original Offset: 00539230
Return Value: n/a
Status: Complete
*/
void __cdecl net_treaty_off(int faction_id, int faction_id_with, int treaty, BOOL await) {
    if (*IsMultiplayerNet) {
        net_diplo_send(NET_MSG_TREATY_OFF, "Sending TREATY OFF", faction_id, faction_id_with,
            treaty, faction_id, faction_id_with, treaty, 0, await);
    } else {
        treaty_off(faction_id, faction_id_with, treaty);
    }
}

/*
Purpose: Set or clear treaty flags between two factions, relayed over the network in a
         multiplayer game.
Original Offset: 005392A0
Return Value: n/a
Status: Complete
*/
void __cdecl net_set_treaty(int faction_id, int faction_id_with, int treaty, BOOL set,
                            BOOL await) {
    if (*IsMultiplayerNet) {
        net_diplo_send(NET_MSG_SET_TREATY, "Sending SET TREATY", faction_id, faction_id_with,
            treaty, faction_id, faction_id_with, treaty, set, await);
    } else {
        set_treaty(faction_id, faction_id_with, treaty, set);
    }
}

/*
Purpose: Set agenda flags between two factions, relayed over the network in a multiplayer game.
Original Offset: 00539310
Return Value: n/a
Status: Complete
*/
void __cdecl net_agenda_on(int faction_id, int faction_id_with, int agenda, BOOL await) {
    if (*IsMultiplayerNet) {
        net_diplo_send(NET_MSG_AGENDA_ON, "Sending AGENDA ON", faction_id, faction_id_with,
            agenda, faction_id, faction_id_with, agenda, 0, await);
    } else {
        agenda_on(faction_id, faction_id_with, agenda);
    }
}

/*
Purpose: Clear agenda flags between two factions, relayed over the network in a multiplayer game.
Original Offset: 00539380
Return Value: n/a
Status: Complete
*/
void __cdecl net_agenda_off(int faction_id, int faction_id_with, int agenda, BOOL await) {
    if (*IsMultiplayerNet) {
        net_diplo_send(NET_MSG_AGENDA_OFF, "Sending AGENDA OFF", faction_id, faction_id_with,
            agenda, faction_id, faction_id_with, agenda, 0, await);
    } else {
        agenda_off(faction_id, faction_id_with, agenda);
    }
}

/*
Purpose: Set or clear agenda flags between two factions, relayed over the network in a
         multiplayer game.
Original Offset: 005393F0
Return Value: n/a
Status: Complete
*/
void __cdecl net_set_agenda(int faction_id, int faction_id_with, int agenda, BOOL set,
                            BOOL await) {
    if (*IsMultiplayerNet) {
        net_diplo_send(NET_MSG_SET_AGENDA, "Sending SET AGENDA", faction_id, faction_id_with,
            agenda, faction_id, faction_id_with, agenda, set, await);
    } else {
        set_agenda(faction_id, faction_id_with, agenda, set);
    }
}

/*
Purpose: Adjust the energy reserves of up to two factions (a transfer or gift), relayed over the
         network in a multiplayer game.
Original Offset: 00539460
Return Value: n/a
Status: Complete
*/
void __cdecl net_energy(int faction_id, int energy, int faction_id_other, int energy_other,
                        BOOL await) {
    if (*IsMultiplayerNet) {
        net_diplo_send(NET_MSG_ENERGY, "Sending ENERGY", faction_id, faction_id_other, energy,
            faction_id, energy, faction_id_other, energy_other, await);
    } else {
        if (faction_id) {
            PlayersData[faction_id].energy_reserves += energy;
        }
        if (faction_id_other) {
            PlayersData[faction_id_other].energy_reserves += energy_other;
        }
        Console_update_data(ConsoleVar, NULL, 1);
    }
}

/*
Purpose: Record a loan one faction owes another, relayed over the network in a multiplayer game.
Original Offset: 00539510
Return Value: n/a
Status: Complete
*/
void __cdecl net_loan(int faction_id, int faction_id_with, int balance, int payment) {
    if (*IsMultiplayerNet) {
        log_say("Sending LOAN", faction_id, faction_id_with, balance);
        message_data(NET_MSG_LOAN, 0, faction_id, faction_id_with, balance, payment);
    } else {
        PlayersData[faction_id].loan_balance[faction_id_with] = balance;
        PlayersData[faction_id].loan_payment[faction_id_with] = payment;
    }
}

/*
Purpose: Exchange maps between two factions, relayed over the network in a multiplayer game.
Original Offset: 00539580
Return Value: n/a
Status: Complete
*/
void __cdecl net_maps(int faction_id, int faction_id_with, BOOL await) {
    if (*IsMultiplayerNet) {
        net_diplo_send(NET_MSG_TRADE_MAPS, "Sending TRADE MAPS", faction_id, faction_id_with, 0,
            faction_id, faction_id_with, 0, 0, await);
    } else {
        trade_maps(faction_id, faction_id_with);
    }
}

/*
Purpose: Give a faction a technology, relayed over the network in a multiplayer game.
Original Offset: 005395F0
Return Value: n/a
Status: Complete
*/
void __cdecl net_tech(int faction_id, int tech_id, int faction_id_other, BOOL await) {
    if (*IsMultiplayerNet) {
        net_diplo_send(NET_MSG_TECH, "Sending TECH", faction_id, tech_id, faction_id_other,
            faction_id, tech_id, faction_id_other, 0, await);
    } else {
        tech_achieved(faction_id, tech_id, faction_id_other, 0);
    }
}

/*
Purpose: End the pact between two factions, relayed over the network in a multiplayer game.
Original Offset: 00539660
Return Value: n/a
Status: Complete
*/
void __cdecl net_pact_ends(int faction_id, int faction_id_with, BOOL await) {
    if (*IsMultiplayerNet) {
        net_diplo_send(NET_MSG_PACT_ENDS, "Sending PACT ENDS", 0, 0, 0,
            faction_id, faction_id_with, 0, 0, await);
    } else {
        pact_ends(faction_id, faction_id_with);
    }
}

/*
Purpose: Withdraw one faction's units from another's territory, relayed over the network in a
         multiplayer game.
Original Offset: 005396D0
Return Value: Whether the withdrawal was carried out (always true for a network game)
Status: Complete
*/
BOOL __cdecl net_withdrawal(int faction_id, int faction_id_from, BOOL await) {
    if (*IsMultiplayerNet) {
        net_diplo_send(NET_MSG_WITHDRAWAL, "Sending WITHDRAWAL", faction_id, faction_id_from, 0,
            faction_id, faction_id_from, 0, 0, await);
        return true;
    }
    return do_withdrawal(faction_id, faction_id_from);
}

/*
Purpose: Cede a base to another faction, relayed over the network in a multiplayer game.
Original Offset: 00539740
Return Value: n/a
Status: Complete
*/
void __cdecl net_cede_base(int base_id, int faction_id_to, BOOL await) {
    if (*IsMultiplayerNet) {
        net_diplo_send(NET_MSG_CEDE_BASE, "Sending CEDE BASE", base_id, faction_id_to, 0,
            base_id, faction_id_to, 0, 0, await);
    } else {
        give_a_base(base_id, faction_id_to);
    }
}

/*
Purpose: Register a betrayal (double cross) between factions, relayed over the network in a
         multiplayer game.
Original Offset: 005397B0
Return Value: n/a
Status: Complete
*/
void __cdecl net_double_cross(int faction_id_atk, int faction_id_def, int faction_id_other,
                              BOOL await) {
    if (*IsMultiplayerNet) {
        net_diplo_send(NET_MSG_DOUBLE_CROSS, "Sending DOUBLE CROSS", faction_id_atk,
            faction_id_def, faction_id_other, faction_id_atk, faction_id_def, faction_id_other, 0,
            await);
    } else {
        double_cross(faction_id_atk, faction_id_def, faction_id_other);
    }
}

/*
Purpose: Acquire the global diplomatic lock from the network daemon. With a non-zero timeout
         (minimum 100) the player is asked whether to keep waiting once it expires.
Original Offset: 00539820
Return Value: Whether the player gave up waiting (true) rather than acquiring the lock (false)
Status: Complete
*/
BOOL __cdecl diplo_lock(int timeout) {
    log_say("Getting DIPLOMATIC LOCK", 0, 0, 0);
    if (timeout && timeout < 100) {
        timeout = 100;
    }
    uint32_t time_start = timeGetTime();
    int attempts = 0;
    while (!*ControlTurnA && NetDaemon_add_global(NetDaemonVar, NULL)) {
        check_spock();
        uint32_t time_now = timeGetTime();
        if (timeout && (time_now - time_start > 9999 || ++attempts > 9999)) {
            if (X_pop("NEEDGLOBAL", NULL)) {
                return true;
            }
            attempts = 0;
            time_start = timeGetTime();
        }
    }
    return false;
}

/*
Purpose: Release the global diplomatic lock held with the network daemon.
Original Offset: 005398C0
Return Value: n/a
Status: Complete
*/
void __cdecl diplo_unlock() {
    log_say("Releasing DIPLOMATIC LOCK", 0, 0, 0);
    NetDaemon_unlock(NetDaemonVar, NULL);
}

/*
Purpose: In a network game, spin in the wait loop until the pending popup is answered.
Original Offset: 005398E0
Return Value: Whether the wait completed (false if the turn control flag aborted it)
Status: Complete
*/
BOOL __cdecl pop_wait() {
    if (*IsMultiplayerNet) {
        if (*ControlTurnA) {
            return false;
        }
        *NetPopWaitActive = true;
        wait_loop();
        *NetPopWaitActive = false;
    }
    return true;
}

/*
Purpose: In a network game, spin in the wait loop until the pending popup is answered, unless
         the game state flag 0x800 is set.
Original Offset: 00539920
Return Value: Whether the wait completed (false if aborted)
Status: Complete
*/
BOOL __cdecl pop_expire() {
    if (*IsMultiplayerNet) {
        if (*ControlTurnA || (*GameState & STATE_UNK_800)) {
            return false;
        }
        *NetPopWaitActive = true;
        wait_loop();
        *NetPopWaitActive = false;
    }
    return true;
}

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
