---
schema: "implement-rfc.card.v1"
id: "BPR-1066"
title: "Test agent attention provenance authority and adversarial inputs"
status: "Ready"
rfcs: ["RFC-BPR-0046"]
depends_on: ["BPR-1064", "BPR-1065"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-15","milestone":"pr-tour-skill","acceptance":["AC46-07","AC46-08","AC46-09"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
---

# BPR-1066: Test agent attention provenance authority and adversarial inputs

Pack alias: **PACK-PRTOUR-15** (live `BPR-1066`, allocated at import).

## Outcome

Add hostile-source and conversation-boundary tests for read≠visit, agent-relayed current-content completion, request-scoped rewrites, outlined skeletons, explicit publish, no manufactured authority and no provider-policy bypass. Keep required host/provider verification separate from claimed text metadata.

## Existing ownership seams

Shared CLI command admission and parent publication guard; skill command trace harness; assistance context policy.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Attempts to batch-ready, pipe yes, call gh directly, set fake approval/provider, return another target or leak private notes fail in the relevant boundary. Interrupted skill sessions recover shared drafts; unsupported approval preserves terminal fallback.

Acceptance ownership: AC46-07, AC46-08, AC46-09

## Execution boundary

Milestone: **pr-tour-skill**. Pack alias PACK-PRTOUR-15 was allocated as live `BPR-1066` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
