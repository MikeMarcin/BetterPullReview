---
id: BPR-0545
title: The refresh erasure must not displace the credential delete
status: Done
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

## Landed — 2026-09-03

Host half at `00628518`, the two platform halves merged from the stations at
`ada1c964` (Windows) and `5d8f9549` (Linux).

**The host.** `start_refresh_delete` left `start_delete`; the erasure is now
issued from `handle_delete_now`, the credential delete's own terminal, so it
reaches the vault only after the vault has released that context. It runs in
every branch, the failing ones included: a credential delete that failed says
nothing about whether renewal material is still beside it. Sequencing was
chosen over minting a second context because request identity is the core's
to allocate and this host has no allocator; the cost, stated in the code, is
that a crash between the two leaves renewable material behind an erased
credential, which nothing can spend since the resume answers only for
device-grant provenance. Residual R1 from the review is fixed in the same
place: `OutstandingRefreshDeletes` is released inside the lane lambda after
the terminal is applied, so quiescence cannot be observed between the release
and the failure report.

**The platforms.** Both stations added the same pair against their real
vaults, and both passed as written with no product change — reported plainly
rather than staged into a red. Windows additionally asserts that the base
credential was never deleted (`backend->Calls.empty()`), which pins the
regression itself rather than the error code, and repeats the pair in reverse
order so the refusal is not an artefact of which slot went first; it also
found that the existing duplicate coverage was all base-slot, so the
two-slot pair was a real and narrow gap.

**What the platform cases are worth, corrected.** The dispatch called the
characterization case "the one that would have caught B1". The Linux station
corrected that and is right: B1 lived in `start_delete`, above the vault
boundary, so a case driving the vault directly could not have failed under
the old code. What it guards is the tempting wrong fix — relaxing the vault
until a duplicate context stops being refused, trading a visible refusal for
two operations racing on one key. The host test below is what fails before
the fix.

| Claim | Command | Result | Commit |
|---|---|---|---|
| An occasion never has two operations pending under one context | `application_github_host` — new case with a hold-the-erasure double mode | passes; reverting the erasure into `start_delete` turns the binary red | `00628518` |
| The composed course completes its cleanup | `application_device_authorization_refresh_course` | `cleanup_failed=0` where the control had `1` | `00628518` |
| A duplicate context is refused, one operation is queued, and the accepted one still succeeds | `linux_secret_service_contract`, `windows_credential_manager_contract` | passed first run, no product change | the two merges |
| Sequenced deletes both complete on their own slots against the real vault | `linux_secret_service_native`, `windows_credential_manager_native` | passed first run; login collection `Locked` read `(<false>,)` before the Linux rows | the two merges |

**Left open, recorded rather than claimed.** The native cases exercise the
port and sequence the two deletes themselves; they do not prove the composed
host sequences them that way. Nothing on any platform composes the real host
against the real vault — the third review's residual R3, and the reason its
three blocking findings went unseen. The host test proves the sequencing
against a double that now enforces the platform rules, which is the closest
evidence that exists today.
