---
schema: "implement-rfc.card.v1"
id: "BPR-2406"
title: "A chat's conversation is one core value with a delta between two, every chat ends a turn with one line in the same words, and the item side chat draws its rounds in the one transcript, which changes only the turns that changed"
status: "Done"
rfcs: ["RFC-BPR-0130"]
depends_on: ["BPR-2404", "BPR-2405"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","reservation":"docs/issues/_reservations/BPR-2406--BPR-2406--25450e81bce7.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"b19614ddc","origin":"RFC-BPR-0130 stage 3 (draft cards DRAFT-0006 and DRAFT-0007 of the attended review 3cd822672efb50572b10ab10747889ba); owner, 2026-11-24: one chat component for all three chats","related_cards":["BPR-2404","BPR-2405","BPR-2322","BPR-2296"],"related_rfcs":["RFC-BPR-0130"],"acceptance":["AC1","AC2","AC3","AC4","AC5"]}
acceptance: []
---

# BPR-2406: One transcript value, its delta and one ending line; the item side chat adopts the one transcript

## Why

Owner, 2026-11-24, accepting one chat component (attended review
`3cd822672efb50572b10ab10747889ba`): "We should also more closely reuse
implementation between the side chats and agent chat so that we're not
constantly having to double implement changes." RFC-BPR-0130 stage 3:
`chat_transcript`, `chat_delta`, `chat_transcript_view`; the item's side chat
adopts it, with the follow-up proposal as the first attachment. Visible: one
ending line.

### What was there (stage 2, main 4159e5f7d)

- No conversation value the chats share: the Agent tab's `agent_thread_turn`,
  the tour's `core::tour_agent_run`, the item's `core::follow_up_round`.
- The item side chat drew every turn again on each change
  (`item_side_chat::rebuild()`: `side_chat::clear` and every bubble, reader,
  ending line and the proposal card made anew), as the tour chat still does.
- Three ending lines built three ways: the Agent tab's footer parts, the
  tour chat's own sentence ("Claude Code answered in 0:07."), the item chat's
  ("Claude answered."); the item chat's replies took no code colouring.

## Change

