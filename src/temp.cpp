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
#include "log.h"
#include "game.h"
#include "veh.h"
#include "technology.h"
#include "path.h"
#include "map.h"
#include "base.h"
#include "alpha.h"

// built-in functions > used to prevent crash from mixed alloc/free SDKs
func1 *_malloc = (func1 *)0x006470A6;
func2 *_free = (func2 *)0x00644EF2;
func3 *_fopen = (func3 *)0x00645646;
func4 *_srand = (func4 *)0x00646010;
func_rand *_rand = (func_rand *)0x0064601D;
func10 *_fread = (func10 *)0x00646178;
func10 *_fwrite = (func10 *)0x0064603F;
func17 *_fgetc = (func17 *)0x00646855;
func18 *_fputc = (func18 *)0x00646886;
//func12 *_realloc = (func12 *)0x00647132;
//func18 *_fclose = (func18 *)0x00645598;
//func19 *_fgets = (func19 *)0x0064726A;

// other
func5 *load_faction_art = (func5 *)0x00453710;
func5 *draw_map = (func5 *)0x0046B190;
func5 *wave_it = (func5 *)0x004455F0;
func5 *veh_kill = (func5 *)0x005C08C0;
func5 *kill = (func5 *)0x005C0B00;
func5 *base_kill = (func5 *)0x004E5250;
func14 *eliminate_player = (func14 *)0x005B3380;
// TODO: crash bug; X_pop > . > BasePop::start > text_close > NULLs 009B7CF4 (FILE *Txt.text_file_)
// Next call to text_get() tries to access 009B7CF4 and the game crashes.
func6 *X_pop = (func6 *)0x005BF310;
func7 *popp = (func7 *)0x0048C0A0;
func9 *fixup_landmarks = (func9 *)0x00592940;
func9 *mapwin_terrain_fixup = (func9 *)0x00471240;
func11 *wants_to_attack_ = (func11 *)0x0055BC80;
func15 *save_daemon = (func15 *)0x005A94F0;
func16 *load_daemon = (func16 *)0x005A9760;
func19 *consider_designs = (func19 *)0x00581260;
func20 *popb = (func20 *)0x0048C650;
func21 *draw_radius = (func21 *)0x0046AEF0;
func22 *alt_set = (func22 *)0x00591290;

// Time
func30 *blink_timer = (func30 *)0x0050EA40;
func30 *blink2_timer = (func30 *)0x0050EE30;
func30 *line_timer = (func30 *)0x0050EE80;
func30 *turn_timer = (func30 *)0x0050EF10;

