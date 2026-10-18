"""Click pass bisection: names the redirect row(s) behind a case that failed in a pass.

From tools/:  python -m clickpass.clickpass bisect <case> [--run <pass folder>]

The case is played again in the OpenSMACX folder with exes patched from subsets of the redirect
table (tools/redirects.tsv, through opensmacx_patch.py -t, from the pristine v2.1 exe), and each run
is judged against the reference dumps and files the pass compared with (its cache entry, which must
still be current): a run reproduces the failure when it fails the same way at the same step, that is a
difference first seen at the pass's failing step, or a crash, hang or blocked write at that step
when that was the failure. A subset run that crashes, hangs or blocks a write when the failure was
something else (or at another step) is recorded and counted as not reproducing; a run that can't be
played (setup-failed) stops the bisection.

The approved-deviation rows (tables.reference_rows) are in every subset and are never culprits.
Two runs first check the ends: the full table must reproduce the failure and the approved rows
alone must not. find_culprits then searches the other rows (ruling R6).

The OpenSMACX folder's exe is swapped for each run (ExeSwap) and put back, checked by its sha256,
however the bisection ends (an error, Ctrl+C). Meanwhile the folder holds the original exe's copy
and a note (runner.SWAP_NOTE) naming the bisect and every exe it installs; `run` refuses the folder
while the note is there. If the bisect was killed, the next bisect of a pass in that folder, from
any out root, or `bisect --recover`, deals with it first (recover_swap): the original goes back
only when the exe is one the bisect installed, else (a batch installed one since) it stays.

Results: <pass folder>/<case>/bisect.json and a folder per run, <pass folder>/<case>/bisect/<nn>/
(table.tsv, the case driver's output and dumps).
"""
import contextlib
import functools
import hashlib
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from datetime import datetime
from typing import Callable, List, NamedTuple, Optional, Sequence, Set, Tuple

from clickpass.compare import IGNORE_TSV, read_ignore
from clickpass.files import MASKS_TSV, Mask, read_masks
from clickpass.runner import (DLL, DRIVER_FILES, HERE, LIVE_DIR, NONDETERMINISTIC, REF_EXE, REFERENCE, SWAP_NOTE,
                              CaseResult, case_save, compare_runs, describe_run, driver_fingerprint, games_running_in,
                              overlaps, process_start, read_meta, reference_key, run_case, stale_reason)
from clickpass.tables import Row, bisect_halves, read_rows, reference_rows, write_table

TOOLS = os.path.dirname(HERE)
TABLE = os.path.join(TOOLS, "redirects.tsv")
PATCHER = os.path.join(TOOLS, "opensmacx_patch.py")
PRISTINE_EXE = os.path.join(LIVE_DIR, "_backup_v2.1", "terranx.exe")  # patch input only, read (no exe is
# ever swapped in or under the live folder)
EXE = REF_EXE  # the patched exe's name, the same in both test folders
BISECTABLE = ("fail", "crash", "hang", "blocked-write")
DONE = ("no difference to bisect", "nothing to bisect", "culprits")  # results that aren't errors
BISECT_JSON = "bisect.json"
SWAP_ORIGINAL = "clickpass_bisect_original.exe"  # the original's copy, beside the note in the game folder

_PASS_NAME = re.compile(r"^(\d{4}-\d{2}-\d{2}-\d{4})(?:-(\d+))?$")


class BisectError(Exception):
    """The bisection can't start or go on; the message says why."""


class BisectResult(NamedTuple):
    culprits: List[Row]
    runs: int  # reproduces() calls
    clean: bool  # each culprit reproduces the failure alone (False: an interaction, see find_culprits)


class Failure(NamedTuple):
    status: str  # the pass's status for the case: fail, crash, hang or blocked-write
    at_step: Optional[int]


def subset_table(rows: Sequence[Row], subset: Sequence[Row]) -> List[Row]:
    """The table for a subset: every approved row and the subset's rows, in the order of `rows`."""
    keep = set(subset)
    return [row for row in rows if row.approved or row in keep]


