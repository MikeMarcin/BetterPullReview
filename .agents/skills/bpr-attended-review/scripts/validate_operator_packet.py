#!/usr/bin/env python3
"""Validate a byte-exact response to a committed markdown operator packet."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import re
import shlex
import subprocess
import sys
from pathlib import Path


APPROVAL_MARKER = b"To approve, reply with this exact statement:\n"
COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
CLAIM_RE = re.compile(r"^[a-z][a-z0-9_]*$")
EXTRACTION_RULE = (
    "first contiguous blockquote after the unique LF approval marker; remove "
    "exactly '> ' when present; preserve UTF-8/LF and one terminal LF"
)
PASS_CHECKS = {
    "approval_statement_exact": True,
    "committed_artifacts_exact": True,
    "decision_subject_digest_bound": True,
    "overall": "pass",
}
CLAIMS_CONTRACT_KEYS = {
    "adjacent_false_claims",
    "approved_claim",
    "gate",
    "schema_name",
    "schema_version",
}
VALIDATION_RECORD_KEYS = {
    "checks",
    "claims",
    "claims_contract",
    "decision_subject",
    "extraction_rule",
    "gate",
    "official_validator",
    "operator_response",
    "outcome",
    "recorded_at",
    "result",
    "reviewed_packet",
    "schema_name",
    "schema_version",
    "validated_commit",
    "validation_command",
}
RFC3339_RE = re.compile(
    r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:\d{2})$"
)


class validation_error(RuntimeError):
    pass


def digest(data: bytes) -> str:
    return f"sha256:{hashlib.sha256(data).hexdigest()}"


def artifact(path: str, data: bytes) -> dict[str, object]:
    return {"path": path, "sha256": digest(data), "byte_length": len(data)}


def strict_json(data: bytes, label: str) -> dict[str, object]:
    def reject_duplicate_keys(pairs: list[tuple[str, object]]) -> dict[str, object]:
        result: dict[str, object] = {}
        for key, value in pairs:
            if key in result:
                raise validation_error(f"{label} repeats key: {key}")
            result[key] = value
        return result

    try:
        value = json.loads(data.decode("utf-8"), object_pairs_hook=reject_duplicate_keys)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise validation_error(f"{label} is not valid UTF-8 JSON") from error
    if not isinstance(value, dict):
        raise validation_error(f"{label} must be a JSON object")
    return value


def run(root: Path, *arguments: str) -> bytes:
    result = subprocess.run(arguments, cwd=root, check=False, capture_output=True)
    if result.returncode != 0:
        message = result.stderr.decode("utf-8", errors="replace").strip()
        raise validation_error(message or f"command failed: {shlex.join(arguments)}")
    return result.stdout


def repository_root(value: str) -> Path:
    root = Path(value).resolve()
    observed = run(root, "git", "rev-parse", "--show-toplevel").decode().strip()
    if Path(observed).resolve() != root:
        raise validation_error("--repository-root is not the Git worktree root")
    return root


def relative_path(root: Path, value: str) -> str:
    path = (root / value).resolve()
    try:
        relative = path.relative_to(root).as_posix()
    except ValueError as error:
        raise validation_error(f"path escapes repository: {value}") from error
    if not relative or relative.startswith(".git/") or ":" in relative:
        raise validation_error(f"invalid repository path: {value}")
    return relative


def exact_commit(root: Path, value: str) -> str:
    commit = value
    if not COMMIT_RE.fullmatch(commit):
        raise validation_error("--commit must be a full lowercase SHA-1")
    resolved = run(root, "git", "rev-parse", "--verify", f"{commit}^{{commit}}")
    if resolved.decode("ascii").strip() != commit:
        raise validation_error("--commit does not resolve exactly")
    return commit


def committed_bytes(root: Path, commit: str, path: str) -> bytes:
    return run(root, "git", "show", f"{commit}:{path}")


def exact_statement(packet: bytes) -> bytes:
    if b"\r" in packet:
        raise validation_error("packet must use LF line endings")
    marker_at = packet.find(APPROVAL_MARKER)
    if marker_at < 0 or packet.find(APPROVAL_MARKER, marker_at + 1) >= 0:
        raise validation_error("packet must contain exactly one LF approval marker")
    lines = packet[marker_at + len(APPROVAL_MARKER) :].split(b"\n")
    if lines and lines[0] == b"":
        lines.pop(0)
    quoted: list[bytes] = []
    for line in lines:
        if not line.startswith(b">"):
            break
        quoted.append(line[2:] if line.startswith(b"> ") else line[1:])
    if not quoted:
        raise validation_error("approval marker is not followed by a blockquote")
    return b"\n".join(quoted) + b"\n"


def subject_gate(subject: bytes) -> str:
    try:
        lines = subject.decode("utf-8").splitlines()
    except UnicodeDecodeError as error:
        raise validation_error("decision subject is not UTF-8") from error
    values: dict[str, str] = {}
    for line in lines:
        if "=" not in line:
            raise validation_error("decision subject contains a malformed line")
        key, value = line.split("=", 1)
        if key in values:
            raise validation_error(f"decision subject repeats key: {key}")
        values[key] = value
    if values.get("schema_name") != "bpr.operator_decision_subject":
        raise validation_error("decision subject schema is not supported")
    if values.get("schema_version") != "1" or not values.get("gate"):
        raise validation_error("decision subject version or gate is invalid")
    return values["gate"]


def recorded_at(value: str) -> str:
    if not RFC3339_RE.fullmatch(value):
        raise validation_error("--recorded-at must be canonical RFC 3339")
    try:
        parsed = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as error:
        raise validation_error("--recorded-at must be RFC 3339") from error
    if parsed.tzinfo is None or parsed.utcoffset() is None:
        raise validation_error("--recorded-at must include a UTC offset")
    return value


def claim_values(contract: bytes, gate: str) -> dict[str, bool]:
    value = strict_json(contract, "claims contract")
    if set(value) != CLAIMS_CONTRACT_KEYS:
        raise validation_error("claims-contract keys do not match the exact schema")
    if (
        type(value.get("schema_name")) is not str
        or value.get("schema_name") != "bpr.operator_claim_contract"
        or type(value.get("schema_version")) is not int
        or value.get("schema_version") != 1
    ):
        raise validation_error("claims-contract schema is not supported")
    approved = value.get("approved_claim")
    false_claims = value.get("adjacent_false_claims")
    expected_approved = f"{gate.replace('-', '_')}_approved"
    if (
        type(value.get("gate")) is not str
        or type(approved) is not str
        or value.get("gate") != gate
        or approved != expected_approved
    ):
        raise validation_error("claims contract does not match the decision-subject gate")
    if not isinstance(false_claims, list) or not false_claims:
        raise validation_error("claims contract must name adjacent false claims")
    names = [approved, *false_claims]
    if (
        len(names) != len(set(names))
        or any(not isinstance(name, str) or not CLAIM_RE.fullmatch(name) for name in names)
    ):
        raise validation_error("claims-contract names must be unique snake_case identifiers")
    return {name: name == approved for name in sorted(names)}


def validate_gate_paths(gate: str, paths: dict[str, str], output_path: str) -> None:
    expected_names = {
        "reviewed_packet": "operator-packet.md",
        "decision_subject": "decision-subject.txt",
        "operator_response": "operator-response.md",
        "claims_contract": "claims-contract.json",
    }
    parents = {Path(path).parent.as_posix() for path in paths.values()}
    if len(parents) != 1 or Path(next(iter(parents))).name != gate:
        raise validation_error("operator artifacts are not confined to the decision gate directory")
    for name, filename in expected_names.items():
        if Path(paths[name]).name != filename:
            raise validation_error(f"operator artifact has an unexpected name: {name}")
    if (
        Path(output_path).parent.as_posix() != next(iter(parents))
        or Path(output_path).name != "validation-record.json"
    ):
        raise validation_error("validation record is not adjacent to its decision gate")


def require_ancestor(root: Path, ancestor: str, descendant: str) -> None:
    if ancestor == descendant:
        raise validation_error("validation record must be committed after the reviewed artifacts")
    result = subprocess.run(
        ("git", "merge-base", "--is-ancestor", ancestor, descendant),
        cwd=root,
        check=False,
        capture_output=True,
    )
    if result.returncode != 0:
        raise validation_error("validated commit is not an ancestor of record commit")


def require_artifact_unchanged(
    root: Path,
    record_commit: str,
    path: str,
    reviewed_data: bytes,
) -> None:
    if committed_bytes(root, record_commit, path) != reviewed_data:
        raise validation_error(f"reviewed artifact changed before record commit: {path}")
    if (root / path).read_bytes() != reviewed_data:
        raise validation_error(f"working-tree reviewed artifact has changed: {path}")


def validate_record_artifact(
    root: Path, commit: str, record: dict[str, object], name: str
) -> bytes:
    value = record.get(name)
    if not isinstance(value, dict):
        raise validation_error(f"record is missing {name}")
    if set(value) != {"byte_length", "path", "sha256"}:
        raise validation_error(f"record {name} keys do not match the exact schema")
    path = value.get("path")
    if type(path) is not str or type(value.get("sha256")) is not str:
        raise validation_error(f"record {name} path is invalid")
    if type(value.get("byte_length")) is not int:
        raise validation_error(f"record {name} byte length is not an integer")
    data = committed_bytes(root, commit, relative_path(root, path))
    if value != artifact(path, data):
        raise validation_error(f"record {name} artifact identity is invalid")
    return data


def generation_invocation(
    validator_path: str,
    commit: str,
    packet_path: str,
    subject_path: str,
    response_path: str,
    claims_contract_path: str,
    timestamp: str,
    output_path: str,
) -> list[str]:
    result = [
        "python3",
        validator_path,
        "--repository-root",
        ".",
        "--commit",
        commit,
        "--packet",
        packet_path,
        "--subject",
        subject_path,
        "--response",
        response_path,
        "--claims-contract",
        claims_contract_path,
        "--recorded-at",
        timestamp,
    ]
    result.extend(("--output", output_path))
    return result


def validate_existing(
    root: Path,
    output_path: str,
    record_commit: str,
) -> dict[str, object]:
    record_bytes = committed_bytes(root, record_commit, output_path)
    if (root / output_path).read_bytes() != record_bytes:
        raise validation_error("working-tree validation record differs from record commit")
    record = strict_json(record_bytes, "validation record")
    if set(record) != VALIDATION_RECORD_KEYS:
        raise validation_error("validation-record keys do not match the exact schema")
    if (
        type(record.get("schema_name")) is not str
        or record.get("schema_name") != "bpr.operator_response_validation"
        or type(record.get("schema_version")) is not int
        or record.get("schema_version") != 1
    ):
        raise validation_error("validation-record schema is not supported")
    string_fields = (
        "extraction_rule",
        "gate",
        "outcome",
        "recorded_at",
        "result",
        "validated_commit",
        "validation_command",
    )
    if any(type(record.get(name)) is not str for name in string_fields):
        raise validation_error("validation-record string field has the wrong type")
    commit = exact_commit(root, record["validated_commit"])
    require_ancestor(root, commit, record_commit)
    packet = validate_record_artifact(root, commit, record, "reviewed_packet")
    subject = validate_record_artifact(root, commit, record, "decision_subject")
    response = validate_record_artifact(root, commit, record, "operator_response")
    claims_contract = validate_record_artifact(root, commit, record, "claims_contract")
    validator = validate_record_artifact(root, commit, record, "official_validator")
    statement = exact_statement(packet)
    gate = subject_gate(subject)
    if response != statement or digest(subject).encode("ascii") not in statement:
        raise validation_error("recorded response or subject binding is invalid")
    if (
        gate != record.get("gate")
        or record.get("result") != "pass"
        or record.get("outcome") != "approved"
    ):
        raise validation_error("recorded gate/result/outcome is invalid")
    checks = record.get("checks")
    if (
        not isinstance(checks, dict)
        or set(checks) != set(PASS_CHECKS)
        or type(checks.get("approval_statement_exact")) is not bool
        or type(checks.get("committed_artifacts_exact")) is not bool
        or type(checks.get("decision_subject_digest_bound")) is not bool
        or type(checks.get("overall")) is not str
        or checks != PASS_CHECKS
    ):
        raise validation_error("recorded checks are invalid")
    expected_claims = claim_values(claims_contract, gate)
    claims = record.get("claims")
    if (
        not isinstance(claims, dict)
        or set(claims) != set(expected_claims)
        or any(type(value) is not bool for value in claims.values())
        or claims != expected_claims
    ):
        raise validation_error("recorded claims differ from expected bounded authority")
    if record.get("extraction_rule") != EXTRACTION_RULE:
        raise validation_error("recorded extraction rule is invalid")
    timestamp = recorded_at(str(record.get("recorded_at", "")))
    paths = {
        name: str(record[name]["path"])
        for name in (
            "reviewed_packet",
            "decision_subject",
            "operator_response",
            "claims_contract",
        )
    }
    validate_gate_paths(gate, paths, output_path)
    validator_path = str(record["official_validator"]["path"])
    current_validator_path = Path(__file__).resolve().relative_to(root).as_posix()
    if validator_path != current_validator_path or Path(__file__).read_bytes() != validator:
        raise validation_error("executing validator differs from the committed official validator")
    reviewed_artifacts = {
        paths["reviewed_packet"]: packet,
        paths["decision_subject"]: subject,
        paths["operator_response"]: response,
        paths["claims_contract"]: claims_contract,
        validator_path: validator,
    }
    for path, data in reviewed_artifacts.items():
        require_artifact_unchanged(root, record_commit, path, data)
    expected_command = shlex.join(
        generation_invocation(
            validator_path,
            commit,
            paths["reviewed_packet"],
            paths["decision_subject"],
            paths["operator_response"],
            paths["claims_contract"],
            timestamp,
            output_path,
        )
    )
    if record.get("validation_command") != expected_command:
        raise validation_error("recorded validation command is invalid")
    return record


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser()
    result.add_argument("--repository-root", required=True)
    result.add_argument("--commit")
    result.add_argument("--packet")
    result.add_argument("--subject")
    result.add_argument("--response")
    result.add_argument("--claims-contract")
    result.add_argument("--recorded-at")
    result.add_argument("--output", required=True)
    result.add_argument("--validate-existing", action="store_true")
    result.add_argument("--record-commit")
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        root = repository_root(arguments.repository_root)
        output_path = relative_path(root, arguments.output)
        if arguments.validate_existing:
            if not arguments.record_commit:
                raise validation_error("--validate-existing requires --record-commit")
            record = validate_existing(
                root,
                output_path,
                exact_commit(root, arguments.record_commit),
            )
            print(f"OPERATOR_RESPONSE_VALID:{record['gate']}")
            return 0

        required = {
            "--commit": arguments.commit,
            "--packet": arguments.packet,
            "--subject": arguments.subject,
            "--response": arguments.response,
            "--claims-contract": arguments.claims_contract,
            "--recorded-at": arguments.recorded_at,
        }
        missing = [name for name, value in required.items() if not value]
        if missing:
            raise validation_error(f"generation mode requires: {', '.join(missing)}")
        commit = exact_commit(root, arguments.commit)
        packet_path = relative_path(root, arguments.packet)
        subject_path = relative_path(root, arguments.subject)
        response_path = relative_path(root, arguments.response)
        claims_contract_path = relative_path(root, arguments.claims_contract)
        validator_path = Path(__file__).resolve().relative_to(root).as_posix()
        artifact_paths = (
            packet_path,
            subject_path,
            response_path,
            claims_contract_path,
            validator_path,
        )
        committed = {path: committed_bytes(root, commit, path) for path in artifact_paths}
        for path, data in committed.items():
            if (root / path).read_bytes() != data:
                raise validation_error(
                    f"working-tree artifact differs from reviewed commit: {path}"
                )

        packet = committed[packet_path]
        subject = committed[subject_path]
        response = committed[response_path]
        claims_contract = committed[claims_contract_path]
        statement = exact_statement(packet)
        gate = subject_gate(subject)
        paths = {
            "reviewed_packet": packet_path,
            "decision_subject": subject_path,
            "operator_response": response_path,
            "claims_contract": claims_contract_path,
        }
        validate_gate_paths(gate, paths, output_path)
        if response != statement:
            raise validation_error("operator response is not the exact packet statement")
        if digest(subject).encode("ascii") not in statement:
            raise validation_error("approval statement does not bind the decision subject")
        validated_claims = claim_values(claims_contract, gate)
        timestamp = recorded_at(arguments.recorded_at)
        invocation = generation_invocation(
            validator_path,
            commit,
            packet_path,
            subject_path,
            response_path,
            claims_contract_path,
            timestamp,
            output_path,
        )
        record: dict[str, object] = {
            "schema_name": "bpr.operator_response_validation",
            "schema_version": 1,
            "gate": gate,
            "outcome": "approved",
            "result": "pass",
            "recorded_at": timestamp,
            "validated_commit": commit,
            "reviewed_packet": artifact(packet_path, packet),
            "decision_subject": artifact(subject_path, subject),
            "operator_response": artifact(response_path, response),
            "claims_contract": artifact(claims_contract_path, claims_contract),
            "official_validator": artifact(validator_path, committed[validator_path]),
            "extraction_rule": EXTRACTION_RULE,
            "validation_command": shlex.join(invocation),
            "checks": PASS_CHECKS,
            "claims": validated_claims,
        }
        destination = root / output_path
        if destination.exists():
            raise validation_error(f"validation record already exists: {output_path}")
        # Byte-exact on purpose. This record is committed and then revalidated
        # against its own commit, and git normalises line endings on the way in.
        # Without `newline=""` the tool wrote CRLF, git stored LF, and the very
        # next `--validate-existing` run rejected the record this tool had just
        # produced. `encoding` is explicit for the same reason: the record is
        # evidence, and evidence that varies with the writer's locale is not
        # evidence.
        destination.write_text(
            json.dumps(record, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
            newline="",
        )
        print(f"OPERATOR_RESPONSE_VALID:{gate}")
        return 0
    except (OSError, UnicodeError, json.JSONDecodeError, validation_error) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
