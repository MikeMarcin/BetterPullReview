---
id: BPR-0548
title: The fixture vault obeys the platform vaults' rules
status: Ready
rfc: RFC-BPR-0016
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
