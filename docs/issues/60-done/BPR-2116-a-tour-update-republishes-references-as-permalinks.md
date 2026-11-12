---
schema: "implement-rfc.card.v1"
id: "BPR-2116"
title: "A tour update republishes an edited stop's references as permalinks"
status: "Done"
rfcs: ["RFC-BPR-0100"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2116--BPR-2117--f5d986a0e8bc.json","origin":"RFC-BPR-0100 closeout review, 2026-11-11 (ledger, Known limits)","related_cards":["BPR-1996","BPR-1997"]}
depends_on: []
acceptance: [{"criterion":"AC06","outcome":"pass","revision":"f042726a674dc2436156529ba6c8b5bb85c53c6d","evidence":{"kind":"command","detail":"Edited published stop republished by a tour update with both permalink forms; restart sees no change and writes nothing; negative control (rewrite removed) fails","command":"ctest --preset dev -R application_review_tour_publication_native"}}]
---

# A tour update republishes an edited stop's references as permalinks

## Scope and acceptance

RFC-BPR-0100 AC06 requires that "the published comment contains the
commit-pinned permalink in the form established on BPR-Test". A new
publication and the stops a tour update creates do. An already published
stop whose text is edited is republished by a tour update with its
references as written: the update path renders the stop from the core,
which has no permalink maker
([ledger, Known limits](../../verification/RFC-BPR-0100.md#known-limits)).
So AC06 is not met on that path, and RFC-BPR-0100 stays open
([[BPR-1997]] depends on this card).

- A tour update that edits a published stop's comment writes the same
  rewritten text a first publication writes (`core::rewrite_tour_references`):
  inline references as Markdown links to the commit-pinned permalink, a
  reference alone on its line as the bare permalink, unresolved ones as
  written.
- The published baseline keeps the authored text, so a restarted app does
  not see the updated tour as changed.

## Intended verification

`application_review_tour_publication_*`: publish, edit a stop's reference,
update on GitHub; the edited comment carries the permalink and a restart sees
no change. A negative control with the update path's rewrite removed fails.

## Implementation record

Product commit `f042726a6`. Two causes: `prepare_tour_repairs` sent a
published tour straight to its preview without observing the pull request's
files, so the plan had none to resolve references against; and the update
rendered existing stops from the core tour, whose text is the author's.
Now the window observes the files before a published tour's preview, and
`prepare_tour_plan` writes every published stop with
`core::tour_stop_text_edited` in its published form
(`core::rewrite_tour_references`) in the frozen update tour; the live draft
keeps the author's text, which `published_tour` stores as the baseline's
`DraftText`.

Verification ([ledger](../../verification/RFC-BPR-0100.md#bpr-2116-a-tour-update-republishes-references-2026-11-11)):
`application_review_tour_publication_native` edits a published stop,
updates, reads both permalink forms on the edited comment and restarts with
no unpublished change and zero writes; failed before the fix and with the
rewrite removed. `core_review_tour_update`, the other eleven publication
courses, 62 tour rows, the composition contracts, format and tidy pass.
