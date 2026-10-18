import contextlib
import hashlib
import io
import json
import os
import shutil
import struct
import subprocess
import tempfile
import unittest
from datetime import datetime
from typing import NamedTuple, Optional, Tuple
from unittest import mock

from clickpass import bisect as bisect_module
from clickpass import clickpass as cli
from clickpass import runner
from clickpass.compare import Difference
from clickpass.files import Mask
from clickpass.runner import (SWAP_NOTE, CaseOutcome, CaseResult, PassRefused, PassResult, read_result,
                              reference_key, run_pass, stop_game)

ACCESS_VIOLATION = 0xC0000005
NOW = datetime(2026, 10, 18, 5, 7)
SAVE_FILE = "game\\saves\\_cp_save.SAV"


def section(name, type_name, elem_size, elements):
    return (name.encode("ascii").ljust(24, b"\0") + type_name.encode("ascii").ljust(24, b"\0")
            + struct.pack("<II", len(elements), elem_size) + b"".join(elements))


def dump_bytes(step, values):
    """A dump holding only the Globals and GlobalNames sections."""
    names = sorted(values)
    sections = [section("Globals", "u32", 4, [struct.pack("<I", values[n]) for n in names]),
                section("GlobalNames", "char32", 32, [n.encode("ascii").ljust(32, b"\0") for n in names])]
    return b"OSXDUMP1" + struct.pack("<II", step, len(sections)) + b"".join(sections)


def states(count, **changes):
    """`count` steps' Globals: turn 1 and x 0 everywhere, except `s<step>={...}` overrides."""
    result = []
    for step in range(count):
        values = {"turn": 1, "x": 0}
        values.update(changes.get("s%d" % step, {}))
        result.append(values)
    return tuple(result)


GOOD = states(5)


def write_files_list(folder, step, files):
    """files_<step>.json in folder as case.ps1 writes it, for `files` (path -> bytes, None for a deleted
    file), with each content in folder\\files\\<sha256>."""
    os.makedirs(os.path.join(folder, "files"), exist_ok=True)
    entries = []
    for path, data in sorted(files.items()):
        if data is None:
            entries.append({"path": path, "deleted": True})
            continue
        sha = hashlib.sha256(data).hexdigest()
        with open(os.path.join(folder, "files", sha), "wb") as f:
            f.write(data)
        entries.append({"path": path, "size": len(data), "sha256": sha})
    with open(os.path.join(folder, "files_%d.json" % step), "w", encoding="utf-8") as f:
        json.dump({"step": step, "files": entries}, f)


def write_dump(folder, step, values, files=None):
    """dump_<step>.bin with these Globals and its files_<step>.json; returns the dump's path."""
    path = os.path.join(folder, "dump_%d.bin" % step)
    write(path, dump_bytes(step, values))
    write_files_list(folder, step, files or {})
    return path


class Run(NamedTuple):
    outcome: str = "ok"
    at_step: Optional[int] = None
    exit_code: Optional[int] = None
    dumps: Tuple[dict, ...] = GOOD
    detail: str = ""
    files: Tuple[dict, ...] = ()  # per step: the files the game had written by then (path -> bytes)


class FakeRunCase:
    """Stands in for runner.run_case: writes the planned run's dumps into out_dir."""

    def __init__(self):
        self.calls = []
        self.plans = {}

    def plan(self, game_dir, *runs):
        """The runs game_dir plays, in order; the last one repeats."""
        self.plans[game_dir] = list(runs)

    def __call__(self, case_file, game_dir, out_dir):
        self.calls.append((os.path.basename(case_file), game_dir))
        runs = self.plans[game_dir]
        run = runs.pop(0) if len(runs) > 1 else runs[0]
        os.makedirs(out_dir, exist_ok=True)
        paths = [write_dump(out_dir, step, values, run.files[step] if step < len(run.files) else {})
                 for step, values in enumerate(run.dumps)]
        return CaseResult(run.outcome, run.at_step, run.exit_code, paths, run.detail, 1.0)


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


class RunnerTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.ref = os.path.join(self.tmp, "ref")
        self.osx = os.path.join(self.tmp, "osx")
        self.cases = os.path.join(self.tmp, "cases")
        self.out = os.path.join(self.tmp, "out")
        self.exe = os.path.join(self.ref, "terranx_opensmacx.exe")
        self.dll = os.path.join(self.ref, "OpenSMACX.dll")
        self.save = os.path.join(self.ref, "saves", "Council.SAV")
        write(self.exe, b"reference exe")
        write(self.dll, b"harness dll 1")
        write(self.save, b"council save")
        write(os.path.join(self.osx, "saves", "Council.SAV"), b"council save")
        write(os.path.join(self.cases, "idle.case"), b"# no input\r\nsave Council.SAV\r\n")
        write(os.path.join(self.cases, "reports.case"), b"save Council.SAV\r\nkeys {F1}\r\nkeys {ESC}\r\n")
        self.driver = [os.path.join(self.tmp, "driver", "case.ps1"), os.path.join(self.tmp, "driver", "msg.ps1")]
        write(self.driver[0], b"case driver")
        write(self.driver[1], b"message helpers")
        self.fake = FakeRunCase()
        self.fake.plan(self.ref, Run())
        self.fake.plan(self.osx, Run())

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def run_pass(self, cases=("idle",), ignore=frozenset(), masks=()) -> PassResult:
        return run_pass(list(cases), self.ref, self.osx, self.out, run_case=self.fake,
                        cases_dir=self.cases, ignore=set(ignore), now=NOW, driver_files=self.driver, masks=masks)

    def one(self, ignore=frozenset(), masks=()) -> CaseOutcome:
        outcomes = self.run_pass(ignore=ignore, masks=masks).outcomes
        self.assertEqual(len(outcomes), 1)
        return outcomes[0]

    def sides(self):
        return [game_dir for _, game_dir in self.fake.calls]

    def key(self, case="idle"):
        return reference_key(self.exe, self.dll, self.save, os.path.join(self.cases, case + ".case"))

    def test_cache_hit_runs_osx_only(self):
        first = self.run_pass(("idle", "reports"))
        self.assertEqual([(o.case, o.status, o.cache) for o in first.outcomes],
                         [("idle", "pass", "miss"), ("reports", "pass", "miss")])
        self.assertEqual(self.sides(), [self.ref, self.ref, self.osx, self.ref, self.ref, self.osx])
        self.fake.calls.clear()
        second = self.run_pass(("idle", "reports"))
        self.assertEqual(self.fake.calls, [("idle.case", self.osx), ("reports.case", self.osx)])
        self.assertEqual([(o.status, o.cache) for o in second.outcomes], [("pass", "hit"), ("pass", "hit")])
        # the cached reference dumps are what the OpenSMACX side is compared with
        self.fake.plan(self.osx, Run(dumps=states(5, s4={"x": 7})))
        third = self.run_pass(("idle",)).outcomes[0]
        self.assertEqual((third.status, third.cache, third.at_step), ("fail", "hit", 4))
        self.assertEqual(third.differences, [Difference("Globals", None, "x", 0, 7)])

    def test_key_changes_with_dll(self):
        old = self.key()
        self.assertEqual(self.one().key, old)
        write(self.dll, b"harness dll 2")
        new = self.key()
        self.assertNotEqual(new, old)
        self.fake.calls.clear()
        outcome = self.one()
        self.assertEqual((outcome.status, outcome.cache, outcome.key), ("pass", "miss", new))
        self.assertEqual(self.sides(), [self.ref, self.ref, self.osx])
        self.assertEqual(sorted(os.listdir(os.path.join(self.out, "reference", "idle"))), sorted([old, new]))

    def test_key_covers_all_four_inputs(self):
        keys = {self.key()}
        for path in (self.exe, self.dll, self.save, os.path.join(self.cases, "idle.case")):
            with open(path, "ab") as f:
                f.write(b"!")
            keys.add(self.key())
        self.assertEqual(len(keys), 5)

    def test_self_check_disagreement_is_nondeterministic(self):
        self.fake.plan(self.ref, Run(), Run(dumps=states(5, s1={"turn": 2}, s3={"x": 1})))
        outcome = self.one()
        self.assertEqual((outcome.status, outcome.at_step, outcome.cache), ("nondeterministic", 1, "miss"))
        self.assertEqual(outcome.differences, [Difference("Globals", None, "turn", 1, 2)])
        self.assertEqual(self.sides(), [self.ref, self.ref])  # excluded from the comparison
        # stored as nondeterministic: the next pass runs nothing and says the same
        self.fake.calls.clear()
        again = self.one()
        self.assertEqual(self.fake.calls, [])
        self.assertEqual((again.status, again.at_step, again.cache), ("nondeterministic", 1, "hit"))
        self.assertEqual(again.differences, outcome.differences)

    def test_nondeterministic_rerecorded_when_ignore_set_changes(self):
        self.fake.plan(self.ref, Run(), Run(dumps=states(5, s1={"turn": 2})))
        self.assertEqual(self.one().status, "nondeterministic")
        self.fake.calls.clear()
        outcome = self.one(ignore={("Globals", "turn")})
        self.assertEqual((outcome.status, outcome.cache), ("pass", "miss"))
        self.assertEqual(self.sides(), [self.ref, self.ref, self.osx])

    def test_reference_rerecorded_when_ignore_set_changes(self):
        # recorded as a reference only because the runs' difference was ignored
        self.fake.plan(self.ref, Run(), Run(dumps=states(5, s1={"turn": 2})))
        self.assertEqual(self.one(ignore={("Globals", "turn")}).status, "pass")
        self.fake.plan(self.ref, Run(), Run(dumps=states(5, s1={"turn": 2})))
        self.fake.calls.clear()
        outcome = self.one()
        self.assertEqual((outcome.status, outcome.cache, outcome.at_step), ("nondeterministic", "miss", 1))
        self.assertEqual(self.sides(), [self.ref, self.ref])

    def test_reference_rerecorded_when_driver_changes(self):
        self.assertEqual(self.one().cache, "miss")
        self.assertEqual(self.one().cache, "hit")
        for path in self.driver:
            with open(path, "ab") as f:
                f.write(b"!")
            self.fake.calls.clear()
            outcome = self.one()
            self.assertEqual((outcome.status, outcome.cache), ("pass", "miss"), path)
            self.assertEqual(self.sides(), [self.ref, self.ref, self.osx], path)

    def test_unreadable_cached_dump_is_reference_broken_and_dropped(self):
        outcome = self.one()
        folder = os.path.join(self.out, "reference", "idle", outcome.key)
        write(os.path.join(folder, "dump_2.bin"), b"not a dump")
        self.fake.calls.clear()
        broken = self.one()
        self.assertEqual((broken.status, broken.cache), ("reference-broken", "hit"))
        self.assertIn("dump_2.bin", broken.detail)
        self.assertEqual(self.fake.calls, [])  # nothing to compare the OpenSMACX side with
        self.assertFalse(os.path.exists(folder))
        again = self.one()
        self.assertEqual((again.status, again.cache), ("pass", "miss"))
        self.assertEqual(self.sides(), [self.ref, self.ref, self.osx])

    def test_osx_crash_reported_as_crash(self):
        self.fake.plan(self.osx, Run("crash", 3, ACCESS_VIOLATION, GOOD[:3],
                                     "the game ended (exit code 0xC0000005) before clickpass\\dump_3.bin"))
        outcome = self.one()
        self.assertEqual((outcome.status, outcome.at_step, outcome.differences), ("crash", 3, []))
        self.assertIn("0xC0000005", outcome.detail)

    def test_osx_hang_reported_as_hang(self):
        self.fake.plan(self.osx, Run("hang", 2, None, GOOD[:2], "no clickpass\\dump_2.bin within 30 s"))
        outcome = self.one()
        self.assertEqual((outcome.status, outcome.at_step), ("hang", 2))
        self.assertIn("dump_2.bin", outcome.detail)

    def test_osx_blocked_write_reported_as_blocked_write(self):
        self.fake.plan(self.osx, Run("blocked-write", 4, None, GOOD, "the write guard refused a write"))
        outcome = self.one()
        self.assertEqual((outcome.status, outcome.at_step), ("blocked-write", 4))

    def test_osx_setup_failed_is_setup_failed(self):
        self.fake.plan(self.osx, Run("setup-failed", None, None, GOOD[:2], "driver error in step 2: boom"))
        outcome = self.one()
        self.assertEqual((outcome.status, outcome.at_step), ("setup-failed", None))
        self.assertIn("driver error in step 2", outcome.detail)

    def test_reference_crash_is_reference_broken(self):
        self.fake.plan(self.ref, Run("crash", 1, ACCESS_VIOLATION, GOOD[:1], "the game ended"))
        outcome = self.one()
        self.assertEqual((outcome.status, outcome.at_step), ("reference-broken", 1))
        self.assertIn("crash", outcome.detail)
        self.assertEqual(self.sides(), [self.ref])  # nothing to compare the OpenSMACX side with
        # not cached: the next pass records the reference again
        self.fake.plan(self.ref, Run())
        self.fake.calls.clear()
        self.assertEqual(self.one().status, "pass")
        self.assertEqual(self.sides(), [self.ref, self.ref, self.osx])

    def test_reference_hang_in_self_check_is_reference_broken(self):
        self.fake.plan(self.ref, Run(), Run("hang", 2, None, GOOD[:2], "no clickpass\\dump_2.bin"))
        outcome = self.one()
        self.assertEqual((outcome.status, outcome.at_step), ("reference-broken", 2))
        self.assertEqual(self.sides(), [self.ref, self.ref])

    def test_reference_setup_failed_is_reference_broken(self):
        self.fake.plan(self.ref, Run("setup-failed", None, None, (), "a game from ref is already running"))
        outcome = self.one()
        self.assertEqual((outcome.status, outcome.at_step), ("reference-broken", None))
        self.assertIn("already running", outcome.detail)

    def test_missing_reference_exe_is_reference_broken(self):
        os.remove(self.exe)
        outcome = self.one()
        self.assertEqual(outcome.status, "reference-broken")
        self.assertIn("terranx_opensmacx.exe", outcome.detail)
        self.assertEqual(self.fake.calls, [])

    def test_different_osx_save_is_setup_failed(self):
        write(os.path.join(self.osx, "saves", "Council.SAV"), b"another save")
        outcome = self.one()
        self.assertEqual(outcome.status, "setup-failed")
        self.assertIn("Council.SAV", outcome.detail)
        self.assertEqual(self.fake.calls, [])

    def test_first_difference_step(self):
        self.fake.plan(self.osx, Run(dumps=states(5, s2={"turn": 9}, s3={"turn": 9, "x": 5}, s4={"x": 5})))
        outcome = self.one()
        self.assertEqual((outcome.status, outcome.at_step), ("fail", 2))
        self.assertEqual(outcome.differences, [Difference("Globals", None, "turn", 1, 9)])
        self.assertEqual(str(outcome.differences[0]), "Globals.turn: 1 vs 9")

    def test_missing_osx_dump_is_a_difference(self):
        self.fake.plan(self.osx, Run(dumps=GOOD[:4]))
        outcome = self.one()
        self.assertEqual((outcome.status, outcome.at_step), ("fail", 4))
        self.assertEqual([str(d) for d in outcome.differences], ["dump_4.bin: present vs missing"])

    def test_refused_while_a_bisect_note_is_there(self):
        for game_dir in (self.osx, self.ref):
            write(os.path.join(game_dir, SWAP_NOTE), b"{}")
            with self.assertRaises(PassRefused) as caught:
                self.run_pass()
            self.assertIn("--recover", str(caught.exception))
            self.assertEqual(self.fake.calls, [])
            os.remove(os.path.join(game_dir, SWAP_NOTE))
        self.assertEqual(self.one().status, "pass")

    def test_case_setup_failed_when_a_bisect_starts_during_the_pass(self):
        fake = self.fake

        def run_case(case_file, game_dir, out_dir):
            result = fake(case_file, game_dir, out_dir)
            if game_dir == self.osx:  # a bisect swaps the exe after the first case
                write(os.path.join(self.osx, SWAP_NOTE), b"{}")
            return result
        outcomes = run_pass(["idle", "reports"], self.ref, self.osx, self.out, run_case=run_case,
                            cases_dir=self.cases, ignore=set(), now=NOW, driver_files=self.driver).outcomes
        self.assertEqual([o.status for o in outcomes], ["pass", "setup-failed"])
        self.assertIn(SWAP_NOTE, outcomes[1].detail)
        self.assertEqual(self.sides().count(self.osx), 1)  # the second case's OpenSMACX side wasn't played

    # -- the files the games write (final review F1)

    def saved(self, *steps):
        """Per-step files: the save written from the given step on (step -> its bytes)."""
        files, current = [], {}
        for step in range(5):
            if step in steps[0]:
                current = {SAVE_FILE: steps[0][step]}
            files.append(dict(current))
        return tuple(files)

    def test_file_written_by_one_game_only_fails(self):
        self.fake.plan(self.osx, Run(files=self.saved({3: b"save"})))
        outcome = self.one()
        self.assertEqual((outcome.status, outcome.at_step), ("fail", 3))
        self.assertEqual([str(d) for d in outcome.differences],
                         ["files[%s]: not written vs written (4 bytes)" % SAVE_FILE])
        self.assertIn("files[", json.dumps(self.result_json()["cases"][0]["differences"]))

    def test_file_bytes_compared_except_masked_ones(self):
        ref, osx = bytearray(32), bytearray(32)
        osx[8:12] = b"\xff" * 4  # a pointer the mask hides
        masks = [Mask("*.SAV", 8, 4, "a heap pointer")]
        self.fake.plan(self.ref, Run(files=self.saved({4: bytes(ref)})))
        self.fake.plan(self.osx, Run(files=self.saved({4: bytes(osx)})))
        self.assertEqual(self.one(masks=masks).status, "pass")
        osx[20] = 7
        self.fake.plan(self.osx, Run(files=self.saved({4: bytes(osx)})))
        outcome = self.one(masks=masks)
        self.assertEqual((outcome.status, outcome.at_step, outcome.cache), ("fail", 4, "hit"))
        self.assertEqual([str(d) for d in outcome.differences], ["files[%s]+0x14: 00 vs 07" % SAVE_FILE])
        # without the mask the pointer differs too
        self.assertEqual(len(self.one().differences), 5)

    def test_cached_reference_keeps_its_files(self):
        self.fake.plan(self.ref, Run(files=self.saved({2: b"abc"})))
        self.fake.plan(self.osx, Run(files=self.saved({2: b"abc"})))
        first = self.one()
        self.assertEqual((first.status, first.cache), ("pass", "miss"))
        folder = os.path.join(self.out, "reference", "idle", first.key)
        self.assertTrue(os.path.isfile(os.path.join(folder, "files_4.json")))
        self.assertTrue(os.path.isfile(os.path.join(folder, "files", hashlib.sha256(b"abc").hexdigest())))
        self.fake.calls.clear()
        self.fake.plan(self.osx, Run(files=self.saved({2: b"abd"})))
        outcome = self.one()
        self.assertEqual((outcome.status, outcome.at_step, outcome.cache), ("fail", 2, "hit"))
        self.assertEqual([str(d) for d in outcome.differences], ["files[%s]+0x02: 63 vs 64" % SAVE_FILE])
        self.assertEqual(self.sides(), [self.osx])

    def test_cached_reference_without_its_file_list_recorded_again(self):
        key = self.one().key
        os.remove(os.path.join(self.out, "reference", "idle", key, "files_3.json"))
        self.fake.calls.clear()
        self.assertEqual((self.one().status, self.one().cache), ("pass", "hit"))
        self.assertEqual(self.sides()[:3], [self.ref, self.ref, self.osx])

    def test_file_difference_between_reference_runs_is_nondeterministic(self):
        self.fake.plan(self.ref, Run(files=self.saved({4: b"\x01"})), Run(files=self.saved({4: b"\x02"})))
        outcome = self.one()
        self.assertEqual((outcome.status, outcome.at_step), ("nondeterministic", 4))
        self.assertEqual([str(d) for d in outcome.differences], ["files[%s]+0x00: 01 vs 02" % SAVE_FILE])

    def test_reference_rerecorded_when_file_masks_change(self):
        masks = [Mask("*.SAV", 0, 1, "why")]
        self.assertEqual(self.one(masks=masks).cache, "miss")
        self.assertEqual(self.one(masks=masks).cache, "hit")
        self.fake.calls.clear()
        outcome = self.one(masks=masks + [Mask("*.ini", "after:Latest Save=", "eol", "why")])
        self.assertEqual((outcome.status, outcome.cache), ("pass", "miss"))
        self.assertEqual(self.sides(), [self.ref, self.ref, self.osx])

    def test_missing_osx_file_list_is_setup_failed(self):
        fake = self.fake

        def run_case(case_file, game_dir, out_dir):
            result = fake(case_file, game_dir, out_dir)
            if game_dir == self.osx:
                os.remove(os.path.join(out_dir, "files_1.json"))
            return result
        outcome = run_pass(["idle"], self.ref, self.osx, self.out, run_case=run_case, cases_dir=self.cases,
                           ignore=set(), now=NOW, driver_files=self.driver, masks=()).outcomes[0]
        self.assertEqual(outcome.status, "setup-failed")
        self.assertIn("files_1.json", outcome.detail)

    # -- final review F8

    def test_unexpected_error_in_a_case_goes_on_with_the_next(self):
        fake = self.fake

        def run_case(case_file, game_dir, out_dir):
            if os.path.basename(case_file) == "idle.case":
                raise PermissionError(13, "the folder is in use", out_dir)
            return fake(case_file, game_dir, out_dir)
        result = run_pass(["idle", "reports"], self.ref, self.osx, self.out, run_case=run_case,
                          cases_dir=self.cases, ignore=set(), now=NOW, driver_files=self.driver, masks=())
        self.assertEqual([(o.case, o.status) for o in result.outcomes], [("idle", "setup-failed"), ("reports", "pass")])
        self.assertIn("PermissionError", result.outcomes[0].detail)
        self.assertEqual([c["status"] for c in self.result_json(result.run_dir)["cases"]], ["setup-failed", "pass"])

    def test_live_folder_refused_before_anything_is_written(self):
        # a stand-in live folder: if the check ever broke, this test must not write into the real one
        live = os.path.join(self.tmp, "live", "Sid Meier's Alpha Centauri")
        os.makedirs(live)
        for kwargs in ({"out_root": os.path.join(live, "clickpass")}, {"ref_dir": live}, {"osx_dir": live + "\\saves"},
                       {"cache_root": os.path.join(live, "ref")}, {"out_root": os.path.dirname(live)}):
            args = dict(out_root=self.out, ref_dir=self.ref, osx_dir=self.osx, cache_root=None)
            args.update(kwargs)
            with self.assertRaises(PassRefused, msg=kwargs) as caught:
                run_pass(["idle"], args["ref_dir"], args["osx_dir"], args["out_root"], run_case=self.fake,
                         cases_dir=self.cases, cache_root=args["cache_root"], ignore=set(), now=NOW,
                         driver_files=self.driver, masks=(), live_dir=live)
            self.assertIn("live game folder", str(caught.exception))
        self.assertEqual(self.fake.calls, [])
        self.assertFalse(os.path.exists(self.out))
        self.assertEqual(os.listdir(live), [])

    def test_live_refusal_names_the_real_live_folder(self):
        live = runner.LIVE_DIR  # (only compared, nothing is written)
        self.assertEqual(live, r"G:\Sid Meier's Alpha Centauri")
        for folder in (live, live.lower() + "\\", os.path.join(live, "saves"), os.path.dirname(live)):
            self.assertIn("live game folder", runner.live_refusal([(folder, "the out root")]), folder)
        self.assertEqual(runner.live_refusal([(r"G:\Sid Meier's Alpha Centauri 2", "x"), (self.out, "y")]), "")

    def result_json(self, run_dir=None):
        run_dir = run_dir or os.path.join(self.out, sorted(n for n in os.listdir(self.out) if n != "reference")[-1])
        with open(os.path.join(run_dir, "result.json"), encoding="utf-8") as f:
            return json.load(f)

    def test_run_folder_and_result_json(self):
        first = self.run_pass(("idle", "reports"))
        second = self.run_pass(("idle",))
        self.assertEqual(first.run_dir, os.path.join(self.out, "2026-10-18-0507"))
        self.assertEqual(second.run_dir, os.path.join(self.out, "2026-10-18-0507-2"))
        with open(os.path.join(first.run_dir, "result.json"), encoding="utf-8") as f:
            data = json.load(f)
        self.assertEqual([(c["case"], c["status"], c["cache"]) for c in data["cases"]],
                         [("idle", "pass", "miss"), ("reports", "pass", "miss")])
        self.assertEqual([r["side"] for r in data["cases"][0]["runs"]], ["reference-1", "reference-2", "osx"])
        self.assertEqual(sorted(os.listdir(os.path.join(first.run_dir, "idle"))),
                         ["osx", "reference-1", "reference-2"])
        self.assertEqual(os.listdir(os.path.join(second.run_dir, "idle")), ["osx"])


