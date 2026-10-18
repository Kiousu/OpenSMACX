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
#include "base.h"
#include "faction.h"
#include "map.h"
#include "veh.h"
#include "game.h"
#include "general.h"
#include "random.h"
#include "iat.h"
#include "clickpass.h"

#ifdef OPENSMACX_REGRESS
#include "path.h"
#include "technology.h"

static_assert(sizeof(PlayerData) == 0x20CC, "PlayerData size");
static_assert(sizeof(Player) == 0x59C, "Player size");
static_assert(sizeof(Base) == 0x134, "Base size");
static_assert(sizeof(Veh) == 0x34, "Veh size");
static_assert(sizeof(Map) == 0x2C, "Map size");
static_assert(sizeof(Landmark) == 0x28, "Landmark size");
static_assert(sizeof(Continent) == 0x1C, "Continent size");
static_assert(sizeof(VehPrototype) == 0x34, "VehPrototype size");
static_assert(sizeof(Monument) == 0x4F4, "Monument size");
static_assert(sizeof(BaseSecretProject) == 0x100, "BaseSecretProject size");
static_assert(sizeof(ClickpassGameState) == 0x398, "ClickpassGameState size");
static_assert(offsetof(ClickpassGameState, secret_projects) == 0x9A6514 - 0x9A6490, "secret_projects");
static_assert(offsetof(ClickpassGameState, tech_achieved) == 0x9A6670 - 0x9A6490, "tech_achieved");
static_assert(offsetof(ClickpassGameState, save_path) == 0x9A66C9 - 0x9A6490, "save_path");
static_assert(offsetof(ClickpassGameState, climate_value_a) == 0x9A67CC - 0x9A6490, "climate_value_a");
static_assert(offsetof(ClickpassGameState, sunspot_duration) == 0x9A6800 - 0x9A6490, "sunspot_duration");
static_assert(offsetof(ClickpassGameState, more_rules) == 0x9A681C - 0x9A6490, "more_rules");
static_assert(sizeof(Path) == 0x2C, "Path size");

static ClickpassGameState *const GameStateBlock = (ClickpassGameState *)0x009A6490; // game_data()'s block
// The path the game saves under by default, without ".SAV" (256 bytes, GameStateBlock->save_path):
// game_data() copies it from the loaded file's header (0x5A6BAE), the save routine opens its file
// box there (0x5AA03B) and a save sets it to the new file.
static char *const SaveGamePath = (char *)0x009A66C9;
static const size_t SaveGamePathSize = sizeof(ClickpassGameState::save_path);

BOOL ClickpassMode = false;
static char GameFolder[MAX_PATH]; // the exe's folder, without a trailing backslash
static size_t GameFolderLen = 0; // 0 while the game folder is unknown
static WCHAR GameFolderW[MAX_PATH]; // the same, for the guard's comparisons
static size_t GameFolderWLen = 0;
static WCHAR OutsideFolderW[MAX_PATH]; // the trigger's "outside" folder, full path, or empty
static size_t OutsideFolderWLen = 0;
static char OutsideFolderA[MAX_PATH]; // the same as ANSI, for the dump's SavePath
static size_t OutsideFolderALen = 0;
static char ClickpassDir[MAX_PATH]; // <game folder>\clickpass, set once the game is under way
static UINT ClickpassDumpMessage = 0; // "OpenSMACX.ClickpassDump"

// The kernel32 functions behind the guarded imports, saved by guard_install() before it points
// an import at its wrapper, and called by the wrappers and by the logs below, so that neither goes
// through the guard (again). The two the logs use are there from the start.
static decltype(&CreateFileA) RealCreateFileA = CreateFileA;
static decltype(&CreateDirectoryA) RealCreateDirectoryA = CreateDirectoryA;
static decltype(&DeleteFileA) RealDeleteFileA = NULL;
static decltype(&MoveFileA) RealMoveFileA = NULL;
static decltype(&MoveFileExA) RealMoveFileExA = NULL;
static decltype(&CopyFileA) RealCopyFileA = NULL;
static decltype(&RemoveDirectoryA) RealRemoveDirectoryA = NULL;
static decltype(&WritePrivateProfileStringA) RealWritePrivateProfileStringA = NULL;
static decltype(&CreateFileW) RealCreateFileW = NULL;
static decltype(&DeleteFileW) RealDeleteFileW = NULL;
static decltype(&MoveFileW) RealMoveFileW = NULL;
static decltype(&MoveFileExW) RealMoveFileExW = NULL;
static decltype(&CopyFileW) RealCopyFileW = NULL;
static decltype(&CreateDirectoryW) RealCreateDirectoryW = NULL;
static decltype(&RemoveDirectoryW) RealRemoveDirectoryW = NULL;
static decltype(&WritePrivateProfileStringW) RealWritePrivateProfileStringW = NULL;

/*
Purpose: Appends text to the file at path, made if need be.
Return Value: 0, or the Windows error code
*/
static DWORD append_text(const char *path, const char *text) {
    HANDLE file = RealCreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return GetLastError();
    }
    DWORD size = (DWORD)strlen(text), written = 0, error = 0;
    if (!WriteFile(file, text, size, &written, NULL)) {
        error = GetLastError();
    } else if (written != size) {
        error = ERROR_HANDLE_DISK_FULL;
    }
    if (!CloseHandle(file) && !error) {
        error = GetLastError();
    }
    return error;
}

