---
schema: "implement-rfc.card.v1"
id: "BPR-2384"
title: "The Agent tab and the tour side chat open one History menu: New session is its first item and is greyed with its reason when it would change nothing, session rows are lit under the pointer and the arrow keys, and the tour chat's header and buttons fit its narrowest column"
status: "Done"
rfcs: ["RFC-BPR-0130"]
depends_on: ["BPR-2377"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","reservation":"docs/issues/_reservations/BPR-2384--BPR-2384--4d530562edf7.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"f2f8ba711","origin":"owner, attended review 374cb080088e516421011d3a88d707e0, 2026-11-24 (verbatim below)","related_cards":["BPR-2377","BPR-2369","BPR-1995","BPR-2337"],"related_rfcs":["RFC-BPR-0130","RFC-BPR-0096","RFC-BPR-0100"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6"],"owner_step":"attended look at the History menu and the tour chat's header (see Owner judgement)"}
acceptance: []
---

# BPR-2384: One History menu for the Agent tab and the tour side chat

## Why

Owner, attended review `374cb080088e516421011d3a88d707e0`, 2026-11-24, on the
History menu's session rows (BPR-2377):

> there no highlight on the mouse over for the session rows. New session
> either needs to always work or get greyed out if it won't. Right now you
> click it and it does nothing which feels broken. You can tell it does
> nothing because if you rename the session it keeps its name.

and on the tour chat:

> What happened to my tour window? Those buttons are unreadable and thats not
> even the narrowest width. ... you must fix these other issues. It looks like
> tour built a different session selector from agent too for no reason.

RFC-BPR-0130 (header A, as the owner amended it) is the plan this follows:
"move new session into that menu as the top item so it doesn't take up extra
space on the drawer".

### What existed (main 30276994b)

- The session rows were plain widgets inside a `QWidgetAction`: the menu drew
  no highlight under them, the arrow keys skipped them, and their titles
  started left of the menu's own items.
- New session was three things: the Agent tab's pen icon, the side chat
  panel's "Start a new session" in History (enabled only after a moved head),
  and the tour chat's Session › New session. The store reuses a session
  nothing was asked in, so New session on one changed nothing, silently.
- The tour chat had its own **Session N** button under the composer with a
  near copy of the Agent tab's menu code. Its header wrapped the stop's title
  over five lines at 300 px; a suggestion's four buttons and a revision's
  three ran out of the column and the transcript panned sideways by up to
  80 px (`before-*` renders and logs).

## Change

1. **Core** `chat_history` (new): `decide_new_session` refuses New session
   with a reason when there is no agent, the chat's part is gone, the sessions
   are still read, a turn runs, the agent is busy with another part, or
   nothing was asked in the open session (unless the chosen agent cannot
   continue natively and the session is not yet one it can use).
   `plan_chat_history` lists the menu's items in order for the Agent tab and
   for the tour side chat; `chat_header_tool_for` says whether a header shows
   History or Copy.
2. **Qt** `chat_history_menu` (new): the one menu both chats open, built from
   core's plan. New session is its first item, a row that is greyed and says
   why under its name when core refuses; pressed anyway it sends nothing. Then
   the session rows, transcript replay, Delete all sessions…, the tour's other
   chats and Copy chat or the Agent tab's private runs and Reset agents…, and
   last the revision the open session reads. `menu_row` (new) has the style
   draw a row as it draws a menu item (highlight and tick), aligned to the
   items' words; rows are lit under the pointer and the arrow keys, Return
   opens, Left and Right reach the pencil, trash can and i.
3. **Agent tab**: the pen icon and the Sessions submenu are gone; the title
   ends in an ellipsis; "Start a new session" and "Continue against the old
   revision" leave History (the pinned-revision card keeps them).
4. **Tour side chat**: the Session button and its menu builder are gone; the
   header is one row (part and place, each one elided line; Show in code,
   which reads Show in the narrowest column; History; close); the agent glyph
   is gone; the suggestion and revision button rows and the foot wrap. A local
   draft's discussion, which keeps no sessions, shows Copy chat where History
   would be (`set_sessions_offered`, told by the composition).

Not in this card, left for RFC-BPR-0130's stage 1: the replacement words
("Transcript replay", "Native continuation", "Private history and runs"), the
revision chip and continuation heading in the Agent tab, the cost line under
the composers, and the item side chat's header.

## Acceptance

- AC1. A session row is lit under the pointer and under the arrow keys as the
  menu's own items are, one item at a time; Return opens it; the open session
  is ticked.
- AC2. New session is History's first item in both chats. Core decides
  whether it can be chosen; refused, it is greyed, says why, and sends
  nothing. Chosen on a named session with turns, it opens an empty unnamed
  session and keeps the named one.
- AC3. The Agent tab and the tour side chat open the same menu component; the
  tour chat has no session selector of its own.
- AC4. In every state the tour chat's column reaches, at 300, 340 and 420 px
  and at 100%, 125% and 200%, every button and label lies inside the column,
  no button is narrower than its words, none lies over another, and the
  transcript does not pan sideways.
- AC5. Public path: the real window with the Fixture agent.
- AC6. Format, tidy on touched units.

## Evidence

Revision `f2f8ba711`, macOS arm64 Debug, `ctest --test-dir build/dev -R
'^<row>$'`.

- AC1 presentation: `qt_agent_session_menu` passed (a row is lit under the
  pointer and is then the menu's active item; Down reaches the first row,
  Down the next, Right its pencil, Left back, Down past the last row is
  Delete all sessions…, Up returns to the last row, Return opens it and the
  menu closes; the open session's row is the ticked one; a click anywhere on
  a row outside its marks opens it).
- AC2 rules: `core_chat_history` passed in dev and in the `core-only` preset
  (the six refusals and their order; the unsupported-agent exception; each
  chat's items; no separator left by an empty group).
- AC2, AC3 presentation: `qt_chat_history_menu` passed (both chats' item
  order from one component; New session lit and sent once on a session with
  turns; greyed with "This session is new: nothing has been asked in it." on
  an empty one, a click and a trigger send nothing; a running turn, the
  sessions being read, no agent, the agent busy elsewhere and a part gone
  each grey it, the last four with their words; carry over, the rows, the
  private runs, Reset agents…, the other parts' chats and Copy chat reach
  their dispatch).
- AC2, AC3 in the tour chat: `qt_tour_side_chat` passed (no Session button;
  History in the one header row between Show in code and close; New session
  History's first item; greyed with its reason on a chat nothing was asked in
  and it asks the host nothing; Copy chat from History; a chat that keeps no
  sessions shows Copy in History's place).
- AC2, AC3 in the Agent tab: `qt_workbench_window_scale_100` passed (New
  session is History's first item and no button of the row; triggered, it
  reaches the host without a confirmation), `qt_button_faces` and
  `qt_button_faces_cocoa` passed, `qt_item_side_chat` passed.
- AC4: `qt_tour_side_chat_width_scale_100`, `_125` and `_200` passed (seven
  states at 300, 340 and 420 px). The same course run with the native look
  passed in light and dark at 100% and 200%. The course's first version, run
  on main 30276994b in those four ways, reported 13 such faults each time (a
  suggestion's question, text and Ask more outside the column at 300 and
  340 px, a revision's Copy outside it at 300 px, the transcript panning
  sideways by 80, 40 and 20 px; `before-*.log`). Those logs also hold 18
  lines from a size check on icon tools that the course no longer makes.
  `qt_drawer_surfaces_fit_scale_100`, `_125` and `_200` passed.
- AC5 Agent tab: `application_native_agent_sessions` passed, with
  `AGENT_SESSION_MANAGEMENT:renamed:restored:deleted-shared:workspace-kept:agent-files-kept`
  and the new
  `AGENT_NEW_SESSION:named-session-kept:empty-unnamed-opened:greyed-when-new:asks-nothing`,
  which the course now requires (Fixture agent through the real window: a
  session with turns is renamed, New session opens an empty unnamed session
  and the title returns to the pull request's, the named session stays in
  History with its turns and name, and on the new session New session is
  greyed with its reason and asks nothing). Its tour course checks the same
  greying in a stop's chat, and its replay course on an agent that cannot
  continue natively.
- AC5 tour chat: `application_tour_chat_queue_stop_public_path`,
  `application_tour_chat_queue_whole_tour_public_path` and
  `application_tour_chat_queue_local_draft_public_path` passed;
  `application_local_draft_tour_desktop_public_path` passed.
- `application_setup_destination_public_path` passed (the window's word rules
  over the new text); `application_agent_finding_composition_contract`
  passed; `application_review_round_trip_composition_contract` passed with
  the production app manifest resealed over the two `set_sessions_offered`
  calls (full application and mutation-branch digests unchanged);
  `test_network_isolation_manifest`, `test_network_isolation_structure_contract`
  and `core_agent_session_management` passed.
- AC6: `format_check.py` (clang-format 22.1.8) passed on every touched C++
  file. `tidy.py --warnings-as-errors` over the touched
  translation units reports nothing on a changed line; its two findings are
  in `agent_tab_view` at lines last changed by 0dcfce828.

Not passing in this tree, and not from this change: `architecture_boundaries`
fails in dev and core-only with one violation, "Forbidden installed-Git
process surface", on `"checkout"` and `"diff"` in
`src/adapters/git/src/installed_git_process.cpp`, which this card does not
touch (the branch has no change under `src/adapters/git`, `cmake/` or
`scripts/`); the run prints "Application, provider, and Qt architecture
boundaries verified" before it. `scripts/verify_documentation.py --root .`
stops on `docs/issues/NEXT_ID: next_id: BPR-2381 rewinds historical high
water BPR-2384`, the lane's tracker state behind main's.

Renders, native look (Cocoa, QMacStyle, nothing on screen), light and dark,
100% and 200%, in
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/tour-chat-renders/`:
`before-<scheme>-<scale>-<state>-<width>.png` and
`after-<scheme>-<scale>-<state>-<width>.png` for the states stop,
stop-moved-head, stop-replay, stop-queue, stop-suggestion, tour-revision and
tour-plan-check at 300, 340 and 420 px, each with its History menu
(`…-history-menu.png`), and `after-menus-<scheme>-<scale>/` with the lit row,
the Agent tab's History, New session refused, and the tour chat's History.

## Owner judgement

- The look of the History menu and the tour chat's header is the owner's to
  accept; the renders above are what a frozen tests-on build would show with
  the Fixture agent.
- New session on a session nothing was asked in is greyed rather than made to
  open a second empty session: "always work" would pile up empty sessions.
- The words RFC-BPR-0130 accepted are not applied here; this card keeps
  today's words so the menu and the notices still agree.
