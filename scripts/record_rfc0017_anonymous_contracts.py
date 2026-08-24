#!/usr/bin/env python3
"""Record sanitized anonymous GitHub service contracts for RFC-BPR-0017.

BPR-0405 authors this; BPR-0298 executes it. The split matters: the live
capture is the only thing here that touches a network, and it is the only thing
this module refuses to do without a digest-bound authority the operator signed.

Two modes:

* default — validate the authority, then perform exactly the requests the
  authority names, sanitize each response, and write it under the output root.
* ``--validate-existing`` — perform no traffic at all and revalidate every
  packet, response, manifest, capture digest, and minimization record.

The module deliberately holds no network client at import time. ``urllib`` is
imported inside the one function that issues a request, so that importing this
module for validation cannot reach the network even by mistake.
"""
from __future__ import annotations

import argparse
import hashlib
import ipaddress
import json
import os
import re
import sys
from pathlib import Path
from typing import Any
from urllib.parse import urlsplit

# Mirrors scripts/verify_documentation.py. Keep this block free of syntax newer
# than the version it refuses, or it cannot report anything.
MINIMUM_PYTHON = (3, 10)
if sys.version_info < MINIMUM_PYTHON:
    sys.stderr.write(
        "record_rfc0017_anonymous_contracts.py requires Python {}.{} or newer; "
        "this is {}.{}.{} at {}.\n".format(
            MINIMUM_PYTHON[0], MINIMUM_PYTHON[1], sys.version_info[0],
            sys.version_info[1], sys.version_info[2], sys.executable))
    raise SystemExit(2)


class ContractError(Exception):
    """A refusal. Every one of these leaves the tree unchanged."""


# --------------------------------------------------------------------------
# Outer bounds. The authority names targets *within* these; it cannot widen
# them. A packet asking for a different host, scheme, or method is refused
# before its digest is even considered, because a signed request for the wrong
# thing is still the wrong thing.
# --------------------------------------------------------------------------

ALLOWED_SCHEME = "https"
ALLOWED_HOST = "api.github.com"
GRAPHQL_PATH = "/graphql"

#: Operation -> (method, whether the operation may carry a request body).
ALLOWED_OPERATIONS: dict[str, tuple[str, bool]] = {
    "rest_rate_limit": ("GET", False),
    "rest_pull_request": ("GET", False),
    "rest_pull_request_comments": ("GET", False),
    "rest_conditional_not_modified": ("GET", False),
    "rest_absent_not_disclosed": ("GET", False),
    # GitHub's GraphQL API rejects unauthenticated callers. Recording that
    # refusal is the point of the operation, not a failure of it, and it is the
    # only operation permitted to POST.
    "graphql_unauthenticated_refusal": ("POST", True),
}

#: Header names retained in a capture. Everything else is dropped rather than
#: redacted: a dropped header cannot leak, and a redaction step is a place a
#: silent mistake can live.
RETAINED_RESPONSE_HEADERS = frozenset({
    "content-type",
    "etag",
    "last-modified",
    "x-ratelimit-limit",
    "x-ratelimit-remaining",
    "x-ratelimit-reset",
    "x-ratelimit-resource",
    "x-ratelimit-used",
    "x-github-media-type",
    "x-github-api-version-selected",
})

#: Byte patterns that must never appear in a persisted artifact. Checked over
#: the exact bytes written, after minimization, so a pattern introduced by the
#: minimizer itself is caught too.
FORBIDDEN_BYTE_PATTERNS: tuple[tuple[str, re.Pattern[bytes]], ...] = (
    ("authorization header", re.compile(rb"(?i)authorization\s*:")),
    ("bearer token", re.compile(rb"(?i)\bbearer\s+[A-Za-z0-9._~+/-]{8,}")),
    ("basic credential", re.compile(rb"(?i)\bbasic\s+[A-Za-z0-9+/=]{8,}")),
    # gh[pousr]_ already covers ghp_, gho_, ghu_, ghs_, and ghr_ — personal,
    # OAuth, user-to-server, server-to-server, and refresh. A separate ghs_
    # rule was written here and removed: it could never fire, because this one
    # matches first, and a rule that cannot fire reads as coverage without
    # being any.
    ("github token", re.compile(rb"\bgh[pousr]_[A-Za-z0-9]{16,}")),
    ("private key header", re.compile(rb"-----BEGIN [A-Z ]*PRIVATE KEY-----")),
    ("cookie header", re.compile(rb"(?i)\b(set-)?cookie\s*:")),
    ("embedded credential url", re.compile(rb"https?://[^/\s]*:[^/\s]*@")),
)

