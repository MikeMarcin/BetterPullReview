---
id: BPR-0549
title: A retained refresh slot must not outlive the account it renews
status: Backlog
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
