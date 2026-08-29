#!/usr/bin/env python3
"""Falsify `scripts/verify_rfc0016_production_identity.py` before it is trusted.

The verifier is the only thing standing between a plausible directory of JSON
and the claim that BetterPullReview has a distributable GitHub App identity. A
gate like that is worth nothing unless the wrong evidence actually fails it, so
most of this file builds a valid course and then breaks exactly one thing.

The mutations are chosen to cross seams rather than to sit inside one file.
Every field in a single document can be checked correctly while the join
between two documents goes unread, and it is the join that carries the meaning
here: that the course ran as the identity the observation records, under the
subject the owner approved, against the registration bytes sitting beside it.
So a registration-field mutation is applied *before* the tree is digested, and
the tree stays internally consistent — otherwise the run would fail on a stale
digest and the field check would never be reached, which is a green test for a
red reason.

The valid tree is also the shape the attended sitting has to produce, so its
positive controls double as the schema the runner is being asked for.

Owned by BPR-0318.
"""

from __future__ import annotations

import ast
import json
import subprocess
import sys
import unittest
from pathlib import Path
from typing import Any


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.contained_temporary_support import (  # noqa: E402
    contained_temporary_directory,
)
from scripts.verify_rfc0016_production_identity import (  # noqa: E402
    APPROVED_CLAIM,
    APPROVED_REPOSITORY_PERMISSIONS,
    DESIGNATED_DATABASE_ID,
    DESIGNATED_NAME,
    DESIGNATED_OWNER,
    DESIGNATED_PULL_REQUEST,
    DESIGNATED_REPOSITORY,
    DEVICE_CODE_URL,
    GATE,
    OFFICIAL_VALIDATOR,
    TOKEN_URL,
    VERIFICATION_CLIENT_ID,
    IdentityFailure,
    check_committed,
    check_course_evidence,
    check_operator_record,
    check_recorded_decision,
    check_registration_observation,
    check_secret_free,
    check_selected_client,
    designated_route_catalog,
    evidence_files,
    sha256,
)


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
TOOL_PATH = REPOSITORY_ROOT / "scripts" / "verify_rfc0016_production_identity.py"
REVIEW_DIRECTORY = (
    REPOSITORY_ROOT
    / "docs"
    / "verification"
    / "reviews"
    / "rfc0016-production-app-registration-v001"
)

PRODUCTION_CLIENT = "Iv23liitiYN1WSSbag50"
FOREIGN_CLIENT = "Iv23liZZZZZZZZZZZZZZ"
SUBJECT_DIGEST = (
    "sha256:18b867d3a05321cedcd4570227c732459779bebff23728d354de9411c4e5370b"
)
FOREIGN_SUBJECT = (
    "sha256:d3cfb906170799ee7c90a8290cabee1b1e7e6afef40937a08f7b21bd5b42d178"
)

GRAPHQL_DOCUMENT = (
    "query GetReviewData($owner:String!,$name:String!,$number:Int!)"
    "{repository(owner:$owner,name:$name){id pullRequest(number:$number)"
    "{id headRefOid}}}"
)


def write_json(path: Path, document: dict[str, Any]) -> bytes:
    payload = (json.dumps(document, indent=2, sort_keys=True) + "\n").encode("utf-8")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    return payload


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def registration_fields(client: str) -> dict[str, Any]:
    """The nine approved facts, as a registration record records them."""
    return {
        "application_kind": "github_app",
        "application_visibility": "public",
        "callback_url_present": False,
        "device_flow_enabled": True,
        "installation_owner_login": DESIGNATED_OWNER,
        "installation_policy": "any_account",
        "installation_repositories": [DESIGNATED_REPOSITORY],
        "installation_repository_selection": "selected_repositories",
        "oauth_client_type": "public",
        "oauth_scopes": [],
        "owner_kind": "user",
        "owner_login": DESIGNATED_OWNER,
        "permissions": {
            "account": {},
            "enterprise": {},
            "organization": {},
            "repository": dict(APPROVED_REPOSITORY_PERMISSIONS),
        },
        "public_client_id": client,
        "refresh_policy": "expiring_user_token_with_refresh",
        "user_access_token_expiration_enabled": True,
        "webhook_active": False,
    }


def write_observation(
    directory: Path,
    *,
    client: str = PRODUCTION_CLIENT,
    overrides: dict[str, Any] | None = None,
) -> Path:
    observation = registration_fields(client)
    if overrides:
        observation.update(overrides)
    write_json(
        directory / "registration-observation.json",
        {
            "captured_at_utc": "2026-08-28T02:38:00Z",
            "observation": observation,
            "schema_name": "bpr.rfc0016.registration_observation",
            "schema_version": 1,
        },
    )
    return directory


