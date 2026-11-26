---
schema: "implement-rfc.card.v1"
id: "BPR-2405"
title: "One composer serves the Agent tab and both side chats, with one Send and Stop rule decided in core: Send is Stop while a turn runs and nothing typed could follow it, and words that would follow it make Send Queue or Steer with Stop beside it"
status: "Done"
rfcs: ["RFC-BPR-0130"]
depends_on: ["BPR-2384", "BPR-2400", "BPR-2402"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","reservation":"docs/issues/_reservations/BPR-2405--BPR-2405--2ab0a7eb8430.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"e67c0c4ef","origin":"RFC-BPR-0130 stage 2 (draft card DRAFT-0005 of the attended review 3cd822672efb50572b10ab10747889ba); owner, 2026-11-24: one chat component for all three chats","related_cards":["BPR-2337","BPR-2154","BPR-2000","BPR-1999"],"related_rfcs":["RFC-BPR-0130","RFC-BPR-0126"],"acceptance":["AC1","AC2","AC3","AC4","AC5"],"stage_1_card":"the stage-1 card of RFC-BPR-0130 (one header row), landed before this one; add its ID to depends_on at allocation"}
acceptance: []
---

# BPR-2405: One composer and one Send and Stop rule for every chat

## Why

Owner, 2026-11-24, accepting one chat component (attended review
`3cd822672efb50572b10ab10747889ba`): "We should also more closely reuse
implementation between the side chats and agent chat so that we're not
constantly having to double implement changes." RFC-BPR-0130 stage 2:
`chat_composer` extracted from the Agent tab's field and adopted by both side
chats; one Send and Stop rule.

### What was there (stage 1 of RFC-BPR-0130)

- Three composers built three ways from `composer_parts.hpp`: the Agent tab
  a private `rounded` field around the side chat panel's borrowed words, the
  tour chat and the item chat a `composer_field` declared in
  `tour_side_chat.hpp` with a `passage_chip`, each setting up its words (frame,
  margins, height, focus ring, skill list) itself.
- Three key rules: the Agent tab's (`agent_tab_view::eventFilter`: not while
  an input method composes or the skill list is open, plain Return in an
  empty field does nothing, Alt+Return is not Return), the side chats'
  `side_chat::send_keys` (none of that), and the hidden panel's.
- Three Stop rules: the Agent tab's Send was Stop for as long as a turn ran,
  even with words typed that Return would queue; the tour chat's Send was
  Stop only with nothing typed that could follow, Queue or Steer otherwise
  with Stop beside it; the item chat's Send was Stop while a round ran.

## Change

