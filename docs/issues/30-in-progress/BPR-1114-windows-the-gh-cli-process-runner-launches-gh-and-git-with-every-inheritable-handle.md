---
schema: "implement-rfc.card.v1"
id: "BPR-1114"
title: "Windows: the gh CLI process runner launches gh and git with every inheritable handle"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"independent rfc-reviewer-critical review of BPR-1104 (claude-fable-5-1, read-only, macOS from source), finding 1","reviewed_candidate":"51784082","origin":"owner-authorized review, attended session cbd48b91000b00f1e72e1ad62c7c384c, decision ae582fb8-d480-456a-8ef5-79011030e131, 2026-09-24","related_cards":["BPR-1104","BPR-1115","BPR-1040"],"reservation":"docs/issues/_reservations/BPR-1111--BPR-1115--c042082d78b1.json"}
---

# BPR-1114: Windows: the gh CLI process runner launches gh and git with every inheritable handle

## Source

[[BPR-1104]]'s independent review, 2026-09-24. The owner asked for it in
attended session `cbd48b91` (decision `ae582fb8`). The reviewer was an
`rfc-reviewer-critical` (`claude-fable-5-1`), working read-only on macOS
from source. The candidate was `51784082`. The review found no blocking
issue. It found BPR-1104's handle list correct: it fails closed with respect
to handles, and the retry is idempotent and bounded. This card takes finding
1. The review is recorded on BPR-1104 under "Independent review
(2026-09-24)".

## Defect

BPR-1104 found that a Git child held another operation's pack spool. The
child had inherited the handle, because unguarded Windows Git launches used
Qt's default of inheriting every inheritable handle, and MSVC C runtime
streams are inheritable. BPR-1104 fixed that in the Git adapter's
`installed_git_process.cpp`, with `standard_handles_only`.

The GitHub adapter has a second launcher that was not changed.
`src/adapters/github/src/github_cli_process_runner.cpp:584-611` launches `gh`
and `git` on Windows with Qt's inherit-all default. It installs no
`setCreateProcessArgumentsModifier`. On Unix the same launch sets
`CloseFileDescriptors` (`:606-610`). The file is unchanged from `51784082` to
the revision where this card was written.

So `gh version`, `git --version` and `gh api` reads can still inherit, and
hold, a spool handle that another thread has open at that moment. That
reproduces BPR-1104's second cause. It is pre-existing. The reviewer's fix is
to apply the same handle list.

**Why P2.** It is the same fault that BPR-1104 measured at 3 of 80 seeds for
the Git adapter, before the list was added there. How often it happens
through this runner has not been measured.

## Scope

- Give every Windows launch in the gh CLI process runner a
  `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` of its standard handles, or no
  inheritance at all, as BPR-1104 did for unguarded Git.
- Share the mechanism if the architecture allows it, rather than copying it.
  Both files are adapters, and neither may depend on the other's provider
  types. If the two copies must stay separate, say why and keep them
  identical in behaviour.
- A launch that cannot build its list must not run the child with
  inheritance. Use the refusal shape that [[BPR-1115]] settles, and do not
  start then kill the child.

## Acceptance

- AC1 Every Windows launch through the gh CLI process runner inherits only
  its standard handles. The code shows it, and AC2 pins it.
- AC2 A committed Windows row holds an inheritable handle on a file, such as
  an `std::ifstream`. It starts a long-lived child through the runner, and
  while the child runs it asserts that an exclusive `CreateFileW` on that
  file succeeds. Removing the list fails the row (negative control).
- AC3 The Unix launch is unchanged, or the change is run on macOS and Linux.
- AC4 No regression on Windows: the gh CLI runner's contract and transport
  rows, `architecture_boundaries`, `architecture_boundary_contract` and
  `msvc_secure_crt_contract`.

## Every Windows launch restricted, with committed rows (bpr-main-win, 2026-09-24)

Run `bpr-main-win-20260923`, `build/dev` (MSVC 19.51 Debug, Qt 6.11.2),
Windows 11 10.0.26200, on `45191097` plus this change. The logs are station
records under `build/logs/bpr-1114-main-win/` and are not committed.

**One mechanism, shared (AC1).** `github_cli_process_runner.cpp` takes the
platform target's `launch_restriction`, the same one [[BPR-1115]] gave the
Git adapter's unguarded Git
(`src/platform/include/bpr/platform/windows_standard_handles.hpp`). There
is no second copy, and no adapter-to-adapter edge. On Windows the GitHub
adapter links `BetterPullReview::Platform`, a header-only target that
depends on nothing first-party. Every launch the runner makes goes through
the one `execute_process`, and so gets it:

- **gh and Git inherit their three standard handles and nothing else.** That
  covers `gh --version`, every `gh api`, and the local Git of the Git-object
  path. The runner's one `setCreateProcessArgumentsModifier` hands the
  restriction the startup block, the creation flags and the inheritance
  flag. A standard handle may be a pipe, or the Git-acquisition spool file
  Qt opens for standard output. The list names whichever it is.
- **A launch whose list cannot be built is refused at creation**, by the
  shape [[BPR-1115]] settled. Inheritance is turned off, and the refused flag
  pair makes `CreateProcessW` fail before it opens the program. Nothing
  starts, and the run fails as `start_failure`.
- **A child created without the restriction all the same is refused after
  the start**, as `start_failure`. This is the same check the Git runner
  keeps, so the two launchers behave identically.

**The seam.** `github_cli_process_runner_test_seam` gains
`fail_standard_handle_list()`. It returns false by default, and it is
consulted once per execution, after `before_execute`. A seam that returns
true makes the runner withhold the launch's standard error handle, as a null
standard handle would, so the list cannot be built. Only
`make_github_cli_process_runner_for_test` accepts a seam. The production
factories pass none, as `architecture_boundaries` already requires.

