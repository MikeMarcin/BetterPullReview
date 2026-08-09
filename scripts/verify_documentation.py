#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import shlex
import sys
from pathlib import Path


FRONT_MATTER_RE = re.compile(r"^---\n(.*?)\n---\n", re.S)
LINK_RE = re.compile(r"!?\[[^\]]+\]\(([^)]+)\)")
NAMED_LINK_RE = re.compile(r"!?\[([^\]]+)\]\(([^)]+)\)")
FENCED_CODE_RE = re.compile(r"```.*?```", re.S)
INLINE_CODE_RE = re.compile(r"`[^`]*`")
ISSUE_ID_RE = re.compile(r"^BPR-(\d{4})$")
CLOSEOUT_TITLE_RE = re.compile(r"^Verify and close (RFC-BPR-\d{4})$")
CLOSEOUT_FILENAME_RE = re.compile(r"^BPR-(\d{4})-close-rfc-(\d{4})\.md$")
RFC_BASE_RE = re.compile(r"^RFC base: `([0-9a-f]{40})`\.$", re.M)
LEDGER_BASE_RE = re.compile(r"^RFC base: `([0-9a-f]{40})`\.?$", re.M)
ACCEPTANCE_HEADING_RE = re.compile(
    r"^## (?:\d+\.\s+)?Acceptance criteria\s*$",
    re.I,
)
MARKDOWN_ATX_HEADING_RE = re.compile(
    r"^ {0,3}(#{1,6})[ \t]+(.+?)[ \t]*$"
)
MARKDOWN_FENCE_RE = re.compile(r"^ {0,3}(`{3,}|~{3,})")
MARKDOWN_LINK_TEXT_RE = re.compile(r"!?\[([^]]+)\]\([^)]+\)")
MARKDOWN_HTML_COMMENT_RE = re.compile(r"<!--.*?(?:-->|$)", re.S)
CLOSEOUT_COLUMNS = (
    "State",
    "Criterion",
    "Direct verification surface",
    "Command or attended interaction",
    "Expected result",
    "Durable evidence location",
    "Gate class",
    "Live blocker",
)
CLOSEOUT_GATE_CLASSES = {
    "automated",
    "target-native",
    "owner-attended",
    "external-authority",
    "canonical-order",
}
CLOSEOUT_REPLAY_STATEMENT = (
    "Historical evidence is provenance only; every row requires final replay "
    "at the final closeout head."
)
CLOSEOUT_NO_LIVE_BLOCKER = (
    "No live blocker; final-head focused replay remains required."
)
CLOSEOUT_ACTION_RE = re.compile(
    r"^(?:Run|First|Extend|Perform|Complete|After|Verify)\b"
)
CLOSEOUT_PROCEDURE_HEADING_RE = re.compile(r"^(?:Run|Verify|Confirm)\b")
CLOSEOUT_PROCEDURE_BODY_MINIMUM_LENGTH = 80
CLOSEOUT_PROCEDURE_BODY_MINIMUM_TOKENS = 10
CLOSEOUT_EVIDENCE_TOKEN_RE = re.compile(r"[A-Za-z0-9][A-Za-z0-9_./:+#-]*")
CLOSEOUT_SUBSTANTIVE_FIELDS = {
    "Direct verification surface": (30, 3),
    "Command or attended interaction": (20, 3),
    "Expected result": (50, 8),
}
CLOSEOUT_PLACEHOLDER_PATTERNS = {
    "Direct verification surface": re.compile(
        r"^RFC-BPR-\d{4} criterion \d+ through the direct architecture, "
        r"runtime, or authority boundary named by the criterion$"
    ),
    "Command or attended interaction": re.compile(
        r"^`cmake --preset dev && cmake --build --preset dev && "
        r"ctest --preset dev --output-on-failure`$"
    ),
    "Expected result": re.compile(
        r"^The final-head replay demonstrates the criterion exactly as "
        r"written, without historical-count or proxy substitution\.$"
    ),
    "Durable evidence location": re.compile(
        r".*criterion-specific final-head row.*"
    ),
}


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
        result[key.strip()] = value.strip()
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
    index = (root / "docs/rfcs/README.md").read_text(encoding="utf-8")
    rfc_files = sorted((root / "docs/rfcs").glob("RFC-BPR-*.md"))
    if len(rfc_files) != 14:
        fail(f"expected 14 canonical RFCs, found {len(rfc_files)}")

    for path in rfc_files:
        metadata = front_matter(path)
        rfc_id = metadata.get("id")
        status = metadata.get("status")
        if not rfc_id or not status:
            fail(f"RFC id/status missing: {path}")
        row = re.compile(
            rf"\| \[{re.escape(rfc_id)}\]\([^)]+\) \|[^\n]*\| "
            rf"{re.escape(status)} \|"
        )
        if not row.search(index):
            fail(f"RFC index status mismatch: {rfc_id} -> {status}")


