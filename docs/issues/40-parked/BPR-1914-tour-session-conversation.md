---
schema: "implement-rfc.card.v1"
id: "BPR-1914"
title: "The tour session is a conversation: ask about the tour, get answers, and have it propose edits"
status: "Parked"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-1900","BPR-1913"]
parked_on: "human-review"
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-06, coordinator chat after using BPR-1900","related_cards":["BPR-1900","BPR-1912","BPR-1913","BPR-1915","BPR-1916"]}
acceptance: [{"criterion": "AC1 Selecting a passage in Solution and asking \"what does this mean?\" gets an answer in the conversation that cites the code it read", "outcome": "pass", "revision": "da676456a", "evidence": {"kind": "command", "detail": "qt_tour_agent_session_view: a Solution selection offers Ask about this, which opens the conversation with the passage quoted; Return sends passage and question; the answer citing src/engine/renderer.cpp:3 is in the conversation, its list and inline code rendered. Offscreen look of the frozen build: ATTENDED_TOUR_ANSWER cites renderer.cpp:3, 4-7 and 8 after Read and rg steps.", "path": "docs/issues/40-parked/BPR-1914-tour-session-conversation.md#verification"}}, {"criterion": "AC2 A follow-up question keeps context from the earlier turn (stub checks the resumed context)", "outcome": "pass", "revision": "da676456a", "evidence": {"kind": "command", "detail": "core_tour_agent_session (tour_conversation_of replays answered turns, bounded), application_tour_agent_conversation (conversation_so_far carries the earlier reply). The stub answers a follow-up that refers to \"that\" only when conversation_so_far holds its first reply's words; the look's follow-up got the rewrite, not \"I can't see what we said before\".", "path": "docs/issues/40-parked/BPR-1914-tour-session-conversation.md#verification"}}, {"criterion": "AC3 \"Rewrite that so it says X\" returns a proposal for that field, and accepting it updates the field; nothing changes without acceptance", "outcome": "pass", "revision": "da676456a", "evidence": {"kind": "command", "detail": "core_tour_agent_session (propose_tour_changes, stop_text, merge_tour_proposal); offscreen look: the follow-up proposes a Solution, the Tour tab shows it pending, Accept makes the draft's Solution the rewrite (ATTENDED_TOUR_SOLUTION).", "path": "docs/issues/40-parked/BPR-1914-tour-session-conversation.md#verification"}}, {"criterion": "AC4 The conversation is copyable as a whole (BPR-1913's rules)", "outcome": "pass", "revision": "da676456a", "evidence": {"kind": "command", "detail": "qt_tour_agent_session_view reads the clipboard after Copy conversation: every turn, the quoted passage, steps and replies as written, the proposal line; a selection spans turns.", "path": "docs/issues/40-parked/BPR-1914-tour-session-conversation.md#verification"}}, {"criterion": "AC5 Owner look: attended Accept/Reject on a frozen build that shows ask, answer, follow-up and an accepted rewrite", "outcome": "pending", "revision": "da676456a", "evidence": {"kind": "observation", "detail": "Frozen build /Users/jaeger/bpr1914-tour-conversation-v001.6cph2wud of da676456a; new short session ac06de1b29f46bbf0be650bd3943fbf6, candidate BPR-1914-tour-conversation, one Accept/Reject question, ready and awaiting the owner.", "path": "docs/issues/40-parked/BPR-1914-tour-session-conversation.md#attended-review-ac5"}}, {"criterion": "AC6 The RFC amendment supersedes T1 and records the resume mechanism, and the documentation gate passes", "outcome": "pass", "revision": "da676456a", "evidence": {"kind": "command", "detail": "RFC-BPR-0092 Amendment BPR-1914, BPR-1915 supersedes T1 (one conversation per tour, replayed for every agent kind; what is stored where); review-tours.md and ai-agent-boundary.md updated; verify_documentation.py prints DOCUMENTATION_OK.", "path": "docs/issues/40-parked/BPR-1914-tour-session-conversation.md#implementation-2026-11-06"}}]
---

# BPR-1914: The tour session is a conversation: ask about the tour, get answers, and have it propose edits

## Owner (2026-11-06)

