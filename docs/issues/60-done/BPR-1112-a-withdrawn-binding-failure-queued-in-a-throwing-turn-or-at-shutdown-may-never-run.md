---
schema: "implement-rfc.card.v1"
id: "BPR-1112"
title: "A withdrawn binding failure queued in a throwing turn or at shutdown may never run, and start_delete/prepare_device_rotation lose their completion on allocation failure"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"independent rfc-reviewer-critical review of BPR-1048 (claude-fable-5-1, read-only, macOS from source), findings 1, 2 and 4","reviewed_candidate":"a670fd95","origin":"owner-authorized review, attended session cbd48b91000b00f1e72e1ad62c7c384c, decision e2321e81-3011-4178-bd9b-aef06139a3be, 2026-09-24","reachable":"a throwing terminal or projection sink inside a turn or at shutdown; an allocation failure inside start_delete or prepare_device_rotation","related_cards":["BPR-1048","BPR-0884","BPR-0541","BPR-0841"],"reservation":"docs/issues/_reservations/BPR-1111--BPR-1115--c042082d78b1.json"}
acceptance: [{"criterion":"AC1 With a projection sink that throws during shutdown, the queued withdrawn failures run and the host reaches is_quiescent(). A test shows it. If the design needs a second shutdown(), the test makes that call and the card records who makes it in composition.","outcome":"pass","revision":"b7caeb2f","evidence":{"kind":"command","detail":"run_turn's catch fails the queued withdrawn requests before rethrowing, so no second shutdown() is needed. Every projection publication in the host is contained (publish_transition, publish_diagnostic, publish_public_preview, republish_accepted_session), so a sink cannot end a turn in an exception; the realistic source is allocation failure. application_github_host test_a_shutdown_that_fails_after_a_withdrawal_still_runs_its_failure fails each of the 7 allocations after the lane withdrew the shutdown's tombstone in turn (a replaced operator new): every one reaches is_quiescent() after draining alone. None ends the turn in an exception on macOS, so the stranding is latent on the unchanged product. Design trial (a throw placed after the cleanup's start_delete): with the new catch the withdrawn failure had run (vault delete, shutdown_cleanup_failed) and the host was quiescent with no second call; with the old catch it never ran and the host was not quiescent.","path":"docs/issues/60-done/BPR-1112-a-withdrawn-binding-failure-queued-in-a-throwing-turn-or-at-shutdown-may-never-run.md#what-the-injection-found-finding-1-is-latent"}},{"criterion":"AC2 With the persistence lane refusing both executors while the host shuts down, the withdrawn request's failure runs, its count is released, and the host reaches is_quiescent(). A test shows it.","outcome":"pass","revision":"b7caeb2f","evidence":{"kind":"command","detail":"application_github_host test_a_withdrawal_at_shutdown_runs_its_failure: an accepted device grant never stored, removed by the shutdown policy; its tombstone is withdrawn during shutdown(). The failure runs in the shutdown turn (vault_unavailable published, shutdown_cleanup_failed, the credential still erased) and the host is quiescent after draining alone. Trial: with the end-of-turn drain removed the test fails at 'the withdrawn tombstone's failure ran in the shutdown turn'."}},{"criterion":"AC3 With the allocation in start_delete and in prepare_device_rotation made to throw, neither site leaves a counted request, a pending flag or changed durability state without a terminal. The rotation answers false exactly once, or its caller gets the exception and no state has changed. The card states which. A test shows it for each site.","outcome":"pass","revision":"b7caeb2f","evidence":{"kind":"command","detail":"start_delete builds its handler before any state change: an allocation failure there reaches the caller with nothing changed, counted or pending. application_github_host test_a_failed_allocation_in_start_delete_changes_nothing fails each of a durable grant's 9 shutdown allocations in turn under delete_retained_on_shutdown: none leaves durability changed without a request or a terminal, and nothing stays counted. On the unchanged start_delete it fails: 'allocation 2 left durability changed with no request and no terminal' (the handler's std::function). The rotation site no longer allocates (BPR-1116, b71456a1): its caller gets any exception of aCompletion and no state has changed; test_the_rotation_barrier_writes_nothing shows one answer, no store operation, no count and no diagnostic while the lane withdraws everything.","path":"docs/issues/60-done/BPR-1112-a-withdrawn-binding-failure-queued-in-a-throwing-turn-or-at-shutdown-may-never-run.md#decisions"}},{"criterion":"AC4 Every new assertion fails on the unchanged product (negative control). Revised under the owner's authority to revise stale cards: AC1 and AC2 pin behaviour the unchanged product already has (AC2's end-of-turn drain from BPR-1048; AC1's gap is latent), so their controls are trials.","outcome":"pass","revision":"b7caeb2f","evidence":{"kind":"command","detail":"AC3's assertion fails on the unchanged start_delete (allocation 2). AC1: the design trial fails the property with the old catch and passes it with the new one. AC2: removing the end-of-turn drain fails the AC2 test. The rotation assertions fail with the old tombstone barrier restored (recorded on BPR-1116).","path":"docs/issues/60-done/BPR-1112-a-withdrawn-binding-failure-queued-in-a-throwing-turn-or-at-shutdown-may-never-run.md#negative-controls-ac4"}},{"criterion":"AC5 No regression: application_github_host, application_device_binding_restart, _launch_renewal and _relaunch, and the device authorization host and public path.","outcome":"pass","revision":"b7caeb2f","evidence":{"kind":"command","detail":"8 of 8 on macOS: application_github_host, application_device_authorization_host, _refresh, _public_path, sqlite_device_binding_storage, application_device_binding_restart 29.4 s, _launch_renewal 67.2 s, _relaunch 10.1 s. Windows and Linux confirm in their next ordinary pass (command on the card)."}}]
---

