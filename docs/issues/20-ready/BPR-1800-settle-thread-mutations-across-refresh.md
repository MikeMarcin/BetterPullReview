---
schema: "implement-rfc.card.v1"
id: "BPR-1800"
title: "Settle admitted thread mutations across an accepted refresh"
status: "Ready"
rfcs: ["RFC-BPR-0008", "RFC-BPR-0091", "RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate": "release-v040-typed-mutation-lifecycle-r1", "milestone_plan": {"id": "thread-resolution-refresh-lifecycle", "path": "docs/rfcs/plans/thread-resolution-refresh-lifecycle/milestone-plan.json"}}
---

# Settle admitted thread mutations across an accepted refresh

The controlled public composition admits Resolve while a second Refresh read is
held. Accepting that read retires the mutation host; releasing the successful
resolution terminal then leaves the card pending indefinitely. This is a product
interaction, not a Debug timing exception. Original Windows 40-second and
243.197-second failures remain failures. No real GitHub write was used.

## Acceptance

- AC1: Establish the typed ownership and lifecycle contract, with a controlled
  public regression that holds the read and terminal independently. The admitted
  mutation settles across accepted same-context refresh in Debug and Release.
- AC2: Preserve exact request, authority, account, document and head binding.
  Success, failure, refusal, cancellation/unknown, stale/duplicate terminals,
  genuine context retirement and shutdown are safe, with no duplicate write.
- AC3: Run the affected native Windows functional course on repaired source
  after the shared Windows/WSL machine is quiet. Preserve original failure
  artifacts and keep the five other agent-course failures separate unless an
  actual common mechanism is demonstrated.
- AC4: Run focused affected integration, format and documentation checks;
  intentionally commit the repair and upkeep, record exact evidence and pending
  release obligations, and return at this milestone boundary.

## Authority and evidence

The release owner authorized implement-rfc repairs for actual release failures.
The coordinator assigned this sole non-delegating milestone under mandate
`release-v040-typed-mutation-lifecycle-r1`. Its initial source is
`b1dcb8ba2482ffed96712764ee05efe830431e50`; allocation-only commit
`b29afb66c` was consumed as `7eb7e05ad`. External evidence is under
`/Users/jaeger/.codex/release-runs/v0.4.0-20261028/qualification-repairs-r1/`:
`threaded-refresh-controlled-r1`, `windows-threaded-reply-native-r1` and
`windows-agent-completion-native-r1`. No numbered cohort is allocated here.
