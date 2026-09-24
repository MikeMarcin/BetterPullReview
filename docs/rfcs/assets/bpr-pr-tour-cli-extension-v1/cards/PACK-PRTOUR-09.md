---
schema: "implement-rfc.card.v1"
id: "PACK-PRTOUR-09"
title: "Persist shared draft history and presentation-neutral resumption"
status: "Backlog"
rfcs: ["RFC-BPR-0045"]
depends_on: ["PACK-PRTOUR-05", "PACK-PRTOUR-06"]
metadata: {"pack": "bpr-pr-tour-cli-extension-v1", "milestone": "same-machine-handoff", "acceptance": ["AC45-03", "AC45-08"]}
---

# Persist shared draft history and presentation-neutral resumption

## Outcome

Extend the existing admitted tour store/journal so committed text/structure history, conflict candidates, exact source/readiness/baselines, private origin and minimal section/stop hints survive CLI/desktop/skill switches. Use monotonic revisions and bounded reversible history; no new cloud or shadow file format.

## Existing ownership seams

Parent local tour schema; profile-private store admission/migration; revisioned_review_draft; shared edit journal.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Real-store migrate/reopen/rollback, account/profile isolation, cross-interface undo-back-to-equal-text, no-op history, bounds and purge. Ordinary save/switch never uploads. Unresolved publication records cannot be evicted with undo history.

Acceptance ownership: AC45-03, AC45-08

## Execution boundary

Milestone: **same-machine-handoff**. This is a pack-local alias, not an allocated live BPR issue. Allocate/remap only extension cards at intake, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
