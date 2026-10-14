# Unredirected functions named by Thinker, cross-referenced against the OpenSMACX patcher

Generated 2026-10-14 from `tools/opensmac_patcher.py` (v0.2.3, 462 redirected functions) and Thinker v5.5 `src/engine.cpp` (2503 named function pointers in .text).

- Thinker-named functions already redirected by the patcher: **410**
- Thinker-named functions **not** redirected (candidates below): **2093**
- Of those, a name match exists in the DLL's export table (decompiled but not patched in): **47**

`area` = the patcher section of the nearest redirected neighbour, so it is a proximity hint, not a classification. `size_est` = bytes to the next known function start (Thinker or patcher), an upper bound on the function's size. Full table in `candidates-thinker-xref.tsv`.

## First-batch pool (156 functions), grouped by family

Pool = sandwiched between two redirected functions within 4 KB each side, estimated size <= 512 bytes, no name match in the DLL. Families are by Thinker name prefix. Popup wrappers (`X_pop*`, `pops_*`, `pop_ask*`) are listed last: each is a ~48-byte thunk around the same core, cheap but low value.

### synch_* (14, 752 bytes est., 0x593220-0x5934E0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x593220 | `synch_veh` | `void (int)` | 48 | kill_landmark (0x5926F0) (2864) | hex_cost (0x593510) (752) |
| 0x593250 | `synch_base` | `void (int)` | 48 | kill_landmark (0x5926F0) (2912) | hex_cost (0x593510) (704) |
| 0x593280 | `synch_energy` | `void (int)` | 48 | kill_landmark (0x5926F0) (2960) | hex_cost (0x593510) (656) |
| 0x5932B0 | `synch_researching` | `void (int)` | 48 | kill_landmark (0x5926F0) (3008) | hex_cost (0x593510) (608) |
| 0x5932E0 | `synch_leader` | `void (int)` | 48 | kill_landmark (0x5926F0) (3056) | hex_cost (0x593510) (560) |
| 0x593310 | `synch_ai` | `void (int)` | 48 | kill_landmark (0x5926F0) (3104) | hex_cost (0x593510) (512) |
| 0x593340 | `synch_research` | `void (int)` | 80 | kill_landmark (0x5926F0) (3152) | hex_cost (0x593510) (464) |
| 0x593390 | `synch_alloc` | `void (int)` | 48 | kill_landmark (0x5926F0) (3232) | hex_cost (0x593510) (384) |
| 0x5933C0 | `synch_soc` | `void (int)` | 48 | kill_landmark (0x5926F0) (3280) | hex_cost (0x593510) (336) |
| 0x5933F0 | `synch_proto` | `void (int)` | 48 | kill_landmark (0x5926F0) (3328) | hex_cost (0x593510) (288) |
| 0x593420 | `synch_obs` | `void (int)` | 48 | kill_landmark (0x5926F0) (3376) | hex_cost (0x593510) (240) |
| 0x593450 | `synch_diplo` | `void (int, int)` | 96 | kill_landmark (0x5926F0) (3424) | hex_cost (0x593510) (192) |
| 0x5934B0 | `synch_template` | `void (int)` | 48 | kill_landmark (0x5926F0) (3520) | hex_cost (0x593510) (96) |
| 0x5934E0 | `synch_radius` | `void (int)` | 48 | kill_landmark (0x5926F0) (3568) | hex_cost (0x593510) (48) |

### net_* (14, 1632 bytes est., 0x5391C0-0x5397B0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5391C0 | `net_treaty_on` | `void (int, int, int, int)` | 112 | auto_contact (0x539160) (96) | great_beelzebub (0x539B70) (2480) |
| 0x539230 | `net_treaty_off` | `void (int, int, int, int)` | 112 | auto_contact (0x539160) (208) | great_beelzebub (0x539B70) (2368) |
| 0x5392A0 | `net_set_treaty` | `void (int, int, int, int, int)` | 112 | auto_contact (0x539160) (320) | great_beelzebub (0x539B70) (2256) |
| 0x539310 | `net_agenda_on` | `void (int, int, int, int)` | 112 | auto_contact (0x539160) (432) | great_beelzebub (0x539B70) (2144) |
| 0x539380 | `net_agenda_off` | `void (int, int, int, int)` | 112 | auto_contact (0x539160) (544) | great_beelzebub (0x539B70) (2032) |
| 0x5393F0 | `net_set_agenda` | `void (int, int, int, int, int)` | 112 | auto_contact (0x539160) (656) | great_beelzebub (0x539B70) (1920) |
| 0x539460 | `net_energy` | `void (int, int, int, int, int)` | 176 | auto_contact (0x539160) (768) | great_beelzebub (0x539B70) (1808) |
| 0x539510 | `net_loan` | `void (int, int, int, int)` | 112 | auto_contact (0x539160) (944) | great_beelzebub (0x539B70) (1632) |
| 0x539580 | `net_maps` | `void (int, int, int)` | 112 | auto_contact (0x539160) (1056) | great_beelzebub (0x539B70) (1520) |
| 0x5395F0 | `net_tech` | `void (int, int, int, int)` | 112 | auto_contact (0x539160) (1168) | great_beelzebub (0x539B70) (1408) |
| 0x539660 | `net_pact_ends` | `void (int, int, int)` | 112 | auto_contact (0x539160) (1280) | great_beelzebub (0x539B70) (1296) |
| 0x5396D0 | `net_withdrawal` | `void (int, int, int)` | 112 | auto_contact (0x539160) (1392) | great_beelzebub (0x539B70) (1184) |
| 0x539740 | `net_cede_base` | `void (int, int, int)` | 112 | auto_contact (0x539160) (1504) | great_beelzebub (0x539B70) (1072) |
| 0x5397B0 | `net_double_cross` | `void (int, int, int, int)` | 112 | auto_contact (0x539160) (1616) | great_beelzebub (0x539B70) (960) |

### message_* (7, 1040 bytes est., 0x592E10-0x593190)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x592E10 | `message_veh` | `void (int, int, int, int)` | 96 | kill_landmark (0x5926F0) (1824) | hex_cost (0x593510) (1792) |
| 0x592E70 | `message_base` | `void (int, int, char*, int)` | 112 | kill_landmark (0x5926F0) (1920) | hex_cost (0x593510) (1696) |
| 0x592EE0 | `message_data` | `void (int, int, int, int, int, int)` | 272 | kill_landmark (0x5926F0) (2032) | hex_cost (0x593510) (1584) |
| 0x592FF0 | `message_big_data` | `void (int, int, int*, int)` | 160 | kill_landmark (0x5926F0) (2304) | hex_cost (0x593510) (1312) |
| 0x593090 | `message_lock` | `void (int, int, int, int, int, int, int, int)` | 128 | kill_landmark (0x5926F0) (2464) | hex_cost (0x593510) (1152) |
| 0x593110 | `message_landmark` | `void (int, int, char*, int, int, int, int)` | 128 | kill_landmark (0x5926F0) (2592) | hex_cost (0x593510) (1024) |
| 0x593190 | `message_chat` | `void (int, int, char*)` | 144 | kill_landmark (0x5926F0) (2720) | hex_cost (0x593510) (896) |

### tech_* (5, 1504 bytes est., 0x5BADC0-0x5BEB70)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5BADC0 | `tech_secret` | `void (int, int)` | 160 | tech_avail (0x5BAC20) (416) | tech_effects (0x5BAE60) (160) |
| 0x5BE2A0 | `tech_wizard_unk2` | `void (int, int, char, int, int, int)` | 224 | wants_prototype (0x5BE100) (416) | tech_rate (0x5BE6B0) (1040) |
| 0x5BE380 | `tech_selection` | `int (int)` | 432 | wants_prototype (0x5BE100) (640) | tech_rate (0x5BE6B0) (816) |
| 0x5BE530 | `tech_advance` | `int (int)` | 384 | wants_prototype (0x5BE100) (1072) | tech_rate (0x5BE6B0) (384) |
| 0x5BEB70 | `tech_pick` | `int (int, int, int, const char*)` | 304 | tech_rate (0x5BE6B0) (1216) | is_sensor (0x5BF010) (1184) |

### veh_* (4, 800 bytes est., 0x5C0CB0-0x5C1B70)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5C0CB0 | `veh_find` | `int (int, int, int, int)` | 256 | veh_clear (0x5C02D0) (2528) | can_arty (0x5C0DB0) (256) |
| 0x5C1A20 | `veh_selectable` | `int (int)` | 144 | veh_cost (0x5C1850) (464) | veh_jail (0x5C1C40) (544) |
| 0x5C1AB0 | `veh_unmoved` | `int (int)` | 192 | veh_cost (0x5C1850) (608) | veh_jail (0x5C1C40) (400) |
| 0x5C1B70 | `veh_ready` | `int (int)` | 208 | veh_cost (0x5C1850) (800) | veh_jail (0x5C1C40) (208) |

### BaseButton_* (3, 176 bytes est., 0x607C30-0x607CC0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x607C30 | `BaseButton_UNK2` | `int (BaseButton* This)` | 80 | basebutton_set_bubble_text (0x607550) (1760) | text_~text (0x608C00) (4048) |
| 0x607C80 | `BaseButton_set` | `void (BaseButton* This, int a2)` | 64 | basebutton_set_bubble_text (0x607550) (1840) | text_~text (0x608C00) (3968) |
| 0x607CC0 | `BaseButton_scalar_dtor` | `int (BaseButton* This, uint32_t a2)` | 32 | basebutton_set_bubble_text (0x607550) (1904) | text_~text (0x608C00) (3904) |

### diplo_* (2, 192 bytes est., 0x539820-0x5398C0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x539820 | `diplo_lock` | `int (int)` | 160 | auto_contact (0x539160) (1728) | great_beelzebub (0x539B70) (848) |
| 0x5398C0 | `diplo_unlock` | `int ()` | 32 | auto_contact (0x539160) (1888) | great_beelzebub (0x539B70) (688) |

### base_* (2, 368 bytes est., 0x4E3A50-0x4EC3B0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x4E3A50 | `base_at` | `int (int, int)` | 304 | say_base (0x4E3A00) (80) | base_find (0x4E3B80) (304) |
| 0x4EC3B0 | `base_compute` | `int (int)` | 64 | base_rank (0x4EB490) (3872) | crappy (0x4ECB90) (2016) |

### social_* (2, 416 bytes est., 0x5B4600-0x5B5620)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5B4600 | `social_set` | `void (int)` | 304 | social_upheaval (0x5B4550) (176) | society_avail (0x5B4730) (304) |
| 0x5B5620 | `social_select` | `void (int)` | 112 | society_avail (0x5B4730) (3824) | sort (0x5B5690) (112) |

### scan_* (2, 256 bytes est., 0x6265A0-0x626D90)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x6265A0 | `scan_out_line_poly` | `uint32_t (int, int)` | 128 | log_set_state (0x626450) (336) | set_language (0x627100) (2912) |
| 0x626D90 | `scan_out_line_poly_dest` | `uint32_t (int, int)` | 128 | log_set_state (0x626450) (2368) | set_language (0x627100) (880) |

### spot_* (2, 672 bytes est., 0x5B57D0-0x5B58E0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5B57D0 | `spot_base` | `void (int, int)` | 272 | sort (0x5B5690) (320) | want_to_wake (0x5B5EA0) (1744) |
| 0x5B58E0 | `spot_stack` | `void (int, int)` | 400 | sort (0x5B5690) (592) | want_to_wake (0x5B5EA0) (1472) |

### proto_* (2, 816 bytes est., 0x5A6270-0x5A63D0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5A6270 | `proto_sort` | `void (int)` | 352 | make_proto (0x5A5D40) (1328) | radius_move (0x5A65A0) (816) |
| 0x5A63D0 | `proto_sort_2` | `void (int)` | 464 | make_proto (0x5A5D40) (1680) | radius_move (0x5A65A0) (464) |

### BasePop_* (1, 16 bytes est., 0x5A5900-0x5A5900)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5A5900 | `BasePop_exec_2` | `int (BasePop* This)` | 16 | parse_set (0x5A58E0) (32) | vector_dist (0x5A5910) (16) |

### Font_* (1, 16 bytes est., 0x618F30-0x618F30)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x618F30 | `Font_UNK1` | `int (Font* This, int a2, int a3, int a4, int a5)` | 16 | font_~font (0x618EE0) (80) | font_init (0x618F40) (16) |

### space (1, 48 bytes est., 0x5A58B0-0x5A58B0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5A58B0 | `space` | `int ()` | 48 | say_label (0x5A5880) (48) | parse_set (0x5A58E0) (48) |

### Path_* (1, 80 bytes est., 0x59A320-0x59A320)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x59A320 | `Path_dtor` | `int (Path* This)` | 80 | path_shutdown (0x59A2D0) (80) | path_zoc_path (0x59A370) (80) |

### site_* (1, 112 bytes est., 0x5797D0-0x5797D0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5797D0 | `site_at` | `int (int, int, int)` | 112 | x_dist (0x579790) (64) | is_known (0x579840) (112) |

### get_* (1, 112 bytes est., 0x5ABFF0-0x5ABFF0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5ABFF0 | `get_rating` | `int (int, int)` | 112 | auto_undo (0x5ABF20) (208) | is_objective (0x5AC060) (112) |

### turn_* (1, 64 bytes est., 0x50EF10-0x50EF10)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x50EF10 | `turn_timer` | `void (int unused)` | 64 | convoy (0x50E5C0) (2384) | start_timers (0x50F3D0) (1216) |

### unspot_* (1, 96 bytes est., 0x5B5A70-0x5B5A70)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5B5A70 | `unspot_stack` | `void (int)` | 96 | sort (0x5B5690) (992) | want_to_wake (0x5B5EA0) (1072) |

### blink2_timer (1, 80 bytes est., 0x50EE30-0x50EE30)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x50EE30 | `blink2_timer` | `void (int)` | 80 | convoy (0x50E5C0) (2160) | start_timers (0x50F3D0) (1440) |

### act_* (1, 96 bytes est., 0x50C2E0-0x50C2E0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x50C2E0 | `act_of_aggression` | `int (int, int)` | 96 | bit_count (0x50BA30) (2224) | steal_energy (0x50C4B0) (464) |

### stack_* (1, 112 bytes est., 0x5B9510-0x5B9510)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5B9510 | `stack_kill` | `void (int)` | 112 | stack_fix (0x5B8E10) (1792) | stack_check (0x5B9580) (112) |

### X_* (1, 128 bytes est., 0x5BECA0-0x5BECA0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5BECA0 | `X_text_open` | `int (const char*, const char*)` | 128 | tech_rate (0x5BE6B0) (1520) | is_sensor (0x5BF010) (880) |

### most_* (1, 144 bytes est., 0x5AC5A0-0x5AC5A0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5AC5A0 | `most_objectives` | `int (int*, int*)` | 144 | is_objective (0x5AC060) (1344) | transcending (0x5AC630) (144) |

### mandate_* (1, 112 bytes est., 0x50E820-0x50E820)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x50E820 | `mandate_color` | `int (int)` | 112 | convoy (0x50E5C0) (608) | start_timers (0x50F3D0) (2992) |

### set_* (1, 112 bytes est., 0x592A80-0x592A80)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x592A80 | `set_dirty` | `int ()` | 112 | kill_landmark (0x5926F0) (912) | hex_cost (0x593510) (2704) |

### checksum_* (1, 176 bytes est., 0x538FE0-0x538FE0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x538FE0 | `checksum_file` | `uint8_t (char*)` | 176 | my_srand (0x538FB0) (48) | checksum (0x539090) (176) |

### world_* (1, 176 bytes est., 0x5C5A30-0x5C5A30)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5C5A30 | `world_climate` | `int ()` | 176 | world_alt_put_detail (0x5C58C0) (368) | near_landmark (0x5C5BD0) (416) |

### find_* (1, 80 bytes est., 0x5882F0-0x5882F0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5882F0 | `find_font` | `int (int, int)` | 80 | read_rules (0x5873C0) (3888) | planetfall (0x589180) (3728) |

### line_* (1, 144 bytes est., 0x50EE80-0x50EE80)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x50EE80 | `line_timer` | `void (int)` | 144 | convoy (0x50E5C0) (2240) | start_timers (0x50F3D0) (1360) |

### is_* (1, 128 bytes est., 0x57DFC0-0x57DFC0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x57DFC0 | `is_bunged` | `bool (int, int, int, int, int, int)` | 128 | say_stats (0x57DAA0) (1312) | best_reactor (0x57EFA0) (4064) |

### scroll_* (1, 144 bytes est., 0x588340-0x588340)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x588340 | `scroll_normal` | `int ()` | 144 | read_rules (0x5873C0) (3968) | planetfall (0x589180) (3648) |

### fixup_* (1, 320 bytes est., 0x592940-0x592940)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x592940 | `fixup_landmarks` | `int ()` | 320 | kill_landmark (0x5926F0) (592) | hex_cost (0x593510) (3024) |

### upgrade_* (1, 304 bytes est., 0x4EFD50-0x4EFD50)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x4EFD50 | `upgrade_cost` | `int (int, int, int)` | 304 | pop_goal (0x4EF090) (3264) | base_queue (0x4F06E0) (2448) |

### steal_* (1, 368 bytes est., 0x50C340-0x50C340)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x50C340 | `steal_tech` | `int (int, int, int)` | 368 | bit_count (0x50BA30) (2320) | steal_energy (0x50C4B0) (368) |

### delete_* (1, 368 bytes est., 0x5927D0-0x5927D0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5927D0 | `delete_landmark` | `void (int, int)` | 368 | kill_landmark (0x5926F0) (224) | hex_cost (0x593510) (3392) |

### say_* (1, 432 bytes est., 0x592250-0x592250)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x592250 | `say_loc` | `void (char*, int, int, int, int, int)` | 432 | goody_at (0x592140) (272) | site_radius (0x592400) (432) |

### filefind_* (1, 464 bytes est., 0x600400-0x600400)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x600400 | `filefind_init` | `int (const char*, int)` | 464 | filefind_set_alternative (0x6003B0) (80) | filefind_get (0x6005D0) (464) |

### kill (1, 432 bytes est., 0x5C0B00-0x5C0B00)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5C0B00 | `kill` | `void (int)` | 432 | veh_clear (0x5C02D0) (2096) | can_arty (0x5C0DB0) (688) |

### auto_* (1, 432 bytes est., 0x50E890-0x50E890)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x50E890 | `auto_play_callback` | `void (int)` | 432 | convoy (0x50E5C0) (720) | start_timers (0x50F3D0) (2880) |

### diplomacy_* (1, 464 bytes est., 0x5399A0-0x5399A0)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x5399A0 | `diplomacy_caption` | `void (int, int)` | 464 | auto_contact (0x539160) (2112) | great_beelzebub (0x539B70) (464) |

### do_* (1, 512 bytes est., 0x4EFB50-0x4EFB50)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x4EFB50 | `do_upgrade` | `void (int, int, int)` | 512 | pop_goal (0x4EF090) (2752) | base_queue (0x4F06E0) (2960) |

### popup wrappers (66, 5440 bytes est., 0x50B970-0x627B60)

| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |
|---|---|---|---|---|---|
| 0x50B970 | `popp_2` | `int (const char*, const char*, fp_none)` | 48 | get_noun (0x50B930) (64) | say_stats_3 (0x50B9A0) (48) |
| 0x50B9E0 | `NetMsg_pop_2` | `int (const char* label, const char* filename)` | 32 | say_stats_3 (0x50B9A0) (64) | bitmask (0x50BA00) (32) |
| 0x5398E0 | `pop_wait` | `int ()` | 64 | auto_contact (0x539160) (1920) | great_beelzebub (0x539B70) (656) |
| 0x539920 | `pop_expire` | `int ()` | 128 | auto_contact (0x539160) (1984) | great_beelzebub (0x539B70) (592) |
| 0x5A5990 | `Popup_start_4` | `void (Popup* This, const char* label)` | 32 | vector_dist (0x5A5910) (128) | veh_put (0x5A59B0) (32) |
| 0x5BF310 | `X_pop` | `int (const char*, fp_none)` | 32 | has_abil (0x5BF1F0) (288) | veh_at (0x5BFE90) (2944) |
| 0x5BF330 | `X_pop_2` | `int (const char*, const char*, fp_none)` | 32 | has_abil (0x5BF1F0) (320) | veh_at (0x5BFE90) (2912) |
| 0x5BF350 | `X_pop_3` | `int (const char*, const char*, char*, int, fp_none)` | 64 | has_abil (0x5BF1F0) (352) | veh_at (0x5BFE90) (2880) |
| 0x5BF390 | `X_pop_4` | `int (const char*, int, int, fp_none)` | 48 | has_abil (0x5BF1F0) (416) | veh_at (0x5BFE90) (2816) |
| 0x5BF3C0 | `X_pop_5` | `int (const char*, const char*, int, int, fp_none)` | 48 | has_abil (0x5BF1F0) (464) | veh_at (0x5BFE90) (2768) |
| 0x5BF3F0 | `X_pop_6` | `int (const char*, int, fp_none)` | 48 | has_abil (0x5BF1F0) (512) | veh_at (0x5BFE90) (2720) |
| 0x5BF420 | `X_pop_7` | `int (const char*, const char*, int, fp_none)` | 48 | has_abil (0x5BF1F0) (560) | veh_at (0x5BFE90) (2672) |
| 0x5BF450 | `X_pop_8` | `int (const char*, int, char*, int, fp_none)` | 48 | has_abil (0x5BF1F0) (608) | veh_at (0x5BFE90) (2624) |
| 0x5BF480 | `X_pop_9` | `int (const char*, const char*, int, char*, int, fp_none)` | 336 | has_abil (0x5BF1F0) (656) | veh_at (0x5BFE90) (2576) |
| 0x5BF5D0 | `X_pops` | `int (const char*, Sprite*, fp_none)` | 48 | has_abil (0x5BF1F0) (992) | veh_at (0x5BFE90) (2240) |
| 0x5BF600 | `X_pops_2` | `int (const char*, Sprite*, int, int, fp_none)` | 48 | has_abil (0x5BF1F0) (1040) | veh_at (0x5BFE90) (2192) |
| 0x5BF630 | `X_pops_3` | `int (const char*, const char*, Sprite*, fp_none)` | 48 | has_abil (0x5BF1F0) (1088) | veh_at (0x5BFE90) (2144) |
| 0x5BF660 | `X_pops_4` | `int (const char*, const char*, Sprite*, int, int, fp_none)` | 48 | has_abil (0x5BF1F0) (1136) | veh_at (0x5BFE90) (2096) |
| 0x5BF690 | `X_pops_5` | `int (const char*, int, int, Sprite*, fp_none)` | 48 | has_abil (0x5BF1F0) (1184) | veh_at (0x5BFE90) (2048) |
| 0x5BF6C0 | `X_pops_6` | `int (const char*, int, int, Sprite*, int, int, fp_none)` | 48 | has_abil (0x5BF1F0) (1232) | veh_at (0x5BFE90) (2000) |
| 0x5BF6F0 | `X_pops_7` | `int (const char*, const char*, char*, int, Sprite*, fp_none)` | 64 | has_abil (0x5BF1F0) (1280) | veh_at (0x5BFE90) (1952) |
| 0x5BF730 | `X_pops_8` | `int (const char*, const char*, char*, int, Sprite*, int, int, fp_none)` | 64 | has_abil (0x5BF1F0) (1344) | veh_at (0x5BFE90) (1888) |
| 0x5BF770 | `X_pops_9` | `int (const char*, const char*, int, int, Sprite*, fp_none)` | 48 | has_abil (0x5BF1F0) (1408) | veh_at (0x5BFE90) (1824) |
| 0x5BF7A0 | `X_pops_10` | `int (const char*, const char*, int, int, Sprite*, int, int, fp_none)` | 48 | has_abil (0x5BF1F0) (1456) | veh_at (0x5BFE90) (1776) |
| 0x5BF7D0 | `X_pops_11` | `int (const char*, int, Sprite*, fp_none)` | 48 | has_abil (0x5BF1F0) (1504) | veh_at (0x5BFE90) (1728) |
| 0x5BF800 | `X_pops_12` | `int (const char*, int, Sprite*, int, int, fp_none)` | 48 | has_abil (0x5BF1F0) (1552) | veh_at (0x5BFE90) (1680) |
| 0x5BF830 | `X_pops_13` | `int (const char*, const char*, int, Sprite*, fp_none)` | 48 | has_abil (0x5BF1F0) (1600) | veh_at (0x5BFE90) (1632) |
| 0x5BF860 | `X_pops_14` | `int (const char*, const char*, int, Sprite*, int, int, fp_none)` | 48 | has_abil (0x5BF1F0) (1648) | veh_at (0x5BFE90) (1584) |
| 0x5BF890 | `X_pops_15` | `int (const char*, int, char*, int, Sprite*, fp_none)` | 48 | has_abil (0x5BF1F0) (1696) | veh_at (0x5BFE90) (1536) |
| 0x5BF8C0 | `X_pops_16` | `int (const char*, int, char*, int, Sprite*, int, int, fp_none)` | 64 | has_abil (0x5BF1F0) (1744) | veh_at (0x5BFE90) (1488) |
| 0x5BF900 | `X_pops_17` | `int (const char*, const char*, int, char*, int, Sprite*, fp_none)` | 48 | has_abil (0x5BF1F0) (1808) | veh_at (0x5BFE90) (1424) |
| 0x5BF930 | `X_pops_18` | `int (const char*, const char*, int, char*, int, Sprite*, int, int, fp_none)` | 384 | has_abil (0x5BF1F0) (1856) | veh_at (0x5BFE90) (1376) |
| 0x5BFAB0 | `X_pop_ask` | `int (const char*, int, char*, fp_none, int)` | 48 | has_abil (0x5BF1F0) (2240) | veh_at (0x5BFE90) (992) |
| 0x5BFAE0 | `X_pop_ask_2` | `int (const char*, const char*, int, fp_none, int)` | 48 | has_abil (0x5BF1F0) (2288) | veh_at (0x5BFE90) (944) |
| 0x5BFB10 | `X_pop_ask_3` | `int (const char*, int, fp_none, int)` | 48 | has_abil (0x5BF1F0) (2336) | veh_at (0x5BFE90) (896) |
| 0x5BFB40 | `X_pop_ask_4` | `int (const char*, const char*, char*, fp_none, int)` | 64 | has_abil (0x5BF1F0) (2384) | veh_at (0x5BFE90) (848) |
| 0x5BFB80 | `X_pop_ask_5` | `int (const char*, char*, fp_none, int)` | 64 | has_abil (0x5BF1F0) (2448) | veh_at (0x5BFE90) (784) |
| 0x5BFBC0 | `X_pop_ask_6` | `int (const char*, const char*, int, char*, fp_none, int)` | 336 | has_abil (0x5BF1F0) (2512) | veh_at (0x5BFE90) (720) |
| 0x5BFD10 | `X_pop_ask_number` | `int (const char*, int, fp_none)` | 32 | has_abil (0x5BF1F0) (2848) | veh_at (0x5BFE90) (384) |
| 0x5BFD30 | `X_pop_ask_number_2` | `int (const char*, const char*, fp_none)` | 32 | has_abil (0x5BF1F0) (2880) | veh_at (0x5BFE90) (352) |
| 0x5BFD50 | `X_pop_ask_number_3` | `int (const char*, fp_none)` | 32 | has_abil (0x5BF1F0) (2912) | veh_at (0x5BFE90) (320) |
| 0x5BFD70 | `X_pop_ask_number_4` | `int (const char*, const char*, int, fp_none)` | 288 | has_abil (0x5BF1F0) (2944) | veh_at (0x5BFE90) (288) |
| 0x6273A0 | `pops_4` | `int (const char*, const char*, Sprite*, int, int, fp_none)` | 48 | set_language (0x627100) (672) | filemap_filemap (0x628380) (4064) |
| 0x6273D0 | `pops_5` | `int (const char*, int, int, Sprite*, fp_none)` | 48 | set_language (0x627100) (720) | filemap_filemap (0x628380) (4016) |
| 0x627400 | `pops_6` | `int (const char*, int, int, Sprite*, int, int, fp_none)` | 64 | set_language (0x627100) (768) | filemap_filemap (0x628380) (3968) |
| 0x627440 | `pops_7` | `int (const char*, const char*, const char*, int, Sprite*, fp_none)` | 64 | set_language (0x627100) (832) | filemap_filemap (0x628380) (3904) |
| 0x627480 | `pops_8` | `int (const char*, const char*, const char*, int, Sprite*, int, int, fp_none)` | 80 | set_language (0x627100) (896) | filemap_filemap (0x628380) (3840) |
| 0x6274D0 | `pops_9` | `int (const char*, const char*, int, int, Sprite*, fp_none)` | 48 | set_language (0x627100) (976) | filemap_filemap (0x628380) (3760) |
| 0x627500 | `pops_10` | `int (const char*, const char*, int, int, Sprite*, int, int, fp_none)` | 64 | set_language (0x627100) (1024) | filemap_filemap (0x628380) (3712) |
| 0x627540 | `pops_11` | `int (const char*, int, Sprite*, fp_none)` | 48 | set_language (0x627100) (1088) | filemap_filemap (0x628380) (3648) |
| 0x627570 | `pops_12` | `int (const char*, int, Sprite*, int, int, fp_none)` | 48 | set_language (0x627100) (1136) | filemap_filemap (0x628380) (3600) |
| 0x6275A0 | `pops_13` | `int (const char*, const char*, int, Sprite*, fp_none)` | 48 | set_language (0x627100) (1184) | filemap_filemap (0x628380) (3552) |
| 0x6275D0 | `pops_14` | `int (const char*, const char*, int, Sprite*, int, int, fp_none)` | 48 | set_language (0x627100) (1232) | filemap_filemap (0x628380) (3504) |
| 0x627600 | `pops_15` | `int (const char*, int, char*, int, Sprite*, fp_none)` | 48 | set_language (0x627100) (1280) | filemap_filemap (0x628380) (3456) |
| 0x627630 | `pops_16` | `int (const char*, int, char*, int, Sprite*, int, int, fp_none)` | 64 | set_language (0x627100) (1328) | filemap_filemap (0x628380) (3408) |
| 0x627670 | `pops_17` | `int (const char*, const char*, int, const char*, int, Sprite*, fp_none)` | 48 | set_language (0x627100) (1392) | filemap_filemap (0x628380) (3344) |
| 0x6276A0 | `pops_18` | `int (const char*, const char*, int, const char*, int, Sprite*, int, int, fp_none)` | 352 | set_language (0x627100) (1440) | filemap_filemap (0x628380) (3296) |
| 0x627800 | `pop_ask` | `int (const char*, int, char*, fp_none, int)` | 48 | set_language (0x627100) (1792) | filemap_filemap (0x628380) (2944) |
| 0x627830 | `pop_ask_2` | `int (const char*, const char*, int, fp_none, int)` | 48 | set_language (0x627100) (1840) | filemap_filemap (0x628380) (2896) |
| 0x627860 | `pop_ask_3` | `int (const char*, int, fp_none, int)` | 48 | set_language (0x627100) (1888) | filemap_filemap (0x628380) (2848) |
| 0x627890 | `pop_ask_4` | `int (const char*, const char*, const char*, fp_none, int)` | 64 | set_language (0x627100) (1936) | filemap_filemap (0x628380) (2800) |
| 0x6278D0 | `pop_ask_5` | `int (const char*, const char*, fp_none, int)` | 64 | set_language (0x627100) (2000) | filemap_filemap (0x628380) (2736) |
| 0x627910 | `pop_ask_6` | `int (const char*, const char*, int, const char*, fp_none, int)` | 176 | set_language (0x627100) (2064) | filemap_filemap (0x628380) (2672) |
| 0x6279C0 | `pop_ask_number` | `int (const char*, int, fp_none)` | 208 | set_language (0x627100) (2240) | filemap_filemap (0x628380) (2496) |
| 0x627A90 | `pop_ask_number_2` | `int (const char*, const char*, fp_none)` | 208 | set_language (0x627100) (2448) | filemap_filemap (0x628380) (2288) |
| 0x627B60 | `pop_ask_number_3` | `int (const char*, fp_none)` | 208 | set_language (0x627100) (2656) | filemap_filemap (0x628380) (2080) |


## Already referenced by OpenSMACX through `src/temp.cpp` pointers, not decompiled (26)

OpenSMACX code already calls these exe functions through typed pointers, so decompiling them removes a dependency on the original binary. Both names are given where they differ.

| VA | OpenSMACX name | Thinker name | Thinker signature | est. size | area |
|---|---|---|---|---|---|
| 0x4455F0 | `wave_it` | `wave_it` | `void (int)` | 96 | GENERAL |
| 0x453710 | `load_faction_art` | `load_faction_art` | `void (int)` | 784 | MAP |
| 0x46AEF0 | `draw_radius` | `draw_radius` | `void (int, int, int, int)` | 80 | MAP |
| 0x46B190 | `draw_map` | `draw_map` | `void (int)` | 64 | MAP |
| 0x471240 | `mapwin_terrain_fixup` | `mapwin_terrain_fixup` | `int ()` | 96 | MAP |
| 0x48C0A0 | `popp` | `popp` | `int (const char* filename, const char* label, int a3, const char* pcx_filename, fp_none fn)` | 640 | MAP |
| 0x48C650 | `popb` | `popb` | `int (const char* label, int flags, int sound_id, const char* pcx_filename, Sprite* a5)` | 2144 | MAP |
| 0x4E3A50 | `base_at` | `base_at` | `int (int, int)` | 304 | BASE |
| 0x50EA40 | `blink_timer` | `blink_timer` | `void (int)` | 1008 | MAP |
| 0x50EE30 | `blink2_timer` | `blink2_timer` | `void (int)` | 80 | TIME |
| 0x50EE80 | `line_timer` | `line_timer` | `void (int)` | 144 | TIME |
| 0x50EF10 | `turn_timer` | `turn_timer` | `void (int unused)` | 64 | TIME |
| 0x55BC80 | `wants_to_attack_` | `wants_to_attack` | `int (int, int, int)` | 1968 | FACTION |
| 0x581260 | `consider_designs` | `consider_designs` | `void (int)` | 10832 | VEH |
| 0x591290 | `alt_set` | `alt_set` | `void (int, int, int)` | 1552 | MAP |
| 0x592940 | `fixup_landmarks` | `fixup_landmarks` | `int ()` | 320 | MAP |
| 0x5A94F0 | `save_daemon` | `save_daemon` | `int (char*)` | 480 | FACTION |
| 0x5A9760 | `load_daemon` | `load_daemon` | `int (char*, int)` | 960 | FACTION |
| 0x5B4600 | `social_set` | `social_set` | `void (int)` | 304 | FACTION |
| 0x5BF310 | `X_pop` | `X_pop` | `int (const char*, fp_none)` | 32 | VEH |
| 0x5C4470 | `world_rainfall` | `world_rainfall` | `int ()` | 2912 | MAP |
| 0x5FCA30 | `do_non_input_` | `do_non_input` | `int ()` | 240 | TIME |
| 0x62D5B0 | `do_net` | `do_net` | `int ()` | 32 | GENERAL |
| 0x62D5D0 | `check_net` | `check_net` | `int ()` | 208 | GENERAL |
| 0x636300 | `do_video` | `do_video` | `int ()` | 60402 | GENERAL |
| 0x644EF2 | `_free` | `mem_free` | `void (void*)` | 4395 | GENERAL |

Temp pointers at .text addresses Thinker does not name: `_fopen` (0x645646), `_srand` (0x646010), `_fwrite` (0x64603F), `_fread` (0x646178), `_fgetc` (0x646855), `_fputc` (0x646886), `_malloc` (0x6470A6)

## Name matches: in DLL exports but not redirected (47)

Thinker names these addresses with a name that also appears (as `Class_name` / `name`) in the DLL export table. OpenSMACX has decompiled these (they are in the DLL) but the patcher does not yet redirect the exe to them, so the exe still runs the original code. Marked where `src/temp.cpp` keeps an `_OG` pointer to the original for comparison testing. Do not re-decompile these; the open question is only why they are not yet redirected.

| VA | Thinker name | area | note |
|---|---|---|---|
| 0x4E4350 | `base_mark` | BASE |  |
| 0x4EA540 | `base_psych` | FACTION |  |
| 0x501DA0 | `battle_compute` | VEH |  |
| 0x5044D0 | `best_defender` | VEH | `_OG` pointer kept |
| 0x54CB50 | `value_of_base` | BASE | `_OG` pointer kept |
| 0x55BC80 | `wants_to_attack` | FACTION |  |
| 0x560DD0 | `enemy_capabilities` | FACTION | `_OG` pointer kept |
| 0x59A530 | `Path_find` | PATH |  |
| 0x59BC10 | `Path_move` | PATH |  |
| 0x59CCA0 | `Path_sensors` | PATH |  |
| 0x5B4790 | `social_ai` | FACTION |  |
| 0x5C2020 | `world_alt_set` | VEH |  |
| 0x5C2380 | `world_raise_alt` | VEH |  |
| 0x5C23E0 | `world_lower_alt` | VEH |  |
| 0x5C2440 | `brush` | VEH |  |
| 0x5C27F0 | `paint_land` | VEH |  |
| 0x5C28F0 | `build_continent` | VEH |  |
| 0x5C2B40 | `build_hills` | VEH |  |
| 0x5C3680 | `world_riverbeds` | VEH |  |
| 0x5C40F0 | `world_validate` | MAP |  |
| 0x5C4170 | `world_temperature` | MAP |  |
| 0x5C55C0 | `world_analysis` | MAP |  |
| 0x5C58E0 | `world_polar_caps` | MAP |  |
| 0x5C5AE0 | `world_linearize_contours` | MAP |  |
| 0x5C5C70 | `world_crater` | MAP |  |
| 0x5C5EF0 | `world_monsoon` | MAP |  |
| 0x5C6200 | `world_sargasso` | MAP |  |
| 0x5C64A0 | `world_ruin` | MAP |  |
| 0x5C6740 | `world_dune` | MAP |  |
| 0x5C69E0 | `world_diamond` | MAP |  |
| 0x5C6C40 | `world_fresh` | MAP |  |
| 0x5C6DB0 | `world_volcano` | MAP |  |
| 0x5C7020 | `world_borehole` | MAP |  |
| 0x5C7540 | `world_temple` | GAME |  |
| 0x5C7750 | `world_unity` | GAME |  |
| 0x5C7A80 | `world_fossil` | GAME |  |
| 0x5C7CB0 | `world_canyon` | GAME |  |
| 0x5C7F40 | `world_mesa` | GAME |  |
| 0x5C8150 | `world_ridge` | GAME |  |
| 0x5C83B0 | `world_geothermal` | GAME |  |
| 0x5C8630 | `world_landmarks` | GAME |  |
| 0x5FCA30 | `do_non_input` | TIME |  |
| 0x5FCB20 | `do_all_non_input` | TIME |  |
| 0x5FCB60 | `do_draw` | TIME |  |
| 0x5FCBB0 | `do_all_draws` | TIME |  |
| 0x5FCC20 | `do_keyboard` | TIME |  |
| 0x5FCC70 | `do_all_keyboard` | TIME |  |

