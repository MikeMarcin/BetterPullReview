---
schema: "implement-rfc.card.v1"
id: "BPR-1059"
title: "Compose terminal publication maintenance and read-first recovery"
status: "Ready"
rfcs: ["RFC-BPR-0044"]
depends_on: ["BPR-1055", "BPR-1056", "BPR-1058"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-08","milestone":"publish-and-recover","acceptance":["AC44-08","AC44-09","AC44-10"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
---

# BPR-1059: Compose terminal publication maintenance and read-first recovery

Pack alias: **PACK-PRTOUR-08** (live `BPR-1059`, allocated at import).

## Outcome

Expose existing finite publication and maintenance plans through preview/publish/recover/resume. Bind exact actor/head/target, surface staged versus published visibility, retain real child IDs/receipts, and use current native/admitted gh transports. Implement human-terminal confirmation; unsupported agent approval modes return interaction_required without writes.

## Existing ownership seams

Parent RFC0041/0042 session/journal/admission; current provider/native/gh CLI adapters; headless host.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Fixture remote counters show zero writes from all local/read commands. Kill before/after send and acknowledgement, recover twice, and verify no duplicate publication. Wrong account, unknown gh platform, missing vault and unscoped bypass refuse honestly.

Acceptance ownership: AC44-08, AC44-09, AC44-10

## Execution boundary

Milestone: **publish-and-recover**. Pack alias PACK-PRTOUR-08 was allocated as live `BPR-1059` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
