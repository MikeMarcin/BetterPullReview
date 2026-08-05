---
id: BPR-0160
title: Core: attention buckets, reasons, and queue navigation
status: Backlog
rfc: RFC-BPR-0010
depends_on: [BPR-0158]
---

# Core: attention buckets, reasons, and queue navigation

Seven buckets, derived from GitHub activity and local review state: initial
review requested, changed since my review, new comments or author replies,
fixes awaiting verification, authored reviews awaiting reviewer action,
observed with new activity, and quiet or current.

Each item carries the reason it is where it is. A reviewer looking at an
unfamiliar pull request in their own queue is owed that.

## Required direct evidence

- Every bucket is reached from a bounded observation set, and the boundaries
  between adjacent buckets are exercised rather than assumed.
- The reason travels with the item rather than being recomputed by whoever
  presents it, so two surfaces cannot disagree about why something is there.
- Replaying one observation set twice produces identical buckets and identical
  ordering — not merely equivalent.
- Nothing in this slice reads a clock or schedules a timer. It builds and runs
  under `core-only`.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
