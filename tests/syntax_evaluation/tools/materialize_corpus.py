#!/usr/bin/env python3
"""Materialize the RFC-BPR-0081 C++ identifier evaluation corpus.

Writes a deterministic tree and DIR/index.json from the committed manifest
(tests/syntax_evaluation/corpus/manifest.json). Re-running with the same
inputs produces byte-identical output. Nothing here touches the network:
third-party sources come only from archives already pinned in the CPM cache,
each verified by SHA-256 and byte size before a byte is extracted.

Usage:
    python3 -I materialize_corpus.py --repo-root ROOT --cpm-cache CACHE --out DIR
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
import random
import re
import subprocess
import sys
import tarfile
from dataclasses import dataclass, field
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "scripts"))
from contained_temporary_support import contained_temporary_directory  # noqa: E402

MANIFEST_RELATIVE = Path("tests/syntax_evaluation/corpus/manifest.json")
SUPPORT_SUFFIXES = {".inc", ".def"}


# ---------------------------------------------------------------- utilities


def fail(message: str) -> None:
    raise SystemExit(f"materialize_corpus: {message}")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def derived_seed(master: int, *parts: str) -> int:
    text = "\x1f".join([str(master), *parts]).encode()
    return int.from_bytes(hashlib.sha256(text).digest()[:8], "big")


def is_cpp_path(path: str, extensions: list[str]) -> bool:
    name = path.rsplit("/", 1)[-1]
    if "." not in name:
        return False
    suffix = "." + name.rsplit(".", 1)[-1]
    if suffix == ".c":
        return False
    lowered = {extension.lower() for extension in extensions}
    return suffix.lower() in lowered


def size_class(size: int) -> str:
    if size < 64 * 1024:
        return "S"
    if size < 1024 * 1024:
        return "M"
    return "L"


def is_utf8(data: bytes) -> bool:
    try:
        data.decode("utf-8")
    except UnicodeDecodeError:
        return False
    return True


def split_lines(data: bytes) -> list[bytes]:
    """Lines as core display_lines counts them: only LF ends a line.

    Each element keeps its ending. The empty line after a final LF is not a line.
    """
    lines = data.split(b"\n")
    result = [line + b"\n" for line in lines[:-1]]
    if lines[-1]:
        result.append(lines[-1])
    return result


def row_text(line: bytes) -> bytes:
    """A share-sheet row: the line without LF or CRLF."""
    if line.endswith(b"\r\n"):
        return line[:-2]
    if line.endswith(b"\n"):
        return line[:-1]
    return line


def expand_tabs(text: str, width: int) -> str:
    """note_code_view's expand_tabs: columns counted in UTF-16 units."""
    if "\t" not in text:
        return text
    out: list[str] = []
    column = 0
    for character in text:
        if character == "\t":
            pad = width - column % width
            out.append(" " * pad)
            column += pad
        else:
            out.append(character)
            column += 2 if ord(character) > 0xFFFF else 1
    return "".join(out)


@dataclass
class CorpusFile:
    path: str  # relative to DIR, POSIX separators
    data: bytes
    cls: str
    source: str
    parent: str | None = None
    seed: int | None = None
    extra: dict = field(default_factory=dict)
    size_class: str | None = None


class Corpus:
    def __init__(self) -> None:
        self.files: dict[str, CorpusFile] = {}
        # Non-C++ files a third-party C++ file includes (.inc, .def): written
        # under support/ for the labeller's include path, never corpus files.
        self.support: dict[str, bytes] = {}

    def add(self, item: CorpusFile) -> None:
        if item.path in self.files:
            fail(f"duplicate corpus path {item.path}")
        if ".." in item.path.split("/") or item.path.startswith("/"):
            fail(f"unsafe corpus path {item.path}")
        self.files[item.path] = item

    def of_class(self, cls: str) -> list[CorpusFile]:
        return [self.files[path] for path in sorted(self.files) if self.files[path].cls == cls]


# ---------------------------------------------------------------- class 1


