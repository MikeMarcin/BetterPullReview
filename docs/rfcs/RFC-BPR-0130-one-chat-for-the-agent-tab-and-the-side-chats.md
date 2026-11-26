---
id: "RFC-BPR-0130"
title: "One chat for the Agent tab and the side chats, under one header"
status: "Proposed"
metadata: {"origin":"owner request 2026-11-24 (verbatim below)","inspected_revision":"f84075042","inspected_lane":"worktree-agent-af351d74dc278d83f, main at f84075042","attended_review":{"session":"3cd822672efb50572b10ab10747889ba","state":"~/.mask/attended-review-sessions/bpr-agent-panel-shared-chat-20261124","status":"handed back 2026-11-24; three verdicts recorded; host stopped","design_copy":"design/ inside the state directory: the Phase 1 draft and its renders"},"mandate":"owner, 2026-11-24, attended review: header A as amended in the owner's note, the replacement words with the agent's name substituted, and one chat component in the listed stages; four questions remain open (see Open owner questions)","cards":"drafted, not allocated"}
depends_on: ["RFC-BPR-0085", "RFC-BPR-0100", "RFC-BPR-0101", "RFC-BPR-0126", "RFC-BPR-0128"]
---

# One chat for the Agent tab and the side chats, under one header

Every observation below was read at `f84075042`; the line numbers are that
revision's, to within a few lines. The owner chose the header, accepted the
words and accepted the direction in an attended review on 2026-11-24
([Owner answers](#owner-answers-2026-11-24)). The owner answered question 4
on 2026-11-25 (an item's side chat gains sessions); the other open
questions are built as their defaults.

## Problem and grounded current behavior

The owner, 2026-11-24, verbatim:

> We might need to clean up the top controls on the agent windows. We should
> also more closely reuse implementation between the side chats and agent chat
> so that we're not constantly having to double implement changes.

There are three chats with an agent, and a fourth that reuses one of them:

| Chat | Widget | Conversation state | Host |
|---|---|---|---|
| Agent tab (Review's and Overview's drawer) | `qt::agent_tab_view` (5,172 lines), over `agent_launch_panel`, `side_chat_panel` and `finding_queue_view` | `qt::agent_thread_turn`, and a live turn kept in the widget | `agent_chat_session_host`, `agent_conversation_host`, `agent_queue_host`, native sessions |
| Tour side chat (a stop, or the whole tour) | `qt::tour_side_chat` (2,302 lines) | `core::tour_agent_session` | the tour host, `agent_queue_host` per part |
| Report item side chat (a finding, a draft, a thread) | `qt::item_side_chat` (864 lines) | `core::follow_up_state` | `application::follow_up_host` |
| A local draft's discussion | `qt::tour_side_chat` again | `core::tour_agent_session` | the local draft's own lane |

### The controls at the top, and where the same fact lives elsewhere

**Agent tab** (`src/adapters/qt/src/agent_tab_view.cpp`).

| Control | What it does | Where | Finding |
|---|---|---|---|
| Drawer tabs, then **Open on GitHub** and **Hide the Status drawer** | The drawer's own tools | `overview_status_drawer.cpp:129-133` | Not the chat's. Open on GitHub is the icon that reads as a pop-out. Review's drawer has neither. |
| Title, "#2368 Queued prompt rows…" | Names the pull request | `:523`; set at `window_agent_setup.cpp:989` | Cut mid-letter where it does not fit, with no ellipsis. |
| Short revision, "3f9c2ab" | The head the session reads; its state in the tooltip | `:531`, `:1252-1260` | Seven characters here, twelve in the tour chat's notice. Useful only when the head has moved. |
| History (clock) | A menu | `:535-553` | Holds four unrelated things, below. |
| History › Sessions | A submenu: "Use Transcript replay with selected agent", then "Native: *title* (2 turns)" for each session | `:2466-2508` | "Native" and "Transcript replay" are internal words. Sessions are one level down. "(1 turns)". |
| History › Start a new session, Continue against the old revision | Actions owned by `side_chat_panel` (`side_chat_panel.cpp:230-234`), enabled only when the head moved | `:544-545` | Greyed almost always. A second "new session", with different code behind it than the button beside the menu. |
| History › Private history and runs | Replaces the conversation with the private run list, with "Back to the conversation" | `:547`, `:1047-1059` | A page swap from a menu. |
| History › Reset agents… | Clears every agent and its settings | `:1188` | A destructive settings action in a chat's history menu. |
| New session (pen) | Asks, then empties the thread and starts fresh | `:555-559`, `clear_session()` | Third way to start a session, with the banner's "Start a new session" (`:3746`). |
| "Native continuation" | Always shown; says "Transcript replay: …" or "Native continuation unavailable. …" otherwise | `:561-565`, `:2474-2476` | Internal wording, shown when there is nothing to say. Its tooltip speaks of KiB. |
| Under the composer: "$0.42 this session" | The session's cost | `:1033` | The window's footer also shows cost. |
| Window footer: agent, model, effort; run and Stop; cost | The one picker for every agent feature of the window | `agent_footer.hpp` | Already the single home for these. |

**Tour side chat** (`src/adapters/qt/src/tour_side_chat.cpp`).

| Control | Where | Finding |
|---|---|---|
| Agent glyph, subject, place, **Show in code**, Copy chat, Close | `:238-278` | The subject wraps to two lines because the tools take the row. |
| Notice above the composer: continuation mode (only when it is not the ordinary one), "Head changed. This chat reads 3f9c2ab11111; current head: 9d04e7c11111. Choose New session or Continue old revision.", **Continue old revision** | `:306-337`, `:660`, `:705-716` | The Agent tab's heading, chip and menu items, in another place with other words. |
| **Session 3** (bottom left): New session, Use Transcript replay with selected agent, "Native: …" entries, Other tour chats and detached history | `:410-447`, `:632-703` | The Agent tab's History at the other end of the chat, with a near copy of its menu code. Its tooltip gives different replay limits (12 turns, 48 KiB) from the Agent tab's (50 turns, 8 KiB a field, 48 KiB). |
| **Plan a check…** (local drafts), "$0.14 this session" | `:427-442` | |
| Stop beside Send while a follow-up can queue | `:397-407` | The Agent tab and the item's chat turn Send into Stop instead. |

**Report item side chat** (`src/adapters/qt/src/item_side_chat.cpp`).

| Control | Where | Finding |
|---|---|---|
| Agent glyph, subject, the claim, **Show**, Copy chat, Close | `:243-282` | A line-for-line copy of the tour chat's row with other object names. The subject is cut where it does not fit. |
| Status line above the composer | `:303-306` | The tour chat's notice and the Agent tab's banner, a third time. |
| **Update suggested comment** under the composer | `:353-360` | |
| No sessions, no history, no cost, no queue | | One chat an item, each round a fresh run. |

**The same fact, three ways.**

| Fact | Agent tab | Tour side chat | Item side chat |
|---|---|---|---|
| What the chat is about | The pull request's title | Subject, place, Show in code | Subject, claim, Show |
| Which session | Not shown; History › Sessions | "Session 3", bottom left | None |
| New session | Icon, top right; a menu item; a banner button | In the Session menu | None |
| Revision read | Chip, 7 characters | Only in the notice, 12 characters | None |
| How it continues | A heading, always | A notice, only when unusual | None |
| Cost | Under the composer, and the footer | Under the composer | None |
| Copy the conversation | No (each reply only) | Icon, top right | Icon, top right |
| Stop | Send becomes Stop | Stop beside Send | Send becomes Stop |
| Who answered | "Claude Code answered in 0:42." | "The tour agent answered in 0:31 · $0.14" | "Claude answered." |

### What is written more than once

Shared today, and the seed of the component: `composer_parts`,
`side_chat_parts`, `composer_field` and `passage_chip` (declared in
`tour_side_chat.hpp`, used by the item's chat), `agent_pending_list` (Agent
tab and tour), `agent_suggestion_list` (Agent tab and tour), `skill_completer`
(all three), `agent_run_status_line` (all three), `agent_activity_view` (Agent
tab and tour), `markdown_body_view`, `stick_to_bottom`,
`agent_reference_links`.

| Part | Agent tab | Tour side chat | Item side chat |
|---|---|---|---|
| Header row | `:506-565` | `:238-278` | `:243-282` (copy of the tour's) |
| Session and history menu | `present_native_history`, `:2466-2508` | `present_native_history`, `:632-703` (near copy) | none |
| Moved-head choice | banner `:3743-3750`, two menu actions | notice and button, `:705-716` | none |
| Transcript | `add_turn_widgets` and its helpers, about `:1390-2160`; one widget set a turn, updated in place | `rebuild()`, `:1613-2277`; everything rebuilt on each change | `rebuild()`, `:630-800`; everything rebuilt |
| Reply reader set-up | `dress_answer`, `fit_answer` | `reader()`, `:1579` | `reader()`, `:612` |
| Copy reply, ending line and its words | `:1660-1680` | `:1781-1800` | `:689-730` |
| Composer (field, height, focus, keys) | `:796-1030`, its own event filter and `side_chat_panel`'s | `:349-411` | `:311-349` |
| Send, Stop, Steer, Queue | `refresh_send_label`, `ask()` | `refresh_actions`, `send()`, `follow_up()` | `send()`, `Running` |
| Suggested prompts | `choose_prompt`, `press_offer`, `send_decided_prompt` | `choose_prompt`, `prompt_offers`, `refresh_prompts` | none |
| Queue presentation | `present_queue_rows` | `present_queue` per part, `show_queue` | none |
| Refusals and errors | send-refusal strip `:1005-1025`, banner `:3683-3790`, error turns | `refused_line`, the notice | `Status`, `item_chat_refusal_words` |
| Proposals | finding cards, report card | proposal items, revision card | proposal card with Use this and Discard |
| Cost line | `:1033` | `:427` | none |
| Skills | `present_skills` | `present_skills` | `present_skills`; `window_skills.cpp` feeds each |

A message becomes a run four ways: the Agent tab through
`window_agent_chat.cpp` and `agent_chat_session_host`; the tour through
`window_tours.cpp` (`request_tour_ask`, `dispatch_tour_run`) with each part's
queue in `window_chat_queues.cpp`; an item through `window_follow_up.cpp` and
`follow_up_host`, with no native session, queue, steer, cost or history; a
local draft's discussion through the tour chat's widget on its own lane
(`local_draft_desktop.cpp`).

Two structural causes make each change a double or triple implementation:

1. **There is no conversation value the three share.** The Agent tab's thread
   is a Qt struct (`agent_thread_turn`) plus about sixty members a turn inside
   the widget; the tour's is `core::tour_agent_run`; the item's is
   `core::follow_up_round`.
2. **The Agent tab's widget is also its state machine.** `agent_tab_view`
   borrows controls from three other widgets and reparents them (`Borrowed`,
   `:513`), reads its state from them 145 times (`Launch.`, `Chat.`,
   `Findings.`), listens to their item models, and runs the live turn itself
   (`begin_turn`, `update_turn`, `progress`). That is policy in Qt, and it is
   why nothing of the Agent tab's transcript can be lifted into a side chat.

## Proposed behavior and boundaries

### Decision

One chat component presents every conversation with an agent. It has four
parts, top to bottom: **header**, **transcript**, **notices and pending
messages**, **composer**. The Agent tab, the tour's side chat, the item's side
chat and a local draft's discussion are that component given different
subjects, different extras, and different hosts behind the same intents.

### The header

One row, the same in every chat (the owner's choice, header A as amended):

| Chat | The row, left to right |
|---|---|
| Agent tab | The pull request's title, ending in an ellipsis where it does not fit; **History** |
| Tour side chat, a local draft's discussion | Subject and place; **Show in code**; **History**; **Close** |
| Item side chat | Subject and place; **Show**; **Copy conversation**; **Close** |

There is no New session button in the row. A chat with a single extra action
shows it as its icon rather than a menu of one, which is why the item's chat
keeps Copy and has no History.

**The History menu**, top to bottom:

1. **New session**, always first.
2. The sessions, one row each, from the session list component that session
   management provides (this RFC consumes it and does not build it). A row is
   the session's name, ticked when it is the current one, with three marks at
   its right: a pencil to rename it, a trash can to delete it, and an **i**
   whose tooltip gives its turns and what it cost.
3. **Delete all sessions…**. There is no "Rename or delete sessions…" item.
4. Chats about other parts of the tour (tour side chat only).
5. **Copy conversation**; **Show every run…** (Agent tab only).
6. Last, greyed: "Reads the pull request at 3f9c2ab".

A session's name is its first question until it is renamed.

Where each fact lives after this:

| Fact | Where |
|---|---|
| Which session, and switching | The History menu |
| A session's turns and cost | Its row's **i** tooltip |
| What the pull request's agent work has cost; the agent, model and effort; a running turn and Stop | The window's footer, unchanged |
| The revision a session reads | The menu's last line; and the pinned-revision card when the head has moved |
| That a conversation was carried over, or cannot be continued by the chosen agent | A notice, only then |

Removed: the "Native continuation" heading; the always-on revision chip; the
New session button; the agent glyph beside a side chat's subject (the seam
says whose column it is); "Start a new session" and "Continue against the old
revision" as menu items (the pinned-revision card carries them); the tour
chat's Session button and the cost line under each composer.

A consequence the owner's note implies: a session's own cost is no longer on
the chat's face. It is in the row's tooltip, and the footer keeps the total.

### Words

Accepted by the owner, with the agent's name substituted. `{agent}` is the
chosen agent's display name ("Claude Code", "Codex", a custom agent's name);
no sentence hard-codes one.

| Today | From this RFC |
|---|---|
| Native continuation | (nothing) |
| Transcript replay: saved questions and replies; prior tool memory is absent; PR files and Git are shared. | Carried over from a saved transcript. {agent} has the questions and answers, not what the earlier agent read. |
| Native continuation unavailable. Choose Transcript replay or New session in History. | {agent} can't pick up this conversation where it stopped. Start a new session, or carry the transcript over. |
| Use Transcript replay with selected agent | Carry this conversation over to {agent} |
| Native: *title* (2 turns) | *title*; its turns and cost in the row's tooltip |
| Session 3 | The session's name |
| Private history and runs | Show every run… |
| Head changed. This chat reads 3f9c2ab11111; current head: 9d04e7c11111. Choose New session or Continue old revision. | The pinned-revision card's words, the same in every chat, seven characters |
| "Claude answered." / "The tour agent answered in 0:31 · $0.14" | One ending line: who, how long, what it cost |
| Replay uses at most 50 retained turns, 8 KiB per field and 48 KiB total… | Long conversations are shortened: the oldest turns are left out. (The exact limits stay in the diagnostic log.) |

**Note, 2026-11-25 (the table above is kept as the owner accepted it).** The
action these words call "Start a new session" is named **New session**
wherever it shows: History's first item, the pinned-revision card's second
choice, and every refusal, notice and tooltip that names it. The card took
History's name when its two choices were put on one line, and the owner
accepted that wording on 2026-11-25.
The third row is therefore shipped as "{agent} can't pick up this conversation
where it stopped. Choose New session, or carry the transcript over." The
replacement words for the continuation heading, the carried-over notice, the
carry-over item, "Private history and runs" and the replay's limits landed
ahead of the rest of stage 1, with that correction, after the owner asked of
the heading: "I thought we were getting rid of 'Native continuation'
altogether". The Agent tab's notice stands on the question field, where the
tour chat's does. "Head changed. …" (stage 6) and the one ending line
(stage 3) are not part of that.

### One chat component

**Core decides** (`src/core/`, pure values and functions):

- `chat_header`: the subject (title, place, whether it can be shown), the
  session (name, revision read), and the set of offered actions. Which actions
  a chat offers is a core decision, not a widget's `setVisible`.
- `chat_transcript`: turns by stable identity. A turn is the question, the
  steered messages, the reply (Markdown, outcome, ending: who, seconds, cost),
  the activity, the offered turn actions, and **attachments**: a typed list a
  chat's own projector fills (findings block, report card, verdict, tour
  proposal, tour revision, follow-up proposal).
- `chat_live`: the running turn's status. `chat_notice`: one value for a send
  refusal, a stopped run, a moved head, a carried-over conversation.
- `chat_delta`: what changed between two transcripts (turn appended, reply
  grew, turn settled, turn removed), so Qt updates one turn and never rebuilds
  the column.
- Three projectors, `project_agent_thread`, `project_tour_thread` and
  `project_follow_up`, from the states that exist today. Those states stay as
  they are.
- The live turn's reducer, moved out of `agent_tab_view`.

**The host performs** (`src/application/`): `chat_projection` (header,
transcript, live, notices, pending rows) and `chat_intent`, one variant: send
(text, steer or queue), stop, new session, choose session, continue at the
pinned revision, carry over, a turn's action, a queue row's action, and a
typed extension a chat defines (accept a tour proposal, use a follow-up
proposal, draft a finding). The hosts keep their jobs; the composition maps an
intent to the host it already calls.

**Qt presents** (`src/adapters/qt/`): `chat_view`, made of `chat_header_view`
and the History menu; `chat_transcript_view`, applying `chat_delta` by turn
identity; `chat_notice_view`; `agent_pending_list` as it is; `chat_composer`
(the field, a chip slot, a tools slot, Send and Stop, Return and Cmd+Return,
`skill_completer`, the suggested prompts); and an attachment factory a chat
registers. `chat_view` receives a projection and emits intents. It holds no
launcher, no other widget's controls and no turn state.

### What stays a chat's own

| Chat | Its own |
|---|---|
| Agent tab | Finding cards, the report card and verdict, Draft all, Keep local and Draft on a reply, the conversation and lines chips, suggested-prompt macros, setup in place of the conversation, Show every run |
| Tour side chat | One chat a part and the switch between parts, the quoted passage chip, proposal and revision cards with Accept, Update this stop from this chat, Plan a check… |
| Item side chat | The quoted item at the top, the one apply action, Use this and Discard, the column's width handle, belonging to a document tab |

### Stages

Each stage lands alone, changes one thing a reviewer can see or none, and
keeps the drawer-fit course, the side chats' Send course and each chat's own
tests passing.

| Stage | Change | Visible |
|---|---|---|
| 0 | The work in flight lands (below). No stage 1 code starts before it. | its own |
| 1 | The header and the History menu in all three chats, and the words. Removes both `present_native_history` menu builders, the heading, the chip and the New session button. | Yes |
| 2 | `chat_composer` extracted from the Agent tab's field and adopted by both side chats; one Send and Stop rule. | Stop in one place |
| 3 | `chat_transcript`, `chat_delta`, `chat_transcript_view`; the item's side chat adopts it, with the follow-up proposal as the first attachment. | One ending line |
| 4 | The tour side chat adopts it; proposal and revision cards become attachments. | No |
| 5 | The Agent tab adopts it; finding cards, report card, verdict and Draft all become attachments; the live turn's reducer moves to core; `agent_tab_view` stops borrowing controls. | No (see open question 1) |
| 6 | `chat_notice` for the refusal strip, the banner, the pinned-revision card, the tour's notice and the item's status line. | One notice style |
| 7 | An item's chat runs on the session runner; `side_chat_panel`'s hidden controls are deleted. | Yes: History, sessions and Stop in the item's chat (open question 4, answered 2026-11-25) |

### Work in flight this RFC builds on

| In flight | Here |
|---|---|
| Report side chat reads the pull request's workspace read-only (`window_follow_up.cpp`, `follow_up_host`, `launch_follow_up_round`) | Untouched by stages 1 to 6, which sit above the host. It is the first half of stage 7. |
| Pinned-revision card, Continue at *rev* and Start a new session (`core::offer_pinned_conversation`) | The one place the moved head and those two actions show. Stage 6 makes it a `chat_notice` for every chat. |
| Session management: the reusable session list (rows with rename, delete and the info tooltip), Delete all sessions… | The History menu hosts that component in the Agent tab and the tour side chat. |
| An accepted side-chat proposal updates the report's suggested change and draft | An attachment's action; the fix lives in the composition and is not touched. |
| Queued rows leave the pending list once a turn begins (landed) | `agent_pending_list` is reused as it is. |

### Boundaries

- No new agent capability, and no change to what an agent may read or write.
- The window's footer keeps the picker, Stop and the total cost.
- No QSS and no forced palette; the header is built from `quiet_tool_button`
  and `make_drawer_tool` and fits the narrowest drawer without panning.
- Stored conversations, native sessions and queues keep their formats.
- The drawer's own tools (Open on GitHub, Hide the Status drawer) are outside
  this RFC (open question 3).

### Architectural allocation

| Layer | New | Removed by the end |
|---|---|---|
| `src/core/` | `chat_header`, `chat_transcript`, `chat_delta`, `chat_notice`, three projectors, the live-turn reducer | none |
| `src/application/` | `chat_projection`, `chat_intent` | none before stage 7 |
| `src/adapters/qt/` | `chat_view` and its parts, the attachment factory | the three header rows, two history menu builders, two composer copies, two `rebuild()` transcripts, the Agent tab's borrowed controls |
| `src/app/` | one binding from `chat_intent` to the existing hosts, a chat at a time | the wiring it replaces |

## Owner answers (2026-11-24)

Attended review, session `3cd822672efb50572b10ab10747889ba`, handed back
2026-11-24. The candidates were renders of mock rows on the live widgets; no
build was frozen.

| Question | Verdict | The owner's note, verbatim |
|---|---|---|
| Which header should the Agent tab and side chats share? (A, one row with a History menu; B, a named session line at the top; C, the session line under the composer) | **A** | "A but move new session into that menu as the top item so it doesn't take up extra space on the drawer and maybe put trash can and pencil on the session rows in the menu to delete or rename them. Put the cost and turns in a info tooltip to mouse over to get space for the trash and pencil and i. Rename or delete sessions can then go away as a menu option and you can just have delete all sessions or something like that. And then " |
| Are these the right words for sessions and history? | **Accept** | "Yes assuming Codex there is {agent}" |
| Build one chat component for all three chats? | **Accept** | |

The first note ends mid-sentence as recorded. Nothing in this RFC is inferred
from the missing words (open question 5).

## Open owner questions

Each is built as its default until answered. Question 4 held stage 7 back
until the owner answered it on 2026-11-25.

1. **The transcript's look.** The Agent tab draws questions and replies as
   cards; the side chats draw a bubble and a plain reply. Should the Agent tab
   take the side chats' look, or keep its cards? Default: the one transcript
   view keeps both looks and no chat changes appearance. Affects stage 5 only.
2. **Reset agents…** is a destructive settings action in the Agent tab's
   History menu. May it leave for Settings › Agents? Default: it stays, last
   in the menu, after a separator.
3. **Open on GitHub** in Overview's drawer row reads as a pop-out. Leave it,
   move it, or give it another glyph? Default: untouched; it is the drawer's
   tool and outside this RFC.
4. **Sessions for an item's chat.** Should the side chat about one item gain
   sessions, cost and Stop by running on the session runner, or stay one
   conversation an item? Stage 7 does not start until this is answered.
   **Answered 2026-11-25** (the owner, relayed by the RFC's coordinator,
   verbatim): "sure seems like they should right". An item's side chat gains
   sessions: History with New session and the session rows, a session's cost
   in its row's tooltip, and Stop. Stage 7 is unparked and runs after
   stage 6.
5. **The rest of the header note.** The owner's note on the header stops at
   "And then ". What followed?

## Acceptance criteria

- **AC01:** In the Agent tab, the tour side chat and the item side chat the header is one row from one widget and one core value: what the chat is about, History where the chat has sessions, Copy where it has none, Close where the column closes; no row holds a New session button.
- **AC02:** The History menu opens with New session first; each session below it is a row of the shared session list with rename, delete and an info mark whose tooltip gives its turns and cost; Delete all sessions… follows; no "Rename or delete sessions…" item exists; the last line names the revision read.
- **AC03:** In no state does a chat show "Native continuation", "Transcript replay" or "Native:"; with two differently named agents chosen in turn, every replacement sentence that names an agent names the chosen one.
- **AC04:** Every header and menu state fits the narrowest drawer and the narrowest column at 100%, 125% and 200% with no sideways pan, and a title too long for its row ends in an ellipsis with the whole title in its tooltip.
- **AC05:** One composer widget serves the three chats: Return sends, Shift+Return starts a line, the other-action chord steers or queues where the chat can, and a running turn is stopped from the same control in each.
- **AC06:** One transcript widget serves the three chats; while a reply grows, the widgets of the other turns are the same objects before and after, in every chat.
- **AC07:** `agent_tab_view` holds no control owned by another widget and no state of a running turn; the turn's state comes from a core reducer covered without Qt.
- **AC08:** A send refusal, a stopped run, a moved head and a carried-over conversation are each shown by the one notice, with the same words, in every chat that can reach that state.
- **AC09:** The owner accepts the header and its menu on a frozen tests-on build that opens on a pull request with a tour, a report and at least two agent sessions, run with the Fixture agent.
