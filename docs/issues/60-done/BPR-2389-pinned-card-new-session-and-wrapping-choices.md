---
schema: "implement-rfc.card.v1"
id: "BPR-2389"
title: "Start a new session on the card of a session that read an earlier head opens an empty session at the current head at once, and the card's choices wrap and fit every drawer width"
status: "Done"
rfcs: ["RFC-BPR-0130", "RFC-BPR-0096"]
depends_on: ["BPR-2379", "BPR-2384"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","reservation":"docs/issues/_reservations/BPR-2389--BPR-2389--45edbc400886.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"95fae79da","origin":"found preparing attended review 5627b21a56838de9fae9f2ac4e4349d4 with a probe on application_agent_invocation_native_pinned_contract, 2026-11-24; the owner's earlier words on New session are quoted below","related_cards":["BPR-2379","BPR-2384","BPR-2377"],"related_rfcs":["RFC-BPR-0130","RFC-BPR-0096"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6"],"owner_step":"attended look at the card at 280, 300 and 340 px (see Owner judgement)"}
acceptance: []
---

# BPR-2389: Start a new session on the pinned-revision card opens it at once, and the card fits

## Why

Owner, attended review `374cb080088e516421011d3a88d707e0`, 2026-11-24, on
History's New session (BPR-2384):

> New session either needs to always work or get greyed out if it won't.
> Right now you click it and it does nothing which feels broken.

Two defects of the same card were found while preparing attended review
`5627b21a56838de9fae9f2ac4e4349d4`, by a probe that walked the path on the
pinned course:

1. A transcript replay opened after a push shows the card "This session read
   *rev*". **Start a new session** hid the card and changed nothing else: the
   replay's transcript, its name in the title and its tick in History stayed
   under the new head, and no new session existed until a question was sent
   (probe: `start-new ... selected=<the replay> turns=1 mode=Transcript
   replay`).
2. The card's second button, "Continue against the old revision", was cut
   off in a 300 px drawer. The card's two buttons sat on one unwrapped row,
   so the card was wider than the drawer and its right end was not drawn.
   The stopped card had the same row: at 280 and 300 px "Start a new
   session" was cut after "Continue at *rev*", and "Keep my changes on a
   branch and move" alone is wider than a 280 px drawer's column.

### What existed (main 10ee349ff)

- The card's Start a new session sent the chat's continuation choice. The
  composition started a provisional chat session and remembered that the next
  turn starts fresh; the native session store, which the tab's transcript,
  title and History read, was not asked until that turn.
- Core's `decide_new_session` called any session nothing was asked in "the
  new session", pinned to another head or not, so History's New session could
  be greyed on a session whose card offered a new one.
- The card's choices were a `QHBoxLayout`; a push button cannot be narrower
  than its words, so the row forced the card's width.
- Under the macOS style the reconcile row's native "Rebase onto *rev*" was
  laid out by its bezel inside the row's own widget, came out 28 px tall
  where the style draws 32, and was drawn as a square button.
- After a refused question's card, the pre-question card's second button
  kept the agent accent though it starts no run.
- The store took up an unused empty replay (the new session of an agent that
  cannot continue natively) whatever revision it read, so a new session asked
  for after the head moved would have been the old one again.

## Change

1. **Core** `chat_history`: `chat_history_facts::Pinned` (the open session is
   pinned to a revision other than the one under review and the chat asks
   which to go on with). `decide_new_session` never calls such a session
   `already_new`: New session opens one at the revision under review. It is
   the one rule for History's New session and the card's Start a new session.
2. **Composition**: the continuation choice `start_new_session` also opens
   History's new session (`open_new_agent_session`, the History item's own
   route): the selected agent's store starts an empty session at the revision
   under review and the tab is shown it, with nothing sent. The conversation
   left behind is kept in History.
3. **Store**: an unused empty replay is taken up again only at the revision
   it reads.
4. **Qt**, the card (`agent_tab_view`): its two choices sit on a
   `wrapping_row`. Before a question it says **Start a new session** and
   **Continue at *rev*** (RFC-BPR-0130's words; the stopped card's already),
   each with a tooltip saying what it does. Start a new session is enabled
   exactly when History's New session is (core's plan, read as the card is
   shown) and says core's reason when it is not. The second button and the
   reconcile row's two quiet choices wrap their words; "Rebase onto *rev*" is
   laid out by its widget; the pre-question card's choices wear the ordinary
   roles.
