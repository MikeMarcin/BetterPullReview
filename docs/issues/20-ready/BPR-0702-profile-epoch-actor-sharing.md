---
schema: "implement-rfc.card.v1"
id: "BPR-0702"
title: "Share profile cache actors with independent window epoch state"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0697", "BPR-0698"]
---

# Share profile cache actors with independent window epoch state

## Scope

Extract a bounded profile-owned cache actor registry from epoch_application_host.
Share the exact-authority repository cache session and serialized FIFO, while
retaining selected-diff/blob/realignment warrants, PR catalog sessions, jobs,
projection and cancellation per window. Include access mode in actor identity.
Admit exact profile/window context; no second cache open for a sibling lease.
Last-consumer retirement releases the session only after admitted work drains.
Profile shutdown separately refuses admission and drains owned actors.

Keep the current facade using the same extracted implementation. Executors are
supplied by the profile owner and outlive quiescence. No Git/SQLite schema or
Qt changes; actual multiple-window construction remains later integration.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns architecture, app composition, CMake, bookkeeping and acceptance.
Workers own separate epoch or ledger paths and preserve parallel work.

## Acceptance

Two valid window consumers share one cache open and retain independent selected
comparisons. Closing A preserves B work/projection; final detach/drain closes
once and later admission reopens once. Different host/account/revision/object
format/access mode never coalesce; foreign/stale contexts refuse before effects.
Existing epoch host contracts and a composed epoch path remain passing.

## Evidence

Bounded read-only ownership investigation identified this slice. No new code
or acceptance check has run. Full same-profile multi-window criteria remain open.
