---
schema: "implement-rfc.card.v1"
id: "BPR-0926"
title: "Show conversations in Discuss and investigate a file-independent default"
status: "In Progress"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0028", "RFC-BPR-0008"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"}}
---

# Show conversations in Discuss and investigate a file-independent default

This work belongs to [RFC-BPR-0037](../../rfcs/RFC-BPR-0037-first-pr-review-workflow.md),
criterion AC-09. Earlier RFC references retain their governing contracts.

## Owner feedback

“The Discuss tab doesn't show any conversation threads. I realize also that
discuss would probably be useful without files, and maybe that's even a more
sensible default for it.”

Reported after the first loaded PR, following publication of 0.1.3. The exact
running build, PR and environment have not been independently captured.

## Scope and investigation

Treat this as two related requirements: diagnose why existing threads are
absent, and evaluate a PR-wide Discuss entry that does not require selecting a
file. Trace loading, filters, current selection and projection before attributing
the empty view to absent provider data. The file-independent default is an owner
suggestion for design, not an already accepted layout.

Compare a PR-wide thread/conversation view with an optional file filter and a
clear route from an anchored thread to its code. Decide how top-level PR comments,
review summaries, inline threads, resolved threads and outdated anchors appear.
Keep local/private content provenance intact. Coordinate [[BPR-0923]] so an
unexplained Show review threads gesture is not a hidden prerequisite.

Starting points: `src/adapters/qt/src/workbench_window.cpp`,
`src/adapters/qt/src/github_conversation_panel.cpp`, and the conversation model
and host projections. Coordinate the overall arrangement with [[BPR-0920]].

## Acceptance

- A composed PR with known conversation threads shows them in Discuss or explains a specific loading/filter/failure state; no unexplained empty surface.
- Investigate and demonstrate entering Discuss without a selected file, recommend the default scope and define optional file filtering.
- Preserve clear thread-to-code navigation while allowing PR-wide discussion reading when code comparison is unavailable.
- Exercise top-level and inline content, resolved/outdated threads, empty PRs, refresh and PR replacement with synthetic fixtures; do not conflate loading failure with no conversations.

## Status

Not started. This intake records feedback and investigation/repair scope only;
no reproduction, root cause, runtime fix or live-service verification is claimed.
