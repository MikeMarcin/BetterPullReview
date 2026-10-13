#!/usr/bin/env python3
"""Write seeded, stratified hand-review sheets for RFC-BPR-0081 labels.

For each third-party source and for fragments, draw up to N machine labels
per category, plus N identifier-like words from inside protected ranges.
Each sheet item shows the word in context but not the machine label; a
separate key file maps each item id to the machine label so reviewers can
label blind and an adjudicator can compare. Sheets and keys are review
artifacts for a scratch directory, never committed.

Usage:
    python3 -I sample_for_review.py --corpus DIR --labels LABELS_DIR --out SCRATCH
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import random
import re
import sys
from collections import defaultdict
from pathlib import Path

SOURCES = ("abseil", "re2", "glaze", "quill", "ksyntaxhighlighting", "fragments")
WORD = re.compile(rb"[A-Za-z_][A-Za-z0-9_]*")
CONTEXT_LINES = 6
MASTER_SEED = 81


def seed_for(*parts: str) -> int:
    text = "\x1f".join([str(MASTER_SEED), "review", *parts]).encode()
    return int.from_bytes(hashlib.sha256(text).digest()[:8], "big")


def read_labels(labels: Path, source: str) -> list[dict]:
    plain = labels / f"{source}.jsonl"
    packed = labels / f"{source}.jsonl.gz"
    if plain.exists():
        text = plain.read_text(encoding="utf-8")
    elif packed.exists():
        text = gzip.decompress(packed.read_bytes()).decode("utf-8")
    else:
        return []
    return [json.loads(line) for line in text.splitlines() if line]


class Files:
    def __init__(self, corpus: Path) -> None:
        self.corpus = corpus
        self.cache: dict[str, bytes] = {}

    def data(self, relative: str) -> bytes:
        if relative not in self.cache:
            self.cache[relative] = (self.corpus / relative).read_bytes()
        return self.cache[relative]


def item_for(files: Files, relative: str, offset: int, length: int) -> dict:
    data = files.data(relative)
    line_start = data.rfind(b"\n", 0, offset) + 1
    line = data.count(b"\n", 0, offset) + 1
    column = len(data[line_start:offset].decode("utf-8", "replace")) + 1
    first = line_start
    for _ in range(CONTEXT_LINES):
        if first == 0:
            break
        first = data.rfind(b"\n", 0, first - 1) + 1
    last = offset + length
    for _ in range(CONTEXT_LINES + 1):
        following = data.find(b"\n", last)
        if following < 0:
            last = len(data)
            break
        last = following + 1
    target = data[offset : offset + length]
    context = data[first:offset] + "⟦".encode() + target + "⟧".encode() + data[offset + length : last]
    return {"f": relative, "line": line, "col": column, "t": target.decode("utf-8", "replace"), "context": context.decode("utf-8", "replace")}


def protected_words(records: list[dict], files: Files, rng: random.Random, count: int) -> list[tuple[str, int, int]]:
    pool = [(record["f"], start, end) for record in records if record.get("c") == "protected" for start, end in record["ranges"]]
    rng.shuffle(pool)
    chosen: list[tuple[str, int, int]] = []
    seen: set[tuple[str, int]] = set()
    for relative, start, end in pool:
        if len(chosen) >= count:
            break
        words = list(WORD.finditer(files.data(relative), start, end))
        if not words:
            continue
        word = words[rng.randrange(len(words))]
        key = (relative, word.start())
        if key in seen:
            continue
        seen.add(key)
        chosen.append((relative, word.start(), word.end() - word.start()))
    return chosen


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--corpus", required=True, type=Path)
    parser.add_argument("--labels", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--per-category", type=int, default=50)
    arguments = parser.parse_args()
    files = Files(arguments.corpus)
    (arguments.out / "sheets").mkdir(parents=True, exist_ok=True)
    (arguments.out / "keys").mkdir(parents=True, exist_ok=True)
    for source in SOURCES:
        records = read_labels(arguments.labels, source)
        if not records:
            print(f"{source}: no labels", file=sys.stderr)
            continue
        by_category: dict[str, list[dict]] = defaultdict(list)
        for record in records:
            if record.get("c") != "protected":
                by_category[record["c"]].append(record)
        drawn: list[tuple[dict, dict]] = []
        for category in sorted(by_category):
            rng = random.Random(seed_for(source, category))
            labels = by_category[category]
            for label in rng.sample(labels, min(arguments.per_category, len(labels))):
                drawn.append((item_for(files, label["f"], label["o"], label["n"]), label))
        rng = random.Random(seed_for(source, "protected"))
        for relative, offset, length in protected_words(records, files, rng, arguments.per_category):
            item = item_for(files, relative, offset, length)
            drawn.append((item, {"f": relative, "o": offset, "n": length, "t": item["t"], "c": "protected", "e": "protected"}))
        drawn.sort(key=lambda pair: (pair[0]["f"], pair[1]["o"]))
        random.Random(seed_for(source, "order")).shuffle(drawn)
        sheet_lines = []
        keys = {}
        for number, (item, label) in enumerate(drawn, 1):
            identifier = f"{source}-{number:04d}"
            sheet_lines.append(json.dumps({"id": identifier, **item}, ensure_ascii=False, sort_keys=True))
            keys[identifier] = label
        (arguments.out / "sheets" / f"{source}.jsonl").write_text("\n".join(sheet_lines) + "\n", encoding="utf-8")
        (arguments.out / "keys" / f"{source}.keys.json").write_text(json.dumps(keys, ensure_ascii=False, sort_keys=True, indent=1) + "\n", encoding="utf-8")
        counts = defaultdict(int)
        for _, label in drawn:
            counts[label["c"]] += 1
        print(f"{source}: {len(drawn)} items {dict(sorted(counts.items()))}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
