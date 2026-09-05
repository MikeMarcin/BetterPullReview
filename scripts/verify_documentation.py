#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path


# This module uses 3.10 syntax and library behaviour, most visibly
# ``zip(..., strict=True)`` in ``closeout_table``. Without this guard an older
# interpreter fails deep inside a table parser with ``TypeError: zip() takes no
# keyword arguments``, which reads like a malformed closeout card rather than
# the wrong ``python3`` on PATH. Keep this block free of syntax newer than the
# version it refuses, or it cannot report anything.
MINIMUM_PYTHON = (3, 10)

if sys.version_info < MINIMUM_PYTHON:
    sys.stderr.write(
        "verify_documentation.py requires Python {}.{} or newer; this is {}.{}.{} "
        "at {}.\n".format(
            MINIMUM_PYTHON[0],
            MINIMUM_PYTHON[1],
            sys.version_info[0],
            sys.version_info[1],
            sys.version_info[2],
            sys.executable,
        )
    )
    raise SystemExit(2)


# A front-matter value is unquoted and carries a colon that YAML would read
# as a nested mapping key. Quoting it makes both readers agree.
UNQUOTED_YAML_COLON_RE = re.compile(r"""(?!["'])[^\n]*:(?:\s|$)""")
FRONT_MATTER_RE = re.compile(r"^---\n(.*?)\n---\n", re.S)
LINK_RE = re.compile(r"!?\[[^\]]+\]\(([^)]+)\)")
FENCED_CODE_RE = re.compile(r"```.*?```", re.S)
INLINE_CODE_RE = re.compile(r"`[^`]*`")
ISSUE_ID_RE = re.compile(r"^BPR-(\d{4})$")
CLOSEOUT_TITLE_RE = re.compile(r"^Verify and close (RFC-BPR-\d{4})$")
CLOSEOUT_FILENAME_RE = re.compile(r"^BPR-(\d{4})-close-rfc-(\d{4})\.md$")
RFC_BASE_RE = re.compile(r"^RFC base: `([0-9a-f]{40})`\.$", re.M)
ACCEPTANCE_HEADING_RE = re.compile(
    r"^## (?:\d+\.\s+)?Acceptance criteria\s*$",
    re.I,
)
# The closeout checklist is a light contract: a table under one
# `## Closeout checklist` heading with a `Criterion` column and at least one
# more, covering every canonical acceptance criterion with non-empty cells.
# The eight-column schema, its gate-class and live-blocker tokens, anchored
# evidence links, per-row procedure links, and the final-replay statement were
# retired on 2026-09-01: they policed the shape of the record rather than the
# truth of the acceptance, and a closeout cost a full replay to satisfy them.
CLOSEOUT_SECTION_HEADING = "## Closeout checklist"
CLOSEOUT_CRITERION_COLUMN = "Criterion"
# Ready splits by platform because a lane has to tell a session what it can
# actually start. "Blocked" said something was unresolved when nothing was: the
# machine exists and the work is specified, so it is ready somewhere else.
BLOCKED_BY_RE = re.compile(r"^[a-z0-9][a-z0-9-]{0,63}$")
PLATFORM_READY_LANES = {
    "21-ready-win": "Windows",
    "22-ready-mac": "macOS",
    "23-ready-lin": "Linux",
}
OPEN_ISSUE_LANES = (
    "10-backlog",
    "40-parked",
    "50-blocked",
    "20-ready",
    *PLATFORM_READY_LANES,
    "30-in-progress",
)
DONE_ISSUE_LANE = "60-done"
# A platform lane defers exactly as the blocked lane it replaces did: the
# platform is why the card is not in `20-ready/`, and its dependency graph is a
# separate question that stays in `depends_on`.
DEFERRING_ISSUE_LANES = frozenset(
    {"10-backlog", "50-blocked", *PLATFORM_READY_LANES}
)
REVIEW_ROOT = "docs/verification/reviews"
OPERATOR_PACKET_NAME = "operator-packet.md"
OPERATOR_RESPONSE_NAME = "operator-response.md"
PACKET_VERSION_RE = re.compile(
    r"(?P<family>.+)-v(?P<version>\d{3})(?:-[A-Za-z0-9-]+)?"
)
PACKET_REFERENCE_RE = re.compile(
    r"(?P<family>[a-z0-9][a-z0-9-]*?)-v(?P<version>\d{3})\b"
)
PACKET_STATUS_RE = re.compile(r"^Status:\s*\*\*(.+?)\*\*", re.M | re.S)
PACKET_PENDING_RE = re.compile(r"pending\b", re.I)
RESPONSE_PACKET_SHA_RE = re.compile(
    r"^Packet SHA-256: `([0-9a-f]{64})`(?: \([^\r\n]*\))?$", re.M
)
PARKED_ON_VALUES = frozenset(
    {
        "awaiting-owner-verdict",
        "awaiting-live-github",
        "awaiting-product-decision",
    }
)


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def front_matter(path: Path) -> dict[str, str]:
    text = path.read_text(encoding="utf-8")
    match = FRONT_MATTER_RE.match(text)
    if not match:
        fail(f"missing front matter: {path}")
    result: dict[str, str] = {}
    for line in match.group(1).splitlines():
        if ":" not in line:
            fail(f"invalid front matter in {path}: {line}")
        key, value = line.split(":", 1)
        value = value.strip()
        # This parser keeps everything after the first colon, so it reads an
        # unquoted `title: Core: buckets and reasons` correctly. A YAML parser
        # does not -- it rejects the whole block with "mapping values are not
        # allowed here" and returns no front matter at all. The `---` fence
        # tells every reader the block is YAML, so refuse the inputs on which
        # the two disagree rather than depending on which reader arrives.
        if UNQUOTED_YAML_COLON_RE.match(value):
            fail(
                f"unquoted colon in front matter of {path}: {line}\n"
                "  a YAML reader rejects the whole block; quote the value"
            )
        result[key.strip()] = value
    return result


