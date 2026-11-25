---
schema: "implement-rfc.card.v1"
id: "BPR-2392"
title: "The right drawer is measured and drawn in the application's own window: a composed course holds Comments, the Agent tab and the tour chat to no sideways pan, no cut-off control and no chip over another at 280 to 420 px, and the faults it found are fixed"
status: "Done"
rfcs: ["RFC-BPR-0130", "RFC-BPR-0085"]
depends_on: ["BPR-2384", "BPR-2389"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","reservation":"docs/issues/_reservations/BPR-2392--BPR-2392--f1f1c0fe897b.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"465859ba1","origin":"owner, attended review 5627b21a56838de9fae9f2ac4e4349d4, 2026-11-25, rejecting looks 08 (tour chat narrow) and 10 (replay pinned card); verbatim below","related_cards":["BPR-2384","BPR-2389","BPR-2379","BPR-2005","BPR-1934"],"related_rfcs":["RFC-BPR-0130","RFC-BPR-0085","RFC-BPR-0096","RFC-BPR-0103"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6","AC7"],"owner_step":"attended look at the tour chat and the Agent tab's stopped card in a frozen build (see Owner judgement)"}
acceptance: []
---

# BPR-2392: The right drawer is measured and drawn in the application's own window

## Why

Owner, attended review `5627b21a56838de9fae9f2ac4e4349d4`, 2026-11-25, on the
frozen build of main 10ee349ff.

Look 08, the tour chat at its narrowest (BPR-2384):

> No, doesnt look like your screenshots.

Look 10, the card of a transcript replay that read an earlier head (BPR-2379,
BPR-2389):

> This made the drawer have a horizontal scroll which should have been
> disallowed by our tests.

### What was wrong with the evidence

Both looks were drawn and measured in a window nothing composed.

- The tour chat's renders (`qt_tour_side_chat_width`) drew the chat as a
  window of its own, 300 px wide, holding sessions built by hand. The
  application puts the chat in the Tour tab's second column, beside a tour
  that keeps 340 px, in a drawer no narrower than 660 px, so the column is
  313 px at its narrowest; and a real run adds what a canned session has
  not: the line that folds what the agent did, the suggested prompts of a
  chat nothing was asked in, the proposal on the stop beside it.
- The pinned card's renders and `qt_drawer_surfaces_fit` used a workbench
  window with no application behind it, and that course held the refused
  turn under the card only to `require_whole`: its comment excused "a code
  block, which pans inside itself".

### What the application's window showed (main 947caf003)

A new course drives the desktop composition with the Fixture agent
(`before-*` renders and logs). It found, under the macOS style:

1. **Every answered turn of every chat** ends in the folded activity line
   ("Worked for 0 s: 1 file read") drawn as a platform bezel: a grey framed
   box with a large arrow round small words. It was a plain auto-raise
   `QToolButton` with an arrow, which this adapter's own rule forbids; no
   test showed it because only a real run folds an activity.
2. **A chat nothing was asked in** shows its suggested prompts as chips each
   drawn over the foot of the one above (the Agent tab and the tour chat).
   The chip paints its whole rect and the macOS style lays a push button out
   by a smaller bezel.
3. **The tour beside the chat**, at the 340 px the chat leaves it, cut the
   second line of an unopened stop's explanation in half: the row's height
   was the explanation's length over the width, which is a line short when a
   word does not fit the end of the first.
4. **The stopped card's turn** said the refusal twice: once in words, and
   again as "It wrote:" in a code block with a horizontal scroll bar. The
   application had refused the question before any agent ran; the words were
   its own.
5. **Draft Tour** was drawn a pixel over the tool wrapped under it at 280 and
   300 px.

The card's own buttons, which BPR-2389 wrapped after the frozen build was
made, hold in the composed window: no state at any width shows a horizontal
bar, has sideways range, or holds content wider than its viewport.

## Change

1. **The composed course** (`tests/end_to_end/composed_drawer_fit_public_path.cpp`):
   the desktop composition on returning-review-v1, signed in, with the
   Fixture agent, in Review on the renderer. Fifteen states, each reached by
   the window's own gestures: Comments; the Agent tab ready, answered, with a
   review's report, and with a turn running over a pending list; the Tour
   tab's route; the tour chat about a stop new, answered, with a turn running
   over a pending list, and with a suggested rewrite; the whole tour's chat
   answered and with a proposed revision; then, after the author pushes and
   Refresh reads the new head, the card of a transcript replay that read the
   earlier head, the stopped card of a question whose session's workspace was
   not moved, and the tour chat whose session read the earlier head. In each
   the drawer's handle is dragged to 280, 300, 340 and 420 px (for the chat
   column: the drawer as narrow as it goes, then the column at each width it
   can have), and the window is made as narrow as it goes. It refuses a
   horizontal scroll bar, sideways range, content wider than its viewport, a
   button or label outside the drawer or its scrolled content, a button
   narrower than its words, a button over another (a chip's face is its whole
   rect), and a tool button with words or an arrow that the platform style
   draws. Negative controls show each check failing on what it claims.
   CTest runs it offscreen at 100% and 200% and, on macOS, under Cocoa with
   every window kept off screen.
2. **Renders come from that window.** `BPR_COMPOSED_DRAWER_RENDER_ROOT` saves
   the drawer in each state at each width and the whole window at each
   state's narrowest, under the native look. `qt_tour_side_chat_width` keeps
   its checks and saves no renders.
3. **Fixture agent**: a tour chat's update of a stop proposes a new
   explanation for it, and "Drop stop N" in the whole tour's chat proposes
   that removal, so a look and a course reach a proposal and its choices.
4. **Qt fixes**: the folded activity line is a quiet chip with a chevron in
   the status line's faint ink; the quiet notice's two choices wrap; the
   suggested prompts' chips and Draft Tour are laid out by the rect they
   paint; a route row is as tall as the lines its explanation is drawn on; a
   refusal that is the application's own is said once.
5. **Guidance** (`src/adapters/qt/AGENTS.md`): the composed course is where a
   right drawer state is held to the rule and where its look is drawn; a
   chip that paints its whole rect is laid out by it.

Not in this card: a code block keeps its lines and pans inside its own frame
until its Wrap is pressed (the block itself is held to the rules); the card
of work in the way of a move (five choices) is not reached in the composed
course, because the tour's agent holds the pull request's checkout there, and
stays with `qt_drawer_surfaces_fit`; the Agent tab's "Native continuation"
line, which has no left inset, is RFC-BPR-0130 stage 1's.

## Acceptance

- AC1. In the application's own window with the Fixture agent, every state
  of the right drawer the course reaches, at 280, 300, 340 and 420 px and in
  the narrowest window, at 100% and 200%: no horizontal scroll bar, no
  sideways range, no content wider than its viewport.
- AC2. In the same states: every button and label lies inside the drawer, no
  button is narrower than its words, none lies over another, and no tool
  button with words or an arrow is drawn by the platform style.
- AC3. The checks fail on what they claim to catch, and the course fails if a
  state is not reached.
- AC4. The tour chat in the composed window matches the accepted design: one
  header row, the shared History menu, wrapping buttons, nothing cut.
- AC5. The stopped card's turn says a refusal once, with no code block.
- AC6. Renders of these surfaces come from the composed window.
- AC7. Format, tidy on touched units.

## Evidence

Revision `465859ba1` (rebased on main e3c4b08bc), macOS arm64 Debug, `ctest
--test-dir build/dev -R '^<row>$'`.

- AC1, AC2, AC3: `application_composed_drawer_fit_scale_100`, `_scale_200`
  and `_cocoa` passed (fifteen states, each at its widths and in the
  narrowest window; the negative controls first; "every state of the course
  is reached, in order"). Run by hand under the native look (Cocoa,
  QMacStyle, nothing on screen), Light and Dark at 100% and 200%, the course
  passed four times with no fault (`after-native-<scheme>-<percent>.log`).
- The same course on main's `src/adapters/qt` (947caf003; the course and the
  Fixture agent as here) reported 68 faulted state-and-width lines in each
  of those four runs (`before-native-<scheme>-<percent>.log`), none of them a
  pan: the activity line "is drawn by the platform style" in every state
  with an answered turn (80 mentions a run), a suggested prompt's chip "lies
  over" the next in every state that offers prompts, and Draft Tour over the
  reset tool at 280 and 300 px. No log, before or after, holds "pans
  sideways", a horizontal scroll bar or content wider than its viewport: the
  pinned card's wrapped choices (BPR-2389) hold in the composed window.
  Offscreen (Fusion) the same sources report nothing, which is why only a
  Cocoa row guards the chips and the bezel.
