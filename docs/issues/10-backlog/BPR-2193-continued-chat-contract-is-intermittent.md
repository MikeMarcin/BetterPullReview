---
schema: "implement-rfc.card.v1"
id: "BPR-2193"
title: "The continued chat contract fails about half the time: the second turn ends stale_result and never quiesces"
status: "Backlog"
rfcs: ["RFC-BPR-0126"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-2181 second round, 2026-11-15: failed in the scratch run at d9962f2ff and at its parent 4bb44f28f, passed at 0d70ff618","related_cards":["BPR-2181"],"reservation":"docs/issues/_reservations/BPR-2192--BPR-2193--222f766917c9.json"}
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
