#!/usr/bin/env python3
"""Falsify the RFC-BPR-0016 production-course runner before it touches GitHub.

The tool this exercises is the one the attended sitting runs against the real
service with a real grant, so the cases that matter are the ones where it must
refuse: a client that is not the distributable one, a route outside the closed
set, a terminal it was not authorized to observe, and a deadline it has run past.

Two of these tests are not about the runner at all. One pins BPR-0276's attended
runner to the digest its own admitted course evidence records, because that file
is immutable while that capture stands and the quickest way to discover this is
to edit it and watch the accepted fixture stop validating. The other pins this
file's authority constant to the committed decision subject, so the two cannot
drift into disagreeing about which decision authorizes the traffic.

Owned by BPR-0480.
"""

from __future__ import annotations

import hashlib
import json
import sys
import unittest
from pathlib import Path
from typing import Any


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.contained_temporary_support import (  # noqa: E402
    contained_temporary_directory,
)
from scripts.run_rfc0016_production_course_attended import (  # noqa: E402
    AUTHORITY_SUBJECT_SHA256,
    GATE,
    PRODUCTION_PUBLIC_CLIENT_ID,
    ProductionCourseError,
    attended_module,
    build_course_documents,
    publish_course,
    require_expected_authority,
    run_token_success_course,
    validate_client_id,
    verifier_module,
)


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
REVIEW_DIRECTORY = (
    REPOSITORY_ROOT
    / "docs"
    / "verification"
    / "reviews"
    / "rfc0016-production-app-registration-v001"
)
PRODUCTION_RUNNER = (
    REPOSITORY_ROOT / "scripts" / "run_rfc0016_production_course_attended.py"
)
RECORDED_PRODUCTION_COURSE = (
    REVIEW_DIRECTORY / "read-only-course" / "course-execution.json"
)
RECORDED_COURSE = (
    REPOSITORY_ROOT
    / "docs"
    / "verification"
    / "rfc0016-live-capture-v001"
    / "courses"
    / "token-success"
    / "course-execution.json"
)

LIVE = attended_module()
VERIFIER = verifier_module()
OFFLINE = LIVE._offline_module()

ACCESS_TOKEN = "ghu_" + "a" * 36
REFRESH_TOKEN = "ghr_" + "b" * 36
DEVICE_CODE = "d" * 40
USER_CODE = "WDJB-MJHT"

OBSERVATION = json.loads(
    (REVIEW_DIRECTORY / "registration-observation.json").read_bytes()
)["observation"]


def reply(payload: dict[str, Any], url: str, status: int = 200) -> Any:
    return LIVE.WireReply(
        status, "application/json", url, json.dumps(payload).encode("utf-8")
    )


class StubWire:
    """A device flow that pends once and then succeeds."""

    def __init__(self, *, terminal: str = "token_success", pends: int = 1) -> None:
        self.terminal = terminal
        self.pends = pends
        self.polls = 0
        self.urls: list[str] = []

    def post_form(self, url: str, fields: Any, *, timeout_seconds: float) -> Any:
        self.urls.append(url)
        if url == LIVE.DEVICE_URL:
            return reply(
                {
                    "device_code": DEVICE_CODE,
                    "user_code": USER_CODE,
                    "verification_uri": LIVE.VERIFY_URL,
                    "expires_in": 899,
                    "interval": 5,
                },
                url,
            )
        if url != LIVE.TOKEN_URL:
            raise AssertionError(f"unexpected url {url}")
        self.polls += 1
        if self.polls <= self.pends:
            return reply(
                {
                    "error": "authorization_pending",
                    "error_description": "pending",
                    "error_uri": "https://docs.github.com/x",
                },
                url,
                400,
            )
        if self.terminal == "token_success":
            return reply(
                {
                    "access_token": ACCESS_TOKEN,
                    "expires_in": 28800,
                    "refresh_token": REFRESH_TOKEN,
                    "refresh_token_expires_in": 15897600,
                    "scope": "",
                    "token_type": "bearer",
                },
                url,
            )
        return reply(
            {
                "error": self.terminal,
                "error_description": "terminal",
                "error_uri": "https://docs.github.com/x",
            },
            url,
            400,
        )