- AC4: the `after-*` renders of `tour-stop-chat-*`, `tour-whole-chat-*` and
  their `-narrowest-drawer-313` and `-narrowest-window-660` forms: one header
  row (part and place each one shortened line, Show, History, close), the
  activity line as quiet words, the prompts' chips apart, the suggestion's
  and the revision's choices wrapped, the tour's rows whole beside it.
  `qt_tour_side_chat` and `qt_tour_side_chat_width_scale_100`, `_125` and
  `_200` passed; `application_tour_chat_queue_stop_public_path` and
  `_whole_tour_public_path` passed on the changed Fixture agent, and
  `_local_draft_public_path` on the changed Qt sources.
- AC5: `qt_drawer_surfaces_fit_scale_100`, `_125` and `_200` passed with the
  pinned card's refused state held to the whole drawer rule (`require_drawer`
  in place of the excused `require_whole`) and the new check that the refused
  turn says "No question was sent." once and never "It wrote".
  `application_agent_invocation_native_pinned_contract` passed. Composed:
  `after-*-agent-pinned-refused-*.png` against `before-*`, where the turn
  under the card holds a `text` code block with its own scroll bar.
- The fixes' own rows: `qt_agent_activity_view` passed (the folded line is a
  `quiet_tool_button`, bare until opened, with a chevron, no wider than the
  turn; the quiet notice's two choices lie whole inside a 220 px column, one
  under the other); `qt_button_faces` and `qt_button_faces_cocoa` passed;
  `qt_tour_chat_in_tab`, `qt_tour_stop_card` and `qt_tour_stop_card_cleanup`
  passed.
