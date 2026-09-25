---
schema: "implement-rfc.card.v1"
id: "BPR-1120"
title: "An rfc0016 operator course leaves bpr-operator-* temp roots containing read-only Git objects on Windows"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win segment 7, the core-only CTest run (94a09016..877e3d60) left 12 bpr-operator-* roots","related_rfcs":["RFC-BPR-0016"],"related_cards":["BPR-1103","BPR-0868","BPR-1121","BPR-1119"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1119--BPR-1120--11fa21d04f58.json"}
acceptance: [{"criterion":"AC1 The cause is named with evidence from a real Windows run","outcome":"pass","revision":"82d997bd","evidence":{"kind":"command","detail":"bpr-main-win, Python 3.14.7, TEMP/TMP at a new empty directory: the test as it was (685ae35f) printed OPERATOR_PACKET_VALIDATION_TESTS_OK and left 12 bpr-operator-* roots (one per test) holding 170 files, all under .git/objects and all with the read-only attribute; rmtree(ignore_errors=True) cannot unlink read-only files on Windows and hid the failure. The shared helper's read-only retry removes them all, so no handle was held.","path":"docs/issues/60-done/BPR-1120-an-rfc0016-operator-course-leaves-bpr-operator-temp-roots-with-read-only-git-objects-on-windows.md#named-fixed-with-the-shared-helper-and-nothing-is-left-bpr-main-win-2026-09-24"}},{"criterion":"AC2 The row passes on Windows through CTest as registered and leaves nothing in a fresh TEMP, in N of N runs (N >= 3)","outcome":"pass","revision":"82d997bd","evidence":{"kind":"command","detail":"build/dev configured at 8a56b80a: operator_response_validation_contract through CTest (Windows uncontained, as registered) passed 3 of 3 (12.88-12.99 s) and once more on the committed bytes (23.30 s), 0 entries left each time; direct runs 3 of 3, 0 left. With BPR-1119's row, contained_temporary_usage_contract, contained_temporary_support_contract and rfc0016_relocated_operator_contract: 5 of 5 passed. The rebased 82d997bd has an identical diff to the measured d94e5c09.","path":"docs/issues/60-done/BPR-1120-an-rfc0016-operator-course-leaves-bpr-operator-temp-roots-with-read-only-git-objects-on-windows.md#named-fixed-with-the-shared-helper-and-nothing-is-left-bpr-main-win-2026-09-24"}},{"criterion":"AC3 Negative control: a root that cannot be removed fails the row, naming what is left; and the test as it was leaves its roots again","outcome":"pass","revision":"82d997bd","evidence":{"kind":"command","detail":"The fixed test with test_success's root holding an open file (no delete sharing): exit 1, no OK marker, standard error and the raised RemovalFailed name held.txt under bpr-operator-589512475b8dfc9b with WinError 32. The test as it was passes and leaves 12 roots.","path":"docs/issues/60-done/BPR-1120-an-rfc0016-operator-course-leaves-bpr-operator-temp-roots-with-read-only-git-objects-on-windows.md#named-fixed-with-the-shared-helper-and-nothing-is-left-bpr-main-win-2026-09-24"}},{"criterion":"AC4 macOS and Linux keep their path","outcome":"pass","revision":"82d997bd","evidence":{"kind":"freeform","detail":"On POSIX the shared helper creates the root with tempfile.mkdtemp (0o700, as tempfile.TemporaryDirectory did) and removes it with a read-only retry that raises on failure, as TemporaryDirectory's cleanup did; only the prefix becomes bpr-operator-. No macOS or Linux run from this station.","path":"docs/issues/60-done/BPR-1120-an-rfc0016-operator-course-leaves-bpr-operator-temp-roots-with-read-only-git-objects-on-windows.md#named-fixed-with-the-shared-helper-and-nothing-is-left-bpr-main-win-2026-09-24"}}]
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

## Named, fixed with the shared helper, and nothing is left (bpr-main-win, 2026-09-24)

The change drops the test's own helper and uses
`contained_temporary_directory(prefix="bpr-operator-")` from
`scripts/contained_temporary_support.py`. It was authored as `d94e5c09`, and
the push gate's rebase made it `82d997bd` on main, with an identical diff.
Between the measured tree and main, nothing under `scripts/` or
`tests/scripts/` differs except this change and [[BPR-1119]]'s. The retained,
digest-bound `validate_operator_packet.py` is untouched.

Run `bpr-main-win-20260923`, `build/dev` configured at `8a56b80a`, Python
3.14.7. Each run was alone, with `TEMP` and `TMP` at a new empty directory
that was listed afterwards. The logs are station records under
`build/logs/bpr-1120-main-win/`, and are not committed.

**The cause (AC1).** The test as it was (`685ae35f`) printed
`OPERATOR_PACKET_VALIDATION_TESTS_OK` and left 12 roots, one per test. They
held 170 files, every one under `.git/objects`, and every one had the
read-only attribute. So `rmtree(ignore_errors=True)` failed on Git's
read-only object files and said nothing. Nothing held a file open: the
shared helper's read-only retry removes them all (AC2).

| Run | Result |
| --- | --- |
| Fixed, run directly, 3 runs | Exit 0 and `OPERATOR_PACKET_VALIDATION_TESTS_OK` every time (12.5–13.0 s). 0 entries left each time. |
| Fixed, `operator_response_validation_contract` through CTest as registered (Windows uncontained), 3 runs | Passed in 12.88–12.99 s. 0 entries left each time. |
| The same row on the committed bytes, 1 run | Passed in 23.30 s, and 0 entries left. The earlier runs had a space missing in `GATE = ...`, which was restored before the commit. |
| With [[BPR-1119]]'s row, serially: `operator_response_validation_contract`, `contained_temporary_usage_contract`, `contained_temporary_support_contract`, `rfc0016_relocated_operator_contract` and `rfc0016_production_course_security` | 5 of 5 passed. The usage gate also scans `scripts/`, so the new call's keywords are checked. |
| Negative control (AC3): the fixed test, with `test_success`'s root holding an open file (no delete sharing), in memory | Exit 1 with no OK marker. Standard error and the raised `RemovalFailed` both name the file: `contained temporary: could not remove ...\bpr-operator-589512475b8dfc9b\test_success\held.txt: [WinError 32]`. |
| The test as it was (AC3) | Passes and leaves its 12 roots again, as in AC1. |

**The older leftovers.** `%TEMP%` on the station held 270 `bpr-operator-*`
roots, last written 2026-08-18 to 2026-09-18.

- Every one matched the exact name, `bpr-operator-` and 16 hex digits.
- Every one held only directories named after this course's 12 tests.
- Between them they held 3,788 files, all read-only Git objects.

No process of this course was running. The station removed all 270 by that
rule, using the shared helper's removal, and none is left.

**AC4.** On macOS and Linux, the shared helper makes the root with
`tempfile.mkdtemp`, mode `0o700` as before. It removes the root with the
same read-only retry that `tempfile.TemporaryDirectory` applies there, and
it raises if the removal fails. Only the prefix changes, to
`bpr-operator-`. No run was made there from this station.
