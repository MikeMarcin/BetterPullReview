---
schema: "implement-rfc.card.v1"
id: "BPR-1057"
title: "Integrate trusted external editors and local undo redo"
status: "In Progress"
rfcs: ["RFC-BPR-0044", "RFC-BPR-0045"]
depends_on: ["BPR-1056"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-06","milestone":"headless-manual","acceptance":["AC44-07"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
---

# BPR-1057: Integrate trusted external editors and local undo redo

Pack alias: **PACK-PRTOUR-06** (live `BPR-1057`, allocated at import).

## Outcome

Open one Markdown field through explicit trusted editor configuration or file/stdin input. Use private safe temp projections, release profile ownership while editing, validate save against exact original revision, retain conflict candidates, and expose one shared committed undo/redo transaction per imported edit. Preserve parent in-place rewrite semantics.

## Existing ownership seams

CLI presentation; existing process/temp/private-store facilities; parent tour text transaction services.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Editor exit/cancel/crash, changed field while editor open, symlink substitution, invalid Unicode, filenames with spaces, no-shell interpolation and exact undo/redo. Test no model/network calls and no dangling runtime owner while waiting.

Acceptance ownership: AC44-07

## Execution boundary

Milestone: **headless-manual**. Pack alias PACK-PRTOUR-06 was allocated as live `BPR-1057` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
