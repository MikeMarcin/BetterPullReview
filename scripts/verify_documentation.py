#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


FRONT_MATTER_RE = re.compile(r"^---\n(.*?)\n---\n", re.S)
LINK_RE = re.compile(r"!?\[[^\]]+\]\(([^)]+)\)")
FENCED_CODE_RE = re.compile(r"```.*?```", re.S)
INLINE_CODE_RE = re.compile(r"`[^`]*`")
ISSUE_ID_RE = re.compile(r"^BPR-(\d{4})$")


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
            if len(dependencies) != len(set(dependencies)):
                fail(f"duplicate dependency in {path}: {raw_dependencies}")
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
    verify_links(root)
    verify_github_actions_not_selected(root)

    print("DOCUMENTATION_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
