"""Click pass report: <pass folder>/report.md, from the pass's result.json, its cases' bisect.json and
three files next to this one.

From tools/:  python -m clickpass.clickpass report [--run <pass folder>]
(`run` writes it at the end of every pass and `bisect` rewrites it with the culprits it named.)

inventory.tsv       screen, control, case, status, note: the screens, controls and actions the pass
                    knows of; status is covered (pressed by the case(s) named), not-reachable or
                    skipped (the note says why). The Coverage section comes from it.
leftover_reads.tsv  function, offset, reads, note: replaced functions whose original reads memory it
                    never wrote, so its result depends on what ran before (offset: the function's
                    address in terranx.exe, such as 0x522180).
decisions.tsv       case (or *), function (or *), decision, date, why: what was decided for a
                    function's differences in a case (Kiousu's decisions on leftover reads, or the
                    fix loop's steps); shown as what's being done.

Sections, in this order:
  Summary                  counts per status, then every case with its save, status and step
  Failures                 every failing case (fail, crash, hang, blocked-write, setup-failed,
                           reference-broken) with its step, first differences, the culprit rows its
                           bisection named and what's being done: "held back" (the row is no longer
                           in tools/redirects.tsv), the decisions.tsv entries for those rows, else "open"
  Needs Kiousu's decision  failures whose culprit is in leftover_reads.tsv or whose bisection was not
                           clean (an interaction, or a leftover read not known yet), with what each
                           game had and why; unless decisions.tsv covers every culprit row for that
                           case, which puts the case back under Failures with the decision
  Nondeterministic cases   the reference's own two runs differ: not compared
  Coverage                 the inventory's counts and rows, with the statuses of its cases in this pass
"""
import json
import os
import re
from typing import Dict, List, NamedTuple, Optional, Sequence, Tuple

from clickpass.bisect import BISECT_JSON, BISECTABLE, TABLE, _pass_order
from clickpass.runner import CASES_DIR, FAILING, HERE, NONDETERMINISTIC, STATUSES, CaseOutcome, PassResult, case_save
from clickpass.tables import Row, read_rows, reference_rows

INVENTORY_TSV = os.path.join(HERE, "inventory.tsv")
LEFTOVERS_TSV = os.path.join(HERE, "leftover_reads.tsv")
DECISIONS_TSV = os.path.join(HERE, "decisions.tsv")
REPORT_MD = "report.md"
INVENTORY_STATUSES = ("covered", "not-reachable", "skipped")
IMAGE_BASE = 0x400000  # a redirect row's offset is in the exe file: the function's address minus this
DIFFERENCES_SHOWN = 10
CULPRITS_SHOWN = 20
SIDES = "unmodified game vs OpenSMACX"

_MANGLED = re.compile(r"^\?([A-Za-z_]\w*)((?:@[A-Za-z_]\w*)*)@@")


class InventoryRow(NamedTuple):
    screen: str
    control: str
    case: str  # the case(s) that press it, comma-separated
    status: str  # one of INVENTORY_STATUSES
    note: str


class Leftover(NamedTuple):
    function: str
    offset: int  # the function's address in terranx.exe
    reads: str  # what it reads before writing it
    note: str  # which code leaves it there, and what uses it


class Decision(NamedTuple):
    case: str  # a case name, or * for every case
    function: str  # a function name, or * for every culprit of the case
    decision: str
    date: str
    why: str


def _read_tsv(path: str, columns: Sequence[str], required: int) -> List[Tuple[int, List[str]]]:
    """(line number, fields) of every row; blank lines, '#' comments and the header are skipped, a
    short row is filled up with "", and the first `required` fields must not be empty."""
    rows = []
    with open(path, encoding="utf-8-sig") as f:
        for ln, line in enumerate(f, 1):
            text = line.rstrip("\r\n")
            if not text.strip() or text.lstrip().startswith("#"):
                continue
            fields = [s.strip() for s in text.split("\t")]
            if fields == list(columns):
                continue
            if len(fields) > len(columns) or not all(fields[:required]) or len(fields) < required:
                raise ValueError("%s:%d: expected %s" % (path, ln, "\\t".join(columns)))
            rows.append((ln, fields + [""] * (len(columns) - len(fields))))
    return rows


