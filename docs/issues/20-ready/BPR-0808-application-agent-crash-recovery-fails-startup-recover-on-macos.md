---
schema: "implement-rfc.card.v1"
id: "BPR-0808"
title: "application_agent_crash_recovery fails its startup-recover step on macOS"
status: "Ready"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"BPR-0666 neighbour selectors","found_at":"94f4e6dc","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
---

# BPR-0808: application_agent_crash_recovery fails its startup-recover step on macOS

## Defect

At `94f4e6dc` in a fresh `build/r2-dev` tree, `application_agent_crash_recovery`
fails 3 of 3 runs, each after about 20 s:

    tests/scripts/agent_composition_kill_recovery_tests.py:109 live_recovery
    RuntimeError: startup-recover failed (1):
    application-startup-failed:

Nothing follows the colon. Putting `installed_git_process.cpp` back to its
pre-[[BPR-0666]] bytes leaves the failure unchanged
(`build/r2-logs/bpr0666-crash-recovery-without-tidy-edit.log`). BPR-0622
recorded this course passing at `0427dc76` on 2026-09-05. It is in
[[BPR-0622]]'s Linux and Windows verification set, so it must be green on
the Mac before those stations' results mean anything.

A BetterPullReview app from `build/dev` was running under the user's
default profile throughout (PID 88792). The same tree's profile courses did
not report `PROFILE_ALREADY_OPEN`, but whether this course's live startup
uses an isolated root has not been checked.

## Scope

- Make the startup failure say why: the empty detail is itself a
  diagnosability defect. Then find the cause, either by bisecting from
  `0427dc76` or by reading what the live `startup-recover` course composes now.
- Rule out the running default-profile app first.
- Fix the regression where it lives. Keep the course's crash, pause and
  recovery checkpoints.

## Acceptance

- `application_agent_crash_recovery` passes three consecutive runs on macOS
  with no user app interfering, and the cause is named on this card.
- A startup failure in that course prints a non-empty typed reason.
- `application_agent_materialization_crash_recovery`,
  `agent_workspace_ownership` and `agent_parent_death` still pass.
