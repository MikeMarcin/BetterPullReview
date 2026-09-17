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
