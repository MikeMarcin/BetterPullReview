#!/usr/bin/env python3
"""Writes the constructed Status-attention contract the product can serve.

BPR-1589. The owner judges the Overview's Status drawer: my role, Needs
attention, My review, Checks and People. overview-labels-v1 is read by its
author with no checks, so the drawer has little to say. This copies that
contract (`tests/fixtures/github/contracts/v1/overview-labels-v1`, BPR-1089)
and changes three things:

- the viewer is tbutler, a reviewer who requested changes and whose first
  thread has a reply from the author;
- the description mentions @tbutler;
- the head has thirteen check runs: two failed (build-linux, lint), one
  skipped and ten passed.

Review requests and the merge assessment come from GraphQL operations a
fixture contract does not serve, so this pull request shows neither a request
nor conflicts; the Qt and application courses cover those. Nothing here was
fetched, and no identity is real.

Usage:
    build_status_attention_fixture.py <repository root>
"""

from __future__ import annotations

import json
import shutil
import sys
from pathlib import Path

SOURCE = "tests/fixtures/github/contracts/v1/overview-labels-v1"
TARGET = "tests/fixtures/github/contracts/v1/status-attention-v1"
HEAD = "2222222222222222222222222222222222222222"

CHECKS = (
    ("build-linux", "failure"),
    ("build-mac", "success"),
    ("build-windows", "success"),
    ("lint", "failure"),
    ("format", "success"),
    ("test-engine", "success"),
    ("test-ui", "success"),
    ("test-shaders", "success"),
    ("package", "success"),
    ("docs", "success"),
    ("license-scan", "success"),
    ("size-report", "success"),
    ("deploy-preview", "skipped"),
)


def write(path: Path, document: object) -> None:
    path.write_text(json.dumps(document, indent="\t", sort_keys=True) + "\n", encoding="utf-8", newline="")


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    root = Path(sys.argv[1])
    target = root / TARGET
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(root / SOURCE, target)
    responses = target / "responses"
    write(responses / "viewer.json", {"id": 3, "login": "tbutler", "name": "tbutler", "node_id": "U_fixture_tbutler"})
    request = json.loads((responses / "pull_request.json").read_text(encoding="utf-8"))
    request["body"] = request["body"].rstrip("\n") + "\n\ncc @tbutler for the renderer changes.\n"
    write(responses / "pull_request.json", request)
    write(responses / "checks.json", {
        "total_count": len(CHECKS),
        "check_runs": [
            {"node_id": f"CR_sa_{index}", "name": name, "head_sha": HEAD, "status": "completed", "conclusion": conclusion}
            for index, (name, conclusion) in enumerate(CHECKS, start=1)
        ],
    })
    print(f"STATUS_ATTENTION_FIXTURE_OK:{len(CHECKS)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
