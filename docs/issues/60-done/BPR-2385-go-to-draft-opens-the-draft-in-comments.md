---
schema: "implement-rfc.card.v1"
id: "BPR-2385"
title: "Go to draft opens the draft in Comments, a closed draft reads as one, and a report does not restate an updated comment"
status: "Done"
rfcs: []
depends_on: ["BPR-2376", "BPR-2375"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","defect":true,"mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner, attended review 374cb080088e516421011d3a88d707e0, rejecting BPR-2376's look, 2026-11-24","reservation":"docs/issues/_reservations/BPR-2385--BPR-2385--2d69d8af3394.json","related":["BPR-2376","BPR-2375","BPR-2258","BPR-2092","BPR-1590","BPR-1773","BPR-2158","BPR-2352"],"acceptance":["BPR-2385:go","BPR-2385:card","BPR-2385:page","BPR-2385:format"]}
---

# BPR-2385: Go to draft opens the draft, a closed draft reads as one, and a report does not restate an updated comment

## The owner's words

Attended review `374cb080088e516421011d3a88d707e0`, rejecting [[BPR-2376]]'s
look:

"How come when I clicked goto draft it didnt switch to the comments tab? Then
goto draft went to the comment but didn't select it, it was still collapsed
so it was hard to even tell it was the draft. I think collapsed draft might
need a more substantial visual than the little draft in top right corner.
Maybe a border? This updated comment section at the bottom is more confusing
than helpful. It's just restating the thing that's drawn above it."

## Cause

- Go to draft. A report finding's Go to draft (`report_link::go`) and an
  Agent answer's went to the finding's *lines* (`go_to_file_line`), never to
  the draft: the right tray stayed on whatever tab it showed, and the
  file chosen again closed the draft's composer, so the draft was left a
  closed note. The window already had one route that goes to a draft
  ([[BPR-2092]], `open_roster_draft`, used by a Threads row and Finish your
  review's list); neither Go to draft used it, and from a report it would
  have left the report in front.
- The closed draft. Its frame was dashed in the hairline's grey
  (`Rule2`) and its "Draft" was in the faint ink, so the note read as one
  more published comment.
- The report. [[BPR-2376]] draws an updated finding from its update (the
  Suggested change, through `core::revise_report`), and the page still wrote
  the whole updated comment under the keep row as "Suggested comment,
  updated:", its suggestion in a second box under the one above it.

## Change

Go to draft (`src/adapters/qt/src/workbench_window_drafts_and_behaviour.cpp`,
`workbench_window_comments_and_threads.cpp`, `workbench_window.hpp`;
`src/app/src/github_application/window_quick_ask.cpp`,
`window_agent_setup.cpp`):

- A report finding's Go to draft names the draft its Draft comment made (the
  finding's kept record) and an Agent answer's names the draft it wrote
  (`agent_answer_draft::Drafted`); both call
  `workbench_window::open_roster_draft`. A draft the pull request no longer
  lists falls back to its lines, as before.
- The route arrives alike from everywhere: the file's own tab in front, the
  right tray on Comments with the drawer open, the draft open in its
  composer and holding the keyboard, whole in the drawer by [[BPR-2375]]'s
  reveal (the composer opens through `begin_draft`).
- From a report (`reveal_pending_comment`): the file behind the report has
  its tab made active as it stands; another file is opened as a line gone to
  from a report opens it. The line is chosen only once the report is gone
  and the file's tab has its own reading back (`pending_comment_pick::FromReport`,
  resumed from the tab projection): a tab made active is given the line it
  remembered, and a line chosen sooner was given up to it, closing the draft.
- The line is revealed, not only selected, so a long file scrolls to it. A
  line gone to earlier and still waited for gives way. A draft whose line
  shows before the drawer lists it now waits for it instead of being
  dropped with a status message.

The closed draft (`src/adapters/qt/src/conversation_cards_view.cpp`, `.hpp`):

- My closed draft is the composer shut: a card on a wash of the writing blue
  (the colour the open composer is edged in), inside a dashed frame in that
  blue at 1.5 px on whole pixels, with its pencil and "Draft" in that ink at
  the name's weight. Dashed is "not sent", as a draft's mark is in Threads
  and the gutter. Pending in my review, not sent and earlier head wear the
  same card with their own words.
- A Local comment wears the same card in the quiet ink its open card is
  edged in, with the computer and "Local".
- The wash gives way to the words: it is reduced until the state's words
  reach the theme's text floor on it, so a high contrast theme takes little
  or none. Under the pointer the wash deepens; the keyboard's cursor keeps
  its ring, with the dashes inside it.
- `draft_tag_rect` says where the state is drawn (for tests). No height or
  layout changes.

The report (`src/adapters/qt/src/report_page.cpp`, `report_page.hpp`):

- An updated comment is not written out under its finding, and nor is the
  comment it replaced. One line under the keep row says what is so: "Draft
  comment drafts the comment as the follow-up updated it." (or Keep local)
  for a finding not yet kept; "The draft differs from the updated suggested
  comment. Nothing is published" beside Update draft for a draft the
  reviewer changed; nothing more for a draft in step with the update.
- A changed draft's row is its three controls (Go to draft, Update draft,
  Ask follow-up); "Drafted." and the sentence stand under them, whole at any
  report width. Beside them, in a report narrowed by its side chat, each had
  a letter's width.
