#!/usr/bin/env python3
"""Run RFC-BPR-0016's designated read-only course as the distributable client.

BPR-0318 registered a second, distributable GitHub App and committed
`verify_rfc0016_production_identity.py`, which defines exactly what the course
that proves it must leave behind. Nothing could produce that evidence, so this
is the tool the attended sitting runs.

It is a separate file rather than a flag on BPR-0276's attended runner for a
reason that is checkable rather than stylistic: that runner's own bytes are
digest-bound into its admitted capture — `course-execution.json` records
`capture_runner_sha256`, and it is the hash of that file — so editing it
invalidates evidence the owner has already accepted. The same is true of the
offline module's provenance, which hardcodes BPR-0276's authority digest and
emits no gate at all. Those two files are immutable while that capture stands.

What is shared is everything that can be: the strict HTTPS transport, the
route-closed probe wire, the designated probe catalog, and the probe reply
validation are imported from that runner and run here unchanged, so route
closure keeps exactly one definition. What is reimplemented is only the thin
protocol layer whose client pin cannot be widened in place — the device grant,
the token poll, and the token-success loop — each carrying this gate's own
client and refusing BPR-0276's.

The course writes nothing it has not first proved. Evidence is built in memory,
published to a staging directory, and then admitted by
`verify_rfc0016_production_identity.py` itself before anything is renamed into
place; a run whose output that verifier would reject leaves no output at all.

Owned by BPR-0480. The sitting it serves belongs to BPR-0318, under the already
accepted `rfc0016-production-app-registration-v001` authority, which authorizes
this one course against `MikeMarcin/BPR-Test` and no mutation anywhere.
"""

from __future__ import annotations

import argparse
import contextlib
import importlib.util
import json
import os
import pathlib
import shutil
import sys
import time
import types
from collections.abc import Callable, Mapping


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent

GATE = "rfc0016-production-app-registration-v001"

# The distributable identity BPR-0318 recorded. Pinned here, and cross-checked
# against the sanitized observation, so this tool authorizes exactly one client.
PRODUCTION_PUBLIC_CLIENT_ID = "Iv23liitiYN1WSSbag50"

# The decision subject the owner approved. The accepted record is read at run
# time and must agree with this; a swapped record cannot redirect the course.
AUTHORITY_SUBJECT_SHA256 = (
    "sha256:18b867d3a05321cedcd4570227c732459779bebff23728d354de9411c4e5370b"
)

COURSE = "token-success"
CAPTURE_TOOL = "scripts/run_rfc0016_production_course_attended.py"

# Material a projection records the *absence* of, never the value of.
TRANSIENT_FIELDS = frozenset(
    {
        "access_token",
        "device_code",
        "error_description",
        "error_uri",
        "refresh_token",
        "user_code",
    }
)


class ProductionCourseError(RuntimeError):
    """A fixed, non-sensitive production-course failure."""


def _load(name: str, filename: str) -> types.ModuleType:
    path = pathlib.Path(__file__).with_name(filename)
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise ProductionCourseError(f"{name}_unavailable")
    if name in sys.modules:
        return sys.modules[name]
    module = importlib.util.module_from_spec(spec)
    # Registered before execution because `@dataclass` resolves a class's own
    # module out of `sys.modules`, and a module loaded by path is not there yet.
    sys.modules[name] = module
    try:
        spec.loader.exec_module(module)
    except Exception:
        del sys.modules[name]
        raise
    return module


def attended_module() -> types.ModuleType:
    """BPR-0276's runner, imported for its transport and probe catalog only."""
    return _load("rfc0016_attended_runner", "run_rfc0016_device_capture_attended.py")


def verifier_module() -> types.ModuleType:
    return _load("rfc0016_production_identity", "verify_rfc0016_production_identity.py")


@contextlib.contextmanager
def translated(live: types.ModuleType):
    """Surface the imported runner's refusals as this tool's own.

    The transport and validation helpers come from BPR-0276's runner and raise
    its `LiveCaptureError`. Letting that reach `main()` would report a real,
    named refusal - a deadline, a closed route - as an internal error, so the
    boundary is translated rather than widened.
    """
    try:
        yield
    except ProductionCourseError:
        raise
    except live.LiveCaptureError as error:
        raise ProductionCourseError(str(error)) from None


