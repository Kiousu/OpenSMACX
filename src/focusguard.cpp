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
#include "focusguard.h"
#include "iat.h"
#include "regress.h"

#ifdef OPENSMACX_REGRESS

BOOL FocusGuardOn = false;

// user32's own functions, from GetProcAddress (never through an import slot the guard may point at
// itself)
static decltype(&ShowWindow) RealShowWindow = NULL;
static decltype(&SetWindowPos) RealSetWindowPos = NULL;
static BOOL FirstShow = true; // no top-level window has been shown yet

enum FocusNote {
    NOTE_SET_FOREGROUND_WINDOW, NOTE_SET_ACTIVE_WINDOW, NOTE_BRING_WINDOW_TO_TOP, NOTE_SET_CURSOR_POS,
    NOTE_CLIP_CURSOR, NOTE_SET_WINDOW_POS, NOTE_SHOW_WINDOW, // + the command asked for (0..11)
    NOTE_COUNT = NOTE_SHOW_WINDOW + 12
};
static volatile LONG Noted[NOTE_COUNT];

/*
Purpose: Notes in opensmacx_regress.log, once per kind, what the guard did instead of a call.
Return Value: n/a
*/
static void note(int kind, const char *text) {
    if (kind < 0 || kind >= NOTE_COUNT || InterlockedExchange(&Noted[kind], 1)) {
        return;
    }
    char line[160];
    _snprintf_s(line, _TRUNCATE, "FOCUS-GUARD %s", text);
    regress_note(line);
}

/*
Purpose: The no-activate form of a ShowWindow command (the others as they are).
Return Value: The command to use
*/
static int no_activate(int cmd) {
    switch (cmd) {
      case SW_SHOWNORMAL:
      case SW_SHOWMAXIMIZED:
      case SW_RESTORE:
      case SW_SHOWDEFAULT:
        return SW_SHOWNOACTIVATE;
      case SW_SHOW:
        return SW_SHOWNA; // (as SW_SHOW: a minimized window stays minimized)
      case SW_SHOWMINIMIZED:
      case SW_MINIMIZE:
        return SW_SHOWMINNOACTIVE;
      default:
        return cmd; // SW_HIDE, SW_SHOWNOACTIVATE, SW_SHOWMINNOACTIVE, SW_SHOWNA, SW_FORCEMINIMIZE
    }
}

static BOOL WINAPI focus_show_window(HWND hwnd, int cmd) {
    int asked = cmd;
    if (FirstShow && !(GetWindowLongA(hwnd, GWL_STYLE) & WS_CHILD)) {
        FirstShow = false; // Windows itself takes the first top-level show's command from the launcher
        STARTUPINFOA startup = { sizeof(startup) };
        GetStartupInfoA(&startup);
        if (startup.dwFlags & STARTF_USESHOWWINDOW) {
            cmd = startup.wShowWindow;
        }
    }
    int safe = no_activate(cmd);
    if (safe != asked && asked >= 0 && asked < 12) {
        char text[96];
        sprintf_s(text, "ShowWindow(%d) shown as ShowWindow(%d)", asked, safe);
        note(NOTE_SHOW_WINDOW + asked, text);
    }
    return RealShowWindow(hwnd, safe);
}

static BOOL WINAPI focus_set_window_pos(HWND hwnd, HWND after, int x, int y, int cx, int cy, UINT flags) {
    if (!(flags & SWP_NOACTIVATE)) {
        note(NOTE_SET_WINDOW_POS, "SetWindowPos given SWP_NOACTIVATE");
    }
    return RealSetWindowPos(hwnd, after, x, y, cx, cy, flags | SWP_NOACTIVATE);
}

static BOOL WINAPI focus_set_foreground_window(HWND) {
    note(NOTE_SET_FOREGROUND_WINDOW, "SetForegroundWindow not done");
    return true;
}

static HWND WINAPI focus_set_active_window(HWND) {
    note(NOTE_SET_ACTIVE_WINDOW, "SetActiveWindow not done");
    return GetActiveWindow(); // (what it would return: the window that was active, which stays so)
}

static BOOL WINAPI focus_bring_window_to_top(HWND) {
    note(NOTE_BRING_WINDOW_TO_TOP, "BringWindowToTop not done");
    return true;
}

static BOOL WINAPI focus_set_cursor_pos(int, int) {
    note(NOTE_SET_CURSOR_POS, "SetCursorPos not done");
    return true;
}

static BOOL WINAPI focus_clip_cursor(const RECT *) {
    note(NOTE_CLIP_CURSOR, "ClipCursor not done");
    return true;
}

static HMODULE focus_this_dll() {
    HMODULE dll = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
        | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&focus_this_dll, &dll);
    return dll;
}

void focus_guard_install() {
    if (!FocusGuardOn) {
        return;
    }
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (!user32) {
        return;
    }
    if (!RealShowWindow) {
        RealShowWindow = (decltype(&ShowWindow))GetProcAddress(user32, "ShowWindow");
        RealSetWindowPos = (decltype(&SetWindowPos))GetProcAddress(user32, "SetWindowPos");
    }
    if (!RealShowWindow || !RealSetWindowPos) {
        return;
    }
    struct FocusImport {
        const char *name;
        void *wrapper;
    };
    const FocusImport imports[] = {
        { "SetForegroundWindow", (void *)focus_set_foreground_window },
        { "SetActiveWindow", (void *)focus_set_active_window },
        { "BringWindowToTop", (void *)focus_bring_window_to_top },
        { "SetCursorPos", (void *)focus_set_cursor_pos },
        { "ClipCursor", (void *)focus_clip_cursor },
        { "ShowWindow", (void *)focus_show_window },
        { "SetWindowPos", (void *)focus_set_window_pos },
    };
    const HMODULE modules[] = { GetModuleHandle(NULL), focus_this_dll() };
    for (HMODULE module : modules) {
        for (const FocusImport &imp : imports) {
            void **slot = module ? iat_slot(module, "user32.dll", imp.name) : NULL;
            if (slot && *slot != imp.wrapper) {
                iat_patch(slot, imp.wrapper);
            }
        }
    }
}

#endif // OPENSMACX_REGRESS
