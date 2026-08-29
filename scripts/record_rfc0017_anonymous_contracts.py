#!/usr/bin/env python3
"""Record sanitized anonymous GitHub service contracts for RFC-BPR-0017.

BPR-0405 authors this; BPR-0298 executes it. The split matters: the live
capture is the only thing here that touches a network, and it is the only thing
this module refuses to do without a digest-bound authority the operator signed.

Four modes, exactly one of which touches a network:

* default — validate the authority, then perform exactly the requests the
  authority names, sanitize each response, and write it under the output root.
* ``--validate-existing`` — perform no traffic at all and revalidate every
  packet, response, manifest, capture digest, minimization record, external
  commit binding, and derived artifact.
* ``--write-bindings`` — perform no traffic; revalidate everything, then write
  the external ``capture-bindings.json`` that ties each committed capture to
  the commit that owns it.
* ``--synthesize-exhaustion`` — perform no traffic; derive the synthetic
  primary-rate-exhaustion fixture from committed captures alone.

The module deliberately holds no network client at import time. ``urllib`` is
imported inside the one function that issues a request, so that importing this
module for validation cannot reach the network even by mistake. ``subprocess``
is imported at module scope because the only child process this module ever
launches is a local read-only ``git`` query — ``ls-files``, ``status``,
``log``, and ``cat-file`` — none of which contacts a remote.
"""
from __future__ import annotations

import argparse
import hashlib
import ipaddress
import json
import os
import re
import subprocess
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
#: Version 2 adds exactly one target member — ``condition_source`` — and only
#: to a conditional target. Version 1 keeps being parsed by the rules it was
#: written under, so every packet already committed still means what it meant.
AUTHORITY_CONDITION_SOURCE_VERSION = 2
AUTHORITY_SCHEMA_VERSIONS = (
    AUTHORITY_SCHEMA_VERSION, AUTHORITY_CONDITION_SOURCE_VERSION)
CAPTURE_SCHEMA = "bpr.rfc0017.anonymous_capture"
CAPTURE_SCHEMA_VERSION = 1
MANIFEST_SCHEMA = "bpr.rfc0017.anonymous_course_manifest"
MANIFEST_SCHEMA_VERSION = 1
COURSE_SELECTION_SCHEMA = "bpr.rfc0017.anonymous_course_selection"
COURSE_SELECTION_SCHEMA_VERSION = 1
RECORDER_REPOSITORY_PATH = "scripts/record_rfc0017_anonymous_contracts.py"
REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
CAPTURE_ATTEMPT_SCHEMA = "bpr.rfc0017.anonymous_capture_attempt"
CAPTURE_ATTEMPT_SCHEMA_VERSION = 1
CAPTURE_ATTEMPT_REGISTRY = (
    REPOSITORY_ROOT
    / "docs/verification/reviews/rfc0017-anonymous-capture-attempts"
)
#: A ``condition_source`` path is resolved against the repository root, because
#: the capture it pins belongs to an earlier review directory than the packet
#: naming it. Held as a module constant so a test can point it somewhere else
#: rather than writing fixtures into the repository.
CONDITION_SOURCE_ROOT = REPOSITORY_ROOT

BINDING_SCHEMA = "bpr.rfc0017.anonymous_capture_binding"
BINDING_SCHEMA_VERSION = 1
BINDING_FILE_NAME = "capture-bindings.json"
BINDING_KEYS = frozenset({
    "schema_name",
    "schema_version",
    "packet_sha256",
    "bindings",
    "derived_artifacts",
})
BINDING_ENTRY_KEYS = frozenset({
    "course",
    "capture_file",
    "file_sha256",
    "minimized_body_sha256",
    "owning_commit",
})
#: A derived artifact carries no ``owning_commit``. It cannot: it is written in
#: the same change that first commits it, so no commit owns it at the moment its
#: binding is computed, and re-deriving the binding after that commit would
#: produce different bytes. Its authenticity comes from the stronger property
#: instead — it re-derives byte-for-byte from captures that *are* commit-bound.
DERIVED_ENTRY_KEYS = frozenset({
    "artifact",
    "artifact_file",
    "file_sha256",
    "minimized_body_sha256",
})

SYNTHETIC_DIRECTORY_NAME = "synthetic"
SYNTHETIC_EXHAUSTION_NAME = "rate-exhaustion"
SYNTHETIC_EXHAUSTION_FILE = f"{SYNTHETIC_EXHAUSTION_NAME}.json"
#: The three committed courses the synthetic fixture is a pure function of.
SYNTHETIC_BUDGET_COURSE = "rate-limit"
SYNTHETIC_REFUSAL_COURSE = "graphql-refusal"
SYNTHETIC_REQUEST_COURSE = "pull-request"
SYNTHETIC_SOURCE_COURSES = (
    SYNTHETIC_BUDGET_COURSE,
    SYNTHETIC_REFUSAL_COURSE,
    SYNTHETIC_REQUEST_COURSE,
)
SYNTHETIC_EXHAUSTED_RESOURCE = "core"
EXHAUSTED_REMAINING = "0"

GIT_EXECUTABLE = "git"
#: Keep the child deterministic and silent. None of these enables a remote.
GIT_ENVIRONMENT = {
    "GIT_OPTIONAL_LOCKS": "0",
    "GIT_TERMINAL_PROMPT": "0",
    "GIT_PAGER": "cat",
    "LC_ALL": "C",
}