class Clock:
    def __init__(self) -> None:
        self.now = 1000.0

    def monotonic(self) -> float:
        return self.now

    def sleep(self, duration: float) -> None:
        self.now += duration


def run_course(wire: Any, *, client: str = PRODUCTION_PUBLIC_CLIENT_ID) -> Any:
    clock = Clock()
    return run_token_success_course(
        wire,
        client,
        LIVE,
        present=lambda code: None,
        probe=lambda token: OFFLINE.expected_route_probes(),
        sleep=clock.sleep,
        monotonic=clock.monotonic,
    )


def publish(evidence: Any, directory: str) -> tuple[Path, int]:
    documents = build_course_documents(
        evidence, OBSERVATION, LIVE, subject_digest=AUTHORITY_SUBJECT_SHA256
    )
    course = Path(directory) / "read-only-course"
    probes = publish_course(
        documents,
        course,
        VERIFIER,
        production_client=PRODUCTION_PUBLIC_CLIENT_ID,
        subject_digest=AUTHORITY_SUBJECT_SHA256,
    )
    return course, probes


class ImmutableAuthorityTests(unittest.TestCase):
    """The two pins this runner's whole design rests on."""

    def test_the_attended_runner_still_matches_its_admitted_digest(self) -> None:
        """BPR-0276's runner is evidence, not just code.

        Its admitted `course-execution.json` records `capture_runner_sha256`,
        and that is the hash of the file. Editing the file — to add a flag, to
        widen a client — silently invalidates an accepted attended capture, so
        this fails the moment anyone tries.
        """
        recorded = json.loads(RECORDED_COURSE.read_bytes())["capture_runner_sha256"]
        path = REPOSITORY_ROOT / "scripts" / "run_rfc0016_device_capture_attended.py"
        live = "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()
        self.assertEqual(
            live,
            recorded,
            "run_rfc0016_device_capture_attended.py is digest-bound into "
            "BPR-0276's admitted capture and must not be edited; add a separate "
            "tool instead",
        )

    def test_this_runner_freezes_once_a_course_is_recorded(self) -> None:
        """The symmetric guard for this runner, armed by the sitting itself.

        `verify_rfc0016_production_identity.py` requires a course to name the
        digest of this runner, so the moment a course is committed this file
        becomes evidence exactly as BPR-0276's runner already is. Nothing would
        say so: the identity suite recomputes the digest from the live file and
        therefore always agrees with itself, so an edit would leave every gate
        green and break only the ledger's manual run.

        So this case reads the *recorded* course rather than recomputing, and it
        arms itself — inert while no course exists, load-bearing the moment one
        lands, with no follow-up card to remember.
        """
        if not RECORDED_PRODUCTION_COURSE.is_file():
            # Not a silent skip: assert the reason it is absent, so a half
            # written course cannot disarm the guard it is supposed to arm.
            self.assertFalse(
                (REVIEW_DIRECTORY / "read-only-course").exists(),
                "a course directory exists without course-execution.json; the "
                "runner-immutability guard below is disarmed until it does",
            )
            return
        recorded = json.loads(RECORDED_PRODUCTION_COURSE.read_bytes())[
            "capture_runner_sha256"
        ]
        live = "sha256:" + hashlib.sha256(PRODUCTION_RUNNER.read_bytes()).hexdigest()
        self.assertEqual(
            live,
            recorded,
            "run_rfc0016_production_course_attended.py is digest-bound into the "
            "recorded designated read-only course and must not be edited; the "
            "course would stop being admissible. Add a separate tool instead.",
        )

    def test_the_authority_constant_matches_the_committed_decision(self) -> None:
        subject = json.loads(
            (REVIEW_DIRECTORY / "validation-record.json").read_bytes()
        )["decision_subject"]["sha256"]
        self.assertEqual(AUTHORITY_SUBJECT_SHA256, subject)

    def test_the_gate_matches_the_committed_claims_contract(self) -> None:
        contract = json.loads((REVIEW_DIRECTORY / "claims-contract.json").read_bytes())
        self.assertEqual(GATE, contract["gate"])

    def test_the_pinned_client_matches_the_sanitized_observation(self) -> None:
        self.assertEqual(PRODUCTION_PUBLIC_CLIENT_ID, OBSERVATION["public_client_id"])


