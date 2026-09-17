---
schema: "implement-rfc.card.v1"
id: "BPR-0845"
title: "Two SQLite storage rows fail contained on Windows"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-creation-descriptor new defect","found_at":"6c633c3dc34e4e0622338e193ebfe6b692ed5ffe","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0845: Two SQLite storage rows fail contained on Windows

## Defect

On the Windows station, two rows registered contained fail inside the
AppContainer:

- `sqlite_queue_enrollment_storage`: `FAILED: queue port creates its state directory`;
- `sqlite_workbench_settings_storage`: `FAILED: explicit defaults factory creates private storage`.

Both fail the same way with [[BPR-0833]]'s adapter change reverted, so the
failures predate it. Because the rows are registered contained, the manifest
gate does not report them.

Reported by `bpr-win` in `task-20260917-win-creation-descriptor`, not carded
there.

## Scope

- Measure each row's first failing call contained, and compare with an
  uncontained run.
- Fix the cause: likely the same inherited-descriptor or canonicalisation
  shape as BPR-0833 and [[BPR-0812]]. Otherwise pin the row with the measured
  reason.

## Acceptance

- Both rows pass on the Windows station as registered, or are pinned with a
  measured reason, and `test_network_isolation_manifest` reports no finding.
- They still pass on macOS.
