---
schema: "implement-rfc.card.v1"
id: "BPR-0995"
title: "Compose manual stop editing and reviewer route navigation"
status: "Done"
rfcs: ["RFC-BPR-0039", "RFC-BPR-0040"]
depends_on: ["BPR-0994"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-05","milestone":"manual-authoring","acceptance":["AC39-03","AC39-06","AC40-08"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
acceptance: [{"criterion":"manual-route","outcome":"pass","revision":"791decea33b6ca3d85c2b89c6f0c3a80b3eb84c0","evidence":{"kind":"command","detail":"Final application_review_tour_composition_public_path passed 20.28s: actual accepted fixture PR, multiple stops in one file, cross-file conceptual order, exact original range/related source, duplicate titles, keyboard reorder, split/merge/duplicate/remove/insert-before and structural undo/redo, retained editor on normal file excursion, exact Return and Ready/next, preview and admitted SQLite application restart."}},{"criterion":"native-bounds-conflict","outcome":"pass","revision":"791decea33b6ca3d85c2b89c6f0c3a80b3eb84c0","evidence":{"kind":"command","detail":"Final composed course preserves a refused 64 KiB+1 editor candidate against Add/Insert/related/row switching; native Undo recovers accepted text. Final application_review_tour_host passed 0.28s including explicit conflict discard after bounds refusal and untouched PR observation without durable slot consumption. Core/store bounds, whole encoded payload and profile/account multiwindow CAS evidence from BPR-0992/0993 reused."}},{"criterion":"zero-review-effects","outcome":"pass","revision":"791decea33b6ca3d85c2b89c6f0c3a80b3eb84c0","evidence":{"kind":"command","detail":"Actual composition asserts zero agent launches and unchanged submission/comment/reply/resolution counters. With admitted auto-mark-read enabled, persisted review-ledger inspection after tour navigation shows no file receipts. Source-unavailable paths retain authoring."}},{"criterion":"layout-integration-upkeep","outcome":"pass","revision":"791decea33b6ca3d85c2b89c6f0c3a80b3eb84c0","evidence":{"kind":"command","detail":"Focused 5-row integration passed 65.50s: Tour base/125%/200%, existing review-submission composition and architecture boundaries. Full-window 1440x1000,960x720,1100x900/18pt images inspected: source and editor visible, wrapped labels, vertical access without horizontal drawer overflow. Final guard/insert corrections do not change layout. Shipped app and affected tests built; documentation gate DOCUMENTATION_OK; git diff --check passed. Cocoa/native screen-reader and live GitHub qualifications remain later obligations."}}]
---

# BPR-0995: Compose manual stop editing and reviewer route navigation

Pack alias: **PACK-TOUR-05** (live `BPR-0995`, allocated at import).

## Outcome and work

Add source-selection Add stop, independent title/body/related-source editing, outline insert/reorder/split/merge/remove, keyboard equivalents and preview. Keep selected code and comment visible together. Add Previous/Next stop and a local Return to tour without marking source reviewed or hiding the normal file/change path.

## Existing ownership seams

`Current source selection/diff navigation, conversation model, workbench window and review composition`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Composed manual route with multiple stops in one file and cross-file conceptual order; excursion/return, keyboard reorder, duplicate titles, retained selection and zero viewed/resolve/approve/publication effects.

Requirement ownership: `AC39-03`, `AC39-06`, `AC40-08`.

## Integration boundary

Milestone: **manual-authoring**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
