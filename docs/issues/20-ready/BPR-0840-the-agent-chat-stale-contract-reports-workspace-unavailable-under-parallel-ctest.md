---
schema: "implement-rfc.card.v1"
id: "BPR-0840"
title: "The agent chat stale contract reported workspace unavailable under a parallel ctest run"
status: "Ready"
rfcs: ["RFC-BPR-0012"]
depends_on: []
priority: "P3"
platforms: ["mac"]
metadata: {"found_by":"dsp-003 integration pass at 8a03a5be","found_at":"8a03a5bee813fc285e1c0be91f6a5c070436364f","run":"run-20260916-stations","dispatch":"dsp-003"}
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
