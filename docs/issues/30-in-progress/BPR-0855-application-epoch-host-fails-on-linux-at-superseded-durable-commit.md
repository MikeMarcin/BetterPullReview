---
schema: "implement-rfc.card.v1"
id: "BPR-0855"
title: "application_epoch_host fails on Linux at the superseded durable commit"
status: "In Progress"
rfcs: ["RFC-BPR-0029"]
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-0851-sweep full ctest sweep","found_at":"a1864fa296ce3450ce0e0470f36bd0d95cd59470","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0855: application_epoch_host fails on Linux at the superseded durable commit

## Defect

`application_epoch_host` fails on `bpr-linux` at `a1864fa2` with
`FAILED: superseded durable commit rebases the latest selection and keeps core/catalog synchronized`.
It fails in 0.26 s in the sweep and in 0.23 s alone
(`docs/verification/dsp4-linux-confirmation.md`).

On macOS the row passes: 0.73 s at `20d99914`
(`build/r2-logs/dsp4-linux-sweep-mac-classify.log`), and earlier 0.54 s in
dsp-003's bundle-focused set.

## Scope

- Name why the case diverges on Linux: ordering, timing, a platform
  difference in the host test, or a product defect.
- Fix the cause. A product fix in epoch selection or catalog synchronization
  gets a review.

## Acceptance

- The row passes on Linux 3 runs in a row, still passes on macOS, and the
  cause is named here.

## Current frontier (dsp-004, bpr-linux, task-20260917-lnx-sweep-fixes)

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-004` (station
  dispatch `bpr-linux/int-11`), mandate r2. Station `bpr-linux`
  (`stn-3f7fcc92`, `wsl2`), branch `linux-lane`, authority `lane-commit`.
  RFC-BPR-0029 base `5d5df7cd79115b9b4b2f8be6ebdb5392458b9e1d`.
- **Outcome.** The cause is a standard-library difference in the test's fake
  catalog, not ordering, timing or the product. The fix is in the test only.
  The card stays In Progress until main runs the row on macOS.
- **Changed path.** `tests/application/epoch_application_host_tests.cpp`. No
  product source, contract or pinned digest changed.
- **Logs.** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-sweep-fixes/`
  (`0855-*`, and the diagnosis in `0855-diag/`). None is committed.
- **Still open.** The macOS run under "For macOS".

## Cause

The fake catalog runs a test's commit hook by moving it out:
`auto callback = std::move(State.AfterCommitAuthorized); callback();` in
`record_candidate` (the same shape for `AfterRefCommitInProgress` in
`transition_operation`). The hooks date from `0d493961`. A moved-from
`std::function` is valid but unspecified, and the two standard libraries
differ:

- **libstdc++** (Linux, GCC 15.2) leaves the source empty, so a hook fires
  once.
- **libc++** (macOS) copies a callable that fits its small buffer and leaves
  the source armed, so a small hook such as `[&value] { ... }` fires on every
  commit. A callable too large for the buffer is moved and emptied there too.

Two cases added in `1bd5c5e8` import two historical occurrences one after the
other, and each commit's `recover_append` asks the cache for one scripted
recovery observation (`inspect_operation_recovery`,
`src/application/src/epoch_application_host.cpp` ~2361). Only the hook
supplies it. On Linux the second commit finds the queue empty, the fake
returns `invalid_request`, and the host correctly fails closed with a cache
failure (`EPOCH_CACHE_FAILURE:0:`). The second occurrence never gets a local
epoch, so the latest selection keeps the first commit.

- `selected_large_occurrence_rebases_after_superseded_commit()` (the `false`
  call) fails: its linked occurrence has no `LocalEpoch`, and the accepted
  left commit is still the first import's. Core and catalog stay
  synchronized. The `true` call reuses one occurrence and passes.
- `selected_large_occurrence_reconciles_cancel_after_commit_boundary()` fails
  the same way, but the binary exits at the first failure, so the sweep never
  showed it.

Evidence (`0855-diag/`, Linux):

| Run | Result |
|---|---|
| Unchanged binary, 3 runs (`run-existing-{1,2,3}.log`) | Fail, 3 of 3 |
| A probe of the hook at each commit (`run-probe-libstdcxx.log`) | Empty at the second commit, with 0 queued observations |
| `std::function` move probe (`function-move-probe.log`) | libstdc++ empties both a small and a large source |
| Test copy that copies the hook instead of moving it, as libc++ does (`run-probe-emulate-libcxx-commit.log`) | All cases pass |
| Clang 22 against libstdc++ (`run-probe3-clang-libstdcxx.log`) | Fails the same way, so evaluation order is not the cause |
| Both cases skipped (`run-probe3-skip-both-libstdcxx.log`) | Every other case passes |

libc++ is not installed here, so its behaviour is from its source
(`__value_func`'s move constructor clones a small-buffer callable), and is
supported by macOS passing and by the copy experiment above. On a Mac,
`0855-diag/function_move_probe.cpp` is expected to print
`stdlib=libc++ small_source_armed_after_move=1 large_source_armed_after_move=0`.

## Fix

The fake's three hook sites use `std::exchange(hook, { })`, as its projection
ingress already does, so a hook fires once on every standard library. The two
cases re-arm the same observation hook before their second import, which
scripts what macOS already ran. With the libc++ behaviour emulated on Linux,
only these two cases fired any hook twice; every other hook user fires at
most once, so no other case changes on either platform.

## Verification before commit (Linux, bpr-linux, WSL2)

Stock `dev` preset; only the test target rebuilt, with no warning.

| Run | Result | Time |
|---|---|---|
| The fixed test, registered row (`0855-fixed-uncommitted.log`) | Pass | 0.25 s |
| The fixed copy built with GCC, 3 runs, and with clang 22, 3 runs (`0855-diag/run-fix-{gcc,clang}-{1,2,3}.log`) | Pass, 6 of 6 | |
| The fixed copy with the libc++ behaviour emulated (`0855-diag/run-fix-libcxx-retained-emulation.log`) | Pass | |

Gates: `/usr/bin/clang-format` 22 and `clang-tidy` (`0855-tidy.log`) report
the whole file clean, and `git diff --check` is clean.

## For macOS

- **Still needed:** `application_epoch_host` passes on macOS with this change.
