---
id: BPR-0545
title: The refresh erasure must not displace the credential delete
status: Ready
rfc: RFC-BPR-0016
depends_on: []
---

# The refresh erasure must not displace the credential delete

## Why

The third independent review of RFC-BPR-0016 at `0e75c2e0` ([[BPR-0285]])
found that [[BPR-0540]]'s erasure regressed the credential delete on every
shipped platform. `start_delete` builds one
`github_credential_operation_context` and submits it twice — first to
`delete_refresh` through `start_refresh_delete`, then to
`delete_credential`. All three platform vaults key their pending-operation
map by exactly that context (`try_emplace` in `macos_keychain.cpp`,
`linux_secret_service.cpp`, `windows_credential_manager.cpp`) and answer a
duplicate with `invalid_request` before queuing; the macOS contract pins
that refusal deliberately. So the second submission — the base credential's
own delete — is refused unless the vault's worker thread happens to dequeue
the first between the two calls, which the main thread ordinarily wins.

The consequences are exactly the ones the vault boundary exists to prevent.
At shutdown the refusal sets `ShutdownCleanupFailed` and the access token
stays in the platform vault. Outside shutdown it becomes
`fail_delete_operation`: the deferred session fails with `vault`, the
binding is not forgotten, and every later re-enrollment in the process
repeats the refusal — so pasting a token over an existing session fails on
a real vault, which falsifies the RFC's manual-enrollment acceptance row.

No registered row can see it. The fixture vault posts each operation as its
own closure with no keyed queue, and the host tests' double appends to
vectors, so both accept two operations under one context where every real
vault refuses.

## Required final state

- Two vault operations are never pending under one context: either the
  refresh erasure is issued from the credential delete's terminal (keeping
  `OutstandingRefreshDeletes` in quiescence), or it carries a context the
  vault can tell apart. The choice is recorded with its reason.
- The fixture vault and the host tests' double refuse a duplicate context
  exactly as the platform vaults do, so a future slice cannot reintroduce
  this against green rows.
- A host test covers "a refresh delete and a credential delete under one
  occasion both complete on their own slots", failing before the fix.
- Each platform suite gains the same case against its real vault.

## Non-goals

- Changing when the refresh slot is erased ([[BPR-0540]]'s occasions stand,
  as narrowed by [[BPR-0547]]).
