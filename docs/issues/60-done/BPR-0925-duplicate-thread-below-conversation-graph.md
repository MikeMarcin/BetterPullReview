---
schema: "implement-rfc.card.v1"
id: "BPR-0925"
title: "Investigate duplicate thread presentation below the conversation graph"
status: "Done"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"}}
acceptance: [{"criterion":"Trace both apparent copies to their source identities and record whether duplication is in data or presentation.","outcome":"pass","revision":"cc038748","evidence":{"kind":"observation","detail":"The duplication was presentation: the compact card/index, graph and tree/detail were projections of the same retained root/post identities. milestone-thread-reading-integration-evidence.md records the integrated identity model."}},{"criterion":"Present one clear thread-reading path with consistent styling, or make the graph/selected-detail relationship explicit without redundant full bodies.","outcome":"pass","revision":"cc038748","evidence":{"kind":"command","detail":"qt_github_conversation_panel passes with one compact PR-wide index, a structural graph and one selected full-body detail. Nonselected model roots are hidden in detail, while the graph carries structure rather than a second body rendering."}},{"criterion":"Long selected threads avoid unexplained fixed-height clipping; graph selection, navigation, resolution state and comment identity remain correct.","outcome":"pass","revision":"cc038748","evidence":{"kind":"command","detail":"Long-detail width/font and workbench scale tests pass; exact reply selection maps to its owning root and graph node, explicit index activation opens detail, and resolution/outdated identities remain stable. Independent correction review reported no remaining finding."}}]
---

# Investigate duplicate thread presentation below the conversation graph

This work belongs to [RFC-BPR-0037](../../rfcs/RFC-BPR-0037-first-pr-review-workflow.md),
criterion AC-08. Earlier RFC references retain their governing contracts.

## Owner feedback

The treeverse-style graph “looks okay,” but below it seemingly the same
review thread is shown again “in a very ugly style.” This second representation
also seems fixed-height, although its scrolling works at normal speed.

Reported after the first loaded PR, following publication of 0.1.3. The exact
running build, PR and environment have not been independently captured.

## Scope and investigation

Identify both rendered surfaces and their underlying thread/comment IDs.
Determine whether the lower view is an intended selected-thread detail, a legacy
fallback, or duplicate data/projection. The duplicate identity is suspected,
not confirmed. Preserve the graph the owner found useful; do not remove it as
a shortcut. Explain any intentional graph/detail relationship and consolidate
body rendering and appearance where appropriate.

Coordinate [[BPR-0920]] and [[BPR-0924]]; compare their layout and input behavior.
Starting points: `src/adapters/qt/src/github_thread_map.cpp`,
`src/adapters/qt/src/github_conversation_panel.cpp`, and
`src/adapters/qt/src/conversation_cards_view.cpp`.

## Acceptance

- Trace both apparent copies to their source identities and record whether duplication is in data or presentation.
- Present one clear thread-reading path with consistent styling, or make the graph/selected-detail relationship explicit without redundant full bodies.
- Long selected threads avoid unexplained fixed-height clipping; graph selection, navigation, resolution state and comment identity remain correct.

## Status

Not started. This intake records feedback and investigation/repair scope only;
no reproduction, root cause, runtime fix or live-service verification is claimed.
