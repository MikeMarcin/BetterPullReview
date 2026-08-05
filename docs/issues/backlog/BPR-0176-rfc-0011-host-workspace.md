---
id: BPR-0176
title: "Host: workspace preparation, pinned and dirty"
status: Backlog
rfc: RFC-BPR-0011
depends_on: [BPR-0171]
---

# Host: workspace preparation, pinned and dirty

Two modes. A disposable workspace pinned to the review head, isolated from the
active worktree; and an existing checkout, chosen deliberately, where the
reviewer's uncommitted work is visible to the agent.

## Required direct evidence

- A disposable workspace matches the session's head exactly and is unaffected
  by changes made to the active worktree afterwards.
- Existing-checkout mode is never the default and never reached without an
  explicit choice.
- The dirty state of an existing checkout is reported rather than assumed
  clean — an agent reasoning about uncommitted work should be known to be
  doing so.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
