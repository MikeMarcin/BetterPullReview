---
schema: "implement-rfc.card.v1"
id: "BPR-1124"
title: "Windows Debug: BPR-1112's allocation sweeps end application_github_host in std::terminate, because MSVC's debug STL allocates inside noexcept moves"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"origin":"found by bpr-main-win in BPR-1112's Windows post-close run at a8c36152, 2026-09-25 (segment 9)","origin_card":"BPR-1112","related_cards":["BPR-1112","BPR-1116","BPR-1111"],"reservation":"docs/issues/_reservations/BPR-1123--BPR-1124--e9357c87ad44.json"}
acceptance: [{"criterion":"AC1 application_github_host passes in Windows Debug, in build/dev and build/windows-dev; every other case still runs, and the output says the two sweeps did not run and why","outcome":"pass","revision":"1b914abd","evidence":{"kind":"command","detail":"JaegAW, MSVC 19.51, Qt 6.11.2. build/dev Debug (Ninja): pass 0.11 s; build/windows-dev Debug (VS 18): pass 0.09 s. Both print 'BPR-1112 throwing shutdown: not run, because _ITERATOR_DEBUG_LEVEL 2 allocates inside noexcept moves ([[BPR-1124]])' and the start_delete equivalent, then 'application github host tests passed'.","path":"docs/issues/60-done/BPR-1124-windows-debug-bpr-1112-allocation-sweeps-terminate-under-msvc-debug-stl.md#built-and-verified-bpr-main-win-2026-09-25-1b914abd"}},{"criterion":"AC2 In windows-dev Release the unchanged sweeps run and print 7 and 9; the gate changes nothing where _ITERATOR_DEBUG_LEVEL is 0 or absent, so macOS and Linux are unaffected","outcome":"pass","revision":"1b914abd","evidence":{"kind":"command","detail":"ctest --test-dir build/windows-dev -C Release -R ^application_github_host$: pass 0.04 s, printing 'BPR-1112 throwing shutdown: 7 allocations after the withdrawal failed in turn, 0 ended the turn in an exception' and 'BPR-1112 start_delete: 9 allocations failed in turn'. The gate is the MSVC macro _ITERATOR_DEBUG_LEVEL, which libc++ and libstdc++ do not define."}},{"criterion":"AC3 Negative controls: without the gate Debug still terminates with exit 3; with the gate the Release sweep still catches the defect BPR-1112 fixed","outcome":"pass","revision":"1b914abd","evidence":{"kind":"command","detail":"Gate forced open (#if 0, temporary) in build/dev Debug: TEST_RUNNER_CHILD_EXIT_FAILED:3 (abort) in 0.06 s; restored, it passes. start_delete's state changes moved back before its handler (the pre-b7caeb2f ordering, trial not committed) in windows-dev Release: 'start_delete: allocation 2 left durability changed with no request and no terminal' then 'FAILED: start_delete: no failed allocation changes durability without a request or a terminal'; restored with no diff and rebuilt, it passes with 7 and 9.","path":"docs/issues/60-done/BPR-1124-windows-debug-bpr-1112-allocation-sweeps-terminate-under-msvc-debug-stl.md#built-and-verified-bpr-main-win-2026-09-25-1b914abd"}},{"criterion":"AC4 A note on BPR-1112 records the rerun in Debug and Release","outcome":"pass","revision":"1b914abd","evidence":{"kind":"artifact","detail":"BPR-1112's 'Post-close: Windows after [[BPR-1124]]' section: build/dev Debug and build/windows-dev Debug pass with the sweeps reported as not run, windows-dev Release passes with 7 and 9, and the Release trial with the old start_delete fails at allocation 2.","path":"docs/issues/60-done/BPR-1112-a-withdrawn-binding-failure-queued-in-a-throwing-turn-or-at-shutdown-may-never-run.md#post-close-windows-after-bpr-1124-bpr-main-win-2026-09-25"}}]
---
# BPR-1124: Windows Debug: BPR-1112's allocation sweeps end application_github_host in std::terminate, because MSVC's debug STL allocates inside noexcept moves

