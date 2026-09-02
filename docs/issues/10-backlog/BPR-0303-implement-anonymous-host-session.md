---
id: BPR-0303
title: Implement viewer-less anonymous host sessions
status: Backlog
rfc: RFC-BPR-0017
depends_on: [BPR-0300, BPR-0302, BPR-0531]
---

# Implement viewer-less anonymous host sessions

## Required final state

- The application host owns a stable anonymous scope without zero/default
  authenticated account identity and performs no enrollment or vault lookup.
- Anonymous/authenticated/host switches invalidate every viewer-, account-,
  capability-, queue-, epoch-, cache-, and projection-sensitive generation.
- Concurrency, cancellation, stale completion, refresh/rate refusal, shutdown,
  and repeated switching are bounded and tested with manual executors/counters.
- Mutations, triage, queue roles, and viewed/resolution actions remain
  unavailable rather than approximated. BPR-0317 owns the distinct credential-
  free public Git/cache/epoch/diff authority.
