---
schema: "implement-rfc.card.v1"
id: "BPR-1120"
title: "An rfc0016 operator course leaves bpr-operator-* temp roots containing read-only Git objects on Windows"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win segment 7, the core-only CTest run (94a09016..877e3d60) left 12 bpr-operator-* roots","related_rfcs":["RFC-BPR-0016"],"related_cards":["BPR-1103","BPR-0868","BPR-1121","BPR-1119"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1119--BPR-1120--11fa21d04f58.json"}
---

# BPR-1120: An rfc0016 operator course leaves bpr-operator-* temp roots containing read-only Git objects on Windows

## Defect

`operator_response_validation_contract` passes on Windows and leaves roots
behind in `%TEMP%` on every run. The row runs
`scripts/verification/rfc0016_attended_contract/scripts/test_validate_operator_packet.py`.
It leaves one `bpr-operator-<16 hex>` directory for each of its 12 tests.

- Each test makes a Git repository in its root, and Git writes its object
  files read-only.
- The test's own helper removes the root with
  `shutil.rmtree(candidate, ignore_errors=True)`.
- On Windows that cannot unlink a read-only file, and `ignore_errors` hides
  the failure.

Measured at `685ae35f` with Python 3.14.7. `TEMP` and `TMP` pointed at a new
empty directory, and the test file was run directly. It printed
`OPERATOR_PACKET_VALIDATION_TESTS_OK` and exited 0. It left 12 roots holding
170 files. Every file was under `.git/objects`, and every one was read-only
(`ReadOnly, Archive`). The station's own `%TEMP%` held 270 `bpr-operator-*`
roots.

The helper's comment says why it does not use the repository's
`scripts/contained_temporary_support.py`: "a skill pack resolves paths only
within its own directory". That stopped being true when the directory moved
out of the skill pack into `scripts/verification/` (`074f2473`). Its sibling,
`tests/scripts/rfc0016_relocated_operator_contract_tests.py`, already imports
the shared helper.

## Scope

- Use the shared helper, `contained_temporary_directory` ([[BPR-0868]]). It
  is the Python counterpart of [[BPR-1103]]'s `remove_course_root`: it clears
  a read-only bit and retries, and it names and raises whatever it still
  cannot remove.
- Keep the `bpr-operator-` prefix. On Windows, keep the root inheriting its
  parent's entries, which is why the test's own helper existed.
- Leave the retained validator alone. `validate_operator_packet.py` is
  digest-bound in `source-provenance.json`.
- Remove the station's older `bpr-operator-*` leftovers only if they are
  provably from this course, by exact name pattern and contents. Otherwise,
  list them.

## Acceptance

- AC1 The cause is named with evidence from a real Windows run.
- AC2 The row passes on Windows through CTest as registered. It leaves nothing
  in a fresh `TEMP`, in N of N runs (N >= 3).
- AC3 Negative control: a root that cannot be removed fails the row, naming
  what is left. The test as it was leaves its roots again.
- AC4 macOS and Linux keep their path. The shared helper uses
  `tempfile.mkdtemp` there and removes the root the way
  `tempfile.TemporaryDirectory` did.