GRAPHQL_REFUSAL_OPERATION = "graphql_unauthenticated_refusal"
#: The conditional revalidation operation: the only one that reads an earlier
#: capture before it asks anything, and the only one whose recorded body must
#: be empty. A 304 is not a document — it is the service agreeing that a
#: document already on record is still current — so the request cannot be
#: composed from the packet alone.
CONDITIONAL_OPERATION = "rest_conditional_not_modified"
CONDITIONAL_EXPECTED_STATUS = 304
CONDITION_SOURCE_KEY = "condition_source"
CONDITION_SOURCE_KEYS = frozenset({"capture_file", "file_sha256"})
#: Exactly the outbound headers a conditional request may add. Nothing else may
#: be attached to any request this module issues.
CONDITIONAL_VALIDATOR_HEADERS = ("If-None-Match", "If-Modified-Since")
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


def require_commit_id(value: Any, label: str) -> str:
    """A full object name only. An abbreviation can become ambiguous later."""
    if not isinstance(value, str) or not re.fullmatch(
        r"[0-9a-f]{40}(?:[0-9a-f]{24})?", value
    ):
        raise ContractError(f"{label} is not a full commit identifier")
    return value


# --------------------------------------------------------------------------
# Local Git evidence
#
# The captures are byte-frozen files whose authority is that they were
# committed. Nothing in a capture can prove that, because a self-consistent
# rewrite of a record and its own digests is trivially cheap. So the proof is
# taken from outside the record: the commit that owns the file, and the blob
# that commit stores. Every call below is a local read; none contacts a remote.
# --------------------------------------------------------------------------

def run_git(arguments: list[str], *, cwd: Path, label: str) -> bytes:
    environment = dict(os.environ)
    environment.update(GIT_ENVIRONMENT)
    try:
        completed = subprocess.run(
            [GIT_EXECUTABLE, "-C", str(cwd), *arguments],
            capture_output=True, check=False, env=environment)
    except OSError as error:
        raise ContractError(
            f"{label} needs git, which could not be run") from error
    if completed.returncode != 0:
        raise ContractError(f"{label} could not be read from git history")
    return completed.stdout


def git_tracked_path(path: Path, label: str) -> str:
    """Return the repository-relative path of a tracked file."""
    output = run_git(
        ["ls-files", "--full-name", "-z", "--", path.name],
        cwd=path.parent, label=f"{label} tracking state")
    entries = [entry for entry in output.split(b"\0") if entry]
    if len(entries) != 1:
        raise ContractError(f"{label} is not tracked in git")
    return entries[0].decode("utf-8")


def require_committed_unmodified(path: Path, label: str) -> None:
    output = run_git(
        ["status", "--porcelain", "-z", "--", path.name],
        cwd=path.parent, label=f"{label} worktree state")
    if output.strip(b"\0"):
        raise ContractError(f"{label} is untracked or differs from HEAD")


def git_owning_commit(path: Path, label: str) -> str:
    output = run_git(
        ["log", "-n", "1", "--format=%H", "--", path.name],
        cwd=path.parent, label=f"{label} owning commit")
    commit = output.decode("utf-8", "replace").strip()
    if not re.fullmatch(r"[0-9a-f]{40}(?:[0-9a-f]{24})?", commit):
        raise ContractError(f"{label} has no owning commit in git history")
    return commit


def git_committed_blob(
    path: Path, commit: str, repository_path: str, label: str
) -> bytes:
    return run_git(
        ["cat-file", "blob", f"{commit}:{repository_path}"],
        cwd=path.parent, label=f"{label} blob at commit {commit[:12]}")


# --------------------------------------------------------------------------
# Authority
# --------------------------------------------------------------------------

FENCED_JSON_BLOCK = re.compile(r"```json\s+(\{.*?\})\s*```", re.S)
RESPONSE_PACKET_SHA = re.compile(
    r"^Packet SHA-256: `([0-9a-f]{64})`(?: \([^\r\n]*\))?$", re.MULTILINE)
QUOTED_OPERATOR_DECISION = re.compile(r"^> ([^\r\n]+)$", re.MULTILINE)
REJECTION_DECISION = re.compile(r"\bI reject\b", re.IGNORECASE)
RECORDER_BINDING_ROW = re.compile(
    r"^\|\s*`" + re.escape(RECORDER_REPOSITORY_PATH)
    + r"`\s*\|\s*`sha256:([0-9a-f]{64})`\s*\|\s*([0-9][0-9,]*)\s*\|\s*$",
    re.MULTILINE,
)


def parse_packet_schema_block(
    packet_bytes: bytes, schema_name: str, label: str
) -> dict:
    """Find one packet JSON object by schema, independent of member order."""
    try:
        text = packet_bytes.decode("utf-8")
    except UnicodeDecodeError as error:
        raise ContractError("packet is not UTF-8") from error
    blocks = [
        parse_json_unique(block, f"packet JSON block[{index}]")
        for index, block in enumerate(FENCED_JSON_BLOCK.findall(text))
    ]
    matches = [
        block for block in blocks
        if block.get("schema_name") == schema_name
    ]
    if len(matches) != 1:
        raise ContractError(
            f"packet must carry exactly one {label} block, found {len(matches)}")
    return matches[0]


