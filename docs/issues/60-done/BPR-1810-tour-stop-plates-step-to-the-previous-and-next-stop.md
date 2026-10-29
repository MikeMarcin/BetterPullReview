---
schema: "implement-rfc.card.v1"
id: "BPR-1810"
title: "A tour stop's plate steps to the previous and next stop"
status: "Done"
rfcs: []
depends_on: ["BPR-1806"]
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1810--BPR-1810--b03cdcbef00a.json","kind":"ux","found_by":"owner attended review 00fef88b0f9a8d6133fc06b452010a90, 2026-10-29","related_cards":["BPR-1806","BPR-1805","BPR-1802"],"frontend_design":"frontend-design:frontend-design loaded before the Qt surface"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "aa4d56cf1b6c93be898f381ea691bd43ac3cbf66", "evidence": {"kind": "command", "command": "ctest -R '^core_review_tour_markdown$'", "detail": "published_tour_stop_neighbours: a whole tour of three given out of order leads each stop to its neighbours by index, and a tour of one leads nowhere; a neighbour not at hand, a number two stops of the tour share, a stop without its Previous or Next link, and a stop of another tour (another Tour overview) at the next number are reached by nothing; no stops, no links."}}, {"criterion": "AC2", "outcome": "pass", "revision": "aa4d56cf1b6c93be898f381ea691bd43ac3cbf66", "evidence": {"kind": "command", "command": "ctest -R '^qt_tour_stop_card$'", "detail": "test_the_plate_steps_along_the_tour: the middle stop's first post knows its neighbours' threads, numbers and titles; the first has no previous, the last no next, a reply and an ordinary comment none. Open, its plate has Previous and Next, both enabled, named and tooltipped 'Previous stop: Where the frame starts' and 'Next stop: Teardown', StrongFocus, inside the plate in order. Next (clicked while focused) opens stop-3; there Next is unavailable, says 'This is the tour's last stop', and focus lands on Previous. Return on Previous opens stop-2 with focus on Previous again; Space opens stop-1, where Previous is unavailable and says it is the first stop; step_tour past the first does nothing. With a step dispatch the view names the thread and opens nothing. A neighbour removed from the forest is unavailable and says 'Stop 3 is not among these comments' while the other still leads; a tour of one has no chevrons. Existing BPR-1806 plate, Open in tour and theme checks pass."}}, {"criterion": "AC3", "outcome": "pass", "revision": "aa4d56cf1b6c93be898f381ea691bd43ac3cbf66", "evidence": {"kind": "command", "command": "cmake --build --preset dev; ctest -R '^(qt_workbench_overview|qt_workbench_window_scale_100|qt_conversation_cards_view|qt_feedback_label_threads|qt_review_tour_stop_view|qt_review_tour_steps|application_review_tour_composition_public_path|application_review_tour_writing|qt_discuss_comment_bodies|architecture_boundaries)$'", "detail": "The window's tour step dispatch opens a neighbour among the notes in place and otherwise goes to it as Next new comment does (go_to_conversation, factored out of step_to_next_new_comment unchanged). Built and run on macOS dev; no composition change, so the round-trip composition pins are untouched."}}, {"criterion": "AC4", "outcome": "pass", "revision": "aa4d56cf1b6c93be898f381ea691bd43ac3cbf66", "evidence": {"kind": "artifact", "detail": "With BPR_TOUR_STOP_RENDER_DIR set, qt_tour_stop_card writes tour-stop-step-{first,middle,last}-{light,dark,high-contrast-light,high-contrast-dark}.png; copies are in the attended-review session folder outside the repository."}}, {"criterion": "AC5 owner look", "outcome": "pass", "revision": "aa4d56cf1b6c93be898f381ea691bd43ac3cbf66", "evidence": {"kind": "observation", "detail": "Owner accepted, no note, candidate BPR-1810-plate-step-navigation in attended screenshot review session 3313da47114fe2f10516fc0166882a08 (state /Users/jaeger/bpr-attended/tour-cards-v002/state, handed back 2026-10-29T07:22Z, host stopped): the plate's previous/next stepper in four themes at the first, middle and last stop, rendered at aa4d56cf1. The merge of main with BPR-1802 at 9816b7092 renders all 46 of the session's images pixel-identical."}}]
---

