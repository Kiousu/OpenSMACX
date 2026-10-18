"""Click pass command line.

From tools/:  python -m clickpass.clickpass run [--cases idle,reports,...] [--ref-dir DIR] [--osx-dir DIR]
(or python tools/clickpass/clickpass.py run ... from anywhere).

run   plays the cases (default: every tools/clickpass/cases/*.case) in the reference folder and the
      OpenSMACX folder and compares their dumps and the files the games wrote, step by step (see
      runner.py). The reference side comes from build/clickpass/reference/ when its exe, DLL, save and
      case file are unchanged, otherwise it is recorded with the self-check (two runs). Results go to
      build/clickpass/<YYYY-MM-DD-HHMM>/: a folder per case and result.json. Prints one line per case;
      exit code 1 if any case is fail, crash, hang, blocked-write, setup-failed or reference-broken
      (nondeterministic cases are listed but don't fail the run). It refuses to start, before anything
      is made, when --out-root, --ref-dir or --osx-dir is, is in or holds the live game folder, when the
      reference folder's exe is not the pristine exe patched with only the table's approved rows
      (`reference` builds it), and while a bisect's note (clickpass_bisect_swap.json) is in either
      folder; it warns when the OpenSMACX folder's exe is not the redirect table's patch (--table,
      --pristine). Ends by writing the pass's report.md (`report`).

reference [--ref-dir DIR] [--dll build\\regress\\OpenSMACX.dll]
      makes DIR the reference folder: the pristine exe patched with only the approved rows of --table
      and its own harness DLL (ruling R19: replaced only when the harness plumbing or the dump format
      changes, never with a batch), both checked by sha256. Refuses the live folder, a folder a game
      runs from and one with a bisect's note.

bisect <case> [--run <pass folder>]
      names the redirect row(s) behind a case that failed in a pass (default: the newest pass that ran
      it) by playing it in the OpenSMACX folder with exes patched from parts of tools/redirects.tsv
      (see bisect.py); the approved rows are in every one. Prints the culprit rows and writes
      bisect.json into the pass's case folder. The runs are played in the pass's OpenSMACX folder,
      whose exe is put back afterwards; if a bisect was killed before it could, the next bisect there
      (from any out root) puts it back first.
      A passing case prints "no difference to bisect". Exit code 1 when it couldn't name a culprit.
      A finished bisection rewrites the pass's report.md with what it found.
bisect --recover [--osx-dir DIR]
      only that: puts back the exe a killed bisect left in DIR (default the OpenSMACX folder) and
      removes its note, so `run` plays there again.

report [--run <pass folder>]
      writes report.md into the pass folder (default: the newest pass) from its result.json, its cases'
      bisect.json and tools/clickpass/inventory.tsv, leftover_reads.tsv and decisions.tsv (see report.py).
"""
import argparse
import os
import sys

if __package__ in (None, ""):  # run as a script: import the package from tools/, not this file; and
    # not from this file's folder either, where bisect.py would stand in for the standard library's
    # bisect (which tempfile imports through random)
    sys.path[0] = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

from clickpass.bisect import (DONE, PRISTINE_EXE, TABLE, BisectError, bisect_case, exe_table_warning,  # noqa: E402
                              format_bisect, install_reference, recover_swap, reference_exe_refusal)
from clickpass.report import newest_pass, report_pass  # noqa: E402
from clickpass.runner import (CASES_DIR, FAILING, HERE, PassRefused, format_outcome, live_refusal,  # noqa: E402
                              run_pass)

REF_DIR = r"G:\SMACX-opensmacx-test-clickref"
OSX_DIR = r"G:\SMACX-opensmacx-test-clickosx"
OUT_ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "build", "clickpass"))
HARNESS_DLL = os.path.normpath(os.path.join(HERE, "..", "..", "build", "regress", "OpenSMACX.dll"))


def _say(text: str) -> None:
    print(text, flush=True)


