#!/usr/bin/env python3
"""Hermetic tests for the RFC-BPR-0017 anonymous contract recorder.

Nothing here touches a network. The one test that would notice if the module
did is `test_validate_existing_cannot_reach_a_network`, which replaces the sole
request function with one that fails the test if it is ever called.
"""
from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

MINIMUM_PYTHON = (3, 10)
if sys.version_info < MINIMUM_PYTHON:
    sys.stderr.write(
        "record_rfc0017_anonymous_contracts_tests.py requires Python {}.{} or "
        "newer; this is {}.{}.{} at {}.\n".format(
            MINIMUM_PYTHON[0], MINIMUM_PYTHON[1], sys.version_info[0],
            sys.version_info[1], sys.version_info[2], sys.executable))
    raise SystemExit(2)

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))

from scripts import record_rfc0017_anonymous_contracts as REC  # noqa: E402
from scripts.contained_temporary_support import ContainedTemporaryDirectory  # noqa: E402

STATEMENT = (
    "I authorize exactly the credential-free read-only requests named below "
    "against api.github.com.")


def authority(targets=None, *, statement=STATEMENT, version=1, schema=None):
    return {
        "schema_name": schema or REC.AUTHORITY_SCHEMA,
        "schema_version": version,
        "statement": statement,
        "targets": targets if targets is not None else [
            {
                "name": "rate-limit",
                "operation": "rest_rate_limit",
                "url": "https://api.github.com/rate_limit",
            },
        ],
    }


def packet_text(auth=None) -> str:
    block = json.dumps(auth if auth is not None else authority(), indent=2)
    return (
        "# RFC-BPR-0017 anonymous probe operator packet\n\n"
        "Status: **pending operator decision**\n\n"
        "## Authority\n\n"
        f"```json\n{block}\n```\n"
    )


class AuthorityTests(unittest.TestCase):
    def test_accepts_a_well_formed_authority(self) -> None:
        parsed = REC.parse_authority(packet_text().encode())
        self.assertEqual(parsed["targets"][0]["name"], "rate-limit")

    def test_rejects_a_packet_with_no_authority_block(self) -> None:
        with self.assertRaisesRegex(REC.ContractError, "exactly one authority"):
            REC.parse_authority(b"# packet\n\nno block here\n")

    def test_rejects_a_packet_with_two_authority_blocks(self) -> None:
        doubled = packet_text() + packet_text()
        with self.assertRaisesRegex(REC.ContractError, "exactly one authority"):
            REC.parse_authority(doubled.encode())

    def test_rejects_a_schema_version_it_does_not_know(self) -> None:
        with self.assertRaisesRegex(REC.ContractError, "schema version"):
            REC.parse_authority(packet_text(authority(version=99)).encode())

    def test_rejects_a_repeated_target(self) -> None:
        target = authority()["targets"][0]
        with self.assertRaisesRegex(REC.ContractError, "repeats target"):
            REC.parse_authority(
                packet_text(authority([target, dict(target)])).encode())

    def test_rejects_more_targets_than_the_bound(self) -> None:
        many = [
            {"name": f"t{index}", "operation": "rest_rate_limit",
             "url": "https://api.github.com/rate_limit"}
            for index in range(REC.MAX_TARGETS + 1)
        ]
        with self.assertRaisesRegex(REC.ContractError, "too many targets"):
            REC.parse_authority(packet_text(authority(many)).encode())


class AllowlistTests(unittest.TestCase):
    """The authority names targets within the outer bounds; it cannot widen them."""

    def refuse(self, url, operation="rest_rate_limit"):
        with self.assertRaises(REC.ContractError):
            REC.validate_url(url, operation, "target")

    def test_refuses_another_host(self) -> None:
        self.refuse("https://api.github.example.com/rate_limit")

    def test_refuses_plain_http(self) -> None:
        self.refuse("http://api.github.com/rate_limit")

    def test_refuses_an_explicit_port(self) -> None:
        self.refuse("https://api.github.com:8443/rate_limit")

    def test_refuses_an_embedded_credential(self) -> None:
        self.refuse("https://user:secret@api.github.com/rate_limit")

    def test_refuses_a_fragment(self) -> None:
        self.refuse("https://api.github.com/rate_limit#x")

    def test_refuses_get_against_graphql(self) -> None:
        self.refuse("https://api.github.com/graphql", "rest_rate_limit")

    def test_refuses_post_away_from_graphql(self) -> None:
        self.refuse(
            "https://api.github.com/repos/o/r/pulls/1",
            "graphql_unauthenticated_refusal")

    def test_accepts_post_to_graphql_only(self) -> None:
        REC.validate_url(
            "https://api.github.com/graphql",
            "graphql_unauthenticated_refusal", "target")

    def test_refuses_an_unknown_operation(self) -> None:
        with self.assertRaisesRegex(REC.ContractError, "is not allowed"):
            REC.validate_target(
                {"name": "x", "operation": "rest_delete_everything",
                 "url": "https://api.github.com/rate_limit"}, "target")


