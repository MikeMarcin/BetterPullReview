---
id: BPR-0240
title: Complete agent context and process lifecycle semantics
status: Backlog
rfc: RFC-BPR-0011
depends_on: [BPR-0238]
---

# Complete agent context and process lifecycle semantics

The current launch builder omits required context from child input. The process
adapter waits for completion, reads stdout once, does not drain stderr, and does
not classify nonzero exit status. Those are component gaps, not merely missing
composition.

## Required final state

- Serialize the PR URL, expected base/head OIDs, selected epochs, previous
  upload, last-reviewed head, changed/unread/accepted file state, local defect
  state, current selected file/semantic selection, and only explicitly included
  private notes or drafts. Version the supplement and keep absent fields
  distinguishable from empty values.
- Carry exact host/repository/PR/base/head/epoch/session identity and the
  permitted semantic segments, groups, mapping confidence, and selected source
  text without presentation coordinates or live scroll state.
- Implement and test Codex App Server, Codex JSONL, Claude stream-JSON, and
  external-terminal protocol behavior rather than treating one canary framing
  as proof for all profiles.
- Stream stdout incrementally, drain stderr without deadlock, and preserve
  structured protocol framing across partial reads.
- Distinguish failed-to-start, nonzero exit, protocol failure, cancellation,
  timeout, stale result, and successful completion as typed outcomes.
- Prove bounded output/backpressure and teardown without leaking credentials or
  placing context in argv.
- A canary asserts all required context fields, stderr pressure, partial
  frames, nonzero exit, cancellation, and stale-result rejection.