/*
Purpose: Appends "ERROR <what> <path> (<code_name> <code>)" to <game folder>\clickpass_errors.log,
         outside clickpass\ so that it works even when that folder can't be made (relative to the
         working folder while the game folder is unknown). code_name is "GetLastError" or "errno".
Return Value: n/a
*/
static void log_error(const char *what, const char *path, const char *code_name, unsigned code) {
    char log[MAX_PATH];
    sprintf_s(log, "%s%sclickpass_errors.log", GameFolder, GameFolderLen ? "\\" : "");
    char line[MAX_PATH + 96];
    _snprintf_s(line, _TRUNCATE, "ERROR %s %.260s (%s %u)\r\n", what, path, code_name, code);
    append_text(log, line);
}

/*
Purpose: Finds the game folder (the exe's folder) once, as ANSI for the paths of the click pass's
         files and as UTF-16 for the guard.
Return Value: Whether it is known
*/
static BOOL find_game_folder() {
    if (GameFolderLen) {
        return true;
    }
    char exe[MAX_PATH] = "";
    DWORD len = GetModuleFileNameA(NULL, exe, sizeof(exe));
    DWORD error = GetLastError();
    char *slash = strrchr(exe, '\\');
    if (!len || len >= sizeof(exe) || !slash) {
        log_error("game-folder", exe, "GetLastError", error);
        return false;
    }
    WCHAR exe_w[MAX_PATH] = L"";
    DWORD len_w = GetModuleFileNameW(NULL, exe_w, MAX_PATH);
    WCHAR *slash_w = wcsrchr(exe_w, L'\\');
    if (!len_w || len_w >= MAX_PATH || !slash_w) {
        log_error("game-folder", exe, "GetLastError", GetLastError());
        return false;
    }
    *slash = 0;
    strcpy_s(GameFolder, exe);
    GameFolderLen = strlen(GameFolder);
    *slash_w = 0;
    wcscpy_s(GameFolderW, exe_w);
    GameFolderWLen = wcslen(GameFolderW);
    return true;
}

// The exe's C runtime keeps rand()'s state per thread: _getptd() (0x6491C3) returns the calling
// thread's data, holdrand is at +0x14 (as the exe's srand() at 0x646010 and rand() show).
typedef uint8_t *__cdecl func_getptd();
static func_getptd *const exe_getptd = (func_getptd *)0x006491C3;
// Win_get_key_window() (0x5F6A50): the game window that has the keyboard focus, or NULL (what
// tools/mp/msg.ps1's KeyWindow reads). It also notes the top window at 0x9B7AC8, which every call
// recomputes from the same globals.
typedef void *__cdecl func_get_key_window();
static func_get_key_window *const Win_get_key_window = (func_get_key_window *)0x005F6A50;
// Win::is_visible() (0x5F7E90): the window and all its parents are shown
typedef BOOL __fastcall func_win_is_visible(void *This, void *edx_unused);
static func_win_is_visible *const Win_is_visible = (func_win_is_visible *)0x005F7E90;

/*
A window that is part of the exe's image (such as the main map window, 0x939444) by its address;
one on the heap, whose address changes from run to run, as 1; none as 0.
*/
static uint32_t window_id(const void *win) {
    if (!win) {
        return 0;
    }
    uint8_t *base = (uint8_t *)GetModuleHandle(NULL);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    uintptr_t addr = (uintptr_t)win;
    if (addr >= (uintptr_t)base && addr < (uintptr_t)base + nt->OptionalHeader.SizeOfImage) {
        return (uint32_t)addr;
    }
    return 1;
}

// A window object's vtable (its first member): part of the exe's image, so the same in every run
// even for a window on the heap, and different for each kind of window; 0 for none.
static uint32_t window_vtable(const void *win) {
    return win ? *(const uint32_t *)win : 0;
}

// ReportIf (0x885F38), member +8: the report screen the F2-F8 keys show in ReportWin (0x876478), 2
// for F2 ... 8 for F8 (the key handler at 0x4982AD sets it before it opens the screen).
static const int *const ReportScreen = (const int *)0x00885F40;

/*
Popups: every popup the exe opens from a script label goes through BasePop::start() (0x601BF0,
thiscall: filename, label, ...), which passes the label only to the text parser and keeps it
nowhere. The hook below (click-pass mode only) notes the popup object, its vtable and its label,
then runs the original: its first instruction, sub esp,0x10C (6 bytes), runs in the hook, which
jumps on behind it. The dump names the label while that popup has the focus.
*/
static uint8_t *const BasePopStart = (uint8_t *)0x00601BF0;
static const uint8_t BasePopStartCode[6] = { 0x81, 0xEC, 0x0C, 0x01, 0x00, 0x00 };
static const void *PopupObject = NULL;
static uint32_t PopupVtable = 0;
static char PopupLabel[32];

static void __cdecl note_popup(const void *popup, const char *label) {
    PopupObject = popup;
    PopupVtable = window_vtable(popup);
    strncpy_s(PopupLabel, label ? label : "", _TRUNCATE);
}

static __declspec(naked) void popup_start_hook() {
    __asm {
        push ecx // this, kept for the original
        push dword ptr [esp + 0xC] // label ([esp + 8] at the entry)
        push ecx
        call note_popup
        add esp, 8
        pop ecx
        sub esp, 0x10C // the original's first instruction
        mov eax, 0x00601BF6
        jmp eax
    }
}

