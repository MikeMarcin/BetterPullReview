---
schema: "implement-rfc.card.v1"
id: "BPR-1698"
title: "A thread picked into a long file that is not open shows its line"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1698--BPR-1698--443572af2f9b.json","origin":"owner bug report 2026-10-21","owner_note":"if I'm in the threads view and there's a comment and I click review and that file isn't open and that comment is way down the page. In this case it's like change four on line 3000 when the file opens it's scrolled to line one instead of making the comment I tried to go to visible.","regressing_commit":"7778886c9","related_cards":["BPR-1264","BPR-1293","BPR-1383","BPR-1582","BPR-1682"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"ac5cf6d8c","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_workbench_conversation_reveal$'","detail":"test_a_pick_into_a_long_unopened_file_reveals_its_line, through the real core's transitions: Review shows a 30-line file; src/big.cpp (3,200 lines, four changes, the fourth at line 2,991) is not open. In Threads a click puts the cursor on the deep thread's row and a click on its Review button opens it in Review; every click and key goes to the window through QWindowSystemInterface. The host answers in the composition's order (placement, loading, publication, placement in one turn), so the 6,400-line comparison is staged and adopted turns later. Once it shows, the thread is open at its post, the surface's conversation anchor is right line 2,990 (zero-based) and that line is in view while line 1 is not. A Return pick of a left-side thread on the same, open file reveals left line 2,001 at once; after a pick away to the short file, a Return pick back into the prepared long file reveals line 2,991 again. Failing-before control: with src/adapters/qt/src/workbench_window.cpp at the parent commit the case fails at 'the thread's line is the comparison's conversation anchor'. macOS offscreen at 1x."}},{"criterion":"AC2","outcome":"pass","revision":"ac5cf6d8c","evidence":{"kind":"command","command":"ctest --preset dev -R '^(command_correctness_registry_contract|qt_workbench_conversation_reveal|qt_workbench_line_draft|qt_thread_moves_scale_100|qt_thread_scroll_stability_scale_100|qt_workbench_window_scale_(100|125|200))$'","detail":"The existing picks (test_threads_finds_and_picks_into_review, Discuss's Open in code and Open in Review, the Threads Discuss route), line drafts, thread moves and thread scroll stability pass, and the full window run passes at 1x, 1.25x and 2x. The new case joins --conversation-reveal, so tests/command_correctness_registry.json and tests/workbench_case_inventory.json carry the new source pin and case. coding-format-check (clang-format 22.1.8) passes; clang-tidy --warnings-as-errors on workbench_window.cpp and workbench_window_tests.cpp reports only the two existing performance-avoid-endl findings in the test runner's case trace (b059676cc), none in this change. macOS, a Debug configuration of the dev preset built without debug information (-g0) to fit the disk."}}]
---

# BPR-1698: A thread picked into a long file that is not open shows its line

## Owner note

The owner, 2026-10-21, verbatim:

> if I'm in the threads view and there's a comment and I click review and
> that file isn't open and that comment is way down the page. In this case
> it's like change four on line 3000 when the file opens it's scrolled to
> line one instead of making the comment I tried to go to visible.

## Cause

A pick from Threads ([[BPR-1293]], and Discuss's Open in code since
[[BPR-1383]]) selects the file and keeps the thread as the window's pending
pick. `resume_conversation_pick` resolves it once the file's comparison
shows: it opens the thread's card and makes the picked post the conversation
view's current row. It never revealed the line itself. It relied on the
view's current change, whose handler calls `synchronize_selected_conversation`,
and so `reveal_conversation_line`, unless `ApplyingConversationAnchor` is set.

A comparison of more than 4,096 lines on its two sides is filled over
several turns and adopted only then ([[BPR-1264]]). The composition places the
conversation endpoints straight after the surface takes the publication,
while the long file is still staged, so the pick still waits. The adoption
then reaches the window through the surface's selection observer, which sets
`ApplyingConversationAnchor` while it re-projects the drawer and refreshes the
cards. The refresh resolves the pick under that guard, the current change
reveals nothing, and the file opens where its own selection puts it. A short
file is adopted at once, and the composition's placement afterwards
synchronizes the chosen post, which is why the existing picks, all on
30-line files, passed.

The defect came with the pending pick in 7778886c9 ([[BPR-1293]], 2026-10-06),
a day after ac0d9d1d4 ([[BPR-1264]]) made long comparisons adopt later.

## Change

`resume_conversation_pick` reveals the picked post's line itself, once: it
chooses the post with the guard held, so the current change does not reveal
it a second time, then calls `synchronize_selected_conversation` for the post.
The surface keeps the reveal through the file's measurement as before: its
conversation anchor makes the rebuilt geometry reveal that line. The pick
keeps its identity: a newer pick replaces it, and another file's comparison
arriving first drops it. What a pick may reveal (the comparison, the side's
current endpoint, the semantic anchor) is decided as before. The pick is the
window's presentation state, not core state, and the routing stays in
`core::route_workbench_thread`.

Split and commit-filtered comparisons (RFC-BPR-0089) go through the same
resolution and the same placement policy; anchoring under filters is that
RFC's M4 and is not changed here.

## Acceptance

- **AC1.** From Threads, the Review button on a thread at change four, line
  2,991 of a 3,200-line file that is not open, opens the file with that line
  in view and the thread open at its post. A left-side thread on the open
  file and a pick back into the prepared file are revealed too. The case
  fails before the change.
- **AC2.** The existing reveal, line draft, thread move and thread scroll
  courses and the full window run are unchanged.

## Verification

- `qt_workbench_conversation_reveal` runs the new
  `test_a_pick_into_a_long_unopened_file_reveals_its_line`; it fails at the
  parent commit's window source as recorded in AC1. Its clicks and keys go
  to the window in its points, so like [[BPR-1682]]'s case it runs in its
  selector at 1.0 only.
- `qt_workbench_line_draft`, `qt_thread_moves_scale_100`,
  `qt_thread_scroll_stability_scale_100` and
  `qt_workbench_window_scale_{100,125,200}` pass.
- `tests/command_correctness_registry.json` and
  `tests/workbench_case_inventory.json` are resealed for the new case;
  `command_correctness_registry_contract` passes.
- `coding-format-check` (clang-format 22.1.8) passes; clang-tidy on the
  changed units as recorded in AC2.

macOS offscreen only. The change is Qt widget state with no platform branch;
Windows and Linux station runs are not gating.