def read_inventory(path: str = INVENTORY_TSV) -> List[InventoryRow]:
    rows = []
    for ln, fields in _read_tsv(path, InventoryRow._fields, 2):
        row = InventoryRow(*fields)
        if row.status not in INVENTORY_STATUSES:
            raise ValueError("%s:%d: status %r is not one of %s"
                             % (path, ln, row.status, ", ".join(INVENTORY_STATUSES)))
        rows.append(row)
    return rows


def read_leftovers(path: str = LEFTOVERS_TSV) -> List[Leftover]:
    rows = []
    for ln, fields in _read_tsv(path, Leftover._fields, 2):
        try:
            offset = int(fields[1], 16)
        except ValueError:
            raise ValueError("%s:%d: offset %r is not a hex address" % (path, ln, fields[1]))
        rows.append(Leftover(fields[0], offset, fields[2], fields[3]))
    return rows


def read_decisions(path: str = DECISIONS_TSV) -> List[Decision]:
    return [Decision(*fields) for _, fields in _read_tsv(path, Decision._fields, 3)]


def plain_name(mangled: str) -> str:
    """`?alien_fauna@@YAXXZ` -> `alien_fauna`, `?sensors@Path@@...` -> `Path::sensors`; other names as
    they are."""
    m = _MANGLED.match(mangled)
    if not m:
        return mangled
    scopes = [s for s in m.group(2).split("@") if s]
    return "::".join(scopes[::-1] + [m.group(1)])


def _names(row: Row) -> Tuple[str, str]:
    return row.name, plain_name(row.name)


def _row_text(row: Row) -> str:
    return "`%s` (0x%X, row `0x%08X %s`)" % (plain_name(row.name), row.offset + IMAGE_BASE, row.offset, row.name)


def _leftover(row: Row, leftovers: Sequence[Leftover]) -> Optional[Leftover]:
    for entry in leftovers:
        if entry.function in _names(row) or entry.offset == row.offset + IMAGE_BASE:
            return entry
    return None


def _decided(case: str, row: Row, decisions: Sequence[Decision]) -> List[Decision]:
    return [d for d in decisions if d.case in ("*", case) and (d.function == "*" or d.function in _names(row))]


def _culprits(bisect: Optional[dict]) -> List[Row]:
    if not bisect or bisect.get("result") != "culprits":
        return []
    return [Row(int(c["offset"], 16), c["name"], False) for c in bisect.get("culprits") or []]


def needs_decision(o: CaseOutcome, bisect: Optional[dict], leftovers: Sequence[Leftover],
                   decisions: Sequence[Decision]) -> bool:
    """Whether a failing case goes to "Needs Kiousu's decision": a culprit reads leftover memory or the
    bisection was not clean, and decisions.tsv doesn't cover every culprit row for this case."""
    rows = _culprits(bisect)
    if not rows or o.status not in FAILING:
        return False
    if bisect.get("clean") is not False and not any(_leftover(r, leftovers) for r in rows):
        return False
    return not all(_decided(o.case, r, decisions) for r in rows)


def _cell(text) -> str:
    return str(text).replace("|", "\\|").replace("\r", " ").replace("\n", " ")


def _plural(n: int, word: str) -> str:
    return "%d %s%s" % (n, word, "" if n == 1 else "s")


def _where(o: CaseOutcome) -> str:
    return o.status if o.at_step is None else "%s at step %d" % (o.status, o.at_step)


def _save(case: str, cases_dir: str) -> str:
    try:
        return case_save(os.path.join(cases_dir, case + ".case"))
    except (OSError, ValueError):
        return "?"


