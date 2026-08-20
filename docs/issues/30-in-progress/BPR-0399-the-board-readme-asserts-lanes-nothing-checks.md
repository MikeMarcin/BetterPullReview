---
id: BPR-0399
title: The board README asserts lanes that nothing checks
status: In Progress
rfc: RFC-BPR-0013
depends_on: []
---

# The board README asserts lanes that nothing checks

`docs/issues/README.md` states a lane for a card in eight places where the lanes
say otherwise, and its frontier-roots table omits two of the roots its own
definition admits. `scripts/verify_documentation.py` checks front matter,
dependencies, and lane/status agreement on every card, and checks nothing at all
about the prose that describes them.

The file already knows this happens. Lines 618–623 record the last time the same
table drifted and was hand-corrected. Hand-correcting it a second time without
adding a check would just schedule a third.

## The eight drifted attributions

Each was resolved against the card's actual folder, not against another sentence
in the same file.

| Line | Says | Card | Actually in |
|---|---|---|---|
| 449 | "dependent backlog BPR-0262" | BPR-0262 | `60-done/` |
| 569 | "Blocked BPR-0235" | BPR-0235 | `10-backlog/` |
| 666 | "Backlog BPR-0262" | BPR-0262 | `60-done/` |
| 666 | "gating backlog BPR-0256" | BPR-0256 | `40-parked/` |
| 667 | "Backlog BPR-0100" | BPR-0100 | `40-parked/` |
| 667 | "Backlog ... BPR-0210" | BPR-0210 | `60-done/` |
| 669 | "Blocked BPR-0235" | BPR-0235 | `10-backlog/` |
| 672 | "BPR-0156 moved to `50-blocked/`" | BPR-0156 | `60-done/` |

Two of these are worth naming individually.

`BPR-0235` is called blocked twice, and [[BPR-0358]] is the card that took it out
of `50-blocked/` — so the file contradicts a move it describes approvingly
elsewhere. `BPR-0156` is the sharper case: line 664 says "Done BPR-0156" and line
672 says it "moved to `50-blocked/`". The file disagrees with itself within nine
lines, and only one of the two can be true.

## The table claims to be complete and is not

Line 602 defines a frontier root as "a card in `40-parked/`, `50-blocked/`, or an
active lane". Line 625 then says "The table is now the complete frontier rather
than a selection from it."

Both cannot hold. The table lists sixteen roots — three in `30-in-progress/` and
thirteen in `40-parked/` — and no card from `50-blocked/`. [[BPR-0310]] and
[[BPR-0314]] satisfy the definition and are missing, so the real count is
eighteen.

This is not the same class of error as the eight above. Those are stale; this one
was never right.

## Required final state

- Every lane attribution in `docs/issues/README.md` agrees with the lanes.
- The frontier-roots table lists every card in `20-ready/`, `30-in-progress/`,
  `40-parked/`, and `50-blocked/` exactly once, with its correct lane, and lists
  nothing else. The stated count matches.
- `scripts/verify_documentation.py` fails when the table omits such a card,
  lists a card that is done or in `10-backlog/`, names the wrong lane for one, or
  states a count that disagrees with its own rows.
- The check is proven against a tree where each of those is false.

## Non-goals

- Do not machine-check the prose outside the table. A sentence may legitimately
  narrate a past lane; the table is the only place the file claims a current one,
  and it is the only place worth pinning.
- Do not change any card's lane. This card repairs descriptions, not state.
- Do not restate the transitive-closure claim as verified. That the 52 backlog
  cards all reach a root remains derived from the dependency graph the verifier
  already checks, not from this table.

## Verification

```sh
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
ctest --test-dir build/core-only --output-on-failure \
  -R '^(documentation_structure|documentation_issue_graph_contract)$'
```