## All candidates by nearest patcher area

### BASE (286 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x401000 | `StringStruct_ctor` | `DWORD (StringStruct* This, int a2)` | 96 | None | 132720 |
| 0x401060 | `StringStruct_close` | `int (StringStruct* This)` | 160 | None | 132624 |
| 0x401100 | `StringStruct_add` | `int (StringStruct* This, int a2)` | 1120 | None | 132464 |
| 0x401560 | `StringStruct_seek_id` | `int (StringStruct* This, int a2)` | 80 | None | 131344 |
| 0x4015B0 | `StringStruct_seek_pos` | `int (StringStruct* This, int a2)` | 144 | None | 131264 |
| 0x401640 | `StringStruct_current_id` | `int (StringStruct* This)` | 3776 | None | 131120 |
| 0x402500 | `StringStruct_next_entry` | `StringStructNode* (StringStruct* This)` | 48 | None | 127344 |
| 0x402530 | `StringStruct_current_entry` | `StringStructNode* (StringStruct* This)` | 1088 | None | 127296 |
| 0x402970 | `StringStruct_remove_all` | `int (StringStruct* This)` | 128 | None | 126208 |
| 0x4029F0 | `StringStruct_scalar_dtor` | `void* (StringStruct* This, uint32_t flags)` | 1360 | None | 126080 |
| 0x402F40 | `AlphaMenu_init` | `int (Menu* This, Win* a2)` | 608 | None | 124720 |
| 0x4031A0 | `AlphaMenu_compute` | `int (Menu* This)` | 256 | None | 124112 |
| 0x4032A0 | `AlphaMenu_adjust` | `int (Menu* This)` | 160 | None | 123856 |
| 0x403340 | `AlphaMenu_on_redraw` | `int (Menu* This)` | 528 | None | 123696 |
| 0x403550 | `AlphaMenu_on_mouse_move` | `void (Menu* This, int x, int y)` | 192 | None | 123168 |
| 0x403610 | `AlphaMenu_requested_height` | `int (Menu* This)` | 16 | None | 122976 |
| 0x403620 | `AlphaMenu_on_adjust_pulldown_pos` | `void (Menu* This, int* a2, int* a3)` | 96 | None | 122960 |
| 0x403680 | `AlphaMenu_show` | `int (Menu* This)` | 432 | None | 122864 |
| 0x403830 | `AlphaMenu_hide` | `int (Menu* This)` | 704 | None | 122432 |
| 0x403AF0 | `AlphaMenu_fill_func` | `int (GraphicWin* a1, int a2, RECT* a3)` | 128 | None | 121728 |
| 0x403B70 | `AlphaMenu_backfill_func` | `int (GraphicWin* a1)` | 48 | None | 121600 |
| 0x403BA0 | `amovie_project` | `void (int)` | 64 | None | 121552 |
| 0x403BE0 | `amovie_project_2` | `void (const char*)` | 3200 | None | 121488 |
| 0x404860 | `sprite_draw_func` | `void (Sprite*, GraphicWin*, int, int, int)` | 64 | None | 118288 |
| 0x4048A0 | `Popup_ctor` | `int (Popup* This)` | 96 | None | 118224 |
| 0x404900 | `Popup_close` | `int (Popup* This)` | 32 | None | 118128 |
| 0x404920 | `Popup_show` | `void (Popup* This, int a2)` | 352 | None | 118096 |
| 0x404A80 | `Popup_hide` | `int (Popup* This)` | 128 | None | 117744 |
| 0x404B00 | `Popup_on_redraw` | `int (Popup* This)` | 16 | None | 117616 |
| 0x404B10 | `Popup_calculate_dimensions` | `int (Popup* This)` | 992 | None | 117600 |
| 0x404EF0 | `Popup_on_post_window_init` | `int (Popup* This)` | 144 | None | 116608 |
| 0x404F80 | `Popup_on_redraw_nc` | `void (Popup* This, RECT* a2, int a3)` | 16 | None | 116464 |
| 0x404F90 | `Popup_on_nc_hittest` | `int (Popup* This, int a2, int a3)` | 32 | None | 116448 |
| 0x404FB0 | `Popup_alloc` | `int ()` | 112 | None | 116416 |
| 0x405020 | `Popup_on_adjust_button_width` | `int (Popup* This)` | 48 | None | 116304 |
| 0x405050 | `Popup_on_adjust_button_placement` | `void (Popup* This, int* a2, int* a3, int* a4)` | 80 | None | 116256 |
| 0x4050A0 | `Popup_on_link_clicked` | `int (Popup* This, int a2, char* a3)` | 160 | None | 116176 |
| 0x405140 | `POP2` | `int (const char* label, const char* pcx_filename, int a3)` | 1120 | None | 116016 |
| 0x4055A0 | `POP3` | `void (const char* label, const char* pcx_filename, int a3, int a4, int a5)` | 880 | None | 114896 |
| 0x405910 | `Popup_on_exec` | `int (Popup* This)` | 512 | None | 114016 |
| 0x405B10 | `Popup_on_dialog_back_draw` | `int (Popup* This, GraphicWin* a2)` | 16 | None | 113504 |
| 0x405B20 | `Popup_on_dialog_item_back_draw` | `int (Popup* This, GraphicWin* a2, int a3, int a4, RECT* a5)` | 352 | None | 113488 |
| 0x405C80 | `Popup_on_draw_background` | `int (Popup* This)` | 1504 | None | 113136 |
| 0x406260 | `Popup_on_listbox_scroll_create` | `int (Popup* This)` | 208 | None | 111632 |
| 0x406330 | `Popup_on_listbox_scroll_delete` | `int (Popup* This, Scroll* a2)` | 80 | None | 111424 |
| 0x406380 | `Popup_start` | `int (Popup* This, const char* filename, const char* label, int a4, char* a5, int a6, GraphicWin* a7)` | 336 | None | 111344 |
| 0x4064D0 | `BasePop_dtor` | `int (BasePop* This)` | 896 | None | 111008 |
| 0x406850 | `Sprite_dtor` | `int (Sprite* This)` | 16 | None | 110112 |
| 0x406860 | `BaseButton_hide` | `int (BaseButton* This)` | 928 | None | 110096 |
| 0x406C00 | `Popup_dtor` | `int (Popup* This)` | 1200 | None | 109168 |
| 0x4070B0 | `BasePop_operator_delete` | `void (void* a1, uint32_t a2)` | 32 | None | 107968 |
| 0x4070D0 | `Popup_operator_delete` | `void (void* a1, uint32_t a2)` | 4928 | None | 107936 |
| 0x408410 | `load_inverts` | `int ()` | 128 | None | 103008 |
| 0x408490 | `BaseWin_ctor` | `int (BaseWindow* This)` | 656 | None | 102880 |
| 0x408720 | `BaseWin_psych_row` | `void (BaseWindow* This, int a2, int a3, int a4, int a5, int a6, int a7, int a8)` | 1024 | None | 102224 |
| 0x408B20 | `BaseWin_draw_psych` | `int (BaseWindow* This)` | 912 | None | 101200 |
| 0x408EB0 | `BaseWin_draw_support` | `int (BaseWindow* This)` | 528 | None | 100288 |
| 0x4090C0 | `BaseWin_UNK1` | `int (BaseWindow* This, int a2)` | 64 | None | 99760 |
| 0x409100 | `BaseWin_farm_get` | `int (BaseWindow* This, int y, int a3)` | 256 | None | 99696 |
| 0x409200 | `BaseWin_queue_click` | `void (BaseWindow* This, int a2, int a3, int a4, int a5)` | 4048 | None | 99440 |
| 0x40A1D0 | `BaseWin_farm_click` | `void (BaseWindow* This, int y, int a3, int a4)` | 1712 | None | 95392 |
| 0x40A880 | `BaseWin_pop_click` | `void (BaseWindow* This, int a2, int a3, int a4, int a5)` | 2208 | None | 93680 |
| 0x40B120 | `BaseWin_UNK2` | `void (BaseWindow* This, int veh_id, int a3, int a4, int a5)` | 32 | None | 91472 |
| 0x40B140 | `BaseWin_garrison_click` | `void (BaseWindow* This, int veh_id, int a3, int a4, int a5)` | 3888 | None | 91440 |
| 0x40C070 | `BaseWin_facilities_click` | `int (BaseWindow* This, int entry_id, int a3, int a4, int a5)` | 2016 | None | 87552 |
| 0x40C850 | `BaseWin_draw_supported` | `void (BaseWindow* This, int a2)` | 2560 | None | 85536 |
| 0x40D250 | `BaseWin_draw_flash` | `void (BaseWindow* This, int a2)` | 4368 | None | 82976 |
| 0x40E360 | `BaseWin_draw_production` | `void (BaseWindow* This, int a2)` | 3472 | None | 78608 |
| 0x40F0F0 | `BaseWin_draw_farm` | `void (BaseWindow* This, int a2)` | 3024 | None | 75136 |
| 0x40FCC0 | `BaseWin_draw_facilities` | `void (BaseWindow* This, int a2)` | 1776 | None | 72112 |
| 0x4103B0 | `BaseWin_draw_garrison` | `void (BaseWindow* This, int a2)` | 1216 | None | 70336 |
| 0x410870 | `BaseWin_draw_commerce` | `void (BaseWindow* This, int a2)` | 2512 | None | 69120 |
| 0x411240 | `BaseWin_draw_nutrients` | `void (BaseWindow* This, int a2)` | 1856 | None | 66608 |
| 0x411980 | `BaseWin_draw_energy_alloc` | `void (BaseWindow* This, int a2)` | 4384 | None | 64752 |
| 0x412AA0 | `BaseWin_draw_expenses` | `void (BaseWindow* This, int a2)` | 8352 | None | 60368 |
| 0x414B40 | `BaseWin_draw_pop` | `void (BaseWindow* This, int a2)` | 1344 | None | 52016 |
| 0x415080 | `BaseWin_draw_queue` | `void (BaseWindow* This, int a2)` | 1712 | None | 50672 |
| 0x415730 | `BaseWin_draw_misc` | `void (BaseWindow* This, int a2)` | 1008 | None | 48960 |
| 0x415B20 | `BaseWin_draw_info` | `void (BaseWindow* This, int a2)` | 688 | None | 47952 |
| 0x415DD0 | `BaseWin_focus` | `int (BaseWindow* This)` | 144 | None | 47264 |
| 0x415E60 | `BaseWin_UNK3` | `void (BaseWindow* This, int base_id)` | 160 | None | 47120 |
| 0x415F00 | `BaseWin_gov_activate` | `int (BaseWindow* This)` | 112 | None | 46960 |
| 0x415F70 | `BaseWin_gov_on` | `void (BaseWindow* This, int a2)` | 384 | None | 46848 |
| 0x4160F0 | `BaseWin_iface_click` | `void (BaseWindow* This, int x, int y, int a4, int a5)` | 1104 | None | 46464 |
| 0x416540 | `BaseWin_bubble_me_elmo2` | `void (BaseWindow* This, char* a2, int a3, int a4)` | 64 | None | 45360 |
| 0x416580 | `BaseWin_bubble_me_elmo2_2` | `void (char* a1, int a2, int a3)` | 80 | None | 45296 |
| 0x4165D0 | `BaseWin_click` | `void (BaseWindow* This, int x, int y, int a4, int a5)` | 1984 | None | 45216 |
| 0x416D90 | `BaseWin_recompute` | `void (BaseWindow* This, int a2)` | 80 | None | 43232 |
| 0x416DE0 | `BaseWin_add_queue` | `void (BaseWindow* This, int a2, int a3)` | 464 | None | 43152 |
| 0x416FB0 | `BaseWin_delete_queue` | `void (BaseWindow* This, int a2)` | 144 | None | 42688 |
| 0x417040 | `BaseWin_production` | `void (BaseWindow* This, int a2, int a3)` | 7600 | None | 42544 |
| 0x418DF0 | `BaseWin_hurry` | `int (BaseWindow* This)` | 3232 | None | 34944 |
| 0x419A90 | `BaseWin_governor` | `int (BaseWindow* This, int a2)` | 1408 | None | 31712 |
| 0x41A010 | `BaseWin_prev` | `int (BaseWindow* This)` | 336 | None | 30304 |
| 0x41A160 | `BaseWin_next` | `int (BaseWindow* This)` | 336 | None | 29968 |
| 0x41A2B0 | `BaseWin_rename` | `int (BaseWindow* This)` | 528 | None | 29632 |
| 0x41A4C0 | `BaseWin_exit` | `int (BaseWindow* This)` | 336 | None | 29104 |
| 0x41A610 | `BaseWin_UNK5` | `void (BaseWindow* This, int a2)` | 16 | None | 28768 |
| 0x41A620 | `BaseWin_base_killed` | `void (BaseWindow* This, int base_id)` | 384 | None | 28752 |
| 0x41A7A0 | `BaseWin_check_base` | `void (BaseWindow* This, int base_id)` | 64 | None | 28368 |
| 0x41A7E0 | `BaseWin_check_loc` | `void (BaseWindow* This, int a2, int a3, int a4)` | 208 | None | 28304 |
| 0x41A8B0 | `BaseWin_check_veh` | `void (BaseWindow* This, int a2)` | 112 | None | 28096 |
| 0x41A920 | `BaseWin_zoom` | `void (BaseWindow* This, int base_id, int a3)` | 1312 | None | 27984 |
| 0x41AE40 | `BaseWin_soft_zoom` | `void (BaseWindow* This, int base_id)` | 256 | None | 26672 |
| 0x41AF40 | `BaseWin_timer_callback` | `void (BaseWindow* This, int, int)` | 16 | None | 26416 |
| 0x41AF50 | `BaseWin_on_key_click` | `void (BaseWindow* This, int a2, int a3)` | 32 | None | 26400 |
| 0x41AF70 | `BaseWin_on_left_click` | `void (BaseWindow* This, int x, int y)` | 32 | None | 26368 |
| 0x41AF90 | `BaseWin_on_right_click` | `void (BaseWindow* This, int x, int y)` | 32 | None | 26336 |
| 0x41AFB0 | `BaseWin_on_left_double_click` | `void (BaseWindow* This, int x, int y)` | 32 | None | 26304 |
| 0x41AFD0 | `BaseWin_on_left_up` | `void (BaseWindow* This, int a2, int a3)` | 320 | None | 26272 |
| 0x41B110 | `BaseWin_on_left_down` | `void (BaseWindow* This, int x, int y)` | 208 | None | 25952 |
| 0x41B1E0 | `BaseWin_on_mouse_leave` | `void (BaseWindow* This, int a2, int a3)` | 16 | None | 25744 |
| 0x41B1F0 | `BaseWin_on_mouse_move` | `void (BaseWindow* This, int x, int y)` | 688 | None | 25728 |
| 0x41B4A0 | `BaseWin_on_scrolled` | `void (BaseWindow* This, int a2, int a3)` | 32 | None | 25040 |
| 0x41B4C0 | `BaseWin_on_scrolling` | `void (BaseWindow* This, int a2, int a3)` | 32 | None | 25008 |
| 0x41B4E0 | `BaseWin_on_button_toggled` | `void (BaseWindow* This, int a2, int a3)` | 16 | None | 24976 |
| 0x41B4F0 | `BaseWin_nerve_staple` | `int (BaseWindow* This)` | 672 | None | 24960 |
| 0x41B790 | `BaseWin_base_editor_fac` | `int (BaseWindow* This)` | 1072 | None | 24288 |
| 0x41BBC0 | `BaseWin_base_editor_proj` | `int (BaseWindow* This)` | 1664 | None | 23216 |
| 0x41C240 | `BaseWin_base_editor_copy` | `void (BaseWindow* This, int a2)` | 1376 | None | 21552 |
| 0x41C7A0 | `BaseWin_base_editor` | `int (BaseWindow* This)` | 3424 | None | 20176 |
| 0x41D500 | `BaseWin_on_button_clicked` | `void (BaseWindow* This, int a2)` | 1488 | None | 16752 |
| 0x41DAD0 | `BaseWin_on_status` | `void (BaseWindow* This, int is_visible)` | 432 | None | 15264 |
| 0x41DC80 | `BaseWin_on_iface_scrolled` | `void (BaseWindow* This, int a2, int a3)` | 48 | None | 14832 |
| 0x41DCB0 | `BaseWin_on_iface_scrolling` | `void (BaseWindow* This, int a2, int a3)` | 32 | None | 14784 |
| 0x41DCD0 | `BaseWin_on_iface_left_click` | `void (BaseWindow* This, int x, int y)` | 32 | None | 14752 |
| 0x41DCF0 | `BaseWin_on_iface_right_click` | `void (BaseWindow* This, int x, int y)` | 32 | None | 14720 |
| 0x41DD10 | `BaseWin_on_iface_left_up` | `void (BaseWindow* This, int a2, int a3)` | 336 | None | 14688 |
| 0x41DE60 | `BaseWin_on_iface_left_down` | `void (BaseWindow* This, int x, int y)` | 224 | None | 14352 |
| 0x41DF40 | `BaseWin_on_iface_right_down` | `void (BaseWindow* This, int a2, int a3)` | 16 | None | 14128 |
| 0x41DF50 | `BaseWin_on_iface_mouse_move` | `void (BaseWindow* This, int x, int y)` | 1168 | None | 14112 |
| 0x41E3E0 | `BaseWin_on_iface_mouse_leave` | `void (BaseWindow* This, int a2, int a3)` | 224 | None | 12944 |
| 0x41E4C0 | `BaseWin_on_iface_selected` | `void (BaseWindow* This, int a2, int a3)` | 16 | None | 12720 |
| 0x41E4D0 | `BaseWin_on_iface_left_double_click` | `void (BaseWindow* This, int x, int y)` | 32 | None | 12704 |
| 0x41E4F0 | `BaseWin_on_iface_right_double_click` | `void (BaseWindow* This, int x, int y)` | 32 | None | 12672 |
| 0x41E510 | `BaseWin_on_iface_button_clicked` | `void (BaseWindow* This, int a2)` | 32 | None | 12640 |
| 0x41E530 | `BaseWin_on_iface_button_toggled` | `void (BaseWindow* This, int a2, int a3)` | 48 | None | 12608 |
| 0x41E560 | `BaseWin_update_gov_buttons` | `int (BaseWindow* This)` | 560 | None | 12560 |
| 0x41E790 | `BaseWin_on_redraw` | `int (BaseWindow* This)` | 256 | None | 12000 |
| 0x41E890 | `BaseWin_show` | `void (BaseWindow* This, int a2)` | 32 | None | 11744 |
| 0x41E8B0 | `BaseWin_hide` | `int (BaseWindow* This)` | 32 | None | 11712 |
| 0x41E8D0 | `BaseWin_init` | `int (BaseWindow* This)` | 8560 | None | 11680 |
| 0x420A40 | `BaseWin_on_nc_hittest` | `int (BaseWindow* This, int a2, int a3)` | 48 | None | 3120 |
| 0x420A70 | `BaseWin_dtor` | `int (BaseWindow* This)` | 1312 | None | 3072 |
| 0x420F90 | `MapWin_dtor` | `int (Console* This)` | 1760 | None | 1760 |
| 0x421810 | `BaseWin_operator_delete` | `void (void* a1, uint32_t a2)` | 416 | 416 | 5872 |
| 0x4219B0 | `BattleWin_atexit_dtor` | `int ()` | 32 | 832 | 5456 |
| 0x4219D0 | `BattleWin_dynamic_init` | `int ()` | 16 | 864 | 5424 |
| 0x4219E0 | `BattleWin_init` | `int (BattleWindow* This)` | 320 | 880 | 5408 |
| 0x421B20 | `BattleWin_pulse_timer` | `int (BattleWindow* This)` | 32 | 1200 | 5088 |
| 0x421B40 | `BattleWin_stop_timer` | `int (BattleWindow* This)` | 16 | 1232 | 5056 |
| 0x421B50 | `BattleWin_battle_report` | `void (BattleWindow* This, int a2, int a3, int a4, int a5, int a6)` | 4352 | 1248 | 5040 |
| 0x4D7BE0 | `Console_road_to` | `void (Console* This, int a2, int a3)` | 272 | 57744 | 48624 |
| 0x4D7CF0 | `Console_designate` | `void (Console* This, int veh_id)` | 240 | 58016 | 48352 |
| 0x4D7DE0 | `Console_go_home` | `void (Console* This, int veh_id)` | 256 | 58256 | 48112 |
| 0x4D7EE0 | `Console_set_patrol` | `void (Console* This, int veh_id)` | 128 | 58512 | 47856 |
| 0x4D7F60 | `Console_use_ability` | `void (Console* This, int veh_id)` | 2832 | 58640 | 47728 |
| 0x4D8A70 | `Console_automate` | `void (Console* This, int veh_id, int order_auto_type)` | 608 | 61472 | 44896 |
| 0x4D8CD0 | `Console_set_bombing_run` | `void (Console* This, int veh_id, int x, int y)` | 240 | 62080 | 44288 |
| 0x4D8DC0 | `Console_set_road_to` | `void (Console* This, int veh_id, int x, int y, int a5)` | 384 | 62320 | 44048 |
| 0x4D8F40 | `Console_explore` | `void (Console* This, int veh_id)` | 96 | 62704 | 43664 |
| 0x4D8FA0 | `Console_convoy` | `void (Console* This, int veh_id)` | 464 | 62800 | 43568 |
| 0x4D9170 | `Console_arty` | `void (Console* This, int veh_id)` | 352 | 63264 | 43104 |
| 0x4D92D0 | `Console_group` | `void (Console* This, int a2)` | 800 | 63616 | 42752 |
| 0x4D95F0 | `Console_assemble` | `int (Console* This)` | 896 | 64416 | 41952 |
| 0x4D9970 | `Console_editor_on` | `int (Console* This)` | 352 | 65312 | 41056 |
| 0x4D9AD0 | `Console_editor_who` | `int (Console* This, int a2, int position)` | 7584 | 65664 | 40704 |
| 0x4DB870 | `Console_editor_diplomacy` | `int (Console* This)` | 720 | 73248 | 33120 |
| 0x4DBB40 | `Console_editor_reset_faction` | `int (Console* This)` | 256 | 73968 | 32400 |
| 0x4DBC40 | `Console_editor_reset_tech` | `int (Console* This)` | 224 | 74224 | 32144 |
| 0x4DBD20 | `Console_editor_personality` | `int (Console* This)` | 656 | 74448 | 31920 |
| 0x4DBFB0 | `Console_editor_strategy` | `int (Console* This)` | 640 | 75104 | 31264 |
| 0x4DC230 | `Console_editor_rules` | `int (Console* This)` | 752 | 75744 | 30624 |
| 0x4DC520 | `Console_editor_scen_rules` | `int (Console* This)` | 832 | 76496 | 29872 |
| 0x4DC860 | `Console_editor_scen_victory` | `int (Console* This)` | 1120 | 77328 | 29040 |
| 0x4DCCC0 | `Console_editor_scen_param` | `int (Console* This)` | 1264 | 78448 | 27920 |
| 0x4DD1B0 | `Console_custom_my_faction` | `int (Console* This)` | 1344 | 79712 | 26656 |
| 0x4DD6F0 | `Console_editor_diff` | `int (Console* This)` | 864 | 81056 | 25312 |
| 0x4DDA50 | `Console_editor_edit_veh` | `void (Console* This, int veh_id)` | 3856 | 81920 | 24448 |
| 0x4DE960 | `Console_editor_kill_veh` | `void (Console* This, int veh_id)` | 928 | 85776 | 20592 |
| 0x4DED00 | `Console_editor_veh` | `int (Console* This)` | 2032 | 86704 | 19664 |
| 0x4DF4F0 | `Console_editor_view` | `int (Console* This)` | 1504 | 88736 | 17632 |
| 0x4DFAD0 | `Console_editor_tech` | `int (Console* This)` | 1616 | 90240 | 16128 |
| 0x4E0120 | `Console_editor_energy` | `int (Console* This)` | 240 | 91856 | 14512 |
| 0x4E0210 | `Console_editor_date` | `int (Console* This)` | 128 | 92096 | 14272 |
| 0x4E0290 | `Console_editor_reload` | `int (Console* This)` | 848 | 92224 | 14144 |
| 0x4E05E0 | `Console_editor_eliminate` | `int (Console* This)` | 976 | 93072 | 13296 |
| 0x4E09B0 | `Console_editor_kill_vehicles` | `int (Console* This)` | 48 | 94048 | 12320 |
| 0x4E09E0 | `Console_editor_save` | `int (Console* This)` | 32 | 94096 | 12272 |
| 0x4E0A00 | `Console_editor_load` | `int (Console* This)` | 80 | 94128 | 12240 |
| 0x4E0A50 | `Console_editor_paints` | `void (Console* This, int a2)` | 128 | 94208 | 12160 |
| 0x4E0AD0 | `Console_editor_minor` | `int (Console* This)` | 48 | 94336 | 12032 |
| 0x4E0B00 | `Console_editor_terrain` | `int (Console* This)` | 928 | 94384 | 11984 |
| 0x4E0EA0 | `Console_editor_set_brush` | `int (Console* This)` | 144 | 95312 | 11056 |
| 0x4E0F30 | `Console_editor_flat` | `int (Console* This)` | 80 | 95456 | 10912 |
| 0x4E0F80 | `Console_editor_polar` | `int (Console* This)` | 32 | 95536 | 10832 |
| 0x4E0FA0 | `Console_editor_climate` | `int (Console* This)` | 48 | 95568 | 10800 |
| 0x4E0FD0 | `Console_editor_generate` | `int (Console* This)` | 240 | 95616 | 10752 |
| 0x4E10C0 | `Console_editor_fast` | `int (Console* This)` | 208 | 95856 | 10512 |
| 0x4E1190 | `Console_editor_randomize` | `int (Console* This)` | 112 | 96064 | 10304 |
| 0x4E1200 | `Console_editor_clear_random` | `int (Console* This)` | 144 | 96176 | 10192 |
| 0x4E1290 | `Console_editor_clear` | `int (Console* This)` | 112 | 96320 | 10048 |
| 0x4E1300 | `Console_editor_clear_vehicles` | `int (Console* This)` | 128 | 96432 | 9936 |
| 0x4E1380 | `Console_editor_clear_terrain` | `int (Console* This)` | 96 | 96560 | 9808 |
| 0x4E13E0 | `Console_editor_fungus` | `int (Console* This)` | 192 | 96656 | 9712 |
| 0x4E14A0 | `Console_editor_rockiness` | `int (Console* This)` | 48 | 96848 | 9520 |
| 0x4E14D0 | `Console_editor_natural` | `void (Console* This, int a2)` | 2640 | 96896 | 9472 |
| 0x4E1F20 | `Console_editor_undo` | `int (Console* This)` | 16 | 99536 | 6832 |
| 0x4E1F30 | `Console_editor_redo` | `int (Console* This)` | 16 | 99552 | 6816 |
| 0x4E1F40 | `Console_edit_lock` | `int (Console* This)` | 48 | 99568 | 6800 |
| 0x4E1F70 | `Console_editor_lock2` | `int (Console* This)` | 144 | 99616 | 6752 |
| 0x4E2000 | `Console_corner_the_market` | `int (Console* This)` | 1168 | 99760 | 6608 |
| 0x4E2490 | `AlphaNet_ctor` | `int (AlphaNet* This)` | 288 | 100928 | 5440 |
| 0x4E25B0 | `AlphaNet_close` | `int (AlphaNet* This)` | 48 | 101216 | 5152 |
| 0x4E25E0 | `AlphaNet_pid_2_idx` | `int (AlphaNet* This, uint32_t a2)` | 48 | 101264 | 5104 |
| 0x4E2610 | `AlphaNet_pid_2_who` | `int (AlphaNet* This, uint32_t a2)` | 80 | 101312 | 5056 |
| 0x4E2660 | `AlphaNet_who_2_pid` | `int (AlphaNet* This, int a2)` | 80 | 101392 | 4976 |
| 0x4E26B0 | `AlphaNet_who_2_idx` | `void (AlphaNet* This, int a2)` | 48 | 101472 | 4896 |
| 0x4E26E0 | `AlphaNet_pick_service` | `int (AlphaNet* This, int* a2)` | 768 | 101520 | 4848 |
| 0x4E29E0 | `AlphaNet_do_join_or_create` | `int (AlphaNet* This)` | 80 | 102288 | 4080 |
| 0x4E2A30 | `AlphaNet_create` | `int (AlphaNet* This)` | 1056 | 102368 | 4000 |
| 0x4E2E50 | `AlphaNet_join` | `int (AlphaNet* This)` | 912 | 103424 | 2944 |
| 0x4E31E0 | `AlphaNet_setup` | `int (AlphaNet* This, int a2)` | 752 | 104336 | 2032 |
| 0x4E34D0 | `Net_dtor` | `int (Net* This)` | 1280 | 105088 | 1280 |
| 0x4E3A50 | `base_at` | `int (int, int)` | 304 | 80 | 304 |
| 0x4E4350 | `base_mark` | `void (int)` | 224 | 704 | 224 |
| 0x4E4B80 | `base_init` | `int (int, int, int)` | 1744 | 224 | 6272 |
| 0x4E5250 | `base_kill` | `void (int)` | 2064 | 1968 | 4528 |
| 0x4E5A60 | `base_change` | `void (int, int)` | 1408 | 4032 | 2464 |
| 0x4E5FE0 | `base_reset` | `void (int, int)` | 800 | 5440 | 1056 |
| 0x4E6300 | `bases_reset` | `void (int, int, int)` | 256 | 6240 | 256 |
| 0x4E68B0 | `farm_compute` | `int ()` | 1440 | 368 | 12992 |
| 0x4E6E50 | `crop_yield` | `int (int, int, int, int, int)` | 1216 | 1808 | 11552 |
| 0x4E7310 | `mine_yield` | `int (int, int, int, int, int)` | 1088 | 3024 | 10336 |
| 0x4E7750 | `energy_yield` | `int (int, int, int, int, int)` | 1648 | 4112 | 9248 |
| 0x4E7DC0 | `resource_yield` | `int (int, int, int, int, int)` | 752 | 5760 | 7600 |
| 0x4E80B0 | `base_yield` | `int ()` | 5280 | 6512 | 6848 |
| 0x4E9550 | `base_support` | `int ()` | 1568 | 11792 | 1568 |
| 0x4EB560 | `base_energy` | `int ()` | 3664 | 208 | 5680 |
| 0x4EF1C0 | `base_growth` | `int ()` | 2448 | 304 | 5408 |
| 0x4EFB50 | `do_upgrade` | `void (int, int, int)` | 512 | 2752 | 2960 |
| 0x4EFD50 | `upgrade_cost` | `int (int, int, int)` | 304 | 3264 | 2448 |
| 0x4EFE80 | `upgrade_prototype` | `int (int, int, int, int)` | 1504 | 3568 | 2144 |
| 0x4F0460 | `upgrade_prototypes` | `void (int, int)` | 496 | 5072 | 640 |
| 0x4F0650 | `upgrade_any_prototypes` | `void (int)` | 144 | 5568 | 144 |
| 0x4F07E0 | `base_production` | `int ()` | 14336 | 256 | 17888 |
| 0x4F3FE0 | `base_hurry` | `int ()` | 2128 | 14592 | 3552 |
| 0x4F4830 | `base_check_support` | `int ()` | 1424 | 16720 | 1424 |
| 0x4F4E80 | `base_research` | `int ()` | 304 | 192 | 5776 |
| 0x4F4FB0 | `drone_riot` | `int ()` | 3376 | 496 | 5472 |
| 0x4F5CE0 | `base_drones` | `int ()` | 592 | 3872 | 2096 |
| 0x4F5F30 | `base_doctors` | `int ()` | 1504 | 4464 | 1504 |
| 0x4F67F0 | `base_ecology` | `int ()` | 4560 | 512 | 6128 |
| 0x4F79C0 | `base_upkeep` | `int (int)` | 1568 | 5072 | 1568 |
| 0x4F8110 | `base_claimed` | `int (int, int, int, int, int, int)` | 144 | 64 | 32832 |
| 0x4F81A0 | `base_build` | `int (int, int*, int*, int*)` | 32640 | 208 | 32688 |
| 0x50B9E0 | `NetMsg_pop_2` | `int (const char* label, const char* filename)` | 32 | 64 | 32 |
| 0x543A20 | `trade_maps` | `void (int, int)` | 416 | 39008 | 30080 |
| 0x543BC0 | `propose_pact` | `int (int, int)` | 4736 | 39424 | 29664 |
| 0x544E40 | `propose_treaty` | `int (int, int)` | 3872 | 44160 | 24928 |
| 0x545D60 | `propose_trade_maps` | `void (int, int)` | 288 | 48032 | 21056 |
| 0x545E80 | `propose_attack` | `int (int, int)` | 7568 | 48320 | 20768 |
| 0x547C10 | `make_gift` | `int (int, int)` | 4544 | 55888 | 13200 |
| 0x548DD0 | `dont_withdrawal` | `int (int, int)` | 432 | 60432 | 8656 |
| 0x548F80 | `do_withdrawal` | `int (int, int)` | 752 | 60864 | 8224 |
| 0x549270 | `demand_withdrawal` | `int (int, int)` | 1728 | 61616 | 7472 |
| 0x549930 | `threaten` | `void (int, int)` | 5008 | 63344 | 5744 |
| 0x54ACC0 | `suggest_plan` | `int (int, int)` | 736 | 68352 | 736 |
| 0x54B140 | `battle_routine` | `void (int)` | 128 | 416 | 57440 |
| 0x54B1C0 | `battle_plans` | `void (int, int)` | 3328 | 544 | 57312 |
| 0x54BEC0 | `call_off_vendetta` | `void (int, int)` | 1696 | 3872 | 53984 |
| 0x54C560 | `diplomacy_menu` | `int (int, int)` | 1520 | 5568 | 52288 |
| 0x54CB50 | `value_of_base` | `int (int, int, int, int, int)` | 1008 | 7088 | 50768 |
| 0x54CF40 | `give_a_base` | `void (int, int)` | 1232 | 8096 | 49760 |
| 0x54D410 | `base_swap` | `int (int, int)` | 2272 | 9328 | 48528 |
| 0x54DCF0 | `proposal_menu` | `int (int, int)` | 5936 | 11600 | 46256 |
| 0x54F420 | `make_a_proposal` | `void (int, int, int)` | 2992 | 17536 | 40320 |
| 0x54FFD0 | `communicate` | `void (int, int, int)` | 35344 | 20528 | 37328 |
| 0x5ABFF0 | `get_rating` | `int (int, int)` | 112 | 208 | 112 |
| 0x5AC110 | `num_objectives` | `int (int, int)` | 1168 | 176 | 1312 |
| 0x5AC5A0 | `most_objectives` | `int (int*, int*)` | 144 | 1344 | 144 |
| 0x5AC690 | `rankings` | `void (int)` | 1360 | 16 | 9920 |
| 0x5ACBE0 | `compute_score` | `void (int, int*, int*, int)` | 4112 | 1376 | 8560 |
| 0x5BEB70 | `tech_pick` | `int (int, int, int, const char*)` | 304 | 1216 | 1184 |
| 0x5BECA0 | `X_text_open` | `int (const char*, const char*)` | 128 | 1520 | 880 |
| 0x5BED20 | `do_unity_crash` | `int (int, int, int)` | 752 | 1648 | 752 |

