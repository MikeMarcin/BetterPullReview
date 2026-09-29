---
schema: "implement-rfc.card.v1"
id: "BPR-1062"
title: "Connect desktop buffers and same-machine authoring handoff"
status: "Done"
rfcs: ["RFC-BPR-0045"]
depends_on: ["BPR-1060", "BPR-1061"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-11","milestone":"same-machine-handoff","acceptance":["AC45-04","AC45-07"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC45-04","outcome":"pass","revision":"637cd41df1b87748fedbd1003270bebec8c7a45f","evidence":{"kind":"command","detail":"Real desktop owner with a fixture pull request open and the shipped bpr binary (application_profile_tour_handoff_public_path): typed text is read back at once, CLI edits reach the open editor with caret and focus unchanged, independent fields proceed, a duplicate projection applies once, and a CLI reorder, removal and undo keep every stop ID and the author's selected stop; the committed text is durable and readable standalone after the owner drains. Source-bearing commands (add/open) are not forwarded while a desktop owns the profile: BPR-1227. Cross-interface reads of exact source binding, readiness and conflicts rely on the same stored draft bytes and are not re-proved through a separate skill client."}},{"criterion":"AC45-07","outcome":"pass","revision":"637cd41df1b87748fedbd1003270bebec8c7a45f","evidence":{"kind":"command","detail":"IME preedit answers editor_busy for status and edit with nothing changed, then proceeds after commit (with a negative control that fails when the gate is disabled); a stale edit after further typing is revision_conflict with both texts intact; an external-editor session whose owner is killed and replaced saves through the new owner with its captured revision exactly once; foreign selection semantics and request replay are unit-tested. Real IME hardware and another window holding the same draft are not evidenced."}}]
---

# BPR-1062: Connect desktop buffers and same-machine authoring handoff

Pack alias: **PACK-PRTOUR-11** (live `BPR-1062`, allocated at import).

## Outcome

Mount shared draft services in the live desktop tour editors and integrate flush/IME/conflict behavior for forwarded requests. Keep one committed undo authority and preserve active selections/focus. Let CLI external-editor saves re-enter the current owner with captured revisions; keep candidates if any conflict arises.

## Existing ownership seams

Current parent Qt tour editors; application serialized lane; profile desktop fanout; shared edit journal.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

CLI→desktop→skill-command-driver→CLI preserves the same draft with desktop open. Cover preedit, dirty buffers, stale AI, independent fields, structural reorder and duplicate projection; no silent clobber or clipboard/export dance.

Acceptance ownership: AC45-04, AC45-07

## Execution boundary

Milestone: **same-machine-handoff**. Pack alias PACK-PRTOUR-11 was allocated as live `BPR-1062` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