def parse_authority(packet_bytes: bytes) -> dict:
    """Extract and bound the authority block the operator approved."""
    authority = parse_packet_schema_block(
        packet_bytes, AUTHORITY_SCHEMA, "authority")

    require_exact_keys(
        authority,
        {"schema_name", "schema_version", "statement", "targets"},
        "authority")
    if authority["schema_name"] != AUTHORITY_SCHEMA:
        raise ContractError("authority schema name mismatch")
    authority_version = require_positive_int(
        authority["schema_version"],
        "authority schema version",
        max(AUTHORITY_SCHEMA_VERSIONS),
    )
    if authority_version not in AUTHORITY_SCHEMA_VERSIONS:
        # Unreachable while the supported versions run contiguously from 1.
        # It is what keeps a future gap from being admitted by the bound alone,
        # and it is the only place the supported set is stated as a set.
        raise ContractError("authority schema version mismatch")
    require_text(authority["statement"], "authority statement", 512)

    targets = authority["targets"]
    if not isinstance(targets, list) or not targets:
        raise ContractError("authority names no targets")
    if len(targets) > MAX_TARGETS:
        raise ContractError("authority names too many targets")
    seen: set[str] = set()
    for index, target in enumerate(targets):
        name = validate_target(
            target, f"target[{index}]", schema_version=authority_version)
        if name in seen:
            raise ContractError(f"authority repeats target {name}")
        seen.add(name)
    return authority


def validate_target(target: Any, label: str, *, schema_version: int) -> str:
    if not isinstance(target, dict):
        raise ContractError(f"{label} is not an object")
    conditional = target.get("operation") == CONDITIONAL_OPERATION
    if conditional and schema_version < AUTHORITY_CONDITION_SOURCE_VERSION:
        # A conditional request is defined by the validators it replays, and
        # only a version-2 target can say where they come from. A version-1
        # packet therefore cannot authorize one at all.
        raise ContractError(
            f"{label}.operation {CONDITIONAL_OPERATION!r} needs authority "
            f"schema version {AUTHORITY_CONDITION_SOURCE_VERSION}")
    if CONDITION_SOURCE_KEY in target and not conditional:
        raise ContractError(
            f"{label}.{CONDITION_SOURCE_KEY} is allowed only on "
            f"{CONDITIONAL_OPERATION}")
    expected = {"name", "operation", "url"}
    if conditional:
        expected.add(CONDITION_SOURCE_KEY)
    require_exact_keys(target, expected, label)
    name = require_text(target["name"], f"{label}.name", 64)
    if not re.fullmatch(r"[a-z0-9][a-z0-9_-]*", name):
        raise ContractError(f"{label}.name is not a safe identifier")
    operation = require_text(target["operation"], f"{label}.operation", 64)
    if operation not in ALLOWED_OPERATIONS:
        raise ContractError(f"{label}.operation {operation!r} is not allowed")
    validate_url(require_text(target["url"], f"{label}.url", 512), operation, label)
    if conditional:
        validate_condition_source(
            target[CONDITION_SOURCE_KEY], f"{label}.{CONDITION_SOURCE_KEY}")
    return name


def require_repository_relative_path(value: Any, label: str) -> str:
    """A path under the repository root, with no component that leaves it."""
    text = require_text(value, label, 256)
    if text.startswith("/") or "\\" in text or ":" in text:
        raise ContractError(f"{label} is not a repository-relative path")
    for part in text.split("/"):
        if not part or part.startswith("."):
            raise ContractError(f"{label} has an unsafe path component")
    return text


def validate_condition_source(value: Any, label: str) -> dict:
    """Bound the pin: one repository-relative capture and its exact digest.

    The authority names the source by path *and* digest, so the operator's
    signature covers the bytes the request will replay. Nothing about the
    conditional request is looked up ambiently at run time.
    """
    require_exact_keys(value, set(CONDITION_SOURCE_KEYS), label)
    require_repository_relative_path(
        value["capture_file"], f"{label}.capture_file")
    require_sha256(value["file_sha256"], f"{label}.file_sha256")
    return value


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


def parse_packet_course_selection(packet_bytes: bytes) -> list[dict]:
    """Extract the exact request courses approved with this packet."""
    selection = parse_packet_schema_block(
        packet_bytes, COURSE_SELECTION_SCHEMA, "course-selection")
    require_exact_keys(
        selection,
        {"schema_name", "schema_version", "courses"},
        "course selection",
    )
    if selection["schema_name"] != COURSE_SELECTION_SCHEMA:
        raise ContractError("course-selection schema name mismatch")
    selection_version = require_positive_int(
        selection["schema_version"],
        "course-selection schema version",
        COURSE_SELECTION_SCHEMA_VERSION,
    )
    if selection_version != COURSE_SELECTION_SCHEMA_VERSION:
        raise ContractError("course-selection schema version mismatch")
    courses = selection["courses"]
    if not isinstance(courses, list) or not courses:
        raise ContractError("packet course selection names no courses")
    seen: set[str] = set()
    for index, course in enumerate(courses):
        require_exact_keys(
            course, {"name", "expected_status"},
            f"packet course[{index}]")
        name = require_text(
            course["name"], f"packet course[{index}].name", 64)
        if name in seen:
            raise ContractError(f"packet course selection repeats course {name}")
        seen.add(name)
        require_positive_int(
            course["expected_status"],
            f"packet course[{index}].expected_status",
            599,
        )
    return courses