def validate_client_id(client_id: str, live: types.ModuleType) -> str:
    """Admit the distributable client, and refuse BPR-0276's by name.

    The verification identity is checked first and reported distinctly. Both
    branches refuse, so the order changes no outcome; it changes what a run says
    when someone points this tool at the wrong identity, which is the case worth
    naming. The value is read out of BPR-0276's runner rather than copied, so
    the two pins cannot drift into overlapping.
    """
    if client_id == live.APPROVED_PUBLIC_CLIENT_ID:
        raise ProductionCourseError("client_is_the_verification_identity")
    if client_id != PRODUCTION_PUBLIC_CLIENT_ID:
        raise ProductionCourseError("client_id_invalid")
    return client_id


def require_expected_authority(subject_digest: str) -> None:
    """The accepted record must be the decision this tool was built against."""
    if subject_digest != AUTHORITY_SUBJECT_SHA256:
        raise ProductionCourseError("authority_subject_unexpected")


def request_device_grant(
    wire: object,
    client_id: str,
    live: types.ModuleType,
    *,
    timeout_seconds: float | None = None,
) -> object:
    """One device grant for the production client, validated as BPR-0276 does."""
    validate_client_id(client_id, live)
    with translated(live):
        return _device_grant(wire, client_id, live, timeout_seconds)


def _device_grant(
    wire: object,
    client_id: str,
    live: types.ModuleType,
    timeout_seconds: float | None,
) -> object:
    reply = wire.post_form(
        live.DEVICE_URL,
        {"client_id": client_id},
        timeout_seconds=(
            live.REQUEST_TIMEOUT_SECONDS if timeout_seconds is None else timeout_seconds
        ),
    )
    if reply.status != 200:
        raise ProductionCourseError("device_response_status_invalid")
    value = live._closed_json(reply.body)
    expected = {"device_code", "user_code", "verification_uri", "expires_in", "interval"}
    if set(value) != expected or value.get("verification_uri") != live.VERIFY_URL:
        raise ProductionCourseError("device_response_invalid")
    user_code = live._required_text(value["user_code"], "user_code", 64)
    if not live.USER_CODE.fullmatch(user_code):
        raise ProductionCourseError("user_code_invalid")
    return live.DeviceGrant(
        device_code=live._required_text(value["device_code"], "device_code", 512),
        user_code=user_code,
        expires_in=live._required_int(value["expires_in"], "expires_in", 3600),
        interval=live._required_int(value["interval"], "interval", 300),
        reply=reply,
    )


def poll_token(
    wire: object,
    client_id: str,
    device_code: str,
    live: types.ModuleType,
    *,
    timeout_seconds: float | None = None,
) -> object:
    """One token poll. Only the empty-scope success the approved permissions imply."""
    validate_client_id(client_id, live)
    with translated(live):
        return _token_poll(wire, client_id, device_code, live, timeout_seconds)


def _token_poll(
    wire: object,
    client_id: str,
    device_code: str,
    live: types.ModuleType,
    timeout_seconds: float | None,
) -> object:
    reply = wire.post_form(
        live.TOKEN_URL,
        {
            "client_id": client_id,
            "device_code": device_code,
            "grant_type": live.DEVICE_GRANT,
        },
        timeout_seconds=(
            live.REQUEST_TIMEOUT_SECONDS if timeout_seconds is None else timeout_seconds
        ),
    )
    value = live._closed_json(reply.body)
    if "error" in value:
        if reply.status not in {200, 400}:
            raise ProductionCourseError("token_error_status_invalid")
        error = live._required_text(value["error"], "error", 64)
        if error not in {"authorization_pending", "slow_down", *live.TERMINALS}:
            raise ProductionCourseError("token_error_unknown")
        expected_fields = (
            live.SLOW_DOWN_FIELDS if error == "slow_down" else live.ERROR_FIELDS
        )
        if set(value) != expected_fields:
            raise ProductionCourseError("token_error_schema_invalid")
        live._required_text(value["error_description"], "error_description", 512)
        live._required_text(value["error_uri"], "error_uri", 512)
        interval = (
            live._required_int(value["interval"], "slow_down_interval", 300)
            if error == "slow_down"
            else None
        )
        return live.TokenPoll(error, reply, interval)
    expected = {
        "access_token",
        "expires_in",
        "refresh_token",
        "refresh_token_expires_in",
        "scope",
        "token_type",
    }
    if reply.status != 200:
        raise ProductionCourseError("token_response_status_invalid")
    if (
        set(value) != expected
        or value.get("scope") != ""
        or value.get("token_type") != "bearer"
    ):
        raise ProductionCourseError("token_response_invalid")
    live._required_text(value["access_token"], "access_token", 4096)
    live._required_text(value["refresh_token"], "refresh_token", 4096)
    return live.TokenPoll("token_success", reply)


