"""The files a case's game wrote, compared between two runs.

With every dump case.ps1 writes <run folder>/files_<step>.json: {"step": n, "files": [...]}, one entry
per file under the game folder ("game\\<path>") or the outside folder ("outside\\<path>") that is new,
changed or gone since just before the launch:
  {"path", "size", "sha256"}   its content is kept as <run folder>/files/<sha256>
  {"path", "deleted": true}
  {"path", "error"}            it could not be read
Two runs' lists for a step are compared by name, and the contents byte by byte, except for the bytes
file_masks.tsv masks. Differences read like the dumps' (compare.Difference):
  files[game\\saves\\_cp_save.SAV]+0x144C8: 1f vs 20
  files[game\\saves\\_cp_save.SAV].size: 686926 vs 686930
  files[outside\\x.SAV]: written (686926 bytes) vs not written
A file that could not be read never compares equal ("unreadable (...)").

file_masks.tsv (header `pattern\\toffset\\tlength\\treason`, '#' comments):
  pattern  fnmatch pattern for the path, any case (`*.SAV`, `game\\Alpha Centauri.ini`)
  offset   where the masked bytes start: a number (0x561 or 1377), or `after:<text>`, right after
           the first <text> in the file (found in each file by itself; no <text>, no mask)
  length   how many bytes: a number, or `eol`, up to the end of that line
  reason   why those bytes may differ between the two games (required)
"""
import fnmatch
import json
import os
import re
from typing import Dict, List, NamedTuple, Sequence, Tuple, Union

from clickpass.compare import Difference, check_limit

HERE = os.path.dirname(os.path.abspath(__file__))
MASKS_TSV = os.path.join(HERE, "file_masks.tsv")
MASKS_HEADER = ["pattern", "offset", "length", "reason"]
COPIES = "files"  # the run folder's subfolder with the contents
AFTER = "after:"
EOL = "eol"

_DUMP_NAME = re.compile(r"^dump_(\d+)\.bin$")
_NUMBER = re.compile(r"^(0x[0-9A-Fa-f]+|\d+)$")


class Mask(NamedTuple):
    pattern: str
    offset: Union[int, str]  # a number, or "after:<text>"
    length: Union[int, str]  # a number, or "eol"
    reason: str


def read_masks(path: str = MASKS_TSV) -> List[Mask]:
    """The masks of a file_masks.tsv; ValueError for a row that isn't one."""
    masks = []
    with open(path, encoding="utf-8-sig") as f:
        for ln, line in enumerate(f, 1):
            text = line.rstrip("\r\n")
            if not text.strip() or text.lstrip().startswith("#"):
                continue
            fields = [s.strip() for s in text.split("\t")]
            if fields == MASKS_HEADER:
                continue
            where = "%s:%d: " % (path, ln)
            if len(fields) != 4 or not fields[0] or not fields[3]:
                raise ValueError(where + "expected pattern\\toffset\\tlength\\treason, every one given")
            offset, length = fields[1], fields[2]
            if _NUMBER.match(offset):
                offset = int(offset, 0)
            elif not offset.startswith(AFTER) or offset == AFTER:
                raise ValueError(where + "offset %r is neither a number nor after:<text>" % offset)
            if _NUMBER.match(length) and int(length, 0) > 0:
                length = int(length, 0)
            elif length != EOL:
                raise ValueError(where + "length %r is neither a number above 0 nor eol" % length)
            masks.append(Mask(fields[0], offset, length, fields[3]))
    return masks


def _matches(path: str, pattern: str) -> bool:
    return fnmatch.fnmatchcase(path.lower(), pattern.lower())


