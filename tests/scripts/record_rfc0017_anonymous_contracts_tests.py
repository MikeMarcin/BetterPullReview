#!/usr/bin/env python3
"""Hermetic tests for the RFC-BPR-0017 anonymous contract recorder.

Nothing here touches a network. The one test that would notice if the module
did is `test_validate_existing_cannot_reach_a_network`, which replaces the sole
request function with one that fails the test if it is ever called.
"""
from __future__ import annotations

import json
import os
import stat
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

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
GRAPHQL_HEADERS = {
    "Content-Type": "application/json",
    "X-RateLimit-Resource": "graphql",
    "X-RateLimit-Limit": "0",
    "X-RateLimit-Remaining": "0",
    "X-RateLimit-Used": "0",
}


def synthetic_graphql_body(address="192.0.2.42", **extra) -> bytes:
    body = {
        "message": f"Synthetic API rate limit exceeded for {address}.",
    }
    body.update(extra)
    return REC.canonical_json(body)


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
    recorder_bytes = Path(REC.__file__).read_bytes()
    return (
        "# RFC-BPR-0017 anonymous probe operator packet\n\n"
        "Status: **pending operator decision**\n\n"
        "## Authority\n\n"
        f"```json\n{block}\n```\n\n"
        "## Bound evidence\n\n"
        "| Artifact | SHA-256 | Bytes |\n"
        "|---|---|---:|\n"
        f"| `{REC.RECORDER_REPOSITORY_PATH}` | "
        f"`sha256:{REC.sha256_bytes(recorder_bytes)}` | "
        f"{len(recorder_bytes)} |\n"
    )


