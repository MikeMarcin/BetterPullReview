---
schema: "implement-rfc.card.v1"
id: "BPR-0840"
title: "The agent chat stale contract reported workspace unavailable under a parallel ctest run"
status: "Done"
rfcs: ["RFC-BPR-0012"]
depends_on: []
priority: "P3"
platforms: ["mac"]
metadata: {"found_by":"dsp-003 integration pass at 8a03a5be","found_at":"8a03a5bee813fc285e1c0be91f6a5c070436364f","run":"run-20260916-stations","dispatch":"dsp-003"}
acceptance: [{"criterion":"AC1 the cause is named, and the agent chat and finding composition courses pass together under ctest -j 4 for 5 consecutive batches on macOS","outcome":"pass","revision":"98a661be68ae42bba613b159cb1da54cdd7cbc9b","evidence":{"kind":"command","detail":"Cause named on the card: BPR-0809's setpgid supervisor race (fixed at 6a596d91, after both failing revisions), reproduced exactly by injecting the pre-fix EPERM start report for Git steps (chat stale workspace_unavailable) and the agent (full-agent failed_to_start); no shared resource or lock wait under loads up to 64. At 7a37d89c plus the test-only panel-status report, all 15 application_agent_(chat|finding)_ rows passed 5/5 consecutive ctest -j 4 batches at load 8.10 to 5.23; the other 15 application_agent_* rows passed once (build/r2-logs/dsp4-0840-final.log, dsp4-0840-accept-{1..5}.log)."}}]
---

# BPR-0840: The agent chat stale contract reported workspace unavailable under a parallel ctest run

## Observation

In a `ctest -j 3` batch at `8a03a5be`, alongside other agent chat and finding
composition courses and two concurrent worker builds (load about 5),
`application_agent_chat_stale_contract` failed with

    agent-chat-composition-failure:chat-turn failed=run-quiescent process=0 replies=0 panel=Finished: workspace_unavailable run-quiescent=false

(`build/r2-logs/dsp3-int1-ctest.log`). Run alone twice it passed (12.86 s and
13.07 s; `dsp3-agent-ab-head.log`, `dsp3-agent-ab-pretests.log`). One
observation only.

## Scope

- Establish whether concurrent agent composition courses share anything that
  makes a workspace unavailable (a neighbourhood path, a lease, a registry
  under a shared temporary parent), or whether this is a load-dependent
  timeout inside workspace preparation.
- Fix a shared resource, or give the course the bounded wait its other
  conditions have if it is purely load, and say which.

## Acceptance

- The cause is named here, and the agent chat and finding composition
  courses pass together under `ctest -j 4` for 5 consecutive batches on
  macOS.

## More rows under load (dsp-003)

At main `6c44b6ea` with [[BPR-0835]]'s named failure terms, two `ctest -j 2`
batches of the agent-finding rows ran at load averages of 13 to 31. A
concurrent `until-fail:40` stress run and worker builds were loading the
machine. In each batch one row failed, and the other batch passed it
(`build/r2-logs/dsp3-int6-ctest-{1,2}.log`):

- `application_agent_finding_full_agent_contract`:
  `positive-evidence failed=queued-findings-is-one workshop-proposals-is-one draft-body-is-suggestion deliveries-is-one …`.
  No delivery had arrived when the course sampled.
- `application_agent_finding_workshop_press_contract`:
  `agent-finding-workshop-press-turn:3:failed=run-quiescent process=2 run-quiescent=false`.

These are the same kind of observation as the chat stale row: a composed
agent turn that has not settled by the course's bound under heavy load. The
scope extends to these rows.

## Cause and fix (dsp-004)

**The cause is [[BPR-0809]]'s supervisor race.** It is not a shared
resource, a product timeout or the course budget.

At `8a03a5be` and `6c44b6ea` the Unix agent supervisor and its forked child
both called `setpgid`, and Darwin sometimes refused the child's call with
EPERM. The same supervisor runs the agent and, with `--workspace-lease`, every
Git step of workspace materialization. So one race produced two failures:

- **A refused Git step.** It exits 127. That becomes `nonzero_exit`, then the
  cache's `process_failure` (`installed_git_cache.cpp` ~4586), then
  `agent_workspace_failure::unavailable` (`epoch_application_host.cpp:4479`),
  then `workspace_unavailable` (`agent_invocation_host.cpp` 46-50). No process
  starts. This is the chat stale row and, by the same shape, workshop press
  turn 3.
- **A refused agent launch.** It becomes `failed_to_start` "Operation not
  permitted" (`agent_process_launcher.cpp` ~446-450). This is the full-agent
  row, which failed in 12.7 s.

Load changed only how often the race hit; each turn makes about 8 supervised
launches. The 45-second waits ran out because a terminal failure can never
satisfy them.

**Evidence.**

- **The fix predates both failures.** Both failing revisions predate BPR-0809's
  fix at `6a596d91`.
- **Both lines reproduce exactly** when the pre-fix EPERM start report is
  injected:
  - chat stale, in 55.9 s:
    `chat-turn failed=run-quiescent process=0 replies=0 panel=Finished: workspace_unavailable run-quiescent=false`
  - full-agent, in 11.8 s:
    `agent-finding-positive-evidence:0:0:1:1:0:0:0:1:0:0 … panel-status:Finished: failed_to_start`
  (`build/r2-logs/dsp4-0840-inject-*.log`)
- **No shared resource.** Each course has its own private TMPDIR, socket root,
  data root, workspace area, registry and Git cache root.
- **No failures at HEAD under load.** Six instrumented batches under loads up
  to 64 had no failures and no lock waits, including fsync and spawn load and
  a run alongside the Git cache contracts (`dsp4-0840-repro*.log`).
- **Negative control.** With the fix reversed, 4 batches passed 60/60, so the
  race did not recur. That is consistent with its rate of about 0.5% per
  launch, so the cause rests on the injected reproduction, not on this
  control.

**Fix.** No product change is needed. The only change is a test-only failure
report: both run waits in `agent_finding_composition_public_path.cpp` now report
the agent panel's status. A turn that ends without launching now names how it
ended, as an injected run showed:
`agent-run failed=run-quiescent process=0 panel=Finished: workspace_unavailable`.

**Acceptance run.** It ran at `7a37d89c` with the course change; the build dir
held no diagnostics. All 15 `application_agent_(chat|finding)_` rows passed
together under `ctest -j 4` for 5 consecutive batches. Load averages at the
start and end of each batch:

| Batch | Start | End |
|---|---|---|
| 1 | 8.10 | 6.60 |
| 2 | 6.60 | 6.07 |
| 3 | 6.07 | 5.74 |
| 4 | 5.74 | 5.35 |
| 5 | 5.35 | 5.23 |

The other 15 `application_agent_*` rows passed once
(`dsp4-0840-final.log`, `dsp4-0840-accept-{1..5}.log`).

**Note.** During the acceptance run the integrator edited `src/app` sources for
[[BPR-0861]], without building. The measured binaries were built at 10:23:10,
before those edits.
