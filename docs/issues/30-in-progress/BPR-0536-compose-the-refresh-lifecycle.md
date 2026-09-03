---
id: BPR-0536
title: Compose the device-authorization refresh lifecycle
status: In Progress
rfc: RFC-BPR-0016
depends_on: [BPR-0450, BPR-0526]
---

# Compose the device-authorization refresh lifecycle

## Why

The independent review of the closeout head `28fa0741` ([[BPR-0285]],
2026-09-03) found that `bind_refresh_authority`, `resume_refresh`, and
`refresh_observation()` have no caller outside
`device_authorization_application_host` and its tests. Without a bound
refresh authority `authorize()` parks the refresh token in `PendingRefresh`,
`schedule_refresh()` returns at once, and nothing resumes at restart. In
the shipped product the user token therefore expires after about eight
hours with no renewal and no typed `reenrollment_required` ever reaching
Qt, while the long-lived refresh token stays resident in process memory for
the session. The ledger's claim-matrix row 9 ("refreshes before expiry and
atomically rotates access/refresh material under existing vault
authority") was verified only at the host level, which the ledger's own rule
forbids counting, and three documents state the product behaviour the
composition lacks.

The review also found a latent defect the composition would expose
([[BPR-0285]] residual R1): cancelling during a re-enrollment's refresh
store leaves `RefreshStoreContext` outstanding, so a later
`on_refresh_stored` sets `RefreshDurable` and arms a renewal from a
cancelled flow, and a failed store reports `reenrollment_required` after a
cancel. No test covers it.

## Required final state

- On the authenticated `/user` observation for the device-enrolled profile,
  composition binds the refresh authority; on profile resume it resumes
  refresh; `refresh_observation()` reaches the panel so `reenrollment_required`
  is presented.
- Cancel forgets the outstanding refresh-store context; a store completing
  after a cancel neither arms a renewal nor reports re-enrollment; tests
  cover both.
- A composed course proves a refresh before expiry rotates material under
  the existing vault authority through the real graph, and the tests-disabled
  Release composes the same binding.
- The ledger row, the RFC's token-handling disposition, and the three
  documents describe the composed behaviour.

## Non-goals

- Changing the host's refresh contract ([[BPR-0450]]) beyond the cancel fix.

## Lane — 2026-09-03

Claimed straight into `30-in-progress/` by the integrator under the
transition exception, both dependencies terminal. A scout confirmed the
review's reading before any edit: the host's lifecycle is complete and
tested, composition never calls `bind_refresh_authority` or
`resume_refresh`, the six-field presentation is a closed list so the
refresh observation needs a second projection method, `perform(cancel)`
lacks the store-context reset `shutdown()` already has, and the composed
clock has no testing seam, so a composed rotation needs a short-lived
documented grant or a clock seam. The slice runs on `bpr-0536-work` in a
worktree, after [[BPR-0537]] so the two do not edit `main.cpp` at once;
the three documents and ledger row 9 are the integrator's at close.
