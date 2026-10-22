---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ4"
title: "Each agent session gets its own detached worktree of the linked clone at the head"
status: "Backlog"
rfcs: ["RFC-BPR-0091"]
depends_on: ["BPR-PENDING-AQ3"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0091 adoption, 2026-10-22 (owner answers Q7 and Q8)","related_cards":["BPR-PENDING-AN","BPR-1738","BPR-0348"],"milestone":"RFC-BPR-0091 M4"}
---

# BPR-PENDING-AQ4: Each agent session gets its own detached worktree of the linked clone at the head

RFC-BPR-0091 M4 ([RFC](../../rfcs/RFC-BPR-0091-agent-first-run-setup.md#milestones)), owner answers Q7 and Q8 A.

## Scope

- Getting the head from the application's own copy: nothing when the clone
  has it; a local fetch from the cache on a complete review; the command to
  copy on a sparse review.
- `git worktree add --detach` under the application's data root and
  `git worktree remove --force` on that exact path, with hooks, fsmonitor and
  filters off; the ownership marker, the per-repository limit of three and
  crash reclamation; never `git worktree prune`.
- The worktree row of Check it works, and the agent launched in the worktree.

## Acceptance criteria

- AC1: RFC AC06 on a fixture clone, including byte-identical refs, index,
  stashes, configuration and checkout.
- AC2: RFC AC07 for a new head.
- AC3: RFC AC10's worktree row results.
