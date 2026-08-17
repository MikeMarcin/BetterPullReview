---
id: BPR-0331
title: Introduce the parked lane for person and service blockers
status: Done
rfc: RFC-BPR-0013
depends_on: [BPR-0319]
---

# Introduce the parked lane for person and service blockers

[[BPR-0319]] split the platform blockers out of `10-backlog/` and said, in the
same breath, what it was leaving behind: a card parked on an attended owner
verdict, a live GitHub session, or an unmade product decision stays in the
backlog, "because the missing input is a person or a service rather than a
machine. Those blockers are just as real, and several of them gate more work
than the platform blockers do."

They do gate more. The board README already had to write the consequence out in
prose — a "Frontier roots" table listing which cards sit at the front of the
graph and what each one is waiting for — because the lanes could not express it.
A table maintained by hand is a second tracker, and it had already drifted: it
lists BPR-0262 as needing an operator statement that the card's own text records
as already given.

`40-parked/` is the lane the README's `40` gap was left for.

## Lane definition

`40-parked/` holds a card whose every dependency is already done, and whose own
remaining work needs a person or a live service that this project cannot supply
on demand.

Both halves matter. The first makes the lane mean *frontier root*: nothing is in
this lane because it is queued behind something else. The second is the blocker
itself — an attended verdict, an owner decision, or a real GitHub call.

A card is `Parked`, and carries a `parked_on` field from a closed vocabulary:

| `parked_on` | Meaning |
|---|---|
| `awaiting-owner-verdict` | A person must look, click, or return an exported verdict |
| `awaiting-live-github` | A real GitHub service call against a designated target |
| `awaiting-product-decision` | An unmade product decision, not a judgement of rendered evidence |

## Why this lane does not take transitive dependents

`50-blocked/` holds three cards — BPR-0035, BPR-0054, BPR-0083 — whose every
unmet dependency resolves through a platform-blocked card. Mirroring that here
would be wrong, and the numbers say why. The platform closure is three cards.
The person-and-service closure is most of the board: nearly every open card
reaches an attended verdict or a live call eventually, so folding the closure in
would empty `10-backlog/` and leave it meaning nothing.

Keeping the lane to frontier roots preserves the distinction that matters.
`10-backlog/` means "waiting on another card", which `depends_on` already
records exactly. `40-parked/` means "waiting on a person or a service, and
nothing else is in the way." The second is the only lane whose contents someone
could act on today.

The rule is enforced rather than asserted: `40-parked/` is deliberately not a
deferring lane, so the existing dependency check already fails a parked card
that has an unmet dependency.

## Required final state

- A `40-parked/` lane exists with status `Parked`, documented in the board
  README alongside the other lanes.
- Every card in `40-parked/` carries a `parked_on` drawn from the closed
  vocabulary, and no card outside the lane carries one.
- `scripts/verify_documentation.py` enforces the folder/status agreement,
  requires and validates `parked_on`, keeps `40-parked/` out of the deferring
  lanes so a parked card with an unmet dependency fails, permits a backlog card
  to depend on a parked card, and counts a parked card as an open closeout
  blocker.
- The contract suite gains cases that fail without the change.
- Every move cites the moved card's own text.
- The README's hand-maintained "Frontier roots" table is reconciled with the
  lane, including the stale BPR-0262 row.

## Classification

Computed from the board itself — every card in an open lane whose dependencies
are all in `60-done/` — and then read against each card's own text:

| Card | `parked_on` | The card's own words |
|---|---|---|
| BPR-0168 | `awaiting-owner-verdict` | "Attendance was never what was missing"; one banner observation remains |
| BPR-0220 | `awaiting-owner-verdict` | Implementation complete; "Awaiting the owner foreground run" |
| BPR-0233 | `awaiting-owner-verdict` | "the card remains open until the exact captures receive the required attended verdict" |
| BPR-0257 | `awaiting-owner-verdict` | "the rejected v003 subject is evidence, not acceptance" |
| BPR-0258 | `awaiting-owner-verdict` | "parked in Backlog while the exact exported human verdict is pending" |
| BPR-0276 | `awaiting-owner-verdict` | "After a new plain operator direction authorizes exactly one recovery early poll" |
| BPR-0298 | `awaiting-live-github` | "A digest-bound attended packet authorizes exact credential-free, read-only, bounded requests against named public targets" |

