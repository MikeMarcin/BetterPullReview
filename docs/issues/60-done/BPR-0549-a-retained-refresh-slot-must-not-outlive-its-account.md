---
id: BPR-0549
title: A retained refresh slot must not outlive the account it renews
status: Done
rfc: RFC-BPR-0016
depends_on: [BPR-0547]
---

# A retained refresh slot must not outlive the account it renews

## Why

[[BPR-0547]] stops a rotation from erasing the refresh material it just
stored: when a device grant supersedes a device-grant session on the same
profile, the invalidation half retains the slot instead of erasing it. The
condition is evaluated at `invalidate`, where the incoming enrollment's
provenance and profile are known but its **account** is not — the account
arrives with the authenticated `/user` observation that follows.

So one case retains material nobody can spend: a device grant that
supersedes a device-grant session on the same profile but for a *different
account*. The old authority's slot is keyed by the old account, the new
session never reaches it, and no later occasion erases it. It is the same
shape as the orphan [[BPR-0540]] closed, reached by a narrower door, and it
leaves a six-month refresh token in the platform vault after the reviewer
has signed in as somebody else.

The worker that delivered BPR-0547 recorded this deliberately rather than
widening its slice, and the reasoning is sound: closing it means deferring
the erasure decision until the new authority is stored and re-erasing on
mismatch, which adds a window that the session-failure and shutdown paths
must also answer for.

## Required final state

- A device-grant supersession retains the outgoing refresh slot only while
  the incoming session can still turn out to name the same account; once
  the new authority is known, a mismatch erases the outgoing slot under its
  own authority, and a session that fails or a shutdown inside that window
  erases it too.
- Host tests cover each of the three exits from the window: same account
  (retained, one erasure at the ordinary occasion), different account
  (erased on mismatch), and a failure or shutdown inside the window.
- The fixture reports enough to tell the cases apart, and the composed
  course pins the same-account path it already drives.

## Non-goals

- Changing when a manual enrollment erases; [[BPR-0540]]'s occasions stand.
- Persisting a session across a restart ([[BPR-0541]]).

## Landed — 2026-09-03

At `838b740c`. The withheld erasure is now held rather than dropped:
[[BPR-0547]]'s `retain` branch stores the exact operation context that would
have erased the slot, and that stored context is the window.

**How it closes.** On the first adoption of a new `CredentialAuthority` —
the bootstrap `/user` observation, and `select_persistent_credential` for a
resume — the held context is compared against the adopted authority by
**full key equality**, not account equality. That is the smaller and safer
predicate: the platform vaults key the slot by the whole authority key
(profile, profile revision, host, account, server contract, hostname), and
the incoming session's own refresh probe names that same key, so key
equality is exactly "something may still spend this" — BPR-0540's rule
stated precisely. It subsumes the different-account case this card names and
also catches a revision or host drift the card did not.

Three other exits erase under the outgoing authority: `reset_session_scratch`,
which every session terminal and the orderly shutdown pass through; the top
of a later `invalidate`, for a session discarded before it ever named an
authority; and a refusal to open the window at all when the host is already
closing. The erasure runs through `start_refresh_delete`, so it stays inside
the existing quiescence accounting. Over-erasure is the deliberate direction
on every ambiguous exit: the cost is a re-authentication, not leaked
material.

| Claim | Command | Result | Commit |
|---|---|---|---|
| A grant naming another account erases the outgoing slot | `application_github_host` — `test_a_retained_slot_leaves_when_another_account_arrives` | passes; at the pre-fix host source, `FAILED: a grant naming another account erases the outgoing slot under the outgoing authority` | this commit |
| A session that fails inside the window erases the slot | the first block of `test_a_retained_slot_does_not_survive_a_failure_or_a_shutdown` | passes; pre-fix, `FAILED: a session that fails inside the window erases the slot it was being held for` | this commit |
| A shutdown inside the window erases it | the second block of the same test | passes; pre-fix, `FAILED: and a shutdown inside the window erases it rather than leaving it in the vault` | this commit |
| The same-account path still retains, and the ordinary occasion is the one erasure | `test_a_retained_slot_stays_when_the_same_account_arrives` | passes | this commit |
| Renewal still survives its own supersession twice | `application_device_authorization_refresh_course` | unchanged: `ROTATIONS:2`, `REFRESH_DELETES:1`, `REFRESH_STORED_TOKEN:e2e8c030e3a107e9` | this commit |
| Every device, vault, and public-path row; boundaries; tidy; format | the 16-row selector set; `check_boundaries`; `tidy.py`; `clang-format --Werror` | 16/16 in 28 s; exit 0; 2 TUs clean; exit 0 | this commit |

**The same-account test does not fail before the fix, and was not made to.**
That path is what BPR-0547 established and this card only narrows, so
nothing on it is wrong at the previous head. The test earns its place as the
guard that the narrowing did not become "erase whenever the window closes",
which would put renewal back to working exactly once. The worker recorded it
green at the previous head rather than contriving a red, and that reading is
accepted.

**Left open.** `erase_retained_refresh()` sits at the top of
`reset_session_scratch()`, which reads as a pure reset and now has one
effect; it is commented, and it is the single point covering all four
terminals plus shutdown, but a future non-terminal caller of that helper
would silently widen the erasure. The `select_persistent_credential` call
site is defensive: no resume that adopts an authority while a window is open
could be constructed, so it is unexercised by these tests.

