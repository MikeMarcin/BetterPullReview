---
schema: "implement-rfc.card.v1"
id: "BPR-1060"
title: "Persist shared draft history and presentation-neutral resumption"
status: "Done"
rfcs: ["RFC-BPR-0045"]
depends_on: ["BPR-1056", "BPR-1057"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-09","milestone":"same-machine-handoff","acceptance":["AC45-03","AC45-08"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC45-03","outcome":"pass","revision":"012edc856ddae35249721f6e995ce8ad73e1c545","evidence":{"kind":"command","detail":"Four focused public CLI/host/real-store courses pass; legacy migration, process COMMIT rollback, exact reopen, monotonic cross-host undo/redo, no-op/bounds/current conflicts and origin/resumption. Existing publication-journal and profile purge boundaries unchanged. DOCUMENTATION_OK."}},{"criterion":"AC45-08","outcome":"pass","revision":"012edc856ddae35249721f6e995ce8ad73e1c545","evidence":{"kind":"command","detail":"Four focused public CLI/host/real-store courses pass; legacy migration, process COMMIT rollback, exact reopen, monotonic cross-host undo/redo, no-op/bounds/current conflicts and origin/resumption. Existing publication-journal and profile purge boundaries unchanged. DOCUMENTATION_OK."}}]
---

# BPR-1060: Persist shared draft history and presentation-neutral resumption

Pack alias: **PACK-PRTOUR-09** (live `BPR-1060`, allocated at import).

## Outcome

Extend the existing admitted tour store/journal so committed text/structure history, conflict candidates, exact source/readiness/baselines, private origin and minimal section/stop hints survive CLI/desktop/skill switches. Use monotonic revisions and bounded reversible history; no new cloud or shadow file format.

## Existing ownership seams

Parent local tour schema; profile-private store admission/migration; revisioned_review_draft; shared edit journal.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Real-store migrate/reopen/rollback, account/profile isolation, cross-interface undo-back-to-equal-text, no-op history, bounds and purge. Ordinary save/switch never uploads. Unresolved publication records cannot be evicted with undo history.

Acceptance ownership: AC45-03, AC45-08

## Execution boundary

Milestone: **same-machine-handoff**. Pack alias PACK-PRTOUR-09 was allocated as live `BPR-1060` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
