---
schema: "implement-rfc.card.v1"
id: "BPR-2409"
title: "The Agent tab's turns are drawn by the one transcript in its card look, from a core projection of its conversation: the question, the steered messages, the reply, the ending line, what the run did, Copy, the one recovery and Delete"
status: "Done"
rfcs: ["RFC-BPR-0130"]
depends_on: ["BPR-2406", "BPR-2407"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","reservation":"docs/issues/_reservations/BPR-2409--BPR-2409--dbcd57cb927b.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"b0a122c19","origin":"RFC-BPR-0130 stage 5 (draft card DRAFT-0009 of the attended review 3cd822672efb50572b10ab10747889ba); owner, 2026-11-24: one chat component for all three chats","related_cards":["BPR-2404","BPR-2405","BPR-2406","BPR-2407","BPR-1847","BPR-1876","BPR-2005"],"related_rfcs":["RFC-BPR-0130"],"acceptance":["AC1","AC2","AC3","AC4"]}
acceptance: []
---

# BPR-2409: The Agent tab's turns are drawn by the one transcript

## Why

Owner, 2026-11-24, accepting one chat component (attended review
`3cd822672efb50572b10ab10747889ba`): "We should also more closely reuse
implementation between the side chats and agent chat so that we're not
constantly having to double implement changes." RFC-BPR-0130 stage 5, first
card (DRAFT-0009): `project_agent_thread` maps the pull request's
conversation to the transcript, and the Agent tab's plain turns are drawn by
`chat_transcript_view` in the Agent tab's card look. Visible: nothing (open
question 1's default: each chat keeps its look).

### What was there (stage 4, main 3c6c5c10e)

- `agent_tab_view` built every turn itself (`add_turn_widgets`, about 530
  lines): the asked time, the question's field, the steered messages, the
  reply card with its reader, ending line, activity block, Copy, recovery,
  Delete and answered time, beside its own parts (the notes over the answer,
  the report card, Draft all, the finding cards, Keep local, Draft, Ask
  more). Its turn record held about sixty members, a dozen of them that
  turn's widgets, which every presenter reached into (`Answer`, `Footer`,
  `Activity`, `Copy`, `Reply`, `Recovery`, `Delete`, `SteeredBox`).
- What an ended turn offers was decided in `present_turn_actions` by calling
  `core::decide_agent_turn_actions` per turn and setting each widget.
- The two side chats drew their turns in `chat_transcript_view` (stages 3
  and 4); the Agent tab did not.

## Change

1. **Core** `project_agent_thread` (`chat_transcript.cpp`): a turn each
   `agent_thread_entry` (the tab's turn as the core reads it: serial,
   question, steered messages, answer, done, outcome, its line as said, what
   its run did, whether its cause passed since), identity
   `agent-turn-<serial>`; a turn not done runs; an ended turn's line as it
   was said (`chat_ending::Said`, which `chat_ending_words` says as kept: a
   stored conversation keeps the line, not what it was made from) and what it
   offers (`decide_agent_turn_actions` with `agent_thread_facts`: a run
   going, the pinned-revision card asking).
2. **Qt** `chat_transcript_view` gains the card look
   (`chat_transcript_look::card`): the column the Agent tab had (12, 8, 12,
   12; 10 between items; its turns in their own column, `agentThread`, shown
   once there is a turn); a turn as the Agent tab drew it (`card_turn_view`):
   the asked time over the question's outlined field, the steered messages,
   the reply's card (`agent_card_frame.hpp`, the Agent tab's `rounded` moved
   out of its source) with the reader, the ending line, the activity block
   and the foot (Copy, the chat's actions, the one recovery, Delete, the
   answered time). The ending line settles from the turn's ending; the card's
   error look, Copy, Resend or Agent settings…, and Delete are drawn from the
   turn's actions; Resend, Agent settings… and Delete leave as gestures
   naming the turn (`chat_turn_gesture::agent_settings` is new). The chat's
   own parts are made once with the turn: `DressBubble` (what it was about,
   the prompt's name, Show prompt, the question's words and links),
   `DressCard` (the slots over and under the reply), `DressFoot` (its
   actions after Copy); `ReplyBody` and `ReplyIdentity` say what the reader
   shows under which identity; `DressEnding` gives the line its tooltip. A
   card's reader is fitted at once to the column's width, as the tab did.
3. **Agent tab**: the conversation is the transcript, its state card, ready
   card and empty line its lead, the replies, findings, suggested prompts and
   queue its trail. `add_turn_widgets`, `show_steered`, `fit_answer`, the
   thread's own column and the per-turn widget members are gone; the turn
   record keeps what it shows (`Body`, `BodyKey`, `Line`, `LineTip`, `Ran`)
   and the tab's own parts, which `dress_question`, `dress_card` and
   `dress_foot` make as before. `present_transcript` projects the turns and
   presents them; a reply that grows, a turn that settles and a deleted turn
   change their own turn alone. `present_turn_actions` reads what the
   projector decided for the fixed-since note, the keep action and Ask more.
   A thread drawn again from its stored history is presented anew, as it was
   made anew before.
