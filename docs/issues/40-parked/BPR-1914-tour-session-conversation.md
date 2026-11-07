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
acceptance: [{"criterion": "AC1 Selecting a passage in Solution and asking \"what does this mean?\" gets an answer in the conversation that cites the code it read", "outcome": "pass", "revision": "067c921f2", "evidence": {"kind": "command", "detail": "qt_tour_agent_session_view: a Solution selection offers Ask about this, which opens the conversation with the passage quoted; Return sends passage and question; the answer citing src/engine/renderer.cpp:3 is in the conversation, its list and inline code rendered. Offscreen look of the frozen build: ATTENDED_TOUR_ANSWER cites renderer.cpp:3, 4-7 and 8 after Read and rg steps. v002: qt_tour_thread_view repeats it in the Solution's thread under the field (ask, answer, follow-up, Accept in the thread), and the frozen look runs it in context. v003: qt_tour_side_chat repeats it in the Solution's side chat (a selected passage, the reply, a follow-up, Update the Solution from this chat with Accept, Copy chat).", "path": "docs/issues/40-parked/BPR-1914-tour-session-conversation.md#verification"}}, {"criterion": "AC2 A follow-up question keeps context from the earlier turn (stub checks the resumed context)", "outcome": "pass", "revision": "067c921f2", "evidence": {"kind": "command", "detail": "core_tour_agent_session (tour_conversation_of replays answered turns, bounded), application_tour_agent_conversation (conversation_so_far carries the earlier reply). The stub answers a follow-up that refers to \"that\" only when conversation_so_far holds its first reply's words; the look's follow-up got the rewrite, not \"I can't see what we said before\". v002: qt_tour_thread_view repeats it in the Solution's thread under the field (ask, answer, follow-up, Accept in the thread), and the frozen look runs it in context. v003: qt_tour_side_chat repeats it in the Solution's side chat (a selected passage, the reply, a follow-up, Update the Solution from this chat with Accept, Copy chat).", "path": "docs/issues/40-parked/BPR-1914-tour-session-conversation.md#verification"}}, {"criterion": "AC3 \"Rewrite that so it says X\" returns a proposal for that field, and accepting it updates the field; nothing changes without acceptance", "outcome": "pass", "revision": "067c921f2", "evidence": {"kind": "command", "detail": "core_tour_agent_session (propose_tour_changes, stop_text, merge_tour_proposal); offscreen look: the follow-up proposes a Solution, the Tour tab shows it pending, Accept makes the draft's Solution the rewrite (ATTENDED_TOUR_SOLUTION). v002: qt_tour_thread_view repeats it in the Solution's thread under the field (ask, answer, follow-up, Accept in the thread), and the frozen look runs it in context. v003: qt_tour_side_chat repeats it in the Solution's side chat (a selected passage, the reply, a follow-up, Update the Solution from this chat with Accept, Copy chat).", "path": "docs/issues/40-parked/BPR-1914-tour-session-conversation.md#verification"}}, {"criterion": "AC4 The conversation is copyable as a whole (BPR-1913's rules)", "outcome": "pass", "revision": "067c921f2", "evidence": {"kind": "command", "detail": "qt_tour_agent_session_view reads the clipboard after Copy conversation: every turn, the quoted passage, steps and replies as written, the proposal line; a selection spans turns. v002: qt_tour_thread_view repeats it in the Solution's thread under the field (ask, answer, follow-up, Accept in the thread), and the frozen look runs it in context. v003: qt_tour_side_chat repeats it in the Solution's side chat (a selected passage, the reply, a follow-up, Update the Solution from this chat with Accept, Copy chat).", "path": "docs/issues/40-parked/BPR-1914-tour-session-conversation.md#verification"}}, {"criterion": "AC5 Owner look: attended Accept/Reject on a frozen build that shows ask, answer, follow-up and an accepted rewrite", "outcome": "pending", "revision": "067c921f2", "evidence": {"kind": "observation", "detail": "v001, v002 and v003 rejected by the owner. v004: frozen build /Users/jaeger/bpr1914-side-chat-v004.gye36phm of 067c921f2; new short session ca5eae33ab574443875c249beee013db, candidate BPR-1914-side-chats-v004, one Accept/Reject question, ready and awaiting the owner.", "path": "docs/issues/40-parked/BPR-1914-tour-session-conversation.md#v004-2026-11-07"}}, {"criterion": "AC6 The RFC amendment supersedes T1 and records the resume mechanism, and the documentation gate passes", "outcome": "pass", "revision": "067c921f2", "evidence": {"kind": "command", "detail": "RFC-BPR-0092 Amendment BPR-1914, BPR-1915 supersedes T1 (one conversation per tour, replayed for every agent kind; what is stored where); review-tours.md and ai-agent-boundary.md updated; verify_documentation.py prints DOCUMENTATION_OK.", "path": "docs/issues/40-parked/BPR-1914-tour-session-conversation.md#implementation-2026-11-06"}}]
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