// testing
func8 *parse_string_OG = (func8 *)0x00625880;
func12 *enemy_capabilities_OG = (func12 *)0x00560DD0;
//
funct5 *value_of_base_OG = (funct5 *)0x0054CB50;
funct4 *best_defender_OG = (funct4 *)0x005044D0;
func_battle *battle_compute_OG = (func_battle *)0x00501DA0;
func_wants *wants_to_attack_OG = (func_wants *)0x0055BC80;
func_bool_void *world_validate_OG = (func_bool_void *)0x005C40F0;
func_diplo3u *treaty_on_OG = (func_diplo3u *)0x0055B870;
func_diplo3u *treaty_off_OG = (func_diplo3u *)0x0055B760;
func_diplo3u *agenda_on_OG = (func_diplo3u *)0x0055BA80;
func_diplo3u *agenda_off_OG = (func_diplo3u *)0x0055B820;
func_diplo2 *trade_maps_OG = (func_diplo2 *)0x00543A20;
func_diplo2r *do_withdrawal_OG = (func_diplo2r *)0x00548F80;
func_u32 *base_mark_OG = (func_u32 *)0x004E4350;
func_veh_pred *veh_selectable_OG = (func_veh_pred *)0x005C1A20;
func_veh_pred *veh_unmoved_OG = (func_veh_pred *)0x005C1AB0;
func_veh_pred *veh_ready_OG = (func_veh_pred *)0x005C1B70;
func_veh_find *veh_find_OG = (func_veh_find *)0x005C0CB0;
func_bunged *is_bunged_OG = (func_bunged *)0x0057DFC0;
func_u32 *clear_bunglist_OG = (func_u32 *)0x0057DF00;
func_add_abil *add_abil_OG = (func_add_abil *)0x00581190;
func5 *unspot_stack_OG = (func5 *)0x005B5A70;
func5 *stack_kill_OG = (func5 *)0x005B9510;
func_rating *get_rating_OG = (func_rating *)0x005ABFF0;
func_most_obj *most_objectives_OG = (func_most_obj *)0x005AC5A0;
func_abil_cond *abil_cond_OG = (func_abil_cond *)0x00584B50;
func_bung_proto *bung_proto_OG = (func_bung_proto *)0x0057DF30;
func_diplo2 *spot_base_OG = (func_diplo2 *)0x005B57D0;
func_diplo2 *spot_stack_OG = (func_diplo2 *)0x005B58E0;
func_diplo2 *retire_proto_OG = (func_diplo2 *)0x0057F1D0;
func_veh_pred *study_artifact_OG = (func_veh_pred *)0x0057CFA0;
func14 *base_at_OG = (func14 *)0x004E3A50;
func_iii *upgrade_cost_OG = (func_iii *)0x004EFD50;
func_xyi *do_upgrade_OG = (func_xyi *)0x004EFB50;
func_diplo2 *upgrade_prototypes_OG = (func_diplo2 *)0x004F0460;
func5 *upgrade_any_prototypes_OG = (func5 *)0x004F0650;
func_claimed *base_claimed_OG = (func_claimed *)0x004F8110;
func_iii *site_at_OG = (func_iii *)0x005797D0;
func_veh_pred *spying_OG = (func_veh_pred *)0x0055BC00;
func_diplo2r *act_of_aggression_OG = (func_diplo2r *)0x0050C2E0;
func_iii *council_get_vote_OG = (func_iii *)0x0052BE60;
func_diplo2r *can_call_council_OG = (func_diplo2r *)0x0052C670;
func_i *pick_top_veh_OG = (func_i *)0x005593E0;
func_diplo2r *can_convoy_OG = (func_diplo2r *)0x00564D90;
func5 *promote_OG = (func5 *)0x005062B0;
func_u32 *proto_sort_OG = (func_u32 *)0x005A6270;
func_u32 *proto_sort_2_OG = (func_u32 *)0x005A63D0;
func_str *yearmotize_OG = (func_str *)0x005A9DB0;
func_void *world_rocky_OG = (func_void *)0x005C32A0;
func_void *world_fungus_OG = (func_void *)0x005C3440;
func_void *world_shorelines_OG = (func_void *)0x005C3F70;
func_void *world_rainfall_OG = (func_void *)0x005C4470;
func_i *tech_advance_OG = (func_i *)0x005BE530;
func_xy *tech_research_OG = (func_xy *)0x005BE940;
func_iii *steal_tech_OG = (func_iii *)0x0050C340;
func_num_obj *num_objectives_OG = (func_num_obj *)0x005AC110;
func_bkill *battle_kill_OG = (func_bkill *)0x00505D80;
func_bkill_n *battle_kill_stack_OG = (func_bkill_n *)0x00506130;
func_bkill_n *battle_kill_chaff_OG = (func_bkill_n *)0x005061B0;
func_void *clear_monuments_OG = (func_void *)0x00476B30;
func_xy *mon_tech_discovered_OG = (func_xy *)0x00476C90;
func_u32 *social_set_OG = (func_u32 *)0x005B4600;
func_xyi *bases_reset_OG = (func_xyi *)0x004E6300;
func_yield5 *crop_yield_OG = (func_yield5 *)0x004E6E50;
func_yield5 *mine_yield_OG = (func_yield5 *)0x004E7310;
func_yield5 *energy_yield_OG = (func_yield5 *)0x004E7750;
func_rsc_yield *resource_yield_OG = (func_rsc_yield *)0x004E7DC0;
func_iii *world_site_OG = (func_iii *)0x005C4FD0;
func_uii *good_sensor_OG = (func_uii *)0x00564EB0;
func_xyi *spot_loc_OG = (func_xyi *)0x005B5AD0;
func_territory *territory_OG = (func_territory *)0x0055EB80;
func_uu *suggest_plan_OG = (func_uu *)0x0054ACC0;
func_uu_v *scan_prototypes_OG = (func_uu_v *)0x0053A4A0;
func_set_course *set_course_OG = (func_set_course *)0x00564890;
func_xy *enemies_trade_tech_OG = (func_xy *)0x0055D430;
func_iu *tech_analysis_OG = (func_iu *)0x0053E800;
func_b *rankings_OG = (func_b *)0x005AC690;
func_stack_veh *stack_veh_OG = (func_stack_veh *)0x005B8EE0;
func_void *reset_territory_OG = (func_void *)0x00523DD0;
func_void *base_check_support_OG = (func_void *)0x004F4830;
func_void *base_research_OG = (func_void *)0x004F4E80;
func_void *farm_compute_OG = (func_void *)0x004E68B0;
func_void *base_support_OG = (func_void *)0x004E9550;
func_void *base_yield_OG = (func_void *)0x004E80B0;
func_void *base_energy_OG = (func_void *)0x004EB560;
func_void *base_minerals_OG = (func_void *)0x004E9CB0;
func_rand *base_growth_OG = (func_rand *)0x004EF1C0;
func_void *base_doctors_OG = (func_void *)0x004F5F30;
func_s_i *get_name_OG = (func_s_i *)0x00559060;
func_s_i *get_title_OG = (func_s_i *)0x005590A0;
func_s_i *get_adjective_leader_OG = (func_s_i *)0x005590E0;
func_s_i *get_adjective_insult_leader_OG = (func_s_i *)0x00559100;
func_s_i *get_adjective_faction_OG = (func_s_i *)0x00559120;
func_s_i *get_adjective_insult_faction_OG = (func_s_i *)0x00559140;
func_s_i *get_insult_leader_OG = (func_s_i *)0x00559160;
func_s_i *get_pact_OG = (func_s_i *)0x0058F9D0;
func_s_i *get_pacts_OG = (func_s_i *)0x0058FA20;
func_s_ii *get_pacts_2_OG = (func_s_ii *)0x0058FA70;
func_s_ii *get_pact_hood_OG = (func_s_ii *)0x0058FB00;
func_s_ii *get_his_her_OG = (func_s_ii *)0x0058FBC0;
func_s_ii *get_him_her_OG = (func_s_ii *)0x0058FC50;
func_s_ii *get_he_she_OG = (func_s_ii *)0x0058FCE0;
func_s_ii *get_phrase_OG = (func_s_ii *)0x0058F810;
func_s_ii *get_noun_phrase_OG = (func_s_ii *)0x0058F8B0;
func_i *mandate_color_OG = (func_i *)0x0050E820;
func_in_box *in_box_OG = (func_in_box *)0x005FA7A0;
func_in_box_2 *in_box_2_OG = (func_in_box_2 *)0x005FA7E0;
func_void *clear_OG = (func_void *)0x00500120;
func_void *space_OG = (func_void *)0x005A58B0;
func_b *base_compute_OG = (func_b *)0x004EC3B0;
func_generators *generators_OG = (func_generators *)0x005247B0;
func_contiguous *contiguous = (func_contiguous *)0x0053A780;
func_iii *alien_base_OG = (func_iii *)0x005665D0;
func_assemble *assemble_passengers_OG = (func_assemble *)0x00564B90;
func5 *repair_phase_OG = (func5 *)0x00526030;
func5 *allocate_energy_OG = (func5 *)0x005267B0;
func_i *energy_limit = (func_i *)0x00445440;
func5 *social_select = (func5 *)0x005B5620;
func_xy *spot_all = (func_xy *)0x005B6260;
func_void *set_dirty_OG = (func_void *)0x00592A80;
func_void *clear_dirty_OG = (func_void *)0x00592AF0;
func_xy *world_UNK1_OG = (func_xy *)0x005C4410;
func_void *do_fungal_towers_OG = (func_void *)0x00522F00;
func_void *drone_riot = (func_void *)0x004F4FB0;
func_void *base_drones_OG = (func_void *)0x004F5CE0;
func_mon_s *mon_colony_founded_OG = (func_mon_s *)0x00476B70;
func5 *mon_secrets_of_tech_OG = (func5 *)0x00476DA0;
func5 *mon_prototype_built_OG = (func5 *)0x00476EE0;
func_mon_s *mon_facility_built_OG = (func_mon_s *)0x00476FE0;
func_diplo2 *mon_secret_project_OG = (func_diplo2 *)0x00477100;
func_diplo2 *mon_enemy_destroyed_OG = (func_diplo2 *)0x00477210;
func_mon_s *mon_conquer_base_OG = (func_mon_s *)0x00477320;
func5 *mon_naval_unit_built_OG = (func5 *)0x00477440;
func5 *mon_air_unit_built_OG = (func5 *)0x00477540;
func5 *mon_native_life_bred_OG = (func5 *)0x00477640;
func5 *mon_first_in_space_OG = (func5 *)0x00477740;
func5 *mon_built_preserve_OG = (func5 *)0x00477840;
func_diplo2 *mon_killed_faction_OG = (func_diplo2 *)0x00477A60;
func_iii *valid_patrol_OG = (func_iii *)0x004CD6A0;
func_diplo2 *energy_compute_OG = (func_diplo2 *)0x00445130;
func_name_proto *name_proto_OG = (func_name_proto *)0x0057E040;
func_propose_proto *propose_proto_OG = (func_propose_proto *)0x00580860;
func_prune_protos *prune_protos = (func_prune_protos *)0x0057F4B0;
func_can_terraform *can_terraform_OG = (func_can_terraform *)0x00565320;
func_void *balance_OG = (func_void *)0x005B0420;
func_void *base_psych_OG = (func_void *)0x004EA540;
func_social_ai *social_ai_OG = (func_social_ai *)0x005B4790;
func_path_sensors *Path_sensors_OG = (func_path_sensors *)0x0059CCA0;
func_xyib *world_alt_set_OG = (func_xyib *)0x005C2020;
func_xy *world_raise_alt_OG = (func_xy *)0x005C2380;
func_xy *world_lower_alt_OG = (func_xy *)0x005C23E0;
func_xyi *brush_OG = (func_xyi *)0x005C2440;
func_xyuu *paint_land_OG = (func_xyuu *)0x005C27F0;
func_u32 *build_continent_OG = (func_u32 *)0x005C28F0;
func5 *build_hills_OG = (func5 *)0x005C2B40;
func_void *world_riverbeds_OG = (func_void *)0x005C3680;
func_void *world_temperature_OG = (func_void *)0x005C4170;
func_void *world_analysis_OG = (func_void *)0x005C55C0;
func_void *world_polar_caps_OG = (func_void *)0x005C58E0;
func_void *world_linearize_contours_OG = (func_void *)0x005C5AE0;
func_xy *world_crater_OG = (func_xy *)0x005C5C70;
func_xy *world_monsoon_OG = (func_xy *)0x005C5EF0;
func_xy *world_sargasso_OG = (func_xy *)0x005C6200;
func_xy *world_ruin_OG = (func_xy *)0x005C64A0;
func_xy *world_dune_OG = (func_xy *)0x005C6740;
func_xy *world_diamond_OG = (func_xy *)0x005C69E0;
func_xy *world_fresh_OG = (func_xy *)0x005C6C40;
func_xyb *world_volcano_OG = (func_xyb *)0x005C6DB0;
func_xy *world_borehole_OG = (func_xy *)0x005C7020;
func_xy *world_temple_OG = (func_xy *)0x005C7540;
func_xy *world_unity_OG = (func_xy *)0x005C7750;
func_xy *world_fossil_OG = (func_xy *)0x005C7A80;
func_xy *world_canyon_OG = (func_xy *)0x005C7CB0;
func_xy *world_mesa_OG = (func_xy *)0x005C7F40;
func_xy *world_ridge_OG = (func_xy *)0x005C8150;
func_xy *world_geothermal_OG = (func_xy *)0x005C83B0;
func_void *world_landmarks_OG = (func_void *)0x005C8630;
///
char1032 *stringTemp = (char1032 *)0x009B86A0;
char256 *ParseStrBuffer = (char256 *)0x009BB5E8;

