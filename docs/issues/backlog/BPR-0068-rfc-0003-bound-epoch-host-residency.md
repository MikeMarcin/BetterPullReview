---
id: BPR-0068
title: Bound epoch host job and ingress residency
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0045, BPR-0055, BPR-0056]
---

# Bound epoch host job and ingress residency

The RFC-BPR-0003 application host claims one bounded FIFO actor per repository,
but `authority_effect_actor::Queue`, the host `Jobs` ledger, and reentrant
`DeferredIngress` are structurally unbounded. Cancellation and supersession
request stop without retiring queued closures or job controls, so accepted
refreshes behind a stalled effect can grow resident host state until the actor
and decision lane drain.

Make the retained `Jobs` collection the explicit global epoch-host admission
ledger with a public operational capacity. Admit a job only when the control,
actor closure, worker submission, and eventual serialized completion can all
remain within that bound. Every queued, running, or completion-pending actor
operation must retain exactly one ledger entry, so the invariant bounds total
jobs, the sum of actor FIFOs, each individual FIFO, and decision completions
awaiting retirement. Release the slot before dispatching its terminal so
terminal-driven replacement work can proceed.

Do not silently erase canceled FIFO entries or weaken per-repository ordering.
Capacity refusal must produce the exact request-specific
`quota_exceeded` terminal; allocation/executor refusal must produce
`unavailable`; rejected offers must revoke their smart authority and publish
the matching bounded diagnostic. Roll back partial admission before invoking
any terminal.

Give `DeferredIngress` a separate explicit capacity. Ordinary accept/dispatch
overflow must be rejected before core acceptance with exact cleanup and one
bounded deferred diagnostic. Invalidation and shutdown are terminal-priority:
deduplicate or replace matching lower-priority deferred work as necessary,
never lose those events, and retain FIFO order among admitted ordinary
entries. Include every epoch-host-owned deque in the audit; record the similar
GitHub-host ingress surface as separate future scope rather than expanding
RFC-BPR-0003 silently.

## Required direct evidence

- A deterministic manual-executor refresh flood exceeds the job capacity
  while effects and decisions are withheld. It proves one actor drain,
  capacity-bounded jobs/closures/completions, exact quota terminals,
  stale/canceled older work unable to publish, slot reuse after retirement,
  and shutdown quiescence with work pending.
- A reentrant projection-callback flood exceeds the ingress capacity and mixes
  accept, dispatch, matching invalidation, and shutdown. It proves projection
  depth remains one, rejected leases are revoked, diagnostics remain bounded,
  invalidation/shutdown survive, and exact quiescence is reachable.
- Focused host, core, maintenance, persistence, sanitizer, architecture,
  documentation, workflow-absence, and diff-hygiene gates pass from relevant
  clean builds.

Workers remain Qt-free, no semantic state moves into the executor, and no
GitHub Actions or remote mutation is involved.

## Scheduling disposition

This resource-hardening claim remains required before final RFC-BPR-0003
Auto-Verified closeout, but it does not block replacement RFC-BPR-0004
implementation by product-owner direction. Resume it as an explicit issue
slice; do not silently broaden RFC-BPR-0004 or weaken this card's flood and
quiescence evidence.
