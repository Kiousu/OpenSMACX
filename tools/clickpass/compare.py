"""Field-by-field comparison of two click-pass state dumps.

Differences read like `Vehs[412].dmg_incurred: 3 vs 2`, `Globals.turn: 10 vs 11`,
`SavePath: <game folder>\\saves\\a vs <game folder>\\saves\\b` (a "text" section; "char32" ones the
same, by element), or, for bytes no field covers (struct padding, "bytes" sections),
`Random+0x04: 1f vs 20`. (The files a step wrote are compared by files.py.)

Command line (from tools/): python -m clickpass.compare <dump a> <dump b> [--ignore TSV] [--limit N]
prints the differences (N at least 1) and exits 1 if there are any.
"""
import argparse
import os
import re
import struct
import sys
from typing import Iterator, List, NamedTuple, Optional, Set, Tuple

from clickpass.dump import BYTES, CHAR32, GLOBAL_NAMES, GLOBALS, TEXT, U32, Dump, read_dump
from clickpass.structs import checked_layout, value_format

IGNORE_TSV = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ignore.tsv")
IGNORE_HEADER = "section\tfield\treason"
MISSING = "missing"
NAME_SECTIONS = {GLOBALS: GLOBAL_NAMES}  # u32 section -> char32 section naming its elements


class Difference(NamedTuple):
    section: str
    index: Optional[int]  # element index; None for whole-section values and named globals
    field: str  # "+0xNN" for a raw byte
    a: object
    b: object

    def __str__(self) -> str:
        where = self.section if self.index is None else "%s[%d]" % (self.section, self.index)
        if self.field.startswith("+"):
            return "%s%s: %02x vs %02x" % (where, self.field, self.a, self.b)
        if self.field:
            where += "." + self.field
        return "%s: %s vs %s" % (where, self.a, self.b)


def _raw_byte(offset: int) -> str:
    return "+0x%02X" % offset


def _text(raw: bytes) -> str:
    """A NUL-padded string: what comes before the first NUL."""
    return raw.split(b"\0", 1)[0].decode("ascii", "replace")


def _text32(data: bytes, index: int) -> str:
    return _text(data[index * 32:(index + 1) * 32])


def _struct_plan(type_name: str, elem_size: int) -> List[Tuple[int, int, str, str]]:
    """(offset, size, format, name) of every field, plus one entry per padding byte, by offset."""
    plan = []
    covered = bytearray(elem_size)
    for f in checked_layout(type_name, elem_size):
        plan.append((f.offset, f.size, value_format(f.type), f.name))
        covered[f.offset:f.offset + f.size] = b"\1" * f.size
    plan.extend((off, 1, "B", _raw_byte(off)) for off in range(elem_size) if not covered[off])
    plan.sort()
    return plan


def _diff_section(name: str, a: Dump, b: Dump) -> Iterator[Difference]:
    sa, sb = a.sections.get(name), b.sections.get(name)
    if sa is None or sb is None:
        yield Difference(name, None, "count", sa.count if sa is not None else MISSING,
                         sb.count if sb is not None else MISSING)
        return
    if sa.type != sb.type:
        yield Difference(name, None, "type", sa.type, sb.type)
        return
    if sa.elem_size != sb.elem_size:
        yield Difference(name, None, "elem_size", sa.elem_size, sb.elem_size)
        return
    plan = _struct_plan(sa.type, sa.elem_size) if sa.type not in (U32, CHAR32, BYTES, TEXT) else None
    if sa.count != sb.count:
        yield Difference(name, None, "count", sa.count, sb.count)
    size = sa.elem_size
    common = min(sa.count, sb.count) * size
    da, db = sa.data[:common], sb.data[:common]
    if da == db:
        return
    if sa.type == TEXT:
        if _text(da) != _text(db):
            yield Difference(name, None, "", _text(da), _text(db))
        return
    if sa.type == BYTES:
        for off in range(common):
            if da[off] != db[off]:
                yield Difference(name, None, _raw_byte(off), da[off], db[off])
        return
    names_section = a.sections.get(NAME_SECTIONS.get(name, ""))
    for i in range(common // size):
        base = i * size
        ea, eb = da[base:base + size], db[base:base + size]
        if ea == eb:
            continue
        if sa.type == U32:
            va, vb = struct.unpack("<I", ea)[0], struct.unpack("<I", eb)[0]
            if names_section is not None and i < names_section.count:
                yield Difference(name, None, _text32(names_section.data, i), va, vb)
            else:
                yield Difference(name, i, "", va, vb)
        elif sa.type == CHAR32:
            if _text32(da, i) != _text32(db, i):  # (what follows the NUL is not part of it)
                yield Difference(name, i, "", _text32(da, i), _text32(db, i))
        else:
            for off, fsize, fmt, fname in plan:
                if ea[off:off + fsize] != eb[off:off + fsize]:
                    yield Difference(name, i, fname, struct.unpack_from("<" + fmt, ea, off)[0],
                                     struct.unpack_from("<" + fmt, eb, off)[0])


def _ignored(d: Difference, ignore: Set[Tuple[str, str]]) -> bool:
    return (d.section, d.field) in ignore or (d.section, re.sub(r"\[\d+\]", "", d.field)) in ignore


def check_limit(limit: int) -> int:
    """limit itself; ValueError below 1, which would report no differences for dumps that differ."""
    if not isinstance(limit, int) or limit < 1:
        raise ValueError("the difference limit must be at least 1, not %r" % (limit,))
    return limit


def _limit_arg(text: str) -> int:
    try:
        return check_limit(int(text))
    except ValueError:
        raise argparse.ArgumentTypeError("must be a whole number of at least 1, not %r" % text)


def diff_dumps(a: Dump, b: Dump, ignore: Set[Tuple[str, str]], limit: int = 50) -> List[Difference]:
    """The first `limit` (at least 1) differences, in section order, skipping (section, field) pairs
    in `ignore` (a field named without indices, like `waypoint_x`, covers all its elements)."""
    check_limit(limit)
    diffs = []
    names = list(a.sections) + [n for n in b.sections if n not in a.sections]
    for name in names:
        for d in _diff_section(name, a, b):
            if not _ignored(d, ignore):
                diffs.append(d)
                if len(diffs) >= limit:
                    return diffs[:limit]
    return diffs


def read_ignore(path: str) -> Set[Tuple[str, str]]:
    """(section, field) pairs from an ignore table (`section\\tfield\\treason`, header line first)."""
    entries = set()
    with open(path, encoding="utf-8") as f:
        for ln, line in enumerate(f, 1):
            s = line.rstrip("\r\n")
            if not s.strip() or s.startswith("#") or (ln == 1 and s == IGNORE_HEADER):
                continue
            parts = s.split("\t")
            if len(parts) < 2:
                raise ValueError("%s:%d: expected <section>\\t<field>\\t<reason>" % (path, ln))
            entries.add((parts[0].strip(), parts[1].strip()))
    return entries


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="Compare two click-pass state dumps.")
    parser.add_argument("a")
    parser.add_argument("b")
    parser.add_argument("--ignore", default=IGNORE_TSV)
    parser.add_argument("--limit", type=_limit_arg, default=50, help="at most this many differences (at least 1)")
    args = parser.parse_args(argv)
    diffs = diff_dumps(read_dump(args.a), read_dump(args.b), read_ignore(args.ignore), args.limit)
    for d in diffs:
        print(d)
    if not diffs:
        print("no differences")
    return 1 if diffs else 0


if __name__ == "__main__":
    sys.exit(main())
