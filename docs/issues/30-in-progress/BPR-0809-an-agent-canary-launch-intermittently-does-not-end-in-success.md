---
schema: "implement-rfc.card.v1"
id: "BPR-0809"
title: "An agent canary launch intermittently does not end in success"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"BPR-0666 repro and final batches","found_at":"94f4e6dc","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
---

# BPR-0809: An agent canary launch intermittently does not end in success

## Defect

While proving [[BPR-0666]], a plain agent canary launch occasionally ended
without its success line, separately from the workspace-guard race that card
fixed:

- `agent_parent_death` HEAD run 3, in the `--workspace-guard` course;
- the first final `until-fail:20` attempt, run 1, in `--recover-agent-parent`
  (the guard check had already passed);
- about 1 in 60 serial recovery launches (`build/r2-logs/bpr0666-recover-direct.log`).

A temporary result printout did not reproduce it in 72 parallel launches, so
the terminal outcome is unknown. The launch lives in the agent launcher's run
loop, outside BPR-0666's owned paths.

## Scope

- Capture the terminal outcome of a failing launch: exit status, signal, last
  protocol event, and whether the supervisor or the canary ended first.
- Fix the race or defect it shows, or give the course the bounded wait its
  other conditions have if it is purely an observation race. Say which.

## Acceptance

- The launch outcome of a failing run is named on this card with evidence.
- `agent_parent_death` passes 40 consecutive runs on macOS under the load of
  a concurrent build.

## Cause and fix (dsp-003)

**The failing launch.** An instrumented harness caught 7 failures in 1,527
recovery launches, at load 2.3 to 8.3 (`build/r2-logs/dsp3-0809-repro1.log`,
`dsp3-0809-repro2.log`). Each ended about 0.1 s after start:

- The outcome was `failed_to_start`, with standard error
  "Operation not permitted". There was no exit code and no protocol event.
- The supervisor was still running, and it had sent a 4-byte start report
  of errno 1 (EPERM).
- The canary never exec'd, so the supervisor ended the launch before the
  canary could run.

**Cause: a race in the product.** The Unix agent supervisor calls
`setpgid(child, child)` while the forked child calls `setpgid(0, 0)`
(`src/adapters/agent/src/unix_agent_supervisor.cpp`, ~127 and ~113 at
`866c5b90`). When the two calls overlap, Darwin refuses the child's with EPERM.

- The child forwards that errno through the exec pipe, and the launcher turns
  it into `failed_to_start` (`agent_process_launcher.cpp` ~448).
- A standalone fork probe with no Qt reproduced the race
  (`dsp3-0809-pgrace.log`). With both sides calling `setpgid`, the child got
  EPERM 24 to 33 times per 20,000 forks. In 2 of those, the child was not yet
  its group's leader right after the refusal, so simply ignoring EPERM would
  not be safe.
- With only the parent calling, or only the child, there were 0 failures in
  120,000 forks each.

**Fix.** The supervisor alone creates the group:

- Before exec, the child blocks on a close-on-exec pipe until the supervisor
  has called `setpgid`. It then checks `getpgrp() == getpid()` and execs.
- If the child reads end-of-file (the supervisor gave up or died), or finds
  itself not the group leader, it reports EPERM and never execs.
- If `setpgid` or the release write fails, the supervisor reports the errno
  and SIGKILLs the child.
- The old EACCES exception is gone: the child cannot have exec'd before the
  supervisor's call.
- On failure, the canary tests now print the launch outcome and standard
  error, as the Windows course already does.

**Verification.**

- **Slice** (`build/r2-logs/`):
  - `agent_parent_death` passes 40 of 40 under `until-fail:40` (1,100 s).
    The load came from three concurrent builds and six CPU spinners, with
    1-minute load 7 to 40 (`dsp3-0809-until-fail-40.log`).
  - After the fix, the harness ran 736 alternating recovery and
    workspace-guard launches with 0 failures, at load 7 to 14
    (`dsp3-0809-repro-after-fix.log`).
  - `qt_agent_launch_canary`, `agent_parent_death`,
    `agent_workspace_ownership` and `application_agent_crash_recovery` pass
    (`dsp3-0809-neighbours.log`).
  - Format is clean. Strict tidy shows only the two findings already present
    at HEAD (`dsp3-0809-format.log`, `dsp3-0809-tidy.log`).
- **Integrated** at main `eade8828` with the change (`dsp3-int8-ctest.log`,
  `dsp3-int8-parent-death-x10.log`):
  - 11 of 11 pass: `qt_agent_launch_canary`, `git_process_contract`, both
    crash-recovery courses, `agent_workspace_ownership`,
    `architecture_boundaries` and five other rows;
  - `agent_parent_death` passes 10 of 10.
- **Independent critical review:** no blocker. It traced exec gating, parent
  death at every point, descriptor handling, the reaping and owner-death
  group kill in every window, errno reporting, Linux `setpgid` and fortify
  semantics, and the two test lines. Its non-blocking notes:
  - The fork probe's gated mode still discarded EACCES on the parent's call,
    so the probe alone does not show EACCES never occurs. The real-binary
    runs cover this, and XNU returns EACCES only after exec.
  - A supervisor killed after the release write but before the child's exec
    leaves the agent orphaned in its own group. The old code behaved the same
    way, and this is outside BPR-0666's owner-death contract.
