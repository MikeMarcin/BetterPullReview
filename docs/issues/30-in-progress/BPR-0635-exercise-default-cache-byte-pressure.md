---
schema: "implement-rfc.card.v1"
id: "BPR-0635"
title: "Exercise default cache byte pressure"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0634","BPR-0612"]
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
---

# Exercise default cache byte pressure

BPR-0634 covers the default entry ceiling. Small eight-line results cannot
establish pressure against the independent default 64-MiB cache byte ceiling.

## Bounded acceptance and ownership

- Use the existing large-review stored heads and real public refresh/epoch/file
  selectors to admit enough distinct 20,000-line comparison keys to exceed the
  unchanged default 64-MiB retained diff/index budget before the entry ceiling.
  Do not lower limits, synthesize cached values or bypass installed Git.
- Verify exact source/anchor identities, bounded accounted cache bytes, actual
  eviction and weak raw/index ownership release. Publicly revisit an evicted
  key and verify its rematerialized exact source and stale-selection safety.
- Record hardware/workload, native memory, timings/input and bounded job/cache
  metrics before justifying any new workload-specific local thresholds. Exercise
  close/drain/destruction after pressure and preserve durable Git/catalog data.
- Reuse the existing benchmark/fixture configuration and inspection seams.
  Split wider defects if evidence requires different ownership. Preserve the
  entry-pressure course and earlier public acceptance; broader RFC and native
  platform closure remains separate.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
Local Mac, synthetic and network-denied only. No Actions, dispatch, push, paid
CI, credentials or production account/process operations. Earlier human/live
and Windows/Linux gates and deferred work remain open.

## Sequencing

Owner steering prioritizes the full Glaze migration in BPR-0612 before further
performance work. Resume this course after that migration so the measurements
exercise the intended parser implementation.
