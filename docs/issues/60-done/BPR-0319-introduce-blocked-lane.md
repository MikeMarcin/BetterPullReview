---
schema: "implement-rfc.card.v1"
id: "BPR-0319"
title: "Introduce the blocked lane and classify platform-blocked cards"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# Introduce the blocked lane and classify platform-blocked cards

`10-backlog/` currently carries two different things under one name. Some cards
are waiting on another card and will start as soon as it lands. Others cannot
be executed at all, on any host this project has, because their acceptance
criteria require target-native execution on an operating system nobody here
can boot. A reader scanning sixty backlog cards cannot tell those apart, and
the count reads as a queue that could be drained by working harder.

It cannot. Eleven of those cards are waiting on hardware.

## Lane definition

`50-blocked/` holds a card whose own remaining work cannot be executed on this
host because it requires target-native execution on a different operating
system, together with any card whose every unmet dependency resolves only
through such a card.

The rule is deliberately narrower than "cannot progress right now". A card
parked on an attended owner verdict, a live GitHub session, or an unmade
product decision stays in `10-backlog/`, because the missing input is a person
or a service rather than a machine. Those blockers are just as real, and several
of them gate more work than the platform blockers do, but they are not what
this lane records and folding them in would make the lane mean nothing.

`50-blocked/` is not terminal. A card returns to `10-backlog/` or `20-ready/`
on the day the platform exists, with no other change required.

## Required final state

- A `50-blocked/` lane exists with status `Blocked`, documented in the board
  README alongside the other four lanes.
- Every card in `50-blocked/` carries a `blocked_by` front-matter field naming
  the exact platform requirement, drawn from a closed vocabulary.
- `scripts/verify_documentation.py` treats `50-blocked/` as a real lane: it
  enforces the folder/status agreement, requires and validates `blocked_by`,
  permits unmet dependencies the way `10-backlog/` does, and accepts a blocked
  card as an open closeout blocker.
- The documentation gate and its own contract suite pass with the new lane
  populated.
- No card moves on a narrative reading. Each move cites the card's own text.

## Classification

Direct — the card's own acceptance criteria name a host this project does not
have:

| Card | Requires |
|---|---|
| BPR-0036 | real Windows 10/11 x64 |
| BPR-0053 | real Windows 10/11 x64 |
| BPR-0156 | a Linux host or virtual machine; this one has no Linux toolchain, container runtime, or emulator |
| BPR-0235 | target-native Linux and Windows; its portable slice is complete and directly verified |
| BPR-0310 | target-native Linux |
| BPR-0311 | real Windows 10/11 x64 |
| BPR-0314 | target-native Linux |
| BPR-0315 | real Windows 10/11 x64 |

Transitive — every unmet dependency resolves only through the cards above:

| Card | Path |
|---|---|
| BPR-0035 | BPR-0036 |
| BPR-0054 | BPR-0035, BPR-0053 |
| BPR-0083 | BPR-0054 |

Deliberately not moved, because their blocker is a person or a service:
BPR-0100, BPR-0114, BPR-0142, BPR-0143, BPR-0144, BPR-0153, BPR-0168,
BPR-0169, BPR-0210, BPR-0214, BPR-0220, BPR-0233, BPR-0256, BPR-0257,
BPR-0258, BPR-0262, BPR-0276, BPR-0298, BPR-0299, BPR-0312, BPR-0318, and the
closeout cards that depend on them.

Deliberately not moved, because they are not slices: BPR-0211 is owner-deferred
by explicit instruction, and BPR-0212 records an RFC-scale idea that its own
text says should not be worked as an issue.

BPR-0240 is held back from this slice. Its remaining work is not purely
platform-bound: a portable regression matrix, the documentation gate, and an
independent review are all still owed on this host. It is classified after
that work is done, not before.

## Verification

```sh
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
ctest --test-dir build/dev --output-on-failure \
  -R '^(documentation_structure|documentation_issue_graph_contract)$'
```

The contract suite must gain cases that fail without the change: a blocked card
missing `blocked_by`, a blocked card carrying an unknown platform value, and a
blocked card whose folder and status disagree.

## Result

The classification landed at `a6760a3` and the lane itself at `db1e5ec`.
`50-blocked/` holds exactly the eleven cards named above — the eight direct and
the three transitive — and every one of them carries a `blocked_by` drawn from
the closed vocabulary: `platform-windows` for BPR-0035, BPR-0036, BPR-0053,
BPR-0054, BPR-0083, BPR-0311, and BPR-0315; `platform-linux` for BPR-0156,
BPR-0310, and BPR-0314; `platform-linux-and-windows` for BPR-0235.

`scripts/verify_documentation.py` treats the lane as real rather than tolerated.
`50-blocked` maps to `Blocked` in the lane/status table and the mismatch is a
failure, not a warning. `blocked_by` is required inside the lane, validated
against `BLOCKED_BY_VALUES`, and rejected outside it — a backlog card cannot
quietly claim a platform blocker it is not parked on. `DEFERRING_ISSUE_LANES`
carries `50-blocked` alongside `10-backlog`, so a blocked card may depend on
open work the way a backlog card may, and `OPEN_ISSUE_STATUSES` carries
`Blocked`, so a blocked card still blocks its RFC's closeout. That last pair is
the point of the lane: it records why a card cannot move without pretending the
card is finished.

The contract suite gained the three cases the card required —
`test_rejects_blocked_card_without_blocked_by`,
`test_rejects_unknown_blocked_by_value` (which uses
`waiting-on-the-product-owner`, the exact kind of blocker this lane refuses),
and `test_rejects_blocked_lane_status_mismatch` — plus
`test_rejects_blocked_by_outside_the_blocked_lane`,
`test_accepts_blocked_card_with_open_dependency`, and
`test_rejects_ready_card_depending_on_a_blocked_card`.

Gates, with the lane populated:

```
python3 scripts/verify_documentation.py --root .      DOCUMENTATION_OK
python3 tests/scripts/verify_documentation_tests.py   69 tests, OK
ctest -R '^(documentation_structure|documentation_issue_graph_contract)$'   2/2
```

BPR-0240 is still unclassified, as this card said it would be. Its remaining
work is not purely platform-bound, and it is classified after the portable
gates it owes are run, not before.
