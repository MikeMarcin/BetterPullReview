---
schema: "implement-rfc.card.v1"
id: "BPR-0920"
title: "Revisit review composition placement and Conversations drawer layout"
status: "Done"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"}}
acceptance: [{"criterion":"Identify the reported gap and clipped controls with a reproducible synthetic layout, distinguishing confirmed findings from hypotheses.","outcome":"pass","revision":"fe6ac3633a61bf5fc4abab0057f6546d412c6446","evidence":{"kind":"observation","detail":"conversation-surface-layout-study.md reproduces a 669-pixel empty stacked page with retained content and confirms reviewDraftBody/reviewWorkshopOriginal/reviewWorkshopProposal compressed to 23 pixels against 90-pixel hints inside a 220-pixel editor. The whole-review target did not clip and the owner's original two-box identities remain explicitly unbound."}},{"criterion":"Recommend where thread browsing, local drafting and PR-wide review submission belong and allocate bounded implementation.","outcome":"pass","revision":"fe6ac3633a61bf5fc4abab0057f6546d412c6446","evidence":{"kind":"artifact","detail":"The selected layout keeps contextual threads in the drawer, makes Discuss a PR-wide central surface and moves the existing whole-review workflow to a dedicated in-window Compose review page. BPR-0929 is the canonical coupled runtime follow-up."}},{"criterion":"Proposed layouts give inputs usable space without accidental target wrapping/clipping or unexplained gaps and support keyboard/large-font use.","outcome":"pass","revision":"fe6ac3633a61bf5fc4abab0057f6546d412c6446","evidence":{"kind":"observation","detail":"The study records current 900/1400-pixel and 13/20-point geometry, selects a full main-content page with a short-height outer-scroll fallback, explicit Back/focus order and complete selectable target disclosure, and binds post-change geometry/input checks to BPR-0929."}},{"criterion":"Preserve drafts, explicit publication confirmation, and review type/summary semantics when relocating controls.","outcome":"pass","revision":"fe6ac3633a61bf5fc4abab0057f6546d412c6446","evidence":{"kind":"artifact","detail":"The selected design retains the same editor/workflow instance and explicitly preserves draft/type/cursor/conflict state, exact confirmation, one-use grants, idempotency and reconciliation. Opening, closing and navigation remain non-publishing; runtime verification stays open on BPR-0929."}}]
---

# Revisit review composition placement and Conversations drawer layout

This work belongs to [RFC-BPR-0037](../../rfcs/RFC-BPR-0037-first-pr-review-workflow.md),
criterion AC-03. Earlier RFC references retain their governing contracts.

## Owner feedback

The Read page's Conversations drawer has “a ton of blank space” below
“Show review threads” and above “Review pull request.” Under “Choose a review
type and enter a summary” there is a clipped, line-wrapped box that looks like
the PR link, and the next box appears similarly broken. The owner says too
much review-starting UI is crammed into the narrow drawer, probably does not
belong there, and needs revisiting/redesign.

Reported after the first loaded PR, following publication of 0.1.3. The exact
running build, PR and environment have not been independently captured.

## Scope and investigation

Inventory the actual controls and their jobs before proposing a layout. The
reported boxes' identities are not established: do not assume they are PR links
or summary fields from appearance alone. Inspect minimum sizes, stretch,
visibility, wrapping and vertical allocation to explain both the blank gap and
clipped controls.

Compare moving PR-wide review composition into the central Review surface or
a dedicated composition surface, leaving the drawer focused on conversations.
Produce reviewable layouts for Read, Review and Discuss, narrow/wide windows
and large fonts. Coordinate [[BPR-0923]], [[BPR-0924]], [[BPR-0925]] and
[[BPR-0926]] so the redesign does not preserve duplicate or unexplained UI.

Starting points: `src/adapters/qt/src/github_conversation_panel.cpp`,
`src/adapters/qt/src/review_submission_editor.cpp`, and
`src/app/src/review_submission_workflow.cpp`.

## Acceptance

- Identify the reported gap and both clipped controls with a reproducible synthetic layout; distinguish confirmed findings from hypotheses.
- Recommend where thread browsing, local drafting and PR-wide review submission belong; provide concrete layouts and a bounded implementation follow-up.
- Proposed layouts give text inputs usable space, avoid accidental URL wrapping/clipping and unexplained gaps, and support keyboard/large-font use.
- Preserve drafts, explicit publication confirmation, and review type/summary semantics when relocating controls; no live mutation is needed for investigation.

## Status

Selected design recorded in
[`conversation-surface-layout-study.md`](../../rfcs/plans/rfc-bpr-0037-first-pr-workflow/conversation-surface-layout-study.md):
move the existing whole-review workflow to a dedicated in-window Compose review
page and open inline/reply drafting explicitly, leaving the drawer contextual.
The synthetic composed layout confirms a 220-pixel inline editor against a
429/453-pixel minimum hint and 23-pixel body/workshop fields. The owner's two
reported boxes remain unidentified; the whole-review target did not clip in the
probe. [[BPR-0929]] owns the coupled runtime implementation while preserving
draft and publication authority. AC-03 runtime acceptance remains pending.