def _all_cases():
    return sorted(os.path.splitext(n)[0] for n in os.listdir(CASES_DIR) if n.lower().endswith(".case"))


def _write_report(run_dir: str, table: str) -> str:
    """Writes the pass's report.md; says where, or why it couldn't."""
    try:
        return "report: %s" % report_pass(run_dir, table=table)
    except (OSError, ValueError, KeyError, TypeError) as e:
        return "can't write the report for %s: %s" % (run_dir, e)


def _live(*folders) -> bool:
    """Says why and returns True when one of the (folder, what) is, is in or holds the live folder."""
    refusal = live_refusal([f for f in folders if f[0]])
    if refusal:
        _say("can't run: " + refusal)
    return bool(refusal)


def cmd_run(args) -> int:
    if _live((args.out_root, "--out-root"), (args.ref_dir, "--ref-dir"), (args.osx_dir, "--osx-dir")):
        return 1  # (before anything is made or patched)
    cases = [c.strip() for c in args.cases.split(",") if c.strip()] if args.cases else _all_cases()
    cases = list(dict.fromkeys(cases))  # each once, in the order given
    refusal = reference_exe_refusal(args.ref_dir, args.table, args.pristine)
    if refusal:
        _say("can't run: " + refusal)
        return 1
    warning = exe_table_warning(args.osx_dir, args.table, args.pristine)
    if warning:
        _say(warning)
    try:
        result = run_pass(cases, args.ref_dir, args.osx_dir, args.out_root, log=_say if args.verbose else None,
                          warnings=[warning] if warning else [])
    except PassRefused as e:
        _say("can't run: %s" % e)
        return 1
    _say("")
    width = max([len(o.case) for o in result.outcomes] + [4])
    for o in result.outcomes:
        _say("%-*s  %s" % (width, o.case, format_outcome(o)))
        for d in o.differences[:10]:
            _say("%-*s    %s" % (width, "", d))
    counts = {}
    for o in result.outcomes:
        counts[o.status] = counts.get(o.status, 0) + 1
    _say("%d case(s): %s" % (len(result.outcomes), ", ".join("%d %s" % (n, s) for s, n in counts.items())))
    if warning:
        _say(warning)
    _say("results: %s" % os.path.join(result.run_dir, "result.json"))
    _say(_write_report(result.run_dir, args.table))
    return 1 if any(o.status in FAILING for o in result.outcomes) else 0


def cmd_reference(args) -> int:
    try:
        _say(install_reference(args.ref_dir, args.dll, args.table, args.pristine))
    except (BisectError, OSError, ValueError) as e:
        _say("can't make %s the reference folder: %s" % (args.ref_dir, e))
        return 1
    refusal = reference_exe_refusal(args.ref_dir, args.table, args.pristine)
    _say(refusal or "checked: %s is the reference `run` expects" % args.ref_dir)
    return 1 if refusal else 0


def cmd_bisect(args) -> int:
    if _live((args.out_root, "--out-root"), (args.run, "--run"), (args.osx_dir, "--osx-dir")):
        return 1
    if args.recover or args.osx_dir:
        if args.case or not args.recover:
            _say("bisect: --recover takes no case and --osx-dir goes with --recover (a bisect uses its pass's folder)")
            return 2
        osx_dir = args.osx_dir or OSX_DIR
        try:
            text = recover_swap(osx_dir)
        except BisectError as e:
            _say("can't recover %s: %s" % (osx_dir, e))
            return 1
        _say(text or "%s has no bisect note: nothing to recover" % osx_dir)
        return 0
    if not args.case:
        _say("bisect: name a case (or --recover)")
        return 2
    try:
        data = bisect_case(args.case, args.out_root, run_dir=args.run, table=args.table, pristine=args.pristine,
                           log=_say if args.verbose else None)
    except BisectError as e:
        _say("%s: can't bisect: %s" % (args.case, e))
        return 1
    except KeyboardInterrupt:
        _say("%s: bisect interrupted" % args.case)
        return 130
    _say("")
    for line in format_bisect(data):
        _say(line)
    _say(_write_report(data["pass"], args.table))
    return 0 if data["result"] in DONE else 1