5. **Qt**, `quiet_push_button::set_word_wrap`: a chip without an icon whose
   words are a sentence takes further lines where it is given less than their
   width (its hint stays the one line, its minimum is its longest word,
   height for width). `button_surface` measures and draws the wrapped words.

Not in this card: the tour side chat's own "Head changed" notice, which
RFC-BPR-0130 stage 6 replaces with this card; the stopped card's Start a new
session, which resends the kept message and so already shows a turn.

## Acceptance

- AC1. On the card of a replay opened after the head moved, Start a new
  session shows, at once and with no process started, an empty unnamed
  session at the head under review: no transcript, the pull request's title,
  no "Transcript replay" line, the new head in the header, the new session
  ticked in History. The replay stays in History with its name and turns, and
  opened again offers its card again.
- AC2. Core decides: a session pinned to another revision is never refused
  New session as "already new"; a running turn or sessions still being read
  refuse it as before.
- AC3. In its three states (offered before a question, a refused question,
  work in the way of the move) at 280, 300, 340 and 760 px, in the four
  themes and at 100%, 125% and 200%: every choice lies inside the card, none
  is narrower or shorter than its words need, none lies over another, and the
  conversation is no wider than the drawer's column.
- AC4. The card says "Continue at *rev*" before a question as it does after a
  refused one.
- AC5. Public path: the real window with the Fixture agent.
- AC6. Format, tidy on touched units.

## Evidence

Revision `95fae79da` (rebased on main 6c985ba45), macOS arm64 Debug, `ctest
--test-dir build/dev -R '^<row>$'`.

