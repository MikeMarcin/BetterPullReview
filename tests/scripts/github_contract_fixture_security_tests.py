#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re
import unittest
from urllib.parse import urlsplit


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
FIXTURE_ROOT = (
    REPOSITORY_ROOT / "tests" / "fixtures" / "github" / "contracts" / "v1"
)
CAPABILITY_FIXTURE_ROOT = (
    REPOSITORY_ROOT
    / "tests"
    / "fixtures"
    / "github"
    / "capabilities"
    / "v1"
)

OPERATIONS = (
    "GetViewerIdentity",
    "GetRepository",
    "GetPullRequest",
    "ListPullRequestFiles",
    "ListPullRequestCommits",
    "ListReviews",
    "ListReviewThreads",
    "ListIssueComments",
    "ListChecks",
    "SearchPullRequests",
)

REST_HEADERS = {
    "Accept": "application/vnd.github+json",
    "User-Agent": "BetterPullReview/0.1",
    "X-GitHub-Api-Version": "2022-11-28",
}
GRAPHQL_HEADERS = {
    "Accept": "application/json",
    "Content-Type": "application/json",
    "User-Agent": "BetterPullReview/0.1",
}
GRAPHQL_QUERY = (
    "query GetReviewThreads($owner:String!,$name:String!,$number:Int!){"
    "repository(owner:$owner,name:$name){id pullRequest(number:$number){id "
    "files(first:100){nodes{path viewerViewedState} "
    "pageInfo{hasNextPage endCursor}} reviewThreads(first:100){nodes{id "
    "isResolved isOutdated viewerCanResolve viewerCanUnresolve subjectType "
    "path line originalLine comments(first:100){nodes{id bodyText path line "
    "originalLine outdated author{login ... on Node{id}} replyTo{id}} "
    "pageInfo{hasNextPage endCursor}}} pageInfo{hasNextPage endCursor}}}}}"
)
GRAPHQL_BODY = json.dumps(
    {
        "query": GRAPHQL_QUERY,
        "variables": {
            "name": "review-repository",
            "number": 17,
            "owner": "octo-org",
        },
    },
    separators=(",", ":"),
    sort_keys=True,
)

HOSTS = {
    "github-com": {
        "product": "github.com",
        "host": "github.com",
        "api_roots": {
            "rest": "https://api.github.com",
            "graphql": "https://api.github.com/graphql",
        },
        "version": "cloud",
        "rest_prefix": "",
        "graphql_path": "/graphql",
    },
    "ghes-3.18": {
        "product": "github-enterprise-server",
        "host": "ghe.example.invalid",
        "api_roots": {
            "rest": "https://ghe.example.invalid/api/v3",
            "graphql": "https://ghe.example.invalid/api/graphql",
        },
        "version": "3.18",
        "rest_prefix": "/api/v3",
        "graphql_path": "/api/graphql",
    },
}