MAX_RESPONSE_BYTES = 1 << 20
MAX_ARTIFACT_BYTES = 1 << 20
MAX_TARGETS = 32
REQUEST_TIMEOUT_SECONDS = 15

AUTHORITY_SCHEMA = "bpr.rfc0017.anonymous_probe_authority"
AUTHORITY_SCHEMA_VERSION = 1
CAPTURE_SCHEMA = "bpr.rfc0017.anonymous_capture"
CAPTURE_SCHEMA_VERSION = 1
MANIFEST_SCHEMA = "bpr.rfc0017.anonymous_course_manifest"
MANIFEST_SCHEMA_VERSION = 1
RECORDER_REPOSITORY_PATH = "scripts/record_rfc0017_anonymous_contracts.py"
CAPTURE_ATTEMPT_SCHEMA = "bpr.rfc0017.anonymous_capture_attempt"
CAPTURE_ATTEMPT_SCHEMA_VERSION = 1
CAPTURE_ATTEMPT_REGISTRY = (
    Path(__file__).resolve().parents[1]
    / "docs/verification/reviews/rfc0017-anonymous-capture-attempts"
)

GRAPHQL_REFUSAL_OPERATION = "graphql_unauthenticated_refusal"
CALLER_ADDRESS_NORMALIZED_FIELD = "caller_address_normalized"
CALLER_ADDRESS_PLACEHOLDER = "[normalized-caller-address]"
CAPTURE_RECORD_KEYS = frozenset({
    "schema_name",
    "schema_version",
    "name",
    "operation",
    "url",
    "http_status",
    "retained_headers",
    "body",
    "body_sha256",
    "source_kind",
    "authority_sha256",
})

# This only locates address-shaped lexical tokens. ``ipaddress.ip_address``
# below remains the authority for whether a token is syntactically an address;
# the lexer is never used for blind substitution.
ADDRESS_SHAPED_TOKEN = re.compile(r"[0-9A-Fa-f:.]+")
ADDRESS_BOUNDARY_PUNCTUATION = frozenset("()[]{}<>,.;!?\"'")
RATE_LIMIT_MESSAGE = re.compile(r"\brate[ -]+limit\b", re.IGNORECASE)
GRAPHQL_ZERO_QUOTA_HEADERS = {
    "x-ratelimit-resource": "graphql",
    "x-ratelimit-limit": "0",
    "x-ratelimit-remaining": "0",
    "x-ratelimit-used": "0",
}


# --------------------------------------------------------------------------
# Exact helpers
# --------------------------------------------------------------------------

def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def canonical_json(value: Any) -> bytes:
    """One serialization, so a digest over it means something."""
    return json.dumps(
        value, ensure_ascii=False, sort_keys=True, separators=(",", ":"),
    ).encode("utf-8") + b"\n"


def parse_json_unique(payload: bytes | str, label: str) -> Any:
    """Parse JSON without allowing an earlier object member to disappear."""
    def build_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        parsed: dict[str, Any] = {}
        for key, value in pairs:
            if key in parsed:
                raise ContractError(f"{label} contains duplicate JSON key")
            parsed[key] = value
        return parsed

    try:
        return json.loads(payload, object_pairs_hook=build_object)
    except json.JSONDecodeError as error:
        raise ContractError(f"{label} is not JSON") from error


def require_exact_keys(value: Any, expected: set[str], label: str) -> dict:
    if not isinstance(value, dict):
        raise ContractError(f"{label} is not an object")
    actual = set(value)
    if actual != expected:
        missing = sorted(expected - actual)
        unexpected = sorted(actual - expected)
        raise ContractError(
            f"{label} key mismatch: missing={missing} unexpected={unexpected}")
    return value


def require_text(value: Any, label: str, limit: int = 256) -> str:
    if not isinstance(value, str) or not value or len(value) > limit:
        raise ContractError(f"{label} is not bounded text")
    if any(ord(ch) < 0x20 or ord(ch) == 0x7F for ch in value):
        raise ContractError(f"{label} contains a control character")
    return value