## Finding

[[BPR-1112]] added two allocation sweeps to
`tests/application/github_application_host_tests.cpp`. The test binary
replaces `operator new` with a seam (`allocation_fault`) that fails a chosen
allocation, and each sweep fails every allocation of one path in turn:

- `test_a_shutdown_that_fails_after_a_withdrawal_still_runs_its_failure`;
- `test_a_failed_allocation_in_start_delete_changes_nothing`.

On Windows, in every Debug build, `application_github_host` ends in
`std::terminate`. Observed by bpr-main-win at `a8c36152` (MSVC 19.51, Qt
6.11.2), in `build/windows-dev` Debug and `build/dev` (Ninja) Debug alike, so
the generator does not matter. BPR-1112's Windows post-close note has the run.

- The row fails in 0.06 s with child exit 3, which is `abort()`, and no
  `FAILED:` line.
- A temporary terminate handler printed the stacks.
  - **The first sweep.** The fault is armed inside `shutdown()`. The failure is
    in `fail_withdrawn_bindings`, then `start_delete`'s handler,
    `binding_failure`, `publish_diagnostic`, and the test's
    `recording_projections::publish`. There `push_back` reallocates and moves
    each `github_presentation_update`. That reaches
    `std::vector<github_command_projection>`'s move constructor, which is
    `noexcept`. Under `_ITERATOR_DEBUG_LEVEL` 2 that constructor allocates its
    `std::_Container_proxy` through the replaced `operator new`. The armed
    failure throws `std::bad_alloc` there, inside `noexcept`, and the program
    terminates.
  - **The second sweep**, with the first skipped, fails the same way in the
    core: `shutdown_now`, then `core::transition`, then `deactivate_identity`
    (`github_transition.cpp:213`), then the move of
    `github_transition_output`'s request vector, then `_Alloc_proxy`.
- libc++ and libstdc++ allocate nothing in those moves, so neither sweep can
  reach one on macOS or Linux. A Release build, which is what ships, makes no
  such allocation either.
- With the two sweeps skipped (a temporary edit), the Debug binary passes the
  rest of the suite, BPR-1112's other cases among them.
- In `windows-dev`'s Release configuration (iterator debugging off), the
  unchanged binary passes in full and prints the counts macOS printed:

```text
BPR-1112 throwing shutdown: 7 allocations after the withdrawal failed in turn, 0 ended the turn in an exception
BPR-1112 start_delete: 9 allocations failed in turn
```

- The replaced `operator new` pairs correctly on both CRTs: the Debug run
  printed no heap or CRT report, and the rest of the binary passed with the
  replacement active.

The abort comes from where the sweep injects its failure. It shows no product
defect. It makes `application_github_host` red in every Windows Debug build,
which is the configuration this station tests.

## Scope

- The sweeps must never inject inside a `noexcept` standard-library move. The
  debug STL's proxy allocations are not the product's, and failing one inside
  `noexcept` can only terminate.
- Release coverage stays as it is. A Release build still runs both sweeps and
  prints 7 and 9, as on macOS. macOS and Linux Debug keep running them.
- State the reason where the choice is made, in the test file and here.

## Acceptance

- AC1: `application_github_host` passes in Windows Debug, in `build/dev` and
  `build/windows-dev`. Every other case still runs, and the output says the
  two sweeps did not run and why.
- AC2: In `windows-dev` Release the unchanged sweeps run and print 7 and 9.
  The gate changes nothing where `_ITERATOR_DEBUG_LEVEL` is 0 or absent, so
  macOS and Linux are unaffected.
- AC3: Negative controls. Without the gate, Debug still terminates with exit 3.
  With the gate, the Release sweep still catches the defect BPR-1112 fixed: the
  old `start_delete` ordering, restored as a trial, fails it.
