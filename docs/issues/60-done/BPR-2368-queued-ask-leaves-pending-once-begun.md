---
schema: "implement-rfc.card.v1"
id: "BPR-2368"
title: "A queued ask whose turn is running leaves the queue instead of staying Sending until it ends"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner report, 2026-11-24","reservation":"docs/issues/_reservations/BPR-2368--BPR-2368--XXXXXXXXXXXX.json","related":["BPR-2181","BPR-2152","BPR-2154","BPR-2245"],"acceptance":["BPR-2368:core","BPR-2368:runtime","BPR-2368:course"]}
---

# BPR-2368: A queued ask whose turn is running leaves the queue

## Report

The owner, 2026-11-24: "If I open a PR and click ask -> review this pull
request and then it switches to the agent tab but the agent tab doesnt have
a workspace yet so I need to click check that all seems to work except that
the "review this pull request" goes into the queue and then stays in the
queue as "sending" even during the entire time the prompt is running until
it finishes. Like presumably once it's answering the question it has sent
it right?"

## Cause

Every Ask press goes through the session queue (RFC-BPR-0126,
[[BPR-2181]]). The host claims the entry (`core::claim`, state Sending) and
stores the claim before the turn starts ([[BPR-2152]]); the entry is
settled and removed only when its run retires (`core::finish` from
`agent_queue_host::retired`). Nothing between those two events told the
queue that the turn had begun, so the pending list kept the entry as
Sending for the whole run, while the transcript already showed the same
question as the running turn. This happens whether or not the agent was
set up when Ask was pressed, because a plain press always queues. The owner
noticed it in the set-up-first flow, where the queue is on screen before
the turn starts.

## Fix

- Core: `core::begin_turn(queue, id, run)` marks a sending release Sent and
  binds it to its run. It refuses a steered correction, another run's turn
  and run 0. A repeat for the same run changes nothing.
  `core::listed(entry)` is false only for such a begun release. `finish`
  settles a begun release exactly as it settled a sending one (went, failed,
  stopped, usage limit, not taken, refused, unknown). `restore` still makes
  it Delivery unknown after a crash, and `next_release` still holds the
  session until its run ends.
- Application: `review_agent_runtime::dispatch` reports the turn of a
  reserved release run as begun (`on_began`). `review_agent_runtimes` and
  `agent_session_runner::began` pass that report on for the run the release
  was admitted as, and `agent_queue_host::began` applies `core::begin_turn`.
  When the turn begins inside its own start, the start commits it. A
  release that reads its material first (awaiting) stays Sending until its
  turn is sent on the later pass.
- Presentation: the Agent tab and the tour or local-draft chat pending
  lists skip entries that are not `core::listed`. A steered correction
  still reads Sent to agent until its run settles it, the known limit
  recorded for RFC-BPR-0126.
- Docs: `docs/ux/ai-experience.md` (the pending list) and
  `docs/architecture/system-architecture.md` (the delivery rule).

## Acceptance

- **BPR-2368:core** `core_agent_queue`
  (`test_begun_release_leaves_the_pending_list`): a begun release is Sent,
  bound to its run and not listed. Another run, run 0 and a correction are
  refused. The session stays held until `finish` settles it. Not taken goes
  back first and listed, unknown is listed with Discard, a crash restores it
  as uncertain, and a sent correction stays listed. Passed at b9efea960.
- **BPR-2368:runtime** `application_review_agent_runtime`
  (`test_a_released_turn_leaves_the_pending_list_once_it_begins`): Review
  this pull request is queued while the presenter is not ready (setup
  unfinished). Once ready, its turn runs and the subscriber's pending list
  is empty while the turn runs, with the session held. A follow-up waits,
  listed. Retirement settles the review and the follow-up goes. An awaiting
  explain stays listed until its later-pass turn. A begun turn that ends
  cancelled pauses what remains. Also `application_agent_queue_host`
  (`test_a_begun_turn_leaves_the_pending_list`). Passed at b9efea960.
- **BPR-2368:course** `application_ask_setup_review_public_path`: through
  the window, Review this pull request is pressed before setup and Check
  passes. At every look while the review's turn shows working, the Agent
  tab's pending list has no row. Passed at b9efea960, 6 of 6 runs. With the
  runtime's report disabled it failed ("while the review's turn runs it is
  not also pending in the queue"), and so did
  `application_review_agent_runtime` ("its turn began: Sent, bound to its
  run").
- Also passing at b9efea960: `core_agent_steering`,
  `application_agent_queue_handoff`, `qt_agent_steering_fixture`,
  `application_queue_composition_contract`,
  `application_ask_{menu,addressed,report,queue}_public_path`,
  `application_agent_steer_{sent,unavailable}_public_path`,
  `application_tour_feedback_queue_public_path`,
  `application_agent_queue_{windows,second_window_loading,close_runs_on,restart,tour_feedback_crash,finish_pauses}_public_path`,
  `application_tour_chat_queue_{stop,whole_tour,local_draft}_public_path`;
  `coding-format-check`; clang-tidy (LLVM 22, warnings as errors) on the
  ten touched translation units.
