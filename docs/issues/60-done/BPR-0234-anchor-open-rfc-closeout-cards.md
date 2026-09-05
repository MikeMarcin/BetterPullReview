---
schema: "implement-rfc.card.v1"
id: "BPR-0234"
title: "Anchor open RFC closeout cards to immutable bases and checklists"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# Anchor open RFC closeout cards to immutable bases and checklists

BPR-0225 found that the five open RFC-BPR-0002 through RFC-BPR-0006 closeout
cards are narrative plans with no explicit immutable RFC base and no executable
acceptance checklist. The later RFCs had only done preparation cards and no
live terminal closeout cards at all; BPR-0241 through BPR-0245 now make that
future graph explicit. Their verification ledgers often name a base, but the
live terminal card does not consistently make that authority or its remaining
gates mechanically reviewable.

## Required final state

- BPR-0035, BPR-0054, BPR-0083, BPR-0101, BPR-0115, and BPR-0241 through
  BPR-0245 name the exact immutable RFC base used for closeout.
- Each card has a checklist mapping every RFC acceptance criterion to a direct
  verification surface, command or attended interaction, expected result, and
  durable evidence location.
- Every external-authority, target-native, owner-attended, and canonical-order
  blocker remains unchecked and links to its live owning card.
- Historical run counts remain provenance only; the checklist requires final
  replay at the eventual closeout head.
- Documentation validation fails if a future open RFC closeout card omits its
  immutable base or executable checklist.

This card changes no RFC status and does not rerun an attended or target-native
gate. It makes the existing closure contract explicit before one can be
claimed.

## Completion evidence

- The ten open terminal cards name their immutable RFC bases and map all 118
  canonical acceptance criteria in RFC order: `9/8/39/10/9/9/9/8/8/9`.
- Every row names a criterion-specific direct surface, executable repository
  command or anchored owning-card procedure, falsifiable expected result,
  exact criterion-keyed ledger record, gate class, and live-blocker state.
- `scripts/verify_documentation.py` discovers terminal cards generically and
  rejects identity mismatches, missing or dead anchors, placeholder evidence,
  invalid commands, invented procedures, checked open rows, and blockers that
  are done or outside the dependency closure.
- `python3 tests/scripts/verify_documentation_tests.py` passed all 63 focused
  positive and negative contract tests; the root documentation gate and both
  registered documentation CTests passed.
- All 75 distinct CTest selectors embedded across 103 row occurrences select
  registered tests, and all 13 procedure links resolve to substantive action
  sections on open dependency-owned cards.
- An independent skeptical review drove four correction rounds and ended
  `CLEAR` with no blocking finding. A conservative simplification pass reused
  the shared dependency-list parser without changing behavior.

All checklist rows remain deliberately unchecked for final replay at their
eventual closeout heads. No RFC status changed and no attended, target-native,
or external-authority result was inferred.
