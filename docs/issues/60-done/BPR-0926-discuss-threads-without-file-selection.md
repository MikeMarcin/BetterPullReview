---
schema: "implement-rfc.card.v1"
id: "BPR-0926"
title: "Show conversations in Discuss and investigate a file-independent default"
status: "Done"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0028", "RFC-BPR-0008"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"}}
acceptance: [{"criterion":"A composed PR with known conversations shows them in Discuss or explains a specific loading, filter or failure state.","outcome":"pass","revision":"fe6ac3633a61bf5fc4abab0057f6546d412c6446","evidence":{"kind":"observation","detail":"The synthetic composed probe retained one whole-PR card while an inherited absent-file anchor gated the loaded page behind zero roots; clearing file and line anchors revealed three roots and Discuss page two. This identifies a filter/stack cause without inferring the owner's provider state."}},{"criterion":"Demonstrate entering Discuss without a selected file, recommend the default scope and define optional file filtering.","outcome":"pass","revision":"fe6ac3633a61bf5fc4abab0057f6546d412c6446","evidence":{"kind":"observation","detail":"The probe demonstrates the existing complete PR projection after clearing code anchors. The selected design makes All PR conversations the default and defines independent optional file, Unread and Unresolved filters with visible scope and Clear filters."}},{"criterion":"Preserve clear thread-to-code navigation while discussion remains readable without an available comparison.","outcome":"pass","revision":"fe6ac3633a61bf5fc4abab0057f6546d412c6446","evidence":{"kind":"artifact","detail":"The design separates reading selection from explicit Open in code, preserves exact endpoint guards, leaves outdated/removed/unavailable anchors readable with a specific reason and restores PR-wide scope/selection on return."}},{"criterion":"Exercise top-level and inline content, resolved/outdated threads, empty content, refresh and PR replacement with synthetic evidence without conflating failure and empty.","outcome":"pass","revision":"fe6ac3633a61bf5fc4abab0057f6546d412c6446","evidence":{"kind":"command","detail":"The focused github conversation panel/cards tests passed over synthetic conversation types, filters and stable selection; the study maps top-level comments, review summaries, inline/resolved/outdated content and replacement semantics into distinct initial, empty, filtered, refreshing, failed, unavailable and authority-replacement states. BPR-0929 requires the composed runtime matrix before AC-09 closes."}}]
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

The synthetic composed probe reproduced known retained conversation content
behind an empty page: an inherited absent-file anchor produced zero visible
roots while one whole-PR card remained retained; clearing file and line anchors
revealed the loaded page. The selected design makes Discuss PR-wide by default,
with an explicit optional file scope and distinct loading, empty, filtered,
refreshing, failure and unavailable states. Evidence is in
[`conversation-surface-layout-study.md`](../../rfcs/plans/rfc-bpr-0037-first-pr-workflow/conversation-surface-layout-study.md);
[[BPR-0929]] owns runtime implementation. AC-09 remains pending there.