### GENERAL (195 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x422C50 | `BattleWin_clear` | `int (BattleWindow* This)` | 112 | 5600 | 688 |
| 0x422CC0 | `BattleWin_on_status` | `void (BattleWindow* This, int a2)` | 256 | 5712 | 576 |
| 0x422DC0 | `BattleWin_on_iface_button_clicked` | `void (BattleWindow* This, int a2)` | 32 | 5968 | 320 |
| 0x422DE0 | `BattleWin_on_iface_group_clicked` | `void (BattleWindow* This, int a2, int a3, int a4)` | 48 | 6000 | 288 |
| 0x422E10 | `BattleWin_on_iface_left_click` | `void (BattleWindow* This, int a2, int a3)` | 16 | 6048 | 240 |
| 0x422E20 | `BattleWin_on_iface_right_click` | `void (BattleWindow* This, int a2, int a3)` | 16 | 6064 | 224 |
| 0x422E30 | `BattleWin_on_iface_left_down` | `void (BattleWindow* This, int x, int y)` | 64 | 6080 | 208 |
| 0x422E70 | `BattleWin_on_iface_right_down` | `void (BattleWindow* This, int a2, int a3)` | 16 | 6144 | 144 |
| 0x422E80 | `BattleWin_on_iface_mouse_move` | `void (BattleWindow* This, int a2, int a3)` | 16 | 6160 | 128 |
| 0x422E90 | `BattleWin_on_iface_mouse_leave` | `void (BattleWindow* This, int a2, int a3)` | 16 | 6176 | 112 |
| 0x422EA0 | `BattleWin_on_iface_selected` | `void (BattleWindow* This, int a2, int a3)` | 16 | 6192 | 96 |
| 0x422EB0 | `BattleWin_timer_callback_daemon` | `void (int a1, int a2)` | 16 | 6208 | 80 |
| 0x422EC0 | `BattleWin_timer_callback_daemon_2` | `void (int a1)` | 16 | 6224 | 64 |
| 0x422ED0 | `BattleWin_dtor` | `int (BattleWindow* This)` | 16 | 6240 | 48 |
| 0x422EE0 | `BattleWin_ctor` | `int (BattleWindow* This)` | 32 | 6256 | 32 |
| 0x422F20 | `compare_palettes` | `int ()` | 224 | 32 | 320384 |
| 0x423000 | `colortables_init_faction` | `void (int)` | 1392 | 256 | 320160 |
| 0x423570 | `colortables_init` | `int ()` | 3680 | 1648 | 318768 |
| 0x4243D0 | `clear_council_notify` | `void (int)` | 32 | 5328 | 315088 |
| 0x4243F0 | `clear_council_notify_2` | `int ()` | 16224 | 5360 | 315056 |
| 0x428350 | `council` | `void (int, int, int)` | 2720 | 21584 | 298832 |
| 0x428DF0 | `show_credits` | `int ()` | 38848 | 24304 | 296112 |
| 0x4325B0 | `ButtonGroup_dtor` | `int (ButtonGroup* This)` | 7888 | 63152 | 257264 |
| 0x434480 | `DesignWin_ctor` | `int (DesignWindow* This)` | 320 | 71040 | 249376 |
| 0x4345C0 | `DesignWin_UNK1` | `int (DesignWindow* This)` | 16 | 71360 | 249056 |
| 0x4345D0 | `DesignWin_UNK2` | `int (DesignWindow* This, int unit_id)` | 128 | 71376 | 249040 |
| 0x434650 | `DesignWin_UNK3` | `int (DesignWindow* This)` | 80 | 71504 | 248912 |
| 0x4346A0 | `DesignWin_draw_info` | `int (DesignWindow* This)` | 1312 | 71584 | 248832 |
| 0x434BC0 | `DesignWin_draw_chassis` | `void (DesignWindow* This, int a2)` | 896 | 72896 | 247520 |
| 0x434F40 | `DesignWin_draw_weapon` | `void (DesignWindow* This, int a2)` | 1216 | 73792 | 246624 |
| 0x435400 | `DesignWin_draw_armor` | `void (DesignWindow* This, int a2)` | 1024 | 75008 | 245408 |
| 0x435800 | `DesignWin_draw_reactor` | `void (DesignWindow* This, int a2)` | 816 | 76032 | 244384 |
| 0x435B30 | `DesignWin_draw_special_1` | `void (DesignWindow* This, int a2)` | 688 | 76848 | 243568 |
| 0x435DE0 | `DesignWin_draw_special_2` | `void (DesignWindow* This, int a2)` | 688 | 77536 | 242880 |
| 0x436090 | `DesignWin_draw_vehicle` | `void (DesignWindow* This, int a2)` | 6416 | 78224 | 242192 |
| 0x4379A0 | `DesignWin_on_redraw` | `int (DesignWindow* This)` | 512 | 84640 | 235776 |
| 0x437BA0 | `DesignWin_on_mouse_move` | `void (DesignWindow* This, int x, int y)` | 64 | 85152 | 235264 |
| 0x437BE0 | `DesignWin_on_mouse_leave` | `void (DesignWindow* This, int a2, int a3)` | 16 | 85216 | 235200 |
| 0x437BF0 | `DesignWin_design_it` | `int (DesignWindow* This)` | 2160 | 85232 | 235184 |
| 0x438460 | `DesignWin_on_button_clicked` | `void (DesignWindow* This, int a2)` | 4624 | 87392 | 233024 |
| 0x439670 | `DesignWin_bubble_me_elmo2` | `void (DesignWindow* This, const char* a2, int a3, int a4)` | 64 | 92016 | 228400 |
| 0x4396B0 | `DesignWin_bubble_me_elmo2_2` | `void (DesignWindow* This, const char* a2, int a3, int a4)` | 80 | 92080 | 228336 |
| 0x439700 | `DesignWin_on_right_click` | `void (Win* This, int x, int y)` | 608 | 92160 | 228256 |
| 0x439960 | `DesignWin_on_left_click` | `void (DesignWindow* This, int x, int y)` | 192 | 92768 | 227648 |
| 0x439A20 | `DesignWin_on_key_click` | `int (DesignWindow* This, int a2, int a3)` | 480 | 92960 | 227456 |
| 0x439C00 | `DesignWin_check_abil` | `int (DesignWindow* This)` | 304 | 93440 | 226976 |
| 0x439D30 | `DesignWin_suggest_abil` | `void (DesignWindow* This, int ability_id)` | 384 | 93744 | 226672 |
| 0x439EB0 | `DesignWin_delete_abil` | `void (DesignWindow* This, int ability_id)` | 112 | 94128 | 226288 |
| 0x439F20 | `DesignWin_select_chassis` | `int (DesignWindow* This)` | 992 | 94240 | 226176 |
| 0x43A300 | `DesignWin_select_weapon` | `int (DesignWindow* This)` | 1120 | 95232 | 225184 |
| 0x43A760 | `DesignWin_select_armor` | `int (DesignWindow* This)` | 672 | 96352 | 224064 |
| 0x43AA00 | `DesignWin_select_reactor` | `int (DesignWindow* This)` | 624 | 97024 | 223392 |
| 0x43AC70 | `DesignWin_select_special` | `void (DesignWindow* This, int a2)` | 4512 | 97648 | 222768 |
| 0x43BE10 | `DesignWin_select_special_1` | `int (DesignWindow* This)` | 16 | 102160 | 218256 |
| 0x43BE20 | `DesignWin_select_special_2` | `int (DesignWindow* This)` | 16 | 102176 | 218240 |
| 0x43BE30 | `DesignWin_select_name` | `int (DesignWindow* This)` | 288 | 102192 | 218224 |
| 0x43BF50 | `DesignWin_construct_vehicle` | `int (DesignWindow* This)` | 288 | 102480 | 217936 |
| 0x43C070 | `DesignWin_construct_chassis` | `int (DesignWindow* This)` | 48 | 102768 | 217648 |
| 0x43C0A0 | `DesignWin_construct_weapon` | `int (DesignWindow* This)` | 80 | 102816 | 217600 |
| 0x43C0F0 | `DesignWin_construct_armor` | `void (DesignWindow* This, int a2)` | 64 | 102896 | 217520 |
| 0x43C130 | `DesignWin_construct_reactor` | `int (DesignWindow* This)` | 64 | 102960 | 217456 |
| 0x43C170 | `DesignWin_timer_function` | `void (int a1)` | 48 | 103024 | 217392 |
| 0x43C1A0 | `DesignWin_close` | `int (DesignWindow* This)` | 208 | 103072 | 217344 |
| 0x43C270 | `DesignWin_init` | `int (DesignWindow* This)` | 3664 | 103280 | 217136 |
| 0x43D0C0 | `DesignWin_show` | `void (DesignWindow* This, int a2)` | 48 | 106944 | 213472 |
| 0x43D0F0 | `DesignWin_hide` | `int (DesignWindow* This)` | 48 | 106992 | 213424 |
| 0x43D120 | `DesignWin_on_status` | `void (DesignWindow* This, int a2)` | 256 | 107040 | 213376 |
| 0x43D220 | `DesignWin_on_iface_left_click` | `void (DesignWindow* This, int x, int y)` | 112 | 107296 | 213120 |
| 0x43D290 | `DesignWin_on_iface_right_click` | `void (DesignWindow* This, int x, int y)` | 160 | 107408 | 213008 |
| 0x43D330 | `DesignWin_on_iface_left_down` | `void (DesignWindow* This, int a2, int a3)` | 16 | 107568 | 212848 |
| 0x43D340 | `DesignWin_on_iface_right_down` | `void (DesignWindow* This, int a2, int a3)` | 16 | 107584 | 212832 |
| 0x43D350 | `DesignWin_on_iface_mouse_move` | `void (DesignWindow* This, int x, int y)` | 112 | 107600 | 212816 |
| 0x43D3C0 | `DesignWin_on_iface_mouse_leave` | `void (DesignWindow* This, int a2, int a3)` | 64 | 107712 | 212704 |
| 0x43D400 | `DesignWin_on_iface_selected` | `void (DesignWindow* This, int a2, int a3)` | 16 | 107776 | 212640 |
| 0x43D410 | `DesignWin_on_iface_left_double_click` | `void (DesignWindow* This, int a2, int a3)` | 16 | 107792 | 212624 |
| 0x43D420 | `DesignWin_on_iface_right_double_click` | `void (DesignWindow* This, int a2, int a3)` | 16 | 107808 | 212608 |
| 0x43D430 | `DesignWin_on_iface_button_clicked` | `void (DesignWindow* This, int a2)` | 32 | 107824 | 212592 |
| 0x43D450 | `DesignWin_on_iface_button_pressed` | `void (DesignWindow* This, int a2)` | 32 | 107856 | 212560 |
| 0x43D470 | `DesignWin_on_iface_button_toggled` | `void (DesignWindow* This, int a2, int a3)` | 16 | 107888 | 212528 |
| 0x43D480 | `DesignWin_on_iface_group_clicked` | `void (DesignWindow* This, int a2, int a3, int a4)` | 16 | 107904 | 212512 |
| 0x43D490 | `DesignWin_center_stack` | `void (DesignWindow* This, int a2, int a3)` | 128 | 107920 | 212496 |
| 0x43D510 | `DesignWin_draw_stack` | `int (DesignWindow* This)` | 2112 | 108048 | 212368 |
| 0x43DD50 | `DesignWin_draw_flash` | `int (DesignWindow* This)` | 1856 | 110160 | 210256 |
| 0x43E490 | `DesignWin_setup_veh` | `void (DesignWindow* This, int unit_id, int a3)` | 1232 | 112016 | 208400 |
| 0x43E960 | `DesignWin_shutdown` | `int (DesignWindow* This)` | 64 | 113248 | 207168 |
| 0x43E9A0 | `DesignWin_exec` | `int (DesignWindow* This, int faction_id, int unit_id)` | 784 | 113312 | 207104 |
| 0x43ECB0 | `DesignWin_dtor` | `int (DesignWindow* This)` | 256 | 114096 | 206320 |
| 0x43EDB0 | `DesignWin_scalar_dtor` | `int (DesignWindow* This, uint32_t flags)` | 448 | 114352 | 206064 |
| 0x43EF70 | `Popup_start_2` | `void (Popup* This, char* filename, const char* label, int a4, char* a5, int a6)` | 48 | 114800 | 205616 |
| 0x43EFA0 | `DesignWin_operator_delete` | `void (DesignWindow* This, uint32_t a2)` | 8160 | 114848 | 205568 |
| 0x440F80 | `clear_hotseat_chat` | `int ()` | 720 | 123008 | 197408 |
| 0x441250 | `clear_all_offers` | `int ()` | 15168 | 123728 | 196688 |
| 0x444D90 | `diplo` | `void (int, int)` | 928 | 138896 | 181520 |
| 0x445130 | `energy_compute` | `void (int, int)` | 784 | 139824 | 180592 |
| 0x445440 | `energy_limit` | `int (int)` | 432 | 140608 | 179808 |
| 0x4455F0 | `wave_it` | `void (int)` | 96 | 141040 | 179376 |
| 0x445650 | `ok_callback` | `int ()` | 80 | 141136 | 179280 |
| 0x4456A0 | `passover_callback` | `int ()` | 16 | 141216 | 179200 |
| 0x4456B0 | `popup_wave_callback` | `void (void*, int)` | 272 | 141232 | 179184 |
| 0x4457C0 | `input_callback` | `int ()` | 32 | 141504 | 178912 |
| 0x4457E0 | `load_music` | `void (int)` | 1264 | 141536 | 178880 |
| 0x445CD0 | `FX_init` | `int (Wave* This)` | 3216 | 142800 | 177616 |
| 0x446960 | `FX_play2` | `int (Wave* This, int a2, int a3, int a4)` | 160 | 146016 | 174400 |
| 0x446A00 | `FX_play` | `void (Wave* This, int a2)` | 144 | 146176 | 174240 |
| 0x446A90 | `FX_stop` | `void (Wave* This, int a2)` | 64 | 146320 | 174096 |
| 0x446AD0 | `FX_fade` | `void (Wave* This, int a2)` | 64 | 146384 | 174032 |
| 0x446B10 | `FX_engine_pitch` | `void (Wave* This, int a2)` | 48 | 146448 | 173968 |
| 0x446B40 | `ambience` | `int (int)` | 464 | 146496 | 173920 |
| 0x446D10 | `ambience_veh` | `void (int)` | 16704 | 146960 | 173456 |
| 0x50BA50 | `intervention` | `void (int, int)` | 624 | 32 | 2656 |
| 0x50BCC0 | `double_cross` | `void (int, int, int)` | 1568 | 656 | 2032 |
| 0x5389F0 | `NetDaemon_ctor` | `int (AlphaNet* This)` | 368 | 24192 | 1344 |
| 0x538B60 | `AlphaNet_dtor` | `int (AlphaNet* This)` | 432 | 24560 | 976 |
| 0x538D10 | `NetDaemon_dtor` | `int (AlphaNet* This)` | 544 | 24992 | 544 |
| 0x538FE0 | `checksum_file` | `uint8_t (char*)` | 176 | 48 | 176 |
| 0x579240 | `enemy_veh` | `int (int)` | 720 | 56768 | 1328 |
| 0x579510 | `enemy_turn` | `int (int)` | 608 | 57488 | 608 |
| 0x5AAAB0 | `load_game` | `int (int, int)` | 2896 | 5088 | 4704 |
| 0x5AB600 | `load_map` | `int ()` | 1424 | 7984 | 1808 |
| 0x5ABB90 | `save_map` | `int ()` | 384 | 9408 | 384 |
| 0x5B5620 | `social_select` | `void (int)` | 112 | 3824 | 112 |
| 0x5B57D0 | `spot_base` | `void (int, int)` | 272 | 320 | 1744 |
| 0x5B58E0 | `spot_stack` | `void (int, int)` | 400 | 592 | 1472 |
| 0x5B5A70 | `unspot_stack` | `void (int)` | 96 | 992 | 1072 |
| 0x5FF470 | `Palette_get_nearest_palette_index` | `int (Palette* This, uint8_t a2, uint8_t a3, uint8_t a4, int a5)` | 448 | 4608 | 3888 |
| 0x5FF630 | `Palette_get_nearest_palette_index_2` | `int (Palette* This, HSV* a2, HSV* a3, int a4)` | 768 | 5056 | 3440 |
| 0x5FF930 | `Palette_UNK8` | `int (Palette* This, void* a2, UINT iStartIndex, UINT cEntries, int a5, int a6)` | 480 | 5824 | 2672 |
| 0x5FFB10 | `Palette_UNK9` | `int (Palette* This, UINT a2, UINT iStartIndex, UINT cEntries, int a5, int a6)` | 2192 | 6304 | 2192 |
| 0x600400 | `filefind_init` | `int (const char*, int)` | 464 | 80 | 464 |
| 0x600860 | `BasePop_ctor` | `int (BasePop* This)` | 1696 | 32 | 27776 |
| 0x600F00 | `BasePop_close` | `int (BasePop* This)` | 1712 | 1728 | 26080 |
| 0x6015B0 | `BasePop_init` | `int (BasePop* This, int a2, size_t len)` | 496 | 3440 | 24368 |
| 0x6017A0 | `BasePop_set_ok_text` | `int (BasePop* This, char* a2)` | 128 | 3936 | 23872 |
| 0x601820 | `BasePop_set_cancel_text` | `int (BasePop* This, char* a2)` | 128 | 4064 | 23744 |
| 0x6018A0 | `BasePop_set_def_ok_text` | `int (char* a1)` | 112 | 4192 | 23616 |
| 0x601910 | `BasePop_set_def_cancel_text` | `int (char* a1)` | 112 | 4304 | 23504 |
| 0x601980 | `BasePop_string` | `int (BasePop* This, const char* a2)` | 112 | 4416 | 23392 |
| 0x6019F0 | `BasePop_button` | `int (BasePop* This, char* a2)` | 80 | 4528 | 23280 |
| 0x601A40 | `BasePop_UNK1` | `int (BasePop* This, char* a2)` | 128 | 4608 | 23200 |
| 0x601AC0 | `BasePop_set_height` | `void (BasePop* This, int a2)` | 96 | 4736 | 23072 |
| 0x601B20 | `BasePop_set_width` | `void (BasePop* This, int a2)` | 96 | 4832 | 22976 |
| 0x601B80 | `BasePop_set_loc` | `void (BasePop* This, int a2, int a3)` | 48 | 4928 | 22880 |
| 0x601BB0 | `BasePop_write_check` | `void (BasePop* This, int a2)` | 32 | 4976 | 22832 |
| 0x601BD0 | `BasePop_read_check` | `int (BasePop* This)` | 32 | 5008 | 22800 |
| 0x601BF0 | `BasePop_start` | `int (BasePop* This, const char* filename, const char* label, int a4, char* a5, int a6, GraphicWin* a7)` | 2576 | 5040 | 22768 |
| 0x602600 | `BasePop_exec_3` | `int (BasePop* This, int a2, int a3)` | 4608 | 7616 | 20192 |
| 0x603800 | `BasePop_calculate_dimensions` | `int (BasePop* This)` | 2816 | 12224 | 15584 |
| 0x628B70 | `HSV_to_RGB` | `void (HSV*, PALETTEENTRY*)` | 576 | 64 | 1392 |
| 0x628DB0 | `RGB_to_HSV` | `void (PALETTEENTRY*, HSV*)` | 816 | 640 | 816 |
| 0x629780 | `StringBox_clear` | `int (GraphicWin* This)` | 752 | 1696 | 15856 |
| 0x629A70 | `StringBox_clip_ids` | `int (GraphicWin* This, int len)` | 7504 | 2448 | 15104 |
| 0x62B7C0 | `ButtonGroup_ctor` | `int (ButtonGroup* This)` | 48 | 9952 | 7600 |
| 0x62B7F0 | `ButtonGroup_close` | `int (ButtonGroup* This)` | 32 | 10000 | 7552 |
| 0x62B810 | `ButtonGroup_init` | `void (ButtonGroup* This, int a2, int a3)` | 64 | 10032 | 7520 |
| 0x62B850 | `ButtonGroup_add` | `void (ButtonGroup* This, BaseButton* button)` | 32 | 10096 | 7456 |
| 0x62B870 | `ButtonGroup_set` | `int (ButtonGroup* This, int a2, int a3)` | 48 | 10128 | 7424 |
| 0x62B8A0 | `ButtonGroup_button_click` | `int (ButtonGroup* This, int a2)` | 6912 | 10176 | 7376 |
| 0x62D3A0 | `jackal_init_real` | `int (Palette*, Font*, char*, int, int, int, int)` | 352 | 17088 | 464 |
| 0x62D500 | `jackal_close` | `int ()` | 112 | 17440 | 112 |
| 0x62D5B0 | `do_net` | `int ()` | 32 | 64 | 30208 |
| 0x62D5D0 | `check_net` | `int ()` | 208 | 96 | 30176 |
| 0x62D6A0 | `Net_ctor` | `int (Net* This)` | 576 | 304 | 29968 |
| 0x62D8E0 | `Net_init` | `int (Net* This, GUID* a2, int a3, int a4)` | 256 | 880 | 29392 |
| 0x62D9E0 | `Net_check_for_lobby` | `int (Net* This, char* a2, GUID* a3, int a4, int a5)` | 1344 | 1136 | 29136 |
| 0x62DF20 | `Net_start_voice` | `int (Net* This, uint32_t a2)` | 80 | 2480 | 27792 |
| 0x62DF70 | `Net_start_voice_2` | `int (Net* This, int a2)` | 80 | 2560 | 27712 |
| 0x62DFC0 | `Net_stop_voice` | `int (Net* This)` | 80 | 2640 | 27632 |
| 0x62E010 | `Net_close` | `int (Net* This)` | 576 | 2720 | 27552 |
| 0x62E250 | `Net_poll_services` | `int (Net* This, ServiceList* a2)` | 80 | 3296 | 26976 |
| 0x62E2A0 | `Net_join_service` | `int (Net* This, ServiceStruct* a2)` | 384 | 3376 | 26896 |
| 0x62E420 | `Net_on_player_added` | `int (Net* This, char* a2, uint32_t a3)` | 288 | 3760 | 26512 |
| 0x62E540 | `Net_join_session` | `int (Net* This, SessionStruct* a2, char* a3, char* a4)` | 1376 | 4048 | 26224 |
| 0x62EAA0 | `Net_create_session` | `int (Net* This, char* a2, int a3, char* a4)` | 656 | 5424 | 24848 |
| 0x62ED30 | `Net_poll_players` | `int (Net* This, SessionStruct* a2)` | 16 | 6080 | 24192 |
| 0x62ED40 | `Net_create_group` | `int (Net* This, char* a2)` | 112 | 6096 | 24176 |
| 0x62EDB0 | `Net_delete_group` | `void (Net* This, int a2)` | 32 | 6208 | 24064 |
| 0x62EDD0 | `Net_add_to_group` | `int (Net* This, int a2, uint32_t a3)` | 208 | 6240 | 24032 |
| 0x62EEA0 | `Net_send_group` | `int (Net* This, void* a2, int a3, int a4, int a5)` | 2560 | 6448 | 23824 |
| 0x62F8A0 | `Net_send_packet_type` | `int (Net* This, void* a2, int a3, uint32_t a4, int a5, int a6)` | 2016 | 9008 | 21264 |
| 0x630080 | `Net_send` | `int (Net* This, void* a2, int a3, uint32_t a4, int a5)` | 2432 | 11024 | 19248 |
| 0x630A00 | `Net_get` | `int (Net* This, uint32_t* a2, uint32_t* a3)` | 640 | 13456 | 16816 |
| 0x630C80 | `Net_enum_players` | `int (Net* This, uint32_t a2, uint32_t a3, const void* a4, uint32_t a5, void* a6)` | 3488 | 14096 | 16176 |
| 0x631A20 | `Net_get_player_name` | `char* (Net* This, uint32_t a2)` | 64 | 17584 | 12688 |
| 0x631A60 | `Net_process_voice` | `int (Net* This)` | 208 | 17648 | 12624 |
| 0x631B30 | `Net_close_game` | `int (Net* This)` | 1344 | 17856 | 12416 |
| 0x632070 | `Net_enable_join` | `int (Net* This)` | 112 | 19200 | 11072 |
| 0x6320E0 | `Net_check_polling` | `int (Net* This)` | 544 | 19312 | 10960 |
| 0x632300 | `Net_set_polling` | `void (Net* This, int a2)` | 80 | 19856 | 10416 |
| 0x632350 | `Net_drop_player` | `int (Net* This, uint32_t a2, int a3)` | 1168 | 19936 | 10336 |
| 0x6327E0 | `Net_internal_receive` | `int (NetThreadInfo* a1)` | 1488 | 21104 | 9168 |
| 0x632DB0 | `Net_handle_sys_msg` | `int (Net* This)` | 496 | 22592 | 7680 |
| 0x632FA0 | `Net_check_message` | `int (Net* This, uint32_t a2, uint32_t a3)` | 7184 | 23088 | 7184 |
| 0x636300 | `do_video` | `int ()` | 60402 | 5968 | None |
| 0x644EF2 | `mem_free` | `void (void*)` | 4395 | 66370 | None |
| 0x64601D | `game_rand` | `int ()` | None | 70765 | None |