def _differences(o: CaseOutcome, label: str) -> List[str]:
    if not o.differences:
        return []
    shown = [str(d) for d in o.differences[:DIFFERENCES_SHOWN]]
    lines = ["- %s (%s):" % (label, SIDES)] + ["  - `%s`" % d for d in shown]
    if len(o.differences) > len(shown):
        lines.append("  - ... %d more in the pass's result.json" % (len(o.differences) - len(shown)))
    return lines


def _culprit_lines(rows: List[Row], bisect: dict) -> List[str]:
    runs = len(bisect.get("runs") or [])
    how = "clean" if bisect.get("clean") else "not clean"
    lines = ["- Culprit%s (bisection: %s, %s): %s" % ("" if len(rows) == 1 else "s", how, _plural(runs, "run"),
                                                     ", ".join(_row_text(r) for r in rows[:CULPRITS_SHOWN]))]
    if len(rows) > CULPRITS_SHOWN:
        lines[-1] += " and %d more (bisect.json)" % (len(rows) - CULPRITS_SHOWN)
    return lines


def _bisect_warnings(bisect: Optional[dict]) -> List[str]:
    lines = []
    if bisect and bisect.get("exe_is_full_table") is False:
        lines.append("- WARNING: the OpenSMACX folder's exe was not the redirect table's patch when this was "
                     "bisected: the pass played another exe than the table bisected")
    if bisect and bisect.get("cleanup"):
        lines.append("- WARNING: the bisect could not remove %s: `clickpass.py bisect --recover`" % bisect["cleanup"])
    return lines


def _being_done(case: str, rows: List[Row], decisions: Sequence[Decision], table: Optional[List[Row]]) -> str:
    parts = []
    if table is not None and rows:
        present = {(r.offset, r.name) for r in table}
        out = [r for r in rows if (r.offset, r.name) not in present]
        if out:
            parts.append("held back" + ("" if len(out) == len(rows) else " (%s)" % ", ".join(
                plain_name(r.name) for r in out)))
    seen = []
    for row in rows:
        for d in _decided(case, row, decisions):
            if d not in seen:
                seen.append(d)
    parts += ["decided %s: %s%s" % (d.date, d.decision, "; " + d.why if d.why else "") for d in seen]
    return "; ".join(parts) if parts else "open"


def _failure(o: CaseOutcome, bisect: Optional[dict], decisions, table, cases_dir) -> List[str]:
    lines = ["### %s: %s" % (o.case, _where(o)), "",
             "- On %s: %s" % (_save(o.case, cases_dir), o.detail or o.status)]
    lines += _differences(o, "First differences")
    rows = _culprits(bisect)
    if rows:
        lines += _culprit_lines(rows, bisect)
    elif bisect:
        lines.append("- Culprit: none named; the bisection's result: %s%s" % (
            bisect.get("result") or "?", ": " + bisect["message"] if bisect.get("message") else ""))
    elif o.status in BISECTABLE:
        lines.append("- Culprit: not bisected yet (`clickpass.py bisect %s`)" % o.case)
    else:
        lines.append("- Culprit: none to look for: the case could not be played (%s)" % o.status)
    lines += _bisect_warnings(bisect)
    lines += ["- Being done: %s" % _being_done(o.case, rows, decisions, table), ""]
    return lines


def _decision(o: CaseOutcome, bisect: dict, leftovers, cases_dir) -> List[str]:
    rows = _culprits(bisect)
    lines = ["### %s: %s" % (o.case, _where(o)), "",
             "- On %s: %s" % (_save(o.case, cases_dir), o.detail or o.status)]
    lines += _culprit_lines(rows, bisect)
    for row in rows:
        entry = _leftover(row, leftovers)
        if entry:
            lines.append("- Why: `%s` (0x%X) reads memory it never wrote: %s%s (from leftover_reads.tsv)"
                         % (plain_name(row.name), entry.offset, entry.reads or "?", "; " + entry.note if entry.note
                            else ""))
    if bisect.get("clean") is False:
        lines.append("- Why: the bisection did not narrow cleanly: neither half of this set reproduces the failure "
                     "alone, so these rows act together, or one of them reads memory it never wrote (a leftover "
                     "read not yet in leftover_reads.tsv)")
    lines += _differences(o, "What each game had, first differences") or [
        "- What each game had: no state differences (the failure is the %s)" % o.status]
    lines += _bisect_warnings(bisect)
    lines.append("")
    return lines


