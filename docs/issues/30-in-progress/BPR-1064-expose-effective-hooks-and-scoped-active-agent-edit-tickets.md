---
schema: "implement-rfc.card.v1"
id: "BPR-1064"
title: "Expose effective hooks and scoped active-agent edit tickets"
status: "In Progress"
rfcs: ["RFC-BPR-0046"]
depends_on: ["BPR-1056", "BPR-1059", "BPR-1062"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-13","milestone":"pr-tour-skill","acceptance":["AC46-05","AC46-06"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
---

# BPR-1064: Expose effective hooks and scoped active-agent edit tickets

Pack alias: **PACK-PRTOUR-13** (live `BPR-1064`, allocated at import).

## Outcome

Expose assist context/complete and outline proposal/use through the CLI using the parent four hooks and result DTOs. The active agent supplies output; BPR launches no nested model. Tickets bind fixed local target/range/revision/source/hook/session, survive normal short-process pairing, and refuse stale/superseded/ambiguous output.

## Existing ownership seams

Parent RFC0043 settings/context/result validation; shared text transaction; CLI contract.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

UTF-8 range/CRLF/emoji, repeated text, source/field/hook change, owner switch, explicit cancellation, no-op, invalid result and ABA tests. Only selected text changes as one undoable edit; outline bodies/ready directives reject. No model call from a read.

Acceptance ownership: AC46-05, AC46-06

## Execution boundary

Milestone: **pr-tour-skill**. Pack alias PACK-PRTOUR-13 was allocated as live `BPR-1064` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