def validate_packet_course_selection(
    packet_bytes: bytes, manifest: dict
) -> None:
    if parse_packet_course_selection(packet_bytes) != manifest["courses"]:
        raise ContractError("course manifest differs from the approved packet")


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


def write_artifact_once(path: Path, payload: bytes, label: str) -> None:
    """Write, or accept an identical existing file. Never overwrite.

    Both derived artifacts are pure functions of committed evidence, so a
    differing file on disk is a disagreement to report rather than a stale
    result to replace.
    """
    if path.is_symlink():
        raise ContractError(f"{label} is a symlink")
    if path.exists():
        if read_bounded(path, label) == payload:
            return
        raise ContractError(f"{label} already exists with different content")
    directory = path.parent
    if directory.is_symlink():
        raise ContractError(f"{label} directory is a symlink")
    directory.mkdir(parents=True, exist_ok=True)
    try:
        descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644)
    except FileExistsError as error:
        raise ContractError(
            f"{label} appeared while it was being written") from error
    try:
        with os.fdopen(descriptor, "wb") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
    except OSError as error:
        raise ContractError(f"{label} cannot be durably written") from error
    fsync_directory(directory, f"{label} directory")


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

    if operation == CONDITIONAL_OPERATION:
        # A not-modified response has no body, so the record must have none
        # either — and its digest must be the digest of that absence, which is
        # the one case where the stored digest really does digest what is
        # stored. The status was already required to match the course above.
        if record["body"] is not None:
            raise ContractError(
                f"capture {course['name']} not-modified body is not empty")
        if record["body_sha256"] != sha256_bytes(b""):
            raise ContractError(
                f"capture {course['name']} not-modified body digest mismatch")


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
    # Two generations live in this one assignment, and the older one is kept
    # deliberately. For every non-GraphQL operation this digests the raw bytes
    # the service returned, while the record stores their canonical
    # re-serialization; the four committed non-GraphQL captures carry exactly
    # that digest. Only the GraphQL branch below recomputes a digest of what it
    # actually stores. Tamper evidence for a minimized body therefore lives
    # outside the record, in `capture-bindings.json`. On the conditional path
    # the two generations coincide: an empty raw body and an empty minimized
    # body have the same digest.
    body_sha256 = sha256_bytes(body)
    if body:
        if operation == CONDITIONAL_OPERATION:
            raise ContractError(
                "conditional not-modified response carries a body")
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
# Conditional revalidation
#
# The conditional course asks GitHub whether a document already on record is
# still current, so its request has to carry that document's validators. They
# come from one committed capture, pinned by the authority by path and digest:
# the packet's own digest therefore binds the exact source bytes, and a source
# that has moved, changed, or lost its entity tag stops the run before it
# reserves anything. No ambient lookup, no most-recent-capture guessing.
# --------------------------------------------------------------------------

def load_condition_source(condition_source: dict, label: str) -> dict:
    """Load the pinned capture, accepting only its exact recorded bytes."""
    root = CONDITION_SOURCE_ROOT.resolve()
    path = root / condition_source["capture_file"]
    if root not in path.resolve().parents:
        raise ContractError(f"{label} condition source escapes the repository")
    payload = read_bounded(path, f"{label} condition source")
    if sha256_bytes(payload) != condition_source["file_sha256"]:
        raise ContractError(
            f"{label} condition source differs from its pinned digest")
    record = parse_json_unique(payload, f"{label} condition source")
    if not isinstance(record, dict):
        raise ContractError(f"{label} condition source is not an object")
    if record.get("schema_name") != CAPTURE_SCHEMA:
        raise ContractError(
            f"{label} condition source is not an anonymous capture record")
    version = require_positive_int(
        record.get("schema_version"),
        f"{label} condition source schema version",
        CAPTURE_SCHEMA_VERSION,
    )
    if version != CAPTURE_SCHEMA_VERSION:
        raise ContractError(f"{label} condition source schema version mismatch")
    headers = record.get("retained_headers")
    if minimize_headers(headers) != headers:
        raise ContractError(
            f"{label} condition source retained headers mismatch")
    return record


def conditional_request_headers(
    target: dict, label: str
) -> dict[str, str] | None:
    """Build one conditional request's validators, or nothing for any other."""
    if target["operation"] != CONDITIONAL_OPERATION:
        return None
    headers = load_condition_source(
        target[CONDITION_SOURCE_KEY], label)["retained_headers"]
    if "etag" not in headers:
        # Without an entity tag there is nothing exact to revalidate against,
        # and a modification date alone is a weaker request than the authority
        # describes. Refuse rather than quietly ask a different question.
        raise ContractError(f"{label} condition source records no etag")
    # `load_condition_source` already proved every retained value is bounded
    # text, so these go on the wire exactly as they were recorded.
    request_headers = {"If-None-Match": headers["etag"]}
    if "last-modified" in headers:
        request_headers["If-Modified-Since"] = headers["last-modified"]
    return request_headers


# --------------------------------------------------------------------------
# External commit binding
#
# `sanitize_capture` records `body_sha256` over the bytes the service returned,
# and for every operation except the GraphQL refusal those bytes no longer
# exist: the persisted `body` is the canonical re-serialization of them. The
# digest is therefore historically true and operationally inert — it cannot
# detect a later edit to the minimized body, because it was never a digest of
# the minimized body. The captures are byte-frozen evidence, so the fix cannot
# be to recompute inside them. It is layered outside instead, in one artifact
# that names, for every capture, its exact bytes, its exact minimized body, and
# the commit that owns it.
# --------------------------------------------------------------------------

