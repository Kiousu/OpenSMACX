"""Click pass runner: plays cases in the reference game and in OpenSMACX and compares their dumps.

A case runs through case.ps1 (run_case) in the reference folder and in the OpenSMACX folder, from
the same save. Each step's dump is compared with compare.py (less tools/clickpass/ignore.tsv), and
the files the game had written by then with files.py (less the bytes tools/clickpass/file_masks.tsv
masks). The reference side is recorded once and reused: build/clickpass/reference/<case>/<key>/
holds its dumps, file lists and file contents and meta.json, where the key is the sha256 of the
reference exe, the harness DLL, the save and the case file (reference_key). meta.json also records
the ignore set, the file masks and the case driver's fingerprint (case.ps1, folders.ps1 and
tools/mp/msg.ps1) it was recorded with; an entry recorded with other ones counts as a miss. On a miss
the reference plays the case twice (the self-check): two runs that differ store the case as
nondeterministic (with the differences, no dumps), which keeps it out of the comparison until one of
those inputs changes.

Statuses (CaseOutcome.status):
  pass              every step's dump and files match the reference
  fail              a step differs; at_step is the first such step, differences are that step's
  crash, hang, blocked-write
                    case.ps1's outcome for the OpenSMACX game, at_step where it happened
  setup-failed      the OpenSMACX side could not be run (case.ps1 setup-failed, including driver
                    errors after the launch, at_step null), an unreadable OpenSMACX dump or file
                    list, a bad case file, a save that differs between the two folders, a bisect's
                    note (SWAP_NOTE) in the OpenSMACX folder (its exe may be a bisect's subset exe),
                    or an unexpected error while the case was played (the pass goes on)
  nondeterministic  the reference's two runs differ (at_step and differences from that step)
  reference-broken  the reference side did not complete (setup-failed, crash, hang or blocked-write
                    in either run, or a missing exe, DLL or save in the reference folder), or a
                    cached reference dump can't be read (the entry is dropped and recorded again
                    on the next pass)
"""
import ctypes
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import time
from datetime import datetime
from ctypes import wintypes
from typing import Callable, Dict, List, NamedTuple, Optional, Sequence, Set, Tuple

from clickpass.compare import IGNORE_TSV, MISSING, Difference, check_limit, diff_dumps, read_ignore
from clickpass.dump import read_dump
from clickpass.files import MASKS_TSV, Mask, diff_files, manifest_for, read_masks, run_copies

HERE = os.path.dirname(os.path.abspath(__file__))
CASES_DIR = os.path.join(HERE, "cases")
CASE_PS1 = os.path.join(HERE, "case.ps1")
FOLDERS_PS1 = os.path.join(HERE, "folders.ps1")
MSG_PS1 = os.path.normpath(os.path.join(HERE, "..", "mp", "msg.ps1"))
DRIVER_FILES = (CASE_PS1, FOLDERS_PS1, MSG_PS1)  # the case driver, fingerprinted into every cache entry
LIVE_DIR = r"G:\Sid Meier's Alpha Centauri"  # Kiousu's game: nothing is ever written in or under it
GAME_PID = "game.pid"  # case.ps1 writes "<pid> <start time, UTC FILETIME>" of its game here
REF_EXE = "terranx_opensmacx.exe"
DLL = "OpenSMACX.dll"
CASE_TIMEOUT_SEC = 1800  # case.ps1 has its own (shorter) timeouts; this only catches a stuck driver
SWAP_NOTE = "clickpass_bisect_swap.json"  # in a game folder while a bisect has its exe swapped (bisect.py)
REFERENCE = "reference"  # meta.json status of a recorded reference
NONDETERMINISTIC = "nondeterministic"
FAILING = ("fail", "crash", "hang", "blocked-write", "setup-failed", "reference-broken")
STATUSES = ("pass",) + FAILING + (NONDETERMINISTIC,)
OSX_STATUS = {"crash": "crash", "hang": "hang", "blocked-write": "blocked-write", "setup-failed": "setup-failed"}

_DUMP_NAME = re.compile(r"^dump_(\d+)\.bin$")