def response_text(packet_bytes: bytes, decision=STATEMENT) -> str:
    return REC.exact_capture_response(packet_bytes, decision).decode("utf-8")


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

    def test_rejects_non_integer_authority_schema_versions(self) -> None:
        for version in (True, 1.0):
            with self.subTest(version=version):
                with self.assertRaisesRegex(REC.ContractError, "not an integer"):
                    REC.parse_authority(
                        packet_text(authority(version=version)).encode())

    def test_rejects_duplicate_authority_members(self) -> None:
        packet = packet_text().replace(
            '"schema_version": 1,',
            '"schema_version": 1,\n  "schema_version": 1,',
            1,
        )
        with self.assertRaisesRegex(REC.ContractError, "duplicate JSON key"):
            REC.parse_authority(packet.encode())

    def test_capture_binding_rejects_other_recorder_bytes(self) -> None:
        packet = packet_text().replace(
            REC.sha256_bytes(Path(REC.__file__).read_bytes()), "f" * 64)
        with self.assertRaisesRegex(REC.ContractError, "current recorder"):
            REC.validate_current_recorder_binding(packet.encode())

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

    def graphql_capture(self, **overrides):
        base = dict(
            name="graphql-refusal",
            operation=REC.GRAPHQL_REFUSAL_OPERATION,
            url="https://api.github.com/graphql",
            http_status=403,
            headers=GRAPHQL_HEADERS,
            body=synthetic_graphql_body(),
            authority_sha256="0" * 64,
            source_kind="synthetic",
        )
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

    def test_other_operations_retain_duplicate_key_behavior(self) -> None:
        raw_body = b'{"a":1,"a":2}'
        record = self.capture(body=raw_body)

        self.assertEqual(record["body"], '{"a":2}\n')
        self.assertEqual(record["body_sha256"], REC.sha256_bytes(raw_body))

    def test_records_the_digest_of_the_bytes_received(self) -> None:
        record = self.capture(body=b'{"a":1}')
        self.assertEqual(record["body_sha256"], REC.sha256_bytes(b'{"a":1}'))

    def test_normalizes_one_synthetic_ipv4_caller_address(self) -> None:
        raw_body = synthetic_graphql_body("192.0.2.42", stable="retained")
        record = self.graphql_capture(body=raw_body)
        persisted = REC.canonical_json(record)

        self.assertTrue(record[REC.CALLER_ADDRESS_NORMALIZED_FIELD])
        self.assertIn(REC.CALLER_ADDRESS_PLACEHOLDER, record["body"])
        self.assertIn('"stable":"retained"', record["body"])
        self.assertNotIn(b"192.0.2.42", persisted)
        self.assertNotIn(REC.sha256_bytes(raw_body).encode(), persisted)
        self.assertEqual(
            record["body_sha256"],
            REC.sha256_bytes(record["body"].encode("utf-8")),
        )

    def test_normalizes_one_synthetic_ipv6_caller_address(self) -> None:
        raw_body = synthetic_graphql_body("2001:db8::42")
        record = self.graphql_capture(body=raw_body)
        persisted = REC.canonical_json(record)

        self.assertIn(REC.CALLER_ADDRESS_PLACEHOLDER, record["body"])
        self.assertNotIn(b"2001:db8::42", persisted)
        self.assertNotIn(REC.sha256_bytes(raw_body).encode(), persisted)

    def test_does_not_normalize_an_address_for_another_operation(self) -> None:
        raw_body = synthetic_graphql_body()
        record = self.capture(body=raw_body)

        self.assertIn("192.0.2.42", record["body"])
        self.assertNotIn(REC.CALLER_ADDRESS_NORMALIZED_FIELD, record)
        self.assertEqual(record["body_sha256"], REC.sha256_bytes(raw_body))

    def test_refuses_a_graphql_shape_with_the_wrong_status(self) -> None:
        with self.assertRaisesRegex(REC.ContractError, "status is not 403"):
            self.graphql_capture(http_status=401)

    def test_refuses_wrong_or_missing_graphql_quota_headers(self) -> None:
        for header in REC.GRAPHQL_ZERO_QUOTA_HEADERS:
            with self.subTest(header=header, defect="missing"):
                headers = {
                    name: value for name, value in GRAPHQL_HEADERS.items()
                    if name.lower() != header
                }
                with self.assertRaisesRegex(REC.ContractError, header):
                    self.graphql_capture(headers=headers)
            with self.subTest(header=header, defect="wrong"):
                headers = dict(GRAPHQL_HEADERS)
                source_name = next(
                    name for name in headers if name.lower() == header)
                headers[source_name] = "wrong"
                with self.assertRaisesRegex(REC.ContractError, header):
                    self.graphql_capture(headers=headers)

    def test_refuses_a_missing_or_nontext_graphql_message(self) -> None:
        for body in (REC.canonical_json({}), REC.canonical_json({"message": 7})):
            with self.subTest(body=body):
                with self.assertRaisesRegex(REC.ContractError, "message is not text"):
                    self.graphql_capture(body=body)

    def test_refuses_a_graphql_message_without_an_address(self) -> None:
        body = REC.canonical_json({
            "message": "Synthetic API rate limit exceeded for this caller.",
        })
        with self.assertRaisesRegex(REC.ContractError, "exactly one"):
            self.graphql_capture(body=body)

    def test_refuses_an_invalid_address_token(self) -> None:
        with self.assertRaisesRegex(REC.ContractError, "malformed address"):
            self.graphql_capture(
                body=synthetic_graphql_body("999.999.999.999"))

    def test_refuses_an_address_with_an_unsupported_boundary(self) -> None:
        for address in (
            "192.0.2.42/24",
            "2001:db8::42%eth0",
            "192.0.2.42x",
            "192.0.2.42.x",
            "x192.0.2.42",
            "2001:db8::g",
            "2001:db8::1zz",
        ):
            with self.subTest(address=address):
                with self.assertRaisesRegex(
                    REC.ContractError, "unsupported address boundary"
                ):
                    self.graphql_capture(body=synthetic_graphql_body(address))

    def test_refuses_multiple_or_residual_addresses(self) -> None:
        bodies = (
            REC.canonical_json({
                "message": (
                    "Synthetic API rate limit exceeded for 192.0.2.42 and "
                    "2001:db8::42."),
            }),
            synthetic_graphql_body(
                "192.0.2.42", residual="2001:db8::42"),
        )
        for body in bodies:
            with self.subTest(body=body):
                with self.assertRaisesRegex(REC.ContractError, "exactly one"):
                    self.graphql_capture(body=body)

    def test_refuses_duplicate_json_keys_before_normalization(self) -> None:
        bodies = (
            b'{"message":"Synthetic rate limit for 192.0.2.42.",'
            b'"message":"Synthetic rate limit for 2001:db8::42."}',
            b'{"message":"Synthetic rate limit for 192.0.2.42.",'
            b'"residual":"2001:db8::42","residual":"safe"}',
            b'{"message":"Synthetic rate limit for 192.0.2.42.",'
            b'"nested":{"residual":"2001:db8::42",'
            b'"residual":"safe"}}',
        )
        for body in bodies:
            with self.subTest(body=body):
                with self.assertRaisesRegex(
                    REC.ContractError, "duplicate JSON key"
                ):
                    self.graphql_capture(body=body)

        private_key = "2001:db8::42"
        body = (
            '{"message":"Synthetic rate limit for 192.0.2.42.",'
            f'"{private_key}":1,"{private_key}":2}}\n'
        ).encode("utf-8")
        with self.assertRaisesRegex(
            REC.ContractError, "duplicate JSON key"
        ) as raised:
            self.graphql_capture(body=body)
        self.assertNotIn(private_key, str(raised.exception))

    def test_refuses_a_preexisting_normalization_placeholder(self) -> None:
        bodies = (
            synthetic_graphql_body(
                "192.0.2.42",
                note=REC.CALLER_ADDRESS_PLACEHOLDER,
            ),
            REC.canonical_json({
                "message": (
                    "Synthetic rate limit "
                    f"{REC.CALLER_ADDRESS_PLACEHOLDER} for 192.0.2.42."
                ),
            }),
        )
        for body in bodies:
            with self.subTest(body=body):
                with self.assertRaisesRegex(REC.ContractError, "placeholder"):
                    self.graphql_capture(body=body)


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

    def test_accepts_another_safe_descendant_output_directory(self) -> None:
        resolved = REC.resolve_output_root(
            self.review / "captures-reuse", self.packet)
        self.assertEqual(resolved.name, "captures-reuse")

    def test_refuses_a_traversal(self) -> None:
        with self.assertRaisesRegex(REC.ContractError, "beneath"):
            REC.resolve_output_root(self.review / ".." / "escape", self.packet)