def find_culprits(rows: List[Row], reproduces: Callable[[List[Row]], bool]) -> BisectResult:
    """The candidate (not approved) rows behind a failure. Assumes that all of `rows` reproduce it
    and the approved rows alone don't (the bisect command checks both ends first). reproduces() is
    given the approved rows plus some candidates, in table order; approved rows are never culprits.

    Ruling R6: halve the candidates and test the first half; go into it if it reproduces, else into
    the second half; once a row is found, one run of all remaining candidates without it, and if
    that still reproduces, search them again. The second half is entered without its own run: when
    the row it comes down to reproduces alone, every half on the way (each holds that row) does too.
    When it doesn't, the halves entered that way are tested from the outside in, and the first that
    doesn't reproduce sits at a split where neither half does: an interaction. Its parent set is
    reported as the culprits and `clean` is False. One culprit among 700 rows takes about ten runs.
    """
    if not any(not row.approved for row in rows):
        return BisectResult([], 0, False)
    runs = 0

    def test(subset: List[Row]) -> bool:
        nonlocal runs
        runs += 1
        return bool(reproduces(subset_table(rows, subset)))

    def search(known: List[Row]) -> Tuple[List[Row], bool]:
        """Halves `known`, which reproduces, down to one row: (culprits, clean)."""
        current = known
        untested = []  # (set, its second half) for each second half entered without a run, outermost first
        while len(current) > 1:
            first, second = bisect_halves(current)
            if test(first):
                current, untested = first, []
            else:
                untested.append((current, second))
                current = second
        if not untested or test(current):
            return current, True
        for parent, second in untested[:-1]:  # (the last second half is `current`: it doesn't reproduce)
            if not test(second):
                return parent, False
        return untested[-1][0], False

    culprits, clean = [], True
    remaining = [row for row in rows if not row.approved]
    while remaining:
        found, found_clean = search(remaining)
        culprits += found
        clean = clean and found_clean
        gone = set(found)
        remaining = [row for row in remaining if row not in gone]
        if not remaining or not test(remaining):
            break
    return BisectResult(culprits, runs, clean)


def _failure_text(failure: Failure) -> str:
    return "%s at step %s" % ("a difference" if failure.status == "fail" else failure.status, failure.at_step)


def judge(r: CaseResult, failure: Failure, reference: List[str], ignore: Set[Tuple[str, str]],
          masks: Optional[Sequence[Mask]] = None) -> Tuple[bool, str]:
    """Whether a subset's run reproduces the failure, and what the run did, in words. A crash, hang or
    blocked write reproduces only a failure of that kind at that step; a run that couldn't be played
    raises BisectError. The run's dumps and files are compared as in the pass (masks: the file masks,
    default file_masks.tsv)."""
    if r.outcome not in ("ok",) + BISECTABLE[1:]:
        raise BisectError("the case could not be played: %s" % describe_run(r))
    not_it = "; the failure was %s: counted as not reproducing" % _failure_text(failure)
    if r.outcome != "ok":
        what = describe_run(r)
        if r.outcome == failure.status and r.at_step == failure.at_step:
            return True, what
        return False, what + not_it
    try:
        step, diffs = compare_runs(reference, r.dumps, ignore, limit=3, masks=masks)
    except (OSError, ValueError) as e:
        raise BisectError("the run's dumps can't be compared with the reference: %s" % e)
    if step is None:
        return False, "no differences"
    what = "differs from step %d on (%s)" % (step, "; ".join(str(d) for d in diffs))
    if failure.status == "fail" and step == failure.at_step:
        return True, what
    return False, what + not_it


def _sha256(path: str) -> Optional[str]:
    """The file's sha256 (hex), or None when it can't be read."""
    try:
        with open(path, "rb") as f:
            return hashlib.sha256(f.read()).hexdigest()
    except OSError:
        return None


def _write_json(path: str, data: dict) -> None:
    """Writes data as JSON (CRLF line ends) to a .tmp file, then moves it over path, so that the file
    is never half written."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\r\n") as f:
        json.dump(data, f, indent=2)
        f.write("\n")
    os.replace(tmp, path)


@contextlib.contextmanager
def _ctrl_c_ignored():
    """Ctrl+C is ignored inside (while the exe is put back); outside the main thread nothing changes."""
    try:
        previous = signal.signal(signal.SIGINT, signal.SIG_IGN)
    except ValueError:
        yield
        return
    try:
        yield
    finally:
        signal.signal(signal.SIGINT, previous if previous is not None else signal.default_int_handler)


COPY_TRIES = 10  # a game that has only just ended, or a virus scan of the new exe, can hold the file


def copy_retrying(copy: Callable[[str, str], object], source: str, target: str, tries: int = COPY_TRIES,
                  wait: float = 1.0, sleep: Callable[[float], None] = time.sleep) -> None:
    """copy(source, target), tried again up to `tries` times, `wait` s apart, while the target is in
    use (PermissionError: a sharing violation); the last failure is raised."""
    for attempt in range(tries):
        try:
            copy(source, target)
            return
        except PermissionError:
            if attempt == tries - 1:
                raise
            sleep(wait)


def _copy_checked(source: str, exe: str, sha256: str, backup_note: str) -> None:
    """Copies the original back over exe (with its timestamps) and checks its sha256."""
    try:
        copy_retrying(shutil.copy2, source, exe)
    except OSError as e:
        raise BisectError("could NOT put %s back (%s): %s" % (exe, e, backup_note))
    if _sha256(exe) != sha256:
        raise BisectError("could NOT put %s back: it doesn't match sha256 %s after the copy; %s"
                          % (exe, sha256, backup_note))


def _remove_all(paths: Sequence[str]) -> str:
    """Removes those of the files that exist; says which couldn't be removed ("" when none)."""
    failed = []
    for path in paths:
        try:
            if os.path.exists(path):
                os.remove(path)
        except OSError as e:
            failed.append("%s (%s)" % (path, e))
    return "; ".join(failed)