def require_files(root: Path) -> None:
    required = [
        "AGENTS.md",
        "README.md",
        "CONTRIBUTING.md",
        "docs/AGENTS.md",
        "docs/README.md",
        "docs/rfcs/README.md",
        "docs/issues/README.md",
        "docs/issues/NEXT_ID",
        "docs/architecture/product-contract.md",
        "docs/architecture/system-architecture.md",
        "docs/architecture/foundation-implementation.md",
        "docs/architecture/github-host-implementation.md",
        "docs/development/build-and-test.md",
        "docs/security/foundation-security.md",
        "docs/verification/RFC-BPR-0002.md",
        "docs/verification/RFC-BPR-0003.md",
        "src/core/AGENTS.md",
        "src/application/AGENTS.md",
        "src/adapters/AGENTS.md",
        "src/adapters/github/AGENTS.md",
        "src/adapters/qt/AGENTS.md",
        "src/app/AGENTS.md",
        "tests/AGENTS.md",
        "packaging/AGENTS.md",
    ]
    for relative in required:
        if not (root / relative).is_file():
            fail(f"missing required documentation: {relative}")


def verify_rfc_index(root: Path) -> None:
    rfc_root = root / "docs/rfcs"
    if (rfc_root / "future").exists():
        fail("live RFCs belong directly in docs/rfcs; remove the future folder")
    index = (rfc_root / "README.md").read_text(encoding="utf-8")
    rfc_files = sorted(rfc_root.glob("RFC-BPR-*.md"))
    # The original pack remains required; later live RFCs use the same folder
    # and index instead of encoding their status in a separate directory.
    required_pack = {f"RFC-BPR-{number:04d}" for number in range(1, 15)}
    seen: set[str] = set()

    for path in rfc_files:
        metadata = front_matter(path)
        rfc_id = metadata.get("id")
        status = metadata.get("status")
        if not rfc_id or not re.fullmatch(r"RFC-BPR-\d{4}", rfc_id) or not status:
            fail(f"RFC id/status missing: {path}")
        if not path.name.startswith(f"{rfc_id}-"):
            fail(f"RFC filename/id mismatch: {path} -> {rfc_id}")
        if rfc_id in seen:
            fail(f"duplicate canonical RFC id: {rfc_id}")
        seen.add(rfc_id)
        row = re.compile(
            rf"\| \[{re.escape(rfc_id)}\]\({re.escape(path.name)}\) \|[^\n]*\| "
            rf"{re.escape(status)} \|"
        )
        if not row.search(index):
            fail(f"RFC index status mismatch: {rfc_id} -> {status}")
    if missing := required_pack - seen:
        fail(f"missing original-pack RFCs: {', '.join(sorted(missing))}")


