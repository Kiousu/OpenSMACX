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
#include "faction.h"
#include "general.h"
#include "maininterface.h"

 /*
  * Temp functions and offset pointers. This allows interfacing with parts of the exe not yet
  * decompiled. Eventually, this will be removed.
  */

typedef struct char256 { char str[256]; } char256;
typedef struct char1032 { char str[1032]; } char1032;

// built-in functions
typedef void *func1(size_t);
extern func1 *_malloc;
typedef void *func2(void *);
extern func2 *_free;
typedef FILE *func3(LPCSTR, LPCSTR);
extern func3 *_fopen;
typedef void *func4(unsigned int);
extern func4 *_srand;

typedef int *func10(void *, size_t, size_t, FILE *);
extern func10 *_fread;
extern func10 *_fwrite;
typedef int func17(FILE *);
extern func17 *_fgetc;
typedef int func18(int, FILE *);
extern func18 *_fputc;
//typedef void *func12(void *, size_t);
//extern func12 *_realloc;
//typedef int *func18(FILE *);
//extern func18 *_fclose;
//typedef LPSTR *func19(LPSTR, int, FILE *);
//extern func19 *_fgets;

// other
typedef void func5(int);
extern func5 *load_faction_art;
extern func5 *draw_map;
extern func5 *wave_it;
typedef void *func6(char const *, int(*)(void));
extern func6 *X_pop;
typedef int func7(LPCSTR, LPCSTR, int, LPCSTR, int(__cdecl *)());
extern func7 *popp;
typedef void *func9(void);
extern func9 *fixup_landmarks;
extern func9 *mapwin_terrain_fixup;
extern func9 *world_rainfall;
typedef BOOL func11(int, int, int);
extern func11 *wants_to_attack_;
typedef int func14(int, int);
extern func14 *base_at;
typedef uint32_t func15(LPCSTR);
extern func15 *save_daemon;
typedef uint32_t func16(LPCSTR, BOOL);
extern func16 *load_daemon;
typedef void func19(uint32_t);
extern func19 *social_set;
extern func19 *consider_designs;
typedef int func20(LPCSTR, int, int, LPCSTR, Sprite *);
extern func20 *popb;
typedef void func21(int, int, int, int);
extern func21 *draw_radius;
typedef void func22(int, int, uint32_t);
extern func22 *alt_set;

// Time
typedef void func30(int);
extern func30 *blink_timer;
extern func30 *blink2_timer;
extern func30 *line_timer;
extern func30 *turn_timer;

// testing
typedef int *func8(LPSTR, LPSTR);
extern func8 *parse_string_OG;
typedef void func12(int);
extern func12 *enemy_capabilities_OG;
///
typedef int funct5(int, uint32_t, uint32_t, uint32_t, BOOL);
extern funct5 *value_of_base_OG;
typedef uint32_t funct4(uint32_t, int, BOOL);
extern funct4 *best_defender_OG;
typedef void func_battle(int, int, int *, int *, int);
extern func_battle *battle_compute_OG;
typedef BOOL func_wants(uint32_t, uint32_t, int);
extern func_wants *wants_to_attack_OG;
typedef BOOL func_bool_void();
extern func_bool_void *world_validate_OG;
typedef void func_diplo3u(int, int, uint32_t);
extern func_diplo3u *treaty_on_OG;
extern func_diplo3u *treaty_off_OG;
extern func_diplo3u *agenda_on_OG;
extern func_diplo3u *agenda_off_OG;
typedef void func_diplo2(int, int);
extern func_diplo2 *trade_maps_OG;
typedef int func_diplo2r(int, int);
extern func_diplo2r *do_withdrawal_OG;
//
extern char1032 *stringTemp;
extern char256 *ParseStrBuffer;

extern int *BufferStrHeight;
extern int *ParseNumTable;
extern int *ParseStrPlurality;
extern int *ParseStrGender;
extern HWND *HandleMain;

extern uint32_t *UnkBitfield1;

