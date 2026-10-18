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
  * Click-pass mode of the regression harness (only compiled in with /DOPENSMACX_REGRESS). The
  * click pass plays the same scripted clicks in two games, the reference exe and OpenSMACX, and
  * compares their game state after every step (tools/clickpass). A trigger file with the lines
  *   load saves\<file>
  *   notests
  *   clickpass
  * loads the save input-free. From start-up (DllMain) on, timeGetTime() on the game's thread is a
  * clock that starts at a fixed value and advances 16 ms per call, so the load sees it too; once
  * the game is under way both random generators are reseeded, the game's save path is set to
  * <game folder>\saves\<file without .SAV> (a loaded file brings the path it was saved under, and
  * the save dialog starts there), and the empty file clickpass\ready appears in the game folder.
  * From then on a registered window message "OpenSMACX.ClickpassDump" posted to the main window,
  * wParam = step number, writes the game state to clickpass\dump_<step>.bin (first as
  * dump_<step>.tmp, renamed when complete). Dump format (read by tools/clickpass/dump.py),
  * little-endian: "OSXDUMP1", u32 step, u32 section count, then per section char name[24],
  * char type[24], u32 count, u32 elem_size and the data. The type is a struct of src/*.h, "u32",
  * "char32" (32-byte names), "text" (one NUL-padded string) or "bytes" (no layout). To name the
  * popup that has the focus, the harness notes the label each popup is opened with (a hook on the
  * exe's BasePop::start(), 0x601BF0, that only records it).
  *
  * Write guard: from DllMain on, the exe's and this DLL's kernel32 imports (whichever each has)
  * CreateFile, DeleteFile, MoveFile, MoveFileEx, CopyFile, CreateDirectory, RemoveDirectory and
  * WritePrivateProfileString, ...A and ...W, refuse (ERROR_ACCESS_DENIED) to create, write, delete,
  * rename or overwrite anything outside the game folder (the exe's folder) and log each refusal to
  * clickpass\guard.log as "WRITE-BLOCKED <api> <full path>"; reads are never refused. A trigger line
  *   outside <folder>
  * allows writes inside that folder (and creating it) too, for saves to another folder. A line
  *   guardprobe [<folder>]
  * makes the harness try writes right after the click pass starts, to test the guard (see
  * clickpass_guard_probe()). Anything of the click pass's own that fails (clickpass folder, ready,
  * dumps, guard.log, the "outside" folder) is logged to <game folder>\clickpass_errors.log as
  * "ERROR <what> <full path> (GetLastError <n>)" or "(errno <n>)".
  */

#ifdef OPENSMACX_REGRESS
/*
 * The game state block the save stores first (the exe's game_data(), 0x9A6490, 0x398 bytes), as the
 * dump's GameState section writes it; tools/clickpass/structs.py reads this layout. Members are named
 * after the global src/*.cpp has at that address, unk_<address> where there is none. save_path is
 * the save path of Ruling R13 (each game's own folder): the dump also writes it with the folder
 * named instead (SavePath), and tools/clickpass/ignore.tsv leaves this raw copy out.
 */
struct ClickpassGameState {
    uint32_t preferences; // 0x9A6490 GamePreferences
    uint32_t more_preferences; // 0x9A6494 GameMorePreferences
    uint32_t warnings; // 0x9A6498 GameWarnings
    uint32_t rules; // 0x9A649C GameRules
    uint32_t unk_9A64A0;
    uint32_t interlude_state; // 0x9A64A4 GameInterludeState
    uint32_t unk_9A64A8[6];
    uint32_t game_state; // 0x9A64C0 GameState
    int diff_level; // 0x9A64C4 DiffLevelCurrent
    int veh_count; // 0x9A64C8 VehCurrentCount
    int base_count; // 0x9A64CC BaseCurrentCount
    uint32_t unk_9A64D0;
    int turn; // 0x9A64D4 TurnCurrentNum
    uint32_t mission_year; // 0x9A64D8 MissionYearCurrent
    uint32_t unk_9A64DC[3];
    uint8_t factions_status[4]; // 0x9A64E8 FactionsStatus
    uint32_t faction_rankings[8]; // 0x9A64EC FactionRankings
    uint32_t ranking_faction_unk1; // 0x9A650C RankingFactionIDUnk1
    uint32_t ranking_faction_unk2; // 0x9A6510 RankingFactionIDUnk2
    BaseSecretProject secret_projects; // 0x9A6514 SecretProject: the base of each project
    int governor_faction_id; // 0x9A6614 GovernorFactionID
    int proposal_pass_count[11]; // 0x9A6618 ProposalPassCount
    int unk_9A6644[11];
    uint8_t tech_achieved[89]; // 0x9A6670 GameTechAchieved: the factions that have each tech
    char save_path[256]; // 0x9A66C9 the save path (see above)
    uint8_t unk_9A67C9[3];
    int climate_value_a; // 0x9A67CC ClimateValueA
    int climate_value_b; // 0x9A67D0 ClimateValueB
    int climate_value_c; // 0x9A67D4 ClimateValueC
    int climate_future_change; // 0x9A67D8 ClimateFutureChange
    uint32_t unk_9A67DC[9];
    int sunspot_duration; // 0x9A6800 SunspotDuration
    uint32_t mount_x; // 0x9A6804 MountPlanetX
    uint32_t mount_y; // 0x9A6808 MountPlanetY
    int dust_cloud_duration; // 0x9A680C DustCloudDuration
    uint32_t unk_9A6810;
    uint32_t scn_vict_facility_obj; // 0x9A6814 ScnVictFacilityObj
    uint32_t solar_flares; // 0x9A6818 SolarFlaresEvent
    uint32_t more_rules; // 0x9A681C GameMoreRules
    uint32_t unk_9A6820[2];
};

extern BOOL ClickpassMode; // the trigger file has a line "clickpass"

// from DllMain in click-pass mode: guard the file-writing imports, pin the clock and note popup
// labels; outside: the trigger's "outside" folder or NULL
void clickpass_guard_install(const char *outside);
// the game is under way (loaded_save: the trigger's "load" path): reseed, set the save path and
// write clickpass\ready
void clickpass_start(const char *loaded_save);
void clickpass_guard_probe(const char *folder); // the trigger line "guardprobe [<folder>]"
void clickpass_on_message(const MSG *msg); // every message the game thread takes off its queue
#endif
