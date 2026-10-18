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
#include "iat.h"

#ifdef OPENSMACX_REGRESS

void **iat_slot(HMODULE module, const char *dll_name, const char *func_name) {
    uint8_t *base = (uint8_t *)module;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir->VirtualAddress) {
        return NULL;
    }
    for (IMAGE_IMPORT_DESCRIPTOR *desc = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir->VirtualAddress);
        desc->Name; desc++) {
        if (_stricmp((const char *)(base + desc->Name), dll_name) != 0) {
            continue;
        }
        IMAGE_THUNK_DATA *names = (IMAGE_THUNK_DATA *)(base + desc->OriginalFirstThunk);
        IMAGE_THUNK_DATA *slots = (IMAGE_THUNK_DATA *)(base + desc->FirstThunk);
        for (; names->u1.AddressOfData; names++, slots++) {
            if (names->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
                continue;
            }
            IMAGE_IMPORT_BY_NAME *imp = (IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData);
            if (strcmp((const char *)imp->Name, func_name) == 0) {
                return (void **)&slots->u1.Function;
            }
        }
    }
    return NULL;
}

void *iat_patch(void **slot, void *replacement) {
    if (!slot) {
        return NULL;
    }
    DWORD old_protect;
    VirtualProtect(slot, sizeof(void *), PAGE_READWRITE, &old_protect);
    void *previous = *slot;
    *slot = replacement;
    VirtualProtect(slot, sizeof(void *), old_protect, &old_protect);
    return previous;
}

#endif // OPENSMACX_REGRESS
