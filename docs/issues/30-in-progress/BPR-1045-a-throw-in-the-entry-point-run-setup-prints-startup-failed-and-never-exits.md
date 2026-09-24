---
schema: "implement-rfc.card.v1"
id: "BPR-1045"
title: "A throw in the entry point's run setup prints STARTUP_FAILED and never exits"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-main-linux, BPR-0882's entry-point-run-setup probe","found_at":"fe2216e3","reproduced_at":["fe2216e3","8e080bbc"],"code_rechecked_at":"78c8ac85","related_cards":["BPR-0882"],"run":"bpr-main-linux-20260923","reservation":"docs/issues/_reservations/BPR-1045--BPR-1045--81a0934d27bc.json"}
---

# BPR-1045: A throw in the entry point's run setup prints STARTUP_FAILED and never exits

## Defect

The desktop's startup completion in `src/app/main.cpp` wraps the entry
point's own run setup in a `try`. That setup is `application->show()` and
the scheduling of the automated run. When it throws, the catch prints
`STARTUP_FAILED` and calls `desktop->quit(EXIT_FAILURE)`. The process then
keeps running until something kills it.

[[BPR-0882]] found this with a temporary probe. The probe threw from that
`try`, and the self-test then ran through the network sandbox with the
automated-test marker. Its logs are under `build/logs/bpr-0882/probes/` on
bpr-main-linux and are not committed:

| Probe | stdout | Exit |
| --- | --- | --- |
| `entry.*`, with BPR-0882's change | `AUTOMATED_PROFILE_PRIVATE_OK`, `STARTUP_FAILED` | killed by `timeout` at 30 s (124; the runner saw `-15`) |
| `entry-head-catch.*`, the catch exactly as it was at `fe2216e3` | the same | the same |

Every other probed arm exited 1 at once. Those arms all refuse through the
startup result, so the completion takes its failure branch. The
window-wiring arm does so with a window already built, and it still exits.
This arm is different: it fails after the completion has accepted the
startup. It is older than [[BPR-0882]], which only added the
`STARTUP_FAILURE_SOURCE:entry-point-run-setup` line on stderr.

## Why it does not exit (read from the code at `78c8ac85`)

This section is reasoned from the code. None of it was traced in a run.

- `profile_desktop::implementation::quit` (`src/app/src/profile_desktop.cpp`)
  only calls `force_shutdown` when there is no profile-management host. Here
  there is one, so it calls `Host->request_quit()` and discards the result.
  The header states that behaviour: "failed admission leaves windows usable".
- `profile_management_host`'s `request_quit`
  (`src/application/src/profile_management_host.cpp`) refuses with `busy`
  while the registry is not quiescent or the host is delivering. The
  completion runs right after the initial window opened, so one of those
  is the likely refusal. Which one was not measured.
- Nothing retries the refused quit. The window stays open, so the desktop
  never becomes quiescent, and `main`'s 10 ms poll never calls
  `QCoreApplication::exit`.

The desktop already handles the same situation in one place.
`quit_empty_desktop` retries a refused orderly quit on each poll, and after
`empty_desktop_quit_attempts` it forces the shutdown. The completion's quit
has no such retry.

**A sibling arm, not measured.** In the same completion,
`AUTOMATED_PROFILE_PRIVATE_FAILED` also calls `desktop->quit(EXIT_FAILURE)`
after a successful startup, with the window open. It probably behaves the
same way.

**Still current.** Since [[BPR-0882]]'s candidate `8e080bbc`, the only change
to `main.cpp`, `profile_desktop.cpp` or `profile_management_host.cpp` is
`3727a762`. That change adds a device-refresh counter and does not touch
this path.

## Impact

A shipped build reaches this arm only if `show()` throws. The person would
then see a window that looks open while the startup has already reported
failure. Automated courses pay the larger cost. A course that reaches this
arm prints its verdict and then holds its row until the harness timeout,
and that timeout shows up as the row's result instead of the failure.

## Scope

- Make a failure after the window exists end the process with
  `EXIT_FAILURE` in bounded time. Options include retrying the orderly quit
  on the desktop poll and then forcing the shutdown, as `quit_empty_desktop`
  does, or forcing the shutdown directly, since the startup has already
  failed. The implementer chooses and records the reason.
- Check the `AUTOMATED_PROFILE_PRIVATE_FAILED` arm and either fix it in the
  same change or record why it differs.