class ClientClosureTests(unittest.TestCase):
    """Exactly one identity runs this course."""

    def test_the_production_client_is_admitted(self) -> None:
        self.assertEqual(
            validate_client_id(PRODUCTION_PUBLIC_CLIENT_ID, LIVE),
            PRODUCTION_PUBLIC_CLIENT_ID,
        )

    def test_the_verification_client_is_refused_by_name(self) -> None:
        with self.assertRaises(ProductionCourseError) as caught:
            validate_client_id(LIVE.APPROVED_PUBLIC_CLIENT_ID, LIVE)
        self.assertIn("client_is_the_verification_identity", str(caught.exception))

    def test_another_client_is_refused(self) -> None:
        with self.assertRaises(ProductionCourseError) as caught:
            validate_client_id("Iv23liZZZZZZZZZZZZZZ", LIVE)
        self.assertIn("client_id_invalid", str(caught.exception))

    def test_the_pinned_client_has_a_public_client_shape(self) -> None:
        self.assertIsNotNone(
            LIVE.PUBLIC_CLIENT_ID.fullmatch(PRODUCTION_PUBLIC_CLIENT_ID)
        )

    def test_the_expected_authority_is_required(self) -> None:
        require_expected_authority(AUTHORITY_SUBJECT_SHA256)
        with self.assertRaises(ProductionCourseError) as caught:
            require_expected_authority("sha256:" + "0" * 64)
        self.assertIn("authority_subject_unexpected", str(caught.exception))

    def test_a_wrong_client_is_refused_before_any_request(self) -> None:
        wire = StubWire()
        with self.assertRaises(ProductionCourseError):
            run_course(wire, client=LIVE.APPROVED_PUBLIC_CLIENT_ID)
        self.assertEqual(wire.urls, [])

    def test_the_old_runner_still_refuses_the_production_client(self) -> None:
        """The second identity did not widen the first."""
        with self.assertRaises(LIVE.LiveCaptureError):
            LIVE.validate_client_id(PRODUCTION_PUBLIC_CLIENT_ID)


class CourseTests(unittest.TestCase):
    """The one authorized course, and the terminals it may not accept."""

    def test_a_successful_course_reaches_the_token_and_the_probes(self) -> None:
        wire = StubWire()
        evidence = run_course(wire)
        self.assertEqual(
            sorted(evidence["outcomes"]), ["authorization_pending", "token_success"]
        )
        self.assertEqual(len(evidence["route_probes"]), 10)
        self.assertEqual(
            wire.urls,
            [LIVE.DEVICE_URL, LIVE.TOKEN_URL, LIVE.TOKEN_URL],
        )

    def test_a_denied_authorization_is_not_this_course(self) -> None:
        with self.assertRaises(ProductionCourseError) as caught:
            run_course(StubWire(terminal="access_denied"))
        self.assertIn("unexpected_terminal", str(caught.exception))

    def test_an_expired_grant_is_not_this_course(self) -> None:
        with self.assertRaises(ProductionCourseError) as caught:
            run_course(StubWire(terminal="expired_token"))
        self.assertIn("unexpected_terminal", str(caught.exception))

    def test_a_course_that_never_authorizes_stops_at_the_deadline(self) -> None:
        with self.assertRaises(ProductionCourseError) as caught:
            run_course(StubWire(pends=10_000))
        self.assertIn("course_deadline_refused", str(caught.exception))

    def test_a_slow_down_that_does_not_progress_is_refused(self) -> None:
        class BadSlowDown(StubWire):
            def post_form(self, url: str, fields: Any, *, timeout_seconds: float) -> Any:
                if url == LIVE.TOKEN_URL:
                    self.urls.append(url)
                    self.polls += 1
                    return reply(
                        {
                            "error": "slow_down",
                            "error_description": "slow down",
                            "error_uri": "https://docs.github.com/x",
                            "interval": 5,
                        },
                        url,
                        400,
                    )
                return super().post_form(url, fields, timeout_seconds=timeout_seconds)

        with self.assertRaises(ProductionCourseError) as caught:
            run_course(BadSlowDown())
        self.assertIn("slow_down_interval_progression_invalid", str(caught.exception))

    def test_a_scoped_token_is_refused(self) -> None:
        class Scoped(StubWire):
            def post_form(self, url: str, fields: Any, *, timeout_seconds: float) -> Any:
                result = super().post_form(url, fields, timeout_seconds=timeout_seconds)
                if url == LIVE.TOKEN_URL and self.polls > self.pends:
                    payload = json.loads(result.body)
                    payload["scope"] = "repo"
                    return reply(payload, url)
                return result

        with self.assertRaises(ProductionCourseError) as caught:
            run_course(Scoped())
        self.assertIn("token_response_invalid", str(caught.exception))