Deliberately not moved:

- BPR-0100, BPR-0114, BPR-0142, BPR-0153, BPR-0210, BPR-0214, BPR-0299, and
  BPR-0318 all name a person or a service in their own text, but each has an
  unmet dependency, so a person acting today could not finish them. They are
  waiting on a card first, which is what `10-backlog/` records.
- BPR-0211 and BPR-0212 are not slices, exactly as [[BPR-0319]] found: one is
  owner-deferred by explicit instruction, the other records an RFC-scale idea
  its own text says should not be worked as an issue.
- BPR-0262 is a frontier root but is not parked. Its own text records the
  operator approval as already given, and every remaining obligation is local
  code, tests, and gates.
- BPR-0240 is a frontier root whose classification [[BPR-0319]] deferred until
  its portable gates ran on this host. It is classified on its own card, not
  this one.

## Verification

```sh
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
ctest --test-dir build/dev --output-on-failure \
  -R '^(documentation_structure|documentation_issue_graph_contract)$'
```

The contract suite must gain cases that fail without the change: a parked card
missing `parked_on`, a parked card carrying an unknown value, a parked card
whose folder and status disagree, a `parked_on` outside the lane, and — the case
that distinguishes this lane from `50-blocked/` — a parked card with an unmet
dependency.

## Result

`40-parked/` holds the seven cards classified above, each with the `parked_on`
its own text supports. Every quote in the table was checked against the card
rather than against a summary of it; one was a paraphrase and was replaced with
the card's exact sentence. BPR-0258 turned out to describe itself as "parked in
Backlog while the exact exported human verdict is pending", which is the lane
this card built, named the way the board had already started naming it.

The frontier-root invariant needed no new mechanism. Because `40-parked` is
absent from `DEFERRING_ISSUE_LANES`, the pre-existing dependency check already
fails a parked card with an unmet dependency:

```
ERROR: 40-parked card BPR-0002 depends on BPR-0001 in 10-backlog
```

and a backlog card depending on a parked card still passes, because
`10-backlog` remains deferring. Both directions are asserted by the suite, which
grew from 69 cases to 76.

Six of the seven new cases fail against the pre-change validator. The seventh —
a parked card whose dependencies are all done — passes vacuously, because before
the change the lane was never scanned and the card was simply invisible. That is
recorded rather than disguised: an acceptance case can only fail beforehand if
the old validator were stricter, and the one shape where that happens is already
covered by the backlog-depends-on-parked case.

The moves broke four markdown links, which is how the gate found them. BPR-0115
and BPR-0244 linked BPR-0220 and BPR-0168 as same-directory neighbours, and
RFC-BPR-0017 linked BPR-0298 through its old lane. All four now point at
`40-parked/`.

The README's hand-maintained frontier table is reconciled and its stale BPR-0262
row is corrected: that card's approval is recorded in its own text, so it is a
frontier root that needs nothing and stays in `20-ready/`. The table now names
each root's lane, which is what makes the list checkable instead of narrative.

Gates:

```
python3 scripts/verify_documentation.py --root .      DOCUMENTATION_OK
python3 tests/scripts/verify_documentation_tests.py   76 tests, OK
ctest -R '^(documentation_structure|documentation_issue_graph_contract)$'   2/2
```

One deliberate non-change: `40-parked` is inserted beside `50-blocked` in
`OPEN_ISSUE_LANES` and `lane_status` rather than in ordinal position, because
those structures were never in ordinal order and iteration order there decides
only which diagnostic fires first, never a verdict.