def run_git(arguments: list[str], cwd: Path | None = None, data: bytes | None = None) -> bytes:
    environment = dict(os.environ)
    environment["GIT_CONFIG_NOSYSTEM"] = "1"
    environment["GIT_TERMINAL_PROMPT"] = "0"
    completed = subprocess.run(["git", *arguments], cwd=cwd, input=data, capture_output=True, env=environment, check=False)
    if completed.returncode != 0:
        fail(f"git {' '.join(arguments)} failed: {completed.stderr.decode(errors='replace').strip()}")
    return completed.stdout


def pack_blobs(pack: Path, extensions: list[str]) -> list[tuple[str, str, bytes]]:
    """(blob id, path, bytes) for every C++ path of every commit in a pack."""
    with contained_temporary_directory(prefix="bpr-corpus-pack-") as scratch:
        repository = Path(scratch) / "pack.git"
        run_git(["init", "--bare", "--quiet", str(repository)])
        target = repository / "objects" / "pack" / "fixture.pack"
        target.write_bytes(pack.read_bytes())
        run_git(["index-pack", str(target)], cwd=repository)
        listing = run_git(["cat-file", "--batch-all-objects", "--batch-check"], cwd=repository).decode()
        commits = sorted(line.split()[0] for line in listing.splitlines() if line.split()[1] == "commit")
        found: dict[str, tuple[str, str]] = {}
        for commit in commits:
            tree = run_git(["ls-tree", "-r", "-z", commit], cwd=repository).decode("utf-8", "surrogateescape")
            for entry in tree.split("\0"):
                if not entry:
                    continue
                meta, path = entry.split("\t", 1)
                mode, kind, blob = meta.split()
                if kind != "blob" or mode not in ("100644", "100755") or not is_cpp_path(path, extensions):
                    continue
                if blob not in found or path < found[blob][0]:
                    found[blob] = (path, commit)
        result = []
        for blob in sorted(found):
            data = run_git(["cat-file", "blob", blob], cwd=repository)
            result.append((blob, found[blob][0], data))
        return result


def patch_head_text(patch: str) -> str:
    out: list[str] = []
    for line in patch.split("\n"):
        if line.startswith("@@") or line.startswith("\\"):
            continue
        if line.startswith("+") or line.startswith(" "):
            out.append(line[1:])
        elif line == "":
            continue
    return "\n".join(out) + "\n"


def add_contract(corpus: Corpus, root: Path, spec: dict, extensions: list[str]) -> None:
    fixtures = sorted((root / "tests/fixtures/syntax").glob("*.cpp"))
    for fixture in fixtures:
        corpus.add(CorpusFile(path=f"contract/fixtures/{fixture.name}", data=fixture.read_bytes(), cls="contract", source="contract", extra={"origin": f"tests/fixtures/syntax/{fixture.name}"}))
    seen: set[str] = set()
    for pack_relative in spec["packs"]:
        pack = root / pack_relative
        name = pack.parent.name
        for blob, path, data in pack_blobs(pack, extensions):
            if blob in seen:
                continue
            seen.add(blob)
            corpus.add(CorpusFile(path=f"contract/{name}/{blob}/{path}", data=data, cls="contract", source="contract", extra={"origin": f"{name}:{path}", "blob": blob}))
    for recording in spec["recordings"]:
        record_path = root / recording["path"]
        record = json.loads(record_path.read_text(encoding="utf-8"))
        name = record_path.parent.name
        for entry in sorted(record["files"], key=lambda item: item["filename"]):
            if not is_cpp_path(entry["filename"], extensions) or "patch" not in entry:
                continue
            text = patch_head_text(entry["patch"])
            corpus.add(CorpusFile(path=f"contract/{name}/{entry['filename']}", data=text.encode("utf-8"), cls="contract", source="contract", extra={"origin": f"{name}:{entry['filename']}", "reconstructed": "patch head side"}))


# ---------------------------------------------------------------- class 2


def safe_members(archive: tarfile.TarFile):
    for member in archive.getmembers():
        name = member.name
        if name.startswith("/") or ".." in name.split("/"):
            fail(f"unsafe archive member {name}")
        if member.isfile():
            yield member