class HostileResponseTests(unittest.TestCase):
    """What the service may send that this course must refuse to record.

    Every case replaces exactly one field of an otherwise valid reply, because a
    validator that only rejects obvious rubbish is one that records whatever a
    compromised or changed endpoint returns in a plausible shape.
    """

    def refuse(self, mutate: Any, expected: str, *, endpoint: str = "device") -> None:
        class Hostile(StubWire):
            def post_form(inner, url: str, fields: Any, *, timeout_seconds: float) -> Any:
                result = super().post_form(url, fields, timeout_seconds=timeout_seconds)
                target = LIVE.DEVICE_URL if endpoint == "device" else LIVE.TOKEN_URL
                if url != target:
                    return result
                if endpoint == "token" and inner.polls <= inner.pends:
                    return result
                return mutate(result, url)

        with self.assertRaises(ProductionCourseError) as caught:
            run_course(Hostile())
        self.assertIn(expected, str(caught.exception))

    def test_a_non_200_device_response_is_refused(self) -> None:
        self.refuse(
            lambda result, url: LIVE.WireReply(400, result.media_type, url, result.body),
            "device_response_status_invalid",
        )

    def test_a_device_response_with_an_extra_field_is_refused(self) -> None:
        def mutate(result: Any, url: str) -> Any:
            payload = json.loads(result.body)
            payload["extra"] = "field"
            return reply(payload, url)

        self.refuse(mutate, "device_response_invalid")

    def test_a_redirected_verification_uri_is_refused(self) -> None:
        def mutate(result: Any, url: str) -> Any:
            payload = json.loads(result.body)
            payload["verification_uri"] = "https://github.example.invalid/login/device"
            return reply(payload, url)

        self.refuse(mutate, "device_response_invalid")

    def test_a_malformed_user_code_is_refused(self) -> None:
        def mutate(result: Any, url: str) -> Any:
            payload = json.loads(result.body)
            payload["user_code"] = "not a user code"
            return reply(payload, url)

        self.refuse(mutate, "user_code_invalid")

    def test_a_non_200_token_success_is_refused(self) -> None:
        self.refuse(
            lambda result, url: LIVE.WireReply(201, result.media_type, url, result.body),
            "token_response_status_invalid",
            endpoint="token",
        )

    def test_an_unknown_token_error_is_refused(self) -> None:
        def mutate(result: Any, url: str) -> Any:
            return reply(
                {
                    "error": "teapot",
                    "error_description": "no",
                    "error_uri": "https://docs.github.com/x",
                },
                url,
                400,
            )

        self.refuse(mutate, "token_error_unknown", endpoint="token")

    def test_a_token_error_with_an_unexpected_schema_is_refused(self) -> None:
        def mutate(result: Any, url: str) -> Any:
            return reply({"error": "authorization_pending"}, url, 400)

        self.refuse(mutate, "token_error_schema_invalid", endpoint="token")

    def test_a_token_error_with_a_refused_status_is_refused(self) -> None:
        def mutate(result: Any, url: str) -> Any:
            body = json.dumps(
                {
                    "error": "authorization_pending",
                    "error_description": "pending",
                    "error_uri": "https://docs.github.com/x",
                }
            ).encode("utf-8")
            return LIVE.WireReply(500, "application/json", url, body)

        self.refuse(mutate, "token_error_status_invalid", endpoint="token")

    def test_a_deadline_reached_between_polls_stops_the_course(self) -> None:
        """The clock jumps past the grant's life while a poll is in flight."""
        clock = Clock()
        wire = StubWire(pends=1)

        original = wire.post_form

        def jumping(url: str, fields: Any, *, timeout_seconds: float) -> Any:
            result = original(url, fields, timeout_seconds=timeout_seconds)
            if url == LIVE.TOKEN_URL:
                clock.now += 10_000.0
            return result

        wire.post_form = jumping  # type: ignore[method-assign]
        with self.assertRaises(ProductionCourseError) as caught:
            run_token_success_course(
                wire,
                PRODUCTION_PUBLIC_CLIENT_ID,
                LIVE,
                present=lambda code: None,
                probe=lambda token: OFFLINE.expected_route_probes(),
                sleep=clock.sleep,
                monotonic=clock.monotonic,
            )
        self.assertIn("course_deadline_refused", str(caught.exception))


