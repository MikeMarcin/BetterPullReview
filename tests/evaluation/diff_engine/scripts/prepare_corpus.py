#!/usr/bin/env python3
"""Build the diff-engine evaluation corpus ([[RFC-BPR-0082]], [[BPR-1466]]).

Writes every comparison of the evaluation as two files and a manifest under an
output directory in the build tree. Nothing it writes is committed. Sources:

* committed fixtures: large-review-v1's 20,000-line comparison and generated
  table (BPR-0625), extracted from the stored pack with an explicit Git, and
  google/UIforETW#74's recorded base and head contents;
* the two pinned Git source archives the evaluation build extracts: real files
  changed between Git 2.39.5 and 2.56.0;
* deterministic generators: the BPR-1366 benchmark sizes, the BPR-0719 replica
  and the RFC's pathological classes.

GitHub oracle hunks come from the committed recording (diff-oracle-v1) and
from UIforETW#74's recorded patches. A pair whose regenerated head blob does
not match the recording's blob identity is reported and gets no oracle.

Usage:
  prepare_corpus.py --repo ROOT --stage BUILD/_bpr/diff-evaluation \
      --git /absolute/git --output BUILD/diff-evaluation-corpus
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import pathlib
import re
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[4]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

MASK64 = (1 << 64) - 1
SIZE_LIMIT_S = 2_000
SIZE_LIMIT_M = 10_000


class XorShift:
    """The xorshift generator the C++ benchmark and tests use."""

    def __init__(self, state: int) -> None:
        self.state = state & MASK64

    def below(self, bound: int) -> int:
        state = self.state
        state ^= (state << 13) & MASK64
        state ^= state >> 7
        state ^= (state << 17) & MASK64
        self.state = state
        return state % bound


def join(lines: list[str], ending: str = "\n") -> bytes:
    return "".join(line + ending for line in lines).encode("utf-8")


def benchmark_pair(lines: int, edited_permille: int) -> tuple[bytes, bytes]:
    """tests/core/diff_compute_benchmark.cpp's synthetic_pair, byte for byte."""
    random = XorShift(7 * 0x9E3779B97F4A7C15 + 1)

    def line(ordinal: int) -> str:
        kind = random.below(12)
        if kind == 0:
            return ""
        if kind == 1:
            return "    }"
        first = random.below(100_000)
        second = random.below(1_000)
        third = random.below(50_000)
        return f"    auto value_{ordinal} = compute({first}, item{second}) + token{third};"

    left = [line(ordinal) for ordinal in range(lines)]
    right: list[str] = []
    moved: list[str] = []
    ordinal = 0
    while ordinal < lines:
        roll = random.below(1_000)
        if roll < 2 and ordinal + 30 < lines:
            moved.extend(left[ordinal:ordinal + 20])
            ordinal += 20
            continue
        if roll < 12:
            ordinal += 1
            continue
        if roll < 22:
            right.append(line(lines + ordinal))
        if roll >= 1_000 - edited_permille:
            edited = left[ordinal]
            if not edited:
                edited = "  // note"
            else:
                middle = len(edited.encode("utf-8")) // 2
                edited = edited[:middle] + "x_" + str(random.below(9)) + edited[middle:]
            right.append(edited)
        else:
            right.append(left[ordinal])
        if moved and random.below(1_000) < 5:
            right.extend(moved)
            moved = []
        ordinal += 1
    right.extend(moved)
    return join(left), join(right)


def bpr0719_replica() -> tuple[bytes, bytes]:
    """test_large_files_degrade_instead_of_refusing's insertion pair."""
    cycle = ["}", "", "    VERIFY_IS_TRUE(result);", "    auto& buffer = si.GetTextBuffer();", "    {"]
    left = [cycle[line % 5] for line in range(8000)]
    inserted = [cycle[line % 5] for line in range(291)]
    right = left[:4000] + inserted + left[4000:]
    return join(left), join(right)