def build_course(
    root: Path,
    *,
    client: str = PRODUCTION_CLIENT,
    subject: str = SUBJECT_DIGEST,
    registration_overrides: dict[str, Any] | None = None,
) -> Path:
    """A complete, internally consistent designated read-only course."""
    course = root / "read-only-course"
    registration: dict[str, Any] = {
        "schema_name": "bpr.rfc0016.github_app_registration",
        "schema_version": 1,
        **registration_fields(client),
    }
    if registration_overrides:
        registration.update(registration_overrides)
    registration_digest = sha256(write_json(course / "registration.json", registration))

    def provenance(observation: str) -> dict[str, Any]:
        return {
            "authority_subject_sha256": subject,
            "capture_tool": "scripts/run_rfc0016_device_capture_attended.py",
            "captured_at_utc": "2026-08-29T01:02:03Z",
            "gate": GATE,
            "observation": observation,
            "registration_record_sha256": registration_digest,
            "service": "github.com",
            "source_kind": "unattested_capture",
        }

    write_json(
        course / "course-execution.json",
        {
            "authority_subject_sha256": subject,
            "course": "token-success",
            "executed_at_utc": "2026-08-29T01:02:03Z",
            "gate": GATE,
            "registration_record_sha256": registration_digest,
            "schema_name": "bpr.rfc0016.course_execution",
            "schema_version": 1,
        },
    )

    write_json(
        course / "outcomes" / "device_code_success.json",
        {
            "content_type": "application/json",
            "endpoint": "device_code",
            "endpoint_url": DEVICE_CODE_URL,
            "http_status": 200,
            "outcome": "device_code_success",
            "projection": {
                "observed_fields": [
                    "device_code",
                    "expires_in",
                    "interval",
                    "user_code",
                    "verification_uri",
                ],
                "omitted_transient_fields": ["device_code", "user_code"],
                "values": {
                    "expires_in": 899,
                    "interval": 5,
                    "verification_uri": "https://github.com/login/device",
                },
            },
            "provenance": provenance("device_code_success_response"),
            "request_content_type": "application/x-www-form-urlencoded",
            "request_method": "POST",
            "schema_name": "bpr.rfc0016.device_contract",
            "schema_version": 1,
        },
    )

    write_json(
        course / "outcomes" / "token_success.json",
        {
            "content_type": "application/json",
            "endpoint": "token",
            "endpoint_url": TOKEN_URL,
            "http_status": 200,
            "outcome": "token_success",
            "projection": {
                "observed_fields": [
                    "access_token",
                    "expires_in",
                    "refresh_token",
                    "refresh_token_expires_in",
                    "scope",
                    "token_type",
                ],
                "omitted_transient_fields": ["access_token", "refresh_token"],
                "values": {
                    "expires_in": 28800,
                    "refresh_token_expires_in": 15897600,
                    "scope": "",
                    "token_type": "bearer",
                },
            },
            "provenance": provenance("token_success_response"),
            "request_content_type": "application/x-www-form-urlencoded",
            "request_method": "POST",
            "schema_name": "bpr.rfc0016.device_contract",
            "schema_version": 1,
        },
    )

    probes = [
        {
            "http_status": 200,
            "method": method,
            "operation": operation,
            "request_contract_sha256": sha256(operation.encode("utf-8")),
            "result": "success",
            "transport": transport,
            "url": url,
        }
        for operation, (method, transport, url) in designated_route_catalog().items()
    ]
    write_json(
        course / "route-probes.json",
        {
            "graphql_document": GRAPHQL_DOCUMENT,
            "graphql_document_sha256": sha256(GRAPHQL_DOCUMENT.encode("utf-8")),
            "graphql_variables": {
                "name": DESIGNATED_NAME,
                "number": DESIGNATED_PULL_REQUEST,
                "owner": DESIGNATED_OWNER,
            },
            "probes": probes,
            "provenance": provenance("read_only_route_probes"),
            "repository": {
                "database_id": DESIGNATED_DATABASE_ID,
                "name": DESIGNATED_NAME,
                "owner": DESIGNATED_OWNER,
                "pull_request_number": DESIGNATED_PULL_REQUEST,
            },
            "schema_name": "bpr.rfc0016.read_only_route_probes",
            "schema_version": 1,
        },
    )
    return course


def admit(course: Path, *, client: str = PRODUCTION_CLIENT) -> int:
    return check_course_evidence(
        course,
        production_client=client,
        subject_digest=SUBJECT_DIGEST,
    )


