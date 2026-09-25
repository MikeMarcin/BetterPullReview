---
schema: "implement-rfc.card.v1"
id: "BPR-1114"
title: "Windows: the gh CLI process runner launches gh and git with every inheritable handle"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"independent rfc-reviewer-critical review of BPR-1104 (claude-fable-5-1, read-only, macOS from source), finding 1","reviewed_candidate":"51784082","origin":"owner-authorized review, attended session cbd48b91000b00f1e72e1ad62c7c384c, decision ae582fb8-d480-456a-8ef5-79011030e131, 2026-09-24","related_cards":["BPR-1104","BPR-1115","BPR-1040"],"reservation":"docs/issues/_reservations/BPR-1111--BPR-1115--c042082d78b1.json"}
acceptance: [{"criterion":"AC1 Every Windows launch through the gh CLI process runner inherits only its standard handles. The code shows it, and AC2 pins it.","outcome":"pass","revision":"c636eef2","evidence":{"kind":"observation","detail":"github_cli_process_runner.cpp: every launch goes through the one execute_process, which on Windows installs one setCreateProcessArgumentsModifier handing the platform launch_restriction (src/platform/include/bpr/platform/windows_standard_handles.hpp, shared with the Git runner, BPR-1115) the startup block, creation flags and inheritance flag; a list that cannot be built is refused at creation (start_failure, no process); a child started without the restriction is refused after the start. The GitHub adapter links BetterPullReview::Platform on Windows; no adapter-to-adapter edge. architecture_boundaries pins exactly that one modifier (over code alone) plus the include, object and post-start refusal, and refuses any other modifier; architecture_boundary_contract covers removal, doubling, comment-only, widened bodies and a second modifier. test_windows_spooled_launch_keeps_its_file shows a file-redirected (Git-acquisition spool) launch starts under the restriction and writes 7 bytes.","path":"docs/issues/60-done/BPR-1114-windows-the-gh-cli-process-runner-launches-gh-and-git-with-every-inheritable-handle.md#every-windows-launch-restricted-with-committed-rows-bpr-main-win-2026-09-24"}},{"criterion":"AC2 A committed Windows row holds an inheritable handle on a file, such as an std::ifstream. It starts a long-lived child through the runner, and while the child runs it asserts that an exclusive CreateFileW on that file succeeds. Removing the list fails the row (negative control).","outcome":"pass","revision":"c636eef2","evidence":{"kind":"command","detail":"bpr-main-win, build/dev MSVC 19.51 Debug, Qt 6.11.2, Windows 11 10.0.26200. github_cli_process_runner's test_windows_launch_inherits_only_standard_handles (production factory): the parent holds an std::ifstream while the runner starts the helper's hold child; once it reports running the stream closes and CreateFileW with no sharing succeeds while the child runs. Printed 'BPR-1114 inherited handle: the parent opened its file with no sharing while the runner's child ran'. Negative control (station patch, restored): with no list attached the row fails with error 32 ERROR_SHARING_VIOLATION. The refusal row (seam): start_failure with 0 processes created by job accounting, 2 when allowed; with the inheritance-off-only refusal it fails with 'created 1'.","path":"docs/issues/60-done/BPR-1114-windows-the-gh-cli-process-runner-launches-gh-and-git-with-every-inheritable-handle.md#every-windows-launch-restricted-with-committed-rows-bpr-main-win-2026-09-24"}},{"criterion":"AC3 The Unix launch is unchanged, or the change is run on macOS and Linux.","outcome":"pass","revision":"c636eef2","evidence":{"kind":"freeform","detail":"The Unix launch (setUnixProcessParameters with CloseFileDescriptors, and the RLIMIT_FSIZE modifier) is untouched; Unix already closes every non-standard descriptor. The modifier, restriction object, post-start check, the helper's binary mode and the three rows are inside Q_OS_WIN/_WIN32. Portable edits are inert on Unix: the seam's new virtual defaults to false, execute_process takes the flag [[maybe_unused]], the helper's hold mode is used only by a Windows row, and the boundary gate gains text patterns and a contract test. macOS/Linux reruns of github_cli_process_runner, gh_cli_transport, gh_cli_git_smart_transport, architecture_boundaries and architecture_boundary_contract on the pushed revision are recorded as owed to those stations."}},{"criterion":"AC4 No regression on Windows: the gh CLI runner's contract and transport rows, architecture_boundaries, architecture_boundary_contract and msvc_secure_crt_contract.","outcome":"pass","revision":"c636eef2","evidence":{"kind":"command","detail":"ctest --preset dev -j1 on the candidate (45191097 plus this change): architecture_boundaries 18.98 s, architecture_boundary_contract 362.46 s, msvc_secure_crt_contract, gh_cli_transport, github_cli_process_runner, github_provider 23.57 s, github_capability_probe 12.79 s and application_public_path_ghes_cli_unavailable pass. github_cli_process_runner was red at 877e3d60 (BPR-0878 item 3, the helper's text-mode \\r\\n); the helper now writes binary and a text-mode control fails the row again. application_device_flow_cli_choice also failed, identically with the GitHub adapter at 45191097 (BPR-0803's mac-only course; since 89907c1f registered on macOS only). After the spool row was added: rebuilt, github_cli_process_runner 1.57 s, gh_cli_transport and architecture_boundaries pass; the push gate on c636eef2 rebuilt build/dev and passed architecture_boundaries and architecture_boundary_contract. clang-format clean; clang-tidy nothing in the runner or platform header (two pre-existing naming findings in the test's exact_environment_matches).","path":"docs/issues/60-done/BPR-1114-windows-the-gh-cli-process-runner-launches-gh-and-git-with-every-inheritable-handle.md#every-windows-launch-restricted-with-committed-rows-bpr-main-win-2026-09-24"}}]
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
  so the one-account ending cannot sign in on Windows. While this card was
  being verified, `89907c1f` registered the course on macOS only, so the row
  no longer runs here and needs no card of its own. Both of its failing runs
  left a temporary directory, and both were removed by exact name.

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