# BPR-1112: A withdrawn binding failure queued in a throwing turn or at shutdown may never run, and start_delete/prepare_device_rotation lose their completion on allocation failure

## Source

[[BPR-1048]]'s independent review, 2026-09-24. The owner asked for it in
attended session `cbd48b91` (decision `e2321e81`). The reviewer was an
`rfc-reviewer-critical` (`claude-fable-5-1`), working read-only on macOS
from source. The candidate was `a670fd95`. The review found no blocking
issue, and it found the four acceptance criteria supported. This card takes
findings 1, 2 and 4. The review is recorded on BPR-1048 under "Independent
review (2026-09-24)".

Line references are the reviewer's and are in
`src/application/src/github_application_host.cpp` unless another file is
named. That file is unchanged from `a670fd95` to the revision where this card
was written.

## Defect

**1. A failure queued in a turn that throws may never run.** BPR-1048 queues
a withdrawn request's failure while a turn is active, and runs the queue when
the turn ends. If the turn ends in an exception, `run_turn`'s catch
(`:518-522`) does not drain `WithdrawnBindings`. The failures wait for a later
turn, and they stay counted until it comes. After `Closing`, almost no
ingress supplies one:

- `dispatch` returns early when `Closing` (`:557-560`);
- so does `enroll_profile` (`:656`);
- so does `invalidate_device_grant` (`:4499-4503`).

So `is_quiescent()` may never hold unless `shutdown()` is called again. The
reviewer suggests a test: a projection sink that throws at shutdown, then
`shutdown()` again, then assert `is_quiescent()`.

**2. A withdrawal at shutdown is not exercised.** The four BPR-1048 tests
reach quiescence through `quiescent_once_closed`
(`tests/application/github_application_host_tests.cpp:5613-5618`). None of
them withdraws a request while the host is shutting down. The reviewer gave a
suggested test. The summary this card was written from does not carry that
test's text, so the acceptance below states the property instead.

**3. An allocation before the lane call still escapes (review finding 4).**
BPR-1048 made each site build its terminal handler before it counts the
request. Two sites still change state before an allocation that can throw:

- `start_delete` sets `DeviceBindingDurable`, `BindingGeneration` and the
  delete terminal at `:4086-4090`, and only then builds `finish` as a
  `std::function` at `:4093`.
- `prepare_device_rotation` does the same at `:4536-4538`.

If that allocation throws, the state has changed but no handler exists to
undo it. For the rotation, `aCompletion` is lost. This predates BPR-1048 and
was outside its scope. The reviewer marks it as a follow-up.

## Scope

- Decide how a turn that ends in an exception delivers its queued withdrawn
  failures. It could drain them in the catch, or at shutdown, or it could
  rely on a second `shutdown()`. State the choice next to BPR-1048's paragraph
  in `docs/architecture/github-host-implementation.md`. If a second
  `shutdown()` is the answer, say who calls it.
