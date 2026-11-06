---
schema: "implement-rfc.card.v1"
id: "BPR-1880"
title: "Every failed agent message offers Delete"
status: "Backlog"
rfcs: []
depends_on: ["BPR-1876"]
priority: "P2"
platforms: ["mac"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-06: 'Put Delete on every failed message, not just unsent ones'"}
acceptance: []
---

# BPR-1880: Every failed agent message offers Delete

BPR-1876 v002 offers Delete only on a failed turn whose message never reached
the agent (did not start, workspace not made, history not recorded). The
owner wants it on every failed turn: stopped, timed out, error exit,
unreadable, set aside, as well.

## Change

- Offer Delete beside the failed turn's action (Resend, or Agent settings…
  for an unreadable answer) on every failed turn, latest or earlier, while
  nothing is running.
- Deleting removes the turn from the thread and from what is kept for next
  time, as BPR-1876 does.
- For a message the agent did receive, decide and record what deletion means
  for the agent's own resumable session (for example: the next message starts
  a fresh agent session, or the agent may still remember it). Never leave the
  reviewer believing the agent forgot something it still holds; if it may
  still remember, the tooltip says so in one line.

## Acceptance

- AC1. Every failed turn kind shows Delete; successful turns do not. Core and
  Qt tests.
- AC2. Deleting a received-and-failed turn behaves as recorded for the agent
  session, covered by a test.
- AC3. `python3 scripts/verify_documentation.py --root .` passes.