GAME_START = 133700000000000000  # a UTC FILETIME, as case.ps1 writes it into game.pid


class FakeProcess:
    def __init__(self, exe, start):
        self.exe, self.start = exe, start
        self.terminated = self.closed = False

    def terminate(self):
        self.terminated = True

    def close(self):
        self.closed = True


class StopGameTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.game_dir = os.path.join(self.tmp, "clickref")
        self.exe = os.path.join(self.game_dir, "terranx_opensmacx.exe")
        self.pid_file = os.path.join(self.tmp, "out", "game.pid")
        write(self.pid_file, b"4242 %d\r\n" % GAME_START)
        self.opened = []

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def stop(self, process):
        def open_process(pid):
            self.opened.append(pid)
            return process
        return stop_game(self.pid_file, self.game_dir, open_process=open_process)

    def test_stops_the_game_it_started(self):
        process = FakeProcess(self.exe, GAME_START)
        text = self.stop(process)
        self.assertEqual(self.opened, [4242])
        self.assertTrue(process.terminated)
        self.assertTrue(process.closed)
        self.assertIn("4242", text)

    def test_folder_compared_without_case(self):
        process = FakeProcess(self.exe.upper(), GAME_START)
        self.stop(process)
        self.assertTrue(process.terminated)

    def test_process_already_ended(self):
        self.assertIn("4242", self.stop(None))
        self.assertEqual(self.opened, [4242])

    def test_exe_outside_the_game_folder_left_alone(self):
        for exe in (os.path.join(self.tmp, "clickref2", "terranx_opensmacx.exe"),
                    os.path.join(self.tmp, "terranx_opensmacx.exe"), r"C:\Windows\notepad.exe"):
            process = FakeProcess(exe, GAME_START)
            self.assertIn("left alone", self.stop(process))
            self.assertFalse(process.terminated, exe)
            self.assertTrue(process.closed, exe)

    def test_other_start_time_left_alone(self):
        process = FakeProcess(self.exe, GAME_START + 1)  # the PID was reused
        self.assertIn("left alone", self.stop(process))
        self.assertFalse(process.terminated)

    def test_no_or_bad_pid_file_opens_nothing(self):
        process = FakeProcess(self.exe, GAME_START)
        os.remove(self.pid_file)
        self.stop(process)
        write(self.pid_file, b"4242")
        self.stop(process)
        self.assertEqual(self.opened, [])
        self.assertFalse(process.terminated)

    def test_open_process_reads_a_running_process(self):
        process = runner._open_process(os.getpid())  # (never terminated here)
        self.assertIsNotNone(process)
        try:
            self.assertTrue(os.path.basename(process.exe).lower().startswith("python"), process.exe)
            self.assertGreater(process.start, 116444736000000000)  # after 1970
        finally:
            process.close()
        self.assertIsNone(runner._open_process(0))


class RunCaseTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.game_dir = os.path.join(self.tmp, "clickref")
        self.out = os.path.join(self.tmp, "out")
        self.process = FakeProcess(os.path.join(self.game_dir, "terranx_opensmacx.exe"), GAME_START)

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def run_case(self, error, writes_pid=True):
        def fake_run(cmd, **kwargs):
            if writes_pid:
                write(os.path.join(self.out, "game.pid"), b"4242 %d\r\n" % GAME_START)
            raise error
        with mock.patch.object(runner.subprocess, "run", side_effect=fake_run), \
                mock.patch.object(runner, "_open_process", return_value=self.process) as open_process:
            return runner.run_case("x.case", self.game_dir, self.out, timeout=5), open_process

    def test_timeout_stops_the_game(self):
        r, _ = self.run_case(subprocess.TimeoutExpired("powershell.exe", 5))
        self.assertEqual((r.outcome, r.at_step), ("setup-failed", None))
        self.assertTrue(self.process.terminated)
        self.assertIn("4242", r.detail)

    def test_interrupt_stops_the_game_and_reraises(self):
        with self.assertRaises(KeyboardInterrupt):
            self.run_case(KeyboardInterrupt())
        self.assertTrue(self.process.terminated)

    def test_pid_file_of_an_earlier_run_is_not_used(self):
        write(os.path.join(self.out, "game.pid"), b"4242 %d\r\n" % GAME_START)
        _, open_process = self.run_case(subprocess.TimeoutExpired("powershell.exe", 5), writes_pid=False)
        open_process.assert_not_called()
        self.assertFalse(self.process.terminated)


class ReadResultTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def test_reads_case_ps1_result(self):
        result = {"case": "reports", "save": "Council.SAV", "steps": 11, "outcome": "crash", "at_step": 10,
                  "exit_code": 3221225477, "seconds": 23, "detail": "the game ended (exit code 0xC0000005)",
                  "errors": ["ERROR dump x (GetLastError 5)"],
                  "blocked": ["WRITE-BLOCKED CreateFileA G:\\elsewhere\\x.SAV"],
                  "warning": "could not put back Alpha Centauri.ini"}
        write(os.path.join(self.tmp, "result.json"), json.dumps(result).encode("utf-8"))
        for step in (0, 1, 2, 9, 10):
            write(os.path.join(self.tmp, "dump_%d.bin" % step), b"")
        r = read_result(self.tmp)
        self.assertEqual((r.outcome, r.at_step, r.exit_code, r.seconds), ("crash", 10, ACCESS_VIOLATION, 23.0))
        self.assertEqual([os.path.basename(p) for p in r.dumps],
                         ["dump_0.bin", "dump_1.bin", "dump_2.bin", "dump_9.bin", "dump_10.bin"])
        self.assertIn("exit code 0xC0000005", r.detail)
        self.assertIn("ERROR dump x", r.detail)
        self.assertIn("WRITE-BLOCKED CreateFileA", r.detail)
        self.assertIn("could not put back Alpha Centauri.ini", r.detail)

    def test_no_result_json_is_setup_failed(self):
        r = read_result(self.tmp)
        self.assertEqual((r.outcome, r.at_step, r.dumps), ("setup-failed", None, []))
        self.assertIn("result.json", r.detail)


