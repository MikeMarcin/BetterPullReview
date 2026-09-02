---
id: BPR-0300
title: Implement deterministic anonymous-read policy
status: Ready
rfc: RFC-BPR-0017
depends_on: [BPR-0035, BPR-0312]
---

# Implement deterministic anonymous-read policy

## Required final state

- Core owns explicit public-preview mode, stable request/revision identity,
  legal read-only intents, unavailable viewer/thread-resolution/viewed/action
  observations, rate/private/GHES reasons, and authenticated-switch policy.
- No provider, URL, authorization, vault, account credential, clock, thread,
  filesystem, Qt, database, or default synthetic viewer enters core.
- Replay/property tests cover every legal/illegal action, stale/duplicate
  observation, switch, refusal, rate terminal, and missing capability.

## Lane — 2026-09-02

[[BPR-0312]] is satisfied: the owner's verdict validated on 2026-09-02 and
the four public-preview definitions are bound in
`docs/verification/reviews/rfc0017-public-preview-decision-v001/validation-record.json`.
Two of them shape this card's core. The mode is named "Public preview" and
is stated as signed out wherever it is named, never as the authenticated
workbench with fields missing, so core's mode is explicit and its
unavailable observations are typed rather than defaulted. Budget exhaustion
is a typed refusal that names the limit and the reset time, and every
refusal is a place the sign-in handoff is offered from, so the rate
terminal and the authenticated-switch policy must carry what a presentation
needs to say that. [[BPR-0035]] is terminal. Both dependencies hold, so this
card moves to `20-ready/` as generic work runnable on any station.
