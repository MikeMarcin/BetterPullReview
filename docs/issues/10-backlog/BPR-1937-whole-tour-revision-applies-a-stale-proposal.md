---
schema: "implement-rfc.card.v1"
id: "BPR-1937"
title: "A later whole-tour revision doesn't replace the earlier one: asking for 8 stops applies the first 10-stop plan"
status: "Backlog"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-1930"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-08, coordinator chat","related_cards":["BPR-1930","BPR-1934","BPR-1925"]}
acceptance: []
---

# BPR-1937: A later whole-tour revision doesn't replace the earlier one: asking for 8 stops applies the first 10-stop plan

## Observed (owner, 2026-11-08)

In the whole-tour chat on a 12-stop tour:

1. The owner pressed the suggested prompt "Too many stops?". The agent
   suggested going from 12 to 10.
2. The owner asked to go lower. The agent suggested a way to get to 8.
3. The owner said "apply to review". The tour became 10 stops.
4. The owner said "it's still 10". The agent agreed and again suggested a way
   to get to 8.
5. "Apply to review" again. Still 10.

## Suspects (verify)

- Only the first pending revision is kept: a later turn's revision doesn't
  replace or merge with the waiting one, and Apply, Accept all, or Update
  from this chat applies the stale one.
- The later turns described the 8-stop plan only in prose, and the answer
  carried no `bpr.tour-revise.v1` payload (or carried the earlier one
  again). Then "apply" re-derives from the conversation and lands on the
  first plan.
- The 8-stop plan's merges or removals were refused by the core's
  validation, e.g. a merge across files with unconfirmed lines (BPR-1930
  uncertainty), so they were silently dropped, and only a count was shown.
  The owner saw no reason.
- The replay given to the agent on each turn shows the tour as it was
  before the applied revision, so the agent re-proposes against stale state.

## Change

- Reproduce with a stub agent following the exact sequence above (12 → a
  10-stop revision, then an 8-stop revision, then apply), and find the cause.
- A newer revision from the same chat supersedes the pending one. The
  in-place marks show the newest plan, and applying applies exactly what is
  shown.
- Every turn's replay and prompt carries the tour as it is now, including
  applied revisions.
- If any part of a revision is refused, the chat and the tour say which part
  and why, in plain words, not just a count.
- If the agent describes a plan without a revision payload, the chat offers
  "Update the tour from this chat", which asks for the structured revision,
  rather than leaving the person to type "apply".

## Acceptance

- AC1. A stub-driven course of the sequence above ends at 8 stops after
  apply (application and Qt tests).
- AC2. A refused part is named with its reason in the chat and on the tour.
- AC3. The replay after an applied revision reflects the applied tour.
- AC4. The documentation gate passes. No owner look is needed unless visible
  behaviour beyond messages changes.