extern Filefind *FilefindPath;
extern MainInterface *MainInterfaceVar;

DLLEXPORT int __cdecl tester();

// tech_val output
typedef int func13(int, int, BOOL);
extern func13 *tech_val_OG;
DLLEXPORT void __cdecl tech_calc_output();

// message handling testing
typedef void *func_msg(void);
extern func_msg *do_video;
extern func_msg *check_net;
extern func_msg *do_net;
extern func_msg *do_non_input_;

extern uint32_t *MsgStatus;

// net
// __thiscall emulated via __fastcall: 'This' goes in ecx, 'edx_unused' is a placeholder for edx
typedef void __fastcall func_net_synch(void *This, void *edx_unused, int16_t type, int id,
    int id2, int unk, char *data, uint32_t delay, int16_t unk2);
extern func_net_synch *NetDaemon_synch; // NetDaemon::synch()
extern void *NetDaemonVar; // global NetDaemon instance
typedef void __fastcall func_net_send_msg(void *This, void *edx_unused, void *msg, uint32_t size,
    int flags);
extern func_net_send_msg *NetDaemon_send_message; // NetDaemon::send_message()
typedef void __fastcall func_net_await(void *This, void *edx_unused, int msg_code);
extern func_net_await *NetDaemon_await_diplo; // NetDaemon::await_diplo()
typedef int __fastcall func_net_this_only(void *This, void *edx_unused);
extern func_net_this_only *NetDaemon_add_global; // NetDaemon::add_global()
extern func_net_this_only *NetDaemon_unlock; // NetDaemon::unlock()
typedef void __fastcall func_console_update(void *This, void *edx_unused, int unk);
extern func_console_update *Console_update_data; // Console::update_data()
extern void *ConsoleVar; // global Console instance
typedef void func_diplo3(int, int, int);
extern func_diplo3 *double_cross;
extern func5 *ambience; // ambience(sound_id)
typedef void __fastcall func_win_redraw(void *This, void *edx_unused);
extern func_win_redraw *GraphicWin_redraw; // GraphicWin::redraw()
extern void *MultiWinVar; // global MultiWin instance (diplomacy/commlink window)
extern int *MultiWinFlag; // non-zero while MultiWin needs redrawing on treaty changes
extern uint32_t *AmbienceFlags; // bit 0: war ambience playing
typedef void func_diplo2(int, int);
extern func_diplo2 *pact_ends;
extern func_diplo2 *give_a_base;
extern func_diplo2 *spot_base; // spot_base(base_id, faction_id)
typedef void func_draw3(int, int, int);
extern func_draw3 *draw_tiles; // draw_tiles(x, y, mode)
typedef void func_draw4(int, int, int, int);
extern func_draw4 *draw_tile_fixup; // draw_tile_fixup(x, y, unk, mode)
extern void *WorldWinVar; // global WorldWin instance (lower right world map view)
extern BOOL *TradeMapsRevealedNew; // set by trade_maps() and friends when new tiles were revealed
typedef void func_tech_ach(int, int, int, int);
extern func_tech_ach *tech_achieved;
extern func9 *wait_loop;
extern func9 *check_spock;
extern int *FactionIDCurrent; // faction of the player currently at the controls (MapWin owner)
extern int *NetMsgSequence; // incremented for each message sent by NetDaemon
extern int *NetMsgSendDepth; // non-zero while a message is being sent
extern BOOL *NetPopWaitActive; // set while pop_wait()/pop_expire() spin in wait_loop()
extern int *ControlTurnA; // name from Thinker; non-zero aborts network waits

DLLEXPORT BOOL __cdecl do_non_input();
DLLEXPORT void __cdecl do_all_non_input();
DLLEXPORT BOOL __cdecl do_draw();
DLLEXPORT void __cdecl do_all_draws();
DLLEXPORT BOOL __cdecl do_keyboard();
DLLEXPORT void __cdecl do_all_keyboard();
