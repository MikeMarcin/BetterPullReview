---
schema: "implement-rfc.card.v1"
id: "BPR-0795"
title: "Show PR loading promptly and reserve lazy objects for large reviews"
status: "Ready"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0793"]
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
---

# Show PR loading promptly and reserve lazy objects for large reviews

Mandate r16 adds the owner's experience requirement to the current selected-diff
milestone: opening any pull request should transition promptly to a visible
loading state. Small pull requests should continue loading their complete Git
comparison asynchronously in the background. Only pull requests classified as
large from bounded accepted snapshot facts may defer selected-file or older-epoch
objects until the user requests them. This companion work preserves BPR-0794's
exact-target selected-diff acceptance.

## Acceptance

- A public composition test opens a pull request with the provider effect held,
  observes the loading state after a paint turn, and proves no provider or Git
  effect blocks that first visible transition. Diagnose and correct any measured
  decision-turn obstruction rather than masking it with a spinner alone.
- A deterministic policy uses validated bounded snapshot shape and documented
  limits to classify small versus large. Missing, inconsistent, stale or
  superseded shape never silently grants lazy object loading.
- Small reviews keep complete Git acquisition on an effect worker and reach exact
  diff readiness without a selected-file-triggered object read. Large reviews
  defer older epoch objects until that epoch is selected and acquire only the
  exact current selected file objects needed for immediate comparison. Both
  routes preserve authority, cancellation, budgets, and explicit unavailable
  states.
- Focused positive, negative and stale-switch checks, a composed loading and
  small/large branch check, documentation gate, and an integration-focused check
  pass. No live pull request write, account change, token extraction or private
  content retention occurs.

## Evidence

Pending. The existing core `open_pull_request_intent` publishes
`loading_pull_request` before the provider fetch starts, and the provider posts
network/normalization work to an effect executor. The reported pause before a
paint is not yet attributed. Accepted snapshot file and commit counts exist at
the application handoff boundary; the epoch handoff does not yet carry them.