def pathological_pairs() -> list[tuple[str, bytes, bytes]]:
    pairs: list[tuple[str, bytes, bytes]] = []
    random = XorShift(0x5EED_0082)

    # All-identical lines with a few inserted and deleted lines.
    left = ["same line"] * 20_000
    right = list(left)
    for position in range(500, 20_000, 1_000):
        right[position] = "a different line " + str(position)
    for position in range(19_000, 0, -2_500):
        right.insert(position, "inserted " + str(position))
    pairs.append(("identical-lines", join(left), join(right)))

    # Highly repetitive source with scattered edits.
    cycle = ["    {", "        value += step;", "        check(value);", "    }", ""]
    left = [cycle[line % 5] for line in range(10_000)]
    right = list(left)
    for position in range(37, 10_000, 211):
        right[position] = right[position].replace("step", "stride") or "    // blank"
    for position in range(9_900, 0, -997):
        right[position:position] = ["        log(value);"]
    pairs.append(("repetitive", join(left), join(right)))

    # Phase-shifted repeats: the BPR-0719 ambiguous region, larger.
    left = ["left only opening line"] + [cycle[line % 5] for line in range(12_000)] + ["left only closing line"]
    right = ["right only opening line"] + [cycle[(line + 2) % 5] for line in range(12_000)] + ["right only closing line"]
    pairs.append(("phase-shifted", join(left), join(right)))

    # One huge replacement: every line differs.
    left = [f"old line {line} {random.below(1_000_000)}" for line in range(6_000)]
    right = [f"new line {line} {random.below(1_000_000)}" for line in range(6_000)]
    pairs.append(("huge-replacement", join(left), join(right)))

    # Long lines of more than 10,000 scalars with small edits.
    left = []
    for line in range(40):
        words = [f"w{random.below(100_000)}" for _ in range(1_800)]
        left.append(" ".join(words))
    right = []
    for line, text in enumerate(left):
        if line % 3 == 0:
            middle = len(text) // 2
            right.append(text[:middle] + " changed" + text[middle:])
        elif line % 7 == 1:
            right.append(text.replace("w1", "W1", 25))
        else:
            right.append(text)
    pairs.append(("long-lines", join(left), join(right)))

    # Moved blocks: forty blocks of a hundred lines, reordered.
    blocks = [[f"block {block} line {line} {random.below(10_000)}" for line in range(100)] for block in range(40)]
    order = list(range(40))
    for index in range(39, 0, -1):
        swap = random.below(index + 1)
        order[index], order[swap] = order[swap], order[index]
    left = [line for block in blocks for line in block]
    right = [line for block in order for line in blocks[block]]
    pairs.append(("moved-blocks", join(left), join(right)))

    # Whitespace-only edits: indentation, tabs and trailing spaces.
    base = [f"    statement_{line}({line % 17});" if line % 9 else "" for line in range(3_000)]
    left = list(base)
    right = []
    for line, text in enumerate(base):
        if line % 11 == 0 and text:
            right.append("\t" + text.lstrip())
        elif line % 13 == 0:
            right.append(text + "   ")
        elif line % 17 == 0 and text:
            right.append(text.replace("(", " ( "))
        else:
            right.append(text)
    pairs.append(("whitespace-only", join(left), join(right)))

    # Unicode: several scripts, emoji, combining marks, with edits.
    samples = ["café crème", "日本語のテキスト", "مرحبا بالعالم",
               "été näive", "\U0001F600 \U0001F680 \U0001F469‍\U0001F4BB", "Жизнь прекрасна"]
    left = [f"{samples[line % len(samples)]} {line}" for line in range(1_200)]
    right = [text.replace("é", "e").replace("\U0001F600", "\U0001F601") if line % 5 == 0 else text for line, text in enumerate(left)]
    pairs.append(("unicode", join(left), join(right)))

    # CRLF: the same text with some lines' endings changed.
    text = [f"line {line} of a windows file" for line in range(2_000)]
    left_bytes = join(text, "\r\n")
    right_lines = [line + ("\n" if index % 7 == 0 else "\r\n") for index, line in enumerate(text)]
    right_lines[1000] = "line 1000 was edited\r\n"
    pairs.append(("crlf", left_bytes, "".join(right_lines).encode("utf-8")))

    # Lone CR: a CR inside a line is content; only LF ends a line.
    left = [f"field {line}\rvalue {line}" if line % 4 == 0 else f"record {line}" for line in range(1_000)]
    right = list(left)
    for position in range(2, 1_000, 50):
        right[position] = right[position] + "\rappended"
    right[500] = "record 500 edited"
    pairs.append(("lone-cr", join(left), join(right)))
    return pairs


def run(command: list[str], **kwargs) -> subprocess.CompletedProcess:
    return subprocess.run(command, check=True, capture_output=True, **kwargs)


