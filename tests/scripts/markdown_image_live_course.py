#!/usr/bin/env python3
"""RFC-BPR-0084. The live image course against MikeMarcin/BPR-Test.

Run by hand, never by CTest. It reads one pull request's description and
conversation comments through the owner's `gh` (REST for the Markdown, the
shipped `ResolveBodyImages` GraphQL body for GitHub's served addresses), then
hands both to `test_better_pull_review_markdown_image_live_course --render`,
which fetches every image through the shipped credential-free media transport,
decodes it with the shipped Qt decoder and renders the reader offscreen.

GitHub's signed attachment addresses live only in a private scratch directory
removed on exit; nothing printed names one. Every `gh` call reads the
`X-RateLimit-Remaining` header and the course stops below `--rate-floor`.

    python3 tests/scripts/markdown_image_live_course.py \\
        --executable /abs/build/dev/tests/github_contract/test_better_pull_review_markdown_image_live_course \\
        --pull 73 --output /abs/output
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

OWNER = "MikeMarcin"
REPOSITORY = "BPR-Test"


class CourseFailure(RuntimeError):
    pass


def gh(arguments: list[str], floor: int, payload: str | None = None) -> object:
    result = subprocess.run(["gh", "api", "-i", *arguments], input=payload, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise CourseFailure(f"gh-failed:{arguments[0]}:{result.stderr.strip()[:200]}")
    head, _, body = result.stdout.partition("\r\n\r\n")
    if not body:
        head, _, body = result.stdout.partition("\n\n")
    remaining = None
    for line in head.splitlines():
        name, _, value = line.partition(":")
        if name.strip().lower() == "x-ratelimit-remaining":
            remaining = int(value.strip())
    if remaining is None:
        raise CourseFailure("rate-limit-header-missing")
    print(f"markdown-image-live:rate-remaining:{remaining}")
    if remaining < floor:
        raise CourseFailure(f"rate-limit-below-floor:{remaining}")
    return json.loads(body)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--pull", required=True, type=int)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--width", type=int, default=880)
    parser.add_argument("--rate-floor", type=int, default=500)
    options = parser.parse_args()
    if not options.executable.is_absolute() or not options.output.is_absolute():
        raise CourseFailure("paths-must-be-absolute")

    pull = gh([f"repos/{OWNER}/{REPOSITORY}/pulls/{options.pull}"], options.rate_floor)
    comments = gh([f"repos/{OWNER}/{REPOSITORY}/issues/{options.pull}/comments?per_page=25"], options.rate_floor)
    bodies = [("description", pull["node_id"], pull.get("body") or "")]
    bodies += [(f"comment-{index + 1}", comment["node_id"], comment.get("body") or "") for index, comment in enumerate(comments)]

    request = subprocess.run(
        [str(options.executable), "--resolution-body", OWNER, REPOSITORY, str(options.pull), *(node for _, node, _ in bodies)],
        capture_output=True, text=True, check=False,
    )
    if request.returncode != 0:
        raise CourseFailure(f"resolution-request:{request.stderr.strip()}")
    resolved = gh(["graphql", "--input", "-"], options.rate_floor, request.stdout)
    if resolved.get("errors"):
        raise CourseFailure(f"resolution-errors:{len(resolved['errors'])}")
    data = resolved["data"]
    if data["repository"]["pullRequest"]["id"] != pull["node_id"]:
        raise CourseFailure("resolution-pull-request-mismatch")
    html = {node["id"]: node["bodyHTML"] for node in data["nodes"] if node}

    with contained_temporary_directory(prefix="bpr-markdown-image-live-") as scratch:
        document = {
            "hostname": "github.com",
            "width": options.width,
            "bodies": [{"name": name, "markdown": markdown, "html": html.get(node, "")} for name, node, markdown in bodies],
        }
        source = Path(scratch) / "bodies.json"
        source.write_text(json.dumps(document), encoding="utf-8")
        environment = dict(os.environ, QT_QPA_PLATFORM="offscreen")
        render = subprocess.run([str(options.executable), "--render", str(source), str(options.output)],
                                capture_output=True, text=True, check=False, env=environment, timeout=600)
    sys.stdout.write(render.stdout)
    if render.returncode != 0:
        raise CourseFailure(f"render:{render.returncode}:{render.stderr.strip()[-400:]}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except CourseFailure as failure:
        print(f"markdown-image-live:fail:{failure}")
        sys.exit(1)
