"""[[BPR-1274]]. The large-project course's targets, judged on reports it wrote.

The course's own run proves only that this build meets them. This shows the
targets would have caught the defect: the same checks on the macOS Release
report [[BPR-1184]] measured before the fix fail, naming each phase; on the
report measured after it they pass; a note change made to cost what the old
build's did fails the scaling check in an optimized build and is only
reported in Debug; and where `--enforce` is off nothing timed fails a run.
Nothing is launched.
"""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BEFORE = ROOT / "docs/verification/BPR-1184-large-project/mac-release-report.json"
AFTER = ROOT / "docs/verification/BPR-1274-large-project/mac-release-report.json"


def load_course():
    sys.path.insert(0, str(HERE))
    spec = importlib.util.spec_from_file_location("review_cycle_large_project_contract", HERE / "review_cycle_large_project_contract.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def judge(course, report, enforce, build_type):
    """The failures and targets the course sets on `report`, as `--check-report` would."""
    args = argparse.Namespace(files=None, no_stress=False, limit_probe=False, enforce=enforce, build_type=build_type, no_warm_up=False, source_root=None)
    judged = course.Course(args, None, None)
    judged.report = dict(copy.deepcopy(report), targets=[], failures=[])
    for name, entry in judged.report["corpora"].items():
        judged.cold_open_ratio(name, entry)
    judged.targets()
    judged.stress_targets()
    return judged.report["failures"], judged.report["targets"]


def require(condition, message):
    if not condition:
        raise SystemExit(f"FAILED: {message}")


def main():
    course = load_course()
    before = json.loads(BEFORE.read_text(encoding="utf-8"))
    after = json.loads(AFTER.read_text(encoding="utf-8"))

    failures, _ = judge(course, before, 1, "Release")
    for phase in ("stress compose_open held the window", "stress desktop_suggest p95", "stress freeze_and_copy p95", "stress save_document p95"):
        require(any(failure.startswith(phase) for failure in failures), f"the BPR-1184 report fails on '{phase}': {failures}")
    require(not any("typing_during_capture" in failure or "navigation_during_capture" in failure for failure in failures), f"and only on the stress round: {failures}")
    print(f"BEFORE: the BPR-1184 macOS Release report misses {len(failures)} targets")

    failures, targets = judge(course, after, 1, "Release")
    require(not failures, f"the BPR-1274 report meets every target: {failures}")
    ratio = next(target for target in targets if target["phase"] == "note_change_ratio")
    require(ratio["enforced"] and ratio["met"], f"its note change scaling is checked and met: {ratio}")
    print(f"AFTER: the BPR-1274 macOS Release report meets {len(targets)} targets; note change ratio {ratio['ratio']}")

    # A note change that costs what the old build's did, in a round otherwise
    # as measured: the scaling check alone catches it in an optimized build.
    slowed = copy.deepcopy(after)
    phases = slowed["stress_round"]["phases"]
    phases["desktop_suggest"] = dict(phases["desktop_suggest"], p50=phases["desktop_suggest_first"]["p50"] * 66.0, p95=200.0)
    failures, _ = judge(course, slowed, 1, "Release")
    require(len(failures) == 1 and failures[0].startswith("a note change in the stress round took"), f"the scaling check fails a change that grows with the round: {failures}")
    failures, targets = judge(course, slowed, 1, "Debug")
    ratio = next(target for target in targets if target["phase"] == "note_change_ratio")
    require(not failures and not ratio["enforced"] and not ratio["met"], f"in Debug it is reported, not enforced: {failures} {ratio}")
    print("SCALING: a note change 66 times the first fails in Release, is reported in Debug")

    failures, targets = judge(course, before, 0, "Release")
    require(not any(failure.startswith("stress ") for failure in failures), f"with --enforce 0 the timed stress targets are reported only: {failures}")
    require(any(target["phase"] == "compose_open_busy" and not target["met"] and not target["enforced"] for target in targets), "and still reported as missed")
    print("REPORTED: with --enforce 0 the same misses are reported, not failed")
    print("REVIEW_CYCLE_LARGE_PROJECT_TARGETS_OK")


if __name__ == "__main__":
    main()