def large_review_pairs(repo: pathlib.Path, git: str) -> list[tuple[str, bytes, bytes]]:
    fixture = repo / "tests/fixtures/github/pull-requests/large-review-v1"
    manifest = json.loads((fixture / "manifest.json").read_text())
    wanted = {"large/comparison.cpp": "bpr0625-comparison", "generated/table.inc": "bpr0625-generated-table"}
    environment = {"GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": "/dev/null", "HOME": "/nonexistent", "LC_ALL": "C", "PATH": "/usr/bin:/bin"}
    pairs = []
    with contained_temporary_directory(prefix="bpr-diff-eval-pack-") as temporary:
        repository = pathlib.Path(temporary) / "objects.git"
        run([git, "init", "--bare", "--quiet", "--template=", str(repository)], env=environment)
        run([git, "--git-dir", str(repository), "unpack-objects", "-q"], input=(fixture / "objects.pack").read_bytes(), env=environment)
        for entry in manifest["files"]:
            if entry["path"] not in wanted:
                continue
            sides = []
            for side in ("base", "head"):
                blob = run([git, "--git-dir", str(repository), "cat-file", "blob", entry[side]["oid"]], env=environment).stdout
                if blob_sha1(blob) != entry[side]["oid"]:
                    raise SystemExit(f"large-review-v1 blob mismatch for {entry['path']}")
                sides.append(blob)
            pairs.append((wanted[entry["path"]], sides[0], sides[1]))
    return sorted(pairs)


def uiforetw_pairs(repo: pathlib.Path) -> list[tuple[str, bytes, bytes, str, str, str]]:
    recording = json.loads((repo / "tests/fixtures/github/pull-requests/uiforetw-74/recording.json").read_text())
    pairs = []
    for entry in recording["files"]:
        left = base64.b64decode(entry["base_content_base64"])
        right = base64.b64decode(entry["head_content_base64"])
        name = "uiforetw74-" + re.sub(r"[^A-Za-z0-9]+", "-", entry["filename"]).strip("-").lower()
        pairs.append((name, left, right, entry["patch"], entry["sha"], entry["filename"]))
    return pairs