def review_directory(packet: Path) -> Path:
    return packet.resolve(strict=True).parent


def binding_path(review: Path) -> Path:
    return review / BINDING_FILE_NAME


def synthetic_exhaustion_path(review: Path) -> Path:
    return review / SYNTHETIC_DIRECTORY_NAME / SYNTHETIC_EXHAUSTION_FILE


def contained_relative_name(path: Path, review: Path, label: str) -> str:
    try:
        return path.relative_to(review).as_posix()
    except ValueError as error:
        raise ContractError(
            f"{label} is not inside the packet's review directory") from error


def minimized_body_digest(record: Any, label: str) -> str:
    """Digest the persisted body exactly as it is stored.

    A null body is a real recorded outcome, not a missing value, so it digests
    the empty byte string rather than being skipped.
    """
    if not isinstance(record, dict):
        raise ContractError(f"{label} is not an object")
    body = record.get("body")
    if body is None:
        return sha256_bytes(b"")
    if not isinstance(body, str):
        raise ContractError(f"{label} body is neither text nor null")
    return sha256_bytes(body.encode("utf-8"))


def read_capture(root: Path, name: str) -> tuple[bytes, dict]:
    label = f"capture {name}"
    payload = read_bounded(root / f"{name}.json", label)
    record = parse_json_unique(payload, label)
    if not isinstance(record, dict):
        raise ContractError(f"{label} is not an object")
    return payload, record


def compute_capture_bindings(
    *, manifest: dict, root: Path, review: Path
) -> list[dict]:
    """Bind every manifest course to its committed bytes.

    Ordered by course name rather than by manifest order, so that reordering
    the manifest cannot change these bytes.
    """
    bindings = []
    for name in sorted(course["name"] for course in manifest["courses"]):
        label = f"capture {name}"
        capture_path = root / f"{name}.json"
        payload, record = read_capture(root, name)
        git_tracked_path(capture_path, label)
        require_committed_unmodified(capture_path, label)
        bindings.append({
            "course": name,
            "capture_file": contained_relative_name(
                capture_path, review, label),
            "file_sha256": sha256_bytes(payload),
            "minimized_body_sha256": minimized_body_digest(record, label),
            "owning_commit": git_owning_commit(capture_path, label),
        })
    return bindings


def compute_derived_bindings(review: Path) -> list[dict]:
    path = synthetic_exhaustion_path(review)
    label = "synthetic rate-exhaustion fixture"
    if not path.exists():
        return []
    payload = read_bounded(path, label)
    record = parse_json_unique(payload, label)
    return [{
        "artifact": SYNTHETIC_EXHAUSTION_NAME,
        "artifact_file": contained_relative_name(path, review, label),
        "file_sha256": sha256_bytes(payload),
        "minimized_body_sha256": minimized_body_digest(record, label),
    }]


def build_binding_record(
    *, packet_bytes: bytes, manifest: dict, root: Path, review: Path
) -> dict:
    return {
        "schema_name": BINDING_SCHEMA,
        "schema_version": BINDING_SCHEMA_VERSION,
        "packet_sha256": sha256_bytes(packet_bytes),
        "bindings": compute_capture_bindings(
            manifest=manifest, root=root, review=review),
        "derived_artifacts": compute_derived_bindings(review),
    }


def verify_one_binding(entry: dict, *, root: Path, review: Path) -> None:
    """Prove one capture's working bytes are bytes some commit already stored.

    ``owning_commit`` is written as the last commit that touched the file, but
    what is *checked* is weaker and deliberately so: that this commit stores
    exactly these bytes at exactly this path. Requiring it to still be the
    latest touching commit would fail on a later commit that restored identical
    content — a non-event — and would buy nothing against the threat, since an
    attacker who can commit tampered bytes can name their own commit either
    way. What the check forecloses is the cheap attack: rewriting a record and
    its own digests in the working tree, where no commit backs the result.
    """
    name = entry["course"]
    label = f"capture {name}"
    capture_path = root / f"{name}.json"
    if entry["capture_file"] != contained_relative_name(
        capture_path, review, label
    ):
        raise ContractError(f"{label} binding names another file")
    payload, record = read_capture(root, name)
    if require_sha256(
        entry["file_sha256"], f"{label} binding file_sha256"
    ) != sha256_bytes(payload):
        raise ContractError(f"{label} bytes differ from their bound digest")
    if require_sha256(
        entry["minimized_body_sha256"],
        f"{label} binding minimized_body_sha256",
    ) != minimized_body_digest(record, label):
        raise ContractError(
            f"{label} minimized body differs from its bound digest")
    commit = require_commit_id(
        entry["owning_commit"], f"{label} binding owning_commit")
    repository_path = git_tracked_path(capture_path, label)
    if git_committed_blob(capture_path, commit, repository_path, label) != payload:
        raise ContractError(
            f"{label} differs from the blob commit {commit[:12]} committed")