class ValidEvidenceTests(unittest.TestCase):
    """The shape the attended sitting must produce is accepted."""

    def test_a_complete_course_is_admitted(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            self.assertEqual(admit(course), len(designated_route_catalog()))

    def test_the_optional_pending_outcome_is_permitted(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            write_json(
                course / "outcomes" / "authorization_pending.json",
                {
                    "content_type": "application/json",
                    "endpoint": "token",
                    "endpoint_url": TOKEN_URL,
                    "http_status": 200,
                    "outcome": "authorization_pending",
                    "projection": {
                        "observed_fields": ["error", "error_description", "error_uri"],
                        "omitted_transient_fields": ["error_description", "error_uri"],
                        "values": {"error": "authorization_pending"},
                    },
                    "provenance": read_json(course / "route-probes.json")["provenance"]
                    | {"observation": "authorization_pending_response"},
                    "request_content_type": "application/x-www-form-urlencoded",
                    "request_method": "POST",
                    "schema_name": "bpr.rfc0016.device_contract",
                    "schema_version": 1,
                },
            )
            self.assertEqual(admit(course), len(designated_route_catalog()))

    def test_a_pending_outcome_may_be_400(self) -> None:
        """GitHub sends the pending error as 200 or 400; both are correct."""
        for status in (200, 400):
            with self.subTest(status=status):
                with contained_temporary_directory() as directory:
                    course = build_course(Path(directory))
                    write_json(
                        course / "outcomes" / "authorization_pending.json",
                        {
                            "content_type": "application/json",
                            "endpoint": "token",
                            "endpoint_url": TOKEN_URL,
                            "http_status": status,
                            "outcome": "authorization_pending",
                            "projection": {
                                "observed_fields": [
                                    "error",
                                    "error_description",
                                    "error_uri",
                                ],
                                "omitted_transient_fields": [
                                    "error_description",
                                    "error_uri",
                                ],
                                "values": {"error": "authorization_pending"},
                            },
                            "provenance": read_json(course / "route-probes.json")[
                                "provenance"
                            ]
                            | {"observation": "authorization_pending_response"},
                            "request_content_type": "application/x-www-form-urlencoded",
                            "request_method": "POST",
                            "schema_name": "bpr.rfc0016.device_contract",
                            "schema_version": 1,
                        },
                    )
                    self.assertEqual(admit(course), len(designated_route_catalog()))

    def test_a_token_success_may_not_be_400(self) -> None:
        """The permission a pending error gets does not extend to a success."""
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            path = course / "outcomes" / "token_success.json"
            document = read_json(path)
            document["http_status"] = 400
            write_json(path, document)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("outcome:token_success:http-status", str(caught.exception))

    def test_a_valid_observation_yields_the_production_client(self) -> None:
        with contained_temporary_directory() as directory:
            review = write_observation(Path(directory))
            self.assertEqual(check_registration_observation(review), PRODUCTION_CLIENT)

    def test_the_course_shape_carries_no_secret_bearing_material(self) -> None:
        """The evidence the runner must write survives its own byte scan."""
        with contained_temporary_directory() as directory:
            root = Path(directory)
            build_course(root)
            write_observation(root)
            scanned = evidence_files(root)
            self.assertGreater(len(scanned), 0)
            self.assertEqual(check_secret_free(scanned), len(scanned))

    def test_the_committed_review_directory_carries_no_secret(self) -> None:
        """A false positive here would block the real gate, so it is a control."""
        scanned = evidence_files(REVIEW_DIRECTORY)
        self.assertGreater(len(scanned), 0)
        self.assertEqual(check_secret_free(scanned), len(scanned))


ADJACENT_FALSE_CLAIMS = (
    "distribution_authorized",
    "ghes_registration_authorized",
    "platform_capability_flip_authorized",
    "release_authorized",
    "repository_mutation_authorized",
    "rfc0016_closeout_authorized",
    "secret_bearing_evidence_authorized",
    "unbounded_repository_access_authorized",
    "undesignated_target_access_authorized",
    "verification_identity_widening_authorized",
)


def build_review(
    root: Path,
    *,
    record_overrides: dict[str, Any] | None = None,
) -> Path:
    """A committed-decision directory whose record binds the files beside it."""
    review = root / "review"
    review.mkdir(parents=True, exist_ok=True)

    contract = write_json(
        review / "claims-contract.json",
        {
            "adjacent_false_claims": list(ADJACENT_FALSE_CLAIMS),
            "approved_claim": APPROVED_CLAIM,
            "gate": GATE,
            "schema_name": "bpr.operator_claim_contract",
            "schema_version": 1,
        },
    )
    artifacts = {"claims-contract.json": contract}
    for name, text in (
        ("decision-subject.txt", f"gate={GATE}\n"),
        ("operator-response.md", "> I approve.\n"),
        ("operator-packet.md", "# packet\n"),
    ):
        payload = text.encode("utf-8")
        (review / name).write_bytes(payload)
        artifacts[name] = payload

    def binding(name: str) -> dict[str, Any]:
        return {
            "byte_length": len(artifacts[name]),
            "path": name,
            "sha256": sha256(artifacts[name]),
        }

    record: dict[str, Any] = {
        "checks": {
            "approval_statement_exact": True,
            "committed_artifacts_exact": True,
            "decision_subject_digest_bound": True,
            "overall": "pass",
        },
        "claims": {APPROVED_CLAIM: True}
        | {name: False for name in ADJACENT_FALSE_CLAIMS},
        "claims_contract": binding("claims-contract.json"),
        "decision_subject": binding("decision-subject.txt"),
        "gate": GATE,
        "operator_response": binding("operator-response.md"),
        "outcome": "approved",
        "result": "pass",
        "reviewed_packet": binding("operator-packet.md"),
        "schema_name": "bpr.operator_response_validation",
        "schema_version": 1,
    }
    if record_overrides:
        record.update(record_overrides)
    write_json(review / "validation-record.json", record)
    return review


class RecordedDecisionTests(unittest.TestCase):
    """What the accepted record says, checked without a repository."""

    def test_a_valid_record_yields_its_subject_digest(self) -> None:
        with contained_temporary_directory() as directory:
            review = build_review(Path(directory))
            expected = read_json(review / "validation-record.json")
            self.assertEqual(
                check_recorded_decision(review),
                expected["decision_subject"]["sha256"],
            )

    def assert_record_rejected(
        self,
        overrides: dict[str, Any],
        expected: str,
    ) -> None:
        with contained_temporary_directory() as directory:
            review = build_review(Path(directory), record_overrides=overrides)
            with self.assertRaises(IdentityFailure) as caught:
                check_recorded_decision(review)
            self.assertIn(expected, str(caught.exception))

    def test_a_record_for_another_gate_fails(self) -> None:
        self.assert_record_rejected({"gate": "other-gate-v001"}, "validation-record:gate")

    def test_a_failing_record_fails(self) -> None:
        self.assert_record_rejected({"result": "fail"}, "validation-record:result")

    def test_a_rejected_outcome_fails(self) -> None:
        self.assert_record_rejected(
            {"outcome": "rejected"},
            "validation-record:outcome",
        )

    def test_a_record_whose_checks_did_not_pass_overall_fails(self) -> None:
        self.assert_record_rejected(
            {
                "checks": {
                    "approval_statement_exact": True,
                    "committed_artifacts_exact": True,
                    "decision_subject_digest_bound": True,
                    "overall": "fail",
                }
            },
            "validation-record:checks-overall",
        )

    def test_an_unbound_decision_subject_fails(self) -> None:
        self.assert_record_rejected(
            {
                "checks": {
                    "approval_statement_exact": True,
                    "committed_artifacts_exact": True,
                    "decision_subject_digest_bound": False,
                    "overall": "pass",
                }
            },
            "check-not-true:decision_subject_digest_bound",
        )

    def test_an_inexact_approval_statement_fails(self) -> None:
        self.assert_record_rejected(
            {
                "checks": {
                    "approval_statement_exact": 1,
                    "committed_artifacts_exact": True,
                    "decision_subject_digest_bound": True,
                    "overall": "pass",
                }
            },
            "check-not-true:approval_statement_exact",
        )

    def test_an_unapproved_claim_fails(self) -> None:
        self.assert_record_rejected(
            {
                "claims": {APPROVED_CLAIM: False}
                | {name: False for name in ADJACENT_FALSE_CLAIMS}
            },
            "approved-claim-not-true",
        )

    def test_an_adjacent_claim_asserted_true_fails(self) -> None:
        claims = {APPROVED_CLAIM: True} | {
            name: False for name in ADJACENT_FALSE_CLAIMS
        }
        claims["platform_capability_flip_authorized"] = True
        self.assert_record_rejected(
            {"claims": claims},
            "adjacent-claim-not-false:platform_capability_flip_authorized",
        )

    def test_a_claim_outside_the_contract_fails(self) -> None:
        claims = {APPROVED_CLAIM: True} | {
            name: False for name in ADJACENT_FALSE_CLAIMS
        }
        claims["some_other_authority"] = True
        self.assert_record_rejected(
            {"claims": claims},
            "claims-not-exactly-the-contract",
        )

    def test_an_artifact_edited_after_the_record_fails(self) -> None:
        with contained_temporary_directory() as directory:
            review = build_review(Path(directory))
            (review / "operator-response.md").write_bytes(b"> I approve, mostly.\n")
            with self.assertRaises(IdentityFailure) as caught:
                check_recorded_decision(review)
            self.assertIn("operator_response-digest-mismatch", str(caught.exception))

    def test_a_record_whose_byte_length_disagrees_fails(self) -> None:
        with contained_temporary_directory() as directory:
            review = build_review(Path(directory))
            record = read_json(review / "validation-record.json")
            record["reviewed_packet"]["byte_length"] += 1
            write_json(review / "validation-record.json", record)
            with self.assertRaises(IdentityFailure) as caught:
                check_recorded_decision(review)
            self.assertIn("reviewed_packet-length-mismatch", str(caught.exception))

    def test_a_contract_for_another_gate_fails(self) -> None:
        with contained_temporary_directory() as directory:
            review = build_review(Path(directory))
            contract = read_json(review / "claims-contract.json")
            contract["gate"] = "other-gate-v001"
            payload = write_json(review / "claims-contract.json", contract)
            record = read_json(review / "validation-record.json")
            record["claims_contract"] = {
                "byte_length": len(payload),
                "path": "claims-contract.json",
                "sha256": sha256(payload),
            }
            write_json(review / "validation-record.json", record)
            with self.assertRaises(IdentityFailure) as caught:
                check_recorded_decision(review)
            self.assertIn("claims-contract:gate", str(caught.exception))


class SeamMutationTests(unittest.TestCase):
    """Break exactly one join between two documents; the run must fail."""

    def test_a_course_run_as_another_registered_client_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory), client=FOREIGN_CLIENT)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("client-differs", str(caught.exception))

    def test_a_course_run_as_the_verification_client_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory), client=VERIFICATION_CLIENT_ID)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course, client=VERIFICATION_CLIENT_ID)
            self.assertIn("private-verification-identity", str(caught.exception))

    def test_an_observation_naming_the_verification_client_fails(self) -> None:
        with contained_temporary_directory() as directory:
            review = write_observation(Path(directory), client=VERIFICATION_CLIENT_ID)
            with self.assertRaises(IdentityFailure) as caught:
                check_registration_observation(review)
            self.assertIn("private-verification-identity", str(caught.exception))

    def test_a_registration_edited_after_the_course_ran_fails(self) -> None:
        """The digest join, not the field: the execution record goes stale."""
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            registration = read_json(course / "registration.json")
            registration["webhook_active"] = False
            registration["installation_repositories"] = [DESIGNATED_REPOSITORY]
            registration["captured_at_utc"] = "2026-08-30T00:00:00Z"
            write_json(course / "registration.json", registration)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("registration-digest-mismatch", str(caught.exception))

    def test_an_execution_record_bound_to_another_registration_fails(self) -> None:
        """Isolated: only the execution record's digest moves, so only it can fail."""
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            execution = read_json(course / "course-execution.json")
            execution["registration_record_sha256"] = sha256(b"other")
            write_json(course / "course-execution.json", execution)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn(
                "course-execution:registration-digest-mismatch",
                str(caught.exception),
            )

    def test_an_execution_record_bound_to_another_subject_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            execution = read_json(course / "course-execution.json")
            execution["authority_subject_sha256"] = FOREIGN_SUBJECT
            write_json(course / "course-execution.json", execution)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn(
                "course-execution:not-bound-to-approved-subject",
                str(caught.exception),
            )

    def test_an_execution_record_for_another_gate_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            execution = read_json(course / "course-execution.json")
            execution["gate"] = "rfc0016-github-app-registration-v001"
            write_json(course / "course-execution.json", execution)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("course-execution:gate", str(caught.exception))

    def test_probe_provenance_naming_another_gate_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            probes = read_json(course / "route-probes.json")
            probes["provenance"]["gate"] = "rfc0016-github-app-registration-v001"
            write_json(course / "route-probes.json", probes)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("route-probes:provenance-gate", str(caught.exception))

    def test_outcome_provenance_naming_another_gate_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            path = course / "outcomes" / "token_success.json"
            outcome = read_json(path)
            outcome["provenance"]["gate"] = "rfc0016-attended-live-capture-v001"
            write_json(path, outcome)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("outcome:token_success:provenance-gate", str(caught.exception))

    def test_outcome_provenance_bound_to_another_registration_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            path = course / "outcomes" / "device_code_success.json"
            outcome = read_json(path)
            outcome["provenance"]["registration_record_sha256"] = sha256(b"other")
            write_json(path, outcome)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn(
                "outcome:device_code_success:provenance-registration-digest-mismatch",
                str(caught.exception),
            )

    def test_probe_provenance_bound_to_another_registration_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            probes = read_json(course / "route-probes.json")
            probes["provenance"]["registration_record_sha256"] = sha256(b"other")
            write_json(course / "route-probes.json", probes)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("registration-digest-mismatch", str(caught.exception))

    def test_a_course_bound_to_another_authority_subject_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory), subject=FOREIGN_SUBJECT)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("approved-subject", str(caught.exception))

    def test_an_outcome_bound_to_another_authority_subject_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            outcome = read_json(course / "outcomes" / "token_success.json")
            outcome["provenance"]["authority_subject_sha256"] = FOREIGN_SUBJECT
            write_json(course / "outcomes" / "token_success.json", outcome)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("approved-subject", str(caught.exception))