- AC4: A note on BPR-1112 records the rerun in Debug and Release.

## Built and verified, bpr-main-win, 2026-09-25 (`1b914abd`)

Verified as `f2ca7bea` on `79a96104`. It was rebased onto origin's macOS test-only commits `053abefe` to `feabac34`, none of which this binary builds, and pushed as `1b914abd`: the same patch (patch-id `bf73293a`).

**The choice: the sweeps do not run where the standard library is checked.**
A sweep that skipped only the debug STL's own allocations would have to
recognise a `std::_Container_proxy` allocation from inside `operator new`.
Nothing there identifies it: it goes through `std::allocator` to the same
global `operator new` as every other 16-byte allocation. So the gate is the
documented macro instead. In `tests/application/github_application_host_tests.cpp`,
`BPR_TEST_ALLOCATION_SWEEPS` is 0 where `_ITERATOR_DEBUG_LEVEL` is defined
and not 0, with the reason stated beside it. Each sweep's body compiles only
where it is 1. A checked build prints instead:

```text
BPR-1112 throwing shutdown: not run, because _ITERATOR_DEBUG_LEVEL 2 allocates inside noexcept moves ([[BPR-1124]])
BPR-1112 start_delete: not run, because _ITERATOR_DEBUG_LEVEL 2 allocates inside noexcept moves ([[BPR-1124]])
```

Every other case in the binary still runs, [[BPR-1112]]'s
`test_a_withdrawal_at_shutdown_runs_its_failure` among them. The replaced
`operator new` stays active in every build; only the two sweeps arm it.
`_ITERATOR_DEBUG_LEVEL` is MSVC's, so macOS and Linux Debug keep both
sweeps. So does every Release build.

**Verification** (JaegAW, Windows 11 25H2, MSVC 19.51, Qt 6.11.2; tree at the
rebased candidate on `79a96104`, which also carries BPR-1123 and BPR-1125's
uncommitted edits; neither touches this binary):

| Run | Result |
| --- | --- |
| `build/dev` Debug (Ninja), `application_github_host` | pass, 0.11 s; both "not run" lines, then `application github host tests passed` |
| `build/windows-dev` Debug (Visual Studio 18), same row | pass, 0.09 s; the same lines |
| `build/windows-dev` Release, same row (`ctest -C Release`) | pass, 0.04 s; `BPR-1112 throwing shutdown: 7 allocations after the withdrawal failed in turn, 0 ended the turn in an exception` and `BPR-1112 start_delete: 9 allocations failed in turn`, the counts macOS printed |
| clang-format 22.1.8 | clean; it indents the two `#define`s under their `#if` |
| `_coding-standard/scripts/tidy.py` | nothing on a changed line; the four lambda-name findings in the file predate this change |

**Negative controls (AC3).**

- **The gate forced open** (`#if 0` in place of the `_ITERATOR_DEBUG_LEVEL`
  test, a temporary edit) in `build/dev` Debug: the row fails in 0.06 s with
  `TEST_RUNNER_CHILD_EXIT_FAILED:3`, which is `abort()`. That is the failure
  this card was written for. Restored and rebuilt, it passes.
- **Release still catches BPR-1112's defect.** `start_delete`'s six state
  changes were moved back before its handler is built, as they were before
  `b7caeb2f`, a trial in `src/application/src/github_application_host.cpp`
  that was not committed. In `windows-dev` Release the sweep then fails
  exactly as BPR-1112 recorded on macOS: `start_delete: allocation 2 left
  durability changed with no request and no terminal`, then `FAILED:
  start_delete: no failed allocation changes durability without a request or
  a terminal`. Restored (no diff left) and rebuilt, the row passes again with
  7 and 9.

The logs are station records under `build/logs/bpr-1124-main-win/`, not
committed.

Every criterion is met. The note on [[BPR-1112]] records the reruns.
