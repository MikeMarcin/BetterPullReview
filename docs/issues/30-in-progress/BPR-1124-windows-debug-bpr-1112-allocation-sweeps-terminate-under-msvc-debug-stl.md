---
schema: "implement-rfc.card.v1"
id: "BPR-1124"
title: "Windows Debug: BPR-1112's allocation sweeps end application_github_host in std::terminate, because MSVC's debug STL allocates inside noexcept moves"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"origin":"found by bpr-main-win in BPR-1112's Windows post-close run at a8c36152, 2026-09-25 (segment 9)","origin_card":"BPR-1112","related_cards":["BPR-1112","BPR-1116","BPR-1111"],"reservation":"docs/issues/_reservations/BPR-1123--BPR-1124--e9357c87ad44.json"}
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
