---
schema: "implement-rfc.card.v1"
id: "BPR-0547"
title: "A rotation must not erase the refresh material it just stored"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: ["BPR-0545"]
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

## Landed — 2026-09-03

Fix at `0da8beba`, the two-rotation course at `09f94e14`.

**The rule.** `start_delete` carries a `refresh_disposition` (`erase` by
default) threaded through the delete callback as a parameter rather than host
state, so a stale occasion cannot decide another's erasure. One call site
passes `retain`, in `invalidate`, and only when the incoming enrollment is a
device grant, the session being superseded was one too, and the incoming
enrollment names the profile the outgoing authority is keyed under. The
profile condition goes beyond the card's wording deliberately: a device grant
for a *different* profile leaves nobody able to spend the old slot, so it must
still erase. The deferred-invalidation path carries the decision forward so
deferral cannot re-decide it. Every other occasion still erases.

**The evidence.** The fixture counts accepted refresh deletes and the course
pins `DEVICE_FLOW_REFRESH_DELETES`, which is what tells the rotation's own
erasure apart from the shutdown's — the old pin set could not. The course
clock now steps once per armed renewal (capped at two) instead of latching
one offset, and each renewal is made due one second later rather than
instantly, because a zero wait let the second renewal outrun the
supersession's vault operations; the shipped product's next renewal is eight
hours after the supersession settles, so one second is a thousandfold margin.
The row's bounds rose to `BUDGET_MS 12000` / `TIMEOUT 25` with the measured
reason in the CMake comment.

| Claim | Command | Result | Commit |
|---|---|---|---|
| A device grant superseding a device-grant session keeps the slot | `application_github_host` — new case `test_a_rotation_keeps_the_material_it_just_stored` | passes; at the pre-fix host source it fails by name | `0da8beba` |
| A pasted credential superseding the same session still erases | the adjacent pre-existing case | unchanged, one refresh delete | `0da8beba` |
| Renewal survives its own supersession, twice | `application_device_authorization_refresh_course` | `ROTATIONS:2`, `REFRESH_STATE:scheduled`, `VAULT_STORES:3`, `REFRESH_STORES:3`, `REFRESH_DELETES:1`, `REFRESH_STORED_TOKEN:e2e8c030e3a107e9` (the second rotation's material) | `09f94e14` |
| The course fails on the second rotation without the rule | the same row with the disposition forced to `erase` | `ROTATIONS:1`, `reenrollment_required`, `REFRESH_DELETES:2`, last material renewal 1's | `09f94e14` |
| Every device, vault, and public-path row; boundaries; tidy; format | the 16-row selector set; `check_boundaries`; `tidy.py`; `clang-format --Werror` | 16/16; exit 0; exit 0; exit 0 | both commits |

**Left open.** A device grant superseding a device-grant session on the same
profile but a *different account* now retains a slot nobody can spend; the
account is unknown at `invalidate`. Carded as [[BPR-0549]] and added to the
closeout closure. The step size derives from two constants it does not own
(the fixture's lifetime and the host's lead); if either moves the row waits
out its budget and says so.
