---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AB"
title: "A comment on a selection shows its lines, from the menu to the published thread"
status: "Done"
rfcs: []
depends_on: ["BPR-1212"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"owner report 2026-10-21 (relayed by the coordinator)","owner_note":"Am I supposed to be able to leave comments on a line range? Seems like I can only do it on a single line. / I would expect to be able to do it from the right click context menu. / On a selection","related_cards":["BPR-1212","BPR-1335","BPR-1564","BPR-1713"],"related_rfcs":["RFC-BPR-0037","RFC-BPR-0052"],"frontend_design":"frontend-design:frontend-design loaded before the menu labels and the range thread's wash, header and mark wording"}
acceptance: [{"criterion":"AC1: right lines selected by a drag either way, a Shift-click or Shift+Down, then Add comment from a right-click inside them, on their code or a line number, or Return, give one draft on all of them; Suggest change fills its block with exactly their lines; a right-click outside takes its own line; base lines take one comment only with GitHub's numbering, and say why otherwise","outcome":"pass","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_workbench_line_draft$'","detail":"test_a_selection_takes_a_range_comment_from_its_menu drives every gesture through QWindowSystemInterface with the event loop turning between events, opens the window's popup and clicks its item on it. Failing before: 'Add comment and Suggest change name the lines they take: Add comment', and a right-click on a line number opened no menu."}},{"criterion":"AC2: on several lines the menu's items name them (\"Add comment on lines a–b\", \"on base lines a–b\")","outcome":"pass","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_workbench_line_draft|application_line_draft_contract)$'","detail":"The Qt course and the composed course read the item's text on the popup the window opened."}},{"criterion":"AC3: a published thread on several lines is shown on all of them: \"Thread on lines a–b\", header \"a–b\", mark \"Lines a–b: N posts\", open wash and gutter bar across them","outcome":"pass","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_conversation_cards_view|qt_workbench_line_draft|application_line_draft_contract)$'","detail":"Cards view: a thread with start_line 12 and line 13 is named for both, its header shows 12–13 and its mark spans both; one with no start line keeps 'Thread on line 13'. Window: the thread opened from the menu's Open thread has the open mark with FirstLine 2 for lines 3–5. Composed: the thread the conversations show after Publish has line start+3 and first line start+1. Failing before: 'Thread on line 13'."}},{"criterion":"AC4: the composed application publishes a mouse-made range; related suites, gates and docs","outcome":"pass","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_line_draft_contract|application_label_authoring_contract|qt_workbench_conversation_reveal|qt_workbench_window_scale_100|qt_diff_workbench_scale_100|qt_composed_github_scale_100|qt_composed_inline_drawer_scale_100|qt_composed_long_thread_scale_100|command_correctness_registry_contract|application_review_round_trip_composition_contract)$'; cmake --build --preset dev --target coding-format-check; python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <changed units>; python3 scripts/verify_documentation.py --root .; python3 tests/scripts/verify_documentation_tests.py","detail":"LINE_DRAFT_EVIDENCE:11111111. Results in the Verification section."}}]
---

# BPR-PENDING-AB: A comment on a selection shows its lines, from the menu to the published thread

## Owner note

The owner, 2026-10-21, verbatim, in three messages:

> Am I supposed to be able to leave comments on a line range? Seems like I
> can only do it on a single line

> I would expect to be able to do it from the right click context menu

> On a selection

## What was found

[[BPR-1212]] made a draft on several lines: a drag, a Shift-click or
Shift+arrows select them, and Add comment or Suggest change from a right-click
inside them, or Return and Shift+Return, start one draft whose `StartLine` is
GitHub's `start_line`. Reproduced offscreen through the window's own event
routing (`QWindowSystemInterface`, the event loop turning between events, the
menu the window popped up, its item clicked on it), in a Qt course and in the
composed application over `conventional-comments-v1`, that path works: the
right-click inside a dragged, Shift-clicked or keyboard selection keeps it, the
draft opens on all its lines, and the composed Publish sends `start_line`,
`line`, `start_side` and `side`. What made a range comment look like a
one-line one was its presentation on either side of that path:

1. **The menu did not say it took a range.** On several lines its items read
   "Add comment" and "Suggest change", as on one line. Only the disabled
   caption above them said "Lines a–b".
2. **A published range showed as one line.** Line marks carried the first
   line only for my drafts (`card.Draft && card.FirstLine`), so a published
   thread on lines a–b had its mark, its wash and its gutter bar on line b
   alone and was described "Line b: N posts". Its open card was named "Thread
   on line b" and its header showed "b". [[BPR-1564]] had added the thread's
   `StartLine` to the card for suggestions only. A comment the owner put on
   several lines therefore came back as a thread on its last line.
3. **A right-click on a line number opened nothing.** The numbers are a child
   of the pane, not of its viewport, and `QAbstractScrollArea` drops a
   pointer's menu request that is not its viewport's.

No commit regressed this: none of the three ever worked (`git log -S` puts
the thread's `StartLine` on cards at e3a6dbaff, BPR-1562..1564, for
suggestions only; the open card's name at 201c96f21, BPR-1290; the number
gutter at 7411eb169, BPR-0455).