1. **Core** `chat_transcript.hpp` (new). `chat_transcript`: turns by stable
   identity, each the question, the steered messages, the reply (Markdown),
   whether it runs, its `chat_ending` (who, seconds, cost, findings), why it
   failed, its activity, its `agent_turn_actions` and typed
   `chat_attachment`s (`chat_follow_up_proposal` first).
   `chat_transcript_delta`: removals (by the earlier place), then each later
   turn appended, `reply_grew`, `settled` or `changed`, one change a turn,
   none for an equal turn; a kept turn out of its order is removed and
   appended again. `chat_ending_words` (and `chat_ending_first_words`,
   `chat_agent_short_name`, `chat_elapsed_words`): the one ending line,
   "Claude answered in 0:42 · $0.14", "Codex answered in 1:03.", "Stopped
   after 0:12", "Stopped when the app closed, after 16:46".
   `project_follow_up`: a turn each round (identity the item's key and the
   round), the question going as the next round's running turn, a proposal's
   run and the proposal it writes as one turn (the run's identity) whose
   attachment it is; `follow_up_apply_words`.
2. **Qt** `chat_transcript_view` (new, public): the chat's scroll area, at
   its end (`stick_to_bottom`) and edged (`transcript_edge`); `present`
   applies the delta by identity: an appended turn is made at its place, a
   removed one goes, a changed one is changed in place (its reply set again
   in its own reader, its ending line settled, an attachment that differs
   made again by the chat's factory). A turn in the side chats' look: the
   bubble, steered bubbles, the reply, why it failed, the ending line with
   Copy, Resend and Delete as the turn offers; the chat's own items above the
   turns (`lead`) and under them (`trail`); each press a gesture naming the
   turn.
3. **Item side chat**: draws its rounds in the view from
   `core::project_follow_up`; `rebuild()` and `reader()` are gone; Use this
   and Discard are the proposal attachment; the item and the invitation are
   the view's lead. Its replies' code asks the window's highlighter
   (`set_code_syntax_dispatch`, `apply_code_syntax`, wired in
   `workbench_window`), as the Agent tab's does.
4. **One ending line**: the tour chat's settled turns and the Agent tab's
   turn footer say `core::chat_ending_words`; `agent_turn_ending_words` and
   `agent_elapsed_words` are core's words. The tour chat now names the agent
   short, as the Agent tab does ("Claude answered in 0:07.").
5. **Courses**: `core_chat_transcript` (new: the ending words; equal
   transcripts change nothing; a growing reply names its turn; settled,
   changed, removed, inserted and moved turns; the item chat's projection
   through ask, answer, propose, a question while the proposal waits, a
   failed question and Discard, another item); `qt_chat_transcript_view`
   (new: while a reply grows and settles, a turn is appended, an attachment
   is kept or made again, a failed turn offers Resend and Delete and a turn
   is removed, every other turn's widgets are the same objects; the item chat
   keeps its earlier rounds' widgets through a new question and its answer).
   `qt_item_side_chat` reads the invitation by visibility, since it is no
   longer deleted; `qt_tour_side_chat` holds the tour chat's ending to the
   one line.
6. **Docs**: `src/adapters/qt/AGENTS.md` (a chat draws its conversation in
   the one transcript and does not rebuild its column),
   `docs/ux/workbench-specification.md` (the one ending line).

No `src/app` file changes; the sealed production manifest is untouched.

Not in this card: the tour side chat's transcript (stage 4), the Agent tab's
(stage 5), one notice (stage 6), the item chat on the session runner (stage
7, open question 4). `follow_up_state`, `tour_agent_session` and stored
conversations keep their shapes.

## Acceptance

- AC1 (DRAFT-0006). A reply that grows yields one delta naming that turn; a
  removed turn yields one removal; equal transcripts yield none.
- AC2 (DRAFT-0006). The ending line's words come from one function for every
  chat.
- AC3 (RFC AC06 for the item side chat, DRAFT-0007). The item side chat
  draws its turns in `chat_transcript_view`; while a reply grows (and as a
  round is asked and answered) the widgets of the other turns are the same
  objects; `rebuild()` is gone; Use this and Discard are its proposal
  attachment.
- AC4 (DRAFT-0007). The item chat's replies take the same code colouring as
  the other chats' (verified first: its reader took no syntax dispatch).
- AC5. The item chat's tests, the follow-up public path and the composed
  course in the native look pass with no visible regression; format and tidy
  on touched units.

## Evidence

Revision `b19614ddc`, macOS arm64 Debug, `ctest --test-dir build/dev -R '^<row>$'`.

- AC1, AC2 rules: `core_chat_transcript` passed in dev and in the
  `core-only` preset.
- AC2 in each chat: `qt_tour_side_chat` ("Claude answered in 0:07."),
  `qt_agent_activity_view` (`agent_turn_ending_words` through core),
  `qt_workbench_window_scale_100` (the Agent tab's footer "… answered in … ·
  $0.41" and its tooltip) passed.
- AC3: `qt_chat_transcript_view` passed; `qt_item_side_chat`,
  `qt_stick_to_bottom` (the item chat follows new rounds at its end and a
  reader scrolled up stays put), `qt_side_chat_tab_and_send_scale_100`,
  `_200` passed.
- AC4: `qt_item_side_chat` and the composed course passed with the window's
  relay set on the item chat; the item chat's reader is dressed with the
  window's highlighter dispatch exactly as the Agent tab's.
- AC5: `application_follow_up_public_path`,
  `application_composed_drawer_fit_scale_100` and `_200` (39 states) passed.
  The same course under the native look (Cocoa, kept off screen, light, the
  display's 200%): "composed-drawer-fit: 39 states, 0 faults, 50 transcript
  edges drawn, dpr 2, cocoa/macos". `qt_tour_side_chat_width_scale_100`,
  `architecture_boundaries`, `architecture_boundary_contract` passed.
- `scripts/verify_documentation.py --root .` (DOCUMENTATION_OK) and
  `tests/scripts/verify_documentation_tests.py` passed.
- Format: `_coding-standard/scripts/format.py` (clang-format 22) over every
  touched C++ file. Tidy (`tidy.py`) over `chat_transcript.cpp`,
  `chat_transcript_view.cpp`, `item_side_chat.cpp`, `agent_run_status.cpp`
  and the two new tests: clean.

Renders, native look, light, 200%, under
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/rfc130-renders/`:
before is stage 2's `stage-2-after/cocoa-macos/after-light-200-<state>-<width>.png`,
after is `stage-3-after/cocoa-macos/after-light-200-<state>-<width>.png`. All
274 renders are byte-identical: the composed course's agent is the Fixture
agent ("Fixture agent", no " Code" to shorten, no reported cost), so the
one ending line reads as each chat's did, and the item chat's column
(`item-chat-answered` at 260 to 420 px) is drawn the same by the one
transcript.

## Owner judgement

- None asked. The visible change is the words of a turn's ending line
  where an agent named "… Code" answered in the tour chat ("Claude Code
  answered in 0:07." now reads "Claude answered in 0:07.", as the Agent tab's
  always did) and where an item chat's agent reported a cost; the RFC's
  accepted words ("One ending line: who, how long, what it cost").
