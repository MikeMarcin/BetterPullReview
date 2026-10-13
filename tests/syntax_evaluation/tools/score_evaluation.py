#!/usr/bin/env python3
"""Scores RFC-BPR-0081's harness output against the committed labels.

Reads the spans and metrics that run_evaluation.py wrote, the labels under
tests/syntax_evaluation/labels and the materialized corpus, and writes one
summary JSON:

- accuracy: per pin, engine, source and category, true positives, false
  negatives and false positives. A label's predicted role is the role every
  byte of it carries, or "mixed". A completely labelled source also counts as a
  false positive every run of namespace or enum-member bytes no positive label
  expects; fragment labels are a derived subset, so fragments count only
  labelled errors.
- hypotheses: H1 to H7 confirmed or refuted per engine from the gap suite's
  labels, and H8 from the peak heap on the two 8 MiB inputs.
- timing, memory, stop latency and determinism, per size class.

Usage: python3 score_evaluation.py --repo-root ROOT --corpus DIR --results DIR --out FILE
"""

from __future__ import annotations

import argparse
import gzip
import json
import re
import statistics
import sys
from collections import defaultdict
from pathlib import Path

ROLE_HEADER = Path("src/core/include/bpr/core/appearance_model.hpp")
CATEGORIES = ["ns_decl", "enumerator_decl", "enum_use", "ns_qualifier", "conservative_negative", "macro_keep", "protected"]
GUESSED = {"syntax_namespace", "syntax_enum_member", "syntax_macro"}
EXPECTED_ROLE = {"namespace": "syntax_namespace", "enum_member": "syntax_enum_member", "macro": "syntax_macro"}
WORD = re.compile(rb"[A-Za-z_][A-Za-z0-9_]*")


def role_names(root: Path) -> list[str]:
    text = (root / ROLE_HEADER).read_text(encoding="utf-8")
    body = text.split("enum class appearance_role : std::uint8_t {", 1)[1].split("};", 1)[0]
    body = re.sub(r"//[^\n]*", "", body)
    return [name.strip() for name in body.split(",") if name.strip()]


def read_jsonl(path: Path):
    opener = gzip.open if path.suffix == ".gz" else open
    with opener(path, "rt", encoding="utf-8") as stream:
        for line in stream:
            if line.strip():
                yield json.loads(line)


def load_labels(root: Path) -> dict[str, dict[str, list]]:
    """source -> file -> records, with the adjudicated hand review applied."""
    labels: dict[str, dict[str, list]] = defaultdict(lambda: defaultdict(list))
    directory = root / "tests/syntax_evaluation/labels"
    for path in sorted(list(directory.glob("*.jsonl")) + list(directory.glob("*.jsonl.gz"))):
        source = path.name.split(".")[0]
        if source == "hand-review":
            continue
        for record in read_jsonl(path):
            labels[source][record["f"]].append(record)
    review = directory / "hand-review.jsonl"
    if review.exists():
        for item in read_jsonl(review):
            if item["c"] == "protected" or item["final"] == item["machine_after_rule_f"]:
                continue
            for record in labels[item["source"]][item["f"]]:
                if record.get("o") == item["o"] and record["c"] != "protected":
                    record["e"] = item["final"]
                    if item["final"] == "neutral":
                        record["c"] = "conservative_negative"
                    record["r"] = "hand"
                    record["m"] = "clang+hand"
    return labels


def line_starts(data: bytes) -> list[int]:
    starts = [0]
    index = data.find(b"\n")
    while index != -1:
        starts.append(index + 1)
        index = data.find(b"\n", index + 1)
    return starts


def role_bytes(data: bytes, starts: list[int], spans: list) -> bytearray:
    """One role ordinal per byte; 255 marks syntax_normal (no span)."""
    roles = bytearray(b"\xff" * len(data))
    for line, column, length, role in spans:
        begin = starts[line] + column
        roles[begin:begin + length] = bytes([role]) * length
    return roles