def masked_ranges(path: str, data: bytes, masks: Sequence[Mask]) -> List[Tuple[int, int]]:
    """(start, end) of every byte range the masks hide in this file's content, in mask order."""
    ranges = []
    for m in masks:
        if not _matches(path, m.pattern):
            continue
        if isinstance(m.offset, int):
            start = m.offset
        else:
            anchor = m.offset[len(AFTER):].encode("latin-1")
            found = data.find(anchor)
            if found < 0:
                continue
            start = found + len(anchor)
        if m.length == EOL:
            ends = [i for i in (data.find(b"\r", start), data.find(b"\n", start)) if i >= 0]
            end = min(ends) if ends else len(data)
        else:
            end = start + m.length
        ranges.append((start, end))
    return ranges


def manifest_for(dump: str) -> str:
    """The files_<step>.json next to a dump_<step>.bin."""
    m = _DUMP_NAME.match(os.path.basename(dump))
    if not m:
        raise ValueError("%s is not named dump_<step>.bin" % dump)
    return os.path.join(os.path.dirname(dump), "files_%s.json" % m.group(1))


def _read_manifest(path: str) -> Dict[str, dict]:
    """A files_<step>.json's entries by path (any case); OSError or ValueError when it can't be read."""
    with open(path, encoding="utf-8-sig") as f:
        data = json.load(f)
    entries = {}
    try:
        for e in data["files"]:
            if not isinstance(e.get("path"), str) or not (e.get("deleted") is True or "error" in e
                                                           or isinstance(e.get("sha256"), str)):
                raise ValueError("bad entry %r" % (e,))
            entries[e["path"].lower()] = e
    except (KeyError, TypeError, AttributeError) as e:
        raise ValueError("%s is not a file list: %s" % (path, e))
    return entries


def run_copies(manifest: str) -> List[str]:
    """The content copies a files_<step>.json refers to (they need not exist)."""
    folder = os.path.join(os.path.dirname(manifest), COPIES)
    return [os.path.join(folder, e["sha256"]) for e in _read_manifest(manifest).values() if "sha256" in e]


def _describe(e) -> str:
    if e is None:
        return "not written"
    if e.get("deleted"):
        return "deleted"
    if "error" in e:
        return "unreadable (%s)" % e["error"]
    return "written (%d bytes)" % e["size"]


def _content(manifest: str, e: dict) -> bytes:
    copy = os.path.join(os.path.dirname(manifest), COPIES, e["sha256"])
    try:
        with open(copy, "rb") as f:
            return f.read()
    except OSError as err:
        raise ValueError("%s: the content of %s is missing (%s)" % (manifest, e["path"], err))


def diff_files(a_manifest: str, b_manifest: str, masks: Sequence[Mask], limit: int = 50) -> List[Difference]:
    """The first `limit` (at least 1) differences between two runs' files_<step>.json, by path."""
    check_limit(limit)
    a, b = _read_manifest(a_manifest), _read_manifest(b_manifest)
    diffs = []
    for key in sorted(set(a) | set(b)):
        ea, eb = a.get(key), b.get(key)
        section = "files[%s]" % (ea or eb)["path"]
        if ea is not None and eb is not None and "sha256" in ea and "sha256" in eb:
            if ea["sha256"] == eb["sha256"]:
                continue
            da, db = _content(a_manifest, ea), _content(b_manifest, eb)
            if len(da) != len(db):
                diffs.append(Difference(section, None, "size", len(da), len(db)))
            hidden = bytearray(max(len(da), len(db)))
            for start, end in masked_ranges(ea["path"], da, masks) + masked_ranges(eb["path"], db, masks):
                hidden[start:end] = b"\1" * (min(end, len(hidden)) - min(start, len(hidden)))
            for off in range(min(len(da), len(db))):
                if da[off] != db[off] and not hidden[off]:
                    diffs.append(Difference(section, None, "+0x%02X" % off, da[off], db[off]))
                    if len(diffs) >= limit:
                        break
        elif (ea is None) != (eb is None) or "sha256" in (ea or eb) or "error" in ea or "error" in eb \
                or bool(ea.get("deleted")) != bool(eb.get("deleted")):
            diffs.append(Difference(section, None, "", _describe(ea), _describe(eb)))
        if len(diffs) >= limit:
            return diffs[:limit]
    return diffs
