#!/usr/bin/env python3
"""Validate the positive RFC-BPR-0017 public-preview build decision.

BPR-0299 already decided the product question: build anonymous public preview
as a credential-optional mode on the existing read path. What that acceptance
did not carry is the four definitions the subject asked for — the
public-preview name, how the shared hourly budget and the missing capabilities
are presented, the sign-in handoff, and the supported-platform policy. BPR-0312
binds those, and this module is the gate that decides whether they are bound.

Two modes, neither of which touches a network and neither of which reads a
credential:

* default — validate the packet, the offline review page it was built beside,
  and the owner's exact ``bpr.attended_review_verdict``, then write the
  validation record once.
* ``--validate-existing`` — write nothing at all and revalidate the response
  and the record already on disk, so closeout can re-prove the gate at the
  immutable final head.

The module imports no network client at any point. ``subprocess`` is imported
at module scope because the only child process it ever launches is a local
read-only ``git`` query — ``rev-parse`` and ``cat-file`` — neither of which
contacts a remote.

What this gate is not: it is not RFC-BPR-0017 closeout, it does not start an
implementation card, and it grants no network, credential, private-read,
mutation, release, or platform-enablement authority.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any

# Mirrors scripts/verify_documentation.py. Keep this block free of syntax newer
# than the version it refuses, or it cannot report anything.
MINIMUM_PYTHON = (3, 10)
if sys.version_info < MINIMUM_PYTHON:
    sys.stderr.write(
        "validate_rfc0017_public_preview_decision.py requires Python {}.{} or "
        "newer; this is {}.{}.{} at {}.\n".format(
            MINIMUM_PYTHON[0], MINIMUM_PYTHON[1], sys.version_info[0],
            sys.version_info[1], sys.version_info[2], sys.executable))
    raise SystemExit(2)


class DecisionError(Exception):
    """A refusal. Every one of these leaves the tree unchanged."""


# --------------------------------------------------------------------------
# Fixed identities. The packet names values *within* these; it cannot widen
# them. A packet asking for a different decision, or a response carrying a
# different schema, is refused before its digest is considered.
# --------------------------------------------------------------------------

DEFINITIONS_SCHEMA = "bpr.rfc0017.public_preview_definitions"
DEFINITIONS_SCHEMA_VERSION = 1
VERDICT_SCHEMA = "bpr.attended_review_verdict"
VERDICT_SCHEMA_VERSION = 1
MANIFEST_SCHEMA = "bpr.attended_review_batch"
MANIFEST_SCHEMA_VERSION = 1
RECORD_SCHEMA = "bpr.rfc0017.public_preview_decision_record"
RECORD_SCHEMA_VERSION = 1

#: The only decision this gate can record. BPR-0312 exists to validate the
#: positive branch; a rejection leaves the gate unresolved rather than
#: producing a softer record of itself.
REQUIRED_DECISION = "build"

#: The four definitions BPR-0299's acceptance owed, in the order the packet
#: proposes them and the record binds them.
DEFINITION_KEYS = (
    "public_preview_name",
    "limit_presentation",
    "sign_in_handoff",
    "supported_platform_policy",
)

#: The note that accepts the packet's proposed text unchanged. An empty note
#: means the same thing: the page omits a note key entirely when the textarea
#: is blank, so "silent" and "as proposed" have to agree or a blank textarea
#: would refuse a decision the owner did make.
AS_PROPOSED_NOTE = "as proposed"

VALIDATOR_REPOSITORY_PATH = "scripts/validate_rfc0017_public_preview_decision.py"

MAX_ARTIFACT_BYTES = 1 << 20
MAX_DEFINITION_CHARS = 4000
MAX_NOTE_CHARS = 20000
MAX_EVIDENCE_ENTRIES = 64

GIT_EXECUTABLE = "git"
GIT_ENVIRONMENT = {
    "GIT_OPTIONAL_LOCKS": "0",
    "GIT_TERMINAL_PROMPT": "0",
    "GIT_PAGER": "cat",
    "LC_ALL": "C",
}

FENCED_JSON_BLOCK = re.compile(r"```json\s+(\{.*?\})\s*```", re.S)
#: A note line that assigns one definition. Anything else at the start of a
#: line that *looks* like an assignment is refused rather than folded into the
#: previous definition: a typo in a key would otherwise silently become part of
#: the preceding answer.
NOTE_KEY_LINE = re.compile(r"^([a-z][a-z0-9_]{2,63})[ \t]*:[ \t]*(.*)$")
#: Refused wherever it appears in the note. BPR-0299 already chose to build;
#: a note that reverses that is a different decision than the one this packet
#: asks for, and it must not be absorbed as definition prose.
DO_NOT_BUILD = re.compile(r"\bdo\s+not\s+build\b", re.IGNORECASE)

RFC3339_UTC = re.compile(
    r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?"
    r"(?:Z|[+-]\d{2}:\d{2})$")


# --------------------------------------------------------------------------
# Exact helpers
# --------------------------------------------------------------------------

def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def prefixed_digest(payload: bytes) -> str:
    return f"sha256:{sha256_bytes(payload)}"


def canonical_json(value: Any) -> bytes:
    """One serialization, so a digest over it means something."""
    return json.dumps(
        value, ensure_ascii=False, sort_keys=True, separators=(",", ":"),
    ).encode("utf-8") + b"\n"


def readable_json(value: Any) -> bytes:
    """The record is read by people as well as by this module."""
    return json.dumps(value, ensure_ascii=False, indent=2).encode("utf-8") + b"\n"


def parse_json_unique(payload: bytes | str, label: str) -> Any:
    """Parse JSON without allowing an earlier object member to disappear."""
    def build_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        parsed: dict[str, Any] = {}
        for key, value in pairs:
            if key in parsed:
                raise DecisionError(f"{label} contains duplicate JSON key")
            parsed[key] = value
        return parsed

    try:
        return json.loads(payload, object_pairs_hook=build_object)
    except json.JSONDecodeError as error:
        raise DecisionError(f"{label} is not JSON") from error


def require_object(value: Any, label: str) -> dict:
    if not isinstance(value, dict):
        raise DecisionError(f"{label} is not an object")
    return value


def require_exact_keys(value: Any, expected: set[str], label: str) -> dict:
    require_object(value, label)
    actual = set(value)
    if actual != expected:
        missing = sorted(expected - actual)
        unexpected = sorted(actual - expected)
        raise DecisionError(
            f"{label} key mismatch: missing={missing} unexpected={unexpected}")
    return value


def require_text(value: Any, label: str, limit: int = 256) -> str:
    if not isinstance(value, str) or not value or len(value) > limit:
        raise DecisionError(f"{label} is not bounded text")
    if any(ord(ch) < 0x20 or ord(ch) == 0x7F for ch in value):
        raise DecisionError(f"{label} contains a control character")
    return value


def require_prose(value: Any, label: str, limit: int) -> str:
    """Bounded text that may wrap. Newlines and tabs only."""
    if not isinstance(value, str) or not value.strip() or len(value) > limit:
        raise DecisionError(f"{label} is not bounded prose")
    for ch in value:
        if ord(ch) < 0x20 and ch not in "\n\t":
            raise DecisionError(f"{label} contains a control character")
        if ord(ch) == 0x7F:
            raise DecisionError(f"{label} contains a control character")
    return value


def require_positive_int(value: Any, label: str, maximum: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise DecisionError(f"{label} is not an integer")
    if value <= 0 or value > maximum:
        raise DecisionError(f"{label} is out of range")
    return value


def require_prefixed_sha256(value: Any, label: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(
        r"sha256:[0-9a-f]{64}", value
    ):
        raise DecisionError(f"{label} is not a prefixed SHA-256 digest")
    return value


def require_commit_id(value: Any, label: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(
        r"[0-9a-f]{40}(?:[0-9a-f]{24})?", value
    ):
        raise DecisionError(f"{label} is not a full commit identifier")
    return value


def read_bounded(path: Path, label: str, limit: int = MAX_ARTIFACT_BYTES) -> bytes:
    if path.is_symlink():
        raise DecisionError(f"{label} is a symlink")
    if not path.is_file():
        raise DecisionError(f"{label} is not a regular file")
    if path.stat().st_size > limit:
        raise DecisionError(f"{label} exceeds {limit} bytes")
    return path.read_bytes()


# --------------------------------------------------------------------------
# Local Git evidence. Every call is a local read; none contacts a remote.
# --------------------------------------------------------------------------

def run_git(arguments: list[str], *, cwd: Path, label: str) -> bytes:
    environment = dict(os.environ)
    environment.update(GIT_ENVIRONMENT)
    try:
        completed = subprocess.run(
            [GIT_EXECUTABLE, "-C", str(cwd), *arguments],
            capture_output=True, check=False, env=environment)
    except OSError as error:
        raise DecisionError(
            f"{label} needs git, which could not be run") from error
    if completed.returncode != 0:
        raise DecisionError(f"{label} could not be read from git")
    return completed.stdout


def repository_root(anchor: Path) -> Path:
    output = run_git(
        ["rev-parse", "--show-toplevel"], cwd=anchor, label="repository root")
    return Path(output.decode("utf-8", "replace").strip()).resolve()


def head_commit(root: Path) -> str:
    output = run_git(["rev-parse", "HEAD"], cwd=root, label="head commit")
    return require_commit_id(
        output.decode("utf-8", "replace").strip(), "head commit")


def require_commit_present(root: Path, commit: str, label: str) -> None:
    run_git(
        ["cat-file", "-e", f"{commit}^{{commit}}"], cwd=root, label=label)


def repository_relative(path: Path, root: Path, label: str) -> str:
    try:
        return path.resolve().relative_to(root).as_posix()
    except ValueError as error:
        raise DecisionError(f"{label} is outside the repository") from error


# --------------------------------------------------------------------------
# The packet
# --------------------------------------------------------------------------

def parse_definitions_block(packet_bytes: bytes) -> dict:
    """Extract the one block the owner is being asked to bind."""
    try:
        text = packet_bytes.decode("utf-8")
    except UnicodeDecodeError as error:
        raise DecisionError("packet is not UTF-8") from error
    blocks = [
        parse_json_unique(block, f"packet JSON block[{index}]")
        for index, block in enumerate(FENCED_JSON_BLOCK.findall(text))
    ]
    matches = [
        block for block in blocks
        if isinstance(block, dict)
        and block.get("schema_name") == DEFINITIONS_SCHEMA
    ]
    if len(matches) != 1:
        raise DecisionError(
            "packet must carry exactly one public-preview definitions block, "
            f"found {len(matches)}")
    block = require_exact_keys(
        matches[0],
        {
            "schema_name", "schema_version", "decision", "batch_id",
            "subject_id", "subject_digest", "definitions", "probe_evidence",
            "decision_evidence",
        },
        "definitions block")
    version = require_positive_int(
        block["schema_version"], "definitions block schema version",
        DEFINITIONS_SCHEMA_VERSION)
    if version != DEFINITIONS_SCHEMA_VERSION:
        raise DecisionError("definitions block schema version mismatch")
    decision = require_text(block["decision"], "definitions block decision", 64)
    if decision != REQUIRED_DECISION:
        raise DecisionError(
            f"packet records decision {decision!r}; this gate validates only "
            f"the {REQUIRED_DECISION!r} branch")
    require_text(block["batch_id"], "definitions block batch_id", 128)
    require_text(block["subject_id"], "definitions block subject_id", 256)
    require_prefixed_sha256(
        block["subject_digest"], "definitions block subject_digest")
    block["definitions"] = parse_packet_definitions(block["definitions"])
    block["probe_evidence"] = parse_evidence(
        block["probe_evidence"], "probe_evidence")
    block["decision_evidence"] = parse_evidence(
        block["decision_evidence"], "decision_evidence")
    return block


def parse_packet_definitions(raw: Any) -> list[dict]:
    if not isinstance(raw, list) or len(raw) != len(DEFINITION_KEYS):
        raise DecisionError(
            f"packet must propose exactly {len(DEFINITION_KEYS)} definitions")
    definitions: list[dict] = []
    for index, entry in enumerate(raw):
        label = f"packet definition[{index}]"
        require_exact_keys(
            entry, {"key", "term", "question", "proposed"}, label)
        key = require_text(entry["key"], f"{label}.key", 64)
        if key != DEFINITION_KEYS[index]:
            raise DecisionError(
                f"{label}.key is {key!r}; the packet must propose "
                f"{DEFINITION_KEYS[index]!r} in this position")
        definitions.append({
            "key": key,
            "term": require_text(entry["term"], f"{label}.term", 128),
            "question": require_prose(
                entry["question"], f"{label}.question", MAX_DEFINITION_CHARS),
            "proposed": require_prose(
                entry["proposed"], f"{label}.proposed", MAX_DEFINITION_CHARS),
        })
    return definitions


def parse_evidence(raw: Any, label: str) -> list[dict]:
    if not isinstance(raw, list) or not raw or len(raw) > MAX_EVIDENCE_ENTRIES:
        raise DecisionError(f"packet {label} is not a bounded non-empty list")
    entries: list[dict] = []
    seen: set[str] = set()
    for index, entry in enumerate(raw):
        item = f"packet {label}[{index}]"
        require_exact_keys(entry, {"path", "sha256", "byte_length"}, item)
        path = require_text(entry["path"], f"{item}.path", 256)
        if path.startswith("/") or ".." in Path(path).parts:
            raise DecisionError(f"{item}.path is not a repository-relative path")
        if path in seen:
            raise DecisionError(f"packet {label} repeats {path}")
        seen.add(path)
        entries.append({
            "path": path,
            "sha256": require_prefixed_sha256(entry["sha256"], f"{item}.sha256"),
            "byte_length": require_positive_int(
                entry["byte_length"], f"{item}.byte_length", MAX_ARTIFACT_BYTES),
        })
    return entries


def verify_evidence(entries: list[dict], root: Path, label: str) -> list[dict]:
    """Re-read every bound file. A digest nobody recomputes proves nothing."""
    for entry in entries:
        path = (root / entry["path"]).resolve()
        repository_relative(path, root, f"{label} {entry['path']}")
        payload = read_bounded(path, f"{label} {entry['path']}")
        if prefixed_digest(payload) != entry["sha256"]:
            raise DecisionError(
                f"{label} {entry['path']} is {prefixed_digest(payload)} but the "
                f"packet binds {entry['sha256']}")
        if len(payload) != entry["byte_length"]:
            raise DecisionError(
                f"{label} {entry['path']} is {len(payload)} bytes but the "
                f"packet binds {entry['byte_length']}")
    return entries


# --------------------------------------------------------------------------
# The offline review page the packet was built beside
#
# The packet carries the batch identity and the proposed text; the page carries
# what the owner actually saw. Neither alone is enough. The page is found from
# the packet's own directory rather than from a flag, so the two cannot be
# paired across review directories by mistake.
# --------------------------------------------------------------------------

def review_directory(packet_path: Path) -> Path:
    directory = packet_path.resolve().parent
    if not directory.is_dir():
        raise DecisionError("packet is not inside a review directory")
    return directory


def manifest_path(review: Path) -> Path:
    return review / "offline-package" / "attended-review-manifest.json"


def load_review_manifest(manifest_bytes: bytes, block: dict) -> dict:
    manifest = require_object(
        parse_json_unique(manifest_bytes, "review manifest"), "review manifest")
    if manifest.get("schema_name") != MANIFEST_SCHEMA:
        raise DecisionError("review manifest schema name mismatch")
    if manifest.get("schema_version") != MANIFEST_SCHEMA_VERSION:
        raise DecisionError("review manifest schema version mismatch")
    if manifest.get("batch_id") != block["batch_id"]:
        raise DecisionError(
            "review manifest does not carry the batch the packet names")
    subjects = manifest.get("subjects")
    if not isinstance(subjects, list) or len(subjects) != 1:
        raise DecisionError(
            "this gate is one bounded decision; the review manifest must hold "
            "exactly one subject")
    subject = require_object(subjects[0], "review manifest subject")
    if subject.get("subject_id") != block["subject_id"]:
        raise DecisionError(
            "review manifest subject is not the subject the packet names")
    if subject.get("subject_digest") != block["subject_digest"]:
        raise DecisionError(
            "review manifest subject digest differs from the packet; the "
            "packet and the page the owner saw are not the same question")
    audit = subject.get("audit_verdict")
    if isinstance(audit, dict) and audit.get("status") == "fail":
        raise DecisionError(
            "the review subject failed its mechanical audit and cannot be "
            "accepted")
    verify_manifest_definitions(subject, block)
    for field in ("review_gate", "review_scope"):
        require_text(manifest.get(field), f"review manifest {field}", 128)
    claims = require_object(
        manifest.get("downstream_claims"), "review manifest downstream_claims")
    if not claims or any(value is not False for value in claims.values()):
        raise DecisionError(
            "review manifest downstream_claims must be a non-empty false-only "
            "map")
    return manifest


def verify_manifest_definitions(subject: dict, block: dict) -> None:
    """The page must have shown exactly the text the packet proposes."""
    sections = subject.get("detail_sections")
    if not isinstance(sections, list):
        raise DecisionError("review manifest subject has no detail sections")
    expected = [
        {"term": entry["term"], "definition": entry["proposed"]}
        for entry in block["definitions"]
    ]
    matches = [
        section for section in sections
        if isinstance(section, dict)
        and section.get("kind") == "definitions"
        and section.get("items") == expected
    ]
    if len(matches) != 1:
        raise DecisionError(
            "the review page does not present the packet's four proposed "
            "definitions verbatim exactly once")


# --------------------------------------------------------------------------
# The owner's response
# --------------------------------------------------------------------------

VERDICT_REQUIRED_FIELDS = frozenset({
    "schema_name",
    "schema_version",
    "batch_id",
    "review_gate",
    "review_scope",
    "accepted_subjects",
    "rejected_subjects",
    "notes_by_subject_identity",
    "downstream_claims",
})
VERDICT_OPTIONAL_FIELDS = frozenset({"not_chosen_subjects"})


def identity_tuple(value: Any, label: str) -> tuple[str, str]:
    require_exact_keys(value, {"subject_id", "subject_digest"}, label)
    return (
        require_text(value["subject_id"], f"{label}.subject_id", 256),
        require_prefixed_sha256(
            value["subject_digest"], f"{label}.subject_digest"),
    )


def identity_list(verdict: dict, field: str) -> list[tuple[str, str]]:
    values = verdict.get(field, [])
    if not isinstance(values, list):
        raise DecisionError(f"response {field} is not an array")
    return [
        identity_tuple(value, f"response {field}[{index}]")
        for index, value in enumerate(values)
    ]


def validate_response(
    response_bytes: bytes, block: dict, manifest: dict
) -> str:
    """Return the owner's verbatim note, or refuse."""
    verdict = require_object(
        parse_json_unique(response_bytes, "response"), "response")
    actual = set(verdict)
    if not VERDICT_REQUIRED_FIELDS <= actual:
        missing = sorted(VERDICT_REQUIRED_FIELDS - actual)
        raise DecisionError(
            "response is a partial verdict; it is missing "
            f"{missing}")
    if not actual <= (VERDICT_REQUIRED_FIELDS | VERDICT_OPTIONAL_FIELDS):
        unexpected = sorted(actual - VERDICT_REQUIRED_FIELDS
                            - VERDICT_OPTIONAL_FIELDS)
        raise DecisionError(f"response carries unsupported fields {unexpected}")
    if verdict["schema_name"] != VERDICT_SCHEMA:
        raise DecisionError(
            f"response is not a {VERDICT_SCHEMA}; this gate accepts only the "
            "exported attended-review verdict")
    if verdict["schema_version"] != VERDICT_SCHEMA_VERSION:
        raise DecisionError("response verdict schema version mismatch")
    for field in ("batch_id", "review_gate", "review_scope"):
        if verdict[field] != manifest.get(field):
            raise DecisionError(
                f"response {field} does not match the review batch")
    if verdict["batch_id"] != block["batch_id"]:
        raise DecisionError("response answers a different batch than the packet")
    if verdict["downstream_claims"] != manifest.get("downstream_claims"):
        raise DecisionError(
            "response downstream_claims differ from the manifest's declared "
            "false-only claim set")

    identity = (block["subject_id"], block["subject_digest"])
    accepted = identity_list(verdict, "accepted_subjects")
    rejected = identity_list(verdict, "rejected_subjects")
    not_chosen = identity_list(verdict, "not_chosen_subjects")
    decided = accepted + rejected + not_chosen
    if len(decided) != len(set(decided)):
        raise DecisionError("response decides a subject more than once")
    if set(decided) != {identity}:
        raise DecisionError(
            "response does not decide exactly this packet's subject; it "
            f"decides {sorted(subject_id for subject_id, _ in decided)}")
    if identity in rejected:
        raise DecisionError(
            "the owner rejected the public-preview definitions; BPR-0312 stays "
            "unresolved and every implementation card stays blocked")
    if identity in not_chosen:
        raise DecisionError(
            "the response records this subject as not chosen; a single bounded "
            "decision has no unchosen alternative")
    if identity not in accepted:
        raise DecisionError("response does not accept this packet's subject")

    notes = require_object(
        verdict["notes_by_subject_identity"], "response notes")
    expected_key = f"{identity[0]}@{identity[1]}"
    for key, value in notes.items():
        if key != expected_key or not isinstance(value, str):
            raise DecisionError(f"response carries an invalid note entry {key!r}")
    note = notes.get(expected_key, "")
    if len(note) > MAX_NOTE_CHARS:
        raise DecisionError("response note exceeds the byte limit")
    return note


