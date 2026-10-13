#!/usr/bin/env python3
"""Writes the hand labels of the RFC-BPR-0081 gap suite.

The gap fixtures in tests/fixtures/syntax/evaluation/gap are small enough to
label exhaustively by hand. The table below is that hand labelling: every
identifier occurrence the RFC-BPR-0072 contract has an opinion about, with the
role it must carry. This script only turns (line, word, occurrence) into byte
offsets and adds the protected ranges (comments, string literals and whole
directive lines), which are mechanical for these files. It reads no classifier
output.

Usage: python3 label_gap.py --repo-root ROOT [--output FILE]
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

GAP_DIRECTORY = Path("tests/fixtures/syntax/evaluation/gap")
CORPUS_PREFIX = "gap/"

# (line, word, occurrence on that line, category, expectation, subcategory,
#  declaring (line, word, occurrence) or None, rule)
# Lines are 1-based as an editor shows them; occurrence counts whole-word
# matches on the line from 1.
NS = ("ns_decl", "namespace")
ENUM = ("enumerator_decl", "enum_member")
USE = ("enum_use", "enum_member")
QUAL = ("ns_qualifier", "namespace")
NEG = ("conservative_negative", "neutral")
MACRO = ("macro_keep", "macro")

LABELS: dict[str, list[tuple]] = {
    "h1_inline_namespace.cpp": [
        (2, "A", 1, *NS, "nested", None, "H1"),
        (2, "B", 1, *NS, "nested", None, "H1"),
        (3, "Fast", 1, *ENUM, "scoped", None, "H1"),
        (3, "Slow", 1, *ENUM, "scoped", None, "H1"),
        (5, "A", 1, *QUAL, None, (2, "A", 1), "H1"),
        (5, "B", 1, *QUAL, None, (2, "B", 1), "H1"),
        (5, "Fast", 1, *USE, None, (3, "Fast", 1), "H1"),
    ],
    "h2_attributed_namespace.cpp": [
        (2, "Legacy", 1, *NS, "simple", None, "H2"),
        (3, "OK", 1, *ENUM, "unscoped", None, "H2"),
        (3, "FAILED", 1, *ENUM, "unscoped", None, "H2"),
        (5, "Legacy", 1, *QUAL, None, (2, "Legacy", 1), "H2"),
        (5, "FAILED", 1, *USE, None, (3, "FAILED", 1), "H2"),
    ],
    "h3_enum_comparisons.cpp": [
        (4, "Low", 1, *ENUM, "unscoped", None, "H3"),
        (4, "X", 1, *NEG, None, None, "H3-operand"),
        (4, "High", 1, *ENUM, "unscoped", None, "H3"),
        (4, "Y", 1, *NEG, None, None, "H3-operand"),
        (4, "Last", 1, *ENUM, "unscoped", None, "H3"),
        (5, "High", 1, *USE, None, (4, "High", 1), "H3"),
    ],
    "h4_x_macro.cpp": [
        (4, "COLOUR_LIST", 1, *MACRO, None, None, "H4"),
        (4, "ENTRY", 1, *MACRO, None, None, "H4"),
        # RED is produced by the macro, not declared by the syntax in the blob.
        (5, "RED", 1, *NEG, None, None, "R7"),
    ],
    "h5_conditional_enum.cpp": [
        (7, "Idle", 1, *ENUM, "unscoped", None, "H5"),
        (8, "Busy", 1, *ENUM, "unscoped", None, "H5"),
        (10, "first", 1, *NEG, None, None, "H5-after"),
        (10, "second", 1, *NEG, None, None, "H5-after"),
        (11, "left", 1, *NEG, None, None, "H5-after"),
        (11, "right", 1, *NEG, None, None, "H5-after"),
        (12, "a", 1, *NEG, None, None, "H5-after"),
        (12, "b", 1, *NEG, None, None, "H5-after"),
        (12, "c", 1, *NEG, None, None, "H5-after"),
    ],
    "h6_brace_shadow.cpp": [
        (2, "ID", 1, *ENUM, "scoped", None, "H6"),
        (2, "DNA", 1, *ENUM, "scoped", None, "H6"),
        (5, "ID", 1, *NEG, None, None, "H6"),
        (9, "ID", 1, *NEG, None, None, "R3"),
        (11, "DNA", 1, *USE, None, (2, "DNA", 1), "H6"),
    ],
    "h7_non_ascii.cpp": [
        (2, "café", 1, *ENUM, "unscoped", None, "H7"),
        (2, "thé", 1, *ENUM, "unscoped", None, "H7"),
        (3, "café", 1, *USE, None, (2, "café", 1), "H7"),
    ],
    "namespace_macros.cpp": [
        (4, "BEGIN_NAMESPACE", 1, *MACRO, None, None, "namespace-macro"),
        (5, "Load", 1, *ENUM, "scoped", None, "namespace-macro"),
        (5, "Run", 1, *ENUM, "scoped", None, "namespace-macro"),
        (6, "END_NAMESPACE", 1, *MACRO, None, None, "namespace-macro"),
        (7, "QT_BEGIN_NAMESPACE", 1, *MACRO, None, None, "namespace-macro"),
        (9, "QT_END_NAMESPACE", 1, *MACRO, None, None, "namespace-macro"),
        # Engine is declared only inside a macro invocation, so no syntax in
        # the blob establishes the chain; the use must not guess.
        (10, "Run", 1, *NEG, None, None, "namespace-macro"),
    ],
    "extern_c_guards.cpp": [
        (5, "FLAG_NONE", 1, *ENUM, "unscoped", None, "extern-c"),
        (5, "FLAG_READ", 1, *ENUM, "unscoped", None, "extern-c"),
        (10, "After", 1, *NS, "simple", None, "extern-c"),
        (10, "END", 1, *ENUM, "unscoped", None, "extern-c"),
        (11, "After", 1, *QUAL, None, (10, "After", 1), "extern-c"),
        (11, "END", 1, *USE, None, (10, "END", 1), "extern-c"),
        (12, "FLAG_READ", 1, *USE, None, (5, "FLAG_READ", 1), "extern-c"),
    ],
    "conflict_markers.cpp": [
        (2, "Merge", 1, *NS, "simple", None, "conflict"),
        (4, "Ours", 1, *ENUM, "scoped", None, "conflict"),
        (4, "Both", 1, *ENUM, "scoped", None, "conflict"),
        (6, "Theirs", 1, *ENUM, "scoped", None, "conflict"),
        (6, "Both", 1, *ENUM, "scoped", None, "conflict"),
        (9, "Merge", 1, *QUAL, None, (2, "Merge", 1), "conflict"),
        # Side is declared twice in the same scope: ambiguous (R5).
        (9, "Both", 1, *NEG, None, None, "R5"),
        (10, "Kept", 1, *NS, "simple", None, "conflict"),
        (10, "Done", 1, *ENUM, "scoped", None, "conflict"),
        (11, "Kept", 1, *QUAL, None, (10, "Kept", 1), "conflict"),
        (11, "Done", 1, *USE, None, (10, "Done", 1), "conflict"),
    ],
    "cpp26_reflection.cpp": [
        (3, "Reflect", 1, *NS, "simple", None, "reflection"),
        (4, "Red", 1, *ENUM, "scoped", None, "reflection"),
        (4, "Green", 1, *ENUM, "scoped", None, "reflection"),
        (13, "Reflect", 1, *QUAL, None, (3, "Reflect", 1), "reflection"),
        (13, "Red", 1, *USE, None, (4, "Red", 1), "reflection"),
    ],
}


def line_starts(data: bytes) -> list[int]:
    starts = [0]
    for index, byte in enumerate(data):
        if byte == 0x0A:
            starts.append(index + 1)
    return starts


def locate(data: bytes, starts: list[int], line: int, word: str, occurrence: int) -> tuple[int, int]:
    begin = starts[line - 1]
    end = starts[line] - 1 if line < len(starts) else len(data)
    text = data[begin:end].decode("utf-8")
    pattern = re.compile(r"(?<![\w])" + re.escape(word) + r"(?![\w])")
    matches = list(pattern.finditer(text))
    if len(matches) < occurrence:
        raise SystemExit(f"{word!r} occurrence {occurrence} not on line {line}")
    match = matches[occurrence - 1]
    offset = begin + len(text[: match.start()].encode("utf-8"))
    return offset, len(word.encode("utf-8"))


def protected_ranges(data: bytes) -> list[list[int]]:
    """Comments, string literals and whole directive lines of a gap file."""
    ranges: list[list[int]] = []
    text = data.decode("utf-8")
    offset = 0
    for line in text.split("\n"):
        encoded = line.encode("utf-8")
        stripped = line.lstrip(" \t")
        if stripped.startswith("#"):
            ranges.append([offset, offset + len(encoded)])
        else:
            for match in re.finditer(r'//.*|"(?:[^"\\]|\\.)*"', line):
                start = offset + len(line[: match.start()].encode("utf-8"))
                stop = offset + len(line[: match.end()].encode("utf-8"))
                ranges.append([start, stop])
        offset += len(encoded) + 1
    return ranges


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()
    root = arguments.repo_root.resolve()
    output = arguments.output or root / "tests/syntax_evaluation/labels/gap.jsonl"
    records: list[dict] = []
    for name in sorted(path.name for path in (root / GAP_DIRECTORY).glob("*.cpp")):
        data = (root / GAP_DIRECTORY / name).read_bytes()
        starts = line_starts(data)
        corpus_path = CORPUS_PREFIX + name
        for line, word, occurrence, category, expectation, subcategory, declaring, rule in LABELS.get(name, []):
            offset, length = locate(data, starts, line, word, occurrence)
            declaration = locate(data, starts, *declaring)[0] if declaring else None
            records.append({
                "c": category, "d": declaration, "e": expectation, "f": corpus_path, "m": "hand",
                "n": length, "o": offset, "r": rule, "s": subcategory, "t": word,
            })
        records.append({"c": "protected", "f": corpus_path, "m": "hand", "ranges": protected_ranges(data)})
    records.sort(key=lambda record: (record["f"], record.get("o", -1), record["c"]))
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8", newline="\n") as stream:
        for record in records:
            stream.write(json.dumps(record, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n")
    print(f"gap labels: {sum(1 for record in records if record['c'] != 'protected')} identifiers, "
          f"{len(LABELS)} files -> {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
