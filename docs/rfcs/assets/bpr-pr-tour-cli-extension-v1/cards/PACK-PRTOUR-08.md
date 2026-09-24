---
schema: "implement-rfc.card.v1"
id: "PACK-PRTOUR-08"
title: "Compose terminal publication maintenance and read-first recovery"
status: "Backlog"
rfcs: ["RFC-BPR-0044"]
depends_on: ["PACK-PRTOUR-04", "PACK-PRTOUR-05", "PACK-PRTOUR-07"]
metadata: {"pack": "bpr-pr-tour-cli-extension-v1", "milestone": "publish-and-recover", "acceptance": ["AC44-08", "AC44-09", "AC44-10"]}
---

# Compose terminal publication maintenance and read-first recovery

## Outcome

Expose existing finite publication and maintenance plans through preview/publish/recover/resume. Bind exact actor/head/target, surface staged versus published visibility, retain real child IDs/receipts, and use current native/admitted gh transports. Implement human-terminal confirmation; unsupported agent approval modes return interaction_required without writes.

## Existing ownership seams

Parent RFC0041/0042 session/journal/admission; current provider/native/gh CLI adapters; headless host.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Fixture remote counters show zero writes from all local/read commands. Kill before/after send and acknowledgement, recover twice, and verify no duplicate publication. Wrong account, unknown gh platform, missing vault and unscoped bypass refuse honestly.

Acceptance ownership: AC44-08, AC44-09, AC44-10

## Execution boundary

Milestone: **publish-and-recover**. This is a pack-local alias, not an allocated live BPR issue. Allocate/remap only extension cards at intake, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