/*
Purpose: Points BasePop::start() at popup_start_hook(), once, after checking that its first
         instruction is the expected one (else nothing is changed and it is logged).
Return Value: n/a
*/
static void popup_hook_install() {
    static BOOL done = false;
    if (done) {
        return;
    }
    done = true;
    if (memcmp(BasePopStart, BasePopStartCode, sizeof(BasePopStartCode))) {
        log_error("popup-hook", "BasePop::start 0x601BF0 is not the expected code", "GetLastError", 0);
        return;
    }
    DWORD protect;
    if (!VirtualProtect(BasePopStart, 5, PAGE_EXECUTE_READWRITE, &protect)) {
        log_error("popup-hook", "BasePop::start 0x601BF0", "GetLastError", GetLastError());
        return;
    }
    int32_t rel = (int32_t)((uint8_t *)popup_start_hook - (BasePopStart + 5));
    BasePopStart[0] = 0xE9; // jmp rel32
    memcpy(BasePopStart + 1, &rel, sizeof(rel));
    VirtualProtect(BasePopStart, 5, protect, &protect);
    FlushInstructionCache(GetCurrentProcess(), BasePopStart, 5);
}

/*
Purpose: The label of the popup that has the focus: the key window is the popup the hook saw last
         (same object, same vtable), else "".
Return Value: n/a
*/
static void focused_popup_label(const void *key_window, char *label, size_t size) {
    label[0] = 0;
    if (key_window && key_window == PopupObject && window_vtable(key_window) == PopupVtable) {
        strncpy_s(label, size, PopupLabel, _TRUNCATE);
    }
}

/*
Purpose: The save path (SaveGamePath) with this game's folder or its "outside" folder named as
         "<game folder>" or "<outside folder>", so that both games of a click pass write the same
         text when they saved to the same place in their own folders (Ruling R13).
Return Value: n/a
*/
static void relative_save_path(char *out, size_t size) {
    char path[0x101];
    memcpy(path, SaveGamePath, SaveGamePathSize);
    path[SaveGamePathSize] = 0;
    const struct { const char *folder; size_t len; const char *name; } folders[] = {
        { GameFolder, GameFolderLen, "<game folder>" },
        { OutsideFolderA, OutsideFolderALen, "<outside folder>" } };
    for (const auto &f : folders) {
        if (f.len && !_strnicmp(path, f.folder, f.len) && (path[f.len] == '\\' || !path[f.len])) {
            _snprintf_s(out, size, _TRUNCATE, "%s%s", f.name, path + f.len);
            return;
        }
    }
    strncpy_s(out, size, path, _TRUNCATE);
}

static void write_section(FILE *f, uint32_t *sections, const char *name, const char *type,
    uint32_t count, uint32_t elem_size, const void *data) {
    char head[48] = {}; // char name[24], char type[24], NUL-padded
    strncpy_s(head, 24, name, _TRUNCATE);
    strncpy_s(head + 24, 24, type, _TRUNCATE);
    fwrite(head, 1, sizeof(head), f);
    fwrite(&count, sizeof(count), 1, f);
    fwrite(&elem_size, sizeof(elem_size), 1, f);
    if (count && elem_size) {
        fwrite(data, elem_size, count, f);
    }
    (*sections)++;
}

struct ClickpassGlobal {
    const char *name;
    uint32_t value;
};

