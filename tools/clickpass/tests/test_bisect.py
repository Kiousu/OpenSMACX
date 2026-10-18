import contextlib
import hashlib
import io
import json
import os
import shutil
import tempfile
import unittest
from datetime import datetime
from unittest import mock

from clickpass import bisect as bisect_module
from clickpass import clickpass as cli
from clickpass.bisect import (SWAP_ORIGINAL, BisectError, BisectResult, ExeSwap, Failure, bisect_case, copy_retrying,
                              exe_table_warning, find_culprits, find_pass, format_bisect, install_reference, judge,
                              recover_swap, reference_exe_refusal, subset_table)
from clickpass.runner import SWAP_NOTE, CaseResult, PassRefused, run_pass
from clickpass.tables import Row, read_rows, write_table
from clickpass.tests.test_runner import GOOD, FakeRunCase, Run, states, write, write_dump

APPROVED_NAMES = ("?battle_compute@@YAXHHPAH0H@Z", "?best_defender@@YAIIHH@Z", "?game_data@@YAHPAU_iobuf@@H@Z")
EXE = "terranx_opensmacx.exe"
FULL_EXE = b"the full table's exe"


def make_rows(count, approved_at=()):
    """`count` candidate rows in offset order, with the three approved rows placed just before the
    candidates at the given indexes (as in tools/redirects.tsv, where they sit among the others)."""
    approved = dict(zip(approved_at, APPROVED_NAMES))
    rows = []
    for i in range(count):
        if i in approved:
            rows.append(Row(0x00200000 + i * 0x10 - 8, approved[i], True))
        rows.append(Row(0x00200000 + i * 0x10, "?f%d@@YAXXZ" % i, False))
    return rows


def candidates(rows):
    return [row for row in rows if not row.approved]


class FakeReproduces:
    """Stands in for the real reproduces(): records each subset and applies `rule` to it."""

    def __init__(self, rule):
        self.rule = rule
        self.calls = []

    def __call__(self, subset):
        self.calls.append(list(subset))
        return self.rule(set(subset))


class FindCulpritsTest(unittest.TestCase):
    def test_single_culprit(self):
        rows = make_rows(700)
        fake = FakeReproduces(lambda s: rows[37] in s)
        result = find_culprits(rows, fake)
        self.assertEqual(result.culprits, [rows[37]])
        self.assertTrue(result.clean)
        self.assertLessEqual(result.runs, 11)
        self.assertEqual(result.runs, len(fake.calls))

    def test_two_culprits(self):
        rows = make_rows(700)
        fake = FakeReproduces(lambda s: rows[5] in s or rows[600] in s)
        result = find_culprits(rows, fake)
        self.assertCountEqual(result.culprits, [rows[5], rows[600]])
        self.assertTrue(result.clean)  # each of them reproduces the failure alone
        self.assertEqual(result.runs, len(fake.calls))
        # the run after the last culprit: every other row, which no longer reproduces
        self.assertEqual(fake.calls[-1], [row for row in rows if row not in (rows[5], rows[600])])

    def test_interaction_not_clean(self):
        rows = make_rows(700)
        fake = FakeReproduces(lambda s: rows[10] in s and rows[20] in s)
        result = find_culprits(rows, fake)
        self.assertIn(rows[10], result.culprits)
        self.assertIn(rows[20], result.culprits)
        self.assertFalse(result.clean)
        # the set at the split where neither half reproduces
        self.assertEqual(result.culprits, rows[10:21])

    def test_approved_rows_always_present(self):
        rows = make_rows(700, approved_at=(0, 333, 699))
        approved = [row for row in rows if row.approved]
        self.assertEqual(len(approved), 3)
        c = candidates(rows)
        rules = (lambda s: c[37] in s, lambda s: c[5] in s or c[600] in s, lambda s: c[10] in s and c[20] in s)
        for rule in rules:
            fake = FakeReproduces(rule)
            result = find_culprits(rows, fake)
            self.assertTrue(fake.calls)
            for subset in fake.calls:
                for row in approved:
                    self.assertIn(row, subset)
                self.assertEqual(subset, [row for row in rows if row in subset])  # in table order
            for row in approved:
                self.assertNotIn(row, result.culprits)
            self.assertTrue(result.culprits)

    def test_search_order(self):
        # first half first; into the second half untested when the first doesn't reproduce; the row it
        # comes down to is tested alone; then one run of every other row
        rows = make_rows(8)
        fake = FakeReproduces(lambda s: rows[5] in s)
        self.assertEqual(find_culprits(rows, fake), BisectResult([rows[5]], 5, True))
        self.assertEqual(fake.calls, [rows[0:4], rows[4:6], rows[4:5], rows[5:6], rows[:5] + rows[6:]])

    def test_interaction_across_the_first_split(self):
        rows = make_rows(8)
        fake = FakeReproduces(lambda s: rows[1] in s and rows[6] in s)
        result = find_culprits(rows, fake)
        self.assertEqual((result.culprits, result.clean), (rows, False))
        # the untested second half is tested once the row it came down to doesn't reproduce alone
        self.assertEqual(fake.calls, [rows[0:4], rows[4:6], rows[6:7], rows[7:8], rows[4:8]])

    def test_interaction_inside_a_half(self):
        rows = make_rows(8)
        fake = FakeReproduces(lambda s: rows[4] in s and rows[5] in s)
        result = find_culprits(rows, fake)
        self.assertEqual((result.culprits, result.clean), (rows[4:6], False))

    def test_subset_table_keeps_approved_rows_and_order(self):
        rows = make_rows(6, approved_at=(2,))
        c = candidates(rows)
        self.assertEqual(subset_table(rows, [c[4], c[0]]), [c[0], rows[2], c[4]])


class JudgeTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.ref = self.dumps("ref", GOOD)

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def dumps(self, name, values):
        return [write_dump(os.path.join(self.tmp, name), step, v) for step, v in enumerate(values)]

    def judge(self, failure, outcome="ok", at_step=None, values=GOOD, name="run"):
        r = CaseResult(outcome, at_step, 0xC0000005 if outcome == "crash" else None, self.dumps(name, values),
                       "what happened")
        return judge(r, failure, self.ref, set())

    def test_difference_at_the_failing_step_reproduces(self):
        ok, note = self.judge(Failure("fail", 2), values=states(5, s2={"x": 7}, s3={"x": 7}))
        self.assertTrue(ok)
        self.assertIn("step 2", note)
        self.assertIn("Globals.x: 0 vs 7", note)

    def test_no_difference_does_not_reproduce(self):
        ok, note = self.judge(Failure("fail", 2))
        self.assertFalse(ok)
        self.assertIn("no differences", note)

    def test_difference_at_another_step_does_not_reproduce(self):
        for values in (states(5, s1={"x": 7}), states(5, s3={"x": 7})):
            ok, note = self.judge(Failure("fail", 2), values=values)
            self.assertFalse(ok)
            self.assertIn("counted as not reproducing", note)

    def test_crash_when_the_failure_was_a_difference(self):
        for outcome in ("crash", "hang", "blocked-write"):
            ok, note = self.judge(Failure("fail", 2), outcome, 2, GOOD[:2])
            self.assertFalse(ok, outcome)
            self.assertIn(outcome, note)
            self.assertIn("counted as not reproducing", note)

    def test_same_kind_at_the_same_step_reproduces(self):
        for status in ("crash", "hang", "blocked-write"):
            failure = Failure(status, 2)
            self.assertTrue(self.judge(failure, status, 2, GOOD[:2])[0], status)
            self.assertFalse(self.judge(failure, status, 1, GOOD[:1])[0], status)
            other = "hang" if status != "hang" else "crash"
            ok, note = self.judge(failure, other, 2, GOOD[:2])
            self.assertFalse(ok, status)
            self.assertIn("counted as not reproducing", note)
            self.assertFalse(self.judge(failure, "ok", None, states(5, s2={"x": 7}))[0], status)

    def test_setup_failed_stops_the_bisection(self):
        with self.assertRaises(BisectError):
            self.judge(Failure("fail", 2), "setup-failed", None, ())


