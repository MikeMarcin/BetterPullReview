---
id: BPR-0547
title: A rotation must not erase the refresh material it just stored
status: Backlog
rfc: RFC-BPR-0016
depends_on: [BPR-0545]
---

# A rotation must not erase the refresh material it just stored

## Why

The third independent review of RFC-BPR-0016 at `0e75c2e0` ([[BPR-0285]])
found that one rotation destroys the renewal chain. After a refresh
succeeds, the device host stores the new refresh material, then enrolls the
rotated access token through the device-grant enrollment boundary. That
enrollment activates the profile, whose transition emits an invalidate
beside the establish; the GitHub host's invalidate runs `start_delete`,
and since [[BPR-0540]] `start_delete` also erases the refresh slot — the
slot the rotation filled a moment earlier, on the same executor.

The session survives (the bootstrap re-stores the access token) but the
probe then answers absent, `bind_device_refresh_authority` returns early on
key equality so nothing re-stores, and the device host still reports
`scheduled`. The next renewal retrieves an absent slot and reports
`reenrollment_required(refresh_absent)`. So renewal works exactly once,
which falsifies the ledger's atomic-rotation claim and the host
implementation document's rotation-order rule.

The refresh course cannot see it: it drives one rotation, and its
`REFRESH_RETAINED:false` pin is satisfied by this erasure rather than by
the shutdown's. The only superseding-enrollment host test uses a manual
credential.

## Required final state

- A device-grant enrollment that supersedes a device-grant session keeps
  the refresh slot; every other occasion [[BPR-0540]] names still erases it.
- A host test proves it (no refresh delete recorded for that supersession),
  failing before the fix.
- The refresh course pins the erasure it means: a refresh-delete count that
  distinguishes the supersession from the shutdown, and a two-rotation
  course that ends `scheduled` with the second rotation's material durable.

## Non-goals

- Persisting a session across a restart ([[BPR-0541]]).