def require_positive_int(value: Any, label: str, maximum: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise ContractError(f"{label} is not an integer")
    if value <= 0 or value > maximum:
        raise ContractError(f"{label} is out of range")
    return value


def read_bounded(path: Path, label: str, limit: int = MAX_ARTIFACT_BYTES) -> bytes:
    if path.is_symlink():
        raise ContractError(f"{label} is a symlink")
    if not path.is_file():
        raise ContractError(f"{label} is not a regular file")
    size = path.stat().st_size
    if size > limit:
        raise ContractError(f"{label} exceeds {limit} bytes")
    return path.read_bytes()


def assert_no_forbidden_bytes(payload: bytes, label: str) -> None:
    for name, pattern in FORBIDDEN_BYTE_PATTERNS:
        if pattern.search(payload):
            raise ContractError(f"{label} contains a {name}")


def require_sha256(value: Any, label: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{64}", value):
        raise ContractError(f"{label} is not a SHA-256 digest")
    return value


# --------------------------------------------------------------------------
# Authority
# --------------------------------------------------------------------------

AUTHORITY_BLOCK = re.compile(
    r"```json\s+(\{\s*\"schema_name\"\s*:\s*\"" + re.escape(AUTHORITY_SCHEMA)
    + r"\".*?\})\s*```", re.S)
RESPONSE_PACKET_SHA = re.compile(
    r"^Packet SHA-256: `([0-9a-f]{64})`(?: \([^\r\n]*\))?$", re.MULTILINE)
QUOTED_OPERATOR_DECISION = re.compile(r"^> ([^\r\n]+)$", re.MULTILINE)
REJECTION_DECISION = re.compile(r"\bI reject\b", re.IGNORECASE)
RECORDER_BINDING_ROW = re.compile(
    r"^\|\s*`" + re.escape(RECORDER_REPOSITORY_PATH)
    + r"`\s*\|\s*`sha256:([0-9a-f]{64})`\s*\|\s*([0-9][0-9,]*)\s*\|\s*$",
    re.MULTILINE,
)


def parse_authority(packet_bytes: bytes) -> dict:
    """Extract and bound the authority block the operator approved."""
    try:
        text = packet_bytes.decode("utf-8")
    except UnicodeDecodeError as error:
        raise ContractError("packet is not UTF-8") from error
    matches = AUTHORITY_BLOCK.findall(text)
    if len(matches) != 1:
        raise ContractError(
            f"packet must carry exactly one authority block, found {len(matches)}")
    authority = parse_json_unique(matches[0], "authority block")

    require_exact_keys(
        authority,
        {"schema_name", "schema_version", "statement", "targets"},
        "authority")
    if authority["schema_name"] != AUTHORITY_SCHEMA:
        raise ContractError("authority schema name mismatch")
    authority_version = require_positive_int(
        authority["schema_version"],
        "authority schema version",
        AUTHORITY_SCHEMA_VERSION,
    )
    if authority_version != AUTHORITY_SCHEMA_VERSION:
        raise ContractError("authority schema version mismatch")
    require_text(authority["statement"], "authority statement", 512)

    targets = authority["targets"]
    if not isinstance(targets, list) or not targets:
        raise ContractError("authority names no targets")
    if len(targets) > MAX_TARGETS:
        raise ContractError("authority names too many targets")
    seen: set[str] = set()
    for index, target in enumerate(targets):
        name = validate_target(target, f"target[{index}]")
        if name in seen:
            raise ContractError(f"authority repeats target {name}")
        seen.add(name)
    return authority


def validate_target(target: Any, label: str) -> str:
    require_exact_keys(target, {"name", "operation", "url"}, label)
    name = require_text(target["name"], f"{label}.name", 64)
    if not re.fullmatch(r"[a-z0-9][a-z0-9_-]*", name):
        raise ContractError(f"{label}.name is not a safe identifier")
    operation = require_text(target["operation"], f"{label}.operation", 64)
    if operation not in ALLOWED_OPERATIONS:
        raise ContractError(f"{label}.operation {operation!r} is not allowed")
    validate_url(require_text(target["url"], f"{label}.url", 512), operation, label)
    return name


def validate_url(url: str, operation: str, label: str) -> None:
    parts = urlsplit(url)
    if parts.scheme != ALLOWED_SCHEME:
        raise ContractError(f"{label}.url scheme is not {ALLOWED_SCHEME}")
    if parts.hostname != ALLOWED_HOST:
        raise ContractError(f"{label}.url host is not {ALLOWED_HOST}")
    if parts.port is not None:
        raise ContractError(f"{label}.url names a port")
    if parts.username or parts.password:
        raise ContractError(f"{label}.url embeds a credential")
    if parts.fragment:
        raise ContractError(f"{label}.url carries a fragment")
    method, _ = ALLOWED_OPERATIONS[operation]
    if method == "POST" and parts.path != GRAPHQL_PATH:
        raise ContractError(f"{label}.url may POST only to {GRAPHQL_PATH}")
    if method == "GET" and parts.path == GRAPHQL_PATH:
        raise ContractError(f"{label}.url may not GET {GRAPHQL_PATH}")
    if not parts.path.startswith("/"):
        raise ContractError(f"{label}.url has no absolute path")


def validate_response_statement(packet_bytes: bytes, response_bytes: bytes) -> str:
    """The response must bind this packet and its one exact approval."""
    authority = parse_authority(packet_bytes)
    statement = authority["statement"]
    try:
        text = response_bytes.decode("utf-8")
    except UnicodeDecodeError as error:
        raise ContractError("response is not UTF-8") from error
    packet_digests = RESPONSE_PACKET_SHA.findall(text)
    if packet_digests != [sha256_bytes(packet_bytes)]:
        raise ContractError("response does not bind this packet exactly once")
    decisions = QUOTED_OPERATOR_DECISION.findall(text)
    if decisions != [statement] or REJECTION_DECISION.search(text):
        raise ContractError(
            "response does not carry exactly one packet approval")
    assert_no_forbidden_bytes(response_bytes, "operator response")
    return statement


def exact_capture_response(packet_bytes: bytes, statement: str) -> bytes:
    return (
        "# RFC-BPR-0017 anonymous probe operator response\n\n"
        f"Packet SHA-256: `{sha256_bytes(packet_bytes)}`\n\n"
        "## Exact operator decision\n\n"
        f"> {statement}\n"
    ).encode("utf-8")


def validate_exact_capture_response(
    packet_bytes: bytes, response_bytes: bytes, statement: str
) -> None:
    if response_bytes != exact_capture_response(packet_bytes, statement):
        raise ContractError("capture response does not match the exact byte grammar")


def validate_current_recorder_binding(packet_bytes: bytes) -> None:
    """Capture may run only with a packet bound to these recorder bytes."""
    try:
        packet_text = packet_bytes.decode("utf-8")
    except UnicodeDecodeError as error:
        raise ContractError("packet is not UTF-8") from error
    matches = RECORDER_BINDING_ROW.findall(packet_text)
    recorder_bytes = Path(__file__).resolve().read_bytes()
    expected = [(sha256_bytes(recorder_bytes), str(len(recorder_bytes)))]
    normalized = [(digest, size.replace(",", "")) for digest, size in matches]
    if normalized != expected:
        raise ContractError("packet does not bind the current recorder exactly once")


def fsync_directory(path: Path, label: str) -> None:
    flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0)
    try:
        descriptor = os.open(path, flags)
    except OSError as error:
        raise ContractError(f"{label} cannot be opened for durable sync") from error
    try:
        os.fsync(descriptor)
    except OSError as error:
        raise ContractError(f"{label} cannot be durably synced") from error
    finally:
        os.close(descriptor)


def reserve_capture_attempt(path: Path, record: dict[str, Any]) -> None:
    registry = path.parent
    try:
        registry.mkdir()
    except FileExistsError:
        if not registry.is_dir() or registry.is_symlink():
            raise ContractError("capture-attempt registry is not a real directory")
    else:
        fsync_directory(registry.parent, "capture-attempt registry parent")

    try:
        descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError as error:
        raise ContractError(
            "packet already has a capture attempt; authorization is single-use"
        ) from error
    try:
        with os.fdopen(descriptor, "wb") as handle:
            handle.write(canonical_json(record))
            handle.flush()
            os.fsync(handle.fileno())
    except OSError as error:
        raise ContractError("capture-attempt receipt cannot be durably synced") from error
    fsync_directory(registry, "capture-attempt registry")


# --------------------------------------------------------------------------
# Output containment
# --------------------------------------------------------------------------

def resolve_output_root(output_root: Path, packet: Path) -> Path:
    """The output root must sit beside the packet it is authorized by."""
    review_directory = packet.resolve(strict=True).parent
    resolved = output_root.resolve()
    if resolved == review_directory or review_directory not in resolved.parents:
        raise ContractError(
            "output root must be a directory beneath the packet's review directory")
    for part in resolved.relative_to(review_directory).parts:
        if part in {"..", ""} or part.startswith("."):
            raise ContractError("output root has an unsafe path component")
    if resolved.exists() and not resolved.is_dir():
        raise ContractError("output root exists and is not a directory")
    if resolved.is_symlink():
        raise ContractError("output root is a symlink")
    return resolved


# --------------------------------------------------------------------------
# Sanitization
# --------------------------------------------------------------------------

def minimize_headers(headers: Any) -> dict[str, str]:
    if not isinstance(headers, dict):
        raise ContractError("headers are not an object")
    retained: dict[str, str] = {}
    for name, value in headers.items():
        if not isinstance(name, str) or not isinstance(value, str):
            raise ContractError("header name or value is not text")
        lowered = name.lower()
        if lowered in RETAINED_RESPONSE_HEADERS:
            retained[lowered] = require_text(value, f"header {lowered}", 512)
    return retained


def require_graphql_zero_quota(
    http_status: int, retained_headers: dict[str, str]
) -> None:
    if http_status != 403:
        raise ContractError("GraphQL refusal status is not 403")
    for name, expected in GRAPHQL_ZERO_QUOTA_HEADERS.items():
        if retained_headers.get(name) != expected:
            raise ContractError(
                f"GraphQL refusal header {name} is not {expected!r}")


def address_spans(text: str, label: str) -> list[tuple[int, int]]:
    """Return exact spans for syntactically valid IP address tokens.

    Tokens with the basic shape of an IPv4 or IPv6 address are rejected when
    ``ipaddress`` cannot parse them. This keeps an almost-address from slipping
    through a successful normalization as unrelated message text.
    """
    spans: list[tuple[int, int]] = []
    for match in ADDRESS_SHAPED_TOKEN.finditer(text):
        raw = match.group(0)
        token = raw.strip(".")
        if token.count(".") < 3 and token.count(":") < 2:
            continue
        start = match.start() + len(raw) - len(raw.lstrip("."))
        end = match.end() - len(raw) + len(raw.rstrip("."))
        before = text[match.start() - 1] if match.start() else ""
        after = text[match.end()] if match.end() < len(text) else ""
        if ((before and not before.isspace()
             and before not in ADDRESS_BOUNDARY_PUNCTUATION)
                or (after and not after.isspace()
                    and after not in ADDRESS_BOUNDARY_PUNCTUATION)):
            raise ContractError(
                f"{label} contains an unsupported address boundary")
        try:
            ipaddress.ip_address(token)
        except ValueError as error:
            raise ContractError(
                f"{label} contains a malformed address token") from error
        spans.append((start, end))
    return spans


def iter_json_text(value: Any):
    if isinstance(value, str):
        yield value
    elif isinstance(value, list):
        for item in value:
            yield from iter_json_text(item)
    elif isinstance(value, dict):
        for key, item in value.items():
            if isinstance(key, str):
                yield key
            yield from iter_json_text(item)


def assert_no_address_text(value: Any, label: str) -> None:
    for text in iter_json_text(value):
        if address_spans(text, label):
            raise ContractError(f"{label} retains an IP address")


def normalize_graphql_refusal_body(parsed: Any) -> dict:
    if not isinstance(parsed, dict):
        raise ContractError("GraphQL refusal body is not an object")
    message = parsed.get("message")
    if not isinstance(message, str) or not message:
        raise ContractError("GraphQL refusal message is not text")
    if not RATE_LIMIT_MESSAGE.search(message):
        raise ContractError("GraphQL refusal message is not a rate-limit refusal")

    message_spans = address_spans(message, "GraphQL refusal message")
    all_spans = sum(
        len(address_spans(text, "GraphQL refusal body"))
        for text in iter_json_text(parsed)
    )
    if len(message_spans) != 1 or all_spans != 1:
        raise ContractError(
            "GraphQL refusal body must contain exactly one caller address")

    start, end = message_spans[0]
    normalized = dict(parsed)
    normalized["message"] = (
        message[:start] + CALLER_ADDRESS_PLACEHOLDER + message[end:])
    return normalized


def validate_normalized_graphql_body(body_text: Any) -> bytes:
    if not isinstance(body_text, str) or not body_text:
        raise ContractError("normalized GraphQL refusal body is not text")
    parsed = parse_json_unique(body_text, "normalized GraphQL refusal body")
    if not isinstance(parsed, dict):
        raise ContractError("normalized GraphQL refusal body is not an object")
    message = parsed.get("message")
    if not isinstance(message, str) or not RATE_LIMIT_MESSAGE.search(message):
        raise ContractError("normalized GraphQL refusal message is unsupported")
    placeholder_count = sum(
        text.count(CALLER_ADDRESS_PLACEHOLDER)
        for text in iter_json_text(parsed)
    )
    if placeholder_count != 1 or CALLER_ADDRESS_PLACEHOLDER not in message:
        raise ContractError(
            "normalized GraphQL refusal body has no exact caller-address placeholder")
    assert_no_address_text(parsed, "normalized GraphQL refusal body")
    canonical = canonical_json(parsed)
    if canonical.decode("utf-8") != body_text:
        raise ContractError("normalized GraphQL refusal body is not canonical JSON")
    return canonical


def validate_capture_record(record: Any, target: dict, course: dict) -> None:
    operation = target["operation"]
    expected_keys = set(CAPTURE_RECORD_KEYS)
    if operation == GRAPHQL_REFUSAL_OPERATION:
        expected_keys.add(CALLER_ADDRESS_NORMALIZED_FIELD)
    require_exact_keys(record, expected_keys, f"capture {course['name']}")

    if record["schema_name"] != CAPTURE_SCHEMA:
        raise ContractError(f"capture {course['name']} schema name mismatch")
    schema_version = require_positive_int(
        record["schema_version"],
        f"capture {course['name']} schema version",
        CAPTURE_SCHEMA_VERSION,
    )
    if schema_version != CAPTURE_SCHEMA_VERSION:
        raise ContractError(f"capture {course['name']} schema version mismatch")
    if record["name"] != target["name"]:
        raise ContractError(f"capture {course['name']} name mismatch")
    if record["operation"] != operation:
        raise ContractError(f"capture {course['name']} operation mismatch")
    if record["url"] != target["url"]:
        raise ContractError(f"capture {course['name']} URL mismatch")
    http_status = require_positive_int(
        record["http_status"], f"capture {course['name']} status", 599)
    if http_status != course["expected_status"]:
        raise ContractError(
            f"capture {course['name']} status does not match its course")
    if record["source_kind"] not in {"live", "synthetic"}:
        raise ContractError(f"capture {course['name']} source kind mismatch")
    require_sha256(record["authority_sha256"], "capture authority_sha256")
    require_sha256(record["body_sha256"], "capture body_sha256")

    retained_headers = record["retained_headers"]
    if minimize_headers(retained_headers) != retained_headers:
        raise ContractError(f"capture {course['name']} retained headers mismatch")

    if operation == GRAPHQL_REFUSAL_OPERATION:
        if record[CALLER_ADDRESS_NORMALIZED_FIELD] is not True:
            raise ContractError(
                "GraphQL refusal caller-address normalization marker mismatch")
        require_graphql_zero_quota(http_status, retained_headers)
        normalized_body = validate_normalized_graphql_body(record["body"])
        if record["body_sha256"] != sha256_bytes(normalized_body):
            raise ContractError("normalized GraphQL refusal body digest mismatch")
        assert_no_address_text(record, "normalized GraphQL refusal record")


def sanitize_capture(
    *,
    name: str,
    operation: str,
    url: str,
    http_status: int,
    headers: Any,
    body: bytes,
    authority_sha256: str,
    source_kind: str,
) -> dict[str, object]:
    """Build the persisted record. No transient value survives this call."""
    if operation not in ALLOWED_OPERATIONS:
        raise ContractError("unknown operation")
    if source_kind not in {"live", "synthetic"}:
        raise ContractError("unknown source kind")
    require_positive_int(http_status, "http status", 599)
    if len(body) > MAX_RESPONSE_BYTES:
        raise ContractError("response body exceeds the byte limit")

    retained_headers = minimize_headers(headers)
    body_text: str | None = None
    body_sha256 = sha256_bytes(body)
    if body:
        if operation == GRAPHQL_REFUSAL_OPERATION:
            parsed = parse_json_unique(body, "response body")
            require_graphql_zero_quota(http_status, retained_headers)
            parsed = normalize_graphql_refusal_body(parsed)
        else:
            try:
                parsed = json.loads(body)
            except json.JSONDecodeError as error:
                raise ContractError("response body is not JSON") from error
        # The parsed value is re-serialized canonically rather than stored raw,
        # so an unstable field ordering cannot make two identical contracts
        # compare unequal.
        canonical_body = canonical_json(parsed)
        if operation == GRAPHQL_REFUSAL_OPERATION:
            canonical_body = validate_normalized_graphql_body(
                canonical_body.decode("utf-8"))
            body_sha256 = sha256_bytes(canonical_body)
        body_text = canonical_body.decode("utf-8")
    elif operation == GRAPHQL_REFUSAL_OPERATION:
        raise ContractError("GraphQL refusal body is empty")

    record = {
        "schema_name": CAPTURE_SCHEMA,
        "schema_version": CAPTURE_SCHEMA_VERSION,
        "name": name,
        "operation": operation,
        "url": url,
        "http_status": http_status,
        "retained_headers": retained_headers,
        "body": body_text,
        "body_sha256": body_sha256,
        "source_kind": source_kind,
        "authority_sha256": authority_sha256,
    }
    if operation == GRAPHQL_REFUSAL_OPERATION:
        record[CALLER_ADDRESS_NORMALIZED_FIELD] = True
        assert_no_address_text(record, "normalized GraphQL refusal record")
    payload = canonical_json(record)
    assert_no_forbidden_bytes(payload, f"capture {name}")
    return record


# --------------------------------------------------------------------------
# The only function that reaches a network
# --------------------------------------------------------------------------

def build_bounded_opener():
    """Build an HTTPS opener with no ambient proxy or redirect authority."""
    import urllib.request

    class RefuseRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, req, fp, code, msg, headers, newurl):
            raise ContractError("redirected request is outside probe authority")

    return urllib.request.build_opener(
        urllib.request.ProxyHandler({}),
        RefuseRedirect(),
        urllib.request.HTTPSHandler(),
    )


