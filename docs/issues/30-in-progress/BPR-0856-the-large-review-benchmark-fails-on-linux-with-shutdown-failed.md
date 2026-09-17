---
schema: "implement-rfc.card.v1"
id: "BPR-0856"
title: "application_large_review_benchmark fails on Linux with shutdown failed"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-0851-sweep full ctest sweep","found_at":"a1864fa296ce3450ce0e0470f36bd0d95cd59470","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0856: application_large_review_benchmark fails on Linux with shutdown failed

## Defect

`application_large_review_benchmark` fails on `bpr-linux` at `a1864fa2` with
`large-review-benchmark-failed: shutdown failed`. It fails in 5.59 s in the
sweep, where it runs serially, and in 5.56 s alone
(`docs/verification/dsp4-linux-confirmation.md`).

On macOS it passes: 21.85 s at `20d99914`
(`build/r2-logs/dsp4-linux-sweep-mac-classify.log`).

The related `application_unavailable_diff_public_path` fails on both
platforms with a different message. It is carded separately.

## Scope

- Name what fails to shut down on Linux, and whether it is the benchmark's
  harness or the product's shutdown. See [[BPR-0847]] and [[BPR-0849]], where
  a shared `/tmp` parent made an owner's workspace recovery refuse and its
  drain exit 1.
- Fix the cause.

## Acceptance

- The row passes on Linux 3 runs in a row, still passes on macOS, and the
  cause is named here.

## Current frontier (dsp-004, bpr-linux, task-20260917-lnx-sweep-fixes)

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-004` (station
  dispatch `bpr-linux/int-11`), mandate r2. Station `bpr-linux`
  (`stn-3f7fcc92`, `wsl2`), branch `linux-lane`, authority `lane-commit`.
  The lane started at `e794da856e0af2cfb2c5f234138a2f606ffe9cb7`.
- **Outcome.** The cause is named below, and the fix is in the benchmark
  harness only: `b841dfb5365c852365640009566014eb22a13727` on `linux-lane`.
  The row passes 3 runs in a row on Linux at that pushed commit (see
  "Verification at the commit"). The card stays In Progress until main runs
  the benchmark's rows on macOS.
- **Changed path.** `tests/end_to_end/large_review_benchmark.cpp`. No product
  source and no pinned digest changed.
- **Logs.** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-sweep-fixes/`
  (`0856-*`). None is committed.
- **Still open.** The macOS runs under "For macOS".

## Cause

The benchmark's application data root was a bare `QTemporaryDir`, which is
made in `QDir::tempPath()`. Linux's network sandbox sets no `TMPDIR`, so that
is shared `/tmp` (`drwxrwxrwt root`). This is [[BPR-0847]]'s and
[[BPR-0849]]'s cause in the C++ harness:

- The application keeps its agent workspace neighbourhood beside its data
  root, as `<root>-agent-workspaces-v1` (`src/app/src/github_application.cpp`
  ~4827).
- Recovery prepares that neighbourhood only under a parent this user owns and
  nobody else can write (`workspace_parent_trusted`,
  `src/adapters/git/src/installed_git_process.cpp` ~3413). `/tmp` is neither,
  so recovery returns `repair_required`.
- That sets `AgentWorkspaceCleanupFailed` (~4908), which becomes the profile's
  `CleanupFailed` (~4189) and `status().ShutdownCleanupFailed`. The benchmark
  requires that flag to be false: `shutdown failed`.

On macOS, `scripts/run_network_isolated_test.py` sets `TMPDIR` to a private
`0700` fixture folder, so the parent is trusted there and the row passes.

What discriminated it, at `e794da85` with the unchanged binary:

| Run | Result | Time |
|---|---|---|
| Registered row, shared `/tmp` (`0856-repro-shared-tmp.log`) | Fail, `shutdown failed` | 5.26 s |
| Same row with `TMPDIR` set to a private `0700` folder (`0856-repro-private-tmpdir.log`) | Pass, `LARGE_REVIEW_COMPOSED_BENCHMARK_OK` | 5.37 s |

After the private run, `better_pull_review_large_review-XXXXXX-agent-workspaces-v1`
was left in the private folder: nothing removed the neighbourhood.

