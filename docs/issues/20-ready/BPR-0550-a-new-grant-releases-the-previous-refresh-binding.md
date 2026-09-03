---
id: BPR-0550
title: A new device grant releases the previous refresh binding
status: Ready
rfc: RFC-BPR-0016
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
