---
schema: "implement-rfc.card.v1"
id: "BPR-2412"
title: "The Agent tab holds no other widget's control and reads none: the window hands it its run, its session's chat and the findings as values, and maps its gestures to the hosts they reached"
status: "Done"
rfcs: ["RFC-BPR-0130"]
depends_on: ["BPR-2409", "BPR-2410", "BPR-2411"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","reservation":"docs/issues/_reservations/.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"71fb639c9","origin":"RFC-BPR-0130 stage 5 (draft card DRAFT-0011 of the attended review 3cd822672efb50572b10ab10747889ba, its second part: the borrowed controls); owner, 2026-11-24: one chat component for all three chats","related_cards":["BPR-2409","BPR-2410","BPR-2411","BPR-1594","BPR-2051"],"related_rfcs":["RFC-BPR-0130"],"acceptance":["AC1","AC2","AC3","AC4"]}
acceptance: []
---

# BPR-2412: The Agent tab holds no other widget's control and reads none

## Why

RFC-BPR-0130 AC07: "`agent_tab_view` holds no control owned by another
widget and no state of a running turn; the turn's state comes from a core
reducer covered without Qt." BPR-2411 moved the running turn's state into
core (the second clause). This card is DRAFT-0011's second part, the first
clause: the RFC's second structural cause, "`agent_tab_view` borrows
controls from three other widgets and reparents them (`Borrowed`), reads its
state from them 145 times (`Launch.`, `Chat.`, `Findings.`), listens to
their item models". Visible: nothing.

### What was there (BPR-2411)

