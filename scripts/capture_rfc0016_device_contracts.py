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
    "slow_down": ["error", "error_description", "error_uri"],
}

GRAPHQL_DOCUMENT = (
    "query GetReviewData($owner:String!,$name:String!,$number:Int!){"
    "repository(owner:$owner,name:$name){id pullRequest(number:$number){id "
    "files(first:100){totalCount nodes{path viewerViewedState} "
    "pageInfo{hasNextPage endCursor}} reviewThreads(first:100){totalCount "
    "nodes{id isResolved isOutdated viewerCanResolve viewerCanUnresolve "
    "subjectType path line originalLine comments(first:100){totalCount "
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
        if artifact[key] != expected_value:
            raise ContractError("registration authority mismatch")
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
        or artifact["schema_version"] != SCHEMA_VERSION
        or artifact["graphql_document"] != GRAPHQL_DOCUMENT
        or artifact["graphql_document_sha256"] != _sha256(GRAPHQL_DOCUMENT.encode("utf-8"))
        or artifact["graphql_variables"] != GRAPHQL_VARIABLES
        or artifact["repository"]
        != {
            "database_id": 1328277631,
            "name": "BPR-Test",
            "owner": "MikeMarcin",
            "pull_request_number": 1,
        }
        or artifact["probes"] != expected_route_probes()
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
    if manifest != expected:
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


def validate_fixture_root(root: Path, *, require_complete: bool = False) -> str:
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
        if pending != expected or canonical_json(pending) != payload:
            raise ContractError("pending fixture state mismatch")
        _validate_persisted_security(pending)
        if require_complete:
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
    if require_complete:
        raise ContractError("external capture attestation is required")
    return "unattested_capture"


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    validate = subparsers.add_parser("validate", help="validate an offline fixture root")
    validate.add_argument("--fixture-root", type=Path, required=True)
    validate.add_argument("--require-complete", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    try:
        state = validate_fixture_root(
            arguments.fixture_root,
            require_complete=arguments.require_complete,
        )
    except (ContractError, OSError):
        print("RFC0016_DEVICE_CAPTURE_INVALID", file=sys.stderr)
        return 1
    print(f"RFC0016_DEVICE_CAPTURE_VALID:{state}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
