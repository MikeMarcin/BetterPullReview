---
schema: "implement-rfc.card.v1"
id: "BPR-2413"
title: "Every chat says a refused send, a stopped run, a moved head and a carried-over conversation with one notice planned in core; the tour chat's moved head is the pinned-revision card"
status: "Done"
rfcs: ["RFC-BPR-0130"]
depends_on: ["BPR-2405", "BPR-2407", "BPR-2412"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","reservation":"docs/issues/_reservations/.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"b01adaf9c","origin":"RFC-BPR-0130 stage 6 (draft card DRAFT-0012 of the attended review 3cd822672efb50572b10ab10747889ba); owner, 2026-11-24: one chat component for all three chats","related_cards":["BPR-2389","BPR-2393","BPR-2402","BPR-2404","BPR-2407","BPR-2412"],"related_rfcs":["RFC-BPR-0130","RFC-BPR-0096"],"acceptance":["AC1","AC2","AC3","AC4"]}
acceptance: []
---

# BPR-2413: One notice for every chat

## Why

RFC-BPR-0130 AC08: "A send refusal, a stopped run, a moved head and a
carried-over conversation are each shown by the one notice, with the same
words, in every chat that can reach that state." Stage 6 of the RFC's table:
"`chat_notice` for the refusal strip, the banner, the pinned-revision card,
the tour's notice and the item's status line. Visible: one notice style."

### What was there (BPR-2412)

- The Agent tab built three notices of its own: the send-refusal strip (a
  danger card with a close), the state banner (the pinned-revision card, a
  stopped run's card with Resend and Show the log, the run working or
  finished) and the session's quiet line. Every word of them was the tab's.
- The tour chat said a moved head in a line of its own, "Head changed. This
  chat reads b20cb4434282; current head: cd24836cfb58. Choose New session in
  History or Continue old revision.", with a Continue old revision button
  under it, twelve characters a revision where the Agent tab's card says
  seven, and other words for its choices. After Continue it went on saying
  "Continuing saved revision …". Its suggested prompts waited with "Choose
  New session in History or Continue old revision first."
- The item chat said what happened in a status line of its own.

## Change

1. **Core** (`bpr/core/chat_notice.hpp`): `chat_notice` (kind, form, tone,
   title, words, tip, choices, whether it closes) and its planners:
   `plan_send_refused_notice`, `plan_moved_head_notice` (the pinned-revision
   card before a question and after a refused one: Continue at the revision
   read, then New session; seven characters; each choice starts a run only
   after a refused question), `chat_moved_head_wait_words`,
   `plan_stopped_notice` (Resend, Show the log beside it),
   `plan_session_notice` (`chat_session_notice`'s words) and
   `plan_said_notice` (a chat's own line). Core test `core_chat_notice`.
2. **Qt** (`chat_notice_view`): the one notice view in the three forms the
   notices had (a quiet line, a strip with a close, a card with a title, its
   words, two choices and a close), drawing a `core::chat_notice` and
   reporting the choice pressed. The card paints with the Agent tab's card
   painter, now a shared function (`agent_card::paint_card`).
3. **Agent tab**: its banner, refusal strip and session line are
   `chat_notice_view`s; `present_banner` plans each state's notice and maps
   a choice to the gesture it was (`choose_on_banner`). Object names, words
   and layout are as they were.
4. **Tour chat**: the session line is the notice's line; the moved head is
   the pinned-revision card (`head_moved_card()`), its New session History's
   own and greyed with History's reason; once Continue is chosen the card
   has been answered and History's last line names the revision. Prompts
   wait with `chat_moved_head_wait_words`. The tour editor's writing notice
   names the card's choices.
5. **Item chat**: its status line is the notice's line.
6. **Courses**: `qt_tour_side_chat` checks the card's words and choices,
   that no line says "Head changed", and that Continue answers it; the
   composed course checks the stop chat's card in the native look; the
   agent invocation course presses the card's Continue.
7. **Docs**: `src/adapters/qt/AGENTS.md`.

## Acceptance

- AC1 (RFC AC08). A send refused, a stopped run, a moved head and a
  carried-over conversation (or one the agent cannot pick up) are each a
  `core::chat_notice` drawn by `chat_notice_view`, worded once in core.
- AC2. The tour chat's moved head is the pinned-revision card, with the
  Agent tab's title, words and choices, in the same order; nothing says
  "Head changed".
- AC3. The Agent tab and the item chat look as they did: every composed
  render of theirs matches stage 5c's but for timing.
- AC4. Format and tidy on touched units.

## Evidence

Revision `b01adaf9c` (on main `989da1b33`), macOS arm64 Debug,
`ctest --test-dir build/dev -R '^<row>$'`.

- AC1: `core_chat_notice` (each planner's words, choices, order, tone and
  close; equal facts give equal notices; no internal word; the chosen agent
  named), `core_chat_header`, `qt_tour_side_chat`, `qt_item_side_chat`
  passed. The Agent tab, the tour chat and the item chat construct no
  notice label or card of their own for these states.
- AC2: `qt_tour_side_chat` (the card's title "This session read b20cb44",
  "The pull request has moved on.", Continue at b20cb44 then New session,
  equal to `core::plan_moved_head_notice`; no label says "Head changed";
  Continue answers it; prompts wait with "Choose Continue or New session
  first.") passed. The composed course under the native look checks the
  stop chat's card ("This session read <head>", "Continue at <head>", New
  session enabled): "composed-drawer-fit: 39 states, 0 faults, 50 transcript
  edges drawn, dpr 2, cocoa/macos".
- AC3: `qt_workbench_window_scale_100`, `_125`, `_200` (the Agent tab's
  refusal strip, banner, pinned and stopped cards by their words and object
  names), `qt_drawer_surfaces_fit_scale_100`, `_200` passed. Renders, native
  look, light, 200%, under
  `/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/rfc130-renders/`:
  before is `stage-5c2-after-rerun/cocoa-macos/`, after is
  `stage-6-after/cocoa-macos/` (274 each): 165 byte-identical; the intended
  change is `tour-stop-chat-head-moved-*` (the card where the line and
  button were); the rest differ where a run's timing shows, as BPR-2412
  recorded (the question field's caret, a 2 by 31 box; the window's run
  clock; the code pane's overlay scroll bar), and in
  `agent-pinned-offered-280` and `-300`, where the lines chip and the
  Comments count had not arrived yet (both are in `-340` and `-420` of the
  same run). The Agent tab's notices are pixel for pixel as they were in
  every render.
- AC4: `_coding-standard/scripts/format.py` (clang-format 22) over every
  touched C++ file. `tidy.py --warnings-as-errors` over `chat_notice.cpp`,
  `chat_notice_view.cpp`, `item_side_chat.cpp`, `tour_side_chat.cpp`,
  `agent_tab_view.cpp`, `chat_notice_tests.cpp`: clean but for the one
  finding older than this card (`press_offer`'s copy of `aChosen`).

## Owner judgement

- The tour chat's moved head as the pinned-revision card is a change of
  look the RFC's stage 6 names ("one notice style"); it is part of AC09's
  final attended look.

## Not in this card

- The item chat cannot reach a moved head or a carried-over session until
  it keeps sessions (stage 7). Its own outcomes (a proposal that could not
  be used, a round that failed) stay its own words on the notice's line.
- The tour chat reaches no send refusal in the Agent tab's sense: its Send
  is withheld, with why in its tooltip.