def verify_issue_board(root: Path) -> None:
    issue_root = root / "docs/issues"
    lane_status = {
        "10-backlog": "Backlog",
        "40-parked": "Parked",
        "50-blocked": "Blocked",
        "20-ready": "Ready",
        **{lane: "Ready" for lane in PLATFORM_READY_LANES},
        "30-in-progress": "In Progress",
        DONE_ISSUE_LANE: "Done",
    }
    cards: dict[str, tuple[str, list[str], Path]] = {}
    highest = 0
    for lane, expected_status in lane_status.items():
        lane_path = issue_root / lane
        if not lane_path.exists():
            continue
        for path in lane_path.glob("BPR-*.md"):
            metadata = front_matter(path)
            issue_id = metadata.get("id", "")
            status = metadata.get("status")
            match = ISSUE_ID_RE.fullmatch(issue_id)
            if not match:
                fail(f"invalid issue id in {path}: {issue_id}")
            if issue_id in cards:
                fail(f"duplicate issue id: {issue_id}")
            if status != expected_status:
                fail(
                    f"issue lane/status mismatch: {path} is {status}, "
                    f"expected {expected_status}"
                )
            dependencies = issue_dependencies(metadata, path)
            if len(dependencies) != len(set(dependencies)):
                fail(
                    f"duplicate dependency in {path}: "
                    f"{metadata.get('depends_on')}"
                )
            # `50-blocked/` is for a real blocker: something unresolved that
            # is not a platform, a person, or another card. A platform is a
            # lane now, so naming one here would hide the card from the session
            # that could actually run it.
            blocked_by = metadata.get("blocked_by")
            if lane == "50-blocked":
                if not blocked_by:
                    fail(f"blocked card does not declare blocked_by: {path}")
                if not BLOCKED_BY_RE.fullmatch(blocked_by):
                    fail(f"malformed blocked_by in {path}: {blocked_by}")
                if blocked_by.startswith("platform-"):
                    fail(
                        f"{path} declares {blocked_by}; a platform is a lane, "
                        f"not a blocker: use one of {sorted(PLATFORM_READY_LANES)}"
                    )
            elif blocked_by is not None:
                fail(f"blocked_by outside the blocked lane: {path}")
            parked_on = metadata.get("parked_on")
            if lane == "40-parked":
                if not parked_on:
                    fail(f"parked card does not declare parked_on: {path}")
                if parked_on not in PARKED_ON_VALUES:
                    fail(
                        f"unknown parked_on in {path}: {parked_on}; "
                        f"expected one of {sorted(PARKED_ON_VALUES)}"
                    )
            elif parked_on is not None:
                fail(f"parked_on outside the parked lane: {path}")
            cards[issue_id] = (lane, dependencies, path)
            highest = max(highest, int(match.group(1)))

    next_id = (issue_root / "NEXT_ID").read_text(encoding="utf-8").strip()
    next_match = ISSUE_ID_RE.fullmatch(next_id)
    if not next_match:
        fail(f"invalid NEXT_ID: {next_id}")
    if int(next_match.group(1)) <= highest:
        fail(f"NEXT_ID {next_id} does not follow highest allocated issue")

    for issue_id, (lane, dependencies, path) in cards.items():
        for dependency in dependencies:
            if dependency == issue_id:
                fail(f"self dependency in {path}: {dependency}")
            if dependency not in cards:
                fail(f"missing dependency in {path}: {dependency}")
            dependency_lane = cards[dependency][0]
            if (
                lane not in DEFERRING_ISSUE_LANES
                and dependency_lane != DONE_ISSUE_LANE
            ):
                fail(
                    f"{lane} card {issue_id} depends on {dependency} "
                    f"in {dependency_lane}"
                )

    visiting: set[str] = set()
    visited: set[str] = set()

    def visit(issue_id: str, trail: list[str]) -> None:
        if issue_id in visited:
            return
        if issue_id in visiting:
            cycle_start = trail.index(issue_id)
            cycle = " -> ".join(trail[cycle_start:] + [issue_id])
            fail(f"issue dependency cycle: {cycle}")
        visiting.add(issue_id)
        trail.append(issue_id)
        for dependency in cards[issue_id][1]:
            visit(dependency, trail)
        trail.pop()
        visiting.remove(issue_id)
        visited.add(issue_id)

    for issue_id in sorted(cards):
        visit(issue_id, [])


