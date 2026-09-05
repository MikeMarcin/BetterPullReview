#!/usr/bin/env python3
"""Audit live issue-board and RFC state against the implement-rfc process contract.

Repo-side checks, all driven by config/project.json: lane canonicality,
folder/status agreement, dangling dependencies, next-ID sanity (never below the
highest ID on the board or the highest value the counter ever held in Git
history), active work filed against terminal RFCs, and closeout-anchor presence.
Transcript presence is informational; the audit never turns delegation behavior
into a closeout requirement.

Exit status is 1 when any violation is found, else 0.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))
import project_config  # noqa: E402
import board_model  # noqa: E402

TERMINAL_RFC_STATUSES = {"Auto-Verified", "Rejected", "Superseded"}
CARD_ID = re.compile(r"^[A-Z]{2,8}-\d{4}$")
ANCHOR_RE = re.compile(r"RFC_BASE_SHA|RFC base:")


def counter_history_max(repo_root: Path, path: str) -> str | None:
    """Highest ID the next-ID file ever held on this checkout's history, or None."""
    result = subprocess.run(
        ["git", "-C", str(repo_root), "log", "-p", "--format=", "--", path],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        return None
    best: tuple[str, int] | None = None
    for match in re.finditer(r"^\+([A-Z]{2,8}-\d{4})\s*$", result.stdout, re.MULTILINE):
        token = match.group(1)
        number = int(token.rsplit("-", 1)[1])
        if best is None or number > best[1]:
            best = (token, number)
    return best[0] if best else None


def audit(
    repo_root: Path,
    config: dict[str, Any],
    transcripts_dir: Path | None = None,
) -> tuple[list[str], list[str], list[str]]:
    violations: list[str] = []
    warnings: list[str] = []
    infos: list[str] = []
    issues_root = repo_root / config["issue_root"]
    rfcs_root = repo_root / config["rfc_root"]
    active = project_config.active_lanes(config)
    is_closeout = project_config.closeout_card_matcher(config)

    if not issues_root.is_dir():
        violations.append(f"issue root missing: {config['issue_root']}")
        return violations, warnings, infos

    cards, structural_errors = board_model.validate_board(repo_root, config)
    violations.extend(map(str, structural_errors))
    active_cards = [card for card in cards.values() if card.lane in active]

    next_id_path = repo_root / config["next_id"]["path"]
    if next_id_path.is_file():
        value = next_id_path.read_text(encoding="utf-8").strip()
        historical = counter_history_max(repo_root, config["next_id"]["path"])
        if CARD_ID.fullmatch(value) and historical:
            area, number = value.rsplit("-", 1)
            if historical.startswith(area + "-") and int(historical.rsplit("-", 1)[1]) > int(number):
                violations.append(f"next {value} is below {historical}, which the counter already held in Git history")

    rfc_status: dict[str, str] = {}
    for card in active_cards:
        for rfc in card.rfcs:
            if rfc not in rfc_status:
                matches = list(rfcs_root.glob(f"{rfc}-*.md")) if rfcs_root.is_dir() else []
                if not matches:
                    rfc_status[rfc] = ""
                    continue
                try:
                    rfc_status[rfc] = board_model.parse_matter(matches[0].read_bytes(), matches[0]).values.get("status", "")
                except board_model.BoardError:
                    rfc_status[rfc] = ""  # Already reported by structural validation.
            status = rfc_status[rfc]
            if status in TERMINAL_RFC_STATUSES:
                message = f"{card.id} in {card.lane} implements {rfc}, which is already {status}"
                if status == config["terminal_rfc_status"]:
                    warnings.append(message + " (follow-up/deferred pattern; confirm disposition)")
                else:
                    violations.append(message)

    open_rfcs = sorted(
        {rfc for card in active_cards for rfc in card.rfcs if rfc_status.get(rfc) not in TERMINAL_RFC_STATUSES}
    )
    for rfc in open_rfcs:
        family = [card for card in cards.values() if rfc in card.rfcs]
        closeouts = [card for card in family if is_closeout(card.path, card.fields)]
        if not closeouts:
            warnings.append(f"{rfc}: no closeout card found among its {len(family)} cards")
        elif not any(ANCHOR_RE.search(card.body) for card in closeouts):
            warnings.append(f"{rfc}: closeout card lacks an RFC_BASE_SHA run anchor")

    if not transcripts_dir or not transcripts_dir.is_dir():
        infos.append(
            "transcripts: none found for this checkout on this machine; sessions "
            "on remote stations audit their own transcripts (--transcripts-dir "
            "overrides the checkout-derived location)"
        )
    else:
        transcript_count = sum(1 for _ in transcripts_dir.glob("*.jsonl"))
        infos.append(
            f"transcripts: {transcript_count} file(s) present; delegation behavior "
            "is informational and is not a closeout gate"
        )

    return violations, warnings, infos


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=project_config.repo_root())
    parser.add_argument("--transcripts-dir", type=Path, default=None)
    parser.add_argument(
        "--project-config",
        type=Path,
        default=project_config.PROJECT_PATH,
        help=argparse.SUPPRESS,
    )
    args = parser.parse_args()
    config = project_config.load(args.project_config)

    transcripts = args.transcripts_dir
    if transcripts is None:
        flattened = str(args.repo_root.resolve()).replace("/", "-")
        candidate = Path.home() / ".claude/projects" / flattened
        transcripts = candidate if candidate.is_dir() else None

    violations, warnings, infos = audit(args.repo_root.resolve(), config, transcripts)
    for title, rows in (
        ("VIOLATIONS", violations),
        ("WARNINGS", warnings),
        ("INFO", infos),
    ):
        if rows:
            print(f"{title} ({len(rows)})")
            for row in rows:
                print(f"  - {row}")
    if not violations and not warnings:
        print("board conforms: no violations, no warnings")
    elif not violations:
        print("board conforms: no violations")
    return 1 if violations else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except project_config.ProjectConfigError as error:
        print(f"error: {error}", file=sys.stderr)
        sys.exit(2)
