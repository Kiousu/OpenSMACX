#!/usr/bin/env python3
"""
OpenSMACX redirect patcher (data-driven replacement for ImportAdder.exe + opensmac_patcher.py).

Reads a redirect table (default: redirects.tsv next to this script) whose rows are
    <file offset in terranx.exe>  <MSVC-mangled export name>   (tab separated, '#' comments)
then:
  1. checks that every name is exported by the given OpenSMACX.dll,
  2. appends a new section to the exe holding a rebuilt import directory with an extra
     OpenSMACX.dll descriptor importing every name in table order,
  3. overwrites the first 6 bytes of each listed function with  jmp dword ptr [IAT slot].

Usage:
    python opensmacx_patch.py -e <terranx.exe> -d <OpenSMACX.dll> -o <output folder> [-t redirects.tsv]

Requires: pip install pefile
"""
import argparse
import os
import shutil
import struct
import sys

import pefile

SECTION_NAME = b"opnsmacx"
DLL_NAME = b"OpenSMACX.dll"


def align(v, a):
    return (v + a - 1) // a * a


def read_table(path):
    rows = []
    with open(path, encoding="utf-8") as f:
        for ln, line in enumerate(f, 1):
            s = line.strip()
            if not s or s.startswith("#"):
                continue
            parts = s.split("\t")
            if len(parts) < 2:
                sys.exit("%s:%d: expected <offset>\\t<mangled name>" % (path, ln))
            rows.append((int(parts[0], 16), parts[1].strip()))
    return rows


def dll_exports(path):
    pe = pefile.PE(path)
    if not hasattr(pe, "DIRECTORY_ENTRY_EXPORT"):
        sys.exit("%s has no export table" % path)
    return {e.name.decode() for e in pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name}


def build_import_section(pe, data, names, new_rva):
    """Return the raw bytes of the new section and the (descriptor table size, IAT rva)."""
    orig = pe.OPTIONAL_HEADER.DATA_DIRECTORY[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]]
    orig_off = pe.get_offset_from_rva(orig.VirtualAddress)
    # copy the existing descriptors verbatim (they keep pointing at their original tables)
    descs = []
    off = orig_off
    while True:
        d = data[off:off + 20]
        if d == b"\0" * 20:
            break
        descs.append(d)
        off += 20
    n = len(names)
    desc_size = 20 * (len(descs) + 2)          # existing + OpenSMACX + null terminator
    ilt_rva = new_rva + desc_size
    iat_rva = ilt_rva + 4 * (n + 1)
    names_rva = iat_rva + 4 * (n + 1)

    name_blob = bytearray()
    name_rvas = []
    for nm in names:
        name_rvas.append(names_rva + len(name_blob))
        name_blob += struct.pack("<H", 0) + nm.encode() + b"\0"
        if len(name_blob) & 1:
            name_blob += b"\0"
    dllname_rva = names_rva + len(name_blob)
    name_blob += DLL_NAME + b"\0"

    new_desc = struct.pack("<IIIII", ilt_rva, 0, 0, dllname_rva, iat_rva)
    thunks = b"".join(struct.pack("<I", r) for r in name_rvas) + b"\0\0\0\0"
    blob = b"".join(descs) + new_desc + b"\0" * 20 + thunks + thunks + bytes(name_blob)
    return blob, desc_size, iat_rva