Also seen, and left to its own card: in the composed course, a selection made
while a kept tab revalidates after a publication's reread is replaced when the
revalidation lands ([[BPR-1713]]). Add comment chosen in that window then
opens nothing, because its line is no longer selected. A right-click inside a
highlight whose selected line a restored position moved away already keeps
the highlighted lines, because it selects the clicked line, which the
highlight holds; the Qt course checks this.

The coordinator's brief called the left pane single-line by design. It is
not: since [[BPR-1212]] v002 the core's `decide_comment_range` takes base
lines on several lines once the host's evidence says the base is numbered as
GitHub's, and refuses them as `base_side` otherwise. The menu then says why
("This file's base is not GitHub's") rather than taking one line, and Suggest
change stays head-only.

## Change

- **Menu.** When the core's decision has a first line, Add comment and
  Suggest change read "Add comment on lines a–b" and "Suggest change on lines
  a–b", or "on base lines a–b". An item that cannot apply keeps its plain
  name, and the row under the items says why.
- **Number gutter.** A right-click on a line number asks the pane for its
  menu at that line, so it behaves as a right-click on the line's code:
  inside the selection it keeps it, and outside it selects that line.
- **Published range threads.** `thread_first_line` gives a card's first line:
  my draft's `FirstLine`, or a published thread's `StartLine` when it is above
  its `Line`. The line mark spans it, so the open thread's amber wash and its
  gutter bar cover all its lines, and the edge mark stays on the last line,
  where GitHub anchors it. The mark is described "Lines a–b: N posts", the
  open card is named "Thread on lines a–b", and its header shows "a–b" with
  "Show lines a–b in the code".
- **Composed course.** `--line-draft-course` gains the owner's course: three
  right lines dragged, a right-click inside them and "Add comment on lines
  a–b" clicked on the popup, each delivered to the window. The draft opens on
  all three, and its Publish sends them. The thread the conversations show
  afterwards is on all three. The course waits for the tab presentation to
  settle first ([[BPR-1713]]). `LINE_DRAFT_EVIDENCE` gains two digits.
- **Docs.** The workbench specification's right-click paragraphs name the
  range items, the line-number menu and a published range thread's
  presentation.

The range rules stay in the core (`decide_comment_range`,
`valid_comment_range`). The window formats their decision and adds no rule.
The shared files touched are `workbench_window.cpp`, `diff_workbench.cpp`,
`conversation_cards_view.cpp`/`.hpp`, `conversation_open_card.cpp`,
`github_application.cpp` (the course only), `main.cpp` (the evidence line) and
`github_application_fixture_access.hpp`. Review submission and Finish review
are untouched.

Not in scope: the one-pane comparison has no code menu at all, so Add comment
is reachable there only by Return, the "+" and Comment. Clicking or
Shift-clicking a line number does not select lines, as GitHub's does. A drag
that ends on an empty line stops at the line above it, because the selection
then ends at that line's start. Comment (Option-C) still opens the selected
line's own draft, not the selection's. These are follow-ups.

## Acceptance

- **AC1.** Right lines selected by a drag in either direction, a Shift-click
  or Shift+Down, followed by Add comment from a right-click inside them (on
  their code or on a line number), or by Return, give one draft on all of
  them. Suggest change fills its block with exactly their lines. A right-click
  outside the selection takes its own line. Base lines take one comment only
  with GitHub's numbering, and the menu says why otherwise.
- **AC2.** On several lines the menu's items name them.
- **AC3.** A published thread on several lines is shown on all of them: its
  name, its header, its mark's description, and its open wash and bar.
- **AC4.** The composed application publishes a range made with the mouse
  and shows the thread on its lines. The related suites and the gates pass.

## Verification

macOS (Apple silicon), 2026-10-21, the `dev` preset's Debug configuration
built with `-g0` and the main checkout's CPM cache.

- **Failing before.** The Qt course run against the unchanged Qt sources
  fails at "Add comment and Suggest change name the lines they take: Add
  comment"; the line-number right-click opened no popup; the cards view's
  range thread was "Thread on line 13". The composed course with the menu
  check relaxed to "Add comment" published the range exactly
  (`LINE_DRAFT_EVIDENCE:11111111`), so the range path itself held.
- **Suites pass:** `qt_workbench_line_draft`, `qt_conversation_cards_view`,
  `qt_workbench_conversation_reveal`, `qt_workbench_window_scale_100`,
  `qt_diff_workbench_scale_100`, `qt_composed_github_scale_100`,
  `qt_composed_inline_drawer_scale_100`, `qt_composed_long_thread_scale_100`,
  `application_line_draft_contract` (three variants) and
  `application_label_authoring_contract`.
- **Renders.** Offscreen renders of the range menu and of an open thread on
  lines 3–5 were checked by eye and kept outside the repository: the items
  read "Add comment on lines 4–6" and "Suggest change on lines 4–6", and the
  amber wash and bar span lines 3 to 5 with the header "3–5".
