---
schema: "implement-rfc.card.v1"
id: "BPR-2399"
title: "Go to comment opens the Local comment in Comments, a report's keep rows fit every width, a finding card's follow-up moves the draft it made, and the Agent tab no longer pans under finding cards"
status: "Done"
rfcs: []
depends_on: ["BPR-2385", "BPR-2391", "BPR-2376"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","defect":true,"mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"024cda00d","origin":"owner, 2026-11-25: the three things BPR-2385's and BPR-2376's Limits left, to be finished","reservation":"docs/issues/_reservations/BPR-2399--BPR-2399--93845b610b5d.json","related":["BPR-2385","BPR-2391","BPR-2376","BPR-2375","BPR-2268","BPR-2331","BPR-2286","BPR-2325","BPR-2059"],"acceptance":["BPR-2399:comment","BPR-2399:rows","BPR-2399:card","BPR-2399:pan","BPR-2399:format"],"owner_step":"attended look, tests-on build with the Fixture agent: Keep local on a report's README finding and Go to comment once; Ask follow-up on a finding and read its keep row beside the chat"}
---

# BPR-2399: Go to comment opens the Local comment, a report's keep rows fit every width, and a finding card's follow-up moves its draft

## What was left

Three limits recorded on Done cards, which the owner asked to have finished
(2026-11-25).

- [[BPR-2385]]: "A report finding kept local (Go to comment) still goes to
  its lines only; it does not turn the tray to Comments or open the Local
  comment's card."
