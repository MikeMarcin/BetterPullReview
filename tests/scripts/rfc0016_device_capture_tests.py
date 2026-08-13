#!/usr/bin/env python3
from __future__ import annotations

import ast
import copy
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
TOOL_PATH = REPOSITORY_ROOT / "scripts" / "capture_rfc0016_device_contracts.py"
FIXTURE_ROOT = (
    REPOSITORY_ROOT
    / "tests"
    / "fixtures"
    / "github"
    / "device-authorization"
    / "v1"
)
SPEC = importlib.util.spec_from_file_location("rfc0016_capture", TOOL_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("could not load capture module")
CAPTURE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CAPTURE)

CAPTURED_AT = "2026-08-13T12:34:56Z"
SYNTHETIC_REGISTRATION_SHA256 = "sha256:" + "1" * 64
SYNTHETIC_ACCESS_TOKEN = "gh" + "u_" + "BPR_SYNTHETIC_ACCESS_CANARY_001"
SYNTHETIC_REFRESH_TOKEN = "gh" + "r_" + "BPR_SYNTHETIC_REFRESH_CANARY_001"
SYNTHETIC_PERSONAL_TOKEN = "gh" + "p_" + "BPR_SYNTHETIC_TOKEN_CANARY_001"
SYNTHETIC_DEVICE_CODE = "BPR_SYNTHETIC_DEVICE_CODE_CANARY_001"
SYNTHETIC_USER_CODE = "BPR1" + "-" + "TEST"
EXPECTED_AUTHORITY_SUBJECT_SHA256 = (
    "sha256:d3cfb906170799ee7c90a8290cabee1b1e7e6afef40937a08f7b21bd5b42d178"
)
EXPECTED_AUTHORITY_AMENDMENT_ID = "rfc0016-private-owner-only-2026-08-13"
REQUIRED_OUTCOMES = (
    "access_denied",
    "authorization_pending",
    "device_code_success",
    "device_flow_disabled",
    "expired_token",
    "slow_down",
    "token_success",
)
EXPECTED_PERMISSIONS = {
    "account": {},
    "enterprise": {},
    "organization": {},
    "repository": {
        "actions": "none",
        "administration": "none",
        "checks": "read",
        "commit_statuses": "none",
        "contents": "read",
        "issues": "none",
        "metadata": "read",
        "pull_requests": "write",
        "workflows": "none",
    },
}
FORBIDDEN_FIELD_CANARIES = (
    "access_token",
    "authorization",
    "body",
    "client_secret",
    "cookie",
    "credentials",
    "device_code",
    "environment",
    "headers",
    "password",
    "private_key",
    "raw",
    "refresh_token",
    "request",
    "response",
    "set-cookie",
    "stderr",
    "stdout",
    "transcript",
    "user_code",
)
DOCS_ERROR_URI = (
    "https://docs.github.com/apps/oauth-apps/building-oauth-apps/"
    "authorizing-oauth-apps#error-codes-for-the-device-flow"
)


def encode(value: object) -> bytes:
    return json.dumps(value, separators=(",", ":"), sort_keys=True).encode()


def registration_observation() -> dict[str, object]:
    return {
        "application_kind": "github_app",
        "application_visibility": "private",
        "callback_url_present": False,
        "device_flow_enabled": True,
        "installation_owner_login": "MikeMarcin",
        "installation_policy": "owner_only",
        "installation_repositories": ["MikeMarcin/BPR-Test"],
        "installation_repository_selection": "selected_repositories",
        "oauth_client_type": "public",
        "oauth_scopes": [],
        "owner_kind": "user",
        "owner_login": "MikeMarcin",
        "permissions": EXPECTED_PERMISSIONS,
        "public_client_id": "Iv1.0123456789abcdef",
        "refresh_policy": "expiring_user_token_with_refresh",
        "user_access_token_expiration_enabled": True,
        "webhook_active": False,
    }


