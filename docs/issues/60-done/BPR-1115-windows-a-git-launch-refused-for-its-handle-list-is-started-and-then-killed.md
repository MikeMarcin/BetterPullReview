---
schema: "implement-rfc.card.v1"
id: "BPR-1115"
title: "Windows: a Git launch refused for its handle list is started and then killed; add committed coverage of the inheritance restriction"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"independent rfc-reviewer-critical review of BPR-1104 (claude-fable-5-1, read-only, macOS from source), findings 2 and 3","reviewed_candidate":"51784082","origin":"owner-authorized review, attended session cbd48b91000b00f1e72e1ad62c7c384c, decision ae582fb8-d480-456a-8ef5-79011030e131, 2026-09-24","reachable":"the refusal only when the attribute list cannot be created or a standard handle is NULL; the missing coverage always","related_cards":["BPR-1104","BPR-1114","BPR-1040"],"reservation":"docs/issues/_reservations/BPR-1111--BPR-1115--c042082d78b1.json"}
acceptance: [{"criterion":"AC1 A launch refused for its handle list never runs Git, and the result is still process_start. A committed row drives the refusal branch through a test seam that makes the list fail to build, and it shows that no Git process started.","outcome":"pass","revision":"0987dede","evidence":{"kind":"command","detail":"bpr-main-win, build/dev MSVC 19.51 Debug, Qt 6.11.2, Windows 11 10.0.26200. The platform launch_restriction (src/platform/include/bpr/platform/windows_standard_handles.hpp) answers a list it cannot build with inheritance off plus DETACHED_PROCESS|CREATE_NEW_CONSOLE, which CreateProcessW refuses before opening the program. git_process_contract's run_windows_refused_launch_contract: the compile-time fault seam withholds standard error; the refused index_pack returns process_start, a job the test process joins counts 0 processes created, the fake Git's invocation log is unchanged; without the marker the same request succeeds and creates 2. Printed: 'BPR-1115 refused launch: process_start, 0 processes created; the allowed launch created 2'. Negative control (station patch, restored): with the BPR-1104 shape (inheritance off only) the row fails 'creates no process at all (created 1)'. Rows ran on 86e67772 (49f4a9c5 plus this change); 0987dede is the same change rebased over commits that touch only Linux secret-service and SQLite profile-activation tests, and its gate rebuilt build/dev and passed both boundary rows.","path":"docs/issues/60-done/BPR-1115-windows-a-git-launch-refused-for-its-handle-list-is-started-and-then-killed.md#refused-at-creation-with-committed-rows-bpr-main-win-2026-09-24"}},{"criterion":"AC2 A committed Windows row holds an std::ifstream on a file, runs git cat-file --batch through an unguarded launch, and while Git runs asserts that an exclusive CreateFileW on the file succeeds. Removing the handle list fails the row (negative control).","outcome":"pass","revision":"0987dede","evidence":{"kind":"command","detail":"git_process_contract's run_windows_inherited_handle_contract holds an std::ifstream on a file while the adapter starts an unguarded index-pack of the contract's fake Git (the test executable, blocked on BPR_BLOCK; a real Git exits as soon as its closed input ends, and inheritance is decided by the launch, not the program); once the fake reports it runs, the stream closes and CreateFileW with no sharing succeeds while Git still runs. Printed: 'BPR-1115 inherited handle: the parent opened its file with no sharing while the unguarded Git ran'. Negative control (station patch, restored): with the list not attached (Qt's inherit-everything default) the row fails with error 32 ERROR_SHARING_VIOLATION.","path":"docs/issues/60-done/BPR-1115-windows-a-git-launch-refused-for-its-handle-list-is-started-and-then-killed.md#refused-at-creation-with-committed-rows-bpr-main-win-2026-09-24"}},{"criterion":"AC3 macOS and Linux keep their paths: every change is inside Q_OS_WIN, or the change is run there.","outcome":"pass","revision":"0987dede","evidence":{"kind":"freeform","detail":"Every change in behaviour is inside Q_OS_WIN/_WIN32: the platform header, the Git runner's restriction and post-start check, both rows, and the WIN32-only platform link in src/adapters/git/CMakeLists.txt. The portable parts change no behaviour: the fake Git's blocked index-pack also writes an unread block-started marker, and the boundary gate gains text patterns and two contract tests. macOS and Linux reruns of git_process_contract, git_cache_contract, architecture_boundaries and architecture_boundary_contract on the pushed revision are recorded as owed to those stations."}},{"criterion":"AC4 No regression on Windows: git_process_contract, git_cache_contract, git_sparse_cache_contract, git_protocol_boundary, native_git_smart_transport_contract, application_git_protocol_v2, architecture_boundaries, architecture_boundary_contract and msvc_secure_crt_contract.","outcome":"pass","revision":"0987dede","evidence":{"kind":"command","detail":"ctest --preset dev -j1 on the candidate: architecture_boundaries 18.77 s, architecture_boundary_contract 336.68 s, git_protocol_boundary, msvc_secure_crt_contract, application_git_protocol_v2, native_git_smart_transport_contract, git_process_contract 47.95 s, git_cache_contract 73.66 s, git_sparse_cache_contract 26.02 s: 9/9 pass. After a cast-only header change the full tree rebuilt without a warning and git_process_contract passed again (48.20 s); the push gate on 0987dede rebuilt build/dev and passed architecture_boundaries and architecture_boundary_contract. clang-format clean; clang-tidy adds no finding (header checked with an explicit header filter); its five findings in installed_git_process.cpp predate this change.","path":"docs/issues/60-done/BPR-1115-windows-a-git-launch-refused-for-its-handle-list-is-started-and-then-killed.md#refused-at-creation-with-committed-rows-bpr-main-win-2026-09-24"}}]
---

