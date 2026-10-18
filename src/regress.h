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
  * In-process regression harness. Runs a decompiled function and the original exe code side by
  * side on the live game state and logs any difference. Used to promote "Complete - testing"
  * functions to redirected status.
  *
  * Trigger: create the file "opensmacx_regress.txt" in the game directory while a game is
  * loaded. The next time the main thread passes through veh_at() the suite runs once, writes
  * "opensmacx_regress.log" next to it and deletes the trigger. The file may list test names,
  * one per line, to run a subset; empty runs everything.
  *
  * The harness (this file's implementation, the watcher thread and the veh_at() hook) is only
  * compiled in with /DOPENSMACX_REGRESS; a normal build contains none of it.
  */

#ifdef OPENSMACX_REGRESS // build with /DOPENSMACX_REGRESS to compile the harness in
extern volatile LONG RegressPending; // set by the watcher thread, consumed on the main thread

void regress_init(); // starts the trigger watcher thread (idempotent, cheap)
void regress_run(); // runs the suite now on the calling thread
void regress_note(const char *text); // a line for opensmacx_regress.log (kept for regress_run's log too)
#else
#define RegressPending 0
inline void regress_init() {}
inline void regress_run() {}
#endif