- Make a withdrawal during shutdown reach quiescence, and pin it.
- In `start_delete` and `prepare_device_rotation`, allocate before the first
  state change, or restore the state when the allocation throws. The site
  then either keeps its previous state or delivers its one failure terminal.
  For the rotation that terminal is `aCompletion(false)`, exactly once.
  [[BPR-0541]]'s rule holds: a withdrawal is a failure, never a success.

## Acceptance

- AC1 With a projection sink that throws during shutdown, the queued withdrawn
  failures run and the host reaches `is_quiescent()`. A test shows it. If the
  design needs a second `shutdown()`, the test makes that call and the card
  records who makes it in composition.
- AC2 With the persistence lane refusing both executors while the host shuts
  down, the withdrawn request's failure runs, its count is released, and the
  host reaches `is_quiescent()`. A test shows it.
- AC3 With the allocation in `start_delete` and in `prepare_device_rotation`
  made to throw, neither site leaves a counted request, a pending flag or
  changed durability state without a terminal. The rotation answers `false`
  exactly once, or its caller gets the exception and no state has changed.
  The card states which. A test shows it for each site.
- AC4 Every new assertion fails on the unchanged product (negative control).
- AC5 No regression: `application_github_host`,
  `application_device_binding_restart`, `_launch_renewal` and `_relaunch`,
  and the device authorization host and public path.

The code is platform-neutral, and so are the test executors. One platform's
run meets the acceptance.

## Recorded on BPR-1048 from the same review, not in this card's acceptance

- **Finding 3.** When no turn is active, the failure runs at once. If the
  rotation's `completion(false)` then throws, the exception escapes to the
  caller: `:3956` calls the terminal outside any `try`, while the queued path
  catches it at `:3968-3971`.
  There is also a wording nit about "releases its own `Outstanding` count" on
  the card. If AC3's work touches `fail_withdrawn_binding`, make the two paths
  consistent.
- **Finding 5.** The launch-erase site counts the request (`:4473`) before the
  `std::function` conversion (`:4474`). In practice that conversion does not
  allocate.
- **Finding 6.** More than eight queued withdrawals is untested. The reviewer
  judges it unreachable.

## Built (Mac batch 8, 2026-09-24)

Claimed at `7a0e0406`. The product change, its tests and the architecture
note are `b7caeb2f`, patch-identical to the verified `98173609` it was
rebased from onto `3f8f954d`. Station `bpr-main-mac`: macOS 26.6.2 arm64, AppleClang,
Qt 6.11.2, `build/dev` Debug.

### Decisions

- **Finding 1: a turn that ends in an exception.** `run_turn`'s catch now
  runs the queued withdrawn failures before it rethrows. So they run before
  the exception reaches the turn's caller, and no later turn or second
  `shutdown()` is needed. Nobody has to call `shutdown()` twice. The
  architecture note says so next to BPR-1048's paragraph.
- **Finding 3, made consistent while touching `fail_withdrawn_binding`.** A
  failure delivered at once, outside a turn, now swallows a throwing terminal
  as the queued path does. The exception no longer reaches the lane call's
  caller.
- **Finding 4 and AC3: `start_delete` builds its terminal handler first.** It
  copies the authority and builds the `std::function` before it changes any
  state. If either allocation throws, **the caller gets the exception and
  nothing has changed**: durability, generation, the stored terminal, the
  count and the pending flag are all as they were. **The rotation site no
  longer allocates.** [[BPR-1116]] (`b71456a1`) made `prepare_device_rotation`
  answer once, write nothing, count nothing and call no lane. So there is no
  allocation left that could lose its completion. If `aCompletion` itself
  throws, its caller gets the exception and no state has changed.
- **Finding 5 is gone.** The launch-restore erase site was removed by
  BPR-0644's keep-grant change. **Finding 6** (more than eight queued
  withdrawals) is unchanged and untested; the reviewer judged it unreachable.

### What the injection found: finding 1 is latent

The card read finding 1 as reachable through "a projection sink that throws
during shutdown". In this host every projection publication is contained:
`publish_transition`, `publish_diagnostic`, `publish_public_preview` and
`republish_accepted_session` catch and set `ProjectionDeliveryFailed`. So a
sink cannot end a turn in an exception. The one realistic source is running
out of memory. The test binary now replaces `operator new` with a seam that
fails a chosen allocation (`allocation_fault`). The fault is armed at the
moment the lane withdraws the tombstone, and each later allocation is failed
in turn. In a shutdown there are 7 such allocations, and in a Sign out there
are 7. **None ends the turn in an exception.** Every one is inside a `try`,
and the end-of-turn drain delivers the failure as before. So on the
unchanged product the stranded queue is not reachable. The drain in the
catch makes the property hold by construction for any future throw.

