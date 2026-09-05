#!/usr/bin/env python3
"""Validate the RFC-BPR-0022 source pack."""

from __future__ import annotations

import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "PACK-MANIFEST.json"
SUMS = ROOT / "SHA256SUMS"
CARD_ID = re.compile(r"^PACK-BPR-0022-\d{2}$")


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def fail(message: str) -> None:
    raise ValueError(message)


def front_matter(text: str, path: str) -> dict[str, str]:
    if not text.startswith("---\n"):
        fail(f"{path}: missing front matter")
    end = text.find("\n---\n", 4)
    if end < 0:
        fail(f"{path}: unterminated front matter")
    values: dict[str, str] = {}
    for line in text[4:end].splitlines():
        if ":" not in line:
            fail(f"{path}: malformed front matter line {line!r}")
        key, value = line.split(":", 1)
        if key in values:
            fail(f"{path}: repeated front matter key {key}")
        values[key.strip()] = value.strip()
    return values


def parse_deps(value: str, path: str) -> list[str]:
    if not value.startswith("[") or not value.endswith("]"):
        fail(f"{path}: depends_on must use a simple inline list")
    inner = value[1:-1].strip()
    return [] if not inner else [item.strip() for item in inner.split(",")]


def main() -> int:
    try:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        if manifest.get("schema_name") != "bpr.rfc_source_pack" or manifest.get("schema_version") != 1:
            fail("unsupported manifest schema")
        entries = manifest.get("payload")
        if not isinstance(entries, list) or not entries:
            fail("manifest payload must be a non-empty list")
        expected: set[str] = set()
        for index, entry in enumerate(entries):
            if not isinstance(entry, dict) or set(entry) != {"path", "sha256", "bytes"}:
                fail(f"manifest payload[{index}] has the wrong shape")
            relative = entry["path"]
            if not isinstance(relative, str) or relative.startswith("/") or ".." in Path(relative).parts:
                fail(f"invalid payload path: {relative!r}")
            if relative in expected:
                fail(f"duplicate payload path: {relative}")
            expected.add(relative)
            path = ROOT / relative
            if not path.is_file():
                fail(f"missing payload file: {relative}")
            data = path.read_bytes()
            if b"\r" in data:
                fail(f"payload must use LF only: {relative}")
            if len(data) != entry["bytes"] or digest(path) != entry["sha256"]:
                fail(f"payload identity mismatch: {relative}")

        actual = {
            path.relative_to(ROOT).as_posix()
            for path in ROOT.rglob("*")
            if path.is_file() and path.name not in {"PACK-MANIFEST.json", "SHA256SUMS"}
        }
        if actual != expected:
            fail(f"payload inventory mismatch: missing={sorted(expected-actual)}, extra={sorted(actual-expected)}")

        sums: dict[str, str] = {}
        for line in SUMS.read_text(encoding="utf-8").splitlines():
            value, separator, relative = line.partition("  ")
            if not separator or not re.fullmatch(r"[0-9a-f]{64}", value):
                fail(f"malformed SHA256SUMS line: {line!r}")
            if relative in sums:
                fail(f"duplicate SHA256SUMS path: {relative}")
            sums[relative] = value
        sum_paths = expected | {"PACK-MANIFEST.json"}
        if set(sums) != sum_paths:
            fail("SHA256SUMS inventory mismatch")
        for relative, value in sums.items():
            if digest(ROOT / relative) != value:
                fail(f"SHA256SUMS mismatch: {relative}")

        cards = sorted((ROOT / "cards").glob("*.md"))
        if len(cards) != 15:
            fail(f"expected 15 provisional cards, found {len(cards)}")
        ids: set[str] = set()
        references: list[tuple[str, str]] = []
        for card in cards:
            relative = card.relative_to(ROOT).as_posix()
            values = front_matter(card.read_text(encoding="utf-8"), relative)
            card_id = values.get("provisional_id", "")
            if not CARD_ID.fullmatch(card_id):
                fail(f"{relative}: invalid provisional_id {card_id!r}")
            if card_id in ids:
                fail(f"duplicate provisional_id: {card_id}")
            ids.add(card_id)
            if values.get("live_id") != "allocate-on-import" or values.get("rfc") != "RFC-BPR-0022":
                fail(f"{relative}: wrong live_id or RFC binding")
            for dependency in parse_deps(values.get("depends_on", ""), relative):
                references.append((relative, dependency))
        for relative, dependency in references:
            if dependency not in ids:
                fail(f"{relative}: unknown provisional dependency {dependency}")

        rfc = (ROOT / "docs/rfcs/future/RFC-BPR-0022-semantic-source-line-comments-off-hunk-publication.md").read_text(encoding="utf-8")
        if rfc.count("## Acceptance criteria") != 1:
            fail("RFC must contain one canonical acceptance-criteria section")
        numbered = re.findall(r"^(\d+)\. ", rfc.split("## Acceptance criteria",1)[1].split("## Verification matrix",1)[0], re.M)
        if numbered != [str(i) for i in range(1,16)]:
            fail(f"RFC acceptance criteria must be exactly 1..15, got {numbered}")
        if "does not edit, replace, or reopen RFC-BPR-0008" not in rfc:
            fail("RFC must preserve the separate-RFC decision")

        print(f"RFC_BPR_0022_PACK_OK files={len(expected)+2} cards={len(cards)}")
        return 0
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