def read(path):
    with open(path, "rb") as f:
        return f.read()


def ended(pid, start):
    return False


def sha(data):
    return hashlib.sha256(data).hexdigest()


class ExeSwapTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.game = os.path.join(self.tmp, "game")
        self.exe = os.path.join(self.game, EXE)
        self.subset = os.path.join(self.tmp, "patched", EXE)
        write(self.exe, FULL_EXE)
        write(self.subset, b"a subset's exe")
        self.marker = os.path.join(self.game, SWAP_NOTE)
        self.backup = os.path.join(self.game, SWAP_ORIGINAL)

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def note(self):
        with open(self.marker, encoding="utf-8") as f:
            return json.load(f)

    def killed_swap(self):
        """A bisect that installed the subset exe and was killed (no __exit__)."""
        swap = ExeSwap(self.exe).__enter__()
        swap.install(self.subset)
        return swap

    def test_restored_at_the_end(self):
        os.utime(self.exe, (1700000000, 1700000000))
        with ExeSwap(self.exe) as swap:
            swap.install(self.subset)
            self.assertEqual(read(self.exe), b"a subset's exe")
            self.assertEqual(read(self.backup), FULL_EXE)
            self.assertEqual(self.note()["installed"], [sha(b"a subset's exe")])
        self.assertEqual(read(self.exe), FULL_EXE)
        self.assertEqual(os.path.getmtime(self.exe), 1700000000)  # its timestamp too
        self.assertEqual(os.listdir(self.game), [EXE])  # no note, no copy
        self.assertTrue(swap.restored)

    def test_restored_after_an_error_or_ctrl_c(self):
        for error in (RuntimeError("boom"), KeyboardInterrupt()):
            with self.assertRaises(type(error)):
                with ExeSwap(self.exe) as swap:
                    swap.install(self.subset)
                    raise error
            self.assertEqual(read(self.exe), FULL_EXE)
            self.assertEqual(os.listdir(self.game), [EXE])

    def test_note_names_an_exe_before_it_is_installed(self):
        with ExeSwap(self.exe) as swap:
            with mock.patch.object(bisect_module.shutil, "copyfile", side_effect=OSError("locked")):
                with self.assertRaises(BisectError):
                    swap.install(self.subset)
            self.assertEqual(self.note()["installed"], [sha(b"a subset's exe")])
            self.assertEqual(self.note()["pid"], os.getpid())

    def test_cleanup_failure_keeps_the_error_and_the_restore(self):
        real_remove = os.remove

        def remove(path):
            if path == self.marker:
                raise PermissionError("in use")
            real_remove(path)
        with self.assertRaises(RuntimeError):  # the body's error, not the cleanup's
            with mock.patch.object(bisect_module.os, "remove", side_effect=remove):
                with ExeSwap(self.exe) as swap:
                    swap.install(self.subset)
                    raise RuntimeError("boom")
        self.assertTrue(swap.restored)
        self.assertIn(self.marker, swap.cleanup_error)
        self.assertEqual(read(self.exe), FULL_EXE)
        # the leftover note: the exe is the original, so recovery only removes it
        self.assertIn("already the original", recover_swap(self.game, running=ended))
        self.assertEqual(os.listdir(self.game), [EXE])

    def test_refused_without_its_own_start_time(self):
        with mock.patch.object(bisect_module, "process_start", return_value=None):
            with self.assertRaises(BisectError):
                ExeSwap(self.exe).__enter__()
        self.assertEqual(os.listdir(self.game), [EXE])

    def test_restore_checks_the_hash(self):
        with self.assertRaises(BisectError) as caught:
            with ExeSwap(self.exe) as swap:
                swap.install(self.subset)
                write(swap.backup, b"damaged backup")
        self.assertIn(self.exe, str(caught.exception))
        self.assertTrue(os.path.exists(self.marker))  # the next bisect tries again

    def test_refused_while_another_swap_is_open(self):
        with ExeSwap(self.exe):
            with self.assertRaises(BisectError):
                ExeSwap(self.exe).__enter__()
        self.assertEqual(read(self.exe), FULL_EXE)

    def test_recover_restores_an_exe_the_ended_bisect_installed(self):
        os.utime(self.exe, (1700000000, 1700000000))
        self.killed_swap()
        text = recover_swap(self.game, running=ended)
        self.assertIn("restored", text)
        self.assertEqual(read(self.exe), FULL_EXE)
        self.assertEqual(os.path.getmtime(self.exe), 1700000000)
        self.assertEqual(os.listdir(self.game), [EXE])
        self.assertEqual(recover_swap(self.game), "")

    def test_recover_leaves_an_exe_installed_since_alone(self):
        self.killed_swap()
        write(self.exe, b"the next batch's exe")  # e.g. a batch promoted after the kill
        text = recover_swap(self.game, running=ended)
        self.assertIn("left %s alone" % self.exe, text)
        self.assertEqual(read(self.exe), b"the next batch's exe")
        self.assertEqual(os.listdir(self.game), [EXE])

    def test_recover_with_the_original_back_removes_the_note(self):
        self.killed_swap()
        write(self.exe, FULL_EXE)
        self.assertIn("already the original", recover_swap(self.game, running=ended))
        self.assertEqual(os.listdir(self.game), [EXE])

    def test_recover_leaves_a_running_bisect_alone(self):
        swap = self.killed_swap()
        with self.assertRaises(BisectError):
            recover_swap(self.game)  # (this process: still running)
        self.assertEqual(read(self.exe), b"a subset's exe")
        swap.__exit__(None, None, None)
        self.assertEqual(read(self.exe), FULL_EXE)


