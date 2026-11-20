---
schema: "implement-rfc.card.v1"
id: "BPR-2337"
title: "The item side chat belongs to its tab, and both side chats place Send as the Agent tab does"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","defect":true,"target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2337--BPR-2337--1c14a173601d.json","origin":"owner's attended review b64bbc238073ff3b665048b611ef1c60 (frozen from main d802ef303), Reject on BPR-2333's side chat","related_cards":["BPR-2322","BPR-2333","BPR-1925","BPR-1995"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6"]}
---

# BPR-2337: The item side chat belongs to its tab, and both side chats place Send as the Agent tab does

## The owner's words

Attended review session `b64bbc238073ff3b665048b611ef1c60`, frozen from main
`d802ef303`, rejected [[BPR-2333]]'s side chat:

"side chat is staying open when i switch off that tab. it should be part of
that tab. The send button on both the side chats (report and tour) should be
positioned more like the agent tab."

The column is one widget on the comparison page, so it stayed beside
whichever document tab came to the front. In the report's and the tour's
side chats Send stood outside the composer's field, to its right, with the
word "Send"; the Agent tab's Send is the arrow at the right end of a row
under a thin rule inside the field.

## Change

### The chat belongs to its tab

- Opening the column (`item_side_chat::set_column_open(true)`) tells the
  window, which records the active document tab as the chat's
  (`workbench_window::adopt_side_chat_tab`, `item_side_chat_tab()`). Opened
  where it does not show (another page in front, or no tab yet, as when the
  Agent tab's Ask follow-up brings up the code), the next tab made active,
  the one its opening brings up, is its own.
- Each tab projection (`workbench_window::publish(document_tabs_projection)`)
  places it (`place_item_side_chat`): with its tab active it shows; with
  another active it hides (`item_side_chat::set_tab_shown(false)`) and stays
  open, keeping its chat, the words being written and its width, and nothing
  is closed; with its tab gone it closes, once, through the column's own
  close, so the host keeps the item's chat as Close does ([[BPR-2322]] AC5).
- Asked about again from another tab, the chat is that tab's. Close closes it
  and it belongs to no tab.

### Send stands as the Agent tab's

A shared private unit, `src/adapters/qt/src/composer_parts.{hpp,cpp}`, holds
what the Agent tab's composer was built from: the field's insets (8, 6, 6,
6), the thin rule under the words (moved out of `agent_tab_view.cpp`), the
row under it (`add_send_row`), and Send (`make_send_button`: the default
button, the send arrow, no words, named "Send"). The Agent tab now builds its
composer from these; the item's side chat and the tour's build theirs from
the same parts: Send ends the row under the rule inside the field, and the
tour's Stop (shown while a follow-up queues) stands just before it. The
item chat's Send says "Stop" in words while it answers, as the Agent tab's
does; the tour's keeps Stop, Queue and Steer. Nothing in `src/app` changed,
so the composition digests are untouched.

## Acceptance

- AC1. Opened beside the report, the chat is the report tab's; another tab
  hides it without closing it; its tab again shows it with its width, its
  item and the words being written.
- AC2. Opened from another tab, it is that tab's; the report's tab no longer
  shows it.
- AC3. Closing its tab closes it once; a later tab does not bring it back;
  its own Close closes it and it belongs to no tab; a closed chat stays
  closed across tab switches.
- AC4. Opened with no tab active, the next active tab is its own.
- AC5. In the report's side chat and the tour's, Send is in the composer's
  field under its rule, the arrow with no words, the Agent tab's size, at
  the Agent tab's right inset, distance under the rule and bottom inset, at
  1.0 and 2.0 scale.
- AC6. The side chats' existing tests, the follow-up public path,
  `architecture_boundaries`, the format check and the documentation gate
  pass.

## Decisions

- The frontend-design skill was loaded for the composer. The look is the
  Agent tab's, unchanged: the change is placement and one shared source, not
  a new design.
- Ownership is the Qt window's: which tab a column sits beside is
  presentation. The column opens, hides and closes through its existing
  intents; the host sees a close only when the column's tab closes.
- What is shared: the field's insets, the rule, the send row and Send.
  The field's own drawing stays each surface's (`rounded` in the Agent tab,
  `composer_field` with its focus ring in the side chats); the owner asked
  about Send.
- A chat whose tab is hidden still receives its projections: an answer that
  arrives while another tab is in front is there when its tab returns.

## Verification

macOS arm64, dev preset (Debug with `-gline-tables-only`), a task tree of
its own on main `cb8a83eee` plus this change. `format_check.py`
(clang-format 22.1.8) on every changed C++ file.

| Criterion | Check | Result |
|---|---|---|
| AC1–AC4 | `qt_side_chat_tab_and_send_scale_100/200` (`test_the_side_chat_belongs_to_its_tab`, in a `workbench_window` publishing document tab projections) | pass |
| AC5 | `qt_side_chat_tab_and_send_scale_100/200` (`test_send_stands_as_the_agent_tabs`: all three 43x29, right 6, 4 under the rule, bottom 6, at both scales); `qt_tour_side_chat` (Send in the composer's field; under the transcript only the field and the session line) | pass |
| AC6 | `qt_item_side_chat`, `qt_tour_side_chat`, `qt_side_chat_panel`, `qt_tour_agent_session_view`, `qt_review_tour_agent_help`, `qt_review_tour_agent_setup_call`, `qt_agent_footer`, `qt_button_faces`, `qt_agent_report_view`, `qt_workbench_window_scale_100`, `application_follow_up_public_path`, `application_review_report_public_path`, `application_fixture_agent_guard_public_path`; `architecture_boundaries`; `format_check.py`; `tidy.py` on every changed `.cpp` (no finding on a changed line; three earlier findings at `agent_tab_view.hpp:422`, `agent_tab_view.cpp:2602` and `workbench_window_comments_and_threads.cpp:2067` predate this); `python3 scripts/verify_documentation.py --root .` (`DOCUMENTATION_OK`) | pass |

Looks: offscreen renders of the Agent tab's composer and both side chats at
2.0 (`BPR_SIDE_CHAT_SEND_RENDER`), viewed and discarded. No owner look was
prepared in this round; the owner's judgement of either change is not
recorded here.

Not run: Windows and Linux, `qt_button_faces_cocoa` (it draws on the
desktop), a full `ctest`. `application_review_round_trip_composition_contract`
was not run: no `src/app` source changed, so its digests are unaffected.
