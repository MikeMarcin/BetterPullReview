---
schema: "implement-rfc.card.v1"
id: "BPR-0808"
title: "application_agent_crash_recovery fails its startup-recover step on macOS"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"BPR-0666 neighbour selectors","found_at":"94f4e6dc","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
acceptance: [{"criterion":"application_agent_crash_recovery passes three consecutive runs on macOS with no user app interfering, and the cause is named on this card","outcome":"pass","revision":"a5667197","evidence":{"kind":"command","detail":"3/3 pass (47.73/46.72/46.51 s) at a5667197; causes: 3e7a26d6 intended owner_busy lease made the course stale, 826fb871 dropped the repair message; the running default-profile app was not involved (course uses its own roots)"}},{"criterion":"A startup failure in that course prints a non-empty typed reason","outcome":"pass","revision":"282e4908","evidence":{"kind":"command","detail":"exhaustive failure-name mapping; original course prints application-startup-failed:failure=owner_busy (build/r2-logs/bpr0808-diagnosed.log); negative startup-owner-busy prints application-startup-not-refused:failure=none"}},{"criterion":"application_agent_materialization_crash_recovery, agent_workspace_ownership and agent_parent_death still pass","outcome":"pass","revision":"a5667197","evidence":{"kind":"command","detail":"int2-ctest.log: 41.02 s, 2.42 s, 26.20 s pass"}}]
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

## Cause and evidence, 2026-09-17 (run-20260916-stations, dispatch dsp-001-r2)

Two regressions were chained; the second only showed once the first was
fixed. The fix is `282e4908`.

1. **The course's assumption went stale at `3e7a26d6`** ("Admit exclusive
   profile owners before services and window reservations"). Startup now takes
   the exclusive profile runtime lease. The course's first `live_recovery` runs
   `startup-recover` while its own `crash-running` owner is still alive on the
   same root, so startup is refused with `owner_busy`. The architecture document
   from that commit specifies exactly that refusal. The detail was empty because
   the course's startup wait checked only that startup finished, not the typed
   failure.
   - The old course fails at line 109 at `3e7a26d6`
     (`build/r2-logs/bpr0808-bisect-3e7a26d6.log`) and gets past it at the
     parent `13a4337b`.
   - At HEAD, with the reason printed, it reads
     `application-startup-failed:failure=owner_busy`
     (`bpr0808-diagnosed.log`).
2. **A repair message was lost at `826fb871`** ("Finish BPR-0705"). BPR-0705
   moved workspace recovery out of the window into the profile runtime and
   dropped the agent panel's "Disposable workspace recovery needs repair or has
   reached its limit." `13a4337b` and HEAD-with-only-the-course-fix both fail at
   `startup-repair-visible failed=repair-visible repair-visible=false`. The drop
   was accidental: BPR-0705 forbids "disabling existing agent features to avoid
   ownership work", and BPR-0622 requires a visible repair or refusal.

**The change.**

- The course prints a named startup failure. Its mapping has no default case,
  so a new failure type breaks the build rather than printing an empty reason.
- A new `startup-owner-busy` step passes only on an `owner_busy` refusal with
  no application started.
- After the owner dies, with the supervisor still paused, `startup-recover`
  still runs and asserts nothing is reclaimed.
- The crash, pause, substitution, repair, reclamation, reopen and session
  checkpoints are unchanged.
- A refused recovery is recorded on the profile and presented in every open
  window and in windows opened later.

The running default-profile app (PID 88792) was not involved: every course
invocation uses its own temporary data root.

**Verification.**

| Check | Result | Revision |
|---|---|---|
| `application_agent_crash_recovery`, integrated serial pass + 2 more | 3/3 pass (47.7 s, 46.7 s, 46.5 s); `build/r2-logs/int2-ctest.log`, `int2-crash-recovery-{2,3}.log` | `a5667197` |
| `application_agent_materialization_crash_recovery`, `agent_workspace_ownership`, `agent_parent_death` | pass (41.0 s, 2.4 s, 26.2 s) | `a5667197` |
| Negative: `startup-owner-busy` with no live owner | exits 1 with `application-startup-not-refused:failure=none application=true` (`bpr0808-owner-busy-negative.log`) | scratch `74c605c5`+patch |
| Profile courses touched by the same composition (`application_profile_startup/management/multi_window_public_path`, `application_profile_empty_desktop_native_menu_bar`) | pass | `a5667197` |

**Residual.**

- The restored message is proven only on the standalone single-window startup
  path; no test opens a window after recovery has already failed.
- `agent_chat_composition_public_path.cpp`,
  `agent_finding_composition_public_path.cpp` and
  `queue_composition_public_path.cpp` print the same empty
  `application-startup-failed:` pattern.
- bpr-linux saw this course fail identically before the fix
  ([BPR-0622 Linux evidence](../../verification/BPR-0622-linux-native-evidence.md)).
  Linux has not re-run it at the fix.