def normalize_whitespace(value: str) -> str:
    return " ".join(value.split())


def normalize_markdown_code(value: str) -> str:
    return normalize_whitespace(
        INLINE_CODE_RE.sub(lambda match: match.group(0)[1:-1], value)
    )


def markdown_table_cells(line: str, path: Path) -> list[str]:
    stripped = line.strip()
    if not stripped.startswith("|") or not stripped.endswith("|"):
        fail(f"invalid closeout checklist table row in {path}: {line}")

    cells: list[str] = []
    current: list[str] = []
    escaped = False
    for character in stripped[1:-1]:
        if escaped:
            current.append(character)
            escaped = False
        elif character == "\\":
            escaped = True
        elif character == "|":
            cells.append("".join(current).strip())
            current = []
        else:
            current.append(character)
    if escaped:
        current.append("\\")
    cells.append("".join(current).strip())
    return cells


def canonical_acceptance_criteria(path: Path) -> list[str]:
    lines = path.read_text(encoding="utf-8").splitlines()
    heading_index = next(
        (
            index
            for index, line in enumerate(lines)
            if ACCEPTANCE_HEADING_RE.fullmatch(line)
        ),
        None,
    )
    if heading_index is None:
        fail(f"missing acceptance criteria heading: {path}")

    criteria: list[str] = []
    current: list[str] | None = None
    for line in lines[heading_index + 1 :]:
        if line.startswith("## "):
            break
        bullet = re.fullmatch(r"- (?:\[[ xX]\] )?(.*)", line)
        if bullet:
            if current is not None:
                criteria.append(normalize_whitespace(" ".join(current)))
            current = [bullet.group(1)]
        elif current is not None and line[:1].isspace() and line.strip():
            current.append(line.strip())
    if current is not None:
        criteria.append(normalize_whitespace(" ".join(current)))
    if not criteria:
        fail(f"empty acceptance criteria section: {path}")
    return criteria


def issue_dependencies(metadata: dict[str, str], path: Path) -> list[str]:
    raw_dependencies = metadata.get("depends_on")
    if (
        raw_dependencies is None
        or not raw_dependencies.startswith("[")
        or not raw_dependencies.endswith("]")
    ):
        fail(f"invalid depends_on list in {path}: {raw_dependencies}")
    dependency_text = raw_dependencies[1:-1].strip()
    dependencies = (
        [part.strip() for part in dependency_text.split(",")]
        if dependency_text
        else []
    )
    if any(not ISSUE_ID_RE.fullmatch(item) for item in dependencies):
        fail(f"invalid dependency id in {path}: {raw_dependencies}")
    return dependencies