class CaseResult(NamedTuple):
    """One game's run of a case, from case.ps1's result.json."""
    outcome: str  # ok, crash, hang, blocked-write, setup-failed
    at_step: Optional[int]
    exit_code: Optional[int]  # unsigned
    dumps: List[str]  # dump_<step>.bin paths, by step
    detail: str = ""
    seconds: float = 0.0


class CaseOutcome(NamedTuple):
    case: str
    status: str  # one of STATUSES
    at_step: Optional[int]
    differences: List[Difference]
    detail: str
    cache: str = ""  # reference "hit" or "miss"; "" when the cache wasn't reached
    key: str = ""  # reference_key, when it could be computed
    seconds: float = 0.0  # the case's wall time, both sides


class PassResult(NamedTuple):
    run_dir: str
    outcomes: List[CaseOutcome]


class PassRefused(Exception):
    """run_pass won't play in these folders; the message says why and what to do."""


def swap_refusal(game_dir: str) -> str:
    """Why no case may be played in game_dir while a bisect's note is there ("" when it may): the
    folder's exe may be one of the bisect's subset exes."""
    note = os.path.join(game_dir, SWAP_NOTE)
    if not os.path.exists(note):
        return ""
    return ("%s exists: a bisect has swapped this folder's exe and not put it back (it still runs, or it was "
            "ended before it could); `clickpass.py bisect --recover --osx-dir \"%s\"` puts it back once that "
            "bisect has ended" % (note, game_dir))


def overlaps(a: str, b: str) -> bool:
    """Whether two folders are the same or one holds the other (any case, whole folder names only)."""
    a = os.path.normcase(os.path.realpath(a)).rstrip("\\/") + os.sep
    b = os.path.normcase(os.path.realpath(b)).rstrip("\\/") + os.sep
    return a.startswith(b) or b.startswith(a)


def live_refusal(folders: Sequence[Tuple[str, str]], live: str = LIVE_DIR) -> str:
    """Why these (folder, what it is) may not be used ("" when they may): one is, is in or holds the
    live game folder."""
    for folder, what in folders:
        if folder and overlaps(folder, live):
            return "%s %s is, is in or holds the live game folder %s: nothing is written there" % (what, folder, live)
    return ""


def _dump_files(folder: str) -> List[str]:
    found = []
    if os.path.isdir(folder):
        for name in os.listdir(folder):
            m = _DUMP_NAME.match(name)
            if m:
                found.append((int(m.group(1)), os.path.join(folder, name)))
    return [path for _, path in sorted(found)]


def _lines(value) -> List[str]:
    if value is None:
        return []
    if isinstance(value, str):
        return [value]
    return [str(v) for v in value]


def read_result(out_dir: str) -> CaseResult:
    """case.ps1's result.json in out_dir and the dumps next to it (setup-failed when unreadable).
    The harness's failed writes (`errors`), the refused writes (`blocked`) and a clean-up `warning` go
    into the detail."""
    path = os.path.join(out_dir, "result.json")
    dumps = _dump_files(out_dir)
    try:
        with open(path, encoding="utf-8-sig") as f:
            data = json.load(f)
    except (OSError, ValueError) as e:
        return CaseResult("setup-failed", None, None, dumps, "case.ps1 left no readable result.json (%s)" % e)
    parts = [data.get("detail") or ""]
    blocked, errors = _lines(data.get("blocked")), _lines(data.get("errors"))
    if blocked:
        parts.append("blocked: " + " | ".join(blocked))
    if errors:
        parts.append("harness errors: " + " | ".join(errors))
    if data.get("warning"):
        parts.append("warning: %s" % data["warning"])
    return CaseResult(data.get("outcome") or "setup-failed", data.get("at_step"), data.get("exit_code"), dumps,
                      "; ".join(p for p in parts if p), float(data.get("seconds") or 0))


