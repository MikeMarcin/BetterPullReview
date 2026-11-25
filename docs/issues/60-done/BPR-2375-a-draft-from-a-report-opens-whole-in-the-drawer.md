---
schema: "implement-rfc.card.v1"
id: "BPR-2375"
title: "A draft made from a report opens whole in the Comments drawer, not cut off at its foot"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","defect":true,"reservation":"docs/issues/_reservations/BPR-2375--BPR-2375--331c96d74b36.json","origin":"owner report with a photo, 2026-11-24: \"Draft comment on a report barely puts the draft on screen in the comments drawer\"","related_cards":["BPR-2258","BPR-2158","BPR-2173","BPR-1740","BPR-1293"],"acceptance":["AC1","AC2","AC3","AC4","AC5"]}
---

# BPR-2375: A draft made from a report opens whole in the Comments drawer, not cut off at its foot

## The owner's words

"Draft comment on a report barely puts the draft on screen in the comments
drawer."

The photo: the report in front; in the Comments drawer beside it an earlier
draft at line 234 and, under it, the new draft's composer ("Line 235",
"question: Can a streaming-enabled legacy-grid sender reach a cloud-edit
RCC?") at the drawer's very foot, its editor half cut off and its controls
out of view, with empty drawer above.

## Cause

Beside the code the Comments drawer hangs each note from its line
([[BPR-1290]]); the composer being written is pinned at its line, and only
at the drawer's *top* is it kept whole ([[BPR-2158]]). Draft comment on a
report ([[BPR-2258]]) makes the draft behind the report through the window's
Comment gesture, which opens the composer (`conversation_cards_view::begin_draft`)
on the finding's line. `begin_draft` then brought only the *editor* into
view, *at once*, with `ensure_visible`: the editor was still empty, the
finding's words were written into it a moment later by the promotion, and
the card's foot (the label row, "What it changes", Start a review) was never
considered. The card grew past the drawer's foot after the only reveal, and
nothing brought it back. Measured on the old code: in the cards view course
a composer on a line 640 px down a 700 px drawer stood at 586 to 802; in the
new public-path course the report's draft stood at 155 to 599 in a 566 px
drawer.

## Change

`src/adapters/qt/src/conversation_cards_view.cpp`, `conversation_cards_view.hpp`:

- `whole_card_move` and `grown_card_move` (pure, constexpr, in the header)
  decide how far the drawer's content moves. A card whole in the band clear
  of the edge chips stays; a cut one rests with its top a third of the way
  down, or higher when its foot would otherwise fall past the band, never
  above the band (a card taller than the drawer shows its head and editor).
  A grown card moves the least that keeps its foot in view, never its top out.
- `begin_draft` no longer calls `ensure_visible` on the editor. It arms a
  reveal: one step at once, and another each time the drawer has held still
  for 120 ms (every placement of the open card restarts the wait); the reveal
  ends once the settled card is whole. Bounded by 1.5 s and eight moves. The move goes through
  `move_notes`, factored out of `reveal_reading`: beside the code the code
  moves and the drawer follows it, so the composer still hangs at its line;
  only what the code cannot take is the notes' own overflow ([[BPR-1740]]).
- A reader's own scroll ends the reveal: a wheel over the drawer, the open
  card or the draft's editor (one a field or a latched gesture takes
  included), a press, a scroll bar action. Nothing pulls the card back after
  that.
- When the composer grows while the reader types in it (a key typed in its
  editor within the last second), its foot is kept in view by the notes' own
  scroll, the least move, never its top out; only if it was whole in view
  before it grew: scrolled away from, it is left where the reader put it.
  Words presented to it, not typed (a draft read back, a longer one shown),
  do not move the drawer; `qt_composed_inline_drawer`'s long-draft momentum
  check holds the notes still there and caught the first version of this
  change, which kept any growth in view.
- Focus is untouched: the editor keeps the keyboard when the gesture gave it
  the editor; from the report it stays on the report ([[BPR-2258]]).

Entry points: every path that opens the drawer's composer goes through
`begin_draft`: Draft comment on a report and on a finding card (Draft all
too), Draft on an Agent answer, a side-chat proposal promoted to a draft
(`promote_private_item`), Comment on a code line (C, Return, the context
menu), a draft opened from a pending row, the review sheet, the draft roster,
Threads or a tour stop (`finish_pending_comment_pick` opens the draft card,
whose choice is `begin_draft`), Edit on a saved draft and reopening a draft in
the drawer. The reply written in place (`begin_reply`) and a picked post
(`open_thread_at`) keep their own reveals.

## Acceptance

- AC1. A draft made from a report finding whose composer would hang past the
  drawer's foot is whole in the drawer's view (editor and controls), with
  the report in front.
- AC2. The composer still hangs at its line: the code moves, not the notes'
  own overflow, where the code can move.
- AC3. A reader who scrolls the drawer away is not pulled back.
- AC4. A composer whole in view keeps its foot in view as it grows while
  written, and the keyboard stays in its editor.
- AC5. The format check passes on every changed C++ file and clang-tidy
  reports nothing on a changed line.

## Verification

macOS arm64, dev preset (Debug), lane `build/dev` at REVISION (lane commit
`4ce2a2511`, rebased onto main `fc4a7f6cc`, which carries the BPR-2374
fixture fix, BPR-2372 and BPR-2373). The rebase was clean. On the
committed tree: `application_report_draft_reveal_public_path`,
`qt_conversation_cards_view`, `qt_composed_inline_drawer_scale_100/125/200`
and `qt_composed_github_scale_100` pass. The earlier rows marked "before the
rebase" ran on main `9cbc5b604` plus this change with BPR-2364's two source
commits reverted in the working tree only, because the fixture courses could
not load a file there.

| Criterion | Check | Result |
|---|---|---|
| AC1 | `application_report_draft_reveal_public_path` (new): inline-drawer#22, the fixture agent's Review this pull request, Draft comment on the first finding from the report in a 1440x720 window; the composer at 80 to 524 of a 566 px drawer (84 to 528 before the rebase), the notes' own scroll 0; after Go to draft the draft's note is in view | pass |
| AC1, AC2, AC3, AC4 | `qt_conversation_cards_view` › `test_a_drafted_finding_opens_whole_in_view`: the decision's cases; a composer opened empty on a line 640 px down a 700 px drawer, its words arriving after, rests whole a third of the way down with its line at its notch and no overflow, the editor focused; a wheel away is not undone; with its foot near the drawer's foot, four typed lines keep its foot in view and the keyboard in its editor; longer words presented, not typed, leave it where it was | pass |
| Reproduced | Before the rebase, both courses on the old `conversation_cards_view.cpp`: "top 586, foot 802 of 700, code moved 0" and "card top 155, foot 599 of a 566 px drawer" | fail, as the owner saw |
| Not broken | On the committed tree: `qt_conversation_cards_view` (whole suite), `qt_composed_inline_drawer_scale_100/125/200`, `qt_composed_github_scale_100`. Before the rebase: also `qt_workbench_window_scale_100`, `application_review_report_public_path`, `application_agent_keep_action_public_path` | pass |
| AC5 | `_coding-standard/scripts/format.py` (clang-format 22) on the changed C++ files; `_coding-standard/scripts/tidy.py` on `conversation_cards_view.cpp` and the new course (no finding on a changed line) | pass |

Not run: Windows and Linux, a full `ctest`. The documentation gate could
not run in the lane's worktree (`ModuleNotFoundError: board_model`, the
MASK-deployed implement-rfc scripts are not in a worktree). No owner look was prepared in this round.
