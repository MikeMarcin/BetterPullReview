---
schema: "implement-rfc.card.v1"
id: "BPR-2404"
title: "Every chat with an agent wears one header row decided in core: the Agent tab's revision chip and the composers' cost lines are gone, the item side chat's header is one row with Copy conversation and no agent glyph, and History names its items in the accepted words"
status: "Done"
rfcs: ["RFC-BPR-0130"]
depends_on: ["BPR-2384", "BPR-2400", "BPR-2402"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","reservation":"docs/issues/_reservations/BPR-2404--BPR-2404--969be94c3814.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"fbb7053c8","origin":"RFC-BPR-0130 stage 1 (draft cards DRAFT-0001, DRAFT-0002 and DRAFT-0003 of the attended review 3cd822672efb50572b10ab10747889ba); owner, 2026-11-24: header A as amended, the words with {agent}, one chat component","related_cards":["BPR-2384","BPR-2400","BPR-2402","BPR-2393"],"related_rfcs":["RFC-BPR-0130"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6"],"owner_step":"the frozen look of the header (RFC-BPR-0130 AC09, draft card DRAFT-0004) is still to be asked"}
acceptance: []
---

# BPR-2404: One header row for every chat, decided in core

## Why

Owner, 2026-11-24:

> We might need to clean up the top controls on the agent windows. We should
> also more closely reuse implementation between the side chats and agent chat
> so that we're not constantly having to double implement changes.

The owner chose header A in attended review
`3cd822672efb50572b10ab10747889ba`, amended: "A but move new session into
that menu as the top item so it doesn't take up extra space on the drawer
... Put the cost and turns in a info tooltip". RFC-BPR-0130 stage 1 is that
header in all three chats. BPR-2384 landed the one History menu and the tour
chat's one-row header; BPR-2400 landed the accepted words early. Left for
this card (their "Not in this card"): the Agent tab's always-on revision chip,
the cost line under the Agent tab's and the tour chat's composers, the item
side chat's header, and the header as one core value worn by one widget.

### What was there (main b6290b9ae)

- Each chat built its own row: `agent_tab_view` (title, a seven-character
  revision chip "b20cb44", History), `tour_side_chat` (its own eliding row,
  its own fit for Show in code), `item_side_chat` (a copy of the tour chat's
  older row: the agent glyph, the subject "Suggested comment at
  renderer.cpp:12–13" wrapping, the claim under it). Which tool each showed
  was the widget's `setVisible`.
- "$0.00 this session" under the Agent tab's composer and the tour chat's.
- The item chat's header wrapped over five lines in a 280 px column
  (`stage-1-before-item-chat/…/before-light-200-item-chat-answered-280.png`).

## Change

1. **Core** `chat_header` (new): `chat_kind` (Agent tab, tour part, whole
   tour, local draft discussion, review item); `chat_actions_offered` lists
   what each kind offers from its row and its History menu, in order;
   `plan_chat_header` turns what a chat observed (subject, place, whether it
   can be shown, the claim, the open session's name and revision) into the
   row: title, title note, place, Show ("Show" or "Show in code"), the one
   conversation tool (History where the chat keeps sessions, Copy
   conversation where it keeps none), Close. No kind offers a New session
   button. The accepted words are produced here, `{agent}` substituted
   ("The agent" while none is known): `chat_session_notice`,
   `chat_carry_over_words`, `chat_carry_over_tip`,
   `chat_revision_read_words`, `chat_revision_words` (seven characters).
   `plan_chat_history` reads the kinds' offers from it; the Agent tab's
   History gains Copy conversation; `chat_header_tool_for` is gone.
2. **Qt** `chat_header_view` (new): draws a `core::chat_header` and sends
   gestures (Show, Copy conversation, Close); the chat hangs its
   `chat_history_menu` on the History tool. The title and the place each keep
   one line, end in an ellipsis and are whole in their tooltips; Show in code
   reads Show where the row is too narrow. `chat_history_menu`'s words are
   core's; "Other tour chats and detached history" reads "Chats about other
   parts of the tour", "Copy chat" reads "Copy conversation".
3. **Agent tab**: wears the row (the pull request's title, or the open
   session's name with the pull request in its tooltip, and History). The
   revision chip (`agentHeadChip`) and the cost line (`agentSessionCost`) are
   gone; the revision read is History's last line and the pinned-revision
   card's; `present_session_cost` still feeds the footer. History's Copy
   conversation copies the whole thread (`chat_text`).
4. **Tour side chat** (and a local draft's discussion): wears the row; its
   own row, `fit_header` and its cost line (`tourSideChatSessionCost`) are
   gone. A discussion keeps no sessions, so Copy conversation is its tool and
   its tooltip says what the conversation has cost.
5. **Item side chat**: wears the row: the kind of item ("Suggested comment",
   "Your draft", "Conversation") over its file and lines, Show, Copy
   conversation, close; the claim is the subject's tooltip; no agent glyph.
   The column keeps its width handle and its document tab.
6. **Courses**: `qt_chat_header_view` (new, every kind at 280 to 520 px);
   `core_chat_header` (new); `qt_drawer_surfaces_fit` holds the Agent tab's
   long title to one elided row with History in every theme at every width
   and scale; the composed drawer course gains `item-chat-answered` (a
   finding's Ask follow-up opens the item's chat beside the code, it is
   answered, the column is given 260, 280, 300, 340 and 420 px and the window
   made as narrow as it goes) and `header_faults`: a chat whose header is not
   the shared row, a replaced control still shown (`agentHeadChip`,
   `agentSessionCost`, `tourSideChatSessionCost`, `itemSideChatGlyph`,
   `tourSideChatGlyph`, `agentClearSession`), a title on more than one line
   or cut without its ellipsis and tooltip, a part outside its row, a New
   session button in a row. Negative controls in `check_the_header`.
7. **Docs**: `docs/ux/workbench-specification.md`,
   `docs/ux/shared-action-buttons.md`, `docs/architecture/review-tours.md`,
   `src/adapters/qt/AGENTS.md`.

No `src/app` file changes; the sealed production manifest is untouched.

Not in this card: one composer (stage 2), the transcript (stages 3 to 5),
one notice (stage 6, which also replaces the tour chat's "Head changed"
notice), the item chat on the session runner (stage 7, open question 4),
Reset agents… (it stays last in the Agent tab's History, open question 2),
the drawer's own tools (open question 3).

## Acceptance

- AC1 (RFC AC01). In the Agent tab, the tour side chat and the item side chat
  the header is one `chat_header_view` row from one `core::chat_header`: what
  the chat is about, History where it keeps sessions, Copy where it keeps
  none, Close where the column closes; no row holds a New session button.
- AC2 (RFC AC02). History opens with New session; the session rows (BPR-2377,
  BPR-2384) follow; Delete all sessions…; no "Rename or delete sessions…";
  the last line names the revision read.
- AC3 (RFC AC03). No chat shows "Native continuation", "Transcript replay" or
  "Native:"; each sentence that names an agent names the one passed in.
- AC4 (RFC AC04). Every header state fits the narrowest drawer and column at
  100%, 125% and 200%; a title too long for its row ends in an ellipsis with
  the whole title in its tooltip.
- AC5. No revision chip and no cost line under any composer; a session's
  cost is its History row's i and the footer's total.
- AC6. Public path in the composed window with the Fixture agent, native
  look; format and tidy on touched units.

## Evidence

Revision `fbb7053c8`, macOS arm64 Debug, `ctest --test-dir build/dev -R '^<row>$'`.

- AC1, AC3 rules: `core_chat_header` and `core_chat_history` passed in dev
  and in the `core-only` preset (each kind's offers in order; an item's chat
  Copy and no History; two different agent names substituted; an ordinary
  session says no continuation sentence).
- AC1, AC4 widget: `qt_chat_header_view` passed (Agent tab row at 280, 300,
  340, 420 px: title and History only, cut with an ellipsis and whole in its
  tooltip, one line; a named session's title and tooltip; a tour part's row
  left to right with Show in code reading Show at 300 and whole at 520; the
  whole tour; an item and a local draft with Copy and no History; no New
  session button in any).
- AC1, AC2, AC5 in each chat: `qt_item_side_chat`, `qt_tour_side_chat`,
  `qt_chat_history_menu` (Agent tab's History order with Copy conversation),
  `qt_workbench_window_scale_100` (no chip, no cost line; the total still
  kept) passed.
- AC4: `qt_drawer_surfaces_fit_scale_100`, `_125`, `_200` passed with the
  new header check (four themes; 280, 300, 340 and 760 px).
  `qt_tour_side_chat_width_scale_100`, `_125`, `_200` passed on the stage-2
  tree above this revision (stage 2 leaves the header as it is).
  `qt_button_faces`, `qt_button_faces_cocoa`,
  `qt_side_chat_tab_and_send_scale_100`, `_200` passed.
- AC6: `application_composed_drawer_fit_scale_100` and `_200` passed (39
  states). The same course under the native look (Cocoa, kept off screen,
  light, the display's 200%): "composed-drawer-fit: 39 states, 0 faults, 50
  transcript edges drawn, dpr 2, cocoa/macos". The same course built with
  main's `item_side_chat` reported six faults, one each width, "QLabel
  'itemSideChatGlyph' is shown, which the one header replaced".
  `application_native_agent_sessions`,
  `application_agent_chat_composition_contract`,
  `application_setup_destination_public_path`,
  `application_local_draft_tour_desktop_public_path` (the discussion's cost
  on Copy conversation), `application_tour_chat_queue_stop_public_path`,
  `_whole_tour_public_path`, `_local_draft_public_path`,
  `architecture_boundaries` passed.
- `scripts/verify_documentation.py --root .` (DOCUMENTATION_OK) and
  `tests/scripts/verify_documentation_tests.py` passed.
- Format: `_coding-standard/scripts/format.py` (clang-format 22.1.8) over
  every touched C++ file. Tidy (`tidy.py`) over `chat_header.cpp`,
  `chat_history.cpp`, `chat_header_view.cpp`, `chat_history_menu.cpp`,
  `item_side_chat.cpp`, `tour_side_chat.cpp`, `agent_tab_view.cpp` and the
  two new tests: the one finding on a changed line was fixed; two remain in
  `agent_tab_view` on lines this card does not touch (`press_offer`'s
  parameter names, a copy at line 2742).

Renders, native look, light, 200%, under
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/rfc130-renders/`:
`stage-1-before/cocoa-macos/before-light-200-<state>-<width>.png` (main),
`stage-1-after/cocoa-macos/after-light-200-<state>-<width>.png`, and
`stage-1-before-item-chat/cocoa-macos/` (the item's chat as main draws it,
in the same course).

## Owner judgement

- RFC-BPR-0130 AC09 (draft card DRAFT-0004): the frozen look of the header
  and its menu on a build that opens on a pull request with a tour, a report
  and two sessions, with the Fixture agent, is still to be asked.
- Decisions made here: a chat without sessions (a local draft's discussion)
  says its cost in its Copy conversation tooltip, having no History row; the
  item chat's place names the file without its folders, as its chip does.