4. **Courses**: `core_chat_transcript` (the Agent tab's projection: identity,
   the kept line, Copy and the keep action, a running turn, a growing answer
   named alone, an error's outcome, line, recovery and Delete, none while a
   run goes or while the pinned-revision card asks, a deleted turn one
   removal); `qt_chat_transcript_view` (the card look: the turns' column
   waits for a turn, the kept line, a growing answer keeps every other
   turn's widgets and its own reader, the chat's parts made once a turn, an
   error's card, Resend and Delete as gestures naming the turn, a deleted
   turn leaving alone).
5. **Docs**: `src/adapters/qt/AGENTS.md` (the Agent tab's projector and its
   card look).

No `src/app` file changes; the sealed production manifest is untouched.

Not in this card: the finding cards, the report card and verdict, and Draft
all as attachments (DRAFT-0010); the live turn's reducer in core and the
borrowed controls (DRAFT-0011).

## Acceptance

- AC1 (RFC AC06 for the Agent tab, DRAFT-0009). The Agent tab draws its
  turns in `chat_transcript_view`; while a reply grows, the widgets of the
  other turns are the same objects.
- AC2 (DRAFT-0009). A stored conversation opens as it did, turn for turn.
- AC3. What an ended turn offers is the projector's decision, drawn by the
  view.
- AC4. The Agent tab's tests and the composed course in the native look pass
  with every render byte-identical; format and tidy on touched units.

## Evidence

Revision `b0a122c19` (rebased on main `9c815303c`), macOS arm64 Debug,
`ctest --test-dir build/dev -R '^<row>$'`.

- AC1, AC3: `core_chat_transcript` (the Agent tab's projection) and
  `qt_chat_transcript_view` (the card look) passed.
- AC2: `qt_agent_restored_byline`, `qt_agent_finding_cards_restart`,
  `qt_agent_finding_cards_retained`, `application_agent_chat_composition_contract`
  passed.
- AC4: `qt_workbench_window_scale_100`, `_200`, `qt_agent_report_summary_scale_100`,
  `_125`, `_200`, `_native_look`, `qt_report_attachment_title`,
  `qt_agent_codex_launcher_turn`, `qt_agent_tone_colours`, `qt_agent_footer`,
  `qt_agent_activity_view`, `qt_agent_launch_panel`, `qt_agent_report_view`,
  `qt_agent_session_menu`, `qt_stick_to_bottom`, `qt_transcript_edge`,
  `qt_prompt_macro_chips`, `qt_skill_completion_keys`, `qt_button_faces`,
  `qt_markdown_code_1_0`, `_1_25`, `_2_0`, `qt_drawer_surfaces_fit_scale_100`,
  `qt_item_side_chat`, `qt_tour_side_chat`, `qt_side_chat_tab_and_send_scale_100`,
  `_200`, `qt_review_tour_agent_failure`; `application_agent_finding_composition_contract`,
  `application_agent_keep_action_public_path`, `application_agent_steer_sent_public_path`,
  `_unavailable_public_path`, `application_explain_file_public_path`,
  `application_suggest_change_public_path`, `application_returning_review_latest`,
  `_addressed`, `_gone`, `_mark_public_path`, `application_review_report_public_path`,
  `application_report_draft_reveal_public_path`,
  `application_composed_drawer_fit_scale_100`, `_200` passed. The composed
  course under the native look (Cocoa, kept off screen, light, the display's
  200%): "composed-drawer-fit: 39 states, 0 faults, 50 transcript edges
  drawn, dpr 2, cocoa/macos".
- Format: `_coding-standard/scripts/format.py` (clang-format 22) over every
  touched C++ file. Tidy (`tidy.py --warnings-as-errors`) over
  `chat_transcript.cpp`, `chat_transcript_view.cpp`, `agent_tab_view.cpp` and
  both tests: clean but for one finding older than this card
  (`performance-unnecessary-copy-initialization` on `press_offer`'s copy of
  `aChosen`, which guards against the offer list changing under it; left as
  it was). The `press_offer` declaration's parameter name now matches.
- `scripts/verify_documentation.py --root .`: its one error is
  `docs/issues/NEXT_ID` (BPR-2408) behind the history's high water
  (BPR-2409), a tracker state this card does not touch.

Renders, native look, light, 200%, under
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/rfc130-renders/`:
before is `stage-4-after/cocoa-macos/`, after is `stage-5a-after/cocoa-macos/`
(`after-light-200-<state>-<width>.png`). All 274 renders are byte-identical,
the Agent tab's states (`agent-*`) among them.

## Owner judgement

- None asked: nothing changes on screen.
