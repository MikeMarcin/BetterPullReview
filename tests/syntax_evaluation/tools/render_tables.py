#!/usr/bin/env python3
"""Renders score_evaluation.py's summary as the report's Markdown tables.

Usage: python3 render_tables.py SUMMARY.json [--size SIZE.json]
Writes Markdown to standard output; the report quotes it, so no number in the
report is typed by hand.
"""

from __future__ import annotations

import argparse
import json
import sys

CATEGORIES = ["ns_decl", "enumerator_decl", "enum_use", "ns_qualifier", "conservative_negative", "macro_keep", "protected"]
SIZE_CLASSES = ["S", "M", "L", "pathological", "fragment"]


def accuracy(pin: str, data: dict) -> list[str]:
    lines = [f"#### Accuracy, pin `{pin}`", "",
             "| Source | Category | Labels | Baseline FP | Baseline FN | T FP | T FN |", "|---|---|---|---|---|---|---|"]
    baseline = data["accuracy"].get("baseline", {})
    candidate = data["accuracy"].get("candidate", {})
    totals = {engine: {"fp": 0, "fn": 0} for engine in ("baseline", "candidate")}
    for source in sorted(set(baseline) | set(candidate)):
        for category in CATEGORIES:
            b = baseline.get(source, {}).get(category, {})
            c = candidate.get(source, {}).get(category, {})
            labels = b.get("labels", c.get("labels", 0))
            if not labels and not b.get("fp") and not c.get("fp"):
                continue
            lines.append(f"| {source} | {category} | {labels} | {b.get('fp', 0)} | {b.get('fn', 0)} | {c.get('fp', 0)} | {c.get('fn', 0)} |")
            for engine, values in (("baseline", b), ("candidate", c)):
                totals[engine]["fp"] += values.get("fp", 0)
                totals[engine]["fn"] += values.get("fn", 0)
    lines.append(f"| **all** | | | **{totals['baseline']['fp']}** | **{totals['baseline']['fn']}** | **{totals['candidate']['fp']}** | **{totals['candidate']['fn']}** |")
    return lines + [""]


def per_category(pin: str, data: dict) -> list[str]:
    lines = [f"#### Errors per category across sources, pin `{pin}`", "",
             "| Category | Baseline FP | T FP | New T FP | Baseline FN | T FN |", "|---|---|---|---|---|---|"]
    for category in CATEGORIES:
        sums = {}
        for engine in ("baseline", "candidate"):
            per_source = data["accuracy"].get(engine, {})
            sums[engine] = (sum(values.get(category, {}).get("fp", 0) for values in per_source.values()),
                            sum(values.get(category, {}).get("fn", 0) for values in per_source.values()))
        gained = max(0, sums["candidate"][0] - sums["baseline"][0])
        lines.append(f"| {category} | {sums['baseline'][0]} | {sums['candidate'][0]} | {gained} | {sums['baseline'][1]} | {sums['candidate'][1]} |")
    return lines + [""]


def hypotheses(pin: str, data: dict) -> list[str]:
    lines = [f"#### Gap hypotheses, pin `{pin}`", "", "| Hypothesis | Baseline | T |", "|---|---|---|"]
    for name in sorted(data["hypotheses"]):
        row = data["hypotheses"][name]
        cell = lambda engine: row.get(engine, {}).get("status", "not run")
        lines.append(f"| {name} | {cell('baseline')} | {cell('candidate')} |")
    return lines + [""]


def timing(pin: str, data: dict) -> list[str]:
    lines = [f"#### Classification time (ms per file, medians of 3 rounds), pin `{pin}`", "",
             "| Size class | Files | Measure | Baseline median | T median | Ratio | Baseline p95 | T p95 |",
             "|---|---|---|---|---|---|---|---|"]
    for size_class in SIZE_CLASSES:
        engines = data["metrics"]["timing"].get(size_class)
        if not engines:
            continue
        for measure in ("total_ns", "grammar_ns", "refine_ns"):
            b = engines.get("baseline", {}).get(measure)
            c = engines.get("candidate", {}).get(measure)
            if not b or not c:
                continue
            ratio = c["median_ms"] / b["median_ms"] if b["median_ms"] else float("inf")
            lines.append(f"| {size_class} | {b['files']} | {measure.replace('_ns', '')} | {b['median_ms']:.3f} | {c['median_ms']:.3f} | {ratio:.2f} | {b['p95_ms']:.3f} | {c['p95_ms']:.3f} |")
    return lines + [""]


