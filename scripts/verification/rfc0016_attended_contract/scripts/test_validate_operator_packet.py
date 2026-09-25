#!/usr/bin/env python3
"""Contract tests for committed markdown operator-packet validation."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
from pathlib import Path

# Scratch roots come from the repository's shared helper ([[BPR-0868]]). This
# file kept its own while it lived in a skill pack that could not reach the
# repository; it has lived in the repository since 074f2473. The shared helper
# makes a root a contained process can still use (on Windows it inherits the
# parent's entries). It clears the read-only bit Git puts on its object files
# before removing them, and it names and raises a root it cannot remove. The
# old rmtree(ignore_errors=True) left every root behind on Windows (BPR-1120).
sys.path.insert(0, str(Path(__file__).resolve().parents[4]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

VALIDATOR = Path(__file__).with_name("validate_operator_packet.py")


GATE = "synthetic-gate-v001"
RECORD = Path(GATE) / "validation-record.json"
FAILURES: list[str] = []


def expect(condition: bool, message: str) -> None:
    if not condition:
        FAILURES.append(message)


def git(root: Path, *arguments: str) -> str:
    result = subprocess.run(
        ["git", *arguments],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def fixture(root: Path, newline: str = "\n") -> tuple[str, str]:
    root.mkdir()
    (root / GATE).mkdir()
    subprocess.run(["git", "init", "-q"], cwd=root, check=True)
    git(root, "config", "user.name", "BPR Test")
    git(root, "config", "user.email", "bpr-test@example.invalid")
    subject = (
        "schema_name=bpr.operator_decision_subject\n"
        "schema_version=1\n"
        f"gate={GATE}\n"
        "fact_1=bounded synthetic fact\n"
    )
    subject_hash = hashlib.sha256(subject.encode()).hexdigest()
    statement = f"I approve the synthetic gate bound by decision subject\nsha256:{subject_hash}.\n"
    packet = newline.join(
        (
            "# Synthetic operator packet",
            "",
            "To approve, reply with this exact statement:",
            "",
            "> I approve the synthetic gate bound by decision subject",
            f"> sha256:{subject_hash}.",
            "",
            "To reject, name the fact.",
            "",
        )
    )
    (root / GATE / "decision-subject.txt").write_text(
        subject, encoding="utf-8", newline=""
    )
    (root / GATE / "operator-packet.md").write_bytes(packet.encode())
    (root / GATE / "operator-response.md").write_text(
        statement, encoding="utf-8", newline=""
    )
    claims = {
        "schema_name": "bpr.operator_claim_contract",
        "schema_version": 1,
        "gate": GATE,
        "approved_claim": "synthetic_gate_v001_approved",
        "adjacent_false_claims": ["adjacent_authority_approved"],
    }
    (root / GATE / "claims-contract.json").write_text(
        json.dumps(claims), encoding="utf-8", newline=""
    )
    (root / "validator.py").write_bytes(VALIDATOR.read_bytes())
    git(root, "add", ".")
    git(root, "commit", "-qm", "fixture")
    return statement, git(root, "rev-parse", "HEAD")


def command(
    root: Path,
    commit: str,
    validate_existing: bool = False,
    record_commit: str | None = None,
    artifact_directory: str = GATE,
) -> list[str]:
    result = [
        sys.executable,
        str(root / "validator.py"),
        "--repository-root",
        ".",
        "--output",
        f"{artifact_directory}/validation-record.json",
    ]
    if validate_existing:
        return [
            *result,
            "--validate-existing",
            "--record-commit",
            record_commit or commit,
        ]
    return [
        *result,
        "--commit",
        commit,
        "--packet",
        f"{artifact_directory}/operator-packet.md",
        "--subject",
        f"{artifact_directory}/decision-subject.txt",
        "--response",
        f"{artifact_directory}/operator-response.md",
        "--claims-contract",
        f"{artifact_directory}/claims-contract.json",
        "--recorded-at",
        "2026-08-13T12:00:00-07:00",
    ]


def run(root: Path, arguments: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(arguments, cwd=root, check=False, capture_output=True, text=True)


def test_success(root: Path) -> None:
    _, commit = fixture(root)
    generated = run(root, command(root, commit))
    expect(generated.returncode == 0, f"exact response passes: {generated.stderr.strip()}")
    record = json.loads((root / RECORD).read_text())
    expect(record["validated_commit"] == commit, "reviewed commit is bound")
    expect(
        record["claims"]
        == {
            "adjacent_authority_approved": False,
            "synthetic_gate_v001_approved": True,
        },
        "claims are isolated",
    )
    git(root, "add", RECORD.as_posix())
    git(root, "commit", "-qm", "validation record")
    record_commit = git(root, "rev-parse", "HEAD")
    existing = run(root, command(root, commit, True, record_commit))
    expect(existing.returncode == 0, f"existing record revalidates: {existing.stderr.strip()}")


def test_tampered_record_is_rejected(root: Path) -> None:
    _, commit = fixture(root)
    generated = run(root, command(root, commit))
    expect(generated.returncode == 0, f"record fixture generates: {generated.stderr.strip()}")
    git(root, "add", RECORD.as_posix())
    git(root, "commit", "-qm", "validation record")
    record_commit = git(root, "rev-parse", "HEAD")

    original = json.loads((root / RECORD).read_text())
    record = dict(original)
    record["claims"] = {
        "adjacent_authority_approved": True,
        "synthetic_gate_v001_approved": False,
    }
    (root / RECORD).write_text(json.dumps(record))
    dirty = run(root, command(root, commit, True, record_commit))
    expect(dirty.returncode != 0, "dirty validation-record metadata is rejected")

    git(root, "add", RECORD.as_posix())
    git(root, "commit", "-qm", "tampered validation record")
    tampered_commit = git(root, "rev-parse", "HEAD")
    committed = run(root, command(root, commit, True, tampered_commit))
    expect(committed.returncode != 0, "committed claim substitution is rejected")

    record = dict(original)
    record["claims"] = {
        "adjacent_authority_approved": False,
        "synthetic_gate_v001_approved": True,
    }
    record["validation_command"] = "false"
    (root / RECORD).write_text(json.dumps(record))
    git(root, "add", RECORD.as_posix())
    git(root, "commit", "-qm", "tampered command")
    command_commit = git(root, "rev-parse", "HEAD")
    changed_command = run(root, command(root, commit, True, command_commit))
    expect(changed_command.returncode != 0, "committed validation-command substitution is rejected")

    record = dict(original)
    record["repository_mutation_authorized"] = True
    (root / RECORD).write_text(json.dumps(record))
    git(root, "add", RECORD.as_posix())
    git(root, "commit", "-qm", "unknown authority-bearing field")
    unknown_commit = git(root, "rev-parse", "HEAD")
    unknown = run(root, command(root, commit, True, unknown_commit))
    expect(unknown.returncode != 0, "unknown validation-record keys are rejected")


def test_record_scalar_types_are_exact(root: Path) -> None:
    _, commit = fixture(root)
    generated = run(root, command(root, commit))
    expect(generated.returncode == 0, f"record fixture generates: {generated.stderr.strip()}")
    original = json.loads((root / RECORD).read_text())
    mutations = (
        ("boolean schema version", lambda record: record.__setitem__("schema_version", True)),
        (
            "numeric check boolean",
            lambda record: record["checks"].__setitem__("approval_statement_exact", 1),
        ),
        (
            "numeric claim boolean",
            lambda record: record["claims"].__setitem__("synthetic_gate_v001_approved", 1),
        ),
        (
            "floating artifact length",
            lambda record: record["reviewed_packet"].__setitem__(
                "byte_length", float(record["reviewed_packet"]["byte_length"])
            ),
        ),
    )
    for label, mutate in mutations:
        record = json.loads(json.dumps(original))
        mutate(record)
        (root / RECORD).write_text(json.dumps(record))
        git(root, "add", RECORD.as_posix())
        git(root, "commit", "-qm", label)
        record_commit = git(root, "rev-parse", "HEAD")
        result = run(root, command(root, commit, True, record_commit))
        expect(result.returncode != 0, f"{label} is rejected")


def test_disconnected_record_commit_is_rejected(root: Path) -> None:
    _, commit = fixture(root)
    generated = run(root, command(root, commit))
    expect(generated.returncode == 0, f"record fixture generates: {generated.stderr.strip()}")
    git(root, "add", RECORD.as_posix())
    git(root, "commit", "-qm", "validation record")
    record_commit = git(root, "rev-parse", "HEAD")
    tree = git(root, "rev-parse", f"{record_commit}^{{tree}}")
    disconnected = subprocess.run(
        ["git", "commit-tree", tree],
        cwd=root,
        check=True,
        capture_output=True,
        input="disconnected validation record\n",
        text=True,
    ).stdout.strip()
    result = run(root, command(root, commit, True, disconnected))
    expect(result.returncode != 0, "disconnected validation-record commit is rejected")


def test_changed_reviewed_artifact_is_rejected(root: Path) -> None:
    _, commit = fixture(root)
    generated = run(root, command(root, commit))
    expect(generated.returncode == 0, f"record fixture generates: {generated.stderr.strip()}")
    git(root, "add", RECORD.as_posix())
    git(root, "commit", "-qm", "validation record")

    packet_path = root / GATE / "operator-packet.md"
    packet_path.write_text((root / GATE / "operator-response.md").read_text())
    git(root, "add", f"{GATE}/operator-packet.md")
    git(root, "commit", "-qm", "changed reviewed packet")
    changed_commit = git(root, "rev-parse", "HEAD")
    changed = run(root, command(root, commit, True, changed_commit))
    expect(changed.returncode != 0, "reviewed artifact change before record commit is rejected")


def test_changed_validator_is_rejected(root: Path) -> None:
    _, commit = fixture(root)
    generated = run(root, command(root, commit))
    expect(generated.returncode == 0, f"record fixture generates: {generated.stderr.strip()}")
    git(root, "add", RECORD.as_posix())
    git(root, "commit", "-qm", "validation record")
    record_commit = git(root, "rev-parse", "HEAD")

    (root / "validator.py").write_text((root / "validator.py").read_text() + "\n# changed\n")
    changed = run(root, command(root, commit, True, record_commit))
    expect(changed.returncode != 0, "changed executing validator is rejected")


def test_response_changes(root: Path) -> None:
    statement, commit = fixture(root)
    for index, changed in enumerate((
        statement.replace("approve", "accept"),
        statement.replace("\n", "\r\n"),
        statement.removesuffix("\n"),
    )):
        (root / GATE / "operator-response.md").write_bytes(changed.encode())
        result = run(root, command(root, commit))
        expect(result.returncode != 0, f"response byte change {index} is rejected")


def test_crlf_packet(root: Path) -> None:
    _, commit = fixture(root, "\r\n")
    result = run(root, command(root, commit))
    expect(result.returncode != 0, "CRLF packet cannot redefine LF extraction")


def test_dirty_and_subject_drift(root: Path) -> None:
    _, commit = fixture(root)
    (root / GATE / "decision-subject.txt").write_text(
        "schema_name=bpr.operator_decision_subject\nschema_version=1\ngate=changed\n"
    )
    dirty = run(root, command(root, commit))
    expect(dirty.returncode != 0, "dirty subject/commit mismatch is rejected")
    git(root, "checkout", "--", f"{GATE}/decision-subject.txt")
    packet_path = root / GATE / "operator-packet.md"
    packet = packet_path.read_text().replace("sha256:", "sha256:0")
    packet_path.write_text(packet)
    git(root, "add", f"{GATE}/operator-packet.md")
    git(root, "commit", "-qm", "changed packet")
    changed_commit = git(root, "rev-parse", "HEAD")
    changed = run(root, command(root, changed_commit))
    expect(changed.returncode != 0, "altered approval subject binding is rejected")


def test_claim_contract_cannot_broaden_authority(root: Path) -> None:
    _, commit = fixture(root)
    claims_path = root / GATE / "claims-contract.json"
    contract = json.loads(claims_path.read_text())
    contract["approved_claim"] = "repository_mutation_authorized"
    claims_path.write_text(json.dumps(contract))
    git(root, "add", f"{GATE}/claims-contract.json")
    git(root, "commit", "-qm", "broadened claims")
    broadened_commit = git(root, "rev-parse", "HEAD")
    result = run(root, command(root, broadened_commit))
    expect(result.returncode != 0, "broadened approved claim is rejected")

    contract["approved_claim"] = "synthetic_gate_v001_approved"
    contract["adjacent_false_claims"] = []
    claims_path.write_text(json.dumps(contract))
    git(root, "add", f"{GATE}/claims-contract.json")
    git(root, "commit", "-qm", "missing adjacent claims")
    missing_commit = git(root, "rev-parse", "HEAD")
    missing = run(root, command(root, missing_commit))
    expect(missing.returncode != 0, "missing adjacent false claims are rejected")

    contract["adjacent_false_claims"] = ["adjacent_authority_approved"]
    contract["claims_contract_override"] = {
        "approved_claim": "repository_mutation_authorized"
    }
    claims_path.write_text(json.dumps(contract))
    git(root, "add", f"{GATE}/claims-contract.json")
    git(root, "commit", "-qm", "unknown claims-contract field")
    unknown_commit = git(root, "rev-parse", "HEAD")
    unknown = run(root, command(root, unknown_commit))
    expect(unknown.returncode != 0, "unknown claims-contract keys are rejected")

    contract.pop("claims_contract_override")
    contract["schema_version"] = True
    claims_path.write_text(json.dumps(contract))
    git(root, "add", f"{GATE}/claims-contract.json")
    git(root, "commit", "-qm", "boolean claims-contract version")
    boolean_commit = git(root, "rev-parse", "HEAD")
    boolean_version = run(root, command(root, boolean_commit))
    expect(boolean_version.returncode != 0, "boolean claims-contract version is rejected")


def test_gate_path_and_timestamp_are_exact(root: Path) -> None:
    _, commit = fixture(root)
    timestamp_command = command(root, commit)
    timestamp_at = timestamp_command.index("--recorded-at") + 1
    timestamp_command[timestamp_at] = "2026-08-13 12:00:00-07:00"
    timestamp = run(root, timestamp_command)
    expect(timestamp.returncode != 0, "non-canonical RFC 3339 timestamp is rejected")

    git(root, "mv", GATE, "different-gate")
    git(root, "commit", "-qm", "misplaced gate artifacts")
    moved_commit = git(root, "rev-parse", "HEAD")
    misplaced = run(root, command(root, moved_commit, artifact_directory="different-gate"))
    expect(misplaced.returncode != 0, "decision artifacts outside the gate directory are rejected")


def test_validation_record_must_be_adjacent(root: Path) -> None:
    _, commit = fixture(root)
    arguments = command(root, commit)
    output_at = arguments.index("--output") + 1
    arguments[output_at] = "validation-record.json"
    result = run(root, arguments)
    expect(result.returncode != 0, "non-adjacent validation-record path is rejected")


def main() -> int:
    tests = (
        test_success,
        test_tampered_record_is_rejected,
        test_record_scalar_types_are_exact,
        test_disconnected_record_commit_is_rejected,
        test_changed_reviewed_artifact_is_rejected,
        test_changed_validator_is_rejected,
        test_response_changes,
        test_crlf_packet,
        test_dirty_and_subject_drift,
        test_claim_contract_cannot_broaden_authority,
        test_gate_path_and_timestamp_are_exact,
        test_validation_record_must_be_adjacent,
    )
    for test in tests:
        with contained_temporary_directory(prefix="bpr-operator-") as directory:
            test(Path(directory) / test.__name__)
    if FAILURES:
        for failure in FAILURES:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    print("OPERATOR_PACKET_VALIDATION_TESTS_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
