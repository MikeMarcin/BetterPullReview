#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
PATH = ROOT / "scripts" / "run_rfc0016_device_capture_attended.py"
SPEC = importlib.util.spec_from_file_location("rfc0016_live", PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("could not load live runner")
LIVE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = LIVE
SPEC.loader.exec_module(LIVE)

CLIENT = LIVE.APPROVED_PUBLIC_CLIENT_ID
CAPTURED_AT = "2026-08-13T14:00:00Z"
EXECUTED_AT = "2026-08-13T14:10:00Z"
RUNNER_SHA256 = LIVE._runner_sha256()
FAILED_RUNNER_SHA256 = "sha256:" + "1" * 64
ORIGINAL_RESERVATION_SHA256 = "sha256:" + "2" * 64
RECOVERY_RESERVATION_SHA256 = "sha256:" + "3" * 64
REGISTRATION_OBSERVATION = LIVE._registration_observation(LIVE._offline_module(), CLIENT)
REGISTRATION = LIVE.RegistrationCapture(CAPTURED_AT, REGISTRATION_OBSERVATION)


def registration_document(observation=REGISTRATION_OBSERVATION) -> dict[str, object]:
    return {
        "captured_at_utc": CAPTURED_AT,
        "observation": observation,
        "schema_name": "bpr.rfc0016.registration_observation",
        "schema_version": 1,
    }
DEVICE = "device-secret-value"
USER = "ABCD-EFGH"


def payload(value: object) -> bytes:
    return json.dumps(value, separators=(",", ":")).encode()


DEVICE_BODY = payload(
    {
        "device_code": DEVICE,
        "user_code": USER,
        "verification_uri": LIVE.VERIFY_URL,
        "expires_in": 900,
        "interval": 5,
    }
)


def error_body(name: str) -> bytes:
    document = {
        "error": name,
        "error_description": "ignored",
        "error_uri": "https://docs.github.com/x",
    }
    if name == "slow_down":
        document["interval"] = 10
    return payload(document)


TOKEN_BODY = payload(
    {
        "access_token": "access-secret-value",
        "expires_in": 28800,
        "refresh_token": "refresh-secret-value",
        "refresh_token_expires_in": 15811200,
        "scope": "",
        "token_type": "bearer",
    }
)


def device_flow_transition() -> bytes:
    return LIVE._offline_module().canonical_json(
        {
            "application_id": LIVE.APPROVED_APPLICATION_ID,
            "authority_amendment_id": LIVE.AUTHORITY_AMENDMENT_ID,
            "authority_subject_sha256": LIVE._offline_module().AUTHORITY_SUBJECT_SHA256,
            "observation_method": "attended_github_settings_page",
            "owner_login": "MikeMarcin",
            "public_client_id": CLIENT,
            "schema_name": "bpr.rfc0016.device_flow_transition_observation",
            "schema_version": 1,
            "states": [
                {
                    "device_flow_enabled": True,
                    "observed_at_utc": "2026-08-13T14:09:58Z",
                    "phase": "before_enabled",
                },
                {
                    "device_flow_enabled": False,
                    "observed_at_utc": "2026-08-13T14:09:59Z",
                    "phase": "disabled_for_capture",
                },
                {
                    "device_flow_enabled": True,
                    "observed_at_utc": "2026-08-13T14:10:01Z",
                    "phase": "restored_enabled",
                },
            ],
        }
    )


def slow_down_recovery_state_records(
    runner_digest: str = RUNNER_SHA256,
) -> dict[str, bytes]:
    offline = LIVE._offline_module()
    original_authority = offline.canonical_json(
        {
            "authority_amendment_id": LIVE.AUTHORITY_AMENDMENT_ID,
            "authority_id": LIVE.SLOW_DOWN_REPLACEMENT_AUTHORITY_ID,
            "authority_subject_sha256": offline.AUTHORITY_SUBJECT_SHA256,
            "authorized_at_utc": "2026-08-13T14:00:00Z",
            "course": "slow-down",
            "previous_early_poll_consumed": True,
            "replacement_early_poll_limit": 1,
            "schema_name": "bpr.rfc0016.slow_down_replacement_authority",
            "schema_version": 1,
            "state": "replacement_authorized",
        }
    )
    original_marker = offline.canonical_json(
        {
            "authority_amendment_id": LIVE.AUTHORITY_AMENDMENT_ID,
            "authority_id": LIVE.SLOW_DOWN_REPLACEMENT_AUTHORITY_ID,
            "authority_record_sha256": LIVE._sha256(original_authority),
            "authority_subject_sha256": offline.AUTHORITY_SUBJECT_SHA256,
            "capture_runner_sha256": FAILED_RUNNER_SHA256,
            "course": "slow-down",
            "schema_name": "bpr.rfc0016.slow_down_reservation",
            "schema_version": 1,
            "spent_before_traffic_at_utc": "2026-08-13T14:01:00Z",
            "state": "spent",
        }
    )
    recovery_authority = offline.canonical_json(
        {
            "authority_amendment_id": LIVE.AUTHORITY_AMENDMENT_ID,
            "authority_id": LIVE.SLOW_DOWN_RECOVERY_AUTHORITY_ID,
            "authority_subject_sha256": offline.AUTHORITY_SUBJECT_SHA256,
            "authorized_at_utc": "2026-08-13T14:02:00Z",
            "course": "slow-down",
            "failed_attempt_authority_sha256": LIVE._sha256(original_authority),
            "failed_attempt_capture_runner_sha256": FAILED_RUNNER_SHA256,
            "failed_attempt_spent_marker_sha256": LIVE._sha256(original_marker),
            "parser_repair_capture_runner_sha256": runner_digest,
            "recovery_early_poll_limit": 1,
            "schema_name": (
                "bpr.rfc0016.slow_down_parser_repair_recovery_authority"
            ),
            "schema_version": 1,
            "state": "parser_repair_recovery_authorized",
        }
    )
    return {
        "original-authority.json": original_authority,
        "original-spent.json": original_marker,
        "recovery-authority.json": recovery_authority,
    }


def graphql_reply() -> bytes:
    return payload(
        {
            "data": {
                "repository": {
                    "id": "R_test",
                    "pullRequest": {
                        "files": {
                            "nodes": [],
                            "pageInfo": {"endCursor": None, "hasNextPage": False},
                            "totalCount": 0,
                        },
                        "id": "PR_test",
                        "reviewThreads": {
                            "nodes": [],
                            "pageInfo": {"endCursor": None, "hasNextPage": False},
                            "totalCount": 0,
                        },
                    },
                }
            }
        }
    )


class FakeWire:
    def __init__(self, replies: list[bytes | LIVE.WireReply]) -> None:
        self.replies = [
            reply
            if isinstance(reply, LIVE.WireReply)
            else LIVE.WireReply(200, "application/json", "", reply)
            for reply in replies
        ]
        self.calls: list[tuple[str, dict[str, str]]] = []
        self.timeouts: list[float] = []

    def post_form(self, url: str, fields, *, timeout_seconds: float) -> LIVE.WireReply:
        self.calls.append((url, dict(fields)))
        self.timeouts.append(timeout_seconds)
        reply = self.replies.pop(0)
        if not reply.final_url:
            return LIVE.WireReply(reply.status, reply.media_type, url, reply.body)
        return reply


class FakeClock:
    def __init__(self) -> None:
        self.now = 0.0
        self.sleeps: list[float] = []

    def sleep(self, duration: float) -> None:
        self.sleeps.append(duration)
        self.now += duration

    def monotonic(self) -> float:
        return self.now


class LiveRunnerTests(unittest.TestCase):
    def _evidence_for(self, course: str) -> object:
        reply = lambda url, body: LIVE.WireReply(200, "application/json", url, body)
        device = reply(LIVE.DEVICE_URL, DEVICE_BODY)
        pending = reply(LIVE.TOKEN_URL, error_body("authorization_pending"))
        if course == "access-denied":
            return LIVE.CourseEvidence(
                device,
                {"authorization_pending": pending, "access_denied": reply(
                    LIVE.TOKEN_URL, error_body("access_denied")
                )},
                EXECUTED_AT,
                RUNNER_SHA256,
            )
        if course == "device-flow-disabled":
            return LIVE.CourseEvidence(
                None,
                {"device_flow_disabled": reply(
                    LIVE.DEVICE_URL, error_body("device_flow_disabled")
                )},
                EXECUTED_AT,
                RUNNER_SHA256,
                device_flow_transition=device_flow_transition(),
            )
        if course == "expired-token":
            return LIVE.CourseEvidence(
                device,
                {"expired_token": reply(LIVE.TOKEN_URL, error_body("expired_token"))},
                EXECUTED_AT,
                RUNNER_SHA256,
            )
        if course == "slow-down":
            return LIVE.CourseEvidence(
                device,
                {"authorization_pending": pending, "slow_down": reply(
                    LIVE.TOKEN_URL, error_body("slow_down")
                )},
                EXECUTED_AT,
                RUNNER_SHA256,
                slow_down_original_reservation_sha256=(
                    ORIGINAL_RESERVATION_SHA256
                ),
                slow_down_recovery_reservation_sha256=(
                    RECOVERY_RESERVATION_SHA256
                ),
            )
        if course == "token-success":
            return LIVE.CourseEvidence(
                device,
                {"authorization_pending": pending, "token_success": reply(
                    LIVE.TOKEN_URL, TOKEN_BODY
                )},
                EXECUTED_AT,
                RUNNER_SHA256,
                route_probes=LIVE._offline_module().expected_route_probes(),
            )
        raise AssertionError(course)

    def test_read_only_probe_inventory_is_exact_and_secret_free(self) -> None:
        class ProbeWire:
            def __init__(self):
                self.requests = []

            def perform(self, request, access_token):
                self.assert_token = access_token
                self.requests.append(request)
                if request.operation == "contents_info_refs":
                    body = b"001e# service=git-upload-pack\n0000"
                    media = "application/x-git-upload-pack-advertisement"
                elif request.operation == "contents_upload_pack":
                    body = b"0008NAK\n"
                    media = "application/x-git-upload-pack-result"
                elif request.operation == "metadata":
                    body = payload({"id": 1328277631, "full_name": "MikeMarcin/BPR-Test"})
                    media = "application/json"
                elif request.operation == "pull_request":
                    body = payload({"number": 1})
                    media = "application/json"
                elif request.operation == "checks":
                    body = payload({"check_runs": []})
                    media = "application/json"
                elif request.operation == "review_threads":
                    body = graphql_reply()
                    media = "application/json"
                else:
                    body = b"[]"
                    media = "application/json"
                return LIVE.WireReply(200, media, request.url, body)

        wire = ProbeWire()
        secret = "access-secret-value"
        with mock.patch.object(LIVE, "ReadOnlyProbeWire", return_value=wire):
            observed = LIVE.run_read_only_probes(secret)
        requests = wire.requests
        self.assertEqual(observed, LIVE._offline_module().expected_route_probes())
        self.assertEqual(len(requests), 10)
        self.assertEqual({request.method for request in requests}, {"GET", "POST"})
        self.assertEqual(
            {request.operation for request in requests if request.method == "POST"},
            {"contents_upload_pack", "review_threads"},
        )
        self.assertEqual(
            next(request.body for request in requests if request.operation == "contents_upload_pack"),
            LIVE.GIT_UPLOAD_PACK_BODY,
        )
        upload = next(
            request for request in requests if request.operation == "contents_upload_pack"
        )
        projected = {
            probe["operation"]: probe
            for probe in LIVE._offline_module().expected_route_probes()
        }
        self.assertEqual(
            projected["contents_upload_pack"]["request_contract_sha256"],
            LIVE._offline_module()._request_digest(
                upload.method,
                upload.url,
                upload.body.decode("ascii"),
            ),
        )
        self.assertNotIn(secret, repr(observed))
        self.assertNotIn(secret, repr(requests))

    def test_read_only_probe_wire_refuses_any_mutated_route_before_open(self) -> None:
        opener = mock.Mock()
        with mock.patch.object(LIVE, "_strict_opener", return_value=opener):
            wire = LIVE.ReadOnlyProbeWire()
        admitted = LIVE._probe_requests()[0]
        mutated = LIVE.ProbeRequest(
            admitted.operation,
            "POST",
            admitted.url,
            b"{}",
            "application/json",
            admitted.accept,
        )
        with self.assertRaisesRegex(LIVE.LiveCaptureError, "route_refused"):
            wire.perform(mutated, "access-secret-value")
        opener.open.assert_not_called()

    def test_graphql_probe_refuses_empty_or_incomplete_pull_request(self) -> None:
        request = next(
            request
            for request in LIVE._probe_requests()
            if request.operation == "review_threads"
        )
        hostile = (
            {"data": {"repository": {"pullRequest": {}}}},
            {
                "data": {
                    "repository": {
                        "id": "R_test",
                        "pullRequest": {
                            "files": {
                                "nodes": [],
                                "pageInfo": {
                                    "endCursor": None,
                                    "hasNextPage": False,
                                },
                                "totalCount": 0,
                            },
                            "id": "PR_test",
                            "reviewThreads": {
                                "nodes": [],
                                "pageInfo": {},
                                "totalCount": 0,
                            },
                        },
                    }
                }
            },
        )
        for document in hostile:
            with self.subTest(document=document), self.assertRaisesRegex(
                LIVE.LiveCaptureError,
                "probe_graphql_invalid",
            ):
                LIVE._validate_probe_reply(
                    request,
                    LIVE.WireReply(
                        200,
                        "application/json",
                        request.url,
                        payload(document),
                    ),
                )

    def test_success_course_never_uses_early_poll(self) -> None:
        wire = FakeWire([DEVICE_BODY, error_body("authorization_pending"), TOKEN_BODY])
        clock = FakeClock()
        presented: list[str] = []
        evidence = LIVE.run_authorization_course(
            wire,
            CLIENT,
            "token-success",
            present=presented.append,
            sleep=clock.sleep,
            monotonic=clock.monotonic,
        )
        self.assertEqual(presented, [USER])
        self.assertEqual(clock.sleeps, [5.0, 5.0])
        self.assertIn("token_success", evidence.outcomes)
        self.assertEqual([call[0] for call in wire.calls], [LIVE.DEVICE_URL] + [LIVE.TOKEN_URL] * 2)
        self.assertEqual(wire.calls[1][1]["device_code"], DEVICE)

    def test_denial_stops_without_another_request(self) -> None:
        wire = FakeWire(
            [DEVICE_BODY, error_body("authorization_pending"), error_body("access_denied")]
        )
        clock = FakeClock()
        evidence = LIVE.run_authorization_course(
            wire,
            CLIENT,
            "access-denied",
            present=lambda _value: None,
            sleep=clock.sleep,
            monotonic=clock.monotonic,
        )
        self.assertIn("access_denied", evidence.outcomes)
        self.assertEqual(len(wire.calls), 3)

    def test_slow_down_is_isolated_to_explicit_course(self) -> None:
        wire = FakeWire(
            [DEVICE_BODY, error_body("authorization_pending"), error_body("slow_down")]
        )
        clock = FakeClock()
        evidence = LIVE.run_authorization_course(
            wire,
            CLIENT,
            "slow-down",
            present=lambda _value: self.fail("slow-down course presented a code"),
            slow_down_reservation=LIVE.SlowDownReservation(
                ORIGINAL_RESERVATION_SHA256,
                RECOVERY_RESERVATION_SHA256,
                RUNNER_SHA256,
            ),
            sleep=clock.sleep,
            monotonic=clock.monotonic,
        )
        self.assertEqual(clock.sleeps, [5.0])
        self.assertEqual(set(evidence.outcomes), {"authorization_pending", "slow_down"})
        self.assertEqual(len(wire.calls), 3)

    def test_slow_down_requires_reservation_before_first_wire_request(self) -> None:
        wire = FakeWire(
            [DEVICE_BODY, error_body("authorization_pending"), error_body("slow_down")]
        )
        with self.assertRaisesRegex(
            LIVE.LiveCaptureError,
            "slow_down_reservation_required",
        ):
            LIVE.run_authorization_course(wire, CLIENT, "slow-down")
        self.assertEqual(wire.calls, [])

    def test_recovery_reservation_is_one_shot_before_wire_construction(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            parent = Path(directory).resolve()
            parent.chmod(0o700)
            registration = parent / "registration.json"
            registration.write_bytes(payload(registration_document()))
            registration.chmod(0o600)
            records = slow_down_recovery_state_records()
            for name, record_payload in records.items():
                path = parent / name
                path.write_bytes(record_payload)
                path.chmod(0o600)
            original_spent = parent / "original-spent.json"
            recovery_spent = parent / "recovery-spent.json"
            original_spent_before = original_spent.read_bytes()
            output = parent / "unused"
            constructors = 0

            def construct_wire():
                nonlocal constructors
                constructors += 1
                return mock.sentinel.wire

            evidence = self._evidence_for("slow-down")
            with mock.patch.object(
                LIVE.sys,
                "platform",
                "darwin",
            ), mock.patch.object(
                LIVE,
                "SLOW_DOWN_REPLACEMENT_AUTHORITY_PATH",
                parent / "original-authority.json",
            ), mock.patch.object(
                LIVE,
                "SLOW_DOWN_SPENT_MARKER_PATH",
                original_spent,
            ), mock.patch.object(
                LIVE,
                "SLOW_DOWN_RECOVERY_AUTHORITY_PATH",
                parent / "recovery-authority.json",
            ), mock.patch.object(
                LIVE,
                "SLOW_DOWN_RECOVERY_SPENT_MARKER_PATH",
                recovery_spent,
            ), mock.patch.object(
                LIVE,
                "HttpsWire",
                side_effect=construct_wire,
            ), mock.patch.object(
                LIVE,
                "run_authorization_course",
                return_value=evidence,
            ) as run_course, mock.patch.object(
                LIVE,
                "persist_sanitized_course",
            ), mock.patch(
                "sys.stderr",
                io.StringIO(),
            ):
                arguments = [
                    "capture",
                    str(registration),
                    str(output),
                    "--course",
                    "slow-down",
                ]
                self.assertEqual(LIVE.main(arguments), 0)
                self.assertEqual(LIVE.main(arguments), 1)
            self.assertEqual(constructors, 1)
            self.assertEqual(run_course.call_count, 1)
            self.assertEqual(original_spent.read_bytes(), original_spent_before)
            self.assertTrue(recovery_spent.is_file())

    def test_main_refuses_slow_down_without_recovery_authority_before_wire(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            parent = Path(directory).resolve()
            parent.chmod(0o700)
            registration = parent / "registration.json"
            registration.write_bytes(payload(registration_document()))
            registration.chmod(0o600)
            records = slow_down_recovery_state_records()
            for name in ("original-authority.json", "original-spent.json"):
                path = parent / name
                path.write_bytes(records[name])
                path.chmod(0o600)
            with mock.patch.object(
                LIVE.sys,
                "platform",
                "darwin",
            ), mock.patch.object(
                LIVE,
                "SLOW_DOWN_REPLACEMENT_AUTHORITY_PATH",
                parent / "original-authority.json",
            ), mock.patch.object(
                LIVE,
                "SLOW_DOWN_SPENT_MARKER_PATH",
                parent / "original-spent.json",
            ), mock.patch.object(
                LIVE,
                "SLOW_DOWN_RECOVERY_AUTHORITY_PATH",
                parent / "absent-recovery-authority.json",
            ), mock.patch.object(
                LIVE,
                "SLOW_DOWN_RECOVERY_SPENT_MARKER_PATH",
                parent / "recovery-spent.json",
            ), mock.patch.object(
                LIVE,
                "HttpsWire",
            ) as constructor, mock.patch(
                "sys.stderr",
                io.StringIO(),
            ):
                result = LIVE.main(
                    [
                        "capture",
                        str(registration),
                        str(parent / "unused"),
                        "--course",
                        "slow-down",
                    ]
                )
        self.assertEqual(result, 1)
        constructor.assert_not_called()

    def test_original_marker_cannot_be_reused_as_recovery_marker(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            parent = Path(directory).resolve()
            parent.chmod(0o700)
            records = slow_down_recovery_state_records()
            for name, record_payload in records.items():
                path = parent / name
                path.write_bytes(record_payload)
                path.chmod(0o600)
            recovery_spent = parent / "recovery-spent.json"
            recovery_spent.write_bytes(records["original-spent.json"])
            recovery_spent.chmod(0o600)
            with mock.patch.object(
                LIVE,
                "SLOW_DOWN_REPLACEMENT_AUTHORITY_PATH",
                parent / "original-authority.json",
            ), mock.patch.object(
                LIVE,
                "SLOW_DOWN_SPENT_MARKER_PATH",
                parent / "original-spent.json",
            ), mock.patch.object(
                LIVE,
                "SLOW_DOWN_RECOVERY_AUTHORITY_PATH",
                parent / "recovery-authority.json",
            ), mock.patch.object(
                LIVE,
                "SLOW_DOWN_RECOVERY_SPENT_MARKER_PATH",
                recovery_spent,
            ), self.assertRaisesRegex(
                LIVE.LiveCaptureError,
                "slow_down_recovery_reservation_spent",
            ):
                LIVE._reserve_slow_down_recovery_once(RUNNER_SHA256)
            self.assertEqual(
                recovery_spent.read_bytes(),
                records["original-spent.json"],
            )

    def test_recovery_authority_is_exact_and_runner_bound_before_wire(self) -> None:
        for mutation in ("extra", "boolean-limit", "wrong-runner", "wrong-original"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as directory:
                parent = Path(directory).resolve()
                parent.chmod(0o700)
                registration = parent / "registration.json"
                registration.write_bytes(payload(registration_document()))
                registration.chmod(0o600)
                records = slow_down_recovery_state_records()
                recovery = json.loads(records["recovery-authority.json"])
                if mutation == "extra":
                    recovery["unexpected"] = True
                elif mutation == "boolean-limit":
                    recovery["recovery_early_poll_limit"] = True
                elif mutation == "wrong-runner":
                    recovery["parser_repair_capture_runner_sha256"] = (
                        FAILED_RUNNER_SHA256
                    )
                else:
                    recovery["failed_attempt_spent_marker_sha256"] = (
                        "sha256:" + "0" * 64
                    )
                records["recovery-authority.json"] = (
                    LIVE._offline_module().canonical_json(recovery)
                )
                for name, record_payload in records.items():
                    path = parent / name
                    path.write_bytes(record_payload)
                    path.chmod(0o600)
                with mock.patch.object(
                    LIVE.sys,
                    "platform",
                    "darwin",
                ), mock.patch.object(
                    LIVE,
                    "SLOW_DOWN_REPLACEMENT_AUTHORITY_PATH",
                    parent / "original-authority.json",
                ), mock.patch.object(
                    LIVE,
                    "SLOW_DOWN_SPENT_MARKER_PATH",
                    parent / "original-spent.json",
                ), mock.patch.object(
                    LIVE,
                    "SLOW_DOWN_RECOVERY_AUTHORITY_PATH",
                    parent / "recovery-authority.json",
                ), mock.patch.object(
                    LIVE,
                    "SLOW_DOWN_RECOVERY_SPENT_MARKER_PATH",
                    parent / "recovery-spent.json",
                ), mock.patch.object(
                    LIVE,
                    "HttpsWire",
                ) as constructor, mock.patch(
                    "sys.stderr",
                    io.StringIO(),
                ):
                    result = LIVE.main(
                        [
                            "capture",
                            str(registration),
                            str(parent / "unused"),
                            "--course",
                            "slow-down",
                        ]
                    )
                self.assertEqual(result, 1)
                constructor.assert_not_called()

    def test_slow_down_state_root_does_not_follow_home_environment(self) -> None:
        account_home = LIVE._account_home()
        with mock.patch.dict(os.environ, {"HOME": "/private/tmp/alternate-home"}):
            self.assertTrue(LIVE._SLOW_DOWN_STATE_ROOT.is_relative_to(account_home))
            self.assertEqual(
                {
                    LIVE.SLOW_DOWN_REPLACEMENT_AUTHORITY_PATH.parent,
                    LIVE.SLOW_DOWN_SPENT_MARKER_PATH.parent,
                    LIVE.SLOW_DOWN_RECOVERY_AUTHORITY_PATH.parent,
                    LIVE.SLOW_DOWN_RECOVERY_SPENT_MARKER_PATH.parent,
                },
                {LIVE._SLOW_DOWN_STATE_ROOT},
            )

    def test_expiry_course_has_one_explicit_post_expiry_probe(self) -> None:
        device = json.loads(DEVICE_BODY)
        device["expires_in"] = 6
        wire = FakeWire([payload(device), error_body("expired_token")])
        clock = FakeClock()
        evidence = LIVE.run_authorization_course(
            wire,
            CLIENT,
            "expired-token",
            present=lambda _value: self.fail("expiry course presented a code"),
            sleep=clock.sleep,
            monotonic=clock.monotonic,
        )
        self.assertEqual(
            clock.sleeps,
            [6.0 + float(LIVE.EXPIRY_CAPTURE_GRACE_SECONDS)],
        )
        self.assertEqual(set(evidence.outcomes), {"expired_token"})
        self.assertEqual(len(wire.calls), 2)

    def test_expiry_course_does_not_retry_a_service_grace_reply(self) -> None:
        device = json.loads(DEVICE_BODY)
        device["expires_in"] = 6
        wire = FakeWire([payload(device), error_body("authorization_pending")])
        clock = FakeClock()
        with self.assertRaisesRegex(LIVE.LiveCaptureError, "expired_token_not_observed"):
            LIVE.run_authorization_course(
                wire,
                CLIENT,
                "expired-token",
                present=lambda _value: self.fail("expiry course presented a code"),
                sleep=clock.sleep,
                monotonic=clock.monotonic,
            )
        self.assertEqual(len(wire.calls), 2)

    def test_deadline_is_rechecked_before_sleep_and_poll(self) -> None:
        device = json.loads(DEVICE_BODY)
        device["expires_in"] = 1
        wire = FakeWire([payload(device)])
        clock = FakeClock()
        with self.assertRaisesRegex(LIVE.LiveCaptureError, "course_deadline_refused"):
            LIVE.run_authorization_course(
                wire,
                CLIENT,
                "token-success",
                present=lambda _value: None,
                sleep=clock.sleep,
                monotonic=clock.monotonic,
            )
        self.assertEqual(len(wire.calls), 1)

    def test_blocking_request_completion_cannot_cross_deadline(self) -> None:
        device = json.loads(DEVICE_BODY)
        device["expires_in"] = 6
        clock = FakeClock()

        class SlowReplyWire(FakeWire):
            def post_form(self, url, fields, *, timeout_seconds):
                reply = super().post_form(
                    url,
                    fields,
                    timeout_seconds=timeout_seconds,
                )
                if len(self.calls) == 2:
                    clock.now += 2.0
                return reply

        wire = SlowReplyWire([payload(device), error_body("authorization_pending")])
        with self.assertRaisesRegex(LIVE.LiveCaptureError, "course_deadline_refused"):
            LIVE.run_authorization_course(
                wire,
                CLIENT,
                "token-success",
                present=lambda _value: None,
                sleep=clock.sleep,
                monotonic=clock.monotonic,
            )
        self.assertEqual(len(wire.calls), 2)

    def test_disabled_course_never_uses_token_endpoint(self) -> None:
        wire = FakeWire([error_body("device_flow_disabled")])
        evidence = LIVE.run_authorization_course(wire, CLIENT, "device-flow-disabled")
        self.assertEqual(set(evidence.outcomes), {"device_flow_disabled"})
        self.assertIsNone(evidence.device_reply)
        self.assertEqual([call[0] for call in wire.calls], [LIVE.DEVICE_URL])

    def test_device_code_is_only_in_subprocess_stdin(self) -> None:
        with mock.patch.object(LIVE.subprocess, "run") as run:
            LIVE.show_device_code(USER)
        first, second = run.call_args_list
        self.assertEqual(first.args[0], ["/usr/bin/open", LIVE.VERIFY_URL])
        self.assertNotIn(USER, repr(second.args[0]))
        self.assertIn(USER, second.kwargs["input"])
        self.assertEqual(second.args[0], ["/usr/bin/osascript", "-"])
        self.assertNotIn(USER, repr(second.kwargs["env"]))

    def test_cli_failure_output_contains_no_transient_value(self) -> None:
        stderr = io.StringIO()
        with tempfile.TemporaryDirectory() as directory:
            registration = Path(directory) / "registration.json"
            registration.write_bytes(payload(registration_document()))
            with mock.patch.object(
                LIVE.sys, "platform", "darwin"
            ), mock.patch.object(
                LIVE, "HttpsWire", return_value=mock.sentinel.wire
            ), mock.patch.object(
                LIVE, "run_authorization_course", side_effect=LIVE.LiveCaptureError(USER)
            ), mock.patch("sys.stderr", stderr):
                result = LIVE.main(
                    [
                        "capture",
                        str(registration),
                        str(Path(directory) / "course"),
                        "--course",
                        "token-success",
                    ]
                )
        self.assertEqual(result, 1)
        self.assertNotIn(USER, stderr.getvalue())
        self.assertEqual(
            stderr.getvalue(),
            "RFC0016_ATTENDED_CAPTURE_FAILED:closed_failure\n",
        )

    def test_transport_rejects_non_github_authority_before_open(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=True):
            wire = LIVE.HttpsWire()
        with mock.patch.object(wire._opener, "open") as opened:
            with self.assertRaises(LIVE.LiveCaptureError):
                wire.post_form(
                    "https://example.com/login/device/code",
                    {"client_id": CLIENT},
                    timeout_seconds=1.0,
                )
        opened.assert_not_called()

    def test_device_code_rejects_apple_script_shaped_user_code(self) -> None:
        hostile = json.loads(DEVICE_BODY)
        hostile["user_code"] = 'ABCD-\" & do shell script "id" & "'
        with self.assertRaisesRegex(LIVE.LiveCaptureError, "user_code_invalid"):
            LIVE.request_device_grant(FakeWire([payload(hostile)]), CLIENT)

    def test_response_limit_is_enforced(self) -> None:
        class Reply:
            status = 200
            headers = mock.Mock(get_content_type=mock.Mock(return_value="application/json"))
            def __enter__(self): return self
            def __exit__(self, *_args): return False
            def read(self, _limit): return b"x" * (LIVE.MAX_RESPONSE_BYTES + 1)
            def geturl(self): return LIVE.DEVICE_URL
        with mock.patch.dict(os.environ, {}, clear=True):
            wire = LIVE.HttpsWire()
        with mock.patch.object(wire._opener, "open", return_value=Reply()):
            with self.assertRaisesRegex(LIVE.LiveCaptureError, "response_too_large"):
                wire.post_form(
                    LIVE.DEVICE_URL,
                    {"client_id": CLIENT},
                    timeout_seconds=1.0,
                )

    def test_persisted_course_contains_no_transient_values(self) -> None:
        evidence = LIVE.CourseEvidence(
            LIVE.WireReply(200, "application/json", LIVE.DEVICE_URL, DEVICE_BODY),
            {
                "authorization_pending": LIVE.WireReply(
                    200, "application/json", LIVE.TOKEN_URL, error_body("authorization_pending")
                ),
                "token_success": LIVE.WireReply(
                    200, "application/json", LIVE.TOKEN_URL, TOKEN_BODY
                ),
            },
            EXECUTED_AT,
            RUNNER_SHA256,
            route_probes=LIVE._offline_module().expected_route_probes(),
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve() / "course"
            LIVE.persist_sanitized_course(root, REGISTRATION, "token-success", evidence)
            durable = b"".join(path.read_bytes() for path in root.rglob("*.json"))
            for transient in (DEVICE, USER, b"access-secret-value".decode(), b"refresh-secret-value".decode()):
                self.assertNotIn(transient.encode(), durable)
            self.assertEqual(root.stat().st_mode & 0o077, 0)
            self.assertTrue(all(path.stat().st_mode & 0o077 == 0 for path in root.rglob("*.json")))

    def test_course_artifacts_use_execution_timestamp_not_registration_timestamp(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve() / "course"
            LIVE.persist_sanitized_course(
                root,
                REGISTRATION,
                "token-success",
                self._evidence_for("token-success"),
            )
            registration = json.loads((root / "registration.json").read_bytes())
            execution = json.loads((root / "course-execution.json").read_bytes())
            outcome = json.loads(
                (root / "outcomes" / "token_success.json").read_bytes()
            )
            probes = json.loads((root / "route-probes.json").read_bytes())
        self.assertNotEqual(CAPTURED_AT, EXECUTED_AT)
        self.assertEqual(
            registration["provenance"]["captured_at_utc"],
            CAPTURED_AT,
        )
        self.assertEqual(execution["executed_at_utc"], EXECUTED_AT)
        self.assertEqual(outcome["provenance"]["captured_at_utc"], EXECUTED_AT)
        self.assertEqual(probes["provenance"]["captured_at_utc"], EXECUTED_AT)

    def test_disabled_course_requires_exact_restored_transition(self) -> None:
        evidence = self._evidence_for("device-flow-disabled")
        evidence.device_flow_transition = None
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve() / "course"
            with self.assertRaisesRegex(
                LIVE.LiveCaptureError,
                "course_evidence_invalid",
            ):
                LIVE.persist_sanitized_course(
                    root,
                    REGISTRATION,
                    "device-flow-disabled",
                    evidence,
                )
            self.assertFalse(root.exists())

        evidence = self._evidence_for("device-flow-disabled")
        transition = json.loads(evidence.device_flow_transition)
        transition["states"][2]["device_flow_enabled"] = False
        evidence.device_flow_transition = LIVE._offline_module().canonical_json(
            transition
        )
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(
                LIVE.LiveCaptureError,
                "device_flow_transition_invalid",
            ):
                LIVE.persist_sanitized_course(
                    Path(directory).resolve() / "course",
                    REGISTRATION,
                    "device-flow-disabled",
                    evidence,
                )

    def test_invalid_client_is_rejected_before_wire_construction(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            observed = dict(REGISTRATION_OBSERVATION)
            observed["public_client_id"] = "Iv1.not-approved-client"
            registration = Path(directory) / "registration.json"
            registration.write_bytes(payload(registration_document(observed)))
            with mock.patch.object(LIVE.sys, "platform", "darwin"), mock.patch.object(
                LIVE, "HttpsWire"
            ) as constructor, mock.patch("sys.stderr", io.StringIO()):
                result = LIVE.main(
                    [
                        "capture",
                        str(registration),
                        str(Path(directory) / "unused"),
                        "--course",
                        "slow-down",
                    ]
                )
        self.assertEqual(result, 1)
        constructor.assert_not_called()

    def test_duplicate_and_augmented_error_bodies_are_refused(self) -> None:
        duplicate = b'{"error":"authorization_pending","error":"slow_down",' \
            b'"error_description":"x","error_uri":"https://docs.github.com/x"}'
        with self.assertRaisesRegex(LIVE.LiveCaptureError, "duplicate"):
            LIVE.poll_token(FakeWire([duplicate]), CLIENT, DEVICE)
        augmented = json.loads(error_body("authorization_pending"))
        augmented["unexpected"] = True
        with self.assertRaisesRegex(LIVE.LiveCaptureError, "schema"):
            LIVE.poll_token(FakeWire([payload(augmented)]), CLIENT, DEVICE)

    def test_poll_token_accepts_documented_slow_down_interval(self) -> None:
        poll = LIVE.poll_token(
            FakeWire([error_body("slow_down")]),
            CLIENT,
            DEVICE,
        )
        self.assertEqual(poll.outcome, "slow_down")
        self.assertEqual(poll.interval, 10)
        self.assertEqual(poll.reply.status, 200)

    def test_poll_token_rejects_missing_and_invalid_slow_down_intervals(self) -> None:
        invalid = (None, True, 1.5, "10", 0, -1, 301)
        for interval in invalid:
            document = json.loads(error_body("slow_down"))
            if interval is None:
                document.pop("interval")
                diagnostic = "schema"
            else:
                document["interval"] = interval
                diagnostic = "slow_down_interval_invalid"
            with self.subTest(interval=interval), self.assertRaisesRegex(
                LIVE.LiveCaptureError,
                diagnostic,
            ):
                LIVE.poll_token(FakeWire([payload(document)]), CLIENT, DEVICE)

    def test_poll_token_rejects_interval_on_every_generic_error(self) -> None:
        for outcome in ("authorization_pending", "access_denied", "expired_token"):
            document = json.loads(error_body(outcome))
            document["interval"] = 10
            with self.subTest(outcome=outcome), self.assertRaisesRegex(
                LIVE.LiveCaptureError,
                "schema",
            ):
                LIVE.poll_token(FakeWire([payload(document)]), CLIENT, DEVICE)

    def test_slow_down_course_requires_exact_plus_five_progression(self) -> None:
        slow_down = json.loads(error_body("slow_down"))
        slow_down["interval"] = 11
        wire = FakeWire(
            [
                DEVICE_BODY,
                error_body("authorization_pending"),
                payload(slow_down),
            ]
        )
        with self.assertRaisesRegex(
            LIVE.LiveCaptureError,
            "slow_down_interval_progression_invalid",
        ):
            LIVE.run_authorization_course(
                wire,
                CLIENT,
                "slow-down",
                slow_down_reservation=LIVE.SlowDownReservation(
                    ORIGINAL_RESERVATION_SHA256,
                    RECOVERY_RESERVATION_SHA256,
                    RUNNER_SHA256,
                ),
                sleep=FakeClock().sleep,
                monotonic=lambda: 0.0,
            )

    def test_token_error_accepts_actual_http_400_but_success_does_not(self) -> None:
        poll = LIVE.poll_token(
            FakeWire([
                LIVE.WireReply(
                    400,
                    "application/json",
                    LIVE.TOKEN_URL,
                    error_body("authorization_pending"),
                )
            ]),
            CLIENT,
            DEVICE,
        )
        self.assertEqual(poll.outcome, "authorization_pending")
        self.assertIsNone(poll.interval)
        self.assertEqual(poll.reply.status, 400)
        with self.assertRaisesRegex(LIVE.LiveCaptureError, "status_invalid"):
            LIVE.poll_token(
                FakeWire([
                    LIVE.WireReply(400, "application/json", LIVE.TOKEN_URL, TOKEN_BODY)
                ]),
                CLIENT,
                DEVICE,
            )

    def test_ambient_proxy_and_ca_configuration_is_refused(self) -> None:
        for name in LIVE.AMBIENT_NETWORK_ENVIRONMENT:
            with self.subTest(name=name), mock.patch.dict(os.environ, {name: "hostile"}, clear=True):
                with self.assertRaisesRegex(LIVE.LiveCaptureError, "ambient"):
                    LIVE.HttpsWire()

    def test_response_metadata_is_not_synthesized(self) -> None:
        class Reply:
            status = 201
            headers = mock.Mock(get_content_type=mock.Mock(return_value="text/plain"))
            def __enter__(self): return self
            def __exit__(self, *_args): return False
            def read(self, _limit): return DEVICE_BODY
            def geturl(self): return LIVE.DEVICE_URL
        with mock.patch.dict(os.environ, {}, clear=True):
            wire = LIVE.HttpsWire()
        with mock.patch.object(wire._opener, "open", return_value=Reply()):
            with self.assertRaisesRegex(LIVE.LiveCaptureError, "response_status"):
                wire.post_form(
                    LIVE.DEVICE_URL,
                    {"client_id": CLIENT},
                    timeout_seconds=1.0,
                )

    def test_write_failure_never_publishes_partial_final_tree(self) -> None:
        evidence = LIVE.CourseEvidence(
            LIVE.WireReply(200, "application/json", LIVE.DEVICE_URL, DEVICE_BODY),
            {
                "authorization_pending": LIVE.WireReply(
                    200, "application/json", LIVE.TOKEN_URL, error_body("authorization_pending")
                ),
                "slow_down": LIVE.WireReply(
                    200, "application/json", LIVE.TOKEN_URL, error_body("slow_down")
                ),
            },
            EXECUTED_AT,
            RUNNER_SHA256,
            slow_down_original_reservation_sha256=ORIGINAL_RESERVATION_SHA256,
            slow_down_recovery_reservation_sha256=RECOVERY_RESERVATION_SHA256,
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve() / "course"
            original = LIVE._write_private
            calls = 0
            def fail_second(directory_fd, name, data):
                nonlocal calls
                calls += 1
                if calls == 2:
                    raise OSError("injected")
                original(directory_fd, name, data)
            with mock.patch.object(LIVE, "_write_private", side_effect=fail_second):
                with self.assertRaises(OSError):
                    LIVE.persist_sanitized_course(root, REGISTRATION, "slow-down", evidence)
            self.assertFalse(root.exists())

    def test_symlinked_output_parent_is_refused(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            private = Path(directory).resolve()
            target = private / "target"
            target.mkdir(mode=0o700)
            alias = private / "alias"
            alias.symlink_to(target, target_is_directory=True)
            with self.assertRaisesRegex(LIVE.LiveCaptureError, "link_refused"):
                LIVE._publish_projection_tree(alias / "course", {"registration.json": b"{}"})

    def test_five_courses_assemble_atomically_into_exact_unattested_bundle(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            parent = Path(directory).resolve()
            roots = {}
            for course in sorted(LIVE.COURSES):
                root = parent / course
                LIVE.persist_sanitized_course(root, REGISTRATION, course, self._evidence_for(course))
                roots[course] = root
            bundle = parent / "bundle"
            LIVE.assemble_capture_bundle(bundle, roots)
            fixture = bundle / "fixture"
            self.assertEqual(
                LIVE._offline_module().validate_fixture_root(fixture),
                "unattested_capture",
            )
            attestation = json.loads((bundle / "live-run-attestation.json").read_bytes())
            self.assertEqual(attestation["state"], "attended_runner_completed_unattested")
            self.assertEqual(set(attestation["course_tree_sha256"]), set(LIVE.COURSES))
            self.assertEqual(
                attestation["authority_amendment_id"],
                LIVE.AUTHORITY_AMENDMENT_ID,
            )
            self.assertEqual(attestation["capture_runner_sha256"], RUNNER_SHA256)
            self.assertEqual(set(attestation["course_execution"]), set(LIVE.COURSES))
            durable = b"".join(path.read_bytes() for path in bundle.rglob("*.json"))
            for transient in (DEVICE, USER, "access-secret-value", "refresh-secret-value"):
                self.assertNotIn(transient.encode(), durable)

    def test_assembly_uses_execution_bound_runner_not_current_runner_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            parent = Path(directory).resolve()
            roots = {}
            for course in sorted(LIVE.COURSES):
                root = parent / course
                LIVE.persist_sanitized_course(
                    root,
                    REGISTRATION,
                    course,
                    self._evidence_for(course),
                )
                roots[course] = root
            assemble_time_digest = "sha256:" + "9" * 64
            with mock.patch.object(
                LIVE,
                "_runner_sha256",
                return_value=assemble_time_digest,
            ):
                bundle = parent / "bundle"
                LIVE.assemble_capture_bundle(bundle, roots)
            attestation = json.loads(
                (bundle / "live-run-attestation.json").read_bytes()
            )
        self.assertEqual(attestation["capture_runner_sha256"], RUNNER_SHA256)
        self.assertNotEqual(attestation["capture_runner_sha256"], assemble_time_digest)

    def test_course_binds_loaded_runner_when_source_path_changes_after_import(self) -> None:
        device = json.loads(DEVICE_BODY)
        device["expires_in"] = 6
        wire = FakeWire([payload(device), error_body("expired_token")])
        clock = FakeClock()
        with mock.patch.object(
            LIVE,
            "_runner_sha256",
            return_value="sha256:" + "9" * 64,
        ):
            evidence = LIVE.run_authorization_course(
                wire,
                CLIENT,
                "expired-token",
                sleep=clock.sleep,
                monotonic=clock.monotonic,
            )
        self.assertEqual(
            evidence.capture_runner_sha256,
            LIVE.LOADED_RUNNER_SHA256,
        )

    def test_assembly_refuses_mismatched_registration_and_publishes_nothing(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            parent = Path(directory).resolve()
            roots = {}
            for course in sorted(LIVE.COURSES):
                root = parent / course
                LIVE.persist_sanitized_course(root, REGISTRATION, course, self._evidence_for(course))
                roots[course] = root
            registration = roots["expired-token"] / "registration.json"
            document = json.loads(registration.read_bytes())
            document["provenance"]["captured_at_utc"] = "2026-08-13T14:00:01Z"
            registration.write_bytes(LIVE._offline_module().canonical_json(document))
            with self.assertRaisesRegex(LIVE.LiveCaptureError, "registration_mismatch"):
                LIVE.assemble_capture_bundle(parent / "bundle", roots)
            self.assertFalse((parent / "bundle").exists())


if __name__ == "__main__":
    unittest.main()