class ExeSwap:
    """Swaps a game folder's exe for subset exes (install) and puts the original back when the `with`
    ends in any way, checked by its sha256. Meanwhile the folder holds the original's copy
    (SWAP_ORIGINAL) and the note SWAP_NOTE: the exe, its sha256, this process (pid and start time) and
    the sha256 of every exe installed, written before it is installed. `run` refuses the folder while
    the note is there; if this process is killed before it puts the exe back, recover_swap does."""

    def __init__(self, exe: str):
        self.exe = os.path.abspath(exe)
        folder = os.path.dirname(self.exe)
        self.backup = os.path.join(folder, SWAP_ORIGINAL)
        self.marker = os.path.join(folder, SWAP_NOTE)
        self.sha256 = ""
        self.note = {}
        self.swapped = False  # the copy and the note exist: the exe may differ from the original
        self.restored = False  # the original is back, checked
        self.cleanup_error = ""  # the copy or note that couldn't be removed after the restore

    def __enter__(self) -> "ExeSwap":
        if os.path.exists(self.marker):
            raise BisectError("%s exists: a bisect has swapped this folder's exe and not put it back" % self.marker)
        pid = os.getpid()
        start = process_start(pid)
        if start is None:  # (without it, another bisect's recover_swap couldn't tell that this one still runs)
            raise BisectError("this process's start time can't be read for the note: no exe is swapped")
        self.sha256 = _sha256(self.exe)
        if self.sha256 is None:
            raise BisectError("%s can't be read" % self.exe)
        try:
            shutil.copy2(self.exe, self.backup)  # (with its timestamps, which the restore puts back too)
        except OSError as e:
            _remove_all([self.backup])
            raise BisectError("can't copy %s to %s: %s" % (self.exe, self.backup, e))
        if _sha256(self.backup) != self.sha256:
            _remove_all([self.backup])
            raise BisectError("the copy %s of %s doesn't match it" % (self.backup, self.exe))
        self.note = {"exe": self.exe, "sha256": self.sha256, "pid": pid, "start": start, "installed": []}
        try:
            _write_json(self.marker, self.note)
        except OSError as e:
            _remove_all([self.backup, self.marker])
            raise BisectError("can't write %s: %s" % (self.marker, e))
        self.swapped = True
        return self

    def install(self, path: str) -> None:
        """Puts the exe at `path` in place of the folder's exe, its sha256 into the note first."""
        if not self.swapped:
            raise BisectError("ExeSwap.install outside its `with`")
        sha256 = _sha256(path)
        if sha256 is None:
            raise BisectError("%s can't be read" % path)
        if sha256 not in self.note["installed"]:
            self.note["installed"].append(sha256)
            try:
                _write_json(self.marker, self.note)
            except OSError as e:
                raise BisectError("can't add the next exe to %s, so it isn't installed: %s" % (self.marker, e))
        try:
            copy_retrying(shutil.copyfile, path, self.exe)
        except OSError as e:
            raise BisectError("can't install a subset's exe as %s: %s" % (self.exe, e))

    def restore(self) -> None:
        """Puts the original exe back (when it isn't there) and checks it; raises BisectError, keeping
        the copy and the note, when it can't. Then removes both; one that can't be removed is only
        noted (cleanup_error), for recover_swap to remove later."""
        if not self.swapped or self.restored:
            return
        if _sha256(self.exe) != self.sha256:
            _copy_checked(self.backup, self.exe, self.sha256, "the original is %s (sha256 %s); the next bisect or "
                          "`clickpass.py bisect --recover` puts it back" % (self.backup, self.sha256))
        self.restored = True
        self.cleanup_error = _remove_all([self.backup, self.marker])

    def __exit__(self, *exc) -> bool:
        with _ctrl_c_ignored():
            self.restore()
        return False