//  ; int
int *BufferStrHeight = (int *)0x009B3A9C;
int *ParseNumTable = (int *)0x009BB598;
int *ParseStrPlurality = (int *)0x009BB570;
int *ParseStrGender = (int *)0x009BB5C0;
HWND *HandleMain = (HWND *)0x009B7B28;

uint32_t *UnkBitfield1 = (uint32_t *)0x0090D91C;

Filefind *FilefindPath = (Filefind *)0x009B8198;
MainInterface *MainInterfaceVar = (MainInterface *)0x007AE820;

int __cdecl tester() {
    log_set_state(true);
    log_say("Start test", 0, 0, 0);

    /*
    add_site(4, AI_GOAL_COLONIZE, 4, 31, 51);
    add_site(4, AI_GOAL_COLONIZE, 4, 33, 51);
    add_site(4, AI_GOAL_COLONIZE, 4, 35, 51);
    add_site(4, AI_GOAL_COLONIZE, 4, 37, 51);
    add_site(4, AI_GOAL_COLONIZE, 4, 39, 51);
    add_site(4, AI_GOAL_COLONIZE, 4, 41, 51);
    add_site(4, AI_GOAL_COLONIZE, 4, 43, 51);
    add_site(4, AI_GOAL_COLONIZE, 4, 45, 51);
    add_site(4, AI_GOAL_COLONIZE, 4, 47, 51);

    add_site(4, AI_GOAL_COLONIZE, 4, 39, 43);
    add_site(4, AI_GOAL_COLONIZE, 4, 39, 45);
    add_site(4, AI_GOAL_COLONIZE, 4, 39, 47);
    add_site(4, AI_GOAL_COLONIZE, 4, 39, 49);
    add_site(4, AI_GOAL_COLONIZE, 4, 39, 53);
    add_site(4, AI_GOAL_COLONIZE, 4, 39, 55);
    add_site(4, AI_GOAL_COLONIZE, 4, 39, 57);
    add_site(4, AI_GOAL_COLONIZE, 4, 39, 59);
    
    int xx = 39, yy = 51;

    int x_radius, y_radius;
    for (int i = 0; i < RadiusRange[3]; i++) {
        x_radius = xrange(xx + RadiusOffsetX[i]);
        y_radius = yy + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius)) {
            if (i < 21) {
                bit_set(x_radius, y_radius, BIT_AIRBASE, true);
            }
            else {
                bit_set(x_radius, y_radius, BIT_SENSOR_ARRAY, true);
            }
        }
    }

    for (uint32_t y = 0; y < *MapLatitudeBounds; y++) {
        for (uint32_t x = y & 1; x < *MapLongitudeBounds; x += 2) {
            int dist = vector_dist(x_radius, y_radius, x, y);
            if (dist <= 3) {
                bit_set(x, y, BIT_FUNGUS, true);
                //add_site(1, AI_GOAL_COLONIZE, 10, x, y);
                log_say("Added site: ", x, y, 0);
            }
        }
    }
   
    /*
    for (int i = -2; i < *BaseCurrentCount; i++) {
        for (int j = 0; j < MaxPlayerNum; j++) {
            for (int l = 0; l < MaxPlayerNum; l++) {
                for (int k = 0; k < 2; k++) {
                    for (int h = 0; h < 2; h++) {
                        if (value_of_base_OG(i, j, l, k, h) != value_of_base(i, j, l, k, h)) {
                            log_say("value_of_base_error: ", i, j, l);
                            log_say("value_of_base_error: ", k, h, 0);
                        }
                    }
                }
            }
        }
    }

    /*
    for (int i = 0; i < *VehCurrentCount; i++) { // test -1
        for (int j = 0; j < *VehCurrentCount; j++) {
            for (int k = 0; k < 2; k++) {
                if (best_defender_OG(j, i, k) != best_defender(j, i, k)) {
                    log_say("error: ", j, i, k);
                }
            }
        }
    }
    */
    /*
    for (int i = 0; i < *BaseCurrentCount; i++) {
        log_say(Base[i].name_string, Base[i].xCoord, Base[i].yCoord, 0);
        log_say("economy_total: ", Base[i].economy_total, 0, 0);
        log_say("psych_total: ", Base[i].psych_total, 0, 0);
        log_say("labs_total: ",  Base[i].labs_total, 0, 0);
        log_say("unk_10: ", Base[i].unk_10, 0, 0);
    }

    /*
    char szTemp[80];
    for (uint32_t y = 0; y < *MapLatitudeBounds; y++) {
        for (uint32_t x = y & 1; x < *MapLongitudeBounds; x += 2) {
            map *tile = map_loc(x, y);
            if (tile->unk_1) {
                log_say("map_unk1:", x, y, tile->unk_1);
            }
            if (tile->bit2 & 0x80000000) {
                sprintf_s(szTemp, 80, "0x%08X", tile->bit2);
                log_say("map_bit2:", szTemp, x, y, 0);
            }
        }
    }
    */
    /*
    for (uint32_t y = 0; y < *MapLatitudeBounds; y++) {
        for (uint32_t x = y & 1; x < *MapLongitudeBounds; x += 2) {
            map *tile = map_loc(x, y);
            if (tile->unk_1) {
                log_say("map_unk1:", x, y, tile->unk_1);
            }
        }
    }
    //
    /*
    for (int i = 0; i < MaxContinentNum; i++) {
        //log_say("Continent_unk2_count:", i, Continents[i].unk_2, 0);
        log_say("Continent_unk3_count:", i, Continents[i].unk_3, 0);
    }
    for (uint32_t y = 0; y < *MapVerticalBounds; y++) {
        for (uint32_t x = y & 1; x < *MapHorizontalBounds; x += 2) {
            map *tile = map_loc(x, y);
            if (altitude_at(x, y) >= ALT_BIT_SHORE_LINE && (tile->climate & (RAINFALL_RAINY | RAINFALL_MOIST)) != 0
                && (tile->val3 & 0xC0u) < TERRAIN_ROCKY && (!(tile->bit & BIT_FUNGUS) || altitude_at(x, y) < ALT_BIT_OCEAN_SHELF)) {
                //log_say("unk2_found:", tile->region, x, y);
            }
            uint32_t site = tile->val2 >> 4;
            if (site) {
                char region[10];
                sprintf_s(region, 10, "%d", tile->region);
                log_say("site_found:", region, site, x, y);
            }
        }
    }
    */
    /*
    for (int i = -1; i < *BaseCurrentCount; i++) {
        for (int j = -2; j < 10; j++) {
            for (int k = -2; k < 10; k++) {
                for (int p = 0; p < MaxPlayerNum; p++) {
                    for (int a = 0; a < 2; a++) {
                        for (int b = 0; b < 2; b++) {
                            //
                        }
                    }
                }
            }
        }
    }
    */
    /*
    for (int i = 0; i < MaxPlayerNum; i++) {
        for (int j = 0; j < *BaseCurrentCount; j++) {
            for (int k = 0; k < 2; k++) {
                int mc1 = mind_control(j, i, k);
                int mc2 = mind_control_OG(j, i, k);
                if (mc1 != mc2) {
                    log_say("MC error", Base[j].name_string, i, j, k);
                }
                else {
                    char szTemp[200];
                    sprintf_s(szTemp, 200, "base: %s - xCoord: %d, yCoord: %d, MC cost for %s : %d",
                        Base[j].name_string, Base[j].xCoord, Base[j].yCoord, Players[i].formal_name_faction, mc1);
                    log_say(szTemp, 0, 0, 0);
                }
            }
        }
    }
    */

    /*
    for (int i = 0; i < MaxPlayerNum; i++) {
        for (int j = 0; j < 75; j++) {
            int type = PlayersData[i].goals[j].type;
            if (type != AI_GOAL_UNUSED) {
                PlayersData[i].goals[j].type = AI_GOAL_UNK_1;
                /*
                if (type == 6) {
                    char szTemp[100];
                    sprintf_s(szTemp, 100, "goal1 - xCoord: %d, yCoord: %d",
                    PlayersData[i].goals_1[j].xCoord, PlayersData[i].goals_1[j].yCoord);
                    log_say(szTemp, 0, 0, 0);
                }
                */
                /*
                        }
                    }
                }
                */
                /*
                for (int i = 0; i < MaxPlayerNum; i++) {
                    log_say(Players[i].nameFaction, "global rep", PlayersData[i].global_reputation, 0, 0);
                }
                for (int i = 0; i < MaxPlayerNum; i++) {
                    log_say(Players[i].nameFaction, "energy cost", PlayersData[i].energyCost, 0, 0);
                }
                /*
                for (int i = 0; i < MaxPlayerNum; i++) {
                    for (int j = 0; j < MaxPlayerNum; j++) {
                        int val = PlayersData[i].diploUnk3[j];
                        if (val) {
                            char szTemp[100];
                            sprintf_s(szTemp, 100, "combat_diplo_unk3: %s for %s", Players[i].nameFaction, Players[j].nameFaction);
                            log_say(szTemp, val, 0, 0);
                        }
                    }
                }
                for (int i = 0; i < MaxPlayerNum; i++) {
                    for (int j = 0; j < MaxPlayerNum; j++) {
                        int val = PlayersData[i].diploUnk4[j];
                        if (val) {
                            char szTemp[100];
                            sprintf_s(szTemp, 100, "combat_diplo_unk4: %s for %s", Players[i].nameFaction, Players[j].nameFaction);
                            log_say(szTemp, val, 0, 0);
                        }
                    }
                }
                for (int i = 0; i < MaxPlayerNum; i++) {
                    for (int j = 0; j < MaxPlayerNum; j++) {
                        int val = PlayersData[i].diploUnk1[j];
                        if (val) {
                            char szTemp[100];
                            sprintf_s(szTemp, 100, "diploUnk1: %s for %s", Players[i].nameFaction, Players[j].nameFaction);
                            log_say(szTemp, val, 0, 0);
                        }
                    }
                }

                //tech_calc_output();

                /*
                for (int i = 0; i < MaxPlayerNum; i++) {
                    log_say(Players[i].search_key, "PSI Atk", weap_strat(WPN_PSI_ATTACK, i), 0, 0);
                    log_say(Players[i].search_key, "PSI Def", arm_strat(ARM_PSI_DEFENSE, i), 0, 0);
                }

                for (int z = 0; z < 10; z++) {
                    for (int i = 1; i < MaxPlayerNum; i++) {
                        int t1 = PlayersData[i].enemyBestWeaponVal;
                        int t2 = PlayersData[i].enemyBestArmorVal;
                        int t3 = PlayersData[i].enemy_best_land_speed;
                        int t4 = PlayersData[i].enemyBestPsiAtkVal;
                        int t5 = PlayersData[i].enemyBestPsiDefVal;
                        int t6 = PlayersData[i].best_land_speed;
                        int t7 = PlayersData[i].bestPsiDefVal;
                        int t8 = PlayersData[i].bestArmorVal;
                        int t9 = PlayersData[i].bestPsiAtkVal;
                        int t0 = PlayersData[i].bestWeaponVal;
                        enemy_capabilities_t(i);
                        int a1 = PlayersData[i].enemyBestWeaponVal;
                        int a2 = PlayersData[i].enemyBestArmorVal;
                        int a3 = PlayersData[i].enemy_best_land_speed;
                        int a4 = PlayersData[i].enemyBestPsiAtkVal;
                        int a5 = PlayersData[i].enemyBestPsiDefVal;
                        int a6 = PlayersData[i].best_land_speed;
                        int a7 = PlayersData[i].bestPsiDefVal;
                        int a8 = PlayersData[i].bestArmorVal;
                        int a9 = PlayersData[i].bestPsiAtkVal;
                        int a0 = PlayersData[i].bestWeaponVal;
                        PlayersData[i].enemyBestWeaponVal = t1;
                        PlayersData[i].enemyBestArmorVal = t2;
                        PlayersData[i].enemy_best_land_speed = t3;
                        PlayersData[i].enemyBestPsiAtkVal = t4;
                        PlayersData[i].enemyBestPsiDefVal = t5;
                        PlayersData[i].best_land_speed = t6;
                        PlayersData[i].bestPsiDefVal = t7;
                        PlayersData[i].bestArmorVal = t8;
                        PlayersData[i].bestPsiAtkVal = t9;
                        PlayersData[i].bestWeaponVal = t0;
                        enemy_capabilities_OG(i);
                        int b1 = PlayersData[i].enemyBestWeaponVal;
                        int b2 = PlayersData[i].enemyBestArmorVal;
                        int b3 = PlayersData[i].enemy_best_land_speed;
                        int b4 = PlayersData[i].enemyBestPsiAtkVal;
                        int b5 = PlayersData[i].enemyBestPsiDefVal;
                        int b6 = PlayersData[i].best_land_speed;
                        int b7 = PlayersData[i].bestPsiDefVal;
                        int b8 = PlayersData[i].bestArmorVal;
                        int b9 = PlayersData[i].bestPsiAtkVal;
                        int b0 = PlayersData[i].bestWeaponVal;

                        if (a1 != b1) {
                            log_say("enemyBestWeaponVal error", a1, b1, i);
                        }
                        if (a2 != b2) {
                            log_say("enemyBestArmorVal error", a2, b2, i);
                        }
                        if (a3 != b3) {
                            log_say("enemy_best_land_speed error", a3, b3, i);
                        }
                        if (a4 != b4) {
                            log_say("enemyBestPsiAtkVal error", a4, b4, i);
                        }
                        if (a5 != b5) {
                            log_say("enemyBestPsiDefVal error", a5, b5, i);
                        }
                        if (a6 != b6) {
                            log_say("best_land_speed error", a6, b6, i);
                        }
                        if (a7 != b7) {
                            log_say("bestPsiDefVal error", a7, b7, i);
                        }
                        if (a8 != b8) {
                            log_say("bestArmorVal error", a8, b8, i);
                        }
                        if (a9 != b9) {
                            log_say("bestPsiAtkVal error", a9, b9, i);
                        }
                        if (a0 != b0) {
                            log_say("bestWeaponVal error", a0, b0, i);
                        }
                        log_say("results: ", i, a1, a2);
                        log_say("results: ", i, a3, a4);
                        log_say("results: ", i, a5, a6);
                        log_say("results: ", i, a7, a8);
                        log_say("results: ", i, a9, a0);
                    }
                }
                /*
                log_say("original start", 0, 0, 0);
                for (int z = 0; z < 10; z++) {
                    for (int i = 1; i < MaxPlayerNum; i++) {
                        int t1 = PlayersData[i].enemyBestWeaponVal;
                        int t2 = PlayersData[i].enemyBestArmorVal;
                        int t3 = PlayersData[i].enemy_best_land_speed;
                        int t4 = PlayersData[i].enemyBestPsiAtkVal;
                        int t5 = PlayersData[i].enemyBestPsiDefVal;
                        int t6 = PlayersData[i].best_land_speed;
                        int t7 = PlayersData[i].bestPsiDefVal;
                        int t8 = PlayersData[i].bestArmorVal;
                        int t9 = PlayersData[i].bestPsiAtkVal;
                        int t0 = PlayersData[i].bestWeaponVal;
                        enemy_capabilities_OG(i);
                        int a1 = PlayersData[i].enemyBestWeaponVal;
                        int a2 = PlayersData[i].enemyBestArmorVal;
                        int a3 = PlayersData[i].enemy_best_land_speed;
                        int a4 = PlayersData[i].enemyBestPsiAtkVal;
                        int a5 = PlayersData[i].enemyBestPsiDefVal;
                        int a6 = PlayersData[i].best_land_speed;
                        int a7 = PlayersData[i].bestPsiDefVal;
                        int a8 = PlayersData[i].bestArmorVal;
                        int a9 = PlayersData[i].bestPsiAtkVal;
                        int a0 = PlayersData[i].bestWeaponVal;
                        PlayersData[i].enemyBestWeaponVal = t1;
                        PlayersData[i].enemyBestArmorVal = t2;
                        PlayersData[i].enemy_best_land_speed = t3;
                        PlayersData[i].enemyBestPsiAtkVal = t4;
                        PlayersData[i].enemyBestPsiDefVal = t5;
                        PlayersData[i].best_land_speed = t6;
                        PlayersData[i].bestPsiDefVal = t7;
                        PlayersData[i].bestArmorVal = t8;
                        PlayersData[i].bestPsiAtkVal = t9;
                        PlayersData[i].bestWeaponVal = t0;
                        enemy_capabilities_OG(i);
                        int b1 = PlayersData[i].enemyBestWeaponVal;
                        int b2 = PlayersData[i].enemyBestArmorVal;
                        int b3 = PlayersData[i].enemy_best_land_speed;
                        int b4 = PlayersData[i].enemyBestPsiAtkVal;
                        int b5 = PlayersData[i].enemyBestPsiDefVal;
                        int b6 = PlayersData[i].best_land_speed;
                        int b7 = PlayersData[i].bestPsiDefVal;
                        int b8 = PlayersData[i].bestArmorVal;
                        int b9 = PlayersData[i].bestPsiAtkVal;
                        int b0 = PlayersData[i].bestWeaponVal;

                        if (a1 != b1) {
                            log_say("enemyBestWeaponVal error", a1, b1, i);
                        }
                        if (a2 != b2) {
                            log_say("enemyBestArmorVal error", a2, b2, i);
                        }
                        if (a3 != b3) {
                            log_say("enemy_best_land_speed error", a3, b3, i);
                        }
                        if (a4 != b4) {
                            log_say("enemyBestPsiAtkVal error", a4, b4, i);
                        }
                        if (a5 != b5) {
                            log_say("enemyBestPsiDefVal error", a5, b5, i);
                        }
                        if (a6 != b6) {
                            log_say("best_land_speed error", a6, b6, i);
                        }
                        if (a7 != b7) {
                            log_say("bestPsiDefVal error", a7, b7, i);
                        }
                        if (a8 != b8) {
                            log_say("bestArmorVal error", a8, b8, i);
                        }
                        if (a9 != b9) {
                            log_say("bestPsiAtkVal error", a9, b9, i);
                        }
                        if (a0 != b0) {
                            log_say("bestWeaponVal error", a0, b0, i);
                        }
                        log_say("results: ", i, a1, a2);
                        log_say("results: ", i, a3, a4);
                        log_say("results: ", i, a5, a6);
                        log_say("results: ", i, a7, a8);
                        log_say("results: ", i, a9, a0);
                    }
                }
                */

                /*
                for (int i = 0; i < MaxPlayerNum; i++) {
                    for (int j = 0; j < 128; j++) {
                        if (PlayersData[i].baseCountByRegion[j]) {
                            log_say(Players[i].search_key, " ? baseCountByRegion ? ", j, PlayersData[i].baseCountByRegion[j], 0);
                        }
                    }
                }

                for (int i = 0; i < MaxPlayerNum; i++) {
                    for (int j = 0; j < 128; j++) {
                        if (PlayersData[i].region_good_radius_tiles[j]) {
                            log_say(Players[i].search_key, "? region_good_radius_tiles ?", j, PlayersData[i].region_good_radius_tiles[j], 0);
                        }
                    }
                }

                for (int i = 0; i < MaxPlayerNum; i++) {
                    for (int j = 0; j < 128; j++) {
                        if (PlayersData[i].unk_84[j]) {
                            //log_say(Players[i].search_key, "? unk_84 ? ", j, PlayersData[i].unk_84[j], 0);
                        }
                    }
                }

                for (int i = 0; i < MaxPlayerNum; i++) {
                    //log_say(Players[i].search_key, "unk_70 - sea?", PlayersData[i].unk_70, 0, 0);
                    //log_say(Players[i].search_key, "unk_71", PlayersData[i].unk_72, 0, 0);
                    log_say(Players[i].search_key, " ? unk_26 | earned_techs_saved | tech_ranking ? ", PlayersData[i].unk_26, PlayersData[i].earned_techs_saved, PlayersData[i].tech_ranking);

                    //log_say(Players[i].search_key, "armed_unit_weight - base support sum", PlayersData[i].armed_unit_weight, 0, 0);
                    //log_say(Players[i].search_key, "? unk_49 - 4xecon dmg sum ? ", PlayersData[i].unk_49, 0, 0);
                    //log_say(Players[i].search_key, "armed_unit_weight - support", PlayersData[i].armed_unit_weight, 0, 0);
                }
                */
                /*
                for (int i = 0; i < MaxBaseNum; i++) {
                    char szTemp[512];
                    szTemp[0] = 0;
                    strcat_s(szTemp, " - ");
                    strcat_s(szTemp, Players[Base[i].faction_id_current].noun_faction);
                    strcat_s(szTemp, " > unk3 > ");
                    log_say(Base[i].name_string, szTemp, Base[i].unk3, 0, 0);
                    /*
                    szTemp[0] = 0;
                    strcat_s(szTemp, " - ");
                    strcat_s(szTemp, Players[Base[i].faction_id_current].noun_faction);
                    strcat_s(szTemp, " - unk4 - ");
                    log_say(Base[i].name_string, szTemp, Base[i].unk4, 0, 0);
                    szTemp[0] = 0;
                    strcat_s(szTemp, " - ");
                    strcat_s(szTemp, Players[Base[i].faction_id_current].noun_faction);
                    strcat_s(szTemp, " - unk5 - ");
                    log_say(Base[i].name_string, szTemp, Base[i].unk5, 0, 0);
                    */
                    //}

                    //

    log_say("End test", 0, 0, 0);
    log_set_state(false);
    /*
    for (uint32_t y = 0; y < *MapVerticalBounds; y++) {
        for (uint32_t x = y & 1; x < *MapHorizontalBounds; x += 2) {
            uint8_t att = abstract_at(x, y);
            if (att) {
                log_say("abstract_at: ", x, y, att);
                //log_say("region:      ", x, y, region_at(x,y));
            }

            //if (goody) {
            //    log_say("goody_at: ", x, y, goody);
            //}
            /*
            uint32_t bit = bit_at(x, y);
            if (bit & BIT_UNK_4000000) {
                log_say("found BIT_UNK_4000000: ", x, y, bit);
            }
            if (bit & BIT_UNK_8000000) {
                log_say("found BIT_UNK_8000000: ", x, y, bit);
            }
            /*
            for (uint32_t alt = 0; alt < 8; alt++) {
                map_loc(x, y)->val1 &= 0x1F;
                map_loc(x, y)->val1 |= alt << 5;
                uint32_t min = minerals_at_OG(x, y);
                if (minerals_at(x, y) != min) {
                    log_say("minerals_at error: ", x, y, min);
                }
            }
            */
            //for (int f = 0; f < MaxPlayerNum; f++) {
                //
            //}
            //int goody = goody_at_OG(x, y);
            //if (bonus != bonus_at_OG(x, y, 0)) {
            //    MessageBoxA(NULL, "bonus_at Error", "FATAL ERROR", MB_ICONWARNING);
            //}
            //if (goody) {
            //    log_say("goody_at: ", x, y, goody);
            //}
            /*
            uint32_t bit = bit_at(x, y);

            if (bit & BIT_UNK_200) {
                log_say("BIT_UNK_200: ", x, y, bit);
            }
            if (bit & BIT_UNK_4000) {
                log_say("BIT_UNK_4000: ", x, y, bit);
            }
            //if (bit & BIT_SUPPLY_REMOVE) {
            //    log_say("BIT_SUPPLY_REMOVE: ", x, y, bit);
            //}
            if (bit & BIT_UNK_2000000) {
                log_say("BIT_UNK_2000000: ", x, y, bit);
            }
            if (bit & BIT_UNK_4000000) {
                log_say("BIT_UNK_4000000: ", x, y, bit);
            }
            if (bit & BIT_UNK_8000000) {
                log_say("BIT_UNK_8000000: ", x, y, bit);
            }
            //if (bit & BIT_UNK_40000000) {
            //    log_say("BIT_UNK_40000000: ", x, y, bit);
            //}
            */
            //}
        //}

        /*
        for (int y = -10; y < (int)*MapVerticalBounds; y++) {
            for (int x = -1; x < (int)*MapHorizontalBounds; x++) {
                for (int f = -1; f < 10; f++) {
                    int FMTemp = *MapFlatToggle;
                    for (int t = 0; t < 2; t++) {
                        *MapFlatToggle = t;
                        BOOL MPtemp = *MultiplayerToggle;
                        for (int m = 0; m < 2; m++) {
                            *MultiplayerToggle = m;
                            if (zoc_any_OG(x, y, f) != zoc_any(x, y, f)) {
                                MessageBoxA(NULL, "zoc_any Error", "FATAL ERROR", MB_ICONWARNING);
                            }
                            if (zoc_veh_OG(x, y, f) != zoc_veh(x, y, f)) {
                                MessageBoxA(NULL, "zoc_veh Error", "FATAL ERROR", MB_ICONWARNING);
                            }
                            if (zoc_sea_OG(x, y, f) != zoc_sea(x, y, f)) {
                                MessageBoxA(NULL, "zoc_sea Error", "FATAL ERROR", MB_ICONWARNING);
                            }
                            if (zoc_move_OG(x, y, f) != zoc_move(x, y, f)) {
                                MessageBoxA(NULL, "zoc_move Error", "FATAL ERROR", MB_ICONWARNING);
                            }
                        }
                        *MultiplayerToggle = MPtemp;
                    }
                    *MapFlatToggle = FMTemp;
                }
            }
        }
        */
    return 0;
}