- `architecture_boundaries`, `test_network_isolation_manifest` (the three new
  rows pinned "unmeasured (registered on macOS)") and
  `test_network_isolation_structure_contract` passed.
  `scripts/verify_documentation.py --root .` printed `DOCUMENTATION_OK` (with
  the main checkout's `implement-rfc/scripts` on `PYTHONPATH`, which a
  worktree without the MASK-deployed skills needs).
- The Fixture agent's own rows: `fixture_agent_answers`,
  `application_fixture_agent_guard_public_path` and
  `application_fixture_agent_every_ask` passed on the agent that now proposes
  a stop's explanation and a stop's removal.
- AC6: 420 `before-*` and 420 `after-*` renders, all drawn by the composed
  course from the application's window, in
  `/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/composed-drawer-renders/cocoa-macos/`
  as `<before|after>-<light|dark>-<100|200>-<state>-<width>.png`, with
  `…-window.png` for the whole window at each state's narrowest width and in
  the narrowest window; the eight runs' logs are beside that folder.
- AC7: `format_check.py` (clang-format 22.1.8) passed on the eleven touched
  C++ files. `tidy.py --warnings-as-errors` over the ten touched translation
  units reports nothing on a changed line; its findings are in lines this
  card does not change (`agent_tab_view.hpp:469`, `agent_tab_view.cpp:2700`,
  `local_draft_workflow_area.hpp:72`, `review_tour_editor.cpp:2194`,
  `fixture_agent_main.cpp:97`, `:124` and `:482`).

Not covered by a row: the route row's height against the lines it draws has
no check of its own (the measure and the paint now use one wrap; the
`before-*` and `after-*` renders of `tour-stop-chat-*-narrowest-drawer-313`
show the cut line and the whole one). Windows and Linux have not run the new
rows.

## Owner judgement

- The look of the tour chat and of the Agent tab's cards is the owner's to
  accept, from a frozen tests-on build with the Fixture agent; the `after-*`
  renders are that window's own drawing.
- The folded activity line is now quiet words with a chevron and no frame.
  Opened, it wears the chip's raised face, as a checked quiet choice does.
- A code block still pans inside its own frame; say so if a block in the
  drawer should wrap by default instead.