def closeout_table(text: str, path: Path) -> list[dict[str, str]]:
    if text.count(CLOSEOUT_SECTION_HEADING) != 1:
        fail(f"expected exactly one closeout checklist in {path}")
    section = text.split(CLOSEOUT_SECTION_HEADING, 1)[1]
    next_heading = re.search(r"^## ", section, re.M)
    if next_heading:
        section = section[: next_heading.start()]

    table_lines = [line for line in section.splitlines() if line.startswith("|")]
    if len(table_lines) < 3:
        fail(f"missing closeout checklist table in {path}")

    columns = markdown_table_cells(table_lines[0], path)
    if CLOSEOUT_CRITERION_COLUMN not in columns or len(columns) < 2:
        fail(
            f"closeout checklist needs a {CLOSEOUT_CRITERION_COLUMN!r} column and "
            f"at least one more in {path}"
        )
    separator = markdown_table_cells(table_lines[1], path)
    if len(separator) != len(columns) or any(
        not re.fullmatch(r":?-{3,}:?", cell) for cell in separator
    ):
        fail(f"invalid closeout checklist separator in {path}")

    rows: list[dict[str, str]] = []
    for line in table_lines[2:]:
        values = markdown_table_cells(line, path)
        if len(values) != len(columns):
            fail(f"wrong closeout checklist row width in {path}: {line}")
        if any(not normalize_whitespace(value) for value in values):
            fail(f"empty closeout checklist cell in {path}: {line}")
        rows.append(dict(zip(columns, values, strict=True)))
    return rows


def verify_closeout_card(
    root: Path,
    path: Path,
    metadata: dict[str, str],
    rfc_id: str,
) -> None:
    """The light closeout contract.

    An open closeout card records, for every canonical acceptance criterion of
    its RFC, the check run, its result, and the commit. This checks that the
    card anchors the immutable RFC base and covers every criterion; it does not
    prescribe columns, tokens, links, or replay. Order and extra rows are the
    card's business.
    """
    text = path.read_text(encoding="utf-8")
    rfc_matches = sorted((root / "docs/rfcs").glob(f"{rfc_id}-*.md"))
    if len(rfc_matches) != 1:
        fail(f"expected one canonical RFC for {path}: {rfc_id}")

    bases = RFC_BASE_RE.findall(text)
    if len(bases) != 1:
        fail(f"expected exactly one immutable RFC base marker in {path}")

    rows = closeout_table(text, path)
    canonical = canonical_acceptance_criteria(rfc_matches[0])
    recorded = {
        normalize_markdown_code(row[CLOSEOUT_CRITERION_COLUMN]) for row in rows
    }
    for criterion in canonical:
        if normalize_markdown_code(criterion) not in recorded:
            fail(
                f"closeout checklist lacks canonical criterion in {path}: "
                f"{criterion}"
            )


def verify_terminal_closeout_cards(root: Path) -> None:
    for lane in OPEN_ISSUE_LANES:
        for path in sorted((root / "docs/issues" / lane).glob("BPR-*.md")):
            metadata = front_matter(path)
            title_match = CLOSEOUT_TITLE_RE.fullmatch(metadata.get("title", ""))
            filename_match = CLOSEOUT_FILENAME_RE.fullmatch(path.name)
            if not title_match and not filename_match:
                continue
            if not title_match or not filename_match:
                fail(f"closeout filename/title mismatch: {path}")

            filename_issue_id = f"BPR-{filename_match.group(1)}"
            filename_rfc_id = f"RFC-BPR-{filename_match.group(2)}"
            title_rfc_id = title_match.group(1)
            if (
                metadata.get("id") != filename_issue_id
                or metadata.get("rfc") != filename_rfc_id
                or title_rfc_id != filename_rfc_id
            ):
                fail(f"closeout filename/title/front-matter mismatch: {path}")
            verify_closeout_card(root, path, metadata, filename_rfc_id)