def add_first_party(corpus: Corpus, root: Path, spec: dict, extensions: list[str]) -> None:
    data = run_git(["archive", "--format=tar", spec["commit"], spec["tree"]], cwd=root)
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:") as archive:
        for member in sorted(safe_members(archive), key=lambda item: item.name):
            if not is_cpp_path(member.name, extensions):
                continue
            stream = archive.extractfile(member)
            assert stream is not None
            corpus.add(CorpusFile(path=f"first-party/{member.name}", data=stream.read(), cls="first-party", source="first-party"))


# ---------------------------------------------------------------- class 3


def add_archive(corpus: Corpus, cache: Path, spec: dict, extensions: list[str]) -> dict:
    archive_path = cache / spec["file"]
    if not archive_path.is_file() or archive_path.is_symlink():
        fail(f"pinned archive missing: {archive_path}")
    size = archive_path.stat().st_size
    if size != spec["bytes"]:
        fail(f"{spec['file']}: {size} bytes, manifest pins {spec['bytes']}")
    digest = sha256_file(archive_path)
    if digest != spec["sha256"]:
        fail(f"{spec['file']}: SHA-256 {digest}, manifest pins {spec['sha256']}")
    source = spec["label_source"]
    count = 0
    with tarfile.open(archive_path, mode="r:gz") as archive:
        for member in sorted(safe_members(archive), key=lambda item: item.name):
            parts = member.name.split("/", 1)
            if len(parts) != 2:
                continue
            if is_cpp_path(parts[1], extensions):
                stream = archive.extractfile(member)
                assert stream is not None
                corpus.add(CorpusFile(path=f"third-party/{source}/{parts[1]}", data=stream.read(), cls="third-party", source=source))
                count += 1
            elif suffix_of(parts[1]).lower() in SUPPORT_SUFFIXES:
                stream = archive.extractfile(member)
                assert stream is not None
                corpus.support[f"support/{source}/{parts[1]}"] = stream.read()
    return {"label_source": source, "sha256": digest, "bytes": size, "files": count}


def source_tree_digest(directory: Path) -> str:
    """bpr_syntax_source_digest from cmake/BprSyntaxHighlightingProvider.cmake."""
    relatives: list[str] = []
    for current, directories, names in os.walk(directory):
        directories.sort()
        for name in names:
            path = Path(current) / name
            if path.is_symlink():
                fail(f"verified source must not contain a symbolic link: {path}")
            relatives.append(path.relative_to(directory).as_posix())
    relatives.sort()
    manifest = "".join(f"{sha256_file(directory / relative)}  {relative}\n" for relative in relatives)
    return sha256_bytes(manifest.encode("utf-8"))


def add_source_tree(corpus: Corpus, cache: Path, spec: dict, extensions: list[str]) -> dict:
    base = cache / spec["cache_directory"]
    chosen: Path | None = None
    for candidate in sorted(path.parent for path in base.glob("*/CMakeLists.txt")):
        if source_tree_digest(candidate) == spec["source_manifest_sha256"]:
            chosen = candidate
            break
    if chosen is None:
        fail(f"no verified {spec['name']} source under {base}")
    source = spec["label_source"]
    count = 0
    for current, directories, names in os.walk(chosen):
        directories.sort()
        for name in sorted(names):
            path = Path(current) / name
            relative = path.relative_to(chosen).as_posix()
            if not is_cpp_path(relative, extensions):
                continue
            corpus.add(CorpusFile(path=f"third-party/{source}/{relative}", data=path.read_bytes(), cls="third-party", source=source))
            count += 1
    return {"label_source": source, "source_manifest_sha256": spec["source_manifest_sha256"], "archive_sha256": spec["archive_sha256"], "files": count}


# ---------------------------------------------------------------- class 4