## Post-close: Linux at `3c3c34d8` (bpr-main-linux, 2026-09-24)

**Four of the five reruns owed from Linux pass. The fifth,
`gh_cli_git_smart_transport`, is not registered on Linux.** It sits inside
`if(APPLE)` in `tests/github_contract/CMakeLists.txt`, so it is macOS only
and no other row was run in its place. macOS was still owed then. It was run
at `d01f506c`, below.

The run was in the station's main checkout, with a clean tree, at
`3c3c34d8`, which contains `c636eef2`. WSL2 Ubuntu 26.04, GCC 15.2, Qt
6.11.2, `build/dev` (Debug, `-Werror`), rebuilt at `3c3c34d8` with no
`warning:` line. `ctest --test-dir build/dev -j1 --output-on-failure`,
offscreen, with `DISPLAY`, `WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS`
unset and fresh absolute `HOME` and XDG roots. The names were resolved with
`ctest -N`.

| Row | Result |
| --- | --- |
| `github_cli_process_runner` | pass, 1.49 s |
| `gh_cli_transport` | pass, 0.03 s |
| `gh_cli_git_smart_transport` | not registered on Linux |
| `architecture_boundaries` | pass, 12.48 s |
| `architecture_boundary_contract` | pass, 172.42 s |

The three new rows in `github_cli_process_runner` are compiled only under
`_WIN32`. So on Linux the runner's row covers the unchanged Unix launch and
the portable seam, whose new `fail_standard_handle_list()` defaults to
false. The boundary rows read the source text, so they check the runner's
one pinned modifier on this host too. The runs added nothing to `/tmp` and
no `bpr-` name to `/run/user/1000`.

The acceptance record above is unchanged. The logs are station records under
`build/logs/bpr-main-linux-20260924/batch7-linux/` and are not committed:
`bpr-1114-1115-rows.log` and `build-dev-3c3c34d8.log`.

## macOS at `d01f506c` (bpr-main-mac, Mac batch 8, 2026-09-24)

The owed macOS runs pass, and the runner's macOS launch did not change. The
head was `d01f506c`, which contains `c636eef2` and `3c3c34d8`. Apple Silicon,
macOS 26.6.2, AppleClang, Qt 6.11.2, `build/dev` Debug, rebuilt at that head
with no `warning:` or `error:` line. The command was
`ctest --test-dir build/dev -j1 --output-on-failure -V -R "^(github_cli_process_runner|gh_cli_transport|gh_cli_git_smart_transport|architecture_boundaries|architecture_boundary_contract|...)$"`,
run once together with BPR-1115's rows.

| Row | Result |
| --- | --- |
| `github_cli_process_runner` | Passed, 3.87 s (`GitHub CLI process runner tests passed`) |
| `gh_cli_transport` | Passed, 0.58 s |
| `gh_cli_git_smart_transport` | Passed, 11.89 s |
| `architecture_boundaries` | Passed, 20.17 s |
| `architecture_boundary_contract` | Passed, 107.36 s |

**Nothing changed in the macOS launch.** `github_cli_process_runner.cpp` was
preprocessed with its macOS compile command, at `c636eef2^` and at
`d01f506c`. The outputs differ only in the portable seam the card describes:

- the new virtual `fail_standard_handle_list()`, which answers `false`;
- `execute_process`'s extra `[[maybe_unused]] bool` parameter;
- the one call that passes the seam's answer to it.

The Unix launch (`setUnixProcessParameters`, the `RLIMIT_FSIZE` modifier) is
unchanged.

With the Linux section above, the reruns AC3 recorded as owed are all run.