# BPR-1115: Windows: a Git launch refused for its handle list is started and then killed; add committed coverage of the inheritance restriction

## Source

[[BPR-1104]]'s independent review, 2026-09-24. The owner asked for it in
attended session `cbd48b91` (decision `ae582fb8`). The reviewer was an
`rfc-reviewer-critical` (`claude-fable-5-1`), working read-only on macOS
from source. The candidate was `51784082`. The review found no blocking
issue. This card takes findings 2 and 3. The review is recorded on BPR-1104
under "Independent review (2026-09-24)".

Line references are the reviewer's, in
`src/adapters/git/src/installed_git_process.cpp`. That file is unchanged
from `51784082` to the revision where this card was written.

## Defect

**1. "Refused" means started and then terminated (review finding 2).**
BPR-1104 says that a start which bypassed the list "inherits nothing and is
refused as `process_start`". In the code:

- When building the list fails, the catch at `:1891-1894` only clears
  `inheritHandles`. The program is still Git.
- The process starts at `:2627`.
- The refusal at `:2652-2654` comes after that start, and before
  `job.attach` (`:2675-2678`). So the child is not yet in the job object.
- `kill_process_group` is a no-op on Windows (`:1334-1349`).

So Git runs, without inherited handles, until it is terminated, and a
grandchild it started can survive. The guarded path already avoids this. When
it cannot redirect, it replaces the program with one that exits at once
(`:2036-2043`). The reviewer suggests the same pattern here, and tighter
wording. The branch is reachable only if the attribute list cannot be
created, or if a standard handle is NULL.

**2. The inheritance restriction has no committed failure-mode coverage
(review finding 3).** BPR-1104's Windows evidence is a station loop that was
not committed. Nothing in the tree fails if the handle list is removed, and
nothing drives the refusal branch.

## Scope

- In the catch at `:1891-1894`, replace the program with one that exits at
  once, as the guarded path does at `:2036-2043`, so that no Git runs on a
  refused launch. Or refuse before `start()`. Either way the result stays
  `process_start`.
- Change the code comments, and the wording on any live document, to say
  what now happens. BPR-1104 is Done and keeps its text; its review section
  already records the correction.
