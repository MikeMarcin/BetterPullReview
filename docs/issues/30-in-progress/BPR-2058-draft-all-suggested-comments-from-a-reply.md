---
schema: "implement-rfc.card.v1"
id: "BPR-2058"
title: "Draft all suggested comments from an agent reply with several"
status: "In Progress"
rfcs: ["RFC-BPR-0092"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2058--BPR-2059--bd1353aea511.json","origin":"owner request, 2026-11-10","related_cards":["BPR-1753","BPR-1861","BPR-2059"]}
depends_on: []
---

# Draft all suggested comments from an agent reply with several

## Origin

The owner, 2026-11-10: "Can we get a draft all suggested comments from an
agent response with several."

## Scope and acceptance

1. A reply whose finding cards carry two or more suggested comments offers
   **Draft all (N)**. N counts only the suggestions it would draft: not already
   drafted, not dismissed, not under way, and still placeable at the current
   head.
2. A press makes each a local draft at its file and lines exactly as the
   card's own **Add as draft comment** does. Nothing goes to GitHub.
3. Suggestions whose lines are no longer in the diff, or already drafted, are
   skipped; the reply says how many were drafted and skipped, and why.
4. One **Undo** removes the whole batch (a draft edited since is kept, and the
   reply says so).
5. Draft all starts no run: no Agent accent, and the accent inventory stays
   green.
6. Owner look on a frozen build whose agent reply holds several suggested
   comments: Accept or Reject Draft all.

Load frontend-design before the Qt work.