def verify_issue_board(root: Path) -> None:
    issue_root = root / "docs/issues"
    lane_status = {
        "backlog": "Backlog",
        "ready": "Ready",
        "in-progress": "In Progress",
        "done": "Done",
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
            if lane != "backlog" and dependency_lane != "done":
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


def is_substantive(value: str, minimum_length: int, minimum_tokens: int) -> bool:
    normalized = normalize_markdown_code(value)
    tokens = CLOSEOUT_EVIDENCE_TOKEN_RE.findall(normalized)
    return (
        len(normalized) >= minimum_length
        and len(tokens) >= minimum_tokens
        and len({token.casefold() for token in tokens}) >= minimum_tokens
    )


def markdown_heading_sections(text: str) -> dict[str, tuple[str, str]]:
    lines = MARKDOWN_HTML_COMMENT_RE.sub("", text).splitlines()
    headings: list[tuple[int, int, str]] = []
    fence: tuple[str, int] | None = None

    for index, line in enumerate(lines):
        fence_match = MARKDOWN_FENCE_RE.match(line)
        if fence_match:
            marker = fence_match.group(1)
            if fence is None:
                fence = (marker[0], len(marker))
            elif marker[0] == fence[0] and len(marker) >= fence[1]:
                fence = None
            continue
        if fence is not None:
            continue

        heading_match = MARKDOWN_ATX_HEADING_RE.fullmatch(line)
        if heading_match is None:
            continue
        heading = re.sub(
            r"[ \t]+#+[ \t]*$",
            "",
            heading_match.group(2),
        )
        headings.append((index, len(heading_match.group(1)), heading))

    anchors: set[str] = set()
    next_suffix: dict[str, int] = {}
    sections: dict[str, tuple[str, str]] = {}
    for position, (line_index, level, heading) in enumerate(headings):
        linked_heading = MARKDOWN_LINK_TEXT_RE.sub(r"\1", heading)
        normalized_heading = normalize_markdown_code(linked_heading)
        base_slug = re.sub(r"[^\w\s-]", "", normalized_heading.lower())
        base_slug = re.sub(r"\s+", "-", base_slug)
        if not base_slug:
            continue

        suffix = next_suffix.get(base_slug, 0)
        candidate = base_slug if suffix == 0 else f"{base_slug}-{suffix}"
        while candidate in anchors:
            suffix += 1
            candidate = f"{base_slug}-{suffix}"
        next_suffix[base_slug] = suffix + 1
        anchors.add(candidate)
        section_end = len(lines)
        for next_line, next_level, _ in headings[position + 1 :]:
            if next_level <= level:
                section_end = next_line
                break
        body = "\n".join(lines[line_index + 1 : section_end]).strip()
        sections[candidate] = (normalized_heading, body)

    return sections


def markdown_heading_anchors(text: str) -> set[str]:
    return set(markdown_heading_sections(text))


def repository_command_executable(command: str) -> str | None:
    if "\n" in command:
        return None
    try:
        tokens = shlex.split(command)
    except ValueError:
        return None
    if tokens == ["python3", "scripts/verify_documentation.py", "--root", "."]:
        return "python3"
    if tokens == [
        "cmake",
        "--build",
        "--preset",
        "dev",
        "--target",
        "coding-format-check",
        "coding-tidy",
        "&&",
        "git",
        "diff",
        "--check",
    ]:
        return "cmake"
    if (
        len(tokens) == 6
        and tokens[:4] == ["ctest", "--preset", "dev", "-R"]
        and tokens[5] == "--output-on-failure"
    ):
        pattern = tokens[4]
        if (
            len(pattern) <= 2
            or not pattern.startswith("^")
            or not pattern.endswith("$")
        ):
            return None
        try:
            re.compile(pattern)
        except re.error:
            return None
        return "ctest"
    return None


def backticked_repository_executables(value: str) -> list[str]:
    executables: list[str] = []
    for match in INLINE_CODE_RE.finditer(value):
        executable = repository_command_executable(match.group(0)[1:-1].strip())
        if executable is not None:
            executables.append(executable)
    return executables


def starts_with_backticked_repository_command(value: str) -> bool:
    match = INLINE_CODE_RE.match(value)
    if match is None:
        return False
    command = match.group(0)[1:-1].strip()
    return repository_command_executable(command) is not None


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


def closeout_table(text: str, path: Path) -> list[dict[str, str]]:
    section_heading = "## Closeout checklist"
    if text.count(section_heading) != 1:
        fail(f"expected exactly one closeout checklist in {path}")
    section = text.split(section_heading, 1)[1]
    next_heading = re.search(r"^## ", section, re.M)
    if next_heading:
        section = section[: next_heading.start()]

    table_lines = [line for line in section.splitlines() if line.startswith("|")]
    if len(table_lines) < 2:
        fail(f"missing closeout checklist table in {path}")

    columns = markdown_table_cells(table_lines[0], path)
    if tuple(columns) != CLOSEOUT_COLUMNS:
        fail(f"wrong closeout checklist columns in {path}")
    separator = markdown_table_cells(table_lines[1], path)
    if len(separator) != len(CLOSEOUT_COLUMNS) or any(
        not re.fullmatch(r":?-{3,}:?", cell) for cell in separator
    ):
        fail(f"invalid closeout checklist separator in {path}")

    rows: list[dict[str, str]] = []
    for line in table_lines[2:]:
        values = markdown_table_cells(line, path)
        if len(values) != len(CLOSEOUT_COLUMNS):
            fail(f"wrong closeout checklist row width in {path}: {line}")
        if any(not normalize_whitespace(value) for value in values):
            fail(f"empty closeout checklist cell in {path}: {line}")
        rows.append(dict(zip(CLOSEOUT_COLUMNS, values, strict=True)))
    return rows


def issue_card_for_link(card_path: Path, target: str, root: Path) -> Path | None:
    if "://" in target or target.startswith("#"):
        return None
    clean_target = target.split("#", 1)[0]
    if not clean_target:
        return None
    resolved = (card_path.parent / clean_target).resolve()
    issue_root = (root / "docs/issues").resolve()
    try:
        resolved.relative_to(issue_root)
    except ValueError:
        return None
    if not resolved.is_file():
        return None
    return resolved


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


def issue_dependency_closure(root: Path, issue_id: str) -> set[str]:
    cards: dict[str, tuple[list[str], Path]] = {}
    for lane in ("backlog", "ready", "in-progress", "done"):
        for path in (root / "docs/issues" / lane).glob("BPR-*.md"):
            metadata = front_matter(path)
            card_id = metadata.get("id", "")
            cards[card_id] = (issue_dependencies(metadata, path), path)
    if issue_id not in cards:
        fail(f"closeout card is absent from issue graph: {issue_id}")

    closure: set[str] = set()
    pending = list(cards[issue_id][0])
    while pending:
        dependency = pending.pop()
        if dependency in closure:
            continue
        if dependency not in cards:
            fail(f"missing dependency for {issue_id}: {dependency}")
        closure.add(dependency)
        pending.extend(cards[dependency][0])
    return closure


def verify_closeout_procedure_links(
    root: Path,
    closeout_path: Path,
    interaction: str,
    dependency_closure: set[str],
) -> None:
    procedure_links = [
        (label, target)
        for label, target in NAMED_LINK_RE.findall(interaction)
        if ISSUE_ID_RE.fullmatch(label)
    ]
    if not procedure_links:
        fail(
            f"closeout interaction lacks an anchored owning procedure in "
            f"{closeout_path}: {interaction}"
        )

    for label, target in procedure_links:
        _, separator, anchor = target.partition("#")
        if not separator or not anchor:
            fail(
                f"closeout procedure link lacks an anchor in {closeout_path}: "
                f"{target}"
            )
        procedure_path = issue_card_for_link(closeout_path, target, root)
        if procedure_path is None:
            fail(
                f"closeout procedure link does not resolve to an issue in "
                f"{closeout_path}: {target}"
            )
        if procedure_path == closeout_path.resolve():
            fail(
                "closeout procedure cannot be the closeout card itself: "
                f"{closeout_path}"
            )

        procedure = front_matter(procedure_path)
        if procedure.get("id") != label:
            fail(
                f"closeout procedure label does not match its issue in "
                f"{closeout_path}: {label}"
            )
        if label not in dependency_closure:
            fail(
                f"closeout procedure is outside the dependency closure in "
                f"{closeout_path}: {label}"
            )
        if (
            procedure.get("status") not in {"Backlog", "Ready", "In Progress"}
            or procedure_path.parent.name not in {"backlog", "ready", "in-progress"}
        ):
            fail(
                f"closeout procedure is not open in {closeout_path}: "
                f"{procedure_path}"
            )

        sections = markdown_heading_sections(
            procedure_path.read_text(encoding="utf-8")
        )
        section = sections.get(anchor)
        if section is None:
            fail(
                f"closeout procedure anchor does not exist in {closeout_path}: "
                f"{target}"
            )
        heading, body = section
        if not (
            CLOSEOUT_PROCEDURE_HEADING_RE.match(heading)
            or heading == "Required direct evidence"
        ):
            fail(
                f"closeout procedure anchor is not action-shaped in "
                f"{closeout_path}: {heading}"
            )
        if not is_substantive(
            body,
            CLOSEOUT_PROCEDURE_BODY_MINIMUM_LENGTH,
            CLOSEOUT_PROCEDURE_BODY_MINIMUM_TOKENS,
        ):
            fail(
                f"closeout procedure section is not substantive in "
                f"{closeout_path}: {target}"
            )


def verify_closeout_card(
    root: Path,
    path: Path,
    metadata: dict[str, str],
    rfc_id: str,
) -> None:
    text = path.read_text(encoding="utf-8")
    rfc_matches = sorted((root / "docs/rfcs").glob(f"{rfc_id}-*.md"))
    if len(rfc_matches) != 1:
        fail(f"expected one canonical RFC for {path}: {rfc_id}")
    ledger_path = root / "docs/verification" / f"{rfc_id}.md"
    if not ledger_path.is_file():
        fail(f"missing verification ledger for {path}: {ledger_path}")

    bases = RFC_BASE_RE.findall(text)
    if len(bases) != 1:
        fail(f"expected exactly one immutable RFC base marker in {path}")
    ledger_text = ledger_path.read_text(encoding="utf-8")
    ledger_bases = LEDGER_BASE_RE.findall(ledger_text)
    if len(ledger_bases) != 1:
        fail(f"expected exactly one RFC base in {ledger_path}")
    if bases[0] != ledger_bases[0]:
        fail(
            f"RFC base mismatch in {path}: {bases[0]} != {ledger_bases[0]}"
        )
    ledger_anchors = markdown_heading_anchors(ledger_text)

    if normalize_whitespace(CLOSEOUT_REPLAY_STATEMENT) not in normalize_whitespace(
        text
    ):
        fail(f"missing final-head replay requirement in {path}")

    rows = closeout_table(text, path)
    canonical = canonical_acceptance_criteria(rfc_matches[0])
    row_criteria = [normalize_whitespace(row["Criterion"]) for row in rows]
    if row_criteria != canonical:
        fail(f"closeout criteria differ from canonical order in {path}")

    dependency_closure = issue_dependency_closure(root, metadata["id"])

    for row in rows:
        if row["State"] != "[ ]":
            fail(f"open closeout row must be unchecked in {path}: {row['Criterion']}")
        gate_class = row["Gate class"]
        if gate_class not in CLOSEOUT_GATE_CLASSES:
            fail(f"invalid closeout gate class in {path}: {gate_class}")
        for column, pattern in CLOSEOUT_PLACEHOLDER_PATTERNS.items():
            if pattern.fullmatch(normalize_whitespace(row[column])):
                fail(
                    f"placeholder closeout checklist cell in {path}: "
                    f"{column} -> {row[column]}"
                )

        for column, (minimum_length, minimum_tokens) in (
            CLOSEOUT_SUBSTANTIVE_FIELDS.items()
        ):
            if not is_substantive(
                row[column], minimum_length, minimum_tokens
            ):
                fail(
                    f"non-substantive closeout checklist cell in {path}: "
                    f"{column} -> {row[column]}"
                )

        interaction = normalize_whitespace(row["Command or attended interaction"])
        if not (
            starts_with_backticked_repository_command(interaction)
            or CLOSEOUT_ACTION_RE.match(interaction)
        ):
            fail(
                f"closeout command is not action-shaped in {path}: "
                f"{interaction}"
            )
        command_executables = backticked_repository_executables(interaction)
        if not command_executables:
            verify_closeout_procedure_links(
                root,
                path,
                interaction,
                dependency_closure,
            )

        evidence = row["Durable evidence location"]
        ledger_links = []
        for target in LINK_RE.findall(evidence):
            ledger_target, separator, anchor = target.partition("#")
            if (
                separator
                and anchor
                and (path.parent / ledger_target).resolve()
                == ledger_path.resolve()
                and anchor in ledger_anchors
            ):
                ledger_links.append(target)
        normalized_evidence = normalize_markdown_code(evidence)
        normalized_criterion = normalize_markdown_code(row["Criterion"])
        expected_replay_key = (
            f"final replay record keyed by “{normalized_criterion}”"
        )
        if not ledger_links:
            fail(
                f"closeout evidence lacks an anchored RFC ledger link in "
                f"{path}: {evidence}"
            )
        if expected_replay_key not in normalized_evidence:
            fail(
                f"closeout evidence lacks the exact final replay key in {path}: "
                f"{evidence}"
            )

        live_blocker = normalize_whitespace(row["Live blocker"])
        if live_blocker == CLOSEOUT_NO_LIVE_BLOCKER:
            if gate_class != "automated":
                fail(
                    f"non-automated closeout row lacks an issue blocker in "
                    f"{path}: {row['Criterion']}"
                )
            continue

        blocker_targets = LINK_RE.findall(row["Live blocker"])
        if not blocker_targets:
            fail(
                f"closeout row lacks a recognized live blocker in {path}: "
                f"{row['Criterion']}"
            )
        blocker_links: list[Path] = []
        for target in blocker_targets:
            blocker_path = issue_card_for_link(path, target, root)
            if blocker_path is None:
                fail(
                    f"closeout row has a non-issue blocker link in {path}: "
                    f"{target}"
                )
            blocker_links.append(blocker_path)
        for blocker_path in blocker_links:
            if blocker_path == path.resolve():
                fail(f"closeout row cannot block on itself in {path}")
            blocker = front_matter(blocker_path)
            blocker_id = blocker.get("id", "")
            if blocker_id not in dependency_closure:
                fail(
                    f"closeout blocker is outside the dependency "
                    f"closure in {path}: {blocker_id}"
                )
            if (
                blocker.get("status") not in {"Backlog", "Ready", "In Progress"}
                or blocker_path.parent.name not in {"backlog", "ready", "in-progress"}
            ):
                fail(
                    f"closeout blocker is not open in {path}: "
                    f"{blocker_path}"
                )
            if gate_class == "canonical-order" and not CLOSEOUT_TITLE_RE.fullmatch(
                blocker.get("title", "")
            ):
                fail(
                    f"canonical-order row does not link a closeout card in {path}: "
                    f"{blocker_path}"
                )


def verify_terminal_closeout_cards(root: Path) -> None:
    for lane in ("backlog", "ready", "in-progress"):
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
    verify_links(root)
    verify_github_actions_not_selected(root)

    print("DOCUMENTATION_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
