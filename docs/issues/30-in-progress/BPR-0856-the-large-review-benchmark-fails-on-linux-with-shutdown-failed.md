---
schema: "implement-rfc.card.v1"
id: "BPR-0856"
title: "application_large_review_benchmark fails on Linux with shutdown failed"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-0851-sweep full ctest sweep","found_at":"a1864fa296ce3450ce0e0470f36bd0d95cd59470","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0856: application_large_review_benchmark fails on Linux with shutdown failed

## Defect

`application_large_review_benchmark` fails on `bpr-linux` at `a1864fa2` with
`large-review-benchmark-failed: shutdown failed`. It fails in 5.59 s in the
sweep, where it runs serially, and in 5.56 s alone
(`docs/verification/dsp4-linux-confirmation.md`).

On macOS it passes: 21.85 s at `20d99914`
(`build/r2-logs/dsp4-linux-sweep-mac-classify.log`).

The related `application_unavailable_diff_public_path` fails on both
platforms with a different message. It is carded separately.

## Scope

- Name what fails to shut down on Linux, and whether it is the benchmark's
  harness or the product's shutdown. See [[BPR-0847]] and [[BPR-0849]], where
  a shared `/tmp` parent made an owner's workspace recovery refuse and its
  drain exit 1.
- Fix the cause.

## Acceptance

- The row passes on Linux 3 runs in a row, still passes on macOS, and the
  cause is named here.
