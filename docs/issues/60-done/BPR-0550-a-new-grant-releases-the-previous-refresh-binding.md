---
schema: "implement-rfc.card.v1"
id: "BPR-0550"
title: "A new device grant releases the previous refresh binding"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: []
---

# A new device grant releases the previous refresh binding

## Why

The fourth independent review of RFC-BPR-0016 at `1f1f986c` ([[BPR-0285]])
found that only the first device grant in a process binds its own authority.
`RefreshAuthority` is assigned by `bind_refresh_authority` and `resume_refresh`
and never released: `perform(begin)` clears the parked material, the
authorizing context, and the flow transients, but not the binding. So a second
grant reaches `authorize()` with a binding already held, takes the
`RefreshAuthority.has_value()` branch, and stores its refresh material under
the *previous* session's authority key.

For a second grant naming the same account that is harmless — the key is the
same. For a different account it is not. The material lands under the old key;
the incoming session's `/user` names the new account; [[BPR-0549]]'s window
sees the mismatch and erases the old slot, which is the material the new grant
just stored. Nothing leaks, which is why the erasure work holds. But
`RefreshDurable` is already true from the mis-keyed store, so the host arms a
renewal and the panel renders `scheduled` for a chain that cannot be spent;
at the lead time the retrieve answers absent and the reviewer is told to
enroll again, minutes before a token that was working expires.

The same shape reaches a pasted token: composition rebinds the device host to
a manual session's authority while the device stage is still `authorized`,
and the grant's armed renewal then fires against that authority and renders
the re-enrollment line for a valid personal access token (the review's R3).

This has been latent since [[BPR-0536]] composed the lifecycle and became
effective only when [[BPR-0546]] made the store succeed at all. No registered
row can see it: both composed courses drive one account.

## Required final state

- A new flow releases what the previous one bound. `perform(begin)` clears
  the refresh binding, its identity generation, and `RefreshDurable`, and
  cancels an armed renewal, so the new grant parks its material until the
  new session's `/user` binds it — the first-grant path the courses already
  prove.
- Composition drops its device-refresh authority memo when the device
  presentation reaches `authorized`, or on the GitHub host's invalidation, so
  the next acceptance rebinds and flushes rather than early-returning on a
  stale key.
- A host test proves a second grant stores under the authority its own
  session named, not the previous one, failing before the fix.
- A composed course covers it if the fixture can name a second account; if it
  cannot, that limitation is recorded rather than worked around.

## Non-goals

- Changing when material is erased ([[BPR-0540]], [[BPR-0547]],
  [[BPR-0549]] stand).

## Landed — 2026-09-03

At `45ad5e1e`. Two changes, in the two layers the defect spans.

**The host releases what a new flow supersedes.** A new
`release_refresh_binding()` clears `RefreshAuthority`, its identity
generation, and `RefreshDurable`, resets the renewal observation to `idle`,
cancels an armed renewal timer (matching on the timer's refresh purpose only,
so a live poll is untouched), and cancels the in-flight refresh, retrieve,
and store contexts. The cancellation is load-bearing rather than tidy:
`on_refresh_material` and `on_refresh_token` dereference `*RefreshAuthority`
unguarded, so releasing a binding under an in-flight renewal without
cancelling it would be undefined behaviour. `perform(begin)` calls it beside
the existing transient resets.

`bind_refresh_authority` also releases a previous binding when the incoming
authority or generation differs. That is the smaller answer to the review's
R3 — a pasted token rebinding the device host while a grant's renewal is
armed — and it belongs in the host: only the host knows that nothing is
durable under a slot it has never written, where composition can only see
that a key changed.

**Composition drops its memo on the edge into `authorized`.** Edge and not
level: a level test would also drop it at `idle`, and the next acceptance
would re-enter the restart branch and resume a second time, which the
`REFRESH_RESUMES:0` pins would catch. Without this the host fix alone would
strand a same-account second grant, because an equal-key memo early-returns
and the binding is never remade.

**A harness consequence, taken deliberately.** The host cannot tell a binding
made by the previous session from one made by the current session before
`begin`, so releasing at `begin` also releases a pre-bound authority. Three
refresh tests and the shared `authorize_with_bound_authority` helper bound
before reaching the awaiting stage; those binds moved inside the flow. Every
downstream assertion — order log, timing, rotation — is unchanged, and the
store-before-enroll ordering invariant still governs the shape it was written
for.

| Claim | Command | Result | Commit |
|---|---|---|---|
| A second grant stores under the authority its own session named | `application_device_authorization_refresh` — new case `test_a_second_grant_stores_under_the_authority_its_own_session_named` with a second-account authority | passes; pre-fix `FAILED: the second grant's material waits for its own authority` | this commit |
| Every device, vault, and public-path row | the 16-row selector set | 16/16, refresh course 9.15 s against its 12 s budget | this commit |
| The composed round trip and its pins | `application_review_round_trip_composition_contract` alone | passed, 68 s, after two repins | this commit |
| Boundaries, tidy, format | `check_boundaries`; `tidy.py` over 4 translation units; `clang-format --Werror` | exit 0; exit 0; exit 0 | this commit |

**Left open.** No composed course covers a second grant: the fixture drives
one account and has no second-account credential, so a composed second grant
would authenticate as the same account and prove nothing about the key. The
host test is the only guard, and that limitation is recorded rather than
worked around. A bind arriving mid-flow is still trusted; closing that window
needs a session-identity signal the host does not have. The memo drop is
edge-triggered, so a build that published `authorized` twice with no
intervening stage would not drop it.
