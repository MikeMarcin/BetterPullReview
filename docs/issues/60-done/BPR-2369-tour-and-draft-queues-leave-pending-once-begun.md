---
schema: "implement-rfc.card.v1"
id: "BPR-2369"
title: "A queued tour feedback, tour chat or local draft message leaves the pending list once its turn begins"
status: "Done"
rfcs: []
depends_on: ["BPR-2368"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner approval, 2026-11-24, extending BPR-2368","reservation":"docs/issues/_reservations/ BPR-2369--BPR-2369--63705c803e28.json","related":["BPR-2368","BPR-2154","BPR-2181","BPR-2151","BPR-2364"],"acceptance":["BPR-2369:runtime","BPR-2369:course"]}
---

# BPR-2369: Tour and local draft queues drop a message from the pending list once its turn begins

## Report

[[BPR-2368]] took a released Agent tab message off the pending list once its
turn began. The owner approved, 2026-11-24, doing the same for the other
queues: the tour's first-draft feedback, each tour part's chat (the whole
tour and each stop) and each part of a local draft's discussion. These
queues still showed a released message as Sending for its whole run, even
though their chat already showed it as the running turn.

## Cause

These sessions run on the window's own runtimes, which the window offers to
the profile's `agent_session_runner`. The tour feedback runtime is in
`window_quick_ask.cpp` and `window_tours.cpp` (`release_tour_feedback`). The
tour-part and local-draft runtime is in `window_chat_queues.cpp`
(`release_chat_queued`). Only `review_agent_runtime::dispatch` reported a
turn as begun (`on_began`). These runtimes never reported it, so
`agent_queue_host::began` was never called for their sessions.

## Fix

- Application composition: when a release's turn has really started, each
  window runtime reports it through the existing
  `agent_session_runner::began` inside its own start, for the run it is
  admitted as. For the tour feedback and a tour part, that is when
  `request_tour_ask` has put the turn in the tour chat
  (`TourAskPending`). For a local draft part, it is when the draft's
  question is asking. `agent_queue_host::began` applies `core::begin_turn`
  while draining, and the start commits it. No new mechanism was added.
  A start that is answered without a run, or refused, reports nothing, so
  the message stays Sending and listed as before.
- None of these runtimes reads material first (none returns `awaiting`).
  Crash recovery is unchanged: a stored begun release is restored as
  Delivery unknown. Steered corrections are unchanged:
  `core::begin_turn` refuses them.
- `recover_tour_feedback` counts a begun release as in flight, as it counts
  a sending one.
- Test access: `agent_session_evidence::Listed` counts the entries the
  pending list shows (`core::listed`).
- Docs: `docs/architecture/system-architecture.md` (the delivery rule now
  names the window runtimes that report a begun turn).

The tour feedback queue has no pending-list row of its own in this window.
The tour chat's waiting buffer shows only Queued entries, so the visible
change there is limited to the queue's state.

## Acceptance

- **BPR-2369:runtime** `application_agent_queue_host`
  (`test_a_window_runtime_turn_begun_inside_its_start_leaves_the_pending_list`).
  It covers a tour feedback session, the whole tour's and a stop's tour-part
  sessions, and a local draft discussion session, each on a real
  `agent_session_runner` whose runtime reports its turn begun inside its own
  start. The release is Sent, bound to its run, stored, and off the pending
  list while it still holds the session. Another runtime's or run's
  beginning changes nothing. Retirement settles it and the next message
  begins. A begun turn that fails pauses what remains. A release that has
  not begun stays Sending and listed, and a refused one goes back first. A
  restored begun release is Delivery unknown, listed and paused. Passed at
  99d16a133.
- **BPR-2369:course** `application_tour_feedback_queue_public_path`: while
  the feedback's turn runs in the tour chat, the tour's queue holds it
  (Sending) and lists nothing, and the run's retirement settles it.
  `application_tour_chat_queue_stop_public_path`: at every look while a
  follow-up's turn runs, the stop's pending list does not show that
  follow-up. `application_tour_chat_queue_whole_tour_public_path`: the same
  for the resumed follow-up in the whole tour's chat.
  `application_tour_chat_queue_local_draft_public_path`: the same while the
  queued follow-up is the draft's asking question. All four passed at
  99d16a133, built with BPR-2364 (606cf91a1, c88b2bfa6) reverted locally;
  see the note below. With the window runtimes' `began` reports disabled,
  each of the four failed on its new assertion ("... is not also pending
  ..."). `application_tour_feedback_queue_public_path` passed 4 times.
- Also passing at 99d16a133, on unmodified sources: `core_agent_queue`,
  `core_agent_steering`, `application_agent_queue_host`,
  `application_agent_queue_handoff`, `application_review_agent_runtime`,
  and `coding-format-check` (clang-format 22). clang-tidy (LLVM 22,
  warnings as errors) passed on `window_chat_queues.cpp`,
  `window_tours.cpp`, `fixture_effect_holds_and_agent_evidence.cpp` and
  `agent_queue_host_tests.cpp`.
- Also passing at 99d16a133 in the tree with BPR-2364 reverted (19 of 19,
  one serial run): `application_ask_setup_review_public_path`
  (BPR-2368's course), `qt_agent_steering_fixture`,
  `application_queue_composition_contract`,
  `application_ask_{menu,addressed,report,queue}_public_path`,
  `application_agent_steer_{sent,unavailable}_public_path`,
  `application_agent_queue_{windows,second_window_loading,close_runs_on,restart,tour_feedback_crash,finish_pauses}_public_path`,
  and the four courses above.

## Note: public-path courses on main

On main at 113ae4ac2, every course that opens returning-review-v1's
nested-folders#21 fails before it reaches the queue. The review reads
`fetch_history_too_large` ("BetterPullReview couldn't limit the history
download ..."), and the file never opens. This happens on unmodified main
sources too, and it no longer happens with BPR-2364's 606cf91a1 and
c88b2bfa6 reverted. BPR-2364 landed beside BPR-2368 and did not run these
courses. The fixture's smart transport probably reports no sparse
capability, and with no known repository size `acquire_review_transport`
now refuses. This needs its own card.