def add_gap(corpus: Corpus, root: Path, spec: dict) -> None:
    directory = root / Path(spec["glob"]).parent
    if not directory.is_dir():
        return
    for path in sorted(directory.glob(Path(spec["glob"]).name)):
        if path.is_file() and not path.is_symlink():
            corpus.add(CorpusFile(path=f"gap/{path.name}", data=path.read_bytes(), cls="gap", source="gap"))


# ---------------------------------------------------------------- class 5


def eligible_parents(corpus: Corpus) -> list[CorpusFile]:
    result = []
    for path in sorted(corpus.files):
        item = corpus.files[path]
        if item.cls not in ("contract", "first-party", "third-party") or item.source == "first-party-concat":
            continue
        if not is_utf8(item.data) or len(split_lines(item.data)) < 30:
            continue
        result.append(item)
    return result


def suffix_of(path: str) -> str:
    name = path.rsplit("/", 1)[-1]
    return "." + name.rsplit(".", 1)[-1] if "." in name else ".cpp"


def excerpt(parent: CorpusFile, rng: random.Random, spec: dict) -> tuple[bytes, list[int | None], list[list[int]]]:
    lines = split_lines(parent.data)
    count = len(lines)
    context = spec["context_lines"]
    anchors = rng.randint(*spec["anchors_per_excerpt"])
    parts = []
    for _ in range(anchors):
        length = rng.randint(*spec["anchor_lines"])
        first = rng.randrange(0, max(1, count - length + 1))
        parts.append((first, min(count, first + length)))
    parts.sort()
    windows: list[list[int]] = []
    for first, last in parts:
        begin = max(0, first - context)
        stop = min(count, last + context)
        if windows and begin <= windows[-1][1] + 1:
            windows[-1][1] = max(windows[-1][1], stop)
        else:
            windows.append([begin, stop])
    out: list[bytes] = []
    shown: list[int | None] = []
    rows = 0
    for index, (begin, stop) in enumerate(windows):
        if index > 0:
            out.append(b"\n")
            shown.append(None)
            rows += 1
        for line in range(begin, stop):
            if rows >= spec["row_limit"]:
                break
            text = row_text(lines[line]).decode("utf-8")
            out.append(expand_tabs(text, spec["tab_width"]).encode("utf-8") + b"\n")
            shown.append(line + 1)
            rows += 1
    return b"".join(out), shown, [[first + 1, last] for first, last in parts]


ENUM_OPEN = re.compile(rb"^\s*(?:typedef\s+)?enum\b[^;{}()]*\{\s*(?://.*)?$")
NAMESPACE_OPEN = re.compile(rb"^\s*(?:inline\s+)?namespace\b[^;{}()=]*\{\s*(?://.*)?$")
RAW_OPEN = re.compile(rb'R"([^()\\\s"]{0,16})\(')


def matching_close_line(lines: list[bytes], opener: int) -> int | None:
    depth = 0
    for index in range(opener, len(lines)):
        stripped = lines[index].lstrip()
        if stripped.startswith(b"//") or stripped.startswith(b"#"):
            continue
        for byte in lines[index]:
            if byte == ord("{"):
                depth += 1
            elif byte == ord("}"):
                depth -= 1
                if depth == 0:
                    return index
    return None


