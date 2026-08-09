---
id: BPR-0240
title: Complete agent context and process lifecycle semantics
status: Backlog
rfc: RFC-BPR-0011
depends_on: [BPR-0238]
---

# Complete agent context and process lifecycle semantics

The current launch builder omits last-reviewed head, changed-since-review,
defects, and selected-file context from child input. The process adapter waits
for completion, reads stdout once, does not drain stderr, and does not classify
nonzero exit status. Those are component gaps, not merely missing composition.

## Required final state

- Serialize every RFC-required context field with an explicit version and
  exact revision/session identity; absent fields remain distinguishable from
  empty values.
- Stream stdout incrementally, drain stderr without deadlock, and preserve
  structured protocol framing across partial reads.
- Distinguish failed-to-start, nonzero exit, protocol failure, cancellation,
  timeout, stale result, and successful completion as typed outcomes.
- Prove bounded output/backpressure and teardown without leaking credentials or
  placing context in argv.
- A canary asserts all required context fields, stderr pressure, partial
  frames, nonzero exit, cancellation, and stale-result rejection.