// tech_val output
func13 *tech_val_OG = (func13 *)0x005BCBE0;
void tech_calc_output() {
    for (int i = 0; i < MaxPlayerNum; i++) {
        for (int j = 0; j < MaxTechnologyNum; j++) {
            for (int k = 0; k < 2; k++) {
                int techVal1 = tech_val(j, i, k);
                int techVal2 = tech_val_OG(j, i, k);
                if (techVal1 != techVal2) {
                    log_say("tech_val error: ", j, i, k);
                    log_say("good:", techVal2, 0, 0);
                    log_say("bad: ", techVal1, 0, 0);
                }
                else {
                    log_say(Players[i].formal_name_faction, Technology[j].name, techVal1, k, 0);
                }
            }
        }
        for (int j = 89; j < 609; j++) {
            int techVal1 = tech_val(j, i, 0);
            int techVal2 = tech_val_OG(j, i, 0);
            if (techVal1 != techVal2) {
                log_say("tech_val error: ", j, i, 0);
                if (j < 97) {
                    log_say(Players[i].formal_name_faction, Players[j - 89].formal_name_faction, techVal1, techVal2, 0);
                }
                else {
                    log_say(Players[i].formal_name_faction, VehPrototypes[j - 97].veh_name, techVal1, techVal2, 0);
                }
            }
            else {
                if (j < 97) {
                    log_say(Players[i].formal_name_faction, Players[j - 89].formal_name_faction, techVal1, 0, 0);
                }
                else {
                    log_say(Players[i].formal_name_faction, VehPrototypes[j - 97].veh_name, techVal1, 0, 0);
                }
            }
        }
    }
}