def construct_starts(parent: CorpusFile, kind: str) -> list[int]:
    """0-based line indexes strictly inside the construct, found by text search."""
    lines = split_lines(parent.data)
    starts: list[int] = []
    for index, line in enumerate(lines):
        if kind in ("enum-body", "namespace-body"):
            pattern = ENUM_OPEN if kind == "enum-body" else NAMESPACE_OPEN
            if not pattern.match(line):
                continue
            close = matching_close_line(lines, index)
            if close is not None and close - index >= 2:
                starts.append(index + 1 + (close - index - 2) // 3)
        elif kind == "block-comment":
            at = line.find(b"/*")
            if at < 0 or b"*/" in line[at + 2 :] or b"//" in line[:at] or b'"' in line[:at]:
                continue
            for later in range(index + 1, min(len(lines), index + 400)):
                if b"*/" in lines[later]:
                    if later - index >= 2:
                        starts.append(index + 1 + (later - index - 2) // 2)
                    break
        elif kind == "raw-string":
            match = RAW_OPEN.search(line)
            if match is None:
                continue
            closer = b")" + match.group(1) + b'"'
            if closer in line[match.end() :]:
                continue
            for later in range(index + 1, min(len(lines), index + 2000)):
                if closer in lines[later]:
                    if later - index >= 2:
                        starts.append(index + 1 + (later - index - 2) // 2)
                    break
    return starts


def add_fragments(corpus: Corpus, master: int, spec: dict) -> None:
    parents = eligible_parents(corpus)
    by_class: dict[str, list[CorpusFile]] = {}
    for parent in parents:
        by_class.setdefault(parent.cls, []).append(parent)
    excerpt_spec = spec["excerpts"]
    for cls in ("contract", "first-party", "third-party"):
        pool = by_class.get(cls, [])
        if not pool:
            continue
        for number in range(excerpt_spec["counts"][cls]):
            seed = derived_seed(master, "fragment-excerpt", cls, str(number))
            rng = random.Random(seed)
            parent = pool[rng.randrange(len(pool))]
            data, shown, anchors = excerpt(parent, rng, excerpt_spec)
            path = f"fragments/excerpt/{cls}-{number:03d}{suffix_of(parent.path)}"
            corpus.add(CorpusFile(path=path, data=data, cls="fragments", source="fragments", parent=parent.path, seed=seed, size_class="fragment", extra={"kind": "excerpt", "lines": shown, "tab_width": excerpt_spec["tab_width"], "anchors": anchors}))
    hunk_spec = spec["hunks"]
    for kind in hunk_spec["kinds"]:
        candidates: list[tuple[str, int]] = []
        for parent in parents:
            for start in construct_starts(parent, kind):
                candidates.append((parent.path, start))
        seed = derived_seed(master, "fragment-hunk", kind)
        rng = random.Random(seed)
        chosen = rng.sample(candidates, min(hunk_spec["per_kind"], len(candidates)))
        chosen.sort()
        for number, (parent_path, start) in enumerate(chosen):
            parent = corpus.files[parent_path]
            lines = split_lines(parent.data)
            item_seed = derived_seed(master, "fragment-hunk", kind, str(number))
            length = random.Random(item_seed).randint(*hunk_spec["lines"])
            stop = min(len(lines), start + length)
            data = b"".join(lines[start:stop])
            path = f"fragments/hunk/{kind}-{number:03d}{suffix_of(parent.path)}"
            corpus.add(CorpusFile(path=path, data=data, cls="fragments", source="fragments", parent=parent.path, seed=item_seed, size_class="fragment", extra={"kind": f"hunk-{kind}", "lines": list(range(start + 1, stop + 1)), "tab_width": None}))


# ---------------------------------------------------------------- class 6


def utf8_cut(data: bytes, offset: int) -> int:
    while 0 < offset < len(data) and (data[offset] & 0xC0) == 0x80:
        offset -= 1
    return offset


def random_utf8(rng: random.Random, size: int) -> bytes:
    pieces: list[bytes] = []
    total = 0
    while total < size:
        roll = rng.random()
        if roll < 0.6:
            code = rng.randint(0x20, 0x7E)
        elif roll < 0.65:
            code = rng.choice([0x09, 0x0A, 0x0D])
        elif roll < 0.85:
            code = rng.randint(0x80, 0x7FF)
        elif roll < 0.97:
            code = rng.randint(0x800, 0xFFFF)
            if 0xD800 <= code <= 0xDFFF:
                code = 0xFFFD
        else:
            code = rng.randint(0x10000, 0x10FFFF)
        encoded = chr(code).encode("utf-8")
        if total + len(encoded) > size:
            break
        pieces.append(encoded)
        total += len(encoded)
    pieces.append(b" " * (size - total))
    return b"".join(pieces)


def add_malformed(corpus: Corpus, master: int, spec: dict) -> None:
    parents = [item for item in eligible_parents(corpus) if len(item.data) <= 1024 * 1024]

    def add(name: str, data: bytes, seed: int | None, parent: str | None = None, **extra) -> None:
        if not is_utf8(data):
            fail(f"generated malformed file {name} is not UTF-8")
        corpus.add(CorpusFile(path=f"malformed/{name}", data=data, cls="malformed", source="malformed", parent=parent, seed=seed, size_class="pathological", extra=extra))

    for number in range(spec["truncated"]):
        seed = derived_seed(master, "truncated", str(number))
        rng = random.Random(seed)
        parent = parents[rng.randrange(len(parents))]
        offset = utf8_cut(parent.data, rng.randrange(1, len(parent.data)))
        add(f"truncated-{number:03d}{suffix_of(parent.path)}", parent.data[:offset], seed, parent.path, kind="truncated", cut=offset)

    brackets = b"{}()[]<>"
    for number in range(spec["unbalanced"]):
        seed = derived_seed(master, "unbalanced", str(number))
        rng = random.Random(seed)
        parent = parents[rng.randrange(len(parents))]
        data = bytearray(parent.data)
        edits = []
        for _ in range(rng.randint(*spec["unbalanced_edits"])):
            positions = [index for index in range(len(data)) if data[index] in brackets]
            if positions and rng.random() < 0.5:
                at = positions[rng.randrange(len(positions))]
                edits.append(["delete", at, chr(data[at])])
                del data[at]
            else:
                at = utf8_cut(bytes(data), rng.randrange(0, len(data) + 1)) if data else 0
                symbol = brackets[rng.randrange(len(brackets))]
                data.insert(at, symbol)
                edits.append(["insert", at, chr(symbol)])
        add(f"unbalanced-{number:03d}{suffix_of(parent.path)}", bytes(data), seed, parent.path, kind="unbalanced", edits=edits)

    depth = spec["nesting_depth"]
    for opener, closer, label in ((b"(", b")", "paren"), (b"[", b"]", "bracket"), (b"{", b"}", "brace"), (b"<", b">", "angle")):
        add(f"nested-{label}-balanced.cpp", opener * depth + closer * depth + b"\n", None, kind="nested", depth=depth, closed=True)
        add(f"nested-{label}-open.cpp", opener * depth + b"\n", None, kind="nested", depth=depth, closed=False)

    target = spec["single_line_bytes"]
    pieces: list[bytes] = []
    total = 0
    number = 0
    while True:
        piece = f"namespace line_ns{number} {{ enum class Line{number} {{ A{number}, B{number} = {number}, C{number} }}; }} ".encode()
        if total + len(piece) > target:
            break
        pieces.append(piece)
        total += len(piece)
        number += 1
    pieces.append(b" " * (target - total))
    add("single-line-1mib.cpp", b"".join(pieces), None, kind="single-line")

    low, high = spec["brace_flood_bytes"]
    add(f"brace-flood-{low}.cpp", b"{" * low, None, kind="brace-flood")
    add(f"brace-flood-{high}.cpp", b"{" * high, None, kind="brace-flood")

    limit = spec["enum_names_bytes"]
    head = b"enum Many {\n"
    tail = b"\n};\n"
    seed = derived_seed(master, "enum-names")
    rng = random.Random(seed)
    body: list[bytes] = []
    total = len(head) + len(tail)
    letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_"
    while True:
        name = "".join(rng.choice(letters) for _ in range(rng.randint(1, 3))) + str(rng.randint(0, 99))
        piece = (name + rng.choice([", ", ",", " ,", ",\n"])).encode()
        if total + len(piece) > limit:
            break
        body.append(piece)
        total += len(piece)
    data = head + b"".join(body)
    data += b" " * (limit - len(data) - len(tail)) + tail
    add("enum-names-8mib.cpp", data, seed, kind="enum-names")

    seed = derived_seed(master, "random-utf8")
    add("random-utf8-1mib.cpp", random_utf8(random.Random(seed), spec["random_utf8_bytes"]), seed, kind="random-utf8")


# ---------------------------------------------------------------- class 7


PASTED_FUNCTION = """\
auto describe_{n}(const int aValue) -> int
{{
	switch (aValue) {{
	case 0:
		return 10;
	case 1:
		return 11;
	case 2:
		return 12;
	default:
		break;
	}}
	auto total = 0;
	for (auto index = 0; index < aValue; ++index) {{
		if (index % 2 == 0) {{
			total += index;
		}} else {{
			total -= index;
		}}
	}}
	const auto scaled = total * 3;
	const auto shifted = scaled << 1;
	if (shifted > 100) {{
		return shifted - 100;
	}}
	// pasted helper {n}
	const char* label = "pasted {n}";
	(void)label;
	return shifted;
}}
"""


def typed_snippet(rng: random.Random, number: int) -> str:
    choice = rng.randrange(4)
    if choice == 0:
        return f"enum class Edit{number} {{ Alpha{number}, Beta{number} = 2, Gamma{number} }};\n"
    if choice == 1:
        return f"enum Plain{number}\n{{\n\tFIRST_{number},\n\tSECOND_{number} = FIRST_{number} + 1,\n}};\n"
    if choice == 2:
        return f"namespace edit{number} {{\n\tint value{number} = 0;\n}}\n"
    return f"namespace edit{number}::inner {{\n\tauto pick{number} = Edit{number}::Alpha{number};\n}}\n"


def composer_steps(base: bytes, rng: random.Random, steps: int) -> list[bytes]:
    text = base.decode("utf-8")
    states = [text]
    number = 0

    def line_start(current: str) -> int:
        starts = [0] + [index + 1 for index, character in enumerate(current) if character == "\n"]
        return starts[rng.randrange(len(starts))]

    while len(states) < steps + 1:
        operation = rng.choices(["type", "delete-readd", "paste", "undo"], weights=[5, 2, 2, 1])[0]
        current = states[-1]
        if operation == "type":
            snippet = typed_snippet(rng, number)
            number += 1
            at = line_start(current)
            typed = 0
            while typed < len(snippet) and len(states) < steps + 1:
                typed = min(len(snippet), typed + rng.randint(1, 6))
                states.append(current[:at] + snippet[:typed] + current[at:])
        elif operation == "delete-readd":
            lines = current.splitlines(keepends=True)
            if len(lines) < 25:
                continue
            first = rng.randrange(0, len(lines) - 20)
            count = rng.randint(5, 20)
            removed = lines[:first] + lines[first + count :]
            states.append("".join(removed))
            if len(states) < steps + 1:
                states.append(current)
        elif operation == "paste":
            at = line_start(current)
            states.append(current[:at] + PASTED_FUNCTION.format(n=number) + current[at:])
            number += 1
        else:
            back = rng.randint(1, min(5, len(states) - 1)) if len(states) > 1 else 0
            states.append(states[-1 - back])
    return [state.encode("utf-8") for state in states]


def add_composer(corpus: Corpus, master: int, spec: dict) -> dict:
    low, high = spec["parent_bytes"]
    summary: dict[str, int] = {}
    sequence = 0
    for cls, count in sorted(spec["sequences"].items()):
        pool = [item for item in eligible_parents(corpus) if item.cls == cls and low <= len(item.data) <= high]
        if not pool:
            fail(f"no composer parent in class {cls}")
        seed = derived_seed(master, "composer", cls)
        rng = random.Random(seed)
        parents = rng.sample(pool, min(count, len(pool)))
        for parent in parents:
            name = f"seq-{sequence:02d}-{cls}"
            item_seed = derived_seed(master, "composer", name)
            states = composer_steps(parent.data, random.Random(item_seed), spec["steps"])
            for step, data in enumerate(states):
                corpus.add(CorpusFile(path=f"composer/{name}/{step:03d}.cpp", data=data, cls="composer", source="composer", parent=parent.path, seed=item_seed, extra={"sequence": name, "step": step}))
            summary[name] = len(states) - 1
            sequence += 1
    return summary


# ---------------------------------------------------------------- large inputs


def add_concatenations(corpus: Corpus, spec: dict) -> None:
    """L-class inputs: whole first-party files in sorted path order, added
    while the total stays at or under each target. They carry no labels."""
    pool = [item for item in corpus.of_class("first-party") if item.source == "first-party"]
    for target in spec["targets"]:
        if target > spec["limit"]:
            fail(f"concatenation target {target} exceeds {spec['limit']}")
        parts: list[bytes] = []
        parents: list[str] = []
        total = 0
        for item in pool:
            if total + len(item.data) > target:
                break
            data = item.data if item.data.endswith(b"\n") or not item.data else item.data + b"\n"
            if total + len(data) > target:
                break
            parts.append(data)
            parents.append(item.path)
            total += len(data)
        corpus.add(CorpusFile(path=f"first-party-concat/concat-{target}.cpp", data=b"".join(parts), cls="first-party", source="first-party-concat", parent=None, size_class="L", extra={"parents": parents, "target": target, "labelled": False}))


# ---------------------------------------------------------------- output


def write_corpus(corpus: Corpus, out: Path, extra_index: dict) -> None:
    entries = []
    for path in sorted(corpus.files):
        item = corpus.files[path]
        target = out / path
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open("xb") as stream:
            stream.write(item.data)
        entry = {
            "id": path,
            "class": item.cls,
            "source": item.source,
            "path": path,
            "bytes": len(item.data),
            "sha256": sha256_bytes(item.data),
            "size_class": item.size_class or size_class(len(item.data)),
            "parent": item.parent,
            "seed": item.seed,
            "utf8": is_utf8(item.data),
        }
        entry.update(item.extra)
        entries.append(entry)
    support = []
    for path in sorted(corpus.support):
        target = out / path
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open("xb") as stream:
            stream.write(corpus.support[path])
        support.append({"path": path, "bytes": len(corpus.support[path]), "sha256": sha256_bytes(corpus.support[path])})
    index = {"schema": "bpr.syntax-evaluation.corpus-index.v1", **extra_index, "files": entries, "support": support}
    text = json.dumps(index, sort_keys=True, separators=(",", ":"), ensure_ascii=True) + "\n"
    (out / "index.json").write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--repo-root", required=True, type=Path)
    parser.add_argument("--cpm-cache", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    arguments = parser.parse_args()
    root = arguments.repo_root.resolve()
    cache = arguments.cpm_cache.resolve()
    out = arguments.out
    if out.exists() and any(out.iterdir()):
        fail(f"output directory {out} is not empty")
    manifest_bytes = (root / MANIFEST_RELATIVE).read_bytes()
    manifest = json.loads(manifest_bytes)
    extensions = manifest["cpp_extensions"]
    master = manifest["master_seed"]
    classes = {entry["name"]: entry for entry in manifest["classes"]}

    corpus = Corpus()
    add_contract(corpus, root, classes["contract"], extensions)
    add_first_party(corpus, root, classes["first-party"], extensions)
    third = []
    for spec in classes["third-party"]["archives"]:
        third.append(add_archive(corpus, cache, spec, extensions))
    for spec in classes["third-party"]["source_trees"]:
        third.append(add_source_tree(corpus, cache, spec, extensions))
    add_gap(corpus, root, classes["gap"])
    add_fragments(corpus, master, classes["fragments"])
    add_malformed(corpus, master, classes["malformed"])
    composer = add_composer(corpus, master, classes["composer"])
    add_concatenations(corpus, classes["first-party"]["concatenations"])

    out.mkdir(parents=True, exist_ok=True)
    write_corpus(corpus, out, {"manifest_sha256": sha256_bytes(manifest_bytes), "third_party": third, "composer": composer})
    totals: dict[str, list[int]] = {}
    for item in corpus.files.values():
        bucket = totals.setdefault(item.cls, [0, 0])
        bucket[0] += 1
        bucket[1] += len(item.data)
    for cls in sorted(totals):
        print(f"{cls}: {totals[cls][0]} files, {totals[cls][1]} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
