---
schema: "implement-rfc.card.v1"
id: "BPR-0548"
title: "The fixture vault obeys the platform vaults' rules"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: []
---

# The fixture vault obeys the platform vaults' rules

## Why

The three blocking findings of RFC-BPR-0016's third review ([[BPR-0285]],
[[BPR-0545]], [[BPR-0546]], [[BPR-0547]]) were all invisible to every
registered row for one reason: the fixture vault every composed course runs
against does not share the platform vaults' rules. It does not key pending
operations by context, so it accepts two operations under one context where
every real vault refuses; it does not validate refresh-slot contexts, so it
accepts a context whose identity generation is zero. The host tests' double
has the same two gaps.

A fixture that is more permissive than the thing it stands in for cannot
falsify anything about that thing. This card closes the class rather than
the three instances.

## Required final state

- The fixture vault and the host tests' vault double enforce what every
  platform vault enforces: `valid()` on every context and enrollment for
  both slots, one pending operation per context with a typed
  `invalid_request` for a duplicate, and the same completion discipline.
- A short list in the fixture's own comment states which platform rules it
  mirrors and why each one is there, so the next divergence is deliberate.
- Applying this before the three fixes turns the affected courses red;
  those runs are recorded as the negative controls for
  [[BPR-0545]] through [[BPR-0547]].
- No composed course loses coverage: a course that now refuses is repaired
  by fixing the composition, never by relaxing the fixture.

## Non-goals

- Making the fixture a second implementation of a platform vault; it stands
  in for one, and only its refusals must match.

## The two rules, and the negative controls they produced — 2026-09-03

Both doubles now enforce, on all six port methods and for both slots, what
`macos_keychain.cpp`, `linux_secret_service.cpp`, and
`windows_credential_manager.cpp` enforce: `valid()` on the context or
enrollment plus a callable completion, refused `invalid_request` before
anything is queued; and one pending operation per
`github_credential_operation_context`, the duplicate refused
`invalid_request` before it is queued. The fixture claims the context
synchronously and releases it when the queued task begins, which is the
platform vaults' `queue`/`execute` order; it still publishes every terminal
on the effect lane, which is its own discipline. The host tests' double
holds a recorded call pending until the test runs its completion.

Selector run (16 rows, `-j 2`): 15 passed, one red.

- **`application_device_authorization_refresh_course`** — green before this
  card, red after. Two independent signals in the one row:
  `DEVICE_FLOW_ROTATIONS:0`, `DEVICE_FLOW_REFRESH_STATE:reenrollment_required`,
  `DEVICE_FLOW_VAULT_STORES:1`, `DEVICE_FLOW_REFRESH_STORES:0`,
  `DEVICE_FLOW_REFRESH_FAILED` against a required
  `ROTATIONS:1 / scheduled / VAULT_STORES:2 / REFRESH_STORES:2 / REFRESH_OK`;
  and `PUBLIC_PATH_VERDICT: ... cleanup_failed=1`. Disabling only the
  duplicate rule and rerunning left the `DEVICE_FLOW_*` markers unchanged
  and turned `cleanup_failed` back to `0`, so the refresh-slot half is the
  validity rule refusing the device host's identity-generation-zero
  contexts ([[BPR-0546]]) and the shutdown half is the pending-context rule
  refusing the second delete under one context ([[BPR-0545]]).
  [[BPR-0547]] is not demonstrated here: the course never reaches a first
  renewal, so a second one cannot fail.

`check_boundaries` and `clang-format` are clean on the touched files.

## Landed — 2026-09-03

At `3beeec7a`. The fixture vault and the host tests' `fake_vault` now enforce
what all three platform vaults enforce, on all six port methods and both
slots: `application::valid()` on every context and enrollment, refused
`invalid_request` before anything is queued; and one pending operation per
operation context, a duplicate refused the same way. The fixture claims the
context synchronously at submit and releases it at the top of the queued
task, which is the platform's `queue`/`execute` order; the double wraps its
recorded handlers so the context is released when the test runs the
completion. A comment above the fixture names each mirrored rule, why it
exists, and what is deliberately not mirrored (storage stays in-memory and
authority-keyed).

That alone turned `application_device_authorization_refresh_course` red at a
head where it had been green, and disabling one rule at a time separated the
two signals: the refresh markers (`REFRESH_STORES:0`,
`REFRESH_STATE:reenrollment_required`, `ROTATIONS:0`, `VAULT_STORES:1`) came
from rule 1 and became [[BPR-0546]]'s control; `cleanup_failed=1` came from
rule 2 and became [[BPR-0545]]'s. [[BPR-0547]] stayed masked behind
BPR-0546, as recorded there.

| Claim | Command | Result | Commit |
|---|---|---|---|
| Both rules hold in both doubles and the tree builds clean | `cmake --build --preset dev` | exit 0, zero `error:`/`warning:` lines | this commit |
| The rules turn exactly one row red, and the red is the two defects | the 16-row device/vault/public-path selector set | 15 passed, `refresh_course` red with the markers above | this commit |
| The architecture boundaries hold, format is clean | `check_boundaries`; `clang-format --dry-run --Werror` | exit 0; exit 0 | this commit |

**Left open.** The host-tests double cannot exhibit BPR-0545's shape: its
`delete_refresh` answers inline, so the pending window closes before the
credential delete is submitted. The composed fixture course carries that
control instead, and [[BPR-0545]]'s own host test uses a deliberate
hold-the-erasure mode added for it.
