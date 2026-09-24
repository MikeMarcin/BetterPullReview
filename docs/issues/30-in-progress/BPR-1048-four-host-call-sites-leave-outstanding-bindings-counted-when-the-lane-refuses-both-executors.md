---
schema: "implement-rfc.card.v1"
id: "BPR-1048"
title: "Four host call sites leave OutstandingBindings counted when the device-binding lane refuses both executors"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-main-linux, BPR-0884's Linux half","found_at":"02e3d59d","read_at":"664d6e36","reachable":"only when the effect and decision executors both refuse a post (allocation failure)","related_cards":["BPR-0884","BPR-0541","BPR-0841","BPR-0644"],"reservation":"docs/issues/_reservations/BPR-1048--BPR-1048--c59e95a4d877.json"}
---

# BPR-1048: Four host call sites leave OutstandingBindings counted when the device-binding lane refuses both executors

## Defect

[[BPR-0884]] fixed the persistence lane at `02e3d59d`. When the lane can post
neither a request's effect nor its `unavailable` terminal, it now withdraws
the request and the call throws. Nothing stays queued and no terminal
follows. The header contract on `device_binding_persistence`
(`src/application/include/bpr/application/device_binding_store.hpp:63-67`)
says so.

The host calls that lane from five places in
`src/application/src/github_application_host.cpp`. Each one raises
`OutstandingBindings` before the call, and lowers it only in the completion.
Only one of the five catches the throw: the startup load in
`restore_device_profile` (`:4310-4313`, caught at `:4430-4435`). It lowers the
count, clears `BindingLoadPending`, completes the restore as `unavailable` and
publishes the binding failure.

The other four do not catch. After a withdrawal, their count stays up and
their pending flag stays set for the life of the host. Line numbers are at
`664d6e36`:

| Call site | Raised before the call | What follows a withdrawal |
| --- | --- | --- |
| `persist_settled_device_binding` (`:4007-4011`) | `BindingStorePending = true`, `++OutstandingBindings` | The flag makes every later persist return at its guard (`:3999`), so the grant is never remembered. The throw escapes to `settle_device_binding` (`:4460`) or to the session acceptance turn (`:2736`). |
| `start_delete` (`:4044-4047`) | `BindingErasePending = true`, `++OutstandingBindings`; `BindingGeneration` bumped and the delete terminal stored in `BindingDeleteTerminal` | Every later `start_delete` stores its terminal and returns at `if (BindingErasePending)` (`:4041`). No delete reaches `start_delete_unblocked`, so Sign out, replacement and invalidation never deliver their terminal. |
| `prepare_device_rotation` (`:4476-4478`) | `++OutstandingBindings`; `DeviceBindingDurable = false`, `BindingGeneration` bumped | `aCompletion` was moved into the destroyed callback and is never called. The throw escapes to `github_host_device_authorization_enrollment::prepare_refresh` (`src/application/src/device_authorization_application_host.cpp:170`). |
| The launch-restore erase inside the load completion (`:4412-4414`) | `++OutstandingBindings` | `start_deferred_session()` (`:4415`) is skipped. The `terminal_guard` still completes the restore as `unavailable`. The lane's `complete` (`src/application/src/device_binding_store.cpp:73-87`) swallows the throw. |

At every site the leftover count has the same two effects:

- Every later `restore_device_profile` answers `unavailable` at its admission
  check, `OutstandingBindings != 0U` (`:4303`).
- `github_application_host::is_quiescent()` requires `OutstandingBindings == 0U`
  (`:4621`), and for `start_delete` also `!BindingErasePending`. So it never
  returns true after `Closing`, and composition keeps the host alive for the
  rest of the process.

These consequences are read from the code. No test reproduces them yet.

## Why P3

The gap predates BPR-0884. Before `02e3d59d` the call also threw, and the
lane stranded the job as well, so the host's count stayed up the same way.
It is reachable only when
the effect executor **and** the decision executor both refuse a post. In
practice that means allocation failure. BPR-0884 recorded it under "Recorded,
not changed" and asked for this card.

## Scope

- Decide once what a withdrawn request means at each of the four sites, and
  state it next to the lane contract. The fail-closed rule from [[BPR-0541]]
  holds: a withdrawal is a failure, never a success.
  - Suggested direction: catch at the call. Undo the count and the site's
    pending flag. Then take the same path as a failed terminal from the lane.
    For `prepare_device_rotation` that means `aCompletion(false)` exactly
    once, so no refresh material is spent without a written tombstone. For
    `start_delete` it means the existing `!aResult` branch.
  - The launch-restore erase must still start or fail the deferred session.
    It must not leave that session waiting.
- Keep the one site that already catches as it is, unless the decision
  changes it too.
- Say whether a throw should still escape to the caller after the host has
  handled it. [[BPR-0841]]'s rule that only an admitted request gets a
  terminal is the reference.

## Acceptance

- A host test drives each of the four sites with the persistence lane
  refusing both executors. It reuses the `refusable_executor` pair from
  `test_device_binding_persistence_stays_drainable_when_its_posts_are_refused`
  (`tests/application/github_application_host_tests.cpp:5491`). After each
  refusal:
  - the host reaches `is_quiescent()` once closed;
  - a later `restore_device_profile` is not refused by the leftover count;
  - the site's own pending flag no longer blocks its next call.
- `prepare_device_rotation` completes `false` exactly once on a withdrawal,
  and never `true` without a written tombstone.
- Every new assertion fails on the unchanged product (negative control).
- No regression: `application_github_host`,
  `application_device_binding_restart`,
  `application_device_binding_launch_renewal`,
  `application_device_binding_relaunch` and the device authorization public
  path still pass.

The code is platform-neutral, and so are the test executors. One platform's
run meets the acceptance. The other platforms can confirm it in their next
ordinary sweep.

## Evidence

- Source of the finding: [[BPR-0884]], section "Decision: what a failure does
  to the persistence lane", under "Recorded, not changed".
- Line references were re-read at `664d6e36`.