def parse_definition_overrides(note: str) -> dict[str, str]:
    """Read the note as zero or more ``key: text`` definition assignments."""
    entries: list[tuple[str, list[str]]] = []
    seen: set[str] = set()
    for line in note.splitlines():
        match = NOTE_KEY_LINE.match(line)
        if match:
            key = match.group(1)
            if key not in DEFINITION_KEYS:
                raise DecisionError(
                    f"response note names an unknown definition {key!r}; the "
                    f"four keys are {list(DEFINITION_KEYS)}")
            if key in seen:
                raise DecisionError(
                    f"response note repeats definition {key!r}")
            seen.add(key)
            entries.append((key, [match.group(2)]))
            continue
        if not entries:
            if line.strip():
                raise DecisionError(
                    "response note is not a definition assignment; answer with "
                    f"an empty note, {AS_PROPOSED_NOTE!r}, or one or more "
                    "'<definition>: <text>' lines")
            continue
        entries[-1][1].append(line)
    if not entries:
        raise DecisionError("response note carries no definition assignment")
    overrides: dict[str, str] = {}
    for key, lines in entries:
        value = "\n".join(lines).strip()
        if not value:
            raise DecisionError(
                f"response note names definition {key!r} but leaves it empty")
        require_prose(value, f"response definition {key!r}", MAX_DEFINITION_CHARS)
        overrides[key] = value
    return overrides