- Add the two rows the reviewer suggests.

## Acceptance

- AC1 A launch refused for its handle list never runs Git, and the result is
  still `process_start`. A committed row drives the refusal branch through a
  test seam that makes the list fail to build, and it shows that no Git
  process started.
- AC2 A committed Windows row holds an `std::ifstream` on a file, runs
  `git cat-file --batch` through an unguarded launch, and while Git runs
  asserts that an exclusive `CreateFileW` on the file succeeds. Removing the
  handle list fails the row (negative control).
- AC3 macOS and Linux keep their paths: every change is inside `Q_OS_WIN`,
  or the change is run there.
- AC4 No regression on Windows: `git_process_contract`, `git_cache_contract`,
  `git_sparse_cache_contract`, `git_protocol_boundary`,
  `native_git_smart_transport_contract`, `application_git_protocol_v2`,
  `architecture_boundaries`, `architecture_boundary_contract` and
  `msvc_secure_crt_contract`.

[[BPR-1114]] should use the refusal shape this card settles for the gh CLI
runner.

## Refused at creation, with committed rows (bpr-main-win, 2026-09-24)

Run `bpr-main-win-20260923`, `build/dev` (MSVC 19.51 Debug, Qt 6.11.2),
Windows 11 10.0.26200, on `49f4a9c5` plus this change. The logs are station
records under `build/logs/bpr-1115-main-win/` and are not committed.

**The shape settled here.** The list-building code moved out of
`installed_git_process.cpp` into the platform target, as
`src/platform/include/bpr/platform/windows_standard_handles.hpp`
(`launch_restriction`), so that [[BPR-1114]] can give the gh CLI runner the
same code instead of a copy. It depends on nothing first-party and includes no
Qt: an adapter hands it the three fields of its own `CreateProcessArguments`
(the startup block, the creation flags and the inheritance flag).

- On success it points the call at its own extended startup block, whose
  `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` names exactly the standard input,
  output and error handles, and turns inheritance on. That is what
  [[BPR-1104]] shipped.
- When a standard handle is null or invalid, or the list cannot be built, it
  turns inheritance off and adds `DETACHED_PROCESS | CREATE_NEW_CONSOLE`.
  Those two flags exclude each other, so `CreateProcessW` fails the call with
  `ERROR_INVALID_PARAMETER` before it opens the program. **No process is
  created**, so no Git runs, with or without handles. Qt reports
  `FailedToStart`, and the adapter's existing mapping makes that
  `process_start`.

This is "refuse before anything starts" rather than the guarded path's
"replace the program with one that exits at once". The guarded path has the
bundled supervisor to substitute. The gh CLI runner in [[BPR-1114]] has no
such program, and a substitute still creates a process. Refusing at creation
needs nothing on disk and creates nothing, and both runners can share it.

The check after the start stays. It now fires only for a child created
without the restriction all the same: a Qt that never called the modifier, or
a Windows that accepted the refused flags. Such a child is still terminated
before it is attached to the job. The guarded path is unchanged.

**The rows (AC1, AC2).** Both are Windows-only rows at the end of
`git_process_contract`. They drive the adapter through
`make_installed_git_process`, with the contract's fake Git: a copy of the
test executable, launched through the same unguarded path as every cache
operation.

- `run_windows_inherited_handle_contract` (AC2). The parent holds an
  `std::ifstream` on a file while the adapter starts an `index-pack`. The
  fake blocks in it. Once the fake reports that it is running, the parent
  closes the stream and opens the file with `CreateFileW` and no sharing.
  That open succeeds while Git still runs.
- `run_windows_refused_launch_contract` (AC1). The contract's
  compile-time fault seam (`BPR_INSTALLED_GIT_PROCESS_FAULT_INJECTION`, a
  marker in the isolation root) withholds the standard error handle, as a
  null standard handle would, so the list cannot be built. The row counts
  processes with a job object that this test process joins, so every process
  it creates afterwards is counted, even after it has exited. The refused
  `index_pack` returns `process_start`, **0 processes are created**, and the
  fake's invocation log is unchanged. Without the marker, the same request
  succeeds and creates 2 processes. So the count can see a launch.