- A finding no follow-up updated keeps its "Suggested comment:" and its fix
  box ([[BPR-2352]]).

Docs: `docs/architecture/review-tours.md` (the item side chat section).

## Acceptance

- **BPR-2385:go** Go to draft switches the right tray to Comments, opens the
  draft in its composer whole in the drawer, and gives it the keyboard, from
  a report finding and from an Agent answer.
- **BPR-2385:card** A closed draft and a closed Local comment stand on their
  own ground inside a dashed frame that reaches the theme's floor for a
  mark, and say what they are in ink that reaches the text floor, in every
  built-in theme.
- **BPR-2385:page** A report finding whose comment a follow-up updated shows
  no restatement of it; the Suggested change shows once; the draft-differs
  state and Update draft remain.
- **BPR-2385:format** The format check passes on the changed C++ files and
  clang-tidy reports nothing on a changed line.

## Verification

macOS arm64, dev preset (Debug), offscreen, fixture agent only. Evidence
revision `6b4f8900b`: the lane's one commit, rebased onto main `c8f13e21b`
(BPR-2381 to BPR-2384). The rebase conflicted only in the production app
manifest pin's notes; both notes are kept and the pin recomputed. Rerun at
`6b4f8900b`: the three courses, `qt_conversation_cards_view`,
`qt_item_side_chat`, `qt_agent_report_view`, `qt_workbench_window_scale_100`,
the composition contract and the format check. The other "Not broken" rows
and clang-tidy ran before the rebase, at `200541987` on main `30276994b`.

