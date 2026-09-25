---
schema: "implement-rfc.card.v1"
id: "BPR-1116"
title: "An in-process renewal that gives up, or a restored session whose refresh slot is missing, still erases the remembered grant"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"BPR-0644 keep-grant round, Mac batch 7, 2026-09-24","origin":"owner decision 3c46d164 on BPR-0644-grant-policy-v001, attended session cbd48b91000b00f1e72e1ad62c7c384c, note 'Shouldn't remove until the user removes it in the settings menu.'","related_cards":["BPR-0644","BPR-0541","BPR-1048","BPR-1112"],"reservation":"docs/issues/_reservations/BPR-1116--BPR-1116--c8c449f21922.json"}
acceptance: [{"criterion":"AC1 An accepted device grant whose in-process renewal gives up for a network or credential-store failure keeps both slots and a live row, and the next launch restores or renews it. A unit case and a composed course show it, and each fails with the erasure restored.","outcome":"pass","revision":"b71456a1","evidence":{"kind":"command","detail":"macOS build/dev. application_github_host test_an_accepted_renewal_that_gives_up_keeps_the_grant: network and vault each end the session as session_failed, erase neither slot, write no tombstone, publish GrantKept with their reason, keep the grant through an orderly quit, and a fresh host over the same store restores it and reads the kept access slot. application_device_binding_launch_renewal (66.4 s), offline in-process course: a device sign-in in this lifetime whose renewal is offline on every attempt ends visibly after the retries, keeps both slots with no delete attempted, keeps them through an orderly quit, and the next launch restores the session with nothing from the token endpoint. Negative controls with the old deactivation restored: the unit case fails at 'network: a renewal that gives up ends its session visibly' (and, with that check disabled, at 'network: and keeps the grant: no tombstone, and neither slot is erased'); the course fails at 'the renewal gives up and the session ends visibly'. With the old rotation tombstone restored the course fails at the kept-grant check.","path":"docs/issues/60-done/BPR-1116-an-in-process-renewal-that-gives-up-still-erases-the-remembered-grant.md#built-mac-batch-8-2026-09-24"}},{"criterion":"AC2 A refused in-process renewal keeps the grant, says it needs signing in again, and offers Begin and Sign out, as BPR-0644's launch does.","outcome":"pass","revision":"b71456a1","evidence":{"kind":"command","detail":"application_github_host: the refused case publishes GrantKept with authentication_failed and keeps both slots; Sign out then erases both behind a tombstone. application_device_binding_launch_renewal, refused in-process course: the card's KeptGrant is needs_sign_in with the sentence 'Your sign-in on this Mac needs signing in again. It stays remembered until you sign in or sign out.', Begin is enabled and Sign out is visible; Sign out erases both slots and the next launch is signed out with Begin offered. The device host's chain is released through the launch renewal sink so its sign-in row does not replace the card's Sign out.","path":"docs/issues/60-done/BPR-1116-an-in-process-renewal-that-gives-up-still-erases-the-remembered-grant.md#built-mac-batch-8-2026-09-24"}},{"criterion":"AC3 A restored session whose refresh slot is missing is not signed out and erased on its own. The card records what it does instead.","outcome":"pass","revision":"b71456a1","evidence":{"kind":"command","detail":"application_github_host test_a_restored_grant_without_a_refresh_slot_stays_signed_in: after the probe answers absent the session stays accepted and durable, nothing is erased or tombstoned, and the grant projection says Renews false and Remembered true; Sign out removes it. Negative control with the old binding_failure plus deactivation restored fails at 'a missing refresh slot does not sign the restored session out'. Recorded on the card: the session stays on its access token, unrenewed, the card says it does not renew itself, and the next launch after expiry meets refresh_absent and keeps the grant asking for signing in again.","path":"docs/issues/60-done/BPR-1116-an-in-process-renewal-that-gives-up-still-erases-the-remembered-grant.md#what-each-removal-does-now"}},{"criterion":"AC4 application_github_host, application_device_authorization_refresh, the device-binding courses and the composition contracts pass.","outcome":"pass","revision":"b71456a1","evidence":{"kind":"command","detail":"12 of 12 on macOS: application_github_host, application_device_authorization_host, _refresh, _public_path, _registered_course, _refresh_course, _orphan_course, application_composition_contract (206.1 s), application_review_round_trip_composition_contract, application_device_binding_restart, _launch_renewal, _relaunch. The committed candidate rebuilt: application_github_host, application_device_authorization_refresh and application_device_binding_launch_renewal pass again. Windows and Linux confirm in their next ordinary pass (command on the card)."}}]
---