```text
BPR-1115 inherited handle: the parent opened its file with no sharing while the unguarded Git ran
BPR-1115 refused launch: process_start, 0 processes created; the allowed launch created 2
Git process contract passed
```

AC2 names `git cat-file --batch`. The row runs the fake Git's blocked
`index-pack` instead. A real Git exits as soon as the adapter closes its
input, so it never stays alive long enough to look at what it holds. What a
child inherits is decided by the launch, not by the program, and the fake is
started by exactly the same launch code.

**Negative controls.** These are station patches to the platform header,
rebuilt and run, then restored. They were never committed.

| Patch | Result |
| --- | --- |
| The list is not attached. The restriction reports success but leaves Qt's inherit-everything default in place (the list removed). | `git_process_contract` fails at the inherited-handle row: the exclusive open is refused with error 32, `ERROR_SHARING_VIOLATION`. |
| The refusal only turns inheritance off, which is the shape [[BPR-1104]] shipped. | The inherited-handle row passes. The refusal row fails with "creates no process at all (created 1)": the child is created, and is then killed by the check after the start. |

Each negative control run left its temporary root behind, because `require`
exits the process. Both roots were removed by their exact names.

**AC3.** macOS and Linux keep their paths. Every change in behaviour is
inside `Q_OS_WIN` or `_WIN32`: the platform header, the Git runner's
restriction and its post-start check, the two rows, and the Windows-only
platform link in `src/adapters/git/CMakeLists.txt`. The rest is portable,
but it changes no behaviour there:

- The fake Git's blocked `index-pack` now also writes a `block-started`
  marker. Nothing else reads it.
- `architecture_boundaries` gains required patterns, which are checked as
  text on every host.
- `architecture_boundary_contract` gains two tests.

The macOS and Linux stations should rerun `git_process_contract`,
`git_cache_contract`, `architecture_boundaries` and
`architecture_boundary_contract` on the pushed revision. This station did not
run them there.

**AC4.** On the candidate, serially: `architecture_boundaries` (18.77 s),
`architecture_boundary_contract` (336.68 s), `git_protocol_boundary`,
`msvc_secure_crt_contract`, `application_git_protocol_v2`,
`native_git_smart_transport_contract`, `git_process_contract` (47.95 s),
`git_cache_contract` (73.66 s) and `git_sparse_cache_contract` (26.02 s):
9 of 9 pass. After that run, the platform header's two casts were rewritten
in the form the Git runner's own list used before, which clang-tidy accepts.
The full tree was rebuilt without a warning, and `git_process_contract` was
rerun: it passes (48.20 s), with the same two lines. `clang-format` is clean. `clang-tidy` finds nothing in the new code,
including the platform header, which was checked with a header filter the
repository's `/`-only `HeaderFilterRegex` does not reach on Windows. Its five
findings in `installed_git_process.cpp` predate this change.

**Baseline, for the record.** Before this change, at `877e3d60`,
`git_process_contract`, `gh_cli_transport` and `msvc_secure_crt_contract`
passed. `github_cli_process_runner` failed at "opt-in preserves only exact
bounded stdout and completed exit code", which is [[BPR-0878]]'s item 3 and
is taken up under [[BPR-1114]].

**Boundaries.** `architecture_boundaries` now allows the handle-list APIs
in the platform header, alongside the Git runner and the two agent sources.
It requires the Git runner to include and apply the restriction and to keep
the check after the start. `architecture_boundary_contract` checks that
removing any of those pieces fails the gate, that the same list-building
code anywhere else is refused, and that the platform target must own the
header. `AGENTS.md`, `docs/architecture/system-architecture.md`,
`docs/architecture/git-and-epoch-model.md` and the platform target's
`CMakeLists.txt` describe the new definition.