### v001 rejected (2026-11-06)

The owner, verbatim: "I don't understand this editing interface. Like I want
to ask questions about the specific parts of the tour, how do I do that if I
can't see the tour or the code?" Session `ac06de1b29f46bbf0be650bd3943fbf6`
recorded the Reject; its host is stopped and the v001 frozen root removed.
Coordinator direction for v002: ask where the part is shown (a passage in the
Problem or Solution, a stop's explanation, or the stop's code lines), the
answer attached there as an inline thread, follow-ups and rewrites in that
thread, the full conversation only a secondary view, and a stop's code visible
next to its thread.

## v002: asking in context (2026-11-06)

`frontend-design:frontend-design` was loaded first. The one memorable element
is the thread: a margin note in the agent accent (a 2 px rule and a faint
wash) that hangs under the part it is about, so the tour text sits above it
and, for a stop, the diff beside it shows the stop's lines. Everything else
stays quiet.

- **Core**: `tour_thread` (tour, Problem, Solution, stop) on every ask turn
  (`begin_tour_ask` takes it); a `stop_code` passage with path and lines;
  `thread_runs`; `tour_proposal_item::Run` and `proposed_in_thread`; the
  replay names each turn's thread.
- **Application**: the request names each turn's thread and this turn's, says
  a follow-up refers first to its own thread, and carries a code passage's
  path and lines.
- **Qt** (`tour_thread_view`): each turn shows the passage (code in the code
  face with its path and lines), the question, one quiet line of what the
  agent read, the reply rendered, and its proposed parts with Accept, Edit
  text and Discard. The ask box under them takes follow-ups (Return asks), and
  Ask becomes Stop while the thread's own turn runs. A new answer scrolls the
  drawer to it. The editor has one thread under the Problem, one under the
  Solution and one under the open stop; the panel has one for the tour as a
  whole, opened by **Ask about the tour** on its own row under Draft the tour.
  **Ask about the Problem/Solution/this stop** sits beside each Enhance
  control, reading **Ask about this** with text selected. Under a stop, **Ask
  about this stop** and **Ask about its code** are stacked on their own rows so
  the narrow route keeps their words. Asking in a stop's thread triggers Show
  in code. The proposal box under Draft the tour shows only a draft's parts.
  The conversation window keeps its turns list, document and Copy actions; its
  ask box is gone. Every thread's ask box sends through the editor's single
  `set_tour_ask_dispatch` entry point (thread, passage, question), the one
  entry point for BPR-1916's skill wiring.
- **Composition** (`window_tours.cpp`): `request_tour_ask` takes the thread;
  `tour_editor_action::ask_code` reads the diff's selected lines
  (`selected_tour_source`) and opens the stop's thread on them.
- **Look**: `--attended-tour-drafting --attended-tour-conversation` drafts with
  the stub and accepts the Problem and Solution. It selects "between begin and
  present", presses Ask about this, asks and follows up with a rewrite in the
  Solution's thread, and accepts the rewrite there. It then accepts the
  draft's first stop, opens it, presses Ask about its code and asks why
  `clear()` comes first. The stop's lines show in the diff, and the reply (with
  a highlighted C++ block) shows under the stop.

