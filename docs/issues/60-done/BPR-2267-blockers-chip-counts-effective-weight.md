---
schema: "implement-rfc.card.v1"
id: "BPR-2267"
title: "The review's blockers count a draft by its effective weight"
status: "Done"
rfcs: []
depends_on: []
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2267--BPR-2267--c8a92ffb77ac.json","origin":"coordinator, 2026-11-18, after BPR-2264","related_cards":["BPR-2264"]}
acceptance: [{"criterion":"AC-a","outcome":"pass","revision":"1a58cf41f","evidence":{"kind":"command","detail":"core_feedback_convention implied_weights: implied_expectation equals the catalog's Weight for every label"}},{"criterion":"AC-b","outcome":"pass","revision":"1a58cf41f","evidence":{"kind":"command","detail":"core_review_preflight Effective weight: issue: and suggestion (blocking): drafts are pending blockers, issue (non-blocking):, suggestion:, nitpick: and question: are not; a published issue: blocks, mine or theirs; an undecorated todo or chore stays guidance"}},{"criterion":"AC-c","outcome":"pass","revision":"1a58cf41f","evidence":{"kind":"command","detail":"qt_review_submission_editor_scale_100/125/200: drafts weighed by core::project_review_preflight give the chip 2 (not sent), its tooltip Yours, not sent: 2 blocking, and Approve 2 open blocking"}}]
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

## Root cause

`project_review_preflight` counted a header as blocking only when its
decorations said `(blocking)`. BPR-2264 stopped writing a weight the label
already implies, so `issue: …` drafts lost the decoration and dropped out of
the count.

## Fix

The catalog carries each label's implied weight
(`feedback_label_definition::Weight`, read by `implied_expectation`), and
`effective_expectation` gives a header's weight: the written decoration
when there is one, otherwise the label's weight. The preflight counts my
published and pending items, and other reviewers' items, by that weight. An
undecorated `todo`, `typo` or `chore` stays label guidance (the chip's "not
marked blocking" row) as before. That rule is unchanged from BPR-1085.