static BOOL write_dump(FILE *f, uint32_t step) {
    Map *tiles = *MapTiles;
    uint32_t tile_count = tiles ? *MapArea : 0;
    void *key_window = Win_get_key_window();
    char popup_label[32] = {}; // NUL-padded: a "char32" section
    focused_popup_label(key_window, popup_label, sizeof(popup_label));
    char save_path[0x120] = {}; // NUL-padded: a "text" section
    relative_save_path(save_path, sizeof(save_path));
    // the path finder (as GameSnapshot keeps it): Path's members (path.h) without its three table
    // pointers, and the table map_table_ points at (MapArea entries)
    const uint32_t *path = (const uint32_t *)Paths;
    const int *path_table = (const int *)path[0];
    const ClickpassGlobal globals[] = {
        { "turn", (uint32_t)*TurnCurrentNum },
        { "game_state", *GameState },
        { "ambience_flags", *AmbienceFlags },
        { "draw_state", *UnkBitfield1 },
        { "maps_revealed", (uint32_t)*TradeMapsRevealedNew },
        { "base_count", (uint32_t)*BaseCurrentCount },
        { "veh_count", (uint32_t)*VehCurrentCount },
        { "landmark_count", (uint32_t)*MapLandmarkCount },
        { "faction_current", (uint32_t)*FactionIDCurrent },
        { "brush1", *BrushVal1 },
        { "brush2", *BrushVal2 },
        { "mount_x", *MountPlanetX },
        { "mount_y", *MountPlanetY },
        { "crt_holdrand", *(uint32_t *)(exe_getptd() + 0x14) },
        { "key_window", window_id(key_window) },
        { "base_win_visible", (uint32_t)(Win_is_visible(BaseWinVar, NULL) != 0) },
        { "key_window_vtable", window_vtable(key_window) },
        { "report_screen", (uint32_t)*ReportScreen },
        { "veh_selected", (uint32_t)*MapWinVehIDSelected },
        { "base_selected", (uint32_t)*BaseIDCurrentSelected },
        { "path_index1", path[3] },
        { "path_index2", path[4] },
        { "path_faction_id", path[5] },
        { "path_x_dst", path[6] },
        { "path_y_dst", path[7] },
        { "path_unk", path[8] },
        { "path_faction_id_2", path[9] },
        { "path_proto_id", path[10] },
    };
    const uint32_t global_count = sizeof(globals) / sizeof(globals[0]);
    uint32_t values[global_count];
    char names[global_count][32] = {};
    for (uint32_t i = 0; i < global_count; i++) {
        values[i] = globals[i].value;
        strncpy_s(names[i], globals[i].name, _TRUNCATE);
    }
    const char magic[8] = { 'O', 'S', 'X', 'D', 'U', 'M', 'P', '1' };
    uint32_t sections = 0; // counted as they are written, then filled in at offset 12
    fwrite(magic, 1, sizeof(magic), f);
    fwrite(&step, sizeof(step), 1, f);
    fwrite(&sections, sizeof(sections), 1, f);
    write_section(f, &sections, "PlayersData", "PlayerData", MaxPlayerNum, sizeof(PlayerData),
        PlayersData);
    write_section(f, &sections, "Players", "Player", MaxPlayerNum, sizeof(Player), Players);
    write_section(f, &sections, "Bases", "Base", *BaseCurrentCount, sizeof(Base), Bases);
    write_section(f, &sections, "Vehs", "Veh", *VehCurrentCount, sizeof(Veh), Vehs);
    write_section(f, &sections, "Tiles", "Map", tile_count, sizeof(Map), tiles);
    write_section(f, &sections, "Landmarks", "Landmark", MaxLandmarkNum, sizeof(Landmark),
        MapLandmark);
    write_section(f, &sections, "Continents", "Continent", MaxContinentNum, sizeof(Continent),
        Continents);
    write_section(f, &sections, "Random", "bytes", 1, sizeof(Random), Rand);
    write_section(f, &sections, "GameState", "ClickpassGameState", 1, sizeof(ClickpassGameState),
        GameStateBlock);
    write_section(f, &sections, "SavePath", "text", 1, sizeof(save_path), save_path);
    write_section(f, &sections, "VehPrototypes", "VehPrototype", MaxVehProtoNum,
        sizeof(VehPrototype), VehPrototypes);
    write_section(f, &sections, "Monuments", "Monument", MaxPlayerNum, sizeof(Monument), Monuments);
    write_section(f, &sections, "PathTable", "u32", path_table ? tile_count : 0, sizeof(int),
        path_table);
    write_section(f, &sections, "SubmergedCount", "u32", MaxPlayerNum, sizeof(int),
        MapBaseSubmergedCount);
    write_section(f, &sections, "SubmergedVeh", "u32", MaxPlayerNum, sizeof(int),
        MapBaseIdClosestSubmergedVeh);
    write_section(f, &sections, "PopupLabel", "char32", 1, sizeof(popup_label), popup_label);
    write_section(f, &sections, "Globals", "u32", global_count, sizeof(uint32_t), values);
    write_section(f, &sections, "GlobalNames", "char32", global_count, 32, names);
    fseek(f, sizeof(magic) + sizeof(step), SEEK_SET);
    fwrite(&sections, sizeof(sections), 1, f);
    return !ferror(f);
}

/*
Write guard: a click-pass game may run while the real game folder is in use, so it must never write
outside its own folder. The exe's and this DLL's kernel32 imports that create, write, delete or
rename files, the ...A ones and their ...W twins (which this DLL's C runtime uses for fopen(),
remove() and rename()), go through the wrappers below. A call that would write to a path whose
full path (GetFullPathName, i.e. as the call itself would resolve it) is inside neither the game
folder nor the trigger's "outside" folder fails with ERROR_ACCESS_DENIED and is logged to
clickpass\guard.log as "WRITE-BLOCKED <api> <full path>". Reads are never refused. Writes through
other functions or other modules' imports are not seen.
*/

/*
Purpose: Logs a refused call to clickpass\guard.log (a failure to do so to clickpass_errors.log).
Return Value: n/a
*/
static void guard_log(const char *api, const char *path) {
    char dir[MAX_PATH], log[MAX_PATH], line[MAX_PATH + 64];
    sprintf_s(dir, "%s\\clickpass", GameFolder);
    sprintf_s(log, "%s\\guard.log", dir);
    _snprintf_s(line, _TRUNCATE, "WRITE-BLOCKED %s %.260s\r\n", api, path);
    DWORD error = ERROR_PATH_NOT_FOUND; // no game folder: find_game_folder() has logged why
    if (GameFolderLen) {
        RealCreateDirectoryA(dir, NULL); // a call refused before clickpass_start() made it
        error = append_text(log, line);
    }
    if (error) {
        log_error("write-guard-log", log, "GetLastError", error);
    }
}

// The same for a UTF-16 path, written to the log as ANSI like the rest of it.
static void guard_log(const char *api, const WCHAR *path) {
    char ansi[2 * MAX_PATH + 1];
    int len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, path, (int)wcsnlen(path, MAX_PATH),
        ansi, 2 * MAX_PATH, NULL, NULL);
    ansi[len] = 0;
    guard_log(api, ansi);
}

/*
Purpose: Whether full (a full path of len characters) is inside folder (folder_len characters,
         no trailing backslash, 0 for none), compared case-insensitively up to a real folder
         boundary, so that "<folder>2\..." is not inside; with folder_itself the folder itself
         counts too.
Return Value: Whether it is
*/
static BOOL in_folder(const WCHAR *full, size_t len, const WCHAR *folder, size_t folder_len,
    BOOL folder_itself) {
    if (!folder_len || len < folder_len
        || CompareStringOrdinal(full, (int)folder_len, folder, (int)folder_len, true) != CSTR_EQUAL) {
        return false;
    }
    if (len == folder_len || (len == folder_len + 1 && full[folder_len] == L'\\')) {
        return folder_itself;
    }
    return full[folder_len] == L'\\';
}