def git_source_pairs(stage: pathlib.Path) -> list[tuple[str, bytes, bytes, str]]:
    """Real files changed between the two pinned Git releases."""
    old_root = stage / "git-2.39.5/git-2.39.5"
    new_root = stage / "git-2.56.0/git-2.56.0"
    buckets: dict[str, list[tuple[str, bytes, bytes]]] = {"S": [], "M": [], "L": []}
    patterns = ("*.c", "*.h", "builtin/*.c", "refs/*.c", "t/t[0-9]*.sh", "po/*.po")
    seen = set()
    for pattern in patterns:
        for old in sorted(old_root.glob(pattern)):
            relative = old.relative_to(old_root).as_posix()
            new = new_root / relative
            if relative in seen or not new.is_file():
                continue
            seen.add(relative)
            left, right = old.read_bytes(), new.read_bytes()
            if left == right or b"\0" in left or b"\0" in right:
                continue
            lines = max(left.count(b"\n"), right.count(b"\n"))
            bucket = "S" if lines < SIZE_LIMIT_S else "M" if lines <= SIZE_LIMIT_M else "L"
            buckets[bucket].append((relative, left, right))
    quotas = {"S": 30, "M": 14, "L": 4}
    pairs = []
    for bucket, entries in buckets.items():
        step = max(1, len(entries) // quotas[bucket])
        for relative, left, right in entries[::step][:quotas[bucket]]:
            name = "git-" + re.sub(r"[^A-Za-z0-9]+", "-", relative).strip("-").lower()
            pairs.append((name, left, right, relative))
    return pairs


def blob_sha1(data: bytes) -> str:
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def size_class(left: bytes, right: bytes) -> str:
    lines = max(left.count(b"\n"), right.count(b"\n"))
    return "S" if lines < SIZE_LIMIT_S else "M" if lines <= SIZE_LIMIT_M else "L"


HUNK = re.compile(rb"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@", re.M)


def patch_hunks(patch: str) -> list[list[int]]:
    """Each `@@ -a,b +c,d @@` header as [a, b, c, d], one-based as written."""
    result = []
    for match in HUNK.finditer(patch.encode("utf-8")):
        a, b, c, d = match.groups()
        result.append([int(a), int(b) if b is not None else 1, int(c), int(d) if d is not None else 1])
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=pathlib.Path, required=True)
    parser.add_argument("--stage", type=pathlib.Path, required=True)
    parser.add_argument("--git", required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    arguments = parser.parse_args()
    if not pathlib.Path(arguments.git).is_absolute():
        raise SystemExit("--git must be an absolute path")

    output = arguments.output
    (output / "pairs").mkdir(parents=True, exist_ok=True)
    oracle_path = arguments.repo / "tests/fixtures/github/diff-oracle-v1/oracle.json"
    recorded = json.loads(oracle_path.read_text()) if oracle_path.is_file() else {"files": []}
    recorded_by_id = {entry["id"]: entry for entry in recorded["files"]}

    pairs: list[dict] = []
    mismatches: list[str] = []

    def add(identifier: str, category: str, origin: str, left: bytes, right: bytes, *, path: str | None = None,
            oracle: dict | None = None, options: tuple[str, ...] = ("default",)) -> None:
        extension = pathlib.PurePosixPath(path).suffix if path else ".txt"
        left_name = f"pairs/{identifier}.left{extension}"
        right_name = f"pairs/{identifier}.right{extension}"
        (output / left_name).write_bytes(left)
        (output / right_name).write_bytes(right)
        if oracle is None and identifier in recorded_by_id:
            entry = recorded_by_id[identifier]
            if entry.get("head_sha") == blob_sha1(right) and entry.get("base_sha") == blob_sha1(left):
                if entry.get("hunks") is not None:
                    oracle = {"kind": "github", "source": recorded.get("source", "diff-oracle-v1"), "hunks": entry["hunks"]}
            else:
                mismatches.append(identifier)
        for option in options:
            pairs.append({
                "id": identifier if option == "default" else f"{identifier}@{option}",
                "class": category,
                "size": size_class(left, right),
                "origin": origin,
                "path": path or f"{identifier}.txt",
                "left": left_name,
                "right": right_name,
                "left_lines": left.count(b"\n") + (0 if left.endswith(b"\n") or not left else 1),
                "right_lines": right.count(b"\n") + (0 if right.endswith(b"\n") or not right else 1),
                "options": option,
                "oracle": oracle if option == "default" else None,
                "left_sha": blob_sha1(left),
                "right_sha": blob_sha1(right),
            })

    for lines in (200, 800, 2_000, 10_000, 40_000, 100_000):
        for edited in (30, 300):
            left, right = benchmark_pair(lines, edited)
            add(f"bench-{lines}-{edited}", size_class(left, right), "BPR-1366 benchmark generator", left, right)
    left, right = bpr0719_replica()
    add("bpr0719-replica", "real", "BPR-0719 replica (test_large_files_degrade_instead_of_refusing)", left, right, path="ScreenBufferTests.cpp")
    for identifier, left, right in large_review_pairs(arguments.repo, arguments.git):
        add(identifier, "real", "large-review-v1 (BPR-0625)", left, right, path="comparison.cpp" if "comparison" in identifier else "table.inc")
    for identifier, left, right, patch, head_sha, filename in uiforetw_pairs(arguments.repo):
        oracle = {"kind": "github", "source": "uiforetw-74 recording", "hunks": patch_hunks(patch)} if head_sha == blob_sha1(right) else None
        if oracle is None:
            mismatches.append(identifier)
        add(identifier, "real", "google/UIforETW#74 recording", left, right, path=filename, oracle=oracle)
    for identifier, left, right, relative in git_source_pairs(arguments.stage):
        add(identifier, "real", f"Git 2.39.5 to 2.56.0: {relative}", left, right, path=relative)
    whitespace_variants = ("default", "ignore-trailing", "ignore-all", "normalize-endings", "ignore-all+normalize-endings")
    for identifier, left, right in pathological_pairs():
        variants = whitespace_variants if identifier in ("whitespace-only", "crlf") else ("default",)
        add(f"patho-{identifier}", "pathological", "generated pathological class", left, right, options=variants)

    manifest = {"schema": "bpr.diff-evaluation.corpus.v1", "pairs": pairs, "oracle_mismatches": mismatches}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=1, sort_keys=True) + "\n")
    print(f"CORPUS_PAIRS {len(pairs)} ORACLE {sum(1 for pair in pairs if pair['oracle'])} MISMATCHES {len(mismatches)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
