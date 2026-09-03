---
id: BPR-0553
title: An abandoned second sign-in keeps the first session's renewal
status: Ready
rfc: RFC-BPR-0016
depends_on: []
---

# An abandoned second sign-in keeps the first session's renewal

## Why

The fifth independent review of RFC-BPR-0016 at `1bec6179` ([[BPR-0285]])
found a reachable regression in [[BPR-0550]]'s own changed line, and it is
the integrator's to own: the fix released the refresh binding too early.

`perform(begin)` calls `release_refresh_binding()`, which cancels the armed
renewal timer, clears the authority and `RefreshDurable`, and resets the
observation to `idle`. The panel enables *Get a code* in every terminal
stage, `authorized` included, and the core accepts a begin from there. So a
reviewer with a live, renewing session who presses *Get a code* again and
then abandons, denies, or fails that second flow is left with the first
session's access token still valid in the vault and its renewal silently
gone. The state is `idle`, not `reenrollment_required`, so nothing is said;
about eight hours later the token expires with nothing to renew it. Before
BPR-0550 the first renewal stayed armed through a failed second begin.

Nothing leaks and no wrong material is stored — this is strictly the
renewal being dropped — but acceptance row 9 says refresh precedes expiry,
and after an abandoned re-authorization it does not. No verification surface
exercises it: both courses drive one flow.

## Required final state

- A flow that begins and does not succeed leaves the previous session's
  renewal exactly as it was. The release moves to the point where the new
  grant's material is about to become durable — the authorization itself —
  rather than the gesture that starts a flow, so a second grant still never
  stores under the previous session's key ([[BPR-0550]]'s defect stays
  fixed) while an abandoned flow changes nothing.
- If a case genuinely requires releasing earlier, the reviewer is told:
  the observation reports `reenrollment_required` rather than `idle`, so a
  dropped renewal is never silent. Choose one and record the reason.
- Host tests for both halves, each failing before the fix: a second grant
  that authorizes stores under its own session's authority (BPR-0550's
  case, which must keep passing), and a second flow that begins and then
  fails or is cancelled leaves the first session's renewal armed and its
  observation unchanged.

## Non-goals

- Changing what a successful second grant does ([[BPR-0550]] stands).
- The mid-flow bind window, which is recorded on BPR-0550 and unchanged.
