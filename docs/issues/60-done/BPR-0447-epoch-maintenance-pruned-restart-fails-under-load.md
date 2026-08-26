---
id: BPR-0447
title: The epoch-maintenance pruned-restart run fails under parallel load
status: Done
rfc: RFC-BPR-0003
depends_on: []
---

# The epoch-maintenance pruned-restart run fails under parallel load

`application_epoch_maintenance_public_path` fails identically in three
target-native Windows runs on 2026-08-23 (the settled full matrix and two
focused reruns, one with and one without Git's `usr/bin` on `PATH`). These
are the contract's first real executions on Windows: every earlier run was
refused by the host policy before capturing anything.

Measured, from the contract's own output:

- `EPOCH_MAINTENANCE_PRUNED_RESTART_DELETE_OK process failed (1)` — the
  restart-after-prune application run exits 1.
- `PUBLIC_PATH_STAGE:14`, `PUBLIC_PATH_VERDICT:presentation=0
  shutdown_complete=1`, `PUBLIC_PATH_GROUPS:core=0 fixture=0 maintenance=0
  render=1`.
- The restart run resolves no exact pair: `PUBLIC_PATH_EPOCH_PAIR:::-1:-1:2:0`,
  `PUBLIC_PATH_EPOCH_CORE_STATUS:-1`, epoch status stuck at "Resolving the
  exact comparison…" while `PUBLIC_PATH_STATUS` reads "Pull request ready."
- `EPOCH_FIXTURE_REUSE_FAILED`, `REVIEW_LEDGER_PRESENT_ON_ENTRY:0`,
  `REVIEW_LEDGER_FAILED`, and an all-zero
  `PUBLIC_PATH_MAINTENANCE_EVIDENCE` ending `:-1:-1`.
- The non-restart epoch flows pass on this host:
  `application_epoch_persistence_github-com`,
  `application_epoch_persistence_ghes-3_18`, and the composition contracts
  all passed in the same matrix.

## Required final state

- The failure is diagnosed to a cause on this host, not a symptom string.
- The cause is repaired on its own card if it is product code, or here if it
  is contract/fixture environment; the contract then passes target-native.
- No assertion is weakened and no Windows exemption is added to make it pass.

## Non-goals

- Not the Smart-App-Control refusal class (owner-deferred on [[BPR-0036]]).
- Not RFC-BPR-0002 closeout; none of its selectors depend on this contract.

## It is not a Windows failure — reproduced on macOS, 2026-08-26

Found by [[BPR-0454]]'s closing `dev` matrix on this macOS host, which returned
217 of 218 with this contract as the only failure. It then reproduced
deliberately: **two of three** runs failed under 40 spinning processes on a
14-core host, and it passes in isolation on the same commit (57.25 s, exit 0).
So the trigger is parallel load, not the operating system.

Every marker this card recorded from Windows appears identically here:

```
EPOCH_MAINTENANCE_PRUNED_RESTART_DELETE_OK process failed (1)
EPOCH_FIXTURE_REUSE_FAILED
EPOCH_MAINTENANCE_PRUNED_RESTART_DELETE_FAILED
PUBLIC_PATH_STAGE:14
PUBLIC_PATH_VERDICT:presentation=0 shutdown_complete=1 cleanup_failed=0
PUBLIC_PATH_GROUPS:core=0 fixture=0 maintenance=0 render=1
PUBLIC_PATH_STATUS:Pull request ready.
PUBLIC_PATH_EPOCH_PAIR:::-1:-1:2:0
PUBLIC_PATH_EPOCH_CORE_STATUS:-1
PUBLIC_PATH_MAINTENANCE_EVIDENCE:0:0:0:0:0:0:0:0:0:0:0:0:0:-1:-1
REVIEW_LEDGER_PRESENT_ON_ENTRY:0
REVIEW_LEDGER_FAILED
```

That is the whole list above, including the all-zero maintenance evidence
ending `:-1:-1` and the epoch status stuck at "Resolving the exact
comparison…" while the status line reads "Pull request ready." A Windows
session is therefore not required to diagnose this, and the card moves to
`20-ready/`.

The Windows observation stands as the first sighting; what it does not
support is the attribution. The three Windows runs were the contract's first
real executions on that host, so there was nothing to compare them against —
and this host had never run it under enough load to see it either.

### What this changes about the card

- The trigger to reproduce is parallel load, not a platform. Reproduce with
  the contract under a loaded matrix, not with a bare focused run.
- The required final state is unchanged, and the prohibition on a Windows
  exemption now reads more strongly: there is no platform to exempt.
- The "non-restart epoch flows pass on this host" observation held on Windows
  and holds here.

## Diagnosed — 2026-08-26

### It is not a timeout, and the card's marker list is half noise

Reproduced here first: three runs of `application_epoch_maintenance_public_path`
under 40 spinning processes on a 14-core host, two failed. Both failures carry a
line neither this card nor the Windows sighting recorded, because `ctest`
prints the phase's stderr below its stdout and the card kept only the stdout:

```
PUBLIC_PATH_BUDGET:expired=0 budget_ms=60000 elapsed_ms=167 stage_ticks=2
EPOCH_CACHE_FAILURE:14:
EPOCH_EFFECT_FAILED:comparison-resolve:0
```

The run fails **167 ms into a 60-second budget it never touches**. Every
timeout hypothesis this card invites — a budget too tight for a loaded host, a
`bpr_bound_automated_run` pair that drifted — is falsified by that one line.
The passing run in the same batch took 66.84 s wall clock for all four phases.

Two of the markers listed as measured evidence above are not evidence of
anything. `REVIEW_LEDGER_PRESENT_ON_ENTRY:0` and `REVIEW_LEDGER_FAILED` print
identically on a passing phase: `stage::open` takes the maintenance-probe
branch at `src/app/main.cpp:4267` and breaks before reaching the ledger round
trip at `:4357`, so both fields keep their `false` defaults for every
maintenance phase, and the print guard at `:3562` does not exclude those runs.
`PUBLIC_PATH_EPOCH_CORE_STATUS:-1` is the same kind of artefact: the field is
only ever assigned in `capture_automated_timeout_evidence`, which runs on
budget expiry, and this failure does not go that way — so `-1` is its
initialiser, not an observation. The all-zero
`PUBLIC_PATH_MAINTENANCE_EVIDENCE` is real but downstream.

`PUBLIC_PATH_EPOCH_STATUS` and `PUBLIC_PATH_EPOCH_PAIR` **are** live: they fall
back to reading the selectors at print time when nothing captured them
(`:3188`). That matters, because what they say is the whole diagnosis.

### The cause

`git_cache_failure` 14 is `cancelled`
(`src/application/include/bpr/application/git_cache_port.hpp:579`). Under load
the comparison-resolve effect's cache open is cancelled;
`start_comparison_resolution` maps that to `epoch_failure::unavailable` and
dispatches `comparison_resolve_failed_event`
(`src/application/src/epoch_application_host.cpp:6217`). No exact comparison is
accepted — the selectors read `Whole PR.  No exact pair is resolved.` — while a
fresh resolution is still in flight, which is why the live status line still
reads `Resolving the exact comparison…` at the moment the run gives up.

`request_fixture_diff` reads `state.AcceptedComparison`
(`src/app/src/github_application.cpp:10606`) and refuses while it is not the
expected exact pair. `begin_epoch_maintenance_probe` returns that refusal
straight through (`src/app/main.cpp:2624`), and `stage::epoch_maintenance_history`
called `fail_automated_run` on it.

That is the defect, and it is one line of judgement rather than a race of its
own: **the same predicate is polled in one stage and treated as fatal in the
next.** `stage::open` calls `begin_epoch_maintenance_probe` and discards a
false return (`:4269`), because every condition inside that function is
established asynchronously. Stage 14 read the identical false as "wrong" and
ended the run. Instrumenting each refusal point and re-running under the same
load named the gate directly: `diff-request-refused`, on the tick the diff was
first requested.

### Why it looks like a platform failure and is not

It needs the cancellation to land in the window between the restarted process
accepting a revision and stage 14's first tick. On an idle host the resolution
wins that race every time. Under a loaded matrix it does not, at roughly one
run in three here — which is also why three consecutive Windows runs on a busy
host all failed and read as a Windows fact.

## Done — 2026-08-26

### The repair

`stage::epoch_maintenance_history` polls, exactly as `stage::open` already does
with the same predicate. `automated_run::MaintenanceGate` records which of the
ten conditions the probe is waiting on, and the failure report prints
`PUBLIC_PATH_MAINTENANCE_GATE:<name>` beside `PUBLIC_PATH_MAINTENANCE_EVIDENCE`.

Nothing the contract asserts was removed, relaxed, or exempted. What changed is
when the verdict is reached: a condition that never becomes true now fails the
run through the budget the fixture already had, and names itself on the way
out. The old behaviour reached a verdict in 163 ms and the verdict was wrong.

The repair is entirely in `src/app/main.cpp`, under `BPR_BUILD_TESTING`. No
product code is touched, which is what this card's required final state asks
for.

### The wait is what fixes it, not luck

Ten runs of the contract under 40 spinning processes on a 14-core host: **10
passed**. The same load failed two of three and one of four before the change.

Ten green runs are not proof on their own — the race could simply have stopped
happening — so the wait was instrumented and the four phases run directly under
the same load, six times, capturing each phase's own stderr:

| iteration | phase-3 exit | ticks spent in `comparison-not-resolved` |
|---|---|---|
| 1 | 0 | 0 |
| 2 | 0 | 462 |
| 3 | 0 | 463 |
| 4 | 0 | 0 |
| 5 | 0 | 0 |
| 6 | 0 | 466 |

Three of six runs hit the race at about the rate it used to fail at, waited
roughly 460 ticks, and passed. Before this change the first of those ticks
ended the run. That table is the evidence; the ten green runs are the
consequence.

### It still fails when the claim is false

Two mutations, each built successfully, run against the repaired contract, and
reverted. Neither hangs and neither passes, and they name different conditions:

| mutation | expected | result |
|---|---|---|
| `request_fixture_diff` issues its request and always reports refusal (`src/app/src/github_application.cpp`) | budget expiry naming the diff condition | **fails**, `expired=1 budget_ms=60000 elapsed_ms=60023`, `PUBLIC_PATH_MAINTENANCE_GATE:comparison-not-resolved` |
| the Qt maintenance projection never enables the delete command (`src/adapters/qt/src/epoch_comparison_control.cpp`) | budget expiry naming the pruned-state condition | **fails**, `expired=1 budget_ms=60000 elapsed_ms=60014`, `PUBLIC_PATH_MAINTENANCE_GATE:pruned-state` |

The second one crosses the core-projection → Qt → fixture-course seam rather
than mutating the same layer the repair is in, and together the two prove the
new marker discriminates: two different false claims produce two different gate
names, where both used to produce `PUBLIC_PATH_STAGE:14` and nothing else.

A third mutation attacks the risk this repair actually introduces. Making a
stage wait where it used to fail is only safe if a permanently wrong state
still reaches a verdict, so `settled` in the `verify_deleted` branch was forced
false — the one gate whose `true` ends the run *successfully*:

| mutation | expected | result |
|---|---|---|
| `const auto settled = false && ...` in the `verify_deleted` branch | the deleted-restart phase fails rather than passing | **fails**, `expired=1 budget_ms=60000 elapsed_ms=60019`, `PUBLIC_PATH_MAINTENANCE_GATE:deleted-state-unsettled` |

It reported `PUBLIC_PATH_STAGE:2`, not 14, and that is the point rather than an
oddity. A `verify_deleted` probe is driven from `stage::open`, which has always
discarded the same false return, so an unsettled run stalls in `open` and the
stage number names the place every run passes through. The gate names the
condition. That is the whole reason the marker exists.

### The matrix, and the guard that caught the edit

The full parallel `dev` matrix is the condition this failure came from — it is
where [[BPR-0454]] first saw it — so it is the closing evidence rather than a
formality: **218 of 218**, exit 0, 282.64 s at `-j 14`, with
`application_epoch_maintenance_public_path` among the passes.

The first attempt returned 216 of 218. Both failures were one cause and neither
was a regression: `bpr_verify_app_source_tree` pins a content digest over the
shipped `src/app/` tree, and editing `main.cpp` drifted it. Repinned to
`94e0136069b81fc783f8c048b9068bf8674af196e05850a6abdac2d9091fc99d` from the
formatted source, which is the state committed here — [[BPR-0414]] recorded the
same two-step and the same trap, that `coding-format` rewrites `main.cpp` after
a first repin and drifts the pin twice from one change.

The guard is worth the interruption. It exists so that a change to shipped
application source gets looked at, and what it caught here is real: this card
edits `src/app/main.cpp`. What it edits is the automated fixture course, inside
`BPR_BUILD_TESTING`, and no interactive, provider, or mutation path is
reachable from it.

That matrix ran on a settled tree. An earlier attempt was discarded rather than
reported: the application binary was relinked underneath it while it ran, so
its result described no single tree.

### The review gate ran serially, and why

`rfc-reviewer` was launched against the uncommitted change set with six named
claims to falsify. It ran for over an hour without returning a report and did
not answer a request to report what it had, so it was stopped and the skeptical
pass was taken in the primary agent instead. That is a serial fallback and it
is recorded here rather than presented as an independent review.

What the integrator pass checked, against the diff rather than the narrative:

- **Can a wrong state now pass?** No. Stage fourteen's success branch requires
  `begin_epoch_maintenance_probe` to return true *and* the probe to be
  `verify_deleted`; for that probe the return value is `settled`, so the
  mutation above is the direct test and it fails. For `prune` and
  `verify_pruned_and_delete` a true return advances the stage and the second
  conjunct is false, so nothing passes there at all.
- **Is the budget really the bound?** `bpr_bound_automated_run` refuses a pair
  that does not fit: four runs of 60,000 ms plus 8,000 ms reserve is 248,000 ms
  against the 270,000 ms three-quarter share of the 360 s timeout. All three
  mutations expired at ~60,020 ms and the contract completed in 103-109 s.
- **Can the gate print a stale condition?** It is cleared on every path that
  advances or succeeds, so a run failing at a later maintenance stage prints
  `none`, which is true — the probe is not what is blocking it.
- **Does the new marker line break a contiguous match?** No consumer parses
  `PUBLIC_PATH_MAINTENANCE_EVIDENCE` outside the application itself, and the
  one contiguous multi-line marker regex, in
  `native_appearance_probe_contract.cmake`, is over a block no maintenance
  probe reaches. The 218/218 matrix is the empirical half of that answer.

### Not repaired here, and carded instead

`start_comparison_resolution` reports a **cancelled** cache open to the core as
`comparison_resolve_failed_event{ unavailable }`, while both catalog results in
the same effect check for cancellation and dispatch
`comparison_resolve_cancelled_event`. That is product code, so by this card's
own rule it belongs on its own card: [[BPR-0459]]. It is not this failure's
cause — in these runs the failed event arrives stale and changes no state,
which is why the selectors still read `Resolving the exact comparison…` — and
the repair above is verified without it.
