---
schema: "implement-rfc.card.v1"
id: "BPR-2397"
title: "The Markdown readers mark the keyboard's link with the window's focus ring, not a text selection, and a press on a link is the link's"
status: "Done"
rfcs: []
depends_on: ["BPR-2391", "BPR-2392"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","defect":true,"mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"21b7d9ecd","origin":"follow-up to BPR-2391's Limits (owner, attended review 5627b21a56838de9fae9f2ac4e4349d4, 2026-11-25, of a report link that came up selected)","reservation":"docs/issues/_reservations/BPR-2397--BPR-2397--24b2b25841f9.json","related":["BPR-2391","BPR-2392","BPR-1157","BPR-1290","BPR-1543","BPR-1544","BPR-1802","BPR-2184"],"acceptance":["BPR-2397:ring","BPR-2397:selection","BPR-2397:click","BPR-2397:return","BPR-2397:shared","BPR-2397:composed","BPR-2397:format"],"owner_step":"attended look: in the Agent tab press Tab into an answer until a place it names is ringed, then click that place once"}
---

# BPR-2397: The Markdown readers ring the keyboard's link and give a press on a link to the link

## Why

[[BPR-2391]] fixed the report's page after the owner's look (attended review
`5627b21a56838de9fae9f2ac4e4349d4`, 2026-11-25):

"Mostly, When I click draft comment it sends me to go to draft with this
button text selected and then I cant actually click go to draft properly
until I deselect it and then click it again."

The page had marked "the link the keyboard is on" by selecting its words, and
a press on selected words is, to Qt, the start of a drag of them. [[BPR-2391]]
made the keyboard's link a state of its own, drawn as the window's focus
ring, and gave a press on a link to the link. Its Limits left the same mark
in the Markdown readers: "The Markdown readers (a comment's body, a report
drawn as Markdown, the code boxes) still show the link Tab reached as a text
selection."

Those readers are one class (`markdown_text_view` inside
`markdown_body_view`). Its `focus_link` selected the link's words with the
text cursor, on Tab and on a footnote jump or a `<details>` summary made with
the pointer. One click did follow a link there, because the reader followed
it after Qt had handled the release, which clears a selection pressed on. So
the swallowed click was not reproduced in these readers; what was left was
the selection used as the keyboard's mark, a summary that came up selected
after a click on it, and a press on a link that started a selection or a
drag.

## Change

`src/adapters/qt/src/markdown_body_view.cpp`,
`src/adapters/qt/include/bpr/qt/markdown_body_view.hpp`:

- The link the keyboard is on is a state (`FocusedLink` and `OnLink`), not a
  selection. No code path of the reader selects a link's words. While the
  reader holds the keyboard the link wears the window's focus ring
  ([[BPR-1157]]'s colour and width), close round its words, one box a line.
  The caret waits unseen at the link and is not drawn; the caret's own width
  is kept and given back (the description shows none at all).
- Tab and Shift+Tab move it as before, a closed section's links still
  skipped. Return follows the link the keyboard is on, else the link the
  caret is in. A screen reader is told the words of the link reached (an
  image's alternative text), since no selection is there to be read.
- An arrow key, Select All or a press of the pointer ends the ring. Tab then
  goes on from the link it left, or from the link the pointer pressed.
- A press of the pointer on a link is the link's. It starts no selection and
  no drag, and the release on that link follows it whatever the reader has
  selected, its own words included; the selection is left as it was. A press
  dragged off its link follows nothing. A double click follows its link once
  and selects none of its words. A press with Shift held extends the
  selection, as anywhere in the text.
- A footnote jump puts the keyboard on the link it lands on, in its ring,
  whether Return or the pointer made it: the ring is what shows where the
  jump landed, and Return there leads back. A `<details>` summary keeps the
  ring only when Return pressed it; one the pointer pressed wears none.
- An image that arrives while the keyboard is on a link leaves it there.
- `markdown_body_view::keyboard_link()` says the words of the keyboard's
  link, for tests and accessibility.

Shared with the report's page
(`src/adapters/qt/include/bpr/qt/keyboard_focus_ring.hpp`,
`src/adapters/qt/src/keyboard_focus_ring.cpp`,
`src/adapters/qt/src/agent_report_view.cpp`):

- `text_focus_boxes` (where a link's words stand, one box a line) and
  `paint_text_focus_ring` (the ring round them) are one implementation. The
  report's page draws a link in its text through them and keeps its own ring
  on a chip's border.
- The ring's middle stands two pixels beside the words and one above and
  below, so its stroke is in the middle of the space between two words. The
  report's page drew a link in its text three pixels out, where the stroke
  met the next word; it now draws as the readers do.
- A reader in place has no margin: a link may start at the viewport's edge
  or on its first line. A side with no room beyond the words is brought to
  the edge. Above and below it is drawn whole inside the edge, where a line
  has room over its letters; beside the words it is drawn on the edge, where
  half its width shows and no letter is drawn over.

Docs: `docs/architecture/native-markdown.md` (links and footnotes),
`docs/ux/workbench-specification.md` (a post's links),
`docs/ux/ai-experience.md` (the readers follow the report's rule).

## Acceptance

- **BPR-2397:ring** In a comment's body, a report drawn as its Markdown and
  a code box's header, Tab puts the keyboard on a link, which wears the
  window's focus ring on all four sides while the reader holds the keyboard
  and none without it; nothing else on the reader changes.
- **BPR-2397:selection** Tab, Shift+Tab, Return and a click on a link select
  no text in any of the three readers.
- **BPR-2397:click** One click of the pointer follows a link whatever the
  reader has selected, its own words and the whole body included, and leaves
  that selection as it was. A press dragged off its link follows nothing.
- **BPR-2397:return** Return follows the link the keyboard is on.
- **BPR-2397:shared** The report's page and the Markdown readers draw a link
  in the text through one implementation.
- **BPR-2397:composed** In the application's own window, with the Fixture
  agent, Tab in an answer of the Agent tab puts the keyboard on the place
  the answer names, ringed and not selected, at every drawer width, and the
  composed drawer course passes with that state in it.
- **BPR-2397:format** The format check passes on the changed C++ files and
  clang-tidy reports nothing on a changed line.

## Verification

macOS arm64, dev preset (Debug), Qt 6.12.1, fixture agent only. Evidence
revision `21b7d9ecd` (one commit on main `5bc62c938`).

| Criterion | Check | Result |
|---|---|---|
| Before | `application_composed_drawer_fit` with the new state, on main `5bc62c938`'s reader, native (Cocoa, kept off screen) in Dark and Light: after Tab in the first answer the reader's selection is "src/engine/renderer.cpp:12" (`before/course-output-dark.txt`, `-light.txt`; `before/cocoa-macos/drawer-dark-200-agent-answer-link-keyboard-340.png`, the place drawn as selected text to the line's end). `qt_markdown_body`, `qt_markdown_github` and `qt_markdown_github_html` asserted that selection ("Tab selects the first link") | as BPR-2391's Limits said |
| BPR-2397:ring, BPR-2397:selection, BPR-2397:click, BPR-2397:return | `qt_markdown_link_mark_scale_100`, `_125`, `_200` (new, `tests/adapters_qt/markdown_link_mark_tests.cpp`), keys and clicks through the window system, once for each of: a comment's body in place in its open thread (its first link starts the post, at the reader's edge and top line); a report without the convention's sections, drawn as its Markdown by `agent_report_view`; a code box (a permalink's lines), whose header's file and commit are links. Tab reaches the first link and selects nothing; the accessibility bridge is told its words; the ring's colour is on all four sides of the words and was not there without the keyboard; every pixel that differs between the two is within the ring's reach of the words; with the keyboard back the ring is on the same link; Return follows it and it stays; Tab goes on and the link left wears no ring; Shift+Tab goes back; an arrow key ends it. One click follows a link with nothing selected, with its own words selected (twice), and with the whole body selected (two links), the selection left as it was; one click on the ringed link follows it and ends the ring; a press dragged off follows nothing and selects nothing; a double click follows once and selects nothing. Also run under the native look in Dark and Light | pass |
| BPR-2397:selection, BPR-2397:return | `qt_markdown_body_1_0/1_25/2_0` (Tab puts the keyboard on First and selects none of it, Return follows, Tab goes on, Tab leaves), `qt_markdown_github_1_0/1_25/2_0` (footnote references and back references by Return and by a click: the keyboard lands on the other, ringed, nothing selected), `qt_markdown_github_html_1_0/1_25/2_0` (a summary by Return keeps the keyboard; one the pointer pressed wears no ring and is not selected; Tab skips a closed section's link) | pass |
| BPR-2397:shared | `qt_agent_report_view` (the report's page, its chip ring and one-click links, unchanged) with `agent_report_view.cpp` drawing a link in its text through `text_focus_boxes` and `paint_text_focus_ring` | pass |
| BPR-2397:composed | `application_composed_drawer_fit_scale_100`, `_scale_200`, and the same course natively in Dark and Light with renders: new state `agent-answer-link-keyboard` after `agent-answered`. Tab from the first answer's text reaches "src/engine/renderer.cpp:12"; nothing is selected; the drawer is dragged to 280, 300, 340 and 420 and the window made its narrowest; the keyboard is still on the link. 29 states, 0 faults | pass |
| Not broken | `qt_conversation_cards_view`, `qt_composed_markdown_1_0/1_25/2_0`, `qt_markdown_images_*`, `qt_markdown_code_*`, `qt_markdown_blob_snippet_view`, `qt_markdown_cmark_spec`, `qt_pull_request_overview_page`, `qt_feedback_label_threads`, `qt_tour_stop_card`, `qt_review_tour_stop_view`, `qt_agent_report_summary_scale_100/125/200`, `qt_item_side_chat`, `qt_drawer_surfaces_fit_scale_100/125/200`, `qt_nested_scroll_latch`, `qt_tour_side_chat`, `qt_tour_chat_in_tab`, `qt_destructive_question_scale_100/200`, `architecture_boundaries` (42 rows in one run); `qt_workbench_window_scale_100`, `qt_composed_github_scale_100`, `application_report_draft_reveal_public_path` | pass |
| Reseal | None needed: the print mode of `application_review_round_trip_composition_contract` reports the three pinned values already in the file | not needed |
| Docs | `python3 scripts/verify_documentation.py --root .` (with the deployed implement-rfc scripts on `PYTHONPATH`; the worktree has no MASK deployment): `DOCUMENTATION_OK` | pass |
| BPR-2397:format | `_coding-standard/scripts/format.py` (clang-format 22.1.8) on the ten changed C++ files, and `coding_format_check`. `_coding-standard/scripts/tidy.py` on `keyboard_focus_ring.cpp`, `markdown_body_view.cpp`, `agent_report_view.cpp` and the new test: no finding on a changed line; the three it reports in `agent_report_view.cpp` (lines 826, 1084, 1271) are main's | pass |

Renders, from the application's own window by the composed course, native
(Cocoa, QMacStyle, kept off screen, the display's 200%), under
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/markdown-link-renders/`:

- `before/cocoa-macos/drawer-dark-200-agent-answer-link-keyboard-*.png` and
  `drawer-light-200-…`: the place selected, the selection running to the
  line's end.
- `after/cocoa-macos/` the same names: the place in the window's focus ring,
  nothing selected. `…-280-window.png` is the whole window.

## Course change

`tests/end_to_end/composed_drawer_fit_public_path.cpp`: the new state, and
`cut_off` no longer judges a widget in another window. The destination
tooltip a reader raises under the link Tab reached is a tool tip window
parented under the drawer; the course read it as a label of the drawer at
its screen position ("spans 1541..1696 of the drawer's 280"). The tooltip is
main's behaviour ([[BPR-1290]]); only the course's reading of it changed.

## Limits

- The fixture's comments hold no link, so the composed window's look is the
  Agent tab's answer. A comment's body, the report drawn as Markdown and the
  code box are judged in `qt_markdown_link_mark`, each in the surface that
  holds it, not in the composed window.
- A press that begins inside a link no longer starts a drag selection, as on
  the report's page. A selection can begin beside the link and run over it,
  and Shift with a press extends one onto it.
- The report's page now draws a link in its text two pixels out instead of
  three, and at the page's edge where it had been cut. Its chips are
  unchanged.
- `QLabel` links elsewhere (a byline) keep Qt's own keyboard mark; they were
  not in this card's scope.
- `test_network_isolation_manifest` fails in this worktree's tree with
  "gh_cli_git_smart_transport must run only its own executable
  better_pull_review_gh_cli_git_smart_transport_tests with no wrapper or
  argument: None". That row is not one this change registers or touches,
  and the message names none of the three new rows. It was not compared
  with main's tree, so whether main fails the same way is not known.
- The CTest row `application_composed_drawer_fit_cocoa` was not run; the
  same binary was run natively by hand in Dark and Light (29 states, 0
  faults each), which is where the renders come from.
  `qt_workbench_window_scale_125` and `_200`, `qt_composed_github_scale_125`
  and `_200`, `application_review_tour_composition_public_path` and
  `application_follow_up_public_path` were not run in this round (the
  machine's disk was full for most of it; three builds stopped on "No space
  left on device").
- `qt_markdown_link_mark` passed under the native look in Dark and Light
  with the ring as committed, but before the test's scan of the ring's four
  sides was rewritten for clang-tidy (a whole-pixel step in place of a
  fractional one). The committed test ran offscreen at the three scales
  only.
- No owner look was prepared in this round. A keep-or-change look of the
  ring on an answer's link needs a frozen build.
