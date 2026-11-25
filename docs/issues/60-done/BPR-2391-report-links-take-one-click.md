---
schema: "implement-rfc.card.v1"
id: "BPR-2391"
title: "A report's links take one click, and the link the keyboard is on is a focus ring, not a text selection"
status: "Done"
rfcs: []
depends_on: ["BPR-2385", "BPR-2258"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","defect":true,"mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"421938ce7","origin":"owner, attended review 5627b21a56838de9fae9f2ac4e4349d4, rejecting BPR-2385's look, 2026-11-25","reservation":"docs/issues/_reservations/BPR-2391--BPR-2391--399807a341ca.json","related":["BPR-2385","BPR-2258","BPR-2184","BPR-2219","BPR-2375","BPR-1157"],"acceptance":["BPR-2391:click","BPR-2391:selection","BPR-2391:keyboard","BPR-2391:format"],"owner_step":"attended look: click Draft comment on a report finding, then click Go to draft once"}
---

# BPR-2391: A report's links take one click, and the keyboard's link is a focus ring

## The owner's words

Attended review `5627b21a56838de9fae9f2ac4e4349d4`, rejecting [[BPR-2385]]'s
look:

"Mostly, When I click draft comment it sends me to go to draft with this
button text selected and then I cant actually click go to draft properly
until I deselect it and then click it again."

Then, to the session: "It's worse than that actually, clicking on it does
nothing, you had to click away first before you were allowed to click." And:
"I'm not sure why creating a draft selects that text to begin with, so i
would look there".

## Cause

Two things in the report page (`src/adapters/qt/src/agent_report_view.cpp`),
each harmless without the other.

- Why the text was selected. The page made the selection itself, on purpose.
  [[BPR-2258]] hands the keyboard back to the page when Draft comment (or
  Keep local) ends, and its `focus_link` marked "the keyboard's current
  link" by selecting the new link's words with the text cursor, as Tab on
  the page did ([[BPR-2184]]). It ran whenever the page held focus, and a
  click gives the page focus, so a Draft comment made with the pointer
  ended with Go to draft's words selected. Nothing was carried over from
  the click and no label was reused: the page is drawn again from HTML, and
  the selection was set after that.
- Why the click was swallowed. The page's event filter read a release as a
  click on a link only with nothing selected
  (`!Dressed->textCursor().hasSelection()`), so a drag that ended on a link
  followed nothing. A press inside the selection is, to Qt, the start of a
  drag of the selected text, so the selection was still there at the
  release and the click was refused. Qt then re-marked the link under the
  release, so the words stayed selected.

[[BPR-2385]]'s course pressed both links through `agent_report_view::press`,
which goes round the pointer, so it never met this.

## Change

`src/adapters/qt/src/agent_report_view.cpp`,
`src/adapters/qt/include/bpr/qt/agent_report_view.hpp`:

- The link the keyboard is on is a state of its own (`keyboard_link`), not a
  text selection. No code path of the page selects a link's words any more.
  While the page holds the keyboard the link wears the window's focus ring
  ([[BPR-1157]]'s colour and width): on the border of the whole chip for a
  button-shaped link, close round the words for a link in the text. The
  caret is not drawn while the keyboard is on a link.
- Tab and Shift+Tab move it and Return follows it, as before. A link
  written in several runs is one stop. Tab brings the link into the pane,
  and a screen reader hears its words.
- Only a keep action made with Return hands the keyboard on: to Go to draft
  or Go to comment when it was made, back to the keep action when it was
  refused. One made with the pointer hands it to nothing. A press of the
  pointer on the page, or moving the caret, ends it. A render keeps it on
  the same action or place where the page still has it.
- A press of the pointer on a link belongs to the link, as a press on a
  button does. It starts no selection and no drag, and the release on that
  link follows it, whatever the page has selected. A press dragged off its
  link follows nothing. A click on a link leaves a selection made elsewhere
  as it was.

The same pattern elsewhere:

- Every action of the report page is one of these links (Draft comment, Keep
  local, Keep summary, Draft all, Undo, Dismiss, Go to draft, Go to comment,
  Update draft, Ask follow-up) and so are its places; all are fixed together.
- The side chats' Update draft, Use, Discard and Draft reply are push
  buttons, not links, and their link labels are not selectable. The Markdown
  readers follow a link after Qt has handled the release, which clears a
  selection pressed on, so one click works there. None had the defect.

Docs: `docs/ux/ai-experience.md` (the report's links).

## Acceptance

- **BPR-2391:click** One click of the pointer on Go to draft, straight after
  one click on Draft comment and with no click anywhere else between them,
  goes to the draft. One click follows any link of the report whatever the
  page has selected, its own words included.
- **BPR-2391:selection** After Draft comment or Keep local, made with the
  pointer or with Return, nothing on the report is selected.
- **BPR-2391:keyboard** Tab reaches the report's links and Return follows
  the one reached; a keep action made with Return leaves the keyboard on Go
  to draft, drawn in the window's focus ring on the chip's own border.
- **BPR-2391:format** The format check passes on the changed C++ files and
  clang-tidy reports nothing on a changed line.

## Verification

macOS arm64, dev preset (Debug), Qt 6.12.0, fixture agent only. Evidence
revision `421938ce7` (three commits on main `947caf003`).

| Criterion | Check | Result |
|---|---|---|
| Reproduced | `application_report_draft_reveal_public_path`, with its presses made as window-system clicks, on main `947caf003`'s source, offscreen and under the native look in Dark and Light: "after Draft comment the page's selection is "Go to draft"" and "one click on Go to draft goes nowhere". `before/*/course-output.txt`, `before/cocoa-macos-dark/report-draft-drafted.png` | fail, as the owner saw |
| Reproduced | `qt_agent_report_view` on the same source: after one click on Draft comment the selection is "Go to draft"; one click on Go to draft went 0 times | fail, as the owner saw |
| BPR-2391:click, BPR-2391:selection | `application_report_draft_reveal_public_path`: inline-drawer#22, the fixture agent's review. One window-system click on the first finding's Draft comment; the page's selection is empty; one click on Go to draft with no other click between shows the file with the draft open in Comments. The three later Go to draft presses (its own line, another line, another file) are one click each | pass |
| BPR-2391:keyboard, BPR-2391:selection | The same course: Tab reaches another finding's Draft comment with nothing selected; Return drafts it behind the report; the keyboard is on "Go to draft", the selection is empty and the page keeps focus; Return goes to that draft. Nothing is published | pass |
| BPR-2391:click | `qt_agent_report_view` > `check_a_link_takes_one_click`, clicks and keys through the window system: Draft comment then Go to draft, one click each; with Go to draft's own words selected the first click goes, and the next; with the whole page selected one click each on Go to draft, Ask follow-up, a place and Update draft, the selection left as it was; Keep local then Go to comment, one click each; a press dragged off Go to draft goes nowhere and selects nothing | pass |
| BPR-2391:keyboard | The same check: Tab selects no text and reaches Draft comment, announced; Return hands the keyboard to Go to draft; the chip's border is the ring's colour on all four sides and the pixel beyond it is unchanged; the other chip, and either chip without the keyboard or after the pointer's Draft comment, wear none; a render keeps the keyboard's link; Return and one click on the ringed link both go; the pointer's press ends the ring. At 100%, 125% and 200% offscreen and under the native look in Dark and Light | pass |
| Not broken | `qt_item_side_chat`, `qt_agent_report_summary_scale_100/125/200`, `qt_agent_tone_colours`, `qt_agent_restored_byline`, `qt_side_chat_tab_and_send_scale_100`, `qt_markdown_code_1_0`, `qt_nested_scroll_latch`, `qt_drawer_surfaces_fit_scale_100`, `qt_workbench_window_scale_100`, `application_review_report_public_path`, `application_follow_up_public_path`, `application_agent_keep_action_public_path`, `application_ask_report_public_path`, `application_commit_asks_public_path`, `architecture_boundaries` | pass |
| Reseal | None needed: the print mode of `application_review_round_trip_composition_contract` reports the three pinned values already in the file. The contract itself was not run (the application target was not built) | not run |
| BPR-2391:format | `_coding-standard/scripts/format.py` (clang-format 22.1.8) on the four changed C++ files: nothing to change. `_coding-standard/scripts/tidy.py` on the three changed translation units: no finding on a changed line; the three it reports in `agent_report_view.cpp` are main's, at the same statements | pass |

Renders, the whole window from the course, native (Cocoa, QMacStyle, kept
off screen) in Dark and Light, and offscreen:

- Before, `goto-draft-click-renders/before/cocoa-macos-dark/`,
  `cocoa-macos-light/`, `offscreen-fusion/`: `report-draft-offered.png`,
  `report-draft-drafted.png` (Go to draft selected, the owner's screen),
  `report-go-to-draft-one-click.png` (after the one click: still the
  report).
- After, `goto-draft-click-renders/after/` in the same three folders: the
  same three names (Go to draft unselected; after the one click, the file
  and the open draft), `report-keyboard-on-draft-comment.png` and
  `report-keyboard-go-to-draft.png` (the ring on the chip), and the three
  later presses with their `-before.png`.

## Limits

- The Markdown readers (a comment's body, a report drawn as Markdown, the
  code boxes) still show the link Tab reached as a text selection. One click
  follows a link there; only the keyboard's mark differs from the report
  page's.
- Dismiss and Undo made with Return hand the keyboard to nothing, as before.
- `python3 scripts/verify_documentation.py --root .` could not pass in this
  worktree: it stops at `docs/issues/NEXT_ID: next_id: BPR-2390 rewinds
  historical high water BPR-2391`, a tracker state this change does not
  touch, so the gate never reached the edited document.
- No owner look was prepared in this round. The owner's rejected criterion
  needs a new attended look on a frozen build.