/*
Purpose: Whether a call may write to path: only when its full path is inside the game folder or
         the "outside" folder (with creating_outside, also when it is the "outside" folder itself);
         with the game folder unknown, nowhere. A refused call is logged and gets
         ERROR_ACCESS_DENIED.
Return Value: Whether to make the call
*/
static BOOL guard_allows(const char *api, const WCHAR *path, BOOL creating_outside = false) {
    if (!path) {
        return true; // no file named: the call fails by itself
    }
    WCHAR full[MAX_PATH];
    DWORD len = GetFullPathNameW(path, MAX_PATH, full, NULL);
    BOOL resolved = len && len < MAX_PATH;
    if (resolved && GameFolderWLen && (in_folder(full, len, GameFolderW, GameFolderWLen, false)
        || in_folder(full, len, OutsideFolderW, OutsideFolderWLen, creating_outside))) {
        return true;
    }
    guard_log(api, resolved ? full : path);
    SetLastError(ERROR_ACCESS_DENIED);
    return false;
}

// The same for an ANSI path, converted as the ...A call itself converts it.
static BOOL guard_allows(const char *api, const char *path, BOOL creating_outside = false) {
    if (!path) {
        return true;
    }
    WCHAR wide[MAX_PATH];
    if (!MultiByteToWideChar(AreFileApisANSI() ? CP_ACP : CP_OEMCP, 0, path, -1, wide, MAX_PATH)) {
        guard_log(api, path); // too long for an ...A call anyway
        SetLastError(ERROR_ACCESS_DENIED);
        return false;
    }
    return guard_allows(api, wide, creating_outside);
}

/*
Purpose: Whether a CreateFile call may write: write or delete access (MAXIMUM_ALLOWED may get
         either), delete on close, or a disposition that creates or truncates. Opening an existing
         file to read it does not.
Return Value: Whether it may
*/
static BOOL create_file_writes(DWORD access, DWORD disposition, DWORD flags) {
    const DWORD write_access = GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA
        | DELETE | MAXIMUM_ALLOWED;
    return (access & write_access) || (flags & FILE_FLAG_DELETE_ON_CLOSE)
        || disposition == CREATE_NEW || disposition == CREATE_ALWAYS || disposition == OPEN_ALWAYS
        || disposition == TRUNCATE_EXISTING;
}

static HANDLE WINAPI guard_create_file_a(LPCSTR path, DWORD access, DWORD share,
    LPSECURITY_ATTRIBUTES security, DWORD disposition, DWORD flags, HANDLE template_file) {
    if (create_file_writes(access, disposition, flags) && !guard_allows("CreateFileA", path)) {
        return INVALID_HANDLE_VALUE;
    }
    return RealCreateFileA(path, access, share, security, disposition, flags, template_file);
}

static HANDLE WINAPI guard_create_file_w(LPCWSTR path, DWORD access, DWORD share,
    LPSECURITY_ATTRIBUTES security, DWORD disposition, DWORD flags, HANDLE template_file) {
    if (create_file_writes(access, disposition, flags) && !guard_allows("CreateFileW", path)) {
        return INVALID_HANDLE_VALUE;
    }
    return RealCreateFileW(path, access, share, security, disposition, flags, template_file);
}

static BOOL WINAPI guard_delete_file_a(LPCSTR path) {
    return guard_allows("DeleteFileA", path) && RealDeleteFileA(path);
}

static BOOL WINAPI guard_delete_file_w(LPCWSTR path) {
    return guard_allows("DeleteFileW", path) && RealDeleteFileW(path);
}

static BOOL WINAPI guard_move_file_a(LPCSTR from, LPCSTR to) {
    return guard_allows("MoveFileA", from) && guard_allows("MoveFileA", to)
        && RealMoveFileA(from, to);
}

static BOOL WINAPI guard_move_file_w(LPCWSTR from, LPCWSTR to) {
    return guard_allows("MoveFileW", from) && guard_allows("MoveFileW", to)
        && RealMoveFileW(from, to);
}

static BOOL WINAPI guard_move_file_ex_a(LPCSTR from, LPCSTR to, DWORD flags) {
    return guard_allows("MoveFileExA", from) && guard_allows("MoveFileExA", to) // to NULL: delete
        && RealMoveFileExA(from, to, flags);
}

static BOOL WINAPI guard_move_file_ex_w(LPCWSTR from, LPCWSTR to, DWORD flags) {
    return guard_allows("MoveFileExW", from) && guard_allows("MoveFileExW", to) // to NULL: delete
        && RealMoveFileExW(from, to, flags);
}

static BOOL WINAPI guard_copy_file_a(LPCSTR from, LPCSTR to, BOOL fail_if_exists) {
    return guard_allows("CopyFileA", to) && RealCopyFileA(from, to, fail_if_exists);
}

static BOOL WINAPI guard_copy_file_w(LPCWSTR from, LPCWSTR to, BOOL fail_if_exists) {
    return guard_allows("CopyFileW", to) && RealCopyFileW(from, to, fail_if_exists);
}

static BOOL WINAPI guard_create_directory_a(LPCSTR path, LPSECURITY_ATTRIBUTES security) {
    return guard_allows("CreateDirectoryA", path, true) && RealCreateDirectoryA(path, security);
}

static BOOL WINAPI guard_create_directory_w(LPCWSTR path, LPSECURITY_ATTRIBUTES security) {
    return guard_allows("CreateDirectoryW", path, true) && RealCreateDirectoryW(path, security);
}