def _running(pid: int, start: int) -> bool:
    return process_start(pid) == start


def recover_swap(game_dir: str, running: Optional[Callable[[int, int], bool]] = None) -> str:
    """Deals with the note a bisect left in game_dir because it was ended before it could put the exe
    back. The original goes back only when the exe is one the bisect installed; when it is neither
    that nor the original, something else (a batch) has installed it since and it is left alone.
    Then the note and the copy are removed. Refuses while that bisect still runs. Says what it did
    ("" when there is no note)."""
    marker = os.path.join(game_dir, SWAP_NOTE)
    if not os.path.exists(marker):
        return ""
    try:
        with open(marker, encoding="utf-8") as f:
            note = json.load(f)
        name, sha256, pid, start = os.path.basename(note["exe"]), note["sha256"], int(note["pid"]), int(note["start"])
        installed = [str(s) for s in note.get("installed", [])]
    except (OSError, ValueError, KeyError, TypeError) as e:
        raise BisectError("%s can't be read (%s): check the folder's exe by hand, then delete the note"
                          % (marker, e))
    if (running or _running)(pid, start):
        raise BisectError("the bisect with pid %d still runs with %s swapped (%s)" % (pid, game_dir, marker))
    exe = os.path.join(game_dir, name)
    backup = os.path.join(game_dir, SWAP_ORIGINAL)
    current = _sha256(exe)
    if current is None:
        raise BisectError("%s is missing or can't be read: if %s (sha256 %s) is still the right exe, copy it "
                          "there, then delete %s" % (exe, backup, sha256, marker))
    if current == sha256:
        text = "%s was already the original (sha256 %s)" % (exe, sha256)
    elif current in installed:
        if _sha256(backup) != sha256:
            raise BisectError("%s is an exe a bisect (pid %d) installed, and the original's copy %s doesn't match "
                              "sha256 %s: put the original back by hand (opensmacx_patch.py with the full table), "
                              "then delete %s" % (exe, pid, backup, sha256, marker))
        _copy_checked(backup, exe, sha256, "the original is %s (sha256 %s)" % (backup, sha256))
        text = "restored %s (sha256 %s), which an ended bisect (pid %d) had left swapped" % (exe, sha256, pid)
    else:
        text = ("left %s alone: it is neither its original (sha256 %s) nor an exe the ended bisect (pid %d) "
                "installed, so something else put it there since (sha256 %s)" % (exe, sha256, pid, current))
    failed = _remove_all([backup, marker])
    if failed:
        raise BisectError("%s; but %s could not be removed: delete it by hand" % (text, failed))
    return text + "; the bisect's note is removed"


def exe_table_warning(game_dir: str, table: str = TABLE, pristine: str = PRISTINE_EXE, patch=None) -> str:
    """"" when game_dir's exe is the table's patch (opensmacx_patch.py with game_dir's DLL), else a
    WARNING line: the cases there don't play that table."""
    exe = os.path.join(game_dir, EXE)
    patch = patch or functools.partial(patch_exe, pristine=pristine, dll=os.path.join(game_dir, DLL))
    try:
        with tempfile.TemporaryDirectory(prefix="clickpass-check-") as tmp:
            want = _sha256(patch(table, tmp))
    except (BisectError, OSError) as e:
        return "WARNING: can't check that %s is %s's patch: %s" % (exe, table, e)
    have = _sha256(exe)
    if have == want:
        return ""
    return ("WARNING: %s (sha256 %s) is not %s's patch (sha256 %s): the OpenSMACX side does not play that table"
            % (exe, have, table, want))


def _reference_patch(table: str, out_dir: str, patch: Callable[[str, str], str]) -> str:
    """The pristine exe patched with only the table's approved rows (tables.reference_rows), into
    out_dir; returns the exe's path."""
    rows = reference_rows(read_rows(table))
    table_path = os.path.join(out_dir, "reference.tsv")
    write_table(rows, table_path)
    exe_dir = os.path.join(out_dir, "exe")
    os.makedirs(exe_dir)
    return patch(table_path, exe_dir)


