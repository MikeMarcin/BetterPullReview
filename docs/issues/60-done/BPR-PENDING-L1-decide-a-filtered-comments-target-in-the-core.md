---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-L1"
title: "Decide where a comment on a commit-filtered diff goes, and which threads it marks, in the core"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-1685"]
priority: "P1"
platforms: ["mac","win","linux"]
metadata: {"milestone":"RFC-BPR-0089 M4","related_cards":["BPR-PENDING-L2","BPR-PENDING-L3"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c44429b78d91a42b463ca924a743c56141e28a8a","evidence":{"kind":"command","command":"cmake --preset core-only && cmake --build --preset core-only --target test_filtered_comment test_commit_filter test_source_search_policy test_better_pull_review_window_review_restoration_core_tests && ctest --preset core-only -R '^core_(filtered_comment|commit_filter|source_search_policy|window_review_restoration)$'","detail":"core_filtered_comment, on BPR-Test #116's shapes: with C3 hidden a right line C3 leaves unchanged goes to the head at the head's line when it lies within three lines of a change of the merge base to the head, and is refused (outside_github_diff) otherwise; a line C3 changed goes on C1 at its own line when it lies inside a hunk of the merge base to C1 (not_on_commit otherwise); a left line of a diff whose left side is not the merge base is GitHub's LEFT line in the merge base when the merge base has it unchanged and it lies inside a hunk of the merge base to the head on that side (base_line_changed, outside_github_diff otherwise); a range is refused where a line moves; a side that is not of the accepted first-parent chain (or its fork or the merge base on the left), a chain that does not end at the head and a diff of one commit are refused (outside_pull_request); lines not yet read are pending. filtered_thread_line marks a right thread whose commit_id or original_commit_id is the diff's right commit, at its current or original line, and a LEFT thread only when the left side is the merge base. All under core-only, without Git or Qt. macOS."}}]
---

# BPR-PENDING-L1: Decide where a comment on a commit-filtered diff goes, and which threads it marks, in the core

RFC-BPR-0089 M4, the core decisions under owner decision Q1 and the thread
markers (RFC AC08, M4's part).

## Change

- `bpr/core/filtered_comment.hpp` (new). `line_correspondence` reduces one
  computed text diff to the runs of lines it leaves unchanged
  (`correspondence_of`), with `corresponding_right_line`,
  `corresponding_left_line` and `line_in_change_context` (GitHub's three lines
  of context).
- `place_filtered_comment` decides from `filtered_comment_source` (head, merge
  base, the diff's two commits, the chain and its fork, and four
  correspondences the host reads): `head`, `earlier_commit` or `refused` with
  a reason.
- `filtered_thread_line` decides where a thread is marked on a filtered diff.

## Measurement it rests on

BPR-Test #116 (2026-10-21), recorded in the RFC's engineering questions: an
earlier `commit_id` at a line a later commit changed is accepted and
outdated at once; a line no later commit changed is moved to the head by
GitHub; a `LEFT` line counts in the merge base; a line outside every hunk is
refused with 422. #117 then showed a line moved to the head must lie inside a
hunk of the merge base to the head.

## Acceptance

- **AC1.** The decisions run under `core-only`, without Git or Qt.
