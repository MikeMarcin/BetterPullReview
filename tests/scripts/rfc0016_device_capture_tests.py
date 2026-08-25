#!/usr/bin/env python3
from __future__ import annotations

import ast
import copy
import importlib.util
import json
import os
import subprocess
import sys
import unittest
from collections.abc import Callable
from pathlib import Path
from unittest import mock


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

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
RECORDED_REVIEW_ROOT = (
    REPOSITORY_ROOT
    / "docs"
    / "verification"
    / "reviews"
    / "rfc0016-attended-live-capture-v001"
)
RECORDED_COURSE_ROOT = (
    REPOSITORY_ROOT
    / "docs"
    / "verification"
    / "rfc0016-live-capture-v001"
    / "courses"
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


def link_or_skip(link_path: Path, target: Path) -> None:
    """Create a symbolic link, or skip when Windows withholds the privilege.

    Creating a symbolic link on Windows needs SeCreateSymbolicLinkPrivilege,
    which an unelevated account does not hold unless Developer Mode is on. The
    mutation this stages is an adversary the validator must reject, so where the
    platform will not let the adversary exist the case is skipped by name rather
    than reported as a failure of the thing under test.

    `tests/sqlite_contract/epoch_catalog_storage_tests.cpp` does the same, and
    says the same thing in its own message.
    """
    try:
        link_path.symlink_to(target)
    except OSError as error:
        if getattr(error, "winerror", None) != 1314:
            raise
        raise unittest.SkipTest(
            "symbolic-link rejection requires the Windows symlink privilege; "
            "the other identity-drift mutations remain covered"
        ) from error


def encode(value: object) -> bytes:
    return json.dumps(value, separators=(",", ":"), sort_keys=True).encode()


def registration_observation(
    public_client_id: str = "Iv1.0123456789abcdef",
) -> dict[str, object]:
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
        "public_client_id": public_client_id,
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
    bodies["slow_down"]["interval"] = 10
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


def write_payload_tree(root: Path, payloads: dict[str, bytes]) -> None:
    for relative, payload in payloads.items():
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(payload)


def course_evidence_records(
    registration_payload: bytes,
    runner_digest: str,
    original_reservation_digest: str,
    recovery_reservation_digest: str,
) -> dict[str, dict[str, bytes]]:
    course_times = {
        "access-denied": "2026-08-13T16:00:00Z",
        "device-flow-disabled": "2026-08-13T16:01:00Z",
        "expired-token": "2026-08-13T16:02:00Z",
        "slow-down": "2026-08-13T16:03:00Z",
        "token-success": "2026-08-13T16:04:00Z",
    }
    outcomes = {
        "access-denied": (
            "device_code_success",
            "authorization_pending",
            "access_denied",
        ),
        "device-flow-disabled": ("device_flow_disabled",),
        "expired-token": ("device_code_success", "expired_token"),
        "slow-down": (
            "device_code_success",
            "authorization_pending",
            "slow_down",
        ),
        "token-success": (
            "device_code_success",
            "authorization_pending",
            "token_success",
        ),
    }
    registration_digest = CAPTURE._sha256(registration_payload)
    records: dict[str, dict[str, bytes]] = {}
    for course in CAPTURE.ATTESTATION_COURSES:
        executed_at = course_times[course]
        bindings = {
            "device_flow_transition_sha256": None,
            "slow_down_original_reservation_sha256": (
                original_reservation_digest if course == "slow-down" else None
            ),
            "slow_down_recovery_reservation_sha256": (
                recovery_reservation_digest if course == "slow-down" else None
            ),
        }
        course_records = {"registration.json": registration_payload}
        for outcome in outcomes[course]:
            artifact = CAPTURE.sanitize_outcome(
                outcome,
                outcome_bodies()[outcome],
                http_status=200,
                captured_at_utc=executed_at,
                source_kind="unattested_capture",
                registration_record_sha256=registration_digest,
            )
            course_records[f"outcomes/{outcome}.json"] = CAPTURE.canonical_json(
                artifact
            )
        if course == "token-success":
            course_records["route-probes.json"] = CAPTURE.canonical_json(
                CAPTURE.sanitize_route_probes(
                    CAPTURE.expected_route_probes(),
                    captured_at_utc=executed_at,
                    source_kind="unattested_capture",
                    registration_record_sha256=registration_digest,
                )
            )
        if course == "device-flow-disabled":
            transition = CAPTURE.canonical_json(
                {
                    "application_id": CAPTURE.APPROVED_APPLICATION_ID,
                    "authority_amendment_id": CAPTURE.AUTHORITY_AMENDMENT_ID,
                    "authority_subject_sha256": CAPTURE.AUTHORITY_SUBJECT_SHA256,
                    "observation_method": "attended_github_settings_page",
                    "owner_login": "MikeMarcin",
                    "public_client_id": CAPTURE.APPROVED_PUBLIC_CLIENT_ID,
                    "schema_name": (
                        "bpr.rfc0016.device_flow_transition_observation"
                    ),
                    "schema_version": 1,
                    "states": [
                        {
                            "device_flow_enabled": True,
                            "observed_at_utc": "2026-08-13T16:00:58Z",
                            "phase": "before_enabled",
                        },
                        {
                            "device_flow_enabled": False,
                            "observed_at_utc": "2026-08-13T16:00:59Z",
                            "phase": "disabled_for_capture",
                        },
                        {
                            "device_flow_enabled": True,
                            "observed_at_utc": "2026-08-13T16:01:01Z",
                            "phase": "restored_enabled",
                        },
                    ],
                }
            )
            course_records["device-flow-transition.json"] = transition
            bindings["device_flow_transition_sha256"] = CAPTURE._sha256(transition)
        course_records["course-execution.json"] = CAPTURE.canonical_json(
            {
                "authority_amendment_id": CAPTURE.AUTHORITY_AMENDMENT_ID,
                "authority_subject_sha256": CAPTURE.AUTHORITY_SUBJECT_SHA256,
                "bindings": bindings,
                "capture_runner_sha256": runner_digest,
                "course": course,
                "executed_at_utc": executed_at,
                "registration_record_sha256": registration_digest,
                "schema_name": "bpr.rfc0016.course_execution",
                "schema_version": 1,
            }
        )
        records[course] = course_records
    return records


def slow_down_authority_records(runner_digest: str) -> dict[str, bytes]:
    failed_runner_digest = "sha256:" + "f" * 64
    authority = CAPTURE.canonical_json(
        {
            "authority_amendment_id": CAPTURE.AUTHORITY_AMENDMENT_ID,
            "authority_id": CAPTURE.SLOW_DOWN_REPLACEMENT_AUTHORITY_ID,
            "authority_subject_sha256": CAPTURE.AUTHORITY_SUBJECT_SHA256,
            "authorized_at_utc": "2026-08-13T16:02:50Z",
            "course": "slow-down",
            "previous_early_poll_consumed": True,
            "replacement_early_poll_limit": 1,
            "schema_name": "bpr.rfc0016.slow_down_replacement_authority",
            "schema_version": 1,
            "state": "replacement_authorized",
        }
    )
    spent = CAPTURE.canonical_json(
        {
            "authority_amendment_id": CAPTURE.AUTHORITY_AMENDMENT_ID,
            "authority_id": CAPTURE.SLOW_DOWN_REPLACEMENT_AUTHORITY_ID,
            "authority_record_sha256": CAPTURE._sha256(authority),
            "authority_subject_sha256": CAPTURE.AUTHORITY_SUBJECT_SHA256,
            "capture_runner_sha256": failed_runner_digest,
            "course": "slow-down",
            "schema_name": "bpr.rfc0016.slow_down_reservation",
            "schema_version": 1,
            "spent_before_traffic_at_utc": "2026-08-13T16:02:51Z",
            "state": "spent",
        }
    )
    recovery_authority = CAPTURE.canonical_json(
        {
            "authority_amendment_id": CAPTURE.AUTHORITY_AMENDMENT_ID,
            "authority_id": CAPTURE.SLOW_DOWN_RECOVERY_AUTHORITY_ID,
            "authority_subject_sha256": CAPTURE.AUTHORITY_SUBJECT_SHA256,
            "authorized_at_utc": "2026-08-13T16:02:58Z",
            "course": "slow-down",
            "failed_attempt_authority_sha256": CAPTURE._sha256(authority),
            "failed_attempt_capture_runner_sha256": failed_runner_digest,
            "failed_attempt_spent_marker_sha256": CAPTURE._sha256(spent),
            "parser_repair_capture_runner_sha256": runner_digest,
            "recovery_early_poll_limit": 1,
            "schema_name": (
                "bpr.rfc0016.slow_down_parser_repair_recovery_authority"
            ),
            "schema_version": 1,
            "state": "parser_repair_recovery_authorized",
        }
    )
    recovery_spent = CAPTURE.canonical_json(
        {
            "authority_amendment_id": CAPTURE.AUTHORITY_AMENDMENT_ID,
            "authority_id": CAPTURE.SLOW_DOWN_RECOVERY_AUTHORITY_ID,
            "authority_record_sha256": CAPTURE._sha256(recovery_authority),
            "authority_subject_sha256": CAPTURE.AUTHORITY_SUBJECT_SHA256,
            "capture_runner_sha256": runner_digest,
            "course": "slow-down",
            "failed_attempt_authority_sha256": CAPTURE._sha256(authority),
            "failed_attempt_spent_marker_sha256": CAPTURE._sha256(spent),
            "schema_name": (
                "bpr.rfc0016.slow_down_parser_repair_recovery_reservation"
            ),
            "schema_version": 1,
            "spent_before_traffic_at_utc": "2026-08-13T16:02:59Z",
            "state": "spent",
        }
    )
    return {
        CAPTURE.SLOW_DOWN_AUTHORITY_PATH: authority,
        CAPTURE.SLOW_DOWN_SPENT_PATH: spent,
        CAPTURE.SLOW_DOWN_RECOVERY_AUTHORITY_PATH: recovery_authority,
        CAPTURE.SLOW_DOWN_RECOVERY_SPENT_PATH: recovery_spent,
    }


def assembled_fixture(
    course_records: dict[str, dict[str, bytes]],
) -> dict[str, bytes]:
    return CAPTURE._assemble_course_fixture(course_records)


def unvalidated_assembled_fixture(
    courses: dict[str, dict[str, bytes]],
) -> dict[str, bytes]:
    artifacts = {
        "registration.json": courses["token-success"]["registration.json"],
        "route-probes.json": courses["token-success"]["route-probes.json"],
        "outcomes/access_denied.json": courses["access-denied"][
            "outcomes/access_denied.json"
        ],
        "outcomes/authorization_pending.json": courses["slow-down"][
            "outcomes/authorization_pending.json"
        ],
        "outcomes/device_code_success.json": courses["token-success"][
            "outcomes/device_code_success.json"
        ],
        "outcomes/device_flow_disabled.json": courses["device-flow-disabled"][
            "outcomes/device_flow_disabled.json"
        ],
        "outcomes/expired_token.json": courses["expired-token"][
            "outcomes/expired_token.json"
        ],
        "outcomes/slow_down.json": courses["slow-down"][
            "outcomes/slow_down.json"
        ],
        "outcomes/token_success.json": courses["token-success"][
            "outcomes/token_success.json"
        ],
    }
    artifacts["manifest.json"] = CAPTURE.canonical_json(
        {
            "artifacts": [
                {
                    "path": relative,
                    "sha256": CAPTURE._sha256(payload),
                    "source_kind": json.loads(payload)["provenance"][
                        "source_kind"
                    ],
                }
                for relative, payload in sorted(artifacts.items())
            ],
            "authority_amendment_id": CAPTURE.AUTHORITY_AMENDMENT_ID,
            "authority_subject_sha256": CAPTURE.AUTHORITY_SUBJECT_SHA256,
            "schema_name": "bpr.rfc0016.device_capture_manifest",
            "schema_version": 1,
            "state": "unattested_capture",
        }
    )
    return artifacts


def live_run_attestation(
    fixture_payloads: dict[str, bytes],
    course_records: dict[str, dict[str, bytes]],
    runner_digest: str,
) -> bytes:
    executions = {
        course: json.loads(records["course-execution.json"])
        for course, records in course_records.items()
    }
    return CAPTURE.canonical_json(
        {
            "artifacts": [
                {
                    "path": relative,
                    "sha256": CAPTURE._sha256(payload),
                }
                for relative, payload in sorted(fixture_payloads.items())
            ],
            "authority_amendment_id": CAPTURE.AUTHORITY_AMENDMENT_ID,
            "authority_subject_sha256": CAPTURE.AUTHORITY_SUBJECT_SHA256,
            "capture_runner_sha256": runner_digest,
            "course_execution": {
                course: {
                    "bindings": execution["bindings"],
                    "capture_runner_sha256": execution["capture_runner_sha256"],
                    "executed_at_utc": execution["executed_at_utc"],
                }
                for course, execution in sorted(executions.items())
            },
            "course_tree_sha256": {
                course: CAPTURE._course_tree_digest(records)
                for course, records in sorted(course_records.items())
            },
            "schema_name": "bpr.rfc0016.live_run_attestation",
            "schema_version": 1,
            "state": "attended_runner_completed_unattested",
        }
    )


def artifact_triple(path: Path, repository_root: Path) -> dict[str, object]:
    payload = path.read_bytes()
    return {
        "byte_length": len(payload),
        "path": path.relative_to(repository_root).as_posix(),
        "sha256": CAPTURE._sha256(payload),
    }


def build_attestation_review(
    repository_root: Path,
    *,
    mutate_evidence: Callable[
        [dict[str, dict[str, bytes]], dict[str, bytes]],
        None,
    ]
    | None = None,
) -> tuple[Path, Path, Path]:
    validator = repository_root / CAPTURE.ATTENDED_VALIDATOR_PATH
    validator.parent.mkdir(parents=True)
    validator.write_bytes(
        (
            REPOSITORY_ROOT
            / CAPTURE.ATTENDED_VALIDATOR_PATH
        ).read_bytes()
    )
    for asset_name in ("attended_review.css", "attended_review_runtime.js"):
        asset = validator.parent.parent / "assets" / asset_name
        asset.parent.mkdir(parents=True, exist_ok=True)
        asset.write_bytes(
            (
                REPOSITORY_ROOT
                / ".agents"
                / "skills"
                / "bpr-attended-review"
                / "assets"
                / asset_name
            ).read_bytes()
        )
    runner = repository_root / CAPTURE.CAPTURE_RUNNER_PATH
    runner.parent.mkdir(parents=True, exist_ok=True)
    runner.write_bytes((REPOSITORY_ROOT / CAPTURE.CAPTURE_RUNNER_PATH).read_bytes())
    runner_digest = CAPTURE._sha256(runner.read_bytes())
    registration = CAPTURE.sanitize_registration(
        registration_observation(CAPTURE.APPROVED_PUBLIC_CLIENT_ID),
        captured_at_utc=CAPTURED_AT,
        source_kind="unattested_capture",
    )
    registration_payload = CAPTURE.canonical_json(registration)
    course_root = repository_root / "capture" / "courses"
    authority_records = slow_down_authority_records(runner_digest)
    write_payload_tree(course_root, authority_records)
    course_records = course_evidence_records(
        registration_payload,
        runner_digest,
        CAPTURE._sha256(authority_records[CAPTURE.SLOW_DOWN_SPENT_PATH]),
        CAPTURE._sha256(
            authority_records[CAPTURE.SLOW_DOWN_RECOVERY_SPENT_PATH]
        ),
    )
    fixture_payloads = assembled_fixture(course_records)
    if mutate_evidence is not None:
        mutate_evidence(course_records, fixture_payloads)
    for course, records in course_records.items():
        write_payload_tree(course_root / course, records)
    fixture_root = repository_root / "capture" / "fixture"
    write_payload_tree(fixture_root, fixture_payloads)
    attestation = repository_root / "capture" / "live-run-attestation.json"
    attestation.write_bytes(
        live_run_attestation(fixture_payloads, course_records, runner_digest)
    )
    review = (
        repository_root
        / "docs"
        / "verification"
        / "reviews"
        / "rfc0016-attended-live-capture-v001"
    )
    review.mkdir(parents=True)
    input_path = review / "offline-review-input.json"
    identity_files = [
        {
            "label": f"fixture:{relative}",
            "path": os.path.relpath(fixture_root / relative, review),
        }
        for relative in sorted(fixture_payloads)
    ] + [
        {
            "label": f"course:{course}/{relative}",
            "path": os.path.relpath(course_root / course / relative, review),
        }
        for course, records in sorted(course_records.items())
        for relative in sorted(records)
    ] + [
        {
            "label": f"authority:{relative}",
            "path": os.path.relpath(course_root / relative, review),
        }
        for relative in CAPTURE.SLOW_DOWN_AUTHORITY_RECORD_PATHS
    ] + [
        {
            "label": "attestation:live-run-attestation.json",
            "path": os.path.relpath(attestation, review),
        }
    ]
    input_path.write_bytes(
        CAPTURE.canonical_json(
            {
                "downstream_claims": CAPTURE.ATTESTATION_DOWNSTREAM_CLAIMS,
                "project_id": "betterpullreview",
                "reference_disclaimer": CAPTURE.ATTESTATION_REFERENCE_DISCLAIMER,
                "review_gate": CAPTURE.ATTESTATION_REVIEW_GATE,
                "review_scope": CAPTURE.ATTESTATION_REVIEW_SCOPE,
                "schema_name": "bpr.attended_review_batch_input",
                "schema_version": 1,
                "subjects": [
                    {
                        "ab_group": None,
                        "acceptance_criteria": CAPTURE.ATTESTATION_ACCEPTANCE_CRITERIA,
                        "audit_verdict": CAPTURE.ATTESTATION_AUDIT,
                        "choice_group": None,
                        "description": CAPTURE.ATTESTATION_DESCRIPTION,
                        "detail_sections": [],
                        "display_name": CAPTURE.ATTESTATION_DISPLAY_NAME,
                        "identity_files": identity_files,
                        "known_concerns": CAPTURE.ATTESTATION_KNOWN_CONCERNS,
                        "media_groups": [],
                        "subject_id": CAPTURE.ATTESTATION_SUBJECT_ID,
                        "subject_key": CAPTURE.ATTESTATION_SUBJECT_KEY,
                    }
                ],
                "title": CAPTURE.ATTESTATION_REVIEW_TITLE,
            }
        )
    )
    package = review / "offline-package"
    subprocess.run(
        [
            sys.executable,
            str(
                REPOSITORY_ROOT
                / CAPTURE.ATTENDED_VALIDATOR_PATH
            ),
            "build",
            "--input",
            str(input_path),
            "--out",
            str(package),
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    manifest_path = package / "attended-review-manifest.json"
    manifest = json.loads(manifest_path.read_bytes())
    subject = manifest["subjects"][0]
    verdict_path = review / "human-verdict.json"
    verdict_path.write_text(
        json.dumps(
            {
                "schema_name": "bpr.attended_review_verdict",
                "schema_version": 1,
                "batch_id": manifest["batch_id"],
                "review_gate": CAPTURE.ATTESTATION_REVIEW_GATE,
                "review_scope": CAPTURE.ATTESTATION_REVIEW_SCOPE,
                "accepted_subjects": [
                    {
                        "subject_id": subject["subject_id"],
                        "subject_digest": subject["subject_digest"],
                    }
                ],
                "rejected_subjects": [],
                "notes_by_subject_identity": {},
                "downstream_claims": CAPTURE.ATTESTATION_DOWNSTREAM_CLAIMS,
            },
            ensure_ascii=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    validation_path = review / "verdict-validation.json"
    validation_path.write_bytes(
        CAPTURE.canonical_json(
            {
                "batch_id": manifest["batch_id"],
                "capture_runner": artifact_triple(runner, repository_root),
                "claims": CAPTURE.ATTESTATION_VALIDATION_CLAIMS,
                "invocation": (
                    f"python3 {CAPTURE.ATTENDED_VALIDATOR_PATH} validate-verdict "
                    f"--manifest {manifest_path.relative_to(repository_root).as_posix()} "
                    f"--verdict {verdict_path.relative_to(repository_root).as_posix()}"
                ),
                "manifest": artifact_triple(manifest_path, repository_root),
                "official_validator": artifact_triple(validator, repository_root),
                "outcome": "all_subjects_accepted",
                "recorded_at": "2026-08-13T09:30:00-07:00",
                "result": "valid complete verdict",
                "review_input": artifact_triple(input_path, repository_root),
                "review_page": artifact_triple(
                    package / "index.html",
                    repository_root,
                ),
                "review_gate": CAPTURE.ATTESTATION_REVIEW_GATE,
                "review_scope": CAPTURE.ATTESTATION_REVIEW_SCOPE,
                "schema_name": "bpr.attended_review_validation_record",
                "schema_version": 1,
                "verdict": artifact_triple(verdict_path, repository_root),
            }
        )
    )
    return fixture_root, course_root, review


def rewrite_json(path: Path, document: object) -> None:
    path.write_bytes(CAPTURE.canonical_json(document))


def refresh_validation_triple(
    review: Path,
    repository_root: Path,
    field: str,
    path: Path,
) -> None:
    validation_path = review / "verdict-validation.json"
    validation = json.loads(validation_path.read_bytes())
    validation[field] = artifact_triple(path, repository_root)
    rewrite_json(validation_path, validation)


class CaptureContractTests(unittest.TestCase):
    def assert_rejected(self, action) -> None:
        with self.assertRaises(CAPTURE.ContractError) as caught:
            action()
        diagnostic = str(caught.exception)
        self.assertTrue(diagnostic)
        self.assertNotIn("runtime-", diagnostic)
        self.assertNotIn(SYNTHETIC_USER_CODE, diagnostic)

    def test_tracked_fixture_is_exact_recorded_attended_capture(self) -> None:
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
            "unattested_capture",
        )
        self.assert_rejected(
            lambda: CAPTURE.validate_fixture_root(
                FIXTURE_ROOT,
                require_complete=True,
            )
        )
        self.assertEqual(
            CAPTURE.validate_fixture_root(
                FIXTURE_ROOT,
                require_complete=True,
                attestation_review=RECORDED_REVIEW_ROOT,
                course_evidence_root=RECORDED_COURSE_ROOT,
            ),
            "attended_human_accepted_capture",
        )
        manifest = json.loads(
            (
                RECORDED_REVIEW_ROOT
                / "offline-package"
                / "attended-review-manifest.json"
            ).read_bytes()
        )
        self.assertEqual(
            manifest["batch_id"],
            "bpr:/attended-review/betterpullreview/42fd8114a6c2bbeb0cbf6f54",
        )
        self.assertEqual(
            manifest["subjects"][0]["subject_digest"],
            "sha256:a72c61372e2a3da00c4746f400da6bf983121b55e6be2651a9a5abb0ae67afac",
        )

    def test_synthetic_complete_tree_cannot_claim_real_and_is_not_modified(self) -> None:
        with contained_temporary_directory() as temporary:
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
        with contained_temporary_directory() as temporary:
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

    def test_exact_attended_review_admits_unattested_capture_via_cli(self) -> None:
        with contained_temporary_directory() as temporary:
            repository_root = Path(temporary).resolve()
            fixture_root, course_root, review = build_attestation_review(
                repository_root
            )
            with mock.patch.object(CAPTURE, "REPOSITORY_ROOT", repository_root):
                self.assertEqual(
                    CAPTURE.validate_fixture_root(fixture_root),
                    "unattested_capture",
                )
                self.assert_rejected(
                    lambda: CAPTURE.validate_fixture_root(
                        fixture_root,
                        require_complete=True,
                    )
                )
                self.assert_rejected(
                    lambda: CAPTURE.validate_fixture_root(
                        fixture_root,
                        require_complete=True,
                        attestation_review=review,
                    )
                )
                with mock.patch.object(sys, "argv", [
                    str(TOOL_PATH),
                    "validate",
                    "--fixture-root",
                    str(fixture_root),
                    "--require-complete",
                    "--attestation-review",
                    str(review),
                    "--course-evidence-root",
                    str(course_root),
                ]):
                    self.assertEqual(CAPTURE.main(), 0)

    def test_attended_review_rejects_schema_verdict_and_claim_tampering(self) -> None:
        mutations = (
            ("manifest-unknown", "manifest", lambda value: value.update(extra=True)),
            (
                "identity-unknown",
                "manifest",
                lambda value: value["subjects"][0]["identity_files"][0].update(
                    extra=True
                ),
            ),
            (
                "false-audit",
                "manifest",
                lambda value: value["subjects"][0]["audit_verdict"].update(
                    status="fail"
                ),
            ),
            (
                "wrong-gate",
                "verdict",
                lambda value: value.update(review_gate="other_gate"),
            ),
            (
                "rejected",
                "verdict",
                lambda value: (
                    value["rejected_subjects"].extend(value["accepted_subjects"]),
                    value["accepted_subjects"].clear(),
                ),
            ),
            (
                "wrong-claims",
                "verdict",
                lambda value: value["downstream_claims"].update(
                    bpr_0276_closed=True
                ),
            ),
            (
                "boolean-integer-alias",
                "validation",
                lambda value: value.update(schema_version=True),
            ),
            (
                "claim-integer-alias",
                "validation",
                lambda value: value["claims"].update(
                    release_gate_satisfied=0
                ),
            ),
        )
        for name, artifact, mutate in mutations:
            with self.subTest(name=name), contained_temporary_directory() as temporary:
                repository_root = Path(temporary).resolve()
                fixture_root, course_root, review = build_attestation_review(
                    repository_root
                )
                paths = {
                    "manifest": review / "offline-package" / "attended-review-manifest.json",
                    "verdict": review / "human-verdict.json",
                    "validation": review / "verdict-validation.json",
                }
                document = json.loads(paths[artifact].read_bytes())
                mutate(document)
                rewrite_json(paths[artifact], document)
                if artifact != "validation":
                    refresh_validation_triple(
                        review,
                        repository_root,
                        artifact,
                        paths[artifact],
                    )
                with mock.patch.object(CAPTURE, "REPOSITORY_ROOT", repository_root):
                    self.assert_rejected(
                        lambda: CAPTURE.validate_fixture_root(
                            fixture_root,
                            require_complete=True,
                            attestation_review=review,
                            course_evidence_root=course_root,
                        )
                    )

    def test_attended_review_rejects_identity_path_link_and_byte_drift(self) -> None:
        mutations = (
            "missing",
            "extra",
            "escape",
            "symlink",
            "fixture-bytes",
            "course-bytes",
            "authority-bytes",
            "attestation-bytes",
        )
        for mutation in mutations:
            with (
                self.subTest(mutation=mutation),
                contained_temporary_directory() as temporary,
            ):
                repository_root = Path(temporary).resolve()
                fixture_root, course_root, review = build_attestation_review(
                    repository_root
                )
                manifest_path = (
                    review / "offline-package" / "attended-review-manifest.json"
                )
                manifest = json.loads(manifest_path.read_bytes())
                identities = manifest["subjects"][0]["identity_files"]
                identity_path = review / "offline-package" / identities[0]["path"]
                if mutation == "missing":
                    identities.pop(0)
                    rewrite_json(manifest_path, manifest)
                    refresh_validation_triple(
                        review, repository_root, "manifest", manifest_path
                    )
                elif mutation == "extra":
                    identities.append(copy.deepcopy(identities[0]))
                    rewrite_json(manifest_path, manifest)
                    refresh_validation_triple(
                        review, repository_root, "manifest", manifest_path
                    )
                elif mutation == "escape":
                    identities[0]["path"] = "../../../../capture/fixture/manifest.json"
                    rewrite_json(manifest_path, manifest)
                    refresh_validation_triple(
                        review, repository_root, "manifest", manifest_path
                    )
                elif mutation == "symlink":
                    identity_path.unlink()
                    link_or_skip(identity_path, fixture_root / "manifest.json")
                elif mutation == "fixture-bytes":
                    identity_path.write_bytes(identity_path.read_bytes() + b" ")
                elif mutation == "course-bytes":
                    course_identity = next(
                        identity
                        for identity in identities
                        if identity["label"].startswith("course:")
                    )
                    course_path = (
                        review / "offline-package" / course_identity["path"]
                    )
                    course_path.write_bytes(course_path.read_bytes() + b" ")
                elif mutation == "authority-bytes":
                    authority_identity = next(
                        identity
                        for identity in identities
                        if identity["label"].startswith("authority:")
                    )
                    authority_path = (
                        review / "offline-package" / authority_identity["path"]
                    )
                    authority_path.write_bytes(authority_path.read_bytes() + b" ")
                else:
                    attestation_path = (
                        review / "offline-package" / identities[-1]["path"]
                    )
                    attestation_path.write_bytes(
                        attestation_path.read_bytes() + b" "
                    )
                with mock.patch.object(CAPTURE, "REPOSITORY_ROOT", repository_root):
                    self.assert_rejected(
                        lambda: CAPTURE.validate_fixture_root(
                            fixture_root,
                            require_complete=True,
                            attestation_review=review,
                            course_evidence_root=course_root,
                        )
                    )

    def test_course_evidence_rejects_runner_tree_and_execution_fabrication(self) -> None:
        mutations = (
            "runner",
            "course-runner",
            "tree",
            "execution",
            "runner-triple",
        )
        for mutation in mutations:
            with (
                self.subTest(mutation=mutation),
                contained_temporary_directory() as temporary,
            ):
                repository_root = Path(temporary).resolve()
                fixture_root, course_root, review = build_attestation_review(
                    repository_root
                )
                if mutation == "runner":
                    runner = repository_root / CAPTURE.CAPTURE_RUNNER_PATH
                    runner.write_bytes(runner.read_bytes() + b"\n")
                elif mutation == "course-runner":
                    execution_path = (
                        course_root / "access-denied" / "course-execution.json"
                    )
                    execution = json.loads(execution_path.read_bytes())
                    execution["capture_runner_sha256"] = "sha256:" + "0" * 64
                    rewrite_json(execution_path, execution)
                elif mutation == "tree":
                    manifest = json.loads(
                        (
                            review
                            / "offline-package"
                            / "attended-review-manifest.json"
                        ).read_bytes()
                    )
                    attestation_identity = next(
                        identity
                        for identity in manifest["subjects"][0]["identity_files"]
                        if identity["label"]
                        == "attestation:live-run-attestation.json"
                    )
                    attestation_path = (
                        review / "offline-package" / attestation_identity["path"]
                    )
                    attestation = json.loads(attestation_path.read_bytes())
                    attestation["course_tree_sha256"]["access-denied"] = (
                        "sha256:" + "0" * 64
                    )
                    rewrite_json(attestation_path, attestation)
                elif mutation == "execution":
                    execution_path = (
                        course_root / "access-denied" / "course-execution.json"
                    )
                    execution = json.loads(execution_path.read_bytes())
                    execution["course"] = "slow-down"
                    rewrite_json(execution_path, execution)
                else:
                    validation_path = review / "verdict-validation.json"
                    validation = json.loads(validation_path.read_bytes())
                    validation["capture_runner"]["sha256"] = "sha256:" + "0" * 64
                    rewrite_json(validation_path, validation)
                with mock.patch.object(CAPTURE, "REPOSITORY_ROOT", repository_root):
                    self.assert_rejected(
                        lambda: CAPTURE.validate_fixture_root(
                            fixture_root,
                            require_complete=True,
                            attestation_review=review,
                            course_evidence_root=course_root,
                        )
                    )

    def test_course_evidence_rejects_missing_extra_and_swapped_files(self) -> None:
        mutations = (
            "missing",
            "extra",
            "symlink",
            "swapped",
            "mixed-registration",
        )
        for mutation in mutations:
            with (
                self.subTest(mutation=mutation),
                contained_temporary_directory() as temporary,
            ):
                repository_root = Path(temporary).resolve()
                fixture_root, course_root, review = build_attestation_review(
                    repository_root
                )
                access_pending = (
                    course_root
                    / "access-denied"
                    / "outcomes"
                    / "authorization_pending.json"
                )
                if mutation == "missing":
                    access_pending.unlink()
                elif mutation == "extra":
                    (course_root / "access-denied" / "unexpected.json").write_text(
                        "{}\n",
                        encoding="utf-8",
                    )
                elif mutation == "symlink":
                    access_pending.unlink()
                    link_or_skip(
                        access_pending,
                        course_root
                        / "slow-down"
                        / "outcomes"
                        / "authorization_pending.json",
                    )
                elif mutation == "swapped":
                    slow_pending = (
                        course_root
                        / "slow-down"
                        / "outcomes"
                        / "authorization_pending.json"
                    )
                    access_payload = access_pending.read_bytes()
                    access_pending.write_bytes(slow_pending.read_bytes())
                    slow_pending.write_bytes(access_payload)
                else:
                    foreign = CAPTURE.sanitize_registration(
                        registration_observation(),
                        captured_at_utc=CAPTURED_AT,
                        source_kind="unattested_capture",
                    )
                    (course_root / "expired-token" / "registration.json").write_bytes(
                        CAPTURE.canonical_json(foreign)
                    )
                with mock.patch.object(CAPTURE, "REPOSITORY_ROOT", repository_root):
                    self.assert_rejected(
                        lambda: CAPTURE.validate_fixture_root(
                            fixture_root,
                            require_complete=True,
                            attestation_review=review,
                            course_evidence_root=course_root,
                        )
                    )

    def test_course_evidence_rejects_changed_final_assembly_source(self) -> None:
        with contained_temporary_directory() as temporary:
            repository_root = Path(temporary).resolve()
            fixture_root, course_root, review = build_attestation_review(
                repository_root
            )
            replacement = (
                course_root
                / "access-denied"
                / "outcomes"
                / "authorization_pending.json"
            ).read_bytes()
            (fixture_root / "outcomes/authorization_pending.json").write_bytes(
                replacement
            )
            artifacts = {
                relative: (fixture_root / relative).read_bytes()
                for relative in CAPTURE.COMPLETE_ARTIFACTS
            }
            (fixture_root / "manifest.json").write_bytes(
                CAPTURE.canonical_json(
                    CAPTURE.build_manifest(sorted(artifacts.items()))
                )
            )
            with mock.patch.object(CAPTURE, "REPOSITORY_ROOT", repository_root):
                self.assert_rejected(
                    lambda: CAPTURE.validate_fixture_root(
                        fixture_root,
                        require_complete=True,
                        attestation_review=review,
                        course_evidence_root=course_root,
                    )
                )

    def test_complete_evidence_requires_own_slow_down_interval_progression(
        self,
    ) -> None:
        def change_slow_course_device_interval(
            courses: dict[str, dict[str, bytes]],
            _fixture: dict[str, bytes],
        ) -> None:
            relative = "outcomes/device_code_success.json"
            document = json.loads(courses["slow-down"][relative])
            document["projection"]["values"]["interval"] = 6
            courses["slow-down"][relative] = CAPTURE.canonical_json(document)

        with contained_temporary_directory() as temporary:
            repository_root = Path(temporary).resolve()
            fixture_root, course_root, review = build_attestation_review(
                repository_root,
                mutate_evidence=change_slow_course_device_interval,
            )
            slow_root = course_root / "slow-down"
            relative = "outcomes/device_code_success.json"
            slow_device_payload = (slow_root / relative).read_bytes()
            slow_device = json.loads(slow_device_payload)
            slow_response = json.loads(
                (slow_root / "outcomes/slow_down.json").read_bytes()
            )
            self.assertEqual(
                slow_device["projection"]["values"]["interval"],
                6,
            )
            self.assertEqual(
                slow_response["projection"]["values"]["interval"],
                10,
            )
            assembled_device = json.loads(
                (fixture_root / "outcomes/device_code_success.json").read_bytes()
            )
            self.assertEqual(
                assembled_device["projection"]["values"]["interval"],
                5,
            )

            slow_records = {
                path.relative_to(slow_root).as_posix(): path.read_bytes()
                for path in slow_root.rglob("*")
                if path.is_file()
            }
            attestation = json.loads(
                (repository_root / "capture/live-run-attestation.json").read_bytes()
            )
            self.assertEqual(
                attestation["course_tree_sha256"]["slow-down"],
                CAPTURE._course_tree_digest(slow_records),
            )

            manifest_path = (
                review / "offline-package/attended-review-manifest.json"
            )
            manifest = json.loads(manifest_path.read_bytes())
            identity = next(
                item
                for item in manifest["subjects"][0]["identity_files"]
                if item["label"] == f"course:slow-down/{relative}"
            )
            self.assertEqual(
                identity["sha256"],
                CAPTURE._sha256(slow_device_payload),
            )
            self.assertEqual(
                (manifest_path.parent / identity["path"]).read_bytes(),
                slow_device_payload,
            )
            verdict = json.loads((review / "human-verdict.json").read_bytes())
            self.assertEqual(
                verdict["accepted_subjects"][0]["subject_digest"],
                manifest["subjects"][0]["subject_digest"],
            )

            with mock.patch.object(CAPTURE, "REPOSITORY_ROOT", repository_root):
                self.assert_rejected(
                    lambda: CAPTURE.validate_fixture_root(
                        fixture_root,
                        require_complete=True,
                        attestation_review=review,
                        course_evidence_root=course_root,
                    )
                )

    def test_slow_down_authority_chain_rejects_missing_tampered_and_unbound_records(
        self,
    ) -> None:
        mutations = (
            "missing-original",
            "missing-recovery",
            "extra",
            "original-authority",
            "original-authority-digest",
            "original-time",
            "failed-runner",
            "original-marker-binding",
            "recovery-authority",
            "recovery-original-binding",
            "recovery-marker-binding",
            "recovery-time",
        )
        for mutation in mutations:
            with (
                self.subTest(mutation=mutation),
                contained_temporary_directory() as temporary,
            ):
                repository_root = Path(temporary).resolve()
                fixture_root, course_root, review = build_attestation_review(
                    repository_root
                )
                authority_path = course_root / CAPTURE.SLOW_DOWN_AUTHORITY_PATH
                spent_path = course_root / CAPTURE.SLOW_DOWN_SPENT_PATH
                recovery_authority_path = (
                    course_root / CAPTURE.SLOW_DOWN_RECOVERY_AUTHORITY_PATH
                )
                recovery_spent_path = (
                    course_root / CAPTURE.SLOW_DOWN_RECOVERY_SPENT_PATH
                )
                if mutation == "missing-original":
                    authority_path.unlink()
                elif mutation == "missing-recovery":
                    recovery_authority_path.unlink()
                elif mutation == "extra":
                    (course_root / "unexpected.json").write_text(
                        "{}\n",
                        encoding="utf-8",
                    )
                elif mutation == "original-authority":
                    authority = json.loads(authority_path.read_bytes())
                    authority["previous_early_poll_consumed"] = False
                    rewrite_json(authority_path, authority)
                elif mutation == "original-authority-digest":
                    spent = json.loads(spent_path.read_bytes())
                    spent["authority_record_sha256"] = "sha256:" + "0" * 64
                    rewrite_json(spent_path, spent)
                elif mutation == "original-time":
                    spent = json.loads(spent_path.read_bytes())
                    spent["spent_before_traffic_at_utc"] = "2026-08-13T16:03:01Z"
                    rewrite_json(spent_path, spent)
                elif mutation == "failed-runner":
                    spent = json.loads(spent_path.read_bytes())
                    spent["capture_runner_sha256"] = "sha256:" + "0" * 64
                    rewrite_json(spent_path, spent)
                elif mutation in {
                    "original-marker-binding",
                    "recovery-marker-binding",
                }:
                    execution_path = (
                        course_root / "slow-down" / "course-execution.json"
                    )
                    execution = json.loads(execution_path.read_bytes())
                    key = (
                        "slow_down_original_reservation_sha256"
                        if mutation == "original-marker-binding"
                        else "slow_down_recovery_reservation_sha256"
                    )
                    execution["bindings"][key] = "sha256:" + "0" * 64
                    rewrite_json(execution_path, execution)
                elif mutation == "recovery-authority":
                    authority = json.loads(recovery_authority_path.read_bytes())
                    authority["recovery_early_poll_limit"] = True
                    rewrite_json(recovery_authority_path, authority)
                elif mutation == "recovery-original-binding":
                    marker = json.loads(recovery_spent_path.read_bytes())
                    marker["failed_attempt_spent_marker_sha256"] = (
                        "sha256:" + "0" * 64
                    )
                    rewrite_json(recovery_spent_path, marker)
                else:
                    marker = json.loads(recovery_spent_path.read_bytes())
                    marker["spent_before_traffic_at_utc"] = (
                        "2026-08-13T16:03:01Z"
                    )
                    rewrite_json(recovery_spent_path, marker)
                with mock.patch.object(CAPTURE, "REPOSITORY_ROOT", repository_root):
                    self.assert_rejected(
                        lambda: CAPTURE.validate_fixture_root(
                            fixture_root,
                            require_complete=True,
                            attestation_review=review,
                            course_evidence_root=course_root,
                        )
                    )

    def test_attended_review_rejects_changed_input_page_and_validator_proof(self) -> None:
        mutations = ("input", "page", "validator", "self-authored-only")
        for mutation in mutations:
            with (
                self.subTest(mutation=mutation),
                contained_temporary_directory() as temporary,
            ):
                repository_root = Path(temporary).resolve()
                fixture_root, course_root, review = build_attestation_review(
                    repository_root
                )
                validation_path = review / "verdict-validation.json"
                if mutation == "input":
                    path = review / "offline-review-input.json"
                    document = json.loads(path.read_bytes())
                    document["review_scope"] = "other_scope"
                    rewrite_json(path, document)
                    refresh_validation_triple(
                        review, repository_root, "review_input", path
                    )
                elif mutation == "page":
                    path = review / "offline-package" / "index.html"
                    path.write_bytes(path.read_bytes() + b"<!-- changed -->\n")
                    refresh_validation_triple(
                        review, repository_root, "review_page", path
                    )
                elif mutation == "validator":
                    validation = json.loads(validation_path.read_bytes())
                    validation["official_validator"]["sha256"] = "sha256:" + "0" * 64
                    rewrite_json(validation_path, validation)
                else:
                    validation_path.unlink()
                with mock.patch.object(CAPTURE, "REPOSITORY_ROOT", repository_root):
                    self.assert_rejected(
                        lambda: CAPTURE.validate_fixture_root(
                            fixture_root,
                            require_complete=True,
                            attestation_review=review,
                            course_evidence_root=course_root,
                        )
                    )

    def test_attestation_and_validation_triples_require_exact_integer_types(self) -> None:
        with contained_temporary_directory() as temporary:
            repository_root = Path(temporary).resolve()
            fixture_root, course_root, review = build_attestation_review(
                repository_root
            )
            validation_path = review / "verdict-validation.json"
            validation = json.loads(validation_path.read_bytes())
            validation["manifest"]["byte_length"] = True
            rewrite_json(validation_path, validation)
            with mock.patch.object(CAPTURE, "REPOSITORY_ROOT", repository_root):
                fixture_payloads = {
                    path.relative_to(fixture_root).as_posix(): path.read_bytes()
                    for path in fixture_root.rglob("*")
                    if path.is_file()
                }
                (
                    _courses,
                    _authority_records,
                    executions,
                    course_digests,
                    runner_digest,
                ) = CAPTURE._validate_course_evidence(
                    course_root,
                    fixture_payloads,
                )
                attestation = json.loads(
                    (repository_root / "capture/live-run-attestation.json").read_bytes()
                )
                attestation["schema_version"] = True
                self.assert_rejected(
                    lambda: CAPTURE._validate_live_run_attestation(
                        CAPTURE.canonical_json(attestation),
                        fixture_payloads,
                        executions=executions,
                        course_digests=course_digests,
                        runner_digest=runner_digest,
                    )
                )
                self.assert_rejected(
                    lambda: CAPTURE.validate_fixture_root(
                        fixture_root,
                        require_complete=True,
                        attestation_review=review,
                        course_evidence_root=course_root,
                    )
                )

    def test_full_attended_path_rejects_boolean_schema_version_aliases(self) -> None:
        def replace_registration(
            courses: dict[str, dict[str, bytes]],
            fixture: dict[str, bytes],
        ) -> None:
            registration = json.loads(
                courses["token-success"]["registration.json"]
            )
            registration["schema_version"] = True
            registration_payload = CAPTURE.canonical_json(registration)
            registration_digest = CAPTURE._sha256(registration_payload)
            for records in courses.values():
                records["registration.json"] = registration_payload
                for relative, payload in list(records.items()):
                    if relative.startswith("outcomes/") or relative == "route-probes.json":
                        document = json.loads(payload)
                        document["provenance"]["registration_record_sha256"] = (
                            registration_digest
                        )
                        records[relative] = CAPTURE.canonical_json(document)
                execution = json.loads(records["course-execution.json"])
                execution["registration_record_sha256"] = registration_digest
                records["course-execution.json"] = CAPTURE.canonical_json(
                    execution
                )
            fixture.clear()
            fixture.update(unvalidated_assembled_fixture(courses))

        def replace_course_schema(
            course: str,
            relative: str,
        ) -> Callable[
            [dict[str, dict[str, bytes]], dict[str, bytes]],
            None,
        ]:
            def mutate(
                courses: dict[str, dict[str, bytes]],
                fixture: dict[str, bytes],
            ) -> None:
                document = json.loads(courses[course][relative])
                document["schema_version"] = True
                courses[course][relative] = CAPTURE.canonical_json(document)
                fixture.clear()
                fixture.update(unvalidated_assembled_fixture(courses))

            return mutate

        def replace_manifest(
            _courses: dict[str, dict[str, bytes]],
            fixture: dict[str, bytes],
        ) -> None:
            manifest = json.loads(fixture["manifest.json"])
            manifest["schema_version"] = True
            fixture["manifest.json"] = CAPTURE.canonical_json(manifest)

        cases = (
            ("registration", replace_registration),
            (
                "outcome",
                replace_course_schema(
                    "token-success",
                    "outcomes/device_code_success.json",
                ),
            ),
            (
                "route-probes",
                replace_course_schema("token-success", "route-probes.json"),
            ),
            ("manifest", replace_manifest),
        )
        for name, mutate_evidence in cases:
            with (
                self.subTest(name=name),
                contained_temporary_directory() as temporary,
            ):
                repository_root = Path(temporary).resolve()
                fixture_root, course_root, review = build_attestation_review(
                    repository_root,
                    mutate_evidence=mutate_evidence,
                )
                with mock.patch.object(
                    CAPTURE,
                    "REPOSITORY_ROOT",
                    repository_root,
                ):
                    self.assert_rejected(
                        lambda: CAPTURE.validate_fixture_root(
                            fixture_root,
                            require_complete=True,
                            attestation_review=review,
                            course_evidence_root=course_root,
                        )
                    )

        with contained_temporary_directory() as temporary:
            pending_root = Path(temporary)
            pending = {
                "authority_subject_sha256": CAPTURE.AUTHORITY_SUBJECT_SHA256,
                "schema_name": "bpr.rfc0016.device_capture_state",
                "schema_version": True,
                "state": "pending_real_capture",
            }
            (pending_root / "pending.json").write_bytes(
                CAPTURE.canonical_json(pending)
            )
            self.assert_rejected(
                lambda: CAPTURE.validate_fixture_root(pending_root)
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
        with contained_temporary_directory() as temporary:
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
        with contained_temporary_directory() as temporary:
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

        with contained_temporary_directory() as temporary:
            root = Path(temporary)
            write_synthetic_complete_root(root)
            pending = root / "outcomes" / "authorization_pending.json"
            pending.write_bytes(pending.read_bytes().replace(b"\n", b"\r\n"))
            self.assert_rejected(lambda: CAPTURE.validate_fixture_root(root))

    def test_fixture_size_and_outside_root_link_are_rejected(self) -> None:
        with contained_temporary_directory() as temporary:
            root = Path(temporary)
            (root / "pending.json").write_bytes(
                b"x" * (CAPTURE.MAX_JSON_BYTES + 1)
            )
            self.assert_rejected(lambda: CAPTURE.validate_fixture_root(root))

        with contained_temporary_directory() as temporary:
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

    def test_slow_down_interval_is_retained_as_non_secret_contract_data(self) -> None:
        artifact = CAPTURE.sanitize_outcome(
            "slow_down",
            outcome_bodies()["slow_down"],
            http_status=200,
            captured_at_utc=CAPTURED_AT,
            source_kind="synthetic",
            registration_record_sha256=SYNTHETIC_REGISTRATION_SHA256,
        )
        self.assertEqual(
            artifact["projection"]["observed_fields"],
            ["error", "error_description", "error_uri", "interval"],
        )
        self.assertEqual(
            artifact["projection"]["values"],
            {
                "error": "slow_down",
                "interval": 10,
                "official_error_uri": True,
            },
        )
        CAPTURE.validate_outcome_artifact(artifact, require_real=False)

    def test_slow_down_interval_sanitizer_and_artifact_are_exact_typed(self) -> None:
        valid_artifact = CAPTURE.sanitize_outcome(
            "slow_down",
            outcome_bodies()["slow_down"],
            http_status=200,
            captured_at_utc=CAPTURED_AT,
            source_kind="synthetic",
            registration_record_sha256=SYNTHETIC_REGISTRATION_SHA256,
        )
        for interval in (None, True, 1.5, "10", 0, -1, 301):
            raw = json.loads(outcome_bodies()["slow_down"])
            hostile_artifact = copy.deepcopy(valid_artifact)
            if interval is None:
                raw.pop("interval")
                hostile_artifact["projection"]["values"].pop("interval")
            else:
                raw["interval"] = interval
                hostile_artifact["projection"]["values"]["interval"] = interval
            with self.subTest(interval=interval):
                self.assert_rejected(
                    lambda raw=raw: CAPTURE.sanitize_outcome(
                        "slow_down",
                        encode(raw),
                        http_status=200,
                        captured_at_utc=CAPTURED_AT,
                        source_kind="synthetic",
                        registration_record_sha256=(
                            SYNTHETIC_REGISTRATION_SHA256
                        ),
                    )
                )
                self.assert_rejected(
                    lambda hostile_artifact=hostile_artifact: (
                        CAPTURE.validate_outcome_artifact(
                            hostile_artifact,
                            require_real=False,
                        )
                    )
                )

    def test_interval_is_rejected_on_every_non_slow_error_contract(self) -> None:
        for outcome in (
            "access_denied",
            "authorization_pending",
            "device_flow_disabled",
            "expired_token",
        ):
            raw = json.loads(outcome_bodies()[outcome])
            raw["interval"] = 10
            artifact = CAPTURE.sanitize_outcome(
                outcome,
                outcome_bodies()[outcome],
                http_status=200,
                captured_at_utc=CAPTURED_AT,
                source_kind="synthetic",
                registration_record_sha256=SYNTHETIC_REGISTRATION_SHA256,
            )
            artifact["projection"]["values"]["interval"] = 10
            with self.subTest(outcome=outcome):
                self.assert_rejected(
                    lambda outcome=outcome, raw=raw: CAPTURE.sanitize_outcome(
                        outcome,
                        encode(raw),
                        http_status=200,
                        captured_at_utc=CAPTURED_AT,
                        source_kind="synthetic",
                        registration_record_sha256=(
                            SYNTHETIC_REGISTRATION_SHA256
                        ),
                    )
                )
                self.assert_rejected(
                    lambda artifact=artifact: CAPTURE.validate_outcome_artifact(
                        artifact,
                        require_real=False,
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
        with contained_temporary_directory() as temporary:
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
            "sha256:e4991e65a368fbbd51c7ba19793eb182fdbecc2500594de5bcd9dbdba255793e",
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
                "unattested_capture",
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
            "RFC0016_DEVICE_CAPTURE_VALID:unattested_capture\n",
        )

        with contained_temporary_directory() as temporary:
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
