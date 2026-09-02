---
id: BPR-0504
title: "Composition: wire findings, chat, workshop, and MCP into the shipped workbench"
status: In Progress
rfc: RFC-BPR-0012
depends_on: [BPR-0500, BPR-0501, BPR-0502, BPR-0503]
---

# Composition: wire findings, chat, workshop, and MCP into the shipped workbench

`src/app/src/github_application.cpp` constructs the new finding host, the MCP
helper lifecycle, the side-chat panel, and the finding-queue and workshop
surfaces beside the existing `AgentHost` construction near line 2493 and the
review-mutation host, and `scripts/verify_composed_hosts.py` gains the new
constructors in `SHARED_REQUIRED`. An end-to-end contract modeled on
`tests/end_to_end/agent_invocation_composition_public_path.cpp` proves a real
gesture reaches a finding and a local draft with zero GitHub mutation, and that
a full agent's GitHub changes return only through RFC-BPR-0008 synchronization.

## Required final state

- The new hosts and panels are constructed in the same place and under the
  same fixture and production split as `AgentHost` and the review-mutation host.
- `SHARED_REQUIRED` names each new constructor, so `composed_application_hosts`
  fails on either binary if one is dropped.
- One real workbench gesture reaches one delivered finding, and converting it
  produces one local draft with zero GitHub mutation calls.
- A full-agent launch's GitHub changes appear only through the RFC-BPR-0008
  fixture synchronization path. There is no second edge from the agent host to
  the review-mutation coordinator.
- Each new host has a construction cut: removing it keeps the gesture
  observable, leaves every downstream effect at zero, and makes the positive
  predicate false.

## Non-goals

- The components themselves — BPR-0497 through BPR-0503.
- Documentation and the matrix rows — BPR-0505.
- The simplification pass and the ledger's closeout sections — BPR-0506.
- Any live GitHub call; the RFC-BPR-0008 fixture path is the surface.

## Required direct evidence

- A new `application_agent_finding_composition_contract` registered like
  `application_agent_invocation_composition_contract` in
  `tests/end_to_end/CMakeLists.txt`; run
  `ctest --preset dev -R
  '^(application_agent_finding_composition_contract|composed_application_hosts|application_review_round_trip_composition_contract)$'
  --output-on-failure`.
- The positive course drives a real gesture through the shipped window to one
  delivered finding and one local draft, with zero GitHub mutation calls.
- The full-agent course proves the resulting GitHub state arrives only through
  RFC-BPR-0008 synchronization; a backchannel would appear as a mutation call
  outside it.
- One construction cut per new host: the gesture still fires, effects stay at
  zero, and the shared positive marker is false.
- `composed_application_hosts` fails when a new anchor is removed by deleting
  its construction, not by editing the verifier.
- `ctest --preset dev -R '^qt_composed_github_scale_100$' --output-on-failure`
  stays green — new drawer content is downstream of the composed window.