class SanitizationTests(unittest.TestCase):
    def capture(self, **overrides):
        base = dict(
            name="rate-limit", operation="rest_rate_limit",
            url="https://api.github.com/rate_limit", http_status=200,
            headers={"Content-Type": "application/json"},
            body=b'{"rate":{"limit":60}}', authority_sha256="0" * 64,
            source_kind="live")
        base.update(overrides)
        return REC.sanitize_capture(**base)

    def test_drops_headers_outside_the_retained_set(self) -> None:
        record = self.capture(headers={
            "Content-Type": "application/json",
            "X-RateLimit-Remaining": "59",
            "Set-Cookie": "session=abc",
            "Server": "GitHub.com",
        })
        self.assertEqual(
            sorted(record["retained_headers"]),
            ["content-type", "x-ratelimit-remaining"])

    def test_refuses_a_body_carrying_a_token(self) -> None:
        with self.assertRaisesRegex(REC.ContractError, "github token"):
            self.capture(body=b'{"note":"ghp_' + b"a" * 20 + b'"}')

    def test_refuses_a_body_that_is_not_json(self) -> None:
        with self.assertRaisesRegex(REC.ContractError, "not JSON"):
            self.capture(body=b"<html>nope</html>")

    def test_refuses_an_oversized_body(self) -> None:
        with self.assertRaisesRegex(REC.ContractError, "byte limit"):
            self.capture(body=b"0" * (REC.MAX_RESPONSE_BYTES + 1))

    def test_body_is_canonically_reserialized(self) -> None:
        record = self.capture(body=b'{"b":2,"a":1}')
        self.assertEqual(record["body"], '{"a":1,"b":2}\n')

    def test_records_the_digest_of_the_bytes_received(self) -> None:
        record = self.capture(body=b'{"a":1}')
        self.assertEqual(record["body_sha256"], REC.sha256_bytes(b'{"a":1}'))


class ForbiddenByteTests(unittest.TestCase):
    def test_each_pattern_is_load_bearing(self) -> None:
        samples = {
            "authorization header": b"Authorization: x",
            "bearer token": b"bearer abcdefghijkl",
            "basic credential": b"basic YWJjZGVmZ2hp",
            "github token": b"ghp_" + b"a" * 20,
            "private key header": b"-----BEGIN RSA PRIVATE KEY-----",
            "cookie header": b"Set-Cookie: a=b",
            "embedded credential url": b"https://u:p@example.com/",
        }
        for name, payload in samples.items():
            with self.subTest(pattern=name):
                with self.assertRaisesRegex(REC.ContractError, name):
                    REC.assert_no_forbidden_bytes(payload, "sample")

    def test_every_github_token_prefix_is_covered_by_the_one_rule(self) -> None:
        # ghp_, gho_, ghu_, ghs_, ghr_ all match the single token pattern. A
        # separate ghs_ rule was tried and removed because it could never fire.
        for prefix in (b"ghp_", b"gho_", b"ghu_", b"ghs_", b"ghr_"):
            with self.subTest(prefix=prefix.decode()):
                with self.assertRaisesRegex(REC.ContractError, "github token"):
                    REC.assert_no_forbidden_bytes(prefix + b"a" * 20, "sample")

    def test_benign_content_passes(self) -> None:
        REC.assert_no_forbidden_bytes(b'{"rate":{"limit":60}}', "sample")


class OutputContainmentTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = ContainedTemporaryDirectory()
        self.review = Path(self.temporary.name) / "review"
        self.review.mkdir()
        self.packet = self.review / "operator-packet.md"
        self.packet.write_text(packet_text())
        self.addCleanup(self.temporary.cleanup)

    def test_accepts_a_directory_beneath_the_review_directory(self) -> None:
        resolved = REC.resolve_output_root(self.review / "captures", self.packet)
        self.assertEqual(resolved.name, "captures")

    def test_refuses_the_review_directory_itself(self) -> None:
        with self.assertRaisesRegex(REC.ContractError, "beneath"):
            REC.resolve_output_root(self.review, self.packet)

    def test_refuses_a_directory_outside_the_review_directory(self) -> None:
        with self.assertRaisesRegex(REC.ContractError, "beneath"):
            REC.resolve_output_root(
                Path(self.temporary.name) / "elsewhere", self.packet)

    def test_refuses_a_traversal(self) -> None:
        with self.assertRaisesRegex(REC.ContractError, "beneath"):
            REC.resolve_output_root(self.review / ".." / "escape", self.packet)


class NoTrafficTests(unittest.TestCase):
    """`--validate-existing` must not be able to reach a network."""

    def setUp(self) -> None:
        self.temporary = ContainedTemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.review = Path(self.temporary.name) / "review"
        self.review.mkdir()
        self.packet = self.review / "operator-packet.md"
        self.packet.write_text(packet_text())
        self.response = self.review / "operator-response.md"
        self.response.write_text(f"> {STATEMENT}\n")
        auth = REC.parse_authority(self.packet.read_bytes())
        self.authority_sha = REC.sha256_bytes(REC.canonical_json(auth))
        self.manifest = self.review / "course-manifest.json"
        self.manifest.write_bytes(REC.canonical_json({
            "schema_name": REC.MANIFEST_SCHEMA,
            "schema_version": REC.MANIFEST_SCHEMA_VERSION,
            "packet_sha256": REC.sha256_bytes(self.packet.read_bytes()),
            "courses": [{"name": "rate-limit", "expected_status": 200}],
        }))
        self.captures = self.review / "captures"
        self.captures.mkdir()
        record = REC.sanitize_capture(
            name="rate-limit", operation="rest_rate_limit",
            url="https://api.github.com/rate_limit", http_status=200,
            headers={"Content-Type": "application/json"},
            body=b'{"rate":{"limit":60}}',
            authority_sha256=self.authority_sha, source_kind="live")
        (self.captures / "rate-limit.json").write_bytes(REC.canonical_json(record))

    def argv(self):
        return [
            "--packet", str(self.packet),
            "--response", str(self.response),
            "--course-manifest", str(self.manifest),
            "--output-root", str(self.captures),
            "--validate-existing",
        ]

    def test_validate_existing_accepts_a_consistent_set(self) -> None:
        self.assertEqual(REC.main(self.argv()), 0)

    def test_validate_existing_cannot_reach_a_network(self) -> None:
        original = REC.perform_request

        def explode(*args, **kwargs):
            raise AssertionError("validate-existing issued a request")

        REC.perform_request = explode
        self.addCleanup(lambda: setattr(REC, "perform_request", original))
        self.assertEqual(REC.main(self.argv()), 0)

    def test_refuses_a_capture_recorded_under_another_authority(self) -> None:
        path = self.captures / "rate-limit.json"
        record = json.loads(path.read_bytes())
        record["authority_sha256"] = "1" * 64
        path.write_bytes(REC.canonical_json(record))
        self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_a_capture_whose_status_contradicts_its_course(self) -> None:
        path = self.captures / "rate-limit.json"
        record = json.loads(path.read_bytes())
        record["http_status"] = 404
        path.write_bytes(REC.canonical_json(record))
        self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_a_capture_that_is_not_canonically_serialized(self) -> None:
        path = self.captures / "rate-limit.json"
        record = json.loads(path.read_bytes())
        path.write_bytes(json.dumps(record, indent=4).encode())
        self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_a_manifest_that_binds_another_packet(self) -> None:
        manifest = json.loads(self.manifest.read_bytes())
        manifest["packet_sha256"] = "2" * 64
        self.manifest.write_bytes(REC.canonical_json(manifest))
        self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_a_response_missing_the_statement(self) -> None:
        self.response.write_text("> I approve something else entirely.\n")
        self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_a_course_naming_an_unauthorized_target(self) -> None:
        manifest = json.loads(self.manifest.read_bytes())
        manifest["courses"] = [{"name": "not-authorized", "expected_status": 200}]
        self.manifest.write_bytes(REC.canonical_json(manifest))
        self.assertEqual(REC.main(self.argv()), 3)


if __name__ == "__main__":
    unittest.main()
