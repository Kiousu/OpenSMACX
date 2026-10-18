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
typedef int func_rand(void);
extern func_rand *_rand; // the exe's CRT rand(); code still in the exe calls it inline

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
extern func5 *veh_kill;
extern func5 *kill; // kill(veh_id): veh_kill() plus stack and interface cleanup
extern func5 *base_kill; // base_kill(base_id)
typedef void *func6(char const *, int(*)(void));
extern func6 *X_pop;
typedef int func7(LPCSTR, LPCSTR, int, LPCSTR, int(__cdecl *)());
extern func7 *popp;
typedef void *func9(void);
typedef BOOL func11(int, int, int);
extern func11 *wants_to_attack_;
typedef int func14(int, int);
extern func14 *eliminate_player; // eliminate_player(faction_id, faction_id_victor)
typedef uint32_t func15(LPCSTR);
extern func15 *save_daemon;
typedef uint32_t func16(LPCSTR, BOOL);
extern func16 *load_daemon;
typedef void func19(uint32_t);
extern func19 *consider_designs_OG;
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
typedef void func_void(void);
typedef void func_u32(uint32_t);
typedef void func_xy(int, int);
typedef void func_xyb(int, int, BOOL);
typedef void func_xyi(int, int, int);
typedef void func_xyib(int, int, int, BOOL);
typedef void func_xyuu(int, int, uint32_t, uint32_t);
typedef void func_social_ai(uint32_t, int, int, int, int, SocialCategory *);
typedef BOOL __fastcall func_path_sensors(void *This, void *edx_unused, uint32_t faction_id,
    int *x_sensor, int *y_sensor);