- `main.cpp` and `profile_desktop.cpp` are pinned composition-contract code.
  Reseal the pins, record the review here, and run the contract rows.
- Do not add a test-only seam just to make run setup throw. If a regression
  row cannot be built on existing seams, prove the fix with a temporary
  uncommitted probe, as [[BPR-0882]] did, and say so here.

## Acceptance

- AC1 A throw in the entry point's run setup exits 1 within a few seconds,
  prints `STARTUP_FAILED` on stdout and its `STARTUP_FAILURE_SOURCE:` line on
  stderr, and is not killed by a timeout. Shown in a real run.
- AC2 When the orderly quit is admitted it still saves the profile records.
  The existing profile quit and restart courses still pass.
- AC3 The `AUTOMATED_PROFILE_PRIVATE_FAILED` arm is either shown to exit
  promptly or recorded with the reason it cannot hang.

## Measured at `8957cf6b` (bpr-main-linux, 2026-09-23)

WSL2 Ubuntu, GCC 15.2, Qt 6.11.2, `build/dev` (Debug, tests enabled). A
temporary probe was built as a test course and reverted byte-for-byte
afterwards; none of it is committed. It had three parts:

- in `main.cpp`, an environment variable made the run setup throw
  `probe: entry` right after `application->show()`, or made the
  automated-profile check report `AUTOMATED_PROFILE_PRIVATE_FAILED`;
- `profile_management_host::request_quit` printed the four conditions its
  admission reads, and `profile_desktop`'s quit printed whether it was
  admitted;
- a temporary row per arm ran the shipped `--self-test` through the network
  sandbox and checked the exit code, the verdict, the source line and the
  elapsed time.

The rows ran through `ctest -j1`, offscreen, with fresh `HOME`, XDG roots
and `TMPDIR`. Both arms fail the same way:

| Arm | stdout | Quit request | Result |
| --- | --- | --- | --- |
| run setup throws | `AUTOMATED_PROFILE_PRIVATE_OK`, `STARTUP_FAILED` | `closing=0 ready=1 registry_quiescent=1 delivering=1`, refused | killed by the course's 25 s timeout |
| automated profile root missing | `AUTOMATED_PROFILE_PRIVATE_FAILED` | the same, refused | killed by the course's 25 s timeout |