def operator_packets(root: Path) -> dict[tuple[str, int], Path]:
    """Every review directory holding an operator packet, keyed by family and version."""
    packets: dict[tuple[str, int], Path] = {}
    for directory in sorted((root / REVIEW_ROOT).glob("*")):
        packet = directory / OPERATOR_PACKET_NAME
        if not packet.is_file():
            continue
        match = PACKET_VERSION_RE.fullmatch(directory.name)
        if not match:
            fail(f"operator packet directory is not versioned: {directory}")
        packets[(match.group("family"), int(match.group("version")))] = packet
    return packets


def packet_status(path: Path) -> str:
    match = PACKET_STATUS_RE.search(path.read_text(encoding="utf-8"))
    if not match:
        fail(f"operator packet has no bolded Status line: {path}")
    return normalize_whitespace(match.group(1))


def frozen_packets(root: Path) -> set[Path]:
    """Packets an answered artifact binds by digest; never edit these bytes."""
    frozen: set[Path] = set()
    for record in sorted((root / REVIEW_ROOT).glob("*/validation-record.json")):
        try:
            recorded = json.loads(record.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            fail(f"invalid validation record {record}: {error}")
        for key, entry in recorded.items():
            if not isinstance(entry, dict) or "path" not in entry:
                continue
            bound = root / entry["path"]
            # Only artifacts inside the record's own review directory are frozen.
            # A reference outside it, such as the validator that produced the
            # record, is a historical attribution anchored by `validated_commit`
            # and is expected to move as that tool evolves.
            if bound.parent != record.parent:
                continue
            if not bound.is_file():
                fail(f"{record} binds a missing artifact: {entry['path']}")
            content = bound.read_bytes()
            digest = f"sha256:{hashlib.sha256(content).hexdigest()}"
            if entry.get("sha256") != digest or entry.get("byte_length") != len(content):
                fail(
                    f"{record} binds {key} at {entry.get('sha256')} "
                    f"({entry.get('byte_length')} bytes) but {entry['path']} is now "
                    f"{digest} ({len(content)} bytes); an answered packet's bytes are "
                    "the record of what the operator was shown and must not be edited"
                )
            if bound.name == OPERATOR_PACKET_NAME:
                frozen.add(bound)

    for response in sorted((root / REVIEW_ROOT).glob(
        f"*/{OPERATOR_RESPONSE_NAME}"
    )):
        packet = response.parent / OPERATOR_PACKET_NAME
        if not packet.is_file():
            continue
        digests = RESPONSE_PACKET_SHA_RE.findall(
            response.read_text(encoding="utf-8"))
        if len(digests) != 1:
            continue
        actual = hashlib.sha256(packet.read_bytes()).hexdigest()
        if digests[0] != actual:
            continue
        frozen.add(packet)
    return frozen


def verify_pending_packet_manifests(root: Path) -> None:
    """A pending packet's source manifest must describe the tree it claims to.

    `frozen_packets` re-hashes artifacts bound by an answered validation record
    or exact digest-bearing response. That leaves the window in which a packet
    is most dangerous unchecked: written, pending, and being put in front of an
    operator.

    `designated-live-review-publication-v002` went through exactly that window
    stale at birth. Six of its 31 entries did not match the tree and three did
    not match at the commit that introduced it, and nothing noticed until a
    reviewer hashed the files by hand. A stale manifest is invisible by
    construction — same shape, same 64-hex values, same byte count — so this is
    a gate rather than a matter of diligence. BPR-0411.
    """
    review_root = root / REVIEW_ROOT
    if not review_root.is_dir():
        return
    answered = frozen_packets(root)
    for manifest in sorted(review_root.glob("*/source-manifest.txt")):
        packet = manifest.parent / OPERATOR_PACKET_NAME
        if not packet.is_file():
            continue
        # Only an unanswered *pending* packet is checked. An answered,
        # superseded, or invalidated one describes the tree as it was on purpose.
        if packet in answered or not PACKET_PENDING_RE.search(packet_status(packet)):
            continue
        for number, line in enumerate(
            manifest.read_text(encoding="utf-8").splitlines(), start=1
        ):
            if not line.strip():
                continue
            relative, separator, expected = line.partition("=")
            if not separator or len(expected) != 64:
                fail(f"{manifest}:{number} is not a path=sha256 entry")
            bound = root / relative
            if not bound.is_file():
                fail(f"{manifest}:{number} names a missing file: {relative}")
            actual = hashlib.sha256(bound.read_bytes()).hexdigest()
            if actual != expected:
                fail(
                    f"{manifest}:{number} does not match the tree: {relative} "
                    f"is {actual}, the manifest says {expected}"
                )


def verify_operator_packet_records(root: Path) -> None:
    review_root = root / REVIEW_ROOT
    if not review_root.is_dir():
        return

    packets = operator_packets(root)
    frozen = frozen_packets(root)

    live: dict[str, int] = {}
    for family, version in packets:
        live[family] = max(version, live.get(family, version))

    for (family, version), path in sorted(packets.items()):
        status = packet_status(path)
        if version == live[family] or path in frozen:
            continue
        if PACKET_PENDING_RE.match(status):
            fail(
                f"{path} is superseded by v{live[family]:03d} but its Status line still "
                f"claims to be pending: {status!r}"
            )

    for lane in OPEN_ISSUE_LANES:
        for card in sorted((root / "docs/issues" / lane).glob("BPR-*.md")):
            text = card.read_text(encoding="utf-8")
            named: dict[str, set[int]] = {}
            for match in PACKET_REFERENCE_RE.finditer(text):
                family = match.group("family")
                version = int(match.group("version"))
                if (family, version) in packets:
                    named.setdefault(family, set()).add(version)
            for family, versions in sorted(named.items()):
                if live[family] not in versions:
                    fail(
                        f"{card} names {family}-v{max(versions):03d} but not the live "
                        f"{family}-v{live[family]:03d}; an open card that discusses a "
                        "packet family must carry its current pointer"
                    )


def markdown_files(root: Path) -> list[Path]:
    files = [
        root / "AGENTS.md",
        root / "README.md",
        root / "CONTRIBUTING.md",
    ]
    files.extend((root / "docs").rglob("*.md"))
    files.extend((root / "src").rglob("AGENTS.md"))
    files.extend((root / "tests").rglob("*.md"))
    files.extend((root / "packaging").rglob("*.md"))
    return sorted(set(files))


def verify_links(root: Path) -> None:
    for path in markdown_files(root):
        text = path.read_text(encoding="utf-8")
        text = FENCED_CODE_RE.sub("", text)
        text = INLINE_CODE_RE.sub("", text)
        for target in LINK_RE.findall(text):
            if (
                "://" in target
                or target.startswith("#")
                or target.startswith("mailto:")
            ):
                continue
            clean = target.split("#", 1)[0]
            if not clean:
                continue
            resolved = (path.parent / clean).resolve()
            try:
                resolved.relative_to(root)
            except ValueError:
                fail(f"link escapes repository in {path}: {target}")
            if not resolved.exists():
                fail(f"broken relative link in {path}: {target}")


def verify_github_actions_not_selected(root: Path) -> None:
    workflow_root = root / ".github/workflows"
    if not workflow_root.exists():
        return

    workflows: list[Path] = []
    for pattern in ("*.yml", "*.yaml"):
        workflows.extend(workflow_root.glob(pattern))
    if workflows:
        selected = ", ".join(
            str(path.relative_to(root)) for path in sorted(workflows)
        )
        fail(
            "GitHub Actions is not selected for BetterPullReview: "
            f"{selected}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate BetterPullReview documentation and tracker structure."
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=Path.cwd(),
        help="repository root (default: current directory)",
    )
    arguments = parser.parse_args()
    root = arguments.root.resolve()

    require_files(root)
    verify_rfc_index(root)
    verify_issue_board(root)
    verify_terminal_closeout_cards(root)
    verify_operator_packet_records(root)
    verify_pending_packet_manifests(root)
    verify_links(root)
    verify_github_actions_not_selected(root)

    print("DOCUMENTATION_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