class RouteClosureTests(unittest.TestCase):
    """Route closure keeps one definition, and it is the imported one."""

    def test_the_form_transport_refuses_an_unlisted_url(self) -> None:
        wire = LIVE.HttpsWire()
        with self.assertRaises(LIVE.LiveCaptureError) as caught:
            wire.post_form(
                "https://github.com/login/oauth/authorize",
                {"client_id": PRODUCTION_PUBLIC_CLIENT_ID},
                timeout_seconds=1.0,
            )
        self.assertIn("authority_refused", str(caught.exception))

    def test_the_probe_transport_refuses_an_unlisted_route(self) -> None:
        wire = LIVE.ReadOnlyProbeWire()
        forged = LIVE.ProbeRequest(
            "metadata",
            "POST",
            "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1/reviews",
            b"",
            None,
            "application/vnd.github+json",
        )
        with self.assertRaises(LIVE.LiveCaptureError) as caught:
            wire.perform(forged, ACCESS_TOKEN)
        self.assertIn("probe_route_refused", str(caught.exception))

    def test_the_designated_catalog_names_only_the_designated_target(self) -> None:
        for request in LIVE._probe_requests():
            with self.subTest(operation=request.operation):
                self.assertIn(
                    request.url.split("?")[0].rstrip("/").startswith(
                        (
                            "https://api.github.com/repos/MikeMarcin/BPR-Test",
                            "https://github.com/MikeMarcin/BPR-Test.git",
                            "https://api.github.com/graphql",
                        )
                    ),
                    (True,),
                )


