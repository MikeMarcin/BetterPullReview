---
schema: "implement-rfc.card.v1"
id: "BPR-0855"
title: "application_epoch_host fails on Linux at the superseded durable commit"
status: "In Progress"
rfcs: ["RFC-BPR-0029"]
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-0851-sweep full ctest sweep","found_at":"a1864fa296ce3450ce0e0470f36bd0d95cd59470","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0855: application_epoch_host fails on Linux at the superseded durable commit

## Defect

`application_epoch_host` fails on `bpr-linux` at `a1864fa2` with
`FAILED: superseded durable commit rebases the latest selection and keeps core/catalog synchronized`.
It fails in 0.26 s in the sweep and in 0.23 s alone
(`docs/verification/dsp4-linux-confirmation.md`).

On macOS the row passes: 0.73 s at `20d99914`
(`build/r2-logs/dsp4-linux-sweep-mac-classify.log`), and earlier 0.54 s in
dsp-003's bundle-focused set.

## Scope

- Name why the case diverges on Linux: ordering, timing, a platform
  difference in the host test, or a product defect.
- Fix the cause. A product fix in epoch selection or catalog synchronization
  gets a review.

## Acceptance

- The row passes on Linux 3 runs in a row, still passes on macOS, and the
  cause is named here.