def reference_exe_refusal(ref_dir: str, table: str = TABLE, pristine: str = PRISTINE_EXE, patch=None) -> str:
    """"" when ref_dir's exe is the pristine exe patched with only the table's approved rows (with
    ref_dir's own DLL, ruling R19), else why no pass may use that folder as the reference (also
    when it can't be checked). `clickpass.py reference` builds it."""
    exe = os.path.join(ref_dir, EXE)
    patch = patch or functools.partial(patch_exe, pristine=pristine, dll=os.path.join(ref_dir, DLL))
    try:
        with tempfile.TemporaryDirectory(prefix="clickpass-reference-") as tmp:
            want = _sha256(_reference_patch(table, tmp, patch))
    except (BisectError, OSError, ValueError) as e:
        return "can't check that the reference exe %s is the approved rows' patch: %s" % (exe, e)
    have = _sha256(exe)
    if have != want:
        return ("the reference exe %s (sha256 %s) is not the pristine exe patched with only the approved rows of %s "
                "(sha256 %s): the reference would not be the unmodified game; build it with `clickpass.py reference`"
                % (exe, have, table, want))
    return ""


def install_reference(ref_dir: str, dll: str, table: str = TABLE, pristine: str = PRISTINE_EXE, patch=None,
                      running: Optional[Callable[[str], List[int]]] = None, live_dir: str = LIVE_DIR) -> str:
    """`clickpass.py reference`: makes ref_dir the reference folder for the current table. Puts `dll`
    there as its own harness DLL (ruling R19: replaced only when the harness plumbing or the dump
    format changes) and the pristine exe patched with only the approved rows, both checked by sha256.
    Refuses (BisectError, nothing written) a folder that is, is in or holds the live game folder, a
    missing folder, one a game runs from or one with a bisect's note. Says what it installed."""
    ref_dir = os.path.abspath(ref_dir)
    if overlaps(ref_dir, live_dir):
        raise BisectError("%s is, is in or holds the live game folder %s: nothing is installed there"
                          % (ref_dir, live_dir))
    if not os.path.isdir(ref_dir):
        raise BisectError("there is no folder %s (make it with tools\\mp\\make_testdir.ps1)" % ref_dir)
    if os.path.exists(os.path.join(ref_dir, SWAP_NOTE)):
        raise BisectError("%s holds a bisect's note (%s): `clickpass.py bisect --recover` first" % (ref_dir, SWAP_NOTE))
    pids = (running or games_running_in)(ref_dir)
    if pids:
        raise BisectError("a game runs from %s (pid %s): nothing is installed while it runs"
                          % (ref_dir, ", ".join(str(p) for p in pids)))
    dll_sha = _sha256(dll)
    if dll_sha is None:
        raise BisectError("the harness DLL %s can't be read" % dll)
    patch = patch or functools.partial(patch_exe, pristine=pristine, dll=dll)
    targets = []
    with tempfile.TemporaryDirectory(prefix="clickpass-reference-") as tmp:
        exe = _reference_patch(table, tmp, patch)
        exe_sha = _sha256(exe)
        for source, name, sha in ((exe, EXE, exe_sha), (dll, DLL, dll_sha)):
            target = os.path.join(ref_dir, name)
            partial = target + ".clickpass-new"
            shutil.copyfile(source, partial)
            if _sha256(partial) != sha:
                _remove_all([partial])
                raise BisectError("the copy %s doesn't match %s" % (partial, source))
            os.replace(partial, target)
            targets.append("%s (sha256 %s)" % (target, sha))
    return "installed %s and %s: the pristine exe with the %d approved rows of %s, and the harness DLL %s" % (
        targets[0], targets[1], len(reference_rows(read_rows(table))), table, dll)