### Verification (v002, focused)

`qt_tour_thread_view` (new: ask in context, Return, Stop, two turns under the
Solution, Accept in the thread, nothing in the draft's box, Ask about its code
with the stop's own lines and Show in code, highlighted reply and proposal),
`qt_tour_agent_session_view`, `qt_review_tour_agent_failure`,
`qt_review_tour_agent_help`, `qt_review_tour_agent_setup_call`,
`qt_review_tour_stop_view`, `core_tour_agent_session`,
`application_tour_agent_conversation`, `application_agent_launch`,
`application_writing_delivery`, `application_review_tour_writing` (+ wrapper),
`qt_markdown_github_1_0`, `application_review_tour_composition_public_path`,
`application_agent_chat_composition_contract`,
`application_review_round_trip_composition_contract` (resealed),
`architecture_boundaries`, `architecture_boundary_contract`: 18 of 18 pass.
`coding-format-check` passes; `verify_documentation.py` prints
`DOCUMENTATION_OK`. Offscreen look on the dev build and the frozen executable:
`ATTENDED_TOUR_ANSWER`, `ATTENDED_TOUR_FOLLOW_UP`, `ATTENDED_TOUR_SOLUTION`
(the accepted rewrite), `ATTENDED_TOUR_CODE:src/engine/renderer.cpp:3-8 ...`,
`ATTENDED_AGENT_TAB_TURNS:0`, `ATTENDED_TOUR_CONVERSATION_SHOWN`.

### Attended review (v002)

Frozen build `/Users/jaeger/bpr1914-tour-threads-v002.h3uvepo1` of `dce73be36` (executable SHA-256 `96db7fca290af2bf2719bd1f3811fd423c6f5b5e299812b3239e5c0270c1a1b2`); the launcher's
own offscreen check reached `ATTENDED_TOUR_CONVERSATION_SHOWN`. Launch:
`python3 /Users/jaeger/bpr1914-tour-threads-v002.h3uvepo1/launch.py`. New short session `2ebe870c371ac8ddd6d5c490add563b5`, state `/Users/jaeger/.codex/attended-review/bpr1914-v002-Pfb36H/state`, host started with
`serve --tailscale`. Candidate `BPR-1914-tour-threads` (revision
`dce73be36-v002`), one Accept/Reject question; BPR-1919 is the session's second
candidate. Status: ready, awaiting the owner.

### v002 rejected (2026-11-06)

BPR-1919 was accepted in the same session. The owner on BPR-1914 v002,
verbatim: "This is much closer than the previous version but it's too in the
way and noisy. I think I need something more like a side chat from the
chatgpt app. On each part I can pop open a side chat, ask some questions and
then the other part the current version is missing is being able to ask the
agent to update the section based on our chat. I dunno if each side chat is a
session or what, you figure out the best way." Session
`2ebe870c371ac8ddd6d5c490add563b5` recorded it; its host is stopped and the
v002 frozen root removed. The owner attached a reference picture of a desktop
chat app's docked side chat; it is not kept in the repository.

## v003: side chats (2026-11-06)

`frontend-design:frontend-design` was loaded first. The tour is left exactly as
it reads. The one new thing on it is a small speech-bubble glyph per part,
and the conversation lives in a pane docked at the window's edge, as the
owner's reference shows.

- **Session model** (RFC-BPR-0092 amendment v003, replacing v001's
  whole-conversation replay): one side chat is one thread. A turn replays that
  chat's own answered turns with the part's current text
  (`core::tour_conversation_of(session, thread)`). Chats are independent, kept
  in memory for the review, and no agent keeps a session.
- **Core**: `tour_agent_run::Update` and `Seconds`, `time_tour_run`,
  `keep_thread_part` (an Update keeps only its part), per-thread replay.
- **Application**: `build_tour_ask_prompt(..., aUpdate)` adds `"update":true`
  and asks for the part's whole new text from the chat.
- **Qt** (`tour_side_chat`, docked by `workbench_window` in a title-less
  `QDockWidget` on the right edge, hidden until a chat opens):
  - One tab per chat, + (tour, Problem, Solution, any stop), Copy chat and
    close.
  - What the chat is about (the part's text quoted, or a stop's title and
    file and lines) with Show / Show in code.
  - The author's right-aligned bubbles and the agent's rendered replies, with
    highlighted code (BPR-1919). Each reply has a folded "Worked for Ns" (the
    steps) and Copy.
  - **Update the Solution from this chat** (or the Problem, this stop, the
    tour), with its proposal decided in the chat.
  - A composer with Return to send and BPR-1916's `/skill` completion.
- **On the tour**: one glyph each beside the Problem's, the Solution's and a
  stop's helpers, Open side chat in their context menus (quoting a selection),
  and one on Draft the tour's row for the whole tour. A stop's chat shows its
  lines in the diff, and Ask about its code quotes the diff's selected lines or
  the stop's own. v002's threads and Ask buttons are removed. The draft's
  proposal box shows only whole-tour drafts.
- **Composition** (`window_tours.cpp`): `request_tour_ask(thread, passage,
  question, update)` replays the chat's own turns, merges BPR-1916's skill run,
  times each run, and keeps an Update's proposal to its part. A question sent
  while the session is still finishing, or with no agent, lands in its chat as
  a turn that did not run, saying why. `/skill` completion moved from v001's ask
  box to the side chat's composer (`window_skills.cpp`).
- Rebased onto `main` past BPR-1916/1917 (`8ad3fbd46`); the branch is one
  commit. The pre-rebase history is on the local branch
  `bpr1914-v003-before-rebase`.

### Verification (v003, focused)

18 of 18 pass, among them:
- `qt_tour_side_chat` (new). It covers the glyph and the selection quote, the
  dock, the tab per part and the subject line, Return, Stop, the bubble on the
  right, Worked for 7s folding, Copy reply and Copy chat. It also covers
  Update, with its proposal accepted in the chat and not in the draft's box,
  and BPR-1919's highlighting of a reply and a proposal in the stop reader's
  colours, with an unknown language staying plain. Last, it covers a stop's
  chat with its lines, Show in code, Ask about its code, and closing and
  reopening a chat.
- `qt_tour_agent_session_view` (BPR-1916's `/skill` completion, now in the
  composer).

The rest: `qt_review_tour_agent_failure`, `qt_review_tour_agent_help`,
`qt_review_tour_agent_setup_call`, `qt_review_tour_stop_view`,
`core_tour_agent_session`, `application_tour_agent_conversation`,
`application_agent_launch`, `application_writing_delivery`,
`application_review_tour_writing` (+ wrapper), `qt_markdown_github_1_0`,
`application_review_tour_composition_public_path`,
`application_agent_chat_composition_contract`,
`application_review_round_trip_composition_contract` (resealed),
`architecture_boundaries`, `architecture_boundary_contract`.
`coding-format-check` passes; `verify_documentation.py` prints
`DOCUMENTATION_OK`.

The offscreen look ran on the dev build and on the frozen executable. It
prints `ATTENDED_TOUR_ANSWER`, `ATTENDED_TOUR_FOLLOW_UP`,
`ATTENDED_TOUR_UPDATE`, `ATTENDED_TOUR_SOLUTION` (the accepted update),
`ATTENDED_TOUR_CODE:src/engine/renderer.cpp:3-8 ...` and
`ATTENDED_TOUR_CHATS:2`, then `ATTENDED_AGENT_TAB_TURNS:0` and
`ATTENDED_TOUR_CONVERSATION_SHOWN`.

### Attended review (v003)

- Frozen build `/Users/jaeger/bpr1914-side-chat-v003.ub9r3dy1` of `06fc9c99b` (executable SHA-256 `d322c6b65707acf77af359bf801979e3528645f289e9355790d3b1e62cfa398e`). The
  launcher's own offscreen check reached `ATTENDED_TOUR_CONVERSATION_SHOWN`.
- Launch: `python3 /Users/jaeger/bpr1914-side-chat-v003.ub9r3dy1/launch.py`.
- New short session `43c5a6c630697b0ad457408f287d4558`, state `/Users/jaeger/.codex/attended-review/bpr1914-v003-eEOn3M/state`, host started with
  `serve --tailscale`.
- One candidate, `BPR-1914-side-chats` (revision `06fc9c99b-v003`), with one
  Accept/Reject question.
- Status: ready, awaiting the owner.

### v003 rejected (2026-11-06)

v003 was merged to `main` as `8b3b8a327`. The owner then rejected the look in
session `43c5a6c630697b0ad457408f287d4558`, with four pictures. The pictures
stay in that session's state; none is kept in the repository. The owner's
note, verbatim:

> can we simplify the enhance buttons to just the "<symbol> Enhance" and leave
> the tooltips in place.
>
> The side chat buttons should be in the agent color.
>
> The side chat tabs behave weirdly, they kind of behave like the mane pane
> tabs but they don't stretch and I can't seem to close the first tab or even
> select it (solution in this screenshot)
>
> Clicking things in the tour tab is disorienting. The 3rd and 4th screenshot
> are my mouse not moving and just giving a single left-click to activate (1)
> but it scrolls almost all the way off the top of the screen.
>
> Also I can't tell but it seems like inline code isnt being highlighted only
> blocks.

The owner later added: "I liked the tabs I just wanted them to behave more
like our diff window tabs." The v003 host is stopped and its frozen root
removed.

## v004 (2026-11-07)

`frontend-design:frontend-design` was loaded first. v004 is on branch
`bpr1914-v004`, made from `origin/main` `8b3b8a327`.

- **Enhance.** Each field's writing helper reads "Enhance" beside the agent
  spark when it rewrites. The tooltips are unchanged ("Asks the drafting agent
  to rewrite this whole field ..." or "... only the selected text ..."). The
  transcript keeps the request's longer name. Draft from PR context, Generate
  comment and Stop drafting keep their words.
- **Agent colour.** The side chat glyphs beside each part (Problem, Solution,
  a stop) and on Draft the tour's row wear the BPR-1881 agent accent, as do
  the side chat's + and its Update and Send.
- **Tabs: the diff window's own.** The side chats' tabs are
  `document_tab_strip`, the diff window's tab strip, reused as it is:
  - equal tabs that stretch across the pane;
  - a close button on every tab;
  - the tab menu (Close Tab, Close Other Tabs);
  - + for a chat about another part (its Duplicate Tab opens the same parts
    menu).

  The strip gains `set_minimum_tab_width` so the pane fits three chats; the
  window's stays 220. The diff tabs have no middle-click or shortcut close, so
  the side chat has none either. `qt_tour_side_chat` clicks every tab, the
  first included, to select it, then closes the first, then the last, then the
  remaining one, each with its own close button.
- **No scroll jump.** I could not reproduce the jump offscreen; the owner's
  clicks were on macOS. I found two things a click sets off:
  - Going to a stop's code went through `open_review_tour()`, which reopened
    the Problem step and moved the keyboard there before showing the stops
    again. From the open Tour tab it now keeps the step
    (`open_review_tour(true)`).
  - The window then answers asynchronously: a file opens and the tour is
    presented again.

  A click on a stop, on Show in code or on a related source now holds the
  Tour tab's scroll position for 600 ms (`review_tour_editor::hold_scroll`).
  Any move the person did not make is put back; dragging the bar or turning
  the wheel ends the hold at once. `qt_tour_click_scroll` (new) clicks each of
  six stops, then Show in code and the pencil. The simulated window reopens
  the Problem, shows the stops, then sends the drawer to its end. The test
  fails without the hold ("219 then 263") and passes with it.
- **Inline code: coloured, in the part's language.** I chose to colour inline
  code, not only show it as monospace on a wash. It uses the Agent tab's
  BPR-1753 path (`set_inline_code_selector`):
  - a stop's chat, and the stop reader, use the stop's own file;
  - the tour's, the Problem's and the Solution's chats use the language most
    of the tour's stops are in;
  - with no language, inline code stays plain monospace on its wash.

### Verification (v004, focused)

21 tests pass:
- `qt_tour_side_chat` and `qt_tour_click_scroll` (new);
- `qt_scoped_prose_editor` and `qt_review_tour_agent_help` (Enhance label);
- `qt_document_tabs_scale_100` (the diff window's tabs, unchanged);
- `qt_tour_agent_session_view`, `qt_review_tour_agent_failure`,
  `qt_review_tour_agent_setup_call`, `qt_review_tour_stop_view`;
- `core_tour_agent_session`, `application_tour_agent_conversation`,
  `application_agent_launch`, `application_writing_delivery`;
- `application_review_tour_writing` and its wrapper;
- `qt_markdown_github_1_0`, `application_review_tour_composition_public_path`,
  `application_agent_chat_composition_contract`;
- `application_review_round_trip_composition_contract` (resealed);
- `architecture_boundaries`, `architecture_boundary_contract`.

`coding-format-check` passes; `verify_documentation.py` prints
`DOCUMENTATION_OK`. The offscreen look reaches
`ATTENDED_TOUR_CONVERSATION_SHOWN` with two chats, on the dev build and on the
frozen executable.

### Attended review (v004)

- Frozen build `/Users/jaeger/bpr1914-side-chat-v004.gye36phm` of `067c921f2` (executable SHA-256 `da32506f6f3287e26e273405babb5ddf4924f3fc77a9a6a133f7983e78c01dff`). Its launcher's
  own offscreen check passed.
- Launch: `python3 /Users/jaeger/bpr1914-side-chat-v004.gye36phm/launch.py`.
- New short session `ca5eae33ab574443875c249beee013db`, state `/Users/jaeger/.codex/attended-review/bpr1914-v004-QBUboY/state`, host started with
  `serve --tailscale`.
- One candidate, `BPR-1914-side-chats-v004` (revision `067c921f2-v004`), with one
  Accept/Reject question.
- Status: ready, awaiting the owner.

### v004 fix: a click on the first side chat tab (2026-11-07)

v004 was merged to `main` as `6220a9362`. On its frozen build, the owner
reported: "There's something eating click on the side chat tabs. I cant
reliably select them with a single click."

The coordinator checked the owner's running build, with permission. An
accessibility press switched to the "Solution" tab, but a real click did not;
a real click on "Stop 1" did.

- **Cause.** The side chat's constructor made a label for the empty state
  (`tourSideChatEmpty`) as a child of the pane and never put it in a layout.
  It stayed at the pane's top-left corner, 100 by 30 pixels, over the first
  tab. A real click there landed on the label, not the tab bar. The
  accessibility press does not hit-test, which is why it worked. The empty
  state the chat shows is a separate label made in `rebuild()`, so this one
  was a leftover.
- **Fix.** The stray label is gone (branch `bpr1914-v005-tab-click`).
- **Test.** `qt_tour_side_chat_tab_click` (new) clicks through the window, as
  the platform delivers a click, so the press also moves the keyboard. The
  tour's selected stop is Stop 1. The test runs with tabs that select on the
  press (offscreen Fusion) and on the release (macOS's style). It covers:
  - every tab that is not current, once, from the composer;
  - the left edge of a tab;
  - with the keyboard on the tabs;
  - each click followed by 700 ms of events, past the scroll hold.

  Each click must leave its tab current. The test also checks that every
  point along the tab row belongs to the tab strip. Before the fix it failed
  on the first click ("one click selects tab 1 (now 2)"). The label was under
  the tab.
