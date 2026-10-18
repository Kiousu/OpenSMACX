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
  * Focus guard of the regression harness (only compiled in with /DOPENSMACX_REGRESS). A test game
  * (an input-free load: the trigger file opensmacx_regress.txt has a "load" line when the game
  * starts) runs in the background beside whatever Kiousu is using, and must never take the foreground
  * or move the real cursor; it also has to behave the same way in every run, which it didn't while
  * its window took the foreground for a varying time (the click pass's walks differed from run to
  * run). From DllMain on, the exe's and this DLL's user32 imports (whichever each has) go through
  * the guard:
  *   SetForegroundWindow, SetActiveWindow, BringWindowToTop   do nothing (and succeed)
  *   SetCursorPos, ClipCursor                                  do nothing (and succeed)
  *   ShowWindow    an activating command is replaced by its no-activate form: SW_SHOWNORMAL,
  *                 SW_SHOWMAXIMIZED, SW_RESTORE and SW_SHOWDEFAULT by SW_SHOWNOACTIVATE, SW_SHOW by
  *                 SW_SHOWNA, SW_SHOWMINIMIZED and SW_MINIMIZE (which activates the next window) by
  *                 SW_SHOWMINNOACTIVE. The first show of a top-level window takes the launcher's show
  *                 command (STARTUPINFO), as Windows does, before that.
  *   SetWindowPos  gets SWP_NOACTIVATE.
  * In windowed mode (window.cpp) the main window also gets WS_EX_NOACTIVATE, so not even a click on
  * it activates it; the exe's own window procedure reacts to WM_ACTIVATE only (a full redraw, the
  * palette, two globals cleared), so a game that is never activated runs the same way every time.
  * The input hooks of the input-free run (regress.cpp) don't read the real mouse and keyboard either.
  * Each kind of call the guard changes is noted once in opensmacx_regress.log ("FOCUS-GUARD ..."),
  * which no check counts as an error.
  */

#ifdef OPENSMACX_REGRESS
extern BOOL FocusGuardOn; // a test game: set by regress_init() before anything is hooked

// Points the guarded imports at the guard (only while FocusGuardOn); slots already pointing there are
// left alone, so calling it again only redoes what the loader or window_hook_imports() undid.
void focus_guard_install();
#endif