def bind_definitions(note: str, block: dict) -> list[dict]:
    """Bind the packet's proposals, replaced only where the note replaces one."""
    if DO_NOT_BUILD.search(note):
        raise DecisionError(
            "response note carries a 'do not build' verdict; BPR-0299 already "
            "decided to build and this gate cannot record its reversal")
    stripped = note.strip()
    if not stripped or stripped.casefold() == AS_PROPOSED_NOTE:
        overrides: dict[str, str] = {}
    else:
        overrides = parse_definition_overrides(note)
    bound: list[dict] = []
    for entry in block["definitions"]:
        key = entry["key"]
        replacement = overrides.get(key)
        bound.append({
            "key": key,
            "term": entry["term"],
            "question": entry["question"],
            "proposed": entry["proposed"],
            "bound": replacement if replacement is not None else entry["proposed"],
            "source": "response_note" if replacement is not None else "packet",
        })
    if len(bound) != len(DEFINITION_KEYS) or any(
        not entry["bound"].strip() for entry in bound
    ):
        raise DecisionError("the four bound definitions are not all non-empty")
    return bound


# --------------------------------------------------------------------------
# The record
# --------------------------------------------------------------------------

def artifact_triple(path: Path, root: Path, payload: bytes, label: str) -> dict:
    return {
        "path": repository_relative(path, root, label),
        "sha256": prefixed_digest(payload),
        "byte_length": len(payload),
    }