- [[BPR-2385]]: "In a report narrowed by its side chat, the keep row of the
  other states still wraps its sentence a letter to a line ('at
  renderer.cpp:11. Nothing is published'); only the changed-draft row was
  moved under its controls."
- [[BPR-2376]]: "A follow-up opened from an Agent tab finding card that no
  open report holds updates the card's comment but not a draft that card
  made."

## Cause

- Go to comment. The report's go dispatch (`window_quick_ask.cpp`) took the
  draft route only for a drafted finding. A finding kept local fell through
  to `go_to_file_line` or `open_file_in_review`: the file came in front, the
  right tray stayed on whatever tab it showed, and the Local comment stayed a
  closed note. The window had no route that arrives at a Local comment from a
  report: `go_to_private_note` opens a card in the file shown, and
  `open_local_from_threads` does not bring a file's tab in front of a report.
- The keep rows. A row was one HTML table row: its chips, then a cell with
  the sentence. Qt's rich text lays a table row out on one line at any width
  and takes the room it lacks from the cells that may wrap, in proportion, so
  a row of three chips in a 413 px page left the sentence 20 px and stood it
  a letter to a line, and the word before the chips ("Drafted", "Dismissed")
  gave up its width with it. The page was written once and only laid out
  again at a new width, so nothing in it could answer the width.
- The finding card. On Use, `use_follow_up` revised the draft only of a
  finding the open report holds (`revise_finding_draft`, keyed by the
  report's own record). A card of an answer that is not a review has no
  report, so its draft was never looked at.
- The pan, found on the way. A finding card's Draft, Dismiss and Ask
  follow-up stood in a box layout that cannot wrap: the row needed 256 px,
  and with its margins the Agent tab's content was 302 px in the 262 px a
  280 px drawer shows. The composed drawer course never drew a finding card:
  the Fixture agent's only findings were a review's, which its report holds.

## Change

Go to comment (`src/adapters/qt/src/workbench_window_comments_and_threads.cpp`,
`workbench_window_drafts_and_behaviour.cpp`, `workbench_window.hpp`,
`conversation_cards_view.cpp`, `.hpp`; `src/app/src/github_application/window_quick_ask.cpp`):

- `workbench_window::open_local_comment` is the Local comment's
  `open_roster_draft`. The file behind the report has its tab made active as
  it stands; another file's own tab opens. The right tray turns to Comments
  with the drawer open, the card is opened (never toggled shut), and the
  keyboard goes to it. From a report it waits for the file's tab to have its
  own reading back (`file_tab_settled`, which a draft gone to already used).
- Whole in the drawer: the cards view's settling reveal, which only a draft
  being written had ([[BPR-2375]]), serves any open card
  (`conversation_cards_view::reveal_open_card`).
- A Local comment on the pull request opens where it is, on Overview's
  Comments tab. One the pull request no longer holds falls back to the
  finding's lines, as a deleted draft does.
- The report's go dispatch names the Local comment its keep made (the
  finding's kept record, `noted`).

The keep rows (`src/adapters/qt/src/report_page.cpp`, `report_page.hpp`,
`agent_report_view.cpp`, `.hpp`):

- The page arranges each row for the width it is drawn at
  (`report_page_fit`). It measures the row's chips and its sentence in the
  row's own font and chooses:
  - everything on one line where it fits, as before;
  - the sentence beside its chips where it has room for two lines of whole
    words (a third line makes a thin column taller than the chips it
    explains);
  - otherwise the sentence under its chips at the row's whole width, and the
    chips on as many lines as they need, in their order.
- The word that says a state keeps its line, as a chip does.
- What stands under a row (the sentence, and the line that says a draft
  differs) is a line of its own six pixels under it. A paragraph's margin is
  not kept after a table, and its words touched the chips above them.
- The page reports the widths at which some row's arrangement changes, and
  the view draws the page again only when its column crosses one
  (`agent_report_view::fit_page`); between them a new width lays the same
  page out, as before. The keyboard's link and the scroll are kept through
  the redraw, as through any render.
- The same for Keep summary, Draft all and its Undo, and Dismissed with its
  Undo.

The finding card (`src/app/src/github_application/window_follow_up.cpp`,
`window_agent_setup.cpp`, `window_composition.hpp`;
`src/adapters/qt/src/agent_tab_view.cpp`, `agent_tab_view.hpp`):

- On Use, a card's finding that no open report holds takes the card path
  (`revise_card_draft`): the card says which draft it made
  (`agent_tab_view::finding_card_draft`), the core decides
  (`core::decide_finding_draft_revision`, unchanged), an unchanged draft
  takes the update in one revisioned edit, and a draft the reviewer changed
  is kept.
- A kept draft's card offers **Update draft** in its row and says under the
  row "Drafted. The draft differs from the updated suggested comment.
  Nothing is published.", in place of its state beside the buttons
  (`present_finding_draft_outdated`). Update draft writes the updated body
  the same way; refused (the draft changed meanwhile), it is offered again.
  A card that stops saying Drafted stops offering it.
- The report's path and the card's share `decide_finding_draft` and
  `write_finding_draft`; the report's behaviour is unchanged.
- The side chat's words are the ones it already said for a report's finding.

The pan (`src/adapters/qt/src/agent_tab_view.cpp`):

- The card's choices and what it says of itself are a
  `detail::wrapping_row`: on one row where they fit, on further rows where
  they do not, and what the card says takes a row of its own at the card's
  width when none holds it. Each chip of the row is laid out by its own rect
  (`Qt::WA_LayoutUsesWidgetRect`).

Tests' fixtures: `tests/end_to_end/agent_suggestions_fixture_main.cpp` answers
Update suggested comment with a rewritten comment, and with another when asked
over that one. The Fixture agent (`tests/support/fixture_agent_main.cpp`)
answers "Any findings worth a comment?" with a sentence and three findings
and no heading, so the Agent tab shows them as cards no report holds. The
composed drawer course (`tests/end_to_end/composed_drawer_fit_public_path.cpp`)
asks it and sweeps two new states: the cards open, and one dismissed.

Docs: `docs/architecture/review-tours.md` (the item side chat section),
`docs/ux/ai-experience.md` (the report's keep actions).

## Acceptance

- **BPR-2399:comment** One click on a report finding's Go to comment brings
  the comment's file in front, turns the right tray to Comments, and opens
  the Local comment's card whole in the drawer with the keyboard, from
  another file and from the comment's own.
- **BPR-2399:rows** At every report width that holds one chip, no keep row
  is wider than the page, every chip keeps its words on one line, and every
  row's sentence breaks only between words; beside its side chat too.
- **BPR-2399:card** A follow-up from an Agent tab finding card that no report
  holds updates the unchanged draft that card made; a draft the reviewer
  changed is kept, its card says it differs and offers Update draft, and
  Update draft replaces it.
- **BPR-2399:pan** With finding cards no report holds under an answer, open
  and one dismissed, the Agent tab has no sideways range and cuts no control
  at a 280, 300, 340 or 420 px drawer or in the narrowest window.
- **BPR-2399:format** The format check passes on the changed C++ files and
  clang-tidy reports nothing on a changed line.

## Verification

macOS arm64, dev preset (Debug), fixture agents only: the Fixture agent, and
the suggestions fixture `codex` for the keep-action course. No real agent, no
network. Evidence revision `024cda00d`: the lane's four commits rebased onto
main `60673a9fc` ([[BPR-2395]] to [[BPR-2400]]). The ctest rows ran offscreen;
three courses and the page test ran a second time as the window is drawn on
screen (Cocoa, `BPR_NATIVE_LOOK=1`, kept off screen, the display's own 200%).

| Criterion | Check | Result |
|---|---|---|
| BPR-2399:comment | `application_report_draft_reveal_public_path`: inline-drawer#22, the Fixture agent's review. Keep local on the README finding (one click on the page's own link), then twice, with the comment a closed note and the right tray on the Agent tab, one click on Go to comment: from another file behind the report and from README.md behind it. Each time README.md's tab is in front, the tray shows Comments, the Local comment's card is open at 107 to 324 of a 565 px drawer (99 to 323 of 585 under Cocoa), and the keyboard is inside the card. Nothing is published | pass |
| BPR-2399:rows | `qt_agent_report_view` > `check_keep_rows_fit_every_width`: a report with a row of each kind (offered, drafted, drafted with a draft that differs, kept local, dismissed, Keep summary, Draft all) at 23 widths, the page 1188 px down to 168 px. At each: no sideways range, every chip on one line inside the page, every sentence breaking only between words inside the page. The first finding's row: one line down to a 668 px page; the sentence beside its three chips on two lines from 608 to 528; under them from 488; the chips on two lines from 368 and on three from 228. Wide again, the page is as it was | pass |
| BPR-2399:rows | `qt_agent_report_view` > `check_keep_chip` ([[BPR-2268]]'s chip), widened to 27 widths: the chip keeps its one line and its height throughout; the words beside it take two lines at most and are level with it; narrower they stand clear under it | pass |
| BPR-2399:rows | `application_report_draft_reveal_public_path`: Ask follow-up opens the finding's chat beside the report; the rows are judged in windows of 1440, 1400, 1360, 1320, 1280, 1250 and 1220 px (the page 400, 360, 322, 282, 244, 214 and 184 px offscreen; 413 down to 197 under Cocoa): whole in each | pass |
| BPR-2399:rows | `application_follow_up_public_path`: beside its side chat the report's rows are whole with a finding offered and one drafted, and again with a finding whose draft differs | pass |
| BPR-2399:card | `application_agent_keep_action_public_path` (nested-folders#21, the suggestions fixture): the placed card's Draft, then Ask follow-up, Update suggested comment and Use. The card shows the update, the unchanged draft holds it and the chat says "Suggested comment and its draft updated." Then the reviewer changes the draft and a second update is used: the draft is kept, the chat says it was left, the card shows Update draft and "Drafted. The draft differs from the updated suggested comment. Nothing is published." under its row in place of its state; Update draft replaces the draft and the card says Drafted again. With the drawer dragged to 280, 300, 340 and 420 px the two buttons keep their words inside the card, do not stand over each other, and the sentence wraps under them. Nothing reaches GitHub | pass |
| BPR-2399:pan | `application_composed_drawer_fit_scale_100`: the Fixture agent answers "Any findings worth a comment?" with three findings no report holds; the new states `agent-finding-cards-open` and `agent-finding-card-dismissed` are swept at 280, 300, 340 and 420 px and in the narrowest window with the course's other 29: 31 states, 0 faults. The same course drawn under Cocoa off screen: 31 states, 0 faults | pass |
| BPR-2399:pan | `application_agent_keep_action_public_path`: with six cards under the answer, one of them saying its draft differs, the Agent tab has no sideways range at 280, 300, 340 or 420 px (it was 302 px of content in a 262 px viewport) | pass |
| BPR-2399:pan | Reproduced with the choices' row alone made to wrap: "agent-finding-cards-open at 280 (drawer 280): pans sideways: 'agentConversationScroll' pans 80 px sideways; content is 342 wide in a 262 px viewport ('agentFindingAnchor' reaches 274, 'agentFindingSuggestionLabel' \"issue (blocking)\" reaches 298)" and "cut off: 'agentTurnDraftAll' spans -16..64 of the drawer's 280". The card's two other rows were then made to wrap | fail, then fixed |
| BPR-2399:card | `qt_workbench_window_scale_100` (the finding card): no Update draft by itself; the card names the draft it was told of; marked, it offers Update draft and says the draft differs in place of its state; the press names the card; a card drafted again offers none | pass |
| Reproduced | The new courses on main's sources (`5bc62c938`, built in the lane's tree, Cocoa off screen). Report: "beside its side chat, in a 1400 px window ... 'at renderer.cpp:12-13. Nothing is published' breaks inside a word, its line 1 of 35 being 'a'", and from 1360 px down the page has a sideways range of 59 to 197 px with Ask follow-up past its edge; "Go to comment turns the right tray to Comments; it shows Agent". Card: "a follow-up from a finding card updates the unchanged draft that card made; the draft still says: ... Consider moving the reset into `begin_request` instead" | fail, as recorded |
| Not broken | `qt_item_side_chat`; `qt_agent_report_summary_scale_100`, `_125`, `_200` and `_native_look` with one check corrected (below) | pass |
| Pin | `application_review_round_trip_composition_contract` fails on the production app manifest digest alone ("production application source content digest drifted"): the lane keeps main's value, and the coordinator reseals at the merge with the print mode. The full application and mutation-branch pins are unchanged | open, by the coordinator's word |
| Docs | `python3 scripts/verify_documentation.py --root .`: `DOCUMENTATION_OK`. The lane's worktree has no deployed implement-rfc skill, so the gate read its `board_model` from the main checkout's copy (`PYTHONPATH`) | pass |
| BPR-2399:format | `_coding-standard/scripts/format.py --dry-run` (clang-format 22.1.8) on the changed C++ files: nothing to change. `_coding-standard/scripts/tidy.py` on 15 changed translation units: passed; its one finding on a changed line (a course's local constant in the wrong case) is fixed. `tests/adapters_qt/workbench_window_tests.cpp` was not run through clang-tidy, and clang-tidy was not run again after the last round's changes to `agent_tab_view.cpp`, the composed course, the Fixture agent and the summary test | pass, with that gap |

The summary test's one corrected check
(`tests/adapters_qt/agent_report_summary_tests.cpp`): in the narrowest drawer
with a larger font it asked whether the full donut would fit up to the
drawer's edge. That passed only while finding cards whose rows could not wrap
made the conversation 69 px wider than the drawer, and the summary with it.
With the rows wrapping the conversation fits, the summary is 217 px in a
262 px drawer and the full donut needs 233, so it is the smaller one, as the
check's own words mean. The check now measures against the width the summary
is given.

One earlier run of `application_composed_drawer_fit_scale_100` failed while
clang-tidy and four other rows ran beside it: its wait "the question is sent:
What changed in this push?" ran out at 120 s. It passed alone each time since.
`application_composed_drawer_fit_scale_200` was not run in the last round.

Renders, all under
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/report-followups-renders`:

- Before, the composed window on main's sources, Cocoa off screen at 200%:
  `before/cocoa-macos-200/report/report-beside-side-chat-1440.png` to
  `-1220.png`, each with its `-page.png` (the sentence a letter to a line);
  `report-go-to-comment-before.png` and `report-go-to-comment.png` (the press
  and where it left the window: the tray on the Agent tab, no card open);
  `before/cocoa-macos-200/agent-card/card-follow-up-used-comments.png` and
  `-agent-tab.png` (the card updated, its draft not).
- After, the same states at `024cda00d`:
  `after/cocoa-macos-200/report/` (the same names, and
  `report-go-to-comment-own-file.png` with its `-before.png`);
  `after/cocoa-macos-200/agent-card/` (`card-follow-up-used-*`,
  `card-draft-differs-agent-tab.png`, `-comments.png`,
  `card-draft-differs-drawer-280.png` to `-420.png`,
  `card-draft-updated-*`); `after/cocoa-macos-200/composed-drawer/cocoa-macos/`
  (every state of the composed drawer course,
  `drawer-light-200-agent-finding-cards-open-280.png` and
  `drawer-light-200-agent-finding-card-dismissed-280.png` among them, each
  also at 300, 340 and 420 and in the narrowest window).
- The page alone with a row of each kind, each file named for its page's
  width: `after/cocoa-macos-200/keep-rows/` (1006, 566, 426, 326 and 236 px)
  and, as layout evidence only, `after/offscreen-100/keep-rows/` (988, 548,
  408, 308 and 218 px).

## Limits

- A chip never wraps its words, so no arrangement fits a page narrower than
  its widest chip inside a finding's box (about 170 px). With both drawers and
  the side chat open, the window leaves the report that little under about
  1200 px wide (106 px at 1140): the page is then cut as a whole. The side
  chat's column keeps 260 px and the report has no least width of its own;
  that is the window's layout, not the row's, and is not changed here.
- At the drawer's narrowest a card's severity, kind and place can take two
  rows or three, where the kind's words are long: nothing is cut, and the
  place has a row of its own.
- A card's Update draft offer is kept for the window's session, as the
  report's is ([[BPR-2376]]). A follow-up opened from a report whose finding
  is a card, then used after another report took the view, updates the draft
  through the card's path and marks the hidden card, not the report's row.
- A Local comment on the pull request (a finding on a file the pull request
  does not change) goes to Overview's Comments tab; no course covers that
  branch, since the fixtures' review names only changed files.
- The open Local card's own heading reads "Local reply" for a comment on a
  whole file. This change does not touch that card's words, and whether that
  heading is meant was not looked at.
- No owner look was prepared in this round.