def cmd_report(args) -> int:
    if _live((args.out_root, "--out-root"), (args.run, "--run")):
        return 1
    try:
        run_dir = args.run or newest_pass(args.out_root)
        if not os.path.isdir(run_dir) and os.path.isdir(os.path.join(args.out_root, run_dir)):
            run_dir = os.path.join(args.out_root, run_dir)
        path = report_pass(os.path.abspath(run_dir), table=args.table)
    except (OSError, ValueError, KeyError, TypeError) as e:
        _say("can't write the report: %s" % e)
        return 1
    _say("report: %s" % path)
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog="clickpass", description="Click pass: the game's UI, compared with "
                                     "the unmodified game.")
    sub = parser.add_subparsers(dest="command", required=True)
    run = sub.add_parser("run", help="play cases on both sides and compare them")
    run.add_argument("--cases", help="comma-separated case names (default: every cases/*.case)")
    run.add_argument("--ref-dir", default=REF_DIR, help="the reference game folder (default %(default)s)")
    run.add_argument("--osx-dir", default=OSX_DIR, help="the OpenSMACX game folder (default %(default)s)")
    run.add_argument("--out-root", default=OUT_ROOT, help="results and reference cache (default %(default)s)")
    run.add_argument("--table", default=TABLE, help="the redirect table the OpenSMACX exe should be patched "
                     "from (default %(default)s)")
    run.add_argument("--pristine", default=PRISTINE_EXE, help="the unpatched exe, read only (default "
                     "%(default)s)")
    run.add_argument("-q", "--quiet", dest="verbose", action="store_false", help="no progress lines")
    bis = sub.add_parser("bisect", help="name the redirect rows behind a failing case")
    bis.add_argument("case", nargs="?", help="the case's name, as in the pass's result.json")
    bis.add_argument("--recover", action="store_true", help="only put back the exe a killed bisect left swapped")
    bis.add_argument("--osx-dir", help="with --recover: the OpenSMACX folder (default %s)" % OSX_DIR)
    bis.add_argument("--run", help="the pass folder, or its name in --out-root (default: the newest pass with "
                     "the case)")
    bis.add_argument("--out-root", default=OUT_ROOT, help="passes and reference cache (default %(default)s)")
    bis.add_argument("--table", default=TABLE, help="the redirect table (default %(default)s)")
    bis.add_argument("--pristine", default=PRISTINE_EXE, help="the unpatched exe, read only (default "
                     "%(default)s)")
    bis.add_argument("-q", "--quiet", dest="verbose", action="store_false", help="no progress lines")
    rep = sub.add_parser("report", help="write a pass's report.md")
    rep.add_argument("--run", help="the pass folder, or its name in --out-root (default: the newest pass)")
    rep.add_argument("--out-root", default=OUT_ROOT, help="passes and reference cache (default %(default)s)")
    rep.add_argument("--table", default=TABLE, help="the redirect table, for held-back rows and the approved "
                     "deviations (default %(default)s)")
    ref = sub.add_parser("reference", help="make the reference folder: the approved rows' exe and its own DLL")
    ref.add_argument("--ref-dir", default=REF_DIR, help="the reference game folder (default %(default)s)")
    ref.add_argument("--dll", default=HARNESS_DLL, help="the harness DLL it gets (default %(default)s)")
    ref.add_argument("--table", default=TABLE, help="the redirect table whose approved rows it carries (default "
                     "%(default)s)")
    ref.add_argument("--pristine", default=PRISTINE_EXE, help="the unpatched exe, read only (default %(default)s)")
    args = parser.parse_args(argv)
    return {"run": cmd_run, "bisect": cmd_bisect, "report": cmd_report, "reference": cmd_reference}[args.command](args)


if __name__ == "__main__":
    sys.exit(main())