class scorer:
    def __init__(self, root: Path, corpus: Path) -> None:
        self.root = root
        self.corpus = corpus
        self.names = role_names(root)
        self.labels = load_labels(root)
        self.file_source = {path: source for source, files in self.labels.items() for path in files}
        # Clang's coverage: only fully labelled files count unlabelled guesses,
        # and never inside a preprocessor branch clang skipped.
        coverage_path = root / "tests/syntax_evaluation/labels/coverage.json"
        self.coverage = {}
        if coverage_path.exists():
            coverage = json.loads(coverage_path.read_text(encoding="utf-8"))
            self.coverage = {entry["f"]: entry for entry in coverage.get("files", [])}

    def name(self, ordinal: int) -> str:
        return "syntax_normal" if ordinal == 255 else self.names[ordinal]

    def predicted(self, roles: bytearray, offset: int, length: int) -> str:
        chunk = set(roles[offset:offset + length])
        return self.name(chunk.pop()) if len(chunk) == 1 else "mixed"

    def score_file(self, path: str, engines: dict[str, list], tallies: dict, failures: list) -> None:
        source = self.file_source.get(path)
        if source is None:
            return
        data = (self.corpus / path).read_bytes()
        starts = line_starts(data)
        grammar = role_bytes(data, starts, engines.get("grammar", []))
        records = self.labels[source][path]
        entry = self.coverage.get(path)
        complete = source != "fragments" and (entry is None or entry.get("status") == "full")
        skipped = [tuple(item) for item in ((entry or {}).get("unparsed") or [])]
        for engine in ("baseline", "candidate"):
            if engine not in engines:
                continue
            roles = role_bytes(data, starts, engines[engine])
            tally = tallies[engine][source]
            expected_spans: dict[str, list[tuple[int, int]]] = {"syntax_namespace": [], "syntax_enum_member": []}
            for record in records:
                category = record["c"]
                if category == "protected":
                    for begin, end in record["ranges"]:
                        for match in WORD.finditer(data, begin, end):
                            tally["protected"]["labels"] += 1
                            if roles[match.start():match.end()] != grammar[match.start():match.end()]:
                                tally["protected"]["fp"] += 1
                                failures.append((engine, path, match.start(), "protected", match.group().decode("utf-8", "replace")))
                    continue
                offset, length, expectation = record["o"], record["n"], record["e"]
                guess = self.predicted(roles, offset, length)
                tally[category]["labels"] += 1
                if expectation in EXPECTED_ROLE:
                    wanted = EXPECTED_ROLE[expectation]
                    if wanted in expected_spans:
                        expected_spans[wanted].append((offset, offset + length))
                    if guess == wanted:
                        tally[category]["tp"] += 1
                    else:
                        tally[category]["fn"] += 1
                        failures.append((engine, path, offset, category, f"{record['t']}: {guess} (want {wanted}, {record.get('r')})"))
                elif expectation == "neutral":
                    if guess in GUESSED or guess == "mixed" and any(self.name(r) in GUESSED for r in set(roles[offset:offset + length])):
                        tally[category]["fp"] += 1
                        failures.append((engine, path, offset, category, f"{record['t']}: {guess} ({record.get('r')})"))
            if complete:
                self.unlabelled_guesses(data, roles, expected_spans, records, tally, failures, engine, path, skipped)

    def unlabelled_guesses(self, data: bytes, roles: bytearray, expected: dict, records: list, tally: dict,
                           failures: list, engine: str, path: str, skipped: list) -> None:
        neutral = {record["o"] for record in records if record.get("e") == "neutral"}
        for role_name in ("syntax_namespace", "syntax_enum_member"):
            ordinal = self.names.index(role_name)
            covered = expected[role_name]
            at = 0
            while True:
                at = roles.find(bytes([ordinal]), at)
                if at < 0:
                    break
                end = at
                while end < len(roles) and roles[end] == ordinal:
                    end += 1
                in_skipped = any(item[0] <= at < item[1] for item in skipped if len(item) >= 2)
                if at not in neutral and not in_skipped and not any(begin <= at < stop for begin, stop in covered):
                    before = data[max(0, at - 64):at].rstrip()
                    after = data[end:end + 3].lstrip()
                    if role_name == "syntax_namespace":
                        category = "ns_qualifier" if after.startswith(b"::") else "ns_decl"
                    else:
                        category = "enum_use" if before.endswith(b"::") else "enumerator_decl"
                    tally[category]["fp"] += 1
                    failures.append((engine, path, at, category, f"unlabelled {role_name}: {data[at:end].decode('utf-8', 'replace')}"))
                at = end


