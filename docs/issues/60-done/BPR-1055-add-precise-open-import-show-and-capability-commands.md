---
schema: "implement-rfc.card.v1"
id: "BPR-1055"
title: "Add precise open, import, show and capability commands"
status: "Done"
rfcs: ["RFC-BPR-0044", "RFC-BPR-0045"]
depends_on: ["BPR-1054"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-04","milestone":"headless-manual","acceptance":["AC44-05"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC44-05","outcome":"pass","revision":"699c619d79c25f9f61eaa3c38462561d2b40d9ec","evidence":{"kind":"command","detail":"tour_cli_composition, sqlite_review_tour_storage, architecture_boundaries and desktop application_review_tour_cold_import_public_path pass; docs ledger records scope. No live PR or credential effects."}}]
---

# BPR-1055: Add precise open, import, show and capability commands

Pack alias: **PACK-PRTOUR-04** (live `BPR-1055`, allocated at import).

## Outcome

Implement CLI-only first-run local profile initialization and explicit connect through existing admitted auth services, plus exact host/profile/account/PR/draft/root resolution, unambiguous human convenience selection, machine explicit IDs, and read/capability/status/show projections. Reuse parent cold importer and return completeness/provenance limits; lazy code context follows pinned source.

## Existing ownership seams

Headless composition; existing GitHub provider/read normalization; parent format/import application path.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Two simultaneous agent sessions cannot redirect one another via last-opened state. Full URL outside checkout, PR-number ambiguity, multiple roots, wrong account and cached/offline cases are explicit and make zero remote writes.

Acceptance ownership: AC44-05

## Execution boundary

Milestone: **headless-manual**. Pack alias PACK-PRTOUR-04 was allocated as live `BPR-1055` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