"having used it for a while I want to do more than just enhance the tour text with my agent. I want to ask questions, get back and forth and have it update it. For example right now I have Solution section where the draft said something I don't understand, but I don't really have a way to ask for clarification. It also feels like the output of the draft is entirely devoid of any formatting. It feels just like big walls of text. I have a skill here for /technical-book-humanizer but I also have a skill elsewhere for technical writing that is slightly more tuned to those purposes. I wonder if I can't have a hook to let me reference a custom skill in my prompts if the skill is available in as a personal user skill or a skill in the repo being reviewed. I also wonder if we can improve the settings agent tab where we're editing the built-in prompts its a bit weak maybe some /frontend-design:frontend-design could improve that."

## Change

Load `frontend-design:frontend-design` first.

1. **Ask the tour agent.** The Tour tab gets a way to talk to the tour session about the tour. You can ask about a selected passage of Problem, Solution or a stop ("What does this mean?"), or ask a free question about the tour or the change. The agent answers in the same read-only workspace session (BPR-1900 tools, BPR-1912 rules). The answer reads as a reply, not as an edit.
2. **Back and forth.** Follow-ups keep the conversation. This supersedes RFC-BPR-0092 Amendment BPR-1900's T1 (fresh run per request): the tour session keeps one conversation per tour, with the tour so far given fresh on each turn. Record how the session resumes for each agent kind (Claude `--resume` with saved session state, Codex equivalents, or a replayed transcript) and what is stored where.
3. **Have it update the tour.** From the conversation you can ask it to change the tour ("rewrite that sentence so it says X", "add a stop on the retry loop"). Changes come back as BPR-1900 proposals: per field and per stop, accept/edit/discard, with the hunk placement and drop rules. They never silently overwrite.
4. **Where it lives.** It's next to the tour, not in the editing column's way, and coherent with BPR-1913's transcript window: either the transcript window becomes the conversation, or a compact ask box sits next to the selected field. Choose with the skill, and keep the transcript copyable.
5. Asking about a selected passage is a first-class action: select text, then "Ask about this".

## Acceptance

