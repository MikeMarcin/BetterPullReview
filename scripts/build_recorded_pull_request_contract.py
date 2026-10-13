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


# Byte-for-byte what the application compiles. `strict_recorded_transport`
# compares headers with `!=` over the whole vector, so an empty table is not a
# wildcard — it is a mismatch, reported as `unexpected_headers` on the very
# first bootstrap read. Mirrors `rest_headers()` and `graphql_headers()` in
# src/adapters/github/src/github_read_contract.cpp:423-457.
REST_VERSION = "2022-11-28"
REST_HEADERS = {
    "Accept": "application/vnd.github+json",
    "User-Agent": "BetterPullReview/0.1",
    "X-GitHub-Api-Version": REST_VERSION,
}
GRAPHQL_HEADERS = {
    "Accept": "application/json",
    "Content-Type": "application/json",
    "User-Agent": "BetterPullReview/0.1",
}

# The exact document the application compiles for ListReviewThreads. Qt emits
# object keys sorted with compact separators, which is why the body below is
# built the same way rather than pasted as a string.
GRAPHQL_QUERY = (
    "query GetReviewData($owner:String!,$name:String!,$number:Int!){"
    "repository(owner:$owner,name:$name){id pullRequest(number:$number){id headRefOid "
    "files(first:100){totalCount nodes{path viewerViewedState} "
    "pageInfo{hasNextPage endCursor}} reviewThreads(first:100){totalCount "
    "nodes{id isResolved isOutdated viewerCanResolve viewerCanUnresolve "
    "subjectType path diffSide line originalLine comments(first:100){totalCount "
    "nodes{id bodyText body url updatedAt fullDatabaseId commit{oid} originalCommit{oid} startLine originalStartLine pullRequestReview{id} path line originalLine outdated author{__typename login ... on "
    "Node{id}} replyTo{id}} "
    "pageInfo{hasNextPage endCursor}}} pageInfo{hasNextPage endCursor}}}}}"
)

# Served metadata carries placeholders, substituted at load time by the same
# `replace_all` the github-com fixture already uses. The recording names
# GitHub's real commits, which cannot be rebuilt from it — a commit OID hashes
# its trees, parents and author records, and the recording carries none of
# them. Serving the real OIDs alongside a pack built from generated ones is the
# half-divergence that connects and then fails at import, so the served
# metadata is generated end to end and the divergence is documented.
BASE_OID_PLACEHOLDER = "1" * 40
HEAD_OID_PLACEHOLDER = "2" * 40


def graphql_body(owner: str, name: str, number: int) -> str:
    return json.dumps(
        {
            "query": GRAPHQL_QUERY,
            "variables": {"name": name, "number": number, "owner": owner},
        },
        separators=(",", ":"),
        sort_keys=True,
    )