def memory(pin: str, data: dict) -> list[str]:
    lines = [f"#### Peak refinement heap (bytes), pin `{pin}`", "",
             "| Size class | Baseline max | T max | Ratio | Baseline median | T median |", "|---|---|---|---|---|---|"]
    for size_class in SIZE_CLASSES:
        engines = data["metrics"]["heap"].get(size_class)
        if not engines or "baseline" not in engines or "candidate" not in engines:
            continue
        b, c = engines["baseline"], engines["candidate"]
        ratio = c["max_bytes"] / b["max_bytes"] if b["max_bytes"] else float("inf")
        lines.append(f"| {size_class} | {b['max_bytes']:,} | {c['max_bytes']:,} | {ratio:.2f} | {int(b['median_bytes']):,} | {int(c['median_bytes']):,} |")
    lines += ["", "| Corpus class process | Peak resident bytes |", "|---|---|"]
    for corpus_class, value in sorted(data["metrics"]["resident_bytes"].items()):
        lines.append(f"| {corpus_class} | {value:,} |")
    return lines + [""]


def stops(pin: str, data: dict) -> list[str]:
    lines = [f"#### Time from stop to return (ms), pin `{pin}`", "",
             "| Size class / mode | Engine | Trials | Median | p95 | Max |", "|---|---|---|---|---|---|"]
    for key in sorted(data["metrics"]["stop"]):
        for engine, values in sorted(data["metrics"]["stop"][key].items()):
            lines.append(f"| {key} | {engine} | {values['trials']} | {values['median_ms']:.3f} | {values['p95_ms']:.3f} | {values['max_ms']:.3f} |")
    lines.append("")
    for engine, outcomes in sorted(data["metrics"].get("stop_outcomes", {}).items()):
        lines.append(f"Stop outcomes, {engine}: " + "; ".join(f"{name} {count}" for name, count in sorted(outcomes.items())) + ".")
    lines.append("")
    lines.append(f"Stops that published a partial answer: {data['metrics']['stop_publication_failures']}. "
                 f"Inputs whose spans differed between rounds: {len(data['metrics']['nondeterministic'])}.")
    composer = data["metrics"].get("composer_per_edit", {})
    if composer:
        lines.append("")
        lines.append("| Composer replay | Edits | Median ms per edit | p95 |")
        lines.append("|---|---|---|---|")
        for engine, values in sorted(composer.items()):
            lines.append(f"| {engine} | {values['edits']} | {values['median_ms']:.3f} | {values['p95_ms']:.3f} |")
    return lines + [""]


def size_table(size: dict) -> list[str]:
    lines = ["#### Executable size added per macOS slice", "", "| Slice | Baseline probe | v0.23.4 added | master added |", "|---|---|---|---|"]
    for arch, values in size["slices"].items():
        lines.append(f"| {arch} | {values['baseline']['bytes']:,} B | {values['v0_23_4']['added_bytes']:,} B ({values['v0_23_4']['added_mib']} MiB) | "
                     f"{values['master']['added_bytes']:,} B ({values['master']['added_mib']} MiB) |")
    lines += ["", "| Translation unit | arm64 s | x86_64 s |", "|---|---|---|"]
    for name, values in size["compile_seconds"].items():
        lines.append(f"| {name} | {values.get('arm64', '')} | {values.get('x86_64', '')} |")
    return lines + [""]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("summary")
    parser.add_argument("--size")
    arguments = parser.parse_args()
    with open(arguments.summary, encoding="utf-8") as stream:
        summary = json.load(stream)
    out: list[str] = []
    for pin, data in summary["pins"].items():
        out += per_category(pin, data) + accuracy(pin, data) + hypotheses(pin, data) + timing(pin, data) + memory(pin, data) + stops(pin, data)
    if arguments.size:
        with open(arguments.size, encoding="utf-8") as stream:
            out += size_table(json.load(stream))
    sys.stdout.write("\n".join(out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
