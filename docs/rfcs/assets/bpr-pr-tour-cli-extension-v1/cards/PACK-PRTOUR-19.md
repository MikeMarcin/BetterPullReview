---
schema: "implement-rfc.card.v1"
id: "PACK-PRTOUR-19"
title: "Document the workflow and simplify the integration"
status: "Backlog"
rfcs: ["RFC-BPR-0044", "RFC-BPR-0045", "RFC-BPR-0046"]
depends_on: ["PACK-PRTOUR-15", "PACK-PRTOUR-17"]
metadata: {"pack": "bpr-pr-tour-cli-extension-v1", "milestone": "verification-closeout", "acceptance": []}
---

# Document the workflow and simplify the integration

## Outcome

Document simple human commands, exact machine contracts, install prerequisites, canonical GitHub versus local/staged state, same-machine handoff, actual limitations and user undo/approval behavior. Remove duplicate parsers/publishers/prompt defaults introduced during development; amend only relevant architecture/privacy/command docs.

## Existing ownership seams

BPR product/build/user/security docs; canonical pr-tour references; imported tour docs support links.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Focused docs/link/command gate passes; examples match shipped help; no obsolete review-tour skill name, parent re-import instructions, hidden metadata dependency or unsupported guarantees remain.

Acceptance ownership: Integration/lineage/closeout slice; no new product criterion is implied.

## Execution boundary

Milestone: **verification-closeout**. This is a pack-local alias, not an allocated live BPR issue. Allocate/remap only extension cards at intake, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
