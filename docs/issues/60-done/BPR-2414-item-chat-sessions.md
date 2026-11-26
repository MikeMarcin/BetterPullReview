---
schema: "implement-rfc.card.v1"
id: "BPR-2414"
title: "The side chat about one review item keeps sessions: History with New session first and a row each, its rounds continue the chosen session, and a session read at an earlier head asks on the pinned-revision card"
status: "Done"
rfcs: ["RFC-BPR-0130"]
depends_on: ["BPR-2412", "BPR-2413"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","reservation":"docs/issues/_reservations/BPR-2414--BPR-2414--796f1bfba5f5.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"<revision>","origin":"RFC-BPR-0130 stage 7 (draft card DRAFT-0013 of the attended review 3cd822672efb50572b10ab10747889ba); owner, 2026-11-25, open question 4: \"sure seems like they should right\"","related_cards":["BPR-2286","BPR-2322","BPR-2370","BPR-2377","BPR-2412","BPR-2413"],"related_rfcs":["RFC-BPR-0130","RFC-BPR-0096"],"acceptance":["AC1","AC2","AC3","AC4","AC5"]}
acceptance: []
---

# BPR-2414: An item's side chat keeps sessions

## Why

RFC-BPR-0130 stage 7: "An item's chat runs on the session runner;
`side_chat_panel`'s hidden controls are deleted. Visible: History, sessions
and Stop in the item's chat (open question 4, answered 2026-11-25)." The
owner, asked whether the side chat about one item should gain sessions,
cost and Stop: "sure seems like they should right".

### What was there (BPR-2413)

- The item chat (`qt::item_side_chat`) kept one conversation an item. Each
  round was a fresh run (`core::agent_run_path::follow_up`, session mode
  fresh) whose message carried the item and every earlier round.
- Its header was Show, Copy conversation, Close: core gave it Copy because
  it kept no sessions. No History, no cost, no revision read.
- Stop already stood in Send's place while a round ran (stage 2).

## Change

1. **Core.** `chat_keeps_sessions(review_item)`; the item chat's header
   offers Show, History (New session, Delete all sessions, Copy
   conversation), Close; `chat_history_kind::review_item` plans the same
   History as a tour part's, without other parts' chats.
   `follow_up_state::InSession` and `restore_follow_up`: a chosen session's
   rounds become the chat's, and every later message carries the item and
   the question, never the earlier rounds (the agent's session holds them).
   `agent_run_path::follow_up` resumes, as a tour part's chat does.
2. **Application.** `follow_up_host::restore`; `follow_up_agent::Session`
   and `Asked` (the session a round continues and what the store records as
   asked: the question, or the apply action's words). The native store reads
   an item's scope (`item_chat_native_scope`, `item:` and the item's key) as
   read-only, as it reads a tour's (`read_only_native_scope`).
3. **Qt.** The item chat builds History from the one `chat_history_menu`
   (New session first, a row each with pencil, trash and the i whose tooltip
   gives turns and cost, Delete all sessions…, Copy conversation, the
   revision read), the session's notice line (carried over, cannot continue)
   and the pinned-revision card, all from core's planners. Send and the apply
   action wait while the sessions are read or the card asks.
4. **Window.** Each item's sessions live under its own scope on the item
   chat's own invocation host: opened, the item reads them and a chat that
   has shown nothing shows its open session's rounds; History's choices,
   rename and delete go through the host calls the other chats use; a round
   continues the chosen session (Continue at the revision read when chosen on
   the card); another agent chosen shows that agent's sessions.
5. **Courses.** `core_chat_header`, `core_chat_history`,
   `core_agent_follow_up`, `application_follow_up_host`,
   `application_agent_launch_policy`, `qt_item_side_chat` (History, its
   intents, the carried-over notice, the card), the composed course and
   `application_follow_up_public_path` (the second round continues the
   session; New session keeps the first in History; opening it shows its
   rounds again), `application_agent_keep_action_public_path` (waits for the
   sessions before Update).
6. **Docs.** `src/adapters/qt/AGENTS.md`.

## Acceptance

- AC1 (RFC AC01, AC02). The item chat's header is the one row with History;
  its History opens with New session, a row each with rename, delete and an
  i giving turns and cost, Delete all sessions…, Copy conversation, the
  revision read.
- AC2. A round continues the open session: its message carries no earlier
  round, and History keeps it as that session's turn.
- AC3. New session keeps the open session in History and empties the chat;
  opening a session from History shows its rounds.
- AC4 (RFC AC08). A session carried over says so on the notice line, and
  one read at an earlier head asks on the pinned-revision card, in the words
  every chat says them with.
- AC5. Format and tidy on touched units.

## Evidence

Revision `5fc465fcd` (on main `f8ab64606`), macOS arm64 Debug,
`ctest --test-dir build/dev -R '^<row>$'`.

- AC1: `core_chat_header`, `core_chat_history`, `qt_chat_history_menu`,
  `qt_item_side_chat` (History is the one menu; New session, the sessions,
  Delete all, Copy conversation, "Reads the pull request at 3f9c2ab"; the
  row is Show, History, close) passed; the composed course checks the item
  chat's header has History and no Copy of its own.
- AC2: `core_agent_follow_up` (an in-session message carries no earlier
  round), `application_follow_up_host` (a round names its session and what
  it asked), `application_follow_up_public_path` (the second round answers
  "0 earlier rounds", History keeps one session with two turns) and the
  composed course (the round is its session's turn in History) passed.
- AC3: `application_follow_up_public_path` (New session empties the chat
  and keeps the first in History; opening it shows its three rounds again)
  passed.
- AC4: `qt_item_side_chat` (carried-over notice naming the agent; the card
  equal to `core::plan_moved_head_notice`, Send and the apply action held
  until Continue) passed.
- Also passed: `application_agent_launch_policy` (follow_up now resumes),
  `application_agent_keep_action_public_path`,
  `application_report_draft_reveal_public_path`, `qt_tour_side_chat`,
  `qt_side_chat_tab_and_send_scale_100`, `_200`,
  `application_composed_drawer_fit_scale_100`, `_200`
  ("39 states, 0 faults, 50 transcript edges drawn", offscreen) and `_cocoa`
  ("39 states, 0 faults, 50 transcript edges drawn, dpr 2, cocoa/macos").
- AC5: `_coding-standard/scripts/format.py` (clang-format 22) over every
  touched C++ file; `tidy.py --warnings-as-errors` over `agent_follow_up.cpp`,
  `chat_header.cpp`, `chat_history.cpp`, `follow_up_host.cpp`,
  `agent_invocation_host.cpp`, `item_side_chat.cpp`, `chat_history_menu.cpp`,
  `window_follow_up.cpp`: clean.

Renders, native look, light, 200%, under
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/rfc130-renders/`:
before `stage-6-after/cocoa-macos/`, after `stage-7-after/cocoa-macos/`
(274 each): 217 byte-identical. The intended change is
`item-chat-answered-*`: a 34 by 33 box at the header's right, History's
clock where Copy stood (and, in the `-window` renders, the code pane's
overlay scroll bar). The rest differ where a run's timing shows, as
BPR-2412 and BPR-2413 recorded: the question field's caret (2 by 31), the
window footer's run clock (73 by 18), the drafting line, and
`agent-pinned-offered-280`/`-300`.

## Owner judgement

- The item chat's History is the intended change of look (RFC stage 7);
  part of AC09's final attended look.

## Not in this card

- Queueing a question typed while a round runs (the tour chat's pending
  list) and deleting `side_chat_panel`'s hidden controls: see the RFC's
  ledger.
