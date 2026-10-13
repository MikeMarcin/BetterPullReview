#!/usr/bin/env python3
"""Aggregate the diff-engine evaluation's raw results ([[RFC-BPR-0082]]).

Reads the harness's JSON-lines results and the suite results of one station,
writes a compact raw-results file (every sample time, every outcome) and a
generated Markdown section: corpus, measurements per size class, GitHub
agreement, stops, overhead, determinism, gates and the mechanical decision of
each component under the RFC's fixed thresholds.

Statistics, fixed before the macOS results were read:
* a pair's time is the median of its samples (rounds x repetitions);
* a class's speed ratio is the median, over the class's pairs, of the
  candidate's pair time divided by the baseline's; "2x faster" is a ratio of
  0.5 or less, "regresses by more than 10%" a ratio above 1.10;
* memory ratios are formed the same way from the peak memory the comparison
  added to its process (its process and, for the process shape, Git's);
* stop-to-return is taken over the samples a stop actually interrupted.

Usage:
  make_report.py --results DIR --corpus DIR --station NAME --raw OUT.json --markdown OUT.md
"""

from __future__ import annotations

import argparse
import collections
import json
import math
import pathlib
import statistics
import sys

GITHUB = "github"
CLASSES = ("S", "M", "L", "pathological", "real")
COMPONENT_NAMES = {"0": "baseline", "1": "primary", "2": "slide", "3": "pairing", "4": "intraline"}
SHAPE_NAMES = {"0": "baseline", "1": "process (P)", "2": "linked (L)", "3": "core (C)"}

# What each candidate would need before it could ship, from the RFC's
# boundaries and the owner decisions supplied with the mandate.
OWNER_ANSWERS = {
    "process": "Q3 (results depend on the installed Git version)",
    "linked": "Q1 (statically linking LGPL-2.1 xdiff)",
    "dtl": "Q2 (dtl, BSD-3-Clause, is not an approved core library)",
    "rapidfuzz": "Q2 (rapidfuzz-cpp, MIT, is not an approved core library)",
    "dmp": "Q2 (diff-match-patch port, Apache-2.0, is not an approved core library)",
    "first-party": "owner approval of a hand-written diff algorithm in the core (AGENTS.md)",
}


def load(path: pathlib.Path) -> list[dict]:
    if not path.is_file():
        return []
    return [json.loads(line) for line in path.read_text().splitlines() if line.strip()]


def median(values: list[float]) -> float:
    return statistics.median(values) if values else math.nan


def percentile(values: list[float], fraction: float) -> float:
    if not values:
        return math.nan
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, math.ceil(fraction * len(ordered)) - 1))
    return ordered[index]


def fmt(value: float, digits: int = 2) -> str:
    if value is None or (isinstance(value, float) and math.isnan(value)):
        return "-"
    return f"{value:.{digits}f}"