OPERATION_CONTRACTS = {
    "GetViewerIdentity": (
        "GET",
        "/user",
        "",
        REST_HEADERS,
        None,
        "viewer.json",
    ),
    "GetRepository": (
        "GET",
        "/repos/octo-org/review-repository",
        "",
        REST_HEADERS,
        None,
        "repository.json",
    ),
    "GetPullRequest": (
        "GET",
        "/repos/octo-org/review-repository/pulls/17",
        "",
        REST_HEADERS,
        None,
        "pull_request.json",
    ),
    "ListPullRequestFiles": (
        "GET",
        "/repos/octo-org/review-repository/pulls/17/files",
        "page=1&per_page=100",
        REST_HEADERS,
        None,
        "files.json",
    ),
    "ListPullRequestCommits": (
        "GET",
        "/repos/octo-org/review-repository/pulls/17/commits",
        "page=1&per_page=100",
        REST_HEADERS,
        None,
        "commits.json",
    ),
    "ListReviews": (
        "GET",
        "/repos/octo-org/review-repository/pulls/17/reviews",
        "page=1&per_page=100",
        REST_HEADERS,
        None,
        "reviews.json",
    ),
    "ListReviewThreads": (
        "POST",
        "",
        "",
        GRAPHQL_HEADERS,
        GRAPHQL_BODY,
        "review_threads.json",
    ),
    "ListIssueComments": (
        "GET",
        "/repos/octo-org/review-repository/issues/17/comments",
        "page=1&per_page=100",
        REST_HEADERS,
        None,
        "issue_comments.json",
    ),
    "ListChecks": (
        "GET",
        (
            "/repos/octo-org/review-repository/commits/"
            "2222222222222222222222222222222222222222/check-runs"
        ),
        "page=1&per_page=100",
        REST_HEADERS,
        None,
        "checks.json",
    ),
    "SearchPullRequests": (
        "GET",
        "/search/issues",
        (
            "page=1&per_page=100&q=is%3Apr%20is%3Aopen%20"
            "repo%3Aocto-org%2Freview-repository"
        ),
        REST_HEADERS,
        None,
        "search.json",
    ),
}

EXPECTED_RESPONSE_FIELDS = {
    "GetViewerIdentity": {"id", "login", "name", "node_id"},
    "GetRepository": {
        "id",
        "name",
        "full_name",
        "owner",
        "login",
        "node_id",
        "private",
        "default_branch",
    },
    "GetPullRequest": {
        "id",
        "node_id",
        "number",
        "state",
        "title",
        "body",
        "draft",
        "user",
        "login",
        "base",
        "head",
        "ref",
        "sha",
        "repo",
        "full_name",
        "merged",
    },
    "ListPullRequestFiles": {
        "filename",
        "status",
        "additions",
        "deletions",
    },
    "ListPullRequestCommits": {
        "sha",
        "node_id",
        "commit",
        "message",
        "author",
        "login",
    },
    "ListReviews": {
        "node_id",
        "user",
        "login",
        "body",
        "state",
        "commit_id",
    },
    "ListReviewThreads": {
        "data",
        "repository",
        "pullRequest",
        "files",
        "viewerViewedState",
        "reviewThreads",
        "nodes",
        "id",
        "isResolved",
        "isOutdated",
        "viewerCanResolve",
        "viewerCanUnresolve",
        "subjectType",
        "path",
        "line",
        "originalLine",
        "comments",
        "bodyText",
        "author",
        "login",
        "replyTo",
        "pageInfo",
        "hasNextPage",
        "endCursor",
        "outdated",
    },
    "ListIssueComments": {
        "node_id",
        "user",
        "login",
        "body",
    },
    "ListChecks": {
        "total_count",
        "check_runs",
        "node_id",
        "name",
        "head_sha",
        "status",
        "conclusion",
    },
    "SearchPullRequests": {
        "total_count",
        "incomplete_results",
        "items",
        "node_id",
        "number",
        "state",
        "title",
        "draft",
    },
}

ALLOWED_URL_HOSTS = {
    "api.github.com",
    "docs.github.com",
    "github.com",
    "ghe.example.invalid",
}
FORBIDDEN_ARTIFACT_FIELDS = {
    "_links",
    "arguments",
    "authorization",
    "command",
    "cookie",
    "credentials",
    "cwd",
    "email",
    "environment",
    "headers",
    "local_path",
    "request",
    "request_headers",
    "response",
    "response_headers",
    "set-cookie",
    "stderr",
    "stdout",
    "url",
}
SYNTHETIC_CANARY = "BPR_FIXTURE_SECRET_CANARY_7F2C"


