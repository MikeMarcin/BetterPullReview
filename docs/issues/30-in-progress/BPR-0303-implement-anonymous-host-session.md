---
id: BPR-0303
title: Implement viewer-less anonymous host sessions
status: In Progress
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

## Lane — 2026-09-02

[[BPR-0300]], [[BPR-0302]], and [[BPR-0531]] are terminal, so this card
is claimed straight into `30-in-progress/` by the integrator: its worker started in a worktree on the head that carries BPR-0531, under the transition exception. What they
leave at its door: core's `github_access_mode` and the mode-discriminated
`pull_request_authority` (zero account and viewer under public preview, by
rule); `mode_change_invalidation`, which yields the same
`invalidate_github_identity_request` an account switch yields; the
adapter's `github_provider_read_authority` chosen at provider construction;
an anonymous `fetch_pull_request` that refuses any read shape that is not
all-false; and a snapshot whose REST review comments are a distinct
unavailable-or-observed field. The host's own gates — the session read model
refusing a zero account, the capability and credential authority keys, and
the epoch-catalog key — are the surfaces this card routes an anonymous
session around, never by relaxing them.