**Design trial (not committed).** A throw was placed in `shutdown_now` right
after the cleanup's `start_delete`, while the lane withdrew the tombstone.
With the drain in the catch, the shutdown's exception reached the caller, the
withdrawn failure had already run (vault delete issued, `shutdown_cleanup_failed`
true), and the host was quiescent with no second call. With the catch as it
was, the failure never ran: no delete, not quiescent.

### Tests (`tests/application/github_application_host_tests.cpp`)

| Test | Shows |
| --- | --- |
| `test_a_withdrawal_at_shutdown_runs_its_failure` (AC2) | An accepted device grant that was never stored, so the shutdown policy removes it. Its tombstone is withdrawn during `shutdown()`. There is no throw. The failure runs in the shutdown turn: `vault_unavailable`, `shutdown_cleanup_failed`, and the credential is still erased. After draining alone, with no second shutdown, the host is quiescent |
| `test_a_shutdown_that_fails_after_a_withdrawal_still_runs_its_failure` (AC1) | The same shutdown, with each of the 7 allocations after the withdrawal failed in turn. Every one reaches quiescence after draining alone |
| `test_a_failed_allocation_in_start_delete_changes_nothing` (AC3) | A durable grant under `delete_retained_on_shutdown`, with each of the shutdown's 9 allocations failed in turn. If the cleanup's request was neither admitted nor failed, the accepted grant's durability is unchanged. The host always reaches quiescence: a failure before the shutdown closed the host needs the caller's second `shutdown()`, and nothing stays counted |
| `test_the_rotation_barrier_writes_nothing` ([[BPR-1116]]) | The rotation site: answers once, with no store operation, count or diagnostic, even while the lane withdraws everything |

### Negative controls (AC4)

- **AC3 fails on the unchanged product.** With the new test and the old
  `start_delete`: `start_delete: allocation 2 left durability changed with no
  request and no terminal`, then `FAILED: start_delete: no failed allocation
  changes durability without a request or a terminal`. Allocation 2 is the
  handler's `std::function`, built after the state had changed.
- **AC1 and AC2 pass on the unchanged product,** because they pin behaviour it
  already has. AC2 is the end-of-turn drain BPR-1048 built. AC1's gap is
  latent, as measured above. **AC4 is revised for these two:** under the
  owner's authority to revise stale cards, their controls are trials instead.
  The design trial above fails AC1's property with the old catch and passes
  it with the new one. For AC2, a trial that removes the end-of-turn drain
  fails the AC2 test: `FAILED: shutdown: the withdrawn tombstone's failure ran in the shutdown turn, and its path still erases the credential`.

### Verification

| Check | Result |
| --- | --- |
| AC5 set, 8 rows | 8 of 8 pass: `application_github_host`, `application_device_authorization_host`, `_refresh`, `_public_path`, `sqlite_device_binding_storage`, `application_device_binding_restart` (29.4 s), `_launch_renewal` (67.2 s), `_relaunch` (10.1 s). The unit row passes again at the committed candidate, after two test-only tidy fixes |
| clang-format 23.1.1 | Clean on both touched C++ files. Their HEAD versions were clean |
| `_coding-standard/scripts/tidy.py` | Nothing on a changed product line. On new test lines it found `size() > 0` (now `!empty()`), and the replaced `operator new` and `operator delete` naming their parameters differently from libc++'s reserved names (`NOLINTNEXTLINE`, with the reason, as the project does elsewhere). The four naming findings left in the file predate this change |

The code is platform-neutral, and so are the test's executors. The
allocation seam is standard C++ (a replaced global `operator new`). One
platform's run meets the acceptance, as the card says. Windows and Linux
confirm it in their next ordinary pass, and the allocation counts they print
(`BPR-1112 throwing shutdown: …`, `BPR-1112 start_delete: …`) may differ
by standard library:

```sh
ctest --test-dir <build> --output-on-failure -R "^(application_github_host|application_device_binding_restart|application_device_binding_launch_renewal|application_device_binding_relaunch|application_device_authorization_host|application_device_authorization_public_path)$"
```

