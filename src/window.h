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
  * Optional windowed mode, off unless the game's ini has
  *   [OpenSMACX]
  *   Windowed=1
  * The game then draws without DirectDraw into an ordinary 1024x768 window (WindowWidth,
  * WindowHeight, WindowX and WindowY in the same section change the size and the position of
  * the window's top left corner). Without the key the game runs exactly as before.
  */
extern BOOL WindowedMode;

void window_init(); // reads the ini and hooks the exe's imports; called from DllMain
void window_hook_imports(); // (again) points the exe's imports at the windowed versions
