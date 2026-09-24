---
schema: "implement-rfc.card.v1"
id: "PACK-PRTOUR-15"
title: "Test agent attention provenance authority and adversarial inputs"
status: "Backlog"
rfcs: ["RFC-BPR-0046"]
depends_on: ["PACK-PRTOUR-13", "PACK-PRTOUR-14"]
metadata: {"pack": "bpr-pr-tour-cli-extension-v1", "milestone": "pr-tour-skill", "acceptance": ["AC46-07", "AC46-08", "AC46-09"]}
---

# Test agent attention provenance authority and adversarial inputs

## Outcome

Add hostile-source and conversation-boundary tests for read≠visit, agent-relayed current-content completion, request-scoped rewrites, outlined skeletons, explicit publish, no manufactured authority and no provider-policy bypass. Keep required host/provider verification separate from claimed text metadata.

## Existing ownership seams

Shared CLI command admission and parent publication guard; skill command trace harness; assistance context policy.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Attempts to batch-ready, pipe yes, call gh directly, set fake approval/provider, return another target or leak private notes fail in the relevant boundary. Interrupted skill sessions recover shared drafts; unsupported approval preserves terminal fallback.

Acceptance ownership: AC46-07, AC46-08, AC46-09

## Execution boundary

Milestone: **pr-tour-skill**. This is a pack-local alias, not an allocated live BPR issue. Allocate/remap only extension cards at intake, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
