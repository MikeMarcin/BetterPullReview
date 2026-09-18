---
schema: "implement-rfc.card.v1"
id: "BPR-0920"
title: "Revisit review composition placement and Conversations drawer layout"
status: "Backlog"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"}}
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

Not started. This intake records feedback and investigation/repair scope only;
no reproduction, root cause, runtime fix or live-service verification is claimed.