// message handling testing
// possible issues different SDKs?
func_msg *do_video = (func_msg *)0x00636300;
func_msg *check_net = (func_msg *)0x0062D5D0;
func_msg *do_net = (func_msg *)0x0062D5B0;
func_msg *do_non_input_ = (func_msg *)0x005FCA30;

uint32_t *MsgStatus = (uint32_t *)0x009B7B9C;

// net
func_net_synch *NetDaemon_synch = (func_net_synch *)0x00532E00;
void *NetDaemonVar = (void *)0x0093CD90;
func_net_send_msg *NetDaemon_send_message = (func_net_send_msg *)0x00532940;
func_net_await *NetDaemon_await_diplo = (func_net_await *)0x005315C0;
func_net_this_only *NetDaemon_add_global = (func_net_this_only *)0x00530AA0;
func_net_this_only *NetDaemon_unlock = (func_net_this_only *)0x00530D00;
func_console_update *Console_update_data = (func_console_update *)0x00514880;
void *ConsoleVar = (void *)0x009156B0;
func_diplo3 *double_cross = (func_diplo3 *)0x0050BCC0;
func5 *ambience = (func5 *)0x00446B40;
func_win_redraw *GraphicWin_redraw = (func_win_redraw *)0x005D5A70;
void *MultiWinVar = (void *)0x007FD648;
int *MultiWinFlag = (int *)0x007FE06C;
uint32_t *AmbienceFlags = (uint32_t *)0x0074DA9C;
func_diplo2 *pact_ends = (func_diplo2 *)0x0053C650;
func_diplo2 *give_a_base = (func_diplo2 *)0x0054CF40;
func_draw3 *draw_tiles = (func_draw3 *)0x0046B140;
func_draw4 *draw_tile_fixup = (func_draw4 *)0x0046AF90;
func_draw3 *draw_tile = (func_draw3 *)0x0046AF40;
func_fx_play *FX_play = (func_fx_play *)0x00446A00;
void *FXVar = (void *)0x00749CF8;
func_netmsg_pop *NetMsg_pop = (func_netmsg_pop *)0x0047A890;
void *NetMsgVar = (void *)0x00805338;
int *BaseIDUnk = (int *)0x009A6564;
func_iii *wants_prop = (func_iii *)0x0052AEB0;
func_upgrade_proto *upgrade_prototype = (func_upgrade_proto *)0x004EFE80;
func_console_focus *Console_focus = (func_console_focus *)0x005108A0;
BOOL *IsLoggingActive = (BOOL *)0x0093A94C;
int *DialogToggle = (int *)0x009BC070;
int *CouncilSessionPending = (int *)0x00703DE8;
int *VehDrawBoomID = (int *)0x00689F30;
int *MapWinVehIDSelected = (int *)0x0093928C;
BOOL *MapWinUnitNotViewMode = (BOOL *)0x00939288;
int *MapWinUnkFlag = (int *)0x00939294;
int *VehUpgradeUnk = (int *)0x0090EA3C;
func_veh_init *veh_init = (func_veh_init *)0x005C03D0;
func_win_redraw *MapWin_clear_terrain = (func_win_redraw *)0x0046FD90;
func_i *tech_selection = (func_i *)0x005BE380;
func_tech_pick *tech_pick = (func_tech_pick *)0x005BEB70;
int *SkipTechScreenA = (int *)0x00945F40;
int *SkipTechScreenB = (int *)0x00945F44;
int *NetUpkeepState = (int *)0x00945F48;
func_xy *base_reset = (func_xy *)0x004E5FE0;
func5 *monument = (func5 *)0x00476A50;
func_interlude *interlude = (func_interlude *)0x005230E0;
void *BaseWinVar = (void *)0x006A7628;
void *WorldWinVar = (void *)0x008E9F60;
BOOL *TradeMapsRevealedNew = (BOOL *)0x0093FAD8;
func_tech_ach *tech_achieved = (func_tech_ach *)0x005BB000;
func9 *wait_loop = (func9 *)0x0051F200;
func9 *check_spock = (func9 *)0x0051F1B0;
int *FactionIDCurrent = (int *)0x00939284;
int *NetMsgSequence = (int *)0x0093E8BC;
int *NetMsgSendDepth = (int *)0x0093F664;
BOOL *NetPopWaitActive = (BOOL *)0x0093FAC4;
int *ControlTurnA = (int *)0x009B2068;