def percentile(values: list[float], fraction: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int(round(fraction * (len(ordered) - 1)))))
    return ordered[index]


def summarize_metrics(results: Path, pin: str) -> dict:
    timing: dict = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))
    heap: dict = defaultdict(lambda: defaultdict(list))
    per_file_heap: dict = defaultdict(dict)
    digests: dict = defaultdict(set)
    statuses: dict = defaultdict(lambda: defaultdict(int))
    stops: dict = defaultdict(lambda: defaultdict(list))
    stop_failures: list = []
    resident: dict = {}
    composer: dict = defaultdict(list)
    uncancelled: dict = {}
    for path in sorted((results / pin).glob("*.metrics.jsonl")):
        for record in read_jsonl(path):
            if "digest" in record and "mode" not in record and record.get("round") == 0:
                uncancelled[(record["f"], record["engine"])] = (record["status"], record["digest"], record["spans"])
    stop_outcomes: dict = defaultdict(lambda: defaultdict(int))
    for path in sorted((results / pin).glob("*.metrics.jsonl")):
        corpus_class = path.name.split(".")[0]
        per_file: dict = defaultdict(lambda: defaultdict(list))
        for record in read_jsonl(path):
            if record.get("summary"):
                resident[corpus_class] = record["peak_resident_bytes"]
                continue
            if "skipped" in record:
                continue
            engine = record["engine"]
            if "mode" in record:
                if record["missed"]:
                    stop_outcomes[engine]["finished before the stop"] += 1
                    continue
                stops[(record["size_class"], record["mode"])][engine].append(record["latency_ns"] / 1e6)
                whole = uncancelled.get((record["f"], engine))
                if record["status"] == "cancelled" and record["spans"] == 0:
                    stop_outcomes[engine]["cancelled, no spans"] += 1
                elif whole and record["status"] == whole[0] and record.get("digest", whole[1]) == whole[1] and record["spans"] == whole[2]:
                    # The stop arrived after the last check: the complete
                    # answer, not a partial one.
                    stop_outcomes[engine]["complete answer (stop after the last check)"] += 1
                else:
                    stop_outcomes[engine]["partial or other"] += 1
                    stop_failures.append(record)
                continue
            size_class = record["size_class"]
            statuses[engine][record["status"]] += 1
            digests[(record["f"], engine)].add(record["digest"])
            per_file[(record["f"], size_class)][engine].append(record)
        for (file, size_class), engines in per_file.items():
            for engine, rows in engines.items():
                for measure in ("total_ns", "refine_ns", "grammar_ns"):
                    timing[size_class][engine][measure].append(statistics.median(row[measure] for row in rows) / 1e6)
                peak = max(row["peak_heap"] for row in rows)
                heap[size_class][engine].append(peak)
                per_file_heap[engine][file] = peak
                if corpus_class == "composer":
                    composer[engine].append(statistics.median(row["total_ns"] for row in rows) / 1e6)
    summary: dict = {"timing": {}, "heap": {}, "stop": {}, "resident_bytes": resident, "statuses": statuses}
    for size_class, engines in timing.items():
        summary["timing"][size_class] = {
            engine: {measure: {"files": len(values), "median_ms": statistics.median(values), "p95_ms": percentile(values, 0.95),
                               "max_ms": max(values)} for measure, values in measures.items()}
            for engine, measures in engines.items()}
    for size_class, engines in heap.items():
        summary["heap"][size_class] = {engine: {"max_bytes": max(values), "median_bytes": statistics.median(values)}
                                       for engine, values in engines.items()}
    for (size_class, mode), engines in stops.items():
        summary["stop"][f"{size_class}/{mode}"] = {
            engine: {"trials": len(values), "p95_ms": percentile(values, 0.95), "max_ms": max(values),
                     "median_ms": statistics.median(values)} for engine, values in engines.items()}
    summary["stop_publication_failures"] = len(stop_failures)
    summary["stop_outcomes"] = {engine: dict(values) for engine, values in stop_outcomes.items()}
    summary["nondeterministic"] = sorted(f"{file}:{engine}" for (file, engine), seen in digests.items() if len(seen) > 1)
    summary["composer_per_edit"] = {engine: {"edits": len(values), "median_ms": statistics.median(values),
                                             "p95_ms": percentile(values, 0.95)} for engine, values in composer.items()}
    summary["per_file_heap"] = per_file_heap
    return summary