## Post-close: Linux at `39460e81` (bpr-main-linux, 2026-09-24)

**The Linux confirmation this card asked for passes: its six rows, 6 of 6,
within a nine-row run.** Windows is still owed.

The run was in the station's main checkout, with a clean tree, at
`39460e81` ("Checkpoint Mac batch 8: complete"). WSL2 Ubuntu 26.04, GCC 15.2,
Qt 6.11.2, `build/dev` (Debug, `-Werror`), rebuilt at `39460e81` with no
`warning:` line. The run began straight after that build, at a load average
of 10.95. `ctest --test-dir build/dev -j1 --output-on-failure`, offscreen,
with `DISPLAY`, `WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS` unset and
fresh absolute `HOME` and XDG roots. The coordinator's nine rows ran together
for [[BPR-1116]], [[BPR-1111]] and [[BPR-1112]], and this station's own
uncommitted [[BPR-1121]] edits to two of these tests were set aside for it,
so the result is the Mac's change alone.

| Row | Result |
| --- | --- |
| `application_github_host` | pass, 0.03 s |
| `application_device_authorization_host` | pass, 0.00 s |
| `application_device_authorization_refresh` | pass, 0.00 s |
| `application_device_authorization_public_path` | pass, 0.39 s |
| `sqlite_device_binding_storage` | pass, 2.98 s |
| `sqlite_private_store_open` | pass, 0.07 s |
| `application_device_binding_restart` | pass, 29.01 s |
| `application_device_binding_launch_renewal` | pass, 66.61 s |
| `application_device_binding_relaunch` | pass, 8.14 s |

The runs added nothing to `/tmp` and no `bpr-` name to `/run/user/1000`.

The allocation counts differ by standard library, as the card expected. A
verbose rerun of `application_github_host` printed, with libstdc++:

```text
BPR-1112 throwing shutdown: 7 allocations after the withdrawal failed in turn, 0 ended the turn in an exception
BPR-1112 start_delete: 10 allocations failed in turn
```

The shutdown count is the 7 macOS counted. `start_delete` fails 10
allocations here against 9 on macOS, and none of them leaves durability
changed without a request or a terminal.

The acceptance record above is unchanged. The logs are station records under
`build/logs/bpr-main-linux-20260924/batch7-linux/` and are not committed:
`batch8-rows.log`, `batch8-github-host-verbose.log` and
`build-dev-39460e81.log`.

## Post-close: Windows at `a8c36152` (bpr-main-win, 2026-09-24)

**On Windows, this card's two allocation sweeps end
`application_github_host` in `std::terminate` in Debug builds. In Release
they pass, with the same counts macOS printed.** Its other five rows pass.
Its six rows are `application_github_host`, `_restart`, `_launch_renewal`,
`_relaunch`, `application_device_authorization_host` and `_public_path`.

The run was in the station's main checkout, with a clean tree, at
`a8c36152`, which contains `39460e81` ("Checkpoint Mac batch 8: complete").
The station is Windows 11 25H2 with MSVC 19.51 and Qt 6.11.2. The tree is
`build/windows-dev` (the Visual Studio 18 2026 generator, Debug), rebuilt at
`a8c36152` with no error and no compiler warning. The coordinator's nine rows
ran together for [[BPR-1116]], [[BPR-1111]] and [[BPR-1112]], serially, each
contained or Windows-uncontained as registered, at 00:49Z–00:52Z on
2026-09-25:
`ctest --preset windows-dev --output-on-failure -R "^(application_github_host|application_device_authorization_host|application_device_authorization_refresh|application_device_authorization_public_path|sqlite_device_binding_storage|sqlite_private_store_open|application_device_binding_restart|application_device_binding_launch_renewal|application_device_binding_relaunch)$"`.

| Row | Result |
| --- | --- |
| `application_github_host` | **fail**, 0.06 s: child exit 3, no `FAILED:` line |
| `application_device_authorization_host` | pass, 0.04 s |
| `application_device_authorization_refresh` | pass, 0.05 s |
| `application_device_authorization_public_path` | pass, 2.19 s |
| `sqlite_device_binding_storage` | pass, 3.43 s |
| `sqlite_private_store_open` | pass, 0.41 s |
| `application_device_binding_restart` | pass, 37.46 s |
| `application_device_binding_launch_renewal` | pass, 86.37 s, uncontained as registered, inside its 180 s bound |
| `application_device_binding_relaunch` | pass, 17.47 s |