/*
Purpose: Process non-input related message.
Original Offset: 005FCA30
Return Value: Is message available? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-full.log, message-level test)
*/
BOOL __cdecl do_non_input() {
    do_video();
    check_net();
    do_net();
    MSG msg[3];
    for (int i = 0; i < 3; i++) {
        msg[i].time = 0xFFFFFFFF;
    }
    // The original checks the three ranges in turn and stops at the first one that has a
    // message (the others keep time 0xFFFFFFFF, so the selection below picks the one found).
    // 0x20A: WM_MOUSELAST in the SDK the game was built with (WM_MOUSEWHEEL); newer SDKs
    // define WM_MOUSELAST as 0x20E, which would skip the mouse wheel and X-button messages.
    if (!PeekMessage(&msg[0], NULL, WM_NULL, WM_INPUT, PM_NOREMOVE)
        && !PeekMessage(&msg[1], NULL, WM_UNICHAR, WM_KEYDOWN | WM_INPUT, PM_NOREMOVE)
        && !PeekMessage(&msg[2], NULL, 0x20A, UNICODE_NOCHAR, PM_NOREMOVE)) {
        return false;
    }
    int a = 0;
    for (int c = 1; c < 3; c++) {
        if (msg[c].time < msg[a].time) {
            a = c;
        }
    }
    PeekMessage(&msg[0], msg[a].hwnd, msg[a].message, msg[a].message, PM_REMOVE);
    TranslateMessage(&msg[0]);
    DispatchMessage(&msg[0]);
    return true;
}

