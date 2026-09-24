---
schema: "implement-rfc.card.v1"
id: "PACK-PRTOUR-11"
title: "Connect desktop buffers and same-machine authoring handoff"
status: "Backlog"
rfcs: ["RFC-BPR-0045"]
depends_on: ["PACK-PRTOUR-09", "PACK-PRTOUR-10"]
metadata: {"pack": "bpr-pr-tour-cli-extension-v1", "milestone": "same-machine-handoff", "acceptance": ["AC45-04", "AC45-07"]}
---

# Connect desktop buffers and same-machine authoring handoff

## Outcome

Mount shared draft services in the live desktop tour editors and integrate flush/IME/conflict behavior for forwarded requests. Keep one committed undo authority and preserve active selections/focus. Let CLI external-editor saves re-enter the current owner with captured revisions; keep candidates if any conflict arises.

## Existing ownership seams

Current parent Qt tour editors; application serialized lane; profile desktop fanout; shared edit journal.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

CLI→desktop→skill-command-driver→CLI preserves the same draft with desktop open. Cover preedit, dirty buffers, stale AI, independent fields, structural reorder and duplicate projection; no silent clobber or clipboard/export dance.

Acceptance ownership: AC45-04, AC45-07

## Execution boundary

Milestone: **same-machine-handoff**. This is a pack-local alias, not an allocated live BPR issue. Allocate/remap only extension cards at intake, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