static BOOL WINAPI guard_remove_directory_a(LPCSTR path) {
    return guard_allows("RemoveDirectoryA", path) && RealRemoveDirectoryA(path);
}

static BOOL WINAPI guard_remove_directory_w(LPCWSTR path) {
    return guard_allows("RemoveDirectoryW", path) && RealRemoveDirectoryW(path);
}

static BOOL WINAPI guard_write_private_profile_string_a(LPCSTR section, LPCSTR key, LPCSTR value,
    LPCSTR file) {
    // as for the real call, a file name without a folder is one in the Windows folder, none win.ini
    const char *path = file;
    char windows_file[MAX_PATH];
    if (!file || !strpbrk(file, "\\/:")) {
        char windows[MAX_PATH] = "";
        GetWindowsDirectoryA(windows, sizeof(windows));
        _snprintf_s(windows_file, _TRUNCATE, "%s\\%s", windows, file ? file : "win.ini");
        path = windows_file;
    }
    return guard_allows("WritePrivateProfileStringA", path)
        && RealWritePrivateProfileStringA(section, key, value, file);
}

static BOOL WINAPI guard_write_private_profile_string_w(LPCWSTR section, LPCWSTR key,
    LPCWSTR value, LPCWSTR file) {
    const WCHAR *path = file; // as in guard_write_private_profile_string_a
    WCHAR windows_file[MAX_PATH];
    if (!file || !wcspbrk(file, L"\\/:")) {
        WCHAR windows[MAX_PATH] = L"";
        GetWindowsDirectoryW(windows, MAX_PATH);
        _snwprintf_s(windows_file, _TRUNCATE, L"%ls\\%ls", windows, file ? file : L"win.ini");
        path = windows_file;
    }
    return guard_allows("WritePrivateProfileStringW", path)
        && RealWritePrivateProfileStringW(section, key, value, file);
}

static HMODULE this_dll() {
    HMODULE dll = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
        | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&this_dll, &dll);
    return dll;
}

/*
Purpose: Points the exe's and this DLL's guarded imports (whichever each imports) at the wrappers;
         slots already pointing there are left alone, so calling it again only redoes what the
         loader may have undone.
Return Value: n/a
*/
static void guard_install() {
    struct GuardImport {
        const char *name;
        void *wrapper;
        void **real;
    };
    const GuardImport imports[] = {
        { "CreateFileA", (void *)guard_create_file_a, (void **)&RealCreateFileA },
        { "CreateFileW", (void *)guard_create_file_w, (void **)&RealCreateFileW },
        { "DeleteFileA", (void *)guard_delete_file_a, (void **)&RealDeleteFileA },
        { "DeleteFileW", (void *)guard_delete_file_w, (void **)&RealDeleteFileW },
        { "MoveFileA", (void *)guard_move_file_a, (void **)&RealMoveFileA },
        { "MoveFileW", (void *)guard_move_file_w, (void **)&RealMoveFileW },
        { "MoveFileExA", (void *)guard_move_file_ex_a, (void **)&RealMoveFileExA },
        { "MoveFileExW", (void *)guard_move_file_ex_w, (void **)&RealMoveFileExW },
        { "CopyFileA", (void *)guard_copy_file_a, (void **)&RealCopyFileA },
        { "CopyFileW", (void *)guard_copy_file_w, (void **)&RealCopyFileW },
        { "CreateDirectoryA", (void *)guard_create_directory_a, (void **)&RealCreateDirectoryA },
        { "CreateDirectoryW", (void *)guard_create_directory_w, (void **)&RealCreateDirectoryW },
        { "RemoveDirectoryA", (void *)guard_remove_directory_a, (void **)&RealRemoveDirectoryA },
        { "RemoveDirectoryW", (void *)guard_remove_directory_w, (void **)&RealRemoveDirectoryW },
        { "WritePrivateProfileStringA", (void *)guard_write_private_profile_string_a,
            (void **)&RealWritePrivateProfileStringA },
        { "WritePrivateProfileStringW", (void *)guard_write_private_profile_string_w,
            (void **)&RealWritePrivateProfileStringW },
    };
    const HMODULE modules[] = { GetModuleHandle(NULL), this_dll() };
    for (HMODULE module : modules) {
        for (const GuardImport &imp : imports) {
            void **slot = module ? iat_slot(module, "kernel32.dll", imp.name) : NULL;
            if (slot && *slot != imp.wrapper) {
                *imp.real = *slot; // first: the wrapper may run as soon as the slot points at it
                iat_patch(slot, imp.wrapper);
            }
        }
    }
}

/*
Purpose: The trigger line "outside <folder>": writes inside that folder, and creating the folder
         itself, are allowed too (the save dialog can save anywhere). Resolved as the guard
         resolves paths; a folder that can't be resolved, or a drive's root, is not taken (logged).
Return Value: n/a
*/
static void set_outside_folder(const char *folder) {
    WCHAR wide[MAX_PATH];
    DWORD len = MultiByteToWideChar(CP_ACP, 0, folder, -1, wide, MAX_PATH)
        ? GetFullPathNameW(wide, MAX_PATH, OutsideFolderW, NULL) : 0;
    DWORD error = !len ? GetLastError() : len >= MAX_PATH ? ERROR_FILENAME_EXCED_RANGE : 0;
    if (!error) {
        while (len && OutsideFolderW[len - 1] == L'\\') {
            OutsideFolderW[--len] = 0;
        }
        if (!wcschr(OutsideFolderW, L'\\')) {
            error = ERROR_BAD_PATHNAME; // a drive's root
        }
    }
    if (error) {
        OutsideFolderW[0] = 0;
        log_error("outside-folder", folder, "GetLastError", error);
        return;
    }
    OutsideFolderWLen = len;
    int len_a = WideCharToMultiByte(CP_ACP, 0, OutsideFolderW, -1, OutsideFolderA, MAX_PATH, NULL,
        NULL);
    OutsideFolderALen = len_a > 0 ? strlen(OutsideFolderA) : 0; // (0: SavePath names it in full)
}

