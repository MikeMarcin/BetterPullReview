---
schema: "implement-rfc.card.v1"
id: "BPR-0810"
title: "Windows: the multi-window public path reaches an empty optional"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win under BPR-0806","found_at":"d587b5f1","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
acceptance: [{"criterion":"application_profile_multi_window_public_path passes through ctest --preset windows-dev on the Windows station","outcome":"pass","revision":"3ca3e12c","evidence":{"kind":"observation","detail":"bpr-win 33.70/33.76 s pass; WINDOWS_UNCONTAINED pinned dos-volume-name with measured contained cause; gate delta zero"}},{"criterion":"The dereference site is guarded or unreachable by construction, and the macOS course still passes","outcome":"pass","revision":"f33a10c9","evidence":{"kind":"command","detail":"adopt_review_selection re-presents the selected file's accepted receipt after selection_cleared; application_profile_multi_window_public_path 74.56 s pass on macOS at merge; build/r2-logs/int3-ctest.log"}}]
---

# BPR-0810: Windows: the multi-window public path reaches an empty optional

## Defect

On `windows-dev` Debug at `d587b5f1`:

- through ctest (contained), before the uncontained decision:
  `FAILED: private desktop test root under the canonical storage policy`;
- run directly (uncontained), after 30.45 s: MSVC
  `optional(429) : Assertion failed: operator->() called on empty optional`.

Before [[BPR-0806]] it stopped at `startup failure: 5 / FAILED: profile owner admitted`
after 0.09 s. The same course passes on macOS at `c0f2fa31` in 79.36 s. In a
release build an empty-optional dereference is undefined behaviour, not an
assertion, so the defect may exist on every platform and only MSVC's checked
iterators report it.

## Scope

- Get a stack for the assertion on the Windows station and identify the
  optional and the path that reaches it empty.
- Fix the source. If the empty state is legitimate on Windows (for example a
  capability the platform lacks), make the caller handle it explicitly and
  check whether macOS and Linux can reach it too.
- Register the course contained or uncontained with a measured cause, as
  BPR-0806 did.

## Acceptance

- `application_profile_multi_window_public_path` passes through
  `ctest --preset windows-dev` on the Windows station.
- The dereference site is guarded or made unreachable by construction, and the
  macOS course still passes.

## Evidence

Station `bpr-win` recorded this for task `task-20260917-win-regressions`, from
base `53a0a7477264c71423b11fa3bc85d2e668b389b4` plus the change committed with
this evidence.

### The stack and the empty optional

The stack was captured with a temporary Debug CRT report hook, symbolized
through dbghelp and since reverted:

    std::optional<bpr::core::file_receipt>::operator->  <MSVC>/include/optional:429
    main  tests/end_to_end/profile_multi_window_public_path_tests.cpp:573

The empty optional is `review_editor()->shown_receipt()`, read in the
same-review receipt course that follows `receipt course starts with the same
exact unread file material`.

Temporary probes, also reverted, recorded this sequence:

1. Both windows had their receipts (the course's wait passed).
2. The course then calls `comparison_surface()->select_conversation_line(...)`
   in windows B and C.
3. For each window, the composition runs `observe_review_selection` (a new
   selection, no pending mutation), then
   `adopt_review_selection(..., selection_cleared)`.
4. `adopt_review_selection` calls `review_draft_editor::present(clear_reason)`.
   That call resets the shown draft and the shown receipt together.
5. Both receipts were empty immediately afterwards and stayed empty for 10 s.

No ledger publication follows a line selection. Receipts are published only
when the ledger host delivers a file's receipt.

### Can macOS and Linux reach it?

**Yes, on every platform.** The clear is platform-independent composition code.
The macOS course passes only because libc++ without hardening does not check
`optional::operator->`. It reads the destroyed receipt's bytes, which still hold
the same values, so the dereference is undefined behaviour that happens to
compare equal. MSVC's checked `operator->` is the only one that reports it.

The empty state is not legitimate. A new line in the same file leaves that
file's accepted ledger receipt unchanged. Presenting none disables Accept and
Exclude, which are gated on the shown receipt, until some later publication.

### Fix at the source

`adopt_review_selection` (`src/app/src/github_application.cpp`, composition,
portable) now re-presents the receipt after clearing, under three conditions:

- the adopted selection exists;
- the reason is `selection_cleared`;
- the composition's accepted `ReviewLedgerReceipt` is for the selected file
  (`SelectedDiffPublication->Request.File.File`, read under
  `ReviewLedgerStateMutex`).

`selection_invalidated` and a cleared selection keep today's behaviour. The
dereference is now unreachable by construction for a same-file line selection.

### The course's diagnostics claim

With the receipt fixed, the course ran through to its end and stopped at `one
profile services startup`. It read `logs/diagnostics.log`, and Windows has no
file sink. The course now makes the same typed per-platform claim that
[[BPR-0806]] made for the management course: the logging adapter's
`diagnostic_log_status` from a probe `quill_log` decides it
([[BPR-0701]], [[BPR-0603]]).

- Where the sink is `available`, it keeps the exact one-startup and
  one-shutdown assertions.
- On Windows it requires `unavailable` and no diagnostics file.

### Registration

Contained, the course stops at `FAILED: private desktop test root under the
canonical storage policy`. That is `make_storage_root_owner_private` refusing
the package temp root, the measured `dos-volume-name` cause pinned for the
BPR-0806 profile rows. `application_setup_destination_public_path` stops
contained at the identical line.

Both are now `WINDOWS_UNCONTAINED` and pinned in
`scripts/verify_test_network_isolation.py` with that cause. The envelope
permitted the setup destination row explicitly.

`test_network_isolation_manifest` shows the same 25 sorted findings before and
after the registration, so the delta is zero.
`tests/scripts/verify_test_network_isolation_tests.py` passes (skipped=9).

### Verification on `windows-dev` (Debug, MSVC)

**Before the fix**, run directly: `optional(429) : Assertion failed:
operator->() called on empty optional` after 30.45 s.

**After the fix**, `ctest --preset windows-dev -R
"^(application_profile_multi_window_public_path|application_setup_destination_public_path|application_device_binding_restart|application_profile_management_public_path|application_profile_startup_public_path|application_windows_profile_storage_startup_contract)$"`,
run twice with identical source:

| Course | Run 1 | Run 2 |
|---|---|---|
| `application_profile_multi_window_public_path` | 33.70 s | 33.76 s |
| `application_setup_destination_public_path` | 5.21 s | 5.15 s |
| `application_device_binding_restart` | 38.91 s | 38.82 s |
| `application_profile_management_public_path` | 13.35 s | 13.28 s |
| `application_profile_startup_public_path` | 8.78 s | 8.69 s |
| `application_windows_profile_storage_startup_contract` | 0.07 s | 0.06 s |

All six passed in both runs.

**A failure not explained.** In one earlier run, the first after reconfiguring
and relinking the same source, four of those courses exited `0xc0000409`
within 2 to 8 s:

- `application_device_binding_restart`
- `application_setup_destination_public_path`
- `application_profile_startup_public_path`
- `application_profile_management_public_path`

It was not reproduced. The next two full ctest runs passed, and so did a direct
run of the management course. A temporary guard around the new lock never
fired. It is recorded rather than explained.

The `windows-app-release` image builds with the change.

**The macOS course was not run on this station.** The change is portable: the
composition, the course and the CMake and script registrations. The integrator
runs the Mac courses at merge.

## Integration and macOS evidence, 2026-09-17 (integrator, run-20260916-stations, dsp-001-r2)

Merged on main as `f33a10c9`. The fix is in `adopt_review_selection`. For a
present selection cleared by `selection_cleared`, it re-presents the file's
accepted `ReviewLedgerReceipt` when that receipt belongs to the selected file,
read under `ReviewLedgerStateMutex`. The empty-optional read is therefore
unreachable by construction on every platform. Before the fix it was undefined
behaviour everywhere: macOS passed only because libc++'s unhardened
`optional::operator->` read destroyed bytes.

| Criterion | Result | Revision |
|---|---|---|
| Passes through `ctest --preset windows-dev` | pass: 33.70 s and 33.76 s on bpr-win. Contained registration fails on the canonical-storage temp root, so the course is `WINDOWS_UNCONTAINED`, pinned `dos-volume-name`; `application_setup_destination_public_path` is pinned under the same measured cause. Isolation gate delta is zero | `3ca3e12c` |
| Dereference site guarded or unreachable, and the macOS course still passes | pass: `application_profile_multi_window_public_path` 74.56 s at the merge, asserting the diagnostics sink `available` on macOS | `f33a10c9` |

Other courses passed at the merge: `application_setup_destination_public_path`,
`application_profile_startup_public_path`,
`application_profile_management_public_path`, `qt_review_draft_editor`,
`application_review_ledger_host`, `application_review_ledger_composition_contract`,
`application_review_submission_composition_public_path`,
`application_window_review_restoration`, `qt_composed_github_scale_100` and
`architecture_boundaries`. Two review contracts failed. Both already fail at a
clean `8b2bea5b`, so this change did not cause them; they are [[BPR-0818]] and
[[BPR-0819]].

**Residual.** On one Windows run, the first after reconfiguring and relinking
the same source, four courses exited `0xc0000409` in 2–8 s. It did not
reproduce in two full runs, a direct run, or with a temporary guard around the
new lock. It is recorded but not explained.