# BPR-1810: A tour stop's plate steps to the previous and next stop

## Request

Owner attended review of [[BPR-1806]], session 00fef88b…, on the open
thread's tour-stop plate, rejected, verbatim:

> Should still provide tour next/prev navigation in a compact and beautiful ux

[[BPR-1806]] set aside the published Previous / Tour overview / Next links
with the rest of the website's scaffolding, so a reader in the conversation
had no way along the tour but back to the Tour tab.

## Outcome

- A stop of a tour longer than one shows a small stepper on its plate: "Stop
  2 of 3" in a hairline pill whose round ends are a previous and a next
  chevron.
- Each chevron is named and tooltipped for the stop it reaches ("Next stop:
  Teardown") and opens that stop's thread where the reader is: in place among
  the same notes (Review's Comments drawer, Discuss, Overview's Comments), or,
  for a thread on another file in Review's drawer, at its line as Next new
  comment goes there.
- At the tour's ends, or while the neighbouring stop is not among the
  comments at hand, the chevron stays in place, faint and unavailable, and
  says why. A tour of one has no stepper.
- Tab reaches Previous, Next, then Open in tour. Space or Return steps. After
  a step the keyboard stays on the same chevron of the new plate, or on the
  other one at the tour's end.
- Core decides which stop a link reaches; widgets perform no effect.

## Design

Loaded `frontend-design:frontend-design`. The plate already speaks the
tour's route language (amber ring, dotted route line). The stepper is the
quietest thing that still reads as a control: the badge the plate already
had, now framed in a route-line hairline pill, with the chevrons as its round
ends. Hover is a round amber wash; focus is the post tools' signal ring; a
chevron with nowhere to go keeps its place in faint ink so the stepper does
not change shape from stop to stop. The neighbouring stop's title is in the
tooltip and accessible name rather than on the plate, which keeps the plate
one line taller than BPR-1806's at most.

Stepping stays in the conversation, where the reader is; Open in tour
remains the way to the Tour tab.

## Built

- **Core.** `published_tour_stop_links` and
  `published_tour_stop_neighbours` in `review_tour_markdown`: a stop's
  Previous or Next reaches the one other stop of the same tour (same Tour
  overview) numbered one less or one more, when the stop has that link;
  otherwise nothing.
- **Cards view.** `RetainedTourLinks` beside BPR-1806's counts;
  `card_post::TourPrevious` and `TourNext` (`tour_stop_neighbour`: thread,
  number, title) for a thread's first post; the open card's key includes
  them. `set_tour_step_dispatch` and `step_tour`; without a dispatch a thread
  among the notes opens in place.
- **Plate.** `tour_step_button` and the stepper in `tour_stop_plate`
  (`previous_button`, `next_button`, `StepRequested`), relayed by
  `thread_post_view` and `open_thread_card` as `TourStepRequested`.
- **Window.** `set_tour_step_dispatch` opens a neighbour among the notes,
  else `go_to_conversation`, factored unchanged out of
  `step_to_next_new_comment`.
- Not changed: Open in tour, the closed note, permalink snippets
  ([[BPR-1802]]).

## Owner review

Attended screenshot review session 3313da47114fe2f10516fc0166882a08 (state
/Users/jaeger/bpr-attended/tour-cards-v002/state, handed back 2026-10-29) at
aa4d56cf1: the plate's previous/next navigation accepted, no note. The merge
with main's BPR-1802 (9816b7092) renders the session's images
pixel-identical; those renders do not show BPR-1802's snippet box, which
appears under the plate once a pinned source's lines are read.

## Not verified

- macOS offscreen only; Windows and Linux were not run.
- Keys were sent as synthetic events, not through the native key map.
- A cross-file step in Review's drawer is covered by the shared
  `go_to_conversation` path, not by a dedicated window course.

## Follow-ups (not allocated)

- The route list's one-line excerpt under each stop on the Tour tab shows the
  stop's Markdown source (`**`, list dashes, backticks). It predates
  [[BPR-1805]] and could read as plain words.