static void clock_install();

/*
Purpose: Click-pass mode, from DllMain (regress_init): find the game folder, take the trigger's
         "outside" folder (or NULL), guard the imports that write files, pin the clock and hook
         BasePop::start() for the popup labels, before the game itself runs.
Return Value: n/a
*/
void clickpass_guard_install(const char *outside) {
    find_game_folder();
    if (outside && *outside) {
        set_outside_folder(outside);
    }
    guard_install();
    clock_install();
    popup_hook_install();
}

// Whether a probe's import slot goes through the guard; logged as "unguarded-<api>" when not.
static BOOL probe_slot_guarded(HMODULE module, void **slot, void *wrapper, const char *what) {
    if (slot && *slot == wrapper) {
        return true;
    }
    char file[MAX_PATH] = "";
    GetModuleFileNameA(module, file, sizeof(file));
    log_error(what, file, "GetLastError", 0);
    return false;
}

static void probe_create_file(void **slot, const char *path) {
    HANDLE probe = ((decltype(&CreateFileA))*slot)(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (probe != INVALID_HANDLE_VALUE) {
        CloseHandle(probe); // let through: the check finds the file
    }
}

static void probe_fopen(const char *path) {
    FILE *probe = fopen(path, "wb"); // this DLL's C runtime: CreateFileW
    if (probe) {
        fclose(probe);
    }
}

/*
Purpose: The trigger line "guardprobe [<folder>]", right after clickpass_start(): writes for a check
         to read the guard's answers from clickpass\guard.log, without a game action. Always
         refused: ..\clickpass_guard_probe.txt through the exe's CreateFileA import and
         ..\clickpass_guard_probe_w.txt through this DLL's C runtime (CreateFileW). With a folder
         (no trailing backslash), through the same two: creating <folder> (CreateDirectoryA),
         <folder>\clickpass_guard_probe.txt and <folder>\clickpass_guard_probe_w.txt, allowed only
         when <folder> is the "outside" folder, then <folder>\..\clickpass_guard_probe.txt and
         <folder>2\clickpass_guard_probe.txt, always refused. Nothing is tried unless all three
         imports are guarded.
Return Value: n/a
*/
void clickpass_guard_probe(const char *folder) {
    HMODULE exe = GetModuleHandle(NULL), dll = this_dll();
    void **create_file = iat_slot(exe, "kernel32.dll", "CreateFileA");
    void **create_directory = iat_slot(exe, "kernel32.dll", "CreateDirectoryA");
    void **create_file_w = dll ? iat_slot(dll, "kernel32.dll", "CreateFileW") : NULL;
    if (!probe_slot_guarded(exe, create_file, (void *)guard_create_file_a, "unguarded-CreateFileA")
        || !probe_slot_guarded(exe, create_directory, (void *)guard_create_directory_a,
            "unguarded-CreateDirectoryA")
        || !probe_slot_guarded(dll, create_file_w, (void *)guard_create_file_w,
            "unguarded-CreateFileW")) {
        return;
    }
    probe_create_file(create_file, "..\\clickpass_guard_probe.txt");
    probe_fopen("..\\clickpass_guard_probe_w.txt");
    if (folder && *folder) {
        char path[2 * MAX_PATH];
        ((decltype(&CreateDirectoryA))*create_directory)(folder, NULL);
        sprintf_s(path, "%s\\clickpass_guard_probe.txt", folder);
        probe_create_file(create_file, path);
        sprintf_s(path, "%s\\clickpass_guard_probe_w.txt", folder);
        probe_fopen(path);
        sprintf_s(path, "%s\\..\\clickpass_guard_probe.txt", folder);
        probe_create_file(create_file, path);
        sprintf_s(path, "%s2\\clickpass_guard_probe.txt", folder);
        probe_create_file(create_file, path);
    }
}

/*
Purpose: A loaded file brings the path it was saved under, for the test saves a folder outside this
         game's folder, where the guard (rightly) refuses the save dialog's writes. Sets the save
         path to <game folder>\saves\<loaded_save's file name without .SAV>, as if the file had been
         saved in this folder (Ruling R13).
Return Value: n/a
*/
static void set_save_path(const char *loaded_save) {
    const char *name = loaded_save;
    for (const char *c = loaded_save; *c; c++) {
        if (*c == '\\' || *c == '/' || *c == ':') {
            name = c + 1;
        }
    }
    char path[MAX_PATH];
    _snprintf_s(path, _TRUNCATE, "%s\\saves\\%s", GameFolder, name);
    char *dot = strrchr(path, '.');
    if (dot && dot > strrchr(path, '\\')) {
        *dot = 0;
    }
    memset(SaveGamePath, 0, SaveGamePathSize); // nothing of the old path left behind the new one
    strncpy_s(SaveGamePath, SaveGamePathSize, path, _TRUNCATE);
}

/*
Determinism (Ruling R14, from start-up since the final review): from clickpass_guard_install() (DllMain)
on, timeGetTime() as the exe and this DLL import it returns, on the thread that loaded this DLL (the
game's), a clock that starts at ClickpassClockStart and advances 16 ms per call (as the aiplan live
runs' clock), so the load, animations, waits and anything seeded from the clock see the same times
in both games. Other threads get the real clock: their calls come at times of their own and would
make the game thread's readings vary.
*/
static const DWORD ClickpassClockStart = 0x00100000;
static DWORD ClickpassClock = ClickpassClockStart;
static DWORD ClickpassThread = 0;
static decltype(&timeGetTime) RealTimeGetTime = NULL;

static DWORD WINAPI clickpass_time_get_time() {
    if (GetCurrentThreadId() != ClickpassThread) {
        return RealTimeGetTime();
    }
    return ClickpassClock += 16;
}

/*
Purpose: Points the exe's and this DLL's timeGetTime imports at the pinned clock; slots already
         pointing there are left alone, so calling it again only redoes what the loader may have
         undone. The first call (from DllMain) starts the clock on the calling thread and logs a
         module without the import.
Return Value: n/a
*/
static void clock_install() {
    static BOOL started = false;
    if (!started) {
        ClickpassThread = GetCurrentThreadId();
        ClickpassClock = ClickpassClockStart;
    }
    const HMODULE modules[] = { GetModuleHandle(NULL), this_dll() };
    for (HMODULE module : modules) {
        void **slot = module ? iat_slot(module, "winmm.dll", "timeGetTime") : NULL;
        if (!slot) {
            if (!started) {
                char file[MAX_PATH] = "";
                GetModuleFileNameA(module, file, sizeof(file));
                log_error("pin-timeGetTime", file, "GetLastError", 0);
            }
        } else if (*slot != (void *)clickpass_time_get_time) {
            RealTimeGetTime = (decltype(&timeGetTime))*slot; // first, as in guard_install()
            iat_patch(slot, (void *)clickpass_time_get_time);
        }
    }
    started = true;
}

/*
Purpose: The game is under way (from the input-free run's timer, on the game's thread; loaded_save is
         the trigger's "load" path): reseed the exe's rand() and the game's Random object, so both
         games of a click pass roll the same dice from here on, set the save path and tell the
         driver with the empty file clickpass\ready. The clock pinned since DllMain must be this
         thread's (else that is logged and the clock moves to this thread). Failures go to
         clickpass_errors.log.
Return Value: n/a
*/
void clickpass_start(const char *loaded_save) {
    guard_install(); // again, in case the loader filled in the exe's imports after DllMain
    if (!find_game_folder()) {
        return;
    }
    set_save_path(loaded_save);
    clock_install(); // again, as guard_install()
    if (GetCurrentThreadId() != ClickpassThread) {
        char thread[32];
        sprintf_s(thread, "%lu", ClickpassThread);
        log_error("clock-thread", thread, "GetCurrentThreadId", GetCurrentThreadId());
        ClickpassThread = GetCurrentThreadId();
    }
    sprintf_s(ClickpassDir, "%s\\clickpass", GameFolder);
    if (!CreateDirectoryA(ClickpassDir, NULL)) {
        DWORD error = GetLastError(); // ERROR_ALREADY_EXISTS for a file of that name too
        DWORD attributes = GetFileAttributesA(ClickpassDir);
        if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            log_error("create-folder", ClickpassDir, "GetLastError", error);
        }
    }
    ClickpassDumpMessage = RegisterWindowMessageA("OpenSMACX.ClickpassDump");
    if (!ClickpassDumpMessage) {
        log_error("register-message", "OpenSMACX.ClickpassDump", "GetLastError", GetLastError());
    }
    my_srand(1);
    random_reseed(1);
    char path[MAX_PATH];
    sprintf_s(path, "%s\\ready", ClickpassDir);
    FILE *ready = fopen(path, "wb");
    if (!ready || fclose(ready)) {
        log_error("write-ready", path, "errno", errno);
    }
}

