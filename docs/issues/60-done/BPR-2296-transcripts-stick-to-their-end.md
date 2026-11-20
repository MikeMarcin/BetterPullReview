---
schema: "implement-rfc.card.v1"
id: "BPR-2296"
title: "A transcript at its end stays there as it grows; one scrolled up stays put"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"owner-request","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2296--BPR-2296--cfba6762f944.json","found_by":"owner, 2026-11-19","related_cards":["BPR-2279","BPR-2286","BPR-2292"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"1f9f3bf75","evidence":{"kind":"command","detail":"qt_stick_to_bottom: a bare transcript, the Agent tab (streamed turn) and the follow-up window (new rounds) stay at their end as turns are appended and rows grow; within 8 px of the end counts as the end"}},{"criterion":"AC2","outcome":"pass","revision":"1f9f3bf75","evidence":{"kind":"command","detail":"qt_stick_to_bottom: after a wheel notch up (and a one-pixel nudge by the bar) appended turns, streamed text and a growing row leave the value where the reader put it, on the bare transcript, the Agent tab and the follow-up window; back at the end it follows again"}},{"criterion":"AC3","outcome":"pass","revision":"1f9f3bf75","evidence":{"kind":"command","detail":"qt_stick_to_bottom: a tool row growing, shrinking and growing (200,60,240,30,300,120,320 on the bare transcript; 180,40,220,24,260 in the Agent tab's thread) records no falling scroll value; the turn's end lets the held height go and settles at the real end. Without the held height the bare course fails with 346 206 386 176 446 266 466"}},{"criterion":"AC4","outcome":"pass","revision":"1f9f3bf75","evidence":{"kind":"command","detail":"qt_nested_scroll_latch passes and the course checks no gesture is claimed by following; qt_follow_up_window, qt_tour_side_chat, qt_tour_chat_in_tab, qt_explain_file_card_scale_100/200, qt_agent_restored_byline, qt_agent_finding_cards_retained/restart, qt_agent_steering_fixture, qt_agent_activity_view, qt_side_chat_panel, qt_review_tour_agent_failure/help/setup_call, qt_composed_github_scale_100, application_follow_up_public_path, application_explain_file_public_path, application_tour_chat_queue_{stop,whole_tour,local_draft}_public_path pass; format (clang-format 22.1.8) and tidy on the changed files; verify_documentation"}}]
---

# BPR-2296: A transcript at its end stays there as it grows; one scrolled up stays put

## Asked

Owner, 2026-11-19: "The the agent tab and any similar session transcript
window should Auto scroll to the bottom if you're at the bottom when anything
new gets added below."

And, the same day: "If tool rows grow and then contract at the bottom it
should stay at its lowest auto scroll value it shouldn't ping pong up and
down".

## Acceptance

- AC1. The Agent tab and every similar transcript (the follow-up window of
  [[BPR-2286]], the tour and local-draft side chat, the Explain this file
  card's answer) stay at their end when they are at it, or within a few
  pixels of it, and something is added or grows below: a turn, streamed text,
  a tool row, a failure block, a disclosure opening.
- AC2. A transcript the reader scrolled up, by a wheel, a gesture or the bar,
  does not move when anything is added or grows; back at its end it follows
  again.
- AC3. While a turn runs and the transcript is at its end, a row that grows
  and shrinks never moves the view back up; the held height goes when the
  turn ends.
- AC4. The nested scroll latch ([[BPR-2279]], [[BPR-2292]]) is not fought:
  wheel events are left to it. Qt tests cover append, growth while pinned
  and no movement when scrolled up for the Agent tab and the follow-up
  window, and the grow-shrink-grow course; focused tests, format, tidy on the
  changed files and the documentation gate pass.

## Design

`bpr::qt::stick_to_bottom` (`src/adapters/qt/include/bpr/qt/stick_to_bottom.hpp`)
attaches once to a resizable `QScrollArea` and follows its vertical bar only:

- A transcript starts pinned. A move down to within `slack` (8 px) of the end
  pins it; any move up, however small, unpins it, so a reader's first nudge
  is never pulled back.
- While pinned, every change of range moves it to the new end, ahead of the
  bar's own clamp, so growth, a shorter viewport and a shorter content alike
  end at the end.
- While a turn runs (`set_turn_running`) and the transcript is pinned, the
  content keeps the tallest height it reached as its least height, so a row
  that shrinks leaves room under it (the views' columns end in a stretch)
  rather than pulling the view up. The turn's end, or a change of width
  (a reflow), lets it go. `pin()` takes the transcript to its end on purpose:
  another follow-up item or side chat opened, or the author's own new
  question in a side chat.
- It never handles a wheel event, so the latch still chooses which scroller
  a gesture moves; it only answers the content's growth.

Views:

- Agent tab conversation (`agentConversationScroll`): attached; the turn
  runs while the tab is `working`.
- Follow-up window (`followUpRounds`): attached; another item opens at its
  end; the turn runs while the window's phase is not idle. It no longer
  jumps to the end on every new round, and lays the rounds out with the
  event loop instead of `adjustSize`, so a reader scrolled up keeps their
  place through a rebuild.
- Tour and local-draft side chat (`tourSideChatScroll`): attached in place of
  its own `Stick` flag, which only a bar drag cleared (a wheel scroll up was
  pulled back on the next update); another chat and the author's new
  question pin; the running turn holds the height.
- Explain this file card's answer (`explainFileAnswerScroll`): attached, with
  no held height (one text only grows).

Not transcripts, left alone: an Agent tab report (a document read from its
top, with its own section navigation), the review cycle's notes and source,
and the tour conversation window's whole-session text, which shows a chosen
turn by its anchor.

## Verification

- `qt_stick_to_bottom` (new, `tests/adapters_qt/stick_to_bottom_tests.cpp`),
  offscreen: a bare transcript with the nested scroll latch installed, the
  Agent tab in a workbench window with a streamed fixture turn, and the
  follow-up window with six rounds and more. Append, growth while pinned and
  no movement when scrolled up on all three; the grow-shrink-grow course on
  the bare transcript and in the Agent tab's thread, watching every value the
  bar takes. Without the held height it fails at "grow, shrink, grow: the
  scroll value never decreases: 346 206 386 176 446 266 466".
- Also passing: `qt_follow_up_window`, `qt_tour_side_chat`,
  `qt_tour_chat_in_tab`, `qt_nested_scroll_latch`,
  `qt_explain_file_card_scale_100/200`, `qt_agent_restored_byline`,
  `qt_agent_finding_cards_retained/restart`, `qt_agent_steering_fixture`,
  `qt_agent_activity_view`, `qt_side_chat_panel`,
  `qt_review_tour_agent_failure/help/setup_call`,
  `qt_composed_github_scale_100`, `application_follow_up_public_path`,
  `application_explain_file_public_path` and
  `application_tour_chat_queue_{stop,whole_tour,local_draft}_public_path`.
- Format passes on the changed files. Tidy passes on the new files and on
  the touched follow-up window, side chat and Explain card; on
  `agent_tab_view.cpp` it reports two findings in untouched code (`press_offer`'s
  parameter names, a copy at `agent_tab_view.cpp:2623`), none in this change.
- The tour side chat has no dedicated stick-to-bottom course: its behaviour
  is the shared helper's, and its existing courses pass.
