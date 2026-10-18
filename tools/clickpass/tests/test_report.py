import contextlib
import io
import json
import os
import shutil
import tempfile
import unittest

from clickpass import clickpass as cli
from clickpass.compare import Difference
from clickpass.report import (DECISIONS_TSV, INVENTORY_TSV, LEFTOVERS_TSV, Decision, InventoryRow, Leftover,
                              read_decisions, read_inventory, read_leftovers, report_pass, write_report)
from clickpass.runner import CaseOutcome, PassResult, run_pass
from clickpass.tables import Row
from clickpass.tests.test_runner import FakeRunCase, Run, states, write

SECTIONS = ("Summary", "Failures", "Needs Kiousu's decision", "Nondeterministic cases", "Coverage")
FAUNA = Row(0x122180, "?alien_fauna@@YAXXZ", False)
WTA = Row(0x15BC80, "?wants_to_attack@@YAHIIH@Z", False)
OTHER = Row(0x12345, "?base_hurry@@YAXXZ", False)
FAUNA_LEFTOVER = Leftover("alien_fauna", 0x522180, "two stack slots (draw x, y)", "stale draw coordinates")


def culprits(*rows, clean=True):
    return {"result": "culprits", "message": "", "clean": clean, "search_runs": 10, "runs": [{}] * 12,
            "culprits": [{"offset": "0x%08X" % r.offset, "name": r.name} for r in rows]}


def failed(case, step=2):
    return CaseOutcome(case, "fail", step, [Difference("Vehs", 412, "dmg_incurred", 3, 2)],
                       "differs from the reference from step %d on (1 differences shown)" % step, "hit", "k", 9.5)


def passed(case):
    return CaseOutcome(case, "pass", None, [], "3 steps, no differences", "hit", "k", 4.0)


def sections(text):
    """{heading: body} for the report's `## ` sections, and the headings in order."""
    found, order, current = {}, [], None
    for line in text.splitlines():
        if line.startswith("## "):
            current = line[3:].strip()
            order.append(current)
            found[current] = []
        elif current is not None:
            found[current].append(line)
    return {k: "\n".join(v) for k, v in found.items()}, order


class WriteReportTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.path = os.path.join(self.tmp, "report.md")

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def report(self, outcomes, bisects=None, inventory=(), leftovers=(), decisions=(), table=None):
        write_report(PassResult(self.tmp, list(outcomes)), bisects or {}, list(inventory), list(leftovers),
                     list(decisions), self.path, table=table)
        with open(self.path, encoding="utf-8") as f:
            return sections(f.read())[0]

    def test_section_order(self):
        write_report(PassResult(self.tmp, [passed("idle")]), {}, [], [], [], self.path)
        with open(self.path, encoding="utf-8") as f:
            self.assertEqual(sections(f.read())[1], list(SECTIONS))
        with open(self.path, "rb") as f:
            data = f.read()
        self.assertIn(b"\r\n", data)
        self.assertNotIn(b"\n", data.replace(b"\r\n", b""))  # CRLF only

    def test_summary_counts_per_status(self):
        s = self.report([passed("idle"), failed("reports"), passed("save_game"),
                         CaseOutcome("end_turn", "nondeterministic", 1, [], "", "hit", "k", 0.0)])
        self.assertIn("4 cases: 2 pass, 1 fail, 1 nondeterministic", s["Summary"])

    def test_leftover_culprit_goes_to_decision_section(self):
        s = self.report([passed("idle"), failed("end_turn")], {"end_turn": culprits(FAUNA)},
                        leftovers=[FAUNA_LEFTOVER])
        self.assertIn("end_turn", s["Needs Kiousu's decision"])
        self.assertIn("alien_fauna", s["Needs Kiousu's decision"])
        self.assertIn("stale draw coordinates", s["Needs Kiousu's decision"])  # why
        self.assertIn("Vehs[412].dmg_incurred: 3 vs 2", s["Needs Kiousu's decision"])  # what each game had
        self.assertNotIn("end_turn", s["Failures"])

    def test_leftover_matched_by_address(self):
        renamed = Leftover("Fauna::alien", 0x522180, "", "")
        s = self.report([failed("end_turn")], {"end_turn": culprits(FAUNA)}, leftovers=[renamed])
        self.assertIn("end_turn", s["Needs Kiousu's decision"])

    def test_decided_case_not_raised_again(self):
        decided = Decision("*", "alien_fauna", "accept the difference", "2026-10-18", "Kiousu: only a redraw")
        s = self.report([failed("end_turn")], {"end_turn": culprits(FAUNA)}, leftovers=[FAUNA_LEFTOVER],
                        decisions=[decided])
        self.assertNotIn("end_turn", s["Needs Kiousu's decision"])
        self.assertIn("end_turn", s["Failures"])  # still a failure, never silent
        self.assertIn("accept the difference", s["Failures"])  # what's being done: the decision
        # a decision for another case settles nothing here
        other = decided._replace(case="reports")
        s = self.report([failed("end_turn")], {"end_turn": culprits(FAUNA)}, leftovers=[FAUNA_LEFTOVER],
                        decisions=[other])
        self.assertIn("end_turn", s["Needs Kiousu's decision"])

    def test_unclean_bisect_goes_to_decision_section(self):
        s = self.report([failed("end_turn")], {"end_turn": culprits(OTHER, WTA, clean=False)})
        self.assertIn("end_turn", s["Needs Kiousu's decision"])
        self.assertIn("base_hurry", s["Needs Kiousu's decision"])
        self.assertIn("not clean", s["Needs Kiousu's decision"])
        self.assertNotIn("end_turn", s["Failures"])
        # settled only when every row of the set is decided
        one = Decision("*", "wants_to_attack", "start the region counts at zero", "2026-10-18", "Kiousu")
        s = self.report([failed("end_turn")], {"end_turn": culprits(OTHER, WTA, clean=False)}, decisions=[one])
        self.assertIn("end_turn", s["Needs Kiousu's decision"])

    def test_clean_culprit_in_failures(self):
        s = self.report([failed("reports")], {"reports": culprits(OTHER)}, table=[FAUNA, OTHER])
        self.assertIn("reports", s["Failures"])
        self.assertIn("fail at step 2", s["Failures"])
        self.assertIn("Vehs[412].dmg_incurred: 3 vs 2", s["Failures"])
        self.assertIn("0x00012345", s["Failures"])  # the culprit's row
        self.assertIn("base_hurry", s["Failures"])
        self.assertIn("open", s["Failures"])
        self.assertIn("None.", s["Needs Kiousu's decision"])

    def test_culprit_out_of_the_table_is_held_back(self):
        s = self.report([failed("reports")], {"reports": culprits(OTHER)}, table=[FAUNA])
        self.assertIn("held back", s["Failures"])

    def test_decision_on_a_clean_culprit_is_what_is_being_done(self):
        one = Decision("*", "wants_to_attack", "start the region counts at zero", "2026-10-18", "Kiousu")
        s = self.report([failed("end_turn")], {"end_turn": culprits(WTA)}, decisions=[one])
        self.assertIn("start the region counts at zero", s["Failures"])
        self.assertNotIn("open", s["Failures"])

    def test_failure_not_bisected_yet(self):
        s = self.report([failed("reports"), CaseOutcome("idle", "crash", 0, [], "crash at step 0", "hit", "k", 1.0)])
        self.assertIn("clickpass.py bisect reports", s["Failures"])
        self.assertIn("crash at step 0", s["Failures"])

    def test_nondeterministic_cases(self):
        nd = CaseOutcome("end_turn", "nondeterministic", 1, [Difference("Globals", None, "turn", 290, 291)],
                         "the reference's two runs differ from step 1 on", "hit", "k", 0.0)
        s = self.report([nd])
        self.assertIn("end_turn", s["Nondeterministic cases"])
        self.assertIn("Globals.turn: 290 vs 291", s["Nondeterministic cases"])
        self.assertNotIn("end_turn", s["Failures"])

    def test_coverage_counts(self):
        rows = [InventoryRow("Main map", "F%d" % n, "reports", "covered", "") for n in (1, 2, 3)]
        rows += [InventoryRow("Base screen", "Hurry", "", "not-reachable", "no base"),
                 InventoryRow("Council", "Vote", "", "not-reachable", "no council")]
        s = self.report([passed("reports")], inventory=rows)
        self.assertIn("3 covered, 2 not reachable", s["Coverage"])
        self.assertIn("no council", s["Coverage"])


class TsvFilesTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def write(self, name, text):
        path = os.path.join(self.tmp, name)
        with open(path, "w", encoding="utf-8", newline="\r\n") as f:
            f.write(text)
        return path

    def test_shipped_decisions(self):
        decisions = {d.function: d for d in read_decisions(DECISIONS_TSV)}
        self.assertEqual(sorted(decisions), ["action_airdrop", "action_fungal", "action_give", "action_home",
                                             "action_tectonic", "action_terraform", "drone_riot", "hex_cost",
                                             "wants_to_attack"])
        self.assertEqual(decisions["wants_to_attack"],
                         Decision("*", "wants_to_attack", "start the region counts at zero (the intended design)",
                                  "2026-10-18", "Kiousu: replicate the original's intent, not the shipped "
                                  "uninitialized buffer"))
        self.assertEqual(decisions["drone_riot"].case, "*")
        self.assertIn("REVOLT slots start at -1", decisions["drone_riot"].decision)

    def test_shipped_leftovers(self):
        self.assertEqual([(r.function, r.offset) for r in read_leftovers(LEFTOVERS_TSV)], [("alien_fauna", 0x522180)])

    def test_shipped_inventory(self):
        rows = read_inventory(INVENTORY_TSV)
        controls = [r.control for r in rows]
        for n in range(1, 13):
            self.assertTrue(any(c.startswith("F%d " % n) for c in controls), n)
        self.assertTrue(any("Ctrl+S" in c for c in controls))
        self.assertTrue(any("TURN COMPLETE" in c for c in controls))
        self.assertEqual({r.status for r in rows}, {"covered"})

    def test_bad_inventory_status_rejected(self):
        path = self.write("inv.tsv", "screen\tcontrol\tcase\tstatus\tnote\nMap\tF1\treports\tdone\t\n")
        with self.assertRaises(ValueError):
            read_inventory(path)

    def test_comments_header_and_short_rows(self):
        path = self.write("dec.tsv", "# a comment\ncase\tfunction\tdecision\tdate\twhy\n\n"
                          "end_turn\tfoo\taccept\t2026-10-18\n")
        self.assertEqual(read_decisions(path), [Decision("end_turn", "foo", "accept", "2026-10-18", "")])


class CommandTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.run_dir = os.path.join(self.tmp, "2026-10-18-0600")
        os.makedirs(os.path.join(self.run_dir, "reports"))
        entries = [{"case": "idle", "status": "pass", "at_step": None, "detail": "1 step, no differences",
                    "differences": [], "cache": "hit", "key": "k", "seconds": 5.0},
                   {"case": "reports", "status": "fail", "at_step": 4, "detail": "differs",
                    "differences": ["Globals.turn: 289 vs 290"], "cache": "hit", "key": "k", "seconds": 30.0}]
        with open(os.path.join(self.run_dir, "result.json"), "w", encoding="utf-8") as f:
            json.dump({"started": "2026-10-18T06:00:00", "ref_dir": "R", "osx_dir": "O",
                       "warnings": ["WARNING: O\\terranx_opensmacx.exe is not the table's patch"],
                       "summary": {"pass": 1, "fail": 1}, "cases": entries}, f)
        with open(os.path.join(self.run_dir, "reports", "bisect.json"), "w", encoding="utf-8") as f:
            json.dump(dict(culprits(OTHER), case="reports"), f)
        self.table = os.path.join(self.tmp, "redirects.tsv")
        with open(self.table, "w", encoding="utf-8") as f:
            f.write("0x00012345\t?base_hurry@@YAXXZ\n0x001A6AC0\t?game_data@@YAHPAU_iobuf@@H@Z\tapproved\n")

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def test_report_pass_reads_the_pass_folder(self):
        path = report_pass(self.run_dir, table=self.table)
        self.assertEqual(path, os.path.join(self.run_dir, "report.md"))
        with open(path, encoding="utf-8") as f:
            text = f.read()
        s = sections(text)[0]
        self.assertIn("2 cases: 1 pass, 1 fail", s["Summary"])
        self.assertIn("is not the table's patch", text)  # the run's warning
        self.assertIn("game_data", text)  # the approved deviations are part of the reference
        self.assertIn("Globals.turn: 289 vs 290", s["Failures"])
        self.assertIn("base_hurry", s["Failures"])

    def test_report_command(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = cli.main(["report", "--run", os.path.basename(self.run_dir), "--out-root", self.tmp,
                             "--table", self.table])
        self.assertEqual(code, 0)
        self.assertIn(os.path.join(self.run_dir, "report.md"), out.getvalue())
        self.assertTrue(os.path.isfile(os.path.join(self.run_dir, "report.md")))

    def test_report_command_newest_pass(self):
        os.makedirs(os.path.join(self.tmp, "reference"))
        with contextlib.redirect_stdout(io.StringIO()):
            code = cli.main(["report", "--out-root", self.tmp, "--table", self.table])
        self.assertEqual(code, 0)
        self.assertTrue(os.path.isfile(os.path.join(self.run_dir, "report.md")))


class PassToReportTest(unittest.TestCase):
    """run_pass's result.json, read back by report_pass."""

    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def test_a_failing_pass_and_its_report(self):
        ref, osx, cases = (os.path.join(self.tmp, name) for name in ("ref", "osx", "cases"))
        for path, data in ((os.path.join(ref, "terranx_opensmacx.exe"), b"exe"),
                           (os.path.join(ref, "OpenSMACX.dll"), b"dll"),
                           (os.path.join(ref, "saves", "Council.SAV"), b"save"),
                           (os.path.join(osx, "saves", "Council.SAV"), b"save"),
                           (os.path.join(cases, "reports.case"), b"save Council.SAV\r\nkeys {F1}\r\n"),
                           (os.path.join(self.tmp, "case.ps1"), b"driver")):
            write(path, data)
        fake = FakeRunCase()
        fake.plan(ref, Run())
        fake.plan(osx, Run(dumps=states(5, s2={"x": 7})))
        result = run_pass(["reports"], ref, osx, os.path.join(self.tmp, "out"), run_case=fake, cases_dir=cases,
                          ignore=set(), driver_files=[os.path.join(self.tmp, "case.ps1")], warnings=["WARNING: w"])
        with open(report_pass(result.run_dir, table=None, cases_dir=cases), encoding="utf-8") as f:
            text = f.read()
        s = sections(text)[0]
        self.assertIn("**WARNING: w**", text)
        self.assertIn("1 case: 1 fail", s["Summary"])
        self.assertIn("| reports | Council.SAV | fail | 2 |", s["Summary"])
        self.assertIn("### reports: fail at step 2", s["Failures"])
        self.assertIn("`Globals.x: 0 vs 7`", s["Failures"])
        self.assertIn("clickpass.py bisect reports", s["Failures"])


if __name__ == "__main__":
    unittest.main()