class _Process:
    """A process opened for query and terminate: its exe path and start time (a UTC FILETIME)."""

    def __init__(self, handle, exe: str, start: int):
        self.handle, self.exe, self.start = handle, exe, start

    def terminate(self) -> None:
        k32 = _kernel32()
        if k32.TerminateProcess(self.handle, 1):
            k32.WaitForSingleObject(self.handle, 15000)

    def close(self) -> None:
        _kernel32().CloseHandle(self.handle)


_K32 = None


def _kernel32():
    global _K32
    if _K32 is None:
        k32 = ctypes.WinDLL("kernel32", use_last_error=True)
        k32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        k32.OpenProcess.restype = wintypes.HANDLE
        k32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        k32.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR,
                                                   ctypes.POINTER(wintypes.DWORD)]
        k32.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
        k32.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
        k32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        k32.WaitForSingleObject.restype = wintypes.DWORD
        k32.CloseHandle.argtypes = [wintypes.HANDLE]
        _K32 = k32
    return _K32


def _open_process(pid: int) -> Optional[_Process]:
    """The running process with this PID, or None (gone, or it can't be opened)."""
    k32 = _kernel32()
    # PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE: checked and ended through
    # the same handle, so the PID can't be reused in between
    handle = k32.OpenProcess(0x1000 | 0x0001 | 0x00100000, False, pid)
    if not handle:
        return None
    code, size = wintypes.DWORD(), wintypes.DWORD(32768)
    exe = ctypes.create_unicode_buffer(size.value)
    times = [wintypes.FILETIME() for _ in range(4)]
    if (k32.GetExitCodeProcess(handle, ctypes.byref(code)) and code.value == 259  # STILL_ACTIVE
            and k32.QueryFullProcessImageNameW(handle, 0, exe, ctypes.byref(size))
            and k32.GetProcessTimes(handle, *[ctypes.byref(t) for t in times])):
        return _Process(handle, exe.value, (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime)
    k32.CloseHandle(handle)
    return None


def games_running_in(folder: str) -> List[int]:
    """The PIDs of the running processes whose exe is inside folder (by exe path, never by name);
    a process that can't be queried (another user's, elevated) is not seen."""
    k32 = _kernel32()
    k32.K32EnumProcesses.argtypes = [ctypes.POINTER(wintypes.DWORD), wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
    pids = (wintypes.DWORD * 4096)()
    used = wintypes.DWORD()
    if not k32.K32EnumProcesses(pids, ctypes.sizeof(pids), ctypes.byref(used)):
        raise OSError(ctypes.get_last_error(), "EnumProcesses failed")
    found = []
    for pid in list(pids)[:used.value // ctypes.sizeof(wintypes.DWORD)]:
        handle = k32.OpenProcess(0x1000, False, pid)  # PROCESS_QUERY_LIMITED_INFORMATION
        if not handle:
            continue
        try:
            size = wintypes.DWORD(32768)
            exe = ctypes.create_unicode_buffer(size.value)
            if k32.QueryFullProcessImageNameW(handle, 0, exe, ctypes.byref(size)) and _within(exe.value, folder):
                found.append(pid)
        finally:
            k32.CloseHandle(handle)
    return found


def process_start(pid: int) -> Optional[int]:
    """The start time (a UTC FILETIME) of the running process with this PID, or None."""
    process = _open_process(pid)
    if process is None:
        return None
    try:
        return process.start
    finally:
        process.close()


def _within(path: str, folder: str) -> bool:
    """Whether path is inside folder (any case, whole folder names only)."""
    path = os.path.normcase(os.path.realpath(path))
    folder = os.path.normcase(os.path.realpath(folder)).rstrip("\\/")
    return path.startswith(folder + os.sep)


def stop_game(pid_file: str, game_dir: str, open_process=None) -> str:
    """Stops the game case.ps1 started, for when case.ps1 itself was ended before it could (pid_file:
    "<pid> <start time as a UTC FILETIME>"). Only that PID, and only while that process still runs
    from inside game_dir and started at that time, so a reused PID is never hit; never by name.
    Says what it did."""
    open_process = open_process or _open_process
    try:
        with open(pid_file, encoding="ascii") as f:
            pid_text, start_text = f.read().split()[:2]
        pid, start = int(pid_text), int(start_text)
    except (OSError, ValueError) as e:
        return "no game to stop (%s: %s)" % (pid_file, e)
    process = open_process(pid)
    if process is None:
        return "its game (pid %d) had already ended" % pid
    try:
        if not _within(process.exe, game_dir):
            return "pid %d is %s, not a game from %s: left alone" % (pid, process.exe, game_dir)
        if process.start != start:
            return "pid %d started at another time than its game (a reused PID): left alone" % pid
        process.terminate()
        return "its game (pid %d) was stopped" % pid
    finally:
        process.close()


def run_case(case: str, game_dir: str, out_dir: str, timeout: int = CASE_TIMEOUT_SEC) -> CaseResult:
    """Plays the case file `case` in game_dir with case.ps1 (its output goes to out_dir\\driver.log).
    When case.ps1 has to be ended (the timeout, or Ctrl+C, which is passed on) its `finally` doesn't
    run, so the game it started is stopped here, from out_dir\\game.pid (stop_game)."""
    os.makedirs(out_dir, exist_ok=True)
    for name in ("result.json", GAME_PID):  # never act on an earlier run's files
        path = os.path.join(out_dir, name)
        if os.path.exists(path):
            os.remove(path)
    cmd = ["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", CASE_PS1,
           "-CaseFile", case, "-GameDir", game_dir, "-OutDir", out_dir]
    with open(os.path.join(out_dir, "driver.log"), "wb") as log:
        try:
            subprocess.run(cmd, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
        except subprocess.TimeoutExpired:
            stopped = stop_game(os.path.join(out_dir, GAME_PID), game_dir)
            return CaseResult("setup-failed", None, None, _dump_files(out_dir),
                              "case.ps1 did not finish within %d s and was ended; %s" % (timeout, stopped))
        except KeyboardInterrupt:
            stop_game(os.path.join(out_dir, GAME_PID), game_dir)
            raise
    return read_result(out_dir)


def case_save(case_file: str) -> str:
    """The save a case file loads: its first step, `save <file.SAV>` (as case.ps1 reads it)."""
    with open(case_file, encoding="utf-8-sig", errors="replace") as f:
        for line in f:
            s = line.strip()
            if not s or s.startswith("#"):
                continue
            verb, _, arg = s.partition(" ")
            if verb == "save" and arg.strip():
                return arg.strip()
            break
    raise ValueError("%s: the first step must be 'save <file.SAV>'" % case_file)


def _sha256(path: str) -> str:
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def _files_sha256(paths: Sequence[str]) -> str:
    """sha256 (hex) over the files' contents, each preceded by its length."""
    h = hashlib.sha256()
    for path in paths:
        with open(path, "rb") as f:
            data = f.read()
        h.update(struct.pack("<Q", len(data)))
        h.update(data)
    return h.hexdigest()


def reference_key(ref_exe: str, dll: str, save: str, case_file: str) -> str:
    """sha256 (hex) over the four files' contents, each preceded by its length. The DLL is the
    reference folder's own: it is only replaced when the harness plumbing changes, not with every
    batch's OpenSMACX DLL (ruling R19), so references survive batches."""
    return _files_sha256((ref_exe, dll, save, case_file))


def driver_fingerprint(paths: Sequence[str] = DRIVER_FILES) -> str:
    """sha256 (hex) over the case driver's files (case.ps1, tools/mp/msg.ps1), like reference_key."""
    return _files_sha256(paths)


def _by_step(dumps: List[str]) -> Dict[int, str]:
    steps = {}
    for path in dumps:
        m = _DUMP_NAME.match(os.path.basename(path))
        if not m:
            raise ValueError("%s is not named dump_<step>.bin" % path)
        steps[int(m.group(1))] = path
    return steps


def compare_runs(a: List[str], b: List[str], ignore: Set[Tuple[str, str]], limit: int = 50,
                 masks: Optional[Sequence[Mask]] = None) -> Tuple[Optional[int], List[Difference]]:
    """The first step at which two runs differ and that step's differences (at most `limit`, at
    least 1), or (None, []) when every step matches: the dumps (less `ignore`), then the files the
    game had written, from the files_<step>.json next to each dump (less `masks`, default
    file_masks.tsv). A dump only one run has is a difference at its step; a file list that is
    missing or can't be read is an error (OSError or ValueError), never a match."""
    check_limit(limit)
    masks = read_masks(MASKS_TSV) if masks is None else masks
    sa, sb = _by_step(a), _by_step(b)
    for step in sorted(set(sa) | set(sb)):
        if step not in sa or step not in sb:
            return step, [Difference("dump_%d.bin" % step, None, "", "present" if step in sa else MISSING,
                                     "present" if step in sb else MISSING)]
        diffs = diff_dumps(read_dump(sa[step]), read_dump(sb[step]), ignore, limit)
        if len(diffs) < limit:
            diffs += diff_files(manifest_for(sa[step]), manifest_for(sb[step]), masks, limit - len(diffs))
        if diffs:
            return step, diffs
    return None, []


def run_complete(dumps: Sequence[str]) -> bool:
    """Whether every dump has its file list and every content the lists name is there."""
    try:
        return all(os.path.isfile(p) and all(os.path.isfile(c) for c in run_copies(manifest_for(p))) for p in dumps)
    except (OSError, ValueError):
        return False


def describe_run(r: CaseResult) -> str:
    text = r.outcome
    if r.at_step is not None:
        text += " at step %d" % r.at_step
    if r.exit_code is not None:
        text += " (exit code 0x%08X)" % (r.exit_code & 0xFFFFFFFF)
    return text + (": " + r.detail if r.detail else "")


def _difference_json(d: Difference) -> list:
    return [d.section, d.index, d.field, d.a, d.b]


def read_meta(folder: str, key: str) -> Optional[dict]:
    """A complete cache entry's meta.json, or None (missing, unreadable, or not matching the key)."""
    try:
        with open(os.path.join(folder, "meta.json"), encoding="utf-8") as f:
            meta = json.load(f)
        if meta.get("key") != key or meta.get("status") not in (REFERENCE, NONDETERMINISTIC):
            return None
        meta["differences"] = [Difference(*d) for d in meta.get("differences", [])]
        if meta["status"] == REFERENCE:
            dumps = [os.path.join(folder, name) for name in meta["dumps"]]
            if not dumps or not run_complete(dumps):  # (every dump, file list and content)
                return None
            meta["dumps"] = dumps
        return meta
    except (OSError, ValueError, KeyError, TypeError):
        return None


def mask_rows(masks: Sequence[Mask]) -> list:
    """The file masks as meta.json records them (the reasons left out, like the ignore set's)."""
    return sorted([[m.pattern, m.offset, m.length] for m in masks], key=str)


def stale_reason(meta: dict, ignore: Set[Tuple[str, str]], driver: str, masks: Sequence[Mask] = ()) -> str:
    """Why a cache entry can't be used with this ignore set, these file masks and case driver (""
    when it can): the runs' verdict (agree or not) depends on the ignore set and the masks, the
    dumps and file lists on the driver."""
    if meta.get("ignore") != sorted([list(entry) for entry in ignore]):
        return "recorded with another ignore table"
    if meta.get("masks") != mask_rows(masks):
        return "recorded with other file masks"
    if meta.get("driver") != driver:
        return "recorded with another case driver"
    return ""


def _store(folder: str, meta: dict, dumps: List[str]) -> dict:
    """Writes a cache entry (dumps with their file lists and contents, then meta.json) beside
    `folder` and moves it into place."""
    partial = folder + ".partial"
    if os.path.exists(partial):
        shutil.rmtree(partial)
    os.makedirs(os.path.join(partial, "files"))
    for path in dumps:
        shutil.copy2(path, partial)
        manifest = manifest_for(path)
        shutil.copy2(manifest, partial)
        for copy in run_copies(manifest):
            shutil.copy2(copy, os.path.join(partial, "files"))
    stored = dict(meta, dumps=[os.path.basename(p) for p in dumps],
                  differences=[_difference_json(d) for d in meta["differences"]])
    with open(os.path.join(partial, "meta.json"), "w", encoding="utf-8") as f:
        json.dump(stored, f, indent=2)
        f.write("\n")
    if os.path.exists(folder):
        shutil.rmtree(folder)
    os.rename(partial, folder)
    return dict(meta, dumps=[os.path.join(folder, os.path.basename(p)) for p in dumps])


class _Case:
    """One case's run in a pass: its folders, the runs made so far, and the progress log."""

    def __init__(self, name, case_file, case_dir, run_case, log):
        self.name, self.case_file, self.case_dir = name, case_file, case_dir
        self.run_case, self.log = run_case, log
        self.runs = []  # what result.json lists per run
        self.cache = ""
        self.key = ""
        self.inputs = {}  # the files behind the key

    def play(self, side: str, game_dir: str) -> CaseResult:
        self.log("%s: %s run in %s" % (self.name, side, game_dir))
        out = os.path.join(self.case_dir, side)
        r = self.run_case(self.case_file, game_dir, out)
        self.runs.append({"side": side, "outcome": r.outcome, "at_step": r.at_step, "exit_code": r.exit_code,
                          "seconds": r.seconds, "dumps": len(r.dumps), "folder": out, "detail": r.detail})
        return r

    def outcome(self, status, at_step=None, differences=(), detail="") -> CaseOutcome:
        return CaseOutcome(self.name, status, at_step, list(differences), detail, self.cache, self.key)


def _unreadable(dumps: List[str]) -> str:
    """The first dump read_dump rejects, or whose file list can't be read, with why; "" when all of
    them read."""
    for path in dumps:
        try:
            read_dump(path)
            run_copies(manifest_for(path))  # (reads the file list)
        except (OSError, ValueError) as e:
            return "%s (%s)" % (path, e)
    return ""


def _reference(c: _Case, ref_dir: str, folder: str, ignore, driver: str,
               masks: Sequence[Mask] = ()) -> Tuple[Optional[dict], Optional[CaseOutcome]]:
    """The case's reference entry from the cache, or recorded now with the self-check; or the
    reference-broken outcome. An entry recorded with another ignore set, file masks or case driver
    is stale (stale_reason)."""
    ignored = sorted([list(entry) for entry in ignore])
    meta = read_meta(folder, c.key)
    stale = stale_reason(meta, ignore, driver, masks) if meta is not None else ""
    if meta is not None and not stale:
        c.cache = "hit"
        bad = _unreadable(meta["dumps"]) if meta["status"] == REFERENCE else ""
        if bad:
            shutil.rmtree(folder, ignore_errors=True)
            return None, c.outcome("reference-broken", None, (), "the cached reference dump %s can't be read; "
                                   "the entry was dropped, the next pass records it again" % bad)
        c.log("%s: reference cached (%s)" % (c.name, folder))
        return meta, None
    if stale:
        c.log("%s: the cached reference was %s, recording it again" % (c.name, stale))
    c.cache = "miss"
    runs = []
    for side in ("reference-1", "reference-2"):
        r = c.play(side, ref_dir)
        if r.outcome != "ok":
            return None, c.outcome("reference-broken", r.at_step, (), "%s: %s" % (side, describe_run(r)))
        runs.append(r)
    try:
        step, diffs = compare_runs(runs[0].dumps, runs[1].dumps, ignore, masks=masks)
    except (OSError, ValueError) as e:
        return None, c.outcome("reference-broken", None, (), "the reference dumps can't be compared: %s" % e)
    meta = {"case": c.name, "key": c.key, "status": REFERENCE if step is None else NONDETERMINISTIC,
            "at_step": step, "differences": diffs, "recorded": datetime.now().isoformat(timespec="seconds"),
            "inputs": c.inputs, "ignore": ignored, "masks": mask_rows(masks), "driver": driver}
    return _store(folder, meta, runs[0].dumps if step is None else []), None


def _play_case(case: str, ref_dir: str, osx_dir: str, run_dir: str, cases_dir: str, cache_root: str,
               run_case, ignore, driver, log, masks: Sequence[Mask] = ()) -> Tuple[CaseOutcome, list]:
    if case.lower().endswith(".case") and os.path.isfile(case):
        case_file = os.path.abspath(case)
    else:
        case_file = os.path.join(cases_dir, case + ".case")
    name = os.path.splitext(os.path.basename(case_file))[0]
    c = _Case(name, case_file, os.path.join(run_dir, name), run_case, log)
    try:
        save = case_save(case_file)
    except (OSError, ValueError) as e:
        return c.outcome("setup-failed", detail="bad case file: %s" % e), c.runs
    ref_save = os.path.join(ref_dir, "saves", save)
    inputs = [os.path.join(ref_dir, REF_EXE), os.path.join(ref_dir, DLL), ref_save]
    missing = [p for p in inputs if not os.path.isfile(p)]
    if missing:
        return c.outcome("reference-broken", detail="missing in the reference folder: " + ", ".join(missing)), c.runs
    c.key = reference_key(inputs[0], inputs[1], ref_save, case_file)
    c.inputs = {"ref_exe": inputs[0], "dll": inputs[1], "save": ref_save, "case_file": case_file}
    osx_save = os.path.join(osx_dir, "saves", save)
    if not os.path.isfile(osx_save) or _sha256(osx_save) != _sha256(ref_save):
        return c.outcome("setup-failed", detail="%s is missing or differs from %s" % (osx_save, ref_save)), c.runs

    meta, broken = _reference(c, ref_dir, os.path.join(cache_root, name, c.key), ignore, driver, masks)
    if broken is not None:
        return broken, c.runs
    if meta["status"] == NONDETERMINISTIC:
        return c.outcome(NONDETERMINISTIC, meta["at_step"], meta["differences"],
                         "the reference's two runs differ from step %d on (recorded %s); not compared"
                         % (meta["at_step"], meta.get("recorded", "?"))), c.runs

    refusal = swap_refusal(osx_dir)  # (a bisect may have started since the pass did)
    if refusal:
        return c.outcome("setup-failed", detail=refusal), c.runs
    r = c.play("osx", osx_dir)
    if r.outcome != "ok":
        status = OSX_STATUS.get(r.outcome, "setup-failed")
        return c.outcome(status, r.at_step, (), describe_run(r)), c.runs
    try:
        step, diffs = compare_runs(meta["dumps"], r.dumps, ignore, masks=masks)  # (the reference's were read)
    except (OSError, ValueError) as e:
        return c.outcome("setup-failed", detail="the OpenSMACX dumps can't be compared: %s" % e), c.runs
    if step is None:
        steps = len(r.dumps)
        return c.outcome("pass", detail="%d step%s, no differences" % (steps, "" if steps == 1 else "s")), c.runs
    return c.outcome("fail", step, diffs, "differs from the reference from step %d on (%d differences shown)"
                     % (step, len(diffs))), c.runs


def _new_run_dir(out_root: str, now: datetime) -> str:
    base = os.path.join(out_root, now.strftime("%Y-%m-%d-%H%M"))
    path, n = base, 1
    while os.path.exists(path):
        n += 1
        path = "%s-%d" % (base, n)
    os.makedirs(path)
    return path


def _write_result(run_dir: str, ref_dir: str, osx_dir: str, started: datetime, entries: list,
                  warnings: Sequence[str] = ()) -> None:
    summary = {}
    for e in entries:
        summary[e["status"]] = summary.get(e["status"], 0) + 1
    data = {"started": started.isoformat(timespec="seconds"), "ref_dir": ref_dir, "osx_dir": osx_dir,
            "warnings": list(warnings), "summary": summary, "cases": entries}
    with open(os.path.join(run_dir, "result.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
        f.write("\n")


def run_pass(cases: List[str], ref_dir: str, osx_dir: str, out_root: str, run_case=run_case,
             cases_dir: str = CASES_DIR, cache_root: Optional[str] = None,
             ignore: Optional[Set[Tuple[str, str]]] = None, log: Optional[Callable[[str], None]] = None,
             now: Optional[datetime] = None, driver_files: Sequence[str] = DRIVER_FILES,
             warnings: Sequence[str] = (), masks: Optional[Sequence[Mask]] = None,
             live_dir: str = LIVE_DIR) -> PassResult:
    """Plays each case (a name from cases_dir, or a .case path) on both sides and compares them.
    Writes <out_root>/<YYYY-MM-DD-HHMM>/ with a folder per case (reference-1, reference-2 when the
    reference was recorded, osx) and result.json (rewritten after every case); the reference cache
    is <out_root>/reference unless cache_root says otherwise; `warnings` (such as the OpenSMACX exe not
    being the table's patch) go into result.json for the report; `ignore` and `masks` default to
    ignore.tsv and file_masks.tsv. Raises PassRefused, before anything is written, when out_root,
    the cache or either game folder is, is in or holds the live game folder, and while a bisect's
    note (SWAP_NOTE) is in either folder; a case whose OpenSMACX folder gets one during the pass is
    setup-failed. An unexpected error in one case (a file that can't be written, ...) makes that case
    setup-failed and the pass goes on."""
    ref_dir, osx_dir = os.path.abspath(ref_dir), os.path.abspath(osx_dir)
    cache_root = cache_root or os.path.join(out_root, "reference")
    refusal = live_refusal([(out_root, "the out root"), (cache_root, "the reference cache"),
                            (ref_dir, "the reference folder"), (osx_dir, "the OpenSMACX folder")], live_dir)
    if refusal:
        raise PassRefused(refusal)
    if os.path.normcase(ref_dir) == os.path.normcase(osx_dir):
        raise ValueError("the reference and OpenSMACX folders are the same: %s" % ref_dir)
    for game_dir in (ref_dir, osx_dir):
        refusal = swap_refusal(game_dir)
        if refusal:
            raise PassRefused(refusal)
    ignore = read_ignore(IGNORE_TSV) if ignore is None else ignore
    masks = read_masks(MASKS_TSV) if masks is None else list(masks)
    driver = driver_fingerprint(driver_files)
    log = log or (lambda text: None)
    started = now or datetime.now()
    run_dir = _new_run_dir(out_root, started)
    outcomes, entries = [], []
    for case in cases:
        t = time.monotonic()
        try:
            outcome, runs = _play_case(case, ref_dir, osx_dir, run_dir, cases_dir, cache_root, run_case, ignore,
                                       driver, log, masks)
        except Exception as e:  # (not KeyboardInterrupt): this case only, the pass goes on
            name = os.path.splitext(os.path.basename(case))[0]
            outcome, runs = CaseOutcome(name, "setup-failed", None, [], "unexpected error while playing the case: "
                                        "%s: %s" % (type(e).__name__, e)), []
        outcome = outcome._replace(seconds=round(time.monotonic() - t, 1))
        outcomes.append(outcome)
        entries.append({"case": outcome.case, "status": outcome.status, "at_step": outcome.at_step,
                        "detail": outcome.detail, "differences": [str(d) for d in outcome.differences],
                        "cache": outcome.cache, "key": outcome.key, "seconds": outcome.seconds,
                        "reference": os.path.join(cache_root, outcome.case, outcome.key) if outcome.key else "",
                        "runs": runs})
        _write_result(run_dir, ref_dir, osx_dir, started, entries, warnings)
        log("%s: %s" % (outcome.case, format_outcome(outcome)))
    if not cases:
        _write_result(run_dir, ref_dir, osx_dir, started, entries, warnings)
    return PassResult(run_dir, outcomes)


def format_outcome(o: CaseOutcome) -> str:
    """`pass (7.0 s, reference hit)`, `fail at step 2 (...)`, followed by the detail."""
    text = o.status
    if o.at_step is not None:
        text += " at step %d" % o.at_step
    extra = ["%.1f s" % o.seconds]
    if o.cache:
        extra.append("reference " + o.cache)
    text += " (%s)" % ", ".join(extra)
    if o.detail:
        text += ": " + o.detail
    return text
