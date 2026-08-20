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


# --------------------------------------------------------------------------
# Authority
# --------------------------------------------------------------------------

AUTHORITY_BLOCK = re.compile(
    r"```json\s+(\{\s*\"schema_name\"\s*:\s*\"" + re.escape(AUTHORITY_SCHEMA)
    + r"\".*?\})\s*```", re.S)


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
    try:
        authority = json.loads(matches[0])
    except json.JSONDecodeError as error:
        raise ContractError("authority block is not valid JSON") from error

    require_exact_keys(
        authority,
        {"schema_name", "schema_version", "statement", "targets"},
        "authority")
    if authority["schema_name"] != AUTHORITY_SCHEMA:
        raise ContractError("authority schema name mismatch")
    if authority["schema_version"] != AUTHORITY_SCHEMA_VERSION:
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
    """The response must carry the packet's statement byte-for-byte."""
    authority = parse_authority(packet_bytes)
    statement = authority["statement"]
    try:
        text = response_bytes.decode("utf-8")
    except UnicodeDecodeError as error:
        raise ContractError("response is not UTF-8") from error
    if statement not in text:
        raise ContractError(
            "response does not carry the packet statement byte-for-byte")
    assert_no_forbidden_bytes(response_bytes, "operator response")
    return statement


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

    body_text: str | None = None
    if body:
        try:
            parsed = json.loads(body)
        except json.JSONDecodeError as error:
            raise ContractError("response body is not JSON") from error
        # The parsed value is re-serialized canonically rather than stored raw,
        # so an unstable field ordering cannot make two identical contracts
        # compare unequal.
        body_text = canonical_json(parsed).decode("utf-8")

    record = {
        "schema_name": CAPTURE_SCHEMA,
        "schema_version": CAPTURE_SCHEMA_VERSION,
        "name": name,
        "operation": operation,
        "url": url,
        "http_status": http_status,
        "retained_headers": minimize_headers(headers),
        "body": body_text,
        "body_sha256": sha256_bytes(body),
        "source_kind": source_kind,
        "authority_sha256": authority_sha256,
    }
    payload = canonical_json(record)
    assert_no_forbidden_bytes(payload, f"capture {name}")
    return record


# --------------------------------------------------------------------------
# The only function that reaches a network
# --------------------------------------------------------------------------

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
    opener = urllib.request.build_opener(urllib.request.HTTPSHandler())
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
    try:
        manifest = json.loads(manifest_bytes)
    except json.JSONDecodeError as error:
        raise ContractError("course manifest is not valid JSON") from error
    require_exact_keys(
        manifest,
        {"schema_name", "schema_version", "packet_sha256", "courses"},
        "course manifest")
    if manifest["schema_name"] != MANIFEST_SCHEMA:
        raise ContractError("course manifest schema name mismatch")
    if manifest["schema_version"] != MANIFEST_SCHEMA_VERSION:
        raise ContractError("course manifest schema version mismatch")
    require_text(manifest["packet_sha256"], "packet_sha256", 64)
    courses = manifest["courses"]
    if not isinstance(courses, list) or not courses:
        raise ContractError("course manifest names no courses")
    authorized = {target["name"] for target in authority["targets"]}
    for index, course in enumerate(courses):
        require_exact_keys(course, {"name", "expected_status"}, f"course[{index}]")
        name = require_text(course["name"], f"course[{index}].name", 64)
        if name not in authorized:
            raise ContractError(f"course {name} is not an authorized target")
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
        if record.get("authority_sha256") != authority_sha:
            raise ContractError(
                f"capture {course['name']} was recorded under another authority")
        if record.get("http_status") != course["expected_status"]:
            raise ContractError(
                f"capture {course['name']} status does not match its course")
        if canonical_json(record) != payload:
            raise ContractError(
                f"capture {course['name']} is not canonically serialized")
        checked += 1
    print(f"RFC0017_ANONYMOUS_CONTRACTS_VALID:{checked}")
    return 0


def run_capture(args: argparse.Namespace) -> int:
    packet_bytes, _response, authority, manifest, authority_sha = load_inputs(args)
    if manifest["packet_sha256"] != sha256_bytes(packet_bytes):
        raise ContractError("course manifest does not bind this packet")
    root = resolve_output_root(Path(args.output_root), Path(args.packet))
    targets = {target["name"]: target for target in authority["targets"]}

    root.mkdir(parents=True, exist_ok=True)
    written = 0
    for course in manifest["courses"]:
        target = targets[course["name"]]
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
        destination = root / f"{target['name']}.json"
        with os.fdopen(os.open(
                destination, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600),
                "wb") as handle:
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