def recorded_at_now() -> str:
    return dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat()


def validation_command(
    root: Path, packet: Path, response: Path, require: str, output: Path
) -> str:
    """Reproducible regardless of the directory the validator ran from."""
    return (
        f"python3 {VALIDATOR_REPOSITORY_PATH}"
        f" --packet {repository_relative(packet, root, 'packet')}"
        f" --response {repository_relative(response, root, 'response')}"
        f" --require {require}"
        f" --output {repository_relative(output, root, 'output')}"
    )


def build_record(
    *,
    root: Path,
    packet: Path,
    packet_bytes: bytes,
    response: Path,
    response_bytes: bytes,
    output: Path,
    require: str,
    review: Path,
    manifest: dict,
    manifest_bytes: bytes,
    block: dict,
    note: str,
    definitions: list[dict],
    commit: str,
    recorded_at: str,
) -> dict:
    validator = Path(__file__).resolve()
    return {
        "schema_name": RECORD_SCHEMA,
        "schema_version": RECORD_SCHEMA_VERSION,
        "recorded_at": recorded_at,
        "validated_commit": commit,
        "validation_command": validation_command(
            root, packet, response, require, output),
        "required_decision": require,
        "decision": block["decision"],
        "result": (
            "accepted: the owner's exact verdict approves building the measured "
            "public-preview mode and binds all four definitions"
        ),
        "batch_id": block["batch_id"],
        "review_gate": manifest["review_gate"],
        "review_scope": manifest["review_scope"],
        "subject_id": block["subject_id"],
        "subject_digest": block["subject_digest"],
        "reviewed_packet": artifact_triple(
            packet, root, packet_bytes, "packet"),
        "response": artifact_triple(response, root, response_bytes, "response"),
        "review_manifest": artifact_triple(
            manifest_path(review), root, manifest_bytes, "review manifest"),
        # Recorded at its canonical repository path rather than relative to
        # the packet: this is a historical attribution anchored by
        # `validated_commit`, and the tool is expected to move as it evolves.
        "validator": {
            "path": VALIDATOR_REPOSITORY_PATH,
            "sha256": prefixed_digest(validator.read_bytes()),
            "byte_length": len(validator.read_bytes()),
        },
        "response_note": note,
        "definitions": definitions,
        "probe_evidence": block["probe_evidence"],
        "decision_evidence": block["decision_evidence"],
        "downstream_claims": manifest["downstream_claims"],
        "claims": {
            "bpr_0312_positive_build_gate_satisfied": True,
            "public_preview_definitions_bound": True,
            "rfc_0017_closeout_claimed": False,
            "implementation_cards_started": False,
            "credential_mutation_or_private_read_authorized": False,
            "platform_enablement_claimed": False,
            "release_gate_satisfied": False,
        },
    }


