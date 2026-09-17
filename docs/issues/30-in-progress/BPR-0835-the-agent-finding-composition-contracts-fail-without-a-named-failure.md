---
schema: "implement-rfc.card.v1"
id: "BPR-0835"
title: "The agent finding composition contracts fail without a named failure"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win", "mac"]
metadata: {"found_by":"bpr-win task-20260917-win-hardening new defect","found_at":"711d3ddc5f8db9f237e4c4b84758d116b9e897af","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0835: The agent finding composition contracts fail without a named failure

## Defect

`application_agent_finding_composition_contract` is red on Windows
uncontained and prints no named failure term, even with the fixture data root
narrowed to the owner-private shape by [[BPR-0812]]
(`tests/end_to_end/agent_finding_composition_public_path.cpp`, data root
`<tmp>/data`).

Reported by `bpr-win` in `task-20260917-win-hardening`, not carded there.

**macOS too, and before the Windows merge (dsp-003).** On macOS
`application_agent_finding_composition_contract` and
`application_agent_finding_full_agent_contract` both fail at main `5a2e264a`
and identically at `2ee49a16`, before windows-lane's BPR-0812 test changes
were merged, so this is not a Windows defect and not caused by that merge.
Both print `agent-finding-positive-evidence:1:1:1:1:1:1:0:1:0:0` and
`panel-status:Finished: success`, and the full-agent contract prints
`agent-finding-full-agent:0:0:<n>:0:0:0`; neither prints its pass term
(`build/r2-logs/dsp3-int1-ctest.log`, `dsp3-agent-ab-head.log`,
`dsp3-agent-ab-pretests.log` with the two test sources restored from
`2ee49a16`, `dsp3-base-finding-ctest.log` built at `2ee49a16`). When it
started is not established.

## Scope

- Find the first bad revision on macOS and the mechanism (which of the
  positive-evidence fields is 0, and why).
- Make the course name its failure first (the step it stopped at), then fix
  the cause.
- Whatever the cause, a composition course that ends without a named term is
  itself a defect: the course must print the failed step.

## Acceptance

- Both contracts pass on macOS and on the Windows station, and the cause is
  named here.