def expected_interactions(host: dict[str, object]) -> list[dict[str, object]]:
    result = []
    for operation in OPERATIONS:
        method, path, query, headers, body, response_name = (
            OPERATION_CONTRACTS[operation]
        )
        if operation == "ListReviewThreads":
            expected_path = host["graphql_path"]
        else:
            expected_path = f"{host['rest_prefix']}{path}"
        result.append(
            {
                "operation": operation,
                "method": method,
                "path": expected_path,
                "canonical_query": query,
                "safe_headers": headers,
                "body": body,
                "response_file": f"responses/{response_name}",
            }
        )
    return result


def manifest_violations(
    host_name: str,
    manifest: object,
) -> list[str]:
    if not isinstance(manifest, dict):
        return ["manifest must be an object"]

    host = HOSTS[host_name]
    expected = {
        "schema_revision": 1,
        "product": host["product"],
        "host": host["host"],
        "api_roots": host["api_roots"],
        "version": host["version"],
        "sanitized": True,
        "interactions": expected_interactions(host),
    }
    if manifest == expected:
        return []

    violations = []
    if set(manifest) != set(expected):
        violations.append("unexpected manifest fields")
    if manifest.get("sanitized") is not True:
        violations.append("manifest is not sanitized")
    if manifest.get("interactions") != expected["interactions"]:
        violations.append("unexpected interaction contract")
    for field in (
        "schema_revision",
        "product",
        "host",
        "api_roots",
        "version",
    ):
        if manifest.get(field) != expected[field]:
            violations.append(f"unexpected {field}")
    return violations


def json_fields(value: object) -> set[str]:
    fields: set[str] = set()
    if isinstance(value, dict):
        for key, nested in value.items():
            fields.add(key)
            fields.update(json_fields(nested))
    elif isinstance(value, list):
        for nested in value:
            fields.update(json_fields(nested))
    return fields


def artifact_field_violations(value: object) -> list[str]:
    violations = []
    for field in json_fields(value):
        lowered = field.lower()
        if (
            lowered in FORBIDDEN_ARTIFACT_FIELDS
            or lowered.endswith("_url")
            or any(
                fragment in lowered
                for fragment in ("password", "secret", "token", "credential")
            )
        ):
            violations.append(f"forbidden artifact field: {field}")
    return violations


def byte_security_violations(payload: bytes) -> list[str]:
    lowered = payload.lower()
    violations = []
    forbidden_fragments = (
        b"authorization",
        b"proxy-authorization",
        b"cookie",
        b"set-cookie",
        b"bearer ",
        b"basic ",
        b"github_pat_",
        b"ghp_",
        b"gho_",
        b"ghu_",
        b"ghs_",
        b"ghr_",
        b"access_token",
        b"refresh_token",
        b"client_secret",
        b"private_key",
        b"password",
        b"credential",
        b"secret",
    )
    for fragment in forbidden_fragments:
        if fragment in lowered:
            violations.append(
                f"forbidden byte pattern: {fragment.decode('ascii')}"
            )

    text = payload.decode("utf-8")
    if re.search(r"(?i)(?:/Users/|/home/|[A-Z]:\\+Users\\+)", text):
        violations.append("home-directory path")
    if re.search(
        r"(?i)\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}\b",
        text,
    ):
        violations.append("email address")
    if re.search(r"(?<![\d.])(?:\d{1,3}\.){3}\d{1,3}(?![\d.])", text):
        violations.append("IP address")

    for match in re.finditer(r"https?://[^\"\\\s]+", text):
        hostname = (urlsplit(match.group(0)).hostname or "").lower()
        if hostname not in ALLOWED_URL_HOSTS:
            violations.append(f"non-allowlisted URL host: {hostname}")
    return violations


def reject_duplicate_members(
    members: list[tuple[str, object]],
) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in members:
        if key in result:
            raise ValueError("duplicate JSON member")
        result[key] = value
    return result


def decoded_string_values(value: object) -> list[str]:
    values: list[str] = []
    if isinstance(value, dict):
        for key, nested in value.items():
            values.append(key)
            values.extend(decoded_string_values(nested))
    elif isinstance(value, list):
        for nested in value:
            values.extend(decoded_string_values(nested))
    elif isinstance(value, str):
        values.append(value)
    return values