def perform_request(url: str, operation: str, body: bytes | None):
    """Issue one credential-free request. Imported lazily on purpose."""
    import urllib.error
    import urllib.request

    method, allows_body = ALLOWED_OPERATIONS[operation]
    if body is not None and not allows_body:
        raise ContractError(f"{operation} may not carry a request body")
    validate_url(url, operation, "request")

    request = urllib.request.Request(url, data=body, method=method)
    request.add_header("Accept", "application/vnd.github+json")
    request.add_header("X-GitHub-Api-Version", "2022-11-28")
    request.add_header("User-Agent", "BetterPullReview-RFC0017-anonymous-probe")
    for header, _ in request.header_items():
        if header.lower() in {"authorization", "cookie"}:
            raise ContractError("a credential header was constructed")
    opener = build_bounded_opener()
    try:
        with opener.open(request, timeout=REQUEST_TIMEOUT_SECONDS) as response:
            return response.status, dict(response.headers), response.read(
                MAX_RESPONSE_BYTES + 1)
    except urllib.error.HTTPError as error:
        # A 401, 403, 404, or 304 is a contract to record, not a failure.
        return error.code, dict(error.headers or {}), error.read(
            MAX_RESPONSE_BYTES + 1)


# --------------------------------------------------------------------------
# Modes
# --------------------------------------------------------------------------