class PublishedEvidenceTests(unittest.TestCase):
    """What the course writes, and what it must never write."""

    def test_published_evidence_is_admitted_by_the_verifier(self) -> None:
        evidence = run_course(StubWire())
        with contained_temporary_directory() as directory:
            course, probes = publish(evidence, directory)
            self.assertEqual(probes, 10)
            self.assertEqual(
                sorted(str(path.relative_to(course)) for path in course.rglob("*")),
                [
                    "course-execution.json",
                    "outcomes",
                    "outcomes/authorization_pending.json",
                    "outcomes/device_code_success.json",
                    "outcomes/token_success.json",
                    "registration.json",
                    "route-probes.json",
                ],
            )

    def test_no_published_byte_carries_the_grant(self) -> None:
        """The strongest form of the claim: search for the actual values used."""
        evidence = run_course(StubWire())
        with contained_temporary_directory() as directory:
            course, _ = publish(evidence, directory)
            published = b"".join(
                path.read_bytes() for path in sorted(course.rglob("*")) if path.is_file()
            )
            for secret in (ACCESS_TOKEN, REFRESH_TOKEN, DEVICE_CODE, USER_CODE):
                self.assertNotIn(secret.encode("ascii"), published)

    def test_the_pending_error_description_is_not_published(self) -> None:
        evidence = run_course(StubWire())
        with contained_temporary_directory() as directory:
            course, _ = publish(evidence, directory)
            pending = json.loads(
                (course / "outcomes" / "authorization_pending.json").read_bytes()
            )
            self.assertNotIn("error_description", pending["projection"]["values"])
            self.assertIn("error_description", pending["projection"]["observed_fields"])

    def test_an_existing_output_root_is_refused(self) -> None:
        evidence = run_course(StubWire())
        with contained_temporary_directory() as directory:
            publish(evidence, directory)
            with self.assertRaises(ProductionCourseError) as caught:
                publish(evidence, directory)
            self.assertIn("output_root_exists", str(caught.exception))

    def test_a_leftover_staging_directory_is_refused(self) -> None:
        evidence = run_course(StubWire())
        documents = build_course_documents(
            evidence, OBSERVATION, LIVE, subject_digest=AUTHORITY_SUBJECT_SHA256
        )
        with contained_temporary_directory() as directory:
            course = Path(directory) / "read-only-course"
            course.with_name(course.name + ".staging").mkdir()
            with self.assertRaises(ProductionCourseError) as caught:
                publish_course(
                    documents,
                    course,
                    VERIFIER,
                    production_client=PRODUCTION_PUBLIC_CLIENT_ID,
                    subject_digest=AUTHORITY_SUBJECT_SHA256,
                )
            self.assertIn("staging_root_exists", str(caught.exception))

    def test_evidence_the_verifier_rejects_is_not_left_behind(self) -> None:
        evidence = run_course(StubWire())
        documents = dict(
            build_course_documents(
                evidence, OBSERVATION, LIVE, subject_digest=AUTHORITY_SUBJECT_SHA256
            )
        )
        broken = json.loads(documents["route-probes.json"])
        broken["repository"]["owner"] = "Someone"
        documents["route-probes.json"] = (
            json.dumps(broken, indent=2, sort_keys=True) + "\n"
        ).encode("utf-8")
        with contained_temporary_directory() as directory:
            course = Path(directory) / "read-only-course"
            with self.assertRaises(ProductionCourseError) as caught:
                publish_course(
                    documents,
                    course,
                    VERIFIER,
                    production_client=PRODUCTION_PUBLIC_CLIENT_ID,
                    subject_digest=AUTHORITY_SUBJECT_SHA256,
                )
            self.assertIn("self_check_failed", str(caught.exception))
            self.assertFalse(course.exists())
            self.assertFalse(course.with_name(course.name + ".staging").exists())

    def test_a_course_bound_to_another_subject_is_refused_by_its_own_check(self) -> None:
        evidence = run_course(StubWire())
        documents = build_course_documents(
            evidence, OBSERVATION, LIVE, subject_digest="sha256:" + "0" * 64
        )
        with contained_temporary_directory() as directory:
            course = Path(directory) / "read-only-course"
            with self.assertRaises(ProductionCourseError) as caught:
                publish_course(
                    documents,
                    course,
                    VERIFIER,
                    production_client=PRODUCTION_PUBLIC_CLIENT_ID,
                    subject_digest=AUTHORITY_SUBJECT_SHA256,
                )
            self.assertIn("self_check_failed", str(caught.exception))


class ToolSurfaceTests(unittest.TestCase):
    """This tool mutates nothing and reaches nothing but the closed routes."""

    def test_the_runner_declares_no_mutating_probe(self) -> None:
        for request in LIVE._probe_requests():
            with self.subTest(operation=request.operation):
                self.assertIn(request.method, {"GET", "POST"})
                if request.method == "POST":
                    self.assertIn(
                        request.operation,
                        {"contents_upload_pack", "review_threads"},
                    )

    def test_the_form_transport_reaches_only_the_two_device_endpoints(self) -> None:
        self.assertEqual(
            LIVE.ALLOWED_URLS, frozenset({LIVE.DEVICE_URL, LIVE.TOKEN_URL})
        )


if __name__ == "__main__":
    unittest.main()
