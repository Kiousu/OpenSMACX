"""Struct layouts from the OpenSMACX headers (MSVC x86 layout, natural alignment).

Every `struct X { ... };` in src/*.h is parsed; members of other structs are flattened into
dotted names and arrays are expanded element by element (`goals[1].x`, `waypoint_x[3]`).
The source directory is the repository's own src/, or the OSRC environment variable when set.

A member the parser does not understand (an unknown type, a union, a bit field, a method) is an
error, as is a layout whose size differs from what the exe wrote: fields are never guessed.
"""
import glob
import itertools
import math
import os
import re
from functools import lru_cache
from typing import Dict, List, NamedTuple, Tuple

DEFAULT_SRC = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src"))

# struct module format code (little-endian) of each basic type; size and alignment follow from it
BASIC = {"char": "b", "int8_t": "b", "uint8_t": "B",
         "int16_t": "h", "uint16_t": "H",
         "int": "i", "int32_t": "i", "BOOL": "i",
         "uint32_t": "I", "DWORD": "I", "LPSTR": "I", "LPCSTR": "I",
         "float": "f", "int64_t": "q", "uint64_t": "Q"}
SIZES = {"b": 1, "B": 1, "h": 2, "H": 2, "i": 4, "I": 4, "f": 4, "q": 8, "Q": 8}
POINTER_FORMAT = "I"


class Field(NamedTuple):
    offset: int
    size: int
    type: str
    name: str


class LayoutError(ValueError):
    pass


class _Headers(NamedTuple):
    structs: Dict[str, str]  # struct name -> body between the braces
    consts: Dict[str, int]


def source_dir() -> str:
    return os.environ.get("OSRC") or DEFAULT_SRC


def _strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


@lru_cache(maxsize=None)
def _headers(src: str) -> _Headers:
    paths = sorted(glob.glob(os.path.join(src, "*.h")))
    if not paths:
        raise LayoutError("no headers in %s" % src)
    structs = {}
    consts = {}
    for path in paths:
        with open(path, encoding="utf-8", errors="replace") as f:
            text = _strip_comments(f.read())
        # bodies hold no braces, so a one-line `typedef struct X { ... } X;` can't swallow the next struct
        for m in re.finditer(r"struct\s+(\w+)\s*\{([^{}]*)\n\};", text):
            structs[m.group(1)] = m.group(2)
        for m in re.finditer(r"(?:constexpr|const)\s+(?:int|uint32_t|size_t)\s+(\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\b",
                             text):
            consts[m.group(1)] = int(m.group(2), 0)
        for m in re.finditer(r"#define\s+(\w+)\s+\(?(\d+)\)?", text):
            consts[m.group(1)] = int(m.group(2))
        for m in re.finditer(r"\b([A-Za-z_]\w*)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*[,}]", text):
            consts.setdefault(m.group(1), int(m.group(2), 0))
    return _Headers(structs, consts)


def value_format(type_name: str) -> str:
    """struct module format code for a leaf field's type (pointers read as unsigned)."""
    if type_name.endswith("*"):
        return POINTER_FORMAT
    return BASIC[type_name]


@lru_cache(maxsize=None)
def _layout(name: str, src: str) -> Tuple[int, int, Tuple[Field, ...]]:
    """(size, alignment, leaf fields) of a struct."""
    headers = _headers(src)
    if name not in headers.structs:
        raise LayoutError("no plain `struct %s { ... };` in %s/*.h" % (name, src))
    fields = []
    offset = 0
    max_align = 1
    for decl in headers.structs[name].split(";"):
        decl = re.sub(r"\s+", " ", decl).strip()
        if not decl:
            continue
        m = re.match(r"(?:struct )?(\w+) ?(\*?) ?(\w+) ?((?:\[[^\]]+\] ?)*)$", decl)
        if not m:
            raise LayoutError("%s: cannot parse member '%s'" % (name, decl))
        type_name, pointer, member, dims_text = m.groups()
        dims = []
        for dim in re.findall(r"\[([^\]]+)\]", dims_text):
            dim = dim.strip()
            if re.fullmatch(r"0x[0-9A-Fa-f]+|\d+", dim):
                dims.append(int(dim, 0))
            elif dim in headers.consts:
                dims.append(headers.consts[dim])
            else:
                raise LayoutError("%s.%s: unknown array size '%s'" % (name, member, dim))
        if pointer:
            elem_size = align = SIZES[POINTER_FORMAT]
            elem_fields = [Field(0, elem_size, type_name + "*", "")]
        elif type_name in BASIC:
            elem_size = align = SIZES[BASIC[type_name]]
            elem_fields = [Field(0, elem_size, type_name, "")]
        elif type_name in headers.structs:
            elem_size, align, nested = _layout(type_name, src)
            elem_fields = [f._replace(name="." + f.name) for f in nested]
        else:
            raise LayoutError("%s.%s: unknown type '%s'" % (name, member, type_name))
        offset = (offset + align - 1) // align * align
        for i, index in enumerate(itertools.product(*(range(d) for d in dims))):
            elem_name = member + "".join("[%d]" % k for k in index)
            elem_offset = offset + i * elem_size
            fields.extend(f._replace(offset=elem_offset + f.offset, name=elem_name + f.name) for f in elem_fields)
        offset += elem_size * math.prod(dims)
        max_align = max(max_align, align)
    size = (offset + max_align - 1) // max_align * max_align
    return size, max_align, tuple(fields)


def layout(struct_name: str) -> List[Field]:
    """Leaf fields of a struct in offset order, nested structs and arrays expanded."""
    return list(_layout(struct_name, source_dir())[2])


def struct_size(struct_name: str) -> int:
    return _layout(struct_name, source_dir())[0]


def checked_layout(struct_name: str, elem_size: int) -> List[Field]:
    """layout(), after checking that the header's size matches the size the exe wrote."""
    size = struct_size(struct_name)
    if size != elem_size:
        raise LayoutError("%s is 0x%X bytes in %s but 0x%X in the dump: the header and the exe disagree"
                          % (struct_name, size, source_dir(), elem_size))
    return layout(struct_name)