def load_inputs(args: argparse.Namespace) -> tuple[bytes, bytes, dict, dict, str]:
    packet_bytes = read_bounded(Path(args.packet), "packet")
    response_bytes = read_bounded(Path(args.response), "response")
    manifest_bytes = read_bounded(Path(args.course_manifest), "course manifest")
    authority = parse_authority(packet_bytes)
    validate_response_statement(packet_bytes, response_bytes)
    manifest = validate_course_manifest(manifest_bytes, authority)
    return packet_bytes, response_bytes, authority, manifest, sha256_bytes(
        canonical_json(authority))


def validate_course_manifest(manifest_bytes: bytes, authority: dict) -> dict:
    manifest = parse_json_unique(manifest_bytes, "course manifest")
    require_exact_keys(
        manifest,
        {"schema_name", "schema_version", "packet_sha256", "courses"},
        "course manifest")
    if manifest["schema_name"] != MANIFEST_SCHEMA:
        raise ContractError("course manifest schema name mismatch")
    manifest_version = require_positive_int(
        manifest["schema_version"],
        "course manifest schema version",
        MANIFEST_SCHEMA_VERSION,
    )
    if manifest_version != MANIFEST_SCHEMA_VERSION:
        raise ContractError("course manifest schema version mismatch")
    require_text(manifest["packet_sha256"], "packet_sha256", 64)
    courses = manifest["courses"]
    if not isinstance(courses, list) or not courses:
        raise ContractError("course manifest names no courses")
    authorized = {target["name"] for target in authority["targets"]}
    seen: set[str] = set()
    for index, course in enumerate(courses):
        require_exact_keys(course, {"name", "expected_status"}, f"course[{index}]")
        name = require_text(course["name"], f"course[{index}].name", 64)
        if name not in authorized:
            raise ContractError(f"course {name} is not an authorized target")
        if name in seen:
            raise ContractError(f"course manifest repeats course {name}")
        seen.add(name)
        require_positive_int(
            course["expected_status"], f"course[{index}].expected_status", 599)
    return manifest