### MAP (359 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x44AE50 | `hall_of_fame` | `void (int)` | 432 | 163664 | 156752 |
| 0x44B000 | `hof_save` | `int ()` | 112 | 164096 | 156320 |
| 0x44B070 | `hof_load` | `int ()` | 784 | 164208 | 156208 |
| 0x44B380 | `tech_heck` | `void (void (__cdecl*)(char*), int)` | 1632 | 164992 | 155424 |
| 0x44B9E0 | `help_tech_info` | `void (void (__cdecl*)(char*), int, int, int)` | 3568 | 166624 | 153792 |
| 0x44C7D0 | `help_info_pop` | `void (char*)` | 32 | 170192 | 150224 |
| 0x44C7F0 | `help_tech_info_2` | `void (Popup*, int, int)` | 144 | 170224 | 150192 |
| 0x44C880 | `help_tech` | `void (int)` | 144 | 170368 | 150048 |
| 0x44C910 | `help_weapon` | `void (int)` | 48 | 170512 | 149904 |
| 0x44C940 | `help_armor` | `void (int)` | 64 | 170560 | 149856 |
| 0x44C980 | `help_chassis` | `void (int)` | 48 | 170624 | 149792 |
| 0x44C9B0 | `help_facility` | `void (int)` | 48 | 170672 | 149744 |
| 0x44C9E0 | `help_project` | `void (int)` | 48 | 170720 | 149696 |
| 0x44CA10 | `help_abil` | `void (int)` | 48 | 170768 | 149648 |
| 0x44CA40 | `help_social` | `void (int)` | 48 | 170816 | 149600 |
| 0x44CA70 | `help_faction` | `void (int)` | 96 | 170864 | 149552 |
| 0x44CAD0 | `help_veh` | `void (int)` | 96 | 170960 | 149456 |
| 0x44CB30 | `help_any` | `int ()` | 48 | 171056 | 149360 |
| 0x44CB60 | `help_topic` | `void (int, int)` | 32 | 171104 | 149312 |
| 0x44CB80 | `help_create_link` | `void (char*, char*, char*)` | 17584 | 171136 | 149280 |
| 0x451030 | `load_voxels` | `int ()` | 2528 | 188720 | 131696 |
| 0x451A10 | `load_palette` | `int ()` | 576 | 191248 | 129168 |
| 0x451C50 | `load_terrain` | `int ()` | 6848 | 191824 | 128592 |
| 0x453710 | `load_faction_art` | `void (int)` | 784 | 198672 | 121744 |
| 0x453A20 | `load_bases` | `int ()` | 80 | 199456 | 120960 |
| 0x453A70 | `load_box_sprites` | `int ()` | 1280 | 199536 | 120880 |
| 0x453F70 | `load_flags` | `int ()` | 768 | 200816 | 119600 |
| 0x454270 | `load_vehicles` | `int ()` | 1392 | 201584 | 118832 |
| 0x4547E0 | `load_icons` | `int ()` | 1328 | 202976 | 117440 |
| 0x454D10 | `load_misc` | `int ()` | 688 | 204304 | 116112 |
| 0x454FC0 | `load_cursors` | `int ()` | 1088 | 204992 | 115424 |
| 0x455400 | `load_cycling` | `int ()` | 192 | 206080 | 114336 |
| 0x4554C0 | `def_caption_draw` | `int (GraphicWin*, char*, Buffer*)` | 336 | 206272 | 114144 |
| 0x455610 | `load_interface` | `void (int)` | 2112 | 206608 | 113808 |
| 0x455E50 | `load_deswin_sprites` | `int ()` | 48 | 208720 | 111696 |
| 0x455E80 | `load_fixed_sprites` | `int ()` | 48 | 208768 | 111648 |
| 0x455EB0 | `load_sprites` | `int ()` | 14096 | 208816 | 111600 |
| 0x4595C0 | `bung_momma` | `int ()` | 15648 | 222912 | 97504 |
| 0x45D2E0 | `SubInterface_delete_iface_mode` | `int (SubInterface* This)` | 48 | 238560 | 81856 |
| 0x45D310 | `SubInterface_set_iface_mode` | `int (SubInterface* This)` | 112 | 238608 | 81808 |
| 0x45D380 | `SubInterface_release_iface_mode` | `int (SubInterface* This)` | 192 | 238720 | 81696 |
| 0x45D440 | `slide_show` | `void (GraphicWin*, int)` | 1328 | 238912 | 81504 |
| 0x45D970 | `slide_hide` | `void (GraphicWin*, int)` | 1328 | 240240 | 80176 |
| 0x45DEA0 | `scroll_show` | `void (GraphicWin*, Scroll*)` | 1696 | 241568 | 78848 |
| 0x45E540 | `scroll_hide` | `void (GraphicWin*, Scroll*)` | 15440 | 243264 | 77152 |
| 0x462190 | `alt_get_ocean_detail` | `int (int, int, int, int)` | 1360 | 258704 | 61712 |
| 0x4626E0 | `MapWin_ctor` | `DWORD (Console* This, int a2)` | 400 | 260064 | 60352 |
| 0x462870 | `MapWin_clear` | `void (Console* This, int a2)` | 240 | 260464 | 59952 |
| 0x462960 | `MapWin_UNK1` | `int (Console* This)` | 16 | 260704 | 59712 |
| 0x462970 | `MapWin_UNK2` | `int (Console* This)` | 16 | 260720 | 59696 |
| 0x462980 | `MapWin_calculate_dimensions` | `int (Console* This)` | 1408 | 260736 | 59680 |
| 0x462F00 | `MapWin_tile_to_pixel` | `void (Console* This, int x, int y, long* px, long* py)` | 320 | 262144 | 58272 |
| 0x463040 | `MapWin_pixel_to_tile` | `int (Console* This, int x, int y, long* px, long* py)` | 656 | 262464 | 57952 |
| 0x4632D0 | `MapWin_gen_terrain_poly` | `int (Buffer* a1, Console* a2, int zoom, int faction_id, int a5, int flags, int x1, int y1, int x2, int y2)` | 14800 | 263120 | 57296 |
| 0x466CA0 | `MapWin_draw_vehicles` | `void (Console* This, int x, int y, int a4, int a5, int a6)` | 1264 | 277920 | 42496 |
| 0x467190 | `MapWin_draw_bases` | `void (Console* This, int x, int y, int a4, int a5, int a6)` | 304 | 279184 | 41232 |
| 0x4672C0 | `MapWin_direct_dest_line` | `void (Console* This, int x, int y, int a4, int a5, int a6, int a7, int a8, int a9)` | 368 | 279488 | 40928 |
| 0x467430 | `MapWin_dest_line` | `void (Console* This, int x1, int y1, int x2, int y2, int veh_id, int faction_id, int a8, int a9, int a10, int a11, int a12)` | 1344 | 279856 | 40560 |
| 0x467970 | `MapWin_draw_base_dest` | `void (Console* This, int a2)` | 1136 | 281200 | 39216 |
| 0x467DE0 | `MapWin_draw_base_names` | `int (Console* This)` | 2816 | 282336 | 38080 |
| 0x4688E0 | `MapWin_gen_overlays` | `void (Console* This, int x, int y)` | 832 | 285152 | 35264 |
| 0x468C20 | `MapWin_gen_tile` | `void (Console* This, int x, int y, int a4, int a5)` | 176 | 285984 | 34432 |
| 0x468CD0 | `MapWin_compute_clip` | `int (Console* This, int a2, int a3, int a4, RECT* lprcDst)` | 704 | 286160 | 34256 |
| 0x468F90 | `MapWin_draw_vehicles_2` | `void (Console* This, int a2, int a3, int a4, int a5)` | 512 | 286864 | 33552 |
| 0x469190 | `MapWin_gen_radius` | `void (Console* This, int x, int y, int a4, int faction_id, int a6)` | 1424 | 287376 | 33040 |
| 0x469720 | `MapWin_draw_legend` | `int (Console* This)` | 432 | 288800 | 31616 |
| 0x4698D0 | `MapWin_draw_wars` | `int (Console* This)` | 624 | 289232 | 31184 |
| 0x469B40 | `MapWin_gen_bases_only` | `int (Console* This)` | 352 | 289856 | 30560 |
| 0x469CA0 | `MapWin_gen_map` | `void (Console* This, int faction_id, int units_only)` | 1536 | 290208 | 30208 |
| 0x46A2A0 | `MapWin_draw_radius` | `void (Console* This, int x, int y, int a4, int a5)` | 512 | 291744 | 28672 |
| 0x46A4A0 | `MapWin_draw_tile` | `void (Console* This, int x, int y, int a4)` | 144 | 292256 | 28160 |
| 0x46A530 | `MapWin_draw_radius_2` | `void (Console* This, int x, int y, int a4)` | 32 | 292400 | 28016 |
| 0x46A550 | `MapWin_draw_map` | `void (Console* This, int a2)` | 2064 | 292432 | 27984 |
| 0x46AD60 | `MapWin_draw_cursor` | `int (Console* This)` | 160 | 294496 | 25920 |
| 0x46AE00 | `draw_cursor` | `int ()` | 240 | 294656 | 25760 |
| 0x46AEF0 | `draw_radius` | `void (int, int, int, int)` | 80 | 294896 | 25520 |
| 0x46AF40 | `draw_tile` | `void (int, int, int)` | 80 | 294976 | 25440 |
| 0x46AF90 | `draw_tile_fixup` | `void (int, int, int, int)` | 176 | 295056 | 25360 |
| 0x46B040 | `draw_tile_fixup_2` | `void (int, int, int, int, int, int)` | 256 | 295232 | 25184 |
| 0x46B140 | `draw_tiles` | `void (int, int, int)` | 80 | 295488 | 24928 |
| 0x46B190 | `draw_map` | `void (int)` | 64 | 295568 | 24848 |
| 0x46B1D0 | `MapWin_on_redraw` | `int (Console* This)` | 32 | 295632 | 24784 |
| 0x46B1F0 | `MapWin_set_center` | `void (Console* This, int x, int y, int a4)` | 288 | 295664 | 24752 |
| 0x46B310 | `MapWin_focus` | `int (Console* This, int x, int y)` | 512 | 295952 | 24464 |
| 0x46B510 | `MapWin_editor` | `void (Console* This, int x, int y, int a4)` | 2832 | 296464 | 23952 |
| 0x46C020 | `MapWin_UNK4` | `void (Console* This, int x, int y)` | 176 | 299296 | 21120 |
| 0x46C0D0 | `MapWin_right_menu` | `void (Console* This, int x, int y, int a4, int a5)` | 5376 | 299472 | 20944 |
| 0x46D5D0 | `MapWin_click` | `void (Console* This, int a2, int base_id_dst, int a4)` | 5568 | 304848 | 15568 |
| 0x46EB90 | `MapWin_on_lose_mouse_capture` | `int (Console* This)` | 16 | 310416 | 10000 |
| 0x46EBA0 | `MapWin_on_left_click` | `void (Console* This, int a2, int a3)` | 48 | 310432 | 9984 |
| 0x46EBD0 | `MapWin_on_left_double_click` | `void (Console* This, int a2, int a3)` | 16 | 310480 | 9936 |
| 0x46EBE0 | `MapWin_on_right_click` | `void (Console* This, int a2, int a3)` | 48 | 310496 | 9920 |
| 0x46EC10 | `MapWin_on_right_down` | `void (Console* This, int a2, int a3)` | 288 | 310544 | 9872 |
| 0x46ED30 | `MapWin_on_left_down` | `void (Console* This, int a2, int a3)` | 704 | 310832 | 9584 |
| 0x46EFF0 | `MapWin_on_left_up` | `void (Console* This, int a2, int a3)` | 16 | 311536 | 8880 |
| 0x46F000 | `MapWin_on_mouse_move` | `void (Console* This, int a2, int a3)` | 1632 | 311552 | 8864 |
| 0x46F660 | `MapWin_on_nc_hittest` | `void (Console* This, int a2, int a3)` | 160 | 313184 | 7232 |
| 0x46F700 | `MapWin_on_nc_left_down` | `void (Console* This, int a2, int a3)` | 384 | 313344 | 7072 |
| 0x46F880 | `MapWin_on_sys_close` | `int (Console* This)` | 64 | 313728 | 6688 |
| 0x46F8C0 | `MapWin_on_button_clicked` | `void (Console* This, int a2)` | 320 | 313792 | 6624 |
| 0x46FA00 | `MapWin_on_resize` | `void (Console* This, int a2, int a3)` | 32 | 314112 | 6304 |
| 0x46FA20 | `MapWin_UNK5` | `void (Console* This, int a2)` | 240 | 314144 | 6272 |
| 0x46FB10 | `MapWin_main_caption` | `int (Console* This)` | 16 | 314384 | 6032 |
| 0x46FB20 | `MapWin_standard_caption` | `int (Console* This)` | 96 | 314400 | 6016 |
| 0x46FB80 | `MapWin_do_image_buttons` | `int (Console* This)` | 16 | 314496 | 5920 |
| 0x46FB90 | `MapWin_clear_alt` | `void (Console* This, int a2, int a3, int a4)` | 160 | 314512 | 5904 |
| 0x46FC30 | `MapWin_clear_alt_2` | `int (Console* This)` | 80 | 314672 | 5744 |
| 0x46FC80 | `MapWin_UNK6` | `void (Console* This, int a2, int a3, int a4)` | 96 | 314752 | 5664 |
| 0x46FCE0 | `MapWin_UNK7` | `int (Console* This)` | 64 | 314848 | 5568 |
| 0x46FD20 | `MapWin_UNK8` | `void (Console* This, int a2, int a3, int a4)` | 112 | 314912 | 5504 |
| 0x46FD90 | `MapWin_clear_terrain` | `int (Console* This)` | 112 | 315024 | 5392 |
| 0x46FE00 | `MapWin_get_relative_alt` | `int (Console* This, int a2, int a3, int a4)` | 112 | 315136 | 5280 |
| 0x46FE70 | `MapWin_get_alt` | `int (Console* This, int a2, int a3, int a4)` | 1456 | 315248 | 5168 |
| 0x470420 | `MapWin_compute_lighting_table` | `int ()` | 304 | 316704 | 3712 |
| 0x470550 | `MapWin_get_brighting` | `int (Console* This, int a2, int a3, int a4)` | 576 | 317008 | 3408 |
| 0x470790 | `MapWin_get_point_light` | `int (Console* This, int a2, int a3, int a4)` | 400 | 317584 | 2832 |
| 0x470920 | `MapWin_init` | `void (Console* This, RECT* a2, char* a3, int a4)` | 368 | 317984 | 2432 |
| 0x470A90 | `MapWin_init_2` | `void (Console* This, int a2, int a3)` | 1088 | 318352 | 2064 |
| 0x470ED0 | `MapWin_init_dummy` | `int (Console* This)` | 160 | 319440 | 976 |
| 0x470F70 | `MapWin_close` | `int (Console* This)` | 192 | 319600 | 816 |
| 0x471030 | `mapwin_system_init` | `void (int)` | 176 | 319792 | 624 |
| 0x4710E0 | `mapwin_system_shutdown` | `int ()` | 192 | 319968 | 448 |
| 0x4711A0 | `terrain_fixup` | `void (Console*)` | 160 | 320160 | 256 |
| 0x471240 | `mapwin_terrain_fixup` | `int ()` | 96 | 320320 | 96 |
| 0x4712D0 | `mapwin_unk1` | `int (Console*, int, int, int, int, int, int, int, int)` | 208 | 48 | 109584 |
| 0x4713A0 | `MessageWin_UNK1` | `void (MessageWindow* This, int a2, int a3, int a4, int a5, int a6)` | 16 | 256 | 109376 |
| 0x4713B0 | `MessageWin_add_base` | `void (MessageWindow* This, int a2, char* lpString, Sprite* a4)` | 192 | 272 | 109360 |
| 0x471470 | `MessageWin_UNK2` | `int (MessageWindow* This, int a2, char* lpString, int a4)` | 96 | 464 | 109168 |
| 0x4714D0 | `MessageWin_UNK3` | `int (MessageWindow* This, int a2, int a3, char* lpString)` | 96 | 560 | 109072 |
| 0x471530 | `MessageWin_add_message` | `void (MessageWindow* This, char* lpString, int a3)` | 624 | 656 | 108976 |
| 0x4717A0 | `MessageWin_clear` | `int (MessageWindow* This)` | 208 | 1280 | 108352 |
| 0x471870 | `MessageWin_listbox_draw` | `int (GraphicWin* a1, int a2, int a3, int a4, int a5, int a6)` | 80 | 1488 | 108144 |
| 0x4718C0 | `MessageWin_on_selected` | `void (MessageWindow* This, int a2, int a3)` | 1152 | 1568 | 108064 |
| 0x471D40 | `MessageWin_UNK4` | `void (MessageWindow* This, int a2)` | 16 | 2720 | 106912 |
| 0x471D50 | `MessageWin_init` | `int (MessageWindow* This)` | 544 | 2736 | 106896 |
| 0x471F70 | `MessageWin_listbox_fill` | `int (GraphicWin* a1, int a2, int a3, RECT* a4)` | 96 | 3280 | 106352 |
| 0x471FD0 | `MessageWin_listbox_write` | `int (GraphicWin* a1, int a2, int a3, RECT* a4, char* a5)` | 80 | 3376 | 106256 |
| 0x472020 | `MessageWin_dtor` | `int (MessageWindow* This)` | 368 | 3456 | 106176 |
| 0x472190 | `MessageWin_ctor` | `int (MessageWindow* This)` | 18624 | 3824 | 105808 |
| 0x476A50 | `monument` | `void (int)` | 224 | 22448 | 87184 |
| 0x476B30 | `clear_monuments` | `int ()` | 64 | 22672 | 86960 |
| 0x476B70 | `mon_colony_founded` | `void (int, char*)` | 288 | 22736 | 86896 |
| 0x476C90 | `mon_tech_discovered` | `void (int, int)` | 272 | 23024 | 86608 |
| 0x476DA0 | `mon_secrets_of_tech` | `void (int)` | 320 | 23296 | 86336 |
| 0x476EE0 | `mon_prototype_built` | `void (int)` | 256 | 23616 | 86016 |
| 0x476FE0 | `mon_facility_built` | `void (int, char*)` | 288 | 23872 | 85760 |
| 0x477100 | `mon_secret_project` | `void (int, int)` | 272 | 24160 | 85472 |
| 0x477210 | `mon_enemy_destroyed` | `void (int, int)` | 272 | 24432 | 85200 |
| 0x477320 | `mon_conquer_base` | `void (int, char*)` | 288 | 24704 | 84928 |
| 0x477440 | `mon_naval_unit_built` | `void (int)` | 256 | 24992 | 84640 |
| 0x477540 | `mon_air_unit_built` | `void (int)` | 256 | 25248 | 84384 |
| 0x477640 | `mon_native_life_bred` | `void (int)` | 256 | 25504 | 84128 |
| 0x477740 | `mon_first_in_space` | `void (int)` | 256 | 25760 | 83872 |
| 0x477840 | `mon_built_preserve` | `void (int)` | 256 | 26016 | 83616 |
| 0x477940 | `mon_winning_unify` | `void (int, int)` | 144 | 26272 | 83360 |
| 0x4779D0 | `mon_winning_trans` | `void (int, int)` | 144 | 26416 | 83216 |
| 0x477A60 | `mon_killed_faction` | `void (int, int)` | 1040 | 26560 | 83072 |
| 0x477E70 | `MultiWin_on_mouse_move` | `void (MultiWindow* This, int x, int y)` | 112 | 27600 | 82032 |
| 0x477EE0 | `MultiWin_on_mouse_leave` | `void (MultiWindow* This, int a2, int a3)` | 32 | 27712 | 81920 |
| 0x477F00 | `MultiWin_on_button_clicked` | `void (MultiWindow* This, int a2)` | 48 | 27744 | 81888 |
| 0x477F30 | `MultiWin_on_left_click` | `void (MultiWindow* This, int x, int y)` | 544 | 27792 | 81840 |
| 0x478150 | `MultiWin_on_right_click` | `void (MultiWindow* This, int x, int y)` | 1248 | 28336 | 81296 |
| 0x478630 | `MultiWin_draw_player` | `void (MultiWindow* This, int a2, int a3, RECT* a4)` | 1456 | 29584 | 80048 |
| 0x478BE0 | `MultiWin_draw_time` | `void (MultiWindow* This, RECT* a2, int a3)` | 1680 | 31040 | 78592 |
| 0x479270 | `MultiWin_UNK1` | `void (MultiWindow* This, RECT* a2)` | 192 | 32720 | 76912 |
| 0x479330 | `MultiWin_draw` | `void (MultiWindow* This, int a2)` | 1472 | 32912 | 76720 |
| 0x4798F0 | `MultiWin_on_redraw` | `int (MultiWindow* This)` | 16 | 34384 | 75248 |
| 0x479900 | `MultiWin_UNK2` | `int (MultiWindow* This)` | 112 | 34400 | 75232 |
| 0x479970 | `MultiWin_show` | `int (MultiWindow* This)` | 656 | 34512 | 75120 |
| 0x479C00 | `MultiWin_hide` | `int (MultiWindow* This)` | 912 | 35168 | 74464 |
| 0x479F90 | `MultiWin_init` | `int (MultiWindow* This)` | 1184 | 36080 | 73552 |
| 0x47A430 | `MultiWin_dtor` | `int (MultiWindow* This)` | 352 | 37264 | 72368 |
| 0x47A590 | `MultiWin_ctor` | `int (MultiWindow* This)` | 304 | 37616 | 72016 |
| 0x47A6C0 | `MultiWin_scalar_dtor` | `void* (MultiWindow* This, uint32_t flags)` | 48 | 37920 | 71712 |
| 0x47A6F0 | `MultiWin_UNK3` | `int (MultiWindow* This, int a2, int type, RECT* a4)` | 48 | 37968 | 71664 |
| 0x47A720 | `MultiWin_UNK4` | `int (MultiWindow* This, LPSTR lpString, RECT* a3)` | 64 | 38016 | 71616 |
| 0x47A760 | `MultiWin_operator_delete` | `void (void* a1, uint32_t a2)` | 112 | 38080 | 71552 |
| 0x47A7D0 | `NetMsg_close` | `int (NetMessage* This)` | 48 | 38192 | 71440 |
| 0x47A800 | `NetMsg_timer_daemon` | `void (int a1)` | 96 | 38240 | 71392 |
| 0x47A860 | `NetMsg_on_button_clicked` | `void (NetMessage* This, int a2)` | 48 | 38336 | 71296 |
| 0x47A890 | `NetMsg_pop` | `int (NetMessage* This, const char* label, int delay, int a4, const char* filename)` | 1120 | 38384 | 71248 |
| 0x47ACF0 | `NetMsg_ctor` | `DWORD (NetMessage* This, int a2, int a3, int a4)` | 128 | 39504 | 70128 |
| 0x47AD70 | `NetMsg_scalar_dtor` | `int (NetMessage* This, uint32_t a2)` | 48 | 39632 | 70000 |
| 0x47ADA0 | `NetMsg_dtor` | `int (NetMessage* This)` | 576 | 39680 | 69952 |
| 0x47AFE0 | `NetMsg_operator_delete` | `void (void* a1, uint32_t a2)` | 64 | 40256 | 69376 |
| 0x47B020 | `send_faction_data` | `void (int)` | 208 | 40320 | 69312 |
| 0x47B0F0 | `send_all_player_packets` | `void (int)` | 368 | 40528 | 69104 |
| 0x47B260 | `send_session_packet` | `void (int, int)` | 128 | 40896 | 68736 |
| 0x47B2E0 | `send_session2_packet` | `void (int, int)` | 36016 | 41024 | 68608 |
| 0x483F90 | `NewTechWin_exec` | `void (NetTechWindow* This, int a2, int a3)` | 18416 | 77040 | 32592 |
| 0x488780 | `list_was_dblclicked` | `void (int)` | 32 | 95456 | 14176 |
| 0x4887A0 | `PickWin_init` | `void (PickWindow* This, void(__cdecl *a2)(int), int a3, int a4)` | 3776 | 95488 | 14144 |
| 0x489660 | `PickWin_on_redraw` | `int (PickWindow* This)` | 1184 | 99264 | 10368 |
| 0x489B00 | `PickWin_close` | `int (PickWindow* This)` | 16 | 100448 | 9184 |
| 0x489B10 | `PickWin_on_button_passover` | `void (PickWindow* This, int a2, int a3)` | 16 | 100464 | 9168 |
| 0x489B20 | `PickWin_on_group_clicked` | `void (PickWindow* This, int a2, int a3, int a4)` | 128 | 100480 | 9152 |
| 0x489BA0 | `PickWin_on_button_clicked` | `void (PickWindow* This, int a2)` | 3104 | 100608 | 9024 |
| 0x48A7C0 | `PickWin_on_key_click` | `int (PickWindow* This, int a2, int a3)` | 80 | 103712 | 5920 |
| 0x48A810 | `PickWin_show_window` | `int (PickWindow* This)` | 48 | 103792 | 5840 |
| 0x48A840 | `PickWin_on_dialog_item_back_draw` | `int (PickWindow* This, GraphicWin* a2, int a3, int a4, RECT* a5)` | 160 | 103840 | 5792 |
| 0x48A8E0 | `PickWin_dtor` | `int (PickWindow* This)` | 816 | 104000 | 5632 |
| 0x48AC10 | `PickWin_ctor` | `int (PickWindow* This)` | 432 | 104816 | 4816 |
| 0x48ADC0 | `PickWin_scalar_dtor` | `void* (PickWindow* This, uint32_t flags)` | 48 | 105248 | 4384 |
| 0x48ADF0 | `PickWin_operator_delete` | `void (void* a1, uint32_t a2)` | 3632 | 105296 | 4336 |
| 0x48BC20 | `PlanWin_blink` | `int (PlanWindow* This)` | 704 | 108928 | 704 |
| 0x48C060 | `parse_it` | `void (const char*, const char*)` | 64 | 384 | 163696 |
| 0x48C0A0 | `popp` | `int (const char* filename, const char* label, int a3, const char* pcx_filename, fp_none fn)` | 640 | 448 | 163632 |
| 0x48C320 | `popt` | `int (const char*, const char*, int, const char*, int)` | 64 | 1088 | 162992 |
| 0x48C360 | `popv` | `int (const char*, const char*, int, int)` | 752 | 1152 | 162928 |
| 0x48C650 | `popb` | `int (const char* label, int flags, int sound_id, const char* pcx_filename, Sprite* a5)` | 2144 | 1904 | 162176 |
| 0x48CEB0 | `veh_sprite_draw` | `int (Sprite*, GraphicWin*, int, int, int)` | 160 | 4048 | 160032 |
| 0x48CF50 | `veh_sprite_click` | `int (int)` | 192 | 4208 | 159872 |
| 0x48D010 | `veh_sprite_double_click` | `int (int)` | 192 | 4400 | 159680 |
| 0x48D0D0 | `veh_pick` | `int (int, const char*, int)` | 38304 | 4592 | 159488 |
| 0x496670 | `quayle` | `void (int)` | 752 | 42896 | 121184 |
| 0x496960 | `ReportWin_init` | `int (ReportWindow* This)` | 592 | 43648 | 120432 |
| 0x496BB0 | `ReportWin_reset` | `int (ReportWindow* This)` | 336 | 44240 | 119840 |
| 0x496D00 | `ReportWin_on_redraw` | `int (ReportWindow* This)` | 272 | 44576 | 119504 |
| 0x496E10 | `ReportWin_on_left_click` | `void (ReportWindow* This, int x, int y)` | 3152 | 44848 | 119232 |
| 0x497A60 | `ReportWin_on_left_double_click` | `void (ReportWindow* This, int x, int y)` | 96 | 48000 | 116080 |
| 0x497AC0 | `ReportWin_right_menu` | `void (ReportWindow* This, int base_id, int a3, int a4)` | 1088 | 48096 | 115984 |
| 0x497F00 | `ReportWin_on_right_click` | `void (ReportWindow* This, int x, int y)` | 496 | 49184 | 114896 |
| 0x4980F0 | `ReportWin_on_mouse_move` | `void (ReportWindow* This, int a2, int a3)` | 16 | 49680 | 114400 |
| 0x498100 | `ReportWin_on_mouse_leave` | `void (ReportWindow* This, int a2, int a3)` | 16 | 49696 | 114384 |
| 0x498110 | `ReportWin_UNK1` | `void (ReportWindow* This, int a2, int a3, int a4, int a5)` | 112 | 49712 | 114368 |
| 0x498180 | `ReportWin_on_scrolled` | `void (ReportWindow* This, int a2, int a3)` | 128 | 49824 | 114256 |
| 0x498200 | `ReportWin_on_scrolling` | `void (ReportWindow* This, int a2, int a3)` | 128 | 49952 | 114128 |
| 0x498280 | `ReportWin_on_key_click` | `int (ReportWindow* This, int a2, int a3)` | 1104 | 50080 | 114000 |
| 0x4986D0 | `ReportWin_on_button_clicked` | `void (ReportWindow* This, int a2)` | 800 | 51184 | 112896 |
| 0x4989F0 | `ReportWin_on_group_clicked` | `void (ReportWindow* This, int a2, int a3, int a4)` | 320 | 51984 | 112096 |
| 0x498B30 | `ReportWin_draw_labs` | `int (ReportWindow* This)` | 9968 | 52304 | 111776 |
| 0x49B220 | `ReportWin_on_button_passover` | `void (ReportWindow* This, int a2, int a3)` | 560 | 62272 | 101808 |
| 0x49B450 | `ReportWin_labs_line` | `void (ReportWindow* This, int a2, int a3, int a4, int a5, int a6, int a7, int a8)` | 2176 | 62832 | 101248 |
| 0x49BCD0 | `ReportWin_draw_energy` | `int (ReportWindow* This)` | 6080 | 65008 | 99072 |
| 0x49D490 | `ReportWin_on_selected` | `void (ReportWindow* This, int a2, int a3)` | 176 | 71088 | 92992 |
| 0x49D540 | `ReportWin_draw_ops` | `int (ReportWindow* This)` | 5584 | 71264 | 92816 |
| 0x49EB10 | `ReportWin_draw_projects` | `int (ReportWindow* This)` | 2112 | 76848 | 87232 |
| 0x49F350 | `ReportWin_draw_sat` | `int (ReportWindow* This)` | 2800 | 78960 | 85120 |
| 0x49FE40 | `ReportWin_sat_anim` | `int (ReportWindow* This)` | 384 | 81760 | 82320 |
| 0x4EC3B0 | `base_compute` | `int (int)` | 64 | 3872 | 2016 |
| 0x4EC3F0 | `base_connect` | `int (int)` | 1952 | 3936 | 1952 |
| 0x4ECBF0 | `base_terraform` | `void (int)` | 9104 | 96 | 9104 |
| 0x500120 | `clear` | `int ()` | 16 | 32848 | 48 |
| 0x500130 | `says` | `void (const char*)` | 32 | 32864 | 32 |
| 0x50DD90 | `clock_wait` | `void (int)` | 48 | 6368 | 48 |
| 0x50E820 | `mandate_color` | `int (int)` | 112 | 608 | 2992 |
| 0x50E890 | `auto_play_callback` | `void (int)` | 432 | 720 | 2880 |
| 0x50EA40 | `blink_timer` | `void (int)` | 1008 | 1152 | 2448 |
| 0x52ED40 | `NetDaemon_send_files` | `int (AlphaNet* This)` | 2016 | 16160 | 15696 |
| 0x52F520 | `NetDaemon_send_rules` | `int (AlphaNet* This)` | 960 | 18176 | 13680 |
| 0x52F8E0 | `NetDaemon_game_checksum` | `void (AlphaNet* This, int* a2)` | 2624 | 19136 | 12720 |
| 0x530320 | `NetDaemon_receive` | `int (AlphaNet* This)` | 64 | 21760 | 10096 |
| 0x530360 | `NetDaemon_net_tasks` | `int (AlphaNet* This)` | 80 | 21824 | 10032 |
| 0x5303B0 | `NetDaemon_wait` | `int (AlphaNet* This, char* a2, int* a3, int a4, int a5)` | 240 | 21904 | 9952 |
| 0x5304A0 | `NetDaemon_open_channel` | `int (AlphaNet* This, int a2, int a3)` | 352 | 22144 | 9712 |
| 0x530600 | `NetDaemon_close_channel` | `int (AlphaNet* This)` | 48 | 22496 | 9360 |
| 0x530630 | `NetDaemon_lock` | `int (AlphaNet* This, int a2, int a3, int a4, int a5, int a6, int a7)` | 752 | 22544 | 9312 |
| 0x530920 | `NetDaemon_add_lock` | `int (AlphaNet* This, int a2, int sz, int a4)` | 384 | 23296 | 8560 |
| 0x530AA0 | `NetDaemon_add_global` | `int (AlphaNet* This)` | 608 | 23680 | 8176 |
| 0x530D00 | `NetDaemon_unlock` | `int (AlphaNet* This)` | 80 | 24288 | 7568 |
| 0x530D50 | `NetDaemon_lock_base` | `int (AlphaNet* This, int a2, int a3, int a4, int a5)` | 256 | 24368 | 7488 |
| 0x530E50 | `NetDaemon_unlock_base` | `void (AlphaNet* This, int a2)` | 464 | 24624 | 7232 |
| 0x531020 | `NetDaemon_lock_veh` | `int (AlphaNet* This, net_int_t* veh_id, int a3, int a4, int a5, int a6)` | 208 | 25088 | 6768 |
| 0x5310F0 | `NetDaemon_unlock_veh` | `int (AlphaNet* This)` | 96 | 25296 | 6560 |
| 0x531150 | `NetDaemon_automated_lock` | `int (AlphaNet* This, int* a2, int a3)` | 432 | 25392 | 6464 |
| 0x531300 | `NetDaemon_await_exec` | `void (AlphaNet* This, int a2)` | 384 | 25824 | 6032 |
| 0x531480 | `NetDaemon_await_synch` | `int (AlphaNet* This)` | 320 | 26208 | 5648 |
| 0x5315C0 | `NetDaemon_await_diplo` | `void (AlphaNet* This, int a2)` | 320 | 26528 | 5328 |
| 0x531700 | `NetDaemon_maybe_lock` | `int (AlphaNet* This, net_int_t* veh_id, int a3, int a4, int a5, int a6, int a7)` | 128 | 26848 | 5008 |
| 0x531780 | `NetDaemon_order_veh` | `int (AlphaNet* This, int veh_id, int a3, int a4)` | 2496 | 26976 | 4880 |
| 0x532140 | `NetDaemon_veh_drop` | `int (AlphaNet* This, int veh_id, int x, int y)` | 1072 | 29472 | 2384 |
| 0x532570 | `NetDaemon_action` | `int (AlphaNet* This, int veh_id, int a3)` | 848 | 30544 | 1312 |
| 0x5328C0 | `NetDaemon_end_of_event` | `int (AlphaNet* This, int a2, int a3)` | 128 | 31392 | 464 |
| 0x532940 | `NetDaemon_send_message` | `void (AlphaNet* This, char* a2, uint32_t a3, int a4)` | 336 | 31520 | 336 |
| 0x532C30 | `synch_display` | `void (int, int, int, int)` | 464 | 192 | 25344 |
| 0x532E00 | `NetDaemon_synch` | `void (AlphaNet* This, int16_t a2, int id, int a4, int a5, char* a6, int uDelay, int16_t a8)` | 5424 | 656 | 24880 |
| 0x534330 | `NetDaemon_fixup_message` | `int (AlphaNet* This, char* a2)` | 208 | 6080 | 19456 |
| 0x534400 | `NetDaemon_process_message` | `void (AlphaNet* This, void* data, uint32_t a3, int input)` | 17904 | 6288 | 19248 |
| 0x5665D0 | `alien_base` | `int (int, int, int)` | 576 | 22656 | 19792 |
| 0x566810 | `alien_move` | `int (int)` | 4272 | 23232 | 19216 |
| 0x5678C0 | `air_power` | `int (int, int, int, int*, int*)` | 12256 | 27504 | 14944 |
| 0x56A8A0 | `enemy_planet_buster` | `int (int)` | 2688 | 39760 | 2688 |
| 0x56B5B0 | `enemy_move` | `int (int)` | 56464 | 304 | 57792 |
| 0x5797D0 | `site_at` | `int (int, int, int)` | 112 | 64 | 112 |
| 0x58D6B0 | `map_menu` | `int (BOOL)` | 1152 | 17712 | 14304 |
| 0x58DB30 | `multiplayer_init` | `int (int)` | 2096 | 18864 | 13152 |
| 0x58E360 | `top_menu` | `int (int)` | 2800 | 20960 | 11056 |
| 0x58EE50 | `desktop_update` | `int ()` | 16 | 23760 | 8256 |
| 0x58EE60 | `desktop_init` | `int (int)` | 400 | 23776 | 8240 |
| 0x58EFF0 | `desktop_close` | `int ()` | 80 | 24176 | 7840 |
| 0x58F040 | `system_init` | `int ()` | 528 | 24256 | 7760 |
| 0x58F250 | `system_close` | `int ()` | 160 | 24784 | 7232 |
| 0x58F2F0 | `game_init` | `int (BOOL, BOOL)` | 320 | 24944 | 7072 |
| 0x58F430 | `game_close` | `void (int)` | 32 | 25264 | 6752 |
| 0x58F450 | `game_reload` | `int (int, BOOL)` | 448 | 25296 | 6720 |
| 0x58F610 | `say_special` | `void (char*, char*, int)` | 240 | 25744 | 6272 |
| 0x58F700 | `say_fac_special` | `void (char*, char*, int)` | 272 | 25984 | 6032 |
| 0x58F810 | `get_phrase` | `char* (int, int)` | 160 | 26256 | 5760 |
| 0x58F8B0 | `get_noun_phrase` | `char* (int, int)` | 288 | 26416 | 5600 |
| 0x58F9D0 | `get_pact` | `char* (int)` | 80 | 26704 | 5312 |
| 0x58FA20 | `get_pacts` | `char* (int)` | 80 | 26784 | 5232 |
| 0x58FA70 | `get_pacts_2` | `char* (int, int)` | 144 | 26864 | 5152 |
| 0x58FB00 | `get_pact_hood` | `char* (int, int)` | 192 | 27008 | 5008 |
| 0x58FBC0 | `get_his_her` | `char* (int, int)` | 144 | 27200 | 4816 |
| 0x58FC50 | `get_him_her` | `char* (int, int)` | 144 | 27344 | 4672 |
| 0x58FCE0 | `get_he_she` | `char* (int, int)` | 1008 | 27488 | 4528 |
| 0x5900D0 | `Lock_clear` | `int (Lock* This)` | 112 | 28496 | 3520 |
| 0x590140 | `Lock_reset_map` | `int (Lock* This)` | 48 | 28608 | 3408 |
| 0x590170 | `Lock_unlock` | `void (Lock* This, int a2)` | 96 | 28656 | 3360 |
| 0x5901D0 | `Lock_check_global` | `int (Lock* This)` | 112 | 28752 | 3264 |
| 0x590240 | `Lock_check_global_2` | `int (Lock* This, int a2)` | 128 | 28864 | 3152 |
| 0x5902C0 | `Lock_global_lock` | `int (Lock* This, int a2)` | 64 | 28992 | 3024 |
| 0x590300 | `Lock_lock` | `int (Lock* This, int faction_id, int flags, int x, int y, int a6, int a7, int a8)` | 368 | 29056 | 2960 |
| 0x590470 | `Lock_add_lock` | `int (Lock* This, int faction_id, int a3, int x, int y)` | 48 | 29424 | 2592 |
| 0x5904A0 | `Lock_any_locks` | `int (Lock* This)` | 112 | 29472 | 2544 |
| 0x590510 | `say_reactor` | `void (void*, int, int)` | 176 | 29584 | 2432 |
| 0x5905C0 | `say_weapon` | `void (void*, int, int)` | 304 | 29760 | 2256 |
| 0x5906F0 | `say_armor` | `void (void*, int, int)` | 272 | 30064 | 1952 |
| 0x590800 | `say_chassis` | `void (void*, int, int)` | 272 | 30336 | 1680 |
| 0x590910 | `say_facility` | `void (void*, int, int)` | 128 | 30608 | 1408 |
| 0x590990 | `say_project` | `void (void*, int, int)` | 128 | 30736 | 1280 |
| 0x590A10 | `say_society` | `void (void*, int, int)` | 144 | 30864 | 1152 |
| 0x590AA0 | `say_ability` | `void (void*, int, int)` | 144 | 31008 | 1008 |
| 0x590B30 | `say_techs` | `void (void*, int, int)` | 112 | 31152 | 864 |
| 0x590BA0 | `say_vehicles` | `void (void*, int, int)` | 752 | 31264 | 752 |
| 0x591290 | `alt_set` | `void (int, int, int)` | 1552 | 48 | 1552 |
| 0x592250 | `say_loc` | `void (char*, int, int, int, int, int)` | 432 | 272 | 432 |
| 0x5927D0 | `delete_landmark` | `void (int, int)` | 368 | 224 | 3392 |
| 0x592940 | `fixup_landmarks` | `int ()` | 320 | 592 | 3024 |
| 0x592A80 | `set_dirty` | `int ()` | 112 | 912 | 2704 |
| 0x592AF0 | `clear_dirty` | `int ()` | 800 | 1024 | 2592 |
| 0x593A00 | `supply_options` | `int (int, int)` | 3520 | 464 | 26656 |
| 0x5947C0 | `order_veh` | `int (int, int, int)` | 23024 | 3984 | 23136 |
| 0x5A5900 | `BasePop_exec_2` | `int (BasePop* This)` | 16 | 32 | 16 |
| 0x5A6270 | `proto_sort` | `void (int)` | 352 | 1328 | 816 |
| 0x5A63D0 | `proto_sort_2` | `void (int)` | 464 | 1680 | 464 |
| 0x5A66E0 | `encrypt_write` | `int (void*, uint32_t, uint32_t, FILE*)` | 208 | 176 | 12272 |
| 0x5A67B0 | `encrypt_read` | `int (void*, uint32_t, uint32_t, FILE*)` | 224 | 384 | 12064 |
| 0x5A6890 | `game_io` | `int (Console*, FILE*)` | 560 | 608 | 11840 |
| 0x5A6AC0 | `game_data` | `int (FILE*, int)` | 10176 | 1168 | 11280 |
| 0x5C3F70 | `world_shorelines` | `int ()` | 384 | 8704 | 6480 |
| 0x5C40F0 | `world_validate` | `int ()` | 128 | 9088 | 6096 |
| 0x5C4170 | `world_temperature` | `int ()` | 672 | 9216 | 5968 |
| 0x5C4410 | `world_UNK1` | `void (int, int)` | 96 | 9888 | 5296 |
| 0x5C4470 | `world_rainfall` | `int ()` | 2912 | 9984 | 5200 |
| 0x5C4FD0 | `world_site` | `int (int, int, int)` | 1520 | 12896 | 2288 |
| 0x5C55C0 | `world_analysis` | `int ()` | 768 | 14416 | 768 |
| 0x5C58E0 | `world_polar_caps` | `int ()` | 336 | 32 | 752 |
| 0x5C5A30 | `world_climate` | `int ()` | 176 | 368 | 416 |
| 0x5C5AE0 | `world_linearize_contours` | `int ()` | 240 | 544 | 240 |
| 0x5C5C70 | `world_crater` | `void (int, int)` | 640 | 160 | 11568 |
| 0x5C5EF0 | `world_monsoon` | `void (int, int)` | 784 | 800 | 10928 |
| 0x5C6200 | `world_sargasso` | `void (int, int)` | 672 | 1584 | 10144 |
| 0x5C64A0 | `world_ruin` | `void (int, int)` | 672 | 2256 | 9472 |
| 0x5C6740 | `world_dune` | `void (int, int)` | 672 | 2928 | 8800 |
| 0x5C69E0 | `world_diamond` | `void (int, int)` | 608 | 3600 | 8128 |
| 0x5C6C40 | `world_fresh` | `void (int, int)` | 368 | 4208 | 7520 |
| 0x5C6DB0 | `world_volcano` | `void (int, int, int)` | 624 | 4576 | 7152 |
| 0x5C7020 | `world_borehole` | `void (int, int)` | 1312 | 5200 | 6528 |

