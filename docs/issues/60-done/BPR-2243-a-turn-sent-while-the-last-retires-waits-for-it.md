---
schema: "implement-rfc.card.v1"
id: "BPR-2243"
title: "A turn sent while the last one's run retires waits for it, not refused as still answering"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"application_suggest_change_public_path failing 2 of 3 runs alone on the lane, 2026-11-17","reservation":"docs/issues/_reservations/BPR-2243--BPR-2243--274f6d7c5f8d.json","related":["BPR-2078","BPR-2181"],"acceptance":["BPR-2243:held","BPR-2243:course"]}
---

# BPR-2243: A turn sent while the last one's run retires waits for it

## What failed

`application_suggest_change_public_path` failed alone at its third press
(the sentence reply), after the one-byte-off refusal. Its last printed line
was that refusal ("the text it quoted differs from the head"), which read as
the cause; the failure itself was:

```text
suggest-change: one call
agent-panel: Not started: side chat refused: launch_refused
panel: Not sent. Claude Code didn't start. (The agent is still answering. Nothing was sent.)
```

Measured in a private worktree, 5 runs each: 4 of 5 failed at lane HEAD
`4287637a9`, 1 of 5 at the lane's base on main `2b602756b`. The race
predates the lane's BPR-2216, BPR-2219, BPR-2231, BPR-2232, BPR-2236 and
BPR-2240 work; lane timing only made it more frequent, so there is no
introducing lane commit to bisect to.

## Cause

A product race, not a stale course. The Agent tab settles a turn on the
run's finish, so the window shows the answer (and Suggest the change its
refusal and Dismiss) and accepts the next send. The profile's runtime of the
session (`application::review_agent_runtime`, BPR-2181) stays `Running`
until the run retires: its process exits and its writing folder is removed.
A send in that gap reached `review_agent_runtime::dispatch`, which refused
it, and the window said the agent was still answering and that Claude Code
didn't start. The course presses again at once, so it hit the gap often; a
person typing or pressing quickly could too.

## Fix

`review_agent_runtime::dispatch` holds one turn sent after the running
turn's finish and before its retirement, and sends it at the retirement.
While held the runtime is busy and not quiescent, so the queue, told of the
retirement first, sends nothing ahead of it; the held turn then goes, and
the queue's next goes after it. Stop drops a held turn and says "Stopped.
Nothing was sent."; shutdown drops it. A send while the turn still answers
is refused as before. Commit `756be6052` on the lane.

## Acceptance

- **BPR-2243:held** `application_review_agent_runtime`
  (`test_a_turn_sent_while_the_last_retires_goes_at_its_retirement`): a turn
  sent between finish and retirement is held, not refused; one at most; it
  goes at the retirement ahead of a queued message, which goes after it;
  Stop drops one and says so. Fails on the old runtime ("the next turn sent
  now is held, not refused"). Passed.
- **BPR-2243:course** `application_suggest_change_public_path` passes 8 of
  8 runs alone with the fix (was 4 of 5 failing). The queue, steer and tour
  chat queue courses and the queue host units (16 rows) pass.