def run_validate_existing(args: argparse.Namespace) -> int:
    packet_bytes, _response, authority, manifest, authority_sha = load_inputs(args)
    if manifest["packet_sha256"] != sha256_bytes(packet_bytes):
        raise ContractError("course manifest does not bind this packet")
    root = resolve_output_root(Path(args.output_root), Path(args.packet))
    if not root.is_dir():
        raise ContractError("output root does not exist; nothing to revalidate")

    targets = {target["name"]: target for target in authority["targets"]}
    checked = 0
    for course in manifest["courses"]:
        capture_path = root / f"{course['name']}.json"
        payload = read_bounded(capture_path, f"capture {course['name']}")
        assert_no_forbidden_bytes(payload, f"capture {course['name']}")
        try:
            record = json.loads(payload)
        except json.JSONDecodeError as error:
            raise ContractError(
                f"capture {course['name']} is not valid JSON") from error
        validate_capture_record(record, targets[course["name"]], course)
        if record.get("authority_sha256") != authority_sha:
            raise ContractError(
                f"capture {course['name']} was recorded under another authority")
        if canonical_json(record) != payload:
            raise ContractError(
                f"capture {course['name']} is not canonically serialized")
        checked += 1
    print(f"RFC0017_ANONYMOUS_CONTRACTS_VALID:{checked}")
    return 0


