---
schema: "implement-rfc.card.v1"
id: "BPR-2402"
title: "The stopped card's New session opens the new session and sends there; a stopped run reports on the question field; prompts wait while the revision card does; a tour chat's first question starts from a fresh answer; and the chat's suggestion card has the pencil and the trash"
status: "Done"
rfcs: ["RFC-BPR-0130", "RFC-BPR-0096", "RFC-BPR-0101"]
depends_on: ["BPR-2400", "BPR-2393", "BPR-2394", "BPR-2389", "BPR-2399"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","reservation":"docs/issues/_reservations/BPR-2402--BPR-2402--0d9ebf215ecb.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"88fac1032","origin":"owner, 2026-11-25, relayed by the coordinator as five follow-ups left by BPR-2389, BPR-2393, BPR-2394 and BPR-2400 (each card's \"Not in this card\"), with their defaults decided","related_cards":["BPR-2400","BPR-2399","BPR-2394","BPR-2393","BPR-2392","BPR-2389","BPR-2296","BPR-2138","BPR-1995"],"related_rfcs":["RFC-BPR-0130","RFC-BPR-0096","RFC-BPR-0101","RFC-BPR-0100","RFC-BPR-0125"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6","AC7"],"owner_step":"attended look at the five states in a frozen tests-on build with the Fixture agent (see Owner judgement)"}
acceptance: []
---

# BPR-2402: New session after a refused question, the stopped card on the question field, prompts that wait with the revision card, and tour chats that start from a fresh answer

## Why

Five things the cards before this one left, each named in their "Not in this
card" (BPR-2389, BPR-2393, BPR-2394, BPR-2400), which the owner asked to have
finished (2026-11-25, relayed by the coordinator with the defaults decided).

### What was happening (main 60673a9fc)

Measured in the composed drawer course's renders of main
(`before/cocoa-macos/before-dark-200-*.png`) and its log.

1. **New session after a refused question resent under the old transcript.**
   The stopped card's New session set a flag for the launch path and resent
   the kept message in place: the thread kept the old transcript, title and
   History tick and gained one more turn, while the store started the new
   conversation unseen. The course's log on main: "new session from the
   refused card: 8 turns (were 7), session the old one"
   (`before-dark-200-agent-new-session-kept-sent-280.png` shows the old
   transcript under the new head). BPR-2389 fixed the same press on the card
   before a question and left this one ("already shows a turn").
2. **A stopped run's report headed the thread.** The thread keeps itself at
   its end (BPR-2296), where the stopped turn is; the card with Resend and
   Show the log was a screen or more above it
   (`before-dark-200-agent-stopped-280.png`, scrolled to the card by the
   course to draw it at all). BPR-2393 moved the revision card to the question
   field and left this one because "it holds a log as long as the agent
   wrote".
3. **Prompts stayed enabled while the revision card waited.** Before a
   question the chat's Send is disabled and so were the prompts, with no
   reason. After a refused question the prompts, and Send, were enabled: each
   press was sent and refused by the host the same way, adding another refused
   turn (`before-dark-200-agent-pinned-refused-280.png`). The tour's side chat
   kept its prompts enabled while its Send waited on "Head changed".
4. **The tour chat never told core its session was pinned.**
   `chat_history_menu::chat_state::Pinned` was set by the Agent tab only. A
   tour chat whose session read an earlier head and showed no turn was decided
   "already new": New session greyed under a notice that says "Choose New
   session in History".
5. **Ask more was the only way a fresh answer reached its chat.** Draft Tour's
   first draft and an Enhance run fresh (BPR-2138), outside any chat's
   conversation. Ask more carried their answer into the chat's next turn.
   BPR-2394 removed it from the stops Draft Tour proposes (owner: it "just
   opens the whole tour chat"), which left the host's branch for it unreached:
   opened by its own toggle, the whole tour's chat started without the draft's
   answer. The chat's own suggestion card kept four worded buttons (Accept,
   Edit text, Discard, and Ask more on an Enhance's suggestion), beside the
   tour's pencil and trash for the same two choices
   (`before-dark-200-tour-stop-chat-suggestion-300.png`).

## Change

1. **New session after a refused question is History's New session, then the
   message** (`agent_tab_view::choose_pinned`, `send_kept_message`,
   `abandon_kept_message`; host `choose_pinned_conversation`,
   `present_agent_native_history`). The tab holds the refused turn's message,
   the host opens an empty session at the head under review as History's item
   does, the card goes at the press, and once History shows the new session
   the host tells the tab, after the presentation has returned, to send the
   kept message there as its first turn. The session left behind stays in
   History with its transcript. If the session cannot be opened (the store
   fails, or the open could not start) the card comes back with the message
   kept. A message that cannot leave stays in the question field, which says
   why. Continue at *rev* is unchanged: it resends at once under the
   transcript it belongs to. `NativeFreshAtCurrentRevision` is no longer set
   by this press.
2. **A stopped run reports on the question field** (`present_banner`,
   `dock_banner`). The card ("*agent* stopped before answering", Resend, Show
   the log, its close) stands on the field like the revision card. The stopped
   turn directly above says why, so the card says "Your message is kept." and
   says the reason only when no turn does. Show the log puts what the run
   wrote on the card under the two buttons, which do not move: a read-only,
   selectable field as tall as its lines up to nine
   (`agent_tab_view::stopped_log_lines`), scrolled inside the card past that,
   wrapped at the card's width so a line with no space in it wraps where it
   must and the card never pans. The design call: the log is bounded on the
   card rather than sending the card back to the thread's head when it is
   shown, so Resend, Hide the log and the question field stay in view.
3. **While the revision card waits, nothing else is sent**, before a question
   and after a refused one alike. Core decides
   (`core::decide_agent_suggestion_use`, new: the revision choice first, then
   whether a question could be sent). Each suggested prompt is greyed and says
   what to choose first as its tooltip and its accessible description, where
   it said what a press would send (`agent_suggestion_list::set_usable` takes
   the reason): "Choose Continue at *rev* or New session first.", "Choose New
   session first.", "Choose how to go on, on the card over the question field,
   first." A question typed meanwhile is refused in place by the existing
   `decide_agent_send` (`ContinuationRequired` now holds after a refused
   question too): "Not sent. The pull request has moved on. Choose Continue or
   New session, then send.", the words in the field. The card's own choices
   send. The tour's side chat does the same while it asks which revision to go
   on with ("Choose New session in History or Continue old revision first.").
   The decision: disable with the reason, not a second way to choose from the
   prompts; the card with the two choices is in view on the field.
4. **The tour chat says its session is pinned** (`tour_side_chat`:
   `chat_state::Pinned` from the revision offer, whether or not Continue old
   revision was chosen). Core's `decide_new_session` is unchanged.
5. **A fresh answer goes with the next question of its chat, without a
   button** (`core::fresh_answer_to_carry`, new; host `tour_fresh_answer`,
   `tour_fresh_seed`, `present_tour_carried_answer`). The newest answered
   fresh run of a thread (Draft Tour's first draft for the whole tour's chat;
   an Enhance for its part's chat, while a part it proposed still waits) is
   carried until a turn of the chat's conversation after it was answered or
   runs. A turn that failed or was stopped leaves it for the next. The host
   applies it to every such turn, so the tour's own chat toggle, History's
   sessions and feedback queued behind the draft all start from the answer;
   `TourAskMore`, `ask_more_from` and the Ask more dispatch are gone. Draft
   Tour's answer is kept for the draft this window ran: a draft read back
   from History carries nothing. The chat says so in one quiet line on its
   question field until that question runs (`tourCarriedAnswer`): "Your next
   question starts from the draft's answer." / "… from the Enhance
   suggestion."
6. **The chat's suggestion card has the tour's row** (`tour_side_chat.cpp`):
   a pencil (Edit text, Show formatted while down) and a trash (Discard), the
   comments' glyphs as `quiet_tool_button`s named first in their tooltips and
   as their accessible names, and Accept in words at the row's right end
   (`wrapping_row::set_last_trails`). No Ask more: on an Enhance's suggestion
   it opened the chat the card is in, and what it also did is Change 5.
7. **The composed drawer course** gains seven states (`agent-stopped`,
   `agent-stopped-log`, `agent-pinned-refused-typed`,
   `agent-new-session-kept-sent`, `agent-new-session-answered`,
   `tour-stop-chat-enhanced`, `tour-whole-chat-new`) and holds the states to
   what their cards say (see Evidence). The Fixture agent's tour reply says
   when its request started from a fresh answer, so the course reads that the
   answer was really sent. `qt_drawer_surfaces_fit` judges the stopped card
   and its log at every width in every theme.
8. **Docs**: `docs/ux/workbench-specification.md` (the cards on the field, New
   session after a refusal, the waiting prompts, the log, History in the tour
   chat, the course), `docs/ux/ai-experience.md`,
   `docs/architecture/review-tours.md`, `src/adapters/qt/AGENTS.md`.
9. **Sealed manifest**: `window_agent_chat.cpp`, `window_agent_setup.cpp`,
   `window_composition.hpp`, `window_construction.cpp` and `window_tours.cpp`
   changed. The pin is not resealed in this lane (the coordinator reseals at
   the merge); see Evidence for what the reseal note has to say.

Not in this card:

- A tour chat that shows "Head changed" with no turn could not be reached in
  the composed course: a tour session is known to the window only once a turn
  has run in it, and a stopped turn is saved with its session and read back.
  Change 4 is covered by the core and Qt tests.
- The tour side chat's "Head changed" notice itself (RFC-BPR-0130 stage 6
  replaces it with the revision card).
- The Agent tab's Ask more on a returning answer (RFC-BPR-0116) is unchanged.
- A stopped card left showing after History opens another session (it reports
  the last run, as before).

## Acceptance

- AC1. On the card of a refused question, New session shows an empty session
  at the head under review with History's tick, sends the kept message there
  as its first turn, and leaves the refused session in History with its
  transcript; nothing is sent under the old transcript. A new session that
  cannot be opened brings the card back with the message kept.
- AC2. The report of a run that stopped stands on the question field, in view
  wherever the thread is scrolled, with Resend and Show the log; its log is at
  most nine lines tall, scrolls inside the card, and never pans the drawer.
- AC3. While the revision card waits (before a question and after a refused
  one), every suggested prompt is disabled and says what to choose first, and
  a typed question is refused in place and stays in the field; core decides.
  The tour's side chat does the same.
- AC4. A tour chat whose session read an earlier head offers New session in
  History even when nothing was asked in it.
- AC5. The first question asked in the whole tour's chat after Draft Tour, and
  in a part's chat while its Enhance suggestion waits, carries that answer to
  the agent, however the chat was opened, and the chat says so until then;
  core decides. No Ask more remains in the tour.
- AC6. The tour chat's suggestion card shows a pencil, a trash and Accept on
  one row at every column width, the icons named in tooltips and to a screen
  reader.
- AC7. The composed drawer course reaches each of those states and finds no
  fault at 280, 300, 340 and 420 px and in the narrowest window, at 100% and
  200% and under Cocoa.

## Evidence

Revision `88fac1032` (rebased onto main d5ac0358d, which had gained BPR-2399's
finding-card rows and the course's two `agent-finding-card*` states), macOS
26.6.2 arm64, Debug, Qt 6.12.1, `ctest --preset dev -R '^<row>$'`, rows one at
a time. Twenty-three of the twenty-four rows below passed in the tree of the
commit before the last one, 39488f53a, and ten were run again at `88fac1032`,
which changed two comments, the core test's comparisons and the pinned e2e
course's New session press. That course's row counts only at `88fac1032`: its
source was edited while the earlier tree built.

Passed at `88fac1032`: `core_tour_agent_session`, `qt_tour_side_chat`,
`qt_workbench_window_scale_100`, `qt_drawer_surfaces_fit_scale_100`,
`application_agent_invocation_composition_contract`,
`application_agent_invocation_native_pinned_contract`,
`application_tour_feedback_queue_public_path`,
`application_composed_drawer_fit_scale_100` (95 s), `_scale_200` (91 s) and
`_cocoa` (92 s; the rows' limit is 420 s), each "38 states, 0 faults".
Passed one commit earlier and not run again (their sources did not change):
`core_chat_history`, `core_agent_turn`, `qt_workbench_window_scale_125` and
`_200`, `qt_drawer_surfaces_fit_scale_125` and `_200`,
`qt_tour_agent_session_view`, `qt_chat_history_menu`,
`qt_tour_side_chat_width_scale_100`, `_125` and `_200`, `qt_tour_chat_in_tab`,
`architecture_boundaries`. The format check passed on every changed file
(clang-format 22). clang-tidy passed on `agent_turn.cpp`,
`tour_agent_session.cpp`, `agent_suggestion_list.cpp`, `tour_side_chat.cpp`,
`agent_tab_view.cpp`, `window_tours.cpp`, `window_agent_chat.cpp`,
`window_agent_setup.cpp`, the three core tests, `tour_side_chat_tests.cpp`,
`drawer_surfaces_fit_tests.cpp`, the composed course and
`fixture_agent_main.cpp`; it was not run on `workbench_window_tests.cpp`,
`tour_agent_session_view_tests.cpp` or
`agent_invocation_composition_public_path.cpp`.

Failed, as expected until the merge reseals it:
`application_review_round_trip_composition_contract`, on the production
application source digest alone ("production application source content
digest drifted"). The reseal note has to say: the stopped card's New session
opens History's new session and the kept message is sent once it is shown
(`window_agent_chat.cpp` `choose_pinned_conversation`,
`window_agent_setup.cpp` `present_agent_native_history`, one flag in
`window_composition.hpp`); a tour chat's next question starts from a fresh
run's answer as core decides, in place of Ask more's dispatch
(`window_tours.cpp` `tour_fresh_answer`, `tour_fresh_seed`,
`present_tour_carried_answer`; `window_construction.cpp` loses the Ask more
wiring and tells the chat as a part's chat opens). Local agent turns and
presentation only; no grant, provider, transport, mutation or publication
path changes.

The documentation gate (`scripts/verify_documentation.py --root .`, run with
the main checkout's deployed implement-rfc scripts on `PYTHONPATH`, since a
worktree has none) reported one error before the rebase, in a file this card
does not touch: "docs/issues/NEXT_ID: next_id: BPR-2401 rewinds historical
high water BPR-2402". It was not run again after the rebase.

- AC1: `qt_workbench_window_scale_*`
  (`test_a_pinned_conversation_card_offers_what_it_names`): New session goes
  to the host and sends nothing, the card goes at the press and the tab holds
  the message; `abandon_kept_message` brings the card back with New session
  and nothing sent; chosen again, an empty thread shows with no card and
  nothing sent; `send_kept_message` sends "Review this pull request" once, and
  a second call sends nothing. `application_agent_invocation_native_pinned_contract`
  (the real host and store): after a question refused for uncommitted edits,
  New session leaves a session with one turn, the refused question, under a
  new History selection with the card gone, answered at the current head with
  one agent start, and the next question follows there. The composed course:
  the refused session had more than one turn; after the press the tab works
  with exactly one turn, "What changed in this push?", History's tick on a
  session that is not the refused one, the new head in the header and no
  card (`agent-new-session-kept-sent`, the turn held open while the drawer is
  dragged); then the answer in that session (`agent-new-session-answered`);
  then the refused session opened again from History with its transcript.
- AC2: `qt_workbench_window_scale_*`: an ordinary failure's card is on the
  question field, outside the thread's scroll area, titled "… stopped before
  answering", saying "Your message is kept."; the log is folded; Show the log
  shows "fixture-agent: exit status 2" read-only on the card under Resend,
  which does not move; a short log is as tall as its lines with no range
  either way; a thirty-line log of lines with no space in them is exactly nine
  lines tall, scrolls vertically inside the card, has no sideways range and no
  horizontal bar, and Resend, Hide the log and the question field stay in
  view. `qt_drawer_surfaces_fit_scale_*`: the stopped card and the card with
  its log, at every drawer width in the four themes, pass the card's fit and
  placement judges (on the field, under the thread, whole in view with the
  thread at its start and its end) and the drawer's. The composed course:
  `agent-stopped` and `agent-stopped-log` after the Fixture agent's scripted
  crash, the card on the field, Resend still to press beside the log.
- AC3: `core_agent_turn` (`test_suggestion_use`): the revision choice blocks
  whether or not a question could leave and is the reason said first; no
  question to send blocks without it. `qt_workbench_window_scale_*`: after a
  refused question every prompt is visible, disabled, and says "Choose
  Continue at 3333333 or New session first." as tooltip and accessible
  description; a prompt asked for by its key sends nothing; a typed question
  is not sent, adds no turn, stays in the field under "Not sent. The pull
  request has moved on. Choose Continue or New session, then send."; with the
  ways to carry work offered the prompts say "Choose how to go on, on the card
  over the question field, first."; after New session and after an ordinary
  failure the prompts are enabled. `qt_tour_side_chat`: in a chat that asks
  which revision to go on with, Send is disabled, each prompt is disabled with
  "Choose New session in History or Continue old revision first.", a press
  sends nothing, and after Continue old revision they are enabled and say
  what they send. The composed course: the same words on the Agent tab's
  prompts in `agent-pinned-offered` and `agent-pinned-refused`, and the typed
  question held in `agent-pinned-refused-typed`.
- AC4: `core_chat_history`: a tour chat's facts with nothing asked and
  `Pinned` set are not "already new"; without it they are. `qt_tour_side_chat`:
  an empty chat on an untouched session has New session greyed with "This
  session is new: nothing has been asked in it."; told the session read an
  earlier head it is enabled with no reason, core's plan says none, and it
  asks the host; still enabled after Continue old revision; greyed again at
  the head under review. The composed course asserts only that History offers
  New session in the stop chat that read the earlier head, which has turns.
- AC5: `core_tour_agent_session` (`fresh_answers`): nothing run, a draft
  running, answered, not for a stop's chat, kept after a failed and after a
  stopped question, not taken by another part's question, taken by an
  answered one, a failed draft carrying nothing, a new draft carried again; an
  Enhance running, answered with nothing proposed, carried while its
  suggestion waits, not once discarded or accepted, the newer of two, taken by
  an answered question; equal inputs give equal answers. `qt_tour_side_chat`:
  the line is hidden and empty until the host says, shows each kind's words on
  the question field under the transcript, goes for another part's chat and
  when told none. The composed course, with the Fixture agent, whose tour
  reply says when its request started from an earlier answer: opened by the
  tour's own chat toggle after Draft Tour, the whole tour's chat says "Your
  next question starts from the draft's answer." (`tour-whole-chat-new`), its
  first answer says "I started from the earlier answer", and the line is
  gone; after a stop's Enhance its chat says "… from the Enhance suggestion."
  (`tour-stop-chat-enhanced`), the next answer says the same, and the line is
  gone. `application_tour_feedback_queue_public_path`: feedback queued behind
  the draft still starts from the draft's answer, now by the same rule.
- AC6: `qt_tour_side_chat`: no button says Ask more, Edit text or Discard in
  words; Accept is a worded Tour action; the pencil and trash are
  `quiet_tool_button`s with icons and no text, named Edit text and Discard in
  their accessible names and first in their tooltips; left to right pencil,
  trash, Accept on one row; the pencil is down and named Show formatted while
  the source shows. The composed course asserts the same on the real
  suggestion (`tour-stop-chat-suggestion`) and sweeps it and its editing
  state at every column width.
- AC7: the three composed rows, 38 states each, no fault.

Renders, from the composed course under the native look (Cocoa, kept off
screen, the display's own 200%), each state at 280, 300, 340 and 420 px, the
narrowest window, and the whole window at the narrowest width:

- before (main 60673a9fc with this card's course states, Dark):
  `/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/tour-stopped-followups-renders/before/cocoa-macos/before-dark-200-<state>-<width>.png`
- after (`88fac1032`, Dark):
  `/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/tour-stopped-followups-renders/after/cocoa-macos/after-dark-200-<state>-<width>.png`
- after (`88fac1032`, Light):
  `/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/tour-stopped-followups-renders/after-light/cocoa-macos/after-light-light-200-<state>-<width>.png`

The states to compare: `agent-stopped`, `agent-stopped-log`,
`agent-pinned-refused`, `agent-new-session-kept-sent`,
`agent-new-session-answered`, `tour-stop-chat-suggestion`,
`tour-stop-chat-enhanced`, `tour-whole-chat-new`, `tour-stop-chat-head-moved`;
`agent-pinned-refused-typed` has no before.

## Owner judgement

Not asked yet. What an attended look in a frozen tests-on build with the
Fixture agent would settle, one question each:

1. The stopped card on the question field with its log bounded at nine lines
   on the card (`agent-stopped`, `agent-stopped-log`). The stopped turn above
   it already shows the same log in its own code block and has its own
   Resend, so the card repeats both; whether the card should keep Show the
   log there is the owner's call.
2. New session after a refused question (`agent-new-session-kept-sent`,
   `agent-new-session-answered`).
3. The greyed prompts and the held question while the revision card waits
   (`agent-pinned-refused`, `agent-pinned-refused-typed`).
4. The line "Your next question starts from the draft's answer." and its
   Enhance form, and that the answer is carried without a button
   (`tour-whole-chat-new`, `tour-stop-chat-enhanced`). An Enhance's answer was
   carried only on Ask more before; it is now carried for the first question
   asked while its suggestion waits.
5. The suggestion card's pencil, trash and Accept (`tour-stop-chat-suggestion`).
