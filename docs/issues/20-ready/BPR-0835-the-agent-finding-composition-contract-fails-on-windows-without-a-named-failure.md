---
schema: "implement-rfc.card.v1"
id: "BPR-0835"
title: "The agent finding composition contract fails on Windows without a named failure"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-hardening new defect","found_at":"711d3ddc5f8db9f237e4c4b84758d116b9e897af","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0835: The agent finding composition contract fails on Windows without a named failure

## Defect

`application_agent_finding_composition_contract` is red on Windows
uncontained and prints no named failure term, even with the fixture data root
narrowed to the owner-private shape by [[BPR-0812]]
(`tests/end_to_end/agent_finding_composition_public_path.cpp`, data root
`<tmp>/data`).

Reported by `bpr-win` in `task-20260917-win-hardening`, not carded there.

## Scope

- Make the course name its failure first (the step it stopped at), then fix
  the cause.
- Whatever the cause, a composition course that ends without a named term is
  itself a defect: the course must print the failed step.

## Acceptance

- The contract passes on the Windows station, and the cause is named here.
- It still passes on macOS.