1. **Core** `chat_send.hpp` (new): `decide_chat_send`, the one rule, the tour
   chat's: words that would follow a running turn (or wait behind a queue
   that holds messages) make Send Queue or Steer (the reviewer's setting),
   with Stop beside it while a turn runs; otherwise Send is Stop while a turn
   runs and Send when none does. An item's chat takes no follow-ups.
2. **Qt** `chat_composer` (new, public): the Agent tab's field made a widget:
   the rounded field (its focus ring while the words have the keyboard), a
   chip slot above the words, the words (no frame, at most about four lines
   tall, Tab leaves), a slot under them, the rule, the send row with the
   chat's tools, Stop and Send at its right end, a slot under the row, and
   the skill list. Return, Shift+Return, Command+Return (Control+Return off
   macOS), the input method and the open skill list are read here and nowhere
   else. It wraps a field another widget made (the Agent tab's, borrowed until
   stage 5). `present` draws Send and Stop as core decided; a chat enables
   Send and writes its tooltip. `passage_chip` moved here; `composer_field`
   and `side_chat::send_keys` are gone.
3. **Agent tab**: places the composer around its words (`agentQuestionField`,
   `agentAsk`, new `agentStop`); its Return filter and its Send/Stop block are
   gone (`present_send`). Words typed while a turn runs now make Send Queue
   or Steer with Stop beside it, as in the tour chat; with nothing typed, Send
   is Stop as before. The picker pill joins the row before Stop.
4. **Tour side chat** and **item side chat**: place the composer; their
   field set-up, key filters and Send/Stop restyling are gone.
5. **Courses**: `core_chat_send` (new), `qt_chat_composer` (new: the keys,
   the input method, each face, Stop beside Send and left of it, Send that
   says Stop stops and sends nothing, a wrapped field kept); the composed
   drawer course holds the Agent tab's `agent-queue` state and the stop chat's
   `tour-stop-chat-queue` to Queue or Steer with Stop beside it, and the Agent
   tab with its field emptied to Send as Stop.
6. **Docs**: `docs/architecture/review-tours.md`, `docs/ux/ai-experience.md`,
   `docs/ux/shared-action-buttons.md`, `docs/ux/workbench-specification.md`,
   `src/adapters/qt/AGENTS.md`.

No `src/app` file changes; the sealed production manifest is untouched.

Not in this card: what a send means (each chat's intents are unchanged); the
suggested prompts, which stay above the composer as each chat already places
the shared `agent_suggestion_list`; the hidden side chat panel's own key
filter (stage 7 deletes the panel's hidden controls); the transcript.

## Acceptance

- AC1 (RFC AC05). One composer widget serves the three chats: Return sends,
  Shift+Return starts a line, Command+Return steers or queues where the chat
  can, and a running turn is stopped from the same control in each.
- AC2. `composer_field`, `passage_chip` and the three set-ups of the field
  have one definition; there is one key rule.
- AC3. Send keeps the place and size `qt_side_chat_tab_and_send` measures.
- AC4. Each chat's send, steer and queue tests pass.
- AC5. The composed course, native look, with the Fixture agent; format and
  tidy on touched units.

## Evidence

Revision `e67c0c4ef`, macOS arm64 Debug, `ctest --test-dir build/dev -R '^<row>$'`.

- AC1 rule: `core_chat_send` passed in dev and in the `core-only` preset
  (idle, running with nothing typed, an item's chat with words typed,
  Queue and Steer with Stop beside, a queue that holds messages with no turn
  running: Queue and no Stop).
- AC1, AC2 widget: `qt_chat_composer` passed (plain Return in an empty field
  sends nothing; Shift+Return starts a line; Return sends; Control+Return and
  Command+Enter send the other way; Command+Return in an empty field still
  asks; Return is the input method's while it composes and sends once it has
  committed; each face's words and accessible name; Stop beside Send and left
  of it; Stop beside stops; a Send that queues sends; a Send that says Stop
  stops and sends nothing; a wrapped field keeps its name and parent).
  `qt_item_side_chat` passed (both side chats compose in a `chat_composer`).
- AC3: `qt_side_chat_tab_and_send_scale_100` and `_200` passed.
- AC4: `qt_tour_side_chat`, `qt_tour_side_chat_width_scale_100`, `_125`,
  `_200`, `qt_workbench_window_scale_100`, `qt_stick_to_bottom`,
  `application_tour_chat_queue_stop_public_path`,
  `_whole_tour_public_path`, `_local_draft_public_path`,
  `application_native_agent_sessions`,
  `application_agent_chat_composition_contract`,
  `application_follow_up_public_path`,
  `application_agent_keep_action_public_path`,
  `application_report_draft_reveal_public_path`,
  `application_local_draft_tour_desktop_public_path`,
  `application_setup_destination_public_path` passed.
- AC5: `application_composed_drawer_fit_scale_100` and `_200` passed (39
  states), now requiring in `agent-queue` and `tour-stop-chat-queue` a Send
  of the one composer that reads Queue or Steer with its Stop beside and left
  of it, and in the Agent tab with its field emptied while the turn runs a
  Send that reads Stop with no Stop beside. The same course under the native
  look (Cocoa, kept off screen, light, the display's 200%): "composed-drawer-fit:
  39 states, 0 faults, 50 transcript edges drawn, dpr 2, cocoa/macos".
  `qt_drawer_surfaces_fit_scale_100`, `_125`, `_200`, `qt_button_faces`,
  `qt_button_faces_cocoa`, `qt_chat_header_view`, `qt_chat_history_menu`,
  `core_chat_header`, `core_chat_history`, `architecture_boundaries` passed.
- `scripts/verify_documentation.py --root .` (DOCUMENTATION_OK) and
  `tests/scripts/verify_documentation_tests.py` passed.
- Format: `_coding-standard/scripts/format.py` (clang-format 22.1.8) over
  every touched C++ file. Tidy over `chat_composer.cpp`,
  `side_chat_parts.cpp`, `item_side_chat.cpp`, `tour_side_chat.cpp`,
  `agent_tab_view.cpp` and the two new tests: nothing on a changed line; the
  same two findings in `agent_tab_view` on lines this card does not touch.

Renders, native look, light, 200%, under
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/rfc130-renders/`:
before is stage 1's `stage-1-after/cocoa-macos/after-light-200-<state>-<width>.png`
(the Agent tab's `agent-queue`: Stop alone in Send's place with words typed),
after is `stage-2-after/cocoa-macos/after-light-200-<state>-<width>.png`
(`agent-queue`: Stop beside Queue; `tour-stop-chat-queue` unchanged in kind).

## Owner judgement

- The Stop rule is the draft card's choice ("Routine unless the owner objects
  at the look"): the Agent tab now shows Stop beside a Send that says Queue
  while words are typed during a turn, where it showed Stop alone.
- The composer's words field is the Agent tab's (at most 90 pixels tall),
  so the side chats' field is taller than its three lines were; the side
  chats' focus ring now also marks the Agent tab's field while its words have
  the keyboard.
- Seen in the renders and not changed here: a side chat's focused words show
  a second, inner focus frame inside the field's own ring (the same in stage
  1's renders, so not from this card).