class CommandLineTest(unittest.TestCase):
    def run_cli(self, argv, statuses, warning="", refused=None, report=None, reference=""):
        outcomes = [CaseOutcome("idle", s, None, [], "") for s in statuses]
        with mock.patch.object(cli, "run_pass", return_value=PassResult("run", outcomes),
                               side_effect=refused) as run_pass_, \
                mock.patch.object(cli, "reference_exe_refusal", return_value=reference) as self.reference_check, \
                mock.patch.object(cli, "exe_table_warning", return_value=warning) as self.table_check, \
                mock.patch.object(cli, "report_pass", return_value=os.path.join("run", "report.md"),
                                  side_effect=report) as self.report_pass, \
                contextlib.redirect_stdout(io.StringIO()) as out:
            code = cli.main(argv)
        return code, run_pass_, out.getvalue()

    def test_run_warns_when_the_exe_is_not_the_tables(self):
        code, _, out = self.run_cli(["run", "--cases", "idle"], ["pass"], warning="WARNING: x is not t's patch")
        self.assertEqual(code, 0)
        lines = out.splitlines()
        self.assertEqual(lines[0], "WARNING: x is not t's patch")  # before the pass
        self.assertEqual(lines[-3], "WARNING: x is not t's patch")  # and in the summary, before results:
        self.assertNotIn("WARNING", self.run_cli(["run", "--cases", "idle"], ["pass"])[2])
        # and in result.json, for the report
        self.assertEqual(self.run_cli(["run", "--cases", "idle"], ["pass"], warning="WARNING: w")[1].call_args[1]
                         ["warnings"], ["WARNING: w"])

    def test_run_writes_the_report(self):
        code, _, out = self.run_cli(["run", "--cases", "idle", "--table", "T"], ["fail"])
        self.assertEqual(code, 1)
        self.report_pass.assert_called_once_with("run", table="T")
        self.assertEqual(out.splitlines()[-1], "report: " + os.path.join("run", "report.md"))
        # a report that can't be written is said, and the exit code stays the cases'
        code, _, out = self.run_cli(["run", "--cases", "idle"], ["pass"], report=OSError("disk full"))
        self.assertEqual(code, 0)
        self.assertIn("can't write the report for run: disk full", out)

    def test_run_refused_while_a_bisect_note_is_there(self):
        code, _, out = self.run_cli(["run", "--cases", "idle"], [], refused=PassRefused("note.json exists"))
        self.assertEqual(code, 1)
        self.assertIn("note.json exists", out)

    def test_run_passes_the_cases_and_folders(self):
        code, run_pass_, out = self.run_cli(["run", "--cases", "idle,reports", "--ref-dir", "R", "--osx-dir", "O"],
                                            ["pass", "pass"])
        self.assertEqual(code, 0)
        args, kwargs = run_pass_.call_args
        self.assertEqual(args[:3], (["idle", "reports"], "R", "O"))
        self.assertIn("pass", out)

    def test_run_refuses_a_reference_exe_that_is_not_the_approved_rows_patch(self):
        code, run_pass_, out = self.run_cli(["run", "--cases", "idle", "--ref-dir", "R", "--table", "T",
                                             "--pristine", "P"], ["pass"],
                                            reference="the reference exe R\\x.exe is not the approved rows' patch")
        self.assertEqual(code, 1)
        self.assertIn("can't run: the reference exe R\\x.exe is not the approved rows' patch", out)
        run_pass_.assert_not_called()
        self.reference_check.assert_called_once_with("R", "T", "P")
        self.assertEqual(self.run_cli(["run", "--cases", "idle"], ["pass"])[0], 0)
        self.reference_check.assert_called_once()

    def test_run_refuses_folders_in_the_live_folder_first(self):
        live = runner.LIVE_DIR  # (only compared: every step after the check is mocked)
        for option, folder in (("--out-root", os.path.join(live, "clickpass")), ("--ref-dir", live),
                               ("--osx-dir", os.path.join(live, "x")), ("--out-root", os.path.dirname(live))):
            code, run_pass_, out = self.run_cli(["run", "--cases", "idle", option, folder], ["pass"])
            self.assertEqual(code, 1, option)
            self.assertIn("live game folder", out)
            run_pass_.assert_not_called()
            self.reference_check.assert_not_called()
            self.table_check.assert_not_called()

    def test_reference_command(self):
        with mock.patch.object(cli, "install_reference", return_value="installed R\\x.exe") as install, \
                mock.patch.object(cli, "reference_exe_refusal", return_value="") as check, \
                contextlib.redirect_stdout(io.StringIO()) as out:
            self.assertEqual(cli.main(["reference", "--ref-dir", "R", "--dll", "D", "--table", "T", "--pristine", "P"]),
                             0)
        install.assert_called_once_with("R", "D", "T", "P")
        check.assert_called_once_with("R", "T", "P")
        self.assertEqual(out.getvalue().splitlines(), ["installed R\\x.exe", "checked: R is the reference `run` expects"])
        with mock.patch.object(cli, "install_reference", side_effect=bisect_module.BisectError("a game runs")), \
                mock.patch.object(cli, "reference_exe_refusal") as check, \
                contextlib.redirect_stdout(io.StringIO()) as out:
            self.assertEqual(cli.main(["reference", "--ref-dir", "R"]), 1)
        check.assert_not_called()
        self.assertIn("can't make R the reference folder: a game runs", out.getvalue())

    def test_exit_code_ignores_nondeterministic_only(self):
        self.assertEqual(self.run_cli(["run", "--cases", "idle"], ["pass", "nondeterministic"])[0], 0)
        for status in ("fail", "crash", "hang", "blocked-write", "setup-failed", "reference-broken"):
            self.assertEqual(self.run_cli(["run", "--cases", "idle"], ["pass", status])[0], 1, status)


if __name__ == "__main__":
    unittest.main()
