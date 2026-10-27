---
schema: "implement-rfc.card.v1"
id: "BPR-1070"
title: "Document the workflow and simplify the integration"
status: "Backlog"
rfcs: ["RFC-BPR-0044", "RFC-BPR-0045", "RFC-BPR-0046"]
depends_on: ["BPR-1066", "BPR-1068"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-19","milestone":"verification-closeout","acceptance":[],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
---

# BPR-1070: Document the workflow and simplify the integration

Pack alias: **PACK-PRTOUR-19** (live `BPR-1070`, allocated at import).

## Outcome

Document simple human commands, exact machine contracts, install prerequisites, canonical GitHub versus local/staged state, same-machine handoff, actual limitations and user undo/approval behavior. Remove duplicate parsers/publishers/prompt defaults introduced during development; amend only relevant architecture/privacy/command docs.

## Existing ownership seams

BPR product/build/user/security docs; canonical pr-tour references; imported tour docs support links.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Focused docs/link/command gate passes; examples match shipped help; no obsolete review-tour skill name, parent re-import instructions, hidden metadata dependency or unsupported guarantees remain.

Acceptance ownership: Integration/lineage/closeout slice; no new product criterion is implied.

## Execution boundary

Milestone: **verification-closeout**. Pack alias PACK-PRTOUR-19 was allocated as live `BPR-1070` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.

## Bundled CLI amendment (2026-10-27)

The owner adopted [bundled-cli-r1](../../rfcs/amendments/RFC-BPR-0044-bundled-cli-r1.md). Documentation and skill setup now target `BetterPullReview.app/Contents/MacOS/bpr`, PATH-first/standard-bundle lookup with one retained absolute path, explicit user-managed shell setup and the automation-off exclusion. The source build remains a developer fallback. This amendment work does not bypass the BPR-1068 dependency or claim the rest of this card complete.
