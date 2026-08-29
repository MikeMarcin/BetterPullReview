#!/usr/bin/env python3
"""Claim the next issue id(s) and advance `docs/issues/NEXT_ID`.

Reading the counter, filing a card, and writing the counter back is not
atomic: two sessions allocating in the same window both read the same value
and the collision is only found later. The claim here is taken under a lock
created with `O_CREAT | O_EXCL`, and the counter is validated before it
advances: it must be ahead of every card in the tree and at least the highest
value the file has held in Git history, which covers a branch that allocated
ids and has not merged yet.

Usage:

    python3 scripts/allocate_issue_id.py
    python3 scripts/allocate_issue_id.py --count 3 --json
    python3 scripts/allocate_issue_id.py --count 8 \\
        --reserve-for bpr-win-jaegaw --purpose "windows-lane block"

`--reserve-for` records the claim as an immutable receipt under
`docs/issues/_reservations/`, which is how a station branch takes a block of
ids without ever editing `NEXT_ID`: the integrator reserves on `main`,
commits the advanced counter together with the receipt, and the consuming
branch verifies the receipt at that commit with `git show` before filing its
first block card.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import uuid
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

AREA = "BPR"
NEXT_ID_PATH = Path("docs/issues/NEXT_ID")
RESERVATIONS_DIR = Path("docs/issues/_reservations")
LOCK_DIR = Path("build/tmp/issue-board")
ID_RE = re.compile(rf"^{AREA}-(\d{{4}})$")
CARD_RE = re.compile(rf"^{AREA}-(\d{{4}})-")
HISTORY_ADDED_RE = re.compile(rf"^\+{AREA}-(\d{{4}})\s*$", re.MULTILINE)
HISTORY_COMMIT_BOUND = 2000
MAX_ISSUE_NUMBER = 9999
MAX_COUNT = 64


class AllocationError(RuntimeError):
    """A refusal the caller has to act on; never a partially applied claim."""


@dataclass(frozen=True)
class Allocation:
    ids: list[str]
    previous_next_id: str
    next_id: str
    reservation_id: str | None = None
    receipt_path: str | None = None


def read_counter(repo_root: Path) -> int:
    path = repo_root / NEXT_ID_PATH
    if not path.is_file():
        raise AllocationError(f"{path} does not exist")
    raw = path.read_text(encoding="utf-8").strip()
    match = ID_RE.match(raw)
    if match is None:
        raise AllocationError(f"{path} holds {raw!r}, which is not {AREA}-####")
    return int(match.group(1))


def highest_card(repo_root: Path) -> int:
    highest = 0
    issues_root = repo_root / "docs/issues"
    for lane in sorted(p for p in issues_root.iterdir() if p.is_dir()):
        if lane.name == RESERVATIONS_DIR.name:
            continue
        for card in lane.glob(f"{AREA}-*.md"):
            match = CARD_RE.match(card.name)
            if match is not None:
                highest = max(highest, int(match.group(1)))
    return highest


def history_high_water(repo_root: Path) -> int:
    """The highest value NEXT_ID has held across branch/remote/tag history.

    A rewound counter hands out ids a concurrent branch has already spent, and
    the working tree cannot see that branch's cards; Git history can. One log
    with patches bounds the walk instead of one `git show` per commit.
    """
    completed = subprocess.run(
        [
            "git", "-C", str(repo_root), "log", "-p",
            f"-n{HISTORY_COMMIT_BOUND}", "--branches", "--remotes", "--tags",
            "--format=%H", "--", str(NEXT_ID_PATH),
        ],
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        return 0
    values = [int(number) for number in HISTORY_ADDED_RE.findall(completed.stdout)]
    return max(values, default=0)


def write_counter(repo_root: Path, number: int) -> None:
    path = repo_root / NEXT_ID_PATH
    temporary = path.with_name(f".{path.name}.{os.getpid()}.{uuid.uuid4().hex}")
    try:
        with open(temporary, "w", encoding="utf-8") as stream:
            stream.write(f"{AREA}-{number:04d}\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def write_receipt(repo_root: Path, allocation: Allocation, reserved_for: str, purpose: str) -> Allocation:
    stamp = datetime.now(timezone.utc)
    reservation_id = f"idr-bpr-{stamp.strftime('%Y%m%d')}-{uuid.uuid4().hex[:4]}"
    directory = repo_root / RESERVATIONS_DIR
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / f"{reservation_id}.json"
    if path.exists():
        raise AllocationError(f"receipt {path} already exists; not overwriting")
    payload = {
        "schema": "betterpullreview.issue-id-reservation.v1",
        "reservation_id": reservation_id,
        "range": {"start": allocation.ids[0], "end": allocation.ids[-1]},
        "reserved_for": reserved_for,
        "purpose": purpose,
        "reserved_at": stamp.isoformat(timespec="seconds").replace("+00:00", "Z"),
        "previous_next_id": allocation.previous_next_id,
        "next_id": allocation.next_id,
    }
    with open(path, "w", encoding="utf-8") as stream:
        json.dump(payload, stream, indent=2, sort_keys=True)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())
    return Allocation(
        ids=allocation.ids,
        previous_next_id=allocation.previous_next_id,
        next_id=allocation.next_id,
        reservation_id=reservation_id,
        receipt_path=str(path.relative_to(repo_root)),
    )


def allocate(repo_root: Path, count: int, reserve_for: str | None, purpose: str) -> Allocation:
    if not 1 <= count <= MAX_COUNT:
        raise AllocationError(f"--count must be between 1 and {MAX_COUNT}")

    lock_dir = repo_root / LOCK_DIR
    lock_dir.mkdir(parents=True, exist_ok=True)
    lock_path = lock_dir / "NEXT_ID.lock"
    try:
        handle = os.open(lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o644)
    except FileExistsError:
        raise AllocationError(
            f"{lock_path} exists; another allocation is running. If none is, "
            "remove that file and retry"
        ) from None
    try:
        os.write(handle, f"pid {os.getpid()}\n".encode("utf-8"))
        os.close(handle)

        current = read_counter(repo_root)
        highest = highest_card(repo_root)
        if current <= highest:
            raise AllocationError(
                f"NEXT_ID is {AREA}-{current:04d} but {AREA}-{highest:04d} is "
                f"already allocated in this tree; set it to at least "
                f"{AREA}-{highest + 1:04d} after checking for duplicates"
            )
        mark = history_high_water(repo_root)
        if current < mark:
            raise AllocationError(
                f"NEXT_ID is {AREA}-{current:04d} but {AREA}-{mark:04d} was "
                "already reached in Git history; allocating here would reuse an "
                "id another branch has spent. Advance the counter instead"
            )

        last = current + count - 1
        if last > MAX_ISSUE_NUMBER:
            raise AllocationError(
                f"{AREA}-{last:04d} exceeds the four-digit id format"
            )

        ids = [f"{AREA}-{number:04d}" for number in range(current, last + 1)]
        write_counter(repo_root, last + 1)
        allocation = Allocation(
            ids=ids,
            previous_next_id=f"{AREA}-{current:04d}",
            next_id=f"{AREA}-{last + 1:04d}",
        )
        if reserve_for is not None:
            allocation = write_receipt(repo_root, allocation, reserve_for, purpose)
        return allocation
    finally:
        lock_path.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--count", type=int, default=1)
    parser.add_argument("--json", action="store_true")
    parser.add_argument(
        "--repo-root", type=Path, default=Path(__file__).resolve().parents[1]
    )
    parser.add_argument(
        "--reserve-for", default=None, metavar="STATION",
        help="record the block as an immutable reservation receipt for this station",
    )
    parser.add_argument("--purpose", default="")
    args = parser.parse_args()

    if args.purpose and args.reserve_for is None:
        print("allocate_issue_id: --purpose requires --reserve-for", file=sys.stderr)
        return 1

    try:
        allocation = allocate(
            args.repo_root.resolve(), args.count, args.reserve_for, args.purpose
        )
    except AllocationError as error:
        print(f"allocate_issue_id: {error}", file=sys.stderr)
        return 1

    if args.json:
        report = {
            "schema": "betterpullreview.issue-id-allocation.v1",
            "ids": allocation.ids,
            "previous_next_id": allocation.previous_next_id,
            "next_id": allocation.next_id,
        }
        if allocation.reservation_id is not None:
            report["reservation_id"] = allocation.reservation_id
            report["receipt_path"] = allocation.receipt_path
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        for issue_id in allocation.ids:
            print(issue_id)
        if allocation.receipt_path is not None:
            print(f"receipt: {allocation.receipt_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
