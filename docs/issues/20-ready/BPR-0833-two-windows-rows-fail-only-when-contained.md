---
schema: "implement-rfc.card.v1"
id: "BPR-0833"
title: "Two Windows rows fail only when contained"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-hardening new defect","found_at":"711d3ddc5f8db9f237e4c4b84758d116b9e897af","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0833: Two Windows rows fail only when contained

## Defect

Two rows pass uncontained on Windows and fail inside the AppContainer:

- `application_composition_capabilities_inside_boundary` prints
  `STARTUP_FAILED` contained. Uncontained, the same probe prints
  `COMPOSITION_TEST_BOUNDARY:inside`, a recording browser and no CLI read.
  No person's app was running.
- `sqlite_profile_storage_root` is registered contained but fails contained:
  exit 3, because its scratch root calls `std::filesystem::canonical` first
  (the same `Access is denied` shape [[BPR-0812]] measured). It passes
  uncontained. Its sources do not touch [[BPR-0813]]'s open path.

Reported by `bpr-win` in `task-20260917-win-hardening` (steps 1 and 4),
not carded there.

## Scope

- Measure each contained failure's first failing call.
- [[BPR-0820]] changes how `application_composition_capabilities_inside_boundary`
  composes its profile (a private, owner-private automated root). Rerun it on
  a `windows-lane` that contains BPR-0820 before diagnosing it.
- Fix the cause, or pin the row with its measured reason in
  `scripts/verify_test_network_isolation.py` as BPR-0812 did. A pin needs a
  passing uncontained run and the measured contained cause.

## Acceptance

- Both rows pass on the Windows station as registered, or are pinned with a
  measured reason, and `test_network_isolation_manifest` reports no new
  finding.
