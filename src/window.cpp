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
#include "window.h"
#include "focusguard.h"

/*
 * Windowed mode works the way Thinker's does. With DirectDraw off (prefs_get() answers 0 for the
 * "DirectDraw" key while this mode is on) the game draws through GDI into its main window, a
 * WS_POPUP window sized from GetSystemMetrics(). Here GetSystemMetrics() reports the window size
 * as the screen size, so the game lays itself out for 1024x768, and the main window is created
 * with a caption and a client area of that size instead of as a popup covering the screen. The
 * game's own SetCursorPos() calls are dropped while its window is not in the foreground, so a
 * game running in the background never moves the mouse. (A test game of the regression harness
 * never takes the foreground or moves the mouse at all: focusguard.h.)
 */

BOOL WindowedMode = false;
static int WindowWidth = 1024;
static int WindowHeight = 768;
static int WindowX = 0;
static int WindowY = 0;
static HWND *MainWindow = (HWND *)0x009B7B28; // set from CreateWindowExA() at 0x5F0312
// the exe's import address table entries (USER32)
static FARPROC *const ImportCreateWindowExA = (FARPROC *)0x00669298;
static FARPROC *const ImportSetCursorPos = (FARPROC *)0x00669310;
static FARPROC *const ImportGetSystemMetrics = (FARPROC *)0x00669334;

static int WINAPI windowed_get_system_metrics(int index) {
    if (index == SM_CXSCREEN) {
        return WindowWidth;
    }
    if (index == SM_CYSCREEN) {
        return WindowHeight;
    }
    return GetSystemMetrics(index);
}

static HWND WINAPI windowed_create_window(DWORD ex_style, LPCSTR class_name, LPCSTR window_name,
    DWORD style, int x, int y, int width, int height, HWND parent, HMENU menu, HINSTANCE instance,
    LPVOID param) {
    if (!parent && style == WS_POPUP && !*MainWindow) { // the main window (0x5F030A)
        style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
        RECT rect = { 0, 0, width, height };
        AdjustWindowRectEx(&rect, style, false, ex_style);
        x = WindowX;
        y = WindowY;
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
#ifdef OPENSMACX_REGRESS
        if (FocusGuardOn) {
            ex_style |= WS_EX_NOACTIVATE; // a test game: not activated even by a click (focusguard.h)
        }
#endif
    }
    return CreateWindowExA(ex_style, class_name, window_name, style, x, y, width, height, parent,
        menu, instance, param);
}

static BOOL WINAPI windowed_set_cursor_pos(int x, int y) {
    if (!*MainWindow || GetForegroundWindow() != *MainWindow) {
        return true;
    }
    return SetCursorPos(x, y);
}

static void hook_import(FARPROC *entry, FARPROC replacement) {
    DWORD protect;
    if (VirtualProtect(entry, sizeof(FARPROC), PAGE_READWRITE, &protect)) {
        *entry = replacement;
        VirtualProtect(entry, sizeof(FARPROC), protect, &protect);
    }
}

void window_init() {
    const char *ini = ".\\Alpha Centauri.ini"; // the path the game itself uses
    WindowedMode = GetPrivateProfileIntA("OpenSMACX", "Windowed", 0, ini) != 0;
    if (!WindowedMode) {
        return;
    }
    // the game's buffer copies need sizes divisible by 8 (Thinker: Buffer_copy)
    int width = GetPrivateProfileIntA("OpenSMACX", "WindowWidth", 1024, ini);
    int height = GetPrivateProfileIntA("OpenSMACX", "WindowHeight", 768, ini);
    if (width >= 800 && height >= 600 && !(width & 7) && !(height & 7)) {
        WindowWidth = width;
        WindowHeight = height;
    }
    WindowX = GetPrivateProfileIntA("OpenSMACX", "WindowX", 0, ini);
    WindowY = GetPrivateProfileIntA("OpenSMACX", "WindowY", 0, ini);
    window_hook_imports();
}

/*
Also called when the game reads its "DirectDraw" setting (prefs_get()), which it does before it
creates its window, in case the loader filled in the exe's imports after DllMain ran.
*/
void window_hook_imports() {
    if (!WindowedMode) {
        return;
    }
    hook_import(ImportGetSystemMetrics, (FARPROC)windowed_get_system_metrics);
    hook_import(ImportCreateWindowExA, (FARPROC)windowed_create_window);
    hook_import(ImportSetCursorPos, (FARPROC)windowed_set_cursor_pos);
#ifdef OPENSMACX_REGRESS
    focus_guard_install(); // a test game's SetCursorPos stays the guard's (focusguard.h)
#endif
}