def patch_exe(table_path: str, out_dir: str, pristine: str, dll: str) -> str:
    """opensmacx_patch.py: the pristine exe patched from the table at table_path, written to out_dir
    with a copy of the DLL (whose exports it checks). Returns the patched exe's path."""
    cmd = [sys.executable, PATCHER, "-e", pristine, "-d", dll, "-o", out_dir, "-t", table_path]
    p = subprocess.run(cmd, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    exe = os.path.join(out_dir, EXE)
    if p.returncode != 0 or not os.path.isfile(exe):
        output = p.stdout.decode("utf-8", "replace").strip()
        raise BisectError("opensmacx_patch.py failed (exit code %d): %s" % (p.returncode, output[-2000:]))
    return exe


class SubsetRunner:
    """The real reproduces(subset): patches an exe from the approved rows and the subset, installs it
    in the OpenSMACX folder (swap), plays the case there and judges the run. Each run has a folder
    <work_dir>/<nn>/ (table.tsv and run_case's output) and an entry in `records`."""

    def __init__(self, rows: List[Row], case_file: str, osx_dir: str, failure: Failure, reference: List[str],
                 ignore: Set[Tuple[str, str]], work_dir: str, swap: ExeSwap, patch: Callable[[str, str], str],
                 run_case: Callable[[str, str, str], CaseResult], log: Callable[[str], None],
                 masks: Optional[Sequence[Mask]] = None):
        self.rows, self.case_file, self.osx_dir, self.failure = rows, case_file, osx_dir, failure
        self.reference, self.ignore, self.work_dir, self.swap = reference, ignore, work_dir, swap
        self.patch, self.run_case, self.log, self.masks = patch, run_case, log, masks
        self.candidates = sum(1 for row in rows if not row.approved)
        self.records = []

    def __call__(self, subset: List[Row]) -> bool:
        table = subset_table(self.rows, subset)
        chosen = [row for row in table if not row.approved]
        n = len(self.records) + 1
        folder = os.path.join(self.work_dir, "%02d" % n)
        purpose = ("full table" if len(chosen) == self.candidates else "approved rows only" if not chosen
                   else "search")
        record = {"run": n, "purpose": purpose, "rows": len(table), "candidates": len(chosen),
                  "first": chosen[0].name if chosen else "", "last": chosen[-1].name if chosen else "",
                  "folder": folder, "exe_sha256": None, "outcome": None, "at_step": None, "exit_code": None,
                  "reproduces": None, "note": "", "seconds": 0.0}
        self.records.append(record)
        t = time.monotonic()
        try:
            os.makedirs(folder)
            table_path = os.path.join(folder, "table.tsv")
            write_table(table, table_path)
            with tempfile.TemporaryDirectory(prefix="clickpass-bisect-") as tmp:
                exe = self.patch(table_path, tmp)
                record["exe_sha256"] = _sha256(exe)
                self.swap.install(exe)
            r = self.run_case(self.case_file, self.osx_dir, folder)
            record.update(outcome=r.outcome, at_step=r.at_step, exit_code=r.exit_code)
            reproduces, note = judge(r, self.failure, self.reference, self.ignore, self.masks)
        except BisectError as e:
            record.update(note=str(e), seconds=round(time.monotonic() - t, 1))
            raise BisectError("run %d (%s): %s" % (n, folder, e))
        record.update(reproduces=reproduces, note=note, seconds=round(time.monotonic() - t, 1))
        self.log("run %d (%s, %d candidate row%s%s): %s, %s [%.1f s]"
                 % (n, purpose, len(chosen), "" if len(chosen) == 1 else "s",
                    " %s .. %s" % (chosen[0].name, chosen[-1].name) if purpose == "search" else "",
                    "reproduces" if reproduces else "does not reproduce", note, record["seconds"]))
        return reproduces


def _pass_order(name: str):
    m = _PASS_NAME.match(name)
    return (m.group(1), int(m.group(2) or 1)) if m else None


def _pass_entry(run_dir: str, case: str) -> Tuple[Optional[dict], dict]:
    """The case's entry in a pass's result.json (None if the pass didn't run it) and the whole file."""
    try:
        with open(os.path.join(run_dir, "result.json"), encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, ValueError):
        return None, {}
    entries = [e for e in data.get("cases", []) if e.get("case") == case]
    return (entries[-1] if entries else None), data


def find_pass(out_root: str, case: str) -> str:
    """The newest pass folder (<YYYY-MM-DD-HHMM>[-n]) in out_root whose result.json has the case."""
    names = os.listdir(out_root) if os.path.isdir(out_root) else []
    for name in sorted((n for n in names if _pass_order(n)), key=_pass_order, reverse=True):
        folder = os.path.join(out_root, name)
        if _pass_entry(folder, case)[0] is not None:
            return folder
    raise BisectError("no pass in %s has run %s (clickpass.py run --cases %s)" % (out_root, case, case))


def _reference(entry: dict, case: str, ignore: Set[Tuple[str, str]], driver: str,
               masks: Sequence[Mask] = ()) -> dict:
    """The cache entry the pass compared the case with, while it is still the current reference."""
    again = "; run the case again (clickpass.py run --cases %s) and bisect that pass" % case
    key, folder = entry.get("key"), entry.get("reference")
    meta = read_meta(folder, key) if key and folder else None
    if meta is None or meta["status"] != REFERENCE:
        raise BisectError("the pass's reference for %s (%s) is gone or not a recorded reference%s"
                          % (case, folder, again))
    stale = stale_reason(meta, ignore, driver, masks)
    if stale:
        raise BisectError("the cached reference %s was %s%s" % (folder, stale, again))
    inputs = meta.get("inputs") or {}
    try:
        current = reference_key(inputs["ref_exe"], inputs["dll"], inputs["save"], inputs["case_file"])
    except (OSError, KeyError, TypeError) as e:
        raise BisectError("the reference's inputs can't be read (%s)%s" % (e, again))
    if current != key:
        raise BisectError("the reference's exe, DLL, save or case file changed since that pass%s" % again)
    meta["folder"] = folder
    return meta


def bisect_case(case: str, out_root: str, run_dir: Optional[str] = None, table: str = TABLE,
                pristine: str = PRISTINE_EXE, patch: Optional[Callable[[str, str], str]] = None,
                run_case: Callable[[str, str, str], CaseResult] = run_case,
                ignore: Optional[Set[Tuple[str, str]]] = None, driver_files: Sequence[str] = DRIVER_FILES,
                log: Optional[Callable[[str], None]] = None, masks: Optional[Sequence[Mask]] = None) -> dict:
    """Bisects the case as it failed in the pass run_dir (a folder, or its name in out_root; default:
    the newest pass that ran the case) and returns what it writes to <run_dir>/<case>/bisect.json.
    `result` is one of DONE, "does not reproduce" (with the full table, now), "reproduces without
    candidates" (with the approved rows alone), or "error"/"interrupted" (also raised). The runs are
    played in the pass's OpenSMACX folder, whose exe is swapped meanwhile."""
    log = log or (lambda text: None)
    if case.lower().endswith(".case"):
        case = os.path.splitext(os.path.basename(case))[0]
    if run_dir is None:
        run_dir = find_pass(out_root, case)
    elif not os.path.isdir(run_dir) and os.path.isdir(os.path.join(out_root, run_dir)):
        run_dir = os.path.join(out_root, run_dir)
    run_dir = os.path.abspath(run_dir)
    entry, pass_data = _pass_entry(run_dir, case)
    if entry is None:
        raise BisectError("%s has no result.json that lists %s" % (run_dir, case))
    recovered = recover_swap(pass_data["osx_dir"]) if os.path.isdir(pass_data.get("osx_dir") or "") else ""
    if recovered:
        log(recovered)
    started, t = datetime.now(), time.monotonic()
    data = {"case": case, "pass": run_dir, "failure": {"status": entry.get("status"), "at_step": entry.get("at_step")},
            "pass_detail": entry.get("detail", ""), "result": "", "message": "", "culprits": [], "clean": None,
            "search_runs": 0, "runs": [], "recovered": recovered, "started": started.isoformat(timespec="seconds"),
            "seconds": 0.0}
    try:
        _bisect(data, entry, pass_data, case, out_root, table, pristine, patch, run_case, ignore, driver_files, log,
                masks)
    except BisectError as e:
        data.update(result="error", message=str(e))
        raise
    except KeyboardInterrupt:
        data.update(result="interrupted", message="ended by Ctrl+C")
        raise
    except Exception as e:
        data.update(result="error", message="%s: %s" % (type(e).__name__, e))
        raise
    finally:
        data["seconds"] = round(time.monotonic() - t, 1)
        _write_json(os.path.join(run_dir, case, BISECT_JSON), data)
    return data


def _bisect(data, entry, pass_data, case, out_root, table, pristine, patch, run_case, ignore, driver_files,
            log, masks) -> None:
    status = entry.get("status")
    if status == "pass":
        data.update(result="no difference to bisect", message="%s passed in this pass" % case)
        return
    if status == NONDETERMINISTIC:
        data.update(result="nothing to bisect", message="the reference's own two runs differ: %s"
                    % entry.get("detail", ""))
        return
    if status not in BISECTABLE:
        raise BisectError("a %s case can't be bisected: %s" % (status, entry.get("detail", "")))
    failure = Failure(status, entry.get("at_step"))
    ignore = read_ignore(IGNORE_TSV) if ignore is None else ignore
    masks = read_masks(MASKS_TSV) if masks is None else list(masks)
    meta = _reference(entry, case, ignore, driver_fingerprint(driver_files), masks)
    osx_dir, ref_dir = pass_data.get("osx_dir"), pass_data.get("ref_dir")
    if not osx_dir or not ref_dir:
        raise BisectError("%s doesn't name its game folders" % os.path.join(data["pass"], "result.json"))
    osx_dir = os.path.abspath(osx_dir)
    for folder, what in ((LIVE_DIR, "the live game folder"), (ref_dir, "the reference folder"),
                         (out_root, "the out root")):
        if overlaps(osx_dir, folder):
            raise BisectError("the pass's OpenSMACX folder %s is, is in or holds %s %s: no exe is swapped there"
                              % (osx_dir, what, folder))
    case_file = meta["inputs"]["case_file"]
    osx_save = os.path.join(osx_dir, "saves", case_save(case_file))
    if _sha256(osx_save) is None or _sha256(osx_save) != _sha256(meta["inputs"]["save"]):
        raise BisectError("%s is missing or differs from the reference's %s" % (osx_save, meta["inputs"]["save"]))
    rows = read_rows(table)
    approved = reference_rows(rows)
    candidates = len(rows) - len(approved)
    if not candidates:
        raise BisectError("%s has no rows but approved ones" % table)
    data.update(table=os.path.abspath(table), rows=len(rows), approved=[row.name for row in approved],
                candidates=candidates, reference=meta["folder"], osx_dir=osx_dir, exe=os.path.join(osx_dir, EXE),
                exe_sha256=None, exe_is_full_table=None, restored=None, cleanup="")
    work_dir = os.path.join(data["pass"], case, "bisect")
    if os.path.exists(work_dir):
        shutil.rmtree(work_dir)
    patch = patch or functools.partial(patch_exe, pristine=pristine, dll=os.path.join(osx_dir, DLL))
    swap = ExeSwap(data["exe"])
    reproduces = SubsetRunner(rows, case_file, osx_dir, failure, meta["dumps"], ignore, work_dir, swap, patch,
                              run_case, log, masks)
    data["runs"] = reproduces.records  # (the same list: bisect.json gets every run, also after an error)
    log("%s: %s; %d candidate rows, the %d approved rows in every run"
        % (case, _failure_text(failure), candidates, len(approved)))
    try:
        with swap:
            data["exe_sha256"] = swap.sha256
            full = reproduces(rows)
            data["exe_is_full_table"] = reproduces.records[0]["exe_sha256"] == swap.sha256
            if not data["exe_is_full_table"]:
                log(_not_full_table(data))
            if not full:
                data.update(result="does not reproduce", message="the full table no longer reproduces %s; "
                            "nothing was bisected" % _failure_text(failure))
                return
            if reproduces(approved):
                data.update(result="reproduces without candidates", message="the approved rows alone (the "
                            "reference's own table) reproduce %s: no candidate row causes it (the OpenSMACX "
                            "folder's DLL or harness?)" % _failure_text(failure))
                return
            result = find_culprits(rows, reproduces)
    finally:
        data["restored"] = swap.restored if swap.swapped else None
        data["cleanup"] = swap.cleanup_error
    data.update(result="culprits", culprits=[{"offset": "0x%08X" % row.offset, "name": row.name}
                                              for row in result.culprits],
                clean=result.clean, search_runs=result.runs,
                message="%d culprit row%s, %s" % (len(result.culprits), "" if len(result.culprits) == 1 else "s",
                                                  "clean" if result.clean else "not clean: neither half of this set "
                                                  "reproduces alone (an interaction, or a read of leftover memory)"))


def _not_full_table(data: dict) -> str:
    runs = data.get("runs") or [{}]
    return ("WARNING: the OpenSMACX folder's exe (sha256 %s) is not %s's patch (sha256 %s): the pass played "
            "another exe than the table bisected here" % (data.get("exe_sha256"), data.get("table"),
                                                         runs[0].get("exe_sha256")))


def format_bisect(data: dict) -> List[str]:
    """What the bisect command prints for a finished bisection (warnings first)."""
    lines = []
    if data.get("recovered"):
        lines.append("first " + data["recovered"])
    if data.get("exe_is_full_table") is False:
        lines.append(_not_full_table(data))
    if data.get("cleanup"):
        lines.append("WARNING: the exe is back, but %s could not be removed: `run` refuses the folder until "
                     "`clickpass.py bisect --recover` removes it" % data["cleanup"])
    failure = data.get("failure") or {}
    where = str(failure.get("status"))
    if failure.get("at_step") is not None:
        where += " at step %d" % failure["at_step"]
    lines.append("%s: %s (%s in %s)" % (data["case"], data["result"], where, data["pass"]))
    if data.get("message"):
        lines.append("  " + data["message"])
    for c in data.get("culprits", []):
        lines.append("  %s  %s" % (c["offset"], c["name"]))
    if data.get("runs"):
        lines.append("  %d runs (%d to check the ends, %d searching), %.1f s"
                     % (len(data["runs"]), len(data["runs"]) - data.get("search_runs", 0),
                        data.get("search_runs", 0), data.get("seconds", 0)))
    if data.get("restored"):
        lines.append("  %s put back (sha256 %s)" % (data.get("exe"), data.get("exe_sha256")))
    lines.append("bisect.json: %s" % os.path.join(data["pass"], data["case"], BISECT_JSON))
    return lines