def owner_answer(name: str) -> str | None:
    if name.startswith("p-"):
        return OWNER_ANSWERS["process"]
    if name.startswith("l-"):
        return OWNER_ANSWERS["linked"]
    if name in ("c-onp", "c-onp-intraline"):
        return OWNER_ANSWERS["dtl"]
    if name == "c-rapidfuzz":
        return OWNER_ANSWERS["rapidfuzz"]
    if name == "c-dmp":
        return OWNER_ANSWERS["dmp"]
    if name in ("c-histogram", "c-slide"):
        return OWNER_ANSWERS["first-party"]
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", type=pathlib.Path, required=True)
    parser.add_argument("--corpus", type=pathlib.Path, required=True)
    parser.add_argument("--station", required=True)
    parser.add_argument("--raw", type=pathlib.Path, required=True)
    parser.add_argument("--markdown", type=pathlib.Path, required=True)
    parser.add_argument("--adjudication", type=pathlib.Path)
    arguments = parser.parse_args()
    results = arguments.results
    manifest = json.loads((arguments.corpus / "manifest.json").read_text())
    pairs = {pair["id"]: pair for pair in manifest["pairs"]}
    environment = json.loads((results / "environment.json").read_text()) if (results / "environment.json").is_file() else {}
    adjudication = json.loads(arguments.adjudication.read_text()) if arguments.adjudication and arguments.adjudication.is_file() else {}

    measure = load(results / "measure.jsonl")
    loads = [row["load"] for row in measure if row.get("load", -1) >= 0]
    phases = load(results / "phases.jsonl")
    intraline = load(results / "intraline.jsonl")
    stops = load(results / "stops.jsonl")
    overhead = load(results / "overhead.jsonl")
    determinism = load(results / "determinism.jsonl")
    rss = load(results / "rss.jsonl")
    suites = load(results / "suites.jsonl")
    inventories = load(results / "inventories.jsonl")

    # ------------------------------------------------------------ per pair
    samples = collections.defaultdict(list)
    outcome = {}
    candidates = []
    meta = {}
    for row in measure:
        key = (row["candidate"], row["pair"])
        samples[key].append(row["time_ms"])
        if row["candidate"] not in meta:
            candidates.append(row["candidate"])
            meta[row["candidate"]] = (COMPONENT_NAMES.get(row["component"], row["component"]), SHAPE_NAMES.get(row["shape"], row["shape"]))
        previous = outcome.get(key)
        fields = {name: row.get(name) for name in ("reason", "published", "valid", "approximate", "edit_size", "equal_lines", "groups", "similar_pairs",
                                                    "exact_pairs", "lost_intraline", "intraline", "segments_digest", "pairs_digest", "intraline_digest",
                                                    "oracle_exact", "oracle_exact_github_join", "oracle_exact_git_join", "comment_flips", "oracle_hunks", "candidate_hunks", "process_ms", "core_ms")}
        if previous is None:
            outcome[key] = fields | {"stable": True}
        elif previous["segments_digest"] != fields["segments_digest"] or previous["pairs_digest"] != fields["pairs_digest"]:
            previous["stable"] = False
    pair_ids = sorted({pair for _, pair in samples})
    time_of = {key: median(values) for key, values in samples.items()}

    def class_pairs(category: str) -> list[str]:
        return [pair for pair in pair_ids if (pairs[pair]["class"] == category if category in ("pathological", "real") else
                                               (pairs[pair]["class"] in ("S", "M", "L") and pairs[pair]["size"] == category))]

    def size_pairs(size: str) -> list[str]:
        return [pair for pair in pair_ids if pairs[pair]["size"] == size]

    def ratio(candidate: str, chosen: list[str], table: dict) -> float:
        values = [table[(candidate, pair)] / table[("baseline", pair)] for pair in chosen
                  if (candidate, pair) in table and ("baseline", pair) in table and table[("baseline", pair)] > 0]
        return median(values)

    # Peak memory the comparison added.
    memory = {}
    for row in rss:
        added = max(0, row["self_peak_kb"] - row["self_before_kb"]) + row["children_peak_kb"]
        memory[(row["candidate"], row["pair"])] = float(max(added, 1))

    # Minimal edit, from the minimal matchers.
    minimal = {}
    for pair in pair_ids:
        values = [outcome[(name, pair)]["edit_size"] for name in candidates
                  if (name in ("c-onp", "l-minimal") or (name.startswith("p-") and "-minimal" in name and not name.endswith("indent")))
                  and (name, pair) in outcome and outcome[(name, pair)]["valid"] and outcome[(name, pair)]["edit_size"] is not None]
        if values:
            minimal[pair] = min(values)

    oracle_pairs = [pair for pair in pair_ids if (pairs[pair].get("oracle") or {}).get("kind") == GITHUB]

    summary = {}
    for name in candidates:
        rows = [outcome[(name, pair)] for pair in pair_ids if (name, pair) in outcome]
        base = {pair: outcome[("baseline", pair)] for pair in pair_ids if ("baseline", pair) in outcome}
        invalid = [pair for pair in pair_ids if (name, pair) in outcome and base.get(pair, {}).get("published") and not outcome[(name, pair)]["valid"]]
        refusals = [pair for pair in pair_ids if (name, pair) in outcome and base.get(pair, {}).get("reason") == "0" and outcome[(name, pair)]["reason"] != "0"]
        fixed = [pair for pair in class_pairs("L") + class_pairs("pathological")
                 if base.get(pair, {}).get("approximate") and (name, pair) in outcome and not outcome[(name, pair)]["approximate"]]
        unfixed = [pair for pair in class_pairs("L") + class_pairs("pathological")
                   if base.get(pair, {}).get("approximate") and (name, pair) in outcome and outcome[(name, pair)]["approximate"]]
        new_approx = [pair for pair in pair_ids if (name, pair) in outcome and outcome[(name, pair)]["approximate"] and not base.get(pair, {}).get("approximate")]
        exact = sum(1 for pair in oracle_pairs if outcome.get((name, pair), {}).get("oracle_exact_github_join"))
        exact_bpr = sum(1 for pair in oracle_pairs if outcome.get((name, pair), {}).get("oracle_exact"))
        exact_git = sum(1 for pair in oracle_pairs if outcome.get((name, pair), {}).get("oracle_exact_git_join"))
        flips = sum(outcome.get((name, pair), {}).get("comment_flips") or 0 for pair in oracle_pairs)
        compared = [pair for pair in minimal if (name, pair) in outcome and outcome[(name, pair)]["valid"] and outcome[(name, pair)]["edit_size"] is not None]
        edit = sum(outcome[(name, pair)]["edit_size"] for pair in compared)
        edit_min = sum(minimal[pair] for pair in compared)
        above = sum(1 for pair in compared if outcome[(name, pair)]["edit_size"] > minimal[pair])
        summary[name] = {
            "component": meta[name][0],
            "shape": meta[name][1],
            "pairs": len(rows),
            "invalid": invalid,
            "new_refusals": refusals,
            "approximate": sum(1 for row in rows if row["approximate"]),
            "fixed_baseline_approximate": len(fixed),
            "unfixed_baseline_approximate": unfixed,
            "new_approximate": new_approx,
            "lost_intraline": sum(row["lost_intraline"] or 0 for row in rows),
            "unstable_within_run": [pair for pair in pair_ids if (name, pair) in outcome and not outcome[(name, pair)]["stable"]],
            "oracle_exact": exact,
            "oracle_exact_bpr_hunks": exact_bpr,
            "oracle_exact_git_join": exact_git,
            "oracle_files": len(oracle_pairs),
            "comment_flips": flips,
            "edit_over_minimal": (edit / edit_min) if edit_min else math.nan,
            "pairs_above_minimal": above,
            "time_ratio": {category: ratio(name, class_pairs(category), time_of) for category in CLASSES},
            "memory_ratio": {category: ratio(name, class_pairs(category), memory) for category in CLASSES},
            "time_median_ms": {category: median([time_of[(name, pair)] for pair in class_pairs(category) if (name, pair) in time_of]) for category in CLASSES},
            "time_max_ms": max([time_of[(name, pair)] for pair in pair_ids if (name, pair) in time_of] or [math.nan]),
            "memory_median_kb": {category: median([memory[(name, pair)] for pair in class_pairs(category) if (name, pair) in memory]) for category in CLASSES},
        }

    # ------------------------------------------------------------ phases
    phase_time = collections.defaultdict(list)
    for row in phases:
        phase_time[(row["candidate"], row["pair"], "refine")].append(row["refine_ms"])
        phase_time[(row["candidate"], row["pair"], "assemble")].append(row["assemble_ms"])
        phase_time[(row["candidate"], row["pair"], "primary")].append(row["primary_ms"])
    refine = {(name, pair): median(values) for (name, pair, phase), values in phase_time.items() if phase == "refine"}
    assemble = {(name, pair): median(values) for (name, pair, phase), values in phase_time.items() if phase == "assemble"}
    primary_phase = {(name, pair): median(values) for (name, pair, phase), values in phase_time.items() if phase == "primary"}
    for name in summary:
        chosen = size_pairs("M") + size_pairs("L")
        summary[name]["refine_ratio_ML"] = ratio(name, chosen, refine)
        summary[name]["assemble_ratio_ML"] = ratio(name, chosen, assemble)
        summary[name]["primary_phase_ratio"] = {category: ratio(name, class_pairs(category), primary_phase) for category in CLASSES}

    # ------------------------------------------------------------ intraline pairs
    pair_times = collections.defaultdict(list)
    pair_valid = collections.defaultdict(lambda: True)
    for row in intraline:
        pair_times[row["candidate"]].append(row["time_ms"])
        pair_valid[row["candidate"]] = pair_valid[row["candidate"]] and bool(row.get("valid"))
    for name in summary:
        summary[name]["intraline_pair_p95_ms"] = percentile(pair_times.get(name, []), 0.95)
        summary[name]["intraline_pair_count"] = len(pair_times.get(name, []))
        summary[name]["intraline_pairs_valid"] = pair_valid[name] if name in pair_times else None

    # ------------------------------------------------------------ stops
    stop_values = collections.defaultdict(list)
    stop_counts = collections.Counter()
    abandoned = collections.defaultdict(list)
    for row in stops:
        category = pairs[row["pair"]]["class"] if pairs[row["pair"]]["class"] == "pathological" else pairs[row["pair"]]["size"]
        stop_counts[(row["candidate"], category, row["stopped"])] += 1
        if row["stopped"]:
            stop_values[(row["candidate"], category)].append(row["stop_to_return_ms"])
            if row.get("abandoned_work_ms"):
                abandoned[row["candidate"]].append(row["abandoned_work_ms"])
    for name in summary:
        gate_values = stop_values.get((name, "L"), []) + stop_values.get((name, "pathological"), [])
        summary[name]["stop_p95_L_patho_ms"] = percentile(gate_values, 0.95)
        summary[name]["stop_max_L_patho_ms"] = max(gate_values) if gate_values else math.nan
        summary[name]["stop_samples_L_patho"] = len(gate_values)
        summary[name]["abandoned_work_p95_ms"] = percentile(abandoned.get(name, []), 0.95)

    # ------------------------------------------------------------ determinism
    repeatable = collections.defaultdict(lambda: [0, 0])
    digests = {}
    for row in determinism:
        repeatable[row["candidate"]][0] += 1 if row["repeatable"] else 0
        repeatable[row["candidate"]][1] += 1
        digests[(row["candidate"], row["pair"])] = row["digest"]
    version_differences = {}
    for name in summary:
        summary[name]["repeatable"] = repeatable[name] if name in repeatable else None
        if name.startswith("p-"):
            _, _, rest = name.partition("-")
            version, _, variant = rest.partition("-")
            siblings = [other for other in summary if other.startswith("p-") and other.split("-", 2)[2] == variant]
            differing = []
            for pair in pair_ids:
                values = {digests.get((other, pair)) for other in siblings if (other, pair) in digests}
                if len(values) > 1:
                    differing.append(pair)
            version_differences[variant] = differing
            summary[name]["differs_across_git_versions"] = differing

    # ------------------------------------------------------------ suites
    # A failed test is new when the baseline did not fail it the same way.
    # The two suites that stop at their first failure are judged from their
    # inventories: every failed expectation the baseline does not share must
    # carry a verdict other than "regression" in the adjudication file.
    def first_failure(row: dict) -> str:
        lines = [line for line in (row.get("failure_tail") or "").splitlines() if line.startswith("FAILED") or "benchmark-failed" in line]
        return lines[-1] if lines else (row.get("failure_tail") or "")[-160:]

    suite_results = collections.defaultdict(dict)
    for row in suites:
        suite_results[row["candidate"]][row["test"]] = row
    inventory = collections.defaultdict(dict)
    for row in inventories:
        inventory[row["candidate"]][row["suite"]] = row
    baseline_suites = suite_results.get("baseline", {})
    baseline_inventory = inventory.get("baseline", {})
    for name in summary:
        rows = suite_results.get(name, {})
        new_failures = []
        for test, row in sorted(rows.items()):
            if row["status"] == "passed":
                continue
            if test in inventory.get(name, {}):
                shared = set(baseline_inventory.get(test, {}).get("failed", []))
                entry = inventory[name][test]
                if not entry["completed"]:
                    new_failures.append((test, f"inventory did not finish ({entry['status']})"))
                new_failures += [(test, message) for message in entry["failed"] if message not in shared]
                continue
            base_row = baseline_suites.get(test)
            if base_row and base_row["status"] != "passed" and first_failure(base_row) == first_failure(row):
                continue
            new_failures.append((test, first_failure(row)))
        verdicts = [(test, message, adjudication.get(f"{test}|{message}", {}).get("verdict", "unadjudicated")) for test, message in new_failures]
        summary[name]["suites"] = {
            "total": len(rows),
            "failed": sorted(test for test, row in rows.items() if row["status"] != "passed"),
            "new_failures": verdicts,
            "unexplained": [f"{test}: {message} ({verdict})" for test, message, verdict in verdicts if verdict in ("regression", "unadjudicated")],
            "ran": bool(rows),
        }

    # ------------------------------------------------------------ gates and decisions
    baseline = summary["baseline"]
    decisions = {}
    for name, item in summary.items():
        if name == "baseline":
            continue
        gates = {}
        suites_state = item["suites"]
        gates["correctness"] = (suites_state["ran"] and not suites_state["unexplained"] and not item["invalid"] and not item["new_refusals"])
        repeat = item["repeatable"]
        gates["determinism"] = bool(repeat) and repeat[0] == repeat[1] and not item["unstable_within_run"]
        gates["interruption"] = item["stop_samples_L_patho"] > 0 and item["stop_p95_L_patho_ms"] <= 10.0
        gates["placement"] = True
        gates["licence"] = True
        failed = [gate for gate, passed in gates.items() if not passed]
        component = item["component"]
        rule = {}
        if component == "primary":
            rule["no approximate where baseline is (L, pathological)"] = not item["unfixed_baseline_approximate"]
            rule["2x faster or 2x smaller (L, pathological)"] = all(
                (item["time_ratio"][category] <= 0.5 or item["memory_ratio"][category] <= 0.5) for category in ("L", "pathological"))
            rule["no class median regresses > 10%"] = all(not (item["time_ratio"][category] > 1.10) for category in CLASSES)
            rule["GitHub agreement >= baseline"] = item["oracle_exact"] >= baseline["oracle_exact"]
            rule["comment flips do not increase"] = item["comment_flips"] <= baseline["comment_flips"]
        elif component == "slide":
            reference = baseline
            if name.startswith(("p-", "l-")):
                reference = summary.get(name[: -len("-indent")], baseline)
            rule["GitHub agreement rises"] = item["oracle_exact"] > reference["oracle_exact"]
            rule["costs at most 5% of compute"] = all(not (item["time_ratio"][category] > 1.05 * (reference["time_ratio"][category] if reference is not baseline else 1.0))
                                                     for category in CLASSES)
        elif component == "pairing":
            rule["pairing decisions bit-identical"] = all(outcome.get((name, pair), {}).get("pairs_digest") == outcome.get(("baseline", pair), {}).get("pairs_digest")
                                                         for pair in pair_ids)
            rule["pairing phase 2x faster on M and L"] = item["refine_ratio_ML"] <= 0.5
        elif component == "intraline":
            rule["every span valid"] = bool(item["intraline_pairs_valid"]) and not item["invalid"]
            rule["p95 time per pair no worse"] = item["intraline_pair_p95_ms"] <= baseline["intraline_pair_p95_ms"]
        rule_failed = [text for text, passed in rule.items() if not passed]
        answer = owner_answer(name)
        if failed:
            decision = "reject: " + ", ".join(failed) + " gate"
        elif rule_failed:
            decision = "reject: " + "; ".join(rule_failed)
        elif component == "intraline":
            decision = "eligible pending owner render review (AC06)" + (f" and {answer}" if answer else "")
        elif answer:
            decision = f"eligible pending {answer}"
        else:
            decision = "adopt"
        decisions[name] = {"gates": gates, "rule": rule, "decision": decision}

    # ------------------------------------------------------------ raw output
    raw = {
        "schema": "bpr.diff-evaluation.results.v1",
        "station": arguments.station,
        "environment": environment,
        "pairs": {pair: {key: pairs[pair][key] for key in ("class", "size", "origin", "options", "left_lines", "right_lines", "left_sha", "right_sha")}
                  | {"oracle": bool(pairs[pair].get("oracle"))} for pair in pair_ids},
        "samples": {f"{name}|{pair}": [round(value, 3) for value in samples[(name, pair)]] for name, pair in sorted(samples)},
        "outcomes": {f"{name}|{pair}": outcome[(name, pair)] for name, pair in sorted(outcome)},
        "memory_kb": {f"{name}|{pair}": value for (name, pair), value in sorted(memory.items())},
        "phases_ms": {f"{name}|{pair}": {"primary": primary_phase.get((name, pair)), "refine": refine.get((name, pair)), "assemble": assemble.get((name, pair))}
                      for (name, pair) in sorted(refine)},
        "stops_ms": {f"{name}|{category}": [round(value, 3) for value in values] for (name, category), values in sorted(stop_values.items())},
        "stop_counts": {f"{name}|{category}|{'stopped' if stopped else 'finished-first'}": count for (name, category, stopped), count in sorted(stop_counts.items())},
        "abandoned_work_ms": {name: [round(value, 3) for value in values] for name, values in sorted(abandoned.items())},
        "intraline_pair_ms": {name: [round(value, 4) for value in values] for name, values in sorted(pair_times.items())},
        "overhead": overhead,
        "git_version_differences": version_differences,
        "suites": suites,
        "summary": summary,
        "decisions": decisions,
    }
    arguments.raw.parent.mkdir(parents=True, exist_ok=True)
    arguments.raw.write_text(json.dumps(raw, sort_keys=True, separators=(",", ":"), default=lambda value: None) + "\n")

    # ------------------------------------------------------------ markdown
    lines = [f"<!-- Generated by tests/evaluation/diff_engine/scripts/make_report.py from the {arguments.station} raw results. -->", ""]
    lines += [f"## {arguments.station}: environment", ""]
    for key in sorted(environment):
        lines.append(f"- **{key}:** {environment[key]}")
    if loads:
        lines.append(f"- **one-minute load average during measure:** median {fmt(median(loads))}, range {fmt(min(loads))}-{fmt(max(loads))} "
                     f"on {environment.get('cpu_count', '?')} cores")
    lines += ["", f"## {arguments.station}: corpus", ""]
    counts = collections.Counter((pairs[pair]["class"], pairs[pair]["size"]) for pair in pair_ids)
    lines += ["| Class | Size | Comparisons |", "|---|---|---|"]
    for (category, size), count in sorted(counts.items()):
        lines.append(f"| {category} | {size} | {count} |")
    lines += ["", f"{len(oracle_pairs)} comparisons carry a GitHub oracle.", ""]

    lines += [f"## {arguments.station}: time and memory by class", "",
              "Ratios are the median over the class's comparisons of candidate time (or added peak memory) over the baseline's; below 1 is faster or smaller.", "",
              "| Candidate | Component | Shape | " + " | ".join(f"{category} ms" for category in CLASSES) + " | " + " | ".join(f"{category} ratio" for category in CLASSES)
              + " | L mem ratio | patho mem ratio | worst ms |",
              "|---|---|---|" + "---|" * (2 * len(CLASSES) + 3)]
    for name in candidates:
        item = summary[name]
        lines.append(f"| {name} | {item['component']} | {item['shape']} | "
                     + " | ".join(fmt(item["time_median_ms"][category]) for category in CLASSES) + " | "
                     + " | ".join(fmt(item["time_ratio"][category]) for category in CLASSES) + " | "
                     + f"{fmt(item['memory_ratio']['L'])} | {fmt(item['memory_ratio']['pathological'])} | {fmt(item['time_max_ms'])} |")

    lines += ["", f"## {arguments.station}: outcomes, approximation and GitHub agreement", "",
              "GitHub exact compares the candidate's hunks, joined as GitHub's file list joins them, with the recording. "
              "BPR hunks exact compares BPR's comment hunks (`github_review_hunks`) instead; comment flips count lines whose in-hunk membership differs from GitHub's.", "",
              "| Candidate | Invalid | New refusals | Approximate | Baseline approximations fixed (L, patho) | Lost intraline | Edit / minimal | Above minimal | GitHub exact | Git-join exact | BPR hunks exact | Comment flips |",
              "|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for name in candidates:
        item = summary[name]
        lines.append(f"| {name} | {len(item['invalid'])} | {len(item['new_refusals'])} | {item['approximate']} | {item['fixed_baseline_approximate']} | {item['lost_intraline']} | "
                     f"{fmt(item['edit_over_minimal'], 4)} | {item['pairs_above_minimal']} | {item['oracle_exact']}/{item['oracle_files']} | {item['oracle_exact_git_join']} | "
                     f"{item['oracle_exact_bpr_hunks']} | {item['comment_flips']} |")

    lines += ["", f"## {arguments.station}: stops", "",
              "Stop-to-return at the host, over samples a stop interrupted; process candidates include the process-group kill and reap. "
              "The linked shape's host abandons its worker, so its return is immediate and the abandoned work runs on.", "",
              "| Candidate | Samples (L, patho) | p95 ms | worst ms | abandoned work p95 ms |", "|---|---|---|---|---|"]
    for name in candidates:
        item = summary[name]
        lines.append(f"| {name} | {item['stop_samples_L_patho']} | {fmt(item['stop_p95_L_patho_ms'], 3)} | {fmt(item['stop_max_L_patho_ms'], 3)} | {fmt(item['abandoned_work_p95_ms'])} |")

    lines += ["", f"## {arguments.station}: process overhead", "", "| Git | Tiny diff median ms | p95 ms | `--version` median ms |", "|---|---|---|---|"]
    by_program = collections.defaultdict(list)
    for row in overhead:
        by_program[(row["program"], row.get("version", "").strip())].append(row)
    for (program, version), rows in sorted(by_program.items()):
        lines.append(f"| {program} ({version}) | {fmt(median([row['tiny_diff_ms'] for row in rows]))} | {fmt(percentile([row['tiny_diff_ms'] for row in rows], 0.95))} | "
                     f"{fmt(median([row['version_ms'] for row in rows]))} |")

    lines += ["", f"## {arguments.station}: pairing and intraline phases", "",
              "| Candidate | Pairing phase ratio (M, L) | Assemble phase ratio (M, L) | Intraline p95 ms per pair | Pairs timed |", "|---|---|---|---|---|"]
    for name in candidates:
        item = summary[name]
        if item["component"] in ("baseline", "pairing", "intraline"):
            lines.append(f"| {name} | {fmt(item['refine_ratio_ML'])} | {fmt(item['assemble_ratio_ML'])} | {fmt(item['intraline_pair_p95_ms'], 4)} | {item['intraline_pair_count']} |")

    lines += ["", f"## {arguments.station}: determinism", ""]
    for name in candidates:
        item = summary[name]
        repeat = item["repeatable"]
        extra = ""
        if "differs_across_git_versions" in item:
            extra = f"; differs across Git 2.39.5, 2.56.0 and the installed Git on {len(item['differs_across_git_versions'])} comparisons"
        lines.append(f"- {name}: repeatable on {repeat[0] if repeat else '-'} of {repeat[1] if repeat else '-'} comparisons{extra}.")

    lines += ["", f"## {arguments.station}: suites", "",
              "New failures are those the baseline does not share in the same build; each carries its adjudication verdict.", "",
              "| Candidate | Tests | Failed tests | New failed expectations (verdict) |", "|---|---|---|---|"]
    for name in candidates:
        state = summary[name]["suites"]
        detail = "; ".join(f"{test}: {message} ({verdict})" for test, message, verdict in state["new_failures"]) or "-"
        lines.append(f"| {name} | {state['total']} | {', '.join(state['failed']) or '-'} | {detail} |")

    lines += ["", f"## {arguments.station}: gates and mechanical decisions", "",
              "| Candidate | Component | Correctness | Determinism | Interruption | Component rule | Decision |", "|---|---|---|---|---|---|---|"]
    for name in candidates:
        if name == "baseline":
            continue
        decision = decisions[name]
        rule = "; ".join(f"{text}: {'pass' if passed else 'fail'}" for text, passed in decision["rule"].items()) or "-"
        gates = decision["gates"]
        lines.append(f"| {name} | {summary[name]['component']} | {'pass' if gates['correctness'] else 'fail'} | {'pass' if gates['determinism'] else 'fail'} | "
                     f"{'pass' if gates['interruption'] else 'fail'} | {rule} | {decision['decision']} |")
    arguments.markdown.parent.mkdir(parents=True, exist_ok=True)
    arguments.markdown.write_text("\n".join(lines) + "\n")
    print(f"REPORT {arguments.markdown} RAW {arguments.raw}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