# --------------------------------------------------------------------------
# Modes
# --------------------------------------------------------------------------

def prepare(args: argparse.Namespace) -> dict:
    """Everything both modes prove. No traffic, no writes."""
    if args.require != REQUIRED_DECISION:
        raise DecisionError(
            f"--require must be {REQUIRED_DECISION!r}; this gate validates only "
            "the positive branch, and a rejection leaves BPR-0312 unresolved "
            "rather than producing a record of itself")
    packet = Path(args.packet)
    review = review_directory(packet)
    root = repository_root(review)
    packet_bytes = read_bounded(packet, "packet")
    block = parse_definitions_block(packet_bytes)
    manifest_bytes = read_bounded(manifest_path(review), "review manifest")
    manifest = load_review_manifest(manifest_bytes, block)
    verify_evidence(block["probe_evidence"], root, "probe evidence")
    verify_evidence(block["decision_evidence"], root, "decision evidence")
    response = Path(args.response)
    response_bytes = read_bounded(response, "response")
    note = validate_response(response_bytes, block, manifest)
    definitions = bind_definitions(note, block)
    return {
        "root": root,
        "packet": packet,
        "packet_bytes": packet_bytes,
        "response": response,
        "response_bytes": response_bytes,
        "output": Path(args.output),
        "require": args.require,
        "review": review,
        "manifest": manifest,
        "manifest_bytes": manifest_bytes,
        "block": block,
        "note": note,
        "definitions": definitions,
    }


