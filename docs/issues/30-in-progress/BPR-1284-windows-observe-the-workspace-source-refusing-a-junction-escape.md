---
schema: "implement-rfc.card.v1"
id: "BPR-1284"
title: "Windows: observe the workspace source refusing a directory junction escape"
status: "In Progress"
rfcs: ["RFC-BPR-0054"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"origin":"BPR-1187 closeout reconciliation (RC54-03)","reservation":"docs/issues/_reservations/BPR-1284--BPR-1284--d375e5454dd1.json","related_cards":["BPR-1148","BPR-1187","BPR-1244"]}
---

# BPR-1284: Windows: observe the workspace source refusing a directory junction escape

## Observation

RC54-03 of RFC-BPR-0054 reads: "Reject root replacement, symlink/junction
escape and unsafe descendant paths under native filesystem semantics." No run
has ever made a junction. `tests/workspace_contract/workspace_source_tests.cpp`
creates only symbolic links, and nothing else in the tree makes a junction for
the workspace source. On Windows its link rows ran on `bpr-main-win`, a machine
that can make symbolic links (recorded on [[BPR-1244]], 2026-10-01).

From reading the code, the Win32 adapter
(`src/adapters/workspace/src/local_workspace_source.cpp`) refuses a descent
through any object with `FILE_ATTRIBUTE_REPARSE_POINT`, whatever its tag, and
lists a non-symlink reparse point as kind `other`. A junction should therefore
take the branch the symbolic links took. The pack's verification contract says
a related code path is not an observed pass, so [[BPR-1187]] cannot close
RFC-BPR-0054 on that reading.

## Outcome

On native Windows, observe the workspace source refuse a directory junction
(`IO_REPARSE_TAG_MOUNT_POINT`, which needs no symlink privilege) pointing
outside the root:

- listing, stat and reading through the junction are refused as `escape`;
- a directory swapped for a junction after it was first read is refused;
- a capture lists the junction as itself (kind `other`, unsupported) and reads
  nothing beyond it.

A row in `workspace_source` (or `review_cycle_capture` for the capture half)
that makes the junction, for example with `mklink /J` or
`FSCTL_SET_REPARSE_POINT`, is the expected form. It must report itself as run.
Run it on `bpr-main-win`, contained as registered or under its measured
`WINDOWS_UNCONTAINED` pin. As a negative control, a build that follows the
junction must fail the row.

## Acceptance

- The junction rows pass natively on Windows, and the run is recorded here
  and in the [execution ledger](../../verification/RFC-BPR-0053-0061.md) under
  BPR-1187, with its revision and environment.
- If the adapter follows a junction, fix it under RFC-BPR-0054 and record the
  failure first.

## Result (bpr-main-win, 2026-10-04)

Observed natively on Windows. The adapter refuses a directory junction, as
the code reading predicted, so no product change was needed. New:
`tests/workspace_contract/windows_junction.hpp`, a junction made with
`FSCTL_SET_REPARSE_POINT` and no privilege. Rows:
`refuses_junction_escapes` in `workspace_source` and
`captures_a_junction_as_itself` in `review_cycle_capture`.

- **Escape refused.** The junction is listed as kind `other`. List, stat and
  read through it are refused as `escape`, and so is a directory swapped for
  a junction after it was read. The row prints `NATIVE: directory-junction
  escapes refused`.
- **Capture.** The junction is listed as itself, `unsupported_kind`, with no
  content and nothing beneath it.
- **Negative control.** With reparse detection narrowed to symbolic links,
  the junction rows fail at their first junction check and the symbolic-link
  rows still pass.
- Both rows run as registered (`WINDOWS_UNCONTAINED`). The ledger entry is
  under BPR-1187 in the [execution ledger](../../verification/RFC-BPR-0053-0061.md).
