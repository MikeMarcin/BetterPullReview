---
schema: "implement-rfc.card.v1"
id: "BPR-2088"
title: "An open from Home while the window refreshes its pull request is not dropped"
status: "Backlog"
rfcs: ["RFC-BPR-0105"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2088--BPR-2090--e27a6d8aee2e.json","origin":"agent-run live course of 2026-11-11 (RFC-BPR-0105 AC08 record, Observed, not changed); layout round of 2026-11-11"}
depends_on: []
---

# An open from Home while the window refreshes its pull request is not dropped

## Scope and acceptance

In the agent-run live course of 2026-11-11 on BPR-Test, the first open from
Home, made while the window still read "Refreshing the pull request." after
the tour's publication, was dropped without a word; the course opened again
and it took ([record](../../verification/RFC-BPR-0105.md)). RFC-BPR-0105 says
no submission waits or vanishes without a visible, bounded, logged
condition; an open is such a submission.

- An open asked from Home while the window refreshes the pull request it
  shows either takes (the refresh is superseded, as another open's would
  be) or is refused with words that say why and what to do, and the refusal
  is logged.
- A composed course reproduces the drop first (open from Home during a
  refresh that follows a tour publication), then passes.

Not scheduled in any milestone; found outside the layout round's surfaces.
