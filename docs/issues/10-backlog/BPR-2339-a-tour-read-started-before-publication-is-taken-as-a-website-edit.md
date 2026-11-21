---
schema: "implement-rfc.card.v1"
id: "BPR-2339"
title: "A pull request read that started before the app's own tour publication lands after it and is taken as a website edit"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2339--BPR-2342--60dd1b567452.json","origin":"BPR-2215 known 0.5.0 intermittent, waived by the owner's standing rule (2026-11-21)","related_cards":["BPR-2215"]}
---

# BPR-2339: A pull request read that started before the app's own tour publication lands after it and is taken as a website edit

## Finding

`application_review_tour_publication_reconciliation` fails intermittently
(4 of 20 runs on the Ubuntu guest beside five CPU burners) at "an uncompared
push leaves the stop Changed since accepted". Traced on the guest, a real
pull request read still in flight when the app publishes lands after the
course's injected push:

- the fixture server still serves the first head, so the tour's observed head
  returns to it (`accepted/none`), the fixture's late `observe_head` revert;
- or the read's comments predate the published edit, and reconciliation
  adopts the older website text as an edit (`not_accepted/text_changed`).

The course already waits for the pushed head itself (a6568dc1d) and names
what it saw on a miss (41416c318); the race remains in the product.

## Direction

Drop tour observations from reads that started before the app's own
publication: the publication's generation bounds which reads may reconcile.

## Acceptance

- AC1. A read started before a tour publication and finishing after it neither
  moves the tour's observed head back nor counts as a website edit.
- AC2. A course that holds such a read across the publication shows both.
- AC3. `application_review_tour_publication_reconciliation` passes 20 of 20
  beside the same load on the Ubuntu guest.
