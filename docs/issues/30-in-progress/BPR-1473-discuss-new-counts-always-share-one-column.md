---
schema: "implement-rfc.card.v1"
id: "BPR-1473"
title: "Discuss's new-post counts always share one column"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1471"]
priority: "P3"
metadata: {"reservation":"docs/issues/_reservations/BPR-1473--BPR-1473--167da41ffd3a.json","origin":"owner request 2026-10-13"}
---

# Discuss's new-post counts always share one column

BPR-1471 lined up the blue new-post counts whether or not a note is
resolved, but an unsent "Your reply" still pushed a note's count left. The
owner asked for the counts to be aligned always.

## Acceptance

- AC1: On every closed note the blue count's right edge is the same, whether
  or not the note is resolved or carries an unsent reply. "Your reply" stands
  left of the count, and a test asserts that geometry.

## Evidence

- AC1: `closed_meta()` places the blue count straight after the resolved
  check's slot and the unsent "Your reply" to its left
  (`conversation_cards_view.cpp`). `test_discuss_new_counts_share_one_column`
  now covers two resolved and two open notes, one of each with an unsent
  reply. It asserts one count rectangle down the list, "Your reply" left of
  the count, and the reply count and faces in one column on notes without an
  unsent reply. The offscreen render `build/dev/bpr-1473/discuss-always-aligned.png`
  shows the four counts in one column.
- Tests: all 14 pass: `qt_conversation_cards_view`,
  `qt_github_conversation_panel`, `qt_feedback_label_threads`,
  `qt_workbench_conversation_reveal`, `qt_conversation_finder_scale_*`,
  `qt_composed_github_scale_*`, `qt_public_preview_conversation`,
  `application_discuss_top_public_path`,
  `application_overview_feedback_public_path` and
  `application_thread_words_public_path`. Changed-line clang-tidy 22 reports
  no findings, and the format check passes.