def run_token_success_course(
    wire: object,
    client_id: str,
    live: types.ModuleType,
    *,
    present: Callable[[str], None],
    probe: Callable[[str], list[dict[str, object]]],
    sleep: Callable[[float], None] = time.sleep,
    monotonic: Callable[[], float] = time.monotonic,
) -> dict[str, object]:
    """Grant, authorize, poll to success, then probe. Bounded throughout."""
    validate_client_id(client_id, live)
    with translated(live):
        return _token_success_course(
            wire, client_id, live, present=present, probe=probe,
            sleep=sleep, monotonic=monotonic,
        )


def _token_success_course(
    wire: object,
    client_id: str,
    live: types.ModuleType,
    *,
    present: Callable[[str], None],
    probe: Callable[[str], list[dict[str, object]]],
    sleep: Callable[[float], None],
    monotonic: Callable[[], float],
) -> dict[str, object]:
    executed_at_utc = live._execution_timestamp()
    live._timestamp(executed_at_utc, "course_timestamp_invalid")

    started = monotonic()
    total_deadline = started + float(live.TOTAL_COURSE_SECONDS)

    def remaining() -> float:
        left = total_deadline - monotonic()
        if left <= 0.0:
            raise ProductionCourseError("course_deadline_refused")
        return min(live.REQUEST_TIMEOUT_SECONDS, left)

    grant = request_device_grant(wire, client_id, live, timeout_seconds=remaining())
    grant_started = monotonic()
    deadline = min(grant_started + float(grant.expires_in), total_deadline)

    present(grant.user_code)
    if monotonic() > deadline:
        raise ProductionCourseError("course_deadline_refused")

    outcomes: dict[str, object] = {}
    interval = grant.interval
    while True:
        live._bounded_sleep(
            float(interval), deadline, sleep=sleep, monotonic=monotonic
        )
        left = deadline - monotonic()
        if left <= 0.0:
            raise ProductionCourseError("course_deadline_refused")
        poll = poll_token(
            wire,
            client_id,
            grant.device_code,
            live,
            timeout_seconds=min(live.REQUEST_TIMEOUT_SECONDS, left),
        )
        if monotonic() > deadline:
            raise ProductionCourseError("course_deadline_refused")
        if poll.outcome == "authorization_pending":
            outcomes.setdefault(poll.outcome, poll.reply)
            continue
        if poll.outcome == "slow_down":
            if poll.interval != interval + 5:
                raise ProductionCourseError("slow_down_interval_progression_invalid")
            interval = poll.interval
            continue
        if poll.outcome != "token_success":
            # Denial and expiry are BPR-0276's recorded contracts. This course is
            # authorized to observe success, so anything else stops it.
            raise ProductionCourseError("unexpected_terminal")
        outcomes[poll.outcome] = poll.reply
        probes = probe(live._access_token(poll.reply))
        return {
            "device_reply": grant.reply,
            "outcomes": outcomes,
            "executed_at_utc": executed_at_utc,
            "route_probes": probes,
        }


def canonical_bytes(document: Mapping[str, object]) -> bytes:
    return (json.dumps(document, indent=2, sort_keys=True) + "\n").encode("utf-8")


