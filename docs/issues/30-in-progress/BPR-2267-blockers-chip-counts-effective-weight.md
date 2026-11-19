---
schema: "implement-rfc.card.v1"
id: "BPR-2267"
title: "The review's blockers count a draft by its effective weight"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2267--BPR-2267--c8a92ffb77ac.json","origin":"coordinator, 2026-11-18, after BPR-2264","related_cards":["BPR-2264"]}
---

# The review's blockers count a draft by its effective weight

## Origin

BPR-2264 writes a label's implied weight no more (`issue:` blocks without
`(blocking)`), but the pre-review blockers chip counted only drafts that say
`(blocking)`, so an `issue:` draft was no longer counted.

## Scope and acceptance

- **AC-a.** The label catalog carries each label's implied weight.
- **AC-b.** A draft blocks by its explicit decoration when it has one,
  otherwise by its label's implied weight; the chip counts those drafts.
- **AC-c.** Core tests and a chip test cover `issue:`, `issue (non-blocking):`,
  `suggestion:` and `suggestion (blocking):`.