def add_section_and_patch(exe_in, exe_out, rows):
    data = bytearray(open(exe_in, "rb").read())
    pe = pefile.PE(data=bytes(data))
    if pe.OPTIONAL_HEADER.Magic != 0x10B:
        sys.exit("only PE32 (32-bit) executables are supported")

    fa = pe.OPTIONAL_HEADER.FileAlignment
    sa = pe.OPTIONAL_HEADER.SectionAlignment
    nsec = pe.FILE_HEADER.NumberOfSections
    sect_tbl = pe.DOS_HEADER.e_lfanew + 4 + 20 + pe.FILE_HEADER.SizeOfOptionalHeader
    new_hdr_off = sect_tbl + 40 * nsec
    if new_hdr_off + 40 > pe.OPTIONAL_HEADER.SizeOfHeaders:
        sys.exit("no room in the PE header for another section header")
    for s in pe.sections:
        if s.Name.rstrip(b"\0") == SECTION_NAME:
            sys.exit("input already contains an %s section (already patched?)" % SECTION_NAME.decode())

    last = pe.sections[-1]
    new_rva = align(last.VirtualAddress + max(last.Misc_VirtualSize, last.SizeOfRawData), sa)
    new_raw = align(max(s.PointerToRawData + s.SizeOfRawData for s in pe.sections), fa)
    if new_raw > len(data):
        data += b"\0" * (new_raw - len(data))
    elif new_raw < len(data):
        sys.exit("unexpected trailing data after the last section (overlay); refusing to patch")

    names = [name for _, name in rows]
    blob, desc_size, iat_rva = build_import_section(pe, data, names, new_rva)
    raw_size = align(len(blob), fa)
    vsize = len(blob)

    # section header
    hdr = struct.pack("<8sIIIIIIHHI", SECTION_NAME.ljust(8, b"\0"), vsize, new_rva, raw_size, new_raw,
                      0, 0, 0, 0, 0xC0000040)  # readable, writable, initialized data
    data[new_hdr_off:new_hdr_off + 40] = hdr
    # file header: section count
    struct.pack_into("<H", data, pe.DOS_HEADER.e_lfanew + 6, nsec + 1)
    # optional header: SizeOfImage and the import directory entry
    opt = pe.DOS_HEADER.e_lfanew + 24
    struct.pack_into("<I", data, opt + 56, align(new_rva + vsize, sa))
    struct.pack_into("<II", data, opt + 96 + 8 * 1, new_rva, desc_size)
    # section body
    data += blob + b"\0" * (raw_size - len(blob))

    # redirect each function: FF 25 <abs address of IAT slot>
    text = pe.sections[0]
    base = pe.OPTIONAL_HEADER.ImageBase
    for i, (off, name) in enumerate(rows):
        if not (text.PointerToRawData <= off < text.PointerToRawData + text.SizeOfRawData):
            sys.exit("offset 0x%08X (%s) is outside the .text section" % (off, name))
        data[off:off + 6] = b"\xFF\x25" + struct.pack("<I", base + iat_rva + 4 * i)

    with open(exe_out, "wb") as f:
        f.write(data)
    return iat_rva


def verify(exe_out, rows, exports):
    pe = pefile.PE(exe_out)
    imported = []
    for e in pe.DIRECTORY_ENTRY_IMPORT:
        if e.dll == DLL_NAME:
            imported = [i.name.decode() for i in e.imports]
    want = [name for _, name in rows]
    if imported != want:
        sys.exit("verification failed: import table does not match the redirect table")
    missing = [n for n in imported if n not in exports]
    if missing:
        sys.exit("verification failed: %d imports missing from the DLL" % len(missing))
    pe.close()


def main():
    ap = argparse.ArgumentParser(description="OpenSMACX redirect patcher")
    ap.add_argument("-e", "--exe", required=True, help="input terranx.exe (unpatched)")
    ap.add_argument("-d", "--dll", required=True, help="built OpenSMACX.dll")
    ap.add_argument("-o", "--output", required=True, help="folder to write terranx_opensmacx.exe + dll to")
    ap.add_argument("-t", "--table", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "redirects.tsv"))
    args = ap.parse_args()

    rows = read_table(args.table)
    seen = set()
    for off, name in rows:
        if name in seen:
            sys.exit("duplicate redirect for %s" % name)
        seen.add(name)
    exports = dll_exports(args.dll)
    missing = [name for _, name in rows if name not in exports]
    if missing:
        print("%d redirect(s) are not exported by %s:" % (len(missing), args.dll))
        for m in missing:
            print("  ", m)
        sys.exit(1)

    os.makedirs(args.output, exist_ok=True)
    exe_out = os.path.join(args.output, "terranx_opensmacx.exe")
    iat_rva = add_section_and_patch(args.exe, exe_out, rows)
    shutil.copy2(args.dll, os.path.join(args.output, os.path.basename(args.dll)))
    verify(exe_out, rows, exports)
    print("Imports added: %d (IAT at 0x%08X)" % (len(rows), iat_rva))
    print("Functions redirected: %d" % len(rows))
    print("Wrote", exe_out)


if __name__ == "__main__":
    main()