`%TEMP%` gained no entry from the run.

**Why `application_github_host` fails on Windows: the two
allocation sweeps end in `std::terminate` under MSVC's Debug standard
library.** Exit 3 is `abort()`. The `build/dev` (Ninja) Debug binary does
the same, so the generator does not matter. A temporary terminate handler,
removed afterwards, printed the stack:

- `test_a_shutdown_that_fails_after_a_withdrawal_still_runs_its_failure`
  arms the fault inside `shutdown()`. The failure is in `fail_withdrawn_bindings`,
  then `start_delete`'s handler, then `binding_failure`, then
  `publish_diagnostic`, then the test's `recording_projections::publish`. There,
  `push_back` reallocates and moves each `github_presentation_update`. That
  move reaches `std::vector<github_command_projection>`'s move constructor,
  which is `noexcept`. Under `_ITERATOR_DEBUG_LEVEL` 2 that constructor
  allocates its `std::_Container_proxy` through the replaced `operator new`.
  The armed failure throws `std::bad_alloc` there, inside `noexcept`, so the
  program terminates.
- `test_a_failed_allocation_in_start_delete_changes_nothing`, with the first
  test skipped, fails the same way in the core. The path is
  `shutdown_now`, then `core::transition`, then `deactivate_identity`
  (`github_transition.cpp:213`), then the move of `github_transition_output`'s
  request vector, then `_Alloc_proxy`.

libc++ and libstdc++ allocate nothing in those moves, so neither sweep can
reach one there. The abort comes from where the sweep injects its failure,
and shows no product defect: a Release build, which is what ships, makes no
such allocation.

- **With those two tests skipped** (a temporary edit, restored), the Debug
  binary passes the rest (`application github host tests passed`). That
  includes this card's other cases, `test_a_withdrawal_at_shutdown_runs_its_failure`
  among them.
- **In `windows-dev`'s Release configuration** (iterator debugging off), the
  unchanged binary passes in full, and prints:

```text
BPR-1112 throwing shutdown: 7 allocations after the withdrawal failed in turn, 0 ended the turn in an exception
BPR-1112 start_delete: 9 allocations failed in turn
```

**The replaced `operator new`.** It replaces only the scalar `new` and the
two scalar `delete`s, with `malloc` and `free`. MSVC's default array,
`nothrow` and aligned forms either route to those or pair with each other.
Under the Debug CRT, `malloc` and `free` are the debug heap's. The Debug run
printed no heap or CRT report, and the rest of the binary passed with the
replacement active. So allocation and release pair correctly on both CRTs.

This makes `application_github_host` red in every Windows Debug build, which
is the configuration the station tests. The fix belongs to a follow-up card,
whose title has gone to the integrator. For example, the sweeps could be
skipped where `_ITERATOR_DEBUG_LEVEL` is not 0, with a line saying why, and
kept in Release.

The acceptance record above is unchanged. The logs are station records
under `build/logs/batch8-main-win/`, and are not committed: `wdev-build-a8c36152.log`, `rows-a8c36152.log`,
`probe-host-run.log`, `probe2-host-run.log`, `probe3-host-run.log` and
`release-host-direct.log`.

## Post-close: Windows after [[BPR-1124]] (bpr-main-win, 2026-09-25)

**`application_github_host` passes on Windows in Debug and in Release.**
BPR-1124 (`f2ca7bea`) keeps this card's two sweeps out of a checked MSVC
build, where the standard library allocates inside `noexcept` moves. Its
card has the reasoning and the controls.

| Tree | Result |
| --- | --- |
| `build/dev` Debug | pass, 0.11 s. Both sweeps print that they did not run, and why. Every other case runs, `test_a_withdrawal_at_shutdown_runs_its_failure` among them |
| `build/windows-dev` Debug | pass, 0.09 s, the same way |
| `build/windows-dev` Release | pass, 0.04 s. Both sweeps run and print 7 and 9, as on macOS |

The Release sweep still catches what this card fixed. With `start_delete`'s
state changes moved back before its handler (a trial, not committed), it
fails at `start_delete: allocation 2 left durability changed with no request
and no terminal`, as this card's macOS control did.

The acceptance record above is unchanged. The logs are station records under
`build/logs/bpr-1124-main-win/`, not committed.