**The rows (AC2, and the refusal).** They are Windows-only rows at the end of
`github_cli_process_runner`. The child is the test executable in its helper
mode, as for every row of this suite. No real `gh` runs, and nothing reaches
GitHub.

- `test_windows_launch_inherits_only_standard_handles` (AC2) uses the
  production factory. The parent holds an `std::ifstream` on a file while
  the runner starts a `hold` child, which reports that it is running and
  then stays alive. The stream then closes, and `CreateFileW` opens the file
  with no sharing. That open succeeds while the child still runs.
- `test_windows_spooled_launch_keeps_its_file` uses the Git-acquisition
  factory. The child's standard output is a new spool file inside the
  store. The child starts under the restriction and writes its 7 bytes into
  the spool.
- `test_windows_refused_launch_creates_nothing` uses the seam. It counts
  processes with a job that the test process joins, as in [[BPR-1115]]. The
  refused `touch` child returns `start_failure`. **0 processes are
  created**, and the child's marker is absent. With the seam off, the same
  launch succeeds, creates 2 processes and writes its marker.

```text
BPR-1114 inherited handle: the parent opened its file with no sharing while the runner's child ran
BPR-1114 spooled launch: the child wrote 7 bytes into its spool
BPR-1114 refused launch: start_failure, 0 processes created; the allowed launch created 2
GitHub CLI process runner tests passed
```

**Negative controls.** Each is a station patch, rebuilt and run, then
restored. None was committed.

| Patch | Result |
| --- | --- |
| The platform restriction attaches no list (Qt's inherit-everything default) | `github_cli_process_runner` fails at the inherited-handle row: the exclusive open is refused with error 32, `ERROR_SHARING_VIOLATION`. |
| The refusal only turns inheritance off, the shape [[BPR-1104]] shipped | The inherited-handle row passes. The refusal row fails with "creates no process at all (created 1)". |
| The helper is left in the C runtime's text mode (below) | The row fails at "opt-in preserves only exact bounded stdout and completed exit code", as it did at `877e3d60`. |

Each failing run left its temporary directory behind, because `expect`
exits the process. So did the baseline run's failure. All four were removed
by exact name.

**A pre-existing failure, fixed here.** At `877e3d60`,
`github_cli_process_runner` already failed on Windows at "opt-in preserves
only exact bounded stdout and completed exit code". That is [[BPR-0878]]'s
item 3. The helper wrote its `\r\n` response through a C runtime stream in
text mode, which writes each `\n` as `\r\n`, so the exact bytes the row
expects could not arrive. The helper now sets binary mode on Windows, as the
Git contract's fake Git does. Without this fix, the new rows could not run,
because the suite stops at its first failure. The control above shows the
cause, and [[BPR-0878]] records item 3 as accounted for.

**AC3.** The Unix launch is unchanged. `setUnixProcessParameters` with
`CloseFileDescriptors`, and the `RLIMIT_FSIZE` modifier, are untouched, and
Unix already closes every descriptor except the standard three. The
modifier, the restriction object, the post-start check, the helper's binary
mode and the three rows are all inside `Q_OS_WIN` or `_WIN32`. The portable
edits change nothing on Unix:

- the seam's new virtual defaults to false, and no Unix seam overrides it;
- `execute_process` takes the flag as `[[maybe_unused]]`;
- the helper's `hold` mode is used only by a Windows row;
- the boundary gate gains text patterns and a contract test.

The macOS and Linux stations should rerun `github_cli_process_runner`,
`gh_cli_transport`, `gh_cli_git_smart_transport` where it is registered,
`architecture_boundaries` and `architecture_boundary_contract` on the pushed
revision. This station did not run them there.

**AC4.** The candidate was run serially, before the spool row was added:

- 8 of 9 selected rows pass: `architecture_boundaries` (18.98 s),
  `architecture_boundary_contract` (362.46 s), `msvc_secure_crt_contract`,
  `gh_cli_transport`, `github_cli_process_runner`, `github_provider`
  (23.57 s), `github_capability_probe` (12.79 s) and
  `application_public_path_ghes_cli_unavailable`.
- The ninth, `application_device_flow_cli_choice`, fails at "one discovered
  account connects at once and signs in". This change does not cause it:
  with the GitHub adapter set back to `45191097` and that target rebuilt, it
  fails at the same line. It is [[BPR-0803]]'s course, which is `mac`-only.
  On other platforms `cli_read_offered_here()` is false in a testing build,
  so the one-account ending cannot sign in on Windows. It is reported for a
  card of its own. Both of its failing runs left a temporary directory, and
  both were removed by exact name.

After the spool row was added, the tree was rebuilt:

- `github_cli_process_runner` (1.57 s), `gh_cli_transport` and
  `architecture_boundaries` pass again. The printed lines are above.
- `clang-format` is clean.
- `clang-tidy` finds nothing in the runner or in the platform header, which
  was checked with an explicit header filter. In the test file it reports
  two naming findings, in `exact_environment_matches`, a function this change
  did not touch.

**Boundaries.** `architecture_boundaries` no longer forbids every
process-creation modifier in the runner. Instead:

- over code alone, it pins exactly one modifier, the platform restriction
  statement, whole;
- it requires the platform include, the restriction object and the
  post-start refusal;
- it still refuses any other modifier, as a raw-command surface.

`architecture_boundary_contract` covers each case:

- the restriction removed, doubled, left only in a comment, withholding
  unconditionally, replacing the program, or forcing inheritance;
- the object or the post-start refusal removed;
- the header missing;
- a second, different modifier.

The GitHub adapter's `AGENTS.md`, `github-host-implementation.md`,
`github-provider-contract.md` and `system-architecture.md` describe the
restriction.
