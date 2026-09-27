---
schema: "implement-rfc.card.v1"
id: "BPR-1061"
title: "Add bounded authenticated owner forwarding for tour commands"
status: "In Progress"
rfcs: ["RFC-BPR-0045"]
depends_on: ["BPR-1054", "BPR-1060"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-10","milestone":"same-machine-handoff","acceptance":["AC45-05","AC45-06"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
---

# BPR-1061: Add bounded authenticated owner forwarding for tour commands

Pack alias: **PACK-PRTOUR-10** (live `BPR-1061`, allocated at import).

## Outcome

Extend existing profile-owner IPC with a distinct finite tour protocol, native peer/profile/owner verification, bounded frames, command admission and durable local request dedupe. Forward CLI commands to the running desktop rather than reopen its database. Keep fixed activation compatibility and no TCP or mandatory daemon.

## Existing ownership seams

profile_runtime_lease and profile_activation APIs; profile desktop ownership; headless command host; platform native sockets/pipes.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Real native channels and isolated fixtures cover owner death, duplicate ID/different payload, commit-before-lost-response, stale endpoint, wrong user/profile, version mismatch, overflow and cancellation. No lock stealing or direct second-writer fallback.

Acceptance ownership: AC45-05, AC45-06

## Execution boundary

Milestone: **same-machine-handoff**. Pack alias PACK-PRTOUR-10 was allocated as live `BPR-1061` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