def write(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    # `encoding` and `newline` are both explicit on purpose. Without them the
    # served tree is written in the platform's locale encoding with `\n`
    # translated to `\r\n`, so the same recording produced different bytes on
    # Windows than the ones checked in — and the contract that compares the
    # served tree to this generator's output could only hold on the platform
    # that last regenerated it.
    path.write_text(
        json.dumps(payload, indent="\t", sort_keys=True) + "\n",
        encoding="utf-8",
        newline="",
    )


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

    # `build_recorded_pull_request_pack.py` builds a two-commit repository:
    # a synthesised base and one head carrying every change. A recording whose
    # commit list holds anything other than the head describes a history this
    # pack cannot represent, and serving it would name commits that are not in
    # `objects.pack`. Refuse rather than generate a fixture that fails at
    # import.
    recorded = [commit["sha"] for commit in recording["commits"]]
    if recorded != [pull["head"]["sha"]]:
        print(
            f"CONTRACT_REFUSED:{slug}#{number}: the pack represents one head "
            f"commit, and this recording names {len(recorded)} commit(s) "
            f"{recorded}. Serving it would reference objects the pack does "
            f"not contain.",
            file=sys.stderr,
        )
        return 1

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
            "sha": BASE_OID_PLACEHOLDER,
        },
        "body": pull["body"],
        "changed_files": pull["changed_files"],
        "commits": pull["commits"],
        "draft": False,
        "head": {
            "ref": pull["head"]["ref"],
            "repo": repo_reference,
            "sha": HEAD_OID_PLACEHOLDER,
        },
        "id": 3,
        "merged": False,
        "node_id": f"PR_{number}",
        "number": number,
        "state": pull["state"],
        "title": pull["title"],
        "user": {"login": login, "node_id": f"U_{login}"},
    })
    # The content a reviewer sees is diffed from git objects, not from this,
    # which is the whole reason `objects.pack` exists beside it. The page still
    # carries the `patch`, `sha` and `changes` GitHub sent, because GitHub
    # always sends them and the product must read a page that holds them.
    # BPR-0892: before this, no course had ever served a files page with a
    # patch in it.
    write(root / "responses/files.json", [
        {
            "additions": entry["additions"],
            "changes": entry["changes"],
            "deletions": entry["deletions"],
            "filename": entry["filename"],
            "patch": entry["patch"],
            "sha": entry["sha"],
            "status": entry["status"],
            **({"previous_filename": entry["previous_filename"]} if "previous_filename" in entry else {}),
        }
        for entry in recording["files"]
    ])
    write(root / "responses/commits.json", [
        {
            "author": {"login": login, "node_id": f"U_{login}"},
            "commit": {"message": commit["message"]},
            "node_id": f"C_{HEAD_OID_PLACEHOLDER[:8]}",
            "sha": HEAD_OID_PLACEHOLDER,
        }
        for commit in recording["commits"]
    ])
    write(root / "responses/reviews.json", [])
    write(root / "responses/issue_comments.json", [])
    # The GraphQL response is validated far more strictly than its REST
    # siblings: `validate_review_threads_fixture` requires an exact key set at
    # every level, a node identifier on the repository and the pull request,
    # a `files` block whose `totalCount` equals its node count, a
    # `viewerViewedState` drawn from a fixed vocabulary, and a `pageInfo` whose
    # `endCursor` is null exactly when the page is empty and a non-empty string
    # otherwise. A response missing any of it is refused as
    # `unexpected_response_fields`, which is what an earlier version of this
    # generator produced.
    #
    # The file paths are the recorded ones. `viewerViewedState` is UNVIEWED
    # because an unauthenticated read carries no viewer state and there is
    # nothing recorded to claim otherwise.
    file_nodes = [
        {"path": entry["filename"], "viewerViewedState": "UNVIEWED"}
        for entry in recording["files"]
    ]
    write(root / "responses/review_threads.json", {
        "data": {
            "repository": {
                "id": f"R_{name}",
                "pullRequest": {
                    "id": f"PR_{number}",
                    "headRefOid": HEAD_OID_PLACEHOLDER,
                    "files": {
                        "nodes": file_nodes,
                        "pageInfo": {
                            "endCursor": f"cursor:{slug}:{number}:files",
                            "hasNextPage": False,
                        },
                        "totalCount": len(file_nodes),
                    },
                    "reviewThreads": {
                        "nodes": [],
                        "pageInfo": {"endCursor": None, "hasNextPage": False},
                        "totalCount": 0,
                    },
                },
            }
        }
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
             "canonical_query": "", "safe_headers": REST_HEADERS, "body": None,
             "response_file": "responses/viewer.json"},
            {"operation": "GetRepository", "method": "GET",
             "path": f"/repos/{slug}", "canonical_query": "",
             "safe_headers": REST_HEADERS, "body": None,
             "response_file": "responses/repository.json"},
            {"operation": "GetPullRequest", "method": "GET",
             "path": f"/repos/{slug}/pulls/{number}", "canonical_query": "",
             "safe_headers": REST_HEADERS, "body": None,
             "response_file": "responses/pull_request.json"},
            {"operation": "ListPullRequestFiles", "method": "GET",
             "path": f"/repos/{slug}/pulls/{number}/files",
             "canonical_query": paged, "safe_headers": REST_HEADERS, "body": None,
             "response_file": "responses/files.json"},
            {"operation": "ListPullRequestCommits", "method": "GET",
             "path": f"/repos/{slug}/pulls/{number}/commits",
             "canonical_query": paged, "safe_headers": REST_HEADERS, "body": None,
             "response_file": "responses/commits.json"},
            {"operation": "ListReviews", "method": "GET",
             "path": f"/repos/{slug}/pulls/{number}/reviews",
             "canonical_query": paged, "safe_headers": REST_HEADERS, "body": None,
             "response_file": "responses/reviews.json"},
            {"operation": "ListReviewThreads", "method": "POST",
             "path": "/graphql", "canonical_query": "",
             "safe_headers": GRAPHQL_HEADERS,
             "body": graphql_body(owner, name, number), "response_file": "responses/review_threads.json"},
            {"operation": "ListIssueComments", "method": "GET",
             "path": f"/repos/{slug}/issues/{number}/comments",
             "canonical_query": paged, "safe_headers": REST_HEADERS, "body": None,
             "response_file": "responses/issue_comments.json"},
            {"operation": "ListChecks", "method": "GET",
             "path": f"/repos/{slug}/commits/{HEAD_OID_PLACEHOLDER}/check-runs",
             "canonical_query": paged, "safe_headers": REST_HEADERS, "body": None,
             "response_file": "responses/checks.json"},
            {"operation": "SearchPullRequests", "method": "GET",
             "path": "/search/issues",
             "canonical_query": (
                 f"{paged}&q=is%3Apr%20is%3Aopen%20repo%3A"
                 f"{owner}%2F{name}"),
             "safe_headers": REST_HEADERS, "body": None,
             "response_file": "responses/search.json"},
        ],
    })

    print(f"CONTRACT_OK:{slug}#{number}:{len(recording['files'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