def decoded_security_violations(value: object) -> list[str]:
    violations = []
    for text in decoded_string_values(value):
        violations.extend(byte_security_violations(text.encode("utf-8")))
    return violations


class GitHubContractFixtureSecurityTests(unittest.TestCase):
    def load_json(self, path: Path) -> object:
        value = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=reject_duplicate_members,
        )
        self.assertEqual(decoded_security_violations(value), [])
        return value

    def test_fixture_tree_is_exact_and_every_byte_is_sanitized(self) -> None:
        expected_files = set()
        for host_name in HOSTS:
            expected_files.add(f"{host_name}/manifest.json")
            for operation in OPERATIONS:
                response_name = OPERATION_CONTRACTS[operation][-1]
                expected_files.add(
                    f"{host_name}/responses/{response_name}"
                )

        actual_files = {
            str(path.relative_to(FIXTURE_ROOT))
            for path in FIXTURE_ROOT.rglob("*")
            if path.is_file()
        }
        self.assertEqual(actual_files, expected_files)

        for relative_path in sorted(actual_files):
            with self.subTest(path=relative_path):
                payload = (FIXTURE_ROOT / relative_path).read_bytes()
                self.assertTrue(payload.endswith(b"\n"))
                self.assertEqual(
                    byte_security_violations(payload),
                    [],
                )

    def test_capability_fixture_tree_is_exact_and_sanitized(self) -> None:
        expected_files = {
            "github-com/manifest.json",
            "github-com/supported.json",
            "ghes-3.18/manifest.json",
            "ghes-3.18/supported.json",
            "synthetic/incompatible-mark.json",
            "synthetic/missing-unresolve.json",
        }
        actual_files = {
            str(path.relative_to(CAPABILITY_FIXTURE_ROOT))
            for path in CAPABILITY_FIXTURE_ROOT.rglob("*")
            if path.is_file()
        }
        self.assertEqual(actual_files, expected_files)

        root = CAPABILITY_FIXTURE_ROOT.resolve()
        for relative_path in sorted(actual_files):
            with self.subTest(path=relative_path):
                path = CAPABILITY_FIXTURE_ROOT / relative_path
                payload = path.read_bytes()
                self.assertTrue(payload.endswith(b"\n"))
                self.assertEqual(byte_security_violations(payload), [])
                document = self.load_json(path)
                self.assertIsInstance(document, dict)
                for reference_name in (
                    "response_file",
                    "base_response_file",
                ):
                    reference = document.get(reference_name)
                    if reference is None:
                        continue
                    self.assertIsInstance(reference, str)
                    target = (path.parent / reference).resolve()
                    self.assertTrue(target.is_relative_to(root))
                    self.assertTrue(target.is_file())

    def test_capability_manifests_pin_independent_official_schemas(
        self,
    ) -> None:
        expected = {
            "github-com": {
                "product": "github.com",
                "version": "cloud",
                "source_url": (
                    "https://docs.github.com/public/fpt/"
                    "schema.docs.graphql"
                ),
                "retrieved_at_utc": "2026-07-30T03:58:23Z",
                "last_modified": "Mon, 27 Jul 2026 14:39:03 GMT",
                "source_sha256": (
                    "f7a98392e2281c215810c5ac648c3a6940bcd49db"
                    "74beb2502143f244fdb4da3"
                ),
                "response_sha256": (
                    "85a692f13e121c130d997454795a0601440b3b03d"
                    "ca5629e0892b451738b6bc3"
                ),
                "mutation_fields": 271,
                "pull_request_fields": 113,
            },
            "ghes-3.18": {
                "product": "github-enterprise-server",
                "version": "3.18",
                "source_url": (
                    "https://docs.github.com/public/ghes-3.18/"
                    "schema.docs-enterprise.graphql"
                ),
                "retrieved_at_utc": "2026-07-30T03:58:23Z",
                "last_modified": "Wed, 29 Jul 2026 23:22:15 GMT",
                "source_sha256": (
                    "41179787f224304625d0e31c4e4928380b013ffe"
                    "3f02bbabfec17c3c1320b066"
                ),
                "response_sha256": (
                    "cc2d5829ce4c75c137f79969b7270034067c753c"
                    "fb8dc66b29ce42a0e211774c"
                ),
                "mutation_fields": 224,
                "pull_request_fields": 109,
            },
        }
        query_sha256 = (
            "839ec85171a2d9fb21e23a50890723331e5944725"
            "2953a7de16f3847d5be6730"
        )

        responses = []
        for host_name, contract in expected.items():
            with self.subTest(host=host_name):
                host_root = CAPABILITY_FIXTURE_ROOT / host_name
                manifest = self.load_json(host_root / "manifest.json")
                self.assertEqual(
                    set(manifest),
                    {
                        "schema_revision",
                        "product",
                        "version",
                        "response_file",
                        "source",
                        "projection",
                        "expected",
                    },
                )
                self.assertEqual(
                    set(manifest["source"]),
                    {
                        "url",
                        "retrieved_at_utc",
                        "last_modified",
                        "sha256",
                    },
                )
                self.assertEqual(
                    set(manifest["projection"]),
                    {
                        "generator",
                        "query_sha256",
                        "response_sha256",
                        "mutation_fields",
                        "pull_request_fields",
                    },
                )
                self.assertEqual(manifest["schema_revision"], 1)
                self.assertEqual(manifest["product"], contract["product"])
                self.assertEqual(manifest["version"], contract["version"])
                self.assertEqual(manifest["response_file"], "supported.json")
                self.assertEqual(
                    manifest["source"]["url"],
                    contract["source_url"],
                )
                self.assertEqual(
                    manifest["source"]["retrieved_at_utc"],
                    contract["retrieved_at_utc"],
                )
                self.assertEqual(
                    manifest["source"]["last_modified"],
                    contract["last_modified"],
                )
                self.assertEqual(
                    manifest["source"]["sha256"],
                    contract["source_sha256"],
                )
                self.assertEqual(
                    manifest["projection"]["generator"],
                    "graphql-js 16.11.0 buildSchema/graphqlSync",
                )
                self.assertEqual(
                    manifest["projection"]["query_sha256"],
                    query_sha256,
                )
                self.assertEqual(
                    manifest["projection"]["mutation_fields"],
                    contract["mutation_fields"],
                )
                self.assertEqual(
                    manifest["projection"]["pull_request_fields"],
                    contract["pull_request_fields"],
                )
                self.assertEqual(
                    manifest["expected"],
                    {
                        "review_threads": "supported",
                        "viewed_file_mutations": "supported",
                        "resolve_thread": "supported",
                        "unresolve_thread": "supported",
                    },
                )

                response = (host_root / "supported.json").read_bytes()
                self.assertEqual(
                    hashlib.sha256(response).hexdigest(),
                    contract["response_sha256"],
                )
                self.assertEqual(
                    manifest["projection"]["response_sha256"],
                    contract["response_sha256"],
                )
                responses.append(response)

        self.assertNotEqual(
            responses[0],
            responses[1],
            "GitHub.com and GHES evidence must be independent",
        )

    def test_manifests_pin_exact_read_only_contracts(self) -> None:
        for host_name in HOSTS:
            with self.subTest(host=host_name):
                manifest = self.load_json(
                    FIXTURE_ROOT / host_name / "manifest.json"
                )
                self.assertEqual(
                    manifest_violations(host_name, manifest),
                    [],
                )
                interactions = manifest["interactions"]
                self.assertEqual(
                    [item["operation"] for item in interactions],
                    list(OPERATIONS),
                )
                self.assertNotIn("mutation", str(interactions).lower())

    def test_responses_have_exact_fields_and_equivalent_semantics(self) -> None:
        for operation in OPERATIONS:
            response_name = OPERATION_CONTRACTS[operation][-1]
            cloud_path = (
                FIXTURE_ROOT / "github-com" / "responses" / response_name
            )
            ghes_path = (
                FIXTURE_ROOT / "ghes-3.18" / "responses" / response_name
            )
            with self.subTest(operation=operation):
                self.assertEqual(cloud_path.read_bytes(), ghes_path.read_bytes())
                response = self.load_json(cloud_path)
                self.assertEqual(
                    json_fields(response),
                    EXPECTED_RESPONSE_FIELDS[operation],
                )
                self.assertEqual(artifact_field_violations(response), [])

    def test_stable_fixture_identities_are_synthetic_and_bounded(self) -> None:
        pull_request = self.load_json(
            FIXTURE_ROOT
            / "github-com"
            / "responses"
            / "pull_request.json"
        )
        self.assertEqual(
            pull_request["base"]["repo"]["full_name"],
            "octo-org/review-repository",
        )
        self.assertEqual(
            pull_request["head"]["repo"]["full_name"],
            "octo-org/review-repository",
        )
        for oid in (
            pull_request["base"]["sha"],
            pull_request["head"]["sha"],
        ):
            self.assertRegex(oid, r"^([1-9])\1{39}$")

    def test_validator_rejects_unsanitized_manifest(self) -> None:
        manifest = self.load_json(
            FIXTURE_ROOT / "github-com" / "manifest.json"
        )
        manifest["sanitized"] = False
        self.assertIn(
            "manifest is not sanitized",
            manifest_violations("github-com", manifest),
        )

    def test_validator_rejects_credentials_and_artifact_fields(self) -> None:
        malicious_response = {
            "html_url": "https://github.com/octo-org/review-repository",
            "Authorization": "Bearer fixture-value",
        }
        self.assertTrue(artifact_field_violations(malicious_response))
        self.assertTrue(
            byte_security_violations(
                json.dumps(malicious_response).encode("utf-8")
            )
        )

    def test_byte_scanner_rejects_each_sensitive_pattern_class(self) -> None:
        unsafe_payloads = {
            "home": b'{"path":"/Users/octo/private"}',
            "email": b'{"identity":"octo@example.com"}',
            "ip": b'{"host":"192.0.2.10"}',
            "foreign_host": b'{"root":"https://outside.example/api"}',
        }
        for pattern_class, payload in unsafe_payloads.items():
            with self.subTest(pattern_class=pattern_class):
                self.assertTrue(byte_security_violations(payload))

    def test_byte_scanner_detects_synthetic_canary(self) -> None:
        payload = json.dumps({"value": SYNTHETIC_CANARY}).encode("utf-8")
        violations = byte_security_violations(payload)
        self.assertTrue(violations)
        self.assertTrue(
            any("secret" in violation for violation in violations),
            violations,
        )

    def test_decoded_escape_bypasses_are_rejected(self) -> None:
        unsafe_payloads = (
            r'{"value":"ghp\u005f1234567890"}',
            r'{"value":"octo\u0040private.example"}',
            r'{"value":"https:\/\/outside.example\/api"}',
            r'{"value":"\/Us\u0065rs\/private\/repo"}',
        )
        for payload in unsafe_payloads:
            with self.subTest(payload=payload):
                value = json.loads(
                    payload,
                    object_pairs_hook=reject_duplicate_members,
                )
                self.assertTrue(decoded_security_violations(value))

    def test_duplicate_json_members_are_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "duplicate JSON member"):
            json.loads(
                '{"id":1,"id":2}',
                object_pairs_hook=reject_duplicate_members,
            )


if __name__ == "__main__":
    unittest.main()