### VEH (254 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x49FFC0 | `sat_anim_daemon` | `void (int)` | 16 | 82144 | 81936 |
| 0x49FFD0 | `ReportWin_start_attack` | `void (ReportWindow* This, int a2, int a3, int a4, int a5)` | 304 | 82160 | 81920 |
| 0x4A0100 | `ReportWin_exp_anim` | `int (ReportWindow* This)` | 336 | 82464 | 81616 |
| 0x4A0250 | `sat_expl_daemon` | `void (int)` | 16 | 82800 | 81280 |
| 0x4A0260 | `sat_attack` | `void (int, int, int)` | 1456 | 82816 | 81264 |
| 0x4A0810 | `sat_edit` | `void (int, int, int)` | 208 | 84272 | 79808 |
| 0x4A08E0 | `ReportWin_draw_security` | `int (ReportWindow* This)` | 2144 | 84480 | 79600 |
| 0x4A1140 | `ReportWin_draw_score` | `int (ReportWindow* This)` | 4304 | 86624 | 77456 |
| 0x4A2210 | `ReportWin_draw_intel` | `int (ReportWindow* This)` | 4688 | 90928 | 73152 |
| 0x4A3460 | `ReportIf_init` | `int (ReportInfc* This)` | 1232 | 95616 | 68464 |
| 0x4A3930 | `ReportIf_done` | `int (ReportInfc* This)` | 32 | 96848 | 67232 |
| 0x4A3950 | `ReportIf_report` | `void (ReportInfc* This, int a2, int faction_id, int a4)` | 976 | 96880 | 67200 |
| 0x4A3D20 | `ReportIf_reset` | `int (ReportInfc* This)` | 672 | 97856 | 66224 |
| 0x4A3FC0 | `ReportIf_on_redraw` | `int (ReportInfc* This)` | 144 | 98528 | 65552 |
| 0x4A4050 | `bl_daemon` | `void (int)` | 16 | 98672 | 65408 |
| 0x4A4060 | `ReportIf_bl_anim` | `int (ReportInfc* This)` | 368 | 98688 | 65392 |
| 0x4A41D0 | `ReportIf_on_status` | `void (ReportInfc* This, int faction_id)` | 7472 | 99056 | 65024 |
| 0x4A5F00 | `ReportWin_show` | `void (ReportWindow* This, int a2)` | 32 | 106528 | 57552 |
| 0x4A5F20 | `ReportWin_hide` | `int (ReportWindow* This)` | 32 | 106560 | 57520 |
| 0x4A5F40 | `ReportIf_on_iface_left_click` | `void (ReportInfc* This, int x, int y)` | 160 | 106592 | 57488 |
| 0x4A5FE0 | `ReportIf_on_iface_right_click` | `void (ReportInfc* This, int a2, int a3)` | 16 | 106752 | 57328 |
| 0x4A5FF0 | `ReportIf_on_iface_left_double_click` | `void (ReportInfc* This, int a2, int a3)` | 16 | 106768 | 57312 |
| 0x4A6000 | `ReportIf_on_iface_right_double_click` | `void (ReportInfc* This, int a2, int a3)` | 16 | 106784 | 57296 |
| 0x4A6010 | `ReportIf_on_iface_left_down` | `void (ReportInfc* This, int a2, int a3)` | 16 | 106800 | 57280 |
| 0x4A6020 | `ReportIf_on_iface_right_down` | `void (ReportInfc* This, int a2, int a3)` | 16 | 106816 | 57264 |
| 0x4A6030 | `ReportIf_on_iface_mouse_move` | `void (ReportInfc* This, int a2, int a3)` | 16 | 106832 | 57248 |
| 0x4A6040 | `ReportIf_on_iface_mouse_leave` | `void (ReportInfc* This, int a2, int a3)` | 16 | 106848 | 57232 |
| 0x4A6050 | `ReportIf_on_iface_selected` | `void (ReportInfc* This, int a2, int a3)` | 720 | 106864 | 57216 |
| 0x4A6320 | `ReportIf_on_iface_dialog_item_back_draw` | `int (ReportInfc* This, GraphicWin* a2, int a3, int a4, RECT* a5)` | 16 | 107584 | 56496 |
| 0x4A6330 | `ReportIf_on_iface_button_clicked` | `void (ReportInfc* This, int a2)` | 2608 | 107600 | 56480 |
| 0x4A6D60 | `ReportIf_on_iface_button_toggled` | `void (ReportInfc* This, int a2, int a3)` | 16 | 110208 | 53872 |
| 0x4A6D70 | `ReportIf_on_iface_group_clicked` | `void (ReportInfc* This, int a2, int a3, int a4)` | 656 | 110224 | 53856 |
| 0x4A7000 | `ReportIf_draw_labs` | `int (ReportInfc* This)` | 5184 | 110880 | 53200 |
| 0x4A8440 | `labs_write` | `void (GraphicWin*, int, int, RECT*, char*)` | 48 | 116064 | 48016 |
| 0x4A8470 | `ReportIf_close_labs` | `int (ReportInfc* This)` | 48 | 116112 | 47968 |
| 0x4A84A0 | `ReportIf_draw_energy` | `int (ReportInfc* This)` | 2944 | 116160 | 47920 |
| 0x4A9020 | `ReportIf_close_energy` | `int (ReportInfc* This)` | 48 | 119104 | 44976 |
| 0x4A9050 | `ReportIf_draw_ops` | `int (ReportInfc* This)` | 3904 | 119152 | 44928 |
| 0x4A9F90 | `ReportIf_close_ops` | `int (ReportInfc* This)` | 288 | 123056 | 41024 |
| 0x4AA0B0 | `ReportIf_draw_projects` | `int (ReportInfc* This)` | 2176 | 123344 | 40736 |
| 0x4AA930 | `ReportIf_close_projects` | `int (ReportInfc* This)` | 64 | 125520 | 38560 |
| 0x4AA970 | `ReportIf_draw_sat` | `int (ReportInfc* This)` | 3040 | 125584 | 38496 |
| 0x4AB550 | `ReportIf_close_sat` | `int (ReportInfc* This)` | 176 | 128624 | 35456 |
| 0x4AB600 | `ReportIf_draw_security` | `int (ReportInfc* This)` | 2096 | 128800 | 35280 |
| 0x4ABE30 | `ReportIf_close_security` | `int (ReportInfc* This)` | 64 | 130896 | 33184 |
| 0x4ABE70 | `ReportIf_draw_score` | `int (ReportInfc* This)` | 720 | 130960 | 33120 |
| 0x4AC140 | `ReportIf_draw_intel` | `int (ReportInfc* This)` | 2112 | 131680 | 32400 |
| 0x4AC980 | `ReportIf_close_intel` | `int (ReportInfc* This)` | 32 | 133792 | 30288 |
| 0x4AC9A0 | `report_labs` | `int ()` | 80 | 133824 | 30256 |
| 0x4AC9F0 | `report_energy` | `int ()` | 80 | 133904 | 30176 |
| 0x4ACA40 | `report_ops` | `int ()` | 80 | 133984 | 30096 |
| 0x4ACA90 | `report_projects` | `int ()` | 80 | 134064 | 30016 |
| 0x4ACAE0 | `report_sat` | `int ()` | 80 | 134144 | 29936 |
| 0x4ACB30 | `report_security` | `int ()` | 80 | 134224 | 29856 |
| 0x4ACB80 | `report_score` | `void (int)` | 80 | 134304 | 29776 |
| 0x4ACBD0 | `report_intel` | `void (int)` | 96 | 134384 | 29696 |
| 0x4ACC30 | `ReportIf_merge_sort` | `void (ReportInfc* This, int a2, int a3)` | 368 | 134480 | 29600 |
| 0x4ACDA0 | `ReportIf_dtor` | `int (ReportInfc* This)` | 976 | 134848 | 29232 |
| 0x4AD170 | `ReportIf_ctor` | `int (ReportInfc* This)` | 576 | 135824 | 28256 |
| 0x4AD3B0 | `ReportWin_dtor` | `int (ReportWindow* This)` | 768 | 136400 | 27680 |
| 0x4AD6B0 | `ReportWin_ctor` | `int (ReportWindow* This)` | 400 | 137168 | 26912 |
| 0x4AD840 | `ReportWin_scalar_dtor` | `void* (ReportWindow* This, uint32_t a2)` | 48 | 137568 | 26512 |
| 0x4AD870 | `ReportWin_operator_delete` | `void (void* a1, uint32_t a2)` | 16 | 137616 | 26464 |
| 0x4AD880 | `SetupWin_ctor` | `int (SetupWin* This)` | 320 | 137632 | 26448 |
| 0x4AD9C0 | `SetupWin_close` | `int (SetupWin* This)` | 192 | 137952 | 26128 |
| 0x4ADA80 | `SetupWin_UNK1` | `int (SetupWin* This, int a2)` | 32 | 138144 | 25936 |
| 0x4ADAA0 | `SetupWin_UNK2` | `int (SetupWin* This, int a2)` | 48 | 138176 | 25904 |
| 0x4ADAD0 | `SetupWin_UNK3` | `int (SetupWin* This, int a2)` | 16 | 138224 | 25856 |
| 0x4ADAE0 | `SetupWin_UNK4` | `int (SetupWin* This, int a2)` | 16 | 138240 | 25840 |
| 0x4ADAF0 | `SetupWin_do_menu` | `int (SetupWin* This, const char* a2, int a3, int a4)` | 48 | 138256 | 25824 |
| 0x4ADB20 | `SetupWin_do_menu_2` | `int (SetupWin* This, Popup* a2, int a3, int a4)` | 80 | 138304 | 25776 |
| 0x4ADB70 | `SetupWin_do_menu_rightside` | `int (SetupWin* This, const char* section_id, int a3)` | 2000 | 138384 | 25696 |
| 0x4AE340 | `SetupWin_on_left_click` | `void (SetupWin* This, int x, int y)` | 64 | 140384 | 23696 |
| 0x4AE380 | `SetupWin_on_mouse_move` | `int (SetupWin* This, int x, int y)` | 96 | 140448 | 23632 |
| 0x4AE3E0 | `SetupWin_on_key_click` | `int (SetupWin* This, int a2, int a3)` | 240 | 140544 | 23536 |
| 0x4AE4D0 | `SetupWin_draw_item` | `int (SetupWin* This, int position, int a3)` | 656 | 140784 | 23296 |
| 0x4AE760 | `SetupWin_scalar_dtor` | `void* (SetupWin* This, uint32_t a2)` | 48 | 141440 | 22640 |
| 0x4AE790 | `SetupWin_dtor` | `int (SetupWin* This)` | 528 | 141488 | 22592 |
| 0x4AE9A0 | `SetupWin_operator_delete` | `void (void* a1, uint32_t a2)` | 22064 | 142016 | 22064 |
| 0x4B43E0 | `say_orders` | `void (char*, int)` | 1424 | 32 | 86080 |
| 0x4B4970 | `say_orders_2` | `void (int)` | 32 | 1456 | 84656 |
| 0x4B4990 | `StatusWin_UNK1` | `int (StatusWindow* This)` | 144 | 1488 | 84624 |
| 0x4B4A20 | `StatusWin_set_title` | `void (StatusWindow* This, int a2)` | 464 | 1632 | 84480 |
| 0x4B4BF0 | `draw_my_dest` | `void (int)` | 224 | 2096 | 84016 |
| 0x4B4CD0 | `StatusWin_UNK2` | `int (StatusWindow* This)` | 32 | 2320 | 83792 |
| 0x4B4CF0 | `StatusWin_draw_active` | `void (StatusWindow* This, int veh_id, int mask, int a4)` | 6208 | 2352 | 83760 |
| 0x4B6530 | `StatusWin_reset_active` | `int (StatusWindow* This)` | 64 | 8560 | 77552 |
| 0x4B6570 | `StatusWin_draw_status` | `void (StatusWindow* This, int a2, int a3, int a4, int y, int a6)` | 8896 | 8624 | 77488 |
| 0x4B8830 | `StatusWin_draw` | `void (StatusWindow* This, int a2, int a3, int a4, int a5, int a6)` | 96 | 17520 | 68592 |
| 0x4B8890 | `StatusWin_on_redraw` | `int (StatusWindow* This)` | 224 | 17616 | 68496 |
| 0x4B8970 | `StatusWin_reset` | `int (StatusWindow* This)` | 32 | 17840 | 68272 |
| 0x4B8990 | `StatusWin_on_left_double_click` | `void (StatusWindow* This, int x, int y)` | 48 | 17872 | 68240 |
| 0x4B89C0 | `StatusWin_arrow_left` | `int (StatusWindow* This)` | 528 | 17920 | 68192 |
| 0x4B8BD0 | `StatusWin_arrow_right` | `int (StatusWindow* This)` | 544 | 18448 | 67664 |
| 0x4B8DF0 | `StatusWin_UNK3` | `int (StatusWindow* This)` | 80 | 18992 | 67120 |
| 0x4B8E40 | `StatusWin_on_left_click` | `void (StatusWindow* This, int x, int y)` | 464 | 19072 | 67040 |
| 0x4B9010 | `StatusWin_on_right_click` | `void (StatusWindow* This, int x, int y, int a4)` | 3712 | 19536 | 66576 |
| 0x4B9E90 | `StatusWin_UNK4` | `int (StatusWindow* This)` | 16 | 23248 | 62864 |
| 0x4B9EA0 | `StatusWin_redraw` | `int (StatusWindow* This)` | 224 | 23264 | 62848 |
| 0x4B9F80 | `StatusWin_close` | `int (StatusWindow* This)` | 16 | 23488 | 62624 |
| 0x4B9F90 | `StatusWin_set_loc` | `void (StatusWindow* This, int a2, int a3)` | 48 | 23504 | 62608 |
| 0x4B9FC0 | `StatusWin_init` | `int (StatusWindow* This)` | 352 | 23552 | 62560 |
| 0x4BA120 | `StatusWin_dtor` | `int (StatusWindow* This)` | 128 | 23904 | 62208 |
| 0x4BA1A0 | `StatusWin_ctor` | `int (StatusWindow* This)` | 128 | 24032 | 62080 |
| 0x4BA220 | `textcolor_init` | `int ()` | 976 | 24160 | 61952 |
| 0x4BA5F0 | `tut_check` | `int (int)` | 96 | 25136 | 60976 |
| 0x4BA650 | `tut_check2` | `int (int)` | 96 | 25232 | 60880 |
| 0x4BA6B0 | `TutWin_ctor` | `int (TutWindow* This)` | 112 | 25328 | 60784 |
| 0x4BA720 | `TutWin_UNK1` | `int (TutWindow* This)` | 80 | 25440 | 60672 |
| 0x4BA770 | `TutWin_on_key_click` | `int (TutWindow* This, int a2, int a3)` | 192 | 25520 | 60592 |
| 0x4BA830 | `TutWin_UNK2` | `void (TutWindow* This, RECT* a2, int* a3, int* a4)` | 64 | 25712 | 60400 |
| 0x4BA870 | `TutWin_do_base` | `void (TutWindow* This, RECT* a2, const char* a3, int a4)` | 128 | 25776 | 60336 |
| 0x4BA8F0 | `TutWin_do_iface` | `void (TutWindow* This, RECT* a2, const char* a3, int a4)` | 128 | 25904 | 60208 |
| 0x4BA970 | `TutWin_do_soc` | `void (TutWindow* This, RECT* a2, const char* a3, int a4)` | 128 | 26032 | 60080 |
| 0x4BA9F0 | `TutWin_do_des` | `void (TutWindow* This, RECT* a2, const char* a3, int a4)` | 128 | 26160 | 59952 |
| 0x4BAA70 | `TutWin_tour6` | `int (TutWindow* This)` | 1200 | 26288 | 59824 |
| 0x4BAF20 | `TutWin_tour7` | `int (TutWindow* This)` | 1120 | 27488 | 58624 |
| 0x4BB380 | `TutWin_tour5` | `int (TutWindow* This)` | 1056 | 28608 | 57504 |
| 0x4BB7A0 | `TutWin_tour1` | `int (TutWindow* This)` | 3584 | 29664 | 56448 |
| 0x4BC5A0 | `TutWin_iface_rect` | `void (TutWindow* This, RECT* a2, int* a3, int* a4)` | 80 | 33248 | 52864 |
| 0x4BC5F0 | `TutWin_base_rect` | `void (TutWindow* This, RECT* a2, int* a3, int* a4)` | 80 | 33328 | 52784 |
| 0x4BC640 | `TutWin_soc_rect` | `void (TutWindow* This, RECT* a2, int* a3, int* a4)` | 80 | 33408 | 52704 |
| 0x4BC690 | `TutWin_des_rect` | `void (TutWindow* This, RECT* a2, int* a3, int* a4)` | 80 | 33488 | 52624 |
| 0x4BC6E0 | `TutWin_tour` | `int (TutWindow* This)` | 5600 | 33568 | 52544 |
| 0x4BDCC0 | `TutWin_on_button_clicked` | `void (TutWindow* This, int a2)` | 240 | 39168 | 46944 |
| 0x4BDDB0 | `TutWin_on_move` | `void (TutWindow* This, int a2, int a3)` | 32 | 39408 | 46704 |
| 0x4BDDD0 | `TutWin_UNK3` | `void (TutWindow* This, int a2)` | 16 | 39440 | 46672 |
| 0x4BDDE0 | `TutWin_reset` | `int (TutWindow* This)` | 192 | 39456 | 46656 |
| 0x4BDEA0 | `TutWin_draw_arrow` | `int (TutWindow* This)` | 128 | 39648 | 46464 |
| 0x4BDF20 | `TutWin_terrain_draw` | `void (Sprite* a1, TutWindow* a2, int a3, int a4, int a5)` | 80 | 39776 | 46336 |
| 0x4BDF70 | `TutWin_veh_draw` | `void (Sprite* a1, TutWindow* a2, int veh_id, int a4, int a5)` | 48 | 39856 | 46256 |
| 0x4BDFA0 | `TutWin_base_draw` | `void (Sprite* a1, TutWindow* a2, int a3, int a4, int a5)` | 64 | 39904 | 46208 |
| 0x4BDFE0 | `TutWin_tut_win` | `int (TutWindow* This, GraphicWin* a2, const char* label, int a4, int a5, Sprite* a6, int a7, int a8, int a9)` | 1648 | 39968 | 46144 |
| 0x4BE650 | `TutWin_tut_map` | `int (TutWindow* This, const char* label, int x, int y, int a5, Sprite* a6, int a7, int x2, int y2)` | 384 | 41616 | 44496 |
| 0x4BE7D0 | `TutWin_dtor` | `int (TutWindow* This)` | 544 | 42000 | 44112 |
| 0x4BE9F0 | `TutWin_scalar_dtor` | `int (TutWindow* This, uint32_t a2)` | 48 | 42544 | 43568 |
| 0x4BEA20 | `TutWin_operator_delete` | `void (void* a1, uint32_t a2)` | 3360 | 42592 | 43520 |
| 0x500560 | `shoot` | `int (int, int, int, int)` | 1536 | 64 | 2912 |
| 0x500B60 | `planet_busting` | `int (int, int, int)` | 1376 | 1600 | 1376 |
| 0x501DA0 | `battle_compute` | `void (int, int, int*, int*, int)` | 10032 | 80 | 18160 |
| 0x5044D0 | `best_defender` | `int (int, int, BOOL)` | 1488 | 10112 | 8128 |
| 0x504AA0 | `boom` | `int (int, int, int)` | 4832 | 11600 | 6640 |
| 0x505D80 | `battle_kill` | `int (int, int*, int*, int*, int, int)` | 944 | 16432 | 1808 |
| 0x506130 | `battle_kill_stack` | `void (int, int*, int*, int*, int, int)` | 128 | 17376 | 864 |
| 0x5061B0 | `battle_kill_chaff` | `void (int, int*, int*, int*, int, int)` | 256 | 17504 | 736 |
| 0x5062B0 | `promote` | `void (int)` | 480 | 17760 | 480 |
| 0x506650 | `interceptor` | `int (int, int, int, int)` | 1040 | 448 | 21184 |
| 0x506A60 | `battle_fight` | `int (int, int, int, int, int*)` | 144 | 1488 | 20144 |
| 0x506AF0 | `battle_fight_2` | `int (int, int, int, int, int, int, int*)` | 19888 | 1632 | 20000 |
| 0x50B970 | `popp_2` | `int (const char*, const char*, fp_none)` | 48 | 64 | 48 |
| 0x55E370 | `encounter` | `void (int, int, int, int, int, int)` | 2064 | 10192 | 10080 |
| 0x55EB80 | `territory` | `int (int, int, int, int*, int*)` | 864 | 12256 | 8016 |
| 0x55EEE0 | `atrocity` | `void (int, int, int, int)` | 1392 | 13120 | 7152 |
| 0x55F450 | `major_atrocity` | `void (int, int)` | 800 | 14512 | 5760 |
| 0x55F770 | `break_treaty` | `int (int, int, int)` | 448 | 15312 | 4960 |
| 0x55F930 | `enemy_diplomacy` | `void (int)` | 4512 | 15760 | 4512 |
| 0x57A050 | `monolith` | `void (int)` | 1552 | 208 | 11776 |
| 0x57A660 | `goody_box` | `int (int)` | 10224 | 1760 | 10224 |
| 0x57DF00 | `clear_bunglist` | `void (int)` | 192 | 1120 | 4256 |
| 0x57DFC0 | `is_bunged` | `bool (int, int, int, int, int, int)` | 128 | 1312 | 4064 |
| 0x57E040 | `name_proto` | `void (char*, int, int, int, int, int, int, int)` | 3936 | 1440 | 3936 |
| 0x57F1D0 | `retire_proto` | `void (int, int)` | 736 | 128 | 8096 |
| 0x57F4B0 | `prune_protos` | `void (int, int, int)` | 5040 | 864 | 7360 |
| 0x580860 | `propose_proto` | `int (int, int, int, int, int, int, int, char*)` | 2320 | 5904 | 2320 |
| 0x581190 | `add_abil` | `void (int*, int, int)` | 208 | 32 | 15312 |
| 0x581260 | `consider_designs` | `void (int)` | 10832 | 240 | 15104 |
| 0x592E10 | `message_veh` | `void (int, int, int, int)` | 96 | 1824 | 1792 |
| 0x592E70 | `message_base` | `void (int, int, char*, int)` | 112 | 1920 | 1696 |
| 0x592EE0 | `message_data` | `void (int, int, int, int, int, int)` | 272 | 2032 | 1584 |
| 0x592FF0 | `message_big_data` | `void (int, int, int*, int)` | 160 | 2304 | 1312 |
| 0x593090 | `message_lock` | `void (int, int, int, int, int, int, int, int)` | 128 | 2464 | 1152 |
| 0x593110 | `message_landmark` | `void (int, int, char*, int, int, int, int)` | 128 | 2592 | 1024 |
| 0x593190 | `message_chat` | `void (int, int, char*)` | 144 | 2720 | 896 |
| 0x593220 | `synch_veh` | `void (int)` | 48 | 2864 | 752 |
| 0x593250 | `synch_base` | `void (int)` | 48 | 2912 | 704 |
| 0x593280 | `synch_energy` | `void (int)` | 48 | 2960 | 656 |
| 0x5932B0 | `synch_researching` | `void (int)` | 48 | 3008 | 608 |
| 0x5932E0 | `synch_leader` | `void (int)` | 48 | 3056 | 560 |
| 0x593310 | `synch_ai` | `void (int)` | 48 | 3104 | 512 |
| 0x593340 | `synch_research` | `void (int)` | 80 | 3152 | 464 |
| 0x593390 | `synch_alloc` | `void (int)` | 48 | 3232 | 384 |
| 0x5933C0 | `synch_soc` | `void (int)` | 48 | 3280 | 336 |
| 0x5933F0 | `synch_proto` | `void (int)` | 48 | 3328 | 288 |
| 0x593420 | `synch_obs` | `void (int)` | 48 | 3376 | 240 |
| 0x593450 | `synch_diplo` | `void (int, int)` | 96 | 3424 | 192 |
| 0x5934B0 | `synch_template` | `void (int)` | 48 | 3520 | 96 |
| 0x5934E0 | `synch_radius` | `void (int)` | 48 | 3568 | 48 |
| 0x5A5990 | `Popup_start_4` | `void (Popup* This, const char* label)` | 32 | 128 | 32 |
| 0x5ADBF0 | `show_replay` | `int ()` | 656 | 5488 | 4448 |
| 0x5ADE80 | `replay_base` | `void (int, int, int, int)` | 672 | 6144 | 3792 |
| 0x5AE120 | `crash_landing` | `void (int)` | 3120 | 6816 | 3120 |
| 0x5AEDE0 | `time_warp` | `int ()` | 5696 | 144 | 8080 |
| 0x5B5AD0 | `spot_loc` | `void (int, int, int)` | 976 | 1088 | 976 |
| 0x5B6260 | `spot_all` | `void (int, BOOL)` | 10384 | 512 | 10384 |
| 0x5B8EE0 | `stack_veh` | `int (int, int)` | 1584 | 208 | 1696 |
| 0x5B9510 | `stack_kill` | `void (int)` | 112 | 1792 | 112 |
| 0x5BE2A0 | `tech_wizard_unk2` | `void (int, int, char, int, int, int)` | 224 | 416 | 1040 |
| 0x5BE380 | `tech_selection` | `int (int)` | 432 | 640 | 816 |
| 0x5BF310 | `X_pop` | `int (const char*, fp_none)` | 32 | 288 | 2944 |
| 0x5BF330 | `X_pop_2` | `int (const char*, const char*, fp_none)` | 32 | 320 | 2912 |
| 0x5BF350 | `X_pop_3` | `int (const char*, const char*, char*, int, fp_none)` | 64 | 352 | 2880 |
| 0x5BF390 | `X_pop_4` | `int (const char*, int, int, fp_none)` | 48 | 416 | 2816 |
| 0x5BF3C0 | `X_pop_5` | `int (const char*, const char*, int, int, fp_none)` | 48 | 464 | 2768 |
| 0x5BF3F0 | `X_pop_6` | `int (const char*, int, fp_none)` | 48 | 512 | 2720 |
| 0x5BF420 | `X_pop_7` | `int (const char*, const char*, int, fp_none)` | 48 | 560 | 2672 |
| 0x5BF450 | `X_pop_8` | `int (const char*, int, char*, int, fp_none)` | 48 | 608 | 2624 |
| 0x5BF480 | `X_pop_9` | `int (const char*, const char*, int, char*, int, fp_none)` | 336 | 656 | 2576 |
| 0x5BF5D0 | `X_pops` | `int (const char*, Sprite*, fp_none)` | 48 | 992 | 2240 |
| 0x5BF600 | `X_pops_2` | `int (const char*, Sprite*, int, int, fp_none)` | 48 | 1040 | 2192 |
| 0x5BF630 | `X_pops_3` | `int (const char*, const char*, Sprite*, fp_none)` | 48 | 1088 | 2144 |
| 0x5BF660 | `X_pops_4` | `int (const char*, const char*, Sprite*, int, int, fp_none)` | 48 | 1136 | 2096 |
| 0x5BF690 | `X_pops_5` | `int (const char*, int, int, Sprite*, fp_none)` | 48 | 1184 | 2048 |
| 0x5BF6C0 | `X_pops_6` | `int (const char*, int, int, Sprite*, int, int, fp_none)` | 48 | 1232 | 2000 |
| 0x5BF6F0 | `X_pops_7` | `int (const char*, const char*, char*, int, Sprite*, fp_none)` | 64 | 1280 | 1952 |
| 0x5BF730 | `X_pops_8` | `int (const char*, const char*, char*, int, Sprite*, int, int, fp_none)` | 64 | 1344 | 1888 |
| 0x5BF770 | `X_pops_9` | `int (const char*, const char*, int, int, Sprite*, fp_none)` | 48 | 1408 | 1824 |
| 0x5BF7A0 | `X_pops_10` | `int (const char*, const char*, int, int, Sprite*, int, int, fp_none)` | 48 | 1456 | 1776 |
| 0x5BF7D0 | `X_pops_11` | `int (const char*, int, Sprite*, fp_none)` | 48 | 1504 | 1728 |
| 0x5BF800 | `X_pops_12` | `int (const char*, int, Sprite*, int, int, fp_none)` | 48 | 1552 | 1680 |
| 0x5BF830 | `X_pops_13` | `int (const char*, const char*, int, Sprite*, fp_none)` | 48 | 1600 | 1632 |
| 0x5BF860 | `X_pops_14` | `int (const char*, const char*, int, Sprite*, int, int, fp_none)` | 48 | 1648 | 1584 |
| 0x5BF890 | `X_pops_15` | `int (const char*, int, char*, int, Sprite*, fp_none)` | 48 | 1696 | 1536 |
| 0x5BF8C0 | `X_pops_16` | `int (const char*, int, char*, int, Sprite*, int, int, fp_none)` | 64 | 1744 | 1488 |
| 0x5BF900 | `X_pops_17` | `int (const char*, const char*, int, char*, int, Sprite*, fp_none)` | 48 | 1808 | 1424 |
| 0x5BF930 | `X_pops_18` | `int (const char*, const char*, int, char*, int, Sprite*, int, int, fp_none)` | 384 | 1856 | 1376 |
| 0x5BFAB0 | `X_pop_ask` | `int (const char*, int, char*, fp_none, int)` | 48 | 2240 | 992 |
| 0x5BFAE0 | `X_pop_ask_2` | `int (const char*, const char*, int, fp_none, int)` | 48 | 2288 | 944 |
| 0x5BFB10 | `X_pop_ask_3` | `int (const char*, int, fp_none, int)` | 48 | 2336 | 896 |
| 0x5BFB40 | `X_pop_ask_4` | `int (const char*, const char*, char*, fp_none, int)` | 64 | 2384 | 848 |
| 0x5BFB80 | `X_pop_ask_5` | `int (const char*, char*, fp_none, int)` | 64 | 2448 | 784 |
| 0x5BFBC0 | `X_pop_ask_6` | `int (const char*, const char*, int, char*, fp_none, int)` | 336 | 2512 | 720 |
| 0x5BFD10 | `X_pop_ask_number` | `int (const char*, int, fp_none)` | 32 | 2848 | 384 |
| 0x5BFD30 | `X_pop_ask_number_2` | `int (const char*, const char*, fp_none)` | 32 | 2880 | 352 |
| 0x5BFD50 | `X_pop_ask_number_3` | `int (const char*, fp_none)` | 32 | 2912 | 320 |
| 0x5BFD70 | `X_pop_ask_number_4` | `int (const char*, const char*, int, fp_none)` | 288 | 2944 | 288 |
| 0x5C03D0 | `veh_init` | `int (int, int, int, int)` | 1264 | 256 | 2528 |
| 0x5C08C0 | `veh_kill` | `void (int)` | 576 | 1520 | 1264 |
| 0x5C0B00 | `kill` | `void (int)` | 432 | 2096 | 688 |
| 0x5C0CB0 | `veh_find` | `int (int, int, int, int)` | 256 | 2528 | 256 |
| 0x5C1A20 | `veh_selectable` | `int (int)` | 144 | 464 | 544 |
| 0x5C1AB0 | `veh_unmoved` | `int (int)` | 192 | 608 | 400 |
| 0x5C1B70 | `veh_ready` | `int (int)` | 208 | 800 | 208 |
| 0x5C2020 | `world_alt_set` | `void (int, int, int, int)` | 864 | 688 | 14496 |
| 0x5C2380 | `world_raise_alt` | `void (int, int)` | 96 | 1552 | 13632 |
| 0x5C23E0 | `world_lower_alt` | `void (int, int)` | 96 | 1648 | 13536 |
| 0x5C2440 | `brush` | `void (int, int, int)` | 944 | 1744 | 13440 |
| 0x5C27F0 | `paint_land` | `void (int, int, int, int)` | 256 | 2688 | 12496 |
| 0x5C28F0 | `build_continent` | `void (int)` | 592 | 2944 | 12240 |
| 0x5C2B40 | `build_hills` | `void (int)` | 368 | 3536 | 11648 |
| 0x5C2CB0 | `world_erosion` | `int ()` | 1520 | 3904 | 11280 |
| 0x5C32A0 | `world_rocky` | `int ()` | 416 | 5424 | 9760 |
| 0x5C3440 | `world_fungus` | `int ()` | 576 | 5840 | 9344 |
| 0x5C3680 | `world_riverbeds` | `int ()` | 560 | 6416 | 8768 |
| 0x5C38B0 | `world_rivers` | `int ()` | 1728 | 6976 | 8208 |

### TERRAFORMING (124 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x4BF740 | `compute_camera` | `int (void*, void*)` | 400 | 45952 | 40160 |
| 0x4BF8D0 | `vehdraw_turn_start` | `int (int, int, int, int)` | 928 | 46352 | 39760 |
| 0x4BFC70 | `vehdraw_turn` | `void (Buffer*, int, int, int, int)` | 400 | 47280 | 38832 |
| 0x4BFE00 | `vehdraw_scoot_start` | `void (int, int, int, int)` | 4432 | 47680 | 38432 |
| 0x4C0F50 | `vehdraw_scoot` | `void (Buffer*, int, int, int, int)` | 848 | 52112 | 34000 |
| 0x4C12A0 | `vehdraw_draw` | `void (Buffer*, int, int, int, int, int, int)` | 1184 | 52960 | 33152 |
| 0x4C1740 | `vehdraw_unk1` | `void (Buffer*, int, int, int, uint32_t, int, int, int)` | 976 | 54144 | 31968 |
| 0x4C1B10 | `vehdraw_compute_facet_normals` | `int ()` | 512 | 55120 | 30992 |
| 0x4C1D10 | `vehdraw_setup` | `int ()` | 16 | 55632 | 30480 |
| 0x4C1D20 | `vehdraw_construct_vehicle` | `int (int, Caviar*)` | 784 | 55648 | 30464 |
| 0x4C2030 | `vehdraw_construct_abil` | `int (int, Caviar*, int)` | 816 | 56432 | 29680 |
| 0x4C2360 | `vehdraw_construct_vehicle_2` | `int (int, int, int, int, int, int, Caviar*)` | 272 | 57248 | 28864 |
| 0x4C2470 | `vehdraw_construct_chassis` | `int (int, int, int, int, int, Caviar*)` | 2176 | 57520 | 28592 |
| 0x4C2CF0 | `vehdraw_construct_weapon` | `int (int, int, int, Caviar*)` | 928 | 59696 | 26416 |
| 0x4C3090 | `vehdraw_construct_armor` | `int (int, int, int, int, int, Caviar*)` | 1904 | 60624 | 25488 |
| 0x4C3800 | `vehdraw_construct_reactor` | `int (int, int, int, int, int, Caviar*)` | 336 | 62528 | 23584 |
| 0x4C3950 | `WorldWin_set_detail_map` | `int (WorldWindow* This)` | 64 | 62864 | 23248 |
| 0x4C3990 | `WorldWin_set_world_map` | `int (WorldWindow* This)` | 64 | 62928 | 23184 |
| 0x4C39D0 | `WorldWin_show_all` | `int (WorldWindow* This)` | 112 | 62992 | 23120 |
| 0x4C3A40 | `WorldWin_hide_all` | `int (WorldWindow* This)` | 48 | 63104 | 23008 |
| 0x4C3A70 | `WorldWin_UNK1` | `int (WorldWindow* This)` | 64 | 63152 | 22960 |
| 0x4C3AB0 | `WorldWin_right_menu` | `void (WorldWindow* This, int a2, int a3)` | 656 | 63216 | 22896 |
| 0x4C3D40 | `WorldWin_click` | `void (WorldWindow* This, int a2, int x, int y, int a5)` | 272 | 63872 | 22240 |
| 0x4C3E50 | `WorldWin_on_left_click` | `void (WorldWindow* This, int a2, int a3)` | 32 | 64144 | 21968 |
| 0x4C3E70 | `WorldWin_on_right_click` | `void (WorldWindow* This, int a2, int a3)` | 32 | 64176 | 21936 |
| 0x4C3E90 | `WorldWin_on_left_double_click` | `void (WorldWindow* This, int a2, int a3)` | 16 | 64208 | 21904 |
| 0x4C3EA0 | `WorldWin_follow_center` | `void (WorldWindow* This, int x, int y)` | 32 | 64224 | 21888 |
| 0x4C3EC0 | `WorldWin_on_redraw` | `int (WorldWindow* This)` | 880 | 64256 | 21856 |
| 0x4C4230 | `WorldWin_UNK2` | `void (WorldWindow* This, int a2)` | 448 | 65136 | 20976 |
| 0x4C43F0 | `WorldWin_pixel_to_tile` | `int (WorldWindow* This, int a2, int a3, int* a4, int* a5)` | 240 | 65584 | 20528 |
| 0x4C44E0 | `WorldWin_tile_to_pixel` | `int (WorldWindow* This, int a2, int a3, int* a4, int* a5)` | 256 | 65824 | 20288 |
| 0x4C45E0 | `WorldWin_clear_terrain` | `int (WorldWindow* This)` | 16 | 66080 | 20032 |
| 0x4C45F0 | `WorldWin_set_center` | `void (WorldWindow* This, int a2, int a3)` | 32 | 66096 | 20016 |
| 0x4C4610 | `WorldWin_focus` | `void (WorldWindow* This, int a2, int a3)` | 32 | 66128 | 19984 |
| 0x4C4630 | `WorldWin_draw_box` | `int (WorldWindow* This)` | 448 | 66160 | 19952 |
| 0x4C47F0 | `WorldWin_UNK3` | `int (WorldWindow* This)` | 432 | 66608 | 19504 |
| 0x4C49A0 | `WorldWin_init` | `void (WorldWindow* This, int a2)` | 208 | 67040 | 19072 |
| 0x4C4A70 | `WorldWin_dtor` | `int (WorldWindow* This)` | 384 | 67248 | 18864 |
| 0x4C4BF0 | `WorldWin_ctor` | `int (WorldWindow* This)` | 128 | 67632 | 18480 |
| 0x4C4C70 | `WorldWin_scalar_dtor` | `int (WorldWindow* This, uint32_t a2)` | 48 | 67760 | 18352 |
| 0x4C4CA0 | `WorldWin_operator_delete` | `void (void* a1, uint32_t a2)` | 4160 | 67808 | 18304 |
| 0x4C5CE0 | `init_sound` | `int (void*, uint32_t)` | 176 | 71968 | 14144 |
| 0x4C5D90 | `release_sound` | `int ()` | 192 | 72144 | 13968 |
| 0x4C5E50 | `load_sound_dll` | `int ()` | 288 | 72336 | 13776 |
| 0x4C5F70 | `unload_sound_dll` | `int ()` | 64 | 72624 | 13488 |
| 0x4C5FB0 | `init_sound_timer` | `int (uint32_t, uint32_t)` | 48 | 72688 | 13424 |
| 0x4C5FE0 | `get_sound_version` | `int ()` | 1536 | 72736 | 13376 |
| 0x4C65E0 | `Sound_fade` | `int (Sound* This)` | 32 | 74272 | 11840 |
| 0x4C6600 | `Sound_fade_2` | `void (Sound* This, uint32_t a2)` | 224 | 74304 | 11808 |
| 0x4C66E0 | `Wave_ctor` | `int (Wave* This)` | 224 | 74528 | 11584 |
| 0x4C67C0 | `Wave_dtor` | `int (Wave* This)` | 304 | 74752 | 11360 |
| 0x4C68F0 | `Wave_play` | `int (Wave* This, int a2)` | 48 | 75056 | 11056 |
| 0x4C6920 | `Wave_play_2` | `int (Wave* This)` | 144 | 75104 | 11008 |
| 0x4C69B0 | `Wave_init` | `void (Wave* This, char* a2, uint32_t a3)` | 304 | 75248 | 10864 |
| 0x4C6AE0 | `Wave_is_hwbuffer` | `int (Wave* This)` | 32 | 75552 | 10560 |
| 0x4C6B00 | `Wave_get_ms_length` | `int (Wave* This)` | 16 | 75584 | 10528 |
| 0x4C6B10 | `Wave_is_playing` | `int (Wave* This)` | 80 | 75600 | 10512 |
| 0x4C6B60 | `Wave_set_fname` | `int (Wave* This, const char* a2)` | 80 | 75680 | 10432 |
| 0x4C6BB0 | `Wave_dyna_load` | `int (Wave* This, char* a2)` | 112 | 75760 | 10352 |
| 0x4C6C20 | `Wave_load` | `int (Wave* This, const char* a2)` | 192 | 75872 | 10240 |
| 0x4C6CE0 | `Wave_load_2` | `int (Wave* This)` | 208 | 76064 | 10048 |
| 0x4C6DB0 | `Wave_load_3` | `int (Wave* This, const char* a2, uint32_t a3)` | 64 | 76272 | 9840 |
| 0x4C6DF0 | `Wave_reload` | `int (Wave* This)` | 176 | 76336 | 9776 |
| 0x4C6EA0 | `Wave_unload` | `int (Wave* This)` | 64 | 76512 | 9600 |
| 0x4C6EE0 | `Wave_set_pitch` | `void (Wave* This, int a2)` | 64 | 76576 | 9536 |
| 0x4C6F20 | `Wave_set_attrib` | `void (Wave* This, uint32_t a2)` | 96 | 76640 | 9472 |
| 0x4C6F80 | `Wave_get_attrib` | `int (Wave* This)` | 80 | 76736 | 9376 |
| 0x4C6FD0 | `Wave_get_time` | `int (Wave* This, uint32_t a2)` | 32 | 76816 | 9296 |
| 0x4C6FF0 | `Wave_get_current_marker` | `int (Wave* This)` | 32 | 76848 | 9264 |
| 0x4C7010 | `Wave_get_game_hwnd` | `int (Wave* This)` | 16 | 76880 | 9232 |
| 0x4C7020 | `Wave_get_ndevices` | `int (Wave* This)` | 32 | 76896 | 9216 |
| 0x4C7040 | `Wave_get_device_description` | `int (Wave* This, char* a2, int a3, int a4)` | 64 | 76928 | 9184 |
| 0x4C7080 | `Wave_UNK1` | `int (Wave* This, int a2)` | 32 | 76992 | 9120 |
| 0x4C70A0 | `Wave_set_reverb_mix` | `int (Wave* This, float a2)` | 48 | 77024 | 9088 |
| 0x4C70D0 | `Wave_set_bufflimit` | `int (Wave* This, uint32_t a2)` | 16 | 77072 | 9040 |
| 0x4C70E0 | `Wave_set_asdr` | `int (Wave* This)` | 16 | 77088 | 9024 |
| 0x4C70F0 | `Wave_set_attack` | `int (Wave* This, uint32_t a2, uint32_t a3, uint32_t a4)` | 16 | 77104 | 9008 |
| 0x4C7100 | `Wave_set_sustain` | `int (Wave* This, uint32_t a2, uint32_t a3, uint32_t a4)` | 16 | 77120 | 8992 |
| 0x4C7110 | `Wave_set_decay` | `int (Wave* This, uint32_t a2, uint32_t a3, uint32_t a4)` | 16 | 77136 | 8976 |
| 0x4C7120 | `Wave_set_release` | `int (Wave* This, uint32_t a2, uint32_t a3, uint32_t a4)` | 16 | 77152 | 8960 |
| 0x4C7130 | `Wave_set_volume` | `void (Wave* This, int a2)` | 96 | 77168 | 8944 |
| 0x4C7190 | `Wave_set_position3d` | `int (Wave* This, float a2, float a3, float a4)` | 48 | 77264 | 8848 |
| 0x4C71C0 | `Wave_set_xpos` | `int (Wave* This, float a2)` | 48 | 77312 | 8800 |
| 0x4C71F0 | `Wave_set_ypos` | `int (Wave* This, float a2)` | 48 | 77360 | 8752 |
| 0x4C7220 | `Wave_set_zpos` | `int (Wave* This, float a2)` | 48 | 77408 | 8704 |
| 0x4C7250 | `Wave_is_3d` | `int (Wave* This)` | 8368 | 77456 | 8656 |
| 0x4C9300 | `Wave_scalar_dtor` | `void* (Wave* This, uint32_t a2)` | 288 | 85824 | 288 |
| 0x4C96E0 | `action_build` | `int (int, char*)` | 880 | 704 | 880 |
| 0x4C9B00 | `action_terraform` | `int (int, int, BOOL)` | 3312 | 176 | 106192 |
| 0x4CA7F0 | `action_staple` | `void (int)` | 608 | 3488 | 102880 |
| 0x4CAA50 | `action_destroy` | `void (int veh_id, int flags, int x, int y)` | 2240 | 4096 | 102272 |
| 0x4CB310 | `action_go_to` | `void (int)` | 624 | 6336 | 100032 |
| 0x4CB580 | `action_road_to` | `void (int)` | 1312 | 6960 | 99408 |
| 0x4CBAA0 | `action_home` | `int (int, int)` | 2240 | 8272 | 98096 |
| 0x4CC360 | `action_airdrop` | `int (int veh_id, int x, int y, int flags)` | 3328 | 10512 | 95856 |
| 0x4CD060 | `action_feature` | `int ()` | 48 | 13840 | 92528 |
| 0x4CD090 | `action_arty` | `void (int, int, int)` | 608 | 13888 | 92480 |
| 0x4CD2F0 | `action_destruct` | `void (int)` | 496 | 14496 | 91872 |
| 0x4CD4E0 | `action_oblit` | `void (int, int)` | 448 | 14992 | 91376 |
| 0x4CD6A0 | `valid_patrol` | `int (int, int, int)` | 912 | 15440 | 90928 |
| 0x4CDA30 | `action_patrol` | `void (int, int, int)` | 192 | 16352 | 90016 |
| 0x4CDAF0 | `shoot_it` | `int (int, int, int, int, int)` | 1824 | 16544 | 89824 |
| 0x4CE210 | `action_tectonic` | `void (int, int, int)` | 1408 | 18368 | 88000 |
| 0x4CE790 | `action_fungal` | `void (int, int, int)` | 1616 | 19776 | 86592 |
| 0x4CEDE0 | `action_give` | `void (int, int)` | 1440 | 21392 | 84976 |
| 0x4CF380 | `action_gate` | `void (int veh_id, int base_id)` | 256 | 22832 | 83536 |
| 0x4CF480 | `action_sat_attack` | `void (int, int, int, int)` | 704 | 23088 | 83280 |
| 0x4CF740 | `action` | `int (int)` | 128 | 23792 | 82576 |
| 0x4CF7C0 | `Console_new_base` | `void (Console* This, int veh_id)` | 3216 | 23920 | 82448 |
| 0x4D0450 | `Console_skip` | `void (Console* This, int veh_id)` | 80 | 27136 | 79232 |
| 0x4D04A0 | `Console_wait` | `void (Console* This, int a2)` | 64 | 27216 | 79152 |
| 0x4D04E0 | `Console_unload` | `void (Console* This, int veh_id)` | 480 | 27280 | 79088 |
| 0x4D06C0 | `Console_upgrade` | `int (Console* This, int veh_id)` | 7008 | 27760 | 78608 |
| 0x4D2220 | `Console_sentry` | `void (Console* This, int veh_id, int a3)` | 672 | 34768 | 71600 |
| 0x4D24C0 | `Console_hold` | `void (Console* This, int veh_id, int a3)` | 480 | 35440 | 70928 |
| 0x4D26A0 | `Console_give_veh` | `void (Console* This, int veh_id)` | 1056 | 35920 | 70448 |
| 0x4D2AC0 | `Console_disband` | `void (Console* This, int veh_id)` | 800 | 36976 | 69392 |
| 0x4D2DE0 | `Console_disband_2` | `void (Console* This, int x, int y)` | 288 | 37776 | 68592 |
| 0x4D2F00 | `Console_activate` | `void (Console* This, int x, int y, int a4)` | 1392 | 38064 | 68304 |
| 0x4D3470 | `Console_terraform` | `void (Console* This, int veh_id, int terraform_id)` | 7728 | 39456 | 66912 |
| 0x4D52A0 | `Console_destroy` | `void (Console* This, int veh_id)` | 2704 | 47184 | 59184 |
| 0x4D5D30 | `Console_oblit` | `void (Console* This, int veh_id)` | 832 | 49888 | 56480 |
| 0x4D6070 | `Console_goto_routine` | `void (int a1)` | 304 | 50720 | 55648 |
| 0x4D61A0 | `Console_go_to` | `void (Console* This, int veh_id, int a3, int a4)` | 6720 | 51024 | 55344 |