| Criterion | Check | Result |
|---|---|---|
| BPR-2385:go | `application_report_draft_reveal_public_path`: inline-drawer#22, the fixture agent's review, Draft comment on the first finding. Then, three times, the draft is closed with its card's Close, the right tray turned to the Agent tab and the report put in front; Go to draft is pressed from the draft's own line, from another line of its file (line 3 selected) and from another file (`src/ui/widgets/button.cpp` behind the report). Each time: the file's tab is in front, the tray shows Comments, the draft is open in its composer at 84 to 528 of a 566 px drawer with its words all in view, and the keyboard is inside its card. Nothing is published | pass |
| BPR-2385:go | `application_agent_keep_action_public_path`: an Agent answer's Draft on selected lines, then with the tray on the Agent tab, the draft closed and another line selected, its Go to draft turns the tray to Comments and opens the draft at its line, whole in the drawer, with the keyboard | pass |
| BPR-2385:go, BPR-2385:page | `application_follow_up_public_path`: after an update is used, the page says once "Draft comment drafts the comment as the follow-up updated it." and holds neither "Suggested comment, updated" nor the update's words; a finding no follow-up updated keeps "Suggested comment:"; a drafted finding's update shows its lines in the Suggested change and not again in the page; a changed draft says it differs and offers Update draft, and after Update draft says neither; then Go to draft from that updated finding, the draft closed and the tray on the Agent tab, opens the draft holding the update in Comments with the keyboard | pass |
| BPR-2385:card | `qt_conversation_cards_view` > `test_my_closed_draft_reads_as_a_draft`, in Light, Dark, High Contrast Light and High Contrast Dark: a draft, a pending draft and a Local comment each have a state tag inside their note, a dashed frame at or above the theme's floor for a mark against the drawer, and state words at or above the text floor on their ground, at rest and under the pointer; a draft's frame is one whether pending or not and (outside the high contrast themes) not a Local comment's; a published note has no ground, frame or tag. Measured at 100%: Light frame 5.18:1 and "Draft" 4.89:1 (4.69:1 under the pointer); Dark 5.19:1 and 5.00:1 (4.69:1); High Contrast Light 21:1 and 17.13:1; High Contrast Dark 8.12:1 and 7.38:1 (7.15:1) | pass |
| BPR-2385:page | `qt_item_side_chat` > `test_a_report_finding_offers_ask_follow_up`, `test_an_updated_comment_moves_the_suggested_change`: an updated finding says once that Draft comment (or Keep local) takes the update and writes out neither comment; drafted and in step it says nothing more; the Suggested change shows once; a changed draft's "Drafted." and reason are one line under its row | pass |
| Reproduced | The new report course on the old source (main `30276994b`): "its own line: Go to draft turns the right tray to Comments; it shows Agent". The old page under an updated finding: `before/follow-up-draft-differs-page.png` | fail, as the owner saw |
| Not broken | At `6b4f8900b`: `qt_workbench_window_scale_100`, `qt_agent_report_view`. Before the rebase: `qt_composed_inline_drawer_scale_100/125/200`, `qt_composed_github_scale_100`, `qt_drawer_surfaces_fit_scale_100`, `qt_github_conversation_panel`, `application_review_report_public_path`, `core_boundary_contract` | pass |
| Reseal | `application_review_round_trip_composition_contract` with the production app manifest pin `17f9b28a3489b998d37fdf29678b3d9e8b98b695856568c40babda355ceb344e` (over BPR-2384's reseal); the print mode reports the same value; the full application and mutation-branch pins are unchanged | pass |
| BPR-2385:format | `_coding-standard/scripts/format.py` (clang-format 22) on the 14 changed C++ files, at `6b4f8900b`: nothing to change. `_coding-standard/scripts/tidy.py` on the 11 changed translation units, before the rebase: no finding on a changed line | pass |

Renders (offscreen, 1440x720 window for the report course):

- Before: `goto-draft-renders/before-baseline/report-draft-go-to-draft.png`
  (the closed draft as it was), `goto-draft-renders/before/report-go-to-draft.png`
  (after the press on the old source: the tray still on the Agent tab),
  `goto-draft-renders/before/follow-up-draft-differs-page.png` and
  `follow-up-report-page.png` (the restated comment).
- After: `goto-draft-renders/after/report-go-to-draft.png`,
  `report-go-to-draft-other-line.png`, `report-go-to-draft-other-file.png`
  (each with its `-before.png`, the state the press was made in),
  `follow-up-go-to-draft.png`, `follow-up-report-page.png`,
  `follow-up-draft-differs-page.png`, `follow-up-draft-in-step-page.png`,
  and `closed-drafts-100/` and `closed-drafts-200/` (the four themes, at
  rest and under the pointer).

## Limits

- A report finding kept local (Go to comment) still goes to its lines only;
  it does not turn the tray to Comments or open the Local comment's card.
- In a report narrowed by its side chat, the keep row of the other states
  still wraps its sentence a letter to a line ("at renderer.cpp:11. Nothing
  is published"); only the changed-draft row was moved under its controls.
- No owner look was prepared in this round.
