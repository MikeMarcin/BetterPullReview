---
id: BPR-0466
status: Done
title: The summary tables name lanes that twenty-three cards have left
rfc: RFC-BPR-0013
depends_on: []
---

# The summary tables name lanes that twenty-three cards have left

`docs/issues/README.md` carries two tables that answer the only two questions a
session opening this board actually asks: *what can I start today* (the
frontier-roots table) and *what is actually missing* (the group table at the
end). Both answer with a lane word attached to a card ID — `done BPR-0350`,
`blocked BPR-0310`, `backlog BPR-0036`. Twenty-three of those words are wrong.

A macOS session reading the board today is told that `BPR-0036` and `BPR-0053`
are backlog when they are two of the three in-progress roots; that `BPR-0310`,
`BPR-0311`, `BPR-0314`, and `BPR-0315` are blocked, in a lane that no longer
exists; that `BPR-0445` is ready when it is parked on an owner verdict; and
that `BPR-0347`, `BPR-0350`, `BPR-0360`, `BPR-0364`, and `BPR-0412` are still
open when all five are in `60-done/`.

The largest single source is [[BPR-0419]]. It replaced `50-blocked/` with the
per-platform ready lanes and moved nine cards, and it updated the lane table
and the narrative that explains them. It did not update the group table, which
still speaks the pre-split vocabulary — `50-blocked/` is not even a directory
in this checkout any more, so every `blocked BPR-nnnn` cell in it is
unsatisfiable by construction rather than merely stale. The rest is ordinary
lag: a card closes, the sentence that closed it is written, and the summary row
that named it two hundred lines earlier is not.

## Why nothing caught it

`verify_frontier_roots` in `scripts/verify_documentation.py` exists because
this exact table drifted once before, and the section says so in its own text:
"This used to be a hand-maintained list, and it drifted." What that repair
derived from the lanes was **membership** — which cards are roots, which lane
each is in, and that the stated count matches. The `Needs` cell beside each
root stayed hand-written prose, and so did every cell of the group table. The
gate proves the table lists the right cards and says nothing about what it
claims of them.

That split is visible in the drift itself. Not one row names a card that is not
a root, because that half is checked. Twenty-three cells describe those cards'
state wrongly, because that half is not.

## Required final state

- Every lane word governing a card ID inside a table cell of
  `docs/issues/README.md` agrees with that card's actual lane folder.
- `scripts/verify_documentation.py` fails when one does not, and the failure
  names the line, the claim, and the lane the card is really in.
- The check is restricted to table rows. The same words in narrative prose
  describe past states truthfully — "BPR-0391 and BPR-0392, which parked on
  operator packets on 2026-08-19" is a correct sentence about a card now in
  `60-done/` — and a check that read prose would reject the board's own
  history. Possessive forms (`BPR-0235's Linux half`) are claims about part of
  a card, not about its lane, and are excluded for the same reason.
- The check is proven in both directions: it fails on the pre-repair text of
  this commit's parent, and passes after the repair.
- `tests/scripts/verify_documentation_tests.py` covers a contradicting cell,
  an agreeing cell, a prose line that must stay free, and a possessive that
  must stay free.
- The two stale frontier-roots `Needs` cells are corrected against the cards
  they summarize: `BPR-0053` no longer asks for four adversaries its card
  records as implemented and green at `697b8f71`, and `BPR-0240` no longer says
  `windows_agent_process.cpp` "has still never been compiled" when that card
  has carried a dated correction since 2026-08-22.

## Non-goals

- No lane moves, no card-body edits, and no change to which cards are frontier
  roots. `verify_frontier_roots` already owns membership and keeps it.
- No new lane, and no removal of `50-blocked/` from the documented lane set. It
  is a defined lane that is currently empty; an empty lane is not a directory
  Git tracks, and its meaning is unchanged.
- Not a semantic check of the free prose in a `Needs` cell. "Rebuild
  `windows-dev`, then rerun the complete matrix" cannot be machine-compared to
  a card's dated sections. The mechanical half is the lane vocabulary; the rest
  stays a reading obligation, and this card states that limit rather than
  implying the gate now covers it.

## Verification plan

- `python3 scripts/verify_documentation.py --root .` fails on the parent commit
  with the twenty-three named contradictions, and reports `DOCUMENTATION_OK`
  after the repair.
- `python3 tests/scripts/verify_documentation_tests.py` passes, including the
  new shapes.
- The new cases are mutated to prove they fail when the check stops working.

## The gate was written before the repair — 2026-08-27

Deliberately in that order, so the twenty-three contradictions are the gate's
output rather than a list assembled by hand and then asserted to be complete.
`verify_board_lane_claims` was added to `scripts/verify_documentation.py` and
run against the unrepaired text at `f3b7b422`:

```
ERROR: docs/issues/README.md table cells claim a lane a card has left:
  line 769: 'backlog' BPR-0333, which is in 60-done/
  line 950: 'parked' BPR-0276, which is in 60-done/
  ... 21 more
```

Twenty-three, matching an independent scan run before the check existed. The
repair then took the gate to `DOCUMENTATION_OK`. Both directions are recorded
because either alone proves nothing: a check that only passes may read nothing,
and a repair that only passes may have been checked by nothing.

The ranges hid more than the endpoints showed. `backlog BPR-0277 through
BPR-0285` named nine cards of which six had closed, and `In-progress BPR-0347
and BPR-0358` named two cards that closed on 2026-08-22 and 2026-08-23. The
prose beside those cells had gone stale with them: the Windows row still
reserved future runs for BPR-0347, BPR-0412, and BPR-0364, all three done, so
that sentence was rewritten to name what is actually left — BPR-0036's and
BPR-0053's matrix replays on a rebuilt tree, and the BPR-0235, BPR-0311, and
BPR-0315 courses.

Three mutations prove the tests are load-bearing rather than merely green:
neutering the check fails four of them; removing the possessive guard fails
the `BPR-0235's Linux half` case; and removing the table-row restriction fails
the narrative-prose case. Each was reverted and the suite returned to 116 OK.

## What this card does not close

The mechanical half is the lane vocabulary. The `Needs` cells' free prose is
still a reading obligation, and this card repaired two of them by hand rather
than by gate: BPR-0053's asked for four adversaries its card records as
implemented and green at `697b8f71`, and BPR-0240's told every session that
`windows_agent_process.cpp` had never been compiled, five days after that card
recorded the opposite under its own heading. Nothing here would catch the third
one. A session reading a `Needs` cell still has to open the card.

## Verification

| Claim | Command | Result |
|---|---|---|
| The gate fails on the unrepaired text | `python3 scripts/verify_documentation.py --root .` at `f3b7b422` plus this commit's check | exit 1, 23 named contradictions |
| The gate passes after the repair | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |
| The new cases hold | `python3 tests/scripts/verify_documentation_tests.py` | 116 OK, 9 new |
| The new cases are load-bearing | three reverted mutations of the check | 4, 1, and 1 failures respectively |
| No whitespace damage | `git diff --check` | clean |