def projection(body: bytes, live: types.ModuleType) -> dict[str, object]:
    """Record which transient fields were present, and none of their values."""
    value = live._closed_json(body)
    return {
        "observed_fields": sorted(value),
        "omitted_transient_fields": sorted(
            name for name in value if name in TRANSIENT_FIELDS
        ),
        "values": {
            name: item for name, item in sorted(value.items())
            if name not in TRANSIENT_FIELDS
        },
    }


def provenance(
    observation: str,
    *,
    captured_at_utc: str,
    registration_digest: str,
    subject_digest: str,
) -> dict[str, object]:
    return {
        "authority_subject_sha256": subject_digest,
        "capture_tool": CAPTURE_TOOL,
        "captured_at_utc": captured_at_utc,
        "gate": GATE,
        "observation": observation,
        "registration_record_sha256": registration_digest,
        "service": "github.com",
        "source_kind": "unattested_capture",
    }


def registration_document(
    observation: Mapping[str, object],
    live: types.ModuleType,
) -> dict[str, object]:
    document = dict(observation)
    document["schema_name"] = "bpr.rfc0016.github_app_registration"
    document["schema_version"] = 1
    client = str(document["public_client_id"])
    document["public_client_id_sha256"] = live._sha256(client.encode("utf-8"))
    return document


def outcome_document(
    outcome: str,
    reply: object,
    live: types.ModuleType,
    **binding: str,
) -> dict[str, object]:
    endpoint = "device_code" if outcome == "device_code_success" else "token"
    return {
        "content_type": "application/json",
        "endpoint": endpoint,
        "endpoint_url": live.DEVICE_URL if endpoint == "device_code" else live.TOKEN_URL,
        "http_status": reply.status,
        "outcome": outcome,
        "projection": projection(reply.body, live),
        "provenance": provenance(f"{outcome}_response", **binding),
        "request_content_type": "application/x-www-form-urlencoded",
        "request_method": "POST",
        "schema_name": "bpr.rfc0016.device_contract",
        "schema_version": 1,
    }


def build_course_documents(
    evidence: Mapping[str, object],
    observation: Mapping[str, object],
    live: types.ModuleType,
    *,
    subject_digest: str,
) -> dict[str, bytes]:
    """Every file the course publishes, digest-joined, before anything is written."""
    registration = canonical_bytes(registration_document(observation, live))
    registration_digest = live._sha256(registration)
    captured_at_utc = str(evidence["executed_at_utc"])
    binding = {
        "captured_at_utc": captured_at_utc,
        "registration_digest": registration_digest,
        "subject_digest": subject_digest,
    }

    documents: dict[str, bytes] = {"registration.json": registration}
    documents["course-execution.json"] = canonical_bytes(
        {
            "authority_subject_sha256": subject_digest,
            "capture_runner_sha256": live._sha256(
                pathlib.Path(__file__).read_bytes()
            ),
            "course": COURSE,
            "executed_at_utc": captured_at_utc,
            "gate": GATE,
            "registration_record_sha256": registration_digest,
            "schema_name": "bpr.rfc0016.course_execution",
            "schema_version": 1,
        }
    )

    outcomes = dict(evidence["outcomes"])  # type: ignore[arg-type]
    documents["outcomes/device_code_success.json"] = canonical_bytes(
        outcome_document(
            "device_code_success", evidence["device_reply"], live, **binding
        )
    )
    for outcome in ("authorization_pending", "token_success"):
        if outcome in outcomes:
            documents[f"outcomes/{outcome}.json"] = canonical_bytes(
                outcome_document(outcome, outcomes[outcome], live, **binding)
            )

    offline = live._offline_module()
    documents["route-probes.json"] = canonical_bytes(
        {
            "graphql_document": offline.GRAPHQL_DOCUMENT,
            "graphql_document_sha256": live._sha256(
                offline.GRAPHQL_DOCUMENT.encode("utf-8")
            ),
            "graphql_variables": dict(offline.GRAPHQL_VARIABLES),
            "probes": list(evidence["route_probes"]),  # type: ignore[arg-type]
            "provenance": provenance("read_only_route_probes", **binding),
            "repository": {
                "database_id": 1328277631,
                "name": "BPR-Test",
                "owner": "MikeMarcin",
                "pull_request_number": 1,
            },
            "schema_name": "bpr.rfc0016.read_only_route_probes",
            "schema_version": 1,
        }
    )
    return documents


