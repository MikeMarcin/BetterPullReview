---
schema: "implement-rfc.card.v1"
id: "BPR-2410"
title: "The Agent tab's verdict, report row, Draft all and finding cards are attachments on a turn: the projector names them, the tab's factory makes them, the card look places them"
status: "Done"
rfcs: ["RFC-BPR-0130"]
depends_on: ["BPR-2409"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","reservation":"docs/issues/_reservations/BPR-2410--BPR-2410--fafad0d06b51.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"1c4dc7206","origin":"RFC-BPR-0130 stage 5 (draft card DRAFT-0010 of the attended review 3cd822672efb50572b10ab10747889ba); owner, 2026-11-24: one chat component for all three chats","related_cards":["BPR-2409","BPR-2407","BPR-2061","BPR-2166","BPR-2236","BPR-2331"],"related_rfcs":["RFC-BPR-0130"],"acceptance":["AC1","AC2","AC3","AC4"]}
acceptance: []
---

# BPR-2410: The Agent tab's report and finding cards are attachments on a turn

## Why

Owner, 2026-11-24, accepting one chat component (attended review
`3cd822672efb50572b10ab10747889ba`): "We should also more closely reuse
implementation between the side chats and agent chat so that we're not
constantly having to double implement changes." RFC-BPR-0130 stage 5, second
card (DRAFT-0010): what the Agent tab hangs on a turn is an attachment, as it
is in the side chats. Visible: nothing.

### What was there (stage 5a, BPR-2409)

- The card look's `DressCard` made every one of the tab's parts of a turn at
  once, hidden: the fixed-since note, the moved notice, the claim and the
  verdict over the reply; the report's row, Draft all and an empty column for
  the finding cards under it. The tab then showed or hid each as the turn
  went, and pushed card widgets into the column itself (`CardColumn`).
- `core::project_agent_thread` hung no attachment on a turn; the card look
  drew none.

## Change

1. **Core** (`chat_transcript.hpp`): four attachment payloads, the Agent
   tab's: `chat_agent_verdict` (the verdict as the report words it and its
   tone), `chat_agent_report` (what the row names the report, its sections),
   `chat_agent_draft_all`, `chat_agent_findings` (the turn's cards by the
   tab's identity of each, in order); and `chat_attachment_place::before_reply`.
   `agent_thread_entry` carries the report, the verdict and the cards; the
   projector hangs, in reading order, `verdict` over the reply, then under it
   `report`, and while the turn has cards `draft-all` and `findings`.
2. **Qt, the card look** (`chat_transcript_view`): attachments stand in the
   reply's card after the chat's own parts in their place (over the reply,
   under it before the ending line, after what the run did); those a turn
   holds when first drawn are made with its parts, in reading order; an equal
   attachment keeps its widget, a changed one is made again alone, one the
   turn no longer holds goes alone. One made after its turn is put in the
   focus order where it stands (`focus_before`: Qt moves one widget at a
   time and orders only widgets that take focus), so Tab goes from the reply
   through the report and the cards to Copy. The side chats' look, which has
   no place over the reply, draws a `before_reply` attachment under it.
3. **Agent tab**: `dress_card` keeps the notes over the answer; the verdict,
   the report row (with its summary's counts), Draft all and the cards are
   made by its factory (`make_attachment`) from the turn's record. The cards
   stand in one holder (`agentTurnFindings`, 8 between cards, as the column
   was), made again from their records whenever the turn's cards change; a
   card made again keeps its identity (`make_card` names a card only once,
   and a card's identity is given when its record is made). The holder hides
   while every card in it is hidden (a report holding them all), or the card
   would take its spacing (6 px) under the reply. Once shown it and the reply lay themselves out at once (`show_turn_cards` activates both layouts): a level of layout more, the cards otherwise settle one event round later than they did in the reply's column, which `qt_workbench_window_scale_100` caught (a card squeezed to 51 px after its four rounds). `show_answer` keeps the
   report's title and sections on the record, `place_report_findings` the
   counts, `mark_open_report` which report is open, so a row made again reads
   as before. A deleted turn's cards leave and the later turns' cards move
   down a place before the transcript is drawn again.
4. **Courses**: `core_chat_transcript` (the tab hangs nothing on a plain
   answer; a report with cards hangs the verdict over the reply and the row,
   Draft all and the cards under it, in order, with their payloads; one
   change, naming that turn; a card more changes the cards alone; cards
   without a report hang Draft all and the cards alone);
   `qt_chat_transcript_view` (the card look: placement over and under the
   reply after the chat's own parts, each attachment made once, Tab from the
   report through the cards to Copy for attachments made after their turn and
   for those made with it, a changed attachment made again alone, a dropped
   one gone alone).
5. **Docs**: `src/adapters/qt/AGENTS.md` (the Agent tab's attachments, the
   holder that hides with its cards, the focus order).

No `src/app` file changes; the sealed production manifest is untouched.

Not in this card: the live turn's reducer in core and the borrowed controls
(DRAFT-0011).

## Acceptance

- AC1 (RFC AC06 for the Agent tab, DRAFT-0010). The verdict, the report row,
  Draft all and the finding cards are attachments the projector names and
  the tab's factory makes; the card look places them.
- AC2 (DRAFT-0010). Cards kept across a restart, another pull request and
  another agent come back as before.
- AC3. A turn's other attachments keep their widgets when its cards change;
  Tab reads them in the order they stand.
- AC4. The Agent tab's tests and the composed course in the native look pass
  with every render byte-identical; format and tidy on touched units.

## Evidence

Revision `1c4dc7206` (on main `83ecfd19e`), macOS arm64 Debug,
`ctest --test-dir build/dev -R '^<row>$'`.

- AC1, AC3: `core_chat_transcript`, `qt_chat_transcript_view` passed.
- AC2: `qt_agent_finding_cards_restart`, `qt_agent_finding_cards_retained`,
  `qt_agent_restored_byline`, `application_agent_chat_composition_contract`
  passed.
- AC4: the 61 rows of BPR-2409's AC4 list (the Agent tab's Qt and end-to-end courses, `qt_workbench_window_scale_100` and `_200`, the side chats, the drawer-fit and composed courses at 100% and 200%) passed. The composed course under the native look (Cocoa, kept
  off screen, light, the display's 200%): "composed-drawer-fit: 39 states, 0
  faults, 50 transcript edges drawn, dpr 2, cocoa/macos".
- Format: `_coding-standard/scripts/format.py` (clang-format 22) over every
  touched C++ file. Tidy (`tidy.py --warnings-as-errors`) over
  `chat_transcript.cpp`, `chat_transcript_view.cpp`, `agent_tab_view.cpp` and
  both tests: clean but for the one finding older than this card
  (`performance-unnecessary-copy-initialization` on `press_offer`'s copy of
  `aChosen`, left as BPR-2409 left it).

Renders, native look, light, 200%, under
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/rfc130-renders/`:
before is `stage-5a-after/cocoa-macos/`, after is `stage-5b-after/cocoa-macos/`
(`after-light-200-<state>-<width>.png`). All 274 renders are byte-identical,
the Agent tab's report and finding-card states (`agent-report-*`,
`agent-finding-cards-open-*`, `agent-finding-card-dismissed-*`) among them.

## Owner judgement

- None asked: nothing changes on screen.
