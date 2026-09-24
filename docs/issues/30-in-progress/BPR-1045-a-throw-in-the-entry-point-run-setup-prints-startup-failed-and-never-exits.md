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
