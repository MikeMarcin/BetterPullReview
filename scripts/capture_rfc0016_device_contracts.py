#!/usr/bin/env python3
"""Offline sanitizer and validator for RFC-BPR-0016 capture inputs.

The module deliberately contains no network client and never writes files.
Attended capture code may pass response bytes through the library functions in
memory, then persist only the returned canonical projections as an unattested
set. A separate live runner and independently verifiable attestation are
required before any execution-provenance claim. The command-line surface
validates an existing fixture root but cannot claim real-service completion.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import re
import sys
from datetime import datetime
from pathlib import Path, PurePosixPath
from typing import Callable, Mapping, Sequence
from urllib.parse import urlsplit


SCHEMA_VERSION = 1
AUTHORITY_SUBJECT_SHA256 = (
    "sha256:d3cfb906170799ee7c90a8290cabee1b1e7e6afef40937a08f7b21bd5b42d178"
)
AUTHORITY_AMENDMENT_ID = "rfc0016-private-owner-only-2026-08-13"
CAPTURE_TOOL = "scripts/capture_rfc0016_device_contracts.py"
REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
CAPTURE_RUNNER_PATH = "scripts/run_rfc0016_device_capture_attended.py"
ATTENDED_VALIDATOR_PATH = (
    "scripts/verification/rfc0016_attended_contract/scripts/build_attended_review.py"
)
HISTORICAL_ATTENDED_VALIDATOR_PATH = ".agents/skills/bpr-attended-review/scripts/build_attended_review.py"
ATTESTATION_REVIEW_GATE = "rfc0016_attended_live_capture"
ATTESTATION_REVIEW_SCOPE = "human_sanitized_capture_bytes_acceptance_only"
ATTESTATION_SUBJECT_ID = (
    "evidence:/betterpullreview/bpr-0276/attended-live-capture"
)
ATTESTATION_SUBJECT_KEY = "bpr0276-attended-live-capture-v1"
ATTESTATION_REVIEW_TITLE = "RFC-BPR-0016 attended live capture evidence"
ATTESTATION_DESCRIPTION = (
    "Decide whether these exact sanitized fixture and live-run-attestation "
    "bytes are accepted as attended BPR-0276 run evidence."
)
ATTESTATION_DISPLAY_NAME = "Accept exact sanitized RFC-BPR-0016 capture bytes"
ATTESTATION_REFERENCE_DISCLAIMER = (
    "Acceptance records human review of these exact sanitized bytes as attended "
    "run evidence. It does not establish cryptographic GitHub or transport authenticity."
)
ATTESTATION_DOWNSTREAM_CLAIMS = {
    "bpr_0276_closed": False,
    "cryptographic_github_authenticity_claimed": False,
    "release_gate_satisfied": False,
    "rfc_closeout_claimed": False,
}
ATTESTATION_VALIDATION_CLAIMS = {
    "verdict_validated_against_its_batch_manifest": True,
    "rfc0016_sanitized_capture_subject_accepted": True,
    **ATTESTATION_DOWNSTREAM_CLAIMS,
}
ATTESTATION_ACCEPTANCE_CRITERIA = [
    "Every sanitized course, fixture, and live-run-attestation byte is bound by this subject.",
    "Acceptance means only that these exact bytes are accepted as attended BPR-0276 run evidence.",
]
ATTESTATION_KNOWN_CONCERNS = [
    "Human acceptance does not establish cryptographic GitHub or transport authenticity."
]
ATTESTATION_AUDIT = {
    "note": "The offline RFC-0016 structural validator accepted the bound unattested fixture.",
    "status": "pass",
}
ATTESTATION_INPUT_PATH = "offline-review-input.json"
ATTESTATION_PAGE_PATH = "offline-package/index.html"
ATTESTATION_MANIFEST_PATH = "offline-package/attended-review-manifest.json"
ATTESTATION_VERDICT_PATH = "human-verdict.json"
ATTESTATION_VALIDATION_PATH = "verdict-validation.json"
ATTESTATION_COURSES = (
    "access-denied",
    "device-flow-disabled",
    "expired-token",
    "slow-down",
    "token-success",
)
ATTESTATION_COURSE_ARTIFACTS = {
    "access-denied": {
        "course-execution.json",
        "registration.json",
        "outcomes/access_denied.json",
        "outcomes/authorization_pending.json",
        "outcomes/device_code_success.json",
    },
    "device-flow-disabled": {
        "course-execution.json",
        "device-flow-transition.json",
        "registration.json",
        "outcomes/device_flow_disabled.json",
    },
    "expired-token": {
        "course-execution.json",
        "registration.json",
        "outcomes/device_code_success.json",
        "outcomes/expired_token.json",
    },
    "slow-down": {
        "course-execution.json",
        "registration.json",
        "outcomes/authorization_pending.json",
        "outcomes/device_code_success.json",
        "outcomes/slow_down.json",
    },
    "token-success": {
        "course-execution.json",
        "registration.json",
        "route-probes.json",
        "outcomes/authorization_pending.json",
        "outcomes/device_code_success.json",
        "outcomes/token_success.json",
    },
}
APPROVED_PUBLIC_CLIENT_ID = "Iv23lil08fH3933w9MkW"
APPROVED_APPLICATION_ID = 4581304
SLOW_DOWN_REPLACEMENT_AUTHORITY_ID = (
    "rfc0016-bpr0276-slow-down-replacement-once-2026-08-13"
)
SLOW_DOWN_RECOVERY_AUTHORITY_ID = (
    "rfc0016-bpr0276-slow-down-parser-repair-recovery-once-2026-08-13"
)
SLOW_DOWN_AUTHORITY_PATH = "slow-down-replacement-authority.json"
SLOW_DOWN_SPENT_PATH = "slow-down-reservation-spent.json"
SLOW_DOWN_RECOVERY_AUTHORITY_PATH = (
    "slow-down-parser-repair-recovery-authority.json"
)
SLOW_DOWN_RECOVERY_SPENT_PATH = "slow-down-parser-repair-recovery-spent.json"
SLOW_DOWN_AUTHORITY_RECORD_PATHS = (
    SLOW_DOWN_AUTHORITY_PATH,
    SLOW_DOWN_SPENT_PATH,
    SLOW_DOWN_RECOVERY_AUTHORITY_PATH,
    SLOW_DOWN_RECOVERY_SPENT_PATH,
)
MAX_JSON_BYTES = 64 * 1024
MAX_FIXTURE_BYTES = 512 * 1024
ARTIFACT_SOURCE_KINDS = frozenset({"synthetic", "unattested_capture"})
PUBLIC_CLIENT_ID = re.compile(r"^[A-Za-z0-9.]{16,64}$")
SHA256 = re.compile(r"^sha256:[0-9a-f]{64}$")
UTC_TIMESTAMP = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")

ALLOWED_URL_HOSTS = frozenset(
    {"api.github.com", "docs.github.com", "github.com"}
)
FORBIDDEN_BYTE_PATTERNS = (
    re.compile(rb"(?i)\b(?:bearer|basic)\s+[A-Za-z0-9._~+/=-]+"),
    re.compile(rb"(?i)\bgh[pousr]_[A-Za-z0-9]{8,}\b"),
    re.compile(rb"(?i)-----BEGIN [A-Z ]*PRIVATE KEY-----"),
    re.compile(rb"\b[A-Z0-9]{4}-[A-Z0-9]{4}\b"),
    re.compile(rb"(?i)\b(?:authorization|proxy-authorization|cookie|set-cookie)\s*[:=]"),
    re.compile(
        rb"(?i)[\"']?(?:access_token|refresh_token|client_secret|device_code|"
        rb"user_code|password|credential|secret)[\"']?\s*[:=]"
    ),
    re.compile(
        rb"(?i)\b(?:GET|POST|PUT|PATCH|DELETE|HEAD|OPTIONS)\s+\S+\s+HTTP/[0-9.]+"
    ),
    re.compile(rb"(?i)\bHTTP/[0-9.]+\s+[1-5][0-9]{2}\b"),
)
OPAQUE_VALUE = re.compile(r"^[A-Za-z0-9_-]{32,}$")
BASE64_VALUE = re.compile(r"^[A-Za-z0-9+/]{32,}={0,2}$")
PUBLIC_CLIENT_ID_IN_TEXT = re.compile(r"\bIv1\.[A-Za-z0-9]{12,}\b")
SAFE_LONG_VALUES = frozenset(
    {
        AUTHORITY_AMENDMENT_ID,
        "attended_runner_completed_unattested",
        "expiring_user_token_with_refresh",
    }
)
EMAIL = re.compile(r"(?i)\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}\b")
IPV4 = re.compile(r"(?<![\d.])(?:\d{1,3}\.){3}\d{1,3}(?![\d.])")
LOCAL_PATH = re.compile(r"(?i)(?:/Users/|/home/|[A-Z]:\\+Users\\+)")
URL = re.compile(r"https?://[^\s\"'<>]+")

OUTCOMES = (
    "access_denied",
    "authorization_pending",
    "device_code_success",
    "device_flow_disabled",
    "expired_token",
    "slow_down",
    "token_success",
)
OUTCOME_ENDPOINT = {
    "device_code_success": "device_code",
    "device_flow_disabled": "device_code",
    "access_denied": "token",
    "authorization_pending": "token",
    "expired_token": "token",
    "slow_down": "token",
    "token_success": "token",
}
ENDPOINT_URL = {
    "device_code": "https://github.com/login/device/code",
    "token": "https://github.com/login/oauth/access_token",
}
OMITTED_FIELDS = {
    "device_code_success": ["device_code", "user_code"],
    "token_success": ["access_token", "refresh_token"],
    "access_denied": ["error_description", "error_uri"],
    "authorization_pending": ["error_description", "error_uri"],
    "device_flow_disabled": ["error_description", "error_uri"],
    "expired_token": ["error_description", "error_uri"],
    "slow_down": ["error_description", "error_uri"],
}
OBSERVED_FIELDS = {
    "device_code_success": [
        "device_code",
        "expires_in",
        "interval",
        "user_code",
        "verification_uri",
    ],
    "token_success": [
        "access_token",
        "expires_in",
        "refresh_token",
        "refresh_token_expires_in",
        "scope",
        "token_type",
    ],
    "access_denied": ["error", "error_description", "error_uri"],
    "authorization_pending": ["error", "error_description", "error_uri"],
    "device_flow_disabled": ["error", "error_description", "error_uri"],
    "expired_token": ["error", "error_description", "error_uri"],
    "slow_down": ["error", "error_description", "error_uri", "interval"],
}

# The designated read-only probe that BPR-0276's owner-attended capture sent on
# 2026-08-24. It is recorded evidence, not the provider's current review query
# (github_read_contract.cpp): the attended review binds route-probes.json by
# digest, and request_contract_sha256 binds this exact text. Leave it unchanged
# when the provider's query grows; a new probe needs a new attended capture.
# BPR-1248 restored it after f44c95dd7 edited it with the provider's query.
GRAPHQL_DOCUMENT = (
    "query GetReviewData($owner:String!,$name:String!,$number:Int!){"
    "repository(owner:$owner,name:$name){id pullRequest(number:$number){id headRefOid "
    "files(first:100){totalCount nodes{path viewerViewedState} "
    "pageInfo{hasNextPage endCursor}} reviewThreads(first:100){totalCount "
    "nodes{id isResolved isOutdated viewerCanResolve viewerCanUnresolve "
    "subjectType path diffSide line originalLine comments(first:100){totalCount "
    "nodes{id bodyText path line originalLine outdated author{login ... on "
    "Node{id}} replyTo{id}} pageInfo{hasNextPage endCursor}}} "
    "pageInfo{hasNextPage endCursor}}}}}"
)
GRAPHQL_VARIABLES = {"name": "BPR-Test", "number": 1, "owner": "MikeMarcin"}
GIT_UPLOAD_PACK_BODY = (
    b"0014command=ls-refs\n"
    b"0015agent=git/2.43.0\n"
    b"0001"
    b"0014ref-prefix HEAD\n"
    b"0000"
)


class ContractError(ValueError):
    """A fail-closed contract rejection with a non-sensitive diagnostic."""


def _reject_nonfinite_number(_value: str) -> None:
    raise ContractError("non-finite JSON number")


def _reject_duplicate_members(
    pairs: list[tuple[str, object]],
) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ContractError("duplicate JSON member")
        result[key] = value
    return result


def _parse_json(payload: bytes) -> object:
    if len(payload) > MAX_JSON_BYTES:
        raise ContractError("JSON size limit exceeded")
    if b"\x00" in payload or payload.startswith(b"\xef\xbb\xbf"):
        raise ContractError("unsupported JSON encoding")
    try:
        text = payload.decode("utf-8")
        document = json.loads(
            text,
            object_pairs_hook=_reject_duplicate_members,
            parse_constant=_reject_nonfinite_number,
        )
        _validate_json_structure(document)
        return document
    except ContractError:
        raise
    except (
        UnicodeDecodeError,
        json.JSONDecodeError,
        RecursionError,
        ValueError,
    ) as error:
        raise ContractError("malformed JSON") from error


def _validate_json_structure(document: object) -> None:
    stack = [(document, 0)]
    visited = 0
    while stack:
        value, depth = stack.pop()
        visited += 1
        if depth > 16 or visited > 1024:
            raise ContractError("JSON structural limit exceeded")
        if isinstance(value, dict):
            if len(value) > 64:
                raise ContractError("JSON structural limit exceeded")
            stack.extend((nested, depth + 1) for nested in value.values())
        elif isinstance(value, list):
            if len(value) > 64:
                raise ContractError("JSON structural limit exceeded")
            stack.extend((nested, depth + 1) for nested in value)
        elif isinstance(value, str) and len(value.encode("utf-8")) > 16 * 1024:
            raise ContractError("JSON string limit exceeded")


def canonical_json(document: object) -> bytes:
    """Return the only admitted durable JSON encoding."""
    return (
        json.dumps(document, indent=2, sort_keys=True, ensure_ascii=True) + "\n"
    ).encode("utf-8")


def _require_exact_keys(
    value: object,
    expected: set[str],
    label: str,
) -> dict[str, object]:
    if not isinstance(value, dict) or set(value) != expected:
        raise ContractError(f"{label} schema mismatch")
    return value


def _sha256(payload: bytes) -> str:
    return f"sha256:{hashlib.sha256(payload).hexdigest()}"


def _validate_timestamp(value: object) -> str:
    if not isinstance(value, str) or not UTC_TIMESTAMP.fullmatch(value):
        raise ContractError("capture timestamp is invalid")
    try:
        datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ")
    except ValueError as error:
        raise ContractError("capture timestamp is invalid") from error
    return value


def _validate_url(value: str) -> None:
    parsed = urlsplit(value)
    try:
        port = parsed.port
    except ValueError as error:
        raise ContractError("URL authority is not allowed") from error
    if (
        parsed.scheme != "https"
        or parsed.hostname not in ALLOWED_URL_HOSTS
        or parsed.username is not None
        or parsed.password is not None
        or port is not None
        or (parsed.fragment and parsed.hostname != "docs.github.com")
    ):
        raise ContractError("URL authority is not allowed")


def _walk_strings(value: object, visit: Callable[[str], None]) -> None:
    if isinstance(value, dict):
        for nested in value.values():
            _walk_strings(nested, visit)
    elif isinstance(value, list):
        for nested in value:
            _walk_strings(nested, visit)
    elif isinstance(value, str):
        visit(value)


def _validate_persisted_security(
    document: object,
    *,
    public_client_id: str | None = None,
) -> None:
    payload = canonical_json(document)
    for pattern in FORBIDDEN_BYTE_PATTERNS:
        if pattern.search(payload):
            raise ContractError("forbidden sensitive byte pattern")

    def validate_string(value: str) -> None:
        if (
            "\x00" in value
            or "\r" in value
            or "/../" in value
            or "\\..\\" in value
        ):
            raise ContractError("forbidden string encoding")
        if EMAIL.search(value) or IPV4.search(value) or LOCAL_PATH.search(value):
            raise ContractError("forbidden local identity material")
        for match in URL.finditer(value):
            _validate_url(match.group(0))
        if PUBLIC_CLIENT_ID_IN_TEXT.search(value) and value != public_client_id:
            raise ContractError("public client identifier outside registration")
        if (
            OPAQUE_VALUE.fullmatch(value)
            or BASE64_VALUE.fullmatch(value)
        ) and (
            value != public_client_id
            and not SHA256.fullmatch(value)
            and value != GRAPHQL_DOCUMENT
            and value not in SAFE_LONG_VALUES
        ):
            raise ContractError("opaque sensitive-looking value")

    _walk_strings(document, validate_string)


def _validate_provenance(
    value: object,
    *,
    observation: str,
    require_real: bool,
    registration_bound: bool,
) -> dict[str, object]:
    expected_keys = {
        "authority_amendment_id",
        "authority_subject_sha256",
        "capture_tool",
        "captured_at_utc",
        "observation",
        "service",
        "source_kind",
    }
    if registration_bound:
        expected_keys.add("registration_record_sha256")
    provenance = _require_exact_keys(
        value,
        expected_keys,
        "provenance",
    )
    if require_real:
        raise ContractError("external capture attestation is required")
    source_kind = provenance["source_kind"]
    if (
        not isinstance(source_kind, str)
        or source_kind not in ARTIFACT_SOURCE_KINDS
    ):
        raise ContractError("capture source is not admissible")
    if (
        provenance["authority_amendment_id"] != AUTHORITY_AMENDMENT_ID
        or provenance["authority_subject_sha256"] != AUTHORITY_SUBJECT_SHA256
        or provenance["capture_tool"] != CAPTURE_TOOL
        or provenance["observation"] != observation
        or provenance["service"] != "github.com"
    ):
        raise ContractError("provenance identity mismatch")
    _validate_timestamp(provenance["captured_at_utc"])
    if registration_bound:
        registration_digest = provenance["registration_record_sha256"]
        if (
            not isinstance(registration_digest, str)
            or not SHA256.fullmatch(registration_digest)
        ):
            raise ContractError("registration provenance digest is invalid")
    return provenance


def make_provenance(
    observation: str,
    captured_at_utc: str,
    *,
    source_kind: str,
    registration_record_sha256: str | None = None,
) -> dict[str, object]:
    provenance: dict[str, object] = {
        "authority_amendment_id": AUTHORITY_AMENDMENT_ID,
        "authority_subject_sha256": AUTHORITY_SUBJECT_SHA256,
        "capture_tool": CAPTURE_TOOL,
        "captured_at_utc": captured_at_utc,
        "observation": observation,
        "service": "github.com",
        "source_kind": source_kind,
    }
    if registration_record_sha256 is not None:
        provenance["registration_record_sha256"] = registration_record_sha256
    _validate_provenance(
        provenance,
        observation=observation,
        require_real=False,
        registration_bound=registration_record_sha256 is not None,
    )
    return provenance


def _require_string(value: object, label: str, maximum: int = 512) -> str:
    if not isinstance(value, str) or not value or len(value.encode("utf-8")) > maximum:
        raise ContractError(f"{label} is invalid")
    return value


def _require_positive_int(value: object, label: str, maximum: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not 0 < value <= maximum:
        raise ContractError(f"{label} is invalid")
    return value


def sanitize_outcome(
    outcome: str,
    raw_body: bytes,
    *,
    http_status: int,
    captured_at_utc: str,
    source_kind: str,
    registration_record_sha256: str,
) -> dict[str, object]:
    """Sanitize one response in memory without retaining transient values."""
    if outcome not in OUTCOMES:
        raise ContractError("unknown outcome")
    body = _require_exact_keys(
        _parse_json(raw_body),
        set(OBSERVED_FIELDS[outcome]),
        "response",
    )
    if (
        isinstance(http_status, bool)
        or not isinstance(http_status, int)
        or not 100 <= http_status <= 599
    ):
        raise ContractError("HTTP status is invalid")

    values: dict[str, object]
    if outcome == "device_code_success":
        _require_string(body["device_code"], "transient device value", 512)
        _require_string(body["user_code"], "transient user value", 64)
        verification_uri = _require_string(body["verification_uri"], "verification URI")
        if verification_uri != "https://github.com/login/device":
            raise ContractError("verification URI is not allowed")
        values = {
            "expires_in": _require_positive_int(body["expires_in"], "expiry", 3600),
            "interval": _require_positive_int(body["interval"], "poll interval", 300),
            "verification_uri": verification_uri,
        }
    elif outcome == "token_success":
        _require_string(body["access_token"], "transient access value", 4096)
        _require_string(body["refresh_token"], "transient refresh value", 4096)
        if body["token_type"] != "bearer" or body["scope"] != "":
            raise ContractError("token response authority mismatch")
        values = {
            "expires_in": _require_positive_int(body["expires_in"], "expiry", 31_536_000),
            "refresh_token_expires_in": _require_positive_int(
                body["refresh_token_expires_in"],
                "refresh expiry",
                63_072_000,
            ),
            "scope": "",
            "token_type": "bearer",
        }
    else:
        if body["error"] != outcome:
            raise ContractError("error outcome mismatch")
        _require_string(body["error_description"], "error description")
        error_uri = _require_string(body["error_uri"], "error URI")
        _validate_url(error_uri)
        if urlsplit(error_uri).hostname != "docs.github.com":
            raise ContractError("error URI is not official documentation")
        values = {
            "error": outcome,
            "official_error_uri": True,
        }
        if outcome == "slow_down":
            values["interval"] = _require_positive_int(
                body["interval"],
                "slow-down interval",
                300,
            )

    artifact: dict[str, object] = {
        "content_type": "application/json",
        "endpoint": OUTCOME_ENDPOINT[outcome],
        "endpoint_url": ENDPOINT_URL[OUTCOME_ENDPOINT[outcome]],
        "http_status": http_status,
        "outcome": outcome,
        "projection": {
            "observed_fields": OBSERVED_FIELDS[outcome],
            "omitted_transient_fields": OMITTED_FIELDS.get(outcome, []),
            "values": values,
        },
        "provenance": make_provenance(
            f"{outcome}_response",
            captured_at_utc,
            source_kind=source_kind,
            registration_record_sha256=registration_record_sha256,
        ),
        "request_content_type": "application/x-www-form-urlencoded",
        "request_method": "POST",
        "schema_name": "bpr.rfc0016.device_contract",
        "schema_version": SCHEMA_VERSION,
    }
    validate_outcome_artifact(artifact, require_real=False)
    return artifact


def validate_outcome_artifact(value: object, *, require_real: bool) -> None:
    artifact = _require_exact_keys(
        value,
        {
            "content_type",
            "endpoint",
            "endpoint_url",
            "http_status",
            "outcome",
            "projection",
            "provenance",
            "request_content_type",
            "request_method",
            "schema_name",
            "schema_version",
        },
        "outcome artifact",
    )
    outcome = artifact["outcome"]
    if outcome not in OUTCOMES:
        raise ContractError("unknown outcome")
    if (
        artifact["schema_name"] != "bpr.rfc0016.device_contract"
        or type(artifact["schema_version"]) is not int
        or artifact["schema_version"] != SCHEMA_VERSION
        or artifact["content_type"] != "application/json"
        or artifact["endpoint"] != OUTCOME_ENDPOINT[outcome]
        or artifact["endpoint_url"] != ENDPOINT_URL[OUTCOME_ENDPOINT[outcome]]
        or artifact["request_content_type"]
        != "application/x-www-form-urlencoded"
        or artifact["request_method"] != "POST"
        or isinstance(artifact["http_status"], bool)
        or not isinstance(artifact["http_status"], int)
        or not 100 <= artifact["http_status"] <= 599
    ):
        raise ContractError("outcome artifact identity mismatch")
    projection = _require_exact_keys(
        artifact["projection"],
        {"observed_fields", "omitted_transient_fields", "values"},
        "outcome projection",
    )
    if (
        projection["observed_fields"] != OBSERVED_FIELDS[outcome]
        or projection["omitted_transient_fields"] != OMITTED_FIELDS.get(outcome, [])
    ):
        raise ContractError("outcome field inventory mismatch")
    values = projection["values"]
    if outcome == "device_code_success":
        expected_keys = {"expires_in", "interval", "verification_uri"}
        _require_positive_int(
            values.get("expires_in") if isinstance(values, dict) else None,
            "expiry",
            3600,
        )
        _require_positive_int(
            values.get("interval") if isinstance(values, dict) else None,
            "poll interval",
            300,
        )
        if (
            not isinstance(values, dict)
            or values.get("verification_uri")
            != "https://github.com/login/device"
        ):
            raise ContractError("device projection mismatch")
    elif outcome == "token_success":
        expected_keys = {"expires_in", "refresh_token_expires_in", "scope", "token_type"}
        if not isinstance(values, dict):
            raise ContractError("token projection mismatch")
        _require_positive_int(values.get("expires_in"), "expiry", 31_536_000)
        _require_positive_int(values.get("refresh_token_expires_in"), "refresh expiry", 63_072_000)
        if values.get("scope") != "" or values.get("token_type") != "bearer":
            raise ContractError("token projection mismatch")
    elif outcome == "slow_down":
        expected_keys = {"error", "interval", "official_error_uri"}
        if (
            not isinstance(values, dict)
            or values.get("error") != outcome
            or values.get("official_error_uri") is not True
        ):
            raise ContractError("error projection mismatch")
        _require_positive_int(
            values.get("interval"),
            "slow-down interval",
            300,
        )
    else:
        expected_keys = {"error", "official_error_uri"}
        if (
            not isinstance(values, dict)
            or values.get("error") != outcome
            or values.get("official_error_uri") is not True
        ):
            raise ContractError("error projection mismatch")
    if not isinstance(values, dict) or set(values) != expected_keys:
        raise ContractError("outcome value schema mismatch")
    _validate_provenance(
        artifact["provenance"],
        observation=f"{outcome}_response",
        require_real=require_real,
        registration_bound=True,
    )
    _validate_persisted_security(artifact)


REGISTRATION_PERMISSIONS = {
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


def sanitize_registration(
    observed: object,
    *,
    captured_at_utc: str,
    source_kind: str,
) -> dict[str, object]:
    registration = _require_exact_keys(
        observed,
        {
            "application_kind",
            "application_visibility",
            "callback_url_present",
            "device_flow_enabled",
            "installation_owner_login",
            "installation_policy",
            "installation_repositories",
            "installation_repository_selection",
            "oauth_scopes",
            "oauth_client_type",
            "owner_kind",
            "owner_login",
            "permissions",
            "public_client_id",
            "refresh_policy",
            "user_access_token_expiration_enabled",
            "webhook_active",
        },
        "registration observation",
    )
    client_id = registration["public_client_id"]
    if not isinstance(client_id, str) or not PUBLIC_CLIENT_ID.fullmatch(client_id):
        raise ContractError("public client identifier is invalid")
    artifact: dict[str, object] = dict(registration)
    artifact.update(
        {
            "provenance": make_provenance(
                "github_app_registration",
                captured_at_utc,
                source_kind=source_kind,
            ),
            "public_client_id_sha256": _sha256(client_id.encode("ascii")),
            "schema_name": "bpr.rfc0016.github_app_registration",
            "schema_version": SCHEMA_VERSION,
        }
    )
    validate_registration_artifact(artifact, require_real=False)
    return artifact


def validate_registration_artifact(value: object, *, require_real: bool) -> None:
    artifact = _require_exact_keys(
        value,
        {
            "application_kind",
            "application_visibility",
            "callback_url_present",
            "device_flow_enabled",
            "installation_owner_login",
            "installation_policy",
            "installation_repositories",
            "installation_repository_selection",
            "oauth_client_type",
            "oauth_scopes",
            "owner_kind",
            "owner_login",
            "permissions",
            "provenance",
            "public_client_id",
            "public_client_id_sha256",
            "refresh_policy",
            "schema_name",
            "schema_version",
            "user_access_token_expiration_enabled",
            "webhook_active",
        },
        "registration artifact",
    )
    client_id = artifact["public_client_id"]
    if not isinstance(client_id, str) or not PUBLIC_CLIENT_ID.fullmatch(client_id):
        raise ContractError("public client identifier is invalid")
    expected = {
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
        "permissions": REGISTRATION_PERMISSIONS,
        "public_client_id_sha256": _sha256(client_id.encode("ascii")),
        "refresh_policy": "expiring_user_token_with_refresh",
        "schema_name": "bpr.rfc0016.github_app_registration",
        "schema_version": SCHEMA_VERSION,
        "user_access_token_expiration_enabled": True,
        "webhook_active": False,
    }
    for key, expected_value in expected.items():
        _require_exact_value(
            artifact[key],
            expected_value,
            "registration authority",
        )
    _validate_provenance(
        artifact["provenance"],
        observation="github_app_registration",
        require_real=require_real,
        registration_bound=False,
    )
    _validate_persisted_security(artifact, public_client_id=client_id)


def _request_digest(method: str, url: str, body: str = "") -> str:
    return _sha256(f"{method}\n{url}\n{body}".encode("utf-8"))


def expected_route_probes() -> list[dict[str, object]]:
    graphql_body = json.dumps(
        {"query": GRAPHQL_DOCUMENT, "variables": GRAPHQL_VARIABLES},
        separators=(",", ":"),
        sort_keys=True,
    )
    routes = (
        ("metadata", "rest", "GET", "https://api.github.com/repos/MikeMarcin/BPR-Test", ""),
        (
            "contents_info_refs",
            "smart_http",
            "GET",
            "https://github.com/MikeMarcin/BPR-Test.git/info/refs?service=git-upload-pack",
            "",
        ),
        (
            "contents_upload_pack",
            "smart_http",
            "POST",
            "https://github.com/MikeMarcin/BPR-Test.git/git-upload-pack",
            GIT_UPLOAD_PACK_BODY.decode("ascii"),
        ),
        (
            "checks",
            "rest",
            "GET",
            "https://api.github.com/repos/MikeMarcin/BPR-Test/commits/HEAD/check-runs",
            "",
        ),
        (
            "pull_request",
            "rest",
            "GET",
            "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1",
            "",
        ),
        (
            "pull_request_files",
            "rest",
            "GET",
            "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1/files?per_page=100&page=1",
            "",
        ),
        (
            "pull_request_commits",
            "rest",
            "GET",
            "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1/commits?per_page=100&page=1",
            "",
        ),
        (
            "pull_request_reviews",
            "rest",
            "GET",
            "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1/reviews?per_page=100&page=1",
            "",
        ),
        (
            "pull_request_issue_comments",
            "rest",
            "GET",
            "https://api.github.com/repos/MikeMarcin/BPR-Test/"
            "issues/1/comments?per_page=100&page=1",
            "",
        ),
        ("review_threads", "graphql", "POST", "https://api.github.com/graphql", graphql_body),
    )
    return [
        {
            "http_status": 200,
            "method": method,
            "operation": operation,
            "request_contract_sha256": _request_digest(method, url, body),
            "result": "success",
            "transport": transport,
            "url": url,
        }
        for operation, transport, method, url, body in routes
    ]


def sanitize_route_probes(
    observed_probes: object,
    *,
    captured_at_utc: str,
    source_kind: str,
    registration_record_sha256: str,
) -> dict[str, object]:
    if observed_probes != expected_route_probes():
        raise ContractError("read-only route inventory mismatch")
    artifact: dict[str, object] = {
        "graphql_document": GRAPHQL_DOCUMENT,
        "graphql_document_sha256": _sha256(GRAPHQL_DOCUMENT.encode("utf-8")),
        "graphql_variables": GRAPHQL_VARIABLES,
        "probes": observed_probes,
        "provenance": make_provenance(
            "read_only_route_probes",
            captured_at_utc,
            source_kind=source_kind,
            registration_record_sha256=registration_record_sha256,
        ),
        "repository": {
            "database_id": 1328277631,
            "name": "BPR-Test",
            "owner": "MikeMarcin",
            "pull_request_number": 1,
        },
        "schema_name": "bpr.rfc0016.read_only_route_probes",
        "schema_version": SCHEMA_VERSION,
    }
    validate_route_probe_artifact(artifact, require_real=False)
    return artifact


def validate_route_probe_artifact(value: object, *, require_real: bool) -> None:
    artifact = _require_exact_keys(
        value,
        {
            "graphql_document",
            "graphql_document_sha256",
            "graphql_variables",
            "probes",
            "provenance",
            "repository",
            "schema_name",
            "schema_version",
        },
        "route probe artifact",
    )
    if (
        artifact["schema_name"] != "bpr.rfc0016.read_only_route_probes"
        or type(artifact["schema_version"]) is not int
        or artifact["schema_version"] != SCHEMA_VERSION
        or artifact["graphql_document"] != GRAPHQL_DOCUMENT
        or artifact["graphql_document_sha256"] != _sha256(GRAPHQL_DOCUMENT.encode("utf-8"))
        or not _matches_exact(artifact["graphql_variables"], GRAPHQL_VARIABLES)
        or not _matches_exact(artifact["repository"], {
            "database_id": 1328277631,
            "name": "BPR-Test",
            "owner": "MikeMarcin",
            "pull_request_number": 1,
        })
        or not _matches_exact(artifact["probes"], expected_route_probes())
    ):
        raise ContractError("read-only route inventory mismatch")
    _validate_provenance(
        artifact["provenance"],
        observation="read_only_route_probes",
        require_real=require_real,
        registration_bound=True,
    )
    _validate_persisted_security(artifact)


COMPLETE_ARTIFACTS = (
    "registration.json",
    "route-probes.json",
    *(f"outcomes/{outcome}.json" for outcome in OUTCOMES),
)


def _artifact_source_kind(document: object) -> object:
    if not isinstance(document, dict):
        return None
    provenance = document.get("provenance")
    if not isinstance(provenance, dict):
        return None
    return provenance.get("source_kind")


def _build_manifest(
    artifacts: Mapping[str, bytes],
    *,
    source_kind: str,
) -> dict[str, object]:
    if (
        not isinstance(source_kind, str)
        or source_kind not in ARTIFACT_SOURCE_KINDS
    ):
        raise ContractError("manifest source is not admissible")
    if set(artifacts) != set(COMPLETE_ARTIFACTS):
        raise ContractError("complete artifact inventory mismatch")
    registration_payload = artifacts["registration.json"]
    if not isinstance(registration_payload, bytes):
        raise ContractError("artifact payload is not bytes")
    registration_document = _parse_json(registration_payload)
    if canonical_json(registration_document) != registration_payload:
        raise ContractError("artifact encoding is not canonical")
    validate_registration_artifact(
        registration_document,
        require_real=False,
    )
    if not isinstance(registration_document, dict):
        raise ContractError("registration artifact is not an object")
    public_client_id = registration_document["public_client_id"]
    if not isinstance(public_client_id, str):
        raise ContractError("public client identifier is invalid")
    public_client_id_bytes = public_client_id.encode("ascii")
    registration_digest = _sha256(registration_payload)
    entries = []
    for relative_path in COMPLETE_ARTIFACTS:
        payload = artifacts[relative_path]
        if not isinstance(payload, bytes):
            raise ContractError("artifact payload is not bytes")
        if (
            relative_path != "registration.json"
            and public_client_id_bytes in payload
        ):
            raise ContractError("public client identifier outside registration")
        document = (
            registration_document
            if relative_path == "registration.json"
            else _parse_json(payload)
        )
        if canonical_json(document) != payload:
            raise ContractError("artifact encoding is not canonical")
        _validate_artifact_for_path(
            relative_path,
            document,
            require_real=False,
        )
        if _artifact_source_kind(document) != source_kind:
            raise ContractError("manifest artifact source mismatch")
        if relative_path != "registration.json":
            provenance = document.get("provenance")
            if (
                not isinstance(provenance, dict)
                or provenance.get("registration_record_sha256")
                != registration_digest
            ):
                raise ContractError("artifact registration binding mismatch")
        entries.append(
            {
                "path": relative_path,
                "sha256": _sha256(payload),
                "source_kind": source_kind,
            }
        )
    return {
        "artifacts": entries,
        "authority_amendment_id": AUTHORITY_AMENDMENT_ID,
        "authority_subject_sha256": AUTHORITY_SUBJECT_SHA256,
        "schema_name": "bpr.rfc0016.device_capture_manifest",
        "schema_version": SCHEMA_VERSION,
        "state": (
            "unattested_capture"
            if source_kind == "unattested_capture"
            else "synthetic_contract_test"
        ),
    }


def build_manifest(
    records: Sequence[tuple[str, bytes]],
) -> dict[str, object]:
    """Build an unattested manifest while rejecting duplicate observations."""
    artifacts: dict[str, bytes] = {}
    for relative_path, payload in records:
        if relative_path in artifacts:
            raise ContractError("duplicate capture observation")
        artifacts[relative_path] = payload
    return _build_manifest(artifacts, source_kind="unattested_capture")


def _validate_artifact_for_path(
    relative_path: str,
    document: object,
    *,
    require_real: bool,
) -> None:
    if relative_path == "registration.json":
        validate_registration_artifact(document, require_real=require_real)
    elif relative_path == "route-probes.json":
        validate_route_probe_artifact(document, require_real=require_real)
    elif relative_path.startswith("outcomes/") and relative_path.endswith(".json"):
        outcome = PurePosixPath(relative_path).stem
        validate_outcome_artifact(document, require_real=require_real)
        if not isinstance(document, dict) or document.get("outcome") != outcome:
            raise ContractError("outcome path identity mismatch")
    else:
        raise ContractError("artifact path is not allowed")


def _validate_manifest(
    value: object,
    artifacts: Mapping[str, bytes],
    *,
    require_real: bool,
) -> None:
    manifest = _require_exact_keys(
        value,
        {
            "artifacts",
            "authority_amendment_id",
            "authority_subject_sha256",
            "schema_name",
            "schema_version",
            "state",
        },
        "manifest",
    )
    if require_real:
        raise ContractError("external capture attestation is required")
    if manifest["authority_amendment_id"] != AUTHORITY_AMENDMENT_ID:
        raise ContractError("manifest authority amendment mismatch")
    source_kind_by_state = {
        "synthetic_contract_test": "synthetic",
        "unattested_capture": "unattested_capture",
    }
    state = manifest["state"]
    source_kind = (
        source_kind_by_state.get(state)
        if isinstance(state, str)
        else None
    )
    if source_kind is None:
        raise ContractError("manifest state is not admissible")
    expected = _build_manifest(artifacts, source_kind=source_kind)
    if not _matches_exact(manifest, expected):
        raise ContractError("manifest identity or digest mismatch")
    _validate_persisted_security(manifest)


def _fixture_files(root: Path) -> dict[str, Path]:
    if not root.is_dir() or root.is_symlink():
        raise ContractError("fixture root is not a directory")
    result: dict[str, Path] = {}
    for path in root.rglob("*"):
        if path.is_symlink():
            raise ContractError("fixture tree contains a link")
        if path.is_dir():
            continue
        if not path.is_file():
            raise ContractError("fixture tree contains a special file")
        relative = path.relative_to(root).as_posix()
        if PurePosixPath(relative).is_absolute() or ".." in PurePosixPath(relative).parts:
            raise ContractError("fixture path escapes root")
        result[relative] = path
    return result


def _read_bounded(path: Path, total: list[int]) -> bytes:
    size = path.stat().st_size
    if size > MAX_JSON_BYTES or total[0] + size > MAX_FIXTURE_BYTES:
        raise ContractError("fixture size limit exceeded")
    payload = path.read_bytes()
    if len(payload) > MAX_JSON_BYTES or total[0] + len(payload) > MAX_FIXTURE_BYTES:
        raise ContractError("fixture size limit exceeded")
    total[0] += len(payload)
    return payload


def _matches_exact(actual: object, expected: object) -> bool:
    if type(actual) is not type(expected):
        return False
    if isinstance(expected, dict):
        return set(actual) == set(expected) and all(
            _matches_exact(actual[key], value)
            for key, value in expected.items()
        )
    if isinstance(expected, list):
        return len(actual) == len(expected) and all(
            _matches_exact(actual_item, expected_item)
            for actual_item, expected_item in zip(actual, expected, strict=True)
        )
    return actual == expected


def _require_exact_value(actual: object, expected: object, label: str) -> None:
    if not _matches_exact(actual, expected):
        raise ContractError(f"{label} mismatch")


def _review_artifact_triple(
    value: object,
    *,
    expected_path: str,
    repository_root: Path,
) -> bytes:
    triple = _require_exact_keys(
        value,
        {"byte_length", "path", "sha256"},
        "review validation artifact",
    )
    if type(triple["path"]) is not str or triple["path"] != expected_path:
        raise ContractError("review validation artifact path mismatch")
    physical_path = (
        ATTENDED_VALIDATOR_PATH
        if expected_path == HISTORICAL_ATTENDED_VALIDATOR_PATH
        else expected_path
    )
    relative = PurePosixPath(physical_path)
    if relative.is_absolute() or ".." in relative.parts:
        raise ContractError("review validation artifact path escapes repository")
    path = repository_root.joinpath(*relative.parts)
    try:
        resolved = path.resolve(strict=True)
    except OSError as error:
        raise ContractError("review validation artifact is missing") from error
    if path.absolute() != resolved or not path.is_file() or path.is_symlink():
        raise ContractError("review validation artifact path is not canonical")
    if path.stat().st_size > MAX_FIXTURE_BYTES:
        raise ContractError("review validation artifact exceeds size limit")
    payload = path.read_bytes()
    if (
        isinstance(triple["byte_length"], bool)
        or not isinstance(triple["byte_length"], int)
        or triple["byte_length"] != len(payload)
        or type(triple["sha256"]) is not str
        or triple["sha256"] != _sha256(payload)
    ):
        raise ContractError("review validation artifact byte identity mismatch")
    return payload


def _current_capture_runner() -> tuple[Path, bytes, str]:
    repository_root = REPOSITORY_ROOT.resolve(strict=True)
    path = repository_root / CAPTURE_RUNNER_PATH
    try:
        resolved = path.resolve(strict=True)
    except OSError as error:
        raise ContractError("attended capture runner is missing") from error
    if (
        path.absolute() != resolved
        or not path.is_file()
        or path.is_symlink()
        or path.stat().st_size > MAX_FIXTURE_BYTES
    ):
        raise ContractError("attended capture runner is invalid")
    payload = path.read_bytes()
    return path, payload, _sha256(payload)


def _course_tree_digest(records: Mapping[str, bytes]) -> str:
    identity = b"".join(
        relative.encode("utf-8")
        + b"\0"
        + _sha256(payload).encode("ascii")
        + b"\n"
        for relative, payload in sorted(records.items())
    )
    return _sha256(identity)


def _validate_course_execution(
    course: str,
    value: object,
    *,
    runner_digest: str,
    registration_digest: str,
) -> dict[str, object]:
    execution = _require_exact_keys(
        value,
        {
            "authority_amendment_id",
            "authority_subject_sha256",
            "bindings",
            "capture_runner_sha256",
            "course",
            "executed_at_utc",
            "registration_record_sha256",
            "schema_name",
            "schema_version",
        },
        "course execution",
    )
    if (
        execution["authority_amendment_id"] != AUTHORITY_AMENDMENT_ID
        or execution["authority_subject_sha256"] != AUTHORITY_SUBJECT_SHA256
        or execution["capture_runner_sha256"] != runner_digest
        or execution["course"] != course
        or execution["registration_record_sha256"] != registration_digest
        or execution["schema_name"] != "bpr.rfc0016.course_execution"
        or type(execution["schema_version"]) is not int
        or execution["schema_version"] != SCHEMA_VERSION
    ):
        raise ContractError("course execution identity mismatch")
    _validate_timestamp(execution["executed_at_utc"])
    bindings = _require_exact_keys(
        execution["bindings"],
        {
            "device_flow_transition_sha256",
            "slow_down_original_reservation_sha256",
            "slow_down_recovery_reservation_sha256",
        },
        "course execution bindings",
    )
    transition = bindings["device_flow_transition_sha256"]
    original_reservation = bindings["slow_down_original_reservation_sha256"]
    recovery_reservation = bindings["slow_down_recovery_reservation_sha256"]
    if course == "device-flow-disabled":
        if type(transition) is not str or not SHA256.fullmatch(transition):
            raise ContractError("course transition binding mismatch")
    elif transition is not None:
        raise ContractError("course transition binding mismatch")
    if course == "slow-down":
        if (
            type(original_reservation) is not str
            or not SHA256.fullmatch(original_reservation)
            or type(recovery_reservation) is not str
            or not SHA256.fullmatch(recovery_reservation)
        ):
            raise ContractError("course reservation binding mismatch")
    elif original_reservation is not None or recovery_reservation is not None:
        raise ContractError("course reservation binding mismatch")
    return execution


def _validate_course_transition(
    payload: bytes,
    *,
    executed_at_utc: str,
) -> None:
    transition = _require_exact_keys(
        _parse_json(payload),
        {
            "application_id",
            "authority_amendment_id",
            "authority_subject_sha256",
            "observation_method",
            "owner_login",
            "public_client_id",
            "schema_name",
            "schema_version",
            "states",
        },
        "device-flow transition",
    )
    if canonical_json(transition) != payload:
        raise ContractError("device-flow transition encoding mismatch")
    if (
        type(transition["application_id"]) is not int
        or transition["application_id"] != APPROVED_APPLICATION_ID
        or transition["authority_amendment_id"] != AUTHORITY_AMENDMENT_ID
        or transition["authority_subject_sha256"] != AUTHORITY_SUBJECT_SHA256
        or transition["observation_method"] != "attended_github_settings_page"
        or transition["owner_login"] != "MikeMarcin"
        or transition["public_client_id"] != APPROVED_PUBLIC_CLIENT_ID
        or transition["schema_name"]
        != "bpr.rfc0016.device_flow_transition_observation"
        or type(transition["schema_version"]) is not int
        or transition["schema_version"] != SCHEMA_VERSION
    ):
        raise ContractError("device-flow transition identity mismatch")
    expected_states = (
        ("before_enabled", True),
        ("disabled_for_capture", False),
        ("restored_enabled", True),
    )
    states = transition["states"]
    if not isinstance(states, list) or len(states) != len(expected_states):
        raise ContractError("device-flow transition state mismatch")
    observed_times: list[datetime] = []
    for value, (expected_phase, expected_enabled) in zip(
        states,
        expected_states,
        strict=True,
    ):
        state = _require_exact_keys(
            value,
            {"device_flow_enabled", "observed_at_utc", "phase"},
            "device-flow transition state",
        )
        if (
            state["phase"] != expected_phase
            or state["device_flow_enabled"] is not expected_enabled
        ):
            raise ContractError("device-flow transition state mismatch")
        timestamp = _validate_timestamp(state["observed_at_utc"])
        observed_times.append(datetime.strptime(timestamp, "%Y-%m-%dT%H:%M:%SZ"))
    if not observed_times[0] < observed_times[1] < observed_times[2]:
        raise ContractError("device-flow transition ordering mismatch")
    execution_time = datetime.strptime(executed_at_utc, "%Y-%m-%dT%H:%M:%SZ")
    if not observed_times[1] <= execution_time <= observed_times[2]:
        raise ContractError("device-flow transition execution mismatch")


def _validate_slow_down_authority(
    payload: bytes,
) -> tuple[dict[str, object], datetime]:
    authority = _require_exact_keys(
        _parse_json(payload),
        {
            "authority_amendment_id",
            "authority_id",
            "authority_subject_sha256",
            "authorized_at_utc",
            "course",
            "previous_early_poll_consumed",
            "replacement_early_poll_limit",
            "schema_name",
            "schema_version",
            "state",
        },
        "slow-down replacement authority",
    )
    if canonical_json(authority) != payload:
        raise ContractError("slow-down replacement authority encoding mismatch")
    if (
        authority["authority_amendment_id"] != AUTHORITY_AMENDMENT_ID
        or authority["authority_id"] != SLOW_DOWN_REPLACEMENT_AUTHORITY_ID
        or authority["authority_subject_sha256"] != AUTHORITY_SUBJECT_SHA256
        or authority["course"] != "slow-down"
        or authority["previous_early_poll_consumed"] is not True
        or type(authority["replacement_early_poll_limit"]) is not int
        or authority["replacement_early_poll_limit"] != 1
        or authority["schema_name"]
        != "bpr.rfc0016.slow_down_replacement_authority"
        or type(authority["schema_version"]) is not int
        or authority["schema_version"] != SCHEMA_VERSION
        or authority["state"] != "replacement_authorized"
    ):
        raise ContractError("slow-down replacement authority mismatch")
    timestamp = _validate_timestamp(authority["authorized_at_utc"])
    return authority, datetime.strptime(timestamp, "%Y-%m-%dT%H:%M:%SZ")


def _validate_slow_down_spent_marker(
    payload: bytes,
    *,
    authority_payload: bytes,
    reservation_digest: object,
    authorized_at: datetime,
    executed_at_utc: str,
) -> tuple[str, datetime]:
    marker = _require_exact_keys(
        _parse_json(payload),
        {
            "authority_amendment_id",
            "authority_id",
            "authority_record_sha256",
            "authority_subject_sha256",
            "capture_runner_sha256",
            "course",
            "schema_name",
            "schema_version",
            "spent_before_traffic_at_utc",
            "state",
        },
        "slow-down spent marker",
    )
    if canonical_json(marker) != payload:
        raise ContractError("slow-down spent marker encoding mismatch")
    failed_runner_digest = marker["capture_runner_sha256"]
    if (
        marker["authority_amendment_id"] != AUTHORITY_AMENDMENT_ID
        or marker["authority_id"] != SLOW_DOWN_REPLACEMENT_AUTHORITY_ID
        or marker["authority_record_sha256"] != _sha256(authority_payload)
        or marker["authority_subject_sha256"] != AUTHORITY_SUBJECT_SHA256
        or type(failed_runner_digest) is not str
        or not SHA256.fullmatch(failed_runner_digest)
        or marker["course"] != "slow-down"
        or marker["schema_name"] != "bpr.rfc0016.slow_down_reservation"
        or type(marker["schema_version"]) is not int
        or marker["schema_version"] != SCHEMA_VERSION
        or marker["state"] != "spent"
        or reservation_digest != _sha256(payload)
    ):
        raise ContractError("slow-down spent marker mismatch")
    spent_timestamp = _validate_timestamp(marker["spent_before_traffic_at_utc"])
    spent_at = datetime.strptime(spent_timestamp, "%Y-%m-%dT%H:%M:%SZ")
    executed_at = datetime.strptime(executed_at_utc, "%Y-%m-%dT%H:%M:%SZ")
    if not authorized_at <= spent_at <= executed_at:
        raise ContractError("slow-down reservation timestamp mismatch")
    return failed_runner_digest, spent_at


def _validate_slow_down_recovery_authority(
    payload: bytes,
    *,
    original_authority_payload: bytes,
    original_marker_payload: bytes,
    failed_runner_digest: str,
    runner_digest: str,
    original_spent_at: datetime,
) -> datetime:
    authority = _require_exact_keys(
        _parse_json(payload),
        {
            "authority_amendment_id",
            "authority_id",
            "authority_subject_sha256",
            "authorized_at_utc",
            "course",
            "failed_attempt_authority_sha256",
            "failed_attempt_capture_runner_sha256",
            "failed_attempt_spent_marker_sha256",
            "parser_repair_capture_runner_sha256",
            "recovery_early_poll_limit",
            "schema_name",
            "schema_version",
            "state",
        },
        "slow-down recovery authority",
    )
    if canonical_json(authority) != payload:
        raise ContractError("slow-down recovery authority encoding mismatch")
    authorized_timestamp = _validate_timestamp(authority["authorized_at_utc"])
    authorized_at = datetime.strptime(authorized_timestamp, "%Y-%m-%dT%H:%M:%SZ")
    if (
        authority["authority_amendment_id"] != AUTHORITY_AMENDMENT_ID
        or authority["authority_id"] != SLOW_DOWN_RECOVERY_AUTHORITY_ID
        or authority["authority_subject_sha256"] != AUTHORITY_SUBJECT_SHA256
        or authority["course"] != "slow-down"
        or authority["failed_attempt_authority_sha256"]
        != _sha256(original_authority_payload)
        or authority["failed_attempt_capture_runner_sha256"]
        != failed_runner_digest
        or authority["failed_attempt_spent_marker_sha256"]
        != _sha256(original_marker_payload)
        or authority["parser_repair_capture_runner_sha256"] != runner_digest
        or type(authority["recovery_early_poll_limit"]) is not int
        or authority["recovery_early_poll_limit"] != 1
        or authority["schema_name"]
        != "bpr.rfc0016.slow_down_parser_repair_recovery_authority"
        or type(authority["schema_version"]) is not int
        or authority["schema_version"] != SCHEMA_VERSION
        or authority["state"] != "parser_repair_recovery_authorized"
        or failed_runner_digest == runner_digest
        or authorized_at < original_spent_at
    ):
        raise ContractError("slow-down recovery authority mismatch")
    return authorized_at


def _validate_slow_down_recovery_spent_marker(
    payload: bytes,
    *,
    authority_payload: bytes,
    original_authority_payload: bytes,
    original_marker_payload: bytes,
    runner_digest: str,
    reservation_digest: object,
    authorized_at: datetime,
    executed_at_utc: str,
) -> None:
    marker = _require_exact_keys(
        _parse_json(payload),
        {
            "authority_amendment_id",
            "authority_id",
            "authority_record_sha256",
            "authority_subject_sha256",
            "capture_runner_sha256",
            "course",
            "failed_attempt_authority_sha256",
            "failed_attempt_spent_marker_sha256",
            "schema_name",
            "schema_version",
            "spent_before_traffic_at_utc",
            "state",
        },
        "slow-down recovery spent marker",
    )
    if canonical_json(marker) != payload:
        raise ContractError("slow-down recovery spent marker encoding mismatch")
    if (
        marker["authority_amendment_id"] != AUTHORITY_AMENDMENT_ID
        or marker["authority_id"] != SLOW_DOWN_RECOVERY_AUTHORITY_ID
        or marker["authority_record_sha256"] != _sha256(authority_payload)
        or marker["authority_subject_sha256"] != AUTHORITY_SUBJECT_SHA256
        or marker["capture_runner_sha256"] != runner_digest
        or marker["course"] != "slow-down"
        or marker["failed_attempt_authority_sha256"]
        != _sha256(original_authority_payload)
        or marker["failed_attempt_spent_marker_sha256"]
        != _sha256(original_marker_payload)
        or marker["schema_name"]
        != "bpr.rfc0016.slow_down_parser_repair_recovery_reservation"
        or type(marker["schema_version"]) is not int
        or marker["schema_version"] != SCHEMA_VERSION
        or marker["state"] != "spent"
        or reservation_digest != _sha256(payload)
    ):
        raise ContractError("slow-down recovery spent marker mismatch")
    spent_timestamp = _validate_timestamp(marker["spent_before_traffic_at_utc"])
    spent_at = datetime.strptime(spent_timestamp, "%Y-%m-%dT%H:%M:%SZ")
    executed_at = datetime.strptime(executed_at_utc, "%Y-%m-%dT%H:%M:%SZ")
    if not authorized_at <= spent_at <= executed_at:
        raise ContractError("slow-down recovery reservation timestamp mismatch")


def _read_course_evidence_root(
    root: Path,
) -> tuple[dict[str, dict[str, bytes]], dict[str, bytes]]:
    if root.is_symlink() or not root.is_dir():
        raise ContractError("course evidence root is invalid")
    root_entries = list(root.iterdir())
    expected_root_entries = {
        *ATTESTATION_COURSES,
        *SLOW_DOWN_AUTHORITY_RECORD_PATHS,
    }
    if (
        {entry.name for entry in root_entries} != expected_root_entries
        or any(entry.is_symlink() for entry in root_entries)
    ):
        raise ContractError("course evidence root inventory mismatch")
    for course in ATTESTATION_COURSES:
        if not (root / course).is_dir():
            raise ContractError("course evidence root inventory mismatch")
    authority_records: dict[str, bytes] = {}
    total = [0]
    for relative in SLOW_DOWN_AUTHORITY_RECORD_PATHS:
        path = root / relative
        if not path.is_file():
            raise ContractError("course authority record is invalid")
        authority_records[relative] = _read_bounded(path, total)
    courses: dict[str, dict[str, bytes]] = {}
    for course in ATTESTATION_COURSES:
        course_root = root / course
        files = _fixture_files(course_root)
        expected_files = ATTESTATION_COURSE_ARTIFACTS[course]
        if set(files) != expected_files:
            raise ContractError("course evidence file inventory mismatch")
        expected_directories = {
            PurePosixPath(relative).parent.as_posix()
            for relative in expected_files
            if PurePosixPath(relative).parent.as_posix() != "."
        }
        actual_directories = {
            path.relative_to(course_root).as_posix()
            for path in course_root.rglob("*")
            if path.is_dir()
        }
        if actual_directories != expected_directories:
            raise ContractError("course evidence directory inventory mismatch")
        records = {
            relative: _read_bounded(files[relative], total)
            for relative in sorted(expected_files)
        }
        for relative, payload in records.items():
            if canonical_json(_parse_json(payload)) != payload:
                raise ContractError("course evidence encoding mismatch")
        courses[course] = records
    return courses, authority_records


def _assemble_course_fixture(
    courses: Mapping[str, Mapping[str, bytes]],
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
    artifacts["manifest.json"] = canonical_json(
        build_manifest(sorted(artifacts.items()))
    )
    return artifacts


def _course_outcome_interval(
    course_records: Mapping[str, bytes],
    outcome: str,
) -> int:
    relative = f"outcomes/{outcome}.json"
    payload = course_records.get(relative)
    if not isinstance(payload, bytes):
        raise ContractError("slow-down interval evidence is missing")
    document = _parse_json(payload)
    _validate_artifact_for_path(relative, document, require_real=False)
    if not isinstance(document, dict) or document.get("outcome") != outcome:
        raise ContractError("slow-down interval evidence identity mismatch")
    projection = _require_exact_keys(
        document.get("projection"),
        {"observed_fields", "omitted_transient_fields", "values"},
        "slow-down interval evidence projection",
    )
    values = projection["values"]
    return _require_positive_int(
        values.get("interval") if isinstance(values, dict) else None,
        "slow-down course interval",
        300,
    )


def _validate_slow_down_interval_progression(
    courses: Mapping[str, Mapping[str, bytes]],
) -> None:
    slow_course = courses.get("slow-down")
    if not isinstance(slow_course, Mapping):
        raise ContractError("slow-down course evidence is missing")
    device_interval = _course_outcome_interval(
        slow_course,
        "device_code_success",
    )
    response_interval = _course_outcome_interval(slow_course, "slow_down")
    if response_interval != device_interval + 5:
        raise ContractError("slow-down interval progression mismatch")


def _validate_course_evidence(
    root: Path,
    fixture_payloads: Mapping[str, bytes],
) -> tuple[
    dict[str, dict[str, bytes]],
    dict[str, bytes],
    dict[str, dict[str, object]],
    dict[str, str],
    str,
]:
    courses, authority_records = _read_course_evidence_root(root)
    _, _, runner_digest = _current_capture_runner()
    registrations = {records["registration.json"] for records in courses.values()}
    if len(registrations) != 1:
        raise ContractError("course registration bytes mismatch")
    registration_payload = next(iter(registrations))
    registration = _parse_json(registration_payload)
    validate_registration_artifact(registration, require_real=False)
    if (
        not isinstance(registration, dict)
        or registration.get("public_client_id") != APPROVED_PUBLIC_CLIENT_ID
    ):
        raise ContractError("course registration identity mismatch")
    registration_digest = _sha256(registration_payload)
    executions: dict[str, dict[str, object]] = {}
    course_digests: dict[str, str] = {}
    for course, records in courses.items():
        execution = _validate_course_execution(
            course,
            _parse_json(records["course-execution.json"]),
            runner_digest=runner_digest,
            registration_digest=registration_digest,
        )
        executions[course] = execution
        executed_at = execution["executed_at_utc"]
        if not isinstance(executed_at, str):
            raise ContractError("course execution timestamp mismatch")
        for relative, payload in records.items():
            if relative in {"course-execution.json", "registration.json"}:
                continue
            if relative == "device-flow-transition.json":
                _validate_course_transition(payload, executed_at_utc=executed_at)
                bindings = execution["bindings"]
                if (
                    not isinstance(bindings, dict)
                    or bindings["device_flow_transition_sha256"] != _sha256(payload)
                ):
                    raise ContractError("course transition digest mismatch")
                continue
            document = _parse_json(payload)
            _validate_artifact_for_path(relative, document, require_real=False)
            if not isinstance(document, dict):
                raise ContractError("course artifact schema mismatch")
            provenance = document.get("provenance")
            if (
                not isinstance(provenance, dict)
                or provenance.get("registration_record_sha256")
                != registration_digest
                or provenance.get("captured_at_utc") != executed_at
            ):
                raise ContractError("course artifact execution binding mismatch")
        course_digests[course] = _course_tree_digest(records)
    _, original_authorized_at = _validate_slow_down_authority(
        authority_records[SLOW_DOWN_AUTHORITY_PATH]
    )
    slow_execution = executions["slow-down"]
    slow_bindings = slow_execution["bindings"]
    slow_executed_at = slow_execution["executed_at_utc"]
    if not isinstance(slow_bindings, dict) or not isinstance(slow_executed_at, str):
        raise ContractError("slow-down execution binding mismatch")
    failed_runner_digest, original_spent_at = _validate_slow_down_spent_marker(
        authority_records[SLOW_DOWN_SPENT_PATH],
        authority_payload=authority_records[SLOW_DOWN_AUTHORITY_PATH],
        reservation_digest=slow_bindings[
            "slow_down_original_reservation_sha256"
        ],
        authorized_at=original_authorized_at,
        executed_at_utc=slow_executed_at,
    )
    recovery_authorized_at = _validate_slow_down_recovery_authority(
        authority_records[SLOW_DOWN_RECOVERY_AUTHORITY_PATH],
        original_authority_payload=authority_records[SLOW_DOWN_AUTHORITY_PATH],
        original_marker_payload=authority_records[SLOW_DOWN_SPENT_PATH],
        failed_runner_digest=failed_runner_digest,
        runner_digest=runner_digest,
        original_spent_at=original_spent_at,
    )
    _validate_slow_down_recovery_spent_marker(
        authority_records[SLOW_DOWN_RECOVERY_SPENT_PATH],
        authority_payload=authority_records[SLOW_DOWN_RECOVERY_AUTHORITY_PATH],
        original_authority_payload=authority_records[SLOW_DOWN_AUTHORITY_PATH],
        original_marker_payload=authority_records[SLOW_DOWN_SPENT_PATH],
        runner_digest=runner_digest,
        reservation_digest=slow_bindings[
            "slow_down_recovery_reservation_sha256"
        ],
        authorized_at=recovery_authorized_at,
        executed_at_utc=slow_executed_at,
    )
    _validate_slow_down_interval_progression(courses)
    assembled = _assemble_course_fixture(courses)
    if not _matches_exact(assembled, dict(fixture_payloads)):
        raise ContractError("course evidence assembly mismatch")
    return courses, authority_records, executions, course_digests, runner_digest


def _validate_live_run_attestation(
    payload: bytes,
    fixture_payloads: Mapping[str, bytes],
    *,
    executions: Mapping[str, Mapping[str, object]],
    course_digests: Mapping[str, str],
    runner_digest: str,
) -> None:
    document = _require_exact_keys(
        _parse_json(payload),
        {
            "artifacts",
            "authority_amendment_id",
            "authority_subject_sha256",
            "capture_runner_sha256",
            "course_execution",
            "course_tree_sha256",
            "schema_name",
            "schema_version",
            "state",
        },
        "live-run attestation",
    )
    if canonical_json(document) != payload:
        raise ContractError("live-run attestation encoding is not canonical")
    if (
        document["authority_amendment_id"] != AUTHORITY_AMENDMENT_ID
        or document["authority_subject_sha256"] != AUTHORITY_SUBJECT_SHA256
        or document["schema_name"] != "bpr.rfc0016.live_run_attestation"
        or type(document["schema_version"]) is not int
        or document["schema_version"] != SCHEMA_VERSION
        or document["state"] != "attended_runner_completed_unattested"
        or document["capture_runner_sha256"] != runner_digest
    ):
        raise ContractError("live-run attestation identity mismatch")
    expected_artifacts = [
        {"path": relative, "sha256": _sha256(fixture_payloads[relative])}
        for relative in sorted(fixture_payloads)
    ]
    if document["artifacts"] != expected_artifacts:
        raise ContractError("live-run attestation artifact binding mismatch")
    attested_executions = _require_exact_keys(
        document["course_execution"],
        set(ATTESTATION_COURSES),
        "live-run course execution",
    )
    attested_course_trees = _require_exact_keys(
        document["course_tree_sha256"],
        set(ATTESTATION_COURSES),
        "live-run course trees",
    )
    expected_executions = {
        course: {
            "bindings": executions[course]["bindings"],
            "capture_runner_sha256": runner_digest,
            "executed_at_utc": executions[course]["executed_at_utc"],
        }
        for course in ATTESTATION_COURSES
    }
    _require_exact_value(
        attested_executions,
        expected_executions,
        "live-run course execution",
    )
    _require_exact_value(
        attested_course_trees,
        dict(course_digests),
        "live-run course tree",
    )
    _validate_persisted_security(document)


def _expected_review_identity(
    fixture_payloads: Mapping[str, bytes],
    course_records: Mapping[str, Mapping[str, bytes]],
    authority_records: Mapping[str, bytes],
    attestation_payload: bytes,
) -> list[tuple[str, str, bytes]]:
    fixture_identity = [
        (f"fixture:{relative}", _sha256(payload), payload)
        for relative, payload in sorted(fixture_payloads.items())
    ]
    course_identity = [
        (f"course:{course}/{relative}", _sha256(payload), payload)
        for course, records in sorted(course_records.items())
        for relative, payload in sorted(records.items())
    ]
    authority_identity = [
        (f"authority:{relative}", _sha256(authority_records[relative]), authority_records[relative])
        for relative in SLOW_DOWN_AUTHORITY_RECORD_PATHS
    ]
    return fixture_identity + course_identity + authority_identity + [
        (
            "attestation:live-run-attestation.json",
            _sha256(attestation_payload),
            attestation_payload,
        )
    ]


def _validate_review_input(
    payload: bytes,
    *,
    review_dir: Path,
    repository_root: Path,
    expected_identity: Sequence[tuple[str, str, bytes]],
) -> list[dict[str, str]]:
    source = _require_exact_keys(
        _parse_json(payload),
        {
            "downstream_claims",
            "project_id",
            "reference_disclaimer",
            "review_gate",
            "review_scope",
            "schema_name",
            "schema_version",
            "subjects",
            "title",
        },
        "attended review input",
    )
    subjects = source.get("subjects")
    if not isinstance(subjects, list) or len(subjects) != 1:
        raise ContractError("attended review input subject mismatch")
    raw_subject = _require_exact_keys(
        subjects[0],
        {
            "ab_group",
            "acceptance_criteria",
            "audit_verdict",
            "choice_group",
            "description",
            "detail_sections",
            "display_name",
            "identity_files",
            "known_concerns",
            "media_groups",
            "subject_id",
            "subject_key",
        },
        "attended review input subject",
    )
    raw_identity = raw_subject.get("identity_files")
    if not isinstance(raw_identity, list) or len(raw_identity) != len(expected_identity):
        raise ContractError("attended review input identity inventory mismatch")
    normalized_identity: list[dict[str, str]] = []
    for item, (expected_label, _, expected_payload) in zip(
        raw_identity,
        expected_identity,
        strict=True,
    ):
        entry = _require_exact_keys(
            item,
            {"label", "path"},
            "attended review input identity",
        )
        if type(entry["label"]) is not str or entry["label"] != expected_label:
            raise ContractError("attended review input identity label mismatch")
        if type(entry["path"]) is not str or not entry["path"]:
            raise ContractError("attended review input identity path mismatch")
        source_path = Path(entry["path"]).expanduser()
        if source_path.is_absolute():
            raise ContractError("attended review input identity path is not portable")
        source_path = review_dir / source_path
        try:
            resolved = source_path.resolve(strict=True)
            relative_source = resolved.relative_to(repository_root)
        except (OSError, ValueError) as error:
            raise ContractError("attended review input identity escapes repository") from error
        cursor = repository_root
        for component in relative_source.parts:
            cursor /= component
            if cursor.is_symlink():
                raise ContractError("attended review input identity contains a link")
        if not source_path.is_file() or source_path.is_symlink():
            raise ContractError("attended review input identity is not a regular file")
        if resolved.stat().st_size > MAX_JSON_BYTES:
            raise ContractError("attended review input identity exceeds size limit")
        if resolved.read_bytes() != expected_payload:
            raise ContractError("attended review input identity byte mismatch")
        normalized_identity.append(
            {"label": expected_label, "path": entry["path"]}
        )
    expected_source = {
        "downstream_claims": ATTESTATION_DOWNSTREAM_CLAIMS,
        "project_id": "betterpullreview",
        "reference_disclaimer": ATTESTATION_REFERENCE_DISCLAIMER,
        "review_gate": ATTESTATION_REVIEW_GATE,
        "review_scope": ATTESTATION_REVIEW_SCOPE,
        "schema_name": "bpr.attended_review_batch_input",
        "schema_version": 1,
        "subjects": [
            {
                "ab_group": None,
                "acceptance_criteria": ATTESTATION_ACCEPTANCE_CRITERIA,
                "audit_verdict": ATTESTATION_AUDIT,
                "choice_group": None,
                "description": ATTESTATION_DESCRIPTION,
                "detail_sections": [],
                "display_name": ATTESTATION_DISPLAY_NAME,
                "identity_files": normalized_identity,
                "known_concerns": ATTESTATION_KNOWN_CONCERNS,
                "media_groups": [],
                "subject_id": ATTESTATION_SUBJECT_ID,
                "subject_key": ATTESTATION_SUBJECT_KEY,
            }
        ],
        "title": ATTESTATION_REVIEW_TITLE,
    }
    _require_exact_value(source, expected_source, "attended review input")
    return normalized_identity


def _load_attestation_review(
    review_dir: Path,
) -> tuple[Path, PurePosixPath, dict[str, Path], dict[str, bytes]]:
    repository_root = REPOSITORY_ROOT.resolve(strict=True)
    try:
        review_resolved = review_dir.resolve(strict=True)
        relative_review = review_resolved.relative_to(repository_root)
    except (OSError, ValueError) as error:
        raise ContractError("attestation review is not repository-recorded") from error
    if (
        review_dir.absolute() != review_resolved
        or tuple(relative_review.parts[:3]) != ("docs", "verification", "reviews")
        or len(relative_review.parts) != 4
        or review_dir.is_symlink()
        or not review_dir.is_dir()
    ):
        raise ContractError("attestation review is not repository-recorded")
    review_files = _fixture_files(review_dir)
    review_total = [0]
    review_payloads = {
        relative: _read_bounded(path, review_total)
        for relative, path in review_files.items()
    }
    required = {
        ATTESTATION_INPUT_PATH,
        ATTESTATION_PAGE_PATH,
        ATTESTATION_MANIFEST_PATH,
        ATTESTATION_VERDICT_PATH,
        ATTESTATION_VALIDATION_PATH,
    }
    if not required <= set(review_files):
        raise ContractError("attestation review is incomplete")
    return repository_root, relative_review, review_files, review_payloads


def _parse_attestation_manifest(
    payload: bytes,
) -> tuple[dict[str, object], dict[str, object], list[object]]:
    manifest = _require_exact_keys(
        _parse_json(payload),
        {
            "batch_id",
            "downstream_claims",
            "project_id",
            "reference_disclaimer",
            "review_gate",
            "review_scope",
            "schema_name",
            "schema_version",
            "subjects",
            "title",
        },
        "attended review manifest",
    )
    subjects = manifest["subjects"]
    if not isinstance(subjects, list) or len(subjects) != 1:
        raise ContractError("attended review subject inventory mismatch")
    subject = _require_exact_keys(
        subjects[0],
        {
            "ab_group",
            "acceptance_criteria",
            "audit_verdict",
            "choice_group",
            "description",
            "detail_sections",
            "display_name",
            "identity_files",
            "known_concerns",
            "media_groups",
            "subject_digest",
            "subject_id",
            "subject_key",
        },
        "attended review subject",
    )
    identity_files = subject["identity_files"]
    if not isinstance(identity_files, list):
        raise ContractError("attended review identity inventory mismatch")
    return manifest, subject, identity_files


def _review_identity_payload(
    value: object,
    *,
    review_dir: Path,
    review_payloads: Mapping[str, bytes],
) -> tuple[dict[str, object], PurePosixPath, bytes]:
    identity = _require_exact_keys(
        value,
        {"label", "media_kind", "path", "sha256"},
        "attended review identity",
    )
    path_value = identity["path"]
    if not isinstance(path_value, str):
        raise ContractError("attended review identity path is invalid")
    relative = PurePosixPath(path_value)
    if relative.is_absolute() or ".." in relative.parts:
        raise ContractError("attended review identity path escapes package")
    path = review_dir / "offline-package" / Path(*relative.parts)
    try:
        resolved = path.resolve(strict=True)
    except OSError as error:
        raise ContractError("attended review identity is missing") from error
    package_path = f"offline-package/{relative.as_posix()}"
    payload = review_payloads.get(package_path)
    if (
        payload is None
        or not path.is_file()
        or path.is_symlink()
        or path.absolute() != resolved
    ):
        raise ContractError("attended review identity is invalid")
    return identity, relative, payload


def _live_attestation_payload(
    identity_files: Sequence[object],
    *,
    review_dir: Path,
    review_payloads: Mapping[str, bytes],
    fixture_payloads: Mapping[str, bytes],
    executions: Mapping[str, Mapping[str, object]],
    course_digests: Mapping[str, str],
    runner_digest: str,
) -> bytes:
    attestation_entries = [
        item
        for item in identity_files
        if isinstance(item, dict)
        and item.get("label") == "attestation:live-run-attestation.json"
    ]
    if len(attestation_entries) != 1:
        raise ContractError("live-run attestation identity is missing")
    _, _, attestation_payload = _review_identity_payload(
        attestation_entries[0],
        review_dir=review_dir,
        review_payloads=review_payloads,
    )
    _validate_live_run_attestation(
        attestation_payload,
        fixture_payloads,
        executions=executions,
        course_digests=course_digests,
        runner_digest=runner_digest,
    )
    return attestation_payload


def _validate_review_identities(
    identity_files: Sequence[object],
    *,
    expected_identity: Sequence[tuple[str, str, bytes]],
    review_dir: Path,
    review_payloads: Mapping[str, bytes],
) -> tuple[set[str], list[dict[str, str]]]:
    if len(identity_files) != len(expected_identity):
        raise ContractError("attended review identity inventory mismatch")
    package_identity_paths: set[str] = set()
    digest_identity: list[dict[str, str]] = []
    for item, (expected_label, expected_digest, expected_payload) in zip(
        identity_files,
        expected_identity,
        strict=True,
    ):
        identity, relative, payload = _review_identity_payload(
            item,
            review_dir=review_dir,
            review_payloads=review_payloads,
        )
        if (
            identity["label"] != expected_label
            or identity["media_kind"] != "file"
            or identity["sha256"] != expected_digest
        ):
            raise ContractError("attended review identity mismatch")
        if payload != expected_payload:
            raise ContractError("attended review identity byte mismatch")
        package_identity_paths.add(f"offline-package/{relative.as_posix()}")
        digest_identity.append(
            {
                "label": expected_label,
                "media_kind": "file",
                "sha256": expected_digest,
            }
        )
    return package_identity_paths, digest_identity


def _validate_review_manifest_identity(
    manifest: Mapping[str, object],
    subject: Mapping[str, object],
    identity_files: Sequence[object],
    digest_identity: Sequence[Mapping[str, str]],
) -> tuple[str, str, dict[str, object], dict[str, object]]:
    digest_input = {
        "ab_group": "",
        "acceptance_criteria": ATTESTATION_ACCEPTANCE_CRITERIA,
        "audit_verdict": ATTESTATION_AUDIT,
        "choice_group": "",
        "description": ATTESTATION_DESCRIPTION,
        "detail_sections": [],
        "display_name": ATTESTATION_DISPLAY_NAME,
        "identity_files": list(digest_identity),
        "known_concerns": ATTESTATION_KNOWN_CONCERNS,
        "media_groups": [],
        "subject_id": ATTESTATION_SUBJECT_ID,
        "subject_key": ATTESTATION_SUBJECT_KEY,
    }
    subject_digest = subject.get("subject_digest")
    if type(subject_digest) is not str or not SHA256.fullmatch(subject_digest):
        raise ContractError("attended review subject digest mismatch")
    expected_subject = {
        **digest_input,
        "identity_files": list(identity_files),
        "subject_digest": subject_digest,
    }
    _require_exact_value(
        subject,
        expected_subject,
        "attended review subject identity",
    )
    batch_identity = {
        "downstream_claims": ATTESTATION_DOWNSTREAM_CLAIMS,
        "project_id": "betterpullreview",
        "reference_disclaimer": ATTESTATION_REFERENCE_DISCLAIMER,
        "review_gate": ATTESTATION_REVIEW_GATE,
        "review_scope": ATTESTATION_REVIEW_SCOPE,
        "subjects": [
            {
                "subject_digest": subject_digest,
                "subject_id": ATTESTATION_SUBJECT_ID,
            }
        ],
        "title": ATTESTATION_REVIEW_TITLE,
    }
    batch_id = manifest.get("batch_id")
    if (
        type(batch_id) is not str
        or not re.fullmatch(
            r"bpr:/attended-review/betterpullreview/[0-9a-f]{24}",
            batch_id,
        )
    ):
        raise ContractError("attended review batch identity mismatch")
    expected_manifest = {
        **batch_identity,
        "batch_id": batch_id,
        "schema_name": "bpr.attended_review_batch",
        "schema_version": 1,
        "subjects": [subject],
    }
    _require_exact_value(
        manifest,
        expected_manifest,
        "attended review manifest identity",
    )
    return subject_digest, batch_id, digest_input, batch_identity


def _validate_review_verdict(
    payload: bytes,
    *,
    subject_digest: str,
    batch_id: str,
) -> dict[str, object]:
    verdict = _require_exact_keys(
        _parse_json(payload),
        {
            "accepted_subjects",
            "batch_id",
            "downstream_claims",
            "notes_by_subject_identity",
            "rejected_subjects",
            "review_gate",
            "review_scope",
            "schema_name",
            "schema_version",
        },
        "attended review verdict",
    )
    accepted_identity = {
        "subject_digest": subject_digest,
        "subject_id": ATTESTATION_SUBJECT_ID,
    }
    notes = verdict["notes_by_subject_identity"]
    note_key = f"{ATTESTATION_SUBJECT_ID}@{subject_digest}"
    if (
        verdict["schema_name"] != "bpr.attended_review_verdict"
        or type(verdict["schema_version"]) is not int
        or verdict["schema_version"] != 1
        or verdict["batch_id"] != batch_id
        or verdict["review_gate"] != ATTESTATION_REVIEW_GATE
        or verdict["review_scope"] != ATTESTATION_REVIEW_SCOPE
        or not _matches_exact(
            verdict["downstream_claims"],
            ATTESTATION_DOWNSTREAM_CLAIMS,
        )
        or not _matches_exact(verdict["accepted_subjects"], [accepted_identity])
        or not _matches_exact(verdict["rejected_subjects"], [])
        or not isinstance(notes, dict)
        or set(notes) - {note_key}
        or any(not isinstance(note, str) for note in notes.values())
    ):
        raise ContractError("attended review verdict is not an exact acceptance")
    return verdict


def _validate_review_record(
    payload: bytes,
    *,
    repository_root: Path,
    relative_review: PurePosixPath,
    batch_id: str,
    runner_digest: str,
) -> tuple[bytes, Path]:
    validation = _require_exact_keys(
        _parse_json(payload),
        {
            "batch_id",
            "capture_runner",
            "claims",
            "invocation",
            "manifest",
            "official_validator",
            "outcome",
            "recorded_at",
            "result",
            "review_input",
            "review_page",
            "review_gate",
            "review_scope",
            "schema_name",
            "schema_version",
            "verdict",
        },
        "attended review validation record",
    )
    review_prefix = relative_review.as_posix()
    expected_manifest_path = f"{review_prefix}/{ATTESTATION_MANIFEST_PATH}"
    expected_verdict_path = f"{review_prefix}/{ATTESTATION_VERDICT_PATH}"
    expected_input_path = f"{review_prefix}/{ATTESTATION_INPUT_PATH}"
    expected_page_path = f"{review_prefix}/{ATTESTATION_PAGE_PATH}"
    validator_binding = validation["official_validator"]
    if not isinstance(validator_binding, dict):
        raise ContractError("attended validator binding must be an object")
    recorded_validator_path = validator_binding.get("path")
    if recorded_validator_path not in (ATTENDED_VALIDATOR_PATH, HISTORICAL_ATTENDED_VALIDATOR_PATH):
        raise ContractError("attended validator source path mismatch")
    expected_invocation = (
        f"python3 {recorded_validator_path} validate-verdict "
        f"--manifest {expected_manifest_path} --verdict {expected_verdict_path}"
    )
    if (
        validation["schema_name"]
        != "bpr.attended_review_validation_record"
        or type(validation["schema_version"]) is not int
        or validation["schema_version"] != 1
        or validation["batch_id"] != batch_id
        or validation["review_gate"] != ATTESTATION_REVIEW_GATE
        or validation["review_scope"] != ATTESTATION_REVIEW_SCOPE
        or validation["invocation"] != expected_invocation
        or validation["result"] != "valid complete verdict"
        or validation["outcome"] != "all_subjects_accepted"
        or not _matches_exact(
            validation["claims"],
            ATTESTATION_VALIDATION_CLAIMS,
        )
        or not isinstance(validation["recorded_at"], str)
    ):
        raise ContractError("attended review validation record mismatch")
    try:
        recorded_at = datetime.fromisoformat(validation["recorded_at"].replace("Z", "+00:00"))
    except ValueError as error:
        raise ContractError("attended review validation timestamp is invalid") from error
    if recorded_at.tzinfo is None or recorded_at.utcoffset() is None:
        raise ContractError("attended review validation timestamp is invalid")
    for field, expected_path in (
        ("manifest", expected_manifest_path),
        ("verdict", expected_verdict_path),
        ("review_input", expected_input_path),
    ):
        _review_artifact_triple(
            validation[field],
            expected_path=expected_path,
            repository_root=repository_root,
        )
    runner_payload = _review_artifact_triple(
        validation["capture_runner"],
        expected_path=CAPTURE_RUNNER_PATH,
        repository_root=repository_root,
    )
    if _sha256(runner_payload) != runner_digest:
        raise ContractError("attended capture runner binding mismatch")
    page_payload = _review_artifact_triple(
        validation["review_page"],
        expected_path=expected_page_path,
        repository_root=repository_root,
    )
    _review_artifact_triple(
        validation["official_validator"],
        expected_path=recorded_validator_path,
        repository_root=repository_root,
    )
    validator_path = repository_root / ATTENDED_VALIDATOR_PATH
    return page_payload, validator_path


def _load_attended_validator(validator_path: Path) -> object:
    spec = importlib.util.spec_from_file_location(
        "rfc0016_attended_verdict_validator",
        validator_path,
    )
    if spec is None or spec.loader is None:
        raise ContractError("official attended validator is unavailable")
    validator = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(validator)
    except Exception as error:
        raise ContractError("official attended validator is unavailable") from error
    return validator


def _validate_official_review(
    validator: object,
    *,
    manifest: Mapping[str, object],
    verdict: Mapping[str, object],
    digest_input: Mapping[str, object],
    batch_identity: Mapping[str, object],
    subject_digest: str,
    batch_id: str,
    source_identity: Sequence[Mapping[str, str]],
    identity_files: Sequence[object],
    page_payload: bytes,
    validator_path: Path,
) -> None:
    try:
        validator.validate_verdict(manifest, verdict)
    except Exception as error:
        raise ContractError("official attended verdict validation failed") from error
    _validate_official_review_identity(
        validator,
        digest_input=digest_input,
        batch_identity=batch_identity,
        subject_digest=subject_digest,
        batch_id=batch_id,
        source_identity=source_identity,
        identity_files=identity_files,
    )
    _validate_review_page(
        validator,
        manifest=manifest,
        page_payload=page_payload,
        validator_path=validator_path,
    )


def _validate_official_review_identity(
    validator: object,
    *,
    digest_input: Mapping[str, object],
    batch_identity: Mapping[str, object],
    subject_digest: str,
    batch_id: str,
    source_identity: Sequence[Mapping[str, str]],
    identity_files: Sequence[object],
) -> None:
    try:
        expected_subject_digest = validator.sha256_bytes(
            validator.canonical_bytes(digest_input)
        )
        expected_batch_id = (
            "bpr:/attended-review/betterpullreview/"
            + hashlib.sha256(
                validator.canonical_bytes(batch_identity)
            ).hexdigest()[:24]
        )
    except Exception as error:
        raise ContractError("official attended identity calculation failed") from error
    if subject_digest != expected_subject_digest or batch_id != expected_batch_id:
        raise ContractError("official attended identity calculation mismatch")
    expected_subject_dir = f"001-{validator.slug(ATTESTATION_SUBJECT_KEY, 'subject')}"
    for index, (source, identity) in enumerate(
        zip(source_identity, identity_files, strict=True),
        start=1,
    ):
        extension = Path(source["path"]).suffix.lower()
        expected_path = (
            f"subjects/{expected_subject_dir}/identity/"
            f"{index:02d}-{validator.slug(source['label'], 'item')}{extension}"
        )
        if identity["path"] != expected_path:
            raise ContractError("official attended identity path mismatch")


def _validate_review_page(
    validator: object,
    *,
    manifest: Mapping[str, object],
    page_payload: bytes,
    validator_path: Path,
) -> None:
    skill_root = validator_path.parent.parent
    try:
        expected_page = validator.render_html(
            manifest,
            (skill_root / "assets" / "attended_review.css").read_text(
                encoding="utf-8"
            ),
            (skill_root / "assets" / "attended_review_runtime.js").read_text(
                encoding="utf-8"
            ),
        ).encode("utf-8")
    except Exception as error:
        raise ContractError("official attended review page rebuild failed") from error
    if page_payload != expected_page:
        raise ContractError("attended review page does not match official rebuild")


def _validate_attestation_review(
    review_dir: Path,
    fixture_payloads: Mapping[str, bytes],
    course_evidence_root: Path,
) -> None:
    (
        repository_root,
        relative_review,
        review_files,
        review_payloads,
    ) = _load_attestation_review(review_dir)
    (
        course_records,
        authority_records,
        executions,
        course_digests,
        runner_digest,
    ) = _validate_course_evidence(course_evidence_root, fixture_payloads)
    manifest, subject, identity_files = _parse_attestation_manifest(
        review_payloads[ATTESTATION_MANIFEST_PATH]
    )
    attestation_payload = _live_attestation_payload(
        identity_files,
        review_dir=review_dir,
        review_payloads=review_payloads,
        fixture_payloads=fixture_payloads,
        executions=executions,
        course_digests=course_digests,
        runner_digest=runner_digest,
    )
    expected_identity = _expected_review_identity(
        fixture_payloads,
        course_records,
        authority_records,
        attestation_payload,
    )
    source_identity = _validate_review_input(
        review_payloads[ATTESTATION_INPUT_PATH],
        review_dir=review_dir,
        repository_root=repository_root,
        expected_identity=expected_identity,
    )
    package_paths, digest_identity = _validate_review_identities(
        identity_files,
        expected_identity=expected_identity,
        review_dir=review_dir,
        review_payloads=review_payloads,
    )
    expected_files = {
        ATTESTATION_INPUT_PATH,
        ATTESTATION_PAGE_PATH,
        ATTESTATION_MANIFEST_PATH,
        ATTESTATION_VERDICT_PATH,
        ATTESTATION_VALIDATION_PATH,
        *package_paths,
    }
    if set(review_files) != expected_files:
        raise ContractError("attestation review file inventory mismatch")
    (
        subject_digest,
        batch_id,
        digest_input,
        batch_identity,
    ) = _validate_review_manifest_identity(
        manifest,
        subject,
        identity_files,
        digest_identity,
    )
    verdict = _validate_review_verdict(
        review_payloads[ATTESTATION_VERDICT_PATH],
        subject_digest=subject_digest,
        batch_id=batch_id,
    )
    page_payload, validator_path = _validate_review_record(
        review_payloads[ATTESTATION_VALIDATION_PATH],
        repository_root=repository_root,
        relative_review=relative_review,
        batch_id=batch_id,
        runner_digest=runner_digest,
    )
    validator = _load_attended_validator(validator_path)
    _validate_official_review(
        validator,
        manifest=manifest,
        verdict=verdict,
        digest_input=digest_input,
        batch_identity=batch_identity,
        subject_digest=subject_digest,
        batch_id=batch_id,
        source_identity=source_identity,
        identity_files=identity_files,
        page_payload=page_payload,
        validator_path=validator_path,
    )


def validate_fixture_root(
    root: Path,
    *,
    require_complete: bool = False,
    attestation_review: Path | None = None,
    course_evidence_root: Path | None = None,
) -> str:
    """Validate a pending or unattested fixture tree without writing."""
    files = _fixture_files(root)
    total = [0]
    if set(files) == {"pending.json"}:
        payload = _read_bounded(files["pending.json"], total)
        pending = _parse_json(payload)
        expected = {
            "authority_subject_sha256": AUTHORITY_SUBJECT_SHA256,
            "schema_name": "bpr.rfc0016.device_capture_state",
            "schema_version": SCHEMA_VERSION,
            "state": "pending_real_capture",
        }
        if not _matches_exact(pending, expected) or canonical_json(pending) != payload:
            raise ContractError("pending fixture state mismatch")
        _validate_persisted_security(pending)
        if (
            require_complete
            or attestation_review is not None
            or course_evidence_root is not None
        ):
            raise ContractError("real capture is still pending")
        return "pending_real_capture"

    expected_files = {"manifest.json", *COMPLETE_ARTIFACTS}
    if set(files) != expected_files:
        raise ContractError("fixture tree is not exact")
    payloads = {
        relative: _read_bounded(files[relative], total)
        for relative in sorted(expected_files)
    }
    documents = {relative: _parse_json(payload) for relative, payload in payloads.items()}
    for relative, document in documents.items():
        if canonical_json(document) != payloads[relative]:
            raise ContractError("fixture encoding is not canonical")
    artifact_payloads = {relative: payloads[relative] for relative in COMPLETE_ARTIFACTS}
    _validate_manifest(
        documents["manifest.json"],
        artifact_payloads,
        require_real=False,
    )
    manifest = documents["manifest.json"]
    if not isinstance(manifest, dict) or manifest.get("state") != "unattested_capture":
        raise ContractError("durable fixture capture is not admissible")
    if attestation_review is not None:
        if not require_complete or course_evidence_root is None:
            raise ContractError("attestation review requires complete validation")
        _validate_attestation_review(
            attestation_review,
            payloads,
            course_evidence_root,
        )
        return "attended_human_accepted_capture"
    if require_complete or course_evidence_root is not None:
        raise ContractError("external capture attestation is required")
    return "unattested_capture"


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    validate = subparsers.add_parser("validate", help="validate an offline fixture root")
    validate.add_argument("--fixture-root", type=Path, required=True)
    validate.add_argument("--require-complete", action="store_true")
    validate.add_argument("--attestation-review", type=Path)
    validate.add_argument("--course-evidence-root", type=Path)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    try:
        state = validate_fixture_root(
            arguments.fixture_root,
            require_complete=arguments.require_complete,
            attestation_review=arguments.attestation_review,
            course_evidence_root=arguments.course_evidence_root,
        )
    except (ContractError, OSError):
        print("RFC0016_DEVICE_CAPTURE_INVALID", file=sys.stderr)
        return 1
    print(f"RFC0016_DEVICE_CAPTURE_VALID:{state}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