def run_capture(args: argparse.Namespace) -> int:
    packet_bytes, response_bytes, authority, manifest, authority_sha = load_inputs(
        args)
    if manifest["packet_sha256"] != sha256_bytes(packet_bytes):
        raise ContractError("course manifest does not bind this packet")
    validate_exact_capture_response(
        packet_bytes, response_bytes, authority["statement"])
    validate_current_recorder_binding(packet_bytes)
    root = resolve_output_root(Path(args.output_root), Path(args.packet))
    targets = {target["name"]: target for target in authority["targets"]}

    root.mkdir(parents=True, exist_ok=True)
    destinations = {
        course["name"]: root / f"{course['name']}.json"
        for course in manifest["courses"]
    }
    for name, destination in destinations.items():
        if destination.exists() or destination.is_symlink():
            raise ContractError(
                f"capture {name} already exists; authorization is single-use")

    attempt_path = CAPTURE_ATTEMPT_REGISTRY / (
        f"{sha256_bytes(packet_bytes)}.json")
    attempt_record = {
        "schema_name": CAPTURE_ATTEMPT_SCHEMA,
        "schema_version": CAPTURE_ATTEMPT_SCHEMA_VERSION,
        "packet_sha256": sha256_bytes(packet_bytes),
        "response_sha256": sha256_bytes(response_bytes),
        "authority_sha256": authority_sha,
        "recorder_sha256": sha256_bytes(Path(__file__).resolve().read_bytes()),
        "courses": manifest["courses"],
    }
    reserve_capture_attempt(attempt_path, attempt_record)

    written = 0
    for course in manifest["courses"]:
        target = targets[course["name"]]
        destination = destinations[target["name"]]
        try:
            descriptor = os.open(
                destination, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        except FileExistsError as error:
            raise ContractError(
                f"capture {target['name']} already exists; "
                "authorization is single-use") from error
        with os.fdopen(descriptor, "wb") as handle:
            status, headers, body = perform_request(
                target["url"], target["operation"], None)
            if len(body) > MAX_RESPONSE_BYTES:
                raise ContractError(
                    f"{course['name']} response exceeds the byte limit")
            record = sanitize_capture(
                name=target["name"], operation=target["operation"],
                url=target["url"], http_status=status, headers=headers, body=body,
                authority_sha256=authority_sha, source_kind="live")
            payload = canonical_json(record)
            handle.write(payload)
        written += 1
    print(f"RFC0017_ANONYMOUS_CONTRACTS_CAPTURED:{written}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Record sanitized anonymous GitHub service contracts.")
    parser.add_argument("--packet", required=True)
    parser.add_argument("--response", required=True)
    parser.add_argument("--course-manifest", required=True)
    parser.add_argument("--output-root", required=True)
    parser.add_argument(
        "--validate-existing", action="store_true",
        help="perform no traffic; revalidate everything already recorded")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.validate_existing:
            return run_validate_existing(args)
        return run_capture(args)
    except ContractError as error:
        sys.stderr.write(f"RFC0017_ANONYMOUS_CONTRACTS_REFUSED:{error}\n")
        return 3


if __name__ == "__main__":
    raise SystemExit(main())