def hypotheses(failures: list, heaps: dict) -> dict:
    result: dict = {}
    files = {"H1": "gap/h1_inline_namespace.cpp", "H2": "gap/h2_attributed_namespace.cpp", "H3": "gap/h3_enum_comparisons.cpp",
             "H4": "gap/h4_x_macro.cpp", "H5": "gap/h5_conditional_enum.cpp", "H6": "gap/h6_brace_shadow.cpp",
             "H7": "gap/h7_non_ascii.cpp"}
    for engine in ("baseline", "candidate"):
        for hypothesis, path in files.items():
            hits = [failure for failure in failures if failure[0] == engine and failure[1] == path]
            result.setdefault(hypothesis, {})[engine] = {"status": "confirmed" if hits else "refuted",
                                                          "failures": [failure[4] for failure in hits]}
        per_file = heaps.get(engine, {})
        if per_file:
            largest = max(per_file, key=per_file.get)
            eight = {file: bytes_ for file, bytes_ in per_file.items() if "brace-flood" in file or "enum-names-8mib" in file}
            result.setdefault("H8", {})[engine] = {
                "status": "confirmed" if largest in eight else "refuted", "largest_input": largest,
                "largest_peak_heap": per_file[largest], "eight_mib_inputs": eight}
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--corpus", type=Path, required=True)
    parser.add_argument("--results", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--failures", type=Path)
    arguments = parser.parse_args()
    score = scorer(arguments.repo_root.resolve(), arguments.corpus)
    report: dict = {"pins": {}}
    all_failures: list = []
    for pin_directory in sorted(path for path in arguments.results.iterdir() if path.is_dir()):
        pin = pin_directory.name
        tallies: dict = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda: defaultdict(int))))
        failures: list = []
        for spans_path in sorted(pin_directory.glob("*.spans.jsonl")):
            current: dict[str, dict[str, list]] = defaultdict(dict)
            for record in read_jsonl(spans_path):
                current[record["f"]][record["engine"]] = record["spans"]
            for path, engines in current.items():
                score.score_file(path, engines, tallies, failures)
        metrics = summarize_metrics(arguments.results, pin)
        report["pins"][pin] = {
            "accuracy": json.loads(json.dumps(tallies)),
            "hypotheses": hypotheses(failures, metrics.pop("per_file_heap")),
            "metrics": json.loads(json.dumps(metrics, default=list)),
        }
        all_failures.extend((pin, *failure) for failure in failures)
    arguments.out.write_text(json.dumps(report, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    if arguments.failures:
        with arguments.failures.open("w", encoding="utf-8") as stream:
            for failure in all_failures:
                stream.write(json.dumps(failure, ensure_ascii=False) + "\n")
    print(f"scored {len(report['pins'])} pins; {len(all_failures)} label disagreements -> {arguments.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
