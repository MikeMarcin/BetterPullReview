---
id: BPR-0234
title: Anchor open RFC closeout cards to immutable bases and checklists
status: Ready
rfc: RFC-BPR-0013
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
