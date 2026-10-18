"""Redirect table handling for the click pass.

tools/redirects.tsv rows are `<file offset>\t<mangled export>`, optionally followed by
`\tapproved` for deviations Kiousu has approved. Approved rows belong to the reference exe
and are never removed by bisection.
"""
from typing import List, NamedTuple, Tuple

APPROVED = "approved"


class Row(NamedTuple):
    offset: int
    name: str
    approved: bool


def read_rows(path: str) -> List[Row]:
    """Read a redirect table, skipping blank lines and '#' comments."""
    rows = []
    with open(path, encoding="utf-8") as f:
        for ln, line in enumerate(f, 1):
            s = line.strip()
            if not s or s.startswith("#"):
                continue
            parts = s.split("\t")
            if len(parts) < 2:
                raise ValueError("%s:%d: expected <offset>\\t<mangled name>" % (path, ln))
            approved = len(parts) > 2 and parts[2].strip() == APPROVED
            rows.append(Row(int(parts[0], 16), parts[1].strip(), approved))
    return rows


def write_table(rows: List[Row], path: str) -> None:
    """Write rows in the redirects.tsv format with CRLF line endings."""
    with open(path, "w", encoding="utf-8", newline="") as f:
        for row in rows:
            line = "0x%08X\t%s" % (row.offset, row.name)
            if row.approved:
                line += "\t" + APPROVED
            f.write(line + "\r\n")


def reference_rows(rows: List[Row]) -> List[Row]:
    """The rows the reference exe carries: only the approved deviations."""
    return [row for row in rows if row.approved]


def bisect_halves(candidates: List[Row]) -> Tuple[List[Row], List[Row]]:
    """Split non-approved candidate rows into two halves, keeping their order."""
    for row in candidates:
        if row.approved:
            raise ValueError("approved row %s cannot be a bisection candidate" % row.name)
    mid = len(candidates) // 2
    return candidates[:mid], candidates[mid:]