extern func_u32 *base_mark_OG;
// VEH helper batch
typedef BOOL func_veh_pred(int);
extern func_veh_pred *veh_selectable_OG;
extern func_veh_pred *veh_unmoved_OG;
extern func_veh_pred *veh_ready_OG;
typedef int func_veh_find(int, int, int, int);
extern func_veh_find *veh_find_OG;
typedef BOOL func_bunged(uint32_t, uint32_t, uint32_t, uint32_t, int, uint32_t);
extern func_bunged *is_bunged_OG;
extern func_u32 *clear_bunglist_OG;
typedef void func_add_abil(uint32_t *, uint32_t, uint32_t);
extern func_add_abil *add_abil_OG;
extern func5 *unspot_stack_OG;
extern func5 *stack_kill_OG;
typedef int func_rating(uint32_t, int);
extern func_rating *get_rating_OG;
typedef int func_most_obj(int *, int *);
extern func_most_obj *most_objectives_OG;
// batch 2
typedef BOOL func_abil_cond(int, int, int, int);
extern func_abil_cond *abil_cond_OG;
typedef void func_bung_proto(uint32_t, int);
extern func_bung_proto *bung_proto_OG;
extern func_diplo2 *spot_base_OG;
extern func_diplo2 *spot_stack_OG;
extern func_diplo2 *retire_proto_OG;
extern func_veh_pred *study_artifact_OG;
// batch 3
typedef int func_iii(int, int, int);
extern func14 *base_at_OG;
extern func_iii *upgrade_cost_OG;
extern func_xyi *do_upgrade_OG;
extern func_diplo2 *upgrade_prototypes_OG;
extern func5 *upgrade_any_prototypes_OG;
typedef BOOL func_claimed(int, int, int, int, int, int);
extern func_claimed *base_claimed_OG;
extern func_iii *site_at_OG;
extern func_veh_pred *spying_OG;
extern func_diplo2r *act_of_aggression_OG;
extern func_iii *council_get_vote_OG;
extern func_diplo2r *can_call_council_OG;
typedef int func_i(int);
extern func_i *pick_top_veh_OG;
extern func_diplo2r *can_convoy_OG;
extern func5 *promote_OG;
extern func_u32 *proto_sort_OG;
extern func_u32 *proto_sort_2_OG;
typedef void func_str(LPSTR);
extern func_str *yearmotize_OG;
// batch 4
extern func_void *world_rocky_OG;
extern func_void *world_fungus_OG;
extern func_void *world_shorelines_OG;
extern func_void *world_rainfall_OG;
// batch 5
extern func_i *tech_advance_OG;
extern func_xy *tech_research_OG;
extern func_iii *steal_tech_OG;
// batch 6
typedef int func_num_obj(uint32_t, BOOL);
extern func_num_obj *num_objectives_OG;
typedef BOOL func_bkill(int, int *, int *, int *, int, int);
typedef void func_bkill_n(int, int *, int *, int *, int, int);
extern func_bkill *battle_kill_OG;
extern func_bkill_n *battle_kill_stack_OG;
extern func_bkill_n *battle_kill_chaff_OG;
extern func_void *clear_monuments_OG;
extern func_xy *mon_tech_discovered_OG;
extern func_u32 *social_set_OG;
extern func_xyi *bases_reset_OG;
// batch 7
typedef int func_yield5(uint32_t, int, int, int, BOOL);
extern func_yield5 *crop_yield_OG;
extern func_yield5 *mine_yield_OG;
extern func_yield5 *energy_yield_OG;
typedef int func_rsc_yield(uint32_t, uint32_t, int, int, int);
extern func_rsc_yield *resource_yield_OG;
// batch 8
extern func_iii *world_site_OG;
typedef BOOL func_uii(uint32_t, int, int);
extern func_uii *good_sensor_OG;
extern func_xyi *spot_loc_OG;
typedef int func_territory(uint32_t, uint32_t, uint8_t, int *, int *);
extern func_territory *territory_OG;
// batch 9
typedef int func_uu(uint32_t, uint32_t);
extern func_uu *suggest_plan_OG;
typedef void func_uu_v(uint32_t, uint32_t);
extern func_uu_v *scan_prototypes_OG;
// batch 10
typedef void func_set_course(int, uint8_t, int, int);
extern func_set_course *set_course_OG;
extern func_xy *enemies_trade_tech_OG;
typedef void func_iu(int, uint32_t);
extern func_iu *tech_analysis_OG;
// batch 11
typedef void func_b(BOOL);
extern func_b *rankings_OG;
typedef uint32_t func_stack_veh(uint32_t, int);
extern func_stack_veh *stack_veh_OG;
// batch 12
extern func_void *reset_territory_OG;
extern func_void *base_check_support_OG;
extern func_void *base_research_OG;
// batch 13
extern func_void *farm_compute_OG;
extern func_void *base_support_OG;
// batch 14
extern func_void *base_yield_OG;
extern func_void *base_energy_OG;
extern func_void *base_minerals_OG;
// batch 15
extern func_rand *base_growth_OG;
extern func_void *base_doctors_OG;
// batch 16
typedef LPSTR func_s_i(int);
typedef LPSTR func_s_ii(int, int);
extern func_s_i *get_name_OG;
extern func_s_i *get_title_OG;
extern func_s_i *get_adjective_leader_OG;
extern func_s_i *get_adjective_insult_leader_OG;
extern func_s_i *get_adjective_faction_OG;
extern func_s_i *get_adjective_insult_faction_OG;
extern func_s_i *get_insult_leader_OG;
extern func_s_i *get_pact_OG;
extern func_s_i *get_pacts_OG;
extern func_s_ii *get_pacts_2_OG;
extern func_s_ii *get_pact_hood_OG;
extern func_s_ii *get_his_her_OG;
extern func_s_ii *get_him_her_OG;
extern func_s_ii *get_he_she_OG;
extern func_s_ii *get_phrase_OG;
extern func_s_ii *get_noun_phrase_OG;
extern func_i *mandate_color_OG;
typedef BOOL func_in_box(int, int, int, int, int, int);
extern func_in_box *in_box_OG;
typedef BOOL func_in_box_2(int, int, RECT *);
extern func_in_box_2 *in_box_2_OG;
extern func_void *clear_OG;
extern func_void *space_OG;
extern func_b *base_compute_OG;
typedef int func_generators(uint32_t, int *);
extern func_generators *generators_OG;
// batch 17
typedef int func_contiguous(int, int, int, int);
extern func_contiguous *contiguous; // contiguous(faction_id, faction_id_2, region, unk)
extern func_iii *alien_base_OG;
typedef void func_assemble(int, int, int);
extern func_assemble *assemble_passengers_OG;
// batch 18
extern func5 *repair_phase_OG;
extern func5 *allocate_energy_OG;
extern func5 *social_select; // social_select(faction_id): the social engineering screen
// batch 19
extern func_xy *spot_all; // spot_all(veh_id, radius): reveal around a unit
extern func_void *set_dirty_OG;
extern func_void *clear_dirty_OG;
extern func_xy *world_UNK1_OG;
extern func_void *do_fungal_towers_OG;
// batch 20
extern func_void *drone_riot; // drone_riot(): riot effects for the current base
extern func_void *base_drones_OG;
// batch 21
typedef void func_mon_s(int, LPCSTR);
extern func_mon_s *mon_colony_founded_OG;
extern func5 *mon_secrets_of_tech_OG;
extern func5 *mon_prototype_built_OG;
extern func_mon_s *mon_facility_built_OG;
extern func_diplo2 *mon_secret_project_OG;
extern func_diplo2 *mon_enemy_destroyed_OG;
extern func_mon_s *mon_conquer_base_OG;
extern func5 *mon_naval_unit_built_OG;
extern func5 *mon_air_unit_built_OG;
extern func5 *mon_native_life_bred_OG;
extern func5 *mon_first_in_space_OG;
extern func5 *mon_built_preserve_OG;
extern func_diplo2 *mon_killed_faction_OG;
// batch 22
extern func_iii *valid_patrol_OG;
extern func_diplo2 *energy_compute_OG;
// batch 23
typedef void func_name_proto(LPSTR, int, int, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
extern func_name_proto *name_proto_OG;
typedef int func_propose_proto(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int,
    LPCSTR);
extern func_propose_proto *propose_proto_OG;
typedef void func_prune_protos(int, int, int);
extern func_prune_protos *prune_protos_OG;
// batch 24
typedef int func_can_terraform(int, int, int, BOOL, int);
extern func_can_terraform *can_terraform_OG;
// batch 25
extern func_void *balance_OG;
// batch 26
typedef BOOL func_upgrade_proto(int, int, int, BOOL);
extern func_upgrade_proto *upgrade_prototype_OG;
typedef int __fastcall func_net_receive(void *This, void *edx_unused);
extern func_net_receive *NetDaemon_receive; // NetDaemon::receive(): non-zero while messages wait
// batch 27
typedef int func_veh_init(int, uint32_t, int, int);
extern func_veh_init *veh_init_OG;
extern func_void *alien_start_OG;
extern func_void *base_psych_OG;
extern func_social_ai *social_ai_OG;
extern func_path_sensors *Path_sensors_OG; // Path::sensors()
extern func_xyib *world_alt_set_OG;
extern func_xy *world_raise_alt_OG;
extern func_xy *world_lower_alt_OG;
extern func_xyi *brush_OG;
extern func_xyuu *paint_land_OG;
extern func_u32 *build_continent_OG;
extern func5 *build_hills_OG;
extern func_void *world_riverbeds_OG;
extern func_void *world_temperature_OG;
extern func_void *world_analysis_OG;
extern func_void *world_polar_caps_OG;
extern func_void *world_linearize_contours_OG;
extern func_xy *world_crater_OG;
extern func_xy *world_monsoon_OG;
extern func_xy *world_sargasso_OG;
extern func_xy *world_ruin_OG;
extern func_xy *world_dune_OG;
extern func_xy *world_diamond_OG;
extern func_xy *world_fresh_OG;
extern func_xyb *world_volcano_OG;
extern func_xy *world_borehole_OG;
extern func_xy *world_temple_OG;
extern func_xy *world_unity_OG;
extern func_xy *world_fossil_OG;
extern func_xy *world_canyon_OG;
extern func_xy *world_mesa_OG;
extern func_xy *world_ridge_OG;
extern func_xy *world_geothermal_OG;
extern func_void *world_landmarks_OG;
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
typedef int __fastcall func_net_lock_base(void *This, void *edx_unused, int base_id, int lock_flags,
    int unk1, int unk2);
extern func_net_lock_base *NetDaemon_lock_base; // NetDaemon::lock_base(): non-zero if not locked
typedef void __fastcall func_net_unlock_base(void *This, void *edx_unused, int base_id);
extern func_net_unlock_base *NetDaemon_unlock_base; // NetDaemon::unlock_base()
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
typedef void func_draw3(int, int, int);
extern func_draw3 *draw_tiles; // draw_tiles(x, y, mode)
typedef void func_draw4(int, int, int, int);
extern func_draw4 *draw_tile_fixup; // draw_tile_fixup(x, y, unk, mode)
extern func_draw3 *draw_tile; // draw_tile(x, y, mode)
typedef int __fastcall func_mapwin_focus(void *This, void *edx_unused, int x, int y);
extern func_mapwin_focus *MapWin_focus; // MapWin::focus(x, y): centre a map view on the tile; callers
                                        // redraw the tile themselves when it returns 0
typedef int func_pop2(LPCSTR, LPCSTR, int);
extern func_pop2 *POP2; // POP2(label, pcx_file, unk): message popup with a picture
extern func5 *do_checksums; // do_checksums(when): logs the network game checksums when logging is on
extern func_win_redraw *WorldWin_clear_terrain; // WorldWin::clear_terrain() on WorldWinVar
typedef void __fastcall func_fx_play(void *This, void *edx_unused, int fx_id);
extern func_fx_play *FX_play; // FX::play()
extern void *FXVar; // global FX instance
typedef int __fastcall func_netmsg_pop(void *This, void *edx_unused, LPCSTR label, int delay,
    int flags, LPCSTR pcx_file);
extern func_netmsg_pop *NetMsg_pop; // NetMsg::pop()
extern void *NetMsgVar; // global NetMsg instance
extern int *BaseIDUnk; // never written by the exe; compared against base ids in study_artifact(),
                       // order_veh(), capture_base(), enemy_move() and Console::go_to()
extern func_iii *wants_prop; // wants_prop(faction_id, proposal, faction_id_proposer)
typedef int __fastcall func_console_focus(void *This, void *edx_unused, int x, int y, int faction_id);
extern func_console_focus *Console_focus; // Console::focus()
extern BOOL *IsLoggingActive; // set by net_end_of_turn() when the per-faction log file could be opened
extern int *DialogToggle; // Thinker's name; tested before the GetAsyncKeyState() call in base_at()
extern int *CouncilSessionPending; // hotseat/PBEM: a council session waits for the other players
extern int *VehDrawBoomID; // unit being blown up by boom(); never picked as the top of a stack
extern int *MapWinVehIDSelected; // Console::iUnit, the unit selected in the map window
extern BOOL *MapWinUnitNotViewMode; // Console field after the owner (Thinker: fUnitNotViewMode)
extern int *MapWinUnkFlag; // Console field +0x23BE4, checked by pick_top_veh()
extern int *VehUpgradeUnk; // zeroed by upgrade_any_prototypes(), set by upgrade_prototype()
extern func_win_redraw *MapWin_clear_terrain; // MapWin::clear_terrain() on ConsoleVar
typedef void __fastcall func_mapwin_clear_alt(void *This, void *edx_unused, int x, int y, int radius);
extern func_mapwin_clear_alt *MapWin_clear_alt; // MapWin::clear_alt(): resets cached heights within
                                               // the radius (index into RadiusRange)
typedef void func_replay_base(int, int, int, int);
extern func_replay_base *replay_base; // replay_base(event, x, y, faction_id): 8 byte replay record entry
typedef int func_battle_fight_2(int, int, int, int, BOOL, int, int);
extern func_battle_fight_2 *battle_fight_2; // battle_fight_2(veh_id, offset, x, y, is_arty, unk1, unk2)
extern func_i *tech_selection; // tech_selection(faction_id): pick the next research goal (dialog for humans)
typedef int func_tech_pick(int, int, int, LPCSTR);
extern func_tech_pick *tech_pick; // tech_pick(faction_id, unk, faction_id_from, label)
extern int *SkipTechScreenA; // Thinker's names: non-zero skips the tech popups
extern int *SkipTechScreenB;
extern int *NetUpkeepState; // 1 while a network game runs its upkeep
typedef int func_base_build(int, int, int, int *);
extern func_base_build *base_build; // base_build(base_id, unk1, unk2, value_out): AI production
                                    // pick; -1 - base_id leaves Secret Projects out
extern func_xy *help_topic; // help_topic(topic, id): the Datalinks entry
extern func5 *monument; // monument(unk): the monument screen
typedef void func_interlude(int, LPCSTR, int, int);
extern func_interlude *interlude; // interlude(event_id, text, unk1, unk2)
extern void *BaseWinVar; // global BaseWin instance
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
// commlink_attempter(faction_id, faction_id_with): non-zero when contact can't be made now
extern func_diplo2r *commlink_attempter;
// wants_to_speak(faction_id, faction_id_with): builds a Popup on the stack (left in the exe)
extern func_diplo2r *wants_to_speak;
extern func_console_update *Console_set_move; // Console::set_move() on ConsoleVar
extern uint32_t *InfoWinUnk; // Thinker: dword_7AD330 (InfoWin); cleared after a tech sale
extern func_xyi *diplomacy_check; // diplomacy_check(faction_id, faction_id_with, unk)
// diplomacy_caption(faction_id, faction_id_with): the mood caption, into parse slot 7
extern func_diplo2 *diplomacy_caption;
// tut_check2(flag): true once per flag while tutorial messages are on
extern func_i *tut_check2;
typedef int func_x_pops_11(LPCSTR, int, Sprite *, int(__cdecl *)());
extern func_x_pops_11 *X_pops_11; // X_pops_11(label, flags, picture, wait_callback)
typedef int func_x_pop_2(LPCSTR, LPCSTR, int(__cdecl *)());
extern func_x_pop_2 *X_pop_2; // X_pop_2(file, label, callback)
extern Sprite **FactionPortraits; // [8] Thinker's name

DLLEXPORT BOOL __cdecl do_non_input();
DLLEXPORT void __cdecl do_all_non_input();
DLLEXPORT BOOL __cdecl do_draw();
DLLEXPORT void __cdecl do_all_draws();
DLLEXPORT BOOL __cdecl do_keyboard();
DLLEXPORT void __cdecl do_all_keyboard();