### FACTION (81 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x4EA540 | `base_psych` | `int ()` | 3920 | 160 | 3920 |
| 0x50B8A0 | `say_num` | `void (int)` | 48 | 21520 | 112 |
| 0x50B8D0 | `POP3_2` | `void (const char*, int, int, int)` | 64 | 21568 | 64 |
| 0x5391C0 | `net_treaty_on` | `void (int, int, int, int)` | 112 | 96 | 2480 |
| 0x539230 | `net_treaty_off` | `void (int, int, int, int)` | 112 | 208 | 2368 |
| 0x5392A0 | `net_set_treaty` | `void (int, int, int, int, int)` | 112 | 320 | 2256 |
| 0x539310 | `net_agenda_on` | `void (int, int, int, int)` | 112 | 432 | 2144 |
| 0x539380 | `net_agenda_off` | `void (int, int, int, int)` | 112 | 544 | 2032 |
| 0x5393F0 | `net_set_agenda` | `void (int, int, int, int, int)` | 112 | 656 | 1920 |
| 0x539460 | `net_energy` | `void (int, int, int, int, int)` | 176 | 768 | 1808 |
| 0x539510 | `net_loan` | `void (int, int, int, int)` | 112 | 944 | 1632 |
| 0x539580 | `net_maps` | `void (int, int, int)` | 112 | 1056 | 1520 |
| 0x5395F0 | `net_tech` | `void (int, int, int, int)` | 112 | 1168 | 1408 |
| 0x539660 | `net_pact_ends` | `void (int, int, int)` | 112 | 1280 | 1296 |
| 0x5396D0 | `net_withdrawal` | `void (int, int, int)` | 112 | 1392 | 1184 |
| 0x539740 | `net_cede_base` | `void (int, int, int)` | 112 | 1504 | 1072 |
| 0x5397B0 | `net_double_cross` | `void (int, int, int, int)` | 112 | 1616 | 960 |
| 0x539820 | `diplo_lock` | `int (int)` | 160 | 1728 | 848 |
| 0x5398C0 | `diplo_unlock` | `int ()` | 32 | 1888 | 688 |
| 0x5398E0 | `pop_wait` | `int ()` | 64 | 1920 | 656 |
| 0x539920 | `pop_expire` | `int ()` | 128 | 1984 | 592 |
| 0x5399A0 | `diplomacy_caption` | `void (int, int)` | 464 | 2112 | 464 |
| 0x53A230 | `mention_prototypes` | `void (int, int)` | 624 | 112 | 68976 |
| 0x53A4A0 | `scan_prototypes` | `void (int, int)` | 736 | 736 | 68352 |
| 0x53A780 | `contiguous` | `int (int, int, int, int)` | 512 | 1472 | 67616 |
| 0x53A980 | `diplomacy_check` | `void (int, int, int)` | 6640 | 1984 | 67104 |
| 0x53C370 | `pact_withdraw` | `int (int, int)` | 736 | 8624 | 60464 |
| 0x53C650 | `pact_ends` | `void (int, int)` | 1184 | 9360 | 59728 |
| 0x53CAF0 | `pact_of_brotherhood` | `void (int, int)` | 800 | 10544 | 58544 |
| 0x53CE10 | `make_treaty` | `void (int, int)` | 848 | 11344 | 57744 |
| 0x53D160 | `pledge_truce` | `void (int, int, int)` | 512 | 12192 | 56896 |
| 0x53D360 | `introduce` | `void (int, int)` | 2432 | 12704 | 56384 |
| 0x53DCE0 | `wants_to_speak` | `int (int, int)` | 1936 | 15136 | 53952 |
| 0x53E470 | `diplomacy_ends` | `void (int, int)` | 80 | 17072 | 52016 |
| 0x53E4C0 | `demands_withdrawal` | `void (int, int)` | 256 | 17152 | 51936 |
| 0x53E5C0 | `renounce_pact` | `void (int, int)` | 576 | 17408 | 51680 |
| 0x53E800 | `tech_analysis` | `void (int, int)` | 848 | 17984 | 51104 |
| 0x53EB50 | `buy_council_vote` | `void (int, int, int, int)` | 5712 | 18832 | 50256 |
| 0x5401A0 | `buy_tech` | `int (int, int, int, int, int)` | 1168 | 24544 | 44544 |
| 0x540630 | `energy_trade` | `int (int, int)` | 2768 | 25712 | 43376 |
| 0x541100 | `tech_trade` | `int (int, int, int, int, int)` | 10528 | 28480 | 40608 |
| 0x5593E0 | `pick_top_veh` | `int (int)` | 272 | 512 | 10064 |
| 0x5594F0 | `veh_draw` | `int (Buffer*, int, int, int, int, int, int)` | 3168 | 784 | 9792 |
| 0x55A150 | `veh_scoot` | `void (int, int, int, int, int)` | 3536 | 3952 | 6624 |
| 0x55AF20 | `base_draw` | `void (Buffer* buffer, int base_id, int x, int y, int zoom, int opts)` | 2112 | 7488 | 3088 |
| 0x55B760 | `treaty_off` | `void (int, int, int)` | 192 | 9600 | 976 |
| 0x55B820 | `agenda_off` | `void (int, int, int)` | 80 | 9792 | 784 |
| 0x55B870 | `treaty_on` | `void (int, int, int)` | 528 | 9872 | 704 |
| 0x55BA80 | `agenda_on` | `void (int, int, int)` | 176 | 10400 | 176 |
| 0x55BC00 | `spying` | `int (int)` | 128 | 96 | 20176 |
| 0x55BC80 | `wants_to_attack` | `int (int, int, int)` | 1968 | 224 | 20048 |
| 0x55C430 | `comm_check` | `void (int, int)` | 768 | 2192 | 18080 |
| 0x55C730 | `pact_attacks` | `void (int, int)` | 272 | 2960 | 17312 |
| 0x55C840 | `enemies_war` | `void (int, int)` | 1120 | 3232 | 17040 |
| 0x55CCA0 | `pact_unpact` | `void (int, int)` | 384 | 4352 | 15920 |
| 0x55CE20 | `enemies_unpact` | `void (int, int)` | 544 | 4736 | 15536 |
| 0x55D040 | `pact_truce` | `void (int, int)` | 336 | 5280 | 14992 |
| 0x55D190 | `pact_treaty` | `void (int, int)` | 272 | 5616 | 14656 |
| 0x55D2A0 | `enemies_team_up` | `void (int, int, int)` | 400 | 5888 | 14384 |
| 0x55D430 | `enemies_trade_tech` | `void (int, int)` | 816 | 6288 | 13984 |
| 0x55D760 | `enemies_treaty` | `void (int, int, int)` | 3088 | 7104 | 13168 |
| 0x560DD0 | `enemy_capabilities` | `void (int)` | 688 | 128 | 42320 |
| 0x561080 | `enemy_strategy` | `void (int)` | 14352 | 816 | 41632 |
| 0x564890 | `set_course` | `void (int, char, int, int)` | 768 | 15168 | 27280 |
| 0x564B90 | `assemble_passengers` | `void (int, int, int)` | 512 | 15936 | 26512 |
| 0x564D90 | `can_convoy` | `int (int, int)` | 288 | 16448 | 26000 |
| 0x564EB0 | `good_sensor` | `int (int, int, int)` | 1136 | 16736 | 25712 |
| 0x565320 | `can_terraform` | `int (int, int, int, int, int)` | 3072 | 17872 | 24576 |
| 0x565F20 | `compute_odds` | `int (int, int, int, int, int)` | 1712 | 20944 | 21504 |
| 0x5A9280 | `map_data` | `int (FILE*, int, int)` | 624 | 11344 | 1104 |
| 0x5A94F0 | `save_daemon` | `int (char*)` | 480 | 11968 | 480 |
| 0x5A9760 | `load_daemon` | `int (char*, int)` | 960 | 144 | 9648 |
| 0x5A9B20 | `save_map_daemon` | `int (char*)` | 240 | 1104 | 8688 |
| 0x5A9C10 | `load_map_daemon` | `int (char*)` | 416 | 1344 | 8448 |
| 0x5A9DB0 | `yearmotize` | `void (char*)` | 256 | 1760 | 8032 |
| 0x5A9EB0 | `save_game` | `int (int)` | 3072 | 2016 | 7776 |
| 0x5B0420 | `balance` | `int ()` | 1552 | 5840 | 2384 |
| 0x5B0A30 | `scenario_setup` | `int ()` | 832 | 7392 | 832 |
| 0x5B0E00 | `setup_player` | `int (int, int, int)` | 9600 | 144 | 10960 |
| 0x5B4600 | `social_set` | `void (int)` | 304 | 176 | 304 |
| 0x5B4790 | `social_ai` | `void (int, int, int, int, int, CSocialCategory*)` | 3728 | 96 | 3840 |

### PROBE (4 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x50C2E0 | `act_of_aggression` | `int (int, int)` | 96 | 2224 | 464 |
| 0x50C340 | `steal_tech` | `int (int, int, int)` | 368 | 2320 | 368 |
| 0x50C510 | `capture_base` | `void (int, int, int)` | 6272 | 96 | 6320 |
| 0x59F120 | `probe` | `int (int, int, int, int)` | 26464 | 576 | 26464 |

### TIME (135 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x50EE30 | `blink2_timer` | `void (int)` | 80 | 2160 | 1440 |
| 0x50EE80 | `line_timer` | `void (int)` | 144 | 2240 | 1360 |
| 0x50EF10 | `turn_timer` | `void (int unused)` | 64 | 2384 | 1216 |
| 0x50EF50 | `go_reset` | `int ()` | 544 | 2448 | 1152 |
| 0x50F170 | `go_timer` | `void (int)` | 608 | 2992 | 608 |
| 0x50F460 | `Console_ctor` | `DWORD (Console* This, int a2)` | 496 | 32 | 112848 |
| 0x50F650 | `Console_clear_group` | `int (Console* This)` | 48 | 528 | 112352 |
| 0x50F680 | `Console_on_nc_hittest` | `void (Console* This, int a2, int a3)` | 736 | 576 | 112304 |
| 0x50F960 | `Console_on_post_redraw_nc_buffer` | `void (Console* This, Buffer* a2, int a3)` | 432 | 1312 | 111568 |
| 0x50FB10 | `Console_on_nc_left_down` | `void (Console* This, int a2, int a3)` | 864 | 1744 | 111136 |
| 0x50FE70 | `Console_on_nc_left_up` | `void (Console* This, int a2, int a3)` | 672 | 2608 | 110272 |
| 0x510110 | `Console_on_nc_mouse_move` | `void (Console* This, int a2, int a3)` | 752 | 3280 | 109600 |
| 0x510400 | `Console_cursor_on` | `void (Console* This, int a2)` | 144 | 4032 | 108848 |
| 0x510490 | `Console_cursor_on_2` | `void (Console* This, int a2, int a3)` | 144 | 4176 | 108704 |
| 0x510520 | `Console_cursor_off` | `int (Console* This)` | 160 | 4320 | 108560 |
| 0x5105C0 | `Console_selecting` | `int (Console* This, int x, int y)` | 544 | 4480 | 108400 |
| 0x5107E0 | `Console_selected` | `int (Console* This)` | 192 | 5024 | 107856 |
| 0x5108A0 | `Console_focus` | `int (Console* This, int x, int y, int faction_id)` | 272 | 5216 | 107664 |
| 0x5109B0 | `Console_cursor_next` | `void (Console* This, int a2, int a3)` | 176 | 5488 | 107392 |
| 0x510A60 | `Console_prev_cursor` | `int (Console* This)` | 272 | 5664 | 107216 |
| 0x510B70 | `Console_next_cursor` | `int (Console* This)` | 368 | 5936 | 106944 |
| 0x510CE0 | `Console_ready_search` | `int (Console* This, int a2)` | 624 | 6304 | 106576 |
| 0x510F50 | `Console_ready` | `int (Console* This)` | 1328 | 6928 | 105952 |
| 0x511480 | `Console_set_view` | `void (Console* This, int a2)` | 160 | 8256 | 104624 |
| 0x511520 | `Console_set_move` | `void (Console* This, int a2)` | 736 | 8416 | 104464 |
| 0x511800 | `Console_turn_hints` | `int (Console* This)` | 944 | 9152 | 103728 |
| 0x511BB0 | `Console_give_hints` | `int (Console* This)` | 8160 | 10096 | 102784 |
| 0x513B90 | `Console_veh_turn` | `int (Console* This)` | 1072 | 18256 | 94624 |
| 0x513FC0 | `Console_end_of_turn` | `int (Console* This)` | 144 | 19328 | 93552 |
| 0x514050 | `Console_human_turn` | `int (Console* This)` | 1936 | 19472 | 93408 |
| 0x5147E0 | `Console_set_cursor` | `void (Console* This, int x, int y)` | 160 | 21408 | 91472 |
| 0x514880 | `Console_update_data` | `void (Console* This, int a2)` | 48 | 21568 | 91312 |
| 0x5148B0 | `Console_move_cursor` | `void (Console* This, int a2)` | 224 | 21616 | 91264 |
| 0x514990 | `Console_quit_game` | `int (Console* This)` | 416 | 21840 | 91040 |
| 0x514B30 | `Console_retire_game` | `int (Console* This)` | 512 | 22256 | 90624 |
| 0x514D30 | `Console_new_game` | `int (Console* This)` | 448 | 22768 | 90112 |
| 0x514EF0 | `Console_set_preferences` | `int (Console* This)` | 16 | 23216 | 89664 |
| 0x514F00 | `Console_set_adv_preferences` | `int (Console* This)` | 16 | 23232 | 89648 |
| 0x514F10 | `Console_set_auto_preferences` | `int (Console* This)` | 16 | 23248 | 89632 |
| 0x514F20 | `Console_set_base_preferences` | `int (Console* This)` | 16 | 23264 | 89616 |
| 0x514F30 | `Console_set_audiovisual` | `int (Console* This)` | 16 | 23280 | 89600 |
| 0x514F40 | `Console_set_map_display` | `int (Console* This)` | 16 | 23296 | 89584 |
| 0x514F50 | `Console_set_ai` | `void (Console* This, int faction_id)` | 192 | 23312 | 89568 |
| 0x515010 | `Console_set_tech` | `void (Console* This, int faction_id)` | 192 | 23504 | 89376 |
| 0x5150D0 | `Console_zoom` | `void (Console* This, int a2, int a3)` | 816 | 23696 | 89184 |
| 0x515400 | `Console_center` | `int (Console* This)` | 80 | 24512 | 88368 |
| 0x515450 | `Console_survey` | `int (Console* This)` | 224 | 24592 | 88288 |
| 0x515530 | `Console_survey2` | `int (Console* This)` | 208 | 24816 | 88064 |
| 0x515600 | `Console_base_names` | `int (Console* This)` | 64 | 25024 | 87856 |
| 0x515640 | `Console_toggle_flatten` | `int (Console* This)` | 96 | 25088 | 87792 |
| 0x5156A0 | `Console_toggle_grid` | `int (Console* This)` | 80 | 25184 | 87696 |
| 0x5156F0 | `Console_toggle_base_grid` | `int (Console* This)` | 80 | 25264 | 87616 |
| 0x515740 | `Console_toggle_dest` | `int (Console* This)` | 48 | 25344 | 87536 |
| 0x515770 | `Console_chat` | `void (Console* This, int a2)` | 400 | 25392 | 87488 |
| 0x515900 | `Console_time_controls` | `int (Console* This)` | 1520 | 25792 | 87088 |
| 0x515EF0 | `Console_toggle_patrol` | `int (Console* This)` | 128 | 27312 | 85568 |
| 0x515F70 | `Console_show_movies` | `int (Console* This)` | 816 | 27440 | 85440 |
| 0x5162A0 | `Console_name_landmark` | `void (Console* This, int x, int y)` | 496 | 28256 | 84624 |
| 0x516490 | `Console_erase_landmark` | `void (Console* This, int x, int y)` | 288 | 28752 | 84128 |
| 0x5165B0 | `Console_zoom_to_upkeep_base` | `int (Console* This)` | 288 | 29040 | 83840 |
| 0x5166D0 | `Console_cursor_key` | `void (Console* This, int a2, int a3, int a4)` | 608 | 29328 | 83552 |
| 0x516930 | `Console_menu_detail` | `int (Console* This)` | 64 | 29936 | 82944 |
| 0x516970 | `Console_menu_simple` | `int (Console* This)` | 64 | 30000 | 82880 |
| 0x5169B0 | `Console_base_me` | `int (Console* This)` | 64 | 30064 | 82816 |
| 0x5169F0 | `Console_end_my_turn` | `int (Console* This)` | 608 | 30128 | 82752 |
| 0x516C50 | `Console_show_scen_objectives` | `int (Console* This)` | 336 | 30736 | 82144 |
| 0x516DA0 | `Console_governor_directives` | `int (Console* This)` | 704 | 31072 | 81808 |
| 0x517060 | `Console_use_time_bonus` | `int (Console* This)` | 288 | 31776 | 81104 |
| 0x517180 | `Console_show_version` | `int (Console* This)` | 80 | 32064 | 80816 |
| 0x5171D0 | `Console_veh_key` | `void (Console* This, int a2, int a3, int a4)` | 1776 | 32144 | 80736 |
| 0x5178C0 | `Console_on_key_click` | `int (Console* This, int a2, int a3)` | 17120 | 33920 | 78960 |
| 0x51BBA0 | `Console_load_proc` | `void (Console* This, int a2, int a3)` | 208 | 51040 | 61840 |
| 0x51BC70 | `Console_menu_proc` | `void (Console* This, int a2)` | 6832 | 51248 | 61632 |
| 0x5FC700 | `wait_task` | `int ()` | 240 | 6912 | 3184 |
| 0x5FC7F0 | `do_task` | `int ()` | 256 | 7152 | 2944 |
| 0x5FC8F0 | `do_all_tasks` | `int ()` | 256 | 7408 | 2688 |
| 0x5FC9F0 | `do_all_tasks_2` | `void (Win*)` | 64 | 7664 | 2432 |
| 0x5FCA30 | `do_non_input` | `int ()` | 240 | 7728 | 2368 |
| 0x5FCB20 | `do_all_non_input` | `int ()` | 64 | 7968 | 2128 |
| 0x5FCB60 | `do_draw` | `int ()` | 80 | 8032 | 2064 |
| 0x5FCBB0 | `do_all_draws` | `int ()` | 112 | 8112 | 1984 |
| 0x5FCC20 | `do_keyboard` | `int ()` | 80 | 8224 | 1872 |
| 0x5FCC70 | `do_all_keyboard` | `int ()` | 800 | 8304 | 1792 |
| 0x5FCF90 | `do_char` | `int ()` | 80 | 9104 | 992 |
| 0x5FCFE0 | `do_all_chars` | `int ()` | 112 | 9184 | 912 |
| 0x5FD050 | `do_mouse` | `int ()` | 80 | 9296 | 800 |
| 0x5FD0A0 | `do_all_mouse` | `int ()` | 128 | 9376 | 720 |
| 0x5FD120 | `flush_input` | `int ()` | 160 | 9504 | 592 |
| 0x5FD1C0 | `flush_keyboard` | `int ()` | 96 | 9664 | 432 |
| 0x5FD220 | `flush_mouse` | `int ()` | 144 | 9760 | 336 |
| 0x5FD2B0 | `do_sound` | `int ()` | 176 | 9904 | 192 |
| 0x5FD360 | `do_all_sounds` | `int ()` | 16 | 10080 | 16 |
| 0x60FF00 | `SpriteBox_ctor` | `DWORD (SpriteBox* This, int a2)` | 544 | 29440 | 25296 |
| 0x610120 | `SpriteBox_dtor` | `int (SpriteBox* This)` | 352 | 29984 | 24752 |
| 0x610280 | `SpriteBox_close` | `int (SpriteBox* This)` | 416 | 30336 | 24400 |
| 0x610420 | `SpriteBox_init` | `int (SpriteBox* This, int a2)` | 32 | 30752 | 23984 |
| 0x610440 | `SpriteBox_init_2` | `int (SpriteBox* This, int a2, int a3, int a4, int a5, int a6)` | 64 | 30784 | 23952 |
| 0x610480 | `SpriteBox_init_3` | `int (SpriteBox* This, RECT* a2, int a3)` | 48 | 30848 | 23888 |
| 0x6104B0 | `SpriteBox_init_4` | `int (SpriteBox* This, Heap* a2)` | 32 | 30896 | 23840 |
| 0x6104D0 | `SpriteBox_init_5` | `int (SpriteBox* This, int a2, int a3, int a4, int a5, Heap* a6)` | 64 | 30928 | 23808 |
| 0x610510 | `SpriteBox_init_6` | `int (SpriteBox* This, RECT* a2, Heap* a3)` | 48 | 30992 | 23744 |
| 0x610540 | `SpriteBox_attach` | `int (SpriteBox* This, GraphicWin* a2, int a3, int a4, int a5)` | 928 | 31040 | 23696 |
| 0x6108E0 | `SpriteBox_sprite` | `int (SpriteBox* This, Sprite* a2, char* a3, int a4)` | 144 | 31968 | 22768 |
| 0x610970 | `SpriteBox_calculate_dimensions` | `int (SpriteBox* This)` | 736 | 32112 | 22624 |
| 0x610C50 | `SpriteBox_draw_item` | `void (SpriteBox* This, int a2)` | 1008 | 32848 | 21888 |
| 0x611040 | `SpriteBox_on_mouse_move` | `void (SpriteBox* This, int a2, int a3)` | 16 | 33856 | 20880 |
| 0x611050 | `SpriteBox_on_mouse_leave` | `void (SpriteBox* This, int a2, int a3)` | 16 | 33872 | 20864 |
| 0x611060 | `SpriteBox_on_left_click` | `void (SpriteBox* This, int x, int y)` | 240 | 33888 | 20848 |
| 0x611150 | `SpriteBox_on_left_down` | `void (SpriteBox* This, int x, int y)` | 80 | 34128 | 20608 |
| 0x6111A0 | `SpriteBox_on_left_up` | `void (SpriteBox* This, int x, int y)` | 80 | 34208 | 20528 |
| 0x6111F0 | `SpriteBox_on_right_click` | `void (SpriteBox* This, int x, int y)` | 80 | 34288 | 20448 |
| 0x611240 | `SpriteBox_on_right_down` | `void (SpriteBox* This, int x, int y)` | 80 | 34368 | 20368 |
| 0x611290 | `SpriteBox_on_right_up` | `void (SpriteBox* This, int x, int y)` | 80 | 34448 | 20288 |
| 0x6112E0 | `SpriteBox_on_left_double_click` | `void (SpriteBox* This, int x, int y)` | 80 | 34528 | 20208 |
| 0x611330 | `SpriteBox_on_right_double_click` | `void (SpriteBox* This, int x, int y)` | 80 | 34608 | 20128 |
| 0x611380 | `SpriteBox_on_redraw` | `int (SpriteBox* This)` | 128 | 34688 | 20048 |
| 0x611400 | `SpriteBox_on_key_down` | `int (SpriteBox* This, int a2)` | 480 | 34816 | 19920 |
| 0x6115E0 | `SpriteBox_on_dialog_focus` | `void (SpriteBox* This, int a2)` | 32 | 35296 | 19440 |
| 0x611600 | `SpriteBox_id_to_pos` | `void (SpriteBox* This, int a2)` | 64 | 35328 | 19408 |
| 0x611640 | `SpriteBox_pos_to_id` | `void (SpriteBox* This, int a2)` | 768 | 35392 | 19344 |
| 0x611940 | `EditGroup_ctor` | `DWORD (EditGroup* This, int a2)` | 224 | 36160 | 18576 |
| 0x611A20 | `EditGroup_dtor` | `int (EditGroup* This)` | 112 | 36384 | 18352 |
| 0x611A90 | `EditGroup_close` | `int (EditGroup* This)` | 96 | 36496 | 18240 |
| 0x611AF0 | `EditGroup_attach` | `int (EditGroup* This, GraphicWin* a2, int a3, int a4, int a5)` | 1168 | 36592 | 18144 |
| 0x611F80 | `EditGroup_edit` | `int (EditGroup* This, char* a2, char* a3, int a4)` | 192 | 37760 | 16976 |
| 0x612040 | `EditGroup_set_text` | `void (EditGroup* This, char* a2, int a3)` | 32 | 37952 | 16784 |
| 0x612060 | `EditGroup_get_text` | `char* (EditGroup* This, int a2)` | 32 | 37984 | 16752 |
| 0x612080 | `EditGroup_append_text` | `void (EditGroup* This, char* a2, int a3)` | 48 | 38016 | 16720 |
| 0x6120B0 | `EditGroup_set_text_limits` | `void (EditGroup* This, int a2)` | 32 | 38064 | 16672 |
| 0x6120D0 | `EditGroup_set_default` | `int (EditGroup* This, char* a2, int a3)` | 128 | 38096 | 16640 |
| 0x612150 | `EditGroup_calculate_dimensions` | `int (EditGroup* This)` | 768 | 38224 | 16512 |
| 0x612450 | `EditGroup_on_redraw` | `int (EditGroup* This)` | 544 | 38992 | 15744 |
| 0x612670 | `EditGroup_on_dialog_focus` | `void (EditGroup* This, int a2)` | 80 | 39536 | 15200 |
| 0x6126C0 | `EditGroup_pass_dialog_focus` | `int (EditGroup* This)` | 944 | 39616 | 15120 |
| 0x612A70 | `Dialogs_item` | `int (Dialogs* This, const char* text, int position)` | 14176 | 40560 | 14176 |

### COUNCIL (63 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x51D720 | `Console_menu_daemon` | `void (int a1)` | 32 | 58080 | 54800 |
| 0x51D740 | `Console_menu_update` | `int (Console* This)` | 128 | 58112 | 54768 |
| 0x51D7C0 | `Console_on_sys_close` | `int (Console* This)` | 16 | 58240 | 54640 |
| 0x51D7D0 | `Console_close` | `int (Console* This)` | 192 | 58256 | 54624 |
| 0x51D890 | `Console_init` | `void (Console* This, int a2)` | 352 | 58448 | 54432 |
| 0x51D9F0 | `Console_dtor` | `int (Console* This)` | 544 | 58800 | 54080 |
| 0x51DC10 | `AlphaMenu_dtor` | `int (Menu* This)` | 128 | 59344 | 53536 |
| 0x51DC90 | `AlphaMenu_scalar_dtor` | `void* (Menu* This, uint32_t flags)` | 112 | 59472 | 53408 |
| 0x51DD00 | `NetDaemon_cleanup` | `int (AlphaNet* This)` | 624 | 59584 | 53296 |
| 0x51DF70 | `AlphaMenu_operator_delete` | `void (void* a1, uint32_t a2)` | 144 | 60208 | 52672 |
| 0x51E000 | `read_tamper` | `int ()` | 144 | 60352 | 52528 |
| 0x51E090 | `set_tamper` | `int ()` | 128 | 60496 | 52384 |
| 0x51E110 | `check_tamper` | `int ()` | 640 | 60624 | 52256 |
| 0x51E390 | `clear_tamper` | `int ()` | 48 | 61264 | 51616 |
| 0x51E3C0 | `clear_all_player_messages` | `int ()` | 928 | 61312 | 51568 |
| 0x51E760 | `do_checksums` | `void (int)` | 2400 | 62240 | 50640 |
| 0x51F0C0 | `help_spock` | `int ()` | 240 | 64640 | 48240 |
| 0x51F1B0 | `check_spock` | `int ()` | 80 | 64880 | 48000 |
| 0x51F200 | `wait_loop` | `int ()` | 192 | 64960 | 47920 |
| 0x51F2C0 | `wait_2` | `int ()` | 32 | 65152 | 47728 |
| 0x51F2E0 | `random_events` | `void (int)` | 11936 | 65184 | 47696 |
| 0x522180 | `alien_fauna` | `int ()` | 3456 | 77120 | 35760 |
| 0x522F00 | `do_fungal_towers` | `int ()` | 480 | 80576 | 32304 |
| 0x5230E0 | `interlude` | `void (int event_id, const char* text, int a3, int a4)` | 2672 | 81056 | 31824 |
| 0x523B50 | `ascent` | `int ()` | 272 | 83728 | 29152 |
| 0x523C60 | `set_time_controls` | `int ()` | 368 | 84000 | 28880 |
| 0x523DD0 | `reset_territory` | `int ()` | 1088 | 84368 | 28512 |
| 0x524210 | `climactic_changes` | `int ()` | 1440 | 85456 | 27424 |
| 0x5247B0 | `generators` | `int (int, int*)` | 192 | 86896 | 25984 |
| 0x524870 | `end_of_game` | `int (int)` | 4176 | 87088 | 25792 |
| 0x5258C0 | `turn_upkeep` | `int ()` | 1904 | 91264 | 21616 |
| 0x526030 | `repair_phase` | `void (int)` | 1920 | 93168 | 19712 |
| 0x5267B0 | `allocate_energy` | `void (int)` | 1728 | 95088 | 17792 |
| 0x526E70 | `production_phase` | `void (int)` | 1056 | 96816 | 16064 |
| 0x527290 | `faction_upkeep` | `void (int)` | 800 | 97872 | 15008 |
| 0x5275B0 | `control_turn` | `int ()` | 6384 | 98672 | 14208 |
| 0x528EA0 | `net_upkeep_phase` | `int (int)` | 512 | 105056 | 7824 |
| 0x5290A0 | `net_upkeep` | `int ()` | 2912 | 105568 | 7312 |
| 0x529C00 | `mash_planes` | `int ()` | 624 | 108480 | 4400 |
| 0x529E70 | `net_not_my_turn` | `int ()` | 192 | 109104 | 3776 |
| 0x529F30 | `net_end_of_turn` | `int ()` | 944 | 109296 | 3584 |
| 0x52A2E0 | `net_control_turn` | `int ()` | 1872 | 110240 | 2640 |
| 0x52AA30 | `control_game` | `int ()` | 768 | 112112 | 768 |
| 0x52AEB0 | `wants_prop` | `int (int, int, int)` | 4016 | 144 | 31712 |
| 0x52BE60 | `council_get_vote` | `int (int, int, int)` | 256 | 4160 | 27696 |
| 0x52BF60 | `council_action` | `void (int, int)` | 1808 | 4416 | 27440 |
| 0x52C670 | `can_call_council` | `int (int, int)` | 528 | 6224 | 25632 |
| 0x52C880 | `call_council` | `void (int)` | 4896 | 6752 | 25104 |
| 0x52DBA0 | `current_server` | `int ()` | 80 | 11648 | 20208 |
| 0x52DBF0 | `next_player_turn` | `int ()` | 128 | 11728 | 20128 |
| 0x52DC70 | `not_my_turn` | `int ()` | 48 | 11856 | 20000 |
| 0x52DCA0 | `DeletionList_ctor` | `int (DeletionList* This)` | 48 | 11904 | 19952 |
| 0x52DCD0 | `DeletionList_clear` | `void (DeletionList* This, int a2)` | 32 | 11952 | 19904 |
| 0x52DCF0 | `DeletionList_UNK1` | `int (DeletionList* This)` | 32 | 11984 | 19872 |
| 0x52DD10 | `DeletionList_UNK2` | `int (DeletionList* This, int a2)` | 64 | 12016 | 19840 |
| 0x52DD50 | `DeletionList_ack` | `void (DeletionList* This, int a2, int a3)` | 112 | 12080 | 19776 |
| 0x52DDC0 | `DeletionList_convert` | `int (DeletionList* This, int a2, int a3)` | 176 | 12192 | 19664 |
| 0x52DE70 | `DeletionList_add_deletion` | `int (DeletionList* This, int veh_id)` | 192 | 12368 | 19488 |
| 0x52DF30 | `NetDaemon_init` | `int (AlphaNet* This, int a2)` | 288 | 12560 | 19296 |
| 0x52E050 | `NetDaemon_hang_up` | `int (AlphaNet* This)` | 272 | 12848 | 19008 |
| 0x52E160 | `NetDaemon_on_player_added` | `void (AlphaNet* This, char* lpszInput, uint32_t a3)` | 528 | 13120 | 18736 |
| 0x52E370 | `NetDaemon_on_player_deleted` | `void (AlphaNet* This, char* a2, uint32_t a3)` | 1424 | 13648 | 18208 |
| 0x52E900 | `NetDaemon_send_game` | `int (AlphaNet* This)` | 1088 | 15072 | 16784 |

### TECHNOLOGY (18 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x5589E0 | `commlink_attempter` | `int (int, int)` | 640 | 55872 | 1984 |
| 0x558C60 | `commlink_attempt` | `void (int)` | 864 | 56512 | 1344 |
| 0x558FC0 | `BasePop_exec` | `int (BasePop* This, int a2)` | 32 | 57376 | 480 |
| 0x558FE0 | `BasePop_item` | `int (BasePop* This, char* lpString, int position)` | 96 | 57408 | 448 |
| 0x559040 | `Popup_start_3` | `void (Popup* This, const char* label, int a3)` | 32 | 57504 | 352 |
| 0x559060 | `get_name` | `char* (int)` | 64 | 57536 | 320 |
| 0x5590A0 | `get_title` | `char* (int)` | 64 | 57600 | 256 |
| 0x5590E0 | `get_adjective_leader` | `char* (int)` | 32 | 57664 | 192 |
| 0x559100 | `get_adjective_insult_leader` | `char* (int)` | 32 | 57696 | 160 |
| 0x559120 | `get_adjective_faction` | `char* (int)` | 32 | 57728 | 128 |
| 0x559140 | `get_adjective_insult_faction` | `char* (int)` | 32 | 57760 | 96 |
| 0x559160 | `get_insult_leader` | `char* (int)` | 64 | 57792 | 64 |
| 0x57CFA0 | `study_artifact` | `int (int)` | 592 | 336 | 592 |
| 0x5BADC0 | `tech_secret` | `void (int, int)` | 160 | 416 | 160 |
| 0x5BB000 | `tech_achieved` | `void (int, int, int, int)` | 7008 | 416 | 7008 |
| 0x5BDE20 | `tech_wizard_unk1` | `void (Popup*, Popup*, int, int, int)` | 736 | 48 | 736 |
| 0x5BE530 | `tech_advance` | `int (int)` | 384 | 1072 | 384 |
| 0x5BE940 | `tech_research` | `void (int, int)` | 560 | 656 | 1744 |

### ALPHA (44 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x583CB0 | `design_new_veh` | `void (int, int)` | 32 | 11072 | 4272 |
| 0x583CD0 | `hq_list_draw` | `void (GraphicWin*, int, int, int, int, int)` | 352 | 11104 | 4240 |
| 0x583E30 | `design_workshop` | `int (int, int)` | 3360 | 11456 | 3888 |
| 0x584B50 | `abil_cond` | `int (int, int, int, int)` | 528 | 14816 | 528 |
| 0x59D3B0 | `popm` | `int (const char* filename, const char* label, int x, int y, fp_none fn)` | 736 | 3104 | 1488 |
| 0x59D690 | `popm_2` | `int (char*, int, int, fp_none)` | 752 | 3840 | 752 |
| 0x5A58B0 | `space` | `int ()` | 48 | 48 | 48 |
| 0x626D90 | `scan_out_line_poly_dest` | `uint32_t (int, int)` | 128 | 2368 | 880 |
| 0x626E10 | `polygon_dest` | `int (Buffer*, Vert*, int, uint32_t*)` | 752 | 2496 | 752 |
| 0x627130 | `pop` | `int (const char*, fp_none)` | 48 | 48 | 4688 |
| 0x627160 | `pop_2` | `int (const char*, const char*, fp_none)` | 48 | 96 | 4640 |
| 0x627190 | `pop_3` | `int (const char*, const char*, const char*, int, fp_none)` | 64 | 144 | 4592 |
| 0x6271D0 | `pop_4` | `int (const char*, int, int, fp_none)` | 48 | 208 | 4528 |
| 0x627200 | `pop_5` | `int (const char*, const char*, int, int, fp_none)` | 48 | 256 | 4480 |
| 0x627230 | `pop_6` | `int (const char*, int, fp_none)` | 48 | 304 | 4432 |
| 0x627260 | `pop_7` | `int (const char*, const char*, int, fp_none)` | 48 | 352 | 4384 |
| 0x627290 | `pop_8` | `int (const char*, int, char*, int, fp_none)` | 48 | 400 | 4336 |
| 0x6272C0 | `pop_9` | `int (const char*, const char*, int, const char*, int, fp_none)` | 48 | 448 | 4288 |
| 0x6272F0 | `pop_close` | `int ()` | 32 | 496 | 4240 |
| 0x627310 | `pops` | `int (const char*, Sprite*, fp_none)` | 48 | 528 | 4208 |
| 0x627340 | `pops_2` | `int (const char*, Sprite*, int, int, fp_none)` | 48 | 576 | 4160 |
| 0x627370 | `pops_3` | `int (const char*, const char*, Sprite*, fp_none)` | 48 | 624 | 4112 |
| 0x6273A0 | `pops_4` | `int (const char*, const char*, Sprite*, int, int, fp_none)` | 48 | 672 | 4064 |
| 0x6273D0 | `pops_5` | `int (const char*, int, int, Sprite*, fp_none)` | 48 | 720 | 4016 |
| 0x627400 | `pops_6` | `int (const char*, int, int, Sprite*, int, int, fp_none)` | 64 | 768 | 3968 |
| 0x627440 | `pops_7` | `int (const char*, const char*, const char*, int, Sprite*, fp_none)` | 64 | 832 | 3904 |
| 0x627480 | `pops_8` | `int (const char*, const char*, const char*, int, Sprite*, int, int, fp_none)` | 80 | 896 | 3840 |
| 0x6274D0 | `pops_9` | `int (const char*, const char*, int, int, Sprite*, fp_none)` | 48 | 976 | 3760 |
| 0x627500 | `pops_10` | `int (const char*, const char*, int, int, Sprite*, int, int, fp_none)` | 64 | 1024 | 3712 |
| 0x627540 | `pops_11` | `int (const char*, int, Sprite*, fp_none)` | 48 | 1088 | 3648 |
| 0x627570 | `pops_12` | `int (const char*, int, Sprite*, int, int, fp_none)` | 48 | 1136 | 3600 |
| 0x6275A0 | `pops_13` | `int (const char*, const char*, int, Sprite*, fp_none)` | 48 | 1184 | 3552 |
| 0x6275D0 | `pops_14` | `int (const char*, const char*, int, Sprite*, int, int, fp_none)` | 48 | 1232 | 3504 |
| 0x627600 | `pops_15` | `int (const char*, int, char*, int, Sprite*, fp_none)` | 48 | 1280 | 3456 |
| 0x627630 | `pops_16` | `int (const char*, int, char*, int, Sprite*, int, int, fp_none)` | 64 | 1328 | 3408 |
| 0x627670 | `pops_17` | `int (const char*, const char*, int, const char*, int, Sprite*, fp_none)` | 48 | 1392 | 3344 |
| 0x6276A0 | `pops_18` | `int (const char*, const char*, int, const char*, int, Sprite*, int, int, fp_none)` | 352 | 1440 | 3296 |
| 0x627800 | `pop_ask` | `int (const char*, int, char*, fp_none, int)` | 48 | 1792 | 2944 |
| 0x627830 | `pop_ask_2` | `int (const char*, const char*, int, fp_none, int)` | 48 | 1840 | 2896 |
| 0x627860 | `pop_ask_3` | `int (const char*, int, fp_none, int)` | 48 | 1888 | 2848 |
| 0x627890 | `pop_ask_4` | `int (const char*, const char*, const char*, fp_none, int)` | 64 | 1936 | 2800 |
| 0x6278D0 | `pop_ask_5` | `int (const char*, const char*, fp_none, int)` | 64 | 2000 | 2736 |
| 0x627910 | `pop_ask_6` | `int (const char*, const char*, int, const char*, fp_none, int)` | 176 | 2064 | 2672 |
| 0x6279C0 | `pop_ask_number` | `int (const char*, int, fp_none)` | 208 | 2240 | 2496 |

