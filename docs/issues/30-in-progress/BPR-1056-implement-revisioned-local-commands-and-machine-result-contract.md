---
schema: "implement-rfc.card.v1"
id: "BPR-1056"
title: "Implement revisioned local commands and machine result contract"
status: "In Progress"
rfcs: ["RFC-BPR-0044", "RFC-BPR-0045"]
depends_on: ["BPR-1053", "BPR-1054", "BPR-1055"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-05","milestone":"headless-manual","acceptance":["AC44-04"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
---

# BPR-1056: Implement revisioned local commands and machine result contract

Pack alias: **PACK-PRTOUR-05** (live `BPR-1056`, allocated at import).

## Outcome

Implement typed local edit/add/move/remove/ready commands, finite schema/command discovery, lossless string IDs, expected field/structure revisions and the shared committed transaction boundary. Human output is default; explicit --json emits one bounded envelope. Add typed noninteractive/error behavior and safely paged reads.

## Existing ownership seams

Shared tour facade; existing profile/private draft store; new CLI parser/printer adapter; Glaze DTO style.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Golden human/JSON output, large integer IDs, malformed/extra fields, EOF, missing TTY, revision conflicts and oversize inputs. No JSON by default, no prompt in machine mode, no ready-to-publication escalation.

Acceptance ownership: AC44-04

## Execution boundary

Milestone: **headless-manual**. Pack alias PACK-PRTOUR-05 was allocated as live `BPR-1056` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
