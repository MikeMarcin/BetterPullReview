---
schema: "implement-rfc.card.v1"
id: "BPR-2193"
title: "The continued chat contract fails about half the time: the second turn ends stale_result and never quiesces"
status: "Done"
rfcs: ["RFC-BPR-0126"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-2181 second round, 2026-11-15: failed in the scratch run at d9962f2ff and at its parent 4bb44f28f, passed at 0d70ff618","related_cards":["BPR-2181"],"reservation":"docs/issues/_reservations/BPR-2192--BPR-2193--222f766917c9.json"}
acceptance: [{"criterion": "AC01: the cause is named and fixed; the contract passes 20 of 20 runs", "outcome": "pass", "revision": "1af5aba43", "evidence": {"kind": "command", "command": "scratch worktree at 1af5aba43 (and at 12aa86eb6 plus c0bdc7297's change before it): ctest --preset dev -R '^application_agent_chat_continued_contract$' --repeat until-fail:20 -j1", "detail": "20 of 20 at c0bdc7297's change and 20 of 20 again at 1af5aba43 (a verbose run prints agent-chat-continued:pass). Cause, from instrumented runs before the fix: the window lent the stack the same head a third time with new comparison epochs (1 to 2); the continued turn's workspace started before the stack's epoch host had settled that loan's ingestion, and the acceptance bumped its blob authorization, so the workspace ended stale (agent_workspace_failure::stale, no process started, finish stale_result). Holding turns only for a moved head still failed 1 in 20; waiting for the window's exact epochs broke application_fixture_agent_every_ask, which passes at 1af5aba43."}}]
---

# BPR-2193: The continued chat contract fails about half the time: the second turn ends stale_result and never quiesces

`application_agent_chat_continued_contract` is intermittent. Measured on the
lane's dev tree at `f65bfa1e2` (with another writer's uncommitted edits in
three window files, while a scratch build ran beside it): 11 of 20 runs
passed. It also failed once and passed once in clean scratch worktrees
(`4bb44f28f` failed, `0d70ff618` passed).

The failure line:

```
agent-chat-composition-failure:continued-turn failed=second-run-quiescent process=1 replies=1 panel=Finished: stale_result second-run-quiescent=false
```

The continued (second) turn finishes as `stale_result` and the agent
machinery does not become quiescent. It fails at `4bb44f28f`, which already
runs Agent-tab turns on the session runtime (BPR-2181 slice 2), so the
likely place is the runtime's stack: the revision its epoch host observes
when the second turn is admitted, and its retirement after a stale result.
This is BPR-2181's area; it was not introduced by that card's second round.

## Acceptance

- **AC01:** the cause is named and fixed; the contract passes 20 of 20 runs.

## Cause

Not the runtime's retirement and not a supersession. The second turn never
started a process: its workspace ended `stale` while it was being made
(`agent_workspace_failure::stale`, from the stack's epoch host), which the
invocation host reports as `stale_result`. Instrumented runs showed the
order on a failing run: the window lends the review to the runtime's stack
three times, the third for the same head with new comparison epochs
(the window's comparison moved from epoch 1 to 2 after the refresh); the
continued turn's workspace starts before the stack's epoch host has
observed that third loan, and accepting the loan re-authorizes the epoch
host's blobs (`authorize_blobs` bumps `BlobAuthorityGeneration`), so the
workspace admitted under the generation before ends stale. On passing runs
the stack observed the third loan before the turn's workspace started.
Before BPR-2181 slice 2 the turn's workspace was made by the window's own
epoch host, which no loan re-authorizes.

## Fix

`review_agent_stack` (BPR-2181's area):

- A turn runs only once the epoch host has settled the ingestion of the
  loan last lent on its revision (`LoanPending`, cleared by the first
  presentation with no pending ingestion at the lent revision); a turn sent
  before then waits for it (`accepting`) and starts on the stack's `Ready`.
  A first version waited for the window's exact binding, comparison epochs
  included; the stack's own epoch host need not reach the window's epochs,
  and `application_fixture_agent_every_ask` then never answered
  (`b16fd02d4`'s set); `1af5aba43` fixed that.
- A loan of the same revision lent while a turn runs waits for that run to
  retire (`WaitingLoan`, applied in the agent host's retirement observer
  before the runtime hears of it), so no workspace in the making is
  re-authorized under it. `lent_for` answers for the waiting loan, so the
  window does not mint another.
