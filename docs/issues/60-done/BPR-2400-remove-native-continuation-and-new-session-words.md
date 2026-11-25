---
schema: "implement-rfc.card.v1"
id: "BPR-2400"
title: "The Agent tab's \"Native continuation\" heading is gone and its transcript starts under the header's row; a chat's transcript draws its top edge while a turn is cut there; a refusal names the card's choices Continue at its head and New session; and the chats say carried over, not Transcript replay"
status: "Done"
rfcs: ["RFC-BPR-0130", "RFC-BPR-0096"]
depends_on: ["BPR-2393", "BPR-2394", "BPR-2384"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","reservation":"docs/issues/_reservations/BPR-2400--BPR-2400--0d3f37668863.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"6edc4d90d","origin":"owner, attended review be60db9187059d2cbc11412859b53947 (frozen at 5bc62c938), 2026-11-25: the refused question's first line partly hidden under the Native continuation line, and the refusal still saying \"start a new session at <sha>\"; then, the same day, relayed by the coordinator verbatim: \"I thought we were getting rid of 'Native continuation' altogether\"","related_cards":["BPR-2393","BPR-2394","BPR-2392","BPR-2389","BPR-2384","BPR-2377","BPR-2296","BPR-2059","BPR-1995"],"related_rfcs":["RFC-BPR-0130","RFC-BPR-0096","RFC-BPR-0085","RFC-BPR-0100"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6","AC7","AC8"],"rfc_stage":"RFC-BPR-0130 stage 1's words, landed early (see Change 3)","owner_step":"attended look at the refused question and the carried-over session in a frozen tests-on build with the Fixture agent (see Owner judgement)"}
acceptance: []
---

# BPR-2400: No "Native continuation" heading, a drawn edge over a cut turn, and the accepted words for a carried-over conversation and for New session

## Why

Owner, attended review `be60db9187059d2cbc11412859b53947` (frozen at
5bc62c938), 2026-11-25, of the Agent tab after a refused question, in a 280 px
drawer (composed course state `agent-pinned-refused`):

1. The first line of the question ("What changed in this push?") was partly
   hidden under the "Native continuation" line at the top of the transcript.
2. The refused turn still said "...or start a new session at cd24836" while
   the card's chip says **New session** (BPR-2393 renamed it; the owner
   accepted the new wording).

Then, of the heading itself, relayed by the coordinator verbatim:

> I thought we were getting rid of 'Native continuation' altogether

RFC-BPR-0130's accepted header (A) and accepted words drop it ("Native
continuation" becomes nothing); BPR-2384 left the words for stage 1.

### What was happening (main 5bc62c938)

Nothing lay over the question. Measured in the frozen build's render
(`before-5bc62c938-dark/cocoa-macos/frozen-dark-200-agent-pinned-refused-280.png`):

- The thread keeps itself at its end (`stick_to_bottom`, BPR-2296). In that
  state it held the session's earlier turns, the question, the refused turn
  and five suggested prompts: about 2,150 px in a viewport of about 460
  (measured here in the same state: 2,147 in 458, scrolled 1,689). At its end
  the viewport's top fell where it fell, which at 280 px was inside the
  question's bubble: its time stamp and the bubble's upper edge were scrolled
  out. Where the cut falls depends on every height above it. With the drawer
  at 340 or 420 px it falls in the turn before, and no layout keeps it out of
  a turn.
- Directly over the viewport stood "Native continuation": a `QLabel` added to
  the tab's column with no margins, so its words began at the drawer's very
  edge (x = 0, where the header's title begins at 10), in the transcript's own
  ink and size, with no room and no line between it and the viewport.
- So a turn cut by the viewport read as a turn covered by that line. The same
  cut shows in the tour's side chat under its header (a bubble's last line,
  "compile", in `frozen-dark-200-tour-stop-chat-head-moved-300.png`).

The heading said nothing a reviewer acts on in the ordinary case, and its
other two texts ("Transcript replay: saved questions and replies; prior tool
memory is absent; PR files and Git are shared.", "Native continuation
unavailable. Choose Transcript replay or New session in History.") and its
tooltip ("Replay uses at most 50 retained turns, 8 KiB per field and 48 KiB
total…") are the internal words RFC-BPR-0130 replaced.

The words for the card's second choice lived in two places that BPR-2393 did
not touch: the application host's refusal text (`pinned_conversation_words`,
`move_refusal_words`: "start a new session at *rev*") and the Agent tab's
send refusals ("Start a new session or continue on the old head, then send").
The core holds no words for it: `decide_conversation_move` and
`offer_pinned_conversation` answer what is offered, and the host and Qt say it.

## Change

1. **The header is its one row** (`agent_tab_view`). The continuation heading
   and its label (`agentContinuationMode`) are removed. Nothing stands between
   the header's row and the transcript. What a session has to say of itself
   is a quiet notice on the question field (`agentSessionNotice`,
   `session_notice()`), under the transcript and over the pinned-revision card
   and the field, where the tour's chat has had its own since BPR-1995: said
   only for a session carried over or one the agent cannot pick up, empty and
   hidden for every other. The line that says a session's shared checkout
   moved while another session asked (`agentConversationMoved`) stands there
   too.
2. **A drawn edge over a cut turn** (`transcript_edge`, new). A chat's
   transcript draws one hairline on its viewport's first row exactly while a
   turn is cut there, as a window's toolbar shows its separator only while
   content is scrolled under it. The blank room a transcript keeps over its
   first turn is not a turn, so a transcript a few pixels taller than its
   viewport draws nothing; one at its start draws nothing. The line is the
   cards' own hairline on the drawer's ground, takes no room in any layout and
   no pointer or keyboard event. Attached where `stick_to_bottom` is: the
   Agent tab's conversation, the tour's side chat and an item's side chat.
3. **RFC-BPR-0130 stage 1's words, landed early**, with the agent's display
   name where the RFC writes {agent} (the Agent tab's picker; the tour's side
   chat is told the name by the composition, `set_agent_name`; "The agent"
   while none is known). One set of words for both chats
   (`chat_history_menu::carried_over_words`, `cannot_continue_words`,
   `carry_over_words`, `carry_over_tip`):

   | Before | Now |
   |---|---|
   | Native continuation | (nothing) |
   | Transcript replay: saved questions and replies; prior tool memory is absent; PR files and Git are shared. | Carried over from a saved transcript. *agent* has the questions and answers, not what the earlier agent read. |
   | Native continuation unavailable. Choose Transcript replay or New session in History. | *agent* can't pick up this conversation where it stopped. Choose New session, or carry the transcript over. |
   | Use Transcript replay with selected agent | Carry this conversation over to *agent* |
   | Private history and runs | Show every run… |
   | Replay uses at most 50 retained turns, 8 KiB per field and 48 KiB total… (Agent tab); Replay keeps at most the newest 12 answered turns within 48 KiB… (tour chat) | Long conversations are shortened: the oldest turns are left out. (and, when it left any out, "This one left out *n* older turns.") |
   | A session row's i: "… Transcript replay." | "… Carried over from a saved transcript." |
   | History's tooltip: "… New session and private runs" | "… New session and every run" |

   The host's refusals of the same cases say the same: "*agent* can't pick up
   this conversation where it stopped. Choose New session, or carry the
   transcript over. No question was sent.", "… Choose New session, or carry
   the transcript over." for a question that may have been sent before an
   interruption, "This carried-over conversation changed or is unavailable.
   No question was sent." and "*agent* can't pick up this conversation with
   its current settings. Nothing was carried over, and no question was sent."
   The actions' object names follow (`agentCarryOver`, `tourCarryOver`,
   `agentShowEveryRun`; the tour's notice is `tourSessionNotice`).
4. **New session, by that name, wherever it is named.** A refusal names the
   card's choices as its chips do and no other way:
   - host, a pinned conversation: "… Choose Continue at 3333333 to keep asking
     about that revision, or New session to ask at 2222222. No question was
     sent."; another pull request's: "… Choose New session. No question was
     sent.";
   - host, a workspace that was not moved: "… Choose Continue at b20cb44 or
     New session. No question was sent." (two ways are "one or the other";
     with the reconcile choices they are listed with commas);
   - Agent tab, a send refused before it leaves: "The pull request has moved
     on. Choose Continue or New session, then send." and "This conversation
     is with another agent. Choose New session in History to ask *agent*.";
   - a workspace that could not be made: "… Choose New session in History;
     this conversation's transcript and files are kept.";
   - the side chat panel's borrowed action reads New session;
   - the comments BPR-2393 noted (`chat_history.hpp`,
     `window_agent_chat.cpp`, `window_agent_setup.cpp`) and
     `agent_tab_view.hpp`.
5. **The composed drawer course** (`composed_drawer_fit_public_path.cpp`)
   judges, in every state at every width:
   - no label or button that is not a scroll area's own is drawn inside its
     viewport;
   - a transcript that keeps itself at its end has an edge; with anything it
     holds cut at its top or gone above it, the edge is on the viewport's
     first row across its width and drawn there (nine tenths of the row's
     pixels or more in the edge's colour, read from the window's own pixels);
     with nothing cut it draws none;
   - nothing stands between the Agent tab's header row and its transcript;
   - no label, button or menu item (a menu is read open or closed) says
     "Native continuation", "Transcript replay", "Native:" or "Private history
     and runs", in its text or its tooltip.
   It also requires the refused turn to end "Choose Continue at *earlier head*
   or New session. No question was sent.", the carried-over session to say so
   on the question field naming the Fixture agent, History to offer "Carry
   this conversation over to Fixture agent", the refused session to have no
   notice, and at least one transcript judged with its edge drawn. Each new
   check has its negative controls in `check_the_checks`.
6. **Docs**: `docs/ux/workbench-specification.md` (the header, the notice,
   the edge, History's items, the course), `docs/ux/ai-experience.md`,
   `docs/ux/shared-action-buttons.md`, `docs/ux/private-review-workflow.md`,
   `docs/architecture/review-tours.md`, `docs/architecture/ai-agent-boundary.md`,
   `src/adapters/qt/AGENTS.md`. RFC-BPR-0130's words table and RFC-BPR-0096's
   continue-at-latest amendment are kept as recorded, each with a dated note
   that the action is named New session and, in RFC-BPR-0130, that these
   words landed ahead of the rest of stage 1.
7. **Sealed manifest**: `window_tours.cpp` (one call to the new Qt setter),
   and two comments. The production application manifest digest is resealed
   with its note; the full-application and mutation-branch digests are
   unchanged. Rebased onto main e847156c3, the value also covers ca0aac0e9
   (BPR-2396), whose change to `window_agent_chat.cpp` and `tour_assist.cpp`
   was on main with the old pin; the note says so and says what was read.

Not in this card:

- The rest of RFC-BPR-0130 stage 1 that BPR-2384 left: the always-on
  revision chip in the Agent tab's header, the cost line under the composers
  and the item side chat's header.
- The tour side chat's "Head changed. This chat reads …" notice (stage 6
  replaces it with the pinned-revision card) and the one ending line
  (stage 3).
- "Transcript replay" as the mechanism's name: the store, the host's request
  field, the agent's input (`transcript_replay.*`) and the architecture
  documents keep it. `docs/architecture/ai-agent-boundary.md` says which name
  is whose.
- "Other tour chats and detached history", "Source history: *id*" in the
  notice's tooltip, and "inspect the native history" in one refusal, which
  the RFC's table does not name.

## Acceptance

- AC1. In no state of the right drawer does the Agent tab show a line between
  its header's row and its transcript; `agentContinuationMode` does not
  exist.
- AC2. A chat's transcript with a turn cut at its top draws its edge on its
  first row, over the cut turn; with only the blank room over its first turn
  scrolled away, or at its start, it draws none; the edge takes no room and
  no event. The Agent tab, the tour's side chat and an item's side chat each
  have one.
- AC3. Nothing that is not a scroll area's own is drawn inside its viewport,
  in every state and width of the composed course.
- AC4. A carried-over session and one the agent cannot pick up say so in the
  accepted words on the question field, naming the chosen agent, in the Agent
  tab and the tour's side chat; with two agents chosen in turn each sentence
  names the chosen one; every other session says nothing.
- AC5. History offers "Carry this conversation over to *agent*" and "Show
  every run…"; no label, button, menu item or tooltip of the drawer says
  "Native continuation", "Transcript replay", "Native:", "Private history and
  runs" or kibibytes.
- AC6. Every refusal, notice and tooltip that names the card's second choice
  says New session; the refused turn names exactly "Continue at *rev*" and
  "New session".
- AC7. Public path: the application's own window with the Fixture agent, the
  drawer dragged to 280, 300, 340 and 420 px and the window made as narrow as
  it goes, every state of the composed course.
- AC8. Format, tidy on touched units, documentation gate.

## Evidence

Revision `6edc4d90d` (rebased onto main e847156c3, which had gained
BPR-2396's move and reconcile prompt and BPR-2397's keyboard ring and the
course's `agent-answer-link-keyboard` state), macOS 26.6.2 arm64, Debug, Qt
6.12.1, `ctest --test-dir build/dev -R '^<row>$'`, GUI rows one at a time.
Everything below was first run before the rebase, on main 5bc62c938, at the
commits that are 69771bbd8 and 68a3aa60d now (8e23d5f02 and f7a34060c then). After it, in a new build tree, these were run again and
passed: `application_composed_drawer_fit_scale_100`, `_scale_200` and
`_cocoa` (29 states), the native hand runs in Dark and Light,
`application_agent_invocation_composition_contract` and
`_native_pinned_contract`, `application_agent_invocation_host`,
`application_review_round_trip_composition_contract`,
`qt_workbench_agent_send_refusal`, `qt_workbench_window_scale_100`,
`qt_drawer_surfaces_fit_scale_100`, `qt_transcript_edge`,
`qt_chat_history_menu`, `qt_tour_side_chat`, `architecture_boundaries`,
`test_network_isolation_manifest`, the documentation gate and the format
check; clang-tidy again on `transcript_edge.cpp`, the composed course,
`chat_history_menu.cpp`, `tour_side_chat.cpp`, `agent_invocation_host.cpp`,
`window_tours.cpp` and `transcript_edge_tests.cpp`.

After the rebase the course under Cocoa first stopped three times running
(the CTest row twice, a hand run once) at "the question is sent: What changed
in this push?", the Agent tab still showing the earlier head's card for the
session that follows the head (`cocoa-stop-after-rebase.log`). That is the
stop BPR-2393 recorded when another lane ran the same course at once; other
lanes were building and running here, at a load average of 45 to 85. The
next five Cocoa runs passed: two with the wait before that question timed
(the card went after 268 ms and 36 ms), then the hand runs in Dark and Light
and the CTest row on the committed course. The offscreen rows passed each
time. The cause was not found; see Owner judgement.

The rows not run again after the rebase are
`qt_workbench_window_scale_125` and `_200`,
`qt_drawer_surfaces_fit_scale_125` and `_200`, `qt_drawer_parts_scale_*`,
`qt_tour_side_chat_width_scale_*`, `qt_agent_session_menu`,
`qt_side_chat_panel`, `qt_side_chat_tab_and_send_scale_*`,
`qt_review_tour_agent_failure`, `qt_stick_to_bottom` and
`test_network_isolation_structure_contract`, and the other seven
`application_agent_invocation_*_contract` rows.

- AC2: `qt_transcript_edge` (new, `tests/adapters_qt/transcript_edge_tests.cpp`)
  passed. On a bare scroll area that follows its end: no edge on an empty
  transcript or one that fits, and the first turn drawn to the viewport's
  top; with four turns in a viewport that holds three and a bit, the edge is
  visible at the viewport's first row, one pixel tall and as wide as the
  viewport, every pixel of that row is the edge's colour (read from a grab,
  over a turn filled green), the viewport's geometry and the content's height
  are what they were, the edge lies over the viewport in the area's children,
  a press on it reaches what is under it and it takes no focus; back at the
  start it is hidden and the row is the turn's colour; one pixel off, shown;
  a wider area has a wider edge; a viewport that holds every turn has none.
  With eight pixels of room over the first turn: none at 0 and at 8 px off the
  start, shown at 9. A workbench window's Agent tab conversation and its
  tour side chat each have an edge and follow their end.
  `qt_stick_to_bottom` passed, with the item side chat's edge asserted.
- AC1, AC4, AC5, AC6: `qt_workbench_agent_send_refusal` and
  `qt_workbench_window_scale_100`, `_125` and `_200` passed (`_125` and
  `_200` before two locals of the test below were renamed for clang-tidy;
  the other two again after). In
  `test_a_pinned_conversation_card_offers_what_it_names`: no
  `agentContinuationMode` exists; an ordinary session's notice is hidden and
  empty and the transcript's top is the row under the header's row; a
  carried-over session shows exactly "Carried over from a saved transcript.
  Fixture agent has the questions and answers, not what the earlier agent
  read." with the tooltip "Long conversations are shortened: the oldest turns
  are left out. This one left out 3 older turns." and keeps
  `replayTurnsOmitted` 3; one the agent cannot pick up shows "Fixture agent
  can't pick up this conversation where it stopped. Choose New session, or
  carry the transcript over."; each in the quiet role, under the transcript
  and over the field, with nothing between header and transcript and no
  visible label saying "Native continuation", "Transcript replay", "KiB" or
  "Replay "; History's tooltip, "Carry this conversation over to Fixture
  agent" (`agentCarryOver`) and "Show every run…" (`agentShowEveryRun`); the
  refused turn holds "New session to ask at 2222222"; and the two send
  refusals' exact words.
- AC4, AC5: `qt_chat_history_menu` passed: the menu's object names; carry
  over names no agent, then Codex, then Claude Code as the state's agent
  changes; no item's text or tooltip says "Transcript replay", "Native",
  "Private history", "KiB" or "Replay "; the four shared sentences word for
  word, with and without an agent's name; the tooltip's count for 0, 1 and 7
  turns and cut text; a carried-over session that left 4 turns out says so.
  `qt_tour_side_chat` passed: the tour chat's notice (`tourSessionNotice`)
  says the carried-over sentence with "The agent", then Codex, then Claude
  Code as `set_agent_name` changes, History's item with it
  (`tourCarryOver`); the cannot-pick-up sentence names the agent and has no
  tooltip; a moved head leaves the notice empty and hidden.
  `qt_agent_session_menu` and `qt_tour_side_chat_width_scale_100`, `_125`
  and `_200` passed.
- AC6: `application_agent_invocation_host` passed: a used conversation's
  refusal holds "Choose Continue at *earlier*" and "or New session to ask at
  *current*" and no "tart a new session"; with edits in the way it ends ",
  or New session. No question was sent."; with the kept workspace on the same
  checkout it ends " Choose Continue at *earlier* or New session. No question
  was sent."; a retained-inspection refusal names neither.
- AC3, AC7 and all of the above in the application's own window:
  `application_composed_drawer_fit_scale_200` and `_cocoa` passed, and
  `_scale_100` passed on a
  run of its own. In the first run of the twelve course rows, with the disk
  this machine's agents share down to 333 MiB free, `_scale_100` stopped at
  "the renderer opens at a line" with the window's status "Review history is
  inactive. The local review-history limit was reached." and
  `application_agent_invocation_native_pinned_contract` stopped at
  `native-open files=1 agents=0`; run again with 4.6 GiB free, both passed
  (100 s and 35 s). Twenty-eight states then, twenty-nine after the
  rebase, no fault, with the four new
  checks at every state and width and their negative controls at the start
  of each run; the run reports how many transcripts it judged with a turn cut
  and the edge drawn (35 in the native runs). Run by hand under the native
  look (Cocoa, QMacStyle, nothing on screen, the display's own 200%), Dark
  and Light, on the rebased tree: each 29 states, 0 faults, 35 and 35
  transcript edges drawn, dpr 2, cocoa/macos
  (`after-6edc4d90d-dark/course-after-dark.log`,
  `after-6edc4d90d-light/course-after-light.log`).
- Negative control (a temporary edit, not committed; made before the
  rebase, on the product and course of what is now 69771bbd8): the Agent tab given
  back a line "Native continuation" between its header's row and its
  transcript, at the drawer's edge, and the Agent tab's and the tour chat's
  transcripts left without an edge. The course, under the native look in
  Dark, reached all 28 states and reported 100 fault lines in the 14 chat
  states and none in the other 14
  (`negative-control-dark/course-negative-dark.log`): in each of the six Agent
  tab states at each of five widths, "QScrollArea 'agentConversationScroll'
  keeps itself at its end and has no edge to draw over a turn cut at its top;
  16 px stand between the Agent tab's header row and its transcript" and
  "QLabel … "Native continuation" says "Native continuation""; in each of the
  eight tour chat states at each of five widths, the missing edge. It then
  ended on "the course judged a chat's transcript off its start, with its
  edge drawn", which no state met.
- The nine `application_agent_invocation_*_contract` rows: passed (eight in
  the first run, `native_pinned` in the run of its own above). They look
  the notice up as `session_notice()` and `tourSessionNotice`, wait for
  "Carried over from a saved transcript." where they waited for "Transcript
  replay", for "can't pick up this conversation where it stopped" where they
  waited for "Native continuation unavailable", for an empty hidden notice
  where the tour chat's read "Native", and press `agentCarryOver` and
  `tourCarryOver`.
- Around it: `qt_drawer_surfaces_fit_scale_100`, `_125` and `_200`,
  `qt_drawer_parts_scale_100`, `_125` and `_200`, `qt_side_chat_panel`,
  `qt_side_chat_tab_and_send_scale_100` and `_200` and
  `qt_review_tour_agent_failure` passed. `architecture_boundaries`,
  `test_network_isolation_structure_contract` and
  `test_network_isolation_manifest` passed.
  `application_review_round_trip_composition_contract` passed with the
  resealed production application manifest digest: `90de0e4d…` to
  `8004c90f…` before the rebase, and after it
  `970567aff70b107ea8f5af9ac8c8dbcfb51570995192597e183e9a4f60acf20c`, which
  also takes in ca0aac0e9 (BPR-2396's reconcile prompt read in
  `window_agent_chat.cpp` and `tour_assist.cpp`). That commit reached main
  without a reseal: main's own tree at e847156c3 computes `f68e6f6c…` against
  its pin of `90de0e4d…`, so this row's digest check cannot pass there
  (computed from an export of main; the row was not run on main). Full
  application `7ee88b4e…` and mutation branch `6a1c1ca2…` are unchanged, as
  `-DBPR_PRINT_PINNED_DIGESTS=ON` prints them. The last two rows first
  failed here for want of binaries a partial build had not made
  (`app_better_pull_review`, `gh_cli_git_smart_transport_tests`) and passed
  once those were built.
  `scripts/verify_documentation.py --root .` printed `DOCUMENTATION_OK`
  (with the main checkout's `implement-rfc/scripts` on `PYTHONPATH`).
- AC8: `format_check.py` (clang-format 22.1.8) passed on the 27 touched C++
  files. `tidy.py --warnings-as-errors` passed on `transcript_edge.cpp`,
  `chat_history_menu.cpp`, `tour_side_chat.cpp`, `item_side_chat.cpp`,
  `agent_session_menu.cpp`, `side_chat_panel.cpp`, `agent_launch_panel.cpp`,
  `agent_setup_view.cpp`, `agent_invocation_host.cpp`, `window_tours.cpp`,
  `transcript_edge_tests.cpp`, `composed_drawer_fit_public_path.cpp`,
  `chat_history_menu_tests.cpp`, `tour_side_chat_tests.cpp` and
  `stick_to_bottom_tests.cpp`. On `agent_tab_view.cpp`, `workbench_window_tests.cpp`,
  `drawer_surfaces_fit_tests.cpp`, `agent_invocation_host_tests.cpp` and
  `agent_invocation_composition_public_path.cpp` its only findings on lines
  this card changes were two locals of the new test block (`in_tab`,
  `shown_words`), renamed in 68a3aa60d; run again on that unit, neither is reported. The other findings are on
  lines this card does not change: `agent_tab_view.hpp:484` and
  `agent_tab_view.cpp:2742` (`press_offer`),
  `local_draft_workflow_area.hpp:72` (seen through the two Qt test units) and
  `agent_invocation_composition_public_path.cpp:2330`..

Renders, all by the composed course under the native look (Cocoa, QMacStyle,
windows kept off screen, the window told it is active, no `QT_SCALE_FACTOR`:
the display's own 200%), in
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/refused-card-renders/`:

- `side-by-side-dark-200-agent-pinned-refused-280.png`: the state the owner
  saw, main 5bc62c938 (the frozen build's own render) beside this change, a
  280 px drawer. `side-by-side-dark-200-agent-pinned-offered-280.png`: the
  carried-over session, its heading of three lines beside its notice on the
  question field. `side-by-side-dark-200-agent-answered-280.png`: an ordinary
  session, the heading beside none.
  `side-by-side-dark-200-agent-pinned-refused-top-of-thread-widths.png`: the
  top of the refused question's transcript at 280, 300, 340 and 420 px,
  before and after; at 340 and 420 the turn before is cut under the edge.
- `before-5bc62c938-dark/cocoa-macos/frozen-dark-200-<state>-<width>.png`:
  the frozen build's renders of the Agent tab's states and two tour chat
  states, copied from the look the owner judged.
- `after-6edc4d90d-dark/cocoa-macos/after-dark-200-<state>-<width>.png` and
  `after-6edc4d90d-light/cocoa-macos/after-light-200-<state>-<width>.png`:
  every state of the course at every width, and `…-window.png` for the whole
  1440 by 960 window at each state's narrowest, with the run's log beside
  them, from the rebased tree.
- `negative-control-dark/`: the negative control's log and renders
  (`cocoa-macos/negative-dark-200-agent-answered-280.png` shows the heading
  back).

## Owner judgement

- The look is the owner's to accept in a frozen tests-on build of the
  integrated revision with the Fixture agent, on a pull request whose head
  moves (the composed course's fixture, `bpr-fixtures/nested-folders#21`):
  the Agent tab after a refused question at 280 px, and a carried-over
  session's notice on the question field. No session was opened for it here,
  and no build was frozen. One question per criterion: the transcript under
  the header (no heading, the edge over a cut turn), and the carried-over
  notice's words and place.
- Choices made here that a Reject's note may change:
  - Where the Agent tab's notice stands: on the question field, under the
    transcript, as the tour chat's does (RFC-BPR-0130: "notices and pending
    messages" between the transcript and the composer). It was the header's
    second line. With the pinned-revision card under it, the notice takes
    three lines at 280 px and the thread keeps about 475 px of a 1440 by 960
    window.
  - "{agent} can't pick up this conversation where it stopped. Choose New
    session, or carry the transcript over." The owner accepted "Start a new
    session, or carry the transcript over."; the action is now named New
    session everywhere.
  - The carry-over tooltip is the RFC's one sentence in both chats. The tour
    chat's "Tour tool and network restrictions still apply." and "The source
    history stays." went with the limits; "This one left out *n* older turns."
    and "Some of its longest text was cut." are added only when true.
  - With no agent's name known, a sentence says "The agent" and the menu item
    "Carry this conversation over to the chosen agent".
  - The edge is a hairline in the cards' own edge colour, shown only while a
    turn is cut. It is not a permanent rule under the header: a transcript at
    its start has nothing over its first turn.
  - A refusal with two ways on reads "Choose Continue at b20cb44 or New
    session." It no longer says which head New session opens at; the sentence
    before it names both heads, and the chip's tooltip says it.
  - "The pull request has moved on. Choose Continue or New session, then
    send." names the first chip by its short words, since the send refusal
    does not hold the revision.
- A refused question's turn sits wherever the transcript's end puts it. At
  280 px in this fixture the cut now falls in the room over the question's
  time stamp, so the question is whole. At 340 and 420 px it falls in the turn
  before, under the edge (`side-by-side-dark-200-agent-pinned-refused-top-of-thread-widths.png`).
- The composed course's stop at "the question is sent: What changed in this
  push?" under Cocoa is intermittent and was not caused or cured here that I
  can show: three stops in a row after the rebase, then five passes, with
  nothing changed between but a timing print added and removed. If it recurs
  alone on an idle machine it is a race between opening the session that
  follows the head and the earlier head's card, in the course or in the
  product, and wants its own card.
- After a refused question the suggested prompts under the turn are enabled
  while the card waits on a choice; before a question (the same card) they
  are drawn disabled. What pressing one does there was not tested. Not
  changed here; the renders of main show the same.
