---
schema: "implement-rfc.card.v1"
id: "BPR-1930"
title: "General feedback on the whole tour: the agent revises stops, focus and structure as one proposal"
status: "Backlog"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-1925"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-07, coordinator chat","related_cards":["BPR-1900","BPR-1914","BPR-1925"]}
acceptance: []
---

# BPR-1930: General feedback on the whole tour: the agent revises stops, focus and structure as one proposal

## Owner (2026-11-07)

"I want to be able to give general feedback on the tour and have it react,
particularly during the initial agent draft but even after. For example I
want to say too many stops/too few stops add/remove some or merge some. Or I
want to say the tour should focus on <this particular part of the PR>."

## Change

Load `frontend-design:frontend-design` first.

- **A whole-tour chat.** It sits with the per-part chats from BPR-1925, inside
  the Tour tab, opened from the tour's header with the same toggle button on
  the right. The person gives feedback on the tour as a whole: "too many
  stops", "merge stops 2 and 3", "add a stop on the retry loop", "focus on the
  renderer changes", "reorder so it starts at main()".
- **During the initial draft.** The person can give the same feedback while
  "Draft the tour" runs, or right after it proposes. The next round revises
  the pending proposal instead of starting over.
- **A whole-tour proposal.** The agent answers with a structured revision of
  the tour, not just text. It can add, remove, merge, split and reorder stops,
  rewrite Problem and Solution, and set a focus. The revision arrives as one
  BPR-1900 proposal that shows the changes clearly:
  - removed and merged stops marked;
  - new stops with their BPR-1900 hunk placement and drop rules;
  - order changes visible.

  The person accepts it all, or accepts and discards per item. Nothing changes
  until they accept.
- **Focus is remembered.** A focus instruction ("focus on X") is remembered
  for the tour's later drafting and chat turns, shown as a small chip the
  person can clear.
- **The core decides.** The core validates the revision: stops anchor to the
  PR, merged stops cover their sources' lines, and the order is valid.

## Acceptance

- AC1. "Too many stops" or "merge 2 and 3" on a 6-stop tour returns a
  proposal that removes or merges stops. Accepting it changes the tour exactly
  as shown. Covered by core and Qt tests with a stub agent.
- AC2. "Focus on <part>" yields a proposal whose stops favour that part. The
  focus chip persists for later turns until cleared.
- AC3. The same feedback works while the initial draft's proposal is still
  pending, and revises it.
- AC4. Owner look: Accept/Reject on a frozen build.
- AC5. The RFC amendment and the documentation gate pass.