- AC1. Selecting a passage in Solution and asking "what does this mean?" gets an answer in the conversation that cites the code it read.
- AC2. A follow-up question keeps context from the earlier turn (test with a stub that checks the resumed context).
- AC3. "Rewrite that so it says X" returns a proposal for that field, and accepting it updates the field. Nothing changes without acceptance.
- AC4. The conversation is copyable as a whole (BPR-1913's rules).
- AC5. Owner look: attended Accept/Reject on a frozen build that shows ask, answer, follow-up and an accepted rewrite.
- AC6. The RFC amendment supersedes T1 and records the resume mechanism, and the documentation gate passes.

## Implementation (2026-11-06)

`frontend-design:frontend-design` was loaded before the UI work.

- **Where it lives (item 4): the transcript window is the conversation.** It
  is renamed **Tour conversation** and opened by **Conversation (n)**, always
  offered. Turns are listed on the left, newest first. On the right the whole
  conversation is one selectable document, oldest first: the passage quoted,
  the question, the steps in a quiet small table, then the reply under **Tour
  agent**, rendered as Markdown, and a line saying what it proposed. The ask
  box sits under it (Return asks, Shift+Return starts a new line), with the
  passage on one quiet line above it that can be dropped. **Ask** wears the
  agent accent and becomes **Stop** while a turn runs. **Copy turn** and
  **Copy conversation** copy plain Markdown text. A compact box beside the
  field was not chosen: it would take back the editing column's room
  (BPR-1913), and a back-and-forth needs the earlier turns in view.
- **Ask about this (item 5).** It appears beside each field's Enhance control
  (Problem, Solution, a stop's explanation) while text is selected, and opens
  the conversation with that passage quoted and the keyboard in the box.
- **Core** (`tour_agent_session.hpp`): the `ask` request (read-only),
  `tour_passage`, `begin_tour_ask` / `answer_tour_ask` (reply up to 16 KiB),
  `tour_conversation_of` (answered ask and Draft the tour turns, newest 12
  within 48 KiB, counting those left out), `propose_tour_changes` (adds
  `stop_text` items naming a stop of the tour), `tally_of` and
  `merge_tour_proposal` (a new Problem, Solution or stop text supersedes the
  pending one; decided parts go).
- **Application** (`tour_agent_conversation.hpp`): `build_tour_ask_prompt`
  sends the tour as it is now (stop numbers and explanations), the earlier
  turns, the question and the passage as untrusted evidence. It asks for
  `path:line` citations and for changes only when asked.
  `decode_tour_ask_answer` reads `bpr.tour-ask.v1` with exactly its keys.
- **Composition** (`github_application/window_tours.cpp`, beside the BPR-1900 tour
  session code, where it fits): `request_tour_ask`, `finish_tour_ask`,
  `adopt_tour_conversation` (one conversation per tour draft), the shared
  `tour_anchor_files`, and Accept of a stop text as `edit_stop`.
- **Resume mechanism (item 2)**: a replayed transcript for every agent kind,
  recorded in RFC-BPR-0092's amendment. Read-only Claude runs keep
  `--no-session-persistence` in a new workspace each run; Codex `exec` is
  one-shot. The turns live in the app's memory for the review only.
- **Look**: `--attended-tour-drafting --attended-tour-conversation` drafts with
  the stub, accepts the Problem and Solution, selects "between begin and
  present", asks, follows up with a rewrite, accepts it, and leaves the
  conversation open. The stub (`scoped_writing_agent.cpp`) reads and searches
  the workspace for an ask turn. It answers a follow-up only when the earlier
  reply came back in `conversation_so_far`.

## Verification

Focused, owner: "dont wait for tests".

- `core_tour_agent_session`, `application_tour_agent_conversation` (new),
  `application_agent_launch`, `application_writing_delivery`,
  `qt_tour_agent_session_view` (ask flow, Return, Stop, rendered reply, two
  turns, Copy conversation, selection across turns),
  `qt_review_tour_agent_failure`, `qt_review_tour_agent_help`,
  `qt_review_tour_agent_setup_call`, `application_review_tour_writing` (+
  wrapper), `application_review_tour_composition_public_path`,
  `application_agent_chat_composition_contract`,
  `application_review_round_trip_composition_contract` (resealed),
  `architecture_boundaries`, `architecture_boundary_contract`: pass.
- Offscreen look on the dev build and on the frozen executable:
  `ATTENDED_TOUR_ANSWER` (cites `src/engine/renderer.cpp:3`, `4-7`, `8`),
  `ATTENDED_TOUR_FOLLOW_UP` (the rewrite proposal),
  `ATTENDED_TOUR_SOLUTION` (the accepted rewrite), `ATTENDED_AGENT_TAB_TURNS:0`,
  `ATTENDED_TOUR_CONVERSATION_SHOWN`.
- `coding-format-check` passes; `verify_documentation.py` prints
  `DOCUMENTATION_OK`.

## Attended review (AC5)

Frozen build `/Users/jaeger/bpr1914-tour-conversation-v001.6cph2wud` of `da676456a` (executable SHA-256 `0f475b2065f4ce0b31ae142b09a839685668bb02acbc51e39ee812ae92535051`), fixture
`conventional-comments-v1` on `src/engine/renderer.cpp`, stub agent
`attended-drafting-claude`, ready line `ATTENDED_TOUR_CONVERSATION_SHOWN`; the
launcher's own offscreen check reached it. Launch: `python3 /Users/jaeger/bpr1914-tour-conversation-v001.6cph2wud/launch.py`. Pictures from
that executable, offscreen, light and dark. New short session `ac06de1b29f46bbf0be650bd3943fbf6`,
state `/Users/jaeger/.codex/attended-review/bpr1914-v001-HmfYOg/state`, host started with `serve --tailscale`. Status: ready,
awaiting the owner. Candidate `BPR-1914-tour-conversation` (revision `da676456a`), one
Accept/Reject question. BPR-1915 is a second candidate in the same session.

The frozen commit `da676456a` stays reachable on the local branch
`bpr1914-v001-frozen-history`. The card's branch was then rebased onto `main`
past BPR-1908, which moved the tour session code into
`src/app/src/github_application/window_tours.cpp`, so the ask code now lives
there. On the rebased commit the 16 focused tests, the format check, the
documentation gate and the offscreen look (`ATTENDED_TOUR_CONVERSATION_SHOWN`)
pass again, with the composition pin resealed.
