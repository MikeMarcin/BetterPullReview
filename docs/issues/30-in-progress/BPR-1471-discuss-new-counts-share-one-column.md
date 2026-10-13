---
schema: "implement-rfc.card.v1"
id: "BPR-1471"
title: "Discuss's new-post counts share one column"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
metadata: {"reservation":"docs/issues/_reservations/BPR-1471--BPR-1471--f569dda6851d.json","origin":"owner request 2026-10-13"}
acceptance: []
---

# Discuss's new-post counts share one column

The owner reported on 2026-10-13: "the blue circles dont always line up on
the discuss tab depending on if the green thing is there or not. It looks
ugly when they arent all aligned."

On Discuss, each closed note's meta row is laid out from the right edge. A
resolved note draws the green resolved check there first. The blue new-post
count and everything to its left (the reply count and the repliers' faces)
then start 20 px further left than they do on an unresolved note, so the
blue counts of a mixed list do not line up.

## Acceptance

- AC1: On every closed note that is not a draft, the blue new-post count
  stands at the same horizontal position whether or not the note carries the
  green resolved check. The reply count and the repliers' faces do the same.
- AC2: A Qt test asserts that geometry across resolved and unresolved notes.
- AC3: Before and after offscreen renders of the Discuss notes exist in the
  build tree. They are not committed.

## Evidence

- Cause: `paint_closed` in `conversation_cards_view.cpp` drew the resolved
  check at the meta row's right edge and took its 14 + 6 px from the row
  only on a resolved note, so the blue count, the reply count and the faces
  of a resolved note started 20 px left of an unresolved note's.
- AC1: `conversation_cards_view::closed_meta` (`closed_note_meta`) computes
  the row once. Every closed note that is not a draft keeps the check's slot,
  and `paint_closed` draws the check and the count from it. Drafts keep their
  own "Pending" meta, unchanged.
- AC2: `test_discuss_new_counts_share_one_column` (`qt_conversation_cards_view`,
  also `--discuss-alignment`) lists two resolved and two unresolved notes in
  Discuss, each with a new reply. It asserts that only the resolved notes
  draw the check, that every blue count has the same left edge and width,
  that the replies' right edge is the same, and that the check sits right of
  the count.
- AC3: `BPR_DISCUSS_ALIGNMENT_PNG` writes an offscreen render. The before
  render, built from the previous source, shows the resolved notes' counts
  20 px left. The after render shows one column. Both are in the build tree
  under `build/dev/bpr-1471/` and are not committed.
- Tests: 18 Qt tests pass (`qt_conversation_cards_view`,
  `qt_github_conversation_panel`, `qt_conversation_finder_scale_*`,
  `qt_feedback_label_threads`, `qt_composed_github_scale_*`,
  `qt_workbench_conversation_reveal`, `qt_workbench_window_scale_*`,
  `qt_bundled_code_font`, `qt_public_preview_conversation`,
  `qt_composed_markdown_*`). Three end-to-end tests also pass:
  `application_discuss_top_public_path`,
  `application_overview_feedback_public_path` and
  `application_thread_words_public_path`.
- Gates: the format check passes, changed-line clang-tidy reports no
  findings, and `verify_documentation.py` passes.