class RequestBoundaryTests(unittest.TestCase):
    def test_opener_disables_proxies_and_refuses_redirects(self) -> None:
        import urllib.request

        with patch.dict(os.environ, {
            "HTTPS_PROXY": "http://fixture-user:fixture-pass@127.0.0.1:9",
            "https_proxy": "http://fixture-user:fixture-pass@127.0.0.1:9",
        }):
            opener = REC.build_bounded_opener()
        proxy_handlers = [
            handler for handler in opener.handlers
            if isinstance(handler, urllib.request.ProxyHandler)
        ]
        redirect_handlers = [
            handler for handler in opener.handlers
            if isinstance(handler, urllib.request.HTTPRedirectHandler)
        ]
        self.assertEqual(proxy_handlers, [])
        self.assertEqual(len(redirect_handlers), 1)

        request = urllib.request.Request(
            "https://api.github.com/graphql", method="POST")
        with self.assertRaisesRegex(REC.ContractError, "redirected request"):
            redirect_handlers[0].redirect_request(
                request, None, 302, "Found", {}, "https://example.com/")

    def test_perform_request_uses_the_bounded_opener(self) -> None:
        class FakeResponse:
            status = 403
            headers = {}

            def __enter__(self):
                return self

            def __exit__(self, *args):
                return False

            def read(self, limit):
                return b"{}"

        class FakeOpener:
            request = None

            def open(self, request, timeout):
                self.request = request
                return FakeResponse()

        opener = FakeOpener()
        original = REC.build_bounded_opener
        REC.build_bounded_opener = lambda: opener
        self.addCleanup(lambda: setattr(REC, "build_bounded_opener", original))

        status, _headers, body = REC.perform_request(
            "https://api.github.com/graphql",
            REC.GRAPHQL_REFUSAL_OPERATION,
            None,
        )

        self.assertEqual((status, body), (403, b"{}"))
        self.assertEqual(opener.request.host, "api.github.com")
        self.assertEqual(opener.request.get_method(), "POST")
        self.assertNotIn(
            "proxy-authorization",
            {name.lower() for name, _ in opener.request.header_items()},
        )