So the refusal is the host's `Delivering`, not the registry. The completion
runs inside the management host's delivery of the open command: `deliver`
calls `perform`, which calls `open_window`, which calls `finish_initial`.
`request_quit` refuses a Quit made there. The host's own test already says
so ("reentrant Quit cannot interrupt a start command batch" and "Quit may
retry after command delivery"), but nothing in the desktop retried. The
stderr line `STARTUP_FAILURE_SOURCE:entry-point-run-setup: probe: entry`
was printed as expected.

## The fix

`profile_desktop::implementation::quit` now tells a person's Quit from a
failure quit by its exit code:

- **Exit code 0.** A refused Quit leaves the windows usable, as before.
- **Nonzero.** A refused failure quit sets `FailureQuit`. Each poll retries
  it, and after 200 refused polls (2 s) the shutdown is forced. This is the
  empty desktop's retry. Both now use one helper, `quit_within_bound`, and
  the empty desktop's own behaviour is unchanged.

Why retry first rather than force at once: the refusal is transient.
Delivery ends before the next poll, and an admitted Quit saves the profile
records as an explicit Quit does. Forcing is only the bound. Why the exit
code rather than the completion: every caller that passes a nonzero code
has already reported a failure. Those callers are the five arms of
`main.cpp`'s startup completion and an automated run's failed verdict. None
is a person's Quit. `main.cpp` itself is unchanged.

After the fix, with the same temporary probe:

| Arm | Exit | Elapsed | stdout / stderr | Path taken |
| --- | --- | --- | --- | --- |
| run setup throws | 1 | 338 ms | `STARTUP_FAILED`; `STARTUP_FAILURE_SOURCE:entry-point-run-setup: probe: entry` | first retry admitted (`delivering=0`), orderly Quit |
| automated profile root missing | 1 | 299 ms | `AUTOMATED_PROFILE_PRIVATE_FAILED` | first retry admitted, orderly Quit |
| run setup throws, every Quit refused | 1 | 2 290 ms | `STARTUP_FAILED` and the source line | forced after 200 refused polls |

The third arm made the desktop treat every request as refused, to exercise
the bound. After the orderly exits the private `TMPDIR` was empty, so the
automated profile directory had been removed on the way out. Before the
fix, the killed runs left it behind.

**AC3.** The `AUTOMATED_PROFILE_PRIVATE_FAILED` arm had the same defect and
is fixed by the same change. The run above shows it exits 1 promptly.

## The regression row

`application_profile_management_public_path` gains a section built on the
existing fixture desktop, with no new seam. It starts a desktop under the
resident policy, which would otherwise keep it alive. From inside the
startup completion, after an accepted start, it calls
`desktop->quit(EXIT_FAILURE)`, exactly as `main.cpp`'s arms do. It then
requires the desktop to reach quiescence with exit code 1.

**Negative control.** With `profile_desktop.cpp` restored to `8957cf6b`,
the row failed after 18.6 s with `FAILED: a failure quit made in the
startup completion ends the desktop`. With the fix it passes in 4.3 s. The
row cannot throw from `main.cpp` itself: that would need the test-only seam
the scope rules out. The temporary probe above covers the entry point.

## Pinned-code review (BPR-1045 reseal)

`profile_desktop.cpp` and `profile_desktop.hpp` are in the production
manifest pin. `main.cpp` and `github_application.cpp` are unchanged. The
change was written at `8957cf6b` and rebased onto `8595684c`. On the way,
[[BPR-1033]]'s `9d3fb998` resealed the full-application and manifest pins
for its own `github_application.cpp` change, and nothing else touched the
pinned scope. At `8595684c` all three pins matched: `6e76dd7c…`,
`4c203614…` and `a7e2081e…`, printed from a `git archive` of that commit.

- `quit`: the request is made exactly as before. The only new effect is
  `FailureQuit = true` when a nonzero-code request is refused.
  `UpdatePreparing` and `Quitting` still return first, and `!Host` still
  forces.
- `quit_within_bound`: `quit_empty_desktop`'s three branches, moved into one
  helper. It ends when `Quitting` is set or the request is admitted, and
  forces with `ExitCode` once the attempts reach the bound or no host
  exists. `quit_empty_desktop` calls it with its own counter, so its
  behaviour is unchanged.
- `poll`: retries a pending failure quit after the empty-desktop retry. It
  clears the flag as soon as `Quitting` is set, so a later orderly or forced
  shutdown never meets a stale retry.
- The constant is renamed `refused_quit_attempts` (200, unchanged), and its
  comment covers both uses.
- No credential, provider, vault, transport or mutation path changes. The
  mutation-branch and full-application pins are unchanged. Only the
  production manifest pin moved, to `8e5bc591…` on the rebased tree.

## Verification (bpr-main-linux)

Every row ran through `ctest -j1`, offscreen, with fresh `HOME`, XDG roots
and `TMPDIR`. Logs are under `build/logs/bpr-1045/` on this station and are
not committed.

| Check | Result |
| --- | --- |
| Probe, both arms, before the fix | killed at 25 s; one refused Quit each, `delivering=1` |
| Probe, both arms and the forced bound, after the fix | exit 1 in 338 ms, 299 ms and 2 290 ms |
| `application_profile_management_public_path` with the fix / without it | passed 4.27 s / failed as above |
| AC2: profile startup, management, multi-window, second-window close, two-account isolation, activation, owner admission, window restart, window layout and review restoration courses | 10/10 passed |
| `application_public_path`, `application_startup_failure_source` | passed |
| `core_profile_window`, `application_profile_management_host`, `application_profile_startup_admission` | passed |
| `application_review_round_trip_composition_contract` with the resealed pin | passed |
| `application_composition_contract` (builds the tests-disabled graph) | passed, 466 s |
| `clang-format` 22 on the four C++ files; `git diff --check` | clean |
| clang-tidy (LLVM 21, the only one on this station) on the two changed units | no finding on a changed line; the findings it reports are in untouched lines |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

## macOS and Windows

No acceptance criterion names a platform, and the change has no platform
branch. `profile_desktop.cpp`'s `__APPLE__` blocks are untouched. The new
section runs wherever `application_profile_management_public_path` does,
which includes Windows (`WINDOWS_UNCONTAINED`). Neither platform has run
it. This is a confirmation for the next routine run there, not something
owed before Done:

```sh
ctest --preset dev -R '^application_profile_management_public_path$' --output-on-failure          # macOS
ctest --preset windows-dev -R '^application_profile_management_public_path$' --output-on-failure  # Windows
```

Expected: `profile-management-public-path:pass`, with no
`FAILED: a failure quit made in the startup completion ends the desktop`.