def verify_derived_bindings(
    entries: Any, *, review: Path
) -> None:
    if not isinstance(entries, list):
        raise ContractError("capture bindings derived_artifacts is not a list")
    path = synthetic_exhaustion_path(review)
    label = "synthetic rate-exhaustion fixture"
    present = [SYNTHETIC_EXHAUSTION_NAME] if path.exists() else []
    names = []
    for index, entry in enumerate(entries):
        require_exact_keys(
            entry, set(DERIVED_ENTRY_KEYS), f"derived artifact[{index}]")
        names.append(require_text(
            entry["artifact"], f"derived artifact[{index}].artifact", 64))
    if sorted(names) != present:
        raise ContractError(
            "capture bindings do not cover exactly the derived artifacts present")
    for entry in entries:
        payload = read_bounded(path, label)
        if entry["artifact_file"] != contained_relative_name(
            path, review, label
        ):
            raise ContractError("derived artifact binding names another file")
        if require_sha256(
            entry["file_sha256"], "derived artifact file_sha256"
        ) != sha256_bytes(payload):
            raise ContractError(f"{label} bytes differ from their bound digest")
        record = parse_json_unique(payload, label)
        if require_sha256(
            entry["minimized_body_sha256"],
            "derived artifact minimized_body_sha256",
        ) != minimized_body_digest(record, label):
            raise ContractError(
                f"{label} minimized body differs from its bound digest")


def verify_capture_bindings(
    *, packet_bytes: bytes, manifest: dict, root: Path, review: Path
) -> int | None:
    """Verify the external binding artifact, or report that there is none.

    ``None`` means the review directory predates this artifact. That is a fact
    the caller reports rather than swallows: an unbound review directory is
    exactly what this obligation existed to make visible.
    """
    path = binding_path(review)
    if path.is_symlink():
        raise ContractError("capture bindings file is a symlink")
    if not path.exists():
        return None
    payload = read_bounded(path, "capture bindings")
    record = parse_json_unique(payload, "capture bindings")
    require_exact_keys(record, set(BINDING_KEYS), "capture bindings")
    if record["schema_name"] != BINDING_SCHEMA:
        raise ContractError("capture bindings schema name mismatch")
    version = require_positive_int(
        record["schema_version"],
        "capture bindings schema version",
        BINDING_SCHEMA_VERSION,
    )
    if version != BINDING_SCHEMA_VERSION:
        raise ContractError("capture bindings schema version mismatch")
    if record["packet_sha256"] != sha256_bytes(packet_bytes):
        raise ContractError("capture bindings do not bind this packet")

    entries = record["bindings"]
    if not isinstance(entries, list) or not entries:
        raise ContractError("capture bindings name no captures")
    bound: list[str] = []
    for index, entry in enumerate(entries):
        require_exact_keys(
            entry, set(BINDING_ENTRY_KEYS), f"capture binding[{index}]")
        bound.append(require_text(
            entry["course"], f"capture binding[{index}].course", 64))
    expected = sorted(course["name"] for course in manifest["courses"])
    if sorted(bound) != expected:
        raise ContractError(
            "capture bindings course set differs from the course manifest")

    for entry in entries:
        verify_one_binding(entry, root=root, review=review)
    verify_derived_bindings(record["derived_artifacts"], review=review)
    return len(entries)


# --------------------------------------------------------------------------
# Derived synthetic primary-rate exhaustion
#
# RFC-BPR-0017 needs an exhausted-budget response to test refusal against, and
# the live course is forbidden from producing one: exhausting the shared 60/hour
# anonymous quota is the exact abuse the authority refuses. Nor can this be a
# sixth authorized course, since every authorized course must appear in a
# packet's authority block and no packet authorizes one.
#
# So it is neither captured nor authored. It is *derived*: a pure, deterministic
# function of three captures that are already committed, issuing no traffic and
# inventing no value. Each field below names the committed source it came from.
#
# One contract is deliberately absent. A secondary-limit `429` with a
# `Retry-After` header would need a retry delay, and no committed capture
# records one; fabricating a plausible number would put an unobserved value into
# evidence. This fixture covers primary exhaustion only.
# --------------------------------------------------------------------------

def require_synthetic_sources_validated(manifest: dict) -> None:
    """Derive only from captures this run has already validated in full."""
    names = {course["name"] for course in manifest["courses"]}
    missing = sorted(set(SYNTHETIC_SOURCE_COURSES) - names)
    if missing:
        raise ContractError(
            f"synthetic exhaustion needs validated courses {missing}")


def require_recorded_header(headers: Any, name: str, label: str) -> str:
    if not isinstance(headers, dict) or not isinstance(headers.get(name), str):
        raise ContractError(f"{label} records no {name} header")
    return headers[name]