def _header(pass_result: PassResult, info: dict, table: Optional[List[Row]]) -> List[str]:
    run_dir = pass_result.run_dir
    lines = ["# Click pass report: %s" % os.path.basename(os.path.normpath(run_dir)), "",
             "Pass folder `%s`%s." % (run_dir, ", started %s" % info["started"] if info.get("started") else "")]
    if info.get("ref_dir") or info.get("osx_dir"):
        lines[-1] += " Reference game `%s`, OpenSMACX game `%s`." % (info.get("ref_dir", "?"), info.get("osx_dir", "?"))
    if table is not None:
        approved = [plain_name(r.name) for r in reference_rows(table)]
        lines[-1] += (" The reference is the unmodified exe with only the approved deviations: %s."
                      % (", ".join(approved) or "none"))
    for warning in info.get("warnings") or []:
        lines += ["", "**%s**" % warning]
    return lines + [""]


def _summary(outcomes: List[CaseOutcome], cases_dir: str) -> List[str]:
    counts = {}
    for o in outcomes:
        counts[o.status] = counts.get(o.status, 0) + 1
    order = [s for s in STATUSES if s in counts] + sorted(s for s in counts if s not in STATUSES)
    lines = ["## Summary", "", "%s: %s." % (_plural(len(outcomes), "case"),
                                            ", ".join("%d %s" % (counts[s], s) for s in order) or "none run")]
    if outcomes:
        lines += ["", "| case | save | status | step | time | reference |", "|---|---|---|---|---|---|"]
        for o in outcomes:
            reference = {"hit": "cached", "miss": "recorded"}.get(o.cache, o.cache)
            lines.append("| %s | %s | %s | %s | %.1f s | %s |" % (
                _cell(o.case), _cell(_save(o.case, cases_dir)), o.status, "" if o.at_step is None else o.at_step,
                o.seconds, reference))
    return lines + [""]


def _coverage(inventory: List[InventoryRow], outcomes: List[CaseOutcome]) -> List[str]:
    lines = ["## Coverage", ""]
    if not inventory:
        return lines + ["The inventory (tools/clickpass/inventory.tsv) is empty: nothing is listed as covered.", ""]
    counts = [(s, sum(1 for r in inventory if r.status == s)) for s in INVENTORY_STATUSES]
    lines.append("Inventory (tools/clickpass/inventory.tsv): %s. Controls not listed there are not covered."
                 % ", ".join("%d %s" % (n, s.replace("-", " ")) for s, n in counts if n))
    status = {o.case: o.status for o in outcomes}
    lines += ["", "| screen | control | status | its cases in this pass | note |", "|---|---|---|---|---|"]
    for r in inventory:
        cases = [c.strip() for c in r.case.split(",") if c.strip()]
        ran = ", ".join("%s %s" % (c, status.get(c, "not run")) for c in cases)
        lines.append("| %s | %s | %s | %s | %s |" % (_cell(r.screen), _cell(r.control), r.status.replace("-", " "),
                                                     _cell(ran), _cell(r.note)))
    return lines + [""]


