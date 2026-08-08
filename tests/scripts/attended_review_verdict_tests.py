#!/usr/bin/env python3
"""Checks the attended-review verdict arithmetic.

A verdict is the only durable record of a product-owner decision, so the
validator is the thing standing between "this was chosen" and "this failed".
The two are different facts and the schema was once unable to express the
difference: an n-way choice had to be recorded as one acceptance and n-1
rejections, which put candidates on the record as defective when they had
merely lost. BPR-0203.
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILDER = ROOT / ".agents/skills/bpr-attended-review/scripts/build_attended_review.py"

FAILURES: list[str] = []


def expect(condition: bool, message: str) -> None:
    if not condition:
        FAILURES.append(message)


def run(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(BUILDER), *args],
        capture_output=True,
        text=True,
        check=False,
    )


def png(path: Path, colour: tuple[int, int, int]) -> None:
    """Writes a one-pixel PNG without depending on an imaging library."""
    import struct
    import zlib

    raw = b"\x00" + bytes(colour)
    def chunk(tag: bytes, payload: bytes) -> bytes:
        return (
            struct.pack(">I", len(payload))
            + tag
            + payload
            + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
        )

    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw))
        + chunk(b"IEND", b"")
    )


def subject(key: str, choice_group: str | None) -> dict:
    record = {
        "subject_id": f"criterion:/betterpullreview/test/{key}",
        "subject_key": key,
        "display_name": key,
        "description": "one alternative of a synthetic choice",
        "acceptance_criteria": ["it is the one to take"],
        "known_concerns": [],
        "audit_verdict": {"status": "pass", "note": "synthetic"},
        "media_groups": [
            {
                "group_id": "frames",
                "title": "frames",
                "required": True,
                "layout": "grid",
                "note": "synthetic",
                "items": [{"label": key, "path": f"src/{key}.png"}],
            }
        ],
        "detail_sections": [],
        "identity_files": [],
    }
    if choice_group is not None:
        record["choice_group"] = choice_group
    return record


def build_batch(root: Path, choice_group: str | None) -> dict:
    source = root / "src"
    source.mkdir(parents=True, exist_ok=True)
    for index, key in enumerate(("alt-a", "alt-b", "alt-c")):
        png(source / f"{key}.png", (40 + 60 * index, 40, 40))
    batch = {
        "schema_name": "bpr.attended_review_batch_input",
        "schema_version": 1,
        "project_id": "betterpullreview",
        "title": "synthetic choice",
        "review_gate": "synthetic_gate",
        "review_scope": "human_ui_criterion_isolation_only",
        "downstream_claims": {"rfc_closeout_claimed": False},
        "reference_disclaimer": "Synthetic fixture built by the verdict tests.",
        "subjects": [subject(key, choice_group) for key in ("alt-a", "alt-b", "alt-c")],
    }
    (root / "input.json").write_text(json.dumps(batch))
    result = run("build", "--input", str(root / "input.json"), "--out", str(root / "pkg"))
    expect(result.returncode == 0, f"the synthetic batch builds: {result.stderr.strip()}")
    return json.loads((root / "pkg" / "attended-review-manifest.json").read_text())


def verdict_for(
    manifest: dict,
    accepted: list[int],
    rejected: list[int],
    not_chosen: list[int] | None,
) -> dict:
    subjects = manifest["subjects"]

    def entries(indexes: list[int]) -> list[dict]:
        return [
            {
                "subject_id": subjects[index]["subject_id"],
                "subject_digest": subjects[index]["subject_digest"],
            }
            for index in sorted(indexes)
        ]

    record = {
        "schema_name": "bpr.attended_review_verdict",
        "schema_version": 1,
        "batch_id": manifest["batch_id"],
        "review_gate": manifest["review_gate"],
        "review_scope": manifest["review_scope"],
        "accepted_subjects": entries(accepted),
        "rejected_subjects": entries(rejected),
        "notes_by_subject_identity": {},
        "downstream_claims": manifest["downstream_claims"],
    }
    if not_chosen is not None:
        record["not_chosen_subjects"] = entries(not_chosen)
    return record


def validate(root: Path, manifest: dict, record: dict) -> subprocess.CompletedProcess[str]:
    path = root / "verdict.json"
    path.write_text(json.dumps(record))
    return run(
        "validate-verdict",
        "--manifest",
        str(root / "pkg" / "attended-review-manifest.json"),
        "--verdict",
        str(path),
    )


def test_choice_group_arithmetic(root: Path) -> None:
    manifest = build_batch(root / "choice", "shade")

    accepted = validate(
        root / "choice", manifest, verdict_for(manifest, [0], [], [1, 2])
    )
    expect(
        accepted.returncode == 0,
        f"one winner and two unchosen alternatives validates: {accepted.stderr.strip()}",
    )

    for name, record in {
        "two winners": verdict_for(manifest, [0, 1], [], [2]),
        "no winner": verdict_for(manifest, [], [], [0, 1, 2]),
    }.items():
        result = validate(root / "choice", manifest, record)
        expect(
            result.returncode != 0 and "exactly one accepted" in result.stderr,
            f"{name} is refused by the choice-group arithmetic",
        )

    # The defect this whole card exists for: a candidate that lost being put on
    # the record as one that failed.
    demoted = validate(
        root / "choice", manifest, verdict_for(manifest, [0], [1, 2], [])
    )
    expect(
        demoted.returncode != 0 and "not chosen rather than rejected" in demoted.stderr,
        "recording a losing alternative as rejected is refused",
    )

    omitted = validate(
        root / "choice", manifest, verdict_for(manifest, [0], [1, 2], None)
    )
    expect(
        omitted.returncode != 0,
        "a choice batch cannot omit not_chosen_subjects entirely",
    )


def test_batches_without_a_choice_group_are_unaffected(root: Path) -> None:
    """The pass/fail path must not acquire a new obligation.

    Every verdict recorded before choice groups existed omits the new field, and
    a change that invalidated them would rewrite history rather than extend it.
    """
    manifest = build_batch(root / "plain", None)
    result = validate(
        root / "plain", manifest, verdict_for(manifest, [0], [1, 2], None)
    )
    expect(
        result.returncode == 0,
        f"a pass/fail batch validates without the new field: {result.stderr.strip()}",
    )

    stray = validate(
        root / "plain", manifest, verdict_for(manifest, [0], [1], [2])
    )
    expect(
        stray.returncode != 0 and "only contain members of a choice group" in stray.stderr,
        "an unchosen subject outside any choice group is refused",
    )


def test_recorded_verdicts_still_validate() -> None:
    """Every verdict already on file, against its own manifest.

    These are the durable record of decisions a person actually made. A
    validator change that silently invalidated one of them would be a far worse
    outcome than the gap it set out to close.
    """
    reviews = sorted((ROOT / "docs/verification/reviews").glob("*/human-verdict.json"))
    expect(len(reviews) >= 4, "there are recorded verdicts to check")
    for verdict in reviews:
        manifest = verdict.parent / "offline-package/attended-review-manifest.json"
        if not manifest.exists():
            continue
        result = run(
            "validate-verdict", "--manifest", str(manifest), "--verdict", str(verdict)
        )
        expect(
            result.returncode == 0,
            f"recorded verdict {verdict.parent.name} still validates: {result.stderr.strip()}",
        )


def main() -> int:
    with tempfile.TemporaryDirectory() as raw:
        root = Path(raw)
        test_choice_group_arithmetic(root)
        test_batches_without_a_choice_group_are_unaffected(root)
    test_recorded_verdicts_still_validate()

    for failure in FAILURES:
        print(f"FAILED: {failure}", file=sys.stderr)
    if FAILURES:
        return 1
    print("attended-review-verdict-tests:pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
