---
schema: "implement-rfc.card.v1"
id: "BPR-2245"
title: "A review asked before the agent's setup finished answers, but shows as stopped and leaves Stop all up"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner report, 2026-11-17","reservation":"docs/issues/_reservations/BPR-2245--BPR-2245--6c262208afb6.json","related":["BPR-2243","BPR-2181","BPR-1876"],"acceptance":["BPR-2245:answered","BPR-2245:once","BPR-2245:stop-all","BPR-2245:cancelled"]}
---

# BPR-2245: A review asked before the agent's setup finished answers, but shows as stopped and leaves Stop all up

## Report

The owner, 2026-11-17, on the 0.5.0 prerelease (main at about 0e01b3e9a):
"Interesting when the agent isn't fully set up, it was on the check step,
and I had clicked review this pull request, it seems like after I clicked
check it did set up the workspace and the agent and it looks like it did
actually run the review and gave me a report back that made sense but it
shows up as red and says canceled even though I'm pretty sure the whole
thing completed successfully."

And: "Also stop all is showing up in the status bar after that even though
no agent stuff is running."

## Scope

- Review this pull request pressed while the agent setup waits on its
  Check step is queued; once Check passes the review runs once and its turn
  says it answered, in the ordinary (not error) look.
- Once that run retires and nothing waits, the agent footer's Stop all is
  hidden.
- A turn genuinely stopped is still shown stopped; its ending never paints
  a later turn that answered.

## Acceptance

- `BPR-2245:answered`: through the window, a review queued before setup
  and released by Check ends answered, not stopped, and not drawn as an
  error.
- `BPR-2245:once`: the agent runs the review once.
- `BPR-2245:stop-all`: afterwards no queued entry remains and the footer's
  Stop all is hidden.
- `BPR-2245:cancelled`: a stopped attempt still reads stopped.

## Findings

- Stop all is shown while `agent_queue_host` reports a waiting message
  (`notify_waiting` → `agent_footer::present_queued`). It counted every
  `queued` entry, a paused queue's too. A release the agent refuses
  (`core::finish` with `queued_finish::refused`) hands the message back and
  pauses the queue with `queue_pause::failed`, so after that Stop all stays
  up with nothing running and nothing about to start. Reproduced through the
  window (a release refused with `launch_refused`: "Queued (paused): Review
  this pull request", footer Stop all visible, no run) and in
  `agent_queue_host` alone.
- Fix: `agent_queue_host::waiting_count` (was `queued_count`) counts only
  messages in queues that are not paused: what would go next by itself.
  A paused queue's message starts only on Resume, and Stop all only pauses
  queues and stops runs, so it has nothing to do for one. RFC-BPR-0126 AC09
  ("shown while any agent turn runs or any message is queued") is read as
  queued to go; Resume shows it again.
- The red "canceled" turn is not reproduced. Through the window, with the
  stub Claude as the installed agent, Review this pull request pressed while
  the setup waits on Check is queued, Check passes, the review runs once
  and its turn ends answered (`application_ask_setup_review_public_path`).
  The same held with a slow agent (4 s), with the setup closed at its Check
  step before the press, and with the fixture agent already set up. No path
  was found that paints a delivered report with a cancelled ending, so no
  change was made to the turn's outcome.

## Evidence

- `BPR-2245:stop-all`: `application_agent_queue_host`
  (`test_a_paused_queue_leaves_nothing_waiting_to_go`: a refused release
  pauses the queue and leaves nothing waiting to go; Resume waits again;
  Stop all leaves none) and `application_ask_setup_review_public_path`
  (Stop all hidden after the review retires). Pass.
- `BPR-2245:answered`, `BPR-2245:once`: `application_ask_setup_review_public_path`
  passes on the owner's sequence; the owner's red ending is not reproduced
  (above), so these stay open until it is, or the owner reports it gone.
- `BPR-2245:cancelled`: not touched; `application_review_agent_runtime`
  passes.
- Also passing: `application_ask_queue_public_path`,
  `application_agent_queue_windows_public_path`; documentation gate OK.

## Owner report (2026-11-17)

The owner confirmed the review was started from the review bar's Ask menu. They don't think a queue was showing but can't be sure; the run is gone now. The red "Canceled" stays unreproduced. If it recurs, record what the queue showed and keep the session log.