def write_report(pass_result: PassResult, bisects: Dict[str, dict], inventory: List[InventoryRow],
                 leftovers: List[Leftover], decisions: List[Decision], path: str, table: Optional[List[Row]] = None,
                 info: Optional[dict] = None, cases_dir: str = CASES_DIR) -> None:
    """Writes report.md (CRLF) for a pass: bisects maps a case to its bisect.json data, table is the
    current redirect table (for "held back" and the approved deviations), info the pass's result.json
    (started, folders, warnings)."""
    outcomes = pass_result.outcomes
    lines = _header(pass_result, info or {}, table) + _summary(outcomes, cases_dir)
    to_decide, failures = [], []
    for o in outcomes:
        if o.status in FAILING:
            (to_decide if needs_decision(o, bisects.get(o.case), leftovers, decisions) else failures).append(o)
    lines += ["## Failures", ""]
    for o in failures:
        lines += _failure(o, bisects.get(o.case), decisions, table, cases_dir)
    if not failures:
        lines += ["None.", ""]
    lines += ["## Needs Kiousu's decision", ""]
    if to_decide:
        lines += ["The replaced function behind each of these reads memory it never wrote, or the bisection could "
                  "not narrow it to rows that fail alone. Kiousu decides each: make the function do what its code "
                  "evidently means, match the original's leftovers, or accept the difference; the decision goes "
                  "into tools/clickpass/decisions.tsv and the case is not raised again.", ""]
        for o in to_decide:
            lines += _decision(o, bisects[o.case], leftovers, cases_dir)
    else:
        lines += ["None.", ""]
    lines += ["## Nondeterministic cases", ""]
    nondeterministic = [o for o in outcomes if o.status == NONDETERMINISTIC]
    for o in nondeterministic:
        first = ", ".join("`%s`" % str(d) for d in o.differences[:DIFFERENCES_SHOWN])  # (a Difference is a tuple)
        lines.append("- %s (save %s): %s%s" % (o.case, _save(o.case, cases_dir), o.detail or "the reference's two "
                                               "runs differ", "; first differences: " + first if first else ""))
    lines += [""] if nondeterministic else ["None.", ""]
    lines += _coverage(inventory, outcomes)
    with open(path, "w", encoding="utf-8", newline="\r\n") as f:
        f.write("\n".join(lines).rstrip("\n") + "\n")


def load_pass(run_dir: str) -> Tuple[PassResult, dict]:
    """A pass's outcomes from its result.json (differences as text), and the whole file."""
    with open(os.path.join(run_dir, "result.json"), encoding="utf-8") as f:
        data = json.load(f)
    outcomes = [CaseOutcome(e["case"], e["status"], e.get("at_step"), list(e.get("differences") or []),
                            e.get("detail") or "", e.get("cache") or "", e.get("key") or "",
                            float(e.get("seconds") or 0)) for e in data.get("cases", [])]
    return PassResult(run_dir, outcomes), data


def load_bisects(run_dir: str, outcomes: List[CaseOutcome]) -> Dict[str, dict]:
    """Each case's <run_dir>/<case>/bisect.json; one that can't be read stands as an error."""
    bisects = {}
    for o in outcomes:
        path = os.path.join(run_dir, o.case, BISECT_JSON)
        if not os.path.exists(path):
            continue
        try:
            with open(path, encoding="utf-8") as f:
                bisects[o.case] = json.load(f)
        except (OSError, ValueError) as e:
            bisects[o.case] = {"result": "error", "message": "%s can't be read: %s" % (path, e)}
    return bisects


def newest_pass(out_root: str) -> str:
    """The newest pass folder (<YYYY-MM-DD-HHMM>[-n]) in out_root with a result.json."""
    names = os.listdir(out_root) if os.path.isdir(out_root) else []
    for name in sorted((n for n in names if _pass_order(n)), key=_pass_order, reverse=True):
        if os.path.isfile(os.path.join(out_root, name, "result.json")):
            return os.path.join(out_root, name)
    raise ValueError("no pass in %s (clickpass.py run)" % out_root)


def report_pass(run_dir: str, table: Optional[str] = TABLE, inventory: str = INVENTORY_TSV,
                leftovers: str = LEFTOVERS_TSV, decisions: str = DECISIONS_TSV, cases_dir: str = CASES_DIR) -> str:
    """Writes <run_dir>/report.md from the pass's files and the three tables; returns its path."""
    pass_result, info = load_pass(run_dir)
    path = os.path.join(run_dir, REPORT_MD)
    write_report(pass_result, load_bisects(run_dir, pass_result.outcomes), read_inventory(inventory),
                 read_leftovers(leftovers), read_decisions(decisions), path,
                 table=read_rows(table) if table else None, info=info, cases_dir=cases_dir)
    return path
