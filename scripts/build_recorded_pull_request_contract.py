#!/usr/bin/env python3
"""Turns a recorded pull request into the contract fixture the product serves.

BPR-0187 step 3. The application reads a fixture as ten recorded HTTP exchanges
under `tests/fixtures/github/contracts/v1/<name>/`, not as a `recording.json`.
This writes that directory from a recording so the two cannot drift: the number,
the title, the file list and the commit identifiers all come from the recorded
pull request rather than being retyped.

Only the three operations a recording actually describes are derived from it —
the pull request, its files, its commits. The other seven are structural: an
identity, a repository, and empty reviews, review threads, issue comments,
checks and search results. They are written here rather than copied from
`github-com` so that changing this fixture cannot silently change that one.

Usage:
    build_recorded_pull_request_contract.py <recording.json> <output-directory>
"""

from __future__ import annotations

import json
import sys
from pathlib import Path


def write(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent="\t", sort_keys=True) + "\n")


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2

    recording = json.loads(Path(sys.argv[1]).read_text())
    root = Path(sys.argv[2])

    pull = recording["pull_request"]
    owner = recording["repository"]["owner"]
    name = recording["repository"]["name"]
    number = pull["number"]
    slug = f"{owner}/{name}"
    login = pull["author"]["login"]

    write(root / "responses/viewer.json", {
        "id": 1,
        "login": login,
        "name": login,
        "node_id": f"U_{login}",
    })
    write(root / "responses/repository.json", {
        "default_branch": pull["base"]["ref"],
        "full_name": slug,
        "id": 2,
        "name": name,
        "node_id": f"R_{name}",
        "owner": {"login": owner},
        "private": False,
    })
    # `repo` on both sides and `node_id` on every actor. The provider reads
    # these, and a payload missing them is refused as `malformed_response` —
    # which is what a shape comparison over top-level keys alone will not tell
    # you, because the difference is one level down.
    repo_reference = {"full_name": slug, "id": 2}
    write(root / "responses/pull_request.json", {
        "base": {
            "ref": pull["base"]["ref"],
            "repo": repo_reference,
            "sha": pull["base"]["sha"],
        },
        "body": pull["body"],
        "changed_files": pull["changed_files"],
        "commits": pull["commits"],
        "draft": False,
        "head": {
            "ref": pull["head"]["ref"],
            "repo": repo_reference,
            "sha": pull["head"]["sha"],
        },
        "id": 3,
        "merged": False,
        "node_id": f"PR_{number}",
        "number": number,
        "state": pull["state"],
        "title": pull["title"],
        "user": {"login": login, "node_id": f"U_{login}"},
    })
    # Metadata only. The content a reviewer sees is diffed from git objects, not
    # from this, which is the whole reason `objects.pack` exists beside it.
    write(root / "responses/files.json", [
        {
            "additions": entry["additions"],
            "deletions": entry["deletions"],
            "filename": entry["filename"],
            "status": entry["status"],
        }
        for entry in recording["files"]
    ])
    write(root / "responses/commits.json", [
        {
            "author": {"login": login, "node_id": f"U_{login}"},
            "commit": {"message": commit["message"]},
            "node_id": f"C_{commit['sha'][:8]}",
            "sha": commit["sha"],
        }
        for commit in recording["commits"]
    ])
    write(root / "responses/reviews.json", [])
    write(root / "responses/issue_comments.json", [])
    write(root / "responses/review_threads.json", {
        "data": {"repository": {"pullRequest": {"reviewThreads": {
            "nodes": [],
            "pageInfo": {"endCursor": None, "hasNextPage": False},
        }}}}
    })
    write(root / "responses/checks.json", {"check_runs": [], "total_count": 0})
    write(root / "responses/search.json", {
        "incomplete_results": False,
        "items": [],
        "total_count": 0,
    })

    paged = "page=1&per_page=100"
    write(root / "manifest.json", {
        "schema_revision": 1,
        "product": "github.com",
        "host": "github.com",
        "api_roots": {
            "rest": "https://api.github.com",
            "graphql": "https://api.github.com/graphql",
        },
        "version": "cloud",
        "sanitized": True,
        "interactions": [
            {"operation": "GetViewerIdentity", "method": "GET", "path": "/user",
             "canonical_query": "", "safe_headers": {}, "body": None,
             "response_file": "responses/viewer.json"},
            {"operation": "GetRepository", "method": "GET",
             "path": f"/repos/{slug}", "canonical_query": "",
             "safe_headers": {}, "body": None,
             "response_file": "responses/repository.json"},
            {"operation": "GetPullRequest", "method": "GET",
             "path": f"/repos/{slug}/pulls/{number}", "canonical_query": "",
             "safe_headers": {}, "body": None,
             "response_file": "responses/pull_request.json"},
            {"operation": "ListPullRequestFiles", "method": "GET",
             "path": f"/repos/{slug}/pulls/{number}/files",
             "canonical_query": paged, "safe_headers": {}, "body": None,
             "response_file": "responses/files.json"},
            {"operation": "ListPullRequestCommits", "method": "GET",
             "path": f"/repos/{slug}/pulls/{number}/commits",
             "canonical_query": paged, "safe_headers": {}, "body": None,
             "response_file": "responses/commits.json"},
            {"operation": "ListReviews", "method": "GET",
             "path": f"/repos/{slug}/pulls/{number}/reviews",
             "canonical_query": paged, "safe_headers": {}, "body": None,
             "response_file": "responses/reviews.json"},
            {"operation": "ListReviewThreads", "method": "POST",
             "path": "/graphql", "canonical_query": "", "safe_headers": {},
             "body": None, "response_file": "responses/review_threads.json"},
            {"operation": "ListIssueComments", "method": "GET",
             "path": f"/repos/{slug}/issues/{number}/comments",
             "canonical_query": paged, "safe_headers": {}, "body": None,
             "response_file": "responses/issue_comments.json"},
            {"operation": "ListChecks", "method": "GET",
             "path": f"/repos/{slug}/commits/{pull['head']['sha']}/check-runs",
             "canonical_query": paged, "safe_headers": {}, "body": None,
             "response_file": "responses/checks.json"},
            {"operation": "SearchPullRequests", "method": "GET",
             "path": "/search/issues",
             "canonical_query": (
                 f"{paged}&q=is%3Apr%20is%3Aopen%20repo%3A"
                 f"{owner}%2F{name}"),
             "safe_headers": {}, "body": None,
             "response_file": "responses/search.json"},
        ],
    })

    print(f"CONTRACT_OK:{slug}#{number}:{len(recording['files'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
