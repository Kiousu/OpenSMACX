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
  * Import address table helpers for the regression harness (only compiled in with
  * /DOPENSMACX_REGRESS): find the slot through which a module calls an imported function and
  * point it somewhere else.
  */

#ifdef OPENSMACX_REGRESS
// The import slot for func_name (imported by name from dll_name) in module, or NULL.
void **iat_slot(HMODULE module, const char *dll_name, const char *func_name);
// Points slot at replacement and returns what it held (NULL, and nothing done, for a NULL slot).
void *iat_patch(void **slot, void *replacement);
#endif