def publish_course(
    documents: Mapping[str, bytes],
    output_root: pathlib.Path,
    verifier: types.ModuleType,
    *,
    production_client: str,
    subject_digest: str,
) -> int:
    """Stage, admit, then rename. A rejected course leaves nothing behind."""
    if output_root.exists():
        raise ProductionCourseError("output_root_exists")
    staging = output_root.with_name(output_root.name + ".staging")
    if staging.exists():
        raise ProductionCourseError("staging_root_exists")

    staging.mkdir(parents=True)
    os.chmod(staging, 0o700)
    for name, payload in documents.items():
        path = staging / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(payload)
        os.chmod(path, 0o600)

    try:
        probes = verifier.check_course_evidence(
            staging,
            production_client=production_client,
            subject_digest=subject_digest,
        )
        verifier.check_secret_free(verifier.evidence_files(staging))
    except verifier.IdentityFailure as error:
        # Unadmitted evidence is not kept: it would be a directory that looks
        # like a course and is not one, and it would block the retry.
        shutil.rmtree(staging, ignore_errors=True)
        raise ProductionCourseError(f"self_check_failed:{error}") from None

    for path in staging.rglob("*"):
        os.chmod(path, 0o755 if path.is_dir() else 0o644)
    os.chmod(staging, 0o755)
    staging.rename(output_root)
    return probes


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run the RFC-BPR-0016 designated read-only course as the recorded "
            "distributable client and publish evidence the identity verifier "
            "admits. Performs one device grant, one authorization, and the "
            "designated read-only probes; mutates nothing."
        )
    )
    parser.add_argument(
        "--registration-observation",
        type=pathlib.Path,
        required=True,
        help="Sanitized production registration observation JSON.",
    )
    parser.add_argument(
        "--output-root",
        type=pathlib.Path,
        required=True,
        help="Course evidence directory to create. Must not already exist.",
    )
    parser.add_argument(
        "--repository-root",
        type=pathlib.Path,
        default=REPOSITORY_ROOT,
        help="Repository root the accepted authority is replayed against.",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        live = attended_module()
        verifier = verifier_module()
        root = arguments.repository_root.resolve()
        observation_path = arguments.registration_observation.resolve()
        review_directory = observation_path.parent

        # Real traffic runs only under an authority that is committed and still
        # replays, not one this process was merely handed.
        _, subject_digest = verifier.check_operator_record(root, review_directory)
        require_expected_authority(subject_digest)

        production_client = verifier.check_registration_observation(review_directory)
        validate_client_id(production_client, live)
        observation = json.loads(observation_path.read_bytes())["observation"]

        evidence = run_token_success_course(
            live.HttpsWire(),
            production_client,
            live,
            present=live.show_device_code,
            probe=live.run_read_only_probes,
        )
        documents = build_course_documents(
            evidence, observation, live, subject_digest=subject_digest
        )
        probes = publish_course(
            documents,
            arguments.output_root,
            verifier,
            production_client=production_client,
            subject_digest=subject_digest,
        )
    except ProductionCourseError as error:
        print(f"rfc0016-production-course:fail:{error}", file=sys.stderr)
        return 1
    except Exception as error:  # noqa: BLE001 - reported, never swallowed
        print(
            f"rfc0016-production-course:fail:internal:{type(error).__name__}",
            file=sys.stderr,
        )
        return 1

    print(f"RFC0016_PRODUCTION_COURSE_GATE:{GATE}")
    print(f"RFC0016_PRODUCTION_COURSE_CLIENT:{production_client}")
    print(f"RFC0016_PRODUCTION_COURSE_PROBES:{probes}")
    print(f"RFC0016_PRODUCTION_COURSE_ROOT:{arguments.output_root}")
    print("RFC0016_PRODUCTION_COURSE_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