def outcome_bodies() -> dict[str, bytes]:
    bodies = {
        "device_code_success": {
            "device_code": SYNTHETIC_DEVICE_CODE,
            "expires_in": 900,
            "interval": 5,
            "user_code": SYNTHETIC_USER_CODE,
            "verification_uri": "https://github.com/login/device",
        },
        "token_success": {
            "access_token": SYNTHETIC_ACCESS_TOKEN,
            "expires_in": 28800,
            "refresh_token": SYNTHETIC_REFRESH_TOKEN,
            "refresh_token_expires_in": 15811200,
            "scope": "",
            "token_type": "bearer",
        },
    }
    for outcome in (
        "access_denied",
        "authorization_pending",
        "device_flow_disabled",
        "expired_token",
        "slow_down",
    ):
        bodies[outcome] = {
            "error": outcome,
            "error_description": f"GitHub reported {outcome.replace('_', ' ')}.",
            "error_uri": DOCS_ERROR_URI,
        }
    return {outcome: encode(body) for outcome, body in bodies.items()}


def contract_artifacts(source_kind: str = "synthetic") -> dict[str, bytes]:
    registration = CAPTURE.sanitize_registration(
        registration_observation(),
        captured_at_utc=CAPTURED_AT,
        source_kind=source_kind,
    )
    registration_payload = CAPTURE.canonical_json(registration)
    registration_sha256 = CAPTURE._sha256(registration_payload)
    routes = CAPTURE.sanitize_route_probes(
        CAPTURE.expected_route_probes(),
        captured_at_utc=CAPTURED_AT,
        source_kind=source_kind,
        registration_record_sha256=registration_sha256,
    )
    result = {
        "registration.json": registration_payload,
        "route-probes.json": CAPTURE.canonical_json(routes),
    }
    for outcome, body in outcome_bodies().items():
        artifact = CAPTURE.sanitize_outcome(
            outcome,
            body,
            http_status=200,
            captured_at_utc=CAPTURED_AT,
            source_kind=source_kind,
            registration_record_sha256=registration_sha256,
        )
        result[f"outcomes/{outcome}.json"] = CAPTURE.canonical_json(artifact)
    return result


def synthetic_complete_artifacts() -> dict[str, bytes]:
    return contract_artifacts()


def write_contract_root(root: Path, source_kind: str) -> None:
    artifacts = contract_artifacts(source_kind)
    for relative, payload in artifacts.items():
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(payload)
    (root / "manifest.json").write_bytes(
        CAPTURE.canonical_json(
            CAPTURE._build_manifest(artifacts, source_kind=source_kind)
        )
    )


def write_synthetic_complete_root(root: Path) -> None:
    write_contract_root(root, "synthetic")