def require_recorded_count(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise ContractError(f"{label} is not a recorded positive count")
    return value


def derive_rate_exhaustion_record(root: Path) -> dict:
    """Build the synthetic exhausted-core record from committed captures only."""
    _budget_bytes, budget = read_capture(root, SYNTHETIC_BUDGET_COURSE)
    _refusal_bytes, refusal = read_capture(root, SYNTHETIC_REFUSAL_COURSE)
    _request_bytes, request = read_capture(root, SYNTHETIC_REQUEST_COURSE)

    # One authority, or the three captures are not one recorded session.
    authority_sha256 = require_sha256(
        budget.get("authority_sha256"), "rate-limit capture authority_sha256")
    if (refusal.get("authority_sha256") != authority_sha256
            or request.get("authority_sha256") != authority_sha256):
        raise ContractError(
            "synthetic exhaustion sources do not share one authority")

    # Budget values: the real anonymous core allowance and its real reset.
    budget_body = budget.get("body")
    if not isinstance(budget_body, str):
        raise ContractError("rate-limit capture records no body")
    resources = parse_json_unique(
        budget_body, "rate-limit capture body").get("resources")
    if not isinstance(resources, dict):
        raise ContractError("rate-limit capture body records no resources")
    core = resources.get(SYNTHETIC_EXHAUSTED_RESOURCE)
    if not isinstance(core, dict):
        raise ContractError(
            f"rate-limit capture body records no {SYNTHETIC_EXHAUSTED_RESOURCE} "
            "resource")
    core_limit = require_recorded_count(
        core.get("limit"), "rate-limit capture core limit")
    core_reset = require_recorded_count(
        core.get("reset"), "rate-limit capture core reset")

    # Refusal shape: the only exhausted refusal GitHub actually returned here.
    refusal_status = refusal.get("http_status")
    if refusal_status != 403:
        raise ContractError(
            "graphql-refusal capture is not the recorded 403 refusal")
    refusal_headers = refusal.get("retained_headers")
    if require_recorded_header(
        refusal_headers, "x-ratelimit-remaining", "graphql-refusal capture"
    ) != EXHAUSTED_REMAINING:
        raise ContractError(
            "graphql-refusal capture does not record a zero-remaining budget")
    refusal_body = refusal.get("body")
    if not isinstance(refusal_body, str) or not refusal_body:
        raise ContractError("graphql-refusal capture records no body")

    # Request identity: the recorded core-charged request this answers.
    request_headers = request.get("retained_headers")
    if require_recorded_header(
        request_headers, "x-ratelimit-resource", "pull-request capture"
    ) != SYNTHETIC_EXHAUSTED_RESOURCE:
        raise ContractError(
            "pull-request capture does not record the core resource")

    headers = {
        # From the recorded refusal template.
        "content-type": require_recorded_header(
            refusal_headers, "content-type", "graphql-refusal capture"),
        "x-ratelimit-remaining": EXHAUSTED_REMAINING,
        # From the recorded core budget. Exhaustion is `used == limit`; that is
        # the definition of the state, not an extra number.
        "x-ratelimit-limit": str(core_limit),
        "x-ratelimit-used": str(core_limit),
        "x-ratelimit-reset": str(core_reset),
        # From the recorded core-charged request.
        "x-ratelimit-resource": request_headers["x-ratelimit-resource"],
    }
    media_type = refusal_headers.get("x-github-media-type")
    if isinstance(media_type, str):
        headers["x-github-media-type"] = media_type

    # The body is the recorded refusal template verbatim. It names no resource
    # — GitHub carries resource identity in the headers, which is where the
    # substitution above happens — so rewriting its prose to say "core" would
    # put a sentence GitHub never sent into evidence. Its caller-address
    # placeholder is inherited from the recorded normalization; this record
    # issues no request, so it has no caller.
    body = refusal_body.encode("utf-8")
    record = sanitize_capture(
        name=SYNTHETIC_EXHAUSTION_NAME,
        operation=request["operation"],
        url=request["url"],
        http_status=refusal_status,
        headers=headers,
        body=body,
        authority_sha256=authority_sha256,
        source_kind="synthetic",
    )
    # The source body is already canonical, so the sanitizer's raw-byte digest
    # and the canonical-body digest coincide here. Assert it rather than assume
    # it: this record's digest must be a digest of what it stores.
    if record["body_sha256"] != sha256_bytes(record["body"].encode("utf-8")):
        raise ContractError(
            "synthetic exhaustion body digest is not a digest of its own body")
    return record


def verify_synthetic_exhaustion(root: Path, review: Path, manifest: dict) -> int:
    """Re-derive the fixture and require the stored bytes to match exactly."""
    path = synthetic_exhaustion_path(review)
    label = "synthetic rate-exhaustion fixture"
    if path.is_symlink():
        raise ContractError(f"{label} is a symlink")
    if not path.exists():
        return 0
    payload = read_bounded(path, label)
    assert_no_forbidden_bytes(payload, label)
    require_synthetic_sources_validated(manifest)
    if payload != canonical_json(derive_rate_exhaustion_record(root)):
        raise ContractError(
            f"{label} does not re-derive from the committed captures")
    return 1


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


def perform_request(
    url: str,
    operation: str,
    body: bytes | None,
    request_headers: dict[str, str] | None = None,
):
    """Issue one credential-free request. Imported lazily on purpose."""
    import urllib.error
    import urllib.request

    method, allows_body = ALLOWED_OPERATIONS[operation]
    if body is not None and not allows_body:
        raise ContractError(f"{operation} may not carry a request body")
    if request_headers and operation != CONDITIONAL_OPERATION:
        raise ContractError(
            f"{operation} may not carry conditional request headers")
    validate_url(url, operation, "request")

    request = urllib.request.Request(url, data=body, method=method)
    request.add_header("Accept", "application/vnd.github+json")
    request.add_header("X-GitHub-Api-Version", "2022-11-28")
    request.add_header("User-Agent", "BetterPullReview-RFC0017-anonymous-probe")
    for name, value in (request_headers or {}).items():
        if name not in CONDITIONAL_VALIDATOR_HEADERS:
            raise ContractError(f"request header {name!r} is not a validator")
        request.add_header(name, value)
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
    authorized = {target["name"]: target for target in authority["targets"]}
    seen: set[str] = set()
    for index, course in enumerate(courses):
        require_exact_keys(course, {"name", "expected_status"}, f"course[{index}]")
        name = require_text(course["name"], f"course[{index}].name", 64)
        if name not in authorized:
            raise ContractError(f"course {name} is not an authorized target")
        if name in seen:
            raise ContractError(f"course manifest repeats course {name}")
        seen.add(name)
        expected_status = require_positive_int(
            course["expected_status"], f"course[{index}].expected_status", 599)
        # The conditional course exists to record a not-modified answer. A
        # course that expects anything else is asking a different question
        # under this operation's name.
        if (authorized[name]["operation"] == CONDITIONAL_OPERATION
                and expected_status != CONDITIONAL_EXPECTED_STATUS):
            raise ContractError(
                f"course {name} must expect {CONDITIONAL_EXPECTED_STATUS}")
    return manifest


def validate_recorded_state(args: argparse.Namespace) -> dict:
    """Everything `--validate-existing` proves. No traffic, no writes."""
    packet_bytes, _response, authority, manifest, authority_sha = load_inputs(args)
    if manifest["packet_sha256"] != sha256_bytes(packet_bytes):
        raise ContractError("course manifest does not bind this packet")
    review = review_directory(Path(args.packet))
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

    derived = verify_synthetic_exhaustion(root, review, manifest)
    bound = verify_capture_bindings(
        packet_bytes=packet_bytes, manifest=manifest, root=root, review=review)
    return {
        "packet_bytes": packet_bytes,
        "manifest": manifest,
        "review": review,
        "root": root,
        "checked": checked,
        "derived": derived,
        "bound": bound,
    }


def run_validate_existing(args: argparse.Namespace) -> int:
    state = validate_recorded_state(args)
    print(f"RFC0017_ANONYMOUS_CONTRACTS_VALID:{state['checked']}")
    if state["bound"] is None:
        # Older review directories carry no binding artifact. Say so out loud:
        # closeout must be able to see which evidence is commit-bound.
        print("RFC0017_ANONYMOUS_CONTRACTS_BINDINGS_ABSENT")
    else:
        print(f"RFC0017_ANONYMOUS_CONTRACTS_BOUND:{state['bound']}")
    print(f"RFC0017_ANONYMOUS_CONTRACTS_DERIVED:{state['derived']}")
    return 0


def run_write_bindings(args: argparse.Namespace) -> int:
    state = validate_recorded_state(args)
    record = build_binding_record(
        packet_bytes=state["packet_bytes"], manifest=state["manifest"],
        root=state["root"], review=state["review"])
    write_artifact_once(
        binding_path(state["review"]), canonical_json(record), "capture bindings")
    print(f"RFC0017_ANONYMOUS_CONTRACTS_BOUND:{len(record['bindings'])}")
    return 0


def run_synthesize_exhaustion(args: argparse.Namespace) -> int:
    state = validate_recorded_state(args)
    require_synthetic_sources_validated(state["manifest"])
    payload = canonical_json(derive_rate_exhaustion_record(state["root"]))
    write_artifact_once(
        synthetic_exhaustion_path(state["review"]), payload,
        "synthetic rate-exhaustion fixture")
    print("RFC0017_ANONYMOUS_CONTRACTS_DERIVED:1")
    return 0


def run_capture(args: argparse.Namespace) -> int:
    packet_bytes, response_bytes, authority, manifest, authority_sha = load_inputs(
        args)
    if manifest["packet_sha256"] != sha256_bytes(packet_bytes):
        raise ContractError("course manifest does not bind this packet")
    validate_exact_capture_response(
        packet_bytes, response_bytes, authority["statement"])
    validate_current_recorder_binding(packet_bytes)
    validate_packet_course_selection(packet_bytes, manifest)
    root = resolve_output_root(Path(args.output_root), Path(args.packet))
    targets = {target["name"]: target for target in authority["targets"]}
    # Load and authenticate every pinned condition source before anything is
    # written or reserved. A request whose validators cannot be proved must not
    # consume the packet's single-use authorization.
    validators = {
        course["name"]: conditional_request_headers(
            targets[course["name"]], f"course {course['name']}")
        for course in manifest["courses"]
    }

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
                target["url"], target["operation"], None,
                validators[target["name"]])
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
    parser.add_argument(
        "--write-bindings", action="store_true",
        help="perform no traffic; revalidate, then write capture-bindings.json")
    parser.add_argument(
        "--synthesize-exhaustion", action="store_true",
        help="perform no traffic; derive the synthetic rate-exhaustion fixture")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    modes = (
        (args.validate_existing, run_validate_existing),
        (args.write_bindings, run_write_bindings),
        (args.synthesize_exhaustion, run_synthesize_exhaustion),
    )
    try:
        selected = [runner for enabled, runner in modes if enabled]
        if len(selected) > 1:
            raise ContractError("choose at most one no-traffic mode")
        if selected:
            return selected[0](args)
        return run_capture(args)
    except ContractError as error:
        sys.stderr.write(f"RFC0017_ANONYMOUS_CONTRACTS_REFUSED:{error}\n")
        return 3


if __name__ == "__main__":
    raise SystemExit(main())