# BPR-1116: An in-process renewal that gives up, or a restored session whose refresh slot is missing, still erases the remembered grant

## Why

On 2026-09-24 the owner answered [[BPR-0644]]'s grant-policy question
(decision `3c46d164`): keep the material, fail visibly, and retry at the next
launch. His note states the rule behind it, verbatim:

> Shouldn't remove until the user removes it in the settings menu.

[[BPR-0644]] builds that rule for **launches**: a restore whose keychain is
locked or denied, whose network is down, whose access slot is missing, or
whose `/user` refuses the token keeps the grant, and so does a launch renewal
that is offline, refused, or past its refresh lifetime. The launch renewal no
longer writes a tombstone before its first attempt.

Three automatic removals are outside that card's launch scope and still
happen **while the application is running**:

1. **An in-process renewal that gives up.** The accepted session's renewal
   retries a network failure three times, five seconds apart, then reports
   `reenrollment_required`. The device host calls `invalidate_grant`, and
   `github_application_host::invalidate_device_grant_now` deactivates the
   accepted session. The deactivation's `invalidate` then runs `start_delete`,
   which tombstones the row and erases both slots. A laptop that is offline
   when the renewal timer fires loses the grant this way.
2. **The in-process rotation's tombstone.** `prepare_device_rotation`, for an
   accepted session, still tombstones the row before the refresh material is
   spent ([[BPR-1048]]'s barrier). If that renewal then fails, or the process
   ends before the rotated grant's binding is stored, the next launch finds the
   row tombstoned and does not restore it.
3. **A restored session whose refresh slot is missing.** After a restored
   session is accepted, the host probes for the refresh slot. If it answers
   absent, `handle_refresh_probe` calls `binding_failure()` and dispatches a
   deactivation, and the grant is erased although its access token still works
   for hours.

A fourth, narrower window: quitting while a launch renewal's rotated access
token is still being stored deletes the provisional credential at shutdown.
The row stays live, so the next launch renews from the rotated refresh token
if that store landed.

## Scope

- Apply the owner's rule to the three in-process removals above. A renewal
  that gives up keeps the grant: the session ends or stays, visibly, and the
  row and both slots remain for the next launch. What the running session shows
  meanwhile is part of the design.
- Decide what the in-process rotation's crash-consistency barrier becomes
  without a tombstone that the next launch reads as a removal. BPR-0644's
  launch renewal answers this for itself: the row stays live, and a crash
  after the spend leaves the next launch to spend whatever the refresh slot
  holds. Coordinate with [[BPR-1112]], which also touches
  `prepare_device_rotation`.
- Keep the removals that are the person's own: Sign out, a replacing token, and
  a new device grant.

## Acceptance

- AC1 An accepted device grant whose in-process renewal gives up for a network
  or credential-store failure keeps both slots and a live row, and the next
  launch restores or renews it. A unit case and a composed course show it, and
  each fails with the erasure restored.
- AC2 A refused in-process renewal keeps the grant, says it needs signing in
  again, and offers Begin and Sign out, as BPR-0644's launch does.
- AC3 A restored session whose refresh slot is missing is not signed out and
  erased on its own. The card records what it does instead.
- AC4 `application_github_host`, `application_device_authorization_refresh`,
  the device-binding courses and the composition contracts pass.

## Boundaries

The core decides, the host performs, Qt presents. No live GitHub, credential or
device-flow request; fixtures only. [[BPR-0541]]'s rule that nothing resumes
from unvalidated material stands: keeping a grant is not using it.

## Built (Mac batch 8, 2026-09-24)

Claimed at `f3d94f65`. The product change, its tests and the architecture
notes are `b71456a1`, patch-identical to the verified `65e06351` it was
rebased from onto `3c3c34d8`. Station `bpr-main-mac`: macOS 26.6.2 arm64,
AppleClang, Qt 6.11.2, `build/dev` Debug.

### What each removal does now

1. **An accepted session's renewal that gives up.** `invalidate_device_grant`
   for an accepted session no longer deactivates it. The host resumes the
   profile from its own remembered binding, which the invalidation that goes
   with a resume leaves in place, and fails that resumed session at once with
   the renewal's failure (`network`, `vault` or `authentication`). So the row
   and both slots stay, and the credential stays selected, so Sign out erases
   both through the ordered path. An orderly quit keeps the grant. The failure
   carries `GrantKept`, and the card says how the grant ends and offers Sign
   out beside Sign in, as the launch does. The device host's chain is released
   through the same sink a launch renewal uses. Left in `reenrollment_required`,
   its sign-in row stood where the card offers Sign out; the composed course
   found this. **What the running session shows:** the session ends with the
   renewal's reason. Offline or credential store: *"Your sign-in is still
   remembered on this Mac and is tried again at the next launch. Sign out
   forgets it."* Refused: *"Your sign-in on this Mac needs signing in again. It
   stays remembered until you sign in or sign out."* A grant whose binding was
   never stored is not claimed as kept. The orderly quit still removes it, as it
   removes any grant kept for one session only.
2. **The in-process rotation's tombstone.** `prepare_device_rotation` writes no
   tombstone for an accepted session either, as BPR-0644 decided for the
   launch. It answers once whether this exact grant may rotate, changes no
   state, counts nothing and calls no lane. **The barrier's replacement:** the
   row stays live until the rotated grant's binding replaces it, so a crash
   after the spend leaves the next launch to spend whatever the refresh slot
   holds. That is the rotated token if its store landed, or the spent one,
   which asks for signing in again. For [[BPR-1112]] this means the rotation
   site no longer has an allocation or a pending state to lose.
3. **A restored session whose refresh slot is missing.** It is not signed out
   or erased. **What it does instead:** the session stays signed in on its
   access token and nothing is armed to renew it. The grant projection's
   `Renews` becomes false, so the card says *"Signed in with a device grant on
   ‹date›; does not renew itself and is remembered on this Mac."* Reads fail
   as `authentication` once that token expires. The next launch then renews
   the access-expired grant, meets the missing refresh token as `refresh_absent`,
   and keeps the grant, saying it needs signing in again.

The person's own removals are unchanged: Sign out, a replacing token and a
new device grant.

### Residual (recorded, not changed)

- **A successful in-process rotation's enrollment is a replacement.** It
  tombstones the row and deletes the old access slot, keeping the refresh slot
  it just filled, and the rotated grant's binding replaces the row once its
  session is accepted. A process that ends inside that window leaves the row
  tombstoned, so the next launch offers Begin. The window runs from the
  tombstone through the rotated session's vault store, `/user` and
  capabilities to its binding store. It is shorter than the old barrier's
  window, which also covered the token exchange and the refresh store. The
  architecture note states it.
- **A renewal timer that fires after the access token expired**, for example
  on a Mac that slept past it, reports `refresh_rejected` without trying the
  refresh token (the device host's `begin_refresh` window check). The grant is
  now kept, and the card says it needs signing in again. The next launch
  renews it silently through BPR-0644's launch renewal, so the sentence is
  stricter than the outcome. The card's scope did not ask to change the device
  host's window.
- The card's fourth window (quitting while a launch renewal's rotated access
  token is being stored) is unchanged.

### Verification (macOS, `build/dev`)

| Check | Result |
| --- | --- |
| `application_github_host` | Pass. `test_an_accepted_renewal_that_gives_up_keeps_the_grant` covers network, vault and refused. Each ends the session as `session_failed`, erases neither slot, writes no tombstone and publishes `GrantKept` with its own reason. Refused: Sign out then erases both slots behind a tombstone. Network and vault: an orderly quit keeps the grant, and a fresh host over the same store restores it and reads the kept access slot. It also covers an unremembered grant, not claimed as kept and removed at quit, and the stale and foreign guards. `test_the_rotation_barrier_writes_nothing`: answers `true` once, while the lane withdraws everything, with no store operation, no tombstone and no diagnostic; foreign or stale answers `false`. `test_a_restored_grant_without_a_refresh_slot_stays_signed_in`: accepted, durable, `Renews` false, `Remembered` true, nothing erased; Sign out removes it |
| `application_device_binding_launch_renewal`, 66.4 s, two new in-process courses | Pass. **Offline:** a device sign-in in this lifetime, the renewal course's clock, and every renewal attempt offline. The session fails visibly after the three retries, the card keeps the grant with the next-launch sentence and Sign out beside Begin, no delete is attempted, and an orderly quit keeps both slots. **The next launch restores it:** `session_ready`, renewal armed, and nothing from the token endpoint. **Refused:** the same with the refresh refused. The card says it needs signing in again and offers Begin and Sign out; Sign out erases both slots, and the next launch is signed out with Begin offered |
| Negative controls, host unit (`erase`, `tombstone` and `probe` restored one at a time) | Each fails. With the old deactivation: `network: a renewal that gives up ends its session visibly`, and with that check disabled `network: and keeps the grant: no tombstone, and neither slot is erased`. With the old barrier: `rotation: the barrier answers true exactly once, at once, while the lane withdraws everything`. With the old probe answer: `a missing refresh slot does not sign the restored session out` |
| Negative controls, composed course | With the old deactivation: `the renewal gives up and the session ends visibly` (the session goes to signed out). With the old barrier: `an in-process renewal that gives up offline keeps the grant and says the next launch tries again` (the tombstoned row is not claimed as kept) |
| AC4 set, 12 rows | 12 of 12 pass: `application_github_host`, `application_device_authorization_host`, `_refresh`, `_public_path`, `_registered_course`, `_refresh_course`, `_orphan_course`, `application_composition_contract` (206.1 s), `application_review_round_trip_composition_contract`, `application_device_binding_restart`, `_launch_renewal`, `_relaunch` |
| Candidate `65e06351` (landed as `b71456a1`) rebuilt | `application_github_host`, `application_device_authorization_refresh` and `application_device_binding_launch_renewal` pass again after formatting |
| clang-format 23.1.1 | Clean on every touched C++ file. The HEAD versions were clean, so only changed lines moved |
| `_coding-standard/scripts/tidy.py` on the three translation units | One finding on a new line, the local `reason_of`, which is renamed `reasonOf` in the finish commit; the unit row passes again. The others are older: `names_only_the_renewal`, `complete_deletes`, `expect_kept` and the other `reason_of` in the host tests, `home_view_state.hpp:35`, and a `std::endl` in the course at line 89 |

The course's `TIMEOUT` rises from 90 to 180 s for the offline retries. No
`src/app` file changed, so the round-trip pins are not resealed.

### Owed elsewhere

The change is portable. One platform's run meets the acceptance. Windows and
Linux confirm it in their next ordinary pass:

```sh
ctest --test-dir <build> --output-on-failure -R "^(application_github_host|application_device_authorization_refresh|application_device_binding_launch_renewal|application_device_binding_relaunch|application_device_binding_restart)$"
```

On Windows the launch renewal course runs uncontained as registered
([[BPR-0876]]); expect about twice its old 30 to 38 s.

## Post-close: Linux at `39460e81` (bpr-main-linux, 2026-09-24)

**The Linux confirmation this card asked for passes: its five rows, 5 of 5,
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

The acceptance record above is unchanged. The logs are station records under
`build/logs/bpr-main-linux-20260924/batch7-linux/` and are not committed:
`batch8-rows.log`, `batch8-github-host-verbose.log` and
`build-dev-39460e81.log`.

## Post-close: Windows at `a8c36152` (bpr-main-win, 2026-09-24)

**This card's rows pass on Windows, except `application_github_host`,
which aborts in [[BPR-1112]]'s allocation sweeps under MSVC's Debug library,
not in this card's cases.** Its five rows are `application_github_host`,
`_refresh`, `_launch_renewal`, `_relaunch` and `_restart`. The launch renewal
course runs uncontained, as the card expected, in 86.37 s. That is about
twice its old 30 to 38 s.

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

**Why `application_github_host` fails on Windows: [[BPR-1112]]'s two
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
  includes this card's cases,
  `test_an_accepted_renewal_that_gives_up_keeps_the_grant` and
  `test_a_restored_grant_without_a_refresh_slot_stays_signed_in`.
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