class CaptureContractTests(unittest.TestCase):
    def assert_rejected(self, action) -> None:
        with self.assertRaises(CAPTURE.ContractError) as caught:
            action()
        diagnostic = str(caught.exception)
        self.assertTrue(diagnostic)
        self.assertNotIn("runtime-", diagnostic)
        self.assertNotIn(SYNTHETIC_USER_CODE, diagnostic)

    def test_tracked_fixture_is_exact_pending_real_state(self) -> None:
        self.assertEqual(
            CAPTURE.AUTHORITY_SUBJECT_SHA256,
            EXPECTED_AUTHORITY_SUBJECT_SHA256,
        )
        self.assertEqual(
            CAPTURE.AUTHORITY_AMENDMENT_ID,
            EXPECTED_AUTHORITY_AMENDMENT_ID,
        )
        self.assertEqual(CAPTURE.OUTCOMES, REQUIRED_OUTCOMES)
        self.assertEqual(CAPTURE.REGISTRATION_PERMISSIONS, EXPECTED_PERMISSIONS)
        self.assertEqual(
            CAPTURE.validate_fixture_root(FIXTURE_ROOT),
            "pending_real_capture",
        )
        self.assert_rejected(
            lambda: CAPTURE.validate_fixture_root(
                FIXTURE_ROOT,
                require_complete=True,
            )
        )

    def test_synthetic_complete_tree_cannot_claim_real_and_is_not_modified(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_synthetic_complete_root(root)
            before = {
                path.relative_to(root).as_posix(): path.read_bytes()
                for path in root.rglob("*")
                if path.is_file()
            }
            self.assert_rejected(
                lambda: CAPTURE.validate_fixture_root(root, require_complete=True)
            )
            after = {
                path.relative_to(root).as_posix(): path.read_bytes()
                for path in root.rglob("*")
                if path.is_file()
            }
            self.assertEqual(before, after)

    def test_unattested_capture_validates_without_claiming_completion(self) -> None:
        artifacts = contract_artifacts("unattested_capture")
        manifest = CAPTURE.build_manifest(list(artifacts.items()))
        self.assertEqual(manifest["state"], "unattested_capture")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for relative, payload in artifacts.items():
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(payload)
            (root / "manifest.json").write_bytes(
                CAPTURE.canonical_json(manifest)
            )
            self.assertEqual(
                CAPTURE.validate_fixture_root(root),
                "unattested_capture",
            )
            self.assert_rejected(
                lambda: CAPTURE.validate_fixture_root(
                    root,
                    require_complete=True,
                )
            )

    def test_provenance_and_manifest_bind_plain_authority_amendment(self) -> None:
        artifacts = contract_artifacts("unattested_capture")
        for payload in artifacts.values():
            document = json.loads(payload)
            self.assertEqual(
                document["provenance"]["authority_amendment_id"],
                EXPECTED_AUTHORITY_AMENDMENT_ID,
            )
        manifest = CAPTURE.build_manifest(list(artifacts.items()))
        self.assertEqual(
            manifest["authority_amendment_id"],
            EXPECTED_AUTHORITY_AMENDMENT_ID,
        )

        registration = json.loads(artifacts["registration.json"])
        registration["provenance"]["authority_amendment_id"] = "other-direction"
        self.assert_rejected(
            lambda: CAPTURE.validate_registration_artifact(
                registration,
                require_real=False,
            )
        )
        manifest["authority_amendment_id"] = "other-direction"
        self.assert_rejected(
            lambda: CAPTURE._validate_manifest(
                manifest,
                artifacts,
                require_real=False,
            )
        )

    def test_public_sanitizers_cannot_fabricate_real_capture_provenance(self) -> None:
        self.assert_rejected(
            lambda: CAPTURE.sanitize_registration(
                registration_observation(),
                captured_at_utc=CAPTURED_AT,
                source_kind="real_service",
            )
        )

        artifacts = synthetic_complete_artifacts()
        registration = json.loads(artifacts["registration.json"])
        registration["provenance"]["source_kind"] = "real_service"
        artifacts["registration.json"] = CAPTURE.canonical_json(registration)
        registration_digest = CAPTURE._sha256(artifacts["registration.json"])
        for relative in CAPTURE.COMPLETE_ARTIFACTS:
            if relative == "registration.json":
                continue
            document = json.loads(artifacts[relative])
            document["provenance"]["source_kind"] = "real_service"
            document["provenance"]["registration_record_sha256"] = (
                registration_digest
            )
            artifacts[relative] = CAPTURE.canonical_json(document)
        self.assert_rejected(
            lambda: CAPTURE.build_manifest(list(artifacts.items()))
        )

        fabricated_manifest = {
            "artifacts": [
                {
                    "path": relative,
                    "sha256": CAPTURE._sha256(artifacts[relative]),
                    "source_kind": "real_service",
                }
                for relative in CAPTURE.COMPLETE_ARTIFACTS
            ],
            "authority_amendment_id": CAPTURE.AUTHORITY_AMENDMENT_ID,
            "authority_subject_sha256": CAPTURE.AUTHORITY_SUBJECT_SHA256,
            "schema_name": "bpr.rfc0016.device_capture_manifest",
            "schema_version": CAPTURE.SCHEMA_VERSION,
            "state": "complete_real_capture",
        }
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for relative, payload in artifacts.items():
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(payload)
            (root / "manifest.json").write_bytes(
                CAPTURE.canonical_json(fabricated_manifest)
            )
            self.assert_rejected(
                lambda: CAPTURE.validate_fixture_root(
                    root,
                    require_complete=True,
                )
            )

    def test_exact_tree_schema_and_canonical_encoding_are_enforced(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_synthetic_complete_root(root)
            (root / "unexpected.json").write_text("{}\n", encoding="utf-8")
            self.assert_rejected(lambda: CAPTURE.validate_fixture_root(root))

        artifacts = synthetic_complete_artifacts()
        manifest = CAPTURE._build_manifest(artifacts, source_kind="synthetic")
        manifest["extra"] = True
        self.assert_rejected(
            lambda: CAPTURE._validate_manifest(
                manifest,
                artifacts,
                require_real=False,
            )
        )

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_synthetic_complete_root(root)
            pending = root / "outcomes" / "authorization_pending.json"
            pending.write_bytes(pending.read_bytes().replace(b"\n", b"\r\n"))
            self.assert_rejected(lambda: CAPTURE.validate_fixture_root(root))

    def test_fixture_size_and_outside_root_link_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "pending.json").write_bytes(
                b"x" * (CAPTURE.MAX_JSON_BYTES + 1)
            )
            self.assert_rejected(lambda: CAPTURE.validate_fixture_root(root))

        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            root = parent / "fixture"
            root.mkdir()
            outside = parent / "outside.json"
            outside.write_bytes(
                CAPTURE.canonical_json(
                    {
                        "authority_subject_sha256": (
                            CAPTURE.AUTHORITY_SUBJECT_SHA256
                        ),
                        "schema_name": "bpr.rfc0016.device_capture_state",
                        "schema_version": 1,
                        "state": "pending_real_capture",
                    }
                )
            )
            try:
                (root / "pending.json").symlink_to(outside)
            except OSError:
                self.skipTest("symbolic link creation is unavailable")
            self.assert_rejected(lambda: CAPTURE.validate_fixture_root(root))

    def test_duplicate_malformed_truncated_and_oversized_json_are_synthetic(self) -> None:
        hostile = (
            b'{"error":"authorization_pending","error":"slow_down"}',
            b'{"error":',
            b'{"error":"authorization_pending"',
            b'{"error":NaN}',
            b"[" * 18 + b"0" + b"]" * 18,
            b"{" + b'"padding":"' + b"x" * CAPTURE.MAX_JSON_BYTES + b'"}',
        )
        for payload in hostile:
            with self.subTest(size=len(payload)):
                self.assert_rejected(
                    lambda payload=payload: CAPTURE.sanitize_outcome(
                        "authorization_pending",
                        payload,
                        http_status=200,
                        captured_at_utc=CAPTURED_AT,
                        source_kind="synthetic",
                        registration_record_sha256=(
                            SYNTHETIC_REGISTRATION_SHA256
                        ),
                    )
                )

    def test_secret_fields_and_byte_canaries_never_enter_projection(self) -> None:
        for field in FORBIDDEN_FIELD_CANARIES:
            raw = json.loads(outcome_bodies()["authorization_pending"])
            raw[field] = "runtime-sensitive-value"
            with self.subTest(field=field):
                self.assert_rejected(
                    lambda raw=raw: CAPTURE.sanitize_outcome(
                        "authorization_pending",
                        encode(raw),
                        http_status=200,
                        captured_at_utc=CAPTURED_AT,
                        source_kind="synthetic",
                        registration_record_sha256=(
                            SYNTHETIC_REGISTRATION_SHA256
                        ),
                    )
                )

        byte_canaries = (
            "Bearer abcdefghijk",
            "Authorization: Bearer abcdefghijk",
            "Cookie: session=abcdefghijk",
            "client_secret=abcdefghijk",
            "GET /login/device/code HTTP/1.1",
            "HTTP/1.1 200 OK",
            SYNTHETIC_USER_CODE,
            "abcdefghijklmnopqrstuvwxyz0123456789ABCD",
            "Abcdefghijklmnopqrstuvwxyz0123456789+/==",
            SYNTHETIC_PERSONAL_TOKEN,
        )
        for payload in byte_canaries:
            raw = encode(
                {
                    "error": "authorization_pending",
                    "error_description": payload,
                    "error_uri": DOCS_ERROR_URI,
                }
            )
            with self.subTest(payload=payload[:40]):
                artifact = CAPTURE.sanitize_outcome(
                    "authorization_pending",
                    raw,
                    http_status=200,
                    captured_at_utc=CAPTURED_AT,
                    source_kind="synthetic",
                    registration_record_sha256=(
                        SYNTHETIC_REGISTRATION_SHA256
                    ),
                )
                durable = CAPTURE.canonical_json(artifact)
                self.assertNotIn(payload.encode("utf-8"), durable)
                self.assertEqual(
                    artifact["projection"]["values"],
                    {
                        "error": "authorization_pending",
                        "official_error_uri": True,
                    },
                )

    def assert_error_free_form_omitted(self, canary: str) -> None:
        raw = encode(
            {
                "error": "authorization_pending",
                "error_description": canary,
                "error_uri": DOCS_ERROR_URI,
            }
        )
        artifact = CAPTURE.sanitize_outcome(
            "authorization_pending",
            raw,
            http_status=200,
            captured_at_utc=CAPTURED_AT,
            source_kind="synthetic",
            registration_record_sha256=SYNTHETIC_REGISTRATION_SHA256,
        )
        durable = CAPTURE.canonical_json(artifact)
        self.assertNotIn(canary.encode("utf-8"), durable)
        CAPTURE.validate_outcome_artifact(artifact, require_real=False)

    def test_lowercase_user_code_shape_is_omitted(self) -> None:
        self.assert_error_free_form_omitted("abcd-efgh")

    def test_session_cookie_material_is_omitted(self) -> None:
        self.assert_error_free_form_omitted(
            "session_id=sess_runtime_sensitive_001"
        )

    def test_jwt_shaped_credential_is_omitted(self) -> None:
        self.assert_error_free_form_omitted(
            "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJicHIifQ.signature"
        )

    def test_sanitizer_omits_transient_codes_and_tokens_in_memory(self) -> None:
        artifacts = synthetic_complete_artifacts()
        combined = b"".join(artifacts.values())
        for forbidden in (
            SYNTHETIC_DEVICE_CODE.encode("ascii"),
            SYNTHETIC_ACCESS_TOKEN.encode("ascii"),
            SYNTHETIC_REFRESH_TOKEN.encode("ascii"),
            SYNTHETIC_USER_CODE.encode("ascii"),
        ):
            self.assertNotIn(forbidden, combined)
        public_id = b"Iv1.0123456789abcdef"
        self.assertIn(public_id, artifacts["registration.json"])
        self.assertIn(
            b'"public_client_id_sha256": "sha256:',
            artifacts["registration.json"],
        )
        for relative, payload in artifacts.items():
            if relative != "registration.json":
                self.assertNotIn(public_id, payload)

    def test_path_ip_email_and_non_allowlisted_urls_are_rejected(self) -> None:
        canaries = (
            "/Users/operator/private",
            "/home/operator/private",
            r"C:\Users\operator\private",
            "192.0.2.1",
            "operator@example.com",
            "https://evil.example/device",
            "http://github.com/login/device",
            "https://github.com:invalid/device",
            "Iv1.0123456789abcdef",
        )
        for canary in canaries:
            artifact = CAPTURE.sanitize_outcome(
                "authorization_pending",
                outcome_bodies()["authorization_pending"],
                http_status=200,
                captured_at_utc=CAPTURED_AT,
                source_kind="synthetic",
                registration_record_sha256=SYNTHETIC_REGISTRATION_SHA256,
            )
            artifact["projection"]["values"]["unexpected_value"] = canary
            with self.subTest(canary=canary):
                self.assert_rejected(
                    lambda artifact=artifact: CAPTURE.validate_outcome_artifact(
                        artifact,
                        require_real=False,
                    )
                )

    def test_raw_transcript_and_unknown_fields_are_rejected(self) -> None:
        artifact = CAPTURE.sanitize_outcome(
            "authorization_pending",
            outcome_bodies()["authorization_pending"],
            http_status=200,
            captured_at_utc=CAPTURED_AT,
            source_kind="synthetic",
            registration_record_sha256=SYNTHETIC_REGISTRATION_SHA256,
        )
        for field in ("raw", "request", "response", "transcript", "headers"):
            hostile = copy.deepcopy(artifact)
            hostile[field] = "GET /login/device/code HTTP/1.1"
            with self.subTest(field=field):
                self.assert_rejected(
                    lambda hostile=hostile: CAPTURE.validate_outcome_artifact(
                        hostile,
                        require_real=False,
                    )
                )

    def test_manifest_digest_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_synthetic_complete_root(root)
            artifacts = {
                relative: (root / relative).read_bytes()
                for relative in CAPTURE.COMPLETE_ARTIFACTS
            }
            manifest = CAPTURE._build_manifest(
                artifacts,
                source_kind="synthetic",
            )
            path = root / "outcomes" / "authorization_pending.json"
            document = json.loads(path.read_text())
            document["projection"]["values"]["official_error_uri"] = False
            path.write_bytes(CAPTURE.canonical_json(document))
            changed_artifacts = {
                relative: (root / relative).read_bytes()
                for relative in CAPTURE.COMPLETE_ARTIFACTS
            }
            self.assert_rejected(
                lambda: CAPTURE._validate_manifest(
                    manifest,
                    changed_artifacts,
                    require_real=False,
                )
            )

    def test_every_artifact_is_bound_to_the_registration_record(self) -> None:
        artifacts = synthetic_complete_artifacts()
        path = "outcomes/authorization_pending.json"
        document = json.loads(artifacts[path])
        document["provenance"]["registration_record_sha256"] = (
            "sha256:" + "0" * 64
        )
        artifacts[path] = CAPTURE.canonical_json(document)
        self.assert_rejected(
            lambda: CAPTURE._build_manifest(
                artifacts,
                source_kind="synthetic",
            )
        )

    def test_public_client_identifier_is_globally_registration_only(self) -> None:
        artifacts = synthetic_complete_artifacts()
        registration = json.loads(artifacts["registration.json"])
        client_id = "abcdefghijklmnop"
        registration["public_client_id"] = client_id
        registration["public_client_id_sha256"] = CAPTURE._sha256(
            client_id.encode("ascii")
        )
        artifacts["registration.json"] = CAPTURE.canonical_json(registration)
        registration_digest = CAPTURE._sha256(artifacts["registration.json"])
        for relative in CAPTURE.COMPLETE_ARTIFACTS:
            if relative == "registration.json":
                continue
            document = json.loads(artifacts[relative])
            document["provenance"]["registration_record_sha256"] = (
                registration_digest
            )
            artifacts[relative] = CAPTURE.canonical_json(document)

        pending_path = "outcomes/authorization_pending.json"
        pending = json.loads(artifacts[pending_path])
        pending["projection"]["values"]["unexpected_value"] = client_id
        artifacts[pending_path] = CAPTURE.canonical_json(pending)
        self.assert_rejected(
            lambda: CAPTURE._build_manifest(
                artifacts,
                source_kind="synthetic",
            )
        )

    def test_route_allowlist_rejects_mutation_and_unknown_routes(self) -> None:
        hostile_sets = []
        mutation = CAPTURE.expected_route_probes()
        mutation[0] = dict(mutation[0])
        mutation[0]["method"] = "POST"
        hostile_sets.append(mutation)

        review_post = CAPTURE.expected_route_probes()
        review_post[0] = dict(review_post[0])
        review_post[0]["url"] = (
            "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1/comments"
        )
        hostile_sets.append(review_post)

        foreign = CAPTURE.expected_route_probes()
        foreign[0] = dict(foreign[0])
        foreign[0]["url"] = "https://api.github.com/repos/Other/Private"
        hostile_sets.append(foreign)

        for probes in hostile_sets:
            self.assert_rejected(
                lambda probes=probes: CAPTURE.sanitize_route_probes(
                    probes,
                    captured_at_utc=CAPTURED_AT,
                    source_kind="synthetic",
                    registration_record_sha256=(
                        SYNTHETIC_REGISTRATION_SHA256
                    ),
                )
            )

        artifact = CAPTURE.sanitize_route_probes(
            CAPTURE.expected_route_probes(),
            captured_at_utc=CAPTURED_AT,
            source_kind="synthetic",
            registration_record_sha256=SYNTHETIC_REGISTRATION_SHA256,
        )
        artifact["graphql_document"] = "mutation PublishReview { x }"
        self.assert_rejected(
            lambda: CAPTURE.validate_route_probe_artifact(
                artifact,
                require_real=False,
            )
        )

    def test_exact_graphql_document_and_read_only_inventory_are_pinned(self) -> None:
        probes = CAPTURE.expected_route_probes()
        operations = [probe["operation"] for probe in probes]
        self.assertEqual(
            operations,
            [
                "metadata",
                "contents_info_refs",
                "contents_upload_pack",
                "checks",
                "pull_request",
                "pull_request_files",
                "pull_request_commits",
                "pull_request_reviews",
                "pull_request_issue_comments",
                "review_threads",
            ],
        )
        self.assertEqual(operations.count("review_threads"), 1)
        self.assertNotIn("mutation", CAPTURE.GRAPHQL_DOCUMENT.lower())
        self.assertNotIn("CreateReview", CAPTURE.GRAPHQL_DOCUMENT)
        self.assertNotIn("SubmitReview", CAPTURE.GRAPHQL_DOCUMENT)
        self.assertEqual(
            CAPTURE._sha256(CAPTURE.GRAPHQL_DOCUMENT.encode("utf-8")),
            "sha256:ad859fce39cfcdbe81ef52aa9b1a3a4e2d64ee9fa2b3bc9f4b7d80622f98b6ed",
        )
        artifact = CAPTURE.sanitize_route_probes(
            probes,
            captured_at_utc=CAPTURED_AT,
            source_kind="synthetic",
            registration_record_sha256=SYNTHETIC_REGISTRATION_SHA256,
        )
        CAPTURE.validate_route_probe_artifact(artifact, require_real=False)

    def test_exactly_one_slow_down_is_required(self) -> None:
        artifacts = synthetic_complete_artifacts()
        duplicate_records = list(artifacts.items())
        duplicate_records.append(
            (
                "outcomes/slow_down.json",
                artifacts["outcomes/slow_down.json"],
            )
        )
        self.assert_rejected(
            lambda: CAPTURE.build_manifest(duplicate_records)
        )

        second = dict(artifacts)
        second["outcomes/slow_down-copy.json"] = artifacts[
            "outcomes/slow_down.json"
        ]
        self.assert_rejected(
            lambda: CAPTURE._build_manifest(second, source_kind="synthetic")
        )

        missing = dict(artifacts)
        missing.pop("outcomes/slow_down.json")
        self.assert_rejected(
            lambda: CAPTURE._build_manifest(missing, source_kind="synthetic")
        )

    def test_synthetic_artifact_cannot_claim_complete_real_capture(self) -> None:
        artifacts = synthetic_complete_artifacts()
        self.assert_rejected(
            lambda: CAPTURE.build_manifest(list(artifacts.items()))
        )
        registration = json.loads(artifacts["registration.json"])
        registration["provenance"]["source_kind"] = []
        self.assert_rejected(
            lambda: CAPTURE.validate_registration_artifact(
                registration,
                require_real=False,
            )
        )
        manifest = CAPTURE._build_manifest(artifacts, source_kind="synthetic")
        manifest["state"] = []
        self.assert_rejected(
            lambda: CAPTURE._validate_manifest(
                manifest,
                artifacts,
                require_real=False,
            )
        )

    def test_registration_is_private_owner_only_with_public_client_id(self) -> None:
        artifact = CAPTURE.sanitize_registration(
            registration_observation(),
            captured_at_utc=CAPTURED_AT,
            source_kind="synthetic",
        )
        CAPTURE.validate_registration_artifact(artifact, require_real=False)
        self.assertEqual(artifact["application_visibility"], "private")
        self.assertEqual(artifact["installation_policy"], "owner_only")
        self.assertEqual(artifact["installation_owner_login"], "MikeMarcin")
        self.assertEqual(
            artifact["installation_repositories"],
            ["MikeMarcin/BPR-Test"],
        )
        self.assertEqual(
            artifact["installation_repository_selection"],
            "selected_repositories",
        )
        self.assertEqual(artifact["oauth_client_type"], "public")
        for field, hostile_value in (
            ("application_visibility", "public"),
            ("callback_url_present", True),
            ("device_flow_enabled", False),
            ("installation_owner_login", "SomebodyElse"),
            ("installation_policy", "any_account"),
            ("installation_repositories", ["MikeMarcin/Other"]),
            ("installation_repository_selection", "all_repositories"),
            ("oauth_client_type", "confidential"),
            ("webhook_active", True),
            ("owner_login", "SomebodyElse"),
        ):
            hostile = copy.deepcopy(artifact)
            hostile[field] = hostile_value
            with self.subTest(field=field):
                self.assert_rejected(
                    lambda hostile=hostile: CAPTURE.validate_registration_artifact(
                        hostile,
                        require_real=False,
                    )
                )
        hostile = copy.deepcopy(artifact)
        hostile["permissions"]["repository"]["pull_requests"] = "read"
        self.assert_rejected(
            lambda: CAPTURE.validate_registration_artifact(
                hostile,
                require_real=False,
            )
        )
        hostile = copy.deepcopy(artifact)
        hostile["permissions"]["account"]["email_addresses"] = "read"
        self.assert_rejected(
            lambda: CAPTURE.validate_registration_artifact(
                hostile,
                require_real=False,
            )
        )
        hostile = copy.deepcopy(artifact)
        hostile["public_client_id_sha256"] = "sha256:" + "0" * 64
        self.assert_rejected(
            lambda: CAPTURE.validate_registration_artifact(hostile, require_real=False)
        )
        hostile = copy.deepcopy(artifact)
        hostile["provenance"]["captured_at_utc"] = "2026-02-30T12:34:56Z"
        self.assert_rejected(
            lambda: CAPTURE.validate_registration_artifact(hostile, require_real=False)
        )

    def test_cli_has_no_network_or_write_primitive(self) -> None:
        source = TOOL_PATH.read_text(encoding="utf-8")
        tree = ast.parse(source)
        imported = {
            alias.name
            for node in ast.walk(tree)
            if isinstance(node, ast.Import)
            for alias in node.names
        } | {
            node.module or ""
            for node in ast.walk(tree)
            if isinstance(node, ast.ImportFrom)
        }
        for forbidden in ("requests", "urllib.request", "http.client", "socket"):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, imported)
        for forbidden in (
            "urlopen",
            "write_bytes(",
            "write_text(",
            "open(\"w",
            "open('w",
        ):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, source)

        with (
            mock.patch.object(
                CAPTURE.Path,
                "write_bytes",
                side_effect=AssertionError,
            ),
            mock.patch.object(
                CAPTURE.Path,
                "write_text",
                side_effect=AssertionError,
            ),
        ):
            self.assertEqual(
                CAPTURE.validate_fixture_root(FIXTURE_ROOT),
                "pending_real_capture",
            )

    def test_cli_is_deterministic_and_diagnostics_do_not_echo_rejected_bytes(self) -> None:
        command = [
            sys.executable,
            str(TOOL_PATH),
            "validate",
            "--fixture-root",
            str(FIXTURE_ROOT),
        ]
        first = subprocess.run(command, check=False, capture_output=True, text=True)
        second = subprocess.run(command, check=False, capture_output=True, text=True)
        self.assertEqual(first.returncode, 0)
        self.assertEqual(first.stdout, second.stdout)
        self.assertEqual(first.stderr, second.stderr)
        self.assertEqual(
            first.stdout,
            "RFC0016_DEVICE_CAPTURE_VALID:pending_real_capture\n",
        )

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            canary = "BPR_RUNTIME_REJECTED_CANARY_708d"
            (root / "pending.json").write_text(canary, encoding="utf-8")
            result = subprocess.run(
                command[:-1] + [str(root)],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 1)
            self.assertEqual(result.stdout, "")
            self.assertEqual(result.stderr, "RFC0016_DEVICE_CAPTURE_INVALID\n")
            self.assertNotIn(canary, result.stderr)


if __name__ == "__main__":
    unittest.main()