### GAME (27 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x5882F0 | `find_font` | `int (int, int)` | 80 | 3888 | 3728 |
| 0x588340 | `scroll_normal` | `int ()` | 144 | 3968 | 3648 |
| 0x5883D0 | `scroll_small` | `int ()` | 144 | 4112 | 3504 |
| 0x588460 | `popups_normal` | `int ()` | 1152 | 4256 | 3360 |
| 0x5888E0 | `popups_tutorial` | `int ()` | 224 | 5408 | 2208 |
| 0x5889C0 | `popups_medium` | `int ()` | 224 | 5632 | 1984 |
| 0x588AA0 | `config_popups` | `int ()` | 1264 | 5856 | 1760 |
| 0x588F90 | `alien_start` | `int ()` | 496 | 7120 | 496 |
| 0x589330 | `time_controls_dialog` | `void (Popup*)` | 1392 | 432 | 31584 |
| 0x5898A0 | `thumb_routine` | `void (int)` | 400 | 1824 | 30192 |
| 0x589A30 | `change_opening` | `int (char*)` | 240 | 2224 | 29792 |
| 0x589B20 | `close_opening` | `int ()` | 64 | 2464 | 29552 |
| 0x589B60 | `init_opening` | `int (const char*)` | 464 | 2528 | 29488 |
| 0x589D30 | `config_game` | `int (int)` | 9584 | 2992 | 29024 |
| 0x58C2A0 | `custom_planet` | `int (int, int)` | 3120 | 12576 | 19440 |
| 0x58CED0 | `size_of_planet` | `int (int)` | 2016 | 15696 | 16320 |
| 0x5B3380 | `eliminate_player` | `int (int, int)` | 1360 | 9744 | 1360 |
| 0x5B3920 | `setup_game` | `void (int)` | 2288 | 80 | 2288 |
| 0x5C7540 | `world_temple` | `void (int, int)` | 528 | 6512 | 5216 |
| 0x5C7750 | `world_unity` | `void (int, int)` | 816 | 7040 | 4688 |
| 0x5C7A80 | `world_fossil` | `void (int, int)` | 560 | 7856 | 3872 |
| 0x5C7CB0 | `world_canyon` | `void (int, int)` | 656 | 8416 | 3312 |
| 0x5C7F40 | `world_mesa` | `void (int, int)` | 528 | 9072 | 2656 |
| 0x5C8150 | `world_ridge` | `void (int, int)` | 608 | 9600 | 2128 |
| 0x5C83B0 | `world_geothermal` | `void (int, int)` | 640 | 10208 | 1520 |
| 0x5C8630 | `world_landmarks` | `int ()` | 176 | 10848 | 880 |
| 0x5C86E0 | `world_build` | `int ()` | 704 | 11024 | 704 |

### PATH (6 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x59A1B0 | `Paths_atexit_dtor` | `int ()` | 16 | 27008 | 112 |
| 0x59A1C0 | `Paths_dynamic_init` | `int ()` | 96 | 27024 | 96 |
| 0x59A320 | `Path_dtor` | `int (Path* This)` | 80 | 80 | 80 |
| 0x59A530 | `Path_find` | `int (Path* This, int x1, int y1, int x2, int y2, int unit_id, int faction_id, int a8, int a9)` | 5856 | 448 | 7376 |
| 0x59BC10 | `Path_move` | `int (Path* This, int veh_id, int faction_id)` | 1520 | 6304 | 1520 |
| 0x59CCA0 | `Path_sensors` | `int (Path* This, int faction_id, int* px, int* py)` | 1808 | 1296 | 3296 |

### HEAP (168 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x5D4CF0 | `GraphicWin_ctor` | `int (GraphicWin* This)` | 224 | 1648 | 154416 |
| 0x5D4DD0 | `GraphicWin_dtor` | `int (GraphicWin* This)` | 112 | 1872 | 154192 |
| 0x5D4E40 | `GraphicWin_close` | `int (GraphicWin* This)` | 176 | 1984 | 154080 |
| 0x5D4EF0 | `GraphicWin_init` | `int (GraphicWin* This, int a2, int a3, int width, int height, char* a6, int a7, Win* a8, Menu* a9, BorderSizing* a10)` | 432 | 2160 | 153904 |
| 0x5D50A0 | `GraphicWin_init_2` | `int (GraphicWin* This, RECT* a2, char* a3, int a4, Win* a5, Menu* a6, BorderSizing* a7)` | 432 | 2592 | 153472 |
| 0x5D5250 | `GraphicWin_fill` | `void (GraphicWin* This, int a2)` | 256 | 3024 | 153040 |
| 0x5D5350 | `GraphicWin_fill_2` | `int (GraphicWin* This, RECT* rect, int buffer)` | 240 | 3280 | 152784 |
| 0x5D5440 | `GraphicWin_fill_3` | `int (GraphicWin* This, int x, int y, int length, int width, int a6)` | 192 | 3520 | 152544 |
| 0x5D5500 | `GraphicWin_load_pcx` | `int (GraphicWin* This, char* lpszFileName, Palette* a3, int a4, int len)` | 64 | 3712 | 152352 |
| 0x5D5540 | `GraphicWin_resize` | `void (GraphicWin* This, int width, int height, int a4)` | 144 | 3776 | 152288 |
| 0x5D55D0 | `GraphicWin_update` | `void (GraphicWin* This, int a2, int a3, int a4, int a5, GraphicWin* a6)` | 96 | 3920 | 152144 |
| 0x5D5630 | `GraphicWin_update_2` | `void (GraphicWin* This, RECT* a2, GraphicWin* a3)` | 128 | 4016 | 152048 |
| 0x5D56B0 | `GraphicWin_update_3` | `void (GraphicWin* This, GraphicWin* a2)` | 112 | 4144 | 151920 |
| 0x5D5720 | `GraphicWin_soft_update` | `void (GraphicWin* This, int a2, int a3, int a4, int a5)` | 368 | 4256 | 151808 |
| 0x5D5890 | `GraphicWin_soft_update_2` | `int (GraphicWin* This)` | 160 | 4624 | 151440 |
| 0x5D5930 | `GraphicWin_soft_update_3` | `void (GraphicWin* This, RECT* a2)` | 320 | 4784 | 151280 |
| 0x5D5A70 | `GraphicWin_redraw` | `int (GraphicWin* This)` | 640 | 5104 | 150960 |
| 0x5D5CF0 | `GraphicWin_on_paint` | `int (GraphicWin* This, RECT* lprcSrc2)` | 1312 | 5744 | 150320 |
| 0x5D6210 | `GraphicWin_on_size` | `void (GraphicWin* This, uint32_t a2, int width, int height)` | 256 | 7056 | 149008 |
| 0x5D6310 | `GraphicWin_on_mouse_move` | `void (GraphicWin* This, int a2, int a3, uint32_t a4, int a5)` | 32 | 7312 | 148752 |
| 0x5D6330 | `GraphicWin_on_set_cursor` | `int (GraphicWin* This, void* a2, uint32_t a3, uint32_t a4)` | 176 | 7344 | 148720 |
| 0x5D63E0 | `GraphicWin_set_rects` | `int (GraphicWin* This)` | 16 | 7520 | 148544 |
| 0x5D63F0 | `GraphicWin_on_redraw_nc` | `void (GraphicWin* This, RECT* a2, int a3)` | 1632 | 7536 | 148528 |
| 0x5D6A50 | `GraphicWin_set_def_nc_sprites` | `void (NCSprites* a1)` | 112 | 9168 | 146896 |
| 0x5D6AC0 | `GraphicWin_overlay_nonclient` | `void (GraphicWin* This, RECT* lprcSrc2)` | 1168 | 9280 | 146784 |
| 0x5D6F50 | `GraphicWin_load_all_nc_sprites_from_defs` | `int (GraphicWin* This)` | 224 | 10448 | 145616 |
| 0x5D7030 | `GraphicWin_compute_min_size` | `int (GraphicWin* This)` | 304 | 10672 | 145392 |
| 0x5D7160 | `GraphicWin_operator_delete` | `void (void* a1, uint32_t a2)` | 176 | 10976 | 145088 |
| 0x5D7210 | `Buffer_ctor` | `int (Buffer* This)` | 512 | 11152 | 144912 |
| 0x5D7410 | `Buffer_dtor` | `int (Buffer* This)` | 96 | 11664 | 144400 |
| 0x5D7470 | `Buffer_close` | `int (Buffer* This)` | 512 | 11760 | 144304 |
| 0x5D7670 | `Buffer_init` | `int (Buffer* This, int width, int height, int a4, ExtDirectDraw* a5)` | 848 | 12272 | 143792 |
| 0x5D79C0 | `Buffer_resize` | `void (Buffer* This, int width, int height)` | 352 | 13120 | 142944 |
| 0x5D7B20 | `Buffer_load_tga` | `int (Buffer* This, const char* a2)` | 32 | 13472 | 142592 |
| 0x5D7B40 | `Buffer_load_tga_2` | `int (const char* a1, Palette* a2, int a3, int a4)` | 320 | 13504 | 142560 |
| 0x5D7C80 | `Buffer_load_bmp` | `int (Buffer* This, const char* a2)` | 32 | 13824 | 142240 |
| 0x5D7CA0 | `Buffer_load_bmp_2` | `int (const char* a1, Palette* a2, int a3, int a4)` | 320 | 13856 | 142208 |
| 0x5D7DE0 | `Buffer_load_pcx` | `int (Buffer* This, const char* filename, Palette* a3, int a4, int len)` | 544 | 14176 | 141888 |
| 0x5D8000 | `Buffer_set_clip` | `int (Buffer* This, RECT* rect)` | 512 | 14720 | 141344 |
| 0x5D8200 | `Buffer_set_clip_2` | `int (Buffer* This, int x, int y, int length, int width)` | 64 | 15232 | 140832 |
| 0x5D8240 | `Buffer_fill` | `int (Buffer* This, int x, int y, int length, int width, int a6)` | 80 | 15296 | 140768 |
| 0x5D8290 | `Buffer_setup_buff_sprite` | `void (Buffer* This, int a2)` | 224 | 15376 | 140688 |
| 0x5D8370 | `Buffer_draw` | `int (Buffer* This, Buffer* a2, int a3, int x, int y)` | 336 | 15600 | 140464 |
| 0x5D84C0 | `Buffer_draw_2` | `int (Buffer* This, Buffer* a2, int a3, int a4, int a5, int a6, int a7)` | 896 | 15936 | 140128 |
| 0x5D8840 | `Buffer_draw_mono` | `int (Buffer* This, Buffer* a2, int a3, int a4, int a5, int a6)` | 240 | 16832 | 139232 |
| 0x5D8930 | `Buffer_draw_mono_2` | `int (Buffer* This, Buffer* a2, int a3, int a4, int a5, int a6, int a7, int a8)` | 256 | 17072 | 138992 |
| 0x5D8A30 | `Buffer_draw_dest` | `int (Buffer* This, Buffer* a2, int a3, int a4, int a5, int a6)` | 240 | 17328 | 138736 |
| 0x5D8B20 | `Buffer_draw_dest_2` | `int (Buffer* This, Buffer* a2, int a3, int a4, int a5, int a6, int a7, uint8_t* a8)` | 720 | 17568 | 138496 |
| 0x5D8DF0 | `Buffer_draw_dest_source` | `int (Buffer* This, Buffer* a2, int a3, int a4, int a5, uint8_t* a6)` | 240 | 18288 | 137776 |
| 0x5D8EE0 | `Buffer_draw_dest_source_2` | `int (Buffer* This, Buffer* a2, int a3, int a4, int a5, int a6, int a7, uint8_t* a8)` | 1248 | 18528 | 137536 |
| 0x5D93C0 | `Buffer_draw_multi_table_dest` | `int (Buffer* This, Buffer* a2, int a3, int a4, int a5, int a6)` | 240 | 19776 | 136288 |
| 0x5D94B0 | `Buffer_draw_multi_table_dest_2` | `int (Buffer* This, Buffer* a2, int a3, int a4, int a5, int a6, int a7, uint8_t* a8)` | 256 | 20016 | 136048 |
| 0x5D95B0 | `Buffer_copy` | `int (Buffer* This, Buffer* dst, int a3, int a4, int width, int height)` | 48 | 20272 | 135792 |
| 0x5D95E0 | `Buffer_copy_2` | `int (Buffer* This, Buffer* dst, RECT* a3)` | 48 | 20320 | 135744 |
| 0x5D9610 | `Buffer_copy_3` | `int (Buffer* This, Buffer* dst, int xSrc, int ySrc, int wSrc, int hSrc, int xDst, int yDst, int wDst, int hDst)` | 1488 | 20368 | 135696 |
| 0x5D9BE0 | `Buffer_copy_to_window` | `int (Buffer* This, Win* dst, int x1, int y1, int a5, int a6, int a7, int a8)` | 1872 | 21856 | 134208 |
| 0x5DA330 | `Buffer_map_colors` | `int (Buffer* This, int a2, int a3, int a4, int a5, uint8_t* a6)` | 416 | 23728 | 132336 |
| 0x5DA4D0 | `Buffer_map_colors_2` | `int (Buffer* This, RECT* a2, uint8_t* a3)` | 432 | 24144 | 131920 |
| 0x5DA680 | `Buffer_poly` | `int (Buffer* This, Vert* a2, int a3, int a4)` | 64 | 24576 | 131488 |
| 0x5DA6C0 | `Buffer_tile_mask` | `int (Buffer* This, Buffer* a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9)` | 416 | 24640 | 131424 |
| 0x5DA860 | `Buffer_tile` | `int (Buffer* This, Buffer* buffer, int a3, int a4, int a5, int a6, int a7, int a8)` | 400 | 25056 | 131008 |
| 0x5DA9F0 | `Buffer_tile_2` | `int (Buffer* This, Sprite* a2, int a3, int a4, int a5, int a6, int a7, int a8)` | 640 | 25456 | 130608 |
| 0x5DAC70 | `Buffer_set_font` | `int (Buffer* This, Font* a2, Font* a3, Font* a4, Font* a5)` | 64 | 26096 | 129968 |
| 0x5DACB0 | `Buffer_set_text_color` | `void (Buffer* This, int color, int a3, int a4, int a5)` | 48 | 26160 | 129904 |
| 0x5DACE0 | `Buffer_set_text_color2` | `void (Buffer* This, int a2, int a3, int a4, int a5)` | 48 | 26208 | 129856 |
| 0x5DAD10 | `Buffer_set_text_color3` | `void (Buffer* This, int a2, int a3, int a4, int a5)` | 48 | 26256 | 129808 |
| 0x5DAD40 | `Buffer_set_text_color_hyper` | `void (Buffer* This, int a2, int a3, int a4, int a5)` | 48 | 26304 | 129760 |
| 0x5DAD70 | `Buffer_write_strings_height` | `int (Buffer* This, StringList* a2, int a3, int a4)` | 720 | 26352 | 129712 |
| 0x5DB040 | `Buffer_write_strings` | `int (Buffer* This, StringList* a2, int x, int y, int a5, int a6)` | 1344 | 27072 | 128992 |
| 0x5DB580 | `Buffer_write_l` | `int (Buffer* This, Font* a2, char* lpString, int a4, int a5, int a6)` | 224 | 28416 | 127648 |
| 0x5DB660 | `Buffer_write_l_2` | `int (Buffer* This, Font* a2, char* lpString, RECT* a4, int a5)` | 96 | 28640 | 127424 |
| 0x5DB6C0 | `Buffer_write_cent_l` | `int (Buffer* This, Font* a2, char* lpString, int a4, int a5, int a6, int a7)` | 96 | 28736 | 127328 |
| 0x5DB720 | `Buffer_write_cent_l_2` | `int (Buffer* This, Font* a2, char* lpString, RECT* a4, int len)` | 96 | 28832 | 127232 |
| 0x5DB780 | `Buffer_write_cent_l_3` | `int (Buffer* This, Font* a2, char* lpString, int a4, int a5, int a6, int a7, int len)` | 144 | 28928 | 127136 |
| 0x5DB810 | `Buffer_write_cent_l_4` | `int (Buffer* This, Font* a2, char* lpString, int a4, int a5, int a6)` | 96 | 29072 | 126992 |
| 0x5DB870 | `Buffer_write_right_l` | `int (Buffer* This, Font* a2, char* lpString, int a4, int a5, int a6, int a7)` | 96 | 29168 | 126896 |
| 0x5DB8D0 | `Buffer_write_right_l_2` | `int (Buffer* This, Font* a2, char* lpString, int a4, int a5, int a6)` | 96 | 29264 | 126800 |
| 0x5DB930 | `Buffer_write_right_l_3` | `int (Buffer* This, Font* a2, char* lpString, RECT* a4, int a5)` | 96 | 29360 | 126704 |
| 0x5DB990 | `Buffer_wrap` | `int (Buffer* This, Font* a2, char* lpString, RECT* a4)` | 80 | 29456 | 126608 |
| 0x5DB9E0 | `Buffer_wrap_cent` | `int (Buffer* This, Font* a2, char* lpString, int x, int y, int a6)` | 288 | 29536 | 126528 |
| 0x5DBB00 | `Buffer_wrap_cent_2` | `int (Buffer* This, Font* a2, char* lpString, int a4, int a5, int a6, int a7)` | 256 | 29824 | 126240 |
| 0x5DBC00 | `Buffer_wrap_cent_3` | `int (Buffer* This, Font* a2, char* lpString, RECT* a4)` | 80 | 30080 | 125984 |
| 0x5DBC50 | `Buffer_wrap_right` | `int (Buffer* This, Font* a2, char* lpString, int a4, int a5, int a6)` | 96 | 30160 | 125904 |
| 0x5DBCB0 | `Buffer_wrap_right_2` | `int (Buffer* This, Font* a2, char* lpString, int a4)` | 80 | 30256 | 125808 |
| 0x5DBD00 | `Buffer_write_raw_l` | `int (Buffer* This, char* lpString, int x, int y, int len)` | 1488 | 30336 | 125728 |
| 0x5DC2D0 | `get_special` | `int (char*, int*)` | 64 | 31824 | 124240 |
| 0x5DC310 | `get_special_link` | `int (char*, int*)` | 80 | 31888 | 124176 |
| 0x5DC360 | `Buffer_wrap_height` | `int (Buffer* This, char* lpString, int a3)` | 176 | 31968 | 124096 |
| 0x5DC410 | `Buffer_find_line_break_l` | `int (Buffer* This, char* lpString, int* breakLen, int len)` | 896 | 32144 | 123920 |
| 0x5DC790 | `Buffer_text_width` | `int (Buffer* This, char* lpString)` | 48 | 33040 | 123024 |
| 0x5DC7C0 | `Buffer_text_width_2` | `int (Buffer* This, char* lpString, int len)` | 704 | 33088 | 122976 |
| 0x5DCA80 | `Buffer_text_height` | `int (Buffer* This)` | 48 | 33792 | 122272 |
| 0x5DCAB0 | `Buffer_text_line_height` | `int (Buffer* This)` | 48 | 33840 | 122224 |
| 0x5DCAE0 | `Buffer_write_multi_font_raw_l` | `int (Buffer* This, char* lpString, int x, int y, int len)` | 960 | 33888 | 122176 |
| 0x5DCEA0 | `Buffer_write_l_3` | `int (Buffer* This, char* lpString, int x, int y, int len)` | 160 | 34848 | 121216 |
| 0x5DCF40 | `Buffer_write_l_4` | `int (Buffer* This, char* lpString, RECT* a3, int len)` | 224 | 35008 | 121056 |
| 0x5DD020 | `Buffer_write_cent_l_5` | `int (Buffer* This, char* lpString, int x, int y, int a5, int len)` | 192 | 35232 | 120832 |
| 0x5DD0E0 | `Buffer_write_cent_l_6` | `int (Buffer* This, char* lpString, int a3, int a4, int a5, int a6, int len)` | 80 | 35424 | 120640 |
| 0x5DD130 | `Buffer_write_cent_l_7` | `int (Buffer* This, char* lpString, RECT* a3, int len)` | 288 | 35504 | 120560 |
| 0x5DD250 | `Buffer_write_cent_l_8` | `int (Buffer* This, char* lpString, int x, int y, int len)` | 176 | 35792 | 120272 |
| 0x5DD300 | `Buffer_write_right_l_4` | `int (Buffer* This, char* lpString, int x, int y, int a5, int len)` | 176 | 35968 | 120096 |
| 0x5DD3B0 | `Buffer_write_right_l_5` | `int (Buffer* This, char* lpString, int x, int y, int len)` | 160 | 36144 | 119920 |
| 0x5DD450 | `Buffer_write_right_l_6` | `int (Buffer* This, char* lpString, RECT* a3, int len)` | 224 | 36304 | 119760 |
| 0x5DD530 | `Buffer_wrap_2` | `int (Buffer* This, char* lpString, RECT* a3)` | 512 | 36528 | 119536 |
| 0x5DD730 | `Buffer_wrap_3` | `int (Buffer* This, char* lpString, int x, int y, int a5)` | 496 | 37040 | 119024 |
| 0x5DD920 | `Buffer_wrap_cent_4` | `int (Buffer* This, char* lpString, int x, int y, int a5)` | 224 | 37536 | 118528 |
| 0x5DDA00 | `Buffer_wrap_cent_5` | `int (Buffer* This, char* lpString, int a3, int a4, int a5, int a6)` | 176 | 37760 | 118304 |
| 0x5DDAB0 | `Buffer_wrap_cent_6` | `int (Buffer* This, char* lpString, RECT* a3)` | 256 | 37936 | 118128 |
| 0x5DDBB0 | `Buffer_wrap_right_3` | `int (Buffer* This, char* lpString, int a3, int a4, int a5)` | 224 | 38192 | 117872 |
| 0x5DDC90 | `Buffer_wrap_right_4` | `int (Buffer* This, char* lpString, int a3)` | 272 | 38416 | 117648 |
| 0x5DDDA0 | `Buffer_wrap_height_flying` | `int (Buffer* This, char* lpString, int a3)` | 64 | 38688 | 117376 |
| 0x5DDDE0 | `Buffer_wrap_height_flying_2` | `int (Buffer* This, char* lpString)` | 272 | 38752 | 117312 |
| 0x5DDEF0 | `Buffer_wrap_height_flying_flush` | `int (Buffer* This)` | 112 | 39024 | 117040 |
| 0x5DDF60 | `Buffer_wrap_flying` | `int (Buffer* This, char* lpString, int a3, int a4, int a5)` | 80 | 39136 | 116928 |
| 0x5DDFB0 | `Buffer_wrap_flying_2` | `int (Buffer* This, char* lpString)` | 544 | 39216 | 116848 |
| 0x5DE1D0 | `Buffer_wrap_flying_flush` | `int (Buffer* This)` | 944 | 39760 | 116304 |
| 0x5DE580 | `Buffer_change_color` | `int (Buffer* This, int a2, int a3)` | 432 | 40704 | 115360 |
| 0x5DE730 | `Buffer_change_color_2` | `int (Buffer* This, RECT* a2, int a3, int a4)` | 448 | 41136 | 114928 |
| 0x5DE8F0 | `Buffer_sync_to_palette` | `int (Buffer* This, Palette* a2)` | 1696 | 41584 | 114480 |
| 0x5DEF90 | `Buffer_clear_links` | `int (Buffer* This)` | 1504 | 43280 | 112784 |
| 0x5DF570 | `Buffer_init_class` | `int ()` | 16 | 44784 | 111280 |
| 0x5DF580 | `Buffer_close_class` | `void (Buffer* This)` | 16 | 44800 | 111264 |
| 0x5DF590 | `Buffer_write_pcx` | `int (Buffer* This, char* filename)` | 896 | 44816 | 111248 |
| 0x5DF910 | `Buffer_write_bmp` | `int (Buffer* This, char* filename)` | 576 | 45712 | 110352 |
| 0x5DFB50 | `Buffer_fill_2` | `int (Buffer* This, int a2)` | 384 | 46288 | 109776 |
| 0x5DFCD0 | `Buffer_fill_3` | `int (Buffer* This, RECT* rect, int a3)` | 560 | 46672 | 109392 |
| 0x5DFF00 | `Buffer_copy_4` | `int (Buffer* This, Buffer* buffer, int x, int y, int a5, int a6, int width, int height)` | 2768 | 47232 | 108832 |
| 0x5E09D0 | `Buffer_line` | `void (Buffer* This, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10)` | 1776 | 50000 | 106064 |
| 0x5E10C0 | `Buffer_hline` | `void (Buffer* This, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9)` | 656 | 51776 | 104288 |
| 0x5E1350 | `Buffer_vline` | `void (Buffer* This, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9)` | 672 | 52432 | 103632 |
| 0x5E15F0 | `Buffer_line_2` | `void (Buffer* This, int x1, int y1, int x2, int y2, int color)` | 1168 | 53104 | 102960 |
| 0x5E1A80 | `Buffer_hline_2` | `void (Buffer* This, int a2, int a3, int a4, int a5)` | 368 | 54272 | 101792 |
| 0x5E1BF0 | `Buffer_vline_2` | `void (Buffer* This, int a2, int a3, int a4, int a5)` | 384 | 54640 | 101424 |
| 0x5E1D70 | `Buffer_dotted_hline` | `void (Buffer* This, int a2, int a3, int a4, int a5)` | 384 | 55024 | 101040 |
| 0x5E1EF0 | `Buffer_dotted_vline` | `void (Buffer* This, int a2, int a3, int a4, int a5)` | 384 | 55408 | 100656 |
| 0x5E2070 | `Buffer_change_color_3` | `int (Buffer* This, int a2, int a3, int a4, int a5, int a6, int a7)` | 416 | 55792 | 100272 |
| 0x5E2210 | `Buffer_get_pixel` | `int (Buffer* This, int a2, int a3)` | 288 | 56208 | 99856 |
| 0x5E2330 | `Buffer_put_pixel` | `int (Buffer* This, int a2, int a3, int a4)` | 336 | 56496 | 99568 |
| 0x5E2480 | `Buffer_get_pcx_dimensions` | `int (const char* filename, int* a2, int* a3)` | 528 | 56832 | 99232 |
| 0x5E2690 | `Buffer_load_pcx_2` | `int (Buffer* This, uint8_t* a2, uint32_t a3, Palette* a4, int len, int a6)` | 1136 | 57360 | 98704 |
| 0x5E2B00 | `Buffer_copy_mask` | `int (Buffer* This, Buffer* a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9)` | 723 | 58496 | 97568 |
| 0x5E2DD3 | `Buffer_box_sprite` | `int (Buffer* This, RECT* a2, BoxSpriteParams* a3)` | 960 | 59219 | 96845 |
| 0x5E3193 | `Buffer_box` | `int (Buffer* This, int a2, int a3, int a4, int a5, int a6, int a7)` | 112 | 60179 | 95885 |
| 0x5E3203 | `Buffer_box_2` | `int (Buffer* This, RECT* a2, int color_h, int color_v)` | 128 | 60291 | 95773 |
| 0x5E3283 | `Buffer_dotted_box` | `int (Buffer* This, int a2, int a3, int a4, int a5, int a6)` | 112 | 60419 | 95645 |
| 0x5E32F3 | `Buffer_dotted_box_2` | `int (Buffer* This, RECT* a2, int a3)` | 128 | 60531 | 95533 |
| 0x5E3373 | `Buffer_get_data` | `int (Buffer* This)` | 128 | 60659 | 95405 |
| 0x5E33F3 | `Buffer_get_data_2` | `int (Buffer* This, int a2, int a3)` | 176 | 60787 | 95277 |
| 0x5E34A3 | `Buffer_free_data` | `void (Buffer* This, int a2)` | 96 | 60963 | 95101 |
| 0x5E3503 | `Buffer_get_hdc` | `int (Buffer* This)` | 96 | 61059 | 95005 |
| 0x5E3563 | `Buffer_release_hdc` | `void (Buffer* This, int a2)` | 637 | 61155 | 94909 |
| 0x5E37E0 | `Sprite_ctor` | `int (Sprite* This)` | 64 | 61792 | 94272 |
| 0x5E3820 | `Sprite_close` | `int (Sprite* This)` | 112 | 61856 | 94208 |
| 0x5E3890 | `Sprite_init` | `int (Sprite* This, const char* a2, int a3, int a4)` | 208 | 61968 | 94096 |
| 0x5E3960 | `Sprite_extract` | `int (Sprite* This, Buffer* a2, int a3, RECT* a4, void* a5)` | 64 | 62176 | 93888 |
| 0x5E39A0 | `Sprite_extract_2` | `int (Sprite* This, Buffer* a2, int a3, int x, int y, int width, int height, void* a8)` | 1120 | 62240 | 93824 |
| 0x5E3E00 | `Sprite_draw_mono` | `int (Sprite* This, Buffer* a2, int a3, int a4, int a5, int a6, int a7, int a8)` | 80 | 63360 | 92704 |
| 0x5E3E50 | `Sprite_draw_mono_2` | `int (Sprite* This, Buffer* a2, int a3, int a4, int a5, int a6)` | 3210 | 63440 | 92624 |
| 0x5E4ADA | `Sprite_UNK1` | `int (Sprite* This, int a2, int a3, int a4, int a5, int a6, int a7, int a8)` | 16 | 66650 | 89414 |
| 0x5E4AEA | `Sprite_UNK2` | `int (Sprite* This, int a2, int a3, int a4, int a5, int a6)` | 16 | 66666 | 89398 |
| 0x5E4AFA | `Sprite_draw` | `int (Sprite* This, Buffer* a2, int a3, int a4, int a5, int a6, int a7, int a8)` | 80 | 66682 | 89382 |
| 0x5E4B4A | `Sprite_draw_2` | `int (Sprite* This, Buffer* a2, int a3, int a4, int a5, int a6, int a7)` | 80 | 66762 | 89302 |
| 0x5E4B9A | `Sprite_draw_3` | `int (Sprite* This, Buffer* a2, int a3, int a4, int a5)` | 3225 | 66842 | 89222 |
| 0x5E5833 | `Sprite_draw_dest` | `int (Sprite* This, Buffer* a2, int a3, int a4, int a5, uint8_t* a6)` | 3615 | 70067 | 85997 |
| 0x5E6652 | `Sprite_draw_dest_2` | `int (Sprite* This, Buffer* a2, int a3, int a4, int a5, int a6, int a7, uint8_t* a8)` | 80 | 73682 | 82382 |
| 0x5E66A2 | `Sprite_draw_dest_unk1` | `int (Sprite* This, Buffer* a2, int a3, int a4, uint8_t* a5)` | 3370 | 73762 | 82302 |
| 0x5E73CC | `Sprite_draw_dest_unk1_2` | `int (Sprite* This, Buffer* a2, int a3, int a4, int a5, int a6, uint8_t* a7)` | 80 | 77132 | 78932 |
| 0x5E741C | `Sprite_draw_dest_source` | `int (Sprite* This, Buffer* a2, int a3, int a4, int a5, uint8_t* a6)` | 3328 | 77212 | 78852 |

