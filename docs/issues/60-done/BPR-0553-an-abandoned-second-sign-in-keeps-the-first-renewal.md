---
id: BPR-0553
title: An abandoned second sign-in keeps the first session's renewal
status: Done
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

## Landed — 2026-09-03

At `02d53ec8`. The card asked for the release to move from the gesture to the
authorization; the worker verified that reading and found it right about
*when* and incomplete about *which*, so the fix is narrower than the card's
wording and better for it.

**Conditioned on provenance, not on timing alone.** An unconditional release
at the top of `authorize()` would make every grant park its material,
destroying the documented rotation order — the refresh entry becomes durable
before the access token is enrolled — and making the "the flow itself fails"
storage branch unreachable, which six existing tests depend on. So
`perform(begin)` now records whether a binding predates this flow rather than
releasing anything, and `authorize()` releases before the store decision only
when the binding was inherited. A second grant's binding always predates its
own flow, so [[BPR-0550]]'s defect stays fixed; an abandoned, denied, or
failed flow now changes nothing at all.

**One timer, two purposes.** Keeping the binding was only half of "exactly as
it was". The host owns a single timer and a second flow's poll displaces the
armed renewal, so a denied second flow still left the observation reading
`scheduled` with nothing armed — the same silent drop by another route, and
the reason the card's "or is cancelled" case was untestable, since cancel
cancels the timer outright. `restore_displaced_renewal` re-arms at a terminal
other than `authorized` when no timer is armed and the observation still says
`scheduled`, recomputing the wait from `AuthorizedAt` so it cannot fire in the
past. The terminal gate is load-bearing: called earlier it would take the
timer back from the poll and the flow would never end.

**The three binds do not move back.** A bind placed before `begin` is exactly
what the new provenance bit marks as the previous session's, so it is released
at the authorization just as it was at the begin. [[BPR-0550]]'s comments on
those tests remain accurate.

| Claim | Command | Result | Commit |
|---|---|---|---|
| A second flow that fails leaves the first renewal armed, on the same timer | `application_device_authorization_refresh` — new case | passes; pre-fix `FAILED: and the renewal is the same one, on the same timer` | this commit |
| A denied second flow gets the first renewal back | the second new case, registered first so it runs first | passes; pre-fix `FAILED: the first session's renewal is still what the host reports` | this commit |
| BPR-0550's second-grant case still holds | `test_a_second_grant_stores_under_the_authority_its_own_session_named` | unchanged and passing, not rewritten | this commit |
| Every device, vault, and public-path row | the 16-row selector set | 16/16 across three runs | this commit |
| Boundaries, tidy, format | `check_boundaries`; `tidy.py` over 2 translation units; `clang-format --Werror` | exit 0; exit 0; exit 0 | this commit |

The worker's first attempt at the second pre-fix run was invalid — dropping a
registration tripped `-Werror,-Wunused-function`, the build failed, and the
stale binary reprinted the first test's message — and it caught that by
reading the build log rather than trusting the output. Recorded because that
is the failure mode this project has a memory about.

**Left open, deliberately outside this card.** A renewal falling due *during*
a second flow still fires late, by up to the flow's duration, because the poll
holds the one timer until its terminal; the observation also reads `scheduled`
with nothing armed for that window. Carded as [[BPR-0554]] and kept out of
RFC-BPR-0016's closure: the renewal is late in a narrow window rather than
lost, which is follow-up work, where the case this card fixed lost it
permanently on any abandoned sign-in. The composition dedup the worker
flagged as a third risk is already answered by BPR-0550's memo drop on the
edge into `authorized`, verified at this head.

