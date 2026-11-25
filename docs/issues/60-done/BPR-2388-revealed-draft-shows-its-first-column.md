---
schema: "implement-rfc.card.v1"
id: "BPR-2388"
title: "A draft gone to shows its first column, and a report does not restate a Suggested change under its suggested comment"
status: "Done"
rfcs: []
depends_on: ["BPR-2385", "BPR-2354"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","defect":true,"mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"found while preparing attended review 5627b21a56838de9fae9f2ac4e4349d4, 2026-11-24","reservation":"docs/issues/_reservations/BPR-2388--BPR-2388--738e616ffdca.json","related":["BPR-2385","BPR-2376","BPR-2375","BPR-2354","BPR-2353","BPR-2352","BPR-2331"],"acceptance":["BPR-2388:field","BPR-2388:arrival","BPR-2388:page","BPR-2388:format"]}
---

# BPR-2388: A draft gone to shows its first column, and a report does not restate a Suggested change

## What was seen

Preparing attended review `5627b21a56838de9fae9f2ac4e4349d4`, in
`application_follow_up_public_path` under the native look:

- Go to draft on a report finding whose draft the reviewer had edited opened
  the draft's editor panned sideways. Its words read "ssue: clamp both sides
  where the extent is / nade.", and its suggestion's box showed the tail of
  its line ("/idth, 1), std::max(aSize.Height, 1) });").