class NoTrafficTests(unittest.TestCase):
    """`--validate-existing` must not be able to reach a network."""

    def setUp(self) -> None:
        self.temporary = ContainedTemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.original_attempt_registry = REC.CAPTURE_ATTEMPT_REGISTRY
        REC.CAPTURE_ATTEMPT_REGISTRY = (
            Path(self.temporary.name) / "capture-attempts")
        self.addCleanup(setattr, REC, "CAPTURE_ATTEMPT_REGISTRY",
                        self.original_attempt_registry)
        self.review = Path(self.temporary.name) / "review"
        self.review.mkdir()
        self.packet = self.review / "operator-packet.md"
        self.packet.write_text(packet_text())
        self.response = self.review / "operator-response.md"
        self.response.write_text(response_text(self.packet.read_bytes()))
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

    def test_refuses_a_normalization_marker_on_another_operation(self) -> None:
        path = self.captures / "rate-limit.json"
        record = json.loads(path.read_bytes())
        record[REC.CALLER_ADDRESS_NORMALIZED_FIELD] = True
        path.write_bytes(REC.canonical_json(record))
        self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_a_manifest_that_binds_another_packet(self) -> None:
        manifest = json.loads(self.manifest.read_bytes())
        manifest["packet_sha256"] = "2" * 64
        self.manifest.write_bytes(REC.canonical_json(manifest))
        self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_non_integer_manifest_schema_versions(self) -> None:
        for version in (True, 1.0):
            with self.subTest(version=version):
                manifest = json.loads(self.manifest.read_bytes())
                manifest["schema_version"] = version
                self.manifest.write_bytes(REC.canonical_json(manifest))
                self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_duplicate_manifest_members(self) -> None:
        manifest = self.manifest.read_text().replace(
            '"schema_version":1}',
            '"schema_version":1,"schema_version":1}',
            1,
        )
        self.manifest.write_text(manifest)
        self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_a_response_missing_the_statement(self) -> None:
        self.response.write_text(response_text(
            self.packet.read_bytes(), "I approve something else entirely."))
        self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_a_response_bound_to_another_packet(self) -> None:
        response = response_text(self.packet.read_bytes()).replace(
            REC.sha256_bytes(self.packet.read_bytes()), "f" * 64)
        self.response.write_text(response)
        self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_stale_recorder_binding_before_any_request(self) -> None:
        current_digest = REC.sha256_bytes(Path(REC.__file__).read_bytes())
        packet = self.packet.read_text().replace(current_digest, "f" * 64)
        self.packet.write_text(packet)
        self.response.write_text(response_text(self.packet.read_bytes()))
        manifest = json.loads(self.manifest.read_bytes())
        manifest["packet_sha256"] = REC.sha256_bytes(self.packet.read_bytes())
        self.manifest.write_bytes(REC.canonical_json(manifest))
        self.requests = 0
        original = REC.perform_request

        def count_request(*args, **kwargs):
            self.requests += 1
            return 200, {}, b"{}"

        REC.perform_request = count_request
        self.addCleanup(lambda: setattr(REC, "perform_request", original))
        argv = self.argv()
        argv.remove("--validate-existing")
        self.assertEqual(REC.main(argv), 3)
        self.assertEqual(self.requests, 0)

    def test_refuses_simultaneous_approval_and_rejection(self) -> None:
        responses = (
            response_text(self.packet.read_bytes())
            + "\nNote: I reject this anonymous public-read probe.\n",
            response_text(self.packet.read_bytes()).replace(
                "`\n\n## Exact", "` and Packet SHA-256: `" + "f" * 64
                + "`\n\n## Exact"),
        )
        for response in responses:
            with self.subTest(response=response):
                self.response.write_text(response)
                self.assertEqual(REC.main(self.argv()), 3)

    def test_capture_refuses_extra_response_prose_before_any_request(self) -> None:
        self.response.write_text(
            response_text(self.packet.read_bytes()) + "\nHarmless note.\n")
        self.requests = 0
        original = REC.perform_request

        def count_request(*args, **kwargs):
            self.requests += 1
            return 200, {}, b"{}"

        REC.perform_request = count_request
        self.addCleanup(lambda: setattr(REC, "perform_request", original))
        argv = self.argv()
        argv.remove("--validate-existing")
        self.assertEqual(REC.main(argv), 3)
        self.assertEqual(self.requests, 0)

    def test_refuses_a_course_naming_an_unauthorized_target(self) -> None:
        manifest = json.loads(self.manifest.read_bytes())
        manifest["courses"] = [{"name": "not-authorized", "expected_status": 200}]
        self.manifest.write_bytes(REC.canonical_json(manifest))
        self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_a_repeated_course_before_any_request(self) -> None:
        manifest = json.loads(self.manifest.read_bytes())
        manifest["courses"].append(dict(manifest["courses"][0]))
        self.manifest.write_bytes(REC.canonical_json(manifest))
        self.requests = 0
        original = REC.perform_request

        def count_request(*args, **kwargs):
            self.requests += 1
            return 200, {}, b"{}"

        REC.perform_request = count_request
        self.addCleanup(lambda: setattr(REC, "perform_request", original))
        argv = self.argv()
        argv.remove("--validate-existing")
        self.assertEqual(REC.main(argv), 3)
        self.assertEqual(self.requests, 0)

    def test_refuses_an_existing_destination_before_any_request(self) -> None:
        self.requests = 0
        original = REC.perform_request

        def count_request(*args, **kwargs):
            self.requests += 1
            return 200, {}, b"{}"

        REC.perform_request = count_request
        self.addCleanup(lambda: setattr(REC, "perform_request", original))
        argv = self.argv()
        argv.remove("--validate-existing")
        self.assertEqual(REC.main(argv), 3)
        self.assertEqual(self.requests, 0)

    def test_allows_a_nonempty_root_when_the_target_is_absent(self) -> None:
        destination = self.captures / "rate-limit.json"
        destination.rename(self.captures / "earlier-course.json")
        self.requests = 0
        original = REC.perform_request

        def count_request(*args, **kwargs):
            self.requests += 1
            return 200, {"Content-Type": "application/json"}, b'{"ok":true}'

        REC.perform_request = count_request
        self.addCleanup(lambda: setattr(REC, "perform_request", original))
        argv = self.argv()
        argv.remove("--validate-existing")
        self.assertEqual(REC.main(argv), 0)
        self.assertEqual(self.requests, 1)

    def test_reserves_the_destination_before_the_single_request(self) -> None:
        destination = self.captures / "rate-limit.json"
        destination.unlink()
        self.requests = 0
        original = REC.perform_request

        def inspect_reservation(*args, **kwargs):
            self.requests += 1
            self.assertTrue(destination.exists())
            return 200, {"Content-Type": "application/json"}, b'{"ok":true}'

        REC.perform_request = inspect_reservation
        self.addCleanup(lambda: setattr(REC, "perform_request", original))
        argv = self.argv()
        argv.remove("--validate-existing")

        self.assertEqual(REC.main(argv), 0)
        self.assertEqual(REC.main(argv), 3)
        self.assertEqual(self.requests, 1)

    def test_attempt_receipt_blocks_a_copied_packet_directory(self) -> None:
        (self.captures / "rate-limit.json").unlink()
        self.requests = 0
        original = REC.perform_request

        def count_request(*args, **kwargs):
            self.requests += 1
            return 200, {"Content-Type": "application/json"}, b'{"ok":true}'

        REC.perform_request = count_request
        self.addCleanup(lambda: setattr(REC, "perform_request", original))
        first_argv = self.argv()
        first_argv.remove("--validate-existing")
        self.assertEqual(REC.main(first_argv), 0)

        attempt_path = REC.CAPTURE_ATTEMPT_REGISTRY / (
            f"{REC.sha256_bytes(self.packet.read_bytes())}.json")
        attempt = json.loads(attempt_path.read_bytes())
        self.assertEqual(
            attempt["packet_sha256"],
            REC.sha256_bytes(self.packet.read_bytes()),
        )
        copied_review = Path(self.temporary.name) / "review-copy"
        copied_review.mkdir()
        copied_packet = copied_review / self.packet.name
        copied_response = copied_review / self.response.name
        copied_manifest = copied_review / self.manifest.name
        copied_packet.write_bytes(self.packet.read_bytes())
        copied_response.write_bytes(self.response.read_bytes())
        copied_manifest.write_bytes(self.manifest.read_bytes())
        second_argv = [
            "--packet", str(copied_packet),
            "--response", str(copied_response),
            "--course-manifest", str(copied_manifest),
            "--output-root", str(copied_review / "captures"),
        ]
        self.assertEqual(REC.main(second_argv), 3)
        self.assertEqual(self.requests, 1)

    def test_attempt_receipt_is_durable_before_the_request(self) -> None:
        (self.captures / "rate-limit.json").unlink()
        synced_kinds = []
        original_fsync = REC.os.fsync
        original_request = REC.perform_request

        def record_fsync(descriptor):
            mode = REC.os.fstat(descriptor).st_mode
            if stat.S_ISREG(mode):
                synced_kinds.append("file")
            elif stat.S_ISDIR(mode):
                synced_kinds.append("directory")
            original_fsync(descriptor)

        def inspect_receipt(*args, **kwargs):
            self.assertIn("file", synced_kinds)
            self.assertIn("directory", synced_kinds)
            return 200, {"Content-Type": "application/json"}, b'{"ok":true}'

        REC.os.fsync = record_fsync
        REC.perform_request = inspect_receipt
        self.addCleanup(lambda: setattr(REC.os, "fsync", original_fsync))
        self.addCleanup(lambda: setattr(REC, "perform_request", original_request))
        argv = self.argv()
        argv.remove("--validate-existing")
        self.assertEqual(REC.main(argv), 0)


class GraphqlExistingValidationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = ContainedTemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.review = Path(self.temporary.name) / "review"
        self.review.mkdir()
        self.target = {
            "name": "graphql-refusal",
            "operation": REC.GRAPHQL_REFUSAL_OPERATION,
            "url": "https://api.github.com/graphql",
        }
        self.packet = self.review / "operator-packet.md"
        self.packet.write_text(packet_text(authority([self.target])))
        self.response = self.review / "operator-response.md"
        self.response.write_text(response_text(self.packet.read_bytes()))
        parsed_authority = REC.parse_authority(self.packet.read_bytes())
        self.authority_sha = REC.sha256_bytes(
            REC.canonical_json(parsed_authority))
        self.manifest = self.review / "course-manifest.json"
        self.manifest.write_bytes(REC.canonical_json({
            "schema_name": REC.MANIFEST_SCHEMA,
            "schema_version": REC.MANIFEST_SCHEMA_VERSION,
            "packet_sha256": REC.sha256_bytes(self.packet.read_bytes()),
            "courses": [{"name": "graphql-refusal", "expected_status": 403}],
        }))
        self.captures = self.review / "captures"
        self.captures.mkdir()
        self.capture_path = self.captures / "graphql-refusal.json"
        self.write_record(self.make_record())

    def make_record(self):
        return REC.sanitize_capture(
            name=self.target["name"], operation=self.target["operation"],
            url=self.target["url"], http_status=403,
            headers=GRAPHQL_HEADERS, body=synthetic_graphql_body(),
            authority_sha256=self.authority_sha, source_kind="synthetic")

    def write_record(self, record) -> None:
        self.capture_path.write_bytes(REC.canonical_json(record))

    def argv(self):
        return [
            "--packet", str(self.packet),
            "--response", str(self.response),
            "--course-manifest", str(self.manifest),
            "--output-root", str(self.captures),
            "--validate-existing",
        ]

    def test_accepts_the_exact_normalized_graphql_record(self) -> None:
        self.assertEqual(REC.main(self.argv()), 0)

    def test_refuses_a_missing_or_incorrect_normalization_marker(self) -> None:
        for marker in (None, False, "true"):
            with self.subTest(marker=marker):
                record = self.make_record()
                if marker is None:
                    record.pop(REC.CALLER_ADDRESS_NORMALIZED_FIELD)
                else:
                    record[REC.CALLER_ADDRESS_NORMALIZED_FIELD] = marker
                self.write_record(record)
                self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_non_integer_schema_version_or_status(self) -> None:
        defects = (
            ("schema_version", True),
            ("schema_version", 1.0),
            ("http_status", 403.0),
        )
        for field, value in defects:
            with self.subTest(field=field, value=value):
                record = self.make_record()
                record[field] = value
                self.write_record(record)
                self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_normalized_body_tampering_even_with_a_new_digest(self) -> None:
        record = self.make_record()
        parsed = json.loads(record["body"])
        parsed["message"] = parsed["message"].replace(
            REC.CALLER_ADDRESS_PLACEHOLDER, "[different-placeholder]")
        record["body"] = REC.canonical_json(parsed).decode("utf-8")
        record["body_sha256"] = REC.sha256_bytes(record["body"].encode())
        self.write_record(record)

        self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_a_residual_address_even_with_a_new_digest(self) -> None:
        record = self.make_record()
        parsed = json.loads(record["body"])
        parsed["residual"] = "192.0.2.42"
        record["body"] = REC.canonical_json(parsed).decode("utf-8")
        record["body_sha256"] = REC.sha256_bytes(record["body"].encode())
        self.write_record(record)

        self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_normalized_body_digest_tampering(self) -> None:
        record = self.make_record()
        record["body_sha256"] = "f" * 64
        self.write_record(record)

        self.assertEqual(REC.main(self.argv()), 3)

    def test_refuses_a_tampered_zero_quota_header(self) -> None:
        record = self.make_record()
        record["retained_headers"].pop("x-ratelimit-used")
        self.write_record(record)

        self.assertEqual(REC.main(self.argv()), 3)


if __name__ == "__main__":
    unittest.main()