def run_write(args: argparse.Namespace) -> int:
    state = prepare(args)
    output = state["output"]
    if output.exists() or output.is_symlink():
        raise DecisionError(
            f"{output} already exists; a validation record is written once and "
            "revalidated with --validate-existing")
    record = build_record(
        commit=head_commit(state["root"]),
        recorded_at=recorded_at_now(),
        **{key: state[key] for key in (
            "root", "packet", "packet_bytes", "response", "response_bytes",
            "output", "require", "review", "manifest", "manifest_bytes",
            "block", "note", "definitions")},
    )
    payload = readable_json(record)
    output.parent.mkdir(parents=True, exist_ok=True)
    descriptor = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644)
    with os.fdopen(descriptor, "wb") as handle:
        handle.write(payload)
    report(record, "RECORDED")
    return 0


def run_validate_existing(args: argparse.Namespace) -> int:
    state = prepare(args)
    output = state["output"]
    payload = read_bounded(output, "validation record")
    recorded = require_object(
        parse_json_unique(payload, "validation record"), "validation record")
    recorded_at = recorded.get("recorded_at")
    if not isinstance(recorded_at, str) or not RFC3339_UTC.match(recorded_at):
        raise DecisionError("validation record recorded_at is not RFC 3339 UTC")
    commit = require_commit_id(
        recorded.get("validated_commit"), "validation record validated_commit")
    require_commit_present(
        state["root"], commit, "validation record validated_commit")
    expected = build_record(
        commit=commit,
        recorded_at=recorded_at,
        **{key: state[key] for key in (
            "root", "packet", "packet_bytes", "response", "response_bytes",
            "output", "require", "review", "manifest", "manifest_bytes",
            "block", "note", "definitions")},
    )
    if canonical_json(recorded) != canonical_json(expected):
        differing = sorted(
            key for key in set(recorded) | set(expected)
            if recorded.get(key) != expected.get(key))
        raise DecisionError(
            "validation record no longer matches the packet, page, and "
            f"response it binds; differing fields {differing}")
    report(expected, "VALID")
    return 0


def report(record: dict, outcome: str) -> None:
    print(f"RFC0017_PUBLIC_PREVIEW_DECISION_{outcome}:{record['decision']}")
    print(
        "RFC0017_PUBLIC_PREVIEW_DEFINITIONS_BOUND:"
        f"{len(record['definitions'])}")
    print(
        "RFC0017_PUBLIC_PREVIEW_EVIDENCE_BOUND:"
        f"{len(record['probe_evidence']) + len(record['decision_evidence'])}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Validate the positive RFC-BPR-0017 public-preview build decision "
            "and bind its four definitions."))
    parser.add_argument("--packet", required=True)
    parser.add_argument("--response", required=True)
    parser.add_argument("--require", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument(
        "--validate-existing", action="store_true",
        help="write nothing; revalidate the response and the existing record")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.validate_existing:
            return run_validate_existing(args)
        return run_write(args)
    except DecisionError as error:
        sys.stderr.write(f"RFC0017_PUBLIC_PREVIEW_DECISION_REFUSED:{error}\n")
        return 3


if __name__ == "__main__":
    raise SystemExit(main())