- A finding no follow-up updated still showed "Suggested comment:" with a
  plain code box of its suggestion's lines, directly under the Suggested
  change drawn from those same lines. [[BPR-2385]] removed that restatement
  for an updated comment (owner: "It's just restating the thing that's drawn
  above it").

## Cause

The editor. Three things met:

- Qt's plain text layout reports the document's width as its widest laid-out
  line plus 8 px once it lays that line's block out again
  (`QPlainTextDocumentLayout::blockWidth`), which it does after every change
  of width. Words that wrap within 8 px of the field's edge therefore gave
  the comment field a hidden sideways range of up to 8 px. Its bar is off,
  so nothing showed it.
- A suggestion's caret is put at the end of its last line ([[BPR-2353]]). In
  a line wider than its box ([[BPR-2354]]) that is far to the right of the
  field.
- `QWidgetTextControl::setTextCursor` asks the field to show the caret before
  it reports the move, so `QPlainTextEdit` panned the field to the caret, by
  all the range it had, before the block's own box scrolled to it.

In the course the field was 260 px wide and its first line 257 px: range 5,
panned 5, first letter at -5. Nothing was wrong with the wrap mode, and no
scroll was kept from an earlier width; the range was made afresh at each
width.

The box showing its line's tail was the same caret rule: text arriving
scrolled the block to the caret at the line's end.

The report. `comment_html` drew every fenced block of a suggested comment as
a box ([[BPR-2352]]), including the `suggestion` block the finding's
Suggested change is drawn from ([[BPR-2376]]).

## Change

The field (`src/adapters/qt/src/composer_code_layout.cpp`, `.hpp`,
`conversation_open_card.cpp`):

- `composer_code_layout::documentSize` reports a document no wider than the
  field. Words wrap at the field's width and a block's code scrolls in its
  own box, so nothing is wider than the field and it has no sideways range
  at any width.
- Text that arrives in the field, not typed (`present_draft`,
  `present_reply`), shows each fenced block from its lines' start. The caret
  keeps the place [[BPR-2353]] gives it.
- A key pressed in the field brings the caret into its block's box, a key
  that leaves the caret where it is included (End at the end of the line). A
  modifier alone does not. A caret moved by keyboard or mouse scrolls the
  block as before.

The report (`src/adapters/qt/src/report_page.cpp`):

- Under a finding that draws a Suggested change, a suggested comment whose
  suggestion holds those same lines (`core::comment_suggestion_lines`) shows
  its words and not the block again. Its label reads "Suggested comment,
  with the suggested change above:", so the comment does not read as words
  only. A comment that is only its suggestion is one line: "The suggested
  comment is the suggested change above."
- Kept as they were: a fix written in the file's language keeps its box
  under the words (owner, 2026-11-21: "The code block didn't appear in the
  suggested comment", accepted 2026-11-22 in session
  `772f868fd02ed420c8889a2d055f5fab`), because nothing above it says the
  comment carries it; a suggestion that is not the one drawn keeps its box;
  a second block after the drawn suggestion keeps its box.

Docs: `docs/architecture/review-tours.md` (the item side chat section).

## The design choice, for the report

The Suggested change box is the comment's suggestion, drawn better than the
plain box drew it: the removed and the added lines, numbered, in the file's
colours. A second box of the added lines alone gave the reviewer nothing
new, and it is what the owner called a restatement one state later. It is
removed. The words are the one thing the finding does not already show, so
they stay. The label takes six more words because the owner's earlier
rejection was a comment that seemed to have lost its code: the page says the
change goes with the words instead of drawing it twice.

## Acceptance

- **BPR-2388:field** The comment field has no sideways range and shows its
  words from their first letter at every drawer width, with the caret at the
  end of a code line wider than the field.
- **BPR-2388:arrival** A draft gone to shows its suggestion's line from its
  start; the caret is at the end of that line, and a key or a caret move
  brings it into sight in its box without moving the field.
- **BPR-2388:page** A finding no follow-up updated shows its suggested
  comment's words once and its suggestion once, as the Suggested change; a
  fix in the file's language and a suggestion not drawn above keep their
  box.
- **BPR-2388:format** The format check passes on the changed C++ files and
  clang-tidy reports nothing on a changed line.

## Verification

macOS arm64, dev preset (Debug), fixture agent only, nothing published.
Evidence revision `7d534425d`: the lane's one commit, rebased onto main
`38cd69f38` (BPR-2387) without conflict. Rerun at `7d534425d`: the first six
rows below and the documentation gate. The other "Not broken" rows and
clang-tidy ran before the rebase, on the same source for every file this
card changes.

| Criterion | Check | Result |
|---|---|---|
| BPR-2388:field, BPR-2388:arrival | `qt_conversation_cards_view` > `test_a_revealed_draft_shows_its_first_column`: a draft of "issue: clamp both sides where the extent is made." and a suggestion of one 78-character line. Opened, and closed and begun again: the caret is at the end of the suggestion's line, the field's range and position are 0, its first letter is at 0 or right of it, and the block's bar is at 0 of a range above 0. Shift alone leaves the block; the end-of-line key shows the caret in the box. Gone to while open, the block stays where it was scrolled. Then each of 49 drawer widths (300 to 348 px): no range after the resize, and with the caret put at the line's end the block scrolls to it and the field stays. 14 of the 49 widths wrap within 8 px of the field's edge, and the test fails if none does | pass |
| BPR-2388:field, BPR-2388:arrival | `application_follow_up_public_path`: after Go to draft from the updated finding, the editor's range and position are 0 with its first letter in view, and the suggestion's bar is at 0 of a range above 0; the end-of-line key shows the caret in its box; then the conversations seam is dragged through 17 positions (`move_handle`, more than 8 distinct editor widths required) and at each the caret put at the line's end moves the box and not the field | pass |
| BPR-2388:page | `application_follow_up_public_path`: the finding no follow-up updated says "Suggested comment, with the suggested change above:" and "Clamp both."; `const auto height = std::max(aSize.Height, 1);` is in one code box (the Suggested change) and not in the page's text | pass |
| BPR-2388:page | `qt_item_side_chat` > `test_a_suggested_comment_does_not_restate_its_suggested_change`: words before and after the suggestion stay and its lines show once, offered and drafted; a comment of only its suggestion is one line; a suggestion with other lines keeps its box under "Suggested comment:"; a `cpp` fix keeps its box (its lines twice, as accepted); with a drawn suggestion and a second block, only the first is left out | pass |
| BPR-2388:page | `qt_agent_report_view` (the `cpp` fix in its own plain box, the comment without code) | pass |
| Not broken | `qt_drawer_surfaces_fit_scale_100`, `application_report_draft_reveal_public_path` | pass |
| Not broken | Before the rebase: `qt_drawer_surfaces_fit_scale_125/200`, `qt_composed_inline_drawer_scale_100/125/200`, `qt_workbench_window_scale_100`, `application_agent_keep_action_public_path`, `application_review_report_public_path` | pass |
| Reproduced | The course's new checks on the unfixed source, under the native look with expectations reported and not thrown (a local probe, not committed): "the draft gone to shows its words from their first letter: range 5, at 5, first letter at -5 of 260"; "its suggestion's line ... shows from its start: at 223 of 224"; 3 px wider "range 2, at 2"; 39 px wider "range 6"; and both report checks. Offscreen, unfixed, the course stops at the report check. A Qt probe of the same draft at 320 px: range 0 as opened, range 5 after one resize, position 5 with the caret at the line's end | fail, as seen |
| Documentation | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |
| Pins | No pinned source changed: the production app manifest pin covers `src/app` only. Nothing resealed | n/a |
| BPR-2388:format | `_coding-standard/scripts/format.py --dry-run` (clang-format 22.1.8) on the 7 changed C++ files at `7d534425d`: nothing to change. `_coding-standard/scripts/tidy.py` on the 6 changed translation units, before the rebase: no finding on a changed line (those it reports are older: `report_page.cpp:80`, `:83`, the cards test's `theirs`, the course's `type_prefix` and line 235) | pass |

Renders, under `draft-editor-renders/` in the session scratchpad. Cocoa with
windows kept off screen, QMacStyle; the course at 1440x720. The course has
no native startup of its own, so these were drawn with a local, uncommitted
change that adds it; under it the three expectations about keyboard focus
and the active window do not hold, a window kept off screen being never
active, and every other expectation passes.

- Go to draft, the Comments drawer cut from the window:
  `go-to-draft-before-light.png`, `go-to-draft-after-light.png`,
  `go-to-draft-after-dark.png`. Whole windows:
  `before/macos-cocoa-light/follow-up-go-to-draft.png`,
  `after/macos-cocoa-light/follow-up-go-to-draft.png`,
  `after/macos-cocoa-dark/follow-up-go-to-draft.png`.
- The finding's suggested comment, cut from the report page:
  `suggested-comment-before-light.png`, `suggested-comment-after-light.png`,
  `suggested-comment-after-dark.png`. Whole pages:
  `<run>/follow-up-report-page-on-ground.png` (the page widget's transparent
  grab composited on the report's own ground, read from the report's grab of
  the same run).
- The cards view alone: `before/macos-cocoa-light-cards-view/` (the unfixed
  probe) and `after/macos-cocoa-light-cards-view/`.

## Limits

- A fix written in the file's language still shows twice in a finding, in
  its body and under "Suggested comment:". That is the look the owner
  accepted on 2026-11-22 and it is left alone here.
- A draft whose text has just arrived has its caret out of sight at the end
  of a long suggestion line until a key is pressed or the caret is moved.
- After the drawer is narrowed, a caret at the end of a long code line can
  be outside its box until a key is pressed. That was so before this card.
- In a report narrowed by its side chat the keep row still wraps its
  sentence a letter to a line ([[BPR-2385]]'s limit).
- The committed course runs offscreen only. The native draws need the local
  change above.
- No owner look was prepared in this round. The label's wording and the
  removal are a keep-or-change judgment for the owner.
