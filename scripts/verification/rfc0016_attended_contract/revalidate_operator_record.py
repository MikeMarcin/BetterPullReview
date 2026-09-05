#!/usr/bin/env python3
"""Replay BPR's existing RFC-0016 operator evidence after source relocation.

No new review workflow or record generation. Every historical artifact, source
byte, commit relationship, bounded claim and command remains checked. The only
mapping is the old official-validator source path to its retained physical file.
"""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent / "scripts"))
from validate_operator_packet import *  # Frozen product protocol helpers.

HISTORICAL_VALIDATOR_PATH = ".agents/skills/bpr-attended-review/scripts/validate_operator_packet.py"
RETAINED_VALIDATOR_PATH = "scripts/verification/rfc0016_attended_contract/scripts/validate_operator_packet.py"
RETAINED_SOURCE = Path(__file__).resolve().parent / "scripts/validate_operator_packet.py"

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
    current_validator_path = RETAINED_VALIDATOR_PATH
    if validator_path not in (HISTORICAL_VALIDATOR_PATH, current_validator_path):
        raise validation_error("official validator has an unrecognized source path")
    if RETAINED_SOURCE.read_bytes() != validator:
        raise validation_error("retained validator differs from the committed official validator")
    reviewed_artifacts = {
        paths["reviewed_packet"]: packet,
        paths["decision_subject"]: subject,
        paths["operator_response"]: response,
        paths["claims_contract"]: claims_contract,
        validator_path: validator,
    }
    for path, data in reviewed_artifacts.items():
        if path == validator_path:
            # The source path remains the path recorded in historical evidence.
            # Only its present physical location changed during skill cutover.
            if committed_bytes(root, record_commit, path) != data:
                raise validation_error("reviewed validator changed before record commit")
            if RETAINED_SOURCE.read_bytes() != data:
                raise validation_error("retained validator bytes have changed")
        else:
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

def main():
    arguments = parser().parse_args()
    try:
        if not arguments.validate_existing or not arguments.record_commit:
            raise validation_error("this retained protocol entrypoint only supports --validate-existing with --record-commit")
        root = repository_root(arguments.repository_root)
        record = validate_existing(root, relative_path(root, arguments.output), exact_commit(root, arguments.record_commit))
        print(f"OPERATOR_RESPONSE_VALID:{record['gate']}")
        return 0
    except (OSError, UnicodeError, ValueError, validation_error) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
