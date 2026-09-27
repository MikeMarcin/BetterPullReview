---
schema: "implement-rfc.card.v1"
id: "BPR-1053"
title: "Expose the shared tour service facade and semantic commands"
status: "Done"
rfcs: ["RFC-BPR-0044", "RFC-BPR-0045"]
depends_on: ["BPR-1052"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-02","milestone":"headless-manual","acceptance":["AC44-02"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC44-02","outcome":"pass","revision":"434b34af8","evidence":{"kind":"command","detail":"application_review_tour_host and architecture_boundaries pass in core-only; exact typed owner/draft/revision service executes the parent host and store; desktop primary editor invokes it directly. Existing observe_review_tours and staged publication services remain sole importer/publisher; public CLI composition follows in BPR-1054."}}]
---

# BPR-1053: Expose the shared tour service facade and semantic commands

Pack alias: **PACK-PRTOUR-02** (live `BPR-1053`, allocated at import).

## Outcome

Identify/extract only the tour orchestration seam needed by CLI and desktop. Define typed command/result and target/revision DTOs; separate read/edit/assist/publication effects. Keep parent importer, anchors, staging session and recovery journal authoritative. Route desktop directly to shared services, not through a child CLI.

## Existing ownership seams

src/core; src/application; parent 0039–0042 tour host/services; existing review_submission_* application ports.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Core/host tests drive equivalent desktop/CLI semantic commands and show one importer/publisher. Boundary scan shows no Qt/effect dependency in core and no parallel business logic in rendering/parsing.

Acceptance ownership: AC44-02

## Execution boundary

Milestone: **headless-manual**. Pack alias PACK-PRTOUR-02 was allocated as live `BPR-1053` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
