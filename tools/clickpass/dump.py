"""Reader for the click-pass state dumps the harness DLL writes after each step.

Format, little-endian: magic b"OSXDUMP1", u32 step, u32 section_count, then per section
char name[24], char type[24] (both NUL-padded), u32 count, u32 elem_size, count * elem_size bytes.
The type is a struct name from src/*.h, "u32" (the Globals section and other dword arrays),
"char32" (32-byte NUL-padded strings: GlobalNames, the names of the Globals, same count, and
PopupLabel), "text" (one NUL-padded string, count 1, such as SavePath) or "bytes" (raw bytes with
no layout, such as the game's Random object).
"""
import struct
from typing import Dict, NamedTuple

from clickpass.structs import checked_layout

MAGIC = b"OSXDUMP1"
NAME_SIZE = 24
U32 = "u32"
CHAR32 = "char32"
BYTES = "bytes"
TEXT = "text"
FIXED_ELEM_SIZES = {U32: 4, CHAR32: 32}
GLOBALS = "Globals"
GLOBAL_NAMES = "GlobalNames"

_HEADER = struct.Struct("<8sII")
_SECTION = struct.Struct("<%ds%dsII" % (NAME_SIZE, NAME_SIZE))


class Section(NamedTuple):
    type: str
    count: int
    elem_size: int
    data: bytes


class Dump(NamedTuple):
    step: int
    sections: Dict[str, Section]


class DumpError(ValueError):
    pass


def _text(raw: bytes) -> str:
    return raw.split(b"\0", 1)[0].decode("ascii")


def read_dump(path: str) -> Dump:
    """Read a dump, checking each struct section's element size against src/*.h (LayoutError)."""
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < _HEADER.size:
        raise DumpError("%s: too short for a dump header" % path)
    magic, step, section_count = _HEADER.unpack_from(data)
    if magic != MAGIC:
        raise DumpError("%s: bad magic %r" % (path, magic))
    sections = {}
    pos = _HEADER.size
    for _ in range(section_count):
        if pos + _SECTION.size > len(data):
            raise DumpError("%s: truncated section header at 0x%X" % (path, pos))
        raw_name, raw_type, count, elem_size = _SECTION.unpack_from(data, pos)
        name, type_name = _text(raw_name), _text(raw_type)
        pos += _SECTION.size
        end = pos + count * elem_size
        if end > len(data):
            raise DumpError("%s: section %s truncated" % (path, name))
        if name in sections:
            raise DumpError("%s: section %s appears twice" % (path, name))
        if type_name in FIXED_ELEM_SIZES:
            if elem_size != FIXED_ELEM_SIZES[type_name]:
                raise DumpError("%s: section %s of type %s has elem_size %d" % (path, name, type_name, elem_size))
        elif type_name == TEXT:
            if count != 1:
                raise DumpError("%s: text section %s has %d strings, not 1" % (path, name, count))
        elif type_name != BYTES:
            checked_layout(type_name, elem_size)
        sections[name] = Section(type_name, count, elem_size, data[pos:end])
        pos = end
    if pos != len(data):
        raise DumpError("%s: %d bytes after the last section" % (path, len(data) - pos))
    if GLOBALS in sections and GLOBAL_NAMES in sections \
            and sections[GLOBALS].count != sections[GLOBAL_NAMES].count:
        raise DumpError("%s: %d Globals but %d GlobalNames"
                        % (path, sections[GLOBALS].count, sections[GLOBAL_NAMES].count))
    return Dump(step, sections)
