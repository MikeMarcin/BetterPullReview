---
schema: "implement-rfc.card.v1"
id: "BPR-0826"
title: "The agent invocation composition contract expects application session 1 and sees 2"
status: "In Progress"
rfcs: ["RFC-BPR-0012"]
depends_on: []
priority: "P2"
platforms: ["mac", "win"]
metadata: {"found_by":"bpr-win under BPR-0622 (pre-existing), confirmed on macOS by the dsp-002 integration run","found_at":"2ba02ddbe59bf6e07302cbb7460f61cc9cad2627","run":"run-20260916-stations","dispatch":"dsp-002"}
---

# BPR-0826: The agent invocation composition contract expects application session 1 and sees 2

## Defect

`application_agent_invocation_composition_contract` fails the same way on
Windows (`bpr-win`, while working on BPR-0622) and on macOS at `2ba02ddbe59bf6e07302cbb7460f61cc9cad2627`
(`build/r2-logs/dsp2-int4-ctest.log`). The seed's exact-context evidence
matches (`agent-exact-context-evidence:1:1:1:1:1:1:1:1:163:163`), but the
context shows `application_session 2`.
`tests/end_to_end/agent_invocation_composition_public_path.cpp` (~1555)
requires `ApplicationSession == application_session_id{ 1U }`.

On Windows, putting BPR-0622's three app conditions back to `Q_OS_UNIX`
gives the same failure, so the failure predates the Windows slices. Every
other `application_agent_invocation_*` course passes on macOS.

## Scope

- Find the first bad revision on macOS and the mechanism. What now opens a
  second application session before the invocation? Candidates include
  [[BPR-0705]]'s multiple windows over one profile runtime, window
  restoration, and startup recovery on the effect lane.
- If the second session is intended, change the course to derive the
  expected session from the composition. Do not hard-code 2. Cite the
  decision. Otherwise fix the product.

## Acceptance

- The contract passes on macOS, and the cause is named here.
- The Windows station reruns it and records the result.
