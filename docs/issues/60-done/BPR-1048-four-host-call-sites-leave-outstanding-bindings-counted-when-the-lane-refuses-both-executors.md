---
schema: "implement-rfc.card.v1"
id: "BPR-1048"
title: "Four host call sites leave OutstandingBindings counted when the device-binding lane refuses both executors"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-main-linux, BPR-0884's Linux half","found_at":"02e3d59d","read_at":"664d6e36","reachable":"only when the effect and decision executors both refuse a post (allocation failure)","related_cards":["BPR-0884","BPR-0541","BPR-0841","BPR-0644"],"reservation":"docs/issues/_reservations/BPR-1048--BPR-1048--c59e95a4d877.json"}
acceptance: [{"criterion":"A host test drives each of the four sites with the persistence lane refusing both executors; after each refusal the host reaches is_quiescent() once closed, a later restore_device_profile is not refused by the leftover count, and the site's own pending flag no longer blocks its next call","outcome":"pass","revision":"a670fd95","evidence":{"kind":"command","detail":"application_github_host on bpr-main-linux: withdrawing_binding_lane drives persist (settle_device_binding), start_delete (replacement and Sign out), prepare_device_rotation and the launch restore's erase; each passes its no-throw, failure-path, next-call, later-restore and quiescence assertions. Passed on the pre-rebase content and again on a670fd95 (build/logs/bpr-main-linux-20260924/bpr-1048/rebased-rows.log).","path":"docs/issues/60-done/BPR-1048-four-host-call-sites-leave-outstanding-bindings-counted-when-the-lane-refuses-both-executors.md#tests-testsapplicationgithub_application_host_testscpp"}},{"criterion":"prepare_device_rotation completes false exactly once on a withdrawal, and never true without a written tombstone","outcome":"pass","revision":"a670fd95","evidence":{"kind":"command","detail":"test_a_withdrawn_rotation_barrier_answers_false_once: answers == {false} with no tombstone after the withdrawal, then {false, true} with the tombstone written after the next barrier.","path":"docs/issues/60-done/BPR-1048-four-host-call-sites-leave-outstanding-bindings-counted-when-the-lane-refuses-both-executors.md#tests-testsapplicationgithub_application_host_testscpp"}},{"criterion":"Every new assertion fails on the unchanged product (negative control)","outcome":"pass","revision":"14f2f888","evidence":{"kind":"command","detail":"Built against the unchanged product with expect made non-fatal: all 21 new assertions printed FAILED and no older assertion did (negctl-unchanged-product.log). A design trial that failed withdrawals at once inside a turn failed the two replacement assertions (visible=1 tombstoned=0 deletes=1 profiles=2).","path":"docs/issues/60-done/BPR-1048-four-host-call-sites-leave-outstanding-bindings-counted-when-the-lane-refuses-both-executors.md#negative-control-and-design-trial"}},{"criterion":"No regression: application_github_host, application_device_binding_restart, application_device_binding_launch_renewal, application_device_binding_relaunch and the device authorization public path still pass","outcome":"pass","revision":"a670fd95","evidence":{"kind":"command","detail":"Serial offscreen with fresh HOME/XDG: 15 of 15 focused rows pass; the 156 application_ rows pass 154, the two failures being the known BPR-0966 and BPR-1036 reds; on a670fd95 the host, device authorization and three device-binding rows pass 8 of 8. architecture_boundaries fails at HEAD without this change (0ba63b0a, BPR-0886), recorded on the card.","path":"docs/issues/60-done/BPR-1048-four-host-call-sites-leave-outstanding-bindings-counted-when-the-lane-refuses-both-executors.md#verification-linux"}}]
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

## The fix, Linux (bpr-main-linux, 2026-09-24)

Station `bpr-main-linux` (WSL2 Ubuntu, GCC 15.2, Qt 6.11.2, `build/dev`
Debug, which compiles with `-Werror`). Claimed at `b5cb3b43`. Every run was
serial and offscreen: `QT_QPA_PLATFORM=offscreen`, `DISPLAY` and
`WAYLAND_DISPLAY` unset, and a fresh `HOME` and fresh XDG data, config,
cache, state and runtime directories, removed afterwards. Logs are station
records under `build/logs/bpr-main-linux-20260924/bpr-1048/` and are not
committed.

**The candidate is `a670fd95` on main**, the fix, its tests and the
architecture note. The runs below were made on its content before the
rebase, then the key rows again on `a670fd95` itself.

### Decision: what a withdrawn request means at each site

A withdrawal is the lane's `unavailable` terminal, delivered by the host,
because the lane cannot deliver it. It is a failure, never a success
([[BPR-0541]]). Each site's own terminal handler now receives that failure,
so it undoes its count and its pending flag and takes the same path as a
refused request:

| Site | What a withdrawal now does |
| --- | --- |
| `persist_settled_device_binding` | `BindingStorePending` is cleared and `binding_failure()` runs: nothing is remembered, and `vault_unavailable` is published. The next settle stores again |
| `start_delete` | The existing `!aResult` branch. With a replacement waiting, the replacement fails and neither vault slot is touched. With nothing waiting, as at Sign out, the credential is still erased. `BindingErasePending` is cleared, so the next tombstone reaches the store |
| `prepare_device_rotation` | `binding_failure()`, then `aCompletion(false)`, exactly once. No refresh material is spent without a written tombstone. Read from the code, not run: the device host's `begin_refresh` then asks for re-enrollment (`storage_failed`) and releases its own `Outstanding` count, which the old throw also left counted |
| The launch restore's erase | The count is undone. `start_deferred_session()` still runs after the call, as before |
| The startup load (`restore_device_profile`) | Unchanged. It already caught the throw and ended the restore `unavailable` |

**The throw does not reach the caller.** The host counted the request, so the
host owns its one terminal, and the site's failure path is that terminal.
Passing the throw on as well would give a rotation's caller two outcomes, a
`false` and an exception. From inside a turn it would also clear the
host's deferred ingress, because `run_turn` drops it on an exception. The
scope named [[BPR-0841]]'s rule, that only an admitted request gets a
terminal, as the reference. The lane did not admit the request, but the
host did when it counted it, so the host's request gets exactly one
terminal, and that terminal is the failure.

**When the failure runs.** Inside a host turn it runs when the turn ends,
after the turn's deferred ingress. That is where the lane's own terminal
would have found it. Outside a turn it runs at once, as the startup load's
catch already does. The timing decides the replacement case: a replacement
records its session after the invalidation that asked for the tombstone.
Run at once, the failure would see no waiting session and take the
Sign-out branch. The old slot would then be erased with no tombstone written,
and the replacement would go ahead, which a refused tombstone never allows.

### What changed

`src/application/src/github_application_host.cpp`:

- `call_binding_lane` calls the lane with a site's terminal handler. If the
  lane throws, the handler goes to `fail_withdrawn_binding`, and the throw
  goes no further.
- `fail_withdrawn_binding` queues the handler when a turn is active, and
  otherwise fails it at once. The queue is a fixed array of eight
  `std::optional<std::function>` slots. Queueing moves a `std::function`
  and allocates nothing, which matters because a withdrawal usually means
  that allocation failed. With every slot taken, the handler is failed at
  once.
- `run_turn` calls `fail_withdrawn_bindings()` after its deferred ingress.
  A failure that makes another request which is withdrawn in turn joins the
  queue and runs before the turn ends. As on the lane, a throwing handler
  cannot stop the ones behind it. If a turn ends in an exception, its queued
  failures run at the end of the next turn. They stay counted until then, so
  the host cannot report itself quiescent early.
- The four sites build their handler before they count the request, then
  call `call_binding_lane`. Each handler's body is unchanged.

`docs/architecture/github-host-implementation.md` states the host's side of
the lane contract, after the lane's own paragraph.

**Unreachable, recorded.** The card asked that the launch restore's erase
still start or fail a waiting session. In every reachable state no session
is waiting there. Every activation emits an invalidation, and an
invalidation during the load becomes `PendingRestoreCleanup`. That sends the
load down the cleanup path, which reaches `start_delete` and not this erase.
An activation also sets `State.ActiveProfile`, which marks the load
superseded. So the erase is reached only with no deferred session. The call
to `start_deferred_session()` stays, and no test can observe it there.

**No pinned code.** Nothing under `src/app/` changed. The round-trip
composition contract's digests cover `src/app` only, so no reseal is needed.

### Tests (`tests/application/github_application_host_tests.cpp`)

`withdrawing_binding_lane` gives the host a persistence lane over two
`refusable_executor`s. `escapes_while_withdrawing` makes both refuse for one
call and reports whether a throw reached the caller. Four tests, 21
assertions, one set per site, and two scenarios for `start_delete`:

| Test | Drives | Asserts, after the withdrawal |
| --- | --- | --- |
| `test_a_withdrawn_binding_store_fails_like_a_refused_one` | `settle_device_binding` | no throw; fails visibly, and nothing is written or remembered; the next settle stores; a later restore reaches the store; quiescent once closed |
| `test_a_withdrawn_binding_tombstone_fails_like_a_refused_one`, replacement | `enroll_profile` over a durable device grant | no throw; the replacement fails, `vault_unavailable` is published, and no vault slot is deleted; Sign out then reaches the store and erases the credential; a later restore reaches the store; quiescent once closed |
| The same, Sign out | `deactivate_github_identity_intent` | no throw; fails visibly, and the signed-out credential is still erased; a later restore reaches the store; quiescent once closed |
| `test_a_withdrawn_rotation_barrier_answers_false_once` | `prepare_device_rotation` | no throw; answers `false` once, with no tombstone written, and fails visibly; the next barrier writes its tombstone and answers `true`; a later restore reaches the store; quiescent once closed |
| `test_a_withdrawn_launch_restore_erase_is_not_left_counted` | a restore of a binding whose refresh lifetime elapsed, withdrawn at the erase in its load's terminal | ends `unavailable` with no tombstone written, and the next restore reaches the store and writes it; quiescent once closed |

