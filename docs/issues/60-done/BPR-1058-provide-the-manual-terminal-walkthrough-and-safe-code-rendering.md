---
schema: "implement-rfc.card.v1"
id: "BPR-1058"
title: "Provide the manual terminal walkthrough and safe code rendering"
status: "Done"
rfcs: ["RFC-BPR-0044"]
depends_on: ["BPR-1057"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-07","milestone":"headless-manual","acceptance":["AC44-03","AC44-06","AC44-11"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC44-03","outcome":"pass","revision":"9c1c9447463d838e7306e3911316c28b8802dd76","evidence":{"kind":"command","detail":"Local authoring/rehearsal portion: real PTY at 24 columns exercises Problem/Solution/add/move/walk/observed Ready/preview/remove/EOF without model or remote mutation capabilities. Publication and actual human qualification remain later milestone work."}},{"criterion":"AC44-06","outcome":"pass","revision":"9c1c9447463d838e7306e3911316c28b8802dd76","evidence":{"kind":"command","detail":"git_haves_trimmed_pack_tour-source passes actual shared epoch/cache/catalog source through installed Git; current added, left, historical, unchanged, Unicode, missing, out-of-range and dirty-checkout cases."}},{"criterion":"AC44-11","outcome":"pass","revision":"9c1c9447463d838e7306e3911316c28b8802dd76","evidence":{"kind":"command","detail":"CLI process and composition tests pass inert OSC/CSI/control/bidi display, raw JSON, bounded pages, no color and 24-column Unicode terminal interaction; pager absence is explicit."}}]
---

# BPR-1058: Provide the manual terminal walkthrough and safe code rendering

Pack alias: **PACK-PRTOUR-07** (live `BPR-1058`, allocated at import).

## Outcome

Implement task-oriented Add/Move/Remove/Walk/Ready and preview. Show current comment beside bounded numbered pinned code/diff; allow navigation/reorder without AI. Source and prose ranges are different types. Neutralize untrusted terminal control sequences and expose no-color/narrow-width/pager behavior.

## Existing ownership seams

CLI presentation adapter; shared source-range/epoch ports and tour semantic events.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Complete AI-free problem→solution→stops rehearsal in a raw terminal. Test left/historical/current/unchanged fallback destinations, dirty worktree mismatch, Unicode paths, escape injection and missing source. Printing JSON is not a human visit.

Acceptance ownership: AC44-03, AC44-06, AC44-11

## Execution boundary

Milestone: **headless-manual**. Pack alias PACK-PRTOUR-07 was allocated as live `BPR-1058` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