class RouteClosureTests(unittest.TestCase):
    """The course reached the designated target's read-only catalog and no more."""

    def test_a_probe_against_another_repository_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            probes = read_json(course / "route-probes.json")
            probes["probes"][0]["url"] = (
                f"https://api.github.com/repos/{DESIGNATED_OWNER}/Another"
            )
            write_json(course / "route-probes.json", probes)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("url-not-designated", str(caught.exception))

    def test_a_probe_against_another_pull_request_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            probes = read_json(course / "route-probes.json")
            for probe in probes["probes"]:
                if probe["operation"] == "pull_request":
                    probe["url"] = probe["url"].replace("/pulls/1", "/pulls/2")
            write_json(course / "route-probes.json", probes)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("url-not-designated", str(caught.exception))

    def test_a_mutating_method_on_a_designated_route_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            probes = read_json(course / "route-probes.json")
            for probe in probes["probes"]:
                if probe["operation"] == "pull_request_reviews":
                    probe["method"] = "POST"
            write_json(course / "route-probes.json", probes)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("method", str(caught.exception))

    def test_an_operation_outside_the_catalog_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            probes = read_json(course / "route-probes.json")
            probes["probes"].append(
                {
                    "http_status": 200,
                    "method": "POST",
                    "operation": "create_review",
                    "request_contract_sha256": sha256(b"create_review"),
                    "result": "success",
                    "transport": "rest",
                    "url": f"https://api.github.com/repos/{DESIGNATED_REPOSITORY}"
                    "/pulls/1/reviews",
                }
            )
            write_json(course / "route-probes.json", probes)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("operation-outside-catalog", str(caught.exception))

    def test_an_unexercised_catalog_entry_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            probes = read_json(course / "route-probes.json")
            probes["probes"] = probes["probes"][:-1]
            write_json(course / "route-probes.json", probes)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("catalog-not-completely-exercised", str(caught.exception))

    def test_a_failed_probe_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            probes = read_json(course / "route-probes.json")
            probes["probes"][0]["result"] = "forbidden"
            write_json(course / "route-probes.json", probes)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("route-probes:result", str(caught.exception))

    def test_a_graphql_document_that_mutates_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            probes = read_json(course / "route-probes.json")
            document = GRAPHQL_DOCUMENT.replace("query", "mutation")
            probes["graphql_document"] = document
            probes["graphql_document_sha256"] = sha256(document.encode("utf-8"))
            write_json(course / "route-probes.json", probes)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("graphql-document-mutates", str(caught.exception))

    def test_a_graphql_document_that_does_not_match_its_digest_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            probes = read_json(course / "route-probes.json")
            probes["graphql_document"] = GRAPHQL_DOCUMENT + " "
            write_json(course / "route-probes.json", probes)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("graphql-document-digest-mismatch", str(caught.exception))

    def test_graphql_variables_naming_another_target_fail(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            probes = read_json(course / "route-probes.json")
            probes["graphql_variables"]["name"] = "Another"
            write_json(course / "route-probes.json", probes)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("graphql-variables-not-designated", str(caught.exception))

    def test_another_designated_repository_identity_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            probes = read_json(course / "route-probes.json")
            probes["repository"]["database_id"] = DESIGNATED_DATABASE_ID + 1
            write_json(course / "route-probes.json", probes)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("repository-not-exactly-designated", str(caught.exception))


class ApprovedConfigurationTests(unittest.TestCase):
    """The registration facts, mutated before the tree is digested."""

    def assert_registration_rejected(
        self,
        overrides: dict[str, Any],
        expected: str,
    ) -> None:
        with contained_temporary_directory() as directory:
            root = Path(directory)
            course = build_course(root, registration_overrides=overrides)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn(expected, str(caught.exception))

            review = write_observation(root, overrides=overrides)
            with self.assertRaises(IdentityFailure) as observed:
                check_registration_observation(review)
            self.assertIn(expected, str(observed.exception))

    def test_an_owner_only_installation_policy_fails(self) -> None:
        self.assert_registration_rejected(
            {"installation_policy": "owner_only"},
            "installation-policy-not-any-account",
        )

    def test_a_private_application_fails(self) -> None:
        self.assert_registration_rejected(
            {"application_visibility": "private"},
            "not-publicly-installable",
        )

    def test_a_widened_permission_fails(self) -> None:
        widened = dict(APPROVED_REPOSITORY_PERMISSIONS)
        widened["issues"] = "read"
        self.assert_registration_rejected(
            {
                "permissions": {
                    "account": {},
                    "enterprise": {},
                    "organization": {},
                    "repository": widened,
                }
            },
            "repository-permissions-not-exactly-approved",
        )

    def test_a_narrowed_permission_fails(self) -> None:
        narrowed = dict(APPROVED_REPOSITORY_PERMISSIONS)
        narrowed["pull_requests"] = "read"
        self.assert_registration_rejected(
            {
                "permissions": {
                    "account": {},
                    "enterprise": {},
                    "organization": {},
                    "repository": narrowed,
                }
            },
            "repository-permissions-not-exactly-approved",
        )

    def test_an_organization_permission_fails(self) -> None:
        self.assert_registration_rejected(
            {
                "permissions": {
                    "account": {},
                    "enterprise": {},
                    "organization": {"members": "read"},
                    "repository": dict(APPROVED_REPOSITORY_PERMISSIONS),
                }
            },
            "non-repository-permission:organization",
        )

    def test_a_requested_oauth_scope_fails(self) -> None:
        self.assert_registration_rejected({"oauth_scopes": ["repo"]}, "oauth-scope-requested")

    def test_an_unbounded_installation_fails(self) -> None:
        self.assert_registration_rejected(
            {"installation_repository_selection": "all"},
            "installation-not-limited-to-selected-repositories",
        )

    def test_an_extra_installed_repository_fails(self) -> None:
        self.assert_registration_rejected(
            {"installation_repositories": [DESIGNATED_REPOSITORY, "MikeMarcin/Other"]},
            "installation-repositories-not-exactly-designated",
        )

    def test_an_active_webhook_fails(self) -> None:
        self.assert_registration_rejected({"webhook_active": True}, "webhook-active")

    def test_a_callback_url_fails(self) -> None:
        self.assert_registration_rejected(
            {"callback_url_present": True},
            "callback-url-present",
        )

    def test_a_confidential_oauth_client_fails(self) -> None:
        self.assert_registration_rejected(
            {"oauth_client_type": "confidential"},
            "client-not-public",
        )

    def test_a_foreign_owner_fails(self) -> None:
        self.assert_registration_rejected({"owner_login": "Someone"}, "owner-login")

    def test_an_oauth_app_rather_than_a_github_app_fails(self) -> None:
        self.assert_registration_rejected(
            {"application_kind": "oauth_app"},
            "not-a-github-app",
        )

    def test_an_organization_owned_registration_fails(self) -> None:
        self.assert_registration_rejected({"owner_kind": "organization"}, "owner-kind")

    def test_an_installation_under_another_account_fails(self) -> None:
        self.assert_registration_rejected(
            {"installation_owner_login": "Someone"},
            "installation-owner-login",
        )

    def test_a_non_expiring_token_policy_fails(self) -> None:
        self.assert_registration_rejected(
            {"refresh_policy": "non_expiring_user_token"},
            "refresh-policy",
        )

    def test_an_extra_permission_scope_fails(self) -> None:
        self.assert_registration_rejected(
            {
                "permissions": {
                    "account": {},
                    "enterprise": {},
                    "organization": {},
                    "repository": dict(APPROVED_REPOSITORY_PERMISSIONS),
                    "marketplace": {},
                }
            },
            "permission-scopes-not-exact",
        )

    def test_a_client_identifier_of_the_wrong_shape_fails(self) -> None:
        self.assert_registration_rejected(
            {"public_client_id": "short"},
            "public-client-id-shape",
        )

    def test_a_client_identifier_digest_that_disagrees_fails(self) -> None:
        self.assert_registration_rejected(
            {"public_client_id_sha256": sha256(b"another client")},
            "public-client-id-digest-mismatch",
        )

    def test_an_integer_alias_for_device_flow_fails(self) -> None:
        """`1` is not `True`; a truthiness read would admit it."""
        self.assert_registration_rejected({"device_flow_enabled": 1}, "device-flow-off")

    def test_an_integer_alias_for_token_expiration_fails(self) -> None:
        self.assert_registration_rejected(
            {"user_access_token_expiration_enabled": 1},
            "token-expiration-off",
        )


class SchemaTests(unittest.TestCase):
    """Every evidence document declares what it is, and is refused if it lies.

    A renamed or re-versioned schema is how evidence from some other tool, or
    some later incompatible revision, would arrive looking like this one.
    """

    DOCUMENTS = (
        ("registration.json", "course-registration"),
        ("course-execution.json", "course-execution"),
        ("outcomes/token_success.json", "outcome:token_success"),
        ("route-probes.json", "route-probes"),
    )

    def test_a_renamed_schema_is_refused(self) -> None:
        for name, diagnostic in self.DOCUMENTS:
            with self.subTest(document=name):
                with contained_temporary_directory() as directory:
                    course = build_course(Path(directory))
                    path = course / name
                    document = read_json(path)
                    document["schema_name"] = "bpr.something.else"
                    write_json(path, document)
                    with self.assertRaises(IdentityFailure) as caught:
                        admit(course)
                    self.assertIn(f"{diagnostic}:schema-name", str(caught.exception))

    def test_a_later_schema_version_is_refused(self) -> None:
        for name, diagnostic in self.DOCUMENTS:
            with self.subTest(document=name):
                with contained_temporary_directory() as directory:
                    course = build_course(Path(directory))
                    path = course / name
                    document = read_json(path)
                    document["schema_version"] = 2
                    write_json(path, document)
                    with self.assertRaises(IdentityFailure) as caught:
                        admit(course)
                    self.assertIn(f"{diagnostic}:schema-version", str(caught.exception))

    def test_a_boolean_alias_for_the_schema_version_is_refused(self) -> None:
        """`True` equals `1`; only an exact int is a version."""
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            path = course / "route-probes.json"
            document = read_json(path)
            document["schema_version"] = True
            write_json(path, document)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("route-probes:schema-version", str(caught.exception))

    def test_an_observation_with_a_renamed_schema_is_refused(self) -> None:
        with contained_temporary_directory() as directory:
            review = write_observation(Path(directory))
            path = review / "registration-observation.json"
            document = read_json(path)
            document["schema_name"] = "bpr.something.else"
            write_json(path, document)
            with self.assertRaises(IdentityFailure) as caught:
                check_registration_observation(review)
            self.assertIn("registration-observation:schema-name", str(caught.exception))

    def test_a_record_with_a_renamed_schema_is_refused(self) -> None:
        with contained_temporary_directory() as directory:
            review = build_review(
                Path(directory),
                record_overrides={"schema_name": "bpr.something.else"},
            )
            with self.assertRaises(IdentityFailure) as caught:
                check_recorded_decision(review)
            self.assertIn("validation-record:schema-name", str(caught.exception))

    def test_a_contract_approving_another_claim_is_refused(self) -> None:
        with contained_temporary_directory() as directory:
            review = build_review(Path(directory))
            contract = read_json(review / "claims-contract.json")
            contract["approved_claim"] = "something_else_approved"
            payload = write_json(review / "claims-contract.json", contract)
            record = read_json(review / "validation-record.json")
            record["claims_contract"] = {
                "byte_length": len(payload),
                "path": "claims-contract.json",
                "sha256": sha256(payload),
            }
            write_json(review / "validation-record.json", record)
            with self.assertRaises(IdentityFailure) as caught:
                check_recorded_decision(review)
            self.assertIn("claims-contract:approved-claim", str(caught.exception))


class DocumentFieldTests(unittest.TestCase):
    """Per-document facts the seam checks would not notice on their own."""

    def mutate(self, name: str, mutate: Any, expected: str) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            path = course / name
            document = read_json(path)
            mutate(document)
            write_json(path, document)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn(expected, str(caught.exception))

    def test_a_course_that_is_not_token_success_fails(self) -> None:
        self.mutate(
            "course-execution.json",
            lambda document: document.update({"course": "slow-down"}),
            "course-execution:course-name",
        )

    def test_an_execution_record_with_no_timestamp_fails(self) -> None:
        self.mutate(
            "course-execution.json",
            lambda document: document.update({"executed_at_utc": "sometime"}),
            "course-execution:executed-at-missing",
        )

    def test_an_outcome_filed_under_another_name_fails(self) -> None:
        self.mutate(
            "outcomes/token_success.json",
            lambda document: document.update({"outcome": "access_denied"}),
            "outcome:token_success:outcome-name",
        )

    def test_an_unsuccessful_token_response_fails(self) -> None:
        self.mutate(
            "outcomes/token_success.json",
            lambda document: document.update({"http_status": 401}),
            "outcome:token_success:http-status",
        )

    def test_a_token_response_from_another_endpoint_fails(self) -> None:
        self.mutate(
            "outcomes/token_success.json",
            lambda document: document.update(
                {"endpoint_url": "https://example.invalid/token"}
            ),
            "outcome:token_success:endpoint-url",
        )

    def test_a_device_code_response_from_the_token_endpoint_fails(self) -> None:
        self.mutate(
            "outcomes/device_code_success.json",
            lambda document: document.update({"endpoint_url": TOKEN_URL}),
            "outcome:device_code_success:endpoint-url",
        )

    def test_a_non_bearer_token_type_fails(self) -> None:
        self.mutate(
            "outcomes/token_success.json",
            lambda document: document["projection"]["values"].update(
                {"token_type": "mac"}
            ),
            "outcome:token_success:token-type",
        )

    def test_provenance_naming_another_service_fails(self) -> None:
        self.mutate(
            "route-probes.json",
            lambda document: document["provenance"].update(
                {"service": "github.example.invalid"}
            ),
            "route-probes:provenance-service",
        )

    def test_a_probe_over_the_wrong_transport_fails(self) -> None:
        def mutate(document: dict[str, Any]) -> None:
            for probe in document["probes"]:
                if probe["operation"] == "review_threads":
                    probe["transport"] = "rest"

        self.mutate("route-probes.json", mutate, "route-probes:transport:review_threads")

    def test_a_probe_that_did_not_return_200_fails(self) -> None:
        def mutate(document: dict[str, Any]) -> None:
            document["probes"][0]["http_status"] = 304

        self.mutate("route-probes.json", mutate, "route-probes:http-status")

    def test_a_repeated_operation_fails(self) -> None:
        def mutate(document: dict[str, Any]) -> None:
            document["probes"].append(dict(document["probes"][0]))

        self.mutate("route-probes.json", mutate, "operation-repeated:metadata")

    def test_a_probe_without_a_request_contract_digest_fails(self) -> None:
        def mutate(document: dict[str, Any]) -> None:
            document["probes"][0]["request_contract_sha256"] = "not-a-digest"

        self.mutate("route-probes.json", mutate, "route-probes:request-contract-digest")


class TokenProjectionTests(unittest.TestCase):
    """A successful token response proves the grant, and records none of it."""

    def mutate_token_success(self, mutate: Any, expected: str) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            path = course / "outcomes" / "token_success.json"
            document = read_json(path)
            mutate(document)
            write_json(path, document)
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn(expected, str(caught.exception))

    def test_a_non_empty_scope_fails(self) -> None:
        def mutate(document: dict[str, Any]) -> None:
            document["projection"]["values"]["scope"] = "repo"

        self.mutate_token_success(mutate, "scope-not-empty")

    def test_a_recorded_access_token_fails(self) -> None:
        def mutate(document: dict[str, Any]) -> None:
            document["projection"]["values"]["access_token"] = "ghu_recorded"

        self.mutate_token_success(mutate, "projection-recorded-transient-material")

    def test_an_unomitted_refresh_token_fails(self) -> None:
        def mutate(document: dict[str, Any]) -> None:
            document["projection"]["omitted_transient_fields"] = ["access_token"]

        self.mutate_token_success(mutate, "transient-field-not-omitted:refresh_token")

    def test_a_missing_required_outcome_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            (course / "outcomes" / "token_success.json").unlink()
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("required-missing:token_success", str(caught.exception))

    def test_an_unexpected_outcome_file_fails(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            write_json(course / "outcomes" / "access_denied.json", {})
            with self.assertRaises(IdentityFailure) as caught:
                admit(course)
            self.assertIn("unexpected-file", str(caught.exception))


class SecretScanTests(unittest.TestCase):
    """Material that should never have been written down is found wherever it is."""

    def assert_rejected(self, name: str, payload: str) -> None:
        with contained_temporary_directory() as directory:
            path = Path(directory) / name
            path.write_text(payload, encoding="utf-8")
            with self.assertRaises(IdentityFailure) as caught:
                check_secret_free([path])
            self.assertIn("secret-bearing-evidence", str(caught.exception))

    def test_a_recorded_token_value_fails(self) -> None:
        self.assert_rejected("leak.json", '{"access_token": "value"}\n')

    def test_a_github_user_token_shape_fails(self) -> None:
        self.assert_rejected(
            "leak.json",
            '{"note": "ghu_16CharactersOrMoreOfToken"}\n',
        )

    def test_a_fine_grained_token_shape_fails(self) -> None:
        self.assert_rejected(
            "leak.md",
            "The value github_pat_11ABCDEFG0abcdefghij was pasted here.\n",
        )

    def test_a_recorded_user_code_shape_fails(self) -> None:
        self.assert_rejected("leak.json", '{"note": "WDJB-MJHT"}\n')

    def test_a_client_secret_assignment_fails(self) -> None:
        self.assert_rejected("leak.md", "client_secret = abcdef\n")

    def test_naming_an_omitted_field_is_not_a_leak(self) -> None:
        with contained_temporary_directory() as directory:
            path = Path(directory) / "outcome.json"
            path.write_text(
                '{"omitted_transient_fields": ["access_token", "refresh_token"]}\n',
                encoding="utf-8",
            )
            self.assertEqual(check_secret_free([path]), 1)


class SelectedClientTests(unittest.TestCase):
    """The shipped artifact selects the recorded identity, and only it."""

    def test_an_artifact_carrying_the_production_client_is_admitted(self) -> None:
        with contained_temporary_directory() as directory:
            binary = Path(directory) / "BetterPullReview"
            binary.write_bytes(b"\x00prefix" + PRODUCTION_CLIENT.encode("ascii"))
            check_selected_client(binary, PRODUCTION_CLIENT)

    def test_an_artifact_without_the_production_client_fails(self) -> None:
        with contained_temporary_directory() as directory:
            binary = Path(directory) / "BetterPullReview"
            binary.write_bytes(b"\x00no identity here")
            with self.assertRaises(IdentityFailure) as caught:
                check_selected_client(binary, PRODUCTION_CLIENT)
            self.assertIn("does-not-select", str(caught.exception))

    def test_an_artifact_carrying_the_verification_client_fails(self) -> None:
        with contained_temporary_directory() as directory:
            binary = Path(directory) / "BetterPullReview"
            binary.write_bytes(
                PRODUCTION_CLIENT.encode("ascii")
                + b"\x00"
                + VERIFICATION_CLIENT_ID.encode("ascii")
            )
            with self.assertRaises(IdentityFailure) as caught:
                check_selected_client(binary, PRODUCTION_CLIENT)
            self.assertIn("private-verification-identity", str(caught.exception))


class CommittedEvidenceTests(unittest.TestCase):
    """Closeout replays committed bytes, so an uncommitted edit must fail."""

    def build_repository(self, directory: str) -> tuple[Path, Path]:
        root = Path(directory)
        for command in (
            ["git", "init", "--quiet"],
            ["git", "config", "user.email", "gate@example.invalid"],
            ["git", "config", "user.name", "Gate"],
        ):
            subprocess.run(command, cwd=root, check=True, capture_output=True)
        evidence = root / "evidence" / "record.json"
        write_json(evidence, {"state": "recorded"})
        subprocess.run(["git", "add", "-A"], cwd=root, check=True, capture_output=True)
        subprocess.run(
            ["git", "commit", "--quiet", "-m", "record"],
            cwd=root,
            check=True,
            capture_output=True,
        )
        return root, evidence

    def test_committed_evidence_is_accepted(self) -> None:
        with contained_temporary_directory() as directory:
            root, evidence = self.build_repository(directory)
            check_committed(root, [evidence])

    def test_a_working_tree_edit_fails(self) -> None:
        with contained_temporary_directory() as directory:
            root, evidence = self.build_repository(directory)
            write_json(evidence, {"state": "edited"})
            with self.assertRaises(IdentityFailure) as caught:
                check_committed(root, [evidence])
            self.assertIn("evidence-not-committed", str(caught.exception))

    def test_a_directory_that_is_not_a_repository_fails(self) -> None:
        """`git status` failing leaves empty output, which would read as clean."""
        with contained_temporary_directory() as directory:
            root = Path(directory)
            evidence = root / "evidence" / "record.json"
            write_json(evidence, {"state": "recorded"})
            with self.assertRaises(IdentityFailure) as caught:
                check_committed(root, [evidence])
            self.assertIn("git-status-failed", str(caught.exception))

    def test_an_untracked_evidence_file_fails(self) -> None:
        with contained_temporary_directory() as directory:
            root, _ = self.build_repository(directory)
            extra = root / "evidence" / "extra.json"
            write_json(extra, {"state": "new"})
            with self.assertRaises(IdentityFailure) as caught:
                check_committed(root, [extra])
            self.assertIn("evidence-not-committed", str(caught.exception))


class OperatorRecordReplayTests(unittest.TestCase):
    """The record is not believed because it says `pass`; it is replayed.

    These run against a throwaway repository, because the replay's whole point
    is that it reads committed bytes through the checked-in attended-review
    validator rather than the working tree it was handed.
    """

    def build(self, directory: str, *, commit: bool, validator: bool) -> tuple[Path, Path]:
        root = Path(directory)
        for command in (
            ["git", "init", "--quiet"],
            ["git", "config", "user.email", "gate@example.invalid"],
            ["git", "config", "user.name", "Gate"],
        ):
            subprocess.run(command, cwd=root, check=True, capture_output=True)
        if validator:
            destination = root / OFFICIAL_VALIDATOR
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes((REPOSITORY_ROOT / OFFICIAL_VALIDATOR).read_bytes())
        # Always land one commit first, so that an uncommitted record fails for
        # having no commit of its own rather than for the repository having no
        # history at all — two different refusals that would otherwise blur.
        self.commit(root, "seed")
        review = build_review(root)
        if commit:
            self.commit(root, "record")
        return root, review

    def commit(self, root: Path, message: str) -> None:
        subprocess.run(["git", "add", "-A"], cwd=root, check=True, capture_output=True)
        subprocess.run(
            ["git", "commit", "--quiet", "--allow-empty", "-m", message],
            cwd=root,
            check=True,
            capture_output=True,
        )

    def test_a_record_the_validator_will_not_revalidate_fails(self) -> None:
        """The real validator runs and refuses; the record's own `pass` is not enough."""
        with contained_temporary_directory() as directory:
            root, review = self.build(directory, commit=True, validator=True)
            with self.assertRaises(IdentityFailure) as caught:
                check_operator_record(root, review)
            self.assertIn("operator-record-replay-failed", str(caught.exception))

    def test_an_absent_validator_fails_rather_than_being_skipped(self) -> None:
        with contained_temporary_directory() as directory:
            root, review = self.build(directory, commit=True, validator=False)
            with self.assertRaises(IdentityFailure) as caught:
                check_operator_record(root, review)
            self.assertIn("operator-record-replay-failed", str(caught.exception))

    def test_an_uncommitted_record_has_no_commit_identity(self) -> None:
        with contained_temporary_directory() as directory:
            root, review = self.build(directory, commit=False, validator=True)
            with self.assertRaises(IdentityFailure) as caught:
                check_operator_record(root, review)
            self.assertIn("record-commit-absent", str(caught.exception))

    def test_the_committed_production_record_replays(self) -> None:
        """The positive control, against the repository's own accepted record."""
        commit, subject = check_operator_record(REPOSITORY_ROOT, REVIEW_DIRECTORY)
        self.assertEqual(len(commit), 40)
        self.assertEqual(subject, SUBJECT_DIGEST)


class CommandContractTests(unittest.TestCase):
    """The ledger's own invocation, against the committed review directory."""

    def run_tool(self, *arguments: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(TOOL_PATH), *arguments],
            cwd=REPOSITORY_ROOT,
            capture_output=True,
            text=True,
            check=False,
        )

    def test_the_identity_alone_is_admitted(self) -> None:
        result = self.run_tool(
            "--registration-observation",
            str(REVIEW_DIRECTORY / "registration-observation.json"),
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("RFC0016_PRODUCTION_IDENTITY_OK", result.stdout)
        self.assertIn(f"RFC0016_PRODUCTION_CLIENT:{PRODUCTION_CLIENT}", result.stdout)
        self.assertIn("RFC0016_PRODUCTION_COURSE:identity-only", result.stdout)

    def test_a_complete_course_is_admitted_end_to_end(self) -> None:
        """The wiring from `main()` through the course, not just the checks."""
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            result = self.run_tool(
                "--registration-observation",
                str(REVIEW_DIRECTORY / "registration-observation.json"),
                "--course-evidence-root",
                str(course),
                "--require-complete",
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("RFC0016_PRODUCTION_IDENTITY_OK", result.stdout)
            self.assertIn("RFC0016_PRODUCTION_COURSE:complete", result.stdout)
            self.assertIn(
                f"RFC0016_PRODUCTION_COURSE_PROBES:{len(designated_route_catalog())}",
                result.stdout,
            )

    def test_closeout_refuses_evidence_outside_the_repository(self) -> None:
        with contained_temporary_directory() as directory:
            course = build_course(Path(directory))
            result = self.run_tool(
                "--registration-observation",
                str(REVIEW_DIRECTORY / "registration-observation.json"),
                "--course-evidence-root",
                str(course),
                "--require-complete",
                "--validate-existing",
            )
            self.assertEqual(result.returncode, 1)
            self.assertIn("evidence-outside-repository", result.stderr)

    def test_require_complete_fails_until_the_course_exists(self) -> None:
        result = self.run_tool(
            "--registration-observation",
            str(REVIEW_DIRECTORY / "registration-observation.json"),
            "--course-evidence-root",
            str(REVIEW_DIRECTORY / "read-only-course"),
            "--require-complete",
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("rfc0016-production-identity:fail:", result.stderr)
        self.assertNotIn("RFC0016_PRODUCTION_IDENTITY_OK", result.stdout)

    def test_require_complete_without_a_root_fails(self) -> None:
        result = self.run_tool(
            "--registration-observation",
            str(REVIEW_DIRECTORY / "registration-observation.json"),
            "--require-complete",
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn(
            "require-complete-without-course-evidence-root",
            result.stderr,
        )

    def test_the_verification_review_directory_is_not_this_gate(self) -> None:
        """Pointing the gate at the other accepted decision must not pass."""
        other = (
            REPOSITORY_ROOT
            / "docs"
            / "verification"
            / "reviews"
            / "rfc0016-github-app-registration-v001"
            / "registration-observation.json"
        )
        result = self.run_tool("--registration-observation", str(other))
        self.assertEqual(result.returncode, 1)
        self.assertNotIn("RFC0016_PRODUCTION_IDENTITY_OK", result.stdout)
        # Pinned so this cannot decay into a pass on some unrelated absence: the
        # record is read, and refused because it is the other gate's.
        self.assertIn(
            "rfc0016-production-identity:fail:validation-record:gate",
            result.stderr,
        )

    def test_the_tool_opens_no_socket(self) -> None:
        """A verifier that could reach the network could also be spoofed.

        Read from the import graph rather than the file's words, because the
        prose legitimately discusses pull requests and probe requests.
        """
        module = ast.parse(TOOL_PATH.read_text(encoding="utf-8"))
        imported: set[str] = set()
        for node in ast.walk(module):
            if isinstance(node, ast.Import):
                imported.update(alias.name for alias in node.names)
            elif isinstance(node, ast.ImportFrom) and node.module is not None:
                imported.add(node.module)
        networked = {
            name
            for name in imported
            if name.split(".")[0]
            in {"asyncio", "ftplib", "http", "requests", "socket", "ssl", "urllib"}
        }
        self.assertEqual(networked, set())


if __name__ == "__main__":
    unittest.main()