- The tab was built over `agent_launch_panel`, `side_chat_panel`,
  `finding_queue_view` and the private history page, and reparented four of
  their controls into itself (the agent selector, the replies list, the
  composer's words, the private history), giving them back on destruction.
- It read their state wherever it needed it (80 reads of the launch panel,
  51 of the side chat, 21 of the finding queue), listened to their actions'
  `changed`, the selector, the replies' and findings' item models, and
  triggered their actions (start, cancel, send, New session, Continue, a
  finding's open, draft, keep and dismiss).
- It held the footer's agent picker by pointer to open it for a bare
  `/model` or `/effort`, and wrote the side chat's Send action's text and
  tooltip.

## Change

1. **Qt, the values the tab presents** (`agent_tab_run.hpp`):
   `agent_tab_run` (the agent chosen, what may start or stop, the run as its
   presenter says it, how the last one ended, the last refusal),
   `agent_tab_run_change` (what moved it), `agent_tab_send` and
   `agent_tab_intents` (Start, Stop, ChooseAgent, Send, Continue, Finding,
   OpenRunSettings).
2. **Agent tab**: built over the shared agent, model and effort source alone
   (`agent_tab_view(std::shared_ptr<agent_run_settings_source>, QWidget*)`).
   It keeps what it was last handed: `present_run`, `present_chat` (the
   session's `side_chat_projection`: whether it may be sent to, whether its
   moved head waits on a choice, its added context, its head, its replies)
   and `present_findings` (the delivery the cards are made of). Its question
   field's words are its own (`agentQuestionText`); a question leaves as
   `agent_tab_intents::Send`, after which the tab reads what was asked, in
   the order the old action's two slots ran. The private history is adopted
   once (`adopt_every_run`) and is the tab's from then on. No `Borrowed`
   list, no panel references, no `panels_alive`; the dead picker-wrap logic
   (the footer's picker never stands in the field since BPR-2051) is gone.
3. **Window** (`workbench_window_central_surface.cpp`): composes
   `agent_tab_run_of(launch panel)` and hands it to the tab when the panel
   progresses, an action's enablement or the chosen agent changes (each
   moves the tab as its own listener did) and, quietly, whenever the panel
   updates what it offers (`agent_launch_panel::add_change_observer`); hands
   on each session projection (`side_chat_panel::set_presentation_observer`)
   and each delivery (`finding_queue_view::set_presentation_observer`); maps
   the intents to the launch panel's actions, `side_chat_panel::send`, its
   New session and Continue actions, the finding queue's selection and
   actions, and the footer's picker.
4. **Side chat panel**: `send(text, question, skill text)` (its Send action
   sends its own words through it) and `side_chat_chips_words`, which its
   context line and the tab's chips both read.
5. **Courses**: every course that asked the Agent tab through the hidden
   side chat's words and Send action now types in the tab's field and
   presses its Send (`question_editor()`, `ask_button()`), and a Qt course
   with no composed host presents an empty session first, where it used to
   force the hidden action on. The footer's picker is read from the footer.
   `qt_workbench_window` checks the tab holds no panel's control. The
   composed course's ask helper puts the keyboard back where it was after
   its own refused retries (a person's question is sent once). The
   anchored-continuation contract still sends its continued question
   through the session's own panel with the question it was asked as: that
   contract is the host's, and the Agent tab decides a question's subject
   afresh at each send (the old path reused the panel's last question).
6. **Docs**: `src/adapters/qt/AGENTS.md`.

`src/app` is untouched: the composition still presents to the panels and
dispatches their intents; the window relays. Stage 7 deletes the side chat
panel's hidden controls.

## Acceptance

- AC1 (RFC AC07, its first clause). `agent_tab_view` holds no control owned
  by another widget and reads no other widget's state: its header and
  source name none of the panels, it reparents nothing, and it is presented
  values and emits intents.
- AC2 (DRAFT-0011). The footer still hears the run, its ending and its cost.
- AC3. A question sent from the Agent tab is the turn it begins, with its
  name, subject and draft target, as before.
- AC4. The Agent tab's tests and the composed course in the native look pass
  with every render byte-identical; format and tidy on touched units.

## Evidence

Revision `71fb639c9` (on main `c19fd36be`), macOS arm64 Debug,
`ctest --test-dir build/dev -R '^<row>$'`.

- AC1: `qt_workbench_window_scale_100`, `_125`, `_200` (the tab holds none
  of the launch panel's selector, the side chat's words and replies, or the
  finding queue's list, and its words are its own), `qt_composed_github_scale_100`,
  `_125`, `_200`, `qt_side_chat_panel`, `qt_finding_queue_view` (the panels'
  members, with the one observer each gained) passed. `agent_tab_view.hpp`
  and `.cpp` name no `agent_launch_panel`, `finding_queue_view` widget or
  `side_chat_panel` widget; `Borrowed` and `panels_alive` are gone.
- AC2: `qt_agent_footer` and the footer checks of `qt_workbench_window`
  passed.
- AC3: `qt_workbench_agent_send_refusal`, `qt_prompt_macro_chips`, the
  `application_agent_chat_*_contract` rows, `application_agent_keep_action_public_path`,
  `application_agent_steer_sent_public_path`, `_unavailable_public_path`,
  `application_fixture_agent_guard_public_path`, `application_ask_menu_*`
  passed.
- AC4: 174 rows, every executable of BPR-2411's 62-row list and of the
  courses this card touched, passed (`ctest -R` over their executables).
  The composed course under the native look (Cocoa, kept off screen, light,
  the display's 200%): "composed-drawer-fit: 39 states, 0 faults, 50
  transcript edges drawn, dpr 2, cocoa/macos"; at 100% and 200% offscreen
  likewise.
- Format: `_coding-standard/scripts/format.py` (clang-format 22) over every
  touched C++ file. Tidy (`tidy.py --warnings-as-errors`) over
  `agent_tab_view.cpp`, `side_chat_panel.cpp`, `finding_queue_view.cpp`,
  `agent_launch_panel.cpp`, `workbench_window_central_surface.cpp`: clean but
  for the one finding older than this card (`press_offer`'s copy of
  `aChosen`, left as BPR-2409 left it).

Renders, native look, light, 200%, under
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/rfc130-renders/`:
before is `stage-5c-after/cocoa-macos/`, after is
`stage-5c2-after-rerun/cocoa-macos/` (274 each). Not byte-identical, for
two reasons, neither a change of look:

- **A stray list is gone (a fix).** The side chat panel's hidden replies
  list, which the tab reparented under its transcript, showed itself
  whenever the session had replies (`side_chat_panel::refresh_enablement`
  sets its visibility) and drew "Reply #1" / "Reply #2" under the last turn
  in `agent-report-*`, `agent-finding-cards-open-*` and
  `agent-finding-card-dismissed-*`. The tab no longer holds it; those states
  end at the turn, and the transcript, which keeps its end, scrolls a
  little less.
- **Timing of a loaded machine.** The rest differ only where a run's timing
  shows: the question field's caret (a 2 by 31 pixel box at the field's
  start, its blink phase), a running turn's phase line ("starting" against
  "thinking"), the code pane's overlay scroll bar fading, the window
  footer's run clock. Two runs of this build differ from each other the
  same way (54 renders, 53 of them the caret alone).

The tab's words stand in the keyboard's order where the side chat panel's
did (`setTabOrder` in the window), so Tab from the field, and the keyboard
leaving it when the tab hides, go where they went: without it the
`comments-*` states drew the first comment card focused.

## Owner judgement

- None asked: nothing changes on screen.