class FakeGame:
    """run_case for the bisect: the installed exe holds the patched table's text (see fake_patch);
    the game fails as `failure` (a Run) when the exe has every row in `culprits`, else plays `other`
    when it has every row in `noise`, else matches the reference."""

    def __init__(self, culprits, failure, noise=(), other=None, setup_failed_at=None):
        self.culprits, self.failure, self.noise, self.other = culprits, failure, noise, other
        self.setup_failed_at = setup_failed_at
        self.tables = []

    def __call__(self, case_file, game_dir, out_dir):
        table = read_rows(os.path.join(game_dir, EXE))
        self.tables.append(table)
        if len(self.tables) == self.setup_failed_at:
            run = Run("setup-failed", None, None, (), "a game from that folder is already running")
        elif all(row in table for row in self.culprits):
            run = self.failure
        elif self.noise and all(row in table for row in self.noise):
            run = self.other
        else:
            run = Run()
        fake = FakeRunCase()
        fake.plan(game_dir, run)
        return fake(case_file, game_dir, out_dir)


class CopyRetryingTest(unittest.TestCase):
    def test_retries_while_the_exe_is_in_use(self):
        calls, waits = [], []

        def copy(source, target):
            calls.append((source, target))
            if len(calls) < 3:
                raise PermissionError(13, "The process cannot access the file because it is being used by another "
                                          "process", target)
        copy_retrying(copy, "new.exe", "game.exe", sleep=waits.append)
        self.assertEqual((len(calls), waits), (3, [1.0, 1.0]))

    def test_gives_up_and_other_errors_are_not_retried(self):
        waits = []

        def in_use(source, target):
            raise PermissionError(13, "in use", target)
        with self.assertRaises(PermissionError):
            copy_retrying(in_use, "a", "b", tries=4, sleep=waits.append)
        self.assertEqual(len(waits), 3)

        def missing(source, target):
            raise FileNotFoundError(2, "no such file", source)
        with self.assertRaises(FileNotFoundError):
            copy_retrying(missing, "a", "b", sleep=self.fail)


