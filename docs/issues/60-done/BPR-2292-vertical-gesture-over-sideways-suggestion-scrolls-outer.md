---
schema: "implement-rfc.card.v1"
id: "BPR-2292"
title: "A vertical gesture over a suggestion that scrolls sideways scrolls the area that holds it"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2292--BPR-2292--79feca23da2c.json","found_by":"owner, 2026-11-19","related_cards":["BPR-2271","BPR-2273","BPR-2279"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8814e0518","evidence":{"kind":"command","detail":"qt_nested_scroll_latch: over either of an Agent tab report's two wide suggestions, a phased gesture moves the report down and back up (owner: its scroll area), a mouse wheel moves it, a sideways gesture moves the suggestion alone; under cocoa (temporary run) the same with the real 0..227 reader range"}},{"criterion":"AC2","outcome":"pass","revision":"8814e0518","evidence":{"kind":"command","detail":"qt_nested_scroll_latch: a reader that declines vertical movement though its bar has a range passes a vertical gesture on both ways and keeps a sideways one"}},{"criterion":"AC3","outcome":"pass","revision":"8814e0518","evidence":{"kind":"command","detail":"qt_nested_scroll_latch: at its top a field hands a new upward gesture to the outer area and keeps a downward one; at its end (BPR-2279) a new downward gesture moves the outer area; a non-in-place body hands a notch on at its end and its top and keeps one it can take"}},{"criterion":"AC4","outcome":"pass","revision":"8814e0518","evidence":{"kind":"command","detail":"fails without the latch fix (outer 0) and without the body fix; qt_conversation_cards_view, qt_agent_report_view, qt_markdown_body_*, qt_composed_markdown_*, qt_discuss_comment_bodies, qt_github_conversation_panel, qt_pull_request_overview_page, application_overview_summary_1, qt_workbench_conversation_reveal, qt_workbench_window_scale_100, qt_public_preview_conversation, qt_suggestion_editor_1_0, architecture_boundaries; format (clang-format 22.1.8) and clang-tidy on the changed files; verify_documentation"}}]
---

# BPR-2292: A vertical gesture over a suggestion that scrolls sideways scrolls the area that holds it

## Observed

Owner, 2026-11-19: "There's something broken on the scrolling code. I have a
agent tab review with a couple suggestions. If my mouse is over a suggestion
that has horizontal scroll I can't scroll the outer area down."

And: "Vertical scroll areas that are at their limit should let you initiate a
scroll of the outer too if scrolling in the direction that's limited".

## Acceptance

- AC1. A vertical trackpad gesture or mouse wheel over a suggestion (or code
  block) in an Agent tab report that only scrolls sideways moves the report,
  down and up; a sideways gesture over it moves the suggestion.
- AC2. A scroller that declines a gesture's axis is passed over whatever
  range its hidden bar reports; the same holds for the in-place readers of
  the Agent tab's answers and the comments drawer's posts.
- AC3. A vertical scroller at its top or bottom hands a new gesture that way
  to the outer area, and still takes a gesture the other way; latching holds
  within one gesture only. A mouse wheel's notch a reader cannot take at its
  end goes to what holds it.
- AC4. Qt tests reproduce the defect (phased wheel events over the report's
  suggestions) and fail without the fix; focused tests, format, tidy on the
  changed files and the documentation gate pass.

## Cause

[[BPR-2279]]'s `nested_scroll_latch` gives a gesture to the innermost
scroller whose bar can still move on the gesture's axis. A report's
suggestion is an in-place `markdown_body_view`: its page scrolls sideways
and declines every vertical wheel, which belongs to what holds it. Its
vertical bar is hidden, but under the macOS style it keeps a range its page
does not have (0..227 under cocoa for a one-line suggestion; 0 offscreen, so
BPR-2279's offscreen course passed). The latch chose that reader for a
downward gesture and sent the gesture to it alone; the reader ignored it,
and the report never moved. Upward, the bar was at its minimum, so the
gesture passed through. The Agent tab's answers and the drawer's posts are
the same in-place reader.

## Fix

The latch offers the gesture's first movement to each inner scroller that
can move that way, innermost first, and gives the gesture to the first one
that takes it; a scroller that ignores the movement (an in-place reader, a
code box, a suggested-change diff) is passed over, and the outermost decides
for itself as before. A scroller at its end that way is still passed over
without being asked, so a new gesture at a limit moves the outer area; the
latch still holds only from one gesture's ScrollBegin to the next.

A `markdown_body_view` that is not in place kept every wheel even at its end
("A body owns scrolling even at its end"). Its unphased mouse-wheel notch
that moves nothing now goes to what holds it; a trackpad gesture stays with
it as before, kept there by Qt or the latch.

## Verification

- `qt_nested_scroll_latch` (extended): an Agent tab report with two
  suggestions wider than the page, each reader's hidden vertical bar given
  the 0..227 range the macOS style leaves; a phased gesture over either moves
  the report 60 px down and back up, its owner the report's scroll area, a
  mouse wheel moves the report, and a sideways gesture moves the suggestion
  alone. A field at its top hands a new upward gesture to the outer area and
  keeps a downward one. A reader that declines vertical movement though its
  bar has a range passes a vertical gesture on both ways and keeps a sideways
  one. A non-in-place body at its end and at its top hands a mouse-wheel
  notch to the outer area and keeps one it can take. Without the latch fix
  it fails at "a vertical gesture down over a reader that declines it moves
  the outer area" (outer 0); without the body fix at "a mouse wheel down at
  the body's end moves the outer area".
- Under `QT_QPA_PLATFORM=cocoa` with the real macOS style (a temporary run,
  not a committed course): the report's suggestion readers report a 0..227
  vertical range; without the fix a downward gesture leaves the report at
  268, with it the report moves 268 to 328 and back.
- Also passing: `qt_conversation_cards_view`, `qt_agent_report_view`,
  `qt_markdown_body_1_0/1_25/2_0`, `qt_composed_markdown_1_0/1_25/2_0`,
  `qt_discuss_comment_bodies`, `qt_github_conversation_panel`,
  `qt_pull_request_overview_page`, `application_overview_summary_1`,
  `qt_workbench_conversation_reveal`, `qt_workbench_window_scale_100`,
  `qt_public_preview_conversation`, `qt_suggestion_editor_1_0` and
  `architecture_boundaries`. Format and tidy pass on the changed files.