### SPOT (168 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x5E811C | `Sprite_draw_dest_source_2` | `int (Sprite* This, Buffer* a2, int a3, int a4, int a5, int a6, int a7, uint8_t* a8)` | 80 | 80540 | 75524 |
| 0x5E816C | `Sprite_draw_dest_unk2` | `int (Sprite* This, Buffer* a2, int a3, int a4, int a5, int a6, int a7, uint8_t* a8)` | 80 | 80620 | 75444 |
| 0x5E81BC | `Sprite_draw_dest_unk2_2` | `int (Sprite* This, Buffer* a2, int a3, int a4, int a5, uint8_t* a6)` | 3561 | 80700 | 75364 |
| 0x5E8FA5 | `Sprite_draw_multi_table_dest` | `int (Sprite* This, Buffer* a2, int a3, int a4, int a5, int a6, int a7, uint8_t* a8)` | 80 | 84261 | 71803 |
| 0x5E8FF5 | `Sprite_draw_multi_table_dest_2` | `int (Sprite* This, Buffer* a2, int a3, int a4, int a5, uint8_t* a6)` | 3407 | 84341 | 71723 |
| 0x5E9D44 | `Sprite_draw_4` | `int (Sprite* This, Buffer* a2, int a3, int a4, int a5, uint8_t* a6)` | 3515 | 87748 | 68316 |
| 0x5EAAFF | `Sprite_UNK3` | `void (Sprite* This, int a2, int a3)` | 16 | 91263 | 64801 |
| 0x5EAB0F | `Sprite_get_clipped_x_table_expand` | `int (Sprite* This, RECT* a2, RECT* a3, int* a4, int* a5, int* a6, int* a7)` | 176 | 91279 | 64785 |
| 0x5EABBF | `Sprite_get_clipped_y_table_expand` | `int (Sprite* This, RECT* a2, RECT* a3, int* a4, int* a5, int* a6, int* a7)` | 176 | 91455 | 64609 |
| 0x5EAC6F | `Sprite_get_clipped_x_table_shrink` | `int (Sprite* This, RECT* a2, RECT* a3, int* a4, int* a5, int* a6, int* a7)` | 144 | 91631 | 64433 |
| 0x5EACFF | `Sprite_get_clipped_y_table_shrink` | `int (Sprite* This, RECT* a2, RECT* a3, int* a4, int* a5, int* a6, int* a7)` | 144 | 91775 | 64289 |
| 0x5EAD8F | `Sprite_UNK4` | `void (Sprite* This, int a2, int a3)` | 16 | 91919 | 64145 |
| 0x5EAD9F | `Sprite_change_color` | `int (Sprite* This, int a2, int a3)` | 176 | 91935 | 64129 |
| 0x5EAE4F | `Sprite_copy` | `int (Sprite* This, Sprite* a2)` | 240 | 92111 | 63953 |
| 0x5EAF3F | `Sprite_create_blank` | `int (Sprite* This, int a2, int a3, int a4)` | 80 | 92351 | 63713 |
| 0x5EAF8F | `Sprite_save` | `int (Sprite* This, int lpFileName)` | 304 | 92431 | 63633 |
| 0x5EB0BF | `Sprite_load` | `int (Sprite* This, int a2)` | 336 | 92735 | 63329 |
| 0x5EB20F | `Sprite_fill_scaling_table` | `int (Sprite* This)` | 449 | 93071 | 62993 |
| 0x5EB3D0 | `Win_ctor` | `int (Win* This)` | 624 | 93520 | 62544 |
| 0x5EB640 | `Win_close` | `int (Win* This)` | 1616 | 94144 | 61920 |
| 0x5EBC90 | `Win_dtor` | `int (Win* This)` | 240 | 95760 | 60304 |
| 0x5EBD80 | `Win_init` | `int (Win* This, int a2, int a3, int a4, int a5, char* a6, int a7, Win* a8, Menu* a9, BorderSizing* a10)` | 2224 | 96000 | 60064 |
| 0x5EC630 | `Win_init_2` | `int (Win* This, RECT* a2, char* a3, int a4, Win* a5, Menu* a6, BorderSizing* a7)` | 80 | 98224 | 57840 |
| 0x5EC680 | `Win_UNK1` | `int (Win* This, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10)` | 16 | 98304 | 57760 |
| 0x5EC690 | `Win_get_hdc` | `HDC ()` | 96 | 98320 | 57744 |
| 0x5EC6F0 | `Win_release_hdc` | `void ()` | 80 | 98416 | 57648 |
| 0x5EC740 | `Win_set_cursor` | `int (Win* This, Sprite* a2, int a3, int a4)` | 64 | 98496 | 57568 |
| 0x5EC780 | `Win_set_cursor_2` | `int (Win* This, HCURSOR* phCursor)` | 64 | 98560 | 57504 |
| 0x5EC7C0 | `Win_set_cursor_3` | `int (Win* This, int nCursorName)` | 64 | 98624 | 57440 |
| 0x5EC800 | `Win_set_mouse_pos` | `void (Win* This, int x, int y)` | 160 | 98688 | 57376 |
| 0x5EC8A0 | `Win_get_mouse_pos` | `void (Win* This, int* a2, int* a3)` | 192 | 98848 | 57216 |
| 0x5EC960 | `Win_get_lbutton_state` | `int (Win* This)` | 32 | 99040 | 57024 |
| 0x5EC980 | `Win_get_rbutton_state` | `int (Win* This)` | 32 | 99072 | 56992 |
| 0x5EC9A0 | `Win_set_parent_dialog` | `void (Win* This, Win* a2)` | 448 | 99104 | 56960 |
| 0x5ECB60 | `Win_remove_parent_dialog` | `int (Win* This)` | 224 | 99552 | 56512 |
| 0x5ECC40 | `Win_set_dialog_focus` | `void (Win* This, Win* a2)` | 224 | 99776 | 56288 |
| 0x5ECD20 | `Win_pass_dialog_focus` | `int (Win* This)` | 160 | 100000 | 56064 |
| 0x5ECDC0 | `Win_UNK2` | `int (Win* This, int a2)` | 96 | 100160 | 55904 |
| 0x5ECE20 | `Win_is_descendant` | `int (Win* This, Win* a2)` | 96 | 100256 | 55808 |
| 0x5ECE80 | `Win_UNK3` | `int (Win* This, int a2)` | 64 | 100352 | 55712 |
| 0x5ECEC0 | `Win_UNK4` | `int (Win* This)` | 96 | 100416 | 55648 |
| 0x5ECF20 | `Win_screen_to_client` | `void (Win* This, RECT* a2)` | 192 | 100512 | 55552 |
| 0x5ECFE0 | `Win_client_to_screen` | `void (Win* This, RECT* a2)` | 192 | 100704 | 55360 |
| 0x5ED0A0 | `Win_nonscreen_to_client` | `void (Win* This, RECT* a2)` | 208 | 100896 | 55168 |
| 0x5ED170 | `Win_nonclient_to_screen` | `void (Win* This, RECT* a2)` | 208 | 101104 | 54960 |
| 0x5ED240 | `Win_client_to_screen_2` | `void (Win* This, int* a2, int* a3)` | 144 | 101312 | 54752 |
| 0x5ED2D0 | `Win_screen_to_client_2` | `void (Win* This, int* a2, int* a3)` | 288 | 101456 | 54608 |
| 0x5ED3F0 | `Win_nonclient_to_screen_2` | `void (Win* This, int* a2, int* a3)` | 304 | 101744 | 54320 |
| 0x5ED520 | `Win_screen_to_nonclient` | `void (Win* This, int* a2, int* a3)` | 160 | 102048 | 54016 |
| 0x5ED5C0 | `Win_center` | `int (Win* This)` | 512 | 102208 | 53856 |
| 0x5ED7C0 | `Win_UNK5` | `int (Win* This)` | 16 | 102720 | 53344 |
| 0x5ED7D0 | `Win_move` | `int (Win* This, int a2, int a3)` | 176 | 102736 | 53328 |
| 0x5ED880 | `Win_resize` | `int (Win* This, int a2, int a3, int a4)` | 320 | 102912 | 53152 |
| 0x5ED9C0 | `Win_on_redraw` | `int (Win* This)` | 16 | 103232 | 52832 |
| 0x5ED9D0 | `Win_show` | `void (Win* This, int a2)` | 768 | 103248 | 52816 |
| 0x5EDCD0 | `Win_hide` | `int (Win* This)` | 400 | 104016 | 52048 |
| 0x5EDE60 | `Win_maximize` | `int (Win* This)` | 80 | 104416 | 51648 |
| 0x5EDEB0 | `Win_minimize` | `int (Win* This)` | 80 | 104496 | 51568 |
| 0x5EDF00 | `Win_show_maximize` | `int (Win* This)` | 80 | 104576 | 51488 |
| 0x5EDF50 | `Win_set_caption` | `void (Win* This, char* a2)` | 144 | 104656 | 51408 |
| 0x5EDFE0 | `Win_UNK6` | `int (Win* This, int a2)` | 16 | 104800 | 51264 |
| 0x5EDFF0 | `Win_UNK7` | `void (Win* This, int x, int y, int a4, int a5)` | 64 | 104816 | 51248 |
| 0x5EE030 | `Win_set_vert_pos` | `void (Win* This, int position)` | 32 | 104880 | 51184 |
| 0x5EE050 | `Win_get_vert_pos` | `int (Win* This)` | 32 | 104912 | 51152 |
| 0x5EE070 | `Win_set_horz_pos` | `void (Win* This, int position)` | 32 | 104944 | 51120 |
| 0x5EE090 | `Win_get_horz_pos` | `int (Win* This)` | 32 | 104976 | 51088 |
| 0x5EE0B0 | `Win_set_vert_range` | `void (Win* This, int a2, int a3)` | 32 | 105008 | 51056 |
| 0x5EE0D0 | `Win_set_horz_range` | `void (Win* This, int a2, int a3)` | 32 | 105040 | 51024 |
| 0x5EE0F0 | `Win_set_vert_paging` | `void (Win* This, int a2)` | 32 | 105072 | 50992 |
| 0x5EE110 | `Win_set_horz_paging` | `void (Win* This, int a2)` | 32 | 105104 | 50960 |
| 0x5EE130 | `Win_UNK8` | `void (Win* This, int a2)` | 48 | 105136 | 50928 |
| 0x5EE160 | `Win_UNK9` | `void (Win* This, int a2)` | 48 | 105184 | 50880 |
| 0x5EE190 | `Win_set_modal` | `int (Win* This, int a2, void* a3, Win* a4)` | 240 | 105232 | 50832 |
| 0x5EE280 | `Win_release_modal` | `int (Win* This)` | 176 | 105472 | 50592 |
| 0x5EE330 | `Win_resize_event` | `int (Win* This, int a2, int a3)` | 1056 | 105648 | 50416 |
| 0x5EE750 | `Win_left_down_event` | `void (Win* This, int a2, int a3, int a4)` | 832 | 106704 | 49360 |
| 0x5EEA90 | `Win_key_click_event` | `int (Win* This, int a2, int a3)` | 256 | 107536 | 48528 |
| 0x5EEB90 | `Win_key_down_event` | `int (Win* This, int a2)` | 560 | 107792 | 48272 |
| 0x5EEDC0 | `Win_key_up_event` | `int (Win* This, int a2)` | 176 | 108352 | 47712 |
| 0x5EEE70 | `Win_client_to_nonclient` | `void (Win* This, int* a2, int* a3)` | 240 | 108528 | 47536 |
| 0x5EEF60 | `Win_nonclient_to_client` | `void (Win* This, int* a2, int* a3)` | 240 | 108768 | 47296 |
| 0x5EF050 | `Win_client_to_nonclient_2` | `void (Win* This, RECT* a2)` | 192 | 109008 | 47056 |
| 0x5EF110 | `Win_nonclient_to_client_2` | `void (Win* This, RECT* a2)` | 3088 | 109200 | 46864 |
| 0x5EFD20 | `Win_flip` | `void (RECT* lprcSrc2)` | 1232 | 112288 | 43776 |
| 0x5F01F0 | `Win_init_class` | `int (const char* lpWindowName)` | 752 | 113520 | 42544 |
| 0x5F04E0 | `Win_close_class` | `void ()` | 96 | 114272 | 41792 |
| 0x5F0540 | `Win_adjust_menus` | `int (HWND hWnd)` | 64 | 114368 | 41696 |
| 0x5F0580 | `Win_set_caption_height` | `void (Win* This, int a2)` | 80 | 114432 | 41632 |
| 0x5F05D0 | `Win_set_border_thickness` | `void (Win* This, int thickness)` | 64 | 114512 | 41552 |
| 0x5F0610 | `Win_set_bottom_border_thickness` | `void (Win* This, int thickness)` | 64 | 114576 | 41488 |
| 0x5F0650 | `Win_window_proc` | `LRESULT (HWND hWnd, uint32_t Msg, WPARAM wParam, LPARAM lParam)` | 2576 | 114640 | 41424 |
| 0x5F1060 | `Win_on_query_new_palette` | `int (Win* This)` | 16 | 117216 | 38848 |
| 0x5F1070 | `Win_OnQueryNewPalette` | `int (HWND hWnd)` | 224 | 117232 | 38832 |
| 0x5F1150 | `Win_OnPaletteChanged` | `int (HWND hWnd, LPARAM lParam)` | 496 | 117456 | 38608 |
| 0x5F1340 | `Win_OnPaint` | `bool (HWND hWnd)` | 224 | 117952 | 38112 |
| 0x5F1420 | `Win_OnNCHitTest` | `LRESULT (HWND hWnd, int a2, int a3)` | 96 | 118176 | 37888 |
| 0x5F1480 | `Win_OnActivate` | `LRESULT (HWND hWnd, LPARAM lParam, void* lParama, int a4)` | 320 | 118272 | 37792 |
| 0x5F15C0 | `Win_OnKey` | `void (void* a1, uint32_t a2, int a3, int a4, uint32_t a5)` | 160 | 118592 | 37472 |
| 0x5F1660 | `Win_OnChar` | `void (void* a1, char a2, int a3)` | 112 | 118752 | 37312 |
| 0x5F16D0 | `Win_OnSysKey` | `void (void* a1, uint32_t a2, int a3, int a4, uint32_t a5)` | 336 | 118864 | 37200 |
| 0x5F1820 | `Win_update_cursor` | `int (Win* a1, int a2)` | 2672 | 119200 | 36864 |
| 0x5F2290 | `Win_OnMouseMove` | `void (void* a1, int a2, int a3, uint32_t a4)` | 160 | 121872 | 34192 |
| 0x5F2330 | `Win_OnLButtonDown` | `void (void* a1, int a2, int a3, int a4, uint32_t a5)` | 576 | 122032 | 34032 |
| 0x5F2570 | `Win_OnLButtonUp` | `void (void* a1, int a2, int a3, uint32_t a4)` | 80 | 122608 | 33456 |
| 0x5F25C0 | `Win_OnRButtonDown` | `void (void* a1, int a2, int a3, int a4, uint32_t a5)` | 96 | 122688 | 33376 |
| 0x5F2620 | `Win_OnRButtonUp` | `void (void* a1, int a2, int a3, uint32_t a4)` | 80 | 122784 | 33280 |
| 0x5F2670 | `Win_OnSetCursor` | `int (void* a1, void* a2, uint32_t a3, uint32_t a4)` | 16 | 122864 | 33200 |
| 0x5F2680 | `Win_OnSysCommand` | `int (HWND hWnd, WPARAM wParam, int a3, int a4)` | 128 | 122880 | 33184 |
| 0x5F2700 | `Win_do_caption_buttons` | `int (Win* This)` | 96 | 123008 | 33056 |
| 0x5F2760 | `Win_redo_caption_buttons` | `int (Win* This)` | 480 | 123104 | 32960 |
| 0x5F2940 | `Win_set_rects` | `int (Win* This)` | 384 | 123584 | 32480 |
| 0x5F2AC0 | `Win_window_line_raw` | `void (Win* This, int x, int y, int x2, int y2, int a6, int a7, uint32_t a8)` | 384 | 123968 | 32096 |
| 0x5F2C40 | `Win_set_display_mode` | `int (int a1, int a2, int a3, int a4)` | 32 | 124352 | 31712 |
| 0x5F2C60 | `Win_sync_palette` | `int (Win* This)` | 64 | 124384 | 31680 |
| 0x5F2CA0 | `Win_is_dialog_focus` | `int (Win* This)` | 64 | 124448 | 31616 |
| 0x5F2CE0 | `Win_set_def_focus` | `void (Win* a1)` | 16 | 124512 | 31552 |
| 0x5F2CF0 | `Win_on_redraw_nc` | `void (Win* This, RECT* a2, int a3)` | 7616 | 124528 | 31536 |
| 0x5F4AB0 | `Win_bring_to_top` | `int (Win* This)` | 384 | 132144 | 23920 |
| 0x5F4C30 | `Win_redraw_nc_buffer` | `int (Win* This, int a2)` | 112 | 132528 | 23536 |
| 0x5F4CA0 | `Win_update_nc_buffer` | `void (Win* This, int a2)` | 32 | 132640 | 23424 |
| 0x5F4CC0 | `Win_redraw` | `void ()` | 64 | 132672 | 23392 |
| 0x5F4D00 | `Win_update` | `void ()` | 448 | 132736 | 23328 |
| 0x5F4EC0 | `recurse_zorder` | `void (Win*)` | 352 | 133184 | 22880 |
| 0x5F5020 | `Win_update_zorder` | `void ()` | 96 | 133536 | 22528 |
| 0x5F5080 | `Win_add_parent` | `void (Win* a1)` | 96 | 133632 | 22432 |
| 0x5F50E0 | `Win_remove_parent` | `void (Win* a1)` | 96 | 133728 | 22336 |
| 0x5F5140 | `Win_bring_parent_to_top` | `void (Win* a1)` | 192 | 133824 | 22240 |
| 0x5F5200 | `Win_add_child` | `int (Win* This)` | 112 | 134016 | 22048 |
| 0x5F5270 | `Win_remove_child` | `int (Win* This)` | 112 | 134128 | 21936 |
| 0x5F52E0 | `Win_bring_child_to_top` | `void (Win* This, Win* a2)` | 192 | 134240 | 21824 |
| 0x5F53A0 | `Win_on_window_pos_changing` | `LRESULT (Win* This, WINDOWPOS* lParam)` | 224 | 134432 | 21632 |
| 0x5F5480 | `Win_on_window_pos_changed` | `LRESULT (Win* This, WINDOWPOS* lParam)` | 48 | 134656 | 21408 |
| 0x5F54B0 | `Win_on_move` | `void (Win* This, int a2, int a3)` | 16 | 134704 | 21360 |
| 0x5F54C0 | `Win_on_size` | `void (Win* This, uint32_t a2, int a3, int a4)` | 16 | 134720 | 21344 |
| 0x5F54D0 | `Win_on_size_nc` | `void (Win* This, uint32_t a2, int a3, int a4)` | 16 | 134736 | 21328 |
| 0x5F54E0 | `Win_on_paint` | `void (Win* This, RECT* lprc)` | 816 | 134752 | 21312 |
| 0x5F5810 | `Win_on_nc_paint` | `void (Win* This, RECT* rect, int a3)` | 704 | 135568 | 20496 |
| 0x5F5AD0 | `Win_on_nc_hittest` | `int (Win* This, int a2, int a3)` | 304 | 136272 | 19792 |
| 0x5F5C00 | `Win_on_activate` | `int (Win* This, uint32_t a2, LPARAM lParam, int a4)` | 272 | 136576 | 19488 |
| 0x5F5D10 | `Win_on_key` | `void (Win* This, uint32_t a2, int a3, int a4, uint32_t a5)` | 592 | 136848 | 19216 |
| 0x5F5F60 | `Win_on_char` | `int (char a1, int a2)` | 720 | 137440 | 18624 |
| 0x5F6230 | `Win_on_sys_key` | `LRESULT (WPARAM wParam, int a2, int a3, uint32_t a4)` | 240 | 138160 | 17904 |
| 0x5F6320 | `Win_on_mouse_move` | `void (Win* This, int a2, int a3, uint32_t a4, int a5)` | 160 | 138400 | 17664 |
| 0x5F63C0 | `Win_on_l_button_down` | `void (Win* This, int a2, int x, int y, uint32_t a5, int a6)` | 400 | 138560 | 17504 |
| 0x5F6550 | `Win_on_l_button_up` | `void (Win* This, int a2, int a3, uint32_t a4, int a5)` | 448 | 138960 | 17104 |
| 0x5F6710 | `Win_on_r_button_down` | `void (Win* This, int a2, int a3, int a4, uint32_t a5, int a6)` | 368 | 139408 | 16656 |
| 0x5F6880 | `Win_on_r_button_up` | `void (Win* This, int a2, int a3, uint32_t a4, int a5)` | 432 | 139776 | 16288 |
| 0x5F6A30 | `Win_on_set_cursor` | `int (Win* This, void* hinstDLL, uint32_t fdwReason, uint32_t lpvReserved)` | 16 | 140208 | 15856 |
| 0x5F6A40 | `Win_on_sys_command` | `int (Win* This, uint32_t a2, int a3, int a4)` | 16 | 140224 | 15840 |
| 0x5F6A50 | `Win_get_key_window` | `int ()` | 96 | 140240 | 15824 |
| 0x5F6AB0 | `get_mouse_window_recurse` | `int (Win*, int*, int*)` | 1120 | 140336 | 15728 |
| 0x5F6F10 | `Win_get_mouse_window` | `int (int* a1, int* a2)` | 1040 | 141456 | 14608 |
| 0x5F7320 | `Win_update_screen` | `int (RECT* lprcSrc2, Win* a2)` | 384 | 142496 | 13568 |
| 0x5F74A0 | `Win_update_window` | `int (Win* This, RECT* lprcSrc2)` | 224 | 142880 | 13184 |
| 0x5F7580 | `Win_do_tracking` | `void (Win* This, int a2, int a3)` | 2320 | 143104 | 12960 |
| 0x5F7E90 | `Win_is_visible` | `int (Win* This)` | 48 | 145424 | 10640 |
| 0x5F7EC0 | `Win_update_back_to_window` | `void (Win* This, Buffer* a2)` | 640 | 145472 | 10592 |
| 0x5F8140 | `Win_update_window_to_buffer` | `void (Win* This, Buffer* a2)` | 656 | 146112 | 9952 |
| 0x5F83D0 | `Win_set_bubble_text` | `void (Win* This, char* lpString, RECT* a3)` | 304 | 146768 | 9296 |
| 0x5F8500 | `Win_clear_bubble_text` | `void ()` | 48 | 147072 | 8992 |
| 0x5F8530 | `Win_on_mousewheel_down_vert` | `void (Win* This, int a2)` | 32 | 147120 | 8944 |
| 0x5F8550 | `Win_on_mousewheel_down_horz` | `void (Win* This, int a2)` | 32 | 147152 | 8912 |
| 0x5F8570 | `Win_on_mousewheel_up_vert` | `void (Win* This, int a2)` | 32 | 147184 | 8880 |
| 0x5F8590 | `Win_on_mousewheel_up_horz` | `void (Win* This, int a2)` | 32 | 147216 | 8848 |
| 0x5F85B0 | `Win_on_mousewheel_down` | `void (Win* This, int a2)` | 48 | 147248 | 8816 |
| 0x5F85E0 | `Win_on_mousewheel_up` | `void (Win* This, int a2)` | 8640 | 147296 | 8768 |
| 0x5FA7A0 | `in_box` | `int (int, int, int, int, int, int)` | 64 | 155936 | 128 |
| 0x5FA7E0 | `in_box_2` | `int (int, int, RECT*)` | 64 | 156000 | 64 |

### TEXTINDEX (25 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x5FE2A0 | `Palette_ctor` | `int (Palette* This)` | 64 | 48 | 8448 |
| 0x5FE2E0 | `Palette_dtor` | `int (Palette* This)` | 16 | 112 | 8384 |
| 0x5FE2F0 | `Palette__as` | `int (Palette* This, const Palette* a2)` | 64 | 128 | 8368 |
| 0x5FE330 | `Palette_init` | `int (Palette* This)` | 304 | 192 | 8304 |
| 0x5FE460 | `Palette_set` | `int (Palette* This)` | 144 | 496 | 8000 |
| 0x5FE4F0 | `Palette_set_active_window` | `void (Palette* This)` | 16 | 640 | 7856 |
| 0x5FE500 | `Palette_close` | `int (Palette* This)` | 96 | 656 | 7840 |
| 0x5FE560 | `Palette_get_rgbquad` | `int (Palette* This, RGBQUAD* a2, int a3, int a4)` | 96 | 752 | 7744 |
| 0x5FE5C0 | `Palette_UNK1` | `int (Palette* This, int a2, int a3, int a4)` | 144 | 848 | 7648 |
| 0x5FE650 | `Palette_set_from_dib` | `int (Palette* This, int* dib)` | 128 | 992 | 7504 |
| 0x5FE6D0 | `Palette_reseed` | `int (Palette* This)` | 48 | 1120 | 7376 |
| 0x5FE700 | `Palette_init_cycle` | `int (Palette* This, int a2, int a3, int a4, uint32_t a5)` | 432 | 1168 | 7328 |
| 0x5FE8B0 | `Palette_start_cycle` | `int (Palette* This, int a2)` | 80 | 1600 | 6896 |
| 0x5FE900 | `Palette_UNK2` | `void (Palette* This, int a2)` | 80 | 1680 | 6816 |
| 0x5FE950 | `Palette_UNK3` | `void (Palette* This, int a2)` | 384 | 1760 | 6736 |
| 0x5FEAD0 | `Palette_timer_callback` | `void (int a1, int a2)` | 224 | 2144 | 6352 |
| 0x5FEBB0 | `Palette_init_palette_class` | `void (int a1)` | 320 | 2368 | 6128 |
| 0x5FECF0 | `Palette_close_palette_class` | `int ()` | 32 | 2688 | 5808 |
| 0x5FED10 | `Palette_get_pos` | `int (Palette* This, int a2)` | 48 | 2720 | 5776 |
| 0x5FED40 | `Palette_create_table` | `int (Palette* This, uint8_t* a2, int a3, int a4, int a5)` | 320 | 2768 | 5728 |
| 0x5FEE80 | `Palette_UNK4` | `int (Palette* This, void* a2, int a3, int a4, int a5, int a6, int a7)` | 368 | 3088 | 5408 |
| 0x5FEFF0 | `Palette_create_table_from_color` | `int (Palette* This, int a2, uint8_t* a3, int a4, int a5, int a6, int a7)` | 432 | 3456 | 5040 |
| 0x5FF1A0 | `Palette_UNK5` | `int (Palette* This, int a2, int a3, int a4, int a5)` | 128 | 3888 | 4608 |
| 0x5FF220 | `Palette_UNK6` | `int (Palette* This, int a2, int a3, int a4)` | 96 | 4016 | 4480 |
| 0x5FF280 | `Palette_UNK7` | `int (Palette* This, int a2, int a3, int a4, int a5, int a6, int a7)` | 496 | 4112 | 4384 |

### BASEBUTTON (64 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x604300 | `BasePop_on_button_clicked` | `void (BasePop* This, int a2)` | 208 | 15040 | 12768 |
| 0x6043D0 | `BasePop_set_def_file` | `int (char* a1)` | 64 | 15248 | 12560 |
| 0x604410 | `BasePop_on_key_down` | `int (BasePop* This, int a2)` | 128 | 15312 | 12496 |
| 0x604490 | `BasePop_on_key_click` | `int (BasePop* This, int a2, int a3)` | 32 | 15440 | 12368 |
| 0x6044B0 | `BasePop_on_key_up` | `int (BasePop* This, int a2)` | 32 | 15472 | 12336 |
| 0x6044D0 | `BasePop_double_click` | `void (BasePop* This, int a2)` | 192 | 15504 | 12304 |
| 0x604590 | `BasePop_init_class` | `int ()` | 240 | 15696 | 12112 |
| 0x604680 | `BasePop_close_class` | `void ()` | 112 | 15936 | 11872 |
| 0x6046F0 | `BasePop_set_string_font` | `int (BasePop* This, Font* a2, Font* a3, Font* a4, Font* a5)` | 64 | 16048 | 11760 |
| 0x604730 | `BasePop_set_string_color` | `void (BasePop* This, int a2, int a3, int a4, int a5)` | 48 | 16112 | 11696 |
| 0x604760 | `BasePop_set_string_color2` | `void (BasePop* This, int a2, int a3, int a4, int a5)` | 48 | 16160 | 11648 |
| 0x604790 | `BasePop_set_string_color3` | `void (BasePop* This, int a2, int a3, int a4, int a5)` | 48 | 16208 | 11600 |
| 0x6047C0 | `BasePop_set_string_color_hyper` | `void (BasePop* This, int a2, int a3, int a4, int a5)` | 48 | 16256 | 11552 |
| 0x6047F0 | `BasePop_set_button_font` | `int (BasePop* This, Font* a2, Font* a3, Font* a4)` | 64 | 16304 | 11504 |
| 0x604830 | `BasePop_set_button_color` | `void (BasePop* This, char a2, int a3, int a4, int a5)` | 48 | 16368 | 11440 |
| 0x604860 | `BasePop_set_button_color2` | `void (BasePop* This, char a2, int a3, int a4, int a5)` | 48 | 16416 | 11392 |
| 0x604890 | `BasePop_set_button_color3` | `void (BasePop* This, char a2, int a3, int a4, int a5)` | 48 | 16464 | 11344 |
| 0x6048C0 | `BasePop_set_def_string_font` | `int (Font* a1, Font* a2, Font* a3, Font* a4)` | 64 | 16512 | 11296 |
| 0x604900 | `BasePop_set_def_string_color` | `void (int a1, int a2, int a3, int a4)` | 48 | 16576 | 11232 |
| 0x604930 | `BasePop_set_def_string_color2` | `void (int a1, int a2, int a3, int a4)` | 48 | 16624 | 11184 |
| 0x604960 | `BasePop_set_def_string_color3` | `void (int a1, int a2, int a3, int a4)` | 48 | 16672 | 11136 |
| 0x604990 | `BasePop_set_def_string_color_hyper` | `void (int a1, int a2, int a3, int a4)` | 48 | 16720 | 11088 |
| 0x6049C0 | `BasePop_set_def_button_font` | `int (Font* a1, Font* a2, Font* a3)` | 48 | 16768 | 11040 |
| 0x6049F0 | `BasePop_set_def_button_color` | `void (int a1, int a2, int a3, int a4)` | 48 | 16816 | 10992 |
| 0x604A20 | `BasePop_set_def_button_color2` | `void (int a1, int a2, int a3, int a4)` | 48 | 16864 | 10944 |
| 0x604A50 | `BasePop_set_def_button_color3` | `void (int a1, int a2, int a3, int a4)` | 48 | 16912 | 10896 |
| 0x604A80 | `BasePop_on_modal` | `int ()` | 16 | 16960 | 10848 |
| 0x604A90 | `BasePop_fallout` | `void ()` | 32 | 16976 | 10832 |
| 0x604AB0 | `BasePop_on_redraw` | `int (BasePop* This)` | 912 | 17008 | 10800 |
| 0x604E40 | `BasePop_basepop_alloc` | `int (BasePop* This)` | 96 | 17920 | 9888 |
| 0x604EA0 | `BasePop_UNK2` | `int (BasePop* This)` | 368 | 18016 | 9792 |
| 0x605010 | `BasePop_set_external_window` | `void (BasePop* This, GraphicWin* a2, int a3, int a4, int a5, int a6)` | 224 | 18384 | 9424 |
| 0x6050F0 | `BasePop_set_def_external_window` | `void (GraphicWin* a1, int a2, int a3, int a4, int a5)` | 64 | 18608 | 9200 |
| 0x605130 | `BasePop_on_left_down` | `void (BasePop* This, int x, int y)` | 80 | 18672 | 9136 |
| 0x605180 | `BasePop_UNK3` | `void (BasePop* This, int a2)` | 32 | 18752 | 9056 |
| 0x6051A0 | `BasePop_UNK4` | `void (BasePop* This, int a2)` | 7568 | 18784 | 9024 |
| 0x606F30 | `BaseButton_ctor` | `int (BaseButton* This)` | 272 | 26352 | 1456 |
| 0x607040 | `BaseButton_dtor` | `int (BaseButton* This)` | 128 | 26624 | 1184 |
| 0x6070C0 | `BaseButton_close` | `int (BaseButton* This)` | 208 | 26752 | 1056 |
| 0x607190 | `BaseButton_UNK1` | `int (BaseButton* This, Font* a2, char* a3, int a4, int a5, int a6, int a7, Win* a8, int a9)` | 128 | 26960 | 848 |
| 0x607210 | `BaseButton_init` | `int (BaseButton* This, char* a2, int a3, int a4, int a5, int a6, int a7, Win* a8, int a9)` | 336 | 27088 | 720 |
| 0x607360 | `BaseButton_set_text_color` | `void (BaseButton* This, int a2, int a3, int a4, int a5)` | 64 | 27424 | 384 |
| 0x6073A0 | `BaseButton_set_text_color2` | `void (BaseButton* This, int a2, int a3, int a4, int a5)` | 64 | 27488 | 320 |
| 0x6073E0 | `BaseButton_set_text_color3` | `void (BaseButton* This, int a2, int a3, int a4, int a5)` | 64 | 27552 | 256 |
| 0x607420 | `BaseButton_set_def_text_color` | `void (int a1, int a2, int a3, int a4)` | 48 | 27616 | 192 |
| 0x607450 | `BaseButton_set_def_text_color2` | `void (int a1, int a2, int a3, int a4)` | 48 | 27664 | 144 |
| 0x607480 | `BaseButton_set_def_text_color3` | `void (int a1, int a2, int a3, int a4)` | 48 | 27712 | 96 |
| 0x6074B0 | `BaseButton_set_def_font` | `int (Font* a1, Font* a2, Font* a3)` | 48 | 27760 | 48 |
| 0x6075C0 | `BaseButton_on_left_down` | `void (BaseButton* This, int a2, int a3)` | 352 | 112 | 5696 |
| 0x607720 | `BaseButton_on_left_up` | `void (BaseButton* This, int a2, int a3)` | 208 | 464 | 5344 |
| 0x6077F0 | `BaseButton_on_key_click` | `void (BaseButton* This, int a2, int a3)` | 16 | 672 | 5136 |
| 0x607800 | `BaseButton_on_key_down` | `void (BaseButton* This, int a2)` | 16 | 688 | 5120 |
| 0x607810 | `BaseButton_on_key_up` | `void (BaseButton* This, int a2)` | 16 | 704 | 5104 |
| 0x607820 | `BaseButton_on_mouse_leave` | `void (BaseButton* This, int a2, int a3)` | 48 | 720 | 5088 |
| 0x607850 | `BaseButton_on_mouse_move` | `void (BaseButton* This, int a2, int a3)` | 432 | 768 | 5040 |
| 0x607A00 | `BaseButton_on_right_click` | `void (BaseButton* This, int a2, int a3)` | 32 | 1200 | 4608 |
| 0x607A20 | `BaseButton_on_left_click` | `void (BaseButton* This, int a2, int a3)` | 224 | 1232 | 4576 |
| 0x607B00 | `BaseButton_on_dialog_focus` | `void (BaseButton* This, int a2)` | 16 | 1456 | 4352 |
| 0x607B10 | `BaseButton_timer_callback_daemon` | `void (int a1, int a2)` | 32 | 1472 | 4336 |
| 0x607B30 | `BaseButton_timer_callback` | `void (BaseButton* This, int a2)` | 256 | 1504 | 4304 |
| 0x607C30 | `BaseButton_UNK2` | `int (BaseButton* This)` | 80 | 1760 | 4048 |
| 0x607C80 | `BaseButton_set` | `void (BaseButton* This, int a2)` | 64 | 1840 | 3968 |
| 0x607CC0 | `BaseButton_scalar_dtor` | `int (BaseButton* This, uint32_t a2)` | 32 | 1904 | 3904 |
| 0x607CE0 | `BaseButton_operator_delete` | `void (void* a1, uint32_t a2)` | 3872 | 1936 | 3872 |

### TEXT (48 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x608C10 | `Dialog_ctor` | `int (Dialog* This)` | 512 | 16 | 54720 |
| 0x608E10 | `Dialog_dtor` | `int (Dialog* This)` | 320 | 528 | 54208 |
| 0x608F50 | `Dialog_close` | `int (Dialog* This)` | 512 | 848 | 53888 |
| 0x609150 | `Dialog_init` | `int (Dialog* This)` | 432 | 1360 | 53376 |
| 0x609300 | `Dialog_init_2` | `int (Dialog* This, int a2, int a3, int a4, int a5, int len)` | 368 | 1792 | 52944 |
| 0x609470 | `Dialog_init_3` | `int (Dialog* This, RECT* a2, int len)` | 384 | 2160 | 52576 |
| 0x6095F0 | `Dialog_init_4` | `int (Dialog* This, int len)` | 320 | 2544 | 52192 |
| 0x609730 | `Dialog_init_5` | `int (Dialog* This, int a2, int a3, int a4, int a5, Heap* a6)` | 192 | 2864 | 51872 |
| 0x6097F0 | `Dialog_init_6` | `int (Dialog* This, RECT* a2, Heap* a3)` | 224 | 3056 | 51680 |
| 0x6098D0 | `Dialog_init_7` | `int (Dialog* This, Heap* a2)` | 160 | 3280 | 51456 |
| 0x609970 | `Dialog_UNK1` | `void (Dialog* This, int a2, int a3)` | 32 | 3440 | 51296 |
| 0x609990 | `Dialog_item` | `int (Dialog* This, const char* text, int position)` | 64 | 3472 | 51264 |
| 0x6099D0 | `Dialog_set_selected_id` | `void (Dialog* This, int a2)` | 128 | 3536 | 51200 |
| 0x609A50 | `Dialog_get_selected_id` | `int (Dialog* This)` | 160 | 3664 | 51072 |
| 0x609AF0 | `Dialog_id_to_pos` | `int (Dialog* This, int a2)` | 96 | 3824 | 50912 |
| 0x609B50 | `Dialog_pos_to_id` | `int (Dialog* This, int a2)` | 160 | 3920 | 50816 |
| 0x609BF0 | `Dialog_UNK2` | `void (Dialog* This, HWND hwnd_parent, UINT msg, HWND hwnd, LPARAM lparam)` | 112 | 4080 | 50656 |
| 0x609C60 | `Dialog_set_dialog_font` | `int (Dialog* This, Font* a2, Font* a3, Font* a4)` | 48 | 4192 | 50544 |
| 0x609C90 | `Dialog_set_dialog_text_color` | `void (Dialog* This, int a2, int a3, int a4, int a5)` | 48 | 4240 | 50496 |
| 0x609CC0 | `Dialog_set_dialog_text_color2` | `void (Dialog* This, int a2, int a3, int a4, int a5)` | 48 | 4288 | 50448 |
| 0x609CF0 | `Dialog_set_dialog_text_color3` | `void (Dialog* This, int a2, int a3, int a4, int a5)` | 48 | 4336 | 50400 |
| 0x609D20 | `Dialog_set_def_dialog_font` | `int (Font* a1, Font* a2, Font* a3)` | 48 | 4384 | 50352 |
| 0x609D50 | `Dialog_set_def_dialog_text_color` | `void (int a1, int a2, int a3, int a4, int a5)` | 64 | 4432 | 50304 |
| 0x609D90 | `Dialog_scalar_dtor` | `void* (Dialog* This, uint32_t a2)` | 32 | 4496 | 50240 |
| 0x609DB0 | `ListBox_ctor` | `DWORD (ListBox* This, int is_dialogs)` | 272 | 4528 | 50208 |
| 0x609EC0 | `ListBox_dtor` | `int (ListBox* This)` | 96 | 4800 | 49936 |
| 0x609F20 | `ListBox_close` | `int (ListBox* This)` | 128 | 4896 | 49840 |
| 0x609FA0 | `ListBox_set_selected_id` | `void (ListBox* This, int a2)` | 128 | 5024 | 49712 |
| 0x60A020 | `ListBox_set_selected_pos` | `void (ListBox* This, int a2, int a3)` | 448 | 5152 | 49584 |
| 0x60A1E0 | `ListBox_calculate_dimensions` | `int (ListBox* This)` | 1168 | 5600 | 49136 |
| 0x60A670 | `ListBox_attach` | `int (Dialog* This, GraphicWin* a2, int a3, int a4, int a5)` | 944 | 6768 | 47968 |
| 0x60AA20 | `ListBox_on_right_down` | `void (ListBox* This, int a2, int a3)` | 64 | 7712 | 47024 |
| 0x60AA60 | `ListBox_on_left_down` | `void (ListBox* This, int x, int y)` | 1328 | 7776 | 46960 |
| 0x60AF90 | `ListBox_on_key_down` | `void (ListBox* This, int a2)` | 1440 | 9104 | 45632 |
| 0x60B530 | `ListBox_draw_item` | `void (ListBox* This, int a2)` | 3248 | 10544 | 44192 |
| 0x60C1E0 | `ListBox_UNK1` | `void (ListBox* This, int a2)` | 368 | 13792 | 40944 |
| 0x60C350 | `ListBox_on_redraw` | `int (ListBox* This)` | 640 | 14160 | 40576 |
| 0x60C5D0 | `ListBox_on_scrolling` | `void (ListBox* This, int a2, int a3)` | 208 | 14800 | 39936 |
| 0x60C6A0 | `ListBox_on_scrolled` | `void (ListBox* This, int a2, int a3)` | 48 | 15008 | 39728 |
| 0x60C6D0 | `ListBox_on_right_double_click` | `void (ListBox* This, int a2, int a3)` | 64 | 15056 | 39680 |
| 0x60C710 | `ListBox_on_left_double_click` | `void (ListBox* This, int a2, int a3)` | 176 | 15120 | 39616 |
| 0x60C7C0 | `ListBox_on_dialog_focus` | `void (ListBox* This, int a2)` | 16 | 15296 | 39440 |
| 0x60C7D0 | `ListBox_UNK2` | `int (ListBox* This, char* a2)` | 336 | 15312 | 39424 |
| 0x60C920 | `ListBox_item` | `int (ListBox* This, const char* text, int position)` | 144 | 15648 | 39088 |
| 0x60C9B0 | `ListBox_update_changes` | `int (ListBox* This)` | 448 | 15792 | 38944 |
| 0x60CB70 | `ListBox_on_mousewheel` | `void (ListBox* This, int a2)` | 80 | 16240 | 38496 |
| 0x60CBC0 | `ListBox_on_mouse_move` | `void (ListBox* This, int a2, int a3)` | 592 | 16320 | 38416 |
| 0x60CE10 | `ListBox_on_mouse_leave` | `void (ListBox* This, int a2, int a3)` | 12528 | 16912 | 37824 |

### FONT (13 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x618F30 | `Font_UNK1` | `int (Font* This, int a2, int a3, int a4, int a5)` | 16 | 80 | 16 |
| 0x619650 | `Texture_ctor` | `int (Texture* This)` | 16 | 64 | 49328 |
| 0x619660 | `Texture_dtor` | `int (Texture* This)` | 48 | 80 | 49312 |
| 0x619690 | `Texture_close` | `int (Texture* This)` | 64 | 128 | 49264 |
| 0x6196D0 | `Texture_extract` | `int (Texture* This, Buffer* a2, RECT* a3)` | 64 | 192 | 49200 |
| 0x619710 | `Texture_extract_2` | `int (Texture* This, Buffer* a2, int x, int y, int height, int width, void* a7)` | 464 | 256 | 49136 |
| 0x6198E0 | `Texture_draw_trans` | `void (Texture* This, Buffer* a2, Vert* a3, int a4, Vert* a5)` | 2400 | 720 | 48672 |
| 0x61A240 | `Texture_draw_trans_2` | `int (Buffer* a1, Vert* a2, int a3)` | 1312 | 3120 | 46272 |
| 0x61A760 | `Texture_draw_upper_threshold_trans` | `int (Buffer* a1, Vert* a2, int* a3, int a4, int a5, int a6)` | 2704 | 4432 | 44960 |
| 0x61B1F0 | `Texture_draw` | `int (Buffer* a1, Vert* a2, int* a3, int a4, Vert* a5)` | 3744 | 7136 | 42256 |
| 0x61C090 | `Texture_draw_trans_3` | `int (Buffer* a1, Vert* a2, int* a3, int a4, Vert* a5)` | 9392 | 10880 | 38512 |
| 0x61E540 | `Texture_draw_mask_dest` | `int (Buffer* a1, Vert* a2, uint8_t* a3, uint8_t* a4, int a5, Vert* a6)` | 1424 | 20272 | 29120 |
| 0x61EAD0 | `Texture_draw_coast` | `int (Buffer* a1, Buffer* a2, Vert* a3, Texture* a4, Texture* a5, uint8_t* a6, Vert* a7)` | 4544 | 21696 | 27696 |

### RANDOM (6 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x61FC90 | `Texture_setup_edge_gouraud` | `int (EdgeScan* a1, int a2)` | 14128 | 26240 | 23152 |
| 0x6233C0 | `Texture_draw_2` | `void (Texture* This, Buffer* a2, Vert* a3, int* a4, int a5, Vert* a6, int a7)` | 1520 | 40368 | 9024 |
| 0x6239B0 | `Texture_draw_trans_4` | `void (Texture* This, Buffer* a2, Vert* a3, int a4, Vert* a5, int a6)` | 1488 | 41888 | 7504 |
| 0x623F80 | `Texture_draw_coast_2` | `int (Texture* This, Buffer* a2, Buffer* a3, Vert* a4, Texture* a5, Texture* a6, uint8_t* a7, Vert* a8, int a9)` | 3584 | 43376 | 6016 |
| 0x624D80 | `Texture_setup_edge` | `int (EdgeScan* a1, int a2)` | 2432 | 46960 | 2432 |
| 0x625880 | `parse_string` | `int (char* src, char* dst)` | 1456 | 48 | 1456 |

### LOG (2 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x6265A0 | `scan_out_line_poly` | `uint32_t (int, int)` | 128 | 336 | 2912 |
| 0x626620 | `polygon` | `int (Buffer*, Vert*, int, int)` | 1904 | 464 | 2784 |

### FILEMAP (3 candidates)

| VA | Thinker name | signature | est. size | dist prev | dist next |
|---|---|---|---|---|---|
| 0x627A90 | `pop_ask_number_2` | `int (const char*, const char*, fp_none)` | 208 | 2448 | 2288 |
| 0x627B60 | `pop_ask_number_3` | `int (const char*, fp_none)` | 208 | 2656 | 2080 |
| 0x627C30 | `pop_ask_number_4` | `int (const char*, const char*, int, fp_none)` | 1872 | 2864 | 1872 |