class ReferenceFolderTest(unittest.TestCase):
    """The reference folder's exe: reference_exe_refusal (checked before every pass) and
    install_reference (`clickpass.py reference`)."""

    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.ref = os.path.join(self.tmp, "ref")
        write(os.path.join(self.ref, "saves", "Council.SAV"), b"council save")
        self.rows = make_rows(10, approved_at=(2, 5, 8))
        self.table = os.path.join(self.tmp, "redirects.tsv")
        write_table(self.rows, self.table)
        self.dll = os.path.join(self.tmp, "build", "OpenSMACX.dll")
        write(self.dll, b"the new harness dll")
        self.patched = []

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def fake_patch(self, table_path, out_dir):
        """Stands in for opensmacx_patch.py: the "exe" is the table's text."""
        self.patched.append(read_rows(table_path))
        exe = os.path.join(out_dir, EXE)
        shutil.copyfile(table_path, exe)
        return exe

    def check(self, patch=None):
        return reference_exe_refusal(self.ref, self.table, patch=patch or self.fake_patch)

    def install(self, ref=None, running=lambda folder: [], live_dir=None):
        return install_reference(ref or self.ref, self.dll, self.table, patch=self.fake_patch, running=running,
                                 live_dir=live_dir or os.path.join(self.tmp, "live"))

    def test_only_the_approved_rows_patch_passes(self):
        self.assertIn("is not", self.check())  # (no exe there yet)
        text = self.install()
        self.assertIn("sha256", text)
        self.assertEqual(self.check(), "")
        approved = [row for row in self.rows if row.approved]
        self.assertTrue(self.patched)
        for rows in self.patched:
            self.assertEqual(rows, approved)
        with open(os.path.join(self.ref, "OpenSMACX.dll"), "rb") as f:
            self.assertEqual(f.read(), b"the new harness dll")  # its own DLL (R19)
        write(os.path.join(self.ref, EXE), FULL_EXE)  # e.g. the OpenSMACX exe copied there by mistake
        refusal = self.check()
        self.assertIn("is not", refusal)
        self.assertIn("clickpass.py reference", refusal)

        def broken(table_path, out_dir):
            raise BisectError("opensmacx_patch.py failed (exit code 1): No module named pefile")
        self.assertIn("can't check", self.check(broken))

    def test_install_refusals_write_nothing(self):
        live = os.path.join(self.tmp, "live")
        os.makedirs(live)
        refusals = [(dict(ref=os.path.join(live, "saves")), "live game folder"),
                    (dict(ref=self.tmp), "live game folder"),  # (it holds the stand-in live folder)
                    (dict(running=lambda folder: [4242]), "4242"),
                    (dict(ref=os.path.join(self.tmp, "missing")), "no folder")]
        for kwargs, words in refusals:
            with self.assertRaises(BisectError, msg=words) as caught:
                self.install(live_dir=live, **kwargs)
            self.assertIn(words, str(caught.exception))
        write(os.path.join(self.ref, SWAP_NOTE), b"{}")
        with self.assertRaises(BisectError):
            self.install(live_dir=live)
        self.assertEqual(sorted(os.listdir(self.ref)), sorted(["saves", SWAP_NOTE]))
        self.assertEqual(os.listdir(live), [])
        self.assertEqual(self.patched, [])


class BisectCaseTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.ref = os.path.join(self.tmp, "ref")
        self.osx = os.path.join(self.tmp, "osx")
        self.cases = os.path.join(self.tmp, "cases")
        self.out = os.path.join(self.tmp, "out")
        for folder in (self.ref, self.osx):
            write(os.path.join(folder, EXE), b"reference exe" if folder == self.ref else FULL_EXE)
            write(os.path.join(folder, "OpenSMACX.dll"), b"harness dll")
            write(os.path.join(folder, "saves", "Council.SAV"), b"council save")
        write(os.path.join(self.cases, "reports.case"), b"save Council.SAV\r\nkeys {F1}\r\nkeys {ESC}\r\n")
        self.driver = [os.path.join(self.tmp, "driver", "case.ps1"), os.path.join(self.tmp, "driver", "msg.ps1")]
        write(self.driver[0], b"case driver")
        write(self.driver[1], b"message helpers")
        self.rows = make_rows(40, approved_at=(3, 17, 30))
        self.table = os.path.join(self.tmp, "redirects.tsv")
        write_table(self.rows, self.table)
        self.patched = []

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def fake_patch(self, table_path, out_dir):
        """Stands in for opensmacx_patch.py: the "exe" is the table's text."""
        self.patched.append(read_rows(table_path))
        exe = os.path.join(out_dir, EXE)
        shutil.copyfile(table_path, exe)
        return exe

    def pass_with(self, osx_run, now=datetime(2026, 10, 18, 5, 7)):
        fake = FakeRunCase()
        fake.plan(self.ref, Run())
        fake.plan(self.osx, osx_run)
        return run_pass(["reports"], self.ref, self.osx, self.out, run_case=fake, cases_dir=self.cases,
                        ignore=set(), now=now, driver_files=self.driver).run_dir

    def bisect(self, game, run_dir=None):
        return bisect_case("reports", self.out, run_dir=run_dir, table=self.table, patch=self.fake_patch,
                           run_case=game, ignore=set(), driver_files=self.driver)

    def bisect_json(self, run_dir):
        with open(os.path.join(run_dir, "reports", "bisect.json"), encoding="utf-8") as f:
            return json.load(f)

    def test_passing_case_has_nothing_to_bisect(self):
        run_dir = self.pass_with(Run())
        data = self.bisect(game=lambda *a: self.fail("a passing case was run"))
        self.assertEqual(data["result"], "no difference to bisect")
        self.assertEqual(self.patched, [])
        self.assertEqual(read(os.path.join(self.osx, EXE)), FULL_EXE)
        self.assertEqual(self.bisect_json(run_dir)["result"], "no difference to bisect")

    def test_finds_the_culprit_and_restores_the_exe(self):
        run_dir = self.pass_with(Run(dumps=states(5, s2={"x": 7})))
        culprit = candidates(self.rows)[21]
        game = FakeGame([culprit], Run(dumps=states(5, s2={"x": 7})))
        data = self.bisect(game)
        self.assertEqual(data["result"], "culprits")
        self.assertEqual(data["culprits"], [{"offset": "0x%08X" % culprit.offset, "name": culprit.name}])
        self.assertTrue(data["clean"])
        self.assertTrue(data["restored"])
        self.assertEqual(read(os.path.join(self.osx, EXE)), FULL_EXE)
        self.assertEqual(self.bisect_json(run_dir), data)
        # first the two ends: the full table reproduces, the approved rows alone don't
        self.assertEqual(game.tables[0], self.rows)
        self.assertEqual(game.tables[1], [row for row in self.rows if row.approved])
        self.assertEqual([r["purpose"] for r in data["runs"][:3]], ["full table", "approved rows only", "search"])
        self.assertEqual(len(data["runs"]), 2 + data["search_runs"])
        approved = [row for row in self.rows if row.approved]
        for table in game.tables:
            for row in approved:
                self.assertIn(row, table)
        self.assertEqual(game.tables, self.patched)
        self.assertEqual(data["failure"], {"status": "fail", "at_step": 2})

    def test_crash_counts_only_as_the_same_crash(self):
        run_dir = self.pass_with(Run("crash", 2, 0xC0000005, GOOD[:2], "the game ended"))
        c = candidates(self.rows)
        game = FakeGame([c[30]], Run("crash", 2, 0xC0000005, GOOD[:2], "the game ended"),
                        noise=[c[5]], other=Run("hang", 1, None, GOOD[:1], "no dump"))
        data = self.bisect(game, run_dir=run_dir)
        self.assertEqual([d["name"] for d in data["culprits"]], [c[30].name])
        hangs = [r for r in data["runs"] if r["outcome"] == "hang"]
        self.assertTrue(hangs)
        for r in hangs:
            self.assertFalse(r["reproduces"])
            self.assertIn("counted as not reproducing", r["note"])
        self.assertEqual(read(os.path.join(self.osx, EXE)), FULL_EXE)

    def test_failure_gone_with_the_full_table(self):
        self.pass_with(Run(dumps=states(5, s2={"x": 7})))
        data = self.bisect(FakeGame([candidates(self.rows)[3]], Run()))
        self.assertEqual(data["result"], "does not reproduce")
        self.assertEqual(len(data["runs"]), 1)
        self.assertEqual(read(os.path.join(self.osx, EXE)), FULL_EXE)

    def test_failure_with_the_approved_rows_alone(self):
        self.pass_with(Run(dumps=states(5, s2={"x": 7})))
        data = self.bisect(FakeGame([], Run(dumps=states(5, s2={"x": 7}))))
        self.assertEqual(data["result"], "reproduces without candidates")
        self.assertEqual(len(data["runs"]), 2)
        self.assertEqual(data["culprits"], [])
        self.assertEqual(read(os.path.join(self.osx, EXE)), FULL_EXE)

    def test_setup_failed_run_stops_and_restores(self):
        run_dir = self.pass_with(Run(dumps=states(5, s2={"x": 7})))
        game = FakeGame([candidates(self.rows)[8]], Run(dumps=states(5, s2={"x": 7})), setup_failed_at=4)
        with self.assertRaises(BisectError):
            self.bisect(game)
        self.assertEqual(read(os.path.join(self.osx, EXE)), FULL_EXE)
        data = self.bisect_json(run_dir)
        self.assertEqual(data["result"], "error")
        self.assertIn("already running", data["message"])
        self.assertEqual(len(data["runs"]), 4)
        self.assertTrue(data["restored"])

    def test_stale_reference_refused(self):
        self.pass_with(Run(dumps=states(5, s2={"x": 7})))
        write(self.driver[0], b"another case driver")
        with self.assertRaises(BisectError) as caught:
            self.bisect(FakeGame([candidates(self.rows)[8]], Run(dumps=states(5, s2={"x": 7}))))
        self.assertIn("driver", str(caught.exception))
        self.assertEqual(self.patched, [])

    def test_never_swaps_in_the_reference_or_live_folder(self):
        run_dir = self.pass_with(Run(dumps=states(5, s2={"x": 7})))
        game = FakeGame([candidates(self.rows)[8]], Run(dumps=states(5, s2={"x": 7})))
        with mock.patch.object(bisect_module, "LIVE_DIR", self.osx):
            with self.assertRaises(BisectError) as caught:
                self.bisect(game)
        self.assertIn("live game folder", str(caught.exception))
        result_json = os.path.join(run_dir, "result.json")
        with open(result_json, encoding="utf-8") as f:
            data = json.load(f)
        data["osx_dir"] = os.path.join(self.ref, "")
        write(result_json, json.dumps(data).encode("utf-8"))
        with self.assertRaises(BisectError) as caught:
            self.bisect(game)
        self.assertIn("reference folder", str(caught.exception))
        self.assertEqual((self.patched, game.tables), ([], []))
        self.assertEqual(read(os.path.join(self.ref, EXE)), b"reference exe")
        self.assertEqual(read(os.path.join(self.osx, EXE)), FULL_EXE)

    def test_full_table_check(self):
        self.pass_with(Run(dumps=states(5, s2={"x": 7})))
        game = FakeGame([candidates(self.rows)[8]], Run(dumps=states(5, s2={"x": 7})))
        data = self.bisect(game)  # the folder's exe (FULL_EXE) is not the fake patch of the table
        self.assertIs(data["exe_is_full_table"], False)
        self.assertTrue(format_bisect(data)[0].startswith("WARNING: the OpenSMACX folder's exe"))
        shutil.copyfile(self.table, os.path.join(self.osx, EXE))  # now it is
        data = self.bisect(game)
        self.assertIs(data["exe_is_full_table"], True)
        self.assertFalse([line for line in format_bisect(data) if "WARNING" in line])

    def test_recovery_from_another_out_root(self):
        run_dir = self.pass_with(Run(dumps=states(5, s2={"x": 7})))
        killed = ExeSwap(os.path.join(self.osx, EXE)).__enter__()
        subset = os.path.join(self.tmp, "subset.exe")
        write(subset, b"a subset's exe")
        killed.install(subset)
        with open(killed.marker, encoding="utf-8") as f:
            note = json.load(f)
        note["start"] = 1  # its bisect has ended
        write(killed.marker, json.dumps(note).encode("utf-8"))
        other_root = os.path.join(self.tmp, "other-out")
        # a pass from anywhere refuses the folder
        with self.assertRaises(PassRefused) as caught:
            run_pass(["reports"], self.ref, self.osx, other_root, run_case=FakeRunCase(), cases_dir=self.cases,
                     ignore=set(), driver_files=self.driver)
        self.assertIn("--recover", str(caught.exception))
        # a bisect from another out root puts the exe back first, then bisects
        culprit = candidates(self.rows)[21]
        game = FakeGame([culprit], Run(dumps=states(5, s2={"x": 7})))
        data = bisect_case("reports", other_root, run_dir=run_dir, table=self.table, patch=self.fake_patch,
                           run_case=game, ignore=set(), driver_files=self.driver)
        self.assertIn("restored", data["recovered"])
        self.assertEqual(data["exe_sha256"], hashlib.sha256(FULL_EXE).hexdigest())
        self.assertEqual([c["name"] for c in data["culprits"]], [culprit.name])
        self.assertEqual(read(os.path.join(self.osx, EXE)), FULL_EXE)
        self.assertEqual(sorted(os.listdir(self.osx)), sorted(["OpenSMACX.dll", EXE, "saves"]))

    def test_recover_option(self):
        killed = ExeSwap(os.path.join(self.osx, EXE)).__enter__()
        subset = os.path.join(self.tmp, "subset.exe")
        write(subset, b"a subset's exe")
        killed.install(subset)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(cli.main(["bisect", "--recover", "--osx-dir", self.osx]), 1)  # (this process runs)
        self.assertIn("still runs", out.getvalue())
        with mock.patch.object(bisect_module, "_running", return_value=False), contextlib.redirect_stdout(out):
            self.assertEqual(cli.main(["bisect", "--recover", "--osx-dir", self.osx]), 0)
            self.assertEqual(cli.main(["bisect", "--recover", "--osx-dir", self.osx]), 0)
        self.assertIn("restored", out.getvalue())
        self.assertIn("nothing to recover", out.getvalue())
        self.assertEqual(read(os.path.join(self.osx, EXE)), FULL_EXE)

    def test_exe_table_warning(self):
        def patch_to(data):
            def patch(table_path, out_dir):
                write(os.path.join(out_dir, EXE), data)
                return os.path.join(out_dir, EXE)
            return patch
        self.assertEqual(exe_table_warning(self.osx, self.table, patch=patch_to(FULL_EXE)), "")
        warning = exe_table_warning(self.osx, self.table, patch=patch_to(b"another table's exe"))
        self.assertTrue(warning.startswith("WARNING: "))
        self.assertIn("is not", warning)

        def broken(table_path, out_dir):
            raise BisectError("opensmacx_patch.py failed (exit code 1): 2 redirect(s) are not exported")
        self.assertIn("can't check", exe_table_warning(self.osx, self.table, patch=broken))

    def test_latest_pass_with_the_case(self):
        older = self.pass_with(Run(dumps=states(5, s2={"x": 7})), now=datetime(2026, 10, 18, 5, 7))
        newer = self.pass_with(Run(), now=datetime(2026, 10, 18, 5, 7))  # 2026-10-18-0507-2
        os.makedirs(os.path.join(self.out, "r20-timeout-check"))
        self.assertEqual(find_pass(self.out, "reports"), newer)
        self.assertNotEqual(older, newer)
        with self.assertRaises(BisectError):
            find_pass(self.out, "idle")

    def test_command_line_passing_case(self):
        run_dir = self.pass_with(Run())
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = cli.main(["bisect", "reports", "--run", os.path.basename(run_dir), "--out-root", self.out,
                             "--table", self.table])
        self.assertEqual(code, 0)
        self.assertIn("no difference to bisect", out.getvalue())
        self.assertEqual(read(os.path.join(self.osx, EXE)), FULL_EXE)

    def test_command_line_error_exit_code(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = cli.main(["bisect", "reports", "--out-root", self.out, "--table", self.table])
        self.assertEqual(code, 1)
        self.assertIn("reports", out.getvalue())


if __name__ == "__main__":
    unittest.main()