`application_unavailable_diff_public_path` runs the same binary with
`--unavailable`, and does not share this cause. With a private `TMPDIR`, and
again with the fix, it still fails before shutdown with
`real host result must exceed the retained comparison limit`
(`0856-unavailable-private-tmpdir.log`, `0856-unavailable-after-fix.log`).
That row is outside this card.

## Fix

Off Windows, the data root is now `<QTemporaryDir>/data`, and
`adapters::sqlite::make_storage_root_owner_private` narrows both the temporary
folder and the data root. This is the shape [[BPR-0812]] gave the agent
courses (`tests/end_to_end/agent_chat_composition_public_path.cpp` ~251). The
neighbourhood's parent is the temporary folder, which this user owns with mode
`0700`, and the neighbourhood is removed with that folder. A narrowing
failure stops the run by name
(`large-review-benchmark-failed: private data root parent`).

- **Unchanged.** The `shutdown failed` check and every other check, the
  registrations, and the `--history-session-race` course, which returns
  before the data root is made.
- **Windows.** The data root is the `QTemporaryDir` itself, as before; the
  only change there is that it is passed as a named `std::filesystem::path`
  built from the same string.
- **macOS.** The data root moves one level down, to `/data` inside the
  temporary folder under the runner's resolved `TMPDIR`. No socket lives under
  this root.

## Verification before commit (Linux, bpr-linux, WSL2)

Stock `dev` preset at `e794da85`, then only the benchmark target rebuilt. The
Secret Service `login` collection read `(<false>,)` before the group.

| Run | Result | Time |
|---|---|---|
| Fixed binary, registered row (`0856-fixed-uncommitted.log`) | Pass, `LARGE_REVIEW_COMPOSED_BENCHMARK_OK` | 5.37 s |

`/tmp` listed the same entries before and after the run.

Gates: `/usr/bin/clang-format` 22 reports the whole file clean, and
`git diff --check` is clean. `clang-tidy` reports 20 findings in this file,
all on lines the change does not touch (`0856-tidy.log`).

## Verification at the commit (Linux, bpr-linux, WSL2)

At `b841dfb5365c852365640009566014eb22a13727`, pushed and equal to
`origin/linux-lane`, a clean tree; `ninja -n` had no work to do. The Secret
Service `login` collection read `(<false>,)` before the group. Each run was a
serial `ctest --test-dir build/dev -j1 -V -R '^application_large_review_benchmark$'`,
from 17:35:26Z, with a 1-minute load average of 0.15 to 0.35
(`0856-at-commit.summary`, `0856-at-commit-run<n>.log`).

| Run | Result | Time |
|---|---|---|
| 1 | Pass, `LARGE_REVIEW_COMPOSED_BENCHMARK_OK` | 5.53 s |
| 2 | Pass, `LARGE_REVIEW_COMPOSED_BENCHMARK_OK` | 5.44 s |
| 3 | Pass, `LARGE_REVIEW_COMPOSED_BENCHMARK_OK` | 5.26 s |

`/tmp` listed the same entries before and after the three runs.

## Confirmation at the lane tip

At `fecf04d2ebc6a3a0773c8cdbf1f24c5c2dd87f5e`, the lane tip this station
pushed, after [[BPR-0855]]'s and [[BPR-0854]]'s changes and a reconfigure, the
row was run three more times from 18:31Z: pass in 5.59 s, 5.49 s and 5.52 s,
each printing `LARGE_REVIEW_COMPOSED_BENCHMARK_OK` (`final-0855-0856.summary`,
`final-0856-run<n>.log`). `/tmp` listed the same entries before and after.

## Independent review

An `rfc-reviewer` reviewed the diff (`0856-candidate.diff`, sha256
`8a6f55970c42c50ce9553434064a18b6b02818204ecde38fe1218ba072450f44`) and found
no blocking finding. It traced the cause chain above, confirmed that Windows
passes the same path value, that the macOS courses of this binary read the
data root only through the fixture options, that the application is
destroyed before the temporary folder is removed, and that no check is
weakened. It also ran the fixed binary three times through the sandbox
wrapper: all passed, and `/tmp` did not change. It could not run macOS.

## For macOS

- **Still needed:** `application_large_review_benchmark` passes on macOS with
  this change. The Apple-only rows of the same binary
  (`application_large_review_cache_bytes` and the others under `if(APPLE)`)
  take the same data-root path, so running them is the direct check.
