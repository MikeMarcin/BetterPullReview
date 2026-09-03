---
id: BPR-0542
title: The Linux Secret Service vault stores refresh material in a real second slot
status: Ready
rfc: RFC-BPR-0016
depends_on: []
---

# The Linux Secret Service vault stores refresh material in a real second slot

## Why

The RFC-BPR-0016 closeout review of the platform vaults on 2026-09-03
([[BPR-0285]], the integrator's own reading before the third independent
review) found that `src/adapters/github/src/linux_secret_service.cpp` implements the refresh slot as three
stubs: `store_refresh`, `retrieve_refresh`, and `delete_refresh` each answer
`github_credential_vault_failure::unavailable` without touching the
platform store. The recorded distributable registration issues
expiring user tokens with refresh tokens
(`registration-observation.json`: `user_access_token_expiration_enabled:
true`, `refresh_policy: expiring_user_token_with_refresh`), so a real
sign-in on Linux runs this path: the grant parks its refresh token in
memory, the `/user` observation binds the authority and flushes it
(`bind_refresh_authority`), the store answers `unavailable`,
`on_refresh_stored` calls `fail_storage`, and the host reports
`reenrollment_required` with `storage_failed` on the sign-in panel a
moment after the sign-in succeeded. The access token then expires after
its lifetime with nothing to renew it. No course sees this: every enabled
platform course stops at `cancelled` before a grant, and every composed
course runs over the fixture vault, which has a real refresh slot. The
platform table's "Enabled since [[BPR-0310]]" row and the ledger's
refresh-lifecycle claim therefore overstate Linux, and RFC-BPR-0016
cannot close until the slot is real here.

## Required final state

- `src/adapters/github/src/linux_secret_service.cpp` stores, retrieves, and deletes refresh material in a
  second slot of the same platform store, keyed by the same authority as
  the base credential and under a distinct target so one slot can never
  answer for the other — the shape `macos_keychain.cpp` gives its
  `RefreshBackend` and its `github-native-refresh` service. `unavailable`
  is answered only when the platform store itself is unreachable, never as
  a stand-in for an unimplemented slot.
- `linux_secret_service_contract` gains the refresh-slot cases the macOS contract has
  (`test_the_refresh_slot_reaches_only_its_own_service` and the store,
  retrieve, absent, and delete answers), and `linux_secret_service_native` exercises the
  slot against the real Secret Service `login` collection on the station: a stored refresh entry
  is retrievable under its authority, is absent from the base slot, and is
  gone after `delete_refresh`. Each new case fails against the stub before
  the change; record that run as the negative control.
- Erasure honesty: with [[BPR-0540]] deleting the refresh slot beside the
  base credential, an orderly shutdown on Linux leaves no refresh
  material in the store; the native test proves the delete answers success
  for an absent entry rather than `unavailable`.
- The card records the host, toolchain, the exact selectors run, and
  results; format and strict tidy pass on the touched files.

## Non-goals

- Any change to the device host, the composition, the macOS vault, or the
  refresh lifecycle's timing.
- Persisting a session across a restart ([[BPR-0541]]).

## Lane — 2026-09-03

Carded by the integrator straight into `23-ready-lin/` with no open dependency:
the slice is Linux-native (the platform store and its native test)
and is dispatched to `bpr-linux` on `linux-lane`. The integrator merges the
lane, then the RFC-BPR-0016 final-head replays run at the head that
carries it.