"A later restore reaches the store" means that, after Sign out, a
`restore_device_profile` adds one `load` to the store and gets one terminal.
A leftover count answers it `unavailable` at admission, with no load.

### Negative control and design trial

- **Every new assertion fails on the unchanged product.** The test was
  written and built first, on `14f2f888`, whose `src/` is the claim's. The
  row failed at the first new assertion (`red-first.log`). A second build
  made `expect` print its failure and carry on, with nothing else changed.
  All 21 new assertions printed `FAILED`, and no older assertion did
  (`negctl-unchanged-product.log`). The test file was then restored, and its
  SHA-256 matched the copy taken before the control (`8bc8b9ad…`).
  Formatting it later changed whitespace only (`diff -w` is empty).
- **Trial: fail at once, even inside a turn.** With the queue bypassed and
  `expect` non-fatal, the changed product fails exactly two assertions, both
  in the replacement scenario (`trial-sync.log`, `trial-sync-detail.log`):
  `visible=1 tombstoned=0 deletes=1 profiles=2`. The failure was published, but the old slot was erased with
  no tombstone written, and the replacement went on to resolve its profile.
  That is why the failure waits for the end of the turn. The source was then
  restored and its SHA-256 checked.

### Verification (Linux)

| Check | Result |
| --- | --- |
| `cmake --build --preset dev` | Exit 0, no `warning:` or `error:` line |
| `application_github_host` | Passed |
| Focused rows (`focused.log`): `application_github_host`, `application_device_authorization_host`, `_refresh`, `sqlite_device_binding_storage`, `sqlite_private_store_open`, `application_device_authorization_public_path`, `_registered_course`, `_refresh_course`, `_orphan_course`, `application_device_binding_restart`, `_launch_renewal`, `_relaunch`, `application_profile_startup_public_path`, `application_profile_management_public_path`, `core_boundary_contract` | 15 of 15 pass |
| `architecture_boundaries`, `architecture_boundary_contract` | **Both fail, at `HEAD` without this change.** See "Red at HEAD, not this card's" below |
| Integration pass: every `application_` row, 156 (`sweep-application.log`) | 154 pass, in 1,176.6 s. The two failures are the known ones, failing as before: `application_agent_finding_dif_context_contract` ([[BPR-0966]], `dif-context-evidence failed=dif-comment-ready-after-conversion dif-acknowledge-after-acceptance`) and `application_uiforetw_selected_diff_contract` ([[BPR-1036]], `UIFORETW_CAPTURE_LAYOUT_FAILED:987:842 width_floor=750 height_floor=850`) |
| The rebased candidate `a670fd95`, rebuilt (`rebased-rows.log`) | `application_github_host`, the device authorization host, refresh unit and public path, `sqlite_device_binding_storage` and the three device-binding courses: 8 of 8 pass. The commits the rebase put under the fix change only `src/adapters/qt`'s drawer and its test ([[BPR-1074]]) |
| `/usr/bin/clang-format` 22.1.8 | Clean on both C++ files |
| `_coding-standard/scripts/tidy.py` on the two translation units (clang-tidy 21.1.8) | No finding on a changed line. The two it reports are older, `names_only_the_renewal` and `complete_deletes` in the renewal test, the pair [[BPR-0884]] recorded |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` at `a670fd95` |

### Red at HEAD, not this card's

`architecture_boundaries` stops at
`Agent process backend duplicates shared protocol logic: maximum_agent_stderr_bytes`,
and `architecture_boundary_contract` fails 658 cases, each because the gate
prints that line before the violation the case plants. The name is used in
`src/adapters/agent/src/windows_agent_process.cpp:639`, added by `0ba63b0a`
([[BPR-0886]]). The gate forbids it in an agent process backend
(`cmake/VerifyArchitectureBoundaries.cmake`, the duplicated-protocol list).
In a detached worktree the gate fails at `14f2f888`, without this change, and
passes at `0ba63b0a^`. This card changes no agent code. It is reported to
the coordinator, for a card of its own.

### Acceptance state

- **A host test drives each of the four sites with the lane refusing both
  executors, and after each: quiescent once closed, a later restore is not
  refused by the leftover count, and the site's pending flag does not block
  its next call:** pass. The launch erase and the rotation have no pending
  flag of their own. For them, the next call is the next restore and the next
  barrier.
- **`prepare_device_rotation` completes `false` exactly once on a
  withdrawal, and never `true` without a written tombstone:** pass.
- **Every new assertion fails on the unchanged product:** pass, 21 of 21.
- **No regression in `application_github_host`,
  `application_device_binding_restart`, `_launch_renewal`, `_relaunch` and
  the device authorization public path:** pass.

The code is platform-neutral, and so are the test's executors. As the card
says, one platform's run meets the acceptance. macOS and Windows can confirm
it in their next ordinary sweep, and nothing else is owed.

**No independent review.** The change moves when four failure paths run,
inside the host's decision turn. This lane runs as a leaf and cannot start a
reviewer. The design trial above is the direct evidence for the timing.