/*
Purpose: Process all non-input related messages.
Original Offset: 005FCB20
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-full.log, message-level test)
*/
void __cdecl do_all_non_input() {
    do {
        *MsgStatus = 32;
    } while (do_non_input_());
    *MsgStatus = 0;
    do_net();
    check_net();
}

/*
Purpose: Process draw related message.
Original Offset: 005FCB60
Return Value: Is message available? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-full.log, message-level test)
*/
BOOL __cdecl do_draw() {
    do_video();
    check_net();
    do_net();
    MSG msg;
    if (PeekMessage(&msg, NULL, WM_PAINT, WM_PAINT, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        return true;
    }
    return false;
}

/*
Purpose: Process all draw related messages.
Original Offset: 005FCBB0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-full.log, message-level test)
*/
void __cdecl do_all_draws() {
    do {
        *MsgStatus = 1;
    } while (do_draw());
    *MsgStatus = 0;
}

/*
Purpose: Process keyboard related message.
Original Offset: 005FCC20
Return Value: Is message available? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-full.log, message-level test)
*/
BOOL __cdecl do_keyboard() {
    do_video();
    check_net();
    MSG msg;
    // 0x108: WM_KEYLAST in the SDK the game was built with; newer SDKs define it as 0x109 (WM_UNICHAR)
    if (PeekMessage(&msg, NULL, WM_KEYDOWN, 0x108, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        return true;
    }
    return false;
}

/*
Purpose: Process all keyboard related messages.
Original Offset: 005FCC70
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-full.log, message-level test)
*/
void __cdecl do_all_keyboard() {
    do {
        *MsgStatus = 2;
    } while (do_keyboard());
    *MsgStatus = 0;
    do_net();
}