/*
Purpose: Called from the harness's message hook for every message the game thread takes off its
         queue. On the dump message (wParam = step) the game state goes to clickpass\dump_<step>.bin,
         written as dump_<step>.tmp first, so a reader never sees half a file. Failures go to
         clickpass_errors.log.
Return Value: n/a
*/
void clickpass_on_message(const MSG *msg) {
    if (!ClickpassDumpMessage || msg->message != ClickpassDumpMessage || !ClickpassDir[0]) {
        return;
    }
    uint32_t step = (uint32_t)msg->wParam;
    char tmp[MAX_PATH], bin[MAX_PATH];
    sprintf_s(tmp, "%s\\dump_%u.tmp", ClickpassDir, step);
    sprintf_s(bin, "%s\\dump_%u.bin", ClickpassDir, step);
    FILE *f = fopen(tmp, "wb");
    if (!f) {
        log_error("open-dump", tmp, "errno", errno);
        return;
    }
    errno = 0;
    BOOL ok = write_dump(f, step);
    if (!ok) {
        log_error("write-dump", tmp, "errno", errno);
    }
    if (fclose(f)) {
        log_error("close-dump", tmp, "errno", errno);
        ok = false;
    }
    if (ok && !MoveFileExA(tmp, bin, MOVEFILE_REPLACE_EXISTING)) {
        log_error("rename-dump", bin, "GetLastError", GetLastError());
        ok = false;
    }
    if (!ok && !DeleteFileA(tmp)) {
        log_error("delete-tmp", tmp, "GetLastError", GetLastError());
    }
}

#endif // OPENSMACX_REGRESS
