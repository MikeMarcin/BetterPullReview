---
id: BPR-0546
title: The device host mints a vault context the platform vaults accept
status: Ready
rfc: RFC-BPR-0016
depends_on: []
---

# The device host mints a vault context the platform vaults accept

## Why

The third independent review of RFC-BPR-0016 at `0e75c2e0` ([[BPR-0285]])
found that the composed device host mints every vault context with
`IdentityGeneration` zero. `initial_device_authorization_state()` sets only
`ApplicationSession` and `NextRequest`; the core transition only reads and
copies the generation; the device host copies it into the context it hands
the vault. The port's own `valid()` requires a nonzero generation, and each
platform vault checks `valid()` before queuing, so `store_refresh` and
`retrieve_refresh` from the device host are refused `invalid_request` on
macOS, Linux, and Windows. `on_refresh_stored` maps that refusal to
`fail_storage`, so a real device sign-in ends in
`reenrollment_required(storage_failed)` moments after it succeeded, and
nothing is ever written to the second slot.

The composed refresh lifecycle is therefore inert in the shipped product on
every platform, which falsifies the RFC's refresh acceptance row and the
platform table. It has been true since [[BPR-0536]] composed the lifecycle;
[[BPR-0542]] and [[BPR-0543]] made the slot real, which is what makes this
reachable rather than moot, and their closing claims about material landing
in the slot are vacuous until this is fixed.

Every row misses it because the fixture vault validates its base slot's
enrollment but not its refresh-slot contexts, and the refresh host tests
build contexts with a nonzero generation of their own.

## Required final state

- The device host's vault contexts carry a nonzero identity generation —
  the accepted session's, passed with `bind_refresh_authority`, or a
  nonzero initial device generation — chosen with a recorded reason.
- The fixture vault validates refresh-slot contexts exactly as the platform
  vaults do. Applying that alone must turn the refresh and orphan courses
  red at the head before this fix, and that run is the negative control.
- A composed course, or a host test against a double with the platform
  rules, proves refresh material reaches the slot under a context a real
  vault accepts.

## Non-goals

- Changing the vault port's validity contract.