- AC1, AC4, AC5: `application_agent_invocation_native_pinned_contract`
  passed. The course names the last replay "Kept replay", moves the head,
  opens the replay (card: "Start a new session" enabled, "Continue at
  <earlier head>"), presses Start a new session and requires, with no process
  started: the tab idle and the card gone, a session selected that History
  did not list before, no turns shown, the title no longer the replay's name,
  no "Transcript replay" line, the header's head changed from the earlier
  one; then History listing one session more, the replay still there as a
  replay with its turns, the new session with none, and New session greyed
  with "This session is new: nothing has been asked in it."; then the replay
  opened again offering its card with its name and transcript. The same
  course on main failed at its card (`pinned-offered-up-front ...
  second=Continue against the old revision`); the probe it grew from showed
  the old behaviour (`start-new ... selected=<the replay> turns=1
  mode=Transcript replay`).
- AC2: `core_chat_history` passed in dev and in the `core-only` preset (a
  pinned session, asked in or not, and a carried-over one nothing was asked
  in are offered New session; loading and a running turn still refuse).
  `qt_chat_history_menu` passed (the menu's New session offered on a pinned
  session nothing was asked in, pressed it starts one, and greyed again once
  not pinned).
- AC3: `qt_drawer_surfaces_fit_scale_100`, `_125` and `_200` passed with the
  new course (the card's three states at 280, 300, 340 and 760 px in the four
  themes: inside the card, no narrower or shorter than the words need, none
  over another, the conversation no wider than the column, and the
  pre-question card's choices in the ordinary roles after a refused
  question). Run as a look under the native style
  (`BPR_PINNED_CARD_RENDER_ROOT`, Cocoa, QMacStyle, nothing on screen) it
  passed in Light and Dark at 100% and 200%. The same course on main's
  `src/` reported 30 faults at each scale (`before-native-*.log`): the
  conversation 409 px wide in a 280, 300 and 340 px column before a question
  and 342 px wide after a refused one, "Rebase onto 2222222" 28 px tall
  where its words need 32, the old words, and the accent left on the
  pre-question card.
- The wrapping chip: `qt_quiet_buttons` passed (one line unless told to
  wrap; then its hint is the one line, its minimum its longest word, a line
  taller at two thirds of its width, its words drawn on two lines inside its
  padding; an icon or turning it off keeps one line). `qt_button_faces` and
  `qt_button_faces_cocoa` passed.
- The store: `sqlite_agent_setup_storage` passed (an unused empty replay is
  taken up again at the revision it reads; after the head moved a new one
  reads the head under review and the one left behind keeps its own).
- AC5, the surfaces around it: `application_native_agent_sessions` passed
  (History's New session, renames, deletions, the tour chat, the replay
  course), the eleven `application_agent_chat_*_contract` rows and the three
  `application_agent_chat_*_public_path` rows passed (the side chat's own
  Start a new session and continuations), the nine
  `application_agent_invocation_*_contract` rows,
  `application_agent_finding_composition_contract`,
  `application_follow_up_public_path` and
  `application_setup_destination_public_path` passed.
  `qt_workbench_window_scale_100`, `_125` and `_200`, `qt_side_chat_panel`,
  `qt_item_side_chat`, `qt_side_chat_tab_and_send_scale_100` and `_200`,
  `qt_tour_side_chat`, `qt_tour_side_chat_width_scale_100`, `_125` and
  `_200`, `qt_agent_session_menu`, `qt_drawer_parts_scale_100`,
  `qt_home_card_fit_scale_100` and `_200`, `application_agent_invocation_host`,
  `application_agent_chat_replies` and `sqlite_agent_session_management`
  passed. `architecture_boundaries`, `test_network_isolation_manifest` and
  `test_network_isolation_structure_contract` passed.
- `application_review_round_trip_composition_contract` passed with the
  production app manifest resealed over `window_agent_chat.cpp`,
  `window_agent_setup.cpp` and `window_composition.hpp` (full application and
  mutation-branch digests unchanged).
- AC6: `format_check.py` (clang-format 22.1.8) passed on the twenty touched
  C++ files. `tidy.py --warnings-as-errors` over the fourteen touched
  translation units reports nothing on a changed line; its findings are in
  lines this card does not change (`agent_tab_view.hpp:469`,
  `agent_tab_view.cpp:2687`, `local_draft_workflow_area.hpp:72`,
  `agent_setup_store.cpp:621-622`,
  `agent_invocation_composition_public_path.cpp:2247`).

Not passing in this tree, and not from this change:
`scripts/verify_documentation.py --root .` stops on `docs/issues/NEXT_ID`'s
tracker state as the lane sees it; in a worktree without the MASK-deployed
skills it also needs the main checkout's `implement-rfc/scripts` on
`PYTHONPATH`.

Renders, native look (Cocoa, QMacStyle, nothing on screen), Light and Dark,
100% and 200%, drawn from the workbench window's Comments drawer on its Agent
tab, in
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/replay-card-renders/cocoa-macos/`:
`before-<theme>-<scale>-<state>-<width>.png` (main's sources) and
`after-<theme>-<scale>-<state>-<width>.png` for the states `offered`,
`refused` and `reconcile` at 280, 300 and 340 px; the runs' logs are
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/replay-card-renders/before-native-light-scale0.5.log`,
`before-native-light-scale1.log`, `after-native-light-scale0.5.log` and
`after-native-light-scale1.log` (each run draws both themes). The pinned
course also saves the real window before and after the press as
`native-pinned-up-front.png` and `native-pinned-new-session.png` beside its
data root.

## Owner judgement

- The look of the card at narrow widths is the owner's to accept; the
  renders above are what a frozen tests-on build would show with the Fixture
  agent. Below about 350 px the two choices stand one under the other.
- "Keep my changes on a branch and move" keeps the owner's words and takes a
  second line in a narrow drawer rather than a shorter name.
- Start a new session keeps the conversation left behind; nothing is deleted
  and nothing is sent.
